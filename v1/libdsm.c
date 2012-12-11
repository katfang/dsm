#define _GNU_SOURCE // this is for accessing fault contexts
#include <assert.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ucontext.h>

#include "debug.h"
#include "libdsm.h"
#include "messages.h"
#include "network.h"
#include "pagelocks.h"

#define DEBUG 1

static int pg_info_fd;
static int map_alloc_fd;
static client_id_t id;

// for handling the service thread 
pthread_mutex_t service_started_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t service_started_cond = PTHREAD_COND_INITIALIZER;
static int dsm_initialized = 0;
static int dsm_opened = 0;

void get_read_access(void * addr) {
  DEBUG_LOG("\e[31mget_read_access\e[0m %p", addr);
  page_lock(addr);
  
  // ask manager for read access to page
  struct RequestPageMessage msg;
  msg.type = READ;
  msg.pg_address = addr;
  msg.from = id;
  sendReqPgMsg(&msg, 0);

  // get response from other node
  page_unlock(addr);
  struct PageInfoMessage *info_msg = recvPgInfoMsg(pg_info_fd);
  assert (info_msg->pg_address == addr);
  DEBUG_LOG("successfully got response.");
  page_lock(addr);
  
  // set data
  // TODO: should stop other threads from accessing
  // data at this point if possible
  int r = mprotect(addr, PGSIZE, PROT_READ | PROT_WRITE);
  if (r < 0) {
    DEBUG_LOG("could not mark as read-writable. error code %d", errno);
    exit(1);
  } else {
    DEBUG_LOG("successfully marked read-writable (temp to copy page contents)");
  }
  memcpy(addr, info_msg->pg_contents, PGSIZE);
  
  // set the page permissions
  if (info_msg->type == READ) {
    r = mprotect(addr, PGSIZE, PROT_READ);
    if (r < 0) {
      DEBUG_LOG("error code %d", errno);
    } else {
      DEBUG_LOG("marked as read-only");
    }
  } else if (info_msg->type == WRITE) {
    r = mprotect(addr, PGSIZE, PROT_READ | PROT_WRITE);
    if (r < 0) {
      DEBUG_LOG("error code %d", errno);
    } else {
      DEBUG_LOG("marked as read-write");
    }
  } else {
    DEBUG_LOG("unknown message type %d!", info_msg->type);
    DEBUG_LOG("however, address is %p", info_msg->pg_address);
    exit(1);
  }

  // TODO: why only free this and not msg 
  // TODO: does sendReqPgMsg block until entire msg is sent?
  free(info_msg);
  page_unlock(addr);
}

/** Get write access to a page ... blocks */
void get_write_access(void * addr) {
  DEBUG_LOG("\e[33mget_write_access\e[0m %p", addr);
  page_lock(addr);
  
  // ask manager for write access to page
  struct RequestPageMessage msg;
  msg.type = WRITE;
  msg.pg_address = addr;
  msg.from = id;
  sendReqPgMsg(&msg, 0);

  // get page data
  page_unlock(addr);
  struct PageInfoMessage *info_msg = recvPgInfoMsg(pg_info_fd);
  assert (info_msg->type == WRITE);
  assert (info_msg->pg_address == addr);  
  DEBUG_LOG("successfully got response.");
  page_lock(addr);

  // set page data
  int r = mprotect(addr, PGSIZE, PROT_READ | PROT_WRITE);
  memcpy(addr, info_msg->pg_contents, PGSIZE);
  if (r < 0) {
    DEBUG_LOG("error code %d", errno);
  } else {
    DEBUG_LOG("marked as writable");
  }

  // send ack to manager
  DEBUG_LOG("sending ack.");
  msg.type = ACK;
  sendAckMsg(&msg, 0);
  DEBUG_LOG("done sending ack.");

  free(info_msg);
  page_unlock(addr);
}

