#include <stdint.h>

// We use a 4-level table for these because we're using a 64-bit with 4 KB
// pages.

#define PGSIZE 4096
#define PGSHIFT 12
#define TBLSHIFT 9
#define TBLENTRIES (1 << TBLSHIFT)

#define PTXSHIFT PGSHIFT
#define PMXSHIFT (PTXSHIFT + TBLSHIFT)
#define PUXSHIFT (PMXSHIFT + TBLSHIFT)
#define PGXSHIFT (PUXSHIFT + TBLSHIFT)

#define PTX(va)    ((((uint64_t) (va)) >> PTXSHIFT) & 0x1FF)
#define PMX(va)    ((((uint64_t) (va)) >> PMXSHIFT) & 0x1FF)
#define PUX(va)    ((((uint64_t) (va)) >> PUXSHIFT) & 0x1FF)
#define PGX(va)    ((((uint64_t) (va)) >> PGXSHIFT) & 0x1FF)
#define PGOFF(va)  (((uint64_t) (va)) & 0xFFF) 

#define E_NO_ENTRY 1309

// These return the corresponding return codes from pthread_mutex_lock
// and pthread_mutex_unlock. Also, page_unlock will return -E_NO_ENTRY
// if the lock hasn't been initialized yet.

int page_lock(void *va);
int page_unlock(void *va);
