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
#include <pwd.h>
#include <stddef.h>
#include "rgPluginAPI.h"
#include "sscHTMLGen.h"
#include "sscPair.h"
#include <time.h>
#include <eventmonapi.h>
#include <sys/stat.h>
#define START 0
#define END 1
#define MAXNUMBERS_OF_KEYS 512
#define MAX_ACTIONS 512
#define MAX_CLASSES 512
#define CHARTYPE 0
#define INTTYPE 1
#define HTMLHEAD 1
#define NOHEAD 0

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
        ACTIONLIST=1,           /* Creates an action list */
        EVENTLIST,              /* Creates an event list */
        TYPELIST,               /* Creates a tyelist */
        MULTISELECT,            /* Flag for single select or multiselect */
        UPDATEACTION,           /* Creates the update action screen with values loaded. If multiselect, new entry screen */
        UPDTACTCONFIRM,         /* Creates the action screen with values loaded for confirmation. */
        RETURNHOSTNAME,         /* Returns the hostname */
        GETSYSID,               /* Returns the sysid */
	GETSYSID_UNHID,
        DELETEACTION,           /* Delete an action screen */
        DELETELISTSET,          /* Set of events from which action will be deleted */
        SETUPDATE,              /* Seup screen for update events if MULTISELECT = 1 or entry screen for events if 0 */
        UPDATE_EVENT,           /* Update the event after getting new parameters */
	EVENT_ADD_PAGE,
	EVENT_VIEW_PAGE,
	EVENT_VIEW_PAGE_CONT,
	EVENT_VIEW_CLASS_PAGE_CONT,
	EVENT_VIEW_TYPE_PAGE,
	EVENT_VIEW_REPORT_PAGE,
	EVENT_REPORT_PAGE,
	EVREPORT_CONT,
	CLASSREPORT_CONT,
	TYPEREPORT_CONT,
	EVENT_ADD_INFO,
	EVENT_REPORT_BY_HOST,
	EVENT_REPORT_TYPE_PAGE,
	EVENT_REPORT_BY_TYPE_HOST,
	EVENT_REPORT_BY_CLASS_HOST,
	EVENT_ADD_CONFIRM,
	EVENT_UPDT_INFO,
	EVENT_UPDT_CONFIRM,
	EVENT_UPDATE_ACTION_LIST,
	EVENT_ACTION_AUD,
	EVENT_ACTION_ADD_CONFIRM,
	EVENT_ACTION_UPD_CONFIRM,
	EVENT_DELETE,
	EVENT_DELETE_LIST,
	CREATE_CLASS_LIST,
	ACTION_REPORT_BY_ACTION,
	ACTIONS_PAGE,
	ACTION_VIEW_CONT,
	EVENT_ACTION_SPECIFIC_REPORT,
	EVENT_ACTIONS_PAGE,
	ACTION_TAKEN_REPORT,
	ARCHIVE_LIST,
	DELETE_ARCHIVE,
	ARCHIVE_TABLE
        };

/* Local "Session" structure */
typedef struct sss_s_MYSESSION {
  unsigned long signature;            /* sizeof(mySession) */
  struct sss_s_MYSESSION *next;       /* next pointer */
  int textonly;                       /* text mode only flag */
  int report_type;
} mySession;

static char   myTitle[] = "Event Reports";

static const char myLogo[]            = "System Event Manager Plugin Server";
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
void create_action_list(sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection,
        ssdb_Error_Handle error_handle, CMDPAIR *cmdp);
void create_type_list(sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection,
        ssdb_Error_Handle error_handle,CMDPAIR *cmdp);
void create_event_list(sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection,
        ssdb_Error_Handle error_handle,CMDPAIR *cmdp);

/* Setup > actions */
void action_parameters(sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection,
        ssdb_Error_Handle error_handle,CMDPAIR *cmdp);
void update_action_params(sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection,
        ssdb_Error_Handle error_handle,CMDPAIR *cmdp);
int deletelistset(sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection,
        ssdb_Error_Handle error_handle,CMDPAIR *cmdp);
void get_event_set_update(sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection,ssdb_Error_Handle error_handle);
void delete_action(sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection,
        ssdb_Error_Handle error_handle,CMDPAIR *cmdp);

void update_eventsetup(sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection,ssdb_Error_Handle error_handle);
void generate_hostname(sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection,ssdb_Error_Handle error_handle);
void generate_sysid(sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection,ssdb_Error_Handle error_handle);
void generate_sysid_unhidden(sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection,ssdb_Error_Handle error_handle);
int getnumrecords (sscErrorHandle hError,ssdb_Error_Handle error_handle,ssdb_Request_Handle req_handle);


