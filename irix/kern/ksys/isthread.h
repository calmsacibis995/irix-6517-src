
/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1996 Silicon Graphics, Inc.                *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/
#ident	"$Revision: 1.1 $"

#ifndef	__ISTHREAD_H__
#define	__ISTHREAD_H__

/* routines common to ithreads and sthreads */

extern void istswtch(int);
extern void istexitswtch(void);
extern void resumethread(kthread_t *, kthread_t *);

#endif /* __ISTHREAD_H__ */
