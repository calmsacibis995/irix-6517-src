/*
 * Thread Interposition
 */

#include "spyCommon.h"
#include "spyBase.h"
#include "spyIO.h"
#include "spyCmd.h"
#include "spyThread.h"

#include "ptGrok.h"
#include "ptCmd.h"
#include <pt/ptABI.h>
#include <pt/pt.h>	/* for PT macros */
#undef pt_flags
#include <pt/ptdbg.h>	/* for version info */

#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include <alloca.h>
#include <signal.h>
#include <sys/fault.h>
#include <sys/procfs.h>
#include <sys/syscall.h>
#include <sys/types.h>


static int stc_ProcNew(spyProc_t*);
static int stc_ProcDel(spyProc_t*);
static int stc_ScanOp(spyProc_t*, int, void*, spyScanOp_t*);

static int thdUScan(tInfo_t*, int, void*, spyScanOp_t*);
static int thdKScanLive(tInfo_t*, int, void*, spyScanOp_t*);
static int thdKScanCore(tInfo_t*, int, void*, spyScanOp_t*);
static int thdOp(tInfo_t*, int, void*, spyScanOp_t*);
static int thdKOpLive(tInfo_t*, int, void*, spyScanOp_t*);
static int thdKOpCore(tInfo_t*, int, void*, spyScanOp_t*);
static int thdAScan(tInfo_t*, int, void* opArg, spyScanOp_t*);
static int thdAScanFilter(spyProc_t*, spyThread_t, void*, void*);
static int chkOpDomain(int, uint_t*);

/* Kernel ops are handled differently when the target is not live
 */
#define thdKScan(ti, op, oparg, sso) \
	(SP_LIVE((ti)->ti_proc) ? thdKScanLive(ti, op, oparg, sso) \
				: thdKScanCore(ti, op, oparg, sso))
#define thdKOp(ti, op, oparg, sso) \
	(SP_LIVE((ti)->ti_proc) ? thdKOpLive(ti, op, oparg, sso) \
				: thdKOpCore(ti, op, oparg, sso))

/* Some ops are only valid for one type of context
 */
#define procfsOp(op)	((IOPIOCCMD(op)) > ('q'<<8))

#define SPY_THREAD_VERSION	0

static spyThreadCalls_t	ptVec = {
	SPY_THREAD_VERSION,

	stc_ProcNew,
	stc_ProcDel,
	stc_ScanOp,
};


/* Init library.  Called by client after library is loaded.
 */
int
spyThreadInit(spyThreadCalls_t* stc, spyCmdList_t** scl)
{
	ioSetTrace(getenv("SPY_TRACE"));

	SANITY( stc != 0, ("Bad stc %p\n", stc) );

	*stc = ptVec;			/* fixed interface */
	if (scl) {
		*scl = &ptCmdList;	/* optional dynamic interface */
	}
	return (0);
}


/* Add new process.  Set up details for new process and return handle.
 */
static int
stc_ProcNew(spyProc_t* p)
{
	off64_t	addr;
	int	e;
	char*	ptLibBuf = alloca(sizeOf(p, pt_lib_info_t));
	uint_t	version;

	TEnter( TL0, ("stc_ProcNew(0x%p)\n", p) );

	if (!(p->sp_spy = malloc(sizeof(ptProcInfo_t)))) {
		TReturn (errno);
	}

	/* Locate and read the pthread debug info table.
	 * Verify the version.
	 */
	if ((e = SPC(spc_symbol_addr, p, "__pt_lib_info", &addr, 0, 0))
	    || (e = ioFetchBytes(p, addr, ptLibBuf, sizeOf(p, pt_lib_info_t)))){
		free(p->sp_spy);
		TReturn (e);
	}

	ioXInt(p, addrOf(p, ptLibBuf, pt_lib_info_t, _ptl_info_version),
	       &version);

	if (version != PT_LIBRARY_INFO_VERSION) {
		free(p->sp_spy);
		TReturn (ENOTSUP);
	}

	/* Read the address of the pt table address and dreference it
	 * to get the pt table address.
	 *
	 * The table is allocated dynamically so if the address is 0
	 * we'll retry later when we need it.
	 */
	ioXPointer(p, addrOf(p, ptLibBuf, pt_lib_info_t, _ptl_pt_tbl),
		   &ptProc(p)->pi_ptrToTable);
	if (e = ioFetchPointer(p, ptProc(p)->pi_ptrToTable,
			       &ptProc(p)->pi_table)) {
		free(p->sp_spy);
		TReturn (e);
	}

	/* Read the address of the pt table last-allocated-entry-counter.
	 * This is used to bound scans of the pthread table.
	 */
	ioXPointer(p, addrOf(p, ptLibBuf, pt_lib_info_t, _ptl_pt_count),
		   &ptProc(p)->pi_max);

	/* If this process is not live then we need to build a
	 * map of kt ids to core data.
	 */
	if (!SP_LIVE(p) && (e = coreNew(p))) {
		free(p->sp_spy);
		TReturn (e);
	}

	TTrace( TL1, ("stc_ProcNew: &tbl %#llx &cnt %#llx\n",
			ptProc(p)->pi_table, ptProc(p)->pi_max) );
	TReturn (0);
}


