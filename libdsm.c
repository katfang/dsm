#include "libdsm.h"
#include "messages.h"
#include "pagedata.h"
#include "sender.h"

#define DEBUG 1

// for handling shared memory
static int fd;

// for handling the service thread 
static pthread_t *tha;
static int service_state = 0;

static struct DataTable *copysets; // TODO: initialize this, with do_get_faults = 0

static client_id_t id; // TODO: initialize this

void process_read_request(void * addr, client_id_t requester) {
  // TODO: Handle case when I'm a writer
  int r;

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
  copyset |= 1 << (requester - 1);
  put_page_data(copysets, addr, copyset);

  // Send page
  struct PageInfoMessage outmsg;
  outmsg.type = READ;
  outmsg.pg_address = addr;
  outmsg.copyset = copyset;
  memcpy(outmsg.pg_contents, addr, PGSIZE);
  send_to_client(requester, &outmsg, sizeof(outmsg));
}

/** Someone is requesting a write */
void process_write_request(void * addr, client_id_t requester) {
  int r;

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
}

/** Check if it's a write fault or read fault: returns 1 if write fault*/
int is_write_fault(int signum, siginfo_t *info, void *ucontext) {
  return 1;
}

/** Get write access to a page ... blocks */
void get_write_access(void * addr) {
  // TODO: Go to the network

  int r = mprotect(addr, PGSIZE, PROT_READ | PROT_WRITE);

  if (r < 0) {
    if (DEBUG) printf("[libdsm] error code %d\n", errno);
  } else {
    if (DEBUG) printf("[libdsm] marked as writable\n");
  }
}

/** Get read access to a page ... blocks */
void get_read_access(void * addr) {
  // TODO: Go to the network

  int r = mprotect(addr, PGSIZE, PROT_READ);

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
static void * 
service_thread(void *xa) {
  // Do set up and release lock so that other thread can continue
  if (DEBUG) printf("[libdsm] Service thread started...\n");

  // === START HACKS
  unsigned i;
  for (i = 0; i < 5; i++) {
    printf("Hello world.\n\n");
    if (i % 5 == 2) process_write_request((void*) 0xdeadbeef000, NULL);
    if (i % 5 == 3) process_read_request((void*) 0xdeadbeef000, NULL);
    pthread_yield();
  }
  // === END HACKS  

  // Ended service thread?!
  if (DEBUG) printf("[libdsm] Service thread is done...\n");
  return NULL;
}

/** Just starts the service thread */
static void
start_service_thread(void) {
  if (service_state > 0) return;
  
  // Create thread or error
  if (pthread_create(&tha[0], NULL, service_thread, NULL) == 0) {
    service_state = 1;
  } else {
    if (DEBUG) printf("[libdsm] Error starting service thread.\n");
  }
}

/** Opens a new distributed shared memory object. */
void * dsm_open(void * addr, size_t size, void * (*loop)(void *)) {
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
