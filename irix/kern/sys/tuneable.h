/**************************************************************************
 *									  *
 * 		 Copyright (C) 1989-1993 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/
#ifndef __SYS_TUNEABLE_H__
#define __SYS_TUNEABLE_H__

#ident	"$Revision: 3.18 $"

typedef struct tune {
	uint	t_gpgslo;	/* If freemem < t_gpgslo, then start	*/
				/* to steal pages from processes.	*/
	uint	t_gpgshi;	/* Once we start to steal pages, don't	*/
				/* stop until freemem > t_gtpgshi.	*/
	uint	t_gpgsmsk;	/* Mask used by getpages to determine	*/
				/* whether a page is stealable.  The	*/
				/* page is stealable if the nr counter	*/
				/* in the page table is less than	*/
				/* t_gpgsmsk.  Possible values are:	*/
				/*   >=7	- steal any valid page.	*/
				/*   0-6	- steal page if not	*/
				/*	referenced in (7 - mask) * 	*/
				/*	t_vhandr seconds.		*/
				/* Every page table's nr counter is	*/
				/* decremented every time getpages runs,*/
				/* and is reset to 7 when referenced.	*/
	uint	t_maxsc;	/* The maximum number of pages which	*/
				/* will be swapped out in a single	*/
				/* operation.  Cannot be larger than	*/
				/* MAXPGLST.				*/
	uint	t_maxfc;	/* The maximum number of pages which	*/
				/* will be saved up and freed at once.	*/
				/* Cannot be larger than MAXPGLST   	*/
	uint	t_maxdc;	/* The maximum number of pages which	*/
				/* will be saved up and written to	*/
				/* disk (mappd files) at once.		*/
				/* Cannot be larger than MAXPGLST   	*/
	uint	t_unused1;	/* Used to be maxumem which is now	*/
				/* controlled via RLIMIT_VMEM		*/
	uint	t_bdflushr;	/* bdflushr is used to compute what	*/
				/* fraction (nbuf/bdflushr) of the	*/
				/* buffers are examined by bdflush when	*/
				/* it wakes up to determine whether the	*/
				/* delayed-write, aged buffers should be*/
				/* flushed to disk.			*/
	uint	t_minarmem;	/* The minimum available resident (not	*/
				/* swapable) memory to maintain in 	*/
				/* order to avoid deadlock.  In pages.	*/
	uint	t_minasmem;	/* The minimum available swapable	*/
				/* memory to maintain in order to avoid	*/
				/* deadlock.  In pages.			*/
	uint	t_maxlkmem;	/* The maximum amount of lockable pages */
				/* per process (via mpin).		*/
	uint	t_tlbdrop;	/* Number of ticks before a procs wired	*/
				/* (second-level) entries are flushed	*/
	uint	t_rsshogfrac;	/* Fraction of memory RSS hogs can use	*/
	uint	t_rsshogslop;	/* # pages excess of RSS before slow 	*/
				/* down process 			*/
	uint	t_dwcluster;	/* Maximum number of delayed write	*/
				/* pages to cluster in each push.	*/
	uint	t_autoup;	/* The age a delayed-write buffer must	*/
				/* be in seconds before bdflush will	*/
				/* write it out.			*/
} tune_t;

extern tune_t	tune;


/*	The following defines are used by the sysmips system call.
 *	If the first argument to the syssgi system call is SGI_TUNE,
 *	then the call pertains to the swap file.  In this case,
 *	the third argument is a pointer to a tune structure.
 *	The second argument specifies the action.
 */
#define TUNE_RD	1	/* Read system tune structure.	*/
			/* System tune structure is	*/
			/* copied to user's buffer.	*/
#define TUNE_WR	2	/* Write system tune structure.	*/
			/* system tune structure is	*/
			/* copied from user's buffer.	*/
#endif /* __SYS_TUNEABLE_H__ */
