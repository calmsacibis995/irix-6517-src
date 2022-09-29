#include "synonyms.h"
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <ns_api.h>

static char *_ns_locks;
#define WORDS(l)	(((l) % 4) != 0) ? (((l) + (4 - ((l) % 4)))/sizeof(uint32_t)) : ((l)/sizeof(uint32_t))

/*
** This is a wrapper arround aquire_lock since calls to spin_lock
** can hang.  In the client code we give up after a while and just
** return an error.  This will force the lookup routine to call out
** to the miss handler which can reset the lock.
*/
static int
lock_get(abilock_t *lock)
{
	int i;

	for (i = 0; i < 10; i++) {
		if (!acquire_lock(lock) || !acquire_lock(lock)) {
			return 1;
		}
		sginap(i);
	}

	return 0;
}

/*
** This routine will open up the lock file, and map it into our context.
*/
static int
init_locks(void)
{
	int fd;

	fd = open(NS_LOCK_FILE, O_RDWR);
	if (fd < 0) {
		return 0;
	}

	_ns_locks = (char *)mmap(0, LOCK_FILE_SIZE, PROT_READ | PROT_WRITE,
	    MAP_SHARED, fd, 0);
	close(fd);
	if ((long)_ns_locks < 0) {
		return 0;
	}

	return 1;
}

/*
** This routine will step through the locks from the lock file to find
** the lock for this map, then call lock_get to spin on it.
*/
int
__ns_map_lock(ns_map_t *map, char *name)
{
	char *lp;
	ns_lock_t *l;

	if (! map) {
		return 0;
	}
	if (! _ns_locks && ! init_locks()) {
		return 0;
	}

	if (map->m_lock) {
		return lock_get(map->m_lock);
	}

	if (! name) {
		return 0;
	}
	for (lp = _ns_locks, l = (ns_lock_t *)lp; l && l->l_len;
	    lp += l->l_len, l = (ns_lock_t *)lp) {
		if (strcasecmp(name, l->l_name) == 0) {
			map->m_lock = &l->l_lock;
			return lock_get(map->m_lock);
		}
	}

	return 0;
}

int
__ns_map_unlock(ns_map_t *map)
{
	if (! _ns_locks || ! map || ! map->m_lock) {
		return 0;
	}

	return release_lock(map->m_lock);
}
