#if !defined(lint) && defined(SCCSIDS)
static char sccsid[] = 	"@(#)getpwent.c	1.6 88/05/11 4.0NFSSRC SMI"; /* from UCB 5.2 3/9/86 SMI 1.25 */
#endif

#ident "@(#)libc-port:gen/getpwent.c $Revision: 2.60 $"

#ifndef DSHLIB
#ifdef __STDC__
	#pragma weak fgetpwent = __sgi_fgetpwent
	#pragma weak _yp_setpwent = __yp_setpwent
	#pragma weak _yp_getpwent = __yp_getpwent
	#pragma weak getpwnam_r = __sgi_getpwnam_r
	#pragma weak getpwuid_r = __sgi_getpwuid_r
#endif
#endif
#include "synonyms.h"

#include <unistd.h>
#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <bstring.h>
#include <values.h>
#include <pwd.h>
#include <shadow.h>
#include <errno.h>
#include <ctype.h>
#include <netdb.h>
#include <netinet/in.h>			/* for prototyping */
#include <rpc/rpc.h>			/* for prototyping */
#include <rpcsvc/yp_prot.h>		/* for prototyping */
#include <rpcsvc/ypclnt.h>

#define	PWBUFSIZ	BUFSIZ

/*
 * flag values..
 */
#define PASSTHRU_YP	0x0001	/* pass back YP entries even if Yp is off */
#define IGNORE_YP	0x0002	/* toss YP/netgrp entries */
#define COPY_LINE	0x0004	/* in interpret - copy line */
#define CHECKUID	0x0008	/* look for uid match */

int _getpwent_no_yp = 0;	/* just the facts, no YP interp of +/- */
int _getpwent_no_shadow = 0;	/* Don't copy password from /etc/shadow
				 * Note that for things that don't need
				 * the passwd info this REALLY speed getpwent
				 */

#define EMPTY ""
static FILE *pwf _INITBSS;	/* pointer into /etc/passwd */
static char *yp _INITBSS;	/* pointer into NIS */
static int yplen _INITBSS;
static char *line _INITBSS;
static int linelen _INITBSS;

/* for interpret(), and _pw_buf_handoff() */
static struct xpasswd passwd = { 0 };

static struct xpasswd *getnamefromyellow(const char *, struct passwd *);
static struct xpasswd *getuidfromyellow(uid_t, struct passwd *);
static struct xpasswd *interpretwithsave(char *, int, struct passwd *, int);
static struct passwd *save(struct xpasswd *pw);
static void getfirstfromyellow(void);
static void getnextfromyellow(void);
static struct xpasswd *interpret(char *, int, FILE *, int, uid_t);

static int matchname(char line1[], struct xpasswd **pwp, const char *name);
static int matchuid(char line1[], struct xpasswd **pwp, uid_t uid);
static void addtominuslist(char *name);
static void freeminuslist(void);
static int onminuslist(struct xpasswd *pw);
static uid_t uidof(const char *name);
static void superimpose_shadow(struct xpasswd *);
struct xpasswd * _pw_buf_handoff(void);

static char *netgroup _INITBSS;
#define CHKNG	(netgroup == NULL && ((netgroup = calloc(1, PWBUFSIZ)) == NULL))

#include <di_passwd.h>
#include <ssdi.h>

static int setshadow(void);
static void endshadow(int);

void 		__files_setpwent(void);
void 		__files_endpwent(void);
struct passwd 	*__files_getpwnam(const char *nam);
struct passwd 	*__files_getpwuid(uid_t uid);
struct passwd 	*__files_getpwent(void);

_SSDI_VOIDFUNC __files_passwd_ssdi_funcs[] = {
	(_SSDI_VOIDFUNC) __files_getpwnam,
	(_SSDI_VOIDFUNC) __files_getpwuid,
	(_SSDI_VOIDFUNC) __files_setpwent,
	(_SSDI_VOIDFUNC) __files_getpwent,
	(_SSDI_VOIDFUNC) __files_endpwent
};

/*
 * Read line and extend buffer as needed
 */
