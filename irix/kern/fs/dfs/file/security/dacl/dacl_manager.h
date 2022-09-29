/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/*
 * HISTORY
 * $Log: dacl_manager.h,v $
 * Revision 65.1  1997/10/20 19:20:16  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.11.1  1996/10/02  18:15:43  damon
 * 	Newest DFS from Transarc
 * 	[1996/10/01  18:46:39  damon]
 *
 * $EndLog$
 */
/*
 *	dacl_manager.h -- code to implement the generic ACL manager
 *
 * Copyright (C) 1994, 1991 Transarc Corporation
 * All rights reserved.
 */

/* $Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/security/dacl/RCS/dacl_manager.h,v 65.1 1997/10/20 19:20:16 jdoak Exp $ */

#ifndef TRANSARC_DACL_MANAGER_H
#define TRANSARC_DACL_MANAGER_H 1

#include <dcedfs/param.h>
#include <dcedfs/stds.h>

#include <epi_id.h>
#include <dacl.h>

typedef long (* dacl_readwrite_fn_P) _TAKES((
					     dacl_t *		aclP,
					     char * 		rockP,
					     epi_uuid_t * 	mgrUuidP
					     ));

typedef long (* dacl_accesscheck_fn_P) _TAKES((
					       dacl_t *			aclP,
					       u_int32 *		permBitsP,
					       dacl_permset_t *		accessRequestedP,
					       epi_uuid_t *		userIdP,
					       epi_uuid_t *		groupIdP,
					       epi_uuid_t *		realmIdP,
					       epi_principal_id_t *	userObjP,
					       epi_principal_id_t *	groupObjP,
					       int			isAuthn
					     ));


typedef struct dacl_manager {
  epi_uuid_t		managerUuid;

  /* the functions that depend on the ACL manager used */
  dacl_readwrite_fn_P	readFunction;
  dacl_readwrite_fn_P	writeFunction;
  dacl_accesscheck_fn_P	accessFunction;
} dacl_manager_t;

#define dacl_mgr_READ_ACL(mgrP, aclP, argP)	\
  (mgrP)->readFunction((aclP), (argP), &((mgrP)->managerUuid))
#define dacl_mgr_WRITE_ACL(mgrP, aclP, argP)	\
  (mgrP)->writeFunction((aclP), (argP), &((mgrP)->managerUuid))
#define dacl_mgr_ACCESS_CHECK(mgrP, aclP, permBits, accessReq, userIdP, groupIdP, realmIdP, userObjP, groupObjP, isAuthn)	\
  (mgrP)->accessFunction((aclP), (permBits), (accessReq), (userIdP), (groupIdP), (realmIdP), (userObjP), (groupObjP), (isAuthn))

#endif /* TRANSARC_DACL_MANAGER_H */
