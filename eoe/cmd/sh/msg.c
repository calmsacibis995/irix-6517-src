/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)sh:msg.c	1.15.19.1"
/*
 *	UNIX shell
 */


#include	"defs.h"
#include	"sym.h"
#ifdef sgi
#include	<msgs/uxsgicore.h>
#endif

/*
 * error messages
 */
const char	getpriv[]	= "Cannot retrieve process privileges";
const char	getprivid[]	= ":825";
const char	rstpriv[]	= "Cannot restore process privileges";
const char	rstprivid[]	= ":826";
const char	setprv[]	= "Cannot turn on privilege";
const char	setprvid[]	= ":827";
const char	clrprv[]	= "Cannot turn off privilege";
const char	clrprvid[]	= ":828";
const char	setnam[]	= "Unrecognized privilege set name";
const char	setnamid[]	= ":829";
const char	prvnam[]	= "Unrecognized privilege name";
const char	prvnamid[]	= ":830";
const char	nopset[]	= "No privilege set specified";
const char	nopsetid[]	= ":831";
const char	prvunsup[]	= "System does not support privilege!";
const char	prvunsupid[]	= ":832";
const char	mldusage[]	= "mldmode [-r | -v] [string]";
const char	mldusageid[]	= ":893";
const char	nosetmode[]	= "Cannot set multilevel mode";
const char	nosetmodeid[]	= ":834";
const char	nogetmode[]	= "Cannot obtain multilevel mode";
const char	nogetmodeid[]	= ":835";
const char	notsupport[]	= "System service not installed";
const char	notsupportid[]	= ":836";
const char	invalcomb[]	= "Invalid combination of options -r & -v.";
const char	invalcombid[]	= ":837";
const char	mldmodeis[]	= "multilevel mode=";
const char	mldmodeisid[]	= ":838";
const char	badopt[]	= "Bad option(s)";
const char	badoptid[]	= ":486";
const char	mailmsg[]	= "You have mail\n";
const char	mailmsgid[]	= ":487";
const char	nospace[]	= "Out of memory";
const char	nospaceid[]	= ":49";
const char	synmsg[]	= "Syntax error";
const char	synmsgid[]	= "uxlibc:81";

