#include "asm.h"
#include "sys/cpu.h"
#include "sys/sbd.h"
#include "sys/z8530.h"
#include "regdef.h"

/*	
#define VERBOSE 1
*/
			
#if _MIPS_SIM == _ABI64
#define REG_L	ld
#define REG_S	sd		
#define LONG_MUL	dmul	
#define BPREG	8
#else
#define REG_L	lw
#define REG_S	sw			
#define LONG_MUL	mul	
#define BPREG	4
#endif

/*
 * must use different temp registers for different ABI's
 */	
#if (_MIPS_SIM == _ABIO32)
#define tSP t5
#define tRA t6
#define tGP t7
#define tFP t8			
#elif (_MIPS_SIM == _ABI64 || _MIPS_SIM == _ABIN32)
#define tSP ta0
#define tRA ta1
#define tGP ta2
#define tFP ta3		
#endif		
	
/*
 * for debug output
 */
#ifdef VERBOSE
#define	VPRINT_MSG(msg)					\
	REG_S	a0,tmp;					\
	LA	a0,9f;					\
	nop;						\
	.data;						\
9:	.asciiz	msg;					\
	.text;						\
	REG_S	t0,tmp0;				\
	REG_S	t1,tmp1;				\
	REG_S	t2,tmp2;				\
	REG_S	t3,tmp3;				\
	REG_S	tSP,tmpSP;				\
	REG_S	tRA,tmpRA;				\
	REG_S	tGP,tmpGP;				\
	REG_S	tFP,tmpFP;				\
	jal	pon_puts;				\
	nop;						\
	REG_L	a0,tmp;					\
	REG_L	t0,tmp0;				\
	REG_L	t1,tmp1;				\
	REG_L	t2,tmp2;				\
	REG_L	t3,tmp3;				\
	REG_L	tSP,tmpSP;				\
	REG_L	tRA,tmpRA;				\
	REG_L	tGP,tmpGP;				\
	REG_L	tFP,tmpFP
#define VPRINT_REG(reg)					\
	REG_S	a0,tmp;					\
	move	a0,reg;					\
	REG_S	t0,tmp0;				\
	REG_S	t1,tmp1;				\
	REG_S	t2,tmp2;				\
	REG_S	t3,tmp3;				\
	REG_S	tSP,tmpSP;				\
	REG_S	tRA,tmpRA;				\
	REG_S	tGP,tmpGP;				\
	REG_S	tFP,tmpFP;				\
	jal	pon_puthex64;				\
	nop;						\
	REG_L	a0,tmp;					\
	REG_L	t0,tmp0;				\
	REG_L	t1,tmp1;				\
	REG_L	t2,tmp2;				\
	REG_L	t3,tmp3;				\
	REG_L	tSP,tmpSP;				\
	REG_L	tRA,tmpRA;				\
	REG_L	tGP,tmpGP;				\
	REG_L	tFP,tmpFP	
#else 
#define	VPRINT_MSG(msg)
#define	VPRINT_REG(reg)
#endif

/* turn on for gp conversion debug */	
#ifdef CONVERSION_DBG
#define CPRINT_MSG(msg) VPRINT_MSG(msg)
#define CPRINT_REG(reg) VPRINT_REG(reg)
#else
#define CPRINT_MSG(msg) 
#define CPRINT_REG(reg) 
#endif
	
	
/*
 * variables for start_module()
 */
		BSS(_fault_sp, BPREG)	  /* small stack for fault handling */
		BSS(initial_stack, BPREG) /* save sp, since debugger may trash it */
#ifdef VERBOSE	
		BSS(tmp, BPREG)		  /* tmp for print stuff */
		BSS(tmp0, BPREG)
		BSS(tmp1, BPREG)		  	
		BSS(tmp2, BPREG)		  	
		BSS(tmp3, BPREG)
		BSS(tmpSP, BPREG)		  	
		BSS(tmpRA, BPREG)		  	
		BSS(tmpGP, BPREG)
		BSS(tmpFP, BPREG)
#endif

/*
 * stack
 */
