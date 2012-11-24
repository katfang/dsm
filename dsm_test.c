#include "libdsm.h"

void * 
loop(void *xa) {
  unsigned i;
  void *addr = (void*) 0xdeadbeef000;

  for (i = 0; i < 10; i++) {
    printf("value at %p is 0x%x.\nTrying to write 0xcafebabe\n", 
      addr, ((int*)addr)[0]);
    ((int*)addr)[0]=0xcafebabe;
    printf("value at %p is 0x%x.\n", addr, ((int*)addr)[0]);
    pthread_yield();
  }
}

int 
main() {
  void *addr = dsm_open((void*) 0xdeadbeef000, (size_t) 4096, loop);
  printf("mapped new read-only page at %p\n", addr);

  printf("Exiting.\n");
  dsm_close();
  exit(0);
}
