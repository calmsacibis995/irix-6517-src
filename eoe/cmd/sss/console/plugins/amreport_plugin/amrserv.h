#ifndef __AMRSERV_H_
#define __AMRSERV_H_

#define SUCCESS			0
#define FAILED			1

#define MAX_DBNAMELEN		64
#define MAX_SYSIDLEN            65

typedef enum { FALSE, TRUE} BOOL;  

#include "ssdbapi.h"
#include "amevent.h"

/* availmon event related macros */

#define AMR_MIN_EVNT		LIVE_SYSEVNT
#define AMR_MAX_EVNT		SU_SW_FIX
#define AVAILTEMPTABLE          "availtemp"

/* Needs to be 1 relative */
#define AMR_N_EVNT		AMR_MAX_EVNT - AMR_MIN_EVNT + 2
#define TOINDX(evnt)		(evnt - AMR_MIN_EVNT + 1)

/* map availmon event type_IDs into index values */

#define MU_UND_NC_IND		TOINDX(MU_UND_NC)
#define MU_UND_TOUT_IND		TOINDX(MU_UND_TOUT)
#define MU_UND_UNK_IND		TOINDX(MU_UND_UNK)
#define MU_UND_ADM_IND		TOINDX(MU_UND_ADM)
#define MU_HW_UPGRD_IND	 	TOINDX(MU_HW_UPGRD)
#define MU_SW_UPGRD_IND	 	TOINDX(MU_SW_UPGRD)
#define MU_HW_FIX_IND		TOINDX(MU_HW_FIX) 
#define MU_SW_PATCH_IND	 	TOINDX(MU_SW_PATCH)
#define MU_SW_FIX_IND	 	TOINDX(MU_SW_FIX)
#define SU_UND_TOUT_IND		TOINDX(SU_UND_TOUT)
#define SU_UND_NC_IND	 	TOINDX(SU_UND_NC)
#define SU_UND_ADM_IND	 	TOINDX(SU_UND_ADM)
#define SU_HW_UPGRD_IND		TOINDX(SU_HW_UPGRD)
#define SU_SW_UPGRD_IND	 	TOINDX(SU_SW_UPGRD)
#define SU_HW_FIX_IND	 	TOINDX(SU_HW_FIX)
#define SU_SW_PATCH_IND	 	TOINDX(SU_SW_PATCH)
#define SU_SW_FIX_IND		TOINDX(SU_SW_FIX)
#define UND_PANIC_IND	 	TOINDX(UND_PANIC)
#define HW_PANIC_IND	 	TOINDX(HW_PANIC)
#define UND_INTR_IND		TOINDX(UND_INTR)
#define UND_SYSOFF_IND	 	TOINDX(UND_SYSOFF)
#define UND_PWRFAIL_IND	 	TOINDX(UND_PWRFAIL) 
#define SW_PANIC_IND	 	TOINDX(SW_PANIC)
#define UND_NMI_IND	 	TOINDX(UND_NMI)
#define UND_RESET_IND		TOINDX(UND_RESET) 
#define UND_PWRCYCLE_IND	TOINDX(UND_PWRCYCLE) 
#define DE_REGISTER_IND		TOINDX(DE_REGISTER) 
#define REGISTER_IND		TOINDX(REGISTER)
#define LIVE_NOERR_IND		TOINDX(LIVE_NOERR)
#define LIVE_HWERR_IND		TOINDX(LIVE_HWERR)
#define LIVE_SWERR_IND		TOINDX(LIVE_SWERR)
#define STATUS_IND		TOINDX(STATUS)
#define ID_CORR_IND		TOINDX(ID_CORR)
#define LIVE_SYSEVNT_IND	TOINDX(LIVE_SYSEVNT)
#define NOTANEVENT_IND		0

#define AMR_LOCAL_REPORT    'L'
#define AMR_SITE_REPORT	    'S'
	
/* report_ID values - single host reports */
#define AMR_INVALID          -1
#define AMR_INIT             0
#define AMR_H1               1
#define AMR_H2               2
#define AMR_H3               3


/* report_ID values - site level reports */
#define AMR_S1              11
#define AMR_S2              12
#define AMR_S3		    13
#define AMR_S4		    14
#define AMR_S5		    15

#define AMR_BEGIN_RGSTRING  "<A HREF=\"/$sss/RG/amrserver~"
#define AMR_END_RGSTRING    "\">"
#define AMR_END_HREF        "</A>"

/*----------------------------------------------------------------------------- 
 * Define the internal data structures where we will read in		
 * the data from SSDB							
 * And those where we claculate and store various stats as we need,	
 * to construct various levels of reports				
 *----------------------------------------------------------------------------- 
 */

/* Statistics that we need to keep, for single_host/sitelevel */
struct	_statname{
    char *sname;
};

static struct _statname statname[] = {
	"Unscheduled ",

	"&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;"
	"panic due to hardware ",

	"&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;"
	"panic due to software ",

	"&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;"
	"panic due to unknown reason ",

	"&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;"
	"reset action ",

