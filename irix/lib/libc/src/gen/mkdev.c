/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libc-port:gen/mkdev.c	1.4"

#include	"synonyms.h"
#include	<sys/types.h>
#include	<errno.h>
#include	<sys/mkdev.h>

/* create a formatted device number */

dev_t
__makedev(version, majdev, mindev)
register const int	version;
register const major_t	majdev;
register const minor_t mindev;
{
dev_t devnum;
	switch(version){

		case OLDDEV:
			if  (majdev > OMAXMAJ || mindev > OMAXMIN) {
				o_dev_t rtv;

				setoserror(EINVAL);
				rtv = (o_dev_t) NODEV;
				return ((dev_t)rtv);
			}
			devnum = ((majdev << ONBITSMINOR) | mindev);
			break;

		case NEWDEV:
			if (majdev > MAXMAJ || mindev > MAXMIN) {
				setoserror(EINVAL);
				return (NODEV);
			}

			if ((devnum = ((majdev << NBITSMINOR) | mindev)) == NODEV){
				setoserror(EINVAL);
				return (NODEV);
			}

			break;

		default:
			setoserror(EINVAL);
			return (NODEV);
			
	}

	return(devnum);
}

/* return major number part of formatted device number */

major_t
__major(const int version, const dev_t devnum)
{
major_t maj;

	switch(version) {

		case OLDDEV:

			maj = (devnum >> ONBITSMINOR);
			if (devnum == NODEV || maj > OMAXMAJ) {
				setoserror(EINVAL);
				return (NODEV);
			}
			break;

		case NEWDEV:
			maj = (devnum >> NBITSMINOR);
			if (devnum == NODEV || maj > MAXMAJ) {
				setoserror(EINVAL);
				return (NODEV);
			}
			break;

		default:

			setoserror(EINVAL);
			return (NODEV);
	}

	return (maj);
}


/* return minor number part of formatted device number */

minor_t
__minor(const int version, const dev_t devnum)
{

	switch(version) {

		case OLDDEV:

			if (devnum == NODEV) {
				setoserror(EINVAL);
				return(NODEV);
			}
			return(devnum & OMAXMIN);
			/*NOTREACHED*/
			break;

		case NEWDEV:

			if (devnum == NODEV) {
				setoserror(EINVAL);
				return(NODEV);
			}
			return(devnum & MAXMIN);
			/*NOTREACHED*/
			break;

		default:

			setoserror(EINVAL);
			return(NODEV);
	}
}
