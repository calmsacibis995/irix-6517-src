#ifdef __STDC__
	#pragma weak gethostbyname_r = _gethostbyname_r
	#pragma weak gethostbyname = _gethostbyname
	#pragma weak gethostbyaddr_r = _gethostbyaddr_r
	#pragma weak gethostbyaddr = _gethostbyaddr
	#pragma weak gethostent_r = _gethostent_r
	#pragma weak gethostent = _gethostent
	#pragma weak fgethostent_r = _fgethostent_r
	#pragma weak fgethostent = _fgethostent
	#pragma weak sethostent = _sethostent
	#pragma weak endhostent = _endhostent
	#pragma weak sethostfile = _sethostfile
	#pragma weak h_errno = _h_errno
	#pragma weak sethostresorder = _sethostresorder
#endif

#define SOCKS_WAR	1

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

#ifdef SOCKS_WAR
#include <arpa/nameser.h>
#include <resolv.h>
#include <alloca.h>
extern struct __res_state _res;
#endif

int h_errno = 0;

#define LOCALBUFLEN	16384
static char *LOCALFILE = "/etc/hosts";
#define LISTMAP		"hosts"

static FILE *_file = (FILE *)0;
static struct hostent *_entry _INITBSS;
static char *_buffer _INITBSS;

extern int _getXbyY_no_stat;

static ns_map_t _map_byname _INITBSSS;
static ns_map_t _map_byaddr _INITBSSS;

extern int _ultoa(unsigned long, char *);

/*
** This routine no longer does anything, but is left here for backward
** compatability.
*/
/* ARGSUSED */
int
sethostresorder(const char *str)
{
	return 0;
}

void
sethostent(int s)
{
	if (_file) {
		rewind(_file);
	}
	_map_byname.m_stayopen |= s;
	_map_byaddr.m_stayopen |= s;
}	

static int
sethostent_files(int s)
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
	_map_byaddr.m_stayopen |= s;

	return(fp?1:0);
}

static void
sethostent_internal(int s)
{
	FILE *fp;

	sethostent(s);
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
endhostent(void)
{
	FILE *fp;

	fp = (FILE *)test_and_set((u_long *)&_file, 0);
	if (fp) {
		fclose(fp);
	}
	_map_byname.m_stayopen = 0;
	_map_byaddr.m_stayopen = 0;
}

/*
** We allocate a buffer large enough to hold the entire hostent structure.
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
	struct hostent *ep;

	ep = (struct hostent *)malloc(sizeof(struct hostent));
	if (! __compare_and_swap((long *)&_entry, 0, (long)ep)) {
		/* Race condition. */
		free(ep);
	}
	return (_entry) ? 1 : 0;
}

/*
** This routine splits up the buffer which contains a host line, and
** sets the pointers in the given hostent structure to the addresses
** inside of the buffer.
**
** Source is of the form:
**	address[\saddress]*\sprimary[\salias]?[\salias]*
*/
static int
strtohent(struct hostent *he, char *buf, int len)
{
	char *p, *q, **list, *end;
	struct in_addr *addrs;
	int elen;

	/*
	** We stick the pointer arrays at the end of the buffer after
	** the data.  We check the address incrementally before we
	** store anything in it since we do not know the length of these
	** lists.
	*/
	elen = (int)strlen(buf) + 1;
	elen += (sizeof(char **) - (elen % sizeof(char **)));
	he->h_addr_list = list = (char **)&buf[elen];
	end = &buf[len - 3];

	/* Address list. */
	he->h_addrtype = AF_INET;
	he->h_length = sizeof(struct in_addr);
	addrs = (struct in_addr *)buf;
	for (p = buf; *p && (*p == ' ' || *p == '\t'); p++);
	for (q = p; *p && *p != '\n' && *p != '#';) {
		for (q = p;
		    *p && *p != ' ' && *p != '\t' && *p != '\n' && *p != '#';
		    p++);
		for (; *p && (*p == ' ' || *p == '\t'); *p++ = 0);
		if (&list[1] > (char **)end) {
			/* Not enough space. */
			return 0;
		}
		if (! inet_aton(q, addrs)) {
			/* Bad address. We must be in the names.*/
			break;
		}
		*list++ = (char *)addrs++;
	}
	*list++ = (char *)0;
	if (! he->h_addr_list[0]) {
		/* found no good addresses */
		return 0;
	}

	/* Primary name. */
	if (! *q || *q == '\n' || *q == '#') {
		/* No name. */
		return 0;
	}
	he->h_name = q;

	/* Alias names. */
	he->h_aliases = list;
	for (; *p && *p != '\n' && *p != '#';) {
		for (q = p;
		    *p && *p != ' ' && *p != '\t' && *p != '\n' && *p != '#';
		    p++);
		for (; *p && (*p == ' ' || *p == '\t'); *p++ = 0);
		if (&list[1] > (char **)end) {
			/* Not enough space. */
			return 0;
		}
		*list++ = q;
	}
	*list = (char *)0;
	*p = 0;

	return 1;
}

