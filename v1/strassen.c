/**
 * Taken from: http://en.wikipedia.org/wiki/Strassen_algorithm
 * Compile this without linking since there is no main method.
 * Assuming that the file name is Strassen.c this can be done using gcc:
 *   gcc -c strassen.c
 */
 
#include <stdio.h>
#include <stdlib.h>

#include "strassen.h"
#include "scheduler.h"

#define MIN_BLOCK (1 << 0)
 
void mmult(double **a, double **b, double **c, int tam);
void strassen(double **a, double **b, double **c, int tam, struct task *t);
void strassen_boxed(void *a, struct task *t);
void sum(double **a, double **b, double **result, int tam);
void subtract(double **a, double **b, double **result, int tam);
struct strassen_args *args(double **a, double **b, double **c,
			   int tam, int freea, int freeb);
double **allocate_real_matrix(int tam, int random);
double **free_real_matrix(double **v, int tam);

// naive matrix multiplication
void mmult(double **a, double **b, double **c, int tam) {
  int i, j, k;
  for (i = 0; i < tam; i++) {
    for (j = 0; j < tam; j++) {
      c[i][j] = 0;
      for (k = 0; k < tam; k++) {
        c[i][j] += a[i][k] * b[k][j];
      }
    }
  }
}
 
void strassen(double **a, double **b, double **c, int tam, struct task *cur) {
 
  if (tam <= MIN_BLOCK) {
    mmult(a, b, c, tam);
    return;
  }
 
  // other cases are treated here:
  int newTam = tam/2;
  double **a11, **a12, **a21, **a22;
  double **b11, **b12, **b21, **b22;
  double **p1, **p2, **p3, **p4, **p5, **p6, **p7;
  double **r1, **r2, **r3, **r4, **r5,
    **r6, **r7, **r8, **r9, **r10;
 
  // memory allocation:
  a11 = allocate_real_matrix(newTam, -1);
  a12 = allocate_real_matrix(newTam, -1);
  a21 = allocate_real_matrix(newTam, -1);
  a22 = allocate_real_matrix(newTam, -1);
 
  b11 = allocate_real_matrix(newTam, -1);
  b12 = allocate_real_matrix(newTam, -1);
  b21 = allocate_real_matrix(newTam, -1);
  b22 = allocate_real_matrix(newTam, -1);
 
  p1 = allocate_real_matrix(newTam, -1);
  p2 = allocate_real_matrix(newTam, -1);
  p3 = allocate_real_matrix(newTam, -1);
  p4 = allocate_real_matrix(newTam, -1);
  p5 = allocate_real_matrix(newTam, -1);
  p6 = allocate_real_matrix(newTam, -1);
  p7 = allocate_real_matrix(newTam, -1);

  r1 = allocate_real_matrix(newTam, -1);
  r2 = allocate_real_matrix(newTam, -1);
  r3 = allocate_real_matrix(newTam, -1);
  r4 = allocate_real_matrix(newTam, -1);
  r5 = allocate_real_matrix(newTam, -1);
  r6 = allocate_real_matrix(newTam, -1);
  r7 = allocate_real_matrix(newTam, -1);
  r8 = allocate_real_matrix(newTam, -1);
  r9 = allocate_real_matrix(newTam, -1);
  r10 = allocate_real_matrix(newTam, -1);
 
  int i, j;
 
  //dividing the matrices in 4 sub-matrices:
  for (i = 0; i < newTam; i++) {
    for (j = 0; j < newTam; j++) {
      a11[i][j] = a[i][j];
      a12[i][j] = a[i][j + newTam];
      a21[i][j] = a[i + newTam][j];
      a22[i][j] = a[i + newTam][j + newTam];
 
      b11[i][j] = b[i][j];
      b12[i][j] = b[i][j + newTam];
      b21[i][j] = b[i + newTam][j];
      b22[i][j] = b[i + newTam][j + newTam];
    }
  }

  struct strassen_continuation *cc =
    malloc(sizeof(struct strassen_continuation));
  struct task *continuation = generate_task(strassen_continue, cc);

  struct strassen_args *cc1, *cc2, *cc3, *cc4, *cc5, *cc6, *cc7;
  struct task *t;
 
  // Calculating p1 to p7:
  sum(a11, a22, r1, newTam); // a11 + a22
  sum(b11, b22, r2, newTam); // b11 + b22
  cc1 = args(r1, r2, p1, newTam, 1, 1); // p1 = (a11+a22) * (b11+b22)
  t = generate_task(strassen_boxed, cc1);
  task_dependency(continuation, t);
  enqueue_task(t);
 
  sum(a21, a22, r3, newTam); // a21 + a22
  cc2 = args(r3, b11, p2, newTam, 1, 0); // p2 = (a21+a22) * (b11)
  t = generate_task(strassen_boxed, cc2);
  task_dependency(continuation, t);
  enqueue_task(t);
 
  subtract(b12, b22, r4, newTam); // b12 - b22
  cc3 = args(a11, r4, p3, newTam, 0, 1); // p3 = (a11) * (b12 - b22)
  t = generate_task(strassen_boxed, cc3);
  task_dependency(continuation, t);
  enqueue_task(t);
 
  subtract(b21, b11, r5, newTam); // b21 - b11
  cc4 = args(a22, r5, p4, newTam, 0, 1); // p4 = (a22) * (b21 - b11)
  t = generate_task(strassen_boxed, cc4);
  task_dependency(continuation, t);
  enqueue_task(t);
 
  sum(a11, a12, r6, newTam); // a11 + a12
  cc5 = args(r6, b22, p5, newTam, 1, 0); // p5 = (a11+a12) * (b22) 
  t = generate_task(strassen_boxed, cc5);
  task_dependency(continuation, t);
  enqueue_task(t);
 
  subtract(a21, a11, r7, newTam); // a21 - a11
  sum(b11, b12, r8, newTam); // b11 + b12
  cc6 = args(r7, r8, p6, newTam, 1, 1); // p6 = (a21-a11) * (b11+b12)
  t = generate_task(strassen_boxed, cc6);
  task_dependency(continuation, t);
  enqueue_task(t);
 
  subtract(a12, a22, r9, newTam); // a12 - a22
  sum(b21, b22, r10, newTam); // b21 + b22
  cc7 = args(r9, r10, p7, newTam, 1, 1); // p7 = (a12-a22) * (b21+b22)
  t = generate_task(strassen_boxed, cc7);
  task_dependency(continuation, t);
  enqueue_task(t);

  free_real_matrix(a12, newTam);
  free_real_matrix(a21, newTam);
 
  free_real_matrix(b12, newTam);
  free_real_matrix(b21, newTam);

  cc->newTam = newTam;
  cc->a11 = a11; cc->a22 = a22;
  cc->b11 = b11; cc->b22 = b22;
  cc->p1 = p1; cc->p2 = p2; cc->p3 = p3; cc->p4 = p4;
  cc->p5 = p5; cc->p6 = p6; cc->p7 = p7;
  cc->c = c;
  
  if(cur && cur->parent) task_dependency(cur->parent, continuation);
  enqueue_task(continuation);
}

