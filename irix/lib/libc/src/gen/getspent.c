/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libc-port:gen/getspent.c	1.6"

#ifdef __STDC__
	#pragma weak setspent = __sgi_setspent
	#pragma weak endspent = __sgi_endspent
	#pragma weak getspent = __sgi_getspent
	#pragma weak fgetspent = __sgi_fgetspent
	#pragma weak getspnam = __sgi_getspnam
	#pragma weak putspent = __sgi_putspent
#endif
#include "synonyms.h"
#include <stdlib.h>
#include <stdio.h>
#include <pwd.h>
#include <shadow.h>
#include <string.h>
#include <bstring.h>
#include <sys/types.h>
#include <errno.h>
#include <rpcsvc/ypclnt.h>
#include <sys/capability.h>

static FILE *spf _INITBSS;
static char *yp _INITBSS;
static char *oldyp _INITBSS;
static int yplen _INITBSS;
static int oldyplen _INITBSS;
static char *line _INITBSS;
static struct spwd spwd ;

extern struct xpasswd *_pw_buf_handoff(void);
extern int _getpwent_no_yp;
static struct spwd *interpret(char *, const char *);
#define	PWBUFSIZ	BUFSIZ

void
setspent(void)
{
	if (!_getpwent_no_yp)
		_yellowup(1);

	if (spf == NULL)
		spf = fopen(SHADOW, "r") ;
	else
		rewind(spf) ;

	if (yp) {
		free(yp);
		yp = NULL;
	}
}

void
endspent(void)
{
	if (spf != NULL) {
		(void) fclose(spf) ;
		spf = NULL ;
	}
	if (yp) {
		free(yp);
		yp = NULL;
	}
}

static char *
spskip(char *p)
{
	while(*p != ':' && *p != '\n' && *p)
		++p ;
	if(*p == '\n')
		*p = '\0' ;
	else if(*p)
		*p++ = '\0' ;
	return(p) ;
}

static void
pwd_to_spwd(struct passwd *pwd, struct spwd *spwd)
{
	time_t when;
	long minweeks, maxweeks;

	spwd->sp_namp	= pwd->pw_name;
	spwd->sp_pwdp	= pwd->pw_passwd;
	
	if (pwd->pw_age && *pwd->pw_age != NULL) {
		when = (time_t)a64l(pwd->pw_age);
		maxweeks = when & 077;
		minweeks = (when >> 6) & 077;
		when >>= 12;
		
		/* pwd entries are in weeks; shadow entries days */
		spwd->sp_lstchg	= when * 7;
		spwd->sp_min	= minweeks * 7;
		spwd->sp_max	= maxweeks * 7;
	} else {
		spwd->sp_lstchg	= -1;
		spwd->sp_min	= -1;
		spwd->sp_max	= -1;
	}
	
	spwd->sp_warn	= -1;
	spwd->sp_inact	= -1;
	spwd->sp_expire	= -1;
	spwd->sp_flag	= 0;
}

static void
getfirstfromyellow(void)
{
	int reason;
	char *key = NULL;
	int keylen;
	cap_t ocap;
	cap_value_t cap_priv_port[] = {CAP_PRIV_PORT};

	ocap = cap_acquire (1, cap_priv_port);
	reason = yp_first(_yp_domain,"shadow.byname",&key,&keylen,&yp,&yplen);
	cap_surrender (ocap);
	if (reason)
		yp = NULL;
	if (oldyp)
		free(oldyp);
	oldyp = key;
	oldyplen = keylen;
}

static void
getnextfromyellow(void)
{
	int reason;
	char *key = NULL;
	int keylen;
	cap_t ocap;
	cap_value_t cap_priv_port[] = {CAP_PRIV_PORT};

	ocap = cap_acquire (1, cap_priv_port);
	reason = yp_next(_yp_domain, "shadow.byname", oldyp, oldyplen,
			 &key, &keylen, &yp, &yplen);
	cap_surrender (ocap);
	if (reason)
		yp = NULL;

	if (oldyp)
		free(oldyp);
	oldyp = key;
	oldyplen = keylen;
}

static struct spwd *
yp_getspent(void)
{
	if (!yp)
		getfirstfromyellow();
	else
		getnextfromyellow();

	if (!yp)
		return NULL;

	return interpret(yp, NULL);
}

struct spwd *
fgetspent(FILE *f)
{
	char *p;

	if (line == NULL && ((line = malloc(BUFSIZ+1)) == NULL))
		return (NULL);
	p = fgets(line, BUFSIZ, f) ;
	if (p == NULL)
		return NULL;
	return interpret(p, NULL);
}

/*
 * The getspent function will return a NULL for an end of 
 * file indication or a bad entry
 */
