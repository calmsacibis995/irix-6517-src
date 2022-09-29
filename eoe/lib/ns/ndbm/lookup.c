#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <alloca.h>
#include <ctype.h>
#include <fcntl.h>
#include <ndbm.h>
#define _DATUM_DEFINED
#include <ns_api.h>
#include <ns_daemon.h>

typedef struct ndbm_file {
	char			*domain;
	char			*table;
	char			*file;
	time_t			version;
	time_t			touched;
	DBM			*map;
	struct ndbm_file	*next;
} ndbm_file_t;

static ndbm_file_t *__ndbm_files = (ndbm_file_t *)0;

/*
** We cache open files so that we will not have to reopen the map on each
** call.  This routine just seaches the list for a matching hash file, and
** creates a new entry if not found.
*/
static ndbm_file_t *
ndbm_get_file(nsd_file_t *rq)
{
	ndbm_file_t *hf;
	char fbuf[PATH_MAX];
	int len;
	char *domain, *file, *table;

	/*
	** There is one hash file for each table in each domain, so
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
	for (hf = __ndbm_files; hf; hf = hf->next) {
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
	hf = (ndbm_file_t *)nsd_calloc(1, sizeof(ndbm_file_t));
	if (! hf) {
		nsd_logprintf(NSD_LOG_RESOURCE, "ndbm_open: failed malloc\n");
		return (ndbm_file_t *)0;
	}

	/*
	** Copy the domain name into the ndbm structure.
	*/
	if (domain) {
		hf->domain = nsd_strdup(domain);
		if (! hf->domain) {
			nsd_logprintf(NSD_LOG_RESOURCE, 
			    "ndbm_open: failed malloc\n");
			free(hf);
			return (ndbm_file_t *)0;
		}
	}

	/*
	** Copy the table name into the ndbm structure.
	*/
	hf->table = nsd_strdup(table);
	if (! hf->table) {
		nsd_logprintf(NSD_LOG_RESOURCE, "ndbm_open: failed malloc\n");
		free(hf->domain);
		free(hf);
		return (ndbm_file_t *)0;
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
				len = nsd_cat(fbuf, sizeof(fbuf), 4,
				    NS_DOMAINS_DIR, domain, "/", file);
			} else {
				len = nsd_cat(fbuf, sizeof(fbuf), 2, "/etc/",
				    file);
			}
		}
	} else {
		if (domain) {
			len = nsd_cat(fbuf, sizeof(fbuf), 4,
			    NS_DOMAINS_DIR, domain, "/", table);
		} else {
			len = nsd_cat(fbuf, sizeof(fbuf), 2, "/etc/", table);
		}
	}

	/*
	** Copy the filename into the ndbm structure.
	*/
	hf->file = (char *)nsd_malloc(len + 1);
	if (! hf->file) {
		nsd_logprintf(NSD_LOG_RESOURCE, "ndbm_open: failed malloc\n");
		if (domain) free(hf->domain);
		free(hf->table);
		free(hf);
		return (ndbm_file_t *)0;
	}
	memcpy(hf->file, fbuf, len);
	hf->file[len] = (char)0;

	hf->next = __ndbm_files;
	__ndbm_files = hf;

	time(&hf->touched);
	return hf;
}

/*
** This routine will attempt to open the ndbm file represented by this
** structure.
*/
static int
ndbm_reopen(ndbm_file_t *hf)
{
	/*
	** Close the map if it were already open.
	*/
	if (hf->map) {
		dbm_close(hf->map);
	}

	if (! hf->file) return NSD_ERROR;

	/*
	** Open it up.
	*/
	hf->map = dbm_open(hf->file, O_RDONLY, 0666);
	if (hf->map) {
		return NSD_OK;
	}

	return NSD_ERROR;
}

/*
** This routine walks the ndbm table list freeing each element.  We
** close any files which may be open.
*/
static void
ndbm_clear(void)
{
	ndbm_file_t *hf;

	for (hf = __ndbm_files; hf; hf = __ndbm_files) {
		__ndbm_files = hf->next;

		free(hf->file);
		free(hf->table);
		if (hf->domain) {
			free(hf->domain);
		}
		if (hf->map) {
			dbm_close(hf->map);
		}
		free(hf);
	}
}

