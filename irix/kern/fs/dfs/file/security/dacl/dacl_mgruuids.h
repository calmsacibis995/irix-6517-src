/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/*
 * HISTORY
 * $Log: dacl_mgruuids.h,v $
 * Revision 65.1  1997/10/20 19:20:16  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.543.1  1996/10/02  18:15:46  damon
 * 	Newest DFS from Transarc
 * 	[1996/10/01  18:46:41  damon]
 *
 * $EndLog$
 */
/*
*/
/*
 * dacl_mgruuids.h -- prototypes for the routines that keep track of properties
 * of relevant ACL managers
 *
 * Copyright (C) 1994, 1991 Transarc Corporation
 * All rights reserved.
 */

/* $Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/security/dacl/RCS/dacl_mgruuids.h,v 65.1 1997/10/20 19:20:16 jdoak Exp $ */

#ifndef TRANSARC_DACL_ACLMGRUUIDS_H
#define TRANSARC_DACL_ACLMGRUUIDS_H 1

#include <dcedfs/param.h>
#include <dcedfs/stds.h>

#include <dcedfs/epi_id.h>

extern epi_uuid_t episodeAclMgmtUuid;

extern epi_uuid_t bosserverAclMgmtUuid;

extern int dacl_AreObjectEntriesRequired(epi_uuid_t *mgrUuidP);

extern int dacl_AreObjectUuidsRequiredOnAccessCheck(epi_uuid_t *mgrUuidP);

extern int dacl_ArePermBitsRequiredOnAccessCheck(epi_uuid_t *mgrUuidP);

extern char *dacl_AclMgrName(epi_uuid_t *mgrUuidP);

#endif /* TRANSARC_DACL_ACLMGRUUIDS_H */
