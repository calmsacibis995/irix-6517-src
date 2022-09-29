/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)passwd:passwd.c	1.4.3.38"

/*	Copyright (c) 1987, 1988 Microsoft Corporation	*/
/*	  All Rights Reserved	*/

/*	This Module contains Proprietary Information of Microsoft  */
/*	Corporation and should be treated as Confidential.	   */
/*
 * Usage:
 *
 * Administrative:
 *
 *		passwd [name]
 *		passwd [-l|-d] [-n min] [-f] [-x max] [-w warn] name
 *		passwd -s [-a]
 *		passwd -s [name]
 *
 * Non-administrative:
 *
 *	 	passwd [-s] [name]
 *
 * Level:	SYS_PUBLIC
 *
 * Inheritable Privileges:	P_AUDIT,P_DEV,P_MACWRITE,P_DACREAD,P_DACWRITE
 *				P_SYSOPS,P_SETFLEVEL,P_OWNER
 *
 * Fixed Privileges:		P_MACREAD
 *
 * Files:	/etc/passwd
 *		/etc/shadow
 *		/etc/.pwd.lock
 *		/etc/default/passwd
 *		/etc/security/ia/index
 *		/etc/security/ia/master
 *
 * Notes:	passwd is a program whose sole purpose is to manage 
 *		the password file. It allows system administrator
 *		to add, change and display password attributes.
 *		Non privileged user can change password or display 
 *		password attributes which corresponds to their login name.
*/

/* LINTLIBRARY */
#include <stdio.h>
#include <signal.h>
#include <pwd.h>
#include <shadow.h>
#include <libgen.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <string.h>
#include <ctype.h>         /* isalpha(c), isdigit(c), islower(c), toupper(c) */
#include <errno.h>
#include <crypt.h>
#include <deflt.h>
#include <limits.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <ia.h>
#include <sys/systeminfo.h>
#include <locale.h>
#include <pfmt.h>
#include <sat.h>
#include <sys/resource.h>
#include <sys/mac.h>

#ifdef _SHAREII
#include <dlfcn.h>
#include <shareIIhooks.h>
SH_DECLARE_HOOK(SETMYNAME);
SH_DECLARE_HOOK(ISUSERADMIN);
SH_DECLARE_HOOK(PWPERM);
#endif /* _SHAREII */

#define	PWADMIN		"passwd"
#define	MAXSTRS		     20	/* max num of strings from password generator */
#define	MINWEEKS	     -1	/* minimum weeks before next password change */
#define	MAXWEEKS	     -1	/* maximum weeks before password change */
#define	MINLENGTH	      6	/* minimum length for passwords */
#define	WARNWEEKS	     -1	/* number of weeks before password expires
				   to warn the user */
#define PWHISTORY	"/etc/passwd.history"
#define PWHISTTEMP	"/etc/.passwd.history"

#ifdef DEBUG
#undef PASSWD
#define PASSWD "/usr/tmp/passwd"
#undef SHADOW
#define SHADOW "/usr/tmp/shadow"
#undef OPASSWD
#define OPASSWD "/usr/tmp/opasswd"
#undef OSHADOW
#define OSHADOW "/usr/tmp/oshadow"
#undef PASSTEMP
#define PASSTEMP "/usr/tmp/ptmp"
#undef SHADTEMP 
#define SHADTEMP "/usr/tmp/stmp"
#undef PWHISTORY
#define PWHISTORY "/usr/tmp/passwd.history"
#undef PWHISTTEMP
#define PWHISTTEMP "/usr/tmp/.passwd.history"
#endif

/* flags indicate password attributes to be modified */

#define	LFLAG	0x001		/* lock user's password  */
#define	DFLAG	0x002		/* delete user's  password */
#define	MFLAG	0x004		/* max field -- # of days passwd is valid */
#define NFLAG	0x008		/* min field -- # of days between passwd mods */
#define SFLAG	0x010		/* display password attributes */
#define FFLAG	0x020		/* expire  user's password */
#define AFLAG	0x040		/* display password attributes for all users*/
#define	PFLAG	0x080		/* change user's password */  
#define WFLAG	0x100		/* warn user to change passwd */
#define	PWGEN	0x077		/* passgen byte in flag field */
#define SAFLAG	(SFLAG|AFLAG)	/* display password attributes for all users*/

/* exit codes */

#define SUCCESS		0	/* succeeded */
#define NOPERM		1	/* No permission */
#define BADSYN		2	/* Incorrect syntax */
#define FMERR		3	/* File manipulation error */
#define FATAL		4	/* Old file can not be recover */
#define FBUSY		5	/* Lock file busy */
#define BADOPT		6	/* Invalid argument to option */
#define UFAIL		7	/* Unexpected failure  */
#define NOID		8	/* Unknown ID  */
#define	BADAGE		9	/* Aging is disabled	*/
 
/* define error messages */

static	const	char 
	*sorry = ":353:Sorry\n",
	*again = ":364:Try again later.\n",
/* usage message of non-password administrator */
 	*usage  = ":362:Usage: passwd [-s] [name]\n",
	*MSG_NP = ":354:Permission denied\n",
	*MSG_NV = ":356:Invalid argument to option -%c\n",
	*MSG_BS = ":357:Invalid combination of options\n",
	*MSG_FE = ":815:Unexpected failure. Password file(s) unchanged.\n",
	*MSG_FF = ":816:Unexpected failure. Password file(s) missing.\n",
	*MSG_FB = ":360:Password file(s) busy.\n",
	*MSG_UF = ":817:Unexpected failure.\n",
	*MSG_UI = ":818:Unknown logname: %s\n",
	*MSG_AD = ":361:Password aging is disabled\n",
	*MSG_NO = ":819:Password may only be changed at login time\n",
	*badusage = ":8:Incorrect usage\n",
	*badentry = ":365:Bad entry found in the password file.\n",
	*pwgenerr = ":820:Must select an offered value\n",
	*pwgenmsg = ":821:Select a password from the following:\n\n",
/* usage message of password administrator */
 	*sausage  = ":363:Usage:\n\tpasswd [name]\n\tpasswd  [-l|-d]  [-n min] [-f] [-x max] [-w warn] name\n\tpasswd -s [-a]\n\tpasswd -s [name]\n";

#define FAIL 		-1
#define NUMCP		13	/* number of characters for valid password */

/* print password status */

#define PRT_PWD(pwdp)	{\
	if (*pwdp == NULL) \
		 (void) fprintf(stdout, "NP  ");\
	else if (strlen(pwdp) < (size_t) NUMCP) \
		(void) fprintf(stdout, "LK  ");\
	else		(void) fprintf(stdout, "PS  ");\
}

#define PRT_AGE()	{\
	if (mastp->ia_max != -1) { \
		if (mastp->ia_lstchg) {\
			lstchg = mastp->ia_lstchg * DAY;\
			tmp = gmtime(&lstchg);\
			(void) fprintf(stdout,"%.2d/%.2d/%.2d  ",(tmp->tm_mon + 1),tmp->tm_mday,tmp->tm_year%100);\
		} else			(void) fprintf(stdout,"00/00/00  ");\
		if ((mastp->ia_min >= 0) && (mastp->ia_warn > 0))\
                        (void) fprintf(stdout, "%ld  %ld	%ld ", mastp->ia_min, mastp->ia_max, mastp->ia_warn);\
            	else if (mastp->ia_min >= 0) \
                        (void) fprintf(stdout, "%ld  %ld	", mastp->ia_min, mastp->ia_max);\
		else if (mastp->ia_warn > 0) \
                        (void) fprintf(stdout, "    %ld	%ld ",  mastp->ia_max, mastp->ia_warn);\
 		else \
                     	(void) fprintf(stdout, "    %ld	", mastp->ia_max);\
	}\
}

/* Macros used for password aging when not using shadow passwords */
#define DAY_WEEK 7	/* days per weeks */
#define DIG_CH	63	/* A character represents upto 64 
			   digits in a radix-64 notation */
#define SEC_WEEK (24 * DAY_WEEK * 60 * 60 )    /* seconds per week */

/*	 Convert base-64 ASCII string to long integer.  *
 * 	 Convert long integer to maxweek, minweeks.     *
 * 	 Convert current time from seconds to weeks.	*
 */ 	
#define CNV_AGE()	{\
	when = (long) a64l(pwd->pw_age);\
	maxweeks = when & 077;\
	minweeks = (when >> 6) & 077;\
	now = time ((long *) 0) / SEC_WEEK;\
}


extern	int	auditctl(),
		putiaent();

extern	int	_getpwent_no_yp;	/* no YP interp of /etc/passwd ents */
extern	int	_getpwent_no_shadow;	/* no passwds from /etc/shadow */

/* SGI IRIX 6.2 getpwent merges in SSDI, which allows custom implementations
 * of passwd database (and others as well). With SSDI, getpwent will
 * return entries from these custom implemetations, whereas passmt
 * only needs stuff from the "files". Setting _getpwent_no_ssdi to 1
 * turns off this behavior which is what passwd needs.
 */
extern int _getpwent_no_ssdi;

static	struct	master	*mastp;

static	char	*pw;

static	int	circ(),
		ckarg(),
		update(),
		display(),
		pwd_adm(),
		ck_ifadm(),
		ck_passwd(),
		std_pwgen(),
		update_sfile(),
		update_noshadow();

static	int 	retval = 0,
		login_only = 0,
	 	mindate, maxdate,		/* password aging information */
		historycnt, historydays,
	 	warndate, minlength;

static	void	call_pwgen(),
		init_defaults();

static	FILE	*get_pwgen(),
		*open_pwgen(),
		*get_pwvalidate(),
		*open_pwval();

static	char	nullstr[] = "";
static	char	newcryptpw[PASS_MAX+1];


