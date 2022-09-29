/*
 ****************************************************************************
 *    Copyright © 1996 Ordinal Technology Corp.  All Rights Reserved.       *
 *                                                                          *
 *    DO NOT DISCLOSE THIS SOURCE CODE OUTSIDE OF SILICON GRAPHICS, INC.    *
 *  This source code contains unpublished proprietary information of        *
 *  Ordinal Technology Corp.  The copyright notice above is not evidence    *
 *  of any actual or intended publication of this information.              *
 *      This source code is licensed to Silicon Graphics, Inc. for          *
 *  its personal use in developing and maintaining products for the IRIX    *
 *  operating system.  This source code may not be disclosed to any third   *
 *  party outside of Silicon Graphics, Inc. without the prior written       *
 *  consent of Ordinal Technology Corp.                                     *
 ****************************************************************************
 *
 *
 * ordinal.h	- headers for most or all ordinal programs
 *
 *	$Ordinal-Id: ordinal.h,v 1.26 1996/11/07 20:59:50 charles Exp $
 *	$Revision: 1.1 $
 */

#ifndef _ORDINAL_H
#define _ORDINAL_H
#define _SGI_MP_SOURCE	/* make 'errno' access the per-thread-private value */

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>
#include <unistd.h>
#include <bstring.h>
#include <ctype.h>
#include <signal.h>
#include <errno.h>
#include <ulocks.h>
#include <siginfo.h>
#include <ucontext.h>

#define CONST /* const*/

typedef signed char	i1;
typedef unsigned char	u1;
typedef u1		byte;
typedef signed short	i2;
typedef unsigned short	u2;
#if (_MIPS_SZLONG == 64)
typedef signed int	i4;
typedef unsigned int	u4;
#else
typedef signed long	i4;
typedef unsigned long	u4;
#endif
typedef long long	i8;
typedef unsigned long long u8;

/* Type of an mp-safe updateable counter which is updated by
 * compare_and_swap() atomic_add()
 */
typedef ptrdiff_t mpcount_t;

#define	MAX_I1	0x7F
#define	MAX_I2	0x7FFF
#define	MAX_I4	0x7FFFFFFF
#define	MAX_I8	0x7FFFFFFFFFFFFFFFLL

#define	MIN_I1	(- MAX_I1 - 1)
#define	MIN_I2	(- MAX_I2 - 1)
#define	MIN_I4	(- MAX_I4 - 1)
#define	MIN_I8	(- MAX_I8 - 1)

#define	MAX_U1	0xFF
#define	MAX_U2	0xFFFF
#define	MAX_U4	0xFFFFFFFF
#define	MAX_U8	0xFFFFFFFFFFFFFFFFLL

#ifndef NULL
#define	NULL		((void *) 0)
#endif

#ifndef TRUE
#define	TRUE	1
#endif

#ifndef FALSE
#define FALSE	0
#endif

#ifndef min
#define min(a, b)	((a) < (b) ? (a) : (b))
#endif
#ifndef max
#define max(a, b)	((a) > (b) ? (a) : (b))
#endif

/* ROUND_UP macro to round up a "ptr" to the next "rnd" (a power of 2) boundry.
 * This is done in a way that doesn't assume the pointer size is the same
 * as a int.
 */
#define ROUND_DOWN(ptr, rnd) \
		    ((byte *)(((ptrdiff_t)(ptr)) & ~((ptrdiff_t)(rnd)-1)))
#define ROUND_UP(ptr, rnd)	ROUND_DOWN((byte *) (ptr) + (rnd) - 1, rnd)

#define ROUND_UP_NUM(num, rnd)	(((num) + (rnd) - 1) & ~((rnd) - 1))

/*
** ROUND_UP_DIV - how many items of size S are needed to hold N bytes of data
*/
#define ROUND_UP_DIV(n_bytes, size)   ((((n_bytes) + (size) - 1) / (size)))
#define ROUND_UP_COUNT(n_bytes, size) (ROUND_UP_DIV((n_bytes), (size)) * (size))


#ifdef DEBUG
#ifdef __ANSI_CPP__
#define ASSERT(e)	if (!(e)) assert_failed(#e, __FILE__, __LINE__); else
#else
#define ASSERT(e)	if (!(e)) assert_failed("e", __FILE__, __LINE__); else
#endif
#else
#define ASSERT(e)
#endif

#define	SIGN_BIT	(1 << 31)

/*
 * Condition the IEEE754 floating point number pointed to by key so that
 * a straight byte-by-byte comparison gives the correct ordering
 */
