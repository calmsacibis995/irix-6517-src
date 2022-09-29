/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(_KERNEL)
static const char *_identString = "$Id: pickling.c,v 65.4 1998/03/23 17:25:52 gwehrman Exp $";
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
 * $Log: pickling.c,v $
 * Revision 65.4  1998/03/23 17:25:52  gwehrman
 * Source Identification changed to bracket with SGIMIPS && !_KERNEL.
 *
 * Revision 65.3  1998/01/07 17:20:08  jdoak
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS
 *
 * Revision 65.2  1997/11/06  19:56:16  jdoak
 * Source Identification added.
 *
 * Revision 65.1  1997/10/20  19:15:41  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.14.1  1996/10/03  14:41:09  arvind
 * 	Make sure the RPC_SS_INIT_CLIENT call is done before doing
 * 	any (un-)pickling.
 * 	[1996/08/08  14:37 UTC  jrr  /main/jrr_122_3/1]
 *
 * Revision 1.1.12.2  1996/02/17  23:58:15  marty
 * 	Update OSF copyright years
 * 	[1996/02/17  22:52:39  marty]
 * 
 * Revision 1.1.12.1  1995/12/07  22:31:25  root
 * 	Submit OSF/DCE 1.2.1
 * 
 * 	HP revision /main/dat_xidl2/1  1995/11/17  17:08 UTC  dat
 * 	Merge second XIDL drop for DCE 1.2.1
 * 	[1995/12/07  21:16:21  root]
 * 
 * Revision 1.1.2.1  1995/10/23  01:49:41  bfc
 * 	oct 95 idl drop
 * 	[1995/10/22  23:36:25  bfc]
 * 
 * 	may 95 idl drop
 * 	[1995/10/22  22:58:09  bfc]
 * 
 * 	DCE for DEC OSF/1: populate from OSF DCE R1.1
 * 	[1995/10/21  18:26:04  bfc]
 * 
 * Revision 1.1.8.3  1994/08/09  17:32:29  burati
 * 	DFS/EPAC/KRPC/dfsbind changes
 * 	[1994/08/09  17:02:50  burati]
 * 
 * Revision 1.1.9.2  94/06/24  17:57:10  rsarbo
 * 	ifdef KERNEL for use by libksec in kernel
 * 
 * Revision 1.1.8.2  1994/05/11  22:03:46  tom
 * 	For no encoding check, skip interface version check too.
 * 	[1994/05/11  22:02:49  tom]
 * 
 * Revision 1.1.8.1  1994/05/10  18:03:21  tom
 * 	Add idl_es_set_attrs and idl_es_inq_attrs functions.
 * 	Add IDL_ES_NO_ENCODING_CHECK attribute.
 * 	[1994/05/10  17:34:59  tom]
 * 
 * Revision 1.1.4.1  1993/10/20  19:43:31  hinxman
 * 	OT 9190 - Memory leak in IDL Encoding Services
 * 	[1993/10/20  19:43:11  hinxman]
 * 
 * Revision 1.1.2.2  1993/07/07  20:10:26  ganni
 * 	reduced stub idl sources
 * 	[1993/07/07  19:39:06  ganni]
 * 
 * $EndLog$
 */
/*
**  Copyright (c) Digital Equipment Corporation, 1992, 1993
**  All Rights Reserved.  Unpublished rights reserved
**  under the copyright laws of the United States.
**  
**  The software contained on this media is proprietary
**  to and embodies the confidential technology of 
**  Digital Equipment Corporation.  Possession, use,
**  duplication or dissemination of the software and
**  media is authorized only pursuant to a valid written
**  license from Digital Equipment Corporation.
**
**  RESTRICTED RIGHTS LEGEND   Use, duplication, or 
**  disclosure by the U.S. Government is subject to
**  restrictions as set forth in Subparagraph (c)(1)(ii)
**  of DFARS 252.227-7013, or in FAR 52.227-19, as
**  applicable.
**
**  NAME:
**
**      pickling.c
**
**  FACILITY:
**
**      Interface Definition Language (IDL) Compiler
**
**  ABSTRACT:
**
**      Routines for IDL encoding services
**
*/

#include <dce/idlddefs.h>
#include <ndrmi.h>
#include <ndrui.h>
#include <dce/rpcsts.h>
#include <lsysdep.h>


static rpc_syntax_id_t ndr_transfer_syntax_id = {
    {0x8a885d04, 0x1ceb, 0x11c9, 0x9f, 0xe8, {0x8, 0x0, 0x2b, 0x10, 0x48, 
0x60}},
    2};

/******************************************************************************/
/*                                                                            */
/*  idl_es_add_state_to_handle - local routine                                */
/*  Allocate an IDL_ms_t block, initialize it, and attach it to an encoding   */
/*      handle                                                                */
/*  Also mark the state block's copy of the pickle header as invalid          */
/*  Returns error_status_ok or rpc_s_no_memory                                */
/*                                                                            */
/******************************************************************************/
static error_status_t idl_es_add_state_to_handle
#ifdef IDL_PROTOTYPES
(
    IDL_es_state_t *p_es_state
)
#else
(p_es_state)
    IDL_es_state_t *p_es_state;
#endif
{
    IDL_msp_t IDL_msp;

    RPC_SS_INIT_CLIENT;

    IDL_msp = (IDL_msp_t)malloc(sizeof(IDL_ms_t));
    if (IDL_msp == NULL)
        return(rpc_s_no_memory);
    /* Initialize state block, except stub must fill in type vector */
    rpc_ss_init_marsh_state(NULL, IDL_msp);
    IDL_msp->IDL_pickling_handle = (rpc_void_p_t)p_es_state;
    p_es_state->IDL_msp = IDL_msp;
    p_es_state->IDL_pickle_header.IDL_op_num = IDL_INVALID_OP_NUM;
    return(error_status_ok);
}

#ifndef KERNEL
/******************************************************************************/
/*                                                                            */
/*  idl_es_encode_incremental - API routine                                   */
/*  Return an encoding handle for incremental encoding                        */
/*                                                                            */
/******************************************************************************/
void idl_es_encode_incremental
#ifdef IDL_PROTOTYPES
(
    idl_void_p_t	    state,  /* [in] user state */
    idl_es_allocate_fn_t    alloc,  /* [in] alloc routine */
    idl_es_write_fn_t	    write,  /* [in] write routine */
    idl_es_handle_t	    *h,	    /* [out] encoding handle */
    error_status_t	    *st	    /* [out] status */
)
#else
(state, alloc, write, h, st)
    idl_void_p_t	    state;
    idl_es_allocate_fn_t    alloc;
    idl_es_write_fn_t	    write;
    idl_es_handle_t	    *h;
    error_status_t	    *st;
#endif
{
    IDL_es_state_t *p_es_state;

    p_es_state = (IDL_es_state_t *)malloc(sizeof(IDL_es_state_t));
    if (p_es_state == NULL)
    {
        *st = rpc_s_no_memory;
        return;
    }

    p_es_state->IDL_version = IDL_ES_STATE_VERSION;
    p_es_state->IDL_action = IDL_encoding_k;
    p_es_state->IDL_style = IDL_incremental_k;
    /* Set transfer syntax null to indicate "not yet determined" */
    uuid_create_nil(&(p_es_state->IDL_pickle_header.IDL_syntax_id.id), st);
    p_es_state->IDL_pickle_header.IDL_syntax_id.version = 0;
    p_es_state->IDL_state = state;
    p_es_state->IDL_alloc = alloc;
    p_es_state->IDL_write = write;
    p_es_state->IDL_es_flags = 0;

    *st = idl_es_add_state_to_handle(p_es_state);
    if (*st == error_status_ok)
        *h = (idl_es_handle_t)p_es_state;
    else
        free(p_es_state);
}

