#ifndef _DSM_MESSAGES_H
#define _DSM_MESSAGES_H

#include "pagedata.h"
#include "copyset.h"

enum msg_t {
  READ,
  WRITE,
  INVAL, // used with RequestPageMessage only
  ACK,
  MALLOC,
  MALLOC_RESPONSE,
  DSM_OPEN,
  DSM_OPEN_RESPONSE,
  RESERVE,
  RESERVE_RESPONSE,
  FREE,
  FREE_RESPONSE
};

#pragma pack(push)
#pragma pack(1)
struct RequestPageMessage {
  enum msg_t type;
  void *pg_address;   
  client_id_t from;
};

struct PageInfoMessage {
  enum msg_t type;
  void *pg_address;
  copyset_t copyset;                    
  char pg_contents[PGSIZE];
};

/** Used for dsm_malloc, dsm_open */
struct AllocMessage {
  enum msg_t type;
  void *pg_address;
  size_t size;
  client_id_t from;
};

#pragma pack(pop)

#endif