#define	SKSTKSZ		0x10000		/* 64K total--both stacks */
#define	EXSTKSZ		0x1000		/* 4K of SKSTKSIZE is fault stack */
		BSS(skstack,SKSTKSZ)

		.text
/*
 * start_module: start-up routine that is called by core to start an IDE
 *               module. This routine is responsible for calculating the
 *               new gp for this module. The gp value is read by core in
 *               the elf loading process (the gp value is in the .reginfo
 *               section of the module binary) and passed to the module
 *               in argv[1] as a string. See the conversion code below
 *               for more details on the gp. We also set up a new stack
 *               for the module, save args and the caller's sp, gp and ra.
 */
STARTFRM=	EXSTKSZ			# leave room for fault stack
NESTED(start_module, STARTFRM, zero)
	.set	noreorder

	# save important caller info - these are saved on the stack below
	move	tSP,sp			# make copy of caller's sp 	
	move	tRA,ra			# save copy of caller's ra
	move	tGP,gp			# save copy of caller's gp
	move	tFP,fp			# save copy of caller's fp
		
#if CONVERSION_DBG
	LI	gp, GP_ADDR		# must load gp for debug prints !!
#endif
	
	#
	# The first thing we have to do (even before we set up
	# the stack) is to set the gp register with the gp
	# value read from the IDE module binary in its .reginfo elf
	# section. This gp value was read during the loading
	# process and is passed to us in a string in argv[1].
	# The string simply contains the hexadecimal representation
	# of the address without 0x or 0X leaders ...
	# The following code converts this hex string and puts
	# the answer in t2 ...
	#
				
	#
	# string to hex conversion - assume null terminated
	# string which contains a hex number (no preceding 0x or 0X)
	# note:	 this code also assumes a 'leading' null ie the ending 
	#        null from argv[0].
	#
#define _0 0x30  # ascii characters	
#define	_9 0x39
#define _A 0x41	
#define _F 0x46
#define _a 0x61
	# first scan till the end of the ascii string
	# once reached - we will go back up converting the number
	REG_L	t0, BPREG(a1)		# load t0 w/ argv[1]
cont:			
	lbu	t1, (t0)		# load i'th byte in argv[1] string
	PTR_ADD	t0, 1			# increment addr for next byte read
	bne	t1, zero, cont		# do till '\0' character
	nop
	PTR_SUB	t0, 1			# get to 1st non-null char
	
	# now start the actual conversion
	move	t2, zero		# t2 holds total
	LI	t3, 1			# holds power of 16
loop:
	CPRINT_MSG("--------------------------\n\r");
	PTR_SUB	t0, 1			# decrement addr for next byte read
	lbu	t1, (t0)		# load i'th byte in argv[1] string
	CPRINT_MSG("addr t0= "); 
	CPRINT_REG(t0); 
	CPRINT_MSG("\n\r");
	CPRINT_MSG("read byte t1= "); 
	CPRINT_REG(t1); 
	CPRINT_MSG("\n\r");
	beq	t1, zero, done		# do till '\0' character
	nop
	ble	t1, _9, dec_dig		# this character holds 0-9
	nop
	ble	t1, _F, HEX_dig		# this character holds A-F
	nop
hex_dig:
	LONG_SUB	t1, (_a-0xa)		# convert a-f
	CPRINT_MSG("hex_dig\n\r"); 
	j	done_dig
	nop
HEX_dig:
	LONG_SUB	t1, (_A-0xA)		# convert A-F
	CPRINT_MSG("HEX_dig\n\r"); 	
	j	done_dig
	nop
dec_dig:
	LONG_SUB	t1, _0			# convert 0-9
	CPRINT_MSG("dec_dig\n\r"); 	
done_dig:
	CPRINT_MSG("after conversion t1= "); 
	CPRINT_REG(t1); 
	CPRINT_MSG("\n\r");
	LONG_MUL	t1, t3			# multiply by power
	LONG_MUL	t3, 16			# multiply power by 16 for next iter
	CPRINT_MSG("next power is t3= "); 
	CPRINT_REG(t3);
	CPRINT_MSG("\n\r");	
	LONG_ADD	t2, t2, t1		# add to total
	CPRINT_MSG("total is t2= "); 
	CPRINT_REG(t2); 
	CPRINT_MSG("\n\r");
	j	loop			# repeat
	nop
