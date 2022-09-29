/*-
 * arch.c --
 *	Functions to manipulate libraries, archives and their members.
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
 *	Once again, cacheing/hashing comes into play in the manipulation
 * of archives. The first time an archive is referenced, all of its members'
 * headers are read and hashed and the archive closed again. All hashed
 * archives are kept on a list which is searched each time an archive member
 * is referenced.
 *
 * The interface to this module is:
 *	Arch_ParseArchive   	Given an archive specification, return a list
 *	    	  	    	of GNode's, one for each member in the spec.
 *	    	  	    	FAILURE is returned if the specification is
 *	    	  	    	invalid for some reason.
 *
 *	Arch_Touch	    	Alter the modification time of the archive
 *	    	  	    	member described by the given node to be
 *	    	  	    	the current time.
 *
 *	Arch_TouchLib	    	Update the modification time of the library
 *	    	  	    	described by the given node. This is special
 *	    	  	    	because it also updates the modification time
 *	    	  	    	of the library's table of contents.
 *
 *	Arch_MTime	    	Find the modification time of a member of
 *	    	  	    	an archive *in the archive*. The time is also
 *	    	  	    	placed in the member's GNode. Returns the
 *	    	  	    	modification time.
 *
 *	Arch_MemTime	    	Find the modification time of a member of
 *	    	  	    	an archive. Called when the member doesn't
 *	    	  	    	already exist. Looks in the archive for the
 *	    	  	    	modification time. Returns the modification
 *	    	  	    	time.
 *
 *	Arch_FindLib	    	Search for a library along a path. The
 *	    	  	    	library name in the GNode should be in
 *	    	  	    	-l<name> format.
 *
 *	Arch_LibOODate	    	Special function to decide if a library node
 *	    	  	    	is out-of-date.
 *
 *	Arch_Init 	    	Initialize this module.
 */
#if !defined(lint) && defined(keep_rcsid)
static char *rcsid = "Id: arch.c,v 1.20 89/11/14 13:43:37 adam Exp $ SPRITE (Berkeley)";
#endif lint

#include    <stdio.h>
#include    <sys/types.h>
#include    <sys/stat.h>
#include    <sys/time.h>
#include    <ctype.h>
#include    <ar.h>
#include    "make.h"
#include    "hash.h"

static Lst	  archives;   /* Lst of archives we've already examined */

typedef struct Arch {
    char	  *name;      /* Name of archive */
    Hash_Table	  members;    /* All the members of the archive described
			       * by <name, struct ar_hdr *> key/value pairs */
#ifdef sgi
    char	  longnames;   /* set if archive permits long names */
#endif
} Arch;

static FILE *ArchFindMember();

/*-
 *-----------------------------------------------------------------------
 * Arch_ParseArchive --
 *	Parse the archive specification in the given line and find/create
 *	the nodes for the specified archive members, placing their nodes
 *	on the given list.
 *
 * Results:
 *	SUCCESS if it was a valid specification. The linePtr is updated
 *	to point to the first non-space after the archive spec. The
 *	nodes for the members are placed on the given list.
 *
 * Side Effects:
 *	Some nodes may be created. The given list is extended.
 *
 *-----------------------------------------------------------------------
 */
