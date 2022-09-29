#ifdef __STDC__
	#pragma weak getprotobyname_r = _getprotobyname_r
	#pragma weak getprotobyname = _getprotobyname
	#pragma weak getprotobynumber_r = _getprotobynumber_r
	#pragma weak getprotobynumber = _getprotobynumber
	#pragma weak getprotoent_r = _getprotoent_r
	#pragma weak getprotoent = _getprotoent
	#pragma weak setprotoent = _setprotoent
	#pragma weak endprotoent = _endprotoent
	#pragma weak fgetprotoent_r = _fgetprotoent_r
	#pragma weak fgetprotoent = _fgetprotoent
#endif

#include "synonyms.h"
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#define _SGI_REENTRANT_FUNCTIONS
#include <string.h>
#include <mutex.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>
#include <ns_api.h>
#include "compare.h"

#define LOCALBUFLEN	8192
#define LOCALFILE	"/etc/protocols"
#define LISTMAP		"protocols"

static FILE *_file = (FILE *)0;
static struct protoent *_entry _INITBSS;
static char *_buffer _INITBSS;

static ns_map_t _map_byname _INITBSSS;
static ns_map_t _map_bynumber _INITBSSS;

extern int _getXbyY_no_stat;

extern int _ltoa(long, char *);

void
setprotoent(int s)
{
	if (_file) {
		rewind(_file);
	}
	_map_byname.m_stayopen |= s;
	_map_bynumber.m_stayopen |= s;
}

static int
setprotoent_files(int s)
{
	FILE *fp;

	fp = (FILE *)test_and_set((u_long *)&_file, 0);
	if (fp) {
		fclose(fp);
	}

	fp = fopen(LOCALFILE, "r");
	if (! __compare_and_swap((long *)&_file, 0, (long)fp)) {
		/* Race condition. */
		fclose(fp);
		fp = _file;
	}

	_map_byname.m_stayopen |= s;
	_map_bynumber.m_stayopen |= s;

	return(fp?1:0);
}

static void
setprotoent_internal(int s)
{
	FILE *fp;

	setprotoent(s);
	if (!_file) {
		fp = ns_list(0, 0, LISTMAP, 0);
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
endprotoent(void)
{
	FILE *fp;

	fp = (FILE *)test_and_set((u_long *)&_file, 0);
	if (fp) {
		fclose(fp);
	}

	_map_byname.m_stayopen = 0;
	_map_bynumber.m_stayopen = 0;
}

/*
** We allocate a buffer large enough to hold the entire protoent structure.
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
	struct protoent *ep;

	ep = (struct protoent *)malloc(sizeof(struct protoent));
	if (! __compare_and_swap((long *)&_entry, 0, (long)ep)) {
		/* Race condition. */
		free(ep);
	}
	return (_entry) ? 1 : 0;
}

/*
** This routine will split the data in the src buffer into the
** result protoent structure and the supplied buffer.
*/
static int
strtoprotoent(struct protoent *entry, char *buf, int len)
{
	char *p, *q, *r, **index, **end;
	int total;

	/*
	** We place the array of pointers after the null in buf.
	*/
	total = (int)strlen(buf);
	p = buf + total + (sizeof(*index) - (total % sizeof(*index)));
	entry->p_aliases = index = (char **)p;
	p = buf + len - (len % sizeof(*index));
	end = (char **)p;

	p = buf;
	q = strpbrk(p, "\n#");
	if (q) {
		*q = (char)0;
	}
	r = (char *)0;
	q = strtok_r(p, " \t\n", &r);
	if (! q) {
		return 0;
	}
	entry->p_name = q;

	p = (char *)0;
	q = strtok_r(p, " \t\n", &r);
	if (! q) {
		return 0;
	}
	entry->p_proto = atoi(q);

	while (q = strtok_r(p, " \t\n", &r)) {
		if (index >= end) {
			return 0;
		}
		*index++ = q;
	}
	if (index >= end) {
		return 0;
	}
	*index = (char *)0;

	return 1;
}

/*
** This is the reintrant version of getprotoent.  If we cannot fit the
** data into the given buffer then we continue with the next item.
** There is no way to tell the difference between an end of enumeration
** and an entry which did not fit into the buffer.  This interface is
** man page compatible with the Solaris version.  It would have been
** better if they allowed the return of an error.
*/
struct protoent *
fgetprotoent_r(FILE *f, struct protoent *entry, char *buffer, int buflen)
{
	if (! entry || ! buffer || ! f) {
		return (struct protoent *)0;
	}

	while (fgets(buffer, buflen, f) != NULL) {
		if (strtoprotoent(entry, buffer, buflen)) {
			return entry;
		}
	}

	return (struct protoent *)0;
}

