/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: dacl_access.c,v 65.6 1998/04/01 14:16:12 gwehrman Exp $";
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
 * $Log: dacl_access.c,v $
 * Revision 65.6  1998/04/01 14:16:12  gwehrman
 * 	Cleanup of errors turned on by -diag_error flags in kernel cflags.
 * 	Added prototype for dacl_AddPermBitsToPermsets().  Changed function
 * 	definitons to ansi style.
 *
 * Revision 65.5  1998/03/23 16:36:16  gwehrman
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS && !KERNEL
 *
 * Revision 65.4  1997/12/16 17:05:41  lmc
 *  1.2.2 initial merge of file directory
 *
 * Revision 65.3  1997/11/06  20:00:31  jdoak
 * Source Identification added.
 *
 * Revision 65.2  1997/10/24  22:01:15  gwehrman
 * Changed include for pthread.h
 * to dce/pthread.h.
 *
 * Revision 65.1  1997/10/20  19:20:15  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.636.1  1996/10/02  18:15:13  damon
 * 	Newest DFS from Transarc
 * 	[1996/10/01  18:46:25  damon]
 *
 * $EndLog$
 */
/*
*/
/*
 *	dacl_access.c -- the routines to do the access checking against a PAC
 *
 * Copyright (C) 1994, 1991 Transarc Corporation
 * All rights reserved.
 */

#include <dcedfs/param.h>
#include <dcedfs/stds.h>
#include <dcedfs/icl.h>
#include <dacl.h>
#include <dacl_debug.h>
#include <dacl_trace.h>

#if !defined(KERNEL) 
#include <dce/pthread.h>
#endif /* !KERNEL */

extern struct icl_set *dacl_iclSetp;
  
#if defined(DACL_LOCAL_DEBUG) && !defined(KERNEL)
#include <dce/uuid.h>
#endif /* defined(DACL_LOCAL_DEBUG) && !defined(KERNEL) */

RCSID("$Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/security/dacl/RCS/dacl_access.c,v 65.6 1998/04/01 14:16:12 gwehrman Exp $")

#ifdef SGIMIPS
extern long dacl_AddPermBitsToPermsets(
     u_int32            permBits,
     dacl_permset_t *   userObjPermsetP,
     dacl_permset_t *   groupObjPermsetP,
     dacl_permset_t *   otherObjPermsetP);
#endif


/* we only need one copy of this routine */
/* VRK - SGIMIPS
#if defined(DACL_EPISODE) || defined(DACL2ACL) */
#ifdef SGIMIPS
EXPORT long dacl_IsSuperUser(
     epi_uuid_t *	userIdP)
#else
EXPORT long dacl_IsSuperUser(userIdP)
     epi_uuid_t *	userIdP;
#endif
{
  epi_uuid_t	zeroes;
  
  bzero((char *)&zeroes, sizeof(epi_uuid_t));
  return Epi_PrinId_Cmp(userIdP, &zeroes);
}
/*
#endif */ /* defined(DACL_EPISODE) || defined(DACL2ACL) */

#ifdef SGIMIPS
PRIVATE int dacl_SuperUserCheck(
     dacl_permset_t *	accessRequestedP,
     dacl_permset_t *	accessGrantedP)
#else
PRIVATE int dacl_SuperUserCheck(accessRequestedP, accessGrantedP)
     dacl_permset_t *	accessRequestedP;
     dacl_permset_t *	accessGrantedP;
#endif
{
  /* eventually, we probably need more smarts in here */
  /* for now, though, we ignore what's requested and grant everything */
  *accessGrantedP = (dacl_perm_read | dacl_perm_write | dacl_perm_execute |
		     dacl_perm_control | dacl_perm_insert | dacl_perm_delete);

  return 0;
}

#define DACL_ACCESS_MASK_WRITE_RIGHTS(permSet)	\
  (permSet) &= (((permSet) & dacl_perm_write) ? \
		~0L : ~(dacl_perm_insert | dacl_perm_delete))

/* the following macros will only work in the access check routine */ 
#define DACL_ACCESS_MASK_ACCESS(permSet, useMaskObj)			\
MACRO_BEGIN								\
  (permSet) &= ((useMaskObj) ? maskObjPermMask : ~0L);		\
  DACL_ACCESS_MASK_WRITE_RIGHTS(permSet);				\
MACRO_END

/* this is what the masking macro used to to - keep the code around for the moment
  if ((entryP)->entry_class != dacl_entry_class_none) {			\
    if ((entryP)->entry_class == dacl_entry_class_owner) {		\
      (permSet) &= ownerClassPermMask;					\
    }									\
    else if ((entryP)->entry_class == dacl_entry_class_group) {		\
      (permSet) &= groupClassPermMask;					\
    }									\
    else if ((entryP)->entry_class == dacl_entry_class_other) {		\
      (permSet) &= otherClassPermMask;					\
    }									\
    else {								\
      osi_printf("%s: Error: illegal entry class in acl entry: %#x\n",	\
		 routineName, (entryP)->entry_class);			\
    }									\
  }									\
*/

#define DACL_ACCESS_ACCUMULATE_GROUP_PERMS	\
      ((groupObjEntryP == (dacl_simple_entry_t *)NULL) || (maskObjPresent == 1))

#if defined(DACL_EPISODE)
/* Episode-only version of the parameter checking routine (avoids the uuid check) */
#ifdef SGIMIPS
PRIVATE long dacl_epi_CheckAccessParams(
     dacl_t *			aclP,
     u_int32 *			permBitsP,
     dacl_permset_t *		accessRequestedP,
     epi_principal_id_t *	userObjP,
     epi_principal_id_t *	groupObjP)
#else
PRIVATE long dacl_epi_CheckAccessParams(aclP, permBitsP, accessRequestedP,
					userObjP, groupObjP)
     dacl_t *			aclP;
     u_int32 *			permBitsP;
     dacl_permset_t *		accessRequestedP;
     epi_principal_id_t *	userObjP;
     epi_principal_id_t *	groupObjP;
