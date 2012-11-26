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
#include "messages.h"
#include "sender.h"
#include "pagedata.h"

pthread_mutex_t manager_lock = PTHREAD_MUTEX_INITIALIZER;
struct DataTable *owner_table;

void forward_request(struct RequestPageMessage * msg) {
  client_id_t pg_owner;
  printf("[master] type: %d address %p from %" PRIu64 "\n", msg->type, msg->pg_address, (uint64_t) msg->from);

  pthread_mutex_lock(&manager_lock);

  // Master is owner -- respond to original dude with an empty page
  if (get_page_data(owner_table, msg->pg_address, &pg_owner) < 0) {
    set_page_data(owner_table, msg->pg_address, msg->from);

    struct PageInfoMessage outmsg;
    outmsg.type = WRITE;
    outmsg.pg_address = msg->pg_address;
    outmsg.copyset = 1 << (msg->from - 1);
    outmsg.pg_size = 0;
    outmsg.pg_contents; // TODO pass actually page contents which is all 0s?
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

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa) {
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}
	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(void) {
  int sockfd;
  struct addrinfo hints, *servinfo, *p;
  int r, numbytes;
  struct sockaddr_storage sender_addr;
  char buf[MAXBUFLEN];
  socklen_t addr_len;
  char s[INET6_ADDRSTRLEN];

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC; // set to AF_INET to force IPv4
  hints.ai_socktype = SOCK_DGRAM;
  hints.ai_flags = AI_PASSIVE; // use my IP
  
  // Get address info
  r = getaddrinfo(NULL, ports[0], &hints, &servinfo);
  if (r != 0) {
    fprintf(stderr, "getaddinfo: %s\n", gai_strerror(r));
    return 1;
  }
  
  for (p = servinfo; p != NULL; p = p->ai_next) {
    if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
      perror("[master] socket");
      continue;
    }

    if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
      close(sockfd);
      perror("[master] bind");
      continue;
    }

    break;
  }

	if (p == NULL) {
		fprintf(stderr, "[master] failed to bind socket\n");
		return 2;
	}

	freeaddrinfo(servinfo);

  owner_table = alloc_data_table();

  while (1) {	
		printf("[master] waiting to receive...\n");

		addr_len = sizeof sender_addr;
		if ((numbytes = recvfrom(sockfd, buf, MAXBUFLEN-1 , 0,
			(struct sockaddr *)&sender_addr, &addr_len)) == -1) {
			perror("[master] failed to recvfrom");
			exit(1);
		}

		printf("[master] got packet from %s %d\n",
			inet_ntop(sender_addr.ss_family,
				get_in_addr((struct sockaddr *)&sender_addr),
				s, sizeof s), *(int*) (get_in_addr((struct sockaddr *)&sender_addr)));
		buf[numbytes] = '\0';
    pthread_t tha;
    pthread_create(&tha, NULL, handle_request, buf);
	}

	close(sockfd);

  printf("[master] ending...\n");
  return 0; 
}
