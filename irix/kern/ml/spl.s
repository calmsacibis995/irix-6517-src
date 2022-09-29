/**************************************************************************
 *									  *
 *		 Copyright (C) 1990-1997, Silicon Graphics, Inc		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#include "ml/ml.h"

#ifdef SPLDEBUG

/* SPLDEBUG is mutually exclusive with SPLMETER */
			
#define SPLLEAF(x,y,x1,x2)			\
	LEAF(x)			; 	\
	move	y, ra		;	\
	j	x1		; 	\
	END(x)			; 	\
	LEAF(x2)
	
#define SPLXLEAF(x,y,x1,x2)			\
	move	y, ra		;	\
	j	x1		; 	\
	END(x)			; 	\
	LEAF(x2)
	
#define	SPLEND(x,x2)	END(x2)

#else /* !SPLDEBUG */
	
#define SPLLEAF(x,y,x1,x2)	LEAF(x)
#define SPLXLEAF(x,y,x1,x2)	XLEAF(x)
#define SPLEND(x,x2)	END(x)		
#endif /* !SPLDEBUG */			
	
/*
 * spl.s - spl routines
 *
 *
 * Among the special cases:
 *
 *   IP20/IP22/IP28 - there is no floating point interrupt.
 *	the order of interrupts is not the same as for other machines and it
 *	has two sets of local interrupts.  buserror interrupt comes in on 7 and
 *	the count/compare interrupt (currently unused) comes in on 8.
 *
 *   IP24 - same as IP20/IP22 except that 5/6 (PTC) are not used and
 *      8 (R4000 timer) is.
 *
 *   IP26 - similar to IP22 on the lower half, but TFP has 11 IP levels.
 *
 * $Revision: 1.67 $
 *
 */

/*
 * These definitions should go into a per platform header file
 * when I have time to do some more cleaning up.
 */
#if IP20 || IP22 || IP28
#define SR_HI_MASK	SR_IMASK5
#define SR_PROF_MASK	SR_IBIT7
#define SR_ALL_MASK	SR_IMASK8
#define SR_IBIT_HI	SR_IBIT5
#define SR_IBIT_PROF	SR_IBIT6
#endif

#if IP30
#define SR_IBIT_HI	SR_IBIT5
#endif

/*
 * SPLMETER
 */
#ifdef SPLMETER

#define SPLHI		5		/* spl6(), splhi() */
#define SPLPROF		6		/* spl65(), splprof() */
#ifdef IP32				/* Avoid naming conflicts on IP32 */
#define SPL7_IP32	7		/* spl7() */
#else
#define SPL7		7		/* spl7() */
#endif  /* IP32 */

#define SPLMETER_PROLOGUE(name)						;\
	move	a0,ra		/* save return for raisespl() */	;\
XLEAF(name)


/* assume noreorder, ospl = v0 */
#define SPLMETER_WORK(level)						;\
	mtc0	v1,C0_SR						;\
	INT_L	a1,spl_level						;\
	bne	a1,level,2f	/* needed spl level is not the same */	;\
	nop								;\
	j	raisespl	/* return from there to our caller */	;\
	move	a1,v0							;\
2:									;\
	j	ra							;\
	nop

#ifdef NOTDEF
LEAF(splstr)
	move	a0,ra
	.set	noreorder
	j	_splstr
	nop
	.set	reorder
	END(splstr)
#endif

/*
 * starttimer: start timer running to fig out how long this routine take
 * to execute until hit stoptimer()
 *
 */
LEAF(starttimer)
	move	a0,ra
	.set	noreorder
	j	_starttimer
	nop
	.set	reorder
	END(starttimer)

LEAF(stoptimer)
	move	a0,ra
	.set	noreorder
	j	_stoptimer
	nop
	.set	reorder
	END(stoptimer)

LEAF(canceltimer)
	move	a0,ra
	.set	noreorder
	j	_canceltimer
	nop
	.set	reorder
	END(canceltimer)

