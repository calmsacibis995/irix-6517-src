/* Copyright 1989 Silicon Graphics, Inc. All Rights Reserved. */
/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"$Revision: 1.27 $"
/*
 *  Go through the process table and kill all valid processes that
 *  are not Process 0 (sched) or Process 1 (init) or that do not
 *  have the current process group leader id.
 */

#include <sys/types.h>
#include <stdio.h>
#include <errno.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <ustat.h>
#include <sys/uadmin.h>
#include <procfs/procfs.h>
#include <stdarg.h>
#include <locale.h>
#include <fmtmsg.h>
#include <sgi_nl.h>
#include <msgs/uxsgicore.h>
#include <paths.h>
#include <sys/capability.h>

extern char  *_sys_signames[];

/*
 * killall - Usage: killall [-v][-g][-k secs][-signame|[-]signal] [procname ...]
 */
#define MAXNAMES	1000

char	dbgdir[] = _PATH_PROCFSPI;

int nname = 0;		/* # of names */

struct n {
	char *name;
	int found;
	int pid;
} names[MAXNAMES];

int Debug = 0;
int Verbose = 0;
int Spgrp = 0;
int Usesig = 0;
int Slowkill = 0;

static int cksigname(char *);
static void Usage(void);
static int killname(int);
static int chksigno(char *);
static int cksigname(char *);

char	cmd_label[] = "UX:killall";

struct kamsg {
	int	flag;
	char	*cmsg;
	char	*dmsg;
};

struct kamsg kausage[] = {
	{ SGINL_USAGE,		_SGI_DMMX_usage_killall,
		"killall [-v] [-g] [-k seconds] [-signame|[-]signal] [name ...]"	},
	{ SGINL_USAGESPC,	_SGI_DMMX_usage_killall_v,
		"-v       = verbose"					},
	{ SGINL_USAGESPC,	_SGI_DMMX_usage_killall_g,
		"-g       = send signal to process's group"		},
	{ SGINL_USAGESPC,	_SGI_DMMX_usage_killall_signal,
		"signal   = send sig # 'signal' to process"		},
	{ SGINL_USAGESPC,	_SGI_DMMX_usage_killall_signame,
		"-signame = send signal 'signame' to process"		},
	{ SGINL_USAGESPC,	_SGI_DMMX_usage_killall_l,
		"-l       = list signal names"				},
	{ 0, 0, 0 },
};

static const cap_value_t kcaps[] = {CAP_KILL, CAP_MAC_WRITE};
static int kcaps_sz = (int) (sizeof (kcaps) / sizeof (kcaps[0]));
static const cap_value_t ocaps[] = {CAP_MAC_READ};
static int ocaps_sz = (int) (sizeof (ocaps) / sizeof (ocaps[0]));

/*
 * some message prints
 */
static void
Usage(void)
{
	register struct kamsg *ump;

	for(ump = kausage; ump->cmsg; ump++) {
	    _sgi_nl_usage(ump->flag, cmd_label, gettxt(ump->cmsg, ump->dmsg));
	}
	exit(-1);
	/* NOTREACHED */
}

static void
err_stat(char *s)
{
	_sgi_nl_error(SGINL_SYSERR, cmd_label,
	    gettxt(_SGI_DMMX_cannotstat, "Cannot stat %s"), s);
}

static void
err_open(char *s)
{
	_sgi_nl_error(SGINL_SYSERR, cmd_label,
	    gettxt(_SGI_DMMX_CannotOpen, "Cannot open %s"), s);
}

static void
err_ioctl(void)
{
	_sgi_nl_error(SGINL_SYSERR, cmd_label,
	    gettxt(_SGI_DMMX_Cannot_PIOCPSINFO, "Cannot ioctl() PIOCPSINFO"));
}

static void
info_kill(int pid, char *name)
{
	_sgi_ffmtmsg(stdout, 0, cmd_label, MM_INFO,
	    gettxt(_SGI_DMMX_killall_info, "killing pid %d <%s> "),
	    pid, name);
}

/*
 * main entry
 */
