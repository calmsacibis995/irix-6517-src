#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <limits.h>
#include <time.h>

#include "rgPluginAPI.h"
#include "sscHTMLGen.h"
#include "amrserv.h"
#include "ssdbapi.h"
#include "ssdberr.h"
#include "amrerrors.h"
#include "amstructs.h"

#include <lmsgi.h>
#include "sgmdefs.h"

LM_CODE(vendorcode, ENCRYPTION_CODE_1, ENCRYPTION_CODE_2,
	VENDOR_KEY1, VENDOR_KEY2, VENDOR_KEY3,
	VENDOR_KEY4, VENDOR_KEY5);

/* --------------------------------------------------------------------------- */
#ifdef __TIME__
#ifdef __DATE__
#define INCLUDE_TIME_DATE_STRINGS 1
#endif
#endif
/* --------------------------------------------------------------------------- */
#define MYVERSION_MAJOR    1 /* Report Generator PlugIn Major Version number */
#define MYVERSION_MINOR    0 /* Report Generator PlugIn Minor Version number */
/* --------------------------------------------------------------------------- */

#define REPORT_WIDTH	        79
#define START                   0
#define END                     1
#define QUERY_MAX_SZ		2048

/* define column index values for AMR data in a row as we read from DB*/
#define EVENT_TYPE		0
#define E_EVENT_START		1
#define EVENT_TIME		2
#define AMR_LASTTICK		3
#define AMR_PREVSTART		4
#define AMR_START		5
#define AMR_INTERVAL		6
#define AMR_BOUNDS		7
#define AMR_METRIC		8
#define AMR_FLAG		9
#define AMR_SUMMARY		10
#define AMR_SYS_ID		11
#define AMR_SYS_NAME		12

#define EVENTS_PER_PAGE         10
#define EVENTS_PER_SET          100

static amrEvDesc_t EventDescriptions[] = {
  {TOINDX(MU_UND_NC),        "Controlled shutdown (unknown)"},
  {TOINDX(MU_UND_TOUT),      "Controlled shutdown (timeout)"},
  {TOINDX(MU_UND_UNK),       "Controlled shutdown (unknown)"},
  {TOINDX(MU_UND_ADM),       "Controlled shutdown (1)"},
  {TOINDX(MU_HW_UPGRD),      "Controlled shutdown (2)"},
  {TOINDX(MU_SW_UPGRD),      "Controlled shutdown (3)"},
  {TOINDX(MU_HW_FIX),        "Controlled shutdown (4)"},
  {TOINDX(MU_SW_PATCH),      "Controlled shutdown (5)"},
  {TOINDX(MU_SW_FIX),        "Controlled shutdown (6)"},
  {TOINDX(SU_UND_TOUT),      "Singleuser shutdown (unknown)"},
  {TOINDX(SU_UND_NC),        "Singleuser shutdown (unknown)"},
  {TOINDX(SU_UND_ADM),       "Singleuser shutdown (1)"},
  {TOINDX(SU_HW_UPGRD),      "Singleuser shutdown (2)"},
  {TOINDX(SU_SW_UPGRD),      "Singleuser shutdown (3)"},
  {TOINDX(SU_HW_FIX),        "Singleuser shutdown (4)"},
  {TOINDX(SU_SW_PATCH),      "Singleuser shutdown (5)"},
  {TOINDX(SU_SW_FIX),        "Singleuser shutdown (6)"},
  {TOINDX(UND_PANIC),        "Panic"},
  {TOINDX(HW_PANIC),         "Panic (H/W)"},
  {TOINDX(UND_INTR),         "Interrupt"},
  {TOINDX(UND_SYSOFF),       "System off"},
  {TOINDX(UND_PWRFAIL),      "Power failure"},
  {TOINDX(SW_PANIC),         "Panic (S/W)"},
  {TOINDX(UND_NMI),          "NMI"},
  {TOINDX(UND_RESET),        "System reset"},
  {TOINDX(UND_PWRCYCLE),     "Power cycle"},
  {TOINDX(DE_REGISTER),      "Deregistration"},
  {TOINDX(REGISTER),         "Registration"},
  {TOINDX(LIVE_NOERR),       "No error"},
  {TOINDX(LIVE_HWERR),       "Hardware error"},
  {TOINDX(LIVE_SWERR),       "Software error"},
  {TOINDX(STATUS),           "Status report"},
  {TOINDX(ID_CORR),          "System ID change"},
  {TOINDX(LIVE_SYSEVNT),     "Live event"},
  {TOINDX(NOTANEVENT),       "Unknown reasons"}
};

static amrEvDesc_t shortEventDescriptions[] = {
  {TOINDX(MU_UND_NC),        "Controlled"},
  {TOINDX(MU_UND_TOUT),      "Controlled"},
  {TOINDX(MU_UND_UNK),       "Controlled"},
  {TOINDX(MU_UND_ADM),       "Controlled"},
  {TOINDX(MU_HW_UPGRD),      "Controlled"},
  {TOINDX(MU_SW_UPGRD),      "Controlled"},
  {TOINDX(MU_HW_FIX),        "Controlled"},
  {TOINDX(MU_SW_PATCH),      "Controlled"},
  {TOINDX(MU_SW_FIX),        "Controlled"},
  {TOINDX(SU_UND_TOUT),      "Singleuser"},
  {TOINDX(SU_UND_NC),        "Singleuser"},
  {TOINDX(SU_UND_ADM),       "Singleuser"},
  {TOINDX(SU_HW_UPGRD),      "Singleuser"},
  {TOINDX(SU_SW_UPGRD),      "Singleuser"},
  {TOINDX(SU_HW_FIX),        "Singleuser"},
  {TOINDX(SU_SW_PATCH),      "Singleuser"},
  {TOINDX(SU_SW_FIX),        "Singleuser"},
  {TOINDX(UND_PANIC),        "Panic"},
  {TOINDX(HW_PANIC),         "Panic (H/W)"},
  {TOINDX(UND_INTR),         "Interrupt"},
  {TOINDX(UND_SYSOFF),       "System off"},
  {TOINDX(UND_PWRFAIL),      "Power failure"},
  {TOINDX(SW_PANIC),         "Panic (S/W)"},
  {TOINDX(UND_NMI),          "NMI"},
  {TOINDX(UND_RESET),        "System reset"},
  {TOINDX(UND_PWRCYCLE),     "Power cycle"},
  {TOINDX(DE_REGISTER),      "Deregistration"},
  {TOINDX(REGISTER),         "Registration"},
  {TOINDX(LIVE_NOERR),       "No error"},
  {TOINDX(LIVE_HWERR),       "Hardware error"},
  {TOINDX(LIVE_SWERR),       "Software error"},
  {TOINDX(STATUS),           "Status report"},
  {TOINDX(ID_CORR),          "System ID change"},
  {TOINDX(LIVE_SYSEVNT),     "Live event"},
  {TOINDX(NOTANEVENT),       "Unknown reasons"}
};

/*--------------------------------------------------------------------*/
/*		Global vars					      */
/*--------------------------------------------------------------------*/
static int    myVersion = 504;
static char   myTitle[] = "AMR Report Generator server (February 1999)";
static BOOL   amr_init_done = FALSE;
static int    amr_data_in_mem = 0;
static time_t now;
static int    count_this_epoch = 1;
static char   *db = "ssdb";       /* name */
static int    license = FALSE;

/* ----------------------------------------------------------------------- */
static const char myLogo[]            = "AMR Report Generator server";
static const char szVersion[]         = "Version";
static const char szTitle[]           = "Title";
static const char szThreadSafe[]      = "ThreadSafe";
static const char szUnloadable[]      = "Unloadable";
static const char szUnloadTime[]      = "UnloadTime";
static const char szAcceptRawCmdString[] = "AcceptRawCmdString";
static const char szAvail_sel[]       = "avail_sel";
static const char szAvail_start[]     = "av_start_time";
static const char szAvail_end[]       = "av_end_time";
static const char szAvail_sysid[]     = "sys_id";
static const char szStartEvent[]      = "startEvent";

static char szServerNameErrorPrefix[] = "AMR Report Generator Server Error: %s";
static char szVersionStr[64];

static pthread_mutex_t seshFreeListmutex;
static const char szUnloadTimeValue[] = "120"; /* Unload time for this plug in (sec.) */
static const char szThreadSafeValue[] = "0";   /* "Thread Safe" flag value - this plugin is thread safe */
static const char szUnloadableValue[] = "1";   /* "Unloadable" flag value - RG core might unload this plugin from memory */
static const char szAcceptRawCmdStringValue[] = "0"; /* Do not accept raw strings. */

static int volatile mutex_inited      = 0;   /* seshFreeListmutex mutex inited flag */
static session_t *sesFreeList         = 0;   /* Session free list */

/*--------------------------------------------------------------------*/
/*		Forward function declarations	       		      */
/*--------------------------------------------------------------------*/

static int process_local_records(session_t *, time_t, time_t, 
				 char *, char *,
				 ssdb_Request_Handle,int flag);
static int process_group_records(session_t *, time_t, time_t,
				 ssdb_Request_Handle,int flag);
static void collect_host_stats(hostinfo_t *);
static void collect_group_stats(session_t *);
static void gather_host_stat_counts(hostinfo_t *);
static void gather_aggregate_stats(session_t *,int , char **);
static void generate_H1(session_t *, int argc,char **argv);
static void generate_H2(session_t *, int argc,char **argv);
static void generate_H3(session_t *, int evnt, int argc,char **argv);
static void generate_S2(session_t *, int argc,char **argv);
static void generate_S1(session_t *, int argc,char **argv);
static char *pretty_minutes(int m,char *buf);
static void drop_temp_table(sscErrorHandle hError,
			    ssdb_Connection_Handle connection,
			    ssdb_Error_Handle error_handle);

static long maketime(const char *dateval1,int startend)
{
    time_t timevalue;
    struct tm timestruct;
    struct tm *timeptr;
    char dateval[40];
    int i;
    char *p;
    strcpy (dateval,dateval1);
    p = strtok(dateval,"/");
    i = atoi(p);
    i = i - 1;
    timestruct.tm_mon = i;
    p = strtok(NULL,"/");
    timestruct.tm_mday = atoi(p);
    p = strtok(NULL,"/");
    i = atoi(p);
    i = i - 1900;
    timestruct.tm_year = i;
    if (!startend)
    {
	timestruct.tm_sec = 00;
	timestruct.tm_min = 00;
	timestruct.tm_hour = 00;
    }
    else
    {
	timestruct.tm_sec = 60;
	timestruct.tm_min = 59;
	timestruct.tm_hour = 23;
    }
    timestruct.tm_isdst = -1;
    timevalue =  mktime(&timestruct);
    return (timevalue);
}

static int exist_sysid(char *h,int num,char **sysids)
{
    if(num<=0 || !sysids)
	return 1;

    while(num--)
    {
	if(!strcasecmp("0",sysids[num]) || 
	   !strcasecmp(h,sysids[num]))
	{
	    return 1;
	}
    }

    return 0;
}

static void FreeAggregate(aggregate_stats_t *a)
{
    free(a);
    return;
}

static void FreeHosts(hostinfo_t *h)
{
    hostinfo_t *hinfo;
    event_t    *Pev;

    while(h)
    {
	while(Pev=h->first_ev)
	{
	    if(Pev->summary)
		free(Pev->summary);
	    h->first_ev=Pev->next;
	    free(Pev);
	}
	
	if(h->hostname)
	    free(h->hostname);
	
	hinfo=h->next;
	free(h);
	h=hinfo;
    }
    return;
}

static void FreeEveryThing(session_t *scb)
{
    request_t               *sreq = NULL;
    hostinfo_t              *hinfo = NULL;
	

    if(scb->Avail_startValue)
	free(scb->Avail_startValue);
    
    if(scb->Avail_endValue)
	free(scb->Avail_endValue);
    
    scb->Avail_startValue=scb->Avail_endValue=NULL;

    if(!scb)
	return;
    
    if(ssdbIsConnectionValid(scb->dberr_h,scb->dbconn_h))
	ssdbCloseConnection(scb->dberr_h,scb->dbconn_h);
    if(ssdbIsClientValid(scb->dberr_h,scb->dbclnt_h))
	ssdbDeleteClient(scb->dberr_h,scb->dbclnt_h);
    ssdbDeleteErrorHandle(scb->dberr_h);
    scb->dbconn_h=scb->dbclnt_h=scb->dberr_h=NULL;
    
    
    if(scb->aggr_stats)
	FreeAggregate(scb->aggr_stats);

    if(scb->hosts)
	FreeHosts(scb->hosts);

    if(scb->sysids)
    {
	while(scb->num_ids)
	    free(scb->sysids[--scb->num_ids]);
	free(scb->sysids);
    }
    return;
}

static void Initialize()
{

    if (amr_init_done)  return;

    now = time(0);
    amr_init_done++;
}

static int no_license(char *feature)
{
    license = FALSE;
    return 0; 
}

static __uint64_t init_sgm_license()
{
    LM_HANDLE    *lic_handle;
    static int already_initialized=0;

    if(already_initialized)
	return 1;

    if ( license_init(&vendorcode, "sgifd", B_TRUE) < 0 )  {
	return(0);
    }
    
    /* 
     * Set the retry count; how many times we want to check for license server
     */
    
    if (license_set_attr(LM_A_RETRY_COUNT, (LM_A_VAL_TYPE)LICENSE_RETRY_COUNT))
    {
	return(0);
    }
    
    /* Register a NULL callback for everything */
    license_set_attr(LMSGI_NO_SUCH_FEATURE, (LM_A_VAL_TYPE) NULL);
    license_set_attr(LMSGI_30_DAY_WARNING, (LM_A_VAL_TYPE) NULL);
    license_set_attr(LMSGI_60_DAY_WARNING, (LM_A_VAL_TYPE) NULL);
    license_set_attr(LMSGI_60_DAY_WARNING, (LM_A_VAL_TYPE) NULL);
    license_set_attr(LM_A_USER_RECONNECT, (LM_A_VAL_TYPE) NULL ) ;
    license_set_attr(LM_A_USER_RECONNECT_DONE, (LM_A_VAL_TYPE) NULL);
    /* Register a Degraded Mode of operation for LM_A_USER_EXITCALL */

    license_set_attr(LM_A_USER_EXITCALL, (LM_A_VAL_TYPE) no_license);

    already_initialized=1;

    return 1;
}

static __uint64_t check_sgm_license()
{
    CONFIG       *conf;

    no_license(NULL);

    if(!init_sgm_license())
	return 0;

    /* Check-out License */

    if ( license_chk_out(&vendorcode, SGM_FEATURE, SGM_VERSION) ) {
	return(0);
    }
    license = TRUE;
    return(1);
}

static void checkin_sgm_license(void)
{
    license_chk_in(SGM_FEATURE,0);
    no_license(NULL);
}

static void generate_begin_background(session_t *scb, char *title,int help)
{
  if ( scb->textonly == 0 )
  {
    Body("<HTML> <HEAD> <TITLE>SGI Embedded Support Partner - Ver. 1.0</TITLE></HEAD> <BODY BGCOLOR=\"#ffffcc\" vlink=\"#333300\" link=\"#333300\">\n");
    Body("<form method=POST>\n");
    TableBegin("border=0 cellpadding=0 cellspacing=0 width=100%");
    RowBegin("");
      CellBegin("bgcolor=\"#cccc99\" width=\"15\"");
	 Body("&nbsp;&nbsp;&nbsp;&nbsp;\n");
      CellEnd();
      CellBegin("bgcolor=\"#cccc99\" ");
	   Body("<font face=\"Arial,Helvetica\">\n");
	   FormatedBody(title);
      CellEnd();
    RowEnd();
    RowBegin("");
      CellBegin("COLSPAN=2");
	   Body("&nbsp;\n");
      CellEnd();
    RowEnd();
    RowBegin("");
      CellBegin("COLSPAN=2");
	   Body("&nbsp;\n");
      CellEnd();
    RowEnd();
    RowBegin("");
      CellBegin("");
	   Body("&nbsp;\n");
      CellEnd();
      CellBegin("");
   } else {
    Body("<HTML> <HEAD> <TITLE>SGI Embedded Support Partner - Ver. 1.0</TITLE></HEAD> <BODY BGCOLOR=\"#ffffcc\">\n");
    FormatedBody("<pre>   %s</pre>",title);
    Body("<hr width=100%>\n");
    Body("<form method=POST>\n");
   }   
    return;
}

static void generate_end_background(session_t *scb)
{
  if ( scb->textonly == 0 )
  {
      CellEnd();
    RowEnd();
   TableEnd();
   Body("</form> </BODY> </HTML>\n");
  } else {
   Body("</form> </BODY> </HTML>\n");
  } 
}

static void generate_error_page(session_t *scb, char *title,char *msg)
{
  if ( scb->textonly == 0 )
  {
   Body("<HTML> <HEAD> <TITLE>SGI Embedded Support Partner - Ver. 1.0</TITLE> </HEAD> <BODY BGCOLOR=\"#ffffcc\">\n");
   TableBegin("border=0 cellpadding=0 cellspacing=0 width=100%");
    RowBegin("");
      CellBegin("bgcolor=\"#cccc99\" width=\"15\"");
	 Body("&nbsp;&nbsp;&nbsp;&nbsp;\n");
      CellEnd();
      CellBegin("bgcolor=\"#cccc99\" ");
	   Body("<font face=\"Arial,Helvetica\">\n");
	   FormatedBody(title);
      CellEnd();
    RowEnd();
    RowBegin("");
      CellBegin("COLSPAN=2");
	   Body("&nbsp;<P>&nbsp;\n");
      CellEnd();
    RowEnd();
    RowBegin("");
      CellBegin("");
	   Body("&nbsp;\n");
      CellEnd();
      CellBegin("");
       	   FormatedBody(msg);
   }
  else 
  {
       Body("<HTML> <HEAD> <TITLE>SGI Embedded Support Partner - Ver. 1.0</TITLE> </HEAD> <BODY BGCOLOR=\"#ffffcc\">\n");
       FormatedBody("<pre>   %s</pre>",title);
       Body("<hr width=100%>\n");
       FormatedBody("<p>%s",msg);
   }
   generate_end_background(scb);
  
}


static void drop_temp_table(sscErrorHandle hError,
			    ssdb_Connection_Handle connection,
			    ssdb_Error_Handle error_handle)
{
    int number_of_records,rec_sequence;
    const char **vector5,**vector6;
    ssdb_Request_Handle req_handle5,req_handle6;

    if (!ssdbUnLockTable(error_handle,connection))
	sscError(hError,"Database API Error: \"%s\"\n\n",
		 ssdbGetLastErrorString(error_handle));

    if (!(req_handle5 = ssdbSendRequest(error_handle,connection,
					SSDB_REQTYPE_SHOW,
					"show tables")))
    {
	sscError(hError,"Database API Error: \"%s\"\n\n",
		 ssdbGetLastErrorString(error_handle));
	return;
    }
    if ((number_of_records = getnumrecords(hError,error_handle,req_handle5)) 
	> 0)
    {
	for (rec_sequence=0;rec_sequence < number_of_records; rec_sequence++)
	{
	    vector5 = ssdbGetRow(error_handle,req_handle5);
	    if (vector5)
	    {
		if(!strcmp(vector5[0],AVAILTEMPTABLE))
		{
		    if (!(req_handle6 = ssdbSendRequest(error_handle,
							connection,
							SSDB_REQTYPE_DROPTABLE,
							"drop table "
							AVAILTEMPTABLE)))
		    {
			sscError(hError,"Database API Error: \"%s\"\n\n",
				 ssdbGetLastErrorString(error_handle));
			return;
		    }
		    ssdbFreeRequest(error_handle,req_handle6);
		}
	    }
	}
    }
    ssdbFreeRequest(error_handle,req_handle5);
}

