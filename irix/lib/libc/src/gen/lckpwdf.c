/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libc-port:gen/lckpwdf.c	1.6"

#ifdef __STDC__
	#pragma weak lckpwdf = _lckpwdf
	#pragma weak ulckpwdf = _ulckpwdf
#endif
#include "synonyms.h"
#include <stdio.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mac.h>
#include <sys/capability.h>
#include <unistd.h>	/* for sysconf() */

#define S_WAITTIME	15

static struct flock flock =	{
			0,	/* l_type */
			0,	/* l_whence */
			0,	/* l_start */
			0,	/* l_len */
			0,	/* l_sysid */
			0	/* l_pid */
			} ;

/*	lckpwdf() returns a 0 for a successful lock within S_WAITTIME
	and -1 otherwise
*/

static	int	fildes = -1;

/*ARGSUSED*/
static void
almhdlr(int sig)
{
}

/*
 * This routine sets a lock on the _LOCKFILE.
*/
int
lckpwdf(void)
{
	int	retval,
		no_lock = 0,
		had_shad = 1;
	struct	stat	buf;
	const	char	*_PWDFILE = "/etc/shadow",
			*_LOCKFILE = "/etc/.pwd.lock";
	mac_t orig_lp = NULL;
	cap_value_t cap = CAP_MAC_RELABEL_SUBJ;
	cap_t ocap;
	int mac_enabled;
	
	/* 
	 * If Mandatory Access Control is enabled, create lock file at dblow.
	 */
	if (mac_enabled = (sysconf(_SC_MAC) > 0)) {
		mac_t dblow_lp;

		orig_lp = mac_get_proc();
		if (orig_lp == NULL)
			return -1;
		dblow_lp = mac_from_text ("msenlow/minthigh");
		if (dblow_lp == NULL) {
			mac_free(orig_lp);
			return -1;
		}
		ocap = cap_acquire (1, &cap);
		(void) mac_set_proc (dblow_lp);
		cap_surrender (ocap);
		mac_free(dblow_lp);
	}
	/*
	 * determine if _LOCKFILE exists.  If it doesn't, set "no_lock"
	 * to 1 so that the stat, chown and lvlfile will be done.
	*/
	if (stat(_LOCKFILE, &buf) < 0) {
		no_lock = 1;
		/*
		 * Only stat the password file to get owner, group and
		 * level if the _LOCKFILE did not exist.
		*/
		if (stat(_PWDFILE, &buf) < 0) {
			had_shad = 0; 		/* stat() FAILED! */
		}
	}
	if ((fildes = creat(_LOCKFILE, 0400)) == -1) {
		if (mac_enabled) {
			ocap = cap_acquire (1, &cap);
			(void) mac_set_proc(orig_lp);
			cap_surrender(ocap);
			mac_free(orig_lp);
		}
		return -1;
	}
	else {
		if (no_lock) {
			if (had_shad) {
				/*
				 * change the owner and group of the _LOCKFILE
				 * to the _PWDFILE owner.  On failure, remove
				 * the file.
				*/
				if (fchown(fildes, buf.st_uid, buf.st_gid) < 0) {
					(void) unlink(_LOCKFILE);
					if (mac_enabled) {
						ocap = cap_acquire (1, &cap);
						(void) mac_set_proc(orig_lp);
						cap_surrender(ocap);
						mac_free(orig_lp);
					}
					return -1;
				}
			}
		}
		if (mac_enabled) {
			ocap = cap_acquire (1, &cap);
			(void) mac_set_proc(orig_lp);
			cap_surrender(ocap);
			mac_free(orig_lp);
		}
		flock.l_type = F_WRLCK;
		(void) sigset(SIGALRM, (void (*)())almhdlr);
		(void) alarm(S_WAITTIME);
		retval = fcntl(fildes, F_SETLKW, &flock); 
		(void) alarm(0);
		(void) sigset(SIGALRM, SIG_DFL);
		return retval;
	}
}

/* 	ulckpwdf() returns 0 for a successful unlock and -1 otherwise
*/
int
ulckpwdf(void)
{
	if (fildes == -1) 
		return -1;
	else {
		flock.l_type = F_UNLCK;
		(void) fcntl(fildes, F_SETLK, &flock);
		(void) close(fildes);
		fildes = -1;
		return 0;
	}
}