/******************************************************************************/
/*                                                                            */
/*  idl_es_encode_fixed_buffer - API routine                                  */
/*  Return an encoding handle for fixed buffer encoding                       */
/*                                                                            */
/******************************************************************************/
void idl_es_encode_fixed_buffer
#ifdef IDL_PROTOTYPES
(
    idl_byte		    *ep,    /* [out] buffer to   */
				    /* receive the encoding (must   */
				    /* be 8-byte aligned).	    */
    idl_ulong_int	    bsize,  /* [in] size of buffer provided */
    idl_ulong_int	    *esize, /* [out] size of the resulting  */
				    /* encoding	(set after	    */
				    /* execution of the stub)	    */
    idl_es_handle_t	    *h,	    /* [out] encoding handle */
    error_status_t	    *st	    /* [out] status */
)
#else
(ep, bsize, esize, h, st)
    idl_byte		    *ep;
    idl_ulong_int	    bsize;
    idl_ulong_int	    *esize;
    idl_es_handle_t	    *h;
    error_status_t	    *st;
#endif
{
    IDL_es_state_t *p_es_state;

    p_es_state = (IDL_es_state_t *)malloc(sizeof(IDL_es_state_t));
    if (p_es_state == NULL)
    {
        *st = rpc_s_no_memory;
        return;
    }

    p_es_state->IDL_version = IDL_ES_STATE_VERSION;
    p_es_state->IDL_action = IDL_encoding_k;
    p_es_state->IDL_style = IDL_fixed_k;
    /* Set transfer syntax null to indicate "not yet determined" */
    uuid_create_nil(&(p_es_state->IDL_pickle_header.IDL_syntax_id.id), st);
    p_es_state->IDL_pickle_header.IDL_syntax_id.version = 0;
    if (((ep - (idl_byte *)0) & 7) != 0)
    {
        /* User buffer is not 8-byte aligned */
        free(p_es_state);
        *st = rpc_s_ss_bad_buffer;
        return;
    }
    p_es_state->IDL_buff_addr = ep;
    if ((bsize & 7) != 0)
    {
        /* User buffer is not multiple of 8 bytes */
        free(p_es_state);
        *st = rpc_s_ss_bad_buffer;
        return;
    }
    p_es_state->IDL_bsize = bsize;
    p_es_state->IDL_esize = esize;
    p_es_state->IDL_es_flags = 0;

    *st = idl_es_add_state_to_handle(p_es_state);
    if (*st == error_status_ok)
    {
        *h = (idl_es_handle_t)p_es_state;
        p_es_state->IDL_msp->IDL_mp = ep;
        p_es_state->IDL_msp->IDL_buff_addr = ep;
        p_es_state->IDL_msp->IDL_data_addr = ep;
        p_es_state->IDL_msp->IDL_left_in_buff = bsize;
    }
    else
        free(p_es_state);
}
#endif /*KERNEL*/

/******************************************************************************/
/*                                                                            */
/*  idl_es_encode_dyn_buffer - API routine                                    */
/*  Return an encoding handle for dynamic buffer encoding                     */
/*                                                                            */
/******************************************************************************/
void idl_es_encode_dyn_buffer
#ifdef IDL_PROTOTYPES
(
    idl_byte		    **ep,   /* [out] pointer to receive the */
				    /* dynamically allocated buffer */
				    /* which contains the encoding  */
    idl_ulong_int	    *esize, /* [out] size of the resulting  */
				    /* encoding (set after	    */
				    /* execution of the stub)	    */
    idl_es_handle_t	    *h,	    /* [out] decoding handle */
    error_status_t	    *st	    /* [out] status */
)
#else
( ep, esize, h, st)
    idl_byte		    **ep;
    idl_ulong_int	    *esize;
    idl_es_handle_t	    *h;
    error_status_t	    *st;
#endif
{
    IDL_es_state_t *p_es_state;

    p_es_state = (IDL_es_state_t *)malloc(sizeof(IDL_es_state_t));
    if (p_es_state == NULL)
    {
        *st = rpc_s_no_memory;
        return;
    }

    p_es_state->IDL_version = IDL_ES_STATE_VERSION;
    p_es_state->IDL_action = IDL_encoding_k;
    p_es_state->IDL_style = IDL_dynamic_k;
    /* Set transfer syntax null to indicate "not yet determined" */
    uuid_create_nil(&(p_es_state->IDL_pickle_header.IDL_syntax_id.id), st);
    p_es_state->IDL_pickle_header.IDL_syntax_id.version = 0;
    p_es_state->IDL_p_buff_addr = ep;
    *ep = NULL;     /* Marker to indicate that no encoding call has yet been
                        made using this handle */
    p_es_state->IDL_esize = esize;
    p_es_state->IDL_dyn_buff_chain_head = NULL;
    p_es_state->IDL_dyn_buff_chain_tail = NULL;
    p_es_state->IDL_es_flags = 0;

    *st = idl_es_add_state_to_handle(p_es_state);
    if (*st == error_status_ok)
        *h = (idl_es_handle_t)p_es_state;
    else
        free(p_es_state);
}

#ifndef KERNEL
/******************************************************************************/
/*                                                                            */
/*  idl_es_decode_incremental - API routine                                   */
/*  Return an encoding handle for incremental decoding                        */
/*                                                                            */
/******************************************************************************/
void idl_es_decode_incremental
#ifdef IDL_PROTOTYPES
(
    idl_void_p_t	    state,  /* [in] user state */
    idl_es_read_fn_t	    read,   /* [in] routine to supply buffers */
    idl_es_handle_t	    *h,	    /* [out] decoding handle */
    error_status_t	    *st	    /* [out] status */
)
#else
(state, read, h, st)
    idl_void_p_t	    state;
    idl_es_read_fn_t	    read;
    idl_es_handle_t	    *h;
    error_status_t	    *st;
#endif
{
    IDL_es_state_t *p_es_state;
    IDL_msp_t IDL_msp;

    p_es_state = (IDL_es_state_t *)malloc(sizeof(IDL_es_state_t));
    if (p_es_state == NULL)
    {
        *st = rpc_s_no_memory;
        return;
    }

    p_es_state->IDL_version = IDL_ES_STATE_VERSION;
    p_es_state->IDL_action = IDL_decoding_k;
    p_es_state->IDL_style = IDL_incremental_k;
    p_es_state->IDL_state = state;
    p_es_state->IDL_read = read;
    p_es_state->IDL_pickle_header_read = idl_false;
    p_es_state->IDL_es_flags = 0;

    *st = idl_es_add_state_to_handle(p_es_state);
    if (*st == error_status_ok)
    {
        *h = (idl_es_handle_t)p_es_state;
        IDL_msp = p_es_state->IDL_msp;
        IDL_msp->IDL_left_in_buff = 0;
    }
    else
        free(p_es_state);
}
#endif/*KERNEL*/