ReturnStatus
Arch_ParseArchive (linePtr, nodeLst, ctxt)
    char	    **linePtr;      /* Pointer to start of specification */
    Lst	    	    nodeLst;   	    /* Lst on which to place the nodes */
    GNode   	    *ctxt;  	    /* Context in which to expand variables */
{
    register char   *cp;	    /* Pointer into line */
    GNode	    *gn;     	    /* New node */
    char	    *libName;  	    /* Library-part of specification */
    char	    *memName;  	    /* Member-part of specification */
    char	    nameBuf[NAME_MAX*2+3]; /* temporary place for node name */
    char	    saveChar;  	    /* Ending delimiter of member-name */
    Boolean 	    subLibName;	    /* TRUE if libName should have/had
				     * variable substitution performed on it */

    libName = *linePtr;
    
    subLibName = FALSE;

    for (cp = libName; *cp != '(' && *cp != '\0'; cp++) {
	if (*cp == '$') {
	    /*
	     * Variable spec, so call the Var module to parse the puppy
	     * so we can safely advance beyond it...
	     */
	    int 	length;
	    Boolean	freeIt;
	    char	*result;
	    
	    result=Var_Parse(cp, ctxt, TRUE, &length, &freeIt);
	    if (result == var_Error) {
		return(FAILURE);
	    } else {
		subLibName = TRUE;
	    }
	    
	    if (freeIt) {
		free(result);
	    }
	    cp += length-1;
	}
    }

    *cp++ = '\0';
    if (subLibName) {
	libName = Var_Subst(libName, ctxt, TRUE);
    }
    

    while (1) {
	/*
	 * First skip to the start of the member's name, mark that
	 * place and skip to the end of it (either white-space or
	 * a close paren).
	 */
	Boolean	doSubst = FALSE; /* TRUE if need to substitute in memName */

	while (*cp != '\0' && *cp != ')' && isspace (*cp)) {
	    cp++;
	}
	memName = cp;
	while (*cp != '\0' && *cp != ')' && !isspace (*cp)) {
	    if (*cp == '$') {
		/*
		 * Variable spec, so call the Var module to parse the puppy
		 * so we can safely advance beyond it...
		 */
		int 	length;
		Boolean	freeIt;
		char	*result;

		result=Var_Parse(cp, ctxt, TRUE, &length, &freeIt);
		if (result == var_Error) {
		    return(FAILURE);
		} else {
		    doSubst = TRUE;
		}

		if (freeIt) {
		    free(result);
		}
		cp += length;
	    } else {
		cp++;
	    }
	}

	/*
	 * If the specification ends without a closing parenthesis,
	 * chances are there's something wrong (like a missing backslash),
	 * so it's better to return failure than allow such things to happen
	 */
	if (*cp == '\0') {
	    printf("No closing parenthesis in archive specification\n");
	    return (FAILURE);
	}

	/*
	 * If we didn't move anywhere, we must be done
	 */
	if (cp == memName) {
	    break;
	}

	saveChar = *cp;
	*cp = '\0';

	/*
	 * XXX: This should be taken care of intelligently by
	 * SuffExpandChildren, both for the archive and the member portions.
	 */
	/*
	 * If member contains variables, try and substitute for them.
	 * This will slow down archive specs with dynamic sources, of course,
	 * since we'll be (non-)substituting them three times, but them's
	 * the breaks -- we need to do this since SuffExpandChildren calls
	 * us, otherwise we could assume the thing would be taken care of
	 * later.
	 */
	if (doSubst) {
	    char    *buf;
	    char    *sacrifice;
	    char    *oldMemName = memName;
	    
	    memName = Var_Subst(memName, ctxt, TRUE);

	    /*
	     * Now form an archive spec and recurse to deal with nested
	     * variables and multi-word variable values.... The results
	     * are just placed at the end of the nodeLst we're returning.
	     */
	    buf=sacrifice=(char *)malloc(strlen(memName)+strlen(libName)+3);

	    sprintf(buf, "%s(%s)", libName, memName);

	    if (index(memName, '$') && strcmp(memName, oldMemName) == 0) {
		/*
		 * Must contain dynamic sources, so we can't deal with it now.
		 * Just create an ARCHV node for the thing and let
		 * SuffExpandChildren handle it...
		 */
		gn = Targ_FindNode(buf, TARG_CREATE);

		if (gn == NILGNODE) {
		    free(buf);
		    return(FAILURE);
		} else {
		    gn->type |= OP_ARCHV;
		    (void)Lst_AtEnd(nodeLst, (ClientData)gn);
		}
	    } else if (Arch_ParseArchive(&sacrifice, nodeLst, ctxt)!=SUCCESS) {
		/*
		 * Error in nested call -- free buffer and return FAILURE
		 * ourselves.
		 */
		free(buf);
		return(FAILURE);
	    }
	    /*
	     * Free buffer and continue with our work.
	     */
	    free(buf);
	} else if (Dir_HasWildcards(memName)) {
	    Lst	  members = Lst_Init(FALSE);
	    char  *member;

	    Dir_Expand(memName, dirSearchPath, members);
	    while (!Lst_IsEmpty(members)) {
		member = (char *)Lst_DeQueue(members);
		
		sprintf(nameBuf, "%s(%s)", libName, member);
		free(member);
		gn = Targ_FindNode (nameBuf, TARG_CREATE);
		if (gn == NILGNODE) {
		    return (FAILURE);
		} else {
		    /*
		     * We've found the node, but have to make sure the rest of
		     * the world knows it's an archive member, without having
		     * to constantly check for parentheses, so we type the
		     * thing with the OP_ARCHV bit before we place it on the
		     * end of the provided list.
		     */
		    gn->type |= OP_ARCHV;
		    (void) Lst_AtEnd (nodeLst, (ClientData)gn);
		}
	    }
	    Lst_Destroy(members, NOFREE);
	} else {
	    sprintf(nameBuf, "%s(%s)", libName, memName);
	    gn = Targ_FindNode (nameBuf, TARG_CREATE);
	    if (gn == NILGNODE) {
		return (FAILURE);
	    } else {
		/*
		 * We've found the node, but have to make sure the rest of the
		 * world knows it's an archive member, without having to
		 * constantly check for parentheses, so we type the thing with
		 * the OP_ARCHV bit before we place it on the end of the
		 * provided list.
		 */
		gn->type |= OP_ARCHV;
		(void) Lst_AtEnd (nodeLst, (ClientData)gn);
	    }
	}
	if (doSubst) {
	    free(memName);
	}
	
	*cp = saveChar;
    }

    /*
     * If substituted libName, free it now, since we need it no longer.
     */
    if (subLibName) {
	free(libName);
    }

    /*
     * We promised the pointer would be set up at the next non-space, so
     * we must advance cp there before setting *linePtr... (note that on
     * entrance to the loop, cp is guaranteed to point at a ')')
     */
    do {
	cp++;
    } while (*cp != '\0' && isspace (*cp));

    *linePtr = cp;
    return (SUCCESS);
}

