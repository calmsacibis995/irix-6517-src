/***********************************************************************\
*	File:		prom_leds.h					*
*									*
*	Contains the led values displayed at various phases in the	*
*	PROM startup sequence. 						*
*									*
\***********************************************************************/

#ifndef _IP21_PROM_LEDS_H_
#define _IP21_PROM_LEDS_H_

#ident "$Revision"

#define PLED_STARTUP    1				/* 1 */
/* PROM has started execution */

#define PLED_TESTICACHE PLED_STARTUP+1			/* 2 */
/* test icache */

#define PLED_CHKSCACHESIZE PLED_TESTICACHE+1		/* 3 */
/* check scache size */

#define PLED_INITCOP0 PLED_CHKSCACHESIZE+1		/* 4 */
/* init cop0 registers */

#define PLED_FLUSHTLB	PLED_INITCOP0 +1		/* 5 */
/* flush tlb */

#define PLED_FLUSHSAQ	PLED_FLUSHTLB +1		/* 6 */
/* flush store address queue */

#define PLED_CLEARTAGS  PLED_FLUSHSAQ +1  		/* 7 */
/* Clearing the pd tags */

#define PLED_CKCCLOCAL  PLED_CLEARTAGS+1 		/* 8 */
/* Testing CC chip local registers */

#define PLED_CCLFAILED_INITUART PLED_CKCCLOCAL+1	/* 9 */
/* Failed the local test but trying to initialize the UART anyway */

#define PLED_CCINIT1 PLED_CCLFAILED_INITUART+1 		/* 10 */

#define PLED_CKCCCONFIG PLED_CCINIT1+1 			/* 11 */
/* Testing the CC config registers */
/* requires a usable bus to pass) */
/* NOTE: Hanging in this test usually means that the bus has failed.
 * 	Check the oscillator.
 */

#define PLED_CCCFAILED_INITUART PLED_CKCCCONFIG+1 	/* 12 */
/* Failed the config reg test but trying to initialize the UART anyway */

#define PLED_NOCLOCK_INITUART PLED_CCCFAILED_INITUART+1 /* 13 */
/* CC clock isn't running init uart anyway */

#define PLED_CCINIT2 PLED_NOCLOCK_INITUART+1		/* 14 */
/* Init CC chip config registers */

#define PLED_UARTINIT  PLED_CCINIT2+1           	/* 15 */
/* Init CC chip UART */
/* NOTE: Hanging in this test usually means that the UART clock is bad.
 * 	Check the connection to the system controller.
 */

#define PLED_CCUARTDONE  PLED_UARTINIT+1		/* 16 */
/* Finished initializing the CC chip UART */

#define PLED_CKACHIP  PLED_CCUARTDONE+1			/* 17 */
/* Testing A chip registers */

#define PLED_AINIT PLED_CKACHIP+1 			/* 18 */
/* Initializing the A ahip */

#define PLED_CKEBUS1 	PLED_AINIT+1			/* 19 */
/* Checking the EBus with interrupts. */

#define PLED_SCINIT PLED_CKEBUS1+1			/* 20 */
/* Init system controller */

#define PLED_BMARB PLED_SCINIT+1			/* 21 */
/* Arbitrating for a bootmaster */

#define PLED_BMASTER PLED_BMARB+1			/* 22 */
/* This processor is the bootmaster */

#define PLED_CCJOIN  PLED_BMASTER+1			/* 23 */
/* Test CC join register */

#define PLED_WG	PLED_CCJOIN+1				/* 24 */
/* Test write gatherer register & buffer */

#define PLED_POD PLED_WG+1				/* 25 */
/* Setting up this CPU slice for POD mode */

#define PLED_PODLOOP PLED_POD+1				/* 26 */
/* Entering POD loop */

#define PLED_CKPDCACHE1  PLED_PODLOOP+1			/* 27 */
/* Checking the pd cache */

#define PLED_MAKESTACK  PLED_CKPDCACHE1+1 		/* 28 */
/* Creating a stack in the primary data cache */

#define PLED_MAIN PLED_MAKESTACK+1			/* 29 */
/* Jumping into C code - calling main() */

