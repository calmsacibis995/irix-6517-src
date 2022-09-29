/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libc-port:sys/lockf.c	1.16"
#ifdef __STDC__
	#pragma weak lockf = _lockf
	#pragma weak lockf64 = _lockf64
#endif
#include "synonyms.h"
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include "mplib.h"

int
lockf(int fildes, int function, off_t size)
{
	struct flock l;
	int rv;

	MTLIB_CNCL_TEST();
	l.l_whence = 1;
	if (size < 0) {
		l.l_start = size;
		l.l_len = -size;
	} else {
		l.l_start = 0L;
		l.l_len = size;
	}
	switch (function) {
	case F_ULOCK:
		l.l_type = F_UNLCK;
		rv = fcntl(fildes, F_SETLK, &l);
		break;
	case F_LOCK:
	{	extern int __fcntl(int, int, void *);

		l.l_type = F_WRLCK;
		MTLIB_BLOCK_CNCL_VAL( rv, __fcntl(fildes, F_SETLKW, &l) );
		break;
	}
	case F_TLOCK:
		l.l_type = F_WRLCK;
		rv = fcntl(fildes, F_SETLK, &l);
		break;
	case F_TEST:
		l.l_type = F_WRLCK;
		rv = fcntl(fildes, F_GETLK, &l);
		if (rv != -1) {
			if (l.l_type == F_UNLCK)
				return (0);
			else {
				setoserror(EACCES);
				return (-1);
			}
		}
		break;
	default:
		setoserror(EINVAL);
		return (-1);
	}
	if (rv < 0) {
		switch(errno) {
		case EMFILE:
		case ENOSPC:
		case ENOLCK:
			/* A deadlock error is given if we run out of resources,
			 * in compliance with /usr/group standards.
			 */
			setoserror(EDEADLK);
			break;
		default:
			break;
		}
	}
	return (rv);
}

int
lockf64(int fildes, int function, off64_t size)
{
	struct flock64 l;
	int rv;

	MTLIB_CNCL_TEST();
	l.l_whence = 1;
	if (size < 0) {
		l.l_start = size;
		l.l_len = -size;
	} else {
		l.l_start = 0L;
		l.l_len = size;
	}
	switch (function) {
	case F_ULOCK:
		l.l_type = F_UNLCK;
		rv = fcntl(fildes, F_SETLK64, &l);
		break;
	case F_LOCK:
	{	extern int __fcntl(int, int, void *);

		l.l_type = F_WRLCK;
		MTLIB_BLOCK_CNCL_VAL( rv, __fcntl(fildes,F_SETLKW64,&l) );
		break;
	}
	case F_TLOCK:
		l.l_type = F_WRLCK;
		rv = fcntl(fildes, F_SETLK64, &l);
		break;
	case F_TEST:
		l.l_type = F_WRLCK;
		rv = fcntl(fildes, F_GETLK64, &l);
		if (rv != -1) {
			if (l.l_type == F_UNLCK)
				return (0);
			else {
				setoserror(EACCES);
				return (-1);
			}
		}
		break;
	default:
		setoserror(EINVAL);
		return (-1);
	}
	if (rv < 0) {
		switch(errno) {
		case EMFILE:
		case ENOSPC:
		case ENOLCK:
			/* A deadlock error is given if we run out of resources,
			 * in compliance with /usr/group standards.
			 */
			setoserror(EDEADLK);
			break;
		default:
			break;
		}
	}
	return (rv);
}
