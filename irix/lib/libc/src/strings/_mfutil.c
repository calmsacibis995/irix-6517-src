#ident "$Revision: 1.1 $"
/*	Copyright (c) 1993 UNIX System Laboratories, Inc.	*/
/*	(a wholly-owned subsidiary of Novell, Inc.).     	*/
/*	All Rights Reserved.                             	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF UNIX SYSTEM     	*/
/*	LABORATORIES, INC. (A WHOLLY-OWNED SUBSIDIARY OF NOVELL, INC.).	*/
/*	The copyright notice above does not evidence any actual or     	*/
/*	intended publication of such source code.                      	*/

/*	"@(#)libc-port:str/_mfutil.c	1.3"				*/
/*LINTLIBRARY*/
/*
* _mfutil.c - Operations on BigInt and MkFlt objects.
*/

#include "synonyms.h"
#include <stdlib.h>
#include <limits.h>
#include "mkflt.h"

typedef unsigned char	Uchar;
typedef unsigned short	Ushort;
typedef unsigned int	Uint;
typedef unsigned long	Ulong;

#define BIT(n)	((Ulong)1 << (n))

static BigInt *
#ifdef __STDC__
newbi(int sz)	/* allocate a new BigInt */
#else
newbi(sz)int sz;
#endif
{
	register BigInt *bp;

	if ((bp = (BigInt *)malloc(sizeof(BigInt)
		+ (sz - 1) * sizeof(Ulong))) == 0)
	{
		return 0;
	}
	bp->allo = 1;
	bp->next = 0;
	bp->size = sz;
	return bp;
}

BigInt *
#ifdef __STDC__
_mf_grow(BigInt *bp, int sz)	/* replace BigInt with larger copy */
#else
_mf_grow(bp, sz)BigInt *bp; int sz;
#endif
{
	register Ulong *sp, *dp;
	register int n;
	register BigInt *xp;

	if (sz <= bp->size)
		sz = (int)(bp->size + NPKT);	/* reasonable growth rate */
	if ((xp = newbi(sz)) == 0)
		return 0;
	xp->next = n = bp->next;
	sp = &bp->pkt[n];
	dp = &xp->pkt[n];
	do
		*--dp = *--sp;
	while (--n != 0);
	if (bp->allo)
		free(bp);
	return xp;
}
