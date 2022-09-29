/* Copyright Abandoned 1996 TCX DataKonsult AB & Monty Program KB & Detron HB
   This file is public domain and comes with NO WARRANTY of any kind */

/*
Read and write locks for Posix threads. All tread must acquire
all locks it needs through thr_multi_lock() to avoid dead-locks.
A lock consists of a master lock (THR_LOCK), and lock instances (THR_LOCK_DATA).
Any thread can have any number of lock instances (read and write:s) on
any lock. All lock instances must be freed.
Locks are prioritized according to:

WRITE_DELAYED, READ, WRITE and WRITE_ONLY

Locks in the same privilege level are scheduled in first-in-first-out order.
*/

#if !defined(MAIN) && !defined(DBUG_OFF) && !defined(EXTRA_DEBUG)
#define DBUG_OFF
#endif

#include "mysys_priv.h"
#include <m_string.h>
#include <thr_lock.h>
#include <errno.h>

/* The following constants are only for debug output */
#define MAX_THREADS 100
#define MAX_LOCKS   100


static LIST *thread_list;			/* List of threads in use */

static inline pthread_cond_t *get_cond(void)
{
  return &my_thread_var->suspend;
}

/*
** For the future (now the thread specific cond is alloced by my_pthread.c)
*/

my_bool init_thr_lock()
{
  return 0;
}

#ifdef EXTRA_DEBUG
static int found_errors=0;

int check_lock(struct st_lock_list *list,char* lock_type,char *where)
{
  THR_LOCK_DATA *data,**prev;
  uint count=0;

  if (list->data)
  {
    prev= &list->data;
    for (data=list->data; data && count++ < MAX_LOCKS ; data=data->next)
    {
      if (data->prev != prev)
      {
	fprintf(stderr,
		"Warning: prev link %d didn't point at previous lock at %s: %s\n",
		count, lock_type, where);
	return 1;
      }
      prev= &data->next;
    }
    if (data)
    {
      fprintf(stderr,"Warning: found too many locks at %s: %s\n",
	      lock_type,where);
      return 1;
    }
    else if (prev != list->last)
    {
      fprintf(stderr,"Warning: last didn't point at last lock at %s: %s\n",
	      lock_type, where);
      return 1;
    }
  }
  return 0;
}

void check_locks(THR_LOCK *lock, char *where)
{
  if (!found_errors)
  {
    if (check_lock(&lock->write,"write",where) |
	check_lock(&lock->write_wait,"write_wait",where) |
	check_lock(&lock->read,"read",where) |
	check_lock(&lock->read_wait,"read_wait",where))
    {
      found_errors=1;
      DBUG_PRINT("error",("Found wrong lock"));
    }
  }
}

#else /* EXTRA_DEBUG */
#define check_locks(A,B)
#endif


	/* Initialize a lock */

void thr_lock_init(THR_LOCK *lock)
{
  DBUG_ENTER("thr_lock_init");
  bzero((char*) lock,sizeof(*lock));
  VOID(pthread_mutex_init(&lock->mutex,NULL));
  lock->read.last= &lock->read.data;
  lock->read_wait.last= &lock->read_wait.data;
  lock->write_wait.last= &lock->write_wait.data;
  lock->write.last= &lock->write.data;

  pthread_mutex_lock(&THR_LOCK_lock);		/* Add to locks in use */
  lock->list.data=(void*) lock;
  thread_list=list_add(thread_list,&lock->list);
  pthread_mutex_unlock(&THR_LOCK_lock);
  DBUG_VOID_RETURN;
}


void thr_lock_delete(THR_LOCK *lock)
{
  DBUG_ENTER("thr_lock_delete");
  VOID(pthread_mutex_destroy(&lock->mutex));
  pthread_mutex_lock(&THR_LOCK_lock);
  thread_list=list_delete(thread_list,&lock->list);
  pthread_mutex_unlock(&THR_LOCK_lock);
  DBUG_VOID_RETURN;
}

	/* Initialize a lock instance */

void thr_lock_data_init(THR_LOCK *lock,THR_LOCK_DATA *data)
{
  data->lock=lock;
  data->type=TL_UNLOCK;
  data->thread=pthread_self();
  data->thread_id=my_thread_id();		/* for debugging */
}


