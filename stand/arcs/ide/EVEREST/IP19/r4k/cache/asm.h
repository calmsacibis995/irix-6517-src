/*
 *
 * Copyright 1991,1992 Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 */

#ident "$Revision: 1.1 $"

/*
 * asm.h -- cpp definitions for assembler files
 */

/*
 * Notes on putting entry pt and frame info into symbol table for debuggers
 *
 *	.ent	name,lex-level	# name is entry pt, lex-level is 0 for c
 * name:			# actual entry point
 *	.frame	fp,framesize,saved_pc_reg
 *				# fp -- register which is pointer to base
 *				#	of previous frame, debuggers are special
 *				#	cased if "sp" to add "framesize"
 *				#	(sp is usually used)
 *				# framesize -- size of frame
 *				#	the expression:
 *				#		new_sp + framesize == old_sp
 *				#	should be true
 *				# saved_pc_reg -- either a register which
 *				#	contains callers pc or $0, if $0
 *				#	saved pc is assumed to be in
 *				#	(fp)+framesize-4
 *
 * Notes regarding multiple entry points:
 * LEAF is used when including the profiling header is appropriate
 * XLEAF is used when the profiling header is in appropriate (e.g.
 * when a entry point is known by multiple names, the profiling call
 * should appear only once.)  The correct ordering of ENTRY/XENTRY in this
 * case is:
 * XLEAF(copypage)		# declare globl and fall into LEAF
 * LEAF(copyseg)		# declare globl and emit profiling code
 */
/*
 * LEAF -- declare leaf routine
 */
#define	LEAF(x)						\
	.globl	x;					\
	.ent	x,0;					\
x:;							\
	.frame	sp,0,ra

/*
 * XLEAF -- declare alternate entry to leaf routine
 */
#define	XLEAF(x)					\
	.globl	x;					\
	.aent	x,0;					\
x:

/*
 * VECTOR -- declare exception routine entry
 */
#ifdef ASM_FIXED
#define	VECTOR(x, regmask)				\
	.globl	x;					\
	.ent	x,0;					\
x:;							\
	.frame	sp,EF_SIZE,$0;				\
	.mask	+(regmask)|M_EXCFRM,-(EF_SIZE-(EF_RA*4))
#else
#define	VECTOR(x, regmask)				\
	.globl	x;					\
	.ent	x,0;					\
x:;							\
	.frame	sp,EF_SIZE,$0;				\
	.mask	regmask,-(EF_SIZE-(EF_RA*4))
#endif

/*
 * NESTED -- declare nested routine entry point
 */
#define	NESTED(x, fsize, rpc)				\
	.globl	x;					\
	.ent	x,0;					\
x:;							\
	.frame	sp,fsize, rpc

/*
 * XNESTED -- declare alternate entry point to nested routine
 */
#define	XNESTED(x)					\
	.globl	x;					\
	.aent	x,0;					\
x:

/*
 * END -- mark end of procedure
 */
#define	END(proc)					\
	.end	proc

/*
 * IMPORT -- import external symbol
 */
#define	IMPORT(sym, size)				\
	.extern	sym,size

/*
 * ABS -- declare absolute symbol
 */
#define	ABS(x, y)					\
	.globl	x;					\
x	=	y

/*
 * EXPORT -- export definition of symbol
 */
#define	EXPORT(x)					\
	.globl	x;					\
x:

/*
 * BSS -- allocate space in bss
 */
#define	BSS(x,y)		\
	.comm	x,y

#define	SHARED(x)		\
	.extern	x 0

#define	SHAREDBSS(x,y)		\
	.comm	x,y

/*
 * LBSS -- allocate static space in bss
 */
#define	LBSS(x,y)		\
	.lcomm	x,y

/*
 * LGLOBAL --	load word of global
 * SGLOBAL --	store word of global
 *
 * these may be different if multi or not
 */
#define LW_GLOBAL(dest,name,temp) \
	lw	dest,name
#define SW_GLOBAL(source,name,temp) \
	sw	source,name


/*
 * The following macros reserve the usage of the local label '9'
 */
#define	PANIC(msg)					\
	sw	zero,waittime;				\
	la	a0,9f;					\
	jal	panic;					\
	MSG(msg)

#define	PRINTF(msg)					\
	la	a0,9f;					\
	jal	printf;					\
	MSG(msg)

#define	MSG(msg)					\
	.data;						\
9:	.asciiz	msg;					\
	.text

#define	SYSMAP(mname, vname, page, len)			\
	.globl	mname;					\
mname:							\
	.space	len*4;					\
	.globl	vname;					\
vname	=	((page)*NBPG)

/*
 * These two #define are to make setting the led register a liitle
 * cleaner in the asm code.
 *
 * LED_SA(pattern) -- machine INDEPENDENT routine to set the led register.
 *	Will set correctly no matter what machine type.  Is C code so
 *	requires a stack to run.  This should be used WHENEVER the above
 *	conditions can be met.
 *
 * LED_MACHDEP(pattern) -- machine DEPENDENT routine to set the led register.
 *	This is used in code where a stack is no available to use.  It
 *	is compiled dependent on the machine it is running on.  Should only
 *	really be used in prom specific code.
 */

#define LED_SA(pattern) \
	.set	noreorder; \
	li	a0,pattern; \
	jal	set_leds; \
	nop; \
	.set	reorder

#define LED_MACHDEP(pattern) \
	.set	noreorder; \
	li	a0,pattern; \
	jal	__set_leds; \
	nop; \
	.set	reorder

#if !defined(MIPSEB) && !defined(MIPSEL)
	.error Must define either MIPSEB or MIPSEL
#endif
#if defined(MIPSEB) && defined(MIPSEL)
	.error Only on of MIPSEB and MIPSEL may be defined
