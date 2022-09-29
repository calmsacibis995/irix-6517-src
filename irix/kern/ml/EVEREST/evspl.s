/**************************************************************************
 *									  *
 *		 Copyright (C) 1992-1994, Silicon Graphics, Inc.	  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#include <ml/ml.h>
#include <sys/EVEREST/everest.h>
#include <sys/EVEREST/evintr.h>

#define	OWNER_NONE -1		/* XXX sys/splock.h */

#ident	"$Revision: 1.59 $"

#ifdef SPLDEBUG
	
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
 * SPLMETER
 */
#ifdef SPLMETER
#define SPL1	1		/* spl1() */
#define SPLNET	2		/* splnet() */
#define SPLVME	3		/* splvme() */
#define SPLTTY	4		/* spl5(), spltty(), splimp() */
#define SPLHI	5		/* spl6(), splhi() */
#define SPLPROF	6		/* spl65(), splprof() */
#define SPL7	7		/* spl7() */
#define SPLMETER_PROLOGUE(name)						;\
	move	a0,ra		/* save return for raisespl() */	;\
XLEAF(name)


/* assume noreorder, ospl = reg1|reg2 */
#define SPLMETER_WORK(level,reg1,reg2)					;\
	INT_L	a1,spl_level						;\
	nop								;\
	bne	a1,level,2f	/* needed spl level is not the same */	;\
	or	v0,reg1,reg2						;\
	j	raisespl	/* return from there to our caller */	;\
	move	a1,v0							;\
2:									;\
	j	ra							;\
	nop

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
#define SPLMETER_WORK(level,reg1,reg2)					;\
	j	ra							;\
	or	v0,reg1,reg2
#endif /* SPLMETER */

#if defined(SPLDEBUG) || defined(SPLDEBUG_CPU_EVENTS)
LEAF(spldebug_log_event)
	move	a1, ra
	j	_spldebug_log_event
	END(spldebug_log_event)
#endif			

/*
 * spl.s - spl routines
 *
 * Everest spl mappings:
 * The following levels hold for R10000 too.
 *	R4000 level 2	-	all device interrupts
 *	R4000 level 3	-	real-time clock interrupt
 *	R4000 level 4	-	CC UART interrupt (used by sysctlr)
 *	R4000 level 5	-	Everest error interrupts
 *	R4000 level 6	-	write gatherer timeout
 *	R4000 level 7	-	sched clock interrupt
 *
 * spl0 	enables all
 *
 * spl1		disable rt clock interrupts (softclock)
 *
 * splnet
 * spl2		disable net interrupts (softnet)
 *
 * splvme
 * spl3 	disable all low priority device interrupts
 *
 * spltty
 * spllintr
 * spl5 	disables all high priority device interrupts
 *
 * splimp	same as spl5, but grab splimp lock too
 *
 * splhi
 * spl6 	disables sched clock and interprocessor interrupts.
 *		leaves the prof clock interrupt and error intrs enabled
 *
 * splhi_relnet	same as splhi, but release net spin locks
 *
 * splprof
 * spl65 	disables all device and clock interrupts, but leaves
 *		error interrupts and write gatherer timeout enabled.
 *
 * spl7		disables all interrupts, including error interrupts and
 *		write gatherer timeout interrupt
 *
 *
 * The value returned from any of these routines is a 32-bit
 * quantity with this interpretation:
 *	bits 31..23	- unused
 *	bits 22..20	- MP networking bits
 *	bits 19..19	- unused
 * 	bits 18..8	- Old SR interrupt mask (only TFP uses 18..16)
 *	bits  7..7	- 1 for ``try'' spin locks to ensure non-zero success
 *			  return values
 * 	bits  6..0	- Old CEL level
 */

/*
 * spl6: block against sched clock interrupts, and inter-processor intrs.
 * Never lowers spl level.
 */
LEAF(spl6)
XLEAF(spl1)
XLEAF(spl3)
XLEAF(spl5)
XLEAF(splvme)
XLEAF(splnet)
XLEAF(spltty)
XLEAF(splimp)
XLEAF(splhi_relnet)
XLEAF(splhi)
SPLXLEAF(spl6,a0,_spl6,__spl6)
	SPLMETER_PROLOGUE(_splhi)
