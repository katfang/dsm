#ifndef _STRASSEN_H
#define _STRASSEN_H

#include "scheduler.h"

struct strassen_args {
  double **a, **b, **c;
  int tam;
  int freea, freeb;
};

struct strassen_continuation {
  int newTam;
  double **a11, **a22;
  double **b11, **b22;
  double **p1, **p2, **p3, **p4, **p5, **p6, **p7;
  double **c;
};

void strassen(double **a, double **b, double **c, int tam, struct task *t);
void strassen_continue(void *a, struct task *t);
void strassen_boxed(void *a, struct task *t);

#endif // _STRASSEN_H
