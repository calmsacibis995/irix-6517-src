/*
 * File: ip25prom.h
 * Purpose: IP25 CPU boot prom defines.
 *
 * Copyright 1995, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 *
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 *
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 *
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 */

#ident "$Revision: 1.8 $"

#ifndef	_IP25PROM_H_
#define	_IP25PROM_H_

#if	LANGUAGE_ASSEMBLY
#   include <asm.h>

#   define 	DMFC1(_v, _r)	dmfc1	_v,_r
#   define	DMTC1(_v, _r)	dmtc1	_v,_r

#   define 	MFC1(_v, _r)	mfc1	_v,_r
#   define	MTC1(_v, _r)	mtc1	_v,_r

#   define	MESSAGE(r, s)	\
         dla	    r, 99f; .data ;99: .asciiz s; .text;

#   define	PMESSAGE_PTR(r)	or a0,r; jal pod_puts; nop
#   define	PMESSAGE(s)	MESSAGE(a0,s); jal pod_puts; nop
#   define      PHEX(x)		or a0,zero,x; jal pod_puthex64; nop
#   define	PHEX32(x)	or a0,zero,x; jal pod_puthex32; nop
#   define	PGETC()		jal pod_getc; nop

#   if DEBUG
#   	define	DPRINT(x)	PMESSAGE(x)
#   	define	DPRINTHEX(x)	PHEX(x)
#   else 
#       define	DPRINT(x)
#   	define	DPRINTHEX(x)
#   endif

/*
 * Define: SCPRINT
 * Purpose: Define Assembler call to print a message on the 
 *	    system controller panel.
 * Parameters: c - ascii string to print.
 */
#   define	SCPRINT(c)\
            dla	a0, 99f; jal sysctlr_message; nop; .data; 99: .asciiz c; .text

#endif

/*
 * Define: LEDS
 * Purpose: Defines call to routine to display pattern on leds. 
 * Paramters: l - led value to display.
 */
#if LANGUAGE_ASSEMBLY
#   define	LEDS(l)	nop; or a0,zero,l; dli v0,EV_LED_BASE; sd a0,0(v0);
#else
#   define 	LEDS(l) set_cc_leds(l)
#endif

/*
 * Define: FLASH
 * Purpose: Defines call to routine to display pattern on leds, flashing
 *	    all off followed by this pattern forever.
 * Paramters: l - led value to display.
 */
#if LANGUAGE_ASSEMBLY
#   define 	FLASH(l) nop;or a0,zero,l; jal flash_cc_leds; nop
#else
#   define 	FLASH(l) flash_cc_leds(l)
#endif

/*
 * Define: DELAY
 * Purpose: To spin-wait for the specifid number of microseconds.
 * Parameters: us - number of micro-seconds to wait.
 */
#ifndef	SABLE
#   ifdef	DELAY
#       undef	    DELAY
#   endif
#   if LANGUAGE_ASSEMBLY
#	define	DELAY(us)	dli	a0, us; jal delay; nop
#   else
#	define	DELAY(us)	delay(us)
#   endif
#else
#   define	DELAY(us)
#endif

#if defined(_LANGUAGE_ASSEMBLY)

/*
 * Defines: We use Coprocessor registers $f0 - $f? to hold special values:
 *
 * 	$f0:
 * 	$f6  - $f31 : owned by pod.
 */

#define	BR_BSR			$f0	/* boot status register - see below */
#define	BR_NOFAULT		$f1
#define	BR_ASMHNDLR		$f2
#define	BR_DUARTBASE		$f3
#define	BR_ERTOIP		$f4
#define	BR_DIAG			$f5	/* diag value if we abdicate */

/*
 * Define: DMFBR
 * Purpose: Defines macro to move value from boot register
 * Parameters: to - regsiter to move to
 *	       from - register to move from (defined as BR_XXXX)
 */
#define	DMFBR(to, from)	DMFC1(to, from)

/*
 * Define: DMTBR
 * Purpose: Defines macro to move value to boot register
 * Parameters: to - regsiter to move to
 *	       from - register to move from (defined as BR_XXXX)
 */
