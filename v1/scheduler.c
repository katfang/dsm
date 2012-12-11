#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#include "scheduler.h"

struct task *head = 0, **tail = &head, *current;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

struct task * generate_task(void (*func)(void *), void *arg) {
  struct task *t = malloc(sizeof(struct task));
  t->func = func;
  t->arg = arg;
  t->next = 0;
  t->deps = 0;
  t->parent = 0;
  return t;
}

struct task * enqueue_task(struct task *t) {
  pthread_mutex_lock(&lock);
  *tail = t;
  tail = &t->next;
  pthread_mutex_unlock(&lock);
  return t;
}

// schedule a task to run
struct task * enqueue(void (*func)(void *), void *arg) {
  return enqueue_task(generate_task(func, arg));
}

void whatsgoinon() {
  struct task *n = head;
  printf("\n[");
  while(n) {
    printf("%p, ", n);
    n = n->next;
  }
  printf("]\n");
  n = head;
  while(n) {
    printf("%p {func: %p, deps: %d, parent %p}\n", n, n->func, n->deps, n->parent);
    n = n->next;
  }
  exit(1);
}

void task_dependency(struct task *parent, struct task *child) {
  child->parent = parent;
  if(parent->deps++ > 10) {
    whatsgoinon();
  }
}

void task_continues(struct task *cc) {
  if(current && current->parent) {
    task_dependency(current->parent, cc);
  }
}

int has_task() {
  return (head != 0);
}

// runs first unblocked task, freeing it afterwards
void dequeue_and_run_task() {
  if(!head) return;
  pthread_mutex_lock(&lock);
  struct task *n;
  struct task *first;
  n = head;
  head = head->next;
  // re-enqueue n if it has unresolved dependencies
  first = n;
  while(n->deps) { 
    *tail = n;
    n->next = 0;
    tail = &n->next;
    n = head;
    if(first == n) whatsgoinon();
    head = head->next;
  }
  current = n;
  pthread_mutex_unlock(&lock);
  n->func(n->arg);
  if (n->parent) {
    pthread_mutex_lock(&lock);
    n->parent->deps--;
    pthread_mutex_unlock(&lock);
  }
  free(n);
}
