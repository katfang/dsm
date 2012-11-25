/*
 * sender.c -- has the functions to send things
 */

#include "sender.h"

int send_to_client(client_id_t id, void * msg, size_t size) {
  int sockfd;
  struct addrinfo hints, *servinfo, *p;
  int rv;
  int numbytes;

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_DGRAM;

  if ((rv = getaddrinfo(ips[id], ports[id], &hints, &servinfo)) != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
    return 1;
  }

  // loop through all the results and make a socket
  for(p = servinfo; p != NULL; p = p->ai_next) {
    if ((sockfd = socket(p->ai_family, p->ai_socktype,
          p->ai_protocol)) == -1) {
      perror("talker: socket");
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

  printf("talker: sent %d bytes to %d (%s:%s)\n", numbytes, 
    id, ips[id], ports[id]);
  close(sockfd);

  return 0;
}

int main(void) {
  printf("Hello world.\n");
  struct RequestPageMessage msg;
  msg.type = READ;
  msg.pg_address = (void *) 0xcafebebe;
  msg.from = 2;
  
  send_to_client(0, &msg, sizeof(msg));
}