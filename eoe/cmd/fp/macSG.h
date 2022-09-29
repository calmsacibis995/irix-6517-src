/* 
   (C) Copyright Digital Instrumentation Technology, Inc., 1990 - 1993 
       All Rights Reserved
*/
/*-- macSG.h ---------
 *
 * This file contains port-specific information
 *
 */

#ifndef MACSG_H
#define MACSG_H

/*
 * FREESTANDING is used internally by DIT to distinguish a distributed version
 * of the Mac library from an "in-context" use internally. It is intended
 * to be configured from the Makefile.
 *
 */

#include <string.h>

/* file mode macros */
#ifndef DT_RDONLY
#define DT_RDONLY   0x00000001 /* open for reading only */
#define DT_WRONLY   0x00000002 /* open for writing only */
#define DT_RDWR     0x00000003 /* open for reading and writing */
#define DT_NDELAY   0x00000004 /* do not block on open */
#define DT_APPEND   0x00000008 /* append on each write */
#define DT_CREAT    0x00000010 /* create file if it does not exist */
#define DT_TRUNC    0x00000020 /* truncate size to 0 */
#define DT_EXCL     0x00000040 /* error if create and file exists */

/* volume type macros */
#ifndef VOL_NONE
#define VOL_NONE  -1
#define VOL_720    0
#define VOL_1_44   1
#define VOL_2_88   2
#define VOL_HD     3
#define VOL_NFS    4
#define VOL_400    5
#define VOL_800    6
#define VOL_40MB   7
#define VOL_MAC_HD 8
#define VOL_320    10 /* 1 sided, 8 sector 0xFE */
#define VOL_360    11 /* 1 sided, 9 sector 0xFC */
#define VOL_640    12 /* 2-sided, 8 sector 0xFF */
#define VOL_1_2    13 /* 2-sided, 15 sector 0xF9 */
#endif /* VOL_NONE */

#endif	/* DT_RDONLY */


/* Standard macros */
#ifndef BOOL
#define BOOL int
#endif	/* BOOL */

#ifndef YES
#define YES 1
#endif	/* YES */

#ifndef TRUE
#define TRUE 1
#endif	/* TRUE */

#ifndef NO
#define NO 0
#endif	/* NO */

#ifndef FALSE
#define FALSE 0
#endif 	/* FALSE */

#endif	/* MACSG_H */

