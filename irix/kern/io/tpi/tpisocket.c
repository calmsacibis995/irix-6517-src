/*
 * Copyright (c) 1990-1997 Silicon Graphics Computer Systems, Inc.
 *
 * tpisocket Provide TPI interface above sockets-based network protocol stacks
 *
 * Many pieces of this code have been re-written in order to support the XTI
 * specification as part of Spec 1170.
 *
 * Among the changes made between Irix 6.4 and Irix 6.5 is the fact that ack
 * messages are sent in band 1 so that flushing the queues prior to sending
 * a disconnect will not flush ack messages before the user has a chance
 * to consume them.  Ack messages are now all sent as M_PCPROTO messages as
 * well, per the Transport Provider Interface Specification.
 *
 * Urgent data is now handled in line.  When sohasoutofband() is called, we
 * will set MSG_OOB flag in pending_recv_flags and it will not be cleared
 * until we reach the mark.  This will cause T_EXDATA_INDs to be generated
 * instead of T_DATA_INDs.
 *
 * We used to try to infer the TPI state from a variety of flag combinations,
 * but this proved to be very difficult under all circumstances.  In 6.5 we
 * maintain a real state in tso_state, although we don't maintain every
 * possible state value; in particular the transition states between the time
 * that an event is received and the event is acknowledged are ignored.
 *
 * The other large change has to do with options; in addition to supporting
 * options management via T_OPTMGMT_REQ, we now pay attention to "association"
 * options that are passed in with T_CONN_REQs, T_CONN_RESs & T_UNITDATA_REQs.
 * Such options are also passed to the transport user with T_CONN_INDs and
 * T_CONN_CONs.
 *
 * NOTE: Two sets of options are supported; the old (non-XTI ones) and the
 * new (XTI ones).  The XTI ones are well defined in the X/Open documentation;
 * the old ones are sort of a hodge-podge, and are not guaranteed to be useful
 * to anybody.
 */
#ident "$Revision: 1.136 $"

#define _TPISOCKD	1		/* hopefully this can go away soon */
/* define _TPI_DEBUG 1 */

#ifndef COMPAT_43
#define COMPAT_43 1
#endif

#include "tpisocket.h"
#include "sys/types.h"
#include "sys/sema.h"
#include "bsd/sys/hashing.h"

#include "sys/conf.h"
#include <sys/cred.h>
#include <sys/ddi.h>	/* for geteminor(), etc. */
#include "sys/domain.h"
#include "sys/socket.h"
#include "sys/socketvar.h"
#include "sys/vsocket.h"
#include "sys/proc.h"
#include "bsd/net/if.h"
#include "bsd/netinet/in_systm.h"
#include "bsd/netinet/in.h"
#include "bsd/netinet/ip.h"
#include "bsd/netinet/in_var.h"
#include "bsd/net/if.h"
#include "bsd/net/raw_cb.h"
#include "bsd/net/route.h"
#include "bsd/netinet/in_pcb.h"
#include "bsd/netinet/tcp.h"
#include "bsd/netinet/tcp_seq.h"
#include "bsd/netinet/tcp_timer.h"
#include "bsd/netinet/tcp_var.h"
#include "sys/idbg.h"
#include "sys/idbgentry.h"

#include "bsd/net/soioctl.h"
#include <sys/major.h>
#include <sys/sesmgr.h>
#include <sys/systm.h>
#include "sys/sysmacros.h"
#include "sys/atomic_ops.h"
#include "sys/xti.h"

#include <sys/cmn_err.h>
#include <sys/ktime.h>

#include <sys/strsubr.h>
#include <sys/sat.h>

#include <ksys/sthread.h>
#include <fs/specfs/spec_lsnode.h>
#include <limits.h>
#ifndef IOCGROUP
#define IOCGROUP(x) (((x) >> 8) & 0xFF)
#endif /* IOCGROUP */

#define TSO_HOLD(x) (atomicAddUlong(&(x)->tso_count,1))
#define TSO_RELE(x) {if ((x)->tso_count == 1) \
			tpisocket_dismantle((x)); \
		     else \
			atomicAddUlong(&(x)->tso_count, -1); }

#define	ISXTI	1
#define	NOTXTI	0

#ifdef _TPI_DEBUG
int _tpiprintfs = 0;
#endif

/*
 * External global variable declarations
 */
extern int queueflag;
extern struct streamtab clninfo;

/*
 * Forward procedure declarations
 */
static int tpisocket_makextiopt(mblk_t *, struct t_opthdr *, void *, int);
void	tpisocket_flushq(struct tpisocket *, int);
int	tpisocket_do_xtioptmgmt(struct tpisocket *,struct T_optmgmt_req *,
		mblk_t *, mblk_t *, int	*);
int	tpisocket_process_assoc_options(struct tpisocket *, struct socket *,
		mblk_t *, u_char *, int, mblk_t **, int *);
void	tpisocket_conn_req_done(struct tpisocket *);
void	tpisocket_convert_error(int *, int *, int);
int	tpisocket_convert_socket(struct vfile *, struct cred *);
void	tpisocket_copy_to_mblk_chain(caddr_t, int, mblk_t **, int *);
struct mbuf *tpisocket_copy_to_mbuf(void *, int, int, int*);
void	tpisocket_daemon(void);
void	tpisocket_discon_req_done(struct tpisocket *);
static void	tpisocket_discard(void);
static void	tpisocket_dismantle(struct tpisocket *);
struct mbuf *tpisocket_dup_mbuf(struct mbuf *, int, queue_t *, int);
int	tpisocket_event(struct socket *, int, struct tpisocket *);
int	tpihold(void *, void **, void *);
int	tpisocket_ifconf( struct tpisocket *, mblk_t *);
int	tpisocket_ifiocdata(struct tpisocket *, mblk_t *);
int	tpisocket_ifioctl(struct tpisocket *, mblk_t *);
void	tpisocket_init(void);
struct mbuf *tpisocket_mblk_to_mbuf(mblk_t *, int, int, int, queue_t *);
void	tpisocket_mbufcall_wakeup(struct tpisocket *);
int	tpisocket_open(queue_t *, dev_t *, int, int, cred_t *,
			struct tpisocket_control *);
void	tpisocket_optmgmt_req_done(struct tpisocket *, int);
void	tpisocket_release_recv_resources(struct tpisocket *);
void	tpisocket_release_send_resources(struct tpisocket *);
void	tpisocket_request_mbufcall(queue_t *);
int	tpisocket_rtiocdata(struct tpisocket *, mblk_t *);
int	tpisocket_rtioctl(struct tpisocket *, mblk_t *);
void	tpisocket_scan_conn_q(struct tpisocket *, struct socket *);
int	tpisocket_sioc_name(struct tpisocket *, mblk_t *, int);
mblk_t *tpisocket_allocb(queue_t *, int, int);
mblk_t *tpisocket_mbuf_to_mblk(queue_t *, struct mbuf *, int);
int	tpisocket_soreceive(struct tpisocket *, struct socket *, 
	 struct mbuf **, struct uio *, struct mbuf **, struct mbuf **, int *);
int	tpisocket_sosend(struct socket *, struct mbuf *, struct uio *,
			struct mbuf *, struct mbuf *, int);
int	tpisocket_ti_name(struct tpisocket *, mblk_t *);
struct mbuf *tpisocket_trim_mbuf_chain(struct mbuf *, int);
int	tpisocket_wact(struct tpisocket *, mblk_t *);
int	tpisocket_wreject(struct tpisocket *, mblk_t *, int, int, int);

#ifdef _TPI_DEBUG
extern void traceme(long, void *);
#define TRACEME(a,b) traceme(a,b)
#else
#define TRACEME(a,b)
#endif

static int tpisocket_send_check(struct tpisocket *);
static int tpisocket_send_iocack(queue_t *, mblk_t *, mblk_t *);
static int tpisocket_send_iocnak(queue_t *, mblk_t *, int);
static int tpisocket_send_ok_message(struct tpisocket *,
	struct socket *, mblk_t *);

#ifdef _TPISOCKD
static int _tpisocket_rsrv(queue_t *, void *, struct tpisocket *);
static int _tpisocket_wput(queue_t *, mblk_t *, struct tpisocket *);
static int _tpisocket_wsrv(queue_t *, void *, struct tpisocket *);
#endif

/*
 * Global variables
 */
int	tpisocket_clone_major = -1;
int	tpisocket_initialized = 0;
static struct socket *tpisocket_discard_list = NULL;
struct tpisocket_control *tpisocket_protocols = NULL;

static int      opt_off = 0;
static struct linger lingerdef = {0, 0};
static struct optdefault sockdefault[] = {
	SO_LINGER,	(char *)&lingerdef,	sizeof(struct linger),
	SO_DEBUG,	(char *)&opt_off,	sizeof(int),
	SO_KEEPALIVE,	(char *)&opt_off,	sizeof(int),
	SO_DONTROUTE,	(char *)&opt_off,	sizeof(int),
	SO_USELOOPBACK,	(char *)&opt_off,	sizeof(int),
	SO_BROADCAST,	(char *)&opt_off,	sizeof(int),
	SO_REUSEADDR,	(char *)&opt_off,	sizeof(int),
	SO_OOBINLINE,	(char *)&opt_off,	sizeof(int),
	SO_IMASOCKET,	(char *)&opt_off,	sizeof(int),
	0, 		(char *)0, 		0
};

struct tsockd_entry {
        struct tsockd_entry *next;
        void    (*func)(void *, void *, void *);
        void    *arg1, *arg2, *arg3;
};

/*
 * tpisocket_init
 *
 * Called via the io_init[] list at system startup time.
 * Adds an entry to the list of kernel processes to be started.
 */
static timespec_t backoff = { 0, 1L }; /* a minimal backoff */
static lock_t tpisocket_discard_lock;

#ifdef _TPISOCKD
static lock_t tpisockd_lock;
static sv_t tpisockd_wait, tpisockd_sync, tpisockd_inuse;
static struct tsockd_entry *tpisockd_head, *tpisockd_tail, *tpisockd_free;
static int tpisockd_running;
static zone_t *tpisockd_zone;
static void tpisockd_call(void (*)(), void *, void *, void *);
static void    tpisockd(void);

#define TPISOCKD_FREEMAX	20
int tpisockd_freecnt = TPISOCKD_FREEMAX;
int tpisockd_freemax = TPISOCKD_FREEMAX;

/*
 * tpisockd_call()
 */
static void
tpisockd_call(void (*fun)(), void *arg1, void *arg2, void *arg3)
{
	int s;
        struct tsockd_entry *entry = kmem_zone_alloc(tpisockd_zone,KM_NOSLEEP);

	TRACEME(0x1, (void *) fun);
	TRACEME(0x2, (void *) arg1);
	TRACEME(0x3, (void *) arg2);
	TRACEME(0x4, (void *) arg3);

	s = mp_mutex_spinlock(&tpisockd_lock);
	if (!entry) {
		entry = tpisockd_free;
		if (!entry)
                	cmn_err(CE_PANIC, "tpisockd_call: could not allocate sockd entry");
		else {
			tpisockd_free = entry->next;
			tpisockd_freecnt--;
		}
	}

        entry->func = fun;
        entry->arg1 = arg1;
        entry->arg2 = arg2;
        entry->arg3 = arg3;
        entry->next = 0;
        if (!tpisockd_tail) {
                ASSERT(!tpisockd_head);
                tpisockd_head = tpisockd_tail = entry;
        }
        else {
                tpisockd_tail->next = entry;
                tpisockd_tail = entry;
        }
        sv_signal(&tpisockd_wait);
        mp_mutex_spinunlock(&tpisockd_lock, s);
}

void
tpisockd_init(void)
{
	register int i;
	extern int tpisockd_pri;

	tpisockd_zone = kmem_zone_init(sizeof(struct tsockd_entry),
				"tpisockd entry");

        spinlock_init(&tpisockd_lock, "tpisockd_lock");
        sv_init(&tpisockd_wait, SV_LIFO, "tpisockd_wait");
        sv_init(&tpisockd_sync, SV_DEFAULT, "tpisockd_sync");
        sv_init(&tpisockd_inuse, SV_DEFAULT, "tpisockd_inuse");

	/* create back up free list */
	for (i=tpisockd_freemax; i; i--) {
        	struct tsockd_entry *entry;

		entry = kmem_zone_alloc(tpisockd_zone, KM_NOSLEEP);
		entry->next = tpisockd_free;
		tpisockd_free = entry;
	}
	sthread_create("tpisockd", 0, 8192, 0, tpisockd_pri, KT_PS,
			(st_func_t *)tpisockd, 0, 0, 0, 0);
}

/*
 * tpisockd()
 */
static void
tpisockd(void)
{
	int s;
        struct tsockd_entry *entry;

        STREAMS_LOCK();
	s = mp_mutex_spinlock(&tpisockd_lock);
        tpisockd_running++;
again:
        if (!(entry = tpisockd_head)) {
                tpisockd_running--;
                if (tpisockd_running == 0)
                        sv_signal(&tpisockd_sync);
                mp_sv_wait(&tpisockd_wait, PZERO-1, &tpisockd_lock, s);
		s = mp_mutex_spinlock(&tpisockd_lock);
                tpisockd_running++;
                goto again;
        }
        tpisockd_head = entry->next;
        if (tpisockd_head == 0) {
                ASSERT(entry == tpisockd_tail);
                tpisockd_tail = 0;
        }
	mp_mutex_spinunlock(&tpisockd_lock, s);
        entry->func(entry->arg1, entry->arg2, entry->arg3);

	s = mp_mutex_spinlock(&tpisockd_lock);
        if (tpisockd_freecnt < tpisockd_freemax) {
                entry->next = tpisockd_free;
                tpisockd_free = entry;
                tpisockd_freecnt++;
        }
        else
        	kmem_zone_free(tpisockd_zone, entry);
        goto again;
}

#endif /* _MP_NETLOCKS */

#if _TPI_DEBUG
extern void _printflags(int, char **, char *);

char *tpi_tab_flags[] = {
	"ISOPEN",
	"CLONE_OPEN",
	"EXCLUSIVE",
	"OPENING",
	"CLOSING",
	"OPEN_WAIT",
	"RECV_PENDING",
	"SEND_PENDING",
	"QUEUE_OUTPUT",
	"IOCTL_PENDING",
	"CONN_REQ_PENDING",
	"OUTPUT_BUFCALL",
	"INPUT_BUFCALL",
	"RECV_ORDREL",
	"RECV_DISCON",
	"UNUSED",
	"DISCON_REQ_PENDING",
	"ADDRESS_BOUND",
	"OPTMGMT_REQ_PENDING",
	"SO_IMASOCKET",
	"WSRV_QUEUED",
	"RSRV_QUEUED",
	"INPUT_MBUFCALL",
	"OUTPUT_MBUFCAL",
	"OOB_INPUT_AVAILABLE",
	"CONN_REQ_FAILED",
	"XOPTMGMT_REQ_PNDNG",
	"XTI"
};

void
idbg_tpisocket(struct tpisocket *t)
{
	qprintf("  tpisocket 0x%x\n", t);
	qprintf("  tso_rq 0x%x, tso_wq 0x%x, tso_rdev 0x%x, tso_index %d\n",
		t->tso_rq, t->tso_wq, t->tso_rdev, t->tso_index);
	qprintf("  tso_vsocket 0x%x, tso_control 0x%x\n",
		t->tso_vsocket, t->tso_control);
	_printflags(t->tso_flags, tpi_tab_flags, "  flags");
	qprintf("\n  tso_state %d, tso_count %d\n",
		t->tso_state, t->tso_count);
	qprintf("  tso_abi 0x%x, tso_saved_conn_opts 0x%x\n",
		t->tso_abi, t->tso_saved_conn_opts);
	return;
}

char *tpi_tab_control_flags[] = {
	"LINK_SOCKET",
	"NO_SEAMS",
	"NO_ORDREL",
	"CONVERT_DISCON",
	"DEFAULT_ADDR",
	"NO_CONV_ADDRESS",
	"ALLOW_SENDFD"
};

void
idbg_tpicontrol(struct tpisocket_control *t)
{
	qprintf("  tpisocket_control 0x%x\n", t);
	qprintf("  nummajor_p = 0x%x, %d\n", t->tsc_nummajor_p,
		t->tsc_nummajor_p ? *t->tsc_nummajor_p : 0);
	qprintf("  maxdevices_p = 0x%x, %d\n", t->tsc_maxdevices_p,
		t->tsc_maxdevices_p ? *t->tsc_maxdevices_p : 0);
	qprintf("  dom=%d, type=%d, proto=%d\n", t->tsc_domain, 
		t->tsc_socket_type, t->tsc_protocol);
	_printflags(t->tsc_flags, tpi_tab_control_flags, "  flags");
	qprintf("\n");
	qprintf("  ttype=%d, tsdu=%d, etsdu=%d, cdata=%d, ddata=%d, tidu=%d\n",
		t->tsc_transport_type, t->tsc_tsdu_size,
		t->tsc_etsdu_size, t->tsc_cdata_size, t->tsc_ddata_size,
		t->tsc_tidu_size);
	qprintf("  pflag=%d, function=0x%x, next=%0x\n",
		t->tsc_provider_flag, t->tsc_function, t->tsc_next);
	return;
}
#endif

void
tpisocket_init(void)
{
#ifdef _TPI_DEBUG
#define	VD (void (*)())
	struct tpi_dbg_func { 
		char *name;
		void (*func)();
		char *desc;
	} tpi_dbg_funcs[] = {
		"tpisock", VD idbg_tpisocket, "print tpisocket struct",
		"tpicontrol", VD idbg_tpicontrol, "print tpisocket_control",
		0, 0, 0
	};
	struct tpi_dbg_func *tf;
#endif

	if (! tpisocket_initialized) {
		tpisocket_initialized = 1;
		tpisocket_clone_major = CLN_MAJOR;
#if _TPI_DEBUG
		for (tf = tpi_dbg_funcs; tf->name; tf++) {
			idbg_addfunc(tf->name, tf->func);
		}
#endif
		spinlock_init(&tpisocket_discard_lock, 
			"tpisocket_discard_lock");
	}
#undef VD
	return;
}

/*
 * tpisocket_register
 * Register a protocol, for use in converting sockets to TPI sockets
 */
int
tpisocket_register(struct tpisocket_control *tpictl)
{
	struct	tpisocket_control *ti;

	for (ti = tpisocket_protocols; ti != NULL; ti = ti->tsc_next)
		if (ti == tpictl)
			return 0;
	tpictl->tsc_next = tpisocket_protocols;
	tpisocket_protocols = tpictl;
	return 1;
}

/*
 * tpisocket_discard
 * Discard sockets discarded via the TPI accept procedure.
 * Since soclose sleeps, this cannot occur in a service routine.
 */
static void
tpisocket_discard(void)
{
	struct	socket *so, *nso;
	int s;

	TRACEME(0x6, (void *)tpisocket_discard_list);
	s = mp_mutex_spinlock(&tpisocket_discard_lock);

	while (tpisocket_discard_list != NULL) {
		so = tpisocket_discard_list;
		tpisocket_discard_list = (struct socket *)so->so_callout_arg;
		mp_mutex_spinunlock(&tpisocket_discard_lock, s);
restart_trash:
		SOCKET_QLOCK(so, s);
		so->so_state &= ~(SS_TPISOCKET | SS_WANT_CALL);
		/*
		 * In case we marked any sockets as being attached to us,
		 * go through and clear them prior to abort, else they
		 * will never be freed.
		 */
		for (nso = so->so_q; nso != NULL && nso != so;
		     nso = nso->so_q) {
			if (!SOCKET_TRYLOCK(nso)) {
				SOCKET_QUNLOCK(so, s);
				nano_delay(&backoff);
				goto restart_trash;
			}
			if ((nso->so_state & SS_NOFDREF) == 0) {
				nso->so_state &= ~SS_WANT_CALL;
				nso->so_state |= SS_NOFDREF;
			}
			SOCKET_UNLOCK(nso);
		}
		SOCKET_QUNLOCK(so, s);
		TRACEME(0x7, (void *)so);
		soclose(so);
		s = mp_mutex_spinlock(&tpisocket_discard_lock);
	}
	mp_mutex_spinunlock(&tpisocket_discard_lock, s);
	return;
}

#ifdef TPISOCKET_DAEMON
/* 
 * tpisocket_daemon
 *
 * XXX needs to be initialized ...
 * Started at system startup time as a kernel daemon.
 * Lives only to discard sockets discarded via the TPI accept procedure.
 */
void
tpisocket_daemon(void)
{
	int s;
	struct	socket *so;

	splnet();
		/* whichever is higher */
	while (1) {
		tpisocket_discard();
		sleep(&tpisocket_discard_list, PRIBIO);
	}
}
#endif /* TPISOCKET_DAEMON */

/*
 * tpisocket_open - Called to do the stream open
 */
int
tpisocket_open(
	register queue_t *rq,
	dev_t	*devp,
	int	flag,
	int	sflag,
	cred_t	*crp,
	struct	tpisocket_control *tpictl)
{
	struct tpisocket *tpiso;
	struct vsocket *vso;
	struct socket *so;
	int	d;

	TRACEME(0x8, (void *)rq);
	TRACEME(0x9, (void *)devp);
	TRACEME(0xa, (void *)(long)flag);
	TRACEME(0xb, (void *)(long)sflag);
	TRACEME(0xc, (void *)tpictl);

	
	if (TSC_MAXDEVICES == 0) {
		return(ENODEV);
	}
	if (TSC_DEVICES == NULL) {
		struct tpisocket **tsod;

#define TSODSIZE (sizeof(struct tpisocket *) * TSC_MAXDEVICES)
		tsod = (struct tpisocket **)kmem_zalloc(TSODSIZE, KM_SLEEP);
		ASSERT(tsod != NULL);
		if (TSC_DEVICES == NULL) {
			TSC_DEVICES = tsod;
		} else {
			kmem_free(tsod, TSODSIZE);
		}
	}
#undef TSODSIZE

	if (sflag) {			/* do 'clone' open */
		for (d = 0; d < TSC_MAXDEVICES && NULL != TSC_DEVICES[d]; d++)
			continue;
	} else {
		d = geteminor(*devp);
	}
	if (d >= TSC_MAXDEVICES) {
		return(ENODEV);
	}

#define TSO_WAITING ((struct tpisocket *)((__psunsigned_t)1))
again:
	if (TSC_DEVICES[d] == NULL || TSC_DEVICES[d] == TSO_WAITING) {
		TSC_DEVICES[d] = TSO_WAITING;
		tpiso = (struct tpisocket *)kmem_zalloc(sizeof *tpiso, 
				KM_SLEEP);
		ASSERT(tpiso != NULL);
		TRACEME(0xd,(void *)tpiso);
		if (TSC_DEVICES[d] == TSO_WAITING) {
			tpiso->tso_count = 1;
			tpiso->tso_index = d;
			tpiso->tso_control = tpictl;
			TSC_DEVICES[d] = tpiso;
			TRACEME(0xe,(void *)tpiso);
		} else {
			TRACEME(0xf,(void *)tpiso);
			kmem_free(tpiso, sizeof *tpiso);
		}
	}

	tpiso = TSC_DEVICES[d];
	if (tpiso->tso_flags & TSOF_EXCLUSIVE && drv_priv(crp) != 0) {
		return EBUSY;
	}
	if (tpiso->tso_flags & TSOF_OPENING) {
		if (flag & (FNDELAY | FNONBLOCK)) {
			return EAGAIN;
		}
		tpiso->tso_flags |= TSOF_OPEN_WAIT;
		TRACEME(0x10, (void *)tpiso);
		TRACEME(0x11, (void *)tpiso->tso_count);
		TSO_HOLD(tpiso);

		if (sleep((caddr_t)tpiso, STOPRI)) {
			TRACEME(0x12, (void *)tpiso);
			TRACEME(0x13, (void *)tpiso->tso_count);
#ifdef _TPISOCKD
			/* wake up tpisocket_close if waiting */
			if ((tpiso->tso_count == 2) &&
			    (tpiso->tso_flags & TSOF_CLOSING)) {
				sv_broadcast(&tpisockd_inuse);
			}
#endif
			TSO_RELE(tpiso);
			return EINTR;
		}
		TSO_RELE(tpiso);
		goto again;
	}

	if (tpiso->tso_flags & TSOF_CLOSING) {
		return EBUSY;
	}
	if (tpiso->tso_flags & TSOF_ISOPEN) {
		if (tpiso->tso_rq != rq) {
			return EBUSY;
		}
	} else {
		int err;

		rq->q_ptr = (caddr_t)tpiso; /* connect new device to stream */
		WR(rq)->q_ptr = (caddr_t)tpiso;
		tpiso->tso_rq = rq;
		tpiso->tso_abi = get_current_abi();
		tpiso->tso_saved_conn_opts = 0;
		tpiso->tso_wq = WR(rq);
		tpiso->tso_flags |= TSOF_OPENING;
		if (sflag)
			tpiso->tso_flags |= TSOF_CLONE_OPEN;

		ASSERT(!tpiso->tso_vsocket);
		/* allocate a socket structure */
		err = vsocreate(TSC_DOMAIN, &tpiso->tso_vsocket,
			       TSC_SOCKET_TYPE, TSC_PROTOCOL);
		if (! err) {
			vso = tpiso->tso_vsocket;
			so = (struct socket *)(BHV_PDATA((VSOCK_TO_FIRST_BHV(vso))));
			SOCKET_LOCK(so);
			so->so_options |= SO_OOBINLINE;
			so->so_callout_arg = (caddr_t)tpiso;
			so->so_callout = tpisocket_event;
			ASSERT(rq->q_monpp);
			so->so_monpp = rq->q_monpp;
			so->so_state |=
				(SS_TPISOCKET | SS_NBIO | SS_WANT_CALL);
			so->so_snd.sb_flags |= SB_NOTIFY;
			so->so_data = 0;
			SOCKET_UNLOCK(so);
		}
		tpiso->tso_flags &= ~TSOF_OPENING;
		if (tpiso->tso_flags & TSOF_OPEN_WAIT) {
			tpiso->tso_flags &= ~TSOF_OPEN_WAIT;
			wakeup((caddr_t)tpiso);
		}
		if (err) {
close_on_error:
			tpiso->tso_flags |= TSOF_CLOSING;
			TRACEME(0x14, (void *)tpiso);
			TRACEME(0x15, (void *)tpiso->tso_count);
			TSO_RELE(tpiso);
#ifdef _TPISOCKD
			/* wake up tpisocket_close if waiting */
			if ((tpiso->tso_count == 1) &&
			    (tpiso->tso_flags & TSOF_CLOSING)) {
				sv_broadcast(&tpisockd_inuse);
			}
#endif
			return(err);
		}
		tpiso->tso_flags |= (TSOF_ISOPEN | TSOF_UNUSED);
		tpiso->tso_cred = crp;
		crhold(crp);
		MGET(tpiso->tso_temporary_mbuf, M_WAIT, MT_DATA);
		err = TSC_OPEN(tpiso);
		if (err)
			goto close_on_error;
	}

	if (sflag) {			/* do 'clone' open */
		*devp = makedevice(getemajor(*devp), d);
	}
	
	TRACEME(0x16,(void *)tpiso);

	/* Create audit record */
#ifdef _TPI_DEBUG
	if (_tpiprintfs) {
		printf("K: open 0x%x, state %d\n",tpiso, tpiso->tso_state);
	}
#endif	/* _TPI_DEBUG */
	return 0;
}

/*
 * tpisocket_dismantle - deallocate a tpisocket structure
 */
static void
tpisocket_dismantle(struct tpisocket *tpiso)
{
	struct tpisocket_control *tpictl = tpiso->tso_control;
	struct	vsocket *vso;
	struct	socket *so;
	/* REFERENCED */
	int	error;

#define RELEASE_MBUF(field) { \
	if (tpiso->field) { \
		m_freem(tpiso->field); \
		tpiso->field = NULL; \
	} }
#define RELEASE_MBLK(field) { \
	if (tpiso->field != NULL) { \
		freemsg(tpiso->field); \
		tpiso->field = NULL; \
	} }

	TRACEME(0x17, (void *)tpiso);
	ASSERT((!(tpiso->tso_flags & (TSOF_OPEN_WAIT))));

	TSC_DEVICES[tpiso->tso_index] = NULL;
	if (tpiso->tso_flags & TSOF_ANY_MBUFCALL) {
		tpiso->tso_flags &= ~TSOF_ANY_MBUFCALL;
		untimeout(tpiso->tso_mbufcall_timeout);
	}
	vso = tpiso->tso_vsocket;
	if (vso != NULL) {
		so = (struct socket *)(BHV_PDATA(VSOCK_TO_FIRST_BHV(vso)));
		tpiso->tso_vsocket = NULL;
		SOCKET_LOCK(so);
		so->so_state &= ~(SS_TPISOCKET | SS_WANT_CALL);
		so->so_callout = NULL;
		so->so_monpp = NULL;
		SOCKET_UNLOCK(so);
		TRACEME(0x19, (void *)so);
		VSOP_CLOSE(vso, 1, 0, error);			/* XXX */
		TRACEME(0x1a, (void *)so);
	}
	if (tpiso->tso_cred) {
		crfree(tpiso->tso_cred);
		tpiso->tso_cred = NULL;
	}
	TRACEME(0x89, (void *)tpiso);
	tpisocket_release_send_resources(tpiso);
	tpisocket_release_recv_resources(tpiso);
	RELEASE_MBLK(tso_pending_ioctl_req);
	RELEASE_MBLK(tso_saved_conn_opts);
	RELEASE_MBLK(tso_pending_request);
	RELEASE_MBUF(tso_temporary_mbuf);
	RELEASE_MBLK(tso_pending_optmgmt_req);
	RELEASE_MBUF(tso_pending_optmgmt_mbuf);
#ifdef _TPI_DEBUG
	{
		int i;
		char *ptr=(char *)tpiso;
		for(i=0;i<sizeof(*tpiso);i++) *ptr = -1;
	}
#endif /* _TPI_DEBUG */
	kmem_free(tpiso, sizeof *tpiso);
	TRACEME(0x1b, (void *)tpiso);

#undef RELEASE_MBLK
#undef RELEASE_MBUF
}

/*
 * tpisocket_close - Close a tpisocket connection
 */
/*ARGSUSED*/
int		/* this should be void, but USL's prototypes are wrong */
tpisocket_close(queue_t	*rq, int flag, cred_t *crp)
{
	struct	tpisocket *tpiso = (struct tpisocket *)rq->q_ptr;
	struct	socket *so =
	  (struct socket *)(BHV_PDATA(VSOCK_TO_FIRST_BHV(tpiso->tso_vsocket)));
	struct	tpisocket_control *tpictl;

	TRACEME(0x1c, (void *)rq);
	TRACEME(0x1d, (void *)(long)flag);
	TRACEME(0x1e, (void *)tpiso);

	if (tpiso == NULL) {
		return 0;
	}
	tpictl = tpiso->tso_control;

#ifdef _TPI_DEBUG
	if (_tpiprintfs) {
		printf("K: closing 0x%x, state %d\n",tpiso, tpiso->tso_state);
	}
#endif	/* _TPI_DEBUG */
	/*
	 * ensure no more entries placed on tpisockd queue.
	 */
	ASSERT(!(tpiso->tso_flags & TSOF_OPENING));
	so = (struct socket *)(BHV_PDATA(VSOCK_TO_FIRST_BHV(tpiso->tso_vsocket)));
	if (so != NULL)
		SOCKET_LOCK(so);

	tpiso->tso_flags |= TSOF_CLOSING;
	if (so != NULL) {
		SOCKET_UNLOCK(so);
	}
#ifdef _TPISOCKD
	/*
	 * while in use, suspend tpisocket_close (may need mutex lock).
	 */
	while (tpiso->tso_count > 1) {
                sv_wait(&tpisockd_inuse, PZERO-1, NULL, 0);
	}
#endif
	TRACEME(0x1f, (void *)tpiso);
	TSC_CLOSE(tpiso);

	/* stop waiting for buffers */
	str_unbcall(rq);

	TRACEME(0x20, (void *)tpiso->tso_count);
	TSO_RELE(tpiso);

#ifndef TPISOCKET_DAEMON
	tpisocket_discard();	/* HACK - should be asynchronous process */
#endif
	return(0);
}

/*
 * tpisocket_event
 * Called by socket stack when it becomes possible to send to
 * or receive from the socket (when formerly it was not).  
 * Enables call on tpisocket_wsrv() or tpisocket_rsrv(), 
 * which will check for queued output or input packets and 
 * transfer them between the streams queue and the socket.
 * Entered at splnet().
 */
