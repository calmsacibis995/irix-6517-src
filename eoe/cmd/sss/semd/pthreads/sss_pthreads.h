#ifndef _SSS_PTHREADS_H_
#define _SSS_PTHREADS_H_

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <sys/types.h>
#include <limits.h>
#include <unistd.h>
#include <pwd.h>


/*
 * Pthread stuff. These are the data structures the two threads SEH and
 * DSM use to communicate with each other.
 */
typedef struct event_node_s {
  struct event_node_s  *next;
  struct event_node_s  *before;
  void *ev;
} event_node_t;

typedef struct list_of_elements_s {
  int count;                    /* count of number of events on the list */
  pthread_cond_t  busy;         /* busy condition variable. */
  pthread_mutex_t mutex;        /* mutex */
  event_node_t    *head;        /* head of list of events. */
  event_node_t    *tail;	/* tail of the list. */
} list_of_elements_t;


extern int sss_pthreads_insert_element(list_of_elements_t *list,void *ev);
extern int sss_pthreads_get_element(list_of_elements_t *list,void **PPvoid);
extern __uint64_t sss_pthreads_create_thread(pthread_t *thread,
					     void *(*start)(void *));
extern __uint64_t sss_pthreads_wait_thread(pthread_t thread,void **retval);
extern void *sss_pthreads_exit();
extern void lock_ruleevent();
extern __uint64_t try_lock_rules();
extern void unlock_rules();

#endif /* _SSS_PTHREADS_H_ */