LEAF(_cancelsplhi)
	move	a0,ra
	move	a1,sp
	j	cancelsplhi
	END(_cancelsplhi)

#else
#define SPLMETER_PROLOGUE(lvl)
#define SPLMETER_WORK(level)						;\
	j	ra							;\
	mtc0	v1,C0_SR
#endif /* SPLMETER */

/*
 * check splx arg
 */
#ifndef STKDEBUG
#define SPLX_CK(tmp)	# not turned on
#else
#define SPLX_MASK  0xCF000080	/* zeros and no coproc 2 or 3 */

#define SPLX_CK(tmp)						;\
	and	tmp,a0,SPLX_MASK				;\
	beq	tmp,zero,1f					;\
	move	a2,ra						;\
	move	a3,a0						;\
	li	a0,CE_PANIC					;\
	la	a1,splx_msg					;\
	/* jump to get a good stack frame */			;\
	j	cmn_err						;\
1:

splx_msg:MSG("splx:ra: %x bad arg:%x")
#endif


/*
 * disable some interrupts, but without "lowering the level"
 *	r0=current SR contents, r1=place for new value, possibly r0=r1
 */
#define DIS_INT(r1,r0,lvl)				;\
	or	r1,r0, SR_IEC | (SR_IMASK & ~(lvl))	;\
	xor	r1, (SR_IMASK & ~(lvl))



#if defined(SPLDEBUG) || defined(SPLDEBUG_CPU_EVENTS)
LEAF(spldebug_log_event)
	move	a1, ra
	j	_spldebug_log_event
	END(spldebug_log_event)
#endif			


#if IP20 || IP22 || IP27 || IP28 || IP30 || IPMHSIM || IP33
/* IP20/IP22/IP28 spls
 *
 * The IP20/IP22/IP28 have two sets of local interrupts: high and low priority.
 *
 * Interrupts:
 * 8		R4000 internal timer (only used on IP24)
 * 7		bus error/timeout
 * 6		profiling clock. (timer channel 1)
 * 5		sched clock. (timer channel 0)
 * 4		high priority vectored devices
 * 3		low priority vectored devices
 * 2		softnet
 * 1		softclock
 */

/* base retained bits in the status register */
#ifdef R10000
#define SR_RETAIN	(SR_KX|SR_UX|SR_DE|SR_ERL)
#else
#define SR_RETAIN	(SR_KX|SR_DE|SR_ERL)
#endif

/*
 * all of the R4000 spl routines must preserve the SR_DE, SR_ERL and SR_KX bits
 * if they are already set */
/*
 * spl0: Don't block against anything, and ignore the previous state.
 *	lowers level if asked
 */
SPLLEAF(spl0,a0,_spl0,__spl0)
#ifdef SPLMETER
	move	a0,ra
	move	a1,sp
	j	_spl0
	END	(spl0)
LEAF(__spl0)
#endif /* SPLMETER */
	.set	noreorder
	mfc0	v0,C0_SR
	li	v1,SR_RETAIN		# Retain base important bits
	and	v1,v0
	or	v1,SR_IE|SR_IMASK0
	j	ra
	mtc0	v1,C0_SR		# BDSLOT
	.set 	reorder
#ifdef SPLMETER
	END(__spl0)
#else
	SPLEND(spl0,__spl0)
#endif

/*
 * spl6: block against sched clock interrupts. (level 5 hardint).
 * never lowers spl
 */
LEAF(splhi)
XLEAF(spl6)
XLEAF(splhi_relnet)
XLEAF(splhintr)
XLEAF(splgio0)
XLEAF(splgio1)
XLEAF(splgio2)
XLEAF(spl1)
XLEAF(spl3)
XLEAF(spl5)
XLEAF(splimp)
XLEAF(splnet)
XLEAF(spltty)
XLEAF(splvme)
XLEAF(splretr)
SPLXLEAF(splhi,a0,_splhi,__splhi)	
	SPLMETER_PROLOGUE(_splhi)
	.set 	noreorder
	mfc0	v0,C0_SR
	li	v1,SR_RETAIN|SR_HI_MASK|SR_IE	#  Retain base + intr mask
	and	v1,v0
	SPLMETER_WORK(SPLHI)
	.set 	reorder
	SPLEND(splhi,__splhi)

