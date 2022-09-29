/* Copyright Abandoned 1996 TCX DataKonsult AB & Monty Program KB & Detron HB
   This file is public domain and comes with NO WARRANTY of any kind */

/*
** Functions to handle initializating and allocationg of all mysys & debug
** thread variables.
*/

#include "mysys_priv.h"
#include <m_string.h>

#ifdef THREAD
#ifdef USE_TLS
pthread_key(struct st_my_thread_var*, THR_KEY_mysys);
#else
pthread_key(struct st_my_thread_var, THR_KEY_mysys);
#endif
pthread_mutex_t THR_LOCK_malloc,THR_LOCK_open,THR_LOCK_keycache,
		THR_LOCK_lock,THR_LOCK_isam;
#ifndef HAVE_LOCALTIME_R
pthread_mutex_t LOCK_localtime_r;
#endif
#ifdef __WIN32__
pthread_mutex_t THR_LOCK_thread;
#endif

/* FIXME  Note.  TlsAlloc does not set an auto destructor, so
	the function my_thread_global_free must be called from
	somewhere before final exit of the library */

my_bool my_thread_global_init(void)
{
  if (pthread_key_create(&THR_KEY_mysys,free))
  {
    fprintf(stderr,"Can't initialize threads: error %d\n",errno);
    exit(1);
  }
  pthread_mutex_init(&THR_LOCK_malloc,NULL);
  pthread_mutex_init(&THR_LOCK_open,NULL);
  pthread_mutex_init(&THR_LOCK_keycache,NULL);
  pthread_mutex_init(&THR_LOCK_lock,NULL);
  pthread_mutex_init(&THR_LOCK_isam,NULL);
#ifdef __WIN32__
  pthread_mutex_init(&THR_LOCK_thread,NULL);
#endif
#ifndef HAVE_LOCALTIME_R
  pthread_mutex_init(&LOCK_localtime_r,NULL);
#endif
  return my_thread_init();
}

void my_thread_global_end(void)
{
#if defined(USE_TLS)
  (void) TlsFree(THR_KEY_mysys);
#endif
}

static long thread_id=0;

my_bool my_thread_init(void)
{
  struct st_my_thread_var *tmp;
  pthread_mutex_lock(&THR_LOCK_lock);
#if !defined(__WIN32__) || defined(USE_TLS)
  if (my_pthread_getspecific(struct st_my_thread_var *,THR_KEY_mysys))
  {
    pthread_mutex_unlock(&THR_LOCK_lock);
    return 0;						/* Safequard */
  }
    /* We must have many calloc() here because these are freed on
       pthread_exit */
  if (!(tmp=(struct st_my_thread_var *)
	calloc(1,sizeof(struct st_my_thread_var))))
  {
    pthread_mutex_unlock(&THR_LOCK_lock);
    return 1;
  }
  pthread_setspecific(THR_KEY_mysys,tmp);

#else
  if (THR_KEY_mysys.id)   /* Allready initialized */
  {
    pthread_mutex_unlock(&THR_LOCK_lock);
    return 0;
  }
  tmp= &THR_KEY_mysys;
#endif
  tmp->id= ++thread_id;
  pthread_mutex_init(&tmp->mutex,NULL);
  pthread_cond_init(&tmp->suspend, NULL);
  pthread_mutex_unlock(&THR_LOCK_lock);
  return 0;
}

void my_thread_end(void)
{
  struct st_my_thread_var *tmp=my_thread_var;
  if (tmp)
  {
#if !defined(DBUG_OFF)
    if (tmp->dbug)
    {
      free(tmp->dbug);
      tmp->dbug=0;
    }
#endif
#if !defined(__bsdi__) || defined(HAVE_mit_thread) /* bsdi dumps core here */
    pthread_cond_destroy(&tmp->suspend);
#endif
    pthread_mutex_destroy(&tmp->mutex);
#if !defined(__WIN32__) || defined(USE_TLS)
    free(tmp);
#endif
  }
#if !defined(__WIN32__) || defined(USE_TLS)
  pthread_setspecific(THR_KEY_mysys,0);
#endif
}

struct st_my_thread_var *_my_thread_var(void)
{
  return my_pthread_getspecific(struct st_my_thread_var*,THR_KEY_mysys);
}

/****************************************************************************
** Get name of current thread.
****************************************************************************/

#define UNKNOWN_THREAD -1

long my_thread_id()
{
#if defined(HAVE_PTHREAD_GETSEQUENCE_NP)
  return pthread_getsequence_np(pthread_self());
#elif defined(__sun) || defined(__sgi) || defined(__linux__)
  return pthread_self();
#else
  return my_thread_var->id;
#endif
}

#ifdef DBUG_OFF
char *my_thread_name(void)
{
  return "no_name";
}

#else

char *my_thread_name(void)
{
  char name_buff[100];
  struct st_my_thread_var *tmp=my_thread_var;
  if (!tmp->name[0])
  {
    long thread_id=my_thread_id();
    sprintf(name_buff,"T@%d", thread_id);
    strmake(tmp->name,name_buff,THREAD_NAME_SIZE);
  }
  return tmp->name;
}
#endif /* DBUG_OFF */

#endif /* THREAD */