char *
__getline(FILE *fp, char *l, int *len)
{
	register int cur = 0;
	int llen = *len;
	char *nl;

	if (llen == 0)
		*len = llen = 200;
	if ((l == NULL) && ((l = malloc(llen)) == NULL)) {
		*len = 0;
		return NULL;
	}
	l[llen - 2] = '\n';	/* if changed, line was too long */
	for (;;)		/* read entire line, grow if necessary */
	{
		if (fgets(&l[cur], llen - cur, fp) == NULL) {
			*len = 0;
			free(l);
			return NULL;
		}
		if (l[llen - 2] == '\n')
			break;
		cur = llen - 1;
		llen <<= 1;
		*len = llen;
		if ((nl = realloc(l, llen)) == NULL) {
			*len = 0;
			free(l);
			return NULL;
		}
		l = nl;
		l[llen - 2] = '\n';
	}

	return l;
}

/*
 * superimpose_shadow - if a shadow password is available and the caller
 *			is privileged to see it, provide it instead of
 *			the value from /etc/passwd or the yp data base.
 *
 *	This permits programs that haven't been modified to understand
 *	shadow passwords to run in a shadow environment, so long as they
 *	are setuid root.
 */
static void
superimpose_shadow(struct xpasswd * pw)
{
	struct spwd *	s;

	if (!pw)
		return;

	if (_getpwent_no_shadow)	/* means return pw contents verbatim */
		return;			/* or there is no /etc/shadow file */

	/*
	 * If there is a shadow file, we must invalidate the /etc/passwd
	 * version of the password, and use only what we find in /etc/shadow.
	 * If there's no corresponding entry in /etc/shadow, there's no
	 * valid password.
	 */
	pw->pw_passwd = "x";
	pw->pw_age = "";

	if (!(s = getspnam(pw->pw_name)))
		return;

	pw->pw_passwd = s->sp_pwdp;
	/* XXX unconvert shadow ageing?? */
}

/*
 * _pw_buf_handoff is a back door, giving access to interpret's
 * static variable to _partial_getspnam() in getspent.c
 */
struct xpasswd *
_pw_buf_handoff(void)
{
	return(&passwd);
}

/*
 * SGI added yp-related extensions to the standard struct passwd.
 * This module uses the sgi-extended passwd structure in place of
 * the standard struct passwd. Functions returning a pointer to the
 * stardard struct passwd casts from pointer to the sgi-extended
 * passwd structure.
 */

#define _pw_file "/etc/passwd"
int	_pw_stayopen = 0;

static struct list {		/* list of - items */
	char		*name;
	struct list	*nxt;
} *minuslist _INITBSS;

struct passwd *
__files_getpwnam(const char *nam)
{
	struct xpasswd *pw;
	int flags = 0;
	int oshadow;

	__files_setpwent();
	if (!pwf)
		return NULL;
	oshadow = setshadow();

	if (_getpwent_no_yp)
		flags |= PASSTHRU_YP;
	if (!_yp_is_bound)
		flags |= IGNORE_YP;

	while ((line = __getline(pwf, line, &linelen)) != NULL) {
		pw = interpret(line, 0, pwf, flags, 0);
		if (pw == NULL)
			continue;

		if (flags & (IGNORE_YP|PASSTHRU_YP)) {
			if (strcmp(pw->pw_name, nam))
				continue;
			if (flags & IGNORE_YP)
				superimpose_shadow(pw);
			goto out;
		} else if (matchname(line, &pw, nam)) {
			superimpose_shadow(pw);
			goto out;
		}
	}
	pw = NULL;
out:
	endshadow(oshadow);
	if (!_pw_stayopen)
		__files_endpwent();
	return ((struct passwd *)pw);		/* cast for xpasswd sub-set */
}

