/* regsI86.h - I80x86 registers */

/* Copyright 1984-1993 Wind River Systems, Inc. */

/*
modification history
--------------------
01f,21sep95,hdn  added X86CPU_NS486.
01e,01nov94,hdn  added X86CPU_386, X86CPU_486, X86CPU_PENTIUM.
01d,27oct94,hdn  added CR0_CD_NOT and CR0_NW_NOT.
01c,08jun93,hdn  added support for c++. updated to 5.1.
01b,26mar93,hdn  added some defines for 486.
01a,28feb92,hdn  written based on TRON, 68k version.
*/

#ifndef	__INCregsI86h
#define	__INCregsI86h

#ifdef __cplusplus
extern "C" {
#endif

#ifndef	_ASMLANGUAGE

#define GREG_NUM	8	/* has 8 32-bit general registers */

typedef struct		/* REG_SET - 80x86 register set */
    {
    ULONG edi;		/* general register */
    ULONG esi;		/* general register */
    ULONG ebp;		/* general register */
    ULONG esp;		/* general register */
    ULONG ebx;		/* general register */
    ULONG edx;		/* general register */
    ULONG ecx;		/* frame pointer register */
    ULONG eax;		/* stack pointer register */
    ULONG eflags;	/* status register (must be second to last) */
    INSTR *pc;		/* program counter (must be last) */
    } REG_SET;

#define fpReg		ebp	/* frame pointer */
#define spReg		esp	/* stack pointer */

#define  G_REG_BASE	0x00		/* data reg's base offset to REG_SET */
#define  G_REG_OFFSET(n)	(G_REG_BASE + (n)*sizeof(ULONG))
#define  SR_OFFSET		G_REG_OFFSET(GREG_NUM)
#define  PC_OFFSET		(SR_OFFSET + sizeof(ULONG))
#endif	/* _ASMLANGUAGE */


#define X86CPU_386	0
#define X86CPU_486	1
#define X86CPU_PENTIUM	2
#define X86CPU_NS486	3

#define	EFLAGS_BRANDNEW	0x200		/* brand new EFLAGS */
#define	CODE_SELECTOR	0x0008		/* brand new code selector */

/* control and test registers */

#define CR0		1
#define CR1		2
#define CR2		3
#define CR3		4
#define TR3		5
#define TR4		6
#define TR5		7
#define TR6		8
#define TR7		9

/* bits on CR0 */

#define CR0_PE		0x00000001	/* protection enable */
#define CR0_MP		0x00000002	/* math present */
#define CR0_EM		0x00000004	/* emulation */
#define CR0_TS		0x00000008	/* task switch */
#define CR0_NE		0x00000020	/* numeric error */
#define CR0_WP		0x00010000	/* write protect */
#define CR0_AM		0x00040000	/* alignment mask */
#define CR0_NW		0x20000000	/* not write through */
#define CR0_CD		0x40000000	/* cache disable */
#define CR0_PG		0x80000000	/* paging */
#define CR0_NW_NOT	0xdfffffff	/* write through */
#define CR0_CD_NOT	0xbfffffff	/* cache disable */

#ifdef __cplusplus
}
#endif

#endif	/* __INCregsI86h */
