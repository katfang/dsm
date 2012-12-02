#define __STDC_FORMAT_MACROS

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

#include "debug.h"
#include "copyset.h"
#include "debug.h"
#include "messages.h"
#include "network.h"
#include "pagedata.h"

#define DEBUG 1

pthread_mutex_t manager_lock = PTHREAD_MUTEX_INITIALIZER;
struct DataTable *owner_table;
int sockfd = -1;

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


void forward_request(struct RequestPageMessage * msg) {
  client_id_t pg_owner;
  DEBUG_LOG("type: %d address %p from %" PRIu64 "", 
    msg->type, msg->pg_address, (uint64_t) msg->from);

  pthread_mutex_lock(&manager_lock);
  
  // Master is owner -- respond to original dude with an empty page
  if (get_page_data(owner_table, msg->pg_address, &pg_owner) < 0) {
    DEBUG_LOG("received initial page request from %ld", msg->from);
    set_page_data(owner_table, msg->pg_address, msg->from);

    struct PageInfoMessage outmsg;
    outmsg.type = WRITE;
    outmsg.pg_address = msg->pg_address;
    outmsg.copyset = 0;
    memset(outmsg.pg_contents, 0, PGSIZE);
    DEBUG_LOG("outmsg.type is %d", outmsg.type);
    DEBUG_LOG("sending info msg %p to %" PRIu64 "", &outmsg, msg->from);
    sendPgInfoMsg(&outmsg, msg->from);

  // Owner exists, so we forward the request
  // Set page's new owner if incorrect.
  } else {
    if (msg->type == WRITE) {
      DEBUG_LOG("received write request from %ld", msg->from);
      set_page_data(owner_table, msg->pg_address, msg->from);
    } else {
      DEBUG_LOG("received read request from %ld", msg->from);
    }
    sendReqPgMsg(msg, pg_owner);
  }

  pthread_mutex_unlock(&manager_lock);
}

int main(void) {

  // start the table to keep track of who's the owner
  owner_table = alloc_data_table();

  signal(SIGINT, interruptHandler);

  // open a socket and listen on it.
  sockfd = open_serv_socket(ports[0].req_port);

  struct RequestPageMessage *msg;
  DEBUG_LOG("starting on port %d...", ports[0].req_port);

  while (msg = recvReqPgMsg(sockfd)) {
    forward_request(msg);
    free(msg);
  }

  DEBUG_LOG("ending...");
  return 0; 
}