/******************************************************************************/
/*                                                                            */
/*  idl_es_decode_buffer - API routine                                        */
/*  Return an encoding handle for fixed decoding                              */
/*                                                                            */
/******************************************************************************/
void idl_es_decode_buffer
#ifdef IDL_PROTOTYPES
(
    idl_byte		    *ep,    /* [in] pointer to buffer	    */
				    /* containing the encoding	    */
    idl_ulong_int	    size,   /* [in] size of buffer provided */
    idl_es_handle_t	    *h,	    /* [out] decoding handle */
    error_status_t	    *st	    /* [out] status */
)
#else
(ep, size, h, st)
    idl_byte		    *ep;
    idl_ulong_int	    size;
    idl_es_handle_t	    *h;
    error_status_t	    *st;
#endif
{
    IDL_es_state_t *p_es_state;
    IDL_msp_t IDL_msp;

    p_es_state = (IDL_es_state_t *)malloc(sizeof(IDL_es_state_t));
    if (p_es_state == NULL)
    {
        *st = rpc_s_no_memory;
        return;
    }

    p_es_state->IDL_version = IDL_ES_STATE_VERSION;
    p_es_state->IDL_action = IDL_decoding_k;
    p_es_state->IDL_style = IDL_fixed_k;
    p_es_state->IDL_bsize = size;
    p_es_state->IDL_buff_addr = ep;
    p_es_state->IDL_pickle_header_read = idl_false;
    p_es_state->IDL_es_flags = 0;

    *st = idl_es_add_state_to_handle(p_es_state);
    if (*st == error_status_ok)
        *h = (idl_es_handle_t)p_es_state;
    else
    {
        free(p_es_state);
        return;
    }

    /* Set up buffer management state */
    IDL_msp = p_es_state->IDL_msp;
    if (((p_es_state->IDL_buff_addr - (idl_byte *)0) & 7) != 0)
    {
        /* User buffer is not 8-byte aligned. Copy data to an area
                    that is */
        p_es_state->IDL_align_buff_addr = (idl_byte *)
                                            malloc(p_es_state->IDL_bsize + 7);
        if (p_es_state->IDL_align_buff_addr == NULL)
        {
            free(p_es_state);
            *st = rpc_s_no_memory;
            return;
        }
        IDL_msp->IDL_data_addr = (idl_byte *)
               (((p_es_state->IDL_align_buff_addr - (idl_byte *)0) + 7) & (~7));
        memcpy(IDL_msp->IDL_data_addr, p_es_state->IDL_buff_addr,
                                                        p_es_state->IDL_bsize);
    }
    else
    {
        p_es_state->IDL_align_buff_addr = NULL;
        IDL_msp->IDL_data_addr = p_es_state->IDL_buff_addr;
    }
    IDL_msp->IDL_mp = IDL_msp->IDL_data_addr;
    IDL_msp->IDL_left_in_buff = p_es_state->IDL_bsize;
}

#ifndef KERNEL
/******************************************************************************/
/*                                                                            */
/*  idl_es_set_transfer_syntax - API routine                                  */
/*  Set a transfer syntax (user must call this before encoding using a stub   */
/*      which supports more than one transfer syntax)                         */
/*                                                                            */
/******************************************************************************/
void idl_es_set_transfer_syntax
#ifdef IDL_PROTOTYPES
(
    idl_es_handle_t h,      /* [in,out] User's encoding handle */
    idl_es_transfer_syntax_t es_transfer_syntax,
                            /* [in] requested transfer syntax */
    error_status_t *st
)
#else
(h, es_transfer_syntax, st)
    idl_es_handle_t h;
    idl_es_transfer_syntax_t es_transfer_syntax;
    error_status_t *st;
#endif
{
    IDL_es_state_t *p_es_state;

    p_es_state = (IDL_es_state_t *)h;
    switch (es_transfer_syntax)
    {
        case idl_es_transfer_syntax_ndr:
            p_es_state->IDL_pickle_header.IDL_syntax_id
                                                     = ndr_transfer_syntax_id;
            *st = error_status_ok;
            break;
        default:
            *st = rpc_s_tsyntaxes_unsupported;
            break;
    }
}
#endif /*KERNEL*/

/******************************************************************************/
/*                                                                            */
/*  idl_es_check_transfer_syntax - local routine                              */
/*  Check the stub supports the transfer syntax and return the appropriate    */
/*      enumeration value                                                     */
/*                                                                            */
/******************************************************************************/
static void idl_es_check_transfer_syntax
#ifdef IDL_PROTOTYPES
(
    rpc_if_rep_t *p_if_spec,    /* [in] Pointer to stub's if_spec */
    IDL_es_state_t *p_es_state, /* [in] State block containing transfer syntax
                                        to be checked */
    idl_es_transfer_syntax_t *p_es_transfer_syntax,
                            /* [out] Transfer syntax to use for encoding */
    IDL_msp_t IDL_msp
)
#else
(p_if_spec, p_es_state, p_es_transfer_syntax, IDL_msp)
    rpc_if_rep_t *p_if_spec;
    IDL_es_state_t *p_es_state;
    idl_es_transfer_syntax_t *p_es_transfer_syntax;
    IDL_msp_t IDL_msp;
#endif
{
    int i = 0;

    /* Check the stub supports the transfer syntax */
    for (i=0; i<p_if_spec->syntax_vector.count; i++)
    {
        if ( uuid_equal(&(p_es_state->IDL_pickle_header.IDL_syntax_id.id),
                        &(p_if_spec->syntax_vector.syntax_id[i].id),
                        (error_status_t *)&IDL_msp->IDL_status)
            && (p_es_state->IDL_pickle_header.IDL_syntax_id.version
                            == p_if_spec->syntax_vector.syntax_id[i].version) )
        {
            break;
        }
    }
    if ( i >= p_if_spec->syntax_vector.count )
    {
        /* No match found in list in stub's ifspec */
        IDL_msp->IDL_status = rpc_s_tsyntaxes_unsupported;
        RAISE(rpc_x_ss_pipe_comm_error);
    }
    /* Return the enumeration value for the transfer syntax */
        *p_es_transfer_syntax = idl_es_transfer_syntax_ndr;
}

/******************************************************************************/
/*                                                                            */
/*  idl_es_encode_get_xfer_syntax - local routine                             */
/*  Get the transfer syntax to be used for the encoding                       */
/*                                                                            */
/******************************************************************************/
static void idl_es_encode_get_xfer_syntax
#ifdef IDL_PROTOTYPES
(
    idl_es_handle_t h,      /* [in] User's encoding handle */
    rpc_if_handle_t ifp,    /* [in] Pointer to stub's ifspec */
    idl_es_transfer_syntax_t *p_es_transfer_syntax,
                            /* [out] Transfer syntax to use for encoding */
    IDL_msp_t IDL_msp
)
#else
(h, ifp, p_es_transfer_syntax, IDL_msp)
    idl_es_handle_t h;
    rpc_if_handle_t ifp;
    idl_es_transfer_syntax_t *p_es_transfer_syntax;
    IDL_msp_t IDL_msp;
