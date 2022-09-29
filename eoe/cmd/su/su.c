/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"$Revision: 1.55 $"
/*	Copyright (c) 1987, 1988 Microsoft Corporation	*/
/*	  All Rights Reserved	*/

/*	This Module contains Proprietary Information of Microsoft  */
/*	Corporation and should be treated as Confidential.	   */

/***************************************************************************
 * Command:	su
 *
 * Files: 	/var/adm/sulog
 *		/etc/default/su
 *
 * Notes:	change userid, `-' changes environment.
 *
 *		If SULOG is defined, all attempts to su to another user are
 *		logged there.
 *
 *		If CONSOLE is defined, all successful attempts to su to a
 *		privileged ID (in an ID-based privilege environment) are also
 *		logged there.
 *
 *		If PROMPT is defined (and ``No''), the user will not be
 *		prompted for a password. 
 *
 *		If SYSLOG is defined, log to syslog all su failures
 *		(SYSLOG=FAIL) or all successes and failures (SYSLOG=ALL).
 *		Log entries are written to the LOG_AUTH facility.
 *
 *		If su cannot create, open, or write entries into SULOG,
 *		(or on the CONSOLE, if defined), the entry will not be logged
 *		thus losing a record of the su commands attempted during this
 *		period.
 *
 **************************************************************************/

/*
 * Additional SGI specific notes:
 *
 * The aformentioned "ID-based privilege environment" is the vanilla IRIX
 * SuperUser environment.
 *
 * If the capabilities system is installed (sysconf(_SC_CAP) != 0)
 * having a uid of 0 does not necessarily grant privilege.
 * In such a configuration, this program should not be installed with the
 * setuid bit set, rather with the necessary privileges (both allowed and
 * forced) set in the program file's capability set.
 *
 * The new user's inheritable capability set is cleared, unless otherwise
 * specified (with -C). If specified, the requested capability set is checked
 * to see if the new user is cleared for it.
 *
 * Each required privilege must be explicitly requested as needed.
 * The privileges required are:
 *
 *	CAP_SETUID		Allowed to change its [er]uid
 *	CAP_SETGID		Allowed to change its [er]gid, group list
 *	CAP_SETPCAP		Allowed to change its capability set
 *	CAP_DAC_READ_SEARCH	Allowed to read "sensitive" administrative data
 *				(i.e. /etc/shadow)
 *	CAP_DAC_WRITE		Allowed to write "sensitive" administrative
 *				data (i.e. SULOG)
 *(MAC)	CAP_MAC_READ		Allowed to read "sensitive" administrative data
 *				(i.e. /etc/clearance)
 *	CAP_MAC_WRITE		Allowed to write "sensitive" administrative
 *				data (i.e. SULOG)
 *(SAT)	CAP_AUDIT_WRITE		Allowed to write to the audit trail
 *
 * Note that being able to shift the userid is sufficient to allow for
 * access to the /etc/shadow file, and that CAP_DAC_READ_SEARCH isn't strictly
 * necessary. It is better to use the "appropriate" privilege for a purpose
 * than one which does the trick as a side effect, even if it means more
 * privileges are used.
 *
 * If the Mandatory Access Control system is installed (sysconf(_SC_MAC) != 0)
 * both the old & new user must be "cleared" for the current process label.
 * Since the clearance database /etc/clearance is itself protected by MAC,
 * privilege is more often than not required to get this information.
 * 
 * If the Security Audit Trail system is installed (sysconf(_SC_SAT) != 0)
 * the program must be have sufficient privilege to write audit records.
 * In the vanilla case, only the SuperUser can do this.
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <bstring.h>
#include <crypt.h>
#include <time.h>
#include <signal.h>
#include <fcntl.h>
#include <string.h>
#include <ia.h>
#include <deflt.h>
#include <libgen.h>
#include <errno.h>
#include <locale.h>
#include <pfmt.h>
#include <paths.h>
#include <pwd.h>
#include <sat.h>
#include <sys/mac.h>
#include <sys/mac_label.h>
#include <capability.h>
#include <sys/capability.h>
#include <clearance.h>
#include <getopt.h>
#include <syslog.h>
#include <proj.h>
#ifdef _SHAREII
#include <dlfcn.h>
#include <shareIIhooks.h>
SH_DECLARE_HOOK(SETMYNAME);
SH_DECLARE_HOOK(SU);
SH_DECLARE_HOOK(SU0);
SH_DECLARE_HOOK(SUMSG);
static int SHshareRunning = 0;
#endif /* _SHAREII */

