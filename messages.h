#ifndef DSM_MESSAGES_H
#define DSM_MESSAGES_H

#include <stdint.h>
#include "pagedata.h"

enum msg_t {
    READ,
    WRITE
};

typedef uint64_t copyset_t; // limits us to 64 processes
typedef uint64_t client_id_t; // given copyset representation, this should probably
                              // be in [0, 64]

client_id_t lowest_id(copyset_t id_set);

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
