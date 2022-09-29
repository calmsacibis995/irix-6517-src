/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libc-port:gen/crypt.c	1.11"
/*LINTLIBRARY*/
/*
 * This program implements a data encryption algorithm to encrypt passwords.
 */
#ifdef __STDC__
	#pragma weak crypt = _crypt
	#pragma weak encrypt = _encrypt
	#pragma weak setkey = _setkey
#endif
#include "synonyms.h"
#include <stdlib.h>
#include <errno.h>
#include <crypt.h>

extern char __libc_IP[];
extern char __libc_FP[];
extern char __libc_PC1_C[];
extern char __libc_PC1_D[];
extern char __libc_shifts[];
extern char __libc_PC2_C[];
extern char __libc_PC2_D[];
extern char __libc_E[];
extern char __libc_S[8][64];
extern char __libc_P[];

#define KSR1 16
#define KSR2 48
struct _KS{
	char elem[KSR1][KSR2];
};
#define ESIZ 48
static struct _KS *KS _INITBSS;
static char *E _INITBSS;

void
setkey(const char *key)
{
	register int i, j, k;
	char C[28], D[28];
	char t;

	if (KS == 0 && ((KS = (struct _KS *)calloc(1, KSR1*KSR2)) == 0))
		return;
	if ((E == 0) && ((E = (char *)calloc(1, ESIZ)) == 0))
		return;
	for(i=0; i < 28; i++) {
		C[i] = key[__libc_PC1_C[i]-1];
		D[i] = key[__libc_PC1_D[i]-1];
	}
	for(i=0; i < 16; i++) {
		for(k=0; k < __libc_shifts[i]; k++) {
			t = C[0];
			for(j=0; j < 28-1; j++)
				C[j] = C[j+1];
			C[27] = t;
			t = D[0];
			for(j=0; j < 28-1; j++)
				D[j] = D[j+1];
			D[27] = t;
		}
		for(j=0; j < 24; j++) {
			KS->elem[i][j] = C[__libc_PC2_C[j]-1];
			KS->elem[i][j+24] = D[__libc_PC2_D[j]-28-1];
		}
	}

	for(i=0; i < 48; i++)
		E[i] = __libc_E[i];
}

/*ARGSUSED*/
void
encrypt(char *block, int fake)
{
	int	i;
	register int t, j, k;
	char L[64], tempL[32], f[32], preS[48];
	char *R = &L[32];

	if (fake != 0) {
#if _ABIAPI
		errno = ENOSYS;
#else
		setoserror(ENOSYS);
#endif
		return;
	}
	if (KS == 0 && ((KS = (struct _KS *)calloc(1, KSR1*KSR2)) == 0))
		return;
	if ((E == 0) && ((E = (char *)calloc(1, ESIZ)) == 0))
		return;
	for(j=0; j < 64; j++)
		L[j] = block[__libc_IP[j]-1];
	for(i=0; i < 16; i++) {
		for(j=0; j < 32; j++)
			tempL[j] = R[j];
		for(j=0; j < 48; j++)
			preS[j] = (char)(R[E[j]-1] ^ KS->elem[i][j]);
		for(j=0; j < 8; j++) {
			t = 6*j;
			k = __libc_S[j][(preS[t+0]<<5)+
				(preS[t+1]<<3)+
				(preS[t+2]<<2)+
				(preS[t+3]<<1)+
				(preS[t+4]<<0)+
				(preS[t+5]<<4)];
			t = 4*j;
			f[t+0] = (char)((k>>3)&01);
			f[t+1] = (char)((k>>2)&01);
			f[t+2] = (char)((k>>1)&01);
			f[t+3] = (char)((k>>0)&01);
		}
		for(j=0; j < 32; j++)
			R[j] = L[j] ^ f[__libc_P[j]-1];
		for(j=0; j < 32; j++)
			L[j] = tempL[j];
	}
	for(j=0; j < 32; j++) {
		t = L[j];
		L[j] = R[j];
		R[j] = (char)t;
	}
	for(j=0; j < 64; j++)
		block[j] = L[__libc_FP[j]-1];
}

/* move out of function scope so we get a global symbol for use with data cording */
static char iobuf[16] _INITBSSS;

char *
crypt(const char *pw, const char *salt)
{
	register int c, i, j;
	char temp, block[66];

	for(i=0; i < 66; i++)
		block[i] = 0;
	for(i=0; (c= *pw) != '\0' && i < 64; pw++) {
		for(j=0; j < 7; j++, i++)
			block[i] = (char)((c>>(6-j)) & 01);
		i++;
	}
	
	setkey(block);
	
	for(i=0; i < 66; i++)
		block[i] = 0;

	for(i=0; i < 2; i++) {
		c = *salt++;
		iobuf[i] = (char)c;
		if(c > 'Z')
			c -= 6;
		if(c > '9')
			c -= 7;
		c -= '.';
		for(j=0; j < 6; j++) {
			if((c>>j) & 01) {
				temp = E[6*i+j];
				E[6*i+j] = E[6*i+j+24];
				E[6*i+j+24] = temp;
			}
		}
	}
	
	for(i=0; i < 25; i++)
		encrypt(block,0);
	
	for(i=0; i < 11; i++) {
		c = 0;
		for(j=0; j < 6; j++) {
			c <<= 1;
			c |= block[6*i+j];
		}
		c += '.';
		if(c > '9')
			c += 7;
		if(c > 'Z')
			c += 6;
		iobuf[i+2] = (char)c;
	}
	iobuf[i+2] = 0;
	if(iobuf[1] == 0)
		iobuf[1] = iobuf[0];
	return(iobuf);
}

