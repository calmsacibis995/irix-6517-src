/* ----------------------------------------------------------------- */
/*                          SSDBTHREAD.H                             */
/*                   SSDB Thread Support header file                 */
/* ----------------------------------------------------------------- */
#ifndef H_SSDBTHREAD_H
#define H_SSDBTHREAD_H

#ifdef OS_UNIX
#ifdef PTHEAD_THREAD          /* POSIX 1.c support */
#include <pthread.h>
typedef pthread_mutex_t CRITICAL_SECTION;
#else                         /* SUNTHREAD_THREAD - Sun/Solaris 2.x */
#include <thread.h>
typedef mutex_t CRITICAL_SECTION;
#endif /* #ifdef PTHEAD_THREAD */
#endif /* #ifdef OS_UNIX */

#ifdef OS_WINDOWS
#include <windows.h>
#endif

/* Critical Section (Light mutex, valid for one process) */
int ssdbthread_init_critical_section(CRITICAL_SECTION *criticalsection);
int ssdbthread_enter_critical_section(CRITICAL_SECTION *criticalsection);
int ssdbthread_leave_critical_section(CRITICAL_SECTION *criticalsection);
int ssdbthread_done_critical_section(CRITICAL_SECTION *criticalsection);

/* Mutex */
/* ....... */

/* Semaphore */
/* ....... */

/* Thread */
/* ....... */


#endif /* #ifndef H_SSDBTHREAD_H */
