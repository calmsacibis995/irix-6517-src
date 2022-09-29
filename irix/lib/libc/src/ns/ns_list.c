#ifdef __STDC__
	#pragma weak ns_list = _ns_list
#endif

#include "synonyms.h"
#include <stdio.h>
#include <unistd.h>
#include <sys/param.h>
#include <fcntl.h>
#include <ns_api.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <alloca.h>

/* ARGSUSED */
FILE *
_ns_list(ns_map_t *map, char *domain, char *table, char *library) {
	char nbuf[MAXPATHLEN], *local_table, *p, *q;
	int fd, count;
	FILE *fp;

	/*
	** sanity checking
	*/
	if (!table || !*table) {
		return (FILE *)0;
	}
	if (strchr(table, '/') || (domain && strchr(domain, '/')) ||
	    (library && strchr(library, '/'))) {
		return (FILE *)0;
	}

	/* 
	** Replace  ':' in table with '/' 
	*/
	local_table = alloca((unsigned) strlen(table) + 1);
	for (p = local_table, q = table; q && *q; q++) {
		*p++ = (*q == ':') ? '/' : *q;
	}
	*p = '\0';

	/*
	** Build file name.
	*/
	if (! domain || ! *domain) {
		domain = NS_DOMAIN_LOCAL;
	}

	if (library && *library) {
		sprintf(nbuf, "%s/%s/%s/.%s/.all", NS_MOUNT_DIR, domain, 
			local_table, library);
	} else {
		sprintf(nbuf, "%s/%s/%s/.all", NS_MOUNT_DIR, domain, 
			local_table);
	}

	/*
	** The first request after the server times out the file we will
	** get back a stale file handle, retrying immediately should succeed.
	*/
	count = 0;
nslist_loop:
	fd = open(nbuf, O_RDONLY);
	if (fd < 0) {
		if ((errno == ESTALE) && (count++ < 4)) {
			goto nslist_loop;
		}
		if (errno == EIO) {
			usleep(100000);
			goto nslist_loop;
		}
		if (errno == ENOENT && map && map->m_flags & NS_MAP_DYNAMIC) {
                        char *p;
                        /*
                        ** If the entry doesn't exist, try a mkdir
                        ** to create the new map.
                        */

                        p = strrchr(nbuf,'/');
                        if (!p) 
                                return (FILE *)0;
                        *p = '\0';
                        if (mkdir(nbuf, 0777) < 0) {
                                return (FILE *)0;
			}
                        *p = '/';
                        goto nslist_loop;
                }
	}

	if (fd < 0) {
		return (FILE *)0;
	}

	fp = fdopen(fd, "r");

	return fp;
}
