#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <fcntl.h>
#include <alloca.h>
#include <ctype.h>
#include <db.h>
#include <ns_api.h>
#include <ns_daemon.h>

extern int _ltoa(long, char *);

typedef struct dbhash_file {
	char			*domain;
	char			*table;
	char			*file;
	time_t			version;
	time_t			touched;
	nsd_attr_t		*attrs;	
	int			fd;
	DB			*map;
	struct dbhash_file	*next;
} dbhash_file_t;

typedef struct {
	DBT key;
	DBT val;
} dbpair;

static dbhash_file_t *__dbhash_files = (dbhash_file_t *)0;

int dbhash_db_lock(dbhash_file_t *);
int dbhash_db_unlock(dbhash_file_t *);

/*
** We cache open files so that we will not have to reopen the map on each
** call.  This routine just seaches the list for a matching dbhash file, and
** creates a new entry if not found.
*/
static dbhash_file_t *
dbhash_get_file(nsd_file_t *rq)
{
	dbhash_file_t *hf;
	char fbuf[PATH_MAX];
	int len;
	char *domain, *file, *table;

	/*
	** There is one dbhash file for each table in each domain, so
	** here we determine which table and domain this request is
	** for.
	*/
	domain = nsd_attr_fetch_string(rq->f_attrs, "domain", 0);
	table = nsd_attr_fetch_string(rq->f_attrs, "table", 0);
	if (! table) {
		rq->f_status = NS_BADREQ;
		return NSD_ERROR;
	}

	/*
	** Seatch through the list for a matching structure.
	*/
	for (hf = __dbhash_files; hf; hf = hf->next) {
		if (((! domain && ! hf->domain) ||
		    (! *domain && ! hf->domain) ||
		    (strcasecmp(domain, hf->domain) == 0)) &&
		    (strcasecmp(table, hf->table) == 0)) {
			time(&hf->touched);
			return hf;
		}
	}

	/*
	** We did not find it in the list so we create a new list
	** entry.  The file can be overridden on a per table basis
	** so here we check if the filename has been set for the
	** table.
	*/
	file = nsd_attr_fetch_string(rq->f_attrs, "file", 0);

	/*
	** Allocate some space.  Note that the nsd_calloc routine
	** may result in a call to shake which walks this list.
	*/
	hf = (dbhash_file_t *)nsd_calloc(1, sizeof(dbhash_file_t));
	if (! hf) {
		nsd_logprintf(NSD_LOG_OPER, "dbhash_open: failed malloc\n");
		return (dbhash_file_t *)0;
	}

	/*
	** Copy the domain name into the dbhash structure.
	*/
	if (domain) {
		hf->domain = nsd_strdup(domain);
		if (! hf->domain) {
			nsd_logprintf(NSD_LOG_OPER, "dbhash_open: failed malloc\n");
			free(hf);
			return (dbhash_file_t *)0;
		}
	}

	/*
	** Copy the table name into the dbhash structure.
	*/
	hf->table = nsd_strdup(table);
	if (! hf->table) {
		nsd_logprintf(NSD_LOG_OPER, "dbhash_open: failed malloc\n");
		free(hf->domain);
		free(hf);
		return (dbhash_file_t *)0;
	}

	/*
	** Figure out the source file from the file and domain attributes,
	** the requested domain and table, or any combination of the above.
	*/
	if (file) {
		if (*file == '/') {
			strcpy(fbuf, file);
			len = strlen(fbuf);
		} else {
			if (domain) {
				len = nsd_cat(fbuf, PATH_MAX, 6, NS_DOMAINS_DIR,
				    "/", domain, "/", file, ".db");
			} else {
				len = nsd_cat(fbuf, PATH_MAX, 3, "/etc/", file,
				    ".db");
			}
		}
	} else {
		if (domain) {
			len = nsd_cat(fbuf, PATH_MAX, 6, NS_DOMAINS_DIR,
			    "/", domain, "/", table, ".db");
		} else {
			len = nsd_cat(fbuf, PATH_MAX, 3, "/etc/", table, ".db");
		}
	}

	/*
	** Copy the filename into the dbhash structure.
	*/
	hf->file = (char *)nsd_malloc(len);
	if (! hf->file) {
		nsd_logprintf(NSD_LOG_OPER, "dbhash_open: failed malloc\n");
		free(hf->domain);
		free(hf->table);
		free(hf);
		return (dbhash_file_t *)0;
	}
	memcpy(hf->file, fbuf, len);
	hf->file[len] = (char)0;

	time(&hf->touched);

	/*
	** Save the pointer to the attributes
	*/

	hf->attrs = rq->f_attrs;

	return hf;
}