/* Remove existing process info.
 */
static int
stc_ProcDel(spyProc_t* p)
{
	TEnter( TL0, ("stc_ProcDel(0x%p)\n", p) );

	if (!SP_LIVE(p)) {
		coreDel(p);
	}
	free(p->sp_spy);
	TReturn (0);
}


/* Control interface.
 * Implement set of ioctl operations on target process.
 * Each op may have associated in/out data (as in /proc).
 * Domain defines the thing to act on it may be the process, a thread or
 * a set of threads.
 * Finally an optional call back may be provided which is invoked for each
 * thread operated upon.
 */
static int
stc_ScanOp(spyProc_t* p, int op, void* opArg, spyScanOp_t* sso)
{
	tInfo_t		ti;
	int		e;

	TEnter( TL0, ("stc_ScanOp(0x%p, %#x, %#x)\n", p, op, sso->sso_dom) );

	ti.ti_proc = p;
	ti.ti_buf = alloca(sizeOf(p, pt_t));
	ti.ti_dom = sso->sso_dom;

	ti.ti_dom |= SP_LIVE(p) ? STC_LIVE : STC_DEAD;

	/* Fix up the domain as appropriate and then validate it against
	 * the request operation and then call the correct processing.
	 */
	switch (ti.ti_dom & STC_TGT) {

	case STC_PROCESS	:

		ti.ti_dom |= STC_KERN;		/* must be procfs */
		if (e = chkOpDomain(op, &ti.ti_dom)) {
			TReturn (e);
		}
		TTrace( TPR, ("stc_ScanOp: procfs(%d, %d, 0x%p)\n",
				p->sp_procfd, op&0xff, opArg) );
		if (ioctl(p->sp_procfd, op, opArg) != 0) {
			TReturn (errno);
		}
		TReturn ((sso->sso_cb)
			? sso->sso_cb(p, 0, opArg, sso->sso_cbArg)
			: 0);

	case STC_THREAD	:

		/* Set domain according to thread identity.
		 */
		if (e = spytIdentify(sso->sso_thd, &ti)) {
			TReturn (e);
		}
		SANITY( !(ti.ti_dom & (STC_USER|STC_KERN)),
			("Bad domain %#x\n", ti.ti_dom) );

		ti.ti_dom |= (tiHasUctx(&ti) ? STC_USER : 0);
		ti.ti_dom |= (tiHasKctx(&ti) ? STC_KERN : 0);
		if (e = chkOpDomain(op, &ti.ti_dom)) {
			TReturn (e);
		}

		TReturn (thdOp(&ti, op, opArg, sso));

	case STC_SCAN	:

		if (e = chkOpDomain(op, &ti.ti_dom)) {
			TReturn (e);
		}

		switch (ti.ti_dom & STC_CTX) {

		case STC_USER|STC_KERN	:
			TReturn (thdAScan(&ti, op, opArg, sso));

		case STC_USER	:
			TReturn (thdUScan(&ti, op, opArg, sso));

		case STC_KERN	:
			TReturn (thdKScan(&ti, op, opArg, sso));
		}

	default		:
		TReturn (EINVAL);
	}
	/* NOTREACHED */
}


/* User scan.
 * Operate on all threads that have a user context.
 */
