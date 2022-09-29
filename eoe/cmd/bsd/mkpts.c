/* Get a PTY
 *	This file appears as both lib/libc/src/gen/getpty.c
 *	and cmd/bsd/mkpts.c
 *
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

#ident "$Revision: 1.20 $"

#ifndef MKPTS
#ifdef __STDC__
	#pragma weak ptsname =	_ptsname
	#pragma weak grantpt =	_grantpt
	#pragma weak unlockpt =	_unlockpt
#endif


#include "synonyms.h"
#endif

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <paths.h>
#include <grp.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/major.h>
#include <sys/sysmacros.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/termios.h>
#include <sys/stropts.h>
#include <fcntl.h>		/* for prototyping */
#include <sys/mac.h>
#include <sys/mac_label.h>
#include <sys/capability.h>

#define PTC_HELP_PATH "/usr/sbin/mkpts"
#define PTC_CMD_NAME "mkpts"

#define IRIX_FORM 0
#define SVR4_FORM 1

#define MAX_SLAVENAME_FORMS 2

char	__slave0[] = "/dev/ttypadpadpad";
char	__slave1[] = "/dev/pts/padpadpad";
static char *slave[MAX_SLAVENAME_FORMS] = {__slave0, __slave1};

static int group_known = 0;
static gid_t pty_gid = 0;

static int spty_mode(int, mode_t, int, u_char);
static int fork_help(int, int *);

/*
 * spty_mode flag
 */

#define MD_SLAVENAME	0x1
#define MD_GRANTPT	0x2
#define MD_GETPTY	0x4
#define MD_MKPTS	0x8

char *
ptsname(int fd)
{
	struct strioctl istr;

	if (spty_mode(fd, 0, 0, MD_SLAVENAME) < 0)
		return NULL;

	istr.ic_cmd = SVR4SOPEN;
	istr.ic_len = 0;
	istr.ic_timout = 0;
	istr.ic_dp = NULL;

	if (ioctl(fd, I_STR, &istr) < 0)
		return NULL;

	return slave[SVR4_FORM];
}

int
grantpt(int fd)
{
	return (spty_mode(fd, 0620, 0, MD_GRANTPT));
}

int
unlockpt(int fd)
{
	struct strioctl istr;

	istr.ic_cmd = UNLKPT;
	istr.ic_len = 0;
	istr.ic_timout = 0;
	istr.ic_dp = NULL;


	if (ioctl(fd, I_STR, &istr) < 0)
		return(-1);

	return(0);
}

/*
 * Get a PTY
 */
char *					/* 0=failed or pointer to name */
_getpty(int *fildes,			/* place for ptc fd */
	int oflag,			/* open with these flags */
	mode_t mode,			/* give it these modes */
	int nofork)			/* 0=ok to use capable helper */
{
	int c;

	/*
	 * Open the controlling side.
	 */
	if (0 > (c = open(_PATH_DEVPTC, oflag))) 
		return 0;

	if (0 > spty_mode(c, mode & 0666, nofork, MD_GETPTY))
		return 0;

	if (0 > unlockpt(c))
		return 0;

	/*
	 * return file descriptor if asked.  Close it if no one can ever
	 * discover it.
	 */
	if (0 != fildes) {
		*fildes = c;
	} else {
		(void)close(c);
	}
	return slave[IRIX_FORM];
}


static const char wildcard[]= {MSEN_EQUAL_LABEL,MINT_EQUAL_LABEL,0,0,0,0,0,0};
static const cap_value_t caps_ch[] = {
	CAP_FOWNER,
	CAP_MAC_RELABEL_OPEN,
	CAP_MAC_UPGRADE,
	CAP_MAC_DOWNGRADE,
};

/*
 * give slave pty correct ownership and modes,
 *	given its name implicitly and the controlling
 */
