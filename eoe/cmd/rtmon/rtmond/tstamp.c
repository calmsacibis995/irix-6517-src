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
 * Timestamp support; everything that deals with the kernel's
 * tstamp support.  The server creates a thread for each CPU
 * where events are being collected.  These events are then
 * filtered according to the events a client has registered
 * interest in and formatted according to a client-specified
 * protocol (e.g. WindView).  Data is then written on the
 * client's network connection.  Note that we do not (at
 * present) merge event data received on multiple CPUs; the
 * client is responsible for doing that.
 */
#include "rtmond.h"
#include "timer.h"

#include <stdlib.h>
#include <fcntl.h>
#include <sys/syssgi.h>
#include <sys/mman.h>
#include <sys/time.h>

/*#define DEBUG_PERFORMANCE*/

/*
 * When no new events are available in either the user or
 * kernel queues we do a syssgi call to wait for a period
 * of time or for the kernel queue to reach half full.
 */
int	waittime = _CONFIG_WAITTIME;
/*
 * Interval for issuing "null records" to help clients in merging
 * event streams from multiple CPUs.  If a client hasn't received
 * any data for this period of time then we send it a SORECORD event
 * with no real events.  The client can then use the timestamp info
 * in the record to trigger event merging.  If we didn't do this
 * then clients might have to buffer very large amounts of data to
 * insure merged events are properly ordered by time.
 */
int	quiettime = _CONFIG_QUIETTIME;

/*
 * Convert milliseconds to RTC ticks.
 */
#define	MStoTicks(ms)	((ms) * clockfreq)
#define	TicksToUS(t)	(1000*(t)/clockfreq)
#define TSTAMP_TIME_THRESH_TICKS MStoTicks(TSTAMP_TIME_THRESHOLD)

#define	WATER_MARK_NUM	3
#define	WATER_MARK_DEN	2

#define	TSTAMP_KERN_IX(dp, count) \
	(dp->kern_index+count)%NUMB_KERNEL_TSTAMPS
#define	TSTAMP_USER_IX(dp, count) \
	(dp->user_index+count)%NUMB_USER_TSTAMPS

#define	NZ(x)	((x) ? (x) : 1)

#ifdef DEBUG_PERFORMANCE
#define	DEBUG_DECL	uint64_t start_time, mid_time
#define	DEBUG_STEP1	start_time = readcc()
#define	DEBUG_STEP2	mid_time = readcc()
#define	DEBUG_DONE(dp) {						      \
	uint64_t end_time = readcc();					      \
	IFTRACE(PERF)(dp, "processed %d tstamps at a %lluus per event",	      \
	    dp->kern_count + dp->user_count,				      \
	    TicksToUS(mid_time-start_time)/NZ(dp->kern_count+dp->user_count));\
	IFTRACE(PERF)(dp, "write and update %lluus/event total %lluus/event", \
	    TicksToUS(end_time-mid_time)/NZ(dp->kern_count+dp->user_count),   \
	    TicksToUS(end_time-start_time)/NZ(dp->kern_count+dp->user_count));\
}
#else
#define	DEBUG_DECL
#define	DEBUG_STEP1
#define	DEBUG_STEP2
#define	DEBUG_DONE(dp)
#endif

static	void init_user_queue(daemon_info_t*);
static	void cleanup_user_queue(daemon_info_t*);
static	void enter_user_event(daemon_info_t*, tstamp_event_entry_t*);
static	void user_tstamp_update(user_queue_t* uq, int nread);
static	void init_kernel_queue(daemon_info_t *dp);
static	void cleanup_kernel_queue(daemon_info_t *dp);
static	void enter_kernel_event(daemon_info_t *dp, tstamp_event_entry_t* ke);
static	void scan_to_pid_event(daemon_info_t*, tstamp_event_entry_t*,
	    kern_queue_t*);
static	void notify_lost_events(daemon_info_t*, __int64_t, int);

static	sem_t threadstart;		/* sema for starting threads */

void
init_thread(void)
{
    uint64_t clockfreq;

    clockfreq = NSEC_PER_SEC / cc.cycleval;
    assert(clockfreq != 0);
    if (sem_init(&threadstart, 0, 0) != 0)
	Fatal(NULL, "Cannot initialize thread startup semaphore: %s",
	    strerror(errno));
    IFTRACE(DEBUG)(NULL, "Clock frequency %llu (cycleval %lu)",
	clockfreq, (unsigned long) cc.cycleval);
    IFTRACE(DEBUG)(NULL,
	"Quiet time %llu Wait time %llu tstamp merge time %llu", 
	MStoTicks(quiettime), MStoTicks(waittime), TSTAMP_TIME_THRESH_TICKS);
}