#endif
{
  long		rtnVal = 0;
  static char	routineName[] = "dacl_epi_CheckAccessParams";
  
  /* first, check the params that must be there no matter what type the manager is */
  if ((aclP != (dacl_t *)NULL) && (accessRequestedP != (dacl_permset_t *)NULL)) {
    /* the parameters whose presence depends on manager type */
    if ((userObjP == (epi_principal_id_t *)NULL) ||
	(groupObjP == (epi_principal_id_t *)NULL)) {
      icl_Trace1(dacl_iclSetp, DACL_ICL_ACCESS_NONE_0 , ICL_TYPE_STRING, dacl_AclMgrName(&(aclP->mgr_type_tag)));
      rtnVal = DACL_ERROR_MGR_PARAMETER_ERROR;
    }
    if (permBitsP == (u_int32 *)NULL) {
      icl_Trace1(dacl_iclSetp, DACL_ICL_ACCESS_NONE_1 , ICL_TYPE_STRING, dacl_AclMgrName(&(aclP->mgr_type_tag)));
      rtnVal = DACL_ERROR_MGR_PARAMETER_ERROR;
    }
  }
  else {
    if (aclP == (dacl_t *)NULL) {
      dacl_LogParamError(routineName, "aclP", dacl_debug_flag, __FILE__, __LINE__);
    }
    if (accessRequestedP == (dacl_permset_t *)NULL) {
      dacl_LogParamError(routineName, "accessRequestedP", dacl_debug_flag,
			 __FILE__, __LINE__);
    }
    rtnVal = DACL_ERROR_PARAMETER_ERROR;
  }

  return rtnVal;
}
#else /* defined(DACL_EPISODE) */
#ifdef SGIMIPS
PRIVATE long dacl_CheckAccessParams(
     dacl_t *			aclP,
     u_int32 *			permBitsP,
     dacl_permset_t *		accessRequestedP,
     epi_principal_id_t *	userObjP,
     epi_principal_id_t *	groupObjP)
#else
PRIVATE long dacl_CheckAccessParams(aclP, permBitsP, accessRequestedP, userObjP, groupObjP)
     dacl_t *			aclP;
     u_int32 *			permBitsP;
     dacl_permset_t *		accessRequestedP;
     epi_principal_id_t *	userObjP;
     epi_principal_id_t *	groupObjP;
#endif
{
  long		rtnVal = 0;
  static char	routineName[] = "dacl_CheckAccessParams";
  
  /* first, check the params that must be there no matter what type the manager is */
  if ((aclP != (dacl_t *)NULL) && (accessRequestedP != (dacl_permset_t *)NULL)) {
    /* the parameters whose presence depends on manager type */
    if ((dacl_AreObjectUuidsRequiredOnAccessCheck(&(aclP->mgr_type_tag)) == 1) &&
	((userObjP == (epi_principal_id_t *)NULL) ||
	 (groupObjP == (epi_principal_id_t *)NULL))) {
      icl_Trace1(dacl_iclSetp, DACL_ICL_ACCESS_NONE_2 , ICL_TYPE_STRING, dacl_AclMgrName(&(aclP->mgr_type_tag)));
      rtnVal = DACL_ERROR_MGR_PARAMETER_ERROR;
    }
    if ((dacl_ArePermBitsRequiredOnAccessCheck(&(aclP->mgr_type_tag)) == 1) &&
	(permBitsP == (u_int32 *)NULL)) {
      icl_Trace1(dacl_iclSetp, DACL_ICL_ACCESS_NONE_3 , ICL_TYPE_STRING, dacl_AclMgrName(&(aclP->mgr_type_tag)));
      rtnVal = DACL_ERROR_MGR_PARAMETER_ERROR;
    }
  }
  else {
    if (aclP == (dacl_t *)NULL) {
      dacl_LogParamError(routineName, "aclP", dacl_debug_flag, __FILE__, __LINE__);
    }
    if (accessRequestedP == (dacl_permset_t *)NULL) {
      dacl_LogParamError(routineName, "accessRequestedP", dacl_debug_flag,
			 __FILE__, __LINE__);
    }
    rtnVal = DACL_ERROR_PARAMETER_ERROR;
  }

  return rtnVal;
}
#endif /* defined(DACL_EPISODE) */

#ifdef SGIMIPS
PRIVATE dacl_entry_t * dacl_FindMatchingListEntry(
     epi_uuid_t *		userIdP,
     epi_uuid_t *		groupIdP,
     epi_uuid_t *		realmIdP,
     dacl_entry_t *		entryListP,
     unsigned int		numEntries)
#else
PRIVATE dacl_entry_t * dacl_FindMatchingListEntry(userIdP, groupIdP, realmIdP,
						  entryListP, numEntries)
     epi_uuid_t *		userIdP;
     epi_uuid_t *		groupIdP;
     epi_uuid_t *		realmIdP;
     dacl_entry_t *		entryListP;
     unsigned int		numEntries;