static int				/* -1=failed */
spty_mode(int fd,			/* controller file descriptor */
	  mode_t mode,			/* target mode */
	  int nofork,			/* 1=do not use helper program */
	  u_char flags)			/* everybody needs some */
{
	int fam, mc, form;
	dev_t slv_dev;
	uid_t uid = getuid();
	int mac_enabled;		/* MAC Option */
	struct stat s_stb[MAX_SLAVENAME_FORMS], c_stb;
	cap_t ocap;

	mac_enabled = (sysconf(_SC_MAC) > 0);
	/*
	 * Discover correct group ID for ptys
	 */
	if (!group_known && !(flags & MD_SLAVENAME)) {
#ifdef PTYGRP
		struct group *grent;

		setgrent();
		grent = getgrname(PTYGRP);
		if (0 != grent)
			pty_gid = grent.gr_gid;
		endgrent();
#endif
		group_known = 1;
	}

	/*
	 * compute the correct file name
	 */
	if (fstat(fd, &c_stb) < 0)
		return -1;
	mc = minor(c_stb.st_rdev);
	if (mc >= 200) {
		setoserror(EINVAL);
		return -1;
	}
	switch (major(c_stb.st_rdev)) {
	case PTC_MAJOR:
		slv_dev = makedev(PTS_MAJOR, mc);
		fam = 0;
		break;
	case PTC1_MAJOR:
		slv_dev = makedev(PTS1_MAJOR, mc);
		fam = 2;
		break;
	case PTC2_MAJOR:
		slv_dev = makedev(PTS2_MAJOR, mc);
		fam = 4;
		break;
	case PTC3_MAJOR:
		slv_dev = makedev(PTS3_MAJOR, mc);
		fam = 6;
		break;
	case PTC4_MAJOR:
		slv_dev = makedev(PTS4_MAJOR, mc);
		fam = 8;
		break;
	default:
		setoserror(EINVAL);
		return -1;
	}
	fam += mc / 100;
	mc %= 100;

	(void)sprintf(slave[IRIX_FORM], "/dev/tty%c%d", "qrstuvwxyz"[fam], mc);
	(void)sprintf(slave[SVR4_FORM], "/dev/pts/%d", (fam * 100) + mc);

	if (flags & MD_SLAVENAME)
		return 0;

	for (form = 0; form < MAX_SLAVENAME_FORMS; form++) {

		/*
	 	 * Create the slave node if it does not exist.
	 	 */
		if (0 > stat(slave[form], &s_stb[form])) {
			if (errno != ENOENT)
				return -1;
			if (form) {
				/* link to original form */
				if (0 > link(slave[0], slave[form])) {
					if (nofork || 0 > fork_help(fd,&nofork))
						return -1;
				}
			} else if (0 > mknod(slave[form],
					     S_IFCHR|mode, slv_dev)) {
				if (nofork || 0 > fork_help(fd, &nofork))
					return -1;
			}
			if (mac_enabled) {
				int r;
				/* Label the new pty */
				ocap = cap_acquire (4, caps_ch);
				r = mac_set_file(slave[form], (mac_t) wildcard);
				cap_surrender (ocap);
				if (r == -1)
					return r;
			}
			if (0 > stat(slave[form], &s_stb[form]))
				return -1;
		} else if (form && !S_ISCHR(s_stb[form].st_mode)) {
			if (0 > unlink(slave[form])) {
				if (nofork || 0 > fork_help(fd, &nofork))
					return -1;
			} else {
				form--;
				continue;
			}
			/* Helper succeeded. */
			if (0 > stat(slave[form], &s_stb[form]))
				return -1;
		}

		/*
		 * Check that the slave is the right sort of beast.
		 */
		if (s_stb[form].st_rdev != slv_dev) {
			setoserror(EINVAL);
			return -1;
		}

		/*
		 * Give the pty private ownership
		 */
		if (s_stb[form].st_uid != uid || s_stb[form].st_gid != pty_gid){
			int i;

			ocap = cap_acquire (1, caps_ch);
			i = chown(slave[form], uid, pty_gid);
			cap_surrender (ocap);
			if (i < 0) {
				if (nofork || 0 > fork_help(fd, &nofork))
					return -1;
				if (0 > stat(slave[form], &s_stb[form]))
					return -1;
			}
		}

		/*
		 * Give the pty correct mode.
		 */
		if (s_stb[form].st_mode != mode) {
			int i;

			ocap = cap_acquire (1, caps_ch);
			i = chmod(slave[form], mode);
			cap_surrender (ocap);
			if (i < 0)
				return -1;
		}
	}

	return 0;
}


/*
 * Use privileged program to creat() and chown() a slave pty.
 * When capabilities are in use the behaviour differs from the
 * vanilla set-uid case.
 */
static int				/* <0 if it failed */
fork_help(int fd, int *doneonce)
{
	pid_t pid;
	int wstat;

	if (*doneonce)
		return -1;
	*doneonce = 1;

	/*
	 * give up if we have sufficient privilege to preform the
	 * operation without help.
	 */
	switch (sysconf(_SC_CAP)) {
	case CAP_SYS_DISABLED:
	default:
		if (!geteuid())
			return -1;
		break;
	case CAP_SYS_SUPERUSER:
		if (!geteuid())
			return -1;
		/* NO BREAK */
	case CAP_SYS_NO_SUPERUSER:
		/*
		 * For now, always proceed.
		 */
		break;
	}

	pid = fork();
	if (pid < 0)
		return -1;

	if (0 != pid) {
		if (0 > waitpid(pid, &wstat, 0))
			return -1;
		if (!WIFEXITED(wstat)) {
			setoserror(EINVAL);
			(void)kill(pid, SIGKILL);
		}
		errno = WEXITSTATUS(wstat);	/* get errno from child */
		return (errno != 0) ? -1 : 0;
	}

	if (0 > dup2(fd, 0))
		exit(errno);

	(void)execl(PTC_HELP_PATH, PTC_CMD_NAME, (char*)0);

	/*
	 * exit with the errno to give to our parent.  Use "return"
	 * instead of exit(2) to make lint happy.
	 */
	return errno;
}


#ifdef MKPTS				/* generate the suid program */
/*
 * Create or chown the slave pty corresponding to the controller opened
 *	as file descriptor 0.
 * This program is safely setuid, because it only does things based on
 *	an open file descriptor and because the file descriptor does not
 *	have a node in the file system (i.e. it is from a clone device).
 *
 *	The most a bad guy can do is to change the ower of a slave pty
 *	corresponding to a controlling pty that only he has open.
 */
main()
{
	if (0 > spty_mode(0, 0600, 1, MD_MKPTS))
		return errno;
	return 0;
}
#endif /* MKPTS */