/*
** This is the reintrant version of gethostent.  If we cannot fit the
** data into the given buffer then we continue with the next item.
** There is no way to tell the difference between an end of enumeration
** and an entry which did not fit into the buffer.  This interface is
** man page compatible with the Solaris version.  It would have been
** better if they allowed the return of an error.
*/
struct hostent *
fgethostent_r(FILE *f, struct hostent *entry, char *buffer, int buflen)
{
	if (! entry || ! buffer || ! f) {
		return (struct hostent *)0;
	}

	while (fgets(buffer, buflen, f) != NULL) {
		if (strtohent(entry, buffer, buflen)) {
			return entry;
		}
	}

	return (struct hostent *)0;
}

struct hostent *
fgethostent(FILE *f)
{
	h_errno = 0;
	if (! f) {
		h_errno = HOST_NOT_FOUND;
		return (struct hostent *)0;
	}
	if (! _entry && ! init_entry()) {
		h_errno = NO_RECOVERY;
		return (struct hostent *)0;
	}
	if (! _buffer && ! init_buffer()) {
		h_errno = NO_RECOVERY;
		return (struct hostent *)0;
	}

	while (fgets(_buffer, LOCALBUFLEN, f) != NULL) {
		if (strtohent(_entry, _buffer, LOCALBUFLEN)) {
			return _entry;
		}
	}

	h_errno = HOST_NOT_FOUND;
	return (struct hostent *)0;
}

struct hostent *
gethostent_r(struct hostent *entry, char *buffer, int buflen)
{
	struct hostent *result;

	if (! _file) {
		sethostent_internal(0);
	}

	result = fgethostent_r(_file, entry, buffer, buflen);

	/*
	** It is possible for the dynamic file to timeout in the
	** middle of reading through it, so if we get a read error
	** we start over.
	*/
	if (! result && _file && ferror(_file)) {
		endhostent();
		sethostent_internal(_map_byname.m_stayopen);
		result = fgethostent_r(_file, entry, buffer, buflen);
	}

	return result;
}

struct hostent *
gethostent(void)
{
	struct hostent *result;

	if (! _file) {
		sethostent_internal(0);
	}

	result = fgethostent(_file);

	/*
	** It is possible for the dynamic file to timeout in the
	** middle of reading through it, so if we get a read error
	** we start over.
	*/
	if (! result && _file && ferror(_file)) {
		endhostent();
		sethostent_internal(_map_byname.m_stayopen);
		result = fgethostent(_file);
	}

	return result;
}

/*
** This is identical to inet_ntoa except that it uses a passed in buffer
** instead of a static.
*/
static void
addrtostr(struct in_addr in, char *b)
{
	char *p = (char *)&in;
#define UC(b)   (((unsigned long)b)&0xff)
	b += _ultoa(UC(p[0]), b);
	*b++ = '.';
	b += _ultoa(UC(p[1]), b);
	*b++ = '.';
	b += _ultoa(UC(p[2]), b);
	*b++ = '.';
	b += _ultoa(UC(p[3]), b);
	*b = 0;
}

/*
** This is the same as hostalias in res_query.c, but uses the passed in
** buffer.
*/
static char *
ha(const char *name, char *buf, int buflen, ns_map_t *map)
{
	char *cp1, *cp2, *file;
	FILE *fp;

	file = getenv("HOSTALIASES");
	if (file == NULL || (fp = fopen(file, "r")) == NULL) {
		/* cache failure and don't try this again */
		map->m_flags |= NS_MAP_NO_HOSTALIAS;
		return (NULL);
	}
	setbuf(fp, NULL);
	buf[buflen - 1] = '\0';
	while (fgets(buf, buflen - 1, fp)) {
		for (cp1 = buf; *cp1 && !isspace(*cp1); ++cp1)
			;
		if (!*cp1)
			break;
		*cp1 = '\0';
		if (!strcasecmp(buf, name)) {
			while (isspace(*++cp1))
				;
			if (!*cp1)
				break;
			for (cp2 = cp1 + 1; *cp2 && !isspace(*cp2); ++cp2)
				;
			*cp2 = '\0';
			fclose(fp);
			return cp1;
		}
	}
	fclose(fp);
	return NULL;
}

