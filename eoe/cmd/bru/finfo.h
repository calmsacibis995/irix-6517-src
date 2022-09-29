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
 *	finfo.h    file information structure definition
 *
 *  SCCS
 *
 *	@(#)finfo.h	9.11	5/11/88
 *
 *  SYNOPSIS
 *
 *	#include <sys/types.h>
 *	#include <sys/stat.h>
 *	#include "typedefs.h"
 *	#include "finfo.h"
 *
 *  DESCRIPTION
 *
 *	Certain information about the current file being processed
 *	is important to the error reporting strategies, archive
 *	corruption recovery strategies, maintenance of archive
 *	block header information, etc.  Rather than make all these
 *	globals, they are collected together in a single structure
 *	called appropriately enough, "finfo".
 *
 */


/*
 *	Defined bits in the fi_flags word.  The flags word is initialized
 *	to zero for Unix hosts and to FI_AMIGA for AmigaDOS.
 *
 *	The state of Zflag is saved in each file header, so that it
 *	can be automatically recovered and set for operations which
 *	read the archive.
 */

#define FI_CHKSUM	000001	/* Checksum error seen on this file */
#define FI_BSEQ		000002	/* Block sequence error seen on this file */
#define FI_AMIGA	000004	/* Enable special AmigaDOS features */
#define FI_LZW		000010	/* File compressed with modified Lempel-Ziv */
#define FI_ZFLAG	000020	/* Zflag was active, even if not compressed */

#if amiga
#    define FI_FLAGS_INIT	FI_AMIGA
#else
#    define FI_FLAGS_INIT	0
#endif

/*
 *	The file information structure is used internally to keep track
 *	of useful information about files that are being processed.
 *	Some of the information is also recorded in the bru archive,
 *	and some is only transient and used internally.
 *
 *	When a file is being stored in compressed form, the 
 *	IS_COMPRESSED() macro returns nonzero.  In this case,
 *	the zsize field is used to hold the size of the file
 *	after compression.
 */

struct finfo {
    struct stat64 *statp;		/* Pointer to stat struct if avail */
    char *fname;		/* Pointer to name of file */
    char *zfname;		/* Pointer to name of compressed file */
    char *lname;		/* Name file linked to if known */
    int fildes;			/* File descriptor */
    int zfildes;		/* Compress file descriptor */
    LBA flba;			/* Archive blk relative to start of file */
    LBA chkerrs;		/* Accumulated checksum error count */
    long fi_flags;		/* Flag word */
    int type;			/* Type of pathname if archived file */
    long fib_Protection;	/* AmigaDOS fib_Protection word */
    char fib_Comment[116];	/* AmigaDOS fib_Comment string */
    long zsize;			/* Size of file after compression */
};

/*
 *	Useful macros from manipulating file information.
 */
 
#define IS_STEM(fip) ((fip) -> type == STEM)
#define IS_LEAF(fip) ((fip) -> type == LEAF)
#define IS_EXTENSION(fip) ((fip) -> type == EXTENSION)
#define IS_NOMATCH(fip) ((fip) -> type == NOMATCH)
#define IS_FINISHED(fip) ((fip) -> type == FINISHED)
#define IS_COMPRESSED(fip) ((fip) -> fi_flags & FI_LZW)

