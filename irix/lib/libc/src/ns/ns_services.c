#ifdef __STDC__
	#pragma weak getservbyname_r = _getservbyname_r
	#pragma weak getservbyname = _getservbyname
	#pragma weak getservbyport_r = _getservbyport_r
	#pragma weak getservbyport = _getservbyport
	#pragma weak getservent_r = _getservent_r
	#pragma weak getservent = _getservent
	#pragma weak setservent = _setservent
	#pragma weak endservent = _endservent
	#pragma weak fgetservent_r = _fgetservent_r
	#pragma weak fgetservent = _fgetservent
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
#include <netdb.h>
#include <alloca.h>
#include <errno.h>
#include <ns_api.h>
#include "compare.h"

#define LOCALBUFLEN	8192
#define LOCALFILE	"/etc/services"
#define LISTMAP		"services.byname"

static FILE *_file = (FILE *)0;
static struct servent *_entry _INITBSS;
static char *_buffer _INITBSS;

static ns_map_t _map_byname _INITBSSS;
static ns_map_t _map_byport _INITBSSS;

extern int _getXbyY_no_stat;

extern int _ltoa(long, char *);
int _getserv_file_is_first _INITBSS;

void
setservent(int s)
{
	if (_file) {
		rewind(_file);
	}
	_map_byname.m_stayopen |= s;
	_map_byport.m_stayopen |= s;
}

static int
setservent_files(int s)
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
		fp=_file;
	}

	_map_byname.m_stayopen |= s;
	_map_byport.m_stayopen |= s;

	return(fp?1:0);
}

static void
setservent_internal(int s)
{
	FILE *fp;

	setservent(s);
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
endservent(void)
{
	FILE *fp;

	fp = (FILE *)test_and_set((u_long *)&_file, 0);
	if (fp) {
		fclose(fp);
	}

	_map_byname.m_stayopen = 0;
	_map_byport.m_stayopen = 0;
}

/*
** We allocate a buffer large enough to hold the entire servent structure.
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
	struct servent *ep;

	ep = (struct servent *)malloc(sizeof(struct servent));
	if (! __compare_and_swap((long *)&_entry, 0, (long)ep)) {
		/* Race condition. */
		free(ep);
	}
	return (_entry) ? 1 : 0;
}

/*
** We assume that a servent entry is of the form:
**	s_name\s+s_port/s_proto[\s+s_aliases[,s_aliases]*]?
*/
static int
strtoservent(struct servent *se, char *buffer, int buflen)
{
	char *p, *q, *r, **list, **end;
	int len;

	p = strpbrk(buffer, "\n#");
	if (p) {
		*p++ = (char)0;
		len = (int)(p - buffer);
	} else {
		len = (int)strlen(buffer);
	}
	len += (sizeof(char **) - (len % sizeof(char **)));
	se->s_aliases = list = (char **)&buffer[len];
	end = (char **)&buffer[buflen - sizeof(*list)];

	q = buffer;
	r = (char *)0;
	p = strtok_r(q, " \t", &r);
	if (! p) {
		return 0;
	}
	se->s_name = p;
	q = (char *)0;

	p = strtok_r(q, "/", &r);
	if (! p) {
		return 0;
	}
	se->s_port = (int)strtol(p, &q, 10);
	if (p == q || se->s_port < 0 || se->s_port > 65536) {
		return 0;
	}
	q = (char *)0;

	p = strtok_r(q, " \t", &r);
	if (! p) {
		return 0;
	}
	se->s_proto = p;

	while (p = strtok_r(q, " \t", &r)) {
		if (&list[1] > end) {
			return 0;
		}
		*list++ = p;
	}
	*list = (char *)0;

	return 1;
}

/*
** This is the reintrant version of getservent.  If we cannot fit the
** data into the given buffer then we continue with the next item.
** There is no way to tell the difference between an end of enumeration
** and an entry which did not fit into the buffer.  This interface is
** man page compatible with the Solaris version.  It would have been
** better if they allowed the return of an error.
*/
struct servent *
fgetservent_r(FILE *f, struct servent *entry, char *buffer, int buflen)
{
	if (! entry || ! buffer || ! f) {
		return (struct servent *)0;
	}

	while (fgets(buffer, buflen, f) != NULL) {
		if (strtoservent(entry, buffer, buflen)) {
			return entry;
		}
	}

	return (struct servent *)0;
}

struct servent *
fgetservent(FILE *f)
{
	if (! f) {
		return (struct servent *)0;
	}
	if (! _entry && ! init_entry()) {
		return (struct servent *)0;
	}
	if (! _buffer && ! init_buffer()) {
		return (struct servent *)0;
	}

	while (fgets(_buffer, LOCALBUFLEN, f) != NULL) {
		if (strtoservent(_entry, _buffer, LOCALBUFLEN)) {
			return _entry;
		}
	}

	return (struct servent *)0;
}