static void
sem_assert(daemon_info_t* dp, sem_t* sem, int vexpect)
{
    int v;
    sem_getvalue(sem, &v);
    if (v != vexpect)
	Fatal(dp, "sem_assert: expected %d, got %d", vexpect, v);
}

static void
syssgi_error(daemon_info_t* dp, const char* op)
{
    Fatal(dp, "syssgi(%s): %s", op, strerror(errno));
}

#ifdef USE_SPROC
static void
tstamp_merge(void* vdp, size_t stacksize)
#else
static void*
tstamp_merge(void* vdp)
#endif
{
    daemon_info_t* dp = (daemon_info_t*) vdp;
    kern_queue_t* kq;
    user_queue_t* uq;
    tstamp_event_entry_t* ke;
    tstamp_event_entry_t* ue;
    client_t* cp;
    uint64_t clockfreq;
    uint64_t quiet_interval;
    uint64_t tstamp_merge_threshold;
    DEBUG_DECL;

    IFTRACE(THREAD)(dp, "Starting event collection thread");
    dp->isrunning = 1;
#ifdef USE_SPROC
    (void) stacksize;
    /*
     * Immediately turn off par tracing for this thread.
     */
    disallow_tracing(dp);
#else
    (void) pthread_detach(pthread_self());
#endif

    init_user_queue(dp);
    init_kernel_queue(dp);

    sem_post(&threadstart);		/* wakeup master thread */

    kq = &dp->kern_queue;
    uq = dp->user_queue;

    ke = &kq->tstamp_event_entry[0];
    ue = &uq->tstamp_event_entry[0];

    dp->kern_index = 0;
    dp->user_index = 0;

    /*
     * Setup various time intervals dependent
     * on the resolution of the cycle counter.
     */
    clockfreq = NSEC_PER_SEC / cc.cycleval;
    quiet_interval = MStoTicks(quiettime);
    tstamp_merge_threshold = TSTAMP_TIME_THRESH_TICKS;

    /*
     * Lock the client list for this thread to guard
     * against the master thread adding or removing clients.
     * We unlock the list each time through the loop so the
     * master thread can get in.  The locking also is used to
     * guarantee that kernel state cleanup done at thread
     * shutdown is completed before the master thread starts
     * up any new event collection thread for this CPU.
     */
    sem_wait(&dp->clients_sem);
    while ((cp = (client_t*) dp->clients) != NULL) {
	int kevts, uevts;
	uint64_t recent_time;

	/*
	 * If no data has been pushed to a client in a while
	 * (nominally 100ms) then push anything currently
	 * buffered (or send a null record) *if* another buffer
	 * is available on the free list.  This helps clients
	 * that need to collate events from multiple cpu's
	 * (otherwise they'd need to buffer potentially
	 * infinite amounts of data).
	 */ 
	recent_time = readcc();
	do {
	    ioblock_t* io = cp->io;
	    if (io != NULL && cp->events) {
		if (recent_time - cp->lastpush > quiet_interval && cp->iofree) {
		    IFTRACE(EVENTIO)(dp,
		"Flush data, %llu us since last event, %llu us since last push",
			TicksToUS(recent_time - cp->lastevt),
			TicksToUS(recent_time - cp->lastpush));
		    cp->pushes++;
		    if (io->off > cp->lowmark)
			(*cp->proto->flush)(dp, cp,
			    io->u.ev[1].tstamp, cp->lastevt);
		    else
			(*cp->proto->flush)(dp, cp,
			    recent_time, recent_time);
		}
	    }
	} while (cp = cp->next);

	/*
	 * Pause waiting for new events or a timeout.
	 *
	 * Note also that we unlock the client list so the
	 * master thread has a chance to add and delete clients.
	 * If the master thread is waiting to manipulate the
	 * client list, we should be preempted as a result of
	 * the sem_post call because the master thread will
	 * have the same rt priority and have been waiting
	 * to run (so have a longer wait time).
	 */
	dp->tstampwaits++;
	sem_post(&dp->clients_sem);		/* unlock before blocking */
	if (syssgi(SGI_RT_TSTAMP_WAIT, dp->cpu, waittime) < 0)
	    syssgi_error(dp, "SGI_RT_TSTAMP_WAIT");
	sem_wait(&dp->clients_sem);		/* lock list again */

	kevts = kq->state->tstamp_counter;
	uevts = uq->state.tstamp_counter;
	recent_time = readcc();

#ifdef notdef
	/*
	 * Events are stamped with the value of the cycle counter.
	 * When processing the queues below we delay processing the
	 * most recent events until they've "aged" at least
	 * TSTAMP_TIME_THRESH_TICKS ticks so that we accurately
	 * merge user and kernel events (either of which might be
	 * delayed slightly by queueing overhead and/or contention
	 * in their delivery to the server).
	 *
	 * XXX This should be optimized according to whether
	 *     or not any user timestamps might be received
	 *     delaying things here may cause inefficiency in
	 *     sending data to clients as well as delays.
	 */
	if (uq->state.nclients != 0)
	    recent_time -= tstamp_merge_threshold;
#endif

	IFTRACE(EVENTS)(dp, "%d kernel events %d user events", kevts, uevts);

	DEBUG_STEP1;
	dp->kern_count = dp->user_count = 0;
	if (uevts > 0 && kevts > 0) {
	    /*
	     * Kernel & user events, merge based on time stamps.
	     */
	    while (dp->kern_count < kevts && dp->user_count < uevts) {
		if (ke[dp->kern_index].evt == 0 || ue[dp->user_index].evt == 0)
		    break;
		if (ke[dp->kern_index].tstamp < ue[dp->user_index].tstamp)
		    enter_kernel_event(dp, &ke[dp->kern_index]);
		else
		    enter_user_event(dp, &ue[dp->user_index]);
	    }
	}
	if (kevts > 0) {
	    while (dp->kern_count < kevts) {
		tstamp_event_entry_t* ev = &ke[dp->kern_index];
		if (ev->evt == 0) {
		    /*
		     * Two possibilities: either kernel has yet to
		     * fill in the event (curr_index is incremented
		     * before the event record is written); or a
		     * multi-record event was discarded because the
		     * second or later record was unavailable for use.
		     *
		     * In the first case we terminate scanning the
		     * event queue so the kernel has time to put things
		     * in a consistent state.
		     *
		     * In the second case the kernel will have set
		     * the lost_counter field to indicate that it
		     * dropped an event; we can check this and if
		     * non-zero discard all records up to curr_index.
		     * When we do an update call below we'll get back
		     * in sync since the kernel should stop entering
		     * events in the queue past this point.
		     */
		    IFTRACE(LOSTEVENTS)(dp,
			"Zero event, kevts %d index %d lost_counter %d"
			, kevts
			, dp->kern_index
			, kq->state->lost_counter
		    );
		    break;
		}
		if (ev->tstamp >= recent_time &&
		    kevts - dp->kern_count <= TSTAMP_NUMB_THRESHOLD) {
		    IFTRACE(EVENTS)(dp,
		"Delay processing %d kernel events; tstamp %llu threshold %llu"
			, kevts - dp->kern_count
			, ev->tstamp
			, recent_time
		    );
		    break;
		}
		enter_kernel_event(dp, ev);
	    }
	} else if (uevts > 0) {
	    while (dp->user_count < uevts) {
		if (ue[dp->user_index].tstamp >= recent_time) {
		    IFTRACE(EVENTS)(dp,
		"Delay processing %d user events; tstamp %llu recent time %llu"
			, uevts - dp->user_count
			, ue[dp->user_index].tstamp
			, recent_time
		    );
		    break;
		}
		enter_user_event(dp, &ue[dp->user_index]);
	    }

	}
	DEBUG_STEP2;

	/*
	 * Check and update the kernel count.
	 */
	if (kq->state->lost_counter > 0) {
	    int klost;
	    /*
	     * Setup to resynchronize with the kernel.  We
	     * know that once lost_counter is non-zero the kernel
	     * will discard all events until we do an "update"
	     * call (below) to indicate we've removed some events
	     * from the queue.  We first flush any events that
	     * have been added to the queue since we grabbed the
	     * value of tstamp_counter above.
	     */
	    if (kevts != kq->state->tstamp_counter) {
		kevts = kq->state->tstamp_counter;
		while (dp->kern_count < kevts && ke[dp->kern_index].evt != 0)
		    enter_kernel_event(dp, &ke[dp->kern_index]);
	    }
	    klost = kq->state->lost_counter;
	    IFTRACE(LOSTEVENTS)(dp,
		"Lost %d kernel timestamps; tstamp_counter %d"
		, klost
		, kq->state->tstamp_counter
	    );
	    notify_lost_events(dp, recent_time, klost);
	    dp->klost += klost;
	    dp->kevents += dp->kern_count;
	    /*
	     * We now know the queue is empty and that no new
	     * events will be inserted until we do an update call
	     * below.  Setup our state to reflect the kernel's
	     * and so that the update will clear out any bogus
	     * kernel state (such as when curr_index is bumped
	     * for a multi-record event but the event doesn't fit).
	     */
	    dp->kern_count = kq->state->tstamp_counter;	/* clear kernel */
	    dp->kern_index = kq->state->curr_index;	/* resync index */
	} else {
	    dp->kevents += dp->kern_count;
	    dp->kdelayed += kevts - dp->kern_count;
	}
	if (dp->kern_count > 0) {
	    IFTRACE(EVENTS)(dp, "tstamp update count %u", dp->kern_count);
	    if (syssgi(SGI_RT_TSTAMP_UPDATE, dp->cpu, dp->kern_count) < 0)
		Log(LOG_WARNING, dp,
		    "Error resetting tstamp counter; count %d: %s",
		    dp->kern_count, strerror(errno));
	}

	/*
	 * Check and update the user count.
	 */
	if (uq->state.lost_counter > 0) {
	    IFTRACE(LOSTEVENTS)(dp, "Lost %d user timestamps",
		uq->state.lost_counter);
	    notify_lost_events(dp,
		ue[dp->user_index].tstamp, uq->state.lost_counter);
	    dp->ulost += uq->state.lost_counter;
	    dp->uevents += dp->user_count;

	    dp->user_count = uq->state.tstamp_counter;
	    dp->user_index = uq->state.curr_index;	/* resync index */
	} else {
	    dp->uevents += dp->user_count;
	    dp->udelayed += uevts - dp->user_count;
	}
	if (dp->user_count > 0)
	    user_tstamp_update(dp->user_queue, dp->user_count);
	DEBUG_DONE(dp);
    }

    /*
     * No more clients to process, cleanup kernel state and
     * terminate the thread.  The client list is locked so
     * we can manipulate daemon_info_t state (e.g. the statistics)
     * w/o problems.  Note that we clear the isrunning flag
     * when we're done (but after cleaning up state).  If the
     * master thread is waiting on the list lock it should
     * start up a new thread for this CPU after we're done.
     */
    cleanup_kernel_queue(dp);
    cleanup_user_queue(dp);
    kid_flush(dp);

    if ((trace & TRACE_PERF) || dp->klost != 0 || dp->ulost != 0)
	Trace(dp,
	    "Kernel events: %lld total, %lld delayed, %lld lost (%d%%) "
	    "User events: %lld total, %lld delayed, %lld lost (%d%%) "
	    "Waits: %lld"
	    , dp->kevents, dp->kdelayed, dp->klost
		, (100*dp->klost)/NZ(dp->kevents+dp->klost)
	    , dp->uevents, dp->udelayed, dp->ulost
		, (100*dp->ulost)/NZ(dp->uevents+dp->ulost)
	    , dp->tstampwaits
	);
    IFTRACE(THREAD)(dp, "Event processing thread done; clients %#x", dp->clients);
    dp->isrunning = 0;
    sem_post(&dp->clients_sem);

#ifdef USE_SPROC
    exit(EXIT_SUCCESS);
#else
    return (EXIT_SUCCESS);
#endif
}

