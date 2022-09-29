/*
 * AIM Suite III v3.1.1
 * (C) Copyright AIM Technology Inc. 1986,87,88,89,90,91,92.
 * All Rights Reserved.
 */

#ifndef	lint
	static char sccs_id[] = { " @(#) fun.c:3.2 5/30/92 20:18:38" };
#endif

#include "suite3.h"
#include "funcal.h"

/*
**  FUNCTION CALLER
**		fcall0, fcal0 -> funcall 0 args
**		fcall1, fcal1 -> funcall 1 arg
**		fcall2, fcal2 -> funcall 2 args
**		fcall15, fcal15 -> funcall 15 args
**
**	Functions are called with various permutations of no-, one-, two-
**	and thirty one-parameters.  Each function is called n*512 times
**	for the drill.
**
** 3/12/90 Tin Le
**	- Feedbacks indicated that very few 'real world' programs out
**	there really uses function calls with 31 args; the suggested
**	maximum is between 10-15.  In looking over X source and a number
**	of 'typical' SunView programs, I picked 15 as a reasonable 'max'
**	number of args a function might use.
**	- As a side note, this should make RISC people a little bit
**	happier (especially the ones with register windows).
*/


/*
 *      fun_cal
 */
fun_cal( argv, res )
char *argv;
Result *res;
{
register int	i, n=1000;
	int	i1;
	long	fcount;
        int	(*p_fcal0)();

	if ( sscanf(argv, "%d %ld", &i1, &fcount) < 2 ) {
		fprintf(stderr, "fun_cal(): needs 2 arguments!\n");
		return(-1);
	}
        p_i1 = &i1;
        p_fcount = &fcount;

        if ( i1 )
                p_fcal0 = fcal0;
        else
                p_fcal0 = fcalfake;

        while (n--)
                for (i=32;i--;)
                      (*p_fcal0)(), (*p_fcal0)(), (*p_fcal0)(), (*p_fcal0)(),
                      (*p_fcal0)(), (*p_fcal0)(), (*p_fcal0)(), (*p_fcal0)(),
                      (*p_fcal0)(), (*p_fcal0)(), (*p_fcal0)(), (*p_fcal0)(),
                      (*p_fcal0)(), (*p_fcal0)(), (*p_fcal0)(), (*p_fcal0)();
	res->l = fcount;
        return(0);
}


/*
 *      fun_cal1
 */
fun_cal1( argv, res )
char *argv;
Result *res;
{
register int	i, n=1000;
	int	i1;
	long	fcount;
        int	(*p_fcal1)();

	if ( sscanf(argv, "%d %ld", &i1, &fcount) < 2) {
		fprintf(stderr, "fun_cal1(): needs 2 arguments!\n");
		return(-1);
	}

        p_i1  = &i1;
        p_fcount = &fcount;

        if ( i1 )
                p_fcal1 = fcal1;
        else
                p_fcal1 = fcalfake;

        while (n--)
                for (i=32;i--;)
                      (*p_fcal1)(n), (*p_fcal1)(n),
                        (*p_fcal1)(n), (*p_fcal1)(n),
                      (*p_fcal1)(n), (*p_fcal1)(n),
                        (*p_fcal1)(n), (*p_fcal1)(n),
                      (*p_fcal1)(n), (*p_fcal1)(n),
                        (*p_fcal1)(n), (*p_fcal1)(n),
                      (*p_fcal1)(n), (*p_fcal1)(n),
                        (*p_fcal1)(n), (*p_fcal1)(n);
	res->l = fcount;
        return(0);
}

/*
 *      fun_cal2
 */
fun_cal2( argv, res )
char *argv;
Result *res;
{
register int	i, n=1000;
	int	i1;
	long	fcount;
        int	(*p_fcal2)();

	if ( sscanf(argv, "%d %ld", &i1, &fcount) < 2) {
		fprintf(stderr, "fun_cal2(): needs 2 arguments!\n");
		return(-1);
	}
        p_i1  = &i1;
        p_fcount = &fcount;

        if ( i1 )
                p_fcal2 = fcal2;
        else
                p_fcal2 = fcalfake;

        while (n--)
                for (i=32;i--;)
                      (*p_fcal2)(n,i), (*p_fcal2)(n,i),
                        (*p_fcal2)(n,i), (*p_fcal2)(n,i),
                      (*p_fcal2)(n,i), (*p_fcal2)(n,i),
                        (*p_fcal2)(n,i), (*p_fcal2)(n,i),
                      (*p_fcal2)(n,i), (*p_fcal2)(n,i),
                        (*p_fcal2)(n,i), (*p_fcal2)(n,i),
                      (*p_fcal2)(n,i), (*p_fcal2)(n,i),
                        (*p_fcal2)(n,i), (*p_fcal2)(n,i);

	res->l = fcount;
        return(0);
}

/*
 *      fun_cal15
 */
fun_cal15( argv, res )
char *argv;
Result *res;
{
register int	i, n=1000;
	int	i1;
	long	fcount;
        int	(*p_fcal15)();

	if ( sscanf(argv, "%d %ld", &i1, &fcount) < 2) {
		fprintf(stderr, "fun_cal15(): needs 2 arguments!\n");
		return(-1);
	}
        p_i1  = &i1;
        p_fcount = &fcount;

        if ( i1 )
                p_fcal15 = fcal15;
        else
                p_fcal15 = fcalfake;

        while ( n-- )  {
                for (i=512; i--; )
                        (*p_fcal15)( i,i,i,i,   i,i,i,i,
                                i,i,i,i,   i,i,i );
        }
	res->l = fcount;
        return(0);
}

