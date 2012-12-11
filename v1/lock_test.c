#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#include "libdsm.h"

#define ADDR (void*)(0xcafebeef000)

int main(int argc, char* argv[]) {
  pthread_mutex_t *m = ADDR;
}