struct passwd *
__files_getpwuid(uid_t uid)
{
	struct xpasswd *pw;
	int flags = 0;
	int oshadow;

	__files_setpwent();
	if (!pwf)
		return NULL;
	oshadow = setshadow();

	if (_getpwent_no_yp)
		flags |= PASSTHRU_YP;
	if (!_yp_is_bound)
		flags |= IGNORE_YP;
	flags |= CHECKUID;

	while ((line = __getline(pwf, line, &linelen)) != NULL) {
		pw = interpret(line, 0, pwf, flags, uid);
		if (pw == NULL)
			continue;
		/*
		 * PASSTHRU_YP is a compatibility mode
		 * that doesn't do any interpretation of entries
		 * with '+' or '-' but just passes them back.
		 * Also - no shadow handling is done.
		 *
		 * IGNORE_YP means really 2 things - one can toss entries
		 * beginning with a '+' or '-' as well as we can simply
		 * check the pw_uid field.
		 * the '+' and '-' entries are removed in interpret
		 */
		if (flags & (IGNORE_YP|PASSTHRU_YP)) {
			if (pw->pw_uid != uid)
				continue;
			if (flags & IGNORE_YP)
				superimpose_shadow(pw);
			goto out;
		} else if (matchuid(line, &pw, uid)) {
			superimpose_shadow(pw);
			goto out;
		}
	}
	pw = NULL;
out:
	endshadow(oshadow);
	if (!_pw_stayopen)
		__files_endpwent();
	return ((struct passwd *)pw);		/* cast for xpasswd sub-set */
}

void
__files_setpwent(void)
{
	/*
	 * If _pw_stayopen is set, we don't really need to re-check for 
	 * _yellowup.  This helps ls....
	 */
	if (pwf == NULL) {
		pwf = fopen(_pw_file, "r");
		if (!_getpwent_no_yp)
			_yellowup(1);		/* recheck whether NIS are up */
	} else
		rewind(pwf);
	yp = NULL;
	freeminuslist();
}

void
__files_endpwent(void)
{
	if (pwf != NULL) {
		fclose(pwf);
		pwf = NULL;
	}
	yp = NULL;
	freeminuslist();
	endnetgrent();
}

/*
 * We need be careful about the state of _getpwent_no_shadow -
 * various programs set this, we shouldn' undo what they want.
 * However we need to somehow reset it if WE changed it to 1 - consider
 * the case of proram that isn't root that calls getpwnam, and we set
 * _getpwent_no_shadow to 1 - then it becomes root (cause it was originally
 * root), and again calls getpwnam - we need to give them the shadow info.
 *
 * The main performance implication is that each time one calls getpw{nam,uid}
 * we now have to call 'access', even if they set _pw_stayopen - of course
 * if the app sets _getpwent_no_shadow to 1 then we don't do the access call.
 */
static int
setshadow(void)
{
	int o;

	o = _getpwent_no_shadow;
	if (o == 0)
		if (access(SHADOW, (EFF_ONLY_OK|R_OK)) < 0)
			_getpwent_no_shadow = 1;
	return o;
}

static void
endshadow(int o)
{
	_getpwent_no_shadow = o;
}

struct passwd *
fgetpwent(FILE *f)
{
	struct xpasswd *pw;

	for (;;) {
		if ((line = __getline(f, line, &linelen)) == NULL)
			return (NULL);
		pw = interpret(line, 0, f, 0, 0);
		if (pw)
			return ((struct passwd*)pw);
	}
	/* NOTREACHED */
}

static char *
pwskip(char *p)
{
	while(*p != ':' && *p != '\n' && *p)
		++p;
	if (*p == '\n')
		*p = '\0';
	else if (*p != '\0')
		*p++ = '\0';
	return(p);
}

/*
 * Note that we don't perform shadow password compatibility here -
 * the main reason is performance - lots of programs - ls, ps, calendar,
 * csh, accting, quotas, which run as root perform getpwent's but
 * of course could care less about the password/aging info.
 * By running superimposeshadow() we end up scanning both the passwd
 * and shadow file which  ends up being kind on an n**2 algorithm.
 * For systems with a large passwd file (say 1000) entries things
 * would slow to a crawl.
 * Turns out that almost ALL programs that want the passwd info calls
 * getpwnam/uid - so we leave in the compatibility for those.
 */
