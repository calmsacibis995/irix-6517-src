#include <stdio.h>
#include <sys/types.h>
#include <string.h>
#include <malloc.h>
#include <ctype.h>
#include <unistd.h>
#include <stdlib.h>
#include <ns_api.h>
#include <ns_daemon.h>
#include "ns_files.h"

extern char _files_result[];

static int
file_ethersByName(nsd_file_t *rq)
{
	int len, klen;
	char *p, *line, *key;
	file_domain_t *dom;
	int (*cmpfunc)(const char *, const char *, size_t);

	/*
	** Open the ethers file if not already done.
	*/
	dom = file_domain_lookup(rq);
	if (! dom) {
		rq->f_status = NS_NOTFOUND;
		return NSD_NEXT;
	}

	/*
	** Start at the beginning and do a linear seach for the name.
	*/
	key = nsd_attr_fetch_string(rq->f_attrs, "key", (char *)0);
	if (! key) {
		rq->f_status = NS_FATAL;
		return NSD_ERROR;
	}
	klen = strlen(key);
	if (klen <= 0) {
		rq->f_status = NS_FATAL;
		return NSD_ERROR;
	}

	if (nsd_attr_fetch_bool(rq->f_attrs, "casefold", FALSE)) {
		cmpfunc = strncasecmp;
	} else {
		cmpfunc = strncmp;
	}
	
	for (line = dom->map; line && *line; line = strchr(line + 1, '\n')) {
		SKIPWSPACE(line);
		if (! *line || *line == '#') continue;
		p = line;
		TOSPACE(p);
		SKIPSPACE(p);
		if (! *p || *p == '#') continue;
		len = strcspn(p, " \t\n#");
		if ((len == klen) &&
		    ((cmpfunc)(key, p, len) == 0)) {
			len = strcspn(line, "\n");
			nsd_set_result(rq, NS_SUCCESS, line, len, VOLATILE);
			nsd_append_result(rq, NS_SUCCESS, "\n", sizeof("\n"));
			return NSD_OK;
		}
	}

	rq->f_status = NS_NOTFOUND;
	return NSD_NEXT;
}

/*
** This just converts a string of the format: %x:%x:%x:%x:%x:%x to
** a 64 bit unsigned that we can use for comparasons.
*/
static uint64_t
aton(char *s)
{
	int u, l, i;
	uint64_t r=0;

	for (i = 0; i < 6; i++) {
		r <<= 8;
		if (isxdigit(*s)) {
			u = *s - (isdigit(*s) ? '0' :
			    (isupper(*s) ? 'A' - 10 : 'a' - 10));
			s++;
			if (isxdigit(*s)) {
				l = *s - (isdigit(*s) ? '0' :
				    (isupper(*s) ? 'A' - 10 : 'a' - 10));
				s++;
			} else {
				l = u;
				u = 0;
			}
			r += (u * 16) + l;
		}
		if ((*s != ':') && (i < 5)) {
			return 0;
		}
		s++;
	}

	return r;
}

static int
file_ethersByAddr(nsd_file_t *rq)
{
	char *line, *key;
	uint64_t addr, check;
	int len;
	file_domain_t *dom;

	/*
	** Open the ethers file if not already done.
	*/
	dom = file_domain_lookup(rq);
	if (! dom) {
		rq->f_status = NS_NOTFOUND;
		return NSD_NEXT;
	}

	/*
	** Start at the beginning and do a linear seach for the addr.
	*/
	key = nsd_attr_fetch_string(rq->f_attrs, "key", (char *)0);
	if (! key) {
		rq->f_status = NS_FATAL;
		return NSD_ERROR;
	}
	addr = aton(key);
	if (! addr) {
		rq->f_status = NS_BADREQ;
		return NSD_ERROR;
	}
	for (line = dom->map; line && *line; line = strchr(line + 1, '\n')) {
		SKIPWSPACE(line);
		if (! *line || *line == '#') continue;
		check = aton(line);
		if (check == addr) {
			len = strcspn(line, "\n");
			nsd_set_result(rq, NS_SUCCESS, line, len, VOLATILE);
			nsd_append_result(rq, NS_SUCCESS, "\n", sizeof("\n"));
			return NSD_OK;
		}
	}

	rq->f_status = NS_NOTFOUND;
	return NSD_NEXT;
}

int
file_ethers_lookup(nsd_file_t *rq)
{
	char *map;

	if (! rq) {
		return NSD_ERROR;
	}

	map = nsd_attr_fetch_string(rq->f_attrs, "table", (char *)0);
	if (! map) {
		rq->f_status = NS_FATAL;
		return NSD_ERROR;
	}

	if (strcasecmp(map, ETHERS_BYNAME) == 0) {
		return file_ethersByName(rq);
	} else {
		return file_ethersByAddr(rq);
	}
}

int
file_ethers_list(nsd_file_t *rq)
{
	file_domain_t *dom;

	if (! rq) {
		rq->f_status = NS_FATAL;
		return NSD_ERROR;
	}

	dom = file_domain_lookup(rq);
	if (! dom) {
		rq->f_status = NS_NOTFOUND;
		return NSD_NEXT;
	}

	nsd_append_element(rq, NS_SUCCESS, dom->map, dom->size);
	if (dom->map[dom->size - 1] != '\n') {
		nsd_append_result(rq, NS_SUCCESS, "\n", sizeof("\n"));
	}
	return NSD_NEXT;
}
