/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1992-1997, Silicon Graphics, Inc.          *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/*
 * promhdr.h -- Format for the contents of the IO6 flash PROM
 *
 *    PROM segments define different versions of code in the IO6 PROM.
 *    One of the segments is picked and loaded by the CPU PROM.
 *    Most fields are __uint64_t so they can be read directly from the PROM.
 */

#ifndef __SYS_SN_PROMHDR_H__
#define __SYS_SN_PROMHDR_H__

#ident "$Revision: 1.7 $"

#include <sys/types.h>

#define PROM_MAGIC		0x4a464b535743534dLL
#define PROM_MAXSEGS		16
#define PROM_DATA_OFFSET	0x1000

#define SFLAG_COMPMASK		0x7
#define SFLAG_NONE		0	/* Uncompressed                    */
#define SFLAG_RLE		1	/* Not yet supported               */
#define SFLAG_LZW		2	/* Not yet supported               */
#define SFLAG_GZIP		3	/* Up to 3:1                       */

#define SFLAG_LOADABLE		0x10

typedef struct {
    char		name[16];	/* Segment name                    */
    __uint64_t		flags;		/* Segment flags                   */
    __uint64_t		offset;		/* Offset of segment (aligned 8)   */
    __uint64_t		entry;		/* Segment entry point             */
    __uint64_t		loadaddr;	/* Address at which seg is loaded  */
    __uint64_t		length;		/* True length of segment          */
    __uint64_t		length_c;	/* Compressed length of segment    */
    __uint64_t		sum;		/* True segment byte sum           */
    __uint64_t		sum_c;		/* Compressed segment byte sum     */
    __uint64_t		memlength;	/* True length plus BSS length     */
    __uint64_t		resv1[5];	/* Pad to 128 bytes (power of 2)   */
} promseg_t;

typedef struct {
    __uint64_t		resv1[7];	/* Reserved                        */
    __uint64_t		revision;	/* PROM revision                   */
    __uint64_t		magic;		/* Magic number                    */
    __uint64_t		version;	/* PROM version                    */
    __uint64_t		length;		/* PROM image length incl. header  */
    __uint64_t		resv2[4];	/* Reserved                        */
    __uint64_t		numsegs;
    promseg_t		segs[PROM_MAXSEGS];

    /* First segment is at offset PROM_DATA_OFFSET (0x1000) */
} promhdr_t;

#endif /* __SYS_SN_PROMHDR_H__ */
