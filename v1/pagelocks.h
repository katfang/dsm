#ifndef DSM_PAGELOCKS_H
#define DSM_PAGELOCKS_H

// These return the corresponding return codes from pthread_mutex_lock
// and pthread_mutex_unlock. Also, page_unlock will return -E_NO_ENTRY
// if the lock hasn't been initialized yet.

int page_lock(void *va);
int page_unlock(void *va);

#endif // DSM_PAGELOCKS_H
