/*
 * getconf.c
 *
 *      get configuration values.
 *
 * Copyright 1995, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 *
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 *
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 *
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 */

#ident "$Revision: 1.15 $"

#include <unistd.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <errno.h>
#include <sys/systeminfo.h>
#include <locale.h>
#include <fmtmsg.h>
#include <sgi_nl.h>
#include <msgs/uxsgicore.h>

struct vals {
    char *name;
    long val;
};
struct cvals {
	char *name;
	long val;
	char *fmt;
};
struct llcvals {
	char *name;
	long long val;
	char *fmt;
};

struct vals confstr_tab[] = { /* command names for XPG4 confstr (sysinfo) */
	"PATH",				_CS_PATH,
	"CS_PATH",			_CS_PATH,
	"_CS_PATH",			_CS_PATH,
	"SYSNAME",			_CS_SYSNAME,
	"HOSTNAME",			_CS_HOSTNAME,
	"RELEASE",			_CS_RELEASE,  
	"VERSION",			_CS_VERSION, 
	"MACHINE",			_CS_MACHINE,
	"ARCHITECTURE",			_CS_ARCHITECTURE, 
	"HW_SERIAL",			_CS_HW_SERIAL,
	"HW_PROVIDER",			_CS_HW_PROVIDER, 
	"SRPC_DOMAIN",			_CS_SRPC_DOMAIN, 
	"INITTAB_NAME",			_CS_INITTAB_NAME,
	"LFS_CFLAGS",			_CS_LFS_CFLAGS,
	"LFS_LDFLAGS",			_CS_LFS_LDFLAGS,
	"LFS_LIBS",			_CS_LFS_LIBS,
	"LFS_LINTFLAGS",		_CS_LFS_LINTFLAGS,
	"LFS64_CFLAGS",			_CS_LFS64_CFLAGS,
	"LFS64_LDFLAGS",		_CS_LFS64_LDFLAGS,
	"LFS64_LIBS",			_CS_LFS64_LIBS,
	"LFS64_LINTFLAGS",		_CS_LFS64_LINTFLAGS,
	"XBS5_ILP32_OFF32_CFLAGS",	_CS_XBS5_ILP32_OFF32_CFLAGS,
	"XBS5_ILP32_OFF32_LDFLAGS",	_CS_XBS5_ILP32_OFF32_LDFLAGS,
	"XBS5_ILP32_OFF32_LIBS",	_CS_XBS5_ILP32_OFF32_LIBS,
	"XBS5_ILP32_OFF32_LINTFLAGS",	_CS_XBS5_ILP32_OFF32_LINTFLAGS,
	"XBS5_ILP32_OFFBIG_CFLAGS",	_CS_XBS5_ILP32_OFFBIG_CFLAGS,
	"XBS5_ILP32_OFFBIG_LDFLAGS",	_CS_XBS5_ILP32_OFFBIG_LDFLAGS,
	"XBS5_ILP32_OFFBIG_LIBS",	_CS_XBS5_ILP32_OFFBIG_LIBS,
	"XBS5_ILP32_OFFBIG_LINTFLAGS",	_CS_XBS5_ILP32_OFFBIG_LINTFLAGS,
	"XBS5_LP64_OFF64_CFLAGS",	_CS_XBS5_LP64_OFF64_CFLAGS,
	"XBS5_LP64_OFF64_LDFLAGS",	_CS_XBS5_LP64_OFF64_LDFLAGS,
	"XBS5_LP64_OFF64_LIBS",		_CS_XBS5_LP64_OFF64_LIBS,
	"XBS5_LP64_OFF64_LINTFLAGS",	_CS_XBS5_LP64_OFF64_LINTFLAGS,
	"XBS5_LPBIG_OFFBIG_CFLAGS",	_CS_XBS5_LPBIG_OFFBIG_CFLAGS,
	"XBS5_LPBIG_OFFBIG_LDFLAGS",	_CS_XBS5_LPBIG_OFFBIG_LDFLAGS,
	"XBS5_LPBIG_OFFBIG_LIBS",	_CS_XBS5_LPBIG_OFFBIG_LIBS,
	"XBS5_LPBIG_OFFBIG_LINTFLAGS",	_CS_XBS5_LPBIG_OFFBIG_LINTFLAGS,
};

