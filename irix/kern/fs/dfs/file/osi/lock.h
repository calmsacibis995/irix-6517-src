/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/*
 * HISTORY
 * $Log: lock.h,v $
 * Revision 65.1  1997/10/20 19:17:41  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.627.1  1996/10/02  18:10:13  damon
 * 	Newest DFS from Transarc
 * 	[1996/10/01  18:44:45  damon]
 *
 * Revision 1.1.622.2  1994/06/09  14:16:47  annie
 * 	fixed copyright in src/file
 * 	[1994/06/09  13:28:58  annie]
 * 
 * Revision 1.1.622.1  1994/02/04  20:26:05  devsrc
 * 	Merged from 1.0.3a to 1.1
 * 	[1994/02/04  15:16:14  devsrc]
 * 
 * Revision 1.1.620.1  1993/12/07  17:30:47  jaffe
 * 	1.0.3a update from Transarc
 * 	[1993/12/06  13:43:41  jaffe]
 * 
 * $EndLog$
 */

/* $Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/osi/RCS/lock.h,v 65.1 1997/10/20 19:17:41 jdoak Exp $ */

/* Copyright (C) 1990, 1994 Transarc Corporation - All rights reserved */

#ifndef TRANSARC_LOCK_H
#define	TRANSARC_LOCK_H	    1

/*
 * Definitions for the OSI locking package.  This package uses a
 * "data lock" (type osi_dlock_t) as its primitive.  The data lock
 * is a reader/writer lock with an additional "shared" state that
 * allows a reader to declare its intention to upgrade to a writer
 * lock in the future.  Only one reader at a time may be in the
 * shared state.
 *
 * Interfaces are provided for the acquisition and release of reader,
 * writer, and shared locks, and for transitions from one lock state
 * to another.  The interfaces for acquiring locks have both blocking
 * and nonblocking versions.
 * 
 * Underlying these interfaces are (at least) three distinct
 * implementations:
 *
 * 1) osi_lock_single.c contains an implementation for traditional
 * Unix kernels that use sleep and wakeup for synchronization; for
 * such kernels, OSI_SINGLE_THREADED should be defined to be true
 * in osi_lock_mach.h.
 *
 * 2) For multithreaded kernels, an implementation should be supplied
 * in the OS-specific subdirectory, and  OSI_SINGLE_THREADED should set
 * to false.  We do this for SunOS, in order to make use of its native
 * locking mechanisms.
 *
 * 3) osi_lock_pthread.c contains a user-level implementation based
 * on pthreads.
 *
 * The file lock.c contains a few generic routines that are common
 * to all implementations.
 */
 
#include <dcedfs/stds.h>
#include <dcedfs/osi.h>

#define READ_LOCK	1
#define WRITE_LOCK	2
#define SHARED_LOCK	4

/*
 * this next is not a flag, but rather a parameter to Lock_Obtain
 */
#define BOOSTED_LOCK	6

#define LOCK_NOLOCK	0	/* failed to get lock */

/*
 * next defines wait_states for which we wait on excl_locked
 */
#define EXCL_LOCKS	(WRITE_LOCK | SHARED_LOCK)

#include <dcedfs/osi_lock_mach.h>
#if !defined(KERNEL)
#include <dcedfs/osi_lock_pthread.h>
#elif OSI_SINGLE_THREADED
#include <dcedfs/osi_lock_single.h>
#endif

/*
 * generic locking routines exported by lock.c:
 */
extern void lock_Obtain(osi_dlock_t *lock, int lockType);
extern void lock_Release(osi_dlock_t *lock, int lockType);
extern int lock_ObtainNoBlock(osi_dlock_t *lock, int lockType);

#endif /* !_TRANSARC_LOCK_H */
