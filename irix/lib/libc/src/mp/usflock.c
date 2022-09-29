/**************************************************************************
 *									  *
 * 		 Copyright (C) 1986-1992 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ident "$Revision: 1.1 $ $Author: jwag $"

#include "synonyms.h"
#include "fcntl.h"
#include "errno.h"
#include "stdlib.h"
#include "ulocks.h"
#include "us.h"

/*
 * _usfreadlock, _usfsplock,  _usfunsplock,  _usfgetlock :
 *
 *	These are routines that lock, unlock and get the status of a filelock.
 * Filelocks are operated on through the use of a flock struct.
 */

/*
 * _usfreadlock - locks the file for read
 * where is the address of the lock, mfd is the
 * file descriptor of the file where the lock is.
 *
 *	returns : 0,  when successful
 *		 -1,  if unable to aquire the lock
 */
int
_usfreadlock(int where, int mfd)
{
	struct flock readlock;
	register int rv;

	do {
		readlock.l_type= F_RDLCK; 
		readlock.l_whence = 0; 
		readlock.l_start= where; 
		readlock.l_len= 1; 
		rv = fcntl(mfd, F_SETLKW, &readlock);
	} while (rv == -1 && oserror() == EINTR);
	if (rv == -1)
		return(-1);
	else
		return(0);
}

/*
 * _usfsplock ( where, mfd )
 * 	locks the file lock. 
 * where is the address of the lock, mfd is the
 * file descriptor of the file where the lock is.
 *
 *	returns : 0,  when successful
 *		 -1,  if unable to aquire the lock
 */
int
_usfsplock(int where, int mfd)
{
	struct flock writelock;
	register int rv;

	do {
		/* spin on lock */
		writelock.l_type= F_WRLCK; 
		writelock.l_whence = 0; 
		writelock.l_start= where; 
		writelock.l_len= 1; 
		rv = fcntl(mfd, F_SETLKW, &writelock);
	} while (rv == -1 && oserror() == EINTR);
	if (rv == -1)
		return(-1);
	else
		return(0);
}

/*
 * _usfunsplock ( where, mfd )
 * 	unlocks the file lock. 
 * where is the address of the lock, mfd is the
 * file descriptor of the file where the lock is.
 *
 *	returns : 0,  when successful
 *		 -1,  if unable to unlock the lock
 */
int
_usfunsplock(int where, int mfd)
{
	struct flock unsetlock;

	unsetlock.l_type = F_UNLCK; 
	unsetlock.l_whence = 0; 
	unsetlock.l_start = where; 
	unsetlock.l_len = 1;
	if ((fcntl (mfd, F_SETLK, &unsetlock)) == -1) {
		if (_uerror)
			_uprint(1, "ERROR: unlock fd %d offset %d pid %d",
				mfd, where, get_pid());
#ifdef DEBUG
		abort();
#endif
		return(-1);
	} else
		return(0);
}

/*
 * _usfgetlock - return status of lock
 *
 *	returns : 0 if unlocked (or caller has it locked)
 *		  > 0 pid of locker
 *		 -1 on error
 */
int
_usfgetlock(int where, int mfd)
{
	struct flock getlock;

	getlock.l_type= F_WRLCK; 
	getlock.l_whence = 0; 
	getlock.l_start = where; 
	getlock.l_len = 1; 
	if ((fcntl(mfd, F_GETLK, &getlock)) == -1) {
		if (_uerror)
			_uprint(1, "ERROR: getlock fd %d offset %d pid %d",
				mfd, where, get_pid());
#ifdef DEBUG
		abort();
#endif
		return(-1);
	} else if (getlock.l_type == F_UNLCK)
		return(0);
	else
		return(getlock.l_pid);
}
