/*
 *=============================================================================
 *                      File:           part.h
 *                      Purpose:        The Partition table manipulator.
 *					Misc. Macros and declarations.
 *=============================================================================
 */
#ifndef	_FDISK_H
#define _FDISK_H

#include <sys/ioctl.h>
#include <sys/bsd_types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <ctype.h>
#include <setjmp.h>
#include <errno.h>
#include "misc.h"

/*
 * Partition table entry structure
 * Please note that the starting and ending
 * cylinder fields are actually 10 bits, and 
 * the starting and ending sector fields are
 * actually 6 bits.
 */
typedef struct pte {
    unsigned char 	sys_ind;      
    unsigned char 	bhead;         
    unsigned char	bsect;		/*  6 bits */
    unsigned char  	bcyl;		/* 10 bits */
    unsigned char 	type;         
    unsigned char 	ehead;     
    unsigned char 	esect;		/*  6 bits */
    unsigned char	ecyl;		/* 10 bits */
    unsigned char 	start_sect_ll;
    unsigned char 	start_sect_lh;
    unsigned char 	start_sect_hl;
    unsigned char 	start_sect_hh;
    unsigned char 	numbr_sects_ll;
    unsigned char 	numbr_sects_lh;
    unsigned char 	numbr_sects_hl;
    unsigned char 	numbr_sects_hh;
} pte_t;

#define	NUM_PRIM_PARTS	(4)
#define MAX_PARTNS	(60)
#define	PTE_EXTENDED	(5)
#define	PTE_HUGE	(6)
#define	PD_OFFSET	(0x1BE)
#define ACTIVE_FLAG     (0x80)

#define	part_flag_set(buffer)		buffer[510] = 0x55; buffer[511] = 0xaa
				  
#define	is_active(p)			(p->type)
#define	is_free(p)			(!p->type)
#define	hole_size(h1, c1, c2)		(part_size(h1, c1, c2)*SECTOR_SIZE)
#define	part_size(h1, c1, c2)		(sectors*((heads-h1)+heads*(c2-c1)))
#define	part_begin(h1, c1)		(sectors*(c1*heads+h1))
#define	part_end(c2)        		(sectors*(c2+1)*heads)

#define	set_bhead(p, head)		(p->bhead = head)
#define	set_ehead(p, head)		(p->ehead = head)

#define	get_bhead(p)			(p->bhead)
#define	get_ehead(p)			(p->ehead)


#define	get_start_sect(p)      ((p->start_sect_ll)        |\
                                (p->start_sect_lh << 8)   |\
                                (p->start_sect_hl << 16)  |\
                                (p->start_sect_hh << 24))

#define	get_numbr_sect(p)      ((p->numbr_sects_ll)       |\
                                (p->numbr_sects_lh << 8)  |\
                                (p->numbr_sects_hl << 16) |\
                                (p->numbr_sects_hh << 24))

#define	p_start_sect(p)	       ((p->start_sect_ll)        |\
				(p->start_sect_lh << 8)   |\
				(p->start_sect_hl << 16)  |\
				(p->start_sect_hh << 24)) * SECTOR_SIZE

#define p_numbr_sect(p)	       ((p->numbr_sects_ll) 	  |\
				(p->numbr_sects_lh << 8)  |\
				(p->numbr_sects_hl << 16) |\
				(p->numbr_sects_hh << 24))* SECTOR_SIZE

#define	set_start_sect(p, offset)				          \
			p->start_sect_ll = ((0x000000FF & offset));       \
			p->start_sect_lh = ((0x0000FF00 & offset) >> 8);  \
			p->start_sect_hl = ((0x00FF0000 & offset) >> 16); \
			p->start_sect_hh = ((0xFF000000 & offset) >> 24)

#define	set_numbr_sect(p, number)				          \
			p->numbr_sects_ll = ((0x000000FF & number));      \
			p->numbr_sects_lh = ((0x0000FF00 & number) >> 8); \
			p->numbr_sects_hl = ((0x00FF0000 & number) >> 16);\
			p->numbr_sects_hh = ((0xFF000000 & number) >> 24)

#define pte_offset(b, n) ((struct pte *)((b) + 0x1BE + (n)*sizeof(struct pte)))

#endif
