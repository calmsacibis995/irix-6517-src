#include "common.h"
#include "common_ssdb.h"
#include "events.h"
#include "events_private.h"
#include "dsm_events.h"
#include "dsm_events_private.h"
#include "dsm_rules.h"
#include "dsm.h"
#include "dsm_errors.h"
#include "ssdberr.h"
#include "ssdbapi.h"
#include "sss_sysid.h"

extern FPssdbCreateErrorHandle  *fp_ssdbCreateErrorHandle;
extern FPssdbDeleteErrorHandle  *fp_ssdbDeleteErrorHandle;
extern FPssdbNewClient          *fp_ssdbNewClient;
extern FPssdbDeleteClient       *fp_ssdbDeleteClient;
extern FPssdbIsClientValid      *fp_ssdbIsClientValid;
extern FPssdbIsErrorHandleValid *fp_ssdbIsErrorHandleValid;
extern FPssdbOpenConnection     *fp_ssdbOpenConnection;
extern FPssdbCloseConnection    *fp_ssdbCloseConnection;
extern FPssdbIsConnectionValid  *fp_ssdbIsConnectionValid;
extern FPssdbSendRequest        *fp_ssdbSendRequest;
extern FPssdbSendRequestTrans   *fp_ssdbSendRequestTrans;
extern FPssdbFreeRequest        *fp_ssdbFreeRequest;
extern FPssdbIsRequestValid     *fp_ssdbIsRequestValid;
extern FPssdbGetNumRecords      *fp_ssdbGetNumRecords;
extern FPssdbGetNumColumns      *fp_ssdbGetNumColumns;
extern FPssdbGetNextField       *fp_ssdbGetNextField;
extern FPssdbLockTable          *fp_ssdbLockTable;
extern FPssdbUnLockTable        *fp_ssdbUnLockTable;
extern FPssdbGetRow             *fp_ssdbGetRow;
extern FPssdbGetFieldMaxSize    *fp_ssdbGetFieldMaxSize;
extern FPssdbRequestSeek        *fp_ssdbRequestSeek;
extern FPssdbGetLastErrorCode   *fp_ssdbGetLastErrorCode;
extern FPssdbGetLastErrorString *fp_ssdbGetLastErrorString;

extern void dsm_free_event_rule(struct event_private *Pev,int *Pobjid);

extern int verbose;
/* 
 * This also comes from the seh thread. Client handle for ssdb api.
 */
extern ssdb_Client_Handle     dbClient;
extern struct event_private *Pevent_head;

/* 
 * Error handle for ssdb api used by the dsm thread.
 */
static ssdb_Error_Handle      dsmdbError;
#if SSDB_CONNECT_ONCE
/* 
 * Connection handle for DSM.
 */
static ssdb_Connection_Handle DSMdbConnect;
#endif

#if EVENTS_DEBUG
/* 
 * Stats/Metrics. Returned when sem is called with the -S option.
 */
unsigned int num_actions_taken_records=0;
unsigned int num_errors_in_dsm_ssdb=0;
unsigned int num_init_events_dsm_ssdb=0;
unsigned int num_init_rules_dsm_ssdb=0;
unsigned int num_init_globals_dsm_ssdb=0;
char *last_proc_executed=NULL;
unsigned long time_last_proc_executed;

/* 
 * Wrapper around ssdb api's error functions.
 */
static void dsm_last_error_ssdb()
{
  fprintf(stderr,"SSDB error code=%d, SSDB error string = %s\n",
	  fp_ssdbGetLastErrorCode(dsmdbError),
	  fp_ssdbGetLastErrorString(dsmdbError));
}
#endif

/*
 * Function to setup or do any initialization needed to talk to the SSDB.
 *   o Get client and error handles.
 *   o Delete run time tables.
 */
__uint64_t dsm_ssdb_setup_interfaces()
{
  __uint64_t err=DSM_ERROR(DSM_MAJ_INIT_ERR,DSM_MIN_INIT_DB);
  ssdb_Request_Handle dbReq;
 
  dsmdbError=fp_ssdbCreateErrorHandle();
  if(!dsmdbError)
    return err;
#if SSDB_CONNECT_ONCE
  DSMdbConnect=
    fp_ssdbOpenConnection(dsmdbError,dbClient,NULL,SSS_SSDB_DBNAME,
		       SSDB_CONFLG_RECONNECT|SSDB_CONFLG_REPTCONNECT);
  if(!DSMdbConnect)
    {
#if EVENTS_DEBUG
      if(verbose > VERB_INFO)
	{
	  fprintf(stderr,"SSDB(DSM:10) Open Connection Error:");
	  dsm_last_error_ssdb();
	}
      num_errors_in_dsm_ssdb++;
#endif
      return err=DSM_ERROR(DSM_MAJ_INIT_ERR,DSM_MIN_INIT_DB);
    }
#endif

  return 0;
}

/*
 * Break any event_rule link if the rule is no longer applicable to an event.
 */
