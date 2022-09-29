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
 * KID -> process name handling.  When a KID is referenced in an
 * event the server is obligated to send a "task name" event ahead
 * that specifies the "name" of the associated process.  This
 * operation happens a lot so we keep a per-client cache of which
 * PIDs have been processed (i.e. the client sent a task name
 * mapping event) and also keep a per-thread cache of KID->name
 * mappings (the latter to avoid having each client query the OS
 * to find out the process name).  Keeping state on a per-client
 * basis also simplifies cleanup when a client goes away; the
 * state is just reclaimed as part of the client exit procedure.
 */
#include "rtmond.h"

#include <sys/syssgi.h>
#include <fcntl.h>
#include <paths.h>
#include <sys/procfs.h>

/*
 * XXX collect stats on how well the hashing/cacheing works.
 */

/*
 * Process names are at most PRCOMSIZ long.
 * Typically this is 32 bytes.
 */
#define	NAMELEN	((PRCOMSIZ+1)&~1)

struct kidentry {
    kidentry_t* next;		/* linked list of entries */
    int64_t	kid;		/* kthread id */
    pid_t	pid;		/* PID */
    char	name[NAMELEN];	/* process tag/name */
};

#define	KIDCACHESIZE	4093	/* prime; close to 4K */
#define	HASH(kid)	(((kid) + 32768) % KIDCACHESIZE)

#define	NKIDS	(sizeof (((kidblock_t*)0)->kids) / sizeof (int64_t))

void
kid_init(daemon_info_t* dp)
{
    dp->kidcache = (kidentry_t**) malloc(KIDCACHESIZE * sizeof (kidentry_t*));
    memset(dp->kidcache, 0, KIDCACHESIZE * sizeof (kidentry_t*));
    dp->lastkid = NULL;
}

void
kid_flush(daemon_info_t* dp)
{
    kidentry_t* ke;
    int i;

    IFTRACE(KID)(dp, "Flush kid cache");
    for (i = 0; i < KIDCACHESIZE; i++)
	while (ke = dp->kidcache[i]) {
	    dp->kidcache[i] = ke->next;
	    IFTRACE(KID)(dp, "Flush KID entry: %lld/%s",
		ke->kid, ke->name);
	    free(ke);
	}
    dp->lastkid = NULL;
}

/*
 * Purge any cached entry for the specified KID.
 */
void
kid_purge_kid(daemon_info_t* dp, int64_t kid)
{
    kidentry_t* ke;
    kidentry_t** kep;

    IFTRACE(KID)(dp, "Purge state for KID %lld", kid);
    for (kep = &dp->kidcache[HASH(kid)]; ke = *kep; kep = &ke->next)
	if (ke->kid == kid) {
	    client_t* cp;
	    *kep = ke->next;
	    free(ke);
	    /* purge client references to kid */
	     for (cp = (client_t*) dp->clients; cp; cp = cp->next) {
		kidblock_t* pb;
		for (pb = &cp->kids; pb; pb = pb->next) {
		    int i;
		    for (i = 0; i < NKIDS; i++)
			if (pb->kids[i] == kid) {
			    IFTRACE(KID)(dp, "Purge KID %lld for client %s:%u",
				kid, cp->host, cp->port);
			    pb->kids[i] = 0;
			    if (cp->lastkid == kid)
				cp->lastkid = (int64_t) -1;
			    goto next;
			}
		}
	next:
		;
	    }
	    break;
	}
}

/*
 * Check whether the specified KID (encountered in the event
 * stream) has been seen before.  If this is the first time
 * any client has seen it, send a "task name" protocol message
 * to the client and record the KID in the client's state
 * block so future events won't cause another message to be
 * transmitted.
 */
