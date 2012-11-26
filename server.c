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

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa) {
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}
	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

void listen_on_socket(int sockfd, void* (*handler) (void*)) {
  int numbytes;
  struct sockaddr_storage sender_addr;
  char buf[MAXBUFLEN];
  socklen_t addr_len;
  char s[INET6_ADDRSTRLEN];

  while (1) {	
		if (DEBUG) printf("[manager] waiting to receive...\n");

		addr_len = sizeof sender_addr;
		if ((numbytes = recvfrom(sockfd, buf, MAXBUFLEN-1 , 0,
			(struct sockaddr *)&sender_addr, &addr_len)) == -1) {
			perror("[manager] failed to recvfrom");
			exit(1);
		}

		if (DEBUG) printf("[manager] got packet from %s %d\n",
			inet_ntop(sender_addr.ss_family,
				get_in_addr((struct sockaddr *)&sender_addr),
				s, sizeof s), *(int*) (get_in_addr((struct sockaddr *)&sender_addr)));
		buf[numbytes] = '\0';
    pthread_t tha;
    pthread_create(&tha, NULL, handler, buf);
	}

	close(sockfd);
}

