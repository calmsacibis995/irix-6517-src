/*
 * File is responsible for any function which interfaces directly to the SSSDB.
 * These functions are called only by the SEH.
 */
#include "common.h"
#include "common_ssdb.h"
#include "events.h"
#include "events_private.h"
#include "seh.h"
#include "seh_errors.h"
#include "msglib.h"
#include "ssdberr.h"
#include "ssdbapi.h"
#include "sss_sysid.h"

static __uint64_t get_sgm_config_details(ssdb_Error_Handle, ssdb_Connection_Handle, SGMLicense_t *);
__uint64_t set_sgm_config_details(SGMLicense_t *);

/*
 * This is a global flag and applies to all events.
 * Don't log anything in the database if this is turned off. 
 */
extern BOOLEAN global_logging_flag;

/*
 * Level of verbose'ness in messages we display.
 */
extern int verbose;
extern struct event_private *Pevent_head;
extern SGMLicense_t license;


ssdb_Client_Handle     dbClient;	/* One ssdb client per process */
static ssdb_Error_Handle semdbError; /* Error handle one per thread. */
#if SSDB_CONNECT_ONCE
static ssdb_Connection_Handle SEMdbConnect;
#endif

/* 
 * This structure contains the mapping of the class value to table name.
 */
struct classtotable_s {
    struct classtotable *Pnext;
    struct classtotable *Pbefore;

    char *table;		/* table name */
    int class;			/* class value */
};

typedef struct classtotable_s classtotable_t;

classtotable_t *PClasstoTblsHead=NULL,*PClasstoTblsTail=NULL;

static __uint64_t seh_init_class_to_tables();

#if EVENTS_DEBUG
unsigned int num_errors_in_seh_ssdb=0;
unsigned int num_init_events_seh_ssdb=0;
unsigned int num_created_event_records_seh_ssdb=0;
unsigned int num_init_globals_seh_ssdb=0;

static void seh_last_error_ssdb()
{
  fprintf(stderr,"SSDB error code=%d, SSDB error string = %s\n",
	  ssdbGetLastErrorCode(semdbError),
	  ssdbGetLastErrorString(semdbError));
}
#endif

__uint64_t seh_read_sysinfo(SGMLicense_t *lic)
{
    __uint64_t err=SEH_ERROR(SEH_MAJ_INIT_ERR, SEH_MIN_INIT_DB);

    if ( init_sysinfolist(dbClient,semdbError,lic->current_mode))
      return err;

    return 0;
}

/*
 * Set up the interfaces to the SSDB for the SEH. The connection handle is
 * not acquired here as the SSDB may terminate the connection after a certain
 * timeout. 
 */
__uint64_t seh_setup_interfaces_ssdb()
{
  __uint64_t err=SEH_ERROR(SEH_MAJ_INIT_ERR,SEH_MIN_INIT_DB);

  ssdb_Request_Handle dbReq;
  char sql_buf[SSDB_SQL_DEFAULT_SIZE];

  semdbError=ssdbCreateErrorHandle();
  if(!semdbError)
    {
      return err;
    }
  dbClient=ssdbNewClient(semdbError,SSS_SSDB_USERNAME,SSS_SSDB_PASSWORD,0);
  if(!dbClient)
    {
#if EVENTS_DEBUG
      if(verbose > VERB_INFO)
	fprintf(stderr,"SSDB(SEH:0) New Client Error:");
      seh_last_error_ssdb();
#endif
      return err;
    }

#if SSDB_CONNECT_ONCE
  SEMdbConnect=
    ssdbOpenConnection(semdbError,dbClient,NULL,SSS_SSDB_DBNAME,
		       SSDB_CONFLG_RECONNECT|SSDB_CONFLG_REPTCONNECT);
  if(!SEMdbConnect)
    {
#if EVENTS_DEBUG
      if(verbose > VERB_INFO)
	{
	  fprintf(stderr,"SSDB(SEM:10) Open Connection Error:");
	  seh_last_error_ssdb();
	}
      num_errors_in_seh_ssdb++;
#endif
      return err=SEH_ERROR(SEH_MAJ_INIT_ERR,SEH_MIN_INIT_DB);
    }
#endif

  /*
   * for now ignore error. 
   */
  seh_init_class_to_tables();
    
#ifdef SGM
  /*
   * Get license details from SSDB 
   */

   if ( get_sgm_config_details (semdbError, SEMdbConnect, &license))
       return err;

   /*
    * Check License and setup timer.  If there is no valid
    * license, we modify license.mode to be SEM.
    */

    license.flag |= SGM_SSDBUPDT_REQD;
    if ( check_sgm_license(&license) ) {

#if EVENTS_DEBUG
    if ( verbose > VERB_INFO ) 
        fprintf(stderr, "Running as SGM\n");
#endif
	/* Install the license timer */

	license.current_mode = license.requested_mode = RUN_AS_SGM;
        set_sgm_config_details(&license);

    } else {
#if EVENTS_DEBUG
    if ( verbose > VERB_INFO ) 
        fprintf(stderr, "Running as SEM\n");
#endif
	license.current_mode = license.requested_mode = RUN_AS_SEM;
        set_sgm_config_details(&license);
    }
#endif

#if EVENTS_DEBUG
    if ( verbose > VERB_INFO ) {
	fprintf(stderr, "License Details : %d, %d,%d, %d, %d\n", 
		license.requested_mode, license.current_mode, license.status, 
                license.max_subscriptions,
		license.curr_subscriptions);
    }
#endif 

  /*
   * setup the SGM sysid code.
   */
  if(err=seh_read_sysinfo(&license))
    return err;

  return 0;
}

/* 
 * Read in a string from the ssdb. Function assumes that the actual sql 
 * request to read in the string has already been done by the caller.
 */
__uint64_t seh_read_string_ssdb(ssdb_Error_Handle dbError,
				ssdb_Request_Handle dbReq,int Ifieldno,
				char **str,int *PItemp)
{
  if(!str)
    return SEH_ERROR(SEH_MAJ_SSDB_ERR,SEH_MIN_INVALID_ARG);

  if(!dbReq)
    return SEH_ERROR(SEH_MAJ_SSDB_ERR,SEH_MIN_INVALID_ARG);

  if((*PItemp=ssdbGetFieldMaxSize(dbError,dbReq,Ifieldno)) < 0 ) 
    {
#if EVENTS_DEBUG
      num_errors_in_seh_ssdb++;
#endif
      return SEH_ERROR(SEH_MAJ_SSDB_ERR,SEH_MIN_DB_API_ERR);	  
    }
  
  *str=sem_mem_alloc_temp(*PItemp+2);
  if(!*str)
    {
      return SEH_ERROR(SEH_MAJ_SSDB_ERR,SEH_MIN_ALLOC_MEM);
    }
  if(!ssdbGetNextField(dbError,dbReq,*str,*PItemp+1))
    {
#if EVENTS_DEBUG
      if(verbose > VERB_INFO)
	{
	  fprintf(stderr,"SSDB(SEH:5) Next Field Error:");
	  seh_last_error_ssdb();
	}
      num_errors_in_seh_ssdb++;
#endif
      sem_mem_free(*str);
      *str=NULL;
      return SEH_ERROR(SEH_MAJ_SSDB_ERR,SEH_MIN_DB_MISS_FLD);	  
    }
  (*str)[*PItemp]='\0';
  return 0;
}  


static int  seh_is_class_exist(classtotable_t *Pctot,void *Pclass)
{
    if(!Pctot || !Pclass)
	return 0;

    if(Pctot->class == *(int *)Pclass)
	return 1;

    /* 
     * Take the wildcard mapping if available.
     */
    if(Pctot->class == WILDCARD_EVENT)
	return 1;
    
    return 0;
}

static  classtotable_t * seh_find_table_from_class(int class)
{
    return (classtotable_t *)FIND_DLIST(PClasstoTblsHead,NULL,
					seh_is_class_exist,&class);
}

