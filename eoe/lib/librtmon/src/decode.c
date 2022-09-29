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

#include <sys/rtmon.h>
#include <sys/param.h>
#include <sys/procfs.h>
#include <stdlib.h>
#include <assert.h>

#include "rtmon.h"

/*
 * Decode RTMON events.
 */
#define	TRUE	1
#define	FALSE	0

/*
 * Process names are at most PRCOMSIZ long.
 * Typically this is 32 bytes.
 */
#define	NAMELEN	((PRCOMSIZ+1)&~1)

typedef	struct kidentry kidentry_t;

struct kidentry {
    kidentry_t* next;		/* linked list of entries */
    int		flags;
#define	PID_TRACED	0x0001	/* trace process events */
    int64_t	kid;		/* kthread id */
    pid_t	pid;		/* PID */
    char	name[NAMELEN];	/* process tag/name */
    void*	data;		/* ancillary data */
    void	(*cleanup)(void*);
};

#define	KIDCACHESIZE	4093	/* prime; close to 4K */
#define	HASH(kid)	(((uint64_t)((kid) + 32768)) % KIDCACHESIZE)

#define MAX_UNKNOWN_TRACED_PIDS 20
int traced_pids[MAX_UNKNOWN_TRACED_PIDS];
int num_traced_pids;

static void
kid_init(rtmonDecodeState* rs)
{
    rs->kidcache = (kidentry_t**) malloc(KIDCACHESIZE * sizeof (kidentry_t*));
    memset(rs->kidcache, 0, KIDCACHESIZE * sizeof (kidentry_t*));
    rs->lastkid = NULL;
}

static void
kid_cleanup(rtmonDecodeState* rs)
{
    int i;
    for (i = 0; i < KIDCACHESIZE; i++) {
	kidentry_t* ke;
	while ((ke = rs->kidcache[i]) != NULL) {
	    rs->kidcache[i] = NULL;
	    if (ke->data && ke->cleanup)
		(*ke->cleanup)(ke->data);
	    free(ke);
	}
    }
    rs->lastkid = NULL;
}

/*
 * Purge any cached entry for the specified KID.
 */
static void
kid_purge(rtmonDecodeState* rs, int64_t kid)
{
    kidentry_t* ke;
    kidentry_t** kep;

    for (kep = &rs->kidcache[HASH(kid)]; ke = *kep; kep = &ke->next)
	if (ke->kid == kid) {
	    *kep = ke->next;
	    if (ke->data && ke->cleanup)
		(*ke->cleanup)(ke->data);
	    if (rs->lastkid == ke)
		rs->lastkid = NULL;
	    free(ke);
	    break;
	}
}

static kidentry_t*
kid_create(int64_t kid, pid_t pid, const char* name)
{
    kidentry_t* ke = (kidentry_t*) malloc(sizeof (*ke));
    if (ke) {
	ke->flags = 0;
	ke->kid = kid;
	ke->pid = pid;
	strncpy(ke->name, name, NAMELEN);
	ke->data = NULL;
	ke->cleanup = NULL;
    }
    return (ke);
}

static void
kid_enter(rtmonDecodeState* rs, int64_t kid, pid_t pid, const char* name)
{
    int h = HASH(kid);
    kidentry_t* ke;

    for (ke = rs->kidcache[h]; ke; ke = ke->next) {
	    if (ke->kid == kid) {
		    if ((int)pid > 0)
			    ke->pid = pid;
		    if (ke->name[0] == '\0' || (strncmp(ke->name, name, strlen(name)) != 0)) {
			    strncpy(ke->name, name, NAMELEN);
		    }
		    return;
	    }
    }
    /* Did not find the kid, create a new one */

    ke = kid_create(kid, pid, name);
    if (ke) {
	    ke->next = rs->kidcache[h];		/* NB: place at front */
	    rs->kidcache[h] = ke;
    }
}

static void
kid_rename(rtmonDecodeState* rs, int64_t kid, const char* name)
{
    int h = HASH(kid);
    kidentry_t* ke;
    for (ke = rs->kidcache[h]; ke; ke = ke->next)
	if (ke->kid == kid) {
	    strncpy(ke->name, name, NAMELEN);
	    return;
	}
    /* We need to set pid to -1 as we have no idea what the pid
     * is at this point
     */
    ke = kid_create(kid, -1, name);
    if (ke) {
	ke->next = rs->kidcache[h];		/* NB: place at front */
	rs->kidcache[h] = ke;
    }
}

static kidentry_t*
kid_lookup(rtmonDecodeState* rs, int64_t kid)
{
    kidentry_t* ke = rs->lastkid;

    if (!ke || (ke->kid != kid)) {
	int h = HASH(kid);
	for (ke = rs->kidcache[h]; ke && ke->kid != kid; ke = ke->next)
	    ;
    }
    /*    if (ke)fprintf(stderr,"Found kid %lld for kid %lld with name %s \n",kid, ke->kid, ke->name); */
    if (ke)
	    rs->lastkid = ke;
    return (ke);
}
static void
kid_dup(rtmonDecodeState* rs, int64_t pkid, int64_t ckid)
{
	kidentry_t* ke = kid_lookup(rs, pkid);
	if (ke)
		kid_enter(rs, ckid, -1, ke->name);
}