/*-
 *-----------------------------------------------------------------------
 * ArchFindArchive --
 *	See if the given archive is the one we are looking for. Called
 *	From ArchStatMember and ArchFindMember via Lst_Find.
 *
 * Results:
 *	0 if it is, non-zero if it isn't.
 *
 * Side Effects:
 *	None.
 *
 *-----------------------------------------------------------------------
 */
static int
ArchFindArchive (ar, archName)
    Arch	  *ar;	      	  /* Current list element */
    char	  *archName;  	  /* Name we want */
{
    return (strcmp (archName, ar->name));
}

/*-
 *-----------------------------------------------------------------------
 * ArchStatMember --
 *	Locate a member of an archive, given the path of the archive and
 *	the path of the desired member.
 *
 * Results:
 *	A pointer to the current struct ar_hdr structure for the member. Note
 *	That no position is returned, so this is not useful for touching
 *	archive members. This is mostly because we have no assurances that
 *	The archive will remain constant after we read all the headers, so
 *	there's not much point in remembering the position...
 *
 * Side Effects:
 *
 *-----------------------------------------------------------------------
 */
static struct ar_hdr *
ArchStatMember (archive, member, hash)
    char	  *archive;   /* Path to the archive */
    char	  *member;    /* Name of member. If it is a path, only the
			       * last component is used. */
    Boolean	  hash;	      /* TRUE if archive should be hashed if not
    			       * already so. */
{
#define AR_MAX_NAME_LEN	    (sizeof(arh.ar_name)-1)
    FILE *	  arch;	      /* Stream to archive */
    int		  size;       /* Size of archive member */
    char	  *cp;	      /* Useful character pointer */
    char	  magic[SARMAG];
    int		  len;
    LstNode	  ln;	      /* Lst member containing archive descriptor */
    Arch	  *ar;	      /* Archive descriptor */
    Hash_Entry	  *he;	      /* Entry containing member's description */
    struct ar_hdr arh;        /* archive-member header for reading archive */
    char	  memName[AR_MAX_NAME_LEN+1];
    	    	    	    /* Current member name while hashing. The name is
			     * truncated to AR_MAX_NAME_LEN bytes, but we need
			     * room for the null byte... */
    char    	  copy[AR_MAX_NAME_LEN+1];
    	    	    	    /* Holds copy of last path element from member, if
			     * it has to be truncated, so we don't have to
			     * figure it out again once the table is hashed. */
    int stringsize = 0;	      /* size of string table */
    char *strings = NULL;     /* string table */

    /*
     * Because of space constraints and similar things, files are archived
     * using their final path components, not the entire thing, so we need
     * to point 'member' to the final component, if there is one, to make
     * the comparisons easier...
     */
    cp = rindex (member, '/');
    if (cp != (char *) NULL) {
	member = cp + 1;
    }

    /* see if already have archive */
    ln = Lst_Find (archives, (ClientData) archive, ArchFindArchive);
    if (ln != NILLNODE)
	ar = (Arch *) Lst_Datum (ln);
    else
	ar = NULL;

    /*
     * Some archives limit the size of file name others have an 'escape'
     * to permit longer names. So we only truncate the name if we know
     * this archive doesn't permit long names
     */
    len = strlen (member);
    if (len > AR_MAX_NAME_LEN && ar && ar->longnames == 0) {
	len = AR_MAX_NAME_LEN;
	strncpy(copy, member, AR_MAX_NAME_LEN);
	copy[AR_MAX_NAME_LEN] = '\0';
	member = copy;
    }

    if (ar) {
	he = Hash_FindEntry (&ar->members, (Address) member);

	if (he != (Hash_Entry *) NULL) {
	    return ((struct ar_hdr *) Hash_GetValue (he));
	} else {
	    return ((struct ar_hdr *) NULL);
	}
    }

    if (!hash) {
	/*
	 * Caller doesn't want the thing hashed, just use ArchFindMember
	 * to read the header for the member out and close down the stream
	 * again. Since the archive is not to be hashed, we assume there's
	 * no need to allocate extra room for the header we're returning,
	 * so just declare it static.
	 */
	 static struct ar_hdr	sarh;

	 arch = ArchFindMember(archive, member, &sarh, "r");

	 if (arch == (FILE *)NULL) {
	    return ((struct ar_hdr *)NULL);
	} else {
	    fclose(arch);
	    return (&sarh);
	}
    }

    /*
     * We don't have this archive on the list yet, so we want to find out
     * everything that's in it and cache it so we can get at it quickly.
     */
    arch = fopen (archive, "r");
    if (arch == (FILE *) NULL) {
	return ((struct ar_hdr *) NULL);
    }
    
    /*
     * We use the ARMAG string to make sure this is an archive we
     * can handle...
     */
    if ((fread (magic, SARMAG, 1, arch) != 1) ||
    	(strncmp (magic, ARMAG, SARMAG) != 0)) {
	    fclose (arch);
	    return ((struct ar_hdr *) NULL);
    }

    ar = (Arch *) malloc (sizeof (Arch));
    ar->name = Str_New (archive);
    ar->longnames = 0;
    Hash_InitTable (&ar->members, -1, HASH_STRING_KEYS);
    memName[AR_MAX_NAME_LEN] = '\0';

    if (DEBUG(ARCH))
	printf("caching archive %s\n", ar->name);
    
    while (fread ((char *)&arh, sizeof (struct ar_hdr), 1, arch) == 1) {
	if (strncmp ( arh.ar_fmag, ARFMAG, sizeof (arh.ar_fmag)) != 0) {
				if (DEBUG(ARCH) || DEBUG(MAKE))
				    printf("archive %s header bad or bad string table\n", ar->name);
bogusar:
				 /*
				  * The header is bogus, so the archive is bad
				  * and there's no way we can recover...
				  */
				 fclose (arch);
				 Hash_DeleteTable (&ar->members);
				 free ((Address)ar);
				 if (strings)
					free (strings);
				 return ((struct ar_hdr *) NULL);
	} else if (arh.ar_name[0] == '/' && arh.ar_name[1] == ' ') {
	    /* symbol table - ignore this */
	    goto skip;
	} else if (strncmp(arh.ar_name, "// ", 3) == 0) {
	    /* long name string table - read in the whole thing */
	    arh.ar_size[sizeof(arh.ar_size)-1] = '\0';
	    (void) sscanf (arh.ar_size, "%10d", &stringsize);
	    stringsize = (stringsize + 1) & ~1;
	    strings = malloc(stringsize);
	    if (fread(strings, stringsize, 1, arch) != 1) {
	        if (DEBUG(ARCH))
		    printf("archive %s has bad string table\n", ar->name);
	        goto bogusar;
	    }
	    continue;
	} else if (arh.ar_name[0] == '/' && isdigit(arh.ar_name[1])) {
	    /* new long name */
	    auto int offset;
	    char *endstr = strings + stringsize;

	    ar->longnames = 1;
	    (void) sscanf(&arh.ar_name[1], "%15d", &offset);
	    if (offset >= stringsize) {
		if (DEBUG(ARCH))
		    printf("archive %s bad string offset %d\n", ar->name,
			offset);
	        goto bogusar;
	    }
	    for (cp = strings + offset; *cp != '/' && cp < endstr; cp++)
		    ;
	    if (cp >= endstr || cp[1] != '\n') {
		if (DEBUG(ARCH))
		    printf("archive %s name at offset %d terminates incorrectly\n",
			ar->name, offset);
	        goto bogusar;
	    }
	    *cp = '\0';
	    if (DEBUG(ARCH))
		printf("cacheing %s\n", strings+offset);
	    he = Hash_CreateEntry (&ar->members, Str_New (strings+offset),
			       (Boolean *)NULL);
	    *cp = '/';
	} else {
	    (void) strncpy (memName, arh.ar_name, sizeof(arh.ar_name));
	    for (cp = &memName[AR_MAX_NAME_LEN]; *cp == ' ' || *cp == '/'; cp--) {
		continue;
	    }
	    /* the only real way to tell if long names are permitted -
	     * there may or may not be a string table - but 'new'
	     * archives have names ending with a '/'
	     */
	    if (cp[1] == '/')
		    ar->longnames = 1;
	    cp[1] = '\0';
	    if (DEBUG(ARCH))
		printf("cacheing %s\n", memName);
	    he = Hash_CreateEntry (&ar->members, Str_New (memName),
			       (Boolean *)NULL);
	}

	Hash_SetValue (he, (ClientData)malloc (sizeof (struct ar_hdr)));
	bcopy ((Address)&arh, (Address)Hash_GetValue (he), 
	    sizeof (struct ar_hdr));
skip:
	/*
	 * We need to advance the stream's pointer to the start of the
	 * next header. Files are padded with newlines to an even-byte
	 * boundary, so we need to extract the size of the file from the
	 * 'size' field of the header and round it up during the seek.
	 */
	arh.ar_size[sizeof(arh.ar_size)-1] = '\0';
	(void) sscanf (arh.ar_size, "%10d", &size);
	fseek (arch, (size + 1) & ~1, 1);
    }

    fclose (arch);

    (void) Lst_AtEnd (archives, (ClientData) ar);

    /*
     * Now that the archive has been read and cached, we can look into
     * the hash table to find the desired member's header.
     */
    he = Hash_FindEntry (&ar->members, (Address) member);

    if (he != (Hash_Entry *) NULL) {
	return ((struct ar_hdr *) Hash_GetValue (he));
    } else {
	return ((struct ar_hdr *) NULL);
    }
}