struct spwd *
getspent(void)
{
	struct spwd *result;

	/*
	 * Do this check first because we don't want to bother
	 * looking any further if the caller can't look into
	 * /etc/shadow
	 */
	if (spf == NULL && (spf = fopen(SHADOW, "r")) == NULL)
		return NULL;

	/*
	 * If we've already started looking in the NIS database
	 * continue to do so, until they're all gone.
	 */
	if (yp && (result = yp_getspent()) != NULL)
		return result;
	/*
	 * If we weren't looking in the NIS database or if
	 * all the entries there have been exhausted look in the file
	 *
	 * If at EOF drop out.
	 */
	if ((result = fgetspent(spf)) == NULL)
		return NULL;
	/*
	 * If it's not an indication that NIS should be used
	 * pass it back.
	 */
	if (*result->sp_namp != '+')
		return result;
	/*
	 * The '+' tells us to go to NIS.
	 */
	return yp_getspent();
}

static struct spwd *
interpret(char *entry, const char *name)
{
	char *p, *end;
	long x;
	unsigned long ux;

	p = entry;
	spwd.sp_namp = p ;
	p = spskip(p) ;

	/*
	 * Always return a result when an NIS indicator is found.
	 * The caller is responcible for doing something appropriate.
	 */
	if (*spwd.sp_namp == '+') {
		spwd.sp_lstchg = -1 ;
		spwd.sp_min = -1 ;
		spwd.sp_max = -1 ;
		spwd.sp_warn = -1 ;
		spwd.sp_inact = -1 ;
		spwd.sp_expire = -1 ;
		spwd.sp_flag = (unsigned long)-1L ;
		return &spwd;
	}

	if (name && (strcmp(name, spwd.sp_namp) != 0))
		return NULL;

	spwd.sp_pwdp = p ;
	p = spskip(p) ;	/* p points to char after ':' or a \0 */

	x = strtol(p, &end, 10) ;
	if (end == p)	
		spwd.sp_lstchg = -1 ;
	else		
		spwd.sp_lstchg = x ;
	p = end;
	if (*p++ != ':') {
		setoserror(EINVAL);
		return NULL;
	}

	x = strtol(p, &end, 10) ;
	if (end == p)	
		spwd.sp_min = -1 ;
	else		
		spwd.sp_min = x ;
	p = end;
	if (*p++ != ':') {
		setoserror(EINVAL);
		return NULL;
	}

	x = strtol(p, &end, 10) ;
	if (end == p)	
		spwd.sp_max = -1 ;
	else		
		spwd.sp_max = x ;
	p = end;
	if (*p == '\n') {
		/* old format */
		spwd.sp_warn = -1 ;
		spwd.sp_inact = -1 ;
		spwd.sp_expire = -1 ;
		return(&spwd) ;
	}
	if (*p++ != ':') {
		setoserror(EINVAL);
		return NULL;
	}

	x = strtol(p, &end, 10) ;
	if (end == p)	
		spwd.sp_warn = -1 ;
	else		
		spwd.sp_warn = x ;
	p = end;
	if (*p++ != ':') {
		setoserror(EINVAL);
		return NULL;
	}

	x = strtol(p, &end, 10) ;
	if (end == p)	
		spwd.sp_inact = -1 ;
	else		
		spwd.sp_inact = x ;
	p = end;
	if (*p++ != ':') {
		setoserror(EINVAL);
		return NULL;
	}

	x = strtol(p, &end, 10) ;
	if (end == p)	
		spwd.sp_expire = -1 ;
	else		
		spwd.sp_expire = x ;
	p = end;
	if (*p++ != ':') {
		setoserror(EINVAL);
		return NULL;
	}

	ux = strtoul(p, &end, 10) ;
	if (end == p)	
		spwd.sp_flag = (unsigned long)-1L ;
	else		
		spwd.sp_flag = ux ;
	p = end;
	if (*p != ':' && *p != '\n') {
		setoserror(EINVAL);
		return NULL;
	}

	return(&spwd) ;
}


/*
 * _pw_buf_copy copies the contents of 'p' into static fields which are
 * assigned to 'copy'.  This is necessary because 'p' has pointers into a
 * static buffer which gets overwritten with new getpwents.
 */
static void
_pw_buf_copy(struct xpasswd *pwd, struct xpasswd *copy, char *save)
{
	char		*s = save;
	char		*p;

#define fieldcopy(src,dest)		\
	if (src) {			\
		dest = s, p = src;	\
		while ((*s++ = *p++)	\
		  && (int)(s-save) < PWBUFSIZ) \
			;		\
	} else				\
		dest = NULL;

	fieldcopy(pwd->pw_name, copy->pw_name);
	fieldcopy(pwd->pw_passwd, copy->pw_passwd);
	copy->pw_uid = pwd->pw_uid;
	copy->pw_gid = pwd->pw_gid;
	fieldcopy(pwd->pw_age, copy->pw_age);
	fieldcopy(pwd->pw_comment, copy->pw_comment);
	fieldcopy(pwd->pw_gecos, copy->pw_gecos);
	fieldcopy(pwd->pw_dir, copy->pw_dir);
	fieldcopy(pwd->pw_shell, copy->pw_shell);
	copy->pw_origin = pwd->pw_origin;
	fieldcopy(pwd->pw_yp_passwd, copy->pw_yp_passwd);
	fieldcopy(pwd->pw_yp_gecos, copy->pw_yp_gecos);
	fieldcopy(pwd->pw_yp_dir, copy->pw_yp_dir);
	fieldcopy(pwd->pw_yp_shell, copy->pw_yp_shell);
	fieldcopy(pwd->pw_yp_netgroup, copy->pw_yp_netgroup);
}


