#ifdef __STDC__
	#pragma weak getpwnam_r = _getpwnam_r
	#pragma weak getpwnam = _getpwnam
	#pragma weak getpwuid_r = _getpwuid_r
	#pragma weak getpwuid = _getpwuid
	#pragma weak getpwent_r = _getpwent_r
	#pragma weak getpwent = _getpwent
	#pragma weak fgetpwent_r = _fgetpwent_r
	#pragma weak fgetpwent = _fgetpwent
	#pragma weak putpwent = _putpwent
	#pragma weak setpwent = _setpwent
	#pragma weak endpwent = _endpwent
	#pragma weak _yp_setpwent = __yp_setpwent
	#pragma weak _yp_getpwent = __yp_getpwent
#endif

#include "synonyms.h"
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/param.h>
#define _SGI_REENTRANT_FUNCTIONS
#include <string.h>
#include <mutex.h>
#include <pwd.h>
#include <shadow.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <ns_api.h>
#include "compare.h"

#define LOCALBUFLEN	4096
#define LOCALFILE	"/etc/passwd"
#define LISTMAP		"passwd"
#define PW_SHADOW	1
#define SHADOW_PW	2

static FILE *_file = (FILE *)0;
static struct xpasswd *_entry _INITBSS;
static char *_buffer _INITBSS;

static ns_map_t _map_byname _INITBSSS;
static ns_map_t _map_byuid _INITBSSS;

int _getpwent_no_yp _INITBSS;
int _getpwent_no_shadow _INITBSS;
int _getpwent_no_ssdi _INITBSS;
int _pw_stayopen _INITBSS;
extern int _getXbyY_no_stat;

extern int _ltoa(long, char *);
extern struct spwd * __pwshadow(const char *, struct spwd *, char *, int, int);

void
_yp_setpwent(void)
{
	FILE *fp;

	if (_file) {
		fclose(_file);
	}
	fp = ns_list(0, 0, LISTMAP, "nis");
	if (! __compare_and_swap((long *)&_file, 0, (long)fp)) {
		/* Race condition. */
		fclose(fp);
	}
}

void
setpwent(void)
{
	if (_file) {
		rewind(_file);
	}
}

void
endpwent(void)
{
	FILE *fp;

	fp = (FILE *)test_and_set((u_long *)&_file, 0);
	if (fp) {
		fclose(fp);
	}
}

/*
** We allocate a buffer large enough to hold the entire passwd structure.
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
	struct passwd *ep;

	ep = (struct passwd *)malloc(sizeof(struct xpasswd));
	if (! __compare_and_swap((long *)&_entry, 0, (long)ep)) {
		/* Race condition. */
		free(ep);
	}
	return (_entry) ? 1 : 0;
}