/*
** This routine simply closes and unmaps a ndbm file.
*/
static void
ndbm_close(ndbm_file_t *hf) {
	if (hf->map) {
		dbm_close(hf->map);
		hf->map = (DBM *)0;
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
	ndbm_clear();

	return NSD_OK;
}

/*
** This routine simply returns the requested key from the ndbm file for
** the requested domain and table.
*/
int
lookup(nsd_file_t *rq)
{
	ndbm_file_t *hf;
	datum key, val;
	struct stat sbuf;

	nsd_logprintf(NSD_LOG_MIN, "entering lookup (ndbm):\n");
	
	if (! rq) {
		return NSD_ERROR;
	}

	/*
	** Lookup the file information in our cache.
	*/
	hf = ndbm_get_file(rq);
	if (! hf) {
		rq->f_status = NS_NOTFOUND;
		return NSD_NEXT;
	}
	
	/*
	** Open the file if closed, or reopen if changed.
	*/
	if (stat(hf->file, &sbuf) < 0) {
		rq->f_status = NS_NOTFOUND;
		return NSD_NEXT;
	}
	if ((! hf->map) || (hf->version < sbuf.st_mtime)) {
		if (! ndbm_reopen(hf)) {
			rq->f_status = NS_NOTFOUND;
			return NSD_NEXT;
		}
	}

	/*
	** Determine the key for the lookup.
	*/
	key.dptr = nsd_attr_fetch_string(rq->f_attrs, "key", 0); 
	if (! key.dptr) {
		rq->f_status = NS_BADREQ;
		return NSD_ERROR;
	}

	key.dsize = strlen(key.dptr);
	if (nsd_attr_fetch_bool(rq->f_attrs, "null_extend_key", FALSE)) {
		key.dsize++;
	}

	if (nsd_attr_fetch_bool(rq->f_attrs, "casefold", FALSE)) {
		char *p, *q;
		int i;

		p = key.dptr;
		q = alloca(key.dsize);
		for (i=0; i < key.dsize; q++, p++, i++) {
			*q = tolower(*p);
		}
		key.dptr = q - key.dsize;
	}

	/*
	** Do the lookup.
	*/
	val = dbm_fetch(hf->map, key);

	if (! val.dptr) {
		rq->f_status = NS_NOTFOUND;
		return NSD_NEXT;
	}

	/*
	** Copy the data out of the ndbm file into the file structure.
	*/
	nsd_set_result(rq, NS_SUCCESS, val.dptr, val.dsize, VOLATILE);
	nsd_append_result(rq, NS_SUCCESS, "\n", sizeof("\n"));

	return NSD_OK;
}

/*
** This routine simply walks the ndbm table appending the values to the
** result.  It buffers up 4KB at a time to reduce the number of times we
** call malloc.
*/
int
list(nsd_file_t *rq)
{
	ndbm_file_t *hf;
	datum key, val;
	struct stat sbuf;
	char buf[4096];
	char *p, *end;
	int addkey;

	if (! rq) {
		return NSD_ERROR;
	}

	/*
	** Find the database pointer in our list.
	*/
	hf = ndbm_get_file(rq);
	if (! hf) {
		rq->f_status = NS_NOTFOUND;
		return NSD_NEXT;
	}
	
	/*
	** Reopen the map if it is currently closed or out of date.
	*/
	if (stat(hf->file, &sbuf) < 0) {
		rq->f_status = NS_NOTFOUND;
		return NSD_NEXT;
	}
	if ((! hf->map) || (hf->version < sbuf.st_mtime)) {
		if (! ndbm_reopen(hf)) {
			rq->f_status = NS_NOTFOUND;
			return NSD_NEXT;
		}
	}

	addkey = nsd_attr_fetch_bool(rq->f_attrs, "enumerate_key", 0);

	/*
	** Loop through the entries.
	*/
	p = buf, end = p + 4095;
	for (key = dbm_firstkey(hf->map); 
	     key.dptr; 
	     key = dbm_nextkey(hf->map)) {
		/*
		** Grab the value associated with the key
		*/
		val = dbm_fetch(hf->map, key);

		/*
		** Copy result into file structure when our local buffer
		** fills up.
		*/
		if ((end - p) < (key.dsize + val.dsize)) {
			nsd_append_result(rq, NS_SUCCESS, buf, p - buf);
			p = buf;
		}

		/*
		** Append the next value to the local buffer.
		*/
		if (addkey) {
			if (key.dptr) {
				memcpy(p, key.dptr, key.dsize);
				p += key.dsize;
			}
			*p++ = '\t';
		}
		if (val.dptr) {
			memcpy(p, val.dptr, val.dsize);
			p += val.dsize;
			*p++ = '\n';
		}
	}

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
	ndbm_file_t *hf;

	if (priority == 10) {
		ndbm_clear();
		return NSD_OK;
	}

	now -= (10 - priority) * 60;
	for (hf = __ndbm_files; hf; hf = hf->next) {
		if (hf->touched < now) {
			ndbm_close(hf);
		}
	}

	return NSD_OK;
}

/*
** The dump function prints out our current state.  For this library that
** is simply the list of open databases.
*/
int
dump(FILE *fp)
{
	ndbm_file_t *hf;

	fprintf(fp, "Current ndbm files:\n");
	for (hf = __ndbm_files; hf; hf = hf->next) {
		fprintf(fp, "domain: %s\n", (hf->domain && *hf->domain) ?
		    hf->domain : "default");
		fprintf(fp, "\tfile: %s.{pag,dir}\n", hf->file);
		fprintf(fp, "\tlast touched: %s", ctime(&hf->touched));
		fprintf(fp, "\tstatus: %s\n", (hf->map) ? "open" : "closed");
	}

	return NSD_OK;
}
