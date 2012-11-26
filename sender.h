#ifndef DSM_SENDER_H
#define DSM_SENDER_H
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

extern char *ips[];
extern char *ports[];

int send_to_client(client_id_t id, void * msg, size_t size);
#endif
