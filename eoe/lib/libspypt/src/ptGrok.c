/*
 * Pthread Internals
 */

#include "spyCommon.h"
#include "spyBase.h"
#include "spyIO.h"
#include "spyThread.h"

#include "ptGrok.h"

#include <alloca.h>
#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>

#include <pt/sys.h>
#include <pt/pt.h>
#include <pt/ptABI.h>
#include <pt/ptABID.h>

#undef malloc
#undef free

static char	*vpStateNames[] = {
	"Unknown",
	"IDLE",
	"EXEC"
};

static char	*ptStateNames[] = {
	"DEAD",
	"DISPATCH",
	"READY",
	"RUNNING",
	"MUTEX-WAIT",
	"JOIN",
	"COND-WAIT",
	"COND-TWAIT",
	"RWL-RWAIT",
	"RWL-WWAIT",
	"SEM-WAIT"
};

#define STATENAME(i, t) (i < sizeof(t) / sizeof(char*)) ? (t[i]) : "Unknown"


/* Read data for a vp.
 */
static int
vpFetch(spyProc_t* p, off64_t vpAddr, caddr_t vpBuf)
{
	TEnter( TL2, ("vpFetch(0x%p, %#llx, 0x%p)\n", p, vpAddr, vpBuf) );
	TReturn (ioFetchBytes(p, vpAddr, vpBuf, sizeOf(p, vp_t)));
}


/* Read data for a pt.
 */
static int
ptFetch(spyProc_t* p, off64_t ptAddr, caddr_t ptBuf)
{
	TEnter( TL2, ("ptFetch(0x%p, %#llx, 0x%p)\n", p, ptAddr, ptBuf) );
	TReturn (ioFetchBytes(p, ptAddr, ptBuf, sizeOf(p,pt_t)));
}


static int
prdaExtract(tInfo_t* ti, off64_t* prdaPT, off64_t* prdaVP)
{
	spyProc_t*	p = ti->ti_proc;
	char*		prdaBuf = alloca(sizeOf(p, prda_lib_t));
	int		e;

	TEnter( TL2, ("prdaExtract(%#x)\n", ti->ti_kt) );

	if (e = iotFetchBytes(p, KtToIO(ti), (off64_t)PRDALIB,
			      prdaBuf, sizeOf(p, prda_lib_t))) {
		TReturn (e);
	}
	/* VP */
	ioXPointer(p, addrOf(p, prdaBuf, prda_lib_t, _pthread_data), prdaVP);
	/* PT */
	ioXPointer(p, addrOf(p, prdaBuf + IOPTRSIZE(p),
			     prda_lib_t, _pthread_data), prdaPT);
	TReturn (0);
}


/* Decrypt and validate a pt, vp pair.
 * Determine if this is an active, scheduled pair.
 *
 * vpAddr will be null if we have started from a user id
 * otherwise we have values from the PRDA for both vp and pt
 */
static int
tSchedCheck(off64_t ptAddr, off64_t vpAddr, tInfo_t* ti, int* paired)
{
	off64_t		pt_vpAddr;	/* pt's ptr to vp */
	off64_t		vp_ptAddr;	/* vp's ptr to pt */
	uint_t		ptLabel;
	tid_t		ktId;
	off64_t		prdaVP;
	off64_t		prdaPT;
	spyProc_t*	p = ti->ti_proc;
	char*		vpBuf = alloca(sizeOf(p, vp_t));
	int		e;

	TEnter( TL2, ("tSchedCheck(%#llx, %#llx)\n", ptAddr, vpAddr) );
	SANITY( ptAddr || vpAddr, ("PT & VP == 0\n") );

	/* Grab the pt_t details.
	 */
	if (e = ptFetch(p, ptAddr, ti->ti_buf)) {
		TReturn (e);
	}

	/* Fix up the pthread_t which is formed from the table index
	 * and the generation number in the pt_label field.
	 */
	ioXInt(p, addrOf(p, ti->ti_buf, pt_t, _pt_label), &ptLabel);
	if (!PT_REF_BITS(ptLabel)) {
		TTrace( TL2, ("no refs on pt\n") );
		if (!vpAddr) {
			TReturn (ESRCH);
		}
		*paired = FALSE;
		TReturn (0);
	}
	ti->ti_pt = PT_MAKE_ID(PT_GEN_BITS(ptLabel), PT_PTR_TO_ID(p, ptAddr));

	/* Find out which vp the pt thinks it is running on and check it's
	 * the vp we have (if we know).
	 */
	ioXPointer(p, addrOf(p, ti->ti_buf, pt_t, _pt_vp), &pt_vpAddr);
	if (pt_vpAddr == 0 || vpAddr && vpAddr != pt_vpAddr) {
		TTrace( TL2, ("no pair: pt_vp=%#llx, vp=%#llx\n",
				pt_vpAddr, vpAddr) );
		*paired = FALSE;
		TReturn (0);
	}

	/* Grab the vp_t details.
	 */
	if (e = vpFetch(p, pt_vpAddr, vpBuf)) {
		TReturn (e);
	}

	/* Find out which pt the vp is running (if any) and check it's
	 * the pt we have.
	 */
	ioXPointer(p, addrOf(p, vpBuf, vp_t, _vp_pt), &vp_ptAddr);
	if (vp_ptAddr != ptAddr) {
		TTrace( TL2, ("no pair: vp_pt=%#llx != pt=%#llx\n",
				vp_ptAddr, ptAddr) );
		*paired = FALSE;
		TReturn (0);
	}

	/* Check the vp is actually live; when it starts it changes its
	 * own id from -1.
	 */
	ioXInt(p, addrOf(p, vpBuf, vp_t, _vp_pid), (uint_t*)&ktId);
	if (ktId == -1) {
		TTrace( TL2, ("no pair: vp_pid=%d\n", ktId) );
		*paired = FALSE;
		TReturn (0);
	}

	/* If we started without a vpAddr verify the PRDA.
	 */
	ti->ti_kt = ktId;
	if (vpAddr == 0) {
		if (e = prdaExtract(ti, &prdaPT, &prdaVP)) {
			TReturn (e);
		}
		if (pt_vpAddr != prdaVP || ptAddr != prdaPT) {
			TTrace( TL2, ("no pair: pt_vp=%#llx != VP=%#llx"
				      " || pt=%#llx != PT=%#llx\n",
				      pt_vpAddr, prdaVP, ptAddr, prdaPT) );
			*paired = FALSE;
			TReturn (0);
		}
	}

	/* Scheduled pair: pt <==> vp
	 */
	*paired = TRUE;
	TReturn (0);
}


