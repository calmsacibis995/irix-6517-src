#ifdef __STDC__
	#pragma weak getspnam_r = _getspnam_r
	#pragma weak getspnam = _getspnam
	#pragma weak getspent_r = _getspent_r
	#pragma weak getspent = _getspent
	#pragma weak setspent = _setspent
	#pragma weak endspent = _endspent
	#pragma weak fgetspent_r = _fgetspent_r
	#pragma weak fgetspent = _fgetspent
	#pragma weak putspent = _putspent
#endif

#include "synonyms.h"
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#define _SGI_REENTRANT_FUNCTIONS
#include <string.h>
#include <mutex.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <shadow.h>
#include <pwd.h>
#include <errno.h>
#include <ns_api.h>
#include "compare.h"

#define LOCALBUFLEN	4096
#define LOCALFILE	"/etc/shadow"
#define LISTMAP		"shadow"
#define SHADOW_PW	2

static FILE *_file = (FILE *)0;
static struct spwd *_entry _INITBSS;
static char *_buffer _INITBSS;

static ns_map_t _map_byname _INITBSSS;

extern int _getpwent_no_yp;
extern int _getXbyY_no_stat;

extern int _ltoa(long, char *);
extern int __pwnam(const char *, struct xpasswd *, char *, size_t,
    struct passwd **, unsigned);

void
setspent(void)
{
	if (_file) {
		rewind(_file);
	}
}

static void
setspent_internal (void)
{
	FILE *fp = 0;

	setspent();
	if (!_file) {
		if (! _getpwent_no_yp) {
			fp = ns_list(0, 0, LISTMAP, 0);
		}
		if (! fp) {
			fp = fopen(LOCALFILE, "r");
		}
		if (! __compare_and_swap((long *)&_file, 0, (long)fp)) {
			/* Race condition. */
			fclose(fp);
		}
	}
}

void
endspent(void)
{
	FILE *fp;

	fp = (FILE *)test_and_set((u_long *)&_file, 0);
	if (fp) {
		fclose(fp);
	}
}

/*
** We allocate a buffer large enough to hold the entire spwd structure.
*/
static int
init_buffer(void)
{
	char *bp;

	bp = (char *)malloc(LOCALBUFLEN);
	if (! __compare_and_swap((long *)&_buffer, 0, (long)bp)) {
		/* Race condition. */
		free(bp);
	}
	return (_buffer) ? 1 : 0;
}

static int
init_entry(void)
{
	struct spwd *ep;

	ep = (struct spwd *)malloc(sizeof(struct spwd));
	if (! __compare_and_swap((long *)&_entry, 0, (long)ep)) {
		/* Race condition. */
		free(ep);
	}
	return (_entry) ? 1 : 0;
}

static char *
spskip(register char *p)
{
	while(*p && *p != ':' && *p != '\n')
		++p ;
	if(*p == '\n')
		*p = '\0' ;
	else if(*p)
		*p++ = '\0' ;
	return(p) ;
}