static int
thdUScan(tInfo_t* ti, int op, void* opArg, spyScanOp_t* sso)
{
	spyProc_t*	p = ti->ti_proc;
	ushort_t	ptMax;
	int		ptId;
	int		e;

	TEnter( TL0, ("thdUScan(0x%p, %#x)\n", p, op) );

	if (e = ioFetchShort(p, ptProc(p)->pi_max, &ptMax)) {
		TReturn (e);
	}

	for (ptId = 0; ptId < ptMax; ptId++) {

		/* Identify the thread.
		 * We cheat a little here by not using using the generation
		 * part of the pthread id.
		 */
		if (e = spytIdentify(ptId, ti)) {
			if (e != ESRCH) {
				TReturn (e);
			}
			e = 0;
			continue;
		}

		if (e = thdOp(ti, op, opArg, sso)) {
			TReturn (e);
		}
	}
	TReturn (e);
}


/* Kernel scan.
 * Operate on all threads that have a kernel procfs context.
 */
static int
thdKScanLive(tInfo_t* ti, int op, void* opArg, spyScanOp_t* sso)
{
	spyProc_t*	p = ti->ti_proc;
	prthreadctl_t	ptc;
	tid_t		kt;
	int		e;

	TEnter( TL0, ("thdKScanLive(0x%p, %#x) %#x\n", p, op, ti->ti_dom) );
	SANITY( SP_LIVE(p), ("Proc is dead\n") );

	ptc.pt_tid = 0;
	ptc.pt_flags = PTFD_GEQ;
	if (ti->ti_dom & STC_EVENTS) {
		ptc.pt_flags |= PTFS_EVENTS;
	} else if (ti->ti_dom & STC_STOPPED) {
		ptc.pt_flags |= PTFS_STOPPED;
	} else {
		ptc.pt_flags |= PTFS_ALL;
	}
	ptc.pt_cmd = PIOCGUTID;
	ptc.pt_data = (caddr_t)&kt;

	for (;;) {
		TTrace( TPR, ("thdKScanLive: procfs(%d, %#x:UTID, 0x%p)\n",
				p->sp_procfd, ptc.pt_tid, &ptc) );
		if (ioctl(p->sp_procfd, PIOCTHREAD, &ptc) < 0) {
			break;
		}

#ifdef	FIX_LIST_WALK

	/* We'd really like to walk the list using the 'op' but not all
	 * ops tell us who's next so we use PIOCGUTID to drive the loop
	 * and run the real op explicitly.
	 * We need a way to pass back the 'next' token for all ops.
	 */

#endif	/* FIX_LIST_WALK */
		ptc.pt_tid = kt;
		ptc.pt_flags = (ptc.pt_flags & PTF_SET) | PTFD_GTR;

		if (procfsOp(op)) {

			/* Do the op here and avoid the identify path.
			 */
			ti->ti_kt = kt;
			if (e = thdKOp(ti, op, opArg, sso)) {
				TReturn (e);
			}

		} else if ((e = spytIdentify(KtToSpyt(kt), ti))
			   || (e = thdOp(ti, op, opArg, sso))) {
			TReturn (e);
		}
	}
	TReturn ((errno == ENOENT) ? 0 : errno);
}


/* Kernel scan.
 * Operate on all threads that have a kernel core context.
 */
static int
thdKScanCore(tInfo_t* ti, int op, void* opArg, spyScanOp_t* sso)
{
	spyProc_t*	p = ti->ti_proc;
	int		coret;
	tid_t*		kts = ptProc(p)->pi_coreMap;
	tid_t		kt;
	int		e;

	TEnter( TL0, ("thdKScanCore(0x%p, %#x)\n", p, op) );
	SANITY( !SP_LIVE(p), ("Proc is live\n") );

	for (coret = 0; (kt = kts[coret]) != KTNULL; coret++) {

		if (procfsOp(op)) {

			/* Do the op here and avoid the identify path.
			 */
			ti->ti_kt = kt;
			if (e = thdKOp(ti, op, opArg, sso)) {
				TReturn (e);
			}

		} else if ((e = spytIdentify(KtToSpyt(kt), ti))
			   || (e = thdOp(ti, op, opArg, sso))) {
			TReturn (e);
		}
	}
	TReturn (0);
}


/* Single thread operation.
 * Operate on a named thread.
 */
