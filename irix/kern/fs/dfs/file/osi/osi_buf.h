/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/*
 * HISTORY
 * $Log: osi_buf.h,v $
 * Revision 65.1  1997/10/20 19:17:42  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.520.1  1996/10/02  18:10:24  damon
 * 	Newest DFS from Transarc
 * 	[1996/10/01  18:44:47  damon]
 *
 * Revision 1.1.515.2  1994/06/09  14:16:50  annie
 * 	fixed copyright in src/file
 * 	[1994/06/09  13:29:02  annie]
 * 
 * Revision 1.1.515.1  1994/02/04  20:26:13  devsrc
 * 	Merged from 1.0.3a to 1.1
 * 	[1994/02/04  15:16:17  devsrc]
 * 
 * Revision 1.1.513.1  1993/12/07  17:30:51  jaffe
 * 	1.0.3a update from Transarc
 * 	[1993/12/03  16:10:07  jaffe]
 * 
 * $EndLog$
 */

/* Copyright (C) 1993, 1992 Transarc Corporation - All rights reserved */

/* $Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/osi/RCS/osi_buf.h,v 65.1 1997/10/20 19:17:42 jdoak Exp $ */

#ifndef OSI_BUF_H
#define	OSI_BUF_H

#include <dcedfs/param.h>
#include <dcedfs/lock.h>

/* Defines some generic system buf handling facilities.  Most of these are
 * defined in the osi_buf_mach. file included below.  The synchronization
 * primitives, however, are different in kernel and user space.  The user space
 * implementation, which is system-independent, appears below.  The
 * system-specific kernel code is in the appropriate osi_buf_mach.h file. */

#include <dcedfs/osi_buf_mach.h>

/* It would be nice to use a condition variable in the bp but I can't do that
 * without modifying the system struct bufs.  Use a global cv instead. */

extern osi_cv_t osi_biowaitCond;
extern osi_mutex_t osi_iodoneLock;

extern void osi_BufInit(
  struct buf *bp,
  int flags,
  caddr_t addr,
  size_t length,
  dev_t device,
  daddr_t dblkno,
  struct vnode *vp,
  osi_iodone_t (*iodone)(struct buf *)
);

#ifndef KERNEL

/* Used only for test_anode from test_vm*.c.  A special macro that provides
 * uniform read-only access to bufsize */

extern u_long vm_pageSize;
#define b_Bufsize(b) (vm_pageSize+0)

#define osi_bio_wakeup(bp, cv) osi_cv_broadcast(cv)
    
#define osi_bio_init(bp)

#define osi_biodone(bp, cv) \
    MACRO_BEGIN \
	bp->b_flags |= B_DONE; \
	osi_bio_wakeup(bp, cv); \
    MACRO_END

#define osi_bio_cleanup(bp, cv) \
    MACRO_BEGIN \
	(bp)->b_flags |= B_DONE; \
	if ((bp)->b_flags & B_ASYNC) \
	    osi_ReleaseBuf(bp); \
    MACRO_END

/* osi_bio_wait -- drops "splbio" and sleeps atomically, then resumes "splbio"
 *     before returning.  This should be called at least once on each buf.
 *     Therefore it is safe to call even if the I/O has completed.  On SunOS5
 *     this is implemented as a semaphore so no spurious wakeups occur and we
 *     should not be called more than once.  On other kernels this may return
 *     spuriously and multiple calls are okay.
 *
 * LOCK USED -- Calls to this function must be bracketed by splbio/splx. */

#define osi_bio_wait(bp, cv, mutex) \
    MACRO_BEGIN \
	if (!((bp)->b_flags & B_DONE)) \
	    osi_cv_wait(cv, mutex); \
    MACRO_END

#endif /* ! KERNEL */

#endif /* OSI_BUF_H */
