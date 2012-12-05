#define _GNU_SOURCE // this is for accessing fault contexts
#include <assert.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ucontext.h>

#include "debug.h"
#include "copyset.h"
#include "libdsm.h"
#include "messages.h"
#include "network.h"
#include "pagedata.h"
#include "pagelocks.h"

#define DEBUG 1

// for handling shared memory
static int fd;

static int pg_info_fd;

static struct DataTable *copysets;
static struct DataTable *owners;
static client_id_t id;

// for handling the service thread 
pthread_mutex_t service_started_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t service_started_cond = PTHREAD_COND_INITIALIZER;
static int dsm_initialized = 0;

void process_read_request(void * addr, client_id_t requester) {    
  int r;
  DEBUG_LOG("\e[31mprocess_read_request\e[0m %p from %lu", addr, requester);

  data_t is_owner = 0;
  get_page_data(owners, addr, &is_owner);
  DEBUG_LOG("is owner? %lu", is_owner);
  while (!is_owner) {
    pthread_yield();
    get_page_data(owners, addr, &is_owner);
  }

  page_lock(addr);

  // Set page perms to READ
  r = mprotect(addr, PGSIZE, PROT_READ);
  if (r < 0) {
    DEBUG_LOG("outside R request: error code %d", errno);
  } else {
    DEBUG_LOG("%p marked as \e[35mread-only\e[0m", addr);
  }

  // Add reader to copyset
  copyset_t copyset;
  get_page_data(copysets, addr, &copyset);
  copyset = add_to_copyset(copyset, requester);
  copyset = add_to_copyset(copyset, id); // NOTE really only necessary if was a writer before
  set_page_data(copysets, addr, copyset);

  // Send page
  struct PageInfoMessage outmsg;
  outmsg.type = READ;
  outmsg.pg_address = addr;
  outmsg.copyset = copyset;
  memcpy(outmsg.pg_contents, addr, PGSIZE);
  outmsg.from = id;
  sendPgInfoMsg(&outmsg, requester);
  
  page_unlock(addr);
  DEBUG_LOG("");
}

/** Someone is requesting a write */
void process_write_request(void * addr, client_id_t requester) {
  int r;
  DEBUG_LOG("\e[33mprocess_write_request\e[0m %p from %lu", addr, requester);

  data_t is_owner = 0;
  get_page_data(owners, addr, &is_owner);
  while (!is_owner) {
    pthread_yield();
    get_page_data(owners, addr, &is_owner);
  }

  page_lock(addr);
  DEBUG_LOG("process_write_request locked");

  // put together message
  copyset_t copyset;
  get_page_data(copysets, addr, &copyset);
  
  struct PageInfoMessage outmsg;
  outmsg.type = WRITE;
  outmsg.pg_address = addr;
  outmsg.copyset = copyset;
  memcpy(outmsg.pg_contents, addr, PGSIZE);
  outmsg.from = id;
  DEBUG_LOG("outmsg.from = %lu", id);
  
  //  if (requester != id) {
    // Set page perms to NONE
    r = mprotect(addr, PGSIZE, PROT_NONE);
    if (r < 0) {
      DEBUG_LOG("outside WR request: error code %d", errno);
    } else {
      DEBUG_LOG("%p marked as \e[35minaccessable\e[0m", addr);
    }

    set_page_data(owners, addr, 0);
    //  }

  // send page
  DEBUG_LOG("sending page info message to %ld", requester);
  sendPgInfoMsg(&outmsg, requester);
  
  //if (requester != id)
  page_unlock(addr);

  DEBUG_LOG("");
}


/** Check if it's a write fault or read fault: returns 1 if write fault */
int is_write_fault(int signum, siginfo_t *info, void *ucontext) {
    // !! normalizes to 0 or 1
    DEBUG_LOG("fault error code is %ld",((ucontext_t *) ucontext)->uc_mcontext.gregs[REG_ERR]);
    return !!(((ucontext_t *) ucontext)->uc_mcontext.gregs[REG_ERR] & PTE_W); 
}

