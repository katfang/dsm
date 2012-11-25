#include <netinet.h>

enum msg_t {
    READ,
    WRITE
};

typedef client_id_t int; // given copyset representation, this should probably
                         // be in [0, 64)

#pragma pack(push)
#pragma pack(1)
struct RequestPageMessage {
    enum msg_t type;
    void *pg_address;   
    client_id_t from;
};

struct SendPageMessage {
    enum msg_t type;
    void *pg_address;
    client_id_t send_to;
}

struct PageInfoMessage {
    enum msg_t type;
    void *pg_address;
    uint64_t copyset; // irrelevant if type != WRITE
                     // also limits us to 64 processes
    int pg_size;
    char pg_contents[];
}
#pragma pack(pop)
