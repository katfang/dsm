#ifndef _LIBDSM_H
#define _LIBDSM_H

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/ucontext.h>
#include <pthread.h>

#include "copyset.h"

#define PTE_P 1
#define PTE_W 2
#define PTE_R 4

void dsm_init(client_id_t myId);
void * dsm_open(void *addr, size_t size);
void * dsm_malloc(size_t size);
void * dsm_reserve(void *addr, size_t size);
void dsm_free(void *addr, size_t size);
int dsm_lock_init(void *addr);

#endif // _LIBDSM_H
