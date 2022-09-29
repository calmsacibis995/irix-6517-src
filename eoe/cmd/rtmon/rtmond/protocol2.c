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
 * (New) protocol support.  Event data is passed
 * back to the client unchanged.
 */
#include "rtmond.h"
#include "timer.h"

#include <time.h>
#include <sys/utsname.h>
#include <sys/kabi.h>

static	void _proto2_flush(daemon_info_t*, client_t*, uint64_t, uint64_t);
static protosw_t protocol2;

static ioblock_t*
iosetup(daemon_info_t* dp, client_t* cp, size_t space)
{
    ioblock_t* io;

    if ((io = cp->io) == NULL) {
        /*
	 * No current io buffer, get one from the free list.
	 */
	sem_wait(&cp->iofreelock);
	    if (cp->io = cp->iofree)
		cp->iofree = cp->io->next;
	    else
		cp->io = io_new_buf(dp, cp);
	sem_post(&cp->iofreelock);
	if (cp->io == NULL)
	    goto drop;
    } else if (io->off + space > io->size) {
	/*
	 * Not enough space in current buffer, push it and
	 * get another buffer from the free list.
	 */
	_proto2_flush(dp, cp, io->u.ev[1].tstamp, cp->lastevt);
	if (cp->io == NULL)
	    goto drop;
    }
    return (cp->io);
drop:
    cp->kdrops++;
    IFTRACE(EVENTIO)(dp, "Drop event; %ld bytes; client %s:%u",
	space, cp->host, cp->port);
    return (NULL);
}

#define	IOSETUP(_dp, _cp, _io, _nev) {					\
    size_t cc = (_nev) * sizeof (tstamp_event_entry_t);			\
    if ((_io = _cp->io) == NULL || _io->off + cc > _io->size) {		\
	if ((_io = iosetup(_dp, _cp, cc)) == NULL)			\
	    return;							\
    }									\
    _cp->kevents++;							\
}

/*
 * Send initial configuration info event.
 */
static void
_proto2_write_header_info(daemon_info_t* dp, client_t* cp, uint64_t eventmask)
{
    tstamp_config_event_t config;
    struct utsname uts;
    ioblock_t* io;

    config.evt = TSTAMP_EV_CONFIG;
    config.cpu = dp->cpu;
    config.jumbocnt = 0;
    config.tstamp = readcc();

    config.revision = TSTAMP_REVISION;
    config.cputype = dp->cpu_type;
    config.cpufreq = 0;			/* XXX not sure how to get this */
    config.eventmask = eventmask;
					/* ticks per sec */
    config.tstampfreq = (__uint32_t)(NSEC_PER_SEC/(cc.cycleval/1000));
    /* XXX sigh, there's gotta be a better way... */
    (void) uname(&uts);
    config.kabi = strcmp(uts.sysname, "IRIX") == 0 ?
	ABI_IRIX5_N32 : ABI_IRIX5_64;
    (void) write(cp->com->fd, &config, sizeof (config));
    /*
     * Allocate the first buffer here so that when event
     * collection begins any idle CPU's will get null
     * event records sent (see tstamp.c).
     */
    IOSETUP(dp, cp, io, 1);		/* allocate first buffer */
}

/*
 * Copy a user/kernel event into the data stream.
 */
static void
_proto2_enter_event(daemon_info_t* dp, client_t* cp, const tstamp_event_entry_t* ev)
{
    ioblock_t* io;

    IOSETUP(dp, cp, io, 1+ev->jumbocnt);
    WRITE_BUF_ALIGNED(io, tstamp_event_entry_t, *ev);
    if (ev->jumbocnt) {
	int n = ev->jumbocnt;
	do {
	    WRITE_BUF_ALIGNED(io, tstamp_event_entry_t, *++ev);
	} while (--n);
    }
}

/*
 * Mark event stream to identify lost events.
 */
static void
_proto2_lost_event(daemon_info_t* dp, client_t* cp, __int64_t tv, int lost_events)
{
    ioblock_t* io;
    tstamp_event_entry_t ev;
    
    IOSETUP(dp, cp, io, 1);
    ev.evt = TSTAMP_EV_LOST_TSTAMP;
    ev.cpu = dp->cpu;
    ev.jumbocnt = 0;
    ev.tstamp = tv;
    ev.qual[0] = lost_events;

    WRITE_BUF_ALIGNED(io, tstamp_event_entry_t, ev);
}

