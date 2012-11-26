#include "server.h"

#define DEBUG 1

int open_socket(char * port) {
  struct addrinfo hints, *servinfo, *p;
  int r, sockfd;

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC; // set to AF_INET to force IPv4
  hints.ai_socktype = SOCK_DGRAM;
  hints.ai_flags = AI_PASSIVE; // use my IP

  // Get address info
  r = getaddrinfo(NULL, port, &hints, &servinfo);
  if (r != 0) {
    fprintf(stderr, "getaddinfo: %s\n", gai_strerror(r));
    return 1;
  }

  for (p = servinfo; p != NULL; p = p->ai_next) {
    if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
      perror("[manager] socket");
      continue;
    }

    if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
      close(sockfd);
      perror("[manager] bind");
      continue;
    }

    break;
  }

	if (p == NULL) {
		fprintf(stderr, "[manager] failed to bind socket\n");
		return 2;
	}

	freeaddrinfo(servinfo);

  return sockfd;
}