/*-
 *-----------------------------------------------------------------------
 * ArchFindMember --
 *	Locate a member of an archive, given the path of the archive and
 *	the path of the desired member. If the archive is to be modified,
 *	the mode should be "r+", if not, it should be "r".
 *
 * Results:
 *	An FILE *, opened for reading and writing, positioned at the
 *	start of the member's struct ar_hdr, or NULL if the member was
 *	nonexistent. The current struct ar_hdr for member.
 *
 * Side Effects:
 *	The passed struct ar_hdr structure is filled in.
 *
 *-----------------------------------------------------------------------
 */
static FILE *
ArchFindMember (archive, member, arhPtr, mode)
    char	  *archive;   /* Path to the archive */
    char	  *member;    /* Name of member. If it is a path, only the
			       * last component is used. */
    struct ar_hdr *arhPtr;    /* Pointer to header structure to be filled in */
    char	  *mode;      /* The mode for opening the stream */
{
    FILE *	  arch;	      /* Stream to archive */
    int		  size;       /* Size of archive member */
    char	  *cp;	      /* Useful character pointer */
    char	  magic[SARMAG];
    int		  len;
    int stringsize = 0;	      /* size of string table */
    char *strings = NULL;     /* string table */

    arch = fopen (archive, mode);
    if (arch == (FILE *) NULL) {
	return ((FILE *) NULL);
    }
    
    /*
     * We use the ARMAG string to make sure this is an archive we
     * can handle...
     */
    if ((fread (magic, SARMAG, 1, arch) != 1) ||
    	(strncmp (magic, ARMAG, SARMAG) != 0)) {
	    fclose (arch);
	    return ((FILE *) NULL);
    }

    /*
     * Because of space constraints and similar things, files are archived
     * using their final path components, not the entire thing, so we need
     * to point 'member' to the final component, if there is one, to make
     * the comparisons easier...
     */
    cp = rindex (member, '/');
    if (cp != (char *) NULL) {
	member = cp + 1;
    }
    len = strlen (member);
    if (len > sizeof (arhPtr->ar_name)) {
	len = sizeof (arhPtr->ar_name);
    }
    if (DEBUG(ARCH))
	printf("searching in archive %s for %s ...\n", archive, member);
    
    while (fread ((char *)arhPtr, sizeof (struct ar_hdr), 1, arch) == 1) {
	if (strncmp(arhPtr->ar_fmag, ARFMAG, sizeof (arhPtr->ar_fmag) ) != 0) {
	    if (DEBUG(ARCH) || DEBUG(MAKE))
		printf("archive %s header bad or bad string table\n", archive);
bogusar:
	     /*
	      * The header is bogus, so the archive is bad
	      * and there's no way we can recover...
	      */
	     if (strings)
		free(strings);
	     fclose (arch);
	     return ((FILE *) NULL);
	} else if (arhPtr->ar_name[0] == '/' && arhPtr->ar_name[1] == ' ') {
		;
	} else if (strncmp(arhPtr->ar_name, "// ", 3) == 0) {
	    /* long name string table - read in the whole thing */
	    arhPtr->ar_size[sizeof(arhPtr->ar_size)-1] = '\0';
	    (void) sscanf (arhPtr->ar_size, "%10d", &stringsize);
	    stringsize = (stringsize + 1) & ~1;
	    strings = malloc(stringsize);
	    if (fread(strings, stringsize, 1, arch) != 1) {
	        if (DEBUG(ARCH))
		    printf("archive %s has bad string table\n", archive);
	        goto bogusar;
	    }
	    continue;
	} else if (arhPtr->ar_name[0] == '/' && isdigit(arhPtr->ar_name[1])) {
	    /* new long name */
	    auto int offset;
	    char *endstr = strings + stringsize;

	    (void) sscanf(&arhPtr->ar_name[1], "%15d", &offset);
	    if (offset >= stringsize) {
		if (DEBUG(ARCH))
		    printf("archive %s bad string offset %d\n", archive,
			offset);
	        goto bogusar;
	    }
	    for (cp = strings + offset; *cp != '/' && cp < endstr; cp++)
		    ;
	    if (cp >= endstr || cp[1] != '\n') {
		if (DEBUG(ARCH))
		    printf("archive %s name at offset %d terminates incorrectly\n",
			archive, offset);
	        goto bogusar;
	    }
	    *cp = '\0';
	    if (strcmp(member, strings+offset) == 0) {
		*cp = '/';
		fseek (arch, -sizeof(struct ar_hdr), 1);
		if (DEBUG(ARCH))
		    printf("found it\n");
		return (arch);
	    }
	    *cp = '/';
	} else if (strncmp (member, arhPtr->ar_name, len) == 0) {
	    /*
	     * If the member's name doesn't take up the entire 'name' field,
	     * we have to be careful of matching prefixes. Names are space-
	     * padded to the right, so if the character in 'name' at the end
	     * of the matched string is anything but a space, this isn't the
	     * member we sought.
	     * New archives end names with a '/'
	     */
	    if (len != sizeof(arhPtr->ar_name) &
			(arhPtr->ar_name[len] != ' ' ||
			 arhPtr->ar_name[len] != '/')) {
		;
	    } else {
		/*
		 * To make life easier, we reposition the file at the start
		 * of the header we just read before we return the stream.
		 * In a more general situation, it might be better to leave
		 * the file at the actual member, rather than its header, but
		 * not here...
		 */
		if (DEBUG(ARCH))
		    printf("found it\n");
		fseek (arch, -sizeof(struct ar_hdr), 1);
		return (arch);
	    }
	}
	/*
	 * This isn't the member we're after, so we need to advance the
	 * stream's pointer to the start of the next header. Files are
	 * padded with newlines to an even-byte boundary, so we need to
	 * extract the size of the file from the 'size' field of the
	 * header and round it up during the seek.
	 */
	arhPtr->ar_size[sizeof(arhPtr->ar_size)-1] = '\0';
	(void)sscanf (arhPtr->ar_size, "%10d", &size);
	fseek (arch, (size + 1) & ~1, 1);
    }

    if (DEBUG(ARCH))
	printf("not found\n");
    /*
     * We've looked everywhere, but the member is not to be found. Close the
     * archive and return NULL -- an error.
     */
    fclose (arch);
    return ((FILE *) NULL);
}