void*
_rtmon_kid_getdata(rtmonDecodeState* rs, int64_t kid)
{
    kidentry_t* ke = kid_lookup(rs, kid);
    return (ke ? ke->data : NULL);
}

void
_rtmon_kid_setdata(rtmonDecodeState* rs, int64_t kid, void* data, void (*clean)(void*))
{
    int h = HASH(kid);
    kidentry_t* ke;

    for (ke = rs->kidcache[h]; ke; ke = ke->next)
	if (ke->kid == kid) {
	    if (ke->data && ke->cleanup)
		(*ke->cleanup)(ke->data);
	    ke->data = data;
	    ke->cleanup = clean;
	    return;
	}
    ke = kid_create(kid, -1, "");
    if (ke) {
	ke->data = data;
	ke->cleanup = clean;
	ke->next = rs->kidcache[h];		/* NB: place at front */
	rs->kidcache[h] = ke;
    }
}

void
_rtmon_kid_clrdata(rtmonDecodeState* rs, int64_t kid)
{
    kidentry_t* ke = kid_lookup(rs, kid);
    if (ke && ke->data && ke->cleanup) {
	(*ke->cleanup)(ke->data);
	ke->data = NULL;
	ke->cleanup = NULL;
    }
}

void
_rtmon_pid_trace(rtmonDecodeState* rs, pid_t pid)
{
	int i;
	kidentry_t	*ke;
	kidentry_t	**kep;
	int found = 0;
	
	for (i = 0; i < KIDCACHESIZE; i++) {
		kep = &rs->kidcache[i];
		for (kep = &rs->kidcache[i]; ke = *kep; kep = &ke->next) {
			if (ke->pid == pid) {
				ke->flags |= PID_TRACED;
				found = 1;
			}
		}
	}
	if (!found)
#ifdef DEBUG		
		fprintf(stderr, "pid %d not found\n",pid);
#endif
		if (num_traced_pids < MAX_UNKNOWN_TRACED_PIDS)
			traced_pids[num_traced_pids++] = pid;
		else
			fprintf(stderr, "to many pids being traced. Try tracing whole system\n");
}

int
_rtmon_pid_istraced(rtmonDecodeState* rs, pid_t pid)
{
	int i;
	kidentry_t	*ke;
	kidentry_t	**kep;
	int found = 0;
	
	for (i = 0; i < KIDCACHESIZE; i++) {
		kep = &rs->kidcache[i];
		for (kep = &rs->kidcache[i]; ke = *kep; kep = &ke->next) {
			if (ke->pid == pid) {
				found = 1;
				if  ((ke->flags & PID_TRACED) != 0)
				    return 1;
			}
		}
	}
	for (i = 0; i < num_traced_pids; i++)
		if (traced_pids[i] == pid)
			return(1);
	return 0;
}

void
_rtmon_kid_trace(rtmonDecodeState* rs, int64_t kid)
{

    kidentry_t* ke = kid_lookup(rs, kid);
    if (!ke) {
	    int h = HASH(kid);
	    ke = kid_create(kid, -1, "");
	    ke->next = rs->kidcache[h];
	    rs->kidcache[h] = ke;
    }
    if (ke)
	ke->flags |= PID_TRACED;
}

void
_rtmon_pid_untrace(rtmonDecodeState* rs, pid_t pid)
{
	int i;
	kidentry_t	*ke;
	kidentry_t	**kep;
	
	for (i = 0; i < KIDCACHESIZE; i++) {
		kep = &rs->kidcache[i];
		for (kep = &rs->kidcache[i]; ke = *kep; kep = &ke->next) {
			if (ke->pid == pid) {
				ke->flags &= ~PID_TRACED;	
			}
		}
	}
}

int
_rtmon_kid_istraced(rtmonDecodeState* rs, int64_t kid)
{
    kidentry_t* ke = kid_lookup(rs, kid);

    if (ke) {
	    if (ke->flags & PID_TRACED)
		    return 1;
	    else {
#ifdef DEBUG
		    fprintf(stderr, "Looking for pid %d \n",ke->pid);
#endif
		    if (ke->pid > 0) {
			    int i;
			    for (i = 0; i < num_traced_pids; i++)
				    if (traced_pids[i] == ke->pid)
					    return(1);	    
		    } else {
			    /* For unknown pids we assume that we are
			     * not tracing them.
			     */
			    return 0;
		    }
	    }
	    return 0;
    }
    return 1;
}

/*
 * Lookup the skecified PID in the cache.
 */
const char*
rtmon_kidLookup(rtmonDecodeState* rs, int64_t kid)
{
    kidentry_t* ke = kid_lookup(rs, kid);
    if (ke) {
	rs->lastkid = ke;			/* set cached PID */
	/*	fprintf(stderr,"Found kid %lld %s\n",kid, ke->name); */
	return (ke->name);
    } else {
	static char name[NAMELEN];
	sprintf(name, "kthread%lld" , kid);
	return (name);
    }
}
pid_t
rtmon_pidLookup(rtmonDecodeState* rs, int64_t kid)
{

    kidentry_t* ke = kid_lookup(rs, kid);
    if (ke) {
	    rs->lastkid = ke;			/* set cached PID */
	    return (ke->pid);
    }
    return (-1);
}