struct vals sysinfo_tab[] = { /* command names for XPG4 (sysinfo) */
	"VENDOR",		_MIPS_SI_VENDOR,
	"OS_PROVIDER",		_MIPS_SI_OS_PROVIDER,
	"OS_NAME",		_MIPS_SI_OS_NAME,
	"HW_NAME",		_MIPS_SI_HW_NAME,
	"NUM_PROCESSORS",	_MIPS_SI_NUM_PROCESSORS,
	"HOSTID",		_MIPS_SI_HOSTID,
	"OSREL_MAJ",		_MIPS_SI_OSREL_MAJ,
	"OSREL_MIN",		_MIPS_SI_OSREL_MIN,
	"OSREL_PATCH",		_MIPS_SI_OSREL_PATCH,
	"PROCESSORS",		_MIPS_SI_PROCESSORS,
	"AVAIL_PROCESSORS",	_MIPS_SI_AVAIL_PROCESSORS,
};

/*
 * Note that the constants (_POSIX_xx_MAX) are in a separate table
 */
struct vals sysconf_tab[] = {	/* command names for sysconf	*/
	"ARG_MAX",			_SC_ARG_MAX,
	"ATEXIT_MAX",			_SC_ATEXIT_MAX,
	"BC_BASE_MAX",			_SC_BC_BASE_MAX,
	"BC_DIM_MAX",			_SC_BC_DIM_MAX,
	"BC_SCALE_MAX",			_SC_BC_SCALE_MAX,
	"BC_STRING_MAX",		_SC_BC_STRING_MAX,
	"CHILD_MAX",			_SC_CHILD_MAX,
	"CLK_TCK",			_SC_CLK_TCK,
	"COLL_WEIGHTS_MAX",		_SC_COLL_WEIGHTS_MAX,
	"EXPR_NEST_MAX",		_SC_EXPR_NEST_MAX,
	"IOV_MAX",			_SC_IOV_MAX,
	"LINE_MAX",			_SC_LINE_MAX,
	"LOGNAME_MAX",			_SC_LOGNAME_MAX,
	"NGROUPS_MAX",			_SC_NGROUPS_MAX,
	"OPEN_MAX",			_SC_OPEN_MAX,
	"PAGESIZE",			_SC_PAGESIZE,
	"PAGE_SIZE",			_SC_PAGE_SIZE,
	"PASS_MAX",			_SC_PASS_MAX,
	"POSIX2_VERSION",		_SC_2_VERSION,
	"RE_DUP_MAX",			_SC_RE_DUP_MAX,
	"STREAM_MAX",			_SC_STREAM_MAX,
	"TIMER_MAX",			_SC_TIMER_MAX,
	"TZNAME_MAX",			_SC_TZNAME_MAX,
	"POSIX2_CHAR_TERM",		_SC_2_CHAR_TERM,
	"POSIX2_C_BIND",		_SC_2_C_BIND,
	"POSIX2_C_DEV",			_SC_2_C_DEV,
	"POSIX2_C_VERSION",		_SC_2_C_VERSION,
	"POSIX2_FORT_DEV",		_SC_2_FORT_DEV,
	"POSIX2_FORT_RUN",		_SC_2_FORT_RUN,
	"POSIX2_LOCALEDEF",		_SC_2_LOCALEDEF,
	"POSIX2_SW_DEV",		_SC_2_SW_DEV,
	"POSIX2_UPE",			_SC_2_UPE,
	"_POSIX2_CHAR_TERM",		_SC_2_CHAR_TERM,
	"_POSIX2_C_BIND",		_SC_2_C_BIND,
	"_POSIX2_C_DEV",		_SC_2_C_DEV,
	"_POSIX2_C_VERSION",		_SC_2_C_VERSION,
	"_POSIX2_FORT_DEV",		_SC_2_FORT_DEV,
	"_POSIX2_FORT_RUN",		_SC_2_FORT_RUN,
	"_POSIX2_LOCALEDEF",		_SC_2_LOCALEDEF,
	"_POSIX2_SW_DEV",		_SC_2_SW_DEV,
	"_POSIX2_UPE",			_SC_2_UPE,
	"_POSIX2_VERSION",		_SC_2_VERSION,
	"_POSIX_JOB_CONTROL",		_SC_JOB_CONTROL,
	"_POSIX_SAVED_IDS",		_SC_SAVED_IDS,
	"_POSIX_VERSION",		_SC_VERSION,
	"_XOPEN_CRYPT",			_SC_XOPEN_CRYPT,
	"_XOPEN_ENH_I18N",		_SC_XOPEN_ENH_I18N,
	"_XOPEN_SHM",			_SC_XOPEN_SHM,
	"_XOPEN_UNIX",			_SC_XOPEN_UNIX,
	"_XOPEN_VERSION",		_SC_XOPEN_VERSION,
	"_XOPEN_XCU_VERSION",		_SC_XOPEN_XCU_VERSION,
	/* SGI additions */
	"ABI_ASYNCHRONOUS_IO",		_SC_ABI_ASYNCHRONOUS_IO,
	"ACL",				_SC_ACL,
	"AUDIT",			_SC_AUDIT,
	"CAP",				_SC_CAP,
	"INF",				_SC_INF,
	"IP_SECOPTS",			_SC_IP_SECOPTS,
	"KERN_POINTERS",		_SC_KERN_POINTERS,
	"KERN_SIM",			_SC_KERN_SIM,
	"MAC",				_SC_MAC,
	"NACLS_MAX",			_SC_NACLS_MAX,
	"NPROC_CONF",			_SC_NPROC_CONF,
	"NPROC_ONLN",			_SC_NPROC_ONLN,
	"SOFTPOWER",			_SC_SOFTPOWER,
	/* additions from 1003.1b - realtime */
	"AIO_LISTIO_MAX",		_SC_AIO_LISTIO_MAX,
	"AIO_MAX",			_SC_AIO_MAX,
	"AIO_PRIO_DELTA_MAX",		_SC_AIO_PRIO_DELTA_MAX,
	"DELAYTIMER_MAX",		_SC_DELAYTIMER_MAX,
	"MQ_OPEN_MAX",			_SC_MQ_OPEN_MAX,
	"MQ_PRIO_MAX",			_SC_MQ_PRIO_MAX,
	"RTSIG_MAX",			_SC_RTSIG_MAX,
	"SEM_NSEMS_MAX",		_SC_SEM_NSEMS_MAX,
	"SEM_VALUE_MAX",		_SC_SEM_VALUE_MAX,
	"SIGQUEUE_MAX",			_SC_SIGQUEUE_MAX,
	"_POSIX_ASYNCHRONOUS_IO",	_SC_ASYNCHRONOUS_IO,
	"_POSIX_FSYNC",			_SC_FSYNC,
	"_POSIX_MAPPED_FILES",		_SC_MAPPED_FILES,
	"_POSIX_MEMLOCK",		_SC_MEMLOCK,
	"_POSIX_MEMLOCK_RANGE",		_SC_MEMLOCK_RANGE,
	"_POSIX_MEMORY_PROTECTION",	_SC_MEMORY_PROTECTION,
	"_POSIX_MESSAGE_PASSING",	_SC_MESSAGE_PASSING,
	"_POSIX_PRIORITIZED_IO",	_SC_PRIORITIZED_IO,
	"_POSIX_PRIORITY_SCHEDULING",	_SC_PRIORITY_SCHEDULING,
	"_POSIX_REALTIME_SIGNALS",	_SC_REALTIME_SIGNALS,
	"_POSIX_SEMAPHORES",		_SC_SEMAPHORES,
	"_POSIX_SHARED_MEMORY_OBJECTS",	_SC_SHARED_MEMORY_OBJECTS,
	"_POSIX_SYNCHRONIZED_IO",	_SC_SYNCHRONIZED_IO,
	"_POSIX_TIMERS",		_SC_TIMERS,
	/* end 1003.1b */
	/* additions from 1003.1c - pthreads */
	"GETPW_R_SIZE_MAX",		_SC_GETGR_R_SIZE_MAX,
	"GETPW_R_SIZE_MAX",		_SC_GETPW_R_SIZE_MAX,
	"LOGIN_NAME_MAX",		_SC_LOGIN_NAME_MAX,
	"THREAD_DESTRUCTOR_ITERATIONS",	_SC_THREAD_DESTRUCTOR_ITERATIONS,
	"THREAD_KEYS_MAX",		_SC_THREAD_KEYS_MAX,
	"THREAD_STACK_MIN",		_SC_THREAD_STACK_MIN,
	"THREAD_THREADS_MAX",		_SC_THREAD_THREADS_MAX,
	"TTY_NAME_MAX",			_SC_TTY_NAME_MAX,
	"_POSIX_THREADS",		_SC_THREADS,
	"_POSIX_THREAD_ATTR_STACKADDR",	_SC_THREAD_ATTR_STACKADDR,
	"_POSIX_THREAD_ATTR_STACKSIZE",	_SC_THREAD_ATTR_STACKSIZE,
	"_POSIX_THREAD_PRIORITY_SCHEDULING",	_SC_THREAD_PRIORITY_SCHEDULING,
	"_POSIX_THREAD_PRIO_INHERIT",	_SC_THREAD_PRIO_INHERIT,
	"_POSIX_THREAD_PRIO_PROTECT",	_SC_THREAD_PRIO_PROTECT,
	"_POSIX_THREAD_PROCESS_SHARED",	_SC_THREAD_PROCESS_SHARED,
	"_POSIX_THREAD_SAFE_FUNCTIONS",	_SC_THREAD_SAFE_FUNCTIONS,
	/* end POSIX 1003.1c */
	"MMAP_FIXED_ALIGNMENT",		_SC_MMAP_FIXED_ALIGNMENT,
	"_XBS5_ILP32_OFF32",		_SC_XBS5_ILP32_OFF32,
	"_XBS5_ILP32_OFFBIG",		_SC_XBS5_ILP32_OFFBIG,
	"_XBS5_LP64_OFF64",		_SC_XBS5_LP64_OFF64,
	"_XBS5_LPBIG_OFFBIG",		_SC_XBS5_LPBIG_OFFBIG,
	"_XOPEN_LEGACY",		_SC_XOPEN_LEGACY,
	"_XOPEN_REALTIME",		_SC_XOPEN_REALTIME,
};

