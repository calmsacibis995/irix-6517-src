/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: dacl_pac.c,v 65.4 1998/03/23 16:36:26 gwehrman Exp $";
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
 * $Log: dacl_pac.c,v $
 * Revision 65.4  1998/03/23 16:36:26  gwehrman
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS && !KERNEL
 *
 * Revision 65.3  1997/11/06 20:00:33  jdoak
 * Source Identification added.
 *
 * Revision 65.2  1997/10/24  22:01:16  gwehrman
 * Changed include for pthread.h
 * to dce/pthread.h.
 *
 * Revision 65.1  1997/10/20  19:20:16  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.747.1  1996/10/02  18:15:48  damon
 * 	Newest DFS from Transarc
 * 	[1996/10/01  18:46:43  damon]
 *
 * $EndLog$
 */
/*
 *	dacl_pac.c -- the only file that needs to include dce include files
 *
 * Copyright (C) 1991, 1996 Transarc Corporation
 * All rights reserved.
 */

#include <dcedfs/param.h>
#include <dcedfs/sysincludes.h>
#include <dcedfs/stds.h>
#include <dcedfs/osi.h>
#include <dcedfs/osi_cred.h>
#include <dcedfs/common_data.h>
#include <dcedfs/icl.h>

#include <dce/uuid.h>
#include <dce/nbase.h>
#include <dce/id_base.h>

#include <epi_id.h>
#include <dacl.h>
#include <dacl_debug.h>
#include <dacl_trace.h>


#if !defined(KERNEL) 
#include <dce/pthread.h>
#endif /* !KERNEL */

extern struct icl_set *dacl_iclSetp;
  
RCSID("$Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/security/dacl/RCS/dacl_pac.c,v 65.4 1998/03/23 16:36:26 gwehrman Exp $")

#if defined(KERNEL)
#ifdef DACL_EPISODE
extern epi_uuid_t dacl_localCellID;
extern epi_uuid_t dacl_sysAdminGroupID;
extern epi_uuid_t dacl_networkRootID;
#else
epi_uuid_t dacl_localCellID;
epi_uuid_t dacl_sysAdminGroupID;
epi_uuid_t dacl_networkRootID;

void
dacl_SetLocalCellID(afsUUID *localCellID)
{
    dacl_localCellID = *((epi_uuid_t *) localCellID);
}
    
void
dacl_SetSysAdminGroupID(afsUUID *sysAdminGroupID)
{
    dacl_sysAdminGroupID = *((epi_uuid_t *) sysAdminGroupID);
    bzero ((char *)&dacl_networkRootID, sizeof(epi_uuid_t));
}

void
dacl_GetLocalCellID(afsUUID *localCellID)
{
    *((epi_uuid_t *) localCellID) = dacl_localCellID;
}

void
dacl_GetSysAdminGroupID(afsUUID *groupID)
{
    *((epi_uuid_t *) groupID) = dacl_sysAdminGroupID;
}

/* 
 * XXX
 * Transformations of other "bcopy"s into structure assignments coming 
 * in another delta for 6274.
 */
void 
dacl_GetNetworkRootID(afsUUID *networkRootID)
{
    *networkRootID = *((afsUUID *)&dacl_networkRootID);
}

#endif /* DACL_EPISODE */
#endif /* defined(KERNEL) */

#if defined(DACL_EPISODE)
EXPORT long dacl_epi_CheckAccessAllowedPac(
     dacl_t *			aclP,
     u_int32 *			permBitsP,
     dacl_permset_t *		accessRequestedP,
     pac_list_t *		pacListP, 
     epi_principal_id_t *	userObjP,
     epi_principal_id_t *	groupObjP,
     int 			networkRootCheck,
     dacl_permset_t *		accessAllowedP)