__uint64_t dsm_check_delete_event_rules_ssdb(int objid)
{
  struct event_private *Pev=Pevent_head;
  struct event_rules *Pevr=NULL;
  ssdb_Request_Handle dbReq;
  __uint64_t err=0;
  struct rule *Prule;
  __uint64_t arysysid[] = {WILDCARD_SYSID, WILDCARD_SYSID};
  int aryClass[]={WILDCARD_EVENT,WILDCARD_EVENT,WILDCARD_EVENT};
  int aryType[]={WILDCARD_EVENT,WILDCARD_EVENT,WILDCARD_EVENT};
  int i,j,Itemp;
  int num_rules=0;
  BOOLEAN bRuleFound=FALSE;

  if(!objid)
    return DSM_ERROR(DSM_MAJ_EVENT_ERR,DSM_MIN_INVALID_ARG);

#if !SSDB_CONNECT_ONCE
  DSMdbConnect=
    fp_ssdbOpenConnection(dsmdbError,dbClient,NULL,SSS_SSDB_DBNAME,
		       SSDB_CONFLG_RECONNECT|SSDB_CONFLG_REPTCONNECT);
  if(!DSMdbConnect)
    {
#if EVENTS_DEBUG
      if(verbose > VERB_INFO)
	{
	  fprintf(stderr,"SSDB(DSM:2) Open Connection Error:");
	  dsm_last_error_ssdb();
	}
      num_errors_in_dsm_ssdb++;
#endif
      return err=DSM_ERROR(DSM_MAJ_INIT_ERR,DSM_MIN_INIT_DB);
    }
#endif
    

  while(Pev)
    {
      /* 
       * Don't worry about configuration events.
       */
      if(Pev->event_class == SSS_INTERNAL_CLASS)
      {
	  Pev=Pev->Pnext;
	  continue;
      }

      /* 
       * This is the order to look for the class and the type. First do the
       * specific lookup in the ssdb, then start looking for the wildcard type.
       * The search method gives equal weight to the wildcard and specific
       * event classes and types. However priority is given to the specific
       * classes and types.
       */
      arysysid[0] = Pev->sys_id;
      aryClass[0]=aryClass[1]=Pev->event_class;
      aryType[0]=Pev->event_type;
      
      /* 
       * Mark as rule not found for this event.
       */
      bRuleFound=FALSE;
      
      for(j=0,num_rules=0; 
	  j < (sizeof(arysysid)/sizeof(__uint64_t)) && !num_rules; 
	  j++ ) 
      {
        for(i=0;(i<sizeof(aryClass)/sizeof(int));i++)
	{
	  /*
	   * Read all the rows in the "event_actionref" table with the 
	   * specified class and type id. Remember to also look for wildcard 
	   * event classes and types if the specific class and type id is not 
	   * found.
	   *  
	   */
	  /* 
	   * Send sql request.
	   */
	  dbReq=fp_ssdbSendRequestTrans(dsmdbError,DSMdbConnect,
				     SSDB_REQTYPE_SELECT,
				     SSDB_EVENT_RULE_TABLE,
				     "SELECT action_id from %s WHERE "
				     "class_id=%d and type_id=%d and "
				     "sys_id = '%llX'",
				     SSDB_EVENT_RULE_TABLE,aryClass[i],
				     aryType[i], arysysid[j]);

	  if(!dbReq)
	    {				
	      /* 
	       * sql request did not work. 
	       */
#if EVENTS_DEBUG
	      if(verbose > VERB_INFO)
		{
		  fprintf(stderr,"SSDB(DSM:1) fp_ssdbSendRequestTrans Error:");
		  dsm_last_error_ssdb();
		}
	      num_errors_in_dsm_ssdb++;
#endif
#if !SSDB_CONNECT_ONCE
	      fp_ssdbCloseConnection(dsmdbError,DSMdbConnect);
#endif
	      return DSM_ERROR(DSM_MAJ_SSDB_ERR,DSM_MIN_DB_NORULE);
	    }
	  /*
	   * Iterate as long as we get the object id.
	   */
	  while(fp_ssdbGetNextField(dsmdbError,dbReq,&Itemp,sizeof(int)))
	    {
	      /* 
	       * Found the rule. So mark it.
	       */
	      if(Itemp == objid)
		bRuleFound = TRUE;
#if EVENTS_DEBUG
	      if(verbose > VERB_DEBUG_2)
		printf("Event: Class=%d, Type=%d objid=%d \n",Pev->event_class,
		       Pev->event_type,Itemp);
#endif
	      num_rules++;
	    }
	  fp_ssdbFreeRequest(dsmdbError,dbReq);
	}
      }
    
      if(bRuleFound == FALSE)
	dsm_free_event_rule(Pev,&objid);
      Pev=Pev->Pnext;
    }
#if !SSDB_CONNECT_ONCE
  fp_ssdbCloseConnection(dsmdbError,DSMdbConnect);
#endif
  
  return 0;
}
/*
 * Initialize event rules link for the event passed as an argument to this 
 * function.
 * Read in any rules necessary.
 */
__uint64_t dsm_init_event_ssdb(struct event_private *Pev)
  {
#if !SSDB_CONNECT_ONCE
    ssdb_Connection_Handle DSMdbConnect;
#endif
    ssdb_Request_Handle dbReq;
    __uint64_t err=0;
    struct rule *Prule;
    int objid;
    __uint64_t arysysid[]={WILDCARD_SYSID, WILDCARD_SYSID};
    int aryClass[]={WILDCARD_EVENT,WILDCARD_EVENT,WILDCARD_EVENT};
    int aryType[]={WILDCARD_EVENT,WILDCARD_EVENT,WILDCARD_EVENT};
    int i, j;
    int num_rules=0;

    if(!Pev)
      return DSM_ERROR(DSM_MAJ_EVENT_ERR,DSM_MIN_INVALID_ARG);

    /* 
     * This is the order to look for the class and the type. First do the
     * specific lookup in the ssdb, then start looking for the wildcard type.
     * The search method gives equal weight to the wildcard and specific
     * event classes and types. However priority is given to the specific
     * classes and types.
     */
    arysysid[0]= Pev->sys_id;
    aryClass[0]=aryClass[1]=Pev->event_class;
    aryType[0]=Pev->event_type;

      
#if !SSDB_CONNECT_ONCE
    DSMdbConnect=
      fp_ssdbOpenConnection(dsmdbError,dbClient,NULL,SSS_SSDB_DBNAME,
			 SSDB_CONFLG_RECONNECT|SSDB_CONFLG_REPTCONNECT);
    if(!DSMdbConnect)
    {
#if EVENTS_DEBUG
      if(verbose > VERB_INFO)
	{
	  fprintf(stderr,"SSDB(DSM:2) Open Connection Error:");
	  dsm_last_error_ssdb();
	}
      num_errors_in_dsm_ssdb++;
#endif
      return err=DSM_ERROR(DSM_MAJ_INIT_ERR,DSM_MIN_INIT_DB);
    }
#endif
    

    for(j=0,num_rules=0; 
	j < (sizeof(arysysid)/sizeof(__uint64_t)) && !num_rules; 
	j++)
    {
      for(i=0;(i<sizeof(aryClass)/sizeof(int));i++)
      {
	/*
	 * Read all the rows in the "event_actionref" table with the specified 
	 * class and type id. Remember to also look for wildcard event classes
	 * and types if the specific class and type id is not found.
	 *  
	 */

	/* 
	 * Send sql request.
	 */
	dbReq=fp_ssdbSendRequestTrans(dsmdbError,DSMdbConnect,SSDB_REQTYPE_SELECT,
				   SSDB_EVENT_RULE_TABLE,
				   "SELECT action_id from %s WHERE "
				   "class_id=%d and type_id=%d and "
				   "sys_id = '%llX'",
				   SSDB_EVENT_RULE_TABLE,aryClass[i],
				   aryType[i], arysysid[j]);
	if(!dbReq)
	  {				
	    /* 
	     * sql request did not work. 
	     */
#if EVENTS_DEBUG
	    if(verbose > VERB_INFO)
	      {
		fprintf(stderr,"SSDB(DSM:1) fp_ssdbSendRequestTrans Error:");
		dsm_last_error_ssdb();
	      }
	    num_errors_in_dsm_ssdb++;
#endif
#if !SSDB_CONNECT_ONCE
	    fp_ssdbCloseConnection(dsmdbError,DSMdbConnect);
#endif
	    return DSM_ERROR(DSM_MAJ_SSDB_ERR,DSM_MIN_DB_NORULE);
	  }
	/*
	 * Iterate as long as we get the object id.
	 */
	while(fp_ssdbGetNextField(dsmdbError,dbReq,&objid,sizeof(int)))
	  {
	    /*
	     * Find the rule for the objid retrieved from the SSDB.
	     * Just ignore any errors.
	     * Do this to maintain the strictly followed principle that if any
	     * event is in memory, all rules applicable to it also are in 
	     * memory.
	     */
	    if(dsm_find_rule(objid,&Prule) || !Prule)
	      continue;
	    
	    /* 
	     * Make the event<->rule link.
	     */
	    dsm_add_eventrule_to_event(objid,Pev);
	    
#if EVENTS_DEBUG
	    if(verbose > VERB_DEBUG_2)
	      printf("Event: Class=%d, Type=%d objid=%d \n",Pev->event_class,
		     Pev->event_type,objid);
#endif
	    num_rules++;
	  }
	fp_ssdbFreeRequest(dsmdbError,dbReq);
        }
      }
    
#if !SSDB_CONNECT_ONCE
    fp_ssdbCloseConnection(dsmdbError,DSMdbConnect);
#endif

#if EVENTS_DEBUG
    num_init_events_dsm_ssdb++;
#endif
    return 0;
  }