struct cvals const_tab[] = { /* constants */
	/* XPG4/POSIX 1003.1a values */
	"CHAR_BIT",		8,		"%d",
	"CHAR_MAX",		CHAR_MAX,	"%u",
	"CHAR_MIN",		CHAR_MIN,	"%u",
	"CHARCLASS_NAME_MAX",	CHARCLASS_NAME_MAX,	"%d",
	"INT_MAX",		INT_MAX,	"%d",
	"INT_MIN",		INT_MIN,	"%d",
	"LONG_BIT",		8*sizeof(long),	"%d",
	"LONG_MAX",		LONG_MAX,	"%ld",
	"LONG_MIN",		LONG_MIN,	"%ld",
	"MB_LEN_MAX",		MB_LEN_MAX,	"%d",
	"NL_ARGMAX",		NL_ARGMAX,	"%d",
	"NL_LANGMAX",		NL_LANGMAX,	"%d",
	"NL_NMAX",		NL_NMAX,	"%d",
	"NL_MSGMAX",		NL_MSGMAX,	"%d",
	"NL_SETMAX",		NL_SETMAX,	"%d",
	"NL_TEXTMAX",		NL_TEXTMAX,	"%d",
	"NZERO",		NZERO,		"%d",
	"POSIX2_BC_BASE_MAX",	_POSIX2_BC_BASE_MAX, "%d",
	"POSIX2_BC_DIM_MAX",	_POSIX2_BC_DIM_MAX, "%d",
	"POSIX2_BC_SCALE_MAX",	_POSIX2_BC_SCALE_MAX, "%d",
	"POSIX2_BC_STRING_MAX",	_POSIX2_BC_STRING_MAX, "%d",
	"POSIX2_COLL_WEIGHTS_MAX",_POSIX2_COLL_WEIGHTS_MAX, "%d",
	"POSIX2_EXPR_NEST_MAX",	_POSIX2_EXPR_NEST_MAX, "%d",
	"POSIX2_LINE_MAX",	_POSIX2_LINE_MAX, "%d",
	"POSIX2_RE_DUP_MAX",	_POSIX2_RE_DUP_MAX, "%d",
	"_POSIX_ARG_MAX",	_POSIX_ARG_MAX,	"%d",
	"_POSIX_CHILD_MAX",	_POSIX_CHILD_MAX,"%d",
	"_POSIX_LINK_MAX",	_POSIX_LINK_MAX,"%d",
	"_POSIX_MAX_CANON",	_POSIX_MAX_CANON,"%d",
	"_POSIX_MAX_INPUT",	_POSIX_MAX_INPUT,"%d",
	"_POSIX_NAME_MAX",	_POSIX_NAME_MAX,"%d",
	"_POSIX_NGROUPS_MAX",	_POSIX_NGROUPS_MAX,"%d",
	"_POSIX_OPEN_MAX",	_POSIX_OPEN_MAX,"%d",
	"_POSIX_PATH_MAX",	_POSIX_PATH_MAX,"%d",
	"_POSIX_PIPE_BUF",	_POSIX_PIPE_BUF,"%d",
	"_POSIX_SSIZE_MAX",	_POSIX_SSIZE_MAX,"%d",
	"_POSIX_STREAM_MAX",	_POSIX_STREAM_MAX,"%d",
	"_POSIX_TZNAME_MAX",	_POSIX_TZNAME_MAX,"%d",
	"SCHAR_MAX",		SCHAR_MAX,	"%d",
	"SCHAR_MIN",		SCHAR_MIN,	"%d",
	"SHRT_MAX",		SHRT_MAX,	"%d",
	"SHRT_MIN",		SHRT_MIN,	"%d",
	"SSIZE_MAX",		SSIZE_MAX,	"%ld",
	"TMP_MAX",		TMP_MAX,	"%d",
	"UCHAR_MAX",		UCHAR_MAX,	"%u",
	"UINT_MAX",		UINT_MAX,	"%u",
	"ULONG_MAX",		ULONG_MAX,	"%lu",
	"USHRT_MAX",		USHRT_MAX,	"%u",
	"WORD_BIT",		4*sizeof(long),	"%d",
	"_XOPEN_IOV_MAX",	_XOPEN_IOV_MAX, "%d",
	/* SGI additions */
	"_ABI_AIO_XFER_MAX",		_ABI_AIO_XFER_MAX, "%d",
	/* 1003.1b - realtime extensions */
	"_POSIX_AIO_LISTIO_MAX",	_POSIX_AIO_LISTIO_MAX, "%d",
	"_POSIX_AIO_MAX",		_POSIX_AIO_MAX, "%d",
	"_POSIX_DELAYTIMER_MAX",	_POSIX_DELAYTIMER_MAX, "%d",
	"_POSIX_MQ_OPEN_MAX",		_POSIX_MQ_OPEN_MAX, "%d",
	"_POSIX_MQ_PRIO_MAX",		_POSIX_MQ_PRIO_MAX, "%d",
	"_POSIX_RTSIG_MAX",		_POSIX_RTSIG_MAX, "%d",
	"_POSIX_SEM_NSEMS_MAX",		_POSIX_SEM_NSEMS_MAX, "%d",
	"_POSIX_SEM_VALUE_MAX",		_POSIX_SEM_VALUE_MAX, "%d",
	"_POSIX_SIGQUEUE_MAX",		_POSIX_SIGQUEUE_MAX, "%d",
	"_POSIX_TIMER_MAX",		_POSIX_TIMER_MAX, "%d",
	"_POSIX_CLOCKRES_MIN",		_POSIX_CLOCKRES_MIN, "%d",
	/* 1003.1c - pthreads extenstions */
	"_POSIX_LOGIN_NAME_MAX",	_POSIX_LOGIN_NAME_MAX, "%d",
	"_POSIX_THREAD_DESTRUCTOR_ITERATIONS",	_POSIX_THREAD_DESTRUCTOR_ITERATIONS, "%d",
	"_POSIX_THREAD_KEYS_MAX",	_POSIX_THREAD_KEYS_MAX, "%d",
	"_POSIX_THREAD_THREADS_MAX",	_POSIX_THREAD_THREADS_MAX, "%d",
	"_POSIX_TTY_NAME_MAX",		_POSIX_TTY_NAME_MAX, "%d",
};
struct llcvals llconst_tab[] = { /* constants */
	"LONGLONG_MAX",		LONGLONG_MAX,	"%lld",
	"LONGLONG_MIN",		LONGLONG_MIN,	"%lld",
	"ULONGLONG_MAX",	ULONGLONG_MAX,	"%llu",
};