static inline my_bool have_old_read_lock(THR_LOCK_DATA *data,pthread_t thread)
{
  for ( ; data ; data=data->next)
  {
    if ((pthread_equal(data->thread,thread)))
      return 1;					/* Already locked by thread */
  }
  return 0;
}

static int wait_for_lock(struct st_lock_list *wait, THR_LOCK_DATA *data)
{
  pthread_cond_t *cond=get_cond();
  struct st_my_thread_var *thread_var=my_thread_var;
  int result;

  (*wait->last)=data;				/* Wait for lock */
  data->prev= wait->last;
  wait->last= &data->next;

  /* Set up control struct to allow others to abort locks */
  pthread_mutex_lock(&thread_var->mutex);
  thread_var->current_mutex= &data->lock->mutex;
  thread_var->current_cond=  cond;
  pthread_mutex_unlock(&thread_var->mutex);

  data->cond=cond;
  do
  {
    pthread_cond_wait(cond,&data->lock->mutex);
  } while (data->cond == cond && !thread_var->abort);

  if (data->cond || data->type == TL_UNLOCK)
  {
    if (data->cond)				/* aborted */
    {
      if ((*data->prev)=data->next)		/* remove from wait-list */
	data->next->prev= data->prev;
      else
	wait->last=data->prev;
    }
    data->type=TL_UNLOCK;			/* No lock */
    result=1;					/* Didn't get lock */
  }
  else
  {
    result=0;
    check_locks(data->lock,"got wait_for_lock");
  }
  pthread_mutex_unlock(&data->lock->mutex);

  /* The following must be done after unlock of lock->mutex */
  pthread_mutex_lock(&thread_var->mutex);
  thread_var->current_mutex= 0;
  thread_var->current_cond=  0;
  pthread_mutex_unlock(&thread_var->mutex);
  return result;
}


int thr_lock(THR_LOCK_DATA *data,enum thr_lock_type lock_type)
{
  THR_LOCK *lock=data->lock;
  int result=0;
  DBUG_ENTER("thr_lock");

  data->next=0;
  data->type=lock_type;
  data->thread=pthread_self();			/* Must be reset ! */
  data->thread_id=my_thread_id();		/* Must be reset ! */
  VOID(pthread_mutex_lock(&lock->mutex));
  DBUG_PRINT("lock",("data: %lx  thread: %ld  lock: %lx  type: %d",
		      data,data->thread_id,lock,(int) lock_type));
  check_locks(lock,(uint) lock_type <= (uint) TL_READ_HIGH_PRIORITY ?
	      "enter read_lock" : "enter write_lock");
  if ((uint) lock_type <= (uint) TL_READ_HIGH_PRIORITY)
  {
    if (lock->write.data)
    {
      DBUG_PRINT("lock",("write locked by thread: %ld",
			 lock->write.data->thread_id));
      if (pthread_equal(data->thread,lock->write.data->thread) ||
	  lock->write.data->type == TL_WRITE_ALLOW_READ)
      {						/* Already got a write lock */
	(*lock->read.last)=data;		/* Add to running FIFO */
	data->prev=lock->read.last;
	lock->read.last= &data->next;
	check_locks(lock,"read lock with old write lock");
	goto end;
      }
      if (lock->write.data->type == TL_WRITE_ONLY)
      {
	data->type=TL_UNLOCK;
	result=1;				/* Can't wait for this one */
	goto end;
      }
    }
    else if (!lock->write_wait.data ||
	     lock->write_wait.data->type == TL_WRITE_DELAYED ||
	     lock_type == TL_READ_HIGH_PRIORITY ||
	     have_old_read_lock(lock->read.data,data->thread))
    {						/* No write-locks */
      (*lock->read.last)=data;			/* Add to running FIFO */
      data->prev=lock->read.last;
      lock->read.last= &data->next;
      check_locks(lock,"read lock with no write locks");
      goto end;
    }
    /* Can't get lock yet */
    DBUG_RETURN(wait_for_lock(&lock->read_wait,data));
  }
  else						/* lock for writing */
  {
    if (lock->write.data)
    {
      /*
	This will not work if the old lock was a TL_WRITE_ALLOW_READ,
	but this will never happen with MySQL.
       */
      if (pthread_equal(data->thread,lock->write.data->thread))
      {						/* Already got a write lock */
	(*lock->write.last)=data;		/* Add to running fifo */
	data->prev=lock->write.last;
	lock->write.last= &data->next;
	check_locks(lock,"second write lock");
	goto end;
      }
      DBUG_PRINT("lock",("write locked by thread: %ld",
			  lock->write.data->thread_id));
    }
    else
    {
      if (!lock->write_wait.data &&
	  (data->type == TL_WRITE_ALLOW_READ || !lock->read.data))
      {
	(*lock->write.last)=data;		/* Add as current write lock */
	data->prev=lock->write.last;
	lock->write.last= &data->next;
	check_locks(lock,"only write lock");
	goto end;
      }
      DBUG_PRINT("lock",("write locked by thread: %ld, type: %ld",
			 lock->read.data->thread_id,data->type));
    }
    DBUG_RETURN(wait_for_lock(&lock->write_wait,data));
  }
end:
  pthread_mutex_unlock(&lock->mutex);
  DBUG_RETURN(result);
}


	/* Unlock lock and free next thread on same lock */

