/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)ksh:sh/msg.c	1.5.4.2"

/*
 *	UNIX shell
 *	S. R. Bourne
 *	Rewritten by David Korn
 *
 *	AT&T Bell Laboratories
 *
 */

#include	<errno.h>
#include	"defs.h"
#include	"sym.h"
#include	"builtins.h"
#include	"test.h"
#include	"timeout.h"
#include	"history.h"

#ifdef MULTIBYTE
#include	"national.h"
    MSG e_version = "\n@(#)Version M-11/16/88f\0\n";
#else
    MSG e_version = "\n@(#)Version 11/16/88f\0\n";
#endif /* MULTIBYTE */

extern struct Bfunction sh_randnum;
extern struct Bfunction sh_seconds;
extern struct Bfunction line_numbers;
extern struct Bfunction opt_indexs;

/* error messages */
MSG	e_timewarn	= "\r\n\007shell will time out in 60 seconds";
MSG	e_timeout	= "timed out waiting for input";
MSG	e_mailmsg	= "you have mail in $_";
MSG	e_query		= "no query process";
MSG	e_history	= "no history file";
MSG	e_option	= "bad option(s)";
MSG	e_space		= "no space";
MSG	e_argexp	= "argument expected";
MSG	e_bracket	= "] missing";
MSG	e_number	= "bad number";
MSG	e_nullset	= "parameter null or not set";
MSG	e_notset	= "parameter not set";
MSG	e_subst		= "bad substitution";
MSG	e_create	= "cannot create";
MSG	e_restricted	= "restricted";
MSG	e_fork		= "cannot fork: too many processes";
MSG	e_pexists	= "process already exists";
MSG	e_fexists	= "file already exists";
MSG	e_swap		= "cannot fork: no swap space";
MSG	e_pipe		= "cannot make pipe";
MSG	e_open		= "cannot open";
MSG	e_logout	= "Use 'exit' to terminate this shell";
MSG	e_arglist	= "arg list too long";
MSG	e_txtbsy	= "text busy";
MSG	e_toobig	= "too big";
MSG	e_exec		= "cannot execute";
MSG	e_pwd		= "cannot access parent directories";
MSG	e_found		= " not found";
MSG	e_flimit	= "too many open files";
MSG	e_ulimit	= "exceeds allowable limit";
MSG	e_subscript	= "subscript out of range";
MSG	e_nargs		= "bad argument count";
MSG	e_combo		= "bad option combination";
MSG     e_nosuchres     = "no such resource";
MSG     e_badscale      = "Improper or unknown scale factor";

#ifdef ELIBACC
    /* shared library error messages */
    MSG	e_libacc 	= "can't access a needed shared library";
    MSG	e_libbad	= "accessing a corrupted shared library";
    MSG	e_libscn	= ".lib section in a.out corrupted";
    MSG	e_libmax	= "attempting to link in too many libs";
#endif	/* ELIBACC */
#ifdef EMULTIHOP
    MSG   e_multihop	= "multihop attempted";
#endif /* EMULTIHOP */
#ifdef ENAMETOOLONG
    MSG   e_longname	= "name too long";
#endif /* ENAMETOOLONG */
#ifdef ENOLINK
    MSG	e_link		= "remote link inactive";
#endif /* ENOLINK */
MSG	e_access	= "permission denied";
MSG	e_direct	= "bad directory";
MSG	e_notdir	= "not a directory";
MSG	e_file		= "bad file unit number";
MSG	e_trap		= "bad trap";
MSG	e_readonly	= "is read only";
MSG	e_ident		= "is not an identifier";
MSG	e_aliname	= "invalid alias name";
MSG	e_testop	= "unknown test operator";
MSG	e_alias		= " alias not found";
MSG	e_aliasspc	= "alias ";
MSG	e_function	= "unknown function";
MSG	e_format	= "bad format";
MSG	e_on		= "on";
MSG	e_off		= "off";
MSG	e_undefsym	= "undefined symbols";
MSG	e_getmap	= "cannot get mapping";
MSG	e_getvers	= "cannot get versions";
MSG	e_setmap	= "cannot set mapping";
MSG	e_setvpath	= "cannot set vpath";
MSG	e_invalidnm	= "invalid name";
MSG	e_no_access	= "not accessible";
MSG	e_nlorsemi	= "newline or ;";
MSG	e_builtexec	= "/sbin/builtin_exec";
MSG	e_write		= "write";
MSG	is_reserved	= " is a reserved keyword";
MSG	is_builtin	= " is a shell builtin";
MSG	is_spc_builtin	= " is a special shell builtin";
MSG	is_alias	= " is an alias for ";
MSG	is_function	= " is a function";
MSG	is_xalias	= " is an exported alias for ";
MSG	is_talias	= " is a tracked alias for ";
MSG	is_xfunction	= " is an exported function";
MSG	is_ufunction	= " is an undefined function";
MSG	is_pathfound	= " is found in PATH";
MSG	is_		= " is ";
MSG	e_fnhdr		= "\n{\n";
MSG	e_runvi		= "fc -e \"${VISUAL:-${EDITOR:-vi}}\" ";
#ifdef JOBS
#   ifdef SIGTSTP
	MSG	e_newtty	= "Switching to new tty driver...";
	MSG	e_oldtty	= "Reverting to old tty driver...";
	MSG	e_no_start	= "Cannot start job control";
	MSG	e_no_jctl	= "No job control";
	MSG	e_terminate	= "You have stopped jobs";
