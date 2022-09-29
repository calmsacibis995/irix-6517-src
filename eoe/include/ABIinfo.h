/**************************************************************************
 *									  *
 * 		 Copyright (C) 1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ifndef __ABI_INFO_H__
#define __ABI_INFO_H__

#ident "$Revision: 1.2 $"

#ifdef __cplusplus
extern "C" {
#endif

#define ABIinfo_current	0
#define ABIinfo_abi32	1
#define ABIinfo_abi64	2

/* ABIinfo selectors */
#define	ABIinfo_BlackBook	0x01
#define	ABIinfo_mpconf		0x02
#define	ABIinfo_abicc		0x03
#define	ABIinfo_XPG		0x04
#define	ABIinfo_backtrace	0x05
#define	ABIinfo_largefile	0x06
#define	ABIinfo_longlong	0x07
#define	ABIinfo_X11		0x08
#define	ABIinfo_mmap		0x09

/* return values */
#define	ConformanceGuide_10	0x010000	/* version 1.0 */
#define	ConformanceGuide_11	0x010100	/* version 1.1 */
#define	ConformanceGuide_12	0x010200	/* version 1.2 */
#define	ConformanceGuide_20	0x020000	/* version 2.0 */
#define	ConformanceGuide_30	0x030000	/* version 3.0 */
#define	ConformanceGuide_31	0x030100	/* version 3.1 */
#define	ConformanceGuide_40	0x040000	/* version 4.0 */

/* function prototype */
extern int ABIinfo(int, int);

#ifdef __cplusplus
}
#endif
#endif