void strassen_continue(void *a, struct task *t) {
  struct strassen_continuation *args = a;
  //printf("\e[32mcontinue\e[0m ");
  printf("\e[3%dmlevel\e[0m %d ", args->newTam, args->newTam);
  if(args->newTam != MIN_BLOCK) printf("\n");

  int newTam = args->newTam;
  double **c = args->c;
 
  double **c11 = allocate_real_matrix(newTam, -1);
  double **c12 = allocate_real_matrix(newTam, -1);
  double **c21 = allocate_real_matrix(newTam, -1);
  double **c22 = allocate_real_matrix(newTam, -1);
 
  double **aResult = allocate_real_matrix(newTam, -1);
  double **bResult = allocate_real_matrix(newTam, -1);

  // calculating c21, c21, c11 & c22:
 
  sum(args->p3, args->p5, c12, newTam); // c12 = p3 + p5
  sum(args->p2, args->p4, c21, newTam); // c21 = p2 + p4
 
  sum(args->p1, args->p4, aResult, newTam); // p1 + p4
  sum(aResult, args->p7, bResult, newTam); // p1 + p4 + p7
  subtract(bResult, args->p5, c11, newTam); // c11 = p1 + p4 - p5 + p7
 
  sum(args->p1, args->p3, aResult, newTam); // p1 + p3
  sum(aResult, args->p6, bResult, newTam); // p1 + p3 + p6
  subtract(bResult, args->p2, c22, newTam); // c22 = p1 + p3 - p2 + p6
 
  int i, j;

  // Grouping the results obtained in a single matrix:
  for (i = 0; i < newTam ; i++) {
    for (j = 0 ; j < newTam ; j++) {
      c[i][j] = c11[i][j];
      c[i][j + newTam] = c12[i][j];
      c[i + newTam][j] = c21[i][j];
      c[i + newTam][j + newTam] = c22[i][j];
    }
  }
 
  // deallocating memory (free):
  free_real_matrix(args->a11, newTam);
  free_real_matrix(args->a22, newTam);
 
  free_real_matrix(args->b11, newTam);
  free_real_matrix(args->b22, newTam);
 
  c11 = free_real_matrix(c11, newTam);
  c12 = free_real_matrix(c12, newTam);
  c21 = free_real_matrix(c21, newTam);
  c22 = free_real_matrix(c22, newTam);
 
  free_real_matrix(args->p1, newTam);
  free_real_matrix(args->p2, newTam);
  free_real_matrix(args->p3, newTam);
  free_real_matrix(args->p4, newTam);
  free_real_matrix(args->p5, newTam);
  free_real_matrix(args->p6, newTam);
  free_real_matrix(args->p7, newTam);

  aResult = free_real_matrix(aResult, newTam);
  bResult = free_real_matrix(bResult, newTam);
} // end of Strassen function

