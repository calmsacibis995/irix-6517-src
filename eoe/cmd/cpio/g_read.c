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
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/cpio/RCS/g_read.c,v 1.4 1995/10/03 03:05:27 ack Exp $"
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/


#include <errno.h>
#include <sys/stat.h>
#include "libgenIO.h"
#include "rmt.h"

/*
 * g_read: Read nbytes of data from fdes (of type devtype) and place
 * data in location pointed to by buf.  In case of end of medium,
 * translate (where necessary) device specific EOM indications into
 * the generic EOM indication of rv = -1, errno = ENOSPC.
 */

int
g_read(int devtype, int fdes, char *buf, unsigned nbytes)
{
	int rv;

	if (devtype < 0 || devtype >= G_DEV_MAX) {
		errno = ENODEV;
		return(-1);
	}

	if ((rv = rmtread(fdes, buf, nbytes)) <= 0) {
		switch (devtype) {
			case G_FILE:	/* do not change returns for files */
				break;
			case G_NO_DEV:
				if(!rv) {
					/* this indicates end-of-media with
					 * many devices */
					rv = -1;
					errno = ENOSPC;
				}
				/* else leave errno alone */
				break;
			case G_TAPE:
				/* cpio expects that multivolume is
				 * is denoted by reading an ENOSPC at the
				 * end of the tape, but the driver has
				 * been changed to create a FM so we have
				 * to fake it here.
				 */
				if (!rv) {
					rv = -1;
					errno = ENOSPC;
				}
				break;
			default:
				rv = -1;
				errno = ENODEV;
		} /* devtype */
	} /* (rv = read(fdes, buf, nbytes)) <= 0 */
	return(rv);
}