#if TFP
	.set	noreorder
	/* Make SURE we don't execute the dmfc0 in same cycle as jalr that
	 * got us here (and make sure we never branch here).
	 */
	li	t2,EVINTR_LEVEL_HIGH+1
	lbu	t0,VPDA_CELSHADOW(zero)	/* t0 = current CEL */
	dmfc0	v0,C0_SR		# see warning above
	LI	ta0, SRB_SWTIMO|SRB_NET|SRB_TIMOCLK|SRB_UART|SRB_SCHEDCLK
	li	t3,SR_IMASK		
	or	t1,v0,ta0
	and	v0,t3

	bge	t0, t2, 1f	/* don't lower spl */
	xor	t1,ta0		/* BDSLOT */

	RAISE_CEL(t2,v1)
1:
	dmtc0	t1,C0_SR
	SPLMETER_WORK(SPLHI,v0,t0)
#else /* R4000 || R10000 */
	.set	noreorder
	MFC0_NO_HAZ(v0,C0_SR)
	li	t2,EVINTR_LEVEL_HIGH+1	# guarantee 4 cycles between MFC0
	li	t3,SR_IMASK		#  and use of v0
	lbu	t0,VPDA_CELSHADOW(zero)	/* t0 = current CEL */
	or	t1,v0,SRB_SWTIMO|SRB_NET|SRB_TIMOCLK|SRB_UART|SRB_SCHEDCLK
	and	v0,t3
	.set	reorder
	# Following instruction is a macro for TFP.  For R4000 the reorder
	# will place the single instruction in the BDSLOT
	xor	t1,SRB_SWTIMO|SRB_NET|SRB_TIMOCLK|SRB_UART|SRB_SCHEDCLK # BDSLOT
	bge	t0, t2, 1f	/* don't lower spl */
	RAISE_CEL(t2,v1)
1:
	.set	noreorder
	MTC0_NO_HAZ(t1,C0_SR)
	SPLMETER_WORK(SPLHI,v0,t0)
#endif /* R4000 || R10000 */
	.set	reorder
	SPLEND(spl6,__spl6)

#if SABLE_RTL
LEAF(nospunlocksplx)
	move	a0,a1
	j	splx
	END(nospunlocksplx)
#endif /* SABLE_RTL */

/*
 * splx(ospl) -- Lower spl level down to previous level.
 * Returns void.
 */

SPLLEAF(splx,a1,_splx,__splx)
#ifdef SPLMETER
 	move	a1,ra
	move	a2,sp
	j	lowerspl
	END(splx)
LEAF(__splx)
#endif SPLMETER
	.set	noreorder
#if TFP	
	/* WARNING: Alter this instruction sequence carefully, as we require
	 * that the dmfc execute in a different cycle than the "jalr" which
	 * got us here.
	 */
	li	ta1,SR_IMASK		# ta1 = SR_IMASK ( > 16 bits)
	NOP_SSNOP			# force dmfc0 into next cycle
	dmfc0	v0, C0_SR		# get current SR

	and	t0,a0,ta1
	andi	a0,OSPL_CEL
	
	/* Disable interrupts so we can do "atomic update" of CEL and SR.
	 * Needed with the addition of kernel preemption. If we take an
	 * interrupt after updating one of the values, the "isspl0" check
	 * in vec_int() when returning will NOT recognize that we were
	 * transitioning to spl0 and will not preempt.  This enables the
	 * lower priority process to run until we enter the kernel again,
	 * probably on the next scheduling clock tick.
	 */

	or	ta0,v0,SR_IE
	xor	ta0,SR_IE		# current SR, without SR_IE
	dmtc0	ta0,C0_SR		# disable all ints by clearing IE

	/* Processor stalls, if needed, on results of "dmfc0 v0, C0_SR".
	 */
	or	v0,ta1
	xor	v0,ta1
	or	v0,t0			# insert arg0 IMASK values into SR
	LOWER_CEL(a0,ta0,v1)
	dmtc0	v0,C0_SR

	/* Following sequence guarentees to insert a cycle between the
	 * prededing "dmtc" and any subsequent "jal" that we may return
	 * to.  Note that new C0_SR may not be in effect until a superscaler
	 * cycle or two after we return.  Since this routine "lowers"
	 * interrupt priority, it shouldn't be a problem (besides, I
	 * understand that the WAIT_FOR_CEL is actually a many cycle
	 * operation, so we're probably OK).
	 */
	j	ra
	NOP_SSNOP
