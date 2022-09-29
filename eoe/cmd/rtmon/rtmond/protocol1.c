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
 * WindView protocol support.  Event data is reformatted
 * according to the old WindView requirements.
 */
#include "rtmond.h"
#include "timer.h"

#include <time.h>
#include <sys/uio.h>
#include <sys/syssgi.h>
#include <sys/param.h>

typedef uint16_t old_event_t;

static	void _proto1_flush(daemon_info_t*, client_t*, uint64_t, uint64_t);
static protosw_t protocol1;

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
	_proto1_flush(dp, cp, 0,0);
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

#define	IOSETUP(_dp, _cp, _io, _cc) {					\
    size_t cc = _cc;							\
    if ((_io = _cp->io) == NULL || _io->off + cc > _io->size) {		\
	if ((_io = iosetup(_dp, _cp, cc)) == NULL)			\
	    return;							\
    }									\
    _cp->kevents++;							\
}

/*
 * Write the initialization information to the stream.
 * To satisfy windview we must write at least
 * EVENT_CONFIG and EVENT_BUFFER (see <sys/wrEventP.h>).
 */
static void
_proto1_write_header_info(daemon_info_t* dp, client_t* cp, uint64_t eventmask)
{
    char comm_name[256];			/* for undefined task name */
    uint32_t name_size;
    ioblock_t* io;

    (void) eventmask;
    IOSETUP(dp, cp, io, 0);
    WRITE_BUF(io, old_event_t, EVENT_CONFIG);
    WRITE_BUF(io, int32_t, WV_REV_ID|WV_EVT_PROTO_REV);
						/* ticks/sec */
    WRITE_BUF(io, int32_t, (int32_t) (NSEC_PER_SEC/(cc.cycleval/1000)));
    WRITE_BUF(io, int32_t, UINT_MAX);		/* timer value at rollover */
    WRITE_BUF(io, int32_t, TRUE);		/* auto rollover */
    WRITE_BUF(io, int32_t, 0);			/* clock rate (not used) */
    WRITE_BUF(io, int32_t, 0);			/* !context switch mode */
    WRITE_BUF(io, int32_t, dp->cpu);		/* processor number */

    WRITE_BUF(io, old_event_t, EVENT_BUFFER);
    WRITE_BUF(io, int32_t, (int32_t) dp->last_pid_cache);

#define MANUFACTURER "Silicon Graphics"
    name_size = sizeof MANUFACTURER - 1;
    WRITE_BUF(io, old_event_t, EVENT_BEGIN);
    WRITE_BUF(io, int32_t, (int32_t) dp->cpu_type);
    WRITE_BUF(io, int32_t, name_size);
    WRITE_BUF_VARIABLE(io, name_size, MANUFACTURER);
    WRITE_BUF(io, int32_t, (int32_t) dp->last_pid_cache);
    WRITE_BUF(io, int32_t, 0);			/* collection mode */
    WRITE_BUF(io, int32_t, WV_REV_ID|WV_EVT_PROTO_REV);

    strcpy(comm_name, "undefined");		/* NB: can't use literal */
    name_size = (strlen(comm_name)+1) &~ 1;	/* must be even */

    WRITE_BUF(io, old_event_t, EVENT_TASKNAME);
    WRITE_BUF(io, uint32_t, 0);			/* status */
    WRITE_BUF(io, uint32_t, 0);			/* pri */
    WRITE_BUF(io, uint32_t, 0);			/* tasklockcount */
    WRITE_BUF(io, int32_t, (int32_t) 0);	/* taskid */
    WRITE_BUF(io, uint32_t, name_size);
    WRITE_BUF_VARIABLE(io, name_size, comm_name);
    (void) write(cp->com->fd, io->u.buf, io->off);
    io->off = 0;
}

static void
sync_event(daemon_info_t* dp, client_t* cp, __int64_t tstamp)
{
    ioblock_t* io;
    
    IOSETUP(dp, cp, io, sizeof (old_event_t) + 2*sizeof (uint32_t));
    WRITE_BUF(io, old_event_t, EVENT_TIMER_SYNC);
    WRITE_BUF(io, uint32_t, tstamp);
    WRITE_BUF(io, uint32_t, tstamp >> 32);
}

#define	RTMON_UNPACKPRI(v, p, bp) { p = v&0xffff; bp = (v>>16)&0xffff; }
#define	MAX_EVSIZE \
    (sizeof (old_event_t) + 3*sizeof (uint32_t) + \
     TSTAMP_NUM_QUALS*sizeof (uint64_t))