/*
** We assume that a passwd entry is of the form:
**	pw_name:pw_passwd,pw_age:pw_uid:pw_gid:pw_gecos:pw_dir:pw_shell
** pw_comment is always null.
**
** The first four fields name, passwd, uid and gid are required, the
** rest will be filled in by login if they do not exist.
*/
static int
strtopwent(struct xpasswd *pwd, char *buffer, uid_t *uidp)
{
	register char *p;
	char *end;
	uid_t x;
	gid_t y;
	int forward;

	pwd->pw_age = pwd->pw_comment = pwd->pw_gecos = pwd->pw_dir =
	    pwd->pw_shell = "";
	pwd->pw_yp_netgroup = (char *)0;

	/*
	** We cannot determine the origin the same way we used to
	** so now we just assume it is remote unless we are only
	** looking at the local file.
	*/
	pwd->pw_origin = (_getpwent_no_yp) ? _PW_LOCAL : _PW_YP_REMOTE;

	/* skip leading white space and comments */
	for (p = buffer; p && *p && isspace(*p); p++);
	if (*p == '#') {
		return 0;
	}

	/*
	** We only return the +/- entries if the getpwent_no_yp
	** flag is set.  This is for programs which explicitely
	** want to walk through the file and find all the remote
	** entries, etc.
	*/
	forward = (*p == '+' || *p == '-') ? 1 : 0;
	if (! _getpwent_no_yp && forward) {
		return 0;
	}

	/* pw_name */
	pwd->pw_name = p;
	for (; *p && *p != ':' && *p != '\n'; p++);
	if (*p) {
		*p++ = (char)0;
	}
	if (! *pwd->pw_name) {
		setoserror(EINVAL);
		return 0;
	}

	/* pw_passwd */
	pwd->pw_yp_passwd = pwd->pw_passwd = p;
	for (; *p && *p != ':' && *p != '\n'; p++);
	if (*p) {
		*p++ = (char)0;
	}

	/* pw_uid */
	if (! forward && (! *p || *p == ':')) {
		/* check for non-null uid */
		setoserror(EINVAL);
		return 0;
	}
	x = (uid_t)strtol(p, &end, 10);	
	pwd->pw_uid = (x < 0 || x > MAXUID) ? (UID_NOBODY): x;
	for (; *p && *p != ':' && *p != '\n'; p++);
	if (end != p && ! forward) {
		/* check for numeric value */
		setoserror(EINVAL);
		return 0;
	}
	if (*p) {
		p++;
	}

	/*
	** performance tweak for getpwuid
	*/
	if (uidp && *uidp != pwd->pw_uid) {
		return 0;
	}

	/* pw_gid */
	if (! forward && (! *p || *p == ':')) {
		/* check for non-null gid */
		setoserror(EINVAL);
		return 0;
	}
	y = (gid_t)strtol(p, &end, 10);	
	pwd->pw_gid = (y < 0 || y > MAXUID) ? (UID_NOBODY): y;
	for (; *p && *p != ':' && *p != '\n'; p++);
	if (end != p && ! forward) {
		/* check for numeric value */
		setoserror(EINVAL);
		return 0;
	}
	if (*p) {
		*p++ = (char)0;
	}

	/* pw_coment and pw_gecos */
	pwd->pw_comment = pwd->pw_yp_gecos = pwd->pw_gecos = p;
	for (; *p && *p != ':' && *p != '\n'; p++);
	if (*p) {
		*p++ = (char)0;
	}

	/* pw_dir */
	pwd->pw_yp_dir = pwd->pw_dir = p;
	for (; *p && *p != ':' && *p != '\n'; p++);
	if (*p) {
		*p++ = (char)0;
	}

	/* pw_shell */
	pwd->pw_yp_shell = pwd->pw_shell = p;
	for (; *p && *p != ':' && ! isspace(*p); p++);
	*p = (char)0;

	/* pw_age */
	for (p = pwd->pw_passwd; *p && *p != ','; p++);
	if (*p) {
		*p++ = '\0';
	}
	pwd->pw_age = p;

	return 1;
}

/*
** This routine is historical.  I believe the only program to ever use it
** was finger, but since I cannot verify that this symbol is still here.
*/
/* ARGSUSED */
struct xpasswd *
_pw_interpret(char *val, int len, FILE *file)
{
	int c;

	if (! _entry && ! init_entry()) {
		return (struct xpasswd *)0;
	}
	if (! _buffer && ! init_buffer()) {
		return (struct xpasswd *)0;
	}
	strncpy(_buffer, val, LOCALBUFLEN);
	_buffer[LOCALBUFLEN - 1] = (char)0;

	if (! strtopwent(_entry, _buffer, 0)) {
		if (file) {
			while((c = getc(file)) != (int)'\n' && c != EOF)
				;
		}
		return (struct xpasswd *)0;
	}

	return (struct xpasswd *)_entry;
}

/*
** This routine overlays password and age from the shadow file onto
** a password entry.
*/
static int
pwshadow(struct xpasswd *pe, char *buf, int len, struct spwd *spe)
{
	time_t age;
	char *ages;
	int nlen, dlen;

	/* Convert the shadow style age data to base64 passwd style. */
	age = (time_t)(spe->sp_max + (spe->sp_min << 6) +
	    (spe->sp_lstchg << 12));
	ages = l64a(age);

	/* Append encrypted password data. */
	dlen = (int)strlen(spe->sp_pwdp) + 1;
	if (dlen > len) {
		return 0;
	}
	strcpy(buf, spe->sp_pwdp);
	pe->pw_passwd = buf;
	nlen = dlen;

	/* Append age. */
	dlen = (int)strlen(ages) + 1;
	if ((dlen + nlen) > len) {
		return 0;
	}
	strcpy(&buf[nlen], ages);
	pe->pw_age = &buf[nlen];
	nlen += dlen;

	return 1;
}

