/* ------------------------------------------------------------------ */
/* | Copyright Unpublished, MIPS Computer Systems, Inc.  All Rights | */
/* | Reserved.  This software contains proprietary and confidential | */
/* | information of MIPS and its suppliers.  Use, disclosure or     | */
/* | reproduction is prohibited without the prior express written   | */
/* | consent of MIPS.                                               | */
/* ------------------------------------------------------------------ */
/* %Q% %M% %I% */
#ident "$Revision: 3.35 $"

/*
 * libc/src/csu/crt0.s -- runtime start-up for C executables
 */

#include "regdef.h"
#include "asm.h"

/*
 * External definitions
 */
#ifdef _LIBC_ABI
	.weakext environ _environ
	.globl	_environ
#endif
	.globl	__Argc
	.globl	__Argv
	.globl	__rld_obj_head
#ifdef _LIBC_ABI
	.globl atexit
	.globl _cleanup
#endif
	.globl exit 
	.text
#ifdef _LIBC_ABI
	.data
_environ:
	.word	0
#endif
	.comm	__Argc,4
#if (_MIPS_SZPTR == 32)
	.comm	__Argv,4
	.comm   __rld_obj_head,4
#endif
#if (_MIPS_SZPTR == 64)
	.comm	__Argv,8
	.comm   __rld_obj_head,8
#endif
/* __rld_obj_head is initialized by run-time-linker(rld).  
   __rld_obj_head is a pointer to a list of dso's currently active 
	in this process (the list is maintained by rld).
   0 means either:  
	not yet initialized (rld has not started up yet) 
	or there are no dso's used by the application (the
		application is fully statically linked).
	or update of this has not yet been implemented in rld.
   See <obj_list.h> and <obj.h> for the list structure.
   __rld_obj_head is used by debuggers to find the current dso list
        in rld (it is referenced each the list changes: see the special
	function __dbx_link() in the rld source code).
*/

/*
 * Stack as provided by kernel after an exec looks like:
 *	STRINGS		strings making up args and environment
 *	0
 *	ENVn		pts into STRINGS
 *	...
 *	ENV0		pts into STRINGS
 *	0
 *	ARGn		pts into STRINGS
 *	...
 *	ARG0		pts into STRINGS
 * sp-> ARGC
 *
 * NOTE: Only register initialized by kernel is sp!
 */
/* Coming from the kernel, argc, argv and environ are all 4 bytes
 * for _MIPS_SIM_NABI32.
 */
#if (_MIPS_SIM == _MIPS_SIM_ABI32 || _MIPS_SIM == _MIPS_SIM_NABI32)
#define ARGCOFF	0
#define BTOR	2	/* byte offset to stack word offset */
#else /* (_MIPS_SIM == _MIPS_SIM_ABI64 */
#define ARGCOFF	4
#define BTOR	3
#endif

	.text

LOCALSZ=	2		# save ra, gp 
FRAMESZ=	(((NARGSAVE+LOCALSZ)*SZREG)+ALSZ)&ALMASK
RAOFF=		FRAMESZ-(1*SZREG)
GPOFF=		FRAMESZ-(2*SZREG)

NESTED(__start, FRAMESZ, zero)
#ifdef _PIC
	SETUP_GPX(t0)
#else
	LA	gp,_gp
#endif
	lw	a0,ARGCOFF(sp)	# pick up argc
	PTR_ADDU a1,sp,_MIPS_SZPTR/8	# argv list starts after argc
	and	sp,sp,ALMASK	# align stack frame
	PTR_SUBU sp,FRAMESZ	# create stack frame
	USE_ALT_CP(gp)
	SETUP_GPX64(gp,t0)	# in 64-bit, we setup gp after frame is set
	PTR_ADDU a2,a1,_MIPS_SZPTR/8	# + to skip null between args
					# and _environ strs
	sll	v0,a0,BTOR	# convert argc to stack word index
	PTR_ADDU a2,v0		# _environ
	/*
	 * Avoid clobbering _environ if its already been set up -
	 * rld does this. If we clobber it, we lose putenv's done in .init
	 * sections. For non_shared - _environ should always be 0.
	 */
	PTR_L	a3,_environ
	bne	a3,0,1f
	PTR_S	a2,_environ
