/* --------------------------------------------------------------------------- */
/* -                             SSRV_MAIN.C                                 - */
/* --------------------------------------------------------------------------- */
/*                                                                             */
/* Copyright 1992-1999 Silicon Graphics, Inc.                                  */
/* All Rights Reserved.                                                        */
/*                                                                             */
/* This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;      */
/* the contents of this file may not be disclosed to third parties, copied or  */
/* duplicated in any form, in whole or in part, without the prior written      */
/* permission of Silicon Graphics, Inc.                                        */
/*                                                                             */
/* RESTRICTED RIGHTS LEGEND:                                                   */
/* Use, duplication or disclosure by the Government is subject to restrictions */
/* as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data      */
/* and Computer Software clause at DFARS 252.227-7013, and/or in similar or    */
/* successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -     */
/* rights reserved under the Copyright Laws of the United States.              */
/*                                                                             */
/* --------------------------------------------------------------------------- */
/*                     SGI Adjustable WEB Server Main Module                   */
/* --------------------------------------------------------------------------- */
/* $Revision: 1.15 $ */
/* #ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 199506L
#endif
*/
#ifndef _BSD_SIGNALS
#define _BSD_SIGNALS
#endif
#ifndef _BSD_TYPES
#define _BSD_TYPES
#endif
#ifndef _BSD_COMPAT
#define _BSD_COMPAT
#endif
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#ifndef FORK_VER
#include <pthread.h>
#include <semaphore.h>
#else
#include <sys/file.h> 
#endif
#include <paths.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <time.h>
#include <dirent.h>
#include <ctype.h>
#include <netdb.h>
#include <sys/un.h>
#include <sys/ioctl.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h> 
#include <malloc.h>

#include "ssrv.h"
#include "ssrv_util.h"
#include "ssrv_main.h"

#ifdef SSRV_SWITCHER_ON
#include "ssrv_swc.h"
#endif

#ifdef SSRV_SSDB_INCLUDE
#include "ssdbapi.h"
#include "ssdberr.h"
#endif

extern char *optarg;
extern int optind;

/* From t6net.h */
#ifndef FORK_VER
extern int tsix_on(int);
extern int tsix_off(int);
#endif

#ifdef SSRV_SWITCHER_ON
/* From ssrv_util.c */
extern int ssrvGetIntegerByTcb(hTaskControlBlock hTcb, int valueIdx, int valueIdx2);
extern const char *ssrvGetStringByTcb(hTaskControlBlock hTcb,int valueIdx, int valueIdx2);
extern int ssrvSetIntegerByTcb(hTaskControlBlock hTcb,int valueIdx, int valueIdx2, int value);
extern int ssrvSetStringByTcb(hTaskControlBlock hTcb,int valueIdx,int valueIdx2,const char *value);
#endif

/* ------------------------------------------------------------------------- */
#ifdef __TIME__
#ifdef __DATE__
#define INCLUDE_TIME_DATE_STRINGS 1
#endif
#endif
/* ------------------------------------------------------------------------- */
#ifndef IPPORT_MAXPORT
#define IPPORT_MAXPORT 65535
#endif
/* ------------------------------------------------------------------------- */
#define RCVINFOREQ_MAXSIZE       (1024*8)     /* Info receive buffer max size */
#define TOUTBASE_LSOPENSOCK      1            /* Listen thread: Base timeout value for open/bind/listen TCP socket (sec.) */
#define TOUTPLUS_LSOPENSOCK      2            /* Listen thread: Plus timeout value for open/bind/listen TCP socket (sec.) */
#define TOUTMAX_LSOPENSOCK       60           /* Listen thread: Max timeout value for open/bind/listen TCP socket (sec.) */
#define TOUTBASE_LSALLOCTCB      5            /* Listen thread: Base timeout value for alloc_tcb_structure (sec.) */
#define TOUTPLUS_LSALLOCTCB      10           /* Listen thread: Plus timeout value for alloc_tcb_structure (sec.) */
#define TOUTMAX_LSALLOCTCB       (60*2)       /* Listen thread: Max timeout value for alloc_tcb_structure (sec.) */
#define TOUTBASE_IFOPENSOCK      1            /* Info thread: Base timeout value for open UNIX socket (sec.) */
#define TOUTPLUS_IFOPENSOCK      2            /* Info thread: Plus timeout value for open UNIX socket (sec.) */
#define TOUTMAX_IFOPENSOCK       10           /* Info thread: Max timeout value for open UNIX socket (sec.) */
#define TOUTBASE_LCFLOADIPFLT    5            /* Load IP filter Info thread: Base timeout value for load ip filter info from db (sec.) */
#define TOUTPLUS_LCFLOADIPFLT    10           /* Load IP filter Info thread: Plus timeout value for load ip filter info from db (sec.) */
#define TOUTMAX_LCFLOADIPFLT     (60*60)      /* Load IP filter Info thread: Max timeout value for load ip filter info from db (sec.) */
#define TOUTMAX_LCDROPFIXFLG     (60*1)       /* Timeout for "drop" fix flag inside "Load IP filter Info thread" (sec.) */
#define TOUTMAX_FIXSWCLOADERR    (60*30)      /* timeout for fix "Can't load Switcher module" error (sec.) */
#define SSRV_LINGER_TIMEOUT      30           /* timeout for set linger option */
/* ------------------------------------------------------------------------- */
#ifdef SSRV_SSDB_INCLUDE
#define SSRV_DEFAULTSSDB_DBNAME  "ssdb"       /* Default Database name */
#define SSRV_DEFAULTSSDB_IPTABLE "ssrv_ipaddr"/* Default Table name with IP addresses */
#define SSRV_DEFAULTSSDB_KTABLE  "key_table"  /* Default Table name with keys values */
#define SSRV_SSDBCLIENT_NAME     ""           /* SSDB Connect Client name */
#define SSRV_SSDBCLIENT_PASSWORD ""           /* SSDB Connect Client password */
#define LOCAL_SSDB_NEWCLIENTFLAGS (SSDB_CLIFLG_ENABLEDEFUNAME|SSDB_CLIFLG_ENABLEDEFPASWD) /* Create new SSDB client flags */
#define LOCAL_SSDB_NEWCONNECTFLAGS (SSDB_CONFLG_RECONNECT|SSDB_CONFLG_REPTCONNECT|SSDB_CONFLG_DEFAULTHOSTNAME) /* Create new SSDB connection flags */
#endif
#define SSRV_DEFAULTHTMLFILE     "index.html" /* Default HTML file instead "/" */
/* ------------------------------------------------------------------------- */
/* SSS Server daemon commands definition                                     */
#define SSRVCMD_STOP             'Q'          /* Stop (and unload from memory) command */
#define SSRVCMD_STAT             'S'          /* Statistics request command */
#define SSRVCMD_TEST             'T'          /* Test command */
#define SSRVCMD_RELOAD           'R'          /* Reload "IP address" and "keys" configuration info command */
/* ------------------------------------------------------------------------- */
/* SSS Server Daemon's syslog messages seq.numbers                           */
#define SRVSEQN_STARTED          0x00200120l  /* Seq.Number for "Started" syslog message */
#define SRVSEQN_STOPED           0x00200121l  /* Seq.Number for "Stoped" syslog message */
#define SRVSEQN_ILLEGALCPUCMD    0x00200122l  /* Seq.Number for "Invalid CPU command" syslog message */
#define SRVSEQN_ILLEGALFPECMD    0x00200123l  /* Seq.Number for "Invalid FPE command" syslog message */
#define SRVSEQN_INITMUTEXERR     0x00200124l  /* Seq.Number for "Can't init mutex \"%s\"" syslog message */
#define SRVSEQN_INITTHREADERR    0x00200125l  /* Seq.Number for "Init thread error" syslog message */
#define SRVSEQN_INITCONDVARERR   0x00200126l  /* Seq.Number for "Can't init condition var \"%s\"" syslog message */
#define SRVSEQN_CANTALLOCTCB     0x00200127l  /* Seq.Number for "Can't alloc thread control block entry" syslog message */
#define SRVSEQN_CANBINDSOCKET    0x00200128l  /* Seq.Number for "Can't bind socket to \"listent\" port" syslog message */
#define SRVSEQN_LISTENSOCKETERR  0x00200129l  /* Seq.Number for "\"listen\" error" syslog message */
#define SRVSEQN_CANFINDSWCLIB    0x0020012Al  /* Seq.Number for "Can't find \"Switcher\" library \"%s\"" syslog message */
#define SRVSEQN_CANFINDRESPATH   0x0020012Bl  /* Seq.Number for "Can't find \"Resources\" path" syslog message */
#define SRVSEQN_PATHNOTALLOWED   0x0020012Cl  /* Seq.Number for "Resources path \"/\" is not allowed" syslog message */
#define SRVSEQN_RESPATHNOTABS    0x0020012Dl  /* Seq.Number for "Resources path \"%s\" is not absolute" syslog message */
#define SRVSEQN_INVPORTNUMBER    0x0020012El  /* Seq.Number for "Invalid port number \"%s\"" syslog message */
#define SRVSEQN_CANTINITDB       0x0020012Fl  /* Seq.Number for "Can't init database" syslog message */
#define SRVSEQN_CANTLOADIPFLT    0x00200130l  /* Seq.Number for "Can't load IP addr info from database" syslog message */ 
#define SRVSEQN_CANTLOADUENTRY   0x00200131l  /* Seq.Number for "Can't load "userentry" from database" syslog message */
#define SRVSEQN_RESERVED000000   0x00200132l  /* Seq.Number for "" syslog message */
#define SRVSEQN_CANTLCONNECTTODB 0x00200133l  /* Seq.Number for "Can't connect to database" syslog message */
/* ------------------------------------------------------------------------- */
/* SServer Daemon's return code                                              */
#define SSERR_SUCCESS            0            /* Success (Daemon started/stopped/etc.) */
#define SSERR_INVPARAM           1            /* Invalid parameters or only "usage" output */
#define SSERR_SYSERRFORK         2            /* System error: fork() */
#define SSERR_INVOSPARAMS        3            /* Incorrect system parameter(s), can't start daemon */
#define SSERR_DIEDSIGNAL         4            /* Killed by some system signal */
#define SSERR_INITDATA           5            /* Init data error */
#define SSERR_INITTHREAD         6            /* Init thread error */
#define SSERR_ALRINSTALLED       7            /* Already installed */
#define SSERR_CANTMAKETEMP       8            /* Can't make temp file name */
#define SSERR_CANTOPENSOCK       9            /* Can't open socket */
#define SSERR_CANTBINDSOCK       10           /* Can't bind socket */
#define SSERR_NOTINSTALLED       11           /* Daemon not installed */
#define SSERR_INVACCOUNT         12           /* Can't start daemon with non root account */
#define SSERR_INVCPUCMD          13           /* Invalid CPU command */
#define SSERR_INVFPECMD          14           /* Invalid FPE command */
#define SSERR_CANTSSOCKOPT       15           /* Can't set socket option(s) */
#define SSERR_CANTINITDB         15           /* Can't init database */

/* ------------------------------------------------------------------------- */
/* Global strings                                                            */
const char szDaemonName[]                  = SSRV_APPLICATION_NAME;         /* WEB server daemon name */
const char szLegalName[]                   = "SGI Configurable Web Server"; /* WEB server legal name */
char szSSServerInfoSockPath[128];                                           /* Server Info Socket name */
/* ------------------------------------------------------------------------- */
/* Static strings                                                            */
static const char szDelimitterLine[]       = "---------------------------------------------------------------";
static const char szCopyright[]            = "Silicon Graphics Inc. (C) 1992-1999";
#ifdef SSRV_SSDB_INCLUDE
static const char szDatabaseName[]         = SSRV_DEFAULTSSDB_DBNAME;        /* Database name */
static const char szDbAddrTableName[]      = SSRV_DEFAULTSSDB_IPTABLE;       /* Database "addr" Table name */
static const char szDbKeyTableName[]       = SSRV_DEFAULTSSDB_KTABLE;        /* Database "key" Table name */
#endif
static const char szTempSockTemplate[]     = "/tmp/.lvahttpd.tmpsock.%lX";   /* Server temp socket template */
static const char szInfoSockPathMask[]     = "/tmp/.%s.info.sock";           /* Server Info Socket name mask */
#ifdef FORK_VER
static const char szInstLockFnameMask[]    = "/tmp/.%s.lock";                /* Server lock file for check previous instance (only "fork" version) */
#endif
static const char szUnknownTime[]          = "Unknown Start Time and Date";  /* Unknown time - info message message */
static const char szDebug[]                = "debug";                        /* "debug" string */
static const char szNormal[]               = "normal";                       /* "normal" string */
static const char szInetd[]                = "inetd";                        /* "inetd" string */
static const char szStarted[]              = "started";                      /* message string for "started" syslog notification */
static const char szStoped[]               = "stopped";                      /* message string for "stoped" syslog notification */
static const char szUnknown[]              = "unknown";                      /* message string for "unknown" reason */
static const char szNONE[]                 = "<NONE>";                       /* NONE message */
static const char szOn[]                   = "on";                           /* "on" message string */
static const char szOff[]                  = "off";                          /* "off" message string */
static const char szUnknownError[]         = "Unknown Error";                /* "Unknown Error" message string */

#ifdef SSRV_SWITCHER_ON
/* "Switcher" functions */
static char szFNswcInitSwitcher[]          = "swcInitSwitcher";              /* Initialize "Switcher" library */
static char szFNswcDoneSwitcher[]          = "swcDoneSwitcher";              /* Deinitialize "Switcher" library */
static char szFNswcBeginProc[]             = "swcBeginProc";                 /* Start of HTTP request processing */
static char szFNswcContProc[]              = "swcContProc";                  /* Continue (get next portion of data) HTTP request processing */
static char szFNswcEndProc[]               = "swcEndProc";                   /* End of HTTP request processing */
#endif

