/* --------------------------------------------------------------------------- */
/* -                                SSRV.H                                   - */
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
#ifndef H_SSRV_H
#define H_SSRV_H

/* ------------------------------------------------------------------------- */
/* Project Name */
/*#define PNAME_CD20 1*/
#define PNAME_ESP  1


/* ------------------------------------------------------------------------- */
#ifdef PNAME_CD20
#ifdef PNAME_ESP
#include "Incorrect_Project_Definition.h"
#endif
#endif
#ifdef PNAME_ESP
#ifdef PNAME_CD20
#include "Incorrect_Project_Definition.h"
#endif
#endif
/* ------------------------------------------------------------------------- */
/* Configurable WEB Server version definition                                */
#define SSRV_VMAJOR                        1     /* Configurable WEB Server major version number */
#define SSRV_VMINOR                        6     /* Configurable WEB Server minor version number */
/* ------------------------------------------------------------------------- */
#ifndef INVALID_SOCKET_VAL
#define INVALID_SOCKET_VAL                 (-1)  /* Invalid socket value */
#endif
/* ========================================================================= */
/*                       Configuration flags Section                         */
/*                      Please, be very careful here!                        */
/* ========================================================================= */
/* ------------------------------------------------------------------------- */
/* Include debug output */
#define INCDEBUG 1
#ifdef INCDEBUG
#define dprintf if(debug_mode) printf
#endif
/* ------------------------------------------------------------------------- */
/* Enable or Disable all IP address by default */
/* defined for CD2.0 project */
/* not defined for ESP project */
#ifdef PNAME_CD20
#define SSRV_OPEN_ALLIPADDR               1  /* CD2.0 */
#endif
/* ------------------------------------------------------------------------- */
/* Include SSDB */
/* defined for ESP project */
/* not defined for CD2.0 project */
#ifdef PNAME_ESP
#define SSRV_SSDB_INCLUDE                 1  /* ESP */
#endif
/* ------------------------------------------------------------------------- */
/* Enable "Switcher" interface support */
/* defined for ESP project */
/* not defined for CD2.0 project */
#ifdef PNAME_ESP
#define SSRV_SWITCHER_ON                  1   /* ESP */
#endif
/* ------------------------------------------------------------------------- */
/* Enable command string key 'p' (change resource path) */
#define SSRV_ENABLE_CHANGE_RESPATH        1  /* ESP, CD2.0 */

/* ------------------------------------------------------------------------- */
/* Undef some definitions for FORK_VER */
#ifdef FORK_VER
#ifdef SSRV_SWITCHER_ON
#undef SSRV_SWITCHER_ON
#endif
#ifdef SSRV_SSDB_INCLUDE
#undef SSRV_SSDB_INCLUDE
#endif
#endif
/* ========================================================================= */

/* ------------------------------------------------------------------------- */
#ifdef PNAME_ESP
#define SSRV_DEFAULT_RESOURCESPATH     "/var/esp/ssc"        /* Default Server resources path: "/var/esp/ssc" for ESP */
#endif

#ifdef PNAME_CD20
#define SSRV_DEFAULT_RESOURCESPATH    "/CDROM/httpd/htdocs"  /* Default Server resources path: "/CDROM/httpd/htdocs" for CD2.0 */
#endif

#define SSRV_DEFAULT_SWITCHLIBPATH     "/usr/lib32/internal/libssrvswitch.so" /* Default "Switcher" library path and name*/

#ifdef PNAME_ESP
#define SSRV_DEFAULT_LISTENPORT        5554                /* Default port for listen: 5554 for ESP project */
#endif

#ifdef PNAME_CD20
#define SSRV_DEFAULT_LISTENPORT        9237                /* Default port for listen: 9237 for CD2.0 project */
#endif

#define SSRV_DEFAULT_DIR_LISTING_VALUE 0                   /* Default "dir_listing_flg" flag value (0 or 1 only) */

#ifdef PNAME_ESP
#define SSRV_APPLICATION_NAME          "esphttpd"          /* Application name: "esphttpd" for ESP project */
#endif