#   endif /*SIGTSTP */
   MSG	e_done		= " Done";
   MSG	e_nlspace	= "\n      ";
   MSG	e_Running	= " Running";
   MSG	e_ambiguous	= "Ambiguous";
   MSG	e_running	= "You have running jobs";
   MSG	e_no_job	= "no such job";
   MSG	e_no_proc	= "no such process";
   MSG	e_killcolon	= "kill: ";
   MSG	e_jobusage	= "Arguments must be %job or process ids";
   MSG	e_kill		= "kill";
#endif /* JOBS */
MSG	e_coredump	= "(coredump)";
#ifdef DEVFD
    MSG	e_devfd		= "/dev/fd/";
#endif /* DEVFD */
#ifdef VPIX
    MSG	e_vpix		= "/vpix";
    MSG	e_vpixdir	= "/usr/bin";
#endif /* VPIX */
#ifdef apollo
    MSG e_rootnode 	= "Bad root node specification";
    MSG e_nover    	= "Version not defined";
    MSG e_badver   	= "Unrecognized version";
#endif /* apollo */
#ifdef LDYNAMIC
    MSG e_badinlib	= "Cannot inlib";
#endif /* LDYNAMIC */
#ifdef sgi
    MSG e_badmagic	= "Program not supported by architecture";
#endif /* sgi */

/* string constants */
MSG	test_unops	= "LSVOGClaeohrwxdcbfugkpsnzt";
MSG	e_heading	= "Current option settings";
MSG	e_nullstr	= "";
MSG	e_sptbnl	= " \t\n";
MSG	e_defpath	= "/bin:/usr/bin:";
MSG	e_defedit	= "/bin/ed";
MSG	e_colon		= ": ";
MSG	e_minus		= "-";
MSG	e_endoffile	= "end of file";
MSG	e_unexpected 	= " unexpected";
MSG	e_unmatched 	= " unmatched";
MSG	e_unknown 	= "<command unknown>";
MSG	e_atline	= " at line ";
MSG	e_devnull	= "/dev/null";
MSG	e_traceprompt	= "+ ";
MSG	e_supprompt	= "# ";
MSG	e_stdprompt	= "$ ";
MSG	e_profile	= "${HOME:-.}/.profile";
MSG	e_sysprofile	= "/etc/profile";
MSG	e_suidprofile	= "/etc/suid_profile";
MSG	e_crondir	= "/var/spool/cron/atjobs";
#ifndef INT16
   MSG	e_prohibited	= "login setuid/setgid shells prohibited";
#endif /* INT16 */
#ifdef SUID_EXEC
   MSG	e_suidexec	= "/etc/suid_exec";
#endif /* SUID_EXEC */
MSG	e_devfdNN	= "/dev/fd/+([0-9])";
MSG	hist_fname	= "/.sh_history";
MSG	e_unlimited	= "unlimited";
#ifdef ECHO_N
   MSG	e_echobin	= "/bin/echo";
   MSG	e_echoflag	= "-R";
#endif	/* ECHO_N */
MSG	e_test		= "test";
MSG	e_dot		= ".";
MSG	e_bltfn		= "function ";
MSG	e_intbase	= "base";
MSG	e_envmarker	= "A__z";
#ifdef FLOAT
    MSG	e_precision	= "precision";
#endif /* FLOAT */
#ifdef PDUBIN
        MSG	e_setpwd	= "PWD=`/usr/pdu/bin/pwd 2>/dev/null`";
#else
        MSG	e_setpwd	= "PWD=`/bin/pwd 2>/dev/null`";
#endif /* PDUBIN */
MSG	e_real		= "\nreal";
MSG	e_user		= "user";
MSG	e_sys		= "sys";
MSG	e_command	= "command";
MSG	e_cant		= ": Can't ";
MSG	e_remove	= "remove ";
MSG	e_set		= "set ";
MSG	e_hard		= "hard ";
MSG	e_limit		= "limit";
MSG	e_malloc_freejobs = "malloc failed for freejobs list";
MSG	e_jobs 		= "too many jobs";
MSG	sc_hours	= "hours";
MSG	sc_minutes	= "minutes";
MSG	sc_megabytes	= "megabytes";
MSG	sc_seconds	= "seconds";
MSG	sc_kbytes	= "kbytes";

#ifdef apollo
#   undef NULL
#   define NULL ""
#   define e_nullstr	""
#endif	/* apollo */

