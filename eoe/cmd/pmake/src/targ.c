/*-
 * targ.c --
 *	Functions for maintaining the Lst allTargets. Target nodes are
 * kept in two structures: a Lst, maintained by the list library, and a
 * hash table, maintained by the hash library.
 *
 * Copyright (c) 1988, 1989 by the Regents of the University of California
 * Copyright (c) 1988, 1989 by Adam de Boor
 * Copyright (c) 1989 by Berkeley Softworks
 *
 * Permission to use, copy, modify, and distribute this
 * software and its documentation for any non-commercial purpose
 * and without fee is hereby granted, provided that the above copyright
 * notice appears in all copies.  The University of California,
 * Berkeley Softworks and Adam de Boor make no representations about
 * the suitability of this software for any purpose.  It is provided
 * "as is" without express or implied warranty.
 *
 * Interface:
 *	Targ_Init 	    	Initialization procedure.
 *
 *	Targ_NewGN	    	Create a new GNode for the passed target
 *	    	  	    	(string). The node is *not* placed in the
 *	    	  	    	hash table, though all its fields are
 *	    	  	    	initialized.
 *
 *	Targ_FindNode	    	Find the node for a given target, creating
 *	    	  	    	and storing it if it doesn't exist and the
 *	    	  	    	flags are right (TARG_CREATE)
 *
 *	Targ_FindList	    	Given a list of names, find nodes for all
 *	    	  	    	of them. If a name doesn't exist and the
 *	    	  	    	TARG_NOCREATE flag was given, an error message
 *	    	  	    	is printed. Else, if a name doesn't exist,
 *	    	  	    	its node is created.
 *
 *	Targ_Ignore	    	Return TRUE if errors should be ignored when
 *	    	  	    	creating the given target.
 *
 *	Targ_Silent	    	Return TRUE if we should be silent when
 *	    	  	    	creating the given target.
 *
 *	Targ_Precious	    	Return TRUE if the target is precious and
 *	    	  	    	should not be removed if we are interrupted.
 *
 * Debugging:
 *	Targ_PrintGraph	    	Print out the entire graphm all variables
 *	    	  	    	and statistics for the directory cache. Should
 *	    	  	    	print something for suffixes, too, but...
 */
#if !defined(lint) && defined(keep_rcsid)
static char     *rcsid = "Id: targ.c,v 1.38 89/11/14 13:44:15 adam Exp $ SPRITE (Berkeley)";
#endif lint

#include	  <stdio.h>
#include	  <time.h>
#include	  <tzfile.h>
#include	  "make.h"
#include	  "hash.h"

static Lst        allTargets;	/* the list of all targets found so far */
static Hash_Table targets;	/* a hash table of same */

#define HTSIZE	191		/* initial size of hash table */

/*-
 *-----------------------------------------------------------------------
 * Targ_Init --
 *	Initialize this module
 *
 * Results:
 *	None
 *
 * Side Effects:
 *	The allTargets list and the targets hash table are initialized
 *-----------------------------------------------------------------------
 */
void
Targ_Init ()
{
    allTargets = Lst_Init (FALSE);
    Hash_InitTable (&targets, HTSIZE, HASH_STRING_KEYS);
}

/*-
 *-----------------------------------------------------------------------
 * Targ_NewGN  --
 *	Create and initialize a new graph node
 *
 * Results:
 *	An initialized graph node with the name field filled with a copy
 *	of the passed name
 *
 * Side Effects:
 *	None.
 *-----------------------------------------------------------------------
 */
GNode *
Targ_NewGN (name)
    char           *name;	/* the name to stick in the new node */
{
    register GNode *gn;

    gn = (GNode *) malloc (sizeof (GNode));
    gn->name = Str_New (name);
    gn->path = (char *) 0;
    if (name[0] == '-' && name[1] == 'l') {
	gn->type = OP_LIB;
    } else {
	gn->type = 0;
    }
    gn->unmade =    	0;
    gn->make = 	    	FALSE;
    gn->made = 	    	UNMADE;
    gn->childMade = 	FALSE;
    gn->mtime = gn->cmtime = 0;
    gn->iParents =  	Lst_Init (FALSE);
    gn->cohorts =   	Lst_Init (FALSE);
    gn->parents =   	Lst_Init (FALSE);
    gn->children =  	Lst_Init (FALSE);
    gn->successors = 	Lst_Init(FALSE);
    gn->preds =     	Lst_Init(FALSE);
    gn->context =   	Lst_Init (FALSE);
    gn->commands =  	Lst_Init (FALSE);

    return (gn);
}

