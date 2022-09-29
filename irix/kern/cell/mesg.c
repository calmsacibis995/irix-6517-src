/**************************************************************************
 *									  *
 * 		 Copyright (C) 1995-1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ident $Id: mesg.c,v 1.100 1997/10/11 22:50:19 leedom Exp $

#include <sys/types.h>
#include <sys/atomic_ops.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <sys/idbgentry.h>
#include <sys/ktrace.h>
#include <sys/param.h>
#include <sys/pda.h>
#include <sys/runq.h>
#include <sys/sema.h>
#include <sys/systm.h>
#include <sys/ksa.h>
#include <sys/cmn_err.h>
#include <sys/kopt.h>

#include <ksys/cell.h>
#include <ksys/cell/mesg.h>
#include <ksys/cell/service.h>
#include <ksys/sthread.h>
#include <ksys/cell/subsysid.h>
#include <ksys/cell/membership.h>
#include <ksys/kqueue.h>
#include <ksys/cell/subsysid.h>
#include <ksys/cell/remote.h>
#include <ksys/partition.h>
#include <sys/alenlist.h>
#include <sys/cellkernstats.h>

#include "mesg_private.h"

typedef struct {
	int			ms_size;
	int			ms_cur;
	service_t		ms_stack[1];
} mesg_stack_t;

#ifdef DEBUG
static char	*chanstate[] = {
	"INVALID",
	"READY",
	"FROZEN",
	"CLOSED"		
};
#endif

#define NMESG_CHANNELS	64
#define CELL_HASH(c)	((c)%NMESG_CHANNELS)

kqueue_t	mesg_channels[NMESG_CHANNELS];
lock_t		mesg_channel_lock;
sv_t		mesg_channel_wait;

#define	MESG_TYPE_NONE		0
#define	MESG_TYPE_RPC		1
#define	MESG_TYPE_SEND		2
#define	MESG_TYPE_REPLY		3
#define	MESG_TYPE_CALLBACK	4
#define MESG_TYPE_CELLDOWN	5

idl_message_handler_t	mesg_handlers[MAX_SUBSYSID];

#ifdef DEBUG
struct ktrace *mesg_trace;	/* tracing buffer header */
__uint64_t mesg_trace_mask = -1L;	/* mask of subsystems */
__uint64_t mesg_id;
#endif

struct mesgstat *mesgstat[MAX_SUBSYSID + 1];
struct mesgstat *mesgstat_pool;
int mesgstat_index;
lock_t mesgstat_lock;
time_t mesgstat_start;  

#define MAX_MESGSTAT	400

/*
 * Setup table for mesglist debug command. The function lists are generated
 * by kernidl but the table enumerating all the lists is built here.
 */

#define SUBSYSID_FOR_NONE(a, b)
#define SUBSYSID_FOR_CELL(a, b)		extern void *b##_msg_list[];
#if CELL_IRIX
#define SUBSYSID_FOR_CELL_IRIX(a, b)	extern void *b##_msg_list[];
#else
#define SUBSYSID_FOR_CELL_IRIX(a, b)	
#endif 
#if CELL_ARRAY
#define SUBSYSID_FOR_CELL_ARRAY(a, b)	extern void *b##_msg_list[];
#else
#define SUBSYSID_FOR_CELL_ARRAY(a, b)	
#endif 
#if DEBUG
#define	SUBSYSID_FOR_CELL_DEBUG(a,b) 	extern void *b##_msg_list[];
#else
#define	SUBSYSID_FOR_CELL_DEBUG(a,b) 
#endif


SUBSYSID_LIST()

#undef SUBSYSID_FOR_NONE
#undef SUBSYSID_FOR_CELL
#undef SUBSYSID_FOR_CELL_IRIX
#undef SUBSYSID_FOR_CELL_ARRAY
#undef SUBSYSID_FOR_CELL_DEBUG

#define SUBSYSID_FOR_NONE(a, b)         0,
#define SUBSYSID_FOR_CELL(a, b)		b##_msg_list,
#if CELL_IRIX
#define SUBSYSID_FOR_CELL_IRIX(a, b)	b##_msg_list,
#else
#define SUBSYSID_FOR_CELL_IRIX(a, b)	0,
#endif 
#if CELL_ARRAY
#define SUBSYSID_FOR_CELL_ARRAY(a, b)	b##_msg_list,
#else
#define SUBSYSID_FOR_CELL_ARRAY(a, b)	0,
#endif 
#if DEBUG
#define	SUBSYSID_FOR_CELL_DEBUG(a,b) 	b##_msg_list,
#else
#define	SUBSYSID_FOR_CELL_DEBUG(a,b) 	0,
#endif

void **mesg_func_list[MAX_SUBSYSID + 2] = {
	0,			/*  0 */
	SUBSYSID_LIST()
	0
};

#undef SUBSYSID_FOR_NONE
#undef SUBSYSID_FOR_CELL
#undef SUBSYSID_FOR_CELL_IRIX
#undef SUBSYSID_FOR_CELL_ARRAY
#undef SUBSYSID_FOR_CELL_DEBUG

#ifdef DEBUG
void idbg_mesgdump(__psint_t);
void idbg_mesgchan(__psint_t);
#endif

/*
 * Message size statistics 
 */
#ifndef DEBUG
#define	MESGSIZE_STATS_COLLECT(m)
#else

static void idbg_mesgsize(__psint_t);
static void idbg_mesgsizetrace(__psint_t);
static void mesgsize_stats_collect(idl_msg_t *);

#define MESGSIZE_STATS_COLLECT(m)	mesgsize_stats_collect(m)
#define MESG_MAXSZ	0xffffffffffffffff

#define MAX_IDL_OPCODE	100

#define	MSZ_TBUF_SCOUNT	0
#define	MSZ_TBUF_SSIZE	1
#define	MSZ_TBUF_RCOUNT	2
#define	MSZ_TBUF_RSIZE	3
typedef	size_t	msz_tbuf_t[MAX_SUBSYSID][MAX_IDL_OPCODE][4];

typedef struct msgsz_stat {
	size_t	msz_hilimit;
	size_t	msz_count;
	msz_tbuf_t	*msz_trace;
} msgsz_stat_t;

mutex_t	msgsz_trace;

msgsz_stat_t msgsz_total[] = {
/*	hilimit		count	trace buffer */
	0x40,		0,	NULL,	/* 0-64 bytes */
	0x80,		0,	NULL,	/* 64-128 bytes */
	0xa0,		0,	NULL,	/* 128-160 bytes */
	0xc0,		0,	NULL,	/* 160-192 bytes */
	0x100,		0,	NULL,	/* 192-256 bytes */
	0x200,		0,	NULL,	/* 256-512 bytes */
	0x400,		0,	NULL,	/* 512-1K bytes */
	0x800,		0,	NULL,	/* 1K-2K bytes */
	0x1000,		0,	NULL,	/* 2K-4K bytes */
	0x2000,		0,	NULL,	/* 4K-8K bytes */
	MESG_MAXSZ,	0,	NULL	/* >8K bytes */
};

msgsz_stat_t msgsz_inline[] = {
/*	hilimit		count	trace buffer */
	0x20,		0,	NULL,	/* 0-32 bytes */
	0x40,		0,	NULL,	/* 32-64 bytes */
	0x80,		0,	NULL,	/* 64-128 bytes */
	0x100,		0,	NULL,	/* 128-256 bytes */
	0x120,		0,	NULL,	/* 256-288 byes */
	0x140,		0,	NULL,	/* 288-384 byes */
	0x180,		0,	NULL,	/* 384-352 byes */
	0x200,		0,	NULL,	/* 352-512 bytes */
	MESG_MAXSZ,	0,	NULL	/* >512 bytes */
};

msgsz_stat_t msgsz_outofline[] = {
/*	hilimit		count	trace buffer */
	0x20,		0,	NULL,	/* 0-32 bytes */
	0x40,		0,	NULL,	/* 32-64 bytes */
	0x80,		0,	NULL,	/* 64-128 bytes */
	0x100,		0,	NULL,	/* 128-256 bytes */
	0x200,		0,	NULL,	/* 256-512 bytes */
	0x400,		0,	NULL,	/* 512-1K bytes */
	0x800,		0,	NULL,	/* 1K-2K bytes */
	0x1000,		0,	NULL,	/* 2K-4K bytes */
	0x2000,		0,	NULL,	/* 4K-8K bytes */
	MESG_MAXSZ,	0,	NULL	/* >8K bytes */
};

msgsz_stat_t msgsz_outoflinecnt[] = {
/*	hilimit		count	trace buffer */
	0,		0,	NULL,	/* No ool data */
	1,		0,	NULL,	/* 1 buffer */
	2,		0,	NULL,	/* 2 buffers */
	3,		0,	NULL,	/* 3 buffers */
	4,		0,	NULL,	/* 4 buffers */
	5,		0,	NULL,	/* 5 buffers */
	MESG_MAXSZ,	0,	NULL	/* >5 buffers */
};

#endif /* DEBUG */