struct hostent *
gethostbyname_r(const char *name, struct hostent *entry, char *buffer,
    int buflen, int *h_errnop)
{
	int status;
	int accessfailed=0;
	char **cp, *np;
	extern int inet_isaddr(const char *, uint32_t *);
	struct hostent *ent;
	struct stat sbuf;

	*h_errnop = 0;

	if (! name || ! entry || ! buffer) {
		*h_errnop = NO_RECOVERY;
		return (struct hostent *)0;
	}

	/*
	** If we are given an address, then we just hand-craft a
	** hostent structure to give back.
	*/
	if (inet_isaddr(name, (uint32_t *)buffer)) {
		entry->h_name = (char *)name;
		entry->h_addr_list = (char **)(buffer + sizeof(void *));
		entry->h_addr_list[0] = (char *)buffer;
		entry->h_addr_list[1] = (char *)0;
		entry->h_aliases = &entry->h_addr_list[2];
		entry->h_aliases[0] = (char *)0;
		entry->h_addrtype = AF_INET;
		entry->h_length = sizeof(u_int32_t);

		return entry;
	}

	if (! _getXbyY_no_stat && stat(LOCALFILE, &sbuf) == 0) {
		_map_byname.m_version = sbuf.st_mtim.tv_sec;
	} else {
		accessfailed=1;
	}

	/*
	** If the nameservice is running, and the user has the HOSTALIAS
	** environment variable set, then lookup the alias instead of the
	** passed in name.
	*/
	if ((_map_byname.m_flags & NS_MAP_NO_HOSTALIAS) ||
	    ! (np = ha(name, buffer, buflen, &_map_byname))) {
		np = (char *)name;
	}

#ifdef SOCKS_WAR
	{
		extern struct in_addr __nsaddr_first;
		char *key, addr[16];

		if (__nsaddr_first.s_addr != 0 &&
		    _res.nsaddr_list[0].sin_addr.s_addr !=
		    __nsaddr_first.s_addr) {
			/*
			** Socks library has messed with nameserver list so we
			** specifically use the first nameserver.
			*/
			key = (char *)alloca((unsigned)strlen(np) + 32);
			addrtostr(_res.nsaddr_list[0].sin_addr, addr);
			sprintf(key, "%s(dns_servers=%s)", np, addr);
			status = ns_lookup(&_map_byname, 0, "hosts.byname", key,
			    0, buffer, buflen);
		} else {
			status = ns_lookup(&_map_byname, 0, "hosts.byname",
			    np, 0, buffer, buflen);
		}
	}
#else
	status = ns_lookup(&_map_byname, 0, "hosts.byname", np, 0,
	    buffer, buflen);
#endif
	if (status == NS_SUCCESS) {
		if (strtohent(entry, buffer, buflen)) {
			return entry;
		}
		*h_errnop = NO_RECOVERY;
		return (struct hostent *)0;
	}

	if (status == NS_NOTFOUND || accessfailed) {
		*h_errnop = HOST_NOT_FOUND;
		return (struct hostent *)0;
	}

	/*
	** Fallback to local files.
	*/
	while (ent = gethostent_r(entry, buffer, buflen)) {
		if (strcasecmp(ent->h_name, name) == 0) {
			break;
		}
		for (cp = ent->h_aliases; *cp != 0; cp++) {
			if (strcasecmp(*cp, name) == 0) {
				goto found;
			}
		}
	}
found:
	if (! _map_byname.m_stayopen) {
		endhostent();
	}

	if (! ent) {
		*h_errnop = HOST_NOT_FOUND;
	}
	return ent;
}

struct hostent *
gethostbyname(const char *name)
{
	h_errno = 0;
	if (! name || ! *name) {
		h_errno = NO_RECOVERY;
		return (struct hostent *)0;
	}
	if (! _entry && ! init_entry()) {
		h_errno = NO_RECOVERY;
		return (struct hostent *)0;
	}
	if (! _buffer && ! init_buffer()) {
		h_errno = NO_RECOVERY;
		return (struct hostent *)0;
	}

	return gethostbyname_r(name, _entry, _buffer, LOCALBUFLEN, &h_errno);
}

struct hostent *
_gethtbyname(const char *name)
{
	struct hostent *ent;
	char **cp;

	h_errno = 0;
	if (! name || ! *name) {
		h_errno = NO_RECOVERY;
		return (struct hostent *)0;
	}
	if (! _entry && ! init_entry()) {
		h_errno = NO_RECOVERY;
		return (struct hostent *)0;
	}
	if (! _buffer && ! init_buffer()) {
		h_errno = NO_RECOVERY;
		return (struct hostent *)0;
	}
	if (! _file) {
		sethostent_files(0);
	}

	while (ent = gethostent_r(_entry, _buffer, LOCALBUFLEN)) {
		if (strcasecmp(ent->h_name, name) == 0) {
			break;
		}
		for (cp = ent->h_aliases; *cp != 0; cp++) {
			if (strcasecmp(*cp, name) == 0) {
				goto found;
			}
		}
	}
found:
	if (! _map_byname.m_stayopen) {
		endhostent();
	}

	if (! ent) {
		h_errno = HOST_NOT_FOUND;
	}

	return ent;
}