static const char szErrorDaemonNotInstalled3[] = "%s - Error: %d (%d) - Daemon not installed\n";
static const char szErrorCantOpenSocket3[]     = "%s - Error: %d (%d) - Can't open socket\n";
static const char szErrorAlreadyInstalled2[]   = "%s - Error: %d - Already installed\n";
static const char szErrorCantBindSocket3[]     = "%s - Error: %d (%d) - Can't bind UNIX socket\n";
static const char szErrorCantSetSockOpt3[]     = "%s - Error: %d (%d) - Can't set socket options\n";
static const char szErrorCantGetHostName3[]    = "%s - Error: %d (%d) - Can't get host name\n";
#ifdef RLIMIT_PTHREAD
static const char szErrorIncorrectOSRes2[]     = "%s - Error: %d - Incorrect OS resources limit\n";
#endif
static const char szErrorCantLoadInNonRoot[]   = "%s - Error: %d - Can't load daemon in \"non root\" mode\n";
static const char szErrorCantMakeTempFile2[]   = "%s - Error: %d - Can't make temp file name\n";
static const char szErrorSystemCantFork3[]     = "%s - Error: %d (%d) - System Can't fork()\n";
static const char szErrorInvalidParameter3[]   = "%s - Error: %d - Invalid parameter(s) - \"%s\"\n";
static const char szErrorPathNotAllowed2[]     = "%s - Error: %d - \"Resources\" path \"/\" is not allowed\n";
static const char szErrorPathNotAbsolute3[]    = "%s - Error: %d - \"Resources\" path \"%s\" is not absolute\n";
static const char szErrorInvalidPortNum3[]     = "%s - Error: %d - Invalid \"Listen\" port number \"%s\"\n";
static const char szErrorInvalidAcceptSocket[] = "%s - Error: %d - Invalid \"Accept\" socket - handle \"0\" must be valid stream socket\n";
#ifdef FORK_VER
static const char szErrorCantOpenLockFile3[]   = "%s - Error: Can't open lock file \"%s\" - %s\n";
static const char szErrorCantLockLockFile3[]   = "%s - Error: Can't lock lock file \"%s\" - %s\n";
#endif
#ifdef SSRV_SSDB_INCLUDE
static const char szErrorCantInitSSDBAPI4[]    = "%s - Error: %d (%d) - Can't init db for \"%s\" reason\n";
#endif
#ifdef SSRV_SWITCHER_ON
static const char szWarningCantFindSwcLib2[]   = "%s - Warning: Can't find \"Switcher\" library \"%s\"\n";
#endif
static const char szWarningCantFindResPath2[]  = "%s - Warning: Can't find \"%s\" resources path\n";
static const char szIllegalCPUcommand[]        = "Invalid CPU command";
static const char szIllegalFPEcommand[]        = "Invalid FPE command";
#ifndef FORK_VER
static const char szCantInitMutex[]            = "Can't init mutex \"%s\"";
static const char szCantInitConVar[]           = "Can't init condition var \"%s\"";
static const char szCantInitThread[]           = "Can't init \"%s\" thread";
#endif
static const char szCantAllocTcbEntry[]        = "Can't alloc \"thread control block\" entry";
static const char szCantBindSocketOnPort[]     = "Can't bind \"listen\" socket on port: %d";
static const char szCantListenSocket[]         = "\"Listen\" error, can't accept new sockets";
static const char szResPathNotAllowed[]        = "Resources path \"/\" is not allowed";
static const char szResPathNotAbsolute[]       = "Resources path \"%s\" is not absolute";
#ifdef SSRV_SWITCHER_ON
static const char szCantFindSwitcherLibrary[]  = "Can't find \"Switcher\" library \"%s\"";
static const char szCantLoadSwitcherLibrary[]  = "Can't load \"%s\" library - \"%s\"";
static const char szCantInitSwitcherLibrary[]  = "Can't init \"%s\" library";
#endif
static const char szCantFindResourcePath[]     = "Can't find \"%s\" resources path";
static const char szInvalidListenPortNum[]     = "Invalid \"listen\" port number \"%s\"";
#ifdef SSRV_SSDB_INCLUDE
static const char szCantInitSSDBAPI[]          = "Can't init database for \"%s\" reason, errcode: %d";
static const char szCantLoadIpFilterInfo[]     = "Can't load IP filter from database \"%s\", table \"%s\", total errors: %ld";
static const char szCantLoadUserEntryInfo[]    = "Can't load \"username\" and \"password\" from database \"%s\", table \"%s\", total errors: %ld";
static const char szCantConnectToDb[]          = "Can't connect to database \"%s\", total errors: %ld";
#endif
static const char szCantFindEntryPoint[]       = "Can't find entry point \"%s\" in library \"%s\"";
/* ------------------------------------------------------------------------- */
/* Globals                                                                   */
int volatile debug_mode                        = 0;              /* Debug mode flag */
int volatile silenceMode                       = 0;              /* Silence mode flag */
int volatile cResourcePathSize                 = 0;              /* Resource path size */
int volatile listen_port                       = SSRV_DEFAULT_LISTENPORT;        /* Port for listen */
int volatile dir_listing_flg                   = SSRV_DEFAULT_DIR_LISTING_VALUE; /* Directory listing enable/disable flag */
unsigned long volatile exit_phase              = 0;              /* Exit phase */
SSRVTATINFO est;                                                 /* Statistics storage struct */
char szServerVersionDate[128]                  = "22 Jul 1999";  /* Server version date */
char szResourcePath[512];                                        /* Resource path */
#ifdef SSRV_SWITCHER_ON
char szSwitcherLibPath[512];                                     /* "Switcher" library path and name */
SWCEXPORT swexf;                                                 /* "Switcher" export function structure */
#endif
char szDefaultHtmlFileName[128];                                 /* Default HTML file for "/" request */
/* ------------------------------------------------------------------------- */
static int volatile inetd_mode                 = 0;                          /* inetd mode flag */
static int volatile inetd_sock                 = INVALID_SOCKET_VAL;         /* inetd socket */
static int volatile alive_timeout              = SSRV_DEFAULT_INETD_TIMEOUT; /* alive timeout for inetd mode */
static int volatile disable_unload_step        = 0;              /* disable unload timeout decrement for inetd mode */
static int volatile privModeFlag               = 0;              /* Priviledge mode */
static int volatile unameInitedFlag            = 0;              /* "username" and "password" inited flag */
static int volatile enable_esptag              = 1;              /* Enable "ESP" tag in syslog messages */
static unsigned long volatile need_init        = 0;              /* Need reinit flag */
static unsigned long volatile inited_mutex     = 0;              /* Inited Flag: Mutex */
static unsigned long volatile inited_cond      = 0;              /* Inited Flag: Condition variables */
static IPADDRE *freeIpAddrList                 = 0;              /* Free Ip Address Entry list */
static IPADDRE *whiteIpAddrList                = 0;              /* "White" IP addr list */
static IPADDRE *darkIpAddrList                 = 0;              /* "Dark" IP addr list */
static TCBENTRY *freeTcbEntryList              = 0;              /* Free ThreadControlBlockEntry list */
#ifdef SSRV_SWITCHER_ON
static void *swcdso_handle                     = 0;              /* "Switcher" dso handler */
static time_t errloadswclib_fixtime            = (time_t)(-1);   /* Can't load "switcher" dso fix time */
#endif
static char szFixStartTime[64];                                  /* Start time string */
static char szAccessUserName[128];                               /* "Username" for access to configuration */
static char szAccessPassword[128];                               /* "Password" for access to configuration */
static char szLocalHostName[MAXHOSTNAMELEN];                     /* Local host name */

#ifdef SSRV_SSDB_INCLUDE
static char *lpszDatabaseName                  = (char*)&szDatabaseName[0];     /* Database name */
static char *lpszDbAddrTableName               = (char*)&szDbAddrTableName[0];  /* Database "Addr" Table name */
static char *lpszDbKeyTableName                = (char*)&szDbKeyTableName[0];   /* Database "Key" Table name */
#else
static char *lpszDatabaseName                  = 0;                             /* Database name */
static char *lpszDbAddrTableName               = 0;                             /* Database "Addr" Table name */
static char *lpszDbKeyTableName                = 0;                             /* Database "Key" Table name */
#endif
/* ------------------------------------------------------------------------- */
#ifndef FORK_VER
static pthread_t listen_tid;          /* "Listen" thread id */
static pthread_t info_tid;            /* "Info" thread id */
static pthread_t confl_tid;           /* "ConfigLoader" thread id */
static pthread_t unloader_tid;        /* "Unloader" thread id (only for inetd_mode) */
static pthread_t killer_tid;          /* "Killer" thread id */
static DMUTEX start_listen_mutex;     /* "Synchro Start" Mutex for Listen thread */
static DMUTEX start_info_mutex;       /* "Synchro Start" Mutex for Info thread */
static DMUTEX start_killer_mutex;     /* "Synchro Start" Mutex for Killer thread */
static DMUTEX start_confl_mutex;      /* "Synchro Start" Mutex for ConfLoader thread */
static DMUTEX start_unloader_mutex;   /* "Synchro Start" Mutex for Unloader thread */
static DMUTEX exit_phase_mutex;       /* "Exit Phase" mutex  for unload */
static DMUTEX reinit_mutex;           /* "Reinit" mutex for reinitialization */
static DMUTEX ipaddrlist_mutex;       /* "Free IP address list" mutex */
static DMUTEX tcblist_mutex;          /* "Free TCBENTRY list" mutex */
static DMUTEX switcher_mutex;         /* "Switcher" mutex */
static DMUTEX password_mutex;         /* "Password" mutex */
static DCOND exit_condition;          /* "Exit" condition variable */
static DCOND reinit_condition;        /* "Reinit" condition variable */
static DCOND password_condition;      /* "Password(s) ready" condition variable */
#endif
/* ========================================================================= */
/*                          SSSERVER DAEMON CODE                             */
/* ========================================================================= */
/* ----------------------- zinit_resources --------------------------------- */
static void zinit_resources(void)
{ time_t t; 
  char *c;
  szFixStartTime[0]    = 0;
#ifndef FORK_VER
  memset(&start_listen_mutex,0,sizeof(DMUTEX));
  memset(&start_info_mutex,0,sizeof(DMUTEX));
  memset(&start_killer_mutex,0,sizeof(DMUTEX));
  memset(&start_confl_mutex,0,sizeof(DMUTEX));
  memset(&start_unloader_mutex,0,sizeof(DMUTEX));
  memset(&exit_phase_mutex,0,sizeof(DMUTEX));
  memset(&reinit_mutex,0,sizeof(DMUTEX));
  memset(&ipaddrlist_mutex,0,sizeof(DMUTEX));
  memset(&tcblist_mutex,0,sizeof(DMUTEX));
  memset(&switcher_mutex,0,sizeof(DMUTEX));
  memset(&password_mutex,0,sizeof(DMUTEX));
  memset(&exit_condition,0,sizeof(DCOND));
  memset(&reinit_condition,0,sizeof(DCOND));
  memset(&password_condition,0,sizeof(DCOND));
#endif
  memset(&est,0,sizeof(SSRVTATINFO));
#ifdef SSRV_SWITCHER_ON
  memset(&swexf,0,sizeof(SWCEXPORT));
#endif
  memset(szLocalHostName,0,sizeof(szLocalHostName));
  if((t = time(NULL)) != (time_t)(-1))
  { strncpy(szFixStartTime,asctime(localtime(&t)),sizeof(szFixStartTime)-1);  /* Fix start time */
    for(c = szFixStartTime;*c && *c != '\n' && *c != '\r';c++);
    *c = 0;
  }
  else strcpy(szFixStartTime,szUnknownTime);
  privModeFlag = (!getuid() && !geteuid()) ? 1 : 0;
  strncpy(szResourcePath,SSRV_DEFAULT_RESOURCESPATH,sizeof(szResourcePath));
  cResourcePathSize = strlen(szResourcePath);
#ifdef SSRV_SWITCHER_ON
  strncpy(szSwitcherLibPath,SSRV_DEFAULT_SWITCHLIBPATH,sizeof(szSwitcherLibPath));
  szSwitcherLibPath[sizeof(szSwitcherLibPath)-1] = 0;
#endif
  strncpy(szDefaultHtmlFileName,SSRV_DEFAULTHTMLFILE,sizeof(szDefaultHtmlFileName));
  szDefaultHtmlFileName[sizeof(szDefaultHtmlFileName)-1] = 0;
#ifdef INCLUDE_TIME_DATE_STRINGS
  sprintf(szServerVersionDate,"%s %s",__TIME__,__DATE__);
#endif
  strcpy(szAccessUserName,"Vladimir Legalov");
  strcpy(szAccessPassword,"legalov@sgi.com");
#ifndef FORK_VER
  snprintf(szSSServerInfoSockPath,sizeof(szSSServerInfoSockPath),szInfoSockPathMask,szDaemonName);
#else
  sprintf(szSSServerInfoSockPath,szInfoSockPathMask,szDaemonName);
#endif
}
/* ----------------------- make_sockaddr ----------------------------------- */
static int make_sockaddr(struct sockaddr_un *s_saddr,const char *sock_path)
{ int addrlen = 0;
  if(s_saddr)
  { memset((char *)s_saddr,0,sizeof(struct sockaddr_un));
    if(sock_path) strcpy(s_saddr->sun_path,sock_path);
    s_saddr->sun_family = AF_UNIX;
    addrlen = strlen(s_saddr->sun_path) + sizeof(s_saddr->sun_family);
  }
  return addrlen;
}
/* ----------------------- prepare_cmdbuffer ------------------------------- */
static void prepare_cmdbuffer(char *buf,char cmd)
{ buf[0] = 'L'; buf[1] = 'V'; buf[2] = 'A'; buf[3] = cmd; 
}
/* ------------------------- ssrvCloseSocket ------------------------------- */
int ssrvCloseSocket(int s)
{ for(;;) if(!close(s) || (errno != EINTR)) break;
  return INVALID_SOCKET_VAL;
}
/* --------------------- flush_listen_socket ------------------------------- */
static void flush_listen_socket(int s)
{ struct sockaddr_in caddr;
  int caddr_len,as;
  if(s != INVALID_SOCKET_VAL && inetd_mode)
  { as = accept(s,&caddr,&caddr_len);
    if(as >= 0) ssrvCloseSocket(as);
  }
}
/* ---------------------------- ssrvExit ----------------------------------- */
static void ssrvExit(int exit_code)
{ flush_listen_socket(inetd_sock);
  exit(exit_code);
}
/* ------------------------- unlink_name ----------------------------------- */
static void unlink_name(const char *socket_path)
{ for(;;) if(!unlink(socket_path) || (errno != EINTR)) break;
}

#ifndef FORK_VER
/* ------------------------- close_unlink ---------------------------------- */
static void close_unlink(int *s,const char *socket_path)
{ *s = ssrvCloseSocket(*s); unlink_name(socket_path);
}
#endif /* #ifndef FORK_VER */

/* ---------------------- setsockopt_trusted ------------------------------- */
static int setsockopt_trusted(int s)
#ifndef FORK_VER
{ return (tsix_on(s) != (-1)) ? 1 : 0; }
#else
{ return 1; /* IRIX 5.3 has not tsix_.. functions :( */ }
#endif
/* --------------------- setsockopt_reuseaddr ------------------------------ */
static int setsockopt_reuseaddr(int s)
{ int one = 1;
  return (setsockopt(s,SOL_SOCKET,SO_REUSEADDR,(const void *)&one,sizeof(int)) < 0) ? 0 : 1;
}
/* --------------------- setsockopt_keepalive ------------------------------ */
static int setsockopt_keepalive(int s)
{ int one = 1;
  return (setsockopt(s,SOL_SOCKET,SO_KEEPALIVE,(const void *)&one,sizeof(int)) < 0) ? 0 : 1;
}
/* ------------------- setsockopt_disable_nagle ---------------------------- */
static int setsockopt_disable_nagle(int s)
{ int one = 1;
  return (setsockopt(s,IPPROTO_TCP,TCP_NODELAY,(const void *)&one,sizeof(int)) < 0) ? 0 : 1;
}
/* ------------------- setsockopt_enable_linger ---------------------------- */
static int setsockopt_enable_linger(int s)
{ struct linger li;
  li.l_onoff  = 1;
  li.l_linger = SSRV_LINGER_TIMEOUT;
  return (setsockopt(s,SOL_SOCKET,SO_LINGER,(const void *)&li,sizeof(struct linger)) < 0) ? 0 : 1;
}

#ifndef FORK_VER
/* ------------------------- init_dmutex ----------------------------------- */
static int init_dmutex(DMUTEX *dm)
{ int retcode = 0;
  if(dm)
  { memset(dm,0,sizeof(DMUTEX));
    if(!pthread_mutex_init(&dm->mutex,0)) { dm->init++; retcode++; }
  }
  return retcode;
}
/* ------------------------ destroy_dmutex --------------------------------- */
static void destroy_dmutex(DMUTEX *dm)
{ if(dm && dm->init)
  { pthread_mutex_lock(&dm->mutex);  dm->init = 0; pthread_mutex_unlock(&dm->mutex);
    pthread_mutex_destroy(&dm->mutex);
  }
}
/* -------------------------- init_dcond ----------------------------------- */
static int init_dcond(DCOND *dc)
{ int retcode = 0;
  if(dc)
  { memset(dc,0,sizeof(DCOND));
    if(!pthread_cond_init(&dc->cond,NULL)) { dc->init++; retcode++; }
  }
  return retcode;
}
/* ------------------------- destroy_dcond --------------------------------- */
static void destroy_dcond(DCOND *dc)
{ if(dc && dc->init) { dc->init = 0; pthread_cond_destroy(&dc->cond); }
}
#endif /* #ifndef FORK_VER */