static __uint64_t seh_delete_class_to_tables(classtotable_t *Pctot,void *unuse)
{

    if(!Pctot)
	return 0;
    
    DELETE_DLIST(PClasstoTblsHead,PClasstoTblsTail,Pctot);
    
    string_free(Pctot->table,strlen(Pctot->table));
    sem_mem_free(Pctot);
    
    return 0;
}


static void seh_delete_all_class_to_tables()
{
    TRAVERSE_DLIST(PClasstoTblsHead,PClasstoTblsTail,
		   seh_delete_class_to_tables,NULL);
}

/*
 * Read the class to table-name mapping from the ssdb.
 */
static __uint64_t seh_init_class_to_tables()
{
    int num_events,i;
    char sql_buf[SSDB_SQL_DEFAULT_SIZE];
    ssdb_Request_Handle dbReq;

    /*
     * Request goes to table "class".
     * Select all rows and only the columns we need in the table.
     * In one big swoop we read in the class_id and table names in the
     * whole table.
     *
     * Build sql statement.
     * Notice the "order by ... desc" option being used now.
     * The ADD_DLIST_AT_TAIL is related to this. If we ever change the "desc"
     * to "asc" for some unknown reason we will need to change the 
     * ADD_DLIST_AT_TAIL to ADD_DLIST_AT_HEAD.
     */
    snprintf(sql_buf,SSDB_SQL_DEFAULT_SIZE,
	     "select tool_option, option_default "
	     "from %s where tool_name = \"SEM_MAPPING\" "
	     "ORDER by tool_option desc",
	     SSDB_TOOL_TABLE);
    /*
     * send sql request.
     */
#if EVENTS_DEBUG
    if(verbose > VERB_DEBUG_2)
	fprintf(stderr,"sql:?%s?\n",sql_buf);
#endif
    dbReq=ssdbSendRequestTrans(semdbError,SEMdbConnect,SSDB_REQTYPE_SELECT,
			       SSDB_TOOL_TABLE,sql_buf);
    if(dbReq)
    {
	/*
	 * sql statement worked.
	 */
	num_events=ssdbGetNumRecords(semdbError,dbReq);
    }
    else
    {
	/*
	 * sql request did not work.
	 */
#if EVENTS_DEBUG
	if(verbose > VERB_INFO)
	{
	    fprintf(stderr,"SSDB(SEH:8) ssdbSendRequestTrans Error:");
	    seh_last_error_ssdb();
	}
#endif
	return SEH_ERROR(SEH_MAJ_SSDB_ERR,SEH_MIN_DB_API_ERR);
    }

    /*
     * Now read in the class<->tablename pairs.
     */
    for(i=0;i<num_events;i++)
    {
	classtotable_t *Pctot;
	char *table;
	int class;
	int Itemp;

	/* class */
	if(seh_read_string_ssdb(semdbError,dbReq,0,&table,&Itemp))
	{
#if EVENTS_DEBUG
	    if(verbose > VERB_INFO)
	    {
		fprintf(stderr,"SSDB(SEH:9) Next Field Error:");
		seh_last_error_ssdb();
	    }
#endif
	    /*
	     * failed.
	     */
	    ssdbFreeRequest(semdbError,dbReq);
	    return SEH_ERROR(SEH_MAJ_SSDB_ERR,SEH_MIN_DB_MISS_FLD);
	}


	/* 
	 * Read in the class.
	 */
	if(!table || (sscanf(table,"%d",&class) != 1))
	    continue;

	
	/*
	 * Get the table name.
	 */
	if(seh_read_string_ssdb(semdbError,dbReq,1,&table,&Itemp))
	{
	    /*
	     * failed.
	     */
	    ssdbFreeRequest(semdbError,dbReq);
	    return SEH_ERROR(SEH_MAJ_SSDB_ERR,SEH_MIN_DB_MISS_FLD);
	}

	if(!table)              /* Don't insert if no table string. */
	    continue;

	if(Pctot=seh_find_table_from_class(class))
	{
	    string_free(Pctot->table,strlen(Pctot->table));
	    Pctot->table=string_dup(table,strlen(table));
	}
	else
	{
	    Pctot=sem_mem_alloc_temp(sizeof(classtotable_t));
	    if(!Pctot)
	    {
		ssdbFreeRequest(semdbError,dbReq);
		return SEH_ERROR(SEH_MAJ_SSDB_ERR,SEH_MIN_ALLOC_MEM);
	    }

	    Pctot->class=class;
	    Pctot->table=string_dup(table,strlen(table));
	    ADD_DLIST_AT_TAIL(PClasstoTblsHead,PClasstoTblsTail,Pctot);
	}
#if EVENTS_DEBUG
	if(verbose > VERB_DEBUG_1)
	{
	    fprintf(stderr,"Class to Tables C=%d, Table=%s\n",class,table);
	}
#endif
    }
    return 0;
}


/*
 * Read in the event parameters from the SSDB for the event.
 * Function assumes that the ssdb request transaction to read in the event is
 * already done by the caller.
 */
static __uint64_t seh_init_event_ssdb(struct event_private *Pev,
			       ssdb_Request_Handle dbReq)
{
  int Itemp;
  short Stemp;


  /* 
   * Added the check for Itemp <= 0 for class and type only as wildcard
   * entries are no longer tolerated for the class and type. Anytime we
   * decide to go back to having wildcard entries this check has to be
   * removed.
   */

  /* class */
  if(!ssdbGetNextField(semdbError,dbReq,&Itemp,sizeof(int)) || Itemp <= 0)
    {
#if EVENTS_DEBUG
      if(verbose > VERB_INFO)
	{
	  fprintf(stderr,"SSDB(SEH:0) Next Field Error:");
	  seh_last_error_ssdb();
	}
      num_errors_in_seh_ssdb++;
#endif
      return SEH_ERROR(SEH_MAJ_INIT_ERR,SEH_MIN_INIT_EVENT);
    }
  if(Itemp > 0)
      Pev->event_class = Itemp;	

  /* type */
  if(!ssdbGetNextField(semdbError,dbReq,&Itemp,sizeof(int)) || Itemp <= 0)
    {
#if EVENTS_DEBUG
      if(verbose > VERB_INFO)
	{
	  fprintf(stderr,"SSDB(SEH:1) Next Field Error:");
	  seh_last_error_ssdb();
	}
      num_errors_in_seh_ssdb++;
#endif
      return SEH_ERROR(SEH_MAJ_INIT_ERR,SEH_MIN_INIT_EVENT);
    }
  if(Itemp > 0)
      Pev->event_type = Itemp;	

  /* should this event be throttled ?. */
  if(!ssdbGetNextField(semdbError,dbReq,&Itemp,sizeof(int)))
    {
#if EVENTS_DEBUG
      if(verbose > VERB_INFO)
	{
	  fprintf(stderr,"SSDB(SEH:2) Next Field Error:");
	  seh_last_error_ssdb();
	}
      num_errors_in_seh_ssdb++;
#endif
      return SEH_ERROR(SEH_MAJ_INIT_ERR,SEH_MIN_INIT_EVENT);
    }
  if(Itemp)
    Pev->isthrottle = TRUE;	
  else
    Pev->isthrottle = FALSE;
  
  /* Throttle parameter -- count */
  if(!ssdbGetNextField(semdbError,dbReq,&Pev->event_throttle_counts,
		       sizeof(unsigned long)))
    {
#if EVENTS_DEBUG
      if(verbose > VERB_INFO)
	{
	  fprintf(stderr,"SSDB(SEH:3) Next Field Error:");
	  seh_last_error_ssdb();
	}
      num_errors_in_seh_ssdb++;
#endif

      Pev->event_throttle_counts = Pev->event_throttle_times = 0;
      Pev->isthrottle=FALSE;

      return SEH_ERROR(SEH_MAJ_INIT_ERR,SEH_MIN_INIT_EVENT);
    }
  if(Pev->event_throttle_counts < 0)
    Pev->event_throttle_counts = 0;

  /* Throttle parameter -- time */
  if(!ssdbGetNextField(semdbError,dbReq,&Pev->event_throttle_times,
		       sizeof(unsigned long)))
    {
#if EVENTS_DEBUG
      if(verbose > VERB_INFO)
	{
	  fprintf(stderr,"SSDB(SEH:4) Next Field Error:");
	  seh_last_error_ssdb();
	}
      num_errors_in_seh_ssdb++;
#endif

      Pev->event_throttle_times = 0;
      if(!Pev->event_throttle_counts)
	Pev->isthrottle=FALSE;

      return SEH_ERROR(SEH_MAJ_INIT_ERR,SEH_MIN_INIT_EVENT);
    }
  if(Pev->event_throttle_times < 0)
    Pev->event_throttle_times = 0;
  
  if(!Pev->event_throttle_counts && !Pev->event_throttle_times)
    Pev->isthrottle=FALSE;

  return 0;
}