struct protoent *
fgetprotoent(FILE *f)
{
	if (! f) {
		return (struct protoent *)0;
	}
	if (! _entry && ! init_entry()) {
		return (struct protoent *)0;
	}
	if (! _buffer && ! init_buffer()) {
		return (struct protoent *)0;
	}

	while (fgets(_buffer, LOCALBUFLEN, f) != NULL) {
		if (strtoprotoent(_entry, _buffer, LOCALBUFLEN)) {
			return _entry;
		}
	}

	return (struct protoent *)0;
}

struct protoent *
getprotoent_r(struct protoent *entry, char *buffer, int buflen)
{
	struct protoent *result;

	if (! _file) {
		setprotoent_internal(0);
	}

	result = fgetprotoent_r(_file, entry, buffer, buflen);

	/*
	** It is possible for the dynamic file to be timed out while
	** we are still reading, so if we get a read error we restart.
	*/
	if (! result && _file && ferror(_file)) {
		endprotoent();
		setprotoent_internal(0);
		result = fgetprotoent_r(_file, entry, buffer, buflen);
	}

	return result;
}

struct protoent *
getprotoent(void)
{
	struct protoent *result;

	if (! _file) {
		setprotoent_internal(0);
	}

	result = fgetprotoent(_file);

	/*
	** It is possible for the dynamic file to be timed out while
	** we are still reading, so if we get a read error we restart.
	*/
	if (! result && _file && ferror(_file)) {
		endprotoent();
		setprotoent_internal(0);
		result = fgetprotoent(_file);
	}

	return result;
}

struct protoent *
getprotobyname_r(const char *name, struct protoent *entry, char *buffer,
    int buflen)
{
	int status;
	int accessfailed=0;
	char **cp;
	struct protoent *ent;
	struct stat sbuf;

	if (! name || ! entry || ! buffer) {
		return (struct protoent *)0;
	}

	if (! _getXbyY_no_stat && stat(LOCALFILE, &sbuf) == 0) {
		_map_byname.m_version = sbuf.st_mtim.tv_sec;
	} else {
		accessfailed=1;
	}

	status = ns_lookup(&_map_byname, 0, "protocols.byname", name, 0,
	    buffer, buflen);
	if (status == NS_SUCCESS) {
		if (strtoprotoent(entry, buffer, buflen)) {
			return entry;
		}
	}

	if (status == NS_NOTFOUND || accessfailed) {
		return (struct protoent *)0;
	}

	/*
	** Fallback to local files.
	*/
	if (!setprotoent_files(0)) {
		return (struct protoent *)0;
	}

	while (ent = getprotoent_r(entry, buffer, buflen)) {
		if (strcmp(ent->p_name, name) == 0) {
			break;
		}
		for (cp = ent->p_aliases; *cp; cp++) {
			if (strcmp(*cp, name) == 0) {
				goto found;
			}
		}
	}
found:
	if (! _map_byname.m_stayopen) {
		endprotoent();
	}

	return ent;
}

struct protoent *
getprotobyname(const char *name)
{
	if (! name || ! *name) {
		return (struct protoent *)0;
	}
	if (! _entry && ! init_entry()) {
		return (struct protoent *)0;
	}
	if (! _buffer && ! init_buffer()) {
		return (struct protoent *)0;
	}

	return getprotobyname_r(name, _entry, _buffer, LOCALBUFLEN);
}

struct protoent *
getprotobynumber_r(int proto, struct protoent *entry, char *buffer, int buflen)
{
	int status;
	int accessfailed=0;
	char key[16];
	struct protoent *ent;
	struct stat sbuf;

	if (! entry || ! buffer) {
		return (struct protoent *)0;
	} 

	_ltoa((long)proto, key);

	if (! _getXbyY_no_stat && stat(LOCALFILE, &sbuf) == 0) {
		_map_bynumber.m_version = sbuf.st_mtim.tv_sec;
	} else {
		accessfailed=1;
	}

	status = ns_lookup(&_map_bynumber, 0, "protocols.bynumber", key, 0,
	    buffer, buflen);
	if (status == NS_SUCCESS) {
		if (strtoprotoent(entry, buffer, buflen)) {
			return entry;
		}
	}

	if (status == NS_NOTFOUND || accessfailed) {
		return (struct protoent *)0;
	}

	/*
	** Fallback to local files.
	*/
	if (!setprotoent_files(0)) {
		return (struct protoent *)0;
	}

	while (((ent = getprotoent_r(entry, buffer, buflen)) != NULL) &&
	    (proto != ent->p_proto));
	if (! _map_bynumber.m_stayopen) {
		endprotoent();
	}

	return ent;
}

struct protoent *
getprotobynumber(int proto)
{
	if (! _entry && ! init_entry()) {
		return (struct protoent *)0;
	}
	if (! _buffer && ! init_buffer()) {
		return (struct protoent *)0;
	}

	return getprotobynumber_r(proto, _entry, _buffer, LOCALBUFLEN);
}
