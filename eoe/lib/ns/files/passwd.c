/*
** This file contains code to parse the password file.  If the 'compat'
** attribute is true we deal with +/- escapes in the file by doing
** loopback requests into nsd.
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <alloca.h>
#include <ns_api.h>
#include <ns_daemon.h>
#include "ns_files.h"
#include "memory.h"
#include "dstring.h"
#include "htree.h"

extern char _files_result[];
extern size_t __pagesize;

typedef struct {
	htree_t		netgroups;
	htree_t		skip;
	htree_t		logins;
	htree_t		tmp;
	dstring_t	login;
	dstring_t	result;
	file_domain_t	*dom;
	unsigned	offset;
	unsigned	state;
	unsigned	flags;
	unsigned	count;
	struct timeval	version;
	uid_t		uid;
	char		*domain;
	char		*merge;
	int		dlen;
	int		(*cmpfunc)(const char *, const char *, size_t);
	void		*arena;
} file_data_t;

#define NETGRP		(1<<0)
#define LOGIN		(1<<1)
#define NO_LOGIN	(1<<2)
#define RESULT		(1<<3)
#define SKIP_ALL	(1<<4)
#define DONE_ALL	(1<<5)
#define ERROR		(1<<31)

typedef struct ng_names {
	char		*name;
	int		len;
	struct ng_names	*last;
} ng_names_t;

/*
** This builds up a password string using the local password entry and
** some other password entry.  Every field that exists in the local
** entry overrides the other.  We build this into the global result buffer
** and return a length.
*/
static int
build_result(char *file, char *other)
{
	char *result=_files_result;

	for (; file && *file && (*file == '+' || *file == '@'); file++) ;

	if (! other) {
		for (; file && *file && (*file != '\n'); file++) {
			*result++ = *file;
		}
		*result++ = '\n';
		*result++ = (char)0;
		return (result - _files_result);
	}

	/*
	** Login, not overridable.
	*/
	while (*other && (*other != ':') && (*other != '\n')) {
		*result++ = *other++;
	}
	for (; *file && (*file != ':') && (*file != '\n'); file++);
	*result++ = ':';
	if (*other == ':') {
		other++;
	}
	if (*file == ':') {
		file++;
	}

	/*
	** Password
	*/
	if (*file && (*file != ':') && (*file != '\n')) {
		while (*file && (*file != ':') && (*file != '\n')) {
			*result++ = *file++;
		}
		for (; *other && (*other != ':') && (*other != '\n'); other++);
	} else {
		while (*other && (*other != ':') && (*other != '\n')) {
			*result++ = *other++;
		}
	}
	*result++ = ':';
	if (*other == ':') {
		other++;
	}
	if (*file == ':') {
		file++;
	}

	/*
	** Uid, not overridable.
	*/
	while (*other && (*other != ':') && (*other != '\n')) {
		*result++ = *other++;
	}
	for (; *file && (*file != ':') && (*file != '\n'); file++);
	*result++ = ':';
	if (*other == ':') {
		other++;
	}
	if (*file == ':') {
		file++;
	}

	/*
	** Gid, not overridable.
	*/
	while (*other && (*other != ':') && (*other != '\n')) {
		*result++ = *other++;
	}
	for (; *file && (*file != ':') && (*file != '\n'); file++);
	*result++ = ':';
	if (*other == ':') {
		other++;
	}
	if (*file == ':') {
		file++;
	}

	/*
	** Gecos
	*/
	if (*file && (*file != ':') && (*file != '\n')) {
		while (*file && (*file != ':') && (*file != '\n')) {
			*result++ = *file++;
		}
		for (; *other && (*other != ':') && (*other != '\n'); other++);
	} else {
		while (*other && (*other != ':') && (*other != '\n')) {
			*result++ = *other++;
		}
	}
	*result++ = ':';
	if (*other == ':') {
		other++;
	}
	if (*file == ':') {
		file++;
	}

	/*
	** Home
	*/
	if (*file && (*file != ':') && (*file != '\n')) {
		while (*file && (*file != ':') && (*file != '\n')) {
			*result++ = *file++;
		}
		for (; *other && (*other != ':') && (*other != '\n'); other++);
	} else {
		while (*other && (*other != ':') && (*other != '\n')) {
			*result++ = *other++;
		}
	}
	*result++ = ':';
	if (*other == ':') {
		other++;
	}
	if (*file == ':') {
		file++;
	}

	/*
	** Shell
	*/
	if (*file && (*file != ':') && (*file != '\n')) {
		while (*file && (*file != ':') && (*file != '\n')) {
			*result++ = *file++;
		}
		for (; *other && (*other != ':') && (*other != '\n'); other++);
	} else {
		while (*other && (*other != ':') && (*other != '\n')) {
			*result++ = *other++;
		}
	}
	*result++ = '\n';
	*result++ = (char)0;

	return (result - _files_result);
}

/*
** This routine just frees the space associated with a file_data_t.
*/
static void
file_data_clear(void **fdp)
{
	if (fdp && *fdp) {
		file_data_t *fd = *fdp;
		if (fd->arena) {
			m_release(fd->arena);
		} else {
			ht_clear(&fd->netgroups);
			ht_clear(&fd->skip);
			ht_clear(&fd->logins);
			ht_clear(&fd->tmp);
			ds_clear(&fd->login, 0);
			ds_clear(&fd->result, 0);
		}
		if (fd->domain) {
			free(fd->domain);
		}
		free(fd);
		*fdp = 0;
	}
}

/*
** This just sets up a loopback call.
*/
static int
loop_pw(nsd_file_t *rq, nsd_callout_proc *proc, char *table, char *key, int len)
{
	nsd_loop_t l = {0};
	char *k;
	
	if (key) {
		/* local copy of key to null terminate */
		k = alloca(len + 1);
		memcpy(k, key, len);
		k[len] = 0;
		key = k;
	}
	
	l.lb_key = key;
	l.lb_table = table;
	l.lb_proc = proc;
	l.lb_flags = NSD_LOOP_SKIP;
	
	return nsd_loopback(rq, &l);
}

/*
** This is the same as the above for netgroup.
*/
static int
loop_ng(nsd_file_t *rq, nsd_callout_proc *proc, char *table, char *key, int len)
{
	nsd_loop_t l = {0};
	char *k;
	
	if (key) {
		k = alloca(len + 1);
		memcpy(k, key, len);
		k[len] = 0;
	} else {
		k = 0;
		l.lb_attrs = "nis_enumerate_key,enumerate_key";
	}
	
	l.lb_key = k;
	l.lb_table = table;
	l.lb_proc = proc;
	
	return nsd_loopback(rq, &l);
}

/*
** This function parses the results of a netgroup.byuser lookup into
** the netgroup hash.
*/
static int
parse_ng_byuser(htree_t *ht, char *buf)
{
	dstring_t *k;
	char *p, *q;

	/*
	** data looks like:
	**	login: netgroup, netgroup, ...
	*/
	for (p = buf; *p;) {
		for (; *p && isspace(*p); p++);
		for (q = p; *q && ! isspace(*q) && *q != ','; q++);

		k = ds_new(p, q - p, 0);
		if (! k) {
			return 0;
		}
		nsd_logprintf(NSD_LOG_HIGH, "\tadded netgroup: %s\n", k->data);
		
		if (! ht_insert(ht, k, (void *)1, 0)) {
			ds_free(k, 0);
		}
		
		for (p = q; *p && (isspace(*p) || *p == ','); p++);
	}
	
	return 1;
}

