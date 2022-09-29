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
 *	links.c    module for resolving file linkages
 *
 *  SCCS
 *
 *	@(#)links.c	9.11	5/11/88
 *
 *  DESCRIPTION
 *
 *	Functions in this module are responsible for maintaining
 *	and manipulating information about linkages between files
 *	in the archive when the archive is created or updated.
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

#include "typedefs.h"		/* Locally defined types */
#include "dbug.h"
#include "manifest.h"		/* Manifest constants */
#include "errors.h"		/* Error codes */
#include "finfo.h"		/* File information structure */


/*
 *	Linkage information is kept in a linked list, one list link
 *	per file with multiple links.
 *
 */

struct link {
    dev_t l_dev;		/* ID of device containing file */
    ino_t l_ino;		/* Inode number of the file */
    int l_nlink;		/* Number of links */
    int l_unres;		/* Number of unresolved links */
    char *l_name;		/* Pathname of 1st archive member linked */
    struct link *l_next;	/* Next link in file link list */
};

/*
 *	Local functions.
 */

static struct link *links = NULL;	/* Pointer to head of list */
static struct link *make_link ();	/* Make a new link */
static struct link *find_link ();	/* Find existing link */

/*
 *	External bru functions.
 */

extern int s_strlen ();			/* Find length of string */
extern char *s_strcpy ();		/* Copy strings */
extern VOID bru_error ();		/* Report an error to user */
extern VOID *get_memory ();		/* Memory allocator */
extern VOID s_free ();			/* Free previously allocated memory */


/*
 *  FUNCTION
 *
 *	add_link    add file to linked list
 *
 *  SYNOPSIS
 *
 *	char *add_link (fip)
 *	struct finfo *fip;
 *	char *name;
 *
 *  DESCRIPTION
 *
 *	Given pointer to a file information structure, adds the
 *	name of the file to the list of multiply linked files,
 *	if it is not already there.
 *
 *	If file was not already in list then returns NULL, otherwise
 *	returns a pointer to the name of the file linked to.
 *
 */


/*
 *  PSEUDO CODE
 *
 *	Begin add_link
 *	    Look for file in current list
 *	    If file already in list then
 *		Prepare to return pointer to file name linked to
 *		Decrement the unresolved link count
 *	    Else
 *		Return pointer will be NULL
 *		Make a new link for the file
 *		If successful then
 *		    Next link is previous head of list
 *		    New head of list is new link
 *		End if
 *	    End if
 *	End add_link
 *
 */

char *add_link (fip)
struct finfo *fip;
{
    register struct link *lp;
    register char *first;

    DBUG_ENTER ("add_link");
    lp = find_link (fip);
    if (lp != NULL) {
	first = lp -> l_name;
	lp -> l_unres--;
    } else {
	first = NULL;
	lp = make_link (fip);
	if (lp != NULL) {
	    lp -> l_next = links;
	    links = lp;
	}
    }
    DBUG_RETURN (first);
}


/*
 *  FUNCTION
 *
 *	make_link    make and initialize a new link
 *
 *  SYNOPSIS
 *
 *	static struct link *make_link (fip)
 *	struct finfo *fip;
 *
 *  DESCRIPTION
 *
 *	Allocate and initialize a new link with the given name.
 *	Note that memory for the link and a fresh copy of the name
 *	are obtained from the standard memory allocator.
 *
 *	Returns pointer to the new link if sufficient memory,
 *	NULL otherwise.
 *
 *
 */


/*
 *  PSEUDO CODE
 *
 *	Begin make_link
 *	    Default return value is NULL
 *	    Allocate storage for the link itself
 *	    If allocation failed then
 *		Inform user we are out of memory
 *	    Else
 *		Allocate memory for copy of name
 *		If allocation failed then
 *		    Free the memory held by the link
 *		    Return value is NULL
 *		Else
 *		    Copy the file name to list storage
 *		    Remember the device number
 *		    Remember the inode number
 *		    Remember the number of links
 *		    Initialize the unresolved link count
 *		    Initialize the pointer to next link
 *		End if
 *	    End if
 *	    Return pointer
 *	End make_link
 *
 */


static struct link *make_link (fip)
struct finfo *fip;
{
    register struct link *new;

    DBUG_ENTER ("make_link");
    new = NULL;
    DBUG_PRINT ("links", ("making new link \"%s\"", fip -> fname));
    new = (struct link *) get_memory (sizeof (struct link), FALSE);
    if (new == NULL) {
	bru_error (ERR_LALLOC, fip -> fname);
    } else {
	new->l_name = (char *) get_memory ((UINT) (s_strlen (fip -> fname) + 1), FALSE);
	if (new -> l_name == NULL) {
	    s_free ((char *) new);
	    new = NULL;
	} else {
	    (VOID) s_strcpy (new -> l_name, fip -> fname);
	    new -> l_dev = fip -> statp -> st_dev;
	    new -> l_ino = fip -> statp -> st_ino;
	    new -> l_nlink = fip -> statp -> st_nlink;
	    new -> l_unres = fip -> statp -> st_nlink;
	    new -> l_next = NULL;
	}
    }
    DBUG_RETURN (new);
}    

/* used to handle hardlinks to symlinks, which is supported by SGI,
 * but not by many (any?) other vendors.
*/
char *
getlinkname(fip)
struct finfo *fip;
{
	struct link *lp = find_link(fip);
	return lp ? lp->l_name : (char *)lp;
}



/*
 *  FUNCTION
 *
 *	find_link    find a previous link which is same file
 *
 *  SYNOPSIS
 *
 *	static struct link *find_link (fip)
 *	struct finfo *fip;
 *
 *  DESCRIPTION
 *
 *	Scans list of linked files to find a match.  A match is found
 *	when the inode numbers and device numbers match.
 *
 *	If match found, returns pointer to the link, otherwise
 *	returns NULL.
 *
 */


/*
 *  PSEUDO CODE
 *
 *	Begin find_link
 *	    For each link in the list
 *		If inode numbers match then
 *		    If same device then
 *			Break link list scan loop
 *		    End if
 *		End if
 *	    End for
 *	    Return pointer to link found or NULL
 *	End find_link
 */


static struct link *find_link (fip)
register struct finfo *fip;
{
    register struct link *lp;

    DBUG_ENTER ("find_link");
    for (lp = links; lp != NULL; lp = lp -> l_next) {
	if (lp -> l_ino == fip -> statp -> st_ino) {
	    if (lp -> l_dev == fip -> statp -> st_dev) {
		break;
	    }
	}
    }
    DBUG_RETURN (lp);
}


/*
 *  FUNCTION
 *
 *	unresolved    complain about unresolved links
 *
 *  SYNOPSIS
 *
 *	VOID unresolved ();
 *
 *  DESCRIPTION
 *
 *	Scans the list of linked files, complaining about each file
 *	which has any unresolved links.
 *
 *  NOTES
 *
 *	Things may get a little funny when the links are changing
 *	dynamically while bru is running.  A typical example is that
 *	the link count goes up or down as links are made to a file
 *	that does not change.
 *
 *	Another, less typical, problem is files with multiple
 *	links that have all their links removed, a new file created
 *	with the same inode and device numbers, and multiple links made
 *	to the new file.  If this happens between the time bru sees the
 *	first set of links, and the second set of links, the second set
 *	essentially becomes linked to the first set (remember, only the
 *	first file in any set of links is actually archived).  Thus
 *	on extraction, all the names seen by bru will be linked to the
 *	first file.
 *	
 */


/*
 *  PSEUDO CODE
 *
 *	Begin unresolved
 *	    For each link in the list
 *		If there are unresolved links (removed or not seen) then
 *		    Issue warning about unresolved links for file
 *		Else if there were links added then
 *		    Issue warning about additional links
 *		End if
 *	    End for
 *	End unresolved
 *
 */

VOID unresolved ()
{
    register struct link *lp;
    
    DBUG_ENTER ("unresolved");
    for (lp = links; lp != NULL; lp = lp -> l_next) {
	if (lp -> l_unres > 1) {
	    bru_error (ERR_URLINKS, lp -> l_name, lp -> l_unres - 1);
	} else if (lp -> l_unres < 1) {
	    bru_error (ERR_ALINKS, lp -> l_name, 1 - lp -> l_unres);
	}
    }
    DBUG_VOID_RETURN;
}


/*
 *  FUNCTION
 *
 *	free_links    discard linkage information and free memory
 *
 *  SYNOPSIS
 *
 *	VOID free_links ();
 *
 *  DESCRIPTION
 *
 *	Discards all linkage information by freeing all memory
 *	associated with the linkage list.
 *
 */


/*
 *  PSEUDO CODE
 *
 *	Begin free_list
 *	End free_list
 *
 */

VOID free_list ()
{
    register struct link *lp;
    register struct link *nextlp;
    
    DBUG_ENTER ("free_list");
    lp = links;
    while (lp != NULL) {
	s_free (lp -> l_name);
	nextlp = lp -> l_next;
	s_free ((char *) lp);
	lp = nextlp;
    }
    links = NULL;
    DBUG_VOID_RETURN;
}