/*
 * Needs to be called by dsm_reconfig_rule. 
 * With the wildcarding feature of events applicable to rules also. 
 * It is not easy to determine which events have changed when a rule changes,
 * so just initialize all the events. During initialization take care of not 
 * overwriting any old event_rule structures.
 */
__uint64_t dsm_init_all_events_ssdb()
{
  extern struct rule *Prule_head;
  extern void dsm_is_noevent_rule(struct rule *Prule, struct rule **PPrule);

  struct event_private *Pev=Pevent_head;
  struct rule *Prule=Prule_head,*Prule_temp;
  __uint64_t err=0;

  /* 
   * First check if any rule is no longer valid for an event.
   */
  while(Prule)
  {
      /* 
       * Remove any links from events to this rule if they are no longer
       * applicable.
       */
      dsm_check_delete_event_rules_ssdb(Prule->rule_objid);

      /* 
       * Check if no event applies to this rule now.
       */
      Prule_temp=NULL;
      dsm_is_noevent_rule(Prule,&Prule_temp);
      if(Prule_temp)
      {
	  /* 
	   * This rule has no more events pointing to it.
	   * So just delete this rule.
	   */
	  Prule_temp=Prule->Pnext;
	  dsm_delete_rule(Prule);
	  Prule=Prule_temp;
      }
      else
      {
	  /* 
	   * Free up the contents of this rule as they may have changed.
	   */
	  dsm_free_rule_contents(Prule);
	  /* 
	   * Re-initialize this rule.
	   */
	  dsm_init_rule_ssdb(Prule,Prule->rule_objid);
	  Prule=Prule->Pnext;
      }
  }

  /* 
   * Now update all events.
   */
  while(Pev)
  {
      if(Pev->event_class == SSS_INTERNAL_CLASS)
      {
	  Pev=Pev->Pnext;
	  continue;
      }
      /* 
       * Create any new event->rule links. Also read in any rules pointed to
       * by event->rule links if they are no longer in memory for some reason.
       */
      if(err=dsm_init_event_ssdb(Pev))
	  return err;
      Pev=Pev->Pnext;
  }

  return 0;
}

/*
 * Initialize the retry, timeout and user part of the rule.
 */
__uint64_t dsm_init_retry_timeout_user_ssdb(struct rule *rule,int Iobjid,
					    ssdb_Connection_Handle dbConnect)
{
    ssdb_Request_Handle dbReq;
    int Itemp;
    char *Pchar=NULL;

    if(!rule || !rule->Paction || !Iobjid)
	return DSM_ERROR(DSM_MAJ_INIT_ERR,DSM_MIN_INVALID_ARG);
    
    if(!fp_ssdbIsConnectionValid(dsmdbError,dbConnect))
	return DSM_ERROR(DSM_MAJ_INIT_ERR,DSM_MIN_INVALID_ARG);
    
    /* 
     * Send the sql request.
     */
    dbReq=fp_ssdbSendRequestTrans(dsmdbError,dbConnect,
			       SSDB_REQTYPE_SELECT,
			       SSDB_RULE_TABLE,
			       "SELECT retrycount, timeoutval, userstring "
			       "from %s WHERE action_id=%d",
			       SSDB_RULE_TABLE,Iobjid);

    if(!dbReq)
    {
#if EVENTS_DEBUG
	if(verbose > VERB_INFO)
	{
	    fprintf(stderr,"SSDB(DSM:10) fp_ssdbSendRequestTrans Error:");
	    dsm_last_error_ssdb();
	}
	num_errors_in_dsm_ssdb++;
#endif
	return DSM_ERROR(DSM_MAJ_SSDB_ERR,DSM_MIN_DB_NORULE);
    }

    /* 
     * Get Retry.
     */
    if(!fp_ssdbGetNextField(dsmdbError,dbReq,&rule->Paction->retry,sizeof(int)))
    {
#if EVENTS_DEBUG
	if(verbose > VERB_INFO)
	{
	    fprintf(stderr,"SSDB(DSM:11) Next Field Error:");
	    dsm_last_error_ssdb();
	}
	num_errors_in_dsm_ssdb++;
#endif
      /* 
       * failed. 
       */
      fp_ssdbFreeRequest(dsmdbError,dbReq);
      return DSM_ERROR(DSM_MAJ_SSDB_ERR,DSM_MIN_DB_NORULE);
    }

    /* 
     * Get Timeout.
     */
    if(!fp_ssdbGetNextField(dsmdbError,dbReq,&rule->Paction->timeout,sizeof(int)))
    {
#if EVENTS_DEBUG
	if(verbose > VERB_INFO)
	{
	    fprintf(stderr,"SSDB(DSM:12) Next Field Error:");
	    dsm_last_error_ssdb();
	}
	num_errors_in_dsm_ssdb++;
#endif
      /* 
       * failed. 
       */
      fp_ssdbFreeRequest(dsmdbError,dbReq);
      return DSM_ERROR(DSM_MAJ_SSDB_ERR,DSM_MIN_DB_NORULE);
    }

    /* 
     * Get user.
     */
    if(seh_read_string_ssdb(dsmdbError,dbReq,2,&rule->Paction->user,&Itemp))
    {
#if EVENTS_DEBUG
	if(verbose > VERB_INFO)
	{
	    fprintf(stderr,"SSDB(DSM:13) Read String Error:");
	    dsm_last_error_ssdb();
	}
	num_errors_in_dsm_ssdb++;
#endif
      /* 
       * failed. 
       */
      fp_ssdbFreeRequest(dsmdbError,dbReq);
      return DSM_ERROR(DSM_MAJ_SSDB_ERR,DSM_MIN_DB_NORULE);
    }

    if(rule->Paction->user && strlen(rule->Paction->user))
    {				
	/*
	 * All user strings are string_dup'ed because they are
	 * string_free'ed later.
	 */
	Pchar=string_dup(rule->Paction->user,strlen(rule->Paction->user));
	sem_mem_free(rule->Paction->user);
	rule->Paction->user=Pchar;
    }

    fp_ssdbFreeRequest(dsmdbError,dbReq);
    return 0;
}