uint
mesg_reply_id_alloc(
	mesg_channel_t	channel,
	reply_info_t *rinfop)
{
	uint		id;
	reply_ent_t	*reply_ent;
	reply_t		*reply;
	chaninfo_t	*chaninfo;

	chaninfo = (chaninfo_t *)channel;

	mutex_lock(&chaninfo->ci_lock, PZERO);
	id = chaninfo->ci_nextid++;
	if (chaninfo->ci_nextid == MESG_REPLY_ID_MAX)
		chaninfo->ci_nextid = 1;
	reply_ent = &chaninfo->ci_reply_tab[id%MESG_REPLY_ID_TABSZ];
	if (reply_ent->rt_free) {
		reply = &reply_ent->rt_reply;
	} else {
		creply_t	*creply;

		if (!reply_ent->rt_chain) {
			/*
			 * Demote head guy
			 */
			creply = (creply_t *)kmem_alloc(sizeof(creply_t), KM_SLEEP);
			ASSERT(creply);
			creply->cr_reply = reply_ent->rt_reply;
			creply->cr_next = NULL;
			reply_ent->rt_chain = 1;
			reply_ent->rt_list = creply;
			reply_ent->rt_reply.re_channel = NULL;
		}
		creply = (creply_t *)kmem_alloc(sizeof(creply_t), KM_SLEEP);
		ASSERT(creply);
		creply->cr_next = reply_ent->rt_list;
		reply_ent->rt_list = creply;
		reply = &creply->cr_reply;
	}
	reply->re_free = 0;
	reply->re_id = id;
	reply->re_callback = 0;
	reply->re_mesg = NULL;
	reply->re_channel = channel;
	reply->re_info = rinfop;
	reply->re_kthread = curthreadp;
	mutex_unlock(&chaninfo->ci_lock);
	return(id);
}

int
mesg_reply_id_lookup(
	reply_ent_t	*reply_ent,
	uint		id,
	idl_msg_t	**msgp,
	mesg_channel_t	*chanp)
{
	reply_t		*reply;
	creply_t	*creply;
	creply_t	**nuke;

	if (!reply_ent->rt_chain) {
		if (reply_ent->rt_free)
			panic("mesg_reply_id_lookup: entry free");
		reply = &reply_ent->rt_reply;
		if (reply->re_id != id)
			panic("mesg_reply_id_lookup: entry id different");
		nuke = NULL;
	} else {
		reply = NULL;
		nuke = &reply_ent->rt_list;
		for (creply = reply_ent->rt_list;
		     creply;
		     creply = creply->cr_next) {
			if (creply->cr_reply.re_id == id) {
				reply = &creply->cr_reply;
				break;
			}
			nuke = &creply->cr_next;
		}
	}
	if (reply ) {


		*msgp = reply->re_mesg;
		reply->re_mesg = NULL;
		*chanp = reply->re_channel;

		if (((chaninfo_t *)reply->re_channel)->ci_state == 
							MESG_CHAN_CLOSED) {
			if (nuke != NULL) {
				*nuke = creply->cr_next;
				kmem_free(creply, sizeof(creply_t));
				if (reply_ent->rt_list == NULL) {
					reply_ent->rt_chain = 0;
					reply_ent->rt_free = 1;
				}
			} else
				reply_ent->rt_free = 1;
			return(MESG_TYPE_CELLDOWN);
		}

		if ((*msgp) == NULL) 
			return (MESG_TYPE_NONE);

		if (reply->re_callback) {
			reply->re_callback = 0;
			return(MESG_TYPE_CALLBACK);
		} else {
			if (nuke != NULL) {
				*nuke = creply->cr_next;
				kmem_free(creply, sizeof(creply_t));
				if (reply_ent->rt_list == NULL) {
					reply_ent->rt_chain = 0;
					reply_ent->rt_free = 1;
				}
			} else
				reply_ent->rt_free = 1;
			return(MESG_TYPE_REPLY);
		}
	}
	return(MESG_TYPE_NONE);
}

void
mesg_do_callback(
	mesg_channel_t	channel,
	idl_msg_t	*idl_mesg,
	reply_info_t	*rinfop)
{
	idl_hdr_t	*idl_hdr;
	int		subsys;

	idl_hdr = IDL_MESG_HDR(idl_mesg);
	subsys = idl_hdr->hdr_subsys;
	if (subsys <= 0 || subsys > MAX_SUBSYSID) {
		printf("subsys = %d, opcode = %d\n",
			subsys, idl_hdr->hdr_opcode);
		panic("mesg_do_callback: message has unknown subsystem");
	}

	rinfop->ri_id = idl_hdr->hdr_id;
	rinfop->ri_callback = 1;
	rinfop->ri_cb_start = time;
	rinfop->ri_cb_subsys = idl_hdr->hdr_subsys;
	rinfop->ri_cb_opcode = idl_hdr->hdr_opcode;

	if (mesg_handlers[subsys-1] == NULL)
		panic("mesg_do_callback: unregistered subsystem message");
	SYSINFO.mesgrcv++;
	mesg_handlers[subsys-1](channel, idl_mesg);
	rinfop->ri_callback = 0;	/* Callback is done */
}

idl_msg_t *
mesg_reply_id_wait(
	mesg_channel_t	channel,
	uint		id,
	reply_info_t	*rinfop)
{
	reply_ent_t	*reply_ent;
	idl_msg_t	*msg;
	chaninfo_t	*chaninfo;

	chaninfo = (chaninfo_t *)channel;

	reply_ent = &chaninfo->ci_reply_tab[id%MESG_REPLY_ID_TABSZ];
	do {
		int	msg_type;

		mutex_lock(&chaninfo->ci_lock, PZERO);
		msg_type = mesg_reply_id_lookup(reply_ent, id, &msg, &channel);
		switch (msg_type) {
		case	MESG_TYPE_REPLY:
			mutex_unlock(&chaninfo->ci_lock);
			return(msg);
		case	MESG_TYPE_CALLBACK:
			mutex_unlock(&chaninfo->ci_lock);
			mesg_do_callback(channel, msg, rinfop);
			break;
		case	MESG_TYPE_NONE:
			sv_wait(&reply_ent->rt_sv, PZERO, &chaninfo->ci_lock, 0);
			break;
		case	MESG_TYPE_CELLDOWN:
			mutex_unlock(&chaninfo->ci_lock);
			return (NULL);
		default:
			panic("mesg_reply_id_wait");
		}
	} while (1);
	/* NOTREACHED */
}

/* ARGSUSED */
void
mesg_reply_id_cancel(
	mesg_channel_t	channel,
	uint		id)
{
	panic("mesg_reply_id_cancel");
}

/* ARGSUSED */
void
mesg_reply_id_wake(
	mesg_channel_t	channel,
	uint		id,
	idl_msg_t	*msg,
	int		iscallback)
{
	reply_ent_t	*reply_ent;
	reply_t		*reply;
	int		(*sv_wakeup)(sv_t *);
	chaninfo_t	*chaninfo;
#if CELL_IRIX
	idl_hdr_t	*reply_hdr = IDL_MESG_HDR(msg);
	xthread_t	*xt;
	uint64_t	cpu_accum;
#endif

	chaninfo = (chaninfo_t *)channel;

	reply_ent = &chaninfo->ci_reply_tab[id%MESG_REPLY_ID_TABSZ];
	mutex_lock(&chaninfo->ci_lock, PZERO);
	if (!reply_ent->rt_chain) {
		if (reply_ent->rt_free)
			panic("mesg_reply_id_wake: no message");
		reply = &reply_ent->rt_reply;
		if (reply->re_id != id)
			panic("mesg_reply_id_wake: bad message id");
		sv_wakeup = sv_signal;
	} else {
		creply_t	*creply;

		for (creply = reply_ent->rt_list;
		     creply;
		     creply = creply->cr_next) {
			if (creply->cr_reply.re_id == id) {
				reply = &creply->cr_reply;
				sv_wakeup = sv_broadcast;
				break;
			}
		}
		if (!creply)
			panic("mesg_reply_id_wake: no message");
	}
#ifdef DEBUG
	if (reply->re_callback) {
		cmn_err(CE_PANIC, 
			"thread 0x%x callback still in progress",
			reply->re_kthread);
	}
#endif
	reply->re_mesg = msg;
	ASSERT(reply->re_channel == channel);
	reply->re_callback = iscallback;

#if CELL_IRIX
	/* Account for cpu time used to process incomming message and
	 * add it to the remote time returned in header.
	 */
	xt = KT_TO_XT(curthreadp);
	cpu_accum = ktimer_readticks(curthreadp, AS_SYS_RUN);
	reply_hdr->hdr_rtime += cpu_accum - xt->xt_mesg_ticks;
	xt->xt_mesg_ticks = cpu_accum;
#endif
	
	sv_wakeup(&reply_ent->rt_sv);

	mutex_unlock(&chaninfo->ci_lock);
}

service_t
mesg_get_callback_svc()
{
	transinfo_t	*ti;
	service_t	svc;

	ti = KT_TO_XT(curthreadp)->xt_info;
	SERVICE_MAKE(svc, ti->ti_cell, 0);
	return(svc);
}

void
mesg_init(void)
{
	int	i;

#if DEBUG
	mesg_trace = ktrace_alloc(1000, 0);
	idbg_addfunc("mesgdump", idbg_mesgdump);
	idbg_addfunc("mesgchan", idbg_mesgchan);
	idbg_addfunc("mesgsize", idbg_mesgsize);
	idbg_addfunc("mesgsizetrace", idbg_mesgsizetrace);
	mutex_init(&msgsz_trace, MUTEX_DEFAULT, "msgsz_trace");
#endif
	mesgstat_pool = (struct mesgstat *)kmem_zalloc((MAX_MESGSTAT + 1) *
		sizeof(struct mesgstat), KM_SLEEP);
	strcpy(mesgstat_pool[MAX_MESGSTAT].name, "OTHER");
	mesgstat_start = lbolt;
	spinlock_init(&mesgstat_lock, "mesgstat_lock");
	spinlock_init(&mesg_channel_lock, "mesg_channel_lock");
	sv_init(&mesg_channel_wait, SV_DEFAULT, "mesg_channel_wait");
	for (i = 0; i < NMESG_CHANNELS; i++)
		kqueue_init(&mesg_channels[i]);
}

