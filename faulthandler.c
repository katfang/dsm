#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>

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


int main() {
  // create a new shared memory object to map.  could also do existing page, but
  // I'm somehow too lazy.  Using a shared memory object is the only
  // POSIX-portable approch, but in Linux you can modify any mapped page.
  int fd = shm_open("/blah", O_RDWR|O_CREAT|O_EXCL, S_IRWXU);
  ftruncate(fd, 4096);
  void *addr = mmap((void*)0xdeadbeef000, 4096, PROT_READ, MAP_PRIVATE, fd, 0);
  printf("mapped new read-only page at %p\n", addr);

  // sigsegv handler initialization
  struct sigaction s;
  s.sa_sigaction = handler;
  sigemptyset(&s.sa_mask);
  s.sa_flags = SA_SIGINFO;

  sigaction(SIGSEGV, &s, NULL);
  printf("value at %p is 0x%x.\nTrying to write 0xcafebabe\n", addr, ((int*)addr)[0]);
  ((int*)addr)[0]=0xcafebabe;
  printf("value at %p is 0x%x.\nExiting\n", addr, ((int*)addr)[0]);
  shm_unlink("/blah");
  close(fd);

}
