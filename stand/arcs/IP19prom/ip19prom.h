/***********************************************************************\
*	File:		ip19prom.h					*
*									*
*	Random definitions used by various files in the IP19 PROM.	*
*									*
\***********************************************************************/

#ident "$Revision: 1.26 $"

#ifndef	_IP19PROM_H_
#define _IP19PROM_H_

/* A simple macro for use during bringup */
#ifdef DEBUG 
#  define DPRINT(x)			\
        la      a0, 99f  ;              \
        jal     ccuart_puts ;           \
        nop ;                           \
        .data ;                         \
99:     .asciiz x ;                    	\
        .text 
#else
#  define DPRINT(_x)
#endif /* DEBUG */

#define SCPRINT(x)			\
	la	a0, 99f  ;		\
	jal	sysctlr_message ;	\
	nop ;				\
	.data ;				\
99:	.asciiz x ;			\
	.text

/*
 * The IP19 PROM uses various floating point registers for temporary
 * storage.  POD own fp regs 6 - 31.  The mainline prom owns registers
 * 0 - 5.  Currently, they're allocated as follows:
 */

#define BSR_REG		$f0
#define NOFAULT_REG	$f1
#define ASMHNDLR_REG	$f2
#define DUARTBASE_REG	$f3
#define NMIVEC_REG	$f4
#define DIAGVAL_REG	$f5	/* Holds our diagval if we abdicate. */

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
#define GET_BSR(_x) 		\
	mfc1	_x, BSR_REG ; 	\
	nop ; nop 
#define SET_BSR(_x)		\
	mtc1	_x, BSR_REG ;	\
	nop ; nop
#define GET_DUARTBASE(_x)	\
	mfc1	_x, DUARTBASE_REG ; nop ; nop

#endif /* LANGUAGE_ASSEMBLY */

#ifdef _LANGUAGE_C
#ifdef DEBUG
#  define DPRINTF loprintf
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
   of the IP19 PROM (except maybe POD mode) */
#define PROM_SR	SR_DE|SR_KX|SR_FR|SR_CU1|SR_BEV

/*
 * The IP19 couldn't deal with software emulation of long longs,
 * so we define our own set of EPC set and get primitives
 */
#define EPC_PROMGET(_r, _i, _reg) \
        load_double_los(SWIN_BASE((_r), (_i)) + (_reg))

#define EPC_PROMSET(_r, _i, _reg, _value) \
        store_double_lo(SWIN_BASE((_r),(_i)) + (_reg), (_value))


#if _LANGUAGE_ASSEMBLY
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


#endif /* _IP19PROM_H_ */