struct passwd *
__files_getpwent(void)
{
	static struct passwd *savepw _INITBSS;
	struct xpasswd *pw;
	char *user;
	char *mach;
	char *dom;
	int flags = 0;

	if (pwf == NULL) {
		__files_setpwent();
		if (pwf == NULL)
			return NULL;
	}

	/*
	 * XXX why do this???
	 * To pick up a failed binding or new binding during a scan??
	 * this is expensive - a getpid and a time() call per
	 * note that the setpwent above WILL check for yp - thus
	 * each time the caller calls endpwent, then getpwent, we'll check
	if (!_getpwent_no_yp)
		_yellowup(0);
	 */

	/*
	 * Special compatibility with IRIX 4.0, where you could
	 * avoid the YP version by not linking with -lsun.  Since
	 * that's no longer an option, here's an other way...
	 *
	 * Note that we do not do shadow password interpretation
	 * either.  Any program that uses putpwent better handle
	 * shadow passwords directly!  (That's generally why programs
	 * would not want the YP interpretation -- so they can write
	 * out a new /etc/passwd file.)
	 */
	if (_getpwent_no_yp) {
		for (;;) {
			if ((line = __getline(pwf, line, &linelen)) == NULL)
				return(NULL);
			pw = interpret(line, 0, pwf, 0, 0);
			if (pw)
				return((struct passwd*)pw);
		}
	}
	if (!_yp_is_bound)
		flags |= IGNORE_YP;

	if (CHKNG)
		return(NULL);
	for (;;) {
		if (yp) {
			pw = interpretwithsave(yp, yplen, savepw, 0); 
			if (pw == NULL) {
				/*
				 * To be consistent with local semantics,
				 * skip any bad entries instead of stopping.
				 */
				getnextfromyellow();
				continue;
			}
			pw->pw_origin = _PW_YP_ALL;
			getnextfromyellow();
			if (!onminuslist(pw))
				return((struct passwd*)pw);
		} else if (getnetgrent(&mach,&user,&dom)) {
			if (user) {
				pw = getnamefromyellow(user, savepw);
				if (pw != NULL && !onminuslist(pw)) {
					pw->pw_origin = _PW_YP_NETGROUP;
					pw->pw_yp_netgroup = netgroup;
					return((struct passwd*)pw);
				}
			}
		} else {
			endnetgrent();
			for (;;) {
				if ((line = __getline(pwf, line, &linelen)) == NULL)
					return(NULL);
				pw = interpret(line, 0, pwf, flags, 0);
				if (pw)
					break;
			}
			switch (line[0]) {
			case '+':
                                if (!_yp_is_bound)
					continue;
				if (strcmp(pw->pw_name, "+") == 0) {
					getfirstfromyellow();
					savepw = save(pw);
				} else if (line[1] == '@') {
					savepw = save(pw);
					(void) strcpy(netgroup, pw->pw_name+2);
					if (innetgr(netgroup, (char *)NULL,
						    "*", _yp_domain)) {
						/* include whole NIS database */
						getfirstfromyellow();
					} else {
						setnetgrent(netgroup);
					}
				} else {
					/*
					 * else look up this entry in NIS
				 	 */
					savepw = save(pw);
					pw = getnamefromyellow(pw->pw_name+1,
							       savepw);
					if (pw != NULL && !onminuslist(pw)) {
						pw->pw_origin = _PW_YP_USER;
						return((struct passwd*)pw);
					}
				}
				break;
			case '-':
                                if (!_yp_is_bound)
                                    continue;
				if (line[1] == '@') {
					if (innetgr(pw->pw_name+2, (char *)NULL,
						    "*", _yp_domain)) {
						/* everybody was subtracted */
						return(NULL);
					}
					setnetgrent(pw->pw_name+2);
					while (getnetgrent(&mach,&user,&dom)) {
						if (user) {
							addtominuslist(user);
						}
					}
					endnetgrent();
				} else {
					addtominuslist(pw->pw_name+1);
				}
				break;
			default:
				if (!onminuslist(pw))
					return((struct passwd*)pw);
				break;
			}
		}
	}
	/* NOTREACHED */
}

