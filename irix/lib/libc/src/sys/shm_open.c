/*************************************************************************
*                                                                        *
*               Copyright (C) 1995 Silicon Graphics, Inc.       	 *
*                                                                        *
*  These coded instructions, statements, and computer programs  contain  *
*  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
*  are protected by Federal copyright law.  They  may  not be disclosed  *
*  to  third  parties  or copied or duplicated in any form, in whole or  *
*  in part, without the prior written consent of Silicon Graphics, Inc.  *
*                                                                        *
**************************************************************************/

#ifdef __STDC__
#pragma weak shm_open = _shm_open
#endif

#include "synonyms.h"
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

int
shm_open(const char *name, int oflag, mode_t mode)
{
	register int fd;

	if ((fd = open(name, oflag, mode)) < 0) {
		/* POSIX compliance error code remap */
		if ((oserror() == ENOENT) && (oflag & O_CREAT))
			setoserror(EINVAL);
		return -1;
	}

	if (fcntl(fd, F_SETFD, FD_CLOEXEC) == -1) {
		(void) close(fd);
		setoserror(EINVAL);
		return -1;
	}

	return (fd);	
}
