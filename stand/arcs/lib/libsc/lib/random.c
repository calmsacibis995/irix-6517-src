/*
 * Copyright (c) 1983 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the University of California, Berkeley.  The name of the
 * University may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

/* Based on the BSD random source, via 4.0.1 lib/libc/bsd/gen/random.c
 * as of Jan 92, by Dave Olson.  See that source for all of the gory
 * details on the algorithm.  This is stripped down to the minimum
 * needed by fx.
static char sccsid[] = "@(#)random.c	5.5 (Berkeley) 7/6/88";
**/

#define		TYPE_3		3		/* x**31 + x**3 + 1 */
#define		BREAK_3		128
#define		DEG_3		31
#define		SEP_3		3

static long const randtbl[ DEG_3 + 1 ]	= { TYPE_3,
			    0x9a319039, 0x32d9c024, 0x9b663182, 0x5da1f342, 
			    0xde3b81e0, 0xdf0a6fb5, 0xf103bc02, 0x48f340fb, 
			    0x7449e56b, 0xbeb1dbb0, 0xab5c5918, 0x946554fd, 
			    0x8c2e680f, 0xeb3d799f, 0xb11ee0b7, 0x2d436b86, 
			    0xda672e2a, 0x1588ca88, 0xe369735d, 0x904f35f7, 
			    0xd7158fd6, 0x6fa6f051, 0x616e6b96, 0xac94efdc, 
			    0x36413f93, 0xc622c298, 0xf5a42ab8, 0x8a88d77b, 
					0xf5ad9d0e, 0x8999220b, 0x27fb47b9 };

/* can't initialize these 2, or they wouldn't work in the PROM,
 * should we ever use it there. */
static long	*fptr;
static long	*rptr;

static long const *state = &randtbl[ 1 ];
static long const *end_ptr = &randtbl[ DEG_3 + 1 ];


/*
 * random: * Returns a 31-bit random number.
 */
long
random(void)
{
	long		i;

	if(!fptr) {	/* first call */
		fptr = (long *)&randtbl[ SEP_3 + 1 ];
		rptr = (long *)&randtbl[ 1 ];
	}
	
	*fptr += *rptr;
	i = (*fptr >> 1)&0x7fffffff;	/* chucking least random bit */
	if(  ++fptr  >=  end_ptr  )  {
	fptr = (long *)state;
	++rptr;
	}
	else  {
	if(  ++rptr  >=  end_ptr  )  rptr = (long *)state;
	}
	return( i );
}

