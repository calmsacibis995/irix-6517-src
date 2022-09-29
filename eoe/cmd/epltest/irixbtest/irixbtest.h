/*
 * Copyright 1991, Silicon Graphics, Inc. 
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or 
 * duplicated in any form, in whole or in part, without the prior written 
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions 
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or 
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished - 
 * rights reserved under the Copyright Laws of the United States.
 */

#ifndef	IRIXBTEST_H__
#define	IRIXBTEST_H__

/*
 * irixbtest.h -- definitions for irixbtest.
 */
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/mac.h>
#include <sys/mac_label.h>
#include <sys/acl.h>
#include <sys/capability.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/prctl.h>
#include <signal.h>
#include <getopt.h>
#include "config_bits.h"
#include "debug.h"

/* irix5 definitions */
#define setlabel    _sgi_setlabel
#define getlabel    _sgi_getlabel
#define setplabel   _sgi_setplabel
#define getplabel   _sgi_getplabel
#define setpsoacl   _sgi_setpsoacl
#define recvlfrom   _recvlfrom

/* Miscellaneous defines */
#ifndef TRUE  
typedef	enum {
	TRUE,
	FALSE
	} compare;
#endif
#ifndef PASS  
typedef	enum {
	PASS,
	FAIL,
	INCOMPLETE
	} case_success;
#endif

#define NULSTR          (char *)0
#define BEL             '\007'
#define SLEN           25   /* Length of various small strings. */
#define NLEN           30   /* Length of temp file and dir names. */
#define MSGLEN        255   /* Length of various large strings. */
#define RBUFLEN      1024   /* Length of various read buffers. */

/* ACL access defs */
#define A_rwx	(ACL_READ|ACL_WRITE|ACL_EXECUTE)
#define A_rw	(ACL_READ|ACL_WRITE)
#define A_rx	(ACL_READ|ACL_EXECUTE)
#define A_wx	(ACL_WRITE|ACL_EXECUTE)
#define A_r	ACL_READ
#define A_w	ACL_WRITE
#define A_x	ACL_EXECUTE

/* Test checks */
#define CALL_PASSED(a) ((a) == 0)  
#define CALL_FAILED(a) ((a) == -1)
#define UNKNOWN_FAILURE(a) ((a) >= 1)

/* UIDS */
#define SUPER           0
#define TEST0           900
#define TEST1           901
#define TEST2           902
#define TEST3           903

/* GROUPS */
#define TESTG0          995
#define TESTG1          997
#define TESTG2          998
/* Do not change ID */
#define UNCHG		-1

/* usr and grp ids */
#define UIDbase	700
#define GIDbase	800
#define	testUID(off) (UIDbase+(off))
#define	testGID(off) (GIDbase+(off))

/* Function prototypes */
extern void flush_sum_log(void);
extern void flush_raw_log(void);
extern void w_sum_log(const char *);
extern void w_raw_log(const char *);
extern void w_error(int, const char *, const char *, int numarg);
extern char **get_envp(void);

/* Error handling */
extern char *err[];  /* Defined in errormsg.c */

/* errtype argument passed to w_error() (defined in irixbtest.c) */
typedef	enum {
	SYSCALL,
	GENERAL,
	PRINTNUM
	} error_types;

/* 
 * The following defines are *err[] indices. 
 */

