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
 *	passwd.c    password file interface routines
 *
 *  SCCS
 *
 *	@(#)passwd.c	9.11	5/11/88
 *
 *  DESCRIPTION
 *
 *	Contains routines for translating from numerical uid's
 *	and gid's to symbolic user names found in /etc/passwd
 *	and vice-versa.
 *
 *	This involves very little overhead and is well worth
 *	the trouble to avoid the use of numerical id's whenever
 *	possible.
 *
 *	Supported routines are:
 *
 *		ur_init		initialize translation list
 *		ur_guid		translate name to uid
 *		ur_gname	translate uid to name
 *
 */


#include "autoconfig.h"

#include <stdio.h>

#if (unix || xenix)
#  include <sys/types.h>
#  include <pwd.h>
#else
#  include "sys.h"
#endif

#include "typedefs.h"		/* Locally defined types */
#include "dbug.h"
#include "manifest.h"		/* Manifest constants */
#include "macros.h"		/* Useful macros */

/*
 *	A translation list of users is kept in a linear linked list
 *	with each link one of the "user" structures.  Search time
 *	to locate any link in the list is considered to be
 *	insignificant with respect to the total run time.
 *
 *	Initially, each new link was added at the head of the list,
 *	since this is the most efficient way to do things.  However,
 *	this results in the list being in reverse order when compared
 *	with entries in the password file.  In cases where there are two
 *	users with the same uid, this resulted in the "ls" command
 *	finding the first and "bru" finding the last.  In order
 *	to make bru's table output exactly equal to that from "ls",
 *	the pointer to the last list element is now used to add new
 *	elements at the end of the list.
 *
 */

struct user {				/* The user translation info */
    char *ur_name;			/* Pointer to name of user */
    uid_t ur_uid;			/* The user id */
    struct user *ur_next;		/* Pointer to next link in list */
};

static struct user *ur_list = NULL;	/* Pointer to head of list */
static struct user *ur_last = NULL;	/* Pointer to last link in list */

/*
 *	External bru functions.
 */

extern int s_strlen ();			/* Find length of string */
extern char *s_strcpy ();		/* Copy string s2 to s1 */
extern int s_sprintf ();		/* Formatted print to buffer */
extern VOID *get_memory ();		/* Memory allocator */
extern struct passwd *s_getpwent ();	/* Get password file entry */
extern VOID s_endpwent ();		/* Close password file */


/*
 *  FUNCTION
 *
 *	ur_init    read password file and initialize translation list
 *
 *  SYNOPSIS
 *
 *	VOID ur_init ();
 *
 *  DESCRIPTION
 *
 *	Reads the system password file and initializes the translation
 *	list.
 *
 *	The tradeoff involved here is time vs space (as usual).
 *	The space consideration is the memory required to hold
 *	the translation list, which allows very fast access to
 *	any given user structure.  The time consideration is
 *	the amount of time required to use the library routines
 *	"getpwuid" and "getpwnam" to do translation on the fly
 *	without a translation list.  Since a large number of
 *	translations is the normal situation, and since memory
 *	demands are minimal, I have opted to read the password
 *	file once and maintain a translation list.
 *
 *  NOTES
 *
 *	Any failure to allocate sufficient memory is immediately
 *	fatal.
 *
 */


/*
 *  PSEUDO CODE
 *
 *	Begin ur_init
 *	    While there is another entry in password file
 *		Allocate memory for a link
 *		Allocate memory for copy of name
 *		Copy name to private storage
 *		Remember the user id
 *		If first link then
 *		    Make this link head of list
 *		Else
 *		    Connect this link to last link
 *		End if
 *		Make new link the last in list
 *	    End while
 *	    Close the password file
 *	End ur_init
 */

VOID ur_init ()
{
    register struct passwd *passwdp;
    register struct user *userp;
    register UINT size;

    DBUG_ENTER ("ur_init");
    while ((passwdp = s_getpwent ()) != NULL) {
	size = (sizeof (struct user));
	userp = (struct user *) get_memory (size, TRUE);
	size = s_strlen (passwdp -> pw_name) + 1;
	userp -> ur_name = (char *) get_memory (size, TRUE);
	(VOID) s_strcpy (userp -> ur_name, passwdp -> pw_name);
	userp -> ur_uid = passwdp -> pw_uid;
	userp -> ur_next = NULL;
	if (ur_list == NULL) {
	    ur_list = userp;
	} else {
	    ur_last -> ur_next = userp;
	}
	ur_last = userp;
    }
    s_endpwent ();
    DBUG_VOID_RETURN;
}


/*
 *  FUNCTION
 *
 *	ur_gname    translate a numerical user id number
 *
 *  SYNOPSIS
 *
 *	char *ur_gname (uid)
 *	uid_t uid;
 *
 *  DESCRIPTION
 *
 *	Translate from a numerical user id number to a name.
 *	Always returns a valid pointer, either to the name string
 *	or a string containing the numeric value of the uid.
 *	Note that the string must be copied between calls if
 *	it is to be saved, since numeric strings are overwritten
 *	at the next call that returns a pointer to a numeric string.
 *
 *  NOTE
 *
 *	This routine does not use the DBUG_ENTER, LEAVE, and DBUG_n
 *	macros because their usage screws up the table of contents
 *	with the verbose option (when trace or debug is enabled).
 *
 */


/*
 *  PSEUDO CODE
 *
 *	Begin ur_gname
 *	    Default return value is NULL
 *	    For each translation link in list
 *		If user id numbers match then
 *		    Remember name
 *		    Break translation scan loop
 *		End if
 *	    End for
 *	    If no translation found then
 *		Build numeric string instead
 *		Will return pointer to this string
 *	    Endif
 *	    Return pointer
 *	End ur_gname
 *
 */


char *ur_gname (uid)
uid_t uid;
{
    register struct user *userp;
    register char *name;
    static char uidstr[16];

    name = NULL;
    for (userp = ur_list; userp != NULL; userp = userp -> ur_next) {
	if (userp -> ur_uid == uid) {
	    name = userp -> ur_name;
	    break;
	}
    }
    if (name == NULL) {
	(VOID) s_sprintf (uidstr, "%u", uid);
	name = uidstr;
    }
    return (name);
}


/*
 *  FUNCTION
 *
 *	ur_guid    translate a name to numerical uid
 *
 *  SYNOPSIS
 *
 *	int ur_guid (name)
 *	char * name;
 *
 *  DESCRIPTION
 *
 *	Translate from an ascii name to numerical user id.
 *	Returns uid if successful, -1 if unsuccessful.
 *
 *	Note that since uid's and gid's are typically unsigned
 *	shorts, -1 can be used (carefully) as a test for translation
 *	failure.  Watch out for machine dependencies.
 *
 */


/*
 *  PSEUDO CODE
 *
 *	Begin ur_guid
 *	    Default return value is -1
 *	    For each link in translation list
 *		If this link has the desired name then
 *		    Remember the uid
 *		    Break translation scan loop
 *		End if
 *	    End for
 *	    Return uid
 *	End ur_guid
 *
 */

int ur_guid (name)
char *name;
{
    register struct user *userp;
    register int uid;

    DBUG_ENTER ("ur_guid");
    uid = -1;
    for (userp = ur_list; userp != NULL; userp = userp -> ur_next) {
	if (STRSAME (userp -> ur_name, name)) {
	    uid = userp -> ur_uid;
	    break;
	}
    }
    DBUG_RETURN (uid);
}
