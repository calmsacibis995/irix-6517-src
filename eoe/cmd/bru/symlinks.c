/************************************************************************
 *									*
 *			Copyright (c) 1985, Fred Fish			*
 *			     All Rights Reserved			*
 *		(Written by Arnold Robbins at Georgia Tech)		*
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
 *	symlinks.c    module for resolving symbolic links
 *
 *  SCCS
 *
 *	@(#)symlinks.c	9.11	5/11/88
 *
 *  DESCRIPTION
 *
 *	Functions in this module are responsible for determining
 *	if a string contains a Pyramid style conditional link, and
 *	when given such a link, to return the UCB or ATT part of
 *	the link depending upon what kind of system bru was
 *	compiled under.  There is also a function to print what a
 *	conditional link points to ala the pyramid ls.
 *
 */

#include "autoconfig.h"

#include <stdio.h>

#include "typedefs.h"		/* Locally defined types */
#include "dbug.h"
#include "manifest.h"		/* Manifest constants */
#include "errors.h"		/* Error codes */
#include "config.h"		/* Configuration file */

/*
 *	External bru functions.
 */

extern VOID bru_error ();	/* Report an error to user */
extern int s_sprintf ();	/* Formatted print to buffer */

/*
 *	Local definitions and functions.
 */

#define ATT	'1'		/* ATT Universe is number 1 */
#define UCB	'2'		/* UCB Universe is number 2 */
#define ISUNIV(c)	(c == ATT || c == UCB)

extern BOOLEAN iscondlink ();
extern char *univlink ();
extern char *namelink ();


/*
 *  FUNCTION
 *
 *	iscondlink	determine if a string is a conditional sym link
 *
 *  SYNOPSIS
 *
 *	BOOLEAN iscondlink (name)
 *	char *name;
 *
 *  DESCRIPTION
 *
 *	Given a pointer to a string, try to determine if it is a
 *	Pyramid conditional symbolic links.  These have the form
 *
 *		<space><universe>:<path><space><universe>:<path>
 *
 *	Current universes are 1 for ATT and 2 for UCB.  This is
 *	pretty shaky, but what can we do?  On a Pyramid, see
 *	csymlink(2) for the (skimpy) details.
 *
 */


/*
 *   PSEUDO CODE
 *
 *	Begin iscondlink
 *	    Set default return value to FALSE
 *	    If name points to nowhere then
 *		Inform user about bug
 *	    Else if first character is space then
 *		If second character is a Universe and third is a colon then
 *		    scan for a blank
 *		    If char after blank is other univ and next is colon then
 *		        return val is TRUE
 *		    Endif
 *		Endif
 *	    Endif
 *	    Return value
 *	End iscondlink
 */

BOOLEAN iscondlink (name)
register char *name;
{
    register BOOLEAN rtnval = FALSE;
    register char univ1 = EOS;

    DBUG_ENTER ("iscondlink");
    if (name == NULL) {
	bru_error (ERR_BUG, "iscondlink");
    } else if (*name == ' ') {
	DBUG_PRINT ("symlink", ("looking at %s", name));
	univ1 = *++name;
	if (*name && ISUNIV(univ1) && *++name == ':') {
	    for (; *name && *name != ' '; name++) {;}
	    if (*name == ' ') {
		name++;
	    }
	    if (*name && ISUNIV (*name) && *name != univ1 && *++name == ':') {
		rtnval = TRUE;
	    }
	}
    }
    DBUG_PRINT ("symlink", ("returning %d", rtnval));
    DBUG_RETURN (rtnval);
}


/*
 *  FUNCTION
 *
 *	univlink	return the currect part of symbolic link
 *
 *  SYNOPSIS
 *
 *	char *univlink (name)
 *	char *name;
 *
 *  DESCRIPTION
 *
 *	Depending upon what kind of non pyramid bru is compiled on, when
 *	given a conditional symbolic link, figures out which part to return
 *	as the file to link to.  To simplify the code in mksymlink in
 *	utils.c, on a pyramid, it just returns its argument.
 *
 *  BUGS
 *
 *	Returns a pointer to static data, which is overwritten on each call.
 *
 */

