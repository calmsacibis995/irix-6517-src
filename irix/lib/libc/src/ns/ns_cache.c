#include "synonyms.h"
#include <stdio.h>
#include <fcntl.h>
#include <mutex.h>
#include <mdbm.h>
#include <ns_api.h>
#include <sys/param.h>
#include <errno.h>
#include "compare.h"

#ifdef __STDC__
	#pragma weak ns_close = __ns_cache_close
#endif

/*
** This routine will map in the cache file, and fill in the map if it
** was not already open.
*/
int
__ns_cache_open(ns_map_t *map, char *name)
{
	char buf[MAXPATHLEN];
	int len;
	MDBM *mtmp;

	if (! map || ! name || ! *name) {
		return 0;
	}
	len = (int)strlen(name);
	if (len >= (MAXPATHLEN - sizeof NS_CACHE_DIR)) {
		return 0;
	}

	strcpy(buf, NS_CACHE_DIR);
	buf[sizeof(NS_CACHE_DIR) - 1] = '/';
	strcpy(buf + sizeof(NS_CACHE_DIR), name);

	/*
	** If this mdbm file has been invalidated
	** Then close it (since fetches will fail.  Try to reopen it.
	** in the case there is a new valid mdbm file there
	*/
	if (map->m_map && _Mdbm_invalid(map->m_map)) {
		mtmp = (MDBM *)test_and_set((u_long *)&map->m_map, 0);
		if (mtmp) {
			mdbm_close(mtmp);
		}
	}

	if (! map->m_map) {
		mtmp = mdbm_open(buf, O_RDONLY, 0400, 0);
		if (! mtmp) {
			return 0;
		}

		/*
		** Close off the file descriptors in the mdbm header.
		** We don't need a file descriptor on a fixed size
		** database.
		*/
		mdbm_close_fd(mtmp);

		if (! __compare_and_swap((long *)&map->m_map, 0, (long)mtmp)) {
			/* Race condition. */
			mdbm_close(mtmp);
		}
	}
		

	return 1;
}

/*
** This routine simply closes an MDBM file that is open.
*/
int
__ns_cache_close(ns_map_t *map)
{
	MDBM *mtmp;

	if (map->m_map) {
		mtmp = (MDBM *)test_and_set((u_long *)&map->m_map, 0);
		if (mtmp) {
			mdbm_close(mtmp);

			return 1;
		}
	}

	return 0;
}