#else
	MFC0_NO_HAZ(v0,C0_SR)		# (BDSLOT) get current SR
1:
	/* this is the normal path */
	li	t3,SR_IMASK		# TFP mask > 16 bits
	and	t0,a0,t3
	andi	a0,OSPL_CEL

	or	ta0,v0,SR_IE
	xor	ta0,SR_IE		# current SR, without SR_IE
	MTC0_NO_HAZ(ta0,C0_SR)		# disable all ints, preserving SR_DE

	or	v0,t3
	xor	v0,t3
	or	v0,t0
	LOWER_CEL(a0,ta0,v1)
	MTC0_AND_JR(v0,C0_SR,ra)	# enable whatever we had & return
#endif
	/* no need to do NOP_0_4 here: we can take an interrupt anytime */
	.set	reorder
#if defined(SPLMETER)
	END(__splx)
#else	
	SPLEND(splx,__splx)
#endif


/*
 * splset(splvalue) -- sets spl to specified value, and enables interrupts.
 *		Under the current implementation, an splvalue is identical
 *		in format to the return value from splx.  splset should be
 *		used in very limited circumstances, and allows spl levels
 *		to be manipulated in an arbitrary manner.  For instance, it
 *		can be used to lower the current spl to a specified value.
 *		This routine explicitly turns ON the SR_IE bit.
 * Returns void.
 */
SPLLEAF(splset,a1,_splset,__splset)
	.set	noreorder
	MFC0_NO_HAZ(v0,C0_SR)		# get current SR
	li	t3,SR_IMASK		# TFP mask > 16 bits
	and	t0,a0,t3		# isolate SR_IMASK bits of new SR

	or	v0,t3			# disable interrupts around rewrite of
	xor	v0,t3			#  EV_CEL and final reset of SR, keep
	MTC0_NO_HAZ(v0,C0_SR)		#  current SR otherwise intact

#if TFP
	and	v0,SR_KERN_USRKEEP	# preserve "user" bits (i.e. DM)
	or	t0,v0
#endif
	andi	a0,OSPL_CEL		# isolate new CEL value
	or	v0,t0,SR_IE|SR_KERN_SET # form new SR: enable intrs
	nop
	LOWER_CEL(a0,v1,t3)
	MTC0_AND_JR(v0,C0_SR,ra)	# return and set SR
	/* no need to do NOP_0_4 here: we can take an interrupt anytime */
	.set	reorder
	SPLEND(splset,__splset)

/*
 * Takes SR as its only argument.  
 * Returns 0 if not at spl0
 *         1 if at spl0
 */
LEAF(isspl0)
	LI	v1,SR_IE|SR_IMASK
	xor	v0,a0,v1
	and	v0,v1
	bne	v0,zero,3f		# if IMASK or IE clear, not spl0
	LI	v1,EVINTR_LEVEL_BASE
	lbu	v0,VPDA_CELSHADOW(zero)
	bne	v0,v1,3f		# CEL must be EVINTR_LEVEL_BASE
	li	v0,1
	j	ra			# we are at spl0
3:
	li	v0,0
	j	ra
	END(isspl0)

/*
 * Takes SR as its only argument.  
 * Returns 0 if not at splhi
 *         1 if at splhi
 */
LEAF(issplhi)
	and	v1,a0,SR_IE
	# following instruction is a MACRO on TFP
	and	a0,SRB_SCHEDCLK		# check sched clock intr
	beq	v1,zero,1f		# intrs totally disabled
	.set	noreorder

	bne	a0,zero,2f
	nop

1:	# We ARE at splhi
	j	ra
	li	v0,1

2:	# We are NOT at splhi
	j	ra
	li	v0,0
	.set	reorder
	END(issplhi)

/*
 * Takes SR as its only argument.  
 * Always use current CEL.
 * Returns 1 if at splprof or higher
 *         0 otherwise
 */