/*
** This routine will attempt to open the dbhash file represented by this
** structure.
*/
static int
dbhash_reopen(dbhash_file_t *hf)
{
	char *dbtype_attr;
	DBTYPE dbtype;
	struct stat sbuf;

	if (stat(hf->file, &sbuf) < 0) {
		return NSD_ERROR;
	}
	if (hf->map && hf->version == sbuf.st_mtime) {
		return NSD_OK;
	}
	hf->version = sbuf.st_mtime;

	/*
	** Close the map if it were already open.
	*/
	if (hf->map) {
		(hf->map->close)(hf->map);
		hf->map = 0;
	}

	/* 
	** Get the dbtype attr 
	*/
	dbtype_attr = nsd_attr_fetch_string(hf->attrs,"db_hash", "btree");
	if (dbtype_attr) {
		if (!strncasecmp(dbtype_attr,"btree",5)) 
			dbtype=DB_BTREE;
		else if (!strncasecmp(dbtype_attr,"hash",4)) 
			dbtype=DB_HASH;
		else if (!strncasecmp(dbtype_attr,"recno",5)) 
			dbtype=DB_RECNO;
		else {
			return NSD_ERROR;
		}
	} else {
		dbtype=DB_BTREE;
	}

	/*
	** Open it up.
	**
	** If the initial open fails, call nsd_shake, then try again
	*/
	hf->map = dbopen(hf->file, O_RDONLY, 0666, dbtype, NULL);
	if (!hf->map) {
		nsd_shake(9);
		hf->map = dbopen(hf->file, O_RDONLY, 0666, dbtype, NULL);
	}

	if (hf->map) {
		if ((hf->fd = (hf->map->fd)(hf->map)) > 0)
			return NSD_OK;
	}

	return NSD_ERROR;
}

/*
** This routine walks the dbhash table list freeing each element.  We
** close any files which may be open.
*/
static void
dbhash_clear(void)
{
	dbhash_file_t *hf;

	for (hf = __dbhash_files; hf; hf = __dbhash_files) {
		__dbhash_files = hf->next;

		free(hf->file);
		free(hf->table);
		if (hf->domain) {
			free(hf->domain);
		}
		if (hf->map) {
			(hf->map->close)(hf->map);
		}
		free(hf);
	}

	/* 
	** clear for pointer so we dont access bogus memory
	*/
	__dbhash_files = (dbhash_file_t *)0;
}

/*
** This routine simply closes and unmaps a dbhash file.
*/
static void
dbhash_close(dbhash_file_t *hf) {
	if (hf->map) {
		(hf->map->close)(hf->map);
		hf->map = (DB *)0;
		hf->fd = -1;
	}
}

/*
** The init function is called when the library is first opened or when
** the daemon receives a signal.  We just set the state back to startup
** by freeing all the cached files.
*/
/* ARGSUSED */
int
init(char *map)
{
	dbhash_clear();

	return NSD_OK;
}

/*
** This routine simply returns the requested key from the dbhash file for
** the requested domain and table.
*/
int
lookup(nsd_file_t *rq)
{
	dbhash_file_t *hf;
	DBT key, val;
	int err;

	if (! rq) {
		return NSD_ERROR;
	}

	/*
	** Lookup the file information in our cache.
	*/
	hf = dbhash_get_file(rq);
	if (! hf) {
		rq->f_status = NS_NOTFOUND;
		return NSD_NEXT;
	}
	
	if (! dbhash_reopen(hf)) {
		rq->f_status = NS_NOTFOUND;
		return NSD_NEXT;
	}

	/*
	** Determine the key for the lookup.
	*/
	key.data = nsd_attr_fetch_string(rq->f_attrs, "key", 0);
	if (! key.data) {
		rq->f_status = NS_BADREQ;
		return NSD_ERROR;
	}

	key.size = strlen(key.data);
	if (nsd_attr_fetch_bool(rq->f_attrs, "null_extend_key", FALSE)) {
		key.size++;
	}

	if (nsd_attr_fetch_bool(rq->f_attrs, "casefold", FALSE)) {
		char *p, *q;
		int i;

		p = key.data;
		q = alloca(key.size);
		for (i=0; i < key.size; q++, p++, i++) {
			*q = tolower(*p);
		}
		key.data = q - key.size;
	}

	/*
	** Lock the db
	*/
	if (dbhash_db_lock(hf) != NS_SUCCESS) {
		rq->f_status = NS_TRYAGAIN;
		return NSD_ERROR;
	}

	/*
	** Do the lookup.
	*/
	err = (hf->map->get)(hf->map, &key, &val, NULL);

	if (err) {
		rq->f_status = NS_NOTFOUND;
		return NSD_NEXT;
	}

	/*
	** Copy the data out of the dbhash file into the file structure.
	*/
	nsd_set_result(rq, NS_SUCCESS, val.data, val.size, VOLATILE);

	/*
	** Append a newline to result since this is not in the db file.
	*/
	nsd_append_result(rq, NS_SUCCESS, "\n", 1);

	return NSD_OK;
}

