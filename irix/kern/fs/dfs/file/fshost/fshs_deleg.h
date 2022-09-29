/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 * 
 */
/*
 * HISTORY
 * $Log: fshs_deleg.h,v $
 * Revision 65.2  1998/04/01 14:15:53  gwehrman
 * 	Cleanup of errors turned on by -diag_error flags in kernel cflags.
 * 	Created prototypes for exported functions.
 *
 * Revision 65.1  1997/10/20  19:19:37  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.7.1  1996/10/02  17:46:33  damon
 * 	Newest DFS from Transarc
 * 	[1996/10/01  18:37:21  damon]
 *
 * Revision 1.1.2.2  1994/10/06  20:26:17  agd
 * 	expand copyright
 * 	[1994/10/06  14:11:57  agd]
 * 
 * 	expand copyright
 * 
 * Revision 1.1.2.1  1994/08/23  18:58:37  rsarbo
 * 	moved from file/security/dacl/dfsdlg_utils.h to allow cleaner
 * 	RIOS kernel extension builds
 * 	[1994/08/23  16:51:17  rsarbo]
 * 
 * $EndLog$
 */

/*
 *      Copyright (C) 1995 Transarc Corporation
 *      All rights reserved.
 */

#ifndef _FSHS_DELEG_H_
#define _FSHS_DELEG_H_
#include <dce/id_epac.h>
#include <dce/id_base.h>
#include <dce/rpcbase.h>

typedef struct pac_list {
    struct pac_list *next;
    sec_id_pa_t *pacp;
    int authenticated;
    struct pac_list *lastDelegate;   /* Ptr to last delegate */
  } pac_list_t;

#ifdef SGIMIPS
/*
 * Exported by fshs_deleg.c
 */
extern void fshs_SetupDelegationChain(
	rpc_authz_cred_handle_t handle,
	unsigned isAuthn,
	sec_id_pa_t *unauthPAp,
	pac_list_t **head,
	error_status_t *st);
extern int fshs_FreeDelegationChain(pac_list_t **head);
#endif
#endif /* _FSHS_DELEG_H_ */