/*
 * Return the default user for the ssdb.
 */
__uint64_t dsm_default_user_ssdb(char **username)
{
  static char *buf=SSS_DEFAULT_USER;

  if(username && !*username)
    *username=string_dup(buf,strlen(buf));
  
  return 0;
}

/*
 * Initialize the action part of the rule. This is a bit memory intensive 
 * because of the variable size strings that is read in.
 */
__uint64_t dsm_init_action_rule_ssdb(struct rule *rule,int Iobjid,
				     ssdb_Connection_Handle dbConnect)
{
  int Itemp;
  __uint64_t err;
  ssdb_Request_Handle dbReq;	
  char *Pchar_rule;		/* pointer to the action string in the ssdb */
  /*
   * if ssdb connection is allocated here set to 1. 
   */
  int close_connection=0;	
  int len;

  /* 
   * If a valid connection was not passed to this function, try opening one
   * here.
   */
  if(!dbConnect || !fp_ssdbIsConnectionValid(dsmdbError,dbConnect))
    {
#if SSDB_CONNECT_ONCE
      dbConnect=DSMdbConnect;
#else
      dbConnect=
	fp_ssdbOpenConnection(dsmdbError,dbClient,NULL,SSS_SSDB_DBNAME,
			   SSDB_CONFLG_RECONNECT|SSDB_CONFLG_REPTCONNECT);
      close_connection=1;
#endif
    }
  if(!dbConnect)
    {
#if EVENTS_DEBUG
      if(verbose > VERB_INFO)
	{
	  fprintf(stderr,"SSDB(DSM:4) Open Connection Error:");
	  dsm_last_error_ssdb();
	}
      num_errors_in_dsm_ssdb++;
#endif
      return err=DSM_ERROR(DSM_MAJ_INIT_ERR,DSM_MIN_INIT_DB);
    }

  /*
   * Request goes to table "event_action".
   * Select only the action.
   *
   */

  /*
   * Send sql statement.
   */
  dbReq=fp_ssdbSendRequestTrans(dsmdbError,dbConnect,SSDB_REQTYPE_SELECT,
			     SSDB_RULE_TABLE,
			     "SELECT action from %s WHERE action_id=%d",
			     SSDB_RULE_TABLE,Iobjid);

  if(!dbReq)
    {
      /*
       * sql did not work.
       */
#if EVENTS_DEBUG
      if(verbose > VERB_INFO)
	{
	  fprintf(stderr,"SSDB(DSM:3) fp_ssdbSendRequestTrans Error:");
	  dsm_last_error_ssdb();
	}
      num_errors_in_dsm_ssdb++;
#endif
#if !SSDB_CONNECT_ONCE
      if(close_connection)
	fp_ssdbCloseConnection(dsmdbError,dbConnect);
#endif
      return DSM_ERROR(DSM_MAJ_SSDB_ERR,DSM_MIN_DB_NORULE);
    }

  /*
   * Last of all, Get the action string.
   */
  if(seh_read_string_ssdb(dsmdbError,dbReq,0,&Pchar_rule,&Itemp))
    {				
      /* 
       * failed. 
       */
      fp_ssdbFreeRequest(dsmdbError,dbReq);
#if !SSDB_CONNECT_ONCE
      if(close_connection)
	fp_ssdbCloseConnection(dsmdbError,dbConnect);
#endif
#if EVENTS_DEBUG
      num_errors_in_dsm_ssdb++;
#endif
      return DSM_ERROR(DSM_MAJ_SSDB_ERR,DSM_MIN_DB_NORULE);
    }

  
  /*
   * Allocate the action_string structure.
   */
  rule->Paction=sem_mem_alloc_temp(sizeof(struct action_string));
  if(!rule->Paction)
    {
      /*
       * failed.
       */
      sem_mem_free(Pchar_rule);
      fp_ssdbFreeRequest(dsmdbError,dbReq);
#if !SSDB_CONNECT_ONCE
      if(close_connection)
	fp_ssdbCloseConnection(dsmdbError,dbConnect);
#endif
#if EVENTS_DEBUG
      num_errors_in_dsm_ssdb++;
#endif
      return DSM_ERROR(DSM_MAJ_INIT_ERR,DSM_MIN_ALLOC_MEM);
    }

  /*
   * Split the action string into its components.
   *  o user
   *  o timeout
   *  o retry
   *  o environment variables
   *  o executable and its arguments.
   */
  if(err=dsm_parse_action(&Pchar_rule,&rule->Paction->timeout,
			  &rule->Paction->retry,&rule->Paction->user,
			  &rule->Paction->envp))
    {
      /*
       * failed.
       */
      sem_mem_free(Pchar_rule);
#if !SSDB_CONNECT_ONCE
      if(close_connection)
	fp_ssdbCloseConnection(dsmdbError,dbConnect);
#endif

      return err;
    }

  if(dsm_init_retry_timeout_user_ssdb(rule,Iobjid,dbConnect))
  {
      /*
       * failed.
       */
      sem_mem_free(Pchar_rule);
#if !SSDB_CONNECT_ONCE
      if(close_connection)
	  fp_ssdbCloseConnection(dsmdbError,dbConnect);
#endif
      
      return err;
  }
  
  if(!rule->Paction->user)/* if no user found then just get the default one. */
      dsm_default_user_ssdb(&rule->Paction->user);
  

  /*
   * Finally allocate memory for the executable and its arguments.
   */
  rule->Paction->action=sem_mem_alloc_temp((len=strlen(Pchar_rule))+1);
  if(!rule->Paction->action)
    {
      /*
       * failed.
       */
      sem_mem_free(Pchar_rule);
      fp_ssdbFreeRequest(dsmdbError,dbReq);
#if !SSDB_CONNECT_ONCE
      if(close_connection)
	fp_ssdbCloseConnection(dsmdbError,dbConnect);
#endif
#if EVENTS_DEBUG
      num_errors_in_dsm_ssdb++;
#endif
      return DSM_ERROR(DSM_MAJ_INIT_ERR,DSM_MIN_ALLOC_MEM);
    }

  strncpy(rule->Paction->action,Pchar_rule,len);

  /*
   * Its finally done!.... i.e., reading in the action.
   * Free the original string and the request read from the ssdb.
   */
  sem_mem_free(Pchar_rule);
  if(!fp_ssdbFreeRequest(dsmdbError,dbReq))
    {
      /*
       * failed.
       */
#if !SSDB_CONNECT_ONCE
      if(close_connection)
	fp_ssdbCloseConnection(dsmdbError,dbConnect);
#endif
#if EVENTS_DEBUG
      num_errors_in_dsm_ssdb++;
#endif
      return DSM_ERROR(DSM_MAJ_SSDB_ERR,DSM_MIN_DB_API_ERR);
    }
#if !SSDB_CONNECT_ONCE
  if(close_connection)
    fp_ssdbCloseConnection(dsmdbError,dbConnect);
#endif

  return 0;
}

