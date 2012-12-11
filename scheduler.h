#ifndef _SCHEDULER_H
#define _SCHEDULER_H

struct task {
  void (*func)(void *, struct task *);
  void *arg;

  struct task *next;

  int deps; // dependencies remaining
  struct task *parent;
};

struct task *generate_task(void (*func)(void *, struct task *), void *arg);
struct task *enqueue_task(struct task *t);
struct task *enqueue(void (*func)(void *, struct task *), void *arg);
void task_dependency(struct task *parent, struct task *child);
int has_task();
void dequeue_and_run_task();

#endif // _SCHEDULER_H
