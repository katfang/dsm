#ifndef _DSM_NETWORK_H
#define _DSM_NETWORK_H
#define __STDC_FORMAT_MACROS
      
#define MAXBUFLEN 5000
#include "copyset.h"
#include "messages.h"

struct PortInfo {
  int info_port;
  int req_port;
};

extern char *ips[];
extern struct PortInfo ports[];

int open_serv_socket(int port);
int open_socket(char *host, int port);

struct PageInfoMessage *recvPgInfoMsg(int sockfd);
struct RequestPageMessage *recvReqPgMsg(int sockfd);
int sendReqPgMsg(struct RequestPageMessage *msg, client_id_t id);
int sendAckMsg(struct RequestPageMessage *msg, client_id_t id);
int sendPgInfoMsg(struct PageInfoMessage *msg, client_id_t id);

#endif