#endif
{
    IDL_es_state_t *p_es_state;
    rpc_if_rep_t *p_if_spec;

    p_es_state = (IDL_es_state_t *)h;
    p_if_spec = (rpc_if_rep_t *)ifp;
    if ( uuid_is_nil(&(p_es_state->IDL_pickle_header.IDL_syntax_id.id),
                     (error_status_t *)&IDL_msp->IDL_status) )
    {
        /* No transfer syntax specified in the handle,
            so can the transfer syntax to use be determined from the stub? */
        if (p_if_spec->syntax_vector.count != 1)
        {
            /* Transfer syntax not adequately specified by stub */
            IDL_msp->IDL_status = rpc_s_tsyntaxes_unsupported;
            RAISE(rpc_x_ss_pipe_comm_error);
        }
        else
        {
            /* Copy the transfer syntax into the encoding state block */
            p_es_state->IDL_pickle_header.IDL_syntax_id =
                                        *(p_if_spec->syntax_vector.syntax_id);
        }
    }

    /* Check the stub supports the transfer syntax the user chose */
    idl_es_check_transfer_syntax(p_if_spec, p_es_state, p_es_transfer_syntax,
                                 IDL_msp);
}

#ifndef KERNEL
/******************************************************************************/
/*                                                                            */
/*  idl_es_encode_new_dyn_buff - Called locally and from MIA/BER support      */
/*  Create new intermediate buffer list element                               */
/*  Returns error_status_ok or rpc_s_no_memory                                */
/*  Does not raise exceptions because it can be called from below DDIS        */
/*                                                                            */
/******************************************************************************/
error_status_t idl_es_encode_new_dyn_buff
#ifdef IDL_PROTOTYPES
(
    idl_ulong_int *p_buff_size,     /* [out] Size of buffer returned */
    IDL_msp_t IDL_msp
)
#else
(p_buff_size, IDL_msp)
    idl_ulong_int *p_buff_size;
    IDL_msp_t IDL_msp;
#endif
{
    IDL_dyn_buff_link_t *p_new_link;
    rpc_iovector_elt_t *p_new_iovec_elt;
    IDL_es_state_t *p_es_state;

    p_es_state = (IDL_es_state_t *)(IDL_msp->IDL_pickling_handle);

    if ( (*(p_es_state->IDL_p_buff_addr) != NULL)
            && (p_es_state->IDL_dyn_buff_chain_head == NULL) )
    {
        /* A previous encoding operation was called using the current
            encoding handle (signified by IDL_p_buff_addr not NULL)
            and this is the first intermediate buffer needed by the
            current operation (signified by IDL_dyn_buff_chain_head == NULL).
            Make the output from the preceding operation(s) be the first element
            of the list of intermediate buffers */
        p_new_link = (IDL_dyn_buff_link_t *)malloc
                                        (sizeof(IDL_dyn_buff_link_t));
        if (p_new_link == NULL)
        {
            (*(IDL_msp->IDL_p_free))(*(p_es_state->IDL_p_buff_addr));
            return(rpc_s_no_memory);
        }
        p_new_link->IDL_p_iovec_elt = NULL;
        p_new_link->IDL_next = NULL;
        p_es_state->IDL_dyn_buff_chain_head = p_new_link;
        p_es_state->IDL_dyn_buff_chain_tail = p_new_link;
        /* Create an rpc_iovector_elt to describe the buffer */
        p_new_iovec_elt = (rpc_iovector_elt_t *)malloc
                                        (sizeof(rpc_iovector_elt_t));
        if (p_new_iovec_elt == NULL)
        {
            (*(IDL_msp->IDL_p_free))(*(p_es_state->IDL_p_buff_addr));
            return(rpc_s_no_memory);
        }
        p_new_link->IDL_p_iovec_elt = p_new_iovec_elt;
        p_new_iovec_elt->buff_addr = *(p_es_state->IDL_p_buff_addr);
        p_new_iovec_elt->data_addr = *(p_es_state->IDL_p_buff_addr);
        p_new_iovec_elt->data_len = *(p_es_state->IDL_esize);
    }

    p_new_link = (IDL_dyn_buff_link_t *)malloc
                                        (sizeof(IDL_dyn_buff_link_t));
    if (p_new_link == NULL)
        return(rpc_s_no_memory);
    p_new_link->IDL_p_iovec_elt = NULL;
    p_new_link->IDL_next = NULL;
    /* Connect it to the state block */
    if (p_es_state->IDL_dyn_buff_chain_head == NULL)
        p_es_state->IDL_dyn_buff_chain_head = p_new_link;
    else
        p_es_state->IDL_dyn_buff_chain_tail->IDL_next = p_new_link;
    p_es_state->IDL_dyn_buff_chain_tail = p_new_link;
    /* Create an rpc_iovector_elt to describe the buffer */
    p_new_iovec_elt = (rpc_iovector_elt_t *)malloc
                                        (sizeof(rpc_iovector_elt_t));
    if (p_new_iovec_elt == NULL)
        return(rpc_s_no_memory);
    p_new_iovec_elt->buff_addr = NULL;
    /* Attach the rpc_iovector_elt to the chain */
    p_new_link->IDL_p_iovec_elt = p_new_iovec_elt;
    /* Create the buffer */
    IDL_msp->IDL_buff_addr = (idl_byte *)
                        (*(IDL_msp->IDL_p_allocate))(IDL_BUFF_SIZE);
    if (IDL_msp->IDL_buff_addr == NULL)
        return(rpc_s_no_memory);
    /* Attach the buffer to the rpc_iovector_elt */
    p_new_iovec_elt->buff_addr = IDL_msp->IDL_buff_addr;
    *p_buff_size = IDL_BUFF_SIZE;
    return(error_status_ok);
}

/******************************************************************************/
/*                                                                            */
/*  idl_es_encode_init_buffer - called from NDR marshalling interpreter       */
/*  Action to be taken by rpc_ss_marsh_init_buffer when used for encoding     */
/*                                                                            */
/******************************************************************************/
void idl_es_encode_init_buffer
#ifdef IDL_PROTOTYPES
(
    idl_ulong_int *p_buff_size,     /* [out] Size of buffer returned */
    IDL_msp_t IDL_msp
)
#else
(p_buff_size, IDL_msp)
    idl_ulong_int *p_buff_size;
    IDL_msp_t IDL_msp;
#endif
{
    IDL_es_state_t *p_es_state;

    p_es_state = (IDL_es_state_t *)(IDL_msp->IDL_pickling_handle);
    switch (p_es_state->IDL_style)
    {
        case IDL_incremental_k:
            *p_buff_size = IDL_BUFF_SIZE;
            (*(p_es_state->IDL_alloc))(p_es_state->IDL_state,
                                         &IDL_msp->IDL_buff_addr, p_buff_size);
            if (((IDL_msp->IDL_buff_addr - (idl_byte *)0) & 7) != 0)
            {
                /* User buffer is not 8-byte aligned */
                IDL_msp->IDL_status = rpc_s_ss_bad_buffer;
                RAISE(rpc_x_ss_pipe_comm_error);
            }
            if (((*p_buff_size & 7) != 0) || (*p_buff_size < 8))
            {
                /* User buffer is not multiple of 8 bytes */
                IDL_msp->IDL_status = rpc_s_ss_bad_buffer;
                RAISE(rpc_x_ss_pipe_comm_error);
            }
            break;
        case IDL_fixed_k:
            /* Ran out of buffer space with data still to be marshalled */
            RAISE(rpc_x_no_memory);
            break;
        case IDL_dynamic_k:
            /* Create new intermediate buffer list element */
            if (idl_es_encode_new_dyn_buff(p_buff_size, IDL_msp)
                                                            != error_status_ok)
                RAISE(rpc_x_no_memory);
            break;
        default:
#ifdef DEBUG_INTERP
            printf("idl_es_encode_init_buffer - unknown encoding style\n");
            exit(0);
#endif
            RAISE(rpc_x_coding_error);
    }
}