int
main(int argc, char *argv[])
{
	register int	sig, pgrp;
	cap_t ocap;
	const cap_value_t cap_shutdown = CAP_SHUTDOWN;

	/*
	 * intnl support
	 */
	(void)setlocale(LC_ALL, "");
	(void)setcat("uxsgicore");
	(void)setlabel(cmd_label);

	if (argc == 1) {
		/* simple - kill all procs outside our pgrp with a SIGKILL */
		sig = 9;
	} else {
		argc--;
		argv++;
		while (argv[0][0] == '-') {
			if (strcmp(argv[0], "-k") == 0) {
				if (argc <= 0)
					Usage();
				Slowkill = atoi(argv[1]) * 100;
				argc--;
				argv++;
			} else if (strcmp(argv[0], "-d") == 0)
				Debug++;
			else if (strcmp(argv[0], "-g") == 0)
				Spgrp++;
			else if (strcmp(argv[0], "-v") == 0)
				Verbose++;
			else if ((sig = cksigname(&argv[0][1])) >= 0)
				Usesig++;
			else if ((sig = chksigno(&argv[0][1])) >= 0)
				Usesig++;
			else {
				_sgi_nl_error(SGINL_NOSYSERR, cmd_label,
				    gettxt(_SGI_DMMX_invalid_arg,
					"invalid argument %s"),
				    argv[0]);
				Usage();		/* not reached */
			}
			argc--;
			argv++;
			if (argc <= 0)
				Usage();
		}
		/* look for signal # - only if entirely a number and within
		 * range consider it a signo - else a program name
		 */
		if (!Usesig) {
			if ((sig = chksigno(&argv[0][0])) >= 0) {
				argc--;
				argv++;
			} else {
				sig = 9;
			}
		}
		while (argc--) {
		    if (Debug)
			_sgi_ffmtmsg(stdout, 0, cmd_label, MM_INFO,
			    gettxt(_SGI_DMMX_add_name, "adding name <%s>"),
			    *argv);
		    names[nname].found = 0;
		    names[nname++].name = *argv++;
		}
	}

	if (nname) {
		/* do by name */
		if (Slowkill) {
			int rc;
			int t;
			int d;
			int i;

			rc = killname(Usesig?sig:SIGTERM);
			for (d = t = 0; t < Slowkill; t += d) {
				sginap(d);
				ocap = cap_acquire(kcaps_sz, kcaps);
				for (i = 0; i < nname; i++)
					if (names[i].found &&
							!kill(names[i].pid,0))
						break;
					else
						names[i].found = 0;
				cap_surrender(ocap);
				if (i == nname)
					exit(0);
				d += 10;
			}
			ocap = cap_acquire(kcaps_sz, kcaps);
			for (i = 0; i < nname; i++)
				if (names[i].found) {
					kill(names[i].pid,SIGKILL);
					sginap(0);
				}
			cap_surrender(ocap);
			exit(rc);
		}
		exit(killname(sig));
	}
	if ((pgrp = getpgrp()) == -1) {
		_sgi_nl_error(SGINL_SYSERR, cmd_label,
		    gettxt(_SGI_DMMX_cannot_getpgrp, "cannot getpgrp()"));
		exit(1);
	}
	ocap = cap_acquire(1, &cap_shutdown);
	if (uadmin(A_KILLALL, pgrp, sig) != 0) {
		if (errno == EPERM)
		    _sgi_nl_error(SGINL_NOSYSERR, cmd_label,
			gettxt(_SGI_DMMX_MustBeSU, "Must be super-user"));
		else
		    _sgi_nl_error(SGINL_SYSERR, cmd_label,
			gettxt(_SGI_DMMX_cannot_kill, "Cannot kill"), "");
		cap_surrender(ocap);
		exit(1);
	}
	cap_surrender(ocap);
	return(0);
}

/*
 * killname - send signal to all processes with name in name array
 */
