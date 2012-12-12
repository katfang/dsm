#ifndef _DSM_SCHEDULER_H
#define _DSM_SCHEDULER_H

#include <pthread.h>
#include "scheduler.h"

#define MASTER_PAGE (void *)0xC0000001000
#define MASTER_LOCK ((pthread_mutex_t *)(MASTER_PAGE + 0))
#define WORKER_TALLY (*(int *)(MASTER_PAGE + 0x40))
#define MASTER_FINISHED (*(int *)(MASTER_PAGE + 0x50))

#define SCHED_PAGE (void *)0xC0000000000
#define SCHED_LOCK ((pthread_mutex_t *)(SCHED_PAGE + 0))

#define DATA_SPACE (void *)0xC1000000000
#define DATA_SIZE  0x1000000

#endif // _DSM_SCHEDULER_H