static int getnumrecords (sscErrorHandle hError,ssdb_Error_Handle error_handle,
			  ssdb_Request_Handle req_handle)
{
    int number_of_records;
    
    number_of_records = ssdbGetNumRecords(error_handle,req_handle);
    
    if(ssdbGetLastErrorCode(error_handle) != SSDBERR_SUCCESS)
    {
	sscError(hError,"Database API Error: \"%s\"\n\n",
		 ssdbGetLastErrorString(error_handle));
	return 0;
    }
    return number_of_records;
}

/*------------------------------------------------------*/
/* Function:  amr_initq_db				*/		
/*							*/
/* This function initializes the target database, and	*/
/* queries it for the information regarding		*/
/* availdata table contents, count, and min,            */
/* max event_time values				*/
/*							*/
/*------------------------------------------------------*/  
static int amr_initq_db(session_t *scb,int flag)
{
    char *amr_user = NULL;
    char *amr_passwd = NULL;
    char query[2*QUERY_MAX_SZ];
    const char **db_data;
    ssdb_Request_Handle		dbreq_h;
   
    /* Need to figure out what the user/passwd values need to be */

    if(!scb->dberr_h)
	scb->dberr_h = ssdbCreateErrorHandle();
    if(!ssdbIsClientValid(scb->dberr_h,scb->dbclnt_h))
    {
	if (!(scb->dbclnt_h = 
	      ssdbNewClient(scb->dberr_h, amr_user, amr_passwd, 0)))
	{
	    scb->lastError = ssdbGetLastErrorCode(scb->dberr_h);
	    return (FAILED); 
	}
    }
    
    if(!ssdbIsConnectionValid(scb->dberr_h,scb->dbconn_h))
    {
	scb->dbconn_h = ssdbOpenConnection(scb->dberr_h,
					   scb->dbclnt_h,
					   0,		/* localhost */
					   scb->dbname,
					   0);		/* future flags */
	
	if (!scb->dbconn_h)
	{
	    scb->lastError = ssdbGetLastErrorCode(scb->dberr_h);
	    return (FAILED);
	}
    }
    
    /* Now see if this was ever a SGM DB, and if so,		*/
    /* is the SGM license valid now?				*/

    if (group_data_in_db(scb) && group_license_valid(scb))
    {
	scb->group_reports_enabled = TRUE;
    } 
    else scb->group_reports_enabled = FALSE;

    return (SUCCESS);
}
    
/*-------------------------------------------------------------	*/
/* Check if this database ever contained group level data	*/
/* If yes, return TRUE, else return FALSE			*/
/*-------------------------------------------------------------	*/
static int group_data_in_db(session_t *scb)
{
    /* When DB contains the WAS_SGM type flag somewhere	*/
    /* query for that; until then return FALSE		*/
    ssdb_Request_Handle dbreq;
    
    if(!scb)
	return FALSE;

    if(!(ssdbIsConnectionValid(scb->dberr_h,scb->dbconn_h)))
	return FALSE;

    dbreq = ssdbSendRequest(scb->dberr_h,scb->dbconn_h,
			    SSDB_REQTYPE_SELECT,
			    "select * from sss_config where status = 1");
    if(!dbreq)
	return FALSE;

    if(ssdbGetNumRecords(scb->dberr_h,dbreq) <= 0)
    {
	ssdbFreeRequest(scb->dberr_h,dbreq);
	return FALSE;
    }

    ssdbFreeRequest(scb->dberr_h,dbreq);

    return (TRUE);
}

/*-------------------------------------------------------------	*/
/* Check for a valid SGM type license on this host     		*/
/* Don't know how at this point;				*/
/*-------------------------------------------------------------	*/
static int group_license_valid(session_t *scb)
{

    if(!check_sgm_license())
	return FALSE;
    if(license == TRUE)
    {
	checkin_sgm_license();
	return (TRUE);
    }

    return (FALSE);
}

/*-------------------------------------------------------------	*/
/* This function will read the availdata table entries into	*/
/* Memory; will use the from_time and to_time entries to	*/
/* determine how much to read in.				*/
/*								*/
/*-------------------------------------------------------------	*/
static int read_db_in(sscErrorHandle hError, session_t *scb, 
		      time_t from_t, time_t to_t,int flag)
{
    
    const char			**db_data;
    char			query[2*QUERY_MAX_SZ];
    char                        archivequery[2*QUERY_MAX_SZ];
    ssdb_Request_Handle		dbreq_h,dbreq1_h;    
    int				rv;
    char			this_sysid[32];
    char			this_hname[MAXHOSTNAMELEN+1];
    char                        *title="SYSTEM INFORMATION &gt; Availability";
    const char                  **vector1;
    int                         i;
    int                         number_of_records;
    /* Start code block	*/

    /* The event records coming in need to be sorted on their	*/
    /* event_time == occurance value. We need to alias the	*/
    /* event.event_start and availdata.event_time to one name	*/
    /* then add sorted by <alias> clause to the query/		*/

    drop_temp_table(hError,scb->dbconn_h,scb->dberr_h);

    if(scb->group_reports_enabled)
    {
	if (!(dbreq_h = ssdbSendRequest(scb->dberr_h,scb->dbconn_h,
					SSDB_REQTYPE_CREATE,
					"create table "
					AVAILTEMPTABLE
					" (type_id int, event_start bigint,"
					"event_time bigint,"
					"lasttick bigint,"
					"prev_start bigint,"
					"start bigint,"
					"status_int int,"
					"bounds int,"
					"metrics smallint,"
					"flag smallint,"
					"summary varchar(255),"
					"sys_id char(16),"
					"hostname varchar(64))")))
	{
	    sscError(hError,"Database API Error: \"%s\"\n\n",
		     ssdbGetLastErrorString(scb->dberr_h));
	    return FAILED;
	}
	ssdbFreeRequest(scb->dberr_h,dbreq_h);
    }
    else
    {
	if (!(dbreq_h = ssdbSendRequest(scb->dberr_h,scb->dbconn_h,
					SSDB_REQTYPE_CREATE,
					"create table "
					AVAILTEMPTABLE
					" (type_id int, event_start bigint,"
					"event_time bigint,"
					"lasttick bigint,"
					"prev_start bigint,"
					"start bigint,"
					"status_int int,"
					"bounds int,"
					"metrics smallint,"
					"flag smallint,"
					"summary varchar(255))")))
	{
	    sscError(hError,"Database API Error: \"%s\"\n\n",
		     ssdbGetLastErrorString(scb->dberr_h));
	    return FAILED;
	}
	ssdbFreeRequest(scb->dberr_h,dbreq_h);
    }
		

    sprintf(query, BEGIN_LOCAL_SYSID_SQL_STATEMENT);
    dbreq_h = ssdbSendRequest(scb->dberr_h,
			      scb->dbconn_h,
			      SSDB_REQTYPE_SELECT,
			      query);
    if(!dbreq_h)
    {
	if(flag != AMR_INIT)
	    generate_error_page(scb,title,"No data available currently.");
	drop_temp_table(hError,scb->dbconn_h,scb->dberr_h);
	return (FAILED);
    }
    
    db_data = ssdbGetRow(scb->dberr_h, dbreq_h);
    if (!db_data){
	rv = ssdbGetLastErrorCode(scb->dberr_h);
	if (rv != 0) 
	    scb->lastError = rv;
	else
	    scb->lastError = AMR_NO_RECORDS_IN_DB;
	ssdbFreeRequest(scb->dberr_h,dbreq_h);
	if(flag != AMR_INIT)
	    generate_error_page(scb,title,"No data available currently.");
	drop_temp_table(hError,scb->dbconn_h,scb->dberr_h);	
	return (FAILED);
    }
    
    strcpy(this_sysid, db_data[0]);
    strcpy(this_hname, db_data[1]);
    
    /* Set up the global var 'local_sys_id' now */
    strcpy(scb->local_sysid, this_sysid);
    ssdbFreeRequest(scb->dberr_h,dbreq_h);

    if (!(dbreq1_h = ssdbSendRequest(scb->dberr_h,scb->dbconn_h,
				     SSDB_REQTYPE_SELECT,
				     "select dbname from archive_list")))
    {
	sscError(hError,"Database API Error: \"%s\"\n\n",
		 ssdbGetLastErrorString(scb->dberr_h));
	drop_temp_table(hError,scb->dbconn_h,scb->dberr_h);
	return FAILED;
    }
    
    if((number_of_records = getnumrecords(hError,scb->dberr_h,dbreq1_h)) > 0)
    {
	for(i=0;i<number_of_records;i++)
	{

	    vector1 = ssdbGetRow(scb->dberr_h,dbreq1_h);
	    if(!vector1)
		continue;
	    if(!(ssdbLockTable(scb->dberr_h,scb->dbconn_h,
			       AVAILTEMPTABLE 
			       ",%s.event,"
			       "%s.availdata",
			       vector1[0],vector1[0])))
	    {
		sscError(hError,"Database API Error: \"%s\"\n\n",
			 ssdbGetLastErrorString(scb->dberr_h));
		
		drop_temp_table(hError,scb->dbconn_h,scb->dberr_h);
		return FAILED;
	    }
					    
	    if (scb->group_reports_enabled)
	    {
		/* Query to read local host data only. No group level privs  */
		/* look for system table entry with active = 1 and local = 1 */
		/* sys_id remains same if hostname change occurs on the local*/
		/* system, so we should have all data related to this sys_id */
		/* in the DB						     */


		/* Get reports for all systems in DB */
		if ((from_t > 0)  && (to_t>0))
		{
		    sprintf(archivequery, 
			    BEGIN_ARCHIVE_GROUP_SQL_STATEMENT
			    
			    "and %s.availdata.event_time >= %ld and  "
			    "%s.availdata.event_time <= %ld "

			    END_ARCHIVE_GROUP_SQL_STATEMENT,
			    vector1[0],vector1[0],vector1[0],vector1[0],
			    vector1[0],vector1[0],vector1[0],vector1[0],
			    vector1[0],vector1[0],vector1[0],vector1[0],
			    vector1[0],vector1[0],vector1[0],vector1[0],
			    vector1[0],vector1[0],vector1[0],vector1[0],
			    vector1[0],from_t,vector1[0],to_t,
			    vector1[0]);
		} 
		else if (from_t>0)
		{
		    sprintf(archivequery, 
			    BEGIN_ARCHIVE_GROUP_SQL_STATEMENT
			    
			    "and %s.availdata.event_time >= %ld "
			    
			    END_ARCHIVE_GROUP_SQL_STATEMENT,
			    vector1[0],vector1[0],vector1[0],vector1[0],
			    vector1[0],vector1[0],vector1[0],vector1[0],
			    vector1[0],vector1[0],vector1[0],vector1[0],
			    vector1[0],vector1[0],vector1[0],vector1[0],
			    vector1[0],vector1[0],vector1[0],vector1[0],
			    vector1[0],from_t,
			    vector1[0]);
		} 
		else if (to_t>0)
		{
		    sprintf(archivequery, 
			    BEGIN_ARCHIVE_GROUP_SQL_STATEMENT
			    
			    "and %s.availdata.event_time <= %ld "
			    
			    END_ARCHIVE_GROUP_SQL_STATEMENT,
			    vector1[0],vector1[0],vector1[0],vector1[0],
			    vector1[0],vector1[0],vector1[0],vector1[0],
			    vector1[0],vector1[0],vector1[0],vector1[0],
			    vector1[0],vector1[0],vector1[0],vector1[0],
			    vector1[0],vector1[0],vector1[0],vector1[0],
			    vector1[0],
			    to_t,
			    vector1[0]);
		} 
		else 
		{
		    /* No time constraints */
		    sprintf(archivequery, 
			    BEGIN_ARCHIVE_GROUP_SQL_STATEMENT
			    
			    END_ARCHIVE_GROUP_SQL_STATEMENT,
			    vector1[0],vector1[0],vector1[0],vector1[0],
			    vector1[0],vector1[0],vector1[0],vector1[0],
			    vector1[0],vector1[0],vector1[0],vector1[0],
			    vector1[0],vector1[0],vector1[0],vector1[0],
			    vector1[0],vector1[0],vector1[0],vector1[0],
			    vector1[0]);
		} /* final else */
		
	    }  /* end if (scb->group_reports_enabled)  */
	    else 
	    {
		/* Query to read local host data only. No group level privs */
		/* look for system table entry with active = 1 and local = 1*/
		/* sys_id remains same if hostname change occurs on the local*/
		/* system, so we should have all data related to this sys_id */
		/* in the DB						*/
		
		/* Now set up query to get records only for this_sysid	*/
		if ((from_t > 0)  && (to_t>0))
		{
		    sprintf(archivequery, 
			    BEGIN_ARCHIVE_LOCAL_SQL_STATEMENT
			    
			    "and "
			    "%s.availdata.event_time >= %ld and "
			    "%s.availdata.event_time <= %ld)",
			    vector1[0],vector1[0],vector1[0],vector1[0],
			    vector1[0],vector1[0],vector1[0],vector1[0],
			    vector1[0],vector1[0],vector1[0],vector1[0],
			    vector1[0],vector1[0],this_sysid, 
			    vector1[0],vector1[0],
			    vector1[0],from_t, 
			    vector1[0],to_t);
		} 
		else if (from_t>0)
		{
		    sprintf(archivequery, 
			    BEGIN_ARCHIVE_LOCAL_SQL_STATEMENT
			    
			    "and "
			    "%s.availdata.event_time >= %ld)",
			    vector1[0],vector1[0],vector1[0],vector1[0],
			    vector1[0],vector1[0],vector1[0],vector1[0],
			    vector1[0],vector1[0],vector1[0],vector1[0],
			    vector1[0],vector1[0],this_sysid, 
			    vector1[0],vector1[0],
			    vector1[0],from_t);
		} 
		else if (to_t>0)
		{
		    sprintf(archivequery, 
			    BEGIN_ARCHIVE_LOCAL_SQL_STATEMENT
			    
			    "and "
			    "%s.availdata.event_time <= %ld)",
			    vector1[0],vector1[0],vector1[0],vector1[0],
			    vector1[0],vector1[0],vector1[0],vector1[0],
			    vector1[0],vector1[0],vector1[0],vector1[0],
			    vector1[0],vector1[0],this_sysid, 
			    vector1[0],vector1[0],
			    vector1[0],to_t);
		} 
		else 
		{
		    /* No time constraints */
		    sprintf(archivequery, 
			    BEGIN_ARCHIVE_LOCAL_SQL_STATEMENT
			    
			    ")"
			    ,
			    vector1[0],vector1[0],vector1[0],vector1[0],
			    vector1[0],vector1[0],vector1[0],vector1[0],
			    vector1[0],vector1[0],vector1[0],vector1[0],
			    vector1[0],vector1[0],this_sysid,
			    vector1[0],vector1[0]);
		} /* final else of query setup */
		
	    } /* end query to read local data only block */
	    
	    /* send the query */
	    dbreq_h = ssdbSendRequest(scb->dberr_h,
				      scb->dbconn_h,
				      SSDB_REQTYPE_INSERT,
				      archivequery);

	    if(dbreq_h)
		ssdbFreeRequest(scb->dberr_h,dbreq_h);
	    if(!ssdbUnLockTable(scb->dberr_h,scb->dbconn_h))
	    {
		sscError(hError,"Database API Error: \"%s\"\n\n",
			 ssdbGetLastErrorString(scb->dberr_h));
		drop_temp_table(hError,scb->dbconn_h,scb->dberr_h);
		return FAILED;
	    }
	}
    }

    ssdbFreeRequest(scb->dberr_h,dbreq1_h);

    if (scb->group_reports_enabled)
    {
	if(!(ssdbLockTable(scb->dberr_h,scb->dbconn_h,
			   AVAILTEMPTABLE
			   ",system,event,"
			   "availdata")))
	{
	    sscError(hError,"Database API Error: \"%s\"\n\n",
		     ssdbGetLastErrorString(scb->dberr_h));
	    drop_temp_table(hError,scb->dbconn_h,scb->dberr_h);
	    return FAILED;
	}
	/* Query to read local host data only. No group level privs  */
	/* look for system table entry with active = 1 and local = 1 */
	/* sys_id remains same if hostname change occurs on the local*/
	/* system, so we should have all data related to this sys_id */
	/* in the DB						     */
	
	
	/* Get reports for all systems in DB */
	if ((from_t > 0)  && (to_t>0))
	{
	    sprintf(query, 
		    BEGIN_GROUP_SQL_STATEMENT
		    
		    "and availdata.event_time >= %ld and  "
		    "availdata.event_time <= %ld "
		    
		    END_GROUP_SQL_STATEMENT,
		    from_t, to_t); 
		    
	} 
	else if (from_t>0)
	{
	    sprintf(query, 
		    BEGIN_GROUP_SQL_STATEMENT
		    
		    "and availdata.event_time >= %ld "
		    
		    END_GROUP_SQL_STATEMENT,
		    from_t);
	    
	} 
	else if (to_t>0)
	{
	    sprintf(query, 
		    BEGIN_GROUP_SQL_STATEMENT
		    
		    "and availdata.event_time <= %ld "
		    
		    END_GROUP_SQL_STATEMENT,
		    to_t);
		    
	} 
	else 
	{
	    /* No time constraints */
	    sprintf(query, 
		    BEGIN_GROUP_SQL_STATEMENT
		    
		    END_GROUP_SQL_STATEMENT
		    );
	    
	} /* final else */
	
    }  /* end if (scb->group_reports_enabled)  */
    else 
    {
	if(!(ssdbLockTable(scb->dberr_h,scb->dbconn_h,
			   AVAILTEMPTABLE
			   ",event,"
			   "availdata")))
	{
	    sscError(hError,"Database API Error: \"%s\"\n\n",
		     ssdbGetLastErrorString(scb->dberr_h));
	    drop_temp_table(hError,scb->dbconn_h,scb->dberr_h);
	    return FAILED;
	}

	/* Query to read local host data only. No group level privs */
	/* look for system table entry with active = 1 and local = 1*/
	/* sys_id remains same if hostname change occurs on the local*/
	/* system, so we should have all data related to this sys_id */
	/* in the DB						*/
	/* Now set up query to get records only for this_sysid	*/
	if ((from_t > 0)  && (to_t>0))
	{
	    sprintf(query, 
		    BEGIN_LOCAL_SQL_STATEMENT
		    
		    "and "
		    "availdata.event_time >= %ld and "
		    "availdata.event_time <= %ld)",
		    this_sysid, from_t, to_t);
	    
	} 
	else if (from_t>0)
	{
	    sprintf(query, 
		    BEGIN_LOCAL_SQL_STATEMENT
		    
		    "and "
		    "availdata.event_time >= %ld)",
		    this_sysid,  from_t);
	    
	} 
	else if (to_t>0)
	{
	    sprintf(query, 
		    BEGIN_LOCAL_SQL_STATEMENT
		    
		    "and "
		    "availdata.event_time <= %ld)",
		    this_sysid, to_t);
	    
	} 
	else 
	{
	    /* No time constraints */
	    sprintf(query, 
		    BEGIN_LOCAL_SQL_STATEMENT
		    
		    ")"
		    ,
		    this_sysid);
		    
	} /* final else of query setup */
	
    } /* end query to read local data only block */

    dbreq_h = ssdbSendRequest(scb->dberr_h,
			      scb->dbconn_h,
			      SSDB_REQTYPE_INSERT,
			      query);
    if(dbreq_h)
	ssdbFreeRequest(scb->dberr_h,dbreq_h);
    if (!(dbreq_h = ssdbSendRequest(scb->dberr_h,scb->dbconn_h,
				    SSDB_REQTYPE_SELECT,
				    "select * from " 
				    AVAILTEMPTABLE
				    " order by event_time")))
    {
	sscError(hError,"Database API Error: \"%s\"\n\n",
		 ssdbGetLastErrorString(scb->dberr_h));

	if(!ssdbUnLockTable(scb->dberr_h,scb->dbconn_h))
	{
	    sscError(hError,"Database API Error: \"%s\"\n\n",
		     ssdbGetLastErrorString(scb->dberr_h));
	    drop_temp_table(hError,scb->dbconn_h,scb->dberr_h);	
	    return FAILED;
	}

	drop_temp_table(hError,scb->dbconn_h,scb->dberr_h);
	return FAILED;
    }
    

    if (!dbreq_h)
    {
	char *title="SYSTEM INFORMATION &gt; Availability";

	rv= ssdbGetLastErrorCode(scb->dberr_h);
	scb->lastError = rv;
	if(flag != AMR_INIT)
	    generate_error_page(scb,title,"No data available currently.");

	if(!ssdbUnLockTable(scb->dberr_h,scb->dbconn_h))
	{
	    sscError(hError,"Database API Error: \"%s\"\n\n",
		     ssdbGetLastErrorString(scb->dberr_h));
	    drop_temp_table(hError,scb->dbconn_h,scb->dberr_h);	
	    return FAILED;
	}

	drop_temp_table(hError,scb->dbconn_h,scb->dberr_h);	
	return (rv);
    }

    /* get records information where event_time is positive only */
    strcpy(query, 
	   "select count(*), min(event_time), max(event_time) from " 
	   AVAILTEMPTABLE);
    dbreq1_h = ssdbSendRequest(scb->dberr_h,
			       scb->dbconn_h,
			       SSDB_REQTYPE_SELECT,
			       query);
    
    if(dbreq1_h)
    {

	db_data = ssdbGetRow(scb->dberr_h, dbreq1_h);
	
	if (!db_data)
	{
	    char *title="SYSTEM INFORMATION &gt; Availability";
	    
	    scb->lastError = ssdbGetLastErrorCode(scb->dberr_h);
	    if(flag != AMR_INIT)
		generate_error_page(scb, title,"No data available currently.");
	    
	    if(!ssdbUnLockTable(scb->dberr_h,scb->dbconn_h))
	    {
		sscError(hError,"Database API Error: \"%s\"\n\n",
			 ssdbGetLastErrorString(scb->dberr_h));
		drop_temp_table(hError,scb->dbconn_h,scb->dberr_h);	
		return FAILED;
	    }
	    
	    drop_temp_table(hError,scb->dbconn_h,scb->dberr_h);	
	    return (FAILED);
	}
	
	/*  *db_data[0] has the first field of the row returned,	*/
	/* and is count in this case					*/
	/*								*/
	scb->record_cnt = atoi(db_data[0]);
	scb->from_time  = atoi(db_data[1]);
	scb->to_time    = atoi(db_data[2]);
	
	ssdbFreeRequest(scb->dberr_h,dbreq1_h);
    }
    
    if(!ssdbUnLockTable(scb->dberr_h,scb->dbconn_h))
    {
	sscError(hError,"Database API Error: \"%s\"\n\n",
		 ssdbGetLastErrorString(scb->dberr_h));
	drop_temp_table(hError,scb->dbconn_h,scb->dberr_h);	
	return FAILED;
    }

    /* Processing data should be done in a separate functions.	*/
    /* these functions will be setting up the lastError if any	*/

    if (scb->group_reports_enabled)
    {
	rv = process_group_records(scb, from_t, to_t, dbreq_h, flag);
    }
    else
    {
	rv = process_local_records(scb, from_t, to_t, this_sysid, 
				   this_hname, dbreq_h, flag);
    }
    
    ssdbFreeRequest(scb->dberr_h,dbreq_h);
    
    if (!rv) 
    {
	/* SUCCESS */
	amr_data_in_mem++;
    }

    drop_temp_table(hError,scb->dbconn_h,scb->dberr_h);
    return (rv);
}

