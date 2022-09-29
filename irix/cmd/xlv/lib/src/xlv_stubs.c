/**************************************************************************
 *                                                                        *
 *            Copyright (C) 1993-1994, Silicon Graphics, Inc.             *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/
#ident "$Revision: 1.10 $"

#include <stdio.h>
#include <stdlib.h>
#include <bstring.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <fcntl.h>
#include <sys/xlv_base.h>
#include <sys/major.h>
#include <sys/debug.h>
#include <assert.h>


/*
 * Need a definition for assfail and doass in case someone wants a debug
 * module but not a debug library.
 */
int doass = 1;

/* ARGSUSED */
void
assfail (char *err_txt, char *file, int line_no)
{
    fprintf (stderr, "Internal error in %s at line %d\n",
             file, line_no);
    abort();
}


/*
 * If the device name is invalid, then (0,0) is returned.
 *
 * Note: This is not intended for XLV device names.
 */
dev_t
name_to_dev (char *dev_name)
{
    struct stat sb;

    if (stat(dev_name, &sb) < 0)
    {
	return (makedev(0,0));
    }
    else
    {
	/*
	 * XXX Should check to make sure that the major number is valid.
	 */
	return (sb.st_rdev);
    }
}
