#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pagelocks.h"

#define DEBUG 1

// These tables reflect the standard 4-level 64-bit page table, whose levels
// are PGD, PUD, PMD, and PTE.

typedef data_t *lme_t;
typedef lme_t *lue_t;
typedef lue_t *lge_t;

lge_t owner_table[TBLENTRIES];
pthread_mutex_t initializer_lock = PTHREAD_MUTEX_INITIALIZER;

int
set_page_owner(void *va, data_t id)
{
  int i;
  
  lge_t *lge = &owner_table[PGX(va)];
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
    *lme = malloc(sizeof(data_t) * TBLENTRIES);
    memset(*lme, 0, sizeof(data_t) * TBLENTRIES);
    pthread_mutex_unlock(&initializer_lock);
  }

  if (DEBUG) printf("Setting data of %p to %d\n", va, id);
  (*lme)[PTX(va)] = id;

  return 0;
}

int
get_page_owner(void *va, data_t *id_out)
{
  lge_t *lge = &lock_table[PGX(va)];
  if(! *lge) return -E_NO_ENTRY;
  
  lue_t *lue = &(*lge)[PUX(va)];
  if(! *lue) return -E_NO_ENTRY;

  lme_t *lme = &(*lue)[PMX(va)];
  if(! *lme) return -E_NO_ENTRY;

  *id_out = (*lme)[PTX(va)];
  return 0;
  
}
