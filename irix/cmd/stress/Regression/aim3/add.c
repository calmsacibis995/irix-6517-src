/*
 * AIM Suite III v3.1.1
 * (C) Copyright AIM Technology Inc. 1986,87,88,89,90,91,92.
 * All Rights Reserved.
 */

#ifndef	lint
	static char sccs_id[] = { " @(#) add.c:3.2 5/30/92 20:18:29" };
#endif

#include "suite3.h"

/*
**  ARITHMETIC TIMERS
**
**                        short   long    float
**              -------------------------------
**              + add     adds    addl    addf
**
**              * mult    muls    mull    mulf
**
**              / div     divs    divl    divf
**
**      Various arithmetic operations are exercised within
**      scale by 32 loops so that 512*n operations are measured
**      per call.  Note:  Optimizing compilers sometimes optimize
**      + timings amazingly well in comparison to * and /.
**
**	Note:  Version 3.0 was changed dramatically from earlier
**	version.  The loop counters are no longer 32 by 16 or
**	whatever it was.  Eventhough it seems that the various
**	operations (+ * /) are all done with the same frequency,
**	it's not really.  We do that by adjusting the percentage
**	of the 'mix' in the defs file.  That keeps the source code
**	consistent, yet still allow us to 'tweak' the % to fit
**	our needs.
**	3/12/90 Tin Le
*/

/*
 *	add_double 
 */
add_double( argv, res )
char *argv;
Result *res;
{
	register int	n, i;
	register double	a, b;
	register double	d = 0.0;
	double		d1, d2;
	int		i32;

	if ( sscanf(argv, "%d %lf %lf", &i32, &d1, &d2) < 3 ) {
		fprintf(stderr, "add_double(): need 3 arguments!\n");
		exit(-1);
	}

	a = d1;
	b = d2;
	n = 100;
	while (n--)  {
		i = i32;
		while (i--) {
			d += b + a;
		}
	}
	res->d = d;
	return(0);
}

/*
 *      add_float
 */
add_float( argv, res )
char *argv;
Result *res;
{
	register int	n, i;
	register float	a, b;
	register float	d = 0.0;
	float		f1, f2;
	int		i32;

	if ( sscanf(argv, "%d %f %f", &i32, &f1, &f2) < 3 ) {
		fprintf(stderr, "add_float(): need 3 arguments!\n");
		exit(-1);
	}

	a = f1;
	b = f2;
	n = 100;
	while (n--)  {
		i = i32;
		while (i--) {
			d += b + a;
		}
	}
	res->d = d;
	return(0);
}

/*
 *      add_long
 */
add_long( argv, res )
char *argv;
Result *res;
{
	register int	n, i;
	register long	a, b;
	register long	l = 0;
	long		l1, l2;
	int		i32;

	if ( sscanf(argv, "%ld %ld %d", &l1, &l2, &i32) < 3 ) {
		fprintf(stderr, "add_long(): needs 3 arguments!\n");
		exit(-1);
	}

	n = 1000;
	a = l1;
	b = l2;
	while (n--)  {
		i = i32;
		while (i--) {
			l += b + a;
		}
	}
	res->l = l;
	return(0);
}

/*
 *      add_short
 */
add_short( argv, res )
char *argv;
Result *res;
{
	register int	n, i;
	register short	a, b;
	register short	l = 0;
	int		s1, s2;
	int		i32;

	if ( sscanf(argv, "%d %d %d", &s1, &s2, &i32) < 3 ) {
		fprintf(stderr, "add_short(): needs 3 arguments!\n");
		exit(-1);
	}

	n = 1000;
	a = (short)s1;
	b = (short)s2;
	while (n--)  {
		i = 256;
		while (i--) {
			l += (b + a);
		}
	}
	res->l = l;
	return(0);
}