/*
** This is the reintrant version of getpwent.  If we cannot fit the
** data into the given buffer then we continue with the next item.
** There is no way to tell the difference between an end of enumeration
** and an entry which did not fit into the buffer.  This interface is
** man page compatible with the Solaris version.  It would have been
** better if they allowed the return of an error.
*/
static struct xpasswd *
__pwent(FILE *f, struct xpasswd *entry, char *buffer, size_t buflen)
{
	if (! entry || ! buffer || ! f) {
		return (struct xpasswd *)0;
	}

	while (fgets(buffer, (int)buflen, f) != NULL) {
		if (strtopwent(entry, buffer, 0)) {
			return entry;
		}
	}

	return (struct xpasswd *)0;
}

struct passwd *
fgetpwent_r(FILE *f, struct passwd *entry, char *buffer, size_t buflen)
{
	struct xpasswd xpw, *xpwp;

	xpwp = __pwent(f, &xpw, buffer, buflen);
	if (xpwp) {
		memcpy(entry, &xpw, sizeof(*entry));
		return entry;
	}

	return (struct passwd *)0;
}

struct passwd *
fgetpwent(FILE *f)
{
	if (! f) {
		return (struct passwd *)0;
	}
	if (! _entry && ! init_entry()) {
		return (struct passwd *)0;
	}
	if (! _buffer && ! init_buffer()) {
		return (struct passwd *)0;
	}

	while (fgets(_buffer, LOCALBUFLEN, f) != NULL) {
		if (strtopwent(_entry, _buffer, 0)) {
			return (struct passwd *)_entry;
		}
	}

	return (struct passwd *)0;
}