const char	badnum[]	= "Bad number";
const char	badnumid[]	= ":177";
const char	badsig[]	= "Bad signal";
const char	badsigid[]	= ":488";
const char	badid[]		= "Invalid ID";
const char	badidid[]	= ":489";
const char	badparam[]	= "Parameter null or not set";
const char	badparamid[]	= ":490";
const char	unset[]		= "Parameter not set";
const char	unsetid[]	= ":491";
const char	badsub[]	= "Bad substitution";
const char	badsubid[]	= ":492";
const char	badcreate[]	= "Cannot create";
const char	badcreateid[]	= ":493";
const char	nofork[]	= "fork() failed: too many processes";
const char	noforkid[]	= ":494";
const char	noswap[]	= "fork() failed: no swap space";
const char	noswapid[]	= ":495";
const char	restricted[]	= "Restricted";
const char	restrictedid[]	= ":496";
const char	piperr[]	= "Cannot make pipe";
const char	piperrid[]	= ":497";
const char	badopen[]	= "Cannot open";
const char	badopenid[]	= ":498";
const char	coredump[]	= " - core dumped";
const char	coredumpid[]	= ":499";
const char	arglist[]	= "Arg list or environment too large";
const char	arglistid[]	= ":839";
const char	txtbsy[]	= "Text busy";
const char	txtbsyid[]	= ":500";
const char	toobig[]	= "Too big";
const char	toobigid[]	= ":501";
const char	badexec[]	= "Cannot execute";
const char	badexecid[]	= ":502";
const char	notfound[]	= "Not found";
const char	notfoundid[]	= ":503";
const char	badfile[]	= "Bad file number";
const char	badfileid[]	= "uxsyserr:12";
const char	badshift[]	= "Cannot shift";
const char	badshiftid[]	= ":504";
const char	baddir[]	= "Bad directory";
const char	baddirid[]	= ":505";
const char	badtrap[]	= "Bad trap";
const char	badtrapid[]	= ":506";
const char	wtfailed[]	= "is read only";
const char	wtfailedid[]	= ":507";
const char	notid[]		= "is not an identifier";
const char	notidid[]	= ":508";
const char 	badulimit[]	= "Bad ulimit";
const char	badulimitid[]	= ":509";
const char	badreturn[] 	= "Cannot return when not in function";
const char	badreturnid[]	= ":511";
const char	badexport[] 	= "Cannot export functions";
const char	badexportid[]	= ":512";
const char	badunset[] 	= "Cannot unset";
const char	badunsetid[]	= ":513";
const char	nohome[]	= "No home directory";
const char	nohomeid[]	= ":514";
const char 	badperm[]	= "Execute permission denied";
const char	badpermid[]	= ":515";
const char	longpwd[]	= "Pwd too long";
const char	longpwdid[]	= ":516";
const char	mssgargn[]	= "Missing arguments";
const char	mssgargnid[]	= ":517";
const char	libacc[] 	= "Cannot access a needed shared library";
const char	libaccid[]	= ":518";
const char	libbad[]	= "Accessing a corrupted shared library";
const char	libbadid[]	= "uxsyserr:87";
const char	libscn[]	= ".lib section in a.out corrupted";
const char	libscnid[]	= "uxsyserr:88";
const char	libmax[]	= "Attempting to link in too many libs";
const char	libmaxid[]	= ":519";
const char    emultihop[]     = "Multihop attempted";
const char    emultihopid[]   = "uxsyserr:77";
const char    nulldir[]       = "Null directory";
const char    nulldirid[]     = ":520";
const char    enotdir[]       = "Not a directory";
const char    enotdirid[]     = "uxsyserr:23";
const char    enoent[]        = "Does not exist";
const char    enoentid[]      = ":521";
const char    eacces[]        = "Permission denied";
const char    eaccesid[]      = "uxsyserr:16";
const char    enolink[]       = "Remote link inactive";
const char    enolinkid[]     = ":522";
const char exited[]		= "Done";
const char exitedid[]		= ":524";
const char running[]		= "Running";
const char runningid[]		= ":525";
const char ambiguous[]		= "Ambiguous";
const char ambiguousid[]	= ":526";
const char usage[]		= ":527:Usage: %s\n";
const char badusage[]		= ":8:Incorrect usage\n";
const char nojc[]		= "No job control";
const char nojcid[]		= ":528";
const char stopuse[]		= "stop id ...";
const char stopuseid[]		= ":529";
const char ulimuse[]		= "ulimit [ -HSacdfnstv ] [ limit ]"; 
const char ulimuseid[]		= ":530";
const char killuse[]		= "kill [ [ -sig ] id ... | -l ]";
const char killuseid[]		= ":531";
const char jobsuse[]		= "jobs [ [ -l | -p ] [ id ... ] | -x cmd ]";
const char jobsuseid[]		= ":532";
const char nosuchjob[]		= "No such job";
const char nosuchjobid[]	= ":533";
const char nosuchpid[]		= "No such process";
const char nosuchpidid[]	= "uxsyserr:6";
const char nosuchpgid[]		= "No such process group";
const char nosuchpgidid[]	= ":534";
const char nocurjob[]		= "No current job";
const char nocurjobid[]		= ":535";
const char jobsstopped[]	= "There are stopped jobs";
const char jobsstoppedid[]	= ":536";
const char jobsrunning[]	= "There are running jobs";
const char jobsrunningid[]	= ":537";
const char loginsh[]		= "Cannot stop login shell";
const char loginshid[]		= ":538";
#ifdef sgi
const char nosuid[]		= "Setuid program execute permission denied";
const char nosuidid[]		= _SGI_MMX_sh_suideperm;
const char badmagic[]		= "Program not supported by architecture";
const char badmagicid[]		= _SGI_MMX_sh_badmagic;
const char nosuchres[]		= "No such resource";
const char nosuchresid[]	= ":510";
const char badscale[]		= "Improper or unknown scale factor";
const char badscaleid[]		= _SGI_MMX_sh_unknscale;
#endif

/*
 * messages for 'builtin' functions
 */
const char	badop[]		= "Unknown operator";
const char	badopid[]	= ":539";
/*
 * built in names
 */
const char	pathname[]	= "PATH";
const char	cdpname[]	= "CDPATH";
const char	homename[]	= "HOME";
const char	mailname[]	= "MAIL";
const char	ifsname[]	= "IFS";
const char	ps1name[]	= "PS1";
const char	ps2name[]	= "PS2";
const char	mchkname[]	= "MAILCHECK";
const char	acctname[]  	= "SHACCT";
const char	mailpname[]	= "MAILPATH";
#ifdef SECURITY_FEATURES
const char	tfadminname[]	= "TFADMIN";
const char	timeoutname[]	= "TIMEOUT";
#endif /* SECURITY_FEATURES */

