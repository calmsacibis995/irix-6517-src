/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/*
 * HISTORY
 * $Log: osi_intr.h,v $
 * Revision 65.1  1997/10/20 19:17:43  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.524.1  1996/10/02  18:10:41  damon
 * 	Newest DFS from Transarc
 * 	[1996/10/01  18:44:56  damon]
 *
 * Revision 1.1.519.2  1994/06/09  14:16:57  annie
 * 	fixed copyright in src/file
 * 	[1994/06/09  13:29:08  annie]
 * 
 * Revision 1.1.519.1  1994/02/04  20:26:25  devsrc
 * 	Merged from 1.0.3a to 1.1
 * 	[1994/02/04  15:16:21  devsrc]
 * 
 * Revision 1.1.517.1  1993/12/07  17:30:58  jaffe
 * 	1.0.3a update from Transarc
 * 	[1993/12/03  16:11:16  jaffe]
 * 
 * $EndLog$
 */

/* Copyright (C) 1993, 1992 Transarc Corporation - All rights reserved */

/* $Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/osi/RCS/osi_intr.h,v 65.1 1997/10/20 19:17:43 jdoak Exp $ */

#ifndef TRANSARC_OSI_INTR_H
#define	TRANSARC_OSI_INTR_H

#include <dcedfs/param.h>
#include <dcedfs/lock.h>

/* Interrupt level stuff used by asevent and osi_bio. */

/* Most kernels implement these function by raising the current processes
 * interrupt priority, thereby deferring I/O device completion interrupts.  In
 * the Solaris kernel we just use a regular mutex.  A similar approach will
 * likely be necessary in other kernels supporting MP architectures. */

/* In user space we provide mutual exclusion using a write lock.  This lock is
 * held in us_io.c:FinishIO when it simulates an I/O completion event,
 * especially around the call to (*bp->b_iodone). */

/* It is far from clear that the osi_splclock defns are used or correctly
 * supported.  I didn't define them for the user-space versions. */

#ifdef KERNEL

#include <dcedfs/osi_intr_mach.h>

#else /* ! KERNEL */

#define osi_intr_exclude(mutex, cookie) osi_mutex_enter(mutex)
#define osi_intr_allow(mutex, cookie)	osi_mutex_exit(mutex)
#define osi_intr_start(mutex)		/* NULL */
#define osi_intr_end(mutex)		/* NULL */

#define osi_intr_wait(cv, mutex)	osi_cv_wait(cv, mutex)
#define osi_intr_wakeup(cv)		osi_cv_broadcast(cv)

#endif

#endif /* TRANSARC_OSI_INTR_H */