#define	ELIM					   1024
#define	DEFFILE					   "su"	/* default file M000 */

#define	PATH					_PATH_USERPATH
#define	SUPATH					_PATH_ROOTPATH

#ifdef	sgi
#define	AUX_LANG				/* LANG handling on */
#endif	/* sgi */

#if defined(sgi)
#define CHECK_MALLOC(_x) \
	if ((_x) == NULL) { \
		fprintf(stderr, "su: malloc failed. Program terminating\n"); \
		exit(1); \
	}
#endif /* sgi */


#ifdef	AUX_LANG
#include <limits.h>
#include <sys/param.h>
#define	LANG_FILE		"/.lang"	/* default lang file */
#endif	/* AUX_LANG */

extern	char	**environ;
extern int	__ruserok_x(char *, int, char *, char *, uid_t, char *);

static	uid_t	uid;

static	uinfo_t	uinfo;

static	char	*ttyn;

static	void	log(FILE *fptr, char *towho, int how, int flag);
static	void	err_out(int err, int astatus, char *alt_msg, char *arg);
static	char	*get_lang(char *to, char *home);
static	int	ask_prompt(char *usr_pwdp, uid_t uid);
static	FILE	*open_con(char *namep);
static	void	to(void),
		envalt(char *);

static	int	prompt = 1,		/* default is to prompt as always */
		syslog_fail = 0,	/* log failures to syslog */
		syslog_success = 0,	/* log failures and successes */
		cap_enabled = 0,	/* set iff capabilities are in use */
		mac_enabled = 0;	/* set iff MAC is available */

static	char	*nptr,
		*Sulog = NULL,
		*Syslog = NULL,
		*Prompt = NULL,
		*Console = NULL;

static	FILE	*su_ptr,
		*con_ptr;


#define	ENV_SIZE	64

static	char	*term,
		*path,
		*supath,
		*hz, *tz,
		*ia_pwdp, 
		*ia_dirp,
		*password,
		*ia_shellp,
		*Path = NULL,
		*Supath = NULL,		/* M004 */
		*envinit[ELIM],
		*str = "PATH=",
		*pshell = _PATH_BSHELL,	/*default shell*/

		su[ENV_SIZE+3] = "su",	/*arg0 for exec of pshell*/
		tznam[ENV_SIZE+4] = "TZ=",
		hzname[ENV_SIZE+4] = "HZ=",
		termtyp[ENV_SIZE+6] = "TERM=",/* M002 */
		logname[ENV_SIZE+9] = "LOGNAME=",

		env_user[ENV_SIZE+6] = "USER=",
		env_shell[ENV_SIZE+7] = "SHELL=",
		rem_user[ENV_SIZE+12] = "REMOTEUSER=",
		rem_host[ENV_SIZE+12] = "REMOTEHOST=",
#ifdef	AUX_LANG
		envlang[NL_LANGMAX + 8] = "LANG=",
#endif	/* AUX_LANG */
		mail[ENV_SIZE] = "MAIL=/usr/mail/",
		homedir[MAXPATHLEN] = "HOME=";

static	char	*myname;

#ifdef	AUX_LANG
/*
 * get LANG from .lang
 */