struct hostent *
gethostbyaddr_r(const void *addr, size_t len, int type, struct hostent *entry,
    char *buffer, int buflen, int *h_errnop)
{
	int status;
	int accessfailed=0;
	char key[16];
	struct hostent *ent;
	struct stat sbuf;

	*h_errnop = 0;

	if (! addr || ! entry || ! buffer || type != AF_INET ||
	    (len != sizeof(struct in_addr))) {
		*h_errnop = NO_RECOVERY;
		return (struct hostent *)0;
	} 

	addrtostr(*(struct in_addr *)addr, key);

	if (! _getXbyY_no_stat && stat(LOCALFILE, &sbuf) == 0) {
		_map_byaddr.m_version = sbuf.st_mtim.tv_sec;
	} else {
		accessfailed=1;
	}

#ifdef SOCKS_WAR
	{
		extern struct in_addr __nsaddr_first;
		char name[64], buf[16];

		if (__nsaddr_first.s_addr != 0 &&
		    _res.nsaddr_list[0].sin_addr.s_addr !=
		    __nsaddr_first.s_addr) {
			/*
			** Socks library has messed with nameserver list so we
			** specifically use the first nameserver.
			*/
			addrtostr(__nsaddr_first, buf);
			sprintf(name, "%s(dns_servers=%s)", key, buf);
			status = ns_lookup(&_map_byaddr, 0, "hosts.byaddr",
			    name, 0, buffer, buflen);
		} else {
			status = ns_lookup(&_map_byaddr, 0, "hosts.byaddr",
			    key, 0, buffer, buflen);
		}
	}
#else
	status = ns_lookup(&_map_byaddr, 0, "hosts.byaddr", key, 0, buffer,
	    buflen);
#endif

	if (status == NS_SUCCESS) {
		if (strtohent(entry, buffer, buflen)) {
			return entry;
		}
		*h_errnop = NO_RECOVERY;
		return (struct hostent *)0;
	}

	if (status == NS_NOTFOUND || accessfailed) {
		*h_errnop = HOST_NOT_FOUND;
		return (struct hostent *)0;
	}

	/*
	** Fallback to local files.
	*/
	if (!sethostent_files(0)) {
		return (struct hostent *)0;
	}
	while ((ent = gethostent_r(entry, buffer, buflen)) != NULL) {
		if (ent->h_addrtype == type &&
		    (ent->h_addr != NULL) && (addr != NULL) &&
		    (memcmp(ent->h_addr, addr, len) == 0)) {
			break;
		}
	}
	if (! _map_byaddr.m_stayopen) {
		endhostent();
	}
	if (! ent) {
		*h_errnop = HOST_NOT_FOUND;
	}

	return ent;
}

struct hostent *
gethostbyaddr(const void *addr, int len, int type)
{
	if (! _entry && ! init_entry()) {
		h_errno = NO_RECOVERY;
		return (struct hostent *)0;
	}
	if (! _buffer && ! init_buffer()) {
		h_errno = NO_RECOVERY;
		return (struct hostent *)0;
	}

	return gethostbyaddr_r(addr, len, type, _entry, _buffer, LOCALBUFLEN,
	    &h_errno);
}

/*
** Compatability for XPG4 where len changed sizes.  We duplicate the buffer
** initialization instead of having an extra call.
*/
struct hostent *
__xpg4_gethostbyaddr(const void *addr, size_t len, int type)
{
	if (! _entry && ! init_entry()) {
		h_errno = NO_RECOVERY;
		return (struct hostent *)0;
	}
	if (! _buffer && ! init_buffer()) {
		h_errno = NO_RECOVERY;
		return (struct hostent *)0;
	}

	return gethostbyaddr_r(addr, len, type, _entry, _buffer, LOCALBUFLEN,
	    &h_errno);
}

struct hostent *
_gethtbyaddr(const void *addr, int len, int type)
{
	struct hostent *ent;

	h_errno = 0;
	if (! _file) {
		sethostent_files(0);
	}
	if (! _entry && ! init_entry()) {
		h_errno = NO_RECOVERY;
		return (struct hostent *)0;
	}
	if (! _buffer && ! init_buffer()) {
		h_errno = NO_RECOVERY;
		return (struct hostent *)0;
	}
	while ((ent = gethostent_r(_entry, _buffer, LOCALBUFLEN)) != NULL) {
		if (ent->h_addrtype == type &&
		    (ent->h_addr != NULL) && (addr != NULL) &&
		    (memcmp(ent->h_addr, addr, len) == 0)) {
			break;
		}
	}
	if (! _map_byaddr.m_stayopen) {
		endhostent();
	}
	if (! ent) {
		h_errno = HOST_NOT_FOUND;
	}

	return ent;
}

void
sethostfile(char *file)
{
	LOCALFILE = file;
}