struct vals pathconf_tab[] = { /* command names for pathconf	*/
	"LINK_MAX",			_PC_LINK_MAX,
	"MAX_CANON",			_PC_MAX_CANON,
	"MAX_INPUT",			_PC_MAX_INPUT,
	"NAME_MAX",			_PC_NAME_MAX,
	"PATH_MAX",			_PC_PATH_MAX,
	"PIPE_BUF",			_PC_PIPE_BUF,
	"_POSIX_CHOWN_RESTRICTED",	_PC_CHOWN_RESTRICTED,
	"_POSIX_NO_TRUNC",		_PC_NO_TRUNC,
	"_POSIX_VDISABLE",		_PC_VDISABLE,
	"_POSIX_SYNC_IO",		_PC_SYNC_IO,
	"_POSIX_PRIO_IO",		_PC_PRIO_IO,
	"_POSIX_ASYNC_IO",		_PC_ASYNC_IO,
	"ABI_ASYNC_IO",			_PC_ABI_ASYNC_IO,
	"ABI_AIO_XFER_MAX",		_PC_ABI_AIO_XFER_MAX,
	"FILESIZEBITS",			_PC_FILESIZEBITS,
};

struct vals hardconf_tab[] = {	/* Hardwired conf value */
	"_XOPEN_XPG4",			_XOPEN_XPG4,
	"_XOPEN_XPG3",			0,
	"_XOPEN_XPG2",			0,
};

