/* Copyright Abandoned 1996 TCX DataKonsult AB & Monty Program KB & Detron HB
   This file is public domain and comes with NO WARRANTY of any kind */

/* For use with thr_lock:s */

#ifndef _thr_lock_h
#define _thr_lock_h
#ifdef	__cplusplus
extern "C" {
#endif

#include <my_pthread.h>
#include <list.h>

struct st_thr_lock;

enum thr_lock_type { TL_UNLOCK, TL_READ,  TL_READ_HIGH_PRIORITY,
		     TL_WRITE_ALLOW_READ, TL_WRITE_DELAYED,
		     TL_WRITE, TL_WRITE_ONLY};

typedef struct st_thr_lock_data {
  pthread_t thread;
  struct st_thr_lock_data *next,**prev;
  struct st_thr_lock *lock;
  pthread_cond_t *cond;
  enum thr_lock_type type;
  ulong thread_id;
} THR_LOCK_DATA;

struct st_lock_list {
  THR_LOCK_DATA *data,**last;
};

typedef struct st_thr_lock {
  LIST list;
  pthread_mutex_t mutex;
  struct st_lock_list read_wait;
  struct st_lock_list read;
  struct st_lock_list write_wait;
  struct st_lock_list write;
} THR_LOCK;


my_bool init_thr_lock(void);		/* Must be called once/thread */
void thr_lock_init(THR_LOCK *lock);
void thr_lock_delete(THR_LOCK *lock);
void thr_lock_data_init(THR_LOCK *lock,THR_LOCK_DATA *data);
int thr_lock(THR_LOCK_DATA *data,enum thr_lock_type lock_type);
void thr_unlock(THR_LOCK_DATA *data);
int thr_multi_lock(THR_LOCK_DATA **data,uint count);
void thr_multi_unlock(THR_LOCK_DATA **data,uint count);
void thr_abort_locks(THR_LOCK *lock);
void thr_print_locks(void);		/* For debugging */
#ifdef	__cplusplus
}
#endif
#endif /* _thr_lock_h */