#define PLED_LOADPROM  PLED_MAIN+1			/* 30 */
/* Loading IO4 prom */

#define PLED_CKSCACHE1  PLED_LOADPROM+1			/* 31 */
/* First pass of secondary cache testing - test the scache like a RAM */

#define PLED_CKBT	PLED_CKSCACHE1+1		/* 32 */
/* Check the bus tags */

#define PLED_INSLAVE    PLED_CKBT+1			/* 33 */
/* This CPU is entering slave mode */

#define PLED_PROMJUMP   PLED_INSLAVE+1			/* 34 */
/* Jumping to the IO prom */

#define PLED_NMIJUMP PLED_PROMJUMP+1			/* 35 */
/* Jumping through the NMIVEC */

#define PLED_INV_IDCACHES PLED_NMIJUMP+1		/* 36 */
/* Slave processor invalidate I & D caches */

#define PLED_INV_SCACHE PLED_INV_IDCACHES+1		/* 37 */
/* Slave processor invalidate G cache */

#define PLED_WRCONFIG   PLED_INV_SCACHE+1		/* 38 */
/* Writing evconfig structure:
 *	The master CPU writes the whole array
 *	The slaves only write their own entries.
 */


/*
 * Failure mode LED values.  If the Power-On Diagnostics
 * find an unrecoverable problem with the hardware,
 * they will call the flash leds routine with one of
 * the following values as an argument.
 */

#define FLED_ICACHE_FAIL PLED_WRCONFIG+1                /* 39 */
#define FLED_CANTSEEMEM  FLED_ICACHE_FAIL+1		/* 40 */	
/* Flashed by slave processors if they take an exception while trying to
 * write their evconfig entries.  Often means the processor's getting D-chip
 * parity errors.
 */

#define FLED_SCACHE_SIZE_INCONSISTENT FLED_CANTSEEMEM+1	/* 41 */
/* Scache size is inconsistent */

#define	FLED_IMPOSSIBLE1 FLED_SCACHE_SIZE_INCONSISTENT+1       /* 42 */
/* We fell through one of the supposedly unreturning subroutines.
 * Really shouldn't be possible. 
 */

#define FLED_DEADCOP1 FLED_IMPOSSIBLE1+1		/* 43 */
/* Coprocessor 1 is dead - not seeing this doesn't mean it works. */

#define FLED_CCCLOCK	FLED_DEADCOP1+1			/* 44 */
#define FLED_CCLOCAL	FLED_CCCLOCK+1			/* 45 */
/* Failed CC local register tests */

#define FLED_CCCONFIG	FLED_CCLOCAL+1		/* 46 */
/* Failed CC config register tests */

#define FLED_ACHIP	FLED_CCCONFIG+1		/* 47 */
/* Failed A Chip register tests */

#define FLED_BROKEWB	FLED_ACHIP+1		/* 48 */
/* By the time this CPU had arrived at the bootmaster arbitration barrier,
 * the rendezvous time had passed.  Thsi implies that a CPU is running too
 * slowly, the ratio of bus clock to CPU clok rate is too high, or a bit
 * in the CC clock is stuck on.
 */

#define FLED_BADDCACHE  FLED_BROKEWB+1		/* 49 */
/* This CPU's primary data cache test failed */

#define FLED_BADIO4	FLED_BADDCACHE+1 	/* 50 */
/* The IO4 board is bad - can't get to the console. */

/* Exception failure mode values */
#define FLED_UTLBMISS	FLED_BADIO4+1		/* 51 */
/* Took a TLB Refill exception */

#define FLED_KTLBMISS	FLED_UTLBMISS+1		/* 52 */
/* Took a kernel TLB Refill exception */

#define FLED_UNUSED1 	FLED_KTLBMISS+1 	/* 53 */

#define FLED_GENERAL	FLED_UNUSED1+1		/* 54 */
/* Took a general exception */

#define FLED_NOTIMPL	FLED_GENERAL+1		/* 55 */
/* Took an unimplemented exception */

#define FLED_SCACHE_SIZE_WRONG	FLED_NOTIMPL+1	/* 56 */
/* Bad scache size */

#endif
