/* vxCpu.h - VxWorks CPU definitions header */

/* Copyright 1984-1994 Wind River Systems, Inc. */

/*
modification history
--------------------
01i,20mar95,kvk  added I960JX specific stuff.
01h,31oct94,kdl  removed comment-within-comment.
01f,27sep93,cd   added R4000 support.
01g,26oct94,tmk  added new cpu type MC68LC040
01f,21jul94,tpr  added MC68060 cpu.
01f,09jun93,hdn  added support for I80X86
01g,02dec93,tpr  added definition for the AMD 29200 microcontroller
	    pad  added definitions for the AMD 29030 cpu
01g,11aug93,gae  vxsim hppa.
01f,22jun93,gae  vxsim: added SIMSPARCSUNOS.
01e,13nov92,dnw  added test for correct definition of CPU
01d,22sep92,rrr  added support for c++
01c,07sep92,smb  added documentation
01b,23jul92,jwt  restored SPARClite reference omitted in file creation.
01a,11jul92,smb  CPU definitions moved from vxWorks.h.
*/

/*
DESCRIPTION
Depending on the value of CPU passed to the system by the compiler command
line, the CPU_FAMILY is defined.  This must be the first header file
included by vxWorks.h.
*/

#ifndef __INCvxCpuh
#define __INCvxCpuh

#ifdef __cplusplus
extern "C" {
#endif


/* CPU types */

/* The Green Hills compiler pre-defines "MC68000"!! */
#ifdef MC68000
#undef MC68000
#endif

#define MC68000         1       /* CPU */
#define MC68010         2       /* CPU */
#define MC68020         3       /* CPU */
#define MC68030         4       /* CPU */
#define MC68040         5       /* CPU */
#define MC68LC040       6       /* CPU */
#define MC68060         7       /* CPU */
#define CPU32           8       /* CPU */
#define MC680X0         9       /* CPU_FAMILY */

#define SPARC           10      /* CPU and CPU_FAMILY */
#define SPARClite       11	/* CPU */

#ifndef I960
#define I960            20      /* CPU_FAMILY */
#endif
#define I960CA          21      /* CPU */
#define I960KA          22      /* CPU */
#define I960KB          23      /* CPU */
#define I960JX          24      /* CPU */

#define TRON            30      /* CPU_FAMILY */
#define G100            31      /* CPU */
#define G200            32      /* CPU */

#define MIPS            40      /* CPU_FAMILY */
#define R3000           41      /* CPU */
/* #define R33000       42      ** reserved */
/* #define R33020       43      ** reserved */
#define R4000		44	/* CPU */

#define AM29XXX         50      /* CPU_FAMILY */
#define AM29030		51      /* CPU */
#define AM29200		52      /* CPU */
#define AM29035		53      /* CPU */

#define SIMSPARCSUNOS	60	/* CPU & CPU_FAMILY */

#define SIMHPPA		70	/* CPU & CPU_FAMILY */

#define I80X86          80      /* CPU_FAMILY */
#define I80386          81      /* CPU */
#define I80486          82      /* CPU */


/* define the CPU family based on the CPU selection */

#undef CPU_FAMILY

#if	(CPU==MC68000 || CPU==MC68010 || CPU==MC68020 || CPU==MC68030 || \
	 CPU==CPU32   || CPU==MC68040 || CPU==MC68060 || CPU==MC68LC040)
#define	CPU_FAMILY	MC680X0
#endif	/* CPU_FAMILY==MC680X0 */

#if     (CPU==SPARC || CPU==SPARClite)
#define CPU_FAMILY      SPARC
#endif  /* CPU_FAMILY==SPARC */

#if 	(CPU==I960CA || CPU==I960KA || CPU==I960KB || CPU==I960JX)
#define	CPU_FAMILY	I960
#endif	/* CPU_FAMILY==I960 */

#if     (CPU==G100 || CPU==G200)
#define CPU_FAMILY      TRON
#endif	/* CPU_FAMILY==TRON */

#if	(CPU==R3000 || CPU==R4000)
#define CPU_FAMILY      MIPS
#endif	/* CPU_FAMILY==MIPS */

#if     (CPU==I80386 || CPU==I80486)
#define CPU_FAMILY      I80X86
#endif	/* CPU_FAMILY==I80X86 */

#if	(CPU==AM29030 || CPU==AM29035 || CPU==AM29200)
#define CPU_FAMILY	AM29XXX
#endif /* CPU_FAMILY==AM29XXX */
#if	(CPU==SIMSPARCSUNOS)
#define	CPU_FAMILY	SIMSPARCSUNOS
#endif	/* CPU_FAMILY==SIMSPARCSUNOS */

#if	(CPU==SIMHPPA)
#define	CPU_FAMILY	SIMHPPA
#endif	/* CPU_FAMILY==SIMHPPA */


/*
 * Check that CPU and CPU_FAMILY are now defined correctly.
 * If CPU is defined to be one of the above valid values then
 * the CPU_FAMILY will have been properly selected.
 * This is required in order to select the right headers
 * and definitions for that CPU in subsequent headers.
 * If CPU or CPU_FAMILY is not defined at this point,
 * we generate an error and abort the compilation.
 */

#if !defined(CPU) || !defined(CPU_FAMILY)
#error CPU is not defined correctly
#endif



#ifdef __cplusplus
}
#endif

#endif /* __INCvxCpuh */