#define COND_FLOAT(floatword) \
    if (((floatword) << 1) == 0) floatword = SIGN_BIT; /* pos or neg 0 */ \
    else if ((floatword) & SIGN_BIT)		/* negative? complement all */ \
	floatword = ~floatword; \
    else					/* positive: toggle sign */ \
	floatword ^= SIGN_BIT			/* to set the sign bit*/

#define COND_DOUBLE(hiword, loword) \
    if ((hiword << 1) == 0 && \
	 loword == 0) hiword = SIGN_BIT;	/* pos or neg 0 */ \
    else if (hiword & SIGN_BIT)			/* negative? complement all */ \
	hiword = ~hiword, loword = ~loword; \
    else					/* positive: toggle sign */ \
	hiword ^= SIGN_BIT

/* call simple system call to cause the equivalent of the 
 * sync processor instruction.
 */
#define SYNC()		/* sync() but this flushes the unix disk cache ! */

#define CONDITIONAL_FREE(ptr)	if (ptr) { free(ptr); ptr = NULL; } else 

/*
 * This returns non-zero if any byte in the word is equal to zero.
 * Each byte which contains zero is 'translated' to a value of 0x80.
 * 
 */
#define FIND_ZEROBYTE(word) (((word) - 0x01010101) & 0x80808080) & ~(word)
#define FIND_DELIM(word, delimword) FIND_ZEROBYTE((word) ^ (delimword))

/* mips-specific directives permitting efficient and
 * relatively straightforward unaligned memory accesses
 */
#pragma pack(1)

typedef struct
{
    byte	*uap;
} uap_t;

typedef struct
{
    u4		ua4;
} ua4_t;

typedef struct
{
    u2		ua2;
} ua2_t;

/* revert to default structure alignment */
#pragma pack(0)

/*
 * The following expressions are lvalues, so one may assign to
 * possibly unaligned values with them
 *
 *	Here, and in ulp/usp/ulw/ulh/usw/ush:
 *		a pointer size is whatever a generic pointer's size is
 *		a word is 4 bytes
 *		a halfward is 2 bytes
 *		a byte is 8 bits
 */
#define UNALIGNED_PTR(ptr)	(((uap_t *) (ptr))->uap)
#define UNALIGNED_WORD(ptr)	(((ua4_t *) (ptr))->ua4)
#define UNALIGNED_SHORT(ptr)	(((ua2_t *) (ptr))->ua2)

#define MEMMOVE_MACROS
#if 1
#define COPY123(top,d,s)t1 = ((const u4 *) (s))[top / 4 - 1],	\
			t2 = ((const u4 *) (s))[top / 4 - 2],	\
			t3 = ((const u4 *) (s))[top / 4 - 3];	\
			((u4 *) (d))[top / 4 - 1] = t1, \
			((u4 *) (d))[top / 4 - 2] = t2, \
			((u4 *) (d))[top / 4 - 3] = t3
#define MEMMOVE(dest, source, nbytes)   do switch (nbytes) {	\
		u4 t1, t2, t3; \
	  case 96: COPY123(96, dest, source);	\
	  case 84: COPY123(84, dest, source);	\
	  case 72: COPY123(72, dest, source);	\
	  case 60: COPY123(60, dest, source);	\
	  case 48: COPY123(48, dest, source);	\
	  case 36: COPY123(36, dest, source);	\
	  case 24: COPY123(24, dest, source);	\
	  case 12: COPY123(12, dest, source);	\
		   break;	\
	  case 104:COPY123(104, dest, source);\
	  case 92: COPY123(92, dest, source);	\
	  case 80: COPY123(80, dest, source);	\
	  case 68: COPY123(68, dest, source);	\
	  case 56: COPY123(56, dest, source);	\
	  case 44: COPY123(44, dest, source);	\
	  case 32: COPY123(32, dest, source);	\
	  case 20: COPY123(20, dest, source);	\
	  case 8: t1 = ((const u4 *) (source))[1]; \
		  t2 = ((const u4 *) (source))[0];\
		  ((u4 *) (dest))[1] = t1; ((u4 *) (dest))[0] = t2; \
		  break;	\
	  case 100:COPY123(100, dest, source);\
	  case 88: COPY123(88, dest, source);	\
	  case 76: COPY123(76, dest, source);	\
	  case 64: COPY123(64, dest, source);	\
	  case 52: COPY123(52, dest, source);	\
	  case 40: COPY123(40, dest, source);	\
	  case 28: COPY123(28, dest, source);	\
	  case 16: COPY123(16, dest, source);	\
	  case 4:  ((u4 *) (dest))[0] = ((const u4 *) (source))[0]; break; \
	  default: memmove(dest, source, nbytes); \
	  } while (FALSE)
