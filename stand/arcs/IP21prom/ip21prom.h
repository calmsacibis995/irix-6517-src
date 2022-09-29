/***********************************************************************\
*	File:		ip21prom.h					*
*									*
*	Random definitions used by various files in the IP21 PROM.	*
*									*
\***********************************************************************/

#ident "$Revision: 1.30 $"

#ifndef	_IP21PROM_H_
#define _IP21PROM_H_

#ifdef LANGUAGE_ASSEMBLY
#include <asm.h>
#endif

#include <sys/tfp.h>

/* A simple macro for use during bringup */
#ifdef DEBUG 
#  define DPRINT(x)			\
        dla      a0, 99f ;		\
        jal     ccuart_puts ;		\
        nop ;				\
        .data ;				\
99:     .asciiz x ;			\
        .text
#else
#  define DPRINT(_x)
#endif /* DEBUG */

#define SCPRINT(x)			\
	dla	a0, 99f  ;		\
	jal	sysctlr_message ;	\
	nop ;				\
	.data ;				\
99:	.asciiz x ;			\
	.text

/*
 * The IP21 PROM uses various floating point registers in addition to the
 * COP0 Work registers for temporary storage.  POD owns FP regs 6 - 31.  
 * The mainline prom owns registers 0 - 5 (though 0 is currently unused).
 * Currently, they're allocated as follows:
 */

#ifdef LANGUAGE_ASSEMBLY

#define BSR_REG		C0_WORK0    /* Boot Status Register */
#define NOFAULT_REG	C0_PBASE
#define ASMHNDLR_REG	C0_GBASE
#define DUARTBASE_REG	C0_UBASE
#define NMIVEC_REG	$f4
#define DIAGVAL_REG	$f5	    /* Holds our diagval if we abdicate. */
#define EV_ERTOIP_REG	$f6

#define CPU_SPEED 90


/*
 * DMFC1 and DMTC1 macros are defined only due to paranoia and the fear 
 * that FPU may not (initially) correctly function as intended. The special 
 * NOPs can be removed once there is established confidence in FPU.
 */
#define DMFC1(_v, _fpr)		\
	dmfc1	_v, _fpr;	\
	NOP_MFC1_HAZ
#define DMTC1(_v, _fpr)		\
	dmtc1	_v, _fpr;	\
	NOP_MTC1_HAZ
#endif /* LANGUAGE_ASSEMBLY */

/*
 * The Boot Status register tracks the progress of the PROM.
 * It is used by the Exception routines to determine the PROM's
 * actions when an exception occurs.
 *
 *  __________________________________________________________________________
 * |         | AB | SL | EPCIOA | IO | ND | MM | GS | NV | EF | IF | CI | RE |
 * +-------------------+--------+----+----+----+----+----+----+----+---------+
 *  31         13   12  11     9   8    7    6    5    4    3    2    1    0
 */

#ifdef LANGUAGE_ASSEMBLY
#define GET_BSR(_x)		DMFC0(_x, BSR_REG)
#define SET_BSR(_x)		DMTC0(_x, BSR_REG)
#define GET_DUARTBASE(_x)	DMFC0(_x, DUARTBASE_REG)

#endif /* LANGUAGE_ASSEMBLY */

#ifdef _LANGUAGE_C
#ifdef DEBUG
#  define DPRINTF ccloprintf
#else
#  define DPRINTF null
#endif
#endif  /*  _LANGUAGE_C */

/* Boot status bits: */

#define BS_RECOV_ERR		0x0001	/* Set when recoverable error occurs */
#define BS_CCUART_INITED	0x0002	/* Set when CC UART is working */
#define BS_POD_MODE		0x0004	/* Goto POD mode instead of IO4 PROM */ 
#define BS_EPC_FOUND		0x0008	/* Set when valid EPC is found */
#define BS_NVRAM_VALID		0x0010	/* Set if NVRAM is legitimate */
#define BS_GOOD_SYSCTLR		0x0020	/* Set if SysCtlr passed Arbitration */
#define BS_MANUMODE		0x0040	/* Set if running with CC UART */
#define BS_NO_DIAGS		0x0080	/* Set if no pod diags should be run */
#define BS_USE_EPCUART		0x0100	/* Set if EPC UART is used for I/O */
#define BS_SLAVE		0x1000	/* Set if we're the slave */
#define BS_ABDICATED		0x2000	/* Set if we've already been master. */

#define BS_EPCIOA_MASK		0xe00	/* Three bits containing EPC IOA */
#define BS_EPCIOA_SHFT		9
	
/* We check the basic sanity of the clock frequency with these. */
#define MIN_FREQ	1000
#define MAX_FREQ	100000000
#define DEFLT_FREQ	50000000

/* If you try to run a memory test on memory lower than this address,
 * the prom will print a warning.
 */
#define LOMEM_STRUCT_END	0x4000

/* By calling the epcuart I/O routines directly the user can select between
 * channel A and channel B.  
 */
#define CHN_A	(EPC_DUART2 + 0x8)
#define CHN_B	(EPC_DUART1 + 0x0)

/* Bits which should always be set in the status register for most
   of the IP21 PROM (except maybe POD mode) */
#define PROM_SR		    SR_CU1|SR_FR|SR_PAGESIZE

/*
 * The IP19 couldn't deal with software emulation of long longs,
 * so we define our own set of EPC set and get primitives
 */
#define EPC_PROMGET(_r, _i, _reg) \
        load_double_los((long long *)(SWIN_BASE((_r), (_i)) + (_reg)))

#define EPC_PROMSET(_r, _i, _reg, _value) \
        store_double_lo((long long *)(SWIN_BASE((_r),(_i)) + (_reg)), (_value))

/*
 * I & D Caches initialization starting address define.
 */
#define DC_INIT_ADDRESS	    0x9000000000000000

/*
 * GCache-related defines.
 */
#define SAQ_INIT_ADDRESS    0x9000000018000380  /* 2 unused local CC register address */
#define	SAQ_DEPTH	    32
#define BTAG_ST_INIT	    0x000000000001f000  /* bus tag state init value */
#define PTAG_ST_INIT	    0x0000007c00000000  /* proc tag state init value */

#define BTAG_ST_INV	    0x000000000001f000  /* bus tag state invalidate */
#define PTAG_ST_INV 	    0x0000007c00000000  /* proc tag state invalidate */
#define BTAG_ST_EXCL	    0x000000000001f0aa  /* bus tag state exclusive */
#define PTAG_ST_EXCL	    0x0000007c2a800000  /* proc tag state exclusive */

#define SCACHE_LINE	    512

#ifdef _LANGUAGE_ASSEMBLY
#ifndef r0

#define r0	$0
#define r1	$1
#define r2	$2
#define r3	$3
#define r4	$4
#define r5	$5
#define r6	$6
#define r7	$7
#define r8	$8
#define r9	$9
#define r10	$10
#define r11	$11
#define r12	$12
#define r13	$13
#define r14	$14
#define r15	$15
#define r16	$16
#define r17	$17
#define r18	$18
#define r19	$19
#define r20	$20
#define r21	$21
#define r22	$22
#define r23	$23
#define r24	$24
#define r25	$25
#define r26	$26
#define r27	$27
#define r28	$28
#define r29	$29
#define r30	$30
#define r31	$31

#endif /* r0 */
#endif /* _LANGUAGE_ASSEMBLY */

#define PODMODE_BIT	    0x01    /* to be used in place of SR_DE */
#endif /* _IP21PROM_H_ */

