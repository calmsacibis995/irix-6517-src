/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"$Revision: 1.11 $"

#ifndef _IA_DOT_H_
#define _IA_DOT_H_

#ifdef __cplusplus
extern "C" {
#endif

/* structure for the master */

struct	master {
	char	ia_name[32];
	char    ia_pwdp[14] ;	/* user password + NULL */
	long	ia_uid;
	long	ia_gid;
	long	ia_lstchg;	/* password lastchanged date */
	long	ia_min;	/* minimum number of days between password changes */
	long	ia_max;		/* number of days password is valid */
	long	ia_warn;/* number of days to warn user to change passwd */
	long	ia_inact;/* number of days the login may be inactive */
	long	ia_expire;/* date when the login is no longer valid */
	long	ia_flag;	/* not used  */
	long	ia_dirsz;
	long	ia_shsz;
	long	ia_sgidcnt;
	char	*ia_dirp;
	char	*ia_shellp;
	char	*ia_pwdpgm;	/* name of passwd modification program */
	gid_t	*ia_sgidp;
};

typedef struct master *uinfo_t;

/*
 *	 Declare all ANSI-C I&A functions 
 */
extern	int	ia_openinfo(char *, uinfo_t *);
extern	int	ia_openinfo_next(uinfo_t *);
extern	void	ia_closeinfo(uinfo_t);
extern	char *	ia_get_name(uid_t);
extern	int	ia_get_sgid(uinfo_t, gid_t **, long *);
extern	void	ia_get_uid(uinfo_t, uid_t *);
extern	void	ia_get_gid(uinfo_t, gid_t *);
extern	void	ia_get_dir(uinfo_t, char **);
extern	void	ia_get_sh(uinfo_t, char **);
extern	void	ia_get_logpwd(uinfo_t, char **);
extern	void	ia_get_pwdpgm(uinfo_t, char **);
extern	void	ia_get_logchg(uinfo_t, long *);
extern	void	ia_get_logmin(uinfo_t, long *);
extern	void	ia_get_logmax(uinfo_t, long *);
extern	void	ia_get_logwarn(uinfo_t, long *);
extern	void	ia_get_loginact(uinfo_t, long *);
extern	void	ia_get_logexpire(uinfo_t, long *);
extern	void	ia_get_logflag(uinfo_t, long *);

#ifdef __cplusplus
}
#endif

#endif /* _IA_DOT_H_ */