void
mesg_handler_register(
	idl_message_handler_t	handler,
	int			subsys)
{
	if (mesg_handlers[subsys-1] != NULL)
		panic("mesg_handler_register: double register");
	mesg_handlers[subsys-1] = handler;
	ASSERT(mesg_func_list[subsys]);
}

static __inline mesg_channel_t
mesg_channel_lookup(
	cell_t	cell,
	int	channelno,
	int	refcnt)
{
	int		spl;
	kqueue_t	*kq;
	chaninfo_t	*chaninfo;
	mesg_channel_t	*chan = NULL;

	kq = &mesg_channels[CELL_HASH(cell)];
	spl = mutex_spinlock(&mesg_channel_lock);
	for (chaninfo = (chaninfo_t *)kqueue_first(kq);
	     chaninfo != (chaninfo_t *)kqueue_end(kq);
	     chaninfo = (chaninfo_t *)kqueue_next(&chaninfo->ci_list)) {
		if (cell == chaninfo->ci_cell &&
		    channelno == chaninfo->ci_channo) {
			chan = (void *)chaninfo;
			break;
		}
	}

	if (refcnt) {	/* Check the state before getting a reference */

		while (chaninfo->ci_state == MESG_CHAN_FROZEN) {
			sv_wait(&chaninfo->ci_wait, PZERO, 
				&mesg_channel_lock, spl);
			spl = mutex_spinlock(&mesg_channel_lock);
		}

		/*
		 * Return NULL if channel is not ready.
		 */
		if (chaninfo->ci_state == MESG_CHAN_READY) 
			chaninfo->ci_refcnt += refcnt;
		else chan = NULL;
	}
	mutex_spinunlock(&mesg_channel_lock, spl);
	return(chan);
}

void
mesg_channel_rele(
	mesg_channel_t	channel)
{
	chaninfo_t	*chaninfo;
	int		spl;

	chaninfo = (chaninfo_t *)channel;
	spl = mutex_spinlock(&mesg_channel_lock);
	chaninfo->ci_refcnt--;
	if (chaninfo->ci_refcnt == 0 &&
	    chaninfo->ci_state == MESG_CHAN_CLOSED)
		sv_broadcast(&mesg_channel_wait);
	mutex_spinunlock(&mesg_channel_lock, spl);
}

/*
 * mesg_channel_set_state:
 * Set the channel to  a given state. If the old state was FROZEN then
 * wakeup message threads that have been suspended.
 */
void
mesg_channel_set_state(
	cell_t cell, 
	int channelno,
	mesg_channel_state_t state)
{
	chaninfo_t      *chaninfo = mesg_channel_lookup(cell, channelno, 0);
	int		spl;

	if (chaninfo) {
		spl = mutex_spinlock(&mesg_channel_lock);

		if (state == chaninfo->ci_state) {
			mutex_spinunlock(&mesg_channel_lock, spl);
			return;
		}

		if (chaninfo->ci_state == MESG_CHAN_FROZEN) {
			sv_broadcast(&chaninfo->ci_wait);
		}
		chaninfo->ci_state = state;
		mutex_spinunlock(&mesg_channel_lock, spl);
	}
}


/*
 * mesg_channel_suspend:
 * Wait for the channel to thaw. If after thawing the channel is in
 * ready state return 1 else 0;
 */

int
mesg_channel_suspend(
	chaninfo_t      *chaninfo)
{
	int	spl = mutex_spinlock(&mesg_channel_lock);
	int	chan_state;

	while (chaninfo->ci_state == MESG_CHAN_FROZEN) {
		sv_wait(&chaninfo->ci_wait, PZERO, &mesg_channel_lock, spl);
		spl = mutex_spinlock(&mesg_channel_lock);
	}

	chan_state = chaninfo->ci_state;
	mutex_spinunlock(&mesg_channel_lock, spl);

	return (chan_state == MESG_CHAN_READY);
}

void
mesg_demux(
	cell_t		cell,
	int		channo,
	mesg_channel_t	channel,
	idl_msg_t	*idl_mesg)
{
	int		subsys;
	transinfo_t	transinfo;
	idl_hdr_t	*idl_hdr;

	/*
	 * Wait for initial membership to be delivered before
	 * accepting messages.
	 */
        if (channo == MESG_CHANNEL_MAIN)
                cms_wait_for_initial_membership();

	channel = cell_to_channel(cell, channo);
	if (channel == NULL) {
		cmn_err(CE_WARN, "Dropping message received from "
				 "Cell %d:not in membership\n",
					cell);
		return;
	}

	ASSERT(KT_ISXTHREAD(curthreadp));

	idl_hdr = IDL_MESG_HDR(idl_mesg);

	switch (idl_hdr->hdr_type) {
	case	MESG_TYPE_RPC:
	case	MESG_TYPE_SEND:
		ASSERT(KT_TO_XT(curthreadp)->xt_info == NULL);
		KT_TO_XT(curthreadp)->xt_info = &transinfo;
		transinfo.ti_id = idl_hdr->hdr_id;
		transinfo.ti_trans = NULL;
		transinfo.ti_cred = NULL;
		transinfo.ti_channo = channo;
		transinfo.ti_cell = cell;

		/* The following are only needed by mesglist */
		transinfo.ti_subsys = idl_hdr->hdr_subsys;
		transinfo.ti_opcode = idl_hdr->hdr_opcode;
		transinfo.ti_start = time;

		subsys = idl_hdr->hdr_subsys;
		if (subsys <= 0 || subsys > MAX_SUBSYSID) {
			printf("message 0x%x subsys %d opcode %d\n",
				idl_mesg, subsys, idl_hdr->hdr_opcode);
			panic("mesg_notify: message has unknown subsystem");
		}
		if (mesg_handlers[subsys-1] == NULL)
			panic("mesg_notify: unregistered subsystem message");

		SYSINFO.mesgrcv++;
		mesg_handlers[subsys-1](channel, idl_mesg);
		KT_TO_XT(curthreadp)->xt_info = NULL;
		break;

	case	MESG_TYPE_REPLY:
		mesg_reply_id_wake(channel, idl_hdr->hdr_id, idl_mesg, 0);
		break;

	case	MESG_TYPE_CALLBACK:
		mesg_reply_id_wake(channel, idl_hdr->hdr_callbackid, idl_mesg, 1);
		break;

	default:
		panic("mesg_notify: unknown header type");
	}
	mesg_channel_rele(channel);
}

int
mesg_connect(
	cell_t	cell,
	int	channelno)
{
	int		err;
	xpchan_t	channel;
	kqueue_t	*kq;
	chaninfo_t	*chaninfo;
	int		spl;
	int		i;

	ASSERT(cell != cellid());

	/*
	 * If a channel is already opened return success.
	 */
	if (mesg_channel_lookup(cell, channelno, 0))
		return(1);

	err = mesg_xpc_connect(cell, channelno, &channel);
	if (err == EBUSY)
		return(1);

	if (err)
		return(0);

	chaninfo = kmem_alloc(sizeof(chaninfo_t), KM_SLEEP);
	ASSERT(chaninfo);
	chaninfo->ci_channo = channelno;
	chaninfo->ci_cell = cell;
	chaninfo->ci_state = MESG_CHAN_FROZEN;
	chaninfo->ci_refcnt = 0;
	chaninfo->ci_chanid = channel;
	init_sv(&chaninfo->ci_wait, SV_DEFAULT, "Channel wait", cell << 8);
	for (i = 0; i < MESG_REPLY_ID_TABSZ; i++) {
		chaninfo->ci_reply_tab[i].rt_free = 1;
		chaninfo->ci_reply_tab[i].rt_chain = 0;
		init_sv(&chaninfo->ci_reply_tab[i].rt_sv, SV_DEFAULT,
			"mesg_reply", cell << 8 || i);
	}
	chaninfo->ci_nextid = 1;
	init_mutex(&chaninfo->ci_lock, MUTEX_DEFAULT, "reply_lock", (long)cell);
	kq = &mesg_channels[CELL_HASH(cell)];
	spl = mutex_spinlock(&mesg_channel_lock);
	kqueue_enter(kq, &chaninfo->ci_list);
	mutex_spinunlock(&mesg_channel_lock, spl);
	return(1);
}

void
mesg_disconnect(
	cell_t	cell,
	int	channelno)
{
	chaninfo_t	*chaninfo;
	int		spl;
	int		i;

	ASSERT(cell != cellid());
	chaninfo = mesg_channel_lookup(cell, channelno, 0);
	if (chaninfo == NULL)
		return;
	spl = mutex_spinlock(&mesg_channel_lock);
	while (chaninfo->ci_refcnt > 0) {
		sv_wait(&mesg_channel_wait, PZERO, &mesg_channel_lock, spl);
		spl = mutex_spinlock(&mesg_channel_lock);
	}
	kqueue_remove(&chaninfo->ci_list);
	mutex_spinunlock(&mesg_channel_lock, spl);
	for (i = 0; i < MESG_REPLY_ID_TABSZ; i++)
		sv_destroy(&chaninfo->ci_reply_tab[i].rt_sv);
	mutex_destroy(&chaninfo->ci_lock);
	xpc_disconnect(chaninfo->ci_chanid);
	sv_destroy(&chaninfo->ci_wait);
	kmem_free(chaninfo, sizeof(chaninfo_t));
}
	

