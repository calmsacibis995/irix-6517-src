/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1996 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: sec_pkl_krpc.h,v $
 * Revision 65.1  1997/10/20 19:16:29  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.41.2  1996/02/18  23:46:51  marty
 * 	Update OSF copyright years
 * 	[1996/02/18  22:45:29  marty]
 *
 * Revision 1.1.41.1  1995/12/08  00:15:33  root
 * 	Submit OSF/DCE 1.2.1
 * 	[1995/12/07  23:56:34  root]
 * 
 * Revision 1.1.39.2  1994/02/02  20:55:43  cbrooks
 * 	Change argument types via cast/declaration
 * 	[1994/02/02  20:47:57  cbrooks]
 * 
 * Revision 1.1.39.1  1994/01/21  22:32:34  cbrooks
 * 	RPC Code Cleanup - Initial Submission
 * 	[1994/01/21  20:57:41  cbrooks]
 * 
 * Revision 1.1.2.3  1993/01/03  22:37:00  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  19:53:42  bbelch]
 * 
 * Revision 1.1.2.2  1992/12/23  19:40:32  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  14:54:28  zeliff]
 * 
 * Revision 1.1  1992/01/19  03:16:22  devrcs
 * 	Initial revision
 * 
 * $EndLog$
 */
/*
**  Copyright (c) 1991 by
**      Hewlett-Packard Company, Palo Alto, Ca.
**
**
**  NAME:
**
**      sec_pkl_krpc.h
**
**  FACILITY:
**
**      DCE kernel RPC
**
**  ABSTRACT:
**
**      Pickling routine prototypes for one sec_ type.
**
**
*/
#ifndef _SEC_PKL_KRPC_H
#define _SEC_PKL_KRPC_H 1 

#include <dce/dce.h>

#include <dce/pklbase.h>
#include <dce/rpcbase.h>
#include <dce/id_base.h>
#include <stddef.h>

/*
 * type IDs for the two pickled types
 */

extern uuid_t  sec_id_pac_t_typeid;

/*
 * pickling routine for sec_id_pac_t
 */
void sec_id_pac_t_pickle _DCE_PROTOTYPE_ ((
    sec_id_pac_t *,
    rpc_syntax_id_t *,
    void *(*)(size_t),
    unsigned32 ,
    idl_pkl_t **,
    unsigned32 *,
    error_status_t *
    ));

/*
 * unpickling routine for sec_id_pac_t
 */
void sec_id_pac_t_unpickle _DCE_PROTOTYPE_ ((
    idl_pkl_t *,
    void *(*)(size_t),
    sec_id_pac_t *,
    error_status_t *
    ));


#endif /*   _SEC_PKL_KRPC_H */
