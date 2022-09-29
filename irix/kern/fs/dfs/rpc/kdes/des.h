/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1996 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: des.h,v $
 * Revision 65.6  1999/03/02 21:09:33  gwehrman
 * Modify the prototype for des_cbc_cksum() to use singed32 as a parameter
 * type instead of long for portability between 32 bit and 64 bit systems.
 *
 * Revision 65.5  1999/02/05 15:44:20  mek
 * Fix for typedefs when using external DES functions.
 *
 * Revision 65.4  1999/02/04 22:37:19  mek
 * Support for external DES library.
 *
 * Revision 65.3  1999/01/19 20:28:13  gwehrman
 * Placed prototypes for des_cbc_encrypt(), des_ecb_encrypt(), make_key_sched(),
 * and des_key_sched() inside SGIMIPS_DES_INLINE ifdefs.
 * Removed SGIMIPS ifdefs inside SGIMIPS_DES_INLINE ifdefs.
 *
 * Revision 65.2  1997/10/20 19:16:13  jdoak
 * Initial IRIX 6.4 code merge
 *
 * Revision 1.1.8.2  1996/02/18  23:46:15  marty
 * 	Update OSF copyright years
 * 	[1996/02/18  22:45:01  marty]
 *
 * Revision 1.3  1996/07/10  22:24:50  bhim
 * Inlined DES algorithm related functions.
 *
 * Revision 1.2  1996/05/29  21:14:35  bhim
 * Modified some variable definitions from unsigned longs or longs to
 * unsigned32 or signed32 to be operable in all kernel bitsizes.
 *
 * Revision 1.1  1994/01/21  22:30:47  dcebuild
 * Original File from OSF
 *
 * Revision 1.1.6.1  1994/01/21  22:30:47  cbrooks
 * 	Code Cleanup
 * 	[1994/01/21  20:20:50  cbrooks]
 *
 * Revision 1.1.4.3  1993/01/03  22:34:32  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  19:49:34  bbelch]
 * 
 * Revision 1.1.4.2  1992/12/23  19:35:16  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  01:15:11  zeliff]
 * 
 * Revision 1.1.2.2  1992/05/01  17:36:10  rsalz
 * 	"Changed pas part of rpc6 drop."
 * 	[1992/05/01  17:33:51  rsalz]
 * 
 * Revision 1.1  1992/01/19  03:16:50  devrcs
 * 	Initial revision
 * 
 * $EndLog$
 */
/*
*/

/*  des.h V=1 11/07/90 //crown/prgy/krb5/lib/destoo
**
** Copyright (c) Hewlett-Packard Company 1991
** Unpublished work. All Rights Reserved.
**
**    ** OSF DCE Confidential - DCE Security Component BL 6   **
**    ** OSF DCE AUTH 3 - prerelease                          **
**
*/
/* [eichin:19901017.0001EST] */

#ifndef _DES_H_
#define _DES_H_ 

#include <dce/dce.h>

typedef unsigned char des_cblock[8];
#ifdef SGIMIPS
typedef unsigned32 des_key_schedule[32];
#else
typedef unsigned long des_key_schedule[32];
#endif /* SGIMIPS */

#ifdef SGIMIPS_DES_INLINE

#ifdef _KERNEL
#include <kdes_inline.h>
#else /* _KERNEL */
#include <krb5/des_inline.h>
#endif /* _KERNEL */

#else /* !SGIMIPS_DES_INLINE */

#ifndef SGIMIPS_DES_EXTERNAL
#ifdef SGIMIPS
unsigned32 des_cbc_cksum _DCE_PROTOTYPE_ (
(des_cblock *, des_cblock *, register signed32, des_key_schedule, des_cblock)
);
#else /* SGIMIPS */
unsigned long des_cbc_cksum _DCE_PROTOTYPE_ (
(des_cblock *, des_cblock *, register long, des_key_schedule, des_cblock)
);
#endif /* SGIMIPS */
int des_cbc_encrypt _DCE_PROTOTYPE_ (
(des_cblock *, des_cblock*, signed32, des_key_schedule, des_cblock, int)
);
int des_ecb_encrypt _DCE_PROTOTYPE_ (
(des_cblock *, des_cblock *, des_key_schedule, int)
);
int des_key_sched _DCE_PROTOTYPE_ (
(des_cblock *, des_key_schedule)
);

#else /* SGIMIPS_DES_EXTERNAL */

#define DES_FUNC_KEY1 61171
#define DES_FUNC_KEY2 122671
#define DES_FUNC_KEY3 43098

#define des_cbc_cksum	sgi_dfs_func_0001
#define des_key_sched	sgi_dfs_func_0002
#define des_cbc_encrypt	sgi_dfs_func_0003
#define des_ecb_encrypt	sgi_dfs_func_0004
#define des_is_weak_key	sgi_dfs_func_0005

unsigned32 des_cbc_cksum _DCE_PROTOTYPE_ (
(int, int, int, des_cblock *, des_cblock *, register signed32,
 des_key_schedule, des_cblock)
);
int des_key_sched _DCE_PROTOTYPE_ (
(int, int, int, des_cblock *, des_key_schedule)
);
int des_cbc_encrypt _DCE_PROTOTYPE_ (
(int, int, int, des_cblock *, des_cblock *, signed32,
 des_key_schedule, des_cblock, int)
);
int des_ecb_encrypt _DCE_PROTOTYPE_ (
(int, int, int, des_cblock *, des_cblock *, des_key_schedule, int)
);

#endif /* SGIMIPS_DES_EXTERNAL */

#endif /* !SGIMIPS_DES_INLINE */

#ifndef SGIMIPS_DES_EXTERNAL
int des_is_weak_key _DCE_PROTOTYPE_ (
(des_cblock)
);
#else
int des_is_weak_key _DCE_PROTOTYPE_ (
(int, int, int, des_cblock)
);
#endif /* SGIMIPS_DES_EXTERNAL */

#endif				/* _DES_H_ */

