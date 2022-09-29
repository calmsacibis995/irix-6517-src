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
 *	config.h    contains configuration parameters
 *
 *  SCCS
 *
 *	@(#)config.h	9.1	12/21/87
 *
 *  SYNOPSIS
 *
 *	#include "config.h"
 *
 *  DESCRIPTION
 *
 *	This file contains parameters which can be changed
 *	as desired to alter bru's defaults.
 *
 *	However, be aware that changing some of these parameters
 *	may make it incompatible with the older versions of bru
 *	or even one of it's siblings with other parameters.
 *
 */


/*
 *	Fundamental constants, these should be considered sacred and
 *	only changed with great reluctance.  Changing them will make your
 *	bru archives incompatible with the rest of the bru's in the world.
 *
 */

#define BLKSIZE		(2048)		/* # of bytes per logical block */
#define NAMESIZE	(128)		/* File name size in info block */
#define IDSIZE		(64)		/* Bru ID string size in info block */
#define BLABELSIZE	(64)		/* Archive label size in info block */
#define MODESZ		(10)		/* Size of mode string */
#define UTSELSZ		(9)		/* Size of my_utsname elements */


/*
 *	The following parameters are not so critical and may be
 *	tuned appropriately.
 *
 */

#define BUFSIZE		(10 * BLKSIZE)	/* Default archive buffer size */

#ifdef amiga
#define TERMINAL	"*"		/* Terminal for interaction */
#define BRUTAB		"s:brutab"	/* Data file for device table */
#define BRUTMPDIR	"ram:"		/* Prefered dir for temp files */
#else
#define TERMINAL	"/dev/tty"	/* Terminal for interaction */
#define BRUTAB		"/etc/brutab"	/* Data file for device table */
#define BRUTMPDIR	"/usr/tmp"	/* Prefered dir for temp files */
#endif

#define BRUTABSIZE 	1024		/* Brutab entry buffer size */


/*
 *	RELEASE	is the numeric release number.  It gets incremented
 *		when there are massive changes and/or bug fixes.
 *		This point may be rather arbitrary.
 *
 *	LEVEL	is the numeric release level number.  It get incremented
 *		each time minor changes (updates) occur.
 *
 *	ID	is the verbose bru identification.  It can be
 *		no longer than IDSIZE - 1 characters!
 *
 *	VARIANT is a unique sequentially incremented number which
 *		gets changed each time a potential incompatibility
 *		is introduced.  This allows limited backward
 *		compatibility.
 *
 *	Note that when sccs gets a file for editing, the RELEASE
 *	and LEVEL keyword substitutions are not made.  When testing
 *	such revisions, the preprocessor symbol TESTONLY must be
 *	defined.
 *
 */


#ifdef TESTONLY
#    define RELEASE		(0)
#    define LEVEL		(0)
#else
#    define RELEASE		(9)
#    define LEVEL		(11)
#endif

#ifdef amiga
#define ID		"Amiga Beta 3 Release"
#else
#ifndef sgi	/* SGI gets from Makefile */
#define ID		"Third OEM Release"
#endif
#endif

#define VARIANT		(1)

/*
 *	For USG systems, a BSD compatible directory access library must
 *	be used.  Bru contains an internal copy of this library that is
 *	used by default.  To disable this, and use your own external
 *	version, define "SYSDIRLIB".  You will also have to add the library
 *	name to LIBS in the makefile if the routines are not in libc.a.
 */

 /* #define SYSDIRLIB */
 