/*
** This routine simply walks the dbhash table appending the values to the
** result.  It buffers up 4KB at a time to reduce the number of times we
** call malloc.
*/
int
list(nsd_file_t *rq)
{
	dbhash_file_t *hf;
	dbpair kv;
	char buf[4096];
	char *p, *end;
	int err, addkey;

	if (! rq) {
		return NSD_ERROR;
	}

	/*
	** Find the database pointer in our list.
	*/
	hf = dbhash_get_file(rq);
	if (! hf) {
		rq->f_status = NS_NOTFOUND;
		return NSD_NEXT;
	}
	
	if (! dbhash_reopen(hf)) {
		rq->f_status = NS_NOTFOUND;
		return NSD_NEXT;
	}

	/*
	** Lock the db
	*/
	if (dbhash_db_lock(hf) != NS_SUCCESS) {
		rq->f_status = NS_TRYAGAIN;
		return NSD_ERROR;
	}

	addkey = nsd_attr_fetch_bool(rq->f_attrs, "enumerate_key", 0);

	/*
	** Loop through the entries.
	*/
	p = buf, end = p + 4095;
	for (err = (hf->map->seq)(hf->map,&(kv.key),&(kv.val),R_FIRST); 
	     !err; 
	     err = (hf->map->seq)(hf->map,&(kv.key),&(kv.val),R_NEXT)) {
		/*
		** Copy result into file structure when our local buffer
		** fills up.
		*/
		if ((end - p) < (kv.key.size + kv.val.size)) {
			nsd_append_result(rq, NS_SUCCESS, buf, p - buf);
			p = buf;
		}

		/*
		** Append the next value to the local buffer.
		*/
		if (addkey) {
			if (kv.key.data) {
				memcpy(p, kv.key.data, kv.key.size);
				p += kv.key.size;
			}
			*p++ = '\t';
		}
		if (kv.val.data) {
			memcpy(p, kv.val.data, kv.val.size);
			p += kv.val.size;
			*p++ = '\n';
		}
	}

	/*
	** We are done reading from the dbhash file now so we release the
	** lock.
	*/
	dbhash_db_unlock(hf);

	/*
	** Copy any remaining data from the local buffer into the file
	** structure.
	*/
	if (p - buf) {
		nsd_append_result(rq, NS_SUCCESS, buf, p - buf);
	}


	return NSD_NEXT;
}

/*
** The shake function for this library just frees up all the saved state,
** closing all the files and releasing the memory.
*/
int
shake(int priority, time_t now)
{
	dbhash_file_t *hf;

	if (priority == 10) {
		dbhash_clear();
		return NSD_OK;
	}

	now -= (10 - priority) * 60;
	for (hf = __dbhash_files; hf; hf = hf->next) {
		if (hf->touched < now) {
			dbhash_close(hf);
			/* XXX patch BACK */
			return NSD_OK;
		}
	}

	return NSD_OK;
}

/*
** berkeley db does not support locking, use flock to do the locking
** and unlocking.
*/

int
dbhash_db_lock(dbhash_file_t *hf) {
	
	if (flock (hf->fd, LOCK_SH)) 
		return(NS_FATAL);
	
	return (NS_SUCCESS);
}

int
dbhash_db_unlock(dbhash_file_t *hf) {

	if (flock(hf->fd, LOCK_UN))
		return(NS_FATAL);

	return (NS_SUCCESS);
}

/*
** The dump function prints out our current state.  For this library that
** is simply the list of open databases.
*/
int
dump(FILE *fp)
{
	dbhash_file_t *hf;

	fprintf(fp, "Current hash files:\n");
	for (hf = __dbhash_files; hf; hf = hf->next) {
		fprintf(fp, "domain: %s\n", (hf->domain && *hf->domain) ?
		    hf->domain : "default");
		fprintf(fp, "\tfile: %s\n", hf->file);
		fprintf(fp, "\tlast touched: %s", ctime(&hf->touched));
		fprintf(fp, "\tstatus: %s\n", (hf->map) ? "open" : "closed");
	}

	return NSD_OK;
}