/*
 * Write the data given the event_id in the event data tables..
 */
__uint64_t 
seh_write_record_ssdb(ssdb_Connection_Handle dbConnect,char *table,
		      __uint64_t sys_id,int event_id,int count,
		      char *Pdata)
{
  ssdb_Request_Handle dbReq;
  __uint64_t err=0;
  char *Pchar;
  int num_records;

  if(!ssdbIsConnectionValid(semdbError,dbConnect))
    {
#if EVENTS_DEBUG
      num_errors_in_seh_ssdb++;
#endif
      return SEH_ERROR(SEH_MAJ_SSDB_ERR,SEH_MIN_DB_API_ERR);
    }

  if(!table)
    return 0;

  if(Pdata)
    {
      /*
       * Find first record. The data coming in can have multiple data records,
       * separated by a delimiter.
       */
      Pchar=strtok_r(Pdata,SSDB_RECORD_DELIMITER,&Pdata);
      /*
       * Create a record and write the data which came in. Iterate over each
       * record which came in.
       */
      for(num_records=0;Pchar;num_records++)
	{
	  sss_block_alarm();	/* update/insert block the alarm. */
	  if(count == 1)
	    {
		if(strncmp(table,SSDB_SYSTEM_DATA_TABLE,
			   strlen(SSDB_SYSTEM_DATA_TABLE)))
		{
		    /*
		     * Send the sql request.
		     */
		    dbReq=ssdbSendRequestTrans(semdbError,dbConnect,
					       SSDB_REQTYPE_UPDATE,
					       table,
					       "INSERT into %s "
					       "values(\"%llX\",NULL,%d,%s)",
					       table,sys_id,event_id,Pchar);
		}
	      else		/* assume it goes to the system data table. */
		  /*
		   * Send the sql request.
		   */
		  dbReq=ssdbSendRequestTrans(semdbError,dbConnect,
					     SSDB_REQTYPE_UPDATE,
					     table,
					     "INSERT into %s "
					     "values(\"%llX\",NULL,%d,\"%s\")",
					     table,sys_id,event_id,Pchar);
	    }
	  else 
	    {
#if NOTANYMORE
	      if(!strncmp(table,SSDB_AVAIL_DATA_TABLE,
			  strlen(SSDB_AVAIL_DATA_TABLE)))
		{
		  return 0;	/* do nothing for now if count > 1. */
		}
	      else		/* assume it goes to the system data table. */
		snprintf(sql_buf,sql_len,"UPDATE %s SET msg_string=\"%s\" "
			 "where event_id=%d",table,Pchar,event_id);
#else
	      return 0;
#endif
	    }
	  sss_unblock_alarm();
	  if(dbReq)
	    {			/* sql request worked correctly. */
	      ssdbFreeRequest(semdbError,dbReq);
	    }
	  else
	    {			/* sql request did not work. */
#if EVENTS_DEBUG
	      if(verbose > VERB_INFO)
		{
		  fprintf(stderr,
			  "SSDB(SEH:0) ssdbSendRequestTrans Error: "
			  "INSERT into %s "
			  "values(\"%llX\",NULL,%d,%s)",
			  table,sys_id,event_id,Pchar);
		  seh_last_error_ssdb();
		}
	      num_errors_in_seh_ssdb++;
#endif
	      return SEH_ERROR(SEH_MAJ_SSDB_ERR,SEH_MIN_DB_WRIT_DATA);
	    }
  	  /*
	   * Find next record.
	   */
	  Pchar=strtok_r(NULL,SSDB_RECORD_DELIMITER,&Pdata);
	}
    }
  else
    {
      sss_block_alarm();	/* update/insert block the alarm. */
      /*
       * Build the sql statement.
       */
      if(count == 1)
	  /*
	   * Send the sql request
	   */
	  dbReq=ssdbSendRequestTrans(semdbError,dbConnect,SSDB_REQTYPE_INSERT,
				     table,
				     "INSERT into %s "
				     "values(\"%llX\",NULL,%d,\"\")",
				     table,sys_id,event_id);
      else
	  /*
	   * Send the sql request
	   */
	  dbReq=ssdbSendRequestTrans(semdbError,dbConnect,SSDB_REQTYPE_INSERT,
				     table,
				     "UPDATE %s SET msg_string=\"\" "
				     "where event_id=%d",table,event_id);

      sss_unblock_alarm();

      if(dbReq)
	{			/* worked correctly. */
	  ssdbFreeRequest(semdbError,dbReq);
	}
      else
	{			/* sql request did not work. */
#if EVENTS_DEBUG
	  if(verbose > VERB_INFO)
	    {
	      fprintf(stderr,"SSDB(SEH:1) ssdbSendRequestTrans Error:");
	      seh_last_error_ssdb();
	    }
	    num_errors_in_seh_ssdb++;
#endif
	  return SEH_ERROR(SEH_MAJ_SSDB_ERR,SEH_MIN_DB_WRIT_DATA);
	}
    }
  return 0;
}

/*
 * Write the event data into the appropriate ssdb table.
 * Availmon event goes into Table avail_data
 * Eventmon event goes into Table system_data
 * Test event goes into Table Test_data.
 */
__uint64_t 
seh_write_event_data_ssdb(ssdb_Connection_Handle dbConnect,
				   struct event_private *Pev)
{
  char *Pdata=NULL;
  __uint64_t err=0;
  char *table=NULL;
  system_info_t *Psys;
  char *Pchar;
  classtotable_t *Pctot;
  
  if(!Pev)			/* silly check. */
    return 0;

  /*
   * The value for the table variable should really be table driven.
   * For now just leave it as is.
   */
  Pctot=seh_find_table_from_class(Pev->event_class);
  if(Pctot && Pctot->table)
  {
      table=Pctot->table;
  }
  else
  {
      
      /* 
       * The value for the table variable should really be table driven.
       * For now just leave it as is.
       * The table name will now be passed in as a flag in the API.
       */
      if(Pev->event_class == AVAIL_EVENT_CLASS) /* availmon event. */
	  table=SSDB_AVAIL_DATA_TABLE; 
	  /* any other new event classes should be added here. */
      else
	  table=SSDB_SYSTEM_DATA_TABLE;
  }
  /*
   * Get the sysid.
  if(sgm_get_sysid(&Psys,Pev->SEHDATA.hostname) || !Psys)
    return SEH_ERROR(SEH_MAJ_SSDB_ERR,SEH_MIN_DB_SYSID);
   */