/*-
 *-----------------------------------------------------------------------
 * Targ_FindNode  --
 *	Find a node in the list using the given name for matching
 *
 * Results:
 *	The node in the list if it was. If it wasn't, return NILGNODE of
 *	flags was TARG_NOCREATE or the newly created and initialized node
 *	if it was TARG_CREATE
 *
 * Side Effects:
 *	Sometimes a node is created and added to the list
 *-----------------------------------------------------------------------
 */
GNode *
Targ_FindNode (name, flags)
    char           *name;	/* the name to find */
    int             flags;	/* flags governing events when target not
				 * found */
{
    GNode         *gn;	      /* node in that element */
    Hash_Entry	  *he;	      /* New or used hash entry for node */
    Boolean	  isNew;      /* Set TRUE if Hash_CreateEntry had to create */
			      /* an entry for the node */


    if (flags & TARG_CREATE) {
	he = Hash_CreateEntry (&targets, name, &isNew);
	if (isNew) {
	    gn = Targ_NewGN (name);
	    Hash_SetValue (he, gn);
	    (void) Lst_AtEnd (allTargets, (ClientData)gn);
	}
    } else {
	he = Hash_FindEntry (&targets, name);
    }

    if (he == (Hash_Entry *) NULL) {
	return (NILGNODE);
    } else {
	return ((GNode *) Hash_GetValue (he));
    }
}

/*-
 *-----------------------------------------------------------------------
 * Targ_FindList --
 *	Make a complete list of GNodes from the given list of names 
 *
 * Results:
 *	A complete list of graph nodes corresponding to all instances of all
 *	the names in names. 
 *
 * Side Effects:
 *	If flags is TARG_CREATE, nodes will be created for all names in
 *	names which do not yet have graph nodes. If flags is TARG_NOCREATE,
 *	an error message will be printed for each name which can't be found.
 * -----------------------------------------------------------------------
 */
Lst
Targ_FindList (names, flags)
    Lst        	   names;	/* list of names to find */
    int            flags;	/* flags used if no node is found for a given
				 * name */
{
    Lst            nodes;	/* result list */
    register LstNode  ln;		/* name list element */
    register GNode *gn;		/* node in tLn */
    char    	  *name;

    nodes = Lst_Init (FALSE);

    if (Lst_Open (names) == FAILURE) {
	return (nodes);
    }
    while ((ln = Lst_Next (names)) != NILLNODE) {
	name = (char *)Lst_Datum(ln);
	gn = Targ_FindNode (name, flags);
	if (gn != NILGNODE) {
	    /*
	     * Note: Lst_AtEnd must come before the Lst_Concat so the nodes
	     * are added to the list in the order in which they were
	     * encountered in the makefile.
	     */
	    (void) Lst_AtEnd (nodes, (ClientData)gn);
	    if (gn->type & OP_DOUBLEDEP) {
		(void)Lst_Concat (nodes, gn->cohorts, LST_CONCNEW);
	    }
	} else if (flags == TARG_NOCREATE) {
	    Error ("\"%s\" -- target unknown.", name);
	}
    }
    Lst_Close (names);
    return (nodes);
}

/*-
 *-----------------------------------------------------------------------
 * Targ_Ignore  --
 *	Return true if should ignore errors when creating gn
 *
 * Results:
 *	TRUE if should ignore errors
 *
 * Side Effects:
 *	None
 *-----------------------------------------------------------------------
 */
Boolean
Targ_Ignore (gn)
    GNode          *gn;		/* node to check for */
{
    if (ignoreErrors || gn->type & OP_IGNORE) {
	return (TRUE);
    } else {
	return (FALSE);
    }
}

/*-
 *-----------------------------------------------------------------------
 * Targ_Silent  --
 *	Return true if be silent when creating gn
 *
 * Results:
 *	TRUE if should be silent
 *
 * Side Effects:
 *	None
 *-----------------------------------------------------------------------
 */
Boolean
Targ_Silent (gn)
    GNode          *gn;		/* node to check for */
{
    if (beSilent || gn->type & OP_SILENT) {
	return (TRUE);
    } else {
	return (FALSE);
    }
}

