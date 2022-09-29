/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)pwconv:pwconv.c	1.11.7.1"
/***************************************************************************
 * Command: pwconv
 *
 * Inheritable Privileges: None		(Maintenance Mode Only)
 *       Fixed Privileges: None
 *
 ***************************************************************************/

/*  pwconv.c  */
/*  Conversion aid to copy appropriate fields from the	*/
/*  password file to the shadow file.			*/

#include	<pwd.h>
#include	<fcntl.h>
#include	<stdio.h>
#include	<sys/types.h>
#include	<sys/stat.h>
#include	<time.h>	
#include	<shadow.h>
#include	<grp.h>
#include	<signal.h>
#include	<errno.h>
#include	<unistd.h>
#include	<stdlib.h>
#include	<locale.h>
#include	<pfmt.h>
#include	"htree.h"

/* exit  code */

#define SUCCESS	0	/* succeeded */
#define NOPERM	1	/* No permission */
#define BADSYN	2	/* Incorrect syntax */
#define FMERR	3	/* File manipulation error */
#define FATAL	4	/* Old file can not be recover */
#define FBUSY	5	/* Lock file busy */
#define BADSHW	6	/* Bad entry in shadow file  */

#define	DELPTMP()	(void) unlink(PASSTEMP)
#define	DELSHWTMP()	(void) unlink(SHADTEMP)

#define NOT_SU	":0:Not super user.\n"
#define	NOPWD	":0:user %s has no password\n"
#define	BADENT	":0:bad entry in /etc/passwd\n" 
#define	USAGE	":0:Invalid command syntax.\nUsage: pwconv\n"
#define	BUSY	":0:Password file(s) busy.  Try again later.\n"
#define	UNEXP	":0:Unexpected failure. Conversion not done.\n"
#define	EMPTY	":0:Unexpected failure. ``%s'' is an empty file.\n"
#define	MISS	":0:Unexpected failure. Password file(s) missing.\n"
#define	NOTDONE	":0:Bad entry in /etc/shadow. Conversion not done.\n"

extern	int	strcmp(),
		lvlfile();

/* SGI IRIX 6.2 getpwent merges in SSDI, which allows custom implementations
 * of passwd database (and others as well). With SSDI, getpwent will
 * return entries from these custom implemetations, whereas passmt
 * only needs stuff from the "files". Setting _getpwent_no_ssdi to 1
 * turns off this behavior which is what passconv needs.
 */
extern int _getpwent_no_ssdi;

extern	int	_getpwent_no_yp;	/* avoid YP '+' interpretation */
extern	int	_getpwent_no_shadow;	/* don't cp passwds from /etc/shadow */
extern	int	_getXbyY_no_stat;

static	char	pwdflr[] = "x";			/* password filler */

static	void	f_err(),
		f_miss(),
		f_bdshw(),
		f_empty(),
		no_recover(),
		no_convert();

