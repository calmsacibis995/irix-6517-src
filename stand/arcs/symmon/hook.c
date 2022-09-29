/*
 * hook.c - save/restore exception vectors
 *
 *
 * These support a four deep save/restore stack
 *
 * $Revision: 1.16 $
 *
 */
#include <sys/types.h>
#include <sys/sbd.h>
#include <arcs/spb.h>
#include <fault.h>
#if EVEREST
#include <sys/EVEREST/evmp.h>
#include <sys/EVEREST/gda.h>
#endif
#if SN0
#include <sys/SN/kldir.h>
#include <sys/SN/arch.h>
#include <sys/SN/gda.h>
#endif
#if IP30
#include <sys/RACER/IP30.h>
#include <sys/RACER/gda.h>
#endif
#include <sys/loaddrs.h>
#include "mp.h"
#include <libsc.h>
#include <libsk.h>

#define HOOKED_UTLB (GDA->g_hooked_utlb)
#define HOOKED_NORM (GDA->g_hooked_norm)
#define HOOKED_XTLB (GDA->g_hooked_xtlb)
#define saved private.saved
#define SAVE_UTLB private.save_utlb
#define SAVE_NORM private.save_norm
#define SAVE_XTLB private.save_xtlb

extern int _j_exceptutlb[];
extern int _j_exceptnorm[];
extern int _j_exceptxut[];
extern int _j_exceptecc[];

/*
 * _save_exceptions -- save current exception vectors
 */
void
_save_exceptions(void)
{
	if (saved > 3)
		printf("too many _save_exceptions\n");

	if (HOOKED_UTLB)
		SAVE_UTLB[saved] = *HOOKED_UTLB;

	if (HOOKED_NORM)
		SAVE_NORM[saved] = *HOOKED_NORM;

	if (HOOKED_XTLB)
		SAVE_XTLB[saved] = *HOOKED_XTLB;

	saved++;
}

/*
 * _restore_exceptions -- restore exception vectors from last save
 */
void
_restore_exceptions(void)
{
	if (!saved) {
#ifdef DEBUG
		printf("not enough _save_exceptions\n");
#endif
		return;
	}
	saved--;

	if (HOOKED_UTLB)
		*HOOKED_UTLB = SAVE_UTLB[saved];

	if (HOOKED_NORM)
		*HOOKED_NORM = SAVE_NORM[saved];

	if (HOOKED_XTLB)
		*HOOKED_XTLB = SAVE_XTLB[saved];
}

/*
 * _hook_exceptions -- point exception vector to prom exception handler
 */
void
_hook_exceptions(void)
{
	nofault = 0;
	if (HOOKED_UTLB)
		*HOOKED_UTLB = (inst_t *)_j_exceptutlb;

	if (HOOKED_NORM)
		*HOOKED_NORM = (inst_t *)_j_exceptnorm;

	if (HOOKED_XTLB)
		*HOOKED_XTLB = (inst_t *)_j_exceptxut;
}

#if	IP19 || IP25 || SN0 || IP30

/* 
 * Special Exception handling code in case of NMI on IP19s.
 * We need to support case where system should be able to dump core
 * when it enters debugger via NMI. (Stuck at splhi?? and not taking
 * keyboard intrs). IP19 uses the tlb handlers saved in pda , and 
 * under some weird circumstances it may not be guranteed that pda is 
 * in sane state. In order to make sure NMI gets into debugger, and works
 * fine, we copy down the default exception handler on NMI. So, 
 * _save_nmi_exceptions, _hook_nmi_exceptions() and _restore_nmi_exceptions()
 * are used for this purpose.
 * Only one cpu has to do all these. So master CPU does these at this time.
 */
#if SN0
#define nmi_save_utlb		private.p_nmi_save_utlb
#define nmi_save_norm		private.p_nmi_save_norm
#define nmi_save_xut		private.p_nmi_save_xut
#define nmi_save_ecc		private.p_nmi_save_ecc
#define nmi_save_gevector	private.p_nmi_save_gevector
#define nmi_save_utlbvector	private.p_nmi_save_utlbvector
#define nmi_saved		private.p_nmi_saved
#else
struct vector10	nmi_save_utlb[4];
struct vector10	nmi_save_norm[4];
struct vector10	nmi_save_xut[4];
struct vector10	nmi_save_ecc[4];
uint		nmi_save_gevector[4];
uint		nmi_save_utlbvector[4];
int		nmi_saved = 0;
#endif

void
_save_nmi_exceptions(void)
{
#if EVEREST
	if (EV_GET_LOCAL(EV_SPNUM) != GDA->g_masterspnum)
		return;
#endif
#if SN0
	/*
	 * all cpus should read the exception vectors and save them in
	 * their genpda.
	 */
#endif
#if IP30
	if (cpuid() != GDA->g_masterspnum)
		return;
#endif

	if (nmi_saved > 3) {
		printf("too many _save_nmi_exceptions\n");
		return;
	}

	nmi_save_utlb[nmi_saved] = *(struct vector10 *)UT_VEC;
	nmi_save_norm[nmi_saved] = *(struct vector10 *)E_VEC;
	nmi_save_xut[nmi_saved] = *(struct vector10 *)XUT_VEC;
	nmi_save_ecc[nmi_saved] = *(struct vector10 *)ECC_VEC;

	clear_cache((void*)UT_VEC, E_VEC - UT_VEC + sizeof(struct vector10));

	nmi_save_gevector[nmi_saved] = *(volatile __psint_t *)SPB_GEVECTOR;
	nmi_save_utlbvector[nmi_saved] = *(volatile __psint_t *)SPB_UTLBVECTOR;

	if (HOOKED_UTLB)
		SAVE_UTLB[nmi_saved] = *HOOKED_UTLB;

	if (HOOKED_NORM)
		SAVE_NORM[nmi_saved] = *HOOKED_NORM;

	nmi_saved += 1;
}

