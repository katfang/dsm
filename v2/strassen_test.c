#include <stdio.h>
#include <stdlib.h>
#include "strassen.h"

#define MAT_DIMEN 4

void print_matrix(char* label, double **m);
int main(void);

void print_matrix(char* label, double **m) {
  int i, j;

  printf("%s:\n", label);
  for (i = 0; i < MAT_DIMEN; i++) {
    for (j = 0; j < MAT_DIMEN; j++) {
      printf("%f ", m[i][j]);
    }
    printf("\n");
  }
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

  printf("here");
  strassen( a, b, c, 4);
  printf("here");
  print_matrix("a", a);
  print_matrix("b", b);
  print_matrix("c", c);
  
  return 0;
}
