/** Actually deals with allocs and locks. */

#include <pthread.h>

#include "debug.h"

#define DEBUG 1
#define END_FREE_LIST (void*) 0x1
#define PAGE_IN_USE (void*) 0x2
#define PAGE_IS_LOCK_INIT (void*) 0x3
#define PAGE_IS_LOCK (void*) 0x4
#define PAGE_IS_RESERVED (void*) 0x5

extern struct DataTable *owner_table;

pthread_mutex_t alloc_started_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t alloc_started_cond = PTHREAD_COND_INITIALIZER;
static int alloc_thread_initialized = 0;

pthread_mutex_t alloc_lock = PTHREAD_MUTEX_INITIALIZER;

struct DataTable *free_table; // does the pointer hopping thing
void *free_page = END_FREE_LIST;

void start_alloc(void);
void * alloc_thread(void *xa);
void * process_alloc_thread(void *xa);
void process_alloc(struct AllocMessage *msg);
void process_malloc(struct AllocMessage *msg);
void process_dsm_open(struct AllocMessage *msg);
void process_reserve(struct AllocMessage *msg);
void process_free(struct AllocMessage *msg);
void process_lock_init(struct AllocMessage *msg);
void process_lock_made(struct AllocMessage *msg);

void start_alloc(void) {
  pthread_t tha;
  
  // free_table[addr] = next free addr
  free_table = alloc_data_table();
  free_table->do_get_faults = 0;
  
  pthread_mutex_lock(&alloc_started_lock);
  if (pthread_create(&tha, NULL, alloc_thread, NULL) == 0) {
    while(!alloc_thread_initialized) pthread_cond_wait(&alloc_started_cond, &alloc_started_lock);
    pthread_mutex_unlock(&alloc_started_lock);
  } else {
    pthread_mutex_unlock(&alloc_started_lock);
    DEBUG_LOG("Error starting alloc thread.");
  }

  DEBUG_LOG("alloc started.");
}

void * alloc_thread(void *xa) {
  int alloc_sockfd;
  
  pthread_mutex_lock(&alloc_started_lock);

  // open socket
  alloc_sockfd = open_serv_socket(ports[0].alloc_port); 
  
  // let everyone know we're done initializing
  alloc_thread_initialized = 1;
  pthread_cond_broadcast(&alloc_started_cond);
  pthread_mutex_unlock(&alloc_started_lock);

  // actually start listening on the socket.
  struct AllocMessage *msg;
  pthread_t tha;
  while (msg = recvAllocMsg(alloc_sockfd)) {
    if (pthread_create(&tha, NULL, process_alloc_thread, msg) < 0) {
      DEBUG_LOG("Error spawning alloc thread.");
    } 
  }

  return NULL;
}

void * process_alloc_thread(void *xa) {
  DEBUG_LOG(">>>>> Spawned thread to process alloc request.");
  struct AllocMessage *msg = (struct AllocMessage *) xa;
  process_alloc(msg);
  free(msg);
  DEBUG_LOG("<<<<< Finished processing an alloc request.");
}

void process_alloc(struct AllocMessage *msg) {
  switch (msg->type) {
  case MALLOC:
    process_malloc(msg);
    break;
  case DSM_OPEN:
    process_dsm_open(msg);
    break;
  case RESERVE:
    process_reserve(msg);
    break;
  case FREE:
    process_free(msg);
    break;
  case LOCK_INIT:
    process_lock_init(msg);
    break;
  case LOCK_MADE:
    process_lock_made(msg);
    break;
  }
}