#ifdef SPLMETER
LEAF(splhi_nosplmeter)
	.set 	noreorder
	mfc0	v0,C0_SR
	li	v1,SR_RETAIN|SR_HI_MASK|SR_IE	#  Retain base + intr mask
	and	v1,v0
	j	ra
	mtc0	v1,C0_SR
	.set 	reorder
	END(splhi_nosplmeter)
#endif

/* a0 has processor status register
 * return 1 if at splhi or higher,
 * or if IE is low, or if EXL is high.
 */
LEAF(issplhi)
	move	v0,zero			# default is not @ splhi
	li	v1,SR_IE
	and	a1,a0,SR_IE|SR_EXL
	bne	a1,v1,2f		# disabled or at exception level

	and	a0,SR_IBIT_HI
	bnez	a0,3f
2:	li	v0,1			# SR_IBIT_HI bit(s) clear
3:
	j	ra
	END(issplhi)

/* a0 has the processor status register
 * return 1 if at splprof or higher,
 * or if IE is low, or if EXL is high.
 */
LEAF(issplprof)
	move	v0,zero			# default not @ splprof
	li	v1,SR_IE
	and	a1,a0,SR_IE|SR_EXL
	bne	a1,v1,2f		# disbled or at exception level

	andi	a0,SR_IBIT_PROF
	bnez	a0,3f			# re SR_IBIT_PROF bits clear?
2:	li	v0,1
3:
	j	ra
	END(issplprof)

/*
** a0 has processor status register
** return 1 if at spl7 or higher
*/
LEAF(isspl7)
	li	v0,0	
	and	v1,a0,SR_IE
	bne	v1,zero,1f	/* its enabled */
	li	v0,1
	b	2f
1:
	and	a0,SR_IMASK
	bne	a0,zero,2f
	li	v0,1
2:
	j	ra
	END(isspl7)

/*
 * splprof: block against all interrupts except bus-error.
 * never lowers spl
 */
SPLLEAF(splprof,a0,_splprof,__splprof)	
XLEAF(spl65)
	SPLMETER_PROLOGUE(_splprof)
	.set 	noreorder
	mfc0	v0,C0_SR
	li	v1,SR_RETAIN|SR_PROF_MASK|SR_IE	# Retain base + intr mask
	and	v1,v0
	SPLMETER_WORK(SPLPROF)
	.set 	reorder
	SPLEND(splprof,__splprof)

/*
 * spl7: block against all interrupts.
 */
SPLLEAF(spl7,a0,_spl7,__spl7)
XLEAF(splerr)
	SPLMETER_PROLOGUE(_spl7)
	.set 	noreorder
	mfc0	v0,C0_SR
	li	v1,SR_RETAIN|SR_ALL_MASK|SR_IE	# Retain base + intr mask
	and	v1,v0
	SPLMETER_WORK(SPL7)
	.set 	reorder
	SPLEND(spl7,__spl7)

/*
 * splecc: block against all interrupts and disable ecc exceptions.
 */
LEAF(splecc)
	.set 	noreorder
	mfc0	v0,C0_SR
	li	v1,SR_UX|SR_KX|SR_ERL		# Keep these bits
	and	v1,v0
	li	t0,SR_DE|SR_ALL_MASK|SR_IE
	or	v1,t0
	j	ra
	mtc0	v1,C0_SR			# BDSLOT
	.set 	reorder
	END(splecc)

/*
 * splx(ipl) -- restore previously saved ipl
 */
SPLLEAF(splx,a1,_splx,__splx)
XLEAF(splxecc)
#ifdef SPLMETER
	move	a1,ra
	move	a2,sp
	j	lowerspl
	END	(splx)