/*
 * Procedure:		main
 *
 * Restrictions:
 *		getpwuid:	none
 *		getiasz:	none
 *		getianam:	none
 *		lckpwdf:	none
 *		ulckpwdf:	none
*/
main(argc, argv)
	int argc;
	char **argv;
{
	int	flag,
		pw_admin;
	uid_t	uid;
	char	*uname;
	struct rlimit rlimits;

	rlimits.rlim_cur=0;
	rlimits.rlim_max=0;
	setrlimit( RLIMIT_CORE, &rlimits);

	(void)setlocale(LC_ALL, "");
	(void)setcat("uxcore.abi");
	(void)setlabel("UX:passwd");


	/*
	 * Disable YP interpretation of /etc/passwd.  This is necessary
	 * so we can make an accurate copy via putpwent.  For the same
	 * reason, disable shadowing of /etc/shadow passwords.
	 */
	_getpwent_no_yp = 1;
	_getpwent_no_shadow = 1;

	/* SGI: turn off SSDI mapping for getpwent */
	_getpwent_no_ssdi = 1;

	/*
	 * get the real user ID.  The effective user ID will always
	 * be 0 since "passwd" is a setuid-on-exec to 0 program.
	*/
	uid = getuid();

	/*
	 * call the init_defaults() routine to set the tunables
	 * used by "passwd".
	*/
	init_defaults();

	/*
	 * a return value > 0 from ck_ifadm indicates that this
	 * is being run by a password administrator. Obviously,
	 * 0 means non-administrator.
	*/
	pw_admin = ck_ifadm(uid);

#ifdef _SHAREII
	/*
	 *      If we already have admin status, no ShareII checks
	 *      are needed. But if we don't, we may still have
	 *      permission if we have a ShareII Admin or Uselim flag.
	 *      
	 *      A Uselim flag gives us all the privileges of a normal
	 *      password administrator.
	 *      An Admin flag gives us privileges only over those
	 *      accounts within our scheduling group. Further checks
	 *      will need to be made later when we know the user
	 *      whose password is being changed.
	 */
	if (!pw_admin && sgidladd(SH_LIMITS_LIB, RTLD_LAZY))
	{
		static const char *Myname = "passwd";

		SH_HOOK_SETMYNAME(Myname);
		pw_admin = SH_HOOK_ISUSERADMIN();
	}
#endif /* _SHAREII */
	
	/* 
	 * ckarg() parses the arguments. In case of an error it 
	 * sets the retval and returns FAIL (-1).  It returns the
	 * value indicating which password attributes to modify.
	*/

	switch (flag = ckarg(argc, argv, pw_admin, uid)) { 
	case FAIL:		/* failed */
		exit(retval); 
		/* NOTREACHED */
	case SAFLAG:		/* display password attributes */
		exit(display((struct master *) NULL));
		/* NOTREACHED */
	default:	
		break;
	}

	argc -= optind;

	if (argc < 1 ) {

		struct	passwd	*p_ptr;

		/* We enable YP interpretation around this call
		   since the caller might be registered in YP. */
		_getpwent_no_yp = 0;
		if ((p_ptr = getpwuid(uid)) == NULL) { 
			(void)pfmt(stderr, MM_ERROR, badusage);
			(void)pfmt(stderr, MM_ACTION, !pw_admin ? usage : sausage);
			exit(NOPERM);
		}
		_getpwent_no_yp = 1;

		uname = strdup(p_ptr->pw_name);

		/*
	  	 * if flag set, must be displaying or modifying
	 	 * password aging attributes
		*/
		if (!flag)
			(void) pfmt(stderr, MM_INFO,
				":366:Changing password for %s\n", uname);
	}
	else
		uname = argv[optind];

	if (ia_openinfo(uname, &mastp) < 0) {
		char *yp_uname;

		/* ia_openinfo may have failed because the user is
		   an entry prefixed by '+' (i.e., YP user).
		   So we perform the operation again. */
		yp_uname = malloc(strlen(uname) + 2);
		yp_uname[0] = '+';
		strcpy (&yp_uname[1], uname);
		uname = yp_uname;
		/* turn NIS back on to get the info, turn it off afterwards
		 * Note that NIS wants the name without the + that we have added
		 * for use in the passwd file itself
		 */
		_getpwent_no_yp = 0;
		if (ia_openinfo(&uname[1], &mastp) < 0) {
			(void) pfmt(stderr, MM_ERROR, MSG_UF);
			exit(UFAIL);
		}
		_getpwent_no_yp = 1;

	}

	/*
	 * the following statement determines who is allowed to
	 * modify a password.  If this user isn't a password
	 * administrator and the real uid isn't equal to the
	 * user's password that was requested or UID_MAX, then
	 * the user doesn't have permission to change the password.
	 *
	 * NOTE:	a real uid of UID_MAX indicates that this
	 *		was called from the login scheme.
	*/
#ifdef _SHAREII
	/*
	 *      The pw_admin flag may be set because of SHAREII.
	 *      If this is the only authorisation we have, we need
	 *      to do a further check now that we know the user that
	 *      is having their password changed.
	 */
	if (!ck_ifadm(uid) && uid != UID_MAX) {
#else  /* _SHAREII */
	if (!pw_admin && uid != UID_MAX) {
#endif /* _SHAREII */		
		struct	master	*cmp_mastp = mastp;

		/*
		 * If this is a YP entry, then we need to get
		 * the UID by querying YP.  We do this by 
		 * calling ia_openinfo again with YP interpretation.
		 */
		if (uname[0] == '+')
		{
			_getpwent_no_yp = 0;

			/*
			 * When you turn on YP interpretation, the
			 * name must not begin with '+'.
			 */
			if (ia_openinfo(&uname[1], &cmp_mastp) < 0) {
                		(void) pfmt(stderr, MM_ERROR, MSG_UF);
                		exit(UFAIL);
        		}
			_getpwent_no_yp = 1;
		}
#ifdef _SHAREII
		/*
		 * We get here if we don't have standard (non-ShareII)
		 * passwd admin privileges.
		 * We may be either an ordinary non-admin user (pw_admin == 0)
		 * or a user with the ShareII Admin or Uselim flag set
		 * (pw_admin == 1).
		 * If ordinary user, we just test to make sure the password
		 * we are changing is our own.
		 * If ShareII Admin or Uselim, we use SHpwperm to see if
		 * we have permission on the password we are changing.
		 */
		if (
			uid != cmp_mastp->ia_uid
			&&
			(
				!pw_admin
				||
				!SH_HOOK_PWPERM(uid, cmp_mastp->ia_uid)
			)
		)
		{
#else  /* _SHAREII */
		if (uid != cmp_mastp->ia_uid) {
#endif /* _SHAREII */			
			(void) pfmt(stderr, MM_ERROR, MSG_NP);
	                exit(NOPERM);
		}
		
	}


		/*
		 * If the flag is set display/update password attributes.
		 */
		switch (flag) {
		case SFLAG:		/* display password attributes */
			exit(display(mastp));
			/* NOTREACHED */
		case 0:			/* changing user password */
			break;
		default:		/* changing user password attributes */
			/* lock the password file */
			if (lckpwdf() != 0) {
				(void) pfmt(stderr, MM_ERROR, MSG_FB);
				(void) pfmt(stderr, MM_ACTION, again);
				exit(FBUSY);
			}
			retval = update(flag, uname);
			(void) ulckpwdf();
			exit(retval);
		}

		/*
		 * prompt for the users new password, check the triviality
		 * and verify the password entered is correct.
		 */
		if ((retval = ck_passwd(&flag, pw_admin, uname)) != SUCCESS) {
			exit(retval);
		}

		/* lock the password file */
		if (lckpwdf() != 0) {
			(void) pfmt(stderr, MM_ERROR, MSG_FB);
			(void) pfmt(stderr, MM_ACTION, again);
			exit(FBUSY);
		}
		flag |= PFLAG;
		retval = update(flag, uname);
		(void) ulckpwdf();
		if (retval == SUCCESS) {
			cap_value_t cap_audit_write = CAP_AUDIT_WRITE;
			cap_t ocap = cap_acquire(1, &cap_audit_write);
			ia_audit("PASSWD", uname, 1, "Updated password/shadow files");
			cap_surrender(ocap);
		}
	exit(retval);
	/* NOTREACHED */
}


/* 
 * Procedure:	ck_passwd
 *
 * Restrictions:
 *		getpass:	none
 *
 * Notes:	Verify users old password. It also checks password 
 *		aging information to verify that user is authorized
 *		to change password.  Also, prompt for the users new 
 *		password, check the triviality and verify that the
 *		password entered is correct.
*/ 
static	int
ck_passwd(flagp, adm, uname)
	int	*flagp,
		adm;
	char	*uname;
{
	/*
	 * passwd calls getpass() to get the password. The getpass() routine
	 * returns at most PASS_MAX charaters.
	 */
	register int	now,
			err = 0;
	char	*pswd,
		buf[PASS_MAX+1],
	 	saltc[2],		/* crypt() takes 2 char string as salt */
		pwbuf[PASS_MAX+1],			
		opwbuf[PASS_MAX+1];
	time_t 	salt;
	int	ret, i, c,		/* for triviality checks */
		pw_genchar = 0;
	FILE	*iop;
	struct stat statbuf;
	cap_t ocap;
	cap_value_t cap_audit_write = CAP_AUDIT_WRITE;

	ret = SUCCESS;
	pwbuf[0] = buf[0] = '\0';		/* used for "while" loop */

	/*
	 * if the user isn't a password administrator and they had an
	 * old password, get it and verify.
	*/
	if (!adm && *mastp->ia_pwdp) {
		if ((pswd = getpass(gettxt(":378", "Old password:"))) == NULL) {
			ocap = cap_acquire(1, &cap_audit_write);
			ia_audit("PASSWD", uname, 0,
			    "Didn't provide old password");
			cap_surrender(ocap);
			(void) pfmt(stderr, MM_ERROR, sorry);
			return FMERR;
		}
		else {
			(void) strcpy(opwbuf, pswd);
			pw = crypt(opwbuf, mastp->ia_pwdp);
		}
		if (strcmp(pw, mastp->ia_pwdp) != 0) {
			ocap = cap_acquire(1, &cap_audit_write);
			ia_audit("PASSWD", uname, 0,
			    "Didn't provide correct old password");
			cap_surrender(ocap);
			(void) pfmt(stderr, MM_ERROR, sorry);
			return NOPERM;
		}
	}
	else {
		opwbuf[0] = '\0';
	}
	/* password age checking applies */
	if (mastp->ia_max != -1 && mastp->ia_lstchg != 0) {
		now  =  DAY_NOW;
		if (mastp->ia_lstchg <= now) {
			if (!adm && ( now < mastp->ia_lstchg  + mastp->ia_min)) { 
				ocap = cap_acquire(1, &cap_audit_write);
				ia_audit("PASSWD", uname, 0,
				    "Changed too recently");
				cap_surrender(ocap);
				(void) pfmt(stderr, MM_ERROR,
					":379:Sorry: < %ld days since the last change\n",
					mastp->ia_min);
				return NOPERM;
			}
			if (mastp->ia_min > mastp->ia_max && !adm) { 
				ocap = cap_acquire(1, &cap_audit_write);
				ia_audit("PASSWD", uname, 0,
				    "password not changeable by user");
				cap_surrender(ocap);
				(void) pfmt(stderr, MM_ERROR,
					":380:You may not change this password\n");
				return NOPERM;
			}
		}
	}
	/* aging is turned on */
	else if(mastp->ia_lstchg == 0 && mastp->ia_max > 0 || mastp->ia_min > 0) {
		ret = SUCCESS;
	}
	else if (stat(SHADOW, &statbuf) == 0) {
		/* aging not turned on */
		/* so turn on passwd for user with default values */
		*flagp |= MFLAG;
		*flagp |= NFLAG;
		*flagp |= WFLAG;
	}
	/*
	 * if this system supports password generating programs,
	 * "exec" the program now.  If the generator fails, or the
	 * the expected password generator does not exist, follow
	 * the standard password algorithm.
	*/
	/* the PWGEN flag is never set in IRIX, and we ignore pw_genchar */
	if (!adm && /*(pw_genchar = (mastp->ia_flag & PWGEN)) &&*/
		((iop = get_pwgen(pw_genchar)) != NULL)) {

		call_pwgen(iop, (char *)&buf, (char *)&pwbuf);
	}
	/*
	 * didn't call password generator or it failed for some
	 * reason so now get the password the old-fashioned way.
	*/
	if (!strlen(buf)) {
		if ((err = std_pwgen(adm, uname, opwbuf,
				 (char *)&buf, (char *)&pwbuf))) {
			return err;
		}
	}
	/* if this system has been configured for an external password
	 * validator, call it with the new password.
	 */
	if (!adm && ((iop = get_pwvalidate()) != NULL))
	{
		if ( (err = call_pwvalidate(iop,pwbuf)) != 0 )
		{
			fprintf(stderr,"Sorry, your new password is not acceptable\n");
			fflush(stderr);
			return err;
		}
	}
		
	/*
	 * Construct salt, then encrypt the new password
	*/
	(void) time((time_t *)&salt);
	salt += (long)getpid();

	saltc[0] = salt & 077;
	saltc[1] = (salt >> 6) & 077;
	for (i = 0; i < 2; i++) {
		c = saltc[i] + '.';
		if (c>'9') c += 7;
		if (c>'Z') c += 6;
		saltc[i] = (char) c;
	}
	pw = crypt(pwbuf, saltc);
	strcpy(newcryptpw,pw);
	pw = &newcryptpw[0];   /* crypt has a static buffer and we'll be
				* using crypt in ck_pwhistory, so save the
				* new encrypted passwd
				*/
	if ( !adm && ((err = ck_pwhistory(uname,pwbuf)) != SUCCESS ) )
		return err;

	return ret;
}

/*
 * Procedure:	update
 *
 * Restrictions:
 *		stat()		none
 *		unlink()	none
 *		creat()		none
 *		fopen()		none
 *		getspent()	none
 *		putspent()	none
 *		chmod()		none
 *		chown()		none
 *
 * Notes:	updates the password file. It takes "flag" as an argument
 *		to determine which password attributes to modify. It returns
 *		0 for success and > 0 for failure.
 *
 *		Side effect: Variable sp points to NULL.
*/
static	int 
update(flag, uname)
	int	flag;
	char	*uname;
{
	register int i, found = 0;
	struct	stat	buf;
	struct	spwd	*sp;
	int	fd,
	 	sp_error = 0,
	 	end_of_file = 0;
	FILE	*tsfp;
	mac_t new_lp, orig_lp = (mac_t) 0;
	int retval, mac_enabled = 0;

	/*
		If mac enabled, save the current label
	*/
	if ((mac_enabled = (sysconf(_SC_MAC)) > 0)) {
		if ((orig_lp = mac_get_proc()) == (mac_t) NULL ) {
			(void) pfmt(stderr, MM_ERROR, MSG_UF);
			exit(UFAIL);
		}
	}
					
	/* ignore all the signals */
	for (i = 1; i < NSIG; i++)
		(void) sigset(i, SIG_IGN);

 	/* Clear the errno. It is set because SIGKILL can not be ignored. */

	errno = 0;

	/*
	 * Mode of the shadow file should be 400.
	 * We should be able to read it for two reasons:
	 *
	 *	1) the process has the P_MACREAD privilege.
	 *
	 *	2) the discretionary access has been set to
	 *	   "root", owner of the shadow file.
	*/
	if (stat(SHADOW, &buf) < 0) {
		/*	
			If mac is enabled, retrieve the mac from the passwd 
			file and set the process to that label.
		*/
		if (mac_enabled) {
			if ((new_lp = mac_get_file (PASSWD)) == (mac_t) NULL ) {
				(void) pfmt(stderr, MM_ERROR, MSG_UF);
				exit(UFAIL);
			}			
			if (mac_set_proc (new_lp) == -1) {
				(void) pfmt(stderr, MM_ERROR, MSG_UF);
				exit(UFAIL);
			}			
			(void) mac_free(new_lp);
		}			
		update_noshadow(flag, uname);
		if (mac_enabled) {
			(void) mac_set_proc (orig_lp);
			(void) mac_free(orig_lp);
		}			
		update_pwhistory(uname,pw);
		return 0;
		/*
		 * (void) pfmt(stderr, MM_ERROR, MSG_FE);
		 * return FMERR;
		 */
	}
	/*	
		If mac is enabled, retrieve the mac from the shadow 
		file and set the process to that label.
	*/
	if (mac_enabled) {
		if ((new_lp = mac_get_file (SHADOW)) == (mac_t) NULL ) {
			(void) pfmt(stderr, MM_ERROR, MSG_UF);
			exit(UFAIL);
		}			
		if (mac_set_proc (new_lp) == -1) {
			(void) pfmt(stderr, MM_ERROR, MSG_UF);
			exit(UFAIL);
		}			
		(void) mac_free(new_lp);
	}	
	/* SGI: lose the leading + when updating shadow file */
	if ( *uname == '+' ) uname++;

	/*
	 * do unconditional unlink of temporary shadow file.
	 * Shouldn't exist anyway!!
	*/
	(void) unlink(SHADTEMP);
	/*
	 * create temporary file.
	*/
	if ((fd = creat(SHADTEMP, 0600)) == FAIL) {
		/*
		 * couldn't create temporary shadow file.
		 * Big problems, better exit with message.
		*/
		if (mac_enabled) {
			(void) mac_set_proc (orig_lp);
			(void) mac_free(orig_lp);
		}
		(void) pfmt(stderr, MM_ERROR, MSG_FE);
		return FMERR;
	}
	(void) close(fd);

	if ((tsfp = fopen(SHADTEMP, "w")) == NULL) {
		if (mac_enabled) {
			(void) mac_set_proc (orig_lp);
			(void) mac_free(orig_lp);
		}			
		(void) pfmt(stderr, MM_ERROR, MSG_FE);
		return FMERR;
	}

	/*
	 * copy passwd file to temp, replacing matching lines
	 * with new password attributes.
	*/

	end_of_file = sp_error = 0;

	/*
	 * while it would seem to be good security practice to turn
	 * OFF the P_MACREAD privilege at this point, it isn't because
	 * "passwd" has that privilege in it's FIXED set to be able to
	 * READ this file.  Therefore, turning it OFF would be an ERROR.
	*/
	while (!end_of_file) {
	if ((sp = getspent()) != NULL) {
		if (strcmp(sp->sp_namp, uname) == 0) { 
			found = 1;
			/* LFLAG and DFLAG should be checked before FFLAG.
			   FFLAG clears the sp_lstchg field. We do not
			   want sp_lstchg field to be set if one execute
			   passwd -d -f name or passwd -l -f name.
			*/

			if (flag & LFLAG) {	 /* lock password */
				sp->sp_pwdp = pw;
				sp->sp_lstchg = DAY_NOW;
				(void) strcpy(mastp->ia_pwdp, pw);
				mastp->ia_lstchg = DAY_NOW;
			} 
			if (flag & DFLAG) {	 /* delete password */
				/*
				 * set the aging information to -1.
				 */
				sp->sp_min = -1;
				sp->sp_max = -1;
				mastp->ia_min = -1;
				mastp->ia_max = -1;
				/*
				 * NULL out the current password
				 */
				sp->sp_pwdp = nullstr;
				sp->sp_lstchg = DAY_NOW;
				mastp->ia_pwdp[0] = '\0';
				mastp->ia_lstchg = DAY_NOW;
			} 
			if (flag & FFLAG) {	 /* expire password */
				sp->sp_lstchg = (long) 0;
				mastp->ia_lstchg = (long) 0;
			}
			if (flag & MFLAG)  { 	/* set max field */
				if (!(flag & NFLAG) && mastp->ia_min == -1) {
					sp->sp_min = 0;
					mastp->ia_min = 0;
				}
				if (maxdate == -1) { 	/* trun off aging */
					sp->sp_min = -1;
					mastp->ia_min = -1;
					sp->sp_warn = -1;
					mastp->ia_warn = -1;
				}
				else if (mastp->ia_max == -1) {
					sp->sp_lstchg = 0;
					mastp->ia_lstchg = 0;
				}
				sp->sp_max = maxdate;
				mastp->ia_max = maxdate;
			}
			if (flag & NFLAG) {   /* set min field */
				if (mastp->ia_max == -1 && mindate != -1) {
					(void) pfmt(stderr, MM_ERROR, MSG_AD);
					(void) pfmt(stderr, MM_ACTION, sausage);
					endspent();
					(void) unlink(SHADTEMP);
					if (mac_enabled) {
						(void) mac_set_proc (orig_lp);
						(void) mac_free(orig_lp);
					}			
					return BADAGE;
				}
				sp->sp_min = mindate;
				mastp->ia_min = mindate;
			}
			if (flag & WFLAG) {   /* set warn field */
				if (mastp->ia_max == -1 && warndate != -1) {
					(void) pfmt(stderr, MM_ERROR, MSG_AD);
					(void) pfmt(stderr, MM_ACTION, sausage);
					endspent();
					(void) unlink(SHADTEMP);
					if (mac_enabled) {
						(void) mac_set_proc (orig_lp);
						(void) mac_free(orig_lp);
					}			
					return BADAGE;
				}
				sp->sp_warn = warndate;
				mastp->ia_warn = warndate;
			}
			if (flag & PFLAG)  {	/* change password */
				sp->sp_pwdp = pw;
				(void) strcpy(mastp->ia_pwdp, pw);
				/* update the last change field */
				sp->sp_lstchg = DAY_NOW;
				mastp->ia_lstchg = DAY_NOW;
				if (mastp->ia_max == 0) {   /* turn off aging */
					sp->sp_max = -1;
					sp->sp_min = -1;
					mastp->ia_max = -1;
					mastp->ia_min = -1;
				}
			}
		}
		if (putspent (sp, tsfp) != 0) { 
			endspent();
			(void) unlink(SHADTEMP);
			if (mac_enabled) {
				(void) mac_set_proc (orig_lp);
				(void) mac_free(orig_lp);
			}			
			(void) pfmt(stderr, MM_ERROR, MSG_FE);
			return FMERR;
		}

	} else {
 		if (errno == 0) 
			/* end of file */
			end_of_file = 1;	
		else if (errno == EINVAL) {
			/*bad entry found in /etc/shadow, skip it */
			errno = 0;
			sp_error++;
		}  else
			/* unexpected error found */
			end_of_file = 1;
	}
	} /*end of while*/

	endspent();

	if (sp_error >= 1)
		pfmt(stderr, MM_ERROR, badentry);

	if (fclose (tsfp)) {
		(void) unlink(SHADTEMP);
		if (mac_enabled) {
			(void) mac_set_proc (orig_lp);
			(void) mac_free(orig_lp);
		}			
		(void) pfmt(stderr, MM_ERROR, MSG_FE);
		return FMERR;
	}
	/*
	 * Check if user name exists
	*/
	if (!found) {
		(void) unlink(SHADTEMP);
		if (mac_enabled) {
			(void) mac_set_proc (orig_lp);
			(void) mac_free(orig_lp);
		}			
		(void) pfmt(stderr, MM_ERROR, MSG_FE);
		return FMERR;
	}
	/* we're committed at this pt, so go ahead and update the history file */
	update_pwhistory(uname,pw);
	/*
	 * change the mode of the temporary file to the mode
	 * of the original shadow file.
	*/
	if (chmod(SHADTEMP, buf.st_mode) != SUCCESS) {
		(void) unlink(SHADTEMP);
		if (mac_enabled) {
			(void) mac_set_proc (orig_lp);
			(void) mac_free(orig_lp);
		}			
		(void) pfmt(stderr, MM_ERROR, MSG_FE);
		return FMERR;
	}
	/*
	 * Do a chown() on the temporary shadow file to the
	 * owner and group of the original shadow file.
	*/
	if (chown(SHADTEMP, buf.st_uid, buf.st_gid) != SUCCESS) {
		(void) unlink(SHADTEMP);
		if (mac_enabled) {
			(void) mac_set_proc (orig_lp);
			(void) mac_free(orig_lp);
		}			
		(void) pfmt(stderr, MM_ERROR, MSG_FE);
		return FMERR;
	}

	/*
	 * Rename temp file back to appropriate passwd file and return.
	*/
	retval = update_sfile(SHADOW, OSHADOW, SHADTEMP, uname);

	if (mac_enabled) {
		(void) mac_set_proc (orig_lp);
		(void) mac_free(orig_lp);
	}			

	return retval;
}


/*
 * Procedure:	circ
 *
 * Notes:	compares the password entered by user with the login name
 *		of the user to determine if the password is just a circular
 *		representation of the users login name.  Since this is a
 *		common practice for ease of remembering one's password, it
 *		is disallowed for just that reason;  someone attempting to
 *		"break-in" would use this method and therefore be successful.
*/
static	int
circ(s, t)
	char *s, *t;
{
	char c, *p, *o, *r, buff[25], ubuff[25], pubuff[25];
	int i, j, k, l, m;
	 
	m = 2;
	i = strlen(s);
	o = &ubuff[0];

	for (p = s; c = *p++; *o++ = c) 
		if (islower(c))
			 c = toupper(c);

	*o = '\0';
	o = &pubuff[0];

	for (p = t; c = *p++; *o++ = c) 
		if (islower(c)) 
			c = toupper(c);

	*o = '\0';
	p = &ubuff[0];

	while (m--) {
		for (k = 0; k  <=  i; k++) {
			c = *p++;
			o = p;
			l = i;
			r = &buff[0];
			while (--l) 
				*r++ = *o++;
			*r++ = c;
			*r = '\0';
			p = &buff[0];
			if (strcmp(p, pubuff) == 0) 
				return 1;
		}
		p = p + i;
		r = &ubuff[0];;
		j = i;
		while (j--) 
			*--p = *r++;
	}
	return SUCCESS;
}


/*
 * Procedure:	ckarg
 *
 * Restrictions:
 *		access(2):	none
 *
 * Notes:	This function parses and verifies the 	
 *		arguments.  It takes three parameters:			
 *
 *			argc => # of arguments				
 *			argv => pointer to an argument			
 *			adm  => password administrator flag
 *
 *		In case of an error it prints the appropriate error
 *		message, sets the retval and returns FAIL(-1).	 		
*/
static	int
ckarg(argc, argv, adm, real_uid)
	int	argc;
	char	**argv;
	int	adm;
	uid_t	real_uid;
{
	extern	char	*optarg;
	register int	c, flag = 0;
	char	*char_p,
		*lkstring = "*LK*";	/*lock string  to lock user's password*/

	while ((c = getopt(argc, argv, "aldfsx:n:w:")) != EOF) {
		switch (c) {
		case 'd':		/* delete the password */
			/* Only password administrator can execute this */
			if (!pwd_adm(adm))
				return FAIL;
			if (flag & (LFLAG|SAFLAG|DFLAG)) {
				(void) pfmt(stderr,MM_ERROR, MSG_BS);
				(void) pfmt(stderr, MM_ACTION, sausage); 
				retval = BADSYN;
				return FAIL;
			}
			flag |= DFLAG;
			pw = '\0';
			break;
		case 'l':		/* lock the password */
			/* Only password administrator can execute this */
			if (!pwd_adm(adm))
				return FAIL;
			if (flag & (DFLAG|SAFLAG|LFLAG))  {
				(void) pfmt(stderr,MM_ERROR, MSG_BS);
				(void) pfmt(stderr, MM_ACTION, sausage); 
				retval = BADSYN;
				return FAIL;
			}
			flag |= LFLAG;
			pw = lkstring;
			break;
		case 'x':		/* set the max date */
			/* Only password administrator can execute this */
			if (!pwd_adm(adm))
				return FAIL;
			if (flag & (SAFLAG|MFLAG)) {
				(void) pfmt(stderr,MM_ERROR, MSG_BS);
				(void) pfmt(stderr, MM_ACTION, sausage); 
				retval = BADSYN;
				return FAIL;
			}
			flag |= MFLAG;
			if ((maxdate = (int) strtol(optarg, &char_p, 10)) < -1
			    || *char_p != '\0' || strlen(optarg) <= (size_t) 0) {
				(void) pfmt(stderr, MM_ERROR, MSG_NV, 'x');
				retval = BADOPT;
				return FAIL;
			}
			break;
		case 'n':		/* set the min date */
			/* Only password administrator can execute this */
			if (!pwd_adm(adm))
				return FAIL;
			if (flag & (SAFLAG|NFLAG)) { 
				(void) pfmt(stderr,MM_ERROR, MSG_BS);
				(void) pfmt(stderr, MM_ACTION, sausage); 
				retval = BADSYN;
				return FAIL;
			}
			flag |= NFLAG;
			if (((mindate = (int) strtol(optarg, &char_p,10)) < 0 
			    || *char_p != '\0') || strlen(optarg) <= (size_t)0) {
				(void) pfmt(stderr, MM_ERROR, MSG_NV, 'n');
				retval = BADOPT;
				return FAIL;
			} 
			break;
		case 'w':		/* set the warning field */
			/* Only password administrator can execute this */
			if (!pwd_adm(adm))
				return FAIL;
			if (flag & (SAFLAG|WFLAG)) { 
				(void) pfmt(stderr,MM_ERROR, MSG_BS);
				(void) pfmt(stderr, MM_ACTION, sausage); 
				retval = BADSYN;
				return FAIL;
			}
			flag |= WFLAG;
			if (((warndate = (int) strtol(optarg, &char_p,10)) < 0 
			    || *char_p != '\0') || strlen(optarg) <= (size_t)0) {
				(void) pfmt(stderr, MM_ERROR, MSG_NV, 'w');
				retval = BADOPT;
				return FAIL;
			} 
			break;
		case 's':		/* display password attributes */
			if (flag && (flag != AFLAG)) { 
				(void) pfmt(stderr,MM_ERROR, MSG_BS);
				(void) pfmt(stderr, MM_ACTION, !adm ? usage : sausage); 
				retval = BADSYN;
				return FAIL;
			} 
			flag |= SFLAG;
			break;
		case 'a':		/* display password attributes */
			/* Only password administrator can execute this */
			if (!pwd_adm(adm))
				return FAIL;
			if (flag && (flag != SFLAG)) { 
				(void) pfmt(stderr,MM_ERROR, MSG_BS);
				(void) pfmt(stderr, MM_ACTION, sausage); 
				retval = BADSYN;
				return FAIL;
			} 
			flag |= AFLAG;
			break;
		case 'f':		/* expire password attributes */
			/* Only password administrator can execute this */
			if (!pwd_adm(adm))
				return FAIL;
			if (flag & (SAFLAG|FFLAG)) { 
				(void) pfmt(stderr,MM_ERROR, MSG_BS);
				(void) pfmt(stderr, MM_ACTION, sausage); 
				retval = BADSYN;
				return FAIL;
			} 
			flag |= FFLAG;
			break;
		case '?':
			(void) pfmt(stderr, MM_ACTION, !adm ? usage : sausage);
			retval = BADSYN;
			return FAIL;
		}
	}

	argc -=  optind;
	if (argc > 1) {
		(void) pfmt(stderr, MM_ERROR, badusage);
		(void) pfmt(stderr, MM_ACTION, !adm ? usage : sausage);
		retval = BADSYN;
		return FAIL;
	}
	/*
	 * if no options specified, or only the show option, return.
	*/
	if (!flag || (flag == SFLAG)) {
		return flag;
	}
	if (flag == AFLAG) {
		(void) pfmt(stderr, MM_ERROR, badusage);
		(void) pfmt(stderr, MM_ACTION, sausage);
		retval = BADSYN;
		return FAIL;
	}
	if (flag != SAFLAG && argc < 1) {
		(void) pfmt(stderr, MM_ERROR, badusage);
		(void) pfmt(stderr, MM_ACTION, sausage);
		retval = BADSYN;
		return FAIL;
	}
	if (flag == SAFLAG && argc >= 1) {
		(void) pfmt(stderr, MM_ERROR, badusage);
		(void) pfmt(stderr, MM_ACTION, sausage);
		retval = BADSYN;
		return FAIL;
	}
	if ((maxdate == -1) &&  (flag & NFLAG)) {
		(void) pfmt(stderr, MM_ERROR, MSG_NV, 'x');
		retval = BADOPT;
		return FAIL;
	}
	return flag;
}

/*
 * Procedure:	display
 *
 * Restrictions:
 *		open()		none
 *		getianam()	none
 *		stat(2):	none
 *		mmap(2):	none
 *
 * Notes:	displays password attributes. It takes master struct
 *		pointer as a parameter. If the pointer is NULL then it
 *		displays password attributes for all entries on the file.
 *		Returns 0 for success and positive integer for failure.	      
*/
static	int
display(mastp)
	struct master *mastp;
{
    	void display_one_entry();
	int found = 0;

	if (mastp != NULL) {
	    	display_one_entry(mastp);
		return SUCCESS;
	} 
	errno = 0;

	while (ia_openinfo_next(&mastp) == 0) {
		found++;
		display_one_entry(mastp);
	}

	if (found == 0) {
	    (void) pfmt(stderr, MM_ERROR, MSG_FF);
	    return (FATAL);
	}
	return (SUCCESS);
}


void display_one_entry(mastp)
     struct master *mastp;
{
	struct tm *tmp;
	long lstchg;
	char *name;
	int ypflag = 0;

	name = mastp->ia_name;
	ypflag = 0;

	if (name && *name == '+') {			/* NIS entry */
	    name++;
	    ypflag++;
	    if (*name == '@') {
		ypflag++;
		name++;
	    }
	}
	if (ypflag == 1 && name[0] == '\0')
	    name = "Entire NIS map";
	(void) fprintf(stdout,"%-15s ", name);
	if (!ypflag || *mastp->ia_pwdp != NULL) {
	    PRT_PWD(mastp->ia_pwdp);
	} else
	    (void) fprintf(stdout, "    ");
	switch (ypflag) {
	    default:
	    case 0:
	    (void) fprintf(stdout, "%5d  %5d ", mastp->ia_uid, mastp->ia_gid);
		break;
	    case 1:
		(void) fprintf(stdout, "%-12s ", "NIS entry");
		break;
	    case 2:
		(void) fprintf(stdout, "%-12s ", "NIS netgroup");
		break;
	}
	if (*mastp->ia_dirp != NULL) 
		(void) fprintf(stdout,"%s  ", mastp->ia_dirp);
	if (*mastp->ia_shellp != NULL) 
		(void) fprintf(stdout,"%s  ", mastp->ia_shellp);
	PRT_AGE();
	(void) fprintf(stdout, "\n");
	ia_closeinfo(mastp);
}

/*
 * Procedure:	pwd_adm
 *
 * Notes:	determines if the user is a password administrator
 *		or not.  If not, print a diagnostic message, set the
 *		return value for the process and return to the caller
 *		indicating the user is NOT an administrator.  Other-
 *		wise return to the caller indicating the user is a
 *		password administrator.
*/
static	int
pwd_adm(adm)
	register int	adm;
{
	if(!adm) {
		(void) pfmt(stderr, MM_ERROR, MSG_NP);
		retval = NOPERM;
		return 0; 
	} 
	return 1;
}


/*
 * Procedure:	ck_ifadm
 *
 * Restrictions:
 *		secsys(2):	none
 *		sysinfo(2):	SEE (2)
 *
 * Notes:	"passwd" is an interesting command.  It really has a
 *		split personality meaning that it must work correctly
 *		under different conditions.  These are some of those
 *		conditions:
 *
 *			1) if the privilege mechanism is ID-based
 *			   and the REAL user is privileged, then
 *			   that user can also be the password admin-
 *			   istrator.
 *
 *			2) if the privilege mechanism is file based
 *			   (i.e., LPM), then the determination of
 *			   whether or not the user is also the pass-
 *			   word administrator is based on the premiss
 *			   that if this process can adjust the system
 *			   clock then the user is a password adminis-
 *			   trator.
 *
 *			   Otherwise, the user is a non-administrator
 *			   and limited to non-administrator type functions.
 *
 *			   SEE NOTE: below
 *
 *		Further restrictions of what non-administrators can and
 *		can't do can be found in the "ckarg" routine.
*/
static	int
ck_ifadm(user_uid)
	uid_t	user_uid;
{
	return (0==user_uid);
}


/*
 * Procedure:	update_sfile
 *
 * Restrictions:
 *		access()	none
 *		unlink()	none
 *		rename()	none
 *		putiaent()	none
 *
 * Notes:	This routine performs all the necessary checks when moving
 *		the current shadow file to the old shadow file and renaming
 *		the temporary shadow file to the current shadow file.
*/
static	int 
update_sfile(sfilep, osfilep, tsfilep, uname)
	char	*sfilep;		/* shadow file */
	char	*osfilep;		/* old shadow file */
	char	*tsfilep;		/* temporary shadow file */
	char	*uname;
{
	/*
	 * First check to see if there was an existing shadow file.
	*/
	if (access(sfilep, 0) == 0) {
		/*
		 * if so, remove old shadow file
		*/
		if (access(osfilep, 0) == 0) {
			if (unlink(osfilep) < 0) {
				(void) unlink(tsfilep);
				(void) pfmt(stderr, MM_ERROR, MSG_FE);
				return FMERR;
			}
		}
		/*
		 * link shadow file to old shadow file
		*/
		if (link(sfilep, osfilep) < 0 ) {
			(void) unlink(tsfilep);
			(void) pfmt(stderr, MM_ERROR, MSG_FE);
			return FMERR;
		}
	}
	/*
	 * rename temporary shadow file to shadow file
	*/
	if (rename(tsfilep, sfilep) < 0 ) {
		(void) unlink(sfilep);
		if (link(osfilep, sfilep) < 0 ) { 
			(void) pfmt(stderr, MM_ERROR, MSG_FF);
			return FATAL;
		}
		(void) pfmt(stderr, MM_ERROR, MSG_FE);
		return FMERR;
	}
	/* let NSD know about the change */
	rmdir("/ns/.local/shadow.byname");
	return SUCCESS;
}


/*
 * Procedure:	get_pwgen
 *
 * Restrictions:
 *		defopen:	none
 *
 * Notes:	does validation for determining what the password
 *		generator is, reads the default passwd file, and
 *		"exec" the password generator if it exists.
 *		This routine calls "open_pwgen" that creates a pipe and
 *		does a "fork()" and "exec()".
*/
static	FILE	*
get_pwgen(pw_ident)
	int	pw_ident;
{
	char	prog[BUFSIZ],
		passgen[BUFSIZ],
		*progp = &prog[0],
		*pstrp = &passgen[0];
	FILE	*iop;

	/*
	 * there is an inclination to clear the P_MACREAD privilege in
	 * this routine so that a user who doesn't have non-privileged
	 * access to the necessary files would fail, but that would be
	 * an error since ordinary users on a system with MAC installed
	 * but no file-based privilege mechanism also need access to
	 * these particular files.
	*/
	progp = '\0';
	/* base code used multiple PASSGEN options, but the necessary
	 * flag was never being set, so just ignore it
	 */
	/*(void) sprintf(pstrp, "PASSGEN%c", pw_ident);*/
	strcpy(pstrp,"PASSGEN");

	if ((iop = defopen(PWADMIN)) == NULL) {
		/*
		 * couldn't open "passwd" default file, no generator!
		*/
		return (FILE *)NULL;
	}
	if ((progp = defread(iop, pstrp)) == NULL) {
		/*
		 * didn't find the password generator keyword in the
		 * "passwd" default file, no generator!
		*/
		defclose(iop);
		return (FILE *)NULL;
	}
	else if (*progp) {
		progp = strdup(progp);
		defclose(iop);
		/*
		 * found a value for a possible password generator, now
		 * check to see if it's a full pathname.  If not, error!
		*/
		if (strncmp(progp, (char *)"/", 1)) {
			return (FILE *)NULL;
		}
		/*
		 * otherwise, try to "exec" whatever was found in the
		 * appropriate password generator field.
		*/
		if ((iop = open_pwgen(progp)) == NULL) {
			return (FILE *)NULL;
		}
		return iop;
	}
	defclose(iop);
	return (FILE *)NULL;
}


/*
 * Procedure:	open_pwgen
 *
 * Restrictions:
 *		fork(2):	none
 *		secsys(2):	none
 *		execl:		P_MACREAD (ID-based)
 *				P_ALLPRIVS (file-based)
 *
 * Notes:	sets up a pipe, forks and executes the password
 *		generator program.  If the exec fails it returns
 *		a value to indicate that it did fail.  On success,
 *		it returns a file pointer to the pipe.
*/
static	FILE *
open_pwgen(progp)
	char *progp;
{
	register int	i;
	int		pfd[2];
	FILE		*iop;

	(void) pipe(pfd);
	if ((i = fork()) < 0) {
		(void) close(pfd[0]);
		(void) close(pfd[1]);
		return (FILE *)NULL;
	}
	else if (i == 0) {
		(void) close(pfd[0]);
		(void) close(1);
		(void) dup(pfd[1]);
		(void) close(pfd[1]);
		for (i = 5; i < 15; i++)
			(void) close(i);

		(void) execl(progp, progp, 0);
		(void) close(1);
		exit(1);		/* tell parent that 'execl' failed */
	}
	else {
		(void) close(pfd[1]);
		iop = fdopen(pfd[0], "r");
		return iop;
	}
	/* NOTREACHED */
}



/* diagnostic strings for following routine */
static	const	char
	*tshort = ":898:Password is too short - must be at least %d characters\n",
	*nocirc = ":371:Password cannot be circular shift of logonid\n",
	*s_char = ":372:Password must contain at least two alphabetic characters\n\tand at least one numeric or special character.\n",
	*df3pos = ":373:Passwords must differ by at least 3 positions\n",
	*tomany = ":375:Too many tries\n",
	*nmatch = ":376:They don't match\n",
	*tagain = ":377:Try again.\n";

/*
 * Procedure:	std_pwgen
 *
 * Restrictions:
 *		getpass:	none
 *
 * Notes:	does the standard password algorithm checking of the
 *		user's entry for the requested password.  If it succeeds
 *		the return is 0, otherwise, an error.
*/
static	int
std_pwgen(adm, uname, opass, buf, pwbuf)
	int	adm;
	char	*uname,
		*opass,
		*buf,
		*pwbuf;
{
	register int	i, j, k,
			pwlen = 0,
			count = 0,
			opwlen = 0,
			insist = 0,
			tmpflag, flags, c;
	char		*pswd,
			*p, *o;
	cap_t		ocap;
	cap_value_t	cap_audit_write = CAP_AUDIT_WRITE;

	opass[8] = '\0';
	opwlen = (int) strlen(opass);

	while (!strlen(buf)) {
		if (insist >= 3) {       /* 3 chances to meet triviality */
			ocap = cap_acquire(1, &cap_audit_write);
			ia_audit("PASSWD", uname, 0,
			    "Password(s) didn't meet the minimum requirement");
			cap_surrender(ocap);
			(void)pfmt(stderr, MM_ERROR, ":368:Too many failures\n");
			(void)pfmt(stderr, MM_ACTION, again);
			return NOPERM;
		}
		if ((pswd = getpass(gettxt(":369", "New password:"))) == NULL) {
			(void) pfmt(stderr, MM_ERROR, sorry);
			return FMERR;
		}
		else {
			(void) strncpy(pwbuf, pswd, 8);
			pwbuf[8] = '\0';
			pwlen = strlen(pwbuf);
		}
		/*
		 * Make sure new password is long enough 
		*/
		if (!adm && (pwlen < minlength)) { 
			(void) pfmt(stderr, MM_ERROR, tshort, minlength);
			++insist;
			continue;
		}
		/*
		 * Check the circular shift of the logonid
		*/
		if(!adm && circ(uname, pwbuf)) {
			(void) pfmt(stderr, MM_ERROR, nocirc);
			++insist;
			continue;
		}
		/*
		 * Insure passwords contain at least two alpha characters
		 * and one numeric or special character
		*/
		tmpflag = flags = 0;
		p = pwbuf;
		if (!adm) {
			while (c = *p++) {
				if (isalpha(c) && tmpflag )
					 flags |= 1;
				else if (isalpha(c) && !tmpflag ) {
					flags |= 2;
					tmpflag = 1;
				} else if (isdigit(c) ) 
					flags |= 4;
				else 
					flags |= 8;
			}

			/*		7 = lca,lca,num
			 *		7 = lca,uca,num
			 *		7 = uca,uca,num
			 *		11 = lca,lca,spec
			 *		11 = lca,uca,spec
			 *		11 = uca,uca,spec
			 *		15 = spec,num,alpha,alpha
			*/

			if ( flags != 7 && flags != 11 && flags != 15  ) {
				(void) pfmt(stderr, MM_ERROR, s_char);
				++insist;
				continue;
			}
		}
		/*
		 * Old password and New password must differ by at least
		 * three characters.
		*/
		if (!adm) {
			p = pwbuf;
			o = opass;
			if (pwlen >= opwlen) {
				i = pwlen;
				k = pwlen - opwlen;
			} else {
				i = opwlen;
				k = opwlen - pwlen;
			}
			for (j = 1; j <= i; j++) 
				if (*p++ != *o++) 
					k++;
			if (k < 3) {
				(void) pfmt(stderr, MM_ERROR, df3pos);
				++insist;
				continue;
			}
		}
		/*
		 * Ensure password was typed correctly, user gets 3 chances
		*/
		if ((pswd = getpass(gettxt(":822", "Re-enter new password:"))) == NULL) {
			(void) pfmt(stderr, MM_ERROR, sorry);
			return FMERR;
		}
		else  {
			(void) strncpy(buf, pswd, 8);
			buf[8] = '\0';
		}
		if (strcmp(buf, pwbuf)) {
			buf[0] ='\0';
			if (++count >= 3) { 
				(void) pfmt(stderr, MM_ERROR, tomany);
				(void) pfmt(stderr, MM_ACTION, again);
				return NOPERM;
			} else {
				(void) pfmt(stderr, MM_ERROR, nmatch);
				(void) pfmt(stderr, MM_ACTION, tagain);
			}
			continue;
		}
		break;			/* terminate "while" loop */
	}	/* end of "insist" while */
	return 0;	/* success */
}


/*
 * Procedure:	call_pwgen
 *
 * Restrictions:
 *		fclose:		none
 *		fflush:		none
 *		getpass:	none
 *
 * Notes:	does the actual work of reading from the pipe, setting
 *		up the choices array, and prompting the user for input.
 *		Also sets the contents of buf and pwbuf for use later.
*/
static	void
call_pwgen(iop, buf, pwbuf)
	FILE	*iop;
	char	*buf,
		*pwbuf;
{
	char	*pswd,
		line[BUFSIZ],
		*linep = &line[0],
		*choices[MAXSTRS];

	int	status = 0;

	register int	i, len,
			lcnt = 0,
			mlen = 0,
			firstime = 1,
			todo = MM_INFO;

	while ((linep = fgets(linep,sizeof(line),iop)) != NULL) {
		if (lcnt > MAXSTRS) {
			(void) pfmt(stderr, MM_ERROR, MSG_UF);
			(void) fclose(iop);
			buf = '\0';
			return;
		}
		/* protect against generators giving us blank lines */
		if ( (len = strlen(linep) - 1) <= 0)
		    continue;
		++lcnt;
		choices[(lcnt - 1)] = (char *)malloc(len + 1);
		(void) strncpy(choices[(lcnt - 1)], linep, len);
		choices[(lcnt - 1)][len] = '\0';
	}
	(void) fflush(iop);
	(void) fclose(iop);
	(void) wait(&status);
	/*
	 * if the password generator exited with 0 (success), show
	 * the user the list of available passwords to select.
	*/
	if (((status >> 8) & 0377) == 0) {
		while (lcnt) {
			if (!firstime)
				todo = MM_ACTION;
			firstime = 0;
			pfmt(stderr, todo, pwgenmsg);
			for (i = 0; i < lcnt; i++) {
				(void) printf("\t%s\n", choices[i]);
			}
			(void) printf("\n");
			pswd = getpass(gettxt(":369", "New password:"));
			if (!strlen(pswd)) {
				(void) pfmt(stderr, MM_ERROR, pwgenerr);
				continue;
			}
			for (i = 0; i < lcnt; i++) {
				if ((size_t)strlen(pswd) < (size_t)strlen(choices[i])) {
					mlen = 8;
				}
				else {
					mlen = strlen(choices[i]);
				}
				if (!strncmp(pswd, choices[i], mlen)) {
					lcnt = 0;
					(void) strcpy(pwbuf, pswd);
					(void) strcpy(buf, pswd);
					break;
				}
			}
			if (lcnt) {
				(void) pfmt(stderr, MM_ERROR, pwgenerr);
			}
		}
	}
	else
		buf = '\0';
}


/*
 * Procedure:	init_defaults
 *
 * Restrictions:
 *		defopen:	none
 *
 * Notes:	opens the default file with the tunables for passwd
 *		if it can and sets the variables accordingly.  If it
 *		can't open the file (for any reason) it sets the
 *		variables to "hard" default values defined in the
 *		source code.
*/
static	void
init_defaults()
{
	int	temp;
	char	*char_p;
	FILE	*defltfp;

        /*
	 * set up default valuses if password administration file
	 * can't be opened, or not all keywords are defined.
        */
	login_only = 0;
	mindate = MINWEEKS;
	maxdate = MAXWEEKS;
	warndate = WARNWEEKS;
	minlength = MINLENGTH;
	historycnt = -1;
	historydays = -1;

	if ((defltfp = defopen(PWADMIN)) != NULL) { /* M005  start */
		if ((char_p = defread(defltfp, "LOGIN_ONLY")) != NULL) {
			if (*char_p) {
				/*
	 			 * if the keyword ``LOGIN_ONLY'' was defined
				 * and had a value, determine if that value
				 * was the string ``Yes''.  If so, then the
				 * user can only change the password at login
				 * time. Otherwise, passwd behaves as always.
				*/
				if (!strcmp(char_p, "Yes")) {
					login_only = 1;
				}
			}
		}
		if ((char_p = defread(defltfp, "PASSLENGTH")) != NULL) {
			if (*char_p) {
				temp = atoi(char_p);
				if (temp > 1 && temp < 9) {
					minlength = temp;
				}
			}
		}
		if ((char_p = defread(defltfp, "MINWEEKS")) != NULL) {
			if (*char_p) {
				temp = atoi(char_p);
				if (temp >= 0) {
					mindate = temp * 7;
				}
			}
		}
		if ((char_p = defread(defltfp, "WARNWEEKS")) != NULL) {
			if (*char_p) {
				temp = atoi(char_p);
				if (temp >= 0) {
					warndate = temp * 7;
				}
			}
		}
		if ((char_p = defread(defltfp, "MAXWEEKS")) != NULL) {
			if (*char_p) {
				if ((temp = atoi(char_p)) == FAIL) {
					mindate = -1;
					warndate = -1;
				}
				else if (temp < -1) {
					maxdate = MAXWEEKS;
				}
				else {
					maxdate = temp * 7;
				}
			}
		}
		if ((char_p = defread(defltfp, "HISTORYCNT")) != NULL) {
			if (*char_p) {
				if ((temp = atoi(char_p)) == FAIL) {
					historycnt = -1;
				}
				else if (temp < -1) {
					historycnt = -1;
					/* defaults to not being used */
				}
				else {
					historycnt = temp;
					if (historycnt > 25)
					    historycnt = 25;
					/* saints protect us from the admin */
				}

			}
		}
		if ((char_p = defread(defltfp, "HISTORYDAYS")) != NULL) {
			if (*char_p) {
				if ((temp = atoi(char_p)) == FAIL) {
					historydays = -1;
				}
				else if (temp < -1) {
					historydays = -1;
					/* defaults to not being used */
				}
				else {
					historydays = temp;
					/* two years is the max we allow */
					if (historydays > 730)
					    historydays = 730;
				}

			}
			/* if no historycnt given, default it to 25 */
			if (historycnt == -1)
			    historycnt = 25;
		}
		defclose(defltfp);                  /* close defaults file */
	}

#ifdef DEBUG
	(void) printf("init_defaults: maxdate == %d, mindate == %d\n", maxdate, mindate);
#endif
	return;
}

static	int 
update_noshadow(flag, uname)
	char	*uname;
	int	flag;
{
	register int i, found = 0;
	struct	stat	buf;
	struct	passwd	*pwd;
	int	fd,
	 	pw_error = 0,
	 	end_of_file = 0;
	FILE	*tpfp;
	char	username[32];
	int	when;
	time_t	now, maxweeks, minweeks;

	/* ignore all the signals */
	for (i = 1; i < NSIG; i++)
		(void) sigset(i, SIG_IGN);

 	/* Clear the errno. It is set because SIGKILL can not be ignored. */
	errno = 0;

	/* Create a temporary file */
	(void) unlink(PASSTEMP);
	if ((fd = creat(PASSTEMP, 0600)) == FAIL) {
		(void) pfmt(stderr, MM_ERROR, MSG_FE);
		return FMERR;
	}
	(void) close(fd);
	if ((tpfp = fopen(PASSTEMP, "w")) == NULL) {
		(void) pfmt(stderr, MM_ERROR, MSG_FE);
		return FMERR;
	}

	/* copy passwd file to temp, replacing passwd in matching line */
	end_of_file = pw_error = 0;
	strcpy(username,uname);
	setpwent();
	while (!end_of_file) {
	    if ((pwd = getpwent()) != NULL) {
		if (strcmp(pwd->pw_name, username) == 0) { 
		    found = 1;
		    /* LFLAG and DFLAG should be checked before FFLAG.
		     * FFLAG clears the lastchg field. We do not
		     * want lastchg field to be set if one execute
		     * passwd -d -f name or passwd -l -f name.
		     */
		    if (flag & LFLAG) {	 /* lock password */
			pwd->pw_passwd = pw;
			if (*pwd->pw_age != NULL) {
			    CNV_AGE();
			    when = maxweeks + (minweeks << 6) + (now << 12);
			    pwd->pw_age = l64a(when);
			}
		    } 
		    if (flag & DFLAG) {	/* delete password */
			pwd->pw_passwd = nullstr;
			if (*pwd->pw_age != NULL) {
			    CNV_AGE();
			    when = maxweeks + (minweeks << 6) + (now << 12);
			    pwd->pw_age = l64a(when);
			}
		    }
		    if (flag & FFLAG) {	 /* expire password */
			if (*pwd->pw_age != NULL)  {
			    CNV_AGE();
			    when = maxweeks + (minweeks << 6);
			    if ( when == 0)
				pwd->pw_age = ".";
			    else
				pwd->pw_age = l64a(when);
			} else
			    pwd->pw_age = ".";
		    }
		    /* MFLAG should be checked before NFLAG.  One 
		     * can not set min field without setting max field.
		     * If aging is off, force to expire a user password --
		     * set lastchg field to 0.
		     */
		    if (flag & MFLAG)  { 	/* set max field */
			if ( maxdate == -1) {
			    pwd->pw_age = nullstr; 
			} else  {
			    if (*pwd->pw_age == NULL) {  
				minweeks  = 0;
				when = 0;
			    } else
				CNV_AGE();
			    maxweeks = (maxdate + (DAY_WEEK -1)) / DAY_WEEK;
			    maxweeks = maxweeks > DIG_CH ? DIG_CH : maxweeks;
			    when = maxweeks + (minweeks << 6) + (when & ~0xfff);
			    if (when == 0)
				pwd->pw_age = ".";
			    else
				pwd->pw_age = l64a(when);
			}
		    }
		    if (flag & NFLAG) {   /* set min field */
			if (*pwd->pw_age == NULL)  {
			    (void) fprintf(stderr,"%s\n %s", MSG_BS, sausage); 
			    (void) unlink(PASSTEMP);
			    return (BADSYN);
			}
			CNV_AGE();
			minweeks = (mindate + (DAY_WEEK  - 1)) / DAY_WEEK;
			minweeks = minweeks > DIG_CH ? DIG_CH : minweeks;
			when = maxweeks + (minweeks << 6) + (when & ~0xfff);
			if (when == 0)
			    pwd->pw_age = ".";
			else
			    pwd->pw_age = l64a(when);
		    }
		    if (flag & PFLAG)  {	/* change password */
			pwd->pw_passwd = pw;
			if (*pwd->pw_age != NULL) {
			    CNV_AGE();
			    if (maxweeks  == 0) {   /* turn off aging */
				pwd->pw_age = nullstr;
			    } else  {
				when = maxweeks + (minweeks << 6) + (now << 12);
				pwd->pw_age = l64a(when);
			    }
			}
			
		    }
		}
		if (putpwent (pwd, tpfp) != 0) { 
		    endpwent();
		    (void) unlink(PASSTEMP);
		    (void) pfmt(stderr, MM_ERROR, MSG_FE);
		    return FMERR;
		}
	    } else {
		if (errno == 0)
		    /* end of file */
		    end_of_file = 1;	
		else if (errno == EINVAL) {
		    /*bad entry found in /etc/passwd, skip it */
		    errno = 0;
		    pw_error++;
		}  else
		    /* unexpected error found */
		    end_of_file = 1;
	    }
	} /*end of while*/

	endpwent();

	if (pw_error >= 1)
		pfmt(stderr, MM_ERROR, badentry);

	if (fclose (tpfp)) {
		(void) unlink(PASSTEMP);
		(void) pfmt(stderr, MM_ERROR, MSG_FE);
		return FMERR;
	}

	/* Check if user name exists */
	if (!found) {
		(void) unlink(PASSTEMP);
		(void) pfmt(stderr, MM_ERROR, MSG_FE);
		return FMERR;
	}

	/* change mode, owner & group of temp file to mode of passwd file */
	if (stat(PASSWD, &buf) < 0) {
		(void) unlink(PASSTEMP);
		(void) pfmt(stderr, MM_ERROR, MSG_FE);
		return FMERR;
	}
	if (chmod(PASSTEMP, buf.st_mode) != SUCCESS) {
		(void) unlink(PASSTEMP);
		(void) pfmt(stderr, MM_ERROR, MSG_FE);
		return FMERR;
	}
	if (chown(PASSTEMP, buf.st_uid, buf.st_gid) != SUCCESS) {
 		(void) unlink(PASSTEMP);
		(void) pfmt(stderr, MM_ERROR, MSG_FE);
		return FMERR;
	}

	/* Rename temp file back to appropriate passwd file and return  */
	if (access(PASSWD, 0) == 0) {
		if (access(OPASSWD, 0) == 0) {
			if (unlink(OPASSWD) < 0) {
				(void) unlink(PASSTEMP);
				(void) pfmt(stderr, MM_ERROR, MSG_FE);
				return FMERR;
			}
		}
		if (link(PASSWD, OPASSWD) < 0 ) {
			(void) unlink(PASSTEMP);
			(void) pfmt(stderr, MM_ERROR, MSG_FE);
			return FMERR;
		}
	}
	if (rename(PASSTEMP, PASSWD) < 0 ) {
		(void) unlink(PASSWD);
		if (link(OPASSWD, PASSWD) < 0 ) { 
			(void) pfmt(stderr, MM_ERROR, MSG_FF);
			return FATAL;
		}
		(void) pfmt(stderr, MM_ERROR, MSG_FE);
		return FMERR;
	}
	/* let NSD know about the change */
	rmdir("/ns/.local/passwd.byname");
	rmdir("/ns/.local/passwd.byuid");
	return SUCCESS;
}

/*
 * Procedure:	get_pwvalidate
 *
 * Restrictions:
 *		defopen:	none
 *
 * Notes:	does validation for determining what the password
 *		validator is, reads the default passwd file, and
 *		"exec" the password validation pgm if it exists.
 *		This routine calls "open_pwvalidate" that creates a pipe and
 *		does a "fork()" and "exec()".
*/
static	FILE	*
get_pwvalidate()
{
	char	prog[BUFSIZ],
		passgen[BUFSIZ],
		*progp = &prog[0],
		*pstrp = &passgen[0];
	FILE	*iop;

	/* borrowed this from the generator invocation */

	progp = '\0';
	(void) strcpy(pstrp, "PASSWDVALIDATE");

	if ((iop = defopen(PWADMIN)) == NULL) {
		/*
		 * couldn't open "passwd" default file, no validation pgm!
		*/
		return (FILE *)NULL;
	}
	if ((progp = defread(iop, pstrp)) == NULL) {
		/*
		 * didn't find the password validator keyword in the
		 * "passwd" default file, no validator!
		*/
		defclose(iop);
		return (FILE *)NULL;
	}
	else if (*progp) {
		progp = strdup(progp);
		defclose(iop);
		/*
		 * found a value for a possible password validator, now
		 * check to see if it's a full pathname.  If not, error!
		*/
		if (strncmp(progp, (char *)"/", 1)) {
			return (FILE *)NULL;
		}
		/*
		 * otherwise, try to "exec" whatever was found in the
		 * appropriate password validator field.
		*/
		if ((iop = open_pwval(progp)) == NULL) {
			return (FILE *)NULL;
		}
		return iop;
	}
	defclose(iop);
	return (FILE *)NULL;
}


/*
 * Procedure:	call_pwvalidate
 *
 * Notes:	writes proposed passwd down the pipe to the external
 *		validation pgm, then waits for exit and returns status.
 */
static	int
call_pwvalidate(iop, pwbuf)
	FILE	*iop;
	char	*pwbuf;
{
	int status;
	(void) fprintf(iop,"%s\n",pwbuf);
	(void) fflush(iop);
	(void) fclose(iop);
	(void) wait(&status);
	return(status);
}

/*
 * Procedure:	open_pwval
 *
 * Notes:	sets up a pipe, forks and executes the password
 *		validator program.  If the exec fails it returns
 *		a value to indicate that it did fail.  On success,
 *		it returns a file pointer to the pipe.
 *		This was derived from open_pwgen, but needs to write vs
 *              read the pipe.
*/
static	FILE *
open_pwval(progp)
	char *progp;
{
	register int	i;
	int		pfd[2];
	FILE		*iop;

	(void) pipe(pfd);
	if ((i = fork()) < 0) {
		(void) close(pfd[0]);
		(void) close(pfd[1]);
		return (FILE *)NULL;
	}
	else if (i == 0) {
		(void) close(pfd[1]);
		(void) close(0);
		(void) dup(pfd[0]);
		(void) close(pfd[0]);
		for (i = 5; i < 15; i++)
			(void) close(i);

		(void) execl(progp, progp, 0);
		(void) close(0);
		exit(1);		/* tell parent that 'execl' failed */
	}
	else {
		(void) close(pfd[0]);
		iop = fdopen(pfd[1], "w");
		return iop;
	}
	/* NOTREACHED */
}


/*
 * Procedure:	ck_pwhistory(user, clear_passwd)
 * Returns:	SUCCESS if the password is OK (not used previously)
 *              error code if it has been used in the last HISTORYCNT passwd changes
 *
 * Notes:	if the site has configured HISTORYCNT to a non zero value
 *		we check to see that this password doesn't match the last
 *		HISTORYCNT values
 * 		The format of the file is:
 *		username:encrypted1:day_it_chgd:encrypted2:day_it_chgd:...
 *		where the newest encrypted passwd used is encrypted1 
 *		day_it_chgd is the DAY_NOW value when the encryptedX
 *		was changed.  This is used in HISTORYDAYS processing.
 */
static int
ck_pwhistory(user, pw)
char * user;
char * pw;	/* clear passwd */
{
	
	FILE *hfp;
	char line[2048];
	char *linep,*pf,*pf2;
	int match;
	char wrkpwbuf[10];
	int now = DAY_NOW;
	int chgd;

	/* don't care about reuse, default */
	if (historycnt == -1 && historydays == -1 )
		return SUCCESS;
	
	if ( (hfp = fopen(PWHISTORY,"r")) ==NULL)
	{
	    /* no history file, so de facto OK */
	    return SUCCESS;
	}

	linep = &line[0];
	while ((linep = fgets(linep,sizeof(line),hfp)) != NULL)
	{
	    /* skip comment lines */
	    if (*linep == '#') continue;

	    pf = strtok(linep,":\n");
	    /* is this the name we're looking for? */
	    if ( strcmp(pf,user) != 0) continue;

	    /*now see if the password matches a previous value */
	    while ( (pf=strtok(NULL,":\n")) != NULL)
	    {

		if (strcmp(crypt(pw,pf),pf) == 0)
		{
		    fclose(hfp);
		    fprintf(stderr,"Sorry, you have reused an old password\n");
		    return 1;
		}
		/* date it changed */
		if ( (pf2 = strtok(NULL,":\n")) == NULL)
		    break;
		/* check for date of change, stop if we encounter one older
		 * than historydays ago
		 */
		if ( historydays != -1)
		{
		    /* the checks are for potential corruption of history file */
		    if ( ((chgd = atoi(pf2)) <= 0) || (chgd > 25000) )
			break;
		    if ( (now - chgd) > historydays)
			break;
		}
	    }
	    fclose(hfp);
	    return SUCCESS;	/* didn't match any prior pw */
	}
	fclose(hfp);
	return SUCCESS;	/* never found user in the file */
}


/* update the history file*/
static int
update_pwhistory(user, pw)
char * user;
char * pw;	/* encrypted passwd */
{
	
	FILE *hfp, *tfp;
	int fd, i, glbl_match;
	char line[2048];
	char *linep;
	char namebuf[16];
	int match;
	struct stat buf;
	char *pf,*pn,*pf2;
	int now;
	int chgd;

	/* don't care about reuse, default */
	if (historycnt == -1 && historydays == -1 )
		return SUCCESS;
	
	if ( (hfp = fopen(PWHISTORY,"r")) ==NULL)
	{
	    hfp = fopen("/dev/null","r");   /* no history file, fake it */
	}

	/* Create a temporary file */
	(void) unlink(PWHISTTEMP);
	if ((fd = creat(PWHISTTEMP, 0600)) == FAIL) {
		(void) pfmt(stderr, MM_ERROR, MSG_FE);
		return FMERR;
	}
	(void) close(fd);
	if ((tfp = fopen(PWHISTTEMP, "w")) == NULL) {
		(void) pfmt(stderr, MM_ERROR, MSG_FE);
		return FMERR;
	}

	now = DAY_NOW;
	linep = &line[0];
	glbl_match = 0;
	while ((linep = fgets(linep,sizeof(line),hfp)) != NULL)
	{
	    /* replicate comment lines */
	    if (*linep == '#') 
	    {
		fputs(linep,tfp);
		continue;
	    }

	    /* if the name doesn't match, just echo the line */
	    /* don't want strtok to munch on this line just yet */
	    for (pf = linep, pn=&namebuf[0]; 
	      ((*pf != ':') && (*pf != '\0') && (*pf != '\n')); pf++,pn++)
	    {
		*pn = *pf;
	    }
	    *pn = '\0';
		
	    if (strcmp((char *)&namebuf[0],user) != 0)
	    {
		fputs(linep,tfp);
		continue;
	    }

	    /* now it's OK to munch the line, we're tossing the first field */

	    pf = strtok(linep,":\n");

	    glbl_match = 1;  /* we found the user */
	    /* matched the user name, update the history */
	    fprintf(tfp,"%s:%s:%d:",user,pw,now);
	    /* put out the other saved passwds, up to the max configured */
	    for (i = 2; i <= historycnt; i++)
	    {
		if ( (pf = strtok(NULL,":\n")) == NULL)
		    break;
		/* date it changed */
		if ( (pf2 = strtok(NULL,":\n")) == NULL)
		    break;
		/* check for date of change, stop if we encounter one older
		 * than historydays ago
		 */
		if ( historydays != -1)
		{
		    /* the checks are for potential corruption of history file */
		    if ( ((chgd = atoi(pf2)) <= 0) || (chgd > 25000) )
			break;
		    if ( (now - chgd) > historydays)
			break;
		}

		fprintf(tfp,"%s:%s:",pf,pf2);
	    }
	    fprintf(tfp,"\n");
	}
	/* never found the user in the history file, so write a new entry */
	if (glbl_match == 0)
	{
	    fprintf(tfp,"%s:%s:%d:\n",user,pw,now);
	}
	fclose(hfp);
	fclose(tfp);

	/* change mode, owner & group of temp file to proper values */
	if (stat(PASSWD, &buf) < 0) {
		(void) unlink(PWHISTTEMP);
		(void) pfmt(stderr, MM_ERROR, MSG_FE);
		return FMERR;
	}
	if (chmod(PWHISTTEMP, 0400) != SUCCESS) {
		(void) unlink(PWHISTTEMP);
		(void) pfmt(stderr, MM_ERROR, MSG_FE);
		return FMERR;
	}
	if (chown(PWHISTTEMP, buf.st_uid, buf.st_gid) != SUCCESS) {
 		(void) unlink(PWHISTTEMP);
		(void) pfmt(stderr, MM_ERROR, MSG_FE);
		return FMERR;
	}

	/* Rename temp file back to appropriate passwd file and return  */
	if (rename(PWHISTTEMP, PWHISTORY) < 0 ) {
		(void) pfmt(stderr, MM_ERROR, MSG_FE);
		return FMERR;
	}
	return SUCCESS;
}

#ifdef DEBUG
lckpwdf()
{
	return(0);
}

ulkpwdf()
{
	return(0);
}
#endif