int
tpisocket_event(struct socket *so, int event, struct tpisocket *tpiso)
{
	int flag;

	TRACEME(0x21, (void *)so);
	TRACEME(0x22, (void *)(long)event);
	TRACEME(0x23, (void *)tpiso);

	if (tpiso->tso_vsocket == 0) {
		return 0;
	}

	flag = event & 0x10;
	event &= ~0x10;

	if (((struct socket *)(BHV_PDATA(VSOCK_TO_FIRST_BHV(tpiso->tso_vsocket)))) != so) {
		return 0;
	}

	/*
	 * If we are closing then nothing to do. The queue may have already
	 * been dismantled by a strclose since we may have slept waiting
	 * for the STREAM monitor to become free.
	 */
	if (flag && (tpiso->tso_flags & TSOF_CLOSING)) {
	        goto done;
	}

	switch (event) {
	case SSCO_OUTPUT_AVAILABLE:
		if (!(tpiso->tso_flags & TSOF_WSRV_QUEUED) &&
		    (tpiso->tso_flags & TSOF_ISOPEN) &&
		    !(tpiso->tso_flags & TSOF_CLOSING)) {
			tpiso->tso_flags |= TSOF_WSRV_QUEUED;
#ifdef notyet
			/*
			 * XXXrs - Not ever if you ask me.  This code is
			 * trying to "outsmart" the streams scheduling
			 * mechanism.  There may be some additional
			 * MP considerations w.r.t. the queuerun flag and
			 * blocking queueruns.  Think carefully before
			 * trying to turn something like this on.
			 *
			 * See the queuerun() code.
			 */
			if (! queueflag) {
				queuerun_block();
				
				tpisocket_wsrv(tpiso->tso_wq);
				queuerun_unblock();
				return;
			} else
#endif /* notyet */
				TRACEME(0x24, (void *) tpiso->tso_wq);
				qenable(tpiso->tso_wq);
		}
		break;

	case SSCO_OOB_INPUT_ARRIVED:
		/*
		 * We used to not do this if MSG_OOB was already set, but
		 * we probably want to, since if we get more urgent data
		 * before we've delivered all of the previous urgent data, we
		 * want to effectively make everything expedited up to the new
		 * mark, and that means the new mark will require adjustment
		 * as well.
		 * I'm still not sure why we need to adjust the OOB mark at
		 * all, but it seems to work, so I'm going with it.
		 *
		 * Make sure that we got some data by only doing this if
		 * SS_ATMARK is set or we're at the mark.  This way we won't
		 * get confused when we get this even during an accept.  It
		 * might be spurious so checking that tcp_input() did something
		 * is necessary.  tpisocket_soreceive() no longer maintains
		 * the SS_ATMARK flag, so it should be safe.
		 */
		if (so->so_state & SS_RCVATMARK || so->so_oobmark) {
			/* enter expedited data mode */
			tpiso->tso_pending_recv_flags |= MSG_OOB;
			/* bump oobmark so that multi-byte urgent stuff works*/
			so->so_oobmark++;
		} else {
			break;
		}
		/* fall through */

	case SSCO_INPUT_ARRIVED:
		if (!(tpiso->tso_flags & TSOF_RSRV_QUEUED) &&
		    canenable(tpiso->tso_rq)) {
			tpiso->tso_flags |= TSOF_RSRV_QUEUED;
#ifdef notyet
			/*
			 * XXXrs - Not ever if you ask me.  This code is
			 * trying to "outsmart" the streams scheduling
			 * mechanism.  There may be some additional
			 * MP considerations w.r.t. the queuerun flag and
			 * blocking queueruns.  Think carefully before
			 * trying to turn something like this on.
			 *
			 * See the queuerun() code.
			 */
			if (! queueflag) {
				queuerun_block();
				
				tpisocket_rsrv(tpiso->tso_rq);
				queuerun_unblock();
				return;
			} else
#endif /* notyet */
				qenable(tpiso->tso_rq);
		}
		break;

	default:
		break;
	}
	
done:
	if (flag) {
		TRACEME(0x25,(void *)tpiso);
		TRACEME(0x26,(void *)tpiso->tso_vsocket);
		TRACEME(0x81,(void *)tpiso->tso_count);
		TSO_RELE(tpiso);
#ifdef _TPISOCKD
		if ((tpiso->tso_count == 1) &&
		    (tpiso->tso_flags & TSOF_CLOSING)) {
			sv_broadcast(&tpisockd_inuse);
		}
#endif
	}
	TRACEME(0x27,(void *)so);
	return 0;
}

/*
 * function call from streams_interrupt to lock tpiso
 */
int
tpihold(void * func, void **arg2, void *tpiso)
{
	long a;
	TRACEME(0x80, (void *)func);
	TRACEME(0x82, (void *)*arg2);
	TRACEME(0x28,(void *)tpiso);
	if (tpiso) {
		TRACEME(0x29,(void *)(((struct tpisocket *)tpiso)->tso_count));
		TRACEME(0x2a,(void *)(((struct tpisocket *)tpiso)->tso_flags));
		TRACEME(0x2b,
			(void *)(((struct tpisocket *)tpiso)->tso_vsocket));
	}
	/*
	 * kludge to lock tpisocket in case we defer operation
	 * because STREAMS monitor already in use.
	 */
	if ((func == (void *)tpisocket_event) && tpiso) {
		if (((struct tpisocket *)tpiso)->tso_flags & TSOF_CLOSING) {
		TRACEME(0x2c,(void *)(((struct tpisocket *)tpiso)->tso_count));
			return 1;
		}
		TSO_HOLD(((struct tpisocket *)tpiso));
		TRACEME(0x2d,(void *)(((struct tpisocket *)tpiso)->tso_count));
		a = (long)*arg2;
		a |= 0x10;
		*arg2 = (void *)a;
	}
	return 0;
}

/*
 * tpisocket_allocb
 * Allocates a buffer. Queues a bufcall if necessary, returning NULL.
 * Entered at splstr().
 */
mblk_t *
tpisocket_allocb(queue_t *qp, int size, int pri)
{
	mblk_t	*bp;

	bp = allocb(size, pri);
	if (!bp) {			/* if we fail, set to try again */
		struct	tpisocket *tpiso;
		int	flag;

		tpiso = (struct tpisocket *)qp->q_ptr;
		flag = (qp == tpiso->tso_rq ? TSOF_INPUT_BUFCALL
					    : TSOF_OUTPUT_BUFCALL);
		if (!(tpiso->tso_flags & flag) &&
		    bufcall(size, pri, qenable, (long)qp)) {
			tpiso->tso_flags |= flag;
			noenable(qp);
		}
	}
	return bp;
}

/*
 * tpisocket_copy_to_mblk_chain
 * Copy a data buffer to an offset in an mblk chain
 */
void
tpisocket_copy_to_mblk_chain(
	caddr_t	datap,
	int	len,
	mblk_t	**bpp,
	int	*offsetp)
{
	int	count;
	mblk_t	*bp;

	bp = *bpp;
	while (len > 0 && bp != NULL) {
		count = (bp->b_wptr - bp->b_rptr) - *offsetp;
		if (count <= 0) {
			*offsetp = 0;
			bp = bp->b_cont;
			*bpp = bp;
			continue;
		}
		if (count > len)
			count = len;
		bcopy(datap, bp->b_rptr + *offsetp, count);
		(*offsetp) += count;
		len -= count;
		datap += count;
	}
}

/*
 * tpisocket_mbufcall_wakeup - Wakes up streams waiting for mbufs
 */
void
tpisocket_mbufcall_wakeup(struct tpisocket *tpiso)
{
	if (tpiso->tso_flags & TSOF_INPUT_MBUFCALL) {
		enableok(tpiso->tso_rq);
		qenable(tpiso->tso_rq);
	}
	if (tpiso->tso_flags & TSOF_OUTPUT_MBUFCALL) {
		enableok(tpiso->tso_wq);
		qenable(tpiso->tso_wq);
	}
	tpiso->tso_flags &= ~TSOF_ANY_MBUFCALL;
}

/*
 * tpisocket_request_mbufcall
 * Request an mbuf available qenable in the specified qp.
 * Entered at splstr().
 */
void
tpisocket_request_mbufcall(queue_t *qp)
{
	struct	tpisocket *tpiso;

	tpiso = (struct tpisocket *)qp->q_ptr;
	ASSERT(tpiso->tso_rq);
	if (!(tpiso->tso_flags & TSOF_ANY_MBUFCALL))
		tpiso->tso_mbufcall_timeout =
		 MP_STREAMS_TIMEOUT(tpiso->tso_rq->q_monpp,
			(strtimeoutfunc_t)tpisocket_mbufcall_wakeup,
			tpiso, 10);
		 tpiso->tso_flags |=
		   (qp == tpiso->tso_rq ? TSOF_INPUT_MBUFCALL : TSOF_OUTPUT_MBUFCALL);
	noenable(qp);
}

/*
 * tpisocket_mbuf_to_mblk
 * Converts an mbuf chain to an mblk chain
 * Entered with Stream monitor lock held
 */
mblk_t *
tpisocket_mbuf_to_mblk(queue_t* qp, struct mbuf *m, int pri)
{
	mblk_t	*bp;

	bp = mbuf_to_mblk(m, pri);
	if (!bp) { /* if we fail, set to try again */
		tpisocket_request_mbufcall(qp);
	}
	return bp;
}

/*
 * tpisocket_mblk_to_mbuf
 * Converts an mblk chain to an mbuf chain.
 * Entered with Stream monitor lock held
 */
struct mbuf *
tpisocket_mblk_to_mbuf(
	mblk_t	*mp,		/* pointer to message being passed */
	int	alloc_flags,
	int	mbuf_type,
	int	mbuf_flags,
	queue_t	*qp)
{
	struct	mbuf *m;

	if ((m = mblk_to_mbuf(mp, alloc_flags, mbuf_type, mbuf_flags))== NULL){
		tpisocket_request_mbufcall(qp);
	}
	return m;
}

/*
 * tpisocket_trim_mbuf_chain
 *
 * Trim len bytes from the front of an mbuf chain
 * Similar to m_adj(), but discards empty mbufs, and
 * returns pointer to beginning of shortened chain.
 */
struct mbuf *
tpisocket_trim_mbuf_chain(struct mbuf *m, int len)
{
	if (m == NULL)
		return NULL;

	while (len > 0 && m != NULL) {
		struct	mbuf *n;

		if (len < m->m_len) {
			m->m_off += len;
			m->m_len -= len;
			break;
		}
		len -= m->m_len;
		MFREE(m, n);
		m = n;
	}
	return m;
}

/*
 * tpisocket_dup_mbuf - Duplicate mbuf chain.  
 * Requests mbuf wakeup if mbufs not available.
 * Entered with Stream monitor lock held
 */
/*ARGSUSED*/
struct mbuf *
tpisocket_dup_mbuf(struct mbuf *src, int alloc_flags, queue_t* qp, int len)
{
	struct mbuf	*first;

	ASSERT(alloc_flags == M_DONTWAIT);	/* m_copy assumes DONTWAIT */
	first = m_copy(src, 0, len);
	if (first == NULL)
		tpisocket_request_mbufcall(qp);
	return first;
}

/*
 * tpisocket_copy_to_mbuf - Copy data buffer to an mbuf
 */
struct mbuf *
tpisocket_copy_to_mbuf(
	void 	*addr,
	int	len,
	int	mbuf_type,
	int	*error_p)
{
	struct	mbuf *m;

	if (((u_int)len) > MLEN) {	/* also catches len < 0 */
		*error_p = EINVAL;
		return NULL;
	}

#if defined(COMPAT_43) && defined(WRONG) 
	if (mbuf_type == MT_SONAME && ((u_int)len < MLEN))
		len = MLEN;	/* unix domain compat. hack */
#endif /* COMPAT_43 */

	MGET(m, M_DONTWAIT, mbuf_type);
	if (m == NULL) {
		*error_p = ENOBUFS;
		return NULL;
	}
	m->m_len = len;
	bcopy((caddr_t)addr, mtod(m, caddr_t), (u_int)len);
#ifdef RENO
	if (mbuf_type == MT_SONAME) 
		mtod(m, struct sockaddr *)->sa_len = len;
#endif
	*error_p = 0;
	return m;
}

/*
 * tpisocket_send_iocack
 * Converts a buffer from an M_IOCTL to an M_IOCACK message,
 * and appends a supplied data buffer.
 * Entered with Stream monitor lock held
 */
static int
tpisocket_send_iocack(queue_t *qp, mblk_t *bp, mblk_t *rbp)
{
	struct iocblk *iocp;

	freemsg(bp->b_cont);
	bp->b_cont = rbp;
	bp->b_datap->db_type = M_IOCACK;
	bp->b_band = 1;
	iocp = (struct iocblk *)bp->b_rptr;
	iocp->ioc_count = msgdsize(rbp);
	qreply(qp, bp);
	return 1;
}

/*
 * tpisocket_send_iocnak
 * Converts a buffer from an M_IOCTL to an M_IOCNAK message,
 * and supplies a given error code.
 * Entered with Stream monitor lock held
 */
static int
tpisocket_send_iocnak(queue_t *qp, mblk_t *bp, int unix_error)
{
	struct iocblk *iocp;

	freemsg(bp->b_cont);
	bp->b_cont = NULL;
	bp->b_datap->db_type = M_IOCNAK;
	bp->b_band = 1;
	iocp = (struct iocblk *)bp->b_rptr;
	iocp->ioc_count = 0;
	iocp->ioc_error = unix_error;
	qreply(qp, bp);
	return 1;
}

/*
 * tpisocket_send_ok_ack
 * Turn bp into a T_OK_ACK for the primitive correct_prim, and send upstream.
 */
static void
tpisocket_send_ok_ack(
	struct tpisocket *tpiso,
	mblk_t *bp,
	int correct_prim)
{
	struct T_ok_ack *oa;

	bp->b_datap->db_type = M_PCPROTO;
	bp->b_band = 1;
	bp->b_wptr = bp->b_rptr + sizeof(*oa);
	oa = (struct T_ok_ack *)bp->b_rptr;
	oa->PRIM_type = T_OK_ACK;
	oa->CORRECT_prim = correct_prim;

	qreply(tpiso->tso_wq, bp);
}

/*
 * tpisocket_send_discon_ind
 */
void
tpisocket_send_discon_ind(
	struct tpisocket *tpiso,
	mblk_t	*bp,
	int	unix_error,
	long	seq)
{
	union	T_primitives *pi;

	tpiso->tso_flags |= TSOF_RECV_DISCON;

	switch (tpiso->tso_state) {
	case TS_DATA_XFER:
	case TS_WIND_ORDREL:
	case TS_WREQ_ORDREL:
		tpisocket_flushq(tpiso, FLUSHRW);
		break;
	default:
		break;
	}
	bp->b_datap->db_type = M_PROTO;
	bp->b_wptr = bp->b_rptr + sizeof(struct T_discon_ind);

	pi = (union T_primitives *)bp->b_rptr;
	pi->type = T_DISCON_IND;
	pi->discon_ind.DISCON_reason = unix_error;
	pi->discon_ind.SEQ_number = seq;

	qreply(tpiso->tso_wq, bp);
}

/*
 * tpisocket_wreject - Rejects a TPI request.
 * Entered with Stream monitor lock held
 */
#define TSO_PUTWQ(tpiso, bp) { \
		tpiso->tso_flags |= TSOF_QUEUE_OUTPUT; \
		putq(tpiso->tso_wq,(bp)); }
#define TSO_PUTBWQ(tpiso, bp) { \
		tpiso->tso_flags |= TSOF_QUEUE_OUTPUT; \
		putbq(tpiso->tso_wq,(bp)); }

int
tpisocket_wreject(
	struct tpisocket *tpiso,
	mblk_t	*bp,
	int	tli_prim,
	int	tli_error,
	int	unix_error)
{
	mblk_t	*nbp;
	struct	T_error_ack *terror;

	if (bp != NULL &&
	    (bp->b_datap->db_type == M_PROTO ||
	     bp->b_datap->db_type == M_PCPROTO) &&
	    (bp->b_datap->db_lim - bp->b_datap->db_base) > 
			sizeof(struct T_error_ack)) {
		bp->b_rptr = bp->b_datap->db_base;
		nbp = bp;
		bp = NULL;
	} else {
		nbp = tpisocket_allocb(tpiso->tso_wq,
				       sizeof(struct T_error_ack),
				       BPRI_HI);
		if (nbp == NULL) {
			if (bp)
				TSO_PUTBWQ(tpiso, bp);
			return 0;
		}
	}
	terror = (struct T_error_ack *)nbp->b_rptr;
	nbp->b_wptr = nbp->b_rptr + sizeof(struct T_error_ack);
	nbp->b_datap->db_type = M_PCPROTO;
	nbp->b_band = 1;
	terror->ERROR_prim = tli_prim;
	terror->PRIM_type = T_ERROR_ACK;
	terror->TLI_error = tli_error;
	terror->UNIX_error = unix_error;
#ifdef _TPI_DEBUG
	if (_tpiprintfs) {
		printf("K: tpiso 0x%x, error ack %d %d %d\n",
			tpiso, tli_prim, tli_error, unix_error);
	}
#endif	/* _TPI_DEBUG */
	qreply(tpiso->tso_wq, nbp);
	if (bp)
		freemsg(bp);
	return 1;
}

static int
tpisocket_send_ok_message(tpiso, so, bp)
	struct tpisocket *tpiso;
	struct socket *so;
	mblk_t*bp;
{
	union	T_primitives *pi = (union T_primitives *)bp->b_rptr;

	if (bp->b_cont != NULL) {
		freemsg(bp->b_cont);
		bp->b_cont = NULL;
	}
	tpisocket_send_ok_ack(tpiso, bp, pi->type);

	/*
	 * get tpisocket_rsrv() to recheck the connection queue 
	 */
	tpisocket_event(so, SSCO_INPUT_ARRIVED, tpiso);

	return 1;
}

/*
 * tpisocket_convert_error
 * Convert a unix error code to a tli error code
 */
void
tpisocket_convert_error(int *unix_error_p, int *tli_error_p, int prim)
{
	int unix_error = *unix_error_p;

	if (unix_error < 0) {
		*tli_error_p = - unix_error;
		*unix_error_p = (*tli_error_p == TSYSERR ? EINVAL : 0);
		return;
	}

	switch (unix_error) {
	case EADDRNOTAVAIL:
	case EADDRINUSE:
		/* return TADDRBUSY and let the library sort it out */
		*tli_error_p = TADDRBUSY;
		break;
	case EINVAL:
		if (prim == T_OPTMGMT_REQ) {
			*tli_error_p = TBADOPT;
		} else {
			*tli_error_p = TBADADDR;
		}
		break;
	case EACCES:
		*tli_error_p = TACCES;
		break;
	case ENOTCONN:
	case EALREADY:
		*tli_error_p = TOUTSTATE;
		break;
	default:
		*tli_error_p = TSYSERR;
		break;
	}
}

#ifdef _TPI_DEBUG
static void
hexdump(unsigned char *p, int l)
{
	int i = 0;
	while (l--) {
		printf("%02x ", *p++);
		i++;
		if (i == 16) {
			i = 0;
			printf("\n");
		}
	}
	printf("\n");
}

void
tpi_dump_conn_con(mblk_t *bp)
{
	struct T_conn_con *tc = (struct T_conn_con *)bp->b_rptr;

	printf("pt=%d ro=%d rl=%d ", tc->PRIM_type,
		tc->RES_offset, tc->RES_length);
	printf("ol=%d, oo=%d\n", tc->OPT_length, tc->OPT_offset); 
	hexdump(bp->b_rptr, bp->b_wptr - bp->b_rptr);
}

void
tpi_dump_bind_req(mblk_t *bp)
{
	struct T_bind_req *ureq = (struct T_bind_req *)bp->b_rptr;

	printf("pt=%d ao=%d al=%d ", ureq->PRIM_type,
		ureq->ADDR_offset, ureq->ADDR_length);
	printf("ql=%d\n", ureq->CONIND_number);
	hexdump(bp->b_rptr, bp->b_wptr - bp->b_rptr);
}

void
tpi_dump_unitdata_req(mblk_t *bp)
{
	struct T_unitdata_req *ureq = (struct T_unitdata_req *)bp->b_rptr;

	printf("pt=%d do=%d dl=%d ", ureq->PRIM_type,
		ureq->DEST_offset, ureq->DEST_length);
	printf("ol=%d, oo=%d\n", ureq->OPT_length, ureq->OPT_offset); 
	hexdump(bp->b_rptr, bp->b_wptr - bp->b_rptr);
}

void
tpi_dump_unitdata_ind(mblk_t *bp)
{
	struct T_unitdata_ind *uind = (struct T_unitdata_ind *)bp->b_rptr;

	printf("pt=%d so=%d sl=%d ", uind->PRIM_type,
		uind->SRC_offset, uind->SRC_length);
	printf("ol=%d, oo=%d\n", uind->OPT_length, uind->OPT_offset); 
	hexdump(bp->b_rptr, bp->b_wptr - bp->b_rptr);
}

void
tpi_dump_uderror_ind(mblk_t *bp)
{
	struct T_uderror_ind *uind = (struct T_uderror_ind *)bp->b_rptr;

	printf("pt=%d do=%d dl=%d et=%d", uind->PRIM_type,
		uind->DEST_offset, uind->DEST_length, uind->ERROR_type);
	printf("ol=%d, oo=%d\n", uind->OPT_length, uind->OPT_offset); 
	hexdump(bp->b_rptr, bp->b_wptr - bp->b_rptr);
}

void
tpi_dump_conn_ind(mblk_t *bp)
{
	struct T_conn_ind *tc = (struct T_conn_ind *)bp->b_rptr;

	printf("pt=%d sn=%d so=%d sl=%d ", tc->PRIM_type, tc->SEQ_number, 
		tc->SRC_offset, tc->SRC_length);
	printf("ol=%d, oo=%d\n", tc->OPT_length, tc->OPT_offset); 

	if (tc->SRC_length && tc->SRC_offset) {
		hexdump(((u_char *)tc) + tc->SRC_offset, tc->SRC_length);
	}
	if (tc->OPT_length && tc->OPT_offset) {
		hexdump(((u_char *)tc) + tc->OPT_offset, tc->OPT_length);
	}
}

void
tpi_dump_conn_req(mblk_t *bp)
{
	struct T_conn_req *tc = (struct T_conn_req *)bp->b_rptr;

	printf("pt=%d dl=%d do=%d oo=%d ol=%d\n", 
		tc->PRIM_type, tc->DEST_length, 
		tc->DEST_offset, tc->OPT_offset,
		tc->OPT_length);

	hexdump(bp->b_rptr, bp->b_wptr - bp->b_rptr);
}

void
tpi_dump_optmgmt_req(mblk_t *bp)
{
	struct T_optmgmt_req *tc = (struct T_optmgmt_req *)bp->b_rptr;

	printf("pt=%d ol=%d oo=%d, fl=0x%x\n", 
		tc->PRIM_type, tc->OPT_length, 
		tc->OPT_offset, tc->MGMT_flags);

	hexdump(bp->b_rptr, bp->b_wptr - bp->b_rptr);
}

void
tpi_dump_optmgmt_ack(mblk_t *bp)
{
	struct T_optmgmt_ack *tc = (struct T_optmgmt_ack *)bp->b_rptr;

	printf("pt=%d ol=%d oo=%d, fl=0x%x\n", 
		tc->PRIM_type, tc->OPT_length, 
		tc->OPT_offset, tc->MGMT_flags);

	hexdump(bp->b_rptr, bp->b_wptr - bp->b_rptr);
}

void
tpi_dump_conn_res(mblk_t *bp)
{
	struct T_conn_res *tc = (struct T_conn_res *)bp->b_rptr;

	printf("pt=%d, qp=0x%x, sn=%d, oo=%d, ol=%d\n", tc->PRIM_type,
		tc->QUEUE_ptr, tc->SEQ_number, 
		tc->OPT_offset, tc->OPT_length);

	hexdump(bp->b_rptr, bp->b_wptr - bp->b_rptr);
}
#endif

#ifdef _TPI_DEBUG
/* ARGSUSED */
void
tpi_catchme(struct tpisocket *tpiso, void *ptr)
{
	/* REFERENCED */
	int a = 5;
}
#endif

/*
 * tpisocket_wact
 *	Acts on an output packet.  
 *	If the packet cannot be processed immediately, 
 *	it is put back on the output queue.
 * Returns:
 *	0 if message not completely processed.
 *	non-0 if message finished, ready for next message.
 * Entered with Stream monitor lock held
 */
