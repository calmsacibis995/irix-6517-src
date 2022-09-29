/**************************************************************************
 *                                                                        *
 *                 Copyright (C) 1996, Silicon Graphics, Inc              *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ident  "$Revision: 1.16 $"

#include <sys/types.h>
#include <sys/conf.h>
#include <sys/cred.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <ksys/ddmap.h>
#include <sys/immu.h>
#include <ksys/exception.h>
#include <sys/kusharena.h>
#include <sys/pfdat.h>
#include <sys/proc.h>
#include <sys/sysmacros.h>
#include <sys/cmn_err.h>
#include <sys/systm.h>
#include <sys/kmem.h>
#include <sys/idbgentry.h>

/*
 * sharena - kernel<->application shared memory. Used for nanothreads
 *
 * It really needs to be a K0SEG address, and have the same color
 * as address in the process to which it is mapped. This is required
 * in order to be able to save the base registers in this area from
 * locore.
 *
 * There are 2 parts to this - getting a kernel page mapped into the processes
 *	space, and recording the kernel address so that it can be used
 *	to make scheduling decisions.
 * For sprocs, one must map in sharena FIRST before any sprocs -
 * the sharena ptr is then copied into the shaddr struct on first sproc.
 * For uthreads, the proc struct holds the sharena ptr.
 */

int sharenadevflag = D_MP;

void
printrsaregs(rsa_t *rsa, void (*f)(char *, ...))
{

#define PFMT3	"%s0x%llx 0x%llx 0x%llx\n"
#define PFMT4	"%s0x%llx 0x%llx 0x%llx 0x%llx\n"

#if _MIPS_SIM == _ABI64 || _MIPS_SIM == _ABIN32
	f(PFMT3, "at/v0/v1:\t", rsa->rsa_context.__gregs[CTX_AT],
	  rsa->rsa_context.__gregs[CTX_V0], rsa->rsa_context.__gregs[CTX_V1]);
	f(PFMT4, "a0-a3:\t\t", rsa->rsa_context.__gregs[CTX_A0],
	  rsa->rsa_context.__gregs[CTX_A1], rsa->rsa_context.__gregs[CTX_A2],
	  rsa->rsa_context.__gregs[CTX_A3]);
	f(PFMT4, "a4-a7 (t0-t4):\t",
	  rsa->rsa_context.__gregs[CTX_A4], rsa->rsa_context.__gregs[CTX_A5],
	  rsa->rsa_context.__gregs[CTX_A6], rsa->rsa_context.__gregs[CTX_A7]);
	f(PFMT4, "t0-t3 (t4-t7):\t",
	  rsa->rsa_context.__gregs[CTX_T0], rsa->rsa_context.__gregs[CTX_T1],
	  rsa->rsa_context.__gregs[CTX_T2], rsa->rsa_context.__gregs[CTX_T3]);
	f(PFMT4, "s0-s3:\t\t", rsa->rsa_context.__gregs[CTX_S0],
	  rsa->rsa_context.__gregs[CTX_S1], rsa->rsa_context.__gregs[CTX_S2],
	  rsa->rsa_context.__gregs[CTX_S3]);
	f(PFMT4, "s4-s7:\t\t", rsa->rsa_context.__gregs[CTX_S4],
	  rsa->rsa_context.__gregs[CTX_S5], rsa->rsa_context.__gregs[CTX_S6],
	  rsa->rsa_context.__gregs[CTX_S7]);
	f(PFMT4, "t8/t9/k0/k1:\t", rsa->rsa_context.__gregs[CTX_T8],
	  rsa->rsa_context.__gregs[CTX_T9], rsa->rsa_context.__gregs[CTX_K0],
	  rsa->rsa_context.__gregs[CTX_K1]);
	f(PFMT4, "gp/sp/s8/ra:\t", rsa->rsa_context.__gregs[CTX_GP],
	  rsa->rsa_context.__gregs[CTX_SP], rsa->rsa_context.__gregs[CTX_S8],
	  rsa->rsa_context.__gregs[CTX_RA]);
	f(PFMT3, "mdlo/mdhi/EPC:\t", rsa->rsa_context.__gregs[CTX_MDLO],
	  rsa->rsa_context.__gregs[CTX_MDHI],rsa->rsa_context.__gregs[CTX_EPC]);
#else

	f(PFMT3, "at/v0/v1:\t", rsa->rsa_context.__gregs[CTX_AT],
	  rsa->rsa_context.__gregs[CTX_V0], rsa->rsa_context.__gregs[CTX_V1]);
	f(PFMT4, "a0-a3:\t\t", rsa->rsa_context.__gregs[CTX_A0],
	  rsa->rsa_context.__gregs[CTX_A1], rsa->rsa_context.__gregs[CTX_A2],
	  rsa->rsa_context.__gregs[CTX_A3]);
	f(PFMT4, "t0-t3:\t\t", rsa->rsa_context.__gregs[CTX_T0],
	  rsa->rsa_context.__gregs[CTX_T1], rsa->rsa_context.__gregs[CTX_T2],
	  rsa->rsa_context.__gregs[CTX_T3]);
	f(PFMT4, "t4-t7:\t\t", rsa->rsa_context.__gregs[CTX_T4],
	  rsa->rsa_context.__gregs[CTX_T5], rsa->rsa_context.__gregs[CTX_T6],
	  rsa->rsa_context.__gregs[CTX_T7]);
	f(PFMT4, "s0-s3:\t\t", rsa->rsa_context.__gregs[CTX_S0],
	  rsa->rsa_context.__gregs[CTX_S1], rsa->rsa_context.__gregs[CTX_S2],
	  rsa->rsa_context.__gregs[CTX_S3]);
	f(PFMT4, "s4-s7:\t\t", rsa->rsa_context.__gregs[CTX_S4],
	  rsa->rsa_context.__gregs[CTX_S5], rsa->rsa_context.__gregs[CTX_S6],
	  rsa->rsa_context.__gregs[CTX_S7]);
	f(PFMT4, "t8/t9/k0/k1:\t", rsa->rsa_context.__gregs[CTX_T8],
	  rsa->rsa_context.__gregs[CTX_T9], rsa->rsa_context.__gregs[CTX_K0],
	  rsa->rsa_context.__gregs[CTX_K1]);
	f(PFMT4, "gp/sp/fp/ra:\t", rsa->rsa_context.__gregs[CTX_GP],
	  rsa->rsa_context.__gregs[CTX_SP], rsa->rsa_context.__gregs[CTX_S8],
	  rsa->rsa_context.__gregs[CTX_RA]);
	f(PFMT3, "mdlo/mdhi/EPC:\t", rsa->rsa_context.__gregs[CTX_MDLO],
	  rsa->rsa_context.__gregs[CTX_MDHI],rsa->rsa_context.__gregs[CTX_EPC]);
#endif

#undef PFMT3
#undef PFMT4
}

