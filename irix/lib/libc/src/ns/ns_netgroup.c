#ifdef __STDC__
	#pragma weak innetgr = _innetgr
#endif
#include "synonyms.h"

#include <stdio.h>
#include <ctype.h>
#include <strings.h>
#include <ns_api.h>

#define MAXLINE	4096

static ns_map_t _netgroup;
static ns_map_t _netgroup_byuser;
static ns_map_t _netgroup_byhost;

typedef struct group {
	size_t		len;
	char		*value;
	struct group	*next;
} group_t;

/*
** This just builds up a key for the netgroup.byuser and netgroup.byhost
** maps.  The first half of the key is case insensitive which means that
** it is stored with a lowercase key in NIS.
*/
static void
mkkey(char *name, char *domain, char *key, size_t len)
{
	int i;

	len--;
	for (i = 0; *name && (i < len); i++) {
		*key++ = tolower(*name++);
	}
	if (i < len) {
		*key++ = '.';
		i++;
	}
	for (; *domain && (i < len); i++) {
		*key++ = *domain++;
	}
	*key = 0;
}

/*
** Just check if the given "what" is found in a comma separated list.
*/
static int
check(char *what, char *buf)
{
	size_t len;

	len = strlen(what);
	do {
		if ((strncmp(what, buf, len) == 0) &&
		    (buf[len] == ',' || buf[len] == '\n' || buf[len] == 0)) {
			return 1;
		}
		while (*buf && *buf != ',' && *buf != '\n') {
			buf++;
		}
		if (*buf) {
			buf++;
		}
	} while (*buf);

	return 0;
}

/*
** Doit() calls itself recursively for embedded netgroups, and keeps a
** list of the group names it has seen so that we can check for loops.
** The stack usage is pretty large so it is dangerous if netgroups are
** embedded to deeply.
*/
static int
doit(group_t *groups, char *netgroup, char *machine, char *user, char *domain)
{
	char buf[MAXLINE], *p, *q, *r;
	size_t len;
	group_t group, *g;
	int match;

	/*
	** Lookup the netgroup.
	*/
	if (ns_lookup(&_netgroup, 0, "netgroup", netgroup, 0, buf,
	    sizeof(buf)) != NS_SUCCESS) {
		return 0;
	}

	/*
	** Parse the netgroup, recursively calling doit if needed.
	*/
	p = buf;
	while (p) {
		match = 0;
		while (isspace(*p)) {
			*p++;
		}
		if (*p == 0 || *p == '#') {
			break;
		}
		if (*p == '(') {
			p++;
			while (isspace(*p)) {
				p++;
			}
			r = q = index(p, ',');
			if (! q) {
				break;
			}
			if (p == q || ! machine) {
				match++;
			} else {
				while (isspace(q[-1])) {
					q--;
				}
				*q = 0;
				if (strcasecmp(machine, p) == 0) {
					match++;
				}
			}
			p = r + 1;

			while (isspace(*p)) {
				p++;
			}
			r = q = index(p, ',');
			if (! q) {
				break;
			}
			if (p == q || ! user) {
				match++;
			} else {
				while (isspace(q[-1])) {
					q--;
				}
				*q = 0;
				if (strcmp(user, p) == 0) {
					match++;
				}
			}
			p = r + 1;

			while (isspace(*p)) {
				p++;
			}
			r = q = index(p, ')');
			if (! q) {
				break;
			}
			if (p == q || ! domain) {
				match++;
			} else {
				while (isspace(q[-1])) {
					q--;
				}
				*q = 0;
				if (strcasecmp(domain, p) == 0) {
					match++;
				}
			}

			if (match == 3) {
				return 1;
			}
		} else {
			q = strpbrk(p, " \t\n#");
			if (q && *q == '#') {
				break;
			}
			if (q) {
				*q = 0;
			}

			/* check for loop */
			len = strlen(p);
			for (g = groups; g; g = g->next) {
				if ((len == g->len) &&
				    (strcmp(p, g->value) == 0)) {
					match = 1;
					break;
				}
			}

			if (! match) {
				group.len = len;
				group.value = p;
				group.next = groups;
				if (doit(&group, p, machine, user, domain)) {
					return 1;
				}
			}

			if (q) {
				*q = ' ';
			}
		}
		while (*p && !isspace(*p)) {
			p++;
		}
	}

	return 0;
}

/*
** The innetgr() routine walks through the named netgroup looking for
** a matching record.  In the simple case we can just look in the
** reverse maps, but if both user and machine are set then we have
** to completely unroll the group to get an answer.
*/
int
innetgr(char *netgroup, char *machine, char *user, char *domain)
{
	int status, i;
	char key[256], buf[MAXLINE];
	group_t group;

	if (! netgroup) {
		return 0;
	}

	/*
	** First we check the easy maps.
	*/
	if (domain) {
		if (user && !machine) {
			for (i = 0; i < 4; i++) {
				switch (i) {
				    case 0:
					mkkey(user, domain, key, sizeof(key));
					break;
				    case 1:
					mkkey("*", domain, key, sizeof(key));
					break;
				    case 2:
					mkkey(user, "*", key, sizeof(key));
					break;
				    case 3:
					mkkey("*", "*", key, sizeof(key));
					break;
				}
				status = ns_lookup(&_netgroup_byuser, 0,
				    "netgroup.byuser", key, 0, buf,
				    sizeof(buf));
				if ((status == NS_SUCCESS) &&
				    check(netgroup, buf)) {
					return 1;
				}
			}
		} else if (machine && !user) {
			for (i = 0; i < 4; i++) {
				switch (i) {
				    case 0:
					mkkey(machine, domain, key,
					    sizeof(key));
					break;
				    case 1:
					mkkey("*", domain, key, sizeof(key));
					break;
				    case 2:
					mkkey(machine, "*", key, sizeof(key));
					break;
				    case 3:
					mkkey("*", "*", key, sizeof(key));
					break;
				}
				status = ns_lookup(&_netgroup_byhost, 0,
				    "netgroup.byhost", key, 0, buf,
				    sizeof(buf));
				if ((status == NS_SUCCESS) &&
				    check(netgroup, buf)) {
					return 1;
				}
			}
		}
	}

	/*
	** Now the hard part.  We have to recursively parse the entire
	** netgroup.
	*/
	group.len = strlen(netgroup);
	group.value = netgroup;
	group.next = 0;
	return doit(&group, netgroup, machine, user, domain);
}