/*-
 *-----------------------------------------------------------------------
 * Arch_Touch --
 *	Touch a member of an archive.
 *
 * Results:
 *	The 'time' field of the member's header is updated.
 *
 * Side Effects:
 *	The modification time of the entire archive is also changed.
 *	For a library, this could necessitate the re-ranlib'ing of the
 *	whole thing.
 *
 *-----------------------------------------------------------------------
 */
void
Arch_Touch (gn)
    GNode	  *gn;	  /* Node of member to touch */
{
    FILE *	  arch;	  /* Stream open to archive, positioned properly */
    struct ar_hdr arh;	  /* Current header describing member */

    arch = ArchFindMember(Var_Value (ARCHIVE, gn),
			  Var_Value (TARGET, gn),
			  &arh, "r+");
    sprintf(arh.ar_date, "%-12d", now);

    if (arch != (FILE *) NULL) {
	(void)fwrite ((char *)&arh, sizeof (struct ar_hdr), 1, arch);
	fclose (arch);
    }
}

/*-
 *-----------------------------------------------------------------------
 * Arch_TouchLib --
 *	Given a node which represents a library, touch the thing, making
 *	sure that the table of contents also is touched.
 * SGI: toc ignored
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	Both the modification time of the library and of the LIBTOC
 *	member are set to 'now'.
 *
 *-----------------------------------------------------------------------
 */
