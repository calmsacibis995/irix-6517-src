#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <ns_api.h>
#include <ns_daemon.h>
#include "ns_files.h"

#ident "$Revision: 1.6 $"

static int
file_macByName(nsd_file_t *rq)
{
	char *key;
	char *line;
	char *colon;
	file_domain_t *dom;
	size_t len;
	int (*cmpfunc)(const char *, const char *, size_t);

	/*
	** Open the label file if not already done.
	*/
	dom = file_domain_lookup(rq);
	if (! dom) {
		rq->f_status = NS_NOTFOUND;
		return NSD_NEXT;
	
	}

	/*
	** Start at the beginning and do a linear seach for the key.
	*/
	key = nsd_attr_fetch_string(rq->f_attrs, "key", (char *)0);
	if (! key) {
		rq->f_status = NS_FATAL;
		return NSD_ERROR;
	}
	len = strlen(key);
	if (len == 0) {
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
		if (! *line  || *line == '#')
			continue;
		colon = strchr(line, ':');
		if (colon == NULL)
			break;
		colon = strchr(colon + 1, ':');
		if (colon == NULL)
			break;
		if ((colon - line) == len) {
			if ((cmpfunc)(key, line, len) == 0) {
				len = strcspn(line, "\n");
				nsd_set_result(rq, NS_SUCCESS, line,
					       len, VOLATILE);
				nsd_append_result(rq, NS_SUCCESS, "\n",
				    sizeof("\n"));
				return NSD_OK;
			}
		}
	}

	rq->f_status = NS_NOTFOUND;
	return NSD_NEXT;
}

static int
file_macByValue(nsd_file_t *rq)
{
	char *key;
	char *line;
	file_domain_t *dom;
	size_t len;
	int (*cmpfunc)(const char *, const char *, size_t);

	/*
	** Open the label file if not already done.
	*/
	dom = file_domain_lookup(rq);
	if (! dom) {
		rq->f_status = NS_NOTFOUND;
		return NSD_NEXT;
	
	}

	/*
	** Start at the beginning and do a linear seach for the key.
	*/
	key = nsd_attr_fetch_string(rq->f_attrs, "key", (char *)0);
	if (! key) {
		rq->f_status = NS_FATAL;
		return NSD_ERROR;
	}
	len = strlen(key);
	if (len == 0) {
		rq->f_status = NS_FATAL;
		return NSD_ERROR;
	}
	if (nsd_attr_fetch_bool(rq->f_attrs, "casefold", FALSE)) {
		cmpfunc = strncasecmp;
	} else {
		cmpfunc = strncmp;
	}

	for (line = dom->map; line && *line; line = strchr(line + 1, '\n')) {
		char *p;

		SKIPWSPACE(line);
		if (! *line || *line == '#')
			continue;
		for (p = line; *p != '\0'; p++) {
			if (*p == '\n')
				break;
			if (*p == ':') {
				if ((cmpfunc)(key, p + 1, len) == 0) {
					len = strcspn(line, "\n");
					nsd_set_result(rq, NS_SUCCESS, line,
						       len, VOLATILE);
					nsd_append_result(rq, NS_SUCCESS,
					    "\n", sizeof("\n"));
					return NSD_OK;
				}
				break;
			}
		}
	}

	rq->f_status = NS_NOTFOUND;
	return NSD_NEXT;
}

int
file_mac_lookup(nsd_file_t *rq)
{
	char *map;

	if (! rq) {
		return NSD_ERROR;
	}

	map = nsd_attr_fetch_string(rq->f_attrs, "table", (char *)0);
	if (! map) {
		return NSD_ERROR;
	}

	if (strcasecmp(map, MAC_BYNAME) == 0) {
		return file_macByName(rq);
	} else {
		return file_macByValue(rq);
	}
}

int
file_mac_list(nsd_file_t *rq)
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