#define	DMTBR(from, to)	DMTC1(from, to)

#endif

/* 
 * Boot Status Register 
 *
 * +---------+----+----+--------+----+----+----+----+----+----+----+---------+
 * |         | AB | SL | EPCIOA | IO | ND | MM | GS | NV | EF | IF | CI | RE |
 * +-------------------+--------+----+----+----+----+----+----+----+---------+
 *  31         13   12  11     9   8    7    6    5    4    3    2    1    0
 */

#define	BSR_RE			0x0001	/* recoverable error occured */
#define BSR_CCUI		0x0002	/* CC uart inited */
#define	BSR_POD			0x0004	/* Go into POD mode not IO4 */
#define	BSR_EPC			0x0008	/* EPC found */
#define	BSR_NVRAM		0x0010	/* MVRAM is legitimate */
#define	BSR_SYSCTLR		0x0020	/* system controller passed arb */
#define BSR_MANUMODE		0x0040  /* running with CC UART */
#define BSR_NODIAG		0x0080	/* no diagnostics */
#define	BSR_EPCUART		0x0100	/* Use EPC uart */
#define BSR_SLAVE		0x1000	/* we are a slave */
#define	BSR_ABDICATE		0x2000	/* Already been master */
#define	BSR_DEBUG		0x4000	/* debug mode - slaves */

#define	BSR_EPCIOA_SHFT		9	/* EPC IOA shift value */
#define	BSR_EPCIOA_MASK		(0xf<<BSR_EPCIOA_SHFT)

#define	PROM_SR			SR_CU1|SR_FR|SR_KX|SR_BEV

/* 
 * If you try to run a memory test on memory lower than this address,
 * the prom will print a warning.
 */
#define LOMEM_STRUCT_END	0x4000

/* 
 * By calling the epcuart I/O routines directly the user can select between
 * channel A and channel B.  
 */
#define CHN_A	(EPC_DUART2 + 0x8)
#define CHN_B	(EPC_DUART1 + 0x0)

#define EPC_PROMGET(_r, _i, _reg) LD_LO(SWIN_BASE((_r), (_i)) + (_reg))

#define EPC_PROMSET(_r, _i, _reg, _value) \
        SD_LO((SWIN_BASE((_r),(_i)) + (_reg)), (_value))

#if	DEBUG
#   define	DPRINTF(x)	loprintf x
#else
#   define	DPRINTF(x)
#endif

#if _LANGUAGE_C
#   define	SPNUM(_s,_p) {\
			 __uint64_t __s, __p;\
				__s = LD(EV_SPNUM); \
				__p = __s & EV_SLOTNUM_MASK>>EV_SLOTNUM_SHFT;\
				__s = __s & EV_PROCNUM_MASK>>EV_PROCNUM_SHFT;\
				_s = (__uint32_t)__s; _p = (__uint32_t)__p; \
			    }

#   define	GET_MY_CONFIG(r)	\
         LD(EV_CONFIGADDR((LD(EV_SPNUM)&EV_SPNUM_MASK)>>EV_SLOTNUM_SHFT, \
			  (LD(EV_SPNUM)&EV_PROCNUM_MASK)>>EV_PROCNUM_SHFT,\
			  r))

#   define	GET_CC_REV()	(GET_MY_CONFIG(EV_CFG_IWTRIG) >> 28 & 0xf)

#else 
#   define	GET_CC_REV(_v, _t)	\
                   EV_GET_SPNUM(_v, _t); \
                   EV_GET_PROCREG(_v, _t, EV_CFG_IWTRIG, _v); \
                   dsrl _v, 28;\
	           andi	_v, 0xf
#endif

#if _LANGUAGE_C

#   define	LB(x)	((char)(*(volatile char *)(x)))
#   define	LH(x)	((short)(*(volatile short *)(x)))
#   define	LW(x)	((__int32_t)(*(volatile __int32_t *)(x)))
#   define	LD(x)	((__int64_t)(*(volatile __int64_t *)(x)))

