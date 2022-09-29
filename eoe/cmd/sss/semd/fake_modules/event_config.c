#include "common.h"
#include "semapi.h"
#include "events.h"

/* 
 * Some global variables.
 */

/* 
 * Signal handler for SIGINT, SIGQUIT...
 */
int verbose=0;
void signal_handler(int i)
{
  semapiDone();
  exit(-1);
}

/*
 * Create the event structure to send an event reconfiguration event for the
 * event passed in as arguments.
 *
 * Reconfiguring events.
 */
char *create_event_config_msg(int class, int type,int isthrottle, char *hname,
			int th_count,int th_time,int configtype,unsigned int *length)
{
  static char sendmsg[MAX_MESSAGE_LENGTH];
  EventConfig_t *Pevconfig;
  
  Pevconfig=(EventConfig_t *)sendmsg;
  /* 
   * Now comes the actual event configuration data.
   */
  Pevconfig->hConfig.iConfigObject=EVENT_CONFIG;
  Pevconfig->hConfig.iConfigType=configtype;
  strcpy(Pevconfig->cHostName, hname);
  Pevconfig->iEventClass=class;
  Pevconfig->iEventType=type;
  Pevconfig->iIsthrottle=isthrottle;
  Pevconfig->ithrottle_counts=th_count;
  Pevconfig->ithrottle_times=th_time;

  *length = sizeof(EventConfig_t);
  return sendmsg;
}

/*
 * Create the event structure to send a MODE_CHANGE reconfiguration event
 * to upgrade/degrade SEM/SGM
 */

char *create_mode_config_msg(char *configtype, unsigned int *length)
{
  static char sendmsg[MAX_MESSAGE_LENGTH];
  SGMConfig_t *Pevconfig;
  char        *confObject;
  char        *confData;
  __uint64_t  data;

  Pevconfig = (SGMConfig_t *) sendmsg;

  Pevconfig->hConfig.iConfigObject = SGM_OPS;
  confObject = strtok(configtype, ":");
  confData   = strtok(NULL, ":");
  if ( confObject ) {
      if ( strcasecmp(confObject, "SUBSCRIBE") == 0 ) {
	  Pevconfig->hConfig.iConfigType = SUBSCRIBE;
      } else if ( strcasecmp(confObject, "UNSUBSCRIBE") == 0 ) {
	  Pevconfig->hConfig.iConfigType = UNSUBSCRIBE;
      } else if ( strcasecmp(confObject, "MODE_CHANGE") == 0 ) {
	  Pevconfig->hConfig.iConfigType = MODE_CHANGE;
      } else {
	  return NULL;
      }

      if ( confData ) {
	  data = strtoull(confData, NULL, 16);
	  if ( data ) Pevconfig->configdata = data;
	  else return NULL;
      }

  } else {
      return NULL;
  }

  *length = sizeof(SGMConfig_t);
  return sendmsg;

}

/*
 * Create the event structure to send a rule reconfiguration event for the
 * rule with objid passed in as arguments.
 *
 * Reconfiguring rules.
 */
char *create_rule_config_msg(int objid,int configtype,unsigned int *length)
{
  static char sendmsg[MAX_MESSAGE_LENGTH];
  RuleConfig_t *Pruleconfig;
  
  *length=sizeof(RuleConfig_t);
  Pruleconfig=(RuleConfig_t *)sendmsg;
  /* 
   * Now comes the actual rule configuration data.
   */
  Pruleconfig->hConfig.iConfigObject=RULE_CONFIG;
  Pruleconfig->hConfig.iConfigType=configtype;
  Pruleconfig->iobjId=objid;

  return sendmsg;
}

main(int argc, char **argv)
{
  int c=0;
  int ev_class=0,ev_type=0;
  struct sigaction sigact;
  int th_count=0,th_time=0;
  int isthrottle=0;
  int configtype=UPDATE_CONFIG;
  int event_config=1, rule_config=0, mode_config=0;
  int objid=0;
  unsigned send=0, length=0, count=0;
  char *sendmsg=NULL;
  char *hname = NULL;
  char *configdata = NULL;

  /* 
   * Options check.
   */
  while ( -1 != (c = getopt(argc,argv,"C:T:c:t:iNDRO:H:S:")))
    {
      switch (c)
	{
	case 'S':
	  mode_config=1;
	  event_config=0;
	  configdata = strdup(optarg);   /* 1 to degrade SGM to SEM etc.*/
	  break;
	case 'H':
	  hname = strdup(optarg);
	  break;
	case 'N':
	  configtype=NEW_CONFIG;
	  break;
	case 'D':
	  configtype=DELETE_CONFIG;
	  break;
	case 'c':
	  th_count = atoi(optarg);
	  break;
	case 't':
	  th_time  = atoi(optarg);
	  break;
	case 'i':
	  isthrottle = 1;
	  break;
	case 'C':
	  ev_class = atoi(optarg);
	  break;
	case 'T':
	  ev_type = atoi(optarg);
	  break;
	case 'R':
	  event_config=0;
	  rule_config=1;
	  break;
	case 'O':
	  objid = atoi(optarg);
	  break;
	default: /* unknown or missing argument */
	  return -1;
	} /* switch */
    } /* while */

  init_mempool();
  /*
   * Generate the message buffer.
   */
  if(event_config)
    sendmsg=create_event_config_msg(ev_class,ev_type,isthrottle,hname,th_count,
				    th_time,configtype,&length);
  else if (rule_config)
    sendmsg=create_rule_config_msg(objid,configtype,&length);
  else if (mode_config)
    sendmsg=create_mode_config_msg(configdata, &length);
  else
    {
      fprintf(stderr,"Error: Exiting ...\n");
      exit(-1);
    }

  if(!sendmsg || length == 0)
    {
      fprintf(stderr,"Unable to create send buffer.\n");
      exit(-1);
    }
  /* 
   * Create  the communication streams.
   */
  if(semapiInit(SEMAPI_VERMAJOR,SEMAPI_VERMINOR) != SEMERR_SUCCESS)
    {
      printf("Can't initialize SEM API\n");
      exit(1);
    }
  
  /* 
   * Send the message.
   */
#if PCP_TRACE
  pmtracebegin("configevent");
#endif
  send = semapiDeliverEvent("localhost",time(NULL),SSS_INTERNAL_CLASS,
			    CONFIG_EVENT_TYPE,0,
			    1,length,sendmsg);
  if(send == SEMERR_SUCCESS)
    printf("Success      :%d\n",length);
  else if(send == SEMERR_FATALERROR) 
    printf("Fatal Error  :%d\n",length);
  else if(send == SEMERR_ERROR)      
    printf("Error        :%d\n",length);
  
#if PCP_TRACE
  pmtraceend("configevent");
#endif
  semapiDone();
}