#define BACKMOVE(dest, source, nbytes)   memmove(dest, source, nbytes)
#define VMEMMOVE(dest, source, nbytes)   do switch (nbytes) {	\
	  case 36:((u4 *) dest)[8] = ((const u4 *) source)[8]; \
	  case 32:((u4 *) dest)[7] = ((const u4 *) source)[7]; \
	  case 28:((u4 *) dest)[6] = ((const u4 *) source)[6]; \
	  case 24:((u4 *) dest)[5] = ((const u4 *) source)[5]; \
	  case 20:((u4 *) dest)[4] = ((const u4 *) source)[4]; \
	  case 16:((u4 *) dest)[3] = ((const u4 *) source)[3]; \
	  case 12:((u4 *) dest)[2] = ((const u4 *) source)[2]; \
	  case 8: ((u4 *) dest)[1] = ((const u4 *) source)[1]; \
	  case 4: ((u4 *) dest)[0] = ((const u4 *) source)[0]; break;	\
	  default:				\
	    {				\
		u4		*from;		\
		u4		*to;		\
		unsigned	bytes_left;	\
		u4		temp;		\
					    \
		bytes_left = (nbytes);	\
		from = (u4 *) (bytes_left - 16 + (byte *) (source));\
		to = (u4 *) (bytes_left - 16 + (byte *) (dest));	\
		for (;;)						\
		{							\
		    temp = from[3];					\
		    bytes_left -= sizeof(u4);			\
		    to[3] = temp;					\
		    if (bytes_left == 0)	break;			\
		    temp = from[2];					\
		    bytes_left -= sizeof(u4);			\
		    to[2] = temp;					\
		    if (bytes_left == 0)	break;			\
		    temp = from[1];					\
		    bytes_left -= sizeof(u4);			\
		    to[1] = temp;					\
		    if (bytes_left == 0)	break;			\
		    temp = from[0];					\
		    bytes_left -= sizeof(u4);			\
		    to[0] = temp;					\
		    if (bytes_left == 0)	break;			\
		    from -= 4;					\
		    to -= 4;					\
		}							\
	    } } while (FALSE)
#elif defined(MEMMOVE_MACROS)
/*
 * Macros for certain fully word-aligned memory-memory moves.
 * Use MEMMOVE for either non-overlapping moves or
 * overlapping moves in which dest is less than source
 * USE BACKMOVE for overlapping moves in which dest is higher than source
 */
#define MEMMOVE(dest, source, nbytes)	\
	do					\
	{					\
	    const u4	*from;			\
	    u4		*to;			\
	    unsigned	bytes_left;		\
	    u4		temp;			\
	    from = (const u4 *) (source);	\
	    to = (u4 *) (dest);			\
	    bytes_left = (nbytes);		\
	    for (;;)				\
	    {					\
		temp = from[0];			\
		bytes_left -= sizeof(u4);	\
		to[0] = temp;			\
		if (bytes_left == 0)	break;	\
		temp = from[1];			\
		bytes_left -= sizeof(u4);	\
		to[1] = temp;			\
		if (bytes_left == 0)	break;	\
		temp = from[2];			\
		bytes_left -= sizeof(u4);	\
		to[2] = temp;			\
		if (bytes_left == 0)	break;	\
		temp = from[3];			\
		bytes_left -= sizeof(u4);	\
		to[3] = temp;			\
		if (bytes_left == 0)	break;	\
		from += 4;			\
		to += 4;			\
	    }					\
	} while (FALSE)

/*
 * Use the following for overlapping moves in which dest is higher than source
 */
#define BACKMOVE(dest, source, nbytes)	\
	do				\
	{				\
	    u4		*from;		\
	    u4		*to;		\
	    unsigned	bytes_left;	\
	    u4		temp;		\
					\
	    bytes_left = (nbytes);	\
	    from = (u4 *) (bytes_left - 16 + (byte *) (source));\
	    to = (u4 *) (bytes_left - 16 + (byte *) (dest));	\
	    for (;;)						\
	    {							\
		temp = from[3];					\
		bytes_left -= sizeof(u4);			\
		to[3] = temp;					\
		if (bytes_left == 0)	break;			\
		temp = from[2];					\
		bytes_left -= sizeof(u4);			\
		to[2] = temp;					\
		if (bytes_left == 0)	break;			\
		temp = from[1];					\
		bytes_left -= sizeof(u4);			\
		to[1] = temp;					\
		if (bytes_left == 0)	break;			\
		temp = from[0];					\
		bytes_left -= sizeof(u4);			\
		to[0] = temp;					\
		if (bytes_left == 0)	break;			\
		from -= 4;					\
		to -= 4;					\
	    }							\
	} while (FALSE)
