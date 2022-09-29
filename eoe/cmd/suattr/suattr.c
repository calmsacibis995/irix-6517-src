/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"$Revision: 1.2 $"
/*	Copyright (c) 1987, 1988 Microsoft Corporation	*/
/*	  All Rights Reserved	*/

/*	This Module contains Proprietary Information of Microsoft  */
/*	Corporation and should be treated as Confidential.	   */

/***************************************************************************
 * Command:	suattr
 *
 **************************************************************************/
/*
 * The origin of this source file is su.c.  Almost all the functionality
 * of su has been omitted; suattr performs the -C, -M, -m options to su
 * and execs _PATH_BSHELL with the remaining arguments.  It does not
 * perform a change of uid or gid, and will exit if not executed by
 * uid 0 (root).  Suattr is not a suid-root program.
 *
 * Additional SGI specific notes:
 *
 * The aformentioned "ID-based privilege environment" is the vanilla IRIX
 * SuperUser environment.
 *
 * If the capabilities system is installed (sysconf(_SC_CAP) != 0)
 * having a uid of 0 does not necessarily grant privilege.
 * In such a configuration, this program should be installed with the
 * necessary privileges (both allowed and forced) set in the
 * program file's capability set.
 *
 *
 * Each required privilege must be explicitly requested as needed.
 * The privileges required are:
 *
 *	CAP_SETPCAP		Allowed to change its capability set
 *(MAC) CAP_MAC_RELABEL_SUBJ    Allows a process to change its own MAC label.
 *				(Only on systems with MAC enabled.)
 *(MAC) CAP_MAC_MLD		Privilege to view the hidden directory
 *                               structure of a multilevel directory.
 *				(Only on systems with MAC enabled.)
 *
 *(MAC)	CAP_MAC_READ		Allowed to read "sensitive" administrative data
 *				(i.e. /etc/clearance)
 *(SAT)	CAP_AUDIT_WRITE		Allowed to write to the audit trail
 *
 * Since the clearance database /etc/clearance is itself protected by MAC,
 * privilege is more often than not required to get this information.
 * 
 * If the Security Audit Trail system is installed (sysconf(_SC_SAT) != 0)
 * the program must be have sufficient privilege to write audit records.
 * In the vanilla case, only the SuperUser can do this.
 */

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <pwd.h>
#include <errno.h>
#include <pfmt.h>
#include <paths.h>
#include <sat.h>
#include <sys/mac.h>
#include <sys/mac_label.h>
#include <capability.h>
#include <sys/capability.h>
#include <clearance.h>
#include <getopt.h>

#define	PATH					_PATH_USERPATH
#define	SUPATH					_PATH_ROOTPATH


static	void	err_out(int err, int astatus, char *alt_msg, char *arg);

static	int	cap_enabled = 0,	/* set iff capabilities are in use */
		mac_enabled = 0;	/* set iff MAC is available */

#define	ENV_SIZE	64

#define NUKE_ALL   0
#define NUKE_MOST  1
	



static int
mac_user_cleared (const char *user, mac_t label)
{

	if (mac_enabled) {
		struct clearance *clp;

		clp = sgi_getclearancebyname (user);
		if (clp == (struct clearance *) NULL ||
		    mac_clearedlbl (clp, label) != MAC_CLEARED)
			return (0);
	}
	return (1);
}

/*
 * Mark all non-std{in,out,err} file descriptors close-on-exec. Redirect
 * stdin/stdout/stderr to /dev/null. This is done when the
 * MAC label being requested doesn't match the current one.
 */
static void
nuke_fds (int flag)
{
	int max_fd, i;

	max_fd = (int) sysconf (_SC_OPEN_MAX);
	if (max_fd == -1)
		return;
	if(flag == NUKE_ALL){
	  for (i = 0; i < 3; i++)
	    {
	      (void) close (i);
	      (void) open ("/dev/null", O_RDWR);
	    }
	}
	for (i = 3; i < max_fd; i++)
		(void) fcntl (i, F_SETFD, FD_CLOEXEC);
}

/*
 * Determine if `user' is cleared for capability state `cap'.
 * This function is dependent upon the internal representation
 * of cap_t.
 */