/* built in names */
const struct name_value node_names[] =
{
	"PATH",		NULL,	0,
	"PS1",		NULL,	0,
	"PS2",		"> ",	N_FREE,
#ifdef apollo
	"IFS",		" \t\n",	N_FREE,
#else
	"IFS",		e_sptbnl,	N_FREE,
#endif	/* apollo */
	"PWD",		NULL,	0,
	"HOME",		NULL,	0,
	"MAIL",		NULL,	0,
	"REPLY",	NULL,	0,
	"SHELL",	"/bin/sh",	N_FREE,
	"EDITOR",	NULL,	0,
#ifdef apollo
	"MAILCHECK",	NULL,	N_FREE|N_INTGER,
	"RANDOM",	NULL,	N_FREE|N_INTGER,
#else
	"MAILCHECK",	(char*)(&sh_mailchk),	N_FREE|N_INTGER,
	"RANDOM",	(char*)(&sh_randnum),	N_FREE|N_INTGER|N_BLTNOD,
#endif	/* apollo */
	"ENV",		NULL,	0,
	"HISTFILE",	NULL,	0,
	"HISTSIZE",	NULL,	0,
	"FCEDIT",	"/bin/ed",	N_FREE,
	"CDPATH",	NULL,	0,
	"MAILPATH",	NULL,	0,
	"PS3",		"#? ",	N_FREE,
	"OLDPWD",	NULL,	0,
	"VISUAL",	NULL,	0,
	"COLUMNS",	NULL,	0,
	"LINES",	NULL,	0,
#ifdef apollo
	"PPID",		NULL,	N_FREE|N_INTGER,
	"_",		NULL,	N_FREE|N_INDIRECT|N_EXPORT,
	"TMOUT",	NULL,	N_FREE|N_INTGER,
	"SECONDS",	NULL,	N_FREE|N_INTGER|N_BLTNOD,
	"ERRNO",	NULL,	N_FREE|N_INTGER,
	"LINENO",	NULL,	N_FREE|N_INTGER|N_BLTNOD,
	"OPTIND",	NULL,	N_FREE|N_INTGER,
	"OPTARG",	NULL,	N_FREE|N_INDIRECT,
#else
	"PPID",		(char*)(&sh.ppid),	N_FREE|N_INTGER,
	"_",		(char*)(&sh.lastarg),	N_FREE|N_INDIRECT|N_EXPORT,
	"TMOUT",	(char*)(&sh_timeout),	N_FREE|N_INTGER,
	"SECONDS",	(char*)(&sh_seconds),	N_FREE|N_INTGER|N_BLTNOD,
	"ERRNO",	NULL,	N_FREE|N_INTGER,
	"LINENO",	(char*)(&line_numbers),	N_FREE|N_INTGER|N_BLTNOD,
	"OPTIND",	(char*)(&opt_indexs),	N_FREE|N_INTGER|N_BLTNOD|N_IMPORT,
	"OPTARG",	NULL,			N_FREE,
#endif	/* apollo */
	"PS4",		NULL,	0,
	"FPATH",	NULL,	0,
	"LANG",		NULL,	0,
	"LC_CTYPE",	NULL,	0,
#ifdef VPIX
	"DOSPATH",	NULL,	0,
	"VPIXDIR",	NULL,	0,
#endif	/* VPIX */
#ifdef ACCT
	"SHACCT",	NULL,	0,
#endif	/* ACCT */
#ifdef MULTIBYTE
	"CSWIDTH",	NULL,	0,
#endif /* MULTIBYTE */
#ifdef apollo
	"SYSTYPE",	NULL,	0,
#endif /* apollo */
	"_XPG",		"0",	N_FREE,
	"NOMSGSEVERITY","1",	N_FREE,
	"NOMSGLABEL",	"1",	N_FREE,
	e_nullstr,	NULL,	0
};

#ifdef VPIX
   const char *suffix_list[] = { ".com", ".exe", ".bat", e_nullstr };
#endif	/* VPIX */

/* built in aliases - automatically exported */
const struct name_value alias_names[] =
{
#ifdef FS_3D
	"2d",		"set -f;_2d ",	N_FREE|N_EXPORT,
#endif /* FS_3D */
	"autoload",	"typeset -fu",	N_FREE|N_EXPORT,
	"chdir",	"cd",		N_FREE|N_EXPORT,
	"functions",	"typeset -f",	N_FREE|N_EXPORT,
	"history",	"fc -l",	N_FREE|N_EXPORT,
	"integer",	"typeset -i",	N_FREE|N_EXPORT,
#ifdef POSIX
	"local",	"typeset",	N_FREE|N_EXPORT,
#endif /* POSIX */
	"nohup",	"nohup ",	N_FREE|N_EXPORT,
	"r",		"fc -e -",	N_FREE|N_EXPORT,
#ifdef SIGTSTP
	"stop",		"kill -STOP",	N_FREE|N_EXPORT,
	"suspend",	"kill -STOP $$",	N_FREE|N_EXPORT,
#endif /*SIGTSTP */
	e_nullstr,	NULL,	0
};

const struct name_value tracked_names[] =
{
	"cat",		"/bin/cat",	N_FREE|N_EXPORT|T_FLAG,
	"chmod",	"/bin/chmod",	N_FREE|N_EXPORT|T_FLAG,
	"cc",		"/bin/cc",	N_FREE|N_EXPORT|T_FLAG,
	"cp",		"/bin/cp",	N_FREE|N_EXPORT|T_FLAG,
	"date",		"/bin/date",	N_FREE|N_EXPORT|T_FLAG,
	"ed",		"/bin/ed",	N_FREE|N_EXPORT|T_FLAG,
#ifdef _bin_grep_
	"grep",		"/bin/grep",	N_FREE|N_EXPORT|T_FLAG,
#else
#  ifdef _usr_ucb_
	"grep",		"/usr/ucb/grep",N_FREE|N_EXPORT|T_FLAG,
#  endif /* _usr_ucb_ */
#endif	/* _bin_grep */
#ifdef _usr_bin_lp
	"lp",		"/usr/bin/lp",	N_FREE|N_EXPORT|T_FLAG,
#endif /* _usr_bin_lpr */
#ifdef _usr_bin_lpr
	"lpr",		"/usr/bin/lpr",	N_FREE|N_EXPORT|T_FLAG,
#endif /* _usr_bin_lpr */
	"ls",		"/bin/ls",	N_FREE|N_EXPORT|T_FLAG,
	"make",		"/bin/make",	N_FREE|N_EXPORT|T_FLAG,
	"mail",		"/bin/mail",	N_FREE|N_EXPORT|T_FLAG,
	"mv",		"/bin/mv",	N_FREE|N_EXPORT|T_FLAG,
	"pr",		"/bin/pr",	N_FREE|N_EXPORT|T_FLAG,
	"rm",		"/bin/rm",	N_FREE|N_EXPORT|T_FLAG,
	"sed",		"/bin/sed",	N_FREE|N_EXPORT|T_FLAG,
	"sh",		"/bin/sh",	N_FREE|N_EXPORT|T_FLAG,
#ifdef _usr_bin_vi_
	"vi",		"/usr/bin/vi",	N_FREE|N_EXPORT|T_FLAG,
#else
#  ifdef _usr_ucb_
	"vi",		"/usr/ucb/vi",	N_FREE|N_EXPORT|T_FLAG,
#  endif /* _usr_ucb_ */
#endif	/* _usr_bin_vi_ */
	"who",		"/bin/who",	N_FREE|N_EXPORT|T_FLAG,
	e_nullstr,	NULL,	0
};

