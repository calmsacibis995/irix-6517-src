/**************************************************************************
 *                                                                        *
 *       Copyright (C) 1997, Silicon Graphics, Inc.                       *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/*
 * Client access control support.
 */
#include "rtmond.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

const char* access_file	= _CONFIG_ACCESS_FILENAME;	/* control file */
const char* global_access = _CONFIG_GLOBAL_ACCESS;	/* global spec */

static int
nextRecord(FILE* db, char line[], u_int size)
{
    while (fgets(line, size-1, db)) {
	char* cp = strchr(line, '#');
	if (cp) {			/* trim trailing white space */
	    for (cp = strchr(line, '\0'); cp > line; cp--)
		if (!isspace(cp[-1]))
		    break;
	    *cp = '\0';
	}
	if (cp = strchr(line, '\n'))
	    *cp = '\0';
	if (line[0] != '\0')
	    return (TRUE);
    }
    return (FALSE);
}

#include <regex.h>

/*
 * Check the host address and (optionally) name
 * against the supplied access control specification.
 * If the host is permitted access, the mask of
 * event classes that it can retrieve is returned.
 */
static int
check_access_entry(char* spec, const char* addr, const char* hostname, uint64_t* pmask)
{
    regex_t re;
    char* cp;
    char sep;
    /*
     * Records are of the form:
     *
     *    regex[:event-mask]
     *
     * where regex is a regular expression that must
     * match the host name/address string (where an
     * address is the dot-notation form of the client
     * host).  If an event mask specification follows,
     * it restricts which events clients on that host
     * can receive.
     */
    for (cp = spec; *cp && *cp != ':'; cp++)
	;
    sep = *cp, *cp = '\0';
    if (regcomp(&re, spec, REG_EXTENDED|REG_ICASE|REG_NOSUB) == 0) {
	int status = (hostname ? regexec(&re, hostname, 0, NULL, 0) : -1);
	if (status != 0)
	    status = regexec(&re, addr, 0, NULL, 0);
	regfree(&re);
	if (status == 0) {
	    IFTRACE(ACCESS)(NULL,
	"Access control match; addr: %s host: %s regex: %s event mask: %s",
		addr, hostname ? hostname : "<null>", spec,
		sep == ':' ? cp+1 : "any");
	    if (sep == ':')
		*pmask = parse_event_str(cp+1);
	    else
		*pmask = RTMON_ALL;
	    return (TRUE);
	}
    } else
	IFTRACE(ACCESS)(NULL, "Regex parse error on \"%s\"", spec);
    return (FALSE);
}

/*
 * Check the host name against the list of hosts
 * that are permitted to use the server and return
 * the mask of event classes that are permitted.
 */
static uint64_t
check_access_file(FILE* db, const char* addr, const char* hostname)
{
    char line[1024];

#ifdef PARANOID
    if (sb.st_mode&077) {	/* file must not be publicly readable */
	Log(LOG_NOTICE, "Access control file not mode 600; access denied.");
	return (0);
    }
#endif
    rewind(db);
    while (nextRecord(db, line, sizeof (line))) {
	uint64_t mask;
	if (check_access_entry(line, addr, hostname, &mask))
	    return (mask);
    }
    return (0);
}

/*
 * Check client access.  If an access control file
 * exists; then the client must be listed to have
 * access to the server (and be assigned a non-null
 * mask of events that it can register interest in).
 * If a global access specification was given on the
 * command line, then it overrides any access control
 * file.  The latter makes it easy to restrict server
 * access to local clients by specifying "-a localhost"
 * when the server is started up.
 */
uint64_t
check_access(const char* hostname, struct in_addr in)
{
    uint64_t mask;
    const char* addr = inet_ntoa(in);

    /* map dot-address to hosts's official name */
    IFTRACE(ACCESS)(NULL, "Check access for host %s addr %s"
	, hostname ? hostname : "<no-host-name>"
	, addr
    );
    if (!global_access) {	/* use access control file, if any */
	FILE* db = fopen(access_file, "r");
	if (db != NULL) {
	    mask = check_access_file(db, addr, hostname);
	    fclose(db);
	} else {
	    IFTRACE(ACCESS)(NULL, "Unable to open the host access file %s: %s",
		access_file, strerror(errno));
	    mask = RTMON_ALL;	/* NB: default to everyone has access */
	}
    } else {			/* command-line specification */
	char spec[1024];
	strcpy(spec, global_access);
	mask = 0;		/* must match to have access */
	(void) check_access_entry(spec, addr, hostname, &mask);
    }
    return (mask);
}