static int
cap_user_cleared (const char *user, const cap_t cap)
{
	struct user_cap *clp;
	cap_t pcap;
	int result;

	if (!cap_enabled)
		return 1;

	if ((clp = sgi_getcapabilitybyname(user)) == NULL)
		pcap = cap_init();
	else
		pcap = cap_from_text(clp->ca_allowed);

	if (pcap == NULL)
		return 0;

	result = CAP_ID_ISSET(cap->cap_effective, pcap->cap_effective) &&
		 CAP_ID_ISSET(cap->cap_permitted, pcap->cap_permitted) &&
		 CAP_ID_ISSET(cap->cap_inheritable, pcap->cap_inheritable);

	(void) cap_free(pcap);

	return result;
}

/*
 * Decide if the program is running with sufficient privilege to complete
 * successfully. If this is a "uid-based privilege" environment
 * (e.g. vanilla IRIX) the effective user id may be that of the SuperUser (0).
 * If this is a capabilities environment check that each of the necessary
 * capabilities is available.
 */
static const cap_value_t privs[] = {CAP_SETPCAP,
				    CAP_MAC_READ,
				    CAP_MAC_RELABEL_SUBJ, CAP_MAC_MLD,
				    CAP_AUDIT_WRITE};

static const size_t privs_size = (sizeof (privs) / sizeof (privs[0]));

