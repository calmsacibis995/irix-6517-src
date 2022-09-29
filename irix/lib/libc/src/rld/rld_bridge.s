/*
 * |-----------------------------------------------------------|
 * | Copyright (c) 1991 MIPS Computer Systems, Inc.            |
 * | All Rights Reserved                                       |
 * |-----------------------------------------------------------|
 * |          Restricted Rights Legend                         |
 * | Use, duplication, or disclosure by the Government is      |
 * | subject to restrictions as set forth in                   |
 * | subparagraph (c)(1)(ii) of the Rights in Technical        |
 * | Data and Computer Software Clause of DFARS 52.227-7013.   |
 * |         MIPS Computer Systems, Inc.                       |
 * |         950 DeGuigne Drive                                |
 * |         Sunnyvale, CA 94086                               |
 * |-----------------------------------------------------------|
 */
#ident "$Id: rld_bridge.s,v 1.4 1994/10/26 20:56:08 jwag Exp $"

#include <sys/regdef.h>
#include <sys/asm.h>

#if (_MIPS_SZPTR == 32)
FRAMESZ=	16
#define ARGCOFF 0
#define BTOR    2       /* byte offset to stack word offset */
#define SZSTKWD	4
#endif

#if (_MIPS_SZPTR == 64)
/* this means that all stack contents are 64 bit words */
FRAMESZ=	0
#define ARGCOFF 4
#define BTOR    3
#define SZSTKWD	8
#endif

NESTED(__exec_rld,FRAMESZ,zero)
/*
 * Stack as provided by kernel after an exec looks like:
 *	STRINGS		strings making up args and environment
 *	0
 *	AUXN		
 *	...
 * 	AUX0		(each auxillary entry's size varies)
 *	0
 *	ENVn		pts into STRINGS
 *	...
 *	ENV0		pts into STRINGS (one word each)
 *	0
 *	ARGn		pts into STRINGS
 *	...
 *	ARG0		pts into STRINGS
 * sp-> ARGC
 *
 * NOTE: Only register initialized by kernel is sp!
 */
	.option	pic2
#if (_MIPS_SIM != _MIPS_SIM_ABI32)
	.cplocal	gp
#endif
	.set	noreorder
	bal	1f
	nop
1:
	.cpload	ra		# setup the gp
	.set	reorder
#if (_MIPS_SIM != _MIPS_SIM_ABI32)
	.cpsetup ra, gp, 1b	# setup the gp for 64-bit
#endif
	PTR_SUBU sp,FRAMESZ	# setup stack for callee
	lw	 a0,FRAMESZ+ARGCOFF(sp)	# pick up argc
	PTR_ADDU a1,sp,FRAMESZ+SZSTKWD	# argv list starts after argc
	PTR_ADDU a2,a1,SZSTKWD	# skip null between args and environ strs
	sll	 v0,a0,BTOR	# convert argc to word index
	PTR_ADDU a2,v0		# environ
	/*
	 * save arguments to main before calling run-time linker
	 */
	move	s0,a0		# argc
	move	s1,a1		# argv
	move	s2,a2		# environ 

tstaux:				# loop thru env ptrs to find the auxv
	PTR_L	v0,0(a2)	# test if the null bet. env and auxv is found
	beqz	v0,getaux
	PTR_ADDU a2,a2,SZSTKWD
	b	tstaux
getaux:
	PTR_ADDU a3,a2,SZSTKWD	# to skip null between environ and aux vector
				# a3 points to auxv
	move	s3,a3		# save a3 also
	
	/* Load rld using __get_rld */
	move 	a0,s2
#ifndef __sgi /* second argument to __get_rld eliminated jlg 9/21/92 */
	li	a1,1
#endif
	jal	__get_rld
	bgtz	v0,2f
	break	10
2:
	/* run rld */
	PTR_L	a0,0(s1)	# main executable's name argv[0]
	move	a1,s2		# environ
	move	a2,s3		# auxv
	PTR_ADDU sp, FRAMESZ
_pgtrld:
	move	t9,v0
	j	t9		# jump to RLD's newmain (only for ELF obj)

.end	__exec_rld