/******************************************************************************/
/*                                                                            */
/*  idl_es_encode_attach_buff - called from NDR marshalling interpreter       */
/*  Action to be taken by rpc_ss_attach_buff_to_iovec when used for encoding  */
/*                                                                            */
/******************************************************************************/
void idl_es_encode_attach_buff
#ifdef IDL_PROTOTYPES
(
    IDL_msp_t IDL_msp
)
#else
(IDL_msp)
    IDL_msp_t IDL_msp;
#endif
{
    IDL_es_state_t *p_es_state;
    rpc_iovector_elt_t *p_iovec_elt;

    p_es_state = (IDL_es_state_t *)(IDL_msp->IDL_pickling_handle);
    switch (p_es_state->IDL_style)
    {
        case IDL_incremental_k:
            (*(p_es_state->IDL_write))(p_es_state->IDL_state,
                                      IDL_msp->IDL_buff_addr,
                                      IDL_msp->IDL_mp - IDL_msp->IDL_data_addr);
            break;
        case IDL_fixed_k:
            /* Can only happen at end of operation - do nothing */
            break;
        case IDL_dynamic_k:
            p_iovec_elt = p_es_state->IDL_dyn_buff_chain_tail->IDL_p_iovec_elt;
            p_iovec_elt->data_addr = (byte_p_t)IDL_msp->IDL_data_addr;
            p_iovec_elt->data_len = IDL_msp->IDL_mp - IDL_msp->IDL_data_addr;
            break;
        default:
#ifdef DEBUG_INTERP
            printf("idl_es_encode_attach_buff - unknown encoding style\n");
            exit(0);
#endif
            RAISE(rpc_x_coding_error);
    }
}
#endif /*KERNEL*/

/******************************************************************************/
/*                                                                            */
/*  idl_es_put_encoding_uuid - local routine                                  */
/*  Write a UUID in the header for a pickle                                   */
/*                                                                            */
/******************************************************************************/
static void idl_es_put_encoding_uuid
#ifdef IDL_PROTOTYPES
(
    uuid_t *p_uuid,     /* [in] Address of UUID */
    IDL_msp_t IDL_msp
)
#else
(p_uuid, IDL_msp)
    uuid_t *p_uuid;
    IDL_msp_t IDL_msp;
#endif
{
    int i;

    IDL_MARSH_ULONG(&p_uuid->time_low);
    IDL_MARSH_USHORT(&p_uuid->time_mid);
    IDL_MARSH_USHORT(&p_uuid->time_hi_and_version);
    IDL_MARSH_USMALL(&p_uuid->clock_seq_hi_and_reserved);
    IDL_MARSH_USMALL(&p_uuid->clock_seq_low);
    for (i=0; i<6; i++)
    {
        IDL_MARSH_BYTE(&p_uuid->node[i]);
    }
}

/******************************************************************************/
/*                                                                            */
/*  idl_es_put_encoding_header - local routine                                */
/*  Write the header for this pickle                                          */
/*                                                                            */
/******************************************************************************/
static void idl_es_put_encoding_header
#ifdef IDL_PROTOTYPES
(
    rpc_if_handle_t ifp,    /* [in] Pointer to stub's ifspec */
    idl_ulong_int op_num,   /* [in] operation number */
    idl_es_transfer_syntax_t es_transfer_syntax,
                         /* [in] Transfer syntax user data will be encoded in */
    IDL_msp_t IDL_msp
)
#else
(ifp, op_num, es_transfer_syntax, IDL_msp)
    rpc_if_handle_t ifp;
    idl_ulong_int op_num;
    idl_es_transfer_syntax_t es_transfer_syntax;
    IDL_msp_t IDL_msp;
#endif
{
    idl_usmall_int version = IDL_ES_HEADER_VERSION;
    idl_usmall_int fill = 0;
    IDL_es_state_t *p_es_state;
    rpc_if_rep_t *p_if_spec;
    int i;
    idl_ushort_int vers_field;

    p_es_state = (IDL_es_state_t *)(IDL_msp->IDL_pickling_handle);
    p_if_spec = (rpc_if_rep_t *)ifp;

    IDL_MARSH_USMALL(&version);
    IDL_MARSH_USMALL(&ndr_g_local_drep.int_rep);
    IDL_MARSH_USMALL(&fill);
    IDL_MARSH_USMALL(&fill);
    idl_es_put_encoding_uuid(&p_es_state->IDL_pickle_header.IDL_syntax_id.id,
                                                                     IDL_msp);
    IDL_MARSH_ULONG(&p_es_state->IDL_pickle_header.IDL_syntax_id.version);
#if defined(_MSDOS)
    idl_es_put_encoding_uuid((uuid_p_t)&p_if_spec->InterfaceId.SyntaxGUID, 
                             IDL_msp);
    vers_field = p_if_spec->InterfaceId.SyntaxVersion.MajorVersion;   /* Major version */
    IDL_MARSH_USHORT(&vers_field);
    vers_field = p_if_spec->InterfaceId.SyntaxVersion.MinorVersion;   /* Minor version */
    IDL_MARSH_USHORT(&vers_field);
#else
    idl_es_put_encoding_uuid(&p_if_spec->id, IDL_msp);
    vers_field = p_if_spec->vers / 65536;   /* Major version */
    IDL_MARSH_USHORT(&vers_field);
    vers_field = p_if_spec->vers % 65536;   /* Minor version */
    IDL_MARSH_USHORT(&vers_field);
#endif
    IDL_MARSH_ULONG(&op_num);
    if (es_transfer_syntax == idl_es_transfer_syntax_ndr)
    {
        IDL_MARSH_USMALL(&ndr_g_local_drep.int_rep);
        IDL_MARSH_USMALL(&ndr_g_local_drep.char_rep);
        IDL_MARSH_USMALL(&ndr_g_local_drep.float_rep);
        IDL_MARSH_BYTE(&ndr_g_local_drep.reserved);
        /* Four filler bytes to make the user data start at (0 mod 8) */
        for (i=0; i<4; i++)
        {
            IDL_MARSH_USMALL(&fill);
        }
    }

    /* Store interface ID and operation number in state block */
    p_es_state->IDL_pickle_header.IDL_if_id.uuid = p_if_spec->id;
    p_es_state->IDL_pickle_header.IDL_if_id.vers_major
                                                     = p_if_spec->vers / 65536;
    p_es_state->IDL_pickle_header.IDL_if_id.vers_minor
                                                     = p_if_spec->vers % 65536;
    p_es_state->IDL_pickle_header.IDL_op_num = op_num;
}

/******************************************************************************/
/*                                                                            */
/*  idl_es_get_encoding_uuid - local routine                                  */
/*  Read a UUID in the header for a pickle                                    */
/*                                                                            */
/******************************************************************************/
static void idl_es_get_encoding_uuid
#ifdef IDL_PROTOTYPES
(
    uuid_t *p_uuid,     /* [in] Address of UUID */
    IDL_msp_t IDL_msp
)
#else
(p_uuid, IDL_msp)
    uuid_t *p_uuid;
    IDL_msp_t IDL_msp;
