#ifndef DSM_COPYSET_H
#define DSM_COPYSET_H
#include <stdint.h>

typedef uint64_t copyset_t;
typedef uint64_t client_id_t; // given copyset representation, this should probably
                              // be in [0, 64]

client_id_t lowest_id(copyset_t id_set);
copyset_t add_to_copyset(copyset_t copyset, client_id_t id);
#endif
