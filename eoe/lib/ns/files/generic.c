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

int
file_generic_lookup(nsd_file_t *rq)
{
	int len, clen;
	char *line, *key, *p, *sep;
	file_domain_t *dom;
	int (*cmpfunc)(const char *, const char *, size_t);

	if (! rq) {
		return NSD_ERROR;
	}

	/*
	** Open the file if not already done.
	*/
	dom = file_domain_lookup(rq);
	if (! dom) {
		rq->f_status = NS_NOTFOUND;
		return NSD_NEXT;
	}

	/*
	** Figure out what the file looks like.
	*/
	sep = nsd_attr_fetch_string(rq->f_attrs, "separator", " \t\n");

	/*
	** Start at the beginning and do a linear seach for the name.
	*/
	key = nsd_attr_fetch_string(rq->f_attrs, "key", (char *)0);
	if (! key) {
		rq->f_status = NS_FATAL;
		return NSD_ERROR;
	}
	len = strlen(key);
	if (len <= 0) {
		rq->f_status = NS_FATAL;
		return NSD_ERROR;
	}

	if (nsd_attr_fetch_bool(rq->f_attrs, "casefold", FALSE)) {
		cmpfunc = strncasecmp;
	} else {
		cmpfunc = strncmp;
	}

	/*
	** Remember to check for any separators at the start of each line and
	** at the start of each line segment (p);  if either starts with a 
	** separator, we'll loop forever
	*/
	for (line = dom->map; line && *line; line = strchr(line + 1, '\n')) {
	        line += strspn(line, sep); 
		while (*line == '\n') {	
			line++;
		}
		for (p = line; p && *p && *p != '\n';
		    p = strpbrk(p, sep)) {
			clen = strcspn(p, sep);
			if (clen == len) {
				if ((cmpfunc)(key, line, len) == 0) {
					len = strcspn(line, "\n");
					nsd_set_result(rq, NS_SUCCESS,
					    line, len, VOLATILE);
					nsd_append_result(rq, NS_SUCCESS,
					    "\n", sizeof("\n"));
					return NSD_OK;
				}
			}
		p += strspn(p, sep); 
		}
	}

	rq->f_status = NS_NOTFOUND;
	return NSD_NEXT;
}

int
file_generic_list(nsd_file_t *rq)
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
