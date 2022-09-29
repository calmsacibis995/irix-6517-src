/* --------------------------------------------------------------------------- */
/* -                             EVENTMOND.C                                 - */
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
/* EventMon will run as a daemon and will startup when the system boots.       */
/* In the case that the SSDB server is not running or the SEM is not running,  */
/* EventMon will buffer events that come from syslogd and sslogger.            */
/* EventMon monitors system uptime by constantly writing to a file the last    */
/* time system is up (former amtickerd functionality).                         */
/* The frequency of writing to the file is driven by command line arguments.   */
/* EventMon also takes another command line option, statusinterval which       */
/* represents the interval at which status reports need to be sent.            */
/* Project name: ESP                                                           */
/* Target file location: /usr/etc/eventmond                                    */
/* Startup script: /etc/init.d/eventmonitor                                    */
/* --------------------------------------------------------------------------- */
/* $Revision: 1.15 $ */
/* Need to do:                                                                 */
/* 1. add gethostbyname->gethostbyaddr into initializeHostnameInfo             */
/* 2. integrate functionality of the semd                                      */

/* #ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 199506L
#endif
*/
/* Include debug output */
#define INCDEBUG 1

#ifndef _BSD_SIGNALS
#define _BSD_SIGNALS
#endif
#ifndef _BSD_TYPES
#define _BSD_TYPES
#endif
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <pthread.h>
#include <paths.h>
#include <fcntl.h>
#include <semaphore.h>
#include <dlfcn.h>
#include <time.h>
#include <dirent.h>
#include <ctype.h>
#include <sys/un.h>
#include <sys/ioctl.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <ssdbapi.h>
#include <ssdberr.h>

extern char *optarg;
extern int optind;

#ifdef __TIME__
#ifdef __DATE__
#define INCLUDE_TIME_DATE_STRINGS 1
#endif
#endif

/* ------------------------------------------------------------------------- */
/* EventMon Daemon version definition                                        */
#define EVENTMOND_VMAJOR   1        /* EventMon Daemon major version number */
#define EVENTMOND_VMINOR   6        /* EventMon Daemon minor version number */
/* ------------------------------------------------------------------------- */
#define RCVPACK_MAXSIZE             (1024*16)                  /* Rcv buffer max size (bytes) */
#define RCVINFOREQ_MAXSIZE          (1024*8)                   /* Rcv/Snd buffer size for info socket (bytes) */
#define RCVSOCK_RCVBUFSIZE          (1024*16)                  /* Receive socket "Internal Buffer Size" value */
#define SEM_EVENTDATA_MAX_SIZE      (1024*16)                  /* SEM API event data max size */
#define EMSGBUF_SIZE                512                        /* EventMon message buffer size (bytes) */
#define EMSGPREALLOC_CNT            50                         /* EventMon message structs prealloced counter */
#define EMSGALLOCMAX_CNT            (EMSGPREALLOC_CNT+150)     /* EventMon message structs alloc max counter */
#define EMSGFPREALLOC_CNT           25                         /* EventMon prealloc counter for file messages */
#define EMSGFILE_MAXCNT             (EMSGFPREALLOC_CNT+75)     /* Max limit: EventMon temp files message counter */
#define EFINFOPREALLOC_CNT          100                        /* Event Filter info structure prealloc counter */
#define EMSGFNAME_MAXSIZE           64                         /* EventMon temp file name max size */
#define INPSOCKNAME_MAXSIZE         512                        /* Input socket name max size */
#define EVENTMONSOCK_PATH   "/tmp/.eventmond.events.sock"      /* default syslogd socket name */
#define SYSLOGCONF_FNAME    "/etc/syslog.conf"                 /* default syslogd config file */
#define EVM_AMTICKER_FNAME  "/var/adm/avail/.save/lasttick"    /* default amticker timestamp file name */
#define EVM_AMDIAG_PATH     "/usr/etc/amdiag"                  /* Amdiag path */
#define EVM_AMDIAG_CMDLINE  "/usr/etc/amdiag STATUS &"         /* Amdiag command string for shell */
#define EVM_SSDBCLIENT_NAME         ""                         /* SSDB Connect Client name */
#define EVM_SSDBCLIENT_PASSWORD     ""                         /* SSDB Connect Client password */
#define INVALID_SOCKET_VAL          (-1)                       /* Invalid socket value */
#define ALLMSG_DEFAULT_ENABLED_FLAG 0                          /* Enabled/Disabled default status for all messages */
#define EVENTMONSOCK_PREFIX         '@'                        /* EventMon UNIX socket prefix in syslog.conf file */
#define SSSMSG_TAG1                 '|'                        /* SSS message tag 1 */
#define SSSMSG_TAG2                 '$'                        /* SSS message tag 2 */
#define SSSMSG_BEGINBLK             '<'                        /* SSS message 'begin block' symbol */
#define SSSMSG_ENDBLK               '>'                        /* SSS message 'end block' symbol */
#define OFBDMSG_PREFIX              '#'                        /* Out of band message prefix */
#define TRYOUTPUT_REPCNT            200                        /* Default max repeate counter for "non fatal errors" in output thread */
#define SEMERRCNT_OPENSOCK          100                        /* SEM API: Error counter: "red limit" for "open socket" errors */
#define SEMERRCNT_BINDSOCK          500                        /* SEM API: Error counter: "red limit" for "bind socket" errors */
#define EVM_MAXHOSTNAMESIZE         128                        /* Local hostname max size */
#define LOCAL_SSDB_NEWCLIENTFLAGS (SSDB_CLIFLG_ENABLEDEFUNAME|SSDB_CLIFLG_ENABLEDEFPASWD) /* Create new SSDB client flags */
#define LOCAL_SSDB_NEWCONNECTFLAGS (SSDB_CONFLG_RECONNECT|SSDB_CONFLG_REPTCONNECT|SSDB_CONFLG_DEFAULTHOSTNAME) /* Create new SSDB connection flags */
/* --------------------------------------------------------------------------- */
/* SEM API version definition                                                  */
#define SEMAPI_VERMAJOR              2                         /* Major version number of SEM API */
#define SEMAPI_VERMINOR              0                         /* Minor version number of SEM API */
/* ------------------------------------------------------------------------- */
/* SEM API error code for all SEM API functions                              */
#define SEMERR_SUCCESS               0                         /* Success */
#define SEMERR_FATALERROR            1                         /* Unrecoverable */
#define SEMERR_ERROR                 2                         /* Recoverable (retry) */
/* ------------------------------------------------------------------------- */
/* Different timeouts definition                                             */
#define TOUTBASE_LLIB       10        /* ReadFilter Thread: "Start" timeout for load ssdb dynamic library (sec.) */
#define TOUTPLUS_LLIB       30        /* ReadFilter Thread: "Plus" timeout for load ssdb library (sec.) */
#define TOUTMAX_LLIB        1800      /* ReadFilter Thread: "Max" timeout for load ssdb library (sec.) */
#define TOUTBASE_ILIB       30        /* ReadFilter Thread: "Start" timeout for init ssdb library (sec.) */
#define TOUTPLUS_ILIB       30        /* ReadFilter Thread: "Plus" timeout for init ssdb library (sec.) */
#define TOUTMAX_ILIB        1800      /* ReadFilter Thread: "Max" timeout for init ssdb library (sec.) */
#define TOUTBASE_RSOPENSOCK 1         /* ReadSock Thread: "Start" timeout for open/bind socket (sec.) */
#define TOUTPLUS_RSOPENSOCK 1         /* ReadSock Thread: "Plus" timeout for open/bin socket (sec.) */
#define TOUTMAX_RSOPENSOCK  5         /* ReadSock Thread: "Max" timeout for open/bind socket (sec.) */
#define TOUTMIN_RSSLOGDROP  600       /* ReadSock Thread: "Min" timeout between log message "There are no buffers" (sec.) */
#define TOUTMAX_RSSLOGDROP  3600      /* ReadSock Thread: "Max" timeout between log message "There are no buffers" (sec.) */
#define TOUTBASE_IFOPENSOCK 3         /* Info Thread: "Start" timeout for open/bind socket (sec.) */
#define TOUTPLUS_IFOPENSOCK 2         /* Info Thread: "Plus" timeout for open/bin socket (sec.) */
#define TOUTMAX_IFOPENSOCK  20        /* Info Thread: "Max" timeout for open/bind socket (sec.) */
#define TOUTBASE_OUTTLIB    30        /* Output Thread: "Start" timeout for try to use 'output' support library (sec.) */
#define TOUTPLUS_OUTTLIB    15        /* Output Thread: "Plus" timeout for try to use 'output' support library (sec.) */
#define TOUTMAX_OUTTLIB     180       /* Output Thread: "Max" timeout for try to use 'output' support library (sec.) */
#define TOUTMIN_LOGFMSG     600       /* Output Thread: "Min" timeout before log error message in syslog (sec.) */
#define TOUTMAX_LOGFMSG     1800      /* Output Thread: "Max" timeout before log error message in syslog (sec.) */
#define TOUTBASE_LOADCONF   5         /* ReadFilter Thread: "Default" timeout before load configuration from database */
#define TOUTBASE_AMINTER    300       /* Amticker Thread: Default timeout for refresh time stamp file (sec.) */
#define TOUTBASE_AMSTATUS   (24*60)   /* Amticker Thread: Default timeout for exec amdiag binary (hours) */
/* ------------------------------------------------------------------------- */
/* EventMon daemon commands definition                                       */
#define EVMCMD_STOP    'Q'  /* Stop (and unload from memory) command */
#define EVMCMD_STAT    'S'  /* Statistics command */
#define EVMCMD_TEST    'T'  /* Test command */
#define EVMCMD_RFILT   'F'  /* Reload filter info */
#define EVMCMD_POUT    'O'  /* Kick off (refresh) output subsystem */
#define EVMCMD_AMTON   'A'  /* Amtickerd "on" command */
#define EVMCMD_AMTOFF  'N'  /* Amtickerd "off" command */
#define EVMCMD_SETTICK 'J'  /* Amtickerd "set tick interval" command */
#define EVMCMD_SETSTAT 'E'  /* Amtickerd "set exec status interval" command */
#define EVMCMD_SETGRP  'G'  /* Enable "Group Mode" command */
#define EVMCMD_CLRGRP  'M'  /* Disable "Group Mode" command */
/* ------------------------------------------------------------------------- */
/* EventMonitor Statistics storage structure                                 */
typedef struct s_statinfo_struct {
  /* ----------------------------------------------------------------------- */
  /* Amticker thread statistics                                              */
  unsigned long volatile statAmTotalOpenFileError;       /* Stat: Amticker thread, total open_timestamp_file_error counter */
  unsigned long volatile statAmTotalWriteFileError;      /* Stat: Amticker thread, total write_timestamp_file_error counter */
  unsigned long volatile statAmTotalFileUpdated;         /* Stat: Amticker thread, total time_stamp_file_updated counter */
  unsigned long volatile statAmTotalStatusTimeExp;       /* Stat: Amticker thread, total status_interval_expired counter */
  unsigned long volatile statAmTotalAmdiagExec;          /* Stat: Amticker thread, total exec_amdiag counter */
  unsigned long volatile statAmTotalAmdiagNotFound;      /* Stat: Amticker thread, total amdiag_not_found counter */

  /* ----------------------------------------------------------------------- */
  /* ReadSocket thread statistics                                            */
  unsigned long volatile statRsTotalOpenSocket;          /* Stat: ReadSocket thread, total open socket counter */
  unsigned long volatile statRsTotalOpenSocketSuccess;   /* Stat: ReadSocket thread, total open socket success counter */
  unsigned long volatile statRsTotalOpenSocketError;     /* Stat: ReadSocket thread, total open socket error counter */
  unsigned long volatile statRsTotalBindSocket;          /* Stat: ReadSocket thread, total bind socket counter */
  unsigned long volatile statRsTotalBindSocketSuccess;   /* Stat: ReadSocket thread, total bind socket success counter */
  unsigned long volatile statRsTotalBindSocketError;     /* Stat: ReadSocket thread, total bind socket error counter */
  unsigned long volatile statRsTotalSockChmodError;      /* Stat: ReadSocket thread, total chmod UNIX socket name error counter */
  unsigned long volatile statRsTotalSetOptError;         /* Stat: ReadSocket thread, total setsockopt error counter */
  unsigned long volatile statRsTotalReadSocket;          /* Stat: ReadSocket thread, total read socket counter */
  unsigned long volatile statRsTotalReadSocketSuccess;   /* Stat: ReadSocket thread, total read socket success counter */
  unsigned long volatile statRsTotalReadSocketError;     /* Stat: ReadSocket thread, total read socket error counter */
  unsigned long volatile statRsTotalOfbdMessages;        /* Stat: ReadSocket thread, total ofbd_messages counter */
  unsigned long volatile statRsTotalOfbdTimeMessages;    /* Stat: ReadSocket thread, total ofbd_time_messages counter */
  unsigned long volatile statRsTotalMemMsgUsage;         /* Stat: ReadSocket thread, total memory buffer usage counter */
  unsigned long volatile statRsTotalFileMsgUsage;        /* Stat: ReadSocket thread, total file buffer usage counter */
  unsigned long volatile statRsTotalDropMsg;             /* Stat: ReadSocket thread, total input msg drop counter */
  unsigned long volatile statRsTotalOpenSndSockError;    /* Stat: ReadSocket thread, total open_snd_OFBD_socket_error counter */
  unsigned long volatile statRsTotalSendOFBDAnsw;        /* Stat: ReadSocket thread, total send_OFBD_answers counter */
  unsigned long volatile statRsTotalSendOFBDAnswSuccess; /* Stat: ReadSocket thread, total send_OFBD_answers_success counter */
  unsigned long volatile statRsTotalSendOFBDAnswError;   /* Stat: ReadSocket thread, total send_OFBD_answers_error counter */
  /* ----------------------------------------------------------------------- */
  /* Info thread statistics                                                  */
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
  unsigned long volatile statInTotalInfoRequest;         /* Stat: Info thread, total info request counter */
  unsigned long volatile statInTotalTestRequest;         /* Stat: Info thread, total test request counter */
  unsigned long volatile statInTotalReloadFilterRequest; /* Stat: Info thread, total reload_filter_info request counter */
  unsigned long volatile statInTotalRefreshOutputRequest;/* Stat: Info thread, total refresh_output_system request counter */
  unsigned long volatile statInTotalAmtOnRequest;        /* Stat: Info thread, total enable_avaimonitoring_support request counter */
  unsigned long volatile statInTotalAmtOffRequest;       /* Stat: Info thread, total disable_avaimonitoring_support request counter */
  unsigned long volatile statInTotalAmtSetTickInterval;  /* Stat: Info thread, total set_avaimonitoring_tick_interval request counter */
  unsigned long volatile statInTotalAmtSetExecInterval;  /* Stat: Info thread, total set_avaimonitoring_exec_interval request counter */
  unsigned long volatile statInTotalSetGrpModeRequest;   /* Stat: Info thread, total set_group_mode request counter */
  unsigned long volatile statInTotalClrGrpModeRequest;   /* Stat: Info thread, total reset_group_mode request counter */
  unsigned long volatile statInTotalUnknownRequest;      /* Stat: Info thread, total unknown request counter */
  unsigned long volatile statInTotalOpenSndInfoSockError;/* Stat: Info thread, total open_send_info_socket_error counter */
  unsigned long volatile statInTotalSendInfoSuccess;     /* Stat: Info thread, total send info seccess counter */
  unsigned long volatile statInTotalSendInfoError;       /* Stat: Info thread, total send info error counter */
  unsigned long volatile statInTotalInvSrcAddr;          /* Stat: Info thread, total invalid_source_address counter */
  /* ----------------------------------------------------------------------- */
  /* ReadFilter thread statistics                                            */
  int volatile statRfCurrentLoadLibraryTimeout;          /* Stat: ReadFilter thread, current load_library_timeout (sec.) */
  int volatile statRfCurrentInitLibraryTimeout;          /* Stat: ReadFilter thread, current init_library_timeout (sec.) */
  unsigned long volatile statRfTotalLoadLibSuccess;      /* Stat: ReadFilter thread, total load library success counter */
  unsigned long volatile statRfTotalLoadLibError;        /* Stat: ReadFilter thread, total load library error counter */
  unsigned long volatile statRfTotalInitLibSuccess;      /* Stat: ReadFilter thread, total init library success counter */
  unsigned long volatile statRfTotalInitLibError;        /* Stat: ReadFilter thread, total init library error counter */
  unsigned long volatile statRfTotalCreateErrHSuccess;   /* Stat: ReadFilter thread, total create_error_handle success counter */
  unsigned long volatile statRfTotalCreateErrHError;     /* Stat: ReadFilter thread, total create_error_handle error counter */
  unsigned long volatile statRfTotalNewClientSuccess;    /* Stat: ReadFilter thread, total new_client success counter */
  unsigned long volatile statRfTotalNewClientError;      /* Stat: ReadFilter thread, total new_client error counter */
  unsigned long volatile statRfTotalOpenConSuccess;      /* Stat: ReadFilter thread, total open_connection success counter */
  unsigned long volatile statRfTotalOpenConError;        /* Stat: ReadFilter thread, total open_connection error counter */
  unsigned long volatile statRfTotalSendReqSuccess;      /* Stat: ReadFilter thread, total send_request success counter */
  unsigned long volatile statRfTotalSendReqError;        /* Stat: ReadFilter thread, total send_request error counter */
  unsigned long volatile statRfTotalGetNumRecSuccess;    /* Stat: ReadFilter thread, total get_num_rec success counter */
  unsigned long volatile statRfTotalGetNumRecError;      /* Stat: ReadFilter thread, total get_num_rec error counter */
  unsigned long volatile statRfTotalGetNumColSuccess;    /* Stat: ReadFilter thread, total get_num_col success counter */
  unsigned long volatile statRfTotalGetNumColError;      /* Stat: ReadFilter thread, total get_num_col error counter */
  unsigned long volatile statRfTotalProcessReqSuccess;   /* Stat: ReadFilter thread, total process_req success counter */
  unsigned long volatile statRfTotalProcessReqError;     /* Stat: ReadFilter thread, total process_req error counter */
  unsigned long volatile statRfTotalWhiteListOverload;   /* Stat: ReadFilter thread, total white_event_list overload counter */
  unsigned long volatile statRfTotalDarkListOverload;    /* Stat: ReadFilter thread, total dark_event_list everload counter */
  /* ----------------------------------------------------------------------- */
  /* ProcessEvent message thread statistics                                  */
  unsigned long volatile statPeTotalPickUpMessage;       /* Stat: ProcessEvent thread, total pick_up_message counter */
  unsigned long volatile statPeTotalUnPickUpMessage;     /* Stat: ProcessEvent thread, total un_pick_up_message counter */
  unsigned long volatile statPeTotalStartProcessMessage; /* Stat: ProcessEvent thread, total start_process_message counter */
  unsigned long volatile statPeTotalProcMsgToOutput;     /* Stat: ProcessEvent thread, total processed_to_output counter */
  unsigned long volatile statPeTotalProcMsgToDrop;       /* Stat: ProcessEvent thread, total message_drop counter */
  unsigned long volatile statPeTotalProcCopyMsgError;    /* Stat: ProcessEvent thread, total copy_event_message_error counter */
  unsigned long volatile statPeTotalProcReptMsgStart;    /* Stat: ProcessEvent thread, total start_process_rept_msg counter */
  unsigned long volatile statPeTotalProcReptMsgSuccess;  /* Stat: ProcessEvent thread, total rept_messages_success counter */
  unsigned long volatile statPeTotalProcReptMsgError;    /* Stat: ProcessEvent thread, total rept_messages_error counter */
  unsigned long volatile statPeTotalProcInvLeadSymbol;   /* Stat: ProcessEvent thread, total invalid_lead_symbol counter */
  unsigned long volatile statPeTotalProcPriorDecimal;    /* Stat: ProcessEvent thread, total priority_type_decimal counter */
  unsigned long volatile statPeTotalProcPriorHex;        /* Stat: ProcessEvent thread, total priority_type_hex counter */
  unsigned long volatile statPeTotalProcPriorError;      /* Stat: ProcessEvent thread, total priority_process_error counter */
  unsigned long volatile statPeTotalProcInvEndSymbol;    /* Stat: ProcessEvent thread, total invalid_end_symbol counter */
  unsigned long volatile statPeTotalProcNormMsgStart;    /* Stat: ProcessEvent thread, total start_process_normal_msg counter */
  unsigned long volatile statPeTotalInvDelimInNormMsg;   /* Stat: ProcessEvent thread, total invalid_delimitter_in_normal_message counter */
  unsigned long volatile statPeTotalInvSSSTagInMsg;      /* Stat: ProcessEvent thread, total invalid_sss_tag_in_message */
  unsigned long volatile statPeTotalProcTypeDecimal;     /* Stat: ProcessEvent thread, total process_type_decimal counter */
  unsigned long volatile statPeTotalProcTypeHex;         /* Stat: ProcessEvent thread, total process_type_hex counter */
  unsigned long volatile statPeTotalProcTypeError;       /* Stat: ProcessEvent thread, total process_type_error counter */
  unsigned long volatile statPeTotalProcCloseTyError;    /* Stat: ProcessEvent thread, total invalid_close_type_section counter */
  unsigned long volatile statPeTotalPFiltInDarkList;     /* Stat: ProcessEvent thread, total process_filter_in_dark_list counter */
  unsigned long volatile statPeTotalPFiltInWhiteList;    /* Stat: ProcessEvent thread, total process_filter_in_white_list counter */
  unsigned long volatile statPeTotalClassNotAssigned;    /* Stat: ProcessEvent thread, total class_value_not_assigned counter */
  unsigned long volatile statPeTotalProcTransparent;     /* Stat: ProcessEvent thread, total transparent_messages counter */
  /* ----------------------------------------------------------------------- */
  /* Output thread statistics                                                */
  int volatile statOuCurrentTryUseLibraryTimeout;        /* Stat: Output thread, current try_use_library timeout (sec.) */
  unsigned long volatile statOuTotalPickUpMessage;       /* Stat: Output thread, total pick_up_message counter */
  unsigned long volatile statOuTotalUnPickUpMessage;     /* Stat: Output thread, total un_pick_up_message counter */
  unsigned long volatile statOuTotalCopyMessageError;    /* Stat: Output thread, total copy_message_to_flat_buf_error counter */
  unsigned long volatile statOuTotalRepLastMessageError; /* Stat: Output thread, total rept_last_message_error counter */
  unsigned long volatile statOuTotalOutputLibProcSuccess;/* Stat: Output thread, total output_library_process_success counter */
  unsigned long volatile statOuTotalOutputLibCommonError;/* Stat: Output thread, total output_library_common_error counter */
  unsigned long volatile statOuTotalOutputLibFatalError; /* Stat: Output thread, total output_library_fatal_error counter */
  unsigned long volatile statOuTotalOutputDropMessage;   /* Stat: Output thread, total output_drop_message counter */
  unsigned long volatile statOuTotalOpenSocketError;     /* Stat: Output thread, total open_socket_error counter */
  unsigned long volatile statOuTotalBindSocketError;     /* Stat: Output thread, total bind_socket_error counter */
  unsigned long volatile statOuTotalSendError;           /* Stat: Output thread, total send_event_data_error counter */
  unsigned long volatile statOuTotalTrySend;             /* Stat: Output thread, total try_send counter */
  unsigned long volatile statOuTotalSelectError;         /* Stat: Output thread, total select_error counter */
  unsigned long volatile statOuTotalSelectTimeout;       /* Stat: Output thread, total select_timeout counter */
  unsigned long volatile statOuTotalSelectIntr;          /* Stat: Output thread, total select_interrupted counter */
  unsigned long volatile statOuTotalReceiveError;        /* Stat: Output thread, total receive_error counter */
} EMSTATINFO;
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
/* ------------------------------------------------------------------------- */
/* Linker structure                                                          */
typedef struct s_linker {
    unsigned long tag;               /* structure tag */
    unsigned long lastmessage_flg:1, /* last message flag */
                  prevdroped_flg:1,  /* droped previous flag */
                  ofbd_flg:1,        /* out of band data flag */
                  transparent:1;     /* transparent message flag (skip filtering) */
    void     *next;                  /* next pointer */
    int      eventclass;             /* event class */
    int      eventtype;              /* event type */
    int      eventpri;               /* priority */
    int      reptcnt;                /* rept counter */
    int      bufidx;                 /* real message start index */
    int      beghostidx;             /* hostname begin index */
    int      hostnamelen;            /* hostname size (without last zero) */
    time_t   timestamp;              /* event time stamp */
} LINKER;
/* ------------------------------------------------------------------------- */
/* EventMon Daemon message structure (buffered)                              */
typedef struct s_messagebuffer {
    LINKER linker;                   /* linker */
    int  bufsize;                    /* buffer size */
    char buf[EMSGBUF_SIZE+1];        /* buffer */
} EMSGB;
/* ------------------------------------------------------------------------- */
/* EventMon Daemon message structure (file storage)                          */
typedef struct s_messagefile {
    LINKER linker;                   /* linker */
    int  bufsize;                    /* buffer size */
    int  filenumber;                 /* file number */
    char fname[EMSGFNAME_MAXSIZE+1]; /* file name */
} EMSGF;
/* ------------------------------------------------------------------------- */
typedef struct s_efilterinfo {
    struct s_efilterinfo *next;      /* next pointer */
    int event_class;                 /* event class, 0 - not check, (-1) - all values */
    int event_type;                  /* event type, 0 - not check, (-1) - all types */
    int prior_facil;                 /* priority and facility, 0 - ignore, (-1) - all values */
} EFINFO;
/* ------------------------------------------------------------------------- */
#define EMSGB_TAG    (0x1965)        /* buffered message structure (EMSGB) tag */
#define EMSGF_TAG    (0x1992)        /* file message structure (EMSGF) tag */
/* ------------------------------------------------------------------------- */
/* Some definitions for easy access to linker members                        */
#define l_tag             linker.tag
#define l_next            linker.next
#define l_lastmessage_flg linker.lastmessage_flg
#define l_prevdroped_flg  linker.prevdroped_flg
#define l_ofbd_flg        linker.ofbd_flg
#define l_transparent     linker.transparent
#define l_event_class     linker.eventclass
#define l_event_type      linker.eventtype
#define l_pri             linker.eventpri
#define l_reptc           linker.reptcnt
#define l_timestamp       linker.timestamp
/* ------------------------------------------------------------------------- */
/* EventMon Daemon's syslog messages seq.numbers                             */
#define EVMSEQN_STARTED          0x00200110l  /* Seq.Number for "EventMon Started" syslog message */
#define EVMSEQN_STOPED           0x00200111l  /* Seq.Number for "EventMon Stoped" syslog message */
#define EVMSEQN_ILLEGALCPUCMD    0x00200112l  /* Seq.Number for "Invalid CPU command" syslog message */
#define EVMSEQN_ILLEGALFPECMD    0x00200113l  /* Seq.Number for "Invalid FPE command" syslog message */
#define EVMSEQN_INITMUTEXERR     0x00200114l  /* Seq.Number for "Can't init mutex \"%s\"" syslog message */
#define EVMSEQN_INITTHREADERR    0x00200115l  /* Seq.Number for "Init thread error" syslog message */
#define EVMSEQN_NOINPBUFFERS     0x00200116l  /* Seq.Number for "There are no input buffers, total dropped %lu input messages" syslog message */
#define EVMSEQN_CANTFINDCONFSTR  0x00200117l  /* Seq.Number for "Can't find string \"@@%s\" in \"%s\" file" syslog message */
#define EVMSEQN_TOOMANYCONFSTR   0x00200118l  /* Seq.Number for "Too many strings \"@@%s\" in \"%s\" file" syslog message */
#define EVMSEQN_DBISEMPTY        0x00200119l  /* Seq.Number for "Database \"%s\", table \"%s\" is empty (0 rows in set)" syslog message */
#define EVMSEQN_INITCONDVARERR   0x0020011Al  /* Seq.Number for "Can't init condition var \"%s\"" syslog message */
#define EVMSEQN_OUTPFERROR       0x0020011Bl  /* Seq.Number for "Can't use output API (total fatal output API error: %lu)" syslog message */
#define EVMSEQN_OUTPSERROR       0x0020011Cl  /* Seq.Number for "Can't use output API (non fatal error), current timeout is %d sec." syslog message */
#define EVMSEQN_CANTOPENTICKER   0x0020011Dl  /* Seq.Number for "Can't open amticker timestamp file" syslog message */
#define EVMSEQN_SSDBINITERROR    0x0020011El  /* Seq.Number for "Can't init SSDB library \"%s\", current timeout is %d sec." syslog message */
#define EVMSEQN_SSDBLOADERROR    0x0020011Fl  /* Seq.Number for "Can't load SSDB library \"%s\", current timeout is %d sec." syslog message */
#define EVMSEQN_CANTWRITETICKER  0x00200134l  /* Seq.Number for "Can't write amticker timestamp file" syslog message */
#define EVMSEQN_CANTFINDAMDIAG   0x00200135l  /* Seq.Number for "Can't find amdiag file" syslog message */
/* ------------------------------------------------------------------------- */
/* EventMon Daemon's return code                                             */
#define EMERR_SUCCESS            0            /* Success (Daemon started) */
#define EMERR_INVPARAM           1            /* Invalid parameters or only "help" output */
#define EMERR_SYSERRFORK         2            /* System error: fork() */
#define EMERR_INVOSPARAMS        3            /* Incorrect system parameter(s), can't start daemon */
#define EMERR_DIEDSIGNAL         4            /* Killed by some system signal */
#define EMERR_INITDATA           5            /* Init data error */
#define EMERR_INITTHREAD         6            /* Init thread error */
#define EMERR_CANTDAEMONIZE      7            /* Can't _daemonize */
#define EMERR_ALRINSTALLED       8            /* Already installed */
#define EMERR_CANTMAKETEMP       9            /* Can't make temp file name */
#define EMERR_CANTOPENSOCK       10           /* Can't open socket */
#define EMERR_CANTBINDSOCK       11           /* Can't bind socket */
#define EMERR_NOTINSTALLED       12           /* Daemon not installed */
#define EMERR_INVACCOUNT         13           /* Can't start daemon with non root account */
#define EMERR_INVCPUCMD          14           /* Invalid CPU command */
#define EMERR_INVFPECMD          15           /* Invalid FPE command */
#define EMERR_CANTSSOCKOPT       16           /* Can't set socket option(s) */
#ifdef INCDEBUG
#define dprintf if(debug_mode) printf
#endif
/* ------------------------------------------------------------------------- */
/* Static strings                                                            */
static const char szDelimitterLine[]       = "---------------------------------------------------------------";
static const char szCopyright[]            = "Silicon Graphics Inc. (C) 1992-1999";
static const char szDaemonName[]           = "eventmond";
static const char szTempFileTemplate[]     = "/tmp/.eventmond.%d.tmp";       /* Template (and location) for temp files */
static const char szEventMonInfoSockPath[] = "/tmp/.eventmond.info.sock";    /* EventMon Info Socket name */
static const char szTempSockTemplate[]     = "/tmp/.eventmond.tmpsock.%lX";  /* EventMon temp socket template */
static const char szTempSockNameMask[]     = ".eventmond.tmpsock.";          /* EventMon temp socket name mask */
static const char szSEMSockNameTemp[]      = "/tmp/.semapievm.tmp";          /* SEM API bind socket name */
static const char szSEMInpSockName[]       = "/tmp/.sehdsm.events.sock";     /* SEM API Events Socket name */
static const char szUnknownTime[]          = "Unknown Start Time and Date";  /* Unknown time - info message message */
static const char szDebug[]                = "debug";                        /* "debug" string */
static const char szNormal[]               = "normal";                       /* "normal" string */
static const char szLastMessageRepeated[]  = "last message repeated";        /* message string from syslog daemon (prefix) */
static const char szLastMsgRetTimes[]      = "times";                        /* message string from syslog daemon (postfix) */
static const char szNONE[]                 = "<NONE>";                       /* <NONE> message string */
static const char szStarted[]              = "started";                      /* message string for "started" syslog notification */
static const char szStopped[]              = "stopped";                      /* message string for "stoped" syslog notification */
static const char szOn[]                   = "on";                           /* "on" message string */
static const char szOff[]                  = "off";                          /* "off" message string */
static const char szSSDBLibraryName[]      = "libssdb.so";                   /* default: SSDB DSO module name */
static const char szDatabaseName[]         = "ssdb";                         /* default: SSS Database name */
static const char szDbTableName[]          = "event_type";                   /* default: SSS/Eventmon table name */
static const char szSyslogConfFname[]      = SYSLOGCONF_FNAME;               /* default: syslog daemon config file */
static const char szLocalHostString[]      = "localhost";                    /* Local Host name string */
static const char szAmtickerFile[]         = EVM_AMTICKER_FNAME;             /* Amticker timestamp file name */
static const char szAmdiagFullPath[]       = EVM_AMDIAG_PATH;                /* Amdiag executable path */

