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
#include "messages.h"

#define MAXBUFLEN 700

char *ips[] = {
  "127.0.0.1", // actually server
  "127.0.0.1",
  "127.0.0.1",
  "127.0.0.1",
};

char *ports[] = {
  "14000", // actually server
  "14001",
  "14002",
  "14003",
};

int send_to_client(client_id_t id, void * msg, size_t size);
