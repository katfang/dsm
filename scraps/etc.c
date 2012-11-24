#include <pthread.h>
#include <stdio.h>

pthread_mutex_t mut;

int main() {
  printf("Size of mutex: %x\n", sizeof(mut));
}