static void init_event(event_t *evnt)
{
    /* Initialize some parts of this data structure evnt */
    evnt->ev_type = UND_INTR;
    evnt->prevstart = 0;
    evnt->event_time =  evnt->start_time = evnt->lasttick = -1;
    evnt->noprevstart = 0;
    evnt->uptime = 0;
    evnt->dntime = 0;
    evnt->bound = -1;
    evnt->metric = 0;
    evnt->summarylen =0;
    evnt->summary = NULL;
    evnt->size = evnt->tag = 0; 
    evnt->next = evnt->prev = NULL;

}

/*----------------------------------------------------------------------*/
/*	     Function:  process_local_records				*/
/*								        */
/*    This function processes set of availdata record read in from the	*/
/*    DB, for the local sys_id only, even though the DB may contain     */
/*    data for other hosts. It assumes a session control block          */
/*    has  no hosts records attached to it, when coming in. If there is */
/*    any case when that is not true, the code needs to change to	*/
/*    accomodate that.							*/
/*----------------------------------------------------------------------*/

static int  process_local_records(session_t *scb, time_t from_t, 
				  time_t to_t, 
				  char *local_sys_id, 
				  char *local_sys_name, 
				  ssdb_Request_Handle dbreq_h,int flag)
{ 

    const char			**db_data;
    int				rv, i;
    int				event_tag = 1;
    int				ind = -1;
    char			this_sysid[32];
    int				db_rows;

    hostinfo_t		*hinfo = NULL, *temp, *tmp1;
    event_t			*ev, *evtmp1, *evtmp2;    
    char *title="SYSTEM INFORMATION &gt; Availability";

    /* loop getting records from db_data */
    db_rows = ssdbGetNumRecords(scb->dberr_h, dbreq_h);
    if (!db_rows){
	scb->lastError = AMR_NO_RECORDS_IN_DB;
	if(flag != AMR_INIT)
	    generate_error_page(scb, title,"No data available currently.");
	return (FAILED);
    }

    while (db_rows){

	db_data = ssdbGetRow(scb->dberr_h, dbreq_h);
	db_rows--;
	if (!db_data){
	    rv= ssdbGetLastErrorCode(scb->dberr_h);
	    scb->lastError = rv;
	    return (FAILED);
	}
    
	if (scb->hosts == NULL){
	    hinfo = (hostinfo_t *) calloc (sizeof(hostinfo_t),1);
	    scb->hosts = hinfo;
	    scb->hosts_last = hinfo;
	    hinfo->tag = 1;		/* This record ID */
	    hinfo->next = NULL;
	    hinfo->prev = NULL;

	    /* Populate hosts info struct */
	    hinfo->from_t = from_t;
	    hinfo->to_t = to_t;
	    strcpy(hinfo->sys_id, local_sys_id);
	    i = hinfo->hostnamelen = strlen(local_sys_name);
	    hinfo->hostname = (char *)calloc(hinfo->hostnamelen + 1,1);
	    strncpy(hinfo->hostname, local_sys_name, hinfo->hostnamelen);  
	    *(hinfo->hostname + i) = '\0';	    
	};

	/* if we have a hosts struct already attached here, it		*/
	/* should in all probability, be from up here			*/
	/* Is there a case when we come into this function with a	*/
	/* host list already, so need a cleanup - don't think so??????	*/

	/* Now process the event */

	ev = (event_t *)calloc(sizeof(event_t),1);
	init_event(ev);
	ev->tag = event_tag++;                /* post increment tag value */
						   /* its record ID */
	
	/* Convert event.type_id value to an index type value, relative to 1 */
	ev->ev_type = TOINDX(atoi(db_data[EVENT_TYPE])); 
	
	/* all  are availmon reports; set them up accordingly */
	ev->prevstart = atol(db_data[AMR_PREVSTART]);
	ev->event_time = atol(db_data[EVENT_TIME]);
	ev->start_time = atol(db_data[AMR_START]);
	ev->lasttick = atol(db_data[AMR_LASTTICK]);
	ev->bound = atoi(db_data[AMR_BOUNDS]);
	ev->metric = atoi(db_data[AMR_METRIC]);
	if (db_data[AMR_SUMMARY]){
	    ev->summarylen = strlen(db_data[AMR_SUMMARY]);
	    ev->summary = (char *)calloc(ev->summarylen + 1,1);
	    strcpy(ev->summary, db_data[AMR_SUMMARY]);
	};
	
	/*						*/
	/* if it is a metric related event, setup its	*/
	/* uptime, as  event_time - prevstart ;		*/
	/* dntime is start_time - event_time		*/
	/*						*/
	
	if (ev->metric){
	    if (ev->event_time > 0){
		if (ev->prevstart > 0) 
		    ev->uptime = ev->event_time - ev->prevstart;
		if (ev->start_time > 0)
		    ev->dntime = ev->start_time - ev->event_time;
	    }
	    else if (ev->lasttick > 0){
		/*						*/
		/* event_time is not available, if lasttick is	*/
		/* consider that to be close to event_time and	*/
		/* calculate as above				*/
		
		if (ev->prevstart >= 0) 
		    ev->uptime = ev->lasttick - ev->prevstart;
		if (ev->start_time > 0)
		  {
		    ev->dntime = ev->start_time - ev->lasttick;	
		  }

		/* 
		 * Use the event_time from now on.
		 */
		ev->event_time=ev->lasttick;
	    }	
	    else
	    {
		/* what if we don't have event_time nor lasttick? */
		/* at this point, uptime = dntime = 0	      */
		ev->dntime=ev->uptime=0;
	    }	
	    
	}  /* end if (ev->metric) */
	/* 
	 * Finished setting up the event structure, now link it to the 
	 * hostinfo to which it belongs				
	 */
	if (hinfo->first_ev == NULL)
	{
	    hinfo->first_ev = ev;
		hinfo->last_ev = ev;
	} 
	else 
	{
	    evtmp1 = hinfo->last_ev;
	    hinfo->last_ev = ev;
	    evtmp1->next = ev;
	    ev->prev = evtmp1;
	}
    }  /* end while (TRUE) */

    collect_host_stats(hinfo);
    return(SUCCESS);
}

/*----------------------------------------------------------------------*/
/*	     Function:  process_group_records				*/
/*								        */
/*    This function processes set of availdata record read in from the	*/
/*    DB, for all sys_ids available in the availdata table.		*/
/*    It assumes a session control block			        */
/*    has  no hosts records attached to it, when coming in. If there is */
/*    any case when that is not true, the code needs to change to	*/
/*    accomodate that.							*/
/*----------------------------------------------------------------------*/

static int  process_group_records(session_t *scb, time_t from_t, 
				  time_t to_t, ssdb_Request_Handle dbreq_h,
				  int flag)
{ 

    const char			**db_data;
    int				rv, i;
    int				host_tag = 1;
    int				e_tag;
    int				ind = -1;
    char			this_sysid[64];
    int				db_rows;
    hostinfo_t		*hinfo = NULL, *temp, *tmp1;
    event_t			*ev, *evtmp1;    
    char *title="SYSTEM INFORMATION &gt; Availability";

    /* loop getting records from db_data */
    /*                                   */

    db_rows = ssdbGetNumRecords(scb->dberr_h, dbreq_h);
    if (!db_rows){
	scb->lastError = AMR_NO_RECORDS_IN_DB;
	if(flag != AMR_INIT)
	    generate_error_page(scb, title,"No data available currently.");
	return (FAILED);
    }
    
    while (db_rows){
	db_data = ssdbGetRow(scb->dberr_h, dbreq_h);
	db_rows--;
	if (!db_data){
	    rv= ssdbGetLastErrorCode(scb->dberr_h);
	    scb->lastError = rv;
	    return (FAILED);
	}
	
	/* Process the avail record */
	strcpy(this_sysid, db_data[AMR_SYS_ID]);

	/* have we seen this sys_id already? */
	temp = scb->hosts;
	hinfo = NULL;
	while (temp){
	    if (!strncasecmp(this_sysid, temp->sys_id,MAX_SYSIDLEN)) {
		hinfo = temp ;        /* point to host_info struct	*/
		break;
	    }
	    else{
		temp = (hostinfo_t *)temp->next;
	    }
	}

	/*
	 * didn't find one in the table, create new entry and link into hosts
	 * list for this session control block
	 */

	if ( hinfo == NULL){
	    hinfo = (hostinfo_t *) calloc (sizeof(hostinfo_t),1);
	    if (scb->hosts == NULL){
		scb->hosts = hinfo;
		scb->hosts_last = hinfo;
		hinfo->next = NULL;
		hinfo->prev = NULL;
	    }
	    else{ 
		temp = scb->hosts_last;
		temp->next = hinfo;
		hinfo->prev = temp;
		hinfo->next = NULL;
		scb->hosts_last = hinfo;
	    }
	    hinfo->tag = host_tag++;          /* record ID */
	    scb->hosts_count++;
	    
	    /* Populate hosts info struct */
	    hinfo->from_t = from_t;
	    hinfo->to_t = to_t;
	    strcpy(hinfo->sys_id, this_sysid);
	    i = hinfo->hostnamelen = strlen(db_data[AMR_SYS_NAME]);
	    hinfo->hostname = (char *)calloc(hinfo->hostnamelen + 1,1);
	    strncpy(hinfo->hostname, db_data[AMR_SYS_NAME], 
		    hinfo->hostnamelen);
	    hinfo->hostname[i]=0;
	}
	
	/* Now process the event */
	ev = (event_t *)calloc(sizeof(event_t),1);
	init_event(ev);

	/* Convert event.type_id value to an index type value, relative to 1 */

	ev->ev_type = TOINDX(atoi(db_data[EVENT_TYPE]));
	/* all others are availmon reports; set them up accordingly */
	
	ev->prevstart = atol(db_data[AMR_PREVSTART]);
	ev->event_time = atol(db_data[EVENT_TIME]);
	ev->start_time = atol(db_data[AMR_START]);
	ev->lasttick = atol(db_data[AMR_LASTTICK]);
	ev->bound = atoi(db_data[AMR_BOUNDS]);
	ev->metric = atoi(db_data[AMR_METRIC]);
	if (db_data[AMR_SUMMARY]) {
	    ev->summarylen = strlen(db_data[AMR_SUMMARY]);
	    ev->summary = (char *)calloc(ev->summarylen + 1,1);
	    strcpy(ev->summary, db_data[AMR_SUMMARY]);
	};
	
	/* if it is a metric related event, setup its	*/
	/* uptime, if otherwise, uptime = 0;		*/
	/* if event_time can't be determined(<=0),		*/
	/* leave the uptime -ve and adjust later	        */
	
	if (ev->metric){
	    if (ev->event_time > 0){
		if (ev->prevstart > 0)
		    ev->uptime = ev->event_time - ev->prevstart;
		if (ev->start_time > 0)
		    ev->dntime = ev->start_time - ev->event_time;
	    }
	    else if (ev->lasttick > 0){
		/*                                              */
		/* event_time is not available, if lasttick is  */
		/* consider that to be close to event_time and  */
		/* calculate as above                           */
		
		if (ev->prevstart >= 0)
		    ev->uptime = ev->lasttick - ev->prevstart;
		if (ev->start_time > 0)
		{
		    ev->dntime = ev->start_time - ev->lasttick;
		}
		
	    }
	}		 

	/* Finished setting up the event structure, now link it to the	*/
	/* hostinfo to which it belongs					*/

	if (hinfo->first_ev == NULL){
	    ev->tag = 1;                     /* First event's record ID */
	    hinfo->first_ev = ev;
	    hinfo->last_ev = ev;
	} else {
	    e_tag = (hinfo->last_ev)->tag;
	    evtmp1 = hinfo->last_ev;
	    ev->tag = ++e_tag;           /* last_event_tag + 1 */
	    hinfo->last_ev = ev;
	    evtmp1->next = ev;
	    ev->prev = evtmp1;
	}
    }  /* end while (TRUE) */

    /* Now process group data and get statistics values.*/
    collect_group_stats(scb);
    return(SUCCESS);
}