mesg_channel_t
cell_to_channel(
	cell_t	cell,
	int	channelno)
{
#ifdef CELL_IRIX
	if (channelno == MESG_CHANNEL_MAIN)
		cms_wait_for_initial_membership();
#endif
	if (!cell_in_membership(cell) && (channelno == MESG_CHANNEL_MAIN))
			return NULL;
	return(mesg_channel_lookup(cell, channelno, 1));
}

int
mesg_is_rpc(
	idl_msg_t	*msg)
{
	idl_hdr_t	*idl_hdr;

	idl_hdr = IDL_MESG_HDR(msg);
	return(idl_hdr->hdr_type & MESG_TYPE_RPC);
}

__inline idl_msg_t *
_mesg_rpc(
	mesg_channel_t	channel,
	idl_msg_t	*msg,
	idl_hdr_t	*idl_hdr)
{
	idl_msg_t	*reply;
	int		rc;
	uint		replyid;
	reply_info_t	rinfo;
	chaninfo_t	*chaninfo;

	SYSINFO.mesgsnt++;

	chaninfo = (chaninfo_t *)channel;

	rinfo.ri_subsys = idl_hdr->hdr_subsys;
	rinfo.ri_opcode = idl_hdr->hdr_opcode;
	rinfo.ri_start = time;
	rinfo.ri_callback = 0;

	replyid = mesg_reply_id_alloc(channel, &rinfo);
	idl_hdr->hdr_id = replyid;
#if CELL_IRIX
	idl_hdr->hdr_rtime = 0;
#endif
	MESGSIZE_STATS_COLLECT(msg);
	rc = xpc_send(chaninfo->ci_chanid, &msg->msg_xpm);

	if (rc) {
		mesg_reply_id_cancel(channel, replyid);
		return(NULL);
	}
	reply = mesg_reply_id_wait(channel, replyid, &rinfo);

#if CELL_IRIX
	if (reply) {
		/* Add remote cpu time used to our timer */
		idl_hdr = IDL_MESG_HDR(reply);
		ktimer_accum(curthreadp, AS_SYS_RUN, idl_hdr->hdr_rtime);
	}
#endif

	return(reply);
}


idl_msg_t *
mesg_callback(
	mesg_channel_t	channel,
	idl_msg_t	*msg)
{
	idl_hdr_t	*idl_hdr;
	transinfo_t	*ti;

	idl_hdr = IDL_MESG_HDR(msg);
	idl_hdr->hdr_type = MESG_TYPE_CALLBACK;
	ti = KT_TO_XT(curthreadp)->xt_info;
	idl_hdr->hdr_callbackid = ti->ti_id;
	return(_mesg_rpc(channel, msg, idl_hdr));
}

idl_msg_t *
mesg_rpc(
	mesg_channel_t	channel,
	idl_msg_t	*msg)
{
	idl_hdr_t	*idl_hdr;

	idl_hdr = IDL_MESG_HDR(msg);
	idl_hdr->hdr_type = MESG_TYPE_RPC;
	return(_mesg_rpc(channel, msg, idl_hdr));
}

int
mesg_send(
	mesg_channel_t	channel,
	idl_msg_t	*msg,
	int		sync)
{
	int		rc;
	idl_hdr_t	*idl_hdr;
	chaninfo_t	*chaninfo;

	SYSINFO.mesgsnt++;

	chaninfo = (chaninfo_t *)channel;

	idl_hdr = IDL_MESG_HDR(msg);
	idl_hdr->hdr_type = MESG_TYPE_SEND;
	if (sync)
		msg->msg_xpm.xpm_flags |= (XPM_F_SYNC|XPM_F_NOTIFY);

	MESGSIZE_STATS_COLLECT(msg);
	rc = xpc_send(chaninfo->ci_chanid, &msg->msg_xpm);
	return(rc);
}

int
mesg_reply(
	mesg_channel_t	channel,
	idl_msg_t	*reply,
	idl_msg_t	*msg,
	int		sync)
{
	int		rc;
	idl_hdr_t	*idl_hdr;
	idl_hdr_t	*reply_hdr;
	chaninfo_t	*chaninfo;
#if CELL_IRIX
	xthread_t	*xt;
	uint64_t	cpu_accum;
#endif

	chaninfo = (chaninfo_t *)channel;

	reply_hdr = IDL_MESG_HDR(reply);
	reply_hdr->hdr_type = MESG_TYPE_REPLY;

	idl_hdr = IDL_MESG_HDR(msg);
	reply_hdr->hdr_id = idl_hdr->hdr_id;

#if DEBUG
	/*
	 * This is for message size tracing
	 */
	reply_hdr->hdr_subsys = idl_hdr->hdr_subsys;
	reply_hdr->hdr_opcode = idl_hdr->hdr_opcode;
#endif

#if CELL_IRIX
	if (curuthread) {
		/* We are at the starting thread handling a callback so we get
		 * charged locally for the cpu time used.  We don't want to
		 * get charged twice so just return the time used to initiate
		 * the callback which was accumulated in the incomming message.
		 */
		ASSERT(idl_hdr->hdr_rtime);
		reply_hdr->hdr_rtime = idl_hdr->hdr_rtime;
	} else {
		/* We are an xthread processing a remote call. Return cpu time
		 * used for this message back with the reply by subtracting
		 * from the previous snaphot of the thread's time.
		 */
		xt = KT_TO_XT(curthreadp);
		cpu_accum = ktimer_readticks(curthreadp, AS_SYS_RUN);
		reply_hdr->hdr_rtime = cpu_accum -xt->xt_mesg_ticks
			+ idl_hdr->hdr_rtime;
		xt->xt_mesg_ticks = cpu_accum;
	}
#endif

	if (sync)
		reply->msg_xpm.xpm_flags |= (XPM_F_SYNC|XPM_F_NOTIFY);
	MESGSIZE_STATS_COLLECT(reply);
	rc = xpc_send(chaninfo->ci_chanid, &reply->msg_xpm);
	return(rc);
}

idl_msg_t *
mesg_allocate(
	mesg_channel_t	channel)
{
	idl_msg_t	*msg;
	chaninfo_t	*chaninfo;

	chaninfo = (chaninfo_t *)channel;

	/*
 	 * If the channel is the main channel and the dest cell is not in 
	 * the membership fail the allocation.
	 */
	if ((chaninfo->ci_channo == MESG_CHANNEL_MAIN) && 
			(!cell_in_membership(chaninfo->ci_cell))) {
		cmn_err(CE_WARN, "Failing mesg allocate cell %d "
				 "not in membership\n", chaninfo->ci_cell);
		return ((idl_msg_t *)0);

	}

	if (xpcSuccess == xpc_allocate(chaninfo->ci_chanid, (xpm_t **)&msg)) {
		msg->msg_xpm.xpm_type = XPM_T_DATA;
		msg->msg_xpm.xpm_cnt = 1;
		msg->msg_xpmb.xpmb_flags = XPMB_F_IMD;
		msg->msg_xpmb.xpmb_ptr = 0;
		return(msg);
	} 
	return(NULL);
}

void
mesg_free(
	mesg_channel_t	channel,
	idl_msg_t	*msg)
{
	mesg_xpc_free(channel, msg);
}

void
mesg_marshal_indirect(
	idl_msg_t	*msg,
	void		*addr,
	size_t		nbytes)
{
	alenlist_t	_alist = NULL;
	xpmb_t		*xpmb = NULL;
	alenaddr_t	paddr;
	size_t		cnt;

	/*
	 * Immediate data must be marshalled after all indirect data
	 */
	ASSERT(msg->msg_xpmb.xpmb_ptr == 0);

	if (nbytes == 0) {
		/*
		 * No data is to be sent, put in an empty xpmb
		 * so the other side knows.
		 */
		xpmb = XPM_BUF(&msg->msg_xpm, msg->msg_xpm.xpm_cnt);
		xpmb->xpmb_flags = 0;
		xpmb->xpmb_cnt = 0;
		msg->msg_xpm.xpm_cnt++;
		return;
	}

	_alist = kvaddr_to_alenlist(_alist, addr, nbytes, 0);
	ASSERT(_alist);

	while (alenlist_get(_alist, NULL, 0, &paddr, &cnt, 0) ==
							ALENLIST_SUCCESS) {
		/*
		 * set the chain bit in the previous buffer
		 */
		if (xpmb)
			xpmb->xpmb_flags |= XPMB_F_BC;

		xpmb = XPM_BUF(&msg->msg_xpm, msg->msg_xpm.xpm_cnt);
		xpmb->xpmb_flags = XPMB_F_PTR;
		xpmb->xpmb_ptr = paddr;
		xpmb->xpmb_cnt = cnt;

		msg->msg_xpm.xpm_cnt++;
	}

	alenlist_done(_alist);
}

void
mesg_unmarshal_indirect(
	idl_msg_t	*msg,
	int		which,
	void		**addr,
	size_t		*nbytes)
{
	xpmb_t		*xpmb = NULL;

	xpmb = XPM_BUF(&msg->msg_xpm, which);
	/*
	 * Check if the remote side sent any data
	 */
	if (xpmb->xpmb_flags == 0) {
		*nbytes = 0;
		return;
	}
	if (*nbytes != 0) {
		size_t	len;

		ASSERT(*addr != NULL);
		len = min(*nbytes, xpmb->xpmb_cnt);
		bcopy((void *)xpmb->xpmb_ptr, *addr, len);
		kmem_free((caddr_t)xpmb->xpmb_ptr, xpmb->xpmb_cnt);
		*nbytes = len;
	} else {
		*addr = (void *)xpmb->xpmb_ptr;
		*nbytes = xpmb->xpmb_cnt;
	}
	xpmb->xpmb_ptr = 0;
}