#define STR_BUF_SZ 	256
char str_buf[STR_BUF_SZ];
static void dosysconf(int, char **);

int
main(int argc, char **argv)
{
	int		c,i;
	size_t		ret;
	int 		iret;
	long		lret;
	char		*cmd;
	int		err=0;

	(void)setlocale(LC_ALL, "");
	(void)setcat("uxsgicore.abi");
	(void)setlabel("UX:getconf");

	if ((cmd = strrchr(argv[0], '/')) == NULL)
		cmd = argv[0];
	else
		cmd++;
	if (strcmp(cmd, "sysconf") == 0 || strcmp(cmd, "pathconf") == 0)
		dosysconf(argc, argv);
		/* NOTREACHED */

	while ((c=getopt(argc, argv, "")) != EOF) 
	{
		switch(c) 
		{
			case '?':
				++err;
				break;
		}
	}
	if( err || argc == optind || (argc-optind) > 2 )
	{
		_sgi_nl_usage(SGINL_USAGE, "UX:getconf",
			gettxt(_SGI_DMMX_getconf_usage1, "sysconf"));
		_sgi_nl_usage(SGINL_USAGE, "UX:getconf",
			gettxt(_SGI_DMMX_getconf_usage2, "pathconf"));
		_sgi_nl_usage(SGINL_USAGE, "UX:getconf",
			gettxt(_SGI_DMMX_getconf_usage3, "getconf system_var"));
		_sgi_nl_usage(SGINL_USAGE, "UX:getconf",
			gettxt(_SGI_DMMX_getconf_usage4, "getconf path_var pathname"));
		exit(2);
	}
	errno = 0;
	if( (argc-optind) == 1 )	/* System variable or string */
	{
		/* Check confstr string value */
		for(i=0 ; i < sizeof(confstr_tab)/sizeof(confstr_tab[0]); i++) 
		{
			if ((strcasecmp(argv[optind], confstr_tab[i].name)) == 0)
			{
				ret = confstr(confstr_tab[i].val, str_buf, STR_BUF_SZ);
				if( ret == 0 && errno == EINVAL )
					exit(1);
				else if( ret == 0 && errno == 0 )
				{
					printf("undefined\n");
					exit(0);
				}
				printf("%s\n", str_buf);
				exit(0);
			}
		}
		/* Check sysinfo string value */
		for(i=0 ; i < sizeof(sysinfo_tab)/sizeof(sysinfo_tab[0]); i++) 
		{
			if ((strcasecmp(argv[optind], sysinfo_tab[i].name)) == 0)
			{
				iret = sysinfo(sysinfo_tab[i].val, str_buf, STR_BUF_SZ);
				if (iret < 0)
					exit(1);
				if (iret == 0) {
					printf("undefined\n");
					exit(0);
				}
				printf("%s\n", str_buf);
				exit(0);
			}
		}
		/* Check sysconf value */
		for(i=0 ; i < sizeof(sysconf_tab)/sizeof(sysconf_tab[0]); i++) 
		{
			if ((strcasecmp(argv[optind], sysconf_tab[i].name)) == 0)
			{
				lret = sysconf(sysconf_tab[i].val);
				if( lret == -1L && errno == EINVAL )
					exit(1);
				else if( lret == -1L && errno == 0 )
				{
					printf("undefined\n");
					exit(0);
				}
				printf("%ld\n", lret);
				exit(0);
			}
		}
		/* Check constant value */
		for(i=0 ; i < sizeof(const_tab)/sizeof(const_tab[0]); i++) 
		{
			if ((strcasecmp(argv[optind], const_tab[i].name)) == 0)
			{
				printf(const_tab[i].fmt, const_tab[i].val);
				printf("\n");
				exit(0);
			}
		}
		/* Check long long constant value */
		for(i=0 ; i < sizeof(llconst_tab)/sizeof(llconst_tab[0]); i++) 
		{
			if ((strcasecmp(argv[optind], llconst_tab[i].name)) == 0)
			{
				printf(llconst_tab[i].fmt, llconst_tab[i].val);
				printf("\n");
				exit(0);
			}
		}
		/* Check hardwired conf value */
		for(i=0 ; i < sizeof(hardconf_tab)/sizeof(hardconf_tab[0]); i++) 
		{
			if ((strcasecmp(argv[optind], hardconf_tab[i].name)) == 0)
			{
				if(hardconf_tab[i].val) {
					printf("%ld\n",hardconf_tab[i].val);
					exit(0);
				}
				else {
					printf("undefined\n");
					exit(0);
				}
			}
		}
		exit(1);
	}
	else				/* Path variable and path name */
	{
		/* Check pathconf value and path */
		for(i=0 ; i < sizeof(pathconf_tab)/sizeof(pathconf_tab[0]); i++) 
		{
			if ((strcasecmp(argv[optind], pathconf_tab[i].name)) == 0)
			{
				lret = pathconf(argv[optind+1],pathconf_tab[i].val);
				if( lret == -1L && errno != 0 )
					exit(1);
				else if( lret == -1L && errno == 0 )
				{
					printf("undefined\n");
					exit(0);
				}
				printf("%ld\n", lret);
				exit(0);
			}
		}
		exit(1);
	}
}