/* Decode a spy thread handle (may be a pt or kt id).
 * On return, the kt and pt ids will be filled in and if the pthread
 * id is valid the pt buffer will be primed.
 */
int
spytIdentify(spyThread_t t, tInfo_t* ti)
{
	off64_t		ptAddr;
	off64_t		vpAddr;
	spyProc_t*	p = ti->ti_proc;
	int		pair;
	int		e;

	TEnter( TL2, ("spytIdentify(%#x)\n", t) );

	/* Ensure pthread table is really available.
	 * If we attached early enough the table would not have been
	 * created and consequently the address would be zero.
	 */
	if (!ptProc(p)->pi_table) {
		if (e = ioFetchPointer(p, ptProc(p)->pi_ptrToTable,
				       &ptProc(p)->pi_table)) {
			TReturn (e);
		}
		if (!ptProc(p)->pi_table) {
			TReturn (EAGAIN);
		}
	}

	/* We're dealing with three interlinked data structures:
	 *	PRDA	VP and PT pointers
	 *	vp_t	pointer to pt_t and vp_pid
	 *	pt_t	pointer to vp_t
	 *
	 * Grab the addresses for the pt_t and vp_t structures to see
	 * if the pthread is currently running on a uthread.
	 */
	if (SpytIsKt(t)) {

		ti->ti_kt = SpytToKt(t);

		if (e = prdaExtract(ti, &ptAddr, &vpAddr)) {
			TReturn (e);
		}

		/* Quick check; no PT, no pair.
		 * Early on VP may also be zero.
		 */
		if (!ptAddr || !vpAddr) {
			TTrace( TL2, ("PT %#llx, VP %#llx\n", ptAddr, vpAddr) );
			ti->ti_pt = PTNULL;
			TReturn (0);
		}

		/* Pass in pt and vp addresses read from the PRDA.
		 */
		if (e = tSchedCheck(ptAddr, vpAddr, ti, &pair)) {
			TReturn (e);
		}

		/* We may have started with a copy of the parent PRDA.
		 * We detect that by double checking the kt id that has
		 * been extracted from the vp.
		 */
		if (ti->ti_kt != SpytToKt(t)) {
			TTrace( TL2, ("COW PRDA\n") );
			ti->ti_pt = PTNULL;
			ti->ti_kt = SpytToKt(t);
			TReturn (0);
		}
		if (!pair) {
			ti->ti_pt = PTNULL;
		}
	} else {

		ptAddr = PT_ID_TO_PTR(p, t);

		/* Pass in the pt address computed from the pthread_t
		 * and 0 for the vp address.
		 */
		if (e = tSchedCheck(ptAddr, 0, ti, &pair)) {
			TReturn (e);
		}
		if (!pair) {
			ti->ti_kt = KTNULL;
		}
		SANITY( ti->ti_pt != PTNULL, ("pt id empty\n") );
	}

	TTrace( TL2, ("identity pt %#x kt %#x\n", ti->ti_pt, ti->ti_kt) );
	TReturn (0);
}


