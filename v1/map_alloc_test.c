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

  int *addr = (int *) dsm_open((void*) 0xdeadbeef000, (size_t) 0xF000);
  // addr = (int *) dsm_malloc((size_t) 4);
  // printf("Alloc'd: %p, size 4\n", addr);
  addr = (int *) dsm_malloc((size_t) 0x1000);
  printf("Alloc'd: %p, size %x\n", addr, 0x1000);
  addr = (int *) dsm_malloc((size_t) 4);
  printf("Alloc'd: %p, size %x\n", addr, 4);
  addr = (int *) dsm_malloc((size_t) 0x2000);
  printf("Alloc'd: %p, size %x\n", addr, 0x2000);
  return 0;
}