void
kid_check_kid(daemon_info_t* dp, int64_t kid, int64_t tv)
{
    client_t* cp = (client_t*) dp->clients;

#ifdef notdef
    IFTRACE(KID)(dp, "Check kid %lld", kid);
#endif
    while (cp) {
	if (cp->lastkid != kid) {
	    kidblock_t* pb;
	    int64_t* fp = NULL;

	    for (pb = &cp->kids; pb; pb = pb->next) {
		/* XXX eliminate linear search */
		int i;
		for (i = 0; i < NKIDS; i++) {
		    if (pb->kids[i] == kid)
			goto hit;
		    if (pb->kids[i] == 0)
			fp = &pb->kids[i];
		}
	    }
	    /*
	     * We could check the return value here and blow away
	     * the client immediately, but since we know we're called
	     * as a result of processing an event, it's likely that
	     * event will get dispatched to the client and the subsequent
	     * write will cause the client to be collected in the
	     * the normal/main tstamp processing loop.
	     */
	    IFTRACE(KID)(dp, "Send %s:%u task name", cp->host, cp->port);
	    (*cp->proto->writeTaskName)(dp, cp, kid, tv);
	    /* record KID in client's state */
	    if (!fp) {
		/*
		 * No space; add another block.
		 */
		IFTRACE(KID)(dp, "New kid cache block for client %s:%u",
		    cp->host, cp->port);
		pb = (kidblock_t*) malloc(sizeof (*pb));
		assert(pb != NULL);			/* XXX */
		memset(pb, 0, sizeof (*pb));
		pb->next = cp->kids.next;
		cp->kids.next = pb;
		fp = &pb->kids[0];
	    }
	    *fp = kid;
    hit:
	    cp->lastkid = kid;
	}
	cp = cp->next;
    }
}

static void
kid_name(int64_t kid, char* name)
{
    sprintf(name, "kthread%lld" , kid);
}

/*
 * Map a KID to a process/command name.  If the KID is
 * associated with a kernel thread then we ask the OS
 * for a name.  Otherwise we fetch the information by
 * going through /dev/proc (ouch).
 */
static void
kid_get_command_name(daemon_info_t *dp, int64_t kid, pid_t *pid,
		     char *name)
{
	int len = NAMELEN;
	*pid = syssgi(SGI_GET_CONTEXT_NAME, &kid, name, &len);
	if (strlen(name) > len || len == 0)
		name[len] = '\0';
	IFTRACE(KID)(dp, "Kid %lld got pid %d and name %s len %d", kid, *pid, name, len);
	if (*pid == -1) {
		IFTRACE(KID)(dp, "Kid %lld not found errno is %d", kid, errno);
		kid_name(kid, name);
	}
}

/*
 * Lookup the specified KID.  We check the cache
 * first.  If that fails we lookup the entry and
 * try to record the info in the cache.  We keep
 * a pointer to the last lookup as an optimization
 * for multiple clients interested in the same
 * processes and because processes run for a time
 * quantum it's likely per-process events will
 * arrive clustered.
 */
void
kid_lookup_kid(daemon_info_t* dp, int64_t kid, pid_t *pid,
	       char* buf, size_t *len)
{
    kidentry_t* ke = dp->lastkid;
    IFTRACE(KID)(dp, "Lookup kid %lld", kid);
    if (!ke || ke->kid != kid) {
	int h = HASH(kid);
	for (ke = dp->kidcache[h]; ke && ke->kid != kid; ke = ke->next)
	    ;
	if (!ke) {				/* not in cache */
	    ke = (kidentry_t*) malloc(sizeof (*ke));
	    IFTRACE(KID)(dp, "kid %lld is new entry", kid);
	    if (!ke) {
		/* no space for new entry, return info directly */
		IFTRACE(KID)(dp, "No space to cache entry for %lld", kid);
		kid_get_command_name(dp, kid, pid, buf);
		return;
	    }
	    ke->kid = kid;
	    kid_get_command_name(dp, kid, &ke->pid, ke->name);
	    ke->next = dp->kidcache[h];		/* NB: place at front */
	    dp->kidcache[h] = ke;
	    IFTRACE(KID)(dp,
		"New kid mapping: %lld(pid-%d) -> %s", ke->kid, ke->pid, ke->name);
	}
	dp->lastkid = ke;			/* set cached KID */
    }
    strncpy(buf, ke->name, NAMELEN);
    *pid = ke->pid;
    IFTRACE(KID)(dp, "Kid mapping: %lld(%d) -> %s", ke->kid, ke->pid, ke->name);
}