#endif
{
    int i;

    IDL_UNMAR_ULONG(&p_uuid->time_low);
    IDL_UNMAR_USHORT(&p_uuid->time_mid);
    IDL_UNMAR_USHORT(&p_uuid->time_hi_and_version);
    IDL_UNMAR_USMALL(&p_uuid->clock_seq_hi_and_reserved);
    IDL_UNMAR_USMALL(&p_uuid->clock_seq_low);
    for (i=0; i<6; i++)
    {
        IDL_UNMAR_BYTE(&p_uuid->node[i]);
    }
}

/******************************************************************************/
/*                                                                            */
/*  idl_es_get_encoding_header - local routine                                */
/*  Read pickle header into local storage                                     */
/*                                                                            */
/******************************************************************************/
static void idl_es_get_encoding_header
#ifdef IDL_PROTOTYPES
(
    idl_es_pvt_header_t *p_pickle_header,       /* [out] local copy of pickle 
                                                         header */
    IDL_msp_t IDL_msp
)
#else
(p_pickle_header, IDL_msp)
    idl_es_pvt_header_t *p_pickle_header;
    IDL_msp_t IDL_msp;
#endif
{
    IDL_es_state_t *p_es_state;

    p_es_state = (IDL_es_state_t *)(IDL_msp->IDL_pickling_handle);

    IDL_UNMAR_USMALL(&p_pickle_header->IDL_version);
    if (p_pickle_header->IDL_version != IDL_ES_HEADER_VERSION)
    {
        IDL_msp->IDL_status = rpc_s_ss_wrong_es_version;
        RAISE(rpc_x_ss_pipe_comm_error);
    }
    IDL_UNMAR_USMALL(&p_pickle_header->IDL_int_drep);
    /* This drep is needed to control decoding the rest of header */
    IDL_msp->IDL_drep.int_rep = p_pickle_header->IDL_int_drep;
    /* Called rtn will do an alignment operation, discarding the filler bytes */
    idl_es_get_encoding_uuid(&p_pickle_header->IDL_syntax_id.id, IDL_msp);
    IDL_UNMAR_ULONG(&p_pickle_header->IDL_syntax_id.version);
    idl_es_get_encoding_uuid(&p_pickle_header->IDL_if_id.uuid, IDL_msp);
    IDL_UNMAR_USHORT(&p_pickle_header->IDL_if_id.vers_major);
    IDL_UNMAR_USHORT(&p_pickle_header->IDL_if_id.vers_minor);
    IDL_UNMAR_ULONG(&p_pickle_header->IDL_op_num);

    /* If the pickle uses NDR encoding, get its drep's */
    if (uuid_equal(&p_pickle_header->IDL_syntax_id.id,
                   &ndr_transfer_syntax_id.id,
                   (error_status_t *)&IDL_msp->IDL_status))
    {
        IDL_UNMAR_USMALL(&IDL_msp->IDL_drep.int_rep);
        IDL_UNMAR_USMALL(&IDL_msp->IDL_drep.char_rep);
        IDL_UNMAR_USMALL(&IDL_msp->IDL_drep.float_rep);
        /* And align to 8 bytes */
        IDL_UNMAR_ALIGN_MP(IDL_msp, 8);
    }
    p_es_state->IDL_pickle_header_read = idl_true;
}

/******************************************************************************/
/*                                                                            */
/*  idl_es_before_interp_call - SPI routine                                   */
/*  Set up for interpreter call to encode/decode user data                    */
/*                                                                            */
/******************************************************************************/
void idl_es_before_interp_call
#ifdef IDL_PROTOTYPES
(
    idl_es_handle_t h,      /* [in] User's encoding handle */
    rpc_if_handle_t ifp,    /* [in] Pointer to stub's ifspec */
    idl_byte IDL_type_vec[],    /* [in] Stub's type vector */
    idl_ulong_int op_num,   /* [in] operation number */
    IDL_es_action_type_k_t stub_action, /* [in] Is this operation labelled
                                            [encode], [decode] */
    idl_es_transfer_syntax_t *p_es_transfer_syntax,
                            /* [out] Transfer syntax to use for encoding */
    IDL_msp_t IDL_msp
)
#else
(h, ifp, IDL_type_vec, op_num, stub_action, p_es_transfer_syntax, IDL_msp)
    idl_es_handle_t h;
    rpc_if_handle_t ifp;
    idl_byte IDL_type_vec[];
    idl_ulong_int op_num;
    IDL_es_action_type_k_t stub_action;
    idl_es_transfer_syntax_t *p_es_transfer_syntax;
    IDL_msp_t IDL_msp;
#endif
{
    IDL_es_state_t *p_es_state;
    rpc_if_rep_t *p_if_spec;

    /* If we get any abnormal condition we need to cope with the situation
        where this is a dynamic encoding and the operation is not the first
        that used the handle. In this case we need to release the encoding
        resulting from the preceding operations */
    TRY
        /* Complete initialization of state block */
        IDL_msp->IDL_type_vec = IDL_type_vec;
        rpc_ss_type_vec_vers_check( IDL_msp );

        p_es_state = (IDL_es_state_t *)h;
        /* Was operation specified to support action? */
        if (stub_action != IDL_both_k)
        {
            if (stub_action != p_es_state->IDL_action)
            {
                IDL_msp->IDL_status = rpc_s_ss_bad_es_action;
                RAISE(rpc_x_ss_pipe_comm_error);
            }
        }

        if (p_es_state->IDL_action == IDL_encoding_k)
        {
            idl_es_encode_get_xfer_syntax(h, ifp, p_es_transfer_syntax, IDL_msp);
            if (p_es_state->IDL_style == IDL_dynamic_k)
            {
                /* Need user allocator for intermediate and final buffers */
                rpc_ss_mts_client_estab_alloc(IDL_msp);
            }
            idl_es_put_encoding_header(ifp, op_num, *p_es_transfer_syntax, IDL_msp);
        }
        else    /* decoding */
        {
            if ( ! p_es_state->IDL_pickle_header_read )
            {
                idl_es_get_encoding_header(&p_es_state->IDL_pickle_header, IDL_msp);
            }
            p_if_spec = (rpc_if_rep_t *)ifp;

            /*
             * Don't check interface uuid's or version number if the user
             * doesn't want to.
             */
            if ( ! (p_es_state->IDL_es_flags & IDL_ES_NO_ENCODING_CHECK))
            {
                if ( ! uuid_equal(&p_es_state->IDL_pickle_header.IDL_if_id.uuid,
                              &p_if_spec->id,
                                  (error_status_t *)&IDL_msp->IDL_status) )
                {
                    /* Wrong interface */
                    IDL_msp->IDL_status = rpc_s_unknown_if;
                    RAISE(rpc_x_ss_pipe_comm_error);
                }
            if (p_if_spec->vers 
                != (p_es_state->IDL_pickle_header.IDL_if_id.vers_major * 65536
                        + p_es_state->IDL_pickle_header.IDL_if_id.vers_minor))
                {
                    /* Wrong version */
                    IDL_msp->IDL_status = rpc_s_unknown_if;
                    RAISE(rpc_x_ss_pipe_comm_error);
                }
            }

            /* Check the stub supports the transfer syntax of the pickle */
            idl_es_check_transfer_syntax(p_if_spec, p_es_state,
                                         p_es_transfer_syntax, IDL_msp);
            /* Now at a point where we won't test for "pickle header read" again
            for this operation. Reset it in case user has another operation in
            the pickle */
            p_es_state->IDL_pickle_header_read = idl_false;
        }
    CATCH_ALL
        if ( (p_es_state->IDL_action == IDL_encoding_k)
            && (p_es_state->IDL_style == IDL_dynamic_k)
            && (*(p_es_state->IDL_p_buff_addr) != NULL) )
        {
            (*(IDL_msp->IDL_p_free))(*(p_es_state->IDL_p_buff_addr));
        }
        RERAISE;
    ENDTRY
}

