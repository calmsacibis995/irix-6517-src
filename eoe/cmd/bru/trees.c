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
 *	trees.c    routines for manipulating directory trees
 *
 *  SCCS
 *
 *	@(#)trees.c	9.11	5/11/88
 *
 *  DESCRIPTION
 *
 *	Contains various modules for creating, manipulating, and
 *	destroying directory trees.
 *
 *	Pathnames specified by the user are used to build a
 *	directory tree, one node for each component of the pathname.
 *
 *	Nodes at the same level (siblings) are kept in a linked
 *	list.  Within each node is a pointer to the parent node
 *	and a pointer to the first child (if any).  This mechanism
 *	allows arbitrary number of children for any given node.
 *
 *  BUGS
 *
 *	This code started out as some of the simplest and most
 *	elegant code in bru.  Then, due to pressure from special
 *	cases and other unmentionable reasons, it distorted to
 *	become some of the most obscure and tricky code in bru.
 *	Now even the author now has trouble following some of the
 *	more subtle aspects.  This whole module needs to be revised.
 *
 */

#include "autoconfig.h"

#ifdef amiga
#include <libraries/dosextens.h>
#include <exec/memory.h>
#undef VOID
#undef TRUE
#undef FALSE
#endif

#include <stdio.h>

#if (unix || xenix)
#  include <fcntl.h>
#  include <sys/types.h>
#  include <sys/stat.h>
#else
#  include "sys.h"
#endif

#include "config.h"		/* Configuration file */
#include "typedefs.h"		/* Locally defined types */

#if (unix || xenix)
#  if (BSD4_2 || HAVE_SEEKDIR)
#    if sgi
#      include <dirent.h>
#    else
#      include <sys/dir.h>	/* BSD flavors only */
#    endif
#  else
#    include "dir.h"		/* NOT <sys/dir.h>; using new dir routines */
#  endif
#else
#  include "dir.h"
#endif
  
#include "dbug.h"
#include "manifest.h"		/* Manifest constants */
#include "errors.h"		/* Error codes */
#include "macros.h"		/* Useful macros */
#include "trees.h"		/* Tree types */
#include "finfo.h"		/* File information structure */
#include "flags.h"		/* Command line arguments */

/*
 *	External bru functions.
 */

extern BOOLEAN file_access ();	/* Check file for access */
extern BOOLEAN file_stat ();	/* Get stat buffer for file */
extern BOOLEAN wild ();		/* Expand pattern metacharacters */
extern VOID bru_error ();	/* Report an error to user */
extern VOID done ();		/* Clean up and exit with status */
extern VOID finfo_init ();	/* Initialize a finfo structure */
extern VOID reset_times ();	/* Reset atime and mtime of file */
extern char *s_strchr ();	/* Find given character in string */
extern char *s_strcpy ();	/* Copy string s2 to s1 */
extern char *s_strrchr ();	/* Find last given character in string */
extern int s_close ();		/* Invoke library file close function */
extern int s_open ();		/* Invoke library file open function */
extern int s_read ();		/* Invoke library file read function */
extern int s_strlen ();		/* Find length of string */
extern VOID *get_memory ();	/* Memory allocator */

/*
 *	External bru variables.
 */

extern struct cmd_flags flags;	/* Flags from command line */

/*
 *	Internal structures.
 */

struct node {
    char *name;				/* String for this component */
    struct node *child;			/* Pointer to first child node */
    struct node *sibling;		/* Pointer to next sibling */
    short nflags;			/* Flags word */
};

static struct node root = {		/* Implicit root node */
    "<root>",
    NULL,
    NULL,
    0
};

/*
 *	Flag bits for node flags word and macros for manipulating
 *	them.
 *
 *	Nodes which terminate an explicit pathname specified by the
 *	user on the command line will be marked for expansion, even
 *	when they are not leaf nodes.  Thus including both "/" and
 *	"/usr" as command line arguments will result in both "/" and
 *	"/usr" being expanded.  Note however that "/usr" will be
 *	ignored during the expansion of "/".
 *
 */
 
#define NF_MATCHED	(000001)	/* Node has been matched */
#define NF_EXPAND	(000002)	/* Always expand if directory */

#define MATCHED(a) (a -> nflags & NF_MATCHED)
#define SET_MATCHED(a) (a -> nflags |= NF_MATCHED)
#define RSET_MATCHED(a) (a -> nflags &= ~NF_MATCHED)

#define EXPAND(a) (a -> nflags & NF_EXPAND)
#define SET_EXPAND(a) (a -> nflags |= NF_EXPAND)
#define RSET_EXPAND(a) (a -> nflags &= ~NF_EXPAND)

/*
 *	Some local buffers and variables.
 */
 
static char namebuf[NAMESIZE];		/* Pathname buffer */
static struct stat64 sbuf;		/* Stat buffer for pathname */
static dev_t dirdev;			/* Device containing explicit dir */

/*
 *	The directory routines are declared in "dir.h". We just use
 *	defines to avoid name conflicts with other s_* routines
 *	under compilers without flexnames.  If this still doesn't work,
 *	you can probably just use the plain names, since this should
 *	be the only source file which actually reads directories.
 */

#define s_closedir closedir
#define s_opendir opendir
#define s_readdir readdir
#define s_rewinddir rewinddir
#define s_seekdir seekdir
#define s_telldir telldir

/*
 *	The following are used to determine when there are nodes in the
 *	tree that have never been matched during an archive scan, or when
 *	an archive scan can be terminated because no more matches will
 *	be found.
 *
 *	When the tree is built, a count of the number of nodes is kept
 *	in "nnodes".  As inquires are made, via the path_type function,
 *	about pathnames encountered while scanning an archive, the
 *	appropriate nodes are marked as "matched" if they represent
 *	a component of any selected pathname.
 *
 *	A count of the matched nodes is kept so that it can be compared
 *	against the count to total nodes to quickly determine if there
 *	are any nodes that have not yet been matched.
 *
 *	The conditions for determining when no more matches are possible
 *	are considerably more tricky.  The current implementation does
 *	not attempt to deal with all possible conditions but simply
 *	elects to continue scanning an archive for more matches if
 *	any filename patterns are matched.  This is done via the boolean
 *	variable "wildcards", which is set TRUE whenever the first pattern
 *	expansion matches a node name.  Whenever this variable is TRUE,
 *	path_type will never return FINISHED.  If the wildcards is FALSE,
 *	and nnodes equals nmatched, and find_type returns NOMATCH, then
 *	the NOMATCH is changed to FINISHED since all nodes have been
 *	matched and no patterns were encountered.
 *
 *	In summary, after an archive has been processed, if the count
 *	of nodes in tree (nnodes) and count of nodes matched (nmatched)
 *	does not match then the user specified some files which were
 *	never found in the archive, or some of the files specified were not
 *	processed. In either case, the tree can be walked to find the
 *	pathnames of those files and issue the appropriate warnings.
 */
 
static int nnodes = 1;			/* Count of nodes in tree */
static int nmatched = 0;		/* Count of nodes processed */
static BOOLEAN wildcards = FALSE;	/* Filename patterns encountered */

/*
 *	Local functions.
 */
 
static BOOLEAN add_leaf ();		/* Add pathname component to buffer */
static BOOLEAN do_it ();		/* Execute function */
static VOID clear_matches ();		/* Clear match bits in tree nodes */
static VOID do_dir ();			/* Open directory and walk it */
static VOID not_matched ();		/* Process nodes not matched */
static VOID save_it ();			/* Save file by name */
static VOID walk_subtree ();		/* Internal tree walk */
static struct node *add_path ();	/* Add path to tree */
static struct node *find_child ();	/* Find child with given name */
static struct node *make_child ();	/* Add a node as immediate child */
static struct node *make_node ();	/* Create a new node */

/*
 *  FUNCTION
 *
 *	path_type    find type of path
 *
 *  SYNOPSIS
 *
 *	int path_type (name)
 *	char *name;
 *
 *  DESCRIPTION
 *
 *	Invokes the internal routine find_type to determine
 *	the type of a path in tree.  Simply 
 *	calls find_type with the name and a pointer to the
 *	root node.  Find_type then invokes itself
 *	recursively on subsets of the pathname until a result
 *	is obtained.
 *
 *	Note the very important result that if there is no
 *	tree (root node has no children) then ALL names
 *	will match.  Fortuitously, this is exactly what we want.
 *
 *	Note the special handling of NOMATCH when there is a
 *	user specified tree, all nodes in the tree have been
 *	matched, and no filename patterns were encountered in the
 *	process of matching the tree.  In this case, the
 *	return value is converted to FINISHED to indicate that
 *	no more matches are possible.
 *
 */

int path_type (name)
char *name;
{
    register int type;

    DBUG_ENTER ("path_type");
    DBUG_PRINT ("tree", ("test path \"%s\"", name));
    type = find_type (&root, name);
    if (nnodes > 1 && nnodes == nmatched && !wildcards && type == NOMATCH) {
	type = FINISHED;
    }
    DBUG_PRINT ("ptype", ("path type is %d", type));
    DBUG_RETURN (type);
}

/*
 *  FUNCTION
 *
 *	tree_add    add a pathname to tree
 *
 *  SYNOPSIS
 *
 *	VOID tree_add (name)
 *	char *name;
 *
 *  DESCRIPTION
 *
 *	Takes pathname pointed to by "name" and builds appropriate
 *	nodes to add it to the current tree.
 *
 *	Note that this function simply invokes the internal tree
 *	building routine.  It was done this way to hide the
 *	fact that the internal routine returns a pointer to a node
 *	structure.  To avoid making the node structure visible
 *	outside this module, tree_add was created instead of
 *	calling add_path directly.
 *
 *	It also serves a secondary purpose of insuring that no
 *	pathname built directly from the tree will exceed the
 *	maximum name size (since it could not be archived
 *	anyway), and marking the leaf node to always be expanded
 *	if it is a directory.
 *
 */

VOID tree_add (name)
char *name;
{
    register struct node *leafnode;

    DBUG_ENTER ("tree_add");
    DBUG_PRINT ("tree", ("add pathname \"%s\" to tree", name));
    if (s_strlen (name) > (NAMESIZE - 1)) {
	bru_error (ERR_BIGPATH, name);
    } else {
	leafnode = add_path (name);
	SET_EXPAND (leafnode);
    }
    DBUG_VOID_RETURN;
}

/*
 *  FUNCTION
 *
 *	add_path    add a pathname to the tree
 *
 *  SYNOPSIS
 *
 *	static struct node *add_path (name)
 *	char *name;
 *
 *  DESCRIPTION
 *
 *	Takes a pointer to a complete pathname, such as "/usr/bin/sh",
 *	and creates nodes as appropriate to add the pathname to the
 *	current tree structure.
 *
 *	Note that add_path is called recursively until the pathname
 *	is "picked apart into pieces" (starting from right end), then
 *	the recursion "unwinds", making nodes and adding them to the tree
 *	as it goes, while rejoining the pathname components.  The end
 *	result is the name string restored to its initial condition, and a
 *	pointer to the current bottom of the tree where nodes are
 *	being added.
 *
 */

/*
 *  PSEUDO CODE
 *
 *	Begin add_path
 *	    If path has more than one component then
 *		Split path into stem and leaf parts
 *		Add stem to tree
 *		If leaf is not null string then
 *		    Add leaf to tree
 *		End if
 *		Rejoin stem and leaf components
 *	    Else
 *		Add leaf to tree
 *	    End if
 *	    Return pointer to leaf node
 *	End add_path
 *
 */

static struct node *add_path (name)
char *name;
{
    register char *leaf;
    register struct node *np;

    DBUG_ENTER ("add_path");
    DBUG_PRINT ("tree", ("add path \"%s\"", name));
    if ((leaf = s_strrchr (name, '/')) != NULL) {
	*leaf++ = EOS;
	np = add_path (name);
	if (*leaf != EOS) {
	    np = make_child (np, leaf);
	}
	*--leaf = '/';
    } else {
	np = make_child (&root, name);
    }
    DBUG_RETURN (np);
}

/*
 *  FUNCTION
 *
 *	make_child    make a child node with given name
 *
 *  SYNOPSIS
 *
 *	static struct node *make_child (np, child)
 *	struct node *np;
 *	char *child;
 *
 *  DESCRIPTION
 *
 *	Given pointer to a father node (np) and the name of a potential
 *	child node, makes a child with given name if one does not
 *	already exist.
 *
 *	Returns pointer to the child node, which can potentially be
 *	used as the father of another node during tree growth.
 *
 *	Note that it is simplier to add siblings at the head
 *	of the list rather than searching for the tail and adding
 *	a new sibling there.  However, this confuses the user
 *	because when the tree is walked, files are archived in
 *	reverse order.  Thus we go to a little more trouble and
 *	add siblings at the end of the list.
 *
 */

/*
 *  PSEUDO CODE
 *
 *	Begin make_child
 *	    If there are no children for the father node then
 *		Unconditionally make node with given name
 *		Make it the first child of the father node
 *	    Else
 *		Look for child in the sibling list
 *		If no child was found during sibling scan then
 *		    Make a new node with given name
 *		    Scan for end of sibling list
 *		    Add the new node at end of sibling list
 *		End if
 *	    End if
 *	    Return pointer to child node
 *	End make_child
 *
 */

static struct node *make_child (np, child)
struct node *np;
char *child;
{
    register struct node *cnp;		/* Child node pointer */
    register struct node *sp;		/* Sibling list scan pointer */

    DBUG_ENTER ("make_child");
    DBUG_PRINT ("tree", ("add node \"%s\" under \"%s\"", child, np -> name));
    if (np -> child == NULL) {
	cnp = make_node (child);
	np -> child = cnp;
    } else {
	cnp = find_child (np, child);
	if (cnp == NULL) {
	    cnp = make_node (child);
	    for (sp = np -> child; sp -> sibling != NULL; sp = sp -> sibling) {;}
	    sp -> sibling = cnp;
	}
    }
    DBUG_RETURN (cnp);
}

/*
 *  FUNCTION
 *
 *	make_node    make a new node with given name
 *
 *  SYNOPSIS
 *
 *	static struct node *make_node (name)
 *	char *name;
 *
 *  DESCRIPTION
 *
 *	Allocate and initialize a new node with the given name.
 *	Note that memory for the node and a fresh copy of the name
 *	are obtained from the standard memory allocator.
 *
 *	Returns pointer to the new node.
 *
 */

/*
 *  PSEUDO CODE
 *
 *	Begin make_node
 *	    Increment count of nodes in tree
 *	    Allocate memory for the node structure
 *	    Allocate memory for copy of name
 *	    Copy name to the allocated area
 *	    Initialize pointer to child to NULL
 *	    Initialize sibling list pointer to NULL
 *	    Return pointer to new node
 *	End make_node
 *
 */

static struct node *make_node (name)
char *name;
{
    register struct node *new;

    DBUG_ENTER ("make_node");
    nnodes++;
    DBUG_PRINT ("nalloc", ("making node \"%s\", number %d", name, nnodes));
    new = (struct node *) get_memory (sizeof (struct node), TRUE);
    new -> name = (char *) get_memory ((UINT) (s_strlen (name) + 1), TRUE);
    (VOID) s_strcpy (new -> name, name);
    new -> child = NULL;
    new -> sibling = NULL;
    new -> nflags = 0;
    DBUG_RETURN (new);
}    

/*
 *  FUNCTION
 *
 *	find_child    find the child node with given name
 *
 *  SYNOPSIS
 *
 *	static struct node *find_child (np, name)
 *	struct node *np;
 *	char *name;
 *
 *  DESCRIPTION
 *
 *	Given pointer to a father node and pointer to a potential
 *	child name, locates the child node with the given name
 *	and returns it's pointer.
 *
 *	Returns NULL if no child with the given name.
 *
 */

/*
 *  PSEUDO CODE
 *
 *	Begin find_child
 *	    For each child in the child list
 *		If the child node has the desired name then
 *		    Break child list scan loop
 *		End if
 *	    End for
 *	    Return pointer to child found or NULL
 *	End find_child
 *
 */

static struct node *find_child (np, name)
register struct node *np;
register char *name;
{
    DBUG_ENTER ("find_child");
    DBUG_PRINT ("tree", ("find child \"%s\"", name));
    for (np = np -> child; np != NULL; np = np -> sibling) {
	if (STRSAME (np -> name, name)) {
	    break;
	}
    }
    DBUG_RETURN (np);
}

/*
 *  FUNCTION
 *
 *	tree_walk    walk current directory tree executing function
 *
 *  SYNOPSIS
 *
 *	VOID tree_walk (funcp)
 *	VOID (*funcp)();
 *
 *  DESCRIPTION
 *
 *	Walks the directory tree in pre-order, executing given function
 *	for each node.  Passes an initialized finfo (file info) struct
 *	pointer to the function executed.
 *
 */

/*
 *  PSEUDO CODE
 *
 *	Begin tree_walk
 *	    Erase current name in buffer
 *	    If there is a tree other than static root node then
 *		Call internal walk function root node's first child
 *	    End if
 *	End tree_walk
 *
 */

VOID tree_walk (funcp)
VOID (*funcp)();
{
    DBUG_ENTER ("tree_walk");
    namebuf[0] = EOS;
    if (root.child != NULL) {
	walk_subtree (root.child, funcp, namebuf);
    }
    DBUG_VOID_RETURN;
}

/*
 *  FUNCTION
 *
 *	walk_subtree    call function for single tree node
 *
 *  SYNOPSIS
 *
 *	static VOID walk_subtree (np, funcp, cp)
 *	register struct node *np;
 *	VOID (*funcp)();
 *	char *cp;
 *
 *  DESCRIPTION
 *
 *	Given pointer to a tree node (np), a pointer to a
 *	function to execute (funcp), and a pointer to name buffer (cp),
 *	builds a complete pathname from this node's name and
 *	previous stem in pathname buffer, and invokes function
 *	with the complete pathname as the argument.
 *	Then walks any children trees, and finally sibling
 *	trees.
 *
 */

/*
 *  PSEUDO CODE
 *
 *	Begin walk_subtree
 *	    If new leaf can be added to end of name then
 *		Find and remember current end of name
 *		If the file is successfully archived then
 *		    If there are no children or always expand then
 *			If the file is a directory then
 *			    Remember device of explicit directory
 *			    Walk subtree rooted in directory
 *			End if
 *		    End if
 *		    If there is a child node then
 *			Make current name a stem
 *			Walk the child subtree
 *			Rejoin stem with rest of name
 *		    End if
 *		End if
 *		Terminate current name
 *	    End if
 *	    If there are any sibling subtrees then
 *		Walk the next sibling subtree
 *	    End if
 *	End walk_subtree
 *
 */

static VOID walk_subtree (np, funcp, cp)
register struct node *np;
VOID (*funcp) ();
char *cp;
{
#ifndef ALMOST_WORKS
    register char *end;

    DBUG_ENTER ("walk_subtree");
    DBUG_PRINT ("walk", ("stem \"%s\"", namebuf));
    DBUG_PRINT ("walk", ("node \"%s\"", np -> name));
    if (add_leaf (np -> name, cp)) {
	STREND (namebuf, end);
	if (do_it (funcp, FALSE)) {
	    if (np -> child == NULL || EXPAND (np)) {
		if (IS_DIR (sbuf.st_mode)) {
		    dirdev = sbuf.st_dev;
		    DBUG_PRINT ("dirdev", ("dirdev = %u", dirdev));
		    do_dir (funcp);
		}
	    }
	    if (np -> child != NULL) {
		*end++ = '/';
		walk_subtree (np -> child, funcp, end);
		*--end = '/';
	    }
	}
	*cp = EOS;
    }
    if (np -> sibling != NULL) {
	walk_subtree (np -> sibling, funcp, cp);
    }
    DBUG_VOID_RETURN;
#else
    /*
     * This experimental code is almost sufficient to let us do
     * our own wild card expansion, as thus also implement leading '!'
     * for archive creation as well as extraction, as well as not
     * depending upon the shell for wildcard expansion (for non-unix).
     *
     * The problem is the way the tree is stored.  When called the
     * first time with a pathname like "/usr/src/cmd", both namebuf and
     * np -> are "" (null strings), so no first level match is found.
     * When the tree manipulation code is cleaned up, or possibly kludged
     * up a little more, maybe this will work...
     */
    register char *end;
    register BOOLEAN deof;
    register DIR *dirp;
    register long dirpos;
    auto struct stat64 dsbuf;	/* Dummy stat buffer for finfo_init */
    auto struct finfo dirf;
#ifdef sgi
    auto struct dirent *dbuf;
#else
    auto struct direct *dbuf;
#endif

    DBUG_ENTER ("walk_subtree");
    DBUG_PRINT ("walk", ("stem \"%s\"", namebuf));
    DBUG_PRINT ("walk", ("node \"%s\"", np -> name));
    finfo_init (&dirf, namebuf, &dsbuf);
    deof = FALSE;
    dirpos = 0L;
    dirp = NULL;
    if (STRSAME ("", dirf.fname)) {
	dirf.fname = ".";
    }
    DBUG_PRINT ("dirf", ("looking inside directory '%s'", dirf.fname));
    while (!deof) {
	if (dirp == NULL) {
	    if (file_stat (&dirf)) {
		dirp = s_opendir (dirf.fname);
		if (dirp == NULL) {
		    bru_error (ERR_OPEN, dirf.fname);
		}
	    }
	}
	if (dirp == NULL) {
	    DBUG_PRINT ("dir", ("dirp == NULL (eof on directory)"));
	    break;
	}

	/*
	 * A seek to a saved position immediately after an open should
	 * be ok -- see the man page. If dirp still open (e.g. looping
	 * after a dot file) then for sure the seek will work.
	 *
	 * In any case, seekdir is of type void, so it is impossible to
	 * check its return value like the old code did.
	 */
	s_seekdir (dirp, dirpos);
	if ((dbuf = s_readdir (dirp)) == NULL) {
	    DBUG_PRINT ("dir", ("found eof (readdir returned NULL)"));
	    deof = TRUE;
	} else if (DOTFILE (dbuf -> d_name)) {
	    DBUG_PRINT ("dir", ("found dotfile \"%s\"", dbuf -> d_name));
	} else {
	    if (wild (dbuf -> d_name, np -> name)) {
		DBUG_PRINT ("dirf", ("process \"%s\"", dbuf -> d_name));
		if (add_leaf (dbuf -> d_name, cp)) {
		    STREND (namebuf, end);
		    dirpos = s_telldir (dirp);
		    s_closedir (dirp);
		    dirp = NULL;
		    if (do_it (funcp, FALSE)) {
			if (np -> child == NULL || EXPAND (np)) {
			    if (IS_DIR (sbuf.st_mode)) {
				dirdev = sbuf.st_dev;
				DBUG_PRINT ("dirdev", ("dirdev = %u", dirdev));
				do_dir (funcp);
			    }
			}
			if (np -> child != NULL) {
			    *end++ = '/';
			    walk_subtree (np -> child, funcp, end);
			    *--end = '/';
			}
		    }
		    *cp = EOS;
		    reset_times (&dirf);
		}
		if (np -> sibling != NULL) {
		    walk_subtree (np -> sibling, funcp, cp);
		}
	    }
	}
	if (dirp != NULL) {
	    dirpos = s_telldir (dirp);
	}
    }
    if (dirp != NULL) {
	s_closedir (dirp);
    }
    DBUG_VOID_RETURN;
#endif
}

/*
 *  FUNCTION
 *
 *	do_it    turn control over from tree walk to given function
 *
 *  SYNOPSIS
 *
 *	static BOOLEAN do_it (funcp, expanding)
 *	VOID (*funcp) ();
 *	BOOLEAN expanding;
 *
 *  DESCRIPTION
 *
 *	Given pointer to a function to execute, and flag indicating
 *	whether we are currently expanding an explicitly named
 *	directory, finishes some bookkeeping and turns control
 *	over to the function to be executed at every node
 *	in the tree.
 *
 *	Returns TRUE if control was turned over to the function,
 *	FALSE otherwise.
 *
 */

/*
 *  PSEUDO CODE
 *
 *	Begin do_it
 *	    Initialize file information structure for node
 *	    Default return is FALSE
 *	    If the file name is null then
 *		Use root as the name
 *	    End if
 *	    If the file is accessible for read then
 *		If the file can be stat'd then
 *		    If we are not expanding a directory then
 *			Always execute the function when not expanding
 *		    Else if excluding remote files and file is remote then
 *			Exclude this file by not calling function
 *		    Else if limiting to mounted file system and outside then
 *			Exclude this file by not calling function
 *		    Else
 *			No other reason to exclude, so go ahead with function
 *		    End if
 *		    If found that we were to continue then
 *			Go ahead and execute function
 *		    End if
 *		End if
 *	    End if
 *	End do_it
 *
 */

#include	<sys/statvfs.h>	/* for local file check */

static BOOLEAN do_it (funcp, expanding)
VOID (*funcp)();
BOOLEAN expanding;
{
    register BOOLEAN rtnval;
    auto struct finfo file;
    struct statvfs vfs;

    DBUG_ENTER ("do_it");
    DBUG_PRINT ("tree", ("archive \"%s\"", namebuf));
    finfo_init (&file, namebuf, &sbuf);
    rtnval = FALSE;
    if (STRSAME ("", file.fname)) {
	file.fname = "/";
    }
    if (file_access (file.fname, A_READ, TRUE)) {
	if (file_stat (&file)) {
	    if (!expanding)
		rtnval = TRUE;
	    else if (flags.mflag && file.statp -> st_dev != dirdev)
		rtnval = FALSE;
	    else if (flags.Rflag) {
		if(IS_FLNK(file.statp->st_mode) ||
		    (statvfs(file.fname, &vfs) == 0 && (vfs.f_flag&ST_LOCAL)))
		    rtnval = TRUE;
		else
		    rtnval = FALSE;
	    }
	    else
		rtnval = TRUE;
	    if (rtnval)
		(*funcp) (&file);
	}
    }
    DBUG_RETURN (rtnval);
}

/*
 *  FUNCTION
 *
 *	do_dir    expand a directory
 *
 *  SYNOPSIS
 *
 *	static VOID do_dir (funcp)
 *	VOID (*funcp) ();
 *
 *  DESCRIPTION
 *
 *	Recursively expands a directory, doing each regular file and
 *	expanding any subdirectories.
 *
 *	Use the Directory Access routines defined by Berkeley. On non-BSD
 *	systems, the directory stuff will be available from the public
 *	domain version written by Doug Gwyn at BRL.
 *
 *	The directory access routines do us two favors:
 *	1) They skip empty directory slots for us (where the inode number is 0)
 *	2) They automatically EOS terminate the name of the file.
 *
 *  V.3 NOTE:
 *      System V.3 did away with limits on the name length of a file (at
 *      least as far as you, the poor programmer, is concerned).  Thus,
 *      we can't pre-allocate a buffer for the name.  I'm not sure why
 *      the original programmer chose to copy the string anyway - if you
 *      look at how it's used, it's never trashed. -- jmb 1/24/88
 */

static VOID do_dir (funcp)
VOID (*funcp)();
{
    auto struct stat64 dsbuf;	/* Dummy stat buffer for finfo_init */
    auto struct finfo file;
    register char *end;
    auto struct dirent *dbuf;
    register DIR *dirp = NULL;

    DBUG_ENTER ("do_dir");
    DBUG_PRINT ("dir", ("archive directory \"%s\"", namebuf));
    STREND (namebuf, end);
    finfo_init (&file, namebuf, &dsbuf);
    if (STRSAME ("", file.fname)) {
	file.fname = "/";
    }

    if (file_stat (&file)) {
	dirp = opendir (file.fname);
	if (dirp == NULL)
	    bru_error (ERR_OPEN, file.fname);
	else  {
	    while (dbuf = readdir (dirp)) {
		if(!DOTFILE (dbuf -> d_name)) {
		    DBUG_PRINT ("dir", ("found \"%s\"", dbuf -> d_name));
		    *end++ = '/';
		    save_it (dbuf -> d_name, funcp, end);
		    *--end = EOS;
		    reset_times (&file);
		}
	    }
	    closedir (dirp);
	}
    }
    DBUG_VOID_RETURN;
}

/*
 *  FUNCTION
 *
 *	add_leaf    add a new pathname component to current pathname
 *
 *  SYNOPSIS
 *
 *	static BOOLEAN add_leaf (name, cp)
 *	char *name;
 *	char *cp;
 *
 *  DESCRIPTION
 *
 *	If there is room in the name buffer to add the component 
 *	pointed to by name then adds the name and returns TRUE.
 *
 *	If the buffer is too small then notifies user and
 *	returns FALSE.
 *
 *	Note that the size condition test is such that there
 *	will always be room at the end of the buffer to append
 *	a '/' without another test.
 *
 */

static BOOLEAN add_leaf (name, cp)
char *name;
char *cp;
{
    register BOOLEAN rtnval;

    DBUG_ENTER ("add_leaf");
    if (cp + s_strlen (name) > &namebuf[NAMESIZE - 2]) {
	rtnval = FALSE;
	bru_error (ERR_DEPTH, namebuf, name);
    } else {
	rtnval = TRUE;
	(VOID) s_strcpy (cp, name);
    }
    DBUG_RETURN (rtnval);
}

static VOID save_it (name, funcp, cp)
char *name;
VOID (*funcp) ();
char *cp;
{
    DBUG_ENTER ("save_it");
    if (add_leaf (name, cp)) {
	if (do_it (funcp, TRUE)) {
	    if (IS_DIR (sbuf.st_mode)) {
		do_dir (funcp);
	    }
	}
	*cp = EOS;
    }
    DBUG_VOID_RETURN;
}

/*
 *  FUNCTION
 *
 *	find_type    test a path for type
 *
 *  SYNOPSIS
 *
 *	static int find_type (np, name)
 *	struct node *np;
 *	char *name;
 *
 *  DESCRIPTION
 *
 *	Takes a pointer to a complete pathname, such as "/usr/bin/sh",
 *	and determines its type, NOMATCH, STEM, LEAF, or EXTENSION.
 *
 *	This is done by splitting the pathname into leading and
 *	trailing components at the first '/' character, looking
 *	for a matching node with the same name as the leading
 *	component, and if found, calling find_type recursively
 *	with the trailing component.  The routine "wild" is
 *	called to expand possible pattern matching metacharacters
 *	in the same manner as the shell.
 *
 *	Returns STEM when the end of the pathname is found
 *	(a null trailing component) without finding a name
 *	mismatch, or reaching a leaf node.
 *
 *	Returns LEAF when the end of the pathname and a leaf
 *	node are reached simultaneously.
 *
 *	Returns EXTENSION when a leaf node is reached before the
 *	end of the pathname.
 *
 *	Returns NOMATCH when any mismatch is found.
 *
 *  NOTES
 *
 *	To properly understand the matching algorithm, it is
 *	important to note the following facts:
 *
 *	(1)	Matches are done from the "bottom up".  That is
 *		the current path through the tree does not match
 *		the complete pathname given until the bottom of
 *		the tree is reached or the pathname runs out of
 *		components.  If a non-match is found at any point,
 *		an alternate path through the tree will be attempted.
 *		Thus a node can only be marked as "matched" if the
 *		result of the current invocation of path_type is
 *		something other than "NOMATCH".
 *
 *	(2)	The current node pointer points to the node which
 *		represents the bottom of the path through the tree
 *		which is a possible match.  The following table
 *		represents the current node and unmatched pathname
 *		components when path_type is invoked with the
 *		pathname "a/b/c/d":
 *
 *		Call number	Node		Name points to
 *		-----------	----------	---------------
 *
 *		1		<root>		a/b/c/d
 *		2		a		b/c/d
 *		3		b		c/d
 *		4		c		d
 *		5		d		<EOS>
 *
 *		Thus the current node pointer always points to the bottom
 *		node of the subtree which represents currently matched
 *		components of the pathname.  Subtrees under the current
 *		node represent possible matches for the current name string.
 *
 *		When picking subtrees to test for matches, the subtree
 *		which has a root node exactly matching the current leading
 *		pathname component is tried first.  Then all other subtrees
 *		which have pattern matches are tried.  If this fails then
 *		no match is found.
 *	
 *
 *	(3)	Matches cannot be done "top down" because at any node
 *		in the tree, more than one child node may match the
 *		next component, and the "correct" one can only be
 *		found by testing subtrees under each of the possible
 *		child node matches.
 *
 */

/*
 *  PSEUDO CODE
 *
 *	Begin find_type
 *	    If current node has no children then
 *		If there is more name left then
 *		    Name is extension of tree
 *		Else
 *		    Name is leaf of tree
 *		End if
 *	    Else
 *		If name has been exhausted then
 *		    Name is stem of tree
 *		Else
 *		    Locate slash separating components
 *		    If there are two components then
 *		        Replace slash temporarily with EOS
 *		    End if
 *		    Default result is no match
 *		    Find the child with given name
 *		    If child not found then
 *			Result is type of the subtree
 *		    End if
 *		    If no match yet then
 *			For each child of the node
 *			    If not an exact name match then
 *				If child matches pattern then
 *				    Type is the type of the subtree
 *				    If match found then
 *					Break match loop
 *				    End if
 *				End if
 *			    End if
 *			End for
 *		    End if
 *		    If slash was replaced then
 *		        Restore it
 *		    End if
 *		End if
 *	    End if
 *	    If result is something other than "no match" then
 *		If this node not yet marked as matched then
 *		    Mark it matched
 *		    Increment count of nodes matched
 *		End if
 *	    End if
 *	    Return result
 *	End find_type
 *
 */

static int find_type (np, name)
struct node *np;
char *name;
{
    register int result;
    register char *sp;
    register struct node *cp;

    DBUG_ENTER ("find_type");
    DBUG_PRINT ("tree", ("test path \"%s\"", name?name:""));
    if (np -> child == NULL) {
	if (name != NULL) {
	    result = EXTENSION;
	} else {
	    result = LEAF;
	}
    } else {
	if (name == NULL) {
	    result = STEM;
	} else {
	    sp = s_strchr (name, '/');
	    if (sp != NULL) {
		*sp++ = EOS;
	    }
	    result = NOMATCH;
	    cp = find_child (np, name);
	    if (cp != NULL) {
		result = find_type (cp, sp);
	    }
	    if (result == NOMATCH) {
		for (cp = np -> child; cp != NULL; cp = cp -> sibling) {
		    if (!STRSAME (name, cp -> name)) {
			if (wild (name, cp -> name)) {
			    wildcards = TRUE;
			    result = find_type (cp, sp);
			    if (result != NOMATCH) {
				break;
			    }
			}
		    }
		}
	    }
	    if (sp != NULL) {
		*--sp = '/';
	    }
	}
    }
    DBUG_PRINT ("tree", ("result is %d", result));
    if (!MATCHED (np)) {
	DBUG_PRINT ("mnodes", ("node \"%s\" matched", np -> name));
	SET_MATCHED (np);
	nmatched++;
    }
    DBUG_RETURN (result);
}

/*
 *  FUNCTION
 *
 *	nodes_ignored    check tree for any nodes ignored in scan
 *
 *  SYNOPSIS
 *
 *	VOID nodes_ignored ()
 *
 *  DESCRIPTION
 *
 *	Tests to see if there are any tree nodes that have not
 *	been matched.  Compares the number of leaf nodes against
 *	the number of matched nodes.  If they do not match then
 *	descends the tree in preorder, checking for individual nodes
 *	that have not been matched.
 *
 */

VOID nodes_ignored (funcp)
VOID (*funcp)();
{
    DBUG_ENTER ("nodes_ignored");
    DBUG_PRINT ("nodes", ("%d nodes, %d matched", nnodes, nmatched));
    namebuf[0] = EOS;
    if (nnodes != nmatched && root.child != NULL) {
	not_matched (root.child, funcp, namebuf);
    }
    DBUG_VOID_RETURN;
}

/*
 *  FUNCTION
 *
 *	not_matched (np, funcp, cp)
 *	register struct node *np;
 *	VOID (*funcp) ();
 *	char *cp;
 *
 *  DESCRIPTION
 *
 *	Given pointer to a node, pointer to a function to execute if
 *	the node has not been visited, and pointer to a place in the
 *	name buffer where the node's name can be appended to the
 *	current contents, builds the complete pathname in the buffer.
 *	Then tests to see if the node has any children.  If so, calls
 *	itself recursively on the child subtree.  If not, tests to see
 *	if the node has been matched.  If so, calls
 *	the function specified via funcp with the name in the
 *	name buffer.  Then recursively checks all sibling subtrees.
 *
 *	The net effect is that all leaf nodes which have never been
 *	matched result in a call to the specified function with
 *	the full pathname corresponding to the unmatched leaf node.
 *
 *	Note that the files "." and ".." are treated specially since
 *	they are never archived.  Thus if there are leaf nodes
 *	with either name, they are essentially ignored.
 *
 */
 
static VOID not_matched (np, funcp, cp)
register struct node *np;
VOID (*funcp) ();
char *cp;
{
    register char *name;
    register char *end;

    DBUG_ENTER ("not_matched");
    DBUG_PRINT ("ltest", ("stem \"%s\"", namebuf));
    DBUG_PRINT ("ltest", ("node \"%s\"", np -> name));
    if (add_leaf (np -> name, cp)) {
	if (np -> child != NULL) {
	    STREND (namebuf, end);
	    *end++ = '/';
	    not_matched (np -> child, funcp, end);
	    *--end = '/';
	} else {
	    if (!MATCHED (np) && !DOTFILE (namebuf)) {
		if (STRSAME ("", namebuf)) {
		    name = "/";
		} else {
		    name = namebuf;
		}
		(*funcp) (name);
	    }
	}
	*cp = EOS;
    }
    if (np -> sibling != NULL) {
	not_matched (np -> sibling, funcp, cp);
    }
    DBUG_VOID_RETURN;
}

/*
 *  FUNCTION
 *
 *	clear_tree    reset all match information in tree
 *
 *  SYNOPSIS
 *
 *	VOID clear_tree ()
 *
 *  DESCRIPTION
 *
 *	Is used between archive scans to reset all match information
 *	in the tree so that successive passes can be made over an
 *	archive.  It must reset the following items:
 *
 *	1.	The match bits in the flag word of each node.
 *	2.	The number of nodes matched.
 *	3.	The "wildcard used" flag.
 *
 */

VOID clear_tree ()
{
    DBUG_ENTER ("clear_tree");
    nmatched = 0;
    wildcards = FALSE;
    clear_matches (&root);
    DBUG_VOID_RETURN;
}

/*
 *  FUNCTION
 *
 *	clear_matches    clear match bits in nodes
 *
 *  SYNOPSIS
 *
 *	static VOID clear_matches (np)
 *	struct node *np;
 *
 *  DESCRIPTION
 *
 *	Given pointer to a node, clears the match bits in the flag
 *	word of the given node, all child nodes of the given node,
 *	and all sibling nodes of the given node.
 *
 */
 
static VOID clear_matches (np)
struct node *np;
{
    DBUG_ENTER ("clear_matches");
    RSET_MATCHED (np);
    if (np -> child != NULL) {
	clear_matches (np -> child);
    }
    if (np -> sibling != NULL) {
	clear_matches (np -> sibling);
    }
    DBUG_VOID_RETURN;
}
