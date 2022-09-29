#ifdef __STDC__
	#pragma weak getrpcbyname_r = _getrpcbyname_r
	#pragma weak getrpcbyname = _getrpcbyname
	#pragma weak getrpcbynumber_r = _getrpcbynumber_r
	#pragma weak getrpcbynumber = _getrpcbynumber
	#pragma weak getrpcent_r = _getrpcent_r
	#pragma weak getrpcent = _getrpcent
	#pragma weak setrpcent = _setrpcent
	#pragma weak endrpcent = _endrpcent
	#pragma weak fgetrpcent_r = _fgetrpcent_r
	#pragma weak fgetrpcent = _fgetrpcent
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
#include <errno.h>
#include <ns_api.h>
#include "compare.h"

#define LOCALBUFLEN	8192
#define LOCALFILE	"/etc/rpc"
#define LISTMAP		"rpc"

static FILE *_file = (FILE *)0;
static struct rpcent *_entry _INITBSS;
static char *_buffer _INITBSS;

static ns_map_t _map_byname _INITBSSS;
static ns_map_t _map_bynumber _INITBSSS;

extern int _getXbyY_no_stat;

extern int _ltoa(long, char *);

void
setrpcent(int s)
{
	if (_file) {
		rewind(_file);
	}

	_map_byname.m_stayopen |= s;
	_map_bynumber.m_stayopen |= s;
}

static int
setrpcent_files(int s)
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

	return (fp?1:0);
}

static void
setrpcent_internal(int s)
{
	FILE *fp;

	setrpcent(s);
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
endrpcent(void)
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
** We allocate a buffer large enough to hold the entire rpcent structure.
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
	struct rpcent *ep;

	ep = (struct rpcent *)malloc(sizeof(struct rpcent));
	if (! __compare_and_swap((long *)&_entry, 0, (long)ep)) {
		/* Race condition. */
		free(ep);
	}
	return (_entry) ? 1 : 0;
}

