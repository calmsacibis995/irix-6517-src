/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1992-1997, Silicon Graphics, Inc.          *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ifndef __SYS_SN_NMI_H__
#define __SYS_SN_NMI_H__

#ident "$Revision: 1.5 $"

#include <sys/SN/addrs.h>

/*
 * The launch data structure resides at a fixed place in each node's memory
 * and is used to communicate between the master processor and the slave
 * processors.
 *
 * The master stores launch parameters in the launch structure
 * corresponding to a target processor that is in a slave loop, then sends
 * an interrupt to the slave processor.  The slave calls the desired
 * function, followed by an optional rendezvous function, then returns to
 * the slave loop.  The master does not wait for the slaves before
 * returning.
 *
 * There is an array of launch structures, one per CPU on the node.  One
 * interrupt level is used per CPU.
 */

#define NMI_MAGIC		0x48414d4d455201
#define NMI_SIZEOF		0x40

#define NMI_OFF_MAGIC		0x00	/* Struct offsets for assembly      */
#define NMI_OFF_FLAGS		0x08
#define NMI_OFF_CALL		0x10
#define NMI_OFF_CALLC		0x18
#define NMI_OFF_CALLPARM	0x20
#define NMI_OFF_GMASTER		0x28

/*
 * The NMI routine is called only if the complement address is
 * correct.
 *
 * Before control is transferred to a routine, the compliment address
 * is zeroed (invalidated) to prevent an accidental call from a spurious
 * interrupt.
 *
 */

#ifdef _LANGUAGE_C

typedef struct nmi_s {
	volatile ulong	 magic;		/* Magic number                     */
	volatile ulong	 flags;		/* Combination of flags above       */
	volatile addr_t *call_addr;	/* Routine for slave to call        */
	volatile addr_t *call_addr_c;	/* 1's complement of address        */
	volatile addr_t *call_parm;	/* Single parm passed to call	    */
	volatile ulong	 gmaster;	/* Flag true only on global master  */
} nmi_t;

#endif /* _LANGUAGE_C */

/* Following definitions are needed both in the prom & the kernel
 * to identify the format of the nmi cpu register save area in the
 * low memory on each node.
 */
#ifdef _LANGUAGE_C
#include <sys/types.h>

struct reg_struct {
	machreg_t	gpr[32];
	machreg_t	sr;
	machreg_t	cause;
	machreg_t	epc;
	machreg_t	badva;
	machreg_t	error_epc;
	machreg_t	cache_err;
	machreg_t	nmi_sr;
};

#endif /* _LANGUAGE_C */

/* These are the assembly language offsets into the reg_struct structure */

#define R0_OFF		0x0
#define R1_OFF		0x8
#define R2_OFF		0x10
#define R3_OFF		0x18
#define R4_OFF		0x20
#define R5_OFF		0x28
#define R6_OFF		0x30
#define R7_OFF		0x38
#define R8_OFF		0x40
#define R9_OFF		0x48
#define R10_OFF		0x50
#define R11_OFF		0x58
#define R12_OFF		0x60
#define R13_OFF		0x68
#define R14_OFF		0x70
#define R15_OFF		0x78
#define R16_OFF		0x80
#define R17_OFF		0x88
#define R18_OFF		0x90
#define R19_OFF		0x98
#define R20_OFF		0xa0
#define R21_OFF		0xa8
#define R22_OFF		0xb0
#define R23_OFF		0xb8
#define R24_OFF		0xc0
#define R25_OFF		0xc8
#define R26_OFF		0xd0
#define R27_OFF		0xd8
#define R28_OFF		0xe0
#define R29_OFF		0xe8
#define R30_OFF		0xf0
#define R31_OFF		0xf8
#define SR_OFF		0x100
#define CAUSE_OFF	0x108
#define EPC_OFF		0x110
#define BADVA_OFF	0x118
#define ERROR_EPC_OFF	0x120
#define CACHE_ERR_OFF	0x128
#define NMISR_OFF	0x130


#endif /* __SYS_SN_NMI_H__ */
