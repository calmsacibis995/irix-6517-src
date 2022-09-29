#include <stdio.h>
#include <limits.h>
#include <fcntl.h>
#include <abi_mutex.h>
#include <time.h>
#include <ctype.h>
#include <malloc.h>
#include <alloca.h>
#include <mdbm.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <unistd.h>
#include <errno.h>
#include <ns_api.h>
#include <ns_daemon.h>

/* This cachesize parameter is 2^n pages */
#define NSD_DEFAULT_CACHESIZE 4

nsd_maps_t *__nsd_maps;

/*
** This routine is called by the database code when a datum
** will not fit on a page to determine if the requested item
** can be deleted.  It is called three times with increasing
** priority values.  On the first pass we throw away items which
** have timed out.  On the second pass items which represent
** failed attempts.  And, finally, we throw anything away on
** the third pass.
*/
/* ARGSUSED */
static int
nsd_cache_shake(MDBM *db, datum key, datum value, void *pri)
{
	ns_cache_t nc;
	int i;
	time_t now;

	memcpy(&nc, value.dptr, sizeof(nc));
	i = (int)pri;
	time(&now);
	if (i == 0) {
		if (now > nc.c_timeout) {
			return 1;
		}
	} else if (i == 1) {
		if (nc.c_status != NS_SUCCESS) {
			return 1;
		}
	} else {
		return 1;
	}

	return 0;
}

/*
** This is the shake routine for the cache files.  We just close off
** maps that have not been touched lately.
*/
void
nsd_map_shake(int priority, time_t now)
{
	nsd_maps_t *mp;

	now -= (10 - priority) * 30;
	for (mp = __nsd_maps; mp; mp = mp->m_next) {
		if (mp->m_map && (mp->m_touched < now)) {
			mdbm_close(mp->m_map);
			mp->m_map = (MDBM *)0;
		}
	}
}

/*
** This will open the cache file for a given table and set the
** permission according to the attributes.
*/
int
nsd_map_open(nsd_file_t *file)
{
	long mode, owner, group, pagesize, cachesize;
	int len;
	char filename[MAXPATHLEN];

	if (! file) {
		return NSD_ERROR;
	}
	if (! file->f_map) {
		return NSD_ERROR;
	}
	if (file->f_map->m_map) {
		time(&file->f_map->m_touched);
		return NSD_OK;
	}

	/*
	** Lookup default values for cache parameters.
	*/
	mode = nsd_attr_fetch_long(file->f_attrs, "mode", 8, 0644);
	owner = nsd_attr_fetch_long(file->f_attrs, "owner", 10, geteuid());
	group = nsd_attr_fetch_long(file->f_attrs, "group", 10, getegid());
	pagesize = nsd_attr_fetch_long(file->f_attrs, "pagesize", 10, 0);
	cachesize = nsd_attr_fetch_long(file->f_attrs, "cachesize", 10, 
					NSD_DEFAULT_CACHESIZE); 

	/*
	** Build filename.
	*/
	if (! file->f_map->m_file) {
		len = nsd_cat(filename, MAXPATHLEN, 3, NS_CACHE_DIR, "/",
		    file->f_map->m_name);
		file->f_map->m_file = (char *)nsd_malloc(len + 1);
		if (! file->f_map->m_file) {
			nsd_logprintf(NSD_LOG_RESOURCE,
			    "nsd_map_open: failed malloc\n");
			return NSD_ERROR;
		}
		memcpy(file->f_map->m_file, filename, len);
		file->f_map->m_file[len] = (char)0;
	}

	/*
	** Check numeric attributes
	*/
	if (pagesize && ((pagesize < 8) || (pagesize > 16))) {
		nsd_logprintf(NSD_LOG_RESOURCE,
		    "nsd_map_open: invalid pagesize %ld, valid range 0,8-16\n",
		    pagesize);
		pagesize=0;
	}

	file->f_map->m_map =
		nsd_mdbm_open(file->f_map->m_file, 
			      O_RDWR | O_CREAT | MDBM_ALLOC_SPACE |
			      MDBM_MEMCHECK, 
			      mode, pagesize);
	if (! file->f_map->m_map) {
		nsd_logprintf(NSD_LOG_OPER, 
		    "nsd_map_open: failed to open cache file %s\n",
		    file->f_map->m_file);
		return NSD_ERROR;
	}

	/*
	** Extend the cache to the default size, and make sure that it
	** is aligned to a time_t.  If it is fixed size we can also close
	** the file since it will never need to be remapped.
	*/
	if (cachesize) {
		if (cachesize < 3 || cachesize > 16) {
			nsd_logprintf(NSD_LOG_RESOURCE, 
		    "nsd_map_open: invalid cachesize %ld, valid range 0,3-16\n",
				      cachesize);
			cachesize=NSD_DEFAULT_CACHESIZE;
		}

		if ((mdbm_pre_split(file->f_map->m_map, cachesize, 
				   MDBM_ALLOC_SPACE) < 0 ) &&
		    (errno != EEXIST)) {
			if (!(file->f_map->m_flags & NSD_MAP_ALLOC)) {
				nsd_logprintf(NSD_LOG_RESOURCE, 
				    "failed to allocate space for %s cache\n", 
					      file->f_map->m_name);
				file->f_map->m_flags |= NSD_MAP_ALLOC;
			}
			/*
			** If we cant allocate the space, then delete it.
			** we dont want it sitting around for others to use
			** and have it act as a normal (growing) mdbm file
			*/
			mdbm_invalidate(file->f_map->m_map);
			mdbm_close(file->f_map->m_map);
			file->f_map->m_map = (MDBM *)0;
			unlink(file->f_map->m_file);
			return NSD_ERROR;
		}
		file->f_map->m_flags &= ~NSD_MAP_ALLOC;
		mdbm_limit_size(file->f_map->m_map, cachesize, nsd_cache_shake);
	}

	/*
	** Fix permissions.  We do this everytime since these may be
	** different than when the file was first created.
	*/
	chown(file->f_map->m_file, (uid_t)owner, (gid_t)group);
	chmod(file->f_map->m_file, (mode_t)mode & 0666);

	time(&file->f_map->m_touched);
	return NSD_OK;
}

