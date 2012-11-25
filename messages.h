#ifndef DSM_MESSAGES_H
#define DSM_MESSAGES_H

#include <stdint.h>

enum msg_t {
    READ,
    WRITE
};

typedef uint64_t copyset_t;
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
    uint64_t copyset; // irrelevant if type != WRITE
                     // also limits us to 64 processes
    int pg_size;
    char pg_contents[];
};
#pragma pack(pop)

#endif