/*
 * When the shadow file is missing, svr4 programs can't cope.
 * Irix, however, puts together as much of the secure password entry
 * as possible out of the password file.
 *
 * This functionality is provided only for getspnam, not for getspent,
 * because getpwent and getspent would then both increment the
 * password file pointer, and alternate records would be omitted.
 */
static struct spwd *
partial_getspnam(const char * name)
{
	struct passwd		*p;
	struct xpasswd		*passwd;
	static char *save;
	static struct spwd	*sp;
	static struct xpasswd	*passwd_copy;
	int			made_copy = 0;

	if (save == NULL) {
		save = malloc(PWBUFSIZ);
		sp = calloc(1, sizeof(*sp));
		passwd_copy = calloc(1, sizeof(*passwd_copy));
		if (save == NULL ||
		    sp == NULL ||
		    passwd_copy == NULL) {
			save = NULL;
			return NULL;
		}
	}
	/* Don't bother with getpwnam if the right entry is already in the   */
	/* static buffer, like when getpwnam & getspnam are called together. */
	passwd = _pw_buf_handoff();
	if (passwd->pw_name == NULL || strcmp(passwd->pw_name, name) != 0) {
		_pw_buf_copy(passwd, passwd_copy, save);
		made_copy = 1;
		/*
		 * Note that we will maintain a user's getpw* return
		 * value (cause all getpw* routines return 'passwd')
		 * but we have changed its contents to point to our save
		 * buffer.
		 */
		p = getpwnam(name);
		if (!p) {
			bcopy(passwd_copy, passwd, sizeof(*passwd_copy));
			return NULL;
		}
	} else
		p = (struct passwd *) passwd;

	pwd_to_spwd(p, sp);

	if (made_copy)
		bcopy(passwd_copy, passwd, sizeof(*passwd_copy));

	return(sp);
}


struct spwd *
getspnam(const char *name)
{
	register struct spwd *p = NULL;
	static int spnam_lock;
	char *entry;

	setspent();

	/*
	 * If there's no shadow available, either because it doesn't
	 * exist or because this process isn't allowed to read it,
	 * call partial_getspnam(name) to get the cooresponding
	 * entry from the passwd database, and cobble a shadow entry,
	 * as described above.
	 *
	 * XXX:casey
	 *	This function probably ought to discriminate between
	 *	those two cases, returning NULL if the process lacks
	 *	appropriate privilege, and the cobbled version otherwise.
	 */
	if (!spf) {
		if (spnam_lock)	/* prevent infinite getpwname/getspnam loop */
			return NULL;
		spnam_lock = 1;
		p = partial_getspnam(name);
		spnam_lock = 0;
	} else {
		if (line == NULL && ((line = malloc(BUFSIZ+1)) == NULL))
			return (NULL);

		while (p == NULL && (entry = fgets(line,BUFSIZ,spf)) != NULL) {
			/*
			 * These aren't the droids we're looking for.
			 */
			if ((p = interpret(entry, name)) == NULL)
				continue;
			/*
			 * Look for it in NIS, if there's a '+'.
			 * Otherwise, we're done
			 */
			if (*p->sp_namp == '+') {
				if (yp == NULL)
					getfirstfromyellow();
				if (yp == NULL) {
					p = NULL;
					continue;
				}
				for (p = interpret(yp, name);
				    yp != NULL && p == NULL;
				    p = interpret(yp, name))
					getnextfromyellow();
			}
		}
	}
	endspent() ;
	return (p) ;
}

int
putspent(const struct spwd *p, FILE *f)
{
	(void) fprintf ( f, "%s:%s:", p->sp_namp, p->sp_pwdp ) ;
	if ( p->sp_lstchg >= 0 )
	   (void) fprintf ( f, "%ld:", p->sp_lstchg ) ;
	else
	   (void) fprintf ( f, ":" ) ;
	if ( p->sp_min >= 0 )
	   (void) fprintf ( f, "%ld:", p->sp_min ) ;
	else
	   (void) fprintf ( f, ":" ) ;
	if ( p->sp_max >= 0 )
	   (void) fprintf ( f, "%ld:", p->sp_max ) ;
	else
	   (void) fprintf ( f, ":" ) ;
	if ( p->sp_warn > 0 )
	   (void) fprintf ( f, "%ld:", p->sp_warn ) ;
	else
	   (void) fprintf ( f, ":" ) ;
	if ( p->sp_inact > 0 )
	   (void) fprintf ( f, "%ld:", p->sp_inact ) ;
	else
	   (void) fprintf ( f, ":" ) ;
	if ( p->sp_expire > 0 )
	   (void) fprintf ( f, "%ld:", p->sp_expire ) ;
	else
	   (void) fprintf ( f, ":" ) ;
	if ( p->sp_flag != 0 )
	   (void) fprintf ( f, "%ld\n", p->sp_flag ) ;
	else
	   (void) fprintf ( f, "\n" ) ;

	(void) fflush(f);
	return(ferror(f)) ;
}
