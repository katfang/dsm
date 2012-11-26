#ifndef _DSM_MESSAGES_H
#define _DSM_MESSAGES_H

#include "pagedata.h"
#include "copyset.h"

enum msg_t {
    READ,
    WRITE,
    INVAL // used with RequestPageMessage only
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

#pragma pack(pop)

#endif