/** Asks the server to malloc some space. */
void process_malloc(struct AllocMessage *msg) {
  DEBUG_LOG("Processing \e[34mmalloc\e[0m for %lu bytes", msg->size);
  // make response message
  struct AllocMessage resp_msg;
  resp_msg.type = MALLOC_RESPONSE;
  resp_msg.size = msg->size;

  // if greater than a page
  // TODO REVIEW THIS CODE
  pthread_mutex_lock(&alloc_lock);
    
  // 'start' = one BEFORE the start of the continuous block
  // 'prev' = one BEFORE curr, last page added to continuous strand
  // 'curr' = current page considered for adding to continuous block
  void *start = NULL, *prev = NULL, *curr = free_page, *next;
  size_t accumulated = 0;

  // keep getting pages, make sure they're continuous
  // 'curr' is the one we're considering to add [or not] to the list.
  do { 
    if (curr == END_FREE_LIST) {
      DEBUG_LOG("Cannot malloc. Ran out of space.");
      resp_msg.size = 0;
      goto send_malloc_response;
    }
    if (prev == NULL || curr == prev + PGSIZE) {
      accumulated += PGSIZE;
    } else {
      accumulated = PGSIZE;
      start = prev;
    }
    prev = curr;

    // get next page to test
    get_page_data(free_table, curr, (data_t*) &curr);
    DEBUG_LOG("%p's next is %p", prev, curr);
    DEBUG_LOG("accumulated: %lu of %lu", accumulated, msg->size);
  } while (accumulated < msg->size);

  // so we want to take from the one AFTER start ... to prev 
  // fix up free_page and the linked list of pages
  if (start != NULL) {
    get_page_data(free_table, start, (data_t*) &next);
    set_page_data(free_table, start, (data_t) curr);
    resp_msg.pg_address = next;
  } else {
    resp_msg.pg_address = free_page;
    free_page = curr;
  }
  resp_msg.size = accumulated;

  // do some clean up and set those pages to PAGE_IN_USE
  void *end = curr;
  curr = resp_msg.pg_address;
  DEBUG_LOG("start at: %p, end at: %p", curr, end);
  while (curr != end) {
    get_page_data(free_table, curr, (data_t*) &next);
    set_page_data(free_table, curr, (data_t) PAGE_IN_USE);
    curr = next;
  }
  
 send_malloc_response:
  // send off the message
  pthread_mutex_unlock(&alloc_lock);
  sendAllocMessage(&resp_msg, msg->from);
  if (resp_msg.size > 0) {
    DEBUG_LOG("malloc processed. %lu bytes at %p malloc'd to %lu",
      resp_msg.size, resp_msg.pg_address, msg->from);
  } else {
    DEBUG_LOG("malloc for %lu bytes from %lu failed. Not enough bytes.",
      msg->size, msg->from);
  }
}

/** Tells the manager that this space is available for allocating. */
void process_dsm_open(struct AllocMessage *msg) {
  DEBUG_LOG("processing \e[34mdsm_open\e[0m at %p for %lu from %lu", msg->pg_address, msg->size, msg->from);
  assert((long) msg->pg_address % PGSIZE == 0);
  assert(msg->size % PGSIZE == 0);

  pthread_mutex_lock(&alloc_lock);

  void *addr, *next;
  void *max_addr = msg->pg_address + msg->size - PGSIZE; 
  // TODO ^ round up max_addr instead of having asserts

  for (addr = max_addr; addr >= msg->pg_address; addr -= PGSIZE) {
    DEBUG_LOG("processing dsm_open for addr: %p", addr);
    if (get_page_data(free_table, addr, (data_t*) &next) < 0) {
      DEBUG_LOG("Error in getting free table data.");
      continue;
    } 
    // if next is not 0x0, it was opened before by someone else. skip
    if (next == 0x0) { 
      set_page_data(free_table, addr, (data_t) free_page);
      void* next;
      get_page_data(free_table, addr, (data_t*) &next);
      DEBUG_LOG("%p's next set to: %p", addr, next);
      free_page = addr; 
    } 
  }
  
  pthread_mutex_unlock(&alloc_lock);

  // reply with open successful
  msg->type = DSM_OPEN_RESPONSE;
  sendAllocMessage(msg, msg->from);
  DEBUG_LOG("finished processing dsm_open at %p for %lu from %lu", msg->pg_address, msg->size, msg->from);
}

