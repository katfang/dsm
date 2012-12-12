#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#include "debug.h"
#include "dsm_scheduler.h"
#include "libdsm.h"

#define DEBUG 0

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
  while (pthread_mutex_trylock(SCHED_LOCK) == EBUSY); //pthread_mutex_lock(SCHED_LOCK);
  //DEBUG_LOG("Queues are hard. Let's go shopping %p", t);
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
  DEBUG_LOG("%p {func: %p, deps: %d, parent %p}", t, t->func, t->deps, t->parent);
}

void whats_goin_on() {
  struct task *n = head;
  DEBUG_LOG("totaldeps: %d", totaldeps);
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
  // exit(1);
}

void task_dependency(struct task *parent, struct task *child) {
  while (pthread_mutex_trylock(SCHED_LOCK) == EBUSY); //pthread_mutex_lock(SCHED_LOCK);
  if(child->parent) {
    DEBUG_LOG("say what?! %p", child);
    whats_goin_on();
  }
  child->parent = parent;
  if(parent->deps++ > 10) {
    DEBUG_LOG("a what's going on");
    whats_goin_on();
  }
  totaldeps++;
  //DEBUG_LOG("%p->deps incremented to %d", parent, parent->deps);
  pthread_mutex_unlock(SCHED_LOCK);
}

int has_task() {
  return (head != 0);
}

// runs first unblocked task, freeing it afterwards
void dequeue_and_run_task() {
  while (pthread_mutex_trylock(SCHED_LOCK) == EBUSY); //pthread_mutex_lock(SCHED_LOCK);
  if(!head) goto end;
  struct task *n;
  n = head;
  head = head->next;
  struct task *first = n;
  int bored = 0;

  while(n->deps) { 
    // re-enqueue n if it has unresolved dependencies
    //DEBUG_LOG("reenqueue");
    *tail = n;
    n->next = 0;
    tail = &n->next;
    if(!head) head = n;

    /*if (first == n) {
      if (bored) {
        DEBUG_LOG("I'm bored.");
        whats_goin_on();
      }
      //DEBUG_LOG("spun through list");
      bored = 1;
      waiting = 1;
      }*/

    pthread_mutex_unlock(SCHED_LOCK);
    //int times = 0;
    //do { 
      // DEBUG_LOG("yielding ... ");
      pthread_yield();
      // DEBUG_LOG("... back");
      //} while(bored && waiting && times++ < 3000);
    while (pthread_mutex_trylock(SCHED_LOCK) == EBUSY);

    if(head) {
      n = head;
      head = head->next;
    }
    else goto end;
  }
  //  bored = 0;
  pthread_mutex_unlock(SCHED_LOCK);
  //print_task(n);
  n->func(n->arg, n);
  while (pthread_mutex_trylock(SCHED_LOCK) == EBUSY);
  if (n->parent) {
    n->parent->deps--;
    totaldeps--;
    //DEBUG_LOG("%p->deps decremented to %d", n->parent, n->parent->deps);
  }
  //  waiting = 0;
  dsm_free(n, sizeof(struct task));
    end:
  pthread_mutex_unlock(SCHED_LOCK);
}
