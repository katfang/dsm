#ifndef DSM_MESSAGES_H
#define DSM_MESSAGES_H

#include "copyset.h"

enum msg_t {
    READ,
    WRITE
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
    copyset_t copyset; // irrelevant if type != WRITE
                     // also limits us to 64 processes
    int pg_size;
    char pg_contents[];
};
#pragma pack(pop)

#endif
