/*
 * |-----------------------------------------------------------|
 * | Copyright (c) 1991, 1990 MIPS Computer Systems, Inc.      |
 * | All Rights Reserved                                       |
 * |-----------------------------------------------------------|
 * |          Restricted Rights Legend                         |
 * | Use, duplication, or disclosure by the Government is      |
 * | subject to restrictions as set forth in                   |
 * | subparagraph (c)(1)(ii) of the Rights in Technical        |
 * | Data and Computer Software Clause of DFARS 252.227-7013.  |
 * |         MIPS Computer Systems, Inc.                       |
 * |         950 DeGuigne Avenue                               |
 * |         Sunnyvale, California 94088-3650, USA             |
 * |-----------------------------------------------------------|
 */
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/cpio/RCS/g_init.c,v 1.5 1997/04/21 00:22:28 olson Exp $"
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/


#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mkdev.h>
#include <sys/mtio.h>
#include <sys/statvfs.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include "libgenIO.h"
#include "rmt.h"

/*
 * g_init: Determine the device being accessed, set the buffer size,
 * and perform any device specific initialization. 
 */

int
g_init(int *devtype, int *fdes)
{
	int bufsize;
	struct stat st_buf;
	struct statvfs64 stfs_buf;
	int blksize;

	*devtype = G_NO_DEV;
	bufsize = -1;

	/* 
		Real file or tape (local or remote)???
		If the ioctl completes and returns a non-negative value and
		the blksize is larger than 0 it must be a tape drive.  Else
		is a file or fifo - then set the block size to the best
		size.  NOTE - that blksize from MTIOCGETBLKSIZE is not bytes,
		but tape blocksize in 512 bytes multiples (see sys/mtio.h).
	*/ 

	if((rmtioctl(*fdes, MTIOCGETBLKSIZE, &blksize) >= 0) && blksize > 0) {
			*devtype = G_TAPE;	 /* Tape drive (remote or local) */
			bufsize = 512*blksize;
	} else {
		if (rmtfstat(*fdes, &st_buf) == -1)
			return(bufsize);
		if (S_ISFIFO(st_buf.st_mode)) {		/* fifo */
			bufsize = 512;
		}
		else if(S_ISCHR(st_buf.st_mode) || S_ISBLK(st_buf.st_mode)) {
			/* leave devtype G_NO_DEV, even though it's a device,
			 * so errno, etc. are left alone; allows multivolume
			 * to floppies, rawdisk, etc. */
			if(!(bufsize = st_buf.st_blksize))
				bufsize = 512;
		} else {
			*devtype = G_FILE; 
			/* find bufsize of filesystem */
			if (fstatvfs64(*fdes, &stfs_buf) < 0) 
				errno = ENODEV;
			else
				bufsize = (int)stfs_buf.f_bsize;
		}
	}

	return(bufsize);
}
