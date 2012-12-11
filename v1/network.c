#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <netinet/in.h>

#include "copyset.h"
#include "debug.h"
#include "messages.h"
#include "network.h"
#define DEBUG 0

char *ips[] = {
  "127.0.0.1", // actually manager
  "127.0.0.1",
  "127.0.0.1",
  "127.0.0.1",
};

struct PortInfo ports[] = {
  {14000, 14001, 14002},
  {14003, 14004, 14005},
  {14006, 14007, 14008},
  {14009, 14010, 14011}
};

void error(const char *msg)
{
    perror(msg);
    exit(1);
}

/* connects to a client and returns a socket file descriptor */
int open_socket(char *host, int port) {
  DEBUG_LOG("Connecting to %s:%d", host, port);
  int sockfd;
  struct sockaddr_in sock_addr;
  struct hostent *host_info;
  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0) {
    error("ERROR opening socket");
  }
  host_info = gethostbyname(host);
  if (host_info == NULL) {
    error("ERROR, no such host.");
  }
  memset(&sock_addr, 0, sizeof(sock_addr));
  sock_addr.sin_family = AF_INET;
  memcpy(&sock_addr.sin_addr.s_addr,
      (char*)host_info->h_addr,
      host_info->h_length);
  sock_addr.sin_port = htons(port);
  if (connect(sockfd, (struct sockaddr *) &sock_addr, sizeof(sock_addr)) < 0) {
    error("Error connecting.");
  }
  return sockfd;
}

// opens a socket on the given port and call listen().  Returns a sockfd.
int open_serv_socket(int port) {
  int sockfd;
  struct sockaddr_in serv_addr;
  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0) 
    error("ERROR opening socket");
  memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = INADDR_ANY;
  serv_addr.sin_port = htons(port);
  if (bind(sockfd, (struct sockaddr *) &serv_addr,
           sizeof(serv_addr)) < 0)
    error("ERROR on binding");
  listen(sockfd,5);
  return sockfd;
}

char * recvMsg(int sockfd, int length) {
  char *msg;
  int numToRead, n;
  int clientfd = accept(sockfd, NULL, NULL);
  if (clientfd < 0) 
    error("ERROR on accept");
  numToRead = length;
  msg = malloc(numToRead);
  while (n = read(clientfd, msg, numToRead)) {
    if (n < 0) {
      error("ERROR on read()");
    }
    numToRead -= n;
    msg += n;
  }
  if (numToRead) {
    error("didn't fully receive message");
  }
  close(clientfd);
  return msg - length;
}

// given a server socket descriptor, accept and handle a connection.
struct PageInfoMessage *recvPgInfoMsg(int sockfd) {
  return (struct PageInfoMessage *) recvMsg(sockfd, sizeof(struct
    PageInfoMessage));
}

struct RequestPageMessage *recvReqPgMsg(int sockfd) {
  return (struct RequestPageMessage *) recvMsg(sockfd, sizeof(struct
    RequestPageMessage));
}

struct AllocMessage *recvAllocMsg(int sockfd) {
  return (struct AllocMessage *) recvMsg(sockfd, sizeof(struct
    AllocMessage));
}

static void sendMsg(client_id_t id, char *msg, int port, int length) {
  int sockfd = open_socket(ips[id], port);
  int toWrite = length;
  int n;
  while (n = write(sockfd, msg, toWrite)) {
    if (n < 0) {
      close(sockfd);
      error("ERROR on write()");
    }
    toWrite -= n;
    msg += n;
  }
  if (toWrite) {
    close(sockfd);
    error("Couldn't fully write message");
  }
  DEBUG_LOG("Wrote message to %d", (int) id);
  close(sockfd);
}

/* returns something negative on failure. */
int sendReqPgMsg(struct RequestPageMessage *msg, client_id_t id) {
  DEBUG_LOG("sending message of size %" PRIu64 " from %" PRIu64, sizeof(struct RequestPageMessage), msg->from);
  sendMsg(id, (char*) msg, ports[id].req_port, sizeof(struct RequestPageMessage));
}

/* only use this for sending acks to the manager. */
int sendAckMsg(struct RequestPageMessage *msg, client_id_t id) {
  assert(id == 0);
  DEBUG_LOG("sending message of size %" PRIu64 " from %" PRIu64, sizeof(struct RequestPageMessage), msg->from);
  sendMsg(id, (char*) msg, ports[id].info_port, sizeof(struct RequestPageMessage));
}

int sendPgInfoMsg(struct PageInfoMessage *msg, client_id_t id) {
  sendMsg(id, (char*) msg, ports[id].info_port, sizeof(struct PageInfoMessage));
}

int sendAllocMessage(struct AllocMessage* msg, client_id_t id) {
  sendMsg(id, (char*) msg, ports[id].alloc_port, sizeof(struct AllocMessage));
}
