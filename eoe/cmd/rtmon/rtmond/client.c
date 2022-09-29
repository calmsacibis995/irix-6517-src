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
 * Client support.
 */
#include "rtmond.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/par.h>
#include <netdb.h>

#define	NZ(x)	((x) ? (x) : 1)

static	sem_t comlock;			/* sema for global client list */
static	client_common_t* master = NULL;	/* master list of clients */
static	int maxindbytes = 0;		/* max over all active clients */

static	void delete_client(daemon_info_t* dp, client_t* cp);

void
init_client(void)
{
    if (sem_init(&comlock, 0, 1) != 0)
	Fatal(NULL, "Cannot initialize client list semaphore: %s",
	    strerror(errno));
}

/*
 * Add a new client.
 *
 * NB: clients_sem is assumed to be locked by the caller.
 */
static client_t*
new_client(daemon_info_t* dp, client_common_t* com)
{
    client_t* cp;

    IFTRACE(CLIENT)(dp,
	"New client at %s:%u, events %#llx, talking %s cookie %#llx%s",
	com->host, com->port, com->events, com->proto->name,
	com->cookie, !dp->isrunning ? " (start thread)" : "");

    cp = (client_t*) malloc(sizeof (*cp));
    if (cp != NULL) {
	cp->next = NULL;
	com->refs++;
	cp->com = com;
	cp->cookie = com->cookie;
	cp->events = 0;				/* NB: set when started */
	cp->proto = com->proto;
	cp->host = com->host;
	cp->port = com->port;
	cp->lastevt = 0;
	cp->lastkid = (int64_t) -1;
	memset(&cp->kids, 0, sizeof (cp->kids));
	cp->kevents = 0;
	cp->kdrops = 0;
	(*com->proto->initClient)(dp, cp);	/* do protocol-specific stuff */
	if (io_client_init(dp, cp)) {		/* setup async write support */
	    cp->next = (client_t*) dp->clients;
	    dp->clients = cp;

	    if (!dp->isrunning)			/* event collection thread */
		startMerge(dp);
	    else {
		int v;
		sem_getvalue(&dp->clients_sem, &v);
		IFTRACE(THREAD)(dp,
		    "Thread already running, clients_sem %d, clients %#x",
		    v, dp->clients
		);
	    }
	} else
	    delete_client(dp, cp), cp = NULL;
    }
    return (cp);
}

/*
 * Recalculate maxindbytes and update kernel
 * state if the value should change.
 */
static void
calculate_maxindbytes(void)
{
    client_common_t* com;
    int max = 0;

    sem_wait(&comlock);
    for (com = master; com; com = com->next)
	if (com->maxindbytes > max)
	    max = com->maxindbytes;
    sem_post(&comlock);
    if (max != maxindbytes && setmaxindbytes(max))
	maxindbytes = max;
}

/*
 * Delete/reclaim common state associated with a client
 * and terminate any active i/o thread associated with it.
 */
static void
delete_client_common(daemon_info_t* dp, client_common_t* com)
{
    client_common_t** cpp;

    IFTRACE(CLIENT)(dp, "Delete common state for %s:%u", com->host, com->port);
    sem_wait(&comlock);
    for (cpp = &master; *cpp; cpp = &(*cpp)->next)
	if (*cpp == com) {
	    *cpp = com->next;		/* remove from list */
	    break;
	}
    sem_post(&comlock);
    io_com_cleanup(dp, com);		/* asynch write support */
    if (com->fd != -1)
	(void) close(com->fd);		/* close stream */
    free((char*) com->host);
    if (com->maxindbytes == maxindbytes)
	calculate_maxindbytes();
    free(com);
}

/*
 * Delete/reclaim per-CPU client state and possibly
 * the common state (on last reference).
 */