/*
 * string constants
 */
const char	nullstr[]	= "";
const char	sptbnl[]	= " \t\n";
#if defined(sgi)
const char	defpath[]	= "/usr/sbin:/usr/bsd:/bin:/usr/bin:";
#else
const char	defpath[]	= "/usr/bin:";
#endif
const char	colon[]		= ": ";
const char	colonid[]	= "uxsyserr:2";
const char	minus[]		= "-";
const char	endoffile[]	= "end of file";
const char	endoffileid[]	= ":540";
const char	unexpected[] 	= " unexpected";
const char	unexpectedid[]	= ":541";
const char	atline[]	= " at line ";
const char	atlineid[]	= ":542";
const char	devnull[]	= "/dev/null";
const char	execpmsg[]	= "+ ";
const char	readmsg[]	= "> ";
const char	stdprompt[]	= "$ ";
const char	supprompt[]	= "# ";
const char	profile[]	= ".profile";
const char	sysprofile[]	= "/etc/profile";

/*
 * tables
 */

const struct sysnod reserved[] =
{
	{ "case",	CASYM	},
	{ "do",		DOSYM	},
	{ "done",	ODSYM	},
	{ "elif",	EFSYM	},
	{ "else",	ELSYM	},
	{ "esac",	ESSYM	},
	{ "fi",		FISYM	},
	{ "for",	FORSYM	},
	{ "if",		IFSYM	},
	{ "in",		INSYM	},
	{ "then",	THSYM	},
	{ "until",	UNSYM	},
	{ "while",	WHSYM	},
	{ "{",		BRSYM	},
	{ "}",		KTSYM	}
};

const int no_reserved = sizeof(reserved)/sizeof(struct sysnod);

const char	export[] = "export";
const char	exportid[]	= ":543";
const char	readonly[] = "readonly";
const char	readonlyid[]	= ":545";


const struct sysnod commands[] =
{
	{ ".",		SYSDOT	},
	{ ":",		SYSNULL	},

#ifndef RES
	{ "[",		SYSTST },
#endif
	{ "bg",		SYSFGBG },
	{ "break",	SYSBREAK },
	{ "cd",		SYSCD	},
	{ "chdir",	SYSCD	},
	{ "continue",	SYSCONT	},
	{ "echo",	SYSECHO },
	{ "eval",	SYSEVAL	},
	{ "exec",	SYSEXEC	},
	{ "exit",	SH_SYSEXIT },
	{ "export",	SYSXPORT },
	{ "fg",		SYSFGBG },
	{ "getopts",	SYSGETOPT },
	{ "hash",	SYSHASH	},
	{ "jobs",	SYSJOBS },
	{ "kill",	SYSKILL },
#ifdef RES
	{ "login",	SYSLOGIN },
	{ "mldmode",	SYSMLD },
	{ "newgrp",	SYSLOGIN },
#else
#ifdef sgi
	{ "limit",	SYSLIMIT },
#endif
	{ "mldmode",	SYSMLD },
	{ "newgrp",	SYSNEWGRP },
#endif
	{ "priv",	SYSPRIV },
	{ "pwd",	SYSPWD },
	{ "read",	SYSREAD	},
	{ "readonly",	SYSRDONLY },
	{ "return",	SYSRETURN },
	{ "set",	SYSSET	},
	{ "shift",	SYSSHFT	},
	{ "stop",	SYSSTOP	},
	{ "suspend",	SYSSUSP},
	{ "test",	SYSTST },
	{ "times",	SYSTIMES },
	{ "trap",	SYSTRAP	},
	{ "type",	SYSTYPE },


#ifndef RES		
	{ "ulimit",	SYSULIMIT },
	{ "umask",	SYSUMASK },
#ifdef sgi
	{ "unlimit",	SYSUNLIMIT },
#endif
#endif

	{ "unset", 	SYSUNS },
	{ "wait",	SYSWAIT	}
};

const int no_commands = sizeof(commands)/sizeof(struct sysnod);

/* Ksh builtin commands NOT found in Bourne */
const struct sysnod ksh_commands[] =
{
	{ "alias",		SYSNULL	},
	{ "builtin_exec",	SYSNULL	},
	{ "command",		SYSNULL	},
	{ "fc",			SYSNULL	},
	{ "unalias",		SYSNULL	}
};
const int no_ksh_commands = sizeof(ksh_commands)/sizeof(struct sysnod);

const char	posix_builtin[]	= "POSIX shell builtin - cannot execute";
const char	posix_builtin_id[]	= ":1019";