void
Arch_TouchLib (gn)
    GNode	    *gn;      	/* The node of the library to touch */
{
    FILE *	    arch;	/* Stream open to archive */
    struct ar_hdr   arh;      	/* Header describing table of contents */
    struct timeval  times[2];	/* Times for utimes() call */

#ifndef sgi
    if ((arch = ArchFindMember (gn->path, LIBTOC, &arh, "r+")) == (FILE *)NULL)
	arch = ArchFindMember (gn->path, LIBTOC_EL, &arh, "r+");
    sprintf(arh.ar_date, "%-12d", now);

    if (arch != (FILE *) NULL) {
	(void)fwrite ((char *)&arh, sizeof (struct ar_hdr), 1, arch);
	fclose (arch);

	times[0].tv_sec = times[1].tv_sec = now;
	times[0].tv_usec = times[1].tv_usec = 0;
	utimes(gn->path, times);
    }
#else
    /*
     * ar keeps TOC up to date and the mod time of an archive is really
     * uninteresting (see OODate)
     */
#endif
}

/*-
 *-----------------------------------------------------------------------
 * Arch_MTime --
 *	Return the modification time of a member of an archive.
 *
 * Results:
 *	The modification time (seconds).
 *
 * Side Effects:
 *	The mtime field of the given node is filled in with the value
 *	returned by the function.
 *
 *-----------------------------------------------------------------------
 */
