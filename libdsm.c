#include "libdsm.h"
#define DEBUG 1

// for handling shared memory
static int fd;

// for handling the service thread 
static pthread_t *tha;
static pthread_mutex_t service_start_lock = PTHREAD_MUTEX_INITIALIZER; 
static int service_state = 0; 

/** Someone is requesting a write */
void process_write_request(void * addr, void * requester) {
  int r;

  // Set page perms to NONE
  r = mprotect(addr, 4096, PROT_NONE);
  if (r < 0) {
    if (DEBUG) printf("[libdsm] outside WR request: error code %d\n", errno);
  } else {
    if (DEBUG) printf("[libdsm] gave up write access to %p.\n", addr);
  }

  // TODO: Send page
}

/** Just try to make a page writable for now. */
void dsm_handler(int signum, siginfo_t *info, void *ucontext) {
  if (DEBUG) printf("[libdsm] fault handled at address %p!\n", info->si_addr);
  if (DEBUG) printf("[libdsm] marking page at %p as writable\n", info->si_addr);

  int r;
  r = mprotect(info->si_addr, 4096, PROT_READ | PROT_WRITE);

  if (r < 0) {
    if (DEBUG) printf("[libdsm] error code %d\n", errno);
  } else {
    if (DEBUG) printf("[libdsm] marked as writable\n");
  }
}

/** Will eventually be the thread that handles requests. */
static void * 
service_thread(void *xa) {
  // Do set up and release lock so that other thread can continue
  if (DEBUG) printf("[libdsm] Service thread started...\n");
//  pthread_mutex_unlock(&service_start_lock);

  unsigned i;
  for (i = 0; i < 10; i++) {
    printf("Hello world.\n");
    if (i % 5 == 0) process_write_request((void*) 0xdeadbeef000, NULL);
    pthread_yield();
  }
  
  // Ended service thread?!
  if (DEBUG) printf("[libdsm] Service thread is done...\n");
  return NULL;
}

/** Just starts the service thread */
static void
start_service_thread(void) {
  if (service_state > 0) return;
  
  // Locks ensure that the service thread is actually created
  // before we return to the user. Associated unlock is in the 
  // * error-handling if case or
  // * service_thread function (after it's done setting up.)
//  pthread_mutex_lock(&service_start_lock);

  // Create thread, wait for lock to release before success 
  if (pthread_create(&tha[0], NULL, service_thread, NULL) == 0) {
    pthread_mutex_lock(&service_start_lock);
    service_state = 1;
    pthread_mutex_unlock(&service_start_lock);

  // Things failed, give up the lock. !!! Probably need to retry or exit.
  } else {
    if (DEBUG) printf("Error starting service thread.\n");
//    pthread_mutex_unlock(&service_start_lock);
  }
}

/** Opens a new distributed shared memory object. */
void * dsm_open(void * addr, size_t size, void * (*loop)(void *)) {
  // set up the shared memory object
  fd = shm_open(SHM_NAME, O_RDWR|O_CREAT|O_EXCL, S_IRWXU);
  ftruncate(fd, 4096);
  void *result = mmap(addr, size, PROT_READ, MAP_PRIVATE, fd, 0);

  // set up fault handling
  struct sigaction s;
  s.sa_sigaction = dsm_handler;
  sigemptyset(&s.sa_mask);
  s.sa_flags = SA_SIGINFO;
  sigaction(SIGSEGV, &s, NULL);

  // start the service thread
  tha = malloc(sizeof(pthread_t) * 2);
  start_service_thread();
  pthread_create(&tha[1], NULL, loop, NULL);
  
  unsigned i;
  void *value;
  for(i = 0; i < 2; i++) {
    pthread_join(tha[i], &value);
  }

  return result;
}

/** Close off things when done. */
void dsm_close() {
  shm_unlink(SHM_NAME);
  close(fd);
}
