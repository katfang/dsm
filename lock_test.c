#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <errno.h>

#include "libdsm.h"

#define ADDR (void*)(0xcafebeef000)

pthread_mutex_t m;

int main(int argc, char* argv[]) {
  int id;

  if (argc != 2) {
    printf("Usage: %s id\n", argv[0]);
    exit(1);
  }

  id = atoi(argv[1]);
  printf("id is %d\n", id); 
  printf("Size of pthread_mutex_t is %lu\n", sizeof(pthread_mutex_t));
  dsm_init(id);

  void *addr = dsm_reserve(ADDR, (size_t) 4096);
  if (dsm_lock_init(addr) == 0) {
    printf("successfully made lock.\n");
  }

  pthread_mutex_t *m = (pthread_mutex_t *) addr;
  int ch;

  while (1) {
    if (pthread_mutex_trylock(m) == EBUSY) {
      printf("Didn't get lock. Try again? (press enter) ");
    } else {
      printf("Got lock. release? (press enter)");
      getchar();
      pthread_mutex_unlock(m);
      printf("released. get lock? (press enter)");
    }
    getchar();
  }
}