static char *
get_lang(char *to, char *home)
{
	int fd, n;
	char *nlp, uhlang[MAXPATHLEN + sizeof(LANG_FILE)];

	(void)strncpy(uhlang, home, MAXPATHLEN);
	(void)strcat(uhlang, LANG_FILE);
	if((fd = open(uhlang, O_RDONLY)) < 0)
	    return((char *)0);
	n = read(fd, uhlang, NL_LANGMAX + 2);
	if(n > 0) {
	    if(nlp = strchr(uhlang, '\n'))
		*nlp = 0;
	    uhlang[n] = 0;
	    (void)strcat(to, uhlang);
	}
	(void)close(fd);
	return((n > 0)? to : (char *)0);
}
#endif	/* AUX_LANG */

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
nuke_fds (void)
{
	int max_fd, i;

	max_fd = (int) sysconf (_SC_OPEN_MAX);
	if (max_fd == -1)
		return;
	for (i = 0; i < 3; i++)
	{
		(void) close (i);
		(void) open ("/dev/null", O_RDWR);
	}
	for (; i < max_fd; i++)
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
static const cap_value_t privs[] = {CAP_SETUID, CAP_SETGID, CAP_SETPCAP,
				    CAP_DAC_READ_SEARCH, CAP_DAC_WRITE,

				    CAP_MAC_READ, CAP_MAC_WRITE,
				    CAP_MAC_RELABEL_SUBJ, CAP_MAC_MLD,

				    CAP_PRIV_PORT, CAP_AUDIT_WRITE,
				    CAP_AUDIT_CONTROL};

static const size_t privs_size = (sizeof (privs) / sizeof (privs[0]));

/*
 * Procedure:	main
 *
 * Notes:	main procedure of "su" command.
*/
int
main(int argc, char *argv[])
{
	int	eflag = 0,
		mflag = 0,
		Sflag = 0,
		envidx = 0;
	char	*Marg = (char *) NULL, *Carg = (char *) NULL, *DEFCAP = "all=";
	int	c, optoffset = 1, close_fds = 0;
	FILE	*def_fp;
	gid_t	gid;
	gid_t   *ia_sgidp;
	long	gidcnt;
	mac_t	newlabel = (mac_t) NULL;
	cap_t	newcap = (cap_t) NULL, ocap;
	cap_value_t capv;

	/* no longer allow env variables significant to rld
	 * to be passed, to avoid potential security problems.
	 * we could do this later only if(!eflag), but since shareII
	 * does an sgidladd, and I'm not sure what the "real" liblim
	 * might spawn off, we'll do it early on */
	{
		char **e = environ, **eb = e;
		while(*e) {
			if(strncmp(*e, "_RLD", 4) && strncmp(*e, "LD_LIBRARY", 10))
				*eb++ = *e;
			e++;
		}
		*eb = NULL;
	}
#ifdef _SHAREII
	/*
	 *	Remember our original lnode before it is
	 *	potentially changed by setuid() or setreuid()
	 *	calls used in the various library routines before
	 *	the next hook.
	 *
	 *	Note that we dynamically load liblim so we don't need to
	 *	ship a stub dso to execute these commands.
	 */

	if (sgidladd(SH_LIMITS_LIB, RTLD_LAZY))
	{
		static const char *Myname = "su";
		
		SHshareRunning = 1;
		SH_HOOK_SETMYNAME(Myname);
		SH_HOOK_SU0();
	}
#endif /* _SHAREII */
	
	/*
	 * Find out a few things about the system's configuration
	 *
	 * For capabilities (_SC_CAP) it may matter which of the
	 * possible values is returned.
	 */
	if ((cap_enabled = sysconf(_SC_CAP)) < 0)
		cap_enabled = 0;
	mac_enabled = (sysconf(_SC_MAC) > 0);

	(void)setlocale(LC_ALL, "");
	(void)setcat("uxcore.abi");
	(void)setlabel("UX:su");

	errno = 0;

	if (argc > 1 && *argv[1] == '-') {
		eflag++;		/*set eflag if `-' is specified*/
		optind++;
	}

	nptr = (argc > optind) ? argv[optind++] : "root";

	opterr = 0;
	while ((c = getopt (argc, argv, "mM:C:S")) != EOF)
	{
		switch (c)
		{
			case 'M':
				Marg = optarg;
				break;
			case 'C':
				Carg = optarg;
				break;
			case 'S':
				Sflag = (sysconf(_SC_AUDIT) > 0);
				break;
			case 'm':
				mflag = 1;
				break;
			case '?':
				if (optopt == 'M' || optopt == 'C')
				{
					fprintf (stderr, "usage: su [-] [name] -M<label> -C<capability> [arg ...]\n");
					exit(1);
				}
				optoffset = 2;
				break;
		}
	}

	/*
	 * Verify that the process has enough privilege to complete.
	 */
	if (cap_envp(0, Sflag ? privs_size : privs_size - 1, privs) == -1) {
		fprintf(stderr, "Running with insufficient privilege.\n");
		exit(1);
	}

	/*
	 * Figure out who invoked us
	 */
	myname = ia_get_name(getuid());
	if (myname == NULL)
		myname = getlogin();
	if (myname == NULL) {
		fprintf(stderr, "Can't figure out who you are.\n");
		exit(1);
	}
	myname = strdup(myname);
	CHECK_MALLOC (myname);

	/*
	 * verify user is cleared for specified capability set
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
		 * If a capability set is specified by the user, make sure
		 * the new user is cleared for it.
		 */
		if (Carg != DEFCAP && !cap_user_cleared (nptr, newcap))
		{
			(void) cap_free (newcap);
			capv = CAP_AUDIT_WRITE, ocap = cap_acquire (1, &capv);
			satvwrite(SAT_AE_IDENTITY, SAT_FAILURE,
				"SU|-|%s|user %s not cleared to su",
				myname, nptr);
			cap_surrender(ocap);
			err_out(EPERM, 2, ":353:Sorry\n", "");
		}
	}

	/*
	 * verify user is cleared for specified MAC label
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
		 * check that both the current and new
		 * user is cleared for the new label
		 */
		if (!mac_user_cleared (myname, newlabel) ||
		    !mac_user_cleared (nptr, newlabel))
		{
			(void) mac_free (newlabel);
			capv = CAP_AUDIT_WRITE, ocap = cap_acquire (1, &capv);
			satvwrite(SAT_AE_IDENTITY, SAT_FAILURE,
				"SU|-|%s|user %s not cleared to su",
				myname, nptr);
			cap_surrender(ocap);
			err_out(EPERM, 2, ":353:Sorry\n", "");
		}
	}

	/* 
	 * determine specified userid, get their master file entry,
	 * and set variables to values in master file entry fields
	 */

	if((ttyn = ttyname(0)) == NULL)
		if((ttyn = ttyname(1)) == NULL)
			if((ttyn = ttyname(2)) == NULL)
				ttyn = "/dev/???";
	
	if ((def_fp = defopen(DEFFILE)) != NULL) {
		if ((Sulog = defread(def_fp, "SULOG")) != NULL)
			if (Sulog && *Sulog)
				Sulog = strdup(Sulog);
			else
				Sulog = NULL;

		if ((Prompt = defread(def_fp, "PROMPT")) != NULL) {
			if (Prompt && *Prompt) {
				/*
	 			 * if the keyword ``PROMPT'' existed and it
				 * had a value, determine if that value was
				 * the string ``No''.  If not, then ``su''
				 * should prompt as always.  Otherwise, set
	 			 * the indicator so ``su'' doesn't prompt.
	 			 *
	 			 * NOTE:	setting the keyword PROMPT to
				 *		``No'' on a system that doesn't
				 *		have the ES Package installed
				 *		is a severe security violation.
	 			 *		The administrator must be aware
				 *		of this possible violation.
				 */
#ifdef NEVER
				if (!strcmp(Prompt, "No")) {
					prompt = 0;
				}
#endif /* NEVER */
			}
		}
		if ((Console = defread(def_fp, "CONSOLE")) != NULL)
			if (Console && *Console)
				Console = strdup(Console);
			else
				Console = NULL;

		if ((Path = defread(def_fp, "PATH")) != NULL)
			if (Path && *Path)
				Path = strdup(Path);
			else
				Path = NULL;

		if ((Supath = defread(def_fp, "SUPATH")) != NULL)
			if (Supath && *Supath)
				Supath = strdup(Supath);
			else
				Supath = NULL;
		
		if ((Syslog = defread(def_fp, "SYSLOG")) != NULL)
			if (Syslog && *Syslog) {
				if (strcmp (Syslog, "FAIL") == 0)
					syslog_fail = 1;
				else if (strcmp (Syslog, "ALL") == 0)
					syslog_success = syslog_fail = 1;
			}

		(void) defclose(def_fp);
	}
	/*
	 * if Sulog defined, create SULOG if it does not exist with
	 * mode read/write user. Change owner and group to that of the
	 * directory SULOG resides in.
	 *
	 */
	if (Sulog && *Sulog) {
		const cap_value_t caps[] = {CAP_MAC_READ, CAP_DAC_READ_SEARCH,
					    CAP_DAC_WRITE, CAP_MAC_WRITE};
		int		fd;
		struct	stat	dirbuf;

		ocap = cap_acquire(2, caps);
		fd = stat(Sulog, &dirbuf);
		cap_surrender(ocap);
		if (fd == -1) {
			char *Sulog_dir, *p;
			uid_t euid = geteuid();
			gid_t egid = getegid();
			mac_t omac;

			CHECK_MALLOC (p = strdup(Sulog));
			Sulog_dir = dirname(p);
			ocap = cap_acquire(2, caps);
			fd = stat(Sulog_dir, &dirbuf);
			cap_surrender(ocap);
			free(p);
			if (fd == -1) {
				fprintf(stderr, "su: stat failed. Program terminating\n");
				exit(1);
			}

			capv = CAP_SETUID, ocap = cap_acquire(1, &capv);
			if (setreuid((uid_t) -1, dirbuf.st_uid) == -1) {
				cap_surrender(ocap);
				fprintf(stderr, "su: setreuid failed. Program terminating\n");
				exit(1);
			}
			cap_surrender(ocap);

			capv = CAP_SETGID, ocap = cap_acquire(1, &capv);
			if (setregid((gid_t) -1, dirbuf.st_gid) == -1) {
				cap_surrender(ocap);
				fprintf(stderr, "su: setregid failed. Program terminating\n");
				exit(1);
			}
			cap_surrender(ocap);

			if (mac_enabled) {
				static mac_label lbl = {MSEN_ADMIN_LABEL,
							MINT_HIGH_LABEL};

				omac = mac_get_proc();
				if (omac == NULL) {
					fprintf(stderr, "su: mac_get_proc failed. Program terminating\n");
					exit(1);
				}

				capv = CAP_MAC_RELABEL_SUBJ;
				ocap = cap_acquire(1, &capv);
				if (mac_set_proc(&lbl) == -1) {
					cap_surrender(ocap);
					fprintf(stderr, "su: mac_set_proc failed. Program terminating\n");
					exit(1);
				}
				cap_surrender(ocap);
			}

			ocap = cap_acquire(4, caps);
			fd = open(Sulog, O_WRONLY | O_APPEND | O_CREAT,
				  (S_IRUSR | S_IWUSR));
			cap_surrender(ocap);
			if (fd == -1) {
				fprintf(stderr, "su: open(SULOG) failed. Program terminating\n");
				exit(1);
			}

			if (mac_enabled) {
				capv = CAP_MAC_RELABEL_SUBJ;
				ocap = cap_acquire(1, &capv);
				if (mac_set_proc(omac) == -1) {
					cap_surrender(ocap);
					mac_free(omac);
					close(fd);
					fprintf(stderr, "su: mac_set_proc failed. Program terminating\n");
					exit(1);
				}
				cap_surrender(ocap);
				mac_free(omac);
			}

			capv = CAP_SETUID, ocap = cap_acquire(1, &capv);
			if (setreuid((uid_t) -1, euid) == -1) {
				cap_surrender(ocap);
				close(fd);
				fprintf(stderr, "su: setreuid failed. Program terminating\n");
				exit(1);
			}
			cap_surrender(ocap);

			capv = CAP_SETGID, ocap = cap_acquire(1, &capv);
			if (setregid((gid_t) -1, egid) == -1) {
				cap_surrender(ocap);
				close(fd);
				fprintf(stderr, "su: setregid failed. Program terminating\n");
				exit(1);
			}
			cap_surrender(ocap);

			su_ptr = fdopen(fd, "a");
		} else {
			ocap = cap_acquire(4, caps);
			su_ptr = fopen(Sulog, "a");
			cap_surrender(ocap);
		}
	}
	if (Console && *Console) {
		con_ptr = open_con(Console);
	}

	/*
	 * ia_openinfo will gather all of the per user information that it
	 * can, leaving the rest undefined.
	 */
	if (ia_openinfo(nptr, &uinfo) || (uinfo == NULL) || strlen(nptr) >= ENV_SIZE) {
		uid = -1;
		capv = CAP_AUDIT_WRITE, ocap = cap_acquire (1, &capv);
		ia_audit("SU", myname, 0, "Invalid user name specified");
		cap_surrender(ocap);
		err_out(0, 1, ":26:Unknown user id: %s\n", nptr);
	}

	path = (char *) malloc(((Path && *Path) ? strlen(Path) : strlen(PATH)) + strlen(str) + 1);
	CHECK_MALLOC(path);
	supath = (char *) malloc(((Supath && *Supath) ? strlen(Supath) : strlen(SUPATH)) + strlen(str) + 1);
	CHECK_MALLOC(supath);
	
	(void) strcpy(path, str);
	(void) strcat(path, (Path && *Path) ? Path : PATH);

	(void) strcpy(supath, str);
	(void) strcat(supath, (Supath && *Supath) ? Supath : SUPATH);

	ia_get_uid(uinfo, &uid);
	ia_get_gid(uinfo, &gid);

	ia_get_logpwd(uinfo, &ia_pwdp);
	ia_get_dir(uinfo, &ia_dirp);

	if (myname && __ruserok_x("localhost", 1, myname, nptr, uid, ia_dirp) == 0 )
		goto ok;
	if (ask_prompt(ia_pwdp, uid)) {
		password = getpass(gettxt(":352", "Password:"));
		if (password == (char *) NULL ||
		    strcmp(ia_pwdp, crypt(password, ia_pwdp)) != 0) {
			capv = CAP_AUDIT_WRITE, ocap = cap_acquire (1, &capv);
			satvwrite(SAT_AE_IDENTITY, SAT_FAILURE,
				  "SU|-|%s|Incorrect password for user %s",
				  myname, nptr);
			cap_surrender(ocap);
			err_out(EPERM, 2, ":353:Sorry\n", "");
		}
	}