static int
matchname(char line1[], struct xpasswd **pwp, const char *name)
{
	struct passwd *savepw;
	struct xpasswd *pw = *pwp;

	switch(line1[0]) {
	case '+':
		if (strcmp(pw->pw_name, "+") == 0) {
			savepw = save(pw);
			pw = getnamefromyellow(name, savepw);
			if (pw) {
				pw->pw_origin = _PW_YP_ALL;
				*pwp = pw;
				return 1;
			}
			return 0;
		}
		if (line1[1] == '@') {
			if (CHKNG)
				return 0;
			if (innetgr(pw->pw_name+2, (char *)NULL, (char *)name,
				    _yp_domain)) {
				savepw = save(pw);
				strcpy(netgroup, pw->pw_name+2);
				pw = getnamefromyellow(name, savepw);
				if (pw) {
					pw->pw_origin = _PW_YP_NETGROUP;
					pw->pw_yp_netgroup = netgroup;
					*pwp = pw;
					return 1;
				}
			}
			return 0;
		}
		if (strcmp(pw->pw_name+1, name) == 0) {
			savepw = save(pw);
			pw = getnamefromyellow(pw->pw_name+1, savepw);
			if (pw) {
				pw->pw_origin = _PW_YP_USER;
				*pwp = pw;
				return 1;
			}
			return 0;
		}
		break;
	case '-':
		if (line1[1] == '@') {
			if (innetgr(pw->pw_name+2, (char *)NULL, (char *)name,
				    _yp_domain)) {
				*pwp = NULL;
				return 1;
			}
		}
		else if (strcmp(pw->pw_name+1, name) == 0) {
			*pwp = NULL;
			return 1;
		}
		break;
	default:
		if (strcmp(pw->pw_name, name) == 0)
			return 1;
	}
	return 0;
}

static int
matchuid(char line1[], struct xpasswd **pwp, uid_t uid)
{
	struct passwd *savepw;
	struct xpasswd *pw = *pwp;

	switch(line1[0]) {
	case '+':
		savepw = save(pw);
		if (strcmp(pw->pw_name, "+") == 0) {
			pw = getuidfromyellow(uid, savepw);
			if (pw) {
				pw->pw_origin = _PW_YP_ALL;
				*pwp = pw;
				return 1;
			}
			return 0;
		}
		if (line1[1] == '@') {
			if (CHKNG)
				return 0;
			strcpy(netgroup, pw->pw_name+2);
			pw = getuidfromyellow(uid, savepw);
			if (pw && innetgr(netgroup, (char *)NULL,
					  pw->pw_name,_yp_domain)) {
				pw->pw_origin = _PW_YP_NETGROUP;
				pw->pw_yp_netgroup = netgroup;
				*pwp = pw;
				return 1;
			}
			return 0;
		}
		pw = getnamefromyellow(pw->pw_name+1, savepw);
		if (pw && pw->pw_uid == uid) {
			pw->pw_origin = _PW_YP_USER;
			*pwp = pw;
			return 1;
		}
		break;
	case '-':
		if (line1[1] == '@') {
			pw = getuidfromyellow(uid, NULL);
			if (pw && innetgr(pw->pw_name+2, (char *)NULL,
					  pw->pw_name,_yp_domain)) {
				*pwp = NULL;
				return 1;
			}
		} else if (uid == uidof(pw->pw_name+1)) {
			*pwp = NULL;
			return 1;
		}
		break;
	default:
		if (pw->pw_uid == uid)
			return 1;
	}
	return 0;
}

static uid_t
uidof(const char *name)
{
	struct xpasswd *pw;

	pw = getnamefromyellow(name, NULL);
	if (pw == NULL)
		return MAXINT;
	return pw->pw_uid;
}

/*
 * Use yp_all over TCP/IP instead of yp_first/yp_next over UDP to implement
 * getfirstfromyellow/getnextfromyellow.  One stream RPC is much, much faster
 * than 1000 datagram RPCs.
 */
struct ypent {
	char	*val;			/* value string and length */
	int	vallen;			/* does not include space for '\0' */
};

struct ypbuf {
	unsigned int	maxsize;	/* maximum number of entries */
	unsigned int	size;		/* current number of entries */
	struct ypent	*base;		/* array of entries */
	struct ypent	*next;		/* next entry to fetch */
	unsigned int	count;		/* number of remaining entries */
	long		exptime;	/* expiration timestamp */
};

#define	YPALL_BUFSIZE	1024		/* initial number of yp_all entries */
#define	YPALL_BUFTIME	90		/* seconds from yp_all to exptime */

