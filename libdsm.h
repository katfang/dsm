#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <pthread.h>

#define SHM_NAME "/blah"

void * dsm_open(void * addr, size_t size);
void dsm_close();
