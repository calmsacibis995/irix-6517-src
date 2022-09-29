/**************************************************************************
 *									  *
 * 		 Copyright (C) 1992, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/


#ifndef __SYS_EVEREST_INTR_H__
#define __SYS_EVEREST_INTR_H__

#define EVINTR_LEVEL_SHFT	8
#define EVINTR_DEST_MASK	0x7f

#define EVINTR_VECTOR(level,dest) \
	(((level) << EVINTR_LEVEL_SHFT)|(dest))

#define EVINTR_LEVEL(vector)		((vector) >> EVINTR_LEVEL_SHFT)
#define EVINTR_DEST(vector)		((vector) & EVINTR_DEST_MASK)

/*
 * Destination descriptor for PROCESSOR-directed interrupt.
 */
#if IP19 || IP25
#define EVINTR_CPUDEST(procid) \
	((cpuid_to_slot[(procid)]<<2) | (cpuid_to_cpu[(procid)]))
#endif
#if IP21
#define EVINTR_CPUDEST(procid) \
	((cpuid_to_slot[(procid)]<<2) | (cpuid_to_cpu[(procid)] << 1))
#endif

/*
 * Destination descriptor for GROUP-directed interrupt.
 */
#define EVINTR_GROUPDEST(groupid)	((1<<6) | (groupid))


/*
 * Destination descriptor for GLOBAL-directed interrupt.
 */
#define EVINTR_GLOBALDEST	0x7f

#define EVINTR_ANY		-2
#define EVINTR_NULL		-1

#define SPLDEV		3		/* low priority I/O devices */
					/* (use spl3) */

#define SPLHIDEV	5		/* high priority I/O devices */
					/* (use spl5) */

#define SPLTLB		6		/* TLB flush (random only) */
#define SPLPROF		7		/* profiling timer level */
					/* (use spl7) */

#define NSPL		8		/* Number of spl levels */
#define SPLMAX		(NSPL-1)	/* Maximum spl level */

#define SPLERROR	SPLMAX

#define EVINTR_MAX_LEVELS	0x80	/* num levels for CEL reg */
#define EVINTR_LEVEL_BASE	0x1

/* Lower priority interrupts. Drivers for these devices block with spl3 */

/* Interrupt levels to be used between VMECC, FCG and Graphics-DANG. 
 * Max of (VMECC + FCG) in a system is 6. Allocate 8 intr levels for each 
 * of these adaps. This would consume 48 ebus intr levels. 
 *
 * VMECC would start allocating interrupt levels from lower end, and FCG
 * would start allocating from higher end. 
 *
 * CAUTION :
 * This would never overlap as long as maximum VMECC+FCG is <= 6
 */
#define	EVINTR_LEVEL_VMECC_START	0x10
#define	EVINTR_LEVEL_FCG_START		0x38
#define	INTR_LEVEL_PER_ADAP		0x8
#define	EVINTR_LEVEL_DANG_START		0x38

#define	FCG_INTR_LEVEL(adap)	(EVINTR_LEVEL_FCG_START - \
					(adap*INTR_LEVEL_PER_ADAP))

#define	DANG_INTR_LEVEL(adap)	(EVINTR_LEVEL_DANG_START - \
					(adap*INTR_LEVEL_PER_ADAP))

/*
 * VME Interrupts levels are allocated based on 
 * IRQ level,  Adapter No, Max VMECC adapters.
 *
 * Algorithm: level for IRQ=i, ADAP=a, MAXADAP=m = (Base + (m*i) + a)
 *
 * This provides a way to keep Ebus interrupt levels corresponding to a 
 * specific IRQ to be consecutive numbers. Provides a way to support splvmex
 * when Nested hierachical VME interrupts are supported
 */
#define	VMECC_OTHER_INDX		0
#define	VMECC_IRQ1_INDX			1
#define	VMECC_IRQ2_INDX			2
#define	VMECC_IRQ3_INDX			3
#define	VMECC_IRQ4_INDX			4
#define	VMECC_IRQ5_INDX			5
#define	VMECC_IRQ6_INDX			6
#define	VMECC_IRQ7_INDX			7

#define	VME_INTR_LEVEL(a,m,i)		(EVINTR_LEVEL_VMECC_START + (m*i) + a)

