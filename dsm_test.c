#include "libdsm.h"

void * 
loop(void *xa) {
  unsigned i;
  void *addr = (void*) 0xdeadbeef000;

  for (i = 0; i < 5; i++) {
    printf("value at %p is 0x%x.\nTrying to write %x\n", 
      addr, ((int*)addr)[0], 0xcafebabe + i);
    ((int*)addr)[0]= 0xcafebabe + i;
    printf("value at %p is 0x%x.\n", addr, ((int*)addr)[0]);
    pthread_yield();
  }
}

int 
main() {
  void *addr = dsm_open((void*) 0xdeadbeef000, (size_t) 4096, loop);

  printf("Exiting.\n");
  dsm_close();
  exit(0);
}
