/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: dacl_strings.c,v 65.4 1998/03/23 16:36:22 gwehrman Exp $";
static void _identFunc(const char *x) {if(!x)_identFunc(_identString);}
#endif


/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/*
 * HISTORY
 * $Log: dacl_strings.c,v $
 * Revision 65.4  1998/03/23 16:36:22  gwehrman
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS && !KERNEL
 *
 * Revision 65.3  1997/11/06 20:00:32  jdoak
 * Source Identification added.
 *
 * Revision 65.2  1997/10/24  22:01:17  gwehrman
 * Changed include for pthread.h
 * to dce/pthread.h.
 *
 * Revision 65.1  1997/10/20  19:20:18  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.452.1  1996/10/02  20:56:15  damon
 * 	Newest DFS from Transarc
 * 	[1996/10/01  18:46:54  damon]
 *
 * $EndLog$
 */
/*
 *	dacl_strings.c -- routines to translate between the internal rep and
 * strings that represent various enumerated types
 *
 * Copyright (C) 1991, 1995 Transarc Corporation
 * All rights reserved.
 */

#include <dcedfs/param.h>
#include <dcedfs/stds.h>
#include <dcedfs/icl.h>

#include <dcedfs/fshs_deleg.h>
#include <dacl.h>
#include <dacl_debug.h>
#include <dacl_trace.h>

#if !defined(KERNEL) 
#include <dce/pthread.h>
#endif /* !KERNEL */

extern struct icl_set *dacl_iclSetp;
  
RCSID("$Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/security/dacl/RCS/dacl_strings.c,v 65.4 1998/03/23 16:36:22 gwehrman Exp $")

typedef struct dacl_entry_type_strings {
  char *		entryString;
  dacl_entry_type_t	entryType;
} dacl_entry_type_strings_t;

dacl_entry_type_strings_t dacl_typeStrings[] = {
  { "user_obj",		dacl_entry_type_user_obj },
  { "group_obj",	dacl_entry_type_group_obj },
  { "other_obj",	dacl_entry_type_other_obj },
  { "mask_obj",		dacl_entry_type_mask_obj },

  { "any_other",	dacl_entry_type_anyother },

  { "unauth",		dacl_entry_type_unauth },
   
  { "user",		dacl_entry_type_user },
  { "group",		dacl_entry_type_group },
  { "foreign_other",	dacl_entry_type_foreign_other },

  { "foreign_user",	dacl_entry_type_foreign_user },
  { "foreign_group",	dacl_entry_type_foreign_group },
  { "user_obj_delegate",  dacl_entry_type_user_obj_delegate},
  { "group_obj_delegate", dacl_entry_type_user_obj_delegate},
  { "other_obj_delegate", dacl_entry_type_user_obj_delegate},
  { "anyother_delegate", dacl_entry_type_user_obj_delegate},
  { "user_delegate",     dacl_entry_type_user_delegate},
  { "group_delegate",    dacl_entry_type_group_delegate},
  { "foreign_user_delegate",  dacl_entry_type_foreign_user_delegate},
  { "foreign_group_delegate", dacl_entry_type_foreign_user_delegate},
  { "foreign_other_delegate", dacl_entry_type_foreign_other_delegate},
  { "extended",		dacl_entry_type_extended }
};

EXPORT char * dacl_EntryType_ToString(probeType)
     dacl_entry_type_t	probeType;
{
  char *	rtnVal = (char *)NULL;
  int		i;
  int		numberEntryTypeStrings = (sizeof(dacl_typeStrings) /
					  sizeof(dacl_typeStrings[0]));

  for (i = 0; (rtnVal == (char *)NULL) && (i < numberEntryTypeStrings); i++) {
    if (probeType == dacl_typeStrings[i].entryType) {
      rtnVal = dacl_typeStrings[i].entryString;
    }
  }
  
  return rtnVal;
}