/*
 *  PSEUDO CODE
 *
 *	Begin univlink
 *	    If name is NULL then
 *		Inform user about bug
 *		Return NULL
 *	    End if
 *	    If on a pyramid then
 *		Just return the argument
 *	    Else if it is not a conditional symbolic link then
 *		Just return the argument
 *	    Else
 *		determine the proper part, and return it
 *	    Endif
 *	End univlink
 */

char *univlink (name)
register char *name;
{
    static char linkbuf[NAMESIZE];
    register char *cp = linkbuf;
    register char *save = name;		/* save argument start, just in case */
    register char lookfor = EOS;

    DBUG_ENTER ("univlink");
    if (name == NULL) {
	bru_error (ERR_BUG, "univlink");
	DBUG_RETURN (((char *)NULL));
    }

#ifdef pyr
    DBUG_PRINT ("symlink", ("on a pyramid, returning %s", name));
    DBUG_RETURN (name);
#else
    if (! iscondlink (name)) {
	DBUG_PRINT ("symlink", ("not a cond link, returning %s", name));
	DBUG_RETURN (name);
    }

    /*
     * At this point, we know that it is a valid conditional symbolic link,
     * since iscondlink() checked it for us.  So we don't do any careful
     * error checking about the format, but just blindly go through it.
     * This may not be the smartest way to do it though.
     */

    name++;	/* skip the leading space */

#if BSD4_2
    lookfor = UCB;
    DBUG_PRINT ("symlink", ("looking for UCB (%c)", lookfor));
#else
    lookfor = ATT;
    DBUG_PRINT ("symlink", ("looking for ATT (%c)", lookfor));
#endif

    if (*name == lookfor) {	/* got it */
	name += 2;		/* skip over colon */
    } else {
	for (; *name && *name != ' '; name++) {;}
	if (*name) {
	    name++;
	}
	if (*name == EOS || *name != lookfor) {
	    bru_error (ERR_BUG, "univlink");
	    DBUG_RETURN (save);
	} else {
	    name += 2;		/* skip over univ and colon */
	}
   }
   /* name now points at the pathname part, copy it to the buffer */
   while (*name && *name != ' ') {
	*cp++ = *name++;
   }
   *cp = EOS;
   DBUG_PRINT ("symlink", ("returning %s", linkbuf));
   DBUG_RETURN ((char *)linkbuf);
#endif
}


/*
 *  FUNCTION
 *
 *	namelink	print out what a conditional link points to
 *
 *  SYNOPSIS
 *
 *	char *namelink (name)
 *	char *name;
 *
 *  DESCRIPTION
 *
 *	Given a conditional symbolic link, turn it into human readable form.
 *
 *  BUGS
 *
 *	Returns a pointer to static data which is overwritten on each call.
 *
 */


char *namelink (name)
register char *name;
{
    static char buf[(NAMESIZE * 2) + 10];
    register char *cp;
    char buf1[NAMESIZE], buf2[NAMESIZE];
    BOOLEAN attfirst = TRUE;

    DBUG_ENTER ("namelink");
    if (name == NULL) {
	bru_error (ERR_BUG, "namelink");
    } else if (*name != ' ' || ! iscondlink (name)) {
	/* do nothing, just return name */
    } else {
	if (*++name == UCB) {
	    attfirst = FALSE;
	}
	name += 2;
	cp = buf1;
	while (*name && *name != ' ') {
	    *cp++ = *name++;
	}
	*cp = EOS;
	while (*name && *name != ':') {
	    name++;
	}
	name++;	/* skip colon */
	cp = buf2;
	while (*name && *name != ' ') {
	    *cp++ = *name++;
	}
	*cp = EOS;

	/* print them in the order encountered in the symbolic link */
	/* even though Pyramid readlink always puts att first */
	/* note leading blank */
	if (attfirst) {
	    (VOID) s_sprintf (buf, " att:%s ucb:%s", buf1, buf2);
	} else {
	    (VOID) s_sprintf (buf, " ucb:%s att:%s", buf1, buf2);
	}
	DBUG_PRINT ("symlink", ("decoded into %s", buf));
	name = buf;
    }
    DBUG_RETURN (name);
}
