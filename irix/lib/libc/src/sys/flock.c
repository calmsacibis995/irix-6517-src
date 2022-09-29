/**************************************************************************
 *									  *
 * 		 Copyright (C) 1989, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ident "$Revision: 1.13 $"

/*
 * 4.xBSD flock(2) compatibility.
 */

#ifdef __STDC__
	#pragma weak flock = _flock
#endif
#include "synonyms.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/file.h>

int
flock(int fd, int operation)
{
	struct flock l;

	if (operation & LOCK_EX) {
		l.l_type = F_WRLCK;
	} else if (operation & LOCK_SH) {
		l.l_type = F_RDLCK;
	} else if (operation == LOCK_UN) {
		l.l_type = F_UNLCK;
	} else {
		setoserror(EINVAL);
		return -1;
	}
	l.l_whence = 0;
	l.l_start = l.l_len = 0;
	if (operation & LOCK_NB) {
		if (fcntl(fd, F_SETBSDLK, &l) < 0) {
			if (errno == EACCES || errno == EAGAIN)
				setoserror(EWOULDBLOCK);
			return -1;
		}
		return 0;
	}
	return fcntl(fd, F_SETBSDLKW, &l);
}