int
tpisocket_wact(struct tpisocket *tpiso, mblk_t *bp)
{
	struct	tpisocket_control *tpictl = tpiso->tso_control;
	struct	vsocket *vso = tpiso->tso_vsocket;
	struct	socket *so =
	    (struct socket *)(BHV_PDATA(VSOCK_TO_FIRST_BHV(vso)));
	struct	mbuf *m;
	union	T_primitives *pi;
	mblk_t	*saved_opts;
	cred_t	*crp;
	mblk_t	*rbp;
	int	tli_prim;
	int	tli_error;
	int	unix_error;

	TRACEME(0x2e, (void *)tpiso);
	TRACEME(0x2f, (void *)tpiso->tso_vsocket);

	SAVE_LASTPROC_ADDRESS(bp, tpisocket_wact);

	switch (bp->b_datap->db_type) {
	case M_DATA:
		tpiso->tso_flags &= ~TSOF_UNUSED;
		tpiso->tso_pending_send_flags = 0;
		tpiso->tso_pending_send_addr.mblk = NULL;
		tpiso->tso_pending_send_control.mblk = NULL;
		tpiso->tso_pending_send_header = NULL;
		tpiso->tso_pending_send.mblk = bp;
		tpiso->tso_pending_send_error = 0;
		tpiso->tso_flags |= TSOF_SEND_PENDING;
		tpiso->tso_flags &= ~TSOF_UNUSED;
		return tpisocket_send_check(tpiso);

	case M_PCPROTO:
	case M_PROTO:
		break;	/* No need to nest switch statements */

	case M_IOCTL:
	{
		struct	iocblk *iocp;

		iocp = (struct iocblk *)bp->b_rptr;
		switch (iocp->ioc_cmd) {
		case SIOCGETNAME:
			return tpisocket_sioc_name(tpiso, bp, PRU_SOCKADDR);

		case SIOCGETPEER:
			if (!(so->so_state &
					(SS_ISCONNECTED|SS_ISCONFIRMING))) {
				return tpisocket_send_iocnak(tpiso->tso_wq, bp,
						      ENOTCONN);
			}
			return tpisocket_sioc_name(tpiso, bp, PRU_PEERADDR);

#ifdef _SVR4_NOTYET
		case INITQPARMS:
			/* XXX */
			return 1;
#endif /* _SVR4_NOTYET */
		case _XTI_MODE:
			tpiso->tso_flags |= TSOF_XTI;
			bp->b_datap->db_type = M_IOCACK;
			bp->b_band = 1;
			(void)qreply(tpiso->tso_wq, bp);
			return 0;

		case TI_GETMYNAME:
			return tpisocket_ti_name(tpiso, bp);

		case TI_GETPEERNAME:
			return tpisocket_ti_name(tpiso, bp);

		case SIOCGETLABEL:
			{
			/* force copy of label to be atomic */
			SOCKET_LOCK(so);
			bcopy((void *)sesmgr_get_label(so),
				bp->b_cont->b_rptr,
				sizeof(mac_label *));
			SOCKET_UNLOCK(so);
			bp->b_datap->db_type = M_IOCACK;
			bp->b_band = 1;
			(void)qreply(tpiso->tso_wq,bp);
			return(0);
			}

		case SIOCSETLABEL:
			{
			mac_label * new_label;
			int error;
                        if (iocp->ioc_count != sizeof(mac_label *))
                                error = EINVAL;
                        else {
                                bcopy(bp->b_cont->b_rptr,
                                        (char *)&new_label,
                                        sizeof(mac_label *));
				SOCKET_LOCK(so);
				error = (*so->so_proto->pr_usrreq)(so,
						PRU_SOCKLABEL,
						(struct mbuf *)0,
						(struct mbuf *) new_label,
						(struct mbuf *)0);
				SOCKET_UNLOCK(so);
			}
			iocp->ioc_error = error;
                        bp->b_datap->db_type = M_IOCACK;
			bp->b_band = 1;
                        (void)qreply(tpiso->tso_wq,bp);
                        return(0);
			}

		default:
			break;
		}
		if (IOCGROUP(iocp->ioc_cmd) == 'i')
			return tpisocket_ifioctl(tpiso, bp);
		if (IOCGROUP(iocp->ioc_cmd) == 'r')
			return tpisocket_rtioctl(tpiso, bp);
		return tpisocket_send_iocnak(tpiso->tso_wq, bp, EINVAL);
	}

	case M_IOCDATA:
		if (!(tpiso->tso_flags & TSOF_IOCTL_PENDING)) {
			freemsg(bp);
			return 1;
		}
		switch (tpiso->tso_pending_ioctl) {
		case TSOI_TI_NAME:
			return tpisocket_ti_name(tpiso, bp);
		case TSOI_IFIOCTL:
			return tpisocket_ifiocdata(tpiso, bp);
		case TSOI_RTIOCTL:
			return tpisocket_rtiocdata(tpiso, bp);
		default:
			break;
		}
		freemsg(bp);
		return 1;

	case M_CTL:
	default:
		freemsg(bp);
		return 1;
	}

	/*
	 * Process M_PROTO and M_PCPROTO
	 */
	pi = (union T_primitives *)bp->b_rptr;
	switch (pi->type) {

	case T_CONN_REQ:	/* connection request     */
#ifdef _TPI_DEBUG
		if (_tpiprintfs) {
			printf("K: tpiso 0x%x conn_req %d\n", tpiso, 
					tpiso->tso_state);
			if (_tpiprintfs > 1) {
				tpi_dump_conn_req(bp);
			}
		}
#endif	/* _TPI_DEBUG */
		if (tpiso->tso_state != TS_IDLE) {
			return tpisocket_wreject(tpiso, bp, pi->type, 
				TOUTSTATE, 0);
		}
		rbp = tpisocket_allocb(tpiso->tso_wq, sizeof(struct T_ok_ack), 
				BPRI_MED);
		if (rbp == NULL) {
			TSO_PUTBWQ(tpiso, bp);
			return 0;
		}
		if (bp->b_cont) { 
			/* no data supported */
			freemsg(bp->b_cont);
			bp->b_cont = (mblk_t *)0;
			freemsg(rbp);
			return tpisocket_wreject(tpiso, bp, pi->type, 
				TBADDATA, 0);
		}
		if (pi->conn_req.DEST_length < 8) { /* family+port+addr */
			freemsg(rbp);
			return tpisocket_wreject(tpiso, bp, pi->type, 
				TBADADDR, 0);
		}
		SOCKET_LOCK(so);
		tli_error = tpisocket_process_assoc_options(tpiso, so, bp,
			bp->b_rptr + pi->conn_req.OPT_offset,
			pi->conn_req.OPT_length, &saved_opts, &unix_error);
		SOCKET_UNLOCK(so);
		if (tli_error) {
			freemsg(rbp);
			return tpisocket_wreject(tpiso, bp, pi->type, 
				tli_error, unix_error);
		}
		m = tpisocket_copy_to_mbuf(
			bp->b_rptr + pi->conn_req.DEST_offset,
			min(pi->conn_req.DEST_length,
			    (bp->b_wptr - bp->b_rptr) -
			       pi->conn_req.DEST_offset),
			MT_SONAME, &unix_error);

		if (m == NULL) {
			freemsg(rbp);
			if (unix_error == ENOBUFS) {
				tli_error = TSYSERR;
				unix_error = ENOSR;
			} else {
				tli_error = TBADADDR;
				unix_error = 0;
			}
			freemsg(saved_opts);
			return tpisocket_wreject(tpiso, bp, pi->type, 
				tli_error, unix_error);
		}
		SOCKET_LOCK(so);
/* SCA */
		if (so->so_options & SO_ACCEPTCONN) {
			so->so_options &= ~SO_ACCEPTCONN;
		}
		/* XXX should be VSOP_CONNECT, right? */
		unix_error = soconnect(so, m);
		SOCKET_UNLOCK(so);
		m_freem(m);

		tpiso->tso_flags &= ~TSOF_UNUSED;

		if (!unix_error) { /* Connection request succeeded */
			tpiso->tso_saved_conn_opts = saved_opts;
			/*
			 * send connection request ack message back
			 */
			tpisocket_send_ok_ack(tpiso, rbp, pi->type);
			tpiso->tso_state = TS_WCON_CREQ;
			/* waiting for a real response */
			tpiso->tso_flags |= TSOF_CONN_REQ_PENDING;
			tpiso->tso_pending_request = bp;
			tpisocket_conn_req_done(tpiso);
			return 1;
		} else {
			freemsg(saved_opts);
			/* Connection request failed */
			tpiso->tso_flags &= ~TSOF_CONN_REQ_PENDING;
			tpiso->tso_flags |= TSOF_CONN_REQ_FAILED;
			tpiso->tso_pending_request = (mblk_t *)NULL;

			if (tpiso->tso_flags & TSOF_RECV_DISCON) {
				ASSERT(tpiso->tso_state == TS_IDLE);
				freemsg(bp); /* discard the message */
				return 1;
			}
			tpiso->tso_state = TS_IDLE;

			/*
			 * Received a socket error.  So return an error message 
			 * with the errno inside when we haven't already
			 * received a disconnect indication.
			 */
			switch (unix_error) {
			case EACCES:
				tli_error = TACCES;
				unix_error = 0;
				break;
			case EINVAL:
				tli_error = TBADADDR;
				unix_error = 0;
				break;
			case EADDRINUSE:
				tli_error = TADDRBUSY;
				unix_error = 0;
				break;
			case EADDRNOTAVAIL:
				tli_error = TSYSERR;
				break;
			default:
				tli_error = TSYSERR;
				break;
			}
			tpisocket_wreject(tpiso, bp, pi->type, tli_error, 
					unix_error);
		}
		return 1;

	case T_CONN_RES:	/* connection response    */
	{
		int	i;
		struct	tpisocket *ntpiso;
		struct	vsocket	*ntvso;
		struct	socket	*ntso;
		struct	vsocket *vnso;
		struct	socket *nso;
		struct	vsocket *voso;
		struct	socket *oso;
		struct	vsocket *vhead;
		struct	socket *head;
		int ss;

		/* tpiso is the listen TPIsocket, its so_q holds the socket
		 * 	for the not-yet-accepted connection.
		 * ntpiso is the TPIsocket newly created to become the
		 *	endpoint for the not-yet-accepted connection. 
		 * oso is the socket that was attached to ntpiso when
		 * 	ntpiso was created.  It will be discarded.
		 * nso is the socket on tpiso's so_q.  It will be removed
		 *	from the queue, and attached to ntpiso.
		 */

		/*
		 * This is a response to a T_CONN_IND, so presume it is
		 * no longer queued.
		 */
		ASSERT((tpiso->tso_flags & TSOF_XTI) || 
			(tpiso->tso_outcnt < 2));

		/* check for a valid tpisocket queue */
		ntpiso = NULL;

		for (i = 0; i < TSC_MAXDEVICES; i++) {
			if (TSC_DEVICES[i] == NULL ||
			    TSC_DEVICES[i]->tso_rq == NULL)
				continue;
			if (TSC_DEVICES[i]->tso_rq == (queue_t*)
					pi->conn_res.QUEUE_ptr) {
				ntpiso = TSC_DEVICES[i];
				break;
			}
		}
#if _MIPS_SIM == _ABI64
		/*
		 * XXX there has to be a better way, but the stream head has
		 * probably swizzled this.
		 */
		if (!ABI_IS_IRIX5_64(tpiso->tso_abi) &&
		    pi->conn_res.OPT_offset) {
			pi->conn_res.OPT_offset = sizeof(struct T_conn_res) - 4;
		}
#endif
#ifdef _TPI_DEBUG
		if (_tpiprintfs) {
	printf("K: T_CONN_RES tpiso 0x%x, state %d, ntpiso 0x%x state %d\n",
			tpiso, tpiso->tso_state, ntpiso,
			ntpiso ? ntpiso->tso_state : 0);
			if (_tpiprintfs > 1) {
				tpi_dump_conn_res(bp);
			}
		}
#endif
		/* check queue state */
		if (ntpiso == NULL ||
		    ntpiso->tso_flags & (TSOF_OPENING|TSOF_CLOSING)) {
			return tpisocket_wreject(tpiso, bp, pi->type, TBADF,0);
		}

                /*
                 * If we're already using this for a connection or fd is
                 * not a listening endpoint, fail.
                 */
		ntvso = ntpiso->tso_vsocket;
		ntso = (struct socket *)(BHV_PDATA(VSOCK_TO_FIRST_BHV(ntvso)));
#ifdef _TPI_DEBUG
		if (_tpiprintfs) {
			printf("K: ntso = 0x%x\n", ntso);
		}
#endif
                if ((ntso->so_state &
                        (SS_ISCONNECTED | SS_ISCONNECTING |
                         SS_ISDISCONNECTING | SS_ISCONFIRMING)) ||
                    !(so->so_options & SO_ACCEPTCONN)) {
                        return tpisocket_wreject(tpiso, bp, pi->type,
                                TOUTSTATE, 0);
                }

                if ((tpiso != ntpiso) &&
		    (ntpiso->tso_outcnt > 0)) {
                        return tpisocket_wreject(tpiso, bp, pi->type,
                                TOUTSTATE, 0);
                }

                /* if resfd is not the same as fd, it can't be listening */
                if ((tpiso != ntpiso) &&
                    (ntso->so_options & SO_ACCEPTCONN)) {
                        return tpisocket_wreject(tpiso, bp, pi->type,
                                TRESQLEN, 0);
                }

                /* resfd must be unbound or idle */
                if (!(ntpiso->tso_flags & TSOF_UNUSED) &&
                    !(ntpiso->tso_flags & TSOF_ADDRESS_BOUND)) {
                        return tpisocket_wreject(tpiso, bp, pi->type,
                                TOUTSTATE, 0);
                }

		/*
		 * If ntpiso == tpiso, then there can be only one connection
		 * pending.  If not equal, then there must be no connections
		 * pending on ntpiso (all pending connections are on tpiso).
		 */
		vhead = ntpiso->tso_vsocket;
		head=  (struct socket *)(BHV_PDATA(VSOCK_TO_FIRST_BHV(vhead)));

		SOCKET_LOCK(head);
		TRACEME(0x30, (void *)head);

		if (head->so_qlen != (ntpiso == tpiso)) {
			SOCKET_UNLOCK(head);
			TRACEME(0x31, (void *)head);
			return tpisocket_wreject(tpiso, bp, pi->type, 
				TINDOUT, 0);
		}
		SOCKET_UNLOCK(head);
		TRACEME(0x32, (void *)head);

		/*
		 * Check if the specified connection is still there
		 */
restart_conn:
		vhead = tpiso->tso_vsocket;
		head = (struct socket *)(BHV_PDATA(VSOCK_TO_FIRST_BHV(vhead)));
#ifdef _TPI_DEBUG
		if (_tpiprintfs) {
			printf("K: head = 0x%x\n", head);
		}
#endif
		SOCKET_LOCK(head);

		TRACEME(0x33, (void *)head);
		for (nso = head->so_q; nso != NULL && nso != head;
		     nso = nso->so_q) {

			if (!SOCKET_TRYLOCK(nso)) {
				SOCKET_UNLOCK(head);
				nano_delay(&backoff);
				goto restart_conn;
			}
			TRACEME(0x34, (void *)nso);
			if (((int)(__psunsigned_t)nso->so_data ==
				pi->conn_res.SEQ_number)) {
				break;
			}
/*SCA*/			SOCKET_UNLOCK(nso);
		}

		/*
		 * We arrive here with the so and nso locks held
		 */
		if (nso == NULL || nso == head ||
		   !(nso->so_state & SS_TPISOCKET)) {
			if ((nso != NULL) && (nso != head)) {
				SOCKET_UNLOCK(nso);
				TRACEME(0x35, (void *)nso);
			}
			SOCKET_UNLOCK(head);
			TRACEME(0x36, (void *)head);
			return tpisocket_wreject(tpiso, bp,pi->type,TBADSEQ,0);
		}

		/*
		 * Process options, if any, prior to messing with the
		 * socket queue.
		 */
		tli_error = tpisocket_process_assoc_options(ntpiso, nso, bp,
			bp->b_rptr + pi->conn_res.OPT_offset,
			pi->conn_res.OPT_length, (mblk_t **)0, &unix_error);
		if (tli_error) {
			if ((nso != NULL) && (nso != head)) {
				SOCKET_UNLOCK(nso);
				TRACEME(0x35, (void *)nso);
			}
			SOCKET_UNLOCK(head);
			TRACEME(0x98, (void *)head);
			return tpisocket_wreject(tpiso, bp,pi->type, tli_error,
				unix_error);
		}
			
		/*
		 * remove the socket from the queue
		 */
		{
			int s1;
			SOCKET_QLOCK(head, s1);
			soqremque(nso, 1);
			SOCKET_QUNLOCK(head, s1);
		}
		tpiso->tso_outcnt--;
		tpiso->tso_flags &= ~TSOF_UNUSED;

		/* swap the new socket for the old one */
		voso = ntpiso->tso_vsocket;
		oso = (struct socket *)(BHV_PDATA(VSOCK_TO_FIRST_BHV(voso)));
		oso->so_state        &= ~SS_WANT_CALL;
		oso->so_snd.sb_flags &= ~SB_NOTIFY;
		unix_error = vsowrap(nso, &vnso, AF_INET, nso->so_type, 
				     nso->so_proto->pr_protocol);
		ntpiso->tso_vsocket    = vnso;
#ifdef _TPI_DEBUG
		if (_tpiprintfs) {
		 printf("K: tpiso 0x%x ntpiso 0x%x conn_res into data_xfer\n", 
				tpiso, ntpiso);
			printf("K: nso = 0x%x, so=0x%x\n", nso, so);
		}
#endif	/* _TPI_DEBUG */

		ntpiso->tso_state      = TS_DATA_XFER;
		ntpiso->tso_flags    &= ~TSOF_UNUSED;
		ntpiso->tso_flags    |= TSOF_ADDRESS_BOUND;
		nso->so_options		&= ~SO_ACCEPTCONN;  /* make sure */
		nso->so_callout       = tpisocket_event;
		nso->so_callout_arg   = (caddr_t)ntpiso;
		ASSERT(ntpiso->tso_rq->q_monpp);
		nso->so_monpp = ntpiso->tso_rq->q_monpp;
		/*
		 * Note: this implementation uses the same bit value
		 * for SS_CONN_IND_SENT and SS_TPISOCKET, since the
		 * field is only a short. Once the socket is attached to
		 * a TPI data structure, it takes on the second meaning.
		 */
#if (SS_TPISOCKET != SS_CONN_IND_SENT)
		nso->so_state        &= ~SS_CONN_IND_SENT;
#endif
		nso->so_state        |= (SS_WANT_CALL | SS_TPISOCKET);
		nso->so_snd.sb_flags |= SB_NOTIFY;
		nso->so_state        &= ~SS_NOFDREF;

		/* queue old socket for discard */
#if 0
		/* enable when tested */
		ASSERT(voso->vs_refcnt == 1);
#endif
		bhv_remove(&(voso->vs_bh), &(oso->so_bhv));
		vsocket_release(voso);
		ss = mp_mutex_spinlock(&tpisocket_discard_lock);
		oso->so_callout_arg = (caddr_t)tpisocket_discard_list; 
		tpisocket_discard_list = oso;
		mp_mutex_spinunlock(&tpisocket_discard_lock, ss);

#ifdef TPISOCKET_DAEMON
		wakeup(&tpisocket_discard_list);
#endif
		/*
		 * wakeup the new socket to look at the current state
		 */
		tpisocket_event(nso, SSCO_INPUT_ARRIVED, ntpiso);
		tpisocket_event(nso, SSCO_OOB_INPUT_ARRIVED, ntpiso);
		tpisocket_event(nso, SSCO_OUTPUT_AVAILABLE, ntpiso);

		SOCKET_UNLOCK(nso);
		TRACEME(0x37, (void *)nso);
		SOCKET_UNLOCK(head);
		TRACEME(0x38, (void *)head);

		/* send the "ok" message */
		/* (reuse the old message) */
		if ((ntpiso != tpiso) && (tpiso->tso_outcnt == 0)) {
			tpiso->tso_state = TS_IDLE;
		}
		return (tpisocket_send_ok_message(tpiso, so, bp));
	}

	case T_DISCON_REQ:	/* disconnect request     */
	{
		struct	socket *oso;

		if (bp->b_cont) {
			freemsg(bp->b_cont);
			bp->b_cont = (mblk_t *)0;
			return tpisocket_wreject(tpiso, bp, pi->type,
				TBADDATA, 0);
		}
		/* which kind of disconnect? */
		if (so->so_options & SO_ACCEPTCONN) {

#ifdef _TPI_DEBUG
			if (_tpiprintfs) {
		printf("K: tpiso 0x%x pending disc request in state %d\n",
				tpiso, tpiso->tso_state);
			}
#endif
			/*
			 * This is a response to a T_CONN_IND, 
			 * so presume it is no longer queued.
			 */
			ASSERT((tpiso->tso_flags & TSOF_XTI) ||
			       (tpiso->tso_outcnt < 2));

			/*
			 * need a valid sequence number
			 */
			SOCKET_LOCK(so);
			TRACEME(0x39, (void *)so);
restart_disconn:
			for (oso = so->so_q;
			     oso != NULL && oso != so;
			     oso = oso->so_q) {
				TRACEME(0x3a, (void *)oso);
				if (!SOCKET_TRYLOCK(oso)) { /* failed restart*/
					SOCKET_UNLOCK(so);
					TRACEME(0x3b, (void *)so);
					nano_delay(&backoff);
					SOCKET_LOCK(so);
					goto restart_disconn;
				}
				TRACEME(0x3c, (void *)oso);
				if (!(oso->so_state & SS_TPISOCKET)) {
					SOCKET_UNLOCK(oso);
					TRACEME(0x3d, (void *)oso);
					continue;
				}
				if ((int)(__psunsigned_t)oso->so_data == 
					pi->discon_req.SEQ_number) {
					break;
				}
/*SCA*/				SOCKET_UNLOCK(oso);
			}
			if (oso == NULL || oso == so ||
			    !(oso->so_state & SS_TPISOCKET)) {
				SOCKET_UNLOCK(so);
				TRACEME(0x3e, (void *)so);
				return tpisocket_wreject(tpiso, bp, pi->type, 
					TBADSEQ, 0);
			}

			tpiso->tso_flags &= ~TSOF_UNUSED;
			/*
			 * dequeue the socket
			 */
			{
				int s1;
				SOCKET_QLOCK(oso, s1);
				soqremque(oso, 1);
				SOCKET_QUNLOCK(oso, s1);
			}
			tpiso->tso_outcnt--;
			oso->so_state &= ~SS_CONN_IND_SENT;
 
			/* discard the socket, releases lock on oso */
			TRACEME(0x3f, (void *)oso);
			if (soabort(oso)) {
				SOCKET_UNLOCK(oso);
			}
			TRACEME(0x40, (void *)oso);

			/*
			 * get tpisocket_rsrv() to recheck connection queue 
			 */
			tpisocket_event(so, SSCO_INPUT_ARRIVED, tpiso);

			SOCKET_UNLOCK(so);
			TRACEME(0x41, (void *)so);

			/* send the "ok" message */
			if (tpiso->tso_outcnt == 0) {
				tpiso->tso_state = TS_IDLE;
			}
			return (tpisocket_send_ok_message(tpiso, so, bp));
		}

		switch (tpiso->tso_state) {
		case TS_DATA_XFER:
		case TS_WIND_ORDREL:
		case TS_WREQ_ORDREL:
			tpisocket_flushq(tpiso, FLUSHRW);
			break;
		default:
			break;
		}
		rbp = tpisocket_allocb(tpiso->tso_wq, sizeof(struct T_ok_ack),
			       BPRI_MED);
		if (rbp == NULL) {
			TSO_PUTBWQ(tpiso, bp);
			return 0;
		}

#ifdef _TPI_DEBUG
		if (_tpiprintfs) {
		 printf("K: tpiso 0x%x disconnect request in state %d\n",
			tpiso, tpiso->tso_state);
		}
#endif	/* _TPI_DEBUG */

		SOCKET_LOCK(so);
		TRACEME(0x42, (void *)so);

		switch (tpiso->tso_state) {
		case TS_DATA_XFER:
		case TS_WIND_ORDREL:
		case TS_WREQ_ORDREL:
			break;
		default:
			freemsg(rbp);
			SOCKET_UNLOCK(so);

#ifdef _TPI_DEBUG
			if (_tpiprintfs) {
		printf("K: tpiso 0x%x rej disconnect request in state %d\n",
				tpiso, tpiso->tso_state);
				if (tpiso->tso_state == 0) {
					printf("zero state tpiso = 0x%x\n",
					tpiso);
					tpi_catchme(tpiso, (void *)0);
				}
			}
#endif	/* _TPI_DEBUG */
			return tpisocket_wreject(tpiso, bp, pi->type,
					TOUTSTATE, 0);
		}
		/* ensure RST sent instead of FIN */
		so->so_options |= SO_LINGER;
		so->so_linger = 0;
		unix_error = sodisconnect(so);
		SOCKET_UNLOCK(so);
		TRACEME(0x43, (void *)so);

		tpiso->tso_flags &= ~TSOF_UNUSED;
		tpiso->tso_flags |= TSOF_DISCON_REQ_PENDING;

#if defined(WRONG) && (WRONG == RIGHT)
		/* send ok acknowledgement */
		tpisocket_send_ok_ack(tpiso, rbp, pi->type);
#else
		freemsg(rbp);	/* why do we have an rbp ?? */
#endif

		freemsg(bp);
		tpiso->tso_state = TS_IDLE;
		tpisocket_discon_req_done(tpiso);
		return 1;
	}

	case T_DATA_REQ:
		tpiso->tso_pending_send_flags =
			(pi->data_req.MORE_flag ? 0 : MSG_EOR);
#ifdef _TPI_DEBUG
		if (_tpiprintfs) {
			printf("K: tpiso 0x%x datareq %d bytes\n",
				tpiso, msgdsize(bp));
		}
#endif	/* _TPI_DEBUG */
		tpiso->tso_pending_send_addr.mblk = NULL;
		tpiso->tso_pending_send_control.mblk = NULL;
		tpiso->tso_pending_send_header = bp;
		tpiso->tso_pending_send.mblk = bp->b_cont;
		bp->b_cont = NULL;
		tpiso->tso_pending_send_error = 0;
		tpiso->tso_flags |= TSOF_SEND_PENDING;
		tpiso->tso_flags &= ~TSOF_UNUSED;
		return tpisocket_send_check(tpiso);

	case T_EXDATA_REQ:	/* expedited data request */

#ifdef _TPI_DEBUG
		if (_tpiprintfs) {
			printf("K: tpiso 0x%x exdatareq %d bytes\n",
				tpiso, msgdsize(bp));
		}
#endif	/* _TPI_DEBUG */
		tpiso->tso_pending_send_flags =
			(pi->exdata_req.MORE_flag ? 0 : MSG_EOR) | MSG_OOB;

		tpiso->tso_pending_send_addr.mblk = NULL;
		tpiso->tso_pending_send_control.mblk = NULL;
		tpiso->tso_pending_send_header = bp;
		tpiso->tso_pending_send.mblk = bp->b_cont;
		bp->b_cont = NULL;
		tpiso->tso_pending_send_error = 0;
		tpiso->tso_flags |= TSOF_SEND_PENDING;
		tpiso->tso_flags &= ~TSOF_UNUSED;
		return tpisocket_send_check(tpiso);

	case T_INFO_REQ:	/* information request    */
	{
		struct T_info_ack *ia;

		rbp = tpisocket_allocb(tpiso->tso_wq,
			       sizeof(struct T_info_ack),
			       BPRI_MED);
		if (rbp == NULL) {
			TSO_PUTBWQ(tpiso, bp);
			return 0;
		}
		ia = (struct T_info_ack *)rbp->b_rptr;
		rbp->b_wptr = rbp->b_rptr + sizeof(struct T_info_ack);
		rbp->b_datap->db_type = M_PCPROTO;
		rbp->b_band = 1;
		ia->PRIM_type  = T_INFO_ACK;
		ia->TSDU_size  = TSC_TSDU_SIZE; 
		ia->ETSDU_size = TSC_ETSDU_SIZE;
		ia->CDATA_size = TSC_CDATA_SIZE;
		ia->DDATA_size = TSC_DDATA_SIZE;
		ia->ADDR_size  = TSC_ADDRESS_SIZE(tpiso, 
					TSCA_REMOTE_ADDRESS, NULL);
		if (ia->ADDR_size == 0)
			ia->ADDR_size = -1;
		ia->OPT_size = TSC_ADDRESS_SIZE(tpiso, TSCA_OPTIONS, 
					NULL);
		if (ia->OPT_size == 0)
			ia->OPT_size = -1;
		ia->TIDU_size = TSC_TIDU_SIZE;
		ia->SERV_type = TSC_TRANSPORT_TYPE;
		ia->CURRENT_state = tpiso->tso_state;
		ia->PROVIDER_flag = TSC_PROVIDER_FLAG | XPG4_1;
		putnext(tpiso->tso_rq, rbp);
		break;
	}

	case T_ADDR_REQ:	/* address request */
	{
		int llen = 0, rlen = 0, roff = 0;
		struct T_addr_ack *tack;

		/* Assume both addresses when allocating reply */
		rbp = tpisocket_allocb(tpiso->tso_wq,
			       (sizeof(struct T_addr_ack) +
			       TSC_ADDRESS_SIZE(tpiso, TSCA_REMOTE_ADDRESS,
			       NULL) +
			       TSC_ADDRESS_SIZE(tpiso, TSCA_LOCAL_ADDRESS,
			       NULL)), BPRI_MED);
		if (rbp == NULL) {
			TSO_PUTBWQ(tpiso, bp);
			return 0;
		}
		if (tpiso->tso_flags & TSOF_ADDRESS_BOUND) {
			llen = TSC_ADDRESS_SIZE(tpiso, TSCA_LOCAL_ADDRESS, 
					NULL);
			if (TSC_TRANSPORT_TYPE != T_CLTS) {
				/* T_CLTS does not provide return address */
				rlen = TSC_ADDRESS_SIZE(tpiso, 
					TSCA_REMOTE_ADDRESS, NULL);
				roff = llen + sizeof(*tack);
			}
		}
		rbp->b_datap->db_type = M_PCPROTO;
		rbp->b_band = 1;
		tack = (struct T_addr_ack *)rbp->b_rptr;
		tack->PRIM_type = T_ADDR_ACK;
		tack->LOCADDR_length = llen;
		tack->LOCADDR_offset = llen ? sizeof(*tack) : 0;
		tack->REMADDR_length = rlen;
		tack->REMADDR_offset = roff;
		rbp->b_wptr += sizeof(*tack);
		if (llen) {
			(void)TSC_CONVERT_ADDRESS(tpiso, (char *)rbp->b_wptr,
				TSCA_LOCAL_ADDRESS | TSCA_MODE_READ,
				llen, NULL, NULL);
			rbp->b_wptr += llen;
		}
		if (rlen) {
			(void)TSC_CONVERT_ADDRESS(tpiso, (char *)rbp->b_wptr,
				TSCA_REMOTE_ADDRESS | TSCA_MODE_READ,
				rlen, NULL, NULL);
			rbp->b_wptr += rlen;
		}
		putnext(tpiso->tso_rq, rbp);
		break;

	}
	case T_BIND_REQ:	/* bind request		  */
	{
		int	size;
		/* REFERENCED */
		int	error1;
		unsigned int addr_len;

#ifdef _TPI_DEBUG
		if (_tpiprintfs) {
			printf("K: tpiso 0x%x bindreq, state %d\n",
				tpiso, tpiso->tso_state);
			if (_tpiprintfs > 1) {
				tpi_dump_bind_req(bp);
			}
		}
#endif	/* _TPI_DEBUG */

		if (tpiso->tso_state != TS_UNBND) {
			return tpisocket_wreject(tpiso, bp, pi->type, 
				TOUTSTATE, 0);
		}

		size = TSC_ADDRESS_SIZE(tpiso, TSCA_LOCAL_ADDRESS, NULL)
			+ sizeof(struct T_bind_ack);
		rbp = tpisocket_allocb(tpiso->tso_wq,
			max(sizeof(struct T_error_ack), size), BPRI_MED);
		if (rbp == NULL) {
			TSO_PUTBWQ(tpiso, bp);
			return 0;
		}
		rbp->b_datap->db_type = M_PCPROTO;
		rbp->b_band = 1;

		if (pi->bind_req.ADDR_length > 0) {
			/*
			 * get mbuf to hold address for bind operation
			 */
			if ((m = m_get(M_DONTWAIT, MT_DATA)) == NULL) {
                                freemsg(rbp);
                                TSO_PUTBWQ(tpiso, bp);
                                return 0;
                        }
			/*
			 * copy address into mbuf for bind operation
			 */
			addr_len = pi->bind_req.ADDR_length;
			bcopy((caddr_t)(bp->b_rptr + pi->bind_req.ADDR_offset),
				mtod(m, caddr_t),
				addr_len);
			m->m_len += addr_len;

                } else if (TSC_FLAGS & TSCF_DEFAULT_ADDRESS) {

			if ((m = m_get(M_DONTWAIT, MT_DATA)) == NULL) {
                                freemsg(rbp);
                                TSO_PUTBWQ(tpiso, bp);
                                return 0;
                        }
                        if (TSC_CONVERT_ADDRESS(tpiso, NULL,
				TSCA_DEFAULT_ADDRESS | TSCA_MODE_TO_SOCKETS,
                                -1, m, NULL)) {
					m_freem(m);
					m = NULL;
                        }
		} else
			m = NULL;

		vso = tpiso->tso_vsocket;
		so = (struct socket *)(BHV_PDATA(VSOCK_TO_FIRST_BHV(vso)));

		/* switch credentials so that we can validate correctly */
		crp = KTOP_GET_CURRENT_CRED();
		KTOP_SET_CURRENT_CRED(tpiso->tso_cred);

		VSOP_BIND(vso, m, unix_error);
		if (m != NULL) {
			m_freem(m);
		}
		KTOP_SET_CURRENT_CRED(crp);
		if (unix_error) {
			tpisocket_convert_error(&unix_error, &tli_error,
				T_BIND_REQ);
			(void)tpisocket_wreject(tpiso, rbp, T_BIND_REQ,
					tli_error, unix_error);
			break;
		}
		if (pi->bind_req.ADDR_offset > 0 ||
		    pi->bind_req.CONIND_number > 0)
			tpiso->tso_flags &= ~TSOF_UNUSED;
		tpiso->tso_flags |= TSOF_ADDRESS_BOUND;

		if (pi->bind_req.CONIND_number > 0) {
			VSOP_LISTEN(vso, pi->bind_req.CONIND_number, error1);
			tpiso->tso_flags &= ~TSOF_UNUSED;
		}
		rbp->b_wptr = rbp->b_rptr + size;

		pi = (union T_primitives *)rbp->b_rptr;
		pi->bind_ack.PRIM_type = T_BIND_ACK;
		pi->bind_ack.ADDR_length = size - sizeof(struct T_bind_ack);
		pi->bind_ack.ADDR_offset = sizeof(struct T_bind_ack);
 		pi->bind_ack.CONIND_number = so->so_qlimit;

		if (pi->bind_ack.ADDR_length > 0) {
			(void)TSC_CONVERT_ADDRESS(tpiso,
				(char *)rbp->b_rptr + pi->bind_ack.ADDR_offset,
				TSCA_LOCAL_ADDRESS | TSCA_MODE_READ,
				pi->bind_ack.ADDR_length,
				NULL, NULL);
		}
		tpiso->tso_state = TS_IDLE;
		putnext(tpiso->tso_rq, rbp);
		break;
	}

	case T_UNBIND_REQ:	/* unbind request */

#ifdef _TPI_DEBUG
		if (_tpiprintfs) {
			printf("K: tpiso 0x%x unbindbindreq, state %d\n",
				tpiso, tpiso->tso_state);
		}
#endif	/* _TPI_DEBUG */

		SOCKET_LOCK(so);
		TRACEME(0x44, (void *)so);

		if (tpiso->tso_state != TS_IDLE) {
			SOCKET_UNLOCK(so);
			TRACEME(0x45, (void *)so);
			return tpisocket_wreject(tpiso, bp, pi->type, 
				TOUTSTATE, 0);
		}
		SOCKET_UNLOCK(so);
		TRACEME(0x46, (void *)so);

		rbp = tpisocket_allocb(tpiso->tso_wq,
				sizeof(struct T_error_ack), BPRI_MED);
		if (rbp == NULL) {
			TSO_PUTBWQ(tpiso, bp);
			return 0;
		}
		rbp->b_datap->db_type = M_PCPROTO;
		rbp->b_band = 1;
		freemsg(bp);
		bp = rbp;
		pi = (union T_primitives *)bp->b_rptr;
		pi->type = T_UNBIND_REQ;
		tli_prim = pi->type;
		unix_error = TSC_UNBIND(tpiso);
		if (unix_error) {
			if (unix_error == EINVAL) {
				tli_error = TOUTSTATE;
				unix_error = 0;
			} else {	
				tli_error = TSYSERR;
			}
			return tpisocket_wreject(tpiso, bp, tli_prim,
					tli_error, unix_error);
		}
		tpiso->tso_flags &= ~TSOF_ADDRESS_BOUND;
		tpiso->tso_state = TS_UNBND;
		tpisocket_flushq(tpiso, FLUSHRW);
		return (tpisocket_send_ok_message(tpiso, so, bp));

	case T_UNITDATA_REQ:	/* unitdata request       */
	{
		mblk_t *ap;
#ifdef _TPI_DEBUG
		if (_tpiprintfs) {
			printf("K: tpiso 0x%x udatareq %d bytes, state %d\n",
				tpiso, msgdsize(bp), tpiso->tso_state);
			if (_tpiprintfs > 1) {
				tpi_dump_unitdata_req(bp);
			}
		}
#endif	/* _TPI_DEBUG */

		if (pi->unitdata_req.DEST_length < 8) {
			tpiso->tso_pending_send_addr.mblk = NULL;
			tpiso->tso_pending_send_error = TBADADDR;
			tpiso->tso_pending_send_header = bp;
			tpiso->tso_pending_send.mblk = bp->b_cont;
			bp->b_cont = NULL;
			tpiso->tso_flags |= TSOF_SEND_PENDING;
			tpiso->tso_flags &= ~TSOF_UNUSED;
			return tpisocket_send_check(tpiso);
		}
#ifdef LATER
		if ((ap = dupb(bp)) == NULL) { /* failed so put back & retry */
			TSO_PUTBWQ(tpiso, bp);
			return 0;
		}
		ap->b_rptr += pi->unitdata_req.DEST_offset;
		ap->b_wptr = ap->b_rptr + pi->unitdata_req.DEST_length;
#else
		ap = allocb((bp->b_datap->db_lim - bp->b_datap->db_base),
			BPRI_MED);
		if (ap == NULL) { /* failed so put back to retry */
			TSO_PUTBWQ(tpiso, bp);
			return 0;
		}
		ap->b_flag = bp->b_flag;
		ap->b_band = bp->b_band;
		bcopy(bp->b_rptr + pi->unitdata_req.DEST_offset,
			ap->b_rptr,
			pi->unitdata_req.DEST_length);

		ap->b_wptr += pi->unitdata_req.DEST_length;
		ap->b_datap->db_type = bp->b_datap->db_type;
#endif /* LATER */
		tpiso->tso_pending_send_flags = MSG_EOR;
		tpiso->tso_pending_send_addr.mblk = ap;
		tpiso->tso_pending_send_control.mblk = NULL;
		tpiso->tso_pending_send_header = bp;
		tpiso->tso_pending_send.mblk = bp->b_cont;
		bp->b_cont = NULL;
		tpiso->tso_pending_send_error = 0;
		tpiso->tso_flags |= TSOF_SEND_PENDING;
		tpiso->tso_flags &= ~TSOF_UNUSED;

		return tpisocket_send_check(tpiso);
	}

	case _XTI_T_OPTMGMT_REQ:	/* manage XTI options req */

		if (tpiso->tso_flags & TSOF_XOPTMGMT_REQ_PNDNG) {
			return tpisocket_wreject(tpiso, bp, pi->type, 
				TOUTSTATE, 0);
		}
#ifdef _TPI_DEBUG
		if (_tpiprintfs) {
			printf("K: XTI_T_OPTMGMT_REQ tpiso 0x%x, state %d\n",
				tpiso, tpiso->tso_state);
				if (_tpiprintfs > 1) {
					tpi_dump_optmgmt_req(bp);
				}
		}
#endif
		tpiso->tso_flags &= ~TSOF_UNUSED;
		if (tpiso->tso_pending_optmgmt_req != NULL) {
			freemsg(tpiso->tso_pending_optmgmt_req);
		}
		tpiso->tso_pending_optmgmt_req = bp;
		tpiso->tso_flags |= TSOF_XOPTMGMT_REQ_PNDNG;
		tpisocket_optmgmt_req_done(tpiso, ISXTI);
		return 1;

	case T_OPTMGMT_REQ:	/* manage options req     */
		if (tpiso->tso_flags & TSOF_OPTMGMT_REQ_PENDING) {
			return tpisocket_wreject(tpiso, bp, pi->type, 
				TOUTSTATE, 0);
		}
#ifdef _TPI_DEBUG
		if (_tpiprintfs) {
			printf("K: T_OPTMGMT_REQ tpiso 0x%x, state %d\n",
				tpiso, tpiso->tso_state);
			if (_tpiprintfs > 1) {
				tpi_dump_optmgmt_req(bp);
			}
		}
#endif
		tpiso->tso_flags &= ~TSOF_UNUSED;
		if (tpiso->tso_pending_optmgmt_req != NULL)
			freemsg(tpiso->tso_pending_optmgmt_req);
		tpiso->tso_pending_optmgmt_req = bp;
		tpiso->tso_flags |= TSOF_OPTMGMT_REQ_PENDING;
		tpisocket_optmgmt_req_done(tpiso, NOTXTI);
		return 1;

	case T_ORDREL_REQ:       /* orderly release req    */
		tpiso->tso_flags &= ~TSOF_UNUSED;
		/*
		 * Orderly release request:
		 *	- if in data transfer, now wait for an orderly release
		 *	  indication
		 *	- if not in data transfer, and not waiting for an
		 *	  orderly release request, this is illegal, so return
		 *	  TOUTSTATE
		 *	- otherwise, shutdown for writing, and become idle
		 */
#ifdef _TPI_DEBUG
		if (_tpiprintfs) {
			printf("K: tpiso 0x%x ordrelreq in state %d\n",
				tpiso,tpiso->tso_state);
		}
#endif	/* _TPI_DEBUG */

		if (tpiso->tso_state == TS_DATA_XFER) {
			tpiso->tso_state = TS_WIND_ORDREL;
		} else {
			if (tpiso->tso_state != TS_WREQ_ORDREL) {
				return tpisocket_wreject(tpiso, bp, pi->type, 
					TOUTSTATE, 0);
			}
			tpiso->tso_state = TS_IDLE;
		}
		/*
		 * pass in 1; soshutdown() will increment it and turn it into
		 * FWRITE.
		 */
		VSOP_SHUTDOWN(vso, 1, unix_error);
#ifdef _TPI_DEBUG
		if (_tpiprintfs) {
			printf(
			"K: tpiso 0x%x new state %d\n",tpiso,tpiso->tso_state);
		}
#endif	/* _TPI_DEBUG */
		break;

	default:
		return tpisocket_wreject(tpiso, bp, pi->type, TNOTSUPPORT, 0);
	}
	freemsg(bp);
	return 1;
}

/*
 * tpisocket_wput
 * Called for each output packet presented to this module.
 * If the socket can accept another packet, and output need
 * not be queued, transmits the packet directly.  Otherwise,
 * queues the packet for later processing.
 * Entered with Stream monitor lock held
 */
int
tpisocket_wput(queue_t *wq, mblk_t *bp)
{
#ifdef _TPISOCKD
	struct tpisocket *tpiso = (struct tpisocket *)wq->q_ptr;

	if ((tpiso == NULL) || !(tpiso->tso_flags & TSOF_ISOPEN)) {
	    /* (tpiso->tso_flags & TSOF_CLOSING) */

		return 0;
        }
	TRACEME(0x47, (void *)tpiso);
	TRACEME(0x48, (void *)tpiso->tso_count);
	TRACEME(0x49, (void *)tpiso->tso_vsocket);

	TSO_HOLD(tpiso);

	tpisockd_call((void (*)())_tpisocket_wput, wq, bp, tpiso);
	return 0;
}

static int
_tpisocket_wput(queue_t	*wq, mblk_t *bp, struct tpisocket *tpiso)
{
#else
	struct tpisocket *tpiso = (struct tpisocket *)wq->q_ptr;
#endif /* _TPISOCKD */

	TRACEME(0x4a, (void *)tpiso);
	if (tpiso == NULL) {
		freemsg(bp);
		goto done;
        }

	TRACEME(0x4b, (void *)tpiso->tso_vsocket);
	ASSERT(tpiso == (struct tpisocket *)wq->q_ptr);

	if (!(tpiso->tso_flags & TSOF_ISOPEN)) {
		sdrv_error(wq, bp);
		goto done;
	}
	TRACEME(0x4c, (void *)tpiso->tso_vsocket);
	TRACEME(0x4d, (void *)(long)bp->b_datap->db_type);

	/*
	 * If we are closing then flush the queue's and drop this message.
	 * This maybe fraught with error since the queue may have already
	 * been dismantled by a strclose since we may have slept waiting
	 * for the STREAM monitor to become free.
	 */
	if (tpiso->tso_flags & TSOF_CLOSING) {
		flushq(wq, FLUSHDATA);
		freemsg(bp);
		goto done;
        }

	switch (bp->b_datap->db_type) {
	case M_FLUSH:
		if (*bp->b_rptr & FLUSHBAND) {
			if (*bp->b_rptr & FLUSHW)
				flushband(wq, FLUSHDATA, *(bp->b_rptr + 1));
			if (*bp->b_rptr & FLUSHR)
				flushband(RD(wq),FLUSHDATA, *(bp->b_rptr + 1));
		} else {
			if (*bp->b_rptr & FLUSHW) {
				flushq(wq, FLUSHDATA);
				/* XXX: flush socket output? */
			}
			if (*bp->b_rptr & FLUSHR) {
				flushq(RD(wq), FLUSHDATA);
				/* XXX: flush socket input? */
			}
		}
		freemsg(bp);
		break;

	case M_DATA:
	case M_DELAY:
	case M_PROTO:
	case M_PCPROTO:
	case M_IOCTL:
	case M_IOCDATA:
	case M_CTL:
		if (tpiso->tso_flags & (TSOF_QUEUE_OUTPUT|TSOF_SEND_PENDING)) {
			TSO_PUTWQ(tpiso, bp);
			goto done;
		}
		(void)tpisocket_wact(tpiso, bp);
		break;
	default:
		sdrv_error(wq, bp);
		break;
	}
done:
	TSO_RELE(tpiso);
#ifdef _TPISOCKD
	if ((tpiso->tso_count == 1) && (tpiso->tso_flags & TSOF_CLOSING)) {
		sv_broadcast(&tpisockd_inuse);
	}
#endif
	return 0;
}

