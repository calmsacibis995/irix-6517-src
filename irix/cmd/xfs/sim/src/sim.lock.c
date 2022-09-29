#ident "$Revision: 1.32 $"

#define _KERNEL 1
#include <sys/types.h>
#include <sys/sema_private.h>
#include <sys/splock.h>
#include <sys/debug.h>
#undef _KERNEL
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include "sim.h"

/* ARGSUSED */
void
init_sema(sema_t *sp, int val, char *name, long sequence)
{
	sp->s_queue = NULL;
	sp->s_un.s_st.count = val;
	sp->s_un.s_st.flags = 0;
}

/* ARGSUSED */
void
freesema(sema_t *sp)
{
}

/* ARGSUSED */
int
cvsema(sema_t *sp)
{
	return 0;
}

/* ARGSUSED */
int
psema(sema_t *sp, int pri)
{
	if (--sp->s_un.s_st.count < 0) {
		printf("psema failed\n");
		abort();
		/* NOTREACHED */
	}
	return 0;
}

int
vsema(sema_t *sp)
{
	if (++sp->s_un.s_st.count <= 0)
		return 1;
	return 0;
}

int
cpsema(sema_t *sp)
{
	if (sp->s_un.s_st.count <= 0)
		return 0;
	sp->s_un.s_st.count--;
	return 1;
}

short
valusema(sema_t *sp)
{
	return sp->s_un.s_st.count;
}

/* ARGSUSED */
void
mrlock_init(mrlock_t *mrp, int lock_type, char *name, long sequence)
{
	bzero(mrp, sizeof(*mrp));
}

/* ARGSUSED */
void
mrfree(mrlock_t *mrp)
{
}

/* ARGSUSED */
void
mrlock(mrlock_t *mrp, int type, int flags)
{
	if (type == MR_ACCESS)
		mraccess(mrp);
	else
		mrupdate(mrp);
}

/* ARGSUSED */
void
mraccessf(mrlock_t *mrp, int flags)
{
	ASSERT(!(mrp->mr_lbits & MR_ACCMAX));
	mrp->mr_lbits += MR_ACCINC;
	ASSERT(mrp->mr_lbits & MR_ACC);
	ASSERT(!(mrp->mr_lbits & MR_UPD));
}

/* ARGSUSED */
void
mrupdatef(mrlock_t *mrp, int flags)
{
	ASSERT(!(mrp->mr_lbits & (MR_UPD|MR_ACC)));
	mrp->mr_lbits |= MR_UPD;
}

int
mrtryaccess(mrlock_t *mrp)
{
	if (mrp->mr_lbits & (MR_UPD|MR_ACCMAX))
		return 0;
	mraccess(mrp);
	return 1;
}

int
mrtrypromote(mrlock_t *mrp)
{
	ASSERT(mrp->mr_lbits & MR_ACC && !(mrp->mr_lbits & MR_UPD));

	if (mrp->mr_lbits - MR_ACCINC == 0) {
		mrp->mr_lbits -= MR_ACCINC;
		mrp->mr_lbits |= MR_UPD;

		return 1;
	}

	return 0;
}

int
mrtryupdate(mrlock_t *mrp)
{
	if (mrp->mr_lbits & (MR_UPD|MR_ACC))
		return 0;
	mrupdate(mrp);
	return 1;
}

void
mraccunlock(mrlock_t *mrp)
{
	ASSERT(mrp->mr_lbits & MR_ACC);
	ASSERT(!(mrp->mr_lbits & MR_UPD));
	mrp->mr_lbits -= MR_ACCINC;
}

void
mrunlock(mrlock_t *mrp)
{
	ASSERT(mrp->mr_lbits & (MR_UPD|MR_ACC));
	ASSERT(!(mrp->mr_lbits & MR_UPD) || !(mrp->mr_lbits & MR_ACC));
	if (mrp->mr_lbits & MR_ACC)
		mrp->mr_lbits -= MR_ACCINC;
	else
		mrp->mr_lbits &= ~MR_UPD;
}

