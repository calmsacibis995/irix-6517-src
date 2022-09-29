/* ivI86.h - I80x86 interrupt vectors */

/* Copyright 1984-1993 Wind River Systems, Inc. */

/*
modification history
--------------------
01d,29may94,hdn  removed I80486 conditional.
01c,08jun93,hdn  added support for c++. updated to 5.1.
01b,26mar93,hdn  added a new vector for 486.
01a,28feb92,hdn  written based on TRON, 68k version.
*/

#ifndef __INCivI86h
#define __INCivI86h

#ifdef __cplusplus
extern "C" {
#endif

/* macros to convert interrupt vectors <-> interrupt numbers */

#define IVEC_TO_INUM(intVec)	((int) (intVec) >> 3)
#define INUM_TO_IVEC(intNum)	((VOIDFUNCPTR *) ((intNum) << 3))


/* interrupt vector definitions */

#define IN_DIVIDE_ERROR			0
#define IV_DIVIDE_ERROR			INUM_TO_IVEC (IN_DIVIDE_ERROR)
#define IN_DEBUG			1
#define IV_DEBUG			INUM_TO_IVEC (IN_DEBUG)
#define IN_NON_MASKABLE			2
#define IV_NON_MASKABLE			INUM_TO_IVEC (IN_NON_MASKABLE)
#define IN_BREAKPOINT			3
#define IV_BREAKPOINT			INUM_TO_IVEC (IN_BREAKPOINT)
#define IN_OVERFLOW			4
#define IV_OVERFLOW			INUM_TO_IVEC (IN_OVERFLOW)
#define IN_BOUND			5
#define IV_BOUND			INUM_TO_IVEC (IN_BOUND)
#define IN_INVALID_OPCODE		6
#define IV_INVALID_OPCODE		INUM_TO_IVEC (IN_INVALID_OPCODE)
#define IN_NO_DEVICE			7
#define IV_NO_DEVICE			INUM_TO_IVEC (IN_NO_DEVICE)
#define IN_DOUBLE_FAULT			8
#define IV_DOUBLE_FAULT			INUM_TO_IVEC (IN_DOUBLE_FAULT)
#define IN_CP_OVERRUN			9
#define IV_CP_OVERRUN			INUM_TO_IVEC (IN_CP_OVERRUN)
#define IN_INVALID_TSS			10
#define IV_INVALID_TSS			INUM_TO_IVEC (IN_INVALID_TSS)
#define IN_NO_SEGMENT			11
#define IV_NO_SEGMENT			INUM_TO_IVEC (IN_NO_SEGMENT)
#define IN_STACK_FAULT			12
#define IV_STACK_FAULT			INUM_TO_IVEC (IN_STACK_FAULT)
#define IN_PROTECTION_FAULT		13
#define IV_PROTECTION_FAULT		INUM_TO_IVEC (IN_PROTECTION_FAULT)
#define IN_PAGE_FAULT			14
#define IV_PAGE_FAULT			INUM_TO_IVEC (IN_PAGE_FAULT)
#define IN_RESERVED			15
#define IV_RESERVED			INUM_TO_IVEC (IN_RESERVED)
#define IN_CP_ERROR			16
#define IV_CP_ERROR			INUM_TO_IVEC (IN_CP_ERROR)
#define IN_ALIGNMENT			17
#define IV_ALIGNMENT			INUM_TO_IVEC (IN_ALIGNMENT)

/* 17 - 31	Unassigned, Reserved */

/* 32 - 255	User Defined Vectors */

#ifdef __cplusplus
}
#endif

#endif	/* __INCivI86h */