/*
 * Initialize the threshold parameters in the rule.
 */
__uint64_t dsm_init_thresholds_rule_ssdb(struct rule *rule,int Iobjid,
                                         ssdb_Connection_Handle dbConnect)
{
  int Itemp;
  __uint64_t err;
  char *Pchar_index;
  ssdb_Request_Handle dbReq;
#if !SSDB_CONNECT_ONCE
  int close_connection=0;
#endif

  rule->isdso=FALSE;
  rule->event_threshold_counts=rule->event_threshold_times=rule->isthreshold=0;

  /* 
   * If a valid connection was not passed to this function, try opening one
   * here.
   */
  if(!dbConnect || !fp_ssdbIsConnectionValid(dsmdbError,dbConnect))
    {
#if SSDB_CONNECT_ONCE
      dbConnect=DSMdbConnect;
#else
      dbConnect=
	fp_ssdbOpenConnection(dsmdbError,dbClient,NULL,SSS_SSDB_DBNAME,
			   SSDB_CONFLG_RECONNECT|SSDB_CONFLG_REPTCONNECT);
      close_connection=1;
#endif
    }
  if(!dbConnect)
    {
#if EVENTS_DEBUG
      if(verbose > VERB_INFO)
	{
	  fprintf(stderr,"SSDB(DSM:5) Open Connection Error:");
	  dsm_last_error_ssdb();
	}
      num_errors_in_dsm_ssdb++;
#endif
      return err=DSM_ERROR(DSM_MAJ_INIT_ERR,DSM_MIN_INIT_DB);
    }

    
  /*
   * Request goes to table "event_action".
   * Select only the dsmthrottle and dsmfrequency.
   *
   */
  dbReq=fp_ssdbSendRequestTrans(dsmdbError,dbConnect,SSDB_REQTYPE_SELECT,
                             SSDB_RULE_TABLE,
			     "SELECT dsmdso, dsmthrottle, dsmfrequency "
			     "from %s WHERE action_id=%d",
			     SSDB_RULE_TABLE,Iobjid);
  if(!dbReq)
    {
      dbReq=fp_ssdbSendRequestTrans(dsmdbError,dbConnect,SSDB_REQTYPE_SELECT,
				 SSDB_RULE_TABLE,
				 "SELECT dsmthrottle, dsmfrequency "
				 "from %s WHERE action_id=%d",
				 SSDB_RULE_TABLE,Iobjid);
      if(!dbReq)
	{
#if EVENTS_DEBUG
	  if(verbose > VERB_INFO)
	    {
	      fprintf(stderr,"SSDB(DSM:4) fp_ssdbSendRequestTrans Error:");
	      dsm_last_error_ssdb();
	    }
	  num_errors_in_dsm_ssdb++;
#endif

#if !SSDB_CONNECT_ONCE
	  if(close_connection)
	    fp_ssdbCloseConnection(dsmdbError,dbConnect);
#endif
	  return DSM_ERROR(DSM_MAJ_SSDB_ERR,DSM_MIN_DB_NORULE);
	}
    }
  else
    {
      /*
       * The select statement worked with the select for the dso.
       * Is this rule a dso ?.
       */
      if(!fp_ssdbGetNextField(dsmdbError,dbReq,&Itemp,sizeof(int)))
	{
#if EVENTS_DEBUG
	  if(verbose > VERB_INFO)
	    {
	      fprintf(stderr,"SSDB(DSM:3) Next Field Error:");
	      dsm_last_error_ssdb();
	    }
	  num_errors_in_dsm_ssdb++;
#endif
	  fp_ssdbFreeRequest(dsmdbError,dbReq);
#if !SSDB_CONNECT_ONCE
	  if(close_connection)
	    fp_ssdbCloseConnection(dsmdbError,dbConnect);
#endif
	  return DSM_ERROR(DSM_MAJ_INIT_ERR,DSM_MIN_INIT_RULE);	  
	}
      if(Itemp)
	rule->isdso=TRUE;
      
    }

  /*
   * Get the threshold count.
   */
  if(!fp_ssdbGetNextField(dsmdbError,dbReq,&Itemp,sizeof(int)))
    {
#if EVENTS_DEBUG
      if(verbose > VERB_INFO)
	{
	  fprintf(stderr,"SSDB(DSM:4) Next Field Error:");
	  dsm_last_error_ssdb();
	}
      num_errors_in_dsm_ssdb++;
#endif
      fp_ssdbFreeRequest(dsmdbError,dbReq);
#if !SSDB_CONNECT_ONCE
      if(close_connection)
        fp_ssdbCloseConnection(dsmdbError,dbConnect);
#endif
      return DSM_ERROR(DSM_MAJ_INIT_ERR,DSM_MIN_INIT_RULE);	  
    }
  if(Itemp > 0)
    rule->event_threshold_counts=Itemp;
  else
    rule->event_threshold_counts=0;
  
  /*
   * Get the threshold time.
   */
  if(!fp_ssdbGetNextField(dsmdbError,dbReq,&Itemp,sizeof(int)))
    {
#if EVENTS_DEBUG
      if(verbose > VERB_INFO)
	{
	  fprintf(stderr,"SSDB(DSM:5) Next Field Error:");
	  dsm_last_error_ssdb();
	}
      num_errors_in_dsm_ssdb++;
#endif

      fp_ssdbFreeRequest(dsmdbError,dbReq);
#if !SSDB_CONNECT_ONCE
      if(close_connection)
        fp_ssdbCloseConnection(dsmdbError,dbConnect);
#endif
      return DSM_ERROR(DSM_MAJ_INIT_ERR,DSM_MIN_INIT_RULE);	  
    }
  if(Itemp > 0)
    rule->event_threshold_times=Itemp;
  else
    rule->event_threshold_times=0;

  if(rule->event_threshold_counts || rule->event_threshold_times)
    rule->isthreshold=TRUE;
  else
    rule->isthreshold=FALSE;
  
  fp_ssdbFreeRequest(dsmdbError,dbReq);
#if !SSDB_CONNECT_ONCE
  if(close_connection)
    fp_ssdbCloseConnection(dsmdbError,dbConnect);
#endif

  return 0;
}

