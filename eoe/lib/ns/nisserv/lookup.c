#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <alloca.h>
#include <mdbm.h>
#include <arpa/inet.h>
#include <ns_api.h>
#include <ns_daemon.h>
#include "ns_nisserv.h"

static hash_file_t *__hash_files = 0;
extern int _ltoa(long, char *);
extern securenets_t *_nisserv_securenets;

/*
** We cache open files so that we will not have to reopen the map on each
** call.  This routine just seaches the list for a matching hash file, and
** creates a new entry if not found.
*/
hash_file_t *
hash_get_file(nsd_file_t *rq)
{
	hash_file_t *hf;
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
	for (hf = __hash_files; hf; hf = hf->next) {
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
	hf = (hash_file_t *)nsd_calloc(1, sizeof(hash_file_t));
	if (! hf) {
		nsd_logprintf(NSD_LOG_OPER, "hash_open: failed malloc\n");
		return (hash_file_t *)0;
	}

	/*
	** Copy the domain name into the hash structure.
	*/
	if (domain) {
		hf->domain = nsd_strdup(domain);
		if (! hf->domain) {
			nsd_logprintf(NSD_LOG_OPER, "hash_open: failed malloc\n");
			free(hf);
			return (hash_file_t *)0;
		}
	}

	/*
	** Copy the table name into the hash structure.
	*/
	hf->table = nsd_strdup(table);
	if (! hf->table) {
		nsd_logprintf(NSD_LOG_OPER, "hash_open: failed malloc\n");
		free(hf->domain);
		free(hf);
		return (hash_file_t *)0;
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
				len = nsd_cat(fbuf, sizeof(fbuf), 5,
				    NS_DOMAINS_DIR, domain, "/", file, ".m");
			} else {
				len = nsd_cat(fbuf, sizeof(fbuf), 3, "/etc/",
				    file, ".m");
			}
		}
	} else {
		if (domain) {
			len = nsd_cat(fbuf, sizeof(fbuf), 5, NS_DOMAINS_DIR,
			    domain, "/", table, ".m");
		} else {
			len = nsd_cat(fbuf, sizeof(fbuf), 3, "/etc/", table,
			    ".m");
		}
	}

	/*
	** Copy the filename into the hash structure.
	*/
	hf->file = (char *)nsd_malloc(len + 1);
	if (! hf->file) {
		nsd_logprintf(NSD_LOG_OPER, "hash_open: failed malloc\n");
		free(hf->domain);
		free(hf->table);
		free(hf);
		return (hash_file_t *)0;
	}
	memcpy(hf->file, fbuf, len);
	hf->file[len] = (char)0;

	hf->next = __hash_files;
	__hash_files = hf;

	time(&hf->touched);
	return hf;
}

/*
** This routine will attempt to open the hash file represented by this
** structure.
*/
int
hash_reopen(hash_file_t *hf)
{
	kvpair kv;

	/*
	** Close the map if it were already open.
	*/
	if (hf->map) {
		mdbm_close(hf->map);
	}

	/*
	** Open it up.
	*/
	hf->map = nsd_mdbm_open(hf->file, O_RDONLY, 0666, 0);
	if (! hf->map) {
		return NSD_ERROR;
	}

	/*
	** Check if we are a "secure" map.
	*/
	kv.key.dptr = "YP_SECURE";
	kv.key.dsize = sizeof("YP_SECURE") - 1;
	kv.val.dsize = MDBM_PAGE_SIZE(hf->map);
	kv.val.dptr = alloca(kv.val.dsize);
	kv.val = mdbm_fetch(hf->map, kv);

	hf->secure = (kv.val.dptr) ? 1 : 0;

	return NSD_OK;
}

/*
** This routine walks the hash table list freeing each element.  We
** close any files which may be open.
*/
void
hash_clear(void)
{
	hash_file_t *hf;

	for (hf = __hash_files; hf; hf = __hash_files) {
		__hash_files = hf->next;

		free(hf->file);
		free(hf->table);
		if (hf->domain) {
			free(hf->domain);
		}
		if (hf->map) {
			mdbm_close(hf->map);
		}
		free(hf);
	}
}

/*
** This routine just closes and unmaps a file.
*/
static void
hash_close(hash_file_t *hf)
{
	if (hf->map) {
		mdbm_close(hf->map);
		hf->map = (MDBM *)0;
	}
}

