#define __STDC_FORMAT_MACROS

#include <assert.h>
#include <arpa/inet.h>
#include <errno.h>
#include <inttypes.h> 
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "copyset.h"
#include "debug.h"
#include "messages.h"
#include "network.h"
#include "pagedata.h"
#include "pagelocks.h"

#define DEBUG 1

pthread_mutex_t manager_lock = PTHREAD_MUTEX_INITIALIZER;
static struct DataTable *copysets;
struct DataTable *owner_table;
int sockfd = -1;

// for handling the ack thread 
pthread_mutex_t ack_started_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t ack_started_cond = PTHREAD_COND_INITIALIZER;
static int ack_thread_initialized = 0;

// invalidate page's copyset
void invalidate(void * addr) {
  // get copyset
  copyset_t copyset;
  get_page_data(copysets, addr, &copyset);

  // invalidate each reader
  while(copyset) {
    client_id_t reader = lowest_id(copyset);

    DEBUG_LOG("sending INVAL of page %p to %" PRIu64, addr, reader);
    struct RequestPageMessage invalmsg;
    invalmsg.type = INVAL;
    invalmsg.pg_address = addr;
    invalmsg.from = 0;
    sendReqPgMsg(&invalmsg, reader);

    copyset = remove_from_copyset(copyset, reader);
  }

  // copyset = {}
  set_page_data(copysets, addr, 0);
}

void interruptHandler(int signum) {
  if (signum == SIGINT) {
    if (sockfd != -1) {
      DEBUG_LOG("Closing sockfd");
      close(sockfd);
    }
    signal(SIGINT, SIG_DFL);
    kill(getpid(), SIGINT);
  }
}

void process_request(struct RequestPageMessage * msg) {
  page_lock(msg->pg_address);
  client_id_t pg_owner;
  
  // Master is owner -- respond to original dude with an empty page
  if (get_page_data(owner_table, msg->pg_address, &pg_owner) < 0) {
    DEBUG_LOG("received \e[32minitial\e[0m page request from %ld", msg->from);
    set_page_data(owner_table, msg->pg_address, msg->from);

    struct PageInfoMessage outmsg;
    outmsg.type = WRITE;
    outmsg.pg_address = msg->pg_address;
    outmsg.copyset = 0;
    memset(outmsg.pg_contents, 0, PGSIZE);
    DEBUG_LOG("outmsg.type is %d", outmsg.type);
    DEBUG_LOG("sending info msg %p to %" PRIu64 "", &outmsg, msg->from);
    sendPgInfoMsg(&outmsg, msg->from);
    page_unlock(msg->pg_address);

  // Owner exists, so we forward the request
  // Set page's new owner if incorrect.
  } else {

    // got a write request, set new owner?
    // don't unlock because need to wait for ack
    if (msg->type == WRITE) {
      DEBUG_LOG("received \e[33mwrite\e[0m request from %ld", msg->from);
      invalidate(msg->pg_address);
      set_page_data(owner_table, msg->pg_address, msg->from);

      DEBUG_LOG("fwding msg to %" PRIu64, pg_owner);
      sendReqPgMsg(msg, pg_owner);

    // got a read request, set new copyset
    // TODO: don't unlock because need to wait for ack
    // TODO: actually, I don't think we need an ack
    } else if (msg->type == READ) {
      DEBUG_LOG("received \e[31mread\e[0m request from %ld", msg->from);
      copyset_t copyset;
      get_page_data(copysets, msg->pg_address, &copyset);
      copyset = add_to_copyset(copyset, msg->from);
      set_page_data(copysets, msg->pg_address, copyset);

      DEBUG_LOG("fwding msg to %" PRIu64, pg_owner);
      sendReqPgMsg(msg, pg_owner);
      page_unlock(msg->pg_address);
    }
  }
}

void process_ack(struct RequestPageMessage * msg) {
  // get owner
  client_id_t pg_owner;
  get_page_data(owner_table, msg->pg_address, &pg_owner);

  // assertions
  assert(msg->type == ACK);
  assert(msg->from == pg_owner);
  
  // unlock page 
  page_unlock(msg->pg_address);
  DEBUG_LOG("\e[32munlocked\e[0m %p for write req from %" PRIu64, msg->pg_address, msg->from);
}

void * ack_thread(void *xa) {
  int ack_sockfd;

  pthread_mutex_lock(&ack_started_lock);

  // open socket
  ack_sockfd = open_serv_socket(ports[0].info_port); 
  
  // let everyone know we're done initializing
  ack_thread_initialized = 1;
  pthread_cond_broadcast(&ack_started_cond);
  pthread_mutex_unlock(&ack_started_lock);

  // actually start listening on the socket.
  struct RequestPageMessage *msg;
  while (msg = recvReqPgMsg(ack_sockfd)) {
    process_ack(msg);
    free(msg);
  }

  return NULL;

}

/** Just starts the thread that listens for acks */
void start_ack_thread(void) {
  pthread_t tha;

  pthread_mutex_lock(&ack_started_lock);
  if (pthread_create(&tha, NULL, ack_thread, NULL) == 0) {
    while(!ack_thread_initialized) pthread_cond_wait(&ack_started_cond, &ack_started_lock);
    pthread_mutex_unlock(&ack_started_lock);
  } else {
    pthread_mutex_unlock(&ack_started_lock);
    DEBUG_LOG("Error starting ack thread.");
  }
}

int main(void) {
  // start the ack thread
  start_ack_thread();

  // start the table to keep track of who's the owner
  // and the copyset
  owner_table = alloc_data_table();
  copysets = alloc_data_table();

  signal(SIGINT, interruptHandler);

  // open a socket and listen on it.
  sockfd = open_serv_socket(ports[0].req_port);

  struct RequestPageMessage *msg;
  DEBUG_LOG("starting on port %d...", ports[0].req_port);

  while (msg = recvReqPgMsg(sockfd)) {
    process_request(msg);
    free(msg);
  }

  DEBUG_LOG("ending...");
  return 0; 
}