/*
 * Initialize the hostname for the rule.
 */
__uint64_t dsm_init_host_rule_ssdb(struct rule *rule,int Iobjid,
				   ssdb_Connection_Handle dbConnect)
  {
    __uint64_t err;
    char *Pchar_index;
    int Itemp;
    ssdb_Request_Handle dbReq;
    char *Pchar_hname;
#if !SSDB_CONNECT_ONCE
    int close_connection=0;
#endif
    /* 
     * If a valid connection was not passed to this function, try opening one
     * here.
     */
    if(!dbConnect || !fp_ssdbIsConnectionValid(dsmdbError,dbConnect))
      {
#if SSDB_CONNECT_ONCE
	dbConnect=DSMdbConnect;
#else
        dbConnect=
	  fp_ssdbOpenConnection(dsmdbError,dbClient,NULL,SSS_SSDB_DBNAME,
			     SSDB_CONFLG_RECONNECT|SSDB_CONFLG_REPTCONNECT);
        close_connection=1;
#endif
      }

    if(!dbConnect)
    {
#if EVENTS_DEBUG
      if(verbose > VERB_INFO)
	{
	  fprintf(stderr,"SSDB(DSM:6) Open Connection Error:");
	  dsm_last_error_ssdb();
	}
      num_errors_in_dsm_ssdb++;
#endif
      return err=DSM_ERROR(DSM_MAJ_INIT_ERR,DSM_MIN_INIT_DB);
    }
    
    /*
     * Request goes to table "event_action".
     * Select only the hostname.
     *
     */

    /*
     * Send sql request.
     */
    dbReq=fp_ssdbSendRequestTrans(dsmdbError,dbConnect,SSDB_REQTYPE_SELECT,
			       SSDB_RULE_TABLE,
			       "SELECT forward_hostname "
			       "from %s WHERE action_id=%d",
			       SSDB_RULE_TABLE,Iobjid);
    if(dbReq)
      {	
	/*
	 * Read the sys_id from the ssdb.
	 */
	if(seh_read_string_ssdb(dsmdbError,dbReq,0,&Pchar_hname,&Itemp))
	  {
#if EVENTS_DEBUG
	    if(verbose > VERB_INFO)
	      {
		fprintf(stderr,"SSDB(DSM:13) NextField Error:");
		dsm_last_error_ssdb();
	      }
	    num_errors_in_dsm_ssdb++;
#endif
	    /*
	     * failed sql request.
	     */
	    fp_ssdbFreeRequest(dsmdbError,dbReq);
#if !SSDB_CONNECT_ONCE
	    if(close_connection)
	      fp_ssdbCloseConnection(dsmdbError,dbConnect);
#endif
	    return DSM_ERROR(DSM_MAJ_SSDB_ERR,DSM_MIN_DB_MISS_FLD);
	  }
      }
    else
      {
#if EVENTS_DEBUG
	if(verbose > VERB_INFO)
	  {
	    fprintf(stderr,"SSDB(DSM:5) fp_ssdbSendRequestTrans Error:");
	    dsm_last_error_ssdb();
	  }
	num_errors_in_dsm_ssdb++;
#endif
	/*
	 * failed sql request.
	 */
#if !SSDB_CONNECT_ONCE
        if(close_connection)
          fp_ssdbCloseConnection(dsmdbError,dbConnect);
#endif
	return DSM_ERROR(DSM_MAJ_SSDB_ERR,DSM_MIN_DB_MISS_FLD);
      }

    rule->forward_hname=Pchar_hname;

    fp_ssdbFreeRequest(dsmdbError,dbReq);
#if !SSDB_CONNECT_ONCE
    if(close_connection)
      fp_ssdbCloseConnection(dsmdbError,dbConnect);
#endif

    return 0;
  }

/*
 * Function to Parse the rule.. and get its individual fields.. but do not 
 * check for syntax.. 
 * Make the assumption that
 * Syntax checking is already done before the rule is entered into the 
 * database.
 * No spaces are left in the rule.
 */
__uint64_t dsm_init_rule_ssdb(struct rule *rule,int Iobjid)
{
  char *Pchar_index=NULL,*Pchar_rule=NULL;
  struct event_private *Pev;
  int Itemp;
  __uint64_t err;
  ssdb_Request_Handle dbReq;
#if !SSDB_CONNECT_ONCE
  ssdb_Connection_Handle DSMdbConnect;

  /*
   * Get the rule from the ssdb.
   */
  DSMdbConnect=
    fp_ssdbOpenConnection(dsmdbError,dbClient,NULL,SSS_SSDB_DBNAME,
		       SSDB_CONFLG_RECONNECT|SSDB_CONFLG_REPTCONNECT);
  if(!DSMdbConnect)
    {
#if EVENTS_DEBUG
      if(verbose > VERB_INFO)
	{
	  fprintf(stderr,"SSDB(DSM:7) Open Connection Error:");
	  dsm_last_error_ssdb();
	}
      num_errors_in_dsm_ssdb++;
#endif
      return err=DSM_ERROR(DSM_MAJ_INIT_ERR,DSM_MIN_INIT_DB);
    }
#endif

  /*
   * Init the objid.
   */
  rule->rule_objid=Iobjid;

  /*
   * Init the hostname.
   */
  if(err=dsm_init_host_rule_ssdb(rule,Iobjid,DSMdbConnect))
    {
#if !SSDB_CONNECT_ONCE
      fp_ssdbCloseConnection(dsmdbError,DSMdbConnect);
#endif
      return err;
    }

  /*
   * Init the threshold parameters.
   */
  if(err=dsm_init_thresholds_rule_ssdb(rule,Iobjid,DSMdbConnect))
    {
#if !SSDB_CONNECT_ONCE
      fp_ssdbCloseConnection(dsmdbError,DSMdbConnect);
#endif
      return err;
    }

  /*
   * the action is initialized later.
   */
  rule->Paction=NULL;

#if !SSDB_CONNECT_ONCE
  fp_ssdbCloseConnection(dsmdbError,DSMdbConnect);
#endif

#if EVENTS_DEBUG
  num_init_rules_dsm_ssdb++;
#endif
  return 0;
}

/* 
 * Convert character c (ex --> ") to escaped chars (ex --> \").
 * XXX: Hack to fix the problem of double-quotes in the action
 *      string being interpreted by mysql as a separate field
 *      in the sql string.
 */
void put_escape_char_before(char *Psrc,char *Pdest,char c)
{
  if(!c)
    return;

  while(*Psrc)
    {
	if(*Psrc == '%')
	{
	    *(Pdest++)='%';
	    *(Pdest++)='%';
	    Psrc++;
	    continue;
	}
	
	if(*(Psrc+1) == c && *Psrc != '\\')
	{
	    *(Pdest++)=*(Psrc++);
	    *(Pdest++)='\\';
	    *(Pdest++)=*(Psrc++);
	}
	else
	{
	    *(Pdest++)=*(Psrc++);
	}
    }
  *Pdest='\0';
}