/*
** This routine simply returns the requested key from the hash file for
** the requested domain and table.
*/
int
lookup(nsd_file_t *rq)
{
	hash_file_t *hf;
	kvpair kv;
	struct stat sbuf;

	if (! rq) {
		return NSD_ERROR;
	}

	/*
	** Lookup the file information in our cache.
	*/
	hf = hash_get_file(rq);
	if (! hf) {
		rq->f_status = NS_UNAVAIL;
		return NSD_NEXT;
	}
	
	/*
	** Open the file if closed, or reopen if changed.
	*/
	if (stat(hf->file, &sbuf) < 0) {
		rq->f_status = NS_UNAVAIL;
		return NSD_NEXT;
	}
	if ((! hf->map) || (hf->version < sbuf.st_mtime) 
	    || _Mdbm_invalid(hf->map)) {
		if (! hash_reopen(hf)) {
			rq->f_status = NS_UNAVAIL;
			return NSD_NEXT;
		}
		hf->version = sbuf.st_mtime;
	}

	/*
	** Determine the key for the lookup.
	*/
	kv.key.dptr = nsd_attr_fetch_string(rq->f_attrs, "key", 0);
	if (! kv.key.dptr) {
		rq->f_status = NS_BADREQ;
		return NSD_ERROR;
	}
	/* key.dsize = strlen(key.dptr); */
	kv.key.dsize=rq->f_namelen;
	kv.val.dsize=MDBM_PAGE_SIZE(hf->map);
	kv.val.dptr=alloca(kv.val.dsize);

	/*
	** Do the lookup.
	*/
	kv.val = mdbm_fetch(hf->map, kv);

	if (! kv.val.dptr || kv.val.dsize < 0) {
		rq->f_status = NS_NOTFOUND;
		return NSD_NEXT;
	}

	/*
	** Copy the data out of the hash file into the file structure.
	*/
	nsd_set_result(rq, NS_SUCCESS, kv.val.dptr, kv.val.dsize, VOLATILE);

	/*
	** Update nis_secure attribute.
	*/
	if (hf->secure) {
		nsd_attr_store(&rq->f_attrs, "nis_secure", "1");
	}

	return NSD_OK;
}

/*
** This routine simply walks the hash table appending the values to the
** result.  It buffers up 4KB at a time to reduce the number of times we
** call malloc.
*/
int
list(nsd_file_t *rq)
{
	hash_file_t *hf;
	kvpair kv;
	struct stat sbuf;
	char buf[4096];
	char *p, *end;
	char *key, *val;
	int pagesize, addkey;

	if (! rq) {
		return NSD_ERROR;
	}

	/*
	** Find the database pointer in our list.
	*/
	hf = hash_get_file(rq);
	if (! hf) {
		rq->f_status = NS_UNAVAIL;
		return NSD_NEXT;
	}
	
	/*
	** Reopen the map if it is currently closed or out of date.
	*/
	if (stat(hf->file, &sbuf) < 0) {
		rq->f_status = NS_UNAVAIL;
		return NSD_NEXT;
	}
	if ((! hf->map) || (hf->version < sbuf.st_mtime)
	    || _Mdbm_invalid(hf->map)) {
		if (! hash_reopen(hf)) {
			rq->f_status = NS_UNAVAIL;
			return NSD_NEXT;
		}
		hf->version = sbuf.st_mtime;
	}

	pagesize=MDBM_PAGE_SIZE(hf->map);
	key=alloca(pagesize);
	val=alloca(pagesize);

	addkey = nsd_attr_fetch_bool(rq->f_attrs, "enumerate_key", 0);

	/*
	** Loop through the entries.
	*/
	p = buf, end = p + 4095;
	for (kv.key.dptr = key , kv.key.dsize = pagesize,
		     kv.val.dptr = val , kv.val.dsize = pagesize, 
		     kv = mdbm_first(hf->map,kv); 
	     kv.key.dptr; 
	     kv.key.dptr = key , kv.key.dsize = pagesize,
		     kv.val.dptr = val , kv.val.dsize = pagesize, 
		     kv = mdbm_next(hf->map,kv)) {
		/* Skip magic keys. */
		if ((kv.key.dsize > 3) &&
		    (strncmp(kv.key.dptr, "YP_", 3) == 0)) {
			continue;
		}

		/*
		** Copy result into file structure when our local buffer
		** fills up.
		*/
		if ((end - p) < (kv.key.dsize + kv.val.dsize)) {
			nsd_append_result(rq, NS_SUCCESS, buf, p - buf);
			p = buf;
		}

		/*
		** Append the next value to the local buffer.
		*/
		if (addkey) {
			if (kv.key.dptr) {
				memcpy(p, kv.key.dptr, kv.key.dsize);
				p += kv.key.dsize;
			}
			*p++ = '\t';
		}
		if (kv.val.dptr) {
			memcpy(p, kv.val.dptr, kv.val.dsize);
			p += kv.val.dsize;
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
	hash_file_t *hf;

	if (priority == 10) {
		hash_clear();
		return NSD_OK;
	}

	now -= (10 - priority) * 60;
	for (hf = __hash_files; hf; hf = hf->next) {
		if (hf->touched < now) {
			hash_close(hf);
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
	hash_file_t *hf;
	securenets_t *sp;

	fprintf(fp, "Current hash files:\n");
	for (hf = __hash_files; hf; hf = hf->next) {
		fprintf(fp, "domain: %s\n", (hf->domain && *hf->domain) ?
		    hf->domain : "default");
		fprintf(fp, "\tfile: %s\n", hf->file);
		fprintf(fp, "\tlast touched: %s", ctime(&hf->touched));
		fprintf(fp, "\tstatus: %s\n", (hf->map) ? "open" : "closed");
	}
	fprintf(fp, "Secure networks:\n");
	if (_nisserv_securenets) {
		fprintf(fp, "\tnetmask: 255.255.255.255 address: 127.0.0.1\n");
		for (sp = _nisserv_securenets; sp; sp = sp->next) {
			fprintf(fp, "\tnetmask: %s ", inet_ntoa(sp->netmask));
			fprintf(fp, "address: %s\n", inet_ntoa(sp->addr));
		}
	} else {
		fprintf(fp, "\tall nets are secure\n");
	}

	return NSD_OK;
}