/* tables */
SYSTAB tab_reserved =
{
#ifdef POSIX
		{"!",		NOTSYM},
#endif /* POSIX */
#ifdef NEWTEST
		{"[[",		BTSTSYM},
#endif /* NEWTEST */
		{"case",	CASYM},
		{"do",		DOSYM},
		{"done",	ODSYM},
		{"elif",	EFSYM},
		{"else",	ELSYM},
		{"esac",	ESSYM},
		{"fi",		FISYM},
		{"for",		FORSYM},
		{"function",	PROCSYM},
		{"if",		IFSYM},
		{"in",		INSYM},
		{"select",	SELSYM},
		{"then",	THSYM},
		{"time",	TIMSYM},
		{"until",	UNSYM},
		{"while",	WHSYM},
		{"{",		BRSYM},
		{"}",		KTSYM},
		{e_nullstr,	0},
};

/*
 * The signal numbers go in the low bits and the attributes go in the high bits
 */

SYSTAB	sig_names =
{
#ifdef SIGABRT
		{"ABRT",	(SIGABRT+1)|(SIGDONE<<SIGBITS)},
#endif /*SIGABRT */
		{"ALRM",	(SIGALRM+1)|((SIGCAUGHT|SIGFAULT)<<SIGBITS)},
#ifdef SIGAPOLLO
		{"APOLLO",	(SIGAPOLLO+1)},
#endif /* SIGAPOLLO */
#ifdef SIGBUS
		{"BUS",		(SIGBUS+1)|(SIGDONE<<SIGBITS)},
#endif /* SIGBUS */
#ifdef SIGCHLD
		{"CHLD",	(SIGCHLD+1)|((SIGCAUGHT|SIGFAULT)<<SIGBITS)},
#   ifdef SIGCLD
#	if SIGCLD!=SIGCHLD
		{"CLD",		(SIGCLD+1)|((SIGCAUGHT|SIGFAULT)<<SIGBITS)},
#	endif
#   endif	/* SIGCLD */
#else
#   ifdef SIGCLD
		{"CLD",		(SIGCLD+1)|((SIGCAUGHT|SIGFAULT)<<SIGBITS)},
#   endif	/* SIGCLD */
#endif	/* SIGCHLD */
#ifdef SIGCONT
		{"CONT",	(SIGCONT+1)},
#endif	/* SIGCONT */
		{"DEBUG",	(DEBUGTRAP+1)},
#ifdef SIGEMT
		{"EMT",		(SIGEMT+1)|(SIGDONE<<SIGBITS)},
#endif	/* SIGEMT */
		{"ERR",		(ERRTRAP+1)},
		{"EXIT",	1},
		{"FPE",		(SIGFPE+1)|(SIGDONE<<SIGBITS)},
		{"HUP",		(SIGHUP+1)|(SIGDONE<<SIGBITS)},
		{"ILL",		(SIGILL+1)|(SIGDONE<<SIGBITS)},
		{"INT",		(SIGINT+1)|(SIGCAUGHT<<SIGBITS)},
#ifdef SIGIO
		{"IO",		(SIGIO+1)},
#endif	/* SIGIO */
#ifdef SIGIOT
		{"IOT",		(SIGIOT+1)|(SIGDONE<<SIGBITS)},
#endif	/* SIGIOT */
		{"KILL",	(SIGKILL+1)},
#ifdef SIGLAB
		{"LAB",		(SIGLAB+1)},
#endif	/* SIGLAB */
#ifdef SIGLOST
		{"LOST",	(SIGLOST+1)},
#endif	/* SIGLOST */
#ifdef SIGPHONE
		{"PHONE",	(SIGPHONE+1)},
#endif	/* SIGPHONE */
#ifdef SIGPIPE
		{"PIPE",	(SIGPIPE+1)|(SIGDONE<<SIGBITS)},
#endif	/* SIGPIPE */
#ifdef SIGPOLL
		{"POLL",	(SIGPOLL+1)},
#endif	/* SIGPOLL */
#ifdef SIGPROF
		{"PROF",	(SIGPROF+1)},
#endif	/* SIGPROF */
#ifdef SIGPWR
#   if SIGPWR>0
		{"PWR",		(SIGPWR+1)},
#   endif
#endif	/* SIGPWR */
		{"QUIT",	(SIGQUIT+1)|((SIGCAUGHT|SIGIGNORE)<<SIGBITS)},
		{"SEGV",	(SIGSEGV+1)},
#ifdef SIGSTOP
		{"STOP",	(SIGSTOP+1)},
#endif	/* SIGSTOP */
#ifdef SIGSYS
		{"SYS",		(SIGSYS+1)|(SIGDONE<<SIGBITS)},
#endif	/* SIGSYS */
		{"TERM",	(SIGTERM+1)|(SIGDONE<<SIGBITS)},
#ifdef SIGTINT
		{"TINT",	(SIGTINT+1)},
#endif	/* SIGTINT */
#ifdef SIGTRAP
		{"TRAP",	(SIGTRAP+1)|(SIGDONE<<SIGBITS)},
#endif	/* SIGTRAP */
#ifdef SIGTSTP
		{"TSTP",	(SIGTSTP+1)},
#endif	/* SIGTSTP */
#ifdef SIGTTIN
		{"TTIN",	(SIGTTIN+1)},
#endif	/* SIGTTIN */
#ifdef SIGTTOU
		{"TTOU",	(SIGTTOU+1)},
#endif	/* SIGTTOU */
#ifdef SIGURG
		{"URG",		(SIGURG+1)},
#endif	/* SIGURG */
#ifdef SIGUSR1
		{"USR1",	(SIGUSR1+1)|(SIGDONE<<SIGBITS)},
#endif	/* SIGUSR1 */
#ifdef SIGUSR2
		{"USR2",	(SIGUSR2+1)|(SIGDONE<<SIGBITS)},
#endif	/* SIGUSR2 */
#ifdef SIGVTALRM
		{"VTALRM",	(SIGVTALRM+1)},
#endif	/* SIGVTALRM */
#ifdef SIGWINCH
		{"WINCH",	(SIGWINCH+1)},
#endif	/* SIGWINCH */
#ifdef SIGWINDOW
		{"WINDOW",	(SIGWINDOW+1)},
#endif	/* SIGWINDOW */
#ifdef SIGWIND
		{"WIND",	(SIGWIND+1)},
#endif	/* SIGWIND */
#ifdef SIGXCPU
		{"XCPU",	(SIGXCPU+1)},
#endif	/* SIGXCPU */
#ifdef SIGXFSZ
		{"XFSZ",	(SIGXFSZ+1)|((SIGCAUGHT|SIGIGNORE)<<SIGBITS)},
#endif	/* SIGXFSZ */
		{e_nullstr,	0}
};