/*
 * Create event record and keep record-id in the permanent event structure..
 */
__uint64_t dsm_create_action_taken_record_ssdb(struct rule *Prule,
					       struct event_private *Pev)
{
  ssdb_Request_Handle dbReq;
#if !SSDB_CONNECT_ONCE
  ssdb_Connection_Handle DSMdbConnect;
#endif
  char *sql_buf;
  system_info_t *Psys;
  int sql_len;
  char *Pchar=NULL;

  if(!Prule->Paction)		/* Silly Check. */
    return 0;

  /*
   * Get the sys_id for the host.
   */
  if(sgm_get_sysid(&Psys,Pev->DSMDATA.hostname) || !Psys)
    return SEH_ERROR(SEH_MAJ_SSDB_ERR,SEH_MIN_DB_SYSID);

  
  /*
   * Allocate the sql buffer.
   * The reason the 4*strlen(RUN(Pev)->action_taken) is there is that the
   * quotes in the RUN(Pev)->action_taken buffer will be expanded to escaped
   * quotes by the function put_escape_char_before. As we don't know how many
   * quotes are present in RUN(Pev)->action_taken... the safe thing to do is
   * to allocate 4 times its length. See "man strecpy" for an explanation.
   */

  if(RUN(Pev)->action_taken)
    {
      if(!(sql_buf=sem_mem_alloc_temp(sizeof(char)*
				  (sql_len=
				   strlen(SSDB_ACTION_TAKEN_TABLE)+
				   (4*strlen(RUN(Pev)->action_taken))+
				   SSDB_SQL_DEFAULT_SIZE))))
	{
	  /*
	   * nobody else needs this...
	   * also this has to be freed before returning from this 
	   * function.
	   */
	  sem_mem_free(RUN(Pev)->action_taken);
	  RUN(Pev)->action_taken=NULL;
	  RUN(Pev)->action_time=0;
	  return DSM_ERROR(DSM_MAJ_SSDB_ERR,DSM_MIN_ALLOC_MEM);
	}
    }
  else 
    if(!(sql_buf=sem_mem_alloc_temp(sizeof(char)*(sql_len=
					      strlen(SSDB_ACTION_TAKEN_TABLE)+
					      SSDB_SQL_DEFAULT_SIZE))))
      return DSM_ERROR(DSM_MAJ_SSDB_ERR,DSM_MIN_ALLOC_MEM);
					    
#if !SSDB_CONNECT_ONCE
  DSMdbConnect=
    fp_ssdbOpenConnection(dsmdbError,dbClient,NULL,SSS_SSDB_DBNAME,
		       SSDB_CONFLG_RECONNECT|SSDB_CONFLG_REPTCONNECT);
  if(!DSMdbConnect)
    {				
      /* 
       * failed.
       */
#if EVENTS_DEBUG
      if(verbose > VERB_INFO)
	{
	  fprintf(stderr,"SSDB(DSM:9) Open Connection Error:");
	  dsm_last_error_ssdb();
	}
      num_errors_in_dsm_ssdb++;
#endif
      sem_mem_free(sql_buf);
      if(RUN(Pev)->action_taken)
	{
	  /*
	   * nobody else needs this...
	   * also this has to be freed before returning from this 
	   * function.
	   */
	  sem_mem_free(RUN(Pev)->action_taken);
	  RUN(Pev)->action_taken=NULL;
	  RUN(Pev)->action_time=0;
	}
      return DSM_ERROR(DSM_MAJ_SSDB_ERR,DSM_MIN_DB_API_ERR);
    }
#endif

  /*
   * Request goes to table "action_taken".
   * Select all rows and only the columns we need in the table.
   */
  if(RUN(Pev)->action_taken)
    {
      /* 
       * Buffer is needed to convert any " to \".
       */
      Pchar=
	sem_mem_alloc_temp((4*strlen(RUN(Pev)->action_taken))*sizeof(char));
      if(!Pchar)
	{
	  /*
	   * nobody else needs this...
	   * also this has to be freed before returning from this 
	   * function.
	   */
	  sem_mem_free(RUN(Pev)->action_taken);
	  RUN(Pev)->action_taken=NULL;
	  RUN(Pev)->action_time=0;
	  sem_mem_free(sql_buf);
	  return DSM_ERROR(DSM_MAJ_SSDB_ERR,DSM_MIN_ALLOC_MEM);
	}
      /* 
       * Convert quotes (") to escaped quotes (\").
       * XXX: Hack to fix the problem of double-quotes in the action
       *      string being interpreted by mysql as a separate field
       *      in the sql string.
       */
      put_escape_char_before(RUN(Pev)->action_taken,Pchar,'\"');

      /*
       * Create the action taken record. Build the sql statement.
       */
      snprintf(sql_buf,sql_len,
	       "INSERT into %s values(\"%llX\",%d,NULL,%d,%ld,\"%s\")",
	       SSDB_ACTION_TAKEN_TABLE,Psys->system_id,Pev->DSMDATA.event_id,
	       Prule->rule_objid,
	       RUN(Pev)->action_time,Pchar);

#if EVENTS_DEBUG
      if(last_proc_executed)
	sem_mem_free(last_proc_executed);
      last_proc_executed=Pchar;
      time_last_proc_executed=RUN(Pev)->action_time;
#else
      sem_mem_free(Pchar);
#endif
      sem_mem_free(RUN(Pev)->action_taken);
      RUN(Pev)->action_taken=NULL;
      RUN(Pev)->action_time=0;
    }
  else
    snprintf(sql_buf,sql_len,
	     "INSERT into %s values(\"%llX\",%d,NULL,%d,0,\"\")",
	     SSDB_ACTION_TAKEN_TABLE,Psys->system_id,Pev->DSMDATA.event_id,
	     Prule->rule_objid);
  sss_block_alarm();	/* update/insert block the alarm. */
  dbReq=fp_ssdbSendRequestTrans(dsmdbError,DSMdbConnect,SSDB_REQTYPE_INSERT,
			     SSDB_ACTION_TAKEN_TABLE,sql_buf);

#if EVENTS_DEBUG
  if(verbose > VERB_INFO)
      fprintf(stderr,"sql:?%s?\n",sql_buf);
#endif
  sss_unblock_alarm();
  
#if (RULES_DEBUG > 1)
  fprintf(stderr,"Trying to insert into " SSDB_ACTION_TAKEN_TABLE 
	  " ?%s?, Result=%lx\n",sql_buf, dbReq);
#endif
  
