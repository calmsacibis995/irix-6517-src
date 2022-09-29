#include "common.h"
#include "sss_pthreads.h"
#include <assert.h>

static pthread_mutex_t   ruleevent_mutex;
static pthread_mutex_t   memory_mutex;
static pthread_mutex_t   string_mutex;
static pthread_rwlock_t  msglib_lock;
static pthread_mutex_t   sehconfig_mutex;
static pthread_mutex_t   dsmconfig_mutex;

void sss_pthreads_initq(list_of_elements_t *list)
{
  /*
   * Zero out everything.
   */
  memset(list,0,sizeof(list_of_elements_t));
  pthread_mutex_init(&list->mutex,NULL);
  pthread_cond_init(&list->busy,NULL);
}  

void sss_pthreads_init()
{
  pthread_mutex_init(&ruleevent_mutex,NULL);
  pthread_mutex_init(&memory_mutex,NULL);
  pthread_mutex_init(&string_mutex,NULL);
  pthread_mutex_init(&sehconfig_mutex,NULL);
  pthread_mutex_init(&dsmconfig_mutex,NULL);
}

void sss_pthreads_msglib_init()
{
  pthread_rwlock_init(&msglib_lock,NULL);
}

void sss_pthreads_msglib_exit()
{
  pthread_rwlock_destroy(&msglib_lock);
}

__uint64_t try_rdlock_msglib()
{
  if(pthread_rwlock_tryrdlock(&msglib_lock))
    {
      return (__uint64_t)-1;
    }
  return 0;
}

__uint64_t try_wrlock_msglib()
{
  if(pthread_rwlock_trywrlock(&msglib_lock))
    {
      return (__uint64_t)-1;
    }
  return 0;
}

void rdlock_msglib()
{
  pthread_rwlock_rdlock(&msglib_lock);
}

void wrlock_msglib()
{
  pthread_rwlock_wrlock(&msglib_lock);
}

void unlock_msglib()
{
  pthread_rwlock_unlock(&msglib_lock);
}

__uint64_t try_lock_ruleevent()
{
  if(pthread_mutex_trylock(&ruleevent_mutex))
    {
      return (__uint64_t)-1;
    }
  return 0;
}

void lock_ruleevent()
{
  pthread_mutex_lock(&ruleevent_mutex);
}

void unlock_ruleevent()
{
  pthread_mutex_unlock(&ruleevent_mutex);
}

void lock_memory()
{
  pthread_mutex_lock(&memory_mutex);
}

void unlock_memory()
{
  pthread_mutex_unlock(&memory_mutex);
}

void lock_strings()
{
  pthread_mutex_lock(&string_mutex);
}

void unlock_strings()
{
  pthread_mutex_unlock(&string_mutex);
}

void lock_sehconfig()
{
    pthread_mutex_lock(&sehconfig_mutex);
}

void unlock_sehconfig()
{
    pthread_mutex_unlock(&sehconfig_mutex);
}

void lock_dsmconfig()
{
    pthread_mutex_lock(&dsmconfig_mutex);
}

void unlock_dsmconfig()
{
    pthread_mutex_unlock(&dsmconfig_mutex);
}

/* 
 * Function to send messages from one thread to another. The message is
 * the second argument to this function. The first argument is the message
 * queue itself.
 */
int sss_pthreads_insert_element(list_of_elements_t *list,void *ev)
{
  event_node_t *Pnode=sem_mem_alloc_temp(sizeof(event_node_t));

  if(!Pnode)
    return -1;
  /* 
   * Wrap the message with data we need to locate it later.
   */
  Pnode->next=Pnode->before=NULL;
  Pnode->ev=ev;

  /* 
   * If too many messages in the queue. Just return failure.
   */
  if(list->count > SEH_DSM_MSG_HIWAT)
    {
      sem_mem_free(Pnode);
      return -1;
    }

  /* 
   * Enter Critical section.
   */
  pthread_mutex_lock(&list->mutex);

  /*
   * Increment count and add element to the head of the list.
   */
  list->count++;
  if(list->head)
    {
      Pnode->next=list->head;
      list->head->before=Pnode;	/* insert element. */
    }
  else
    list->tail=Pnode;		/* insert element. */
  list->head=Pnode;		/* insert element. */
  
  /* 
   * Signal the receiver of the message that there is something on
   * the queue. If the receiver is waiting for this signal fine, he will
   * get it. Otherwise he has to check the count we incremented above.
   */
  pthread_cond_signal(&list->busy);
  pthread_mutex_unlock(&list->mutex); /* exit critical section. */

  return 1;			/* has to be > 0 */
}