/*
** This just searches through a map list until it gets a matching entry.
*/
nsd_maps_t *
nsd_map_get(char *map)
{
	nsd_maps_t *nm;
	int len;

	for (nm = __nsd_maps; nm; nm = nm->m_next) {
		if (strcasecmp(map, nm->m_name) == 0) {
			return nm;
		}
	}

	len = strlen(map);
	nm = (nsd_maps_t *)nsd_calloc(1, sizeof(nsd_maps_t) + len);
	if (! nm) {
		nsd_logprintf(NSD_LOG_RESOURCE, "nsd_map_get: malloc failed\n");
		return nm;
	}

	memcpy(nm->m_name, map, len);
	nm->m_name[len] = 0;

	nm->m_next = __nsd_maps;
	__nsd_maps = nm;

	return nm;
}

/*
** This will update a cache entry for this element.  If we had a
** successful lookup then we will always update the cache.  If we
** had some sort of an error then we will only update the cache if
** the key did not exist or has timed out.
*/
int
nsd_map_update(nsd_file_t *rq)
{
	char *buf;
	ns_cache_t *cp;
	kvpair kv;
	time_t now, then;
	long timeout;
	int status;
	int pagesize;
	char *val;

	nsd_logprintf(NSD_LOG_LOW, "nsd_map_update:\n");

	timeout = nsd_attr_fetch_long(rq->f_attrs, "timeout", 10, 300);
	if (rq->f_status != NS_SUCCESS) {
		timeout = nsd_attr_fetch_long(rq->f_attrs, "negative_timeout", 
					      10, timeout);
	}		
	if (timeout < 0) {
		timeout=0;
	}

	if (! timeout || ! rq->f_map) {
		return NSD_OK;
	}

	kv.key.dptr = rq->f_name;
	kv.key.dsize = rq->f_namelen;

	if (nsd_map_open(rq) != NSD_OK) {
		nsd_logprintf(NSD_LOG_MIN, "\tcould not open map\n");
		return NSD_ERROR;
	}

	pagesize=MDBM_PAGE_SIZE(rq->f_map->m_map);
	val=alloca(pagesize);
	kv.val.dptr=val;
	kv.val.dsize=pagesize;

	time(&now);

	if ((rq->f_status != NS_SUCCESS) && (rq->f_status != NS_NOTFOUND)) {
		if (_Mdbm_invalid(rq->f_map->m_map)) {
			mdbm_close(rq->f_map->m_map);
			rq->f_map->m_map = (MDBM *)0;
			if (nsd_map_open(rq) != NSD_OK) {
				nsd_logprintf(NSD_LOG_MIN,
				    "\tcould not reopen map\n");
				return NSD_ERROR;
			}
		}
		kv.val = mdbm_fetch(rq->f_map->m_map, kv);
		if (!kv.val.dptr && (errno == EDEADLK ||
				     errno == EBADF)) {
			nsd_logprintf(NSD_LOG_OPER,
			    "MDBM corruption detected, flushing %s\n",
			    rq->f_map->m_name);
			if (nsd_map_flush(rq) != NSD_OK) {
				return NSD_ERROR;
			}
			kv.val.dptr=val;
			kv.val.dsize=pagesize;
			kv.val = mdbm_fetch(rq->f_map->m_map, kv);
		}
			
		if (kv.val.dptr) {
			GETLONG(then, kv.val.dptr);
			status = *kv.val.dptr;
			if ((then > now) && (status == NS_SUCCESS)) {
				nsd_logprintf(NSD_LOG_LOW,
				    "Valid entry already exists.\n");
				return NSD_OK;
			}
		}
	}

	/*
	** Get space on the stack, then align a pointer to the next long.
	*/
	buf = alloca(rq->f_used + 8);
	cp = (ns_cache_t *)(buf + (sizeof(time_t) -
	    ((int)buf % sizeof(time_t))));

	cp->c_timeout = now + timeout;
	cp->c_mtime = now;
	cp->c_status = (char)rq->f_status;
	if (rq->f_data) {
		memcpy(cp->c_data, rq->f_data, rq->f_used);
	}

	kv.val.dptr = (char *)cp;
	kv.val.dsize = rq->f_used + (2 * sizeof(time_t)) + sizeof(char);
	if (_Mdbm_invalid(rq->f_map->m_map)) {
		mdbm_close(rq->f_map->m_map);
		rq->f_map->m_map = (MDBM *)0;
		if (nsd_map_open(rq) != NSD_OK) {
			nsd_logprintf(NSD_LOG_MIN, "\tcould not reopen map\n");
			return NSD_ERROR;
		}
	}
		
	if (mdbm_store(rq->f_map->m_map, kv.key, kv.val, MDBM_REPLACE) < 0) {
		if (errno == EDEADLK || errno == EBADF) {
			nsd_logprintf(NSD_LOG_OPER,
			    "MDBM corruption detected, flushing %s\n",
			    rq->f_map->m_name);
			if (nsd_map_flush(rq) != NSD_OK) {
				return NSD_ERROR;
			}
			if (mdbm_store(rq->f_map->m_map, kv.key, kv.val, 
				       MDBM_REPLACE) < 0) {
				nsd_logprintf(NSD_LOG_OPER, 
					      "Failed 2nd store: %s\n", 
					      strerror(errno));
			}
		}
		nsd_logprintf(NSD_LOG_LOW, "Failed store: %s\n",
		    strerror(errno));
	}

	return NSD_OK;
}