static int
thdOp(tInfo_t* ti, int op, void* opArg, spyScanOp_t* sso)
{
	spyProc_t*	p = ti->ti_proc;
	int		e = 0;

	TEnter( TL0, ("thdOp(0x%p, %#x, %#x, %#x)\n",
			p, op, ti->ti_pt, ti->ti_kt) );
	SANITY( tiHasUctx(ti) || tiHasKctx(ti),
		("Missing ctxt %#x, %#x\n", ti->ti_pt, ti->ti_kt) );

	if (tiHasKctx(ti) && procfsOp(op)) {

		/* Apply to kernel context via procfs.
		 */
		TReturn (thdKOp(ti, op, opArg, sso));
	}

	/* Apply to context.
	 */
	switch (IOPIOCCMD(op)) {
	case SPYCITER	:
		/* Nothing to do - we have identified the thread
		 */
		break;
	case SPYCGNAME	:
		if (tiHasUctx(ti)) {
			*(pthread_t*)opArg = tiPt(ti);
		} else {
			*(spyThread_t*)opArg = tiSpyt(ti);
		}
		break;
	case SPYCGINFO0	:
		if (!tiHasUctx(ti)) {
			TReturn (EINVAL);
		}
		e = ptDescribe(p, PT_ID_TO_PTR(p, ti->ti_pt),
				ti->ti_buf, (char**)&opArg, 0);
		break;
	case PIOCGREG	:
		SANITY( !tiHasKctx(ti), ("Bad kt %#x\n", ti->ti_kt) );

		e = ioXJBGregs(p, IOPIOCFMT(op),
			       addrOf(p, ti->ti_buf, pt_t, _pt_context),
			       opArg);
		break;
	case PIOCGFPREG	:
		SANITY( !tiHasKctx(ti), ("Bad kt %#x\n", ti->ti_kt) );

		e = ioXJBFPregs(p, IOPIOCFMT(op),
				addrOf(p, ti->ti_buf, pt_t, _pt_context),
				opArg);
		break;
	case PIOCSREG	:
		SANITY( !tiHasKctx(ti), ("Bad kt %#x\n", ti->ti_kt) );

		e = ioPlaceGregsJB(p, IOPIOCFMT(op), opArg,
				   addrOf(p, ti->ti_buf, pt_t, _pt_context),
				   PT_ID_TO_PTR(p, ti->ti_pt)
				   + offSet(p, pt_t, _pt_context));
		break;
	case PIOCSFPREG	:
		SANITY( !tiHasKctx(ti), ("Bad kt %#x\n", ti->ti_kt) );

		e = ioPlaceFPregsJB(p, IOPIOCFMT(op), opArg,
				   addrOf(p, ti->ti_buf, pt_t, _pt_context),
				   PT_ID_TO_PTR(p, ti->ti_pt)
				   + offSet(p, pt_t, _pt_context));
		break;
	default	:
		TReturn (EINVAL);
	}

	if (!sso->sso_cb || e) {
		TReturn (e);
	}

	/* Invoke callback.
	 */
	TReturn (sso->sso_cb(p, tiSpyt(ti), opArg, sso->sso_cbArg));
}


/* Single kernel procfs thread operation.
 */
static int
thdKOpLive(tInfo_t* ti, int op, void* opArg, spyScanOp_t* sso)
{
	spyProc_t*	p = ti->ti_proc;
	prthreadctl_t	ptc;

	TEnter( TL0, ("thdKOpLive(0x%p, %#x, %#x)\n", p, op, ti->ti_kt) );
	SANITY( SpytIsKt(tiSpyt(ti)), ("Bad kt id %#x\n", ti->ti_kt) );
	SANITY( procfsOp(op), ("Bad op %#x\n", op) );
	SANITY( SP_LIVE(p), ("Proc is dead\n") );

	/* We arrived here with a valid kt identity and a procfs op.
	 */
	ptc.pt_tid = ti->ti_kt;
	ptc.pt_flags = PTFD_EQL;
	ptc.pt_cmd = op;
	ptc.pt_data = (caddr_t)opArg;

	TTrace( TPR, ("thdKOpLive: procfs(%d, %#x:%d, 0x%p)\n",
			p->sp_procfd, ptc.pt_tid, ptc.pt_cmd&0xff, &ptc) );
	if (ioctl(p->sp_procfd, PIOCTHREAD, &ptc) != 0) {
		TReturn (errno);
	}

	/* Invoke callback.
	 */
	TReturn (sso->sso_cb
		 ? sso->sso_cb(p, tiSpyt(ti), opArg, sso->sso_cbArg)
		 : 0);
}


/* Single kernel core thread operation.
 */