static const char szErrorDaemonNotInstalled3[] = "%s - Error: %d (%d) - Daemon not installed\n";
static const char szErrorCantOpenSocket3[]     = "%s - Error: %d (%d) - Can't open socket\n";
static const char szErrorAlreadyInstalled2[]   = "%s - Error: %d - Already installed\n";
static const char szErrorCantBindSocket3[]     = "%s - Error: %d (%d) - Can't bind socket\n";
static const char szErrorCantSetSockOpt3[]     = "%s - Error: %d (%d) - Can't set socket options\n";
#ifdef RLIMIT_PTHREAD
static const char szErrorIncorrectOSRes2[]     = "%s - Error: %d - Incorrect OS resources limit\n";
#endif
static const char szErrorCantLoadInNonRoot[]   = "%s - Error: %d - Can't load daemon in \"non root\" mode\n";
static const char szErrorCantMakeTempFile2[]   = "%s - Error: %d - Can't make temp file name\n";
#ifdef INCDEBUG
static const char szErrorCantOpenOutputFile4[] = "%s - Error: %d (%d) - Can't open output file - \"%s\"\n";
#endif
static const char szErrorSystemCantFork3[]     = "%s - Error: %d (%d) - System Can't fork()\n";
static const char szErrorInvalidParameter3[]   = "%s - Error: %d - Invalid parameter(s) - \"%s\"\n";
static const char szWarningCantLoadDbLib2[]    = "%s - Warning - Can't load \"database support\" library - \"%s\"\n";
static const char szWarningCantFindConfRec3[]  = "%s - Warning - Can't find string \"@@%s\" in \"%s\" file\n";
static const char szWarningTooManyConfRec3[]   = "%s - Warning - Too many strings \"@@%s\" in \"%s\" file\n";
static const char szIllegalCPUcommand[]        = "Invalid CPU command";
static const char szIllegalFPEcommand[]        = "Invalid FPE command";
static const char szSSDBLibraryLoadError[]     = "Can't load SSDB library \"%s\", current timeout is %d sec.";
static const char szSSDBLibraryInitError[]     = "Can't init SSDB library \"%s\", current timeout is %d sec.";
static const char szOutputLibraryUseError[]    = "Can't use output API (non fatal error), current timeout is %d sec.";
static const char szOutputLibraryUseFError[]   = "Can't use output API (total fatal output API error: %lu)";
static const char szCantInitMutex[]            = "Can't init mutex \"%s\"";
static const char szCantInitConVar[]           = "Can't init condition var \"%s\"";
static const char szCantInitThread[]           = "Can't init \"%s\" thread";
static const char szDatabaseIsEmpty[]          = "Database \"%s\", table \"%s\" is empty (0 rows in set)";
static const char szThereAreNoInpBuffers[]     = "There are no input buffers, total dropped %lu input messages";
static const char szCantFindConfRec2[]         = "Can't find string \"@@%s\" in \"%s\" file";
static const char szTooManyConfRec2[]          = "Too many strings \"@@%s\" in \"%s\" file";
static const char szCantOpenAmtickerFile[]     = "Can't open time stamp file \"%s\" - error counter: %lu";
static const char szCantWriteAmtickerFile[]    = "Can't write time stamp file \"%s\" - error counter: %lu";
static const char szCantFindAmdiagBinary[]     = "Can't find \"%s\" file - error counter: %lu";
/* ------------------------------------------------------------------------- */
static int volatile prev_instance               = 0;                /* Previous instance flag */
static int volatile privModeFlag                = 0;                /* Priviledge mode */
static int volatile debug_mode                  = 0;                /* Debug mode flag */
static int volatile silenceMode                 = 0;                /* Silence mode flag */
static int volatile enable_ssstag               = 1;                /* Enable SSS tag in syslog messages (flag) */
static int volatile enable_group_mode           = 0;                /* "Group mode" flag - enable/disable transparent event forwarding for remote hosts */
static int volatile cTimeoutBeforeLoadConfig    = TOUTBASE_LOADCONF;/* Timeout before load configuration from database */
static int volatile enable_amticker             = 0;                /* Enable Amticker functionality */
static int volatile amticker_timeout            = TOUTBASE_AMINTER; /* Default Amticker timeout for refresh time stamp file (sec.) */
static int volatile amticker_statustime         = TOUTBASE_AMSTATUS;/* Default Amticker timeout for exec amdiag binary (hours) */
static unsigned long volatile clearTmpFileCnt   = 0;                /* Clear temp file counter */
static unsigned long volatile exit_phase        = 0;                /* Exit phase */
static unsigned long volatile inited_mutex      = 0;                /* Inited Flag: Mutex */
static unsigned long volatile inited_cond       = 0;                /* Inited Flag: Condition variables */
static unsigned long volatile need_init_filter  = 0;                /* Need initialize filter info */
static unsigned long volatile semapi_clockSize  = 0;                /* System clock size */
static EMSGB *free_emsgb_list                   = 0;                /* Free EventMon Buffered Messages List */
static EMSGF *free_emsgf_list                   = 0;                /* Free EventMon TempFile Messages List */
static LINKER *rcvEmsgList                      = 0;                /* Rcv EventMon Messages List */
static LINKER *outEmsgList                      = 0;                /* Output EventMon Messages List */
static EFINFO *free_efinfo_list                 = 0;                /* Event Filter info structures free list */
static EFINFO *dark_efinfo_list                 = 0;                /* Dark Event Filter info list */
static EFINFO *whit_efinfo_list                 = 0;                /* White Event Filter info list */
static unsigned long volatile rcvMsgListCounter = 0;                /* Rcv messages list counter */
static unsigned long volatile outMsgListCounter = 0;                /* Output messages list counter */
static int volatile dark_efinfo_cnt             = 0;                /* Dark Event Filter info counter */
static int volatile whit_efinfo_cnt             = 0;                /* White Event Filter info counter */
static int volatile emsgAllocCnt                = 0;                /* EMSGB alloc counter */
static int volatile emsgFreeCnt                 = 0;                /* EMSGB free counter */
static int maxMemBufCnt                         = EMSGALLOCMAX_CNT; /* Max memory buffers number */
static int preallocMemBufCnt                    = EMSGPREALLOC_CNT; /* PreAlloc memory buffer counter */
static int maxTmpFileCnt                        = EMSGFILE_MAXCNT;  /* Max Temp file number */
static int preallocTmpFileCnt                   = EMSGFPREALLOC_CNT;/* PreAlloc temp file counter */
static int volatile inUsageTempFileName         = 0;                /* In Usage Temp file counter */
static int lastMessageInfoStrSize               = 0;                /* String size of szLastMessageRepeated */
static int cTryUseOutputApiMax                  = TRYOUTPUT_REPCNT; /* Max repeate counter for "non fatal errors" in output thread */
static void *ssdb_dlhandle                      = NULL;             /* SSDB library handle */
static int volatile filterInfoIsReady           = 0;                /* Filter info ready flag */
static int volatile filterInfoAllocCnt          = 0;                /* Filter info allocation counter */
static int volatile filterInfoFreeCnt           = 0;                /* Filter info free (list) counter */
static char *lpszEventLogFilename               = NULL;             /* EventLog output file name */
static char *lpszSSDBLibraryName                = NULL;             /* pointer to SSDB DSO module name string */
static char *lpszDatabaseName                   = NULL;             /* pointer to SSS Database name string */
static char *lpszDbTableName                    = NULL;             /* pointer to SSS/Eventmon table name string */
static char *lpszAmtickerFile                   = NULL;             /* Amticker timestamp file */
static char freeTempFileName[EMSGFILE_MAXCNT+1];                    /* Temp file names table */
static char szFixStartTime[64];                                     /* Fix Start time */
static char szInpSockName[INPSOCKNAME_MAXSIZE+1];                   /* Input socket name */
static EMSTATINFO est;                                              /* Statistics storage struct */
static char staticBuf[RCVPACK_MAXSIZE+1];                           /* Static buffer for output thread */
static char szLocalHostName[EVM_MAXHOSTNAMESIZE+1];                 /* Local host name buffer */
/* ------------------------------------------------------------------------- */
/* SSDB dso Function pointers                                                */
static FPssdbInit               *fp_ssdbInit               = NULL;
static FPssdbDone               *fp_ssdbDone               = NULL;
static FPssdbCreateErrorHandle  *fp_ssdbCreateErrorHandle  = NULL;
static FPssdbDeleteErrorHandle  *fp_ssdbDeleteErrorHandle  = NULL;
static FPssdbNewClient          *fp_ssdbNewClient          = NULL;
static FPssdbDeleteClient       *fp_ssdbDeleteClient       = NULL;
static FPssdbOpenConnection     *fp_ssdbOpenConnection     = NULL;
static FPssdbCloseConnection    *fp_ssdbCloseConnection    = NULL;
/*static FPssdbSendRequest        *fp_ssdbSendRequest        = NULL; */
static FPssdbSendRequestTrans   *fp_ssdbSendRequestTrans   = NULL;
static FPssdbFreeRequest        *fp_ssdbFreeRequest        = NULL;
static FPssdbGetNumRecords      *fp_ssdbGetNumRecords      = NULL;
static FPssdbGetNumColumns      *fp_ssdbGetNumColumns      = NULL;
static FPssdbGetNextField       *fp_ssdbGetNextField       = NULL;
static FPssdbGetLastErrorCode   *fp_ssdbGetLastErrorCode   = NULL;
#ifdef INCDEBUG
static FPssdbGetLastErrorString *fp_ssdbGetLastErrorString = NULL;
#endif
/* ------------------------------------------------------------------------- */
static pthread_t rsock_tid;           /* ReadSocket thread id */
static pthread_t killer_tid;          /* Killer thread id */
static pthread_t procf_tid;           /* ProcFilter thread id */
static pthread_t rfilter_tid;         /* ReadFilter thread id */
static pthread_t info_tid;            /* Info thread id */
static pthread_t output_tid;          /* Output results thread id */
static pthread_t amticker_tid;        /* Amticker thread id */
static DMUTEX start_rsock_mutex;      /* Synchro Start Mutex for ReadSocket thread */
static DMUTEX start_killer_mutex;     /* Synchro Start Mutex for Killer thread */
static DMUTEX start_procfilter_mutex; /* Synchro Start Mutex for ProcFilter thread */
static DMUTEX start_readfilter_mutex; /* Synchro Start Mutex for ReadFilter thread */
static DMUTEX start_info_mutex;       /* Synchro Start Mutex for Info thread */
static DMUTEX start_output_mutex;     /* Synchro Start Mutex for Output thread */
static DMUTEX start_amticker_mutex;   /* Synchro Start Mutex for Amticker thread */
static DMUTEX rcvbuf_queue_mutex;     /* Rcv queue mutex */
static DMUTEX output_queue_mutex;     /* Output queue mutex */
static DMUTEX emsgbuf_list_mutex;     /* EMSG lists mutex */
static DMUTEX efilter_list_mutex;     /* Event Filter lists mutex */
static DMUTEX exit_phase_mutex;       /* Exit Phase mutex  for unload */
static DMUTEX reinit_filter_mutex;    /* Reinit Filter Info mutex */
static DMUTEX amticker_mutex;         /* Amticker data/flags access mutex */
static DCOND rcvnotempt_condition;    /* Rcv queue "not empty" condition variable */
static DCOND outnotempt_condition;    /* Output queue "not empty" condition variable */
static DCOND exit_condition;          /* "Exit" condition variable */
static DCOND reinit_condition;        /* "Reinit" filter info condition variable */
static DCOND amticker_condition;      /* "Amticker" thread activate condition variable */