  /*
   * If first event in the 'throttling window' then write the data into the
   * record... or else just NULL the data.
   * Currently decided after a couple of minutes discussion with the PM that
   * the write to the ssdb will happen everytime.
   */
  if(Pev->SEHDATA.event_datalength>0)
    {
      if ( Pev->event_class == SSS_INTERNAL_CLASS ) {
	Pdata=seh_form_config_event_record(Pev);
      } else {
        Pdata=sem_mem_alloc_temp(4*Pev->SEHDATA.event_datalength*sizeof(char));
	/* 
	 * Escape'ize any double-quote characters without a backslash
	 * preceding them.
	 */
        if (Pdata) put_escape_char_before(Pev->SEHDATA.Pevent_data,Pdata,'\"');
      }

      if(Pdata)
	{
	  /*
	   * Write the event data out to the SSDB.
	   */
	  err=seh_write_record_ssdb(dbConnect,table,Pev->sys_id,
				    Pev->SEHDATA.event_id,
				    RUN(Pev)->event_counts,Pdata);
	  sem_mem_free(Pdata);
	}
      else if ( Pev->event_class != SSS_INTERNAL_CLASS )
	err=seh_write_record_ssdb(dbConnect,table,Pev->sys_id,
				  Pev->SEHDATA.event_id,
				  RUN(Pev)->event_counts,
				  Pev->SEHDATA.Pevent_data);
    }
  
  return err;
}

/*
 * Create event record and read in the record-id for the event which just 
 * arrived.
 * Function creates event record and retrieves its id by doing an 
 *   o sql -- insert
 *   o sql -- select last_insert_id()
 */
__uint64_t seh_create_event_record_ssdb(struct event_private *Pev,
					struct event_private *Pev_permanent)
{
  __uint64_t err;
  int id;

  ssdb_Request_Handle dbReq;  
#if !SSDB_CONNECT_ONCE
  ssdb_Connection_Handle SEMdbConnect;
#endif
  system_info_t *Psys;
  const char **vector;

  /*
   * User has logging turned off. In this case the SEH acts as a conduit for
   * the event to the DSM. It does not do anything else to or with the event.
   */
  if(global_logging_flag == FALSE)
    return 0;

#if !SSDB_CONNECT_ONCE
  SEMdbConnect=
    ssdbOpenConnection(semdbError,dbClient,NULL,SSS_SSDB_DBNAME,
		       SSDB_CONFLG_RECONNECT|SSDB_CONFLG_REPTCONNECT);
  if(!SEMdbConnect)
    {
#if EVENTS_DEBUG
      if(verbose > VERB_INFO)
	{
	  fprintf(stderr,"SSDB(SEH:1) Open Connection Error:");
	  seh_last_error_ssdb();
	}
      num_errors_in_seh_ssdb++;
#endif
      return err=SEH_ERROR(SEH_MAJ_SSDB_ERR,SEH_MIN_DB_API_ERR);
    }
#endif

  sss_block_alarm();
  if(!ssdbLockTable(semdbError,SEMdbConnect,SSDB_EVENT_DATA_TABLE))
    {
      sss_unblock_alarm();
#if EVENTS_DEBUG
      if(verbose>VERB_INFO)
	{
	  fprintf(stderr,"SSDB Lock Error:");
	  seh_last_error_ssdb();
	}
      num_errors_in_seh_ssdb++;
#endif

#if !SSDB_CONNECT_ONCE
      ssdbCloseConnection(semdbError,SEMdbConnect);
#endif
      return err=SEH_ERROR(SEH_MAJ_SSDB_ERR,SEH_MIN_DB_LOCK_ERR);
    }
  /*
   * Request goes to table "event".
   * Create a new record.
   */
  dbReq=ssdbSendRequest(semdbError,SEMdbConnect,SSDB_REQTYPE_INSERT,
			"INSERT into %s "
			"values(\"%llX\",NULL,%d,%d,%ld,%ld,0,0,0)",
			SSDB_EVENT_DATA_TABLE,
			Pev->sys_id,
			Pev->event_class,
			Pev->event_type,RUN(Pev)->event_counts,
			RUN(Pev)->time_stamp);
  
  sss_unblock_alarm();
  if(dbReq)
    {
      ssdbFreeRequest(semdbError,dbReq);
    }
  else
    {
#if EVENTS_DEBUG
      if(verbose > VERB_INFO)
	{
	  fprintf(stderr,"SSDB(SEH:3) ssdbSendRequest Error:");
	  seh_last_error_ssdb();
	}
      num_errors_in_seh_ssdb++;
#endif
      sss_block_alarm();
      ssdbUnLockTable(semdbError,SEMdbConnect); /* unlock table */
      sss_unblock_alarm();
  
#if !SSDB_CONNECT_ONCE
      ssdbCloseConnection(semdbError,SEMdbConnect);
#endif
      return err=SEH_ERROR(SEH_MAJ_SSDB_ERR,SEH_MIN_DB_WRIT_DATA);
    }

  /*
   * SQL Request goes to table "event".
   * Get event id of record just created.
   */
  dbReq=ssdbSendRequest(semdbError,SEMdbConnect,SSDB_REQTYPE_SELECT,
			"SELECT last_insert_id()");
  if(dbReq)
    {
      /* 
       * Actually Get event-id.
       */	
      if(!(vector = ssdbGetRow(semdbError,dbReq)))
	{
#if EVENTS_DEBUG
	  if(verbose > VERB_INFO)
	    {
	      fprintf(stderr,"SSDB(SEH:0) Error getting last_insert_id:");
	      seh_last_error_ssdb();
	    }
	  num_errors_in_seh_ssdb++;
#endif
	  sss_block_alarm();
	  ssdbUnLockTable(semdbError,SEMdbConnect); /* unlock table */
	  sss_unblock_alarm();

	  ssdbFreeRequest(semdbError,dbReq);
#if !SSDB_CONNECT_ONCE
	  ssdbCloseConnection(semdbError,SEMdbConnect);
#endif
	  return SEH_ERROR(SEH_MAJ_SSDB_ERR,SEH_MIN_DB_MISS_FLD);
	}
      id=atoi(vector[0]);
      Pev_permanent->SEHDATA.event_id=id;
      ssdbFreeRequest(semdbError,dbReq);
    }
  else
    {
#if EVENTS_DEBUG
      if(verbose > VERB_INFO)
	{
	  fprintf(stderr,"SSDB(SEH:4) ssdbSendRequest Error:");
	  seh_last_error_ssdb();
	}
      num_errors_in_seh_ssdb++;
#endif
      sss_block_alarm();
      ssdbUnLockTable(semdbError,SEMdbConnect);
      sss_unblock_alarm();
  
#if !SSDB_CONNECT_ONCE
      ssdbCloseConnection(semdbError,SEMdbConnect);
#endif
      return err=SEH_ERROR(SEH_MAJ_SSDB_ERR,SEH_MIN_DB_MISS_FLD);
    }

  if(!id)
  {
      /*
       * SQL Request goes to table "event".
       * Get event id of record just created.
       */
      dbReq=ssdbSendRequest(semdbError,SEMdbConnect,SSDB_REQTYPE_SELECT,
			    "SELECT count(*) from %s",
			    SSDB_EVENT_DATA_TABLE);
      if(dbReq)
      {
	  /* 
	   * Actually Get event-id.
	   */	
	  if(!ssdbGetNextField(semdbError,dbReq,&id,sizeof(id)))
	  {
#if EVENTS_DEBUG
	      if(verbose > VERB_INFO)
	      {
		  fprintf(stderr,"SSDB(SEH:1) Error getting last_insert_id:");
		  seh_last_error_ssdb();
	      }
	      num_errors_in_seh_ssdb++;
#endif
	      sss_block_alarm();
	      ssdbUnLockTable(semdbError,SEMdbConnect); /* unlock table */
	      sss_unblock_alarm();
	      
	      ssdbFreeRequest(semdbError,dbReq);
#if !SSDB_CONNECT_ONCE
	      ssdbCloseConnection(semdbError,SEMdbConnect);
#endif
	      return SEH_ERROR(SEH_MAJ_SSDB_ERR,SEH_MIN_DB_MISS_FLD);
	  }
	  
	  Pev_permanent->SEHDATA.event_id=id;
	  ssdbFreeRequest(semdbError,dbReq);
      }
      else
      {
#if EVENTS_DEBUG
	  if(verbose > VERB_INFO)
	  {
	      fprintf(stderr,"SSDB(SEH:4) ssdbSendRequest Error:");
	      seh_last_error_ssdb();
	  }
	  num_errors_in_seh_ssdb++;
#endif
	  sss_block_alarm();
	  ssdbUnLockTable(semdbError,SEMdbConnect);
	  sss_unblock_alarm();
	  
#if !SSDB_CONNECT_ONCE
	  ssdbCloseConnection(semdbError,SEMdbConnect);
#endif
	  return err=SEH_ERROR(SEH_MAJ_SSDB_ERR,SEH_MIN_DB_MISS_FLD);
      }
  }