#endif
{
  dacl_entry_t *	rtnVal = (dacl_entry_t *)NULL;
  int			i;
  dacl_entry_t *	thisEntryP;

#if  defined(DACL_LOCAL_DEBUG) && !defined(KERNEL)
  unsigned_char_p_t	uuidString;
  unsigned_char_p_t	uuidStringAux;
  unsigned32		uuidStatus;
#endif  /* defined(DACL_LOCAL_DEBUG) && !defined(KERNEL) */
  static char		routineName[] = "dacl_FindMatchingListEntry";

  icl_Trace0(dacl_iclSetp, DACL_ICL_ACCESS_ACCESS_0);

#if defined(DACL_LOCAL_DEBUG) && !defined(KERNEL)
  if (dacl_debug_flag & DACL_DEBUG_BIT_ACCESS) {
    if (userIdP != (epi_uuid_t *)NULL) {
      uuid_to_string((uuid_t *)userIdP, &uuidString, &uuidStatus);
      if (uuidStatus == error_status_ok) {
	icl_Trace1(dacl_iclSetp, DACL_ICL_ACCESS_ACCESS_1 , ICL_TYPE_STRING, uuidString);
      }
      else {
	icl_Trace0(dacl_iclSetp, DACL_ICL_ACCESS_ACCESS_2);
      }
    }
    
    if (groupIdP != (epi_uuid_t *)NULL) {
      uuid_to_string((uuid_t *)groupIdP, &uuidString, &uuidStatus);
      if (uuidStatus == error_status_ok) {
	icl_Trace1(dacl_iclSetp, DACL_ICL_ACCESS_ACCESS_3 , ICL_TYPE_STRING, uuidString);
      }
      else {
	icl_Trace0(dacl_iclSetp, DACL_ICL_ACCESS_ACCESS_4);
      }
    }
    
    if (realmIdP != (epi_uuid_t *)NULL) {
      uuid_to_string((uuid_t *)realmIdP, &uuidString, &uuidStatus);
      if (uuidStatus == error_status_ok) {
	icl_Trace1(dacl_iclSetp, DACL_ICL_ACCESS_ACCESS_5 , ICL_TYPE_STRING, uuidString);
      }
      else {
	icl_Trace0(dacl_iclSetp, DACL_ICL_ACCESS_ACCESS_6);
      }
    }
  }
#endif /* defined(DACL_LOCAL_DEBUG) && !defined(KERNEL) */
  
  if (entryListP != (dacl_entry_t *)NULL) {
    for (i = 0; (rtnVal == (dacl_entry_t *)NULL) && (i < numEntries); i++) {
      thisEntryP = &(entryListP[i]);
	
      /* the single key entry types: */ 	
      if (thisEntryP->entry_type == dacl_entry_type_user
#if defined(DACL_EPISODE)
		|| thisEntryP->entry_type == dacl_entry_type_user_delegate
#endif /* defined(DACL_EPISODE) */
	  ) { 	 
#if defined(DACL_LOCAL_DEBUG) && !defined(KERNEL)
	if (dacl_debug_flag & DACL_DEBUG_BIT_ACCESS) {
	  uuid_to_string((uuid_t *)(&(thisEntryP->entry_info.id)),
			 &uuidString, &uuidStatus);
	  if (uuidStatus == error_status_ok) {
	    icl_Trace1(dacl_iclSetp, DACL_ICL_ACCESS_ACCESS_7 , ICL_TYPE_STRING, uuidString);
	  }
	  else {
	    icl_Trace0(dacl_iclSetp, DACL_ICL_ACCESS_ACCESS_8);
	  }
	}
#endif /* defined(DACL_LOCAL_DEBUG) && !defined(KERNEL)	*/

	if ((userIdP != (epi_uuid_t *)NULL) &&
	    (Epi_PrinId_Cmp(userIdP, &(thisEntryP->entry_info.id)) == 0) && 	 
	    (realmIdP == (epi_uuid_t *)NULL)) { 	 
	  rtnVal = thisEntryP;
	  icl_Trace0(dacl_iclSetp, DACL_ICL_ACCESS_ACCESS_9);
	} 	
      } 	
      else if (thisEntryP->entry_type == dacl_entry_type_group
#if defined(DACL_EPISODE)
		|| thisEntryP->entry_type == dacl_entry_type_group_delegate
#endif /* defined(DACL_EPISODE) */
	       ) { 	 
#if defined(DACL_LOCAL_DEBUG) && !defined(KERNEL)
	if (dacl_debug_flag & DACL_DEBUG_BIT_ACCESS) {
	  uuid_to_string((uuid_t *)(&(thisEntryP->entry_info.id)),
			 &uuidString, &uuidStatus);
	  if (uuidStatus == error_status_ok) {
	    icl_Trace1(dacl_iclSetp, DACL_ICL_ACCESS_ACCESS_10 , ICL_TYPE_STRING, uuidString);
	  }
	  else {
	    icl_Trace0(dacl_iclSetp, DACL_ICL_ACCESS_ACCESS_11);
	  }
	}
#endif /* defined(DACL_LOCAL_DEBUG) && !defined(KERNEL) */
	
	if ((groupIdP != (epi_uuid_t *)NULL) && 	 
	    (Epi_PrinId_Cmp(groupIdP, &(thisEntryP->entry_info.id)) == 0) && 	 
	    (realmIdP == (epi_uuid_t *)NULL)) { 	 
	  rtnVal = thisEntryP;
	  icl_Trace0(dacl_iclSetp, DACL_ICL_ACCESS_ACCESS_12);
	} 	
      } 	
      else if (thisEntryP->entry_type == dacl_entry_type_foreign_other
#if defined(DACL_EPISODE)
	|| thisEntryP->entry_type == dacl_entry_type_foreign_other_delegate
#endif /* defined(DACL_EPISODE) */
	       ) {
#if defined(DACL_LOCAL_DEBUG) && !defined(KERNEL)
	if (dacl_debug_flag & DACL_DEBUG_BIT_ACCESS) {
	  uuid_to_string((uuid_t *)(&(thisEntryP->entry_info.id)),
			 &uuidString, &uuidStatus);
	  if (uuidStatus == error_status_ok) {
	    icl_Trace1(dacl_iclSetp, DACL_ICL_ACCESS_ACCESS_13 , ICL_TYPE_STRING, uuidString);
	  }
	  else {
	    icl_Trace0(dacl_iclSetp, DACL_ICL_ACCESS_ACCESS_14);
	  }
	}
#endif /* defined(DACL_LOCAL_DEBUG) && !defined(KERNEL) */
	
	if ((realmIdP != (epi_uuid_t *)NULL) && 	 
	    (Epi_PrinId_Cmp(realmIdP, &(thisEntryP->entry_info.id)) == 0)) {
	  rtnVal = thisEntryP;
	  icl_Trace0(dacl_iclSetp, DACL_ICL_ACCESS_ACCESS_15);
	}
      } 	
      /* the two key entry types: */
      else if (thisEntryP->entry_type == dacl_entry_type_foreign_user
#if defined(DACL_EPISODE)
	|| thisEntryP->entry_type == dacl_entry_type_foreign_user_delegate
#endif /* defined(DACL_EPISODE) */
	       ) {
#if defined(DACL_LOCAL_DEBUG) && !defined(KERNEL)
	if (dacl_debug_flag & DACL_DEBUG_BIT_ACCESS) {
	  uuid_to_string((uuid_t *)(&(thisEntryP->entry_info.foreign_id.id)),
			 &uuidString, &uuidStatus);
	  if (uuidStatus == error_status_ok) {
	    uuid_to_string((uuid_t *)(&(thisEntryP->entry_info.foreign_id.realm)),
			   &uuidStringAux, &uuidStatus);

	    if (uuidStatus == error_status_ok) {
	      icl_Trace2(dacl_iclSetp, DACL_ICL_ACCESS_ACCESS_16 , ICL_TYPE_STRING, uuidString, ICL_TYPE_STRING, uuidStringAux);
	    }
	    else {
	      icl_Trace0(dacl_iclSetp, DACL_ICL_ACCESS_ACCESS_17);
	    }
	    
	  }
	  else {
	    icl_Trace0(dacl_iclSetp, DACL_ICL_ACCESS_ACCESS_18);
	  }
	}
#endif /* defined(DACL_LOCAL_DEBUG) && !defined(KERNEL) */
	
	if ((userIdP != (epi_uuid_t *)NULL) &&
	    (realmIdP != (epi_uuid_t *)NULL) &&
	    (Epi_PrinId_Cmp(userIdP, &(thisEntryP->entry_info.foreign_id.id)) == 0) &&
	    (bcmp((char *)realmIdP, (char *)&(thisEntryP->entry_info.foreign_id.realm),
		  sizeof(epi_uuid_t)) == 0)) {
	  rtnVal = thisEntryP;
	  icl_Trace0(dacl_iclSetp, DACL_ICL_ACCESS_ACCESS_19);
	}
      }
      else if (thisEntryP->entry_type == dacl_entry_type_foreign_group
#if defined(DACL_EPISODE)
	|| thisEntryP->entry_type == dacl_entry_type_foreign_group_delegate
#endif /* defined(DACL_EPISODE) */
	       ) {
#if defined(DACL_LOCAL_DEBUG) && !defined(KERNEL)
	if (dacl_debug_flag & DACL_DEBUG_BIT_ACCESS) {
	  uuid_to_string((uuid_t *)(&(thisEntryP->entry_info.foreign_id.id)),
			 &uuidString, &uuidStatus);
	  if (uuidStatus == error_status_ok) {
	    uuid_to_string((uuid_t *)(&(thisEntryP->entry_info.foreign_id.realm)),
			   &uuidStringAux, &uuidStatus);

	    if (uuidStatus == error_status_ok) {
	      icl_Trace2(dacl_iclSetp, DACL_ICL_ACCESS_ACCESS_20 , ICL_TYPE_STRING, uuidString, ICL_TYPE_STRING, uuidStringAux);
	    }
	    else {
	      icl_Trace0(dacl_iclSetp, DACL_ICL_ACCESS_ACCESS_21);
	    }
	    
	  }
	  else {
	    icl_Trace0(dacl_iclSetp, DACL_ICL_ACCESS_ACCESS_22);
	  }
	}
#endif /* defined(DACL_LOCAL_DEBUG) && !defined(KERNEL) */
	
	if ((groupIdP != (epi_uuid_t *)NULL) &&
	    (realmIdP != (epi_uuid_t *)NULL) &&
	    (Epi_PrinId_Cmp(groupIdP, &(thisEntryP->entry_info.foreign_id.id)) == 0) &&
	    (bcmp((char *)realmIdP, (char *)&(thisEntryP->entry_info.foreign_id.realm),
		  sizeof(epi_uuid_t)) == 0)) {
	  rtnVal = thisEntryP;
	  icl_Trace0(dacl_iclSetp, DACL_ICL_ACCESS_ACCESS_23);
	}
      }
      else if (thisEntryP->entry_type != dacl_entry_type_extended) {
	/* unrecognized acl type */
	/* this should be some sort of error, but how do we flag it? */
      }	/* end if/else checking entry type */
    }
  }
  else {
    dacl_LogParamError(routineName, "entryListP", dacl_debug_flag, __FILE__, __LINE__);
  }

  return rtnVal;
}