ok:
	/*
	 * set the new users group id.
	 */
	capv = CAP_SETGID, ocap = cap_acquire (1, &capv);
	if (setgid(gid) != 0)
		err_out(errno, 2, ":841:Invalid GID\n", "");
	cap_surrender (ocap);

#ifdef _SHAREII
	/*
	 *	Perform Share II equivalent of setuid().
	 *	We either explicitly attach to a new lnode,
	 *	or restore the original lnode attachment.
	 *	In either case, we prevent the following
	 *	setuid() from changing our lnode.
	 *
	 */

	if (SHshareRunning && SH_HOOK_SU(uid, eflag))
	{
		err_out(EACCES, 2, "Share II denies access - %s\n", SH_HOOK_SUMSG());
	}
#endif /* _SHAREII */
	
	/*
	 * Initialize the supplementary group access list
	 */
	ia_get_sgid(uinfo, &ia_sgidp, &gidcnt);
	if (gidcnt) {
		capv = CAP_SETGID, ocap = cap_acquire (1, &capv);
		if (setgroups(gidcnt, ia_sgidp) == -1) {
			err_out(errno, 2, ":842:Invalid supplementary GIDs\n", "");
		}
		cap_surrender (ocap);
	}

        /* su to "name" succeded - cannot audit after uid has changed. */
	capv = CAP_AUDIT_WRITE, ocap = cap_acquire (1, &capv);
	satvwrite(SAT_AE_IDENTITY, SAT_SUCCESS,
	    "SU|+|%s|su to user %s succeeded", myname, nptr);
	cap_surrender(ocap);

	/*
	 * Fire up a new array session and set up the correct project ID
	 */
	capv = CAP_SETUID, ocap = cap_acquire (1, &capv);
	newarraysess();
	setprid(getdfltprojuser(nptr));
	cap_surrender (ocap);

	/*
	 * set the new users user-ID.
	 */
	capv = CAP_SETUID, ocap = cap_acquire (1, &capv);
	if (setuid(uid) != 0)
		err_out(errno, 2, ":843:Invalid UID\n", "");
	cap_surrender (ocap);
	/*
	 * set the new users sat-ID.
	 */
	if (Sflag) {
		capv = CAP_AUDIT_CONTROL, ocap = cap_acquire (1, &capv);
		(void) satsetid(uid);
		cap_surrender (ocap);
	}
	/*
	 * set environment variables for new user;
	 * arg0 for exec of pshell must now contain
	 * `-' so that environment of new user is given
	 */

	ia_get_sh(uinfo, &ia_shellp);

	/*
	 * Set our MAC label prior to exec.
	 * We do it here so that "su - user -M 'not-dblow'" can succeed.
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

	if (eflag) {
		register char *tp;

		if (strlen(ia_dirp) < MAXPATHLEN - 6)
		    (void) strcat(homedir, ia_dirp);

		/* tested nptr length above */
		(void) strcat(logname, nptr);		/* M003 */
		(void) strcat(env_user, nptr);
		(void) strcat(mail, nptr);

		if ((hz = getenv("HZ")) && (strlen(hz) < ENV_SIZE))
			(void) strcat(hzname, hz);
		if ((tz = getenv("TZ")) && (strlen(tz) < ENV_SIZE))
			(void) strcat(tznam, tz);
		/*
		 * In a MAC environment...
		 * this chdir might fail if the directory of the new user
		 * we are su'ing to is not at the level of this process.
		*/
		(void) chdir(ia_dirp);
		envinit[envidx = 0] = homedir;
		envinit[++envidx] = ((uid == (uid_t)0) ? supath : path);
		envinit[++envidx] = logname;
		envinit[++envidx] = hzname;
		envinit[++envidx] = tznam;

		envinit[++envidx] = env_user;
		envinit[++envidx] = mail;

		if ((term = getenv("TERM")) && (strlen(term) < ENV_SIZE))
			envinit[++envidx] = strcat(termtyp, term);

		if ((tp = getenv("REMOTEUSER")) && (strlen(tp) < ENV_SIZE))
			envinit[++envidx] = strcat(rem_user,tp);

		if ((tp = getenv("REMOTEHOST")) && (strlen(tp) < ENV_SIZE))
			envinit[++envidx] = strcat(rem_host,tp);

		envinit[++envidx] = strcat(env_shell,
					    *ia_shellp ? ia_shellp : pshell);