  /*
   * Unlock table and close ssdb connection.
   */
  sss_block_alarm();
  if(!ssdbUnLockTable(semdbError,SEMdbConnect))
    {
      sss_unblock_alarm();
#if !SSDB_CONNECT_ONCE
      ssdbCloseConnection(semdbError,SEMdbConnect);
#endif
#if EVENTS_DEBUG
      num_errors_in_seh_ssdb++;
#endif
      return err=SEH_ERROR(SEH_MAJ_SSDB_ERR,SEH_MIN_DB_LOCK_ERR);
    }
  sss_unblock_alarm();

#if !SSDB_CONNECT_ONCE
  ssdbCloseConnection(semdbError,SEMdbConnect);
#endif

#if EVENTS_DEBUG
  num_created_event_records_seh_ssdb++;
#endif

  return 0;
}


/*
 * Update event record with event-id to the new count of the events.
 */ 
__uint64_t seh_update_event_record_ssdb(struct event_private *Pev,
					struct event_record *Pev_rec)
{
  int i;
  __uint64_t err=0;

  ssdb_Request_Handle dbReq;
#if !SSDB_CONNECT_ONCE
  ssdb_Connection_Handle SEMdbConnect;
#endif
  system_info_t *Psys;


  /*
   * User has logging turned off. In this case the SEH acts as a conduit for
   * the event to the DSM. It does not do anything else to or with the event.
   */
  if(global_logging_flag == FALSE)
    return 0;

  /*
   * Is this system in our system table ?.
  if(sgm_get_sysid(&Psys,Pev->SEHDATA.hostname) || !Psys)
    {
      return SEH_ERROR(SEH_MAJ_SSDB_ERR,SEH_MIN_DB_SYSID);
    }
   */

#if !SSDB_CONNECT_ONCE
  sss_block_alarm();
  SEMdbConnect=
    ssdbOpenConnection(semdbError,dbClient,NULL,SSS_SSDB_DBNAME,
		       SSDB_CONFLG_RECONNECT|SSDB_CONFLG_REPTCONNECT);
  sss_unblock_alarm();

  if(!SEMdbConnect)
    {
      if(verbose>VERB_INFO)
	{
	  time_t t;
	  char *c;
			
	  t = time(NULL);
	  c = asctime(localtime(&t));
			 
#if EVENTS_DEBUG
	  fprintf(stderr,"SSDB(SEH:2) Open Connection Error:");
	  seh_last_error_ssdb();
	  fprintf(stderr,
		  "C=%d,T=%d,P=%d T=%ld Errno=%d\n",
		  Pev->event_class,Pev->event_type,RUN(Pev)->msgKey,
		  RUN(Pev)->event_times,errno);
	  num_errors_in_seh_ssdb++;
#endif
	}
      return SEH_ERROR(SEH_MAJ_SSDB_ERR,SEH_MIN_DB_API_ERR);
    }
#endif
  /*
   * Update table "event".
   */
  sss_block_alarm();

  dbReq=ssdbSendRequestTrans(semdbError,SEMdbConnect,SSDB_REQTYPE_UPDATE,
			     SSDB_EVENT_DATA_TABLE,
			     "UPDATE %s SET "
			     "event_count=%ld,event_end=%ld,event_time=%ld "
			     "WHERE event_id=%d",
			     SSDB_EVENT_DATA_TABLE,
			     RUN(Pev)->event_counts,RUN(Pev)->time_stamp,
			     RUN(Pev)->time_stamp,Pev->SEHDATA.event_id);
  
  sss_unblock_alarm();

  if(dbReq)
    {
      ssdbFreeRequest(semdbError,dbReq);
    }
  else
    {
#if EVENTS_DEBUG
      if(verbose > VERB_INFO)
	{
	  fprintf(stderr,"SSDB(SEH:5) ssdbSendRequestTrans Error:");
	  seh_last_error_ssdb();
	  fprintf(stderr,"UPDATE %s SET "
		  "event_count=%ld,event_end=%ld,event_time=%ld "
		  "WHERE event_id=%d",
		  SSDB_EVENT_DATA_TABLE,
		  RUN(Pev)->event_counts,RUN(Pev)->event_times,
		  RUN(Pev)->time_stamp,Pev->SEHDATA.event_id);
	}
      num_errors_in_seh_ssdb++;
#endif
#if !SSDB_CONNECT_ONCE
      ssdbCloseConnection(semdbError,SEMdbConnect);
#endif
      return SEH_ERROR(SEH_MAJ_SSDB_ERR,SEH_MIN_DB_WRIT_DATA);
    }

  /*
   * Update data field for known events.
   */
  err=seh_write_event_data_ssdb(SEMdbConnect,Pev);
  
#if !SSDB_CONNECT_ONCE
  ssdbCloseConnection(semdbError,SEMdbConnect);
#endif
  
  Pev_rec->event_class=Pev->event_class;
  Pev_rec->event_type=Pev->event_type;
  Pev_rec->event_count=RUN(Pev)->event_counts;
  Pev_rec->event_id=Pev->SEHDATA.event_id;
  Pev_rec->valid_record=TRUE;

  return err;
}

/* 
 * Function is called when update of an event is required.
 */
__uint64_t seh_update_event_ssdb(struct event_private *Pev)
{
  __uint64_t err=SEH_ERROR(SEH_MAJ_EVENT_ERR,SEH_MIN_EV_FIND);
  int num_events=0;

  ssdb_Request_Handle dbReq=0;
  int aryClass[]={WILDCARD_EVENT,WILDCARD_EVENT,WILDCARD_EVENT};
  int aryType[]={WILDCARD_EVENT,WILDCARD_EVENT,WILDCARD_EVENT};
  int i;

  /* 
   * This is the order to look for the class and the type. First do the
   * specific lookup in the ssdb, then start using the wildcard.
   * The search method gives equal weight to the wildcard and specific
   * event classes and types. However priority is given to the specific
   * classes and types because we look for them first.
   */
  aryClass[0]=aryClass[1]=Pev->event_class;
  aryType[0]=Pev->event_type;

#if !SSDB_CONNECT_ONCE
  ssdb_Connection_Handle SEMdbConnect;

  SEMdbConnect=
    ssdbOpenConnection(semdbError,dbClient,NULL,SSS_SSDB_DBNAME,
		       SSDB_CONFLG_RECONNECT|SSDB_CONFLG_REPTCONNECT);
  if(!SEMdbConnect)
    {
#if EVENTS_DEBUG
      if(verbose > VERB_INFO)
	{
	  fprintf(stderr,"SSDB(SEH:6) Open Connection Error:");
	  seh_last_error_ssdb();
	}
      num_errors_in_seh_ssdb++;
#endif
      return SEH_ERROR(SEH_MAJ_SSDB_ERR,SEH_MIN_DB_API_ERR);
    }
#endif
  
  for(i=0,num_events=0;(i<sizeof(aryClass)/sizeof(int)) && !num_events;i++)
  {
      /* 
       * Look for the class and type in the event_type table. Make sure the
       * enabled flag is set.
       */
      sss_block_alarm();
      dbReq=ssdbSendRequestTrans(semdbError,SEMdbConnect,SSDB_REQTYPE_SELECT,
				 SSDB_EVENT_TYPES_TABLE,
				 "SELECT class_id, type_id, "
				 "throttled, sehthrottle, "
				 "sehfrequency from %s "
				 "where class_id=%d and type_id=%d and "
				 "enabled=1 and sys_id = '%llX'",
				 SSDB_EVENT_TYPES_TABLE,
				 aryClass[i],aryType[i], Pev->sys_id);
      sss_unblock_alarm();
      
      if(dbReq)
      {
	  num_events=ssdbGetNumRecords(semdbError,dbReq);
	  /* 
	   * No events found.
	   */
	  if(!num_events)
	  {
	      ssdbFreeRequest(semdbError,dbReq);      
	  }
      }
      else
      {
#if EVENTS_DEBUG
	  if(verbose > VERB_INFO)
	  {
	      fprintf(stderr,"SSDB(SEH:7) ssdbSendRequestTrans Error:");
	      seh_last_error_ssdb();
	      fprintf(stderr,
		      "SELECT class_id, type_id, "
		      "throttled, sehthrottle, "
		      "sehfrequency from %s "
		      "where class_id=%d and type_id=%d and "
		      "enabled=1 and sys_id = '%llX'",
		      SSDB_EVENT_TYPES_TABLE,
		      aryClass[i],aryType[i], Pev->sys_id);
	  }
	  num_errors_in_seh_ssdb++;
#endif
#if !SSDB_CONNECT_ONCE
	  ssdbCloseConnection(semdbError,SEMdbConnect);
#endif
	  return err;
      }
  }
  
  /* 
   * No record was found for the event we are looking for.
   */
  if(!num_events)
  {
#if !SSDB_CONNECT_ONCE
      ssdbCloseConnection(semdbError,SEMdbConnect);
#endif
      return err;
  }
  
  /* 
   * Read in the contents of the event from the request we just did.
   * First insert the event being looked for.. Because the ssdb may have 
   * returned a wildcard event. The structure should not get updated
   * with the wildcard values in that case.
   */
  if(seh_init_event_ssdb(Pev,dbReq))
  {
      ssdbFreeRequest(semdbError,dbReq);
#if !SSDB_CONNECT_ONCE
      ssdbCloseConnection(semdbError,SEMdbConnect);
#endif
      return err;
  }
  
  /* 
   * Don't need the ssdb api data structures around anymore.
   * Done with ssdb initialization for this event.
   */
  ssdbFreeRequest(semdbError,dbReq);
#if !SSDB_CONNECT_ONCE
  ssdbCloseConnection(semdbError,SEMdbConnect);
#endif

  return 0;
}

