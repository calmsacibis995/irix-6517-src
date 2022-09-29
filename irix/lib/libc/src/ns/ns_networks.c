#ifdef __STDC__
	#pragma weak getnetbyname_r = _getnetbyname_r
	#pragma weak getnetbyname = _getnetbyname
	#pragma weak getnetbyaddr_r = _getnetbyaddr_r
	#pragma weak getnetbyaddr = _getnetbyaddr
	#pragma weak getnetent_r = _getnetent_r
	#pragma weak getnetent = _getnetent
	#pragma weak setnetent = _setnetent
	#pragma weak endnetent = _endnetent
	#pragma weak fgetnetent_r = _fgetnetent_r
	#pragma weak fgetnetent = _fgetnetent
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
#define LOCALFILE	"/etc/networks"
#define LISTMAP		"networks"

static FILE *_file = (FILE *)0;
static struct netent *_entry _INITBSS;
static char *_buffer _INITBSS;

extern int _getXbyY_no_stat;

static ns_map_t _map_byname _INITBSSS;
static ns_map_t _map_byaddr _INITBSSS;

extern int _ultoa(unsigned long, char *);

void
setnetent(int s)
{
	if (_file) {
		rewind(_file);
	}
	_map_byaddr.m_stayopen |= s;
	_map_byname.m_stayopen |= s;
}

static int
setnetent_files(int s)
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

	_map_byaddr.m_stayopen |= s;
	_map_byname.m_stayopen |= s;

	return(fp?1:0);
}

static void
setnetent_internal(int s)
{
	FILE *fp;

	setnetent(s);
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
endnetent(void)
{
	FILE *fp;

	fp = (FILE *)test_and_set((u_long *)&_file, 0);
	if (fp) {
		fclose(fp);
	}

	_map_byaddr.m_stayopen = 0;
	_map_byname.m_stayopen = 0;
}

/*
** We allocate a buffer large enough to hold the entire netent structure.
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
	struct netent *ep;

	ep = (struct netent *)malloc(sizeof(struct netent));
	if (! __compare_and_swap((long *)&_entry, 0, (long)ep)) {
		/* Race condition. */
		free(ep);
	}
	return (_entry) ? 1 : 0;
}