static struct ypbuf ypallbuf = { 0 };

static void
getnextfromyellow(void)
{
	struct ypent *ent;

	if (--ypallbuf.count <= 0) {
		yp = NULL;
		return;
	}
	ent = ypallbuf.next++;
	yp = ent->val;
	yplen = ent->vallen;
}

/* ARGSUSED */
static int
callback(int status, char *key, int keylen, char *val, int vallen, char *null)
{
	struct ypent *ent;
	char *newval;

	if (!status)
		return 1;
	if (ypallbuf.size == ypallbuf.maxsize) {
		/*
		 * Can't use realloc because whether it frees the initial
		 * allocation or leaves it busy depends on which malloc(3)
		 * you're using.  Besides, using malloc+free we bootstrap
		 * the buffer with the same code that grows it.
		 */
		ypallbuf.maxsize += YPALL_BUFSIZE;
		ent = malloc(ypallbuf.maxsize * sizeof *ypallbuf.base);
		if (ent == NULL) {
			ypallbuf.exptime = 0;
			return 1;
		}
		if (ypallbuf.base) {
			bcopy(ypallbuf.base, ent, (int)ypallbuf.size * (int)sizeof *ent);
			free(ypallbuf.base);
		}
		ypallbuf.base = ent;
	}
	newval = malloc((size_t)vallen + 1);
	if (newval == NULL) {
		ypallbuf.exptime = 0;
		return 1;
	}
	ent = &ypallbuf.base[ypallbuf.size++];
	bcopy(val, newval, vallen);
	newval[vallen] = '\0';
	ent->val = newval;
	ent->vallen = vallen;
	ypallbuf.count++;
	return 0;
}

static struct ypall_callback ypallcb = { callback, NULL };

static void
getfirstfromyellow(void)
{
	time_t now;
	int reason;

	now = time(NULL);
	if (ypallbuf.base) {
		int count;
		struct ypent *ent;

		if (now < ypallbuf.exptime) {
			ypallbuf.next = ypallbuf.base;
			ypallbuf.count = ypallbuf.size;
			getnextfromyellow();
			return;
		}
		count = (int)ypallbuf.size;
		for (ent = ypallbuf.base; --count >= 0; ent++)
			free(ent->val);
		free((char *) ypallbuf.base);
		ypallbuf.maxsize = 0;
	}
	ypallbuf.size = ypallbuf.count = 0;
	ypallbuf.base = NULL;
	reason = yp_all(_yp_domain, "passwd.byname", &ypallcb);
	if (reason) {
#ifdef DEBUG
fprintf(stderr, "reason yp_all failed is %d\n", reason);
#endif
		yp = NULL;
		return;
	}
	ypallbuf.next = ypallbuf.base;
	ypallbuf.exptime = now + YPALL_BUFTIME;
	getnextfromyellow();
}

/*
 * These functions are exported for use by finger, vadmin, etc.
 */
static int yp_gotfirst = 0;

void
_yp_setpwent(void)
{
	yp_gotfirst = 0;
}

struct xpasswd *
_yp_getpwent(void)
{
	struct xpasswd *pw;

	do {
		if (yp_gotfirst)
			getnextfromyellow();
		else {
			getfirstfromyellow();
			yp_gotfirst = (yp != 0);
		}
		if (yp == 0)
			return 0;
		pw = interpret(yp, yplen, 0, 0, 0);
	} while (!pw);
	pw->pw_origin = _PW_YP_REMOTE;
	return pw;
}

static struct xpasswd *
getnamefromyellow(const char *name, struct passwd *savepw)
{
	struct xpasswd *pw;
	int reason;
	char *val;
	int vallen;
	
	reason = yp_match(_yp_domain, "passwd.byname", name, (int)strlen(name),
			  &val, &vallen);
	if (reason) {
#ifdef DEBUG
fprintf(stderr, "reason yp_match failed is %d\n", reason);
#endif
		return NULL;
	}
	/*
	 * COPY_LINE since we wish to free(val)
	 * NOTE - this trashes the global 'line' buffer
	 */
	pw = interpretwithsave(val, vallen, savepw, COPY_LINE);
	free(val);
	return pw;
}

