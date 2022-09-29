/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: dacl_default.c,v 65.4 1998/03/23 16:36:33 gwehrman Exp $";
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
 * $Log: dacl_default.c,v $
 * Revision 65.4  1998/03/23 16:36:33  gwehrman
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS && !KERNEL
 *
 * Revision 65.3  1997/11/06 20:00:36  jdoak
 * Source Identification added.
 *
 * Revision 65.2  1997/10/24  22:01:15  gwehrman
 * Changed include for pthread.h
 * to dce/pthread.h.
 *
 * Revision 65.1  1997/10/20  19:20:15  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.838.1  1996/10/02  18:15:34  damon
 * 	Newest DFS from Transarc
 * 	[1996/10/01  18:46:30  damon]
 *
 * $EndLog$
 */
/*
 *	eacl_default.c -- produce a new linearized ACL from the default
 *  ACL and other information
 *
 * Copyright (C) 1991, 1995 Transarc Corporation
 * All rights reserved.
 */

#include <dcedfs/param.h>
#include <dcedfs/stds.h>

#include <dacl.h>
#include <dacl_debug.h>
#include <dcedfs/fshs_deleg.h>

#if !defined(KERNEL) 
#include <dce/pthread.h>
#endif /* !KERNEL */

RCSID("$Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/security/dacl/RCS/dacl_default.c,v 65.4 1998/03/23 16:36:33 gwehrman Exp $")

/*
 * dacl_ChangeUnauthMask -- set unauth mask of a parsed ACL
 *
 * Called by Episode when creating a file on behalf of an unauthorized
 * principal.
 */

EXPORT void dacl_ChangeUnauthMask(daclP, permBits)
     dacl_t *		daclP;
     dacl_permset_t	permBits;
{
  dacl_simple_entry_t *	unauthEntryP = (dacl_simple_entry_t *)NULL;

  unauthEntryP = &daclP->simple_entries[(int)dacl_simple_entry_type_unauthmask];
  if (!unauthEntryP->is_entry_good) {
    unauthEntryP->is_entry_good = 1;
    daclP->num_entries++;
  }
  unauthEntryP->perms = permBits;
}

/* dacl_ChangeRealm -- change default realm of a parsed ACL
 * 
 * This code needs to map user and group entries into 
 * foreign_{user,group}@currentDefaultCEll respectively.
 * Also, foreign_{user,group}@newDefaultCell need to be mapped to user and 
 * group entries respectively.
 * 
 * NOTE: Removed functionality.
 * The code used to map the current other_obj entry into 
 * foreign_other@currentDefaultCell and if a foreign_other@newDefaultCell existed
 * into other_obj. 
 * WHY ? A consensus was reached to use other_obj as a template/prototype
 * ONLY like the user_obj and group_obj entries. For details refer OT 8005.
 */ 
EXPORT long dacl_ChangeRealm(daclP, ownerRealmUuidP)
     dacl_t *		daclP;
     epi_uuid_t *	ownerRealmUuidP;
{
  long			rtnVal = 0;
  epi_uuid_t		oldDefaultRealm;
  int			i, j;
  dacl_entry_t *	thisListP = (dacl_entry_t *)NULL;
  dacl_entry_t *	thisEntryP = (dacl_entry_t *)NULL;
  dacl_entry_t *	newCellsOtherEntryP = (dacl_entry_t *)NULL;
  dacl_entry_t *	newEntryArrayP = (dacl_entry_t *)NULL;
  int			oldEntryCount;
  int			newEntryCount;
  int			oldNumEntries;
  int			oldEntriesAllocated;
  dacl_permset_t	tempPermset;
  epi_uuid_t		tempUuid;
  static char		routineName[] = "dacl_ChangeRealm";
  
  if ((daclP != (dacl_t *)NULL) && (ownerRealmUuidP != (epi_uuid_t *)NULL)) {
    /* save the old default realm for reference */
    oldDefaultRealm = daclP->default_realm;
    
    /* put the new realm in place */
    daclP->default_realm = *ownerRealmUuidP;
    
    for (j = 0; j <= (int)dacl_complex_entry_type_other; j++) {
      thisListP = daclP->complex_entries[j].complex_entries;
      
      for (i = 0; i < daclP->complex_entries[j].num_entries; i++) {
	thisEntryP = &(thisListP[i]);
	if ((thisEntryP->entry_type == dacl_entry_type_user) ||
	    (thisEntryP->entry_type == dacl_entry_type_group) ||
            (thisEntryP->entry_type == dacl_entry_type_user_delegate) ||
            (thisEntryP->entry_type == dacl_entry_type_group_delegate)) {
            switch (thisEntryP->entry_type){
            case dacl_entry_type_user:
                 thisEntryP->entry_type = dacl_entry_type_foreign_user;
                 break;
            case dacl_entry_type_group:
                 thisEntryP->entry_type = dacl_entry_type_foreign_group;
                 break;
            case dacl_entry_type_user_delegate:
                 thisEntryP->entry_type = dacl_entry_type_foreign_user_delegate;
                 break;
            case dacl_entry_type_group_delegate:
                 thisEntryP->entry_type = dacl_entry_type_foreign_group_delegate;
                 break;
            
            }
	  /* this entry is no longer local */
	  tempUuid = thisEntryP->entry_info.id;
	  thisEntryP->entry_info.foreign_id.id = tempUuid;
	 
	  thisEntryP->entry_info.foreign_id.realm = oldDefaultRealm; 
	}
	else if ((thisEntryP->entry_type == dacl_entry_type_foreign_user) ||
		 (thisEntryP->entry_type == dacl_entry_type_foreign_group) ||
                 (thisEntryP->entry_type == dacl_entry_type_foreign_user_delegate) ||
                 (thisEntryP->entry_type == dacl_entry_type_foreign_group_delegate)) {
	  if (bcmp((char *)ownerRealmUuidP,
		   (char *)(&(thisEntryP->entry_info.foreign_id.realm)),
		   sizeof(epi_uuid_t)) == 0) {
	    /* if this entry is no longer foreign */
            switch (thisEntryP->entry_type){
	    case dacl_entry_type_foreign_user:
               thisEntryP->entry_type =dacl_entry_type_user;
               break;
            case dacl_entry_type_foreign_group:
               thisEntryP->entry_type = dacl_entry_type_group;
               break;
            case dacl_entry_type_foreign_user_delegate:
               thisEntryP->entry_type = dacl_entry_type_user_delegate;
               break;
            case dacl_entry_type_foreign_group_delegate:
               thisEntryP->entry_type =dacl_entry_type_group_delegate;
               break;
            }
           
	    tempUuid = thisEntryP->entry_info.foreign_id.id;
	    thisEntryP->entry_info.id = tempUuid;
	  }
	}
      }	/* end for loop through complex entry list */
    }	/* end for loop through list of complex entry lists */
  }
  else {
    if (daclP == (dacl_t *)NULL) {
      dacl_LogParamError(routineName, "daclP", dacl_debug_flag, __FILE__, __LINE__);
    }
    if (ownerRealmUuidP == (epi_uuid_t *)NULL) {
      dacl_LogParamError(routineName, "ownerRealmUuidP", dacl_debug_flag,
			 __FILE__, __LINE__);
    }

    rtnVal = DACL_ERROR_PARAMETER_ERROR;
  }
  
  return rtnVal;
}

