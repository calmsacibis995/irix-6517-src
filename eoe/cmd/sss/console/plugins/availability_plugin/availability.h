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

#define START 0
#define END 1
#define MAXNUMBERS_OF_KEYS 512
#define MAX_ACTIONS 512
#define CHARTYPE 0
#define INTTYPE 1

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
	GLOBALPAGE=1,		/* Bring up global page */
	GLOBALCONFIRM,		/* Confirm global setting changes */
	AVAILMON_CONFIG,
	AVAILMON_CONFIG_CONFIRM,
	AVAILMON_MAIL,
	AVAILMON_MAIL_CONFIRM
        };

/* Local "Session" structure */
typedef struct sss_s_MYSESSION {
  unsigned long signature;            /* sizeof(mySession) */
  struct sss_s_MYSESSION *next;       /* next pointer */
  int textonly;                       /* text mode only flag */
  int report_type;
} mySession;

static char   myTitle[] = "Event Reports";

static const char myLogo[]            = "Availability Plugin Server";
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

/* Global Setup */
void global_setup(sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection,
        ssdb_Error_Handle error_handle,CMDPAIR *cmdp);
void globalconfirm(sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection,
        ssdb_Error_Handle error_handle,CMDPAIR *cmdp);
int getnumrecords (sscErrorHandle hError,ssdb_Error_Handle error_handle,ssdb_Request_Handle req_handle);

void availmon_config(sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection,
        ssdb_Error_Handle error_handle);
void availmon_config_confirm(sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection,
        ssdb_Error_Handle error_handle,CMDPAIR *cmdp);
void availmon_mail(sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection,
        ssdb_Error_Handle error_handle,CMDPAIR *cmdp);
void availmon_mail_confirm(sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection,
        ssdb_Error_Handle error_handle,CMDPAIR *cmdp);
void availmon_mail_confirm_html(sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection,
        ssdb_Error_Handle error_handle, int pageselect);

/* utility functions */
int read_java_scritps(const char *screen_name,const char *screen_end);
void print_error(void);
void help_create(sscErrorHandle hError,char *helpfile);
int get_variable(sscErrorHandle hError,CMDPAIR *cmdp,int type, char *varname,char *string,int *intval,int errornum);

void message(sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection,ssdb_Error_Handle error_handle,CMDPAIR *cmdp);

#endif /* endif H_UI_H */
