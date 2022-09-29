#ifndef __COMMON_H_
#define __COMMON_H_

#define TRUE                       1
#define FALSE                      0

#define SSS_PTHREADS               TRUE   /* Are we running as pthreads ?. */
#define PCP_TRACE                  FALSE  /* This flag is for debug purposes */
#define SSDB_CONNECT_ONCE          TRUE	  /* Connect just once to the SSDB. */
/*
 * no alarm handler if set to TRUE. The task of the alarm handler is then 
 * done by a separate pthread.
 */
#define SEM_NO_ALARM      TRUE

#include <unistd.h>        /* for getopt() */
#include <errno.h>         /* errno and perror */
#include <fcntl.h>         /* O_RDONLY */
#include <stdlib.h>        /* calloc(3) */
#include <assert.h>
#include <stdio.h>
#include <time.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/file.h>
#include <signal.h>
#include <pthread.h>
#include <syslog.h>
#include "sgmdefs.h"

/* 
 * Verbose level.
 */
#define VERB_INFO                  1
#define VERB_DEBUG_0               2
#define VERB_DEBUG_1               4
#define VERB_DEBUG_2               8

/* 
 * Now with stream sockets there is no real MAX_MESSAGE_LENGTH. This define
 * is just used to declare BUFFER_SIZE in different functions.
 */
#define MAX_MESSAGE_LENGTH         1024
#define SSS_WORKING_DIRECTORY      "/tmp/"
#define SSS_HOME_DIRECTORY         "/usr/etc/"
#define SSS_DEFAULT_USER           "guest"
#define LARGE_BUFFER_SIZE          1024

/*
 * MSGLIB stuff.
 *
 * Any one of those below should be set to TRUE. All the rest should be FALSE.
 */
#define POSIX_MSGLIB               FALSE
#define SYSV_MSGLIB                FALSE
#define UNIX_SOCK_MSGLIB           FALSE
#define STREAM_SOCK_MSGLIB         TRUE

#if POSIX_MSGLIB || UNIX_SOCK_MSGLIB || STREAM_SOCK_MSGLIB

#define SEH_TO_DSM_MSGQ            ".sehtodsm"
#define SEH_TO_SSDB_MSGQ           ".sehtossdb"
#define SEH_TO_API_MSGQ            ".sehtoapi"

#define SSDB_TO_SEH_MSGQ           ".ssdbtoseh"
#define API_TO_SEH_MSGQ            ".apitoseh"

#elif SYSV_MSGLIB

#define SEH_TO_DSM_MSGQ            999990
#define SEH_TO_SSDB_MSGQ           999909
#define SEH_TO_API_MSGQ            999099

#define SSDB_TO_SEH_MSGQ           990999
#define API_TO_SEH_MSGQ            909999

#endif

#if UNIX_SOCK_MSGLIB || STREAM_SOCK_MSGLIB
#define SELECT_BEFORE_RECEIVE        FALSE
#define DO_NOT_SELECT_BEFORE_RECEIVE TRUE
#define SEM_TIMEOUT_IO_Q             15 /* 
					 * timeout values for select on 
				         * input/ouput sockets.
				         */
#endif

/* 
 * Fields which define what comes in, in the event from the api.
 */
#define EVENT_MAGIC_NUMBER         0x0CABEEEE
#define EVENT_RESULT_MAGIC_NUMBER  0x4DEAFBEE
#define DSM_EVENT_MAGIC_NUMBER     0x0FEACEEE
#define EVENT_VERSION_1_0          1000
#define EVENT_VERSION_1_1          1100
#define SSSMAXHOSTNAMELEN          64
#define SEH_DSM_MSG_HIWAT          64  /* 
					* start dropping messages to the DSM 
					* after this high watermark is reached.
					*/
/*
 * Structures below have the first, second and third field be the same ...
 * i.e., message key, magic number and version number.
 *       the magic number is always a long.
 *       the first two fields are longs.
 */
typedef struct Event_s {
  unsigned long  LmsgKey;          /* message key to identify sender */
  unsigned long  LEventMagicNumber;/* magic number to verify sender */
  int   iVersion;		   /* version number of structure. */
  char  strHost[SSSMAXHOSTNAMELEN];/* remote hostname */
  int   iEventClass;		   /* specific event type to process */
  int   iEventType;		   /* event code specific to event type */
  unsigned int   iEventCount;	   /* number of events. */
  int   iPriority;		   /* priority value, applicable to eventmon */
  unsigned long  LTime;		   /* time the event occurred. */
  int   iEventDataLength;	   /* length of incoming event data */
  void  *ptrEventData;		   /* pointer to data (file/string/etc) */
} Event_t;