EXPORT long dacl_NewFromDefault(defaultLinearACLP, defaultBufferLength, ownerRealmUuidP,
 				newLinearACLPP, newBufferLengthP, mode, noChangesP)
     char *		defaultLinearACLP;
     int		defaultBufferLength;
     epi_uuid_t *	ownerRealmUuidP;
     char **		newLinearACLPP;
     unsigned int *	newBufferLengthP;
     u_int32		mode;
     int *		noChangesP;
{
  long			rtnVal = DACL_SUCCESS;
  dacl_t		defaultAcl;
  u_int32		defaultModeBits;
  static char		routineName[] = "dacl_NewFromDefault";

  if ((defaultLinearACLP != (char *)NULL) &&
      (ownerRealmUuidP != (epi_uuid_t *)NULL) &&
      (newLinearACLPP != (char **)NULL) && (newBufferLengthP != (unsigned int *)NULL) &&
      (noChangesP != (int *)NULL)) {
    /* the following assumes that the manager uuid is at the start of the buffer */
    rtnVal = dacl_ParseAcl(defaultLinearACLP, defaultBufferLength, &defaultAcl,
			   (epi_uuid_t *)defaultLinearACLP /* <= here is the use of
							      the assumption */);
    if (rtnVal == DACL_SUCCESS) {
      *noChangesP = 1;		/* assume we can get away with it */

      /* if the realm is the same, we can skip the first transformation */
      if (bcmp((char *)&(defaultAcl.default_realm), (char *)ownerRealmUuidP,
			   sizeof(epi_uuid_t)) != 0) {
	rtnVal = dacl_ChangeRealm(&defaultAcl, ownerRealmUuidP);
	*noChangesP = 0;
      }
      
      if (rtnVal == DACL_SUCCESS) {
	rtnVal = dacl_ExtractPermBits(&defaultAcl, &defaultModeBits);
	if ((rtnVal == DACL_SUCCESS) && (mode != defaultModeBits)) {
	  rtnVal = dacl_ChmodAcl(&defaultAcl, mode, 0);
	  *noChangesP = 0;
	}
      }
    }
    else {
      /* errors within dacl_ParseAcl are logged by that routine */
    }
  }
  else {
    if (defaultLinearACLP != (char *)NULL) {
      dacl_LogParamError(routineName, "defaultLinearACLP", 1, __FILE__, __LINE__);
    }
    if (ownerRealmUuidP != (epi_uuid_t *)NULL) {
      dacl_LogParamError(routineName, "ownerRealUuidP", 1, __FILE__, __LINE__);
    }
    if (newLinearACLPP != (char **)NULL) {
      dacl_LogParamError(routineName, "newLinearACLPP", 1, __FILE__, __LINE__);
    }
    if (newBufferLengthP != (unsigned int *)NULL) {
      dacl_LogParamError(routineName, "newBufferLengthP", 1, __FILE__, __LINE__);
    }
    if (noChangesP != (int *)NULL) {
      dacl_LogParamError(routineName, "noChangesP", 1, __FILE__, __LINE__);
    }

    rtnVal = DACL_ERROR_PARAMETER_ERROR;
  }
  
  return rtnVal;
}