int sss_pthreads_get_count(list_of_elements_t *list)
{
  return(list->count);
}

int sss_pthreads_get_element(list_of_elements_t *list,void **PPvoid)
{
  event_node_t *Pnode;
  void *ev;

  /* 
   * Enter Critical section.
   */
  pthread_mutex_lock(&list->mutex);

  /* 
   * blocks here otherwise waiting for input 
   */
  while(!list->count)		
    pthread_cond_wait(&list->busy,&list->mutex);
  
  /*
   * At this point there is something on the message queue.
   * So list->tail cannot be NULL.
   */
  list->count--;
  Pnode=list->tail;		/* extricate the message. */
  ev=Pnode->ev;
  if(Pnode->before)
    {
      Pnode->before->next=NULL;
      list->tail=Pnode->before;
    }
  else
    {
      assert(!list->count);
      list->head=NULL;
      list->tail=NULL;
    }
  sem_mem_free(Pnode);
  
  pthread_mutex_unlock(&list->mutex); /* exit critical section. */

  *PPvoid=ev;
  
  return 1;			/* has to be > 0 */
}

/* 
 * wrapper around pthread_create.
 */
__uint64_t sss_pthreads_create_thread(pthread_t *thread, 
				      void *(*start)(void *))
{
  pthread_attr_t attr;
  __uint64_t t;

  pthread_attr_init(&attr);
  pthread_attr_setguardsize(&attr,1024);
  t=(__uint64_t)pthread_create(thread,&attr,start,NULL);
  pthread_attr_destroy(&attr);
  return t;
}

/* 
 * wrapper around pthread_join.
 */
__uint64_t sss_pthreads_wait_thread(pthread_t thread,void **retval)
{
  return (__uint64_t)pthread_join(thread,retval);
}


/* 
 * Free up pthread resources allocated to list. Callers responsiblity to
 * free data on the list by calling sss_pthreads_get_element repeatedly.
 */
void sss_pthreads_exitq(list_of_elements_t *list)
{
  pthread_mutex_destroy(&list->mutex);
  pthread_cond_destroy(&list->busy);
}

/*
 * Nothing pthread specific here. But since this function is necessary
 * to guarantee the critical section this is here. All it does is to block
 * the alarm signal.
 */
void sss_block_alarm()
{
  sigset_t sigmask;
  
  
  sigemptyset(&sigmask);
  sigaddset(&sigmask,SIGALRM);

  pthread_sigmask(SIG_BLOCK,&sigmask,NULL);
}

/*
 * Nothing pthread specific here. All it does is to unblock the alarm signal.
 */
void sss_unblock_alarm()
{
  sigset_t sigmask;
  
  sigemptyset(&sigmask);
  sigaddset(&sigmask,SIGALRM);

  pthread_sigmask(SIG_UNBLOCK,&sigmask,NULL);
}

/* 
 * Time to exit. Clean up all we can.. 
 * Calling exit here is debatable.
 */
void *sss_pthreads_exit()
{
  pthread_mutex_destroy(&ruleevent_mutex);
  pthread_mutex_destroy(&memory_mutex);
  pthread_mutex_destroy(&string_mutex);
  pthread_mutex_destroy(&sehconfig_mutex);
  pthread_mutex_destroy(&dsmconfig_mutex);

  exit(-1);
  return NULL;
}