rtmonDecodeState*
rtmon_decodeBegin(rtmonDecodeState* rs)
{
    if (rs == NULL) {
	rs = (rtmonDecodeState*) malloc(sizeof (*rs));
	rs->mystorage = TRUE;
    } else
	rs->mystorage = FALSE;
    rs->starttime = 0;
    rs->lasttime = 0;
    rs->tstampadj = 0;
    rs->tstampfreq = 1;
    rs->tstampmulf = 1000.;
    rs->kabi = 0;
    rs->ncpus = 0;

    kid_init(rs);

    return (rs);
}

void
rtmon_decodeEnd(rtmonDecodeState* rs)
{
    kid_cleanup(rs);
    if (rs->mystorage)
	free(rs);
}

static void
decodeConfigEvent(rtmonDecodeState* rs, const tstamp_event_entry_t* ev)
{
    const tstamp_config_event_t* config = (const tstamp_config_event_t*) ev;
    if (config->tstampfreq != 0) {
	rs->tstampfreq = config->tstampfreq;
	rs->tstampmulf = 1000. / (float) config->tstampfreq;		/* ms */
	if (rs->tstampmulf == 0)
	    rs->tstampmulf = 1;
    }
    if (rs->kabi && rs->kabi != config->kabi)
	fprintf(stderr, "Warning, different CPU's have different kernel ABIs; system call decoding may not work right\n");
    rs->kabi = config->kabi;
    if (config->cpu >= rs->ncpus)
	rs->ncpus = config->cpu+1;
}

void
rtmon_decodeEventBegin(rtmonDecodeState* rs, const tstamp_event_entry_t* ev)
{
    switch (ev->evt) {
    case TSTAMP_EV_CONFIG:
	decodeConfigEvent(rs, ev);
	break;
    case TSTAMP_EV_TASKNAME:
#if 0
	fprintf(stderr,"name event for kid %lld pid %d name %s\n",
		((const tstamp_taskname_event_t*) ev)->k_id,
		((const tstamp_taskname_event_t*) ev)->pid,
		((const tstamp_taskname_event_t*) ev)->name);
#endif
	kid_enter(rs,
	    ((const tstamp_taskname_event_t*) ev)->k_id,
	    ((const tstamp_taskname_event_t*) ev)->pid,
	    ((const tstamp_taskname_event_t*) ev)->name);
	return;				/* tstamp is not meaningful */
	/*
	 * We need to pick up kid->pid mappings from syscalls and switch
	 * events.
	 */
    case TSTAMP_EV_SYSCALL_BEGIN:
    case TSTAMP_EV_SYSCALL_END:{
	    const tstamp_event_syscall_data_t* sdp =
		    (const tstamp_event_syscall_data_t*) &ev->qual[0];
	    kid_enter(rs, sdp->k_id, sdp->pid, "");
	    break;
    }
    case TSTAMP_EV_EOSWITCH: 
	    kid_enter(rs, ev->qual[0], ev->qual[2], "");
	    break;
    case TSTAMP_EV_SIGRECV:
	    kid_enter(rs, ev->qual[0], ev->qual[3], "");
	    break;
    case TSTAMP_EV_EXEC: {
	    const tstamp_event_exec_data_t* edp =
		    (const tstamp_event_exec_data_t*) &ev->qual[0];
	    /*
	     * Here we just want to do the kid<->pid mapping.
	     * we do the rename after the syscall
	     */
	    kid_enter(rs, edp->k_id, edp->pid, "");
	break;
    }
	    

    }

    if (rs->starttime == 0)
	rs->starttime = rs->lasttime = ev->tstamp;
    else if (ev->tstamp < rs->lasttime)
	/*
	 * Time values wrapped, bump the adjustment
	 * factor use to calculate a 64-bit time.
	 * Note that this assumes that events are
	 * ordered by time.
	 */
	rs->tstampadj += 0xffffffffULL;
}

void
rtmon_decodeEventEnd(rtmonDecodeState* rs, const tstamp_event_entry_t* ev)
{
    switch (ev->evt) {
    case TSTAMP_EV_EXIT:
	    kid_purge(rs, (int64_t) ev->qual[0]); 
	break;
    case TSTAMP_EV_EXEC: {
	const tstamp_event_exec_data_t* edp =
	    (const tstamp_event_exec_data_t*) &ev->qual[0];
	kid_rename(rs, edp->k_id, edp->name);
	break;
        }
    case TSTAMP_EV_FORK: {
	const tstamp_event_fork_data_t* fdp =
	    (const tstamp_event_fork_data_t*) &ev->qual[0];
	kid_rename(rs, (int64_t) fdp->pkid, fdp->name);
	kid_dup(rs, (int64_t) fdp->pkid, (int64_t) fdp->ckid);
	break;
        }
    }
    rs->lasttime = ev->tstamp;
}
