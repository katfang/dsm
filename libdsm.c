#include "libdsm.h"
#define DEBUG 1

// for handling shared memory
static int fd;

// for handling the service thread 
static int service_state = 0;
pthread_mutex_t service_started_lock = PTHREAD_MUTEX_INITIALIZER;

void process_read_request(void * addr, void * requester) {
  // TODO: Make sure _I'm_ not a writer.
  int r;

  // Set page perms to NONE
  r = mprotect(addr, 4096, PROT_READ);
  if (r < 0) {
    if (DEBUG) printf("[libdsm] outside R request: error code %d\n", errno);
  } else {
    if (DEBUG) printf("[libdsm] now only have read on %p.\n", addr);
  }

  // TODO: Add reader to copyset 

  // TODO: Send page
}

/** Someone is requesting a write */
void process_write_request(void * addr, void * requester) {
  int r;

  // Set page perms to NONE
  r = mprotect(addr, 4096, PROT_NONE);
  if (r < 0) {
    if (DEBUG) printf("[libdsm] outside WR request: error code %d\n", errno);
  } else {
    if (DEBUG) printf("[libdsm] gave up all access to %p.\n", addr);
  }

  // TODO: Send page
}


/** Check if it's a write fault or read fault: returns 1 if write fault*/
int is_write_fault(int signum, siginfo_t *info, void *ucontext) {
  return 1;
}

/** Get write access to a page ... blocks */
void get_write_access(void * addr) {
  // TODO: Go to the network

  int r = mprotect(addr, 4096, PROT_READ | PROT_WRITE);

  if (r < 0) {
    if (DEBUG) printf("[libdsm] error code %d\n", errno);
  } else {
    if (DEBUG) printf("[libdsm] marked as writable\n");
  }
}

/** Get read access to a page ... blocks */
void get_read_access(void * addr) {
  // TODO: Go to the network

  int r = mprotect(addr, 4096, PROT_READ);

  if (r < 0) {
    if (DEBUG) printf("[libdsm] error code %d\n", errno);
  } else {
    if (DEBUG) printf("[libdsm] marked as writable\n");
  }
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
  pthread_mutex_unlock(&service_started_lock);

  while(1) {
    // TODO real processing goes here.
    process_read_request((void*) 0xdeadbeef000, NULL);
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
    pthread_mutex_lock(&service_started_lock);
    service_state = 1;
    pthread_mutex_unlock(&service_started_lock);
  } else {
    pthread_mutex_unlock(&service_started_lock);
    if (DEBUG) printf("[libdsm] Error starting service thread.\n");
  }
}

/** Opens a new distributed shared memory object. */
void * dsm_open(void *addr, size_t size) {
  // set up the shared memory object
  fd = shm_open(SHM_NAME, O_RDWR|O_CREAT|O_EXCL, S_IRWXU);
  ftruncate(fd, 4096);
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
