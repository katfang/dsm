#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#include "dsm_scheduler.h"
#include "libdsm.h"

#define head (*(struct task **)(SCHED_PAGE + 0x88))
#define tail (*(struct task ***)(SCHED_PAGE + 0x90))
#define waiting (*(int *)(SCHED_PAGE + 0x98))

struct task * generate_task(void (*func)(void *, struct task *), void *arg) {
  struct task *t = dsm_malloc(sizeof(struct task));
  t->func = func;
  t->arg = arg;
  t->next = 0;
  t->deps = 0;
  t->parent = 0;
  return t;
}

struct task * enqueue_task(struct task *t) {
  pthread_mutex_lock(SCHED_LOCK);
  //printf("Queues are hard. Let's go shopping %p\n", t);
  t->next = head;
  if(!head) tail = &t->next;
  head = t;    
  //*tail = t;
  //tail = &t->next;
  pthread_mutex_unlock(SCHED_LOCK);
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

void whats_goin_on() {
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
  pthread_mutex_lock(SCHED_LOCK);
  if(child->parent) {
    printf("say what?! %p\n", child);
    whats_goin_on();
  }
  child->parent = parent;
  if(parent->deps++ > 10) {
    whats_goin_on();
  }
  totaldeps++;
  //printf("%p->deps incremented to %d\n", parent, parent->deps);
  pthread_mutex_unlock(SCHED_LOCK);
}

int has_task() {
  return (head != 0);
}

// runs first unblocked task, freeing it afterwards
void dequeue_and_run_task() {
  pthread_mutex_lock(SCHED_LOCK);
  if(!head) goto end;
  struct task *n;
  n = head;
  head = head->next;
  struct task *first = n;
  int bored = 0;

  while(n->deps) { 
    // re-enqueue n if it has unresolved dependencies
    //printf("reenqueue\n");
    *tail = n;
    n->next = 0;
    tail = &n->next;
    if(!head) head = n;

    if (first == n) {
      //printf("spun through list\n");
      bored = 1;
      waiting = 1;
    }

    pthread_mutex_unlock(SCHED_LOCK);
    do { 
      pthread_yield();
    } while(bored && waiting);
    pthread_mutex_lock(SCHED_LOCK);

    if(head) {
      n = head;
      head = head->next;
    }
    else goto end;
  }
  pthread_mutex_unlock(SCHED_LOCK);
  //print_task(n);
  n->func(n->arg, n);
  pthread_mutex_lock(SCHED_LOCK);
  if (n->parent) {
    n->parent->deps--;
    totaldeps--;
    //printf("%p->deps decremented to %d\n", n->parent, n->parent->deps);
  }
  waiting = 0;
  dsm_free(n, sizeof(struct task));
    end:
  pthread_mutex_unlock(SCHED_LOCK);
}