#if defined(DACL_EPISODE)
EXPORT long dacl_epi_DetermineAccessAllowed(
     dacl_t *			aclP,
     u_int32 *			permBitsP,
     dacl_permset_t *		accessRequestedP,
     epi_uuid_t *		realmIdP,
     epi_uuid_t *		userIdP,
     epi_uuid_t *		groupIdP,
     epi_sec_id_t *		groupIdListP,
     unsigned int		numGroups,
     sec_id_foreign_groupset_t 	*foreignGroupSetP,
     unsigned16 		numForeignGrpSets,
     epi_principal_id_t *	userObjP,
     epi_principal_id_t *	groupObjP,
     int			isAuthn,
     int			do_dlg,
     dacl_permset_t *		accessAllowedP)
#else /* defined(DACL_EPISODE) */
EXPORT long dacl_DetermineAccessAllowed(
     dacl_t *			aclP,
     u_int32 *			permBitsP,
     dacl_permset_t *		accessRequestedP,
     epi_uuid_t *		realmIdP,
     epi_uuid_t *		userIdP,
     epi_uuid_t *		groupIdP,
     epi_sec_id_t *		groupIdListP,
     unsigned int		numGroups,
     epi_sec_id_foreign_t *	foreignGroupIdListP,
     unsigned int		numForeignGroups,
     epi_principal_id_t *	userObjP,
     epi_principal_id_t *	groupObjP,
     int			isAuthn,
     dacl_permset_t *		accessAllowedP)