#ifdef PNAME_CD20
#define SSRV_APPLICATION_NAME          "ist_httpd"         /* Application name: "ist_httpd" for CD2.0 project */
#endif
/* ------------------------------------------------------------------------- */
#define SSRV_DEFAULT_INETD_TIMEOUT     300                 /* Default "inetd" unload timeout */
#define SSRV_UNLOAD_SLEEP_STEP         3                   /* Sleep timeout inside "unload" thread */
#define SSRV_SSDBWAIT_TIMEOUT          30                  /* "SSDB ready" wait timeout (only for inetd mode) */
/* ------------------------------------------------------------------------- */
#define SSRV_UNAME_KEY_VALUE     0x19921965              /* xcode key for "username" */
#define SSRV_PASSWD_KEY_VALUE    0x3092E4CA              /* xcode key for "password" */
/* ------------------------------------------------------------------------- */
/* SServer Statistics storage structure                                      */
typedef struct s_statinfo_struct {
  /* Global counters */
  unsigned long volatile statGlbIpAddrEntryAllocCnt;     /* Stat: Global info, total ip_addr_entry_alloc counter */
  unsigned long volatile statGlbIpAddrEntryFreeCnt;      /* Stat: Global info, total ip_addr_entry_free_list counter */
  unsigned long volatile statGlbTcbEntryAllocCnt;        /* Stat: Global info, total tcb_entry_alloc counter */
  unsigned long volatile statGlbTcbEntryFreeCnt;         /* Stat: Global info, total tcb_entry_free_list counter */
  /* ----------------------------------------------------------------------- */
  /* Connection thread(s) */
  unsigned long volatile statConSwchLibLoadError;        /* Stat: Connection thread, total load_switcher_library_error counter */
  unsigned long volatile statConSwchLibInitError;        /* Stat: Connection thread, total init_switcher_library_error counter */
  unsigned long volatile statConTotalRcvError;           /* Stat: Connection thread, total receive_error counter */
  unsigned long volatile statConTotalParseHeaderError;   /* Stat: Connection thread, total parse_http_header_error counter */
  unsigned long volatile statConTotalSendRespHdrError;   /* Stat: Connection thread, total send_response_header_error counter */
  unsigned long volatile statConTotalRcvContentLenTooBig;/* Stat: Connection thread, total rcv_content_length_too_big counter */
  unsigned long volatile statConTotalInvalidPostBodyLen; /* Stat: Connection thread, total invalid_post_request_body_length counter */
  unsigned long volatile statConTotalRcvContentLenIsBad; /* Stat: Connection thread, total rcv_content_length_is_bad counter */
  /* ----------------------------------------------------------------------- */
  /* Load IP filter info thread */
  unsigned long volatile statLcfTotalCreateErrHError;    /* Stat: LoadIPFilterInfo thread, total create_error_handle_error counter */
  unsigned long volatile statLcfTotalNewClientError;     /* Stat: LoadIPFilterInfo thread, total create_new_client_error counter */
  unsigned long volatile statLcfTotalOpenConError;       /* Stat: LoadIPFilterInfo thread, total open_connection_error counter */
  unsigned long volatile statLcfTotalSendReqError;       /* Stat: LoadIPFilterInfo thread, total send_request_error counter */
  unsigned long volatile statLcfTotalSendKeyReqError;    /* Stat: LoadIPFilterInfo thread, total send_key_request_error counter */
  unsigned long volatile statLcfTotalGetNumRecError;     /* Stat: LoadIPFilterInfo thread, total get_number_of_records_error counter */
  unsigned long volatile statLcfTotalGetNumColError;     /* Stat: LoadIPFilterInfo thread, total get_number_of_columns_error counter */
  unsigned long volatile statLcfTotalWhiteListOverload;  /* Stat: LoadIPFilterInfo thread, total white_list_overload counter */
  unsigned long volatile statLcfTotalDarkListOverload;   /* Stat: LoadIPFilterInfo thread, total dark_list_overload counter */
  unsigned long volatile statLcfTotalProcessReqError;    /* Stat: LoadIPFilterInfo thread, total process_request_error counter */
  unsigned long volatile statLcfTotalUnZeroNumRecord;    /* Stat: LoadIPFilterInfo thread, total userentry_zero_records_error counter */
  unsigned long volatile statLcfTotalLoadIpError;        /* Stat: LoadIPFilterInfo thread, total load_ip_filter_error counter */
  unsigned long volatile statLcfTotalLoadUentryError;    /* Stat: LoadIPFilterInfo thread, total load_userentry_error counter */
  /* ----------------------------------------------------------------------- */
  /* Listen thread statiscs */
  unsigned long volatile statLsTotalOpenSocket;          /* Stat: Listen thread, total open socket counter */
  unsigned long volatile statLsTotalOpenSocketSuccess;   /* Stat: Listen thread, total open socket success counter */
  unsigned long volatile statLsTotalOpenSocketError;     /* Stat: Listen thread, total open socket error counter */
  unsigned long volatile statLsTotalSSOTrustedError;     /* Stat: Listen thread, total tsix_on error counter */
  unsigned long volatile statLsTotalSSOReuseAddrError;   /* Stat: Listen thread, total setsockopt_reuse_addr error counter */
  unsigned long volatile statLsTotalSSOKeepAliveError;   /* Stat: Listen thread, total setsockopt_keep_alive error counter */
  unsigned long volatile statLsTotalSSODisNagleError;    /* Stat: Listen thread, total setsockopt_disable_Nagle error counter */
  unsigned long volatile statLsTotalSSOEnbLingerError;   /* Stat: Listen thread, total setsockopt_enable_linger error counter */
  unsigned long volatile statLsTotalSSOAccKeepAliveError;/* Stat: Listen thread, total accept_setsockopt_keep_alive error counter */
  unsigned long volatile statLsTotalSSOAccDisNagleError; /* Stat: Listen thread, total accept_setsockopt_disable_Nagle error counter */
  unsigned long volatile statLsTotalSSOAccEnbLingerError;/* Stat: Listen thread, total accept_setsockopt_enable_linger error counter */
  unsigned long volatile statLsTotalBindSocket;          /* Stat: Listen thread, total bind_listen_socket counter */
  unsigned long volatile statLsTotalBindSocketError;     /* Stat: Listen thread, total bind_listen_socket_error counter */
  unsigned long volatile statLsTotalBindSocketSuccess;   /* Stat: Listen thread, total bind_listen_socket_success counter */
  unsigned long volatile statLsTotalListenSuccess;       /* Stat: Listen thread, total listen_socket_success counter */
  unsigned long volatile statLsTotalListenError;         /* Stat: Listen thread, total listen_socket_error counter */
  unsigned long volatile statLsTotalAcceptSuccess;       /* Stat: Listen thread, total accept_socket_success counter */
  unsigned long volatile statLsTotalAcceptError;         /* Stat: Listen thread, total accept_socket_error counter */
  unsigned long volatile statLsTotalThreadCreated;       /* Stat: Listen thread, total thread_created_success counter */
  unsigned long volatile statLsTotalCreThreadPermErr;    /* Stat: Listen thread, total create_thread_permanent_error counter */
  unsigned long volatile statLsTotalCreThreadNotPermErr; /* Stat: Listen thread, total create_thread_non_permanent_error counter */
  /* ----------------------------------------------------------------------- */
  /* Info thread statistics */
  unsigned long volatile statInTotalOpenSocket;          /* Stat: Info thread, total open socket counter */
  unsigned long volatile statInTotalOpenSocketSuccess;   /* Stat: Info thread, total open socket success counter */
  unsigned long volatile statInTotalOpenSocketError;     /* Stat: Info thread, total open socket error counter */
  unsigned long volatile statInTotalBindSocket;          /* Stat: Info thread, total bind socket counter */
  unsigned long volatile statInTotalBindSocketSuccess;   /* Stat: Info thread, total bind socket success counter */
  unsigned long volatile statInTotalBindSocketError;     /* Stat: Info thread, total bind socket error counter */
  unsigned long volatile statInTotalSetOptError;         /* Stat: Info thread, total setsockopt error counter */
  unsigned long volatile statInTotalReadSocket;          /* Stat: Info thread, total read socket counter */
  unsigned long volatile statInTotalReadSocketSuccess;   /* Stat: Info thread, total read socket success counter */
  unsigned long volatile statInTotalReadSocketError;     /* Stat: Info thread, total read socket error counter */
  unsigned long volatile statInTotalExitRequest;         /* Stat: Info thread, total "Exit" request counter */
  unsigned long volatile statInTotalInfoRequest;         /* Stat: Info thread, total "GetInfo" reqiest counter */
  unsigned long volatile statInTotalSendInfoSuccess;     /* Stat: Info thread, total send info success counter */
  unsigned long volatile statInTotalSendInfoError;       /* Stat: Info thread, total send info error counter */
  unsigned long volatile statInTotalTestRequest;         /* Stat: Info thread, total test request counter */
  unsigned long volatile statInTotalReloadRequest;       /* Stat: Info thread, total reload request counter */
  unsigned long volatile statInTotalUnknownRequest;      /* Stat: Info thread, total unknown request counter */
} SSRVTATINFO;
#ifndef FORK_VER
/* ------------------------------------------------------------------------- */
/* DMUTEX - smart mutex structure                                            */
typedef struct s_dmutex {
    pthread_mutex_t mutex;           /* pthread mutex */
    int init;                        /* initialize flag */
} DMUTEX;
/* ------------------------------------------------------------------------- */
/* DCOND - smart cond structure                                              */
typedef struct s_dcond {
    pthread_cond_t cond;             /* pthread conditional variable */
    int init;                        /* initialize flag */
} DCOND;
#endif
/* ------------------------------------------------------------------------- */
/* IP address entry structure                                                */
typedef struct s_sss_ipaddr_entry {
 struct s_sss_ipaddr_entry *next;    /* pointer to next structure */
 unsigned long ipaddr;               /* IP address in network notation */
 int mask;                           /* mask of IP address */
} IPADDRE;
/* ------------------------------------------------------------------------- */
#define HHATTRN_UNKNOWN             0   /* HTTP header attribute: unknown */
#define HHATTRN_ACCEPT              1   /* HTTP header attribute: "Accept" */
#define HHATTRN_ACCEPT_CHARSET      2   /* HTTP header attribute: "Accept-Charset" */
#define HHATTRN_ACCEPT_ENCODING     3   /* HTTP header attribute: "Accept-Encoding" */    
#define HHATTRN_ACCEPT_LANGUAGE     4   /* HTTP header attribute: "Accept-Language" */   
#define HHATTRN_AUTHORIZATION       5   /* HTTP header attribute: "Authorization" */   
#define HHATTRN_FROM                6   /* HTTP header attribute: "From" */     
#define HHATTRN_HOST                7   /* HTTP header attribute: "Host" */              
#define HHATTRN_IF_MODIFIED_SINCE   8   /* HTTP header attribute: "If-Modified-Since" */
#define HHATTRN_IF_MATCH            9   /* HTTP header attribute: "If-Match" */ 
#define HHATTRN_IF_NONE_MATCH       10  /* HTTP header attribute: "If-None-Match" */
#define HHATTRN_IF_RANGE            11  /* HTTP header attribute: "If-Range" */     
#define HHATTRN_IF_UNMODIFIED_SINCE 12  /* HTTP header attribute: "If-Unmodified-Since" */
#define HHATTRN_MAX_FORWARDS        13  /* HTTP header attribute: "Max-Forwards" */
#define HHATTRN_PROXY_AUTHORIZATION 14  /* HTTP header attribute: "Proxy-Authorization" */
#define HHATTRN_RANGE               15  /* HTTP header attribute: "Range" */
#define HHATTRN_REFERER             16  /* HTTP header attribute: "Referer" */
#define HHATTRN_USER_AGENT          17  /* HTTP header attribute: "User-Agent" */
#define HHATTRN_CONNECTION          18  /* HTTP header attribute: "Connection" */
#define HHATTRN_NEGOTIATE           19  /* HTTP header attribute: "Negotiate" */
#define HHATTRN_PRAGMA              20  /* HTTP header attribute: "Pragma" */
#define HHATTRN_CONTENT_LENGTH      21  /* HTTP header attribute: "Content-length" */
/* ------------------------------------------------------------------------- */
/* HTTP header parameter structure                                           */
typedef struct s_sss_httpparam {
 int attrnum;                           /* attribute number (HHATTRN_...) */
 char *attrname_str;                    /* attribute name string */
 char *attrval_str;                     /* attribute value string */
} HTTPPAR;
/* ------------------------------------------------------------------------- */
/* Thread control block                                                      */
#define SSRV_TCBMAX_USERARGS  10        /* Max numbers of user defined arguments */
#define SSRV_TCBMAX_REALMSIZE 128       /* Max "realm" string size */
#define SSRV_TCBMAX_MIMETSIZE 64        /* Max "mime type" string size */
#define SSRV_TCBMAX_LOCATION  512       /* Max "location" string size */

