/*
 * AIM Suite III v3.1.1
 * (C) Copyright AIM Technology Inc. 1986,87,88,89,90,91,92.
 * All Rights Reserved.
 */

#ifndef	lint
	static char sccs_id[] = { " @(#) funcal.c:3.2 5/30/92 20:18:40" };
#endif

#define FUNCAL

#include "suite3.h"
#include "funcal.h"


int fcal0()
{
        return ( (*p_fcount += *p_i1) );
}


int fcal1(n)
register int  n;
{
        return ( (*p_fcount += n) );
}


int fcal2(n,i)
register int n,i;
{
        return ( (*p_fcount += n+i) );
}


int
fcal15( i1,i2,i3,i4,i5,i6,i7,i8,
        i9,i10,i11,i12,i13,i14,i15 )
{
        return ( (*p_fcount += i8+i15) );
}


/* special case to foil inlining optimizers */
int fcalfake()
{
        fprintf(stderr, "\nfun_cal: You should not see this message.\n");
        return(-1);     /*NOTREACHED*/
}