/*
 * Procedure:     main
 *
 * Restrictions:	pfmt: none
 *			lvlfile(2): none
 *			lckpwdf: none
 *			chown(2): none
 *			fopen: none
 *			access(2): none
 *			getpwent: none
 *			getspnam: none
 *			putpwent: none
 *			putspent: none 
 *			fclose: none 
 *			rename(2): none
 *			link(2): none
 *			unlink(2): none
 *			chmod(2): none
 *			printf: none
 *			ulckpwdf: none
*/
main(argc)
	int argc;
{
	extern	int	errno;
	struct  passwd  *pwdp;
	struct	spwd	*sp, *sp2, sp_pwd;	/* default entry */
	struct stat buf;
	FILE	*tp_fp, *tsp_fp;
	register time_t	when, minweeks, maxweeks;
	int file_exist = 1;
	int end_of_file = 0;
	mode_t mode;
	int pwerr = 0;
	ushort i;
	gid_t pwd_gid, sp_gid;
	uid_t pwd_uid, sp_uid;
	long day_now;
	ht_tree_t htree;
	ht_node_t *htn;
#if 0
	level_t p_level = 1,	/* default level for /etc/passwd */
	 	s_level = 2;	/* default level for /etc/shadow */
#endif

	(void) setlocale(LC_ALL, "");
	setcat("uxcore.abi");
	setlabel("UX:pwconv");

	/* SGI: turn off SSDI mapping for getpwent ... pre 6.5 */
	/* _getpwent_no_ssdi = 1; */

	_getpwent_no_yp = 1;		/* avoid YP '+' interpretation */
	_getpwent_no_shadow = 1;	/* don't cp passwds from /etc/shadow */
	_getXbyY_no_stat = 1;		/* don't bother checking for changes */
					/* to /etc/passwd during the run */

	/* No argument can be passed to the command*/
	if (argc > 1) {
		pfmt(stderr, MM_ERROR, USAGE);
		exit(BADSYN);
	}

	/* Follow convention used by passmgmt, quotacheck, efs/growfs, ... */
	if (getuid() != 0) {
	     pfmt(stderr, MM_ERROR, NOT_SU);
	     exit(NOPERM);
	}

	/* lock file so that only one process can read or write at a time */
	if (lckpwdf() < 0) { 
		pfmt(stderr, MM_ERROR, BUSY);
		exit(FBUSY);
	}
 

      	/* All signals will be ignored during the execution of pwconv */
	for (i=1; i < NSIG; i++) 
		(void) sigset(i, SIG_IGN);
 
	/* reset errno to avoid side effects of a failed */
	/* sigset (e.g., SIGKILL) */
	errno = 0;

	/* check the file status of the password file */
	/* get the gid of the password file */
	if (stat(PASSWD, &buf) < 0) {
		f_err();
		exit(FMERR);
	} 
	/*
	 * if the size of the PASSWD file was 0, exit with an error!
	*/
	if (!buf.st_size) {
		f_empty(PASSWD);
		exit(FMERR);
	}
	pwd_gid = buf.st_gid;
	pwd_uid = buf.st_uid;
 
	/* mode for the password file should be read-only or less */
	(void) umask(S_IAMB & ~(buf.st_mode & (S_IRUSR|S_IRGRP|S_IROTH)));

	/* open temporary password file */
	if ((tp_fp = fopen(PASSTEMP,"w")) == NULL) {
		f_err();
		exit(FMERR);
	}

        if (chown(PASSTEMP, pwd_uid, pwd_gid) < 0) {
		DELPTMP();
		f_err();
		exit(FMERR);
	}
	/* default mode mask of the shadow file */
	mode = S_IAMB & ~(S_IRUSR);

	/* check the existence of  shadow file */
	/* if the shadow file exists, get mode mask and group id of the file */
	/* if file does not exist, the default group name will be the group  */
	/* name of the password file.  */

	if( access(SHADOW,0) == 0) {
		if (stat(SHADOW, &buf) == 0) {
			mode  = S_IAMB & ~(buf.st_mode & S_IRUSR);
			sp_gid = buf.st_gid;
			sp_uid = buf.st_uid;
		} else {
			DELPTMP();
			f_err();
			exit(FMERR);
		}
		
		errno = 0;
		setspent();
		ht_init(&htree);
		while ((sp = getspent()) != NULL) {

			if (((sp2 = (struct spwd *)
			      calloc(1,sizeof(struct spwd))) == NULL) || 
			    ((htn = calloc(1,sizeof(ht_node_t))) == NULL)) {
				DELPTMP();
				f_err();
				exit(FMERR);
			}

			sp2->sp_namp = strdup(sp->sp_namp);
			sp2->sp_pwdp = strdup(sp->sp_pwdp);
			sp2->sp_lstchg = sp->sp_lstchg;
			sp2->sp_min = sp->sp_min;
			sp2->sp_max = sp->sp_max;
			sp2->sp_warn = sp->sp_warn;
			sp2->sp_inact = sp->sp_inact;
			sp2->sp_expire = sp->sp_expire;
			sp2->sp_flag = sp->sp_flag;
			htn->key = sp2->sp_namp;
			htn->len = strlen(htn->key);
			htn->val = sp2;
			if (!ht_insert(&htree, htn)) {
				DELPTMP();
				f_err();
				exit(FMERR);
			}				
		}
		if (errno == EINVAL) { /* Bad entry in shadow exit */
			DELSHWTMP();
			DELPTMP();
			f_bdshw();
			exit(BADSHW);
		}
		endspent();
	} else  { 
		sp_gid = pwd_gid;
		sp_uid = pwd_uid;
		file_exist = 0;
	}


	/* get the mode of shadow password file  -- mode of the file should
	   be read-only for user or less. */
	(void) umask(mode);

	/* open temporary shadow file */
	if ((tsp_fp = fopen(SHADTEMP, "w")) == NULL) {
		DELPTMP();
		f_err();
		exit(FMERR);
	}

	/* change the group of the temporary shadow password file */
        if (chown(SHADTEMP, sp_uid, sp_gid) < 0) {
		no_convert();
		exit(FMERR);
	}
	/* Reads the password file.  				*/
	/* If the shadow password file not exists, or		*/
	/* if an entry doesn't have a corresponding entry in    */
	/* the shadow file, entries/entry will be created.      */
 
	/* Only bother to compute the modification time for entries once */
	day_now = DAY_NOW;
    while (!end_of_file) {
	if ((pwdp = getpwent()) != NULL) {
		errno = 0;

		/* don't put YP entries in /etc/shadow */
		if (pwdp->pw_name[0] == '+' || pwdp->pw_name[0] == '-') {
			/* write an entry to temporary password file */
			if ((putpwent(pwdp,tp_fp)) != 0 ) {
				no_convert();
				exit(FMERR);
			}
			continue;
		}

		if (!file_exist || ((htn = ht_lookup(&htree, pwdp->pw_name, 
						    strlen(pwdp->pw_name)))
				    == NULL )) {
			sp = &sp_pwd;
			sp->sp_namp = pwdp->pw_name;
 			if (*pwdp->pw_passwd == NULL) 
				pfmt(stderr, MM_WARNING, NOPWD, sp->sp_namp); 

			/* Copy the password field in the password file to */
			/* shadow password file.  Replace the password */
			/* field with an 'x'.			        */

			sp->sp_pwdp=pwdp->pw_passwd;
			pwdp->pw_passwd = pwdflr;

			/* If login has aging info, split the aging info */
			/* into age, max and min.                        */
			/* Convert aging info from weeks to days */

		   	if (*pwdp->pw_age != NULL) {
				when = (long) a64l (pwdp->pw_age);
				maxweeks = when & 077;
				minweeks = (when >> 6) & 077;
				when >>= 12;
				sp->sp_lstchg = when * 7;
				sp->sp_min = minweeks * 7;
				sp->sp_max = maxweeks * 7;
				sp->sp_warn = -1;
				sp->sp_inact = -1;
				sp->sp_expire = -1;
				sp->sp_flag = 0;
				pwdp->pw_age = "";
			} else {
 
			/* if no aging, last_changed will be the day the 
			conversion is done, min and max fields will be null */
			/* use timezone to get local time */
 
				sp->sp_lstchg = day_now;
				sp->sp_min =  -1;
				sp->sp_max =  -1;
				sp->sp_warn = -1;
				sp->sp_inact = -1;
				sp->sp_expire = -1;
				sp->sp_flag = 0;
		   	}
		} else {
			/* refrence the spwd out of the htree */
			sp = htn->val;
			
		/* If an entry in the password file has a string,    	 */
		/* other than 'x', in the password field, that password	 */
		/* entry will be entered in the corresponding entry	 */
		/* in the shadow file.  The password field will be replaced   */
		/* with an 'x'.                                          */
		/* If aging is not specified, last_changed day will be   */
		/* updated and max and min will be cleared.              */
 			if (*pwdp->pw_passwd == NULL) 
				pfmt(stderr, MM_WARNING, NOPWD, sp->sp_namp); 

			if (strcmp(pwdp->pw_passwd, pwdflr)) {
				sp->sp_pwdp=pwdp->pw_passwd;
				pwdp->pw_passwd = pwdflr;
		   		if (*pwdp->pw_age == NULL) {
					sp->sp_lstchg = day_now;
					sp->sp_min =  -1;
					sp->sp_max =  -1;
					sp->sp_warn = -1;
					sp->sp_inact = -1;
					sp->sp_expire = -1;
					sp->sp_flag = 0;
				}
			}
		/* If aging info is added, split aging info into age,    */
		/* max and min.  Convert aging info from weeks to days.  */
		   	if (*pwdp->pw_age != NULL) {
				when = (long) a64l (pwdp->pw_age);
				maxweeks = when & 077;
				minweeks = (when >> 6) & 077;
				when >>= 12;
				sp->sp_lstchg = when * 7;
				sp->sp_min = minweeks * 7;
				sp->sp_max = maxweeks * 7;
				sp->sp_warn = -1;
				sp->sp_inact = -1;
				sp->sp_expire = -1;
				sp->sp_flag = 0;
 			 	pwdp->pw_age = "";
		   	}
		}

		/* write an entry to temporary password file */
		if ((putpwent(pwdp,tp_fp)) != 0 ) {
			no_convert();
			exit(FMERR);
		}

		/* write an entry to temporary shadow password file */
		if (putspent(sp,tsp_fp) != 0) {
			no_convert();
			exit(FMERR);
		}
	} else {
		if (errno == 0)
			end_of_file = 1;
		else if (errno == EINVAL) {
			errno = 0;
			pwerr = 1;
		}
		else
			end_of_file = 1;
	}

	}
 	
	(void)fclose(tsp_fp);
	(void)fclose(tp_fp);

	if (pwerr)
		pfmt(stderr, MM_WARNING, BADENT);

	/* delete old password file if it exists*/
	if (unlink (OPASSWD) && (access (OPASSWD, 0) == 0)) {
		no_convert();
		exit(FMERR);
	}

	/* rename the password file to old password file  */
	if (rename(PASSWD, OPASSWD) == -1) {
		no_convert();
		exit(FMERR);
	}

	/* rename temporary password file to password file */
	if (rename(PASSTEMP, PASSWD) == -1) {
		/* link old password file to password file */
		if (link(OPASSWD, PASSWD) < 0) {
			no_recover();
			exit(FATAL);
 		}
		no_convert();
		exit(FMERR);
	}
 
	/* delete old shadow password file if it exists */
	if ( unlink (OSHADOW) && (access (OSHADOW, 0) == 0)) {
		/* link old password file to password file */
		if (unlink(PASSWD) || link (OPASSWD, PASSWD)) {
			no_recover();
			exit(FATAL);
 		}
		no_convert();
		exit(FMERR);
	}

	/* link shadow password file to old shadow password file */
	if (file_exist && rename(SHADOW, OSHADOW)) {
		/* link old password file to password file */
		if (unlink(PASSWD) || link (OPASSWD, PASSWD)) {
			no_recover();
			exit(FATAL);
 		}
		no_convert();
		exit(FMERR);
	}

	/* link temporary shadow password file to shadow password file */ 
	if (rename(SHADTEMP, SHADOW) == -1) {
		/* link old shadow password file to shadow password file */
		if (file_exist && (link(OSHADOW, SHADOW))) {
			no_recover();
			exit(FATAL);
		} 
		if (unlink(PASSWD) || link (OPASSWD, PASSWD)) {
			no_recover();
			exit(FATAL);
		}
		no_convert();
		exit(FMERR);
	}

	/* Change old password file to read only by owner   */
	/* If chmod fails, delete the old password file so that */
	/* the password fields can not be read by others */

	if (chmod(OPASSWD, S_IRUSR) < 0) {
		(void) unlink(OPASSWD);
	}
	/*
	 * set MAC level on /etc/passwd to ``SYS_PUBLIC''
	 * (void) lvlfile(PASSWD, MAC_SET, &p_level); 
	 * set MAC level on /etc/shadow to ``SYS_PRIVATE''
	 * (void) lvlfile(SHADOW, MAC_SET, &s_level); 
	 */
 
	(void) ulckpwdf();
	exit(0);
	/* NOTREACHED */
}
 
