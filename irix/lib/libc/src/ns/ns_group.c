#ifdef __STDC__
	#pragma weak getgrnam_r = _getgrnam_r
	#pragma weak getgrnam = _getgrnam
	#pragma weak getgrgid_r = _getgrgid_r
	#pragma weak getgrgid = _getgrgid
	#pragma weak getgrent_r = _getgrent_r
	#pragma weak getgrent = _getgrent
	#pragma weak setgrent = _setgrent
	#pragma weak endgrent = _endgrent
	#pragma weak fgetgrent_r = _fgetgrent_r
	#pragma weak fgetgrent = _fgetgrent
	#pragma weak getgrmember = _getgrmember
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
#include <grp.h>
#include <errno.h>
#include <ns_api.h>
#include "compare.h"

#define LOCALBUFLEN	16384
#define LOCALFILE	"/etc/group"
#define LISTMAP		"group"

static FILE *_file = (FILE *)0;
static struct group *_entry _INITBSS;
static char *_buffer _INITBSS;

static ns_map_t _map_byname _INITBSSS;
static ns_map_t _map_bygid _INITBSSS;
static ns_map_t _map_bymember _INITBSSS;

extern int _getXbyY_no_stat;

extern int _getpwent_no_yp;
extern int _ltoa(long, char *);

void
setgrent(void)
{
	if (_file) {
		rewind(_file);
	}
}


static void
setgrent_internal(void)
{
	FILE *fp = 0;

	setgrent();
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
endgrent(void)
{
	FILE *fp;

	fp = (FILE *)test_and_set((u_long *)&_file, 0);
	if (fp) {
		fclose(fp);
	}
}

/*
** We allocate a buffer large enough to hold the entire group structure.
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
	struct group *ep;

	ep = (struct group *)malloc(sizeof(struct group));
	if (! __compare_and_swap((long *)&_entry, 0, (long)ep)) {
		/* Race condition. */
		free(ep);
	}
	return (_entry) ? 1 : 0;
}

/*
** This routine will split the data in the src buffer into the
** result group structure and the supplied buffer.
*/
static int
strtogrent(struct group *entry, char *buf, int len)
{
	char *p, *q, *r;
	char **mem;
	int nlen;

	/*
	** We stick the member pointers at the end of the buffer if there
	** is space.  We find the end of the data and pad out until we are
	** nicely aligned.
	*/
	nlen = (int)strlen(buf) + 1;
	nlen += (sizeof(char **) - (nlen % sizeof(char **)));
	entry->gr_mem = mem = (char **)&buf[nlen];

	/*
	** Skip whitespace and comments.
	*/
	for (p = buf; p && *p && isspace(*p); p++);
	if (*p == '#') {
		return 0;
	}

	/*
	** We assume that a group entry is of the form:
	**	gr_name:gr_passwd:gr_gid:[member,]*
	*/
	p = strchr(buf, '\n');
	if (p) {
		*p = (char)0;
	}
	for (p = buf; *p && isspace(*p); p++);
	if (! *p) {
		return 0;
	}
	for (q = p; *q && (*q != ':'); q++);
	if (! *q) {
		return 0;
	}
	*q++ = (char)0;
	entry->gr_name = p;

	/* Pull out the passwd. */
	for (p = q; *p && (*p != ':'); p++);
	if (! *p) {
		return 0;
	}
	*p++ = (char)0;
	entry->gr_passwd = q;

	/* Get the gid. */
	for (q = p; *q && (*q != ':'); q++);
	if (! *q) {
		return 0;
	}
	*q++ = (char)0;
	entry->gr_gid = (gid_t)strtol(p, &r, 10);
	if (p == r) {
		return 0;
	}

	/* Loop through member list. */
	r = (char *)0;
	while (p = strtok_r(q, " \t,", &r)) {
		q = (char *)0;
		if (&mem[1] > (char **)&buf[len]) {
			/* Not enough space. */
			return 0;
		}
		*mem++ = p;
	}
	*mem = (char *)0;

	return 1;
}

/*
** This is the reintrant version of getgrent.  If we cannot fit the
** data into the given buffer then we continue with the next item.
** There is no way to tell the difference between an end of enumeration
** and an entry which did not fit into the buffer.  This interface is
** man page compatible with the Solaris version.  It would have been
** better if they allowed the return of an error.
*/
struct group *
fgetgrent_r(FILE *f, struct group *entry, char *buffer, size_t buflen)
{
	if (! entry || ! buffer || ! f) {
		return (struct group *)0;
	}

	while (fgets(buffer, (int)buflen, f) != NULL) {
		if (strtogrent(entry, buffer, (int)buflen)) {
			return entry;
		}
	}

	return (struct group *)0;
}

struct group *
fgetgrent(FILE *f)
{
	if (! f) {
		return (struct group *)0;
	}
	if (! _entry && ! init_entry()) {
		return (struct group *)0;
	}
	if (! _buffer && ! init_buffer()) {
		return (struct group *)0;
	}

	while (fgets(_buffer, LOCALBUFLEN, f) != NULL) {
		if (strtogrent(_entry, _buffer, LOCALBUFLEN)) {
			return _entry;
		}
	}

	return (struct group *)0;
}