EXPORT long dacl_EntryType_FromString(probeTypeP, probeString)
     dacl_entry_type_t *	probeTypeP;
     char *			probeString;
{
  long		rtnVal = DACL_ERROR_UNRECOGNIZED_ENTRY_TYPE;	/* assume failure */
  int		i;
  int		numberEntryTypeStrings = (sizeof(dacl_typeStrings) /
					  sizeof(dacl_typeStrings[0]));
  static char	routineName[] = "dacl_EntryType_FromString";
  
  dmprintf(dacl_debug_flag, DACL_DEBUG_BIT_STRINGS,
	   ("%s: entered", routineName));
  icl_Trace0(dacl_iclSetp, DACL_ICL_STRINGS_STRINGS_0 );
  
  for (i = 0; (i < numberEntryTypeStrings); i++) {
    if (strcmp(probeString, dacl_typeStrings[i].entryString) == 0) {
      *probeTypeP = dacl_typeStrings[i].entryType;
      icl_Trace2(dacl_iclSetp, DACL_ICL_STRINGS_STRINGS_1 , ICL_TYPE_LONG, *probeTypeP, ICL_TYPE_STRING, probeString);
      return (DACL_SUCCESS);
    }
  }
  
  dmprintf(dacl_debug_flag, DACL_DEBUG_BIT_STRINGS,
	   ("%s: exiting, returning %#x", routineName, rtnVal));
  icl_Trace1(dacl_iclSetp, DACL_ICL_STRINGS_STRINGS_2 , ICL_TYPE_LONG, rtnVal);

  return rtnVal;
}

typedef struct dacl_perm_printstring {
  char *		printString;
  char *		helpString;
  dacl_permset_t	matchingPermset;
} dacl_perm_printstring_t;

dacl_perm_printstring_t dacl_permStrings[] = {
  /* this is the order in which we expect them to appear in string reps */

  /* first, the standard ones */
  { "r", "read", 		dacl_perm_read },
  { "w", "write",		dacl_perm_write },
  { "x", "execute",		dacl_perm_execute },
  { "c", "control",		dacl_perm_control },
  { "i", "insert",		dacl_perm_insert },
  { "d", "delete",		dacl_perm_delete },

  /* and now, the application definable ones */
  { "A", "application-defined",	dacl_perm_user0 },
  { "B", "application-defined", dacl_perm_user1 },
  { "C", "application-defined", dacl_perm_user2 },
  { "D", "application-defined", dacl_perm_user3 },
  { "E", "application-defined", dacl_perm_user4 },
  { "F", "application-defined", dacl_perm_user5 },
  { "G", "application-defined", dacl_perm_user6 },
  { "H", "application-defined", dacl_perm_user7 }
};

EXPORT void dacl_Permset_FromString(thePermSetP, permStringP)
     dacl_permset_t *	thePermSetP;
     char *		permStringP;
{
  int		i;
  int		numberPermsetStrings = (sizeof(dacl_permStrings) /
					sizeof(dacl_permStrings[0]));
  static char	routineName[] = "dacl_ParsePermsetString";
  
  *thePermSetP = 0;
  if (permStringP != (char *)NULL) {
    while (*permStringP != '\0') {
      icl_Trace1(dacl_iclSetp, DACL_ICL_STRINGS_PERMSET_3,
		 ICL_TYPE_LONG, *permStringP);
      if (*permStringP != '-') {
	for (i = 0; i < numberPermsetStrings; i++) {
	  if (*permStringP == dacl_permStrings[i].printString[0]) {
	    icl_Trace1(dacl_iclSetp, DACL_ICL_STRINGS_PERMSET_4,
		       ICL_TYPE_LONG, *thePermSetP);
	    *thePermSetP |= dacl_permStrings[i].matchingPermset;
	    icl_Trace2(dacl_iclSetp, DACL_ICL_STRINGS_PERMSET_5,
		       ICL_TYPE_LONG, *permStringP,
		       ICL_TYPE_LONG, *thePermSetP);
	  }
	}
      }
      permStringP++;
    }
  }
}

EXPORT void dacl_Permset_ToString(thePermSetP, stringBufferP)
     dacl_permset_t *	thePermSetP;
     char *		stringBufferP;
{
  int			numberPermsetStrings = (sizeof(dacl_permStrings) /
						sizeof(dacl_permStrings[0]));
  int			i;
  dacl_permset_t	probePermset = *thePermSetP;

  stringBufferP[0] = '\0';
  for (i = 0; i < numberPermsetStrings; i++) {
    if ((probePermset & dacl_permStrings[i].matchingPermset) != 0) {
      (void)strcat(stringBufferP, dacl_permStrings[i].printString);
      probePermset &= (~dacl_permStrings[i].matchingPermset);
    }
    else {
      /* if we want to handle abbrevs, this clause should just go away */
      (void)strcat(stringBufferP, "-");
    }
  }
}