#else /* defined(DACL_EPISODE) */
EXPORT long dacl_CheckAccessAllowedPac(
     dacl_t *			aclP,
     u_int32 *			permBitsP,
     dacl_permset_t *		accessRequestedP,
     sec_id_pac_t *		pacP,
     epi_principal_id_t *	userObjP,
     epi_principal_id_t *	groupObjP,
     dacl_permset_t *		accessAllowedP)
#endif /* defined(DACL_EPISODE */
{
    long rtnVal = DACL_ERROR_ACCESS_DENIED; /* assume access denied */
    unsigned int i;
    int	superGroupFound = 0;
    int networkRootFound = 0;
    dacl_permset_t localAccessAllowed = 0;

#if  defined(DACL_LOCAL_DEBUG) && !defined(KERNEL)
    unsigned_char_p_t uuidString;
    unsigned32 uuidStatus;
#endif  /* defined(DACL_LOCAL_DEBUG) && !defined(KERNEL) */

#if defined(DACL_EPISODE)
    int	isPacAuthn, j, do_dlg = 0; 
    unsigned16 NumGroups;
    sec_id_t *GroupsP;
    uuid_t RealmUUID;
    pac_list_t *tmpPacListP; 
    pac_list_t *lastDlgP; 
    sec_id_pa_t *pacP;
    dacl_permset_t	accessAcrossPacList = 0;
    static char		routineName[] = "dacl_epi_CheckAccessAllowedPac";
#else 
    int	isPacAuthn = (pacP->authenticated == true);
    int networkRootCheck = 0;
    static char		routineName[] = "dacl_CheckAccessAllowedPac";
#endif /* defined(DACL_EPISODE) */
    
#if defined(DACL_LOCAL_DEBUG) && !defined(KERNEL)
    uuid_to_string(&(pacP->principal.uuid), &uuidString, &uuidStatus);
    if (uuidStatus == error_status_ok) {
	icl_Trace2(dacl_iclSetp, DACL_ICL_PAC_WARNINGS_1 , 
		   ICL_TYPE_STRING, routineName, 
		   ICL_TYPE_STRING, uuidString);
	rpc_string_free(&uuidString, &uuidStatus);
    }
    else {
	icl_Trace1(dacl_iclSetp, DACL_ICL_PAC_WARNINGS_2 , 
		   ICL_TYPE_STRING, routineName);
    }
#endif /* defined(DACL_LOCAL_DEBUG) && !defined(KERNEL) */
    
#if defined(DACL_EPISODE)
    tmpPacListP = pacListP;
    do { 
	lastDlgP = tmpPacListP;

	pacP = tmpPacListP->pacp;
	isPacAuthn = tmpPacListP->authenticated;
	
	rtnVal =
	    dacl_epi_DetermineAccessAllowed(aclP, permBitsP, accessRequestedP,
					    (epi_uuid_t *)(&(pacP->realm.uuid)),
					    (epi_uuid_t *)(&(pacP->principal.uuid)),
					    (epi_uuid_t *)(&(pacP->group.uuid)),
					    (epi_sec_id_t *)(pacP->groups),
					    pacP->num_groups,
					    pacP->foreign_groupsets,
					    pacP->num_foreign_groupsets,
					    userObjP, groupObjP, isPacAuthn,
					    do_dlg, &localAccessAllowed);
#else /* defined(DACL_EPISODE) */
	rtnVal = 
	    dacl_DetermineAccessAllowed(aclP, permBitsP, accessRequestedP,
					(epi_uuid_t *)(&(pacP->realm.uuid)),
					(epi_uuid_t *)(&(pacP->principal.uuid)),
					(epi_uuid_t *)(&(pacP->group.uuid)),
					(epi_sec_id_t *)(pacP->groups),
					pacP->num_groups,
					(epi_sec_id_foreign_t *)(pacP->foreign_groups),
					pacP->num_foreign_groups,
					userObjP, groupObjP, isPacAuthn,
					&localAccessAllowed);
#endif /* defined(DACL_EPISODE) */
	if (rtnVal == DACL_ERROR_ACCESS_EXPLICITLY_DENIED) 
	    rtnVal = DACL_ERROR_ACCESS_DENIED;
	
#if defined(KERNEL)
	/* 
	 * check whether or not the pac contains the super group; we may 
	 * have to add c rights 
	 *
	 * first, check the local groups (if appropriate) 
	 */
	if (bcmp((char *)&dacl_localCellID,
		 (char *)(&(pacP->realm.uuid)), 
		 sizeof(epi_uuid_t)) == 0) {

	    if (networkRootCheck && 
		(Epi_PrinId_Cmp(&dacl_networkRootID,
				&(pacP->principal.uuid)) == 0))
		networkRootFound = 1;

	    if (!networkRootFound) {
		if (Epi_PrinId_Cmp(&dacl_sysAdminGroupID,
				   &(pacP->group.uuid)) == 0)
		    superGroupFound = 1;
		for (i = 0;
		     (superGroupFound == 0) && (i < pacP->num_groups);
		     i++) {
		    if (Epi_PrinId_Cmp(&dacl_sysAdminGroupID,
				       &(pacP->groups[i].uuid)) == 0) {
			superGroupFound = 1;
		    }
		}
	    }
	}
	
	/* if we still haven't found it, check the foreign groups */
	if (!networkRootFound && !superGroupFound) {
#if defined(DACL_EPISODE)
	    for (j = 0;
		 (superGroupFound == 0) && (j < pacP->num_foreign_groupsets); 
		 j++) {
		RealmUUID = pacP->foreign_groupsets[j].realm.uuid;
		if (bcmp((char *)&dacl_localCellID,
			  (char *)&(RealmUUID), 
			  sizeof(uuid_t) == 0)) {
		    NumGroups = pacP->foreign_groupsets[j].num_groups;
		    GroupsP = pacP->foreign_groupsets[j].groups;
		    for (i = 0; 
			 (superGroupFound == 0) && (i < NumGroups); 
			 i++) {
			if (Epi_PrinId_Cmp(&dacl_sysAdminGroupID,
					   &(GroupsP[i].uuid)) == 0) 
			    superGroupFound = 1;
		    }
		}
	    }
#else /* defined(DACL_EPISODE) */
	    for (i = 0;
		 (superGroupFound == 0) && (i < pacP->num_foreign_groups);
		 i++) {
		if ((bcmp((char *)&dacl_localCellID,
			  (char *)&(pacP->foreign_groups[i].realm.uuid),
			  sizeof(epi_uuid_t) == 0)) &&
		    (Epi_PrinId_Cmp(&dacl_sysAdminGroupID,
				    &(pacP->foreign_groups[i].id.uuid))
		     == 0)) {
		    superGroupFound = 1;
		}
	    }
#endif /* defined(DACL_EPISODE) */
	}
#endif /* defined(KERNEL) */

	if (networkRootFound) 
	    localAccessAllowed |= (dacl_perm_control | dacl_perm_read | 
				   dacl_perm_execute | dacl_perm_write |
				   dacl_perm_insert | dacl_perm_delete);
	if (superGroupFound)
	    localAccessAllowed |= dacl_perm_control;

#if defined(DACL_EPISODE)
	if (pacP == pacListP->pacp) {
	    accessAcrossPacList = localAccessAllowed;
	    do_dlg = 1;
	} else
	    accessAcrossPacList &= localAccessAllowed;
	
	tmpPacListP = tmpPacListP->next;
    } while (tmpPacListP); /* do while */
    
    localAccessAllowed = accessAcrossPacList;
    
    /*
     * Add "c" permission if the last delegate in the chain is
     * the file owner.  This maintains POSIX 1003.1 semantics that
     * says a process with an open file has the rights associated
     * with the file descriptor regardless of the ACL.  px_rdwr()
     * in the fxd server checks for "c" access and grants read and 
     * write permission in that case.
     * Check whether the caller (lastDlgP->pacp-> {uuid, realm})
     * matches the owner (userObjP, aclP->default_realm).
     */ 
    if ((Epi_PrinId_Cmp (&lastDlgP->pacp->principal.uuid, userObjP) == 0)
	&& (bcmp((char *)(epi_uuid_t *)(&lastDlgP->pacp->realm.uuid), 
		 (char *)&(aclP->default_realm),
		 sizeof(epi_uuid_t)) == 0))
	localAccessAllowed |= dacl_perm_control;
    
#endif /* defined(DACL_EPISODE) */
    
    if ((*accessRequestedP & localAccessAllowed) == *accessRequestedP)
	rtnVal = DACL_SUCCESS;
    else
	rtnVal = DACL_ERROR_ACCESS_DENIED;
    
    if (accessAllowedP != (dacl_permset_t *)NULL) {
	*accessAllowedP = localAccessAllowed;
    }
    
    return rtnVal;
}