/*
** This will read the standard group file and return the results one line
** at a time.  If we get an error in the middle of reading the file we
** start over at the beginning.
*/
struct group *
getgrent_r(struct group *entry, char *buffer, size_t buflen)
{
	struct group *result;

	if (! _file) {
		setgrent_internal();
	}

	result = fgetgrent_r(_file, entry, buffer, buflen);

	/*
	** It is possible for the dynamic file to timeout in the middle of
	** reading so if we get a read error then we restart.
	*/
	if (! result && _file && ferror(_file)) {
		endgrent();
		setgrent_internal();
		result = fgetgrent_r(_file, entry, buffer, buflen);
	}

	return result;
}

struct group *
getgrent(void)
{
	struct group *result;

	if (! _file) {
		setgrent_internal();
	}

	result = fgetgrent(_file);

	/*
	** It is possible for the dynamic file to timeout in the middle of
	** reading so if we get a read error then we restart.
	*/
	if (! result && _file && ferror(_file)) {
		endgrent();
		setgrent_internal();
		result = fgetgrent(_file);
	}

	return result;
}

int
getgrnam_r(const char *name, struct group *entry, char *buffer, size_t buflen,
    struct group **grp)
{
	int status;
	int accessfailed=0;
	struct group *ent = NULL;
	FILE *fp;
	struct stat sbuf;

	*grp = (struct group *)0;
	if (! name || ! entry || ! buffer) {
		return EINVAL;
	}
	if (buflen < 80) {
		return ERANGE;
	}

	if (! _getXbyY_no_stat && stat(LOCALFILE, &sbuf) == 0) {
		_map_byname.m_version = sbuf.st_mtim.tv_sec;
	} else {
		accessfailed=1;
	}

	status = ns_lookup(&_map_byname, 0, "group.byname", name, 0,
	    buffer, (int)buflen);
	if (status == NS_SUCCESS) {
		if (strtogrent(entry, buffer, (int)buflen)) {
			*grp = entry;
			return 0;
		}
		return ERANGE;
	}

	if (status == NS_NOTFOUND || accessfailed) {
		return 0;
	}

	/*
	** Fallback to local files.
	*/
	fp = fopen(LOCALFILE, "r");
	if (fp) {
		while (((ent = fgetgrent_r(fp, entry, buffer, buflen)) != NULL)
		       && strcmp(name, ent->gr_name));
		fclose(fp);
	}

	*grp = ent;
	return 0;
}

struct group *
getgrnam(const char *name)
{
	struct group *result;

	if (! name || ! *name) {
		return (struct group *)0;
	}
	if (! _entry && ! init_entry()) {
		return (struct group *)0;
	}
	if (! _buffer && ! init_buffer()) {
		return (struct group *)0;
	}

	getgrnam_r(name, _entry, _buffer, LOCALBUFLEN, &result);
	return result;
}

int
getgrgid_r(gid_t gid, struct group *entry, char *buffer, size_t buflen,
    struct group **grp)
{
	int status;
	int accessfailed=0;
	char key[16];
	struct group *ent = NULL;
	FILE *fp;
	struct stat sbuf;

	*grp = (struct group *)0;
	if (! entry || ! buffer) {
		return EINVAL;
	} 
	if (buflen < 80) {
		return ERANGE;
	}

	_ltoa((long)gid, key);

	if (! _getXbyY_no_stat && stat(LOCALFILE, &sbuf) == 0) {
		_map_bygid.m_version = sbuf.st_mtim.tv_sec;
	} else {
		accessfailed=1;
	}

	status = ns_lookup(&_map_bygid, 0, "group.bygid", key, 0,
	    buffer, (int)buflen);
	if (status == NS_SUCCESS) {
		if (strtogrent(entry, buffer, (int)buflen)) {
			*grp = entry;
			return 0;
		}
		return ERANGE;
	}

	if (status == NS_NOTFOUND || accessfailed) {
		return 0;
	}

	/*
	** Fallback to local files.
	*/
	fp = fopen(LOCALFILE, "r");
	if (fp) {
		while (((ent = fgetgrent_r(fp, entry, buffer, buflen)) != NULL)
		       && (gid != ent->gr_gid));
		fclose(fp);
	}
	*grp = ent;
	return 0;
}

struct group *
getgrgid(gid_t gid)
{
	struct group *result;

	if (! _entry && ! init_entry()) {
		return (struct group *)0;
	}
	if (! _buffer && ! init_buffer()) {
		return (struct group *)0;
	}

	getgrgid_r(gid, _entry, _buffer, LOCALBUFLEN, &result);
	return result;
}

int
getgrmember(const char *name, gid_t gids[], int maxgids, int ngids)
{
	char *p, *q, *r, buffer[BUFSIZ];
	int status, i;
	gid_t gid;
	struct stat sbuf;

	if (! _getXbyY_no_stat && stat(LOCALFILE, &sbuf) == 0) {
		_map_bymember.m_version = sbuf.st_mtim.tv_sec;
	}
	status = ns_lookup(&_map_bymember, 0, "group.bymember", name, 0,
	    buffer, BUFSIZ);
	if (status != NS_SUCCESS) {
		return -1;
	}

	/*
	** The returning data should be of the format:
	**	name: gid, gid, . . .
	*/
	p = buffer;
	p = strchr(p, ':');
	if (p) {
		p++;
		r = 0;
		while ((q = strtok_r(p, ", \t\n", &r)) && (ngids < maxgids)) {
			gid = (gid_t)strtol(q, &p, 10);
			if (p == q) {
				continue;
			}
			p = (char *)0;

			/* Check that this group is not already in the list. */
			for (i = 0; (i < ngids) && (gids[i] != gid); i++);

			if (i == ngids) {
				gids[ngids++] = gid;
			}
		}
	}

	return ngids;
}