LEAF(__splx)
XLEAF(__splxecc)
#endif /* SPLMETER */
#ifdef STKDEBUG
	li	v0,0xC0000000	# zeros and no coproc 2 or 3
	and	v0,v0,a0
	beq	v0,zero,1f
	move	a2,ra
	move	a3,a0
	PANIC("splx:ra: %x bad arg:%x");
1:
#endif
	.set	noreorder
	mfc0	v0,C0_SR
	li	v1,SR_RETAIN		# Retain base important bits
	and	v1,v0
	or	a0,v1
	j	ra
	mtc0	a0,C0_SR		# BDSLOT
	.set	reorder
#ifdef SPLMETER
	END(__splx)
#else
	SPLEND(splx,__splx)
#endif

#endif /* IP20 || IP22 || IP27 || IP28 || IP30 || IP33 */

#if IP32
/* IP32 spls
 *
 * The IP32 has one set of external interrupt sources
 *
 * Interrupts:
 * 8		R4000 internal timer (used on IP32)
 * 7		N/A	
 * 6		N/A
 * 5		N/A
 * 4		N/A
 * 3		CRIME interrupts
 * 2		softnet
 * 1		softclock
 */

/*
 * Table of SR IMASK values for various IPL levels
 */
	.data
sr_iplmasks:
	.dword	SR_IMASK0
	.dword	SR_IMASK5
	.dword	SR_IMASK6
	.dword	SR_IMASK8
	.dword	SR_IMASK8
	.dword	SR_IMASK8	/* SPLMAX = 5 */

	.text

/*
 * SPLMETER code for case where we're already >= splhi
 * Don't write back to CO_SR
 */
#ifdef SPLMETER
/* assume noreorder, ospl = v0 */
#define NOCHANGE_SPLMETER_WORK(level)					;\
	INT_L	a1,spl_level						;\
	bne	a1,level,4f	/* needed spl level is not the same */	;\
	nop								;\
	j	raisespl	/* return from there to our caller */	;\
	move	a1,v0							;\
4:									;\
	j	ra							;\
	nop
#else
#define NOCHANGE_SPLMETER_WORK(level)					;\
	j	ra							;\
	nop
#endif /* SPLMETER */

/*
 * all of the R4000 spl routines must preserve the SR_DE, SR_ERL and SR_KX bits
 * if they are already set */
/*
 * spl0: Don't block against anything, and ignore the previous state.
 *	lowers level if asked
 */
SPLLEAF(spl0,a0,_spl0,__spl0)
#ifdef SPLMETER
        move    a0,ra
        move    a1,sp
        j       _spl0
        END     (spl0)
LEAF(__spl0)
#endif /* SPLMETER */
	.set	noreorder
	mfc0	v0,C0_SR
	# retain the KX, DE and ERL bits
	li	v1,SR_KX|SR_DE|SR_ERL
	and	v1,v0
	ori	v1,SR_IE|SR_IMASK0
	xor	t2,v1,SR_IE		# disable ints for atomicity
	mtc0	t2,C0_SR
	LOAD_CRIME_INTMASK(zero,t0,t1)
	BUILD_RETURN_VALUE(v0,t0)
	sw	zero,VPDA_CURSPL(zero)
	j	ra
	mtc0	v1,C0_SR		# turn CRIME interrupts back on
	.set 	reorder
#ifdef SPLMETER
        END(__spl0)
#else
        SPLEND(spl0,__spl0)
#endif

	
/*
 * spl6: block against sched clock interrupts. (level 5 hardint).
 * never lowers spl
 */