/* Errors in command line, input file, or menu choice */
typedef	enum {
	CMDLINE,
	S_OPTION,
	MISSINGOPT,
	TESTNOTSUP,
	MENUCHOICE,

/* Other fatal errors in driver */
	UIDNOTSU,
	TIMESTRING,
	GETCDLABEL,
	BADCDLABEL,
	STATDIR,
	BADCDMODE,
	BADSTATMO,
	BADQMULTI,

/* Generic errors: */
	FILEOPEN,
	FUNCTION,
	F_CREAT,
	F_MKDIR,
	F_WRITE,
	F_MSGGET,

/* Label errors: */   
	F_SETPLABEL,
	F_CREPLABEL,
	SETLABEL_DIR,
	SETLABEL_FILE,
	BADLABEL,

/* Capability errors: */   
	CAPSETPROC,
	CAPSETACL,
	CAPACQUIRE,

/* Child processes */
	F_FORK,
	F_WAIT,
	C_NOTSTOP,
	C_NOTEXIT,
	C_BADEXIT,
	C_NOTKILLED,

/* Pipes */
	F_PIPE,
	PIPEREAD,
	PIPEREAD_O,
	PIPEREAD_S,
	PIPEWRITE,
	PIPEWRITE_O,
	PIPEWRITE_S,

/* UIDs */
	SETREUID_O,
	SETREUID_S,
	SETUID_S,
	SETUID_O,
	BADUIDS_S,
	BADUIDS_O,
	SUID_SGID,

/* GIDs */
	SETREGID_O,
	SETREGID_S,
	BADGIDS_S,
	BADGIDS_O,
   
/* PGIDs and SIDs */
	BADPGID,
	F_BSDSETPGRP,
	F_SETSID,
  
/* Test call error */
	TESTCALL,
	TEST_ERRNO,
	TEST_RETVAL,
	TEST_UNEXSUCC,

/* Socket stuff */
	F_SOCKET,
	F_BIND,
	F_GETSOCKNAME,
	F_LISTEN,
	F_ACCEPT,
	F_CONNECT,
	F_WRITES0,
	F_READSO,
	F_SENDTO,
	F_RECVFROM,
	F_RECV,
	F_RECVLFROM,
	F_RECVL,
	F_SELECT,
	T_SELECT,
	U_SELECT,
	F_IOCTL,
	F_IOCTLGETLBL,
	F_IOCTLSETLBL,
	F_SYSIFLABEL,
	BADSOCLABEL,
	F_GETLABEL_S,
	F_IOCTLSETACL,
	F_SETPSOACL,
	F_IOCTLGETACL,
	F_IOCTLGETUID,
	BADSOCUID,
	BADACLCOUNT,
	BADACLVAL,
	F_IOCTLGIFUID,
	F_IOCTLGIFLABEL,
	BADIFUID,
	BADIFLABEL,
	
/* Miscellaneous */
	F_EXEC,
	BADRDUBLK,
	BADRDNAME,
	BADPROCSZ,
	BADPTRACE,
	F_RDUBLK,
	F_CHMOD,
	F_CHOWN,
	F_SYMLINK,
	NOTBLOCKED,

/* Object reuse */
	F_STAT,
	BADFSIZE,
	F_BRK,
	F_LSEEK,
	F_READ,
	BADBYTE_EMPTY,
	BADBYTE_BEFORE,
	BADBYTE_BEYOND,
	BADSTRING,
	F_OPENDIR,
	F_READDIR,
	BADDIRSIZE,
	BADFILE,
	F_MKNOD,
	F_FCNTL,
	F_READLINK,
	BADSYM,
	BADLINKSIZE,
	F_MSGRCV,
	BADMSGRCV,
	F_CHDIR,
	F_SPROC,
	F_SEMGET,
	F_SEMOP,
	F_SHMGET,
	F_SHMAT,
	F_SOCKETPAIR,
	READ_DIR_OK,
	F_RENAME,
	NOESUSER
} errormsg_t;		/*end errormsg_t*/

/******************************************************************/

/*
 * The following macros are for writing formatted strings to log files.
 * Note that all of these macros, except the INIT ones, use the
 * string str, which must be defined before invoking the macro.
 */
/* 
 * Macros for writing to summary log file.
 */

#define SUM_INIT0         {w_sum_log("\
Sequence   Test               Result\n");flush_sum_log();}

#define SUM_INIT1         {w_sum_log("\
\n--------------------------------------------------------------------\n\n\
                         SUMMARY LOG FILE \n\
\n--------------------------------------------------------------------\n\n");\
                           flush_sum_log();}


#define SUM_INIT2          {w_sum_log("\
\n--------------------------------------------------------------------\n\n\
Test Results:\n");flush_sum_log();}


#define SUM_STARTTEST(a,b) {sprintf(str,"%5.5d      %-19s",\
	       (a),(b));w_sum_log(str);flush_sum_log();}

/* 
 * Macros for writing to raw log file.
 */

#define RAW_INIT1         {w_raw_log("\
\n--------------------------------------------------------------------\n\n\
                           RAW LOG FILE \n\
\n--------------------------------------------------------------------\n\n");\
                           flush_raw_log();}


#define RAW_INIT2          {w_raw_log("\
\n--------------------------------------------------------------------\n\n\
Test Results:\n");flush_raw_log();}


#define RAW_STARTTEST(a)    {sprintf(str,\
"\n--------------------------------------------------------------------\n\
Sequence   Test               Cases\n%5.5d      ",\
                       (a)); w_raw_log(str);flush_raw_log();}


#define RAWLOG_SETUP(a,b)   {sprintf(str,"%-14s       %d\n\
--------------------------------------------------------------------\n\
Case      Name     P/F         Comment\n",(a),(b));w_raw_log(str);}


#define RAWPASS(a,b)    {sprintf(str," %2.2d       %-9spass\n",\
                            (a),(b)); w_raw_log(str);}


#define RAWFAIL(a,b,c)   {sprintf(str,\
				  " %2.2d       %-9sfail      %s\n",\
                            (a),(b),(c)); w_raw_log(str);}
	    

#define RAWINC(a,b)      {sprintf(str," %2.2d       %-9sincomplete\n",\
                            (a),(b)); w_raw_log(str);}

#endif /* ifndef irixbtest.h */
