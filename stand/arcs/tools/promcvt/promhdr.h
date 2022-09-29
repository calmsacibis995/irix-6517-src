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
 * promhdr.h -- Format for the header information encoded at the
 * 	beginning of the IO4 EPC flash EPROM. 
 */

#ifndef __SYS_EVEREST_PROMHDR_H__
#define __SYS_EVEREST_PROMHDR_H__

#ident "$Revision: 1.1 $"

#define PROM_MAGIC	0x4a464b34

typedef struct {
    uint	prom_magic;		/* Magic number */
    uint	prom_cksum;		/* Checksum of IO4 PROM */
    uint	prom_startaddr;		/* Load address for the IO4 PROM */
    uint	prom_length;		/* Number of bytes in PROM */
    uint	prom_entry;		/* PROM entry point */
    uint	prom_version;		/* PROM revision number */
} evpromhdr_t;

#endif /* __SYS_EVEREST_PROMHDR_H__ */