done:
	#
	# conversion done, set up the gp for this module
	#
	move	gp, t2
	VPRINT_MSG("start_module(): gp = "); 
	VPRINT_REG(gp); 
	VPRINT_MSG("\n\r");
	
	#
	# do usual stack set up ...
	# note:	 the gp has to be set up at this point cause
	#        the register saves to the stack below will
	#        actually use the gp cause we are compiling -shared ...
	#
	LA	sp,skstack		# beginning stack address

	PTR_ADDU sp,SKSTKSZ
	PTR_SUBU	sp,4*BPREG	# leave room for argsaves

	REG_S	sp,_fault_sp		# small stack for fault handling
	PTR_SUBU	sp,STARTFRM	# fault stack can grow to here + 16
	REG_S	zero,STARTFRM-BPREG(sp)	# keep debuggers happy
	REG_S	tSP,STARTFRM-(2*BPREG)(sp)	# save caller's sp	
	REG_S	tRA,STARTFRM-(3*BPREG)(sp)	# save caller's ra
	REG_S	tGP,STARTFRM-(4*BPREG)(sp)	# save caller's gp	
	REG_S	tFP,STARTFRM-(5*BPREG)(sp)	# save caller's fp
	REG_S	s0,STARTFRM-(6*BPREG)(sp)	# save caller's s0
	REG_S	s1,STARTFRM-(7*BPREG)(sp)	# save caller's s1
	REG_S	s2,STARTFRM-(8*BPREG)(sp)	# save caller's s2
	REG_S	s3,STARTFRM-(9*BPREG)(sp)	# save caller's s3
	REG_S	s4,STARTFRM-(10*BPREG)(sp)	# save caller's s4
	REG_S	s5,STARTFRM-(11*BPREG)(sp)	# save caller's s5
	REG_S	s6,STARTFRM-(12*BPREG)(sp)	# save caller's s6
	REG_S	s7,STARTFRM-(13*BPREG)(sp)	# save caller's s7
	
	REG_S	a0,STARTFRM(sp)			# save home args
	REG_S	a1,STARTFRM+BPREG(sp)		# save home args
	REG_S	a2,STARTFRM+(2*BPREG)(sp)	# save home args

	REG_S	sp,initial_stack	# save sp, since debugger may trash it
					# save sp so we can return to core later

	#	
	# call module
	#
	REG_L	a0,STARTFRM(sp)		# reload argc, argv, environ
	REG_L	a1,STARTFRM+BPREG(sp)
	REG_L	a2,STARTFRM+(2*BPREG)(sp)
	jal	module_main
	nop

XNESTED(end_module)
	
	#
	# restore environment & return to core
	#
	REG_L	sp,initial_stack		# restore our sp
	REG_L	ra,STARTFRM-(3*BPREG)(sp)	# restore caller's ra
	REG_L	gp,STARTFRM-(4*BPREG)(sp)	# restore caller's gp
	REG_L	fp,STARTFRM-(5*BPREG)(sp)	# restore caller's fp
	REG_L	s0,STARTFRM-(6*BPREG)(sp)	# restore caller's s0
	REG_L	s1,STARTFRM-(7*BPREG)(sp)	# restore caller's s1
	REG_L	s2,STARTFRM-(8*BPREG)(sp)	# restore caller's s2
	REG_L	s3,STARTFRM-(9*BPREG)(sp)	# restore caller's s3
	REG_L	s4,STARTFRM-(10*BPREG)(sp)	# restore caller's s4
	REG_L	s5,STARTFRM-(11*BPREG)(sp)	# restore caller's s5
	REG_L	s6,STARTFRM-(12*BPREG)(sp)	# restore caller's s6		
	REG_L	s7,STARTFRM-(13*BPREG)(sp)	# restore caller's s7
	# restore caller's sp last !
	REG_L	sp,STARTFRM-(2*BPREG)(sp)	# restore caller's sp	
	j	ra
	nop
	.set	reorder
	END(start_module)

/* 
 * MODULE only stuff END
 */	