static int
thdKOpCore(tInfo_t* ti, int op, void* opArg, spyScanOp_t* sso)
{
	spyProc_t*	p = ti->ti_proc;
	int		e;

	TEnter( TL0, ("thdKOpCore(0x%p, %#x, %#x)\n", p, op, ti->ti_kt) );
	SANITY( SpytIsKt(tiSpyt(ti)), ("Bad kt id %#x\n", ti->ti_kt) );
	SANITY( procfsOp(op), ("Bad op %#x\n", op) );
	SANITY( !SP_LIVE(p), ("Proc is live\n") );

	switch (IOPIOCCMD(op)) {
	case PIOCGREG	:
		if (e = iotFetchGregs(p, KtToIO(ti), opArg)) {
			TReturn (e);
		}
		break;
	case PIOCGFPREG	:
		if (e = iotFetchFPregs(p, KtToIO(ti), opArg)) {
			TReturn (e);
		}
		break;
	default	:
		TReturn (EINVAL);
	}

	if (!sso->sso_cb || e) {
		TReturn (e);
	}

	/* Invoke callback.
	 */
	TReturn (sso->sso_cb
		 ? sso->sso_cb(p, tiSpyt(ti), opArg, sso->sso_cbArg)
		 : 0);
}


typedef struct scanInfo {
	int		si_op;
	void*		si_opArg;
	spyScanOp_t*	si_sso;
	tInfo_t*	si_ti;
} scanInfo_t;

/* Combined scan.
 * Operate on all threads exactly once (union of kernel and user threads).
 */
static int
thdAScan(tInfo_t* ti, int op, void* opArg, spyScanOp_t* sso)
{
	int		e;
	spyScanOp_t	kscanSSO;
	scanInfo_t	si;
	tid_t		kt;

	TEnter( TL0, ("thdAScan(0x%p, %#x)\n", ti->ti_proc, op) );

	/* First grab all the user context threads.
	 */
	if (e = thdUScan(ti, op, opArg, sso)) {
		TReturn (e);
	}

	/* Now do a kernel scan and skip any that have user contexts
	 * since we've already seen them.
	 * Pass along the thread info to allow the filter to
	 * identify the threadi immediately.
	 */
	si.si_op = op;
	si.si_opArg = opArg;
	si.si_sso = sso;
	si.si_ti = ti;
	STCSSO(&kscanSSO, STC_SCAN_KERN, 0, thdAScanFilter, &si);
	TReturn (thdKScan(ti, SPYCITER, &kt, &kscanSSO));
}


/* ARGSUSED */
static int
thdAScanFilter(spyProc_t* p, spyThread_t t, void* opArg, void* cbArg)
{
	scanInfo_t*	si = (scanInfo_t*)cbArg;
	tInfo_t*	ti = si->si_ti;

	TEnter( TL0, ("thdAScanFilter(0x%p, %#x)\n", p, t) );
	SANITY( tiHasKctx(ti), ("No kctxt %#x, %#x\n", ti->ti_pt, ti->ti_kt) );

	if (tiHasUctx(ti)) {
		TReturn (0);	/* skip threads we've already seen in Uscan */
	}
	TReturn (thdOp(ti, si->si_op, si->si_opArg, si->si_sso));
}


/* Idiot proofing.
 */
static int
chkOpDomain(int op, uint_t* dom)
{
#define CLRF(f)		(*dom) &= ~(f)
#define ADDF(f)		(*dom) |= (f)

	switch (IOPIOCCMD(op)) {
	case PIOCSTOP	:
	case PIOCRUN	:
		CLRF(STC_USER|STC_DEAD);
		break;
	case PIOCSREG	:
	case PIOCSFPREG	:
		CLRF(STC_DEAD);
		/* FALLTHROUGH */
	case PIOCGREG	:
	case PIOCGFPREG	:
	case SPYCITER	:
		CLRF(STC_PROCESS);
		break;
	case PIOCGHOLD	:
	case PIOCSHOLD	:
	case PIOCSSIG	:
	case PIOCUNKILL	:
	case PIOCCFAULT	:
	case PIOCSTATUS	:
		CLRF(STC_PROCESS|STC_USER|STC_DEAD);
		break;
	case SPYCGINFO0	:
	case SPYCGNAME	:
		CLRF(STC_PROCESS);
		break;
	default	:
		return (EINVAL);
	}
	if (!(STC_TGT & *dom) || !(STC_CTX & *dom) || !(STC_LIFE & *dom)) {
		return (EDOM);
	}

	return (0);
}
