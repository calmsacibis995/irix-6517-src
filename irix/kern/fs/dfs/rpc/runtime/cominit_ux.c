/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(_KERNEL)
static const char *_identString = "$Id: cominit_ux.c,v 65.4 1998/03/23 17:27:47 gwehrman Exp $";
static void _identFunc(const char *x) {if(!x)_identFunc(_identString);}
#endif



/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1996 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: cominit_ux.c,v $
 * Revision 65.4  1998/03/23 17:27:47  gwehrman
 * Source Identification changed to bracket with SGIMIPS && !_KERNEL.
 *
 * Revision 65.3  1998/01/07 17:20:59  jdoak
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS
 *
 * Revision 65.2  1997/11/06  19:57:00  jdoak
 * Source Identification added.
 *
 * Revision 65.1  1997/10/20  19:16:47  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.39.2  1996/02/18  00:02:38  marty
 * 	Update OSF copyright years
 * 	[1996/02/17  22:55:18  marty]
 *
 * Revision 1.1.39.1  1995/12/08  00:18:20  root
 * 	Submit OSF/DCE 1.2.1
 * 	[1995/12/07  23:58:11  root]
 * 
 * Revision 1.1.37.1  1994/01/21  22:35:26  cbrooks
 * 	RPC Code Cleanyp - Initial Submission
 * 	[1994/01/21  21:55:12  cbrooks]
 * 
 * Revision 1.1.2.3  1993/01/03  23:22:12  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  20:01:58  bbelch]
 * 
 * Revision 1.1.2.2  1992/12/23  20:43:57  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  15:33:41  zeliff]
 * 
 * Revision 1.1  1992/01/19  03:06:06  devrcs
 * 	Initial revision
 * 
 * $EndLog$
 */
/*
**  Copyright (c) 1989 by
**      Hewlett-Packard Company, Palo Alto, Ca. & 
**      Digital Equipment Corporation, Maynard, Mass.
**
**
**  NAME
**
**      cominit_ux.c
**
**  FACILITY:
**
**      Remote Procedure Call (RPC) 
**
**  ABSTRACT:
**
**  Initialization Service support routines for Unix platforms.
**
*/

#include <commonp.h>
#include <com.h>
#include <comprot.h>
#include <comnaf.h>
#include <comp.h>
#include <cominitp.h>


/* 
 * Routines for loading Network Families and Protocol Families as shared
 * images.  i.e., VMS
 */

PRIVATE rpc_naf_init_fn_t  rpc__load_naf
#ifdef _DCE_PROTO_
(
  rpc_naf_id_elt_p_t      naf,
  unsigned32              *status
)
#else
(naf, status)
rpc_naf_id_elt_p_t      naf;
unsigned32              *status;
#endif
{
    *status = rpc_s_ok;
    return((rpc_naf_init_fn_t)NULL);
}


PRIVATE rpc_prot_init_fn_t  rpc__load_prot
#ifdef _DCE_PROTO_
(
    rpc_protocol_id_elt_p_t prot,
    unsigned32              *status
)
#else
(prot, status)
rpc_protocol_id_elt_p_t prot;
unsigned32              *status;
#endif
{
    *status = rpc_s_ok;
    return((rpc_prot_init_fn_t)NULL);
}

PRIVATE rpc_auth_init_fn_t  rpc__load_auth
#ifdef _DCE_PROTO_
(
    rpc_authn_protocol_id_elt_p_t auth,
    unsigned32              *status
)
#else
(auth, status)
rpc_authn_protocol_id_elt_p_t auth;
unsigned32              *status;
#endif
{
    *status = rpc_s_ok;
    return((rpc_auth_init_fn_t)NULL);
}
