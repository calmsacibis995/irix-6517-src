/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: lock.c,v 65.4 1998/03/23 16:26:41 gwehrman Exp $";
static void _identFunc(const char *x) {if(!x)_identFunc(_identString);}
#endif


/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/*
 * HISTORY
 * $Log: lock.c,v $
 * Revision 65.4  1998/03/23 16:26:41  gwehrman
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS && !KERNEL
 *
 * Revision 65.3  1997/12/16 17:05:39  lmc
 *  1.2.2 initial merge of file directory
 *
 * Revision 65.2  1997/11/06  19:58:24  jdoak
 * Source Identification added.
 *
 * Revision 65.1  1997/10/20  19:17:41  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.626.1  1996/10/02  18:10:10  damon
 * 	Newest DFS from Transarc
 * 	[1996/10/01  18:44:44  damon]
 *
 * Revision 1.1.621.2  1994/06/09  14:16:46  annie
 * 	fixed copyright in src/file
 * 	[1994/06/09  13:28:57  annie]
 * 
 * Revision 1.1.621.1  1994/02/04  20:26:02  devsrc
 * 	Merged from 1.0.3a to 1.1
 * 	[1994/02/04  15:16:13  devsrc]
 * 
 * Revision 1.1.619.1  1993/12/07  17:30:45  jaffe
 * 	1.0.3a update from Transarc
 * 	[1993/12/06  13:43:27  jaffe]
 * 
 * $EndLog$
 */

/* Copyright (C) 1993, 1990, 1989 Transarc Corporation - All rights reserved */

#define DEBUG_THIS_FILE OSI_DEBUG_LOCK

#include <dcedfs/param.h>
#include <dcedfs/stds.h>
#include <dcedfs/debug.h>
#include <dcedfs/osi.h>
#include <dcedfs/lock.h>

RCSID("$Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/osi/RCS/lock.c,v 65.4 1998/03/23 16:26:41 gwehrman Exp $")

int afsdb_osi = 0;

/*
 * Generic routines common to all lock package implementations.
 */

/*
 * obtain the specified type of lock
 */
void
lock_Obtain(osi_dlock_t *lock, int lockType)
{
    switch (lockType) {
      case READ_LOCK:
	lock_ObtainRead(lock);
	break;
      case WRITE_LOCK:
	lock_ObtainWrite(lock);
	break;
      case SHARED_LOCK:
	lock_ObtainShared(lock);
	break;
      case BOOSTED_LOCK:
	lock_UpgradeSToW(lock);
	break;
      default:
	panic("lock_Obtain");
    }
}

/*
 * release a lock, taking the type as a parameter.
 */
void
lock_Release(osi_dlock_t *lockp, int type)
{
    switch(type) {
      case READ_LOCK:
	lock_ReleaseRead(lockp);
	break;
      case WRITE_LOCK:
	lock_ReleaseWrite(lockp);
	break;
      case SHARED_LOCK:
	lock_ReleaseShared(lockp);
	break;
      default:
	panic("lock_Release");
    }
}

/*
 * obtain the specified type of lock without blocking on the lock
 */
int
lock_ObtainNoBlock(osi_dlock_t *lock, int lockType)
{
    switch (lockType) {
      case READ_LOCK:
	return lock_ObtainReadNoBlock(lock);
      case WRITE_LOCK:
	return lock_ObtainWriteNoBlock(lock);
      case SHARED_LOCK:
	return lock_ObtainSharedNoBlock(lock);
      case BOOSTED_LOCK:
	return lock_UpgradeSToWNoBlock(lock);
      default:
	panic("lock_ObtainNoBlock");
	return 0;
	/* NOTREACHED */
    }
}
