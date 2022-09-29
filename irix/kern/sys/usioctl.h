/**************************************************************************
 *									  *
 * 		 Copyright (C) 1990-1991 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
/*
 * sys/usioctl.h -- structures and defines for poll-able semaphores
 */
#ifndef __USIOCTL_H__
#define __USIOCTL_H__

#ident "$Revision: 1.16 $"

#include "sys/poll.h"

#define USEMADEV	"/dev/usema"
#define USEMACLNDEV	"/dev/usemaclone"
/*
 * Ioctl commands.
 */
#define UIOC	('u' << 16 | 's' << 8)

#define UIOCATTACHSEMA	(UIOC|2)	/* attach to exising sema */
#define UIOCBLOCK	(UIOC|3)	/* block, sync, intr */
#define UIOCABLOCK	(UIOC|4)	/* block, async */
#define UIOCNOIBLOCK	(UIOC|5)	/* block, sync, intr */
#define UIOCUNBLOCK	(UIOC|6)	/* unblock sync */
#define UIOCAUNBLOCK	(UIOC|7)	/* unblock async */
#define UIOCINIT	(UIOC|8)	/* init semaphore async */
#define	UIOCGETSEMASTATE (UIOC|9)	/* get semaphore state */
#define UIOCABLOCKPID	(UIOC|10)	/* block pid, async */
#define UIOCADDPID	(UIOC|11)	/* add pid */
#define UIOCABLOCKQ	(UIOC|12)	/* block, async with kernel queuing */
#define UIOCAUNBLOCKQ 	(UIOC|13)	/* unblock, async with kernel queuing */
#define UIOCIDADDR	(UIOC|14)	/* register address of sem owner field */
#define UIOCSETSEMASTATE (UIOC|15)	/* set semaphore state */
#define UIOCGETCOUNT	(UIOC|16)	/* get semaphore prepost/waiter counts */

typedef struct usattach_s {
	dev_t	us_dev;		/* attach dev */
	void	*us_handle;	/* user level semaphore handle */
} usattach_t;

typedef struct irix5_usattach_s {
	__uint32_t	us_dev;		/* attach dev */
	__uint32_t	us_handle;	/* user level semaphore handle */
} irix5_usattach_t;

typedef struct ussemastate_s {
	int     ntid;           /* Input: number of tid structs */
	int 	nprepost;       /* Input/Return: prepost value */
	int     nfilled;        /* Return: number filled */
	int     nthread;        /* Return: number of threads */
	struct ussematidstate_s {
	        pid_t   pid;
	        int     tid;
	        int     count;
	} *tidinfo;
} ussemastate_t;

typedef struct ussematidstate_s ussematidstate_t;

#endif
