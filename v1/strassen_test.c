#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include "scheduler.h"
#include "strassen.h"

#define MAT_DIMEN (1 << 7)
#define MIN(x,y) (((x) < (y)) ? (x) : (y))

void print_matrix(char* label, double **m);
int main(void);

void print_matrix(char* label, double **m) {
  int i, j;

  printf("%s:\n", label);
  for (i = 0; i < MIN(MAT_DIMEN, MAT_DIMEN); i++) {
    for (j = 0; j < MIN(MAT_DIMEN, MAT_DIMEN); j++) {
      printf("%d ", (int)m[i][j]);
    }
    printf("\n");
  }
}

void * consume_tasks(void *v) {
  while(has_task()) {
    //printf("running subtask: ");
    dequeue_and_run_task();
  }
  return NULL;
}

int main(void) {
  double **a, **b, **c;
  int i = 0, j = 0;
  srand(123);

  a = (double**) malloc (sizeof(double*) * MAT_DIMEN);
  b = (double**) malloc (sizeof(double*) * MAT_DIMEN);
  c = (double**) malloc (sizeof(double*) * MAT_DIMEN);
  
  for (i = 0; i < MAT_DIMEN; i++) {
    a[i] = (double*) malloc(sizeof(double) * MAT_DIMEN);
    b[i] = (double*) malloc(sizeof(double) * MAT_DIMEN);
    c[i] = (double*) malloc(sizeof(double) * MAT_DIMEN);
    for (j = 0; j < MAT_DIMEN; j++) {
      a[i][j] = rand() % 5;
      b[i][j] = rand() % 5;
    }
  }

  time_t start = time(NULL);
  printf("here\n");
  strassen( a, b, c, MAT_DIMEN, NULL);
  printf("here\n");
  
  pthread_t thr;
  pthread_create(&thr, NULL, &consume_tasks, NULL);
  consume_tasks(NULL);
  pthread_join(thr, NULL);

  time_t end = time(NULL);
  print_matrix("a", a);
  print_matrix("b", b);
  print_matrix("c", c);
  printf("Elapsed time: %ld seconds\n", end - start);
  
  return 0;
}