int
Arch_MTime (gn)
    GNode	  *gn;	      /* Node describing archive member */
{
    struct ar_hdr *arhPtr;    /* Header of desired member */
    int		  modTime;    /* Modification time as an integer */

    arhPtr = ArchStatMember (Var_Value (ARCHIVE, gn),
			     Var_Value (TARGET, gn),
			     TRUE);
    if (arhPtr != (struct ar_hdr *) NULL) {
	(void)sscanf (arhPtr->ar_date, "%12d", &modTime);
    } else {
	modTime = 0;
    }

    gn->mtime = modTime;
    return (modTime);
}

/*-
 *-----------------------------------------------------------------------
 * Arch_MemMTime --
 *	Given a non-existent archive member's node, get its modification
 *	time from its archived form, if it exists.
 *
 * Results:
 *	The modification time.
 *
 * Side Effects:
 *	The mtime field is filled in.
 *
 *-----------------------------------------------------------------------
 */
int
Arch_MemMTime (gn)
    GNode   	  *gn;
{
    LstNode 	  ln;
    GNode   	  *pgn;
    char    	  *nameStart,
		  *nameEnd;

    if (Lst_Open (gn->parents) != SUCCESS) {
	gn->mtime = 0;
	return (0);
    }
    while ((ln = Lst_Next (gn->parents)) != NILLNODE) {
	pgn = (GNode *) Lst_Datum (ln);

	if (pgn->type & OP_ARCHV) {
	    /*
	     * If the parent is an archive specification and is being made
	     * and its member's name matches the name of the node we were
	     * given, record the modification time of the parent in the
	     * child. We keep searching its parents in case some other
	     * parent requires this child to exist...
	     */
	    nameStart = index (pgn->name, '(') + 1;
	    nameEnd = index (nameStart, ')');

	    if (pgn->make &&
		strncmp(nameStart, gn->name, nameEnd - nameStart) == 0) {
				     gn->mtime = Arch_MTime(pgn);
	    }
	} else if (pgn->make) {
	    /*
	     * Something which isn't a library depends on the existence of
	     * this target, so it needs to exist.
	     */
	    gn->mtime = 0;
	    break;
	}
    }

    Lst_Close (gn->parents);

    return (gn->mtime);
}

/*-
 *-----------------------------------------------------------------------
 * Arch_FindLib --
 *	Search for a library along the given search path. 
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	The node's 'path' field is set to the found path (including the
 *	actual file name, not -l...). If the system can handle the -L
 *	flag when linking (or we cannot find the library), we assume that
 *	the user has placed the .LIBRARIES variable in the final linking
 *	command (or the linker will know where to find it) and set the
 *	TARGET variable for this node to be the node's name. Otherwise,
 *	we set the TARGET variable to be the full path of the library,
 *	as returned by Dir_FindFile.
 *
 *-----------------------------------------------------------------------
 */
void
Arch_FindLib (gn, path)
    GNode	    *gn;	      /* Node of library to find */
    Lst	    	    path;	      /* Search path */
{
    char	    *libName;   /* file name for archive */

    libName = (char *)malloc (strlen (gn->name) + 6 - 2);
    sprintf(libName, "lib%s.a", &gn->name[2]);

    gn->path = Dir_FindFile (libName, path);

    free (libName);

#ifdef LIBRARIES
    Var_Set (TARGET, gn->name, gn);
#else
    Var_Set (TARGET, gn->path == (char *) NULL ? gn->name : gn->path, gn);
#endif LIBRARIES
}

