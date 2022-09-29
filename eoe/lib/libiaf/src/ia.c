/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"$Revision: 1.15 $"

#include "sys/types.h"
#include "sys/param.h"
#include "unistd.h"
#include "stdio.h"
#include "ia.h"
#include "pwd.h"
#include "grp.h"
#include "shadow.h"
#include "malloc.h"
#include "time.h"
#include "string.h"
#include "mls.h"

#define	WEEK	(24L * 7 * 60 * 60)	/* 1 week in seconds */
#ifndef TRUE
#define	TRUE	1
#define	FALSE	0
#endif	/* TRUE */

#define	PASSWDPGM	"/usr/bin/passwd"
#define	YPPASSWDPGM	"/usr/bin/yppasswd"

static	int	_ia_openinfo_fill(struct xpasswd *, uinfo_t *);

static int
sgroups(const char *uname, gid_t agroup, gid_t groups[])
{
	int ngroups = 0;
	register struct group *grp;
	register int i, ngroups_max;

	ngroups_max = sysconf(_SC_NGROUPS_MAX);	/* fetch actual system max */
	if (ngroups_max < 0) 			/* *SHOULDN'T* ever occur!! */
		return(0);

	if (agroup >= 0)
		groups[ngroups++] = agroup;
	setgrent();
	while (grp = getgrent()) {
		for (i = 0; i < ngroups; i++)
			if (grp->gr_gid == groups[i])
				break;
		if (i < ngroups)
			continue;
		for (i = 0; grp->gr_mem[i]; i++)
			if (!strcmp(grp->gr_mem[i], uname)) {
				if (ngroups == ngroups_max)
					goto toomany;
				groups[ngroups++] = grp->gr_gid;
			}
	}
toomany:
	endgrent();
	return(ngroups);
}

/*
 * ia_get_name - passed a UID.
 *		 Returns corresponding name or NULL.
 */

char *
ia_get_name(uid_t uid)
{
	struct passwd *pwd;
	extern int _getpwent_no_shadow;
	int oshadow;

	oshadow = _getpwent_no_shadow;
	_getpwent_no_shadow = 1; /* clearly don't need it */

	pwd = getpwuid(uid);

	_getpwent_no_shadow = oshadow;
	if (pwd)
		return (pwd->pw_name);
	else
		return(NULL);
}

/*
 * ia_openinfo - passed a pointer to logname and  a uinfo_t pointer.
 *	  	 A 0 is returned on success, and -1 if an error occurred.
 *		 If logname is not found uinfo is set to NULL and 0 is returned.
 */
int
ia_openinfo(char *u_name, uinfo_t *uinfo)
{
	struct	xpasswd	*pwd;
	extern int _getpwent_no_shadow;
	int oshadow;
	int rv = -1;

	if (uinfo == NULL)
		return(-1);
	*uinfo = NULL;

	oshadow = _getpwent_no_shadow;
	_getpwent_no_shadow = 1; /* clearly don't need it */

	if ((pwd = (struct xpasswd *) getpwnam(u_name)) &&
			(*uinfo = malloc(sizeof(**uinfo))))
		rv = _ia_openinfo_fill(pwd, uinfo);

	_getpwent_no_shadow = oshadow;

	return(rv);
}

/*
 * ia_openinfo_next - supply the identification and authentication info
 *			for the next entry in the password file
 *			(getpwent() remembers its place in the file).
 *		return 0 on success, -1 on failure
 */
int
ia_openinfo_next(uinfo_t *uinfo)
{
	struct	xpasswd	*pwd;

	if (uinfo == NULL)
		return(-1);

	*uinfo = (struct master *) malloc(sizeof(**uinfo));

	if ((pwd = (struct xpasswd *) getpwent()) == NULL) {
		free ((void *)*uinfo);
		*uinfo = NULL;
		return(-1);
	}

	return(_ia_openinfo_fill(pwd, uinfo));
}

void
ia_closeinfo(uinfo_t uinfo)
{
	if (uinfo->ia_shellp)
		free((void *)uinfo->ia_shellp);
	if (uinfo->ia_dirp)
		free((void *)uinfo->ia_dirp);
	if (uinfo->ia_sgidp)
		free((void *)uinfo->ia_sgidp);
	free((void *)uinfo);
}

void
ia_get_uid(uinfo_t uinfo, uid_t *uid)
{

	*uid = uinfo->ia_uid;
}

void
ia_get_gid(uinfo_t uinfo, gid_t *gid)
{

	*gid = uinfo->ia_gid;
}


/*
 * get supplementary group id's
 * Rather than get them at ia_openinfo_fill time - we get them here - many
 * programs don't really need them..
 */