static struct xpasswd *
getuidfromyellow(uid_t uid, struct passwd *savepw)
{
	struct xpasswd *pw;
	int reason;
	char *val;
	int vallen;
	char uidstr[20];

	(void) sprintf(uidstr, "%d", uid);
	reason = yp_match(_yp_domain, "passwd.byuid", uidstr, (int)strlen(uidstr),
			  &val, &vallen);
	if (reason) {
#ifdef DEBUG
fprintf(stderr, "reason yp_match failed is %d\n", reason);
#endif
		return NULL;
	}
	/*
	 * COPY_LINE since we wish to free(val)
	 * NOTE - this trashes the global 'line' buffer
	 */
	pw = interpretwithsave(val, vallen, savepw, COPY_LINE);
	free(val);
	return pw;
}

static struct xpasswd *
interpretwithsave(char *val, int len, struct passwd *savepw, int flags)
{
	struct xpasswd *pw;
	
	pw = interpret(val, len, NULL, flags, 0);
	if (pw == NULL)
		return NULL;
	if (savepw && savepw->pw_passwd && *savepw->pw_passwd) {
		pw->pw_yp_passwd = pw->pw_passwd;
		pw->pw_passwd = savepw->pw_passwd;
	}
	if (savepw && savepw->pw_gecos && *savepw->pw_gecos) {
		pw->pw_yp_gecos = pw->pw_gecos;
		pw->pw_gecos = savepw->pw_gecos;
	}
	if (savepw && savepw->pw_dir && *savepw->pw_dir) {
		pw->pw_yp_dir = pw->pw_dir;
		pw->pw_dir = savepw->pw_dir;
	}
	if (savepw && savepw->pw_shell && *savepw->pw_shell) {
		pw->pw_yp_shell = pw->pw_shell;
		pw->pw_shell = savepw->pw_shell;
	}
	return pw;
}

/*
 * god help us - this is externalized!
 */
struct xpasswd *
_pw_interpret(char *val, int len, FILE *file)
{
	return interpret(val, len, file, COPY_LINE, 0);
}

static struct xpasswd *
interpret(char *val, int len, FILE *file, int flags, uid_t uid)
{
	char *p;
	char *end;
	size_t x;
	int ypentry;
	int c;

	/*
	 * val may not be newline nor NULL terminated.
	 * Seems like most will be - this is extra safety
	 */
	if (len && val[len-1] != '\n')
		flags |= COPY_LINE;

	/* Skip empty lines, allow # to be used as comment character */
	p = val;
	while (*p && isspace(*p))
		p++;
	if (*p == '\n' || *p == '#' || *p == '\0')
		return (NULL);

	/*
 	 * Set "ypentry" if this entry references the NIS;
	 * if so, null UIDs and GIDs are allowed (because they will be
	 * filled in from the matching NIS entry).
	 */
	p = val;
	ypentry = (*p == '+' || *p == '-');
	if (ypentry && (flags & IGNORE_YP))
		return NULL;

	if (flags & COPY_LINE) {
		if (linelen < len + 2) {
			if (line == NULL)
				line = malloc(len + 2);
			else
				line = realloc(line, len + 2);
			linelen = len + 2;
		}
		(void) strncpy(line, val, (size_t)len);
		p = line;
		p[len] = '\n';
		p[len+1] = 0;
	}

	passwd.pw_name = p;
	p = pwskip(p);
	passwd.pw_passwd = p;
	p = pwskip(p);
	if (*p == ':' && !ypentry)
		/* check for non-null uid */
		return (NULL);
	x = strtol(p, &end, 10);
	p = end;
	if (*p++ != ':' && !ypentry)
		/* check for numeric value - must have stopped on the colon */
		return (NULL);
	if ((flags & CHECKUID) && !ypentry && uid != x)
		return NULL;
	passwd.pw_uid = (uid_t )x;
	if (*p == ':' && !ypentry)
		/* check for non-null gid */
		return (NULL);
	x = strtol(p, &end, 10);	
	p = end;
	if (*p != ':' && !ypentry)
		/* check for numeric value - must have stopped on the colon */
		return (NULL);
	if (*p)
		p++;
	passwd.pw_gid = (gid_t)x;
	passwd.pw_comment = EMPTY;
	passwd.pw_gecos = p;
	p = pwskip(p);
	passwd.pw_dir = p;
	p = pwskip(p);
	/* 
	 * Remove whitespace from the shell field. Since it's the last
	 * field in the file, trailing spaces are easy to miss.
	 */
	while (*p && *p != '\n' && isspace(*p))
		p++;
	passwd.pw_shell = p;
	while (*p && *p != ':' && *p != '\n')
		p++;
	/*
	 * At end of line - if line didn't end with a \n then we really
	 * probably had a too long line and we should discard it and
	 * up to the next newline
	 */
	if (*p && *p != '\n') {
		char *q = p;
		while (*q && *q != '\n')
			q++;
		
		if (*p == '\0')
			goto runout;
	}
	*p = '\0';
	while ( *--p == ' ' )
		*p = '\0';
	/*
	 * Extract 'age' field out of passwd field if present
	 */
	p = passwd.pw_passwd;
	while (*p && *p != ',')
		p++;
	if (*p)
		*p++ = '\0';
	passwd.pw_age = p;
	passwd.pw_origin = _PW_LOCAL;
	passwd.pw_yp_passwd = NULL;
	passwd.pw_yp_gecos = NULL;
	passwd.pw_yp_dir = NULL;
	passwd.pw_yp_shell = NULL;
	passwd.pw_yp_netgroup = NULL;
	return(&passwd);

runout:
	if (file) {
		while((c = getc(file)) != '\n' && c != EOF)
			;
	}
	return (NULL);
}

