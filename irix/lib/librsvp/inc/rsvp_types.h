/*
 * If your machine is not running 4.4BSD, look below under
 * CHECK HERE FOR MACHINE TYPE SIZES
 * and make sure that the correct values are defined for your machine.
 *
 * $Id: rsvp_types.h,v 1.6 1998/11/25 08:43:36 eddiem Exp $
 */

/*
 * Copyright 1994, 1995 Massachusetts Institute of Technology
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby
 * granted, provided that both the above copyright notice and this
 * permission notice appear in all copies, that both the above
 * copyright notice and this permission notice appear in all
 * supporting documentation, and that the name of M.I.T. not be used
 * in advertising or publicity pertaining to distribution of the
 * software without specific, written prior permission.  M.I.T. makes
 * no representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied
 * warranty.
 * 
 * THIS SOFTWARE IS PROVIDED BY M.I.T. ``AS IS''.  M.I.T. DISCLAIMS
 * ALL EXPRESS OR IMPLIED WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT
 * SHALL M.I.T. BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#ifndef _RSVP_TYPES_H_
#define _RSVP_TYPES_H_ 1

/*
 * This header file exists solely to emulate the 4.4 BSD case on machines
 * which aren't running 4.4 or a 4.4-derived system.  The header files
 * bsdcdefs.h and bsdendian.h are also a part of this mechanism.
 */

#include <sys/param.h>
#include <sys/types.h>

#if defined(BSD) && (BSD >= 199103)	/* Net/2 or 4.4 BSD */
#  include <sys/cdefs.h>
#  include <machine/endian.h>
#endif

#if !defined(BSD) || (BSD < 199103)	/* older than Net/2 or not BSD */

#ifdef __sgi
#  include "sys/cdefs.h"		/* Berkeley __P macros, etc. */
#else	
#  include "bsdcdefs.h"			/* Berkeley __P macros, etc. */
#endif

/* CHECK HERE FOR USE OF <machine/endian.h> */

#  if defined(__alpha) && defined(__osf__)
	/* slightly broken */
#       undef HAVE_MACHINE_ENDIAN_H
#  endif

#  ifdef HAVE_MACHINE_ENDIAN_H
#      include <machine/endian.h>
#  else
#	if defined(__sgi) && !defined(sgi_53)
#		include <sys/endian.h>
#	else
#     		include "bsdendian.h"
#	endif
#  endif

/* CHECK HERE FOR MACHINE TYPE SIZES */

#  ifndef __sgi

      typedef __signed char	int8_t;
      typedef unsigned char	u_int8_t;
      typedef __signed short	int16_t;
      typedef unsigned short	u_int16_t;
      typedef __signed int	int32_t;
      typedef unsigned int	u_int32_t;

#  endif /* __sgi */
#endif /* old BSD or not BSD at all */

#define ntoh16(x)	((u_int16_t)ntohs((u_int16_t)(x)))
#define NTOH16(x)	NTOHS(x)
#define ntoh32(x)	((u_int32_t)ntohl((u_int32_t)(x)))
#define NTOH32(x)	NTOHL(x)

#define hton16(x)	((u_int16_t)htons((u_int16_t)(x)))
#define HTON16(x)	HTONS(x)
#define hton32(x)	((u_int32_t)htonl((u_int32_t)(x)))
#define HTON32(x)	HTONL(x)

#if !defined(ntohf32)
/*
 *	Define byte order macros for 32-bit floating point
 *	(We define them here on the assumption that system files
 *	do not define them). [Thanks to Tom Calderwood of BBN]
 */
#if BYTE_ORDER == BIG_ENDIAN && !defined(lint)
#define ntohf32(x)	(x)
#define htonf32(x)	(x)

#define NTOHF32(x)
#define HTONF32(x)

#else /* Little-endian */

#define swap32(x)	 (((*((char *) &(x)) & 0xFF) << 24) | \
			  ((*((char *) &(x) + 1) & 0xFF) << 16 ) | \
			  ((*((char *) &(x) + 2) & 0xFF) << 8 ) | \
			  ((*((char *) &(x) + 3) & 0xFF) )) 

#define ntohf32(x)	swap32(x)
#define htonf32(x)	swap32(x)

#define NTOHF32(x)	(void) (*((unsigned long *) &(x)) = swap32(x))
#define HTONF32(x)	NTOHF32(x)

#endif
#endif /* Define floating byte order routines */

/*
 *	Define IEEE floating point Infinity
 */
#ifdef SOLARIS

#ifndef _MATH_H
/* This gross hack is instead of including math.h.  Log is defined
 *  in both rsvp and the math library, so math.h can not be included
 *  directly. */
typedef union _h_val {
  	unsigned long _i[2];
	double _d;
} _h_val;
#ifdef __STDC__
extern const _h_val __huge_val;
#else 
extern _h_val __huge_val;
#endif 
#define INFINITY32f	((float32_t)__huge_val._d)
#endif /* _MATH_H */

#else /* !SOLARIS */
#if defined(freebsd) || defined(__sgi)

/* calling __infinity instead of just using the value causes SIGILL - mwang */
/* This change was posted to rsvp_test 10/22/97 */
extern char   __infinity[];
#define INFINITY32f	(float32_t)(*(double *) __infinity)

#else /* !freebsd and !__sgi*/

extern double __infinity();
#define INFINITY32f	((float32_t)__infinity())
#endif /* !freebsd */
#endif /* !SOLARIS */

#endif /* _RSVP_TYPES_H_ */
