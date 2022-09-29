#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <ns_api.h>
#include <ns_daemon.h>

extern long sginap(long);

static ns_lock_t *_nsd_locks;

/*
** This is a wrapper arround aquire_lock since calls to spin_lock
** can hang.
*/
static int
nsd_lock_get(abilock_t *lock)
{
	int i;

	for (;;) {
		for (i = 0; i < 10; i++) {
			if (!acquire_lock(lock) || !acquire_lock(lock)) {
				return NSD_OK;
			}
			sginap(0);
		}
		release_lock(lock);
	}
}

/*
** This routine will open up the lock file, and map it into our context.
** The file just contains a null terminated array of ns_lock_t structures.
*/
static int
nsd_init_locks(void)
{
	mode_t old;
	int fd;

	/*
	** This file needs to be world writable since it is the lock
	** definitions file for all lamed cache changes.
	*/
	old = umask(0);
	fd = open(NS_LOCK_FILE, O_RDWR | O_CREAT, 0666);
	umask(old);
	if (fd < 0) {
		nsd_logprintf(0,
		    "nsd_init_locks: unable to open lock file: %s\n",
		    strerror(errno));
		return NSD_ERROR;
	}

	_nsd_locks = (ns_lock_t *)nsd_mmap(0, LOCK_FILE_SIZE,
	    PROT_READ | PROT_WRITE, MAP_SHARED | MAP_AUTOGROW, fd, 0);
	if ((int)_nsd_locks < 0) {
		nsd_logprintf(0, "nsd_init_locks: unable to map file: %s\n",
		    strerror(errno));
		close(fd);
		return NSD_ERROR;
	}

	/*
	** mmap holds a reference to the file so we can close the file.
	*/
	close(fd);

	return NSD_OK;
}

/*
** This routine will step through the locks from the lock file to find
** the lock for this map, then call lock_get to spin on it.
*/
int
nsd_lock_map(nsd_maps_t *map)
{
	ns_lock_t *l;
	char *s;
	unsigned len;

	if (! _nsd_locks) {
		if (! nsd_init_locks()) {
			return NSD_ERROR;
		}
	}
	if (map->m_lock) {
		return (nsd_lock_get(map->m_lock));
	}
	for (s = (char *)_nsd_locks, l = (ns_lock_t *)s; l->l_len != 0;
	    s += l->l_len, l = (ns_lock_t *)s) {
		if (strcasecmp(l->l_name, map->m_name) == 0) {
			map->m_lock = &l->l_lock;
			return nsd_lock_get(&l->l_lock);
		}
	}

	l->l_version = 0;
	strcpy(l->l_name, map->m_name);
	map->m_lock = &l->l_lock;
	init_lock(&l->l_lock);

	len = strlen(map->m_name) + 1;
	l->l_len = sizeof(ns_lock_t) + len;
	l->l_len += (4 - (len % 4));
	if ((s + l->l_len) > (((char *)_nsd_locks) + LOCK_FILE_SIZE)) {
		return NSD_ERROR;
	}

	return nsd_lock_get(&l->l_lock);
}

/*
** This just unlocks a locked cache file.
*/
int
nsd_unlock_map(nsd_maps_t *map)
{
	if (! map || ! map->m_lock) {
		return NSD_OK;
	}
	release_lock(map->m_lock);

	return NSD_OK;
}