#endif


/* machine dependent stuff */

#define MACHINE_SPECIFIC \
	.set	noreorder; \
	jal	get_machine_type	/* get machine type in v0 */; \
	nop; \
	sub	v0,1			/* make 0 based */; \
	sll	v1,v0,2 		/* word address */; \
	lw	v1,80f(v1) 		/* load jumptab value */; \
	nop; \
	j	v1; \
	nop; \
	.set	reorder; \
80: 	/* jump table */ \
	.word 81f		/* m500 */; \
	.word 82f		/* m800 */; \
	.word 83f		/* m1000 */; \
	.word 84f		/* m120 */; \
	.word 85f		/* m2000 */; \
	.word 86f		/* Excalibur */; \
	.word 81f		/* m12 */; \
	.word 81f		/* m12 sable */; \
	.word 87f		/* m180 */; \
	.word 88f		/* m20 */; \
	.word 89f		/* Genesis */

#define L_M500	81
#define L_M800	82
#define L_M1000	83
#define L_M120	84
#define L_M2000	85
#define L_M6000 86
#define L_EXCALIBUR L_M6000	/* obsolete -- soon to disappear */
#define L_M180 87
#define	L_M20	88
#define L_RB3125 89
#define L_GENESIS L_RB3125	/* obsolete -- soon to disappear */


/* set v0 to NVRAM value */

#define NVRAM(x) \
	MACHINE_SPECIFIC; \
81: ;\
82: ;\
83: \
	.set	noreorder; \
	sll	v0,v0,2; \
	lw	v1,RT_CLOCK_ADDR(v0); \
	nop; \
	addi	v1,RT_MEM_OFFSET+RT_MEMX(x); \
	j	90f; \
	nop; \
84: ;\
85: ;\
86: ;\
87: ;\
88: ;\
89: ;\
	sll	v0,v0,2; \
	lw	v1,TODC_CLOCK_ADDR(v0); \
	nop; \
	addi	v1,TODC_MEM_OFFSET+TODC_MEMX(x); \
	j	90f; \
90: \
	lbu	v0,(v1); \
	nop; \
	.set	reorder

/* set NVRAM to v0 value */

#define SETNVRAM(y,x) \
	MACHINE_SPECIFIC; \
81: ;\
82: ;\
83: \
	.set	noreorder; \
	sll	v0,v0,2; \
	lw	v1,RT_CLOCK_ADDR(v0); \
	nop; \
	addi	v1,RT_MEM_OFFSET+RT_MEMX(x); \
	j	90f; \
	nop; \
84: ;\
85: ;\
86: ;\
87: ;\
88: ;\
89: ;\
	sll	v0,v0,2; \
	lw	v1,TODC_CLOCK_ADDR(v0); \
	nop; \
	addi	v1,TODC_MEM_OFFSET+TODC_MEMX(x); \
	j	90f; \
90: \
	li	v0,y; \
	sb	v0,(v1); \
	nop; \
	.set	reorder

/* return address of NVRAM field in v0*/

#define NVRAMADDR(x) \
	MACHINE_SPECIFIC; \
81: ;\
82: ;\
83: \
	.set	noreorder; \
	sll	v0,v0,2; \
	lw	v1,RT_CLOCK_ADDR(v0); \
	nop; \
	addi	v1,RT_MEM_OFFSET+RT_MEMX(x); \
	j	90f; \
	nop; \
84: ;\
85: ;\
86: ;\
87: ;\
88: ;\
89: ;\
	sll	v0,v0,2; \
	lw	v1,TODC_CLOCK_ADDR(v0); \
	nop; \
	addi	v1,TODC_MEM_OFFSET+TODC_MEMX(x); \
	j	90f; \
90: \
	move	v0,v1; \
	.set	reorder

/* Macros to return DUART0 or DUART1 address */

#define DUART0_BASE \
	.set	noreorder; \
	jal	get_machine_type; \
	nop; \
	sub	v0,1; \
	sll	v0,v0,2; \
	lw	v0,DUART0(v0); \
	nop; \
	.set	reorder

#define DUART1_BASE \
	.set	noreorder; \
	jal	get_machine_type; \
	nop; \
	sub	v0,1; \
	sll	v0,v0,2; \
	lw	v0,DUART1(v0); \
	nop; \
	.set	reorder

#define SCC0_BASE \
	.set	noreorder; \
	jal	get_machine_type; \
	nop; \
	sub	v0,1; \
	sll	v0,v0,2; \
	lw	v0,SCC_REG_BASE(v0); \
	nop; \
	.set	reorder

/*
 * register mask bit definitions
 */
#define	M_EXCFRM	0x00000001
#define	M_AT		0x00000002
#define	M_V0		0x00000004
#define	M_V1		0x00000008
#define	M_A0		0x00000010
#define	M_A1		0x00000020
#define	M_A2		0x00000040
#define	M_A3		0x00000080
#define	M_T0		0x00000100
#define	M_T1		0x00000200
#define	M_T2		0x00000400
#define	M_T3		0x00000800
#define	M_T4		0x00001000
#define	M_T5		0x00002000
#define	M_T6		0x00004000
#define	M_T7		0x00008000
#define	M_S0		0x00010000
#define	M_S1		0x00020000
#define	M_S2		0x00040000
#define	M_S3		0x00080000
#define	M_S4		0x00100000
#define	M_S5		0x00200000
#define	M_S6		0x00400000
#define	M_S7		0x00800000
#define	M_T8		0x01000000
#define	M_T9		0x02000000
#define	M_K0		0x04000000
#define	M_K1		0x08000000
#define	M_GP		0x10000000
#define	M_SP		0x20000000
#define	M_FP		0x40000000
#define	M_RA		0x80000000