#if defined(DACL_EPISODE)
EXPORT long dacl_epi_CheckAccessPac(
     dacl_t *			aclP,
     u_int32 *			permBitsP,
     dacl_permset_t *		accessRequestedP,
     pac_list_t *		pacListP,
     epi_principal_id_t *	userObjP,
     epi_principal_id_t *	groupObjP,
     int			networkRootCheck)
#else /* defined(DACL_EPISODE) */
EXPORT long dacl_CheckAccessPac(
     dacl_t *			aclP,
     u_int32 *			permBitsP,
     dacl_permset_t *		accessRequestedP,
     sec_id_pac_t *		pacP,
     epi_principal_id_t *	userObjP,
     epi_principal_id_t *	groupObjP)
#endif /* defined(DACL_EPISODE) */     
{
    long rtnVal = DACL_ERROR_ACCESS_DENIED;
    dacl_permset_t accessAllowed = 0L;

#if defined(DACL_EPISODE)
  rtnVal = dacl_epi_CheckAccessAllowedPac(aclP, permBitsP, accessRequestedP, 
					  pacListP, userObjP, groupObjP, 
					  networkRootCheck, &accessAllowed);
#else /* defined(DACL_EPISODE) */
  rtnVal = dacl_CheckAccessAllowedPac(aclP, permBitsP, accessRequestedP, pacP,
				      userObjP, groupObjP, &accessAllowed);
#endif /* defined(DACL_EPISODE) */     

  return rtnVal;
}