/*
 * tpisocket_wsrv - Process queued output messages
 */
int
tpisocket_wsrv(queue_t *wq)
{
#ifdef _TPISOCKD
	struct tpisocket *tpiso = (struct tpisocket *)wq->q_ptr;

	if (! tpiso)
		return 0;

	if (!(tpiso->tso_flags & TSOF_ISOPEN) ||
	     (tpiso->tso_flags & TSOF_CLOSING)) {

		flushq(wq, FLUSHDATA);
		tpiso->tso_flags &= ~TSOF_WSRV_QUEUED;
		return 0;
	}
	TRACEME(0x4e, (void *)tpiso);
	TRACEME(0x4f, (void *)tpiso->tso_count);
	TRACEME(0x50, (void *)tpiso->tso_vsocket);

	TSO_HOLD(tpiso);

	tpisockd_call((void (*)())_tpisocket_wsrv, wq, NULL, tpiso);
	return 0;
}

/*ARGSUSED*/
static int
_tpisocket_wsrv(queue_t *wq, void *arg2, struct tpisocket *tpiso)
{
#else
	struct tpisocket *tpiso = (struct tpisocket *)wq->q_ptr;
#endif /* _TPISOCKD */

	mblk_t *bp;

	TRACEME(0x51, (void *)tpiso);
	/*
	 * stream may be in process of being dismantled. Just quit then.
	 */
	if (tpiso == NULL) { /* || (tpiso->tso_flags & TSOF_CLOSING) */
		goto done;
	}

	TRACEME(0x52, (void *)tpiso->tso_vsocket);
	TRACEME(0x75, (void *)tpiso->tso_count);

	ASSERT(tpiso == (struct tpisocket *)wq->q_ptr);

	tpiso->tso_flags &= ~TSOF_WSRV_QUEUED;

	if (!(tpiso->tso_flags & TSOF_ISOPEN)) {
		flushq(wq, FLUSHDATA);
		TRACEME(0x76, (void *)tpiso->tso_count);
		goto done;
	}
	TRACEME(0x53, (void *)tpiso->tso_vsocket);

	if (tpiso->tso_flags & TSOF_SEND_PENDING &&
	    ! tpisocket_send_check(tpiso)) {
		TRACEME(0x77, (void *)tpiso->tso_count);
		goto done;
	}

	enableok(wq);
	while ((bp = getq(wq)) != NULL) {
		if (! tpisocket_wact(tpiso, bp)) {
			TRACEME(0x78, (void *)tpiso->tso_count);
			goto done;
		}
	}
	tpiso->tso_flags &= ~TSOF_QUEUE_OUTPUT;

done:
	TRACEME(0x79, (void *)tpiso->tso_count);
	TSO_RELE(tpiso);

#ifdef _TPISOCKD
	if ((tpiso->tso_count == 1) && (tpiso->tso_flags & TSOF_CLOSING)) {
		sv_broadcast(&tpisockd_inuse);
	}
#endif
	return 0;
}

/*
 * tpisocket_release_send_resources - Releases data blocks used in sending 
 */
void
tpisocket_release_send_resources(struct tpisocket *tpiso)
{
	struct	mbuf *mp;
	mblk_t	*bp;

#define RELEASE_BUFFER_BLK(field, flag) { \
	if (tpiso->tso_pending_send_flags & flag) { \
		mp = tpiso->field.mbuf; \
		if (mp != NULL) { \
			tpiso->field.mbuf = NULL; \
			m_freem(mp); \
		} \
	} else { \
		bp = tpiso->field.mblk; \
		if (bp != NULL) { \
			tpiso->field.mblk = NULL; \
			freemsg(bp); \
		} \
	} }

	TRACEME(0x85, (void *)tpiso);
	TRACEME(0x86, (void *)(long)tpiso->tso_pending_send_flags);
	TRACEME(0x87, (void *)tpiso->tso_pending_send.mbuf);

	RELEASE_BUFFER_BLK(tso_pending_send,         TSOM_MSG_CONVERTED);
	RELEASE_BUFFER_BLK(tso_pending_send_addr,    TSOM_ADDR_CONVERTED);
	RELEASE_BUFFER_BLK(tso_pending_send_control, TSOM_CONTROL_CONVERTED);

	if (bp = tpiso->tso_pending_send_header) {
		tpiso->tso_pending_send_header = NULL;
		freemsg(bp);
	}
}

/*
 * tpisocket_release_recv_resources - Releases data blocks used in receiving
 */
void
tpisocket_release_recv_resources(struct tpisocket *tpiso)
{
	struct	mbuf *mp;
	mblk_t	*bp;

#define RELEASE_RECV_BUFFER_BLK(field, flag) { \
	if (!(tpiso->tso_pending_recv_flags & flag)) { \
		mp = tpiso->field.mbuf; \
		if (mp != NULL) { \
			m_freem(mp); \
			tpiso->field.mbuf = NULL; \
		} \
	} else { \
		bp = tpiso->field.mblk; \
		if (bp != NULL) { \
			freemsg(bp); \
			tpiso->field.mblk = NULL; \
		} \
	} }

	if (tpiso->tso_pending_recv_flags & TSOM_MSG_PRESENT) {
		RELEASE_RECV_BUFFER_BLK(tso_pending_recv, TSOM_MSG_CONVERTED);
		RELEASE_RECV_BUFFER_BLK(tso_pending_recv_addr,
			TSOM_ADDR_CONVERTED);
		RELEASE_RECV_BUFFER_BLK(tso_pending_recv_control,
			TSOM_CONTROL_CONVERTED);
	}
}

/*
 * tpisocket_send_check - Process pending mbuf send
 */
int
tpisocket_send_check(struct tpisocket *tpiso)
{
	struct mbuf *mp;
	mblk_t	*bp;
	int	error;
	int	pkt_type;
	struct mbuf *cp;
	int space, sendlen;
	struct	vsocket *vso = tpiso->tso_vsocket;
	struct	socket *so =
	    (struct socket *)(BHV_PDATA(VSOCK_TO_FIRST_BHV(vso)));
	struct inpcb *inp;
	int	oldtos;
	int 	unix_error;
	int 	udp = (tpiso->tso_control->tsc_protocol == IPPROTO_UDP);
#ifdef RENO
	int first_time;
#endif
	TRACEME(0x54, (void *) tpiso);

	if (!(tpiso->tso_flags & TSOF_SEND_PENDING))
		return 1;

	pkt_type = (tpiso->tso_pending_send_header != NULL 
			? ((union T_primitives *) 
				tpiso->tso_pending_send_header->b_rptr)->type
			: T_DATA_REQ);

	if (tpiso->tso_pending_send_error)
		goto process_send_error;
/*
 * Define MACRO to convert mblk structures to mbuf structures
 */
#define CONVERT_SEND_BUFFER(ptr, flag, mbuf_type, mbuf_flags) { \
  if (!(tpiso->tso_pending_send_flags & flag) && (tpiso->ptr.mblk != NULL)) { \
		mp = tpisocket_mblk_to_mbuf(tpiso->ptr.mblk, \
			M_DONTWAIT, mbuf_type, mbuf_flags, tpiso->tso_wq); \
		if (mp == NULL) { \
			return 0; \
		} \
		tpiso->ptr.mbuf = mp; \
		tpiso->tso_pending_send_flags |= flag; \
	} }

	CONVERT_SEND_BUFFER(tso_pending_send, TSOM_MSG_CONVERTED, MT_DATA, 0);
	CONVERT_SEND_BUFFER(tso_pending_send_addr, TSOM_ADDR_CONVERTED,
		MT_DATA, 0);
#ifdef RENO
	CONVERT_SEND_BUFFER(tso_pending_send_control, TSOM_CONTROL_CONVERTED,
		MT_CONTROL, 0);
#else
	ASSERT(tpiso->tso_pending_send_control.mblk == NULL);
#endif /* RENO */

	if (pkt_type == T_UNITDATA_REQ) {
		struct T_unitdata_req *ureq = (struct T_unitdata_req *)
			tpiso->tso_pending_send_header->b_rptr;
		SOCKET_LOCK(so);
		inp = (struct inpcb *)so->so_pcb;
		if (inp && udp) {
			oldtos = inp->inp_ip_tos;
		}
		error = tpisocket_process_assoc_options(tpiso, so,
			tpiso->tso_pending_send_header,
			tpiso->tso_pending_send_header->b_rptr +
			ureq->OPT_offset, ureq->OPT_length, (mblk_t **)0,
			&unix_error);
		SOCKET_UNLOCK(so);
		if (error) {
			tpiso->tso_pending_send_error = error;
			goto process_send_error;
		}
	}
	error = 0;

	cp = tpiso->tso_pending_send_addr.mbuf;
	if (cp != NULL &&
	    cp->m_next != NULL &&
	    cp->m_len < MLEN &&
	    (sendlen = m_length(cp)) > 0) {
		cp = tpisocket_dup_mbuf(cp, M_DONTWAIT,tpiso->tso_wq, sendlen);

		if (cp == NULL) {
			if (inp && (pkt_type == T_UNITDATA_REQ)) {
				SOCKET_LOCK(so);
				inp = (struct inpcb *)so->so_pcb;
				if (inp && udp) {
					inp->inp_ip_tos = oldtos;
				}
				SOCKET_UNLOCK(so);
			}
			return 0;
		}
		cp = m_pullup(cp, min(MLEN, sendlen));
		if (cp == NULL) {
			tpisocket_request_mbufcall(tpiso->tso_wq);
			if (inp && (pkt_type == T_UNITDATA_REQ)) {
				SOCKET_LOCK(so);
				inp = (struct inpcb *)so->so_pcb;
				if (inp && udp) {
					inp->inp_ip_tos = oldtos;
				}
				SOCKET_UNLOCK(so);
			}
			return 0;
		}
		m_freem(tpiso->tso_pending_send_addr.mbuf);
		tpiso->tso_pending_send_addr.mbuf = cp;
	}
	cp = NULL;

#ifdef RENO
	first_time = 1;
#endif
	while (error == 0) {
		int pkthdr_len;

		mp = tpiso->tso_pending_send.mbuf;
		if (mp != NULL) {
			pkthdr_len = m_length(mp);
			if (pkt_type == T_UNITDATA_REQ) {
				sendlen = pkthdr_len;
			} else {
				space = sbspace(&so->so_snd);
				if (space == 0) {
					m_freem(cp);
					if (inp && 
					    (pkt_type == T_UNITDATA_REQ)) {
						SOCKET_LOCK(so);
						inp=(struct inpcb *)so->so_pcb;

						if (inp && udp) {
						  inp->inp_ip_tos = oldtos;
						}
						SOCKET_UNLOCK(so);
					}
					return 0;
				}
				sendlen = min(space, pkthdr_len);
			}

			if (NULL == (mp = tpisocket_dup_mbuf(mp, M_DONTWAIT,
						    tpiso->tso_wq, sendlen))) {
				m_freem(cp);
				if (inp && 
				    (pkt_type == T_UNITDATA_REQ)) {
					SOCKET_LOCK(so);
					inp = (struct inpcb *)so->so_pcb;
					if (inp && udp) {
						inp->inp_ip_tos = oldtos;
					}
					SOCKET_UNLOCK(so);
				}
				return 0;
			}
		} else if (cp == NULL && pkt_type != T_UNITDATA_REQ)
			break;

#ifdef RENO
		if (first_time  && tpiso->tso_pending_send_addr.mbuf) {
			mtod(tpiso->tso_pending_send_addr.mbuf,
				struct sockaddr *)->sa_family =
			    mtod(tpiso->tso_pending_send_addr.mbuf,
				    struct osockaddr *)->sa_family;
			mtod(tpiso->tso_pending_send_addr.mbuf,
				struct sockaddr *)->sa_len = 
			    tpiso->tso_pending_send_addr.mbuf->m_len;
		}
		first_time = 0;
#endif
		ASSERT(tpiso->tso_vsocket);
		/* send the message */
		SOCKET_LOCK(so);
		TRACEME(0x55, (void *)tpiso->tso_vsocket);
		error = tpisocket_sosend(
				so,
				tpiso->tso_pending_send_addr.mbuf,
				NULL, mp, cp,
				tpiso->tso_pending_send_flags &
					TSOM_MSG_FLAGS_MASK);
		SOCKET_UNLOCK(so);

		switch (error) {
		case EAGAIN:
		case EWOULDBLOCK:
			if (inp && (pkt_type == T_UNITDATA_REQ)) {
				SOCKET_LOCK(so);
				inp = (struct inpcb *)so->so_pcb;
				if (inp && udp) {
					inp->inp_ip_tos = oldtos;
				}
				SOCKET_UNLOCK(so);
			}
			return 0;
		case ENOBUFS:
			tpisocket_request_mbufcall(tpiso->tso_wq);
			if (inp && (pkt_type == T_UNITDATA_REQ)) {
				SOCKET_LOCK(so);
				inp = (struct inpcb *)so->so_pcb;
				if (inp && udp) {
					inp->inp_ip_tos = oldtos;
				}
				SOCKET_UNLOCK(so);
			}
			return 0;
		default:
			break;
		}
		tpiso->tso_pending_send_error = error;

		if ((pkt_type == T_UNITDATA_REQ) ||
		    (tpiso->tso_pending_send.mbuf == NULL) ||
		    (sendlen == pkthdr_len) ||
		    (NULL == (tpiso->tso_pending_send.mbuf = 
		          tpisocket_trim_mbuf_chain(
			      tpiso->tso_pending_send.mbuf, sendlen))))
			break;

		cp = NULL;
	}

process_send_error:
	if (tpiso->tso_pending_send_error &&
	    (tpiso->tso_flags & TSOF_SO_IMASOCKET ||
	     pkt_type == T_UNITDATA_REQ)) {
		union T_primitives *rpi = 0;
		union T_primitives *pi;
		int	xsize;

		if (pkt_type == T_UNITDATA_REQ) {
			rpi = (union T_primitives *) 
				tpiso->tso_pending_send_header->b_rptr;
			xsize = rpi->unitdata_req.DEST_length +
				rpi->unitdata_req.OPT_length +
				sizeof(struct T_uderror_ind);
		} else
			xsize = sizeof(struct T_uderror_ind);

		bp = tpisocket_allocb(tpiso->tso_wq, xsize, BPRI_HI);
		if (bp == NULL) {
			if (inp && (pkt_type == T_UNITDATA_REQ)) {
				SOCKET_LOCK(so);
				inp = (struct inpcb *)so->so_pcb;
				if (inp && udp) {
					inp->inp_ip_tos = oldtos;
				}
				SOCKET_UNLOCK(so);
			}
			return 0;
		}
		bp->b_datap->db_type = M_PROTO;
		bp->b_wptr = bp->b_rptr + xsize;
		pi = (union T_primitives *)bp->b_rptr;
		pi->type = T_UDERROR_IND;
		if (pkt_type != T_UNITDATA_REQ) {

#ifdef _TPI_DEBUG
			if (_tpiprintfs) {
			printf("K: tpiso 0x%x sending uderrorind for %d\n",
					tpiso, pkt_type);
			}
#endif /* _TPI_DEBUG */
			pi->uderror_ind.DEST_length = 0;
			pi->uderror_ind.DEST_offset = 0;
			pi->uderror_ind.OPT_length = 0;
			pi->uderror_ind.OPT_offset = 0;
		} else {
			ASSERT(rpi);
			pi->uderror_ind.DEST_length =
				rpi->unitdata_req.DEST_length;
			pi->uderror_ind.DEST_offset =
				rpi->unitdata_req.DEST_offset;
			bcopy(((caddr_t)rpi) + rpi->unitdata_req.DEST_offset,
			      ((caddr_t)pi) + pi->uderror_ind.DEST_offset,
			      pi->uderror_ind.DEST_length);
			pi->uderror_ind.OPT_length =
				rpi->unitdata_req.OPT_length;
			pi->uderror_ind.OPT_offset =
				rpi->unitdata_req.OPT_offset;
			bcopy(((caddr_t)rpi) + rpi->unitdata_req.OPT_offset,
			      ((caddr_t)pi) + pi->uderror_ind.OPT_offset,
			      pi->uderror_ind.OPT_length);
		}
		pi->uderror_ind.ERROR_type = tpiso->tso_pending_send_error;

#ifdef _TPI_DEBUG
		if (_tpiprintfs) {
			printf("K: tpiso 0x%x uderror_ind err=%d\n",
				tpiso, pi->uderror_ind.ERROR_type);
			if (_tpiprintfs > 1) {
				tpi_dump_unitdata_ind(bp);
			}
		}
#endif	/* _TPI_DEBUG */
		putnext(tpiso->tso_rq, bp);
	}
	tpiso->tso_pending_send_error = 0;

	/* release left-over resources */
	TRACEME(0x88, (void *)tpiso);
	tpisocket_release_send_resources(tpiso);

	tpiso->tso_flags &= ~TSOF_SEND_PENDING;
	tpiso->tso_pending_send_flags = 0;
	if (inp && (pkt_type == T_UNITDATA_REQ)) {
		SOCKET_LOCK(so);
		inp = (struct inpcb *)so->so_pcb;
		if (inp && udp) {
			inp->inp_ip_tos = oldtos;
		}
		SOCKET_UNLOCK(so);
	}
	return 1;
}

/*
 * tpisocket_conn_req_done - Complete a pending connection request
 * Entered with Stream monitor lock held
 */
void
tpisocket_conn_req_done(struct	tpisocket *tpiso)
{
#define tpictl tpiso->tso_control
	struct	vsocket *vso = tpiso->tso_vsocket;
	struct	socket *so =
	    (struct socket *)(BHV_PDATA(VSOCK_TO_FIRST_BHV(vso)));
	struct	T_conn_con *cc;
	mblk_t	*bp;
	int	a_size;
	int	o_size;
	int	unix_error;

	if (!(tpiso->tso_flags & TSOF_CONN_REQ_PENDING))
		return; /* nothing pending */

	if ((so->so_state & SS_ISCONNECTING) && (so->so_error == 0))
		return; /* not done yet */

	unix_error = so->so_error;

	if (unix_error) { /* outstanding socket error */

		bp = tpiso->tso_pending_request;
		tpiso->tso_pending_request = NULL;
		if (tpiso->tso_saved_conn_opts) {
			freemsg(tpiso->tso_saved_conn_opts);
			tpiso->tso_saved_conn_opts = 0;
		}
		tpiso->tso_flags &= ~TSOF_CONN_REQ_PENDING;
		tpiso->tso_flags |= TSOF_CONN_REQ_FAILED;

		if (tpiso->tso_flags & TSOF_RECV_DISCON) {
			ASSERT(tpiso->tso_state == TS_IDLE);
			freemsg(bp);	/* discard the message in this case */
			return;
		}
		/*
		 * When we haven't already received one,
		 * convert the pending request message into a
		 * discon_ind message containing the errno.
		 */
		tpisocket_send_discon_ind(tpiso, bp, unix_error, (long)-1);
		tpiso->tso_state = TS_IDLE;
		return;
	}

	a_size = TSC_ADDRESS_SIZE(tpiso, TSCA_REMOTE_ADDRESS, NULL);
	/* this includes saved options if present */
	o_size = TSC_ADDRESS_SIZE(tpiso, TSCA_ACTUAL_OPTIONS, NULL);
	bp = tpisocket_allocb(tpiso->tso_rq,
			      sizeof(struct T_conn_con) + a_size + o_size,
			      BPRI_MED);
	if (bp == NULL)
		return;

	freemsg(tpiso->tso_pending_request);
	tpiso->tso_pending_request = NULL;
	tpiso->tso_flags &= ~TSOF_CONN_REQ_PENDING;

	bp->b_wptr= bp->b_rptr + (sizeof(struct T_conn_con) + a_size + o_size);
	bp->b_datap->db_type = M_PROTO;

	cc = (struct T_conn_con *)bp->b_rptr;
	cc->PRIM_type = T_CONN_CON;
	cc->RES_length = a_size;
	cc->RES_offset = sizeof(struct T_conn_con) + o_size;
	cc->OPT_length = o_size;
	cc->OPT_offset = sizeof(struct T_conn_con);
	TSC_CONVERT_ADDRESS(tpiso,((caddr_t)cc) + cc->OPT_offset,
		    TSCA_OPTIONS | TSCA_MODE_READ, o_size, NULL, NULL);
	TSC_CONVERT_ADDRESS(tpiso,((caddr_t)cc) + cc->RES_offset,
		    TSCA_REMOTE_ADDRESS | TSCA_MODE_READ, a_size, NULL, NULL);
	tpiso->tso_state = TS_DATA_XFER;

#ifdef _TPI_DEBUG
	if (_tpiprintfs) {
		printf("K: tpiso 0x%x conn_con into data_xfer\n", tpiso);
		if (_tpiprintfs > 1) {
			tpi_dump_conn_con(bp);
		}
	}
#endif	/* _TPI_DEBUG */
	putnext(tpiso->tso_rq, bp);
#undef tpictl
}

/*
 * tpisocket_discon_req_done
 * Check if disconnection is complete.  If so, send "ok" message.
 */
void
tpisocket_discon_req_done(struct tpisocket *tpiso)
{
#define tpictl tpiso->tso_control
	struct	vsocket *vso = tpiso->tso_vsocket;
	struct	socket *so =
	    (struct socket *)(BHV_PDATA(VSOCK_TO_FIRST_BHV(vso)));
	mblk_t	*bp;

	if (!(tpiso->tso_flags & TSOF_DISCON_REQ_PENDING))
		return; /* nothing pending */

	if (so->so_state & (SS_ISCONNECTING|SS_ISCONNECTED|SS_ISDISCONNECTING))
		return;

	bp= tpisocket_allocb(tpiso->tso_rq, sizeof(struct T_ok_ack), BPRI_MED);
	if (bp == NULL)
		return;
	tpisocket_send_ok_ack(tpiso, bp, T_DISCON_REQ);

	tpiso->tso_flags &= ~ TSOF_DISCON_REQ_PENDING;
#undef tpictl
}

/*
 * tpisocket_optmgmt_req_done
 * Check if optmgmtnection is complete.  If so, send "ok" message.
 */
void
tpisocket_optmgmt_req_done(struct tpisocket *tpiso, int isxti)
{
#define tpictl tpiso->tso_control
	mblk_t	*bp;
	struct T_optmgmt_ack *oma;
	int	more_to_do;
	int	unix_error;
	int	tli_error;
	int	tli_prim;

	if (isxti) {
		if (!(tpiso->tso_flags & TSOF_XOPTMGMT_REQ_PNDNG))
			return; /* nothing pending */
	} else {
		if (!(tpiso->tso_flags & TSOF_OPTMGMT_REQ_PENDING))
			return; /* nothing pending */
	}
	/*
	 * Note:  this code depends on T_optmgmt_ack and T_optmgmt_req
	 * being the same shape and size.
	 */
	more_to_do = TSCO_UNKNOWN;
	unix_error = TSC_OPTMGMT(tpiso, &more_to_do);
	bp = tpiso->tso_pending_optmgmt_req;
	if (unix_error) {
		tpisocket_convert_error(&unix_error, &tli_error,
			T_OPTMGMT_REQ);
		tpiso->tso_pending_optmgmt_req = NULL;
send_rejection:
		if (isxti)
			tli_prim = _XTI_T_OPTMGMT_REQ;
		else
			tli_prim = T_OPTMGMT_REQ;
		if (! tpisocket_wreject(tpiso, bp,
				  tli_prim, tli_error, unix_error))
			return;
	} else {
		switch (more_to_do) {
		default:
		case TSCO_UNKNOWN:
			tli_error = TSYSERR;
			unix_error = EINVAL;
			goto send_rejection;
		case TSCO_DONE:
			oma = (struct T_optmgmt_ack *)bp->b_rptr;
			oma->PRIM_type = T_OPTMGMT_ACK;
		case TSCO_DONE_DO_ACK:
			tpiso->tso_pending_optmgmt_req = NULL;
#ifdef _TPI_DEBUG
			if (_tpiprintfs) {
			printf("K: T_OPTMGMT_ACK tpiso 0x%x, state %d\n",
					tpiso, tpiso->tso_state);
				if (_tpiprintfs > 1) {
					tpi_dump_optmgmt_ack(bp);
				}
			}
#endif
			putnext(tpiso->tso_rq, bp);
			break;
		case TSCO_DONE_NO_ACK:
			tpiso->tso_pending_optmgmt_req = NULL;
			freemsg(bp);
			break;
		case TSCO_MORE:
			return;
		case TSCO_DONE_XTI:
			tpiso->tso_pending_optmgmt_req = NULL;
			break;
		}
	}
	if (isxti)
		tpiso->tso_flags &= ~TSOF_XOPTMGMT_REQ_PNDNG;
	else
		tpiso->tso_flags &= ~TSOF_OPTMGMT_REQ_PENDING;
#undef tpictl
}

/*
 * tpisocket_address_size - Return size of an address or control item.
 */
/*ARGSUSED*/
int
tpisocket_address_size(
	struct	tpisocket *tpiso,
	int	code,
	struct	socket *nso)
{
	switch (code) {
	case TSCA_LOCAL_ADDRESS:
	case TSCA_REMOTE_ADDRESS:
		return sizeof(struct sockaddr);
	case TSCA_OPTIONS:
		return(1024); /* SVR4 value */
	case TSCA_ACTUAL_OPTIONS:
		return(0);
	default:
		return 0;
	}
}

/*ARGSUSED*/
int
tpisocket_convert_address(
	struct	tpisocket *tpiso,
	char   *ptr,
	int	mode,
	int	size,
	struct	mbuf *m,
	struct	socket *nso)
{
	struct	vsocket *vso = tpiso->tso_vsocket;
	struct	socket *so;
	int	error;

	switch (mode & TSCA_CODE) {
	case TSCA_LOCAL_ADDRESS:
	case TSCA_REMOTE_ADDRESS:
		break;
	case TSCA_OPTIONS:
	default:
		return EINVAL;
	}

	if (nso) { /* non-zero means the 'nso' socket is already locked */
		so = nso;
	} else { /* NULL 'nso' means get the socket lock on 'so' */
		so = (struct socket *)(BHV_PDATA(VSOCK_TO_FIRST_BHV(vso)));
		SOCKET_LOCK(so);
	}

	switch (mode & TSCA_MODE) {
	case TSCA_MODE_READ:
		m = tpiso->tso_temporary_mbuf;
		error = (*so->so_proto->pr_usrreq)(so,
				((mode & TSCA_CODE) == TSCA_LOCAL_ADDRESS
					? PRU_SOCKADDR : PRU_PEERADDR), 
				0, m, 0);
		if (error) {
			if (!nso) {
				SOCKET_UNLOCK(so);
			}
			return error;
		}
		if (size > m->m_len)
			bzero(ptr, size);
		bcopy(mtod(m, caddr_t), ptr, min(size, m->m_len));
#ifdef RENO
		((struct sockaddr *)ptr)->sa_len = 0;
#endif
		break;

	case TSCA_MODE_TO_SOCKETS:
		m->m_len = 0;
		m->m_len = min(M_TRAILINGSPACE(m), size);
		bcopy(ptr, mtod(m, caddr_t), m->m_len);

		switch (mode & TSCA_CODE) {
		case TSCA_LOCAL_ADDRESS:
		case TSCA_REMOTE_ADDRESS:
#ifdef RENO
			mtod(m, struct sockaddr *)->sa_family =
				((struct osockaddr *)ptr)->sa_family;
			mtod(m, struct sockaddr *)->sa_len = m->m_len;
#endif
			break;

		case TSCA_OPTIONS:
			break;
		default:
			error = EINVAL;
			break;
		}
		break;

	case TSCA_MODE_TO_STREAMS:
		bcopy(mtod(m, caddr_t), ptr, min(m->m_len, size));
		switch (mode & TSCA_CODE) {
		case TSCA_LOCAL_ADDRESS:
		case TSCA_REMOTE_ADDRESS:
#ifdef RENO
			((struct osockaddr *)ptr)->sa_family =
				((struct sockaddr *)ptr)->sa_family;
#endif
			break;

		case TSCA_OPTIONS:
			break;
		default:
			error = EINVAL;
			break;
		}
		break;

	case TSCA_MODE_WRITE:
	default:
		error = EINVAL;
		break;
	}
	if (!nso) {
		SOCKET_UNLOCK(so);
	}
	return error;
}

/*
 * 'so' is head (listen) socket
 */
void
tpisocket_scan_conn_q(struct tpisocket *tpiso, struct socket *so)
{
	struct socket *nso;
	int s;

#define tpictl (tpiso->tso_control)
	/*
	 * Although the TPI state machine explicitly allows more than
	 * one T_CONN_IND to be outstanding (sent up to the user process)
	 * at a time, the TLI code in libnsl fails if a second T_CONN_IND
	 * arrives at the stream head before the first one is completely
	 * processed.  So, if the count of "outstanding" T_CONN_INDs is
	 * non-zero, we won't send any more.
	 *
	 * Note that merely counting the sockets with the SS_CONN_IND_SENT
	 * bit set doesn't suffice, because if a RST arrives before the
	 * T_CONN_IND is replied to, the following chain of function calls
	 * dequeues and discards the socket with the SS_CONN_IND_SENT bit:
	 * tcp_input->tcp_close->in_pcbdetach->sofree->soqremque
	 */
	ASSERT((tpiso->tso_flags & TSOF_XTI) || (tpiso->tso_outcnt < 2));

	if (tpiso->tso_outcnt && !(tpiso->tso_flags & TSOF_XTI)) {
		return;
	}

	ASSERT(SOCKET_ISLOCKED(so));
	/*
	 * scan the connection queue on the socket
	 */
	SO_UTRACE(UTN('scan','cq00'), so, __return_address);
restart_scan:
	SO_UTRACE(UTN('scan','cq01'), so, __return_address);
	SOCKET_QLOCK(so, s);
	nso = so->so_q;
	while (nso != so) {
		SO_UTRACE(UTN('scan','cq02'), nso, __return_address);
		TRACEME(0x56, (void *)nso);
		ASSERT(!mutex_mine(&(nso)->so_sem));

		if (!SOCKET_TRYLOCK(nso)) { /* failed to get child's lock */
			TRACEME(0x57, (void *)nso);
			TRACEME(0x58, (void *)so);

			SO_UTRACE(UTN('scan','cq04'), nso, __return_address);
			SOCKET_QUNLOCK(so, s);
			SOCKET_UNLOCK(so);
			nano_delay(&backoff);
			SOCKET_LOCK(so);
			goto restart_scan;
		}
		TRACEME(0x59, (void *)nso);
		SO_UTRACE(UTN('scan','cq03'), nso, __return_address);
		/*
		 * could keep this locked longer, but complicates code, and
		 * now that we have lock on child, shouldn't matter.  We'll
		 * reacquire prior to calling soqremque().  Also would be
		 * bad for interrupt behavior, and we might call something
		 * that sleeps; not good.
		 */
		ASSERT(mutex_mine(&(nso)->so_sem));
		SOCKET_QUNLOCK(so, s);
		/*
		 * Arrive here with parent's 'so' and child's 'nso' locks held
		 * Check if connection broken on child before accept finished
		 */
		if (nso->so_state & SS_CANTRCVMORE) {
			long seq = (long)(__psint_t)nso->so_data;
			int need_disc = (nso->so_state & SS_CONN_IND_SENT);
			/* connection broken before accept */

			nso->so_state |= SS_NOFDREF;
			/*
			 * CONN_IND not yet sent: discard 
			 * connection (otherwise, wait for
			 * CONN_RES or DISCON_REQ from user)
			 *
			 * dequeue the socket
			 */
			SOCKET_QLOCK(so, s);
			soqremque(nso, 1);
			SOCKET_QUNLOCK(so, s);

			/* release parent lock to avoid deadlocks */
			SOCKET_UNLOCK(so);
			nso->so_state &= ~SS_CONN_IND_SENT;

			/*
			 * Discard the socket
			 * NOTE: lock on 'nso' MUST be held
			 * on the call to soabort()
			 * abort releases lock on nso
			 * as socket is destroyed
			 */
			TRACEME(0x5a, (void *)nso);
			if (soabort(nso)) {
				TRACEME(0x5b, (void *)nso);
				SOCKET_UNLOCK(nso);
			}

			/* reclaim parent lock for loop invariant */
			TRACEME(0x5c, (void *)so);
			if (need_disc) {
				mblk_t *bp;

				/*
				 * send T_DISCON_IND to notify parent that
				 * we're gone
				 */
#ifdef _TPI_DEBUG
				if (_tpiprintfs) {
				       printf("K: discon_ind seq %d on 0x%x\n",
						seq, tpiso);
				}
#endif /* _TPI_DEBUG */
				if ((bp = allocb(sizeof(union T_primitives),
					BPRI_HI))) {

					tpisocket_send_discon_ind(tpiso, bp, 
						ECONNRESET, seq);
				}
				/* one less pending connection indication */
				tpiso->tso_outcnt--;
				if (tpiso->tso_outcnt == 0) {
					tpiso->tso_state = TS_IDLE;
				}
			}
			SOCKET_LOCK(so);
			SOCKET_QLOCK(so, s);
			SO_UTRACE(UTN('scan','cq05'), nso, __return_address);
			nso = so->so_q;	/* we whacked nso, so start again */
			continue;
		}

		/*
		 * Prior to XPG-4 work, we only sent one connection indication
		 * upstream at a time.  We can't pass the X/Open tests if we
		 * do that, so libxnet will send us a special ioctl saying
		 * that we are in the lame-o-tronic X/Open mode.
		 */
		if (nso->so_state & SS_CONN_IND_SENT) {
			/* skip this one if we already did it */
			/* ASSERT(tpiso->tso_outcnt); */
			TRACEME(0x5d, (void *)nso);
			SOCKET_UNLOCK(nso);
			SOCKET_QLOCK(so, s);
			SO_UTRACE(UTN('scan','cq06'), nso, __return_address);
			nso = nso->so_q;	/* skip to next */
			continue;
		}

		if (((!tpiso->tso_outcnt) ||
		    (tpiso->tso_flags & TSOF_XTI)) &&
		    (nso->so_state & (SS_ISCONNECTED | SS_ISCONFIRMING))) {
			int	a_size;
			int	o_size;
			mblk_t	*bp;
			int cseq;
			union	T_primitives *pi;

			/*
			 * connection with CONN_IND not yet sent 
			 * and no other CONN_IND is pending
			 */
			a_size = TSC_ADDRESS_SIZE(tpiso,
					TSCA_REMOTE_ADDRESS, nso);
			o_size = TSC_ADDRESS_SIZE(tpiso, 
					TSCA_ACTUAL_OPTIONS, nso);
			bp = tpisocket_allocb(tpiso->tso_rq,
				sizeof(struct T_conn_ind) + a_size + o_size,
				BPRI_MED);
			if (bp == NULL) {
				TRACEME(0x5e, (void *)nso);
				SOCKET_UNLOCK(nso);

				/* not holding queue lock on 'so' right now */
				goto out;
			}
			bp->b_wptr = bp->b_rptr +
				(sizeof(struct T_conn_ind) + a_size + o_size);
			bp->b_datap->db_type = M_PROTO;
			pi = (union T_primitives *)bp->b_rptr;
			pi->type = T_CONN_IND;
			cseq = atomicIncWithWrap(&tpiso->tso_connseq, INT_MAX);
			nso->so_data = (void *)(__psunsigned_t)cseq; /* XXX */
			pi->conn_ind.SEQ_number = cseq;
			pi->conn_ind.SRC_length = a_size;
			pi->conn_ind.SRC_offset =
				sizeof(struct T_conn_ind) + o_size;
			pi->conn_ind.OPT_length = o_size;
			pi->conn_ind.OPT_offset = sizeof(struct T_conn_ind);
			nso->so_state |= (SS_CONN_IND_SENT | SS_TPISOCKET);
			TSC_CONVERT_ADDRESS(tpiso,
				((char *)pi) + pi->conn_ind.OPT_offset,
				TSCA_OPTIONS | TSCA_MODE_READ,
				o_size, NULL, nso);
			TSC_CONVERT_ADDRESS(tpiso,
				((char *)pi) + pi->conn_ind.SRC_offset,
				TSCA_REMOTE_ADDRESS | TSCA_MODE_READ,
				a_size, NULL, nso);

			tpiso->tso_outcnt++;

			tpiso->tso_state = TS_WRES_CIND;
			/*
			 * set WANT_CALL so if we get a FIN while waiting
			 * to be accepted we can wake up
			 */
			nso->so_state |= SS_WANT_CALL;
			/*
			 * Ensure that this connection won't be removed
			 * before we have a chance to look at it, in case
			 * we need to send a disconnect indication for it.
			 */
			nso->so_callout       = 0;
			nso->so_callout_arg   = 0;
			nso->so_monpp         = 0;
			SOCKET_UNLOCK(nso);
#ifdef _TPI_DEBUG
			if (_tpiprintfs) {
				printf("K: set WANT_CALL on 0x%x, pt=0x%x\n",
				nso, tpiso);
				printf("K: conn_ind seq %d on 0x%x h=0x%x\n",
					cseq, tpiso, nso->so_head);
				if (_tpiprintfs > 1) {
					tpi_dump_conn_ind(bp);
				}
			}
#endif /* _TPI_DEBUG */
			putnext(tpiso->tso_rq, bp);
		} else {
			/* some other state? at least release lock */
			SOCKET_UNLOCK(nso);
			TRACEME(0x5f, (void *)nso);
		}
		SOCKET_QLOCK(so, s);
		nso = nso->so_q;
		SO_UTRACE(UTN('scan','cq07'), nso, __return_address);
	}
	/* guaranteed to have queue lock if loop exits normally */
	SOCKET_QUNLOCK(so, s);
out:	;
	/* if we come here from break, we're not hold lock so just return */
}