/*------------------------------------------------------*/
/*  Function:  collect_host_stats			*/
/*							*/
/*  This function will go over the linked list of	*/
/*  events for the single host specifed by input arg hp */
/*  and collects					*/
/*  necessary statistics values for it.			*/
/*							*/
/*------------------------------------------------------*/
static void collect_host_stats(hostinfo_t *hp)
{

    int  i, evnt, t;
    int up = 0, dn = 0, offtime = 0, lessdntime = 0;
    event_t			*evp, *evp1, *ev_prev=NULL;    
    

    if (!hp) return;

    hp->num_total_events=0;

    /* Initialize stats values for this hostinfo struct */
    for (i=0; i<AMR_N_EVNT; i++){
	hp->n_ev[i] = hp->ev_uptime[i] = hp->ev_dntime[i] = 0;
	hp->ev_avgup[i] = 0;
    }
    
    for (i=0; i<NSTATS; i++){
	hp->stats[i] = hp->stuptime[i] = hp->stdntime[i] = 0;
    }
    hp->total = hp->most_uptime = hp->most_dntime = 0;
    hp->thisepoch = hp->possible_uptime = hp->nepochs = 0;
    hp->least_uptime = hp->least_dntime = INT_MAX;

    evp = hp->first_ev;
    while (evp) {
	evnt = evp->ev_type;
	hp->n_ev[evnt]++;
	if (!(evp->metric)){
	    /* a non-metric related event, collect no stats for it */
	    evp = evp->next;
	    continue;    /* next event loop */
	}
	
	if((hp->from_t && evp->start_time < hp->from_t) || 
	   (hp->to_t && evp->prevstart > hp->to_t))
	{
	    /* 
	     * If  from_t is greater than this events start time. Then just
	     * ignore this event.
	     */
	    ev_prev = evp;
	    evp = evp->next;
	    continue;
	}

	if (ev_prev != NULL){
	    
	    /* Check to see if this event's prevstart is before		*/
	    /* the previous event's start time -this could happen	*/
	    /* in case of system time adjustment etc.			*/
	    	
	    if (evp->prevstart < ev_prev->start_time)
	    {
		/*
		 * Just ignore this event..
		 */
		evp = evp->next;
		continue;
	    }

	    if(evp->prevstart > ev_prev->start_time)
	    {
		/*
		 * 50-50.
		 */
		offtime += (evp->prevstart-ev_prev->start_time)/2;
		evp->uptime += (evp->prevstart-ev_prev->start_time)/2;
	    }
	}

	/* this is a metric event, so the system			*/
	/* was up once before this event happened, then came down.	*/
	/* so update the number of times system was up and down.	*/
	
	up++; dn++;

	if(hp->from_t > 0 && evp->event_time < hp->from_t && 
	   evp->start_time > hp->from_t)
	{
	    /* 
	     * If  from_t hits in the downtime.
	     */

	    if(hp->to_t > 0 && evp->event_time < hp->to_t &&
	       evp->start_time > hp->to_t)
	    {
		/* 
		 *  to_t also is in the downtime.
		 */
		hp->ev_dntime[evnt] += (hp->to_t-hp->from_t);
	    }
	    else
		/* 
		 * to_t is not in downtime.
		 */
		hp->ev_dntime[evnt] += (hp->from_t-evp->event_time);

	}
	else if(hp->from_t > 0 && evp->prevstart < hp->from_t && 
		evp->event_time > hp->from_t)
	{
	    /* 
	     * If  from_t hits in the uptime.
	     */
	    if(hp->to_t > 0 && evp->event_time < hp->to_t &&
	       evp->start_time > hp->to_t)
	    {			
		/*  to_t is in the downtime */
		hp->ev_uptime[evnt] += 
		    evp->uptime - (hp->from_t-evp->prevstart);
		hp->total += evp->uptime - (hp->from_t-evp->prevstart);
		hp->ev_dntime[evnt] += (hp->to_t-evp->event_time);
	    }
	    else if (hp->to_t > 0 && evp->prevstart < hp->to_t &&
		     evp->event_time > hp->to_t)
	    {
		/*  to_t is in the uptime */
		hp->ev_uptime[evnt] += hp->to_t - hp->from_t;
		hp->total += hp->to_t - hp->from_t;
	    }
	    else
	    {
		/* 
		 * to_t is neither in the uptime nor the downtime.
		 */
		hp->ev_uptime[evnt] += 
		    evp->uptime - (hp->from_t-evp->prevstart);
		hp->total += evp->uptime - (hp->from_t-evp->prevstart);
		hp->ev_dntime[evnt] += evp->dntime;
	    }
	}
	else
	{   
	    /* 
	     * from_t is neither in the uptime nor the downtime.
	     */
	    if(hp->to_t > 0 && evp->event_time < hp->to_t &&
	       evp->start_time > hp->to_t)
	    {			
		/*  to_t is in the downtime */
		hp->ev_uptime[evnt] += evp->uptime;
		hp->total += evp->uptime;
		hp->ev_dntime[evnt] += (hp->to_t-evp->event_time);
	    }
	    else if (hp->to_t > 0 && evp->prevstart < hp->to_t &&
		     evp->event_time > hp->to_t)
	    {
		/*  to_t is in the uptime */
		hp->ev_uptime[evnt] += hp->to_t - evp->prevstart;
		hp->total += hp->to_t - evp->prevstart;
	    }
	    else
	    {
		hp->ev_uptime[evnt] += evp->uptime;
		hp->total += evp->uptime;
		hp->ev_dntime[evnt] += evp->dntime;
	    }
	}

	if ((evp->uptime) && (evp->uptime < hp->least_uptime))
	    hp->least_uptime = evp->uptime;

	if (evp->uptime > hp->most_uptime) 
	    hp->most_uptime = evp->uptime;

	if ((evp->dntime) && (evp->dntime < hp->least_dntime)) 
	    hp->least_dntime = evp->dntime;

	if (evp->dntime > hp->most_dntime) 
	    hp->most_dntime =  evp->dntime;
    
	ev_prev = evp;
	evp = evp->next;

    } /* end of while (evp) */
    

    if(evp = hp->first_ev)
    {
	/* 
	 * Get the beginning of time. Either from_t or prevstart of first 
	 * event.
	 */
	if ((hp->from_t) && (hp->from_t > evp->prevstart)){
	    /* The event is non-subscription */
	    hp->the_beginning = hp->from_t;
	} 
	else 
	{
	    /*  no from_t or from_t <= evp->prevstart */
	    hp->the_beginning = evp->prevstart;
	}
    }
    /* 
     * Do some cleanup.
     */
    while (evp) {
	if(hp->the_beginning > evp->prevstart)
	    hp->the_beginning = evp->prevstart;

	if (!(evp->metric)){
	    /* a non-metric related event, collect no stats for it */
	    evp = evp->next;
	    continue;    /* next event loop */
	}
	evnt = evp->ev_type; 
	if (hp->ev_avgup[evnt] == 0)  
	    hp->ev_avgup[evnt] = (hp->ev_uptime[evnt])/(hp->n_ev[evnt]);

	/* XXXXX - DONOT do averages - just take it as 1 minute == 60 secs */
	if (evp->uptime == 0){
	    evp->uptime = 60;
	    hp->ev_uptime[evnt] += evp->uptime;

	    if (evp->dntime > 60){
		evp->dntime = evp->dntime - evp->uptime;
		hp->ev_dntime[evnt] -= evp->uptime;
	    }
	    hp->total += evp->uptime;
	}
	evp = evp->next;
    }

    evp = hp->last_ev;

    /* get lastboot time = last event's start_time		*/ 
    if(evp)
    {
	if(evp->start_time)
	    hp->lastboot = evp->start_time;
	else
	    if(evp->prevstart)
		hp->lastboot = evp->prevstart;
	else 
	    hp->lastboot = evp->event_time;
    }
    else
    {
	if(hp->from_t)
	    hp->lastboot=hp->from_t;
	else
	    hp->lastboot=hp->the_beginning;
    }

    /* Now look at the last event and adjust for the to_time value	*/
    /* being somewhere inside of event times */
    if (evp && hp->to_t){
	hp->nepochs = up;
	
	if(hp->to_t < evp->start_time) 
	{
	    hp->possible_uptime = hp->to_t - hp->the_beginning - offtime;
	}
	else
	{
	    if(hp->to_t >= now)
	    {
		if(now > hp->lastboot)
		    hp->thisepoch = now - hp->lastboot;
		else
		    hp->thisepoch = 0;
		
		if(now > (hp->the_beginning - offtime))
		    hp->possible_uptime = now - hp->the_beginning - offtime;
		else
		    hp->possible_uptime = hp->lastboot - hp->the_beginning 
			- offtime;
		
		hp->total += hp->thisepoch;
	    }
	    else
	    {
		hp->thisepoch = hp->to_t - hp->lastboot;
		hp->possible_uptime =
		    hp->to_t - hp->the_beginning - offtime;
		hp->total += hp->thisepoch;
	    }
	}
    }
    else if (count_this_epoch)
    {
	/* No cutoff time specified. The last event may have some start	*/
	/* time, different from 'now', perhaps we should consider the	*/
	/*  system to be up since the last starttime, so make that	*/
	/* adjustment.							*/

	if(now > hp->lastboot)
	{
	    hp->thisepoch = now - hp->lastboot;
	    hp->total += hp->thisepoch;
	    if (hp->thisepoch < hp->least_uptime) hp->least_uptime = 
						      hp->thisepoch;
	    if (hp->thisepoch > hp->most_uptime) hp->most_uptime = 
						     hp->thisepoch;
	    hp->nepochs = up + 1;
	}
	else
	{
	    hp->thisepoch = 0;
	    hp->nepochs = up;
	}
	
	if(now > (hp->the_beginning - offtime))
	    hp->possible_uptime = now - hp->the_beginning - offtime;
	else
	    hp->possible_uptime = hp->lastboot - hp->the_beginning - offtime;
    } 
    else 
    {
	/* we don't need to count this epoch */

	hp->nepochs = up;
	hp->possible_uptime = hp->lastboot - hp->the_beginning - offtime;
    }

    if(hp->least_uptime == INT_MAX)
	hp->least_uptime=hp->thisepoch;
    if(hp->least_dntime == INT_MAX)
	hp->least_dntime=0;
    if(hp->most_uptime == 0)
	hp->most_uptime=hp->thisepoch;
    if(hp->possible_uptime == 0)
	hp->possible_uptime=hp->thisepoch;
    if(hp->total == 0)
	hp->total=hp->thisepoch;
    if(hp->nepochs == 0)
	hp->nepochs=1;

    /* Total number of times system was DOWN */
    hp->ndowns = dn;

    for(i=0;i<AMR_N_EVNT;i++)
	hp->num_total_events+=hp->n_ev[i];

    gather_host_stat_counts(hp);

}

/*------------------------------------------------------*/
/*							*/
/*	Function:  gather_host_stat_counts		*/
/*							*/
/* This function takes a hostinfo pointer and		*/
/* collects the necessary stats vaues for it, as	*/
/* defined in current amreport.c			*/
/*							*/
/*------------------------------------------------------*/

static void gather_host_stat_counts(hostinfo_t *hp)
{

    /* ----------------- Event COUNT Values -------------------	*/
    /* Gather event counts for Unscheduled downtime events	*/

    hp->stats[1] = hp->n_ev[HW_PANIC_IND];
    hp->stats[2] = hp->n_ev[SW_PANIC_IND];
    hp->stats[3] = hp->n_ev[UND_PANIC_IND];              /* just some panic */
    
    /* various reset  action values  in number 4 */
    hp->stats[4] = hp->n_ev[UND_INTR_IND] + hp->n_ev[UND_SYSOFF_IND] 
	+ hp->n_ev[UND_NMI_IND] + hp->n_ev[UND_RESET_IND] 
	+ hp->n_ev[UND_PWRCYCLE_IND];

    hp->stats[5] = hp->n_ev[UND_PWRFAIL_IND];

    /* now add these above for a aggregate value */
    hp->stats[0] = hp->stats[1] + hp->stats[2] + hp->stats[3] + 
	hp->stats[4] + hp->stats[5];

    /* Now event counts for service action type downtimes	*/
    /* in stats[7] to stats[14] values				*/

    hp->stats[7] = hp->n_ev[MU_HW_FIX_IND] + hp->n_ev[SU_HW_FIX_IND];
    hp->stats[8] = hp->n_ev[MU_HW_UPGRD_IND] + hp->n_ev[SU_HW_UPGRD_IND];
    hp->stats[9] = hp->n_ev[MU_SW_UPGRD_IND] + hp->n_ev[SU_SW_UPGRD_IND];
    hp->stats[10] = hp->n_ev[MU_SW_FIX_IND] + hp->n_ev[SU_SW_FIX_IND];
    hp->stats[11] = hp->n_ev[MU_SW_PATCH_IND] + hp->n_ev[SU_SW_PATCH_IND];
    hp->stats[12] = hp->n_ev[SU_UND_ADM_IND];
    hp->stats[13] = hp->n_ev[MU_UND_ADM_IND];

    hp->stats[14] = hp->n_ev[MU_UND_UNK_IND] + hp->n_ev[MU_UND_TOUT_IND] 
	+ hp->n_ev[MU_UND_NC_IND] + hp->n_ev[SU_UND_TOUT_IND] 
	+ hp->n_ev[SU_UND_NC_IND];

    hp->stats[6] = hp->stats[7] + hp->stats[8] + hp->stats[9] +  hp->stats[10] 
	+ hp->stats[11] + hp->stats[12] + hp->stats[13] + hp->stats[14];

    /* OVERALL Statistics counts	*/
    hp->stats[15] = hp->stats[0] + hp->stats[6];


    /* ----------------- Event UPTIME Values -------------------	*/
    /* gather statistics uptime values for unscheduled downtime events	*/
    /* same as above							*/

    hp->stuptime[1] = hp->ev_uptime[HW_PANIC_IND];
    hp->stuptime[2] = hp->ev_uptime[SW_PANIC_IND];
    hp->stuptime[3] = hp->ev_uptime[UND_PANIC_IND]; 
    hp->stuptime[4] = hp->ev_uptime[UND_INTR_IND] + hp->ev_uptime[UND_SYSOFF_IND] 
	+ hp->ev_uptime[UND_NMI_IND] + hp->ev_uptime[UND_RESET_IND]
	+ hp->ev_uptime[UND_PWRCYCLE_IND];

    hp->stuptime[5] = hp->ev_uptime[UND_PWRFAIL_IND];

    hp->stuptime[0] = hp->stuptime[1] + hp->stuptime[2] 
	+ hp->stuptime[3] +  hp->stuptime[4] + hp->stuptime[5];

    /* Now collect statistics uptime for service action type downtime events    */

    hp->stuptime[7] =	
	hp->ev_uptime[MU_HW_FIX_IND] + hp->ev_uptime[SU_HW_FIX_IND];
    hp->stuptime[8] = 
	hp->ev_uptime[MU_HW_UPGRD_IND] + hp->ev_uptime[SU_HW_UPGRD_IND];
    hp->stuptime[9] = 
	hp->ev_uptime[MU_SW_UPGRD_IND] + hp->ev_uptime[SU_SW_UPGRD_IND];
    hp->stuptime[10] = 
	hp->ev_uptime[MU_SW_FIX_IND] + hp->ev_uptime[SU_SW_FIX_IND];
    hp->stuptime[11] = 
	hp->ev_uptime[MU_SW_PATCH_IND] + hp->ev_uptime[SU_SW_PATCH_IND];

    hp->stuptime[12] = hp->ev_uptime[SU_UND_ADM_IND];
    hp->stuptime[13] = hp->ev_uptime[MU_UND_ADM_IND];

    hp->stuptime[14] = 
	hp->ev_uptime[MU_UND_UNK_IND] + hp->ev_uptime[MU_UND_TOUT_IND] 
	+ hp->ev_uptime[MU_UND_NC_IND] + hp->ev_uptime[SU_UND_TOUT_IND] 
	+ hp->ev_uptime[SU_UND_NC_IND];

    hp->stuptime[6] = 
	hp->stuptime[7] + hp->stuptime[8] + hp->stuptime[9] +  hp->stuptime[10] 
	+ hp->stuptime[11] + hp->stuptime[12] + hp->stuptime[13] 
	+ hp->stuptime[14];

    /* OVERALL Statistics uptimes	*/
    hp->stuptime[15] = hp->stuptime[0] + hp->stuptime[6];

    /* ----------------- Event DOWNTIME Values -------------------	*/
    /* statistics downtime values for unscheduled downtime events	*/
    /* same indecies as above						*/ 
  
    hp->stdntime[1] = hp->ev_dntime[HW_PANIC_IND];
    hp->stdntime[2] =  hp->ev_dntime[SW_PANIC_IND]; 
    hp->stdntime[3] =  hp->ev_dntime[UND_PANIC_IND];
    hp->stdntime[4] =  
	hp->ev_dntime[UND_INTR_IND] +  hp->ev_dntime[UND_SYSOFF_IND] 
	+ hp->ev_dntime[UND_NMI_IND] +  hp->ev_dntime[UND_RESET_IND]
	+ hp->ev_dntime[UND_PWRCYCLE_IND];

    hp->stdntime[5] =  hp->ev_dntime[UND_PWRFAIL_IND];

    hp->stdntime[0] = hp->stdntime[1] + hp->stdntime[2] +  hp->stdntime[3] 
	+ hp->stdntime[4] + hp->stdntime[5];
 
    /* Now collect statistics downtime for service action type downtime events  */

    hp->stdntime[7] =	
	hp->ev_dntime[MU_HW_FIX_IND] + hp->ev_dntime[SU_HW_FIX_IND];
    hp->stdntime[8] = 
	hp->ev_dntime[MU_HW_UPGRD_IND] + hp->ev_dntime[SU_HW_UPGRD_IND];
    hp->stdntime[9] = 
	hp->ev_dntime[MU_SW_UPGRD_IND] + hp->ev_dntime[SU_SW_UPGRD_IND];
    hp->stdntime[10] = 
	hp->ev_dntime[MU_SW_FIX_IND] + hp->ev_dntime[SU_SW_FIX_IND];
    hp->stdntime[11] = 
	hp->ev_dntime[MU_SW_PATCH_IND] + hp->ev_dntime[SU_SW_PATCH_IND];
    hp->stdntime[12] = hp->ev_dntime[SU_UND_ADM_IND];
    hp->stdntime[13] = hp->ev_dntime[MU_UND_ADM_IND];
    hp->stdntime[14] = 
	hp->ev_dntime[MU_UND_UNK_IND] + hp->ev_dntime[MU_UND_TOUT_IND] 
	+ hp->ev_dntime[MU_UND_NC_IND] + hp->ev_dntime[SU_UND_TOUT_IND] 
	+ hp->ev_dntime[SU_UND_NC_IND];

    hp->stdntime[6] = 
	hp->stdntime[7] + hp->stdntime[8] + hp->stdntime[9] +  hp->stdntime[10] 
	+ hp->stdntime[11] + hp->stdntime[12] + hp->stdntime[13] 
	+ hp->stdntime[14];

    /* OVERALL Statistics down times 	*/
    hp->stdntime[15] = hp->stdntime[0] + hp->stdntime[6];

    /* ----------- Done gathering HOST STATS COUNTS !!! ------------------*/

}

/*------------------------------------------------------*/
/*  Function:  collect_group_stats			*/
/*							*/
/*  This function will go over the linked list of	*/
/*  events for all the hosts for the given session_id   */
/*  and collect						*/
/*  necessary statistics values for each.      		*/
/*							*/
/*------------------------------------------------------*/

static void collect_group_stats(session_t *scb)
{
    int up=0,dn=0;
    int i, evnt, t;
    int offtime = 0, lessdntime = 0;
    int prev_evnt = -1;              /* look at this prev event when >0 */

    hostinfo_t		*hp = NULL, *temp, *tmp1;
    event_t			*evp, *evp1, *ev_prev=NULL;    
    
    
    hp = scb->hosts;            /* First host on te hosts list */
    while (hp)
    {
#if 0
	/* 
	 * The ifdef should be removed after we have a way of getting the
	 * last and first subscribed time.
	 */
	if(hp->last_subscribed_time > 0 && 
	   (hp->to_t > hp->last_subscribed_time || hp->to_t <= 0))
	{
	    hp->to_t = hp->last_subscribed_time;
	}
	if(hp->first_subscribed_time > 0 &&
	   (hp->from_t < hp->first_subscribed_time || hp->from_t <= 0))
	{
	    hp->from_t = hp->first_subscribed_time;
	}
#endif
	collect_host_stats(hp);
	/* process the next host data in the linked list */
	hp = hp->next;
    }  /* while (hp)  */


} /* end collect_group_stats(...)  */