#endif /* defined(DACL_EPISODE) */
{
    /* assume access denied */
    long rtnVal = DACL_ERROR_ACCESS_DENIED;	
    long auxRtnVal = 0;
    epi_uuid_t zeroUuid;


    /* 
     * do_dlg controls whether the access check algorithm checks for delegates.
     *
     * Ignore delegation for admin lists 
     */
#if !defined(DACL_EPISODE)
    int do_dlg = 0;  		
#endif

    /* in case, no perm masks found, don't restrict access */
    dacl_permset_t maskObjPermMask = (dacl_permset_t)~0L;
    dacl_permset_t unauthPermMask = 0L;
    
    dacl_permset_t accessGranted = 0L;	/* assume access denied */
    
    /* we save these for use in comparison in the following order */
    dacl_simple_entry_t *userObjEntryP = (dacl_simple_entry_t *)NULL;
    dacl_entry_t *userEntryP = (dacl_entry_t *)NULL;
    dacl_entry_t *userDelegateEntryP = (dacl_entry_t *)NULL;

    dacl_simple_entry_t *groupObjEntryP = (dacl_simple_entry_t *)NULL;
    dacl_simple_entry_t *groupObjDelegateEntryP = (dacl_simple_entry_t *)NULL;
    dacl_entry_t *groupEntryP = (dacl_entry_t *)NULL;
    dacl_entry_t *groupDelegateEntryP = (dacl_entry_t *)NULL;

    dacl_simple_entry_t *otherObjEntryP = (dacl_simple_entry_t *)NULL;
    dacl_simple_entry_t *otherObjDelegateEntryP = (dacl_simple_entry_t *)NULL;

    dacl_simple_entry_t *anyOtherEntryP = (dacl_simple_entry_t *)NULL;
    dacl_simple_entry_t *anyOtherDelegateEntryP = (dacl_simple_entry_t *)NULL;

    dacl_entry_t *otherEntryP = (dacl_entry_t *)NULL;
    dacl_entry_t *otherDelegateEntryP = (dacl_entry_t *)NULL;

    dacl_simple_entry_t *unauthEntryP = (dacl_simple_entry_t *)NULL;

    int	userKeyMatched = 0;
    int	groupKeyMatched = 0;
    int	groupDelegateKeyMatched = 0;
    int	matchFound = 0;
    dacl_permset_t addlOwnerMask = 0;
    dacl_permset_t addlGroupMask = 0;
    dacl_permset_t addlOtherMask = 0;
    
    int i ;	/* for accumulating the group perms */
    dacl_permset_t accumulatedGroupPerms = 0L;
    dacl_permset_t accumulatedGroupDelegatePerms = 0L;
    
    int realmIsDefault;
    int	maskObjPresent = 0;	

#if defined(DACL_EPISODE)
    int	j;	/* for walking through groupsets */
    sec_id_t *foreignGroupIdListP;
    unsigned int	numForeignGroups;
    uuid_t RealmUUID;
#endif /* defined(DACL_EPISODE) */
	
#if defined(DACL_EPISODE)
    static char routineName[] = "dacl_epi_DetermineAccessAllowed";
#else /* defined(DACL_EPISODE) */
    static char routineName[] = "dacl_DetermineAccessAllowed";
#endif /* defined(DACL_EPISODE) */

#if !defined(DACL_EPISODE)
  if ((userIdP != (epi_uuid_t *)NULL) && (dacl_IsSuperUser(userIdP) == 0)) {
    rtnVal = dacl_SuperUserCheck(accessRequestedP, &accessGranted);
  }
  else { 
    /* everybody else */
#endif

#if defined(DACL_EPISODE)
      auxRtnVal = dacl_epi_CheckAccessParams(aclP, permBitsP, accessRequestedP,
					     userObjP, groupObjP);
#else /* defined(DACL_EPISODE) */
      auxRtnVal = dacl_CheckAccessParams(aclP, permBitsP, accessRequestedP,
					 userObjP, groupObjP);
#endif /* defined(DACL_EPISODE) */

      if (auxRtnVal == 0) {
	  /* just make the following comparison once */
	  bzero((char *)&zeroUuid, sizeof(epi_uuid_t));
	  
	  realmIsDefault = ((realmIdP == (epi_uuid_t *)NULL) ||
			    /* This clause should only be necessary
			     * for operation of completely
			     * stand-alone Episode. Otherwise, we
			     * should have a real uuid for the local
			     * realm to have been used by the
			     * operation that makes pacs from struct
			     * ucreds.  
			     */
			    (bcmp((char *)realmIdP, (char *)&zeroUuid, 
				  sizeof(epi_uuid_t)) == 0) ||
			    (bcmp((char *)realmIdP, 
				  (char *)&(aclP->default_realm),
				  sizeof(epi_uuid_t)) == 0));
	  
	  if (permBitsP != (u_int32 *)NULL) {
	      dacl_AddPermBitsToPermsets(*permBitsP, &addlOwnerMask, 
					 &addlGroupMask, &addlOtherMask);
	  }
	  
	  /* pull out the simple entries right away */
	  
	  if (aclP->simple_entries[(int)dacl_simple_entry_type_maskobj].is_entry_good == 1) {
	      maskObjPermMask =
		  aclP->simple_entries[(int)dacl_simple_entry_type_maskobj].perms;

#if defined(DACL_EPISODE)
	      maskObjPermMask = (maskObjPermMask & ~DACL_PERM_RWX) | 
		  addlGroupMask;
#else /* defined(DACL_EPISODE) */
	      maskObjPermMask |= addlGroupMask;
#endif /* defined(DACL_EPISODE) */
	      maskObjPresent = 1;
	  }
	  else {
	      maskObjPermMask = ~0L;
	      maskObjPresent = 0;
	  }
	  
	  if ((aclP->simple_entries[(int)dacl_simple_entry_type_userobj].is_entry_good == 1) &&
	      (userIdP != (epi_uuid_t *)NULL) && 
	      (userObjP != (epi_principal_id_t *)NULL) &&
	      (Epi_PrinId_Cmp(userIdP, userObjP) == 0) &&
	      (realmIsDefault != 0)) {
	      userObjEntryP = &(aclP->simple_entries[(int)dacl_simple_entry_type_userobj]);
	  }
	  
	  /* 
	   * we set the groupObjEntryP when we iterate through the
	   * local groups in the PAC 
	   */
	  
	  if (aclP->simple_entries[(int)dacl_simple_entry_type_otherobj].is_entry_good == 1) {
	      otherObjEntryP = &(aclP->simple_entries[(int)dacl_simple_entry_type_otherobj]);
	  }
	  
	  if (do_dlg && 
	      aclP->simple_entries[(int)dacl_simple_entry_type_otherobj_delegate].is_entry_good == 1) {
	      otherObjDelegateEntryP = &(aclP->simple_entries[(int)dacl_simple_entry_type_otherobj]);
	  }
	  
	  if (aclP->simple_entries[(int)dacl_simple_entry_type_anyother].is_entry_good == 1) {
	      anyOtherEntryP = &(aclP->simple_entries[(int)dacl_simple_entry_type_anyother]);
	  } 
	  if (do_dlg && 
	      aclP->simple_entries[(int)dacl_simple_entry_type_anyother_delegate].is_entry_good == 1) {
	      anyOtherDelegateEntryP = &(aclP->simple_entries[(int)dacl_simple_entry_type_anyother_delegate]);
	  }
	  
#if defined(DACL_EPISODE)
	  /*
	   * unauthenticated users come in as authenticated nobody
	   */
	  unauthPermMask = ~0L;
#else /* defined(DACL_EPISODE) */
	  
	  /* 
	   * Get the info we need to calc the unauth mask. The mask
	   * is set to something approp in every code path here.
	   */
	  
	  if (isAuthn == 1) {
	      /* doesn't remove any bits if access is authenticated */
	      unauthPermMask = ~0L;	
	  }
	  else {
	      unauthEntryP = &(aclP->simple_entries[(int)dacl_simple_entry_type_unauthmask]);
	      if (unauthEntryP->is_entry_good == 1) {
		  unauthPermMask = unauthEntryP->perms;
		  DACL_ACCESS_MASK_ACCESS(unauthPermMask, 0);
	      }
	      else {
		  /* removes all bits if unauth & no mask specified by ACL */
		  unauthPermMask = 0L;
	      }
	  }
#endif /* defined(DACL_EPISODE */
	  
	  /* now, check the lists only if we must */
	  if (userObjEntryP == (dacl_simple_entry_t *)NULL) {
	      userEntryP = dacl_FindMatchingListEntry(userIdP, 
						      (epi_uuid_t *)NULL,
						      (realmIsDefault) ? (epi_uuid_t *)NULL : realmIdP,
						      aclP->complex_entries[(int)dacl_complex_entry_type_user].complex_entries,
						      aclP->complex_entries[(int)dacl_complex_entry_type_user].num_entries);
	      
	      /*
	       * if not found in the user list, check the user_delegate list 
	       */
	      if (do_dlg) {
		  userDelegateEntryP = 
		      dacl_FindMatchingListEntry(userIdP, (epi_uuid_t *)NULL,
						 ((realmIsDefault) ? (epi_uuid_t *)NULL : realmIdP),
						 aclP->complex_entries[(int)dacl_complex_entry_type_user_delegate].complex_entries,
						 aclP->complex_entries[(int)dacl_complex_entry_type_user_delegate].num_entries);
	      }
	      
	      /*
	       * If matching user entry or user_delegate entry not found 
	       * check groups. 
	       */
	      if ((userEntryP == (dacl_entry_t *)NULL) &&
		  (!do_dlg || 
		   (userDelegateEntryP == (dacl_entry_t *)NULL))) {
		  /* incorporate the primary group into the checks */
		  if (groupIdP != (epi_uuid_t *)NULL) {
		      if ((aclP->simple_entries[(int)dacl_simple_entry_type_groupobj].is_entry_good == 1) &&
			  (groupObjP != (epi_principal_id_t *)NULL) &&
			  (Epi_PrinId_Cmp(groupIdP, groupObjP) == 0) &&
			  (realmIsDefault != 0)) {
			  groupObjEntryP =
			      &(aclP->simple_entries[(int)dacl_simple_entry_type_groupobj]);
		      }
		      
		      if (do_dlg &&
			  (aclP->simple_entries[(int)dacl_simple_entry_type_groupobj_delegate].is_entry_good == 1) &&
			  (groupObjP != (epi_principal_id_t *)NULL) &&
			  (Epi_PrinId_Cmp(groupIdP, groupObjP) == 0) &&
			  (realmIsDefault != 0)) {
			  groupObjDelegateEntryP = 
			      &(aclP->simple_entries[(int)dacl_simple_entry_type_groupobj_delegate]);
		      }
		      
		      /* 
		       * Now, check for match in ACL lists. Do this even 
		       * if we found a match for group_obj 
		       */
		      groupEntryP =
			  dacl_FindMatchingListEntry((epi_uuid_t *)NULL, 
						     groupIdP, 
						     (realmIsDefault) ? (epi_uuid_t *)NULL : realmIdP,
						     aclP->complex_entries[(int)dacl_complex_entry_type_group].complex_entries,
						     aclP->complex_entries[(int)dacl_complex_entry_type_group].num_entries);
		      
		      if (groupEntryP != (dacl_entry_t *)NULL) {
			  accumulatedGroupPerms = groupEntryP->perms;
			  groupKeyMatched = 1;
			  groupEntryP = (dacl_entry_t *)NULL;
		      }
		      
		      if (do_dlg) {
			  groupDelegateEntryP = 
			      dacl_FindMatchingListEntry((epi_uuid_t *)NULL, 
							 groupIdP,
							 (realmIsDefault) ? (epi_uuid_t *)NULL : realmIdP, 
							 aclP->complex_entries[(int)dacl_complex_entry_type_group_delegate].complex_entries, 
							 aclP->complex_entries[(int)dacl_complex_entry_type_group_delegate].num_entries);
			  if (groupDelegateEntryP != (dacl_entry_t *)NULL) {
			      accumulatedGroupDelegatePerms = 
				  groupDelegateEntryP->perms;
			      groupDelegateKeyMatched = 1;
			      groupDelegateEntryP = (dacl_entry_t *)NULL;
			  }
		      }
		  }
		  
		  /* first, accumulate the matching local group perms */
		  for (i = 0;
		       (i < numGroups) && DACL_ACCESS_ACCUMULATE_GROUP_PERMS;
		       i++) {
		      /* first check for match against group obj */
		      if ((groupObjEntryP == (dacl_simple_entry_t *) NULL) &&
			  (aclP->simple_entries[(int)dacl_simple_entry_type_groupobj].is_entry_good == 1) &&
			  (groupObjP != (epi_principal_id_t *)NULL) &&
			  (Epi_PrinId_Cmp(&(groupIdListP[i].uuid), 
					  groupObjP) == 0) &&
			  (realmIsDefault != 0)) {
			  groupObjEntryP =
			      &(aclP->simple_entries[(int)dacl_simple_entry_type_groupobj]);
		      }
		      
		      if (do_dlg && 
			  (groupObjDelegateEntryP ==
			   (dacl_simple_entry_t *)NULL) &&
			  (aclP->simple_entries[(int)dacl_simple_entry_type_groupobj_delegate].is_entry_good == 1) &&
			  (groupObjP != (epi_principal_id_t *)NULL) &&
			  (Epi_PrinId_Cmp(&(groupIdListP[i].uuid),
					  groupObjP) == 0) &&
			  (realmIsDefault != 0)) {
			  groupObjDelegateEntryP = 
			      &(aclP->simple_entries[(int)dacl_simple_entry_type_groupobj_delegate]);
		      }
		      
		      /* 
		       * Now, check for match in ACL lists.
		       * Do this even if we found a match for group_obj 
		       */
		      groupEntryP = 
			  dacl_FindMatchingListEntry((epi_uuid_t*)NULL,
						     &(groupIdListP[i].uuid),
						     (realmIsDefault) ? (epi_uuid_t *)NULL : realmIdP,
						     aclP->complex_entries[(int)dacl_complex_entry_type_group].complex_entries,
						     aclP->complex_entries[(int)dacl_complex_entry_type_group].num_entries);
		      
		      if (groupEntryP != (dacl_entry_t *)NULL) {
			  accumulatedGroupPerms |= groupEntryP->perms;
			  groupKeyMatched = 1;
			  groupEntryP = (dacl_entry_t *)NULL;
		      }
		      
#if defined(DACL_EPISODE)
		      if (do_dlg) {
			  /* check against group_delegate entries for match*/
			  groupDelegateEntryP =
			      dacl_FindMatchingListEntry((epi_uuid_t*)NULL,
							 &(groupIdListP[i].uuid),
							 (realmIsDefault) ? (epi_uuid_t *)NULL : realmIdP,
							 aclP->complex_entries[(int)dacl_complex_entry_type_group_delegate].complex_entries,
							 aclP->complex_entries[(int)dacl_complex_entry_type_group_delegate].num_entries);
			  if (groupDelegateEntryP != (dacl_entry_t *)NULL) {
			      accumulatedGroupDelegatePerms |= 
				  groupDelegateEntryP->perms;
			      groupDelegateKeyMatched = 1;
			      groupDelegateEntryP = (dacl_entry_t *)NULL;
			  }
		      }
#endif /* defined(DACL_EPISODE) */
		  }
		  
#if defined(DACL_EPISODE)
		  for (j = 0; j < ((int)numForeignGrpSets); j++) {
		      numForeignGroups = foreignGroupSetP[j].num_groups;
		      foreignGroupIdListP = foreignGroupSetP[j].groups;
		      RealmUUID = foreignGroupSetP[j].realm.uuid;
#endif /* defined(DACL_EPISODE) */
		      /* now, collect the foreign group perms */
		      for (i = 0;
			   (i < numForeignGroups) && 
			   DACL_ACCESS_ACCUMULATE_GROUP_PERMS;
			   i++) {
			  groupEntryP = 
			      dacl_FindMatchingListEntry((epi_uuid_t*)NULL,

#if defined(DACL_EPISODE)
							 &(foreignGroupIdListP[i].uuid),
							 &RealmUUID,
#else
							 &(foreignGroupIdListP[i].id.uuid),
							 &(foreignGroupIdListP[i].realm.uuid),
#endif /* defined(DACL_EPISODE) */
							 aclP->complex_entries[(int)dacl_complex_entry_type_group].complex_entries,
							 aclP->complex_entries[(int)dacl_complex_entry_type_group].num_entries);
			  
			  if (groupEntryP != (dacl_entry_t *)NULL) {
			      accumulatedGroupPerms |= groupEntryP->perms;
			      groupKeyMatched = 1;
			      groupEntryP = (dacl_entry_t *)NULL;
			  } 
			  
#if defined(DACL_EPISODE)
			  if (do_dlg) {
			      groupDelegateEntryP =
				  dacl_FindMatchingListEntry((epi_uuid_t*)NULL,
							     &(foreignGroupIdListP[i].uuid),
							     &RealmUUID,
							     aclP->complex_entries[(int)dacl_complex_entry_type_group_delegate].complex_entries,
							     aclP->complex_entries[(int)dacl_complex_entry_type_group_delegate].num_entries);
			      if (groupDelegateEntryP != 
				  (dacl_entry_t *)NULL) {
				  accumulatedGroupDelegatePerms |= 
				      groupDelegateEntryP->perms;
				  groupDelegateKeyMatched = 1;
				  groupDelegateEntryP = (dacl_entry_t *)NULL;
			      }
			  } /* do_dlg */
#endif /* defined(DACL_EPISODE) */
		      } /* numForeignGroups */
#ifdef DACL_EPISODE
		  } /* for loop - numforeignGrpSets */
#endif	  
		  if ((groupObjEntryP == (dacl_simple_entry_t *)NULL) && 
		      !groupKeyMatched &&
		      (!do_dlg || 
		       ((groupObjDelegateEntryP == 
			 (dacl_simple_entry_t *)NULL) && 
			!groupDelegateKeyMatched))) {
		      otherEntryP =
			  dacl_FindMatchingListEntry((epi_uuid_t *)NULL,
						     (epi_uuid_t *)NULL, 
						     realmIdP,
						     aclP->complex_entries[(int)dacl_complex_entry_type_other].complex_entries,
						     aclP->complex_entries[(int)dacl_complex_entry_type_other].num_entries);
		      if (do_dlg) {
			  otherDelegateEntryP = 
			      dacl_FindMatchingListEntry((epi_uuid_t *)NULL,
							 (epi_uuid_t *)NULL, 
							 realmIdP,
							 aclP->complex_entries[(int)dacl_complex_entry_type_for_other_delegate].complex_entries,
							 aclP->complex_entries[(int)dacl_complex_entry_type_for_other_delegate].num_entries);
		      }
		  }
	      }
	  }
	  
	  /*
	   * Now, we have pointers to all the matching acl entries, we just 
	   * have to check them in the required order.
	   */
	  if (do_dlg == 0) {
	      /* USER_OBJ */
	      if (userObjEntryP != (dacl_simple_entry_t *)NULL) {
#if defined(DACL_EPISODE)
		  accessGranted = 
		      ((userObjEntryP->perms & ~DACL_PERM_RWX) |
		       addlOwnerMask);
#else /* defined(DACL_EPISODE) */
		  accessGranted = (userObjEntryP->perms | addlOwnerMask);
#endif /* defined(DACL_EPISODE) */
		  DACL_ACCESS_MASK_ACCESS(accessGranted, 0);
		  accessGranted &= unauthPermMask;
		  userKeyMatched = 1;
	      }
	      /* USER */
	      else if (userEntryP != (dacl_entry_t *)NULL) {
		  accessGranted = userEntryP->perms;
		  DACL_ACCESS_MASK_ACCESS(accessGranted, 1);
		  accessGranted &= unauthPermMask;
		  userKeyMatched = 1;
	      }
	      /* GROUP_OBJ */
	      else if (groupObjEntryP != (dacl_simple_entry_t *)NULL) {
		  if (maskObjPresent == 0) {
		      /* if there is no mask obj, we use the addlGroupMask 
		       * here */
#if defined(DACL_EPISODE)
		      accessGranted = 
			  (groupObjEntryP->perms & ~DACL_PERM_RWX) | 
			      addlGroupMask;
#else /* defined(DACL_EPISODE) */
		      accessGranted = 
			  (groupObjEntryP->perms | addlGroupMask);
#endif /* defined(DACL_EPISODE) */
		  }
		  else {
		      /*
		       * If there is a mask, we have to or in all the other 
		       * matching group perms, too.
		       */
		      accessGranted =
			  groupObjEntryP->perms | accumulatedGroupPerms;
		  }
		  
		  /*
		   * Doing the following masking is all right here, because, 
		   * if the mask_obj entry is not present, the mask is set 
		   * to all ones
		   */
		  DACL_ACCESS_MASK_ACCESS(accessGranted, 1);
		  accessGranted &= unauthPermMask;
	      }
	      /* GROUP */
	      else if (groupKeyMatched) {
		  accessGranted = accumulatedGroupPerms;
		  DACL_ACCESS_MASK_ACCESS(accessGranted, 1);
		  accessGranted &= unauthPermMask;
	      }
	      /* OTHER_OBJ */
	      else if ((otherObjEntryP != (dacl_simple_entry_t *)NULL) &&
		       (realmIsDefault)) {
		  /* other_obj is only supposed to apply for the local 
		   * realm */
#if defined(DACL_EPISODE)
		  accessGranted =
		      ((otherObjEntryP->perms & ~DACL_PERM_RWX) | 
		       addlOtherMask);
#else /* defined(DACL_EPISODE) */
		  accessGranted = (otherObjEntryP->perms | addlOtherMask);
#endif /* defined(DACL_EPISODE) */
		  DACL_ACCESS_MASK_ACCESS(accessGranted, 0);
		  accessGranted &= unauthPermMask;
	      }
	      /* OTHER */
	      else if (otherEntryP != (dacl_entry_t *)NULL) {
		  accessGranted = otherEntryP->perms;
		  DACL_ACCESS_MASK_ACCESS(accessGranted, 1);
		  accessGranted &= unauthPermMask;
	      }
	      /* ANY_OTHER */
	      else if (anyOtherEntryP != (dacl_simple_entry_t *)NULL) {
		  accessGranted = anyOtherEntryP->perms;
		  DACL_ACCESS_MASK_ACCESS(accessGranted, 1);
		  accessGranted &= unauthPermMask;
	      }
	  } else {
	      /* Check delegate entries also */
	      /* USER_OBJ */
	      if (userObjEntryP != (dacl_simple_entry_t *)NULL) {
#if defined(DACL_EPISODE)
		  accessGranted = 
		      ((userObjEntryP->perms & ~DACL_PERM_RWX) | 
		       addlOwnerMask);
#else /* defined(DACL_EPISODE) */
		  accessGranted = (userObjEntryP->perms | addlOwnerMask);
#endif /* defined(DACL_EPISODE) */
		  DACL_ACCESS_MASK_ACCESS(accessGranted, 0);
		  userKeyMatched = 1;
		  matchFound = 1;
		  
		  /* 
		   * user_obj_delegate is always ignored because the
		   * owner of the object has all rights to the
		   * object irrespective whether its acting as a
		   * delegate. As the owner can always change the
		   * ACL anytime, it argues against not granting the
		   * owner of a file all rights when acting as a
		   * delegate.
		   */
	      }
	      if (matchFound) 
		  goto matched;
	      
	      /* USER */
	      if (userEntryP != (dacl_entry_t *)NULL) {
		  accessGranted = userEntryP->perms;
		  userKeyMatched = 1;
		  matchFound = 1;
	      }
	      if (userDelegateEntryP != (dacl_entry_t *)NULL) {
		  accessGranted |= userDelegateEntryP->perms;
		  userKeyMatched = 1;
		  matchFound = 1;
	      }
	      if (matchFound) {
		  DACL_ACCESS_MASK_ACCESS(accessGranted, 1);
		  goto matched;
	      }
	      
	      /* GROUP_OBJ */
	      if (groupObjEntryP != (dacl_simple_entry_t *)NULL) {
		  if (maskObjPresent == 0) {
		      /* if there is no mask obj, we use the addlGroupMask 
		       * here */
#if defined(DACL_EPISODE)
		      accessGranted = 
			  (groupObjEntryP->perms & ~DACL_PERM_RWX) | 
			      addlGroupMask;
#else /* defined(DACL_EPISODE) */
		      accessGranted = (groupObjEntryP->perms |
				       addlGroupMask);
#endif /* defined(DACL_EPISODE) */
		  } else {
		      /*
		       * If there is a mask, we have to or in all the other 
		       * matching group perms, too.
		       */
		      accessGranted = groupObjEntryP->perms;
		  }
		  matchFound = 1;
	      }
	      if (groupObjDelegateEntryP != (dacl_simple_entry_t *)NULL) {
		  accessGranted |= groupObjDelegateEntryP->perms;
		  matchFound = 1;
	      }
	      if (groupKeyMatched) {
		  accessGranted |= accumulatedGroupPerms;
		  matchFound = 1;
	      }
	      if (groupDelegateKeyMatched) {
		  accessGranted |= accumulatedGroupDelegatePerms;
		  matchFound = 1;
	      }
	      if (matchFound) {
		  DACL_ACCESS_MASK_ACCESS(accessGranted, 1);
		  goto matched;
	      }
	      
	      /* OTHER_OBJ */
	      if ((otherObjEntryP != (dacl_simple_entry_t *)NULL) &&
		  (realmIsDefault)) {
		  /* other_obj is only supposed to apply for the local 
		   * realm */
#if defined(DACL_EPISODE)
		  accessGranted = 
		      ((otherObjEntryP->perms & ~DACL_PERM_RWX) | 
		       addlOtherMask);
#else /* defined(DACL_EPISODE) */
		  accessGranted = (otherObjEntryP->perms | addlOtherMask);
#endif /* defined(DACL_EPISODE) */
		  matchFound = 1;
	      }
	      
	      if ((otherObjDelegateEntryP != (dacl_simple_entry_t *)NULL) &&
		  (realmIsDefault)) {
		  /* other_obj is only supposed to apply for the local 
		   * realm */
		  accessGranted |= otherObjDelegateEntryP->perms;
		  matchFound = 1;
	      }
	      if (matchFound) {
		  DACL_ACCESS_MASK_ACCESS(accessGranted, 0);
		  goto matched;
	      }
	      
	      /* FOREIGN_OTHER */
	      if (otherEntryP != (dacl_entry_t *)NULL) {
		  accessGranted = otherEntryP->perms;
		  matchFound = 1;
	      }
	      
	      if (otherDelegateEntryP != (dacl_entry_t *)NULL) {
		  accessGranted = otherDelegateEntryP->perms;
		  matchFound = 1;
	      }
	      if (matchFound) {
		  DACL_ACCESS_MASK_ACCESS(accessGranted, 1);
		  goto matched;
	      }
	      
	      /* ANY_OTHER */
	      if (anyOtherEntryP != (dacl_simple_entry_t *)NULL) {
		  accessGranted = anyOtherEntryP->perms;
		  matchFound = 1;
	      }
	      if (anyOtherDelegateEntryP != (dacl_simple_entry_t *)NULL) {
		  accessGranted = anyOtherDelegateEntryP->perms;
		  matchFound = 1;
	      }
	      if (matchFound) {
		  DACL_ACCESS_MASK_ACCESS(accessGranted, 1);
	      }
	      
	    matched:
	      
	      accessGranted &= unauthPermMask;
	  }      
	  if ((*accessRequestedP & accessGranted) == *accessRequestedP) {
	      rtnVal = DACL_SUCCESS;
	  }
	  else if (userKeyMatched == 1) {
	      /*
	       * In this case, there was a user match, but the access wasn't 
	       * allowed. No further checking should be done by the caller 
	       * of this routine.
	       */
	      rtnVal = DACL_ERROR_ACCESS_EXPLICITLY_DENIED;
          }
      } 
      /* if all parameters check out all right */
      else {
	  rtnVal = auxRtnVal;
      }
#if !defined(DACL_EPISODE)
  } /* everybody else */
#endif
  
    /* don't tell the caller about the set if the caller didn't ask */
    if (accessAllowedP) {
	*accessAllowedP = accessGranted;
    }
    
    return rtnVal;
}


