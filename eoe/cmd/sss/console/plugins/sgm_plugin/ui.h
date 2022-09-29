#ifndef H_UI_H
#define H_UI_H
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <ssdbapi.h>
#include <ssdberr.h>
#include <sys/types.h>
#include "rgPluginAPI.h"
#include "sscHTMLGen.h"
#include "sscPair.h"
#include <time.h>
#include <eventmonapi.h>

#define START 0
#define END 1
#define MAXNUMBERS_OF_KEYS 512
#define CHARTYPE 0
#define INTTYPE 1
#define MAX_ACTIONS 512

/* --------------------------------------------------------------------------- */
#ifdef __TIME__
#ifdef __DATE__
#define INCLUDE_TIME_DATE_STRINGS 1
#endif
#endif
/* --------------------------------------------------------------------------- */
#define MYVERSION_MAJOR    1          /* Report Generator PlugIn Major Version number */
#define MYVERSION_MINOR    0          /* Report Generator PlugIn Minor Version number */
/* --------------------------------------------------------------------------- */

/* globals for parsing input */

static char szDefaultUserName[] = "";             /* Default user name */
static char szDefaultPassword[] = "";             /* Default password */
static char szDefaultHostName[] = "localhost";    /* Default host name */
static char szDefaultDatabaseName[] = "ssdb";     /* Default db name */

enum REQUEST {
        SGM_EVENT_VIEW=1,         
        SGM_EVENT_DESCRIPTION_CONT,
	SGM_EVENT_CLASS_VIEW_CONT, 
	SGM_GENERATE_SYSTEMS,
	SGM_CLASS_LIST,
	SGM_UPDATE_EVCLASS_LIST,
	SGM_TYPE_LIST,
	SGM_ACTION_LIST,
	SGM_UPDATE_EVENT_LIST,	
	SGM_EVENT_UPDATE_CONFIRM,
	SGM_EV_REPORTS,
	SGM_EVENT_UPDATE_INFO
	};

/* Local "Session" structure */
typedef struct sss_s_MYSESSION {
  unsigned long signature;            /* sizeof(mySession) */
  struct sss_s_MYSESSION *next;       /* next pointer */
  int textonly;                       /* text mode only flag */
  int report_type;
} mySession;

static char   myTitle[] = "Event Reports";

static const char myLogo[]            = "System Group Manager Plugin Server";
static const char szVersion[]         = "Version";
static const char szTitle[]           = "Title";
static const char szThreadSafe[]      = "ThreadSafe";
static const char szUnloadable[]      = "Unloadable";
static const char szUnloadTime[]      = "UnloadTime";
static const char szAcceptRawCmdString[] = "AcceptRawCmdString"; /* Obligatory attribute name: how plugin will process cmd parameters */
static char szServerNameErrorPrefix[] = "SSS WEB Config Server Error: %s";
static char szVersionStr[64];

static pthread_mutex_t seshFreeListmutex;
static const char szUnloadTimeValue[] = "120"; /* Unload time for this plug in (sec.) */
static const char szThreadSafeValue[] = "0";   /* "Thread Safe" flag value - this plugin is thread safe */
static const char szUnloadableValue[] = "1";   /* "Unloadable" flag value - RG core might unload this plugin from memory */
static const char szAcceptRawCmdStringValue[] = "1";   /* RG server accept "raw" command string like key=value&key2=value&... */
static const char szUserAgent[]          = "User-Agent";

static int volatile mutex_inited      = 0;   /* seshFreeListmutex mutex inited flag */
static mySession *sesFreeList         = 0;   /* Session free list */

/* UI elements */
void sgm_action_list(sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection,
        ssdb_Error_Handle error_handle, CMDPAIR *cmdp);
void sgm_type_list(sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection,
        ssdb_Error_Handle error_handle,CMDPAIR *cmdp);
void sgm_class_list(sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection,
        ssdb_Error_Handle error_handle,CMDPAIR *cmdp);
void generate_sysid_unhidden(sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection,ssdb_Error_Handle error_handle);
int getnumrecords (sscErrorHandle hError,ssdb_Error_Handle error_handle,ssdb_Request_Handle req_handle);
void generate_class_list(sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection, ssdb_Error_Handle error_handle);
int read_java_scritps(const char *screen_name,const char *screen_end);
long maketime(const char *dateval,int startend);
void makedate (time_t timeval, char *dateout, char *event_time);
void create_help(sscErrorHandle hError,char *helpfile);
int get_variable(sscErrorHandle hError,CMDPAIR *cmdp,int type, char *varname,char *string,int *intval,int errornum);
void generate_list_of_systems(sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection,
        ssdb_Error_Handle error_handle,CMDPAIR *cmdp);


void sgm_event_view(sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection,
        ssdb_Error_Handle error_handle,CMDPAIR *cmdp);
void event_description_sgm_view(sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection,
        ssdb_Error_Handle error_handle,CMDPAIR *cmdp);
void event_description_sgm_class(sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection,
        ssdb_Error_Handle error_handle,CMDPAIR *cmdp);
void sgm_update_event_list(sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection,
        ssdb_Error_Handle error_handle,CMDPAIR *cmdp);
void sgm_update_event_info(sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection,
        ssdb_Error_Handle error_handle,CMDPAIR *cmdp);
void sgm_event_update_confirm(sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection,
        ssdb_Error_Handle error_handle,CMDPAIR *cmdp);
void sgm_ev_report(sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection,
        ssdb_Error_Handle error_handle,CMDPAIR *cmdp);
void sgm_all_ev_report(sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection,
        ssdb_Error_Handle error_handle,CMDPAIR *cmdp);
void sgm_specific_ev_report(sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection,
        ssdb_Error_Handle error_handle,CMDPAIR *cmdp);
void sgm_update_evclass_list(sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection,
        ssdb_Error_Handle error_handle,CMDPAIR *cmdp);


#endif /* endif H_UI_H */
