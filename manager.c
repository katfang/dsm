#define __STDC_FORMAT_MACROS

#include <arpa/inet.h>
#include <errno.h>
#include <inttypes.h> 
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "copyset.h"
#include "messages.h"
#include "network.h"
#include "pagedata.h"

#define DEBUG 1

pthread_mutex_t manager_lock = PTHREAD_MUTEX_INITIALIZER;
struct DataTable *owner_table;

void forward_request(struct RequestPageMessage * msg) {
  client_id_t pg_owner;
  if (DEBUG) printf("[manager] type: %d address %p from %" PRIu64 "\n", 
    msg->type, msg->pg_address, (uint64_t) msg->from);

  pthread_mutex_lock(&manager_lock);
  
  // Master is owner -- respond to original dude with an empty page
  if (get_page_data(owner_table, msg->pg_address, &pg_owner) < 0) {
    set_page_data(owner_table, msg->pg_address, msg->from);

    struct PageInfoMessage outmsg;
    outmsg.type = WRITE;
    outmsg.pg_address = msg->pg_address;
    outmsg.copyset = add_to_copyset(0, msg->from);
    memset(outmsg.pg_contents, 0, PGSIZE);
    printf("sending info msg %p to " PRIu64 "\n", &outmsg, msg->from);
    sendPgInfoMsg(&outmsg, msg->from);

  // Owner exists, so we forward the request
  // Set page's new owner if incorrect.
  } else {
    if (msg->type == WRITE) {
      set_page_data(owner_table, msg->pg_address, msg->from);
    }
    sendReqPgMsg(msg, pg_owner);
  }

  pthread_mutex_unlock(&manager_lock);
}

int main(void) {
  int sockfd;

  // start the table to keep track of who's the owner
  owner_table = alloc_data_table();

  // open a socket and listen on it.
  sockfd = open_serv_socket(ports[0].req_port);

  struct RequestPageMessage *msg;
  printf("[manager] starting on port %d...\n", ports[0].req_port);
  while (msg = recvReqPgMsg(sockfd)) {
    if (DEBUG) {
      printf("msg address %p\n", msg->pg_address);
      printf("received msg from " PRIu64 "\n", msg->from);
    }
    forward_request(msg);
    free(msg);
  }
  printf("[manager] ending...\n");
  return 0; 
}
