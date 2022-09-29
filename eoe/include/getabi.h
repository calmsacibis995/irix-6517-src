#ifndef __GETABI_H__
#define __GETABI_H__
#ident "$Revision: 1.3 $"
#ifdef __cplusplus
extern "C" {
#endif

/*
 * Declarations for getabi().
 *
 * Copyright 1994, Silicon Graphics, Inc. 
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

/* enumeration of possible abi types */
typedef enum {
	noabi,
	abi32,	/* -32, generic 32-bit abi */
	abio32,	/* -o32, old mips1 32-bit abi */
	abin32,	/* -n32, new 32-bit abi */
	abi64,	/* -64, 64-bit abi */
	lastabi
} abi_t;

/* values for generic-size */
#define SPECIFIC_ABI		0
#define GENERIC_ABI_SIZE	1

/* values for modify-abi-arg */
#define IGNORE_ABI_ARG		0
#define PRESERVE_ABI_ARG	1
#define REMOVE_ABI_ARG		2
#define ADD_ABI_ARG		3
#define ADD_FRONT_ABI_ARG	4

/*
 * getabi() is used to determine what the abi is.  
 * First it checks the argv list, 
 * then the environment variable SGI_ABI, 
 * then the kernel (64-bit kernel defaults to 64-bit abi).
 * The generic_size boolean says whether to report the generic
 * size of the abi (abi32 or abi64) or to report the specific abi.
 * The modify_abi_arg says to either ignore the argv list,
 * or search the argv list and either preserve the args "as is",
 * or remove the arg that specifies the abi 
 * or add the default value to the argv.
 * Noabi is returned if an error occurs.
 */
extern abi_t getabi (
	int,		/*generic_size*/ 
	int,		/*modify_abi_arg*/
	char***,	/*argv*/
	int*		/*argc*/
);

#ifdef __cplusplus
}
#endif

#endif /* !__GETABI_H__ */

