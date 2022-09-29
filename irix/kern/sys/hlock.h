#ifndef __SYS_HLOCK_H__
#define __SYS_HLOCK_H__

/**************************************************************************
 *									  *
 * 		 Copyright (C) 1988-1992 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident "$Revision: 3.12 $"

/*
 * sys/hardlocks.h -- structures and defines for hardware spinlock allocator
 */

/*
 * Ioctl commands.
 */

/* Allocate lock group. */
#define HL_AMNR	( ('h'<<24) | ('l'<<16) | ('a'<<8) | 'g' )

/* Allocate page of locks. */
#define HL_ALCK	( ('h'<<24) | ('l'<<16) | ('a'<<8) | 'l' )

/* Return size of lock region */
#define HL_RGSZ	( ('h'<<24) | ('l'<<16) | ('s'<<8) | 'r' )

#ifdef _KERNEL

/* Max pages of locks per group (64 locks per page) */
#define HL_MAXPGLCK	256

/*
 * An hlgrp stucture is allocated to each lock group.
 */
typedef struct hlgrp {
	int		hlg_grp;	/* group id	*/
	struct hlgrp	*hlg_next;	/* next group	*/
	pgno_t		hlg_npgs;	/* # pages of locks allocated */
	caddr_t		hlg_pt[HL_MAXPGLCK];	/* page table for group */
} hlgrp_t;

#endif	/* _KERNEL */
#endif /* __SYS_HLOCK_H__ */