SYSMSGTAB	sig_messages =
{
#ifdef SIGABRT
	{_SGI_DMMX_s_abort,	"Abort",			(SIGABRT+1)},
#endif /*SIGABRT */
	{_SGI_DMMX_s_alarmcall,	"Alarm call",			(SIGALRM+1)},
	{_SGI_DMMX_s_buserror,	"Bus error",			(SIGBUS+1)},
#ifdef SIGCHLD
	{_SGI_DMMX_s_childstop,	"Child stopped or terminated",	(SIGCHLD+1)},
#   ifdef SIGCLD
#	if SIGCLD!=SIGCHLD
	{_SGI_DMMX_s_childdeath,"Death of Child", 		(SIGCLD+1)},
#	endif
#   endif	/* SIGCLD */
#else
#   ifdef SIGCLD
	{_SGI_DMMX_s_childdeath,"Death of Child", 		(SIGCLD+1)},
#   endif	/* SIGCLD */
#endif	/* SIGCHLD */
#ifdef SIGCONT
	{_SGI_DMMX_s_stopcont,	"Stopped process continued",	(SIGCONT+1)},
#endif	/* SIGCONT */
#ifdef SIGEMT
	{_SGI_DMMX_s_emttrap,	"EMT trap",			(SIGEMT+1)},
#endif	/* SIGEMT */
	{_SGI_DMMX_s_floatex,	"Floating exception",		(SIGFPE+1)},
	{_SGI_DMMX_s_hangup,	"Hangup",			(SIGHUP+1)},
	{_SGI_DMMX_s_illegal,	"Illegal instruction",		(SIGILL+1)},
#ifdef JOBS
	{_SGI_DMMX_s_intr,	"Interrupt",			(SIGINT+1)},
#else
	{"",		"",			(SIGINT+1)},
#endif	/* JOBS */
#ifdef SIGIO
	{_SGI_DMMX_s_iosignal,	"IO signal",			(SIGIO+1)},
#endif	/* SIGIO */
	{_SGI_DMMX_s_abort,	"Abort",			(SIGIOT+1)},
	{_SGI_DMMX_s_killed,	"Killed",			(SIGKILL+1)},
	{_SGI_DMMX_s_quit,	"Quit",				(SIGQUIT+1)},
#ifdef JOBS
	{_SGI_DMMX_s_brokenpipe,"Broken Pipe",			(SIGPIPE+1)},
#else
	{"",		"",			(SIGPIPE+1)},
#endif	/* JOBS */
#ifdef SIGPROF
	{_SGI_DMMX_s_profalarm,	"Profiling time alarm",		(SIGPROF+1)},
#endif	/* SIGPROF */
#ifdef SIGPWR
#   if SIGPWR>0
	{_SGI_DMMX_s_powerfail,	"Power fail",			(SIGPWR+1)},
#   endif
#endif	/* SIGPWR */
	{_SGI_DMMX_s_memoryfault,"Memory fault",		(SIGSEGV+1)},
#ifdef SIGSTOP
	{_SGI_DMMX_s_stopstop,	"Stopped (SIGSTOP)",		(SIGSTOP+1)},
#endif	/* SIGSTOP */
	{_SGI_DMMX_s_badsyscall,"Bad system call", 		(SIGSYS+1)},
	{_SGI_DMMX_s_term,	"Terminated",			(SIGTERM+1)},
#ifdef SIGTINT
#   ifdef JOBS
	{_SGI_DMMX_s_intr,	"Interrupt",			(SIGTINT+1)},
#   else
	{"",		"",			(SIGTINT+1)},
#   endif /* JOBS */
#endif	/* SIGTINT */
	{_SGI_DMMX_s_tracetrap,	"Trace/BPT trap",		(SIGTRAP+1)},
#ifdef SIGTSTP
	{_SGI_DMMX_s_stoptstp,	"Stopped (SIGTSTP)",		(SIGTSTP+1)},
#endif	/* SIGTSTP */
#ifdef SIGTTIN
	{_SGI_DMMX_s_stopttin,	"Stopped (SIGTTIN)",		(SIGTTIN+1)},
#endif	/* SIGTTIN */
#ifdef SIGTTOU
	{_SGI_DMMX_s_stopttou,	"Stopped (SIGTTOU)",		(SIGTTOU+1)},
#endif	/* SIGTTOU */
#ifdef SIGURG
	{_SGI_DMMX_s_sockintr,	"Socket interrupt",		(SIGURG+1)},
#endif	/* SIGURG */
#ifdef SIGUSR1
	{_SGI_DMMX_s_usrsig1,	"User signal 1",		(SIGUSR1+1)},
#endif	/* SIGUSR1 */
#ifdef SIGUSR2
	{_SGI_DMMX_s_usrsig2,	"User signal 2",		(SIGUSR2+1)},
#endif	/* SIGUSR2 */
#ifdef SIGVTALRM
	{_SGI_DMMX_s_virtalarm,	"Virtual time alarm",		(SIGVTALRM+1)},
#endif	/* SIGVTALRM */
#ifdef SIGWINCH
	{_SGI_DMMX_s_winsizechg,"Window size change", 		(SIGWINCH+1)},
#endif	/* SIGWINCH */
#ifdef SIGXCPU
	{_SGI_DMMX_s_timelimit,	"Exceeded CPU time limit",	(SIGXCPU+1)},
#endif	/* SIGXCPU */
#ifdef SIGXFSZ
	{_SGI_DMMX_s_sizelimit,	"Exceeded file size limit",	(SIGXFSZ+1)},
#endif	/* SIGXFSZ */
#ifdef SIGLOST
	{_SGI_DMMX_s_resource,	"Resources lost", 		(SIGLOST+1)},
#endif	/* SIGLOST */
#ifdef SIGLAB
	{_SGI_DMMX_s_securitychg,"Security label changed",	(SIGLAB+1)},
#endif	/* SIGLAB */
	{"",		"",			0}
};

