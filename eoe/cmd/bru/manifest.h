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
 *	manifest.h    useful manifest constants for bru
 *
 *  SCCS
 *
 *	@(#)manifest.h	9.11	5/11/88
 *
 *  SYNOPSIS
 *
 *	#include "manifest.h"
 *
 *  DESCRIPTION
 *
 *	Some miscellaneous useful manifest constants.  Some of these
 *	really should have been placed in a system header file long
 *	ago.  They show up very often in application programs.
 *
 */


#define TRUE		1	/* Boolean TRUE */
#define FALSE		0	/* Boolean FALSE */

#define EOS		'\000'	/* End Of String character */
#define BELL		'\007'	/* Bell character */

#define BRIEF		0	/* Brief help message */
#define VERBOSE		1	/* Verbose help message */

#define NORMAL_EXIT	0	/* Exit status for normal exit */
#define ERROR_EXIT	1	/* Exit status for error exit */

#define SYS_ERROR	(-1)	/* Returned by system call on error */

/*
 *	Magic numbers which define what type of block is being used.
 *	These were picked rather unscientifically, as is probably
 *	obvious.
 *
 */

#define A_MAGIC		0x1234	/* Archive header magic number */
#define H_MAGIC		0x2345	/* File header magic number */
#define D_MAGIC		0x3456	/* File data block magic number */
#define T_MAGIC		0x7890	/* Archive terminator magic */

/*
 *	Define arguments to system "access" call.  Why aren't these
 *	in a header file somewhere?
 *
 *	(4.2 BSD finally did; they're F_OK, R_OK, W_OK, and X_OK
 *	respectively, but I'm not going to include them since I would
 *	have to test for 4.2 BSD, and then include <sys/file.h>.  ADR)
 *
 */

#define A_EXISTS	00	/* Check existence of file */
#define A_EXEC		01	/* Execute (search) permission */
#define A_WRITE		02	/* Write permission */
#define A_READ		04	/* Read permission */

/*
 *	For compatibility between 4.2BSD and System V machines,
 *	we define certain file types that one has but the other
 *	doesn't.  This is so that a 4.2 system can handle a tape
 *	written under System V, and/or vice versa.  Luckily, there
 *	are only two cases, and the numbers don't conflict.  This
 *	does sort of defeat the whole purpose of standard header files
 *	to define magic constants, but what are we supposed to do?
 *
 *	This also works on the Pyramid, which defines both, and also
 *	happens to use the right magic numbers.
 *
 *	ADR
 */

#ifndef S_IFIFO		/* System V FIFO, i.e. a pipe, named or otherwise */
#define S_IFIFO		0010000
#endif

#ifndef S_IFLNK		/* 4.2BSD Symbolic link */
#define S_IFLNK		0120000
#endif
