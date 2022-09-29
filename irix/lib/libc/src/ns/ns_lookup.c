#ifdef __STDC__
	#pragma weak ns_lookup = _ns_lookup
#endif

#include "synonyms.h"
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/param.h>
#include <mdbm.h>
#include <ns_api.h>
#include <errno.h>

int _getXbyY_no_stat _INITBSS;

extern int __ns_cache_open(ns_map_t *, char *);

#define NS_MAX_KEY	512

/*
** This is the lookup wrapper routine.  We look in the appropriate cache,
** if we do not find it, or the cache element is out of date then we ask
** the name service daemon to add it to the cache.
*/
int
ns_lookup(ns_map_t *map, char *domain, char *table, const char *name,
    char *library, char *buf, size_t len)
{
	char nbuf[MAXPATHLEN], *p, *q;
	kvpair kv;
	time_t now;
	uint32_t then, mtime;
	int status = 0;
	size_t dlen, klen;
	int fd, count;

	/*
	** sanity checking
	*/
	if (!name || !*name || !table || !*table || !map || !buf) {
		return NS_BADREQ;
	}
	klen = (int)strlen(name);
	dlen = (domain && *domain) ? strlen(domain) : 0;
	if ((klen + dlen + strlen(table)) > NS_MAX_KEY) {
		return NS_BADREQ;
	}

	if (strchr(name, '/') || strchr(table, '/') ||
	    (domain && strchr(domain, '/')) ||
	    (library && strchr(library, '/'))) {
		return NS_BADREQ;
	}

	/*
	** check cache
	*/
	if (!(library && *library) && __ns_cache_open(map, table)) {
		/*
		** The key in the database is the item we are looking up
		** followed by a null, followed by the domain.
		*/
		strcpy(nbuf, name);
		kv.key.dsize = (int)klen;
		if (dlen) {
			nbuf[kv.key.dsize] = (char)0;
			strcpy(&nbuf[kv.key.dsize + 1], domain);
			kv.key.dsize += (dlen + 1);
		}
		kv.key.dptr = nbuf;

		/*
		** need to provide space for the copyout
		*/

		kv.val.dptr = buf;
		kv.val.dsize = (int)len;
		/*
		** Look up the key in our local cache file if it exists.
		*/
		kv.val = mdbm_fetch(map->m_map, kv);
		count = (int)(kv.val.dsize - 
			      ((2*sizeof(time_t)) + sizeof(char)));

		/*
		** Check to make sure the data is alright.
		** Dont look at values that are not big enough to hold
		** the header info (then, mtime, status)
		*/
		if (kv.val.dptr && (count >= 0)) {
			GETLONG(then, kv.val.dptr);
			GETLONG(mtime, kv.val.dptr);
			status = *kv.val.dptr++;
			if ((map->m_version < (long)mtime) &&
			    ((status == NS_SUCCESS) || 
			     (status == NS_NOTFOUND))) {
				time(&now);
				if (then > (long)now) {
					memmove(buf, kv.val.dptr, count);
					buf[count] = (char)0;
					return status;
				}
			}
		}
	}

	/*
	** Read file.  We first test the existence of the map so we can
	** tell the difference between not running the daemon, and a key
	** which does not exist.
	*/
	if (! domain || ! *domain) {
		domain = NS_DOMAIN_LOCAL;
	}
	for (p = nbuf, q = NS_MOUNT_DIR; *q; *p++ = *q++);
	*p++ = '/';
	for (q = domain; *q; *p++ = *q++);
	*p++ = '/';
	/* Copy table, replacing ':' with '/' */
	for (q = table; *q; q++) {
		*p++ = (*q == ':') ? '/' : *q;
	}
	*p = (char)0;

	for (count = 4; count > 0; count--) {
		if (access(nbuf, X_OK|EFF_ONLY_OK) != 0) {
			if ((errno == ESTALE) || (errno == EIO)) {
				continue;
			}
			if ((errno == ENOENT) && 
			    (map->m_flags & NS_MAP_DYNAMIC)) { 
                                /*
                                ** If this is a dynamic map, try a mkdir
                                ** to create the new map.  Then continue
				** the loop to retry the access.
                                */
                                if (mkdir(nbuf, 0777) < 0) {
                                        return NS_NOTFOUND;
				}
				continue;
                        }
			return NS_FATAL;
		}
		break;
	}

	if (library && *library) {
		*p++ = '/';
		*p++ = '.';
		for (q = library; *q; *p++ = *q++);
	}

	*p++ = '/';
	for (q = (char *)name; *q; *p++ = *q++);
	*p = (char)0;

	/*
	** The first request that we make after the filehandle has been
	** timed out in the server will return ESTALE.  Retrying should
	** succeed or return a different error.  The server returns EIO
	** when multiple clients are simultaneously making the same request
	** or the server is in resource starvation.  We should wait for
	** a short period of time and try again.
	*/
	for (count = 4; count > 0; ) {
		fd = open(nbuf, O_RDONLY);
		if (fd < 0) {
			if (errno == ESTALE) {
				count--;
				continue;
			}
			if (errno == EIO) {
				usleep(100000);
				continue;
			}
			return NS_NOTFOUND;
		}

		status = (int)read(fd, buf, len);
		if (status < 0) {
			close(fd);
			if ((errno == ESTALE) || (errno == EIO)) {
				count--;
				continue;
			}
			return NS_FATAL;
		}
		buf[(len > status) ? status : len - 1] = 0;
		close(fd);

		return NS_SUCCESS;
	}

	return NS_FATAL;
}