/*
 * tpisocket_rsrv
 *
 * Process incoming messages from the socket and post them to the stream.
 * Entered with queuerun blocked (including from queuerun).
 */
int
tpisocket_rsrv(queue_t	*rq)
{
#ifdef _TPISOCKD
	struct tpisocket *tpiso = (struct tpisocket *)rq->q_ptr;

	if (! tpiso)
                return 0;

	if (!(tpiso->tso_flags & TSOF_ISOPEN) ||
	    !(tpiso->tso_vsocket) ||
	     (tpiso->tso_flags & TSOF_CLOSING)) {
		tpiso->tso_flags &= ~TSOF_RSRV_QUEUED;
		return 0;
	}

	TRACEME(0x60, (void *)tpiso);
	TRACEME(0x61, (void *)tpiso->tso_count);
	TRACEME(0x62, (void *)tpiso->tso_vsocket);

	TSO_HOLD(tpiso);
	TRACEME(0x83, (void *)tpiso->tso_count);

	tpisockd_call((void (*)())_tpisocket_rsrv, rq, NULL, tpiso);
	return 0;
}

/*ARGSUSED*/
static int
_tpisocket_rsrv(queue_t	*rq, void *arg2, struct tpisocket *tpiso)
{
#else
	struct tpisocket *tpiso = (struct tpisocket *)rq->q_ptr;
#endif /* _TPISOCKD */
	struct	vsocket *vso = tpiso->tso_vsocket;
	struct	socket *so;
	mblk_t	*bp;
	union	T_primitives *pi;
	int	prim;
	int	size;
	int	pri;
	mblk_t	*ep;
	mblk_t	*cp;
	int	old_recv_flags;

	ASSERT(tpiso != 0);
	ASSERT(tpiso == (struct tpisocket *)rq->q_ptr);
	ASSERT_ALWAYS(rq == tpiso->tso_rq);
	ASSERT_ALWAYS(STREAM_LOCKED(rq));
	TRACEME(0x8a, (void *)tpiso);

	if (! tpiso)
		goto done;

	TRACEME(0x8b, (void *)tpiso->tso_flags);

	tpiso->tso_flags &= ~TSOF_RSRV_QUEUED;

	if (!(tpiso->tso_flags & TSOF_ISOPEN) || !vso ||
	    !(so = (struct socket *)(BHV_PDATA(VSOCK_TO_FIRST_BHV(vso))))) {
		return 0;
	}
   
	/* check if waiting for special state changes */
	while (tpiso->tso_flags & TSOF_SET_RECV_CHECK) {
		if (tpiso->tso_flags & TSOF_CONN_REQ_PENDING) {
			tpisocket_conn_req_done(tpiso);
			if (tpiso->tso_flags & TSOF_CONN_REQ_PENDING)
				goto done;
			continue;
		}
		if (tpiso->tso_flags & TSOF_DISCON_REQ_PENDING) {
			tpisocket_discon_req_done(tpiso);
			if (tpiso->tso_flags & TSOF_DISCON_REQ_PENDING)
				goto done;
			continue;
		}
		if (tpiso->tso_flags & TSOF_OPTMGMT_REQ_PENDING) {
			tpisocket_optmgmt_req_done(tpiso, NOTXTI);
			if (tpiso->tso_flags & TSOF_OPTMGMT_REQ_PENDING)
				goto done;
			continue;
		}
		if (tpiso->tso_flags & TSOF_XOPTMGMT_REQ_PNDNG) {
			tpisocket_optmgmt_req_done(tpiso, ISXTI);
			if (tpiso->tso_flags & TSOF_XOPTMGMT_REQ_PNDNG)
				goto done;
			continue;
		}
		goto done;
	}

	TRACEME(0x8c, (void *)tpiso->tso_flags);
	SOCKET_LOCK(so);
	TRACEME(0x66, (void *)tpiso->tso_flags);

	/*
	 * Bug# 438858
	 * We can block in SOCKET_MPLOCK() and during this time,
	 * tpisocket_close() can be called. We are holding the tpisocket
	 * structure so we are safe there. However, the stream will be
	 * dismantled and we will no longer be able to send a message
	 * up stream. Since the stream will have been dismantled, there
	 * is no reason to even continue processing and we will drop out
	 * the bottom (after releasing socket lock). The tpisocket_dismantle
	 * routine will clean up the socket if we are now the last user of
	 * the socket (by calling soclose).
	 */
	if (!(tpiso->tso_flags & TSOF_ISOPEN) || !tpiso->tso_vsocket || 
	    !(so = (struct socket *)(BHV_PDATA(VSOCK_TO_FIRST_BHV(tpiso->tso_vsocket)))) ||
	     (tpiso->tso_flags & TSOF_CLOSING)) {
		goto exit;
	}

	/* removed when 438858 is fixed */
	ASSERT(!(tpiso->tso_flags & TSOF_CLOSING));

	if (!(so->so_state & SS_ISCONFIRMING)) {
	    /* pull off pending data */
	    while ((so->so_rcv.sb_cc > 0) || 
                   ((so->so_rcv.sb_cc ==0) && (tpiso->tso_flags & TSOF_OOB_INPUT_AVAILABLE))) {
		/* removed when 438858 is fixed */
		ASSERT(!(tpiso->tso_flags & TSOF_CLOSING));

		if (! canput(tpiso->tso_rq->q_next)) 
			goto exit;
		if (!(tpiso->tso_pending_recv_flags & TSOM_MSG_PRESENT)) {
		    int	error;
		    struct uio uio;

		    tpiso->tso_pending_recv.mbuf = NULL;
		    /* clear all flags except out-of-band flag */
		    tpiso->tso_pending_recv_flags = 
			    tpiso->tso_pending_recv_flags & MSG_OOB;
		    uio.uio_resid = so->so_rcv.sb_cc;
#ifdef RENO
		    else {
			struct	mbuf *m;

			for (m = so->so_rcv.sb_mb; m != NULL; m = m->m_next)
				if (m->m_type != MT_SONAME)
					break;
			for (; m != NULL; m = m->m_next)
				if (m->m_type != MT_CONTROL)
					break;
			for (; m != NULL; m = m->m_next)
				if (m->m_len != 0 || m->m_flags & M_PKTHDR)
					break;
			for (; m != NULL; m = m->m_next)
				if (m->m_len != 0)
					break;

			if (TSC_SOCKET_TYPE == SOCK_DGRAM) {
				uio.uio_resid = m_length(m);
			} else { /* stream so gobble part of what's available*/
				if (m == NULL)
					uio.uio_resid = 0;
				else if (m->m_flags & M_PKTHDR &&
					 m->m_pkthdr.len > 0) {
					uio.uio_resid = m->m_pkthdr.len;
				} else
					uio.uio_resid = m->m_len;
			}
		    }
#endif
		    old_recv_flags = tpiso->tso_pending_recv_flags;
		    error = tpisocket_soreceive(tpiso, so,
				      &tpiso->tso_pending_recv_addr.mbuf,
				      &uio,
				      &tpiso->tso_pending_recv.mbuf,
				      &tpiso->tso_pending_recv_control.mbuf,
				      &tpiso->tso_pending_recv_flags);
		    if (error) {
			if (error == EWOULDBLOCK || error == EAGAIN) {
				tpisocket_request_mbufcall(rq);
				error = 0;
				break;	/* don't return here */
			}
			if (tpiso->tso_flags & TSOF_OOB_INPUT_AVAILABLE) {
			    tpiso->tso_flags &= ~TSOF_OOB_INPUT_AVAILABLE;
			    continue;
			}
			break;
		    }
		    tpiso->tso_pending_recv_flags |= TSOM_MSG_PRESENT;
		} /* NB: this is _NOT_ where the breaks come. */

		/* convert the message and send up the stream */
#ifdef RENO
		if (tpiso->tso_pending_recv_addr.mbuf != NULL) {
	mtod(tpiso->tso_pending_recv_addr.mbuf,struct osockaddr *)->sa_family =
	 mtod(tpiso->tso_pending_recv_addr.mbuf, struct sockaddr *)->sa_family;
		}
#endif

		pri = BPRI_HI;		/* like it matters */

#define CONVERT_RECV_BUFFER(ptr, flag) { \
		if (!(tpiso->tso_pending_recv_flags & flag) && \
		    tpiso->ptr.mbuf != NULL) { \
			bp = tpisocket_mbuf_to_mblk(tpiso->tso_rq, \
				tpiso->ptr.mbuf, pri); \
			if (bp == NULL) \
				break; \
			tpiso->ptr.mblk = bp; \
			tpiso->tso_pending_recv_flags |= flag; \
		} }

		CONVERT_RECV_BUFFER(tso_pending_recv, TSOM_MSG_CONVERTED);
		CONVERT_RECV_BUFFER(tso_pending_recv_addr,TSOM_ADDR_CONVERTED);
		CONVERT_RECV_BUFFER(tso_pending_recv_control, 
			TSOM_CONTROL_CONVERTED);

		if (old_recv_flags & MSG_OOB) {
			size = sizeof(struct T_exdata_ind);
			prim = T_EXDATA_IND;
		} else if (tpiso->tso_pending_recv_addr.mblk != NULL ||
			   tpiso->tso_pending_recv_control.mblk != NULL) {
			size = sizeof(struct T_unitdata_ind);
			prim = T_UNITDATA_IND;
		} else {
			size = sizeof(struct T_data_ind);
			prim = T_DATA_IND;
		}
#ifdef _TPI_DEBUG
		if (_tpiprintfs) {
		   printf("K: tpiso 0x%x generating data prim %d prf=%x, %x\n",
				tpiso, prim, tpiso->tso_pending_recv_flags,
				old_recv_flags);
		}
#endif
		ep = bp = tpisocket_allocb(tpiso->tso_rq, size, pri);
		if (bp == NULL) 
			goto exit;

		bp->b_datap->db_type = M_PROTO;
		bp->b_wptr += size;
		pi = (union T_primitives *)bp->b_rptr;
		pi->type = prim;

		if (prim == T_UNITDATA_IND) {
			pi->unitdata_ind.SRC_length = msgdsize(
				tpiso->tso_pending_recv_addr.mblk);
			pi->unitdata_ind.SRC_offset = size;
			pi->unitdata_ind.OPT_length = msgdsize(
				tpiso->tso_pending_recv_control.mblk);
			pi->unitdata_ind.OPT_offset = size +
				pi->unitdata_ind.SRC_length;
			if (tpiso->tso_pending_recv_addr.mblk) {
				for (cp = tpiso->tso_pending_recv_addr.mblk;
				     cp != NULL; cp = cp->b_cont)
					cp->b_datap->db_type = M_PROTO;
				str_conmsg(&bp, &ep,
					   tpiso->tso_pending_recv_addr.mblk);
			}
			if (tpiso->tso_pending_recv_control.mblk) {
				for (cp = tpiso->tso_pending_recv_control.mblk;
				     cp != NULL; cp = cp->b_cont)
					cp->b_datap->db_type = M_PROTO;
				str_conmsg(&bp, &ep,
					tpiso->tso_pending_recv_control.mblk);
			}
		} else {
			if (TSC_FLAGS & TSCF_NO_SEAMS) {
			    pi->data_ind.MORE_flag = 0;
			} else {
			    pi->data_ind.MORE_flag = 
				!(tpiso->tso_pending_recv_flags & MSG_EOR);
			}
			freemsg(tpiso->tso_pending_recv_addr.mblk);
			freemsg(tpiso->tso_pending_recv_control.mblk);
		}
		if (tpiso->tso_pending_recv.mblk) {
			str_conmsg(&bp, &ep, tpiso->tso_pending_recv.mblk);
		} else {
			if (bp == ep)
				tpiso->tso_pending_recv.mblk = bp;
		}

		/* removed when 438858 is fixed */
		ASSERT(!(tpiso->tso_flags & TSOF_CLOSING));

		/* clear all flags except out-of-band data mode */
		tpiso->tso_pending_recv_flags = tpiso->tso_pending_recv_flags
			& MSG_OOB;
#ifdef _TPI_DEBUG
		if (_tpiprintfs) {
			printf("K: tpiso 0x%x sending %d bytes up, prf=%x\n",
		  	  tpiso, msgdsize(bp), tpiso->tso_pending_recv_flags);
		}
		if (prim == T_UNITDATA_IND) {
			if (_tpiprintfs > 1) {
				tpi_dump_unitdata_ind(bp);
			}
		}
#endif
		putnext(tpiso->tso_rq, bp);
	    }
	}

	/* removed when 438858 is fixed */
	ASSERT(!(tpiso->tso_flags & TSOF_CLOSING));

#if 0
	/*
	 * XXX
	 * this would have meant that the T_ORDREL_IND would not be sent
	 * this seems bad if so, so always send it
	 *
	 * a T_DISCON_IND will cause the queues to be flushed, so who cares
	 * about flow control?
	 */
	if (! canput(tpiso->tso_rq->q_next)) 
		goto exit;
#endif
	/*
	 * Look for connection state changes
	 * I believe that the original algorithm was to send T_ORDREL_IND
	 * if we were not waiting for a connection reply, otherwise to send
	 * T_DISCON_IND.
	 *
	 * The new algorithm attempts to generate a T_DISCON_IND in cases
	 * where the peer has sent a RST.  This is done by checking for
	 * ECONNRESET in so_error.
	 *
	 * This, combined with setting SO_LINGER on but with a zero time value
	 * prior to calling sodisconnect() should make us do what most SVR4
	 * stacks do, namely map T_DISCON_REQ to RST and back to T_DISCON_IND,
	 * leaving T_ORDREL_REQ to generate a FIN and a subsequent T_ORDREL_IND
	 * on the peer.
	 */
	if ((so->so_state & (SS_ISDISCONNECTING |
			     SS_CANTSENDMORE | SS_CANTRCVMORE)) ||
			    !(so->so_state & SS_ISCONNECTED) ||
			    (so->so_q != NULL && so->so_q != so)) {

		if ((so->so_state & SS_CANTRCVMORE) &&
		    !(tpiso->tso_flags & 
		       	    (TSOF_RECV_ORDREL | TSOF_CONN_REQ_FAILED)) &&
		    !(so->so_error == ECONNRESET)) {
#ifdef _TPI_DEBUG
			if (_tpiprintfs) {
			printf("K: tpiso 0x%x want ordrel ind state %d\n",
				tpiso,tpiso->tso_state);
			}
#endif	/* _TPI_DEBUG */
			switch (tpiso->tso_state) {
			case TS_DATA_XFER:
				tpiso->tso_state = TS_WREQ_ORDREL;
				break;
			case TS_WIND_ORDREL:
				tpiso->tso_state = TS_IDLE;
				break;
			default:
				/* not legal to deliver ordrel ind now */
				goto check_conn;
			}
			bp = tpisocket_allocb(tpiso->tso_rq,
					sizeof(struct T_ordrel_ind),
					BPRI_HI);
			if (bp == NULL) 
				goto exit;
			bp->b_datap->db_type = M_PROTO;
			bp->b_wptr = bp->b_rptr + sizeof(struct T_ordrel_ind);
			pi = (union T_primitives *)bp->b_rptr;
			pi->ordrel_ind.PRIM_type = T_ORDREL_IND;

			/* removed when 438858 is fixed */
			ASSERT(!(tpiso->tso_flags & TSOF_CLOSING));
#ifdef _TPI_DEBUG
			if (_tpiprintfs) {
			 printf("K: tpiso 0x%x ordrel ind new state %d\n",
				tpiso,tpiso->tso_state);
			}
#endif	/* _TPI_DEBUG */
			putnext(tpiso->tso_rq, bp);
			tpiso->tso_flags |= TSOF_RECV_ORDREL;
			goto exit;
		}
		if (((so->so_state & (SS_CANTSENDMORE | SS_CANTRCVMORE))
				== (SS_CANTSENDMORE | SS_CANTRCVMORE)) &&
		    !(tpiso->tso_flags & TSOF_RECV_DISCON)) {
#ifdef _TPI_DEBUG
			if (_tpiprintfs) {
			 printf("K: tpiso 0x%x want discon ind state %d\n",
				tpiso,tpiso->tso_state);
			}
#endif	/* _TPI_DEBUG */
			switch (tpiso->tso_state) {
			case TS_DATA_XFER:
			case TS_WIND_ORDREL:
			case TS_WREQ_ORDREL:
			case TS_WCON_CREQ:
				break;
			default:
				/* not legal to send disconnect now */
				goto check_conn;
			}

			bp = tpisocket_allocb(tpiso->tso_rq,
			      sizeof(struct T_discon_ind), BPRI_HI);
			if (bp == NULL)
				goto exit;

			/* removed when 438858 is fixed */
			ASSERT(!(tpiso->tso_flags & TSOF_CLOSING));

			tpisocket_send_discon_ind(tpiso, bp, so->so_error,
				(long)-1);
			tpiso->tso_state = TS_IDLE;
#ifdef _TPI_DEBUG
			if (_tpiprintfs) {
			 printf("K: tpiso 0x%x discon ind new state %d\n",
				tpiso,tpiso->tso_state);
			}
#endif	/* _TPI_DEBUG */
			goto exit;
		}
check_conn:
		if (!(so->so_state & SS_ISCONNECTED) &&
		    (so->so_options & SO_ACCEPTCONN) &&
		    (so->so_q != NULL && so->so_q != so)) {
#ifdef _TPI_DEBUG
			if (_tpiprintfs) {
				printf("K: scanning conns 0x%x %d\n",
					tpiso,tpiso->tso_state);
			}
#endif	/* _TPI_DEBUG */
			tpisocket_scan_conn_q(tpiso, so);
		}
	}
exit:
	SOCKET_UNLOCK(so);
done:
	TRACEME(0x67, (void *)tpiso);
	TRACEME(0x68, (void *)tpiso->tso_count);
        TSO_RELE(tpiso);

#ifdef _TPISOCKD
	if ((tpiso->tso_count == 1) && (tpiso->tso_flags & TSOF_CLOSING)) {
		sv_broadcast(&tpisockd_inuse);
	}
#endif
	return(0);
#undef tpictl
}

/*
 * tpisocket_do_optmgmt
 * This routine performs options processing for SVR4-style protocols
 * When the XTI optmgmt is requested the omr->PRIM_type will be equal
 * to _XTI_T_OPTMGMT_REQ.
 */
int
tpisocket_do_optmgmt(
	struct	tpisocket *tpiso,
	int	*more_to_do_p,
	struct opproc  *functions)
{
	struct T_optmgmt_req *omr;
	int		flags;
	int		error = 0;
	struct opproc	*f;
	struct opthdr	*opt, *nopt, *eopt;
	int		level;
	mblk_t		*mp = NULL;
	mblk_t		*bp;

	bp = tpiso->tso_pending_optmgmt_req;
	if (bp == NULL ||
	    (bp->b_wptr - bp->b_rptr) < sizeof(struct T_optmgmt_req))
		return -TSYSERR;
	omr = (struct T_optmgmt_req *)bp->b_rptr;
	if ((omr->OPT_offset + omr->OPT_length) >
			(bp->b_wptr - bp->b_rptr))
		return -TBADOPT;

	switch (flags = omr->MGMT_flags) {
	case T_CHECK:
	case T_NEGOTIATE:
		if (!(mp = tpisocket_allocb(tpiso->tso_wq, 64, BPRI_MED))) {
			*more_to_do_p = TSCO_MORE;
			return 0;
		}
		mp->b_datap->db_type = M_PCPROTO;

		if (omr->PRIM_type == _XTI_T_OPTMGMT_REQ) {
			error = tpisocket_do_xtioptmgmt(tpiso, omr, bp, mp,
					more_to_do_p);
			if (error && mp)
				freemsg(mp);
			return error;
		}
		opt = (struct opthdr *)(bp->b_rptr + omr->OPT_offset);
		eopt = (struct opthdr *)((char *)opt + omr->OPT_length);
		if ((char *)eopt > (char *)bp->b_wptr) {
			error = -TSYSERR;
			goto done;
		}
		do {
			nopt = (struct opthdr *)((char *)(opt + 1) + opt->len);
			if (nopt > eopt) {
				error = -TBADOPT;
				goto done;
			}
			level = opt->level;
			for (f = functions; f->func; f++) {
				if (f->level == level) {
					if (error = (*f->func)(tpiso, omr, 
								opt, mp))
						goto done;
					break;
				}
			}
			if (!f->func) {
				if (flags == T_CHECK)
					omr->MGMT_flags = T_FAILURE;
				else
					error = -TBADOPT;
				goto done;
			}
			if (flags == T_CHECK && omr->MGMT_flags == T_FAILURE)
				goto done;
		} while ((opt = nopt) < eopt);
		if (flags == T_CHECK)
			omr->MGMT_flags = T_SUCCESS;
		break;

	case T_CURRENT:
	case T_DEFAULT:
		if (!(mp = tpisocket_allocb(tpiso->tso_wq, 256, BPRI_MED))) {
			*more_to_do_p = TSCO_MORE;
			return 0;
		}
		mp->b_datap->db_type = M_PCPROTO;

		if (omr->PRIM_type == _XTI_T_OPTMGMT_REQ) {
			error = tpisocket_do_xtioptmgmt(tpiso, omr, bp, mp,
					more_to_do_p);
			if (error && mp)
				freemsg(mp);
			return error;
		}

		for (f = functions; f->func; f++) {
			if (error = (*f->func)(tpiso, omr, 0, mp))
				break;
		}
		break;

	default:
		error = - TBADFLAG;
		break;
	}

done:
	if (error && mp)
		freemsg(mp);
	if (error)
		return error;

	tpiso->tso_pending_optmgmt_req = NULL;
	omr->PRIM_type = T_OPTMGMT_ACK;
	omr->OPT_offset = sizeof(struct T_optmgmt_ack);
	bp->b_datap->db_type = M_PCPROTO;
	omr->OPT_length = xmsgsize(mp, 1);
	bp->b_band = 1;
	bp->b_wptr = bp->b_rptr + sizeof(struct T_optmgmt_ack);
	if (bp->b_cont)
		freemsg(bp->b_cont);
	bp->b_cont = mp;
	/*
	 * The types of other blocks in the message are irrelevant at this
	 * point.
	 */
	pullupmsg(bp, -1);
	putnext(tpiso->tso_rq, bp);
	*more_to_do_p = TSCO_DONE_NO_ACK;
	return 0;
}

/*
 *  For XTI set the worst single result into the MGMT_flags field
 */
static int
tpisocket_xti_MGMT_flags(mblk_t *bp, struct T_optmgmt_req *omr)
{
	struct	t_opthdr *topt;
	struct	t_opthdr *curopt;
	int	cnt;
	int	successflag = 0;
	int	partsuccessflag = 0;
	int	failureflag = 0;
	int	readonlyflag = 0;
	int	curstatus;

	topt = (struct t_opthdr *)(bp->b_rptr + sizeof(struct T_optmgmt_req));

	for (curopt = topt, cnt = 0; curopt;
		curopt = OPT_NEXTHDR(topt, omr->OPT_length, curopt), cnt++)
	{
		switch(curstatus = curopt->status) {
		case T_NOTSUPPORT:
			/* "worst" status no need to check further */
			break;
		case T_READONLY:
			readonlyflag++;
			continue;
		case T_FAILURE:
			failureflag++;
			continue;
		case T_PARTSUCCESS:
			partsuccessflag++;
			continue;
		case T_SUCCESS:
			successflag++;
			continue;
		default:
			return(0);
		}
		break;
	}
	/*
	 * If only one option result or status is T_NOTSUPPORT,
	 */
	if (curstatus == T_NOTSUPPORT || cnt == 1) {
		return(curstatus);
	} else if (readonlyflag) {
		return(T_READONLY);
	} else if (failureflag) {
		return(T_FAILURE);
	} else if (partsuccessflag) {
		return(T_PARTSUCCESS);
	} else if (successflag) {
		return(T_SUCCESS);
	}
	return(0);
}

#define OPT_BUFFER_SIZE	64
#ifdef _TPI_DEBUG
#define opt_bcopy(s, d, l) { \
	ASSERT_ALWAYS(l < 64); \
	bcopy((s), (d), (l)); \
	}
#define opt_bzero(s, l) { \
	ASSERT_ALWAYS(l < 64); \
	bzero((s), (l)); \
	}
#else
#define opt_bzero(s, l) bzero((s), (l))
#define opt_bcopy(s, d, l) bcopy((s), (d), (l))
#endif

/*
 * tpisocket_makeopt
 * Append formatted option (opthdr + value) to mblk.
 * Extends buffer if possible and necessary.  
 * Returns 0 if unable to allocate space, 1 if successful.
 */
int
tpisocket_makeopt(
	mblk_t	*bp,		/* append formatted option to this mblk */
	int	level,
	int	name,
	void	*ptr,		/* buffer with option value */
	int	len)		/* length of buffer */
{
	struct opthdr  *opt;
	int             rlen, tlen;

	for (; bp->b_cont; bp = bp->b_cont)
		;
	rlen = OPTLEN(len);
	tlen = sizeof(struct opthdr) + rlen;
	if ((bp->b_datap->db_lim - bp->b_wptr) < tlen) {
	   if (!(bp->b_cont = allocb(max(tlen, OPT_BUFFER_SIZE), BPRI_MED)))
			return 0;
		bp = bp->b_cont;
		bp->b_datap->db_type = M_PCPROTO;
	}
	opt = (struct opthdr *)bp->b_wptr;
	opt->level = level;
	opt->name = name;
	opt->len = rlen;
	if (rlen) {
		opt_bzero(OPTVAL(opt), rlen);
	}
	if (len) {
		opt_bcopy(ptr, OPTVAL(opt), len);
	}
	bp->b_wptr += tlen;
	return 1;
}

/*
 * tpisocket_makextiopt
 * Append formatted option (t_opthdr + value) to mblk.
 * Extends buffer if possible and necessary.  
 * Returns 0 if unable to allocate space, 1 if successful.
 */
static int
tpisocket_makextiopt(
	mblk_t	*bp,		/* append formatted option to this mblk */
	struct t_opthdr *curr_outopt,	/* t_opthdr struct added to the mblk */
	void	*ptr,		/* buffer with option value */
	int	len)		/* length of buffer */
{
	struct t_opthdr	*topt;
	int		rlen, tlen;
	int cplen;

	for (; bp->b_cont; bp = bp->b_cont)
		;
	if (!curr_outopt->len) {
		rlen = len;
		tlen = T_ALIGN(sizeof(struct t_opthdr) + rlen);
	} else {
		rlen = 0;
		tlen = T_ALIGN(curr_outopt->len);
	}
	cplen = max(tlen, len + sizeof(*topt));
	if ((bp->b_datap->db_lim - bp->b_wptr) < cplen) {
		if (!(bp->b_cont = allocb(OPT_BUFFER_SIZE, BPRI_MED)))
			return 0;
		bp = bp->b_cont;
		bp->b_datap->db_type = M_PCPROTO;
	}
	topt = (struct t_opthdr *)bp->b_wptr;
	topt->len = tlen;
	topt->level = curr_outopt->level;
	topt->name = curr_outopt->name;
	topt->status = curr_outopt->status;
#ifdef _TPI_DEBUG
	if (_tpiprintfs > 2) {
		printf("K: makextiopt 0x%x 0x%x 0x%x 0x%x\n",
			topt->level, topt->name, topt->len, topt->status);
	}
#endif /* _TPI_DEBUG */
	if (ptr) {
		if (rlen) {
			opt_bzero((void *)(topt + 1), rlen);
		}
		if (len) {
#ifdef _TPI_DEBUG
			if (_tpiprintfs > 4) {
				if ((len + sizeof(*topt)) != tlen) {
				printf("K: makextiopt 0x%x 0x%x 0x%x 0x%x\n",
					       topt->level, topt->name, 
					       topt->len, topt->status);
	printf("K: len = %d, sizeof(*topt) = %d, tlen = %d, l+st = %d\n",
					       len, sizeof(*topt), tlen,
					       len + sizeof(*topt));
	printf("K: lim = 0x%x, len+st 0x%x, tlen 0x%x cplen 0x%x\n",
					bp->b_datap->db_lim, 
					bp->b_wptr + (sizeof(*topt)+ len),
					bp->b_wptr + tlen,
					bp->b_wptr + cplen);
				}
			}
#endif
			opt_bcopy(ptr, (void *)(topt + 1), len);
		}
	}
	bp->b_wptr += tlen;
	return 1;
}

/*
 * tpisocket_optmgmt
 * return 0 if not error, or errno, or negative Terrno.
 */