void
idbg_dumprsa(int rsaid)
{
	kusharena_t *kusp;

	if (rsaid < 0) {
  		qprintf("dumprsa requires a valid rsa number (0 to %d)\n",
			NT_MAXRSAS);
		return;
	}
	if (!curuthread || (kusp = curuthread->ut_sharena) == 0) {
		qprintf("invalid curuthread (0x%x) or sharena (0x%x)\n",
			curuthread, (curuthread ? curuthread->ut_sharena : 0));
		return;
	}
	qprintf("rsa %d (0x%x)\n", rsaid, &kusp->rsa[rsaid] );

	printrsaregs( &kusp->rsa[rsaid].rsa, qprintf );
  
}

void
idbg_dumpnid(int nid)
{
	kusharena_t *kusp;
	int rsaid;

	if (nid <= 0) {
  		qprintf("dumprsa requires a valid rsa number (0 to %d)\n",
			NT_MAXRSAS);
		return;
	}
	if (!curuthread || (kusp = curuthread->ut_sharena) == 0) {
		qprintf("invalid curuthread (0x%x) or sharena (0x%x)\n",
			curuthread, (curuthread ? curuthread->ut_sharena : 0));
		return;
	}
	rsaid = kusp->nid_to_rsaid[nid];

	qprintf("nid %d rsaid %d rbit %d\n", nid, rsaid,
		(kusp->rbits[nid/bitsizeof(uint64_t)] &
		 (1LL <<(nid % bitsizeof(uint64_t))) ? 1 : 0));

	idbg_dumprsa( rsaid );

  
}

int
sharenareg(void)
{
	dev_t sharena_dev;
	(void)hwgraph_char_device_add(GRAPH_VERTEX_NONE, "sharena", "sharena", &sharena_dev);
	hwgraph_chmod(sharena_dev, 0666);
	idbg_addfunc("dumprsa", (void (*)())idbg_dumprsa);
	idbg_addfunc("dumpnid", (void (*)())idbg_dumpnid);

	return(0);
}

/* ARGSUSED */
int
sharenaopen(dev_t *devp, mode_t oflag, int otyp, cred_t *crp)
{
	return 0;
}

/* ARGSUSED */
int
sharenaclose(dev_t dev, int oflag, int otyp, cred_t *crp)
{
	ASSERT(curuthread->ut_sharena == NULL);
	return 0;
}

