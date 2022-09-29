/* wdbBpLib.h - wdb break point library */

/* Copyright 1984-1994 Wind River Systems, Inc. */

/*
modification history
--------------------
01b,08jun95,ms	change CPU==SPARC to CPU_FAMILY==SPARC
01a,20nov94,rrr written.
*/

#ifndef __INCwdbBpLibh
#define __INCwdbBpLibh

/* includes */

#include "wdb/dll.h"

/* defines */

#ifndef offsetof
#define offsetof(type, mbr)	((size_t) &((type *)0)->mbr)
#endif

#define STRUCT_BASE(s,m,p)	((s *)(void *)((char *)(p) - offsetof(s,m)))
#define BP_BASE(p)		(STRUCT_BASE(struct breakpoint, bp_chain, (p)))

#define BP_INSTALLED	0x01	/* bp is installed (unless BP_TMP_REMOVED) */
#define BP_TMP_REMOVED	0x02	/* temporally removed because bp hit in isr */
#define BP_SYSTEM_BP	0x04	/* system break point */
#define BP_GLOBAL_BP	0x08	/* global break point */

#if CPU_FAMILY == I960
#define WDB_BREAK_INST	0x66003e00	/* breakpoint instruction */
#define reg_pc		rip
#define reg_sp		sp
#define reg_fp		fp
#endif

#if CPU_FAMILY == I80X86
#define WDB_BREAK_INST	0xcc		/* breakpoint instruction */
#define reg_pc		pc
#define reg_sp		esp
#define reg_fp		ebp
#endif

#if CPU_FAMILY==MC680X0
#define TRAP0_INST      0x4e40  		/* opcode for trap 0 */
#define	WDB_TRAP_NUM	2			/* debug trap number */
#define WDB_BREAK_INST	(TRAP0_INST + WDB_TRAP_NUM)	
#define reg_pc		regSet.pc
#define reg_sp		regSet.addrReg[7]
#define reg_fp		regSet.addrReg[6]
#endif

#if CPU==SIMSPARCSUNOS
#define WDB_NO_SINGLE_STEP 1
#define WDB_BREAK_INST	0x91d02001
#define reg_sp		reg_out[6]
#define reg_fp		reg_in[6]
#endif

#if CPU_FAMILY==SPARC
#define WDB_NO_SINGLE_STEP 1
#define WDB_TRAP_NUM	3
#define WDB_BREAK_INST	(0x91d02000 + (WDB_TRAP_NUM & 0x7f))
#define reg_pc		pc
#define reg_npc 	npc
#define reg_sp		out[6]
#define reg_fp		in[6]
#endif

#ifndef WDB_NO_SINGLE_STEP
#define WDB_NO_SINGLE_STEP 0
#endif

/* data types */

struct breakpoint
    {
    dll_t	bp_chain;
    dll_t	bp_taskChain;
    INSTR	*bp_addr;
    INSTR	bp_instr;
    int		bp_task;
    unsigned	bp_flags;
    };

/* function declarations */

#ifdef  __STDC__
extern	void	wdbBpRemove	(void);
extern	void	wdbBpInstall	(void);
#else   /* __STDC__ */
extern	void	wdbBpRemove	();
extern	void	wdbBpInstall	();
#endif  /* __STDC__ */

#endif	/* __INCwdbBpLibh */
