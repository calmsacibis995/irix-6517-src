/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef __SYS_AUXV_H__	/* wrapper symbol for kernel use */
#define __SYS_AUXV_H__	/* subject to change without notice */
#ident	"$Revision: 1.6 $"

typedef struct
{
       int     a_type;
       union {
               long    a_val;
               void    *a_ptr;
               void    (*a_fcn)();
       } a_un;
} auxv_t;

#define AT_NULL		0
#define AT_IGNORE	1
#define AT_EXECFD	2
#define AT_PHDR		3	/* &phdr[0] */
#define AT_PHENT	4	/* sizeof(phdr[0]) */
#define AT_PHNUM	5	/* # phdr entries */
#define AT_PAGESZ	6	/* getpagesize(2) */
#define AT_BASE		7	/* ld.so base addr */
#define AT_FLAGS	8	/* processor flags */
#define AT_ENTRY	9	/* a.out entry point */

#define AT_PFETCHFD	100	/* TFP prefetch instruction workaround - 
				 * mips4 n32 and mips4 64-bit binaries only.
				 */

#ifdef _KERNEL
typedef struct
{
	__int32_t     a_type;
	union {
		app32_long_t	a_val;
		app32_ptr_t	a_ptr;
		app32_ptr_t	a_fcn;
	} a_un;
} irix5_auxv_t;

typedef struct
{
	__int32_t     a_type;
	union {
		app64_long_t	a_val;
		app64_ptr_t	a_ptr;
		app64_ptr_t	a_fcn;
	} a_un;
} irix5_64_auxv_t;
#endif	/* _KERNEL */

#endif