int
tpisocket_optmgmt(
	struct	tpisocket *tpiso,
	struct T_optmgmt_req *req,	/* from original request */
	struct opthdr  *opt,		/* from original request */
	mblk_t         *mp)		/* append answer to this mblk */
{
#define tpictl (tpiso->tso_control)
	struct	vsocket *vso = tpiso->tso_vsocket;
	struct	mbuf	*m;
	struct	mbuf	*m2;
	int		error;

	switch (req->MGMT_flags) {

	case T_NEGOTIATE:
		/* m2 will hold copy of OPTVAL, discarded by sosetopt.  */
		MGET(m2, M_DONTWAIT, MT_SOOPTS);
		if (m2 == NULL)
			return ENOSR;
		m2->m_len = 0;
		if (opt->len > M_TRAILINGSPACE(m2)) {
			m_freem(m2);
			return -TBADOPT;
		}
		m2->m_len = opt->len;
		bcopy(OPTVAL(opt), mtod(m2, caddr_t), opt->len);

		/* m is allocated to hold response */
		m = tpiso->tso_pending_optmgmt_mbuf;
		tpiso->tso_pending_optmgmt_mbuf = NULL;
		if (m == NULL) {
			MGET(m, M_DONTWAIT, MT_SOOPTS);
			if (m == NULL) {
				m_free(m2);
				return ENOSR;
			}
		}

		switch (opt->name) {
		case SO_LINGER:
		case SO_DEBUG:
		case SO_KEEPALIVE:
		case SO_DONTROUTE:
		case SO_USELOOPBACK:
		case SO_BROADCAST:
		case SO_REUSEADDR:
		case SO_OOBINLINE:
		case SO_SNDLOWAT:
		case SO_RCVLOWAT:
		case SO_SNDBUF:
		case SO_RCVBUF:
			VSOP_SETATTR(vso, SOL_SOCKET, opt->name, m2, error);
			/* sosetopt frees m2 */
			break;

		case SO_IMASOCKET:
			if ((*(mtod(m2, int *))))
				tpiso->tso_flags |= TSOF_SO_IMASOCKET;
			else
				tpiso->tso_flags &= ~TSOF_SO_IMASOCKET;
			error = 0;
			m_free(m2);
			break;

		case SO_PROTOTYPE:
			/* The correct way to "negotiate" this parameter
			 * is to ignore what the user asks for, and 
			 * return the current value.
			 */
			error = 0;
			m_free(m2);
			break;

		default:
			error = EINVAL;
			m_free(m2);
			break;
		}

		if (error) {
			if (error == EINVAL)
				error = -TBADOPT;
			tpiso->tso_pending_optmgmt_mbuf = m;
			return error;
		}
		goto check_status;

	case T_CHECK:
		m = tpiso->tso_pending_optmgmt_mbuf;
		tpiso->tso_pending_optmgmt_mbuf = NULL;
		if (m == NULL) {
			MGET(m, M_DONTWAIT, MT_SOOPTS);
			if (m == NULL)
				return ENOSR;
		}
check_status:
		switch (opt->name) {
		case SO_LINGER:
		case SO_DEBUG:
		case SO_KEEPALIVE:
		case SO_DONTROUTE:
		case SO_USELOOPBACK:
		case SO_BROADCAST:
		case SO_REUSEADDR:
		case SO_OOBINLINE:
		case SO_SNDLOWAT:
		case SO_RCVLOWAT:
		case SO_SNDBUF:
		case SO_RCVBUF:
		case SO_TYPE:
			VSOP_GETATTR(vso, SOL_SOCKET, opt->name, &m, error);
			if (error) {
				ASSERT(error != ENOPROTOOPT);
				if (error == EINVAL)
					error = -TBADOPT;
				m_freem(m);
				return error;
			}
			/*
			 * for binary options, force "true" value to 1
			 */
			switch (opt->name) {
			case SO_DEBUG:
			case SO_KEEPALIVE:
			case SO_DONTROUTE:
			case SO_USELOOPBACK:
			case SO_BROADCAST:
			case SO_REUSEADDR:
			case SO_OOBINLINE:
				if (m->m_len == sizeof(int) &&
				    *mtod(m, int *) != 0)
					*mtod(m, int *) = 1;
				break;
			case SO_LINGER:
				if (m->m_len == sizeof(struct linger) &&
				    mtod(m, struct linger *)->l_onoff != 0)
					mtod(m,struct linger *)->l_onoff = 1;
				break;
  			}
			break;

		case SO_IMASOCKET:
			*mtod(m, int *) = 
				((tpiso->tso_flags & TSOF_SO_IMASOCKET) != 0);
			m->m_len = sizeof(int);
			break;

		case SO_PROTOTYPE:
			*mtod(m, int *) = TSC_PROTOCOL;
			m->m_len = sizeof(int);
			break;

		default:
			req->MGMT_flags = T_FAILURE;
			tpiso->tso_pending_optmgmt_mbuf = m;
			return 0;
		}
		if (! tpisocket_makeopt(mp, SOL_SOCKET, opt->name,
				mtod(m, caddr_t), m->m_len)) {
			tpiso->tso_pending_optmgmt_mbuf = m;
			return ENOSR;
		}
		tpiso->tso_pending_optmgmt_mbuf = m;
		break;

	case T_CURRENT:
		break;

	case T_DEFAULT:
	{
		struct optdefault *o;
		int		val;

		if (req->PRIM_type == _XTI_T_OPTMGMT_REQ) {
			break;
		}

		/* get default values from table */
		for (o = sockdefault; o->optname; o++) {
			if (! tpisocket_makeopt(mp, SOL_SOCKET,
					     o->optname,
					     o->val, o->len))
				return ENOSR;
		}

		/* add default values that aren't in the table */
		val = TSC_DEFAULT_VALUE(tpiso, ((__psunsigned_t)TSCD_SNDBUF));
		if (! tpisocket_makeopt(mp, SOL_SOCKET, 
					SO_SNDBUF, &val, sizeof(int)))
			return ENOSR;
		val = TSC_DEFAULT_VALUE(tpiso, ((__psunsigned_t)TSCD_RCVBUF));
		if (! tpisocket_makeopt(mp, SOL_SOCKET, 
					SO_RCVBUF, &val, sizeof(int)))
			return ENOSR;
		val = TSC_DEFAULT_VALUE(tpiso, ((__psunsigned_t)TSCD_SNDLOWAT));
		if (! tpisocket_makeopt(mp, SOL_SOCKET,
					SO_SNDLOWAT, &val, sizeof(int)))
			return ENOSR;
		val = TSC_DEFAULT_VALUE(tpiso, ((__psunsigned_t)TSCD_RCVLOWAT));
		if (! tpisocket_makeopt(mp, SOL_SOCKET,
				        SO_RCVLOWAT, &val, sizeof(int)))
			return ENOSR;

		break;
	}

	default:
		break;
	}

	return 0;
#undef tpictl
}

#ifdef _IRIX_LATER
/*
 * tpisocket_convert_socket
 * Convert a socket file descriptor to a TPI stream file descriptor
 */
int
tpisocket_convert_socket(struct	file *fp, struct cred *cred)
{
	struct	socket *so;
	struct  vsocket *vsoo;
	struct	socket *oso;
	struct	tpisocket_control *tpictl;
	dev_t	newdev;
	struct	vnode *vp, *openvp;
	int	error;
	int	newmajor;
	int	newminor;
	struct	tpisocket *tpiso;

	if (fp->f_type != DTYPE_SOCKET)
		return EINVAL;

	so = (struct socket *)fp->vf_data;

	/* calculate the clone device for the protocol */
	for (tpictl = tpisocket_protocols;
	     tpictl != NULL;
	     tpictl = tpictl->tsc_next)
		if (so->so_type == TSC_SOCKET_TYPE &&
		    so->so_proto->pr_protocol == TSC_PROTOCOL &&
		    so->so_proto->pr_domain->dom_family == TSC_DOMAIN)
			break;
	if (tpictl == NULL || tpisocket_clone_major < 0)
		return EPROTONOSUPPORT;
	newdev = makedevice(tpisocket_clone_major, TSC_MAJORS[0]);

	/*
	 * create a stream via a clone open
	 */
	openvp = vp = make_specvp(newdev, VCHR);

	ASSERT(vp != NULL);
	VOP_OPEN(openvp, &vp, FWRITE|FREAD|FNDELAY, cred, error);
	if (error) {
		VN_RELE(vp);
		return error;
	}

	/* locate the tpisocket structure for the device */
	newmajor = getemajor(vp->v_rdev);
	newminor = getminor(vp->v_rdev);
	if (TSC_MAJORS[0] != newmajor ||
	    newminor >= TSC_MAXDEVICES || 
	    NULL == (tpiso = TSC_DEVICES[newminor]) ||
	    !(tpiso->tso_flags & TSOF_UNUSED)) {
		VOP_CLOSE(vp, FWRITE|FREAD|FNDELAY, L_TRUE, cred, error);
		VN_RELE(vp);
		return EPROTONOSUPPORT;
	}

	/* change the file descriptor to a streams descriptor */
	fp->f_type = DTYPE_VNODE;
	fp->f_ops = &vnodefops;
	VF_SET_DATA(fp, vp);

	/* swap the original socket for the unused new socket */
	voso = tpiso->tso_vsocket;
	oso = (struct socket *)(BHV_PDATA(VSOCK_TO_FIRST_BHV(vsoo)));
	oso->so_state        &= ~SS_WANT_CALL;
	oso->so_snd.sb_flags &= ~SB_NOTIFY;
	so->so_callout        = tpisocket_event;
	so->so_callout_arg    = (caddr_t)tpiso;
	ASSERT(tpiso->tso_rq->q_monpp);
	so->so_monpp          = tpiso->tso_rq->q_monpp;
	so->so_state         |= (SS_TPISOCKET | SS_NBIO | SS_WANT_CALL);
	so->so_snd.sb_flags  |= SB_NOTIFY;
	so->so_state         &= ~SS_NOFDREF;
	tpiso->tso_flags     &= ~TSOF_UNUSED;
	tpiso->tso_flags     |= TSOF_ADDRESS_BOUND;

	/* queue old socket for discard */
	oso->so_callout_arg    = (caddr_t)tpisocket_discard_list; 
	tpisocket_discard_list = oso;
	wakeup(&tpisocket_discard_list);

	/* wakeup the new socket to look at the current state */
	tpisocket_event(so, SSCO_INPUT_ARRIVED,     tpiso);
	tpisocket_event(so, SSCO_OOB_INPUT_ARRIVED, tpiso);
	tpisocket_event(so, SSCO_OUTPUT_AVAILABLE,  tpiso);

	return 0;
}
#endif /* _IRIX_LATER */

/*
 * tpisocket_sioc_name - Process an SIOC*NAME ioctl
 */
int
tpisocket_sioc_name(struct tpisocket *tpiso, mblk_t *bp, int pru)
{
	struct vsocket	*vso = tpiso->tso_vsocket;
	struct socket	*so =
		(struct socket *)(BHV_PDATA(VSOCK_TO_FIRST_BHV(vso)));
	struct	mbuf 	*m = tpiso->tso_temporary_mbuf;
	mblk_t		*rbp;
	int		error;

	if ((so->so_state & (SS_ISCONNECTED|SS_ISCONFIRMING)) == 0)
		return tpisocket_send_iocnak(tpiso->tso_wq, bp, ENOTCONN);

	m->m_len = 0;
	rbp = tpisocket_allocb(tpiso->tso_wq, M_TRAILINGSPACE(m), BPRI_MED);
	if (rbp == NULL) {
		TSO_PUTBWQ(tpiso, bp);
		return 0;
	}
	SOCKET_LOCK(so);
	error = (*so->so_proto->pr_usrreq)(so, pru, 0, m, 0);
	SOCKET_UNLOCK(so);
 	if (error) {
		freemsg(rbp);
		return tpisocket_send_iocnak(tpiso->tso_wq, bp, error);
	}
	rbp->b_wptr = rbp->b_rptr + m->m_len;
	bcopy(mtod(m, caddr_t), rbp->b_rptr, m->m_len);
#ifdef RENO
	((struct osockaddr *)rbp->b_rptr)->sa_family =
		((struct sockaddr *)rbp->b_rptr)->sa_family;
#endif
	return tpisocket_send_iocack(tpiso->tso_wq, bp, rbp);
}

/*
 * tpisocket_ti_name - Process a TI_*NAME ioctl
 */
int
tpisocket_ti_name(struct tpisocket *tpiso, mblk_t *bp)
{
	struct tpisocket_control *tpictl = tpiso->tso_control;
	mblk_t	*rbp;
	int	local_size;
	int	remote_size;
	int	error;

	local_size = TSC_ADDRESS_SIZE(tpiso, TSCA_LOCAL_ADDRESS, NULL);
	remote_size = TSC_ADDRESS_SIZE(tpiso, TSCA_REMOTE_ADDRESS, NULL);
	rbp = tpisocket_allocb(tpiso->tso_wq, local_size + remote_size,
		BPRI_MED);
	if (rbp == NULL) {
		TSO_PUTBWQ(tpiso, bp);
		return 0;
	}
	rbp->b_wptr = rbp->b_rptr + local_size + remote_size;
	error = TSC_CONVERT_ADDRESS(tpiso, (char *)rbp->b_rptr,
		    TSCA_LOCAL_ADDRESS | TSCA_MODE_READ, local_size, 
		    NULL, NULL);
	if (error) {
free_after_error:
		freemsg(rbp);
		switch (bp->b_datap->db_type) {
		case M_IOCTL:
			return tpisocket_send_iocnak(tpiso->tso_wq, bp, error);
		case M_IOCDATA:
			tpiso->tso_flags &= ~TSOF_IOCTL_PENDING;
			tpiso->tso_pending_ioctl = TSOI_NULL;
			break;
		default:
			break;
		}
		freemsg(bp);
		return 1;
	}
	error = TSC_CONVERT_ADDRESS(tpiso, (char *)rbp->b_rptr + local_size,
		    TSCA_REMOTE_ADDRESS | TSCA_MODE_READ, remote_size, 
		    NULL, NULL);
	if (error) 
		goto free_after_error;

	switch (bp->b_datap->db_type) {
	case M_IOCTL:
		tpiso->tso_flags |= TSOF_IOCTL_PENDING;
		tpiso->tso_pending_ioctl = TSOI_TI_NAME;
		break;

	case M_IOCDATA:
		if (tpiso->tso_flags & TSOF_IOCTL_PENDING &&
		    tpiso->tso_pending_ioctl == TSOI_TI_NAME) 
			break;	/* ok */
		/* else FALLTHRU */
	default:
		freemsg(bp);
		bp = 0;
		break;
	}
	if (bp &&
	    DONAME_CONT != ti_doname(tpiso->tso_wq, bp, 
				rbp->b_rptr,              local_size, 
				rbp->b_rptr + local_size, remote_size)) {
		tpiso->tso_flags &= ~TSOF_IOCTL_PENDING;
		tpiso->tso_pending_ioctl = TSOI_NULL;
	}
	freemsg(rbp);
	return 1;
}

/*
 * tpisocket_ifioctl - Process interface ioctl's
 */
int
tpisocket_ifioctl(struct tpisocket *tpiso, mblk_t *bp)
{
	struct iocblk *iocp;
	struct ifnet *ifp;
	struct ifreq *ifr;
	mblk_t	*nbp;
	struct vsocket *vso;
	struct socket *so;
	int	error;
	int	cmd;

	if (tpiso->tso_flags & TSOF_IOCTL_PENDING) {
		if (tpiso->tso_pending_ioctl_req) {
			freemsg(tpiso->tso_pending_ioctl_req);
			tpiso->tso_pending_ioctl_req = NULL;
		}
		tpiso->tso_flags &= ~TSOF_IOCTL_PENDING;
		tpiso->tso_pending_ioctl = TSOI_NULL;
	}
	iocp = (struct iocblk *)bp->b_rptr;
	nbp = bp->b_cont;
	if (iocp->ioc_count > 0 &&
	    (nbp == NULL || (msgdsize(nbp) < iocp->ioc_count)))
		return tpisocket_send_iocnak(tpiso->tso_wq, bp, EINVAL);
	cmd = iocp->ioc_cmd;
	switch (cmd) {
	case SIOCGIFCONF:
	case SIOCGIFCONF_INTERNAL:
		/* XXXpaleph Locking??? */
		return tpisocket_ifconf(tpiso, bp);

	case SIOCSARP:
	case SIOCDARP:
		if (tpiso->tso_cred->cr_uid != 0)
			return tpisocket_send_iocnak(tpiso->tso_wq, bp, EPERM);
		/* fall through */
	case SIOCGARP:
#ifdef notyet
		return tpisocket_arpioctl(tpiso, bp);
#else /* notyet */
		return tpisocket_send_iocnak(tpiso->tso_wq, bp, EOPNOTSUPP);
#endif /* notyet */
	case SIOCSIFFLAGS:
	case SIOCSIFMETRIC:
		if (tpiso->tso_cred->cr_uid != 0)
			return tpisocket_send_iocnak(tpiso->tso_wq, bp, EPERM);
		/* fall through */
	default:
		break;
	}

	if (iocp->ioc_count < sizeof(struct ifreq) ||
	    (nbp->b_wptr - nbp->b_rptr) < sizeof(struct ifreq))
		return tpisocket_send_iocnak(tpiso->tso_wq, bp, EINVAL);
	ifr = (struct ifreq *)nbp->b_rptr;
	ifp = ifunit(ifr->ifr_name);
	if (ifp == 0)
		return tpisocket_send_iocnak(tpiso->tso_wq, bp, ENXIO);
	vso = tpiso->tso_vsocket;
	so = (struct socket *)(BHV_PDATA(VSOCK_TO_FIRST_BHV(vso)));
	error = 0;

	switch (cmd) {
#ifdef _RISCOS
	case SIOCGCTLR:
#ifdef notyet
		if (ifp->if_ioctl)
			error = (*ifp->if_ioctl)(ifp, cmd, nbp->b_rptr);
#else /* notyet */
		error = EOPNOTSUPP;
#endif /* notyet */
		break;
#endif /* _RISCOS */

	case SIOCGIFFLAGS:
#ifdef _RISCOS
		ifr->ifr_flags = ifp->if_flags &
			~(IFF_OACTIVE | IFF_SIMPLEX | IFF_LANCE);
#else /* _RISCOS */
		ifr->ifr_flags = ifp->if_flags;
#endif /* _RISCOS */
		break;

	case SIOCGIFMETRIC:
		ifr->ifr_metric = ifp->if_metric;
		break;

	case SIOCSIFFLAGS:
#ifdef notyet
#ifdef _RISCOS

		if ((ifp->if_flags & IFF_UP) != (ifr->ifr_flags & IFF_UP)) {
			ifp->if_lastchange = time;
			timevalsub(&ifp->if_lastchange, &boottime);
		}
#endif /* _RISCOS */

		if (ifp->if_flags & IFF_UP && 
		    (ifr->ifr_flags & IFF_UP) == 0) {
			if_down(ifp);
		}
		ifp->if_flags = (ifp->if_flags & IFF_CANTCHANGE) |
			(ifr->ifr_flags &~ IFF_CANTCHANGE);
		if (ifp->if_ioctl)
			(void)(*ifp->if_ioctl)(ifp, cmd, nbp->b_rptr);
#else /* notyet */
		error = EOPNOTSUPP;
#endif /* notyet */
		break;

	case SIOCSIFMETRIC:
		ifp->if_metric = ifr->ifr_metric;
		break;

	default:
		if (so->so_proto == 0) {
			error = EOPNOTSUPP;
			break;
		}
#ifndef notyet
		switch (cmd) {
		case SIOCGIFDSTADDR:
		case SIOCGIFADDR:
		case SIOCGIFBRDADDR:
		case SIOCGIFNETMASK:
			break;
		default:
			return tpisocket_send_iocnak(tpiso->tso_wq, bp, 
				EOPNOTSUPP);
		}
#endif /* notyet */
#if !(defined(COMPAT_43) && defined(RENO))
		SOCKET_LOCK(so);
		error = ((*so->so_proto->pr_usrreq)(so, PRU_CONTROL,
				    (struct mbuf *)((__psunsigned_t)cmd),
						    (struct mbuf *)nbp->b_rptr,
						    (struct mbuf *) ifp));
		SOCKET_UNLOCK(so);
#else
		{
			int ocmd = cmd;

			switch (cmd) {

			case SIOCSIFDSTADDR:
			case SIOCSIFADDR:
			case SIOCSIFBRDADDR:
			case SIOCSIFNETMASK:
#if BYTE_ORDER != BIG_ENDIAN
				if (ifr->ifr_addr.sa_family == 0 &&
				    ifr->ifr_addr.sa_len < 16) {
					ifr->ifr_addr.sa_family = ifr->ifr_addr.sa_len;
					ifr->ifr_addr.sa_len = 16;
				}
#else
				if (ifr->ifr_addr.sa_len == 0)
					ifr->ifr_addr.sa_len = 16;
#endif
				break;
			}
			SOCKET_LOCK(so);
			error =  ((*so->so_proto->pr_usrreq)(so, PRU_CONTROL,
						cmd, nbp->b_rptr, ifp));
			SOCKET_UNLOCK(so);
		}
		break;
#endif
	}
	if (error)
		return tpisocket_send_iocnak(tpiso->tso_wq, bp, error);
	bp->b_cont = NULL;
	return tpisocket_send_iocack(tpiso->tso_wq, bp, nbp);

}

/*
 * tpisocket_ifiocdata - Process M_IOCDATA continuation of ifioctl.
 */
int
tpisocket_ifiocdata(struct tpisocket *tpiso, mblk_t *bp)
{
	struct iocblk *iocp;
	struct copyresp *crs;
	mblk_t *rbp;
	int	error;

	if (tpiso->tso_pending_ioctl_req == NULL) {
		freemsg(bp);
		return 1;
	}

	iocp = (struct iocblk *)tpiso->tso_pending_ioctl_req->b_rptr;
	switch (iocp->ioc_cmd) {
	case SIOCGIFCONF:
	case SIOCGIFCONF_INTERNAL:
		break;

	default:
		freemsg(bp);
		return 1;
	}

	if ((bp->b_wptr - bp->b_rptr) < sizeof(struct copyresp)) {
		freemsg(bp);
		return 1;
	}
	crs = (struct copyresp *)bp->b_rptr;
	if (crs->cp_id != iocp->ioc_id) {
		freemsg(bp);
		return 1;
	}
	error = (__psunsigned_t)crs->cp_rval;
	freemsg(bp);
	bp = tpiso->tso_pending_ioctl_req;
	tpiso->tso_pending_ioctl_req = NULL;
	tpiso->tso_flags &= ~TSOF_IOCTL_PENDING;
	tpiso->tso_pending_ioctl = TSOI_NULL;
	if (error != 0) 
		return tpisocket_send_iocnak(tpiso->tso_wq, bp, error);
	rbp = bp->b_cont;
	bp->b_cont = NULL;
	return tpisocket_send_iocack(tpiso->tso_wq, bp, rbp);

}

/*
 * tpisocket_ifconf - Process SIOCGIFCONF ioctl
 */
int
tpisocket_ifconf(struct tpisocket *tpiso, mblk_t *bp)
{
	register struct ifconf *ifc;
	register struct ifnet *ifp = ifnet;
	struct in_ifaddr *ia;
	struct ifaddr *ifa;
	char *cp, *ep;
	struct iocblk *iocp;
	struct copyreq *crq;
	mblk_t *rbp;
	mblk_t *copybp;
	int	space;
	int	copyoffset;
	int	i, j;
	struct ifreq ifr;
	char	idstringp[10];

	iocp = (struct iocblk *)bp->b_rptr;
	if (iocp->ioc_cmd == SIOCGIFCONF_INTERNAL) {
		rbp = bp;
		space = iocp->ioc_count;
		if (space <= 0)
			return tpisocket_send_iocnak(tpiso->tso_wq, bp,EINVAL);
	} else {
		if (iocp->ioc_count < sizeof(struct ifconf) ||
		    (bp->b_cont->b_wptr - bp->b_cont->b_rptr) 
		      < sizeof(struct ifconf))
			return tpisocket_send_iocnak(tpiso->tso_wq, bp,EINVAL);

		ifc = (struct ifconf *)bp->b_cont->b_rptr;
		space = ifc->ifc_len;
		if (space <= 0)
			return tpisocket_send_iocnak(tpiso->tso_wq, bp,EINVAL);
		if (space > 0x8000) {
			ifc->ifc_len = 0x8000;
			space = 0x8000;
		}
		rbp = tpisocket_allocb(tpiso->tso_rq, sizeof(struct copyreq), 
			BPRI_LO);
		if (rbp == NULL) {
			TSO_PUTBWQ(tpiso, bp);
			return 0;
		}
		copyoffset = space;
		copybp = rbp;
		while (copyoffset > 0) {
			copybp->b_cont = tpisocket_allocb(tpiso->tso_rq, 
						min(copyoffset, 0x1000), 
						BPRI_LO);
			if (copybp->b_cont == NULL) {
				freemsg(rbp);
				TSO_PUTBWQ(tpiso, bp);
				return 0;
			}
			copybp = copybp->b_cont;
			copybp->b_wptr = copybp->b_datap->db_lim;
			copyoffset -= (copybp->b_wptr - copybp->b_rptr);
		}
		rbp->b_datap->db_type = M_COPYOUT;
		crq = ((struct copyreq *)rbp->b_rptr);
		rbp->b_wptr += sizeof(struct copyreq);
		bzero(rbp->b_rptr, sizeof(struct copyreq));
		crq->cq_cmd = iocp->ioc_cmd;
		crq->cq_uid = iocp->ioc_uid;
		crq->cq_gid = iocp->ioc_gid;
		crq->cq_id = iocp->ioc_id;
		crq->cq_addr = (caddr_t)ifc->ifc_req;
		crq->cq_flag = STRCOPYTRANS;
	}
	copybp = rbp->b_cont;
	copyoffset = 0;
	ep = ifr.ifr_name + sizeof(ifr.ifr_name) - 2;
	for (; space > sizeof(ifr) && ifp; ifp = ifp->if_next) {
		bcopy(ifp->if_name, ifr.ifr_name, 
			sizeof(ifr.ifr_name) - 2);
		for (cp = ifr.ifr_name; cp < ep && *cp; cp++)
			;
		for (i = ifp->if_unit, j = sizeof(idstringp);
		     j > 0;) {
			idstringp[--j] = '0' + (i % 10);
			i /= 10;
			if (i == 0)
				break;
		}
		while (cp <= ep && j < sizeof(idstringp))
			*cp++ = idstringp[j++];
		*cp = 0;

		ia = (struct in_ifaddr *)(ifp->in_ifaddr);
		ifa = (ia) ? &(ia->ia_ifa) : 0;

		if (ifa == 0) {
			bzero((caddr_t)&ifr.ifr_addr, sizeof(ifr.ifr_addr));
			tpisocket_copy_to_mblk_chain(
				(caddr_t)&ifr, sizeof(ifr),
				&copybp, &copyoffset);
			space -= sizeof(ifr);
		} else for ( ; space > sizeof(ifr) && ifa; 
			       ifa = ifa->ifa_next) {
#ifdef RENO
			register struct sockaddr *sa = ifa->ifa_addr;
#ifdef COMPAT_43
			if (iocp->ioc_cmd == SIOCGIFCONF_INTERNAL) {
				struct osockaddr *osa =
				    (struct osockaddr *)&ifr.ifr_addr;
				ifr.ifr_addr = *sa;
				osa->sa_family = sa->sa_family;
				tpisocket_copy_to_mblk_chain(
					(caddr_t)&ifr, sizeof(ifr), 
					&copybp, &copyoffset);
			} else
#endif
			if (sa->sa_len <= sizeof(*sa)) {
				ifr.ifr_addr = *sa;
				tpisocket_copy_to_mblk_chain(
					(caddr_t)&ifr, sizeof(ifr),
					&copybp, &copyoffset);
			} else {
				space -= sa->sa_len - sizeof(*sa);
				if (space < sizeof(ifr))
					break;
				tpisocket_copy_to_mblk_chain(
					(caddr_t)&ifr,
					sizeof(ifr.ifr_name),
					&copybp, &copyoffset);
				tpisocket_copy_to_mblk_chain(
					(caddr_t)sa, sa->sa_len,
					&copybp, &copyoffset);
			}
#else /* !RENO */
			register struct sockaddr *sa = ifa->ifa_addr;
			ifr.ifr_addr = *sa;
			tpisocket_copy_to_mblk_chain(
				(caddr_t)&ifr, sizeof(ifr),
				&copybp, &copyoffset);
#endif

			space -= sizeof(ifr);
		}
	}
	if (iocp->ioc_cmd == SIOCGIFCONF_INTERNAL) {
#ifdef nolonger
		if (space) {
			tpisocket_clear_mblk_chain(space, &copybp, 
				&copyoffset);
			space = 0;
		}
#endif /* nolonger */
		if (copybp != NULL) {
			copybp->b_wptr = copybp->b_rptr + copyoffset;
			if (copybp->b_cont != NULL) {
				freemsg(copybp->b_cont);
				copybp->b_cont = NULL;
			}
		}
		bp->b_datap->db_type = M_IOCACK;
		bp->b_band = 1;
		iocp->ioc_count -= space;
	} else {
		ifc->ifc_len -= space;
		rbp->b_cont->b_wptr += ifc->ifc_len;
		crq->cq_size = ifc->ifc_len;
		tpiso->tso_pending_ioctl_req = bp;
		tpiso->tso_flags |= TSOF_IOCTL_PENDING;
		tpiso->tso_pending_ioctl = TSOI_IFIOCTL;
	}
	qreply(tpiso->tso_wq, rbp);

	return 1;
}

/*
 * tpisocket_rtioctl - Process routing ioctl's [STUB only for now]
 */
int
tpisocket_rtioctl(struct tpisocket *tpiso, mblk_t *bp)
{
	ASSERT (bp->b_datap->db_type == M_IOCTL);
	return tpisocket_send_iocnak(tpiso->tso_wq, bp, EOPNOTSUPP);
}

/* ARGSUSED */
int
tpisocket_rtiocdata(
	struct	tpisocket *tpiso,
	mblk_t	*bp)
{
	ASSERT (bp->b_datap->db_type == M_IOCDATA);
	freemsg(bp);
	return 1;
}

/*
 * XXX this seems to imply that there's no contention in turning on SB_LOCK
 */
int 
tpisocket_sblock(struct sockbuf *sb)
{
	ASSERT((sb->sb_flags & SB_LOCK) == 0);
	sb->sb_flags |= SB_LOCK;
	return 0;
}

void 
tpisocket_sbunlock(struct sockbuf *sb)
{
	ASSERT((sb->sb_flags & SB_LOCK) != 0);
	sb->sb_flags &= ~SB_LOCK;
}

/*
 * Send on a socket.
 *
 * This is an adaptation of the RISCOS/BSD_reno sosend for tpisocket.
 * It uses the reno feature of passing in an mbuf chain, 
 * and not using a uio.  To reduce bloat, much dead code was expunged.
 * Reno "control" buffers are not supported.
 * Formal parameters were kept the same as in reno.
 * 
 * If send must go all at once and message is larger than
 * send buffering, then hard error.
 * If must go all at once and not enough room now, then
 * inform user that this would block and do nothing.
 * Otherwise, if nonblocking, send as much as possible.
 * The data to be sent is described by the mbuf chain "top".  
 * Data provided in mbuf chain must be small enough to send all at once.
 *
 * Returns nonzero on error, timeout or signal; callers
 * must check for short counts if EINTR/ERESTART are returned.
 * Data and control buffers are freed on return.
 */
/* ARGSUSED */
int
tpisocket_sosend(
	register struct socket *so,
	struct mbuf 	*addr,
	struct uio	*uio,
	struct mbuf 	*top,
	struct mbuf	*control,
	int 		flags)
{
	register int	space;
	register int	resid;
	int 		atomic;
	int 		dontroute;
	int 		error;

#define	snderr(errno)	{ error = errno; goto release; }
#undef sblock
#define sblock(sb) tpisocket_sblock(sb)
#undef sbunlock
#define sbunlock(sb) tpisocket_sbunlock(sb)

	ASSERT(uio == NULL);
	ASSERT(control == NULL);

	atomic = sosendallatonce(so) || top;

	resid = m_length(top);
	dontroute = (flags & MSG_DONTROUTE) && 
		(so->so_options & SO_DONTROUTE) == 0 &&
		(so->so_proto->pr_flags & PR_ATOMIC);

	ASSERT((so->so_snd.sb_flags & SB_LOCK) == 0);
	if (error = sblock(&so->so_snd))
		goto out;

	if (so->so_state & SS_CANTSENDMORE)
		snderr(EPIPE);
	if (so->so_error)
		snderr(so->so_error);
	if ((so->so_state & SS_ISCONNECTED) == 0) {
		if (so->so_proto->pr_flags & PR_CONNREQUIRED) {
			if ((so->so_state & SS_ISCONFIRMING) == 0)
				snderr(ENOTCONN);
		} else if (addr == 0)
			snderr(EDESTADDRREQ);
	}
	space = sbspace(&so->so_snd);
	if (flags & MSG_OOB)
		space += 1024;
	if (space < resid &&
	    (atomic || space < so->so_snd.sb_lowat || space < 0)) {
		if (atomic && resid > so->so_snd.sb_hiwat) {
			snderr(EMSGSIZE);
		}
		snderr(EWOULDBLOCK);
	}
	if (dontroute)
		so->so_options |= SO_DONTROUTE;
	error = (*so->so_proto->pr_usrreq)(so,
			(flags & MSG_OOB) ? PRU_SENDOOB : PRU_SEND,
			top, addr, control);
	if (dontroute)
		so->so_options &= ~SO_DONTROUTE;
	top = 0;

release:
	sbunlock(&so->so_snd);
out:
	if (top)
		m_freem(top);
	return error;
}

/*
 * Implement receive operations on a socket.
 *
 * Also adapted from RISCOS/BSD_reno.
 * Supports only non-uio style output (mp0 != 0);
 * Ignores contents of uio completely.
 * Returns the next message (mbuf chain) in the receive sockbuf (for DGRAMs)
 * or the next non-empty mbuf (for STREAM).
 * Since, at most, one mbuf is copied, and then data is only copied if it's
 * not a "cluster", we don't splx() during this code.
 * NB: flags will be either 0 or MSG_OOB, never MSG_PEEK.
 * 
 * The caller receives the data as a single mbuf chain by supplying
 * an mbuf **mp0 for use in returning the chain.  
 */
/*ARGSUSED*/
int
tpisocket_soreceive(
	register struct tpisocket *tpiso,
	register struct socket *so,
	struct mbuf 	**paddr,
	struct uio 	*uio,
	struct mbuf 	**mp0,
	struct mbuf 	**controlp,
	int 		*flagsp)
{
	register struct mbuf *m;
	register struct mbuf **mp;
	register int 	flags;
	register int 	len;
	register int 	error;
	struct protosw	*pr = so->so_proto;
	struct mbuf	*nextrecord;

	ASSERT(SOCKET_ISLOCKED(so));
	ASSERT(mp0    != NULL);
	ASSERT(paddr  != NULL);
	ASSERT(flagsp != NULL);

	mp = mp0;
	*mp = (struct mbuf *)0;
	*paddr = 0;
	if (controlp)
		*controlp = 0;
	flags = *flagsp & ~MSG_EOR;

#if 0
	if (flags & MSG_OOB) {
		m = m_get(M_DONTWAIT, MT_DATA);
		if (m == NULL)
			return EWOULDBLOCK;
		error = (*pr->pr_usrreq)(so, PRU_RCVOOB, m, 
				(struct mbuf *)0, (struct mbuf *)0);
		if (error)
			m_freem(m);
		else
			*mp = m;
		return error;
	}
#endif
	if (so->so_state & SS_ISCONFIRMING)
		(*pr->pr_usrreq)(so, PRU_RCVD, (struct mbuf *)0,
		    (struct mbuf *)0, (struct mbuf *)0);

	if (error = sblock(&so->so_rcv))
		return error;
	m = so->so_rcv.sb_mb;
	ASSERT(m != NULL);
	nextrecord = m->m_act;
	/*
	 * NOTE: This code assumes that PR_ADDR and PR_ATOMIC are always
	 * identical (both true or both false).
	 */
	if (pr->pr_flags & PR_ADDR) {
		/* return entire next message. */
		ASSERT(m->m_type == MT_SONAME);
		sbfree(&so->so_rcv, m);
		*paddr = m;
		so->so_rcv.sb_mb = m->m_next;
		m->m_next = 0;
		m = so->so_rcv.sb_mb;
		if (m != NULL) {
			ASSERT(m->m_type == MT_DATA || m->m_type == MT_HEADER);
			len = m_length(m);
			so->so_rcv.sb_cc -= len;
			*mp = m;
			m = NULL;
		}
		/*
		 * Hack for UDP; put in checksum
		 */
		if (controlp && (tpiso->tso_flags & TSOF_XTI) && 
		    (so->so_proto->pr_protocol == IPPROTO_UDP)) {
			struct mbuf *m3;
			struct t_opthdr *top;
			m3 = m_get(M_DONTWAIT, MT_DATA);
			if (m3) {
				top = mtod(m3, struct t_opthdr *);
				m3->m_len = sizeof(*top) + sizeof(xtiscalar_t);
				top->len = sizeof(*top) + sizeof(xtiscalar_t);
				top->level = INET_UDP;
				top->name = UDP_CHECKSUM;
				top->status = 0;
				*((xtiscalar_t *)(top + 1)) = 1; /* XXX */
				*controlp = m3;
			}
		}
	} else {
		int oobmark;

		ASSERT(m != NULL);
		/* 
		 * return next non-empty mbuf, (up to oobmark) 
		 */
		mp = &so->so_rcv.sb_mb;
		while (m != NULL && m->m_len == 0) {
			*mp = m->m_next;
			m->m_next = NULL;
			m_free(m);
			m = *mp;
		}
		if (m == NULL)
			goto release;
		len = m->m_len;
		if ((oobmark = so->so_oobmark) != 0) {
			if (len > oobmark) { /* want partial mbuf */
				struct mbuf *mc;

				len = oobmark;
				if (NULL == (mc = m_copy(m, 0, len))) {
					error = EWOULDBLOCK;
					goto release;
				}
				mc->m_next = m;
				*mp = mc;
				m->m_off += len;
				m->m_len -= len;
			}
			if ((oobmark -= len) == 0) {
				(*flagsp) &= ~MSG_OOB;
				flags &= ~MSG_OOB;
			}
			so->so_oobmark = oobmark;
		}
		*mp0 = m = so->so_rcv.sb_mb;
		sbfree(&so->so_rcv, m);
		so->so_rcv.sb_mb = m->m_next;
		m->m_next = 0;
		m = so->so_rcv.sb_mb;
	}

	if (m) {
		so->so_rcv.sb_mb = m;
		m->m_act = nextrecord;
	} else
		so->so_rcv.sb_mb = nextrecord;
	if (pr->pr_flags & PR_WANTRCVD && so->so_pcb)
		(*pr->pr_usrreq)(so, PRU_RCVD, (struct mbuf *)0,
		    (struct mbuf *)0, (struct mbuf *)0);
	*flagsp |= flags;
release:
	sbunlock(&so->so_rcv);
	return error;
}

