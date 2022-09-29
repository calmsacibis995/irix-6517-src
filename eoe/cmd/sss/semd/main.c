#include "common.h"
#include "events.h"
#include "events_private.h"
#include "sss_pthreads.h"

extern void *dsm_main(void *);
extern void *seh_main(void *);
extern void *seh_archive(void *);
#if SEM_NO_ALARM
extern void *seh_alarm(void *arg);
#endif

int state=0;
int verbose=0;
int exit_flag=0;
int lockfd=-1;			/* file descriptor for lock file. */

SGMLicense_t  license = {RUN_AS_SEM, RUN_AS_SEM,-1, -1, 0, 0, 0}; /* SGM License */

main(int argc,char **argv)
{
  struct event_private ev;
  struct event_private *event_head,*event_tail;
  struct rule *rules;
  int num_rules;
  pthread_t seh_thread;
  pthread_t dsm_thread;
  pthread_t archive_thread;
#if SEM_NO_ALARM
  pthread_t alarm_thread;
#endif
  extern void seh_clean_everything();

  if(getuid() || geteuid())
  {
      fprintf(stderr,"Access denied\n");
      exit(-1);
  }
  
  seh_check_arguments(argc,argv);  
  dsm_check_arguments(argc,argv);

  INIT_STATE();

  if(seh_initialize(&event_head,&event_tail,&ev)) 
    exit(-1);                                 
  
  if(dsm_initialize(&event_head,&event_tail,&rules,&ev))
    exit(-1);

  /*
   * Start the indivdual threads.
   */
#if 0
  sss_pthreads_create_thread(&alarm_thread,seh_alarm);
#endif
  sss_pthreads_create_thread(&seh_thread,seh_main);
  sss_pthreads_create_thread(&dsm_thread,dsm_main);
  sss_pthreads_create_thread(&archive_thread,seh_archive);

  sss_pthreads_wait_thread(archive_thread,NULL);

  /*
   * Wait for the threads.. We should never come here when everything is
   * ok.
   */
  /* 
   * Terminate if the archive thread died for some reason.
   */
  lock_ruleevent();

  pthread_kill(dsm_thread,SIGINT);
  pthread_kill(seh_thread,SIGINT);

  sss_pthreads_wait_thread(dsm_thread,NULL);
  sss_pthreads_wait_thread(seh_thread,NULL);

#if 0
  pthread_kill(alarm_thread,SIGINT);
  sss_pthreads_wait_thread(alarm_thread,NULL);
#endif


  exit(0);
}
