/*
 * Copyright 1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Community-based SNMP Authorization
 *
 *	$Revision: 1.2 $
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <syslog.h>
#include <string.h>
#include <bstring.h>
#include <stdio.h>
#include <errno.h>
#include <ctype.h>
#include "oid.h"
#include "asn1.h"
#include "snmp.h"
#include "packet.h"
#include "message.h"
#include "sat.h"
#include "table.h"
#include "subagent.h"
#include "agent.h"
#include "commauth.h"
extern "C" {
#include "exception.h"
int gethostname(char *, int);
}

static char *wildcard = "*";

/*
 * Hosts
 */
commAuthHost::commAuthHost(void)
{
    next = 0;
    addr.s_addr = 0;
}

unsigned int
commAuthHost::set(const char *s)
{
    // Try the wildcard
    if (strcmp(s, wildcard) == 0) {
	addr.s_addr = INADDR_NONE;
	return 1;
    }

    // Try an Internet Address
    addr.s_addr = inet_addr(s);
    if (addr.s_addr == INADDR_NONE) {
	// Try a host name
	struct hostent *he = gethostbyname(s);
	if (he == 0)
	    return 0;
	bcopy(he->h_addr, &addr, sizeof addr);
    }

    // Change localhost into this host's address
    if (addr.s_addr == 0x7f000001) {
	char buf[64];
	if (gethostname(buf, 64) < 0)
	    return 0;
	struct hostent *he = gethostbyname(buf);
	if (he == 0)
	    return 0;
	bcopy(he->h_addr, &addr, sizeof addr);
    }

    return 1;
}

/*
 * Communities
 */
commAuthCommunity::commAuthCommunity(void)
{
    next = 0;
    community = 0;
}

commAuthCommunity::~commAuthCommunity(void)
{
    if (community != 0)
	delete community;
}

unsigned int
commAuthCommunity::set(const char *s)
{
    if (strcmp(s, wildcard) == 0)
	community = 0;
    else {
	length = strlen(s);
	community = new char[length];
	bcopy(s, community, length);
    }
    return 1;
}

/*
 * Operations
 */
const unsigned int opSet = 2;
const unsigned int opGet = 1;

commAuthOperation::commAuthOperation(void)
{
    op = 0;
}

unsigned int
commAuthOperation::set(const char *s)
{
    if (strcasecmp(s, "set") == 0)
	op |= opSet;
    else if (strcasecmp(s, "get") == 0)
	op |= opGet;
    else
	return 0;

    return 1;
}

/*
 * Rules
 */
commAuthRule::commAuthRule(void)
{
    next = 0;
    hosts = 0;
    communities = 0;
}

commAuthRule::~commAuthRule(void)
{
    for (commAuthHost *h; hosts != 0; hosts = h) {
	h = hosts->next;
	delete hosts;
    }
    for (commAuthCommunity *c; communities != 0; communities = c) {
	c = communities->next;
	delete communities;
    }
}

int
commAuthRule::parse(char *s)
{
    char *p, *next;

    // Separate out hosts
    next = strchr(s, ':');
    if (next == 0)
	return 0;
    *next++ = '\0';

    // Parse hosts
    commAuthHost **h;
    for (h = &hosts; (p = strtok(s, ",")) != 0; h = &((*h)->next)) {
	*h = new commAuthHost;
	if (!(*h)->set(p))
	    return 0;
	s = 0;
    }
    if (hosts == 0)
	return 0;

    // Separate out communities
    s = next;
    next = strchr(s, '/');
    if (next != 0)
	*next++ = '\0';

    // Parse communities
    commAuthCommunity **c;
    for (c = &communities; (p = strtok(s, ",")) != 0; c = &((*c)->next)) {
	*c = new commAuthCommunity;
	if (!(*c)->set(p))
	    return 0;
	s = 0;
    }
    if (communities == 0)
	return 0;

    // Parse operations
    s = next;
    while ((p = strtok(s, ",")) != 0) {
	if (!operations.set(p))
	    return 0;
	s = 0;
    }

    return 1;
}

