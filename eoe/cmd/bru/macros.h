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
 *	macros.h    useful preprocessor macros for bru utility
 *
 *  SCCS
 *
 *	@(#)macros.h	9.11	5/11/88
 *
 *  SYNOPSIS
 *
 *	#include <sys/types.h>
 *	#include <sys/stat.h>
 *	#include "typedefs.h"
 *	#include "macros.h"
 *
 *  DESCRIPTION
 *
 *	Contains miscellaneous useful macros for the bru utility.
 *
 *	Note that all macros consist of upper case names only.  I
 *	consider it a serious error that the stdio library makes
 *	use of macros with lower case names, because then it is
 *	not obvious that the symbol represents a macro.  Even more
 *	serious is the fact that not all implementations use macros
 *	for the same symbol.  This causes porting problems when
 *	things like "extern int getc();" or "foo (getc,&foobar)"
 *	work in implementations where getc is a real function.
 *
 */


#define IS_DIR(mode) (((mode) & S_IFMT) == S_IFDIR)	/* Is directory */
#define IS_CSPEC(mode) (((mode) & S_IFMT) == S_IFCHR)	/* Is char special */
#define IS_BSPEC(mode) (((mode) & S_IFMT) == S_IFBLK)	/* Is block special */
#define IS_REG(mode) (((mode) & S_IFMT) == S_IFREG)	/* Is normal file */
#define IS_FIFO(mode) (((mode) & S_IFMT) == S_IFIFO)	/* Is fifo */
#define IS_FLNK(mode) (((mode) & S_IFMT) == S_IFLNK)	/* Is symbolic link */
#define TOHEX(out,val) tohex((out),(S32BIT)(val),sizeof(out))
#define FROMHEX(in) fromhex((in),sizeof(in))
#define STRSAME(a,b) (s_strcmp (a,b) == 0)		/* Strings equal */
#define DOTFILE(a) (STRSAME (".", a) || STRSAME ("..", a))
#define SPECIAL(a) (DOTFILE(a) || STRSAME ("/", a))
#define STREND(start,scan) for (scan = start; *scan != EOS; scan++)
#define LINKED(blkp) (*blkp -> sb.data.fh.f_lname != EOS)


/*
 *	The following macros deal with size conversions.
 *
 *	ARBLKS:		Convert number of data bytes to
 *			number of archive data blocks
 *			required to archive the data.
 *
 *	BLOCKS:		Convert number of bytes to number
 *			of archive blocks.
 *
 *	KBYTES:		Convert number of blocks to
 *			kilobytes of archive output.  Note
 *			that this IS NOT kilobytes of
 *			archived data, because it includes
 *			the header overhead.
 *			Divide first, so that we can go up to 4 Tbytes before wrapping
 *			We already have single tapes that go to 2+ Gb, and estimating
 *			can cause us to go way past 4 Gb.  The change assumes that
 *			BLKSIZE is a multiple of 1024, which should remain true.
 *
 *	ZSIZE		Given a file info pointer, return
 *			"real" size of the file if not
 *			compressed, or "compressed" size.
 *
 *	ZARBLKS		Same as ARBLKS, but takes a file
 *			info pointer instead, and returns
 *			number of archive blocks for compressed
 *			version if compression is used.
 *
 */
 
#define ARBLKS(size) (((ULONG)(size) + DATASIZE - 1) / DATASIZE)
#define BLOCKS(size) (((ULONG)(size) + BLKSIZE - 1) / BLKSIZE)
#define KBYTES(blks) ((ULONG)(blks) * ((ULONG)BLKSIZE / (ULONG)1024) ) 
#define B2KB(bytes) ((ULONG)(bytes)/(ULONG)1024)
#define ZSIZE(fip) (IS_COMPRESSED(fip)?((fip)->zsize):((fip)->statp->st_size))
#define ZARBLKS(fip) (ARBLKS(ZSIZE(fip)))


/*
 *	The following macros provide the number of elements
 *	in an array and the address of the first array
 *	element which is past the end of the array.
 */

#define ELEMENTS(array) (sizeof(array)/sizeof(array[0]))
#define OVERRUN(array) (&(array[ELEMENTS(array)]))

/*
 *	Declare certain externals used in the macro expansions
 */

extern S32BIT fromhex ();
extern VOID tohex ();
extern int s_strcmp ();