static void
_proto1_kernel_event(daemon_info_t* dp, client_t* cp, const tstamp_event_entry_t* ke)
{
    event_t evt = ke->evt;
    ioblock_t* io;
    int k;

    if (ke->tstamp - cp->lastevt > UINT_MAX) {
	if (cp->lastevt)
	    IFTRACE(SYNC)(dp, "Send sync due to time wrap (kernel event)");
	sync_event(dp, cp, ke->tstamp);
    }
    IOSETUP(dp, cp, io, MAX_EVSIZE);
    if (IS_USER_EVENT(evt)) {
	/*
	 * The kernel queue events mapped to user events.  Currently the list
	 * includes:
	 *
	 *     TSTAMP_EV_EODISP, TSTAMP_EV_YIELD2, TSTAMP_EV_XRUN,
	 *     TSTAMP_EV_INTRQUAL, UNDEFINED_TSTAMP_SYS
	 */
	WRITE_BUF(io, old_event_t, evt);
	WRITE_BUF(io, uint32_t, ke->tstamp);
	WRITE_BUF(io, uint32_t, NULL);
	WRITE_BUF(io, uint32_t, TSTAMP_NUM_QUALS * sizeof(uint64_t));
	for (k = 0; k < TSTAMP_NUM_QUALS; k++)
	    WRITE_BUF(io, uint64_t, ke->qual[k]);
    } else if (IS_INT_ENT_EVENT(evt)) {
	WRITE_BUF(io, int16_t, evt);
	WRITE_BUF(io, uint32_t, ke->tstamp);
    } else {
	switch (evt) {
	case TSTAMP_EV_INTREXIT:
#if TSTAMP_EV_EVINTREXIT != TSTAMP_EV_INTREXIT
	case TSTAMP_EV_EVINTREXIT:		/* XXX obsolete tag */
#endif
	case EVENT_WIND_EXIT_IDLE:
	    WRITE_BUF(io, int16_t, evt);
	    WRITE_BUF(io, uint32_t, ke->tstamp);
	    break;
	case TSTAMP_EV_EOSWITCH: {		/* check for rt threads */
	    int pri, basepri;
	    RTMON_UNPACKPRI(ke->qual[1], pri, basepri);
	    WRITE_BUF(io, int16_t,
		basepri >= 0 ? TSTAMP_EV_EOSWITCH_RTPRI : TSTAMP_EV_EOSWITCH);
	    WRITE_BUF(io, uint32_t, ke->tstamp);
	    WRITE_BUF(io, uint32_t, ke->qual[0]);	/* pid */
	    WRITE_BUF(io, uint32_t, basepri);
	    break;
	}
	case EVENT_TASK_STATECHANGE:
	    WRITE_BUF(io, int16_t, evt);
	    WRITE_BUF(io, uint32_t, ke->tstamp);
	    WRITE_BUF(io, uint32_t, ke->qual[0]);       /* pid */
	    WRITE_BUF(io, uint32_t, WIND_PEND);		/* XXX */
	    break;
	case TSTAMP_EV_SIGRECV:
	case TSTAMP_EV_SIGSEND:
	    WRITE_BUF(io, int16_t, evt);
	    WRITE_BUF(io, uint32_t, ke->tstamp);
	    WRITE_BUF(io, uint32_t, ke->qual[0]);	/* pid */
	    WRITE_BUF(io, uint32_t, ke->qual[2]);	/* signum */
	    break;
	case TSTAMP_EV_EXIT:
	    WRITE_BUF(io, int16_t, evt);
	    WRITE_BUF(io, uint32_t, ke->qual[0]);
	    break;
	default:
	    WRITE_BUF(io, int16_t, UNDEFINED_TSTAMP_DAEMON);
	    WRITE_BUF(io, uint32_t, ke->tstamp);
	    WRITE_BUF(io, int32_t, NULL);
	    /*
	     * The +1 below is for the extra event at the end which is
	     * the real event number
	     */
	    WRITE_BUF(io, int32_t, (TSTAMP_NUM_QUALS + 1) * sizeof(uint64_t));
	    for (k = 0; k < TSTAMP_NUM_QUALS; k++)
		WRITE_BUF(io, uint64_t, ke->qual[k]);
	    WRITE_BUF(io, uint64_t, evt);
	    break;
	}
    }
}
#undef MAX_EVSIZE