	"&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;"
	"power interruption ",

	"Service action ",

	"&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;"
	"fix/replace hardware ",

	"&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;"
	"upgrade hardware ",

	"&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;"
	"upgrade software ",

	"&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;"
	"fix software ",

	"&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;"
	"install patch ",

	"&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;"
	"administrative: single-user ",

	"&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;"
	"administrative: reboot ",

	"&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;"
	"unknown ",

	"Total "
};


/* This is the same as statname but for Lynx */
static struct _statname statname_txt[] = {
	"Unscheduled ",
	"panic due to hardware ",
	"panic due to software ",
	"panic due to unknown reason ",
	"reset action ",
	"power interruption ",
	"Service action ",
	"fix/replace hardware ",
	"upgrade hardware ",
	"upgrade software ",
	"fix software ",
	"install patch ",
	"administrative: single-user ",
	"administrative: reboot ",
	"unknown ",
	"Total "
};



#define NSTATS (sizeof(statname)/sizeof(struct _statname))

typedef struct _event{
    struct _event *next;
    struct _event *prev;

    char    *summary;

    int     ev_type;
    time_t  prevstart;
    time_t  event_time;
    time_t  start_time;		/* system start after event occurance */
    time_t  lasttick;
    int     noprevstart;
    int     uptime;		/* of system before this event	*/
				/* =event_time - prevstart	*/
    int     dntime;		/* of system due to this event	*/
				/* =start_time - event_time	*/
    int     bound;
    int	    metric;		/* Is this a metric related event? */
    int     summarylen;

    int  size;
    int  tag;				/* == Record_ID */ 
} event_t;

typedef struct _hostinfo_t{
    struct _hostinfo_t *next;
    struct _hostinfo_t *prev;

    event_t  *first_ev;
    event_t  *last_ev;

    char    sys_id[MAX_SYSIDLEN];
    int     hostnamelen;
    char    *hostname;
    time_t  from_t;                     /* info here, from this time	*/
    time_t  to_t;			/* to this time			*/
    int   num_total_events;	        /* number of total events.      */
    int	  n_ev[AMR_N_EVNT];             /* count of events of a type	*/
    int   ev_uptime[AMR_N_EVNT];
    int   ev_dntime[AMR_N_EVNT];
    int   ev_avgup[AMR_N_EVNT];         /* average uptime per event type*/
    int   stats[NSTATS];                /* statistics values for H1	*/
    int   stuptime[NSTATS];
    int   stdntime[NSTATS];
    int   thisepoch;
    int   total;
    int   possible_uptime;
    time_t  lastboot;
    time_t  the_beginning;
    int   nepochs;
    int   ndowns;
    int   avg_uptime;
    int   least_uptime;
    int   most_uptime;
    int   avg_dntime;
    int   least_dntime;
    int   most_dntime;
    int   availability;

    int  size;
    int  tag;                           /* == Record_ID */
} hostinfo_t;

typedef struct _aggregate_stats_t{

    char    *upleasthost;
    char    *upmosthost;
    char    *dnleasthost;
    char    *dnmosthost;
    time_t  from_t;                     /* info here, from this time	*/
    time_t  to_t;			/* to this time			*/
    int	  n_ev[AMR_N_EVNT];             /* count of events of a type	*/
    int   ev_uptime[AMR_N_EVNT];
    int   ev_dntime[AMR_N_EVNT];
    int   ev_avgup[AMR_N_EVNT];         /* average uptime per event type */
    int   stats[NSTATS];                /* statistics values for H1	*/
    int   stuptime[NSTATS];
    int   stdntime[NSTATS];
    int   thisepoch;
    int   total;
    int   possible_uptime;
    time_t  lastboot;
    time_t  the_beginning;
    int   nepochs;
    int   ndowns;
    int   avg_uptime;
    int   least_uptime;
    int   most_uptime;
    int   avg_dntime;
    int   least_dntime;
    int   most_dntime;
    int   availability;

} aggregate_stats_t;

/*----------------------------------------------------------------------------- 
 * Request structure that comes from the client.  
 * It contains the request type and associated data if any. The pointer 
 * req_data can point to AMR_REPORT_PARAMS_T or  AMR_REPORT_NEXT_PARAMS_T
 * or can also be NULL. 
 * This would be the case for example, when the request is for 
 * AVAIL_REP_QUERY as that is the top level initial query  and
 * has no associated data with it.
 *----------------------------------------------------------------------------- 
 */

typedef struct _request_t{
    int		request_type;
    char        dbname[MAX_DBNAMELEN+1];
    void        *req_data;
} request_t;

/*------------------------------------------
 * Session control block data structure     
 * needs to have a pointer to corresponding 
 * request, and response structures         
 *                                          
 *------------------------------------------
 */
