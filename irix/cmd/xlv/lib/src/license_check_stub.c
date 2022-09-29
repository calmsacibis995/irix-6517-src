/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1994, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/
#ident "$Revision: 1.8 $"

#include <sys/stat.h>
#include <stdio.h>

#define XLVPLEX_LICPATH		"/root/etc/flexlm/license.dat"
#define XLVPLEX_NETLS_LICPATH	"/root/etc/nodelock"

/*
 * Stubbed version of check_plexing_license() for use by the miniroot
 * versions of the XLV utilities.
 *
 * We simply check for the existence of the nodelock file,
 * and whether we're actually in the miniroot.
 */
/* ARGSUSED0 */
int
check_plexing_license(char *root)
{
	struct stat statbuf, statbuf2;

	/*
	 * first see if the nodelock file exists
	 */

	if ((stat(XLVPLEX_LICPATH, &statbuf) < 0)
	    && (stat(XLVPLEX_NETLS_LICPATH, &statbuf) < 0)) {
		return (0);
	}

	/*
	 * See if we are in the miniroot.
	 * ie. /dev/root == /dev/swap
	 */

	if (stat("/dev/root", &statbuf) < 0 ||
	    stat("/dev/swap", &statbuf2) < 0) {
		return (0);
	}

	if (statbuf.st_rdev != statbuf2.st_rdev) {
		return (0);
	}

	return(1);
}