LEAF(splhi)
XLEAF(spl6)
XLEAF(splhi_relnet)
XLEAF(splhintr)
XLEAF(splretr)
XLEAF(splgio2)
XLEAF(spl5)
XLEAF(splvme)
XLEAF(spltty)
XLEAF(splimp)
XLEAF(splgio0)
XLEAF(splgio1)
XLEAF(splnet)
XLEAF(spl3)
XLEAF(spl1)
SPLXLEAF(splhi,a0,_splhi,__splhi)
        SPLMETER_PROLOGUE(_splhi)
	.set 	noreorder
	mfc0	v0,C0_SR
	li	t1,1<<3
	lw	t0,VPDA_CURSPL(zero)
	ble	t1,t0,3f		# are we at a higher level?

	# Retain the DE and ERL bits
	li	v1,SR_KX|SR_DE|SR_IMASK5|SR_ERL|SR_IE	
	and	v1,v0
	and	t2,v0,~SR_IE
	mtc0	t2,C0_SR
	LOAD_CRIME_INTMASK(t1,t0,t2)
	BUILD_RETURN_VALUE(v0,t0)
	sw	t1,VPDA_CURSPL(zero)
	SPLMETER_WORK(SPLHI)

3:	BUILD_RETURN_VALUE(v0,t0)
	NOCHANGE_SPLMETER_WORK(SPLHI)
	.set 	reorder
	SPLEND(splhi,__splhi)

#ifdef SPLMETER
LEAF(splhi_nosplmeter)
        .set 	noreorder
	mfc0	v0,C0_SR
	li	t1,1<<3
	lw	t0,VPDA_CURSPL(zero)
	ble	t1,t0,2f		# are we at a higher level?

	# Retain the DE and ERL bits
	li	v1,SR_KX|SR_DE|SR_IMASK5|SR_ERL|SR_IE	
	and	v1,v0
	and	t2,v0,~SR_IE
	mtc0	t2,C0_SR
	LOAD_CRIME_INTMASK(t1,t0,t2)
	BUILD_RETURN_VALUE(v0,t0)
	sw	t1,VPDA_CURSPL(zero)
	j 	ra
	mtc0	v1,C0_SR

2:	BUILD_RETURN_VALUE(v0,t0)
	j	ra
	nop
	.set 	reorder
        END(splhi_nosplmeter)
#endif


/*
** a0 has processor status register
** return 1 if at splhi or higher
*/
LEAF(issplhi)
	li	v0,0	
	and	v1,a0,SR_IE|SR_EXL
	li	a0,SR_IE
	beq	v1,a0,1f	/* its enabled */
	li	v0,1
	b	2f
1:
	lw	v1,VPDA_CURSPL(zero)
	blt	v1,1<<3,2f
	li	v0,1
2:
	j	ra
	END(issplhi)

/*
 * a0 has the processor status register
 * return 1 if at splprof or higher
 */
LEAF(issplprof)
	li	v0,0	
	and	v1,a0,SR_IE|SR_EXL
	li	a0,SR_IE
	beq	v1,a0,1f	/* its enabled */
	li	v0,1
	b	2f
1:
	lw	v1,VPDA_CURSPL(zero)
	blt	v1,2<<3,2f
	li	v0,1
2:
	j	ra
	END(issplprof)

/*
 * spl65: block against all interrupts except bus-error.
 * never lowers spl
 * Count-compare, which interrupts at level 8, is disabled.
 */
SPLLEAF(splprof,a0,_splprof,__splprof)
XLEAF(spl65)
	SPLMETER_PROLOGUE(_splprof)
	.set 	noreorder
	mfc0	v0,C0_SR
	li	t1,2<<3
	lw	t0,VPDA_CURSPL(zero)
	ble	t1,t0,3f		# are we at a higher level?

	# Retain the DE and ERL bits
	li	v1,SR_KX|SR_DE|SR_IMASK6|SR_ERL|SR_IE	
	and	v1,v0
	and	t2,v0,~SR_IE
	mtc0	t2,C0_SR
	LOAD_CRIME_INTMASK(t1,t0,t2)
	BUILD_RETURN_VALUE(v0,t0)
	sw	t1,VPDA_CURSPL(zero)
	SPLMETER_WORK(SPLPROF)

3:	BUILD_RETURN_VALUE(v0,t0)
	NOCHANGE_SPLMETER_WORK(SPLPROF)
	.set 	reorder
	SPLEND(splprof,__splprof)

/*
 * spl7: block against all interrupts.
 */
