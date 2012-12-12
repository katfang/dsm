#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>

#include "debug.h"
#include "libdsm.h"
#include "dsm_scheduler.h"
#include "strassen.h"

#define DEBUG 1

#define MAT_DIMEN (1 << 5)
#define MIN(x,y) (((x) < (y)) ? (x) : (y))

void print_matrix(char* label, double **m);

void print_matrix(char* label, double **m) {
  int i, j;

  printf("%s:\n", label);
  for (i = 0; i < MIN(MAT_DIMEN, 4); i++) {
    for (j = 0; j < MIN(MAT_DIMEN, 4); j++) {
      printf("%d ", (int)m[i][j]);
    }
    printf("\n");
  }
}

void consume_tasks() {
  while(has_task()) { // from scheduler.h
    dequeue_and_run_task();
  }
}

int main(int argc, char *argv[]) {
  time_t start;
  double **a, **b, **c;
  int id, master = 0;
  
  if (argc != 2) {
    printf("Usage: %s id\n", argv[0]);
    exit(1);
  }

  id = atoi(argv[1]);
  printf("id is %d\n", id); 
  dsm_init(id);

  dsm_reserve(SCHED_PAGE, 0x2000);
  DEBUG_LOG("GETTING %d SPACE", DATA_SIZE);
  dsm_open(DATA_SPACE, DATA_SIZE);
  dsm_lock_init(MASTER_LOCK);
  DEBUG_LOG("init'd masterlock");
  dsm_lock_init(SCHED_LOCK);
  DEBUG_LOG("init'd schedlock");

  while (pthread_mutex_trylock(MASTER_LOCK) == EBUSY); // pthread_mutex_lock(MASTER_LOCK);
  DEBUG_LOG("locked masterlock");

  master = !(WORKER_TALLY++);
  DEBUG_LOG("Tally incremented to %d", WORKER_TALLY);

  if (master) {
    printf("I'm master. Setting up arrays.\n");
    master = 1;
    MASTER_FINISHED = 0;
    int i = 0, j = 0;
    srand(123);

    a = (double**) dsm_malloc(sizeof(double*) * MAT_DIMEN);
    b = (double**) dsm_malloc(sizeof(double*) * MAT_DIMEN);
    c = (double**) dsm_malloc(sizeof(double*) * MAT_DIMEN);
  
    for (i = 0; i < MAT_DIMEN; i++) {
      a[i] = (double*) dsm_malloc(sizeof(double) * MAT_DIMEN);
      b[i] = (double*) dsm_malloc(sizeof(double) * MAT_DIMEN);
      c[i] = (double*) dsm_malloc(sizeof(double) * MAT_DIMEN);
      for (j = 0; j < MAT_DIMEN; j++) {
        a[i][j] = rand() % 5;
        b[i][j] = rand() % 5;
      }
    }

    start = time(NULL);
    
    printf("Queueing up strassens\n");
    strassen(a, b, c, MAT_DIMEN, NULL);
  }
  pthread_mutex_unlock(MASTER_LOCK);

  printf("Consuming tasks\n");
  consume_tasks(NULL);
  printf("Finished with tasks\n");
  while (pthread_mutex_trylock(MASTER_LOCK) == EBUSY); // pthread_mutex_lock(MASTER_LOCK);
  WORKER_TALLY--;
  DEBUG_LOG("Tally is %d", WORKER_TALLY);
  pthread_mutex_unlock(MASTER_LOCK);

  while(WORKER_TALLY || (!master && !MASTER_FINISHED)) {
    DEBUG_LOG("Tally: %d, master: %d, master_finished: %d",
	      WORKER_TALLY, master, MASTER_FINISHED);
    pthread_yield();
  }

  if (master) {
    time_t end = time(NULL);

    print_matrix("a", a);
    print_matrix("b", b);
    print_matrix("c", c);
    printf("Elapsed time: %ld seconds\n", end - start);

    MASTER_FINISHED = 1;
  }
  
  return 0;
}
