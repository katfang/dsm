#define _GNU_SOURCE // this is for accessing fault contexts
#include "copyset.h"
#include "libdsm.h"
#include "messages.h"
#include "pagedata.h"
#include "pagelocks.h"
#include "sender.h"
#include <signal.h>
#include <sys/ucontext.h>

#define DEBUG 1

// for handling shared memory
static int fd;

static struct DataTable *copysets;
static client_id_t id; // TODO: initialize this

// for handling the service thread
pthread_mutex_t service_started_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t service_started_cond = PTHREAD_COND_INITIALIZER;
static int service_state = 0;

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
  send_to_client(requester, &outmsg, sizeof(outmsg));
  
  page_unlock(addr);
}

/** Someone is requesting a write */
void process_write_request(void * addr, client_id_t requester) {
  int r;
  if (DEBUG) printf("[libdsm] process_write_request %p\n", addr);
  page_lock(addr);
  
  // Set page perms to NONE
  r = mprotect(addr, PGSIZE, PROT_NONE);
  if (r < 0) {
    if (DEBUG) printf("[libdsm] outside WR request: error code %d\n", errno);
  } else {
    if (DEBUG) printf("[libdsm] gave up all access to %p.\n", addr);
  }

  // Send page
  copyset_t copyset;
  get_page_data(copysets, addr, &copyset);
  
  struct PageInfoMessage outmsg;
  outmsg.type = WRITE;
  outmsg.pg_address = addr;
  outmsg.copyset = copyset;
  memcpy(outmsg.pg_contents, addr, PGSIZE);
  send_to_client(requester, &outmsg, sizeof(outmsg));
  
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
  
  // TODO: ask manager for write access to page
  // TODO: invalidate page's copyset
  
  // copyset = {}
  set_page_data(copysets, addr, 0);

  int r = mprotect(addr, PGSIZE, PROT_READ | PROT_WRITE);

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

  // TODO: ask manager for read access to page

  int r = mprotect(addr, PGSIZE, PROT_READ);

  if (r < 0) {
    if (DEBUG) printf("[libdsm] error code %d\n", errno);
  } else {
    if (DEBUG) printf("[libdsm] marked as writable\n");
  }
  page_unlock(addr);
}

/** Just try to make a page writable for now. */
void faulthandler(int signum, siginfo_t *info, void *ucontext) {
  if (DEBUG) printf("[libdsm] fault %d handled at address %p!\n", info->si_code, info->si_addr);

  if (is_write_fault(signum, info, ucontext)) {
    get_write_access(info->si_addr);
  } else {
    get_read_access(info->si_addr);
  }
}

/** Will eventually be the thread that handles requests. */
void * service_thread(void *xa) {

  unsigned i = 0;
  pthread_mutex_lock(&service_started_lock);
  
  // initialization stuff
  service_state = 1;
  copysets = alloc_data_table();
  copysets->do_get_faults = 0;
  
  pthread_cond_broadcast(&service_started_cond);
  pthread_mutex_unlock(&service_started_lock);

  while(1) {
    // TODO real processing goes here.
    process_read_request((void*) 0xdeadbeef000, 1);
  }
  return NULL;
}

/** Just starts the service thread */
void start_service_thread(void) {
  if (service_state > 0) return;
  
  // Create thread or error
  pthread_t tha;
  pthread_mutex_lock(&service_started_lock);
  if (pthread_create(&tha, NULL, service_thread, NULL) == 0) {
    while(!service_state) pthread_cond_wait(&service_started_cond, &service_started_lock);
    pthread_mutex_unlock(&service_started_lock);
  } else {
    pthread_mutex_unlock(&service_started_lock);
    if (DEBUG) printf("[libdsm] Error starting service thread.\n");
  }
}


// TODO: split into dsm_open() for allocating memory, vs. dsm_init() for setting
// up one-time stuff
/** Opens a new distributed shared memory object. */
void * dsm_open(void *addr, size_t size) {
  // set up the shared memory object
  fd = shm_open(SHM_NAME, O_RDWR|O_CREAT|O_EXCL, S_IRWXU);
  ftruncate(fd, PGSIZE);
  void *result = mmap(addr, size, PROT_NONE, MAP_PRIVATE, fd, 0);

  // set up fault handling
  struct sigaction s;
  s.sa_sigaction = faulthandler;
  sigemptyset(&s.sa_mask);
  s.sa_flags = SA_SIGINFO;
  sigaction(SIGSEGV, &s, NULL);

  // set up the service thread
  start_service_thread();

  // return address
  return result;
}

/** Close off things when done. */
void dsm_close() {
  shm_unlink(SHM_NAME);
  close(fd);
}
