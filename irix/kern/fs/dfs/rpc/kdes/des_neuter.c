/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(_KERNEL)
static const char *_identString = "$Id: des_neuter.c,v 65.8 1999/02/12 23:30:29 mek Exp $";
static void _identFunc(const char *x) {if(!x)_identFunc(_identString);}
#endif



/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1996 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: des_neuter.c,v $
 * Revision 65.8  1999/02/12 23:30:29  mek
 * Change stub symbol name to be in sync with real function name.
 *
 * Revision 65.7  1999/02/04 22:37:19  mek
 * Support for external DES library.
 *
 * Revision 65.6  1999/01/19 20:28:13  gwehrman
 * Placed ifndef SGIMIPS_DES_INLINE around definitions for des_key_sched(),
 * des_cbc_cksum(), des_ecb_encrypt(), des_cbc_encrypt(), and
 * sec_des_is_weak_key().
 * Removed SGIMIPS ifdefs inside SGIMIPS_DES_INLINE ifdefs.
 *
 * Revision 65.5  1998/03/23 17:31:06  gwehrman
 * Source Identification changed to bracket with SGIMIPS && !_KERNEL.
 *
 * Revision 65.4  1998/01/07 17:20:40  jdoak
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS
 *
 * Revision 65.3  1997/11/06  19:56:38  jdoak
 * Source Identification added.
 *
 * Revision 65.2  1997/10/20  19:16:13  jdoak
 * Initial IRIX 6.4 code merge
 *
 * Revision 1.1.8.2  1996/02/17  23:59:36  marty
 * 	Update OSF copyright years
 * 	[1996/02/17  22:53:13  marty]
 *
 * Revision 1.2  1996/05/29  21:14:41  bhim
 * Modified some variable definitions from unsigned longs or longs to
 * unsigned32 or signed32 to be operable in all kernel bitsizes.
 *
 * Revision 1.1  1994/09/20  18:18:22  dcebuild
 * Original File from OSF
 *
 * Revision 1.1.6.2  1994/09/20  18:18:21  tom
 * 	Correct prototypes to match des.h
 * 	[1994/09/20  17:13:13  tom]
 *
 * Revision 1.1.6.1  1994/01/21  22:30:48  cbrooks
 * 	Code Cleanup
 * 	[1994/01/21  20:20:51  cbrooks]
 * 
 * Revision 1.1.4.3  1993/01/03  22:34:34  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  19:49:37  bbelch]
 * 
 * Revision 1.1.4.2  1992/12/23  19:35:23  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  01:15:15  zeliff]
 * 
 * Revision 1.1.2.2  1992/05/01  17:36:15  rsalz
 * 	"Changed pas part of rpc6 drop."
 * 	[1992/05/01  17:33:55  rsalz]
 * 
 * Revision 1.1  1992/01/19  03:17:02  devrcs
 * 	Initial revision
 * 
 * $EndLog$
 */
/*
*/
#include <dce/dce.h>
#include <des.h>

#ifndef SGIMIPS_DES_INLINE

#ifdef SGIMIPS_DES_EXTERNAL
int des_key_sched(x, y, z, k, s)
    int x;
    int y;
    int z;
#else
int des_key_sched(k, s)
#endif
    des_cblock *k;
    des_key_schedule s;
{
    memset( s, 0, sizeof(des_key_schedule)) ;
    return 0;
}

#ifdef SGIMIPS_DES_EXTERNAL
unsigned long des_cbc_cksum(x, y, z, in, out, length, key, iv)
    int x;
    int y;
    int z;
#else
unsigned long des_cbc_cksum(in, out, length, key, iv)
#endif
    des_cblock  *in;            /* >= length bytes of inputtext */
    des_cblock  *out;           /* >= length bytes of outputtext */
    register long length;       /* in bytes */
    des_key_schedule key;           /* precomputed key schedule */
    des_cblock  iv;             /* 8 bytes of ivec */
{
    memset( out, 0, sizeof(des_cblock)) ;
    return 0;
}

#ifdef SGIMIPS_DES_EXTERNAL
int des_cbc_encrypt(x, y, z, in, out, length, schedule, ivec, encrypt)
    int x;
    int y;
    int z;
#else
int des_cbc_encrypt(in, out, length, schedule, ivec, encrypt)
#endif
    des_cblock *in;
    des_cblock *out;
    signed32 length;
    des_key_schedule schedule;
    des_cblock ivec;
    int encrypt;
{
    memcpy( out, in, length ) ;
    return 0;
}

#ifdef SGIMIPS_DES_EXTERNAL
int des_ecb_encrypt(x, y, z, in, out, schedule, encrypt)
    int x;
    int y;
    int z;
#else
int des_ecb_encrypt(in, out, schedule, encrypt)
#endif
    des_cblock *in;
    des_cblock *out;
    des_key_schedule schedule;
    int encrypt;
{
    memcpy( out, in, sizeof(des_cblock)) ;
    return 0;
}

#endif /* !SGIMIPS_DES_INLINE */

#ifdef SGIMIPS_DES_EXTERNAL
int des_is_weak_key(x, y, z, key)
    int x;
    int y;
    int z;
#else
int des_is_weak_key(key)
#endif
    des_cblock key;
{
   return 0;
}