int
commAuthRule::match(struct in_addr *a, asnOctetString *o, int pdutype)
{
    int total = 0;
    int points = 0;
    const int wild = 0x1;
    const int exactHost = 0x10;
    const int exactCommunity = 0x100;
    const int exactOperation = 0x1000;

    // Match operation first because it's fast
    if (operations.op != 0) {
	switch (pdutype) {
	    case SNMP_GetRequest:
	    case SNMP_GetNextRequest:
		if (operations.op & opGet)
		    points = exactOperation;
		break;
	    case SNMP_SetRequest:
		if (operations.op & opSet)
		    points = exactOperation;
		break;
	}
	if (points == 0)
	    return 0;
	total += points;
	points = 0;
    }

    // Match host
    for (commAuthHost *h = hosts; h != 0; h = h->next) {
	if (h->addr.s_addr == INADDR_NONE)
	    points = wild;
	else if (h->addr.s_addr == a->s_addr) {
	    points = exactHost;
	    break;
	}
    }
    if (points == 0)
	return 0;
    total += points;
    points = 0;

    // Match community
    char *c = o->getValue();
    unsigned int l = o->getLength();
    for (commAuthCommunity *co = communities; co != 0; co = co->next) {
	if (co->community == 0)
	    points = wild;
	else if (co->length == l && bcmp(co->community, c, l) == 0) {
	    points = exactCommunity;
	    break;
	}
    }
    if (points == 0)
	return 0;

    return total + points;
}

/*
 * Authorization
 */
communityAuth::communityAuth(const char *f)
{
    // Clear lists
    accept = reject = 0;

    // Store the file
    int l = strlen(f);
    file = new char[l + 1];
    strcpy(file, f);

    // Stat the file
    struct stat st;
    if (stat(file, &st) < 0) {
	st.st_ctime = 0;
	exc_errlog(LOG_WARNING, errno, 
	    "communityAuth: unable to stat %s", file);
    }
    readTime = st.st_ctime;

    // Read the file
    readFile();
}

communityAuth::~communityAuth(void)
{
    clearRules();
    delete file;
}

void
communityAuth::clearRules(void)
{
    struct commAuthRule *ar, *next;

    for (ar = accept; ar != 0; ar = next) {
	next = ar->next;
	delete ar;
    }
    for (ar = reject; ar != 0; ar = next) {
	next = ar->next;
	delete ar;
    }

    accept = reject = 0;
}

void
communityAuth::readFile(void)
{
    FILE *fp = fopen(file, "r");
    if (fp == 0) {
	exc_errlog(LOG_ERR, errno,
	   "communityAuth: unable to open authorization file %s", file);
	return;
    }

    int line = 0;
    char buf[256], tmpbuf[256], *s, *next;
    commAuthRule **acceptp = &accept;
    commAuthRule **rejectp = &reject;
    commAuthRule ***p, *ar;

    while ((s = fgets(buf, sizeof buf, fp)) != 0) {
	// Increment line number
	line++;

	// Get the action
	s = getword(s, &next);
	if (s == 0 || *s == '#')
	    continue;

	// Parse the action
	if (strcasecmp(s, "accept") == 0)
	    p = &acceptp;
	else if (strcasecmp(s, "reject") == 0)
	    p = &rejectp;
	else {
	    exc_errlog(LOG_ERR, 0, "%s: line %d: bad action \"%s\"",
	    			    file, line, s);
	    // fclose(fp);
	    // return;
	    continue;   /* go to next line (while loop) --vaz */
	}

	// Now read any number of rules
	for (s = next; s != 0; s = next) {
	    s = getword(s, &next);
	    if (s == 0)
		break;

	    strcpy(tmpbuf, s);
	    ar = new commAuthRule;
	    if (!ar->parse(tmpbuf)) {
		delete ar;
		exc_errlog(LOG_ERR, 0, "%s: line %d: bad entry \"%s\"",
					file, line, s);
		// fclose(fp);
		// return;
		continue;    // go to next rule (for loop) --vaz
	    }

	    // Link new entry into appropriate list
	    **p = ar;
	    *p = &ar->next;
	}
    }

    exc_errlog(LOG_DEBUG, 0, "%s: read %d line%s", file, line,
						   line == 1 ? "" : "s");
    fclose(fp);
}

int
communityAuth::authorize(struct in_addr *a, asnOctetString *o, int tag)
{
    struct stat st;
    struct commAuthRule *ar;
    int acc, rej, score;

    // See if the authorization file changed
    if (stat(file, &st) < 0) {
	st.st_ctime = 0;
	exc_errlog(LOG_WARNING, errno, "unable to stat %s", file);
    }
    if (readTime < st.st_ctime) {
	clearRules();
	readTime = st.st_ctime;
	readFile();
    }

    // Tabulate scores for accept and reject rules
    for (acc = 0, ar = accept; ar != 0; ar = ar->next) {
	score = ar->match(a, o, tag);
	acc = MAX(acc, score);
    }
    for (rej = 0, ar = reject; ar != 0; ar = ar->next) {
	score = ar->match(a, o, tag);
	rej = MAX(rej, score);
    }

    return acc - rej;
}

char *
communityAuth::getword(char *s, char** nextp)
{
    while (isspace(*s))
	s++;
    if (*s == '\0')
	return 0;

    *nextp = strpbrk(s, " \t\n");
    if (*nextp != NULL)
	*(*nextp)++ = '\0';

    return s;
}
