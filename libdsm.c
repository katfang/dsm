#define _GNU_SOURCE // this is for accessing fault contexts
#include <assert.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ucontext.h>

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
static client_id_t id;

// for handling the service thread 
pthread_mutex_t service_started_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t service_started_cond = PTHREAD_COND_INITIALIZER;
static int dsm_initialized = 0;

void process_read_request(void * addr, client_id_t requester) {    
  int r;
  if (DEBUG) printf("[libdsm] process_read_request %p\n", addr);
  page_lock(addr);
  // TODO: Handle case when I'm a writer... if it's relevant?

  // Set page perms to READ
  r = mprotect(addr, PGSIZE, PROT_READ);
  if (r < 0) {
    if (DEBUG) printf("[libdsm] outside R request: error code %d\n", errno);
  } else {
    if (DEBUG) printf("[libdsm] now only have read on %p.\n", addr);
  }

  // Add reader to copyset
  copyset_t copyset;
  get_page_data(copysets, addr, &copyset);
  copyset = add_to_copyset(copyset, requester);
  set_page_data(copysets, addr, copyset);

  // Send page
  struct PageInfoMessage outmsg;
  outmsg.type = READ;
  outmsg.pg_address = addr;
  outmsg.copyset = copyset;
  memcpy(outmsg.pg_contents, addr, PGSIZE);
  sendPgInfoMsg(&outmsg, requester);
  
  page_unlock(addr);
}

/** Someone is requesting a write */
void process_write_request(void * addr, client_id_t requester) {
  int r;
  if (DEBUG) printf("[libdsm] process_write_request %p\n", addr);
  page_lock(addr);
  if (DEBUG) printf("[libdsm] p_w_r locked\n");  

  // put together message
  copyset_t copyset;
  get_page_data(copysets, addr, &copyset);
  
  struct PageInfoMessage outmsg;
  outmsg.type = WRITE;
  outmsg.pg_address = addr;
  outmsg.copyset = copyset;
  memcpy(outmsg.pg_contents, addr, PGSIZE);
  
  // Set page perms to NONE
  r = mprotect(addr, PGSIZE, PROT_NONE);
  if (r < 0) {
    if (DEBUG) printf("[libdsm] outside WR request: error code %d\n", errno);
  } else {
    if (DEBUG) printf("[libdsm] gave up all access to %p.\n", addr);
  }

  // send page
  sendPgInfoMsg(&outmsg, requester);
  
  page_unlock(addr);
}


/** Check if it's a write fault or read fault: returns 1 if write fault*/
int is_write_fault(int signum, siginfo_t *info, void *ucontext) {
    // !! normalizes to 0 or 1
    return !!(((ucontext_t *) ucontext)->uc_mcontext.gregs[REG_ERR] & PTE_W); 
}

/** Get write access to a page ... blocks */
void get_write_access(void * addr) {
  if (DEBUG) printf("[libdsm] get_write_access %p\n", addr);
  page_lock(addr);
  if (DEBUG) printf("[libdsm] g_w_a locked\n");
  
  // ask manager for write access to page
  struct RequestPageMessage msg;
  msg.type = WRITE;
  msg.pg_address = addr;
  msg.from = id;
  if (DEBUG) printf("[libdsm] setting msg.from to %" PRIu64 "\n", msg.from);
  sendReqPgMsg(&msg, 0);

  struct PageInfoMessage *info_msg = recvPgInfoMsg(pg_info_fd);
  assert (info_msg->type == WRITE);
  assert (info_msg->pg_address == addr);  
  
  // invalidate page's copyset
  copyset_t copyset = info_msg->copyset;
  set_page_data(copysets, addr, copyset);

  while(copyset) {
    client_id_t reader = lowest_id(copyset);

    if (DEBUG) printf("[libdsm] sending INVAL of page %p to " PRIu64 "\n", addr, reader);
    struct RequestPageMessage invalmsg;
    invalmsg.type = INVAL;
    invalmsg.pg_address = addr;
    invalmsg.from = id;
    sendReqPgMsg(&invalmsg, reader);

    copyset = remove_from_copyset(copyset, reader);
  }
  
  // copyset = {}
  set_page_data(copysets, addr, 0);

  int r = mprotect(addr, PGSIZE, PROT_READ | PROT_WRITE);
  memcpy(addr, info_msg->pg_contents, PGSIZE);
  free(info_msg);

  if (r < 0) {
    if (DEBUG) printf("[libdsm] error code %d\n", errno);
  } else {
    if (DEBUG) printf("[libdsm] marked as writable\n");
  }
  page_unlock(addr);
}

/** Get read access to a page ... blocks */
void get_read_access(void * addr) {
  if (DEBUG) printf("[libdsm] get_read_access %p\n", addr);
  page_lock(addr);

  // ask manager for read access to page
  struct RequestPageMessage msg;
  msg.type = READ;
  msg.pg_address = addr;
  msg.from = id;
  sendReqPgMsg(&msg, 0);

  struct PageInfoMessage *info_msg = recvPgInfoMsg(pg_info_fd);
  assert (info_msg->type == READ);
  assert (info_msg->pg_address == addr);
  set_page_data(copysets, addr, info_msg->copyset);

  int r = mprotect(addr, PGSIZE, PROT_READ | PROT_WRITE);
  if (r < 0) {
    if (DEBUG) printf("[libdsm] error code %d\n", errno);
  }
    
  memcpy(addr, info_msg->pg_contents, PGSIZE);
  free(info_msg);
  
  r = mprotect(addr, PGSIZE, PROT_READ);

  if (r < 0) {
    if (DEBUG) printf("[libdsm] error code %d\n", errno);
  } else {
    if (DEBUG) printf("[libdsm] marked as read-only\n");
  }
  page_unlock(addr);
}

/** Just try to make a page writable for now. */
void faulthandler(int signum, siginfo_t *info, void *ucontext) {
  if (is_write_fault(signum, info, ucontext)) {
    if (DEBUG) printf("[libdsm] write fault handled at address %p!\n", info->si_addr);
    get_write_access(info->si_addr);
  } else {
    if (DEBUG) printf("[libdsm] read fault handled at address %p!\n", info->si_addr);
    get_read_access(info->si_addr);
  }
}

void handle_request(struct RequestPageMessage *msg) {
  switch (msg->type) {
  case READ:
    process_read_request(msg->pg_address, msg->from);
  case WRITE:
    process_write_request(msg->pg_address, msg->from);
  case INVAL:
    if (DEBUG) printf("[libdsm] invalidating page %p\n", msg->pg_address);
    mprotect(msg->pg_address, PGSIZE, PROT_NONE);
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
  // TODO: check that it's actually okay to start listening after
  // broadcasting we're done. I think this is fine ... it is if 
  // things will just sit on the socket until we read it.
  
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
    if (DEBUG) printf("[libdsm] Error starting service thread.\n");
  }
}

/** Things that should only happen once. */
void dsm_init(client_id_t myId) {
  if (dsm_initialized > 0) return;
  id = myId;
  
  copysets = alloc_data_table();
  copysets->do_get_faults = 0;

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
    printf("ERROR: dsm_open() called without dsm_init()");
    exit(1);
  }

  // set up the memory mapping 
  void *result = mmap(addr, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

  // return address
  return result;
}
