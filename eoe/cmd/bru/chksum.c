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
 *	chksum.c    routines for computing and testing checksums
 *
 *  SCCS
 *
 *	@(#)chksum.c	9.11	5/11/88
 *
 *  DESCRIPTION
 *
 *	Each archive block has a checksum computed for it.  These
 *	routines compute and test checksums.
 *
 *	The checksum as used here is a 32 bit word which is
 *	simply the sum of all the bytes in the block.  Prior
 *	to computing the checksum, the bytes used to store the
 *	checksum itself are initialized to a single '0' character
 *	preceeded by blanks.
 *
 */


#include "autoconfig.h"

#include <stdio.h>

#if (unix || xenix)
#  include <sys/types.h>
#else
#  include "sys.h"		/* Header to fake stuff for non-unix */
#endif

#include "typedefs.h"		/* Locally defined types */
#include "dbug.h"
#include "manifest.h"		/* Manifest constants */
#include "errors.h"		/* Error codes */
#include "config.h"		/* Configuration file */
#include "blocks.h"		/* Archive format structures */
#include "macros.h"		/* Useful macros */
#include "flags.h"		/* Command line flags */

extern VOID bru_error ();	/* Report an error to user */
extern char *s_memcpy ();	/* Copy memory around */
extern struct cmd_flags flags;	/* Flags from command line */


/*
 *  FUNCTION
 *
 *	sumblock    compute the actual sum of bytes
 *
 *  SYNOPSIS
 *
 *	static CHKSUM sumblock (base, nbytes)
 *	char *base;
 *	int nbytes;
 *
 *  DESCRIPTION
 *
 *	Given a pointer to the first byte of a block, and the size of
 *	the block, compute a byte-wise checksum of the block.  The checksum
 *	is computed by simply adding all the bytes in the block, treating
 *	each byte as a small signed or unsigned two's complement integer
 *	(overflows are ignored), and accumulating the result.
 *
 *	Bru typically spends from 20-65 percent of its cpu time computing
 *	checksums.  Be very careful how you modify this routine and be sure
 *	to test carefully for compatibility against the portable C version.
 *
 *	Note that we copy the base pointer into a variable which has been
 *	explicitly declared to be a register variable.  Some compilers
 *	which support register variables ignore register declarations
 *	on the passed in parameters, and will not do the copy for us.
 *
 *	If you wish, you can replace the byte-wise summing code with
 *	custom assembly code for the target machine.  See the notes
 *	in the config script.  This can be a big performance win on
 *	some machines.
 *
 */

#if FAST_CHKSUM

extern CHKSUM sumblock ();		/* Use your own private assembly version */

#else

static CHKSUM sumblock (base, nbytes)
char *base;
int nbytes;
{
    register char *next;
    register CHKSUM checksum = 0;

    DBUG_ENTER ("sumblock");
    next = base;
    while (nbytes-- > 0) {
	checksum += *next++;
    }
    DBUG_RETURN (checksum);
}

#endif	/* FAST_CHKSUM */


/*
 *  FUNCTION
 *
 *	chksum   compute checksum for a block
 *
 *  SYNOPSIS
 *
 *	CHKSUM chksum (blkp)
 *	register union blk *blkp;
 *
 *  DESCRIPTION
 *
 *	Computes the checksum for a block.  Note that the
 *	checksum DOES NOT include the ascii characters for the checksum
 *	itself.  The checksum string in the block is replaced
 *	with a '0', preceeded with blanks, before the checksum is computed.
 *	The old value (meaningful or not) is then restored.
 *
 *  NOTES
 *
 *	We cannot use the FROMHEX and TOHEX macros to convert the checksum
 *	string because if the bytes don't represent a valid hex number
 *	then the conversion/restoration process will not preserve the
 *	bytes properly.  Therefore we must save the bytes via an image
 *	copy.
 *
 *	When the fast flag is set, this function simply returns 0 as the
 *	checksum, thus effectively eliminating checksum functionality.
 *	Archives made with the fast flag option must also be read with
 *	the fast flag option.  Beware that this also disables the
 *	automatic byte swapping feature, and other features, that depend
 *	on checksum computation.
 *
 */


/*
 *  PSEUDO CODE
 *
 *	Begin chksum
 *	    Default checksum is zero
 *	    If fast flag is not set then
 *		Remember old checksum
 *		Replace existing checksum with 0
 *		Compute the block checksum.
 *		Restore previous value of checksum
 *	    Endif
 *	    Return checksum to caller
 *	End chksum
 *
 */

CHKSUM chksum (blkp)
register union blk *blkp;
{
    register CHKSUM checksum = 0L;
    auto char oldsum[sizeof(blkp -> HD.h_chk)];

    DBUG_ENTER ("chksum");
    if (!flags.Fflag) {
	(VOID) s_memcpy (oldsum, blkp -> HD.h_chk, sizeof (blkp -> HD.h_chk));
	TOHEX (blkp -> HD.h_chk, 0L);
	checksum = sumblock (blkp -> bytes, ELEMENTS (blkp -> bytes));
	DBUG_PRINT ("checksum", ("computed checksum %#lx", checksum));
	(VOID) s_memcpy (blkp -> HD.h_chk, oldsum, sizeof (blkp -> HD.h_chk));
    }
    DBUG_RETURN (checksum);
}


/*
 *  NAME
 *
 *	chksum_ok    check block checksum
 *
 *  SYNOPSIS
 *
 *	BOOLEAN chksum_ok (blkp)
 *	register union blk *blkp;
 *
 *  DESCRIPTION
 *
 *	Given pointer to a block, checks the checksum.
 *	When the "fast flag" is TRUE, this function always
 *	returns TRUE, without doing any checking at all.
 *
 */


/*
 *  PSEUDO CODE
 *
 *	Begin chksum_ok
 *	    Set default return to FALSE
 *	    If block pointer is null then
 *		Tell user about bug
 *	    Else
 *		If fast flag is set then
 *		    Always return true without checking
 *		Else
 *		    Convert ascii checksum to internal format
 *		    Test against actual computed checksum
 *		Endif
 *	    End if
 *	    Return result
 *	End chksum_ok
 *
 */

BOOLEAN chksum_ok (blkp)
register union blk *blkp;
{
    register BOOLEAN result;		/* Result of checksum test */
    register CHKSUM checksum;		/* Decoded ascii checksum */

    DBUG_ENTER ("chksum_ok");
    result = FALSE;
    if (blkp == NULL) {
	bru_error (ERR_BUG, "chksum_ok");
    } else {
	if (flags.Fflag) {
	    result = TRUE;
	} else {
	    checksum = FROMHEX (blkp -> HD.h_chk);
	    DBUG_PRINT ("dir", ("read checksum %#lx", checksum));
	    result = (checksum == chksum (blkp));
	}
    }
    DBUG_PRINT ("chk", ("result %d", result));
    DBUG_RETURN (result);
}