typedef struct _session_t{
    unsigned long signature;            /* sizeof(cm_session_t) */
    struct _session_t *next;       /* next pointer */
    int textonly;                       /* text mode only flag */
    int avail_sel;		/* avail_sel flag. */
    long from_t;
    long to_t;

    aggregate_stats_t    *aggr_stats;  /* pointer to aggregate stats for */ 
				      /* SGM type data, NULL otherwise */
    hostinfo_t      *hosts;       /* head of the host link list */
    hostinfo_t      *hosts_last;  /* tail of the host link list */

    ulong_t             lastError;           /* for this report session */
    int			mode;
    ssdb_Error_Handle	dberr_h;
    ssdb_Client_Handle	dbclnt_h;
    ssdb_Connection_Handle dbconn_h;
    BOOL		group_reports_enabled;
    int			hosts_count;  /* count of unique hosts */
    char		dbname[MAX_DBNAMELEN+1];
    int                 record_cnt;
    time_t              from_time;
    time_t              to_time;
    char               *Avail_startValue,*Avail_endValue;
    int                 num_ids;
    char              **sysids;
    int                 start_event_h2;	        /* start of events. */
    char local_sysid[MAX_SYSIDLEN];
} session_t;

/*----------------------------------------------------------------------------
 * Local reports (== Single system reports) consist of H1, H2, H3 reports   
 * while Site level reports are S1, S2, and then H1-H3 for all hosts in a site.
 *									      
 *----------------------------------------------------------------------------
 */
/* 
 * Get all the group availability records.
 */
#define BEGIN_GROUP_SQL_STATEMENT            \
"INSERT INTO "                               \
AVAILTEMPTABLE                               \
" SELECT "                                   \
"event.type_id, event.event_start, "         \
"availdata.event_time, availdata.lasttick, " \
"availdata.prev_start, availdata.start, "    \
"availdata.status_int, availdata.bounds, "   \
"availdata.metrics, availdata.flag, "        \
"availdata.summary, "                        \
"system.sys_id, system.hostname "            \
"FROM "                                      \
"system, "                                   \
"event, "                                    \
"availdata "                                 \
"WHERE  "                                    \
"availdata.sys_id = system.sys_id "          \
"AND  "                                      \
"availdata.event_id =  event.event_id "     

#define END_GROUP_SQL_STATEMENT              \
"ORDER BY "                                  \
"availdata.sys_id"                           

#define BEGIN_ARCHIVE_GROUP_SQL_STATEMENT            \
"INSERT INTO "                                       \
AVAILTEMPTABLE                                       \
" SELECT "                                           \
"%s.event.type_id, %s.event.event_start, "           \
"%s.availdata.event_time, %s.availdata.lasttick, "   \
"%s.availdata.prev_start, %s.availdata.start, "      \
"%s.availdata.status_int, %s.availdata.bounds, "     \
"%s.availdata.metrics, %s.availdata.flag, "          \
"%s.availdata.summary, "                             \
"%s.system.sys_id, %s.system.hostname "              \
"FROM "                                              \
"%s.system, "                                        \
"%s.event, "                                         \
"%s.availdata "                                      \
"WHERE  "                                            \
"%s.availdata.sys_id = %s.system.sys_id "            \
"AND  "                                              \
"%s.availdata.event_id =  %s.event.event_id "     

#define END_ARCHIVE_GROUP_SQL_STATEMENT              \
"ORDER BY "                                          \
"%s.availdata.sys_id"                           


/* 
 * Get all the local availability records.
 */
#define BEGIN_LOCAL_SQL_STATEMENT            \
"INSERT INTO "                               \
AVAILTEMPTABLE                               \
" SELECT "                                   \
"event.type_id, event.event_start, "         \
"availdata.event_time, availdata.lasttick, " \
"availdata.prev_start, availdata.start, "    \
"availdata.status_int, availdata.bounds, "   \
"availdata.metrics, availdata.flag, "        \
"availdata.summary "                         \
"FROM "                                      \
"event, "                                    \
"availdata "                                 \
"WHERE "                                     \
"availdata.sys_id = \"%s\" "                 \
"AND "                                       \
"(availdata.event_id =  event.event_id "     

/* 
 * Get the local system from system table.
 */
#define BEGIN_LOCAL_SYSID_SQL_STATEMENT      \
"SELECT sys_id, hostname FROM system "       \
"WHERE  system.active = 1 and system.local = 1"

/* 
 * Get all the local availability records.
 */
#define BEGIN_ARCHIVE_LOCAL_SQL_STATEMENT            \
"INSERT INTO "                                       \
AVAILTEMPTABLE                                       \
" SELECT "                                           \
"%s.event.type_id, %s.event.event_start, "           \
"%s.availdata.event_time, %s.availdata.lasttick, "   \
"%s.availdata.prev_start, %s.availdata.start, "      \
"%s.availdata.status_int, %s.availdata.bounds, "     \
"%s.availdata.metrics, %s.availdata.flag, "          \
"%s.availdata.summary "                              \
"FROM "                                              \
"%s.event, "                                         \
"%s.availdata "                                      \
"WHERE "                                             \
"%s.availdata.sys_id = \"%s\" "                      \
"AND "                                               \
"(%s.availdata.event_id =  %s.event.event_id "     

#endif   /*  ifdef __AMRSERV_H  */


