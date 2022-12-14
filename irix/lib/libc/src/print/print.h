/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libc-port:print/print.h	1.9"
/* Maximum number of digits in any integer (including longlong) representation */
#define MAXDIGS 22

/* Maximum total number of digits in E format */
#if u3b || M32 || i286 || i386 || __mips
#define MAXECVT 17
#else
#define MAXECVT 18
#endif

/* Maximum number of digits after decimal point in F format */
#define MAXFCVT 120

/* Maximum significant figures in a floating-point number */
#define MAXFSIG MAXECVT

/* Maximum number of characters in an exponent */
#if u3b || M32 || i286 || i386 || __mips
#define MAXESIZ 5
#else
#define MAXESIZ 4
#endif

/* Maximum (positive) exponent */
#if u3b || M32 || i286 || i386 || __mips
#ifdef __sgi
#define MAXEXP 325
#else
#define MAXEXP 310
#endif
#else
#define MAXEXP 40
#endif

/* Data type for flags */
typedef char bool;

/* Convert a digit character to the corresponding number */
#define tonumber(x) ((x)-'0')

/* Convert a number between 0 and 9 to the corresponding digit */
#define todigit(x) ((x)+'0')

/* Max and Min macros */
#define max(a,b) ((a) > (b)? (a): (b))
#define min(a,b) ((a) < (b)? (a): (b))