  if(dbReq)
    {
      fp_ssdbFreeRequest(dsmdbError,dbReq);      
    }
  else
    {
#if EVENTS_DEBUG
      if(verbose > VERB_INFO)
	{
	  fprintf(stderr,"SSDB(DSM:7) fp_ssdbSendRequestTrans Error:");
	  dsm_last_error_ssdb();
	}
      num_errors_in_dsm_ssdb++;
#endif
      /* request failed. */
      sem_mem_free(sql_buf);
#if !SSDB_CONNECT_ONCE
      fp_ssdbCloseConnection(dsmdbError,DSMdbConnect);
#endif
      return DSM_ERROR(DSM_MAJ_SSDB_ERR,DSM_MIN_DB_WRIT_REC);
    }

  /*
   * Request goes to table "event". Write only the action_taken field.
   */
  sss_block_alarm();	/* update/insert, block the alarm. */

  dbReq=fp_ssdbSendRequestTrans(dsmdbError,DSMdbConnect,SSDB_REQTYPE_UPDATE,
			     SSDB_EVENT_DATA_TABLE,
			     "UPDATE %s SET event_action=1 WHERE event_id=%d",
			     SSDB_EVENT_DATA_TABLE,Pev->DSMDATA.event_id);
#if EVENTS_DEBUG
  if(verbose > VERB_DEBUG_2)
    fprintf(stderr,"sql:?%s?\n",sql_buf);
#endif
  sss_unblock_alarm();

  if(dbReq)
    {
      fp_ssdbFreeRequest(dsmdbError,dbReq);      
    }
  else
    {
      /* 
       * failed.
       */
#if EVENTS_DEBUG
      if(verbose > VERB_INFO)
	{
	  fprintf(stderr,"SSDB(DSM:8) fp_ssdbSendRequestTrans Error:");
	  dsm_last_error_ssdb();
	}
      num_errors_in_dsm_ssdb++;
#endif
      sem_mem_free(sql_buf);
#if !SSDB_CONNECT_ONCE
      fp_ssdbCloseConnection(dsmdbError,DSMdbConnect);
#endif
      return DSM_ERROR(DSM_MAJ_SSDB_ERR,DSM_MIN_DB_WRIT_REC);
    }

  sem_mem_free(sql_buf);
#if !SSDB_CONNECT_ONCE
  fp_ssdbCloseConnection(dsmdbError,DSMdbConnect);  
#endif

#if EVENTS_DEBUG
  if(verbose > VERB_DEBUG_1)
    printf("C=%d E=%d Write SSDB %s\n",
	   Pev->event_class,Pev->event_type,Prule->Paction->action);
  num_actions_taken_records++;
#endif

  return 0;
}

/*
 * Setup the global flags applicable to the dsm. This should really be read in
 * from the ssdb.
 */
__uint64_t dsm_setup_globals_ssdb()
{
  char sql_buf[SSDB_SQL_DEFAULT_SIZE];
  ssdb_Request_Handle dbReq;
#if !SSDB_CONNECT_ONCE
  ssdb_Connection_Handle DSMdbConnect;
#endif
  int Itemp;
  char *Pchar=NULL;


#if !SSDB_CONNECT_ONCE
  DSMdbConnect=
    fp_ssdbOpenConnection(dsmdbError,dbClient,NULL,SSS_SSDB_DBNAME,
		       SSDB_CONFLG_RECONNECT|SSDB_CONFLG_REPTCONNECT);
  if(!DSMdbConnect)
    {
#if EVENTS_DEBUG
      if(verbose > VERB_INFO)
	{
	  fprintf(stderr,"SSDB(SEH:7) Open Connection Error:");
	  dsm_last_error_ssdb();
	}
      num_errors_in_dsm_ssdb++;
#endif
      return DSM_ERROR(DSM_MAJ_SSDB_ERR,DSM_MIN_DB_API_ERR);
    }
#endif
  
  snprintf(sql_buf,SSDB_SQL_DEFAULT_SIZE,"SELECT option_default "
	   "from %s where tool_name=\"%s\" and tool_option=\"%s\"",
	   SSDB_TOOL_TABLE,TOOLNAME,GLOBAL_ACTION);
  
  sss_block_alarm();
  dbReq=fp_ssdbSendRequestTrans(dsmdbError,DSMdbConnect,SSDB_REQTYPE_SELECT,
			     SSDB_TOOL_TABLE,sql_buf);
  sss_unblock_alarm();

#if EVENTS_DEBUG
  if(verbose > VERB_DEBUG_2)
    fprintf(stderr,"sql:?%s?\n",sql_buf);
#endif

  if(dbReq)
    {
      if(seh_read_string_ssdb(dsmdbError,dbReq,0,&Pchar,&Itemp) || !Pchar)
	{
	  /* 
	   * failed
	   */
	  fp_ssdbFreeRequest(dsmdbError,dbReq);
#if !SSDB_CONNECT_ONCE
	  fp_ssdbCloseConnection(dsmdbError,DSMdbConnect);
#endif
#if EVENTS_DEBUG
	  num_errors_in_dsm_ssdb++;
#endif
	  return DSM_ERROR(DSM_MAJ_SSDB_ERR,DSM_MIN_DB_MISS_FLD);
	}
      CLEAR_ERRNO();
      Itemp=(int)strtol(Pchar,NULL,10);
      if(errno)
	{
	  sem_mem_free(Pchar);
	  fp_ssdbFreeRequest(dsmdbError,dbReq);
#if !SSDB_CONNECT_ONCE
	  fp_ssdbCloseConnection(dsmdbError,DSMdbConnect);
#endif
#if EVENTS_DEBUG
	  num_errors_in_dsm_ssdb++;
#endif
	  return DSM_ERROR(DSM_MAJ_SSDB_ERR,DSM_MIN_DB_MISS_FLD);
	}
      if(Itemp)
	dsm_set_action(TRUE);
      else
	dsm_set_action(FALSE);
      sem_mem_free(Pchar);
    }
  else
    {
#if EVENTS_DEBUG
      if(verbose > VERB_INFO)
	{
	  fprintf(stderr,"SSDB(SEH:8) fp_ssdbSendRequestTrans Error:");
	  dsm_last_error_ssdb();
	}
      num_errors_in_dsm_ssdb++;
#endif
#if !SSDB_CONNECT_ONCE
      fp_ssdbCloseConnection(dsmdbError,DSMdbConnect);
#endif
      return DSM_ERROR(DSM_MAJ_SSDB_ERR,DSM_MIN_DB_MISS_FLD);
    }

#if EVENTS_DEBUG
  num_init_globals_dsm_ssdb++;
#endif
  return 0;
}

/*
 * Free our resources.
 */
void dsm_ssdb_clean()
{
#if SSDB_CONNECT_ONCE
  fp_ssdbCloseConnection(dsmdbError,DSMdbConnect);
#endif
  fp_ssdbDeleteErrorHandle(dsmdbError);
}