static void
freeminuslist(void)
{
	struct list *ls;
	struct list *nxt;

	for (ls = minuslist; ls != NULL; ls = nxt) {
		nxt = ls->nxt;
		free(ls->name);
		free((char *) ls);
	}
	minuslist = NULL;
}

static void
addtominuslist(char *name)
{
	struct list *ls;
	char *buf;

	ls = malloc(sizeof(struct list));
	buf = malloc((unsigned) strlen(name) + 1);
	if (ls == NULL || buf == NULL)
		return;
	ls->name = strcpy(buf, name);
	ls->nxt = minuslist;
	minuslist = ls;
}

static struct passwd *sv _INITBSS;

/* 
 * save away psswd, gecos, dir and shell fields, which are the only
 * ones which can be specified in a local + entry to override the
 * value in the NIS
 */
static struct passwd *
save(struct xpasswd *pw)
{
	if (sv == 0) {
		sv = malloc(sizeof *sv);
		if (sv == 0)
			return 0;
	} else {
		/* free stuff saved during the last call */
		if (sv->pw_passwd)
			free(sv->pw_passwd);
		if (sv->pw_gecos)
			free(sv->pw_gecos);
		if (sv->pw_dir)
			free(sv->pw_dir);
		if (sv->pw_shell)
			free(sv->pw_shell);
	}
	sv->pw_passwd = strdup(pw->pw_passwd);
	sv->pw_gecos = strdup(pw->pw_gecos);
	sv->pw_dir = strdup(pw->pw_dir);
	sv->pw_shell = strdup(pw->pw_shell);
	/* XXX eh??
	if (sv->pw_passwd == NULL || sv->pw_gecos == NULL || sv->pw_dir == NULL
	    || sv->pw_shell == NULL) {
		return (sv);
	}
	*/

	return (sv);
}

static int
onminuslist(struct xpasswd *pw)
{
	struct list *ls;
	char *nm;

	nm = pw->pw_name;
	for (ls = minuslist; ls != NULL; ls = ls->nxt) {
		if (strcmp(ls->name,nm) == 0) {
			return(1);
		}
	}
	return(0);
}

/* ARGSUSED */
int
getpwnam_r(
	const char *name,
	struct passwd *pwd,
	char *buf,
	size_t bufsize,
	struct passwd **result)
{
	return ENOSYS;
}

/* ARGSUSED */
int
getpwuid_r(
	uid_t uid,
	struct passwd *pwd,
	char *buf,
	size_t bufsize,
	struct passwd **result)
{
	return ENOSYS;
}
