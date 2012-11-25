#ifndef DSM_PAGEDATA_H
#define DSM_PAGEDATA_H

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

typedef uint64_t data_t;

int set_page_data(void *va, data_t id);
int get_page_data(void *va, data_t *id_out);

#endif // DSM_PAGEDATA_H