typedef struct s_sss_tcb {
 unsigned long struct_size;             /* sizeof(TCBENTRY) - structure signature */
 struct s_sss_tcb *next;                /* pointer to next structure */
 struct sockaddr_in caddr;              /* Client address structure */
 int caddr_len;                         /* Client address structure size */
 char caddr_str[17];                    /* Client IP address string aaa.bbb.ccc.ddd */
 int as;                                /* accepted socket */
#ifndef FORK_VER
 pthread_t tid;                         /* thread id */
#endif
 HTTPPAR *pbuf;                         /* pair buffer */
 int pbufsize;                          /* pair buffer size */
 int pactsize;                          /* pair buffer actual size */
 char *resphdr;                         /* response header buffer pointer */
 int rhrdsize;                          /* response header buffer size */
 int rhrdactsize;                       /* response header buffer actual size */
 char *wbuf;                            /* work buffer pointer (exchange buffer server<->switcher) */
 int wbufsize;                          /* work buffer max size */
 int wbufactsize;                       /* work buffer actual data size */
 int curmethod;                         /* current request method HTTPM_... */
 char *url;                             /* URL pointer */
 int urlsize;                           /* URL size */
 char *wurl;                            /* correct URL pointer to "real" files (only for "Switcher" module */
 int httpver_major;                     /* HTTP protocol version major number */
 int httpver_minor;                     /* HTTP protocol version minor number */
 int keep_alive;                        /* Connection "Keep alive" counter */
 char authorstr[512];                   /* Authorization string */
 char *username;                        /* Pointer to "User name" substring inside authorstr array */
 char *passwd;                          /* Pointer to "Password" substring inside authorstr array */
 char *reqbodyptr;                      /* Body pointer for some request (POST)*/
 int rcvContentLen;                     /* Rcv Content len, not used if equ (0) (make sence only for POST method) */ 

 /* respons values */
 int retcode;                           /* Response return code (see: ssrv_http.h, HTTP_...)*/ 
 int contentLen;                        /* Content len, not used if equ (-1) */ 
 int nocacheflg;                        /* "No-Cache" flag */
 char realm[SSRV_TCBMAX_REALMSIZE+1];   /* Response "realm" string for WWW-Authenticate field */
 char mimetype[SSRV_TCBMAX_MIMETSIZE+1];/* Response MIME type */
 char location[SSRV_TCBMAX_LOCATION+1]; /* Response "Location" and "Content-Location" buffer */

 void *userArg[SSRV_TCBMAX_USERARGS];   /* User arguments array */
} TCBENTRY;
/* ------------------------------------------------------------------------- */
#define SSRV_MAXHTTPREQ_PAIR   512   /* Max http request header pair counter */


#endif /* #ifndef H_SSRV_H */