static void
_proto1_user_event(daemon_info_t* dp, client_t* cp, const tstamp_event_entry_t* ue)
{
    int k, nquals = ue->qual[0];
    ioblock_t* io;

    if (ue->tstamp - cp->lastevt > UINT_MAX) {
	if (cp->lastevt)
	    IFTRACE(SYNC)(dp, "Send sync due to time wrap (user event)");
	sync_event(dp, cp, ue->tstamp);
    }
    /*
     * A user event is of the form:
     * 	short USER_EVENT;
     *	int timestamp;
     *	int spare;
     *	int nBytes;
     *	char buffer[nBytes];
     */
    IOSETUP(dp, cp, io, sizeof (int16_t) + (3+nquals) * sizeof (int32_t));
    WRITE_BUF(io, int16_t, ue->evt);
    WRITE_BUF(io, int32_t, ue->tstamp);
    WRITE_BUF(io, int32_t, NULL);
    WRITE_BUF(io, int32_t, (nquals-1) * sizeof(int32_t));

    for (k = 1; k < nquals; k++)
	WRITE_BUF(io, int32_t, ue->qual[k]);
}

static void
_proto1_lost_event(daemon_info_t* dp, client_t* cp, __int64_t log_time, int lost_events)
{
    ioblock_t* io;
    
    IOSETUP(dp, cp, io, sizeof (old_event_t) + 5*sizeof (int));
    WRITE_BUF(io, old_event_t, TSTAMP_EV_LOST_TSTAMP);
    WRITE_BUF(io, int, (int) log_time);
    WRITE_BUF(io, int, NULL);				/* spare */
    WRITE_BUF(io, int, 2*sizeof (int));			/* buffer size */
    WRITE_BUF(io, int, 2);				/* # of quals */
    WRITE_BUF(io, int, lost_events);
}

static void
_proto1_write_task_name(daemon_info_t* dp, client_t* cp, pid_t pid, uint64_t tv)
{
#define	TASKNAMESIZE \
    (sizeof (old_event_t) + 4*sizeof (uint32_t) + sizeof (int32_t) + name_size)
    char comm_name[256];
    size_t ns = sizeof (comm_name);
    uint32_t name_size;
    ioblock_t* io;

    (void) tv;
    pid_lookup_pid(dp, pid, comm_name, &ns);
    sprintf(comm_name+ns, "(%ld)", (long) pid);		/* tack on pid */
    name_size = (strlen(comm_name)+1) &~ 1;		/* must be even */

    IOSETUP(dp, cp, io, TASKNAMESIZE);
    WRITE_BUF(io, old_event_t, EVENT_TASKNAME);
    WRITE_BUF(io, uint32_t, 0);			/* status */
    WRITE_BUF(io, uint32_t, 0);			/* pri */
    WRITE_BUF(io, uint32_t, 0);			/* tasklockcount */
    WRITE_BUF(io, int32_t, (int32_t) pid);	/* taskid */
    WRITE_BUF(io, uint32_t, name_size);
    WRITE_BUF_VARIABLE(io, name_size, comm_name);
#undef TASKNAMESIZE
}

void
_proto1_init_client(daemon_info_t* dp, client_t* cp)
{
    (void) dp;
    cp->lowmark = 0;
}

void
_proto1_init_ioblock(daemon_info_t* dp, ioblock_t* io)
{
    (void) dp; (void) io;
}

static void
_proto1_flush(daemon_info_t* dp, client_t* cp, uint64_t tfirst, uint64_t tlast)
{
    ssize_t cc;
    ioblock_t* io = cp->io;

    (void) tfirst; (void) tlast;
    if (io != NULL && io->off != 0) {
	IFTRACE(EVENTIO)(dp, "Push %ld bytes to Client %s:%u",
	    (long) io->off, cp->host, cp->port);
	io_post(dp, cp);
    }
}

static protosw_t protocol1 = {
     RTMON_PROTOCOL1,
     "WindView Protocol",
     _proto1_init_client,
     _proto1_init_ioblock,
     _proto1_write_header_info,
     _proto1_flush,
     _proto1_kernel_event,
     _proto1_user_event,
     _proto1_lost_event,
     _proto1_write_task_name
};

void
protocol1_init(void)
{
    registerProtocol(&protocol1);
}
