#ifndef __SYS_PCB_H__
#define __SYS_PCB_H__

/**************************************************************************
 *									  *
 * 		 Copyright (C) 1990, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"$Revision: 3.27 $"

/*
 * MIPS process control block
 */

/*
 * pcb_regs indices
 */
#define	PCB_S0		0	/* callee saved regs.... */
#define	PCB_S1		1
#define	PCB_S2		2
#define	PCB_S3		3
#define	PCB_S4		4
#define	PCB_S5		5
#define	PCB_S6		6
#define	PCB_S7		7
#define	PCB_SP		8	/* stack pointer */
#define	PCB_FP		9	/* frame pointer */
#define	PCB_PC		10	/* program counter */
#define	PCB_SR		11	/* C0 status register */
#define	PCB_LIOMSK	12	/* IP6 local interrupt mask register */
#define	NPCBREGS	13	/* number of regs saved at ctxt switch */

#if defined(_KERNEL) || defined(_KMEMUSER)
/*
 * jmp_buf offsets
 */
#define	JB_S0		0	/* callee saved regs.... */
#define	JB_S1		1
#define	JB_S2		2
#define	JB_S3		3
#define	JB_S4		4
#define	JB_S5		5
#define	JB_S6		6
#define	JB_S7		7
#define	JB_SP		8	/* stack pointer */
#define	JB_FP		9	/* frame pointer */
#define	JB_PC		10	/* program counter */
#define	JB_SR		11	/* C0 status register */

#define	JB_LIOMSK	12	/* IP6 local interrupt mask register */
#define JB_CEL		12	/* Everest Current Execution Level */

#define	NJBREGS		13

/*
 * single step information
 * used to hold instructions that have been replaced by break's when
 * single stepping
 * The ssi_cnt normally holds the count of sstep brk pts that have 
 * been put in the user code, and which have not been taken yet by
 * the processor. It is normally set to 0 as soon as the thread
 * enters the kernel. Except for one case, which is when a trap is
 * taken in user mode with epc pointing to the kernel installed
 * sstep brk. This is needed so that the trapend code does not
 * recompute a new brk address before returning to user mode. See
 * bug 522347 for details.
 */
struct ssi {
	int ssi_cnt;			/* number of bp's installed */
	struct ssi_bp {
		unsigned *bp_addr;	/* address of replaced instruction */
		unsigned bp_inst;	/* replaced instruction */
	} ssi_bp[2];
};

typedef struct pcb {
	__int32_t	pcb_resched;	/* non-zero if time to resched */
	/* These are use in branch delay instruction emulation */
	k_machreg_t	pcb_bd_epc;	/* epc register */
	k_machreg_t	pcb_bd_cause;	/* cause register */
	k_machreg_t	pcb_bd_ra;	/* return address if doing bd emul */
	__int32_t	pcb_bd_instr;	/* the branch instr for the bd emul */
	/* This is use in fp instruction emulation */
	k_fpreg_t	pcb_fpregs[32];	/* floating point */
	k_fpreg_t	pcb_fp_rounded_result; /* for ieee 754 emulation */
	__int32_t	pcb_fpc_csr;	/* floating point control/status reg */
	__int32_t	pcb_fpc_eir;	/* fp exception instruction reg */
	__int32_t	pcb_ownedfp;	/* has owned fp at one time */
	struct	ssi pcb_ssi;	/* single step state info */
} pcb_t;

#define PCB(x)		(curexceptionp->u_pcb.x)

typedef k_machreg_t	label_t[NJBREGS];

#if defined(_KERNEL) && !defined(_STANDALONE)
extern int setjmp(label_t);
extern int longjmp(label_t, int);
#endif

#endif /* _KERNEL || _KMEMUSER */
#endif /* __SYS_PCB_H__ */