/*-
 *-----------------------------------------------------------------------
 * Targ_Precious --
 *	See if the given target is precious
 *
 * Results:
 *	TRUE if it is precious. FALSE otherwise
 *
 * Side Effects:
 *	None
 *-----------------------------------------------------------------------
 */
Boolean
Targ_Precious (gn)
    GNode          *gn;		/* the node to check */
{
    if (allPrecious || (gn->type & (OP_PRECIOUS|OP_DOUBLEDEP))) {
	return (TRUE);
    } else {
	return (FALSE);
    }
}

/******************* DEBUG INFO PRINTING ****************/

static GNode	  *mainTarg;	/* the main target, as set by Targ_SetMain */
/*- 
 *-----------------------------------------------------------------------
 * Targ_SetMain --
 *	Set our idea of the main target we'll be creating. Used for
 *	debugging output.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	"mainTarg" is set to the main target's node.
 *-----------------------------------------------------------------------
 */
void
Targ_SetMain (gn)
    GNode   *gn;  	/* The main target we'll create */
{
    mainTarg = gn;
}

static int
TargPrintName (gn, ppath)
    GNode          *gn;
    int		    ppath;
{
    printf ("%s ", gn->name);
#ifdef notdef
    if (ppath) {
	if (gn->path) {
	    printf ("[%s]  ", gn->path);
	}
	if (gn == mainTarg) {
	    printf ("(MAIN NAME)  ");
	}
    }
#endif notdef
    return (0);
}


int
Targ_PrintCmd (cmd)
    char           *cmd;
{
    printf ("\t%s\n", cmd);
    return (0);
}

/*-
 *-----------------------------------------------------------------------
 * Targ_FmtTime --
 *	Format a modification time in some reasonable way and return it.
 *
 * Results:
 *	The time reformatted.
 *
 * Side Effects:
 *	The time is placed in a static area, so it is overwritten
 *	with each call.
 *
 *-----------------------------------------------------------------------
 */
char *
Targ_FmtTime (time)
    int    time;
{
    struct tm	  	*parts;
    static char	  	buf[40];
    static char	  	*months[] = {
	"Jan", "Feb", "Mar", "Apr", "May", "Jun",
	"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
    };

    parts = localtime(&time);

    sprintf (buf, "%d:%02d:%02d %s %d, %d",
	     parts->tm_hour, parts->tm_min, parts->tm_sec,
	     months[parts->tm_mon], parts->tm_mday, parts->tm_year+TM_YEAR_BASE);
    return(buf);
}
    
/*-
 *-----------------------------------------------------------------------
 * Targ_PrintType --
 *	Print out a type field giving only those attributes the user can
 *	set.
 *
 * Results:
 *
 * Side Effects:
 *
 *-----------------------------------------------------------------------
 */
void
Targ_PrintType (type)
    register int    type;
{
    register int    tbit;
    
#ifdef __STDC__
#define PRINTBIT(attr)	case CONCAT(OP_,attr): printf("." #attr " "); break
#define PRINTDBIT(attr) case CONCAT(OP_,attr): if (DEBUG(TARG)) printf("." #attr " "); break
#else
#define PRINTBIT(attr) 	case CONCAT(OP_,attr): printf(".attr "); break
#define PRINTDBIT(attr)	case CONCAT(OP_,attr): if (DEBUG(TARG)) printf(".attr "); break
#endif /* __STDC__ */

    type &= ~OP_OPMASK;

    while (type) {
	tbit = 1 << (ffs(type) - 1);
	type &= ~tbit;

	switch(tbit) {
	    PRINTBIT(DONTCARE);
	    PRINTBIT(USE);
	    PRINTBIT(EXEC);
	    PRINTBIT(IGNORE);
	    PRINTBIT(PRECIOUS);
	    PRINTBIT(SILENT);
	    PRINTBIT(MAKE);
	    PRINTBIT(JOIN);
	    PRINTBIT(EXPORT);
	    PRINTBIT(NOEXPORT);
	    PRINTBIT(EXPORTSAME);
	    PRINTBIT(INVISIBLE);
	    PRINTBIT(NOTMAIN);
	    PRINTDBIT(LIB);
	    PRINTBIT(M68020);
	    /*XXX: MEMBER is defined, so CONCAT(OP_,MEMBER) gives OP_"%" */
	    case OP_MEMBER: if (DEBUG(TARG)) printf(".MEMBER "); break;
	    PRINTDBIT(ARCHV);
	}
    }
}

