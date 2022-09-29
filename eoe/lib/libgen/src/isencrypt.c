/*
 * isencrypt.c
 *
 *
 * Copyright 1991, Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 */

#ident "$Revision: 1.4 $"


#ifdef __STDC__
	#pragma weak isencrypt = _isencrypt
#endif
#include "synonyms.h"

#include <sys/types.h>
#include <locale.h>
#include <string.h>

#define FBSZ 64

/*
 * function that uses heuristics to determine if
 * a file is encrypted
 */

int
isencrypt(register const char *fbuf, size_t ninbuf)
{
	register const char *fp;
	char *locale;
	int crflag = 0;
	size_t i;
	if(ninbuf == 0)
		return 0;
	fp = fbuf;
	while (fp < &fbuf[ninbuf])
	/* Check if file has non-ASCII characters */
		if(*fp++ & 0200) {
			crflag = 1;
			break;
		}
	if(crflag == 0)
		/* If all characters are ASCII, assume file is cleartext */
		return(0);
	locale = setlocale(LC_CTYPE, 0);
	if(strcmp(locale, "C") == 0 || strcmp(locale, "ascii") == 0)
	/*
	 * If character type is ascii or "C",
	 * assume file is encrypted
	 */
		return(1);
	if(ninbuf >= 64){
		/*
		 * We are in non-ASCII environment; use
		 * chi-square test to determine if file 
		 * is encrypted if there are more
		 * than 64 characters in buffer.
		 */

		int bucket[8];
		float cs;

		for(i=0; i<8; i++) bucket[i] = 0;

		for(i=0; i<64; i++) bucket[(fbuf[i]>>5)&07] += 1;

		cs = 0;
		for(i=0; i<8; i++) cs += (float) ((bucket[i]-8)*(bucket[i]-8));
		cs /= 8.;

		if(cs <= 24.322)
			return 1;
		return 0;
	}

	/* 
	 * If file has nulls, assume it is encrypted
	 */
	
	for(i = 0; i< ninbuf; i++) 
		if(fbuf[i] == '\0')
			return(1);

	/* 
	 * If last character in buffer is not a new-line,
	 * assume file is encrypted
	 */
	
	if(fbuf[ninbuf - 1] != '\n')
		return 1;
	return 0;
}