void thr_unlock(THR_LOCK_DATA *data)
{
  THR_LOCK *lock=data->lock;
  DBUG_ENTER("thr_unlock");
  pthread_mutex_lock(&lock->mutex);
  DBUG_PRINT("lock",("data: %lx  thread: %ld  lock: %lx",
		     data,data->thread_id,lock));
  check_locks(lock,"start of release lock");

  if ((*data->prev)=data->next)		/* remove from lock-list */
    data->next->prev= data->prev;
  else if (data->type == TL_READ || data->type == TL_READ_HIGH_PRIORITY)
    lock->read.last=data->prev;
  else
    lock->write.last=data->prev;
  data->type=TL_UNLOCK;			/* Mark unlocked */
  check_locks(lock,"after releasing lock");

  if (!lock->write.data && !lock->read.data) /* If no more locks in use */
  {
    /* Release write-locks first */
    if ((data=lock->write_wait.data) &&
	(data->type != TL_WRITE_DELAYED || !lock->read_wait.data))
    {
      if ((*data->prev)=data->next)	/* remove from wait-list */
	data->next->prev= data->prev;
      else
	lock->write_wait.last=data->prev;

      (*lock->write.last)=data;		/* Put in execute list */
      data->prev=lock->write.last;
      data->next=0;
      lock->write.last= &data->next;
      DBUG_PRINT("lock",("giving write lock to thread: %ld",
			 data->thread_id));
      check_locks(lock,"giving write lock");
      {
	pthread_cond_t *cond=data->cond;
	data->cond=0;				/* Mark thread free */
	VOID(pthread_cond_signal(cond));	/* Start waiting thread */
      }
      if (data->type != TL_WRITE_ALLOW_READ)
      {
	pthread_mutex_unlock(&lock->mutex);
	DBUG_VOID_RETURN;
      }
      /* Allow possible read locks */
    }
    if ((data=lock->read_wait.data))
    {
      (*lock->read.last)=data;		/* move locks from waiting list */
      data->prev=lock->read.last;
      lock->read.last=lock->read_wait.last;

      lock->read_wait.data=0;
      lock->read_wait.last= &lock->read_wait.data;

      check_locks(lock,"giving read locks");
      do
      {
	pthread_cond_t *cond=data->cond;
	DBUG_PRINT("lock",("giving read lock to thread: %ld",
			   data->thread_id));
	data->cond=0;				/* Mark thread free */
	VOID(pthread_cond_signal(cond));
      } while ((data=data->next));
    }
    else
    {
      DBUG_PRINT("lock",("No locks to free"));
    }
  }
  pthread_mutex_unlock(&lock->mutex);
  DBUG_VOID_RETURN;
}


/*
** Get all locks in a specific order to avoid dead-locks
** Sort acording to lock position and put write_locks before read_locks if
** lock on same lock.
*/


#define LOCK_CMP(A,B) ((byte*) (A->lock) - (uint) ((A)->type) < (byte*) (B->lock)- (uint) ((B)->type))

static void sort_locks(THR_LOCK_DATA **data,uint count)
{
  THR_LOCK_DATA **pos,**end,**prev,*tmp;

  /* Sort locks with insertion sort (fast because almost always few locks) */

  for (pos=data+1,end=data+count; pos < end ; pos++)
  {
    tmp= *pos;
    if (LOCK_CMP(tmp,pos[-1]))
    {
      prev=pos;
      do {
	prev[0]=prev[-1];
      } while (--prev != data && LOCK_CMP(tmp,prev[-1]));
      prev[0]=tmp;
    }
  }
}


