/**
 * TODO stuff:
 * We want our _init() to not only set up the page fault handler, but also to
 * somehow set up the page server.
 *
 */

//#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <pthread.h>

// TODO define the data structure containing client per-page locks here.  May
// need to switch to C++ 

void handler(int signum, siginfo_t *info, void *ucontext) {
  printf("fault handled at address %p!\n", info->si_addr);
  printf("marking page at %p as writable\n", info->si_addr);
  int r;
  r = mprotect(info->si_addr, 4096, PROT_READ | PROT_WRITE);
  if (r < 0) {
    printf("error code %d\n", errno);
  } else {
    printf("marked as writable\n");
  }
}

void *server_loop(void *params) {
    // set up server, then loop reading connections.
    while (1) {
        printf("hello there\n");
        pthread_yield();
    }
}

void _init(void) {
  printf("Loading hack.\nRegistering fault handler\n");
  // sigsegv handler initialization
  struct sigaction s;
  s.sa_sigaction = handler;
  sigemptyset(&s.sa_mask);
  s.sa_flags = SA_SIGINFO;
  sigaction(SIGSEGV, &s, NULL);

  pthread_t serv_thread;
  pthread_create(&serv_thread, NULL, server_loop, NULL);
}