/*-
 *-----------------------------------------------------------------------
 * TargPrintNode --
 *	print the contents of a node
 *-----------------------------------------------------------------------
 */
static int
TargPrintNode (gn, pass)
    GNode         *gn;
    int	    	  pass;
{
    if (!OP_NOP(gn->type)) {
	printf("#\n");
	if (gn == mainTarg) {
	    printf("# *** MAIN TARGET ***\n");
	}
	if (pass == 2) {
	    if (gn->unmade) {
		printf("# %d unmade children\n", gn->unmade);
	    } else {
		printf("# No unmade children\n");
	    }
	    if (! (gn->type & (OP_JOIN|OP_USE|OP_EXEC))) {
		if (gn->mtime != 0) {
		    printf("# last modified %s: %s\n",
			      Targ_FmtTime(gn->mtime),
			      (gn->made == UNMADE ? "unmade" :
			       (gn->made == MADE ? "made" :
				(gn->made == UPTODATE ? "up-to-date" :
				 "error when made"))));
		} else if (gn->made != UNMADE) {
		    printf("# non-existent (maybe): %s\n",
			      (gn->made == MADE ? "made" :
			       (gn->made == UPTODATE ? "up-to-date" :
				(gn->made == ERROR ? "error when made" :
				 "aborted"))));
		} else {
		    printf("# unmade\n");
		}
	    }
	    if (!Lst_IsEmpty (gn->iParents)) {
		printf("# implicit parents: ");
		Lst_ForEach (gn->iParents, TargPrintName, (ClientData)0);
		putc ('\n', stdout);
	    }
	}
	if (!Lst_IsEmpty (gn->parents)) {
	    printf("# parents: ");
	    Lst_ForEach (gn->parents, TargPrintName, (ClientData)0);
	    putc ('\n', stdout);
	}
	
	printf("%-16s", gn->name);
	switch (gn->type & OP_OPMASK) {
	    case OP_DEPENDS:
		printf(": "); break;
	    case OP_FORCE:
		printf("! "); break;
	    case OP_DOUBLEDEP:
		printf(":: "); break;
	}
	Targ_PrintType (gn->type);
	Lst_ForEach (gn->children, TargPrintName, (ClientData)0);
	putc ('\n', stdout);
	Lst_ForEach (gn->commands, Targ_PrintCmd, (ClientData)0);
	printf("\n\n");
	if (gn->type & OP_DOUBLEDEP) {
	    Lst_ForEach (gn->cohorts, TargPrintNode, (ClientData)pass);
	}
    }
    return (0);
}

/*-
 *-----------------------------------------------------------------------
 * TargPrintOnlySrc --
 *	Print only those targets that are just a source.
 *
 * Results:
 *	0.
 *
 * Side Effects:
 *	The name of each file is printed preceeded by #\t
 *
 *-----------------------------------------------------------------------
 */
static int
TargPrintOnlySrc(gn)
    GNode   	  *gn;
{
    if (OP_NOP(gn->type)) {
	printf("#\t%s [%s]\n", gn->name,
		  gn->path ? gn->path : gn->name);
    }
    return (0);
}

/*-
 *-----------------------------------------------------------------------
 * Targ_PrintGraph --
 *	print the entire graph. heh heh
 *
 * Results:
 *	none
 *
 * Side Effects:
 *	lots o' output
 *-----------------------------------------------------------------------
 */
void
Targ_PrintGraph (pass)
    int	    pass; 	/* Which pass this is. 1 => no processing
			 * 2 => processing done */
{
    printf("#*** Input graph:\n");
    Lst_ForEach (allTargets, TargPrintNode, (ClientData)pass);
    printf("\n\n");
    printf("#\n#   Files that are only sources:\n");
    Lst_ForEach (allTargets, TargPrintOnlySrc);
    printf("#*** Global Variables:\n");
    Var_Dump (VAR_GLOBAL);
    printf("#*** Command-line Variables:\n");
    Var_Dump (VAR_CMD);
    printf("\n");
    Dir_PrintDirectories();
    printf("\n");
    Suff_PrintAll();
}