SPLLEAF(spl7,a0,_spl7,__spl7)
        SPLMETER_PROLOGUE(_spl7)
	.set 	noreorder
	mfc0	v0,C0_SR
	li	t1,3<<3
	lw	t0,VPDA_CURSPL(zero)
	ble	t1,t0,3f		# are we at a higher level?

	# Retain the DE and ERL bits
	li	v1,SR_KX|SR_DE|SR_IMASK8|SR_ERL|SR_IE	
	and	v1,v0
	and	t2,v0,~SR_IE
	mtc0	t2,C0_SR	
	LOAD_CRIME_INTMASK(t1,t0,t2)
	BUILD_RETURN_VALUE(v0,t0)
	sw	t1,VPDA_CURSPL(zero)
	SPLMETER_WORK(SPL7_IP32)

3:	BUILD_RETURN_VALUE(v0,t0)
	NOCHANGE_SPLMETER_WORK(SPL7_IP32)
	.set 	reorder
	SPLEND(spl7,__spl7)

/*
 * splecc: block against all interrupts and disable ecc exceptions.
 */
LEAF(splecc)
	.set 	noreorder
	mfc0	v0,C0_SR
	li	v1,SR_ERL
	and	v1,v0
	.set	noat
	li	AT,SR_KX|SR_DE|SR_IMASK8|SR_IE
	or	v1,AT
	.set	at
	li	t0,4<<3
	BUILD_RETURN_VALUE(v0,t0)
	sw	t0,VPDA_CURSPL(zero)
	j	ra
	mtc0	v1,C0_SR
	.set 	reorder
	END(splecc)

/*
 * splx(ipl) -- restore previously saved ipl
 */
SPLLEAF(splx,a1,_splx,__splx)
XLEAF(splxecc)
#ifdef SPLMETER
        move    a1,ra
        move    a2,sp
        j       lowerspl
        END     (splx)
LEAF(__splx)
XLEAF(__splxecc)
#endif /* SPLMETER */
#ifdef STKDEBUG
	li	v0,0xC0000000	# zeros and no coproc 2 or 3
	and	v0,v0,a0
	beq	v0,zero,1f
	move	a2,ra
	move	a3,a0
	PANIC("splx:ra: %x bad arg:%x");
1:
#endif
	.set	noreorder
	mfc0	v0,C0_SR
	li	v1,SR_KX|SR_DE|SR_ERL		# Retain the DE and ERL bits
	and	v1,v0
	BUILD_SR_IMASK(a0,t0,t1)
	or	a0,v1
	and	t2,a0,~SR_IE
	mtc0	t2,C0_SR
	LOAD_CRIME_INTMASK(t0,t1,t2)
	sw	t0,VPDA_CURSPL(zero)
	j	ra
	mtc0	a0,C0_SR           # in BDSLOT
	.set	reorder
#ifdef SPLMETER
	END(__splx)
#else
	SPLEND(splx,__splx)
#endif /* SPLMETER */


/*
 * splint(ipl) -- restore previously saved ipl; set IEC
 */
LEAF(splint)
#ifdef STKDEBUG
	li	v0,0xC0000000	# zeros and no coproc 2 or 3
	and	v0,v0,a0
	beq	v0,zero,1f
	move	a2,ra
	move	a3,a0
	PANIC("splint:ra: %x bad arg:%x");
1:
#endif
	.set	noreorder
	mfc0	v0,C0_SR
	li	v1,SR_KX|SR_DE|SR_ERL		# Retain the DE and ERL bits
	and	v1,v0
	ori	v1,SR_IE
	LOAD_CRIME_INTMASK(a0,t0,t2)
	BUILD_RETURN_VALUE(v0,t0)
	sw	a0,VPDA_CURSPL(zero)
	j	ra
	mtc0	v1,C0_SR           # in BDSLOT
	.set	reorder
	END(splint)
#endif /* IP32 */

