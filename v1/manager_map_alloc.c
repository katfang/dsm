#include <pthread.h>

#include "debug.h"

#define DEBUG 1
#define PAGE_IN_USE 0x1 
#define END_FREE_LIST (void*) 0x2

pthread_mutex_t map_alloc_started_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t map_alloc_started_cond = PTHREAD_COND_INITIALIZER;
static int map_alloc_thread_initialized = 0;

pthread_mutex_t map_alloc_lock = PTHREAD_MUTEX_INITIALIZER;

struct DataTable *free_table; // does the pointer hopping thing
void *free_page = END_FREE_LIST;

void start_map_alloc(void);
void * map_alloc_thread(void *xa);
void process_map_alloc(struct MapAllocMessage *msg);
void process_malloc(struct MapAllocMessage *msg);
void process_dsm_open(struct MapAllocMessage *msg);

void start_map_alloc(void) {
  pthread_t tha;
  
  // free_table[addr] = next free addr
  free_table = alloc_data_table();
  free_table->do_get_faults = 0;
  
  pthread_mutex_lock(&map_alloc_started_lock);
  if (pthread_create(&tha, NULL, map_alloc_thread, NULL) == 0) {
    while(!map_alloc_thread_initialized) pthread_cond_wait(&map_alloc_started_cond, &map_alloc_started_lock);
    pthread_mutex_unlock(&map_alloc_started_lock);
  } else {
    pthread_mutex_unlock(&map_alloc_started_lock);
    DEBUG_LOG("Error starting map_alloc thread.");
  }

  DEBUG_LOG("map_alloc started.");
}

void * map_alloc_thread(void *xa) {
  int map_alloc_sockfd;
  
  pthread_mutex_lock(&map_alloc_started_lock);

  // open socket
  map_alloc_sockfd = open_serv_socket(ports[0].map_alloc_port); 
  
  // let everyone know we're done initializing
  map_alloc_thread_initialized = 1;
  pthread_cond_broadcast(&map_alloc_started_cond);
  pthread_mutex_unlock(&map_alloc_started_lock);

  // actually start listening on the socket.
  struct MapAllocMessage *msg;
  while (msg = recvMapAllocMsg(map_alloc_sockfd)) {
    process_map_alloc(msg);
    free(msg);
  }

  return NULL;
}

void process_map_alloc(struct MapAllocMessage *msg) {
  if (msg->type == MALLOC) {
    process_malloc(msg);
  } else if (msg->type == DSM_OPEN) {  
    process_dsm_open(msg);
  }
}

void process_malloc(struct MapAllocMessage *msg) {
  DEBUG_LOG("Processing \e[34mmalloc\e[0m for %lu bytes", msg->size);
  // make response message
  struct MapAllocMessage resp_msg;
  resp_msg.type = MALLOC_RESPONSE;
  resp_msg.size = msg->size;

  // if greater than a page
  // TODO REVIEW THIS CODE
  pthread_mutex_lock(&map_alloc_lock);
    
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
    set_page_data(free_table, curr, PAGE_IN_USE);
    curr = next;
  }
  
 send_malloc_response:
  // send off the message
  pthread_mutex_unlock(&map_alloc_lock);
  sendMapAllocMessage(&resp_msg, msg->from);
  if (resp_msg.size > 0) {
    DEBUG_LOG("malloc processed. %lu bytes at %p malloc'd to %lu",
      resp_msg.size, resp_msg.pg_address, msg->from);
  } else {
    DEBUG_LOG("malloc for %lu bytes from %lu failed. Not enough bytes.",
      resp_msg.size, msg->from);
  }
}

void process_dsm_open(struct MapAllocMessage *msg) {
  DEBUG_LOG("processing \e[34mdsm_open\e[0m at %p for %lu from %lu", msg->pg_address, msg->size, msg->from);
  assert((long) msg->pg_address % PGSIZE == 0);
  assert(msg->size % PGSIZE == 0);

  pthread_mutex_lock(&map_alloc_lock);

  void *addr, *next;
  void *max_addr = msg->pg_address + msg->size - PGSIZE; 
  // TODO ^ round up max_addr instead of having asserts

  for (addr = max_addr; addr >= msg->pg_address; addr -= PGSIZE) {
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
  
  pthread_mutex_unlock(&map_alloc_lock);

  // reply with open successful
  msg->type = DSM_OPEN_RESPONSE;
  sendMapAllocMessage(msg, msg->from);
  DEBUG_LOG("finished processing dsm_open at %p for %lu from %lu", msg->pg_address, msg->size, msg->from);
}