void process_reserve(struct AllocMessage *msg) {
  DEBUG_LOG("processing \e[34mreserve\e[0m at %p for %lu from %lu", msg->pg_address, msg->size, msg->from);
  assert((long) msg->pg_address % PGSIZE == 0);
  assert(msg->size % PGSIZE == 0);
    
  pthread_mutex_lock(&alloc_lock);

  void *addr, *next, *a;
  void *max_addr = msg->pg_address + msg->size;

  for (addr = msg->pg_address; addr < max_addr; addr += PGSIZE) {
    if (get_page_data(free_table, addr, (data_t*) &next) < 0) {
      DEBUG_LOG("Error in getting free table data.");
      continue;
    } 
    
    // memory hasn't been seen yet, just set to in use.
    if (next == 0x0) { 
      set_page_data(free_table, addr, (data_t) PAGE_IS_RESERVED);
    } else if (next == PAGE_IS_RESERVED || next == PAGE_IS_LOCK 
                                        || next == PAGE_IS_LOCK_INIT) { 
      DEBUG_LOG("Memory already reserved.");
    } else {
      // TODO: could potentially figure out where it is in the free table
      // with a doubly linked list and take it out as part of a hole.
      DEBUG_LOG("Memory is already 'open'd for mallc. Cannot reserve.");
      exit(1); 
    } 
  }
  
  pthread_mutex_unlock(&alloc_lock);

  // reply with reserve successful
  msg->type = RESERVE_RESPONSE;
  sendAllocMessage(msg, msg->from);
  DEBUG_LOG("finished processing reserve at %p for %lu from %lu", msg->pg_address, msg->size, msg->from);
}

/** Tells the manager that this space is available for allocating. */
void process_free(struct AllocMessage *msg) {
  DEBUG_LOG("processing \e[34mfree\e[0m at %p for %lu from %lu", msg->pg_address, msg->size, msg->from);
  assert((long) msg->pg_address % PGSIZE == 0);

  size_t size = msg->size;
  if (msg->size % PGSIZE != 0) {
    size = ((msg->size / PGSIZE) + 1) * PGSIZE;
  }

  pthread_mutex_lock(&alloc_lock);

  void *addr, *next;
  void *max_addr = msg->pg_address + size - PGSIZE;
  // TODO ^ round up max_addr instead of having asserts

  for (addr = max_addr; addr >= msg->pg_address; addr -= PGSIZE) {
    if (get_page_data(free_table, addr, (data_t*) &next) < 0) {
      DEBUG_LOG("Error in getting free table data.");
      continue;
    } 
    // if next is not PAGE_IN_USE, something is wrong
    if (next == PAGE_IN_USE) { 
      set_page_data(free_table, addr, (data_t) free_page);
      void* next;
      get_page_data(free_table, addr, (data_t*) &next);
      DEBUG_LOG("%p's next set to: %p", addr, next);
      free_page = addr; 
    }
  }
  
  pthread_mutex_unlock(&alloc_lock);

  // reply with open successful
  msg->type = FREE_RESPONSE;
  sendAllocMessage(msg, msg->from);
  DEBUG_LOG("finished processing free at %p for %lu from %lu", msg->pg_address, msg->size, msg->from);
}

/** Assumption is that ALL processes think this is a lock and 
 *  NO process could possibly try to use it as something else. */
void process_lock_init(struct AllocMessage *msg) {
  void *addr = msg->pg_address, *next;
  struct AllocMessage resp_msg;
  resp_msg.pg_address = addr;

  page_lock(addr);
  pthread_mutex_lock(&alloc_lock);

  if (get_page_data(free_table, addr, (data_t*) &next) < 0) {
    DEBUG_LOG("Error in getting free table data.");
  }

  if (next == PAGE_IS_RESERVED) {
    set_page_data(free_table, addr, (data_t) PAGE_IS_LOCK_INIT);
    set_page_data(owner_table, msg->pg_address, msg->from); // TODO this doesn't work probably

    resp_msg.type = LOCK_INIT;
  } else if (next == PAGE_IS_LOCK_INIT || next == PAGE_IS_LOCK) {
    page_unlock(addr);
    resp_msg.type = LOCK_MADE;
  }
  
 send_lock_resp:
  pthread_mutex_unlock(&alloc_lock);
  sendAllocMessage(&resp_msg, msg->from);
}

void process_lock_made(struct AllocMessage *msg) {
  pthread_mutex_lock(&alloc_lock);
  set_page_data(free_table, msg->pg_address, (data_t) PAGE_IS_LOCK);
  page_unlock(msg->pg_address); // now other pages can access it to lock it
  pthread_mutex_unlock(&alloc_lock);
}