/*------------------------------------------------------*/
/*							*/
/*   Function: gather_aggregate_stats			*/
/* This function goes thru the host list for a session  */
/* collecting the overall statistics values for the     */
/* set of hosts, for group level reports.		*/
/*							*/
/*------------------------------------------------------*/
static void gather_aggregate_stats(session_t *scb,int num_ids,char **sysids)
{

    hostinfo_t	*hp;
    aggregate_stats_t	*agstat;  
    int	i, j;

    /* But before that, make room to collect aggregate statistics too */

    if (!(scb->aggr_stats = 
	  (aggregate_stats_t *) calloc(sizeof(aggregate_stats_t),1))){
	/* malloc unsuccessful */
	scb->lastError = AMR_NO_MEM;
	return; 
    }   

    agstat = scb->aggr_stats;

    /* Initialize aggregate stats data structure */
    for (i=0; i < AMR_N_EVNT; i++){
	agstat->n_ev[i] = agstat->ev_uptime[i] = agstat->ev_dntime[i] = 0;
	agstat->ev_avgup[i] = 0;
    }
    for (i=0; i < NSTATS; i++)
	agstat->stats[i] = agstat->stuptime[i] = agstat->stdntime[i] = 0;

    agstat->total = agstat->most_uptime = agstat->most_dntime = 0;
    agstat->nepochs =  agstat->ndowns = 0;
    agstat->thisepoch = 0;
    agstat->possible_uptime = 0;
    agstat->least_uptime = agstat->least_dntime = INT_MAX;

    /* Now loop thru sysids list and collect numbers. */
    hp = scb->hosts;
    while (hp) {
	if(num_ids && sysids && !exist_sysid(hp->sys_id,num_ids,sysids))
	{
	    hp=hp->next;
	    continue;
	}

	for (i=0; i < AMR_N_EVNT; i++){
	    agstat->n_ev[i] += hp->n_ev[i];
	    agstat->ev_uptime[i] += hp->ev_uptime[i];
	    agstat->ev_dntime[i] += hp->ev_dntime[i];
	    agstat->ev_avgup[i] += hp->ev_avgup[i];
	}
	
	for (i=0; i < NSTATS; i++){
	    agstat->stats[i] += hp->stats[i];
	    agstat->stuptime[i] += hp->stuptime[i];
	    agstat->stdntime[i] += hp->stdntime[i];
	}

	if (hp->nepochs){
	    if (hp->least_uptime < agstat->least_uptime){
		agstat->least_uptime = hp->least_uptime;
		agstat->upleasthost =  hp->hostname; /* just point to the same buffer! */
	    }
	    if (hp->most_uptime > agstat->most_uptime){
		agstat->most_uptime = hp->most_uptime;
		agstat->upmosthost = hp->hostname;
	    }
	}

	if (hp->ndowns){
	    if (hp->least_dntime < agstat->least_dntime){
		agstat->least_dntime = hp->least_dntime; 
		agstat->dnleasthost = hp->hostname; 
	    }
	    if (hp->most_dntime > agstat->most_dntime){
		agstat->most_dntime = hp->most_dntime;
		agstat->dnmosthost = hp->hostname;
	    }
	}

	agstat->total += hp->total;
	agstat->possible_uptime += hp->possible_uptime;
	agstat->nepochs += hp->nepochs;
	agstat->ndowns += hp->ndowns;

	/* Next host please */
	hp = hp->next;

    } /* end while(hp) */

} /* gather_aggregate_stats */

static char *pretty_minutes(int m,char *s)
{
    int		days, hrs, mins, tm;
    char	tempd[QUERY_MAX_SZ];
    char	temph[QUERY_MAX_SZ];
    char	tempm[QUERY_MAX_SZ];

    tm = m;
    days = m / (60 * 24);
    m %= (60 * 24);
    hrs = m / 60;
    m %= 60;
    mins = m;

    s[0] = tempd[0] = temph[0] = tempm[0] = NULL;
    if (days)
	sprintf(tempd, "%d day%s", days, (days == 1) ? "" : "s");
    if (hrs)
	sprintf(temph, "%s%d hr%s", (days ? " " : "") /* space */,
		hrs, (hrs == 1)? "":"s");
    if (mins)
	sprintf(tempm, "%s%d min%s", ((days||hrs) ? " " : "") /* space */,
		mins, (mins == 1)? "":"s");
    if (days || hrs) /* no point in printing minutes by itself again */
	sprintf(s, "%d minutes (%s%s%s)", tm, tempd, temph, tempm); 
    else 
	sprintf(s, "%d minute%s", tm, (tm == 1) ? "" : "s");
    return(s);
}

static void generate_initial(session_t *scb,int group,int argc,char **argv)
{
    double ftotal,     fpossible;
    hostinfo_t     *hp=NULL;
    aggregate_stats_t   *agstat=NULL; 
    int num_events;
    int dntime;

    if(!scb)
	return;
    

    if(group && scb->group_reports_enabled)
    {
	gather_aggregate_stats(scb,argc,argv);    
	agstat=scb->aggr_stats;
	if(!agstat)
	    return;

	ftotal    = agstat->stuptime[15];         /* total uptime */
	fpossible = agstat->possible_uptime;
	num_events= agstat->stats[15];
	dntime    = agstat->stdntime[15];
    }
    else
    {
	hp=scb->hosts;

	while(hp && strncasecmp(hp->sys_id,scb->local_sysid,MAX_SYSIDLEN))
	{
	    hp=hp->next;
	}

	if(!hp)
	    return;
	ftotal    = hp->stuptime[15];         /* total uptime */
	fpossible = hp->possible_uptime;
	num_events= hp->stats[15];
	dntime    = hp->stdntime[15];
    }

  if ( scb->textonly == 0) 
  {
    TableBegin("BORDER=4 CELLPADDING=6 CELLSPACING=1");
	 
	 RowBegin("ALIGN=LEFT");
	   CellBegin("");
	        if(group && scb->group_reports_enabled)
		{
		    Body("Total Availability for all systems on site (%) &nbsp;=&nbsp;\n");
		}
                else
		{
		    Body("Total Availability (%) &nbsp;=&nbsp;\n");
		}
		FormatedBody("%3.2f",((fpossible-dntime)/fpossible)*100.0);
	   CellEnd();
	 RowEnd();
	 
	 RowBegin("ALIGN=LEFT");
	   CellBegin("");
		if (group && scb->group_reports_enabled)
		{
		    Body("MTBI for all systems on site (min)&nbsp;=&nbsp;\n");
		}
		else
		{
		    Body("MTBI (min)&nbsp;=&nbsp;\n");
		}
		FormatedBody("%7.0f",(fpossible/(60*num_events)));
	   CellEnd();
         RowEnd();
     
    TableEnd();
  } else {
    /* Lynx stuff  */
	        if(group && scb->group_reports_enabled)
		{
		    FormatedBody("<pre>   Total Availability for all systems on site (%%%) = %3.2f</pre>",((fpossible-dntime)/fpossible)*100.0);
		}
                else
		{
		    FormatedBody("<pre>   Total Availability (%%%) = %3.2f</pre>",((fpossible-dntime)/fpossible)*100.0);
		}
		
		if (group && scb->group_reports_enabled)
		{
		    FormatedBody("<pre>   MTBI for all systems on site (min) = %7.0f</pre>",(fpossible/(60*num_events)));
		}
		else
		{
		    FormatedBody("<pre>   MTBI (min)             = %-7.0f</pre>",(fpossible/(60*num_events)));
		}
  }  

    return;
}

/* 
 * Get last error code.
 */
static int amrGetLastErrorCode(session_t *scb)
{
    return scb->lastError;
}

static char *getEventDescription(int EventCode,amrEvDesc_t *Descriptions)
{
      int   i = 0;

      for (i = 0; Descriptions[i].eventcode != NOTANEVENT; i++)
	{
	  if (Descriptions[i].eventcode == EventCode ) {
	    return (Descriptions[i].description);
	  }
	}
      
      /*
       * Not reached.  Should only come here in case of a wrong
       * event code
       */
      
      return (Descriptions[i].description);
}

static void generate_group_header(session_t *scb, int num_ids,char **sysids)
{
    hostinfo_t *hp = scb->hosts;

    if ( scb->textonly == 0 )
    {
	TableBegin("BORDER=0 CELLSPACING=0 CELLPADDING=0");
	   RowBegin("VALIGN=TOP");
	     CellBegin("");
	        Body("System name\n");
	     CellEnd();
	     CellBegin("");
	        Body("&nbsp;&nbsp;&nbsp;&nbsp;:&nbsp;\n");
	     CellEnd();/* this  ^^ is intentional. */
	     CellBegin("");
		while(hp)
		{
		    if(num_ids && sysids && 
		       !exist_sysid(hp->sys_id,num_ids,sysids))
		    {
			hp=hp->next;
			continue;
		    }
		    
		    FormatedBody(hp->hostname);
		    Body("&nbsp;\n");
		    hp=hp->next;
		}
	     CellEnd();
	   RowEnd();
	   RowBegin("VALIGN=TOP");
	     CellBegin("");
		Body("Database\n");
	     CellEnd();
	     CellBegin("");
	        Body("&nbsp;&nbsp;&nbsp;&nbsp;:&nbsp;\n");
	     CellEnd();/* this  ^^ is intentional. */
	     CellBegin("");
	        FormatedBody(scb->dbname);
	     CellEnd();
	   RowEnd();
	   RowBegin("VALIGN=TOP");
	     CellBegin("");
		Body("Number of records\n");
	     CellEnd();
	     CellBegin("");
	        Body("&nbsp;&nbsp;&nbsp;&nbsp;:&nbsp;\n");
	     CellEnd();/* this  ^^ is intentional. */
	     CellBegin("");
	        FormatedBody("%d",scb->record_cnt);
	     CellEnd();
	   RowEnd();
	   RowBegin("VALIGN=TOP");
	     CellBegin("");
		Body("Data start time\n");
	     CellEnd();
	     CellBegin("");
	        Body("&nbsp;&nbsp;&nbsp;&nbsp;:&nbsp;\n");
	     CellEnd();/* this  ^^ is intentional. */
	     CellBegin("");
		FormatedBody("%s",ctime(&scb->from_time));
	     CellEnd();
	   RowEnd();
	   RowBegin("VALIGN=TOP");
	     CellBegin("");
		Body("Data end time\n");
	     CellEnd();
	     CellBegin("");
	        Body("&nbsp;&nbsp;&nbsp;&nbsp;:&nbsp;\n");
	     CellEnd();/* this  ^^ is intentional. */
	     CellBegin("");
	        FormatedBody("%s",ctime(&scb->to_time));
	     CellEnd();
	   RowEnd();
	TableEnd();
     } 
     else 
     {
       /* Lynx */
     
        Body("<p></pre>System name   :\n");
        
		while(hp)
		{
		    if(num_ids && sysids && 
		       !exist_sysid(hp->sys_id,num_ids,sysids))
		    {
			hp=hp->next;
			continue;
		    }
		    
		    FormatedBody("%s",hp->hostname);
		    hp=hp->next;
		}
	FormatedBody("</pre>");
	FormatedBody("<pre>   Database          :  %s</pre>",scb->dbname);
	FormatedBody("<pre>   Number of records :  %d</pre>",scb->record_cnt);
	FormatedBody("<pre>   Data start time   :  %s</pre>",ctime(&scb->from_time));
	FormatedBody("<pre>   Data end time     :  %s</pre>",ctime(&scb->to_time));
     }
	
     return;
}


static void generate_header(session_t *scb, hostinfo_t *hp)
{
    if ( scb->textonly == 0 )
    {
	TableBegin("BORDER=0 CELLSPACING=0 CELLPADDING=0");
	   RowBegin("VALIGN=TOP");
	     CellBegin("");
	        Body("System name\n");
	     CellEnd();
	     CellBegin("");
	        Body("&nbsp;&nbsp;&nbsp;&nbsp;:&nbsp;\n");
	     CellEnd();/* this  ^^ is intentional. */
	     CellBegin("");
		FormatedBody(hp->hostname);
	     CellEnd();
	   RowEnd();
	   RowBegin("VALIGN=TOP");
	     CellBegin("");
		Body("Database\n");
	     CellEnd();
	     CellBegin("");
	        Body("&nbsp;&nbsp;&nbsp;&nbsp;:&nbsp;\n");
	     CellEnd();/* this  ^^ is intentional. */
	     CellBegin("");
	        FormatedBody(scb->dbname);
	     CellEnd();
	   RowEnd();
	   RowBegin("VALIGN=TOP");
	     CellBegin("");
		Body("Number of records\n");
	     CellEnd();
	     CellBegin("");
	        Body("&nbsp;&nbsp;&nbsp;&nbsp;:&nbsp;\n");
	     CellEnd();/* this  ^^ is intentional. */
	     CellBegin("");
	        FormatedBody("%d",scb->record_cnt);
	     CellEnd();
	   RowEnd();
	   RowBegin("VALIGN=TOP");
	     CellBegin("");
		Body("Data start time\n");
	     CellEnd();
	     CellBegin("");
	        Body("&nbsp;&nbsp;&nbsp;&nbsp;:&nbsp;\n");
	     CellEnd();/* this  ^^ is intentional. */
	     CellBegin("");
		FormatedBody("%s",ctime(&scb->from_time));
	     CellEnd();
	   RowEnd();
	   RowBegin("VALIGN=TOP");
	     CellBegin("");
		Body("Data end time\n");
	     CellEnd();
	     CellBegin("");
	        Body("&nbsp;&nbsp;&nbsp;&nbsp;:&nbsp;\n");
	     CellEnd();/* this  ^^ is intentional. */
	     CellBegin("");
	        FormatedBody("%s",ctime(&scb->to_time));
	     CellEnd();
	   RowEnd();
	TableEnd();
    } else {
        /* Lynx */
        FormatedBody("<p><pre>   System name       :  %s</pre>",hp->hostname);
	FormatedBody("<pre>   Database          :  %s</pre>",scb->dbname);
	FormatedBody("<pre>   Number of records :  %d</pre>",scb->record_cnt);
	FormatedBody("<pre>   Data start time   :  %s</pre>",ctime(&scb->from_time));
	FormatedBody("<pre>   Data end time     :  %s</pre>",ctime(&scb->to_time));
    }
    return;
}

static void generate_H2_html(session_t *scb,hostinfo_t *hp,char *str,int start)
{
    if(scb->Avail_startValue && scb->Avail_endValue)
    {
	FormatedBody("%s%d?%s=%s&%s=%s&%s=%s&%s=%d %s%s%s",AMR_BEGIN_RGSTRING,AMR_H2,szAvail_sysid,hp->sys_id,szAvail_start,scb->Avail_startValue,szAvail_end,scb->Avail_endValue,szStartEvent,start,AMR_END_RGSTRING,str,AMR_END_HREF);
    }
    else if (scb->Avail_startValue)
    {
	FormatedBody("%s%d?%s=%s&%s=%s&%s=%s&%s=%d %s%s%s",AMR_BEGIN_RGSTRING,AMR_H2,szAvail_sysid,hp->sys_id,szAvail_start,scb->Avail_startValue,szAvail_end,"0",szStartEvent,start,AMR_END_RGSTRING,str,AMR_END_HREF);
    }
    else if (scb->Avail_endValue)
    {
	FormatedBody("%s%d?%s=%s&%s=%s&%s=%s&%s=%d %s%s%s",AMR_BEGIN_RGSTRING,AMR_H2,szAvail_sysid,hp->sys_id,szAvail_start,"0",szAvail_end,scb->Avail_endValue,szStartEvent,start,AMR_END_RGSTRING,str,AMR_END_HREF);
    }
    else
    {
	FormatedBody("%s%d?%s=%s&%s=%s&%s=%s&%s=%d %s%s%s",AMR_BEGIN_RGSTRING,AMR_H2,szAvail_sysid,hp->sys_id,szAvail_start,"0",szAvail_end,"0",szStartEvent,start,AMR_END_RGSTRING,str,AMR_END_HREF);
    }
}