typedef struct EventResult_s {
  unsigned long  LmsgKey;          /* message key to indentify sender */
  unsigned long  LEventMagicNumber;/* the command type originally sent */
  int   iVersion;		   /* version number of structure. */
  int   iEventResultReturnValue;   /* return value from command */
  unsigned long iEventResultErrno; /* internal errno to event result */
  int   iEventClass;               /* specific event type to process */
  int   iEventType;                /* event code specific to event type */
  int   iEventResultDataLength;    /* length of event result data */
  void  *ptrEventResultData;       /* pointer to data (file/string/etc) */
} EventResult_t;

typedef struct DSMEvent_s {
    __uint64_t     LLsys_id;	     /* sys_id of the event. */
    unsigned long  LEvent_id;	     /* event_id of the ssdb entry */
				     /* created by the SEH. */
    unsigned long  LEventMagicNumber;/* magic number to verify sender */
    int   iVersion;                  /* version number of structure. */
    char  strHost[SSSMAXHOSTNAMELEN];/* remote hostname */
    int   iEventClass;		     /* specific event type to process */
    int   iEventType;                /* event code specific to event type */
    int   iEventCount;               /* number of events. */
    int   iPriority;		     /* priority value, applicable to eventmon*/
    int   iEventDataLength;          /* length of incoming event data */
    int   iTimeStamp;		     /* time stamp */
    void  *ptrEventData;             /* pointer to data (file/string/etc) */
} DSMEvent_t;

#ifdef SGM
/*
 * License Structure
 */
typedef struct SGMLicense_s {
  unsigned long   requested_mode;
  unsigned long   current_mode;
  time_t          license_expiry_date;
  time_t          last_license_check;
  unsigned long   status;
  unsigned long   max_subscriptions;
  unsigned long   curr_subscriptions;
  unsigned long   flag;
} SGMLicense_t;
#endif

/*
 * Configuration stuff.
 */

/*
 * Config types..
 */
#define UPDATE_CONFIG   1
#define NEW_CONFIG      2
#define DELETE_CONFIG   4

/* 
 * Config Object..
 */
#define EVENT_CONFIG            1
#define RULE_CONFIG             2
#define GLOBAL_CONFIG           4
/*
 * Another CONFIG object for SGM.  MODE_CHANGE signifies
 * that SEM/SGM mode is being changed.  This event is
 * logged in case of mode change from Console.
 */

#ifdef SGM
#define SGM_OPS                 8
#endif
#define ARCHIVE_START_CONFIG   16
#define ARCHIVE_END_CONFIG     32
#define ARCHIVE_STATUS_CONFIG  64


/*
 * Data structures for configuration.
 */
typedef struct Config_s {
  int   iConfigObject;		/* rule/event/.. ?.    */
  int   iConfigType;		/* update, delete, new */
} Config_t;

/*
 * Adding/deleting events.
 */
typedef struct EventConfig_s {
  Config_t hConfig;		/* header for each config data structure  */
  int   iEventClass;		/* specific event type to process */
  int   iEventType;		/* event code specific to event type */
  int   iIsenabled;		/* should we throttle this event ?. */
  int   ithrottle_counts;       /* throttle count. */
  int   ithrottle_times;        /* throttle time. */
  char  cHostName[276];         /* MAXHOSTNAMELEN + 20 */
} EventConfig_t;

/*
 * Adding/deleting rules.
 */
typedef struct RuleConfig_s {
  Config_t hConfig;		/* header for each config data structure  */
  int iobjId;			/* internal db id for rule */
} RuleConfig_t;

/*
 * Reading global values.
 */
typedef struct GlobalConfig_s {
  Config_t hConfig;
  int global_throttling;
  int global_logging;
  int global_action;
} GlobalConfig_t;

#ifdef SGM
/*
 * Mode_change/Subscribe/UnSubscribe Data Structures
 */
typedef struct SGMConfig_s {
  Config_t hConfig;
  __uint64_t configdata;
} SGMConfig_t;
#endif

typedef unsigned char BOOLEAN;

/*
 * Memory prototype definitions.
 */
__uint64_t sem_mem_alloc_init();
void *sem_mem_alloc_perm(int size);
void *sem_mem_alloc_temp(int size);
void *sem_mem_realloc_temp(void *ptr,int size);
void *sem_mem_realloc_perm(void *ptr,int size);
void sem_mem_free(void *ptr);

#endif /* _COMMON_H_ */