/*
** This routine will split the source string into a shadow password
** structure using the given structure and buffer.
*/
static struct spwd *
strtospent(struct spwd *se, char *buf)
{
	char *end, *p = buf;
	long x;
	unsigned long y;

	end = strchr(p, '\n');
	if (end) {
		*end = (char)0;
	}
	se->sp_namp = p ;
	p = spskip(p) ;
	se->sp_pwdp = p ;
	p = spskip(p) ;

	x = strtol(p, &end, 10) ;
	if (end != (char *)memchr(p, ':', (int)strlen(p))) {
		/* check for numeric value */
		setoserror(EINVAL);
		return (NULL) ;
	}	
	if (end == p)	
		se->sp_lstchg = -1 ;
	else		
		se->sp_lstchg = x ;
	p = spskip(p) ;
	x = strtol(p, &end, 10) ;	
	if (end != (char *)memchr(p, ':', (int)strlen(p))){
		/* check for numeric value */
		setoserror(EINVAL);
		return (NULL) ;
	} 
	if (end == p)	
		se->sp_min = -1 ;
	else		
		se->sp_min = x ;
	p = spskip(p) ;
	x = strtol(p, &end, 10) ;	
	if ((end != (char *)memchr(p, ':', (int)strlen(p))) &&
	    (*end != (char)0)) {
		/* check for numeric value */
		setoserror(EINVAL);
		return (NULL) ;
	}	
	if (end == p)	
		se->sp_max = -1 ;
	else		
		se->sp_max = x ;
	/* check for old shadow format file */
	if (end == (char *)memchr(p, '\n', (int)strlen(p))) {
		se->sp_warn = -1 ;
		se->sp_inact = -1 ;
		se->sp_expire = -1 ;
		return(se) ;
	}

	p = spskip(p) ;
	x = strtol(p, &end, 10) ;	
	if ((end != (char *)memchr(p, ':', (int)strlen(p)))) {
		/* check for numeric value */
		setoserror(EINVAL);
		return (NULL) ;
	}	
	if (end == p)	
		se->sp_warn = -1 ;
	else		
		se->sp_warn = x ;
	p = spskip(p) ;
	x = strtol(p, &end, 10) ;	
	if ((end != (char *)memchr(p, ':', (int)strlen(p)))) {
		/* check for numeric value */
		setoserror(EINVAL);
		return (NULL) ;
	}	
	if (end == p)	
		se->sp_inact = -1 ;
	else		
		se->sp_inact = x ;
	p = spskip(p) ;
	x = strtol(p, &end, 10) ;	
	if ((end != (char *)memchr(p, ':', (int)strlen(p)))) { 
		/* check for numeric value */
		setoserror(EINVAL);
		return (NULL) ;
	}	
	if (end == p)	
		se->sp_expire = -1 ;
	else		
		se->sp_expire = x ;
	p = spskip(p) ;
	y = strtoul(p, &end, 10) ;	
	if ((end != (char *)memchr(p, ':', (int)strlen(p))) &&
	   (*end != (char)0)) {
		/* check for numeric value */
		setoserror(EINVAL);
		return (NULL) ;
	}	
	if (end == p)	
		se->sp_flag = 0 ;
	else		
		se->sp_flag = y ;

	return(se) ;
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


/*
** This is the reintrant version of getspent.  If we cannot fit the
** data into the given buffer then we continue with the next item.
** There is no way to tell the difference between an end of enumeration
** and an entry which did not fit into the buffer.  This interface is
** man page compatible with the Solaris version.  It would have been
** better if they allowed the return of an error.
*/
struct spwd *
fgetspent_r(FILE *f, struct spwd *entry, char *buffer, int buflen)
{
	if (! entry || ! buffer || ! f) {
		return (struct spwd *)0;
	}

	while (fgets(buffer, buflen, f) != NULL) {
		if (strtospent(entry, buffer)) {
			return entry;
		}
	}

	return (struct spwd *)0;
}

struct spwd *
fgetspent(FILE *f)
{
	if (! f) {
		return (struct spwd *)0;
	}
	if (! _entry && ! init_entry()) {
		return (struct spwd *)0;
	}
	if (! _buffer && ! init_buffer()) {
		return (struct spwd *)0;
	}

	while (fgets(_buffer, LOCALBUFLEN, f) != NULL) {
		if (strtospent(_entry, _buffer)) {
			return _entry;
		}
	}

	return (struct spwd *)0;
}

struct spwd *
getspent_r(struct spwd *entry, char *buffer, int buflen)
{
	struct spwd *result;

	if (! _file) {
		setspent_internal();
	}

	result = fgetspent_r(_file, entry, buffer, buflen);

	/*
	** It is possible for the dynamic file to timeout in the middle
	** of reading through the file, so if we get an error then we
	** start over.
	*/
	if (! result && _file && ferror(_file)) {
		endspent();
		setspent_internal();
		result = fgetspent_r(_file, entry, buffer, buflen);
	}

	return result;
}

struct spwd *
getspent(void)
{
	struct spwd *result;

	if (! _file) {
		setspent_internal();
	}

	result = fgetspent(_file);

	/*
	** It is possible for the dynamic file to timeout in the middle
	** of reading through the file, so if we get an error then we
	** start over.
	*/
	if (! result && _file && ferror(_file)) {
		endspent();
		setspent_internal();
		result = fgetspent(_file);
	}

	return result;
}

struct spwd *
__pwshadow(const char *name, struct spwd *entry, char *buffer, int buflen, int flags)
{
	int status;
	int accessfailed=0;
	struct xpasswd pw;
	struct spwd *ent = NULL;
	struct passwd *pwd;
	FILE *fp;
	struct stat sbuf;

	if (! name || ! *name || ! entry || ! buffer) {
		return (struct spwd *)0;
	}

	if (! _getXbyY_no_stat && stat(LOCALFILE, &sbuf) == 0) {
		_map_byname.m_version = sbuf.st_mtim.tv_sec;
	} else {
		accessfailed=1;
	}

	if (_getpwent_no_yp) {
		/* 
		** if no yp is used, fall thru to the local file
		*/
		status = NS_SUCCESS;
	} else {
		status = ns_lookup(&_map_byname, 0, "shadow.byname", name, 0,
				   buffer, buflen);
		if (status == NS_SUCCESS) {
			if (strtospent(entry, buffer)) {
				return entry;
			}
			return (struct spwd *)0;
		}
	}

	if (! accessfailed && status != NS_NOTFOUND) {
		fp = fopen(LOCALFILE, "r");
		if (fp) {
			while ((ent = fgetspent_r(fp, entry, buffer, buflen)) &&
			       strcmp(name, ent->sp_namp));
			fclose(fp);
		}
	}

	/*
	** Fake a shadow entry from passwd file.
	*/
	if (! ent && (flags & SHADOW_PW)) {
		__pwnam(name, &pw, buffer, buflen, &pwd, 0);
		if (! pwd) {
			return (struct spwd *)0;
		}
		pwd_to_spwd(pwd, entry);
		return entry;
	}

	return ent;
}

struct spwd *
getspnam_r(const char *name, struct spwd *entry, char *buffer, int buflen) {

	return __pwshadow(name, entry, buffer, buflen, SHADOW_PW);
	
}

struct spwd *
getspnam(const char *name)
{
	if (! name || ! *name) {
		return (struct spwd *)0;
	}
	if (! _entry && ! init_entry()) {
		return (struct spwd *)0;
	}
	if (! _buffer && ! init_buffer()) {
		return (struct spwd *)0;
	}

	return getspnam_r(name, _entry, _buffer, LOCALBUFLEN);
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