/*
 * Rename the specified KID.  If no entry existed, then
 * create one.  This interface is used to process kernel
 * events that inform us when a process does an exec.
 */
void
kid_rename_kid(daemon_info_t* dp, int64_t kid, const char* name, int64_t tv, pid_t pid)
{
    kidentry_t* ke = dp->lastkid;
    int h;

    IFTRACE(KID)(dp, "Rename kid %lld(pid-%d) to %s", kid, pid, name);
    if (!ke || ke->kid != kid) {		/* check cache */
	h = HASH(kid);
	for (ke = dp->kidcache[h]; ke && ke->kid != kid; ke = ke->next)
	    ;
    }
    if (!ke) {					/* not found, make new entry */
	pid_t pid2;
	char name[NAMELEN];
	int len = NAMELEN;
	pid2 =  syssgi(SGI_GET_CONTEXT_NAME, &kid, name, &len);
	if (strlen(name) > len || len == 0)
		name[len] = '\0';
	ke = (kidentry_t*) malloc(sizeof (*ke));
	if (!ke) {
	    IFTRACE(KID)(dp, "No space to add entry for %lld", kid);
	    return;
	}
	strncpy(ke->name, name, NAMELEN);
	ke->kid = kid;
	ke->pid = pid2;
	ke->next = dp->kidcache[h];		/* NB: place at front */
	dp->kidcache[h] = ke;
    }
    if ((int)pid > 0)
	    ke->pid = pid;
    /*
     * If the name has chaned then put the new name here.
     * We do not copy if the beginning is the same as
     * fork is not able to pass up the full name and
     * we do not want to lose information.
     */
    if (strncmp(ke->name, name, strlen(name))) {
	    client_t* cp = (client_t*) dp->clients;

	    strncpy(ke->name, name, NAMELEN);
	    while (cp) {
		    IFTRACE(KID)(dp, "Send %s:%u task name", cp->host, cp->port);
		    (*cp->proto->writeTaskName)(dp, cp, kid, tv);
		    cp = cp->next;
	    }
    }
    dp->lastkid = ke;				/* set cached KID */
    IFTRACE(KID)(dp, "New kid mapping: %lld(pid-%d) -> %s", ke->kid,ke->pid, name);
}
/*
 * process fork events
 */
void
kid_dup_kid(daemon_info_t* dp, int64_t pkid, int64_t ckid)
{
    kidentry_t* ke = dp->lastkid;
    int h;

    /*
     * Make sure the clients have the kid->pid mappings
     */
    IFTRACE(KID)(dp, "Dup kid; parent %lld, child %lld", pkid, ckid);
    if (!ke || ke->kid != pkid) {
	h = HASH(pkid);
	for (ke = dp->kidcache[h]; ke && ke->kid != pkid; ke = ke->next)
	    ;
    }
    if (ke) {					/* parent located, add child */
	kidentry_t* cke = (kidentry_t*) malloc(sizeof (*cke));
	if (!cke) {
	    IFTRACE(KID)(dp, "No space to add entry for %lld", ckid);
	    return;
	}
	cke->kid = ckid;
	strcpy(cke->name, ke->name);
	h = HASH(ckid);
	cke->next = dp->kidcache[h];		/* NB: place at front */
	dp->kidcache[h] = cke;
	IFTRACE(KID)(dp,
	    "New kid mapping: %lld -> %s", cke->kid, cke->name);
	dp->lastkid = cke;			/* set cached KID */
    } else
	IFTRACE(KID)(dp, "No entry for parent; child entry not made");
}