int
ia_get_sgid(uinfo_t uinfo, gid_t **sgid, long *cnt)
{
	long gcnt = uinfo->ia_sgidcnt;
	gid_t	groups[NGROUPS_UMAX];

	if (gcnt == -1) {
		/* haven't retrieved them yet */
		gcnt = sgroups(uinfo->ia_name, uinfo->ia_gid, groups);
		uinfo->ia_sgidcnt = gcnt;
		if (gcnt > 0) {
			uinfo->ia_sgidp = malloc(gcnt*sizeof(gid_t));
			(void) memcpy(uinfo->ia_sgidp, groups, gcnt*sizeof(gid_t));
		}
	}

	*cnt = gcnt;
	if (gcnt) {
		*sgid = uinfo->ia_sgidp;
		return(0);
	} else {
		*sgid = (gid_t *)NULL;
		return(-1);
	}
}

void
ia_get_dir(uinfo_t uinfo, char **dir)
{
	*dir = uinfo->ia_dirp;
}

void
ia_get_sh(uinfo_t uinfo, char **shell)
{
	*shell = uinfo->ia_shellp;
}

void
ia_get_logpwd(uinfo_t uinfo, char **passwd)
{
	*passwd = uinfo->ia_pwdp;
}

void
ia_get_pwdpgm(uinfo_t uinfo, char **pwdpgm)
{
	*pwdpgm = uinfo->ia_pwdpgm;
}

void
ia_get_logchg(uinfo_t uinfo, long *logchg)
{
	*logchg = uinfo->ia_lstchg;
}

void
ia_get_logmin(uinfo_t uinfo, long *logmin)
{
	*logmin = uinfo->ia_min;
}

void
ia_get_logmax(uinfo_t uinfo, long *logmax)
{
	*logmax = uinfo->ia_max;
}


void
ia_get_logwarn(uinfo_t uinfo, long *logwarn)
{
	*logwarn = uinfo->ia_warn;
}

void
ia_get_loginact(uinfo_t uinfo, long *loginact)
{
	*loginact = uinfo->ia_inact;
}


void
ia_get_logexpire(uinfo_t uinfo, long *logexpire)
{
	*logexpire = uinfo->ia_expire;
}


void
ia_get_logflag(uinfo_t uinfo, long *logflag)
{
	*logflag = uinfo->ia_flag;
}

static int
_ia_openinfo_fill(struct xpasswd *pwd, uinfo_t *uinfo)
{
	struct	spwd	*spwd;
	uinfo_t ui = *uinfo;

	if (ui == NULL)
		return(-1);

	spwd = getspnam(pwd->pw_name);

	strncpy(ui->ia_name, pwd->pw_name, sizeof(ui->ia_name)-1);
	ui->ia_name[sizeof(ui->ia_name)-1] = '\0';

	ui->ia_uid = pwd->pw_uid;
	ui->ia_gid = pwd->pw_gid;

	if (spwd) {
		strncpy(ui->ia_pwdp, spwd->sp_pwdp, sizeof(ui->ia_pwdp)-1);
		ui->ia_pwdp[sizeof(ui->ia_pwdp)-1] = '\0';
		ui->ia_lstchg	= spwd->sp_lstchg;
		ui->ia_min	= spwd->sp_min;
		ui->ia_max	= spwd->sp_max;
		ui->ia_warn	= spwd->sp_warn;
		ui->ia_inact	= spwd->sp_inact;
		ui->ia_expire	= spwd->sp_expire;
	} else {
		/* /etc/shadow is present, but the entry we need
		 * is not.  Therefore, we don't allow valid passwords
		 * to be returned.
		 */
		strcpy (ui->ia_pwdp, "*LK*");	/* lock passwd */
		ui->ia_lstchg	= -1;
		ui->ia_min	= -1;
		ui->ia_max	= -1;
		ui->ia_warn	= -1;
		ui->ia_inact	= -1;
		ui->ia_expire	= -1;
	}

	if ((pwd->pw_origin != _PW_LOCAL) && (pwd->pw_yp_passwd == NULL))
		ui->ia_pwdpgm = YPPASSWDPGM;
	else
		ui->ia_pwdpgm = PASSWDPGM;

	ui->ia_shsz = strlen(pwd->pw_shell);
	ui->ia_shellp = malloc(ui->ia_shsz+1);
	strcpy(ui->ia_shellp, pwd->pw_shell);

	ui->ia_dirsz = strlen(pwd->pw_dir);
	ui->ia_dirp = malloc(ui->ia_dirsz+1);
	strcpy(ui->ia_dirp, pwd->pw_dir);

	ui->ia_sgidcnt = -1;
	ui->ia_sgidp = NULL;
	return(0);
}