#define EVINTR_LEVEL_VMECC_OTHERS(a,m)	VME_INTR_LEVEL(a,m,VMECC_OTHER_INDX)
#define EVINTR_LEVEL_VMECC_IRQ1(a,m)	VME_INTR_LEVEL(a,m,VMECC_IRQ1_INDX)
#define EVINTR_LEVEL_VMECC_IRQ2(a,m)	VME_INTR_LEVEL(a,m,VMECC_IRQ2_INDX)
#define EVINTR_LEVEL_VMECC_IRQ3(a,m)	VME_INTR_LEVEL(a,m,VMECC_IRQ3_INDX)
#define EVINTR_LEVEL_VMECC_IRQ4(a,m)	VME_INTR_LEVEL(a,m,VMECC_IRQ4_INDX)
#define EVINTR_LEVEL_VMECC_IRQ5(a,m)	VME_INTR_LEVEL(a,m,VMECC_IRQ5_INDX)
#define EVINTR_LEVEL_VMECC_IRQ6(a,m)	VME_INTR_LEVEL(a,m,VMECC_IRQ6_INDX)
#define EVINTR_LEVEL_VMECC_IRQ7(a,m)	VME_INTR_LEVEL(a,m,VMECC_IRQ7_INDX)

#define EVINTR_LEVEL_VMEMAX		((nvmecc == 0)? \
					 EVINTR_LEVEL_VMECC_IRQ7(0,1):\
					 EVINTR_LEVEL_VMECC_IRQ7(nvmecc-1,nvmecc))



#define	EVINTR_LEVEL_VMECC_ERROR	0x40

/* EPC Starts using from 0x41.. */
#define EVINTR_LEVEL_EPC_ENET		0x41

/* Interrupt Levels 0x46 - 0x4a Now used for DANG adapters. */
#define	EVINTR_LEVEL_DANG_GIO1		0x46

/* HIPPI uses 0x4b - 0x4e */
#define EVINTR_LEVEL_HIPPI(adap)	(0x4b+(adap))

#define	EVINTR_LEVEL_S1_CHIP(adap)	(0x4f+(adap))

#define EVINTR_LEVEL_EPC_PPORT		0x5f
#define EVINTR_LEVEL_EPC_PPORT1		0x60
#define EVINTR_LEVEL_EPC_PPORT2		0x61
#define EVINTR_LEVEL_EPC_PPORT3		0x62

#define EVINTR_LEVEL_EPC_INTR4		0x63

/* Dang will use 7 interrupt levels from 0x64 to 0x6a for GIO bus devices */
#define	EVINTR_LEVEL_DANG_GIO		0x64

#define EVINTR_LEVEL_MAXLODEV		0x6b

/* Higher priority interrupts.  Drivers for these devices block with spl5  */
#define EVINTR_LEVEL_EPC_KBDMS		0x6c
#define EVINTR_LEVEL_EPC_DUART0		EVINTR_LEVEL_EPC_KBDMS
#define EVINTR_LEVEL_EPC_DUART1		0x6d
#define EVINTR_LEVEL_EPC_DUARTS2	0x6e
#define EVINTR_LEVEL_EPC_DUARTS3	0x6f
#define EVINTR_LEVEL_EPC_DUARTS4	0x70

#define EVINTR_LEVEL_MAXHIDEV		0x71

/* These interrupts are blocked by splhi */

#if defined(MULTIKERNEL) && defined(CELL)
/* We need an interrupt level for CC interrupts. So we don't get one for
 * EVINTR_LEVEL_TLB (so let CPUACTION handle this request).  Performance
 * issue only and it's not important for MULTIKERNEL CELL.
 */
#define EVINTR_LEVEL_CCINTR		0x72
#define EVINTR_LEVEL_GROUPACTION	0x73
#define EVINTR_LEVEL_CPUACTION		0x74
#define EVINTR_LEVEL_HIGH		0x74
#define	EVINTR_LEVEL_TLB		0x74   

#else /* !MULTIKERNEL || !CELL */

#define EVINTR_LEVEL_GROUPACTION	0x72
#define EVINTR_LEVEL_CPUACTION		0x73
#define EVINTR_LEVEL_HIGH		0x73
#define	EVINTR_LEVEL_TLB		0x74
#endif /* !MULTIKERNEL || !CELL */

/* These interrupts are blocked by splprof */
#define EVINTR_LEVEL_DEBUG		0x75
#define EVINTR_LEVEL_EPC_PROFTIM	0x76

/* high priority duart interrupt */
#define EVINTR_LEVEL_EPC_HIDUART	0x77

/*
 * Error levels could potentially be different, but our current
 * handler simply gathers all potential state on any of these
 * errors.
 */
#define EVINTR_LEVEL_ERR		0x78	/* start error lvls */
#define EVINTR_LEVEL_EPC_ERROR		0x78
#define EVINTR_LEVEL_DANG_ERROR		0x79
/* EVINTR_LEVEL_VMECC_ERROR is moved down to 0x40 to handle write errors */

