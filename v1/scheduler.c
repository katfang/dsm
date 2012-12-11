#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#include "scheduler.h"

struct task *head = 0, **tail = &head, *current;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

struct task * generate_task(void (*func)(void *, struct task *), void *arg) {
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
  //printf("Enqueueing is hard. Let's go shopping %p\n", t);
  t->next = head;
  if(!head) tail = &t->next;
  head = t;    
  //*tail = t;
  //tail = &t->next;
  pthread_mutex_unlock(&lock);
  return t;
}

// schedule a task to run
struct task * enqueue(void (*func)(void *, struct task *), void *arg) {
  return enqueue_task(generate_task(func, arg));
}

int totaldeps = 0;

void print_task(struct task *t) {
  printf("%p {func: %p, deps: %d, parent %p}\n", t, t->func, t->deps, t->parent);
}

void whatsgoinon() {
  struct task *n = head;
  printf("\ntotaldeps: %d\n", totaldeps);
  printf("[");
  while(n) {
    printf("%p, ", n);
    n = n->next;
  }
  printf("]\n");
  n = head;
  while(n) {
    print_task(n);
    n = n->next;
  }
  exit(1);
}

void task_dependency(struct task *parent, struct task *child) {
  pthread_mutex_lock(&lock);
  if(child->parent) {
    printf("say what?! %p\n", child);
    whatsgoinon();
  }
  child->parent = parent;
  if(parent->deps++ > 10) {
    whatsgoinon();
  }
  totaldeps++;
  //printf("%p->deps incremented to %d\n", parent, parent->deps);
  pthread_mutex_unlock(&lock);
}

int has_task() {
  return (head != 0);
}

// runs first unblocked task, freeing it afterwards
void dequeue_and_run_task() {
  pthread_mutex_lock(&lock);
  if(!head) goto end;
  struct task *n;
  n = head;
  head = head->next;

  while(n->deps) { 
    // re-enqueue n if it has unresolved dependencies
    //printf("reenqueue\n");
    *tail = n;
    n->next = 0;
    tail = &n->next;
    if(!head) head = n;

    pthread_mutex_unlock(&lock);
    pthread_yield();
    pthread_mutex_lock(&lock);

    if(head) {
      n = head;
      head = head->next;
    }
    else goto end;
  }
  pthread_mutex_unlock(&lock);
  //print_task(n);
  n->func(n->arg, n);
  pthread_mutex_lock(&lock);
  if (n->parent) {
    n->parent->deps--;
    totaldeps--;
    //printf("%p->deps decremented to %d\n", n->parent, n->parent->deps);
  }
  free(n);
    end:
  pthread_mutex_unlock(&lock);
}