/* ------------------------ ssrvStr2InetAddr ------------------------------- */
static unsigned long ssrvStr2InetAddr(const char *_ipaddrstr,int *_mask)
{ char tmp[16];
  char *c,*c4,*c3,*c2,*c1;
  unsigned long adr;
  int i;

  if(!_ipaddrstr || !_ipaddrstr[0]) return 0;
  *_mask = 0; adr = inet_addr(_ipaddrstr);
  if(adr != INADDR_NONE) return ntohl(adr);
  adr = 0;
  if(strlen(_ipaddrstr) > sizeof(tmp)-1) return 0;
  strcpy(tmp,_ipaddrstr);
  c = tmp;           /* 111.222.333.444 */
            c4 = c;  /* 111 */
  for(i = 0;*c && *c != '.';i++,c++)
   if(*c != '*' && !isdigit((int)*c)) return 0;
  if(!*c || i > 3 || i == 0) return 0;
  *c++ = 0; c3 = c; /* 222 */

  for(i = 0;*c && *c != '.';i++,c++)
   if(*c != '*' && !isdigit((int)*c)) return 0;
  if(!*c || i > 3 || i == 0) return 0;
  *c++ = 0; c2 = c; /* 333 */

  for(i = 0;*c && *c != '.';i++,c++)
   if(*c != '*' && !isdigit((int)*c)) return 0;
  if(!*c || i > 3 || i == 0) return 0;
  *c++ = 0; c1 = c; /* 444 */

  for(i = 0;*c;i++,c++)
   if(*c != '*' && !isdigit((int)*c)) return 0;
  if(i > 3 || i == 0) return 0;

  if(*c4 == '*') { *_mask |= 8; adr |= 0xff000000; }
  else
  { if(sscanf(c4,"%d",&i) != 1 || i > 255) return 0;
    adr |= (i & 0xff) << 24;
  }

  if(*c3 == '*') { *_mask |= 4; adr |= 0x00ff0000; }
  else
  { if(sscanf(c3,"%d",&i) != 1 || i > 255) return 0;
    adr |= (i & 0xff) << 16;
  }

  if(*c2 == '*') { *_mask |= 2; adr |= 0x0000ff00; }
  else
  { if(sscanf(c2,"%d",&i) != 1 || i > 255) return 0;
    adr |= (i & 0xff) << 8;
  }

  if(*c1 == '*') { *_mask |= 1; adr |= 0x000000ff; }
  else
  { if(sscanf(c1,"%d",&i) != 1 || i > 255) return 0;
    adr |= (i & 0xff);
  }
  return adr;
}
/* ------------------------- newIpAddrEntry -------------------------------- */
static IPADDRE *newIpAddrEntry(const char *_ipaddrstr)
{ int _mask; unsigned long adr;
  IPADDRE *ipe = 0;

  if((adr = ssrvStr2InetAddr(_ipaddrstr,&_mask)) != 0)
  { if((ipe = freeIpAddrList) != 0) { freeIpAddrList = ipe->next; est.statGlbIpAddrEntryFreeCnt--; }
    else { if((ipe = (IPADDRE *)malloc(sizeof(IPADDRE))) != 0) est.statGlbIpAddrEntryAllocCnt++; }
    if(ipe)
    { memset(ipe,0,sizeof(IPADDRE));
      ipe->ipaddr = adr; ipe->mask = _mask;
    }
  }
  return ipe;
}
/* ------------------------ deleteIpAddrEntry ------------------------------ */
static void deleteIpAddrEntry(IPADDRE *ipa)
{ if(ipa)
  { ipa->next = freeIpAddrList; freeIpAddrList = ipa; est.statGlbIpAddrEntryFreeCnt++;
  }
}
/* ------------------------ clearAllIpAddrLists ---------------------------- */
static void clearAllIpAddrLists(void)
{ IPADDRE *ipa;
  while((ipa = whiteIpAddrList) != 0) { whiteIpAddrList = ipa->next; deleteIpAddrEntry(ipa); }
  while((ipa = darkIpAddrList) != 0)  { darkIpAddrList = ipa->next;  deleteIpAddrEntry(ipa); }
}
/* --------------------- pushIpAddrIntoWhiteList --------------------------- */
static int pushIpAddrIntoWhiteList(IPADDRE *ipa)
{ IPADDRE *ip;
  for(ip = whiteIpAddrList;ip;ip = ip->next)
  { if((ipa->ipaddr == ip->ipaddr) && (ipa->mask == ip->mask)) return 0;
  }
  ipa->next = whiteIpAddrList; whiteIpAddrList = ipa;
  return 1;
}
/* --------------------- pushIpAddrIntoDarkList ---------------------------- */
static int pushIpAddrIntoDarkList(IPADDRE *ipa)
{ IPADDRE *ip;
  for(ip = darkIpAddrList;ip;ip = ip->next)
  { if((ipa->ipaddr == ip->ipaddr) && (ipa->mask == ip->mask)) return 0;
  }
  ipa->next = darkIpAddrList; darkIpAddrList = ipa;
  return 1;
}
/* ------------------------ isEquIpAddrEntry ------------------------------- */
static int isEquIpAddrEntry(IPADDRE *ipa,unsigned long _addr)
{ if(ipa)
  { if(ipa->mask)
    { if((ipa->mask & 8) == 0 && ((_addr & 0xff000000) != (ipa->ipaddr & 0xff000000))) return 0;
      if((ipa->mask & 4) == 0 && ((_addr & 0x00ff0000) != (ipa->ipaddr & 0x00ff0000))) return 0;
      if((ipa->mask & 2) == 0 && ((_addr & 0x0000ff00) != (ipa->ipaddr & 0x0000ff00))) return 0;
      if((ipa->mask & 1) == 0 && ((_addr & 0x000000ff) != (ipa->ipaddr & 0x000000ff))) return 0;
      return 1;
    }
    return _addr == ipa->ipaddr;
  }
  return 0;
}

/* -------------------------- newTcbEntry ---------------------------------- */
#ifndef FORK_VER
static TCBENTRY *newTcbEntry(void)
{ TCBENTRY *tcb = 0;
  pthread_mutex_lock(&tcblist_mutex.mutex);
  if((tcb = freeTcbEntryList) != 0) { freeTcbEntryList = tcb->next; est.statGlbTcbEntryFreeCnt--; }
  pthread_mutex_unlock(&tcblist_mutex.mutex);
  if(!tcb) if((tcb = (TCBENTRY *)malloc(sizeof(TCBENTRY))) != 0) est.statGlbTcbEntryAllocCnt++;
  if(tcb)
  { memset(tcb,0,sizeof(TCBENTRY));
    tcb->struct_size = sizeof(TCBENTRY);
  }
  return tcb;
}
#else
static TCBENTRY *newTcbEntry(void)
{ TCBENTRY *tcb = 0;
  if((tcb = (TCBENTRY *)malloc(sizeof(TCBENTRY))) != 0) est.statGlbTcbEntryAllocCnt++;
  if(tcb)
  { memset(tcb,0,sizeof(TCBENTRY));
    tcb->struct_size = sizeof(TCBENTRY);
  }
  return tcb;
}
#endif /* #ifndef FORK_VER */


/* ------------------------- deleteTcbEntry -------------------------------- */
#ifndef FORK_VER
static void deleteTcbEntry(TCBENTRY *tcb)
{ if(tcb && (tcb->struct_size == sizeof(TCBENTRY)))
  { pthread_mutex_lock(&tcblist_mutex.mutex);
    tcb->next = freeTcbEntryList; freeTcbEntryList = tcb; est.statGlbTcbEntryFreeCnt++;
    pthread_mutex_unlock(&tcblist_mutex.mutex);
  }
}
#else
static void deleteTcbEntry(TCBENTRY *tcb)
{ if(tcb && (tcb->struct_size == sizeof(TCBENTRY))) free(tcb);
}
#endif /* #ifndef FORK_VER */


/* --------------------- activate_exit_phase ------------------------------- */
#ifndef FORK_VER
static void activate_exit_phase(void)
{ if(!exit_phase)
  { if(inited_mutex && inited_cond)
    { pthread_mutex_lock(&exit_phase_mutex.mutex); exit_phase++; pthread_cond_signal(&exit_condition.cond);
      pthread_mutex_unlock(&exit_phase_mutex.mutex);
    }
    else exit_phase++;
  }
}
#else
static void activate_exit_phase(void) { exit_phase++; }
#endif /* #ifndef FORK_VER */

/* ----------------------------- syslog_msg -------------------------------- */
static int syslog_msg(int pri,int sqnum,const char *msg,...)
{ char tmp[2048];
  va_list args;
  va_start(args,msg);
#ifndef FORK_VER
  vsnprintf(tmp,sizeof(tmp)-1,msg,args);
#else
  vsprintf(tmp,msg,args); /* IRIX 5.3 has not vsnprintf :( */
#endif
  va_end(args);
  openlog(szDaemonName,LOG_PID|LOG_CONS,LOG_DAEMON);
  if(enable_esptag) syslog(pri,"|$(0x%lX)%s - (%s mode)",(unsigned long)sqnum,tmp,debug_mode ? szDebug : (inetd_mode ? szInetd : szNormal));
  else              syslog(pri,"%s - (%s mode)",tmp,debug_mode ? szDebug : (inetd_mode ? szInetd : szNormal));
  closelog();
  return 1;
}

/* ------------------ ssrvIsClientIPAddressEnabled ------------------------- */
static int ssrvIsClientIPAddressEnabled(TCBENTRY *tcb)
{ unsigned long haddr;
  IPADDRE *ip;
  int addr_enabled = 0;

  if(tcb)
  { haddr = ntohl(tcb->caddr.sin_addr.s_addr);
    addr_enabled++;
#ifndef FORK_VER
    pthread_mutex_lock(&ipaddrlist_mutex.mutex);
#endif
    for(ip = darkIpAddrList;ip;ip = ip->next)
    { if(isEquIpAddrEntry(ip,haddr))
      { addr_enabled = 0;
#ifdef INCDEBUG
        dprintf("IP addr %s in dark list\n",tcb->caddr_str);
#endif
        break;
      }
    }
    if(!addr_enabled)
    { for(ip = whiteIpAddrList;ip;ip = ip->next)
      { if(isEquIpAddrEntry(ip,haddr))
        { addr_enabled++;
#ifdef INCDEBUG
          dprintf("IP addr %s in white list\n",tcb->caddr_str);
#endif
          break;
        }
      }
    }
#ifndef FORK_VER
    pthread_mutex_unlock(&ipaddrlist_mutex.mutex);
#endif
  }
  return addr_enabled;
}

/* ----------------------- ssrvIsUserValid --------------------------------- */
int ssrvIsUserValid(const char *username,const char *password)
{ int retcode = 0;
  if(!exit_phase)
  {
#ifndef FORK_VER
    pthread_mutex_lock(&password_mutex.mutex);
#endif
    if(unameInitedFlag)
    { retcode++;
      if((szAccessUserName[0] && (!username || strcmp(username,szAccessUserName))) ||
         (!szAccessUserName[0] && username && username[0])) retcode = 0;
      if((szAccessPassword[0] && (!password || strcmp(password,szAccessPassword))) ||
         (!szAccessPassword[0] && password && password[0])) retcode = 0;
    }
#ifndef FORK_VER
    pthread_mutex_unlock(&password_mutex.mutex);
#endif
  }
  return retcode;
}

#ifndef FORK_VER
/* ------------------ ssrvDeclareReloadIPFilterInfo ------------------------ */
void ssrvDeclareReloadIPFilterInfo(void)
{ if(!exit_phase && inited_mutex && inited_cond)
  { pthread_mutex_lock(&reinit_mutex.mutex); need_init++; pthread_cond_signal(&reinit_condition.cond);
    pthread_mutex_unlock(&reinit_mutex.mutex);
  }
}
#endif /* #ifndef FORK_VER */

#ifdef SSRV_SWITCHER_ON
/* ----------------- isSyslogEnabledForSwitcherLoader ---------------------- */
static int isSyslogEnabledForSwitcherLoader(void)
{ time_t t;
  int retcode = 0;
  if((errloadswclib_fixtime == (time_t)(-1)) ||
     (((t = time(NULL)) != (time_t)(-1)) && (TOUTMAX_FIXSWCLOADERR < (int)difftime(t,errloadswclib_fixtime))))
  { errloadswclib_fixtime = time(NULL);
    retcode++;
  }
  return retcode;
}
/* --------------------- ssrvLoadSwitcherModule ---------------------------- */
int ssrvLoadSwitcherModule(char *errmsgbuf,int bufsize)
{ SSRVEXPORT exf;
  time_t t;
  char *c,*fpname;
  int lfp_error = 0;

  if(errmsgbuf && bufsize) errmsgbuf[0] = 0;
  else                     errmsgbuf = 0;

  pthread_mutex_lock(&switcher_mutex.mutex);
  if(swcdso_handle)
  { pthread_mutex_unlock(&switcher_mutex.mutex);
    return 1; /* Ok - already loaded */
  }

  /* Try load "switcher" module */
  if((swcdso_handle = dlopen(szSwitcherLibPath,RTLD_LAZY)) == NULL)
  { est.statConSwchLibLoadError++;
    if(((c = dlerror()) != 0) && errmsgbuf)
    { snprintf(errmsgbuf,bufsize,szCantLoadSwitcherLibrary,szSwitcherLibPath,c ? c : szUnknownError );
      errmsgbuf[bufsize-1] = 0;
    }
    if(isSyslogEnabledForSwitcherLoader())
     syslog_msg(LOG_ERR,SRVSEQN_CANFINDSWCLIB,szCantLoadSwitcherLibrary,szSwitcherLibPath,c ? c : szUnknownError);
    pthread_mutex_unlock(&switcher_mutex.mutex);
    return 0; /* Can't library */
  }

  /* Prepare Function Export Block */
  memset(&exf,0,sizeof(SSRVEXPORT));
  exf.struct_size                      = sizeof(SSRVEXPORT);
  exf.fp_ssrvGetIntegerByTcb           = ssrvGetIntegerByTcb;
  exf.fp_ssrvGetStringByTcb            = ssrvGetStringByTcb;
  exf.fp_ssrvSetIntegerByTcb           = ssrvSetIntegerByTcb;
  exf.fp_ssrvSetStringByTcb            = ssrvSetStringByTcb;
  exf.fp_ssrvDeclareReloadIPFilterInfo = ssrvDeclareReloadIPFilterInfo;

  fpname = 0;

  /* Try resolv function pointers */
       if((swexf.fp_swcInitSwitcher = (FPswcInitSwitcher*)dlsym(swcdso_handle,(fpname = szFNswcInitSwitcher))) == 0) lfp_error++;
  else if((swexf.fp_swcDoneSwitcher = (FPswcDoneSwitcher*)dlsym(swcdso_handle,(fpname = szFNswcDoneSwitcher))) == 0) lfp_error++;
  else if((swexf.fp_swcBeginProc    = (FPswcBeginProc*)dlsym(swcdso_handle,(fpname = szFNswcBeginProc))) == 0) lfp_error++;
  else if((swexf.fp_swcContProc     = (FPswcContProc*)dlsym(swcdso_handle,(fpname = szFNswcContProc))) == 0) lfp_error++;
  else if((swexf.fp_swcEndProc      = (FPswcEndProc*)dlsym(swcdso_handle,(fpname = szFNswcEndProc))) == 0) lfp_error++;

  if(lfp_error && fpname)
  { if(errmsgbuf) { snprintf(errmsgbuf,bufsize,szCantFindEntryPoint,fpname,szSwitcherLibPath); errmsgbuf[bufsize-1] = 0; }
    if(isSyslogEnabledForSwitcherLoader())
     syslog_msg(LOG_ERR,SRVSEQN_CANFINDSWCLIB,szCantFindEntryPoint,fpname,szSwitcherLibPath);
  }

  if(lfp_error || !swexf.fp_swcInitSwitcher(&exf))
  { est.statConSwchLibInitError++;
    memset(&swexf,0,sizeof(SWCEXPORT));
    dlclose(swcdso_handle); swcdso_handle = 0;
    if(errmsgbuf) { snprintf(errmsgbuf,bufsize,szCantInitSwitcherLibrary,szSwitcherLibPath); errmsgbuf[bufsize-1] = 0; }
    if(isSyslogEnabledForSwitcherLoader())
     syslog_msg(LOG_ERR,SRVSEQN_CANFINDSWCLIB,szCantInitSwitcherLibrary,szSwitcherLibPath);
    pthread_mutex_unlock(&switcher_mutex.mutex);
    return 0;   
  }
  swexf.struct_size = sizeof(SWCEXPORT);
  pthread_mutex_unlock(&switcher_mutex.mutex);
  return 1; 
}
#endif /* #ifdef SSRV_SWITCHER_ON */