/* ========================================================================= */
/*                          EVENTMON DAEMON CODE                             */
/* ========================================================================= */
/* -------------------- zero_ssdbFuncPointers ------------------------------ */
static void zero_ssdbFuncPointers(void)
{ fp_ssdbInit               = NULL;
  fp_ssdbDone               = NULL;
  fp_ssdbCreateErrorHandle  = NULL;
  fp_ssdbDeleteErrorHandle  = NULL;
  fp_ssdbNewClient          = NULL;
  fp_ssdbDeleteClient       = NULL;
  fp_ssdbOpenConnection     = NULL;
  fp_ssdbCloseConnection    = NULL;
/*  fp_ssdbSendRequest        = NULL; */
  fp_ssdbSendRequestTrans   = NULL;
  fp_ssdbFreeRequest        = NULL;
  fp_ssdbGetNumRecords      = NULL;
  fp_ssdbGetNumColumns      = NULL;
  fp_ssdbGetNextField       = NULL;
  fp_ssdbGetLastErrorCode   = NULL;
#ifdef INCDEBUG
  fp_ssdbGetLastErrorString = NULL;
#endif
}
/* ----------------------- zinit_resources --------------------------------- */
static void zinit_resources(void)
{ time_t t; 
  char *c;
  maxMemBufCnt         = EMSGALLOCMAX_CNT;
  preallocMemBufCnt    = EMSGPREALLOC_CNT;
  maxTmpFileCnt        = EMSGFILE_MAXCNT;
  preallocTmpFileCnt   = EMSGFPREALLOC_CNT;
  lpszSSDBLibraryName  = (char*)szSSDBLibraryName;
  lpszDatabaseName     = (char*)szDatabaseName;
  lpszDbTableName      = (char*)szDbTableName;
  lpszAmtickerFile     = (char *)szAmtickerFile;
  szFixStartTime[0]    = 0;
  cTryUseOutputApiMax  = TRYOUTPUT_REPCNT;
  memset(freeTempFileName,0,EMSGFILE_MAXCNT);
  memset(szInpSockName,0,sizeof(szInpSockName));
  memset(szLocalHostName,0,sizeof(szLocalHostName));
  memset(&start_rsock_mutex,0,sizeof(DMUTEX));
  memset(&start_killer_mutex,0,sizeof(DMUTEX));
  memset(&start_procfilter_mutex,0,sizeof(DMUTEX));
  memset(&start_readfilter_mutex,0,sizeof(DMUTEX));
  memset(&start_info_mutex,0,sizeof(DMUTEX));
  memset(&start_output_mutex,0,sizeof(DMUTEX));
  memset(&start_amticker_mutex,0,sizeof(DMUTEX));
  memset(&rcvbuf_queue_mutex,0,sizeof(DMUTEX));
  memset(&output_queue_mutex,0,sizeof(DMUTEX));
  memset(&emsgbuf_list_mutex,0,sizeof(DMUTEX));
  memset(&efilter_list_mutex,0,sizeof(DMUTEX));
  memset(&exit_phase_mutex,0,sizeof(DMUTEX));
  memset(&reinit_filter_mutex,0,sizeof(DMUTEX));
  memset(&amticker_mutex,0,sizeof(DMUTEX));
  memset(&rcvnotempt_condition,0,sizeof(DCOND));
  memset(&outnotempt_condition,0,sizeof(DCOND));
  memset(&exit_condition,0,sizeof(DCOND));
  memset(&reinit_condition,0,sizeof(DCOND));
  memset(&amticker_condition,0,sizeof(DCOND));
  memset(&est,0,sizeof(EMSTATINFO));
  zero_ssdbFuncPointers();
  if((t = time(NULL)) != (time_t)(-1))
  { strncpy(szFixStartTime,asctime(localtime(&t)),sizeof(szFixStartTime)-1);  /* Fix start time */
    for(c = szFixStartTime;*c && *c != '\n' && *c != '\r';c++);
    *c = 0;
  }
  else strcpy(szFixStartTime,szUnknownTime);
  strcpy(szInpSockName,EVENTMONSOCK_PATH);
  privModeFlag = (!getuid() && !geteuid()) ? 1 : 0;
  lastMessageInfoStrSize = strlen(szLastMessageRepeated);
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
static int prepare_cmdbuffer(char *buf,char cmd)
{ buf[0] = 'L'; buf[1] = 'V'; buf[2] = 'A'; buf[3] = cmd; 
  return 4; /* cmd buffer size */
}
/* ------------------------- close_socket ---------------------------------- */
static void close_socket(int s)
{ for(;;) if(!close(s) || (errno != EINTR)) break;
}
/* ------------------------- unlink_file ----------------------------------- */
static void unlink_file(const char *socket_path)
{ if(socket_path) for(;;) if(!unlink(socket_path) || (errno != EINTR)) break;
}
/* ------------------------- close_unlink ---------------------------------- */
static void close_unlink(int *s,const char *socket_path)
{ close_socket(*s); *s = INVALID_SOCKET_VAL;
  unlink_file(socket_path);
}
/* ------------------------- write_file ------------------------------------ */
static int write_file(int fd,char *buffer,int len)
{ int retcode;
  for(;;) if(((retcode = write(fd,buffer,len)) >= 0) || (errno != EINTR)) break;
  return retcode;
}
/* ------------------------- semapi_sleep ---------------------------------- */
static int semapi_sleep(unsigned long duration)
{ if(!semapi_clockSize) sleep(1);
  else sginap((long)(semapi_clockSize*duration));
  return 1;
}
/* ------------------------ newEventInfo ----------------------------------- */
static EFINFO *newEventInfo(int _e_class,int _e_type,int _e_prior)
{ EFINFO *ef;
  pthread_mutex_lock(&efilter_list_mutex.mutex);
  if((ef = free_efinfo_list) != 0) { free_efinfo_list = ef->next; filterInfoFreeCnt--; }
  pthread_mutex_unlock(&efilter_list_mutex.mutex);
  if(!ef) { if((ef = (EFINFO*)malloc(sizeof(EFINFO))) != 0) filterInfoAllocCnt++; }
  if(ef)
  { memset(ef,0,sizeof(EFINFO));
    ef->event_class = _e_class;
    ef->event_type  = _e_type;
    ef->prior_facil = _e_prior;
  }
  return ef;
}
/* ------------------------ freeEventInfo ---------------------------------- */
static void freeEventInfo(EFINFO *ef)
{ if(ef)
  { pthread_mutex_lock(&efilter_list_mutex.mutex);
    ef->next = free_efinfo_list; free_efinfo_list = ef; filterInfoFreeCnt++;
    pthread_mutex_unlock(&efilter_list_mutex.mutex);
  }
}
/* ---------------------- freeEventInfoList -------------------------------- */
static EFINFO *freeEventInfoList(EFINFO *ef)
{ EFINFO *ep;
  while((ep = ef) != 0) { ef = ep->next; freeEventInfo(ep); }
  return 0;
}
/* ---------------------- allocEventInfoList ------------------------------- */
static EFINFO *allocEventInfoList(int cnt)
{ int i;
  EFINFO **epp,*ef = 0; 
  if(cnt > 0)
  { for(epp = &ef,i = 0;i < cnt;i++,epp = &((*epp)->next))
    { if((*epp = newEventInfo(0,0,0)) == 0) { ef = freeEventInfoList(ef); break; }
    }
  }
  return ef;
}
/* -------------------- putEventInfoIntoDarkList --------------------------- */
static int putEventInfoIntoDarkList(EFINFO *ef)
{ EFINFO *e;
  int retcode = 0;
  if(ef)
  { pthread_mutex_lock(&efilter_list_mutex.mutex);
    for(e = dark_efinfo_list;e;e = e->next)
    { if(e->event_type == ef->event_type && e->event_class == ef->event_class && e->prior_facil == ef->prior_facil) break;
    }
    if(!e) { ef->next = dark_efinfo_list; dark_efinfo_list = ef; dark_efinfo_cnt++;retcode++; }
    pthread_mutex_unlock(&efilter_list_mutex.mutex);
  }
  return retcode;
}
/* -------------------- putEventInfoIntoWhiteList -------------------------- */
static int putEventInfoIntoWhiteList(EFINFO *ef)
{ EFINFO *e;
  int retcode = 0;
  if(ef)
  { pthread_mutex_lock(&efilter_list_mutex.mutex);
    for(e = whit_efinfo_list;e;e = e->next)
    { if(e->event_type == ef->event_type && e->event_class == ef->event_class && e->prior_facil == ef->prior_facil) break;
    }
    if(!e) { ef->next = whit_efinfo_list; whit_efinfo_list = ef; whit_efinfo_cnt++; retcode++; }
    pthread_mutex_unlock(&efilter_list_mutex.mutex);
  }
  return retcode;
}
/* ----------------------- dropDarkEventInfoList --------------------------- */
static void dropDarkEventInfoList(void)
{ EFINFO *ef;
  pthread_mutex_lock(&efilter_list_mutex.mutex);
  ef = dark_efinfo_list; dark_efinfo_list = 0; dark_efinfo_cnt = 0;
  pthread_mutex_unlock(&efilter_list_mutex.mutex);
  freeEventInfoList(ef);
}
/* ---------------------- dropWhiteEventInfoList --------------------------- */
static void dropWhiteEventInfoList(void)
{ EFINFO *ef;
  pthread_mutex_lock(&efilter_list_mutex.mutex);
  ef = whit_efinfo_list; whit_efinfo_list = 0; whit_efinfo_cnt = 0;
  pthread_mutex_unlock(&efilter_list_mutex.mutex);
  freeEventInfoList(ef);
}
/* --------------------- resetFilterInfoIsReadyFlag ------------------------ */
static void resetFilterInfoIsReadyFlag(void)
{ pthread_mutex_lock(&efilter_list_mutex.mutex);
  filterInfoIsReady = 0;
  pthread_mutex_unlock(&efilter_list_mutex.mutex);
}
/* ---------------------- setFilterInfoIsReadyFlag ------------------------- */
static void setFilterInfoIsReadyFlag(void)
{ pthread_mutex_lock(&efilter_list_mutex.mutex);
  filterInfoIsReady = 1;
  pthread_mutex_unlock(&efilter_list_mutex.mutex);
}
/* ---------------------- getFilterInfoIsReadyFlag ------------------------- */
static int getFilterInfoIsReadyFlag(void)
{ int retcode;
  pthread_mutex_lock(&efilter_list_mutex.mutex);
  retcode = filterInfoIsReady;
  pthread_mutex_unlock(&efilter_list_mutex.mutex);
  return retcode;
}
/* ---------------------- getFreeTmpFileNumber ----------------------------- */
static int getFreeTmpFileNumber(void)
{ int i;
  int retcode = -1;
  for(i = 0;i < maxTmpFileCnt;i++)
  { if(!freeTempFileName[i]) { freeTempFileName[(retcode = i)] = 'B'; inUsageTempFileName++; break; }
  }
  return retcode;
}  
/* ----------------------- putFreeTmpFileNumber ---------------------------- */
static void putFreeTmpFileNumber(int number)
{ if(number >= 0 && number < maxTmpFileCnt && number < EMSGFILE_MAXCNT && freeTempFileName[number])
  { freeTempFileName[number] = 0; inUsageTempFileName--;
  }
}
/* ------------------------ removeAllTempFiles ----------------------------- */
static void removeAllTempFiles(void)
{ char fname[EMSGFNAME_MAXSIZE*2];
  int i;
  for(i = 0;i < EMSGFILE_MAXCNT;i++)
  { sprintf(fname,szTempFileTemplate,i);
    if(!unlink(fname)) clearTmpFileCnt++;
  }
}
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
/* --------------------------- newEmsgb ------------------------------------ */
static EMSGB *newEmsgb(void)
{ EMSGB *emsg;
  pthread_mutex_lock(&emsgbuf_list_mutex.mutex);
  if((emsg = free_emsgb_list) != 0) { free_emsgb_list = (EMSGB*)emsg->l_next; emsgFreeCnt--; }
  else
  { if(emsgAllocCnt < maxMemBufCnt)
    { if((emsg = (EMSGB*)malloc(sizeof(EMSGB))) != NULL) emsgAllocCnt++;
    }
  }
  pthread_mutex_unlock(&emsgbuf_list_mutex.mutex);
  if(emsg)
  { memset(emsg,0,sizeof(EMSGB));
    emsg->l_tag = EMSGB_TAG;
    emsg->l_lastmessage_flg = 0;
    emsg->l_prevdroped_flg  = 0;
    emsg->l_ofbd_flg        = 0;
    emsg->l_transparent     = 0;
  }
  return emsg;
}

/* --------------------------- freeEmsgb ----------------------------------- */
static EMSGB *freeEmsgb(EMSGB *emsg)
{ if(emsg && emsg->l_tag == EMSGB_TAG)
  { pthread_mutex_lock(&emsgbuf_list_mutex.mutex);
    emsg->l_next = free_emsgb_list; free_emsgb_list = emsg; emsgFreeCnt++;
    pthread_mutex_unlock(&emsgbuf_list_mutex.mutex);
  }
  return 0;
}

/* ------------------------ freeEmsgList ---------------------------------- */
static EMSGB *freeEmsgList(EMSGB *emsg)
{ EMSGB *ep;
  while((ep = emsg) != 0) { emsg = (EMSGB*)ep->l_next; freeEmsgb(ep); }
  return 0;
}

/* ------------------------- allocEmsgList --------------------------------- */
static EMSGB *allocEmsgList(int emsgcnt)
{ int i;
  EMSGB **epp,*emsg = 0; 
  if(emsgcnt > 0)
  { for(epp = &emsg,i = 0;i < emsgcnt;i++,epp = (EMSGB**)&((*epp)->l_next))
    { if((*epp = newEmsgb()) == 0) { emsg = freeEmsgList(emsg); break; }
    }
  }
  return emsg;
}
/* ------------------------ copyMessageToEmsgList -------------------------- */
static EMSGB *copyMessageToEmsgList(int reqbuffer_size,char *buffer)
{ EMSGB *em,*e;
  int reqbuf_cnt = reqbuffer_size/EMSGBUF_SIZE; /* calculate counter of buffers */
  reqbuf_cnt += ((reqbuffer_size%EMSGBUF_SIZE) ? 1 : 0);
  if((em = allocEmsgList(reqbuf_cnt)) != 0)
  { for(e = em;e;e = (EMSGB*)e->l_next)
    { e->bufsize = (reqbuffer_size > EMSGBUF_SIZE) ? EMSGBUF_SIZE : reqbuffer_size;
      memcpy(e->buf,buffer,e->bufsize); buffer += e->bufsize; reqbuffer_size -= e->bufsize;
      if(reqbuffer_size <= 0) { e->l_lastmessage_flg = 1; break; }
    }
  }
  return em;
}

/* ----------------------- copyMemoryMessageToBuffer ----------------------- */
static int copyMemoryMessageToBuffer(EMSGB *em,char *buf,int maxbufsize)
{ int retcode = 0;
  if(em && buf && maxbufsize)
  { for(;em;em = (EMSGB*)em->l_next)
    { if(em->bufsize > maxbufsize) break;
      memcpy(buf,em->buf,em->bufsize);
      maxbufsize -= em->bufsize;
      buf += em->bufsize;
      retcode += em->bufsize;
    }
    if(em) retcode = 0;
  }
  return retcode;
}

/* --------------------------- newEmsgf ------------------------------------ */
static EMSGF *newEmsgf(void)
{ int fnumber;
  EMSGF *emsg = 0;
  pthread_mutex_lock(&emsgbuf_list_mutex.mutex);
  if((fnumber = getFreeTmpFileNumber()) >= 0)
  { if((emsg = free_emsgf_list) != 0) free_emsgf_list = (EMSGF*)emsg->l_next;
    else emsg = (EMSGF*)malloc(sizeof(EMSGF));
    if(emsg)
    { memset(emsg,0,sizeof(EMSGF));
      emsg->l_tag             = EMSGF_TAG;
      emsg->filenumber        = fnumber;
      emsg->l_lastmessage_flg = 1;
      emsg->l_prevdroped_flg  = 0;
      emsg->l_ofbd_flg        = 0;
      emsg->l_transparent     = 0;
    }
    else putFreeTmpFileNumber(fnumber);
  }
  pthread_mutex_unlock(&emsgbuf_list_mutex.mutex);
  if(emsg)
  { sprintf(emsg->fname,szTempFileTemplate,emsg->filenumber);
    (void)unlink(emsg->fname);
  }
  return emsg;
}

/* --------------------------- freeEmsgf ----------------------------------- */
static EMSGF *freeEmsgf(EMSGF *emsg)
{ if(emsg && emsg->l_tag == EMSGF_TAG)
  { (void)unlink(emsg->fname);
    pthread_mutex_lock(&emsgbuf_list_mutex.mutex);
    putFreeTmpFileNumber(emsg->filenumber);
    emsg->l_next = free_emsgf_list; free_emsgf_list = emsg;
    pthread_mutex_unlock(&emsgbuf_list_mutex.mutex);
  }
  return 0;
}

/* ----------------------- freeEmsgfList ---------------------------------- */
static EMSGF *freeEmsgfList(EMSGF *emsg)
{ EMSGF *ep;
  while((ep = emsg) != 0) { emsg = (EMSGF*)ep->l_next; freeEmsgf(ep); }
  return 0;
}

/* ------------------------ allocEmsgfList --------------------------------- */
static EMSGF *allocEmsgfList(int emsgcnt)
{ int i;
  EMSGF **epp,*emsg = 0; 
  if(emsgcnt > 0)
  { for(epp = &emsg,i = 0;i < emsgcnt;i++,epp = (EMSGF**)&((*epp)->l_next))
    { if((*epp = newEmsgf()) == 0) { emsg = freeEmsgfList(emsg); break; }
    }
  }
  return emsg;
}

/* ------------------------ copyMessageToEmsgFile -------------------------- */
static EMSGF *copyMessageToEmsgFile(int reqbuffer_size,char *buffer)
{ int fd;
  EMSGF *fmsg;
  if((fmsg = newEmsgf()) != 0)
  { if((fd = open(fmsg->fname,O_WRONLY|O_CREAT|O_TRUNC,S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH)) >= 0)
    { fmsg->bufsize = write_file(fd,buffer,reqbuffer_size); (void)close(fd);
      if(fmsg->bufsize != reqbuffer_size) { freeEmsgf(fmsg); fmsg = 0; }
    }
    else { freeEmsgf(fmsg); fmsg = 0; }
  }
  return fmsg;
}

/* ------------------------ copyFileMessageToBuffer ------------------------ */
static int copyFileMessageToBuffer(EMSGF *emsgf,char *buf,int maxbufsize)
{ int fd, retcode = 0;
  if(emsgf && buf && maxbufsize && emsgf->bufsize <= maxbufsize)
  { if((fd = open(emsgf->fname,O_RDONLY)) >= 0)
    { if((maxbufsize = read(fd,buf,emsgf->bufsize)) == emsgf->bufsize) retcode = emsgf->bufsize;
      (void)close(fd);
    }
  }
  return retcode;
}

/* ------------------------- freeEmsgAny ----------------------------------- */
static LINKER *freeEmsgAny(LINKER *link)
{ if(link)
  { if(link->tag == EMSGF_TAG) freeEmsgf((EMSGF*)link);
    else                       freeEmsgList((EMSGB*)link);
  }
  return 0;
}

/* --------------------- copyAnyMessageToBuffer ---------------------------- */
static int copyAnyMessageToBuffer(LINKER *link,char *buf,int maxbufsize)
{ if(link)
  { if(link->tag == EMSGF_TAG) return copyFileMessageToBuffer((EMSGF*)link,buf,maxbufsize);
    else                       return copyMemoryMessageToBuffer((EMSGB*)link,buf,maxbufsize);
  }
  return 0;
}
/* ----------------------------- syslog_msg -------------------------------- */
static int syslog_msg(int pri,unsigned long seqnum,const char *msg,...)
{ char tmp[2048];
  va_list args;
  va_start(args,msg); vsnprintf(tmp,sizeof(tmp)-1,msg,args); va_end(args);
  openlog(szDaemonName,LOG_PID|LOG_CONS,LOG_DAEMON);
  if(enable_ssstag) syslog(pri,"|$(0x%lX)%s - (%s mode)",seqnum,tmp,debug_mode ? szDebug : szNormal);
  else              syslog(pri,"%s - (%s mode)",tmp,debug_mode ? szDebug : szNormal);
  closelog();
  return 1;
}
/* ======================= thread_func_amticker ============================ */
void *thread_func_amticker(void *thread_param)
{ int fd, len, amticker_statustime_save, amticker_timeout_save, fixerr_omsg, fixerr_wmsg;
  char timebuffer[64];
  struct timespec time_val;
  struct stat fs;
  time_t fixstatus_time, temp_time;

  fixerr_omsg = (fixerr_wmsg = 0);
  pthread_mutex_lock(&start_amticker_mutex.mutex); pthread_mutex_unlock(&start_amticker_mutex.mutex);

  fixstatus_time = time(NULL);

  for(;!exit_phase;)
  { /* Wait enable_amticker flag */
    pthread_mutex_lock(&amticker_mutex.mutex);
    while(!exit_phase && !enable_amticker) pthread_cond_wait(&amticker_condition.cond,&amticker_mutex.mutex);

    if(!exit_phase)
    { amticker_statustime_save = amticker_statustime; amticker_timeout_save = amticker_timeout;  temp_time = time(NULL);
      pthread_mutex_unlock(&amticker_mutex.mutex);

      /* Write timestamp */
      if((fd = open(lpszAmtickerFile,O_WRONLY|O_CREAT|O_SYNC|O_TRUNC,S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH)) < 0)
      { est.statAmTotalOpenFileError++;
        if(!fixerr_omsg)
	 fixerr_omsg = syslog_msg(LOG_ERR,EVMSEQN_CANTOPENTICKER,szCantOpenAmtickerFile,lpszAmtickerFile,est.statAmTotalOpenFileError);
      }
      else
      { if(write_file(fd,timebuffer,(len = snprintf(timebuffer,sizeof(timebuffer),"%d\n",(int)temp_time))) != len)
	{ est.statAmTotalWriteFileError++;
	  if(!fixerr_wmsg)
	   fixerr_wmsg = syslog_msg(LOG_ERR,EVMSEQN_CANTWRITETICKER,szCantWriteAmtickerFile,lpszAmtickerFile,est.statAmTotalWriteFileError);
        }
	else
	{ fixerr_omsg = (fixerr_wmsg = 0);
	  est.statAmTotalFileUpdated++;
	}
        close_socket(fd);
      }
      
      /* Calculate time before call "amdiag status" */
      if(amticker_statustime_save)
      { if(difftime(temp_time,fixstatus_time) >= ((double)amticker_statustime_save * (double)3600.0))
	{ est.statAmTotalStatusTimeExp++; fixstatus_time = time(NULL);
	  if(!stat(szAmdiagFullPath,&fs) && S_ISREG(fs.st_mode))
	  { system(EVM_AMDIAG_CMDLINE); est.statAmTotalAmdiagExec++;
	  }
	  else
	  { est.statAmTotalAmdiagNotFound++;
	    (void)syslog_msg(LOG_ERR,EVMSEQN_CANTFINDAMDIAG,szCantFindAmdiagBinary,szAmdiagFullPath,est.statAmTotalAmdiagNotFound);
          }
	}
      }
      
      memset(&time_val,0,sizeof(time_val));
      time_val.tv_sec = (time_t)(amticker_timeout_save)+time(NULL);
      
      pthread_mutex_lock(&amticker_mutex.mutex);
      pthread_cond_timedwait(&amticker_condition.cond,&amticker_mutex.mutex,&time_val);
    }
    pthread_mutex_unlock(&amticker_mutex.mutex);
  }

#ifdef INCDEBUG
  dprintf("*** Amticker thread exit ***\n");
#endif
  return 0;    
}
/* ======================= thread_func_readsocket ========================== */
void *thread_func_readsocket(void *thread_param)
{ char buffer[RCVPACK_MAXSIZE+1],timestr[17];
  int i,s,l,slen,ofbd_flg,process_err;
  EMSGB *emsg;
  EMSGF *fmsg;
  LINKER **lpp,*lp;
  FILE *fp;
  char *bufptr;
  struct sockaddr_un saddr;
  time_t fixdrop_time,temp_time;
  int opensock_timeout = TOUTBASE_RSOPENSOCK;
  int msg_dropped = 1;
  int nobuffers_msg_flg = 0;
  int refresh_processor = 0;

  pthread_mutex_lock(&start_rsock_mutex.mutex); pthread_mutex_unlock(&start_rsock_mutex.mutex);
  fixdrop_time = time(NULL); /* save fix "drop_message" time */
 
  for(s = INVALID_SOCKET_VAL;!exit_phase;)
  { if(s < 0)
    { est.statRsTotalOpenSocket++;
      if((s = socket(AF_UNIX,SOCK_DGRAM,0)) >= 0)
      { unlink_file(szInpSockName); est.statRsTotalOpenSocketSuccess++; est.statRsTotalBindSocket++;
        if(bind(s,(struct sockaddr*)&saddr,make_sockaddr(&saddr,szInpSockName)) < 0)
        { est.statRsTotalBindSocketError++; close_unlink(&s,szInpSockName);
        }
        else
        { /* Fix security hole (SGI specific) */ 
	  if(chmod(szInpSockName,0700) == (-1)) est.statRsTotalSockChmodError++;

          ofbd_flg = sizeof(slen); slen = 0;
          if(getsockopt(s,SOL_SOCKET,SO_RCVBUF,&slen,&ofbd_flg) < 0) slen = 0;
          if(slen < RCVSOCK_RCVBUFSIZE)
          { slen = RCVSOCK_RCVBUFSIZE;
            if(setsockopt(s,SOL_SOCKET,SO_RCVBUF,&slen,sizeof(slen)) < 0) est.statRsTotalSetOptError++;
          }
          est.statRsTotalBindSocketSuccess++;
          opensock_timeout = TOUTBASE_RSOPENSOCK;
        }
      }
      else est.statRsTotalOpenSocketError++;

      if(s < 0)
      { sleep(opensock_timeout); opensock_timeout += TOUTPLUS_RSOPENSOCK;
        if(opensock_timeout > TOUTMAX_RSOPENSOCK) opensock_timeout = TOUTMAX_RSOPENSOCK;
      }
    }
    else
    { memset(&saddr,0,sizeof(saddr)); slen = sizeof(saddr); est.statRsTotalReadSocket++;
      process_err = 0; lp = 0;

      if((l = recvfrom(s,buffer,RCVPACK_MAXSIZE,0,(struct sockaddr*)&saddr,&slen)) > 0 && !exit_phase)
      { if(l > RCVPACK_MAXSIZE) l = RCVPACK_MAXSIZE;
        temp_time = time(NULL);
        bufptr = &buffer[0]; ofbd_flg = 0; buffer[l] = 0; est.statRsTotalReadSocketSuccess++;
        if(*bufptr == OFBDMSG_PREFIX)
        { bufptr++; l--;
          if(isxdigit(*bufptr))
          { for(i = 0;isxdigit(*bufptr) && (i < (sizeof(timestr)-1)) && l;l--) timestr[i++] = *bufptr++;
            timestr[i] = 0; sscanf(timestr,"%lx",(unsigned long*)&temp_time);
            est.statRsTotalOfbdTimeMessages++;
          }
          else est.statRsTotalOfbdMessages++;
          ofbd_flg++;
        }
        if(lpszEventLogFilename && (fp = fopen(lpszEventLogFilename,"a")) != NULL)
        { fprintf(fp,"Msg %lu, Size %d:\n\"%s\"\n",est.statRsTotalReadSocketSuccess,l,bufptr); (void)fclose(fp);
        }

        if((emsg = copyMessageToEmsgList(l,bufptr)) != 0)
        { est.statRsTotalMemMsgUsage++; emsg->l_timestamp = temp_time;
          if(ofbd_flg) emsg->l_ofbd_flg = 1;
          else { emsg->l_prevdroped_flg = msg_dropped ? 1 : 0; msg_dropped = 0; }
	  lp = (LINKER*)emsg;
          nobuffers_msg_flg = (refresh_processor = 0);
        }
        else if((fmsg = copyMessageToEmsgFile(l,bufptr)) != 0)
        { est.statRsTotalFileMsgUsage++; fmsg->l_timestamp = temp_time;
          if(ofbd_flg) fmsg->l_ofbd_flg = 1;
          else { fmsg->l_prevdroped_flg = msg_dropped ? 1 : 0; msg_dropped = 0; }
          lp = (LINKER*)fmsg;
          nobuffers_msg_flg = (refresh_processor = 0);
        }
        else /* There are no buffers - drop message */
        { est.statRsTotalDropMsg++;
          if(!refresh_processor)
          { pthread_mutex_lock(&rcvbuf_queue_mutex.mutex);
            if(rcvEmsgList) pthread_cond_signal(&rcvnotempt_condition.cond);
            pthread_mutex_unlock(&rcvbuf_queue_mutex.mutex);
            refresh_processor++;
          }
          if(!ofbd_flg) msg_dropped = 1;
          if(!nobuffers_msg_flg)
          { if(((temp_time = time(NULL)) != (time_t)(-1)) && (difftime(temp_time,fixdrop_time) >= (double)TOUTMIN_RSSLOGDROP))
            { nobuffers_msg_flg = syslog_msg(LOG_ERR,EVMSEQN_NOINPBUFFERS,szThereAreNoInpBuffers,est.statRsTotalDropMsg);
              fixdrop_time = time(NULL);
            }
          } 
          else
          { if(((temp_time = time(NULL)) != (time_t)(-1)) && (difftime(temp_time,fixdrop_time) >= (double)TOUTMAX_RSSLOGDROP))
             nobuffers_msg_flg = 0;
          }
          process_err++;
        }

        /* Check "Out of Band" flag and send response code */
        if(ofbd_flg)
        { if(slen > sizeof(saddr.sun_family))
	  { est.statRsTotalSendOFBDAnsw++;
            if((i = socket(AF_UNIX,SOCK_DGRAM,0)) >= 0)
            { buffer[0] = 'L'; buffer[1] = 'V'; buffer[2] = 'A'; buffer[3] = process_err ? 'E' : 'O';
#ifdef INCDEBUG
              dprintf("OFBD Recv from: \"%s\" socketname size: %d\n",saddr.sun_path,strlen(saddr.sun_path));
#endif
              if(sendto(i,buffer,4,0,(struct sockaddr*)&saddr,slen) != 4)
	      { est.statRsTotalSendOFBDAnswError++;
	        lp = freeEmsgAny(lp);
	      }
              else est.statRsTotalSendOFBDAnswSuccess++;
              close_socket(i);
            }
            else est.statRsTotalOpenSndSockError++;
	  }
	  else { est.statRsTotalSendOFBDAnswError++; lp = freeEmsgAny(lp); }
        }

        /* Add Event Message buffer to Rcv queue */
	if(lp)
	{ pthread_mutex_lock(&rcvbuf_queue_mutex.mutex);
          for(lpp = &rcvEmsgList;*lpp;lpp = (LINKER**)&((*lpp)->next));
          *lpp = lp; rcvMsgListCounter++;
          pthread_cond_signal(&rcvnotempt_condition.cond); pthread_mutex_unlock(&rcvbuf_queue_mutex.mutex);
	}
      }
      if(l <= 0) { close_unlink(&s,szInpSockName); est.statRsTotalReadSocketError++; }
    }
  } /* end of for */
  
  close_unlink(&s,szInpSockName);
#ifdef INCDEBUG
  dprintf("*** Rcv thread exit ***\n");
#endif
  return 0;
}

/* ---------------------- make_reportbuffer -------------------------------- */
static int make_reportbuffer(char *buf,int maxbufsize)
{ int i;
  buf[0] = 0;
  maxbufsize -= 256;

#ifdef INCLUDE_TIME_DATE_STRINGS
  i = sprintf(buf,"%s\nEventMon v%d.%d (%s %s)\nDaemon Statistics since: %s, (%s mode), silence: %d\n",
  szDelimitterLine,EVENTMOND_VMAJOR,EVENTMOND_VMINOR,__TIME__,__DATE__,szFixStartTime,debug_mode ? szDebug : szNormal,silenceMode);
#else
  i = sprintf(buf,"%s\nEventMon v%d.%d\nDaemon Statistics since: %s (%s mode), silence: %d\n",
  szDelimitterLine,EVENTMOND_VMAJOR,EVENTMOND_VMINOR,szFixStartTime, debug_mode ? szDebug : szNormal,silenceMode);
#endif

  if(i < maxbufsize) i += sprintf(&buf[i],"\
Output filename:        \"%s\"\n\
Database library name:  \"%s\"\n\
Database name:          \"%s\"\n\
Database table:         \"%s\"\n\
RcvBuff max size:       %d bytes\n\
InfoBuf max size:       %d bytes\n\
MemBuffer max buf size: %d bytes\n\
MemBuffer prealloc:     %d\n\
MemBuffer max limit:    %d\n\
MemBuffer alloc cnt:    %d\n\
MemBuffer freelist cnt: %d\n\
FileBuffer prealloc:    %d\n\
FileBuffer max limit:   %d\n\
FileBuffer InUsage cnt: %d\n\
Filter Info ready flag: %d\n\
Filter Info alloc cnt:  %d\n\
Filter Info free cnt:   %d\n\
Dark Filter Info cnt:   %d\n\
White Filter Info cnt:  %d\n\
Remove temp file cnt:   %lu\n\
Default EnableAll msg:  %d\n\
RcvMsg List counter:    %lu\n\
OutputMsg List counter: %lu\n\
ESP tag in syslog:      \"%s\"\n\
GroupMode is:           \"%s\"\n\
CurrLoadConfig timeout: %d sec.\n", 
  lpszEventLogFilename ? lpszEventLogFilename : szNONE, /* Output file name */
  lpszSSDBLibraryName  ? lpszSSDBLibraryName  : szNONE, /* Database library file name */
  lpszDatabaseName     ? lpszDatabaseName     : szNONE, /* Database name */
  lpszDbTableName      ? lpszDbTableName      : szNONE, /* Table name */
  RCVPACK_MAXSIZE,    /* RcvBuff max size */
  RCVINFOREQ_MAXSIZE, /* InfoBuf max size */
  EMSGBUF_SIZE,       /* MemBuffer max buf size */
  preallocMemBufCnt,  /* MemBuffer prealloc */
  maxMemBufCnt,       /* MemBuffer max limit */
  emsgAllocCnt,       /* MemBuffer alloc cnt */
  emsgFreeCnt,        /* MemBuffer freelist cnt */
  preallocTmpFileCnt, /* FileBuffer prealloc */
  maxTmpFileCnt,      /* FileBuffer max limit */
  inUsageTempFileName,/* FileBuffer InUsage cnt */
  filterInfoIsReady,  /* Filter Info ready flag */
  filterInfoAllocCnt, /* Filter Info alloc cnt */
  filterInfoFreeCnt,  /* Filter Info free cnt */
  dark_efinfo_cnt,    /* Dark Filter Info cnt */
  whit_efinfo_cnt,    /* White Filter Info cnt */
  clearTmpFileCnt,    /* Remove temp file cnt */
  ALLMSG_DEFAULT_ENABLED_FLAG, /* Default Enable/Disable Flag for all messages */
  rcvMsgListCounter,  /* Rcv messages list counter */
  outMsgListCounter,  /* Output messages list counter */
  enable_ssstag ? szOn : szOff, /* Enable/Disable SSS tag in syslog output */
  enable_group_mode ? szOn : szOff, /* Enable/Disable transparent forwarding */
  cTimeoutBeforeLoadConfig); /* "Load configuration info from database" timeout (sec.) */

  if(i < maxbufsize) i += sprintf(&buf[i],"%s\nAmtickerThread Status:\n\
Thread status:       \"%s\"\n\
Ticker fname:        \"%s\"\n\
Ticker timeout:      %d (sec.)\n\
Status timeout:      %d (hours)\n\
OpenFile Error:      %lu\n\
WriteFile Error:     %lu\n\
Tick File updated:   %lu\n\
StatusTime Expired:  %lu\n\
Amdiag Exec:         %lu\n\
Amdiag not found:    %lu\n",
  szDelimitterLine,            
  enable_amticker ? szOn : szOff,     /* Thread status */
  lpszAmtickerFile,                   /* Ticker fname */
  amticker_timeout,                   /* Ticker timeout */
  amticker_statustime,                /* Status timeout */
  est.statAmTotalOpenFileError,       /* Stat: Amticker thread, total open_timestamp_file_error counter */
  est.statAmTotalWriteFileError,      /* Stat: Amticker thread, total write_timestamp_file_error counter */
  est.statAmTotalFileUpdated,         /* Stat: Amticker thread, total time_stamp_file_updated counter */
  est.statAmTotalStatusTimeExp,       /* Stat: Amticker thread, total status_interval_expired counter */
  est.statAmTotalAmdiagExec,          /* Stat: Amticker thread, total exec_amdiag counter */
  est.statAmTotalAmdiagNotFound);     /* Stat: Amticker thread, total amdiag_not_found counter */

  if(i < maxbufsize) i += sprintf(&buf[i],"%s\nRcvThread Status:\n\
Open socket:         %lu\n\
Open socket success: %lu\n\
Open socket error:   %lu\n\
Bind socket:         %lu\n\
Bind socket success: %lu\n\
Bind socket error:   %lu\n\
Sock chmod error:    %lu\n\
SetSockOpt error:    %lu\n\
Read socket:         %lu\n\
Read socket success: %lu\n\
Read socket error:   %lu\n\
OFBD messages:       %lu\n\
OFBD time messages:  %lu\n\
Memory buffer usage: %lu\n\
File buffer usage:   %lu\n\
Input msg drop:      %lu\n\
Open SndSocket error:%lu\n\
Send OFBD answers:   %lu\n\
Send OFBD answ succ: %lu\n\
Send OFBD answ err:  %lu\n",
  szDelimitterLine,            
  est.statRsTotalOpenSocket,          /* Open socket */
  est.statRsTotalOpenSocketSuccess,   /* Open socket success */
  est.statRsTotalOpenSocketError,     /* Open socket error */
  est.statRsTotalBindSocket,          /* Bind socket */
  est.statRsTotalBindSocketSuccess,   /* Bind socket success */
  est.statRsTotalBindSocketError,     /* Bind socket error */
  est.statRsTotalSockChmodError,      /* Socket name chmod error */
  est.statRsTotalSetOptError,         /* SetSockOpt error */
  est.statRsTotalReadSocket,          /* Read socket */
  est.statRsTotalReadSocketSuccess,   /* Read socket success */
  est.statRsTotalReadSocketError,     /* Read socket error */
  est.statRsTotalOfbdMessages,        /* ofbd_messages counter */
  est.statRsTotalOfbdTimeMessages,    /* ofbd_time_messages counter */
  est.statRsTotalMemMsgUsage,         /* Memory buffer usage */
  est.statRsTotalFileMsgUsage,        /* File buffer usage */
  est.statRsTotalDropMsg,             /* Input msg drop */
  est.statRsTotalOpenSndSockError,    /* Open snd socket error */
  est.statRsTotalSendOFBDAnsw,        /* Total Send OFBD answers */
  est.statRsTotalSendOFBDAnswSuccess, /* Total Send OFBD answers success */
  est.statRsTotalSendOFBDAnswError);  /* Total Send OFBD answers error */

  if(i < maxbufsize) i += sprintf(&buf[i],"%s\nInfoThread Status:\n\
Open socket:         %lu\n\
Open socket success: %lu\n\
Open socket error:   %lu\n\
Bind socket:         %lu\n\
Bind socket success: %lu\n\
Bind socket error:   %lu\n\
SetSockOpt error:    %lu\n\
Read socket:         %lu\n\
Read socket success: %lu\n\
Read socket error:   %lu\n\
Info requests cnt:   %lu\n\
Test requests cnt:   %lu\n\
ReloadFlt requests:  %lu\n\
RefreshOut requests: %lu\n\
AmtickOn requests:   %lu\n\
AmtickOff requests:  %lu\n\
AmtSetTick requests: %lu\n\
AmtSetExec requests: %lu\n\
SetGrpMode requests: %lu\n\
ClrGrpMode requests: %lu\n\
Unknown request cnt: %lu\n\
Open SndInfoSock err:%lu\n\
Send Info success:   %lu\n\
Send Info error:     %lu\n\
Invalid Src address: %lu\n",
  szDelimitterLine,
  est.statInTotalOpenSocket,          /* Open socket */
  est.statInTotalOpenSocketSuccess,   /* Open socket success */
  est.statInTotalOpenSocketError,     /* Open socket error */
  est.statInTotalBindSocket,          /* Bind socket */
  est.statInTotalBindSocketSuccess,   /* Bind socket success */
  est.statInTotalBindSocketError,     /* Bind socket error */
  est.statInTotalSetOptError,         /* SetSockOpt error */
  est.statInTotalReadSocket,          /* Read socket */
  est.statInTotalReadSocketSuccess,   /* Read socket success */
  est.statInTotalReadSocketError,     /* Read socket error */
  est.statInTotalInfoRequest,         /* Info requests cnt */
  est.statInTotalTestRequest,         /* Test requests cnt */
  est.statInTotalReloadFilterRequest, /* ReloadFlt requests */
  est.statInTotalRefreshOutputRequest,/* RefreshOut requests */
  est.statInTotalAmtOnRequest,        /* Amtickerd "on" requests */
  est.statInTotalAmtOffRequest,       /* Amtickerd "off" requests */
  est.statInTotalAmtSetTickInterval,  /* Set avaimonitoring tick interval request counter */
  est.statInTotalAmtSetExecInterval,  /* Set avaimonitoring exec interval request counter */
  est.statInTotalSetGrpModeRequest,   /* Set group mode request counter */
  est.statInTotalClrGrpModeRequest,   /* Reset group mode request counter */
  est.statInTotalUnknownRequest,      /* Unknown request cnt */
  est.statInTotalOpenSndInfoSockError,/* Open Send info Socket error cnt */
  est.statInTotalSendInfoSuccess,     /* Send Info success */
  est.statInTotalSendInfoError,       /* Send Info error */
  est.statInTotalInvSrcAddr);         /* Invalid Source Address */

  if(i < maxbufsize) i += sprintf(&buf[i],"%s\nLoadFilterThread Status:\n\
CurLoadLib timeout:  %d (max %d)\n\
CurInitLib timeout:  %d (max %d)\n\
Load Lib success:    %lu\n\
Load Lib error:      %lu\n\
Init Lib success:    %lu\n\
Init Lib error:      %lu\n\
CreErrh success:     %lu\n\
CreErrh error:       %lu\n\
NewClient success:   %lu\n\
NewClient error:     %lu\n\
OpenConnect success: %lu\n\
OpenConnect error:   %lu\n\
SendRequest success: %lu\n\
SendRequest error:   %lu\n\
GetNumRec success:   %lu\n\
GetNumRec error:     %lu\n\
GetNumCol success:   %lu\n\
GetNumCol error:     %lu\n\
ProcessReq success:  %lu\n\
ProcessReq error:    %lu\n\
WhiteList overload:  %lu\n\
DarkList overload:   %lu\n",
  szDelimitterLine,
  est.statRfCurrentLoadLibraryTimeout,TOUTMAX_LLIB,
  est.statRfCurrentInitLibraryTimeout,TOUTMAX_ILIB,
  est.statRfTotalLoadLibSuccess,
  est.statRfTotalLoadLibError,
  est.statRfTotalInitLibSuccess,
  est.statRfTotalInitLibError,
  est.statRfTotalCreateErrHSuccess,
  est.statRfTotalCreateErrHError,
  est.statRfTotalNewClientSuccess,
  est.statRfTotalNewClientError,
  est.statRfTotalOpenConSuccess,
  est.statRfTotalOpenConError,
  est.statRfTotalSendReqSuccess,
  est.statRfTotalSendReqError,
  est.statRfTotalGetNumRecSuccess,
  est.statRfTotalGetNumRecError,
  est.statRfTotalGetNumColSuccess,
  est.statRfTotalGetNumColError,
  est.statRfTotalProcessReqSuccess,
  est.statRfTotalProcessReqError,
  est.statRfTotalWhiteListOverload,
  est.statRfTotalDarkListOverload);

  if(i < maxbufsize) i += sprintf(&buf[i],"%s\nProcessFilterThread Status:\n\
Pick up messages:    %lu\n\
UnPick up messages:  %lu\n\
StartProc messages:  %lu\n\
ProcToOutput msg:    %lu\n\
ProcToDrop msg:      %lu\n\
Copy message error:  %lu\n\
StartProcReptMsg:    %lu\n\
ProcReptMsg success: %lu\n\
ProcReptMsg error:   %lu\n\
Invalid Lead symbol: %lu\n\
Invalid End symbol:  %lu\n\
Priority Decimal:    %lu\n\
Priority Hex:        %lu\n\
PriorityProc error:  %lu\n\
StartProcNormalMsg:  %lu\n\
InvalDelimInNormMsg: %lu\n\
InvalESPTagInNormMsg:%lu\n\
ProcTypeDecimal:     %lu\n\
ProcTypeHex:         %lu\n\
ProcType error:      %lu\n\
TypeClose error:     %lu\n\
ProcFltInDarkList:   %lu\n\
ProcFltInWhiteList:  %lu\n\
Class not assigned:  %lu\n\
Transparent messages:%lu\n",
  szDelimitterLine,
  est.statPeTotalPickUpMessage,
  est.statPeTotalUnPickUpMessage,
  est.statPeTotalStartProcessMessage,
  est.statPeTotalProcMsgToOutput,
  est.statPeTotalProcMsgToDrop,
  est.statPeTotalProcCopyMsgError,
  est.statPeTotalProcReptMsgStart,
  est.statPeTotalProcReptMsgSuccess,
  est.statPeTotalProcReptMsgError,
  est.statPeTotalProcInvLeadSymbol,
  est.statPeTotalProcInvEndSymbol,
  est.statPeTotalProcPriorDecimal,
  est.statPeTotalProcPriorHex,
  est.statPeTotalProcPriorError,
  est.statPeTotalProcNormMsgStart,
  est.statPeTotalInvDelimInNormMsg,
  est.statPeTotalInvSSSTagInMsg,
  est.statPeTotalProcTypeDecimal,
  est.statPeTotalProcTypeHex,
  est.statPeTotalProcTypeError,
  est.statPeTotalProcCloseTyError,
  est.statPeTotalPFiltInDarkList,
  est.statPeTotalPFiltInWhiteList,
  est.statPeTotalClassNotAssigned,
  est.statPeTotalProcTransparent);

  if(i < maxbufsize) i += sprintf(&buf[i],"%s\nOutputThread Status:\n\
CurTryUseLib timeout:%d (max %d)\n\
Pick up messages:    %lu\n\
UnPick up messages:  %lu\n\
CopyMsgToBuf error:  %lu\n\
ReptLastMsg error:   %lu\n\
OutLib Proc success: %lu\n\
OutLib Common error: %lu\n\
OutLib Fatal error:  %lu\n\
Drop messages:       %lu\n\
TryUsageMax counter: %d\n\
MaxOutput buf size:  %d\n\
OpenSocket error:    %lu\n\
BindSocket error:    %lu\n\
Send error:          %lu\n\
Try Snd:             %lu\n\
Select error:        %lu\n\
Select timeout:      %lu\n\
Select interrupted:  %lu\n\
Rcv error:           %lu\n\
Internal clock size: %lu\n",
  szDelimitterLine,
  est.statOuCurrentTryUseLibraryTimeout,TOUTMAX_OUTTLIB,
  est.statOuTotalPickUpMessage,
  est.statOuTotalUnPickUpMessage,
  est.statOuTotalCopyMessageError,
  est.statOuTotalRepLastMessageError,
  est.statOuTotalOutputLibProcSuccess,
  est.statOuTotalOutputLibCommonError,
  est.statOuTotalOutputLibFatalError,
  est.statOuTotalOutputDropMessage,
  cTryUseOutputApiMax,
  SEM_EVENTDATA_MAX_SIZE,
  est.statOuTotalOpenSocketError,
  est.statOuTotalBindSocketError,
  est.statOuTotalSendError,
  est.statOuTotalTrySend,
  est.statOuTotalSelectError,
  est.statOuTotalSelectTimeout,
  est.statOuTotalSelectIntr,
  est.statOuTotalReceiveError,
  semapi_clockSize);

  return strlen(buf);
}

/* ====================== thread_func_info ================================= */
void *thread_func_info(void *thread_param)
{ char buf[RCVINFOREQ_MAXSIZE+1];
  int i,s,l,slen,buflen;
  struct sockaddr_un saddr;
  int opensock_timeout = TOUTBASE_IFOPENSOCK;

  pthread_mutex_lock(&start_info_mutex.mutex); pthread_mutex_unlock(&start_info_mutex.mutex);

  for(s = INVALID_SOCKET_VAL;!exit_phase;)
  { if(s < 0)
    { est.statInTotalOpenSocket++;
      if((s = socket(AF_UNIX,SOCK_DGRAM,0)) >= 0)
      { unlink_file(szEventMonInfoSockPath); est.statInTotalOpenSocketSuccess++; est.statInTotalBindSocket++;
        if(bind(s,(struct sockaddr*)&saddr,make_sockaddr(&saddr,szEventMonInfoSockPath)) < 0)
        { close_unlink(&s,szEventMonInfoSockPath);
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
    { memset(&saddr,0,sizeof(saddr)); slen = sizeof(saddr);  est.statInTotalReadSocket++;
      if((l = recvfrom(s,buf,RCVINFOREQ_MAXSIZE,0,(struct sockaddr*)&saddr,&slen)) >= 4 && !exit_phase)
      { buf[l] = 0; est.statInTotalReadSocketSuccess++;
        if(buf[0] == 'L' && buf[1] == 'V' && buf[2] == 'A')
        { switch(buf[3]) {
          case EVMCMD_STOP    : pthread_mutex_lock(&exit_phase_mutex.mutex); exit_phase++; pthread_cond_signal(&exit_condition.cond);
                                pthread_mutex_unlock(&exit_phase_mutex.mutex);
                                break;
          case EVMCMD_STAT    : est.statInTotalInfoRequest++;
                                if((slen > sizeof(saddr.sun_family)) && saddr.sun_path[0])
                                { if((buflen = make_reportbuffer(&buf[4],sizeof(buf)-4)) > 0)
                                  { buflen += 5; /* header + 0 */
                                    if((i = socket(AF_UNIX,SOCK_DGRAM,0)) >= 0)
                                    { if((l = sendto(i,buf,buflen,0,(struct sockaddr*)&saddr,slen)) != buflen) est.statInTotalSendInfoError++;
                                      else est.statInTotalSendInfoSuccess++;
                                      close_socket(i);
                                    }
                                    else est.statInTotalOpenSndInfoSockError++;
                                  }
                                }
                                else est.statInTotalInvSrcAddr++;
                                break;                             
          case EVMCMD_TEST    : est.statInTotalTestRequest++;
                                break;
          case EVMCMD_RFILT   : est.statInTotalReloadFilterRequest++;
                                pthread_mutex_lock(&reinit_filter_mutex.mutex); need_init_filter++; pthread_cond_signal(&reinit_condition.cond);
                                pthread_mutex_unlock(&reinit_filter_mutex.mutex);
                                break;
          case EVMCMD_POUT    : est.statInTotalRefreshOutputRequest++;
                                pthread_mutex_lock(&output_queue_mutex.mutex); pthread_cond_signal(&outnotempt_condition.cond);
                                pthread_mutex_unlock(&output_queue_mutex.mutex);
                                break;
          case EVMCMD_AMTON   : est.statInTotalAmtOnRequest++;
	                        pthread_mutex_lock(&amticker_mutex.mutex); enable_amticker = 1; pthread_cond_signal(&amticker_condition.cond);
                                pthread_mutex_unlock(&amticker_mutex.mutex);
                                break;
          case EVMCMD_AMTOFF  : est.statInTotalAmtOffRequest++;
	                        pthread_mutex_lock(&amticker_mutex.mutex); enable_amticker = 0; pthread_cond_signal(&amticker_condition.cond);
                                pthread_mutex_unlock(&amticker_mutex.mutex);
                                break;
          case EVMCMD_SETTICK : est.statInTotalAmtSetTickInterval++;
                                if((i = atoi(&buf[4])) > 0)
				{ pthread_mutex_lock(&amticker_mutex.mutex);
				  amticker_timeout = i; pthread_cond_signal(&amticker_condition.cond);
                                  pthread_mutex_unlock(&amticker_mutex.mutex);
				}
                                break;
          case EVMCMD_SETSTAT : est.statInTotalAmtSetExecInterval++;
                                if((i = atoi(&buf[4])) >= 0)
				{ pthread_mutex_lock(&amticker_mutex.mutex);
				  amticker_statustime = i; pthread_cond_signal(&amticker_condition.cond);
                                  pthread_mutex_unlock(&amticker_mutex.mutex);
				}
                                break;
          case EVMCMD_SETGRP  : est.statInTotalSetGrpModeRequest++;
                                enable_group_mode = 1;
                                break;
          case EVMCMD_CLRGRP  : est.statInTotalClrGrpModeRequest++;
                                enable_group_mode = 0;
                                break;
          default             : est.statInTotalUnknownRequest++;
          }; /* end of switch(buf[3]) */
        }
        else est.statInTotalUnknownRequest++;
      }
      if(l < 0) { close_unlink(&s,szEventMonInfoSockPath); est.statInTotalReadSocketError++; }
    }
  }
  close_unlink(&s,szEventMonInfoSockPath);
#ifdef INCDEBUG
  dprintf("*** Info thread exit ***\n");
#endif
  return 0;
}

/* ------------------- pickUpOutputEmsgList -------------------------------- */
static LINKER *pickUpOutputEmsgList(void)
{ LINKER *lp;
  LINKER *link;  
  if((link = outEmsgList) != 0)
  { for(lp = link;lp;lp = (LINKER*)lp->next)
    { if(lp->lastmessage_flg) break;
    }
    outEmsgList = lp ? (LINKER*)lp->next : 0;
    if(lp) lp->next = 0;
    if(outMsgListCounter) outMsgListCounter--;
  }
  return link;
}

/* -------------------- unpickUpOutputEmsgList ----------------------------- */
static LINKER *unpickUpOutputEmsgList(LINKER *link)
{ LINKER *lhead;
  if((lhead = link) != 0)
  { while(link->next) link = (LINKER*)link->next;
    link->next = outEmsgList; outEmsgList = lhead;
    outMsgListCounter++;
  }
  return 0;
}
/* ---------------------- getCTPfromLinker --------------------------------- */
static time_t getCTPfromLinker(int *_eclass,int *_etype,int *_epri,LINKER *link)
{ *_eclass    = link->eventclass;
  *_etype     = link->eventtype;
  *_epri      = link->eventpri;
  return        link->timestamp;
}
/* ---------------------- semapiDeliverEvent ------------------------------- */
static int semapiDeliverEvent(const char *_hostNameFrom,long eventTimeStamp,int eventClass,int eventType,int eventPri,
int eventCounter,int eventDataLength,void *eventData)
{ int s,i,msglen,slen,reptcnt,sndmaxloopcnt,glbmaxloopcnt,gcnt,need_try;
  char buf[SEM_EVENTDATA_MAX_SIZE+512],rcvbuf[16];
  struct sockaddr_un su_addr;
  struct timeval timeout;
  fd_set fdset;
  int retcode = SEMERR_FATALERROR;

  /* Check some input parameters */
  if((eventDataLength <= 0) || (eventDataLength >= SEM_EVENTDATA_MAX_SIZE) || !eventData || (eventCounter < 0))
    return SEMERR_FATALERROR; /* incorrect input parameters */

  /* Check event counter and adjust it */
  if(!eventCounter) eventCounter++;

  /* Check time stamp value and adjust it */
  if(!eventTimeStamp) eventTimeStamp = time(NULL);

  /* Adjust max loop counter */
  glbmaxloopcnt = (semapi_clockSize == 0) ? 2 : 4;
  sndmaxloopcnt = (semapi_clockSize == 0) ? 2 : 10;

  /* Prepare message header for SEM */
  msglen = sprintf(buf,"SEMC%d.%d<%s><%0lX><%0X><%0X><%0X><%0X>",
                       SEMAPI_VERMAJOR,SEMAPI_VERMINOR,
                       (_hostNameFrom && _hostNameFrom[0]) ? _hostNameFrom : "",
                       eventTimeStamp,eventClass,eventType,eventPri,eventCounter);

  /* Check message size */
  if(!msglen || (msglen+eventDataLength) > sizeof(buf)) return SEMERR_FATALERROR; /* some data size error */

  /* Add message body */
  memcpy(&buf[msglen],eventData,eventDataLength);

  /* Adjust message size */
  msglen += eventDataLength;

  for(need_try = 1,gcnt = 0;gcnt < glbmaxloopcnt && need_try && retcode != SEMERR_SUCCESS;gcnt++)
  { need_try = 0;

    /* Open UNIX socket */
    if((s = socket(AF_UNIX, SOCK_DGRAM, 0)) < 0)
    { need_try = semapi_sleep(1); est.statOuTotalOpenSocketError++;
      retcode = (est.statOuTotalOpenSocketError > SEMERRCNT_OPENSOCK) ? SEMERR_FATALERROR : SEMERR_ERROR;
      continue;
    }

    /* Remove socket name */
    unlink_file(szSEMSockNameTemp);

    /* Bind socket */
    if(bind(s,(struct sockaddr*)&su_addr,make_sockaddr(&su_addr,szSEMSockNameTemp)) < 0)
    { close_socket(s);  need_try = semapi_sleep(1); est.statOuTotalBindSocketError++;
      retcode = (est.statOuTotalBindSocketError > SEMERRCNT_BINDSOCK) ? SEMERR_FATALERROR : SEMERR_ERROR;
      continue;
    }

    /* Adjust size of "send" buffer */
    i = sizeof(reptcnt);
    if(getsockopt(s,SOL_SOCKET,SO_SNDBUF, &reptcnt,&i) < 0) reptcnt = 0;
    if(reptcnt < (SEM_EVENTDATA_MAX_SIZE+1024))
    { reptcnt = (SEM_EVENTDATA_MAX_SIZE+1024); setsockopt(s,SOL_SOCKET,SO_SNDBUF, &reptcnt,sizeof(reptcnt));
    }

    /* Adjust size of "receive" buffer */
    i = sizeof(reptcnt);
    if(getsockopt(s,SOL_SOCKET,SO_RCVBUF, &reptcnt,&i) < 0) reptcnt = 0;
    if(reptcnt < (1024*4))
    { reptcnt = (1024*4); setsockopt(s,SOL_SOCKET,SO_RCVBUF,&reptcnt,sizeof(reptcnt));
    }

    /* Create correct socket address */
    slen = make_sockaddr(&su_addr,szSEMInpSockName);

    /* Try to send message to seh/dsm */
    for(retcode = SEMERR_ERROR,reptcnt = 0;reptcnt < sndmaxloopcnt;reptcnt++)
    { if((i = sendto(s,buf,msglen,0,(struct sockaddr*)&su_addr,slen)) == msglen) { retcode = SEMERR_SUCCESS; break; }
      if(i < 0 && errno == ENOBUFS) { need_try = semapi_sleep(4); est.statOuTotalTrySend++; continue; }
      est.statOuTotalSendError++;
      break;
    }

    /* Check "send message" error code */
    if(retcode != SEMERR_SUCCESS)
    { close_unlink(&s,szSEMSockNameTemp);
      continue;
    }
    
    /* Try to receive if transfer was ok */
    FD_ZERO(&fdset); FD_SET(s,&fdset);
    timeout.tv_sec = 3; timeout.tv_usec = 0;

    /* Try to do select */
    for(retcode = SEMERR_ERROR,reptcnt = 0;reptcnt < 2;reptcnt++)
    { if((i = select((int)s+1,&fdset,0,0,&timeout)) <= 0)
      { if(!i) { est.statOuTotalSelectTimeout++; continue; }
        if(i < 0 && errno == EINTR) { est.statOuTotalSelectIntr++; reptcnt--; continue; }
        est.statOuTotalSelectError++;
        break;
      }
      if(FD_ISSET(s,&fdset)) { retcode = SEMERR_SUCCESS; break; }
    }

    /* Check "select" error code */
    if(retcode != SEMERR_SUCCESS)
    { FD_CLR(s,&fdset); close_unlink(&s,szSEMSockNameTemp);
      break;
    }

    /* Try receive response from sem server */
    for(retcode = SEMERR_ERROR;;)
    { memset(&su_addr,0,sizeof(su_addr)); slen = sizeof(su_addr);
      i = recvfrom(s,rcvbuf,sizeof(rcvbuf),0,(struct sockaddr*)&su_addr,&slen);
      if(i < 0 && errno == EINTR) continue;
      if(i >= 4)
      { retcode = ((rcvbuf[0] == 'S') && (rcvbuf[1] == 'E') && (rcvbuf[2] == 'M') && (rcvbuf[3] == 'O')) ?
                  SEMERR_SUCCESS : SEMERR_FATALERROR;
      }
      else est.statOuTotalReceiveError++;
      break;
    } /* end for */
    FD_CLR(s,&fdset); close_unlink(&s,szSEMSockNameTemp);
  }
  return retcode;
}
/* ==================== thread_func_output ================================= */
void *thread_func_output(void *thread_param)
{ int timeout,bsize,reptcnt,eclass,etype,epri,useliberr_msgout,uselibferr_msgout,ret_code,tryusecnt;
  unsigned long semapi_retcode;
  char buf[RCVPACK_MAXSIZE+1],*buffer,*hostname;
  struct timespec time_val;
  time_t timestamp,temp_time,fixfatal_time;
  LINKER last,*link;

  est.statOuCurrentTryUseLibraryTimeout = TOUTBASE_OUTTLIB;
  memset(&last,0,sizeof(LINKER));
  useliberr_msgout = (uselibferr_msgout = 0);
  last.bufidx  = (-1);
 
  pthread_mutex_lock(&start_output_mutex.mutex); pthread_mutex_unlock(&start_output_mutex.mutex);

  fixfatal_time = time(NULL);

  for(timeout = 0,tryusecnt = 0;!exit_phase;)
  { pthread_mutex_lock(&output_queue_mutex.mutex);
    if(!timeout)
    { for(ret_code = EINVAL;!exit_phase && (link = pickUpOutputEmsgList()) == 0;)
       pthread_cond_wait(&outnotempt_condition.cond,&output_queue_mutex.mutex);
    }
    else
    { memset(&time_val,0,sizeof(struct timespec)); time_val.tv_sec = (time_t)timeout+time(NULL);
      do
      { ret_code = pthread_cond_timedwait(&outnotempt_condition.cond,&output_queue_mutex.mutex,&time_val);
      } while(!exit_phase && (link = pickUpOutputEmsgList()) == 0 && ret_code != ETIMEDOUT);
    }
    pthread_mutex_unlock(&output_queue_mutex.mutex);

    timeout = 0;
    if(exit_phase || !link) continue;

    est.statOuTotalPickUpMessage++; /* One more PickUp message from queue */
 
    hostname = 0; /* reset hostname pointer */
    if(link->reptcnt) /* repeat event */
    { if(last.bufidx >= 0 && (link->reptcnt-1) > 0)         /* valid previous message and correct rept counter */
      { timestamp = getCTPfromLinker(&eclass,&etype,&epri,&last);
        reptcnt = link->reptcnt-1; buffer = (char *)last.next; bsize = last.reptcnt;
      }
      else { link = freeEmsgAny(link); est.statOuTotalRepLastMessageError++; } /* Discard "repeate" event */
    }
    else /* normal message (without repeat counter) */
    { if(link->ofbd_flg) /* normal out of band message */
      { if((bsize = copyAnyMessageToBuffer(link,staticBuf,RCVPACK_MAXSIZE)) != 0)
        { staticBuf[bsize] = 0; timestamp = getCTPfromLinker(&eclass,&etype,&epri,link);
          reptcnt = 1; buffer = (staticBuf+link->bufidx); bsize = bsize - link->bufidx;
          if(link->hostnamelen) (hostname = (staticBuf+link->beghostidx))[link->hostnamelen] = 0;
        }
        else { link = freeEmsgAny(link); est.statOuTotalCopyMessageError++; }  /* Discard "out of band" event */
      }
      else /* normal message from syslogd */
      { if((bsize = copyAnyMessageToBuffer(link,buf,RCVPACK_MAXSIZE)) != 0)
        { buf[bsize] = 0; timestamp = getCTPfromLinker(&eclass,&etype,&epri,link);
          reptcnt = 1;  buffer = (buf+link->bufidx);  bsize = bsize - link->bufidx;
        }
        else { last.bufidx  = (-1); link = freeEmsgAny(link); est.statOuTotalCopyMessageError++; } /* Discard "normal" syslogd message */
      }
    }

    if(link)
    { semapi_retcode = (SEM_EVENTDATA_MAX_SIZE >= bsize) ? semapiDeliverEvent(hostname,(long)timestamp,eclass,etype,epri,reptcnt,bsize,buffer) : SEMERR_FATALERROR;
#ifdef INCDEBUG
      dprintf("OutMsg: <%d>(%d,%d) Rept:%d Size:%d Time: 0x%lX \"%s\"\n",epri,eclass,etype,reptcnt,bsize,timestamp,buffer);
#endif
      if(semapi_retcode == SEMERR_SUCCESS) /* Success */
      { est.statOuTotalOutputLibProcSuccess++; est.statOuCurrentTryUseLibraryTimeout = TOUTBASE_OUTTLIB;
        useliberr_msgout = (uselibferr_msgout = 0);
        if(!link->ofbd_flg)
        { if(!link->reptcnt) /* normal message - save it for future usage */
          { last.eventclass = eclass; last.eventtype = etype; last.eventpri = epri;
            last.reptcnt = bsize;  last.next = buffer; last.bufidx = 0; last.timestamp = timestamp;
          }
          else last.bufidx = (-1); /* if "repeate" message, discard "saved" flag */
        }
        link = freeEmsgAny(link); timeout = (tryusecnt = 0);
      }
      else /* Some Output error */
      { if(semapi_retcode == SEMERR_FATALERROR) /* Unrecoverable Output library error, need drop message */
        { est.statOuTotalOutputLibFatalError++; est.statOuCurrentTryUseLibraryTimeout = TOUTBASE_OUTTLIB;
          if(!link->ofbd_flg) last.bufidx = (-1); /* discard "saved" flag for "normal" syslogd message */
          link = freeEmsgAny(link); est.statOuTotalOutputDropMessage++; /* Disacrd message if fatal error */

          if(!uselibferr_msgout) /* already have syslog notification */
          { if(((temp_time = time(NULL)) != (time_t)(-1)) && (difftime(temp_time,fixfatal_time) >= (double)TOUTMIN_LOGFMSG))
            { uselibferr_msgout = syslog_msg(LOG_WARNING,EVMSEQN_OUTPFERROR,szOutputLibraryUseFError,est.statOuTotalOutputLibFatalError);
              fixfatal_time = time(NULL);
            }
          }
          else
          { if(((temp_time = time(NULL)) != (time_t)(-1)) && (difftime(temp_time,fixfatal_time) >= (double)TOUTMAX_LOGFMSG))
             uselibferr_msgout = 0;
          }
#ifdef INCDEBUG
          dprintf("SEM API ssDeliverEvent - Some Fatal Error - drop message\n"); /* FIXME */
#endif
          timeout = (tryusecnt = 0); /* reset current timeout and try counter */
        }
        else if(semapi_retcode == SEMERR_ERROR) /* non fatal error (may be in future we can fix this problem?) */
        { est.statOuTotalOutputLibCommonError++; timeout = est.statOuCurrentTryUseLibraryTimeout; /* try resend event after timeout */
#ifdef INCDEBUG
          dprintf("SEM API ssDeliverEvent - Non Fatal Error, I'll try to do the same after %d sec.\n",est.statOuCurrentTryUseLibraryTimeout);
#endif              
          if(ret_code == ETIMEDOUT) /* recoverable error, try resend */
          { est.statOuCurrentTryUseLibraryTimeout += TOUTPLUS_OUTTLIB; /* adjust "try" timeout */
            if(est.statOuCurrentTryUseLibraryTimeout > TOUTMAX_OUTTLIB)
            { est.statOuCurrentTryUseLibraryTimeout = TOUTMAX_OUTTLIB;
              if(!useliberr_msgout) useliberr_msgout = syslog_msg(LOG_WARNING,EVMSEQN_OUTPSERROR,szOutputLibraryUseError,timeout);
            }
            if(cTryUseOutputApiMax && (++tryusecnt > cTryUseOutputApiMax)) /* check "try" counter */
            { est.statOuCurrentTryUseLibraryTimeout = TOUTBASE_OUTTLIB;
              if(!link->ofbd_flg) last.bufidx = (-1);
              link = freeEmsgAny(link); timeout = (tryusecnt = 0);
              est.statOuTotalOutputDropMessage++; /* Discard message if too many "retry" */
            }
          }
        }
        else /* unknown SEM error */
        { if(!link->ofbd_flg) last.bufidx = (-1);
          link = freeEmsgAny(link); timeout = (tryusecnt = 0);
          est.statOuTotalOutputDropMessage++;
        }
      } /* else */
    } /* if(link) */

    if(link)
    { pthread_mutex_lock(&output_queue_mutex.mutex); link = unpickUpOutputEmsgList(link);
      pthread_mutex_unlock(&output_queue_mutex.mutex); est.statOuTotalUnPickUpMessage++;
    }
  } /* end of for */

#ifdef INCDEBUG
  dprintf("*** Output thread exit ***\n");
#endif
  return 0;
}

/* ----------------------- pickUpEmsgList ---------------------------------- */
static LINKER *pickUpEmsgList(void)
{ LINKER *lp;
  LINKER *link;  
  if((link = rcvEmsgList) != 0)
  { for(lp = link;lp;lp = (LINKER*)lp->next)
    { if(lp->lastmessage_flg) break;
    }
    rcvEmsgList = lp ? (LINKER*)lp->next : 0;
    if(lp) lp->next = 0;
    if(rcvMsgListCounter) rcvMsgListCounter--;
  }
  return link;
}

/* ----------------------- unpickUpEmsgList -------------------------------- */
static LINKER *unpickUpEmsgList(LINKER *link)
{ LINKER *lhead;
  if((lhead = link) != 0)
  { while(link->next) link = (LINKER*)link->next;
    link->next = rcvEmsgList; rcvEmsgList = lhead;
    rcvMsgListCounter++;
  }
  return 0;
}
/* -------------------------- skipLeadSpace -------------------------------- */
static int skipLeadSpace(char *buf,int idx)
{ while(buf[idx] == ' ' || buf[idx] == '\t') idx++;
  return idx;
}
/* ---------------------- getIntegerFromString ----------------------------- */
static int getIntegerFromString(char *buf,int *_idx,int idxoff,int *dst,unsigned long volatile *hexCnt,unsigned long volatile *decCnt)
{ char tmp[512];
  int idx,hex,i = 0;
  if(buf && _idx && dst)
  { idx = skipLeadSpace(buf,(*_idx)+idxoff);
    hex = (buf[idx] == '0' && (buf[idx+1] == 'x' || buf[idx+1] == 'X')) ? 1 : 0;
    if(hex)
    { if(hexCnt) (*hexCnt)++;
      tmp[i] = buf[idx++]; tmp[++i] = buf[idx++];
      for(;i < sizeof(tmp)-1 && buf[idx];tmp[i++] = buf[idx++]) if(!isxdigit(buf[idx])) break; tmp[i] = 0;
      i = i > 1 ? sscanf(tmp,"%x",dst) : 0;
    }
    else
    { if(decCnt) (*decCnt)++;
      for(;i < sizeof(tmp)-1 && buf[idx];tmp[i++] = buf[idx++]) if(!isdigit(buf[idx])) break; tmp[i] = 0;
      i = i ? sscanf(tmp,"%d",dst) : 0;
    }
    *_idx = skipLeadSpace(buf,idx);
  }
  return i;
}
/* ---------------------- processEventMessage ------------------------------ */
/* Parse message string and extract priority and type values.                */
/* This function work under 'efilter_list_mutex'. Use deep stack for unpack  */
/* original message. Set valid bufidx value.                                 */
/* return value(s): 0 - some error, != 0 - success.                          */
/* ------------------------------------------------------------------------- */
static int processEventMessage(LINKER *link)
{ char buf[RCVPACK_MAXSIZE+1];
  char tmp[512];
  int bsize,i,idx,retcode = 0;;

  if(!link) return 0; /* Incorrect parameter */

  link->eventclass  = 0;
  link->eventtype   = 0;
  link->eventpri    = 0;
  link->reptcnt     = 0;
  link->bufidx      = 0;
  link->beghostidx  = 0;
  link->hostnamelen = 0;

  memset(buf,0,sizeof(buf));

  if((bsize = copyAnyMessageToBuffer(link,buf,RCVPACK_MAXSIZE)) == 0)
  { est.statPeTotalProcCopyMsgError++; /* Can't copy message buffer(s) to flat buffer */
    return 0;
  }

  buf[bsize] = 0; idx = skipLeadSpace(buf,0);
  if(buf[idx] != SSSMSG_BEGINBLK) { est.statPeTotalProcInvLeadSymbol++; return 0; }

  if(!getIntegerFromString(buf,&idx,1,&link->eventpri,&est.statPeTotalProcPriorHex,&est.statPeTotalProcPriorDecimal))
  { est.statPeTotalProcPriorError++;
    return 0;
  }

  if(buf[idx++] != SSSMSG_ENDBLK) { est.statPeTotalProcInvEndSymbol++; return 0; }
  
  idx = skipLeadSpace(buf,idx);

  if(buf[idx++] != SSSMSG_BEGINBLK) { est.statPeTotalProcInvLeadSymbol++; return 0; }
  if(link->ofbd_flg) link->beghostidx = idx; /* save index of hostname */

  while(buf[idx] && buf[idx] != SSSMSG_ENDBLK) /* try find end of hostname block */
  { if(link->ofbd_flg) link->hostnamelen++;
    idx++;
  }
  if(buf[idx] != SSSMSG_ENDBLK) { est.statPeTotalProcInvEndSymbol++; return 0; }
  buf[idx++] = 0;

  if(enable_group_mode && link->hostnamelen)
  { if(strcmp(&buf[link->beghostidx],szLocalHostString) && strcmp(&buf[link->beghostidx],szLocalHostName))
    { link->transparent = 1;
      est.statPeTotalProcTransparent++;
    }
  }

  idx = skipLeadSpace(buf,idx);
  for(i = 0;i < lastMessageInfoStrSize && i < (sizeof(tmp)-1);i++) if((tmp[i] = buf[idx+i]) == 0) break;
  tmp[i] = 0;
  /* Check rept message string */
  if((i == lastMessageInfoStrSize) && !strcmp(tmp,szLastMessageRepeated))
  { est.statPeTotalProcReptMsgStart++;
    if(getIntegerFromString(buf,&idx,i,&link->reptcnt,0,0) && (link->reptcnt > 0))
    { for(i = 0;i < sizeof(tmp)-1 && buf[idx];tmp[i++] = buf[idx++])
       if(buf[idx] == ' ' || buf[idx] == '\t' || buf[idx] == '\n' || buf[idx] == '\r') break;              
      tmp[i] = 0;
      if(!strcmp(tmp,szLastMsgRetTimes)) { retcode++; est.statPeTotalProcReptMsgSuccess++; }
    }
    if(!retcode) est.statPeTotalProcReptMsgError++;
  }
  else /* Check normal message, try to find SSS tag */
  { est.statPeTotalProcNormMsgStart++;
    while(buf[idx] && buf[idx] != ':') idx++;
    if(buf[idx] == ':')
    { if((link->bufidx = (idx = skipLeadSpace(buf,++idx))) > bsize) link->bufidx = bsize; /* additional protection */
      if(buf[idx] == SSSMSG_TAG1 && buf[idx+1] == SSSMSG_TAG2 && buf[idx+2] == '(')
      { /* Read type number (seq.number) */
        if(getIntegerFromString(buf,&idx,3,&link->eventtype,&est.statPeTotalProcTypeHex,&est.statPeTotalProcTypeDecimal))
        { if(buf[idx] == ')')
          { if((link->bufidx = (idx = skipLeadSpace(buf,++idx))) > bsize) link->bufidx = bsize;
            retcode++;
          }
          else est.statPeTotalProcCloseTyError++;
        }
        else est.statPeTotalProcTypeError++;
      }
      else { est.statPeTotalInvSSSTagInMsg++; retcode++; link->bufidx = idx; }
    }
    else est.statPeTotalInvDelimInNormMsg++;
  }
  return retcode;
}
/* ---------------------- processFilter ------------------------------------ */
/* Check message class, type and priority. Use filter info - disable/enable  */
/* At the same time class value will be assigned.                            */
/* return: 0 - disabled, != 0 - enabled                                      */
/* ------------------------------------------------------------------------- */
static int processFilter(LINKER *link,int prevdiscarded)
{ EFINFO *e;
  int enabled = 1;

  if(exit_phase || !link || (link->reptcnt && prevdiscarded)) return 0;

  if(link->transparent) return 1; /* Enabled */
  
  if(!link->reptcnt) /* normal message */
  { enabled = ALLMSG_DEFAULT_ENABLED_FLAG;

    /* Check Dark list and assign class if it possible */
    for(e = dark_efinfo_list;e;e = e->next)
    { if(link->eventclass)
      { if(((e->event_type  == (-1)) || (e->event_type  == link->eventtype)) &&
           ((e->event_class == (-1)) || (e->event_class == link->eventclass)) &&
           ((e->prior_facil == (-1)) || (e->prior_facil == link->eventpri)))
        { enabled = 0; est.statPeTotalPFiltInDarkList++;
#ifdef INCDEBUG
          dprintf("Find Message: <%d> (%d,%d) in dark list <%d> (%d,%d)\n", /* FIXME */
                   link->eventpri,link->eventclass,link->eventtype,e->prior_facil,e->event_class,e->event_type);
#endif
          break;
        }
      }
      else /* for unknown class */
      { if((e->event_type  == (-1)) || (e->event_type  == link->eventtype))
        { if(e->event_class != (-1)) link->eventclass = e->event_class;
          if((e->prior_facil == (-1)) || (e->prior_facil == link->eventpri))
          { enabled = 0; est.statPeTotalPFiltInDarkList++;
#ifdef INCDEBUG
            dprintf("Find Message: <%d> (%d,%d) in dark list <%d> (%d,%d)\n", /* FIXME */
                     link->eventpri,link->eventclass,link->eventtype,e->prior_facil,e->event_class,e->event_type);
#endif
            if(link->eventclass) break;
          }
        }
      }
    }

    /* Check White list and assign class if it possible */
    if((!enabled || !link->eventclass) && !exit_phase)
    { for(e = whit_efinfo_list;e;e = e->next)
      { if(link->eventclass)
        { if(((e->event_type  == (-1)) || (e->event_type  == link->eventtype))  &&
             ((e->event_class == (-1)) || (e->event_class == link->eventclass)) &&
             ((e->prior_facil == (-1)) || (e->prior_facil == link->eventpri)))
          { enabled++; est.statPeTotalPFiltInWhiteList++;
#ifdef INCDEBUG
            dprintf("Find Message: <%d> (%d,%d) in white list <%d> (%d,%d)\n",
                     link->eventpri,link->eventclass,link->eventtype,e->prior_facil,e->event_class,e->event_type);
#endif

            break;
          }
        }
        else /* for unknown class */
        { if((e->event_type  == (-1)) || (e->event_type  == link->eventtype))
          { if(e->event_class != (-1)) link->eventclass = e->event_class;
            if((e->prior_facil == (-1)) || (e->prior_facil == link->eventpri))
            { enabled = 1; est.statPeTotalPFiltInWhiteList++;
#ifdef INCDEBUG
              dprintf("Find Message: <%d> (%d,%d) in white list <%d> (%d,%d)\n",
                       link->eventpri,link->eventclass,link->eventtype,e->prior_facil,e->event_class,e->event_type);
#endif
              if(link->eventclass) break;
            }
          }
        }
      }
    }
    if(!link->eventclass) est.statPeTotalClassNotAssigned++;
  }
  return enabled;
}

/* ==================== thread_func_procfilter ============================= */
void *thread_func_procfilter(void *thread_param)
{ LINKER **lpp;
  LINKER *link;
  int processed,discard,refresh_output;

  pthread_mutex_lock(&start_procfilter_mutex.mutex); pthread_mutex_unlock(&start_procfilter_mutex.mutex);
  
  for(refresh_output = 0,discard = 1;!exit_phase;)
  { pthread_mutex_lock(&rcvbuf_queue_mutex.mutex);
    while((!getFilterInfoIsReadyFlag() || (link = pickUpEmsgList()) == 0) && !exit_phase)
     pthread_cond_wait(&rcvnotempt_condition.cond,&rcvbuf_queue_mutex.mutex);
    pthread_mutex_unlock(&rcvbuf_queue_mutex.mutex);

    if(!exit_phase && link)
    { est.statPeTotalPickUpMessage++;
      pthread_mutex_lock(&efilter_list_mutex.mutex);
      if(!filterInfoIsReady)
      { pthread_mutex_unlock(&efilter_list_mutex.mutex);
        pthread_mutex_lock(&rcvbuf_queue_mutex.mutex);
        unpickUpEmsgList(link);
        pthread_mutex_unlock(&rcvbuf_queue_mutex.mutex);
        est.statPeTotalUnPickUpMessage++;
      }
      else
      { est.statPeTotalStartProcessMessage++;
        if(link->prevdroped_flg) discard = 1;
        if((processed = processEventMessage(link)) != 0) processed = processFilter(link,link->ofbd_flg != 0 ? 0 : discard);
        pthread_mutex_unlock(&efilter_list_mutex.mutex);
        if(processed) /* process: success */
        { est.statPeTotalProcMsgToOutput++;
          if(!link->ofbd_flg) discard = 0;
          pthread_mutex_lock(&output_queue_mutex.mutex);
          for(lpp = &outEmsgList;*lpp;lpp = (LINKER**)&((*lpp)->next));
          *lpp = link; outMsgListCounter++;
          pthread_cond_signal(&outnotempt_condition.cond); pthread_mutex_unlock(&output_queue_mutex.mutex);
          refresh_output = 0;
        }
        else /* process: error */
        { est.statPeTotalProcMsgToDrop++;
          if(!link->ofbd_flg) discard = 1;
          freeEmsgAny(link);
          if(!refresh_output)
          { pthread_mutex_lock(&output_queue_mutex.mutex);
            if(outEmsgList) pthread_cond_signal(&outnotempt_condition.cond);
            pthread_mutex_unlock(&output_queue_mutex.mutex);
            refresh_output++;
          }
        }
      }
    }
  } /* end of while(!exit_phase) */
#ifdef INCDEBUG
  dprintf("*** Process thread exit ***\n");
#endif
  return 0;
}

/* ------------------------ unload_ssdb_library ---------------------------- */
static void unload_ssdb_library(int staton_flg)
{ if(ssdb_dlhandle)
  { if(staton_flg && fp_ssdbDone) fp_ssdbDone();
    if(!dlclose(ssdb_dlhandle)) ssdb_dlhandle = NULL;
  }
  if(!ssdb_dlhandle) zero_ssdbFuncPointers();
}

/* ------------------------- load_ssdb_library ---------------------------- */
static int load_ssdb_library(int staton_flg)
{ int lfp_error = 0;
  if(ssdb_dlhandle) return 1;
/*  if((ssdb_dlhandle = dlopen(lpszSSDBLibraryName,RTLD_NOW)) == NULL) FIXME */

  if((ssdb_dlhandle = dlopen(lpszSSDBLibraryName,RTLD_LAZY)) == NULL)
  { if(staton_flg) est.statRfTotalLoadLibError++;
    return 0;
  }
  if((fp_ssdbInit               = (FPssdbInit*)dlsym(ssdb_dlhandle,"ssdbInit")) == 0) lfp_error++;
  if((fp_ssdbDone               = (FPssdbDone*)dlsym(ssdb_dlhandle,"ssdbDone")) == 0) lfp_error++;
  if((fp_ssdbCreateErrorHandle  = (FPssdbCreateErrorHandle*)dlsym(ssdb_dlhandle,"ssdbCreateErrorHandle")) == 0) lfp_error++;
  if((fp_ssdbDeleteErrorHandle  = (FPssdbDeleteErrorHandle*)dlsym(ssdb_dlhandle,"ssdbDeleteErrorHandle")) == 0) lfp_error++;
  if((fp_ssdbNewClient          = (FPssdbNewClient*)dlsym(ssdb_dlhandle,"ssdbNewClient")) == 0) lfp_error++;
  if((fp_ssdbDeleteClient       = (FPssdbDeleteClient*)dlsym(ssdb_dlhandle,"ssdbDeleteClient")) == 0) lfp_error++;
  if((fp_ssdbOpenConnection     = (FPssdbOpenConnection*)dlsym(ssdb_dlhandle,"ssdbOpenConnection")) == 0) lfp_error++;
  if((fp_ssdbCloseConnection    = (FPssdbCloseConnection*)dlsym(ssdb_dlhandle,"ssdbCloseConnection")) == 0) lfp_error++;
/*  if((fp_ssdbSendRequest        = (FPssdbSendRequest*)dlsym(ssdb_dlhandle,"ssdbSendRequest")) == 0) lfp_error++; */
  if((fp_ssdbSendRequestTrans   = (FPssdbSendRequestTrans*)dlsym(ssdb_dlhandle,"ssdbSendRequestTrans")) == 0) lfp_error++;
  if((fp_ssdbFreeRequest        = (FPssdbFreeRequest*)dlsym(ssdb_dlhandle,"ssdbFreeRequest")) == 0) lfp_error++;
  if((fp_ssdbGetNumRecords      = (FPssdbGetNumRecords*)dlsym(ssdb_dlhandle,"ssdbGetNumRecords")) == 0) lfp_error++;
  if((fp_ssdbGetNumColumns      = (FPssdbGetNumColumns*)dlsym(ssdb_dlhandle,"ssdbGetNumColumns")) == 0) lfp_error++;
  if((fp_ssdbGetNextField       = (FPssdbGetNextField*)dlsym(ssdb_dlhandle,"ssdbGetNextField")) == 0) lfp_error++;
  if((fp_ssdbGetLastErrorCode   = (FPssdbGetLastErrorCode*)dlsym(ssdb_dlhandle,"ssdbGetLastErrorCode")) == 0) lfp_error++;
#ifdef INCDEBUG
  if((fp_ssdbGetLastErrorString = (FPssdbGetLastErrorString*)dlsym(ssdb_dlhandle,"ssdbGetLastErrorString")) == 0) lfp_error++;
#endif

  if(!lfp_error && staton_flg)
  { est.statRfTotalLoadLibSuccess++;
    if(!fp_ssdbInit()) { est.statRfTotalInitLibError++; lfp_error++; }
    else est.statRfTotalInitLibSuccess++;
  }

  if(lfp_error)
  { if(staton_flg) est.statRfTotalLoadLibError++;
    unload_ssdb_library(staton_flg);
    return 0;
  }
  return 1;
}

/* -------------------------- load_filter_info ---------------------------- */
static int load_filter_info(void)
{ ssdb_Error_Handle hError;
  ssdb_Client_Handle hClient;
  ssdb_Connection_Handle hConnect;
  ssdb_Request_Handle hRequest;
  int i,err_flg,rows_cnt,cols_cnt,e_class,e_type,e_prior,e_enable;
  EFINFO *ef;
  static int empty_table_set_errflg = 0;

  if((hError = fp_ssdbCreateErrorHandle()) == 0)
  { est.statRfTotalCreateErrHError++;
    return 0;
  }
  est.statRfTotalCreateErrHSuccess++;

  if((hClient = fp_ssdbNewClient(hError,EVM_SSDBCLIENT_NAME,EVM_SSDBCLIENT_PASSWORD,LOCAL_SSDB_NEWCLIENTFLAGS)) == 0)
  { est.statRfTotalNewClientError++; fp_ssdbDeleteErrorHandle(hError);
    return 0;
  }
  est.statRfTotalNewClientSuccess++;

  if((hConnect = fp_ssdbOpenConnection(hError,hClient,NULL,lpszDatabaseName,LOCAL_SSDB_NEWCONNECTFLAGS)) == 0)
  { est.statRfTotalOpenConError++; 
#ifdef INCDEBUG
    dprintf("SSDB Open Connection Error: %d - %s\n",fp_ssdbGetLastErrorCode(hError),fp_ssdbGetLastErrorString(hError));
#endif
    fp_ssdbDeleteClient(hError,hClient);  fp_ssdbDeleteErrorHandle(hError);
    return 0;
  }
  est.statRfTotalOpenConSuccess++;

  if((hRequest = fp_ssdbSendRequestTrans(hError,hConnect,SSDB_REQTYPE_SELECT,lpszDbTableName,"select class_id,type_id,priority,enabled from %s",lpszDbTableName)) == 0)
  { est.statRfTotalSendReqError++;
#ifdef INCDEBUG
    dprintf("SSDB SendRequestTrans Error: %d - %s\n",fp_ssdbGetLastErrorCode(hError),fp_ssdbGetLastErrorString(hError));
#endif
    fp_ssdbCloseConnection(hError,hConnect); fp_ssdbDeleteClient(hError,hClient);
    fp_ssdbDeleteErrorHandle(hError);
    return 0;
  }
  est.statRfTotalSendReqSuccess++;

  rows_cnt = fp_ssdbGetNumRecords(hError,hRequest);
  if(fp_ssdbGetLastErrorCode(hError) != SSDBERR_SUCCESS)
  { est.statRfTotalGetNumRecError++;
#ifdef INCDEBUG
    dprintf("SSDB GetNumRecords Error: %d - %s\n",fp_ssdbGetLastErrorCode(hError),fp_ssdbGetLastErrorString(hError));
#endif
    fp_ssdbFreeRequest(hError,hRequest); fp_ssdbCloseConnection(hError,hConnect);
    fp_ssdbDeleteClient(hError,hClient); fp_ssdbDeleteErrorHandle(hError);
    return 0;
  }
  est.statRfTotalGetNumRecSuccess++;

  cols_cnt = fp_ssdbGetNumColumns(hError,hRequest);
  if((fp_ssdbGetLastErrorCode(hError) != SSDBERR_SUCCESS) || (cols_cnt < 4))
  { est.statRfTotalGetNumColError++;
#ifdef INCDEBUG
    dprintf("SSDB GetNumColumns Error: %d - %s\n",fp_ssdbGetLastErrorCode(hError),fp_ssdbGetLastErrorString(hError));
#endif
    fp_ssdbFreeRequest(hError,hRequest); fp_ssdbCloseConnection(hError,hConnect);
    fp_ssdbDeleteClient(hError,hClient); fp_ssdbDeleteErrorHandle(hError);
    return 0;
  }
  est.statRfTotalGetNumColSuccess++;

#ifdef INCDEBUG
  dprintf("SSDB Request is Ok! Rows:%d Colom: %d\n",rows_cnt,cols_cnt); /* FIXME */
#endif

  if(!rows_cnt && !empty_table_set_errflg)
  { empty_table_set_errflg = syslog_msg(LOG_WARNING,EVMSEQN_DBISEMPTY,szDatabaseIsEmpty,lpszDatabaseName,lpszDbTableName);
  }
  if(rows_cnt) empty_table_set_errflg = 0;

  for(err_flg = 0,i = 0;i < rows_cnt && !exit_phase;i++)
  { if(!fp_ssdbGetNextField(hError,hRequest,&e_class,sizeof(e_class)))   { err_flg++; break; }
    if(!fp_ssdbGetNextField(hError,hRequest,&e_type,sizeof(e_type)))     { err_flg++; break; }
    if(!fp_ssdbGetNextField(hError,hRequest,&e_prior,sizeof(e_prior)))   { err_flg++; break; }          
    if(!fp_ssdbGetNextField(hError,hRequest,&e_enable,sizeof(e_enable))) { err_flg++; break; }
#ifdef INCDEBUG
    dprintf("|%6d  |%6d  |%6d  | %s |\n",e_class,e_type,e_prior,e_enable ? "On " : "Off"); /* FIXME */
#endif
    if((ef = newEventInfo(e_class,e_type,e_prior)) == 0)                 { err_flg++; break; }
    if(e_enable) { if(!putEventInfoIntoWhiteList(ef)) { freeEventInfo(ef); est.statRfTotalWhiteListOverload++; } }
    else         { if(!putEventInfoIntoDarkList(ef))  { freeEventInfo(ef); est.statRfTotalDarkListOverload++; } }
  } 

  if(err_flg) est.statRfTotalProcessReqError++;
  else        est.statRfTotalProcessReqSuccess++;
 
  fp_ssdbFreeRequest(hError,hRequest);
  fp_ssdbCloseConnection(hError,hConnect);
  fp_ssdbDeleteClient(hError,hClient);
  fp_ssdbDeleteErrorHandle(hError);

  return err_flg ? 0 : 1;
}
/* ------------------------ initializeHostnameInfo ------------------------- */
static void initializeHostnameInfo(void)
{ if(gethostname(szLocalHostName,sizeof(szLocalHostName)) == INVALID_SOCKET_VAL) szLocalHostName[0] = 0;
#ifdef INCDEBUG
  dprintf("Local Host name: %s\n",szLocalHostName);
#endif
}
/* ====================== thread_func_readfilter =========================== */
void *thread_func_readfilter(void *thread_param)
{ int ret_code,timeout,lliberr_msgout,iliberr_msgout;
  struct timespec time_val;

  est.statRfCurrentLoadLibraryTimeout = TOUTBASE_LLIB;
  est.statRfCurrentInitLibraryTimeout = TOUTBASE_ILIB;
  lliberr_msgout = (iliberr_msgout = 0);

  pthread_mutex_lock(&start_readfilter_mutex.mutex); pthread_mutex_unlock(&start_readfilter_mutex.mutex);

  while(cTimeoutBeforeLoadConfig > 0 && !exit_phase)
  { sleep(1); cTimeoutBeforeLoadConfig--;
  }

  initializeHostnameInfo();

  for(ret_code = ETIMEDOUT;!exit_phase;)
  { timeout = 0;
    resetFilterInfoIsReadyFlag();
    dropDarkEventInfoList();
    dropWhiteEventInfoList();
    
#ifdef INCDEBUG
    dprintf("Start Loading Filter Info... LoadLib Timeout: %d Init Library Timeout: %d\n",
            est.statRfCurrentLoadLibraryTimeout,est.statRfCurrentInitLibraryTimeout);
#endif

    if(!load_ssdb_library(1))
    { timeout = est.statRfCurrentLoadLibraryTimeout;
      if(ret_code == ETIMEDOUT)
      { est.statRfCurrentLoadLibraryTimeout += TOUTPLUS_LLIB;
        if(est.statRfCurrentLoadLibraryTimeout > TOUTMAX_LLIB) est.statRfCurrentLoadLibraryTimeout = TOUTMAX_LLIB;
      }
      if(!lliberr_msgout) lliberr_msgout = syslog_msg(LOG_ERR,EVMSEQN_SSDBLOADERROR,szSSDBLibraryLoadError,lpszSSDBLibraryName,timeout);
    }
    else
    { est.statRfCurrentLoadLibraryTimeout = TOUTBASE_LLIB;
      lliberr_msgout = 0;

      if(load_filter_info())    
      { est.statRfCurrentInitLibraryTimeout = TOUTBASE_ILIB;
        setFilterInfoIsReadyFlag();
        iliberr_msgout = 0;
      }
      else
      { dropDarkEventInfoList();
        dropWhiteEventInfoList();
        timeout = est.statRfCurrentInitLibraryTimeout;
        if(ret_code == ETIMEDOUT)
        { est.statRfCurrentInitLibraryTimeout += TOUTPLUS_ILIB;
          if(est.statRfCurrentInitLibraryTimeout > TOUTMAX_ILIB) est.statRfCurrentInitLibraryTimeout = TOUTMAX_ILIB;
        }
        if(!iliberr_msgout) iliberr_msgout = syslog_msg(LOG_ERR,EVMSEQN_SSDBINITERROR,szSSDBLibraryInitError,lpszSSDBLibraryName,timeout);
      }
      /* Notify process filter thread, because filter info is ready */
      pthread_mutex_lock(&rcvbuf_queue_mutex.mutex);  pthread_cond_signal(&rcvnotempt_condition.cond);
      pthread_mutex_unlock(&rcvbuf_queue_mutex.mutex);
    }
    ret_code = EINVAL;
    pthread_mutex_lock(&reinit_filter_mutex.mutex);
    need_init_filter = 0;
    if(timeout)
    { memset(&time_val,0,sizeof(time_val));
      time_val.tv_sec = (time_t)(timeout)+time(NULL);
      while(!exit_phase && !need_init_filter && ret_code != ETIMEDOUT) 
       ret_code = pthread_cond_timedwait(&reinit_condition.cond,&reinit_filter_mutex.mutex,&time_val);
    }
    else
    { while(!exit_phase && !need_init_filter)
       pthread_cond_wait(&reinit_condition.cond,&reinit_filter_mutex.mutex);
    }
    pthread_mutex_unlock(&reinit_filter_mutex.mutex);
  } /* end of while(!exit_phase) */

  filterInfoIsReady = 0;
  dropDarkEventInfoList();
  dropWhiteEventInfoList();
  unload_ssdb_library(1);

#ifdef INCDEBUG
  dprintf("*** LoadFilter thread exit ***\n");
#endif
  return 0;
}

/* ======================== thread_func_killer ============================= */
void *thread_func_killer(void *thread_param)
{ pthread_mutex_lock(&start_killer_mutex.mutex); pthread_mutex_unlock(&start_killer_mutex.mutex);
  pthread_kill(rsock_tid,SIGTERM);
  pthread_kill(procf_tid,SIGTERM);
  pthread_kill(rfilter_tid,SIGTERM);
  pthread_kill(info_tid,SIGTERM);
  pthread_kill(output_tid,SIGTERM);
  pthread_kill(amticker_tid,SIGTERM);
  sleep(1);
  pthread_kill(rsock_tid,SIGKILL);
  pthread_kill(procf_tid,SIGKILL);
  pthread_kill(rfilter_tid,SIGKILL);
  pthread_kill(info_tid,SIGKILL);
  pthread_kill(output_tid,SIGKILL);
  pthread_kill(amticker_tid,SIGKILL);
#ifdef INCDEBUG
  dprintf("*** Killer thread exit ***\n");
#endif
  return 0;
}
/* -------------------------- send_test_request ---------------------------- */
static void send_test_request(void)
{ struct sockaddr_un su_addr;
  char buf[16];
  int s;

  if((s = socket(AF_UNIX, SOCK_DGRAM, 0)) >= 0)
  { sendto(s,buf,prepare_cmdbuffer(buf,EVMCMD_TEST),0,(struct sockaddr*)&su_addr,make_sockaddr(&su_addr,szEventMonInfoSockPath));
    close_socket(s);
  }
}
/* ---------------------------- send_test_event ---------------------------- */
static void send_test_event(void)
{ struct sockaddr_un su_addr;
  char buf[16];
  int s;

  if((s = socket(AF_UNIX, SOCK_DGRAM, 0)) >= 0)
  { sendto(s,buf,prepare_cmdbuffer(buf,EVMCMD_TEST),0,(struct sockaddr*)&su_addr,make_sockaddr(&su_addr,szInpSockName));
    close_socket(s);
  }
}
/* ----------------------------- process ----------------------------------- */
static void process(void)
{ pthread_mutex_unlock(&start_rsock_mutex.mutex);       /* Start RcvSocket thread */
  pthread_mutex_unlock(&start_info_mutex.mutex);        /* Start Info thread */
  pthread_mutex_unlock(&start_readfilter_mutex.mutex);  /* Start ReadFilter thread */
  pthread_mutex_unlock(&start_procfilter_mutex.mutex);  /* Start ProcFilter thread */
  pthread_mutex_unlock(&start_output_mutex.mutex);      /* Start Output thread */
  pthread_mutex_unlock(&start_amticker_mutex.mutex);    /* Start Amticker thread */  
  syslog_msg(LOG_INFO,EVMSEQN_STARTED,szStarted);

  pthread_mutex_lock(&exit_phase_mutex.mutex);
  while(!exit_phase) pthread_cond_wait(&exit_condition.cond,&exit_phase_mutex.mutex);
  pthread_mutex_unlock(&exit_phase_mutex.mutex);

  /* reanimate proc thread (send signal - "rcv queue is not empty") */
  pthread_mutex_lock(&rcvbuf_queue_mutex.mutex);
  pthread_cond_signal(&rcvnotempt_condition.cond);
  pthread_mutex_unlock(&rcvbuf_queue_mutex.mutex);

  /* reanimate load filter thread (send signal - "need reinit filter info" */
  pthread_mutex_lock(&reinit_filter_mutex.mutex);
  pthread_cond_signal(&reinit_condition.cond);
  pthread_mutex_unlock(&reinit_filter_mutex.mutex);

  /* reanimate amticker thread (send signal) */
  pthread_mutex_lock(&amticker_mutex.mutex);
  pthread_cond_signal(&amticker_condition.cond);
  pthread_mutex_unlock(&amticker_mutex.mutex);

  /* reanimate output thread (send signal - "output is not empty") */
  pthread_mutex_lock(&output_queue_mutex.mutex);
  pthread_cond_signal(&outnotempt_condition.cond); 
  pthread_mutex_unlock(&output_queue_mutex.mutex);

  /* reanimate info thread (send test request message) */
  send_test_request();

  /* reanimate rcv thread (send test event message) */
  send_test_event();

  pthread_mutex_unlock(&start_killer_mutex.mutex); /* Start killer thread */
  pthread_join(rsock_tid,NULL);    /* wait exit for rcvsocket thread */
  pthread_join(procf_tid,NULL);    /* wait exit for procfilter thread */
  pthread_join(rfilter_tid,NULL);  /* wait exit for readfilter thread */
  pthread_join(info_tid,NULL);     /* wait exit for info thread */
  pthread_join(output_tid,NULL);   /* wait exit for output thread */
  pthread_join(amticker_tid,NULL); /* wait exit for amticker thread */
  pthread_join(killer_tid,NULL);   /* wait exit for killer thread */
}
/* ------------------------ daemon_killer ---------------------------------- */
void daemon_killer(int sign)
{ if(inited_mutex && inited_cond)
  { pthread_mutex_lock(&exit_phase_mutex.mutex); exit_phase++; pthread_cond_signal(&exit_condition.cond);
    pthread_mutex_unlock(&exit_phase_mutex.mutex);
  }
  else exit_phase++;
#ifdef INCDEBUG
  dprintf("%s - Died signal (%d) received\n",szDaemonName,sign); /* FIXME */
#endif
  signal(sign,daemon_killer);
}
/* ------------------------ reinit_handler --------------------------------- */
void reinit_handler(int sign)
{ if(inited_mutex && inited_cond)
  { pthread_mutex_lock(&reinit_filter_mutex.mutex); need_init_filter++; pthread_cond_signal(&reinit_condition.cond);
    pthread_mutex_unlock(&reinit_filter_mutex.mutex);
  }
  else need_init_filter++;
#ifdef INCDEBUG
  dprintf("SIGHUP - %d, reinit flag = %d\n",sign,(int)need_init_filter);
#endif
  signal(sign,reinit_handler);
}
/* ---------------------- isr_illegal_instr -------------------------------- */
void isr_illegal_instr(int sign)
{ syslog_msg(LOG_ERR,EVMSEQN_ILLEGALCPUCMD,szIllegalCPUcommand);
#ifdef INCDEBUG
  dprintf(szIllegalCPUcommand);
#endif
  exit(EMERR_INVCPUCMD);
}
/* ----------------------- isr_fpe_error ----------------------------------- */
void isr_fpe_error(int sign)
{ syslog_msg(LOG_ERR,EVMSEQN_ILLEGALFPECMD,szIllegalFPEcommand);
#ifdef INCDEBUG
  dprintf(szIllegalFPEcommand);
#endif
  exit(EMERR_INVFPECMD);
}
/* ------------------------- daemon_init ----------------------------------- */
static void daemon_init(void)
{ pid_t pid;
  struct rlimit flim;
  int maxfiles,fd;

  /* Set new signal handlers */
  signal(SIGHUP,reinit_handler);                        /* Reinit different info (filter info, etc.) */
  signal(SIGTERM,daemon_killer);                        /* Process terminated */
  signal(SIGILL,isr_illegal_instr);                     /* Illegal CPU command */
  signal(SIGFPE,isr_fpe_error);                         /* Illegal FPE command */
  signal(SIGTTIN,SIG_IGN);                              /* Ignore: Attempt to read terminal from background process */
  signal(SIGTTOU,SIG_IGN);                              /* Ignore: Attempt to write terminal from background process */
  signal(SIGTSTP,SIG_IGN);                              /* Ignore: Stop key signal from controlling terminal */
#ifdef SIGXCPU
  signal(SIGXCPU,SIG_IGN);                              /* Ignore: CPU usage overload */
#endif
#ifdef SIGXFSZ
  signal(SIGXFSZ,SIG_IGN);                              /* Ignore: File size limit */
#endif
  signal(SIGINT,debug_mode ? daemon_killer : SIG_IGN);  /* Interrupt key signal from controlling terminal */
  signal(SIGQUIT,debug_mode ? daemon_killer : SIG_IGN); /* Quit key signal from controlling terminal */
  signal(SIGCHLD,SIG_IGN);                              /* Ignore: Child process status change */
  signal(SIGPIPE,SIG_IGN);                              /* Ignore: Read or write to broken pipe */
  (void)umask(S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP);

  if(!debug_mode)
  { if((pid = fork()) == (pid_t)(-1))
    { if(!silenceMode) fprintf(stderr,szErrorSystemCantFork3,szDaemonName,EMERR_SYSERRFORK,errno);
      exit(EMERR_SYSERRFORK);
    }
    if(pid) exit(EMERR_SUCCESS);

    setsid(); /* Set new group id */

    /* Get the max file handle number */    
    if(!getrlimit(RLIMIT_NOFILE,&flim)) maxfiles = flim.rlim_max;
    else                                maxfiles = 20;
    (void)chdir("/");
    for(fd = 0;fd < maxfiles;fd++) (void)close(fd); /* Close all files */
#ifdef TIOCNOTTY
    if((fd = open(_PATH_TTY,O_RDWR)) >= 0)
    { ioctl(fd,(int)TIOCNOTTY,0);
      (void)close(fd);
    }
#endif
  }
}
/* ------------------------ init_daemon_threads ---------------------------- */
static void init_daemon_threads(void)
{ /* Init RcvSocket thread */
  pthread_mutex_lock(&start_rsock_mutex.mutex);
  if(pthread_create(&rsock_tid,NULL,thread_func_readsocket,NULL))
  { exit_phase++; pthread_mutex_unlock(&start_rsock_mutex.mutex);
    syslog_msg(LOG_ERR,EVMSEQN_INITTHREADERR,szCantInitThread,"rcvsock"); exit(EMERR_INITTHREAD);
  }

  /* Init ProcessFilter thread */
  pthread_mutex_lock(&start_procfilter_mutex.mutex);
  if(pthread_create(&procf_tid,NULL,thread_func_procfilter,NULL))
  { exit_phase++; pthread_mutex_unlock(&start_procfilter_mutex.mutex);
    syslog_msg(LOG_ERR,EVMSEQN_INITTHREADERR,szCantInitThread,"procfilter"); exit(EMERR_INITTHREAD);
  }

  /* Init ReadFilter info thread */
  pthread_mutex_lock(&start_readfilter_mutex.mutex);
  if(pthread_create(&rfilter_tid,NULL,thread_func_readfilter,NULL))
  { exit_phase++; pthread_mutex_unlock(&start_readfilter_mutex.mutex);
    syslog_msg(LOG_ERR,EVMSEQN_INITTHREADERR,szCantInitThread,"readfilter"); exit(EMERR_INITTHREAD);
  }

  /* Init Info thread */
  pthread_mutex_lock(&start_info_mutex.mutex);
  if(pthread_create(&info_tid,NULL,thread_func_info,NULL))
  { exit_phase++; pthread_mutex_unlock(&start_info_mutex.mutex);
    syslog_msg(LOG_ERR,EVMSEQN_INITTHREADERR,szCantInitThread,"info"); exit(EMERR_INITTHREAD);
  }

  /* Init Output thread */
  pthread_mutex_lock(&start_output_mutex.mutex);
  if(pthread_create(&output_tid,NULL,thread_func_output,NULL))
  { exit_phase++; pthread_mutex_unlock(&start_output_mutex.mutex);
    syslog_msg(LOG_ERR,EVMSEQN_INITTHREADERR,szCantInitThread,"output"); exit(EMERR_INITTHREAD);
  }

  /* Init Amticker thread */
  pthread_mutex_lock(&start_amticker_mutex.mutex);
  if(pthread_create(&amticker_tid,NULL,thread_func_amticker,NULL))
  { exit_phase++; pthread_mutex_unlock(&start_amticker_mutex.mutex);
    syslog_msg(LOG_ERR,EVMSEQN_INITTHREADERR,szCantInitThread,"amticker"); exit(EMERR_INITTHREAD);
  }
  
  /* Init Killer thread */
  pthread_mutex_lock(&start_killer_mutex.mutex);
  if(pthread_create(&killer_tid,NULL,thread_func_killer,NULL))
  { exit_phase++; pthread_mutex_unlock(&start_killer_mutex.mutex);
    syslog_msg(LOG_ERR,EVMSEQN_INITTHREADERR,szCantInitThread,"killer"); exit(EMERR_INITTHREAD);
  }
}
/* -------------------- instance_check_lite -------------------------------- */
static void instance_check_lite(void)
{ char buf[16];
  int su_addrlen,s,l,len;
  struct sockaddr_un su_addr;

  su_addrlen = make_sockaddr(&su_addr,szEventMonInfoSockPath);

  if((s = socket(AF_UNIX, SOCK_DGRAM, 0)) >= 0)
  { l = sendto(s,buf,(len = prepare_cmdbuffer(buf,EVMCMD_TEST)),0,(struct sockaddr*)&su_addr,su_addrlen); close_socket(s);
    if(l == len) prev_instance++;
  }
}
/* ------------------------- instance_check -------------------------------- */
static void instance_check(void)
{ char buf[16];
  int su_addrlen,s,l,len;
  struct sockaddr_un su_addr;

  su_addrlen = make_sockaddr(&su_addr,szEventMonInfoSockPath);

  if((s = socket(AF_UNIX, SOCK_DGRAM, 0)) < 0)
  { if(!silenceMode) fprintf(stderr,szErrorCantOpenSocket3,szDaemonName,EMERR_CANTOPENSOCK,errno);
    exit(EMERR_CANTOPENSOCK);
  }
  len = prepare_cmdbuffer(buf,EVMCMD_TEST); /* Test message packet */

  l = sendto(s,buf,len,0,(struct sockaddr*)&su_addr,su_addrlen); close_socket(s);
  if(l == len)
  { if(!silenceMode) fprintf(stderr,szErrorAlreadyInstalled2,szDaemonName,EMERR_ALRINSTALLED);
    exit(EMERR_ALRINSTALLED);
  }

  unlink_file(szEventMonInfoSockPath);

  su_addrlen = make_sockaddr(&su_addr,szEventMonInfoSockPath);

  if((s = socket(AF_UNIX, SOCK_DGRAM, 0)) < 0)
  { if(!silenceMode) fprintf(stderr,szErrorCantOpenSocket3,szDaemonName,EMERR_CANTOPENSOCK,errno);
    exit(EMERR_CANTOPENSOCK);
  }

  l = bind(s,(struct sockaddr*)&su_addr,su_addrlen);
  if(l < 0)
  { if(!silenceMode) fprintf(stderr,szErrorCantBindSocket3,szDaemonName,EMERR_CANTBINDSOCK,errno);
    close_unlink(&s,szEventMonInfoSockPath);
    exit(EMERR_CANTBINDSOCK);
  }

  l = sizeof(su_addrlen); su_addrlen = 0;
  if(getsockopt(s,SOL_SOCKET,SO_RCVBUF,&su_addrlen,&l) < 0) su_addrlen = 0;
  if(su_addrlen < RCVSOCK_RCVBUFSIZE)
  { su_addrlen = RCVSOCK_RCVBUFSIZE;
    if(setsockopt(s,SOL_SOCKET,SO_RCVBUF,&su_addrlen,sizeof(su_addrlen)) < 0)
    { if(!silenceMode) fprintf(stderr,szErrorCantSetSockOpt3,szDaemonName,EMERR_CANTSSOCKOPT,errno);
      close_unlink(&s,szEventMonInfoSockPath);
      exit(EMERR_CANTSSOCKOPT);
    }
  }
  close_unlink(&s,szEventMonInfoSockPath);
}
/* -------------------------- check_osenv ---------------------------------- */
static void check_osenv(void)
{ int i;
  struct rlimit pthlim;
#ifdef RLIMIT_PTHREAD
  /* Check phtread limitation */
  if(!getrlimit(RLIMIT_PTHREAD,&pthlim))
  { if(pthlim.rlim_max < 6)
    { if(!silenceMode) fprintf(stderr,szErrorIncorrectOSRes2,szDaemonName,EMERR_INVOSPARAMS);
      exit(EMERR_INVOSPARAMS);
    }
  }
#endif
  /* Adjust number of temp files */
  if(!getrlimit(RLIMIT_NOFILE,&pthlim) && pthlim.rlim_max < maxTmpFileCnt) maxTmpFileCnt = pthlim.rlim_max;
  if(maxTmpFileCnt > EMSGFILE_MAXCNT) maxTmpFileCnt = EMSGFILE_MAXCNT;
  for(i = 0;i < 5 && maxTmpFileCnt;i++) maxTmpFileCnt--;
  if(preallocTmpFileCnt > maxTmpFileCnt) preallocTmpFileCnt = maxTmpFileCnt;
}

/* ---------------------- init_daemon_resources ---------------------------- */
static void init_daemon_resources(void)
{ /* init mutex resources */
  if(!init_dmutex(&start_rsock_mutex))      { syslog_msg(LOG_ERR,EVMSEQN_INITMUTEXERR,szCantInitMutex,"start_rsock"); exit(EMERR_INITDATA); }
  inited_mutex++;
  if(!init_dmutex(&start_killer_mutex))     { syslog_msg(LOG_ERR,EVMSEQN_INITMUTEXERR,szCantInitMutex,"start_killer"); exit(EMERR_INITDATA); }
  inited_mutex++;
  if(!init_dmutex(&start_procfilter_mutex)) { syslog_msg(LOG_ERR,EVMSEQN_INITMUTEXERR,szCantInitMutex,"start_procfilter"); exit(EMERR_INITDATA); }
  inited_mutex++;
  if(!init_dmutex(&start_readfilter_mutex)) { syslog_msg(LOG_ERR,EVMSEQN_INITMUTEXERR,szCantInitMutex,"start_readfilter"); exit(EMERR_INITDATA); }
  inited_mutex++;
  if(!init_dmutex(&start_info_mutex))       { syslog_msg(LOG_ERR,EVMSEQN_INITMUTEXERR,szCantInitMutex,"start_info"); exit(EMERR_INITDATA); }
  inited_mutex++;
  if(!init_dmutex(&start_output_mutex))     { syslog_msg(LOG_ERR,EVMSEQN_INITMUTEXERR,szCantInitMutex,"start_output"); exit(EMERR_INITDATA); }
  inited_mutex++;
  if(!init_dmutex(&start_amticker_mutex))   { syslog_msg(LOG_ERR,EVMSEQN_INITMUTEXERR,szCantInitMutex,"start_amticker"); exit(EMERR_INITDATA); }
  inited_mutex++;
  if(!init_dmutex(&rcvbuf_queue_mutex))     { syslog_msg(LOG_ERR,EVMSEQN_INITMUTEXERR,szCantInitMutex,"rcvbuf_queue"); exit(EMERR_INITDATA); }
  inited_mutex++;
  if(!init_dmutex(&output_queue_mutex))     { syslog_msg(LOG_ERR,EVMSEQN_INITMUTEXERR,szCantInitMutex,"output_queue"); exit(EMERR_INITDATA); }
  inited_mutex++;
  if(!init_dmutex(&emsgbuf_list_mutex))     { syslog_msg(LOG_ERR,EVMSEQN_INITMUTEXERR,szCantInitMutex,"emsgbuf_list"); exit(EMERR_INITDATA); }
  inited_mutex++;
  if(!init_dmutex(&efilter_list_mutex))     { syslog_msg(LOG_ERR,EVMSEQN_INITMUTEXERR,szCantInitMutex,"efilter_list"); exit(EMERR_INITDATA); }
  inited_mutex++;
  if(!init_dmutex(&exit_phase_mutex))       { syslog_msg(LOG_ERR,EVMSEQN_INITMUTEXERR,szCantInitMutex,"exit"); exit(EMERR_INITDATA); }
  inited_mutex++;
  if(!init_dmutex(&reinit_filter_mutex))    { syslog_msg(LOG_ERR,EVMSEQN_INITMUTEXERR,szCantInitMutex,"reinit_filter"); exit(EMERR_INITDATA); }
  inited_mutex++;
  if(!init_dmutex(&amticker_mutex))         { syslog_msg(LOG_ERR,EVMSEQN_INITMUTEXERR,szCantInitMutex,"amticker"); exit(EMERR_INITDATA); }
  inited_mutex++;

  /* init condition variables */
  if(!init_dcond(&rcvnotempt_condition))    { syslog_msg(LOG_ERR,EVMSEQN_INITCONDVARERR,szCantInitConVar,"rcvnotempt"); exit(EMERR_INITDATA); }
  inited_cond++;
  if(!init_dcond(&outnotempt_condition))    { syslog_msg(LOG_ERR,EVMSEQN_INITCONDVARERR,szCantInitConVar,"outnotempt"); exit(EMERR_INITDATA); }
  inited_cond++;
  if(!init_dcond(&exit_condition))          { syslog_msg(LOG_ERR,EVMSEQN_INITCONDVARERR,szCantInitConVar,"exit"); exit(EMERR_INITDATA); }
  inited_cond++;
  if(!init_dcond(&reinit_condition))        { syslog_msg(LOG_ERR,EVMSEQN_INITCONDVARERR,szCantInitConVar,"reinit"); exit(EMERR_INITDATA); }
  inited_cond++;
  if(!init_dcond(&amticker_condition))      { syslog_msg(LOG_ERR,EVMSEQN_INITCONDVARERR,szCantInitConVar,"amticker"); exit(EMERR_INITDATA); }
  inited_cond++;

  /* Initialize clock size value for SEM API */
  if((semapi_clockSize = sysconf(_SC_CLK_TCK)) != (-1)) semapi_clockSize /= 5;
  else semapi_clockSize = 0;

  /* prealloc EventMon MemoryBuffer message structures */
  freeEmsgList(allocEmsgList(preallocMemBufCnt));

  /* prealloc EventMon File message structures */
  freeEmsgfList(allocEmsgfList(preallocTmpFileCnt));

  /* prealloc Event Filter Info structures */
  freeEventInfoList(allocEventInfoList(EFINFOPREALLOC_CNT));
}
/* ---------------------- done_daemon_resources ---------------------------- */
static void done_daemon_resources(void)
{ /* Destroy all mutexes */
  if(inited_mutex)
  { inited_mutex = 0;
    destroy_dmutex(&start_rsock_mutex);
    destroy_dmutex(&start_killer_mutex);
    destroy_dmutex(&start_procfilter_mutex);
    destroy_dmutex(&start_readfilter_mutex);
    destroy_dmutex(&start_info_mutex);
    destroy_dmutex(&start_output_mutex);
    destroy_dmutex(&start_amticker_mutex);
    destroy_dmutex(&rcvbuf_queue_mutex);
    destroy_dmutex(&output_queue_mutex);
    destroy_dmutex(&emsgbuf_list_mutex);
    destroy_dmutex(&efilter_list_mutex);
    destroy_dmutex(&exit_phase_mutex);
    destroy_dmutex(&reinit_filter_mutex);
    destroy_dmutex(&amticker_mutex);
  }
  /* Destroy all condition variables */
  if(inited_cond)
  { inited_cond = 0;
    destroy_dcond(&rcvnotempt_condition);
    destroy_dcond(&outnotempt_condition);
    destroy_dcond(&exit_condition);
    destroy_dcond(&reinit_condition);
    destroy_dcond(&amticker_condition);
  }
  /* Remove all EventMon daemon temp files */
  removeAllTempFiles();
}
/* --------------------- syslogconf_check ---------------------------------- */
static void syslogconf_check(void)
{ char cline[4096];
  char *c,*p;
  FILE *cf;
  int find = 0;
  if((cf = fopen(szSyslogConfFname,"r")) == NULL) return;

  cline[sizeof(cline)-1] = 0;
  while(fgets(cline,sizeof(cline)-1,cf) != NULL)
  { if(cline[0] == '\n' || cline[0] == '#') continue;
    c = cline;
    while(*c == ' ' || *c == '\t') c++;
    if(*c == '\n' || *cline == '#') continue;    
    while(*c && *c != '\t') c++;
    while(*c && *c != EVENTMONSOCK_PREFIX) c++;
    if(*c++ == EVENTMONSOCK_PREFIX)
    { if(*c == EVENTMONSOCK_PREFIX)
      { for(p = ++c;*c && *c != '\n' && *c != '\r' && *c != ' ' && *c != '\t';c++);
        *c = 0;
        if(!strcmp(p,szInpSockName)) find++;
      }
    }
  }
  (void)fclose(cf);

  if(!find)
  { if(!silenceMode) fprintf(stderr,szWarningCantFindConfRec3,szDaemonName,szInpSockName,szSyslogConfFname);
    syslog_msg(LOG_WARNING,EVMSEQN_CANTFINDCONFSTR,szCantFindConfRec2,szInpSockName,szSyslogConfFname);
  }
  else
  { if(find > 1)
    { if(!silenceMode) fprintf(stderr,szWarningTooManyConfRec3,szDaemonName,szInpSockName,szSyslogConfFname);
      syslog_msg(LOG_WARNING,EVMSEQN_TOOMANYCONFSTR,szTooManyConfRec2,szInpSockName,szSyslogConfFname);
    }
  }
}
/* ----------------------- clear_temps ------------------------------------- */
static void clear_temps(void)
{ char tmp[2048];
  char pidstr[1024];
  struct dirent *direntp;
  struct stat fs;
  DIR *dirp;
  int i,pid,masklen = strlen(szTempSockNameMask);

  removeAllTempFiles();

  if((dirp = opendir("/tmp")) != NULL)
  { while((direntp = readdir(dirp)) != NULL) 
    { if(((i = strlen(direntp->d_name)) > masklen) && (i < (sizeof(tmp)+masklen)))
      { sprintf(tmp,"/tmp/%s",direntp->d_name);
        if(!stat(tmp,&fs) && S_ISSOCK(fs.st_mode))
        { if(!strncmp(direntp->d_name,szTempSockNameMask,masklen))
          { for(i = 0;i < sizeof(pidstr)-1;i++)
            { if(!isxdigit(direntp->d_name[masklen+i])) break;
              pidstr[i] = direntp->d_name[masklen+i];
            }
            pidstr[i] = 0;
            if(i && sscanf(pidstr,"%x",&pid))
            { if(kill((pid_t)pid,0) < 0 && !unlink(tmp)) clearTmpFileCnt++;
            }
          }
        }
      }
    }
    closedir(dirp); 
  }
}          
/* -------------------------- isdigit_str ---------------------------------- */
static int isdigit_str(char *buf)
{ int retcode;
  for(retcode = 1;*buf && retcode;buf++)
   if(!isdigit(*buf)) retcode = 0;
  return retcode;
}
/* ---------------------- setmax_tempfile_limit ---------------------------- */
static void setmax_tempfile_limit(char *_optarg)
{ if(_optarg && _optarg[0])
  { maxTmpFileCnt = atoi(_optarg);
    if(!isdigit_str(_optarg) || maxTmpFileCnt > EMSGFILE_MAXCNT || maxTmpFileCnt < 0)
    { if(!silenceMode) fprintf(stderr,szErrorInvalidParameter3,szDaemonName,EMERR_INVPARAM,_optarg);
      exit(EMERR_INVPARAM);
    }
    if(preallocTmpFileCnt > maxTmpFileCnt) preallocTmpFileCnt = maxTmpFileCnt;
  }
}
/* ---------------------- setmax_membuf_limit ------------------------------ */
static void setmax_membuf_limit(char *_optarg)
{ if(_optarg && _optarg[0])
  { maxMemBufCnt = atoi(_optarg);
    if(!isdigit_str(_optarg) || maxMemBufCnt > EMSGALLOCMAX_CNT || maxMemBufCnt < 0)
    { if(!silenceMode) fprintf(stderr,szErrorInvalidParameter3,szDaemonName,EMERR_INVPARAM,_optarg);
      exit(EMERR_INVPARAM);
    }
    if(preallocMemBufCnt > maxMemBufCnt) preallocMemBufCnt = maxMemBufCnt;
  }
}
/* ------------------------ set_dblib_name --------------------------------- */
static void set_dblib_name(char *_optarg)
{ if(_optarg && _optarg[0])
  { lpszSSDBLibraryName = _optarg;
    if(!load_ssdb_library(0))
    { if(!silenceMode) fprintf(stderr,szWarningCantLoadDbLib2,szDaemonName,lpszSSDBLibraryName);
    }
    else unload_ssdb_library(0);
  }
}
/* ----------------------- set_database_name ------------------------------- */
static void set_database_name(char *_optarg)
{ if(_optarg && _optarg[0])
  { lpszDatabaseName = _optarg;
  }
}
/* ------------------------ set_table_name --------------------------------- */
static void set_table_name(char *_optarg)
{ if(_optarg && _optarg[0])
  { lpszDbTableName = _optarg;
  }
}
/* ---------------------- set_tryuseout_reptcnt ---------------------------- */
static void set_tryuseout_reptcnt(char *_optarg)
{ if(_optarg && _optarg[0])
  { cTryUseOutputApiMax = atoi(_optarg);
    if(!isdigit_str(_optarg) || cTryUseOutputApiMax < 0)
    { if(!silenceMode) fprintf(stderr,szErrorInvalidParameter3,szDaemonName,EMERR_INVPARAM,_optarg);
      exit(EMERR_INVPARAM);
    }
  }
}
/* --------------------- set_loadconf_timeout ------------------------------ */
static void set_loadconf_timeout(char *_optarg)
{ if(_optarg && _optarg[0])
  { cTimeoutBeforeLoadConfig = atoi(_optarg);
    if(!isdigit_str(_optarg) || cTimeoutBeforeLoadConfig < 0)
    { if(!silenceMode) fprintf(stderr,szErrorInvalidParameter3,szDaemonName,EMERR_INVPARAM,_optarg);
      exit(EMERR_INVPARAM);
    }
  }
}
/* -------------------- set_ssstag_enable_mode ----------------------------- */
static void set_ssstag_enable_mode(char *_optarg)
{ if(_optarg && _optarg[0])
  { if(!strcasecmp(_optarg,szOn) || !strcmp(_optarg,"1")) enable_ssstag = 1;
    else if(!strcasecmp(_optarg,szOff) || !strcmp(_optarg,"0")) enable_ssstag = 0;
    else
    { if(!silenceMode) fprintf(stderr,szErrorInvalidParameter3,szDaemonName,EMERR_INVPARAM,_optarg);
      exit(EMERR_INVPARAM);
    }
  }
}
#ifdef INCDEBUG
/* ----------------------- declare_eventlogfile ---------------------------- */
static int declare_eventlogfile(char *_optarg)
{ FILE *fp;
  int retcode = 0;
  if((lpszEventLogFilename = _optarg) != 0)
  { (void)unlink(lpszEventLogFilename);
    if((fp = fopen(lpszEventLogFilename,"w")) == NULL)
    { if(!silenceMode) fprintf(stderr,szErrorCantOpenOutputFile4,szDaemonName,EMERR_INVPARAM,errno,lpszEventLogFilename);
      lpszEventLogFilename = 0;
    }
    else { (void)fclose(fp); retcode++; }
  }
  return retcode;
}
#endif
/* -------------------- print_daemon_statistics ---------------------------- */
static void print_daemon_statistics(void)
{ char tmpsockname[512];
  char buf[RCVINFOREQ_MAXSIZE+1];
  int su_addrlen,cu_addrlen,s,l,len;
  struct sockaddr_un su_addr,cu_addr;
  struct timeval timeout;
  fd_set fdset;

  if(!sprintf(tmpsockname,szTempSockTemplate,(unsigned long)getpid()))
  { if(!silenceMode) fprintf(stderr,szErrorCantMakeTempFile2,szDaemonName,EMERR_CANTMAKETEMP);
    exit(EMERR_CANTMAKETEMP);
  }

  cu_addrlen = make_sockaddr(&cu_addr,tmpsockname);

  if((s = socket(AF_UNIX, SOCK_DGRAM, 0)) < 0)
  { if(!silenceMode) fprintf(stderr,szErrorCantOpenSocket3,szDaemonName,EMERR_CANTOPENSOCK,errno);
    exit(EMERR_CANTOPENSOCK);
  }

  unlink_file(tmpsockname);

  if(bind(s,(struct sockaddr*)&cu_addr,cu_addrlen) < 0)
  { if(!silenceMode) fprintf(stderr,szErrorCantBindSocket3,szDaemonName,EMERR_CANTBINDSOCK,errno);
    close_unlink(&s,tmpsockname);
    exit(EMERR_CANTBINDSOCK);
  }

  l = sizeof(su_addrlen); su_addrlen = 0;
  if(getsockopt(s,SOL_SOCKET,SO_RCVBUF,&su_addrlen,&l) < 0) su_addrlen = 0;
  if(su_addrlen < RCVINFOREQ_MAXSIZE)
  { su_addrlen = RCVINFOREQ_MAXSIZE;
    if(setsockopt(s,SOL_SOCKET,SO_RCVBUF,&su_addrlen,sizeof(su_addrlen)) < 0)
    { if(!silenceMode) fprintf(stderr,szErrorCantSetSockOpt3,szDaemonName,EMERR_CANTSSOCKOPT,errno);
      close_unlink(&s,tmpsockname);
      exit(EMERR_CANTSSOCKOPT);
    }
  }

  len = prepare_cmdbuffer(buf,EVMCMD_STAT); /* Stat command */
  su_addrlen = make_sockaddr(&su_addr,szEventMonInfoSockPath);

  if((l = sendto(s,buf,len,0,(struct sockaddr*)&su_addr,su_addrlen)) != len)
  { if(!silenceMode) fprintf(stderr,szErrorDaemonNotInstalled3,szDaemonName,EMERR_NOTINSTALLED,errno);
    close_unlink(&s,tmpsockname);
    exit(EMERR_NOTINSTALLED);
  }

  FD_ZERO(&fdset); FD_SET(s,&fdset);
  timeout.tv_sec = 3; timeout.tv_usec = 0;

  for(;;)
  { l = select((int)s+1,&fdset,0,0,&timeout);
    if(l < 0 && errno == EINTR) continue;
    break;
  }

  if(l <= 0 || !FD_ISSET(s,&fdset))
  { if(!silenceMode) fprintf(stderr,szErrorDaemonNotInstalled3,szDaemonName,EMERR_NOTINSTALLED,errno);
    close_unlink(&s,tmpsockname);
    exit(EMERR_NOTINSTALLED);
  }

  memset(&su_addr,0,sizeof(su_addr)); su_addrlen = sizeof(su_addr);
  for(;;)
  { l = recvfrom(s,buf,RCVINFOREQ_MAXSIZE,0,(struct sockaddr*)&su_addr,&su_addrlen);
    if(l < 0 && errno == EINTR) continue;
    break;
  }
  close_unlink(&s,tmpsockname);
  if(l >= 4 && buf[0] == 'L' && buf[1] == 'V' && buf[2] == 'A' && buf[3] == EVMCMD_STAT)
  { buf[l] = 0; puts(&buf[4]);
    exit(EMERR_SUCCESS);
  }
  if(!silenceMode) fprintf(stderr,szErrorDaemonNotInstalled3,szDaemonName,EMERR_NOTINSTALLED,errno);
  exit(EMERR_NOTINSTALLED);
}
/* --------------------- declare_something_string -------------------------- */
static void declare_something_string(char cmd,char *_string)
{ char buf[2048];
  int s,l,len;
  struct sockaddr_un su_addr;

  if((s = socket(AF_UNIX, SOCK_DGRAM, 0)) < 0)
  { if(!silenceMode) fprintf(stderr,szErrorCantOpenSocket3,szDaemonName,EMERR_CANTOPENSOCK,errno);
    exit(EMERR_CANTOPENSOCK);
  }
  
  len = prepare_cmdbuffer(buf,cmd); /* prepare command buffer */
  if(_string && _string[0]) len += snprintf(&buf[len],sizeof(buf)-len,"%s",_string);

  l = sendto(s,buf,len,0,(struct sockaddr*)&su_addr,make_sockaddr(&su_addr,szEventMonInfoSockPath)); close_socket(s);
  if(l != len)
  { if(!silenceMode) fprintf(stderr,szErrorDaemonNotInstalled3,szDaemonName,EMERR_NOTINSTALLED,errno);
    exit(EMERR_NOTINSTALLED);
  }
  exit_phase++;
}
/* ------------------------ declare_something ------------------------------ */
static void declare_something(char cmd)
{ declare_something_string(cmd,NULL);
}
/* --------------------- set_group_enable_mode ----------------------------- */
static void set_group_enable_mode(char *_optarg)
{ if(_optarg && _optarg[0])
  { if(!strcmp(_optarg,szOn) || !strcasecmp(_optarg,szOn) || !strcmp(_optarg,"1")) enable_group_mode = 1;
    else if(!strcmp(_optarg,szOff) || !strcasecmp(_optarg,szOff) || !strcmp(_optarg,"0")) enable_group_mode = 0;
    else
    { if(!silenceMode) fprintf(stderr,szErrorInvalidParameter3,szDaemonName,EMERR_INVPARAM,_optarg);
      exit(EMERR_INVPARAM);
    }
    if(prev_instance) declare_something(enable_group_mode ? EVMCMD_SETGRP : EVMCMD_CLRGRP);
  }
}
/* --------------------- declare_amtickerd_mode ---------------------------- */
static void declare_amtickerd_mode(char *_optarg)
{ enable_amticker = (-1);
  if(_optarg && _optarg[0])
  { if(!strcmp(_optarg,szOn) || !strcasecmp(_optarg,szOn) || !strcmp(_optarg,"1")) enable_amticker = 1;
    else if(!strcmp(_optarg,szOff) || !strcasecmp(_optarg,szOff) || !strcmp(_optarg,"0")) enable_amticker = 0;
  }
  if(enable_amticker < 0)
  { if(!silenceMode) fprintf(stderr,szErrorInvalidParameter3,szDaemonName,EMERR_INVPARAM,_optarg);
    exit(EMERR_INVPARAM);
  }
  if(prev_instance) declare_something(enable_amticker ? EVMCMD_AMTON : EVMCMD_AMTOFF); /* declare amtickerd on/off */
}
/* --------------------- set_amticker_tickfname ---------------------------- */
static void set_amticker_tickfname(char *_optarg)
{ if((lpszAmtickerFile = _optarg)[0] != '/')
  { if(!silenceMode) fprintf(stderr,szErrorInvalidParameter3,szDaemonName,EMERR_INVPARAM,_optarg);
    exit(EMERR_INVPARAM);
  }
}
/* ------------------ declare_amticker_ticktimeout ------------------------- */
static void declare_amticker_ticktimeout(char *_optarg)
{ char tmp[64];
  amticker_timeout = (-1);
  if(_optarg && _optarg[0])
  { amticker_timeout = atoi(_optarg);
    if(!isdigit_str(_optarg) || amticker_timeout <= 0)
    { if(!silenceMode) fprintf(stderr,szErrorInvalidParameter3,szDaemonName,EMERR_INVPARAM,_optarg);
      exit(EMERR_INVPARAM);
    }
    if(prev_instance)
    { snprintf(tmp,sizeof(tmp),"%d",amticker_timeout);
      declare_something_string(EVMCMD_SETTICK,tmp); /* declare amtickerd tick interval */
    }
  }
  if(amticker_timeout == (-1))
  { if(!silenceMode) fprintf(stderr,szErrorInvalidParameter3,szDaemonName,EMERR_INVPARAM,"tick interval");
    exit(EMERR_INVPARAM);
  }
}
/* ------------------ declare_amticker_stattimeout ------------------------- */
static void declare_amticker_stattimeout(char *_optarg)
{ char tmp[64];
  amticker_statustime = (-1);
  if(_optarg && _optarg[0])
  { amticker_statustime = atoi(_optarg);
    if(!isdigit_str(_optarg) || amticker_statustime < 0)
    { if(!silenceMode) fprintf(stderr,szErrorInvalidParameter3,szDaemonName,EMERR_INVPARAM,_optarg);
      exit(EMERR_INVPARAM);
    }
    if(prev_instance)
    { snprintf(tmp,sizeof(tmp),"%d",amticker_statustime);
      declare_something_string(EVMCMD_SETSTAT,tmp); /* declare amtickerd status interval */
    }
  }
  if(amticker_statustime == (-1))
  { if(!silenceMode) fprintf(stderr,szErrorInvalidParameter3,szDaemonName,EMERR_INVPARAM,"status interval");
    exit(EMERR_INVPARAM);
  }
}
/* ---------------------- declare_daemon_stop ------------------------------ */
static void declare_daemon_stop(void)
{ declare_something(EVMCMD_STOP); /* declare 'STOP' */
}
/* ------------------- declare_reload_filterinfo --------------------------- */
static void declare_reload_filterinfo(void)
{ declare_something(EVMCMD_RFILT); /* declare Reload Filter Info */
}
/* --------------------- declare_refresh_output ---------------------------- */
static void declare_refresh_output(void)
{ declare_something(EVMCMD_POUT); /* declare Refresh output */
}
/* ------------------------- print_usage ----------------------------------- */
static void print_usage(int exit_code)
{ if(!silenceMode)
  { fprintf(stderr,"Usage:\n");
    fprintf(stderr,privModeFlag ?
#ifdef INCDEBUG
    "%s [-a on|off] [-u timestamp_fname] [-j tout_sec] [-e tout_hours] [-o eventlogfile]\n\
[-d] [-q] [-r] [-l] [-f maxfiles] [-b maxbuffers] [-t tablename] [-n dbname]\n\
[-w dblib_name] [-m maxreptcnt] [-x on|off] [-p pause_sec] [-i] [-v] [-c] [-h]\n" : "%s [-i] [-v] [-c] [-h]\n",szDaemonName);
#else
    "%s [-a on|off] [-u timestamp_fname] [-j tout_sec] [-e tout_hours] [-q] [-r] [-l]\n\
[-f maxfiles] [-b maxbuffers] [-t tablename] [-n dbname] [-w dblib_name]\n\
[-m maxreptcnt] [-x on|off] [-p pause_sec] [-i] [-v] [-c] [-h]\n" : "%s [-i] [-v] [-c] [-h]\n",szDaemonName);
#endif
    if(privModeFlag)
    {
#ifdef INCDEBUG
      fprintf(stderr," -o - use \"eventlogfile\" for saving all incoming events\n");
      fprintf(stderr," -d - start in \"Debug\" mode\n");
#endif
      fprintf(stderr," -a - set \"on/off\" for availmonitoring functionality (default: \"%s\")\n",enable_amticker ? szOn : szOff);
      fprintf(stderr," -u - set \"time stamp\" file name for availmonitoring (default: \"%s\")\n",lpszAmtickerFile);
      fprintf(stderr," -j - set timeout for refresh \"time stamp file\" for availmonitoring (default: %d sec.)\n",amticker_timeout);
      fprintf(stderr," -e - set \"exec amdiag status\" timeout for availmonitoring (default: %d hour(s), 0 - disable)\n",amticker_statustime);
      fprintf(stderr," -q - unload eventmon from memory (-stop or -kill)\n");
      fprintf(stderr," -r - declare \"reload events filter info\"\n");
      fprintf(stderr," -l - declare reset timeout for \"output subsystem\" (refresh output)\n");
      fprintf(stderr," -f - set max number of temp files for saved messages (no more than: %d)\n",EMSGFILE_MAXCNT);
      fprintf(stderr," -b - set max number of memory buffers for saved messages (no more than: %d)\n",EMSGALLOCMAX_CNT);
      fprintf(stderr," -n - set event filter info \"dbname\" (default: \"%s\")\n",szDatabaseName);
      fprintf(stderr," -t - set event filter info \"tablename\" (default: \"%s\")\n",szDbTableName);
      fprintf(stderr," -w - set database support DSO module name and optional path (default: \"%s\")\n",szSSDBLibraryName);
      fprintf(stderr," -m - set max repeat counter for \"try use output library\" (default: %d), 0 - unlimited number of attempts\n",cTryUseOutputApiMax);
      fprintf(stderr," -x - set enable/disable \"ESP tag\" - (|$) output in syslog (default: \"%s\")\n",enable_ssstag ? szOn : szOff);
      fprintf(stderr," -p - set \"load configuration information from database\" timeout (default: %d sec.)\n",cTimeoutBeforeLoadConfig);
      fprintf(stderr," -g - set enable/disable \"group mode\" - enable transparent event forwarding for remote hosts (default: \"%s\")\n",enable_group_mode ? szOn : szOff);
    }
      fprintf(stderr," -i - print eventmon statistics and exit (-status or -state)\n");
      fprintf(stderr," -v - print version information and exit\n");
      fprintf(stderr," -c - print copyright information and exit\n");
      fprintf(stderr," -h - print \"help\" information and exit\n");
      fprintf(stderr," -silence - suppress all \"info\"/\"warning\"/\"error\" output (default: silence mode is \"%s\")\n",silenceMode ? szOn : szOff);
  }
  exit(exit_code);
}
/* ------------------------ print_version ---------------------------------- */
static void print_version(void)
{ if(!silenceMode) fprintf(stderr,"%s %d.%d (%s mode)\n",szDaemonName,EVENTMOND_VMAJOR,EVENTMOND_VMINOR,privModeFlag ? "priv" : "user");
  exit(EMERR_SUCCESS);
}
/* ----------------------- print_copyright --------------------------------- */
static void print_copyright(void)
{ if(!silenceMode) fprintf(stderr,"%s, %s\n",szDaemonName,szCopyright); exit(EMERR_SUCCESS);
}
/* ------------------------ parse_parameters ------------------------------- */
static void parse_parameters(int argc,char *argv[])
{ char tmp[1024];
  int c,i,err = 0;
#ifdef INCDEBUG
  char *cmdkey_string = privModeFlag ? "hHcCvViIqQrRdDLlA:a:U:u:J:j:E:e:G:g:x:X:o:O:f:F:b:B:s:S:k:K:n:N:t:T:w:W:u:U:m:M:p:P:" : "hHcCvViIs:S:";
#else
  char *cmdkey_string = privModeFlag ? "hHcCvViIqQrRLlA:a:U:u:J:j:E:e:G:g:x:X:f:F:b:B:s:S:k:K:n:N:t:T:w:W:u:U:m:M:p:P:" : "hHcCvViIs:S:";
#endif
  while((c = getopt(argc,argv,cmdkey_string)) != (-1) && !err)
  { switch(c) {
    case 'A' : /* Only in Priv mode */
    case 'a' : declare_amtickerd_mode(optarg);
               break;
    case 'U' : /* Only in Priv mode */
    case 'u' : set_amticker_tickfname(optarg);
               break;
    case 'J' : /* Only in Priv mode */
    case 'j' : declare_amticker_ticktimeout(optarg);
               break;
    case 'E' : /* Only in Priv mode */
    case 'e' : declare_amticker_stattimeout(optarg);
               break;    
#ifdef INCDEBUG
    case 'D' : /* Only in Priv mode */
    case 'd' : debug_mode++;
               break;
#endif
    case 'F' : /* Only in Priv mode */
    case 'f' : setmax_tempfile_limit(optarg);
               break;
    case 'B' : /* Only in Priv mode */
    case 'b' : setmax_membuf_limit(optarg);
               break;
    case 'C' :
    case 'c' : print_copyright();
               break;
    case 'G' : /* Only in Priv mode */
    case 'g' : set_group_enable_mode(optarg);
               break;
#ifdef INCDEBUG
    case 'O' : /* Only in Priv mode */
    case 'o' : if(!declare_eventlogfile(optarg)) err++;
               break;
#endif
    case 'I' :
    case 'i' : print_daemon_statistics();
               break;
    case 'K' : /* Only in Priv mode */
    case 'k' : if(optarg && privModeFlag)
               { for(i = 0;i < sizeof(tmp)-1 && optarg[i];i++) tmp[i] = tolower((int)optarg[i]);
                 tmp[i] = 0;
                 if(!strcmp("ill",tmp)) { declare_daemon_stop(); break; }
               }
               err++;
               break;
    case 'M' : /* Only in Priv mode */
    case 'm' : set_tryuseout_reptcnt(optarg);
               break;
    case 'N' : /* Only in Priv mode */
    case 'n' : set_database_name(optarg);
               break;
    case 'T' : /* Only in Priv mode */
    case 't' : set_table_name(optarg);
               break;
    case 'S' :
    case 's' : if(optarg)
               { for(i = 0;i < sizeof(tmp)-1 && optarg[i];i++) tmp[i] = tolower((int)optarg[i]);
                 tmp[i] = 0;
                      if(!strcmp("top",tmp) && privModeFlag)          { declare_daemon_stop(); break; }
                 else if(!strcmp("tatus",tmp) || !strcmp("tate",tmp)) { print_daemon_statistics(); break; }
                 else if(!strcmp("gi",tmp))                           { print_copyright(); break; }
                 else if(!strcmp("tart",tmp) && privModeFlag)           break;
                 else if(!strcmp("ilence",tmp))                       { silenceMode++; break; }
               }
               err++;
               break;

    case 'Q' : /* Only in Priv mode */
    case 'q' : declare_daemon_stop();
               break;
    case 'P' : /* Only in Priv mode */
    case 'p' : set_loadconf_timeout(optarg);
               break;
    case 'R' : /* Only in Priv mode */
    case 'r' : declare_reload_filterinfo();
               break;
    case 'L' : /* Only in Priv mode */
    case 'l' : declare_refresh_output();
               break;
    case 'V' :
    case 'v' : print_version();
               break;
    case 'W' : /* Only in Priv mode */
    case 'w' : set_dblib_name(optarg);
               break;
    case 'H' :
    case 'h' : print_usage(EMERR_SUCCESS);
               break;
    case 'X' : /* Only in Priv mode */
    case 'x' : set_ssstag_enable_mode(optarg);
               break;
    default  : err++;
               break;
    }; /* end of switch(c) */
  }
  if(err || (argc != optind)) print_usage(EMERR_INVPARAM);

  if(exit_phase) exit(EMERR_SUCCESS);

  if(!privModeFlag)
  { if(!silenceMode) fprintf(stderr,szErrorCantLoadInNonRoot,szDaemonName,EMERR_INVACCOUNT);
    exit(EMERR_INVACCOUNT);
  }
}

/* ------------------------------ main ------------------------------------- */
int main(int argc,char *argv[])
{ /* Initialize all static variables and buffers */
  zinit_resources();

  /* Check previous instance (lite version) */
  instance_check_lite();

  /* Parse command string parameters */
  parse_parameters(argc,argv);

  /* Check previous instance (heavy check before daemonize) */
  instance_check();

  /* Check syslog daemon configuration file */
  syslogconf_check();

  /* Check operation system parameters and adjust some internal variables */
  check_osenv();
  
  /* Remove all temp files from previous instance */
  clear_temps();

  /* Daemonize instance */
  daemon_init();

  /* Initialize mutex/conditions and some additional stuff */
  init_daemon_resources();

  /* Initialize threads */
  init_daemon_threads();

  /* Wait termination */
  process();

  /* Free resources and clear temp files */
  done_daemon_resources();

  /* Send final message to syslog daemon */
  syslog_msg(LOG_INFO,EVMSEQN_STOPED,szStopped);

  return EMERR_SUCCESS;
}
