/************************************************************************
 *									*
 *			Copyright (c) 1984, Fred Fish			*
 *			   All Rights Reserved				*
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
 *	access.c    routines for testing file accessibility
 *
 *  SCCS
 *
 *	@(#)access.c	9.11	5/11/88
 *
 */

#include "autoconfig.h"

#include <stdio.h>
#include <string.h>

#if unix || xenix
#include <sys/types.h>
#include <sys/stat.h>
#endif

#include "typedefs.h"
#include "dbug.h"
#include "manifest.h"
#include "errors.h"
#include "config.h"
#include "macros.h"

extern int s_access ();		/* Invoke the library "access" function */
extern char *s_strrchr ();	/* Find last given character in string */
extern int s_readlink ();	/* System function to read a symbolic link */
extern VOID bru_error ();	/* Report an error to user */


/*
 *  FUNCTION
 *
 *	file_access    test for file accessible with given mode
 *
 *  SYNOPSIS
 *
 *	BOOLEAN file_access (name, amode, flag)
 *	char *name;
 *	int amode;
 *	BOOLEAN flag;
 *
 *  DESCRIPTION
 *
 *	Uses the process real user id and real group id to test the
 *	pathname pointed to by "name" for access in the given "amode".
 *	If access is denied and "flag" is TRUE, the user will be
 *	notified that access was denied.
 *
 *	4.2 BSD symbolic links introduce lots of problems.  Since the
 *	system "access" routine goes through them to the real file, if a
 *	symbolic link points to a non-existant file, "access" will fail.
 *	This is not quite desirable.  So, what we'll do is try a readlink
 *	on the file. If it succeeds, and the test was for existance or
 *	readability, then return true, otherwise false. (If a symlink exists,
 *	another call to symlink on it [i.e. "writing" to it] will fail.)

 *	The paragraph above causes directory access writablilty checks
 *	to fail, which is just plain wrong, and kept bru from restoring
 *	into a dir that was a symlink.  Yuck; I *HATE* this code!
 *	I "fixed" this by always returning true for directory writability
 *	and letting later creates or whatever fail...

 *  Note that this routine is somewhat bogus now that bru is no longer
 *  setuid root; if someone choose to make it setuid root again, or
 *  invokes it from a setuid program, it will claim a lack of permission,
 *  even though it shouldn't.  This has been fixed with a hack of
 *  setting the uid and gid to the effective uid and gid in init.c,
 *  rather than fixing all of the code that calls access.
 *  I've fixed some of the bogosities; the others call file_access
 *  mainly because file_stat() or some other routine prints an error
 *  message, and file_access doesn't... 
 *
 */


/*
 *  PSEUDO CODE
 *
 *	Begin file_access
 *	    If bad arguments then
 *		Warn user about bug
 *	    End if
 *	    If readlink on file succeeds then
 *		If access was read or existance then
 *		    Result is TRUE
 *		Else
 *		    Result is FALSE
 *		End if
 *	    Else if specified access is allowed then
 *		Result is TRUE
 *	    Else
 *		Result is false
 *	    End if
 *	    If Result is false and error reporting enabled then
 *		Switch on type of access mode
 *		    Case "write access":
 *			Report write access failure
 *			Break
 *		    Case "read access":
 *			Report read access failure
 *			Break
 *		    Default:
 *			Report file does not exist
 *			Break
 *		End switch
 *	    End if
 *	    Return result
 *	End file_access
 *
 */


BOOLEAN file_access (name, amode, flag)
char *name;
int amode;
BOOLEAN flag;
{
    register int rtnval;
    char buf[NAMESIZE];

    DBUG_ENTER ("file_access");
    if (name == NULL) {
	bru_error (ERR_BUG, "file_access");
	return FALSE;
    }
    DBUG_PRINT ("faccess", ("test %s for access %d", name, amode));
    if (s_readlink (name, buf, sizeof buf) != SYS_ERROR) {
	if (amode != A_WRITE) {
	    rtnval = TRUE;
	} else {
	    rtnval = FALSE;
	}
    } else if (s_access (name, amode) != SYS_ERROR) {
	rtnval = TRUE;
    } else {
	rtnval = FALSE;
    }
    if (rtnval == FALSE && flag) {
	switch (amode) {
	    case A_WRITE:
		bru_error (ERR_WACCESS, name);
		break;
	    case A_READ:
		bru_error (ERR_RACCESS, name);
		break;
	    default:
		bru_error (ERR_EACCESS, name);
		break;
	}
    }
    DBUG_PRINT ("faccess", ("result is %d", rtnval));
    DBUG_RETURN (rtnval);
}


/*
 *  FUNCTION
 *
 *	dir_access    check the parent directory of a node for access
 *
 *  SYNOPSIS
 *
 *	BOOLEAN dir_access (name, amode, flag)
 *	char *name;
 *	int amode;
 *	BOOLEAN flag;
 *
 *  DESCRIPTION
 *
 *	Uses the process real user id and real group id to test the
 *	parent directory of the pathname pointed to by "name" for
 *	access in the given "amode".  If access is denied and
 *	"flag" is TRUE, the user will be notified that access
 *	was denied.
 *
 *	Returns TRUE or FALSE for access permitted or denied
 *	respectively.
 *
 */

/*
 * SEE My comments above about the bogosity of using access, especially
 * when symlinks are involved.... 
 * So now, all we do is check to see if it is a directory; if so, 
 * return TRUE, and let subsequent syscalls fail.  The error messages
 * may not be as great, but that's life...
 *
 * Preserve previous semantics that if no / in name, check current dir.
 *
 */

BOOLEAN dir_access (name, amode, flag)
char *name;
int amode;
BOOLEAN flag;
{
    struct stat64 s;
    int rtnval, hadslash = 0;
    char *d;

    if(!name) {
	bru_error(ERR_BUG, "dir_access");
	return FALSE;
    }

    if(d = strrchr(name, '/')) {
	if(d != name) {
	    hadslash = 1;
	    *d = '\0';
	}
	rtnval = stat64(name, &s); /* use stat, not lstat! */
	if(hadslash)
	    *d = '/';
    }
    else
	rtnval = stat64(".", &s); /* use stat, not lstat! */

    if(rtnval || !IS_DIR(s.st_mode)) {
	if(flag) switch (amode) {
	    case A_WRITE:
		bru_error (ERR_WACCESS, name);
		break;
	    case A_READ:
		bru_error (ERR_RACCESS, name);
		break;
	    default:
		bru_error (ERR_EACCESS, name);
		break;
	}
	return FALSE;
    }

    return TRUE;
}