#   define	LBU(x)	((unsigned char)(*(volatile unsigned char *)(x)))
#   define	LHU(x)	((unsigned short)(*(volatile unsigned short *)(x)))
#   define	LWU(x)	((__uint32_t)(*(volatile __uint32_t *)(x)))
#   define	LDU(x)	((__uint64_t)(*(volatile __uint64_t *)(x)))

#   define	SB(x,v)	(*(volatile char *)(x))  = (char)(v)
#   define	SH(x,v)	(*(volatile short *)(x)) = (short)(v)
#   define	SW(x,v)	(*(volatile __int32_t *)(x)) = (__int32_t)(v)
#   define	SD(x,v)	(*(volatile __int64_t *)(x)) = (__int64_t)(v)


#   define	LD_HI(x) ((__uint32_t)((LDU(x)&0xffffffff00000000)>>32))
#   define	LD_LO(x) ((__int32_t)(LD(x) & 0x00000000ffffffff))

#   define	SD_HI(x,v)	SD(x, (v) & 0xffffffff00000000)
#   define	SD_LO(x,v) 	SD(x, (v) & 0x00000000ffffffff)

#   define	LD_RTC() ((__uint32_t)((LD(EV_RTC) >> 8)))

#   ifdef	EV_GET_CONFIG
#       undef	    EV_GET_CONFIG
#   endif
#   define	EV_GET_CONFIG(s, r)	LD(EV_CONFIGADDR((s),0,(r)))

#   ifdef	EV_SET_CONFIG
#       undef	    EV_SET_CONFIG
#   endif
#   define	EV_SET_CONFIG(s, r, v)	SD(EV_CONFIGADDR((s),0,(r)), (v))

/*
 * Macro: UNCACHED_ABSADDR
 * Purpose: Traslate an absolute address into an EBUS address, taking into 
 *	    account the "hole" in the address space for SCC registers etc, 
 *	    and re-mapping them to the "alternate" addresses.
 * Parameters: a - address
 * Returns: 64-bit address in uncached space.
 * Notes: The everest address map looks like the following:
 *
 * 			0 --> 0x00100000		Memory
 *			0x00100000 -> 0x001fffff	SCC/Config range
 *			0x00200000 -> 0xffffffffff	Memory
 */
#    define	UNCACHED_ABSADDR(a)				\
        ((__uint64_t)(a)) | 0x9000000000000000 | 		\
            ((((__uint64_t)(a)) & 0xfff0000000) == 0x0010000000)\
                ? 0xff0000000 : 0


#endif

/*
 * Defines: POD_STACKVADDR
 *          POD_STACKPADDR
 *
 * Purpose: Defines the virtual and physical address of the POD stack
 *	    that exists ONLY in the primary cache.
 */
#define	POD_STACKVADDR		0xa8000000000fe000
#define POD_STACKPADDR		0x00000000000fe000
#define	POD_STACKSIZE		0x1000

/*
 * Define: POD_CODEVADDR
 *	   POD_CODEPADDR
 *	   POD_CODESIZE
 *
 * Purpose: Defines the virtual and physical address of POD cache text
 * 	    region used to run PIC cached text. SIZE is the maximum  
 * 	    number of bytes allowed.
 */
#define	POD_CODEVADDR		(POD_STACKVADDR - POD_CODESIZE)
#define	POD_CODEPADDR		(POD_STACKPADDR - POD_CODESIZE)
#define	POD_CODESIZE		0x1000

/*
 * Define: POD_TESTVADDR
 * 	   POD_TESTPADDR
 *
 * Purpose: Defines the virtual and physical addresses that POD uses for
 * 	    testing the secondary cache - POD_TESTVADDR MUST BE 
 *	    POD_TESTPADDR + K0_BASE
 */
#define	POD_TESTPADDR		0x01000000
#define	POD_TESTVADDR		(POD_TESTPADDR + K0_BASE)

#endif _IP25PROM_H_
