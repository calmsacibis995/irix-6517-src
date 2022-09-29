/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: dacl_init.c,v 65.3 1998/03/23 16:36:17 gwehrman Exp $";
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
 * $Log: dacl_init.c,v $
 * Revision 65.3  1998/03/23 16:36:17  gwehrman
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS && !KERNEL
 *
 * Revision 65.2  1997/11/06 20:00:31  jdoak
 * Source Identification added.
 *
 * Revision 65.1  1997/10/20  19:20:16  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.15.1  1996/10/02  18:15:40  damon
 * 	Newest DFS from Transarc
 * 	[1996/10/01  18:46:33  damon]
 *
 * $EndLog$
 */
/*
 *	dacl_init.c -- initialize an ACL for Episode
 *
 * Copyright (C) 1995, 1991 Transarc Corporation
 * All rights reserved.
 */

#include <dcedfs/param.h>
#include <dcedfs/stds.h>
#include <dcedfs/osi.h>
 
#include <dcedfs/fshs_deleg.h>
#include <dacl.h>
#include <dacl_debug.h>

#if defined(KERNEL)
IMPORT epi_uuid_t dacl_localCellID;
#endif /* defined(KERNEL) */

RCSID("$Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/security/dacl/RCS/dacl_init.c,v 65.3 1998/03/23 16:36:17 gwehrman Exp $")

/*
 * initialize an ACL structure suitable for use in Episode
 */
EXPORT long dacl_InitEpiAcl(theAclP, modeP, forDirectory, aclRealmP)
     dacl_t *		theAclP;
     u_int32 *		modeP;
     int		forDirectory;
     epi_uuid_t *       aclRealmP;    /* default realm of acl (optional) */
{
  long		rtnVal = DACL_SUCCESS;
  int		i;
  static char	routineName[] = "dacl_InitEpiAcl";
  
  if (theAclP != (dacl_t *)NULL) {
    theAclP->mgr_type_tag = episodeAclMgmtUuid;

    /* 
     * If aclRealmP is non-null use that as default realm of acl, else
     * use the local cellID in kernel space or all zeroes in user space
     * as before
     */
    if ( aclRealmP != (epi_uuid_t *)NULL ) {
	theAclP->default_realm = *aclRealmP;
    } else {
#if defined(KERNEL)
	theAclP->default_realm = dacl_localCellID;
#else /* defined(KERNEL) */
	bzero((char *)(&(theAclP->default_realm)), sizeof(epi_uuid_t));
#endif /* defined(KERNEL) */
    }
    
    /* init the simple entry array */
    for (i = 0; i <= (int)dacl_simple_entry_type_unauthmask; i++) {
      theAclP->simple_entries[i].is_entry_good = 0;
    }
    
    /* init the complex entry lists */
    for (i = 0; i <= (int)dacl_complex_entry_type_other; i++) {
      theAclP->complex_entries[i].num_entries = 0;
      theAclP->complex_entries[i].entries_allocated = 0;
      theAclP->complex_entries[i].complex_entries = (dacl_entry_t *)NULL;
    }
    
    /* make sure the required simple entries are there */
    theAclP->simple_entries[(int)dacl_simple_entry_type_userobj].is_entry_good = 1;
    theAclP->simple_entries[(int)dacl_simple_entry_type_userobj].perms = dacl_perm_control;
    
    theAclP->simple_entries[(int)dacl_simple_entry_type_groupobj].is_entry_good = 1;
    theAclP->simple_entries[(int)dacl_simple_entry_type_groupobj].perms = 0;
    
    theAclP->simple_entries[(int)dacl_simple_entry_type_otherobj].is_entry_good = 1;
    theAclP->simple_entries[(int)dacl_simple_entry_type_otherobj].perms = 0;
    
    theAclP->num_entries = 3;

    if (modeP != (u_int32 *)NULL) {
      /* if the caller gave us anything to put into the obj entries */
      rtnVal = dacl_ChmodAcl(theAclP, *modeP, forDirectory);
    }
  }
  else {
    dacl_LogParamError(routineName, "theAclP", 1, __FILE__, __LINE__);
  }

  return rtnVal;
}

/*
 * Find default realm of acl
 */
EXPORT void dacl_GetDefaultRealm(theAclP, realmP)
     dacl_t *          theAclP;
     epi_uuid_t *      realmP;  /* Space for this should be allocated by caller */
{
    *realmP = theAclP->default_realm;
}