#define EVINTR_LEVEL_FCHIP_ERROR	0x7a
#define EVINTR_LEVEL_IO4_ERROR		0x7b
#define EVINTR_LEVEL_MC3_MEM_ERROR	0x7c
#define EVINTR_LEVEL_MC3_EBUS_ERROR	0x7d
#define EVINTR_LEVEL_CC_ERROR		0x7e
#define EVINTR_LEVEL_MAX		0x7f


/*
 * cpuid's to receive various interrupts.
 *
 * VME interrupts are specified via system.gen.
 * DUART interrupts go to 0, and enet interrupts to 1.
 * SCSI and PPORT are sent to randomly chosen cpus.
 */
#define EVINTR_DEST_VMECC_DMAENG(adap)	0
#define EVINTR_DEST_VMECC_AUX0(adap)	0
#define EVINTR_DEST_VMECC_AUX1(adap)	0
#define EVINTR_DEST_EPC_KBDMS		0
#define EVINTR_DEST_EPC_DUART1		0
#define EVINTR_DEST_EPC_DUART2		0
#define EVINTR_DEST_EPC_ENET		1
#define EVINTR_DEST_EPC_PPORT		0
#define EVINTR_DEST_EPC_SPARE		1
#define EVINTR_DEST_EPC_PROFTIM		EVINTR_GLOBALDEST
#define EVINTR_DEST_CPUACTION		0 /* not really used */
#define EVINTR_DEST_GROUPACTION         0 /* not really used */
#define	EVINTR_DEST_S1CHIP		EVINTR_ANY

/*
 * Send all error interrupts to processor 0
 */
#define EVINTR_DEST_ERROR		0

#define EVINTR_DEST_EPC_ERROR		EVINTR_DEST_ERROR
#define EVINTR_DEST_FCG_ERROR		EVINTR_DEST_ERROR
#define EVINTR_DEST_VMECC_ERROR		EVINTR_DEST_ERROR
#define EVINTR_DEST_FCHIP_ERROR		EVINTR_DEST_ERROR
#define EVINTR_DEST_IO4_ERROR		EVINTR_DEST_ERROR
#define EVINTR_DEST_MC3_EBUS_ERROR	EVINTR_DEST_ERROR
#define EVINTR_DEST_MC3_MEM_ERROR	EVINTR_DEST_ERROR
#define EVINTR_DEST_CC_ERROR		EVINTR_DEST_ERROR


#if LANGUAGE_ASSEMBLY
/*
 * This macro allows us to wait for a change to the CEL register to
 * take effect.  If we don't WAIT after storing into CEL the effect
 * can be delayed for 10s of instructions.
 * On TFP reading the current CEL takes a long time, so we save a copy
 * of the current CEL in the PDA for fast cachable reading.
 *
 * On TFP writing the CEL register also takes a long time (20% hit on AIM3).
 * So we attempt to keep the current "CEL" level in the PDA (VPDA_CELSHADOW)
 * and also keep track of the last value written to HW (VPDA_CELHW).
 *
 * On an interrupt, we copy the current SHADOW into the HW CEL register,
 * update the CELHW in the PDA, and WAIT_FOR_CEL level to take affect.  Then
 * check to see if we really have any interrupts.
 * In routines which raise CEL level, we simply write to VPDA_SHADOW.  On
 * routines which lower CEL level (or may lower it) we check the new value
 * against the last value actually written to HW and we update the HW register
 * only it the new value is less.
 *
 * WAIT_FOR_CEL(celptr)		wait for CEL to be updated
 * FORCE_NEW_CEL(newval,celptr)	update the SW CEL and force value to HW CEL
 * RAISE_CEL(newval,celptr)	raise CEL level (doesn't touch HW CEL)
 * LOWER_CEL(newval,scr1,scr2)	lower CEL level (touch HW CEL if needed)
 */

#if R4000 || R10000
#ifdef DELAY_CEL_WRITES
#define WAIT_FOR_CEL(celptr)
#define RAISE_CEL(newval,celptr)	sb	newval,VPDA_CELSHADOW(zero)

#if R10000
#define LOWER_CEL(newval,celptr,scr)	sb	newval,VPDA_CELSHADOW(zero);\
					lbu	scr,VPDA_CELHW(zero)	;\
					slt	scr,newval,scr		;\
					beq	scr,zero,50f		;\
					nop				;\
					LI	celptr,EV_CEL		;\
					sd	newval,(celptr)		;\
					sb	newval,VPDA_CELHW(zero)	;\
					ld	zero,(celptr)		;\
