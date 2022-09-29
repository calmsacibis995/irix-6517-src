#ifndef _PTGROK_H
#define _PTGROK_H

#ifdef __cplusplus
extern "C" {
#endif

#include "spyBase.h"
#include <pthread.h>
#include <sys/types.h>

#define PTNULL		SPYTHREADNULL
#define KTNULL		SPYTHREADNULL
#define KtToSpyt(t)	((t) | 0x80000000)	/* set top bit */
#define SpytToKt(t)	((t) & ~0x80000000)	/* clr top bit */
#define SpytIsKt(t)	((t) & 0x80000000)	/* top bit set */
#define KtToIO(ti)	(SP_LIVE((ti)->ti_proc) \
				? ((ti)->ti_kt) : coreKtName((ti)))

typedef struct tInfo {
	spyProc_t*	ti_proc;
	pthread_t	ti_pt;		/* real pthread id */
	tid_t		ti_kt;		/* raw kernel id */
	caddr_t		ti_buf;
	uint_t		ti_dom;	/* current op target */
} tInfo_t;
#define tiHasUctx(ti)	((ti)->ti_pt != PTNULL)
#define tiHasKctx(ti)	((ti)->ti_kt != KTNULL)
#define tiPt(ti)	((ti)->ti_pt)
#define tiSpyt(ti)	(((ti)->ti_kt != KTNULL) \
				? KtToSpyt((ti)->ti_kt) : (ti)->ti_pt)

typedef struct ptProcInfo {
	off64_t		pi_ptrToTable;	/* addr of addr of table */
	off64_t		pi_table;	/* addr of table */
	off64_t		pi_max;		/* addr of used-entry-counter */
	tid_t*		pi_coreMap;	/* map kts to core thread data */
} ptProcInfo_t;

#define ptProc(p)		((ptProcInfo_t *)((p)->sp_spy))

#define offSet(p, s, f)		(s ## Info[(p)->sp_abi]-> f)
#define addrOf(p, b, s, f)	((caddr_t)(b) + offSet(p, s, f))
#define sizeOf(p, s)		(offSet(p, s, s ## _size))


#define PT_ID_TO_PTR(p, ptId)	\
	(ptProc(p)->pi_table + (off64_t)PT_INDEX(ptId) * sizeOf(p, pt_t))

#define PT_PTR_TO_ID(p, ptr)	\
	(pthread_t)(((off64_t)(ptr) - ptProc(p)->pi_table)/sizeOf(p, pt_t))

extern int	spytIdentify(spyThread_t, tInfo_t*);

extern int	vpDescribe(spyProc_t*, off64_t, caddr_t, char**);
extern int	ptDescribe(spyProc_t*, off64_t, caddr_t, char**, int);

extern int	coreNew(spyProc_t*);
extern void	coreDel(spyProc_t*);
extern tid_t	coreKtName(tInfo_t*);

#ifdef __cplusplus
}
#endif

#endif /* _PTGROK_H */
