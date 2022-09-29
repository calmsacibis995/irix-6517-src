/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/*
 * Copyright (c) 1995 Transarc Corporation
 * All Rights Reserved
 */
/*
 * HISTORY
 * $Log: compat.h,v $
 * Revision 65.3  1999/07/21 19:01:14  gwehrman
 * Defined SGIMIPS for this development header.  Fix for bug 657352.
 *
 * Revision 65.2  1997/12/16 17:05:37  lmc
 *  1.2.2 initial merge of file directory
 *
 * Revision 65.1  1997/10/20  19:20:36  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.70.1  1996/10/02  17:54:12  damon
 * 	Newest DFS from Transarc
 * 	[1996/10/01  18:42:02  damon]
 *
 * $EndLog$
*/
#ifndef __COMPAT_H_INCL_ENV_
#define __COMPAT_H_INCL_ENV_ 1
#ifndef SGIMIPS
#define SGIMIPS
#endif /* !SGIMIPS */

#include <dcedfs/param.h>
#include <dcedfs/stds.h>
#include <dcedfs/common_data.h>
#include <dcedfs/sysincludes.h>
#include <dcedfs/osi_net.h>

#include <dce/rpc.h>
#include <dcedfs/osi_net_mach.h>

/*
 * from compat_cds.c:
 */
/* main server-side init routine */
extern void dfs_register_rpc_server _TAKES((
					    rpc_if_handle_t	if_handle,
					    rpc_mgr_epv_t	manager_epv,
					    uuid_t *		obj_uuidP,
					    uuid_t *		type_uuidP,
					    int			maxcalls,
					    char *		adminPathP,
					    char *		annotationP,
					    error_status_t *	st
					  ));

/*
 * The following routine is called only from the fts client, to register a
 * server for token revocation callbacks.
 */
extern void dfs_register_rpc_server_noauth _TAKES((
					    rpc_if_handle_t	if_handle,
					    rpc_mgr_epv_t	manager_epv,
					    uuid_t *		obj_uuidP,
					    uuid_t *		type_uuidP,
 					    int			maxcalls,
					    char *		annotationP,
					    error_status_t *	st
					    ));

/* client-side init routine */
extern void dfs_locate_rpc_server _TAKES((
					  unsigned_char_t *	entry_nameP,
					  rpc_binding_handle_t *bindingP,
					  uuid_t *		obj_uuidP,
					  int 			localAuthFlag,
					  int			noAuthFlag,
					  error_status_t *	st
					));

extern void compat_get_group_object_uuid _TAKES((
					unsigned_char_t *	groupNameP,
					uuid_t *		object_uuidP,
				        error_status_t	*	stp
					      ));
extern void compat_filter_binding_vector _TAKES((
				rpc_binding_vector_t *binding_vectorP,
				error_status_t	*	stp));

extern int compat_binding_vector_size _TAKES((
				       rpc_binding_vector_t *	binding_vectorP,
				       long *			size
				     ));

extern char * compat_print_hostname_from_binding _TAKES((
			   	       rpc_binding_handle_t binding
				     ));
/* debugging aids */
extern void compat_print_binding _TAKES((
				  char *		infoStP,
				  rpc_binding_handle_t	binding
				));
extern void compat_print_binding_vector _TAKES((rpc_binding_vector_t *bindingVectorP));
extern int compat_print_uuid _TAKES((uuid_t uuid));

/*
 * from compat_osi.c:
 */
extern void afsos_panic _TAKES((char *a));

/*
 * from compat_rpcbind.c:
 */
extern long rpcx_ipaddr_from_sockaddr(
    struct sockaddr_in *	asockaddrp,
    char *			aipaddrp
);
extern long rpcx_sockaddr_from_ipaddr(
    char *			aipaddrp,
    struct sockaddr_in *	asockaddrp
);
extern long rpcx_binding_to_sockaddr(
    rpc_binding_handle_t	ahandle,
    struct sockaddr_in *	asockaddrp
);
extern long rpcx_binding_from_sockaddr(
    struct sockaddr *	asockaddrp,
    rpc_binding_handle_t *	ahandlep
);

/* from compat_err.c: */
extern char * dfs_dceErrTxt(unsigned long st);

extern void dfs_copyDceErrTxt _TAKES((
				       unsigned long st,
				       char *txtbuf,
				       int bufsize
				       ));

#define NCSCOMPAT_ERR_BUF_SIZE  256

/* from compat_rpcVers.c */
#include <dcedfs/compat_rpcVers.h>

/*
 * pick up the error codes
 */
#include <dcedfs/compat_errs.h>

/*
 * from compat_dfsnames.c:
 */
#include <dcedfs/compat_dfsnames.h>

/*
 * from compat_serverdup.c:
 */
extern long compat_ShutdownDuplicateServer _TAKES((
				   rpc_if_handle_t	if_handle,
				   uuid_t *	serverInstanceUuidP,
				   int		retainLocalAuthContext
						 ));
extern long compat_UnregisterServer _TAKES((
				    rpc_if_handle_t	if_handle,
				    uuid_t *		serverInstanceUuidP
					  ));

extern int compat_DetectDuplicateServer _TAKES((
			       rpc_if_handle_t	if_handle,
			       uuid_t *	serverInstanceUuidP,
			       rpc_binding_handle_t *returnBinding,
			       error_status_t *errorStatus
				     ));

/*
 * from compat_junct.c:
 */
extern void dfs_GetJunctionName _TAKES((
					 char *,
					 int,
					 char *,
					 int,
#ifdef SGIMIPS
                                         error_status_t *
#else
					 unsigned long *
#endif /* SGIMIPS */
					 ));

/*
 * from compat_authn.c:
 */
extern char *compat_authnToString _TAKES((unsigned32, char *, int));
extern int compat_nameToAuthnLevel _TAKES((char *, unsigned32 *));

/*
 * protection against malicious cancels
 */
#include <dcedfs/compat_handleCancel.h>

#endif /* __COMPAT_H_INCL_ENV_ */
