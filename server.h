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

#define MAXBUFLEN 5000

int open_socket(char * port);
void listen_on_socket(int sockfd, void* (*handler) (void*));