static void
delete_client(daemon_info_t* dp, client_t* cp)
{
    kidblock_t* kb;
    kidblock_t* next;
    uint32_t refs = atomicDec(&cp->com->refs);
    extern int iobufsiz;		/* XXX */

    IFTRACE(CLIENT)(dp, "Delete client %s:%u refs %u", cp->host, cp->port,refs);
    if ((trace & TRACE_PERF) || cp->kdrops != 0)
	Trace(dp,
	    "Client %s:%u: %lu events %lu dropped (%u%%) %lu writes (%u%% push) for %llu bytes, %u push buffers for %u KB"
	    , cp->host, cp->port
	    , cp->kevents
	    , cp->kdrops, 100*cp->kdrops / NZ(cp->kdrops+cp->kevents)
	    , cp->writes, 100*cp->pushes / NZ(cp->writes)
	    , cp->totbytes
	    , cp->niobufs, (cp->niobufs*iobufsiz)/1024
	);

    for (kb = cp->kids.next; kb; kb = next) {
	next = kb->next;
	free(kb);
    }
    if (refs == 1)			/* last ref of common client state */
	delete_client_common(dp, cp->com);
    io_client_cleanup(dp, cp);
    free(cp);
}

/*
 * Purge all vestiges of a client given a pointer
 * to the common state block.
 */
void
purge_client(client_common_t* com)
{
    int cpu;
    for (cpu = getncpu()-1; cpu >= 0; cpu--) {
	daemon_info_t* dp = getdaemoninfo(cpu);
	client_t* cp;
	client_t** cpp;

	sem_wait(&dp->clients_sem);
	for (cpp = (client_t**) &dp->clients; cp = *cpp; cpp = &cp->next)
	    if (cp->com == com) {
		*cpp = cp->next;
		new_cpu_mask(dp);
		delete_client(dp, cp);
		break;
	    }
	sem_post(&dp->clients_sem);
    }
}

/*
 * Locate a client given a file descriptor.
 */
client_common_t*
find_client(int fd)
{
    client_common_t* com;

    sem_wait(&comlock);
    for (com = master; com && com->fd != fd; com = com->next)
	;
    sem_post(&comlock);
    return (com);
}

/*
 * Create a new client's state.
 */
client_common_t*
client_create(int fd, struct sockaddr* sockname)
{
    client_common_t* com;
    const char* hostname;
    int port;
    uint64_t mask;

    if (sockname->sa_family == AF_INET) {
	struct sockaddr_in* sin = (struct sockaddr_in*) sockname;
	struct hostent* hp = gethostbyaddr(&sin->sin_addr, sizeof (struct in_addr), AF_INET);
	if (hp) {
	    hostname = hp->h_name;
	    mask = check_access(hostname, sin->sin_addr);
	} else {
	    hostname = inet_ntoa(sin->sin_addr);
	    mask = check_access(NULL, sin->sin_addr);
	}
	port = sin->sin_port;
    } else if (sockname->sa_family == AF_UNIX) {
	static uint clientnum = 0;	/* for generating unique id */
	struct in_addr in;
	in.s_addr = INADDR_LOOPBACK;	/* use loopback addr for UNIX sockets */
	mask = check_access("localhost", in);
	hostname = "<local>", port = clientnum++;
    } else {
	Log(LOG_ERR, NULL, "Unknown socket address family %d",
	    sockname->sa_family);
	return (NULL);
    }
    if (mask == 0) {
	Log(LOG_ERR, NULL, "Client %s:%u: Access denied", hostname, port);
	return (NULL);
    }
    com = (client_common_t*) malloc(sizeof (*com));
    if (com) {
	memset(com, 0, sizeof (*com));
	com->fd = fd;
	com->maxindbytes = PAR_DEFINDBYTES;
	com->evmask = mask;
	com->host = strdup(hostname);
	com->port = port;

	{ int bs = 128*1024;
	  if (setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &bs, sizeof (bs)) < 0)
	    Log(LOG_WARNING, NULL, "setsockopt(SO_SNDBUF:%u): %s",
		bs, strerror(errno));
	}

	sem_wait(&comlock);
	com->next = master;
	master = com;
	sem_post(&comlock);
    } else
	Log(LOG_ERR, NULL, "Out of memory allocating common client info");
    return (com);
}

/*
 * Establish client connection state in
 * response to client's "open" request.
 */
int
client_open(client_common_t* com, uint64_t cookie)
{
    client_common_t* c;

    /*
     * Verify that the cookie is unique.  If not, then
     * someone is probably trying to use it to do something
     * they're not supposed to; reject the connection and
     * drop the other connection since they're probably
     * compromised (or collaborating).
     */
    sem_wait(&comlock);
    for (c = master; c && c->cookie != cookie; c = c->next)
	;
    sem_post(&comlock);
    if (c) {
	Log(LOG_NOTICE, NULL,
    "Client %s:%u rejected; attempt to reuse cookie %llx owned by %s:%u",
	    com->host, com->port, cookie, c->host, c->port);
	return (EPERM);
    } else {
	com->cookie = cookie;
	return (0);
    }
}