/** Get write access to a page ... blocks */
void get_write_access(void * addr) {
  copyset_t copyset;
  struct PageInfoMessage *info_msg;

  DEBUG_LOG("\e[33mget_write_access\e[0m %p", addr);
  page_lock(addr);
  DEBUG_LOG("get_write_access locked");

  data_t is_owner;
  get_page_data(owners, addr, &is_owner);

  if (is_owner) {
    get_page_data(copysets, addr, &copyset);
  }
  else {
    
    // ask manager for write access to page
    struct RequestPageMessage msg;
    msg.type = WRITE;
    msg.pg_address = addr;
    msg.from = id;
    sendReqPgMsg(&msg, 0);
    
    DEBUG_LOG("waiting for \e[33mwrite\e[0m access response");
    //page_unlock(addr);

    info_msg = recvPgInfoMsg(pg_info_fd);
    assert (info_msg->type == WRITE);
    assert (info_msg->pg_address == addr);  

    //if (info_msg->from != id)
    //page_lock(addr);
    DEBUG_LOG("got \e[33mwrite\e[0m access response from %lu", info_msg->from);
    
    // invalidate page's copyset
    copyset = info_msg->copyset;
    set_page_data(copysets, addr, copyset);
    
  }

  while(copyset) {
    client_id_t reader = lowest_id(copyset);
    copyset = remove_from_copyset(copyset, reader);
    if (reader == id)
      continue;

    DEBUG_LOG("sending INVAL of page %p to %lu", addr, reader);
    struct RequestPageMessage invalmsg;
    invalmsg.type = INVAL;
    invalmsg.pg_address = addr;
    invalmsg.from = id;
    sendReqPgMsg(&invalmsg, reader);
  }
  
  // copyset = {}
  set_page_data(copysets, addr, 0);

  int r = mprotect(addr, PGSIZE, PROT_READ | PROT_WRITE);
  if (r < 0) {
    DEBUG_LOG("error code %d", errno);
  } else {
    DEBUG_LOG("%p marked as \e[35mwritable\e[0m", addr);
  }

  if (!is_owner) {
    memcpy(addr, info_msg->pg_contents, PGSIZE);
    free(info_msg);
  }

  set_page_data(owners, addr, 1);

  page_unlock(addr);
  DEBUG_LOG("");
}

/** Get read access to a page ... blocks */
void get_read_access(void * addr) {
  DEBUG_LOG("\e[31mget_read_access\e[0m %p", addr);
  page_lock(addr);

  // ask manager for read access to page
  struct RequestPageMessage msg;
  msg.type = READ;
  msg.pg_address = addr;
  msg.from = id;
  
  sendReqPgMsg(&msg, 0);

  DEBUG_LOG("waiting for \e[31mread\e[0m access response");
  //page_unlock(addr);

  struct PageInfoMessage *info_msg = recvPgInfoMsg(pg_info_fd);
  // this assertion is not valid, because on the first read of a page, the
  // master actually gives write permissions.
//  assert (info_msg->type == READ);
  assert (info_msg->pg_address == addr);

  //page_lock(addr);
  DEBUG_LOG("got \e[31mread\e[0m access response");

  set_page_data(copysets, addr, info_msg->copyset);

  int r = mprotect(addr, PGSIZE, PROT_READ | PROT_WRITE);
  if (r < 0) {
    DEBUG_LOG("could not mark as read-writable. error code %d", errno);
  } else {
    DEBUG_LOG("%p temporarily marked as \e[35mwritable\e[0m", addr);
  }
    
  memcpy(addr, info_msg->pg_contents, PGSIZE);
  
  if (info_msg->type == READ) {
    r = mprotect(addr, PGSIZE, PROT_READ);
    if (r < 0) {
      DEBUG_LOG("error code %d", errno);
    } else {
      DEBUG_LOG("%p marked as \e[35mread-only\e[0m", addr);
    }
  } else if (info_msg->type == WRITE) {
    r = mprotect(addr, PGSIZE, PROT_READ | PROT_WRITE);
    if (r < 0) {
      DEBUG_LOG("error code %d", errno);
    } else {
      DEBUG_LOG("%p marked as \e[35mwritable\e[0m", addr);
    }
    set_page_data(owners, addr, 1);
  } else {
    DEBUG_LOG("unknown message type %d!", info_msg->type);
    DEBUG_LOG("however, address is %p", info_msg->pg_address);
    exit(1);
  }

  free(info_msg);
  page_unlock(addr);
  DEBUG_LOG("");
}