#if IP26
/* IP26 spls
 *
 * The IP26 has two sets of local interrupts: high and low priority.
 *
 * Interrupts:
 *
 * 11		TFP internal timer (not really used).
 * 10		Odd  bank G-Cache parity error
 * 9		Even bank G-Cache parity error
 * 8		ECC error reporting
 * 7		Bus error/timeout from MC or TCC mach check.  Other TCC
 *		and TFP bus errors reported thru SR_BE.
 * 6		profiling clock. (timer channel 1)
 * 5		sched clock (timer channel 0) and TCC count/compare timer
 * 4		high priority vectored devices
 * 3		low priority vectored devices
 * 2		softnet
 * 1		softclock
 *
 *  For some baseboards in some conditions we need to not enable IP8 since
 * original boards with a 33Mhz GIO the hardware sometimes, until it basically
 * kills the machine.
 */

/*
 * spl0: Don't block against anything, and ignore the previous state.
 */
LEAF(spl0)
	.set	noreorder
	dmfc0	a0,C0_WORK1			# local sr_mask
	ssnop
	dmfc0	v0,C0_SR
	LI	v1,SR_DEFAULT|SR_KERN_USRKEEP
	and	v1,v0
	or	v1,SR_IE|SR_IMASK0
	and	v1,a0
	dmtc0	v1,C0_SR
	ssnop
	ssnop
	j	ra
	nada
	.set 	reorder
	END(spl0)

/*
 * spl6: block against sched clock interrupts. (level 5 hardint).
 *
 * Never lowers spl
 */
LEAF(spl6)
XLEAF(splhi)
XLEAF(splhi_relnet)
XLEAF(spl1)
XLEAF(spl3)
XLEAF(spl5)
XLEAF(splvme)
XLEAF(splnet)
XLEAF(spltty)
XLEAF(splimp)
XLEAF(splgio0)
XLEAF(splgio1)
XLEAF(splgio2)
XLEAF(splhintr)
XLEAF(splretr)
	.set 	noreorder
	dmfc0	a0,C0_WORK1			# local sr_mask
	ssnop
	dmfc0	v0,C0_SR
	LI	t1,SR_DEFAULT|SR_KERN_USRKEEP
	and	t1,v0
	or	t1,SR_IE|SR_IMASK5
	and	t0,v0,SR_IBIT5		# SR_IBIT5 off -> SR_IMASK5 or higher
	and	t1,a0
	movz	t1,v0,t0		# use current SR if higher already
	dmtc0	t1,C0_SR
	ssnop
	ssnop
	j	ra
	nada
	.set 	reorder
	END(spl6)
/*
 * issplhi: a0 has processor status register -- return 1 if at splhi or higher.
 */
LEAF(issplhi)
	move	v0,zero
	li	v1,1

	and	a0,(SR_IE|SR_IBIT5)	# IBIT5 off -> SR_IMASK5 or higher
	xor	a0,(SR_IE|SR_IBIT5)	# SR_IE off -> all ints disabled
	movn	v0,v1,a0		# set rc=1 if true
	j	ra
	END(issplhi)

/*
 * issplprof: a0 has processor status register -- return 1 if at splprof or
 *	      higher.
 */
LEAF(issplprof)
	move	v0,zero
	li	v1,1

	and	a0,(SR_IE|SR_EXL|SR_IBIT6)	# IBIT6 off -> SR_PROF or higher
	xor	a0,(SR_IE|SR_IBIT6)		# SR_IE off -> all ints off
						# SR_EXL on -> all ints off
	movn	v0,v1,a0			# set rc=1 if true
	j	ra
	END(issplprof)

/*
 * spl65: block against all interrupts except external bus-errors and
 *        G cache errors.
 *
 * Never lowers spl.
 */
LEAF(spl65)
XLEAF(splprof)
	.set 	noreorder
	dmfc0	a0,C0_WORK1			# local sr_mask
	ssnop
	dmfc0	v0,C0_SR
	LI	t1,SR_DEFAULT|SR_KERN_USRKEEP
	and	t1,v0
	or	t1,SR_IE|SR_IBIT7|SR_IBIT8|SR_IBIT9|SR_IBIT10
	and	t0,v0,SR_IBIT6		# SR_IBIT6 off -> spl65 or higher
	and	t1,a0
	movz	t1,v0,t0		# use current SR if higher already
	dmtc0	t1,C0_SR
	ssnop
	ssnop
	j	ra
	nada
	.set 	reorder

	END(spl65)