/* ===================== thread_func_connection ============================ */
void *thread_func_connection(void *_tcb)
{ if(_tcb)
  {
#ifndef FORK_VER
    pthread_mutex_lock(&start_listen_mutex.mutex); pthread_mutex_unlock(&start_listen_mutex.mutex);

    if(inetd_mode && !exit_phase)
    { pthread_mutex_lock(&start_unloader_mutex.mutex);
      disable_unload_step++;
      pthread_mutex_unlock(&start_unloader_mutex.mutex);
    }
#endif

    if(ssrvIsClientIPAddressEnabled((TCBENTRY *)_tcb)) ssrvConnectionProcessor((TCBENTRY *)_tcb);
    ssrvCloseSocket(((TCBENTRY *)_tcb)->as);
    deleteTcbEntry((TCBENTRY *)_tcb);  

#ifndef FORK_VER
    if(inetd_mode && !exit_phase)
    { pthread_mutex_lock(&start_unloader_mutex.mutex);
      alive_timeout = inetd_mode; disable_unload_step--;
      pthread_mutex_unlock(&start_unloader_mutex.mutex);
    }
#endif
  }
#ifdef INCDEBUG
  dprintf("* Close Thread: - Total Created thr: %ld, - Free: %ld Alloc: %ld *\n",
          est.statLsTotalThreadCreated,est.statGlbTcbEntryFreeCnt,est.statGlbTcbEntryAllocCnt);
#endif
  return 0;
}
/* ======================= thread_func_listen ============================== */
void *thread_func_listen(void *thread_param)
{ struct timespec time_val;
  struct sockaddr_in saddr;
  int s,i,opensock_timeout,alloctcb_timeout;
#ifdef FORK_VER
  pid_t pid;
#endif
  int cantbindmsg_flg = 0;
  int cantlistenmsg_flg = 0;
  TCBENTRY *tcb = 0;

#ifndef FORK_VER
  pthread_mutex_lock(&start_listen_mutex.mutex); pthread_mutex_unlock(&start_listen_mutex.mutex);
#endif
  opensock_timeout = TOUTBASE_LSOPENSOCK;
  alloctcb_timeout = TOUTBASE_LSALLOCTCB;
 
  for(s = inetd_mode ? inetd_sock : INVALID_SOCKET_VAL;!exit_phase;)
  { /* Waiting for "Password ready" status (for inets mode need timeout) */
#ifndef FORK_VER
    if(inetd_mode)
    { pthread_mutex_lock(&password_mutex.mutex);
      if(!unameInitedFlag)
      { memset(&time_val,0,sizeof(time_val)); time_val.tv_sec = (time_t)(SSRV_SSDBWAIT_TIMEOUT)+time(NULL);
        pthread_cond_timedwait(&password_condition.cond,&password_mutex.mutex,&time_val);
      }
      i = unameInitedFlag;
      pthread_mutex_unlock(&password_mutex.mutex);
      if(!i)
      { flush_listen_socket(s); /* drop new connection */
        activate_exit_phase();
      }
    }
    else /* normal or debug mode */
    { pthread_mutex_lock(&password_mutex.mutex);
      if(!unameInitedFlag) pthread_cond_wait(&password_condition.cond,&password_mutex.mutex);
      pthread_mutex_unlock(&password_mutex.mutex);
    }
    if(exit_phase) break;
#endif

    if(!tcb && (tcb = newTcbEntry()) == 0)
    { if(!inetd_mode) syslog_msg(LOG_ERR,SRVSEQN_CANTALLOCTCB,szCantAllocTcbEntry);
      sleep(alloctcb_timeout); alloctcb_timeout += TOUTPLUS_LSALLOCTCB;
      if(alloctcb_timeout > TOUTMAX_LSALLOCTCB) alloctcb_timeout = TOUTMAX_LSALLOCTCB;
      continue;
    }
    else alloctcb_timeout = TOUTBASE_LSALLOCTCB;

    if(s == INVALID_SOCKET_VAL)
    { est.statLsTotalOpenSocket++;
      if((s = socket(AF_INET,SOCK_STREAM,0)) != INVALID_SOCKET_VAL)
      { est.statLsTotalOpenSocketSuccess++;
        if(!setsockopt_trusted(s))       est.statLsTotalSSOTrustedError++;
        if(!setsockopt_reuseaddr(s))     est.statLsTotalSSOReuseAddrError++;
        if(!setsockopt_keepalive(s))     est.statLsTotalSSOKeepAliveError++;
        if(!setsockopt_disable_nagle(s)) est.statLsTotalSSODisNagleError++;
        if(!setsockopt_enable_linger(s)) est.statLsTotalSSOEnbLingerError++;

        memset(&saddr,0,sizeof(saddr));
        saddr.sin_family      = AF_INET;
        saddr.sin_addr.s_addr = INADDR_ANY;
        saddr.sin_port        = (u_short)htons((u_short)listen_port);

        est.statLsTotalBindSocket++;
        if(bind(s,(struct sockaddr*)&saddr,sizeof(saddr)) == INVALID_SOCKET_VAL)
        { s = ssrvCloseSocket(s);
          est.statLsTotalBindSocketError++;
	  if(!cantbindmsg_flg) cantbindmsg_flg = syslog_msg(LOG_ERR,SRVSEQN_CANBINDSOCKET,szCantBindSocketOnPort,listen_port);
        }
        else
        { est.statLsTotalBindSocketSuccess++;
          if(listen(s,5) == INVALID_SOCKET_VAL)
          { s = ssrvCloseSocket(s);
            est.statLsTotalListenError++;
	    if(!cantlistenmsg_flg) cantlistenmsg_flg = syslog_msg(LOG_ERR,SRVSEQN_LISTENSOCKETERR,szCantListenSocket);
          }
          else
          { opensock_timeout = TOUTBASE_LSOPENSOCK;
            est.statLsTotalListenSuccess++;
          }
        }
      }
      else est.statLsTotalOpenSocketError++;

      if(s == INVALID_SOCKET_VAL)
      { sleep(opensock_timeout); opensock_timeout += TOUTPLUS_LSOPENSOCK;
        if(opensock_timeout > TOUTMAX_LSOPENSOCK) opensock_timeout = TOUTMAX_LSOPENSOCK;
      }
    }
    else /* if(s == INVALID_SOCKET_VAL) */
    { tcb->caddr_len = sizeof(tcb->caddr); memset(&tcb->caddr,0,sizeof(tcb->caddr_len));
      tcb->as = accept(s,&tcb->caddr,&tcb->caddr_len);
      if(tcb->as < 0 && errno != EINTR)
      { s = ssrvCloseSocket(s); inetd_sock = INVALID_SOCKET_VAL;
        est.statLsTotalAcceptError++;
	if(inetd_mode) activate_exit_phase();
      }
      if(tcb->as >= 0 && !exit_phase)
      { est.statLsTotalAcceptSuccess++;
        if(!setsockopt_keepalive(tcb->as))     est.statLsTotalSSOAccKeepAliveError++;
        if(!setsockopt_disable_nagle(tcb->as)) est.statLsTotalSSOAccDisNagleError++;
        if(!setsockopt_enable_linger(tcb->as)) est.statLsTotalSSOAccEnbLingerError++;
        memset(tcb->caddr_str,0,sizeof(tcb->caddr_str));
        strncpy(tcb->caddr_str,inet_ntoa(tcb->caddr.sin_addr),sizeof(tcb->caddr_str)-1);
        tcb->caddr_str[sizeof(tcb->caddr_str)-1] = 0;
#ifdef INCDEBUG
        dprintf("Accept client: %s\n",tcb->caddr_str);
#endif
#ifndef FORK_VER
        pthread_mutex_lock(&start_listen_mutex.mutex);
        if((i = pthread_create(&tcb->tid,NULL,thread_func_connection,tcb)) == 0)
        { pthread_detach(tcb->tid);
          tcb = 0; est.statLsTotalThreadCreated++;
        }
        else if(i == EPERM)  est.statLsTotalCreThreadPermErr++;
        else if(i == EAGAIN) est.statLsTotalCreThreadNotPermErr++;
        pthread_mutex_unlock(&start_listen_mutex.mutex);
#else
        if((pid = fork()) != (pid_t)(-1))
	{ if(pid == 0)
	  { ssrvCloseSocket(s);
	    thread_func_connection((void*)tcb);
	    exit(SSERR_SUCCESS);
	  }
	}
#endif
      }
      if(tcb && tcb->as >= 0) tcb->as = ssrvCloseSocket(tcb->as);
    }
  } /* end of for */

  if(s != INVALID_SOCKET_VAL) ssrvCloseSocket(s);

  if(tcb) deleteTcbEntry(tcb);

#ifdef INCDEBUG
  dprintf("*** Listen thread exit ***\n");
#endif
  return 0;
}
/* -------------------- ssrvMakeReportBuffer ------------------------------- */
int ssrvMakeReportBuffer(char *buf,int maxbufsize)
{ int i = 0;
  if(maxbufsize <= 0 || !buf) return 0;
  buf[maxbufsize-1] = (buf[0] = 0);

#ifdef INCLUDE_TIME_DATE_STRINGS
  if(i < maxbufsize)
  {
#ifndef FORK_VER
    i = snprintf(buf,maxbufsize,"%s\nSSServer v%d.%d (%s %s)\nDaemon Statistics since: %s, (%s mode), silence: %d\n",
#else
    i = sprintf(buf,"%s\nSSServer v%d.%d (%s %s)\nDaemon Statistics since: %s, (%s mode), silence: %d\n",
#endif
    szDelimitterLine,SSRV_VMAJOR,SSRV_VMINOR,__TIME__,__DATE__,szFixStartTime,debug_mode ? szDebug : (inetd_mode ? szInetd : szNormal),silenceMode);
  }    
#else
  if(i < maxbufsize)
  { i = snprintf(buf,maxbufsize,"%s\nEventMon v%d.%d\nDaemon Statistics since: %s (%s mode), silence: %d\n",
    szDelimitterLine,SSRV_VMAJOR,SSRV_VMINOR,szFixStartTime, debug_mode ? szDebug : (inetd_mode ? szInetd : szNormal),silenceMode);
  }
#endif

  /* Add some "common" infornation */
  if(i < maxbufsize)
#ifndef FORK_VER
   i += snprintf(&buf[i],maxbufsize-i,
#else
   i += sprintf(&buf[i],
#endif
"\
Hostname            : \"%s\"\n\
Listen Port         : %d\n\
Unload timeout      : %d (for inetd_mode)\n\
Directory listing   : \"%s\"\n\
Default resource    : \"%s\"\n\
Resource path       : \"%s\"\n\
Switcher lib path   : \"%s\"\n\
Database name       : \"%s\"\n\
IPAddrTable name    : \"%s\"\n\
KeyTable name       : \"%s\"\n\
IpAddrEntry alloc   : %lu\n\
IpAddrEntry free    : %lu\n\
TcbEntry alloc      : %lu\n\
TcbEntry free       : %lu\n",
                    szLocalHostName,                    /* Host name */
		    listen_port,                        /* Listen port */
		    inetd_mode,                         /* inetd mode flag = unload timeout */
                    dir_listing_flg ? szOn : szOff,     /* Directory listing flag */
		    szDefaultHtmlFileName,              /* Default resource name instead "/" */
                    szResourcePath,                     /* Resource path */
#ifdef SSRV_SWITCHER_ON
		    szSwitcherLibPath,                  /* "Switcher" library path */
#else
                    szNONE,                             /* "Switcher" library path for non switcher version */
#endif
                    lpszDatabaseName ? lpszDatabaseName : szNONE,       /* Database name */
                    lpszDbAddrTableName ? lpszDbAddrTableName : szNONE, /* "Addr" Table name */
		    lpszDbKeyTableName ? lpszDbKeyTableName : szNONE,   /* "Key" Table name */
                    est.statGlbIpAddrEntryAllocCnt,     /* Stat: Global info, total ip_addr_entry_alloc counter */
                    est.statGlbIpAddrEntryFreeCnt,      /* Stat: Global info, total ip_addr_entry_free_list counter */
                    est.statGlbTcbEntryAllocCnt,        /* Stat: Global info, total tcb_entry_alloc counter */
                    est.statGlbTcbEntryFreeCnt);        /* Stat: Global info, total tcb_entry_free_list counter */

  /* Add "Connection thread" information */
  if(i < maxbufsize)
#ifndef FORK_VER
   i += snprintf(&buf[i],maxbufsize-i,
#else
   i += sprintf(&buf[i],
#endif
"%s\nConnection Thread status:\n\
Load SWC module err : %lu\n\
Init SWC module err : %lu\n\
Receive error       : %lu\n\
Parse Header error  : %lu\n\
Send RespHdr error  : %lu\n\
RcvContentLenTooBig : %lu\n\
Invalid POST bodylen: %lu\n\
RcvContentLenIsBad  : %lu\n", 
                    szDelimitterLine,                   /* Delimitter line */
                    est.statConSwchLibLoadError,        /* Stat: Connection thread, total load_switcher_library_error counter */
                    est.statConSwchLibInitError,        /* Stat: Connection thread, total init_switcher_library_error counter */
                    est.statConTotalRcvError,           /* Stat: Connection thread, total receive_error counter */
                    est.statConTotalParseHeaderError,   /* Stat: Connection thread, total parse_http_header_error counter */
                    est.statConTotalSendRespHdrError,   /* Stat: Connection thread, total send_response_header_error counter */
                    est.statConTotalRcvContentLenTooBig,/* Stat: Connection thread, total rcv_content_length_too_big counter */
                    est.statConTotalInvalidPostBodyLen, /* Stat: Connection thread, total invalid_post_request_body_length counter */
                    est.statConTotalRcvContentLenIsBad);/* Stat: Connection thread, total rcv_content_length_is_bad counter */

  /* Add "Load IP Filter thread" information */
  if(i < maxbufsize)
#ifndef FORK_VER
   i += snprintf(&buf[i],maxbufsize-i,
#else
   i += sprintf(&buf[i],
#endif
"%s\nLoad IP Filter Thread status:\n\
CreErrHandle error  : %lu\n\
NewClient error     : %lu\n\
OpenConnection error: %lu\n\
IP SendRequest error: %lu\n\
KeySendRequest error: %lu\n\
GetNumRecords error : %lu\n\
GetNumColumn error  : %lu\n\
WhiteList overload  : %lu\n\
DarkList overload   : %lu\n\
ProcessReq error    : %lu\n\
Uentry ZeroRec error: %lu\n\
Load IP filter error: %lu\n\
Load UserEntry error: %lu\n",
                    szDelimitterLine,                   /* Delimitter line */
                    est.statLcfTotalCreateErrHError,    /* Stat: LoadIPFilterInfo thread, total create_error_handle_error counter */
                    est.statLcfTotalNewClientError,     /* Stat: LoadIPFilterInfo thread, total create_new_client_error counter */
                    est.statLcfTotalOpenConError,       /* Stat: LoadIPFilterInfo thread, total open_connection_error counter */
                    est.statLcfTotalSendReqError,       /* Stat: LoadIPFilterInfo thread, total send_request_error counter */
                    est.statLcfTotalSendKeyReqError,    /* Stat: LoadIPFilterInfo thread, total send_key_request_error counter */
                    est.statLcfTotalGetNumRecError,     /* Stat: LoadIPFilterInfo thread, total get_number_of_records_error counter */
                    est.statLcfTotalGetNumColError,     /* Stat: LoadIPFilterInfo thread, total get_number_of_columns_error counter */
                    est.statLcfTotalWhiteListOverload,  /* Stat: LoadIPFilterInfo thread, total white_list_overload counter */
                    est.statLcfTotalDarkListOverload,   /* Stat: LoadIPFilterInfo thread, total dark_list_overload counter */
                    est.statLcfTotalProcessReqError,    /* Stat: LoadIPFilterInfo thread, total process_request_error counter */
                    est.statLcfTotalUnZeroNumRecord,    /* Stat: LoadIPFilterInfo thread, total userentry_zero_records_error counter */
                    est.statLcfTotalLoadIpError,        /* Stat: LoadIPFilterInfo thread, total load_ip_filter_error counter */
                    est.statLcfTotalLoadUentryError);   /* Stat: LoadIPFilterInfo thread, total load_userentry_error counter */

  /* Add "Listen thread" information */
  if(i < maxbufsize)
#ifndef FORK_VER
   i += snprintf(&buf[i],maxbufsize-i,
#else
   i += sprintf(&buf[i],
#endif
"%s\nListen Thread status:\n\
Open Socket         : %lu\n\
Open Socket success : %lu\n\
Open Socket error   : %lu\n\
SSO Trusted error   : %lu\n\
SSO ReuseAddr error : %lu\n\
SSO KeepAlive error : %lu\n\
SSO DisNagle error  : %lu\n\
SSO EnbLinger error : %lu\n\
SSO AccKeepAliveErr : %lu\n\
SSO AccDisNagleErr  : %lu\n\
SSO AccEnbLingErr   : %lu\n\
SSO Bind socket     : %lu\n\
SSO Bind error      : %lu\n\
SSO Bind success    : %lu\n\
Listen success      : %lu\n\
Listen error        : %lu\n\
Accept success      : %lu\n\
Accept error        : %lu\n\
ConnThreadCreated   : %lu\n\
CreThrePerm error   : %lu\n\
CreThreNonPerm error: %lu\n", 
                    szDelimitterLine,                   /* Delimitter line */
                    est.statLsTotalOpenSocket,          /* Stat: Listen thread, total open socket counter */
                    est.statLsTotalOpenSocketSuccess,   /* Stat: Listen thread, total open socket success counter */
                    est.statLsTotalOpenSocketError,     /* Stat: Listen thread, total open socket error counter */
                    est.statLsTotalSSOTrustedError,     /* Stat: Listen thread, total tsix_on error counter */
                    est.statLsTotalSSOReuseAddrError,   /* Stat: Listen thread, total setsockopt_reuse_addr error counter */
                    est.statLsTotalSSOKeepAliveError,   /* Stat: Listen thread, total setsockopt_keep_alive error counter */
                    est.statLsTotalSSODisNagleError,    /* Stat: Listen thread, total setsockopt_disable_Nagle error counter */
                    est.statLsTotalSSOEnbLingerError,   /* Stat: Listen thread, total setsockopt_enable_linger error counter */
                    est.statLsTotalSSOAccKeepAliveError,/* Stat: Listen thread, total accept_setsockopt_keep_alive error counter */
                    est.statLsTotalSSOAccDisNagleError, /* Stat: Listen thread, total accept_setsockopt_disable_Nagle error counter */
                    est.statLsTotalSSOAccEnbLingerError,/* Stat: Listen thread, total accept_setsockopt_enable_linger error counter */
                    est.statLsTotalBindSocket,          /* Stat: Listen thread, total bind_listen_socket counter */
                    est.statLsTotalBindSocketError,     /* Stat: Listen thread, total bind_listen_socket_error counter */
                    est.statLsTotalBindSocketSuccess,   /* Stat: Listen thread, total bind_listen_socket_success counter */
                    est.statLsTotalListenSuccess,       /* Stat: Listen thread, total listen_socket_success counter */
                    est.statLsTotalListenError,         /* Stat: Listen thread, total listen_socket_error counter */
                    est.statLsTotalAcceptSuccess,       /* Stat: Listen thread, total accept_socket_success counter */
                    est.statLsTotalAcceptError,         /* Stat: Listen thread, total accept_socket_error counter */
                    est.statLsTotalThreadCreated,       /* Stat: Listen thread, total thread_created_success counter */
                    est.statLsTotalCreThreadPermErr,    /* Stat: Listen thread, total create_thread_permanent_error counter */
                    est.statLsTotalCreThreadNotPermErr);/* Stat: Listen thread, total create_thread_non_permanent_error counter */

  /* Add "Info thread" information */
  if(i < maxbufsize)
#ifndef FORK_VER
   i += snprintf(&buf[i],maxbufsize-i,
#else
   i += sprintf(&buf[i],
#endif
"%s\nInfo Thread status:\n\
RcvBuffer size      : %d\n\
Open socket         : %lu\n\
Open socket success : %lu\n\
Open socket error   : %lu\n\
Bind socket         : %lu\n\
Bind socket success : %lu\n\
Bind socket error   : %lu\n\
SetSockOpt error    : %lu\n\
Read socket         : %lu\n\
Read socket success : %lu\n\
Read socket error   : %lu\n\
Exit requests       : %lu\n\
Info requests       : %lu\n\
SendInfo success    : %lu\n\
SendInfo error      : %lu\n\
Test requests       : %lu\n\
ReloadInfo requests : %lu\n\
Unknown requests    : %lu\n",
                    szDelimitterLine,                   /* Delemitter line */
                    RCVINFOREQ_MAXSIZE,                 /* Rcv buffer size */
                    est.statInTotalOpenSocket,          /* Stat: Info thread, total open socket counter */
                    est.statInTotalOpenSocketSuccess,   /* Stat: Info thread, total open socket success counter */
                    est.statInTotalOpenSocketError,     /* Stat: Info thread, total open socket error counter */
                    est.statInTotalBindSocket,          /* Stat: Info thread, total bind socket counter */
                    est.statInTotalBindSocketSuccess,   /* Stat: Info thread, total bind socket success counter */
                    est.statInTotalBindSocketError,     /* Stat: Info thread, total bind socket error counter */
                    est.statInTotalSetOptError,         /* Stat: Info thread, total setsockopt error counter */
                    est.statInTotalReadSocket,          /* Stat: Info thread, total read socket counter */
                    est.statInTotalReadSocketSuccess,   /* Stat: Info thread, total read socket success counter */
                    est.statInTotalReadSocketError,     /* Stat: Info thread, total read socket error counter */
                    est.statInTotalExitRequest,         /* Stat: Info thread, total "Exit" request counter */
                    est.statInTotalInfoRequest,         /* Stat: Info thread, total "GetInfo" reqiest counter */
                    est.statInTotalSendInfoSuccess,     /* Stat: Info thread, total send info success counter */
                    est.statInTotalSendInfoError,       /* Stat: Info thread, total send info error counter */
                    est.statInTotalTestRequest,         /* Stat: Info thread, total test request counter */
                    est.statInTotalReloadRequest,       /* Stat: Info thread, total reload request counter */
                    est.statInTotalUnknownRequest);     /* Stat: Info thread, total unknown request counter */

  return strlen(buf);
}
#ifndef FORK_VER
/* ====================== thread_func_info ================================= */
void *thread_func_info(void *thread_param)
{ char buf[RCVINFOREQ_MAXSIZE+1];
  int s,i,l,slen,buflen;
  struct sockaddr_un saddr;
  int opensock_timeout = TOUTBASE_IFOPENSOCK;

  pthread_mutex_lock(&start_info_mutex.mutex); pthread_mutex_unlock(&start_info_mutex.mutex);

  for(s = INVALID_SOCKET_VAL;!exit_phase;)
  { if(s < 0)
    { (void)unlink(szSSServerInfoSockPath); est.statInTotalOpenSocket++;
      if((s = socket(AF_UNIX,SOCK_DGRAM,0)) >= 0)
      { est.statInTotalOpenSocketSuccess++; est.statInTotalBindSocket++;
        if(bind(s,(struct sockaddr*)&saddr,make_sockaddr(&saddr,szSSServerInfoSockPath)) < 0)
        { close_unlink(&s,szSSServerInfoSockPath);
          est.statInTotalBindSocketError++;
        }
        else
        { buflen = sizeof(slen); slen = 0;
          if(getsockopt(s,SOL_SOCKET,SO_SNDBUF,&slen,&buflen) < 0) slen = 0;
          if(slen < RCVINFOREQ_MAXSIZE)
          { slen = RCVINFOREQ_MAXSIZE;
            if(setsockopt(s,SOL_SOCKET,SO_SNDBUF,&slen,sizeof(slen)) < 0) est.statInTotalSetOptError++;
          }
          est.statInTotalBindSocketSuccess++;
          opensock_timeout = TOUTBASE_IFOPENSOCK;
        }
      }
      else est.statInTotalOpenSocketError++;
      if(s < 0)
      { sleep(opensock_timeout); opensock_timeout += TOUTPLUS_IFOPENSOCK;
        if(opensock_timeout > TOUTMAX_IFOPENSOCK) opensock_timeout = TOUTMAX_IFOPENSOCK;
      }
    }
    else
    { slen = sizeof(saddr); est.statInTotalReadSocket++;
      if((l = recvfrom(s,buf,RCVINFOREQ_MAXSIZE,0,(struct sockaddr*)&saddr,&slen)) >= 4 && !exit_phase)
      { buf[l] = 0; est.statInTotalReadSocketSuccess++;
        if(buf[0] == 'L' && buf[1] == 'V' && buf[2] == 'A')
        { switch(buf[3]) {
          case SSRVCMD_STOP   : est.statInTotalExitRequest++;
                                pthread_mutex_lock(&exit_phase_mutex.mutex); exit_phase++; pthread_cond_signal(&exit_condition.cond);
                                pthread_mutex_unlock(&exit_phase_mutex.mutex);
                                break;
          case SSRVCMD_STAT   : est.statInTotalInfoRequest++;
                                if(slen && (buflen = ssrvMakeReportBuffer(&buf[4],sizeof(buf)-4)) > 0)
                                { buflen += 5; /* header + 0 */
                                  if((i = socket(AF_UNIX,SOCK_DGRAM,0)) >= 0)
                                  { if((l = sendto(i,buf,buflen,0,(struct sockaddr*)&saddr,slen)) != buflen) est.statInTotalSendInfoError++;
                                    else est.statInTotalSendInfoSuccess++;
                                    ssrvCloseSocket(i);
                                  }
                                }
                                break;                             
          case SSRVCMD_TEST   : est.statInTotalTestRequest++;
                                break;
          case SSRVCMD_RELOAD : est.statInTotalReloadRequest++;
                                ssrvDeclareReloadIPFilterInfo();
                                break;
          default             : est.statInTotalUnknownRequest++;
          }; /* end of switch(buf[3]) */
        }
        else est.statInTotalUnknownRequest++;
      }
      if(l < 0) { close_unlink(&s,szSSServerInfoSockPath); est.statInTotalReadSocketError++; }
    }
  }
  close_unlink(&s,szSSServerInfoSockPath);
#ifdef INCDEBUG
  dprintf("*** Info thread exit ***\n");
#endif
  return 0;
}
#endif /* #ifndef FORK_VER */

#ifdef SSRV_SSDB_INCLUDE
/* -------------------------- xsDecodeBuf ------------------------------------ */
static int xsDecodeBuf(const char *frombuf,char *out,int sizeto,unsigned long key)
{ char *in,c;
  int size,i,j;

  if(((in  = (char*)frombuf) == 0) || (strlen(frombuf) < 4) || !out) return 0;
  for(i = 0;i < 4;i++) if(!isxdigit(in[i])) return 0;
  c = in[4]; in[4] = 0; i = sscanf(in,"%x",&size); in[4] = c; in += 4;
  if(i != 1 || size > sizeto) return 0;
  for(i = 0;i < (size*2);i++) if(!isxdigit(in[i])) return 0;

  key ^= (0x65193EA5+(size*size));

  if(size & 1)
  { for(i = 0;i < size;i++)
    { c = in[2]; in[2] = 0; sscanf(in,"%x",&j); in[2] = c; in += 2;
      out[i] = (char)((unsigned char)(j&0xff) ^ ((unsigned char)((key >> ((i&0xf)+(i&0x7)+(i&0x3)))+(size-i))));
    }
  }
  else
  { for(i = 0;i < size;i++)
    { c = in[2]; in[2] = 0; sscanf(in,"%x",&j); in[2] = c; in += 2;
      out[i] = (char)((unsigned char)(j&0xff) ^ ((unsigned char)((key >> ((i&0xf)+(i&0x7)+(i&0x2)))+(size+i))));
    }
  }
  out[i] = 0;
  return size;
}
/* ====================== thread_func_confloader =========================== */
void *thread_func_confloader(void *thread_param)
{ struct timespec time_val;
  char ip_string[1024];
  ssdb_Error_Handle hError;
  ssdb_Client_Handle hClient;
  ssdb_Connection_Handle hConnect;
  ssdb_Request_Handle hRequest;
  IPADDRE *ip;
  time_t fixocerr_time,fixiperr_time,fixunerr_time,temp_time;
  int timeout, err, rows_cnt, i, ip_enable, loadflt_timeout;
  int fixocerr_msg, fixloaderr_msg, fixunloaderr_msg;

  fixocerr_msg = (fixloaderr_msg = (fixunloaderr_msg = 0));
  fixocerr_time = (fixiperr_time = (fixunerr_time = time(NULL)));
 
  pthread_mutex_lock(&start_confl_mutex.mutex); pthread_mutex_unlock(&start_confl_mutex.mutex);

  for(loadflt_timeout = TOUTBASE_LCFLOADIPFLT;!exit_phase;)
  { timeout = 0;
    err = 1;
    if((hError = ssdbCreateErrorHandle()) != 0)
    { if((hClient = ssdbNewClient(hError,SSRV_SSDBCLIENT_NAME,SSRV_SSDBCLIENT_PASSWORD,LOCAL_SSDB_NEWCLIENTFLAGS)) != 0)
      { if((hConnect = ssdbOpenConnection(hError,hClient,NULL,lpszDatabaseName,LOCAL_SSDB_NEWCONNECTFLAGS)) != 0)
        { /* Step 1: Load IP filter info */
	  if((hRequest = ssdbSendRequestTrans(hError,hConnect,SSDB_REQTYPE_SELECT,lpszDbAddrTableName,"select ipaddrstr,enabled from %s",lpszDbAddrTableName)) != 0)
	  { rows_cnt = ssdbGetNumRecords(hError,hRequest);
            if(ssdbGetLastErrorCode(hError) == SSDBERR_SUCCESS)
	    { ip_enable = ssdbGetNumColumns(hError,hRequest);
              if((ssdbGetLastErrorCode(hError) == SSDBERR_SUCCESS) && (ip_enable == 2))
	      { pthread_mutex_lock(&ipaddrlist_mutex.mutex); /* begin of critical section */
                clearAllIpAddrLists();
                for(err = (i = 0);i < rows_cnt && !exit_phase && !err;i++)
                { if(!ssdbGetNextField(hError,hRequest,ip_string,sizeof(ip_string)-1) || !ssdbGetNextField(hError,hRequest,&ip_enable,sizeof(ip_enable)))
	          { est.statLcfTotalProcessReqError++; err++;
		    break;
		  }
                  if((ip = newIpAddrEntry((const char *)ip_string)) != 0)
                  { if(ip_enable) { if(!pushIpAddrIntoWhiteList(ip)) { deleteIpAddrEntry(ip); est.statLcfTotalWhiteListOverload++; } }
                    else          { if(!pushIpAddrIntoDarkList(ip))  { deleteIpAddrEntry(ip); est.statLcfTotalDarkListOverload++; } }
		  }
                } 
                pthread_mutex_unlock(&ipaddrlist_mutex.mutex); /* end of critical section */
	      }
              else est.statLcfTotalGetNumColError++;
	    }
            else est.statLcfTotalGetNumRecError++;
	    ssdbFreeRequest(hError,hRequest);
	  } /* End of Load IP filter info */
 	  else est.statLcfTotalSendReqError++;
	  
          if(err) /* Check error flag and fix it */
          { est.statLcfTotalLoadIpError++;
	    if(!fixloaderr_msg)
	    { fixloaderr_msg = syslog_msg(LOG_ERR,SRVSEQN_CANTLOADIPFLT,szCantLoadIpFilterInfo,lpszDatabaseName,lpszDbAddrTableName,est.statLcfTotalLoadIpError);
	      fixiperr_time = time(NULL);
	    }
	  }

	  if(!err && !exit_phase)
          { err++;
	    /* Step 2: Load Username and Password from "key_table" table */
	    if((hRequest = ssdbSendRequestTrans(hError,hConnect,SSDB_REQTYPE_SELECT,lpszDbKeyTableName,"select value,subvalue from %s where keyname=\"userentry\"",lpszDbKeyTableName)) != 0)
	    { rows_cnt = ssdbGetNumRecords(hError,hRequest);
              if(ssdbGetLastErrorCode(hError) == SSDBERR_SUCCESS)
	      { if(rows_cnt)
	        { for(i = (err = 0);i < rows_cnt && !exit_phase && !err;i++)
                  { if(!ssdbGetNextField(hError,hRequest,ip_string,sizeof(ip_string)-1)) { err++; break; }
                    xsDecodeBuf(ip_string,szAccessUserName,sizeof(szAccessUserName)-1,SSRV_UNAME_KEY_VALUE);
		    szAccessUserName[sizeof(szAccessUserName)-1] = 0;
		    /* dprintf("user name=\"%s\"\n",szAccessUserName);  FIXME !!!! */

                    if(!ssdbGetNextField(hError,hRequest,ip_string,sizeof(ip_string)-1)) { err++; break; }
                    xsDecodeBuf(ip_string,szAccessPassword,sizeof(szAccessPassword)-1,SSRV_PASSWD_KEY_VALUE);
		    szAccessPassword[sizeof(szAccessPassword)-1] = 0;
		    /* dprintf("password=\"%s\"\n",szAccessPassword); FIXME !!!! */		    
		  }
		}
		else est.statLcfTotalUnZeroNumRecord++;
              }
	      else est.statLcfTotalGetNumRecError++;
              ssdbFreeRequest(hError,hRequest);	  
	    }
	    else est.statLcfTotalSendKeyReqError++;

            if(err) /* Check error flag and fix it */
            { est.statLcfTotalLoadUentryError++;
	      if(!fixunloaderr_msg)
	      { fixunloaderr_msg = syslog_msg(LOG_ERR,SRVSEQN_CANTLOADUENTRY,szCantLoadUserEntryInfo,lpszDatabaseName,lpszDbKeyTableName,est.statLcfTotalLoadUentryError);
                fixunerr_time = time(NULL);
	      }
	    }
	  }
	  
	  if(!err && !exit_phase)
	  { pthread_mutex_lock(&password_mutex.mutex);
	    unameInitedFlag++; pthread_cond_signal(&password_condition.cond);
	    pthread_mutex_unlock(&password_mutex.mutex);
	  }
	  ssdbCloseConnection(hError,hConnect);
        }
        else
	{ est.statLcfTotalOpenConError++; 
	  if(!fixocerr_msg)
          { fixocerr_msg = syslog_msg(LOG_ERR,SRVSEQN_CANTLCONNECTTODB,szCantConnectToDb,lpszDatabaseName,est.statLcfTotalOpenConError);
	    fixocerr_time = time(NULL);
          }
	}
        ssdbDeleteClient(hError,hClient);
      }
      else est.statLcfTotalNewClientError++;
      ssdbDeleteErrorHandle(hError);    
    }
    else est.statLcfTotalCreateErrHError++;

    if(err)
    { timeout = loadflt_timeout;
      loadflt_timeout += TOUTPLUS_LCFLOADIPFLT;
      if(loadflt_timeout > TOUTMAX_LCFLOADIPFLT) loadflt_timeout = TOUTMAX_LCFLOADIPFLT;
      if((temp_time = time(NULL)) != (time_t)(-1))
      { if(fixocerr_msg && (difftime(temp_time,fixocerr_time) >= (double)TOUTMAX_LCDROPFIXFLG)) fixocerr_msg = 0;
        if(fixloaderr_msg && (difftime(temp_time,fixiperr_time) >= (double)TOUTMAX_LCDROPFIXFLG)) fixloaderr_msg = 0;
        if(fixunloaderr_msg && (difftime(temp_time,fixunerr_time) >= (double)TOUTMAX_LCDROPFIXFLG)) fixunloaderr_msg = 0;
      }
    }
    else
    { loadflt_timeout = TOUTBASE_LCFLOADIPFLT;
      fixloaderr_msg = (fixunloaderr_msg = 0);
    }
    
    pthread_mutex_lock(&reinit_mutex.mutex);  need_init = 0;
    if(timeout)
    { memset(&time_val,0,sizeof(time_val));
      time_val.tv_sec = (time_t)(timeout)+time(NULL);
      pthread_cond_timedwait(&reinit_condition.cond,&reinit_mutex.mutex,&time_val);
    }
    else
    { pthread_cond_wait(&reinit_condition.cond,&reinit_mutex.mutex);
    }
    pthread_mutex_unlock(&reinit_mutex.mutex);
  } /* end of for(;!exit_phase;) */

#ifdef INCDEBUG
  dprintf("*** ConfLoader thread exit ***\n");
#endif
  return 0;
}
#else
#ifndef FORK_VER
/* ====================== thread_func_confloader =========================== */
void *thread_func_confloader(void *thread_param)
{ pthread_mutex_lock(&start_confl_mutex.mutex); pthread_mutex_unlock(&start_confl_mutex.mutex);
  for(;!exit_phase;)
  { pthread_mutex_lock(&password_mutex.mutex);
    unameInitedFlag++; pthread_cond_signal(&password_condition.cond);
    pthread_mutex_unlock(&password_mutex.mutex);
    
    pthread_mutex_lock(&reinit_mutex.mutex);  need_init = 0;
    pthread_cond_wait(&reinit_condition.cond,&reinit_mutex.mutex);
    pthread_mutex_unlock(&reinit_mutex.mutex);
  }
  return 0;
}
#endif
#endif /* SSRV_SSDB_INCLUDE */
#ifndef FORK_VER
/* ====================== thread_func_unloader ============================= */
void *thread_func_unloader(void *thread_param)
{ int i;
  pthread_mutex_lock(&start_unloader_mutex.mutex); pthread_mutex_unlock(&start_unloader_mutex.mutex);
  for(i = (alive_timeout = inetd_mode);!exit_phase && i > 0;)
  { sleep(SSRV_UNLOAD_SLEEP_STEP);
    pthread_mutex_lock(&start_unloader_mutex.mutex);
    if(!disable_unload_step) { alive_timeout -= SSRV_UNLOAD_SLEEP_STEP; i = alive_timeout; }
    pthread_mutex_unlock(&start_unloader_mutex.mutex);
  }
  activate_exit_phase();
#ifdef INCDEBUG
  dprintf("*** Unload thread exit ***\n");
#endif
  return 0;
}
/* ======================== thread_func_killer ============================= */
void *thread_func_killer(void *thread_param)
{ pthread_mutex_lock(&start_killer_mutex.mutex); pthread_mutex_unlock(&start_killer_mutex.mutex);
  pthread_kill(listen_tid,SIGTERM);
  pthread_kill(info_tid,SIGTERM);
  pthread_kill(confl_tid,SIGTERM);
  if(inetd_mode) pthread_kill(unloader_tid,SIGTERM);
  sleep(1);
  pthread_kill(listen_tid,SIGKILL);
  pthread_kill(info_tid,SIGKILL);
  pthread_kill(confl_tid,SIGKILL);
  if(inetd_mode) pthread_kill(unloader_tid,SIGKILL);
#ifdef INCDEBUG
  dprintf("*** Killer thread exit ***\n");
#endif
  return 0;
}
#endif
/* ------------------------ daemon_killer ---------------------------------- */
void daemon_killer(int sign)
{ activate_exit_phase();
#ifdef INCDEBUG
  dprintf("%s - Died signal (%d) received\n",szDaemonName,sign);
#endif
  signal(sign,daemon_killer);
}
/* ------------------------ reinit_handler --------------------------------- */
void reinit_handler(int sign)
{ 
#ifndef FORK_VER
  ssrvDeclareReloadIPFilterInfo();
#endif
#ifdef INCDEBUG
  dprintf("SIGHUP - %d, reinit flag = %d\n",sign,(int)need_init);
#endif
  signal(sign,reinit_handler);
}
/* ---------------------- isr_illegal_instr -------------------------------- */
void isr_illegal_instr(int sign)
{ syslog_msg(LOG_ERR,SRVSEQN_ILLEGALCPUCMD,szIllegalCPUcommand);
#ifdef INCDEBUG
  dprintf(szIllegalCPUcommand);
#endif
  exit(SSERR_INVCPUCMD);
}
/* ----------------------- isr_fpe_error ----------------------------------- */
void isr_fpe_error(int sign)
{ syslog_msg(LOG_ERR,SRVSEQN_ILLEGALFPECMD,szIllegalFPEcommand);
#ifdef INCDEBUG
  dprintf(szIllegalFPEcommand);
#endif
  exit(SSERR_INVFPECMD);
}
/* ------------------------- daemon_init ----------------------------------- */
static void daemon_init(void)
{ pid_t pid;
  struct rlimit flim;
  int maxfiles,fd;

  /* Set new signal handlers */
  signal(SIGHUP,reinit_handler);                        /* Reinit different info */
  signal(SIGTERM,daemon_killer);                        /* Process terminated */
  signal(SIGILL,isr_illegal_instr);                     /* Illegal CPU command */
  signal(SIGFPE,isr_fpe_error);                         /* Illegal FPE command */
  signal(SIGTTIN,SIG_IGN);                              /* Ignore: Attempt to read terminal from background process */
  signal(SIGTTOU,SIG_IGN);                              /* Ignore: Attempt to write terminal from background process */
  signal(SIGTSTP,SIG_IGN);                              /* Ignore: Stop key signal from controlling terminal */
  signal(SIGPIPE,SIG_IGN);                              /* Ignore: Broken PIPE */
#ifdef SIGXCPU
  signal(SIGXCPU,SIG_IGN);                              /* Ignore: CPU usage overload */
#endif
#ifdef SIGXFSZ
  signal(SIGXFSZ,SIG_IGN);                              /* Ignore: File size limit */
#endif
  signal(SIGINT,debug_mode ? daemon_killer : SIG_IGN);  /* Interrupt key signal from controlling terminal */
  signal(SIGQUIT,debug_mode ? daemon_killer : SIG_IGN); /* Quit key signal from controlling terminal */
  signal(SIGCHLD,SIG_IGN);                              /* Ignore: Child process status change */
  (void)umask(S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP);

  if(!debug_mode && !inetd_mode)
  { if((pid = fork()) == (pid_t)(-1))
    { if(!silenceMode) fprintf(stderr,szErrorSystemCantFork3,szDaemonName,SSERR_SYSERRFORK,errno);
      exit(SSERR_SYSERRFORK);
    }
    if(pid) exit(SSERR_SUCCESS);

    setsid(); /* Set new group id */

    /* Get the max file handle number */    
    if(!getrlimit(RLIMIT_NOFILE,&flim)) maxfiles = flim.rlim_max;
    else                                maxfiles = 20;
    (void)chdir("/");
    for(fd = 0;fd < maxfiles;fd++)  close(fd); /* Close all files */

#ifdef FORK_VER
    { char fname[512];
      int fd_lockfile;
      sprintf(fname,szInstLockFnameMask,szDaemonName);
      if((fd_lockfile = open(fname,O_WRONLY|O_CREAT,0600)) >= 0) flock(fd_lockfile,LOCK_EX|LOCK_NB);
    }
#endif

#ifdef TIOCNOTTY
    if((fd = open(_PATH_TTY,O_RDWR)) >= 0)
    { ioctl(fd,(int)TIOCNOTTY,0);
      (void)close(fd);
    }
#endif
  }
}
/* -------------------------- check_osenv ---------------------------------- */
static void check_osenv(void)
{
#ifdef RLIMIT_PTHREAD
  struct rlimit pthlim;
  /* Check phtread limitation */
  if(!getrlimit(RLIMIT_PTHREAD,&pthlim))
  { if(pthlim.rlim_max < 6)
    { if(!silenceMode) fprintf(stderr,szErrorIncorrectOSRes2,szDaemonName,SSERR_INVOSPARAMS);
      ssrvExit(SSERR_INVOSPARAMS);
    }
  }
#endif
  if(gethostname(szLocalHostName,sizeof(szLocalHostName)))
  { if(!silenceMode) fprintf(stderr,szErrorCantGetHostName3,szDaemonName,SSERR_INVOSPARAMS,errno);
    ssrvExit(SSERR_INVOSPARAMS);
  }
}

/* ---------------------- init_daemon_resources ---------------------------- */
#ifndef FORK_VER
static void init_daemon_resources(void)
{ /* init mutex resources */
  if(!init_dmutex(&start_listen_mutex))     { syslog_msg(LOG_ERR,SRVSEQN_INITMUTEXERR,szCantInitMutex,"start_listen"); ssrvExit(SSERR_INITDATA); }
  if(!init_dmutex(&start_killer_mutex))     { syslog_msg(LOG_ERR,SRVSEQN_INITMUTEXERR,szCantInitMutex,"start_killer"); ssrvExit(SSERR_INITDATA); }
  if(!init_dmutex(&start_info_mutex))       { syslog_msg(LOG_ERR,SRVSEQN_INITMUTEXERR,szCantInitMutex,"start_info"); ssrvExit(SSERR_INITDATA); }
  if(!init_dmutex(&start_confl_mutex))      { syslog_msg(LOG_ERR,SRVSEQN_INITMUTEXERR,szCantInitMutex,"start_confloader"); ssrvExit(SSERR_INITDATA); }
  if(!init_dmutex(&start_unloader_mutex))   { syslog_msg(LOG_ERR,SRVSEQN_INITMUTEXERR,szCantInitMutex,"start_unloader"); ssrvExit(SSERR_INITDATA); }
  if(!init_dmutex(&exit_phase_mutex))       { syslog_msg(LOG_ERR,SRVSEQN_INITMUTEXERR,szCantInitMutex,"exit"); ssrvExit(SSERR_INITDATA); }
  if(!init_dmutex(&reinit_mutex))           { syslog_msg(LOG_ERR,SRVSEQN_INITMUTEXERR,szCantInitMutex,"reinit"); ssrvExit(SSERR_INITDATA); }
  if(!init_dmutex(&ipaddrlist_mutex))       { syslog_msg(LOG_ERR,SRVSEQN_INITMUTEXERR,szCantInitMutex,"ipaddrlist"); ssrvExit(SSERR_INITDATA); }
  if(!init_dmutex(&tcblist_mutex))          { syslog_msg(LOG_ERR,SRVSEQN_INITMUTEXERR,szCantInitMutex,"tcblist"); ssrvExit(SSERR_INITDATA); }
  if(!init_dmutex(&switcher_mutex))         { syslog_msg(LOG_ERR,SRVSEQN_INITMUTEXERR,szCantInitMutex,"switcher"); ssrvExit(SSERR_INITDATA); }
  if(!init_dmutex(&password_mutex))         { syslog_msg(LOG_ERR,SRVSEQN_INITMUTEXERR,szCantInitMutex,"password"); ssrvExit(SSERR_INITDATA); }
  inited_mutex++;

  /* init condition variables */
  if(!init_dcond(&exit_condition))          { syslog_msg(LOG_ERR,SRVSEQN_INITCONDVARERR,szCantInitConVar,"exit"); ssrvExit(SSERR_INITDATA); }
  if(!init_dcond(&reinit_condition))        { syslog_msg(LOG_ERR,SRVSEQN_INITCONDVARERR,szCantInitConVar,"reinit"); ssrvExit(SSERR_INITDATA); }
  if(!init_dcond(&password_condition))      { syslog_msg(LOG_ERR,SRVSEQN_INITCONDVARERR,szCantInitConVar,"password"); ssrvExit(SSERR_INITDATA); }
  inited_cond++;
}
#else
static void init_daemon_resources(void) { /* FORK version */ }
#endif /* #ifndef FORK_VER */

/* ---------------------- done_daemon_resources ---------------------------- */
#ifndef FORK_VER
static void done_daemon_resources(void)
{ /* Destroy all mutexes */
  if(inited_mutex)
  { inited_mutex = 0;
    destroy_dmutex(&start_listen_mutex);
    destroy_dmutex(&start_killer_mutex);
    destroy_dmutex(&start_info_mutex);
    destroy_dmutex(&start_confl_mutex);
    destroy_dmutex(&start_unloader_mutex);
    destroy_dmutex(&exit_phase_mutex);
    destroy_dmutex(&reinit_mutex);
    destroy_dmutex(&ipaddrlist_mutex);
    destroy_dmutex(&tcblist_mutex);
    destroy_dmutex(&switcher_mutex);
    destroy_dmutex(&password_mutex);
  }
  /* Destroy all condition variables */
  if(inited_cond)
  { inited_cond = 0;
    destroy_dcond(&exit_condition);
    destroy_dcond(&reinit_condition);
    destroy_dcond(&password_condition);
  }
#ifdef SSRV_SSDB_INCLUDE
  /* Deinitialize database */
  ssdbDone();  
#endif
}
#else
static void done_daemon_resources(void) { /* FORK version */ }
#endif /* #ifndef FORK_VER */

/* ------------------------ init_daemon_threads ---------------------------- */
#ifndef FORK_VER
static void init_daemon_threads(void)
{ /* Init Listen thread */
  pthread_mutex_lock(&start_listen_mutex.mutex);
  if(pthread_create(&listen_tid,NULL,thread_func_listen,NULL))
  { exit_phase++; pthread_mutex_unlock(&start_listen_mutex.mutex);
    syslog_msg(LOG_ERR,SRVSEQN_INITTHREADERR,szCantInitThread,"listen"); ssrvExit(SSERR_INITTHREAD);
  }

  /* Init Info thread */
  pthread_mutex_lock(&start_info_mutex.mutex);
  if(pthread_create(&info_tid,NULL,thread_func_info,NULL))
  { exit_phase++; pthread_mutex_unlock(&start_info_mutex.mutex);
    syslog_msg(LOG_ERR,SRVSEQN_INITTHREADERR,szCantInitThread,"info"); ssrvExit(SSERR_INITTHREAD);
  }

  /* Init Configuration loader thread */
  pthread_mutex_lock(&start_confl_mutex.mutex);
  if(pthread_create(&confl_tid,NULL,thread_func_confloader,NULL))
  { exit_phase++; pthread_mutex_unlock(&start_confl_mutex.mutex);
    syslog_msg(LOG_ERR,SRVSEQN_INITTHREADERR,szCantInitThread,"confloader"); ssrvExit(SSERR_INITTHREAD);
  }

  if(inetd_mode)
  { /* Init Unloader thread */
    pthread_mutex_lock(&start_unloader_mutex.mutex);
    if(pthread_create(&unloader_tid,NULL,thread_func_unloader,NULL))
    { exit_phase++; pthread_mutex_unlock(&start_unloader_mutex.mutex);
      syslog_msg(LOG_ERR,SRVSEQN_INITTHREADERR,szCantInitThread,"unloader"); ssrvExit(SSERR_INITTHREAD);
    }
  }

  /* Init Killer thread */
  pthread_mutex_lock(&start_killer_mutex.mutex);
  if(pthread_create(&killer_tid,NULL,thread_func_killer,NULL))
  { exit_phase++; pthread_mutex_unlock(&start_killer_mutex.mutex);
    syslog_msg(LOG_ERR,SRVSEQN_INITTHREADERR,szCantInitThread,"killer"); ssrvExit(SSERR_INITTHREAD);
  }
}
#else
static void init_daemon_threads(void) { /* FORK version */ }
#endif

/* -------------------------- send_test_request ---------------------------- */
#ifndef FORK_VER
static void send_test_request(void)
{ struct sockaddr_un su_addr;
  char buf[16];
  int s;

  if((s = socket(AF_UNIX, SOCK_DGRAM, 0)) >= 0)
  { prepare_cmdbuffer(buf,SSRVCMD_TEST); /* Test message packet */
    sendto(s,buf,4,0,(struct sockaddr*)&su_addr,make_sockaddr(&su_addr,szSSServerInfoSockPath));
    ssrvCloseSocket(s);
  }
}
#endif /* #ifndef FORK_VER */

/* ----------------------------- process ----------------------------------- */
#ifndef FORK_VER
static void process(void)
{ pthread_mutex_unlock(&start_confl_mutex.mutex);                   /* Start ConfigLoader thread */
  pthread_mutex_unlock(&start_info_mutex.mutex);                    /* Start Info thread */
  pthread_mutex_unlock(&start_listen_mutex.mutex);                  /* Start Listen thread */
  if(inetd_mode) pthread_mutex_unlock(&start_unloader_mutex.mutex); /* Start Unloader thread */
  else syslog_msg(LOG_INFO,SRVSEQN_STARTED,szStarted);

  /* Start exit_phase flag waiting */
  pthread_mutex_lock(&exit_phase_mutex.mutex);
  while(!exit_phase) pthread_cond_wait(&exit_condition.cond,&exit_phase_mutex.mutex);
  pthread_mutex_unlock(&exit_phase_mutex.mutex);

  /* reanimate reinit thread (send signal - "need reinit" */
  pthread_mutex_lock(&reinit_mutex.mutex);
  pthread_cond_signal(&reinit_condition.cond);
  pthread_mutex_unlock(&reinit_mutex.mutex);

  /* reanimate (possible) listen thread */
  pthread_mutex_lock(&password_mutex.mutex);
  pthread_cond_signal(&password_condition.cond);
  pthread_mutex_unlock(&password_mutex.mutex);

  /* reanimate info thread (send test request message) */
  send_test_request();

  pthread_mutex_unlock(&start_killer_mutex.mutex); /* Start killer thread */
  pthread_join(listen_tid,NULL);   /* wait exit for rcvsocket thread */
  pthread_join(info_tid,NULL);     /* wait exit for info thread */
  pthread_join(confl_tid,NULL);    /* wait exit for configloader thread */
  if(inetd_mode) pthread_join(unloader_tid,NULL); /* wait exit for unloader thread (only for inetd_mode) */
  pthread_join(killer_tid,NULL);   /* wait exit for killer thread */
}
#else
static void process(void)
{ syslog_msg(LOG_INFO,SRVSEQN_STARTED,szStarted);
  thread_func_listen(0);
}
#endif

/* ----------------------------- init_db ----------------------------------- */
static void init_db(void)
{
#ifdef SSRV_SSDB_INCLUDE
  int errcode;
  char *errmsg;
  if(!ssdbInit())
  { errcode = ssdbGetLastErrorCode(NULL);
    errmsg = (char*)ssdbGetLastErrorString(NULL);
    if(!inetd_mode) syslog_msg(LOG_ERR,SRVSEQN_CANTINITDB,szCantInitSSDBAPI,errmsg ? errmsg : szUnknown,errcode);
    if(!silenceMode) fprintf(stderr,szErrorCantInitSSDBAPI4,szDaemonName,SSERR_CANTINITDB,errcode,errmsg ? errmsg : szUnknown);
    ssrvExit(SSERR_CANTINITDB);
  }
#endif
}
/* ---------------------- init_default_ipfilter ---------------------------- */
static void init_default_ipfilter(void)
{ IPADDRE *ip;
#ifndef FORK_VER
  pthread_mutex_lock(&ipaddrlist_mutex.mutex); /* begin critical section */
#endif

#ifdef SSRV_OPEN_ALLIPADDR
  if((ip = newIpAddrEntry("*.*.*.*")) != 0) /* Enable all */
  { ip->next = whiteIpAddrList; whiteIpAddrList = ip;
  }
#else
  if((ip = newIpAddrEntry("*.*.*.*")) != 0) /* Disable all */
  { ip->next = darkIpAddrList; darkIpAddrList = ip;
  }
#endif
  if((ip = newIpAddrEntry("127.0.0.1")) != 0) /* Enable localhost (UNIX) */
  { ip->next = whiteIpAddrList; whiteIpAddrList = ip;
  }
  if((ip = newIpAddrEntry("127.0.0.0")) != 0) /* Enable localhost (NT) */
  { ip->next = whiteIpAddrList; whiteIpAddrList = ip;
  }
#ifndef FORK_VER
  pthread_mutex_unlock(&ipaddrlist_mutex.mutex); /* end of critical section */
#endif
}
#ifndef FORK_VER
/* ------------------------- instance_check -------------------------------- */
static void instance_check(void)
{ char buf[16];
  int su_addrlen,s,l;
  struct sockaddr_un su_addr;

  su_addrlen = make_sockaddr(&su_addr,szSSServerInfoSockPath);

  if((s = socket(AF_UNIX, SOCK_DGRAM, 0)) < 0)
  { if(!silenceMode) fprintf(stderr,szErrorCantOpenSocket3,szDaemonName,SSERR_CANTOPENSOCK,errno);
    ssrvExit(SSERR_CANTOPENSOCK);
  }
  prepare_cmdbuffer(buf,SSRVCMD_TEST); /* Test message packet */

  l = sendto(s,buf,4,0,(struct sockaddr*)&su_addr,su_addrlen); ssrvCloseSocket(s);
  if(l == 4)
  { if(!silenceMode) fprintf(stderr,szErrorAlreadyInstalled2,szDaemonName,SSERR_ALRINSTALLED);
    ssrvExit(SSERR_ALRINSTALLED);
  }

  unlink_name(szSSServerInfoSockPath);

  su_addrlen = make_sockaddr(&su_addr,szSSServerInfoSockPath);

  if((s = socket(AF_UNIX, SOCK_DGRAM, 0)) < 0)
  { if(!silenceMode) fprintf(stderr,szErrorCantOpenSocket3,szDaemonName,SSERR_CANTOPENSOCK,errno);
    ssrvExit(SSERR_CANTOPENSOCK);
  }

  l = bind(s,(struct sockaddr*)&su_addr,su_addrlen);
  if(l < 0)
  { if(!silenceMode) fprintf(stderr,szErrorCantBindSocket3,szDaemonName,SSERR_CANTBINDSOCK,errno);
    close_unlink(&s,szSSServerInfoSockPath);
    ssrvExit(SSERR_CANTBINDSOCK);
  }

  l = sizeof(su_addrlen); su_addrlen = 0;
  if(getsockopt(s,SOL_SOCKET,SO_SNDBUF,&su_addrlen,&l) < 0) su_addrlen = 0;
  if(su_addrlen < RCVINFOREQ_MAXSIZE)
  { su_addrlen = RCVINFOREQ_MAXSIZE;
    if(setsockopt(s,SOL_SOCKET,SO_SNDBUF,&su_addrlen,sizeof(su_addrlen)) < 0)
    { if(!silenceMode) fprintf(stderr,szErrorCantSetSockOpt3,szDaemonName,SSERR_CANTSSOCKOPT,errno);
      close_unlink(&s,szSSServerInfoSockPath);
      ssrvExit(SSERR_CANTSSOCKOPT);
    }
  }
  close_unlink(&s,szSSServerInfoSockPath);
}
#else
static void instance_check(void)
{ char fname[512];
  int fd_lockfile;

  /* Make lock file name */
  sprintf(fname,szInstLockFnameMask,szDaemonName);

  if((fd_lockfile = open(fname,O_WRONLY|O_CREAT,0600)) < 0)
  { if(!silenceMode) fprintf(stderr,szErrorCantOpenLockFile3,szDaemonName,fname,strerror(errno));
    ssrvExit(SSERR_INITDATA);
  }

  if(flock(fd_lockfile,LOCK_EX|LOCK_NB) == (-1))
  { if(errno == EWOULDBLOCK)
    { if(!silenceMode) fprintf(stderr,szErrorAlreadyInstalled2,szDaemonName,SSERR_ALRINSTALLED);
      ssrvExit(SSERR_ALRINSTALLED);
    }
    if(!silenceMode) fprintf(stderr,szErrorCantLockLockFile3,szDaemonName,fname,strerror(errno));
    ssrvExit(SSERR_INITDATA);
  }
  close(fd_lockfile);
}
#endif /* #ifndef FORK_VER */
/* -------------------- print_daemon_statistics ---------------------------- */
#ifndef FORK_VER
static void print_daemon_statistics(void)
{ char tmpsockname[512];
  char buf[RCVINFOREQ_MAXSIZE+1];
  int su_addrlen,cu_addrlen,s,l;
  struct sockaddr_un su_addr,cu_addr;
  struct timeval timeout;
  fd_set fdset;

  if(!sprintf(tmpsockname,szTempSockTemplate,(unsigned long)getpid()))
  { if(!silenceMode) fprintf(stderr,szErrorCantMakeTempFile2,szDaemonName,SSERR_CANTMAKETEMP);
    ssrvExit(SSERR_CANTMAKETEMP);
  }

  cu_addrlen = make_sockaddr(&cu_addr,tmpsockname);

  if((s = socket(AF_UNIX, SOCK_DGRAM, 0)) < 0)
  { if(!silenceMode) fprintf(stderr,szErrorCantOpenSocket3,szDaemonName,SSERR_CANTOPENSOCK,errno);
    ssrvExit(SSERR_CANTOPENSOCK);
  }

  if(bind(s,(struct sockaddr*)&cu_addr,cu_addrlen) < 0)
  { if(!silenceMode) fprintf(stderr,szErrorCantBindSocket3,szDaemonName,SSERR_CANTBINDSOCK,errno);
    close_unlink(&s,tmpsockname);
    ssrvExit(SSERR_CANTBINDSOCK);
  }

  l = sizeof(su_addrlen); su_addrlen = 0;
  if(getsockopt(s,SOL_SOCKET,SO_RCVBUF,&su_addrlen,&l) < 0) su_addrlen = 0;
  if(su_addrlen < RCVINFOREQ_MAXSIZE)
  { su_addrlen = RCVINFOREQ_MAXSIZE;
    if(setsockopt(s,SOL_SOCKET,SO_RCVBUF,&su_addrlen,sizeof(su_addrlen)) < 0)
    { if(!silenceMode) fprintf(stderr,szErrorCantSetSockOpt3,szDaemonName,SSERR_CANTSSOCKOPT,errno);
      close_unlink(&s,tmpsockname);
      ssrvExit(SSERR_CANTSSOCKOPT);
    }
  }

  prepare_cmdbuffer(buf,SSRVCMD_STAT); /* Stat command */
  su_addrlen = make_sockaddr(&su_addr,szSSServerInfoSockPath);

  if((l = sendto(s,buf,4,0,(struct sockaddr*)&su_addr,su_addrlen)) != 4)
  { if(!silenceMode) fprintf(stderr,szErrorDaemonNotInstalled3,szDaemonName,SSERR_NOTINSTALLED,errno);
    close_unlink(&s,tmpsockname);
    ssrvExit(SSERR_NOTINSTALLED);
  }

  FD_ZERO(&fdset); FD_SET(s,&fdset);
  timeout.tv_sec = 3; timeout.tv_usec = 0;

  for(;;)
  { l = select((int)s+1,&fdset,0,0,&timeout);
    if(l < 0 && errno == EINTR) continue;
    break;
  }

  if(l <= 0 || !FD_ISSET(s,&fdset))
  { if(!silenceMode) fprintf(stderr,szErrorDaemonNotInstalled3,szDaemonName,SSERR_NOTINSTALLED,errno);
    close_unlink(&s,tmpsockname);
    ssrvExit(SSERR_NOTINSTALLED);
  }

  memset(&su_addr,0,sizeof(su_addr)); su_addrlen = sizeof(su_addr);
  for(;;)
  { l = recvfrom(s,buf,RCVINFOREQ_MAXSIZE,0,(struct sockaddr*)&su_addr,&su_addrlen);
    if(l < 0 && errno == EINTR) continue;
    break;
  }
  close_unlink(&s,tmpsockname);
  if(l >= 4 && buf[0] == 'L' && buf[1] == 'V' && buf[2] == 'A' && buf[3] == SSRVCMD_STAT)
  { buf[l] = 0; puts(&buf[4]);
    ssrvExit(SSERR_SUCCESS);
  }
  if(!silenceMode) fprintf(stderr,szErrorDaemonNotInstalled3,szDaemonName,SSERR_NOTINSTALLED,errno);
  ssrvExit(SSERR_NOTINSTALLED);
}
#else
static void print_daemon_statistics(void)
{ char fname[512];
  int fd_lockfile;

  printf("Not supported in non multithread version.\n");

  /* Make lock file name */
  sprintf(fname,szInstLockFnameMask,szDaemonName);

  if((fd_lockfile = open(fname,O_WRONLY|O_CREAT,0600)) >= 0)
  { if(flock(fd_lockfile,LOCK_EX|LOCK_NB) == (-1))
    { if(errno == EWOULDBLOCK) ssrvExit(SSERR_SUCCESS);
      ssrvExit(SSERR_INITDATA);
    }
    close(fd_lockfile);
  }
  ssrvExit(SSERR_NOTINSTALLED);
}
#endif /* #ifndef FORK_VER */

#ifndef FORK_VER
/* ------------------------ declare_something ------------------------------ */
static void declare_something(char cmd)
{ char buf[16];
  int s,l;
  struct sockaddr_un su_addr;

  if((s = socket(AF_UNIX, SOCK_DGRAM, 0)) < 0)
  { if(!silenceMode) fprintf(stderr,szErrorCantOpenSocket3,szDaemonName,SSERR_CANTOPENSOCK,errno);
    ssrvExit(SSERR_CANTOPENSOCK);
  }
  prepare_cmdbuffer(buf,cmd); /* prepare command buffer */

  l = sendto(s,buf,4,0,(struct sockaddr*)&su_addr,make_sockaddr(&su_addr,szSSServerInfoSockPath)); ssrvCloseSocket(s);
  if(l != 4)
  { if(!silenceMode) fprintf(stderr,szErrorDaemonNotInstalled3,szDaemonName,SSERR_NOTINSTALLED,errno);
    ssrvExit(SSERR_NOTINSTALLED);
  }
  ssrvExit(SSERR_SUCCESS);
}
/* ---------------------- declare_daemon_stop ------------------------------ */
static void declare_daemon_stop(void)
{ declare_something(SSRVCMD_STOP); /* declare 'STOP' */
}
/* ---------------------- declare_reload_info ------------------------------ */
static void declare_reload_filterinfo(void)
{ declare_something(SSRVCMD_RELOAD); /* declare Reload Info */
}
#endif /* #ifndef FORK_VER */

/* ----------------------- set_resource_path ------------------------------- */
#ifdef SSRV_ENABLE_CHANGE_RESPATH
static void set_resource_path(const char *respath)
{ struct stat statbuf;
  int i;
  if(!respath || !respath[0] || (i = strlen(respath)) >= sizeof(szResourcePath))
  { if(!silenceMode) fprintf(stderr,szErrorInvalidParameter3,szDaemonName,SSERR_INVPARAM,respath ? respath : szNONE);
    ssrvExit(SSERR_INVPARAM);
  }
  strcpy(szResourcePath,respath);
  if(szResourcePath[i-1] == '/')
  { if(i == 1)
    { syslog_msg(LOG_ERR,SRVSEQN_PATHNOTALLOWED,szResPathNotAllowed);
      if(!silenceMode) fprintf(stderr,szErrorPathNotAllowed2,szDaemonName,SSERR_INVPARAM);
      ssrvExit(SSERR_INVPARAM);
    }
    szResourcePath[i] = 0;
  }
  if(szResourcePath[0] != '/')
  { syslog_msg(LOG_ERR,SRVSEQN_RESPATHNOTABS,szResPathNotAbsolute,szResourcePath);
    if(!silenceMode) fprintf(stderr,szErrorPathNotAbsolute3,szDaemonName,SSERR_INVPARAM,szResourcePath);
    ssrvExit(SSERR_INVPARAM);
  }
  if(stat(szResourcePath,&statbuf) || !S_ISDIR(statbuf.st_mode))
  { if(!inetd_mode) syslog_msg(LOG_WARNING,SRVSEQN_CANFINDRESPATH,szCantFindResourcePath,szResourcePath);
    if(!silenceMode) fprintf(stderr,szWarningCantFindResPath2,szDaemonName,szResourcePath);
  }
  cResourcePathSize = strlen(szResourcePath);
}
#endif /* #ifdef SSRV_ENABLE_CHANGE_RESPATH */

/* ------------------- set_switcher_path_and_name -------------------------- */
#ifdef SSRV_SWITCHER_ON
static void set_switcher_path_and_name(const char *switchername)
{ struct stat statbuf;
  if(!switchername || !switchername[0] || strlen(switchername) >= sizeof(szSwitcherLibPath))
  { if(!silenceMode) fprintf(stderr,szErrorInvalidParameter3,szDaemonName,SSERR_INVPARAM,switchername ? switchername : szNONE);
    ssrvExit(SSERR_INVPARAM);
  }
  strcpy(szSwitcherLibPath,switchername);
  if(stat(szSwitcherLibPath,&statbuf) || !S_ISREG(statbuf.st_mode))
  { syslog_msg(LOG_WARNING,SRVSEQN_CANFINDSWCLIB,szCantFindSwitcherLibrary,szSwitcherLibPath);
    if(!silenceMode) fprintf(stderr,szWarningCantFindSwcLib2,szDaemonName,szSwitcherLibPath);
  }
}
#endif /* #ifdef SSRV_SWITCHER_ON */

/* ------------------- set_default_html_filename --------------------------- */
static void set_default_html_filename(const char *fname)
{ struct stat statbuf;
  if(fname) while(*fname == '/' || *fname == '\\') fname++;
  if(!fname || !fname[0] || strlen(fname) >= sizeof(szDefaultHtmlFileName))
  { if(!silenceMode) fprintf(stderr,szErrorInvalidParameter3,szDaemonName,SSERR_INVPARAM,fname ? fname : szNONE);
    ssrvExit(SSERR_INVPARAM);
  }
  strcpy(szDefaultHtmlFileName,fname);
}
/* --------------------- set_listen_port_number ---------------------------- */
static void set_listen_port_number(const char *portnumstr)
{ if(!portnumstr || !portnumstr[0] || (sscanf(portnumstr,"%d",&listen_port) != 1) ||
     (listen_port <= 0) || (listen_port > IPPORT_MAXPORT))
  { syslog_msg(LOG_ERR,SRVSEQN_INVPORTNUMBER,szInvalidListenPortNum,portnumstr);
    if(!silenceMode) fprintf(stderr,szErrorInvalidPortNum3,szDaemonName,SSERR_INVPARAM,portnumstr ? portnumstr : szNONE);
    ssrvExit(SSERR_INVPARAM);
  }
}
/* --------------------- set_inetd_mode_value ------------------------------ */
static void set_inetd_mode_value(const char *_timeout)
{ int type,typelen;
  if(_timeout && _timeout[0])
  { if(sscanf(_timeout,"%d",&inetd_mode) != 1)
    { if(!silenceMode) fprintf(stderr,szErrorInvalidParameter3,szDaemonName,SSERR_INVPARAM,_timeout ? _timeout : szNONE);
      ssrvExit(SSERR_INVPARAM);
    }
    if(inetd_mode <= 0) inetd_mode = SSRV_DEFAULT_INETD_TIMEOUT;
  }
  inetd_sock = 0;
  typelen    = sizeof(type);
  type       = SOCK_DGRAM;
  if(getsockopt(inetd_sock,SOL_SOCKET,SO_TYPE,&type,&typelen) < 0 || type != SOCK_STREAM)
  { inetd_sock = INVALID_SOCKET_VAL; inetd_mode = 0;
    if(!silenceMode) fprintf(stderr,szErrorInvalidAcceptSocket,szDaemonName,SSERR_INVPARAM);
    ssrvExit(SSERR_INVPARAM);
  }
}

#ifdef SSRV_SSDB_INCLUDE
/* ----------------------- set_database_name ------------------------------- */
static void set_database_name(char *_optarg)
{ if(_optarg && _optarg[0])
  { lpszDatabaseName = _optarg;
  }
}
/* ------------------------ set_table_name --------------------------------- */
static void set_table_name(char *_optarg)
{ if(_optarg && _optarg[0])
  { lpszDbAddrTableName = _optarg;
  }
}
#endif /* #ifdef SSRV_SSDB_INCLUDE */

/* -------------------- set_dir_listing_flag ------------------------------- */
static void set_dir_listing_flag(char *_optarg)
{ if(_optarg && _optarg[0])
  { if(!strcasecmp(_optarg,szOn) || !strcmp(_optarg,"1")) dir_listing_flg = 1;
    else if(!strcasecmp(_optarg,szOff) || !strcmp(_optarg,"0")) dir_listing_flg = 0;
    else
    { if(!silenceMode) fprintf(stderr,szErrorInvalidParameter3,szDaemonName,SSERR_INVPARAM,_optarg);
      ssrvExit(SSERR_INVPARAM);
    }
  }
}

/* ------------------------- print_usage ----------------------------------- */
static void print_usage(int exit_code)
{ if(!silenceMode)
  { fprintf(stderr,"Usage:\n");
    fprintf(stderr,privModeFlag ?
#ifdef INCDEBUG
#ifdef SSRV_ENABLE_CHANGE_RESPATH
    "%s [-d] [-q] [-r] [-a<listen_port>][-p<resource_path>] [-l<switcher_library>] [-n<dbname>] [-t<addr_tablename>] [-x on|off] [-i] [-v] [-c] [-h]\n" : "%s [-i] [-v] [-c] [-h]\n",szDaemonName);
#else
    "%s [-d] [-q] [-r] [-a<listen_port>] [-l<switcher_library>] [-n<dbname>] [-t<addr_tablename>] [-x on|off] [-i] [-v] [-c] [-h]\n" : "%s [-i] [-v] [-c] [-h]\n",szDaemonName);
#endif
#else
#ifdef SSRV_ENABLE_CHANGE_RESPATH
    "%s [-q] [-r] [-a<listen_port>] [-p<resource_path>] [-l<switcher_library>] [-n<dbname>] [-t<addr_tablename>] [-x on|off] [-i] [-v] [-c] [-h]\n" : "%s [-i] [-v] [-c] [-h]\n",szDaemonName);
#else
    "%s [-q] [-r] [-a<listen_port>] [-l<switcher_library>] [-n<dbname>] [-t<addr_tablename>] [-x on|off] [-i] [-v] [-c] [-h]\n" : "%s [-i] [-v] [-c] [-h]\n",szDaemonName);
#endif
#endif
    if(privModeFlag)
    {
#ifdef INCDEBUG
      fprintf(stderr," -d - start in \"Debug\" mode\n");
#endif
#ifndef FORK_VER
      fprintf(stderr," -q - unload %s from memory (-stop or -kill)\n",szDaemonName);
      fprintf(stderr," -r - declare \"reload configuration\"\n");
#endif
#ifdef SSRV_ENABLE_CHANGE_RESPATH
      fprintf(stderr," -p - set \"resources\" path (default: \"%s\")\n",szResourcePath);
#endif
#ifdef SSRV_SWITCHER_ON
      fprintf(stderr," -l - set \"switcher\" library path and name (default: \"%s\")\n",szSwitcherLibPath);
#endif
      fprintf(stderr," -a - set port number for \"accept\" new connection (default \"listen\" port number: %d)\n",listen_port);
#ifdef SSRV_SSDB_INCLUDE
      fprintf(stderr," -n - set IP address filter \"dbname\" (default: \"%s\")\n",szDatabaseName);
      fprintf(stderr," -t - set IP address filter \"tablename\" (default: \"%s\")\n",szDbAddrTableName);
#endif
      fprintf(stderr," -u - set \"inetd_mode\" timeout\n");
      fprintf(stderr," -f - set default resource fname instead \"/\" (default: \"%s\")\n",szDefaultHtmlFileName);
      fprintf(stderr," -x - set enable/disable directory browsing (default: \"%s\")\n",dir_listing_flg ? szOn : szOff);
    }
#ifndef FORK_VER
      fprintf(stderr," -i - print server statistics and exit (-status or -state)\n");
#endif
      fprintf(stderr," -v - print version info and exit\n");
      fprintf(stderr," -c - print copyright info and exit\n");
      fprintf(stderr," -h - print this info and exit\n");
  }
  ssrvExit(exit_code);
}
/* ------------------------ print_version ---------------------------------- */
static void print_version(void)
{ if(!silenceMode) fprintf(stderr,"%s %d.%d (%s mode)\n",szDaemonName,SSRV_VMAJOR,SSRV_VMINOR,privModeFlag ? "priv" : "user");
  ssrvExit(SSERR_SUCCESS);
}
/* ----------------------- print_copyright --------------------------------- */
static void print_copyright(void)
{ if(!silenceMode) fprintf(stderr,"%s, %s\n",szDaemonName,szCopyright); ssrvExit(SSERR_SUCCESS);
}
/* ------------------------ parse_parameters ------------------------------- */
static void parse_parameters(int argc,char *argv[])
{ char tmp[1024];
  int c,i,err = 0;
#ifdef INCDEBUG
  char *cmdkey_string = privModeFlag ? "hHcCvViIqQrRu:U:f:F:n:N:t:T:p:P:l:L:dDs:S:k:K:a:A:x:X:" : "hHcCvViIs:S:";
#else
  char *cmdkey_string = privModeFlag ? "hHcCvViIqQrRu:U:f:F:n:N:t:T:p:P:l:L:s:S:k:K:a:A:x:X:"   : "hHcCvViIs:S:";
#endif
  while((c = getopt(argc,argv,cmdkey_string)) != (-1) && !err)
  { switch(c) {
    case 'A' : /* Only in Priv mode */
    case 'a' : set_listen_port_number(optarg);
               break;
#ifdef INCDEBUG
    case 'D' : /* Only in Priv mode */
    case 'd' : debug_mode++;
               break;
#endif
    case 'U' : /* Only in Priv mode */
    case 'u' : set_inetd_mode_value(optarg);
               break;
    case 'F' : /* Only in Priv mode */
    case 'f' : set_default_html_filename(optarg);
               break;
    case 'C' :
    case 'c' : print_copyright();
               break;
    case 'I' :
    case 'i' : print_daemon_statistics();
               break;
    case 'K' : /* Only in Priv mode */
    case 'k' : 
#ifndef FORK_VER
               if(optarg && privModeFlag)
               { for(i = 0;i < sizeof(tmp)-1 && optarg[i];i++) tmp[i] = tolower((int)optarg[i]);
                 tmp[i] = 0;
                 if(!strcmp("ill",tmp)) { declare_daemon_stop(); break; }
               }
               err++;
#else
               print_usage(SSERR_INVPARAM);
#endif
               break;
    case 'S' :
    case 's' : if(optarg)
               { for(i = 0;i < sizeof(tmp)-1 && optarg[i];i++) tmp[i] = tolower((int)optarg[i]);
                 tmp[i] = 0;
#ifndef FORK_VER
                      if(!strcmp("top",tmp) && privModeFlag)          { declare_daemon_stop(); break; }
                 else
#endif
		      if(!strcmp("tatus",tmp) || !strcmp("tate",tmp)) { print_daemon_statistics(); break; }
                 else
		      if(!strcmp("gi",tmp))                           { print_copyright(); break; }
                 else
		      if(!strcmp("tart",tmp) && privModeFlag)           break;
                 else
		      if(!strcmp("ilence",tmp))                       { silenceMode++; break; }
               }
               err++;
               break;
    case 'Q' : /* Only in Priv mode */
    case 'q' :
#ifndef FORK_VER
               declare_daemon_stop();
#else
               print_usage(SSERR_INVPARAM);
#endif
               break;
    case 'R' : /* Only in Priv mode */
    case 'r' :
#ifndef FORK_VER
               declare_reload_filterinfo();
#else
               print_usage(SSERR_INVPARAM);
#endif
               break;
    case 'P' : /* Only in Priv mode */
    case 'p' :
#ifdef SSRV_ENABLE_CHANGE_RESPATH
               set_resource_path(optarg);
#else
               print_usage(SSERR_INVPARAM);
#endif
               break;
    case 'N' : /* Only in Priv mode */
    case 'n' :
#ifdef SSRV_SSDB_INCLUDE
               set_database_name(optarg);
#else
               print_usage(SSERR_INVPARAM);
#endif
               break;
    case 'T' : /* Only in Priv mode */
    case 't' :
#ifdef SSRV_SSDB_INCLUDE
               set_table_name(optarg);
#else
               print_usage(SSERR_INVPARAM);
#endif
               break;	       
    case 'L' : /* Only in Priv mode */
    case 'l' : 
#ifdef SSRV_SWITCHER_ON
               set_switcher_path_and_name(optarg);
#else
               print_usage(SSERR_INVPARAM);
#endif
               break;
    case 'V' :
    case 'v' : print_version();
               break;
    case 'H' :
    case 'h' : print_usage(SSERR_SUCCESS);
               break;

    case 'X' :  /* Only in Priv mode */
    case 'x' : set_dir_listing_flag(optarg);
               break;
    default  : err++;
               break;
    }; /* end of switch(c) */
  }
  if(err || (argc != optind)) print_usage(SSERR_INVPARAM);

  if(!privModeFlag)
  { if(!silenceMode) fprintf(stderr,szErrorCantLoadInNonRoot,szDaemonName,SSERR_INVACCOUNT);
    ssrvExit(SSERR_INVACCOUNT);
  }
}
/* ------------------------------ main ------------------------------------- */
int main(int argc,char *argv[])
{ /* Initialize all static variables and buffers */
  zinit_resources();

  /* Parse command string parameters */
  parse_parameters(argc,argv);

  /* Check previous instance */
  instance_check();

  /* Check operation system parameters and adjust some internal variables */
  check_osenv();
  
  /* Initialize database */
  init_db();

  /* Daemonize instance */
  daemon_init();

  /* Initialize mutex/conditions and some additional stuff */
  init_daemon_resources();

  /* Initialize threads */
  init_daemon_threads();

  /* Initialize default IP filter */
  init_default_ipfilter();

  /* Wait termination */
  process();

  /* Free resources and clear temp files */
  done_daemon_resources();

  /* Send final message to syslog daemon */
  if(!inetd_mode) syslog_msg(LOG_INFO,SRVSEQN_STOPED,szStoped);

  return SSERR_SUCCESS;
}