/*
 * Needs to be called by seh_reconfig_event.
 */
__uint64_t seh_update_all_events_ssdb(__uint64_t sys_id)
{
  struct event_private *Pev=Pevent_head;
  struct event_private *Pev_temp=NULL;
  __uint64_t err=0;

  while(Pev)
    {
      if(Pev->event_class == SSS_INTERNAL_CLASS ||
	 Pev->sys_id != sys_id)
	{
	  Pev=Pev->Pnext;
	  continue;
	}
      Pev_temp=Pev->Pnext;
      if(err=seh_update_event_ssdb(Pev))
      {
	  seh_delete_event(Pev);
      }
      Pev=Pev_temp;
    }
  return 0;
}

/*
 * Needs to be called by seh_reconfig_event.
 */
__uint64_t seh_update_all_events_byclass_ssdb(__uint64_t sys_id, int class)
{
  struct event_private *Pev=Pevent_head;
  struct event_private *Pev_temp=NULL;
  __uint64_t err=0;

  while(Pev)
    {
      if(Pev->event_class != class              || 
	 Pev->event_class == SSS_INTERNAL_CLASS ||
	 Pev->sys_id != sys_id)
	{
	  Pev=Pev->Pnext;
	  continue;
	}
      Pev_temp=Pev->Pnext;
      if(err=seh_update_event_ssdb(Pev))
	seh_delete_event(Pev);
      Pev=Pev_temp;
    }
  return 0;
}

/*
 * Needs to be called by seh_reconfig_event.
 */
__uint64_t seh_update_all_events_bytype_ssdb(__uint64_t sys_id, int type)
{
  struct event_private *Pev=Pevent_head;
  struct event_private *Pev_temp=NULL;
  __uint64_t err=0;

  while(Pev)
    {
      if(Pev->event_type != type                || 
	 Pev->event_class == SSS_INTERNAL_CLASS ||
	 Pev->sys_id != sys_id)
	{
	  Pev=Pev->Pnext;
	  continue;
	  
	}
      Pev_temp=Pev->Pnext;
      if(err=seh_update_event_ssdb(Pev))
	seh_delete_event(Pev);
      Pev=Pev_temp;
    }
  return 0;
}


/* 
 * Function is called when a new event comes in which is not active and hence
 * is not in memory.
 */
__uint64_t seh_find_event_ssdb(int class,int type, __uint64_t system_id,
			       struct event_private **PPev)
{
  __uint64_t err=SEH_ERROR(SEH_MAJ_EVENT_ERR,SEH_MIN_EV_FIND);
  int num_events=0;

  ssdb_Request_Handle dbReq=0;

#if !SSDB_CONNECT_ONCE
  ssdb_Connection_Handle SEMdbConnect;

  SEMdbConnect=
    ssdbOpenConnection(semdbError,dbClient,NULL,SSS_SSDB_DBNAME,
		       SSDB_CONFLG_RECONNECT|SSDB_CONFLG_REPTCONNECT);
  if(!SEMdbConnect)
    {
#if EVENTS_DEBUG
      if(verbose > VERB_INFO)
	{
	  fprintf(stderr,"SSDB(SEH:6) Open Connection Error:");
	  seh_last_error_ssdb();
	}
      num_errors_in_seh_ssdb++;
#endif
      return SEH_ERROR(SEH_MAJ_SSDB_ERR,SEH_MIN_DB_API_ERR);
    }
#endif
  
  /* 
   * Look for the class and type in the event_type table. Make sure the
   * enabled flag is set.
   */
  dbReq=ssdbSendRequestTrans(semdbError,SEMdbConnect,SSDB_REQTYPE_SELECT,
				 SSDB_EVENT_TYPES_TABLE,
				 "SELECT class_id, type_id, "
				 "throttled, sehthrottle, "
				 "sehfrequency from %s "
				 "where type_id=%d and "
				 "enabled=1 and sys_id = '%llX'",
				 SSDB_EVENT_TYPES_TABLE,
				 type, system_id);
      
  if(dbReq)
  {
      num_events=ssdbGetNumRecords(semdbError,dbReq);
      /* 
       * No events found.
       */
      if(!num_events)
      {
	  ssdbFreeRequest(semdbError,dbReq);      
      }
  }
  else
  {
#if EVENTS_DEBUG
      if(verbose > VERB_INFO)
      {
	  fprintf(stderr,"SSDB(SEH:7) ssdbSendRequestTrans Error:");
	  seh_last_error_ssdb();
	  fprintf(stderr,
		  "SELECT class_id, type_id, "
		  "throttled, sehthrottle, "
		  "sehfrequency from %s "
		  "where type_id=%d and "
		  "enabled=1 and sys_id = '%llX'",
		  SSDB_EVENT_TYPES_TABLE,
		  type, system_id);
      }
      num_errors_in_seh_ssdb++;
#endif
#if !SSDB_CONNECT_ONCE
      ssdbCloseConnection(semdbError,SEMdbConnect);
#endif
      return err;
  }
  
  /* 
   * No record was found for the event we are looking for.
   */
  if(!num_events)
    {
#if !SSDB_CONNECT_ONCE
      ssdbCloseConnection(semdbError,SEMdbConnect);
#endif
      return err;
    }

  /* 
   * Allocate memory for the event.
   */
  if(!(*PPev=sem_mem_alloc_temp(sizeof(struct event_private))))
    {
	  ssdbFreeRequest(semdbError,dbReq);
#if !SSDB_CONNECT_ONCE
	  ssdbCloseConnection(semdbError,SEMdbConnect);
#endif
	  return SEH_ERROR(SEH_MAJ_INIT_ERR,SEH_MIN_ALLOC_MEM);
    }

#if !KLIB_LIBRARY
  memset(*PPev,0,sizeof(struct event_private));
#endif

