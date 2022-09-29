/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/


#ident	"$Revision: 1.3 $"

#ifdef __STDC__
	#pragma weak enigma_setkey = _enigma_setkey
	#pragma weak enigma_encrypt = _enigma_encrypt
#endif
#include "synonyms.h"
#include "crypt.h"

/*
 *	A one-rotor machine designed along the lines of Enigma
 *	but considerably trivialized.
 */

#include	<string.h>
#include	<crypt.h>

#define	ROTORSZ	256
#define	MASK	0377

static	char	t1[ROTORSZ];
static	char	t2[ROTORSZ];
static	char	t3[ROTORSZ];

static	int	sn1, sn2;

void
enigma_setkey(const char *key)
{
	register	long		seed;
	register	unsigned	random;
	register	int		ic, i;

	int	k, temp;
	char	buf[13];

	if(!key)	{
		return;
	}

	for(i=0; i<8; i++)	{
		buf[i] = '\0';
	}

	strncpy(buf, key, 8);

	buf[8] = buf[0];
	buf[9] = buf[1];

	strncpy(buf, crypt(buf, &buf[8]), 13);

	seed = 123;

	for (i=0; i<13; i++)	{
		seed = seed*buf[i] + i;
	}
	for(i=0; i<ROTORSZ; i++) {
		t1[i] = i;
		t3[i] = 0;
	}
	for(i=0; i<ROTORSZ; i++) {
		seed = 5*seed + buf[i%13];
		random = (int)seed % 65521;
		k = ROTORSZ-1 - i;
		ic = (random & MASK) % (k+1);
		random >>= 8;
		temp = t1[k];
		t1[k] = t1[ic];
		t1[ic] = temp;
		if(t3[k] != 0) continue;
		ic = (random & MASK) % k;
		while(t3[ic] != 0) ic = (ic + 1) % k;
		t3[k] = ic;
		t3[ic] = k;
	}
	for(i=0; i<ROTORSZ; i++)	{
		t2[t1[i]&MASK] = i;
	}

	sn1 = 0;
	sn2 = 0;
}

static void
enigma(char *block, int len)
{
	register int	i;
	register int	n1, n2;
	register char	*cp;

	int	j;

	n1 = sn1;
	n2 = sn2;

	cp = block;
	j = len;
	while(j--)	{
		i = *cp;
		i = t2[(t3[(t1[(i+n1)&MASK]+n2)&MASK]-n2)&MASK]-n1;
		*cp++ = i;
		n1++;
		if(n1 == ROTORSZ) {
			n1 = 0;
			n2++;
			if(n2 == ROTORSZ) n2 = 0;
		}
	}

	sn1 = n1;
	sn2 = n2;
}

/* ARGSUSED */
void
enigma_encrypt(char *block, int flag)
{
	enigma(block, 8);
}
