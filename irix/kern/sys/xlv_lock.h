#ifndef __SYS_XLV_LOCK_H__
#define __SYS_XLV_LOCK_H__

/**************************************************************************
 *                                                                        *
 *            Copyright (C) 1993-1994, Silicon Graphics, Inc.             *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/
#ident "$Revision: 1.5 $"

#ifdef _KERNEL

#ifndef _XLV_MEM_H
typedef struct {
    buf_t		 *head;
    buf_t		 *tail;
    unsigned int	  count;
    unsigned short	  n_queued;
    unsigned short	  max_queued;
} buf_queue_t;
#endif

struct xlv_stat_s;

/*
 * Assuming 128 byte cache line, pad out the lock structure to cache
 * boundaries.  NOTE: If you change the lock structure below, be sure to
 * change this macro as well.
 */
#define XLV_CACHEPADSIZE	128 - sizeof(lock_t) - \
				3 * sizeof(unsigned int) - \
				sizeof(sema_t) - 2 * sizeof(buf_queue_t) - \
				2 * sizeof(void *)
typedef struct {
    lock_t		  lock;
    unsigned int	  error_count;
    unsigned int	  io_count;
    unsigned int	  wait_count;
    sema_t		  wait_sem;
    buf_queue_t		  error_queue;
    buf_queue_t		  reissue_queue;
    struct xlv_stat_s	  *statp;
    void		  *clientdata;
    char		  io_pad[XLV_CACHEPADSIZE];
} xlv_lock_t;

extern xlv_lock_t xlv_io_lock[];

#endif /* KERNEL */

#endif /* SYS_XLV_LOCK_H__ */