SYSTAB tab_options=
{
	{"allexport",		Allexp},
	{"bgnice",		Bgnice},
	{"emacs",		Emacs},
	{"errexit",		Errflg},
	{"gmacs",		Gmacs},
	{"ignoreeof",		Noeof},
	{"interactive",		Intflg},
	{"keyword",		Keyflg},
	{"markdirs",		Markdir},
	{"monitor",		Monitor},
	{"noexec",		Noexec},
	{"noclobber",		Noclob},
	{"noglob",		Noglob},
	{"nolog",		Nolog},
	{"nounset",		Noset},
	{"notify",		Notify},
#ifdef apollo
	{"physical",		Aphysical},
#endif /* apollo */
	{"privileged",		Privmod},
	{"restricted",		Rshflg},
	{"trackall",		Hashall},
	{"verbose",		Readpr},
	{"vi",			Editvi},
	{"viraw",		Viraw},
	{"xtrace",		Execpr},
	{e_nullstr,		0}
};

#ifdef _sys_resource_
#   ifndef included_sys_time_
#	include <sys/time.h>
#   endif
#   include	<sys/resource.h>/* needed for ulimit */
#   define	LIM_FSIZE	RLIMIT_FSIZE
#   define	LIM_DATA	RLIMIT_DATA
#   define	LIM_STACK	RLIMIT_STACK
#   define	LIM_CORE	RLIMIT_CORE
#   define	LIM_CPU		RLIMIT_CPU
#   ifdef RLIMIT_RSS
#	define	LIM_MAXRSS	RLIMIT_RSS
#   endif /* RLIMIT_RSS */
#else
#   ifdef VLIMIT
#	include	<sys/vlimit.h>
#   endif /* VLIMIT */
#endif	/* _sys_resource_ */

#ifdef LIM_CPU
#   define size_resource(a,b) ((a)|((b)<<11))	
SYSMSGTAB limit_names =
{
	{_SGI_DMMX_r_time,	"time(seconds)       ",	size_resource(1,LIM_CPU)},
	{_SGI_DMMX_r_file,	"file(blocks)        ",	size_resource(512,LIM_FSIZE)},
	{_SGI_DMMX_r_data,	"data(kbytes)        ",	size_resource(1024,LIM_DATA)},
	{_SGI_DMMX_r_stack,	"stack(kbytes)       ",	size_resource(1024,LIM_STACK)},
#   ifdef LIM_MAXRSS
	{_SGI_DMMX_r_memory,	"memory(kbytes)      ",	size_resource(1024,LIM_MAXRSS)},
#   else
	{_SGI_DMMX_r_memory,	"memory(kbytes)      ",	size_resource(1024,0)},
#   endif /* LIM_MAXRSS */
	{_SGI_DMMX_r_coredump,	"coredump(blocks)    ",	size_resource(512,LIM_CORE)},
#   ifdef RLIMIT_NOFILE
	{_SGI_DMMX_r_nofiles,	"nofiles(descriptors)",	size_resource(1,RLIMIT_NOFILE)},
#   else
	{_SGI_DMMX_r_nofiles,	"nofiles(descriptors)",	size_resource(1,0)},
#   endif /* RLIMIT_NOFILE */
#   ifdef RLIMIT_VMEM
	{_SGI_DMMX_r_vmemory,	"vmemory(kbytes)     ",	size_resource(1024,RLIMIT_VMEM)},
#   else
	{_SGI_DMMX_r_vmemory,	"vmemory(kbytes)     ",	size_resource(1024,0)},
#   endif /* RLIMIT_VMEM */
#   ifdef RLIMIT_PTHREAD
	{_SGI_DMMX_r_concurrency,	"concurrency(threads)",	size_resource(1,RLIMIT_PTHREAD)}
#   else
	{_SGI_DMMX_r_concurrency,	"concurrency(threads)",	size_resource(1,0)}
#   endif /* RLIMIT_PTHREAD */
};

