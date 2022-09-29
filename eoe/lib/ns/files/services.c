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
file_serviceByName(nsd_file_t *rq)
{
	int len, plen = 0, clen;
	char *line, *p, *q, name[256], *proto, *key;
	file_domain_t *dom;
	int (*cmpfunc)(const char *, const char *, size_t);

	/*
	** Open the services file if not already done.
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
	strcpy(name, key);
	proto = strpbrk(name, "/:");
	if (proto) {
		*proto++ = (char)0;
		plen = strlen(proto);
	}
	len = strlen(name);
	if (len <= 0) {
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

		/*
		** Find protocol.
		*/
		q = line;
		TOSPACE(q);
		SKIPSPACE(q);
		for (; *q && !isspace(*q) && (*q != '/'); q++);
		if (*q++ != '/') {
			continue;
		}
		if (proto && ((cmpfunc)(proto, q, plen) != 0)) {
				continue;
		}

		/*
		** Search through fields looking for name.
		*/
		for (p = line; p && *p && *p != '\n'; p=strpbrk(p, " \t\n#")) {
			SKIPSPACE(p);
			if (!p || !*p || *p == '#') break;
			clen = strcspn(p, " \t\n#");
			if (clen == len) {
				if ((cmpfunc)(name, p, len) == 0) {
					len = strcspn(line, "#\n");
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
file_serviceByPort(nsd_file_t *rq)
{
	int len, clen;
	char *p, *line, *key;
	unsigned long port, check;
	char buf[256], *proto;
	file_domain_t *dom;
	int (*cmpfunc)(const char *, const char *, size_t);

	/*
	** Open the services file if not already done.
	*/
	dom = file_domain_lookup(rq);
	if (! dom) {
		rq->f_status = NS_NOTFOUND;
		return NSD_NEXT;
	}

	/*
	** Start at the beginning and do a linear seach for the port.
	*/
	key = nsd_attr_fetch_string(rq->f_attrs, "key", (char *)0);
	if (! key) {
		rq->f_status = NS_FATAL;
		return NSD_ERROR;
	}
	if (nsd_attr_fetch_bool(rq->f_attrs, "casefold", FALSE)) {
		cmpfunc = strncasecmp;
	} else {
		cmpfunc = strncmp;
	}

	strcpy(buf, key);
	port = strtoul(buf, (char **)0, 10);
	proto = strpbrk(buf, "/:");
	if (proto) {
		proto++;
		len = strlen(proto);
	}
	for (line = dom->map; line && *line; line = strchr(line + 1, '\n')) {
		SKIPWSPACE(line);
		if (! *line || *line == '#') continue;
		p = line;
		TOSPACE(p);
		SKIPSPACE(p);
		check = strtoul(p, (char **)0, 10);
		if (check == port) {
			p = strchr(p, '/');
			if (! p) continue;
			clen = (proto) ? strcspn(++p, " \t\n#") : len;
			if (clen == len) {
				if (!proto ||
				    ((cmpfunc)(proto, p, len) == 0)) {
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

int
file_services_lookup(nsd_file_t *rq)
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

	if (strcasecmp(map, SERVICES_BYNAME) == 0) {
		return file_serviceByName(rq);
	} else {
		return file_serviceByPort(rq);
	}
}

int
file_services_list(nsd_file_t *rq)
{
	file_domain_t *dom;

	if (! rq) {
		return NSD_ERROR;
	}

	/*
	** Open the services file if not already done.
	*/
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