  /* 
   * Read in the contents of the event from the request we just did.
   * No wildcard entries are tolerated in the event_type table.
   * These assignments below are left alone if we decide at any time
   * to move back to having wildcard entries in the event_type table.
   * 
   */
  (*PPev)->event_class = class;
  (*PPev)->event_type  = type;
  (*PPev)->sys_id  = system_id;  
  if(seh_init_event_ssdb(*PPev,dbReq))
    {
      sem_mem_free(*PPev);
      ssdbFreeRequest(semdbError,dbReq);
#if !SSDB_CONNECT_ONCE
      ssdbCloseConnection(semdbError,SEMdbConnect);
#endif
      return err;
    }

  /* 
   * Don't need the ssdb api data structures around anymore.
   * Done with ssdb initialization for this event.
   */
  ssdbFreeRequest(semdbError,dbReq);
#if !SSDB_CONNECT_ONCE
  ssdbCloseConnection(semdbError,SEMdbConnect);
#endif

  /*
   * Do other initialization for the event.
   * -- allocate data structures etc.
   * -- init counters.
   */
  if(seh_init_each_event(*PPev))
    {
      seh_free_event(*PPev,NULL);
      seh_syslog_event(*PPev,"SEH_EVENT: Failed to initialize event "
		       "Class=%d, Type=%d.\n",
		       class,type);

      return err;
    }

  /*
   * This call to seh_add_event should happen before the dsm_init_event_ssdb.
   */
  if(seh_add_event(*PPev))
    {
      seh_delete_event(*PPev);
      return err;
    }

  /*
   * Initialize the rules part of the event.
   */
  if(dsm_init_event_ssdb(*PPev))
    {
      seh_delete_event(*PPev);
      return err;
    }


#if EVENTS_DEBUG  
  num_init_events_seh_ssdb++;
#endif

  return 0;
}


/* 
 * Setup the global variables from the ssdb.
 * At present these are,
 *     o global_logging_flag
 *     o global_throttling_flag
 */
__uint64_t seh_setup_globals_ssdb()
{
  char sql_buf[SSDB_SQL_DEFAULT_SIZE];
  ssdb_Request_Handle dbReq;
#if !SSDB_CONNECT_ONCE
  ssdb_Connection_Handle SEMdbConnect;
#endif
  int Itemp;
  char *Pchar=NULL;

#if !SSDB_CONNECT_ONCE
  SEMdbConnect=
    ssdbOpenConnection(semdbError,dbClient,NULL,SSS_SSDB_DBNAME,
		       SSDB_CONFLG_RECONNECT|SSDB_CONFLG_REPTCONNECT);
  if(!SEMdbConnect)
    {
#if EVENTS_DEBUG
      if(verbose > VERB_INFO)
	{
	  fprintf(stderr,"SSDB(SEH:8) Open Connection Error:");
	  seh_last_error_ssdb();
	}
      num_errors_in_seh_ssdb++;
#endif
      return SEH_ERROR(SEH_MAJ_SSDB_ERR,SEH_MIN_DB_API_ERR);
    }
#endif
  
  snprintf(sql_buf,SSDB_SQL_DEFAULT_SIZE,"SELECT option_default "
	   "from %s where tool_name=\"%s\" and tool_option=\"%s\"",
	   SSDB_TOOL_TABLE,TOOLNAME,GLOBAL_LOGGING);

  sss_block_alarm();
  dbReq=ssdbSendRequestTrans(semdbError,SEMdbConnect,SSDB_REQTYPE_SELECT,
			     SSDB_TOOL_TABLE,sql_buf);
  sss_unblock_alarm();

#if EVENTS_DEBUG
  if(verbose > VERB_DEBUG_2)
    fprintf(stderr,"sql:?%s?\n",sql_buf);
#endif

  if(dbReq)
    {
      if(seh_read_string_ssdb(semdbError,dbReq,0,&Pchar,&Itemp) || !Pchar)
	{
	  /* 
	   * failed
	   */
	  ssdbFreeRequest(semdbError,dbReq);
#if !SSDB_CONNECT_ONCE
	  ssdbCloseConnection(semdbError,SEMdbConnect);
#endif
#if EVENTS_DEBUG
	  num_errors_in_seh_ssdb++;
#endif
	  return SEH_ERROR(SEH_MAJ_SSDB_ERR,SEH_MIN_DB_MISS_FLD);
	}
      CLEAR_ERRNO();
      Itemp=(int)strtol(Pchar,NULL,10);
      if(errno)
	{
	  sem_mem_free(Pchar);
	  ssdbFreeRequest(semdbError,dbReq);
#if !SSDB_CONNECT_ONCE
	  ssdbCloseConnection(semdbError,SEMdbConnect);
#endif
#if EVENTS_DEBUG
	  num_errors_in_seh_ssdb++;
#endif
	  return SEH_ERROR(SEH_MAJ_SSDB_ERR,SEH_MIN_DB_MISS_FLD);
	}
      if(Itemp)
	seh_set_logging(TRUE);
      else
	seh_set_logging(FALSE);
      sem_mem_free(Pchar);
    }
  else
    {
#if EVENTS_DEBUG
      if(verbose > VERB_INFO)
	{
	  fprintf(stderr,"SSDB(SEH:9) ssdbSendRequestTrans Error:");
	  seh_last_error_ssdb();
	}
      num_errors_in_seh_ssdb++;
#endif
#if !SSDB_CONNECT_ONCE
      ssdbCloseConnection(semdbError,SEMdbConnect);
#endif
      return SEH_ERROR(SEH_MAJ_SSDB_ERR,SEH_MIN_DB_MISS_FLD);
    }

  Pchar=NULL;
  snprintf(sql_buf,SSDB_SQL_DEFAULT_SIZE,"SELECT option_default "
	   "from %s where tool_name=\"%s\" and tool_option=\"%s\"",
	   SSDB_TOOL_TABLE,TOOLNAME,GLOBAL_THROTTLING);

  sss_block_alarm();
  dbReq=ssdbSendRequestTrans(semdbError,SEMdbConnect,SSDB_REQTYPE_SELECT,
			     SSDB_TOOL_TABLE,sql_buf);
  sss_unblock_alarm();

#if EVENTS_DEBUG
  if(verbose > VERB_DEBUG_2)
    fprintf(stderr,"sql:?%s?\n",sql_buf);
#endif

  if(dbReq)
    {
      if(seh_read_string_ssdb(semdbError,dbReq,0,&Pchar,&Itemp) || !Pchar)
	{
	  /* 
	   * failed
	   */
	  ssdbFreeRequest(semdbError,dbReq);
#if !SSDB_CONNECT_ONCE
	  ssdbCloseConnection(semdbError,SEMdbConnect);
#endif
#if EVENTS_DEBUG
	  num_errors_in_seh_ssdb++;
#endif
	  return SEH_ERROR(SEH_MAJ_SSDB_ERR,SEH_MIN_DB_MISS_FLD);
	}
      CLEAR_ERRNO();
      Itemp=(int)strtol(Pchar,NULL,10);
      if(errno)
	{
	  sem_mem_free(Pchar);
	  ssdbFreeRequest(semdbError,dbReq);
#if !SSDB_CONNECT_ONCE
	  ssdbCloseConnection(semdbError,SEMdbConnect);
#endif
#if EVENTS_DEBUG
	  num_errors_in_seh_ssdb++;
#endif
	  return SEH_ERROR(SEH_MAJ_SSDB_ERR,SEH_MIN_DB_MISS_FLD);
	}
      if(Itemp)
	seh_set_throttling(TRUE);
      else
	seh_set_throttling(FALSE);
      sem_mem_free(Pchar);
    }
  else
    {
#if EVENTS_DEBUG
      if(verbose > VERB_INFO)
	{
	  fprintf(stderr,"SSDB(SEH:10) ssdbSendRequestTrans Error:");
	  seh_last_error_ssdb();
	}
      num_errors_in_seh_ssdb++;
#endif
#if !SSDB_CONNECT_ONCE
      ssdbCloseConnection(semdbError,SEMdbConnect);
#endif
      return SEH_ERROR(SEH_MAJ_SSDB_ERR,SEH_MIN_DB_MISS_FLD);
    }

#if EVENTS_DEBUG
  num_init_globals_seh_ssdb++;
#endif
  return 0;
}