SYS2MSGTAB blimit_names =
{
	{_SGI_DMMX_l_cputime,		"cputime      ",
	 _SGI_DMMX_sc_seconds,		"seconds",	size_resource(1,LIM_CPU)},
	{_SGI_DMMX_l_filesize,		"filesize     ",
	 _SGI_DMMX_sc_kbytes,		"kbytes",	size_resource(1024,LIM_FSIZE)},
	{_SGI_DMMX_l_datasize,		"datasize     ",
	 _SGI_DMMX_sc_kbytes,		"kbytes",	size_resource(1024,LIM_DATA)},
	{_SGI_DMMX_l_stacksize,		"stacksize    ",
	 _SGI_DMMX_sc_kbytes,		"kbytes",	size_resource(1024,LIM_STACK)},
	{_SGI_DMMX_l_coredumpsize,	"coredumpsize ",
	 _SGI_DMMX_sc_kbytes,		"kbytes",	size_resource(1024,LIM_CORE)},
#   ifdef LIM_MAXRSS
	{_SGI_DMMX_l_memoryuse,		"memoryuse    ",
	 _SGI_DMMX_sc_kbytes,		"kbytes",	size_resource(1024,LIM_MAXRSS)},
#   else
	{_SGI_DMMX_l_memoryuse,		"memoryuse    ",
	 _SGI_DMMX_sc_kbytes,		"kbytes",	size_resource(1024,0)},
#   endif /* LIM_MAXRSS */
#   ifdef RLIMIT_NOFILE
	{_SGI_DMMX_l_descriptors,	"descriptors  ",
			"",		"",		size_resource(1,RLIMIT_NOFILE)},
#   else
	{_SGI_DMMX_l_descriptors,	"descriptors  ",
	 		"",		"",		size_resource(1,0)},
#   endif /* RLIMIT_NOFILE */
#   ifdef RLIMIT_VMEM
	{_SGI_DMMX_l_vmemory,		"vmemory      ",
	 _SGI_DMMX_sc_kbytes,		"kbytes",	size_resource(1024,RLIMIT_VMEM)},
#   else
	{_SGI_DMMX_l_vmemory,		"vmemory      ",
	 _SGI_DMMX_sc_kbytes,		"kbytes",	size_resource(1024,0)},
#   endif /* RLIMIT_VMEM */
#   ifdef RLIMIT_PTHREAD
	{_SGI_DMMX_l_threads,		"threads      ",
			"",		"",		size_resource(1,RLIMIT_PTHREAD)},
#   else
	{_SGI_DMMX_l_threads,		"threads      ",
	 		"",		"",		size_resource(1,0)},
#   endif /* RLIMIT_PTHREAD */
	{NULLSTR,	NULLSTR,	NULLSTR,	NULLSTR,	0}
};
#endif	/* LIM_CPU */

#ifdef cray
    const struct name_fvalue built_ins[] =
#   define VALPTR(x)	x
#else
#   define VALPTR(x)	((char*)x)
    const struct name_value built_ins[] =