LEAF(issplprof)
	and	v1,a0,SR_IE
	beq	v1,zero,1f		# interrupts totally disabled

	and 	v1,a0,SR_EXL
	bne	v1,zero,1f		# exception disables all interrupts

	and	v1,a0,SRB_SWTIMO|SRB_NET|SRB_TIMOCLK|SRB_UART|SRB_SCHEDCLK
	bne	v1,zero,2f		# not at splprof if any of above enabled

	and	v1,a0,SRB_DEV
	beq	v1,zero,1f		# all CEL interrupts disabled

	lbu	v0,VPDA_CELSHADOW(zero)
	li	v1,EVINTR_LEVEL_EPC_PROFTIM+1
	slt	t1, v0, v1
	bne	t1,zero,2f		# CEL < EVINTR_LEVEL_EPC_PROFTIM+1

1:	# We ARE at splprof or higher
	li	v0,1
	j	ra

2:	# We are NOT at splprof or higher
	li	v0,0
	j	ra

	END(issplprof)

/*
 * spl0: Enable all interrupts
 */
SPLLEAF(spl0,a0,_spl0,__spl0)
#ifdef SPLMETER
	move	a0,ra
	move	a1,sp
	j	_spl0
	END(spl0)
LEAF(__spl0)
#endif /* SPLMETER */
	.set	noreorder		
	MFC0_NO_HAZ(v0,C0_SR)
	li	t2,EVINTR_LEVEL_BASE
	li	t3,SR_IMASK		# TFP SR_IMASK > 16 bits
	lbu	t0,VPDA_CELSHADOW(zero)	/* t0 = current CEL */
	ori	t1,v0,SR_IE
	or	t1,t3

	/* Disable interrupts so we can do "atomic update" of CEL and SR.
	 * Needed with the addition of kernel preemption. If we take an
	 * interrupt after updating one of the values, the "isspl0" check
	 * in vec_int() when returning will NOT recognize that we were
	 * transitioning to spl0 and will not preempt.  This enables the
	 * lower priority process to run until we enter the kernel again,
	 * probably on the next scheduling clock tick.
	 */

	or	ta0,v0,SR_IE
	xor	ta0,SR_IE		# current SR, without SR_IE
	dmtc0	ta0,C0_SR		# disable all ints by clearing IE
	
	and	v0,t3
	LOWER_CEL(t2,ta0,t3)
	MTC0_NO_HAZ(t1,C0_SR)
	/* don't need NOPs: can take interrupt anytime now */
	.set	reorder
	or	v0,t0			# or in old CEL
	j	ra
#if defined(SPLMETER)
	END(__spl0)
#else
	SPLEND(spl0,__spl0)
#endif /* SPLMETER */

LEAF(loclkok)
	lreg	t0,EF_SR(a0)
	li	t1,SR_IMASK
	and	t0,t1
	bne	t0,t1,1f

	li	v0,1
	j	ra

1:
	li	v0,0
	j	ra
	END(loclkok)

/*
 * spl65: block against profiling clock
 */
LEAF(spl65)
XLEAF(splprof)
SPLXLEAF(spl65,a0,_spl65,__spl65)
	.set	noreorder
	MFC0_NO_HAZ(v0,C0_SR)
	li	t2,EVINTR_LEVEL_MAX
	li	t3,SR_IMASK
	lbu	t0,VPDA_CELSHADOW(zero)	/* t0 = current CEL */
	or	t1,v0,SRB_SWTIMO|SRB_NET|SRB_TIMOCLK|SRB_UART|SRB_SCHEDCLK
	and	v0,t3
	.set	reorder
	# Following instruction is a macro for TFP.  For R4000 the reorder
	# will place the single instruction in the BDSLOT
	xor	t1,SRB_SWTIMO|SRB_NET|SRB_TIMOCLK|SRB_UART|SRB_SCHEDCLK # BDSLOT
	bge	t0, t2, 1f		# don't lower spl
	RAISE_CEL(t2,v1)
1:
	.set	noreorder
	MTC0_NO_HAZ(t1,C0_SR)
	j	ra
	or	v0,t0
	.set	reorder
	SPLEND(spl65,__spl65)

/*
 * spl7: block against all interrupts
 */