/*
** This routine will split the data in the src buffer into the
** result rpcent structure and the supplied buffer.
*/
static int
strtorpcent(struct rpcent *entry, char *buf, int len)
{
	char *p, *q, *r, **index, **end;
	int total;

	/*
	** We place the array of pointers after the null in buf.
	*/
	total = (int)strlen(buf);
	p = buf + total + (sizeof(*index) - (total % sizeof(*index)));
	entry->r_aliases = index = (char **)p;
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
	entry->r_name = q;

	p = (char *)0;
	q = strtok_r(p, " \t\n", &r);
	if (! q) {
		return 0;
	}
	entry->r_number = atoi(q);

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
** This is the reintrant version of getrpcent.  If we cannot fit the
** data into the given buffer then we continue with the next item.
** There is no way to tell the difference between an end of enumeration
** and an entry which did not fit into the buffer.  This interface is
** man page compatible with the Solaris version.  It would have been
** better if they allowed the return of an error.
*/
struct rpcent *
fgetrpcent_r(FILE *f, struct rpcent *entry, char *buffer, int buflen)
{
	if (! entry || ! buffer || ! f) {
		return (struct rpcent *)0;
	}

	while (fgets(buffer, buflen, f) != NULL) {
		if (strtorpcent(entry, buffer, buflen)) {
			return entry;
		}
	}

	return (struct rpcent *)0;
}

struct rpcent *
fgetrpcent(FILE *f)
{
	if (! f) {
		return (struct rpcent *)0;
	}
	if (! _entry && ! init_entry()) {
		return (struct rpcent *)0;
	}
	if (! _buffer && ! init_buffer()) {
		return (struct rpcent *)0;
	}

	while (fgets(_buffer, LOCALBUFLEN, f) != NULL) {
		if (strtorpcent(_entry, _buffer, LOCALBUFLEN)) {
			return _entry;
		}
	}

	return (struct rpcent *)0;
}

struct rpcent *
getrpcent_r(struct rpcent *entry, char *buffer, int buflen)
{
	struct rpcent *result;

	if (! _file) {
		setrpcent_internal(0);
	}

	result = fgetrpcent_r(_file, entry, buffer, buflen);

	/*
	** It is possible for the dynamic file to be timed out while
	** we are still reading so if we get a read error we restart.
	*/
	if (! result && _file && ferror(_file)) {
		endrpcent();
		setrpcent_internal(0);
		result = fgetrpcent_r(_file, entry, buffer, buflen);
	}

	return result;
}

struct rpcent *
getrpcent(void)
{
	struct rpcent *result;

	if (! _file) {
		setrpcent_internal(0);
	}
	
	result = fgetrpcent(_file);

	/*
	** It is possible for the dynamic file to be timed out while
	** we are still reading so if we get a read error we restart.
	*/
	if (! result && _file && ferror(_file)) {
		endrpcent();
		setrpcent_internal(0);
		result = fgetrpcent(_file);
	}

	return result;
}

struct rpcent *
getrpcbyname_r(const char *name, struct rpcent *entry, char *buffer,
    int buflen)
{
	int status;
	int accessfailed=0;
	char **cp;
	struct rpcent *ent;
	struct stat sbuf;

	if (! name || ! entry || ! buffer) {
		return (struct rpcent *)0;
	}

	if (! _getXbyY_no_stat && stat(LOCALFILE, &sbuf) == 0) {
		_map_byname.m_version = sbuf.st_mtim.tv_sec;
	} else {
		accessfailed=1;
	}

	status = ns_lookup(&_map_byname, 0, "rpc.byname", name, 0,
	    buffer, buflen);
	if (status == NS_SUCCESS) {
		if (strtorpcent(entry, buffer, buflen)) {
			return entry;
		}
	}

	if (status == NS_NOTFOUND || accessfailed) {
		return (struct rpcent *)0;
	}

	/*
	** Fallback to local files.
	*/
	if (!setrpcent_files(0)) {
		return (struct rpcent *)0;
	}
		
	while (ent = getrpcent_r(entry, buffer, buflen)) {
		if (strcmp(ent->r_name, name) == 0) {
			break;
		}
		for (cp = ent->r_aliases; *cp; cp++) {
			if (strcmp(*cp, name) == 0) {
				goto found;
			}
		}
	}
found:
	if (! _map_byname.m_stayopen) {
		endrpcent();
	}

	return ent;
}

struct rpcent *
getrpcbyname(const char *name)
{
	if (! name || ! *name) {
		return (struct rpcent *)0;
	}
	if (! _entry && ! init_entry()) {
		return (struct rpcent *)0;
	}
	if (! _buffer && ! init_buffer()) {
		return (struct rpcent *)0;
	}

	return getrpcbyname_r(name, _entry, _buffer, LOCALBUFLEN);
}

struct rpcent *
getrpcbynumber_r(int rpc, struct rpcent *entry, char *buffer, int buflen)
{
	int status;
	int accessfailed=0;
	char key[16];
	struct rpcent *ent;
	struct stat sbuf;

	if (! entry || ! buffer) {
		return (struct rpcent *)0;
	} 

	_ltoa((long)rpc, key);

	if (! _getXbyY_no_stat && stat(LOCALFILE, &sbuf) == 0) {
		_map_bynumber.m_version = sbuf.st_mtim.tv_sec;
	} else {
		accessfailed=1;
	}

	status = ns_lookup(&_map_bynumber, 0, "rpc.bynumber", key, 0,
	    buffer, buflen);
	if (status == NS_SUCCESS) {
		if (strtorpcent(entry, buffer, buflen)) {
			return entry;
		}
	}

	if (status == NS_NOTFOUND || accessfailed) {
		return (struct rpcent *)0;
	}

	/*
	** Fallback to local files.
	*/
	if (! setrpcent_files(0)) {
		return (struct rpcent *)0;
	}

	while (((ent = getrpcent_r(entry, buffer, buflen)) != NULL) &&
	    (rpc != ent->r_number));
	if (! _map_bynumber.m_stayopen) {
		endrpcent();
	}

	return ent;
}

struct rpcent *
getrpcbynumber(int rpc)
{
	if (! _entry && ! init_entry()) {
		return (struct rpcent *)0;
	}
	if (! _buffer && ! init_buffer()) {
		return (struct rpcent *)0;
	}

	return getrpcbynumber_r(rpc, _entry, _buffer, LOCALBUFLEN);
}
