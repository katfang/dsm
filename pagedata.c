#define __STDC_FORMAT_MACROS
#include <inttypes.h> 
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "debug.h"
#include "pagedata.h"

#define DEBUG 1

struct DataTable *
alloc_data_table()
{  
  struct DataTable *tbl = malloc(sizeof(struct DataTable));
  tbl->table = malloc(sizeof(pge_t) * TBLENTRIES);
  memset(tbl->table, 0, sizeof(pge_t) * TBLENTRIES);
  pthread_mutex_init(&tbl->lock, NULL);
  tbl->do_get_faults = 1;
  return tbl;
}

int
set_page_data(struct DataTable *tbl, void *va, data_t id)
{
  int i;

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

  DEBUG_LOG("set_page_data of %p from %lu to %lu", va, (*pme)[PTX(va)], id);
  (*pme)[PTX(va)] = id;

  return 0;
}

int
get_page_data(struct DataTable *tbl, void *va, data_t *id_out)
{
  pge_t *pge = &(tbl->table)[PGX(va)];
  if(! *pge) goto get_fault;
  
  pue_t *pue = &(*pge)[PUX(va)];
  if(! *pue) goto get_fault;

  pme_t *pme = &(*pue)[PMX(va)];
  if(! *pme) goto get_fault;

  *id_out = (*pme)[PTX(va)];
  return 0;
  
 get_fault:
  if (tbl->do_get_faults)
    return -E_NO_ENTRY;
  *id_out = 0;
  return 0;
}
