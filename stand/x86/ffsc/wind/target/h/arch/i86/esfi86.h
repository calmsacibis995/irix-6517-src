/* esf.h - I80x86 exception stack frames */

/* Copyright 1984-1993 Wind River Systems, Inc. */

/*
modification history
--------------------
01b,08jun93,hdn  added support for c++. updated to 5.1.
01a,28feb92,hdn  written based on TRON, 68k version.
*/

#ifndef	__INCesfI86h
#define	__INCesfI86h

#ifdef __cplusplus
extern "C" {
#endif

#ifndef	_ASMLANGUAGE

/* Exception stack frames.  Most of these can happen only for one
   CPU or another, but they are all defined for all CPU's */

/* Format-0 - no privilege change, no error code */

typedef struct
    {
    INSTR *pc;				/* 0x00 : PC */
    unsigned short cs;			/* 0x04 : code segment */
    unsigned short pad0;		/* 0x06 : padding */
    unsigned long eflags;		/* 0x08 : EFLAGS */
    } ESF0;				/* sizeof(ESF0) -> 0x0c */

/* Format-1 - no privilege change, error code */

typedef struct
    {
    unsigned long errCode;		/* 0x00 : error code */
    INSTR *pc;				/* 0x04 : PC */
    unsigned short cs;			/* 0x08 : code segment */
    unsigned short pad0;		/* 0x0a : padding */
    unsigned long eflags;		/* 0x0c : EFLAGS */
    } ESF1;				/* sizeof(ESF1) -> 0x10 */

#endif	/* _ASMLANGUAGE */

#ifdef __cplusplus
}
#endif

#endif	/* __INCesfI86h */