/*
** This routine will free up all the space for a given list of maps, and
** all of their children.
*/
void
nsd_map_close_all(void)
{
	nsd_maps_t *nm;

	for (nm = __nsd_maps; nm; nm = nm->m_next) {
		if (nm->m_map) {
			mdbm_close(nm->m_map);
			nm->m_map = (MDBM *)0;
		}
	}
}

void
nsd_map_delete(nsd_maps_t *map, char *string, int len)
{
	datum key;

	key.dptr = string;
	key.dsize = len;

	mdbm_delete(map->m_map, key);
}

int
nsd_map_remove(nsd_file_t *rq)
{
	if (! rq) {
		return NSD_ERROR;
	}

	if (nsd_map_open(rq) != NSD_OK) {
		return NSD_ERROR;
	}
	
	nsd_map_delete(rq->f_map, rq->f_name, rq->f_namelen);

	return NSD_OK;
}

int
nsd_map_flush(nsd_file_t *rq)
{
	nsd_logprintf(NSD_LOG_MIN, "Enterring nsd_map_flush:\n");

	if (! rq) {
		return NSD_ERROR;
	}

	if (nsd_map_open(rq) != NSD_OK) {
		return NSD_ERROR;
	}

	nsd_logprintf(NSD_LOG_LOW, "\tflushing map: %s\n", rq->f_map->m_file);
	mdbm_invalidate(rq->f_map->m_map);
	mdbm_close(rq->f_map->m_map);
	rq->f_map->m_map = (MDBM *)0;
	
	return (nsd_map_open(rq));
}

int
nsd_map_unlink(nsd_file_t *rq) 
{
	nsd_logprintf(NSD_LOG_LOW, "\tFlushing and unlinking map: %s\n", 
		      rq->f_map->m_file);
	mdbm_invalidate(rq->f_map->m_map);
	mdbm_close(rq->f_map->m_map);
	rq->f_map->m_map = (MDBM *)0;
	unlink(rq->f_map->m_file);
	return NSD_OK;
}
