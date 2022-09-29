#include <stdio.h>
#include <sys/types.h>
#include <netdb.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <malloc.h>
#include <alloca.h>
#include <ctype.h>
#include <unistd.h>
#include <stdlib.h>
#include <ns_api.h>
#include <ns_daemon.h>
#include "ns_files.h"

extern char _files_result[];

int
file_hostsByName(nsd_file_t *rq)
{
	int len, clen;
	char *p, *line=0, *key;
	file_domain_t *dom;
	int (*cmpfunc)(const char *, const char *, size_t);

	nsd_logprintf(NSD_LOG_MIN, "entering file_hostsByName\n");

	if (! rq) {
		return NSD_ERROR;
	}

	key = nsd_attr_fetch_string(rq->f_attrs, "key", (char *)0);
	if (! key || ! *key) {
		rq->f_status = NS_FATAL;
		return NSD_ERROR;
	}

	/*
	** Open the hosts file if not already done.
	*/
	dom = file_domain_lookup(rq);
	if (! dom) {
		rq->f_status = NS_NOTFOUND;	
		return NSD_NEXT;
	}

	/*
	** Start at the beginning and do a linear search for the key string.
	*/
	len = strlen(key);
	if (nsd_attr_fetch_bool(rq->f_attrs, "casefold", FALSE)) {
		cmpfunc = strncasecmp;
	} else {
		cmpfunc = strncmp;
	}

	for (line = dom->map; line && *line; line = strchr(line + 1, '\n')) {
		SKIPWSPACE(line);
		if (! *line || *line == '#') continue;
		for (p = line; p && *p && *p != '\n';
		    p = strpbrk(p, " \t\n#")) {
			SKIPSPACE(p);
			if (! *p || *p == '#') break;
			clen = strcspn(p, " \t\n#");
			if (clen == len) {
				if ((cmpfunc)(key, p, len) == 0) {
					len = strcspn(line, "\n");
					nsd_set_result(rq, NS_SUCCESS,
					    line, len, VOLATILE);
					nsd_append_result(rq, NS_SUCCESS,
					    "\n", sizeof("\n"));
					return NSD_OK;
				}
			}
		}
	}

	rq->f_status = NS_NOTFOUND;
	return NSD_NEXT;
}

static int
file_hostsByAddr(nsd_file_t *rq)
{
	char *line, *address;
	unsigned long key, check;
	int len;
	file_domain_t *dom;

	/*
	** Open the hosts file if not already done.
	*/
	dom = file_domain_lookup(rq);
	if (! dom) {
		rq->f_status = NS_NOTFOUND;
		return NSD_NEXT;
	}

	/*
	** Start at the beginning and do a linear seach for the address.
	**
	** Address is the first string on the line, but we fist have
	** to convert it into a number and then do a numeric comparison
	** since addresses can be represented with different strings
	** (ie: 127.1 == 127.0.0.1 == 127.000.000.001).
	*/
	address = nsd_attr_fetch_string(rq->f_attrs, "key", (char *)0);
	if (! address) {
		return NSD_ERROR;
	}
	if ((key = inet_addr(address)) == INADDR_NONE) {
		return NSD_ERROR;
	}
	for (line = dom->map; line && *line; line = strchr(line, '\n')) {
		SKIPWSPACE(line);
		check = inet_addr(line);
		if (check == key) {
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
file_hosts_lookup(nsd_file_t *rq)
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
	if (strcasecmp(map, HOSTS_BYNAME) == 0) {
		return file_hostsByName(rq);
	} else {
		return file_hostsByAddr(rq);
	}
}

int
file_hosts_list(nsd_file_t *rq)
{
	file_domain_t *dom;

	if (! rq) {
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
