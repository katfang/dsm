#define __STDC_FORMAT_MACROS
#include <inttypes.h> 
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <pthread.h>
#include "copyset.h"
#include "messages.h"
#include "sender.h"
#include "pagedata.h"
#include "server.h"

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
    send_to_client(msg->from, &outmsg, sizeof(outmsg));

  // Owner exists, so we forward the request
  // Set page's new owner if incorrect.
  } else {
    if (msg->type == WRITE) {
      set_page_data(owner_table, msg->pg_address, msg->from);
    }
    send_to_client(pg_owner, msg, sizeof(*msg));
  }

  pthread_mutex_unlock(&manager_lock);
}

void * handle_request(void *xa) {
  struct RequestPageMessage *msg = (struct RequestPageMessage *) xa;
  forward_request( (struct RequestPageMessage *) xa);
  return NULL;
}

int main(void) {
  int sockfd;

  // start the table to keep track of who's the owner
  owner_table = alloc_data_table();

  // open a socket and listen on it.
  sockfd = open_socket(ports[0]);
  listen_on_socket(sockfd, handle_request); 

  printf("[manager] ending...\n");
  return 0; 
}
