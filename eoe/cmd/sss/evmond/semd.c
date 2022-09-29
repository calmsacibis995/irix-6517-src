#include "common.h"
#include "events.h"
#include "events_private.h"
#include "sss_pthreads.h"
#include "ssdbapi.h"
#include "seh.h"
#include "seh_archive.h"

extern void *dsm_main(void *);
extern int seh_main(struct event_private *Pev,struct event_record *Pev_rec);
extern char *seh_archive(char *buffer,int len);
#if SEM_NO_ALARM
extern void *seh_alarm(void *arg);
#endif

__uint64_t upgrade_sgm(SGMLicense_t *lic);
void checkin_sgm_license(void);

int state=0;
int verbose=0;
int exit_flag=0;

SGMLicense_t  license = {RUN_AS_SEM, RUN_AS_SEM,-1, -1, 0, 0, 0}; /* SGM License */

static  pthread_t dsm_thread;
static  pthread_t another_thread;

void semdInit(void)
{
  struct event_private ev;
  struct rule *rules;
  int num_rules;


  INIT_STATE();

  sss_pthreads_init();
  
  if(seh_initialize(&ev)) 
      return;
  
  if(dsm_initialize(&ev))
      return;

  /*
   * Start the indivdual threads.
   */
  sss_pthreads_create_thread(&dsm_thread,dsm_main);

  return;
}

void semdDone(void)
{
  /* 
   * Terminate if the archive thread died for some reason.
   */
  lock_ruleevent();
  
  pthread_kill(dsm_thread,SIGINT);
  sss_pthreads_exit();

  free_mempool();
  return;
}

/*
 *  Notify semd that semd is on 
 */

void semdOn (void)
{
    lock_ruleevent  ();
    checkin_sgm_license(); 
    upgrade_sgm     (&license); 
    unlock_ruleevent();
}

int semdDeliverEvent(const char *_hostNameFrom,long eventTimeStamp,
		     int eventClass,int eventType,int eventPri,
		     int eventCounter,int eventDataLength,void *eventData)
{
    struct event_private ev;
    struct event_record ev_rec;
    struct event_runtime ev_run;
    int ret;
    
    if(eventDataLength && !eventData)
	return 0;

    if(eventClass == SSS_INTERNAL_CLASS && eventType == ERROR_EVENT_TYPE)
	return 0;

    bzero(&ev_run,0,sizeof(struct event_runtime));
    RUN(&ev)=&ev_run;
    RUN(&ev)->throttling_window=FALSE;

    seh_loop_init(&ev,&ev_rec); /* initialize everytime in loop. */
    
    ev.event_class           = eventClass;
    ev.event_type            = eventType;
    RUN(&ev)->priority       = eventPri;
    RUN(&ev)->time_stamp     = eventTimeStamp;
    ev.SEHDATA.number_events = eventCounter;
    if(!_hostNameFrom || strlen(_hostNameFrom) == 0)
	ev.SEHDATA.hostname=string_dup("localhost",strlen("localhost"));
    else
	ev.SEHDATA.hostname=string_dup((char *)_hostNameFrom,
				       strlen(_hostNameFrom));

    ev.SEHDATA.event_datalength=eventDataLength;
    ev.SEHDATA.Pevent_data=sem_mem_alloc_temp(eventDataLength+1);
    if(!ev.SEHDATA.Pevent_data)
    {
	seh_free_runtime_event(&ev);
	return 0;
    }
    memcpy(ev.SEHDATA.Pevent_data,eventData,eventDataLength);

    ret=seh_main(&ev,&ev_rec);
    
    if(ev.SEHDATA.hostname)
	string_free(ev.SEHDATA.hostname,SSSMAXHOSTNAMELEN);
    ev.SEHDATA.hostname=NULL;
    
    if(ev.SEHDATA.Pevent_data)
	sem_mem_free(ev.SEHDATA.Pevent_data);
    ev.SEHDATA.Pevent_data=NULL;

    return ret;
}

#if 0

FPssdbCreateErrorHandle  *fp_ssdbCreateErrorHandle  = ssdbCreateErrorHandle;
FPssdbDeleteErrorHandle  *fp_ssdbDeleteErrorHandle  = ssdbDeleteErrorHandle;
FPssdbNewClient          *fp_ssdbNewClient          = ssdbNewClient;
FPssdbDeleteClient       *fp_ssdbDeleteClient       = ssdbDeleteClient;
FPssdbOpenConnection     *fp_ssdbOpenConnection     = ssdbOpenConnection;
FPssdbCloseConnection    *fp_ssdbCloseConnection    = ssdbCloseConnection;
FPssdbSendRequest        *fp_ssdbSendRequest        = ssdbSendRequest;
FPssdbSendRequestTrans   *fp_ssdbSendRequestTrans   = ssdbSendRequestTrans;
FPssdbFreeRequest        *fp_ssdbFreeRequest        = ssdbFreeRequest;
FPssdbGetNumRecords      *fp_ssdbGetNumRecords      = ssdbGetNumRecords;
FPssdbGetNumColumns      *fp_ssdbGetNumColumns      = ssdbGetNumColumns;
FPssdbGetNextField       *fp_ssdbGetNextField       = ssdbGetNextField;
FPssdbGetLastErrorCode   *fp_ssdbGetLastErrorCode   = ssdbGetLastErrorCode;
FPssdbGetLastErrorString *fp_ssdbGetLastErrorString = ssdbGetLastErrorString;


void *seh_thread(void *arg)
{
    char *buffer;

    while (1)
    {
	semdDeliverEvent(NULL,time(NULL),47,1774,0,1,
			 strlen("localhost"),"localhost");
	semdDeliverEvent(NULL,time(NULL),47,1775,0,1,
			 strlen("XXXXXXXXXXXX"),"XXXXXXXXXXXX");
	buffer=semdAcceptString(ARCHIVE_STATUS,strlen(ARCHIVE_STATUS)+1);
	if(buffer)
	    fprintf(stderr,buffer);
	sleep(2);
    }
}


main()
{

    ssdbInit();
    semdInit();

    sss_pthreads_create_thread(&another_thread,seh_thread);
    sleep(120000);
    
    semdDone();
    ssdbDone();
}
#endif
