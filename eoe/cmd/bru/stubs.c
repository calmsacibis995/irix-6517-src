/************************************************************************
 *									*
 *			Copyright (c) 1987, Fred Fish			*
 *			    All Rights Reserved				*
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
 *	stubs.c    stubs for functions that have no Unix equivalents
 *
 *  SCCS
 *
 *	@(#)stubs.c	1.11	5/11/88
 *
 *  DESCRIPTION
 *
 *	This module contains stubs for functions that have no Unix
 *	equivalent, and for which no equivalent is necessary.  This
 *	helps to prevent proliferation of lots of #ifdef'd code,
 *	which the author finds to be ugly and detrimental to
 *	readability.  This file will probably grow as #ifdefs are
 *	removed from existing routines.
 *
 */

#include "autoconfig.h"
#include "stdio.h"
#include "typedefs.h"


/*
 *	Note:  these need to be properly declared with the correct
 *	return types and arguments, to prevent lint from choking.
 *	Will do it next rev...
 */

/*VARARGS0*/
VOID clear_archived ()
{
}

/*VARARGS0*/
VOID mark_archived ()
{
}

/*VARARGS0*/
int setinfo ()
{
    return (0);
}

/*
 *	This routine checks to see if the archive bit is set in the file
 *	attributes flags word (currently Amiga only).
 */

/*VARARGS0*/
int abit_set ()
{
    return (0);
}