/* Reports */
void event_report_by_host(sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection,
        ssdb_Error_Handle error_handle,CMDPAIR *cmdp);
void event_report_by_class_host(sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection,
        ssdb_Error_Handle error_handle,CMDPAIR *cmdp);
int generate_system_info(sscErrorHandle hError , mySession *session,char *sysid,ssdb_Connection_Handle connection,
        ssdb_Error_Handle error_handle);

void event_reports_specific(sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection,
        ssdb_Error_Handle error_handle,CMDPAIR *cmdp);
void event_reports_byclass(sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection,
        ssdb_Error_Handle error_handle,CMDPAIR *cmdp,int event_select);
void event_report_type_page(sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection,
        ssdb_Error_Handle error_handle,CMDPAIR *cmdp);

/* Action reports */
void event_actions_page(sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection,
        ssdb_Error_Handle error_handle,CMDPAIR *cmdp);
void event_action_report(sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection,
        ssdb_Error_Handle error_handle,CMDPAIR *cmdp);
void event_action_specific_report(sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection,
        ssdb_Error_Handle error_handle,CMDPAIR *cmdp);
void view_action_page(sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection,
        ssdb_Error_Handle error_handle,CMDPAIR *cmdp);
void action_report_by_action(sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection,
        ssdb_Error_Handle error_handle,CMDPAIR *cmdp);
void view_action_list(sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection,
        ssdb_Error_Handle error_handle,CMDPAIR *cmdp);

/* Event setup */
void event_view_page(sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection,
        ssdb_Error_Handle error_handle,CMDPAIR *cmdp);
void event_description_view(sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection,
        ssdb_Error_Handle error_handle,CMDPAIR *cmdp);
void event_description_class(sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection,
        ssdb_Error_Handle error_handle,CMDPAIR *cmdp);
void event_view_configuration(sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection,
        ssdb_Error_Handle error_handle,CMDPAIR *cmdp);
void event_view_type_configuration(sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection,
        ssdb_Error_Handle error_handle,CMDPAIR *cmdp);
void event_view_report_page(sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection,
        ssdb_Error_Handle error_handle,CMDPAIR *cmdp);
void event_add_page(sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection,
        ssdb_Error_Handle error_handle);
void event_add_info(sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection,
        ssdb_Error_Handle error_handle,CMDPAIR *cmdp);
void event_add_confirm(sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection,
        ssdb_Error_Handle error_handle,CMDPAIR *cmdp);
void event_updt_info(sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection,
        ssdb_Error_Handle error_handle,CMDPAIR *cmdp);
void event_updt_confirm(sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection,
        ssdb_Error_Handle error_handle,CMDPAIR *cmdp);
void event_update_action_list(sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection,
        ssdb_Error_Handle error_handle,CMDPAIR *cmdp);
void event_action_delete(sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection,
        ssdb_Error_Handle error_handle,CMDPAIR *cmdp);
void event_action_add(sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection,
        ssdb_Error_Handle error_handle,CMDPAIR *cmdp);
void event_action_update(sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection,
        ssdb_Error_Handle error_handle,CMDPAIR *cmdp);
void event_action_update_confirm(sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection,
        ssdb_Error_Handle error_handle,CMDPAIR *cmdp);
void create_custom_list(sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection,
        ssdb_Error_Handle error_handle,CMDPAIR *cmdp);
void delete_event(sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection,
        ssdb_Error_Handle error_handle,CMDPAIR *cmdp);
void event_delete(sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection,
        ssdb_Error_Handle error_handle,CMDPAIR *cmdp);
void delete_class(sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection,
        ssdb_Error_Handle error_handle,CMDPAIR *cmdp);
void archive_db_confirm(mySession *session, char *name,int type);
void archive_list(sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection,
        ssdb_Error_Handle error_handle,CMDPAIR *cmdp);
void delete_archive(sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection,
        ssdb_Error_Handle error_handle,CMDPAIR *cmdp);
void generate_class_list(sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection, ssdb_Error_Handle error_handle);
void drop_temp_table(sscErrorHandle hError,ssdb_Connection_Handle connection,ssdb_Error_Handle error_handle);
void archive_table(sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection,
        ssdb_Error_Handle error_handle,CMDPAIR *cmdp);

/* utility functions */
int read_java_scritps(const char *screen_name,const char *screen_end);
long maketime(const char *dateval,int startend);
void makedate (time_t timeval, char *dateout, char *event_time);
void print_error(void);
void create_help(sscErrorHandle hError,char *helpfile);
int get_variable(sscErrorHandle hError,CMDPAIR *cmdp,int type, char *varname,char *string,int *intval,int errornum);
#endif /* endif H_UI_H */
