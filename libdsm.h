#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/ucontext.h>
#include <pthread.h>

#define SHM_NAME "/blah"

#define PTE_P 1
#define PTE_R 2
#define PTE_W 4

void * dsm_open(void * addr, size_t size);
void dsm_close();