/** Check if it's a write fault or read fault: returns 1 if write fault */
int is_write_fault(int signum, siginfo_t *info, void *ucontext) {
    // !! normalizes to 0 or 1
    DEBUG_LOG("fault error code is %ld",((ucontext_t *) ucontext)->uc_mcontext.gregs[REG_ERR]);
    return !!(((ucontext_t *) ucontext)->uc_mcontext.gregs[REG_ERR] & PTE_W); 
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

void process_read_request(void * addr, client_id_t requester) {    
  int r;
  DEBUG_LOG("\e[31mprocess_read_request\e[0m %p", addr);
  page_lock(addr);

  // Set page perms to READ
  r = mprotect(addr, PGSIZE, PROT_READ);
  if (r < 0) {
    DEBUG_LOG("outside R request: error code %d", errno);
  } else {
    DEBUG_LOG("marked %p as read-only.", addr);
  }

  // Send page
  struct PageInfoMessage outmsg;
  outmsg.type = READ;
  outmsg.pg_address = addr;
  memcpy(outmsg.pg_contents, addr, PGSIZE);
  sendPgInfoMsg(&outmsg, requester);
  
  page_unlock(addr);
}

/** Someone is requesting a write */
void process_write_request(void * addr, client_id_t requester) {
  int r;
  DEBUG_LOG("\e[33mprocess_write_request\e[0m %p", addr);
  if (requester != id) {
    page_lock(addr);
    DEBUG_LOG("process_write_request locked");  
  }

  // put together message
  struct PageInfoMessage outmsg;
  outmsg.type = WRITE;
  outmsg.pg_address = addr;
  r = mprotect(addr, PGSIZE, PROT_READ);
  memcpy(outmsg.pg_contents, addr, PGSIZE);
  
  // Set page perms to NONE
  r = mprotect(addr, PGSIZE, PROT_NONE);
  if (r < 0) {
    DEBUG_LOG("outside WR request: error code %d", errno);
  } else {
    DEBUG_LOG("gave up all access to %p.", addr);
  }

  // send page
  DEBUG_LOG("sending page info message to %ld", requester);
  sendPgInfoMsg(&outmsg, requester);
  
  page_unlock(addr);
}

void handle_request(struct RequestPageMessage *msg) {
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
    page_unlock(msg->pg_address);
    break;
  }
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
  struct RequestPageMessage *msg;
  while (msg = recvReqPgMsg(sockfd)) {
    handle_request(msg);
    free(msg);
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
  map_alloc_fd = open_serv_socket(ports[id].map_alloc_port);
}

/** Opens a new distributed shared memory object. */
void * dsm_open(void *addr, size_t size) {
  // initialize other dsm-y type things
  if (!dsm_initialized) {
    DEBUG_LOG("ERROR: dsm_open() called without dsm_init()");
    exit(1);
  }

  // ask the host to open
  struct MapAllocMessage msg;
  msg.type = DSM_OPEN;
  msg.pg_address = addr;
  msg.size = size;
  msg.from = id;
  sendMapAllocMessage(&msg, 0);
  recvMapAllocMsg(map_alloc_fd);

  // set up the memory mapping 
  void *result = mmap(addr, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

  // return address
  dsm_opened = 1;
  return result;
}

/** Malloc some space */
void * dsm_malloc(size_t size) {
  if (!dsm_initialized || !dsm_opened) {
    DEBUG_LOG("ERROR: dsm_malloc() called without dsm_init() or dsm_open()");
  }
  
  void *addr;

  // send off message
  DEBUG_LOG("mallocing %lu bytes", size);
  struct MapAllocMessage msg;
  msg.type = MALLOC;
  msg.size = size;
  msg.from = id;
  sendMapAllocMessage(&msg, 0);

  // get the location
  struct MapAllocMessage *resp_msg;
  resp_msg = recvMapAllocMsg(map_alloc_fd);
  addr = resp_msg->pg_address;
  if (resp_msg->size > 0) {
    DEBUG_LOG("malloc'd %lu bytes at %p", 
      resp_msg->size, resp_msg->pg_address);
  } else {
    DEBUG_LOG("malloc failed. exiting.");
    exit(1);
  }

  free(resp_msg);
  return addr;
}