/*
 * spl7: block against all interrupts except GCache parity errors (fatal).
 */
LEAF(spl7)
	.set 	noreorder
	dmfc0	v0,C0_SR
	LI	v1,SR_DEFAULT|SR_KERN_USRKEEP
	and	v1,v0
	or	v1,SR_IE|SR_IBIT9|SR_IBIT10	/* o+e GCache */
	dmtc0	v1,C0_SR
	ssnop
	ssnop
	j	ra
	nada
	.set 	reorder
	END(spl7)

/*
 * splx(ipl) -- restore previously saved ipl
 */
LEAF(splx)
#ifdef STKDEBUG
	LI	v0,0xfffffe00c000800c	# reserve bits all off
	and	v0,v0,a0
	beq	v0,zero,1f
	move	a2,ra
	move	a3,a0
	PANIC("splx:ra: %x bad arg:%x");
1:
#endif
	.set	noreorder
	dmfc0	a1,C0_WORK1			# local sr_mask
	ssnop
	dmfc0	v0,C0_SR
	LI	v1,SR_DEFAULT|SR_KERN_USRKEEP
	and	v1,v0
	or	a0,v1
	and	a0,a1
	dmtc0	a0,C0_SR
	ssnop
	ssnop
	j	ra
	nada
	.set	reorder
	END(splx)
#endif /* IP26 */

#if DEBUG
/*
 * The one-second code in os/clock.c lowers the currently spl based on
 * examination of the status register at the time of interrupt.  If the
 * status register indicates that all interrupts were previously enabled,
 * it does the one second stuff, otherwise it waits for the next clock tick.
 *
 * Since all spls are now spl or higher, we only now have to check that
 * we were @ spl0.  However, some kernel code disables SR_IE, but can
 * get an interrupt before the mtc0 bubbles through the pipe, and end
 * up with SR_IMASK0 ok but SR_IE=0.  Hence we do not check SR_IE, which
 * is good enough for the clock code.
 *
 * return true if it is okay to lower the spl
 *	a0 - EP at time of interrupt
 */
LEAF(loclkok)
	lreg	a0,EF_SR(a0)
#ifdef IP26
	.set noreorder
	dmfc0	v0,C0_WORK1
	li	v1,SR_IMASK0
	.set reorder
	and	v1,v0
#else
	li	v1,SR_IMASK0		/* For IP32 IMASK0 != IMASK */
#endif
	and	a0,v1
	bne	a0,v1,1f
	li	v0,1
	j	ra

1:
	li	v0,0
	j	ra
	END(loclkok)
#endif

LEAF(isspl0)
#ifdef IP26
	.set noreorder
	dmfc0	v0,C0_WORK1
	li	v1,SR_IMASK|SR_IE
	.set reorder
	and	v1,v0
#else
	li	v1,SR_IMASK0|SR_IE	/* For IP32 IMASK0 != IMASK */
#endif
	and	a0,v1
	bne	a0,v1,1f
	li	v0,1
	j	ra

1:
	li	v0,0
	j	ra
	END(isspl0)

#if !(IP26 || IP27 || IP28 || IP30 || IP33)
/*
 * For compatibility with Everest systems.
 */
LEAF(allowintrs)
	j	ra
	END(allowintrs)
#endif
	
/* delay_for_intr()
 * Routine which can be invoked to allow any pending interrupts to occur
 * before we raise spl level again.
 * Typical use is in long computation where code wishs to drop spl then
 * immediately raise it again, but need to allow pending interrupts to
 * occur.	
 */
LEAF(delay_for_intr)
#if IP32
	j	__sysad_wbflush
#else
	.set	noreorder
	j	ra
	nop
	.set	reorder
#endif
	END(delay_for_intr)