#ifdef	AUX_LANG
		/*
		 * environment has higher priority than .lang file
		 */
		if (tp = getenv("LANG")) {
			if (strlen(tp) < ENV_SIZE)
			    (void)strcat(envlang, tp);
			else
			    tp = NULL;
		} else
		    tp = get_lang(envlang, ia_dirp);
		if(tp)
		    envinit[++envidx] = envlang;
#endif	/* AUX_LANG */
		envinit[++envidx] = NULL;
		environ = envinit;
		(void) strcpy(su, "-su");
	}

	/*
	 * if new user's shell field is not NULL or equal to /sbin/sh,
	 * set:
	 *	pshell = their shell
	 *	su = [-]last component of shell's pathname
	 */
	if (*ia_shellp != '\0' && (strcmp(pshell, ia_shellp) != 0) ) {
		char *ps;

		pshell = (char *)malloc((strlen(ia_shellp)+1));
		CHECK_MALLOC(pshell);
		strcpy(pshell, ia_shellp);

		(void) strcpy(su, eflag ? "-" : "" );

		/* Avoid overruns */
		if ((ps = strrchr(pshell, '/')) == NULL)
			ps = pshell;
		else
			ps++;
		if (strlen(ps) < ENV_SIZE) 
		    (void) strcat(su, ps);
	}

	/*
	 * close the master file so the file descriptor is not
	 * inherited by the exec'ed process.  Besides, it's no
	 * longer needed.
	 */
	ia_closeinfo(uinfo);

	log(su_ptr, nptr, 1, 1);	/*log to sulog and syslog*/

	/*
	 * if new user is privileged (in an ID-based privilege mechanism):
	 */
	if (uid == (uid_t)0) {
		/*
	 	 * if eflag not set, change environment to that of the
	  	 * privileged ID.
		 */
		if (!eflag)
			envalt(supath);
		/*
		 * if CONSOLE is defined, log there only if the privilege
		 * mechanism is ID-based AND the new user identity is that
		 * of the ``privileged-ID''.
		 */
		log(con_ptr, nptr, 1, 0);   /*log to console but not syslog*/
	}

	/*
	 * If this is a capabilities environment set the user's capability
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
	 * unconditionally close the file pointer to the console.
	 */
	(void) fclose(con_ptr);

	/*
	 * set the close-on-exec attributes if required
	 */
	if (close_fds)
		nuke_fds ();

	/*
	 * if additional arguments, exec shell program with array
	 *    of pointers to arguments:
	 *	-> if shell = /sbin/sh, then su = [-]su
	 *	-> if shell != /sbin/sh, then su = [-]last component of
	 *					     shell's pathname
	 */
	if (argc > 2) {
		argv[optind - optoffset] = su;
		(void) execv(pshell, &argv[optind - optoffset]);
	}

	/*
	 *
	 * if no additional arguments, exec shell with arg0 of su su_ptr:
	 *	-> if shell = /sbin/sh, then su = [-]su
	 *	-> if shell != /sbin/sh, then su = [-]last component of
	 *					     shell's pathname
	 */
	else {
		(void) execl(pshell, su, (char *)0);
	}

	pfmt(stderr, MM_ERROR, ":355:No shell\n");
	exit(3);

	/* NOTREACHED */
}


