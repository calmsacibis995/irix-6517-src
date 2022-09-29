#ident "$Revision: 1.1 $"
/*	Copyright (c) 1993 UNIX System Laboratories, Inc.	*/
/*	(a wholly-owned subsidiary of Novell, Inc.).     	*/
/*	All Rights Reserved.                             	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF UNIX SYSTEM     	*/
/*	LABORATORIES, INC. (A WHOLLY-OWNED SUBSIDIARY OF NOVELL, INC.).	*/
/*	The copyright notice above does not evidence any actual or     	*/
/*	intended publication of such source code.                      	*/

/*	"@(#)libc-port:inc/mkflt.h	1.2"				*/
/*
* mkflt.h - Convert strings to floating value
*/

#define LOG2TO10(x) ((x) * 30103L / 100000L) /* floor(x*log(2)/log(10)) */
#define LOG2TO5(x)  ((x) * 43068L / 100000L) /* floor(x*log(2)/log(5)) */

#define ULBITS		(sizeof(unsigned long) * CHAR_BIT)
#define ULDIGS		LOG2TO10(ULBITS)

#if (_MIPS_SZLONG == 64)
#define ULEN(t)	((sizeof(double) + sizeof(t)) / sizeof(t))
#else
#define ULEN(t)	((sizeof(double) + sizeof(t) - 1) / sizeof(t))
#endif

typedef struct
{
	int		allo;	/* nonzero if allocated */
	int		next;	/* == current length */
	int		size;	/* total length of pkt[] */
	unsigned long	pkt[1];	/* grows as needed; [0] is most significant */
} BigInt;

#define NPKT	ULEN(unsigned long)	/* length when in MkFlt */

typedef struct
{
	const char	*str;	/* incoming string; reset to one past end */
	const wchar_t	*wcs;	/* incoming wide str; reset to one past end */
	size_t		ndig;	/* number of significand digits */
	long		exp;	/* exponent, either power of 2 or 10 */
	char		kind;	/* result shape and/or value: MFK_* */
	char		sign;	/* nonzero if result is negative */
	char		want;	/* no. of pkt's needed for target precision */
	BigInt		*bp;	/* integer version of digit string */
	BigInt		ibi;	/* initial BigInt object (+ fill[]) */
	unsigned long	fill[NPKT - 1]; /* must follow ibi */
	union {			/* result floating value */
		float		f;
		double		d;
		unsigned char	uc[ULEN(unsigned char)];
		unsigned short	us[ULEN(unsigned short)];
		unsigned int	ui[ULEN(unsigned int)];
		unsigned long	ul[ULEN(unsigned long)];
	} res;
} MkFlt;

#define EXP_OFLOW	((LONG_MAX - 10) / 10)	/* limiting magnitude */

#define MFK_ZERO	0	/* exactly zero value */
#define MFK_REGULAR	1	/* regular floating value */
#define MFK_OVERFLOW	2	/* specified value is too large */
#define MFK_UNDERFLOW	3	/* specified value is too small */
#define MFK_HEXSTR	4	/* hexadecimal digit string */
#define MFK_INFINITY	5	/* infinite value */
#define MFK_DEFNAN	6	/* default NaN */
#define MFK_VALNAN	7	/* NaN with specified value */

#ifdef __STDC__
#ifndef _mf_wcs
void	_mf_wcs(MkFlt *);	/* fill in MkFlt from wide string */
#endif
BigInt	*_mf_grow(BigInt *, int); /* replace with larger BigInt */
#else
#ifndef _mf_wcs
void _mf_wcs();
#endif
BigInt *_mf_grow();
#endif