void strassen_boxed(void *a, struct task *t) {
  struct strassen_args *args = a;
  //  printf("strassen %d\n", args->tam);
  strassen(args->a, args->b, args->c, args->tam, t);
  if(args->freea) free_real_matrix(args->a, args->tam);
  if(args->freeb) free_real_matrix(args->b, args->tam);
  free(args);
}
 
/*------------------------------------------------------------------------------*/
// function to sum two matrices
void sum(double **a, double **b, double **result, int tam) {
 
  int i, j;
 
  for (i = 0; i < tam; i++) {
    for (j = 0; j < tam; j++) {
      result[i][j] = a[i][j] + b[i][j];
    }
  }
}
 
/*------------------------------------------------------------------------------*/
// function to subtract two matrices
void subtract(double **a, double **b, double **result, int tam) {
 
  int i, j;
 
  for (i = 0; i < tam; i++) {
    for (j = 0; j < tam; j++) {
      result[i][j] = a[i][j] - b[i][j];
    }
  }   
}

/*------------------------------*/
struct strassen_args *args(double **a, double **b, double **c,
			   int tam, int freea, int freeb) {
  struct strassen_args *cc = malloc(sizeof(struct strassen_args));
  cc->a = a;
  cc->b = b;
  cc->c = c;
  cc->tam = tam;
  cc->freea = freea;
  cc->freeb = freeb;
  return cc;
}
 
/*------------------------------------------------------------------------------*/
// This function allocates the matrix using malloc, and initializes it. If the variable random is passed
// as zero, it initializes the matrix with zero, if it's passed as 1, it initializes the matrix with random
// values. If it is passed with any other int value (like -1 for example) the matrix is initialized with no
// values in it. The variable tam defines the length of the matrix.
double **allocate_real_matrix(int tam, int random) {
 
  int i, j, n = tam, m = tam;
  double **v, a;     // pointer to the vector
 
  // allocates one vector of vectors (matrix)
  v = (double**) malloc(n * sizeof(double*));
 
  if (v == NULL) {
    printf ("** Error in matrix allocation: insufficient memory **");
    return (NULL);
  }
 
  // allocates each row of the matrix
  for (i = 0; i < n; i++) {
    v[i] = (double*) malloc(m * sizeof(double));
 
    if (v[i] == NULL) {
      printf ("** Error: Insufficient memory **");
      free_real_matrix(v, n);
      return (NULL);
    }
 
    // initializes the matrix with zeros
    if (random == 0) {
      for (j = 0; j < m; j++)
        v[i][j] = 0.0;
    }
 
    // initializes the matrix with random values between 0 and 10
    else {
      if (random == 1) {
        for (j = 0; j < m; j++) {
          a = rand();
          v[i][j] = (a - (int)a) * 10;
        }
      }
    }
  }
 
  return (v);   // returns the pointer to the vector. 
}
 
/*------------------------------------------------------------------------------*/
// This function unallocated the matrix (frees memory)
double **free_real_matrix(double **v, int tam) {
 
  int i;
 
  if (v == NULL) {
    return (NULL);
  }
 
  for (i = 0; i < tam; i++) { 
    if (v[i]) { 
      free(v[i]); // frees a row of the matrix
      v[i] = NULL;
    } 
  } 
 
  free(v);     // frees the pointer /
  v = NULL;
 
  return (NULL);   //returns a null pointer /
}
 
/*------------------------------------------------------------------------------*/