#if defined(DACL_EPISODE)
/* we really only need one copy of the rest of this file */

/*
 * First we move the uid into a longword;
 * THEN we copy it into the uuid.
 */
#define EPI_WIDE_UUID_FROM_UID(uuidP, uidP)  \
  {u_int32 luid = (u_int32) *uidP; \
   bcopy((char *)(&luid), (char *)(uuidP), sizeof(u_int32));}
#define EPI_WIDE_UUID_FROM_GID(uuidP, gidP)  \
  {u_int32 lgid = (u_int32) *gidP; \
   bcopy((char *)(&lgid), (char *)(uuidP), sizeof(u_int32));}

/*
 *  dacl_PacFromUcred - constructs a pac from a ucred structure
 *
 * N.B.:  In the use of this routine, the caller must remember whether or not
 * the pacP passed to the routine has a valid local group list pointer.  If the
 * local group list pointer is NULL, this routine will osi_Alloc memory for the
 * list; otherwise, it will reuse the memory to which the local group list
 * points.
 *
 * This routine will reuse the memory associated with the local group list,
 * if the local group pointer is non-NULL and will fill it with group
 * information from the given osi_cred_t *.  NOTE THAT THIS ASSUMES that any
 * group list memory that is reused is big enough to hold all the groups from
 * the ucred.  The caller must ensure that the group list is big enough and
 * that the memory is released appropriately.
 *
 * If the local group pointer is NULL, this routine will allocate a block of
 * memory big enough to hold all the groups in the ucred.  This memory must be
 * released by the caller by calling osi_Free(pacP->groups, pacP->num_groups).
 *
 * The foreign group list of the pacP is preserved.  If there are any foreign
 * groups associated with the pacP, pacP->foreign_groups and
 * pacP->num_foreign_groups must be set appropriately.
 *
 * All other fields in the pacP will be modified, as appropriate, by this
 * routine.
 */
