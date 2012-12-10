#include <stdio.h>
#include <stdlib.h>

#include "libdsm.h"

int main(int argc, char* argv[]) {
  int id;

  if (argc != 2) {
    printf("Usage: %s id\n", argv[0]);
    exit(1);
  }

  id = atoi(argv[1]);
  printf("id is %d\n", id); 
  dsm_init(id);

  int *addr = (int *) dsm_open((void*) 0xdeadbeef000, (size_t) 64);
  int i = 0;
  
  while(1) {
    printf("value at %p is 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x. \n", // Trying to write %x to %p\n", 
      addr, addr[0], addr[1], addr[2], addr[3], addr[4], addr[5], 0xcafebabe + i, &addr[id]);
    addr[id]= 0xcafebabe + i;
    printf("value at %p is 0x%x.\n\n", &addr[id], addr[id]);
    i++;
  }
}
