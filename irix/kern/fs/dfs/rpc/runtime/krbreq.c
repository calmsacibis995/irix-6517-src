/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(_KERNEL)
static const char *_identString = "$Id: krbreq.c,v 65.4 1998/03/23 17:27:58 gwehrman Exp $";
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
 * $Log: krbreq.c,v $
 * Revision 65.4  1998/03/23 17:27:58  gwehrman
 * Source Identification changed to bracket with SGIMIPS && !_KERNEL.
 *
 * Revision 65.3  1998/01/07 17:21:02  jdoak
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS
 *
 * Revision 65.2  1997/11/06  19:57:02  jdoak
 * Source Identification added.
 *
 * Revision 65.1  1997/10/20  19:17:03  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.306.1  1996/06/04  21:55:56  arvind
 * 	merge kernel fix  from  mb_u2u
 * 	[1996/05/13  20:40 UTC  burati  /main/DCE_1.2/2]
 *
 * 	Don't register u2u if kernel (can't happen, so ifdef it out)
 * 	[1996/05/13  20:33 UTC  burati  /main/mb_u2u/2]
 *
 * 	u2u: Merge addition of rpc__krb_srv_reg_auth_u2u() and
 * 	rpc__krb_inq_my_princ_name_tgt()
 * 	[1996/05/06  20:30 UTC  burati  /main/DCE_1.2/1]
 *
 * 	fix the fix
 * 	[1996/05/13  20:55 UTC  burati  /main/mb_u2u/3]
 *
 * 	Don't register u2u if kernel (can't happen, so ifdef it out)
 * 	[1996/05/13  20:33 UTC  burati  /main/mb_u2u/2]
 *
 * 	merge  u2u work
 * 	[1996/04/29  21:08 UTC  burati  /main/mb_u2u/1]
 *
 * 	u2u work off Mothra, before 1.2 branch is available
 * 	[1996/04/29  20:26 UTC  burati  /main/mb_mothra8/1]
 *
 * Revision 1.1.304.2  1996/02/18  00:04:38  marty
 * 	Update OSF copyright years
 * 	[1996/02/17  22:56:50  marty]
 * 
 * Revision 1.1.304.1  1995/12/08  00:20:57  root
 * 	Submit OSF/DCE 1.2.1
 * 	[1995/12/07  23:59:43  root]
 * 
 * Revision 1.1.302.1  1994/01/21  22:38:03  cbrooks
 * 	RPC Code Cleanyp - Initial Submission
 * 	[1994/01/21  21:58:20  cbrooks]
 * 
 * Revision 1.1.2.3  1993/01/03  23:26:15  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  20:08:29  bbelch]
 * 
 * Revision 1.1.2.2  1992/12/23  20:52:23  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  15:40:15  zeliff]
 * 
 * Revision 1.1  1992/01/19  03:06:35  devrcs
 * 	Initial revision
 * 
 * $EndLog$
 */
/*
**  Copyright (c) 1989, 1996 by
**      Hewlett-Packard Company, Palo Alto, Ca. & 
**      Digital Equipment Corporation, Maynard, Mass.
**
**
**  NAME
**
**      krbreq.c
**
**  FACILITY:
**
**      Remote Procedure Call (RPC) 
**
**  ABSTRACT:
**
**  Definitions of types/constants internal to RPC facility and common
**  to all RPC components.
**
**
*/

#include  <krbp.h>

void rpc__krb_srv_reg_auth 
#ifdef _DCE_PROTO_
(
        unsigned_char_p_t server_name,
        rpc_auth_key_retrieval_fn_t get_key_func,
        pointer_t arg,
        unsigned32 *st
)
#else
(server_name, get_key_func, arg, st)
    unsigned_char_p_t server_name;
    rpc_auth_key_retrieval_fn_t get_key_func;
    pointer_t arg;
    unsigned32 *st;
#endif
{
    *st = sec_krb_register_server (server_name, get_key_func, arg);
}

void rpc__krb_srv_reg_auth_u2u
(
    unsigned_char_p_t server_name,
    rpc_auth_identity_handle_t id_h,
    unsigned32 *st
)
{
#ifndef _KERNEL
    *st = sec_krb_u2u_register_server (server_name, id_h);
#else
    *st = rpc_s_not_in_kernel;
#endif
}

void rpc__krb_inq_my_princ_name(name_size, name, st)
    unsigned32 name_size;
    unsigned_char_p_t name;
    unsigned32 *st;
{
    unsigned_char_p_t aname;
    error_status_t error;

    if (name_size > 0) {
        rpc__strncpy(name, (unsigned char *)"", name_size - 1);
    }
    error = sec_krb_get_server (&aname);
    if ((error == rpc_s_ok) && (name_size > 0))
    {
        rpc__strncpy(name, aname, name_size - 1);
    }
    *st = error;
}

void rpc__krb_inq_my_princ_name_tgt(
    unsigned32          name_size,              /* [IN] Max princ name size */
    unsigned32          max_tgt_size,           /* [IN] Max TGT data size   */
    unsigned_char_p_t   name,                   /* [OUT] Principal name     */
    unsigned32          *tgt_length,            /* [OUT] TGT data size      */
    unsigned_char_p_t   tgt_data,               /* [OUT] TGT data           */
    unsigned32          *st                     /* [OUT] status             */
)
{
#ifndef _KERNEL
    unsigned_char_p_t aname, tmp_data;
    error_status_t error;

    if (name_size > 0) {
        rpc__strncpy(name, (unsigned char *)"", name_size - 1);
    }
    error = sec_krb_get_server_name_tgt (&aname, tgt_length, &tmp_data);
    if (error == rpc_s_ok) {
        if (name_size > 0) {
            rpc__strncpy(name, aname, name_size - 1);
        }
        if (*tgt_length > max_tgt_size) {
            error = rpc_s_credentials_too_large;
	} else {
            memcpy(tgt_data, tmp_data, *tgt_length);
        }
    }
    *st = error;
#else
    *st = rpc_s_not_in_kernel;
#endif /* _KERNEL */
}