static int
killname(int sig)
{
	struct prpsinfo psinfo;
	int rv, rval = 0;
	int dids = 0;
	char pname[50];
	int pdlen, j;
	DIR *dirp;
	struct dirent *dentp;
	int procfd;	/* filedescriptor for /proc/pinfo/nnnnn */
	struct ustat ustatb;
	struct stat statb;
	dev_t procdev;
	cap_t ocap;

	if (stat(dbgdir, &statb) < 0) {
	    err_stat(dbgdir);
	    exit(1);
	}
	procdev = statb.st_dev;
	if (stat("/", &statb) < 0) {
	    err_stat("/");
	    exit(1);
	}
	if ((procdev == statb.st_dev) || (ustat(procdev, &ustatb) < 0)) {
		_sgi_nl_error(SGINL_NOSYSERR, cmd_label,
		    gettxt(_SGI_DMMX_not_mounted, "%s is not mounted"),
		    dbgdir);
		exit(1);
	}
	if ((dirp = opendir(dbgdir)) == NULL) {
	    err_open(dbgdir);
	    exit(1);
	}
	sprintf(pname, "%s%c", dbgdir, '/');
	pdlen = strlen(pname);

	/* for each active process --- */
	while (dentp = readdir(dirp)) {
		if (dentp->d_name[0] == '.')            /* skip . and .. */
			continue;
		(void) strcpy(pname + pdlen, dentp->d_name);
retry:
		ocap = cap_acquire(ocaps_sz, ocaps);
		if ((procfd = open(pname, O_RDONLY)) == -1) {
			cap_surrender(ocap);
			continue;
		}
		cap_surrender(ocap);
		/*
		 * Get the psinfo structure for the process and close quickly.
		 */
		if (ioctl(procfd, PIOCPSINFO, (char *) &psinfo) == -1) {
			int     saverr = errno;

			(void) close(procfd);
			if (saverr == EACCES)
				continue;
			if (saverr == EAGAIN)
				goto retry;
			if (saverr != ENOENT) {
				errno = saverr;
				err_ioctl();
			}
			continue;
		}
		(void) close(procfd);
		if ((psinfo.pr_state == 0) || (psinfo.pr_zomb))
			continue;
		if (Debug)
		    _sgi_ffmtmsg(stdout, 0, cmd_label, MM_INFO,
			gettxt(_SGI_DMMX_check_for, "checking for <%s> or <%s>"),
			psinfo.pr_fname, psinfo.pr_psargs);
		for (j = 0; j < nname; j++) {
			int namelen = strlen(names[j].name);
			if ((strcmp(names[j].name, psinfo.pr_fname) == 0) ||
			    ((strncmp(names[j].name, psinfo.pr_psargs, namelen) == 0)
			      && (psinfo.pr_psargs[namelen] == '\0' ||
				  psinfo.pr_psargs[namelen] == ' '))) {
				dids++;
				names[j].found++;
				if (Debug || Verbose)
				    info_kill(psinfo.pr_pid, psinfo.pr_fname);
				if (Spgrp) {
					ocap = cap_acquire(kcaps_sz, kcaps);
					rv = kill(-psinfo.pr_pid, sig);
					cap_surrender(ocap);
					names[j].pid = -psinfo.pr_pid;
				} else {
					ocap = cap_acquire(kcaps_sz, kcaps);
					rv = kill(psinfo.pr_pid, sig);
					cap_surrender(ocap);
					names[j].pid = psinfo.pr_pid;
				}
				if (rv < 0)
					rval = -1;
				if (Verbose || Debug) {
				    if (rv < 0) {
					_sgi_nl_error(SGINL_SYSERR2, cmd_label,
					    gettxt(_SGI_DMMX_killall_failed,
						"killing failed"));
				    }
				}
			}
		}
	}
	/*
	 * Report on any names not found if verbose
	 */
	if (Debug || Verbose) {
		for (j = 0; j < nname; j++) {
			if (names[j].found == 0)
				_sgi_nl_error(SGINL_NOSYSERR, cmd_label,
				    gettxt(_SGI_DMMX_killall_pnotfnd,
					"process <%s> not found"),
				    names[j].name);
		}
	}
	/* return error if ANY failures OR nothing done */
	if (dids == 0)
		rval = -1;
	return(rval);
}

static int
chksigno(char *s)
{
	register char *sp = s;
	register char c;
	register int sig;

	while ((c = *sp++) != '\0') {
		if (!isdigit(c))
			return(-1);
	}
	sig = atoi(s);
	if (sig < 0 || sig > NSIG) {
		_sgi_nl_error(SGINL_NOSYSERR, cmd_label,
		    gettxt(_SGI_DMMX_bad_signalnbr, "bad signal # %s"),
		    s);
		Usage();			/* no return */
	}
	return(sig);
}

static int
cksigname(char *name)
{
    register int i;

    if (*name == 'l') {
	int printed = 0;
	/*
	 * Print an abbreviated list of signal names.
	 */
	for (i = 1; i < NSIG; i++) {
	    if (_sys_signames[i])
		printed += printf("%s ", _sys_signames[i]);
	    if (printed > 60) {
		putchar('\n');
		printed = 0;
	    }
	}
	putchar('\n');
	exit(0);
	return(0);
    } else {
	for (i = 1; i < NSIG; i++) {
	    if (_sys_signames[i] && !strcmp(name, _sys_signames[i])) {
		return(i);
	    }
	}
	return(-1);
    }
}
