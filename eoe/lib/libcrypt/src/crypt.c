/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"$Revision: 1.6 $"

#ifdef __STDC__
	#pragma weak setkey = _setkey
	#pragma weak encrypt = _encrypt
	#pragma weak crypt = _crypt
#endif
#include "synonyms.h"
#include "crypt.h"

void
setkey (const char *key)
{
	des_setkey(key);
}

void
encrypt(char *block, int edflag)
{
	des_encrypt(block, edflag);
}

char *
crypt(const char *pw, const char *salt)
{
	return(des_crypt(pw, salt));
}