/*ARGSUSED*/
int
sharenamap(dev_t dev, vhandl_t *vt, off_t off, size_t len, uint_t prot)
{
	uvaddr_t vaddr = v_getaddr(vt);
	pgcnt_t npgs = btoc(len);
	int status, basesize, i, locore_ok = 1;
	caddr_t kvaddr;
	size_t alignment;

	if ((sizeof(kusharena_t) - NT_MAXRSAS*sizeof(padded_rsa_t)) &0x7f) {
		cmn_err(CE_WARN,"kusharena structure needs re-alignment!\n");
		cmn_err(CE_CONT,"RSA array not aligned on cacheline: 0x%x\n",
			sizeof(kusharena_t) - NT_MAXRSAS*sizeof(padded_rsa_t));
	}
	if (curuthread->ut_sharena)
		return EBUSY;
	if (IS_SPROC(curuthread->ut_pproxy))	/* must map BEFORE sproc */
		return EINVAL;

	/* Support multiple pages */

#ifdef SHARENA_DEBUG

	cmn_err(CE_CONT, "sharenamap: request %d pages, max allowed is %d\n",
		npgs, btoc(sizeof(kusharena_t)));
#endif
	if (npgs > btoc(sizeof(kusharena_t)))
		return EINVAL;
	
	/* We need the pages to be physically contiguous and virtually
	 * colored IFF the user library want "pre-allocated" RSAs where
	 * the LOCORE intrnorm/backtouser code directly saves/restores
	 * the user registers into the RSA.  The "allocate on demand" case
	 * does NOT need these restrictions since we will copy the registers
	 * in the kernel where tlbmisses/ VCE exceptions can be resolved.
	 */

	/* For now, we guarentee physical coloring by forcing the user vaddr
	 * to have color 0, and then allocate a "large page" whose alignment
	 * is the same as the cachecolor.
	 */
	alignment = (cachecolormask+1)*NBPP;
	if (colorof(vaddr)) {
		cmn_err(CE_WARN, "sharenamap: uthread vaddr (0x%x) must be color 0, has 0x%x\n",
			vaddr, colorof(vaddr));
		return EINVAL;
	}
	kvaddr = (caddr_t)kmem_contig_alloc( npgs*NBPP, alignment, 0);

	if (kvaddr == NULL) {
		cmn_err(CE_WARN,"sharenamap: kvaddr failed vaddr 0x%x (%d)\n",
			vaddr, npgs);
		return(EAGAIN);
	}
	if (!IS_KSEG0(kvaddr)) {
		cmn_err(CE_WARN,"sharenamap: vaddr 0x%x (%d) NOT contiguous\n",
			kvaddr, npgs);
		cmn_err(CE_WARN,"sharenamap: no LOCORE rsa save or restore!\n");
		locore_ok = 0;
	}
	if (colorof(vaddr) != colorof(kvaddr)) {
		cmn_err(CE_WARN,
			"sharenamap: vaddr 0x%x(%d) kvaddr %x BAD pcolor (%d:%d)\n",
			vaddr, npgs, kvaddr, colorof(vaddr), colorof(kvaddr));

		cmn_err(CE_WARN,"sharenamap: no LOCORE rsa save or restore!\n");
		locore_ok = 0;
	}

	status = v_mapphys(vt, kvaddr, len);
	if (status) {
		cmn_err(CE_WARN, "sharenamap: v_mapphys status: %d\n", status);
		return(EINVAL);
	}
	/* initialize data structures */
	
	bzero(kvaddr, npgs * NBPP);

	basesize = sizeof(kusharena_t) - NT_MAXRSAS * sizeof(padded_rsa_t);

	/* this value is actually the first value we CAN'T use */

	curuthread->ut_maxrsaid = ((len - basesize) / sizeof(padded_rsa_t));

#ifdef SHARENA_DEBUG
	cmn_err(CE_CONT,
		"sharenamap: vaddr 0x%x kvaddr 0x%x len 0x%x npgs %d\n",
		vaddr, kvaddr, len, npgs);
	cmn_err(CE_CONT, "sharenamap: basesize 0x%x maxrsaid %d\n",
		basesize, curuthread->ut_maxrsaid);
#endif

	/* set K0 address into proc struct */
	curuthread->ut_sharena = (kusharena_t *)kvaddr;
	curuthread->ut_rsa_npgs = npgs;
	curuthread->ut_rsa_locore = locore_ok;

	/* RSA 0 is "special" (used when a uthread has lost its' NID due to
	 * another uthread stealing the NID via a nanothread library
	 * resume_nid() call.
	 */

	for (i=1; i < curuthread->ut_maxrsaid; i++)
		((kusharena_t *)kvaddr)->fbits[i/bitsizeof(uint64_t)] |=
			(1LL<<(i % bitsizeof(uint64_t)));
	return 0;
}

/*ARGSUSED*/
int
sharenaunmap(dev_t dev, vhandl_t *vt)
{
	kmem_contig_free( curuthread->ut_sharena,
		curuthread->ut_rsa_npgs*NBPP);
	curuthread->ut_sharena = NULL;
	curuthread->ut_maxrsaid = 0;
	return 0;
}