int
mesg_indirect_count(
	idl_msg_t	*msg)
{
	return(msg->msg_xpm.xpm_cnt - 1);
}

void *
mesg_get_immediate_ptr(
	idl_msg_t	*msg,
	size_t		nbytes)
{

	void	*ptr;

	ptr = (void *)XPM_BUF(&msg->msg_xpm, msg->msg_xpm.xpm_cnt);
	msg->msg_xpmb.xpmb_cnt = sizeof(idl_hdr_t) + nbytes;
	msg->msg_xpmb.xpmb_ptr = (paddr_t)((caddr_t)ptr - (caddr_t)msg);
	if ((MESG_MAX_SIZE - msg->msg_xpmb.xpmb_ptr) < nbytes)
		return(NULL);
	return(ptr);

}

trans_t *
mesg_thread_info()
{
	transinfo_t	*ti;

	if (curthreadp && KT_CUR_ISXTHREAD()) {
		ti = (transinfo_t *)(KT_TO_XT(curthreadp)->xt_info);
		if (ti)
			return(ti->ti_trans);
	}
	return(NULL);
}

#define MESG_STACK_INCR	5

void
mesg_svc_push(
	kthread_t	*kt)
{
	mesg_stack_t	*mesg_stack;
	size_t		stacksize;

	if (kt->k_mesg_stack == NULL) {

		stacksize = (size_t)(sizeof(mesg_stack_t) +
				((MESG_STACK_INCR - 1) * sizeof(service_t)));
		mesg_stack = kmem_alloc(stacksize, KM_SLEEP);
		kt->k_mesg_stack = mesg_stack;
		mesg_stack->ms_size = MESG_STACK_INCR;
		mesg_stack->ms_cur = 0;
	} else {
		size_t	ostacksize;
		int	nentries;
		int	onentries;

		mesg_stack = kt->k_mesg_stack;
		if (mesg_stack->ms_cur == mesg_stack->ms_size) {
			onentries = mesg_stack->ms_size;
			nentries = onentries + MESG_STACK_INCR;
			stacksize = (size_t)(sizeof(mesg_stack_t) +
				((nentries - 1) * sizeof(service_t)));
			mesg_stack = kmem_alloc(stacksize, KM_SLEEP);
			ostacksize = sizeof(mesg_stack_t) +
				((onentries - 1) * sizeof(service_t));
			bcopy(kt->k_mesg_stack, mesg_stack, ostacksize);
			kmem_free(kt->k_mesg_stack, ostacksize);
			kt->k_mesg_stack = mesg_stack;
			mesg_stack->ms_size = nentries;
		}
	}
	mesg_stack->ms_stack[mesg_stack->ms_cur] = kt->k_remote_svc;
	mesg_stack->ms_cur++;
}

void
mesg_svc_pop(
	kthread_t	*kt)
{
	mesg_stack_t	*mesg_stack;
	size_t		stacksize;
	int		nentries;

	ASSERT(kt->k_mesg_stack != NULL);
	mesg_stack = kt->k_mesg_stack;
	mesg_stack->ms_cur--;
	kt->k_remote_svc = mesg_stack->ms_stack[mesg_stack->ms_cur];
	SERVICE_MAKE_NULL(mesg_stack->ms_stack[mesg_stack->ms_cur]);
	if (mesg_stack->ms_cur == 0) {
		nentries = mesg_stack->ms_size;
		stacksize = sizeof(mesg_stack_t) +
			((nentries - 1) * sizeof(service_t));
		kmem_free(mesg_stack, stacksize);
		kt->k_mesg_stack = NULL;
	}
}

static char *
mthrd_time(
	time_t	stime,
	char	*stime_str)
{

#define SECS_PER_HR	3600
#define SECS_PER_MIN	60

	uint hrs, mins, secs, s_tens, s_ones, m_tens, m_ones;

	secs = time - stime;
	hrs = secs / SECS_PER_HR;
	secs -= (hrs * SECS_PER_HR);
	mins = secs / SECS_PER_MIN;
	secs -= (mins * SECS_PER_MIN);
	s_ones = secs % 10;
	s_tens = secs / 10;
	

	if (hrs) {
		m_ones = mins % 10;
		m_tens = mins / 10;
		sprintf(stime_str, "%d:%d%d:%d%d", hrs, m_tens, m_ones,
			 s_tens, s_ones);
	}
	else if (mins) {
		sprintf(stime_str, "%d:%d%d", mins, s_tens, s_ones);
	}
	else {
		sprintf(stime_str, "%d", secs);
	}
	return(stime_str);
}

void
print_mesg_title(void)
{
	qprintf("Thread Addr         MSG ID Type Cell Message                          Time(Secs)\n");
	qprintf("------------------ ------- ---- ---- -------------------------------- ----------\n");
}

#define PRINT_MTHRD(t, msgid, ctype, cell, subsys, opcode, stime)	\
	 	qprintf("0x%x %7x %4s %4d %32s %10s\n", t, msgid, ctype, cell,\
			fetch_kname(mesg_func_list[subsys][opcode], &offset), \
			 mthrd_time(stime, stime_str))

#define PRINT_MTHRD_DECL	__psunsigned_t offset; char stime_str[16];


void
dump_reply(
	cell_t		cell,
	reply_t		*reply)
{
	reply_info_t	rinfo;
	reply_info_t	*rinfop;
	PRINT_MTHRD_DECL

	if (KT_ISUTHREAD(reply->re_kthread)) {
		idbg_get_stack_bytes(KT_TO_UT(reply->re_kthread),
			reply->re_info, &rinfo, sizeof(rinfo));
		rinfop = &rinfo;
	} else
		rinfop = reply->re_info;
	PRINT_MTHRD(reply->re_kthread, reply->re_id, "Snt",
		cell, rinfop->ri_subsys,
		rinfop->ri_opcode, rinfop->ri_start);
	if (rinfop->ri_callback) {
		PRINT_MTHRD(reply->re_kthread,
			rinfop->ri_id, "Cbk",
			cell,
			rinfop->ri_cb_subsys,
			rinfop->ri_cb_opcode,
			rinfop->ri_cb_start);
	}
}

void
dump_reply_tab(
	cell_t		cell,
	reply_ent_t	*reply_tab)
{
	int		id;
	reply_ent_t	*reply_ent;
	reply_t		*reply;
	creply_t	*creply;

	for (id = 0; id < MESG_REPLY_ID_TABSZ; id++) {
		reply_ent = &reply_tab[id];
		if (reply_ent->rt_free)
			continue;

		if (!reply_ent->rt_chain) {
			reply = &reply_ent->rt_reply;
			dump_reply(cell, reply);
			continue;
		}

		creply = reply_ent->rt_list;
		while (creply) {
			reply = &creply->cr_reply;
			dump_reply(cell, reply);
			creply = creply->cr_next;
		}
	}
}

/*
 * mesg_cleanup:
 * Wake up all threads waiting for reply from the failed cell.
 */
void
mesg_cleanup(cell_t cell, int channelno)
{
	chaninfo_t	*chan;
	int		id;
	reply_ent_t	*reply_ent;

	extern xthread_t xthreadlist;
	xthread_t       *xt;
	transinfo_t     *ti;


	chan = (chaninfo_t *)mesg_channel_lookup(cell, channelno, 1);

	/*
	 * The channel can be freed by part_deactivate calling cell_down.
	 * Of course if there any messages it will wait for the ref count
	 * to go to zero. If chan is zero that means there were no message
	 * threads on that channel.
	 */
	if (!chan)
		return;

	mutex_lock(&chan->ci_lock, PZERO);
	for (id = 0; id < MESG_REPLY_ID_TABSZ; id++) {
		reply_ent = &chan->ci_reply_tab[id];
		if (reply_ent->rt_free)
			continue;

		if (!reply_ent->rt_chain) {
			sv_signal(&reply_ent->rt_sv);
		} else {
			sv_broadcast(&reply_ent->rt_sv);
		}
	}
	mutex_unlock(&chan->ci_lock);

	/*
	 * Interrupt all message threads that are  in long term wait.
	 */
	for (xt = xthreadlist.xt_next; xt != &xthreadlist; xt = xt->xt_next) {
		if (!xt->xt_info) {
		
			/* Not a message thread */
			continue;
		}
		ti = (transinfo_t *)xt->xt_info;
		if (ti->ti_cell == cell &&
		    ti->ti_channo == MESG_CHANNEL_MAIN) {
			int	spl;

			spl = kt_lock(XT_TO_KT(xt));
			thread_interrupt(XT_TO_KT(xt), &spl);
			kt_unlock(XT_TO_KT(xt), spl);
		}
	}
	mesg_channel_rele(chan);
}

/*
 * Display messages in progress based on what is in reply_tab and the
 * existence of xthreads handling messages.
 */