/*-
 *-----------------------------------------------------------------------
 * Arch_LibOODate --
 *	Decide if a node with the OP_LIB attribute is out-of-date. Called
 *	from Make_OODate to make its life easier.
 *
 *	There are several ways for a library to be out-of-date that are
 *	not available to ordinary files. In addition, there are ways
 *	that are open to regular files that are not available to
 *	libraries. A library that is only used as a source is never
 *	considered out-of-date by itself. This does not preclude the
 *	library's modification time from making its parent be out-of-date.
 *	A library will be considered out-of-date for any of these reasons,
 *	given that it is a target on a dependency line somewhere:
 *	    Its modification time is less than that of one of its
 *	    	  sources (gn->mtime < gn->cmtime).
 *	    Its modification time is greater than the time at which the
 *	    	  make began (i.e. it's been modified in the course
 *	    	  of the make, probably by archiving).
#ifndef sgi
 *	    Its modification time doesn't agree with the modification
 *	    	  time of its LIBTOC member (i.e. its table of contents
 *	    	  is out-of-date). The sex of the archive might be different.
#endif
 *
 *
 * Results:
 *	TRUE if the library is out-of-date. FALSE otherwise.
 *
 * SGI: - no its not!
 * Side Effects:
 *	The library will be hashed if it hasn't been already.
 *
 *-----------------------------------------------------------------------
 */
Boolean
Arch_LibOODate (gn)
    GNode   	  *gn;  	/* The library's graph node */
{
    Boolean 	  oodate;
    int		  endiantype = 0;
    
#ifdef sgi
    /* XXX whats the difference between checking cmtime == 0 and
     * Lst_IsEmpty(gn->childreb)
     * Follow sysVmake semantics that don't treat libs much different
     * than files - in particular rhs side only still should be dealt with
     */
    if (sysVmake && Lst_IsEmpty(gn->children) &&
		((gn->mtime==0) || (gn->type & OP_DOUBLEDEP))) {
	if (DEBUG(MAKE)) {
	    if (gn->mtime == 0) {
		printf("non-existent and no sources...");
	    } else {
		printf(":: operator and no sources...");
	    }
	}
	oodate = TRUE;
    } else if (gn->type & OP_FORCE) {
	if (DEBUG(MAKE))
		printf("! operator...");
	oodate = TRUE;
    } else
#endif
    if (OP_NOP(gn->type) && Lst_IsEmpty(gn->children)) {
	oodate = FALSE;
    } else if ((gn->mtime > now) || (gn->mtime < gn->cmtime)) {
#ifdef sgi
	if (DEBUG(MAKE))
		printf("modified before source...");
#endif
	oodate = TRUE;
#ifdef sgi
    /*
     * The mod time of an archive is really fairly uninteresting.
     * The real time to re-gen an archive is if any of contents was
     * updated. Since the mod time of an archive gets updated whenever ANY
     * member gets inserted/updated, multi-level source trees going to
     * the same archive can easily get mixed up. So an additional test
     * is whether any child of an archive was updated and if so we
     * update the archive
     */
    } else if (gn->childMade) {
#ifdef sgi
	if (DEBUG(MAKE))
		printf("some child made...");
#endif
	oodate = TRUE;
#endif
    } else {
#ifdef sgi
	/* 
	 * Don't need to check TOC modified time because ar keeps the
	 * TOC up-to-date.  (Can't use TOC modified time to determine 
	 * if the library is out-of-date because ar updates the library 
	 * after the TOC.
	 */
	oodate = FALSE;
#else
	struct ar_hdr  	*arhPtr;    /* Header for __.SYMDEF */
	int 	  	modTimeTOC; /* The table-of-contents's mod time */

	if ((arhPtr = ArchStatMember (gn->path, LIBTOC, FALSE)) == (struct ar_hdr *)NULL) {
		arhPtr = ArchStatMember (gn->path, LIBTOC_EL, FALSE);
		endiantype++;
	}

	if (arhPtr != (struct ar_hdr *)NULL) {
	    (void)sscanf (arhPtr->ar_date, "%12d", &modTimeTOC);

	    if (DEBUG(ARCH) || DEBUG(MAKE)) {
		printf("%s modified %s...", endiantype ? LIBTOC_EL : LIBTOC,
			Targ_FmtTime(modTimeTOC));
	    }
	    oodate = (gn->mtime > modTimeTOC);
	} else {
	    /*
	     * A library w/o a table of contents is out-of-date
	     */
	    if (DEBUG(ARCH) || DEBUG(MAKE)) {
		printf("No t.o.c....");
	    }
	    oodate = TRUE;
	}
#endif
    }
    return (oodate);
}

/*-
 *-----------------------------------------------------------------------
 * Arch_Init --
 *	Initialize things for this module.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	The 'archives' list is initialized.
 *
 *-----------------------------------------------------------------------
 */
void
Arch_Init ()
{
    archives = Lst_Init (FALSE);
}
