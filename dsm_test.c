#include "libdsm.h"

int main() {
  dsm_init(1);
  void *addr = dsm_open((void*) 0xdeadbeef000, (size_t) 4096);
  int i = 0;
  
  while(1) {
     printf("value at %p is 0x%x.\nTrying to write %x\n", 
      addr, ((int*)addr)[0], 0xcafebabe + i);
    ((int*)addr)[0]= 0xcafebabe + i;
    printf("value at %p is 0x%x.\n", addr, ((int*)addr)[0]);
    i++;
  }

  dsm_close();
}
