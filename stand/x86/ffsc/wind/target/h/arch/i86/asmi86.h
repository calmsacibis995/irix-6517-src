/* asm.h - i80x86 assembler definitions header file */

/* Copyright 1984-1993 Wind River Systems, Inc. */

/*
modification history
--------------------
01b,07jun93,hdn  added support for c++
01a,28feb92,hdn  written based on TRON, 68k version.
*/

#ifndef	__INCasmI86h
#define	__INCasmI86h

#ifdef __cplusplus
extern "C" {
#endif

/* fp offsets to arguments */

#define ARG1	8
#define ARG1W	10
#define ARG2	12
#define ARG2W	14
#define ARG3	16
#define ARG3W	18
#define ARG4	20
#define ARG5	24
#define ARG6	28
#define ARG7	32

#define DARG1	8		/* double arguments */
#define	DARG1L	12
#define DARG2	16
#define DARG2L	20
#define DARG3	24
#define DARG3L	28
#define DARG4	32
#define DARG4L	36

/* sp offsets to arguments */

#define SP_ARG1		4
#define SP_ARG1W	6
#define SP_ARG2		8
#define SP_ARG2W	10
#define SP_ARG3		12
#define SP_ARG3W	14
#define SP_ARG4		16
#define SP_ARG5		20
#define SP_ARG6		24
#define SP_ARG7		28

#ifdef __cplusplus
}
#endif

#endif	/* __INCasmI86h */
