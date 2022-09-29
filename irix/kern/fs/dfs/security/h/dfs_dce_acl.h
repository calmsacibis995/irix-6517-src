/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1996 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: dfs_dce_acl.h,v $
 * Revision 65.1  1997/10/20 19:46:10  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.8.2  1996/02/18  22:58:03  marty
 * 	Update OSF copyright years
 * 	[1996/02/18  22:16:52  marty]
 *
 * Revision 1.1.8.1  1995/12/13  16:24:33  root
 * 	Submit
 * 	[1995/12/11  15:14:17  root]
 * 
 * Revision 1.1.6.2  1994/09/22  20:49:47  rsarbo
 * 	pull in pac_list_t from fshs_deleg.h if DCE_DFS_PRESENT
 * 	[1994/09/22  20:08:28  rsarbo]
 * 
 * Revision 1.1.6.1  1994/09/21  15:45:03  rsarbo
 * 	prototype for dfs_dce_acl_FreePacList
 * 	[1994/09/20  20:04:20  rsarbo]
 * 
 * Revision 1.1.4.2  1992/12/29  13:06:30  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/28  20:43:18  zeliff]
 * 
 * Revision 1.1.2.3  1992/07/09  19:47:12  burati
 * 	CR4161 Add prototype for dfs_dce_acl_CheckAccessAllowedPac
 * 	[1992/07/08  19:43:05  burati]
 * 
 * Revision 1.1.2.2  1992/05/08  14:58:35  burati
 * 	Add dfs_dce_acl_get_mgr_types_semantics
 * 	[1992/04/29  03:06:27  burati]
 * 
 * Revision 1.1  1992/01/19  14:43:00  devrcs
 * 	Initial revision
 * 
 * $EndLog$
 */

/*  dfs_dce_acl.h V=2 08/28/91 //littl/prgy/src/h
**
** Copyright (c) Hewlett-Packard Company 1991
** Unpublished work. All Rights Reserved.
**
*/
/*  DFS/DCE acl lookup/replace routines - header file
**
**  The lookup and replace routines here provide the interface between
**  DFS acls and DCE sec_acls.  They have been implemented here to keep
**  the sec_acl client agent from having to have knowledge about non-DCE
**  ACL constructs.  The lookup routine is called by the sec_acl client
**  agent bind routine (sec_acl_bind), before any remote operations are
**  attempted, to determine if the specified object is governed by an DFS
**  ACL.  If that lookup returns sec_acl_object_not_found, then the client
**  agent will assume that the pathname refers to an object in another DCE
**  component, that exports a DCE ACL manager.  Otherwise, all subsequent
**  operations will use the dfs_{lookup,replace}_dce_acl calls.
**
** History:
**  07/29/91    burati  Created.
*/

#include <dce/nbase.h>
#include <dce/aclbase.h>
#ifdef DCE_DFS_PRESENT
#include <dcedfs/fshs_deleg.h>	/* pac_list_t declaration */
#endif

/*
** Replace DFS acl
*/
error_status_t dfs_dce_acl_lookup(
#ifdef __STDC__
    char            *filenameP,
    sec_acl_type_t  sec_acl_type,
    sec_acl_t       *sec_aclP
#endif
);


/*
** Replace DFS acl
*/
error_status_t dfs_dce_acl_replace(
#ifdef __STDC__
    char            *filenameP,
    sec_acl_type_t  sec_acl_type,
    sec_acl_t       *sec_aclP
#endif
);


/*
 * Determine types of acls protecting dfs objects.  This is here to provide
 * some degree of compatibility with the DCE acl interfaces, so that DCE ACL
 * clients may manipulate DFS acls in a manner similar to that of DCE ACLs.
 */
void dfs_dce_acl_get_manager_types(
#ifdef __STDC__
    sec_acl_type_t          sec_acl_type,
    unsigned32              size_avail,
    unsigned32              *size_used,
    unsigned32              *num_types,
    uuid_t                  manager_types[],
    error_status_t          *status
#endif
);


/*
 * Determine types of acls protecting dfs objects.  This is here to provide
 * some degree of compatibility with the DCE acl interfaces, so that DCE ACL
 * clients may manipulate DFS acls in a manner similar to that of DCE ACLs.
 * Let the caller know we want POSIX mask_obj semantics
 */
void dfs_dce_acl_get_mgr_types_semantics(
#ifdef __STDC__
    sec_acl_type_t              sec_acl_type,
    unsigned32                  size_avail,
    unsigned32                  *size_used,
    unsigned32                  *num_types,
    uuid_t                      manager_types[],
    sec_acl_posix_semantics_t   posix_semantics[],
    error_status_t              *status
#endif
);


/*
 * Determine printstrings of acls protecting dfs objects.  This is here to
 * provide some degree of compatibility with the DCE acl interfaces, so that DCE
 * ACL clients may manipulate DFS acls in a manner similar to that of DCE ACLs.
 */
void dfs_dce_acl_get_printstring(
#ifdef __STDC__
    uuid_t                  *manager_type,   
    unsigned32              size_avail,
    uuid_t                  *manager_type_chain,
    sec_acl_printstring_t   *manager_info,
    boolean32               *tokenize,
    unsigned32              *total_num_printstrings,
    unsigned32              *size_used,
    sec_acl_printstring_t   printstrings[],
    error_status_t          *status
#endif
);


/*
 * Determine whether or not "desiredPermsetP" is granted by the acl.  desiredPermsetP
 * may be NULL, in which case, the allowed access set is returned anyway. entryNameP
 * is the name of the DFS file to which the acl corresponds.  This parameter is required
 * because we have to stat the file in order to obtain the correct values to use for the
 * the user_obj and group_obj in the access check.
 */
boolean32 dfs_dce_acl_CheckAccessAllowedPac (
#ifdef __STDC__
   char *		entryNameP,
   sec_acl_t *		secAclP,
   sec_acl_permset_t *	desiredPermsetP,
   sec_acl_permset_t *	accessAllowedP,
   error_status_t *	statusP
#endif	/* __STDC__ */
);

#ifdef DCE_DFS_PRESENT
void dfs_dce_acl_FreePacList (
#ifdef __STDC__
   pac_list_t **	pacListPP
#endif	/* __STDC__ */
);
#endif
