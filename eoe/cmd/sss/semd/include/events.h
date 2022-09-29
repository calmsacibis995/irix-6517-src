#ifndef _EVENTS_H_
#define _EVENTS_H_
#include "Dlist.h"

/* 
 * States of the SEM.
 */
#define INITIALIZATION_STATE  1	/* SEM is initializing. */
#define EVENT_HANDLING_STATE  2	/* Normal state */
#define SEH_ARCHIVING_STATE   3	/* Wait for SEH to signal archiving. */
#define SSDB_ARCHIVING_STATE  4	/* Wait for SSDB to finish archiving */
/* 
 * Things to set different states.
 */
#define SET_STATE(X)          (state=X)
#define INIT_STATE()          SET_STATE(INITIALIZATION_STATE)
#define EVENT_STATE()         SET_STATE(EVENT_HANDLING_STATE)
#define SEH_ARCHIVE_STATE()   SET_STATE(SEH_ARCHIVING_STATE)
#define SSDB_ARCHIVE_STATE()  SET_STATE(SSDB_ARCHIVING_STATE)

#include "common_events.h"



#define DELETEEVENT(HEAD,TAIL,ELEM)         DELETE_DLIST(HEAD,TAIL,ELEM)      
#define ADDEVENT(HEAD,TAIL,ELEM)            ADD_DLIST_AT_TAIL(HEAD,TAIL,ELEM) 
#define TRAVERSEEVENTS(HEAD,END,FUNC,PKEY)  TRAVERSE_DLIST(HEAD,END,FUNC,PKEY)
#define FINDEVENT(HEAD,END,FUNC,PKEY)       FIND_DLIST(HEAD,END,FUNC,PKEY)

#define TIME_TO_WAKEUP_TO_THROTTLE 120 /* should be in seconds */
/* 
 * Timeout to wait for archive to send the archive end state.
 */
#define ARCHIVE_TIMEOUT            (10)


/*
 * Defines for reconfiguration -- the new method which uses the alternate
 * archive port to send data.
 */
#define NEW_CONFIG_STRING         "NEW"
#define DELETE_CONFIG_STRING      "DELETE"
#define UPDATE_CONFIG_STRING      "UPDATE"
#define SUBSCRIBE_CONFIG_STRING   "SUBSCRIBE"
#define UNSUBSCRIBE_CONFIG_STRING "UNSUBSCRIBE"
#define MODE_CONFIG_STRING        "MODE"

#define SGM_LENGTH         3
#define RULE_LENGTH        4
#define EVENT_LENGTH       5
#define GLOBAL_LENGTH      6

/* 
 * SGM config
 */
#define SUBSCRIBE_LENGTH   9
#define UNSUBSCRIBE_LENGTH 11
#define MODE_LENGTH        4

/* 
 * Rule, Event config
 */
#define NEW_LENGTH         3
#define DELETE_LENGTH      6
#define UPDATE_LENGTH      6

/* 
 * Code creates a lock file LOCKFILENAME, puts its pid in it and 
 * tries to lock the file. This is necessary so external tasks know that
 * the sem daemon is running fine.
 */
#define LOCKFILENAME               "/tmp/.semlock.pid"
#define LOCK_FILE()                                           \
{                                                             \
  char pid_buf[100];                                          \
  int num;                                                    \
                                                              \
  num=snprintf(pid_buf,100,"%d",getpid());                    \
  lockfd=open(LOCKFILENAME,O_WRONLY|O_CREAT,0700);            \
  if(lockfd>0)                                                \
    {                                                         \
      if(flock(lockfd,LOCK_EX|LOCK_NB))                       \
        {                                                     \
	  fprintf(stderr,"Failed to lock file %s.\n",         \
		  LOCKFILENAME);                              \
          syslog(LOG_INFO,"%s : %m",                          \
		 "Failed to get lock on lock file "           \
		 ## LOCKFILENAME ## ".\n");                   \
	  exit(-1);                                           \
	}                                                     \
      write(lockfd,pid_buf,num);                              \
    }                                                         \
  else                                                        \
    {                                                         \
      fprintf(stderr,"Failed to open lock file %s.\n",        \
	      LOCKFILENAME);                                  \
      syslog(LOG_INFO,"%s : %m",                              \
		 "Failed to open lock file " ## LOCKFILENAME  \
                 ## ".\n");                                   \
      exit(-1);                                               \
    }                                                         \
}

/*
 * Code tries to unlock and unlink the lock file LOCKFILENAME.
 */
#define UNLOCK_FILE()                                         \
{                                                             \
  flock(lockfd,LOCK_UN|LOCK_NB);                              \
  unlink(LOCKFILENAME);                                       \
}

#define CLEAR_ERRNO()                                         \
{                                                             \
  extern int errno;                                           \
  errno=0;                                                    \
}
#define CONFIGURATION_BY_EVENT 0
#endif /* _EVENTS_H_ */
