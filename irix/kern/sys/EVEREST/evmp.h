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

/*
 * evmp.h -- Contains the data structures for communication
 * 	between the Boot master processor and the slave processors,
 *	as well as the structures for communicating between the
 *	kernel and the PROM.
 */

#ifndef __SYS_EVEREST_EVMP_H__
#define __SYS_EVEREST_EVMP_H__

#ident "$Revision: 1.19 $"

/*
 * The MPCONF data structure is used to communicate between
 * the Boot Master processor and the slave processors.  It
 * resides at a fixed place in virtual memory.  When the
 * boot master wants the Slave Processor to initialize its
 * MPCONF block, it sends that processor an interrupt at level
 * 3 (INITMP_LVL).  When the Boot master wants the slave to execute the
 * routine whose address is contained in the launch field,
 * it sends an interrupt at level 4 (LAUNCH_INTLVL).  Initially,
 * all slave processors are in interrupt group #2 (dest = 65). 
 * Because slave processors are initially unable to determine
 * their virtual processor ID, the MPCONF array is indexed 
 * using the physical processor ID (ppid = (slot << 2) + proc).
 *
 * The vpidmap data structure, which lives at VPM_BASEADDR,
 * is used to map from virtual processor ID (which range from 0 to
 * vpidmap->num_procs - 1).  The prom is responsible for initializing
 * this data structure. 
 */

#include <sys/EVEREST/evaddrmacros.h>
 
#define MPCONF_MAGIC	0xBADDEED2
#define MPCONF_SIZE  	64	

#ifdef LANGUAGE_ASSEMBLY
#define MP_MAGICOFF		0
#define MP_EAROM_CKSUM		4
#define MP_STORED_CKSUM		6
#define MP_PHYSID		8
#define MP_VIRTID		9
#define MP_ERRNO		10
#define MP_PROCTYPE		11
#define MP_LAUNCHOFF		12
#define MP_RNDVZOFF		16
#define MP_BEVUTLB		20
#if IP25
#define	MP_ERTOIP		MP_BEVUTLB
#endif
#define MP_BEVNORMAL		24
#define MP_BEVECC		28
#define MP_SCACHESZ		32
#define MP_NONBSS		36
#define MP_STACKADDR		40
#define MP_LPARM		44
#define MP_RPARM		48
#define MP_PRID			52

#  if (_MIPS_SZPTR == 64)
#define MP_REAL_SP		56
#  else
#define FILLER			56
#define MP_REAL_SP		60
#  endif /* _MIPS_SZPTR */

#endif /* LANGUAGE_ASSEMBLY */

#define INITMP_INTLVL		3
#define LAUNCH_INTLVL		4

#define VPM_BASEADDR	(MPCONF_BASEADDR + 64 * MPCONF_BLKSIZE)

#ifdef _LANGUAGE_C
typedef struct mpconf_blk {	
	uint	mpconf_magic;
	ushort	earom_cksum;	/* straight sum of 48 bytes of EAROM. */
	ushort	stored_cksum;	/* stored checksum of EAROM (should match
				 * if nonzero). */
	unchar	phys_id;	/* Physical ID of the processor */ 
	unchar	virt_id;	/* CPU virtual ID */	
	unchar	errno;		/* bootstrap error */
	unchar	proc_type;	/* Processor type (TFP or R4000 or R10000) */
	/* Signed to extend properly.  See evconfig.h */
	volatile int launch;	/* routine to start bootstrap */
	volatile int rendezvous;/* call this, when done launching*/
	int	bevutlb;	/* address of bev utlb exception handler */
#if IP21
#define pod_page_size bevutlb	/* ip21prom uses location for page size */
#elif IP25
#define ertoip bevutlb		/* ip25prom ertoip value on reset */
#endif
	int	bevnormal;	/* address of bev normal exception handler */
	int	bevecc;		/* address of bev cache error handler  */
	int	scache_size;	/* 2nd level cache size for this CPU */
	int	nonbss;		/* God only knows what this is for */
	volatile int stack;	/* Stack pointer */
	volatile int lnch_parm;	/* User can pass single parm to launch */
	volatile int rndv_parm;	/* User can pass single parm to rendezvous */
	int	pr_id;		/* Processor implementation and revision */
#if (_MIPS_SZPTR == 64)
	void *	real_sp;	/* Space for a scaling pointer to the
				 * real stack pointer.
				 */
#elif (_MIPS_SZPTR == 32)
	int 	sign_fill;	/* For 32-bit kernels, read the _bottom_
				 * of the 64-bit address.  Works great
				 * on R4k compatibility addresses.
				 */
	void *	real_sp;	/* Space for a scaling pointer to the
				 * real stack pointer.  Shouldn't really
				 * be used outside of the 64-bit environment.
				 */
#endif /* _MIPS_SZPTR */
} mpconf_t;

#define MPCONF ((mpconf_t *) MPCONF_ADDR)

extern int	cpu_enabled(cpuid_t);

#endif /* _LANGUAGE_C */

#endif /* __SYS_EVEREST_EVMP_H__ */
