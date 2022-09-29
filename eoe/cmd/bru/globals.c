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
 *  SCCS
 *
 *	@(#)globals.c	9.11	5/11/88
 *
 */
 
/*
 *	Large programs frequently become maintenance nightmares because
 *	of proliferation of global variables.  Whenever possible and
 *	practical, the philosophy of "information hiding" is adhered
 *	to in the individual modules making up this program.
 *
 *	However, there are cases where passing everything as parameters
 *	is either impractical, or at best, awkward.  In deference to this
 *	fact of life, all quantities globally available are collected
 *	into a single file so that such beasts can be
 *	easily identified and dealt with in modifications or
 *	when carving out pieces to build another program.
 *
 *	To avoid obscure system dependencies, every global
 *	should be initialized to some meaningful value.
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
#include "manifest.h"
#include "config.h"
#include "finfo.h"
#include "devices.h"
#include "flags.h"
#include "bruinfo.h"
#include "exeinfo.h"
#include "utsname.h"

struct cmd_flags flags;		/* Flags given on command line */
struct bru_info info;		/* Current invocation information */
struct finfo afile;		/* The archive file */
struct exe_info einfo;		/* Runtime execution information */

FILE *logfp = stdout;		/* Verbosity stream */
time_t ntime;			/* Time given by -n option */
uid_t uid;			/* User ID derived via -o option */
char *label = NULL;		/* Archive label string (63 char max) */
time_t artime = (time_t)0;	/* Time read from existing archive */
char *working_dir = NULL;	/* Current dir pathname */

/*
 *	Items which are built in defaults or version identification.
 *	User may print out with the -h option.
 */

int release = RELEASE;		/* Major release number */
int level = LEVEL;		/* Minor release level number */
int variant = VARIANT;		/* Bru variant */
char *id = ID;			/* String ID */
long msize = 0L;		/* size of archive media */
UINT bufsize = BUFSIZE;		/* Size of archive read/write buffer */
struct device *ardp;		/* Archive device */
char mode = '?';		/* Current mode */
BOOLEAN interrupt = FALSE;	/* Interrupt received */

/*
 *	A copyright notice which simply hangs around in the object code.
 */

#ifndef lint
static const char copyright[] = "bru base 9.11; Copyright (c) 1984, Fred Fish";
#endif

/*
 *	Strictly speaking, the following buffer for the Unix system
 *	name structure does not have to be global, however, there
 *	is a declaration for it in /usr/include/sys/utsname.h, which
 *	must be accounted for in order to make lint happy.
 *	This was previously set up as an auto in the routine "winfo"
 *	in "create.c".
 *	Also note that this note only applies to some versions of unix.
 *	Some variants, including at least xenix release 3.0, have
 *	sys/utsname.h, but do not declare an instance of utsname.
 *
 */

struct utsname utsname;		/* See /usr/include/sys/utsname.h */