int
vpDescribe(spyProc_t* p, off64_t vp, caddr_t vpBuf, char** s)
{
	uint_t		state;
	off64_t		pt;
	tid_t		tid;
	int		e;

	TEnter( TL0, ("vpDescribe(0x%p, %#llx)\n", p, vp) );
	if (!(*s = malloc(256))) {
		TReturn (errno);
	}
	if (e = vpFetch(p, vp, vpBuf)) {
		TReturn (e);
	}

	ioXInt(p, addrOf(p, vpBuf, vp_t, _vp_state), &state);
	ioXPointer(p, addrOf(p, vpBuf, vp_t, _vp_pt), &pt);
	ioXInt(p, addrOf(p, vpBuf, vp_t, _vp_pid), (uint_t*)&tid);

        sprintf(*s, "%#llx state %s pt %#llx tid %d",
		vp, STATENAME(state, vpStateNames), pt, tid);

	TTrace( TL0, ("%s\n", *s) );
	TReturn (0);
}


int
ptDescribe(spyProc_t* p, off64_t pt, caddr_t ptBuf, char** s, int verbosity)
{
	uint_t		state;
	uint_t		flags;
	uint_t		blocked;
	uint_t		occupied;
	off64_t		vp;
	off64_t		stk;
	off64_t		sync;
	int		l;
	int		e;

	TEnter( TL0, ("ptDescribe(0x%p, %#llx, %d)\n", p, pt, verbosity) );

	if (!*s && !(*s = malloc(256))) {
		TReturn (errno);
	}
	if (e = ptFetch(p, pt, ptBuf)) {
		TReturn (e);
	}

	ioXInt(p, addrOf(p, ptBuf, pt_t, _pt_state), &state);

	l = sprintf(*s, "%10s", STATENAME(state, ptStateNames));

	TTrace( TL0, ("%s\n", *s) );

	if (verbosity) {
		ioXPointer(p, addrOf(p, ptBuf, pt_t, _pt_vp), &vp);
		ioXPointer(p, addrOf(p, ptBuf, pt_t, _pt_stk), &stk);
		ioXPointer(p, addrOf(p, ptBuf, pt_t, _pt_sync), &sync);
		l += sprintf(*s + l,
			" vp %#0llx stk %#0llx sync %#0llx\n",
			vp, stk, sync);

		ioXInt(p, addrOf(p, ptBuf, pt_t, _pt_occupied), &occupied);
		ioXInt(p, addrOf(p, ptBuf, pt_t, _pt_blocked), &blocked);
		ioXInt(p, addrOf(p, ptBuf, pt_t, _pt_bits), &flags);
		l += sprintf(*s + l,
			"\t<%s%s>"
			"<%s%s%s>"
			"<Sigs %s>"
			"<Cncl %s%s%s%s>",

			occupied		? "Occ"		: "Nocc",
			blocked			? "Blk"		: "Nblk",

			(flags & PT_SYSTEM)	? "Scs"		: "Pcs",
			(flags & PT_GLUED)	? "Wire,"	: "Mxn",
			(flags & PT_DETACHED)	? "Det"		: "Ndet",

			(flags & PT_SIGNALLED)	? "!"		: "",

			(flags & PT_CNCLENABLED) ? "Ena"	: "Dis",
			(flags & PT_CNCLDEFERRED) ? "Defer"	: "Async",
			(flags & PT_CNCLPENDING) ? "Pend"	: "",
			(flags & PT_CANCELLED)	? "!"		: ""
		);
	}

	TReturn (0);
}


int
coreNew(spyProc_t* p)
{
	int	coret;
	tid_t	kt;
	int	mapSize = 0;
	tid_t*	map = 0;
	char*	prdaBuf = alloca(sizeOf(p, prda_sys_t));
	int	e;

	TEnter( TL0, ("coreNew(0x%p)\n", p) );
	SANITY( !SP_LIVE(p), ("Proc is live\n") );

	for (coret = 0;
	     !(e = iotFetchBytes(p, coret, (off64_t)(&PRDA->sys_prda),
				 prdaBuf, sizeOf(p, prda_sys_t))); coret++) {

		ioXInt(p, addrOf(p, prdaBuf, prda_sys_t, _t_rpid),(uint_t*)&kt);
		TTrace( TL1, ("coreNew: map[%d] = %d\n", coret, kt) );
		if (coret == mapSize) {
			mapSize += 10;
			if (!(map = realloc(map, (mapSize+1)*sizeof(tid_t)))) {
				TReturn (errno);
			}
		}
		map[coret] = kt;
	}
	if (e != EINVAL) {
		TReturn (e);
	}
	if (!coret) {
		TReturn (EINVAL);
	}
	map[coret] = KTNULL;
	ptProc(p)->pi_coreMap = map;
	TReturn (0);
}


void
coreDel(spyProc_t* p)
{
	free(ptProc(p)->pi_coreMap);
}


tid_t
coreKtName(tInfo_t* ti)
{
	int	coret = 0;
	tid_t*	map = ptProc(ti->ti_proc)->pi_coreMap;

	do {
		if (map[coret] == ti->ti_kt) {
			TTrace( TL2, ("coreKtName(%#x) %#x\n",
					ti->ti_kt, coret) );
			return (coret);
		}
	} while (map[++coret] != KTNULL);
	SANITY( 0, ("Failed to find tid %#x\n", ti->ti_kt) );

	return (0xdeaded);
}
