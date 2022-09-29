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
 *	utsname.h    define the internal uname structure, independant of OS
 *
 *  SCCS
 *
 *	@(#)utsname.h	9.11	5/11/88
 *
 *  SYNOPSIS
 *
 *	define the internal uname struct
 *	#ifndef BSD4_2
 *	#include <sys/utsname.h>
 *	#endif
 *
 *  DESCRIPTION
 *
 *	Since 4.2BSD has an entirely different method of getting the hostname
 *	and all that stuff, we simply fake it.  If we are 4.2, we define the
 *	uname structure (which the System V manual happily describes to us in
 *	detail).  We changed the few files which include <sys/utsname.h> to
 *	just include "utsname.h".  Then, in sys.c, we borrow the uname()
 *	emulation from BRL (which is public domain), and fix it up some.
 *	Finally, following the comment in globals.c, we here declare a utsname
 *	structure 'utsname' as extern.
 *
 *	If not 4.2, then just go ahead and get <sys/utsname.h>.
 */

struct my_utsname {		/* Our internal version, invarient */
	char	sysname[9];
	char	nodename[9];
	char	release[9];
	char	version[9];
	char	machine[9];
};

#if BSD4_2 || (!unix && !xenix)
struct utsname {		/* Fake external version for BSD 4.2 */
	char	sysname[9];
	char	nodename[9];
	char	release[9];
	char	version[9];
	char	machine[9];
};
#else
#include <sys/utsname.h>	/* Get real external version */
#endif

extern struct utsname utsname;
extern struct my_utsname my_utsname;
