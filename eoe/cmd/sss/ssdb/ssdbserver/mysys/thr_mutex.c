/* Copyright Abandoned 1996 TCX DataKonsult AB & Monty Program KB & Detron HB
   This file is public domain and comes with NO WARRANTY of any kind */

/* This makes a wrapper for mutex handling to make it easier to debug mutex */

#include <global.h>
#include <m_string.h>
#if defined(THREAD) && defined(SAFE_MUTEX)
#undef SAFE_MUTEX		/* Avoid safe_mutex redefinitions */
#include <my_pthread.h>


/* Remove wrappers */
#undef pthread_mutex_init
#undef pthread_mutex_lock
#undef pthread_mutex_unlock
#undef pthread_mutex_destroy
#undef pthread_cond_wait
#undef pthread_cond_timedwait
#ifdef HAVE_NONPOSIX_PTHREAD_MUTEX_INIT
#define pthread_mutex_init(a,b) my_pthread_mutex_init((a),(b))
#endif

int safe_mutex_init(safe_mutex_t *mp, const pthread_mutexattr_t *attr)
{
  bzero((char*) mp,sizeof(*mp));
  pthread_mutex_init(&mp->global,NULL);
  pthread_mutex_init(&mp->mutex,attr);
  return 0;
}

int safe_mutex_lock(safe_mutex_t *mp,const char *file, uint line)
{
  int error;
  pthread_mutex_lock(&mp->global);
  if (mp->count > 0 && pthread_equal(pthread_self(),mp->thread))
  {
    fprintf(stderr,"safe_mutex: Trying to lock mutex at %s, line %d, when the mutex was already locked at %s, line %d\n",
	    file,line,mp->file,mp->line);
    abort();
  }
  pthread_mutex_unlock(&mp->global);
  error=pthread_mutex_lock(&mp->mutex);
  pthread_mutex_lock(&mp->global);
  if (mp->count++)
  {
    fprintf(stderr,"safe_mutex: Error in thread libray: Got mutex at %s, line %d more than 1 time\n", file,line);
    abort();
  }
  mp->thread=pthread_self();
  mp->file= (char*) file;
  mp->line=line;
  pthread_mutex_unlock(&mp->global);
  return error;
}


int safe_mutex_unlock(safe_mutex_t *mp,const char *file, uint line)
{
  int error;
  pthread_mutex_lock(&mp->global);
  if (mp->count == 0)
  {
    fprintf(stderr,"safe_mutex: Trying to unlock mutex that wasn't locked at %s, line %d\n            Last used at %s, line: %d",
	    file,line,mp->file ? mp->file : "",mp->line);
    abort();
  }
  if (!pthread_equal(pthread_self(),mp->thread))
  {
    fprintf(stderr,"safe_mutex: Trying to unlock mutex at %s, line %d  that was locked by another thread at: %s, line: %d\n",
	    file,line,mp->file,mp->line);
    abort();
  }
  mp->count--;
  error=pthread_mutex_unlock(&mp->mutex);
  pthread_mutex_unlock(&mp->global);
  return error;
}


int safe_cond_wait(pthread_cond_t *cond, safe_mutex_t *mp, const char *file,
		   uint line)
{
  int error;
  pthread_mutex_lock(&mp->global);
  if (mp->count == 0)
  {
    fprintf(stderr,"safe_mutex: Trying to cond_wait on a unlocked mutex at %s, line %d\n",file,line);
    abort();
  }
  if (!pthread_equal(pthread_self(),mp->thread))
  {
    fprintf(stderr,"safe_mutex: Trying to cond_wait on a mutex at %s, line %d  that was locked by another thread at: %s, line: %d\n",
	    file,line,mp->file,mp->line);
    abort();
  }

  if (mp->count != 1)
  {
    abort();
  }
  mp->count--;					/* Mutex will be released */
  pthread_mutex_unlock(&mp->global);
  error=pthread_cond_wait(cond,&mp->mutex);
  pthread_mutex_lock(&mp->global);
  mp->count++;
  mp->thread=pthread_self();
  mp->file= (char*) file;
  mp->line=line;
  pthread_mutex_unlock(&mp->global);
  return error;
}


int safe_cond_timedwait(pthread_cond_t *cond, safe_mutex_t *mp,
			struct timespec *abstime,
			const char *file, uint line)
{
  int error;
  pthread_mutex_lock(&mp->global);
  if (mp->count != 1 || !pthread_equal(pthread_self(),mp->thread))
  {
    fprintf(stderr,"safe_mutex: Trying to cond_wait at %s, line %d on a not hold mutex\n",file,line);
    abort();
  }
  mp->count--;					/* Mutex will be released */
  pthread_mutex_unlock(&mp->global);
  error=pthread_cond_timedwait(cond,&mp->mutex,abstime);
  pthread_mutex_lock(&mp->global);
  mp->count++;
  mp->thread=pthread_self();
  mp->file= (char*) file;
  mp->line=line;
  pthread_mutex_unlock(&mp->global);
  return error;
}

int safe_mutex_destroy(safe_mutex_t *mp)
{
  int error=0;
  if (mp->count != 0)
  {
    fprintf(stderr,"safe_mutex: Trying to destroy a mutex that was locked at %s, line %d\n",
	    mp->file,mp->line);
    abort();
  }
  pthread_mutex_destroy(&mp->global);
  return pthread_mutex_destroy(&mp->mutex);
}

#endif /* SAFE_MUTEX */
