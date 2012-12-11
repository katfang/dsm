struct task {
  void (*func)(void *);
  void *arg;

  struct task *next;

  int deps; // dependencies remaining
  struct task *parent;
};

struct task *generate_task(void (*func)(void *), void *arg);
struct task *enqueue_task(struct task *t);
struct task *enqueue(void (*func)(void *), void *arg);
void task_dependency(struct task *parent, struct task *child);
void task_continues(struct task *cc);
int has_task();
void dequeue_and_run_task();