static void generate_H1_host(session_t *scb,hostinfo_t *hp)
{
    int		i;
    double	ftotal, fpossible;
    char buf[QUERY_MAX_SZ];

    ftotal = hp->stuptime[15];         /* total uptime */
    fpossible = hp->possible_uptime;
    
    generate_header(scb,hp);

    if ( scb->textonly == 0)
    {
    Body("<P><HR><P>\n");

    TableBegin("BORDER=4 CELLSPACING=1 CELLPADDING=6 COLS=5 WIDTH=100% NOSAVE");

	 /* Table Heading */
	 RowBegin("ALIGN=\"CENTER\"");
	   CellBegin("width=35%");
	          Body("&nbsp;\n");
	   CellEnd();
	   CellBegin("width=15%");
	           Body("<B>\n");
	           Body("Count\n");
	   CellEnd();
	   CellBegin("width=15%");
	           Body("<B>\n");
	           Body("Downtime\n");
	           Body("<BR>\n");
	           Body("(min)\n");
	   CellEnd();
	   CellBegin("width=15%");
	           Body("<B>\n");
	           Body("MTBI\n");
	           Body("<BR>\n");
	           Body("(min)\n");
	   CellEnd();
	   CellBegin("width=20%");
	           Body("<B>\n");
	           Body("Availability\n");
	           Body("<BR>\n");
	           Body("%\n");
	   CellEnd();
	 RowEnd();

    for (i = 0; i < NSTATS; i++) {
	if(!hp->stats[i])
	   continue;

        RowBegin("");
	   CellBegin("ALIGN=\"LEFT\"");
	       FormatedBody(statname[i].sname);
	   CellEnd();
	   CellBegin("ALIGN=\"CENTER\"");
	       FormatedBody("%5d",hp->stats[i]);
	   CellEnd();
	   CellBegin("ALIGN=\"CENTER\"");
	       FormatedBody("%6d",((hp->stdntime[i]+30)/60));
	   CellEnd();
	   CellBegin("ALIGN=\"CENTER\"");
	       FormatedBody("%7.0f",(fpossible/(60*hp->stats[i])));
	   CellEnd();
	   CellBegin("ALIGN=\"CENTER\"");
               if(i == 0 || i == 6 || i == 15)
	       {
		 FormatedBody("%3.2f",((fpossible-hp->stdntime[i])/fpossible)*100.0);
	       }
	       else
		 Body("&nbsp;\n");
	   CellEnd();
	RowEnd();
    }

    if(hp->nepochs)
    {
	RowBegin("");
	     CellBegin("ALIGN=\"LEFT\"");
	         FormatedBody("Average uptime");
	     CellEnd();
	     CellBegin("COLSPAN=4 ");
	         FormatedBody(pretty_minutes(hp->total/(60*hp->nepochs),buf));
	     CellEnd();
	RowEnd();
    }    

    RowBegin("");
       CellBegin("ALIGN=\"LEFT\"");
           FormatedBody("Least uptime");
       CellEnd();
       CellBegin("COLSPAN=4 ");
       	   FormatedBody("%s %s",pretty_minutes(hp->least_uptime/60,buf),(hp->thisepoch==hp->least_uptime)?"(current,epoch)":"");
       CellEnd();
    RowEnd();

    RowBegin("");
       CellBegin("ALIGN=\"LEFT\"");
	   FormatedBody("Most uptime");
       CellEnd();
       CellBegin("COLSPAN=4 ");
	   FormatedBody("%s %s",pretty_minutes(hp->most_uptime/60,buf),(hp->thisepoch==hp->most_uptime)?"(current,epoch)":"");
       CellEnd();
    RowEnd();

    if (hp->ndowns) {
	RowBegin("");
	   CellBegin("ALIGN=\"LEFT\"");
	       FormatedBody("Average downtime");
	   CellEnd();
	   CellBegin("COLSPAN=4 ");
	       FormatedBody(pretty_minutes((hp->possible_uptime-hp->total)/(60*hp->ndowns),buf));

           CellEnd();
	RowEnd();
	
	RowBegin("");
	   CellBegin("ALIGN=\"LEFT\"");
	       FormatedBody("Least downtime");
	   CellEnd();
	   CellBegin("COLSPAN=4 ");
	       FormatedBody("%s %s",pretty_minutes((hp->least_dntime+30)/60,buf),(hp->thisepoch==hp->least_dntime)?"(current,epoch)":"");
	   CellEnd();
	RowEnd();

	RowBegin("");
	   CellBegin("ALIGN=\"LEFT\"");
               FormatedBody("Most downtime");
	   CellEnd();
	   CellBegin("COLSPAN=4 ");
	       FormatedBody("%s %s",pretty_minutes((hp->most_dntime+30)/60,buf),(hp->thisepoch==hp->most_dntime)?"(current,epoch)":"");
	   CellEnd();
	RowEnd();
    }

    RowBegin("");
	CellBegin("ALIGN=\"LEFT\"");
	    FormatedBody("Logging started at");
	CellEnd();
	CellBegin("COLSPAN=4 ");
	    FormatedBody(ctime(&(hp->the_beginning)));
	CellEnd();
    RowEnd();
    
    RowBegin("");
	CellBegin("ALIGN=\"LEFT\"");
	    FormatedBody("Last boot at ");
	CellEnd();
	CellBegin("COLSPAN=4 ");
	    FormatedBody(ctime(&(hp->lastboot)));
	CellEnd();
    RowEnd();
    
    if (count_this_epoch){
	RowBegin("");
	    CellBegin("ALIGN=\"LEFT\"");
		FormatedBody("System has been up for");
	    CellEnd();
	    CellBegin("COLSPAN=4 ");
		FormatedBody(pretty_minutes(hp->thisepoch/60,buf));
	    CellEnd();
	RowEnd();
    }

    TableEnd();
    
    Body("<BR>\n");
    generate_H2_html(scb,hp,"Event Availability Information",0);
    Body("<BR>\n");

    Body("<P><HR><P>\n");
    
    
    } 
    else 
    { /* Lynx */ 
      char tmpbuf[1024];
    
    
FormatedBody("<pre>");
FormatedBody("  |----------------------------------------------------------------------|\n");
FormatedBody("  |                               |Count| Downtime |  MTBI  |Availability|\n");
FormatedBody("  |                               |     |   (min)  | (min)  |    (%%%)     |\n");
FormatedBody("  |-------------------------------|-----|----------|--------|------------|\n");

    for (i = 0; i < NSTATS; i++) 
    {
       if(!hp->stats[i])
	   continue;

       if(i == 0 || i == 6 || i == 15)
       {
         FormatedBody ( "  |%-31.31s|%5d|%10d|%8.0f|%12.2f|\n", 
                        statname_txt[i].sname,
                        hp->stats[i],
                        ((hp->stdntime[i]+30)/60),
                        (fpossible/(60*hp->stats[i])),
                        ((fpossible-hp->stdntime[i])/fpossible)*100.0 );
       } else {
         FormatedBody ( "  |%-31.31s|%5d|%10d|%8.0f|%12.12s|\n", 
                        statname_txt[i].sname,
                        hp->stats[i],
                        ((hp->stdntime[i]+30)/60),
                        (fpossible/(60*hp->stats[i])), "" );
       }
FormatedBody("  |----------------------------------------------------------------------|\n");
    }

    if(hp->nepochs)
    {
      FormatedBody ("  | Average uptime        |%-46.46s|\n", 
                     pretty_minutes(hp->total/(60*hp->nepochs),buf));
    }    

    snprintf ( tmpbuf, sizeof(tmpbuf), "%s %s", 
                      pretty_minutes(hp->least_uptime/60,buf),
                     (hp->thisepoch==hp->least_uptime)? "(current epoch)" : "" );
    FormatedBody   ("  | Least uptime          |%-46.46s|\n", tmpbuf );

    snprintf ( tmpbuf, sizeof(tmpbuf), "%s %s", 
                     pretty_minutes(hp->most_uptime/60,buf),
                     (hp->thisepoch==hp->most_uptime )? "(current epoch)" : "");
    FormatedBody   ("  | Most uptime           |%-46.46s|\n",  tmpbuf );

    if (hp->ndowns) 
    {
    
     FormatedBody  ("  | Average downtime      |%-46.46s|\n", 
                    pretty_minutes((hp->possible_uptime-hp->total)/(60*hp->ndowns),buf) );
                    
     snprintf ( tmpbuf, sizeof(tmpbuf), "%s %s", 
                    pretty_minutes((hp->least_dntime+30)/60,buf),
                    (hp->thisepoch==hp->least_dntime)? "(current epoch)":"" );
     FormatedBody  ("  | Least downtime        |%-46.46s|\n",  tmpbuf );
     
                    
     snprintf ( tmpbuf, sizeof(tmpbuf), "%s %s", 
                    pretty_minutes((hp->most_dntime+30)/60,buf),  
                    (hp->thisepoch==hp->most_dntime) ? "(current epoch)": "" );
     FormatedBody  ("  | Most downtime         |%-46.46s| \n",  tmpbuf );
    }

    snprintf ( tmpbuf, sizeof(tmpbuf), "%-26.26s", ctime(&(hp->the_beginning)));
    tmpbuf[24] = 0;
    FormatedBody   ("  | Logging started at    |%-46.46s| \n",  tmpbuf );
                      
    snprintf ( tmpbuf, sizeof(tmpbuf), "%-26.26s", ctime(&(hp->lastboot)));
    tmpbuf[24] = 0;
    FormatedBody   ("  | Last boot at          |%-46.46s| \n",  tmpbuf );
    
    if (count_this_epoch)
    {
    FormatedBody   ("  | System has been up for|%-46.46s|\n",
                      pretty_minutes(hp->thisepoch/60,buf) );
    }
FormatedBody("  ------------------------------------------------------------------------");
    Body("</pre>\n");
    generate_H2_html(scb,hp,"Event Availability Information",0);
    }
    generate_end_background(scb);

    return;
}

/*
 *----------------------------------------------------------------------------
 *		Function:	generate_H1				       
 *----------------------------------------------------------------------------
 */
static void generate_H1(session_t *scb, int argc, char **argv)
{
    hostinfo_t  *hp=NULL;
    char *title=
	"SYSTEM INFORMATION &gt; "
	"Availability &gt; "
	"Overall Availability";

    if(!argc)
    {
	hp=scb->hosts;                 /* Head of list */
	while (hp) 
	{
	    if(!strncasecmp(hp->sys_id, scb->local_sysid,MAX_SYSIDLEN))
	    {
		generate_begin_background( scb, title,0);
		generate_H1_host(scb,hp);
		generate_end_background ( scb );

		break;
	    }
	    else
		hp = hp->next;
	}
    }
    else
    {
	for(;argc;argc--)
	{
	    hp=scb->hosts;

	    while(hp)
	    {
		if (hp->sys_id && !strncasecmp(hp->sys_id,argv[argc-1],
					       MAX_SYSIDLEN))
		{
		    generate_begin_background(scb, title,0);
		    generate_H1_host(scb,hp);
		    generate_end_background(scb);
		    
		    break;
		}
		else
		    hp = hp->next;
	    }
	}

    }

    if(!hp)
	generate_error_page(scb, title,"No data available currently.");
}


/*
 *----------------------------------------------------------------------------
 *		Function:	generate_H2				      
 *----------------------------------------------------------------------------
 */

static void generate_H2(session_t *scb, int argc, char **argv)
{
  event_t *evp;
  hostinfo_t *hp;
  int evnt;
  char *title=
      "SYSTEM INFORMATION &gt; "
      "Availability &gt; "
      "Event Availability Information";
  int i;

  if(!scb)
    return;

  if(!argc)
  {
      hp=scb->hosts;                 /* Head of list */
      while (hp && strncasecmp(hp->sys_id, scb->local_sysid,MAX_SYSIDLEN)){
	      hp = hp->next;
      }
  }
  else
  {
      for(;argc;argc--)
      {
	  hp=scb->hosts;

	  while(hp)
	  {
	      if(hp->sys_id && !strncasecmp(hp->sys_id,argv[argc-1],
					    MAX_SYSIDLEN))
	      {
		  break;
	      }
	      else
		  hp=hp->next;
	  }
      }
  }
  
  if(!hp)
  {
      generate_error_page(scb, title,"No data available currently.");
      return;
  }

  evp  = hp->first_ev;

  generate_begin_background( scb, title,0);

  generate_header(scb,hp);

  if ( scb->textonly == 0 ) 
  {
  Body("<P><HR><P>\n");

  TableBegin("border=0 cellpadding=0 cellspacing=0");
  RowBegin("");
    CellBegin("align=right ");
	 FormatedBody("Page %d of %d",(scb->start_event_h2/EVENTS_PER_PAGE+1),((hp->num_total_events+(EVENTS_PER_PAGE-1))/EVENTS_PER_PAGE));
    CellEnd();
 RowEnd();
 RowBegin("");
    CellBegin("");

       TableBegin("BORDER=4 CELLSPACING=1 CELLPADDING=6 COLS=5 WIDTH=100% NOSAVE");
  
       /* Table Heading */
       RowBegin("ALIGN=\"CENTER\"");
         CellBegin("width=20%");
                 Body("<B>\n");
                 Body("Start Time\n");
         CellEnd();
         CellBegin("width=20%");
                 Body("<B>\n");
                 Body("Incident Time\n");
         CellEnd();
         CellBegin("width=10%");
                 Body("<B>\n");
                 Body("Uptime (min)\n");
         CellEnd();
         CellBegin("width=10%");
                 Body("<B>\n");
                 Body("DownTime (min)\n");
         CellEnd();
         CellBegin("width=20%");
                 Body("<B>\n");
                 Body("Reason\n");
         CellEnd();
	 CellBegin("width=20%");
	         Body("<B>\n");
	         Body("&nbsp\n");
         CellEnd();

       RowEnd();

  i=evnt=0;
  while(evp && evnt < scb->start_event_h2)
  {
      evp=evp->next;
      evnt++;
  }
  while(evp && i<EVENTS_PER_PAGE)
    { 
	i++;
	RowBegin("ALIGN=\"RIGHT\"");
	  CellBegin("");
	     FormatedBody(ctime(&(evp->prevstart)));
	  CellEnd();
	  CellBegin("");
	     FormatedBody(ctime(&(evp->event_time)));
	  CellEnd();
	  CellBegin("");
	     FormatedBody("%d",(evp->uptime+30)/60);
	  CellEnd();
	  CellBegin("");
	     FormatedBody("%d",(evp->dntime+30)/60);
	  CellEnd();
	  CellBegin("ALIGN=\"CENTER\"");
	     FormatedBody(getEventDescription(evp->ev_type,shortEventDescriptions));
	  CellEnd();
	  CellBegin("ALIGN=\"CENTER\"");
	  if (scb->Avail_startValue && scb->Avail_endValue)
	  {
	      FormatedBody("%s%d~%d?%s=%s&%s=%s&%s=%s %sEvent Summary%s",AMR_BEGIN_RGSTRING,AMR_H3,evnt++,szAvail_sysid,hp->sys_id,szAvail_start,scb->Avail_startValue,szAvail_end,scb->Avail_endValue,AMR_END_RGSTRING,AMR_END_HREF);
	  }
          else if (scb->Avail_startValue)
	  {
	      FormatedBody("%s%d~%d?%s=%s&%s=%s&%s=%s %sEvent Summary%s",AMR_BEGIN_RGSTRING,AMR_H3,evnt++,szAvail_sysid,hp->sys_id,szAvail_start,scb->Avail_startValue,szAvail_end,"0",AMR_END_RGSTRING,AMR_END_HREF);
	  }
          else if (scb->Avail_endValue)
	  {
	      FormatedBody("%s%d~%d?%s=%s&%s=%s&%s=%s %sEvent Summary%s",AMR_BEGIN_RGSTRING,AMR_H3,evnt++,szAvail_sysid,hp->sys_id,szAvail_start,"0",szAvail_end,scb->Avail_endValue,AMR_END_RGSTRING,AMR_END_HREF);
	  }
	  else
	  {
	      FormatedBody("%s%d~%d?%s=%s&%s=%s&%s=%s %sEvent Summary%s",AMR_BEGIN_RGSTRING,AMR_H3,evnt++,szAvail_sysid,hp->sys_id,szAvail_start,"0",szAvail_end,"0",AMR_END_RGSTRING,AMR_END_HREF);
	  }
	  CellEnd();
	RowEnd();
	evp = evp->next;
    }

      TableEnd();
    CellEnd();
  RowEnd();
  RowBegin("");
     CellBegin("align=center");
	   i=scb->start_event_h2/EVENTS_PER_PAGE+1;
           Body("<BR>\n");
           if(i>EVENTS_PER_PAGE)
	   {
	       /* 
		* put beginning arrows.
		* start event of previous page = (start_event_h2-EVENTS_PER_SET)/EVENTS_PER_SET*EVENTS_PER_SET
		*/
	       generate_H2_html(scb,hp,"<img src=\"/images/double_arrow_left.gif\" border=0 alt=\"First page\">",0);
	       Body("&nbsp;&nbsp;\n");
	       generate_H2_html(scb,hp,"<img src=\"/images/arrow_left.gif\" border=0 alt=\"Previous 10 pages\">",(scb->start_event_h2)/EVENTS_PER_SET*EVENTS_PER_SET-EVENTS_PER_PAGE);
	       Body("&nbsp;&nbsp;\n");
	   }
	   for(i=0;i<EVENTS_PER_PAGE;i++)
	   {
	       char buf[1024];
	       /* 
		* put the page number.
		*/
	       if((scb->start_event_h2/EVENTS_PER_PAGE+1)%EVENTS_PER_PAGE == (i+1)%EVENTS_PER_PAGE)
	       {
		   FormatedBody("<b><font color=\"#cc6633\">%d</b>",(scb->start_event_h2)/EVENTS_PER_SET*EVENTS_PER_PAGE+i+1);
		   Body("&nbsp;&nbsp;			\n");
	       }
	       else
	       {
		   sprintf(buf,"<b>%d</b>",
			   (scb->start_event_h2)/EVENTS_PER_SET*EVENTS_PER_PAGE+i+1);
		   generate_H2_html(scb,hp,buf,
				    (i*EVENTS_PER_PAGE+(scb->start_event_h2/EVENTS_PER_SET*EVENTS_PER_SET)));
		   Body("&nbsp;&nbsp;\n");
	       }
	       if((scb->start_event_h2/EVENTS_PER_SET*EVENTS_PER_PAGE+i+1)*EVENTS_PER_PAGE>(hp->num_total_events-1))
	       {
		   break;
	       }
	   }
	   if(((scb->start_event_h2+EVENTS_PER_SET)/EVENTS_PER_SET*EVENTS_PER_SET)<(hp->num_total_events))
	   {
	       /*
		* put ending arrows.
		*/
	       generate_H2_html(scb,hp,"<img src=\"/images/arrow_right.gif\" border=0 alt=\"Next 10 pages\">",
				(scb->start_event_h2+EVENTS_PER_SET)/EVENTS_PER_SET*EVENTS_PER_SET);
	       Body("&nbsp;&nbsp;\n");
	       generate_H2_html(scb,hp,"<img src=\"/images/double_arrow_right.gif\" border=0 alt=\"Last page\">",
				(hp->num_total_events-1)/EVENTS_PER_PAGE*EVENTS_PER_PAGE);
	       Body("&nbsp;&nbsp;\n");
	   }
     CellEnd();
  RowEnd();

  TableEnd();

  Body("<P><HR><P>\n");
  
  } 
  else 
  {
    char linkbuf[1024];
    char timebuf[32];
  
    /* Lynx */
    
    FormatedBody("<p><pre>   Page %d of %d\n",(scb->start_event_h2/EVENTS_PER_PAGE+1),((hp->num_total_events+(EVENTS_PER_PAGE-1))/EVENTS_PER_PAGE));
    FormatedBody("|-----------------------------------------------------------------------------|\n");
    FormatedBody("|              Time                 |Uptime|DownTime|      Reason     |       |\n");
    FormatedBody("|                                   | (min)|(min)   |                 |       |\n");
    FormatedBody("|-----------------------------------------------------------------------------|\n");


    i=evnt=0;
    while(evp && evnt < scb->start_event_h2)
    {
       evp=evp->next;
       evnt++;
    }
    while(evp && i<EVENTS_PER_PAGE)
    { 
	i++;

	if (scb->Avail_startValue && scb->Avail_endValue)
        {
	  snprintf ( linkbuf, sizeof(linkbuf), 
	             "%s%d~%d?%s=%s&%s=%s&%s=%s %sSummary%s",
	             AMR_BEGIN_RGSTRING,
	             AMR_H3,
	             evnt++,
	             szAvail_sysid, 
	             hp->sys_id,
	             szAvail_start,
	             scb->Avail_startValue,
	             szAvail_end,
	             scb->Avail_endValue,
	             AMR_END_RGSTRING, 
	             AMR_END_HREF );
	}
        else if (scb->Avail_startValue)
	{
	  snprintf ( linkbuf, sizeof(linkbuf), 
	             "%s%d~%d?%s=%s&%s=%s&%s=%s %sSummary%s", 
	             AMR_BEGIN_RGSTRING,
	             AMR_H3,
	             evnt++,
	             szAvail_sysid, 
	             hp->sys_id, 
	             szAvail_start, 
	             scb->Avail_startValue, 
	             szAvail_end,
	             "0",
	             AMR_END_RGSTRING, 
	             AMR_END_HREF );
	}
        else if (scb->Avail_endValue)
	{
	  snprintf ( linkbuf, sizeof(linkbuf), 
	             "%s%d~%d?%s=%s&%s=%s&%s=%s %sSummary%s",
	             AMR_BEGIN_RGSTRING,
	             AMR_H3,
	             evnt++,
	             szAvail_sysid,
	             hp->sys_id,
	             szAvail_start,
	             "0",
	             szAvail_end,
	             scb->Avail_endValue,
	             AMR_END_RGSTRING,
	             AMR_END_HREF );
	}
	else
	{
	  snprintf ( linkbuf, sizeof(linkbuf), 
	             "%s%d~%d?%s=%s&%s=%s&%s=%s %sSummary%s",
	              AMR_BEGIN_RGSTRING,
	              AMR_H3,
	              evnt++,
	              szAvail_sysid,
	              hp->sys_id,
	              szAvail_start,
	              "0",
	              szAvail_end,
	              "0",
	              AMR_END_RGSTRING,
	              AMR_END_HREF );
	}
	
        snprintf ( timebuf, sizeof(timebuf), "%-26.26s", ctime(&(evp->prevstart ))); timebuf[24] = 0;
        FormatedBody ("|Start   : %-25.25s|%6d|%8d|%-17.17s|%s|\n",
	               timebuf,
 	               (evp->uptime+30)/60,
	               (evp->dntime+30)/60,
	               getEventDescription(evp->ev_type,shortEventDescriptions),
	               linkbuf);
	               
        snprintf ( timebuf, sizeof(timebuf), "%-26.26s", ctime(&(evp->event_time))); timebuf[24] = 0;
        FormatedBody ("|Incident: %-25.25s|      |        |                 |       |\n",
                       timebuf ); 
        FormatedBody("|-----------------------------------------------------------------------------|\n");
	evp = evp->next;
    }
    
    i=scb->start_event_h2/EVENTS_PER_PAGE+1;
  
    Body("</pre>\n");
         if(i>EVENTS_PER_PAGE)
	   {
	       /* 
		* put beginning arrows.
		* start event of previous page = (start_event_h2-EVENTS_PER_SET)/EVENTS_PER_SET*EVENTS_PER_SET
		*/
	       generate_H2_html(scb,hp,"\"Previous 10 pages\"", (scb->start_event_h2)/EVENTS_PER_SET*EVENTS_PER_SET-EVENTS_PER_PAGE);
	       Body("&nbsp;&nbsp;\n");
	       generate_H2_html(scb,hp,"\"First page\">",0);
	       Body("&nbsp;&nbsp;\n");
	   }
	   for(i=0;i<EVENTS_PER_PAGE;i++)
	   {
	       char buf[1024];
	       /* 
		* put the page number.
		*/
	       if((scb->start_event_h2/EVENTS_PER_PAGE+1)%EVENTS_PER_PAGE == (i+1)%EVENTS_PER_PAGE)
	       {
		   FormatedBody("%d",(scb->start_event_h2)/EVENTS_PER_SET*EVENTS_PER_PAGE+i+1);
		   Body("&nbsp;&nbsp;			\n");
	       }
	       else
	       {
		   sprintf(buf,"<b>%d</b>",
			   (scb->start_event_h2)/EVENTS_PER_SET*EVENTS_PER_PAGE+i+1);
		   generate_H2_html(scb,hp,buf,
				    (i*EVENTS_PER_PAGE+(scb->start_event_h2/EVENTS_PER_SET*EVENTS_PER_SET)));
		   Body("&nbsp;&nbsp;\n");
	       }
	       if((scb->start_event_h2/EVENTS_PER_SET*EVENTS_PER_PAGE+i+1)*EVENTS_PER_PAGE>(hp->num_total_events-1))
	       {
		   break;
	       }
	   }
	   if(((scb->start_event_h2+EVENTS_PER_SET)/EVENTS_PER_SET*EVENTS_PER_SET)<(hp->num_total_events))
	   {
	       /*
		* put ending arrows.
		*/
	       generate_H2_html(scb,hp,"\"Last page\"",
				(hp->num_total_events-1)/EVENTS_PER_PAGE*EVENTS_PER_PAGE);
	       Body("&nbsp;&nbsp;\n");
	       generate_H2_html(scb,hp,"\"Next 10 pages\"",
				(scb->start_event_h2+EVENTS_PER_SET)/EVENTS_PER_SET*EVENTS_PER_SET);
	   }
  } /* End of Lynx Stuff */

  generate_end_background(scb);

  return;
}