struct tpi_default {
	int	td_level;
	int	td_name;
	int	td_len;
	int	td_flags;
	void	*td_val;
	int	(*td_get)(struct tpisocket *, struct tpi_default *, 
			struct t_opthdr *, int, void *, int *);
	int	(*td_set)(struct tpisocket *, struct tpi_default *, 
			struct t_opthdr *, int, void *, int *);
};

#define DATA_LEN(top)	(top->len - sizeof(*top))
xtiscalar_t tpi_on = T_YES;
xtiscalar_t tpi_off = T_NO;

int
tpi_generic_get(struct tpisocket *tpiso,
	struct tpi_default *optp, 
	struct t_opthdr *opt,
	int flags, 
	void *val,
	int *lenp)
{
	struct vsocket *vso;
	struct socket *so;
	struct inpcb *inp;
	xtiuscalar_t i;
	extern u_int tcp_sendspace;
	extern u_int tcp_recvspace;
	extern u_int udp_recvgrams;
	extern u_int udp_sendspace;
	xtiuscalar_t *uip = &i;
	struct t_linger *lp;

	*lenp = opt->len;
	if (opt->len) {
		opt_bcopy((char *)(opt + 1), val, opt->len);
	}
	if (DATA_LEN(opt) && (DATA_LEN(opt) != optp->td_len)) {
		return T_FAILURE;
	}
	vso = tpiso->tso_vsocket;
	so = (struct socket *)(BHV_PDATA((VSOCK_TO_FIRST_BHV(vso))));
	SOCKET_LOCK(so);

	inp = (struct inpcb *)so->so_pcb;
	if (!inp) {
		SOCKET_UNLOCK(so);
		return T_FAILURE;
	}
	switch (flags) {
	case T_DEFAULT:
		if (optp->td_val) {
			opt_bcopy(optp->td_val, val, optp->td_len);
			*lenp = optp->td_len;
			break;
		}
		switch (optp->td_name) {
		case XTI_SNDBUF:
			switch (tpiso->tso_control->tsc_protocol) {
			case IPPROTO_TCP:
				i = tcp_sendspace;
				goto cmn;
			case IPPROTO_UDP:
				i = udp_sendspace;
				goto cmn;
			default:
				i = RAWSNDQ;
				goto cmn;
			}
		case XTI_RCVBUF:
			switch (tpiso->tso_control->tsc_protocol) {
			case IPPROTO_TCP:
				i = tcp_recvspace;
				goto cmn;
			case IPPROTO_UDP:
				i = udp_recvgrams * (udp_sendspace +
					sizeof(struct sockaddr_in));
				goto cmn;
			default:
				i = RAWRCVQ;
				goto cmn;
			}
		case XTI_SNDLOWAT:
			i = 0;
			goto cmn;
		case XTI_RCVLOWAT:
			i = 0;
cmn:
			opt_bcopy((char *)uip, val, sizeof(xtiuscalar_t));
			*lenp = sizeof(xtiuscalar_t);
			break;
		}
		break;
	case T_CURRENT:
		switch (optp->td_name) {
		case XTI_RCVLOWAT:
			uip = (xtiuscalar_t *)val;
			*uip = so->so_rcv.sb_lowat;
			*lenp = sizeof(xtiuscalar_t);
			break;
		case XTI_SNDLOWAT:
			uip = (xtiuscalar_t *)val;
			*uip = so->so_snd.sb_lowat;
			*lenp = sizeof(xtiuscalar_t);
			break;
		case XTI_RCVBUF:
			uip = (xtiuscalar_t *)val;
			*uip = so->so_rcv.sb_hiwat;
			*lenp = sizeof(xtiuscalar_t);
			break;
		case XTI_SNDBUF:
			uip = (xtiuscalar_t *)val;
			*uip = so->so_snd.sb_hiwat;
			*lenp = sizeof(xtiuscalar_t);
			break;
		case XTI_LINGER:
			lp = (struct t_linger *)val;
			*lenp = sizeof(*lp);
			lp->l_onoff = (so->so_options & SO_LINGER) ?
				T_YES : T_NO;
			lp->l_linger = so->so_linger;
			break;
		case XTI_DEBUG:
			*((xtiscalar_t *)val) = (so->so_options & SO_DEBUG) ?
				T_YES : T_NO;
			*lenp = sizeof(xtiscalar_t);
			break;
		}
	}

	SOCKET_UNLOCK(so);
	return 0;
}

int
tpi_generic_set(struct tpisocket *tpiso,
	struct tpi_default *optp,
	struct t_opthdr *opt,
	int flags, 
	void *val,
	int *lenp)
{
	struct vsocket *vso;
	struct socket *so;
	struct inpcb *inp;
	xtiuscalar_t *uip;
	struct t_linger *lp;
	int r = T_SUCCESS;

	vso = tpiso->tso_vsocket;
	so = (struct socket *)(BHV_PDATA((VSOCK_TO_FIRST_BHV(vso))));
	SOCKET_LOCK(so);

	inp = (struct inpcb *)so->so_pcb;
	if (!inp) {
		SOCKET_UNLOCK(so);
		return T_FAILURE;
	}
	switch (flags) {
	case T_CHECK:
		switch (optp->td_name) {
		case XTI_SNDBUF:
			if (DATA_LEN(opt) == 0) {
				*lenp = 0;
				goto out;
			}
			if (DATA_LEN(opt) != sizeof(*uip)) {
				r = T_FAILURE;
				goto out;
			}
			opt_bcopy((char *)(opt + 1), val, sizeof(*uip));
			*lenp = sizeof(*uip);
			break;
		case XTI_RCVBUF:
			if (DATA_LEN(opt) == 0) {
				*lenp = 0;
				goto out;
			}
			if (DATA_LEN(opt) != sizeof(*uip)) {
				r = T_FAILURE;
				goto out;
			}
			opt_bcopy((char *)(opt + 1), val, sizeof(*uip));
			*lenp = sizeof(*uip);
			break;
		case XTI_SNDLOWAT:
			if (DATA_LEN(opt) == 0) {
				*lenp = 0;
				goto out;
			}
			if (DATA_LEN(opt) != sizeof(*uip)) {
				r = T_FAILURE;
				goto out;
			}
			opt_bcopy((char *)(opt + 1), val, sizeof(*uip));
			*lenp = sizeof(*uip);
			break;
		case XTI_RCVLOWAT:
			if (DATA_LEN(opt) == 0) {
				*lenp = 0;
				goto out;
			}
			if (DATA_LEN(opt) != sizeof(*uip)) {
				r = T_FAILURE;
				goto out;
			}
			opt_bcopy((char *)(opt + 1), val, sizeof(*uip));
			*lenp = sizeof(*uip);
			break;
		case XTI_LINGER:
			if (DATA_LEN(opt) == 0) {
				*lenp = 0;
				goto out;
			}
			if (DATA_LEN(opt) != sizeof(*lp)) {
				r = T_FAILURE;
				goto out;
			}
			lp = (struct t_linger *)(opt + 1);
			if (lp->l_onoff != T_YES && lp->l_onoff != T_NO) {
				r = T_FAILURE;
				goto out;
			}
			opt_bcopy((char *)lp, val, sizeof(*lp));
			*lenp  = sizeof(*lp);
			break;
		case XTI_DEBUG:
			r = T_READONLY;
			if (DATA_LEN(opt) == 0) {
				*lenp = 0;
				goto out;
			}
			if (DATA_LEN(opt) % sizeof(*uip)) {
				r = T_FAILURE;
				goto out;
			}
			opt_bcopy((char *)(opt + 1), val, DATA_LEN(opt));
			*lenp  = DATA_LEN(opt);
			break;
		}
	case T_NEGOTIATE:
		switch (optp->td_name) {
		case XTI_DEBUG:
			if (DATA_LEN(opt) % sizeof(*uip)) {
				r = T_FAILURE;
				goto out;
			}
			opt_bcopy((char *)(opt + 1), val, DATA_LEN(opt));
			*lenp = DATA_LEN(opt);
			r = T_READONLY;
			break;
		case XTI_LINGER:
			if (DATA_LEN(opt) != sizeof(*lp)) {
				r = T_FAILURE;
				goto out;
			}
			lp = (struct t_linger *)(opt + 1);
			if (lp->l_onoff != T_YES && lp->l_onoff != T_NO) {
				r = T_FAILURE;
				goto out;
			}
			if (lp->l_onoff == T_YES) {
				so->so_options |= SO_LINGER;
				so->so_linger = lp->l_linger;
			} else {
				so->so_options &= ~SO_LINGER;
			}
			opt_bcopy((char *)lp, val, sizeof(*lp));
			*lenp  = sizeof(*lp);
			break;
		case XTI_RCVLOWAT:
			if (DATA_LEN(opt) != sizeof(*uip)) {
				r = T_FAILURE;
				goto out;
			}
			so->so_rcv.sb_lowat = *(xtiuscalar_t *)(opt + 1);
			opt_bcopy((char *)(opt + 1), val, sizeof(*uip));
			*lenp = sizeof(*uip);
			break;
		case XTI_SNDLOWAT:
			if (DATA_LEN(opt) != sizeof(*uip)) {
				r = T_FAILURE;
				goto out;
			}
			so->so_snd.sb_lowat = *(xtiuscalar_t *)(opt + 1);
			opt_bcopy((char *)(opt + 1), val, sizeof(*uip));
			*lenp = sizeof(*uip);
			break;
		case XTI_SNDBUF:
			if (DATA_LEN(opt) != sizeof(*uip)) {
				r = T_FAILURE;
				goto out;
			}
			uip = (xtiuscalar_t *)(opt + 1);
			if (sbreserve(&so->so_snd, 
				(__psunsigned_t) *uip) == 0) {
				r = T_FAILURE;
				goto out;
			}
			opt_bcopy((char *)uip, val, sizeof(*uip));
			*lenp = sizeof(*uip);
			break;
		case XTI_RCVBUF:
			if (DATA_LEN(opt) != sizeof(*uip)) {
				r = T_FAILURE;
				goto out;
			}
			uip = (xtiuscalar_t *)(opt + 1);
			if (sbreserve(&so->so_rcv, 
				(__psunsigned_t) *uip) == 0) {
				r = T_FAILURE;
				goto out;
			}
			opt_bcopy((char *)uip, val, sizeof(*uip));
			*lenp = sizeof(*uip);
			break;
		}
	}
out:
	SOCKET_UNLOCK(so);
	return r;
}

int
tpi_udp_set(struct tpisocket *tpiso,
	struct tpi_default *optp,
	struct t_opthdr *opt,
	int flags, 
	void *val,
	int *lenp)
{
	struct vsocket *vso;
	struct socket *so;
	struct inpcb *inp;
	int r = T_SUCCESS;
	xtiscalar_t *ip;

	vso = tpiso->tso_vsocket;
	so = (struct socket *)(BHV_PDATA((VSOCK_TO_FIRST_BHV(vso))));
	SOCKET_LOCK(so);

	inp = (struct inpcb *)so->so_pcb;
	if (!inp) {
		r = T_FAILURE;
		goto out;
	}
	switch (flags) {
	case T_NEGOTIATE:
	case T_CHECK:
		if (DATA_LEN(opt)) {
			if (DATA_LEN(opt) != sizeof(xtiscalar_t)) {
				r = T_FAILURE;
				goto out;
			}
			ip = (xtiscalar_t *)(opt + 1);
			if ((*ip != T_YES) && (*ip != T_NO)) {
				r = T_FAILURE;
				goto out;
			}
			if (optp->td_val) {
				opt_bcopy(optp->td_val, val, optp->td_len);
				*lenp = optp->td_len;
			}
		} else {
			*lenp = 0;
		}
		r = T_READONLY;
		break;
	}
out:
	SOCKET_UNLOCK(so);
	return r;
}

int
tpi_udp_get(struct tpisocket *tpiso,
	struct tpi_default *optp,
	struct t_opthdr *opt,
	int flags, 
	void *val,
	int *lenp)
{
	struct vsocket *vso;
	struct socket *so;
	struct inpcb *inp;

	*lenp = opt->len;
	if (opt->len) {
		opt_bcopy((char *)(opt + 1), val, opt->len);
	}
	if (DATA_LEN(opt) && (DATA_LEN(opt) != optp->td_len)) {
		return T_FAILURE;
	}
	vso = tpiso->tso_vsocket;
	so = (struct socket *)(BHV_PDATA((VSOCK_TO_FIRST_BHV(vso))));
	SOCKET_LOCK(so);

	inp = (struct inpcb *)so->so_pcb;
	if (!inp) {
		SOCKET_UNLOCK(so);
		return T_FAILURE;
	}
	switch (flags) {
	case T_DEFAULT:
		if (optp->td_val) {
			opt_bcopy(optp->td_val, val, optp->td_len);
			*lenp = optp->td_len;
		}
		break;
	case T_CURRENT:
		switch (optp->td_name) {
		case UDP_CHECKSUM:
			*((xtiscalar_t *)val) = T_YES;
			*lenp = sizeof(xtiscalar_t);
			break;
		}
	}

	SOCKET_UNLOCK(so);
	return 0;
}

int
tpi_tcp_get(struct tpisocket *tpiso,
	struct tpi_default *optp,
	struct t_opthdr *opt,
	int flags, 
	void *val,
	int *lenp)
{
	struct vsocket *vso;
	struct socket *so;
	struct inpcb *inp;
	struct tcpcb *tp;
	struct t_kpalive *kp;

	*lenp = opt->len;
	if (opt->len) {
		opt_bcopy((char *)(opt + 1), val, opt->len);
	}
	if (DATA_LEN(opt) && (DATA_LEN(opt) != optp->td_len)) {
		return T_FAILURE;
	}
	vso = tpiso->tso_vsocket;
	so = (struct socket *)(BHV_PDATA((VSOCK_TO_FIRST_BHV(vso))));
	SOCKET_LOCK(so);

	inp = (struct inpcb *)so->so_pcb;
	if (!inp) {
		SOCKET_UNLOCK(so);
		return T_FAILURE;
	}
	if (!(tp = (struct tcpcb *)inp->inp_ppcb)) {
		SOCKET_UNLOCK(so);
		return T_FAILURE;
	}
	switch (flags) {
	case T_DEFAULT:
		if (optp->td_val) {
			opt_bcopy(optp->td_val, val, optp->td_len);
			*lenp = optp->td_len;
		}
		break;
	case T_CURRENT:
		switch (optp->td_name) {
		case TCP_MAXSEG:
			*((xtiuscalar_t *)val) = (xtiuscalar_t)tp->t_maxseg;
			*lenp = sizeof(xtiuscalar_t);
			break;
		case TCP_KEEPALIVE:
			kp = (struct t_kpalive *)val;
			kp->kp_onoff = (so->so_options & SO_KEEPALIVE)
				? T_YES : T_NO;
			kp->kp_timeout = tcp_keepidle*PR_SLOWHZ;
			*lenp = sizeof(*kp);
			break;
		case TCP_NODELAY:
			*((xtiscalar_t *)val) = (tp->t_flags & TF_NODELAY)
				? T_YES : T_NO;
			*lenp = sizeof(xtiscalar_t);
			break;
		}
	}

	SOCKET_UNLOCK(so);
	return 0;
}

int
tpi_tcp_set(struct tpisocket *tpiso,
	struct tpi_default *optp, 
	struct t_opthdr *opt,
	int flags, 
	void *val,
	int *lenp)
{
	struct vsocket *vso;
	struct socket *so;
	struct inpcb *inp;
	struct tcpcb *tp;
	struct t_kpalive *kp;
	xtiscalar_t *ip;
	int r = T_SUCCESS;

	vso = tpiso->tso_vsocket;
	so = (struct socket *)(BHV_PDATA((VSOCK_TO_FIRST_BHV(vso))));
	SOCKET_LOCK(so);

	inp = (struct inpcb *)so->so_pcb;
	if (!inp) {
		SOCKET_UNLOCK(so);
		return T_FAILURE;
	}
	if (!(tp = (struct tcpcb *)inp->inp_ppcb)) {
		SOCKET_UNLOCK(so);
		return T_FAILURE;
	}
	switch (flags) {
	case T_CHECK:
		switch (optp->td_name) {
		case TCP_MAXSEG:
			if (DATA_LEN(opt) &&
			    DATA_LEN(opt) != sizeof(xtiuscalar_t)) {
				r = T_FAILURE;
				goto out;
			}
			if (DATA_LEN(opt)) {
				opt_bcopy((char *)(opt + 1), val, 
					sizeof(xtiuscalar_t));
				*lenp = sizeof(xtiuscalar_t);
			} else {
				*lenp = 0;
			}
			r = T_READONLY;
			break;
		case TCP_KEEPALIVE:
			if (DATA_LEN(opt) && (DATA_LEN(opt) != sizeof(*kp))) {
				r =  T_FAILURE;
				goto out;
			}
			if (DATA_LEN(opt)) {
				kp = (struct t_kpalive *)(opt + 1);
				if (kp->kp_onoff & ~(T_YES|T_GARBAGE)) {
					r = T_FAILURE;
					goto out;
				}
				if (kp->kp_onoff == T_YES) {
					if (kp->kp_timeout == (tcp_keepidle*PR_SLOWHZ)) {
						r = T_SUCCESS;
					} else {
						r = T_PARTSUCCESS;
					}
				} else if (kp->kp_onoff == T_NO) {
					;
				} else {
					r = T_FAILURE;
					goto out;
				}
				/*
				 * Don't support on per-endpoint basis
				 */
				if ((kp->kp_onoff == T_YES) &&
				    (kp->kp_timeout != (tcp_keepidle*PR_SLOWHZ))) {
					kp->kp_timeout = tcp_keepidle*PR_SLOWHZ;
					opt_bcopy((char *)(opt + 1), val, sizeof(*kp));
					*lenp = sizeof(*kp);
					r = T_PARTSUCCESS;
				} 
			} else {
				*lenp = 0;
			}
			break;
		case TCP_NODELAY:
			if (DATA_LEN(opt) && DATA_LEN(opt) != sizeof(*ip)) {
				r = T_FAILURE;
				goto out;
			}
			if (DATA_LEN(opt)) {
				ip = (xtiscalar_t *)(opt + 1);
				if (*ip == T_YES) {
					;
				} else if (*ip == T_NO) {
					;
				} else {
					r = T_FAILURE;
					goto out;
				}
				opt_bcopy((char *)(opt + 1), val, sizeof(*ip));
				*lenp = sizeof(*ip);
			} else {
				*lenp = 0;
			}
			break;
		}
	case T_NEGOTIATE:
		switch (optp->td_name) {
		case TCP_MAXSEG:
			if (DATA_LEN(opt) != sizeof(xtiuscalar_t)) {
				r = T_FAILURE;
				goto out;
			}
			opt_bcopy((char *)(opt + 1), val, sizeof(xtiuscalar_t));
			*lenp = sizeof(xtiuscalar_t);
			r = T_READONLY;
			break;
		case TCP_KEEPALIVE:
			if (DATA_LEN(opt) != sizeof(*kp)) {
				r =  T_FAILURE;
				goto out;
			}
			kp = (struct t_kpalive *)(opt + 1);
			if (kp->kp_onoff & ~(T_YES|T_GARBAGE)) {
				r = T_FAILURE;
				goto out;
			}
			if (kp->kp_onoff == T_YES) {
				so->so_options |= SO_KEEPALIVE;
				if (kp->kp_timeout == (tcp_keepidle*PR_SLOWHZ)) {
					r = T_SUCCESS;
				} else {
					r = T_PARTSUCCESS;
				}
			} else if (kp->kp_onoff == T_NO) {
				so->so_options &= ~SO_KEEPALIVE;
			} else {
				r = T_FAILURE;
				goto out;
			}
			/*
			 * Don't support on per-endpoint basis
			 */
			if ((kp->kp_onoff == T_YES) &&
			    (kp->kp_timeout != (tcp_keepidle*PR_SLOWHZ))) {
				kp->kp_timeout = tcp_keepidle*PR_SLOWHZ;
				opt_bcopy((char *)(opt + 1), val, sizeof(*kp));
				*lenp = sizeof(*kp);
				r = T_PARTSUCCESS;
			} else {
				opt_bcopy((char *)(opt + 1), val, sizeof(*kp));
				*lenp = sizeof(*kp);
			}
			break;
		case TCP_NODELAY:
			if (DATA_LEN(opt) != sizeof(*ip)) {
				r = T_FAILURE;
				goto out;
			}
			ip = (xtiscalar_t *)(opt + 1);
			if (*ip == T_YES) {
				tp->t_flags |= TF_NODELAY;
			} else if (*ip == T_NO) {
				tp->t_flags &= ~TF_NODELAY;
			} else {
				r = T_FAILURE;
				goto out;
			}
			opt_bcopy((char *)(opt + 1), val, sizeof(*ip));
			*lenp = sizeof(*ip);
			break;
		}
	}
out:
	SOCKET_UNLOCK(so);
	return r;
}

int
tpi_ip_get(struct tpisocket *tpiso,
	struct tpi_default *optp,
	struct t_opthdr *opt,
	int flags, 
	void *val,
	int *lenp)
{
	struct vsocket *vso;
	struct socket *so;
	struct inpcb *inp;
	struct rawcb *rp;
	extern int tcp_ttl;
	extern int udp_ttl;
	int r = 0;

	*lenp = opt->len;
	if (opt->len) {
		opt_bcopy((char *)(opt + 1), val, opt->len);
	}
	if (DATA_LEN(opt) && (DATA_LEN(opt) != optp->td_len)) {
		return T_FAILURE;
	}
	vso = tpiso->tso_vsocket;
	so = (struct socket *)(BHV_PDATA((VSOCK_TO_FIRST_BHV(vso))));
	SOCKET_LOCK(so);

	inp = (struct inpcb *)so->so_pcb;
	rp = (struct rawcb *)so->so_pcb;
	if (!inp) {
		SOCKET_UNLOCK(so);
		return T_FAILURE;
	}
	switch (flags) {
	case T_DEFAULT:
		if (optp->td_val) {
			opt_bcopy(optp->td_val, val, optp->td_len);
			*lenp = optp->td_len;
			break;
		}
		switch (optp->td_name) {
		case IP_TTL:
			switch (tpiso->tso_control->tsc_protocol) {
			case IPPROTO_TCP:
				*(u_char *)val = tcp_ttl;
				*lenp = sizeof(u_char);
				break;
			case IPPROTO_UDP:
				*(u_char *)val = udp_ttl;
				*lenp = sizeof(u_char);
				break;
			default:
				*(u_char *)val = MAXTTL;
				*lenp = sizeof(u_char);
				break;
			}
		case IP_OPTIONS:
			*lenp = 0;
			break;
		}
		break;
	case T_CURRENT:
		switch (optp->td_name) {
		case IP_TTL:
			if (tpiso->tso_control->tsc_socket_type == SOCK_RAW) {
				*(u_char *)val = MAXTTL;
			} else {
				*(u_char *)val = inp->inp_ip_ttl;
			}
			*lenp = sizeof(u_char);
			break;
		case IP_TOS:
			if (tpiso->tso_control->tsc_socket_type == SOCK_RAW) {
				*(u_char *)val = 0;
			} else {
				*(u_char *)val = inp->inp_ip_ttl;
			}
			*lenp = sizeof(u_char);
			break;
		case IP_OPTIONS:
			*lenp = 0;
			if (tpiso->tso_control->tsc_socket_type == SOCK_RAW) {
				if (rp->rcb_options) {
					opt_bcopy(mtod(rp->rcb_options, char *), 
						val, rp->rcb_options->m_len);
					*lenp = rp->rcb_options->m_len;
				} else {
					if (inp->inp_options) {
					   opt_bcopy(mtod(inp->inp_options,
						char *), val, 
					   inp->inp_options->m_len);
					  *lenp = 
					      inp->inp_options->m_len;
					}
				}
			}
			break;
		case IP_DONTROUTE:
			if (so->so_options & SO_DONTROUTE) {
				opt_bcopy((char *)&tpi_on, val, sizeof(tpi_on));
			} else {
				opt_bcopy((char *)&tpi_off, val, sizeof(tpi_on));
			}
			*lenp = sizeof(xtiuscalar_t);
			break;
		case IP_BROADCAST:
			if (so->so_options & SO_BROADCAST) {
				opt_bcopy((char *)&tpi_on, val, sizeof(tpi_on));
			} else {
				opt_bcopy((char *)&tpi_off, val, sizeof(tpi_on));
			}
			*lenp = sizeof(xtiuscalar_t);
			break;
		case IP_REUSEADDR:
			if (so->so_options & SO_REUSEADDR) {
				opt_bcopy((char *)&tpi_on, val, sizeof(tpi_on));
			} else {
				opt_bcopy((char *)&tpi_off, val, sizeof(tpi_on));
			}
			*lenp = sizeof(xtiuscalar_t);
			break;
		}
	}
	SOCKET_UNLOCK(so);
	return r;
}

int
tpi_ip_set(struct tpisocket *tpiso,
	struct tpi_default *optp,
	struct t_opthdr *opt,
	int flags, 
	void *val,
	int *lenp)
{
	struct vsocket *vso;
	struct socket *so;
	int error;
	struct inpcb *inp;
	struct rawcb *rp;
	int r = T_SUCCESS;
	xtiscalar_t *ip;
	struct mbuf *m3;

	vso = tpiso->tso_vsocket;
	so = (struct socket *)(BHV_PDATA((VSOCK_TO_FIRST_BHV(vso))));
	SOCKET_LOCK(so);

	inp = (struct inpcb *)so->so_pcb;
	rp = (struct rawcb *)so->so_pcb;
	if (!inp) {
		r = T_FAILURE;
		goto out;
	}
	switch (flags) {
	case T_CHECK:
		switch (optp->td_name) {
		case IP_BROADCAST:
			if (DATA_LEN(opt) == 0) {
				if (tpiso->tso_control->tsc_protocol == 
					IPPROTO_TCP) {
					r = T_NOTSUPPORT;
					*lenp = 0;
					goto out;
				}
				*lenp = 0;
			} else {
				if (DATA_LEN(opt) != sizeof(xtiscalar_t)) {
					r = T_FAILURE;
					goto out;
				}
				if (tpiso->tso_control->tsc_protocol == 
					IPPROTO_TCP) {
					r = T_NOTSUPPORT;
					goto out;
				}
				ip = (xtiscalar_t *)(opt + 1);
				if ((*ip != T_YES) && (*ip != T_NO)) {
					r = T_FAILURE;
					goto out;
				}
				opt_bcopy((char *)(opt + 1), val, sizeof(*ip));
				*lenp = sizeof(*ip);
			}
			break;
		case IP_DONTROUTE:
			if (DATA_LEN(opt) == 0) {
				*lenp = 0;
			} else {
				if (DATA_LEN(opt) != sizeof(xtiscalar_t)) {
					r = T_FAILURE;
					goto out;
				}
				ip = (xtiscalar_t *)(opt + 1);
				if ((*ip != T_YES) && (*ip != T_NO)) {
					r = T_FAILURE;
					goto out;
				}
				opt_bcopy((char *)(opt + 1), val, sizeof(*ip));
				*lenp = sizeof(*ip);
			}
			break;
		case IP_REUSEADDR:
			if (DATA_LEN(opt) == 0) {
				*lenp = 0;
			} else {
				if (DATA_LEN(opt) != sizeof(xtiscalar_t)) {
					r = T_FAILURE;
					goto out;
				}
				ip = (xtiscalar_t *)(opt + 1);
				if ((*ip != T_YES) && (*ip != T_NO)) {
					r = T_FAILURE;
					goto out;
				}
				opt_bcopy((char *)(opt + 1), val, sizeof(*ip));
				*lenp = sizeof(*ip);
			}
			break;
		case IP_TOS:
		case IP_TTL:
			if (DATA_LEN(opt) == 0) {
				if (tpiso->tso_control->tsc_socket_type == 
					SOCK_RAW) {
					 r = T_NOTSUPPORT;
					*lenp = 0;
					 goto out;
				}
				*lenp = 0;
			} else {
				if (DATA_LEN(opt) != sizeof(u_char)) {
					r = T_FAILURE;
					goto out;
				}
				if (tpiso->tso_control->tsc_socket_type == SOCK_RAW) {
					 r = T_NOTSUPPORT;
					 goto out;
				}
				*(u_char *)val = *(u_char *)(opt + 1);
				*lenp = sizeof(u_char);
			}
			break;
		case IP_OPTIONS:
			if (DATA_LEN(opt) == 0) {
				*lenp = 0;
				goto out;
			}
			*lenp = DATA_LEN(opt);
			opt_bcopy((char *)(opt + 1), val, DATA_LEN(opt));
			break;
		}
		break;
	case T_NEGOTIATE:
		switch (optp->td_name) {
		case IP_BROADCAST:
			if (DATA_LEN(opt) != sizeof(xtiscalar_t)) {
				r = T_FAILURE;
				goto out;
			}
			if (tpiso->tso_control->tsc_protocol == IPPROTO_TCP) {
				r = T_NOTSUPPORT;
				goto out;
			}
			ip = (xtiscalar_t *)(opt + 1);
			if ((*ip != T_YES) && (*ip != T_NO)) {
				r = T_FAILURE;
				goto out;
			}
			opt_bcopy((char *)(opt + 1), val, DATA_LEN(opt));
			*lenp = sizeof(*ip);
			if (*ip == T_YES) {
				so->so_options |= SO_BROADCAST;
			} else {
				so->so_options &= ~SO_BROADCAST;
			}
			break;
		case IP_DONTROUTE:
			if (DATA_LEN(opt) != sizeof(xtiscalar_t)) {
				r = T_FAILURE;
				goto out;
			}
			ip = (xtiscalar_t *)(opt + 1);
			if ((*ip != T_YES) && (*ip != T_NO)) {
				r = T_FAILURE;
				goto out;
			}
			opt_bcopy((char *)(opt + 1), val, DATA_LEN(opt));
			*lenp = sizeof(*ip);
			if (*ip == T_YES) {
				so->so_options |= SO_DONTROUTE;
			} else {
				so->so_options &= ~SO_DONTROUTE;
			}
			break;
		case IP_REUSEADDR:
			if (DATA_LEN(opt) != sizeof(xtiscalar_t)) {
				r = T_FAILURE;
				goto out;
			}
			ip = (xtiscalar_t *)(opt + 1);
			if ((*ip != T_YES) && (*ip != T_NO)) {
				r = T_FAILURE;
				goto out;
			}
			opt_bcopy((char *)(opt + 1), val, DATA_LEN(opt));
			*lenp = sizeof(*ip);
			if (*ip == T_YES) {
				so->so_options |= SO_REUSEADDR;
			} else {
				so->so_options &= ~SO_REUSEADDR;
			}
			break;
		case IP_TOS:
			if (DATA_LEN(opt) != sizeof(u_char)) {
				r = T_FAILURE;
				goto out;
			}
			if (tpiso->tso_control->tsc_socket_type == SOCK_RAW) {
				 r = T_NOTSUPPORT;
				 goto out;
			}
			*(u_char *)val = *(u_char *)(opt + 1);
			*lenp = sizeof(u_char);
			inp->inp_ip_tos = *(u_char *)(opt + 1);
			break;
		case IP_TTL:
			if (DATA_LEN(opt) != sizeof(u_char)) {
				r = T_FAILURE;
				goto out;
			}
			if (tpiso->tso_control->tsc_socket_type == SOCK_RAW) {
				 r = T_NOTSUPPORT;
				 goto out;
			}
			*(u_char *)val = *(u_char *)(opt + 1);
			*lenp = sizeof(u_char);
			inp->inp_ip_ttl = *(u_char *)(opt + 1);
			break;
		case IP_OPTIONS:
			m3 = m_get(M_DONTWAIT, MT_SOOPTS);
			if (m3 == 0) {
				r = T_FAILURE;
				goto out;
			}
			opt_bcopy((char *)(opt + 1), mtod(m3, char *), 
				DATA_LEN(opt));
			m3->m_len = DATA_LEN(opt);
			if (tpiso->tso_control->tsc_socket_type == SOCK_RAW) {
				error = ip_pcbopts(&rp->rcb_options, m3);
			} else {
				error = ip_pcbopts(&inp->inp_options, m3);
			}
			if (error) {
				/* m3 freed in ip_pcbopts() */
				r = T_FAILURE;
				goto out;
			}
			*lenp = DATA_LEN(opt);
			opt_bcopy((char *)(opt + 1), val, DATA_LEN(opt));
			break;
		}
		break;
	}
out:
	SOCKET_UNLOCK(so);
	return r;
}

struct t_kpalive tpi_kpalive = { T_NO, 0 };
struct t_linger tpi_linger = { T_NO, 0 };
u_char	tpi_tos = 0;

extern int tcp_mssdflt;