/* ARGSUSED */
void
idbg_mesglist(
	__psint_t	x,
	__psint_t	a2,
	int		argc,
	char		**argv)
{
	extern xthread_t xthreadlist;
	xthread_t	*xt;
	transinfo_t	*ti;
	int		i;
	kqueue_t	*kq;
	chaninfo_t	*chaninfo;
	reply_ent_t	*reply_tab;
	PRINT_MTHRD_DECL

	qprintf("Cell:%d\n", cellid());
	print_mesg_title();

	/* First scan the xthreads looking for message threads */

	for (xt = xthreadlist.xt_next; xt != &xthreadlist; xt = xt->xt_next) {
		if (!xt->xt_info) {
		
			/* Not a message thread */

			continue;
		}
		ti = (transinfo_t *)xt->xt_info;
		PRINT_MTHRD(xt, ti->ti_id, "Rcv",  ti->ti_cell, ti->ti_subsys,
			ti->ti_opcode, ti->ti_start);
	}

	/* Now check the reply table looking for threads waiting for replies
	 * or processing callbacks.
	 */

	for (i = 0; i < NMESG_CHANNELS; i++) {
		kq = &mesg_channels[i];
		for (chaninfo = (chaninfo_t *)kqueue_first(kq);
		     chaninfo != (chaninfo_t *)kqueue_end(kq);
		     chaninfo = (chaninfo_t *)kqueue_next(&chaninfo->ci_list)) {
			reply_tab = chaninfo->ci_reply_tab;
			dump_reply_tab(chaninfo->ci_cell, reply_tab);
		}
	}
}

#ifdef DEBUG
#define SUBSYSID_FOR_NONE(a, b)		#b, 
#define SUBSYSID_FOR_CELL(a, b)		#b, 
#define SUBSYSID_FOR_CELL_IRIX(a, b)	#b, 
#define SUBSYSID_FOR_CELL_ARRAY(a, b)	#b, 
#define	SUBSYSID_FOR_CELL_DEBUG(a, b)	#b,

static char *subsys_names[MAX_SUBSYSID + 2] = {
	"UNK",				/*  0 */
	SUBSYSID_LIST()
	0
};
static int nsubsys_names = MAX_SUBSYSID + 1;

#undef SUBSYSID_FOR_NONE
#undef SUBSYSID_FOR_CELL
#undef SUBSYSID_FOR_CELL_IRIX
#undef SUBSYSID_FOR_CELL_ARRAY
#undef SUBSYSID_FOR_CELL_DEBUG

/*
 * Print message contents
 * arg0 is subsystem id
 * arg1 is unique message id
 * arg2 is service_t (dest cell)
 * arg3 is src cell (-1 for response messages)
 * arg4 is function address
 */
static void
prmesg(ktrace_entry_t *ktep)
{
	service_t svc;
	int i, hiarg;

	/* print message id */
	qprintf("<%d> ", ktep->val[1]);

	if (ktep->val[3] == (void *)-1L) {
		qprintf("RESP ");
	} else {
		SERVICE_FROMWP_VALUE(svc, (int)(__psint_t)ktep->val[2]);
		qprintf("%d->%d ",
			ktep->val[3],
			SERVICE_TO_CELL(svc));
	}
	qprintf("%s ", subsys_names[(__psint_t)ktep->val[0]]);
	prsymoff(ktep->val[4], NULL, NULL);

	/* try to compute # or args */
	for (i = 5, hiarg = 4; i < 16; i++) {
		if (ktep->val[i] != (void *)0xdeadL)
			hiarg = i;
	}
	qprintf("(");
	for (i = 5; i <= hiarg; i++)
		qprintf("0x%x ", ktep->val[i]);
	qprintf(")\n");
}

/*
 * The first argument is # of messages to examine
 * After that the arguments should be the string names of subsystems to display
 * So, for example, mesgtrace 10 will print the last 10 messages
 * mesgtrace 10 DCSHM will examine the last 10 messages for DCSHM messages.
 */
/* ARGSUSED */
void
idbg_mesgtrace(__psint_t x, __psint_t a2, int argc, char **argv)
{
	ktrace_entry_t *ktep;
	ktrace_snap_t kts;
	int count, nentries;
	int ids[30], j, i, id;

	ktep = ktrace_first(mesg_trace, &kts);

	if (argc && (*argv[0] < '0' || *argv[0] > '9')) {
		qprintf("Usage: mesgtrace # [subsys-names ...]\n");
		return;
	}
	
	nentries = ktrace_nentries(mesg_trace);

	if (nentries <= 0) {
		qprintf("mesgtrace: mesgmask = 0x%x\n", mesg_trace_mask);
		qprintf("mesgtrace: Zero mesg_trace entries available.\n");

		return;
	}

	count = (int)atoi(argv[0]);

	if (count <= 0) {
		qprintf("mesgtrace: mesgmask = 0x%x\n", mesg_trace_mask);
		qprintf("mesgtrace: %d mesg_trace entries available.\n",
								nentries);
		qprintf("Usage: mesgtrace # [subsys-names ...]\n");
		qprintf("     : use 'mesgmask ?' to list subsystems\n");

		return;
	}

	nentries--; /* skip one we already have */
	if (count > nentries)
		count = nentries;

	qprintf("mesgtrace printing %d entries\n", count + 1);

	if (nentries > count)
		ktep = ktrace_skip(mesg_trace, nentries - count, &kts);

	if (argc == 1) {
		while (ktep && (count-- >= 0)) {
			prmesg(ktep);
			ktep = ktrace_next(mesg_trace, &kts);
		}
	} else {
		for (i = 0; i < sizeof(ids)/sizeof(int); i++)
			ids[i] = 0;
		for (j = 0, i = 0; i < argc; i++) {
			for (id = 1; id < nsubsys_names; id++) {
				if (strcmp(argv[i], subsys_names[id]) == 0)
					ids[j++] = id;
			}
		}
		while (ktep && (count-- >= 0)) {
			for (i = 0; i < j; i++)
				if (ktep->val[0] == (void *)(__psunsigned_t)ids[i]) {
					prmesg(ktep);
					break;
				}
			ktep = ktrace_next(mesg_trace, &kts);
		}
	}
}

/*
 * set mask of which subsystems to trace
 * a '0' clears everything
 */
/* ARGSUSED */
void
idbg_mesgmask(__psint_t x, __psint_t a2, int argc, char **argv)
{
	int i, id;

	if ((argc == 0) && (x == -1)) {
		qprintf("mesgmask = 0x%x\n", mesg_trace_mask);

		for (i = 1; i < nsubsys_names; i++) {

			if (mesg_trace_mask & (1 << i))
				qprintf("%4d 0x%16x Subsystem %s\n",
						i, 1 << i, subsys_names[i]);
		}

		return;
	}

	if (argc && *argv[0] == '0') {
		mesg_trace_mask = 0x0;

		qprintf("mesgmask = 0x%x\n", mesg_trace_mask);
		return;
	}

	if (argc && *argv[0] == '?') {

		for (i = 1; i < nsubsys_names; i++)
			qprintf("%4d 0x%16x Subsystem %s\n",
						i, 1 << i, subsys_names[i]);

		return;
	}

	for (i = 0; i < argc; i++) {
		for (id = 1; id < nsubsys_names; id++) {
			if (strcmp(argv[i], subsys_names[id]) == 0) {
				mesg_trace_mask |= (1 << id);
				break;
			}
		}
		if (id >= nsubsys_names)
			qprintf("Unknown subsystem %s\n", argv[i]);
	}

	qprintf("mesgmask = 0x%x\n", mesg_trace_mask);
}

#endif /* DEBUG */

/*
 * Get a pointer to the mesgstat structure for this message.  If the
 * message is new then initialize the structure.
 */
void
mesgstat_enter(int subsysid, void *addr, char *name, void *return_addr, 
		struct mesgstat **mesgp)
{
	int s;
	struct mesgstat *prev_mesgp, *cur_mesgp;

	ASSERT(subsysid <= MAX_SUBSYSID);

	s = mp_mutex_spinlock(&mesgstat_lock);
	cur_mesgp = prev_mesgp = mesgstat[subsysid];
	while (cur_mesgp && cur_mesgp->addr != addr && 
			cur_mesgp->return_addr != return_addr) {
		prev_mesgp = cur_mesgp;
		cur_mesgp = cur_mesgp->next;
	}
	if (!cur_mesgp) {

		/* Get new one from pool */

		if (mesgstat_index < MAX_MESGSTAT) {
			cur_mesgp = &mesgstat_pool[mesgstat_index];
			if (!prev_mesgp) {
				mesgstat[subsysid] = cur_mesgp;
			}
			else {
				cur_mesgp->next = prev_mesgp->next;
				prev_mesgp->next = cur_mesgp;
			}
			cur_mesgp->addr = addr;
			cur_mesgp->return_addr = return_addr;
			strncpy(cur_mesgp->name, name, MESGNAMSZ);
			mesgstat_index++;
		}
		else {

			/* Use overflow */

			cur_mesgp = &mesgstat_pool[MAX_MESGSTAT];
		}
	}
	mp_mutex_spinunlock(&mesgstat_lock, s);
	*mesgp = cur_mesgp;
}

/*
 * Update stats for this message now that it is exiting.
 */
void 
mesgstat_exit(struct mesgstat *mesgp, unsigned int etime, unsigned int ctime)
{
	mesgp->count++;
	mesgp->etime += (get_timestamp() - etime);
	mesgp->ctime += ctime;
}

/* Display message statistics for each message that has occurred.
 * If the first argument is a '0' it clears everything.
 */