/** Just try to make a page writable for now. */
void faulthandler(int signum, siginfo_t *info, void *ucontext) {
  void * addr = (void*) (((uintptr_t) info->si_addr) & ~0xFFF);
  if (is_write_fault(signum, info, ucontext)) {
    DEBUG_LOG("write fault handled at address %p!", addr);
    get_write_access(addr);
  } else {
    DEBUG_LOG("read fault handled at address %p!", addr);
    get_read_access(addr);
  }
}

void handle_request(void *vmsg) {
  DEBUG_LOG("got message");
  struct RequestPageMessage *msg = vmsg;
  switch (msg->type) {
  case READ:
    process_read_request(msg->pg_address, msg->from);
    break;
  case WRITE:
    process_write_request(msg->pg_address, msg->from);
    break;
  case INVAL:
    DEBUG_LOG("invalidating page %p", msg->pg_address);
    page_lock(msg->pg_address);
    mprotect(msg->pg_address, PGSIZE, PROT_NONE);
    DEBUG_LOG("%p marked as \e[35minaccessable\e[0m", msg->pg_address);
    page_unlock(msg->pg_address);
    break;
  default:
    DEBUG_LOG("unknown type of page request message: %u", msg->type);
  }
  free(msg);
}

/** Will eventually be the thread that handles requests. */
void * service_thread(void *xa) {
  int sockfd;

  pthread_mutex_lock(&service_started_lock);

  // open socket
  sockfd = open_serv_socket(ports[id].req_port); 
  
  // let everyone know we're done initializing
  dsm_initialized = 1;
  pthread_cond_broadcast(&service_started_cond);
  pthread_mutex_unlock(&service_started_lock);

  // actually start listening on the socket.
  // TODO: check that it's actually okay to start listening after
  // broadcasting we're done. I think this is fine ... it is if 
  // things will just sit on the socket until we read it.
  
  struct RequestPageMessage *msg;
  pthread_t req_thread;
  while (msg = recvReqPgMsg(sockfd)) {
    if (pthread_create(&req_thread, NULL, handle_request, msg) != 0) {
      DEBUG_LOG("Error starting message-handling thread");
      exit(1);
    }
  }

  return NULL;
}

/** Just starts the service thread */
void start_service_thread(void) {
  // Create thread or error
  pthread_t tha;
  pthread_mutex_lock(&service_started_lock);
  if (pthread_create(&tha, NULL, service_thread, NULL) == 0) {
    while(!dsm_initialized) pthread_cond_wait(&service_started_cond, &service_started_lock);
    pthread_mutex_unlock(&service_started_lock);
  } else {
    pthread_mutex_unlock(&service_started_lock);
    DEBUG_LOG("Error starting service thread.");
  }
}

/** Things that should only happen once. */
void dsm_init(client_id_t myId) {
  if (dsm_initialized > 0) return;
  id = myId;
  
  copysets = alloc_data_table();
  copysets->do_get_faults = 0;

  owners = alloc_data_table();
  //owners->do_get_faults = 0;

  // set up fault handling
  struct sigaction s;
  s.sa_sigaction = faulthandler;
  sigemptyset(&s.sa_mask);
  s.sa_flags = SA_SIGINFO;
  sigaction(SIGSEGV, &s, NULL);

  // set up the service thread
  start_service_thread();

  // make the socket for info messages
  pg_info_fd = open_serv_socket(ports[id].info_port);
}

/** Opens a new distributed shared memory object. */
void * dsm_open(void *addr, size_t size) {
  // initialize other dsm-y type things
  if (!dsm_initialized) {
    DEBUG_LOG("ERROR: dsm_open() called without dsm_init()");
    exit(1);
  }

  // set up the memory mapping 
  void *result = mmap(addr, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

  // return address
  return result;
}