/*
 * Procedure:	envalt
 *
 * Notes:	This routine is called when a user is su'ing to the
 *		privileged ID (in an ID-based privilege mechanism)
 *		without specifying the - flag.  The user's PATH and
 *		PS1 variables are reset to the correct value for that
 *		ID.  All of the user's other environment variables
 *		retain 	their current values after the su (if they are
 *		exported).
 */
static	void
envalt(char *newpath)
{
	char	*suprmt = "PS1=# ";
	static const char nomem[] = ":312:Out of memory: %s\n";

	/*
	 * If user has PATH variable in their environment, change its value
	 *		to /bin:/etc:/usr/bin ;
	 *  if user does not have PATH variable, add it to the user's
	 *		environment;
	 *  if either of the above fail, an error message is printed.
	 */
	if ((putenv(newpath)) != 0) {
		pfmt(stderr, MM_ERROR, nomem, strerror(errno));
		exit(4);
	}
	/*
	 * If user has PROMPT variable in environment, change its value to #;
	 * if user doesn't have PROMPT variable, add it to the environment;
	 * if either of the above fail, an error message is printed.
	 */
	if ((putenv(suprmt)) != 0) {
		pfmt(stderr, MM_ERROR, nomem, strerror(errno));
		exit(4);
	}
	return;
}