#endif /* cray */
{
		{"login",	VALPTR(b_login),	N_BLTIN|BLT_ENV},
		{"exec",	VALPTR(b_exec),		N_BLTIN|BLT_ENV|BLT_SPC},
		{"set",		VALPTR(b_set),		N_BLTIN|BLT_SPC},
		{":",		VALPTR(b_null),		N_BLTIN|BLT_SPC},
		{"true",	VALPTR(b_null),		N_BLTIN},
		{"builtin_exec",VALPTR(b_null),		N_BLTIN|BLT_EXEC},
#ifdef _bin_newgrp_
		{"newgrp",	VALPTR(b_login),	N_BLTIN|BLT_ENV},
#endif	/* _bin_newgrp_ */
		{"false",	VALPTR(b_null),		N_BLTIN},
#ifdef apollo
		{"rootnode",	VALPTR(b_rootnode),	N_BLTIN},
		{"ver",		VALPTR(b_ver),		N_BLTIN},
#endif	/* apollo */
#ifdef LDYNAMIC
		{"inlib",	VALPTR(b_inlib),	N_BLTIN},
#   ifndef apollo
		{"builtin",	VALPTR(b_builtin),	N_BLTIN},
#   endif	/* !apollo */
#endif	/* LDYNAMIC */
		{".",		VALPTR(b_dot),		N_BLTIN|BLT_SPC|BLT_FSUB},
		{"readonly",	VALPTR(b_readonly),	N_BLTIN|BLT_SPC|BLT_DCL},
		{"typeset",	VALPTR(b_typeset),	N_BLTIN|BLT_DCL},
		{"return",	VALPTR(b_ret_exit),	N_BLTIN|BLT_SPC},
		{"export",	VALPTR(b_export),	N_BLTIN|BLT_SPC|BLT_DCL},
		{"eval",	VALPTR(b_eval),		N_BLTIN|BLT_SPC|BLT_FSUB},
		{"fc",		VALPTR(b_fc),		N_BLTIN|BLT_FSUB|BLT_EXEC},
		{"shift",	VALPTR(b_shift),	N_BLTIN|BLT_SPC},
		{"cd",		VALPTR(b_chdir),	N_BLTIN|BLT_EXEC},
#ifdef OLDTEST
		{"[",		VALPTR(b_test),		N_BLTIN},
#endif /* OLDTEST */
		{ "alias",	VALPTR(b_alias),	N_BLTIN|BLT_SPC|BLT_DCL|BLT_EXEC},
		{"break",	VALPTR(b_break),	N_BLTIN|BLT_SPC},
		{"continue",	VALPTR(b_continue),	N_BLTIN|BLT_SPC},
#ifdef ECHOPRINT
		{"echo",	VALPTR(b_print),	N_BLTIN},
#else
		{"echo",	VALPTR(b_echo),		N_BLTIN},
#endif /* ECHOPRINT */
		{"exit",	VALPTR(b_ret_exit),	N_BLTIN|BLT_SPC},
#ifdef JOBS
# ifdef SIGTSTP
		{"bg",		VALPTR(b_bgfg),		N_BLTIN|BLT_EXEC},
		{"fg",		VALPTR(b_bgfg),		N_BLTIN|BLT_EXEC},
# endif	/* SIGTSTP */
		{"jobs",	VALPTR(b_jobs),		N_BLTIN|BLT_EXEC},
		{"kill",	VALPTR(b_kill),		N_BLTIN|BLT_EXEC},
#endif	/* JOBS */
		{"let",		VALPTR(b_let),		N_BLTIN},
		{"print",	VALPTR(b_print),	N_BLTIN},
		{"pwd",		VALPTR(b_pwd),		N_BLTIN},
		{"read",	VALPTR(b_read),		N_BLTIN|BLT_EXEC},
#ifdef SYSCOMPILE
		{"shcomp",	VALPTR(b_shcomp),	N_BLTIN},
#endif /* SYSCOMPILE */
#ifdef SYSSLEEP
		{"sleep",	VALPTR(b_sleep),	N_BLTIN},
#endif /* SYSSLEEP */
#ifdef OLDTEST
		{"test",	VALPTR(b_test),		N_BLTIN},
#endif /* OLDTEST */
		{"times",	VALPTR(b_times),	N_BLTIN|BLT_SPC},
		{"trap",	VALPTR(b_trap),		N_BLTIN|BLT_SPC},
		{"type",	VALPTR(b_type),		N_BLTIN|BLT_EXEC},
		{"ulimit",	VALPTR(b_ulimit),	N_BLTIN|BLT_EXEC},
		{"limit",       VALPTR(b_limit),        N_BLTIN|BLT_EXEC},
		{"unlimit",     VALPTR(b_unlimit),      N_BLTIN|BLT_EXEC},
		{"umask",	VALPTR(b_umask),	N_BLTIN|BLT_EXEC},
		{"unalias",	VALPTR(b_unalias),	N_BLTIN|BLT_EXEC},
		{"unset",	VALPTR(b_unset),	N_BLTIN|BLT_SPC},
		{"wait",	VALPTR(b_wait),		N_BLTIN|BLT_EXEC},
		{"whence",	VALPTR(b_whence),	N_BLTIN},
		{"getopts",	VALPTR(b_getopts),	N_BLTIN|BLT_EXEC},
		{"hash",	VALPTR(b_hash),		N_BLTIN|BLT_EXEC},
#ifdef UNIVERSE
		{"universe",	VALPTR(b_universe),	N_BLTIN},
#endif /* UNIVERSE */
#ifdef FS_3D
		{"vpath",	VALPTR(b_vpath_map),	N_BLTIN},
		{"vmap",	VALPTR(b_vpath_map),	N_BLTIN},
#endif /* FS_3D */
		{"command",	VALPTR(b_command),	N_BLTIN|BLT_EXEC},
		{e_nullstr,		0, 0 }
};

SYSTAB	test_optable =
{
		{"!=",		TEST_SNE},
		{"-a",		TEST_AND},
		{"-ef",		TEST_EF},
		{"-eq",		TEST_EQ},
		{"-ge",		TEST_GE},
		{"-gt",		TEST_GT},
		{"-le",		TEST_LE},
		{"-lt",		TEST_LT},
		{"-ne",		TEST_NE},
		{"-nt",		TEST_NT},
		{"-o",		TEST_OR},
		{"-ot",		TEST_OT},
		{"=",		TEST_SEQ},
		{"==",		TEST_SEQ},
#ifdef NEWTEST
		{"<",		TEST_SLT},
		{">",		TEST_SGT},
		{"]]",		TEST_END},
#endif /* NEWTEST */
		{e_nullstr,	0}
};

SYSTAB	tab_attributes =
{
		{"export",	N_EXPORT},
		{"readonly",	N_RDONLY},
		{"tagged",	T_FLAG},
#ifdef FLOAT
		{"exponential",	(N_DOUBLE|N_INTGER|N_EXPNOTE)},
		{"float",	(N_DOUBLE|N_INTGER)},
#endif /* FLOAT */
		{"long",	(L_FLAG|N_INTGER)},
		{"unsigned",	(N_UNSIGN|N_INTGER)},
		{"function",	(N_BLTNOD|N_INTGER)},
		{"integer",	N_INTGER},
		{"filename",	N_HOST},
		{"lowercase",	N_UTOL},
		{"zerofill",	N_ZFILL},
		{"leftjust",	N_LJUST},
		{"rightjust",	N_RJUST},
		{"uppercase",	N_LTOU},
		{e_nullstr,	0}
};


#ifndef IODELAY
#   undef _SELECT5_
#endif /* IODELAY */
#ifdef _sgtty_
#   ifdef _SELECT5_
	const int tty_speeds[] = {0, 50, 75, 110, 134, 150, 200, 300,
			600,1200,1800,2400,9600,19200,0};
#   endif /* _SELECT5_ */
#endif /* _sgtty_ */