/*
 *--------------------------------------------------------------------
 *		Function:	generate_H3				       
 *--------------------------------------------------------------------
 */
static void generate_H3(session_t *scb,int evnt,int argc, char **argv)
{
  event_t *evp;
  char buf[QUERY_MAX_SZ];
  hostinfo_t *hp;

  if(!argc)
      return;
  
  if(!argc)
  {
      hp=scb->hosts;                 /* Head of list */
      while (hp && strncasecmp(hp->sys_id, scb->local_sysid,MAX_SYSIDLEN)){
	      hp = hp->next;
      }
  }
  else
  {
      for(;argc;argc--)
      {
	  hp=scb->hosts;

	  while(hp)
	  {
	      if(hp->sys_id && !strncasecmp(hp->sys_id,argv[argc-1],
					    MAX_SYSIDLEN))
	      {
		  break;
	      }
	      else
		  hp=hp->next;
	  }
      }
  }
  

  if(!hp)
    return;

  evp  = hp->first_ev;
  while(evp && evnt-- > 0)
  {
      evp = evp->next;
  }


  if(!scb || !evp)
    return;

  generate_begin_background(scb, 
                            "SYSTEM INFORMATION &gt; "
			    "Availability &gt; "
			    "Event Summary Information",0);

  if ( scb->textonly == 0 )
  {
  Body("<P><HR><P>\n");

  TableBegin("BORDER=0 CELLSPACING=0 CELLPADDING=0");
     RowBegin("");
       CellBegin("");
          Body("Internet address \n");
       CellEnd();
       CellBegin("");
	  Body("&nbsp;&nbsp;&nbsp;&nbsp;:&nbsp;\n");
       CellEnd();/* this  ^^ is intentional. */
       CellBegin("");
          FormatedBody(hp->hostname);
       CellEnd();
     RowEnd();
     
     RowBegin("");
       CellBegin("");
          Body("Reason for shutdown\n");
       CellEnd();
       CellBegin("");
	  Body("&nbsp;&nbsp;&nbsp;&nbsp;:&nbsp;\n");
       CellEnd();/* this  ^^ is intentional. */
       CellBegin("");
	  FormatedBody(getEventDescription(evp->ev_type,EventDescriptions));
       CellEnd();
     RowEnd();
     
     RowBegin("");
       CellBegin("");
	  Body("Start time\n");
       CellEnd();
       CellBegin("");
	  Body("&nbsp;&nbsp;&nbsp;&nbsp;:&nbsp;\n");
       CellEnd();/* this  ^^ is intentional. */
       CellBegin("");
	  FormatedBody(ctime(&(evp->prevstart)));
       CellEnd();
     RowEnd();

     RowBegin("");
       CellBegin("");
	  Body("Incident time\n");
       CellEnd();
       CellBegin("");
	  Body("&nbsp;&nbsp;&nbsp;&nbsp;:&nbsp;\n");
       CellEnd();/* this  ^^ is intentional. */
       CellBegin("");
          FormatedBody(ctime(&(evp->event_time)));
       CellEnd();
     RowEnd();

     RowBegin("");
       CellBegin("");
          Body("Re-start time\n");
       CellEnd();
       CellBegin("");
	  Body("&nbsp;&nbsp;&nbsp;&nbsp;:&nbsp;\n");
       CellEnd();/* this  ^^ is intentional. */
       CellBegin("");
	  if(evp->start_time)
  	      FormatedBody(ctime(&(evp->start_time)));
	  else
	      FormatedBody(ctime(&(evp->prevstart)));
       CellEnd();
     RowEnd();

     RowBegin("");
       CellBegin("");
          Body("Uptime\n");
       CellEnd();
       CellBegin("");
	  Body("&nbsp;&nbsp;&nbsp;&nbsp;:&nbsp;\n");
       CellEnd();/* this  ^^ is intentional. */
       CellBegin("");
          FormatedBody(pretty_minutes((evp->uptime+30)/60,buf));
       CellEnd();
     RowEnd();

     RowBegin("");
       CellBegin("");
	  Body("Downtime\n");
       CellEnd();
       CellBegin("");
	  Body("&nbsp;&nbsp;&nbsp;&nbsp;:&nbsp;\n");
       CellEnd();/* this  ^^ is intentional. */
       CellBegin("");
          FormatedBody(pretty_minutes((evp->dntime+30)/60,buf));
       CellEnd();
     RowEnd();
  TableEnd();

  Body("<P><HR><P>\n");
  } 
  else
  {
    /* Lynx */
   FormatedBody("<pre>   Internet address     : %s</pre>",hp->hostname);
   FormatedBody("<pre>   Reason for shutdown  : %s</pre>",getEventDescription(evp->ev_type,EventDescriptions));
   FormatedBody("<pre>   Start time           : %s</pre>",ctime(&(evp->prevstart)));
   FormatedBody("<pre>   Incident time        : %s</pre>",ctime(&(evp->event_time)));
   FormatedBody("<pre>   Re-start time        : %s</pre>",((evp->start_time)?ctime(&(evp->start_time)):ctime(&(evp->prevstart))));
   FormatedBody("<pre>   Uptime               : %s</pre>",pretty_minutes((evp->uptime+30)/60,buf));
   FormatedBody("<pre>   Downtime             : %s</pre>",pretty_minutes((evp->dntime+30)/60,buf));
  }

  generate_end_background(scb);
}

/*
 *----------------------------------------------------------------------------
 *		Function:	generate_S1				       
 *----------------------------------------------------------------------------
 */
static void generate_S1(session_t *scb,int argc,char **argv)
{
    int		i;
    aggregate_stats_t            *agstat; 
    double	ftotal, fpossible;
    char	temp[MAXHOSTNAMELEN+64], temp1[MAXHOSTNAMELEN+64];
    hostinfo_t *hp;
    char buf[QUERY_MAX_SZ];
    char *title=
	"SYSTEM INFORMATION &gt; "
	"Availability &gt; "
	"Group Overall Availability";

    if(!scb)
    {
	generate_error_page(scb, title,"No data available currently.");
	return;
    }
    hp=scb->hosts;
    if(!hp)
    {
	generate_error_page(scb, title,"No data available currently.");
	return;
    }

    gather_aggregate_stats(scb,argc,argv);

    agstat = scb->aggr_stats;
    if(!agstat)
    {
	generate_error_page(scb, title,"No data available currently.");
	return;
    }

    ftotal = agstat->stuptime[15];         /* total uptime */
    fpossible = agstat->possible_uptime;
    
    generate_begin_background(scb, title,0);

    generate_group_header(scb,argc,argv);

    if ( scb->textonly == 0 )
    {
    Body("<P><HR><P>\n");

    TableBegin("BORDER=4 CELLSPACING=1 CELLPADDING=6 COLS=5 WIDTH=100% NOSAVE");

	 /* Table Heading */
	 RowBegin("ALIGN=\"CENTER\"");
	   CellBegin("width=35%");
	          Body("&nbsp;\n");
	   CellEnd();
	   CellBegin("width=15%");
	           Body("<B>\n");
	           Body("Count\n");
	   CellEnd();
	   CellBegin("width=15%");
	           Body("<B>\n");
	           Body("Downtime\n");
	           Body("<BR>\n");
	           Body("(min)\n");
	   CellEnd();
	   CellBegin("width=15%");
	           Body("<B>\n");
	           Body("MTBI\n");
	           Body("<BR>\n");
	           Body("(min)\n");
	   CellEnd();
	   CellBegin("width=20%");
	           Body("<B>\n");
	           Body("Availability\n");
	           Body("<BR>\n");
	           Body("%\n");
	   CellEnd();
	 RowEnd();

    for (i = 0; i < NSTATS; i++) {
	if(!agstat->stats[i])
	   continue;

        RowBegin("");
	   CellBegin("ALIGN=\"LEFT\"");
	       FormatedBody(statname[i].sname);
	   CellEnd();
	   CellBegin("ALIGN=\"CENTER\"");
	       FormatedBody("%5d",agstat->stats[i]);
	   CellEnd();
	   CellBegin("ALIGN=\"CENTER\"");
	       FormatedBody("%6d",(agstat->stdntime[i]/60));
	   CellEnd();
	   CellBegin("ALIGN=\"CENTER\"");
	       FormatedBody("%7.0f",(fpossible/(60*agstat->stats[i])));
	   CellEnd();
	   CellBegin("ALIGN=\"CENTER\"");
               if(i == 0 || i == 6 || i == 15)
		 FormatedBody("%3.2f",((fpossible-agstat->stdntime[i])/fpossible)*100.0);
	       else
		 Body("&nbsp;\n");
	   CellEnd();
	RowEnd();
    }

    if (agstat->nepochs)
    {
        RowBegin("");
           CellBegin("ALIGN=\"LEFT\"");
               FormatedBody("Average uptime");
           CellEnd();
           CellBegin("COLSPAN=4 ");
        	   FormatedBody(pretty_minutes(agstat->total/(60*agstat->nepochs),buf));
           CellEnd();
        RowEnd();
    }

    RowBegin("");
       CellBegin("ALIGN=\"LEFT\"");
           FormatedBody("Least uptime");
       CellEnd();
       CellBegin("COLSPAN=4 ");
       	   FormatedBody("%s %s",pretty_minutes(agstat->least_uptime/60,buf),(agstat->thisepoch==agstat->least_uptime)?"(current,epoch)":"");
       CellEnd();
    RowEnd();

    RowBegin("");
       CellBegin("ALIGN=\"LEFT\"");
           FormatedBody("&nbsp;&nbsp;&nbsp;Recorded at:");
       CellEnd();
       CellBegin("COLSPAN=4 ");
	   if(agstat->upleasthost)
	       FormatedBody(agstat->upleasthost);
	   else
	       FormatedBody("");
       CellEnd();
    RowEnd();

    RowBegin("");
       CellBegin("ALIGN=\"LEFT\"");
	   FormatedBody("Most uptime");
       CellEnd();
       CellBegin("COLSPAN=4 ");
	   FormatedBody("%s %s",pretty_minutes(agstat->most_uptime/60,buf),(agstat->thisepoch==agstat->most_uptime)?"(current,epoch)":"");
       CellEnd();
    RowEnd();

    RowBegin("");
       CellBegin("ALIGN=\"LEFT\"");
           FormatedBody("&nbsp;&nbsp;&nbsp;Recorded at:");
       CellEnd();
       CellBegin("COLSPAN=4 ");
	   if(agstat->upmosthost)
	       FormatedBody(agstat->upmosthost);
	   else
	       FormatedBody("");
       CellEnd();
    RowEnd();

    if (agstat->ndowns) {
	RowBegin("");
	   CellBegin("ALIGN=\"LEFT\"");
	       FormatedBody("Average downtime");
	   CellEnd();
	   CellBegin("COLSPAN=4 ");
	       FormatedBody(pretty_minutes((agstat->possible_uptime-agstat->total)/(60*agstat->ndowns),buf));
	RowEnd();
	
	RowBegin("");
	   CellBegin("ALIGN=\"LEFT\"");
	       FormatedBody("Least downtime");
	   CellEnd();
	   CellBegin("COLSPAN=4 ");
	       FormatedBody("%s %s",pretty_minutes(agstat->least_dntime/60,buf),(agstat->thisepoch==agstat->least_dntime)?"(current,epoch)":"");
	   CellEnd();
	RowEnd();

        RowBegin("");
           CellBegin("ALIGN=\"LEFT\"");
               FormatedBody("&nbsp;&nbsp;&nbsp;Recorded at:");
           CellEnd();
           CellBegin("COLSPAN=4 ");
	       if(agstat->dnleasthost)
		   FormatedBody(agstat->dnleasthost);
	       else
		   FormatedBody("");
           CellEnd();
        RowEnd();

	RowBegin("");
	   CellBegin("ALIGN=\"LEFT\"");
               FormatedBody("Most downtime");
	   CellEnd();
	   CellBegin("COLSPAN=4 ");
	       FormatedBody("%s %s",pretty_minutes(agstat->most_dntime/60,buf),(agstat->thisepoch==agstat->most_dntime)?"(current,epoch)":"");
	   CellEnd();
	RowEnd();

        RowBegin("");
           CellBegin("ALIGN=\"LEFT\"");
               FormatedBody("&nbsp;&nbsp;&nbsp;Recorded at:");
           CellEnd();
           CellBegin("COLSPAN=4 ");
	       if(agstat->dnmosthost)
		   FormatedBody(agstat->dnmosthost);
	       else
		   FormatedBody("");
           CellEnd();
        RowEnd();
    }

    TableEnd();

    Body("<BR>\n");
    FormatedBody("%s%d",AMR_BEGIN_RGSTRING,AMR_S2);
    if(scb->Avail_startValue && scb->Avail_endValue)
    {
	FormatedBody("?%s=%s&%s=%s",szAvail_start,scb->Avail_startValue,szAvail_end,scb->Avail_endValue);
    }
    else if (scb->Avail_startValue)
    {
	FormatedBody("?%s=%s&%s=%s",szAvail_start,scb->Avail_startValue,szAvail_end,"0");
    }
    else if (scb->Avail_endValue)
    {
	FormatedBody("?%s=%s&%s=%s",szAvail_start,"0",szAvail_end,scb->Avail_endValue);
    }
    else
    {
	FormatedBody("?%s=%s&%s=%s",szAvail_start,"0",szAvail_end,"0");
    }
    
    for(i=0;i<argc;i++)
    {
	FormatedBody("&sys_id=%s",argv[i]);
    }

    FormatedBody("%sAvailability Summary For All Hosts%s",AMR_END_RGSTRING,AMR_END_HREF);
    Body("<BR>\n");

    Body("<P><HR><P>\n");
    } 
    else 
    { /* Lynx staff */

    Body("\"<pre>\"\n");
    Body("\"  |----------------------------------------------------------------------|\n\"\n");
    Body("\"  |                               |Count| Downtime |  MTBI  |Availability|\n\"\n");
    Body("\"  |                               |     |   (min)  | (min)  |    (%)     |\n\"\n");
    Body("\"  |-------------------------------|-----|----------|--------|------------|\n\"\n");

    
    for (i = 0; i < NSTATS; i++) 
    {
	if(!agstat->stats[i])
	   continue;

       if(i == 0 || i == 6 || i == 15)
       {
         FormatedBody ( "  |%-31.31s|%5d|%10d|%7.0f|%12.2f|\n", 
                        statname_txt[i].sname,
                        agstat->stats[i],
                        (agstat->stdntime[i]/60),
                        (fpossible/(60*agstat->stats[i])),
                        ((fpossible-agstat->stdntime[i])/fpossible)*100.0 );
       } else {
         FormatedBody ( "  |%-31.31s|%5d|%10d|%7.0f|%12.12s|\n", 
                        statname_txt[i].sname,
                        agstat->stats[i],
                        (agstat->stdntime[i]/60),
                        (fpossible/(60*agstat->stats[i])), "" );
       }
Body("\"  |----------------------------------------------------------------------|\n\"\n");
    }
    



    if (agstat->nepochs)
    {
      FormatedBody ( "  | Average uptime                | %38.38s|\n", 
                     pretty_minutes(agstat->total/(60*agstat->nepochs),buf));
    }    

    FormatedBody   ( "  | Least uptime                  | %s %s |\n", 
                     pretty_minutes(agstat->least_uptime/60,buf),
                     (agstat->thisepoch==agstat->least_uptime)?"(current epoch)":"");


    FormatedBody   ( "  | Recorded at                   | %38.38s|\n", 
                        (agstat->upleasthost)? agstat->upleasthost : "" );


    FormatedBody ( "<pre>   Most uptime  %s %s</pre>", pretty_minutes(agstat->most_uptime/60,buf),
                                                      (agstat->thisepoch==agstat->most_uptime)?"(current epoch)":"");


    FormatedBody   ( "  | Recorded at                   | %38.38s|\n", 
                        (agstat->upmosthost) ? agstat->upmosthost : "" );


    if (agstat->ndowns) 
    {
    
     FormatedBody ("  | Average downtime              | %38.38s|\n", 
                    pretty_minutes((agstat->possible_uptime-agstat->total)/(60*agstat->ndowns),buf) );

                    
     FormatedBody ("  | Least downtime                | %s %s |\n", 
                    pretty_minutes(agstat->least_dntime/60,buf),
                    (agstat->thisepoch==agstat->least_dntime)?"(current epoch)":"" );
                    

     FormatedBody   ( "  | Recorded at                   | %38.38s|\n", 
                        (agstat->dnleasthost) ? agstat->dnleasthost : "" );

                    
     FormatedBody ("  | Most downtime                 | %s %s |\n", 
                    pretty_minutes(agstat->most_dntime/60,buf),  
                    (agstat->thisepoch==agstat->most_dntime)?"(current epoch)":"" );


     FormatedBody   ( "  | Recorded at                   | %38.38s|\n", 
                        (agstat->dnmosthost) ? agstat->dnmosthost : "" );
    }
    Body("\"  ------------------------------------------------------------------------\"\n");
    Body("</pre><BR>\n");

    FormatedBody("%s%d",AMR_BEGIN_RGSTRING,AMR_S2);
    if(scb->Avail_startValue && scb->Avail_endValue)
    {
	FormatedBody("?%s=%s&%s=%s",szAvail_start,scb->Avail_startValue,szAvail_end,scb->Avail_endValue);
    }
    else if (scb->Avail_startValue)
    {
	FormatedBody("?%s=%s&%s=%s",szAvail_start,scb->Avail_startValue,szAvail_end,"0");
    }
    else if (scb->Avail_endValue)
    {
	FormatedBody("?%s=%s&%s=%s",szAvail_start,"0",szAvail_end,scb->Avail_endValue);
    }
    else
    {
	FormatedBody("?%s=%s&%s=%s",szAvail_start,"0",szAvail_end,"0");
    }
    
    for(i=0;i<argc;i++)
    {
	FormatedBody("&sys_id=%s",argv[i]);
    }

    FormatedBody("%sAvailability Summary For All Hosts%s",AMR_END_RGSTRING,AMR_END_HREF);
    Body("<BR><P><HR><P>\n");
    }

    generate_end_background(scb);

    return;
}