/*
** This routine sends the request for all the netgroup thingies.  We
** send them all at once so must be careful to wait until they all
** return.
*/
static int
netgroup(nsd_file_t *rq, file_data_t *fd, nsd_callout_proc *proc,
    char *key, int len)
{
	int status;
	nsd_loop_t l = {0};
	dstring_t ds;
	
	l.lb_proc = proc;
	l.lb_table = "netgroup.byuser";
	fd->count = 0;
	
	ds_init(&ds, 20, 0);
	
	/* login.* */
	ds_set(&ds, key, len, 0);
	ds_append(&ds, ".*", 2, 0);

	l.lb_key = ds.data;

	status = nsd_loopback(rq, &l);
	switch (status) {
	    case NSD_OK:
		/* result was immediately available */
		if (rq->f_loop) {
			if (rq->f_loop->f_data &&
			    (rq->f_loop->f_status == NS_SUCCESS)) {
				parse_ng_byuser(&fd->netgroups,
				    rq->f_loop->f_data);
			}
			nsd_file_clear(&rq->f_loop);
		}
		break;
	    case NSD_ERROR:
		/* remember we had an error and return */
		ds_clear(&ds, 0);
		fd->flags |= ERROR;
		if (fd->count) {
			return NSD_CONTINUE;
		} else {
			return NSD_ERROR;
		}
	    case NSD_CONTINUE:
		fd->count++;
	}
	
	/* login.domain */
	ds_set(&ds, key, len, 0);
	ds_append(&ds, ".", 1, 0);
	ds_append(&ds, fd->domain, fd->dlen, 0);

	l.lb_key = ds.data;

	status = nsd_loopback(rq, &l);
	switch (status) {
	    case NSD_OK:
		/* result was immediately available */
		if (rq->f_loop) {
			if (rq->f_loop->f_data &&
			    (rq->f_loop->f_status == NS_SUCCESS)) {
				parse_ng_byuser(&fd->netgroups,
				    rq->f_loop->f_data);
			}
			nsd_file_clear(&rq->f_loop);
		}
		break;
	    case NSD_ERROR:
		/* remember we had an error and return */
		ds_clear(&ds, 0);
		fd->flags |= ERROR;
		if (fd->count) {
			return NSD_CONTINUE;
		} else {
			return NSD_ERROR;
		}
	    case NSD_CONTINUE:
		fd->count++;
	}
	
	/* *.domain */
	ds_set(&ds, "*.", 2, 0);
	ds_append(&ds, fd->domain, fd->dlen, 0);
	
	l.lb_key = ds.data;
	
	status = nsd_loopback(rq, &l);
	switch (status) {
	    case NSD_OK:
		/* result was immediately available */
		if (rq->f_loop) {
			if (rq->f_loop->f_data &&
			    (rq->f_loop->f_status == NS_SUCCESS)) {
				parse_ng_byuser(&fd->netgroups,
				    rq->f_loop->f_data);
			}
			nsd_file_clear(&rq->f_loop);
		}
		break;
	    case NSD_ERROR:
		/* remember we had an error and return */
		ds_clear(&ds, 0);
		fd->flags |= ERROR;
		if (fd->count) {
			return NSD_CONTINUE;
		} else {
			return NSD_ERROR;
		}
	    case NSD_CONTINUE:
		fd->count++;
	}
	
	/* *.* */
	l.lb_key = "*.*";
	
	status = nsd_loopback(rq, &l);
	switch (status) {
	    case NSD_OK:
		/* result was immediately available */
		if (rq->f_loop) {
			if (rq->f_loop->f_data &&
			    (rq->f_loop->f_status == NS_SUCCESS)) {
				parse_ng_byuser(&fd->netgroups,
				    rq->f_loop->f_data);
			}
			nsd_file_clear(&rq->f_loop);
		}
		break;
	    case NSD_ERROR:
		/* remember we had an error and return */
		ds_clear(&ds, 0);
		fd->flags |= ERROR;
		if (fd->count) {
			return NSD_CONTINUE;
		} else {
			return NSD_ERROR;
		}
	    case NSD_CONTINUE:
		fd->count++;
	}
	
	ds_clear(&ds, 0);
	if (fd->count) {
		return NSD_CONTINUE;
	}
	return NSD_OK;
}
	

