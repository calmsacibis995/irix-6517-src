/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1992, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/*
 * "promhdr.h" -- Format for the header information encoded at the
 * 	beginning of the IO4 EPC flash EPROM. 
 */

#ifndef __SYS_EVEREST_PROMHDR_H__
#define __SYS_EVEREST_PROMHDR_H__

#ident "$Revision: 1.6 $"

#define PROM_MAGIC	0x4a464b34
#define PROM_NEWMAGIC	0x4a4b5357
#define SI_MAGIC	0x53454732

#define NUMSEGS		16
#define PROMDATA_OFFSET 0x1000
#define SEGINFO_OFFSET	0x100		/* 256 bytes */

/* 
 * Segment types
 *	Types are bitfields of the following form:
 *		Bit 0     -- Endianess (1 == BIG, 0 == SMALL)
 *		Bits 1-2  -- Compression type (0 == NONE)
 *		Bits 3-7  -- Reserved.
 *		Bits 8-31 -- Type information.
 */
#define SFLAG_ENDIANMASK	0x1
#define SFLAG_BE		0x1
#define SFLAG_LE		0x0

#define SFLAG_COMPMASK		(0x3 << 1)
#define SFLAG_UNCOMPRESSED	0x0
#define SFLAG_RLE		(0x1 << 1)
#define SFLAG_LZW		(0x2 << 1)

#define STYPE_MASK		(0xff << 8)
#define STYPE_R4000		(0x1 << 8)
#define STYPE_TFP		(0x2 << 8)
#define STYPE_GFX		(0x3 << 8)
#define	STYPE_R10000		(0x4 << 8)
#define STYPE_MASTER		(0xff << 8)

#define SEGTYPE_R4000BE		(STYPE_R4000 | SFLAG_BE)
#define SEGTYPE_TFP		(STYPE_TFP | SFLAG_BE)
#define	SEGTYPE_R10000		(STYPE_R10000 | SFLAG_BE)
#define SEGTYPE_GFX		(STYPE_GFX | SFLAG_BE)
#define SEGTYPE_MASTER		(STYPE_MASTER | SFLAG_BE)	

/*
 * We use segments to define different and possibly conflicting IO4 PROM
 * code segments which are loaded by the secondary prom after initial
 * system checks complete.
 */

typedef struct {
    __uint32_t		seg_type;	/* Segment type */
    __uint32_t		seg_offset;	/* Start of segment in flash prom */
    __int64_t		seg_entry;	/* Segment entry point */
    __int64_t		seg_startaddr;	/* Address at which seg is loaded */
    __uint32_t		seg_length;	/* Length of segment */
    __uint32_t		seg_cksum;	/* Segment checksum */
    __int64_t		seg_data;	/* Free general-purpose data */
    __uint64_t		seg_reserved1;	/* Reserved for future use */
} promseg_t;

/*
 * The promhdr starts the PROM and describes where to load the initial
 * secondary prom startup code.
 *
 */

typedef struct {
    __uint32_t		prom_magic;	/* Magic number */
    __uint32_t		prom_cksum;	/* Checksum of IO4 PROM entry code */
    __uint32_t		prom_startaddr;	/* Load address for the entry code */
    __uint32_t		prom_length;	/* Number of bytes in entry code */
    __uint32_t		prom_entry;	/* PROM entry code entrypoint */
    __uint32_t		prom_version;	/* PROM entry code revision number */
} evpromhdr_t;

typedef struct {
    __uint32_t	si_magic;
    __uint32_t	si_numsegs;
    promseg_t	si_segs[NUMSEGS];
} seginfo_t;

#endif /* __SYS_EVEREST_PROMHDR_H__ */