/*
 *----------------------------------------------------------------------------
 *		Function:	generate_S2				      
 *----------------------------------------------------------------------------
 */
static void generate_S2(session_t *scb,int num_ids,char **sysids)
{
    int		i;
    aggregate_stats_t            *agstat; 
    double	ftotal, fpossible;
    hostinfo_t *hp;
    char *title=
	"SYSTEM INFORMATION &gt; "
	"Availability &gt; "
	"Availability Summary For All Hosts";

    if(!scb)
    {
	generate_error_page(scb, title,"No data available currently.");
	return;
    }

    hp=scb->hosts;
    if(!hp)
    {
	generate_error_page(scb, title,"No data available currently.");
	return;
    }

    gather_aggregate_stats(scb,num_ids,sysids);

    agstat = scb->aggr_stats;
    if(!agstat)
    {
	generate_error_page(scb, title,"No data available currently.");
	return;
    }


    /* the result_data should be cast to whatever format the real */
    /* report data structure would take, this is just for testing */
    /* a sample of calculations. Change this later.    		  */

    generate_begin_background(scb, title,0);

    generate_group_header(scb,num_ids,sysids);

    if ( scb->textonly == 0 )
    {
    Body("<P><HR><P>\n");

    TableBegin("BORDER=4 CELLSPACING=1 CELLPADDING=6 COLS=8 WIDTH=100% NOSAVE");

	 /* Table Heading */
	 RowBegin("ALIGN=\"CENTER\"");
	   CellBegin("width=25%");
	          Body("<B>\n");
	          Body("Serial Number\n");
	   CellEnd();
	   CellBegin("width=20%");
	           Body("<B>\n");
	           Body("Hostname\n");
	   CellEnd();
	   CellBegin("width=15% COLSPAN=2");
	           Body("<B>\n");
	           Body("Unscheduled\n");
	   CellEnd();
	   CellBegin("width=15% COLSPAN=2");
	           Body("<B>\n");
	           Body("Service\n");
	   CellEnd();
	   CellBegin("width=15% COLSPAN=2");
	           Body("<B>\n");
	           Body("Total\n");
	   CellEnd();
	   CellBegin("width=25% ");
	           Body("<B>\n");
	           Body("&nbsp\n");
	   CellEnd();
	 RowEnd();

	while(hp)
	{
	    if(num_ids && sysids && !exist_sysid(hp->sys_id,num_ids,sysids))
	    {
		hp=hp->next;
		continue;
	    }
	    
	    ftotal = hp->stuptime[15];         /* total uptime */
	    fpossible = hp->possible_uptime;

	    RowBegin("");
		 CellBegin("ALIGN=\"CENTER\"");
		     FormatedBody(hp->sys_id);
		 CellEnd();
		 CellBegin("ALIGN=\"CENTER\"");
		     FormatedBody(hp->hostname);
		 CellEnd();
		 CellBegin("ALIGN=\"CENTER\"");
		     FormatedBody("%6d",hp->stats[0]);
		 CellEnd();
		 CellBegin("ALIGN=\"CENTER\"");
		     FormatedBody("%3.2f%%",((fpossible-hp->stdntime[0])/fpossible)*100.0);
		 CellEnd();
		 CellBegin("ALIGN=\"CENTER\"");
		     FormatedBody("%6d",hp->stats[6]);
		 CellEnd();
		 CellBegin("ALIGN=\"CENTER\"");
		     FormatedBody("%3.2f%%",((fpossible-hp->stdntime[6])/fpossible)*100.0);
		 CellEnd();
		 CellBegin("ALIGN=\"CENTER\"");
		     FormatedBody("%6d",hp->stats[15]);
		 CellEnd();
		 CellBegin("ALIGN=\"CENTER\"");
		     FormatedBody("%3.2f%%",((fpossible-hp->stdntime[15])/fpossible)*100.0);
		 CellEnd();
		 CellBegin("ALIGN=\"CENTER\"");
		     FormatedBody("%s%d?%s=%s",AMR_BEGIN_RGSTRING,AMR_H1,szAvail_sysid,hp->sys_id);
		 if(scb->Avail_startValue && scb->Avail_endValue)
		 {
		     FormatedBody("&%s=%s&%s=%s",szAvail_start,scb->Avail_startValue,szAvail_end,scb->Avail_endValue);
		 }
		 else if (scb->Avail_startValue)
		 {
		     FormatedBody("&%s=%s&%s=%s",szAvail_start,scb->Avail_startValue,szAvail_end,"0");
		 }
		 else if (scb->Avail_endValue)
		 {
		     FormatedBody("&%s=%s&%s=%s",szAvail_start,"0",szAvail_end,scb->Avail_endValue);
		 }
		 else
		 {
		     FormatedBody("&%s=%s&%s=%s",szAvail_start,"0",szAvail_end,"0");
		 }

	         FormatedBody("%sHost Overall Availability%s",AMR_END_RGSTRING,AMR_END_HREF);

		 CellEnd();
	     RowEnd();
	     hp=hp->next;
	 }
    TableEnd();

    Body("<P><HR><P>\n");
    } 
    else
    {     
        /* Lynx */
        Body("Must be implemented\n");
    }

    generate_end_background(scb);
    
    return;
}

/* --------------------------- rgpgInit ---------------------------------- */
int RGPGAPI rgpgInit(sscErrorHandle hError)
{ /* Try initialize "free list of session_t" mutex */
  if(pthread_mutex_init(&seshFreeListmutex,0))
  { sscError(hError,szServerNameErrorPrefix,"Can't initialize mutex");
    return 0; /* error */
  }
  mutex_inited++;
#ifdef INCLUDE_TIME_DATE_STRINGS
  sprintf(szVersionStr,"%d.%d %s %s",
	  MYVERSION_MAJOR,MYVERSION_MINOR,__DATE__,__TIME__);
#else
  sprintf(szVersionStr,"%d.%d",MYVERSION_MAJOR,MYVERSION_MINOR);
#endif
  return 1; /* success */
}

/* --------------------------- rgpgDone ---------------------------------- */
int RGPGAPI rgpgDone(sscErrorHandle hError)
{ 
    session_t *s;

    if(mutex_inited)
    { 
	pthread_mutex_destroy(&seshFreeListmutex);
	mutex_inited = 0;
	while((s = sesFreeList) != 0)
	{ 
	    sesFreeList = s->next;
	    free(s);
	}
	return 1; /* success */
    }
    return 0; /* error - not inited */
}

/*---------------rgpgCreateSesion---------*/

rgpgSessionID RGPGAPI rgpgCreateSesion(sscErrorHandle hError)
{
    session_t	*sptr;
    request_t        amr_req;
    int rv;

    /*                                              */
    /* Create a new report session control block    */
    /*                                              */
    pthread_mutex_lock(&seshFreeListmutex);
    if((sptr = sesFreeList) != 0) sesFreeList = sptr->next;
    pthread_mutex_unlock(&seshFreeListmutex);
    if(!sptr) sptr = (session_t *)calloc(sizeof(session_t),1);
    if(!sptr)
    {
	sscError(hError,szServerNameErrorPrefix,
		 "No memory to create session in rgpgCreateSession()");
	return 0;
    }
    memset(sptr,0,sizeof(session_t));
    sptr->signature = sizeof(session_t);
				
    return sptr;
}

/*-----------------rgpgDeleteSesion--------------------*/

void RGPGAPI rgpgDeleteSesion(sscErrorHandle hError, rgpgSessionID session)
{
    FreeEveryThing((session_t *)session);
    free(session);
}

/* ------------------- rgpgFreeAttributeString ---------------------------- */
void RGPGAPI rgpgFreeAttributeString(sscErrorHandle hError,
				     rgpgSessionID session,
				     const char *attributeID,
				     const char *extraAttrSpec,
				     char *attrString,int attrtype)
{  
    session_t *sess = (session_t *)session;
    
    if(!sess || sess->signature != sizeof(session_t))
    { 
	sscError(hError,szServerNameErrorPrefix,
		 "Incorrect session id in rgpgFreeAttributeString()");
	return;
    }
    if ((attrtype == RGATTRTYPE_SPECIALALLOC || 
	 attrtype == RGATTRTYPE_MALLOCED) && attrString) free(attrString);
}


/* 
 * ------------------------ rgpgGetAttribute ---------------------------------
 */
char *RGPGAPI rgpgGetAttribute(sscErrorHandle hError,rgpgSessionID session, const char *attributeID, const char *extraAttrSpec,int *typeattr)
{  
    char *s=NULL;

    if(typeattr) *typeattr = RGATTRTYPE_STATIC;
    if(!strcasecmp(attributeID,szVersion))         
	s = (char*)szVersionStr;
    else if(!strcasecmp(attributeID,szTitle))      
	s = (char*)myLogo;
    else if(!strcasecmp(attributeID,szUnloadTime)) 
	s = (char*)szUnloadTimeValue;
    else if(!strcasecmp(attributeID,szThreadSafe)) 
	s = (char*)szThreadSafeValue;
    else if(!strcasecmp(attributeID,szUnloadable)) 
	s = (char*)szUnloadableValue;
    else if(!strcasecmp(attributeID,szAcceptRawCmdString)) 
	s = (char*)szAcceptRawCmdStringValue;
    else
    { 
	sscError(hError,szServerNameErrorPrefix,
		 "%s No such attribute '%s' in rgpgGetAttribute()",
		 szServerNameErrorPrefix,attributeID);
	return 0;
    }

   if(!s) 
       sscError(hError,szServerNameErrorPrefix,
		szServerNameErrorPrefix,"No memory in rgpgGetAttribute()");
   return s;
}
/*-----------------rgpgSetAttribute--------------------*/
void  RGPGAPI rgpgSetAttribute(sscErrorHandle hError, rgpgSessionID session, 
			       const char *attributeID, 
			       const char *extraAttrSpec, const char *value)
{  
    char buff[128]; 
    session_t *sess = session;

   if(!strcasecmp(attributeID,szVersion)    || 
      !strcasecmp(attributeID,szTitle)      ||
      !strcasecmp(attributeID,szThreadSafe) || 
      !strcasecmp(attributeID,szUnloadable) ||
      !strcasecmp(attributeID,szUnloadTime) ||
      !strcasecmp(attributeID,szAcceptRawCmdString))
   { 
       sscError(hError,szServerNameErrorPrefix,
		"Attempt to set read only attribute in rgpgSetAttribute()");
       return;
   }

   if(!strcasecmp(attributeID,"User-Agent") && value)
   { 
       sess->textonly = (strcasecmp(value,"Lynx") == 0) ? 1 : 0;
       return;
   }

   if(!strcasecmp(attributeID,szAvail_sel) && value)
   {
       sess->avail_sel = atoi(value);
       return;
   }

   if(!strcasecmp(attributeID,szAvail_start) && value)
   {
       sess->from_t = maketime(value,START);
       if(sess->from_t < 0)
	   sess->from_t=0;
       if(sess->Avail_startValue)
	   free(sess->Avail_startValue);

       sess->Avail_startValue=strdup(value);

       return;
   }

   if(!strcasecmp(attributeID,szAvail_end) && value)
   {
       sess->to_t = maketime(value,END);
       if(sess->to_t < 0)
	   sess->to_t=0;
       if(sess->Avail_endValue)
	   free(sess->Avail_endValue);

       sess->Avail_endValue=strdup(value);

       return;
   }

   if(!strcasecmp(attributeID,szAvail_sysid) && value)
   {
       if(value)
       {
	   if(!sess->sysids)
	       sess->sysids=(char **)calloc(sizeof(char *),1000);
	   if(sess->num_ids==1000)
	       sess->sysids=realloc(sess->sysids,
				    sizeof(char *)*(sess->num_ids+10));
	   sess->sysids[sess->num_ids++] = strdup(value);
       }
       return;
   }

   if(!strcasecmp(attributeID,szStartEvent) && value)
   {
       sess->start_event_h2=atoi(value);
       if(sess->start_event_h2 < 0)
	   sess->start_event_h2=0;

       return;
   }

   if(!sess || sess->signature != sizeof(session_t))
   { 
       sscError(hError,szServerNameErrorPrefix,
		"Incorrect session id in rgpgGetAttribute()");
       return;
   }
}


/*-----------------rgpgGenerateReport------------------*/

int RGPGAPI rgpgGenerateReport(sscErrorHandle hError, 
			       rgpgSessionID session, 
			       int argc, char* argv[], 	
			       char *rawCommandString,
			       streamHandle result,
			       char *userAccessMask)
{  
    int rv;
    session_t *pSession = session;

    if(!session)
	return RGPERR_SUCCESS;

    if(pSession->aggr_stats)
	FreeAggregate(pSession->aggr_stats);
    
    if(pSession->hosts)
	FreeHosts(pSession->hosts);
	
    /*  Initialize The HTML Generator  */
    rv = createMyHTMLGenerator(result);
    if (rv != 0)
    { 
	sscError(hError,  "AMR server: Can't open the HTML generator");
	return RGPERR_SUCCESS;
    }

    pSession->hosts = NULL;
    pSession->aggr_stats = NULL;
    strcpy(pSession->dbname,db);
    pSession->group_reports_enabled = FALSE;
    now = time(0);


    if(argc > 1)
      {
	  amr_initq_db(session,atoi(argv[1]));
	  if(read_db_in(hError,pSession,pSession->from_t,pSession->to_t,
			atoi(argv[1])))
	      return RGPERR_SUCCESS;

	  switch (atoi(argv[1])) {
	  case AMR_INIT:
	      generate_initial(session,atoi(argv[2]),
			       pSession->num_ids,pSession->sysids);
	      break;
	  case AMR_H1:
	      generate_H1(session,pSession->num_ids,pSession->sysids);
	      break;
	  case AMR_H2:
	      generate_H2(session,pSession->num_ids,pSession->sysids);
	      break;
	  case AMR_H3:
	      generate_H3(session,atoi(argv[2]),
			  pSession->num_ids,pSession->sysids);
	      break;
	  case AMR_S1:
	      generate_S1(session,pSession->num_ids,pSession->sysids);
	      break;
	  case AMR_S2:
	      generate_S2(session,pSession->num_ids,pSession->sysids);
	      break;
	  default:
	      generate_H1(session,pSession->num_ids,pSession->sysids);
	      break;
	  }
      }
    else if (pSession->avail_sel == 0)
    {
	amr_initq_db(session,AMR_H1);
	if(read_db_in(hError,pSession,pSession->from_t,pSession->to_t,
		      AMR_H1))
	    return RGPERR_SUCCESS;
	generate_H1(session,0,NULL);
    }
    else if (pSession->avail_sel == 1)
    {
	amr_initq_db(session,AMR_H2);
	if(read_db_in(hError,pSession,pSession->from_t,pSession->to_t,
		      AMR_H2))
	    return RGPERR_SUCCESS;
	generate_H2(session,0,NULL);
    }
    else if (pSession->avail_sel == 2)
    {
	amr_initq_db(session,AMR_S1);
	if(read_db_in(hError,pSession,pSession->from_t,pSession->to_t,
		      AMR_S1))
	    return RGPERR_SUCCESS;
	generate_S1(session,pSession->num_ids,pSession->sysids);
    }
    else if (pSession->avail_sel == 3)
    {	
	amr_initq_db(session,AMR_S2);
	if(read_db_in(hError,pSession,pSession->from_t,pSession->to_t,
		      AMR_S2))
	    return RGPERR_SUCCESS;
	generate_S2(session,pSession->num_ids,pSession->sysids);
    }
    else
    {
	amr_initq_db(session,AMR_H1);
	if(read_db_in(hError,pSession,pSession->from_t,pSession->to_t,
		      AMR_H1))
	    return RGPERR_SUCCESS;
	generate_H1(session,0,NULL);
    }

    /*  Destroy The HTML Generator  */
    rv = deleteMyHTMLGenerator();
    if (rv != 0)
	{
	    sscError(hError,  "AMR server: Can't destroy the HTML generator");
	    return RGPERR_SUCCESS;
	}
    return RGPERR_SUCCESS;
}

