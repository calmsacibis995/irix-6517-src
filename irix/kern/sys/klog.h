/**************************************************************************
 *									  *
 * 		 Copyright (C) 1986, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident	"$Revision: 1.8 $"

#ifndef	_KLOG_H
#define	_KLOG_H

/*
** Thie file contains the definition for the kernel error logging
** message area. The buffer area is maintained as a ring buffer.
*/

#define	KLM_BUFSZ	4096		/* must be a power of 2!! */
#define KLM_BUFMASK	(KLM_BUFSZ-1)

typedef struct
{
	unsigned int	klm_readloc;	/* read location	*/
	unsigned int	klm_writeloc;	/* write location	*/
	char		klm_buffer[ KLM_BUFSZ];	/* da buffer	*/
} klogmsgs_t;

extern klogmsgs_t	klogmsgs;

extern void	klogwakeup(void);
extern int	klog_need_action;
extern void	klog_unlocked_wakeup(void);
#endif