/*
 * Procedure:	main
 *
 * Notes:	main procedure of "suattr" command.
*/
int
main(int argc, char *argv[])
{
	int	mflag = 0;
	char	*Marg = (char *) NULL, *Carg = (char *) NULL, *DEFCAP = "all=";
	char	*myname;
	int	c, optoffset = 1, close_fds = 0;
	mac_t	newlabel = (mac_t) NULL;
	cap_t	newcap = (cap_t) NULL, ocap;
	cap_value_t capv;
	struct passwd *mypwent;
	char	*pshell = _PATH_BSHELL;	/*default shell*/
	/*
	 * Exit if not running as uid 0 or doesn't have enough privilege.
	 */

	if(getuid() || cap_envp(0, privs_size, privs) == -1) {
	  fprintf(stderr, "Running with insufficient privilege.\n");
	  exit(1);
	}

	/*
	 * Find out a few things about the system's configuration
	 *
	 * For capabilities (_SC_CAP) it may matter which of the
	 * possible values is returned.
	 */
	if ((cap_enabled = sysconf(_SC_CAP)) < 0)
		cap_enabled = 0;
	mac_enabled = (sysconf(_SC_MAC) > 0);

	errno = 0;
	opterr = 0;
	while ((c = getopt (argc, argv, "mM:C:")) != EOF)
	{
		switch (c)
		{
			case 'M':
				Marg = optarg;
				break;
			case 'C':
				Carg = optarg;
				break;
			case 'm':
				mflag = 1;
				break;
			case '?':
				if (optopt == 'M' || optopt == 'C')
				{
					fprintf (stderr, "usage: suattr -M<label> -C<capability> arg ...\n");
					exit(1);
				}
				optoffset = 2;
				break;
		}
	}

	/*
	 * Figure out my name. 
	 */
	mypwent = getpwuid(0);
	if(mypwent == NULL){
	  fprintf(stderr, "Can't figure out who you are.\n");
	  exit(1);
	}
	myname = mypwent->pw_name;
	
	/*
	 * verify I am cleared for specified capability set
	 */
	if (cap_enabled)
	{
		/*
		 * if no capability set is explicitly requested,
		 * use the default (empty) set.
		 */
		if (Carg == (char *) NULL)
			Carg = DEFCAP;

		/*
		 * Convert the requested capability set into internal form
		 */
		newcap = cap_from_text (Carg);
		if (newcap == (cap_t) NULL)
			err_out(errno, 1, ":26:Invalid capability set \"%s\"\n", Carg);

		/*
		 * If a capability set is specified, make sure
		 * I am cleared for it.
		 */
		if (Carg != DEFCAP && !cap_user_cleared (myname, newcap))
		{
			(void) cap_free (newcap);
			capv = CAP_AUDIT_WRITE, ocap = cap_acquire (1, &capv);
			satvwrite(SAT_AE_IDENTITY, SAT_FAILURE,
				"SUATTR|-|%s|user %s not cleared for suattr",
				myname, myname);
			cap_surrender(ocap);
			err_out(EPERM, 2, ":353:Sorry\n", "");
		}
	}

	/*
	 * verify I am  cleared for specified MAC label
	 */
	if (mac_enabled)
	{
		mac_t proclabel;

		/*
		 * get the label of the current process, and the
		 * label being requested. If no label is explicitly
		 * requested, the new label is that of the current
		 * process.
		 */
		proclabel = mac_get_proc ();
		if (proclabel == (mac_t) NULL)
			err_out(errno, 1, ":26:Invalid MAC label\n", "");

		newlabel = Marg ? mac_from_text (Marg) : proclabel;
		if (newlabel == (mac_t) NULL)
			err_out(errno, 1, ":26:Invalid MAC label\n", "");

		/*
		 * set close-on-exec flags if we execute at a different label
		 */
		if (newlabel != proclabel)
		{
			if (mac_equal (proclabel, newlabel) <= 0)
				close_fds = 1;
			(void) mac_free (proclabel);
		}

		/*
		 * check that I am cleared for the new label
		 */
		if (!mac_user_cleared (myname, newlabel))
		{
			(void) mac_free (newlabel);
			capv = CAP_AUDIT_WRITE, ocap = cap_acquire (1, &capv);
			satvwrite(SAT_AE_IDENTITY, SAT_FAILURE,
				"SUATTR|-|%s|user %s not cleared to suattr",
				myname, myname);
			cap_surrender(ocap);
			err_out(EPERM, 2, ":353:Sorry\n", "");
		}
	}

	/*
	 * Set our MAC label prior to exec.
	 * This must be done before we surrender our privileges below.
	 */
	if (mac_enabled)
	{
		cap_value_t caps[2] = {CAP_MAC_RELABEL_SUBJ, CAP_MAC_MLD};

		if (mflag) {
			mac_t olabel = newlabel;
			newlabel = mac_set_moldy(olabel);
			(void) mac_free(olabel);
			if (newlabel == NULL)
				err_out(errno, 2, ":353:Sorry\n", "");
		}
		ocap = cap_acquire (mflag ? 2 : 1, caps);
		if (mac_set_proc (newlabel) == -1)
			err_out(errno, 2, ":353:Sorry\n", "");
		cap_surrender (ocap);
		(void) mac_free (newlabel);
	}

	/*
	 * If this is a capabilities environment set the new capability
	 * set. Of course, it takes privilege to do this.
	 *
	 * Do not cap_surrender() after cap_set_proc(), as
	 * CAP_SETPCAP+e might have been requested by the user.
	 */
	if (cap_enabled)
	{
		capv = CAP_SETPCAP, ocap = cap_acquire (1, &capv);
		if (cap_set_proc (newcap) == -1)
			err_out(errno, 2, ":353:Sorry\n", "");
		(void) cap_free (newcap);
		(void) cap_free (ocap);
	}

	/*
	 * set the close-on-exec attributes
	 * also close stdin, stdout, stderr if required
	 */
	close_fds ? nuke_fds(NUKE_ALL) : nuke_fds(NUKE_MOST);

	/*
	 * Set up arg vector for exec so that argv[0] will be pshell.
	 */
	if (argc > 2) {
		argv[optind - optoffset] = pshell;
		(void) execv(pshell, &argv[optind - optoffset]);
	}
	/*
	 * Exit if no arguments, since there is nothing to do.
	 */
	else {
		fprintf(stderr, "usage: suattr [ -M label ] [ -C capability set ] arg ...\n");
		exit(1);
	}

       err_out(errno, 3, ":355:No shell\n", "");
       /* not reached */
	return(3);
}



/*
 * Procedure:	err_out
 *
 * Notes:	called in many places by "main" routine to print
 *		the appropriate error message and exit.  The string
 *		"Sorry" is printed if the errno is equal to EPERM.
 *		Otherwise, the "alt_msg" message is printed.
 */
static	void
err_out(int err, int astatus, char *alt_msg, char *arg)
{
	(void)setcat("uxcore.abi");
	(void)setlabel("UX:suattr");

	switch (err) {
	case EPERM:
		(void) pfmt(stderr, MM_ERROR, ":353:Sorry\n");
		break;
	case EACCES:
		(void) pfmt(stderr, MM_ERROR|MM_NOGET, "%s\n", strerror(err));
		break;
	default:
		if (*arg) {
			(void) pfmt(stderr, MM_ERROR, alt_msg, arg);
		}
		else {
			(void) pfmt(stderr, MM_ERROR, alt_msg);
		}
		break;
	}
	exit(astatus);
}