/******************************************************************************/
/*                                                                            */
/*  idl_es_encode_dyn_size - local routine                                    */
/*  Cet the total size of a dynamic encoding                                  */
/*                                                                            */
/******************************************************************************/
static void idl_es_encode_dyn_size
#ifdef IDL_PROTOTYPES
(
    IDL_dyn_buff_link_t *p_list_elt,    /* [in] pointer to list of 
                                                    intermediate buffers */
    idl_ulong_int *p_dyn_size           /* [out] size of encoding */
)
#else
(p_list_elt, p_dyn_size)
    IDL_dyn_buff_link_t *p_list_elt;
    idl_ulong_int *p_dyn_size;
#endif
{
    idl_ulong_int dyn_size = 0;

    for ( ; p_list_elt != NULL; p_list_elt = p_list_elt->IDL_next)
    {
        dyn_size += p_list_elt->IDL_p_iovec_elt->data_len;
    }
    *p_dyn_size = dyn_size;
}

/******************************************************************************/
/*                                                                            */
/*  idl_es_encode_dyn_copy_rel - local routine                                */
/*  Copy the intermediate buffers for a dynamic encoding to the buffer to be  */
/*      delivered to the user, and release the intermediate buffer chain      */
/*                                                                            */
/******************************************************************************/
static void idl_es_encode_dyn_copy_rel
#ifdef IDL_PROTOTYPES
(
    IDL_dyn_buff_link_t *p_list_elt,    /* [in] pointer to list of 
                                                    intermediate buffers */
    idl_byte *dyn_buff,      /* [out] location to copy intermediate buffer to */
    IDL_msp_t IDL_msp
)
#else
(p_list_elt, dyn_buff, IDL_msp)
    IDL_dyn_buff_link_t *p_list_elt;
    idl_byte *dyn_buff;
    IDL_msp_t IDL_msp;
#endif
{
    rpc_iovector_elt_t *p_iovec_elt;
    idl_ulong_int inter_data_len;   /* Length of data in intermediate buffer */
    IDL_dyn_buff_link_t *p_old_list_elt;

    while (p_list_elt != NULL)
    {
        p_iovec_elt = p_list_elt->IDL_p_iovec_elt;
        /* Copy data */
        inter_data_len = p_iovec_elt->data_len;
        memcpy(dyn_buff, p_iovec_elt->data_addr, inter_data_len);
        dyn_buff += inter_data_len;
        /* Release head of chain */
        (*(IDL_msp->IDL_p_free))(p_iovec_elt->buff_addr);
        free(p_iovec_elt);
        p_old_list_elt = p_list_elt;
        p_list_elt = p_list_elt->IDL_next;
        free(p_old_list_elt);
    }
}

/******************************************************************************/
/*                                                                            */
/*  idl_es_after_interp_call - SPI routine                                    */
/*  Tidy up after normal return from interpreter call to encode/decode user   */
/*      data                                                                  */
/*                                                                            */
/******************************************************************************/
void idl_es_after_interp_call
#ifdef IDL_PROTOTYPES
(
    IDL_msp_t IDL_msp
)
#else
(IDL_msp)
    IDL_msp_t IDL_msp;
#endif
{
    IDL_es_state_t *p_es_state;
    rpc_iovector_elt_t *p_iovec_elt;
    idl_ulong_int dyn_size;     /* Size of dynamic encoding */
    idl_byte *dyn_buff;         /* Dynamic buffer delivered to user */

    p_es_state = (IDL_es_state_t *)(IDL_msp->IDL_pickling_handle);

    if (p_es_state->IDL_action == IDL_encoding_k)
    {
        switch (p_es_state->IDL_style)
        {
            case IDL_incremental_k:
                /* Last part of pickle already handed to user */
                /* There may be another operation using the same handle
                    - reset state */
                if (IDL_msp->IDL_mem_handle.memory)
                {
                    rpc_ss_mem_free(&IDL_msp->IDL_mem_handle);
                }
                rpc_ss_init_marsh_state(NULL, IDL_msp);
                IDL_msp->IDL_pickling_handle = (rpc_void_p_t)p_es_state;
                break;
            case IDL_fixed_k:
                /* Set size of pickle for user */
                *(p_es_state->IDL_esize) = IDL_msp->IDL_mp
                                                     - IDL_msp->IDL_data_addr;
                break;
            case IDL_dynamic_k:
                if ( (p_es_state->IDL_dyn_buff_chain_head->IDL_next == NULL)
                    && (IDL_msp->IDL_data_addr == IDL_msp->IDL_buff_addr) )
                {
                    /* Intermediate buffer can be handed off to user */
                    p_iovec_elt = 
                           p_es_state->IDL_dyn_buff_chain_head->IDL_p_iovec_elt;
                    *(p_es_state->IDL_p_buff_addr) = (idl_byte *)
                                                    (p_iovec_elt->buff_addr);
                    *(p_es_state->IDL_esize) = p_iovec_elt->data_len;
                    /* Release chain machinery */
                    free(p_iovec_elt);
                    free(p_es_state->IDL_dyn_buff_chain_head);
                }
                else
                {
                    idl_es_encode_dyn_size(p_es_state->IDL_dyn_buff_chain_head,
                                                                     &dyn_size);
                    *(p_es_state->IDL_esize) = dyn_size;
                    dyn_buff = (idl_byte *)
                                        (*(IDL_msp->IDL_p_allocate))(dyn_size);
                    if (dyn_buff == NULL)
                        RAISE(rpc_x_no_memory);
                    idl_es_encode_dyn_copy_rel(
                                            p_es_state->IDL_dyn_buff_chain_head,
                                            dyn_buff, IDL_msp);
                    *(p_es_state->IDL_p_buff_addr) = dyn_buff;
                }
                p_es_state->IDL_dyn_buff_chain_head = NULL;
                /* There may be another operation using the same handle
                    - reset state */
                if (IDL_msp->IDL_mem_handle.memory)
                {
                    rpc_ss_mem_free(&IDL_msp->IDL_mem_handle);
                }
                rpc_ss_init_marsh_state(NULL, IDL_msp);
                IDL_msp->IDL_pickling_handle = (rpc_void_p_t)p_es_state;
                break;
            default:
#ifdef DEBUG_INTERP
                printf(
                  "idl_es_after_normal_interp_call - unknown encoding style\n");
                exit(0);
#endif
                RAISE(rpc_x_coding_error);
        }
    }
}

