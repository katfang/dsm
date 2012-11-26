#define __STDC_FORMAT_MACROS
#include <inttypes.h> 
#include "sender.h"

/*
 * sender.c -- has the functions to send things
 */

char *ips[] = {
  "127.0.0.1", // actually manager
  "127.0.0.1",
  "127.0.0.1",
  "127.0.0.1",
};

char *ports[] = {
  "14000", // actually manager
  "14001",
  "14002",
  "14003",
};


int send_to_client(client_id_t id, void * msg, size_t size) {
  int sockfd;
  struct addrinfo hints, *servinfo, *p;
  int rv;
  int numbytes;

  if ((rv = getaddrinfo(ips[id], ports[id], &hints, &servinfo)) != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
    return 1;
  }

  // loop through all the results and make a socket
  for(p = servinfo; p != NULL; p = p->ai_next) {
    if ((sockfd = socket(p->ai_family, p->ai_socktype,
          p->ai_protocol)) == -1) {
      perror("[sender] socket");
      continue;
    }
    break;
  }

  if (p == NULL) {
    fprintf(stderr, "talker: failed to bind socket\n");
    return 2;
  }

  if ((numbytes = sendto(sockfd, msg, size, 0,
       p->ai_addr, p->ai_addrlen)) == -1) {
    perror("talker: sendto");
    exit(1);
  }

  freeaddrinfo(servinfo);

  printf("[sender] sent %d bytes to (%s:%s)\n", numbytes, 
    ips[id], ports[id]);
  close(sockfd);

  return 0;
}