/*
 * Notify all clients that events were lost.
 */
static void
notify_lost_events(daemon_info_t* dp, __int64_t time, int counter)
{
    client_t* cp = (client_t*) dp->clients;
    while (cp) {
	(*cp->proto->lostEvent)(dp, cp, time, counter);
	cp->lastevt = time;
	cp = cp->next;
    }
}

/*
 * Start a new thread that runs the event processing/merging
 * procedure.  We assume that we're called with the client list
 * semaphore locked; this guards against clobbering state that
 * is currently in use by a thread that's terminating.
 */
void
startMerge(daemon_info_t* dp)
{
    assert(dp->isrunning == 0);

    dp->mask = 0;
    /* zero statistics */
    dp->kevents = dp->uevents = 0;
    dp->tstampwaits = 0;
    dp->kdelayed = dp->udelayed = 0;
    dp->klost = dp->ulost = 0;
    dp->clientwrites = 0;
    dp->clientdata = 0;

    sem_assert(dp, &threadstart, 0);
#ifdef USE_SPROC
    if (sprocsp(tstamp_merge, PR_SALL, dp, NULL, _SPROC_EVTHREAD_STACKSIZE) >= 0) {
#else
    errno = pthread_create(&dp->pthread, NULL, tstamp_merge, dp);
    if (errno == 0) {
#endif
	/*
	 * Wait for the thread to initialize the kernel event queue.
	 * This does several things, including avoid a race where
	 * the collection thread may delete an existing tstamp buffer
	 * in the kernel after the master thread has installed an
	 * event collection mask.  This also insures that clients
	 * don't get notified that their data connection is setup
	 * until all the collection threads are ready to go--which
	 * is important for clients that need to do things like
	 * enable system call tracing on one or more processes.
	 */
	if (sem_wait(&threadstart) != 0)
	    Log(LOG_ERR, dp, "sem_wait(threadstart): %s", strerror(errno));
    } else
	Log(LOG_ERR, dp, "Could not start event collection thread: %s",
		strerror(errno));
}

static void
init_user_queue(daemon_info_t* dp)
{
    user_queue_t* uq = dp->user_queue;
    int i;

    uq->state.lost_counter = 0;
    uq->state.tstamp_counter = 0;
    uq->state.curr_index = 0;

    uq->nentries = NUMB_USER_TSTAMPS;
    uq->water_mark = (uq->nentries*WATER_MARK_NUM)/WATER_MARK_DEN;

    /* clear all valid bits */
    for (i = 0; i < uq->nentries; i++)
	uq->tstamp_event_entry[i].evt = 0;
    uq->enabled = TRUE;
}

static void
cleanup_user_queue(daemon_info_t* dp)
{
    user_queue_t* uq = dp->user_queue;

    uq->enabled = FALSE;
}

static void
enter_user_event(daemon_info_t *dp, tstamp_event_entry_t *ue)
{
    client_t* cp;
    /*
     * XXX filter based on interest?
     */
    for (cp = (client_t*) dp->clients; cp; cp = cp->next) {
	(*cp->proto->userEvent)(dp, cp, ue);
	cp->lastevt = ue->tstamp;
    }

    ue->evt = 0;
    dp->user_index = TSTAMP_USER_IX(dp, 1);
    dp->user_count++;
}

static void
user_tstamp_update(user_queue_t* uq, int nread)
{
    if ((uq->state.tstamp_counter -= nread) < uq->nentries)
	uq->state.lost_counter = 0;
}

/*
 * Estalish and initialize the kernel timestamp buffers
 * and map them into our address space for reading.
 *
 * Error recovery is annoying since we exit() fatally
 * on everything.  This means we have to tear down
 * whatever state we've built up along the way and also
 * anything that our caller built up.  So far the big
 * issues outside of our own stateis the need to call
 * cleanup_user_queue() and issue a sem_post() against
 * threadstart ...
 *
 * If rtmond is to coexist with UTRACE (kernel event collection
 * using wrap-around mode) then it has save and restore the event
 * mask and EOB mode and carefully setup event collection to avoid
 * losing events already collected by the kernel.  The downside
 * to doing this is that if rtmond is interrupted such that it
 * leaves the system in a state where events are being collected
 * then this (bogus) state will be propagated instead of it being
 * reset to a default (good) state.
 *
 * XXX Need to rework stuff for better interaction
 *     with UTRACE collection.
 */
static void
init_kernel_queue(daemon_info_t* dp)
{
    int addr;
    int i;
    char *sbase;
    int fd;

    /*
     * Force a known state: clear the event mask to
     * stop any event collection.  This should be ok
     * since the master thread should set it after
     * we're properly initialized.  If we don't clear
     * the mask then we may inherit an old mask and
     * receive unexpected/unwanted events until the
     * desired mask is installed.
     */
    IFTRACE(TSTAMP)(dp, "Zero tstamp mask");
    /* NB: can't really check error return here */
    (void) syssgi(SGI_RT_TSTAMP_MASK, dp->cpu, (uint64_t) 0, &dp->omask);
    IFTRACE(TSTAMP)(dp, "Create %d tstamp buffer", NUMB_KERNEL_TSTAMPS);
    addr = syssgi(SGI_RT_TSTAMP_CREATE, dp->cpu, NUMB_KERNEL_TSTAMPS);
    if (addr == -1) {
	if (errno != EEXIST) {
	    cleanup_user_queue(dp);
	    sem_post(&threadstart);
	    syssgi_error(dp, "SGI_RT_TSTAMP_CREATE");
	}

	/*
	 * A timestamp buffer already exists.  If it's from
	 * a previous instance of this server, delete it and
	 * install our own buffer.  If it's for UTRACEs,
	 * reuse the existing buffer.  Find out by attempting
	 * to delete it.
	 */
	IFTRACE(TSTAMP)(dp, "Try to delete existing tstamp buffer");
	if (syssgi(SGI_RT_TSTAMP_DELETE, dp->cpu) < 0) {
	    if (errno != EBUSY) {
		cleanup_user_queue(dp);
		sem_post(&threadstart);
		syssgi_error(dp, "SGI_RT_TSTAMP_DELETE");
	    }
	    /*
	     * Use existing event buffer; should exist for
	     * UTRACE collection.
	     */
	    IFTRACE(TSTAMP)(dp, "Reuse existing tstamp buffer");
	    addr = syssgi(SGI_RT_TSTAMP_ADDR, dp->cpu);
	    if (addr == -1) {
		cleanup_user_queue(dp);
		sem_post(&threadstart);
		syssgi_error(dp, "SGI_RT_TSTAMP_ADDR");
	    }
	    /*XXX don't know how large the buffer is */
	}
	else {
	    /*
	     * The buffer must have been from an old instance of
	     * this server.  Create a new one.
	     */
	    IFTRACE(TSTAMP)(dp, "Create %d tstamp buffer", NUMB_KERNEL_TSTAMPS);
	    addr = syssgi(SGI_RT_TSTAMP_CREATE, dp->cpu, NUMB_KERNEL_TSTAMPS);
	    if (addr == -1) {
		cleanup_user_queue(dp);
		sem_post(&threadstart);
		syssgi_error(dp, "SGI_RT_TSTAMP_CREATE");
	    }
	}
    }

    IFTRACE(TSTAMP)(dp, "Map tstamp buffer at %#x", addr);
    fd = open("/dev/mmem", O_RDONLY);
    if (fd < 0) {
	int err = errno;
	syssgi(SGI_RT_TSTAMP_DELETE, dp->cpu);
	cleanup_user_queue(dp);
	sem_post(&threadstart);
	Fatal(dp, "/dev/mmem: open: %s", strerror(err));
    }
    sbase = (char*) mmap(0,
	NUMB_KERNEL_TSTAMPS*sizeof (tstamp_event_entry_t) +
	    TSTAMP_SHARED_STATE_LEN,
	PROT_READ|PROT_WRITE, MAP_PRIVATE, fd, addr);
    if ((void*) sbase == MAP_FAILED) {
	int err = errno;
	close(fd);
	syssgi(SGI_RT_TSTAMP_DELETE, dp->cpu);
	cleanup_user_queue(dp);
	sem_post(&threadstart);
	Fatal(dp, "mmap(tstamp buffer): %s", strerror(err));
    }
    close(fd);

    dp->kern_queue.state = (tstamp_shared_state_t*) sbase;
    dp->kern_queue.tstamp_event_entry =
	(tstamp_event_entry_t*)(sbase + TSTAMP_SHARED_STATE_LEN);

    IFTRACE(TSTAMP)(dp, "Set tstamp EOB mode to %#x", RT_TSTAMP_EOB_STOP);
    /* set the system to stop collection when end_of_buffer is
       reached.  Also makes sure the buffer and shared state are
       initialized properly */
    if (syssgi(SGI_RT_TSTAMP_EOB_MODE, dp->cpu, RT_TSTAMP_EOB_STOP, &dp->eobmode) < 0) {
	syssgi(SGI_RT_TSTAMP_DELETE, dp->cpu);
	cleanup_user_queue(dp);
	sem_post(&threadstart);
	syssgi_error(dp, "SGI_RT_TSTAMP_EOB_MODE");
    }
}

/*
 * Restore kernel state to how it was prior
 * to our starting up event collection.
 */
static void
cleanup_kernel_queue(daemon_info_t* dp)
{
    IFTRACE(TSTAMP)(dp, "Restore EOB mode to %#x", dp->eobmode);
    if (syssgi(SGI_RT_TSTAMP_EOB_MODE, dp->cpu, dp->eobmode, (caddr_t) NULL) < 0)
	Log(LOG_WARNING, dp, "syssgi(SGI_RT_TSTAMP_EOB_MODE): %s",
	    strerror(errno));
    IFTRACE(TSTAMP)(dp, "Restore tstamp mask to %#llx", dp->omask);
    if (syssgi(SGI_RT_TSTAMP_MASK, dp->cpu, dp->omask, (caddr_t) NULL) < 0)
	Log(LOG_WARNING, dp, "syssgi(SGI_RT_TSTAMP_MASK): %s", strerror(errno));
    dp->user_queue->enabled = FALSE;
    if (munmap(dp->kern_queue.state,
	NUMB_KERNEL_TSTAMPS*sizeof (tstamp_event_entry_t) +
	    TSTAMP_SHARED_STATE_LEN) != 0)
	Log(LOG_WARNING, dp, "munmap(kernel event queue): %s", strerror(errno));
    if (dp->eobmode != RT_TSTAMP_EOB_WRAP) {
	IFTRACE(TSTAMP)(dp, "Delete tstamp buffer");
	if (syssgi(SGI_RT_TSTAMP_DELETE, dp->cpu) < 0)
	    Log(LOG_WARNING, dp, "syssgi(SGI_RT_TSTAMP_DELETE): %s",
		strerror(errno));
    }
}

#define	IN_RANGE(b,ev)	(KERNEL_EVENT(b) <= ev && ev < KERNEL_EVENT(b+100))

/*
 * Return the classes that a client must be 
 * interested in in order to receive the
 * specified event.
 */
static uint64_t
event_to_class(const tstamp_event_entry_t* ev)
{
    switch (ev->evt) {
    case TSTAMP_EV_EODISP:
    case TSTAMP_EV_EOSWITCH:
    case EVENT_TASK_STATECHANGE:
    case EVENT_WIND_EXIT_IDLE:
	return (RTMON_TASK|RTMON_TASKPROC);
    case TSTAMP_EV_SYSCALL_BEGIN:
    case TSTAMP_EV_SYSCALL_END:
	return (RTMON_SYSCALL);
    case TSTAMP_EV_SIGSEND:
    case TSTAMP_EV_SIGRECV:
	return (RTMON_SIGNAL);
    case TSTAMP_EV_PROF_STACK32:
    case TSTAMP_EV_PROF_STACK64:
	return (RTMON_PROFILE);
    case TSTAMP_EV_ALLOC:
	return (RTMON_ALLOC);
    case TSTAMP_EV_EXIT:
    case TSTAMP_EV_EXEC:
    case TSTAMP_EV_FORK:
	/* anything potentially process-oriented gets these */
	return (RTMON_PIDAWARE);
    case UTRACE_EVENT:
    case JUMBO_EVENT:
	/* nobody needs to see UTRACEs */
	return 0;
    }
    if (MIN_INT_ID-2 <= ev->evt && ev->evt <= MAX_INT_ID)
	return (RTMON_INTR);
    if (IN_RANGE(100, ev->evt))				/* XXX */
	return (RTMON_VM);
    if (IN_RANGE(200, ev->evt))				/* XXX */
	return (RTMON_SCHEDULER);
    if (IN_RANGE(DISK_EVENT_BASE, ev->evt))
	return (RTMON_DISK);
    if (IN_RANGE(NET_EVENT_BASE, ev->evt))
	return (RTMON_NETFLOW|RTMON_NETSCHED);		/* XXX */
    return (RTMON_ALL);					/* ??? XXX */
}

static void
enter_kernel_event(daemon_info_t* dp, tstamp_event_entry_t* ke)
{
    uint64_t cmask;
    client_t* cp;

    /*
     * The kernel may not get this right, so we do it since
     * it's easy and there's no difference in cost doing it
     * one place versus another.
     */
    ke->cpu = dp->cpu;
    /*
     * Track pid-related events and update the pid
     * data structures.  We also cache the pid of
     * the last running task for the old WindView
     * protocol.
     */
    switch (ke->evt) {
    case TSTAMP_EV_EOSWITCH:
    case DISK_EVENT_QUEUED:
    case DISK_EVENT_START:
    case DISK_EVENT_DONE:
    case EVENT_TASK_STATECHANGE:
    case TSTAMP_EV_EODISP:
    case TSTAMP_EV_SIGRECV:
	kid_check_kid(dp, (int64_t) ke->qual[0], ke->tstamp);
	break;
	
    case TSTAMP_EV_SYSCALL_BEGIN:
    case TSTAMP_EV_SYSCALL_END: {
	const tstamp_event_syscall_data_t* sdp =
	    (const tstamp_event_syscall_data_t*) &ke->qual[0];

	kid_check_kid(dp, (int64_t) sdp->k_id, ke->tstamp);
	/*
	 * Process system call events specially.  Compare
	 * the magic cookie passed by the kernel to the
	 * client's cookie; if they match pass the event on.
	 */
	for (cp = (client_t*) dp->clients; cp; cp = cp->next)
	    if (cp->events & RTMON_SYSCALL) {
		if (sdp->cookie && sdp->cookie == cp->cookie) {
		    (*cp->proto->kernelEvent)(dp, cp, ke);
		    cp->lastevt = ke->tstamp;
		    break;		/* should be only one client */
		}
	    }
	goto done;
    }
    case TSTAMP_EV_FORK: {
	const tstamp_event_fork_data_t* fdp =
	    (const tstamp_event_fork_data_t*) &ke->qual[0];
	kid_rename_kid(dp,  fdp->pkid, fdp->name, ke->tstamp, -1);
	kid_dup_kid(dp, fdp->pkid, fdp->ckid);
	kid_check_kid(dp,fdp->ckid, ke->tstamp);
	break;
    }
    case TSTAMP_EV_EXEC: {
	const tstamp_event_exec_data_t* edp =
	    (const tstamp_event_exec_data_t*) &ke->qual[0];
	
	kid_rename_kid(dp, (int64_t) edp->k_id, edp->name, ke->tstamp, edp->pid);
	break;
    }
    case TSTAMP_EV_EXIT:
	kid_purge_kid(dp, (int64_t) ke->qual[0]);
	break;
    }
    /*
     * Dispatch event to protocol-specific handler
     * based on client interests.
     *
     * XXX more fine-grained filter support?
     */
    cmask = event_to_class(ke);
    for (cp = (client_t*) dp->clients; cp; cp = cp->next)
	if (cp->events & cmask) {
	    (*cp->proto->kernelEvent)(dp, cp, ke);
	    cp->lastevt = ke->tstamp;
	}
done:
    /*
     * Mark entry as available so kernel will reuse it.
     */
    if (ke->jumbocnt != 0) {
       /*
	* Jumbo event, mark extended entries free first.
	*/
	int n = ke->jumbocnt;
	int ix = TSTAMP_KERN_IX(dp, n);
	do {
	    dp->kern_queue.tstamp_event_entry[ix].jumbocnt = 0;
	    dp->kern_queue.tstamp_event_entry[ix].evt = 0;
	    if (--ix < 0)
		ix += NUMB_KERNEL_TSTAMPS;
	} while (--n);
	dp->kern_index = TSTAMP_KERN_IX(dp, 1+ke->jumbocnt);
	dp->kern_count += 1+ke->jumbocnt;
	ke->jumbocnt = 0;
    } else {
	dp->kern_index = TSTAMP_KERN_IX(dp, 1);
	dp->kern_count++;
    }
    ke->evt = 0;
}

/*
 * Compute a new event mask for a CPU as the union of all
 * the event masks specified by the CPU's clients.  If it's
 * different from the mask currently installed, then install
 * the new mask.
 *
 * NB: Caller is assumed to have the client list locked.
 */
void
new_cpu_mask(daemon_info_t *dp)
{
    uint64_t mask = dp->omask;	/* preserve inherited mask */
    client_t* cp;

    for (cp = (client_t*) dp->clients; cp != NULL; cp = cp->next) {
	/*
	 * If the client event mask is zero, then propagate
	 * the value from the common state before calculating
	 * the union value over all clients.  This is used
	 * when starting up a client to avoid having events
	 * dispatched to a client before everything is setup
	 * and the client request is acknowledged.
	 */
	if (cp->events == 0)			/* propagate common state */
	    cp->events = cp->com->events;
	mask |= cp->events;
    }
    if (dp->mask != mask) {
	IFTRACE(TSTAMP)(dp, "Set tstamp mask; old %#llx, new %#llx",
	    dp->mask, mask);
	if (syssgi(SGI_RT_TSTAMP_MASK, dp->cpu, mask, (caddr_t) NULL) < 0)
	    Log(LOG_WARNING, dp,
		"syssgi(SGI_RT_TSTAMP_MASK): mask %#llx: %s",
		mask, strerror(errno));
	else
	    dp->mask = mask;
    }
}
