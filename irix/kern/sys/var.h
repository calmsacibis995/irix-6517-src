/**************************************************************************
 *									  *
 * 		 Copyright (C) 1989-1993, Silicon Graphics, Inc.	  *
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
#ifndef __SYS_VAR_H__
#define __SYS_VAR_H__

#ident	"$Revision: 3.32 $"

/*
 * System Configuration Information
 */
struct var {
	int	v_buf;		/* Nbr of I/O buffers.			*/
	int	fill_1;
	int	v_inode;	/* Desired number of vnodes to cache	*/
	int 	fill_3;
	int 	fill_4;
	int 	fill_5;
	int 	fill_6;
	int 	fill_7;
	int	v_proc;		/* Size of proc table.			*/
	int	fill_10;
	int	fill_11;	/* Max number of processes per user.	*/
	int	v_hbuf;		/* Nbr of hash buffers to allocate.	*/
	int	v_hmask;	/* Hash mask for buffers.		*/
	int	fill_14;
	int	v_maxpmem;	/* The maximum physical memory to use.	*/
				/* If v_maxpmem == 0, then use all	*/
				/* available physical memory.		*/
				/* Otherwise, value is amount of mem to	*/
				/* use specified in pages.		*/
	int	v_maxdmasz;	/* Max dma unbroken dma transfer size.	*/
	int	fill7;
	int	v_nqueue;	/* Nbr of streams queues.		*/
	int	v_nstream;	/* Number of stream head structures.	*/
	int	fill8;
	int	fill9;
	int	fill10;
	int	fill11;
	int	fill12;
	int	fill13;
	int	fill14;
	int	fill15;
	int	fill16;
	int	fill17;
	int	v_dquot;	/* size of incore dquot table */
	char *	ve_dquot;	/* Ptr to end of incore dquot */
};

#ifdef _KERNEL
extern struct var v;
extern int syssegsz;
extern int maxdmasz;
extern int nproc;
#endif /* _KERNEL */

#endif /* __SYS_VAR_H__ */