int thr_multi_lock(THR_LOCK_DATA **data,uint count)
{
  THR_LOCK_DATA **pos,**end;
  DBUG_ENTER("thr_multi_lock");
  DBUG_PRINT("lock",("data: %lx  count: %d",data,count));
  if (count > 1)
    sort_locks(data,count);
  /* lock everything */
  for (pos=data,end=data+count; pos < end ; pos++)
  {
    if (thr_lock(*pos,(*pos)->type))
    {						/* Aborted */
      thr_multi_unlock(data,(uint) (pos-data));
      DBUG_RETURN(1);
    }
#ifdef MAIN
    printf("Thread: %s  Got lock: %lx  type: %d\n",my_thread_name(),
	   pos[0]->lock, pos[0]->type); fflush(stdout);
#endif
  }
  DBUG_RETURN(0);
}

  /* free all locks */

void thr_multi_unlock(THR_LOCK_DATA **data,uint count)
{
  THR_LOCK_DATA **pos,**end;
  DBUG_ENTER("thr_multi_unlock");
  DBUG_PRINT("lock",("data: %lx  count: %d",data,count));

  for (pos=data,end=data+count; pos < end ; pos++)
  {
#ifdef MAIN
    printf("Thread: %s  Rel lock: %lx  type: %d\n",
	   my_thread_name(), pos[0]->lock, pos[0]->type); fflush(stdout);
#endif
    if ((*pos)->type != TL_UNLOCK)
      thr_unlock(*pos);
    else
    {
      DBUG_PRINT("lock",("Free lock: data: %lx  thread: %ld  lock: %lx",
			 *pos,(*pos)->thread_id,(*pos)->lock));
    }
  }
  DBUG_VOID_RETURN;
}

/* Abort all threads waiting for a lock. The lock will be upgraded to a
  TL_WRITE_ONLY to abort any new accesses to the lock
*/

void thr_abort_locks(THR_LOCK *lock)
{
  THR_LOCK_DATA *data;
  DBUG_ENTER("thr_abort_locks");
  pthread_mutex_lock(&lock->mutex);

  for (data=lock->read_wait.data; data ; data=data->next)
  {
    data->type=TL_UNLOCK;			/* Mark killed */
    pthread_cond_signal(data->cond);
    data->cond=0;				/* Removed from list */
  }
  for (data=lock->write_wait.data; data ; data=data->next)
  {
    data->type=TL_UNLOCK;
    pthread_cond_signal(data->cond);
    data->cond=0;
  }
  lock->read_wait.last= &lock->read_wait.data;
  lock->write_wait.last= &lock->write_wait.data;
  lock->read_wait.data=lock->write_wait.data=0;
  if (lock->write.data)
    lock->write.data->type=TL_WRITE_ONLY;
  pthread_mutex_unlock(&lock->mutex);
  DBUG_VOID_RETURN;
}


#include <my_sys.h>

void thr_print_lock(my_string name,struct st_lock_list *list)
{
  THR_LOCK_DATA *data,**prev;
  uint count=0;

  if (list->data)
  {
    printf("%-10s: ",name);
    prev= &list->data;
    for (data=list->data; data && count++ < MAX_LOCKS ; data=data->next)
    {
      printf("%lx (%ld:%d); ",data,data->thread_id,(int) data->type);
      if (data->prev != prev)
	printf("\nWarning: prev didn't point at previous lock\n");
      prev= &data->next;
    }
    puts("");
    if (prev != list->last)
      printf("Warning: last didn't point at last lock\n");
  }
}

void thr_print_locks(void)
{
  LIST *list;
  uint count=0;

  pthread_mutex_lock(&THR_LOCK_lock);
  puts("Current locks:");
  for (list=thread_list ; list && count++ < MAX_THREADS ; list=rest(list))
  {
    THR_LOCK *lock=(THR_LOCK*) list->data;
    VOID(pthread_mutex_lock(&lock->mutex));
    printf("lock: %lx:",lock);
    if ((lock->write_wait.data || lock->read_wait.data) &&
	(! lock->read.data && ! lock->write.data))
      printf(" WARNING: ");
    if (lock->write.data)
      printf(" write");
    if (lock->write_wait.data)
      printf(" write_wait");
    if (lock->read.data)
      printf(" read");
    if (lock->read_wait.data)
      printf(" read_wait");
    puts("");
    thr_print_lock("write",&lock->write);
    thr_print_lock("write_wait",&lock->write_wait);
    thr_print_lock("read",&lock->read);
    thr_print_lock("read_wait",&lock->read_wait);
    VOID(pthread_mutex_unlock(&lock->mutex));
    puts("");
  }
  fflush(stdout);
  pthread_mutex_unlock(&THR_LOCK_lock);
}