SPLLEAF(spl7,a0,_spl7,__spl7)
	.set	noreorder
	MFC0_NO_HAZ(v0,C0_SR)
	li	t2,EVINTR_LEVEL_MAX
	li	t3,SR_IMASK
	lbu	t0,VPDA_CELSHADOW(zero)	/* t0 = current CEL */
	or	t1,v0,t3		# guarantee 4 cycles btwn mfc0 and use
	and	v0,t3
	xor	t1,t3
	/* set CEL high in case someone at spl7 calls spl5 .. */
	RAISE_CEL(t2,v1)
	MTC0_NO_HAZ(t1,C0_SR)
	j	ra			#  first instruction after exit
	or	v0,t0
	.set	reorder
	SPLEND(spl7,__spl7)

/*
 * splecc: block against *all* interrupts, and disable ecc exceptions
 * Even high priority everest error interrupts will be ignored.
 */
LEAF(splecc)
	.set	noreorder
	MFC0_NO_HAZ(v0,C0_SR)
	NOP_0_4
	LI	v1,EV_CEL
	ld	t0,0(v1)
	li	t3,SR_IMASK
#if R4000 || R10000
	or	t1,v0,SR_DE
#endif
	or	t1,t3
	and	v0,t3
	xor	t1,t3
	/* set CEL high in case someone at spl7 calls spl5 .. */
	li	t2,EVINTR_LEVEL_MAX
	FORCE_NEW_CEL(t2,v1)
	WAIT_FOR_CEL(v1)
	MTC0_NO_HAZ(t1,C0_SR)
	NOP_0_4
	j	ra
	or	v0,t0
	.set	reorder
	END(splecc)

/*
 * splxecc(ospl) -- Lower spl level down to previous level,
 * and enable ecc exceptions.  Returns void.
 * XXX SPLMETER
 */
LEAF(splxecc)
	.set	noreorder
	MFC0_NO_HAZ(v0,C0_SR)			# get current SR
	NOP_0_4
	li	t3,SR_IMASK
	and	t1,a0,t3
	andi	a0,OSPL_CEL
#if R4000 || R10000
	li	t0,~(SR_IMASK|SR_DE)
	li	t2,SR_KADDR
	MTC0_NO_HAZ(t2,C0_SR)			# Disable interrupts
#else	/* TFP */
	li	t0,~(SR_IMASK)
	MTC0_NO_HAZ(zero,C0_SR)			# Disable interrupts
#endif

	NOP_0_4
	and	v0,t0
	or	v0,t1
	nop
	nop

	LI	v1,EV_CEL
	FORCE_NEW_CEL(a0,v1)
	WAIT_FOR_CEL(v1)
	MTC0_NO_HAZ(v0,C0_SR)
	NOP_0_4
	j	ra
	nop
	.set	reorder
	END(splxecc)


/* int sr_ie_clear()
 * Intended for debug use only since completely disabling all interrupts
 * is a bad idea.
 */
LEAF(sr_ie_clear)
	.set	noreorder
	DMFC0(v0,C0_SR)
	NOP_0_1			# NOP for R4000 case
	ori	v1,v0,SR_IE
	xori	v1,SR_IE
	DMTC0(v1,C0_SR)
	j	ra
	NOP_NADA
	.set	reorder
	END(sr_ie_clear)

/* int sr_ie_restore(int IE)
 * Caller can invoked this routine with the return value from sr_ie_clear to
 * restore the state of the IE bit in the SR.
 */
LEAF(sr_ie_restore)
	.set	noreorder
	DMFC0(v0,C0_SR)
	NOP_0_1			# NOP for R4000 case
	ori	v1,v0,SR_IE
	xori	v1,SR_IE
	andi	a0,SR_IE
	or	v1,a0
	DMTC0(v1,C0_SR)
	j	ra
	NOP_NADA
	.set	reorder
	END(sr_ie_restore)
/* delay_for_intr()
 * Routine which can be invoked to allow any pending interrupts to occur
 * before we raise spl level again.
 * Typical use is in long computation where code wishs to drop spl then
 * immediately raise it again, but need to allow pending interrupts to
 * occur.	
 */
LEAF(delay_for_intr)
	.set	noreorder
	/* Make sure CEL register value is in full affect before continuing */
	 
	LA	a0,EV_CEL
	ld	a1,(a0)
	j	ra
	nop
	.set	reorder
	END(delay_for_intr)
	
#ifdef	PSEUDOCODE
/*
 * I got tired of unweaving assembler code.  Here are the relevant
 * spl routines hacked for MP networking in almost-C for IP5.
 * (IP19 will be different because spl level is encoded in S0_SR _and_ EV_CEL)
 */