#undef DACL_ACCESS_MASK_ACCESS

#ifdef notdef
#if defined(DACL_EPISODE)
EXPORT long dacl_epi_CheckAccessId(
#else /* defined(DACL_EPISODE) */
EXPORT long dacl_CheckAccessId(
#endif /* defined(DACL_EPISODE) */
     dacl_t *			aclP,
     u_int32 *			permBitsP,
     dacl_permset_t *		accessRequestedP,
     epi_uuid_t *		realmIdP,
     epi_uuid_t *		userIdP,
     epi_uuid_t *		groupIdP,
     epi_sec_id_t *		groupIdListP,
     unsigned int		numGroups,
     epi_sec_id_foreign_t *	foreignGroupIdListP,
     unsigned int		numForeignGroups,
     epi_principal_id_t *	userObjP,
     epi_principal_id_t *	groupObjP,
     int			isAuthn)
{

  long rtnVal = DACL_ERROR_ACCESS_DENIED; 	/* assume access denied */
  dacl_permset_t accessAllowed = 0L;		/* assume nothing granted */

#if defined(DACL_EPISODE)
  rtnVal = 
      dacl_epi_DetermineAccessAllowed(aclP, permBitsP, accessRequestedP,
				      realmIdP, userIdP, groupIdP,
				      groupIdListP, numGroups,
				      foreignGroupIdListP, numForeignGroups,
				      userObjP, groupObjP, isAuthn,
				      0 /* no delegation */,
				      &accessAllowed);
#else /* defined(DACL_EPISODE) */
  rtnVal = 
      dacl_DetermineAccessAllowed(aclP, permBitsP, accessRequestedP,
				  realmIdP, userIdP, groupIdP,
				  groupIdListP, numGroups,
				  foreignGroupIdListP, numForeignGroups,
				  userObjP, groupObjP, isAuthn,
				  &accessAllowed);
#endif /* defined(DACL_EPISODE) */
  return rtnVal;
}
#endif /* notdef */