/*
 * Set client-controllable parameters in the
 * connection state as specified by a "params" request.
 */
int
client_setparams(client_common_t* com, rtmon_cmd_params_t* params)
{
    int i, n, nonZero;
    protosw_t* proto;

    if (params->ncpus > getncpu())
	params->ncpus = getncpu();
    nonZero = 0;
    for (i = 0, n = params->ncpus; n > 0; n -= 64, i++) {
	if (n < 64)
	    params->cpus[i] &= ((uint64_t)1<<n)-1;
	nonZero += (params->cpus[i] != 0);
    }
    if (!nonZero) {
	Log(LOG_WARNING, NULL, "Client %s:%u: no valid CPUs in the cpu mask",
	    com->host, com->port);
	return (ENXIO);
    }
    proto = findProtocol(params->protocol);
    if (proto == NULL) {
	Log(LOG_WARNING, NULL, "Client %s:%u: unregistered protocol %d",
	    com->host, com->port, params->protocol);
	return (ENXIO);
    }
    if (params->events == 0) {
	Log(LOG_WARNING, NULL, "Client %s:%u: invalid (zero) event mask %#llx",
	    com->host, com->port, params->events);
	return (ENXIO);
    }
    if ((params->events & com->evmask) == 0) {
	Log(LOG_NOTICE, NULL, "Client %s:%u rejected;"
	    " requested event mask %#llx disallowed (retricted to %#llx)",
	    com->host, com->port, params->events, com->evmask);
	return (EPERM);
    }
    params->events &= com->evmask;
    com->proto = proto;
    com->maxindbytes = ntohs(params->maxindbytes);
    com->events = params->events;
    for (i = 0, n = params->ncpus; n > 0; n -= 64, i++)
	com->cpus[i] = params->cpus[i];
    return (0);
}

/*
 * Apply the specified function to each
 * CPU enabled for event collection.
 */
static int
foreachcpu(client_common_t* com, int (*f)(daemon_info_t*, client_common_t*))
{
    int i, n;

    for (i = 0, n = RTMOND_MAXCPU; n > 0; n -= 64, i++) {
	uint64_t cm = com->cpus[i];
	if (cm != 0) {
	    int cpu;
	    for (cpu = 0; cpu < 64 && cm; cpu++) {
		uint64_t m = 1LL<<cpu;
		if (cm & m) {
		    if (!(*f)(getdaemoninfo(64*i+cpu), com))
			return (FALSE);
		    cm &= ~m;
		}
	    }
	}
    }
    return (TRUE);
}

/*
 * Pause transmission of events to a client.  We
 * remove it from the client list for the thread
 * that's monitoring the event queue and, potentially,
 * update the kernel's tstamp event mask.
 */
static int
c_suspend(daemon_info_t* dp, client_common_t* com)
{
    client_t* cp;
    client_t** cpp;

    IFTRACE(CLIENT)(dp, "Pause client at %s:%u", com->host, com->port);

    sem_wait(&dp->clients_sem);
    for (cpp = (client_t**) &dp->clients; cp = *cpp; cpp = &cp->next)
	if (cp->com == com) {
	    *cpp = cp->next;
	    new_cpu_mask(dp);
	    break;
	}
    sem_post(&dp->clients_sem);
    if (cp) {
	if ((trace & TRACE_PERF) || cp->kdrops != 0)
	    Trace(dp,
		"Client %s:%u: %lu events %lu dropped %lu writes (%u%% push) for %llu bytes",
		cp->host, cp->port, cp->kevents,
		cp->kdrops, 100*cp->kdrops / NZ(cp->kdrops+cp->kevents),
		cp->writes, 100*cp->pushes / NZ(cp->writes), cp->totbytes);

	io_client_pause(dp, cp);
	if (com->refs == 1)			/* only ref, kill push thread */
	    io_com_pause(dp, com);
	cp->events = 0;				/* restored when resume'd */
	cp->next = dp->paused;			/* add to paused list */
	dp->paused = cp;
    } else
	Log(LOG_WARNING, dp, "pause_client: %s:%u not found",
	    com->host, com->port);
    return (TRUE);				/* NB: always return OK */
}