/*
 * Procedure:     no_recover
 *
 * Restrictions:
*/
static	void
no_recover()
{
	DELPTMP();
	DELSHWTMP();
	f_miss();
}

/*
 * Procedure:     no_convert
 *
 * Restrictions:
*/
static	void
no_convert()
{
	DELPTMP();
	DELSHWTMP();
	f_err();
}
 
/*
 * Procedure:     f_err
 *
 * Restrictions:
                 pfmt: none 
                 ulckpwdf: none
*/
static	void
f_err()
{
	pfmt(stderr, MM_ERROR, UNEXP);
	(void) ulckpwdf();
}

/*
 * Procedure:     f_miss
 *
 * Restrictions:
                 pfmt: none
                 ulckpwdf: none
*/
static	void
f_miss()
{
	pfmt(stderr, MM_ERROR, MISS);
	(void) ulckpwdf();
}


/*
 * Procedure:     f_empty
 *
 * Restrictions:
                 pfmt: none
                 ulckpwdf: none
*/
static	void
f_empty(fname)
	char	*fname;
{
	pfmt(stderr, MM_ERROR, EMPTY, fname);
	(void) ulckpwdf();
}

/*
 * Procedure:     f_bdshw
 *
 * Restrictions:
                 pfmt: none
                 ulckpwdf: none
*/
static	void
f_bdshw()
{
	pfmt(stderr, MM_ERROR, NOTDONE);
	(void) ulckpwdf();
}
