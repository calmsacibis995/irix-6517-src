/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1992, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#include "sys/types.h"
#include "sys/systm.h"
#include "sys/vnode.h"
#include "sys/idbg.h"
#if defined(EVEREST) && defined(MULTIKERNEL)
#include "sys/EVEREST/everest.h"
#endif

/* external references */
extern struct dbstbl dbstab[];
extern char nametab[];
extern int symmax;

idbgfunc_t *idbgfuncs = &idbgdefault;
int dbgmon_symstop = 0;
int dbgmon_nmiprom = 0;

void 
idbg_setup(void)
{
    idbgfuncs->setup();
    if (dbgmon_symstop)
#if defined(EVEREST) && defined(MULTIKERNEL)
	/* let the non-golden cells perform a full cpu stop */

    	if (evmk_cellid != evmk_golden_cellid)
		debug(0);
    	else
#endif
		debug("quiet");
}

void
idbg_tablesize(rval_t *r)
{
    idbgfuncs->tablesize(r);
}

int
idbg_copytab(caddr_t *uaddr, int l, rval_t *r)
{
    return idbgfuncs->copytab(uaddr, l, r);
}

int
idbg_dofunc(struct idbgcomm *s, rval_t *r)
{
    return idbgfuncs->dofunc(s, r);
}

int 
idbg_error(void)
{
    return idbgfuncs->error();
}

void
idbg_switch(int s, int o)
{
    idbgfuncs->iswitch(s, o);
}

int
idbg_addfunc(char *n, void (*f)())
{
    return idbgfuncs->addfunc(n, f);
}

void
idbg_delfunc(void (*f)())
{
    idbgfuncs->delfunc(f);
}

void
qprintf(char *f, ...)
{
	va_list ap;
	va_start(ap, f);
	idbgfuncs->qprintf(f, ap);
	va_end(ap);
}

void
_prvn(void *v, int a)
{
    idbgfuncs->prvn(v, a);
}

void
idbg_wd93(int t)
{
    idbgfuncs->wd93(t);
}

void
idbg_addfssw(idbgfssw_t *i)
{
    idbgfuncs->addfssw(i);
}

void
idbg_delfssw(idbgfssw_t *i)
{
    idbgfuncs->delfssw(i);
}

void
printflags(uint64_t flags, char **strings, char *name)
{
    idbgfuncs->printflags(flags, strings, name);
}

char *
padstr(char *buf, int len)
{
    return idbgfuncs->padstr(buf, len);
}

void
pmrlock(struct mrlock_s *m)
{
    idbgfuncs->pmrlock(m);
}

void
prsymoff(void *addr, char *pre, char *post)
{
    idbgfuncs->prsymoff(addr, pre, post);
}

int
ROUNDPC(void *pc, int flag)
{
	int found;
	int hi;
	int mid;
	int lo;

	hi = symmax -1;
	mid = (symmax-1)/2;
	lo = 0;

	while (mid != lo) {
		if ((__psint_t)pc < dbstab[mid].addr) {
			hi = mid;
		} else {
			lo = mid;
		}
		mid = (hi + lo)/2;
		if (flag) {
			qprintf("lo=%d hi=%d mid=%d\n",lo,hi,mid);
		}
	}
	if ((__psint_t)pc < dbstab[hi].addr)
		found = lo;
	else
		found = hi;

	if ( ((__psint_t)pc - dbstab[found].addr) > 0x5000 ) {
		if (flag)
			qprintf(
			"pc=%x addr[lo=%d]=%x addr[hi=%d]=%x addr[mid=%d]=%x\n",
				pc,lo,dbstab[lo].addr,hi,dbstab[hi].addr,
				mid,dbstab[mid].addr);
		return(-1);
	}
	return (found);
}

__psunsigned_t
roundpc(inst_t *pc)
{
	int indx = ROUNDPC(pc,0);
	return (dbstab[indx].addr);
}
