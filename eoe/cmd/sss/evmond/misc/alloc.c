#include "common.h"
#include "alloc.h"
#include <signal.h>

#define MEMORY_DEBUG 1
#if MEMORY_DEBUG
int mem_debug=0;
#endif

/*
 * Function protects memory critical sections.
 * Should be called before executing any code which needs to be single threaded.
 */
void sem_mem_enter_critical(void)
{
  sss_block_alarm();
  lock_memory();
}
  
/*
 * Function should be called just before exiting critical sections.
 * i.e., any code which was single threaded using sem_mem_enter_critical.
 */
void sem_mem_exit_critical(void) 
{
  unlock_memory();
  sss_unblock_alarm();
}

/*
 * Any memory data structure initialization we need to do.
 */
__uint64_t sem_mem_alloc_init()
{
  __uint64_t err=0;

#if 0				
  /* 
   * This is not working correctly at all. Seems to hang the process
   * anyway we do it.
   */

  /*
   *
   * This sem_mem_enter_critical.. and sem_mem_exit_critical are necessary
   * because some thread may have entered sem_mem_enter_critical and we
   * do not want to fork when that has happened.
   * We should really do this using the pthread_atfork ... That does
   * exactly what we need.
   */
  pthread_atfork(sem_mem_enter_critical,sem_mem_exit_critical,
		 sem_mem_exit_critical);
#endif
#if KLIB_LIBRARY
  init_mempool(0,0,0);
#endif
  return err;
}

/*
 * Any memory which needs to be permanently allocated, i.e., never freed for
 * the lifetime of the program.
 * this is similar to calloc.
 */
void *sem_mem_alloc_perm(int size)
{
  void *ptr;
  
  sem_mem_enter_critical();
  ptr=MEM_ALLOC_PERM(size);
  sem_mem_exit_critical();

#if MEMORY_DEBUG
  if(mem_debug)
    fprintf(stderr,"Perm Size=%d, Ptr= %x\n",size,ptr);
#endif
  return ptr;
}

/*
 * Any memory which needs to be temporarily allocated, i.e., may be freed later
 * in the normal working of the program.
 * this is similar to calloc
 */
void *sem_mem_alloc_temp(int size)
{
  void *ptr;

  sem_mem_enter_critical();
  ptr=MEM_ALLOC_TEMP(size);
  sem_mem_exit_critical();

#if MEMORY_DEBUG
  if(mem_debug)
    fprintf(stderr,"Size=%d, Ptr= %x\n",size,ptr);
#endif
  return ptr;
}

/*
 * Any memory which needs to be temporarily allocated, i.e., may be freed later
 * in the normal working of the program.
 * this is similar to realloc.
 */
void *sem_mem_realloc_temp(void *ptr,int size)
{
#if MEMORY_DEBUG
  void *old_ptr=ptr;
#endif

  sem_mem_enter_critical();
  ptr=MEM_REALLOC_TEMP(ptr,size);
  sem_mem_exit_critical();

#if MEMORY_DEBUG
  if(mem_debug)
    fprintf(stderr,"Realloc Size=%d, Old= %x Ptr= %x\n",size,old_ptr,ptr);
#endif
  return ptr;
}

/*
 * Any memory which needs to be permanently allocated, i.e., never freed for the
 * lifetime of the program.
 * this is similar to realloc.
 */
void *sem_mem_realloc_perm(void *ptr,int size)
{
#if MEMORY_DEBUG
  void *old_ptr=ptr;
#endif

  sem_mem_enter_critical();
  ptr=MEM_REALLOC_PERM(ptr,size);
  sem_mem_exit_critical();

#if MEMORY_DEBUG
  if(mem_debug)
    fprintf(stderr,"Realloc Perm Size=%d, Old= %x Ptr= %x\n",size,old_ptr,ptr);
#endif
  return ptr;
}

/*
 * Analogous to free(). 
 */
void sem_mem_free(void *ptr)
{
  sem_mem_enter_critical();
  MEM_ALLOC_FREE(ptr);
  sem_mem_exit_critical();

#if MEMORY_DEBUG
  if(mem_debug)
    fprintf(stderr,"Ptr= %x\n",ptr);
#endif
}
  