struct tpi_default tpi_generic_defaults[] = {
	{ XTI_GENERIC, XTI_SNDBUF, sizeof(xtiuscalar_t), 0, 0, tpi_generic_get,
		tpi_generic_set },
	{ XTI_GENERIC, XTI_SNDLOWAT, sizeof(xtiuscalar_t), 0, 0, 
		tpi_generic_get, tpi_generic_set },
	{ XTI_GENERIC, XTI_RCVBUF, sizeof(xtiuscalar_t), 0, 0, tpi_generic_get,
		tpi_generic_set },
	{ XTI_GENERIC, XTI_RCVLOWAT, sizeof(xtiuscalar_t), 0, 0, 
		tpi_generic_get, tpi_generic_set },
	{ XTI_GENERIC, XTI_LINGER, sizeof(tpi_linger), 0, &tpi_linger,
		tpi_generic_get, tpi_generic_set },
	{ XTI_GENERIC, XTI_DEBUG, sizeof(xtiscalar_t), 0, &tpi_off, 
		tpi_generic_get, tpi_generic_set },
	{ -2, 0, 0, 0, 0, 0, 0 }
};

struct tpi_default tpi_ip_defaults[] = {
	{ INET_IP, IP_TOS, sizeof(u_char), 0, &tpi_tos, tpi_ip_get, 
		tpi_ip_set },
	{ INET_IP, IP_TTL, sizeof(u_char), 0, 0, tpi_ip_get, tpi_ip_set },
	{ INET_IP, IP_OPTIONS, 0, 0, 0, tpi_ip_get, tpi_ip_set },
	{ INET_IP, IP_REUSEADDR, sizeof(xtiscalar_t), 0, &tpi_off,
		tpi_ip_get, tpi_ip_set },
	{ INET_IP, IP_BROADCAST, sizeof(xtiscalar_t), 0, &tpi_off, tpi_ip_get,
		tpi_ip_set },
	{ INET_IP, IP_DONTROUTE, sizeof(xtiscalar_t), 0, &tpi_off, tpi_ip_get,
		tpi_ip_set },
	{ -2, 0, 0, 0, 0 , 0, 0}
};

struct tpi_default tpi_udp_defaults[] = {
	{ INET_UDP, UDP_CHECKSUM, sizeof(xtiscalar_t), T_READONLY, &tpi_on,
		tpi_udp_get, tpi_udp_set },
	{ -2, 0, 0, 0, 0, 0, 0 }
};

struct tpi_default tpi_tcp_defaults[] = {
	{ INET_TCP, TCP_NODELAY, sizeof(xtiscalar_t), 0, &tpi_off, tpi_tcp_get,
		tpi_tcp_set },
	{ INET_TCP, TCP_MAXSEG, sizeof(xtiscalar_t), T_READONLY, &tcp_mssdflt,
		tpi_tcp_get, tpi_tcp_set },
	{ INET_TCP, TCP_KEEPALIVE, sizeof(tpi_kpalive), 0, &tpi_kpalive,
		tpi_tcp_get, tpi_tcp_set },
	{ -2, 0, 0, 0, 0, 0, 0 }
};

struct tpi_default *
tpi_find_opt(int level, int name)
{
	struct tpi_default *tp;

#ifdef _TPI_DEBUG
	if (_tpiprintfs > 1) {
		printf("K: find_opt 0x%x 0x%x ", level, name);
	}
#endif	/* _TPI_DEBUG */

	switch (level) {
	case XTI_GENERIC:
		tp = tpi_generic_defaults;
		break;
	case INET_IP:
		tp = tpi_ip_defaults;
		break;
	case INET_UDP:
		tp = tpi_udp_defaults;
		break;
	case INET_TCP:
		tp = tpi_tcp_defaults;
		break;
	default:
#ifdef _TPI_DEBUG
		if (_tpiprintfs > 1) {
			printf(" failed (no level)\n");
		}
#endif	/* _TPI_DEBUG */
		return (struct tpi_default *)0;
	}
	while (tp->td_name && (tp->td_level != -2)) {
		if (tp->td_name == name) {
#ifdef _TPI_DEBUG
			if (_tpiprintfs > 1) {
				printf(" succeeded 0x%x\n", tp);
			}
#endif /* _TPI_DEBUG */
			return tp;
		}
		tp++;
	}
#ifdef _TPI_DEBUG
	if (_tpiprintfs) {
		printf(" failed (no name)\n");
	}
#endif	/* _TPI_DEBUG */
	return (struct tpi_default *)0;
}

int
tpisocket_do_xtioptmgmt(
	struct	tpisocket *tpiso,
	struct	T_optmgmt_req *omr,
	mblk_t	*bp,
	mblk_t	*mp,
	int	*arg)
{
	/*
	 * pointers to message buffer in req 
	 * for processing options
	 */
	struct	tpi_default *optp;
	char	val[OPT_BUFFER_SIZE];
	int	len;
	int	r;
	struct t_opthdr *optreq;  /* beginning of options */
	struct t_opthdr *eoptreq; /* end of options */
	struct t_opthdr *curopt;  /* current option */
	struct T_optmgmt_req *omrnew;
	struct t_opthdr curoutopt;	/* struct: output options */
	struct t_opthdr firstopt;	/* struct: first set of options */

	mp->b_datap->db_type = M_PCPROTO;

	/*
	 * Save pointers to beginning and end of options.
	 * They will be used in validation checks
	 */

	optreq = (struct t_opthdr *)((void *)((char *)omr + omr->OPT_offset));
	eoptreq = (struct t_opthdr *)
	 	 ((void *)((char *)optreq + omr->OPT_length));
	firstopt.level = optreq->level;
	switch (omr->MGMT_flags) {

	case T_CURRENT:
	case T_DEFAULT:
		for (curopt = optreq; curopt;
			curopt = OPT_NEXTHDR(optreq, 
				omr->OPT_length, curopt)) {
#ifdef _TPI_DEBUG
			if (_tpiprintfs) {
				printf("K: opt cur/def tpiso 0x%x 0x%x 0x%x 0x%x 0x%x\n",
					tpiso, omr->MGMT_flags,
					curopt->level, curopt->name,
					curopt->len);
			}
#endif	/* _TPI_DEBUG */
			if (((void *)((char *)curopt + 
				((struct t_opthdr *)curopt)->len)) > 
						(void *)eoptreq) {
				return -TBADOPT;
			}
			if (firstopt.level != curopt->level) {
				return -TBADOPT;
			}
			curoutopt.len = len = curopt->len;
			curoutopt.level = curopt->level;
			curoutopt.name = curopt->name;

			switch(curopt->level) {
			case INET_IP:
				switch(curopt->name) {
				case T_ALLOPT:
					if (DATA_LEN(curopt)) {
						return -TBADOPT;
					}
					optp = tpi_ip_defaults;
					while (optp->td_name && 
					       (optp->td_level != -2)) {
						r = (*optp->td_get)(tpiso, optp,
							curopt, omr->MGMT_flags,
							(void *)val, &len);
						if (r) {
							return -TBADOPT;
						}
						curoutopt.name = optp->td_name;
						curoutopt.status = 
							optp->td_flags ?
							optp->td_flags :
							T_SUCCESS;
						if (!tpisocket_makextiopt(mp,
						  (struct t_opthdr *)&curoutopt,
						  (void *)val, len)) {
							return ENOSR;
						}
						optp++;
					}
					goto out;
				case IP_OPTIONS:
				case IP_TOS:
				case IP_TTL:
				case IP_REUSEADDR:
				case IP_DONTROUTE:
				case IP_BROADCAST:
					optp = tpi_find_opt(curopt->level,
						curopt->name);
					if (!optp) {
						curoutopt.status = T_NOTSUPPORT;
						break;
					}
					r = (*optp->td_get)(tpiso, optp, curopt,
						omr->MGMT_flags, (void *)val, 
							&len);
					if (r) {
						return -TBADOPT;
					}
					break;
				default:
					curoutopt.status = T_NOTSUPPORT;
					break;
				}
				break;
			case INET_TCP:
				switch(curopt->name) {
				case T_ALLOPT:
					if (DATA_LEN(curopt)) {
						return -TBADOPT;
					}
					optp = tpi_tcp_defaults;
					while (optp->td_name && 
					       (optp->td_level != -2)) {
						r = (*optp->td_get)(tpiso, optp,
							curopt, omr->MGMT_flags,
							(void *)val, &len);
						if (r) {
							return -TBADOPT;
						}
						curoutopt.name = optp->td_name;
						curoutopt.status = 
							optp->td_flags ?
							optp->td_flags :
							T_SUCCESS;
						if (!tpisocket_makextiopt(mp,
						  (struct t_opthdr *)&curoutopt,
						  (void *)val, len)) {
							return ENOSR;
						}
						optp++;
					}
					goto out;
				case TCP_NODELAY:
				case TCP_MAXSEG:
				case TCP_KEEPALIVE:
					optp = tpi_find_opt(curopt->level,
						curopt->name);
					if (!optp) {
						curoutopt.status = T_NOTSUPPORT;
						break;
					}
					r = (*optp->td_get)(tpiso, optp, curopt,
							omr->MGMT_flags,
							(void *)val, &len);
					if (r) {
						return -TBADOPT;
					}
					break;
				default:
					curoutopt.status = T_NOTSUPPORT;
					break;
				}
				break;
			case INET_UDP:
				switch(curopt->name) {
				case T_ALLOPT:
					if (DATA_LEN(curopt)) {
						return -TBADOPT;
					}
					optp = tpi_udp_defaults;
					while (optp->td_name && 
					       (optp->td_level != -2)) {
						r = (*optp->td_get)(tpiso, optp,
							curopt,
							omr->MGMT_flags,
							(void *)val, &len);
						if (r) {
							return -TBADOPT;
						}
						curoutopt.name = optp->td_name;
						curoutopt.status = 
							optp->td_flags ?
							optp->td_flags :
							T_SUCCESS;
						if (!tpisocket_makextiopt(mp,
						  (struct t_opthdr *)&curoutopt,
						  (void *)val, len)) {
							return ENOSR;
						}
						optp++;
					}
					goto out;
				case UDP_CHECKSUM:
					optp = tpi_find_opt(curopt->level,
						curopt->name);
					if (!optp) {
						curoutopt.status = T_NOTSUPPORT;
						break;
					}
					r = (*optp->td_get)(tpiso, optp, curopt,
						omr->MGMT_flags, (void *)val,
							&len);
					if (r) {
						return -TBADOPT;
					}
				default:
					curoutopt.status = T_NOTSUPPORT;
					break;
				}
				break;
			case ISO_TP:
				return -TBADOPT;

			case XTI_GENERIC:
				switch(curopt->name) {
				case T_ALLOPT:
					if (DATA_LEN(curopt)) {
						return -TBADOPT;
					}
					optp = tpi_generic_defaults;
					while (optp->td_name && 
					       (optp->td_level != -2)) {
						r = (*optp->td_get)(tpiso, optp,
							curopt, omr->MGMT_flags,
							(void *)val, &len);
						if (r) {
							return -TBADOPT;
						}
						curoutopt.name = optp->td_name;
						curoutopt.status = 
							optp->td_flags ?
							optp->td_flags :
							T_SUCCESS;
						if (!tpisocket_makextiopt(mp,
						  (struct t_opthdr *)&curoutopt,
						  (void *)val, len)) {
							return ENOSR;
						}
						optp++;
					}
					goto out;
					
				case XTI_DEBUG:
				case XTI_LINGER:
				case XTI_RCVBUF:
				case XTI_RCVLOWAT:
				case XTI_SNDBUF:
				case XTI_SNDLOWAT:
					optp = tpi_find_opt(curopt->level,
						curopt->name);
					if (!optp) {
						curoutopt.status = T_NOTSUPPORT;
						break;
					}
					r = (*optp->td_get)(tpiso, optp, curopt,
						omr->MGMT_flags, (void *)val,
							&len);
					if (r) {
						return -TBADOPT;
					}
					break;
				default:
					curoutopt.status = T_NOTSUPPORT;
					break;
				}
				break;
			default:
				return -TBADOPT;
			}
			if (optp) {
				curoutopt.status = 
					optp->td_flags ? optp->td_flags :
				T_SUCCESS;
			}
			if (!tpisocket_makextiopt(mp,
					(struct t_opthdr *)&curoutopt,
						(void *)val, len)) {
				return ENOSR;
			}
		}
		break;

	case T_CHECK:
	case T_NEGOTIATE:
		for (curopt = optreq; curopt;
			curopt = OPT_NEXTHDR(optreq, 
				omr->OPT_length, curopt)) {
#ifdef _TPI_DEBUG
			if (_tpiprintfs) {
				printf("K: opt chk/neg tpiso 0x%x 0x%x 0x%x 0x%x 0x%x\n",
					tpiso, omr->MGMT_flags,
					curopt->level, curopt->name,
					curopt->len);
			}
#endif	/* _TPI_DEBUG */
			if (((void *)((char *)curopt + 
				((struct t_opthdr *)curopt)->len)) > 
						(void *)eoptreq) {
				return -TBADOPT;
			}

			if (firstopt.level != curopt->level) {
				return -TBADOPT;
			}
			curoutopt.len = len = curopt->len;
			curoutopt.level = curopt->level;
			curoutopt.name = curopt->name;
			curoutopt.status = curopt->status;

			switch(curopt->level) {
			case INET_IP:
				switch(curopt->name) {
				case T_ALLOPT:
					if (omr->MGMT_flags == T_CHECK) {
						return -TBADOPT;
					}
					if (DATA_LEN(curopt)) {
						return -TBADOPT;
					}
					optp = tpi_ip_defaults;
					while (optp->td_name && 
					       (optp->td_level != -2)) {
						r = (*optp->td_set)(tpiso, optp,
							curopt, omr->MGMT_flags,
							(void *)val, &len);
						curoutopt.status = r;
						curoutopt.name = optp->td_name;
						if (!tpisocket_makextiopt(mp,
						  (struct t_opthdr *)&curoutopt,
						  (void *)val, len)) {
							return ENOSR;
						}
						optp++;
					}
					goto out;
				case IP_OPTIONS:
				case IP_TOS:
				case IP_TTL:
				case IP_REUSEADDR:
				case IP_DONTROUTE:
				case IP_BROADCAST:
					optp = tpi_find_opt(curopt->level,
						curopt->name);
					if (!optp) {
						curoutopt.status = T_NOTSUPPORT;
						break;
					}
					r = (*optp->td_set)(tpiso, optp, curopt,
						omr->MGMT_flags, (void *)val, 
							&len);
					curoutopt.status = r;
					break;
				default:
					curoutopt.status = T_NOTSUPPORT;
					break;
				}
				break;
			case INET_TCP:
				switch(curopt->name) {
				case T_ALLOPT:
					if (omr->MGMT_flags == T_CHECK) {
						return -TBADOPT;
					}
					if (DATA_LEN(curopt)) {
						return -TBADOPT;
					}
					optp = tpi_tcp_defaults;
					while (optp->td_name && 
					       (optp->td_level != -2)) {
						r = (*optp->td_set)(tpiso, optp,
							curopt, omr->MGMT_flags,
							(void *)val, &len);
						curoutopt.status = r;
						curoutopt.name = optp->td_name;
						if (!tpisocket_makextiopt(mp,
						  (struct t_opthdr *)&curoutopt,
						  (void *)val, len)) {
							return ENOSR;
						}
						optp++;
					}
					goto out;
				case TCP_NODELAY:
				case TCP_MAXSEG:
				case TCP_KEEPALIVE:
					optp = tpi_find_opt(curopt->level,
						curopt->name);
					if (!optp) {
						curoutopt.status = T_NOTSUPPORT;
						break;
					}
					r = (*optp->td_set)(tpiso, optp, curopt,
							omr->MGMT_flags,
							(void *)val, &len);
					curoutopt.status = r;
					break;
				default:
					curoutopt.status = T_NOTSUPPORT;
					break;
				}
				break;
			case INET_UDP:
				switch(curopt->name) {
				case T_ALLOPT:
					if (omr->MGMT_flags == T_CHECK) {
						return -TBADOPT;
					}
					if (DATA_LEN(curopt)) {
						return -TBADOPT;
					}
					optp = tpi_udp_defaults;
					while (optp->td_name && 
					       (optp->td_level != -2)) {
						r = (*optp->td_set)(tpiso, optp,
							curopt,
							omr->MGMT_flags,
							(void *)val, &len);
						curoutopt.status = r;
						curoutopt.name = optp->td_name;
						if (!tpisocket_makextiopt(mp,
						  (struct t_opthdr *)&curoutopt,
						  (void *)val, len)) {
							return ENOSR;
						}
						optp++;
					}
					goto out;
				case UDP_CHECKSUM:
					optp = tpi_find_opt(curopt->level,
						curopt->name);
					if (!optp) {
						curoutopt.status = T_NOTSUPPORT;
						break;
					}
					r = (*optp->td_set)(tpiso, optp, curopt,
						omr->MGMT_flags, (void *)val,
							&len);
					curoutopt.status = r;
				default:
					curoutopt.status = T_NOTSUPPORT;
					break;
				}
				break;
			case ISO_TP:
				return -TBADOPT;

			case XTI_GENERIC:
				switch(curopt->name) {
				case T_ALLOPT:
					if (omr->MGMT_flags == T_CHECK) {
						return -TBADOPT;
					}
					if (DATA_LEN(curopt)) {
						return -TBADOPT;
					}
					optp = tpi_generic_defaults;
					while (optp->td_name && 
					       (optp->td_level != -2)) {
						r = (*optp->td_set)(tpiso, optp,
							curopt, omr->MGMT_flags,
							(void *)val, &len);
						curoutopt.status = r;
						curoutopt.name = optp->td_name;
						if (!tpisocket_makextiopt(mp,
						  (struct t_opthdr *)&curoutopt,
						  (void *)val, len)) {
							return ENOSR;
						}
						optp++;
					}
					goto out;
					
				case XTI_DEBUG:
				case XTI_LINGER:
				case XTI_RCVBUF:
				case XTI_RCVLOWAT:
				case XTI_SNDBUF:
				case XTI_SNDLOWAT:
					optp = tpi_find_opt(curopt->level,
						curopt->name);
					if (!optp) {
						curoutopt.status = T_NOTSUPPORT;
						break;
					}
					r = (*optp->td_set)(tpiso, optp, curopt,
						omr->MGMT_flags, (void *)val,
							&len);
					curoutopt.status = r;
					break;
				default:
					curoutopt.status = T_NOTSUPPORT;
					break;
				}
				break;
			default:
				return -TBADOPT;
			}

			if (!tpisocket_makextiopt(mp,
					(struct t_opthdr *)&curoutopt,
						(void *)val, len)) {
				return ENOSR;
			}
		}
		break;
	default:
		break;
	}
out:
	tpiso->tso_pending_optmgmt_req = NULL;
	bp->b_datap->db_type = M_PCPROTO;
	omr->PRIM_type = T_OPTMGMT_ACK;
	omr->OPT_offset = sizeof(struct T_optmgmt_ack);
	omr->OPT_length = xmsgsize(mp, 1);

	bp->b_wptr = bp->b_rptr + sizeof(struct T_optmgmt_ack);
	if (bp->b_cont)
		freemsg(bp->b_cont);
	bp->b_cont = mp;
	pullupmsg(bp, -1);
	bp->b_band = 1;

	omrnew = (struct  T_optmgmt_req *)bp->b_rptr;
	omrnew->MGMT_flags = tpisocket_xti_MGMT_flags(bp, omrnew);

#ifdef _TPI_DEBUG
	if (_tpiprintfs) {
		printf("K: XTI_T_OPTMGMT_ACK tpiso 0x%x, state %d, l %d, cont 0x%x\n",
			tpiso, tpiso->tso_state, bp->b_wptr - bp->b_rptr,
			bp->b_cont);
		if (_tpiprintfs > 1) {
			tpi_dump_optmgmt_ack(bp);
		}
	}
#endif
	putnext(tpiso->tso_rq, bp);
	if (arg) {
		*((int *)arg) = TSCO_DONE_XTI;
	}
	return 0;
}

#ifdef _TPI_DEBUG
static long tr_buf[2096][4];
static unsigned long tr_cnt;
static int tr_ptr;
lock_t trace_lock;

void
tpisocket_tr_dump()
{
	/* shut up compiler */
	printf("%x\n", tr_buf[0][0]);
}

void
traceme(long x0, void *x1)
{
        int s;
        static firsttime;

        if (firsttime == 0) {
                spinlock_init(&trace_lock, "tracefile");
                firsttime = 1;
        }
        s = mutex_spinlock(&trace_lock);

        tr_buf[tr_ptr][0] = tr_cnt;
        tr_buf[tr_ptr][1] = (long)curthreadp;
        tr_buf[tr_ptr][2] = x0;
        tr_buf[tr_ptr][3] = (long)x1;

        if (tr_ptr >= 2048)
                tr_ptr = 0;
        else
                tr_ptr++;
        tr_cnt++;

        mutex_spinunlock(&trace_lock, s);

}
#endif /* _TPI_DEBUG */

void
tpisocket_flushq(struct tpisocket *tpiso, int flag)
{
	mblk_t *bp;
	
	bp = allocb(2, BPRI_HI);
	if (bp == 0) {
		return;
	}
	bp->b_datap->db_type = M_FLUSH;
	*(bp->b_wptr)++ = FLUSHBAND | flag;
	*(bp->b_wptr)++ = 0;

	(void) putnext(tpiso->tso_rq, bp);
}

void
tpisocket_copyopt(mblk_t **mpp, struct t_opthdr *top)
{
	mblk_t *mp = *mpp;
	mblk_t *bp;

	bp = allocb(top->len, BPRI_HI);
	if (bp == 0) {
		return;
	}
	bcopy((char *)top, bp->b_rptr, top->len);
	bp->b_wptr += top->len;
	*mpp = bp;
	if (mp) {
		mp->b_cont = bp;
	}
}

/*
 * This routine handles options associated with a TCP connection or UDP
 * datagram.  This is pretty straightforward, but completely tedious.
 *
 * The rules are as follows:
 *	- if this is a T_CONN_REQ, negotiate all of the options present,
 *	  and fail if any of them are bogus.  Set the status fields
 *	  appropriately so that the user knows which ones we rejected.
 *	  For every option that was successfully negotiated, copy it into
 *	  an mblk chain so that we can attach it to the T_CONN_CON when it
 *	  is returned.
 *	- if this is a T_CONN_RES, do the same as above, but don't save the
 * 	  message for any reason.
 *	- T_CONN_CONs don't get processed here; the tpitcp_convert_address()
 *	  routine will handle options such as TCP_MAXSEG, and then attach
 *	  whatever we had lying around from the T_CONN_REQ
 *	- The same is true for T_CONN_INDs, except that no saved options will
 *	  be attached
 *	- For "absolute requirements," we return TBADOPT if we cannot give the
 *	  user the requested value, but if we don't know about the option, we
 *	  just ignore it.
 * The primitive is not passed in explicitly; we save opts if there is a place
 * to put them, otherwise we don't.
 *
 * If an error is returned, no saved options are returned; the chain under
 * construction is freed.
 *
 * We also take the mblk as an argument just so that we can validate the length
 * before we start doing anything with it.
 *
 * See section 6 of the XTI specification for the gory details.
 */
int
tpisocket_process_assoc_options(
	struct tpisocket *tpiso,
	struct socket *so,
	mblk_t	*bp,
	u_char	*start,
	int	length,
	mblk_t	**mpp,
	int	*uerror)
{
	struct t_opthdr *top;
	mblk_t *mp = 0;
	mblk_t *rmp = 0;
	int 	first = 1;
	int	len = length;
	int	error = 0;
	xtiscalar_t	*ip;
	xtiuscalar_t	*uip;
	char	*cp;
	struct t_kpalive *kp;
	struct t_linger *l;
	struct inpcb *inp;
	int 	clts = (tpiso->tso_control->tsc_transport_type == T_CLTS);
	int	udp = (clts && 
		(tpiso->tso_control->tsc_protocol == IPPROTO_UDP));

	ASSERT(SOCKET_ISLOCKED(so));
	if (mpp) {
		*mpp = 0;
	}
	if (!(tpiso->tso_flags & TSOF_XTI)) {
		return 0;
	}
#ifdef _TPI_DEBUG
	if (_tpiprintfs) {
		printf("K: tpiso 0x%x assoc opts 0x%x %d, clts=%d, udp=%d\n", 
			tpiso, start, length, clts, udp);
	}
#endif
	if (length > (bp->b_wptr - bp->b_rptr)) {
		return TBADOPT;
	}
	if (length == 0) {
		return 0;
	}
	top = (struct t_opthdr *)start;
	while (top) {
		if ((len < sizeof(*top)) || (top->len < sizeof(*top))) {
			error = TBADOPT;
			goto err;
		}
#ifdef _TPI_DEBUG
		if (_tpiprintfs) {
			printf("K: tpiso 0x%x lev=%d,name=%x,len=%d\n", tpiso,
				top->level, top->name, top->len);
		}
#endif
		switch (top->level) {
		case XTI_GENERIC:
			switch (top->name) {
			case XTI_SNDLOWAT:
				if (DATA_LEN(top) == sizeof(xtiscalar_t)) {
					uip = (xtiuscalar_t *)(top + 1);
					so->so_snd.sb_lowat = *uip;
				} else {
					error = TBADOPT;
					goto err;
				}
				top->status = T_SUCCESS;
				goto good;
			case XTI_RCVLOWAT:
				if (DATA_LEN(top) == sizeof(xtiscalar_t)) {
					uip = (xtiuscalar_t *)(top + 1);
					so->so_rcv.sb_lowat = *uip;
				} else {
					error = TBADOPT;
					goto err;
				}
				top->status = T_SUCCESS;
				goto good;
			case XTI_SNDBUF:
				if (DATA_LEN(top) == sizeof(xtiuscalar_t)) {
					uip = (xtiuscalar_t *)(top + 1);
					if (sbreserve(&so->so_snd,
						(__psunsigned_t) *uip) == 0) {
						error = TBADOPT;
						goto err;
					}
				} else {
					error = TBADOPT;
					goto err;
				}
			case XTI_RCVBUF:
				if (DATA_LEN(top) == sizeof(xtiuscalar_t)) {
					uip = (xtiuscalar_t *)(top + 1);
					if (sbreserve(&so->so_rcv,
						(__psunsigned_t) *uip) == 0) {
						error = TBADOPT;
						goto err;
					}
				} else {
					error = TBADOPT;
					goto err;
				}
				top->status = T_SUCCESS;
				goto good;
			case XTI_LINGER:
				if (DATA_LEN(top) == sizeof(struct t_linger)) {
					l = (struct t_linger *)(top + 1);
					if (l->l_onoff == T_YES) {
						so->so_options |= SO_LINGER;
						so->so_linger = l->l_linger;
					} else if (l->l_onoff == T_NO) {
						so->so_options &= ~SO_LINGER;
					} else {
						error = TBADOPT;
						goto err;
					}
				} else {
					error = TBADOPT;
					goto err;
				}
				top->status = T_SUCCESS;
				goto good;
			case XTI_DEBUG:
				if (DATA_LEN(top) % sizeof(xtiscalar_t)) {
					error = TBADOPT;
					goto err;
				} else if (DATA_LEN(top) == 0) {
					so->so_state &= ~SO_DEBUG;
				} else {
					so->so_state |= SO_DEBUG;
				}
				top->status = T_SUCCESS;
				goto good;
			default:
				top->status = T_NOTSUPPORT;
				len -= top->len;
				top = OPT_NEXTHDR(start, length, top);
				continue;
			}

		case INET_IP:
			switch (top->name) {	
			case IP_TTL:
				if (DATA_LEN(top) != sizeof(u_char)) {
					error = TBADOPT;
					goto err;
				}
				if (tpiso->tso_control->tsc_socket_type == 
					SOCK_RAW) {
					error = TBADOPT;
					goto err;
				}
				cp = (char *)(top + 1);
				((struct inpcb *)so->so_pcb)->inp_ip_ttl = *cp;
				top->status = T_SUCCESS;
				goto good;
			case IP_TOS:
				if (DATA_LEN(top) != sizeof(u_char)) {
					error = TBADOPT;
					goto err;
				}
				if (tpiso->tso_control->tsc_socket_type == 
					SOCK_RAW) {
					error = TBADOPT;
					goto err;
				}
				cp = (char *)(top + 1);
				((struct inpcb *)so->so_pcb)->inp_ip_tos = *cp;
				top->status = T_SUCCESS;
				goto good;
			case IP_OPTIONS:
			       {
				struct mbuf *m3;
				m3 = m_get(M_DONTWAIT, MT_SOOPTS);
				if (m3 == 0) {
					error = TSYSERR;
					*uerror = ENOBUFS;
					goto err;
				}
				bcopy((char *)(top + 1), mtod(m3, char *), 
					DATA_LEN(top));
				m3->m_len = DATA_LEN(top);
				if (so->so_pcb) {
					struct rawcb *rp;
					struct inpcb *inp;
					if (tpiso->tso_control->tsc_socket_type == SOCK_RAW) {
						rp = sotorawcb(so);
						error = ip_pcbopts(
						&rp->rcb_options, m3);
					} else {
						inp = sotoinpcb(so);
						error = ip_pcbopts(
						&inp->inp_options, m3);
					}
					if (error) {
						/* m3 freed in ip_pcbopts() */
						error = TBADOPT;
						goto err;
					}
				}
			       }
			        top->status = T_SUCCESS;
				goto good;
			case IP_BROADCAST:
				/*
				 * Not supported for TCP
				 */
				if (DATA_LEN(top) != sizeof(xtiscalar_t)) {
					error = TBADOPT;
					goto err;
				}
				if (!clts) {
					top->status = T_NOTSUPPORT;
					goto good;
				}
				ip = (xtiscalar_t *)(top + 1);
				if (*ip == T_YES) {
					so->so_options |= SO_BROADCAST;
				} else if (*ip == T_NO) {
					so->so_options &= ~SO_BROADCAST;
				} else {
					error = TBADOPT;
					goto err;
				}
				top->status = T_SUCCESS;
				goto good;
			case IP_REUSEADDR:
				if (DATA_LEN(top) != sizeof(xtiscalar_t)) {
					error = TBADOPT;
					goto err;
				}
				ip = (xtiscalar_t *)(top + 1);
				if (*ip == T_YES) {
					so->so_options |= SO_REUSEADDR;
				} else if (*ip == T_NO) {
					so->so_options &= ~SO_REUSEADDR;
				} else {
					error = TBADOPT;
					goto err;
				}
				top->status = T_SUCCESS;
				goto good;
			case IP_DONTROUTE:
				if (DATA_LEN(top) != sizeof(xtiscalar_t)) {
					error = TBADOPT;
					goto err;
				}
				ip = (xtiscalar_t *)(top + 1);
				if (*ip == T_YES) {
					so->so_options |= SO_DONTROUTE;
				} else if (*ip == T_NO) {
					so->so_options &= ~SO_DONTROUTE;
				} else {
					error = TBADOPT;
					goto err;
				}
				top->status = T_SUCCESS;
				goto good;
			default:
				top->status = T_NOTSUPPORT;
				len -= top->len;
				top = OPT_NEXTHDR(start, length, top);
				continue;
			}
		case INET_UDP:
			if (!udp) {
				top->status = T_NOTSUPPORT;
				goto good;
			}
			switch (top->name) {
			case UDP_CHECKSUM:
				if (DATA_LEN(top) != sizeof(xtiuscalar_t)) {
					error = TBADOPT;
					goto err;
				}
				uip = (xtiuscalar_t *)(top + 1);
				/* XXX */
				top->status = T_NOTSUPPORT;
				goto good;
			default:
				top->status = T_NOTSUPPORT;
				len -= top->len;
				top = OPT_NEXTHDR(start, length, top);
				continue;
			}
		case INET_TCP:
			if (clts) {
				top->status = T_NOTSUPPORT;
				goto good;
			}
			switch (top->name) {	
			case TCP_KEEPALIVE:
				if (DATA_LEN(top) != sizeof(struct t_kpalive)){
					error = TBADOPT;
					goto err;
				}
				kp = (struct t_kpalive *)(top + 1);
				if (kp->kp_onoff & ~(T_YES|T_GARBAGE)) {
					error = TBADOPT;
					goto err;
				}
				if (kp->kp_onoff == T_YES) {
					so->so_options |= SO_KEEPALIVE;
				} else if (kp->kp_onoff == T_NO) {
					so->so_options &= ~SO_KEEPALIVE;
				} else {
					error = TBADOPT;
					goto err;
				}
				/*
				 * Don't support on per-endpoint basis
				 */
				if ((kp->kp_onoff == T_YES) &&
				    (kp->kp_timeout != (tcp_keepidle*PR_SLOWHZ))) {
					kp->kp_timeout = tcp_keepidle*PR_SLOWHZ;
					top->status = T_PARTSUCCESS;
				} else {
					top->status = T_SUCCESS;
				}
				goto good;

			case TCP_MAXSEG:
				error = TACCES;
				goto err;
			case TCP_NODELAY:
				if (DATA_LEN(top) != sizeof(xtiscalar_t)) {
					error = TBADOPT;
					goto err;
				}
				ip = (xtiscalar_t *)(top + 1);
				inp = (struct inpcb *) so->so_pcb;
				if (inp) {
					struct tcpcb *tp;
					tp = (struct tcpcb *) inp->inp_ppcb;
					if (tp) {
						if (*ip == T_YES) {
							tp->t_flags |=
								TF_NODELAY;
						} else if (*ip == T_NO) {
							tp->t_flags &=
								~TF_NODELAY;
						} else {
							error = TBADOPT;
							goto err;
						}
					}
				}
				top->status = T_SUCCESS;
				goto good;
			default:
				top->status = T_NOTSUPPORT;
				len -= top->len;
				top = OPT_NEXTHDR(start, length, top);
				continue;
			}
		default:
			top->status = T_NOTSUPPORT;
			len -= top->len;
			top = OPT_NEXTHDR(start, length, top);
			continue;
		}
good:
		/*
		 * For purposes of T_CONN_CON, we don't need to see options
		 * that were not negotiated.
		 */
		if ((top->status != T_NOTSUPPORT) && mpp) {
			tpisocket_copyopt(&mp, top);
			if (first) {
				first = 0;
				rmp = mp;
			}
#ifdef _TPI_DEBUG
			if (_tpiprintfs > 3) {
				mblk_t *zp = rmp;
				while (zp) {
					hexdump(zp->b_rptr, 
						zp->b_wptr - zp->b_rptr);
					zp = zp->b_cont;
				}
			}
#endif
		}
		len -= top->len;
		top = OPT_NEXTHDR(start, length, top);
	}
	if (mpp) {
		*mpp = rmp;
	}
#ifdef _TPI_DEBUG
	if (_tpiprintfs) {
		printf("K: tpiso 0x%x returned 0x%x for saved options\n",
			tpiso, rmp);
		if (_tpiprintfs > 2) {
			mblk_t *zp = rmp;
			while (zp) {
				hexdump(zp->b_rptr, zp->b_wptr - zp->b_rptr);
				zp = zp->b_cont;
			}
		}
	}
#endif
	return 0;
err:
	if (mp) {
		freemsg(mp);
	}
	return error;
#undef DATA_LEN
}