/*
 * _restore_nmi_exceptions -- restore NMI exception vectors
 */
void
_restore_nmi_exceptions(void)
{
#if EVEREST
	if (EV_GET_LOCAL(EV_SPNUM) != GDA->g_masterspnum)
		return;
#endif
#if SN0
	/*
	 * all cpus can restore the exception vectors. they're write to
	 * their own calias. the two cpus can write over each other's
	 * vectors, but that should be ok since they both read the same
	 * instructions when they took the nmi (no need for a lock).
	 */
#endif
#if IP30
	if (cpuid() != GDA->g_masterspnum)
		return;
#endif

	if (!nmi_saved) {
#ifdef DEBUG
		printf("not enough _nmi_save_exceptions\n");
#endif
		return;
	}
	nmi_saved -= 1;

	*(struct vector10 *)UT_VEC = nmi_save_utlb[nmi_saved];
	*(struct vector10 *)E_VEC = nmi_save_norm[nmi_saved];
	*(struct vector10 *)XUT_VEC = nmi_save_xut[nmi_saved];
	*(struct vector10 *)ECC_VEC = nmi_save_ecc[nmi_saved];

	clear_cache((void*)UT_VEC, E_VEC - UT_VEC + sizeof(struct vector10));

	*(volatile __psint_t *)SPB_GEVECTOR =
				(__psint_t)nmi_save_gevector[nmi_saved];
	*(volatile __psint_t *)SPB_UTLBVECTOR =
				(__psint_t)nmi_save_utlbvector[nmi_saved];

	clear_cache((void*)SPB_GEVECTOR, sizeof(__psint_t) * 2);

	if (HOOKED_UTLB)
		*HOOKED_UTLB = SAVE_UTLB[nmi_saved];

	if (HOOKED_NORM)
		*HOOKED_NORM = SAVE_NORM[nmi_saved];

}

void
_hook_nmi_exceptions(void)
{

	extern int _j_exceptutlb[];
	extern int _j_exceptnorm[];
	extern int _j_exceptxut[];
	extern int _j_exceptecc[];

#if EVEREST
	if (EV_GET_LOCAL(EV_SPNUM) != GDA->g_masterspnum)
		return;
#endif
#if IP30
	if (cpuid() != GDA->g_masterspnum)
		return;
#endif
#if SN0
	/* no locking needed... cross your fingers... */
#endif

	nofault = 0;

	*(struct vector10 *)UT_VEC = *(struct vector10 *)_j_exceptutlb;
	*(struct vector10 *)XUT_VEC = *(struct vector10 *)_j_exceptxut;
	*(struct vector10 *)ECC_VEC = *(struct vector10 *)_j_exceptecc;
	*(struct vector10 *)E_VEC = *(struct vector10 *)_j_exceptnorm;

	clear_cache((void*)UT_VEC, E_VEC - UT_VEC + sizeof(struct vector10));

	*(volatile __psint_t *)SPB_GEVECTOR = (__psint_t)exceptnorm;
	*(volatile __psint_t *)SPB_UTLBVECTOR = (__psint_t)exceptutlb;

	clear_cache((void*)SPB_GEVECTOR, sizeof(struct vector10) * 2);

}

#endif	/* IP19 || IP25 || SN0 || IP30 */


/*
 * _default_exceptions -- point exception vector to prom exception handler
 */
void
_default_exceptions(void)
{
	nofault = 0;
	*(struct vector10 *)UT_VEC = *(struct vector10 *)_j_exceptutlb;
	*(struct vector10 *)XUT_VEC = *(struct vector10 *)_j_exceptxut;
	*(struct vector10 *)ECC_VEC = *(struct vector10 *)_j_exceptecc;
	*(struct vector10 *)E_VEC = *(struct vector10 *)_j_exceptnorm;

	clear_cache((void*)UT_VEC, E_VEC - UT_VEC + sizeof(struct vector10));

	*(volatile __psint_t *)SPB_GEVECTOR = (__psint_t)exceptnorm;
	*(volatile __psint_t *)SPB_UTLBVECTOR = (__psint_t)exceptutlb;

	clear_cache((void*)SPB_GEVECTOR, sizeof(struct vector10) * 2);

	/*
	 * Client will set these when ready.
	 */
	HOOKED_UTLB = 0;
	HOOKED_NORM = 0;
	HOOKED_XTLB = 0;
}

/*
 * _inst_vec -- if exception vectors have been overwritten, return
 * 		the contents of the previous vector at the given address
 */
/* ARGSUSED */
inst_t
_inst_vec(__psunsigned_t addr)
{
	return (inst_t)-1;
}