/*
** This routine will split the data in the src buffer into the
** result netent structure and the supplied buffer.
*/
static int
strtonetent(struct netent *entry, char *buf, int len)
{
	char *p, *q, *r, **index, **end;
	int total;

	/*
	** We place the array of pointers after the null in buf.
	*/
	total = (int)strlen(buf);
	p = buf + total + (sizeof(*index) - (total % sizeof(*index)));
	index = (char **)p;
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
	entry->n_name = q;

	p = (char *)0;
	q = strtok_r(p, " \t\n", &r);
	if (! q) {
		return 0;
	}
	entry->n_net = inet_network(q);
	entry->n_addrtype = AF_INET;

	entry->n_aliases = index;
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
** This is the reintrant version of getnetent.  If we cannot fit the
** data into the given buffer then we continue with the next item.
** There is no way to tell the difference between an end of enumeration
** and an entry which did not fit into the buffer.  This interface is
** man page compatible with the Solaris version.  It would have been
** better if they allowed the return of an error.
*/
struct netent *
fgetnetent_r(FILE *f, struct netent *entry, char *buffer, int buflen)
{
	if (! entry || ! buffer || ! f) {
		return (struct netent *)0;
	}

	while (fgets(buffer, buflen, f) != NULL) {
		if (strtonetent(entry, buffer, buflen)) {
			return entry;
		}
	}

	return (struct netent *)0;
}

struct netent *
fgetnetent(FILE *f)
{
	if (! f) {
		return (struct netent *)0;
	}
	if (! _entry && ! init_entry()) {
		return (struct netent *)0;
	}
	if (! _buffer && ! init_buffer()) {
		return (struct netent *)0;
	}

	while (fgets(_buffer, LOCALBUFLEN, f) != NULL) {
		if (strtonetent(_entry, _buffer, LOCALBUFLEN)) {
			return _entry;
		}
	}

	return (struct netent *)0;
}

struct netent *
getnetent_r(struct netent *entry, char *buffer, int buflen)
{
	struct netent *result;

	if (! _file) {
		setnetent_internal(0);
	}

	result = fgetnetent_r(_file, entry, buffer, buflen);

	/*
	** It is possible for the dynamic file to get timed out while
	** we are still reading so if we get a read error we restart.
	*/

	if (! result && _file && ferror(_file)) {
		endnetent();
		setnetent_internal(0);
		result = fgetnetent_r(_file, entry, buffer, buflen);
	}

	return result;
}

struct netent *
getnetent(void)
{
	struct netent *result;

	if (! _file) {
		setnetent_internal(0);
	}

	result = fgetnetent(_file);

	/*
	** It is possible for the dynamic file to get timed out while
	** we are still reading so if we get a read error we restart.
	*/

	if (! result && _file && ferror(_file)) {
		endnetent();
		setnetent_internal(0);
		result = fgetnetent(_file);
	}

	return result;
}

struct netent *
getnetbyname_r(const char *name, struct netent *entry, char *buffer, int buflen)
{
	int status;
	int accessfailed=0;
	char **cp;
	struct netent *ent;
	struct stat sbuf;

	if (! name || ! entry || ! buffer) {
		return (struct netent *)0;
	}

	if (! _getXbyY_no_stat && stat(LOCALFILE, &sbuf) == 0) {
		_map_byname.m_version = sbuf.st_mtim.tv_sec;
	} else {
		accessfailed=1;
	}

	status = ns_lookup(&_map_byname, 0, "networks.byname", name, 0,
	    buffer, buflen);
	if (status == NS_SUCCESS) {
		if (strtonetent(entry, buffer, buflen)) {
			return entry;
		}
	}

	if (status == NS_NOTFOUND || accessfailed) {
		return (struct netent *)0;
	}

	/*
	** Fallback to local files.
	*/
	if (! setnetent_files(0)) {
		return (struct netent *)0;
	}
	while (ent = getnetent_r(entry, buffer, buflen)) {
		if (strcmp(ent->n_name, name) == 0) {
			break;
		}
		for (cp = ent->n_aliases; *cp; cp++) {
			if (strcmp(*cp, name) == 0) {
				goto found;
			}
		}
	}
found:
	if (! _map_byname.m_stayopen) {
		endnetent();
	}

	return ent;
}

struct netent *
getnetbyname(const char *name)
{
	if (! name || ! *name) {
		return (struct netent *)0;
	}
	if (! _entry && ! init_entry()) {
		return (struct netent *)0;
	}
	if (! _buffer && ! init_buffer()) {
		return (struct netent *)0;
	}

	return getnetbyname_r(name, _entry, _buffer, LOCALBUFLEN);
}

static char *
inet_ntoa_r(struct in_addr in, char *b)
{
	char *p, *q;

	p = (char *)&in;
	q = b;
#define UC(b)   (((int)b)&0xff)
	q += _ultoa(UC(p[0]), q);
	*q++ = '.';
	q += _ultoa(UC(p[1]), q);
	*q++ = '.';
	q += _ultoa(UC(p[2]), q);
	*q++ = '.';
	q += _ultoa(UC(p[3]), q);
	*q = 0;

	return b;
}

static char *
nettoa(in_addr_t anet, char *ntoabuf)
{
	char *p;
	struct in_addr in;
	int addr;

	in = inet_makeaddr((int)anet, INADDR_ANY);
	addr = (int)in.s_addr;
	inet_ntoa_r(in, ntoabuf);
	if ((htonl(addr) & IN_CLASSA_HOST) == 0) {
		p = strchr(ntoabuf, '.');
		if (p == NULL)
			return (NULL);
		*p = 0;
	} else if ((htonl(addr) & IN_CLASSB_HOST) == 0) {
		p = strchr(ntoabuf, '.');
		if (p == NULL)
			return (NULL);
		p = strchr(p+1, '.');
		if (p == NULL)
			return (NULL);
		*p = 0;
	} else if ((htonl(addr) & IN_CLASSC_HOST) == 0) {
		p = strrchr(ntoabuf, '.');
		if (p == NULL)
			return (NULL);
		*p = 0;
	}
	return ntoabuf;
}

struct netent *
getnetbyaddr_r(in_addr_t net, int type, struct netent *entry, char *buffer,
    int buflen)
{
	int status;
	int accessfailed=0;
	char key[16];
	struct netent *ent;
	struct stat sbuf;

	if (! entry || ! buffer) {
		return (struct netent *)0;
	} 
	if (type != AF_INET) {
		return (struct netent *)0;
	}

	nettoa(net, key);

	if (! _getXbyY_no_stat && stat(LOCALFILE, &sbuf) == 0) {
		_map_byaddr.m_version = sbuf.st_mtim.tv_sec;
	} else {
		accessfailed=1;
	}

	status = ns_lookup(&_map_byaddr, 0, "networks.byaddr", key, 0,
	    buffer, buflen);
	if (status == NS_SUCCESS) {
		if (strtonetent(entry, buffer, buflen)) {
			return entry;
		}
	}

	if (status == NS_NOTFOUND ||accessfailed) {
		return (struct netent *)0;
	}

	/*
	** Fallback to local files.
	*/
	if (! setnetent_files(0)) {
		return (struct netent *)0;
	}
	while (((ent = getnetent_r(entry, buffer, buflen)) != NULL) &&
	    (net != ent->n_net));
	if (! _map_byaddr.m_stayopen) {
		endnetent();
	}

	return ent;
}

struct netent *
getnetbyaddr(in_addr_t net, int type)
{
	if (! _entry && ! init_entry()) {
		return (struct netent *)0;
	}
	if (! _buffer && ! init_buffer()) {
		return (struct netent *)0;
	}

	return getnetbyaddr_r(net, type, _entry, _buffer, LOCALBUFLEN);
}
