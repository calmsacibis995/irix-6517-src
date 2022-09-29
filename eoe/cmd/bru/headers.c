/************************************************************************
 *									*
 *			Copyright (c) 1984, Fred Fish			*
 *			     All Rights Reserved			*
 *									*
 *	This software and/or documentation is protected by U.S.		*
 *	Copyright Law (Title 17 United States Code).  Unauthorized	*
 *	reproduction and/or sales may result in imprisonment of up	*
 *	to 1 year and fines of up to $10,000 (17 USC 506).		*
 *	Copyright infringers may also be subject to civil liability.	*
 *									*
 ************************************************************************
 */


/*
 *  FILE
 *
 *	headers.c    routines to manipulate header blocks
 *
 *  SCCS
 *
 *	@(#)headers.c	9.11	5/11/88
 *
 *  DESCRIPTION
 *
 *	Contains routines which are primarily used to manipulate
 *	archive header blocks.
 *
 */


#include "autoconfig.h"

#include <stdio.h>

#if (unix || xenix)
#  include <sys/types.h>
#  include <sys/stat.h>
#else
#  include "sys.h"
#endif

#include "typedefs.h"
#include "dbug.h"
#include "manifest.h"
#include "config.h"
#include "blocks.h"
#include "macros.h"
#include "finfo.h"

/*
 *	External bru functions
 */

extern BOOLEAN chksum_ok ();	/* Check checksum for block */
extern S32BIT fromhex ();	/* Convert hex to 32 bit integer */
extern char *s_strcpy ();


/*
 *  NAME
 *
 *	hcheck    perform sanity checks on header block
 *
 *  SYNOPSIS
 *
 *	BOOLEAN hcheck (blkp)
 *	register union blk *blkp;
 *
 *  DESCRIPTION
 *
 *	Given pointer to a presumed header block, tests
 *	to see if the block passes various sanity checks.
 *
 *	Currently only checks for proper checksum and magic number.
 *	Should eventually perform pattern type checks also.
 *
 */


/*
 *  PSEUDO CODE
 *
 *	Begin hcheck
 *	    Default result is FALSE
 *	    If the checksum is ok then
 *		If the magic number is ok then
 *		    Result is TRUE
 *		End if
 *	    End if
 *	    Return result
 *	End hcheck
 *
 */

BOOLEAN hcheck (blkp)
register union blk *blkp;
{
    register BOOLEAN result;		/* Result of test */

    DBUG_ENTER ("hcheck");
    result = FALSE;
    if (chksum_ok (blkp)) {
	DBUG_PRINT ("sanity", ("header checksum ok"));
	if (FROMHEX (blkp -> HD.h_magic) == H_MAGIC) {
	    DBUG_PRINT ("sanity", ("magic number ok"));
	    result = TRUE;
	}
    }
    DBUG_RETURN (result);
}


/*
 *  NAME
 *
 *	hstat    decode header block into a stat buffer
 *
 *  SYNOPSIS
 *
 *	VOID hstat (blkp, fip)
 *	register union blk *blkp;
 *	register struct finfo *fip;
 *
 *  DESCRIPTION
 *
 *	Decodes the contents of a header block, pointed to by
 *	"blkp", into a "stat" buffer.
 *
 *	Also pulls out a couple of other finfo fields while we
 *	are at it.
 */

VOID hstat (blkp, fip)
register union blk *blkp;
register struct finfo *fip;
{
    DBUG_ENTER ("hstat");
    fip -> statp -> st_mode = (ushort) FROMHEX (blkp -> FH.f_mode);
    fip -> statp -> st_ino = (ino_t) FROMHEX (blkp -> FH.f_ino);
    fip -> statp -> st_dev = (dev_t) FROMHEX (blkp -> FH.f_dev);
    fip -> statp -> st_rdev = (dev_t) FROMHEX (blkp -> FH.f_rdev);
    fip -> statp -> st_nlink = (short) FROMHEX (blkp -> FH.f_nlink);
    fip -> statp -> st_uid = (uid_t) FROMHEX (blkp -> FH.f_uid);
    fip -> statp -> st_gid = (gid_t) FROMHEX (blkp -> FH.f_gid);
    fip -> statp -> st_atime = (time_t) FROMHEX (blkp -> FH.f_atime);
    fip -> statp -> st_mtime = (time_t) FROMHEX (blkp -> FH.f_mtime);
    fip -> statp -> st_ctime = (time_t) FROMHEX (blkp -> FH.f_ctime);
    fip -> fi_flags = (int) FROMHEX (blkp -> FH.f_flags);
    fip -> fib_Protection = (long) FROMHEX (blkp -> FH.f_fibprot);
    (VOID) s_strcpy (fip -> fib_Comment, blkp -> FH.f_fibcomm);
    if (IS_COMPRESSED (fip)) {
	fip -> statp -> st_size = FROMHEX (blkp -> FH.f_xsize);
	fip -> zsize = (off_t) FROMHEX (blkp -> FH.f_size);
    } else {
	fip -> statp -> st_size = FROMHEX (blkp -> FH.f_size);
    }
    DBUG_VOID_RETURN;
}