#ifdef MAIN

struct st_test {
  uint lock_nr;
  enum thr_lock_type lock_type;
};

THR_LOCK locks[5];			/* 4 locks */

struct st_test test_0[] = {{0,TL_READ}};	/* One lock */
struct st_test test_1[] = {{0,TL_READ},{0,TL_WRITE}}; /* Read and write lock of lock 0 */
struct st_test test_2[] = {{1,TL_WRITE},{0,TL_READ},{2,TL_READ}};
struct st_test test_3[] = {{2,TL_WRITE},{1,TL_READ},{0,TL_READ}}; /* Deadlock with test_2 ? */
struct st_test test_4[] = {{0,TL_WRITE},{0,TL_READ},{0,TL_WRITE},{0,TL_READ}};
struct st_test test_5[] = {{0,TL_READ},{1,TL_READ},{2,TL_READ},{3,TL_READ}}; /* Many reads */
struct st_test test_6[] = {{0,TL_WRITE},{1,TL_WRITE},{2,TL_WRITE},{3,TL_WRITE}}; /* Many writes */
struct st_test test_7[] = {{3,TL_READ}};
struct st_test test_8[] = {{1,TL_READ},{2,TL_READ},{3,TL_READ}};	/* Should be quick */
struct st_test test_9[] = {{4,TL_READ_HIGH_PRIORITY}};
struct st_test test_10[] ={{4,TL_WRITE}};
struct st_test test_11[] = {{0,TL_WRITE_DELAYED},{1,TL_WRITE_DELAYED},{2,TL_WRITE_DELAYED},{3,TL_WRITE_DELAYED}}; /* Many writes */
struct st_test test_12[] = {{0,TL_WRITE_ALLOW_READ},{1,TL_WRITE_ALLOW_READ},{2,TL_WRITE_ALLOW_READ},{3,TL_WRITE_ALLOW_READ}}; /* Many writes */

struct st_test *tests[] = {test_0,test_1,test_2,test_3,test_4,test_5,test_6,
			   test_7,test_8,test_9,test_10,test_11,test_12};
int lock_counts[]= {sizeof(test_0)/sizeof(struct st_test),
		    sizeof(test_1)/sizeof(struct st_test),
		    sizeof(test_2)/sizeof(struct st_test),
		    sizeof(test_3)/sizeof(struct st_test),
		    sizeof(test_4)/sizeof(struct st_test),
		    sizeof(test_5)/sizeof(struct st_test),
		    sizeof(test_6)/sizeof(struct st_test),
		    sizeof(test_7)/sizeof(struct st_test),
		    sizeof(test_8)/sizeof(struct st_test),
		    sizeof(test_9)/sizeof(struct st_test),
		    sizeof(test_10)/sizeof(struct st_test),
		    sizeof(test_11)/sizeof(struct st_test),
		    sizeof(test_12)/sizeof(struct st_test)
};


static pthread_cond_t COND_thread_count;
static pthread_mutex_t LOCK_thread_count;
static uint thread_count;
static ulong sum=0;

#define MAX_LOCK_COUNT 8