/* 
 * Clean up everything which is persistent with this interface. This is 
 * usually called before exiting.
 */
void seh_clean_ssdb()
{
  int i=0;

#ifdef SGM
  checkin_sgm_license();
#endif
#if SSDB_CONNECT_ONCE
  ssdbCloseConnection(semdbError,SEMdbConnect);
#endif
  ssdbDeleteClient(semdbError,dbClient);
  ssdbDeleteErrorHandle(semdbError);
  free_sysinfolist();		/* free sgm related data structures. */
}

#ifdef SGM
/*
 * Degrade SGM to SEM
 */

__uint64_t degrade_sgm(SGMLicense_t *lic)
{

    __uint64_t err = SEH_ERROR(SEH_MAJ_INIT_ERR, SEH_MIN_INIT_DB);
#if EVENTS_DEBUG
    char buffer[MAX_MESSAGE_LENGTH];
#endif 

    if ( lic == NULL ) return(1);

    lic->current_mode = RUN_AS_SEM;
    lic->requested_mode = RUN_AS_SEM;
    lic->flag |= SGM_SSDBUPDT_REQD;
    set_sgm_config_details(lic);

#if EVENTS_DEBUG
    if ( verbose > VERB_INFO ) 
        fprintf(stderr, "Degraded SGM to SEM\n");
#endif

    checkin_sgm_license();


    /* Reread information about sys_ids */

    if ( err=seh_read_sysinfo(lic))
	return err;


#if EVENTS_DEBUG
    print_sysinfolist(buffer);
    if ( verbose > VERB_INFO ) 
	fprintf(stderr, "%s", buffer);
#endif

    return 0;

}

/*
 * Upgrade SEM to SGM
 */

__uint64_t upgrade_sgm(SGMLicense_t *lic)
{

    __uint64_t err = SEH_ERROR(SEH_MAJ_INIT_ERR, SEH_MIN_INIT_DB);
#if EVENTS_DEBUG
    char buffer[MAX_MESSAGE_LENGTH];
#endif 

    if ( lic == NULL ) return 1;

    if ( check_sgm_license(&license) ) 
    {
#if EVENTS_DEBUG
        if ( verbose > VERB_INFO ) {
	    fprintf(stderr, "License Check succesful. Upgrading to SGM\n");
	}
#endif
	lic->requested_mode = RUN_AS_SGM;
	lic->current_mode = RUN_AS_SGM;
        lic->flag |= SGM_SSDBUPDT_REQD;

        if ( err=seh_read_sysinfo(lic))
	    return err;

	set_sgm_config_details(lic);

        /* Install the license timer */
    }

#if EVENTS_DEBUG
    print_sysinfolist(buffer);
    if ( verbose > VERB_INFO ) 
	fprintf(stderr, "%s", buffer);
#endif

    return 0;
}

/*
 * Sets up the configuration details in sss_config table of SEM/SGM
 */

__uint64_t set_sgm_config_details(SGMLicense_t *lic)
{
    ssdb_Connection_Handle Connection;
    ssdb_Request_Handle    Request;
    char                   sqlstr[SSDB_SQL_DEFAULT_SIZE];

    if ( !(lic->flag & SGM_SSDBUPDT_REQD) ) return(-1);

    if ( !semdbError || !dbClient ) return (-1);

    if ( !(Connection = ssdbOpenConnection(semdbError, dbClient, NULL, 
					   SSS_SSDB_DBNAME, 
					   SSDB_CONFLG_RECONNECT|
					   SSDB_CONFLG_REPTCONNECT))) {
	return(SEH_ERROR(SEH_MAJ_INIT_ERR,SEH_MIN_INIT_DB));
    }


    sprintf(sqlstr, "UPDATE %s SET %s=%d, %s=%d, %s=%d",
	    SGM_CONF_TABLE, SGM_REQ_MODE, lic->requested_mode, 
	    SGM_SSDB_STATUS, lic->status, SGM_MAX_SUBSCR,
	    lic->max_subscriptions);

    if ( !(Request = ssdbSendRequest(semdbError, Connection, 
				     SSDB_REQTYPE_UPDATE,sqlstr)) ) {
	ssdbCloseConnection(semdbError, Connection);
	return(SEH_ERROR(SEH_MAJ_SSDB_ERR, SEH_MIN_DB_XMIT));
    } else {
	ssdbFreeRequest(semdbError, Request);
    }

    ssdbCloseConnection(semdbError, Connection);
    lic->flag &= ~SGM_SSDBUPDT_REQD;
    return(0);
}

/* 
 * Reads the SEM/SGM Configuration details from sss_config table of
 * SSDB
 */

static __uint64_t get_sgm_config_details(ssdb_Error_Handle Error,
				  ssdb_Connection_Handle Connection,
				  SGMLicense_t  *lic)
{
    ssdb_Request_Handle    Request;
    ssdb_Request_Handle    Request1;
    int                    NumRows = 0, NumCols = 0;
    char                   sqlstr[SSDB_SQL_DEFAULT_SIZE];
    const char             **vector;

    if ( !lic || !Error || !Connection ) 
	return(SEH_ERROR(SEH_MAJ_INIT_ERR, SEH_MIN_INVALID_ARG));

    sprintf(sqlstr, "select %s,%s,%s,%s from %s", SGM_REQ_MODE, SGM_SSDB_STATUS,
	    SGM_MAX_SUBSCR, SGM_CURR_SUBSCR, SGM_CONF_TABLE);

    if ( !(Request = ssdbSendRequest(Error, Connection, SSDB_REQTYPE_SELECT,
				     sqlstr)) ) {
	return(SEH_ERROR(SEH_MAJ_SSDB_ERR, SEH_MIN_DB_XMIT));
    }

    if ( (NumRows = ssdbGetNumRecords(Error, Request)) != 0 ) {
	if ( (NumCols = ssdbGetNumColumns(Error, Request)) == 0 ) {
	    ssdbFreeRequest(Error, Request);
	    return(SEH_ERROR(SEH_MAJ_SSDB_ERR, SEH_MIN_DB_RCV));
	}

	ssdbRequestSeek(Error, Request, 0, 0 );
	vector = ssdbGetRow(Error, Request);

	if ( vector ) {
	    lic->requested_mode = atoi(vector[0]);
	    lic->status         = atoi(vector[1]);
	    lic->max_subscriptions = atoi(vector[2]);
	    lic->curr_subscriptions= atoi(vector[3]);
	}

    } else {
	sprintf(sqlstr, "insert into %s values(%d, %d, %d, %d)", SGM_CONF_TABLE,
		lic->requested_mode, lic->status, lic->max_subscriptions, 
		lic->curr_subscriptions);

        if ( !(Request1 = ssdbSendRequest(Error, Connection, 
					  SSDB_REQTYPE_INSERT, 
					  sqlstr)) ) {
	    ssdbFreeRequest(Error, Request);
	    return(SEH_ERROR(SEH_MAJ_SSDB_ERR, SEH_MIN_DB_XMIT));
        } else {
	    ssdbFreeRequest(Error, Request1);
	}
    }

    ssdbFreeRequest(Error, Request);
    return(0);
}

void verify_license(SGMLicense_t *lic, int local)
{
    time_t    mytime = time(0);

    if ( lic->current_mode != RUN_AS_SGM ) return;

    if ( lic->license_expiry_date < mytime ) {
        lock_ruleevent();
	degrade_sgm(lic);
        unlock_ruleevent();
	return;
    } 

    if ( lic->status == 0  && local != LOCAL ) {
	lic->status = 1;
	lic->flag |=SGM_SSDBUPDT_REQD;
	set_sgm_config_details(lic);
    }
}
#endif