/* ARGSUSED */
void
idbg_mesgstats(__psint_t x, __psint_t a2, int argc, char **argv)
{
	struct mesgstat *mesgp;
	int i, y, tot_count = 0;
	time_t tdiff;
	int mesgsnt, mesgrcv;
	uint64_t tot_etime = 0;
	int tflag = 0;
	__psunsigned_t offset;

	if (!mesgstat_pool) {
		return;
	}

	/*
	 * real hack - allow reset of stats from user mode idbg command. It
	 * passes arguments differently
	 */
	if (x == 9 || argc && *argv[0] == '0') {

		/* Zero out the stats */

		bzero(mesgstat_pool, mesgstat_index * sizeof(struct mesgstat));
		bzero(mesgstat, sizeof(struct mesgstat *) * (MAX_SUBSYSID + 1));
		mesgstat_index = 0;
		mesgstat_start = lbolt;

		for (y = 0; y < maxcpus; y++) {
			if (pdaindr[y].CpuId == -1) {
				continue;
			}
			pdaindr[y].pda->ksaptr->si.mesgsnt = 0;
			pdaindr[y].pda->ksaptr->si.mesgrcv = 0;
		}
		return;
	}

	if (argc && *argv[0] == 't') {
		tflag++;	/* Show timings */
	}

	tdiff = (lbolt - mesgstat_start) / HZ;
	qprintf("INTERVAL: %10d secs\n", tdiff);
	if (!tdiff) {

		/* Stats have just been set to zero */

		return;
	}

	qprintf("%4s %10s %10s %10s %10s\n", "CELL", "MESGSNT", "SNT/SEC",
		"MESGRCV", "RCV/SEC");
	qprintf("---- ---------- ---------- ---------- ----------\n");

	mesgsnt = mesgrcv = 0;
	for (y = 0; y < maxcpus; y++) {
		if (pdaindr[y].CpuId == -1) {
			continue;
		}
		mesgsnt += pdaindr[y].pda->ksaptr->si.mesgsnt;
		mesgrcv += pdaindr[y].pda->ksaptr->si.mesgrcv;
	}
	qprintf("%4d %10d %10d %10d %10d\n",
		cellid(), mesgsnt, mesgsnt / tdiff, mesgrcv, mesgrcv / tdiff);
			
	if (tflag) {
		qprintf("%32s %10s %10s %10s\n", "MESSAGE", "CALLS", 
			"CALLS/SEC", "ETIME/CALL");
		qprintf("-------------------------------- ---------- ---------- ----------\n");
	}
	else {
		qprintf("%32s %32s %10s\n", "MESSAGE", "CALLER", "CALLS");
		qprintf("-------------------------------- -------------------------------- ----------\n");
	}
	for (i = 0; i <= MAX_MESGSTAT; i++) {
		mesgp = &mesgstat_pool[i];
		if (!mesgp->name[0]) {

			/* End of the current list */

			break;
		}
		qprintf("%32s ",  mesgp->name);
		if (tflag) {
			qprintf("%10d %10d %10lld\n", 
				mesgp->count, mesgp->count / tdiff,
				(uint64_t)(((long long)mesgp->etime *
					timer_unit) / (1000 * mesgp->count)));
				
		}
		else {
			qprintf("%32s %10d\n",  
				fetch_kname(mesgp->return_addr, &offset),
				mesgp->count);
		}
		tot_etime += mesgp->etime;
		tot_count += mesgp->count;
	}
	if (tflag) {
		qprintf("-------------------------------- ---------- ---------- ----------\n");
		qprintf("%32s %10d %10d %10lld\n", "", tot_count, 
			tot_count / tdiff, 
			(uint64_t)(((long long)tot_etime * timer_unit) /
				(1000 * tot_count)));
	}
	else {
		qprintf("-------------------------------- -------------------------------- ----------\n");
		qprintf("%32s %32s %10d\n", "", "", tot_count);
	}
}

#ifdef DEBUG

extern void	xpc_dump_xpm(char *, void *, void *, void (*)(char *, ...));

char	*mesg_type[] = {
		"NONE",
		"RPC",
		"SEND",
		"REPLY",
		"CALLBACK"
};

void
dump_mesg(
	idl_msg_t	*idl_mesg)
{
	idl_hdr_t	*idl_hdr;

	qprintf("Dumping message at address 0x%x\n", idl_mesg);

	qprintf("XPC info:\n");
	xpc_dump_xpm("\t", NULL, &idl_mesg->msg_xpm, qprintf);

	idl_hdr = IDL_MESG_HDR(idl_mesg);
	qprintf("hdr 0x%x type %s id 0x%x", idl_hdr, 
		mesg_type[idl_hdr->hdr_type], idl_hdr->hdr_id);
	if (idl_hdr->hdr_type == MESG_TYPE_CALLBACK)
		qprintf(" callback id 0x%x", idl_hdr->hdr_callbackid);
	qprintf(" subsys %d opcode %d", idl_hdr->hdr_subsys, idl_hdr->hdr_opcode);
	prsymoff(mesg_func_list[idl_hdr->hdr_subsys][idl_hdr->hdr_opcode],
		" (", ")\n");
}

void
idbg_mesgdump(__psint_t x)
{
	idl_msg_t	*idl_mesg;

	if (x == -1) {
		qprintf("mesgdump <address>\n");
		return;
	}
	idl_mesg = (idl_msg_t *)x;
	if (idl_mesg->msg_xpm.xpm_type > 4) {
		qprintf("invalid message\n");
		return;
	}
	dump_mesg(idl_mesg);
}

static void
dump_chaninfo(
	chaninfo_t	*chaninfo)
{
	qprintf("%s channel to cell %d at 0x%x\n", 
			chaninfo->ci_channo ? "MEMBERSHIP" : "MAIN",
			chaninfo->ci_cell,
			chaninfo);
	qprintf("\tstate %s refcnt %d nextid 0x%x\n",
			chanstate[chaninfo->ci_state],
			chaninfo->ci_refcnt,
			chaninfo->ci_nextid);
	qprintf("\txpr 0x%x mutex 0x%x\n",
			chaninfo->ci_chanid, &chaninfo->ci_lock);

	print_mesg_title();
	dump_reply_tab(chaninfo->ci_cell, chaninfo->ci_reply_tab);
}

void
idbg_mesgchan(__psint_t x)
{
	kqueue_t	*kq;
	chaninfo_t	*chaninfo;
	int		i;

	if (x >= 0) {
		cell_t	cell;

		cell = (cell_t)x;
		kq = &mesg_channels[CELL_HASH(cell)];
		for (chaninfo = (chaninfo_t *)kqueue_first(kq);
		     chaninfo != (chaninfo_t *)kqueue_end(kq);
		     chaninfo = (chaninfo_t *)kqueue_next(&chaninfo->ci_list)) {
			if (chaninfo->ci_cell == cell)
				dump_chaninfo(chaninfo);
		}
		return;
	}
	if (x != -1) {
		chaninfo = (chaninfo_t *)x;
		dump_chaninfo(chaninfo);
		return;
	}
	for (i = 0; i < NMESG_CHANNELS; i++) {
		kq = &mesg_channels[i];
		for (chaninfo = (chaninfo_t *)kqueue_first(kq);
		     chaninfo != (chaninfo_t *)kqueue_end(kq);
		     chaninfo = (chaninfo_t *)kqueue_next(&chaninfo->ci_list)) {
			dump_chaninfo(chaninfo);
		}
	}
}

static void
mesgsize_stats_add(
	msgsz_stat_t	*stats,
	size_t		ssize,
	uint		subsys,
	uint		opcode,
	int		isreply)
{
	int	i;
	size_t	*counts;

	for (i = 0; ;i++) {
		if (ssize <= stats[i].msz_hilimit) {
			atomicAddUlong(&stats[i].msz_count, 1L);
			if (stats[i].msz_trace == NULL) {
				mutex_lock(&msgsz_trace, PZERO);
				if (stats[i].msz_trace == NULL) {
					stats[i].msz_trace = kmem_zalloc(
							sizeof(msz_tbuf_t),
							KM_SLEEP);
				}
				mutex_unlock(&msgsz_trace);
			}
			counts = (*(stats[i].msz_trace))[subsys][opcode];
			if (isreply) {
				atomicAddUlong(&counts[MSZ_TBUF_RCOUNT], 1L);
				counts[MSZ_TBUF_RSIZE] = ssize;
			} else {
				atomicAddUlong(&counts[MSZ_TBUF_SCOUNT], 1L);
				counts[MSZ_TBUF_SSIZE] = ssize;
			}
			return;
		}
	}
}

static void
mesgsize_stats_collect(
	idl_msg_t	*msg)
{
	size_t		total_size;
	xpm_t		*xpm;
	xpmb_t		*xpmb;
	int		i;
	idl_hdr_t	*idlhdr;
	uint		subsys;
	uint		opcode;
	int		isreply;

	xpm = &msg->msg_xpm;
	xpmb = &msg->msg_xpmb;

	idlhdr = IDL_MESG_HDR(msg);
	subsys = idlhdr->hdr_subsys;
	opcode = idlhdr->hdr_opcode;
	isreply = (idlhdr->hdr_type == MESG_TYPE_REPLY);

	total_size = 0;
	for (i = 0; i < xpm->xpm_cnt; i++)
		total_size += xpmb[i].xpmb_cnt;
	mesgsize_stats_add(msgsz_total, total_size, subsys, opcode, isreply);
	mesgsize_stats_add(msgsz_inline, xpmb[0].xpmb_cnt,
				subsys, opcode, isreply);
	for (i = 1; i < xpm->xpm_cnt; i++) {
		mesgsize_stats_add(msgsz_outofline, xpmb[i].xpmb_cnt,
					subsys, opcode, isreply);
	}
	mesgsize_stats_add(msgsz_outoflinecnt, xpm->xpm_cnt-1,
				subsys, opcode, isreply);
}