50:					
#else
/* Note: The final "ld zero,(celptr)" is actually not necessary EXCEPT to
 * handle the case where higher level code lowers spl to allow an interrupt
 * to occur, then almost immediately raises spl again.  By reading the HW
 * CEL register (but only if we've just written it) we allow sufficient
 * time for the interrupt to occur.
 */
#define LOWER_CEL(newval,celptr,scr)	sb	newval,VPDA_CELSHADOW(zero);\
					lbu	scr,VPDA_CELHW(zero)	;\
					slt	scr,newval,scr		;\
					beq	scr,zero,50f		;\
					nop				;\
					LI	celptr,EV_CEL		;\
					sd	newval,(celptr)		;\
					sb	newval,VPDA_CELHW(zero)	;\
					ld	zero,(celptr)		;\
50:					
#endif

#define FORCE_NEW_CEL(newval,celptr)	sb	newval,VPDA_CELSHADOW(zero);\
					sd	newval,(celptr);\
					sb	newval,VPDA_CELHW(zero);
#else /* !DELAY_CEL_WRITES */

#define WAIT_FOR_CEL(celptr)		ld	zero,0(celptr)
#define FORCE_NEW_CEL(newval,celptr)	sb	newval,VPDA_CELSHADOW(zero);\
     					sd	newval,(celptr)
#define RAISE_CEL(newval, celptr)	LI	celptr,EV_CEL	;\
					FORCE_NEW_CEL(newval, celptr)
#if R4000
#define	LOWER_CEL(newval,celptr,scr)	LI	celptr,EV_CEL	;\
					FORCE_NEW_CEL(newval, celptr)
#else
/* WAIT for CEL to take effect so that the SCC gets enough time
 * to post pending interrupt before the CEL gets raised.
 */
#define	LOWER_CEL(newval,celptr,scr)	LI	celptr,EV_CEL	;\
					FORCE_NEW_CEL(newval, celptr); \
					WAIT_FOR_CEL(celptr)
#endif
#endif /* !DELAY_CEL_WRITES */
#endif /* R4000 || R10000 */

#if TFP
/* TFP does not need WAIT_FOR_CEL since all uncached writes are
 * sychronous (i.e. complete before we start execution of next instruction)
 */
#define WAIT_FOR_CEL(celptr)
#define RAISE_CEL(newval,celptr)	sb	newval,VPDA_CELSHADOW(zero)

#define LOWER_CEL(newval,celptr,scr)	sb	newval,VPDA_CELSHADOW(zero);\
					lbu	scr,VPDA_CELHW(zero)	;\
					slt	scr,newval,scr		;\
					beq	scr,zero,50f		;\
					nop				;\
					dmfc0	celptr,C0_CEL		;\
					sd	newval,(celptr)		;\
					sb	newval,VPDA_CELHW(zero)	;\
50:					

#define FORCE_NEW_CEL(newval,celptr)	sb	newval,VPDA_CELSHADOW(zero);\
					sd	newval,(celptr);\
					sb	newval,VPDA_CELHW(zero);
#endif	/* TFP */

#else /* LANGUAGE_C */

#include "sys/types.h"
#include "sys/reg.h"

/*
 * There will be a one to one mapping between interrupt source and level.
 */

typedef void (*EvIntrFunc)(eframe_t *, void *);

extern void evintr_init(void);
extern int  evintr_connect(evreg_t *, int, int, cpuid_t, EvIntrFunc, void *);
extern void setrtvector(EvIntrFunc);

extern cpuid_t		master_procid;

#define	EV_IP_BITS 64			/* width of EV_IP[01] in bits */
/*
 * When EV_HPIL isn't enough.
 * Returns 1 if interrupt 'level' is asserted, 0 if not
 */
#define	EV_IP_ISSET(level) \
	(level < EV_IP_BITS \
		? (EV_GET_LOCAL(EV_IP0) & 1 << level ? 1 : 0) \
		: (EV_GET_LOCAL(EV_IP1) & 1 << level - EV_IP_BITS ? 1 : 0))
#endif /* LANGUAGE_C */


#define WAIT_FOR_CEL_IMM(_reg)	LI      _reg, EV_CEL; \
				WAIT_FOR_CEL(_reg)

#define GET_CEL(_reg)		lbu     _reg, VPDA_CELSHADOW(zero)
#define SPLHI_DISABLED_INTS	SRB_SWTIMO|SRB_NET|SRB_TIMOCLK|SRB_UART|SRB_SCHEDCLK
#define SPLHI_CEL		EVINTR_LEVEL_HIGH+1

#endif