#define SYSTAB	0
#define PATHTAB	1
const int namelen = 32;

static void
dosysconf(int argc, char **argv)
{
    int disptab(int, char *);

    int i, itssys = 0;
    char *cmd;
    char *path = "/"; /* default for pathconf */
    int c;
    long rv;
    int status = 0;

    if ((cmd = strrchr(argv[0],'/')) == NULL)
        cmd = argv[0];  /* it's only the prog name */
    else
        cmd++;  /* path was skipped, now inc over the last '/' */

    if (strcmp(cmd, "sysconf") == 0)
	itssys = 1;

    while ((c = getopt(argc, argv, "p:")) != EOF)
    switch (c) {
	case 'p':
		path = optarg;
		break;
	default:
		if (itssys)
			fprintf(stderr, "Usage:%s [name] ...\n", cmd);
		else
			fprintf(stderr, "Usage:%s [-p path][name] ...\n", cmd);
		exit(1);
    }

    if (optind == argc) {
	/* dump whole thing */
	status = disptab(itssys?SYSTAB:PATHTAB, path);
    } else {
	for (; optind < argc; optind++) {
	    if (itssys) {
		for (i=0; i < sizeof(sysconf_tab)/sizeof(sysconf_tab[0]); i++) {
		    if (!strcasecmp(argv[optind], sysconf_tab[i].name)) {
			errno = 0;
		        rv = sysconf(sysconf_tab[i].val);
		        if (rv == -1 && errno == 0)
			    errno = ENOTSUP;
			if (errno) {
			    status = 1;
			    printf("%s\n", strerror(errno));
			} else
			    printf("%ld\n", rv);
			goto found;
		    }
		}
	    } else {
		for (i=0; i < sizeof(pathconf_tab)/sizeof(pathconf_tab[0]); i++) {
		    if (!strcasecmp(argv[optind], pathconf_tab[i].name)) {
			errno = 0;
		        rv = pathconf(path, pathconf_tab[i].val);
		        if (errno) {
			    status = 1;
			    printf("%s\n", strerror(errno));
		        } else
			    printf("%ld\n", rv);
			goto found;
		    }
		}
	    }
	    printf("%s: %s is not a valid argument\n",cmd,argv[optind]);
found:;
	}
    }
    exit(status);
}