EXPORT void dacl_PacFromUcred(sec_id_pac_t *pacP, osi_cred_t *ucredP)
{
  unsigned int		i;
  sec_id_t *		savedPacGroupsP = pacP->groups;
  unsigned16		savedNumForeignGroups = pacP->num_foreign_groups;
  sec_id_foreign_t *	savedPacForeignGroupsP = pacP->foreign_groups;

  bzero((char *)pacP, sizeof(sec_id_pac_t));

  pacP->pac_type = sec_id_pac_format_v1;
  pacP->authenticated = true;	/* we'll trust a pac generated in this way */

  /* restore the saved group lists, in case they were meaningful */
  pacP->groups = savedPacGroupsP;
  pacP->num_foreign_groups = savedNumForeignGroups;
  pacP->foreign_groups = savedPacForeignGroupsP;
  
#if defined(KERNEL)
  /* stuff in the local cell id, if we have it */
  *((epi_uuid_t *) &pacP->realm.uuid) = dacl_localCellID;
#endif /* defined(KERNEL) */

  EPI_WIDE_UUID_FROM_UID(&(pacP->principal.uuid), &(osi_GetUID(ucredP)));
  EPI_WIDE_UUID_FROM_GID(&(pacP->group.uuid), &(osi_GetGID(ucredP)));

  /* now, copy the concurrent groups */
  pacP->num_groups = osi_GetNGroups(ucredP);
  if (!pacP->groups)
    pacP->groups = (sec_id_t *) dacl_Alloc(pacP->num_groups * sizeof(sec_id_t));

  for (i = 0; i < pacP->num_groups; i++) {
    EPI_WIDE_UUID_FROM_GID(&(pacP->groups[i].uuid), &(ucredP->cr_groups[i]));
    pacP->groups[i].name = (ndr_char *)NULL;
  }
}

EXPORT void dacl_PacListFromUcred(pac_list_t **pacListPP, osi_cred_t *ucredP)
{
  unsigned int		i;

  *pacListPP = dacl_Alloc(sizeof(pac_list_t));
  bzero((char *)*pacListPP, sizeof(pac_list_t));
  (*pacListPP)->pacp = dacl_Alloc(sizeof(sec_id_pa_t));
  bzero((char *)(*pacListPP)->pacp, sizeof(sec_id_pa_t));

  (*pacListPP)->authenticated = true; /* we'll trust a pac generated this way */

#if defined(KERNEL)
  /* stuff in the local cell id, if we have it */
  bcopy((char *)&dacl_localCellID, (char *)(&((*pacListPP)->pacp->realm.uuid)), 
	sizeof(epi_uuid_t));
#endif /* defined(KERNEL) */

  EPI_WIDE_UUID_FROM_UID(&((*pacListPP)->pacp->principal.uuid), 
		&(osi_GetUID(ucredP)));
  EPI_WIDE_UUID_FROM_GID(&((*pacListPP)->pacp->group.uuid), 
		&(osi_GetGID(ucredP)));

  /* now, copy the concurrent groups */
  (*pacListPP)->pacp->num_groups = osi_GetNGroups(ucredP);
  if (!((*pacListPP)->pacp->groups))
    (*pacListPP)->pacp->groups = (sec_id_t *) dacl_Alloc((*pacListPP)->pacp->num_groups * sizeof(sec_id_t));

  for (i = 0; i < (*pacListPP)->pacp->num_groups; i++) {
    EPI_WIDE_UUID_FROM_GID(&((*pacListPP)->pacp->groups[i].uuid), &(ucredP->cr_groups[i]));
    (*pacListPP)->pacp->groups[i].name = (ndr_char *)NULL;
  }
}
#endif /* defined(DACL_EPISODE) */
