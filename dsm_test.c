#include "libdsm.h"

int main() {
  void *addr = dsm_open((void*) 0xdeadbeef000, (size_t) 4096);
  printf("mapped new read-only page at %p\n", addr);

  printf("value at %p is 0x%x.\nTrying to write 0xcafebabe\n", 
    addr, ((int*)addr)[0]);
  ((int*)addr)[0]=0xcafebabe;
  printf("value at %p is 0x%x.\nExiting\n", addr, ((int*)addr)[0]);

  dsm_close();
  exit(0);
}