static void *test_thread(void *arg)
{
  int i,j,param=*((int*) arg);
  THR_LOCK_DATA data[MAX_LOCK_COUNT];
  THR_LOCK_DATA *multi_locks[MAX_LOCK_COUNT];
  my_thread_init();

  printf("Thread %s (%d) started\n",my_thread_name(),param); fflush(stdout);

  for (i=0; i < lock_counts[param] ; i++)
    thr_lock_data_init(locks+tests[param][i].lock_nr,data+i);
  for (j=1 ; j < 10 ; j++)		/* try locking 10 times */
  {
    for (i=0; i < lock_counts[param] ; i++)
    {					/* Init multi locks */
      multi_locks[i]= &data[i];
      data[i].type= tests[param][i].lock_type;
    }
    thr_multi_lock(multi_locks,lock_counts[param]);
    pthread_mutex_lock(&LOCK_thread_count);
    {
      int tmp=rand() & 7;			/* Do something from 0-2 sec */
      if (tmp == 0)
	sleep(1);
      else if (tmp == 1)
	sleep(2);
      else
      {
	ulong k;
	for (k=0 ; k < (ulong) (tmp-2)*100000L ; k++)
	  sum+=k;
      }
    }
    pthread_mutex_unlock(&LOCK_thread_count);
    thr_multi_unlock(multi_locks,lock_counts[param]);
  }

  printf("Tread %s (%d) ended\n",my_thread_name(),param); fflush(stdout);
  thr_print_locks();
  pthread_mutex_lock(&LOCK_thread_count);
  thread_count--;
  VOID(pthread_cond_signal(&COND_thread_count)); /* Tell main we are ready */
  pthread_mutex_unlock(&LOCK_thread_count);
  free((gptr) arg);
  return 0;
}


int main(int argc __attribute__((unused)),char **argv __attribute__((unused)))
{
  pthread_t tid;
  pthread_attr_t thr_attr;
  int i,*param,error;
  MY_INIT(argv[0]);
  if (argc > 1 && argv[1][0] == '-' && argv[1][1] == '#')
    DBUG_PUSH(argv[1]+2);

  printf("Main thread: %s\n",my_thread_name());

  if ((error=pthread_cond_init(&COND_thread_count,NULL)))
  {
    fprintf(stderr,"Got error: %d from pthread_cond_init (errno: %d)",
	    error,errno);
    exit(1);
  }
  if ((error=pthread_mutex_init(&LOCK_thread_count,NULL)))
  {
    fprintf(stderr,"Got error: %d from pthread_cond_init (errno: %d)",
	    error,errno);
    exit(1);
  }

  for (i=0 ; i < (int) array_elements(locks) ; i++)
    thr_lock_init(locks+i);

  if ((error=pthread_attr_init(&thr_attr)))
  {
    fprintf(stderr,"Got error: %d from pthread_attr_init (errno: %d)",
	    error,errno);
    exit(1);
  }
  if ((error=pthread_attr_setdetachstate(&thr_attr,PTHREAD_CREATE_DETACHED)))
  {
    fprintf(stderr,
	    "Got error: %d from pthread_attr_setdetachstate (errno: %d)",
	    error,errno);
    exit(1);
  }
#ifndef pthread_attr_setstacksize		/* void return value */
  if ((error=pthread_attr_setstacksize(&thr_attr,65536L)))
  {
    fprintf(stderr,"Got error: %d from pthread_attr_setstacksize (errno: %d)",
	    error,errno);
    exit(1);
  }
#endif
#ifdef HAVE_THR_SETCONCURRENCY
  VOID(thr_setconcurrency(2));
#endif
  for (i=0 ; i < (int) array_elements(lock_counts) ; i++)
  {
    param=(int*) malloc(sizeof(int));
    *param=i;

    if ((error=pthread_mutex_lock(&LOCK_thread_count)))
    {
      fprintf(stderr,"Got error: %d from pthread_mutex_lock (errno: %d)",
	      error,errno);
      exit(1);
    }
    if ((error=pthread_create(&tid,&thr_attr,test_thread,(void*) param)))
    {
      fprintf(stderr,"Got error: %d from pthread_create (errno: %d)\n",
	      error,errno);
      pthread_mutex_unlock(&LOCK_thread_count);
      exit(1);
    }
    thread_count++;
    pthread_mutex_unlock(&LOCK_thread_count);
  }

  pthread_attr_destroy(&thr_attr);
  if ((error=pthread_mutex_lock(&LOCK_thread_count)))
    fprintf(stderr,"Got error: %d from pthread_mutex_lock\n",error);
  while (thread_count)
  {
    if ((error=pthread_cond_wait(&COND_thread_count,&LOCK_thread_count)))
      fprintf(stderr,"Got error: %d from pthread_cond_wait\n",error);
  }
  if ((error=pthread_mutex_unlock(&LOCK_thread_count)))
    fprintf(stderr,"Got error: %d from pthread_mutex_unlock\n",error);
  for (i=0 ; i < (int) array_elements(locks) ; i++)
    thr_lock_delete(locks+i);
  printf("Test succeeded\n");
  return 0;
}
#endif