/******************************************************************************/
/*                                                                            */
/*  idl_es_clean_up - SPI routine                                             */
/*  Tidy up after un/pickling action                                          */
/*                                                                            */
/******************************************************************************/
void idl_es_clean_up
#ifdef IDL_PROTOTYPES
(
    IDL_msp_t IDL_msp
)
#else
(IDL_msp)
    IDL_msp_t IDL_msp;
#endif
{
    IDL_es_state_t *p_es_state;
    IDL_dyn_buff_link_t *p_list_elt;
    IDL_dyn_buff_link_t *p_old_list_elt;
    rpc_iovector_elt_t *p_iovec_elt;

    p_es_state = (IDL_es_state_t *)(IDL_msp->IDL_pickling_handle);

    /* Tidy up if abnormal end during incremental encoding */
    if (p_es_state->IDL_action == IDL_encoding_k)
    {
        if (p_es_state->IDL_style == IDL_dynamic_k)
        {
            p_list_elt = p_es_state->IDL_dyn_buff_chain_head;
            /* Release chain of intermediate buffers */
            while (p_list_elt != NULL)
            {
                p_iovec_elt = p_list_elt->IDL_p_iovec_elt;
                if (p_iovec_elt != NULL)
                {
                    if (p_iovec_elt->buff_addr != NULL)
                        (*(IDL_msp->IDL_p_free))(p_iovec_elt->buff_addr);
                    free(p_iovec_elt);
                }
                p_old_list_elt = p_list_elt;
                p_list_elt = p_list_elt->IDL_next;
                free(p_old_list_elt);
            }
            p_es_state->IDL_dyn_buff_chain_head = NULL;
        }
    }
    /* Release memory allocated by interpreter */
    if (IDL_msp->IDL_mem_handle.memory)
    {
        rpc_ss_mem_free(&IDL_msp->IDL_mem_handle);
    }
}

#ifndef KERNEL
/******************************************************************************/
/*                                                                            */
/*  idl_es_decode_check_buffer - called from NDR unmarshalling interpreter    */
/*  Called when interpreter has exhausted current buffer                      */
/*                                                                            */
/******************************************************************************/
void idl_es_decode_check_buffer
#ifdef IDL_PROTOTYPES
(
    IDL_msp_t IDL_msp
)
#else
(IDL_msp)
    IDL_msp_t IDL_msp;
#endif
{
    IDL_es_state_t *p_es_state;

    p_es_state = (IDL_es_state_t *)(IDL_msp->IDL_pickling_handle);
    if (p_es_state->IDL_style == IDL_incremental_k)
    {
        (*(p_es_state->IDL_read))(p_es_state->IDL_state,
                                  &IDL_msp->IDL_data_addr,
                                  &IDL_msp->IDL_left_in_buff);
        if (((IDL_msp->IDL_data_addr - (idl_byte *)0) & 7) != 0)
        {
            /* User buffer is not 8-byte aligned */
            IDL_msp->IDL_status = rpc_s_ss_bad_buffer;
            RAISE(rpc_x_ss_pipe_comm_error);
        }
        IDL_msp->IDL_mp = IDL_msp->IDL_data_addr;
    }
    else    /* Exhausted fixed buffer */
    {
        IDL_msp->IDL_status = rpc_s_ss_bad_buffer;
        RAISE(rpc_x_ss_pipe_comm_error);
    }
}

/******************************************************************************/
/*                                                                            */
/*  idl_es_inq_encoding_id - API routine                                      */
/*  Give interface ID and operation number to user                            */
/*                                                                            */
/******************************************************************************/
void idl_es_inq_encoding_id
#ifdef IDL_PROTOTYPES
(
    idl_es_handle_t	    h,	    /* [in] decoding handle */
    rpc_if_id_t		    *if_id, /* [out] RPC interface	    */
				    /* identifier (including	    */
				    /* version information)	    */
    idl_ulong_int	    *op,    /* [out] operation number */
    error_status_t	    *st	    /* [out] status */
)
#else
(h, if_id, op, st)
    idl_es_handle_t	    h;
    rpc_if_id_t		    *if_id;
    idl_ulong_int	    *op;
    error_status_t	    *st;
#endif
{
    IDL_es_state_t *p_es_state;

    *st = error_status_ok;
    p_es_state = (IDL_es_state_t *)h;
    if ( (p_es_state->IDL_action == IDL_decoding_k)
        && ( ! p_es_state->IDL_pickle_header_read ) )
    {
        /* User wants operation number before decoding pickle body */
        TRY
            idl_es_get_encoding_header(&p_es_state->IDL_pickle_header,
                                                         p_es_state->IDL_msp);
        CATCH(rpc_x_ss_pipe_comm_error)
            *st = p_es_state->IDL_msp->IDL_status;
        ENDTRY
        if (*st != error_status_ok)
            return;
    }
    else if (p_es_state->IDL_pickle_header.IDL_op_num == IDL_INVALID_OP_NUM)
    {
        /* Encoding, and no operation has yet been invoked */
        *st = rpc_s_ss_bad_es_action;
        return;
    }
    *if_id = p_es_state->IDL_pickle_header.IDL_if_id;
    *op = p_es_state->IDL_pickle_header.IDL_op_num;
}
#endif /*KERNEL*/

/******************************************************************************/
/*                                                                            */
/*  idl_es_handle_free - API routine                                          */
/*  Frees a idl_es_handle_t and its associated resources                      */
/*                                                                            */
/******************************************************************************/
void idl_es_handle_free
#ifdef IDL_PROTOTYPES
(
    idl_es_handle_t	*h,	    /* [in,out] handle to free */
    error_status_t	*st	    /* [out] status */
)
#else
(h, st)
    idl_es_handle_t	*h;
    error_status_t	*st;
#endif
{
    IDL_es_state_t *p_es_state;

    p_es_state = (IDL_es_state_t *)(*h);
    free(p_es_state->IDL_msp);
    if ((p_es_state->IDL_action == IDL_decoding_k)
        && (p_es_state->IDL_style == IDL_fixed_k)
        && (p_es_state->IDL_align_buff_addr != NULL))
    {
        free(p_es_state->IDL_align_buff_addr);
    }
    free(p_es_state);
    *h = NULL;
    *st = error_status_ok;
}


#ifndef KERNEL
/******************************************************************************/
/*                                                                            */
/*  idl_es_set_attrs - API routine                                            */
/*  Set attribute flags in a idl_es_handle_t                                  */
/*                                                                            */
/******************************************************************************/
void idl_es_set_attrs(
	idl_es_handle_t h,
	unsigned32 flags,
	error_status_t *st)
{
	IDL_es_state_t *p_es_state = (IDL_es_state_t *)h;

	p_es_state->IDL_es_flags = flags;
	*st = error_status_ok;
	return;
}


/******************************************************************************/
/*                                                                            */
/*  idl_es_inq_attrs - API routine                                            */
/*  Get the flags from a idl_es_handle_t                                      */
/*                                                                            */
/******************************************************************************/
void idl_es_inq_attrs(
	idl_es_handle_t h,
	unsigned32 *flags,
	error_status_t *st)
{
	IDL_es_state_t *p_es_state = (IDL_es_state_t *)h;

	*flags = p_es_state->IDL_es_flags;
	*st = error_status_ok;
	return;
}
#endif /*KERNEL*/