/*
 * Implement a "suspect" request by a client.
 */
int
client_suspend(client_common_t* com)
{
    (void) foreachcpu(com, c_suspend);
    return (0);
}

static int
c_start(daemon_info_t* dp, client_common_t* com)
{
    client_t* cp;

    sem_wait(&dp->clients_sem);
    cp = new_client(dp, com);
    sem_post(&dp->clients_sem);

    return (cp != NULL && dp->isrunning);
}

/*
 * Check for paused stream that should be restarted.
 */
static int
c_resume(daemon_info_t* dp, client_common_t* com)
{
    client_t* cp;
    client_t** cpp;

    for (cpp = &dp->paused; cp = *cpp; cpp = &cp->next)
	if (cp->com == com) {
	    /*
	     * There is already a stream to this host+port; if
	     * it is paused, just restart it.  If the client
	     * went away but we had not cleaned up yet, then
	     * clear the state.  Otherwise, if a connection
	     * exists and is not paused, then reject the request.
	     */
	    IFTRACE(CLIENT)(dp, "Resume client at %s:%u",
		com->host, com->port);

	    *cpp = cp->next;				/* off paused list... */
	    cp->lastevt = 0;				/* force time sync */
	    if (com->refs == 1)				/* restart i/o thread */
		io_com_resume(com);
	    io_client_resume(dp, cp);
	    sem_wait(&dp->clients_sem);
	    cp->next = (client_t*) dp->clients;		/* ...on active list */
	    dp->clients = cp;
	    if (!dp->isrunning)				/* kick off thread */
		startMerge(dp);
	    sem_post(&dp->clients_sem);
	    return (dp->isrunning);
	}
    return (FALSE);
}

/*
 * Resume/start event collection in response to
 * a client's "resume" request.
 */
int
client_resume(client_common_t* com)
{
    if (com->refs == 0) {
	if (!io_com_init(com))			/* start push thread */
	    return (EAGAIN);
	/*
	 * Initiate event dispatching for each cpu marked for
	 * collection.  Note that all cpus get the same event
	 * mask; if the client wants different masks for different
	 * processors, then they need to establish a separate
	 * data connection for each mask.  Also, a single descriptor
	 * is shared between all the event threads, but no global
	 * sorting is done by event timestamp (only on a per-cpu
	 * basis); thus clients may/are likely to receive events
	 * out of order if they receive events from multiple cpus.
	 * To deal with this a client should buffer events on a
	 * per-cpu basis and "flush" events based on the assumption
	 * that events are ordered on a per-cpu basis.
	 */
	com->refs++;				/* hold reference */
	if (foreachcpu(com, c_start)) {
	    (void) atomicDec(&com->refs);
	    /*
	     * Return before completing the work so the command
	     * response can be sent before any event data is
	     * sent to the client.  This splitup in the work
	     * is necessary because the control protocol shares
	     * the same connection as the event data.
	     */
	    return (-1);
	} else {
	    Log(LOG_ERR, NULL, "Client %s:%u: could not setup new client",
		com->host, com->port);
	    (void) atomicDec(&com->refs);
	    purge_client(com);
	    return (EAGAIN);
	}
    } else
	return (foreachcpu(com, c_resume) ? 0 : EAGAIN);
}

static int
c_writeheader(daemon_info_t* dp, client_common_t* com)
{
    client_t* cp;
    sem_wait(&dp->clients_sem);
    for (cp = (client_t*) dp->clients; cp && cp->com != com; cp = cp->next)
	;
    sem_post(&dp->clients_sem);
    if (cp)
	(*com->proto->writeHeader)(dp, cp, com->events);
    return (TRUE);
}

static int
c_setmask(daemon_info_t* dp, client_common_t* com)
{
    sem_wait(&dp->clients_sem);
    new_cpu_mask(dp);
    sem_post(&dp->clients_sem);
    return (TRUE);
}

/* 
 * Complete the work associated with starting up a
 * new client's event collection *after* the command
 * response has been sent to the client.
 */
void
client_start(client_common_t* com)
{
    (void) foreachcpu(com, c_writeheader);
    if (com->maxindbytes > maxindbytes && setmaxindbytes(com->maxindbytes))
	maxindbytes = com->maxindbytes;
    (void) foreachcpu(com, c_setmask);
}