/*
 * Procedure:	log
 *
 * Notes:	Logging routine
 *
 * Restrictions:
 *		cuserid:	none
 *		fclose:		none
 *
 *		towho = specified user (user being su'ed to)
 *		how = 0 if su attempt failed; 1 if su attempt succeeded
 *		flag = 1 to log with syslog; 0  do not log using syslog
 */
static	void
log(FILE *fptr, char *towho, int how, int flag)
{
	long now;
	struct tm *tmp;
	/*
	 * Don't log if the userid didn't change and the attempt succeeded
	 * In support of the -M and -C options.
	 * Fixes 471529
	 */
	if (how && towho && myname && !strcmp(towho, myname))
		return;

	now = time(0);
	tmp = localtime(&now);

	if (flag && (syslog_success || (syslog_fail && !how))) {
		openlog("su", LOG_PID | LOG_CONS, LOG_AUTH);
		syslog(LOG_NOTICE|LOG_AUTH, "%s: %s changing from %s to %s", 
			how ? "succeeded" : "failed",
			(ttyn + sizeof("/dev/") -1),
			myname ? myname : "", towho);
		closelog();
	}

	/*
	 * check if the "fptr" variable is valid.
	 * If so, write out to where it points.
	 */
        if (fptr != NULL) {
		(void) fprintf(fptr,"SU %.2d/%.2d %.2d:%.2d %c %s %s-%s\n",
			tmp->tm_mon+1,tmp->tm_mday,tmp->tm_hour,tmp->tm_min,
			how?'+':'-',(ttyn + sizeof("/dev/") -1),
			myname ? myname:"",towho); 
		(void) fclose(fptr);	/* close the file */
	}
	return;
}