/*
** This is the simple passwd.byname lookup routine which does not
** support +/- escapes.
*/
static int
byname(nsd_file_t *rq)
{
	int len, clen;
	char *line, *key;
	file_domain_t *dom;
	int (*cmpfunc)(const char *, const char *, size_t);

	nsd_logprintf(NSD_LOG_MIN, "byname:\n");

	/*
	** Open the passwd file if not already done.
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
		return NSD_ERROR;
	}
	len = strlen(key);
	if (len <= 0) {
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
		clen = strcspn(line, ":\n");
		if (clen == len) {
			if ((*cmpfunc)(key, line, len) == 0) {
				len = strcspn(line, "\n");
				nsd_set_result(rq, NS_SUCCESS, line,
				    len, VOLATILE);
				nsd_append_result(rq, NS_SUCCESS, "\n", 1);
				return NSD_OK;
			}
		}
	}

	rq->f_status = NS_NOTFOUND;
	return NSD_NEXT;
}

/*
** Not threaded.
*/
/*ARGSUSED*/
static int
true(abilock_t *l)
{
	return 1;
}

static uint32_t
hash(dstring_t *ds)
{
	return ht_hash(ds->data, ds->used);
}

/*ARGSUSED*/
static void
no_free(void *a, void *b)
{
}

/*
** This is the state tree for byname.
**	0. Start - initialize state structure, open file.
**	1. Restart - reopen file if need be.
**	2. Lookup - fetching login information.
**	3. Netgroup - fetching netgroup information.
**	4. Walk - walking through file one line at a time looking for answer.
*/
static int
byname_compat(nsd_file_t *rq)
{
	file_data_t *fd;
	char *line;
	int status, len;
	dstring_t fake;
	
	nsd_logprintf(NSD_LOG_MIN, "byname_compat:\n");
	
	fd = (file_data_t *)rq->f_cmd_data;
	if (fd) {
		/* Restart */
		file_open(fd->dom);
		if ((fd->dom->version.tv_sec != fd->version.tv_sec) ||
		    (fd->dom->version.tv_usec != fd->version.tv_usec)) {
			fd->version = fd->dom->version;
			fd->offset = 0;
		}
	} else {
		/* Start */
		char *p;

		fd = nsd_calloc(1, sizeof(*fd));
		if (! fd) {
			nsd_status(rq, NS_TRYAGAIN);
			return NSD_ERROR;
		}
		rq->f_cmd_data = fd;
		
		p = nsd_attr_fetch_string(rq->f_attrs, "key", 0);
		if (! p) {
			file_data_clear(&rq->f_cmd_data);
			nsd_status(rq, NS_BADREQ);
			return NSD_ERROR;
		}
		if (! ds_set(&fd->login, p, 0, 0)) {
			nsd_logprintf(NSD_LOG_RESOURCE,
			    "byname_compat: malloc failed\n");
			file_data_clear(&rq->f_cmd_data);
			nsd_status(rq, NS_TRYAGAIN);
			return NSD_ERROR;
		}
		
		fd->dom = file_domain_lookup(rq);
		if (! fd->dom) {
			file_data_clear(&rq->f_cmd_data);
			nsd_status(rq, NS_NOTFOUND);
			return NSD_NEXT;
		}
		fd->version = fd->dom->version;
		
		line = nsd_attr_fetch_string(rq->f_attrs, "domain", 0);
		if (! line) {
			char buf[256];
			getdomainname(buf, sizeof(buf));
			line = buf;
		}
		fd->dlen = strlen(line);
		fd->domain = nsd_malloc(fd->dlen + 1);
		if (! fd->domain) {
			file_data_clear(&rq->f_cmd_data);
			nsd_status(rq, NS_TRYAGAIN);
			return NSD_ERROR;
		}
		memcpy(fd->domain, line, fd->dlen);
		fd->domain[fd->dlen] = 0;
		
		if (nsd_attr_fetch_bool(rq->f_attrs, "casefold", FALSE)) {
			fd->cmpfunc = strncasecmp;
		} else {
			fd->cmpfunc = strncmp;
		}

		if (! ht_init(&fd->netgroups, (int (*)(void *, void *))ds_cmp,
		    (void (*)(void *, void *))ds_free,
		    (void (*)(void *, void *))no_free,
		    (uint32_t (*)(void *))hash, true, true, 0, HT_CHECK_DUPS)) {
			file_data_clear(&rq->f_cmd_data);
			nsd_status(rq, NS_TRYAGAIN);
			return NSD_ERROR;
		}

		fd->state = 4;
	}
	
	if (fd->state == 2) {
		/* return from login lookup */
		if (rq->f_loop) {
			if (rq->f_loop->f_data &&
			    (rq->f_loop->f_status == NS_SUCCESS)) {
				if (! ds_set(&fd->result, rq->f_loop->f_data,
				    rq->f_loop->f_used, 0)) {
					nsd_file_clear(&rq->f_loop);
					file_data_clear(&rq->f_cmd_data);
					nsd_status(rq, NS_TRYAGAIN);
					return NSD_ERROR;
				}
			} else {
				fd->flags |= NO_LOGIN;
			}
			nsd_file_clear(&rq->f_loop);
			fd->flags |= LOGIN;
		} else {
			fd->flags |= LOGIN + NO_LOGIN;
		}
		fd->state = 4;
	}

	if (fd->state == 3) {
		/* return from netgroup lookup */
		if (rq->f_loop) {
			if (rq->f_loop->f_data &&
			    (rq->f_loop->f_status == NS_SUCCESS)) {
				parse_ng_byuser(&fd->netgroups,
				    rq->f_loop->f_data);
			}
			nsd_file_clear(&rq->f_loop);
		}
		fd->flags |= NETGRP;
		if (--fd->count > 0) {
			return NSD_CONTINUE;
		}
		if (fd->flags & ERROR) {
			file_data_clear(&rq->f_cmd_data);
			nsd_status(rq, NS_TRYAGAIN);
			return NSD_ERROR;
		}
		fd->state = 4;
	}

	for (line = fd->dom->map + fd->offset; line && *line;
	    line = strchr(line + 1, '\n')) {
		for (; *line && isspace(*line); line++);
		if (! *line || *line == '#') continue;
		fd->offset = line - fd->dom->map;
		if (*line == '-') {
			/* explicit skip */
			if (line[1] == '@') {
				/* netgroup */
				if (! (fd->flags & NETGRP)) {
					/*
					** We need to lookup the netgroup
					** information for this login.
					*/
					status = netgroup(rq, fd,
					    byname_compat, 
					    fd->login.data, fd->login.used);
					switch (status) {
					    case NSD_ERROR:
						file_data_clear(
						    &rq->f_cmd_data);
						nsd_status(rq, NS_TRYAGAIN);
						return NSD_ERROR;
					    case NSD_CONTINUE:
						fd->state = 3;
						return NSD_CONTINUE;
					    /* NSD_OK falls through */
					}
				}

				/*
				** We already know what netgroups this login is
				** in so we just check if this is one of them.
				*/
				len = strcspn(line + 2, ":\n");
				fake.data = line + 2;
				fake.used = len;
				if (ht_lookup(&fd->netgroups, &fake)) {
					file_data_clear(&rq->f_cmd_data);
					nsd_status(rq, NS_NOTFOUND);
					return NSD_OK;
				}
			} else {
				/* login */
				len = strcspn(line + 1, ":\n");
				if ((len == fd->login.used) &&
				    ((*fd->cmpfunc)(line + 1, fd->login.data,
				     len) == 0)) {
					file_data_clear(&rq->f_cmd_data);
					nsd_status(rq, NS_NOTFOUND);
					return NSD_OK;
				}
			}
		} else if (*line == '+') {
			/* add from following maps */
			if (! (fd->flags & LOGIN)) {
				/*
				** We need to fetch login information
				** from following callouts.
				*/
				status = loop_pw(rq, byname_compat, 0,
				    fd->login.data, fd->login.used);
				switch (status) {
				    case NSD_ERROR:
					file_data_clear(&rq->f_cmd_data);
					nsd_status(rq, NS_TRYAGAIN);
					return NSD_ERROR;
				    case NSD_CONTINUE:
					fd->state = 2;
					return NSD_CONTINUE;
				    /* NSD_OK case falls through */
				}
			}
			
			if (! (fd->flags & NO_LOGIN) &&
			    (! line[1] || (line[1] == ':') ||
			     (line[1] == '\n'))) {
				/* empty + means anything from following */
				len = build_result(line, fd->result.data);
				nsd_set_result(rq, NS_SUCCESS, _files_result,
				    len, VOLATILE);
				file_data_clear(&rq->f_cmd_data);
				return NSD_OK;
			}
			
			if ((line[1] == '@') && ! (fd->flags & NO_LOGIN)) {
				/* netgroup */
				if (! (fd->flags & NETGRP)) {
					/*
					** We need to lookup the netgroup
					** information for this login.
					*/
					status = netgroup(rq, fd,
					    byname_compat, fd->login.data,
					    fd->login.used);
					switch (status) {
					    case NSD_ERROR:
						file_data_clear(
						    &rq->f_cmd_data);
						nsd_status(rq, NS_TRYAGAIN);
						return NSD_ERROR;
					    case NSD_CONTINUE:
						fd->state = 3;
						return NSD_CONTINUE;
					    /* NSD_OK falls through */
					}
				}
				
				/*
				** We already know what netgroups this login is
				** in so we just check if this is one of them.
				*/
				len = strcspn(line + 2, ":\n");
				fake.data = line + 2;
				fake.used = len;
				if (ht_lookup(&fd->netgroups, &fake)) {
					len = build_result(line,
					    fd->result.data);
					nsd_set_result(rq, NS_SUCCESS,
					    _files_result, len, VOLATILE);
					file_data_clear(&rq->f_cmd_data);
					return NSD_OK;
				}
			} else if (! (fd->flags & NO_LOGIN)) {
				/* login */
				len = strcspn(line + 1, ":\n");
				if ((len == fd->login.used) &&
				    ((*fd->cmpfunc)(line + 1, fd->login.data,
				     len) == 0)) {
					len = build_result(line,
					    fd->result.data);
					nsd_set_result(rq, NS_SUCCESS,
					    _files_result, len, VOLATILE);
					file_data_clear(&rq->f_cmd_data);
					return NSD_OK;
				}
			}
		} else {
			/* the easy case, no +/- escapes */
			len = strcspn(line, ":\n");
			if ((len == fd->login.used) &&
			    ((*fd->cmpfunc)(line, fd->login.data, len) == 0)) {
				len = strcspn(line, "\n");
				nsd_set_result(rq, NS_SUCCESS, line, len,
				    VOLATILE);
				nsd_append_result(rq, NS_SUCCESS, "\n", 1);
				file_data_clear(&rq->f_cmd_data);
				return NSD_OK;
			}
		}
	}
	
	file_data_clear(&rq->f_cmd_data);
	nsd_status(rq, NS_NOTFOUND);
	return NSD_NEXT;
}

/*
** This is a simple utility routine to check the uid on a passwd line.
*/
static int
check_uid(char *line, uid_t uid)
{
	int count=0;
	char *p;
	uid_t check;
	
	for (; *line && *line != '\n'; line++) {
		if (*line == ':') {
			count++;
			continue;
		}
		if (count == 2) {
			check = strtol(line, &p, 10);
			if (check == uid && line != p) {
				return 1;
			}
			break;
		}
	}
	
	return 0;
}

/*
** This is the simple passwd.byuid lookup routine which does not
** support +/- escapes.
*/
static int
byuid(nsd_file_t *rq)
{
	char *line, *key;
	int len;
	uid_t uid;
	file_domain_t *dom;

	nsd_logprintf(NSD_LOG_MIN, "byuid:\n");

	/*
	** Open the passwd file if not already done.
	*/
	dom = file_domain_lookup(rq);
	if (! dom) {
		rq->f_status = NS_NOTFOUND;
		return NSD_NEXT;
	}

	/*
	** Start at the beginning and do a linear seach for the uid.
	*/
	key = nsd_attr_fetch_string(rq->f_attrs, "key", (char *)0);
	if (! key) {
		return NSD_ERROR;
	}
	uid = strtol(key, (char **)0, 10);
	for (line = dom->map; line && *line; line = strchr(line + 1, '\n')) {
		SKIPWSPACE(line);
		if (! *line || *line == '#') continue;
		if (check_uid(line, uid)) {
			len = strcspn(line, "\n");
			nsd_set_result(rq, NS_SUCCESS,
			    line, len, VOLATILE);
			nsd_append_result(rq, NS_SUCCESS, "\n",
			    sizeof("\n"));
			return NSD_OK;
		}
	}

	rq->f_status = NS_NOTFOUND;
	return NSD_NEXT;
}

/*
** This is the state tree for byuid.
**	0. Start - initialize state structure, open file.
**	1. Restart - reopen file if need be.
**	2. Lookup - fetching login information.
**	3. Netgroup - fetching netgroup information.
**	4. Walk - walking through file one line at a time looking for answer.
*/
static int
byuid_compat(nsd_file_t *rq)
{
	file_data_t *fd;
	char *line;
	int status, len;
	dstring_t fake;

	nsd_logprintf(NSD_LOG_MIN, "byuid_compat:\n");
	
	fd = (file_data_t *)rq->f_cmd_data;
	if (fd) {
		/* Restart */
		file_open(fd->dom);
		if (fd->dom->version.tv_sec != fd->version.tv_sec) {
			fd->version = fd->dom->version;
			fd->offset = 0;
		}
	} else {
		/* Start */
		char *p, *q;

		fd = nsd_calloc(1, sizeof(*fd));
		if (! fd) {
			nsd_status(rq, NS_TRYAGAIN);
			return NSD_ERROR;
		}
		rq->f_cmd_data = fd;
		
		p = nsd_attr_fetch_string(rq->f_attrs, "key", 0);
		if (! p) {
			file_data_clear(&rq->f_cmd_data);
			nsd_status(rq, NS_BADREQ);
			return NSD_ERROR;
		}
		fd->uid = strtol(p, &q, 10);
		if (p == q) {
			nsd_logprintf(NSD_LOG_MIN,
			    "byuid_compat: bad key: %s\n", p);
			file_data_clear(&rq->f_cmd_data);
			nsd_status(rq, NS_BADREQ);
			return NSD_ERROR;
		}
		if (! ds_set(&fd->login, p, 0, 0)) {
			nsd_logprintf(NSD_LOG_RESOURCE,
			    "byname_compat: malloc failed\n");
			file_data_clear(&rq->f_cmd_data);
			nsd_status(rq, NS_TRYAGAIN);
			return NSD_ERROR;
		}

		fd->dom = file_domain_lookup(rq);
		if (! fd->dom) {
			file_data_clear(&rq->f_cmd_data);
			nsd_status(rq, NS_NOTFOUND);
			return NSD_NEXT;
		}
		fd->version = fd->dom->version;
		
		line = nsd_attr_fetch_string(rq->f_attrs, "domain", 0);
		if (! line) {
			char buf[256];
			getdomainname(buf, sizeof(buf));
			line = buf;
		}
		fd->dlen = strlen(line);
		fd->domain = nsd_malloc(fd->dlen + 1);
		if (! fd->domain) {
			file_data_clear(&rq->f_cmd_data);
			nsd_status(rq, NS_TRYAGAIN);
			return NSD_ERROR;
		}
		memcpy(fd->domain, line, fd->dlen);
		fd->domain[fd->dlen] = 0;

		if (nsd_attr_fetch_bool(rq->f_attrs, "casefold", FALSE)) {
			fd->cmpfunc = strncasecmp;
		} else {
			fd->cmpfunc = strncmp;
		}
		
		if (! ht_init(&fd->netgroups, (int (*)(void *, void *))ds_cmp,
		    (void (*)(void *, void *))ds_free,
		    (void (*)(void *, void *))no_free,
		    (uint32_t (*)(void *))hash, true, true, 0, HT_CHECK_DUPS)) {
			file_data_clear(&rq->f_cmd_data);
			nsd_status(rq, NS_TRYAGAIN);
			return NSD_ERROR;
		}


		fd->state = 4;
	}
	
	if (fd->state == 2) {
		/* return from login lookup */
		if (rq->f_loop) {
			if (rq->f_loop->f_data &&
			    (rq->f_loop->f_status == NS_SUCCESS)) {
				if (! ds_set(&fd->result, rq->f_loop->f_data,
				    rq->f_loop->f_used, 0) ||
				    ! ds_set(&fd->login, rq->f_loop->f_data,
				    strcspn(rq->f_loop->f_data, ":\n"), 0)) {
					nsd_file_clear(&rq->f_loop);
					file_data_clear(&rq->f_cmd_data);
					nsd_status(rq, NS_TRYAGAIN);
					return NSD_ERROR;
				}
			} else {
				fd->flags |= NO_LOGIN;
			}
			nsd_file_clear(&rq->f_loop);
			fd->flags |= LOGIN;
		} else {
			fd->flags |= LOGIN + NO_LOGIN;
		}
		fd->state = 4;
	}

	if (fd->state == 3) {
		/* return from netgroup lookup */
		if (rq->f_loop) {
			if (rq->f_loop->f_data &&
			    (rq->f_loop->f_status == NS_SUCCESS)) {
				parse_ng_byuser(&fd->netgroups,
				    rq->f_loop->f_data);
			}
			nsd_file_clear(&rq->f_loop);
		}
		fd->flags |= NETGRP;
		if (--fd->count > 0) {
			return NSD_CONTINUE;
		}
		if (fd->flags & ERROR) {
			file_data_clear(&rq->f_cmd_data);
			nsd_status(rq, NS_TRYAGAIN);
			return NSD_ERROR;
		}
		fd->state = 4;
	}

	for (line = fd->dom->map + fd->offset; line && *line;
	    line = strchr(line + 1, '\n')) {
		for (; *line && isspace(*line); line++);
		if (! *line || *line == '#') continue;
		fd->offset = line - fd->dom->map;
		if (*line == '-') {
			/* explicit skip */
			if (! (fd->flags & LOGIN)) {
				/*
				** We need to fetch login information
				** from following callouts.
				*/
				status = loop_pw(rq, byuid_compat, 0,
				    fd->login.data, fd->login.used);
				switch (status) {
				    case NSD_ERROR:
					file_data_clear(&rq->f_cmd_data);
					nsd_status(rq, NS_TRYAGAIN);
					return NSD_ERROR;
				    case NSD_CONTINUE:
					fd->state = 2;
					return NSD_CONTINUE;
				    /* NSD_OK case falls through */
				}
			}

			if (line[1] == '@') {
				/* netgroup */
				if (! (fd->flags & NETGRP)) {
					/*
					** We need to lookup the netgroup
					** information for this login.
					*/
					status = netgroup(rq, fd,
					    byuid_compat, fd->login.data,
					    fd->login.used);
					switch (status) {
					    case NSD_ERROR:
						file_data_clear(
						    &rq->f_cmd_data);
						nsd_status(rq, NS_TRYAGAIN);
						return NSD_ERROR;
					    case NSD_CONTINUE:
						fd->state = 3;
						return NSD_CONTINUE;
					    /* NSD_OK falls through */
					}
				}

				/*
				** We already know what netgroups this login is
				** in so we just check if this is one of them.
				*/
				fake.data = line + 2;
				fake.used = strcspn(line + 2, ":\n");;
				if (ht_lookup(&fd->netgroups, &fake)) {
					file_data_clear(&rq->f_cmd_data);
					nsd_status(rq, NS_NOTFOUND);
					return NSD_OK;
				}
			} else {
				/* login */
				len = strcspn(line + 1, ":\n");
				if ((len == fd->login.used) &&
				    ((*fd->cmpfunc)(line + 1, fd->login.data,
				     len) == 0)) {
					file_data_clear(&rq->f_cmd_data);
					nsd_status(rq, NS_NOTFOUND);
					return NSD_OK;
				}
			}
		} else if (*line == '+') {
			/* add from following maps */
			if (! (fd->flags & LOGIN)) {
				/*
				** We need to fetch login information
				** from following callouts.
				*/
				status = loop_pw(rq, byuid_compat, 0,
				    fd->login.data, fd->login.used);
				switch (status) {
				    case NSD_ERROR:
					file_data_clear(&rq->f_cmd_data);
					nsd_status(rq, NS_TRYAGAIN);
					return NSD_ERROR;
				    case NSD_CONTINUE:
					fd->state = 2;
					return NSD_CONTINUE;
				    /* NSD_OK case falls through */
				}
			}
			
			if (! (fd->flags & NO_LOGIN) &&
			    (! line[1] || (line[1] == ':') ||
			     (line[1] == '\n'))) {
				/* empty + means anything from following */
				len = build_result(line, fd->result.data);
				nsd_set_result(rq, NS_SUCCESS, _files_result,
				    len, VOLATILE);
				file_data_clear(&rq->f_cmd_data);
				return NSD_OK;
			}
			
			if ((line[1] == '@') && ! (fd->flags & NO_LOGIN)) {
				/* netgroup */
				if (! (fd->flags & NETGRP)) {
					/*
					** We need to lookup the netgroup
					** information for this login.
					*/
					status = netgroup(rq, fd,
					    byuid_compat, fd->login.data,
					    fd->login.used);
					switch (status) {
					    case NSD_ERROR:
						file_data_clear(
						    &rq->f_cmd_data);
						nsd_status(rq, NS_TRYAGAIN);
						return NSD_ERROR;
					    case NSD_CONTINUE:
						fd->state = 3;
						return NSD_CONTINUE;
					    /* NSD_OK falls through */
					}
				}
				
				/*
				** We already know what netgroups this login is
				** in so we just check if this is one of them.
				*/
				fake.data = line + 2;
				fake.used = strcspn(fake.data, ":\n");
				if (ht_lookup(&fd->netgroups, &fake)) {
					len = build_result(line,
					    fd->result.data);
					nsd_set_result(rq, NS_SUCCESS,
					    _files_result, len, VOLATILE);
					file_data_clear(&rq->f_cmd_data);
					return NSD_OK;
				}
			} else if (! (fd->flags & NO_LOGIN)) {
				/* login */
				len = strcspn(line + 1, ":\n");
				if ((len == fd->login.used) &&
				    ((*fd->cmpfunc)(line + 1, fd->login.data,
				     len) == 0)) {
					len = build_result(line,
					    fd->result.data);
					nsd_set_result(rq, NS_SUCCESS,
					    _files_result, len, VOLATILE);
					file_data_clear(&rq->f_cmd_data);
					return NSD_OK;
				}
			}
		} else {
			/* the simple case, no +/- escapes */
			if (check_uid(line, fd->uid)) {
				len = strcspn(line, "\n");
				nsd_set_result(rq, NS_SUCCESS, line, len,
				    VOLATILE);
				nsd_append_result(rq, NS_SUCCESS, "\n", 1);
				file_data_clear(&rq->f_cmd_data);
				return NSD_OK;
			}
		}
	}
	
	file_data_clear(&rq->f_cmd_data);
	nsd_status(rq, NS_NOTFOUND);
	return NSD_NEXT;
}

/*
** This is the lookup routine for passwd.  It looks up the table and compat
** attributes and calls the appropriate function above.
*/
int
file_passwd_lookup(nsd_file_t *rq)
{
	char *map;
	int compat;

	if (! rq) {
		return NSD_ERROR;
	}

	map = nsd_attr_fetch_string(rq->f_attrs, "table", (char *)0);
	if (! map) {
		return NSD_ERROR;
	}

	compat = nsd_attr_fetch_bool(rq->f_attrs, "compat", FALSE);

	if (strcasecmp(map, PASSWD_BYNAME) == 0) {
		if (compat) {
			return byname_compat(rq);
		} else {
			return byname(rq);
		}
	} else {
		if (compat) {
			return byuid_compat(rq);
		} else {
			return byuid(rq);
		}
	}
}

#define ROUNDUP(a, b) ((a > 0) ? (((a) + (b) - 1) / (b)) * (b) : (b))

/*
** This is just like nsd_append_result but only uses nsd_bmalloc, and
** allocates things in larger chunks.
*/
static int
append_result(nsd_file_t *rq, char *buf, int len)
{
	size_t needed;
	
	while (buf[len - 1] == 0) {
		len--;
	}

	needed = ROUNDUP(rq->f_used + len + 1, __pagesize);
	if (needed > rq->f_size) {
		rq->f_data = nsd_brealloc(rq->f_data, needed);
		if (! rq->f_data) {
			rq->f_used = rq->f_size = 0;
			return 0;
		}
		rq->f_size = needed;
		rq->f_free = nsd_bfree;
	}
	memcpy(rq->f_data + rq->f_used, buf, len);
	rq->f_used += len;
	
	return 1;
}

/*
** Because we don't use the standard nsd_append_result we first have to
** copy any data into a bmalloc'd buffer.
*/
static int
append_init(nsd_file_t *rq)
{
	char *n;
	size_t needed;
	
	if (rq->f_data && (rq->f_free != nsd_bfree)) {
		if (rq->f_used) {
			needed = ROUNDUP(rq->f_used + 1, __pagesize);
			n = nsd_bmalloc(needed);
			if (! n) {
				return 0;
			}
			
			memcpy(n, rq->f_data, rq->f_used);
			if (rq->f_free) {
				(*rq->f_free)(rq->f_data);
			}
			rq->f_data = n;
			rq->f_free = nsd_bfree;
			rq->f_size = needed;
		} else {
			if (rq->f_free) {
				(*rq->f_free)(rq->f_data);
			}
			rq->f_data = 0;
			rq->f_used = rq->f_size = 0;
			rq->f_free = 0;
		}
	}
	
	return 1;
}

static int
append_val(dstring_t *d, dstring_t *s, void *arena)
{
	if (! s || ! d) {
		return 0;
	}
	if (d->used == 0) {
		if (! ds_append(d, ",", 1, arena)) {
			return 0;
		}
	}
	if (*s->data == '*') {
		if (! ds_set(d, "*", 1, arena)) {
			return 0;
		}
	} else if (! ds_append(d, s->data, s->used, arena)) {
		return 0;
	}

	return 1;
}

/*
** This routine just expands a netgroup recursively into a list of login
** names.
*/
static int
do_line(ng_names_t *names, dstring_t *kp, dstring_t *vp, nsd_file_t *rq)
{
	ng_names_t this, *tp, *first;
	char *p, *q;
	file_data_t *fd = rq->f_cmd_data;
	dstring_t *ds, *val, user, fake;
	
	val = m_calloc(1, sizeof(*val), fd->arena);
	if (! val) {
		return 0;
	}
	
	if (! ds_init(val, 100, fd->arena) ||
	    ! ds_init(&user, 10, fd->arena)) {
		return 0;
	}
	
	for (first = names; first->last; first = first->last);
	
	for (p = vp->data; p; ) {
		for (; *p && isspace(*p); p++);
		if (! *p) {
			break;
		}
		
		if (*p == '(') {
			/* triplet */
			
			/* skip host */
			for (++p; *p && isspace(*p); p++);
			for (; *p && ! isspace(*p) && (*p != ',') &&
			    (*p != ')'); p++);
			if (! *p) {
				nsd_logprintf(NSD_LOG_MIN,
			    "do_line: unexpected end of line in netgroup: %s\n",
				    kp->data);
				break;
			}
			for (; *p && isspace(*p); p++);
			if (*p++ != ',') {
				nsd_logprintf(NSD_LOG_MIN,
				    "do_line: expected ',' in netgroup: %s\n",
				    kp->data);
				continue;
			}
			
			/* user */
			for (q = p; *q && ! isspace(*q) && (*q != ',') &&
			    (*q != ')'); q++);
			if (! *q) {
				nsd_logprintf(NSD_LOG_MIN,
			    "do_line: unexpected end of line in netgroup: %s\n",
				    kp->data);
				break;
			}
			if (p == q) {
				ds_reset(&user);
			} else {
				if (! ds_set(&user, p, q - p, fd->arena)) {
					goto parse_error;
				}
			}
			for (; *q && isspace(*q); q++);
			if (*q++ != ',') {
				nsd_logprintf(NSD_LOG_MIN,
				    "do_line: expected ',' in netgroup: %s\n",
				    kp->data);
				p = q;
				continue;
			}
			
			/* domain */
			for (p = q; *p && ! isspace(*p) && (*p != ',') &&
			    (*p != ')'); p++);
			if (! *p) {
				nsd_logprintf(NSD_LOG_MIN,
			    "do_line: unexpected end of line in netgroup: %s\n",
				    kp->data);
				break;
			}
			if ((p == q) ||
			    ((fd->dlen == (p - q)) &&
			     (strncasecmp(fd->domain, q, fd->dlen) == 0))) {
				if (val->used == 0) {
					if (! ds_append(val, ",", 1,
					    fd->arena)) {
						goto parse_error;
					}
				}
				if (user.used > 0) {
					if (! ds_append(val, ",", 1,
					    fd->arena) ||
					    ! ds_append(val, user.data,
					    user.used, fd->arena)) {
						goto parse_error;
					}
				} else {
					ds_set(vp, "*", 1, fd->arena);
					break;
				}
			}
			for (; *p && isspace(*p); p++);
			if (*p++ != ')') {
				nsd_logprintf(NSD_LOG_MIN,
				    "do_line: expected ')' in netgroup: %s\n",
				    kp->data);
				break;
			}
		} else {
			/* netgroup */
			for (q = p; *p && ! isspace(*p); p++);
			this.name = q;
			this.len = p - q;
			if (this.len == 0) {
				continue;
			}
			this.last = names;
			for (tp = names; tp; tp = tp->last) {
				if (tp->len == this.len &&
				    (memcmp(tp->name, this.name,
					    this.len) == 0)) {
					break;
				}
			}
			if (! tp) {
				fake.data = this.name;
				fake.used = this.len;
				ds = ht_lookup(&fd->netgroups, &fake);
				if (ds) {
					if (! append_val(val, ds, fd->arena)) {
						goto parse_error;
					}
				} else {
					/* Netgroup has not been parsed yet. */
					ds = ht_lookup(&fd->tmp, &fake);
					if (ds) {
						do_line(&this, &fake, ds, rq);
						ds = ht_lookup(&fd->netgroups,
						    &fake);
						if (ds) {
							if (! append_val(val,
							    ds, fd->arena)) {
								goto
								    parse_error;
							}
						}
					}
				}
			}
		}
	}
	
	ds = ds_new(kp->data, kp->used, fd->arena);
	if (! ds) {
		ds_free(val, fd->arena);
	} else if (! ht_insert(&fd->netgroups, ds, val, 0)) {
		ds_free(val, fd->arena);
		ds_free(ds, fd->arena);
	}
	
	ds_clear(&user, fd->arena);
	
	return 1;

parse_error:
	ds_free(val, fd->arena);
	ds_clear(&user, fd->arena);
	
	return 0;
}

/*
** This is the top of the routine above.  We call the do_line routine
** to unroll a netgroup into a list of logins.
*/
static int
recurse(dstring_t *kp, dstring_t *vp, nsd_file_t *rq)
{
	ng_names_t this;
	
	this.name = kp->data;
	this.len = kp->used;
	this.last = 0;
	do_line(&this, kp, vp, rq);
	
	return 1;
}

/*
** This routine will parse a netgroup into a hash, then call the routines
** above to recursively unroll them into lists of logins.
*/
static int
parse_ng(nsd_file_t *rq, char *buf)
{
	file_data_t *fd = rq->f_cmd_data;
	dstring_t *k, *v;
	char *p, *q;

	for (; buf && *buf; buf = strchr(buf + 1, '\n')) {
		SKIPWSPACE(buf);
		if (*buf == '#') {
			continue;
		}
		
		/*
		** Determine key.
		*/
		for (p = buf; *p && ! isspace(*p); p++);
		if (p == buf) {
			continue;
		}
		k = ds_new(buf, p - buf, fd->arena);
		if (! k) {
			return 0;
		}

		v = m_calloc(1, sizeof(*v), fd->arena);
		if (! v) {
			ds_free(k, fd->arena);
			return 0;
		}
		
		/*
		** Copy in line.
		*/
		for (;; buf = p) {
			for (; *p && (*p == '\t' || *p == ' '); p++);
			for (q = p; *q && *q != '\\' && *q != '\n'; q++);
			
			if (q != p && *p != '#') {
				if (! ds_append(v, p, q - p, fd->arena)) {
					ds_free(k, fd->arena);
					ds_free(v, fd->arena);
					return 0;
				}
			}
			
			if (*q == '\\') {
				p = q + 2;
			} else if (*p == '#') {
				p = q + 1;
			} else {
				break;
			}
		}
		
		if (! ht_insert(&fd->tmp, k, v, 0)) {
			ds_free(k, fd->arena);
			ds_free(v, fd->arena);
			continue;
		}
	}
	
	/*
	** We loop over every element expanding it to a list of login
	** names.
	*/
	ht_walk(&fd->tmp, rq, (int (*)(void *, void *, void *))recurse);
	
	return 1;
}

/*
** This routine will parse a passwd buffer into a hash table.
*/
static int
parse_pw(file_data_t *fd, char *buf)
{
	dstring_t *k, *v;

	for (; buf && *buf; buf = strchr(buf + 1, '\n')) {
		for (; *buf && isspace(*buf); buf++);
		switch (*buf) {
		    case '+':
		    case '-':
		    case '#':
			break;
		    default:
			k = ds_new(buf, strcspn(buf, ":\n"), fd->arena);
			if (! k) {
				return 0;
			}

			v = ds_new(buf, strcspn(buf, "\n"), fd->arena);
			if (! v) {
				ds_free(k, fd->arena);
				return 0;
			}

			if (! ht_insert(&fd->logins, k, v, 0)) {
				ds_free(k, fd->arena);
				ds_free(v, fd->arena);
			}
		}
	}
	
	return 1;
}

/*
** This just adds the given key to the skip hash.
*/
static int
skip_login(file_data_t *fd, char *key, int len)
{
	dstring_t *k;
	
	k = ds_new(key, len, fd->arena);
	if (! k) {
		return 0;
	}
	if (! ht_insert(&fd->skip, k, (void *)1, 0)) {
		ds_free(k, fd->arena);
		return 1;
	}
	
	return 1;
}

/*
** Add an entry in the skip hash for each login in this netgroup.
*/
static int
skip_netgroup(file_data_t *fd, char *key, int len)
{
	dstring_t *ds, fake;
	char *p, *q;
	
	fake.data = key;
	fake.used = len;
	ds = ht_lookup(&fd->netgroups, &fake);
	if (! ds|| (ds->used <= 0)) {
		return 1;
	}
	
	for (p = ds->data; *p;) {
		for (q = p; *q && *q != ','; q++);
		
		if (*q == '*') {
			fd->flags |= SKIP_ALL;
			return 1;
		}
		if (! skip_login(fd, p, q - p)) {
			return 0;
		}

		for (p = q; *p && *p == ','; p++);
	}
	
	return 1;
}

/*
** This looks for the key, or a wildcard in the skip hash.
*/
static int
skip(file_data_t *fd, char *key, int len)
{
	dstring_t fake;

	fake.data = key;
	fake.used = len;
	if ((fd->flags & SKIP_ALL) || ht_lookup(&fd->skip, &fake)) {
		return 1;
	}
	
	return 0;
}

/*
** This just appends the value for this node to the result.
*/
static int
add_line(dstring_t *kp, dstring_t *vp, nsd_file_t *rq)
{
	file_data_t *fd = rq->f_cmd_data;
	int len;
	
	if ((vp->used > 0) &&
	    (! skip(fd, kp->data, kp->used))) {
		if (fd->merge) {
			len = build_result(fd->merge, vp->data);
			append_result(rq, _files_result, len);
		} else {
			append_result(rq, vp->data, vp->used);
			append_result(rq, "\n", 1);
		}

		ds_clear(vp, fd->arena); /* once seen never needed again */
	}
	
	return 1;
}

/*
** This adds all the lines for users in a netgroup.  If the netgroup
** contains a wildcard we add the entire file.  We expect the logins
** hash to already be filled.
*/
static int
add_netgroup(nsd_file_t *rq, file_data_t *fd, char *key, int klen)
{
	dstring_t *ds, fake;
	char *p, *q;
	int len;
	
	fake.data = key;
	fake.used = klen;
	ds = ht_lookup(&fd->netgroups, &fake);
	if (! ds || (ds->used <= 0)) {
		return 1;
	}
	
	for (p = ds->data; *p;) {
		for (q = p; *q && *q != ','; q++);

		len = q - p;
		if (len) {
			if (*p == '*') {
				ht_walk(&fd->logins, rq,
				    (int (*)(void *, void *, void *))add_line);
				return 1;
			} else if (! skip(fd, p, len)) {
				fake.data = p;
				fake.used = len;
				ds = ht_lookup(&fd->logins, &fake);
				if (ds && (ds->used > 0)) {
					if (fd->merge) {
						len = build_result(fd->merge,
						    ds->data);
						append_result(rq, _files_result,
						    len);
					} else {
						append_result(rq, ds->data,
						    ds->used);
						append_result(rq, "\n", 1);
					}

					/* only need to append line once */
					ds_clear(ds, fd->arena);
				}
			}
		}

		for (p = q; *p && *p == ','; p++);
	}
	
	return 1;
}

/*
** This is the state tree for list.
**	0. Start - initialize state structure, open file.
**	1. Restart - reopen file if need be.
**	2. Lookup - fetching single login information.
**	3. List - fetching login enumeration.
**	4. Netgroup - fetching netgroup information.
**	5. Walk - walking through file one line at a time appending to answer.
*/
static int
list_compat(nsd_file_t *rq)
{
	file_data_t *fd;
	char *line;
	dstring_t *k, *v, *ds, fake;
	int status, len;

	nsd_logprintf(NSD_LOG_MIN, "list_compat:\n");
	
	fd = (file_data_t *)rq->f_cmd_data;
	if (fd) {
		/* Restart */
		file_open(fd->dom);
		if ((fd->dom->version.tv_sec != fd->version.tv_sec) ||
		    (fd->dom->version.tv_usec != fd->version.tv_usec)) {
			fd->version = fd->dom->version;
			fd->offset = 0;
		}
	} else {
		/* Start */
		fd = nsd_calloc(1, sizeof(*fd));
		if (! fd) {
			nsd_status(rq, NS_TRYAGAIN);
			return NSD_ERROR;
		}
		rq->f_cmd_data = fd;
		

		fd->dom = file_domain_lookup(rq);
		if (! fd->dom) {
			file_data_clear(&rq->f_cmd_data);
			nsd_status(rq, NS_NOTFOUND);
			return NSD_NEXT;
		}
		fd->version = fd->dom->version;
		
		line = nsd_attr_fetch_string(rq->f_attrs, "domain", 0);
		if (! line) {
			char buf[256];
			getdomainname(buf, sizeof(buf));
			line = buf;
		}
		fd->dlen = strlen(line);
		fd->domain = nsd_malloc(fd->dlen + 1);
		if (! fd->domain) {
			goto no_memory;
		}
		memcpy(fd->domain, line, fd->dlen);
		fd->domain[fd->dlen] = 0;
		
		if (! append_init(rq)) {
			goto no_memory;
		}

		fd->arena = m_arena();
		if (! fd->arena) {
			goto no_memory;
		}

		if (! ht_init(&fd->netgroups, (int (*)(void *, void *))ds_cmp,
			    (void (*)(void *, void *))ds_free,
			    (void (*)(void *, void *))ds_free,
			    (uint32_t (*)(void *))hash, true, true, fd->arena,
			    HT_CHECK_DUPS) ||
		    ! ht_init(&fd->tmp, (int (*)(void *, void *))ds_cmp,
			    (void (*)(void *, void *))ds_free,
			    (void (*)(void *, void *))no_free,
			    (uint32_t (*)(void *))hash, true, true, fd->arena,
			    HT_CHECK_DUPS) ||
		    ! ht_init(&fd->skip, (int (*)(void *, void *))ds_cmp,
			    (void (*)(void *, void *))ds_free,
			    (void (*)(void *, void *))no_free,
			    (uint32_t (*)(void *))hash, true, true, fd->arena,
			    HT_CHECK_DUPS) ||
		    ! ht_init(&fd->logins, (int (*)(void *, void *))ds_cmp,
			    (void (*)(void *, void *))ds_free,
			    (void (*)(void *, void *))ds_free,
			    (uint32_t (*)(void *))hash, true, true, fd->arena,
			    HT_CHECK_DUPS)) {
			goto no_memory;
		}
		
		fd->state = 5;
	}
	
	if (fd->state == 2) {
		/* return from login lookup */
		if (rq->f_loop) {
			if (rq->f_loop->f_data &&
			    (rq->f_loop->f_status == NS_SUCCESS)) {
				k = ds_new(rq->f_loop->f_data,
				    strcspn(rq->f_loop->f_data, ":\n"),
				    fd->arena);
				if (! k) {
					nsd_file_clear(&rq->f_loop);
					goto no_memory;
				}

				v = ds_new(rq->f_loop->f_data,
				    strcspn(rq->f_loop->f_data, "\n"),
				    fd->arena);
				if (! v) {
					ds_free(k, fd->arena);
					nsd_file_clear(&rq->f_loop);
					goto no_memory;
				}

				if (! ht_insert(&fd->logins, k, v, 0)) {
					ds_free(k, fd->arena);
					ds_free(v, fd->arena);
				}
			}
			nsd_file_clear(&rq->f_loop);
		}
		fd->flags |= RESULT;
		fd->state = 5;
	}
	
	if (fd->state == 3) {
		/* return from password enumeration */
		if (rq->f_loop) {
			if (rq->f_loop->f_data &&
			    (rq->f_loop->f_status == NS_SUCCESS)) {
				if (! parse_pw(fd, rq->f_loop->f_data)) {
					goto no_memory;
				}
			}
			nsd_file_clear(&rq->f_loop);
		}
		fd->flags |= LOGIN;
		fd->state = 5;
	}

	if (fd->state == 4) {
		/* return from netgroup enumeration */
		if (rq->f_loop) {
			if (rq->f_loop->f_data &&
			    (rq->f_loop->f_status == NS_SUCCESS)) {
				parse_ng(rq, rq->f_loop->f_data);
			}
			nsd_file_clear(&rq->f_loop);
		}
		fd->flags |= NETGRP;
		fd->state = 5;
	}

	for (line = fd->dom->map + fd->offset; line && *line;
	    line = strchr(line + 1, '\n')) {
		for (; *line && isspace(*line); line++);
		if (! *line || *line == '#') continue;
		fd->offset = line - fd->dom->map;
		if (*line == '-') {
			if (line[1] == '@') {
				/* netgroup */
				if (! (fd->flags & NETGRP)) {
					/*
					** We need to enumerate the netgroup
					** information.
					*/
					status = loop_ng(rq, list_compat, 
					    "netgroup", 0, 0);
					switch (status) {
					    case NSD_ERROR:
						goto no_memory;
					    case NSD_CONTINUE:
						fd->state = 4;
						return NSD_CONTINUE;
					    /* NSD_OK falls through */
					}
				}
				
				if (! (fd->flags & LOGIN)) {
					/*
					** We need to enumerate the following
					** passwd information.
					*/
					status = loop_pw(rq, list_compat,
					    "passwd.byname", 0, 0);
					switch(status) {
					    case NSD_ERROR:
						goto no_memory;
					    case NSD_CONTINUE:
						fd->state = 3;
						return NSD_CONTINUE;
					    /* NSD_OK falls through */
					}
				}

				/*
				** We add each login for this netgroup
				** to the skip hash.
				*/
				len = strcspn(line + 2, ":\n");
				if (! skip_netgroup(fd, line + 2, len)) {
					goto no_memory;
				}
			} else {
				/*
				** We add this login to the skip hash.
				*/
				len = strcspn(line + 1, ":\n");
				if (! skip_login(fd, line + 1, len)) {
					goto no_memory;
				}
			}
		} else if (*line == '+') {
			/* add from following maps */
			len = strcspn(line + 1, ":\n");
			if (len == 0) {
				/* empty + means anything from following */
				if (! (fd->flags & LOGIN)) {
					/*
					** We need to enumerate the following
					** passwd information.
					*/
					status = loop_pw(rq, list_compat,
					    "passwd.byname", 0, 0);
					switch(status) {
					    case NSD_ERROR:
						goto no_memory;
					    case NSD_CONTINUE:
						fd->state = 3;
						return NSD_CONTINUE;
					    /* NSD_OK falls through */
					}
				}
				
				/*
				** Walk the login tree adding each item.
				*/
				if (! (fd->flags & DONE_ALL)) {
					fd->merge = line;
					ht_walk(&fd->logins, rq,
				    (int (*)(void *, void *, void *))add_line);
					fd->flags |= DONE_ALL;
				}
				continue;
			}

			if (line[1] == '@') {
				/* netgroup */
				if (! (fd->flags & NETGRP)) {
					/*
					** We need to enumerate the netgroup
					** information.
					*/
					status = loop_ng(rq, list_compat, 
					    "netgroup", 0, 0);
					switch (status) {
					    case NSD_ERROR:
						goto no_memory;
					    case NSD_CONTINUE:
						fd->state = 4;
						return NSD_CONTINUE;
					    /* NSD_OK falls through */
					}
				}
				
				if (! (fd->flags & LOGIN)) {
					/*
					** We need to enumerate the following
					** passwd information.
					*/
					status = loop_pw(rq, list_compat,
					    "passwd.byname", 0, 0);
					switch(status) {
					    case NSD_ERROR:
						goto no_memory;
					    case NSD_CONTINUE:
						fd->state = 3;
						return NSD_CONTINUE;
					    /* NSD_OK falls through */
					}
				}
			
				if (! (fd->flags & DONE_ALL)) {
					len = strcspn(line + 2, ":\n");
					fd->merge = line;
					if (! add_netgroup(rq, fd, line + 2,
					    len)) {
						goto no_memory;
					}
				}
			} else {
				/* skip if excluded earlier */
				if (skip(fd, line, len) ||
				    (fd->flags & DONE_ALL)) {
					continue;
				}

				/* check our login cache */
				fake.data = line + 1;
				fake.used = len;
				ds = ht_lookup(&fd->logins, &fake);
				if (ds) {
					len = build_result(line,
					    ds->data);
					append_result(rq, _files_result, len);

					/* only once */
					ds_clear(ds, fd->arena);
					fd->flags &= ~RESULT;
					continue;
				}
				
				if (! (fd->flags & RESULT) &&
				    ! (fd->flags & LOGIN)) {
					/*
					** We need to fetch login information
					** from following callouts.
					*/
					status = loop_pw(rq, list_compat,
					    "passwd.byname", line + 1, len);
					switch (status) {
					    case NSD_ERROR:
						goto no_memory;
					    case NSD_CONTINUE:
						fd->state = 2;
						return NSD_CONTINUE;
					    /* NSD_OK case falls through */
					}
				}
				fd->flags &= ~RESULT;
			}
		} else {
			/* simple case, no +/- escapes */
			len = strcspn(line, "\n");
			append_result(rq, line, len);
			append_result(rq, "\n", 1);
		}
	}
	
	file_data_clear(&rq->f_cmd_data);
	nsd_status(rq, NS_SUCCESS);
	return NSD_OK;

no_memory:
	file_data_clear(&rq->f_cmd_data);
	nsd_status(rq, NS_TRYAGAIN);
	return NSD_ERROR;
}

/*
** This is the list function for passwd.  If the compat flag is set we
** just call the more complex routine above.
*/
int
file_passwd_list(nsd_file_t *rq)
{
	file_domain_t *dom;
	int compat;

	if (! rq) {
		return NSD_ERROR;
	}

	compat = nsd_attr_fetch_bool(rq->f_attrs, "compat", FALSE);
	if (compat) {
		return list_compat(rq);
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
