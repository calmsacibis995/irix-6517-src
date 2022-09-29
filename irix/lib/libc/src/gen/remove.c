 /*************************************************************************
 #									  *
 # 		 Copyright (C) 1989, Silicon Graphics, Inc.		  *
 #									  *
 #  These coded instructions, statements, and computer programs  contain  *
 #  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 #  are protected by Federal copyright law.  They  may  not be disclosed  *
 #  to  third  parties  or copied or duplicated in any form, in whole or  *
 #  in part, without the prior written consent of Silicon Graphics, Inc.  *
 #									  *
 #************************************************************************/

#ident "$Revision: 1.9 $"

/*  remove.c 1.0 */

#include "synonyms.h"
#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>

#include <unistd.h>		/* for prototyping */
#include <strings.h>		/* for prototyping */
#include <errno.h>
#include <limits.h>

/*
 * remove() is not defined identically to unlink in XPG3.
 * This is the exact definition.
 *
 * Code to "translate" symbolic link pathname information has been added
 * per SVID.  This code determines whether a symbolic link chain is
 * "infinite".  We do not currently check to see that if the symbolic link
 * chain ends at a directory, whether the directory is empty.
 *
 * The most recent SVR4 source (ES_MP EA5) does not have this algorithm
 * although the man page is the same as SVID remove(BA_OS).
 */

int
remove(filename)
const char *filename;
{
        struct stat64     statb;
	int count;

	if (lstat64(filename, &statb) != 0)
		return(-1);

	if ((statb.st_mode & S_IFMT) == S_IFLNK) {
		/*
		 * "Translate" the name into its final path.
		 */
		char tfile[2][PATH_MAX];
		struct stat64 nstatb;

		strcpy(tfile[0], filename);
		for (count = 0; count < MAXSYMLINKS; count++) {
			/*
			 * Use two buffers, swap back and forth.
			 */
			int ix0 = count & 1;
			int ix1 = ix0 ? 0 : 1;
			
			/*
			 * Read the link
			 */
			if (readlink(tfile[ix0], (void *)tfile[ix1],
						sizeof(PATH_MAX)) == -1)
				return(-1);
			
			/*
			 * Stat the file.  If nothing, let unlink do
			 * its thing.
			 */
		        if (lstat64(tfile[ix1], &nstatb) != 0)
				break;
			
			/*
			 * If not a link, we're done.
			 */
			if ((nstatb.st_mode & S_IFMT) != S_IFLNK)
				break; 	/* exit 'for' loop */
		}
	
		if (count >= MAXSYMLINKS) {
			setoserror(ELOOP);
			return(-1);
		}
	}

        /* If filename is not a directory, call unlink(filename)
         * Otherwise, call rmdir(filename)
	 */
        if ((statb.st_mode & S_IFMT) != S_IFDIR)
                return(unlink(filename));
        return(rmdir(filename));
}
