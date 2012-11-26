#include <stdio.h>
#include <stdlib.h>

#include "libdsm.h"

int main(int argc, char* argv[]) {
  if (argc != 2) {
    printf("Usage: %s id\n", argv[0]);
    exit(1);
  }
  printf("id is %d\n", atoi(argv[1]));
  dsm_init(atoi(argv[1]));
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