#else
#define MEMMOVE(dest, source, nbytes)	memmove((dest), (source), (nbytes))
#define BACKMOVE(dest, source, nbytes)	memmove((dest), (source), (nbytes))
#endif	/* MEMMOVE_MACROS */

#define TICKS_TO_MS(ticks)	((u4)(ticks) * 10)
#define TIMEVAL_TO_TICKS(tv)	((u4)((tv).tv_sec * 100 + (tv).tv_usec / 10000))

int		compare_and_swap(mpcount_t *p, ptrdiff_t old, ptrdiff_t new, usptr_t *u);
mpcount_t	atomic_add(mpcount_t *p, ptrdiff_t inc, usptr_t *u);
unsigned	get_time(void);
unsigned	get_time_us(void);
unsigned	get_cpu_time(void);
int		get_scale(const char **end_num, const char *scales);
unsigned long	get_number(const char *str);
char		*print_number(unsigned n);
void		die(const char *fmt, ...);
void		sigdeath(int sig, siginfo_t *pinfo, ucontext_t *ucp);
void		catch_signals(int first, int last, SIG_PF handler);
void		assert_failed(char *expr, char *file, int line);
void		unpin_range(byte *start, byte *end);
void		ordinal_exit(void);

/*
 * This string is printed out in die() and sigdeath(), as the program name
 * It may be set by any program at any time.
 */
extern char	*Ordinal_Title;
extern void	(*Ordinal_Cleanup)(void);

#if 1
/* Use the #pragma pack(1) directory to tell the compiler to use
 * a pair of special unaligned load word instructions 
 */
#define ulp(ptr)	UNALIGNED_PTR(ptr)		/* e.g. 4 or 8 bytes */
#define usp(ptr, val)	(UNALIGNED_PTR(ptr) = (val))	/* e.g. 4 or 8 bytes */

#define ulw(ptr)	UNALIGNED_WORD(ptr)		/* always 4 bytes */
#define usw(ptr, val)	(UNALIGNED_WORD(ptr) = (val))	/* always 4 bytes */

#define ulh(ptr)	UNALIGNED_SHORT(ptr)		/* always 2 bytes */
#define ush(ptr, val)	(UNALIGNED_SHORT(ptr) = (val))	/* always 2 bytes */
#else
/*
** These are tiny assembly language routines which efficiently perform
** possibly unaligned loads and stores of 32 and 16 bit words.
** See uls.s for the simple details
*/
unsigned ulw(const byte *a0);
void usw(byte *a0, unsigned a1);
unsigned short ulh(const byte *a0);
void ush(byte *a0, unsigned short a1);
#endif

/*
 * XXX TEMPORARY to circumvent mongoose cc -O2's handling of ulp()
 * The UNaligned load double sometimes gets changed to an ALIGNED load
 */
#undef ulp
byte *ulp(void *ptr);

/* macros for determining the file types from st_mode returned by stat/stat64(2)
 *
 * At one time CAN_SEEK() included character special files as `seekable'.
 * In nsort's use of CAN_SEEK(), this is misleading.  While one may indeed seek
 * on both a tty and a raw disk partition, seeking is a no-op on ttys. It would
 * be nice to distinguish a seekable S_ISCHR() dev from a non-seekable one.
 */
#define	CAN_SEEK(mode)	(S_ISREG(mode))
#define	SIZE_VALID(mode) (S_ISREG(mode) || S_ISDIR(mode))

/*
 * These are used in recognizing various command line arguments
 */
#define CHECK_END if (1)  { if (argv[1][2] != '\0') usage(argv[1]); } else
#define CHECK_POS(v) if (1)  { if ((v) <= 0) usage(argv[1]); } else

/* We may like sginap() to sleep only a millisecond or two, rather than 10.
 * Try replacing it with one of our own.
 */
#if defined(DEBUG2)
#define SHOWNAPS
#endif

#if !defined(DEBUG1)
#define ordnap(n, title)	sginap(n)
#elif defined(SHOWNAPS)
#define ordnap(n, title)	ordinal_nap(n, title)
int ordinal_nap(long ticks , const char *title);
#else
#define ordnap(n, title)	ordinal_nap(n)
int ordinal_nap(long ticks);
#endif

void exit_with_parent(void);
void sproc_hup(void);

void *fmemchr(const void *s, int c, size_t n);

u4 next_decimal_value(const byte *data, int len, int max_len, int bytes_so_far,
		      unsigned digits_per_value);

#endif /* _ORDINAL_H */