struct servent *
getservent_r(struct servent *entry, char *buffer, int buflen)
{
	struct servent *result;

	if (! _file) {
		setservent_internal(0);
	}

	result = fgetservent_r(_file, entry, buffer, buflen);

	/*
	** It is possible for the dynamic file to be timed out while
	** we are in the middle so we restart on a read error.
	*/
	if (! result && _file && ferror(_file)) {
		endservent();
		setservent_internal(0);
		result = fgetservent_r(_file, entry, buffer, buflen);
	}

	return result;
}

struct servent *
getservent(void)
{
	struct servent *result;

	if (! _file) {
		setservent_internal(0);
	}

	result = fgetservent(_file);

	/*
	** It is possible for the dynamic file to be timed out while
	** we are in the middle so we restart on a read error.
	*/
	if (! result && _file && ferror(_file)) {
		endservent();
		setservent_internal(0);
		result = fgetservent(_file);
	}

	return result;
}

struct servent *
getservbyname_r(const char *name, const char *proto, struct servent *entry,
    char *buffer, int buflen)
{
	int status;
	int accessfailed=0;
	char **cp;
	char *key;
	struct servent *ent;
	struct stat sbuf;

	if (! name || ! *name || ! entry || ! buffer) {
		return (struct servent *)0;
	}

	if (proto && *proto) {
		key = (char *)alloca((int)strlen(name) +
		    (int)strlen(proto) + 2);
		sprintf(key, "%s\\%s", name, proto);
	} else {
		key = (char *)name;
	}

	if (! _getXbyY_no_stat && stat(LOCALFILE, &sbuf) == 0) {
		_map_byname.m_version = sbuf.st_mtim.tv_sec;
	} else {
		accessfailed=1;
	}

	status = ns_lookup(&_map_byname, 0, "services", key, 0,
	    buffer, buflen);
	if (status == NS_SUCCESS) {
		if (strtoservent(entry, buffer, buflen)) {
			return entry;
		}
	}

	if (status == NS_NOTFOUND || accessfailed ) {
		return (struct servent *)0;
	}

	/*
	** Fallback to local files.
	*/
	if (!setservent_files(0)) {
		return (struct servent *)0;
	}

	while (ent = getservent_r(entry, buffer, buflen)) {
		if (!proto || (strcmp(proto, ent->s_proto) == 0)) {
			if (strcmp(ent->s_name, name) == 0) {
				break;
			}
		}
		for (cp = ent->s_aliases; *cp; cp++) {
			if (!proto || (strcmp(proto, ent->s_proto) == 0)) {
				if (strcmp(*cp, name) == 0) {
					goto found;
				}
			}
		}
	}
found:
	if (! _map_byname.m_stayopen) {
		endservent();
	}

	return ent;
}

struct servent *
getservbyname(const char *name, const char *proto)
{
	if (! name || ! *name) {
		return (struct servent *)0;
	}
	if (! _entry && ! init_entry()) {
		return (struct servent *)0;
	}
	if (! _buffer && ! init_buffer()) {
		return (struct servent *)0;
	}

	return getservbyname_r(name, proto, _entry, _buffer, LOCALBUFLEN);
}

struct servent *
getservbyport_r(int port, const char *proto, struct servent *entry,
    char *buffer, int buflen)
{
	int status;
	int accessfailed=0;
	char *key;
	struct servent *ent;
	struct stat sbuf;

	if (! entry || ! buffer) {
		return (struct servent *)0;
	} 

	if (proto && *proto) {
		key = (char *)alloca((int)strlen(proto) + 16);
		sprintf(key, "%d\\%s", port, proto);
	} else {
		key = (char *)alloca(16);
		_ltoa((long)port, key);
	}

	if (! _getXbyY_no_stat && stat(LOCALFILE, &sbuf) == 0) {
		_map_byport.m_version = sbuf.st_mtim.tv_sec;
	} else {
		accessfailed=1;
	}

	/*
	** NOTE: historically the NIS map was called services.byname
	** but indexed by port/proto.  It did not seem prudent to
	** change that.
	*/
	status = ns_lookup(&_map_byport, 0, "services.byname", key, 0,
	    buffer, buflen);
	if (status == NS_SUCCESS) {
		if (strtoservent(entry, buffer, buflen)) {
			return entry;
		}
	}

	if (status == NS_NOTFOUND || accessfailed) {
		return (struct servent *)0;
	}

	/*
	** Fallback to local files.
	*/
	if (!setservent_files(0)) {
		return (struct servent *)0;
	}

	while ((ent = getservent_r(entry, buffer, buflen)) &&
	    ((port != ent->s_port) ||
	    (proto && (strcmp(proto, ent->s_proto) != 0))));
	if (! _map_byport.m_stayopen) {
		endservent();
	}

	return ent;
}

struct servent *
getservbyport(int port, const char *proto)
{
	if (! _entry && ! init_entry()) {
		return (struct servent *)0;
	}
	if (! _buffer && ! init_buffer()) {
		return (struct servent *)0;
	}

	return getservbyport_r(port, proto, _entry, _buffer, LOCALBUFLEN);
}
