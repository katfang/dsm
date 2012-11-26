#define __STDC_FORMAT_MACROS
#include <inttypes.h> 
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pagedata.h"

#define DEBUG 1

struct DataTable *
alloc_data_table()
{  
  struct DataTable *tbl = malloc(sizeof(struct DataTable));
  tbl->table = malloc(sizeof(pge_t) * TBLENTRIES);
  memset(tbl->table, 0, sizeof(pge_t) * TBLENTRIES);
  pthread_mutex_init(&tbl->lock, NULL);
  return tbl;
}

int
set_page_data(struct DataTable *tbl, void *va, data_t id)
{
  int i;

  if (DEBUG) printf("set_page_data at %p\n", va);
  pge_t *pge = &(tbl->table)[PGX(va)];
  if(! *pge) {
    pthread_mutex_lock(&tbl->lock);
    *pge = malloc(sizeof(pue_t) * TBLENTRIES);
    memset(*pge, 0, sizeof(pue_t) * TBLENTRIES);
    pthread_mutex_unlock(&tbl->lock);
  }
  
  pue_t *pue = &(*pge)[PUX(va)];
  if(! *pue) {
    pthread_mutex_lock(&tbl->lock);
    *pue = malloc(sizeof(pme_t) * TBLENTRIES);
    memset(*pue, 0, sizeof(pme_t) * TBLENTRIES);
    pthread_mutex_unlock(&tbl->lock);
  }

  pme_t *pme = &(*pue)[PMX(va)];
  if(! *pme) {
    pthread_mutex_lock(&tbl->lock);
    *pme = malloc(sizeof(data_t) * TBLENTRIES);
    memset(*pme, 0, sizeof(data_t) * TBLENTRIES);
    pthread_mutex_unlock(&tbl->lock);
  }

  if (DEBUG) printf("Setting data of %p to %" PRIu64 "\n", va, id);
  (*pme)[PTX(va)] = id;

  return 0;
}

int
get_page_data(struct DataTable *tbl, void *va, data_t *id_out)
{
  pge_t *pge = &(tbl->table)[PGX(va)];
  if(! *pge) return -E_NO_ENTRY;
  
  pue_t *pue = &(*pge)[PUX(va)];
  if(! *pue) return -E_NO_ENTRY;

  pme_t *pme = &(*pue)[PMX(va)];
  if(! *pme) return -E_NO_ENTRY;

  *id_out = (*pme)[PTX(va)];
  return 0;
  
}