int
ismrlocked(mrlock_t *mrp, int type)
{
	if (type == MR_ACCESS)
		return (mrp->mr_lbits & MR_ACC);
	else if (type == MR_UPDATE)
		return (mrp->mr_lbits & MR_UPD);
	else if (type == (MR_UPDATE | MR_ACCESS))
		return (mrp->mr_lbits & (MR_UPD | MR_ACC));
	else
		return (mrp->mr_lbits & MR_WAIT);
}

/* ARGSUSED */
void
init_spinlock(lock_t *lp, char *name, long sequence)
{
	*lp = SPIN_INIT;
}

/* ARGSUSED */
void
spinlock_init(lock_t *lp, char *name)
{
	*lp = SPIN_INIT;
}

/* ARGSUSED */
void
spinlock_destroy(lock_t *lp)
{
}

/* ARGSUSED */
void
nested_spinlock(lock_t *lp)
{
}

/* ARGSUSED */
void
nested_spinunlock(lock_t *lp)
{
}

/* ARGSUSED */
void
init_mutex(mutex_t *mp, int type, char *name, long sequence)
{
	ASSERT(type == MUTEX_DEFAULT);
	mp->m_bits = 0;
}

/* ARGSUSED */
void
mutex_destroy(mutex_t *mp)
{
}

/* ARGSUSED */
void
mutex_lock(mutex_t *mp, int pri)
{
	ASSERT(mp->m_bits == 0);
	mp->m_bits = 1;
}

void
mutex_unlock(mutex_t *mp)
{
	ASSERT(mp->m_bits == 1);
	mp->m_bits = 0;
}

/* ARGSUSED */
void
init_sv(sv_t *svp, int type, char *name, long sequence)
{
}

/* ARGSUSED */
int
sv_broadcast(sv_t *svp)
{
	return 0;
}

/* ARGSUSED */
void
sv_wait(sv_t *svp, int flags, void *mp, int s)
{
	ASSERT(0);
}

/* ARGSUSED */
void
sv_destroy(sv_t *svp)
{
}

/* ARGSUSED */
uint
bitlock_set(uint *lp, uint bit, uint val)
{
	uint rval = *lp;
	*lp |= val;
	return rval;
}

/* ARGSUSED */
uint
bitlock_clr(uint *lp, uint bit, uint val)
{
	uint rval = *lp;
	*lp &= ~val;
	return rval;
}

/* ARGSUSED */
void
sv_bitlock_wait(sv_t *svp, int flags, uint *lock, uint bit, int s)
{
	ASSERT(0);
}

int
splhi(void)
{
	return 0;
}

/* ARGSUSED */
void
splx(int x)
{
}

#if !defined(_COMPILER_VERSION) || (_COMPILER_VERSION < 700)
int
atomicAddInt(int *a, int b)
{
	*a += b;
	return *a;
}

long
atomicAddLong(long *a, long b)
{
	*a += b;
	return *a;
}

__int64_t
atomicAddInt64(__int64_t *a, __int64_t b)
{
	*a += b;
	return *a;
}

__uint64_t
atomicAddUint64(__uint64_t *a, __uint64_t b)
{
	*a += b;
	return *a;
}
#endif

int
atomicIncWithWrap(int *a, int b)
{
	int r = *a;

	(*a)++;
	if (*a == b)
		*a = 0;

	return r;
}

int
mrislocked_access(mrlock_t *mrp)
{
        return(mrp->mr_lbits & MR_ACC);
}

int
mrislocked_update(mrlock_t *mrp)
{
        return(mrp->mr_lbits & MR_UPD);
}

/* ARGSUSED */
wchaninfo_t *
wchaninfo(void *wchan, mpkqueuehead_t *winfolist)
{
	return NULL;
}

/* ARGSUSED */
wchaninfo_t *
wchanmeter(void *wchan, mpkqueuehead_t *winfolist)
{
	return NULL;
}

/* ARGSUSED */
char *
makesname(char *name, const char *pref, long ival)
{
	return name;
}
