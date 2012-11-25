#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEBUG 1

struct DataTable *
alloc_data_table()
{
  struct DataTable *tbl = malloc(sizeof(struct DataTable));
  tbl->table = malloc(sizeof(lge_t) * TBLENTRIES);
  memset(tbl->table, 0, sizeof(lge_t) * TBLENTRIES);
  pthread_mutex_init(&tbl->lock, NULL);
}

int
set_page_owner(struct DataTable *tbl, void *va, data_t id)
{
  int i;
  
  lge_t *lge = &(tbl->table)[PGX(va)];
  if(! *lge) {
    pthread_mutex_lock(&tbl->lock);
    *lge = malloc(sizeof(lue_t) * TBLENTRIES);
    memset(*lge, 0, sizeof(lue_t) * TBLENTRIES);
    pthread_mutex_unlock(&tbl->lock);
  }
  
  lue_t *lue = &(*lge)[PUX(va)];
  if(! *lue) {
    pthread_mutex_lock(&tbl->lock);
    *lue = malloc(sizeof(lme_t) * TBLENTRIES);
    memset(*lue, 0, sizeof(lme_t) * TBLENTRIES);
    pthread_mutex_unlock(&tbl->lock);
  }

  lme_t *lme = &(*lue)[PMX(va)];
  if(! *lme) {
    pthread_mutex_lock(&tbl->lock);
    *lme = malloc(sizeof(data_t) * TBLENTRIES);
    memset(*lme, 0, sizeof(data_t) * TBLENTRIES);
    pthread_mutex_unlock(&tbl->lock);
  }

  if (DEBUG) printf("Setting data of %p to %d\n", va, id);
  (*lme)[PTX(va)] = id;

  return 0;
}

int
get_page_owner(struct DataTable *tbl, void *va, data_t *id_out)
{
  lge_t *lge = &(tbl->table)[PGX(va)];
  if(! *lge) return -E_NO_ENTRY;
  
  lue_t *lue = &(*lge)[PUX(va)];
  if(! *lue) return -E_NO_ENTRY;

  lme_t *lme = &(*lue)[PMX(va)];
  if(! *lme) return -E_NO_ENTRY;

  *id_out = (*lme)[PTX(va)];
  return 0;
  
}