/*
 * Send task name event.
 */
static void
_proto2_write_task_name(daemon_info_t* dp, client_t* cp, int64_t kid, int64_t tv)
{
    tstamp_taskname_event_t ev;
    size_t name_size = sizeof (ev.name);
    ioblock_t* io;
    pid_t pid;

    IOSETUP(dp, cp, io, 1);
    kid_lookup_kid(dp, kid, &pid, (char*) ev.name, &name_size);
    IFTRACE(KID)(dp, "kid %lld has name %s and len %d", kid, ev.name, name_size);
    ev.evt = TSTAMP_EV_TASKNAME;
    ev.cpu = dp->cpu;
    ev.tstamp = tv;
    ev.k_id = kid;
    ev.pid = pid;
    ev.jumbocnt = 0;
    WRITE_BUF_ALIGNED(io, tstamp_taskname_event_t, ev);
}

void
_proto2_init_client(daemon_info_t* dp, client_t* cp)
{
    cp->lastevt = readcc();				/* avoid initial sync */
    cp->lowmark = sizeof (tstamp_event_entry_t);
}

void
_proto2_init_ioblock(daemon_info_t* dp, ioblock_t* io)
{
    io->u.ev[0].evt = TSTAMP_EV_SORECORD;
    io->u.ev[0].cpu = dp->cpu;
    io->u.ev[0].jumbocnt = 0;
}

#ifdef notdef
static void
checkevts(daemon_info_t* dp, const tstamp_event_entry_t* ev, int n)
{
    uint64_t t = 0;
    while (n > 0) {
	if (ev->evt == 0)
	    Log(LOG_WARNING, dp, "zero evt: n %d", n);
	if (1+ev->jumbocnt > n)
	    Log(LOG_WARNING, dp, "bogus reserved: n %d evt %d reserved %d",
		n, ev->evt, ev->jumbocnt);
	if (ev->tstamp == 0 || ev->tstamp < t)
	    Log(LOG_WARNING, dp,
		"misordered event: evt %d tstamp %llu, previous %llu",
		ev->evt, ev->tstamp, t);
	t = ev->tstamp;
	n -= 1+ev->jumbocnt;
	ev += 1+ev->jumbocnt;
    }
}
#endif

static void
_proto2_flush(daemon_info_t* dp, client_t* cp, uint64_t tfirst, uint64_t tlast)
{
    ioblock_t* io;
    ssize_t cc;
    extern int dochecksum;

    io = cp->io;
    assert(io != NULL);
    cc = io->off - sizeof (tstamp_event_entry_t);

    /*
     * Fill in the per-write "summary event" that
     * specifies how much data follows and, optionally,
     * a checksum over the data.  The amount of
     * data and the time stamps are used by clients
     * to merge multiple per-CPU event streams.
     */
    io->u.ev[0].tstamp = tfirst;	/* time of first event */
    io->u.ev[0].qual[0] = cc;		/* bytes of data in chunk */
    io->u.ev[0].qual[1] = 0;		/* 0 =>'s no checksum */
    io->u.ev[0].qual[2] = tlast;	/* time of last event in chunk */
    if (dochecksum) {			/* calculate checksum */
	uint64_t* lp = (uint64_t*) &io->u.ev[1];
	for (; cc > 0; cc -= sizeof (*lp))
	    io->u.ev[0].qual[1] += *lp++;
    }
#ifdef notdef
    checkevts(dp, io->u.ev, cp->off / sizeof (tstamp_event_entry_t));
#endif

    IFTRACE(EVENTIO)(dp,
"Push %ld bytes to client %s:%u; cc %llu tfirst %llu tlast %llu checksum %#llx",
	(long) io->off, cp->host, cp->port,
	io->u.ev[0].qual[0], io->u.ev[0].tstamp,
	io->u.ev[0].qual[2], io->u.ev[0].qual[1]);
    io_post(dp, cp);			/* pass to push thread */
}

static protosw_t protocol2 = {
     RTMON_PROTOCOL2,
     "RTMON Native Protocol",
     _proto2_init_client,
     _proto2_init_ioblock,
     _proto2_write_header_info,
     _proto2_flush,
     _proto2_enter_event,
     _proto2_enter_event,
     _proto2_lost_event,
     _proto2_write_task_name
};

void
protocol2_init(void)
{
    registerProtocol(&protocol2);
}
