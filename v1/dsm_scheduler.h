#ifndef _DSM_SCHEDULER_H
#define _DSM_SCHEDULER_H

#include <pthread.h>
#include "scheduler.h"

#define SCHED_PAGE (void *)0xC0000000000
#define MASTER_LOCK ((pthread_mutex_t *)(SCHED_PAGE + 0))
#define SCHED_LOCK ((pthread_mutex_t *)(SCHED_PAGE + 0x40))
#define WORKER_TALLY (*(int *)(SCHED_PAGE + 0x80))

#define DATA_SPACE (void *)0xC1000000000
#define DATA_SIZE  0x1000000

#endif // _DSM_SCHEDULER_H