1:
	sw	a0,__Argc
	PTR_S	a1,__Argv
	SAVE_GP(GPOFF)
	REG_S	zero,RAOFF(sp)	# return PC of 0 stops debugger stack traces
	move	fp,zero		# old-style debugger stack trace stopper

	jal	__istart	# get all the init code executed
#ifdef _LIBC_ABI
        /*
         * init __xpg4 variable.
         * 1) This isn't the greatest place since .init sections won't get it.
         * 2) we always set it to 1 since ABI programs are ALWAYS compiled
         * with _XPG4, they will by definition pick up the XPG4 definitions
         * (like the socket code) and therefore MUST use the XPG4 version
	 * XXX move me when ld does this...
         */
        li      a0, 1
        sw      a0, __xpg4

	LA	a0, _cleanup
	jal	atexit
#endif

#ifdef MCRT0
	/*
	 * If profiling version, start-up profiling
	 */
	.weakext	eprol, _eprol
	.globl	_eprol

_eprol:	LA	a0,_eprol
	LA	a1,_etext
	jal	monstartup
	lw	a0,__Argc
	PTR_L	a1,__Argv
	PTR_L	a2,_environ
#endif /* MCRT0 */

#ifndef  _LIBC_ABI
        jal	__readenv_sigfpe #dummy (libc) or real routine to set up trap
				 #handler according to environment var: TRAP_FPE
#ifndef _PIC
	/*
	 * for nonshared only - call a trace point for the perf package
	 * shared programs can gain access via the perf packages' .so .init
	 * section
	 */
	lw	a0,__Argc
	PTR_L	a1,__Argv
	PTR_L	a2,_environ
	jal	__tp_main	# trace point for perf package
#endif /* !_PIC */
#endif /* _LIBC_ABI */

	lw	a0,__Argc
	PTR_L	a1,__Argv
	PTR_L	a2,_environ
	jal	main
	move    a0,v0           # get value returned from main
	jal	exit
	break	0		# exit should never return
.end	__start

#ifdef CRT0
/*
 * Null mcount routine in case something profiled is accidentally
 * linked with non-profiled code. Null moncontrol is in libc.
 */

 # dummy substitute for _mcount
 
 # _mcount gets called in a funny, nonstandard sequence, so even the
 # dummy routine must fix up the stack and restore the return address

	.set noreorder
	.set noat
	.globl _mcount
	.ent _mcount
_mcount:
	addu	sp,16
	j	ra
	move	ra,AT
.end _mcount
	.set reorder
	.set at

#endif /* CRT0 */

/*
 * for sproc - start up monitoring
 */
#ifdef CRT0
/* dummy _sprocmonstart is in libc */
#endif

#ifdef MCRT0

SLOCALSZ=	2		# save ra, gp
SFRAMESZ=	(((NARGSAVE+LOCALSZ)*SZREG)+ALSZ)&ALMASK
SRAOFF=		FRAMESZ-(1*SZREG)
SGPOFF=		FRAMESZ-(2*SZREG)

NESTED(_sprocmonstart, SFRAMESZ, zero)
	SETUP_GP
	PTR_SUBU sp,SFRAMESZ
	SETUP_GP64(SGPOFF,_sprocmonstart)
	SAVE_GP(SGPOFF)
	REG_S	ra,SRAOFF(sp)
	LA	a0,_eprol
	LA	a1,_etext
	jal	monstartup
	REG_L	ra,SRAOFF(sp)
	RESTORE_GP64
	PTR_ADDU sp,SFRAMESZ
	j	ra
	END(_sprocmonstart)
#endif /* MCRT0 */
