/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/


#ident	"$Revision: 1.2 $"

#ifdef __STDC__
	#pragma weak cryptbuf = _cryptbuf
#endif
#include "synonyms.h"
#include "crypt.h"

/*  [En|De]crypts buffer using key  */

#include <crypt.h>

#ifndef NULL
#define NULL	0
#endif

/*
 *  Common storage used by encryption routines
 */

static char block[64];
static char cv[8] = { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL };
static char last[8];

/*  
 *  Convert work buffer bits into a block of characters.
 *  8 bytes of bits -> 64 bytes of '0's or '1's.
 */

static void
expand(char *to, char *from)
{
	register int i, j;

	for (i=0; i<8; i++) {
		register char octet = *from;

		for (j=7; j>=0; j--)
			*(to++) = (octet >> j) & 1;

		from++;
	}
}

/*  
 *  Convert block of characters into work buffer bits.
 *  64 bytes of '0's or '1's -> 8 bytes of bits.
 */

static void
contract(char *to, char *from)
{
	register int i, j;

	for (i=0; i<8; i++) {
		register char octet = '\0';

		for (j=0; j<8; j++) {
			octet <<= 1;
			octet |= *from++;
		}

		*to++ = octet;
	}
}

/*
 *  Set the chaining value to that specified, or to the default.
 */

static void
set_cv(char *icv)
{
 	int i;

	/* reset Initial Chaining Value array */

	if (icv)
		for (i=0; i<8; i++)
			cv[i] = icv[i];
	else
	    	for (i=0; i<8; i++)
			cv[i] = 0;

	return;
}

/*
 *  Set the encryption key and the initial chaining value
 */

static void
set_key(char *key, char *icv, int mode)
{
	/* set the encryption/decryption key */

	switch (mode & X_ALGORITHM) {

	default:
	case X_DES:
		expand(block, key);
		des_setkey(block);
		break;

	case X_ENIGMA:
		enigma_setkey(key);
		break;

	}

	/* reset Initial Chaining Value array, whenever resetting key */

	set_cv(icv);

	return;
}

static void
Encrypt(char text[], int mode)
{
	/* encrypt/decrypt according to mode settings */

	switch (mode & X_ALGORITHM) {

	default:
	case X_DES:
		expand(block, text);
		des_encrypt(block, mode & X_DECRYPT);
		contract(text, block);
		break;

	case X_ENIGMA:
		enigma_encrypt(text, mode & X_DECRYPT);
		break;

	}

}

/*
 * The functions ecb(), cbc(), cfb(), and ofm() implement
 * the four data modes for encryption. For each, incomplete
 * blocks are handled using truncation by the recommended
 * means. If a NULL key is provided, the previously supplied
 * value is used.
 */

/* Electronic Code Book (ECB) mode encryption/decryption function. */

static void
ecb(char *text, unsigned int nbytes, char *key, char *icv, int mode)
{
	register j;
	register char *tp;

	/*  Set the Key and Initial Chaining Value if given  */

	if (key)
		set_key(key, icv, mode);
	else if (icv)
		set_cv(icv);

	/*  crypt buffer, first do complete blocks ...  */

	for (tp = text; tp-text+8 <= nbytes; tp += 8)
		Encrypt(tp, mode);

	/* Now deal with any remaining bytes. */

	if (nbytes%8) {

		/* Generate a constant string, always ENcrypt it,
		   and XOR with the necessary number of characters. */

		for (j=0; j<8; j++)
			cv[j] = (unsigned char) j;

		Encrypt(cv, mode & ~X_DECRYPT);

		for (j=0; j < (nbytes%8); j++)
			tp[j] ^= cv[j];

	} 
			
	return;
}

/* Cipher Block Chaining (CBC) mode encryption/decryption function. */

static void
cbc(char *text, unsigned int nbytes, char *key, char *icv, int mode)
{
	register j;
	register char *tp;

	/*  Set the Key and Initial Chaining Value if given  */

	if (key)
		set_key(key, icv, mode);
	else if (icv)
		set_cv(icv);

	/*  crypt buffer, first do complete blocks, chaining as we go ...  */

	for (tp = text; tp-text+8 <= nbytes; tp += 8) {

		if (mode & X_DECRYPT) {
			/* if DEcrypting, save and XOR later ... */
		
			for (j = 0; j < 8; j++)
				last[j] = tp[j];

			Encrypt(tp, mode);

			for (j = 0; j < 8; j++) {
				tp[j] ^= cv[j];
				cv[j] = last[j];
			}
		} else {
			/* if ENcrypting, XOR first ... */
			for (j = 0; j < 8; j++)
				tp[j] ^= cv[j]; 

			Encrypt(tp, mode);

			for (j = 0; j < 8; j++)
				cv[j] = tp[j];
		}

	}

	/* Now deal with any remaining bytes. */
	/* Always ENcrypt the chaining value and XOR the necessary bytes */

	if (nbytes%8) {

		Encrypt(cv, mode & ~X_DECRYPT);

		for (j=0; j < (nbytes%8); j++)
			tp[j] ^= cv[j];

		/* reset the chaining value for next call since it is useless */

		for (j=0; j<8; j++)
			cv[j] = 0;
	} 
			
	return;
}

/* Cipher Feedback (CFB) mode encryption/decryption function. */

static void
cfb(char *text, unsigned int nbytes, char *key, char *icv, int mode)
{
	register j = 0;
	register char *tp;
	register char hold;

	/*  Set the Key and Initial Chaining Value if given  */

	if (key)
		set_key(key, icv, mode);
	else if (icv)
		set_cv(icv);

	/* XOR buffer, byte by byte, into and with periodically ENcrypted cv */

	for (tp = text; j < nbytes; j++) {

		if ( j%8 == 0 )
			Encrypt(cv, mode & ~X_DECRYPT);
	
		/* if DEcrypting save the text character to place in the cv */

		if (mode & X_DECRYPT) {	/* DEcrypting */
			hold = tp[j];
			tp[j] ^= cv[j%8];
			cv[j%8] = hold;
		} else {		/* ENcrypting */
			tp[j] ^= cv[j%8];
			cv[j%8] = tp[j];
		}

	}

	return;
}

/* Output Feedback (OFM) mode encryption/decryption function. */

/* ARGSUSED */
static void
ofm(char *text, unsigned int nbytes, char *key, char *icv, int mode)
{
	register j = 0;
	register char *tp;

	/*  Set the Key and Initial Chaining Value if given  */

	if (key)
		set_key(key, icv, mode);
	else if (icv)
		set_cv(icv);

	/* XOR buffer, byte by byte, with periodically ENcrypted values */

	for (tp = text; j < nbytes; j++) {

		if ( j%8 == 0 )
			Encrypt(cv, mode & ~X_DECRYPT);
	
		tp[j] ^= cv[j%8];

	}

	return;
}

/*
 * cryptbuf() provides a generic interface to access any of the
 * four data modes, for encryption or decryption.
 */

void
cryptbuf(char *text, unsigned int nbytes, char *key, char *icv, int mode)
{
	/* If given a key, set it. Then pass NULL below so functions don't. */

	if (key)
		set_key(key, icv, mode);
	else if (icv)
		set_cv(icv);

	switch (mode & X_MODES) {

	default:
	case X_ECB:
		ecb(text, nbytes, NULL, NULL, mode);
		break;

	case X_CBC:
		cbc(text, nbytes, NULL, NULL, mode);
		break;

	case X_OFM:
		ofm(text, nbytes, NULL, NULL, mode);
		break;

	case X_CFB:
		cfb(text, nbytes, NULL, NULL, mode);
		break;

	}

	return;

}
