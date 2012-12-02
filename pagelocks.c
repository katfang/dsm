#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "debug.h"
#include "pagedata.h"

#define DEBUG 1

// These tables reflect the standard 4-level 64-bit page table, whose levels
// are PGD, PUD, PMD, and PTE.

typedef pthread_mutex_t *lme_t;
typedef lme_t *lue_t;
typedef lue_t *lge_t;

lge_t lock_table[TBLENTRIES];
pthread_mutex_t initializer_lock = PTHREAD_MUTEX_INITIALIZER;

int
page_lock(void *va)
{
  int i;
  
  lge_t *lge = &lock_table[PGX(va)];
  if(! *lge) {
    pthread_mutex_lock(&initializer_lock);
    *lge = malloc(sizeof(lue_t) * TBLENTRIES);
    memset(*lge, 0, sizeof(lue_t) * TBLENTRIES);
    pthread_mutex_unlock(&initializer_lock);
  }
  
  lue_t *lue = &(*lge)[PUX(va)];
  if(! *lue) {
    pthread_mutex_lock(&initializer_lock);
    *lue = malloc(sizeof(lme_t) * TBLENTRIES);
    memset(*lue, 0, sizeof(lme_t) * TBLENTRIES);
    pthread_mutex_unlock(&initializer_lock);
  }

  lme_t *lme = &(*lue)[PMX(va)];
  if(! *lme) {
    pthread_mutex_lock(&initializer_lock);
    *lme = malloc(sizeof(pthread_mutex_t) * TBLENTRIES);
    for(i = 0; i < TBLENTRIES; i++) {
      pthread_mutex_init(&(*lme)[i], NULL);
    }
    pthread_mutex_unlock(&initializer_lock);
  }

  pthread_mutex_t *lock = &(*lme)[PTX(va)]; 
//  if (DEBUG) printf("[pagelocks] locking page %p from thread #%ld\n", va, (long)pthread_self());
  DEBUG_LOG("locking page %p from thread #%ld", va, (long)pthread_self());
  int r = pthread_mutex_lock(lock);
  DEBUG_LOG("locked page %p\n from thread #%ld", va, (long)pthread_self());
  return r;
}

int
page_unlock(void *va)
{
  lge_t *lge = &lock_table[PGX(va)];
  if(! *lge) return -E_NO_ENTRY;
  
  lue_t *lue = &(*lge)[PUX(va)];
  if(! *lue) return -E_NO_ENTRY;

  lme_t *lme = &(*lue)[PMX(va)];
  if(! *lme) return -E_NO_ENTRY;

  pthread_mutex_t *lock = &(*lme)[PTX(va)];
  if (DEBUG) printf("[pagelocks] unlocking page %p\n", va);
  return pthread_mutex_unlock(lock);
  
}