/*
 * Procedure:	ask_prompt
 *
 * Notes:	"su" will NOT prompt for a password if:
 *
 *			1)  the new user does not require a password,
 *
 *			2)  the privilege mechanism supports ID-based
 *			    privileges and the real user is privileged.
 *
 *			3)  the real UID is 0.  "su" never prompted in
 *			    this case, so don't start now.  If the user's
 *			    real UID is 0, but doesn't have adequate
 *			    privilege, the "su" will fail anyway.
 *
 *			4)  the PROMPT keyword is defined and equal to
 *			    the string ``No''.  If this is the case, the
 *			    indicator "prompt" will be 0.  Otherwise it
 *			    will be 1.			    
 * #if SGI
 *			5) new user id is the same as current user id.
 *
 */
static int
ask_prompt(char *usr_pwdp, uid_t uid)
{
	uid_t r_uid;

	r_uid = getuid();


	if (*usr_pwdp == '\0' ||		/* #1, above */
	    r_uid == (uid_t) 0 ||		/* #2 & #3, above */
	    r_uid == uid)			/* #5, above */
		return (0);

	return (prompt);			/* #4, above */
}


/*
 * Procedure:	to
 *
 * Notes:	called in "main" routine when logging the "su"
 *		attempt to the console (if defined).  It is
 *		called if the alarm is caught.
 */
static void
to(void) {}


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

	/*
	 * If the /var/adm/sulog file is defined,
	 * log this as a bad attempt.
	 */
	log(su_ptr, nptr, 0, 1);	/*log to sulog and syslog*/

	/*
	 * Only log this failure to CONSOLE iff the
	 * new user-ID has been set and it's equal to
	 * the privileged ID (in an ID-based privilege
	 * mechanism).
	 */
	if (uid != -1) {
		/*
	 	 * close the master file.
		*/
		if (uinfo != (uinfo_t) NULL)
			ia_closeinfo(uinfo);

		if (uid == (uid_t)0) {
		        /*log to console but not syslog*/
			log(con_ptr, nptr, 0, 0); 
		}
	}

	(void) fclose(con_ptr);

	exit(astatus);
}


/*
 * Procedure:	open_con
 *
 * Restrictions:
 *		fopen:	none
 *
 * Notes:	returns a file pointer to the named file ``namep''.
 *		Only waits 30 seconds to open the file.  If the
 *		alarm goes off, NULL is returned.
 */
static	FILE	*
open_con(char *namep)
{
	FILE	*fptr = NULL;

	(void) sigset(SIGALRM, (void (*)())to);
	(void) alarm(30);
	fptr = fopen(namep, "a");
	(void) alarm(0);
	(void) sigset(SIGALRM, SIG_DFL);

	return fptr;
}