int
disptab(int which, char *path)
{
    int i, ret;
    long rv;
    int status = 0;

    if (which == SYSTAB) {
	for (i=0; i < sizeof(sysconf_tab)/sizeof(sysconf_tab[0]); i++) {
	    errno = 0;
	    rv = sysconf(sysconf_tab[i].val);
	    if (rv == -1 && errno == 0)
		errno = ENOTSUP;
	    if (errno) {
		status = 1;
		printf("%-*s %s\n", namelen, sysconf_tab[i].name, strerror(errno));
	    } else 
		printf("%-*s %ld\n", namelen, sysconf_tab[i].name, rv);
	}
	/* for debugging sanity - dump the constant table also */
	for(i=0 ; i < sizeof(const_tab)/sizeof(const_tab[0]); i++) {
		printf("%-*s ", namelen, const_tab[i].name);
		printf(const_tab[i].fmt, const_tab[i].val);
		printf("\n");
	}
	for(i=0 ; i < sizeof(llconst_tab)/sizeof(llconst_tab[0]); i++) {
		printf("%-*s ", namelen, llconst_tab[i].name);
		printf(llconst_tab[i].fmt, llconst_tab[i].val);
		printf("\n");
	}
	for(i=0 ; i < sizeof(sysinfo_tab)/sizeof(sysinfo_tab[0]); i++) {
		ret = sysinfo(sysinfo_tab[i].val, str_buf, STR_BUF_SZ);
		if (ret > 0) {
			printf("%-*s ", namelen, sysinfo_tab[i].name);
			printf("%s\n", str_buf);
		}
	}
	for(i=0 ; i < sizeof(confstr_tab)/sizeof(confstr_tab[0]); i++) {
		errno = 0;
		ret = confstr(confstr_tab[i].val, str_buf, STR_BUF_SZ);
		if (ret == 0 && errno == 0)
			errno = ENOTSUP;
	        if (errno) {
		    status = 1;
		    printf("%-*s %s\n", namelen, confstr_tab[i].name, strerror(errno));
	        } else  {
		    printf("%-*s %s\n", namelen, confstr_tab[i].name, str_buf);
		}
	}
    } else
	for (i=0; i < sizeof(pathconf_tab)/sizeof(pathconf_tab[0]); i++) {
	    errno = 0;
	    rv = pathconf(path, pathconf_tab[i].val);
	    if (errno) {
		status = 1;
	        printf("%-*s %s\n", namelen, pathconf_tab[i].name, strerror(errno));
	    } else
	        printf("%-*s %ld\n", namelen, pathconf_tab[i].name, rv);
	}
    return status;
}