splnet()
{
	osr = CLEAN_SR(get_SR());
	set_SR(SR_IMASK2);
	if (splnetlock_owner == cpuid()) {
#ifdef	DEBUG
		if (splimplock_owner == cpuid())
			cmn_err(CE_PANIC,
				"splnet: ra=%x already had implock, mask=%x,
				ra, osr);
		
#endif
		return osr;
	} else
		return netlock(osr);
}

splimp()
{
	osr = CLEAN_SR(get_SR());
	set_SR(DIS_INT(osr,SR_IMASK4));		/* set the new level */
	if (splimplock_owner == cpuid())	/* if we don't already... */
		return osr;
	else
		return implock(osr);		/* ...grab the lock */
}

splhi_relnet()
{
	osr = CLEAN_SR(get_SR());
	nsr = DIS_INT(osr, SR_IMASK6);
	if (splimplock_owner == cpuid()) {	/* 1st, release locks... */
		osr |= OSPL_IMPSPIN;			/* remember unlock */
		splimplock_owner = NONE;
		splimplock = 0;
	}
	if (splnetlock_owner == cpuid()) {
		osr |= OSPL_NETSPIN;			/* remember unlock */
		splnetlock_owner = NONE;
		splnetlock = 0;
	}
#if CHECK_NETSPIN && DEBUG
	return cknetsplx(osr, -3, nsr);
#else
	set_SR(nsr);				/* ...then, change level */
	return osr;
#endif
}

splx(ospl)
{
	netbits = ospl & OSPL_NETBITS;
	deferredbits = ospl ^ netbits;
	if (netbits) {
		if ( !(netbits & OSPL_NETSET)) {
			/*
			 * locks were released going into this spl section,
			 * now that we're leaving reacquire 
			 */
			nsr = dbits|SR_IMASK4|SR_IEC &
						 get_SR() & SR_IMASK|SR_IEC;
			/* 
			 * partial splx: leave splimp (and splnet!) masked
			 * until after we're done locking
			 */
			set_SR(nsr);
			return netsplx(ospl, netbits, deferredbits ^ nsr);
		}
		/*
		 * locks were set upon entering the spl section we're now
		 * leaving.  release on the way out.
		 */
		if (netbits & OSPL_IMPSPIN) {
			splimplock_owner = NONE;	/* order important */
			splimplock = 0;
		}
		if (netbits & OSPL_NETSPIN) {
			splnetlock_owner = NONE;
			splnetlock = 0;
		}
	}
#if CHECK_NETSPIN && DEBUG
	if (splnetlock_owner == cpuid())
		if (splnetlock != LOCK_TAKEN)
			cmn_err(CE_PANIC, "splx: bad lock with arg:%x",
								deferedbits);
	else
		cmn_err(CE_PANIC, "splx: bad bits in arg:%x", deferedbits);
	return chknetsplx(CLEAN_SR(get_SR()), ospl, deferredbits);
#else
	set_SR(deferredbits);
	return;
#endif
}

spl0()
{
	osr = CLEAN_SR(get_SR());
	if (osr & SR_IBIT2 == 0) {
		if (osr & SR_IBIT4 == 0) {
			if (splimplock_owner == cpuid()) {
				osr |= OSPL_IMPSPIN;
				splimplock_owner = NONE;
				splimplock = 0;
			}
		}
		if (splnetlock_owner = cpuid()) {
			osr |= OSPL_NETSPIN;
			splnetlock_owner = NONE;
			splnetlock = 0;
		}
	}
#if CHECK_NETSPIN && DEBUG
	return cknetsplx(osr, -1, SR_IEC|SR_IMASK0);
#else
	set_SR(SR_IEC|SR_IMASK0);
#endif
	return osr;
}

#if CHECK_NETSPIN && DEBUG
setnetpl(ospl, nspl)
{
	set_SR(nspl);
	return ospl;
}
#endif

/*
 * all other spl routines as normal except CLEAN_SR() is always
 * called on the return value to return OSPL_NETBITS zero'ed
 */
splZZZ()
{
	osr = CLEAN_SR(get_SR());	/* return OSPL_NETBITS zero'ed */

	set_SR( ...mask for ZZZ... );

	return osr;
}
#endif /* PSEUDOCODE */
