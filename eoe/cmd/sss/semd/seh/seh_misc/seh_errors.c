#include "common.h"
#include "events.h"
#include "events_private.h"
#include "seh.h"
#include "seh_errors.h"
#include <syslog.h>
#include <stdarg.h>

#if EVENTS_DEBUG
unsigned int num_error_events_sent=0;
unsigned int num_syslog_entries=0;
#endif
extern struct event_private *Perror_event;

unsigned int pow2(__uint64_t num)
{
  unsigned int i=0;
  
  while(num)
    i++,num>>=1;
  
  return i;
}

unsigned long sixty4to32_error(__uint64_t err64)
{
  unsigned int maj=SEH_ERROR_MAJOR(err64);
  __uint64_t min=SEH_ERROR_MINOR(err64);
  unsigned long err32;

  err32  = (maj<<16);
  err32 |= (pow2(min)-1);
  
  return err32;
}

#if EVENTS_DEBUG
#define VERBOSE_ERROR_MESSAGES 1
#include "seh_error_msgs.h"

void error_parse(__uint64_t err)
{
  unsigned int maj=SEH_ERROR_MAJOR(err);
  __uint64_t min=SEH_ERROR_MINOR(err);
  unsigned int len=0;
  char **PPchar;
  
  for(len=0;seh_major_errors[len++];);
  
  if(maj >= len)
    {
      fprintf(stdout,"Unknown error:\n");
      return;
    }
  else
    {
      fprintf(stdout,"%s:",seh_major_errors[maj]);
    }
  if((((1<<16)-1) & min))
    {				/* Error is a common error. */
      PPchar=seh_common_minor_errors;
      min-=1;
    }
  else
    {
      PPchar=seh_major_to_minor_errors[maj];
      min=pow2(min)-16;		/* Error is not a common error. */
      min-=1;
    }
  if(PPchar)
    {
      for(len=0;PPchar[len++];);
      if(min >= len)
	{
	  fprintf(stdout,"Unknown minor error\n");
	}
      else
	fprintf(stdout,"%s\n",PPchar[min]);
    }
}
#endif

/* 
 * Function sends an error event to the DSM so the DSM can execute any 
 * default rules it has for the error event. This function should be called by 
 * the SEH when it encounters an error condition. 
 */
__uint64_t seh_send_error_event(struct event_private *Pev,int datalength,
				char *data)
{
  int len=Pev->SEHDATA.event_datalength;
  char *Pchar=Pev->SEHDATA.Pevent_data;
  __uint64_t err;

  /*
   * If new data, send it. Otherwise send the pre-existing data in 
   * the error event.
   */
  if(datalength && data)	
    {
      Pev->SEHDATA.event_datalength=datalength;
      Pev->SEHDATA.Pevent_data=data;
    }
  
  /* 
   * Send the event to the dsm.
   */
  err=seh_tell_event_dsm(Pev,NULL);

  /* 
   * Put the old data back into the error event.
   */
  Pev->SEHDATA.event_datalength=len;
  Pev->SEHDATA.Pevent_data=Pchar;
  
#if EVENTS_DEBUG
  num_error_events_sent++;
#endif

  return err;
}
  

/* 
 * Make an entry into the syslog file.
 */
__uint64_t seh_syslog_event(struct event_private *Pev,char *msg,...)
{
  char *buf=sem_mem_alloc_temp(MAX_MESSAGE_LENGTH);
  va_list args;
  int len=0;

  if(!buf)
    return SEH_ERROR(SEH_MAJ_EVENT_ERR,SEH_MIN_ALLOC_MEM);

  if(!msg)
    {
      sem_mem_free(buf);
      return 0;
    }

  /* 
   * Make the string to write into the syslog.
   */
  va_start(args,msg);
  len=vsnprintf(buf,MAX_MESSAGE_LENGTH,msg,args);
  va_end(args);

  openlog("esp",LOG_PID|LOG_CONS|LOG_NOWAIT,LOG_USER);

  if(Pev)
    {
      syslog(LOG_INFO,"%s ev_class=%d ev_type=%d : %m",
	     buf,Pev->event_class,Pev->event_type);
    }
  else
    syslog(LOG_INFO,"%s : %m",buf);

  closelog();
  
  if(Perror_event && Perror_event->event_class == SSS_INTERNAL_CLASS &&
     Perror_event->event_type == ERROR_EVENT_TYPE)
#if STREAM_SOCK_MSGLIB
    /* 
     * DSM free's the buffer if using stream sockets. This is because of the
     * optimization we do, not to waste cpu cycles doing a memcpy when stream
     * sockets are used.
     */
    if(seh_send_error_event(Perror_event,len+1,buf))
      {				
	/*
	 * Free only in case of an error. So buffer does not reach the 
	 * dsm at all. Then it has to be freed here.
	 */
	sem_mem_free(buf);	
      }
#else
    seh_send_error_event(Perror_event,len+1,buf);

  /* 
   * If not using stream sockets free the buffer here.
   */
  sem_mem_free(buf);
#endif

#if EVENTS_DEBUG
  num_syslog_entries++;
#endif

  return 0;
}


/* 
 * Initialize the sss error event. This is not user definable.
 */
__uint64_t seh_initialize_error_event(struct event_private *Pev, 
				      __uint64_t sys_id)
{
  int len=0;
  
  memset(Pev,0,sizeof(struct event_private));

  Pev->event_class=SSS_INTERNAL_CLASS; /* error event class. */
  Pev->event_type=ERROR_EVENT_TYPE;
  Pev->sys_id = sys_id;
  seh_init_each_event(Pev);

  /* 
   * Put in default error message.
   */
  Pev->SEHDATA.event_datalength=
    strlen("Unable to decide on which error occurred.")+1;
  Pev->SEHDATA.Pevent_data=
    (char *)sem_mem_alloc_temp(sizeof(char)*Pev->SEHDATA.event_datalength);
  strncpy(Pev->SEHDATA.Pevent_data,
	  "Unable to decide on which error occurred.",
	  Pev->SEHDATA.event_datalength);

  /* 
   * Put in the hostname as "localhost".
   */
  Pev->SEHDATA.hostname=sem_mem_alloc_temp(strlen("localhost")+1);
  strcpy(Pev->SEHDATA.hostname,string_dup("localhost",strlen("localhost")));

  Perror_event=Pev;
  return 0;
}