static void
mesgsize_trace_print(
	msz_tbuf_t	*tracebuf,
	int		inbytes)
{
	uint	subsys;
	uint	opcode;
	size_t	*counts;
	int	i;

	for (subsys = 0; subsys < MAX_SUBSYSID; subsys++) {
		for (opcode = 0; opcode < MAX_IDL_OPCODE; opcode++) {
			counts = (*tracebuf)[subsys][opcode];
			/*
			 * XXX we just want to step through the two counts
			 * but they are at indexes 0 and 2. RSIZE is 3.
			 */
			for (i = MSZ_TBUF_SCOUNT; i < MSZ_TBUF_RSIZE; i += 2) {
				if (counts[i] != 0) {
					prsymoff(mesg_func_list[subsys][opcode],
						"\t\t\t\t", i ? "(Reply)" : "");
					if (inbytes) {
						qprintf(" (%d bytes)",
							counts[i+1]);
					}
					qprintf(" - %d\n", counts[i]);
				}
			}
		}
	}
}

static void
mesgsize_print(
	msgsz_stat_t	*stats,
	char		*title,
	char		*units,
	int		inbytes,
	int		dotracing)
{
	int	i;
	size_t	lolimit = 0;
	char	*fmt;

	qprintf("%s:\n", title);
	qprintf("\t%s\t\tCount\n", units);
	for (i = 0; ; i++) {
		if (lolimit == stats[i].msz_hilimit) {
			if (inbytes)
				fmt = "\t0x%x\t";
			else
				fmt = "\t%d\t";
			qprintf(fmt, stats[i].msz_hilimit);
		} else if (stats[i].msz_hilimit == MESG_MAXSZ) {
			if (inbytes)
				fmt = "\t>0x%x\t";
			else
				fmt = "\t>%d\t";
			qprintf(fmt, lolimit);
		} else {
			if (inbytes)
				fmt = "\t0x%x-0x%x";
			else
				fmt = "\t%d-%d\t";
			qprintf(fmt, lolimit, stats[i].msz_hilimit);
		}
		qprintf("\t%d\n", stats[i].msz_count);
		if (dotracing && stats[i].msz_count != 0)
			mesgsize_trace_print(stats[i].msz_trace, inbytes);
		if (stats[i].msz_hilimit == MESG_MAXSZ)
			break;
		lolimit = stats[i].msz_hilimit;
	}
}


void
mesgsize_stats_clear(
	msgsz_stat_t	*stats)
{
	int	i;

	for (i = 0; ; i++) {
		stats[i].msz_count = 0;
		if (stats[i].msz_trace != NULL)
			bzero(stats[i].msz_trace, sizeof(msz_tbuf_t));
		if (stats[i].msz_hilimit == MESG_MAXSZ)
			break;
	}
}

void reset_mesgsize_stats()
{
	mesgsize_stats_clear(msgsz_total);
	mesgsize_stats_clear(msgsz_inline);
	mesgsize_stats_clear(msgsz_outofline);
	mesgsize_stats_clear(msgsz_outoflinecnt);
}

void
mesgsize_print_stats(int dotracing)
{
	size_t	total;
	int	i;

	total = 0;
	for (i = 0; ; i++) {
		total += msgsz_total[i].msz_count;
		if (msgsz_total[i].msz_hilimit == MESG_MAXSZ)
			break;
	}
	qprintf("Message size statistics for %d messages%s\n", total,
				dotracing ? " with tracing" : "");

	mesgsize_print(msgsz_total, "Total message size", "Bytes", 1, dotracing);
	mesgsize_print(msgsz_inline, "Inline data size", "Bytes", 1, dotracing);
	mesgsize_print(msgsz_outofline, "Outofline data size", "Bytes", 1, dotracing);
	mesgsize_print(msgsz_outoflinecnt, "Outofline data count", "nbuffs", 0, dotracing);
}

/* ARGSUSED */
void
idbg_mesgsizetrace(__psint_t x)
{
	mesgsize_print_stats(1);
}

void
idbg_mesgsize(__psint_t x)
{
	if (x == 0) {
		mesgsize_stats_clear(msgsz_total);
		mesgsize_stats_clear(msgsz_inline);
		mesgsize_stats_clear(msgsz_outofline);
		mesgsize_stats_clear(msgsz_outoflinecnt);
		return;
	}

	mesgsize_print_stats(0);
}

void
mesgsize_grab(
	msgsz_stat_t	*stats, 
	int		sizeflag, 
	int		lopos,
	int		hipos,
	int		inbytes,
	void		*DS, 
	int		Offset)
{
	uint			opcode;
	uint			subsys;
	uint			i1;
	uint			i2;
	uint			count = 0;
	size_t			lolimit = 0;
	size_t			*counts;
	mesgsizestatsinfo_t	*startptr;
	__psunsigned_t		offset;
	extern  char *lookup_kname(register void *, register void *);

	if( !sizeflag)
		startptr = &(((mesgsizestatsinfo_t *)DS)[lopos + Offset]);
	for (i1 = 0; ; i1++) {
		if (stats[i1].msz_count != 0) {
			for (subsys = 0; subsys < MAX_SUBSYSID; subsys++) {
		 		for (opcode = 0; 
				     opcode < MAX_IDL_OPCODE; 
				     opcode++) {
					counts=(*(stats[i1].msz_trace))\
						[subsys][opcode];
					for (i2= MSZ_TBUF_SCOUNT; 
					     i2 < MSZ_TBUF_RSIZE; 
					     i2 += 2) {
						if(counts[i2]) {
							if(sizeflag)
								count++; 
							else {
								startptr->mesgcount = counts[i2];
								startptr->size =  (inbytes? counts[i2 +1]:-1);
								startptr->sizelolimit = lolimit;
								startptr->sizehilimit = stats[i1].msz_hilimit;
								startptr->replyflag = (i2 ?  1 : 0);
								startptr->sortflag = 0;
								strcpy(startptr->message ,lookup_kname( mesg_func_list[subsys][opcode], &offset));
								startptr++;
							}
						}
					}
				}
			}
		}
		if (stats[i1].msz_hilimit == MESG_MAXSZ)
			break;
		lolimit = stats[i1].msz_hilimit;

	}
	if(sizeflag)
		*((int *)DS)  = count;
}

void
sort_messages(
	void	*DS, 
	int	lopos,
	int	hipos,
	int	offset)
{
	mesgsizestatsinfo_t	*startptr;
	mesgsizestatsinfo_t	*sortptr;
	int			count, count1 ;
	int			loindex;

	startptr = &(((mesgsizestatsinfo_t *)DS)[lopos + offset]);
	sortptr =  &(((mesgsizestatsinfo_t *)DS)[lopos]);
	
	for(count = lopos ; count <= hipos ; count++) {
		loindex = count - lopos ;
		for(count1 = lopos ; count1 <= hipos ; count1++)
			if((startptr[loindex].sortflag ) ||
			   ((startptr[loindex].size > startptr[count1-lopos].size) &&
			    (!startptr[count1 - lopos].sortflag)))
				loindex = count1 -lopos;
		startptr[loindex].sortflag = 1;
		bcopy(startptr + loindex  , sortptr + count - lopos, 
			sizeof(mesgsizestatsinfo_t));
	}
}

void
mesgsize_grab_stats(
	int	sizeflag,
	void	*DS1,
	void	*DS2,
	int	tot_size)
{
	int	lopos = 0;
	int	highpos;

	if (sizeflag) {
		mesgsize_grab(msgsz_total, 1, 0, 0, 1,
				(void *)&(((int *)DS2)[0]), 0);
		mesgsize_grab(msgsz_inline, 1, 0, 0, 1,
				(void *)&(((int *)DS2)[1]), 0);
		mesgsize_grab(msgsz_outofline, 1, 0, 0, 1,
				(void *)&(((int *)DS2)[2]), 0);
		mesgsize_grab(msgsz_outoflinecnt, 1, 0, 0, 0,
				(void *)&(((int *)DS2)[3]), 0);
	} else {
		highpos = ((int *)DS2)[0] - 1;
		mesgsize_grab(msgsz_total, 0, lopos, highpos, 1,
				DS1, tot_size);
		mesgsize_grab(msgsz_inline, 0, lopos += ((int *)DS2)[0],
				(highpos += ((int *)DS2)[1]), 1, DS1, tot_size);
		mesgsize_grab(msgsz_outofline, 0, lopos += ((int *)DS2)[1],
				(highpos += ((int *)DS2)[2]), 1, DS1, tot_size);
		mesgsize_grab(msgsz_outoflinecnt, 0, lopos += ((int *)DS2)[2],
				(highpos += ((int *)DS2)[3]), 0, DS1, tot_size);
		lopos = 0;
		highpos = ((int *)DS2)[0] - 1;
		sort_messages(DS1, lopos, highpos, tot_size);
		sort_messages(DS1, lopos += ((int *)DS2)[0],
				highpos += ((int *)DS2)[1], tot_size);
		sort_messages(DS1, lopos += ((int *)DS2)[1],
				highpos += ((int *)DS2)[2], tot_size);
		sort_messages(DS1,  lopos += ((int *)DS2)[2],
				highpos += ((int *)DS2)[3], tot_size);
	}
}
#endif