struct passwd *
getpwent_r(struct passwd *entry, char *buffer, size_t buflen)
{
	struct passwd *result;
	FILE *fp=0;

	if (! _file) {
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

	result = fgetpwent_r(_file, entry, buffer, buflen);

	/*
	** It is possible for the dynamic file to be timed out while
	** we are still reading.  If we get a read error we just restart.
	*/
	if (! result && _file && ferror(_file)) {
		endpwent();
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
		result = fgetpwent_r(_file, entry, buffer, buflen);
	}

	return result;
}

struct passwd *
getpwent(void)
{
	struct passwd *result;
	FILE *fp=0;

	if (! _file) {
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

	result = fgetpwent(_file);

	/*
	** It is possible for the dynamic file to be timed out while
	** we are still reading.  If we get a read error we just restart.
	*/
	if (! result && _file && ferror(_file)) {
		endpwent();
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
		result = fgetpwent(_file);
	}

	return result;
}

/*
** This is not correct.  It simply exists to deal with the any applications
** that were built using the old interface.
*/
struct xpasswd *
_yp_getpwent(void)
{
	if (! _file) {
		_yp_setpwent();
	}
	if (! _buffer && ! init_buffer()) {
		return (struct xpasswd *)0;
	}
	if (! _entry && ! init_entry()) {
		return (struct xpasswd *)0;
	}

	return (struct xpasswd *)__pwent(_file, _entry, _buffer, LOCALBUFLEN);
}

/*
** This is the primary getpwnam routine which everything ends up calling.
** It is not an exported routine, but it is used in ns_shadow.c.  It was
** added for the flags parameter which tells us whether to call getspnam()
** or not.
*/
int
__pwnam(const char *name, struct xpasswd *entry, char *buffer, size_t buflen,
    struct xpasswd **pwd, unsigned flags)
{
	int status, accessfailed=0;
	size_t len;
	char *p;
	struct spwd spe;
	struct xpasswd *ent = NULL;
	FILE *fp;
	struct stat sbuf;

	if (! name || ! entry || ! buffer) {
		*pwd = (struct xpasswd *)0;
		return EINVAL;
	}
	if (buflen < 80) {
		*pwd = (struct xpasswd *)0;
		return ERANGE;
	}

	if (_getpwent_no_yp) {
		if ((fp = fopen(LOCALFILE, "r")) == NULL) {
			accessfailed=1;
		} else {
			while ((ent = __pwent(fp, entry, buffer, buflen)) 
			       && strcmp(name, ent->pw_name));
			fclose(fp);
		}
	} else {
		ent = entry;
		if (! _getXbyY_no_stat && stat(LOCALFILE, &sbuf) == 0) {
			_map_byname.m_version = sbuf.st_mtim.tv_sec;
		} else {
			accessfailed=1;
		}
		status = ns_lookup(&_map_byname, 0, "passwd.byname", name, 0,
		    buffer, (int)buflen);
		if (status != NS_SUCCESS) {
			if (status == NS_NOTFOUND || accessfailed) {
				*pwd = (struct xpasswd *)0;
				return 0;
			}

			/*
			** Fallback to local files.
			*/
			if ((fp = fopen(LOCALFILE, "r")) == NULL) {
				*pwd = (struct xpasswd *)0;
				return 0;
			}

			ent = (struct xpasswd *)0;
			status = (int)strlen(name);
			while (fgets(buffer, (int)buflen, fp)) {
				for (p = buffer; *p && isspace(*p); p++);
				if (strncmp(name, p, status) == 0) {
					for (; *p && *p != '\n'; p++);
					if ((! *p || *p != '\n') &&
					    ! feof(fp)) {
						*pwd = (struct xpasswd *)0;
						return ERANGE;
					}
					if (strtopwent(entry, buffer, 0) &&
					  (strcmp(name, entry->pw_name) == 0)) {
						ent = entry;
						ent->pw_origin = _PW_LOCAL;
						break;
					}
				}
			}
			fclose(fp);
		} else {
			if (! strtopwent(entry, buffer, 0)) {
				*pwd = (struct xpasswd *)0;
				return ERANGE;
			}
		}
	}

	/*
	** Add shadow password information.
	*/
	if (ent && ! _getpwent_no_shadow) {
		/*
		** Use the space at the end of our buffer for shadow data.
		*/
		for (p = entry->pw_shell; *p; p++);
		len = buflen - (++p - buffer);

		if ((flags & PW_SHADOW) &&
		    __pwshadow(entry->pw_name, &spe, p, (int)len, 0)) {

			/*
			** Again, move to end of buffer.
			*/
			for (p = spe.sp_pwdp; *p; p++);
			len = buflen - (++p - buffer);

			if (! pwshadow(entry, p, (int)len, &spe)) {
				*pwd = (struct xpasswd *)0;
				return ERANGE;
			}
		}
	}

	*pwd = (struct xpasswd *)ent;
	return 0;
}

int
getpwnam_r(const char *name, struct passwd *entry, char *buffer, size_t buflen,
    struct passwd **pwd)
{
	struct xpasswd xpw, *xpwp;
	int status;

	status = __pwnam(name, &xpw, buffer, buflen, &xpwp, PW_SHADOW);
	if (xpwp) {
		memcpy(entry, &xpw, sizeof(*entry));
		*pwd = entry;
	} else {
		*pwd = 0;
	}

	return status;
}

struct passwd *
getpwnam(const char *name)
{
	struct xpasswd *result;

	if (! name || ! *name) {
		return (struct passwd *)0;
	}
	if (! _entry && ! init_entry()) {
		return (struct passwd *)0;
	}
	if (! _buffer && ! init_buffer()) {
		return (struct passwd *)0;
	}

	__pwnam(name, _entry, _buffer, LOCALBUFLEN, &result, PW_SHADOW);

	return (struct passwd *)result;
}

/*
** This is the base routine for getpwuid*.
*/
static int
__pwuid(uid_t uid, struct xpasswd *entry, char *buffer, size_t buflen,
    struct xpasswd **pwd)
{
	int status, accessfailed=0;
	size_t len;
	char key[16], *p;
	struct spwd spe;
	struct xpasswd *ent = NULL;
	FILE *fp;
	struct stat sbuf;

	if (! entry || ! buffer) {
		*pwd = (struct xpasswd *)0;
		return EINVAL;
	} 
	if (buflen < 80) {
		*pwd = (struct xpasswd *)0;
		return ERANGE;
	}

	if (_getpwent_no_yp) {
		if ((fp = fopen(LOCALFILE, "r")) == NULL) {
			accessfailed=1;
		} else {
			while ((ent = __pwent(fp, entry, buffer, buflen)) 
			       && (uid != ent->pw_uid));
			fclose(fp);
		}
	} else {
		ent = entry;
		_ltoa((long)uid, key);
		if (! _getXbyY_no_stat && stat(LOCALFILE, &sbuf) == 0) {
			_map_byuid.m_version = sbuf.st_mtim.tv_sec;
		} else {
			accessfailed=1;
		}
		status = ns_lookup(&_map_byuid, 0, "passwd.byuid", key, 0,
		    buffer, (int)buflen);
		if (status != NS_SUCCESS) {
			if (status == NS_NOTFOUND || accessfailed) {
				*pwd = (struct xpasswd *)0;
				return 0;
			}

			/*
			** Fallback to local files.
			*/
			if ((fp = fopen(LOCALFILE, "r")) == NULL) {
				*pwd = (struct xpasswd *)0;
				return 0;
			}
			ent = (struct xpasswd *)0;
			while (fgets(buffer, (int)buflen, fp)) {
				if (! strchr(buffer, '\n') && ! feof(fp)) {
					*pwd = (struct xpasswd *)0;
					return ERANGE;
				}
				if (strtopwent(entry, buffer, &uid)) {
					ent = entry;
					ent->pw_origin = _PW_LOCAL;
					break;
				}
			}
			fclose(fp);
		} else if (! strtopwent(entry, buffer, 0)) {
			*pwd = (struct xpasswd *)0;
			return ERANGE;
		}
	}

	/*
	** Add shadow password information.
	*/
	if (ent && ! _getpwent_no_shadow) {
		/*
		** Use the space at the end of our buffer for shadow data.
		*/
		for (p = entry->pw_shell; *p; p++);
		len = buflen - (++p - buffer);

		if (__pwshadow(entry->pw_name, &spe, p, (int)len, 0)) {
			/*
			** Again, move to end of buffer.
			*/
			for (p = spe.sp_pwdp; *p; p++);
			len = buflen - (++p - buffer);

			if (! pwshadow(entry, p, (int)len, &spe)) {
				*pwd = (struct xpasswd *)0;
				return ERANGE;
			}
		}
	}

	*pwd = ent;
	return 0;
}

int
getpwuid_r(uid_t uid, struct passwd *entry, char *buffer, size_t buflen,
    struct passwd **pwd)
{
	struct xpasswd xpw, *xpwp;
	int status;

	if (! entry || ! buffer || ! pwd) {
		return EINVAL;
	}

	status = __pwuid(uid, &xpw, buffer, buflen, &xpwp);
	if (xpwp) {
		memcpy(entry, xpwp, sizeof(*entry));
		*pwd = entry;
	} else {
		*pwd = 0;
	}

	return status;
}

struct passwd *
getpwuid(uid_t uid)
{
	struct xpasswd *result;

	if (! _entry && ! init_entry()) {
		return (struct passwd *)0;
	}
	if (! _buffer && ! init_buffer()) {
		return (struct passwd *)0;
	}

	__pwuid(uid, _entry, _buffer, LOCALBUFLEN, &result);
	return (struct passwd *)result;
}

int
putpwent(const struct passwd *p, FILE *f)
{
	if (! p) {
		return (-1);
	}
	(void) fprintf(f, "%s:%s", p->pw_name, p->pw_passwd);
	if(p->pw_age && ((*p->pw_age) != '\0'))
		(void) fprintf(f, ",%s", p->pw_age);
	(void) fprintf(f, ":%d:%d:%s:%s:%s",
		/* pass thru -2 for NFS */
		((p->pw_uid == (uid_t)-2) ? -2 : p->pw_uid),
		((p->pw_gid == (gid_t)-2) ? -2 : p->pw_gid),
		p->pw_gecos,
		p->pw_dir,
		p->pw_shell);
	(void) putc('\n', f);
	(void) fflush(f);
	return(ferror(f));
}
