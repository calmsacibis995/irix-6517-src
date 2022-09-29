/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(_KERNEL)
static const char *_identString = "$Id: ndrmi.c,v 65.4 1998/03/23 17:24:40 gwehrman Exp $";
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
 * $Log: ndrmi.c,v $
 * Revision 65.4  1998/03/23 17:24:40  gwehrman
 * Source Identification changed to bracket with SGIMIPS && !_KERNEL.
 *
 * Revision 65.3  1998/01/07 17:19:49  jdoak
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS
 *
 * Revision 65.2  1997/11/06  19:55:57  jdoak
 * Source Identification added.
 *
 * Revision 65.1  1997/10/20  19:15:37  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.11.2  1996/02/18  18:54:47  marty
 * 	Update OSF copyright years
 * 	[1996/02/18  18:07:09  marty]
 *
 * Revision 1.1.11.1  1995/12/07  22:29:34  root
 * 	Submit OSF/DCE 1.2.1
 * 
 * 	HP revision /main/HPDCE02/jrr_1.2_mothra/1  1995/11/20  21:48 UTC  psn
 * 	Merge second XIDL drop for DCE 1.2.1
 * 	[1995/11/17  17:07 UTC  dat  /main/dat_xidl2/1]
 * 
 * 	oct 95 idl drop
 * 	[1995/10/22  23:36:00  bfc]
 * 	 *
 * 	may 95 idl drop
 * 	[1995/10/22  22:57:18  bfc]
 * 	 *
 * 	DCE for DEC OSF/1: populate from OSF DCE R1.1
 * 	[1995/10/21  18:25:31  bfc]
 * 	 *
 * 
 * 	HP revision /main/HPDCE02/1  1994/08/03  16:32 UTC  tatsu_s
 * 	Merged mothra up to dce 1.1.
 * 
 * 	HP revision /main/HPDCE01/5  1994/06/24  15:26 UTC  ajayb
 * 	Merge OT10345 changes
 * 
 * 	HP revision /main/HPDCE01/ajayb_merge_OT10345/1  1994/06/24  14:53 UTC  ajayb
 * 	Merge OT10345
 * 
 * 	HP revision /main/HPDCE01/lmm_idl_OT10345/1  1994/06/17  16:29 UTC  lmm
 * 	merge fix for OT 10345
 * 	[1995/12/07  21:15:42  root]
 * 
 * Revision 1.1.6.3  1994/07/13  18:56:14  tom
 * 	Bug 10103 - Reduce little-endian bias of stubs
 * 	[1994/07/12  18:49:30  tom]
 * 
 * Revision 1.1.6.2  1994/05/11  17:37:26  rico
 * 	Fixes for [string] and [ptr] interactions.
 * 	[1994/05/10  14:18:31  rico]
 * 	HP revision /main/HPDCE01/4  1994/05/26  20:45 UTC  gaz
 * 	Merge IDL performance changes
 * 
 * 	HP revision /main/HPDCE01/gaz_kk_idl_merge/1  1994/05/06  19:34 UTC  gaz
 * 	Merge IDL performance enhancements with mainline
 * 
 * 	HP revision /main/HPDCE01/ajayb_idl/1  1994/04/12  18:12 UTC  ajayb
 * 	Optimizations to reduce malloc calls.
 * 	Changes for [un]marshalling of full ptrs.
 * 
 * 	HP revision /main/HPDCE01/3  1994/04/13  18:28 UTC  lmm
 * 	Support type vectors spelled in either endian
 * 
 * 	HP revision /main/HPDCE01/lmm_idl_endian_fixes/1  1994/04/08  15:16 UTC  lmm
 * 	Support type vectors spelled in either endian
 * 
 * 	HP revision /main/HPDCE01/2  1994/03/10  21:26 UTC  lmm
 * 	changing idl endian bias
 * 
 * 	HP revision /main/HPDCE01/lmm_idl_endian/1  1994/03/10  21:09 UTC  lmm
 * 	changing idl endian bias
 * 
 * Revision 1.1.5.2  1993/11/11  22:04:07  sanders
 * 	Removed some statics so libbb could link to routines
 * 	[1993/11/11  22:00:56  sanders]
 * 
 * Revision 1.1.6.1  1994/05/03  18:50:44  tom
 * 	From DEC: Add exended string support.
 * 	[1994/05/03  18:46:05  tom]
 * 
 * Revision 1.1.2.3  1993/08/20  15:58:49  hinxman
 * 	OT8470 [out] conformant [v1_array] with [in] pipe
 * 	OT8484 Large arrays of [cs_char]s
 * 	[1993/08/20  15:57:35  hinxman]
 * 
 * Revision 1.1.2.2  1993/07/07  20:08:54  ganni
 * 	reduced stub idl sources
 * 	[1993/07/07  19:38:14  ganni]
 * 
 * $EndLog$
 */
/*
**  Copyright (c) Digital Equipment Corporation, 1991, 1992, 1993
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
**      ndrmi.c
**
**  FACILITY:
**
**      Interface Definition Language (IDL) Compiler
**
**  ABSTRACT:
**
**      NDR marshalling interpreter main module
**
*/

#include <dce/idlddefs.h>
#include <ndrmi.h>
#include <lsysdep.h>
/*
** this is the entry point vector that allows c functions to
** call c++ functions in the runtime library
*/
rpc_ss_ndr_func_p_t rpc_ss_idl_mi_epv[RPC_SS_NDR_LAST_EPV_INDEX];

/*
 *  Forward function references
 */

/* None */

/******************************************************************************/
/*                                                                            */
/*  Attach the current buffer to the iovector                                 */
/*                                                                            */
/******************************************************************************/
void rpc_ss_attach_buff_to_iovec
#ifdef IDL_PROTOTYPES
(
    IDL_msp_t IDL_msp
)
#else
( IDL_msp )
    IDL_msp_t IDL_msp;
#endif
{
    rpc_iovector_elt_t *p_elt;

#ifndef _KERNEL
    if (IDL_msp->IDL_pickling_handle != NULL)
    {
        idl_es_encode_attach_buff(IDL_msp);
        return;
    }
#endif

    p_elt = &(IDL_msp->IDL_iovec.elt[IDL_msp->IDL_elts_in_use]);
    if (IDL_msp->IDL_stack_packet_status == IDL_stack_packet_in_use_k)
    {
        IDL_msp->IDL_stack_packet_status = IDL_stack_packet_used_k;
        p_elt->buff_dealloc = NULL_FREE_RTN;
        p_elt->flags = rpc_c_iovector_elt_reused;
    }
    else if (IDL_msp->IDL_stack_packet_status == IDL_stack_packet_part_used_k)
    {
        p_elt->buff_dealloc = NULL_FREE_RTN;
        p_elt->flags = rpc_c_iovector_elt_reused;
    }
    else
    {
        p_elt->buff_dealloc = (rpc_buff_dealloc_fn_t)free;
        p_elt->flags = 0;
    }
    p_elt->buff_addr = (byte_p_t)IDL_msp->IDL_buff_addr;
    p_elt->buff_len = IDL_BUFF_SIZE;
    p_elt->data_addr = (byte_p_t)IDL_msp->IDL_data_addr;
    p_elt->data_len = IDL_msp->IDL_mp - IDL_msp->IDL_data_addr;
    (IDL_msp->IDL_elts_in_use)++;
    IDL_msp->IDL_buff_addr = NULL;
}

/******************************************************************************/
/*                                                                            */
/*  If all the iovector slots are full or we are marshalling pipe data        */
/*  or [transmit_as] data by pointing, despatch the iovector and reset the    */
/*  slot count to zero                                                        */
/*                                                                            */
/******************************************************************************/
void rpc_ss_xmit_iovec_if_necess
#ifdef IDL_PROTOTYPES
(
    /* [in] */ idl_boolean attached_pointed_at,  /* TRUE => last element added 
                                     to iovector was a pointer to user data */
    IDL_msp_t IDL_msp
)
#else
( attached_pointed_at, IDL_msp )
    idl_boolean attached_pointed_at;
    IDL_msp_t IDL_msp;
#endif
{
#ifndef _KERNEL
    if (IDL_msp->IDL_pickling_handle != NULL)
    {
        return;     /* Data already captured by rpc_ss_attach_buff_to_iovec */
    }
#endif

    if ( (IDL_msp->IDL_elts_in_use == IDL_IOVECTOR_SIZE)
        || (attached_pointed_at
              && (IDL_msp->IDL_marsh_pipe || IDL_msp->IDL_m_xmit_level > 0)) )
    {
        /* Despatch the iovector */
        IDL_msp->IDL_iovec.num_elt = (idl_ushort_int)IDL_msp->IDL_elts_in_use;
        rpc_call_transmit( (rpc_call_handle_t)IDL_msp->IDL_call_h,
                           (rpc_iovector_p_t)&IDL_msp->IDL_iovec, 
                           (unsigned32 *)&IDL_msp->IDL_status );
        if (IDL_msp->IDL_status != error_status_ok)
            RAISE(rpc_x_ss_pipe_comm_error);
        /* And re-initialize the iovector */
        IDL_msp->IDL_elts_in_use = 0;
        /* If there is a stack packet, mark it as reusable */
        if (IDL_msp->IDL_stack_packet_addr != NULL)
            IDL_msp->IDL_stack_packet_status = IDL_stack_packet_unused_k;
    }
}

/******************************************************************************/
/*                                                                            */
/*  Open a new buffer                                                         */
/*  The usable length of the buffer is set up so that the last address        */
/*  in the data area is (7 mod 8)                                             */
/*                                                                            */
/******************************************************************************/
void rpc_ss_ndr_marsh_init_buffer
#ifdef IDL_PROTOTYPES
(
    IDL_msp_t IDL_msp
)
#else
( IDL_msp )
    IDL_msp_t IDL_msp;
#endif
{
    idl_byte *beyond_usable_buffer; /* (0 mod 8) address of byte beyond usable
                                        buffer area */

    if (IDL_msp->IDL_stack_packet_status == IDL_stack_packet_unused_k)
    {
        IDL_msp->IDL_buff_addr = IDL_msp->IDL_stack_packet_addr;
        IDL_msp->IDL_stack_packet_status = IDL_stack_packet_in_use_k;
        beyond_usable_buffer = (idl_byte *)
            (((IDL_msp->IDL_buff_addr + IDL_STACK_PACKET_SIZE)
                                                     - (idl_byte *)0) & (~7));
    }
    else if (IDL_msp->IDL_stack_packet_status == IDL_stack_packet_part_used_k)
    {
        /* IDL_mp was pointing at the first free location in the stack packet
            and has not been changed by the "marshall by pointing" */
        IDL_msp->IDL_buff_addr = IDL_msp->IDL_mp;
        IDL_msp->IDL_stack_packet_status = IDL_stack_packet_in_use_k;
        beyond_usable_buffer = (idl_byte *)
            (((IDL_msp->IDL_stack_packet_addr + IDL_STACK_PACKET_SIZE)
                                                     - (idl_byte *)0) & (~7));
    }
    else
    {
#ifndef _KERNEL
        if (IDL_msp->IDL_pickling_handle != NULL)
        {
            /* This is the only case where we may be doing an encoding */
            idl_ulong_int buff_size;

            idl_es_encode_init_buffer(&buff_size, IDL_msp);
            beyond_usable_buffer = (idl_byte *)
                                     (((IDL_msp->IDL_buff_addr + buff_size)
                                                     - (idl_byte *)0) & (~7));
        }
        else
#endif
        {
            IDL_msp->IDL_buff_addr = (idl_byte *)malloc(IDL_BUFF_SIZE);
            if (IDL_msp->IDL_buff_addr == NULL)
                RAISE(rpc_x_no_memory);
            beyond_usable_buffer = (idl_byte *)
                                     (((IDL_msp->IDL_buff_addr + IDL_BUFF_SIZE)
                                                     - (idl_byte *)0) & (~7));
        }
    }
    IDL_msp->IDL_data_addr = (idl_byte *)
                ((((IDL_msp->IDL_buff_addr - (idl_byte *)0) + 7) & (~7))
                                             + IDL_msp->IDL_mp_start_offset);
    IDL_msp->IDL_mp = IDL_msp->IDL_data_addr;
    IDL_msp->IDL_left_in_buff = beyond_usable_buffer - IDL_msp->IDL_data_addr;
}

/******************************************************************************/
/*                                                                            */
/*  Marshall an array by making an iovector element point at it               */
/*                                                                            */
/******************************************************************************/
void rpc_ss_ndr_marsh_by_pointing
#ifdef IDL_PROTOTYPES
(
    /* [in] */  idl_ulong_int element_count,
    /* [in] */  idl_ulong_int element_size,
    /* [in] */  rpc_void_p_t array_addr,
    IDL_msp_t IDL_msp
)
#else
(element_count, element_size, array_addr, IDL_msp)
    idl_ulong_int element_count;
    idl_ulong_int element_size;
    rpc_void_p_t array_addr;
    IDL_msp_t IDL_msp;
#endif
{
    rpc_iovector_elt_t *p_elt;
    idl_ulong_int array_size_in_bytes;

    /* Close the current buffer if one is open */
    if (IDL_msp->IDL_buff_addr != NULL)
    {
        if ((IDL_msp->IDL_stack_packet_status == IDL_stack_packet_in_use_k)
            && (IDL_msp->IDL_left_in_buff >= 8))
        {
            /* Can use the rest of the stack packet later */
            IDL_msp->IDL_stack_packet_status = IDL_stack_packet_part_used_k;
        }
        rpc_ss_attach_buff_to_iovec( IDL_msp );
        rpc_ss_xmit_iovec_if_necess( idl_false, IDL_msp );
        IDL_msp->IDL_left_in_buff = 0;
        IDL_msp->IDL_mp_start_offset = (IDL_msp->IDL_mp - (idl_byte *)0) % 8;
    }

    p_elt = &(IDL_msp->IDL_iovec.elt[IDL_msp->IDL_elts_in_use]);
    array_size_in_bytes = element_count * element_size;
    p_elt->buff_dealloc = NULL_FREE_RTN;
    if ( (IDL_msp->IDL_side == IDL_server_side_k)
         || IDL_msp->IDL_marsh_pipe
         || (IDL_msp->IDL_m_xmit_level > 0) )
    {
        /* Run time must copy the data before server stub stack is unwound
            or pipe or [transmit_as] buffer is reused */
        p_elt->flags = rpc_c_iovector_elt_reused;
    }
    else
        p_elt->flags = 0;
    p_elt->buff_addr = (byte_p_t)array_addr;
    p_elt->buff_len = array_size_in_bytes;
    p_elt->data_addr = (byte_p_t)array_addr;
    p_elt->data_len = array_size_in_bytes;
    (IDL_msp->IDL_elts_in_use)++;
    rpc_ss_xmit_iovec_if_necess( idl_true, IDL_msp );
    IDL_msp->IDL_mp_start_offset =
                     (IDL_msp->IDL_mp_start_offset + array_size_in_bytes) % 8;
}

/******************************************************************************/
/*                                                                            */
/*  If we have an array whose base type is such that it could be marshalled   */
/*  by pointing, but its size is below the threshold level for having its own */
/*  iovector_elt, marshall it by block copying into one or more buffers       */
/*                                                                            */
/******************************************************************************/
void rpc_ss_ndr_marsh_by_copying
#ifdef IDL_PROTOTYPES
(
    /* [in] */  idl_ulong_int element_count,
    /* [in] */  idl_ulong_int element_size,
    /* [in] */  rpc_void_p_t array_addr,
    IDL_msp_t IDL_msp
)
#else
(element_count, element_size, array_addr, IDL_msp)
    idl_ulong_int element_count;
    idl_ulong_int element_size;
    rpc_void_p_t array_addr;
    IDL_msp_t IDL_msp;
#endif
{
    idl_ulong_int bytes_required;   /* Number of bytes left to copy */
    idl_ulong_int bytes_to_copy;  /* Number of bytes to copy into this buffer */

    bytes_required = element_count * element_size;
    while (bytes_required != 0)
    {
        rpc_ss_ndr_marsh_check_buffer( 1, IDL_msp);
        if (bytes_required > IDL_msp->IDL_left_in_buff)
            bytes_to_copy = IDL_msp->IDL_left_in_buff;
        else
            bytes_to_copy = bytes_required;
        memcpy(IDL_msp->IDL_mp, array_addr, bytes_to_copy);
        IDL_msp->IDL_mp += bytes_to_copy;
        IDL_msp->IDL_left_in_buff -= bytes_to_copy;
        bytes_required -= bytes_to_copy;
        array_addr = (rpc_void_p_t)((idl_byte *)array_addr + bytes_to_copy);
    }
}

/******************************************************************************/
/*                                                                            */
/*  Marshall a structure                                                      */
/*                                                                            */
/******************************************************************************/
void rpc_ss_ndr_marsh_struct
#ifdef IDL_PROTOTYPES
(
    /* [in] */  idl_byte struct_type,   /* DT_FIXED_STRUCT, DT_CONF_STRUCT 
                                            or DT_V1_CONF_STRUCT */
    /* [in] */  idl_ulong_int defn_index,
    /* [in] */  rpc_void_p_t struct_addr,
    IDL_msp_t IDL_msp
)
#else
(struct_type, defn_index, struct_addr, IDL_msp)
    idl_byte struct_type;
    idl_ulong_int defn_index;
    rpc_void_p_t struct_addr;
    IDL_msp_t IDL_msp;
#endif
{
    idl_ulong_int offset_index;
    idl_byte *defn_vec_ptr;
    idl_ulong_int *struct_offset_vec_ptr; /* Start of offsets for this struct */
    idl_ulong_int *offset_vec_ptr;
    idl_byte type_byte;
    idl_ulong_int offset;
    idl_ulong_int field_defn_index;
    idl_byte *field_defn_ptr;
    idl_ulong_int conf_dims;    /* Number of dimensions of conformance info */
    IDL_bound_pair_t *conf_bounds;   /* Bounds list from conformance info */
    IDL_bound_pair_t normal_conf_bounds[IDL_NORMAL_DIMS];
    idl_ulong_int *Z_values;
    idl_ulong_int normal_Z_values[IDL_NORMAL_DIMS];
    IDL_bound_pair_t *range_list;
    IDL_bound_pair_t normal_range_list[IDL_NORMAL_DIMS];
    idl_boolean v1_array = idl_false;
    idl_ulong_int node_number;
    long already_marshalled;
    idl_ulong_int switch_index; /* Index of switch field for non-encapsulated
                                                                        union */
    idl_ushort_int v1_size;     /* Number of elements in open [v1_array] */
    idl_boolean add_null;
#ifndef _KERNEL
    idl_ulong_int shadow_length;    /* Number of elements in a cs_shadow */
    IDL_cs_shadow_elt_t *cs_shadow = NULL;
#endif
    idl_ulong_int i;

    defn_vec_ptr = IDL_msp->IDL_type_vec + defn_index;
    IDL_GET_LONG_FROM_VECTOR(offset_index,defn_vec_ptr);
    struct_offset_vec_ptr = IDL_msp->IDL_offset_vec + offset_index;
    offset_vec_ptr = struct_offset_vec_ptr + 1;
                                        /* Skip over size at start of offsets */

    if ( (struct_type == IDL_DT_CONF_STRUCT)
        || (struct_type == IDL_DT_V1_CONF_STRUCT) )
    {
        /* Next integer in the definition vector is index to a fully flattened 
            array rep for the conformant array field */
        IDL_GET_LONG_FROM_VECTOR(field_defn_index,defn_vec_ptr);
        field_defn_ptr = IDL_msp->IDL_type_vec + field_defn_index;
        conf_dims = (idl_ulong_int)*field_defn_ptr;
        if (conf_dims > IDL_NORMAL_DIMS)
        {
            conf_bounds = NULL;
            Z_values = NULL;
        }
        else
        {
            conf_bounds = normal_conf_bounds;
            Z_values = normal_Z_values;
        }
        field_defn_ptr++;
#ifndef _KERNEL
        if (*defn_vec_ptr == IDL_DT_CS_SHADOW)
        {
            /* Need to set up I-char machinery */
            defn_vec_ptr++;
            IDL_GET_LONG_FROM_VECTOR(shadow_length, defn_vec_ptr);
            rpc_ss_ndr_m_struct_cs_shadow(struct_addr, struct_type,
                                          shadow_length, offset_index, 
                                          defn_vec_ptr, &cs_shadow, IDL_msp);
            if (*(field_defn_ptr
                 + conf_dims * IDL_CONF_BOUND_PAIR_WIDTH) == IDL_DT_CS_TYPE)
            {
                rpc_ss_conf_struct_cs_bounds(field_defn_ptr, cs_shadow,
                                         conf_bounds, IDL_msp);
            }
            else
                rpc_ss_build_bounds_list( &field_defn_ptr, NULL, struct_addr,
                    struct_offset_vec_ptr, conf_dims, &conf_bounds, IDL_msp );
        }
        else
#endif
            rpc_ss_build_bounds_list( &field_defn_ptr, NULL, struct_addr,
                    struct_offset_vec_ptr, conf_dims, &conf_bounds, IDL_msp );
        rpc_ss_Z_values_from_bounds(conf_bounds, conf_dims, &Z_values,IDL_msp);
#ifndef _KERNEL
        if (struct_type == IDL_DT_V1_CONF_STRUCT)
        {
            v1_size = 1;
            for (i=0; i<conf_dims; i++)
                v1_size *= (idl_ushort_int)Z_values[i];
            IDL_MARSH_CUSHORT( &v1_size );
        }
        else
#endif
            rpc_ss_ndr_marsh_Z_values( conf_dims, Z_values, IDL_msp );
    }

    do {
        type_byte = *defn_vec_ptr;
        defn_vec_ptr++;
        switch(type_byte)
        {
#ifndef _KERNEL
            case IDL_DT_CS_SHADOW:
                /* I-char machinery  - struct has varying array(s) of [cs_char]
                    but no conformant array of [cs_char] */
                IDL_GET_LONG_FROM_VECTOR(shadow_length, defn_vec_ptr);
                rpc_ss_ndr_m_struct_cs_shadow(struct_addr, struct_type,
                                          shadow_length, offset_index, 
                                          defn_vec_ptr, &cs_shadow, IDL_msp);
                break;
#endif
            case IDL_DT_BYTE:
                offset = *offset_vec_ptr;
                offset_vec_ptr++;
                IDL_MARSH_BYTE( (idl_byte *)struct_addr + offset );
                break;
            case IDL_DT_CHAR:
                offset = *offset_vec_ptr;
                offset_vec_ptr++;
                IDL_MARSH_CHAR( (idl_byte *)struct_addr + offset );
                break;
            case IDL_DT_BOOLEAN:
                offset = *offset_vec_ptr;
                offset_vec_ptr++;
                IDL_MARSH_BOOLEAN( (idl_byte *)struct_addr + offset );
                break;
#ifndef _KERNEL
            case IDL_DT_DOUBLE:
                offset = *offset_vec_ptr;
                offset_vec_ptr++;
                IDL_MARSH_DOUBLE( (idl_byte *)struct_addr + offset );
                break;
#endif
            case IDL_DT_ENUM:
                offset = *offset_vec_ptr;
                offset_vec_ptr++;
                IDL_MARSH_ENUM( (idl_byte *)struct_addr + offset );
                break;
#ifndef _KERNEL
            case IDL_DT_FLOAT:
                offset = *offset_vec_ptr;
                offset_vec_ptr++;
                IDL_MARSH_FLOAT( (idl_byte *)struct_addr + offset );
                break;
#endif
            case IDL_DT_SMALL:
                offset = *offset_vec_ptr;
                offset_vec_ptr++;
                IDL_MARSH_SMALL( (idl_byte *)struct_addr + offset );
                break;
            case IDL_DT_SHORT:
                offset = *offset_vec_ptr;
                offset_vec_ptr++;
                IDL_MARSH_SHORT( (idl_byte *)struct_addr + offset );
                break;
            case IDL_DT_LONG:
                offset = *offset_vec_ptr;
                offset_vec_ptr++;
                IDL_MARSH_LONG( (idl_byte *)struct_addr + offset );
                break;
#ifndef _KERNEL
            case IDL_DT_HYPER:
                offset = *offset_vec_ptr;
                offset_vec_ptr++;
                IDL_MARSH_HYPER( (idl_byte *)struct_addr + offset );
                break;
#endif
            case IDL_DT_USMALL:
                offset = *offset_vec_ptr;
                offset_vec_ptr++;
                IDL_MARSH_USMALL( (idl_byte *)struct_addr + offset );
                break;
            case IDL_DT_USHORT:
                offset = *offset_vec_ptr;
                offset_vec_ptr++;
                IDL_MARSH_USHORT( (idl_byte *)struct_addr + offset );
                break;
            case IDL_DT_ULONG:
                offset = *offset_vec_ptr;
                offset_vec_ptr++;
                IDL_MARSH_ULONG( (idl_byte *)struct_addr + offset );
                break;
#ifndef _KERNEL
            case IDL_DT_UHYPER:
                offset = *offset_vec_ptr;
                offset_vec_ptr++;
                IDL_MARSH_UHYPER( (idl_byte *)struct_addr + offset );
                break;
#endif
            case IDL_DT_ERROR_STATUS:
                offset = *offset_vec_ptr;
                offset_vec_ptr++;
                IDL_MARSH_ERROR_STATUS( (idl_byte *)struct_addr + offset );
                break;
#ifndef _KERNEL
            case IDL_DT_V1_ENUM:
                offset = *offset_vec_ptr;
                offset_vec_ptr++;
                IDL_MARSH_V1_ENUM( (idl_byte *)struct_addr + offset );
                break;
#endif
            case IDL_DT_FIXED_ARRAY:
                defn_vec_ptr++;     /* Skip over properties byte */
                IDL_DISCARD_LONG_FROM_VECTOR(defn_vec_ptr);
                                                    /* Full array definition */
                IDL_GET_LONG_FROM_VECTOR(field_defn_index, defn_vec_ptr);
                offset = *offset_vec_ptr;
                offset_vec_ptr++;
                rpc_ss_ndr_marsh_fixed_arr( field_defn_index,
                            (idl_byte *)struct_addr+offset, 0, IDL_msp);
                break;
            case IDL_DT_VARYING_ARRAY:
                defn_vec_ptr++;     /* Skip over properties byte */
                IDL_DISCARD_LONG_FROM_VECTOR(defn_vec_ptr);
                                                    /* Full array definition */
                IDL_GET_LONG_FROM_VECTOR(field_defn_index, defn_vec_ptr);
                offset = *offset_vec_ptr;
                offset_vec_ptr++;
                rpc_ss_ndr_marsh_varying_arr(field_defn_index,
                        (rpc_void_p_t)((idl_byte *)struct_addr+offset),
                        struct_addr, struct_offset_vec_ptr,
                        (v1_array ? IDL_M_V1_ARRAY : 0),
                        IDL_msp);
                v1_array = idl_false;
                break;
            case IDL_DT_CONF_ARRAY:
                defn_vec_ptr++;     /* Skip over properties byte */
                IDL_DISCARD_LONG_FROM_VECTOR(defn_vec_ptr);
                                                    /* Full array definition */
                IDL_GET_LONG_FROM_VECTOR(field_defn_index, defn_vec_ptr);
                offset = *offset_vec_ptr;   /* Must be last field of struct */
                field_defn_ptr = IDL_msp->IDL_type_vec + field_defn_index
                                   + 1; /* We already know the dimensionality */
                IDL_ADV_DEFN_PTR_OVER_BOUNDS( field_defn_ptr, conf_dims );
                rpc_ss_ndr_m_fix_or_conf_arr(
                        (rpc_void_p_t)((idl_byte *)struct_addr+offset),
                        conf_dims, conf_bounds, field_defn_ptr,
                        IDL_M_CONF_ARRAY, IDL_msp);
                if (conf_dims > IDL_NORMAL_DIMS)
                {
                    rpc_ss_mem_item_free(&IDL_msp->IDL_mem_handle,
                                         (byte_p_t)Z_values);
                    rpc_ss_mem_item_free(&IDL_msp->IDL_mem_handle,
                                         (byte_p_t)conf_bounds);
                }
                break;
            case IDL_DT_OPEN_ARRAY:
                defn_vec_ptr++;     /* Skip over properties byte */
                IDL_DISCARD_LONG_FROM_VECTOR(defn_vec_ptr);
                                                    /* Full array definition */
                IDL_GET_LONG_FROM_VECTOR(field_defn_index, defn_vec_ptr);
                offset = *offset_vec_ptr;   /* Must be last field of struct */
                field_defn_ptr = IDL_msp->IDL_type_vec + field_defn_index
                                   + 1; /* We already know the dimensionality */
                IDL_ADV_DEFN_PTR_OVER_BOUNDS( field_defn_ptr, conf_dims );
                if (conf_dims > IDL_NORMAL_DIMS)
                    range_list = NULL;
                else
                    range_list = normal_range_list;
                rpc_ss_build_range_list( &field_defn_ptr,
                            (rpc_void_p_t)((idl_byte *)struct_addr+offset),
                             struct_addr, struct_offset_vec_ptr, conf_dims,
                             conf_bounds, &range_list, &add_null, IDL_msp );
                rpc_ss_ndr_m_var_or_open_arr(
                        (rpc_void_p_t)((idl_byte *)struct_addr+offset),
                        Z_values, conf_dims, range_list, field_defn_ptr,
                        ((v1_array ? IDL_M_V1_ARRAY : 0) | IDL_M_CONF_ARRAY
                            | (add_null ? IDL_M_ADD_NULL : 0)),
                        IDL_msp);
                if (conf_dims > IDL_NORMAL_DIMS)
                {
                    rpc_ss_mem_item_free(&IDL_msp->IDL_mem_handle,
                                            (byte_p_t)range_list);
                    rpc_ss_mem_item_free(&IDL_msp->IDL_mem_handle,
                                            (byte_p_t)Z_values);
                    rpc_ss_mem_item_free(&IDL_msp->IDL_mem_handle,
                                            (byte_p_t)conf_bounds);
                }
                v1_array = false;
                break;
            case IDL_DT_ENC_UNION:
                defn_vec_ptr++;     /* Skip over properties byte */
                IDL_GET_LONG_FROM_VECTOR(field_defn_index, defn_vec_ptr);
                offset = *offset_vec_ptr;
                offset_vec_ptr++;
                rpc_ss_ndr_m_enc_union_or_ptees( (idl_byte *)struct_addr+offset,
                                          field_defn_index, idl_false, IDL_msp);
                break;
#ifndef _KERNEL
            case IDL_DT_N_E_UNION:
                defn_vec_ptr++;     /* Skip over properties byte */
                IDL_GET_LONG_FROM_VECTOR(switch_index, defn_vec_ptr);
                IDL_GET_LONG_FROM_VECTOR(field_defn_index, defn_vec_ptr);
                offset = *offset_vec_ptr;
                offset_vec_ptr++;
                rpc_ss_ndr_m_n_e_union_or_ptees( (idl_byte *)struct_addr+offset,
                                             switch_index, field_defn_index,
                                             struct_addr, struct_offset_vec_ptr,
                                             idl_false, IDL_msp);
                break;
#endif
            case IDL_DT_FULL_PTR:
                defn_vec_ptr++;     /* Properties byte */
                IDL_DISCARD_LONG_FROM_VECTOR(defn_vec_ptr);
                                                /* Pointee definition */
                offset = *offset_vec_ptr;
                offset_vec_ptr++;
                node_number = rpc_ss_register_node( IDL_msp->IDL_node_table,
                        *(byte_p_t *)((idl_byte *)struct_addr+offset),
                        ndr_false, &already_marshalled );

                IDL_MARSH_ULONG( &node_number );
                break;
#ifndef _KERNEL
            case IDL_DT_UNIQUE_PTR:
                defn_vec_ptr++;     /* Properties byte */
                IDL_DISCARD_LONG_FROM_VECTOR(defn_vec_ptr);
                                                /* Pointee definition */
                offset = *offset_vec_ptr;
                offset_vec_ptr++;
                /* Get a value of 0 if pointer is null, 1 otherwise */
                node_number = 
                        (*(byte_p_t *)((idl_byte *)struct_addr+offset) != NULL);

                IDL_MARSH_ULONG( &node_number );
                break;
#endif
            case IDL_DT_REF_PTR:
                defn_vec_ptr++;     /* Properties byte */
                IDL_DISCARD_LONG_FROM_VECTOR(defn_vec_ptr);
                                                /* Pointee definition */
                offset_vec_ptr++;
                /* Aligned 4-byte place holder */
                IDL_MARSH_ALIGN_MP( IDL_msp, 4 );
                rpc_ss_ndr_marsh_check_buffer( 4, IDL_msp );
                IDL_msp->IDL_mp += 4;
                IDL_msp->IDL_left_in_buff -= 4;
                break;
            case IDL_DT_IGNORE:
                offset_vec_ptr++;
                /* Aligned 4-byte place holder */
                IDL_MARSH_ALIGN_MP( IDL_msp, 4 );
                rpc_ss_ndr_marsh_check_buffer( 4, IDL_msp );
                IDL_msp->IDL_mp += 4;
                IDL_msp->IDL_left_in_buff -= 4;
                break;
            case IDL_DT_STRING:
                /* Varying/open array code will do the right thing */
                break;
            case IDL_DT_TRANSMIT_AS:
            case IDL_DT_REPRESENT_AS:
                defn_vec_ptr++;     /* Skip over properties byte */
                IDL_GET_LONG_FROM_VECTOR(field_defn_index, defn_vec_ptr);
                offset = *offset_vec_ptr;
                offset_vec_ptr++;
                rpc_ss_ndr_marsh_xmit_as(field_defn_index,
                                       (idl_byte *)struct_addr+offset, IDL_msp);
                break;
            case IDL_DT_INTERFACE:
                defn_vec_ptr++;     /* Skip over properties byte */
                IDL_GET_LONG_FROM_VECTOR(field_defn_index, defn_vec_ptr);
                offset = *offset_vec_ptr;
                offset_vec_ptr++;
                rpc_ss_ndr_marsh_interface(field_defn_index,
                                    (idl_byte *)struct_addr+offset, IDL_msp);
                break;
#ifndef _KERNEL
            case IDL_DT_V1_ARRAY:
                v1_array = idl_true;
                break;
            case IDL_DT_V1_STRING:
                defn_vec_ptr += 2;  /* DT_VARYING_ARRAY and properties */
                IDL_DISCARD_LONG_FROM_VECTOR(defn_vec_ptr);
                                                        /* Full array defn */
                IDL_DISCARD_LONG_FROM_VECTOR(defn_vec_ptr);
                                                      /* Flattened array defn */
                offset = *offset_vec_ptr;
                offset_vec_ptr++;
                rpc_ss_ndr_marsh_v1_string((idl_byte *)struct_addr+offset,
                                                       0, IDL_msp);
                break;
            case IDL_DT_CS_TYPE:
                defn_vec_ptr++;     /* Skip over properties byte */
                IDL_GET_LONG_FROM_VECTOR(field_defn_index, defn_vec_ptr);
                offset = *offset_vec_ptr;
                offset_vec_ptr++;
                rpc_ss_ndr_marsh_cs_char((idl_byte *)struct_addr+offset,
                                         field_defn_index, IDL_msp);
                break;
            case IDL_DT_CS_ATTRIBUTE:
                /* Is followed by an array attribute, which is an integer */
                rpc_ss_ndr_marsh_scalar(*defn_vec_ptr,
                        (rpc_void_p_t)&cs_shadow[offset_vec_ptr
                                        -(struct_offset_vec_ptr+1)].IDL_data,
                        IDL_msp);
                defn_vec_ptr++;     /* Attribute type */
                offset_vec_ptr++;
                break;
            case IDL_DT_CS_ARRAY:
                /* Is followed by an array description */
                offset = *offset_vec_ptr;
                rpc_ss_ndr_marsh_cs_array(
                        (rpc_void_p_t)((idl_byte *)struct_addr+offset),
                        cs_shadow,
                        offset_vec_ptr-(struct_offset_vec_ptr+1),
                        idl_true, &defn_vec_ptr, IDL_msp);
                offset_vec_ptr++;
                break;
            case IDL_DT_CS_RLSE_SHADOW:
                rpc_ss_ndr_m_rlse_cs_shadow(cs_shadow, shadow_length, IDL_msp);
                break;
#endif
            case IDL_DT_NDR_ALIGN_2:
                IDL_MARSH_ALIGN_MP( IDL_msp, 2 );
                break;
            case IDL_DT_NDR_ALIGN_4:
                IDL_MARSH_ALIGN_MP( IDL_msp, 4 );
                break;
            case IDL_DT_NDR_ALIGN_8:
                IDL_MARSH_ALIGN_MP( IDL_msp, 8 );
                break;
            case IDL_DT_BEGIN_NESTED_STRUCT:
            case IDL_DT_END_NESTED_STRUCT:
            case IDL_DT_EOL:
                break;
            default:
#ifdef DEBUG_INTERP
                printf("rpc_ss_ndr_marsh_struct:unrecognized type %d\n",
                        type_byte);
                exit(0);
#endif
                RAISE(rpc_x_coding_error);
        }
    } while (type_byte != IDL_DT_EOL);
}

/******************************************************************************/
/*                                                                            */
/*  Marshall a contiguous set of elements one by one                          */
/*                                                                            */
/******************************************************************************/
void rpc_ss_ndr_marsh_by_looping
#ifdef IDL_PROTOTYPES
(
    /* [in] */  idl_ulong_int element_count,
    /* [in] */  idl_byte base_type,
    /* [in] */  rpc_void_p_t array_addr,
    /* [in] */  idl_ulong_int element_size,
                                /* Used if array of constructed type */
    /* [in] */  idl_ulong_int element_defn_index,
                                /* Used if array of constructed type */
    IDL_msp_t IDL_msp
)
#else
(element_count, base_type, array_addr, element_size, element_defn_index,
  IDL_msp)
    idl_ulong_int element_count;
    idl_byte base_type;
    rpc_void_p_t array_addr;
    idl_ulong_int element_size;
    idl_ulong_int element_defn_index;
    IDL_msp_t IDL_msp;
#endif
{
    idl_ulong_int i;
    idl_ulong_int node_number;
    long already_marshalled;

    for (i=0; i<element_count; i++)
    {
        switch (base_type)
        {
            case IDL_DT_BOOLEAN:
                IDL_MARSH_BOOLEAN( array_addr );
                array_addr = (rpc_void_p_t)((idl_boolean *)(array_addr) + 1);
                break;
            case IDL_DT_BYTE:
                IDL_MARSH_BYTE( array_addr );
                array_addr = (rpc_void_p_t)((idl_byte *)(array_addr) + 1);
                break;
            case IDL_DT_CHAR:
                IDL_MARSH_CHAR( array_addr );
                array_addr = (rpc_void_p_t)((idl_char *)(array_addr) + 1);
                break;
#ifndef _KERNEL
            case IDL_DT_DOUBLE:
                IDL_MARSH_DOUBLE( array_addr );
                array_addr = (rpc_void_p_t)((idl_long_float *)(array_addr) + 1);
                break;
#endif
            case IDL_DT_ENUM:
                IDL_MARSH_ENUM( array_addr );
                array_addr = (rpc_void_p_t)((int *)(array_addr) + 1);
                break;
#ifndef _KERNEL
            case IDL_DT_V1_ENUM:
                IDL_MARSH_V1_ENUM( array_addr );
                array_addr = (rpc_void_p_t)((int *)(array_addr) + 1);
                break;
            case IDL_DT_FLOAT:
                IDL_MARSH_FLOAT( array_addr );
                array_addr = (rpc_void_p_t)
                                         ((idl_short_float *)(array_addr) + 1);
                break;
#endif
            case IDL_DT_SMALL:
                IDL_MARSH_SMALL( array_addr );
                array_addr = (rpc_void_p_t)((idl_small_int *)(array_addr) + 1);
                break;
            case IDL_DT_SHORT:
                IDL_MARSH_SHORT( array_addr );
                array_addr = (rpc_void_p_t)((idl_short_int *)(array_addr) + 1);
                break;
            case IDL_DT_LONG:
                IDL_MARSH_LONG( array_addr );
                array_addr = (rpc_void_p_t)((idl_long_int *)(array_addr) + 1);
                break;
#ifndef _KERNEL
            case IDL_DT_HYPER:
                IDL_MARSH_HYPER( array_addr );
                array_addr = (rpc_void_p_t)((idl_hyper_int *)(array_addr) + 1);
                break;
#endif
            case IDL_DT_USMALL:
                IDL_MARSH_USMALL( array_addr );
                array_addr = (rpc_void_p_t)((idl_usmall_int *)(array_addr) + 1);
                break;
            case IDL_DT_USHORT:
                IDL_MARSH_USHORT( array_addr );
                array_addr = (rpc_void_p_t)((idl_ushort_int *)(array_addr) + 1);
                break;
            case IDL_DT_ULONG:
                IDL_MARSH_ULONG( array_addr );
                array_addr = (rpc_void_p_t)((idl_ulong_int *)(array_addr) + 1);
                break;
            case IDL_DT_ERROR_STATUS:
                IDL_MARSH_ERROR_STATUS( array_addr );
                array_addr = (rpc_void_p_t)((idl_ulong_int *)(array_addr) + 1);
                break;
#ifndef _KERNEL
            case IDL_DT_UHYPER:
                IDL_MARSH_UHYPER( array_addr );
                array_addr = (rpc_void_p_t)((idl_uhyper_int *)(array_addr) + 1);
                break;
#endif
            case IDL_DT_FIXED_STRUCT:
                rpc_ss_ndr_marsh_struct( base_type, element_defn_index,
                                                        array_addr, IDL_msp);
                array_addr = (rpc_void_p_t)
                                    ((idl_byte *)(array_addr) + element_size);
                break;
            case IDL_DT_FIXED_ARRAY:
                /* Base type of pipe is array */
                rpc_ss_ndr_marsh_fixed_arr( element_defn_index, array_addr,
                                            0, IDL_msp );
                array_addr = (rpc_void_p_t)
                                    ((idl_byte *)(array_addr) + element_size);
                break;
            case IDL_DT_ENC_UNION:
                rpc_ss_ndr_m_enc_union_or_ptees( array_addr, element_defn_index,
                                                    idl_false, IDL_msp );
                array_addr = (rpc_void_p_t)
                                    ((idl_byte *)(array_addr) + element_size);
                break;
            case IDL_DT_FULL_PTR:
                node_number = rpc_ss_register_node( IDL_msp->IDL_node_table,
                                            *(byte_p_t *)array_addr,
                                            ndr_false, &already_marshalled );

                IDL_MARSH_ULONG( &node_number );
                array_addr = (rpc_void_p_t)((rpc_void_p_t *)(array_addr) + 1);
                break;
#ifndef _KERNEL
            case IDL_DT_UNIQUE_PTR:
                /* Get a value of 0 if pointer is null, 1 otherwise */
                node_number = (*(byte_p_t *)array_addr != NULL);

                IDL_MARSH_ULONG( &node_number );
                array_addr = (rpc_void_p_t)((rpc_void_p_t *)(array_addr) + 1);
                break;
#endif
            case IDL_DT_STRING:
#if defined(PACKED_BYTE_ARRAYS) && defined(PACKED_CHAR_ARRAYS) && defined(PACKED_SCALAR_ARRAYS)
                if (IDL_msp->IDL_language == IDL_lang_c_k ||
                    IDL_msp->IDL_language == IDL_lang_cxx_k
		   )
                {
                    idl_byte *element_defn_ptr;
                    idl_ulong_int A=0, B;
                    idl_ulong_int base_type_size;

                    element_defn_ptr = IDL_msp->IDL_type_vec
                                        + element_defn_index
                                        + 1     /* Dimensionality */
                                        + IDL_FIXED_BOUND_PAIR_WIDTH
                                        + IDL_DATA_LIMIT_PAIR_WIDTH;
                    /* Now pointing at base type of string */
                    base_type_size = rpc_ss_type_size(element_defn_ptr,
                                                                     IDL_msp);
                    if (base_type_size == 1)
                        B = strlen(array_addr) + 1;
                    else
                        B = rpc_ss_strsiz((idl_char *)array_addr,
                                                               base_type_size);
                    IDL_MARSH_ALIGN_MP(IDL_msp, 4);
                    rpc_ss_ndr_marsh_check_buffer(8, IDL_msp);
                    rpc_marshall_ulong_int(IDL_msp->IDL_mp, A);
                    IDL_msp->IDL_mp += 4;
                    rpc_marshall_ulong_int(IDL_msp->IDL_mp, B);
                    IDL_msp->IDL_mp += 4;
                    IDL_msp->IDL_left_in_buff -= 8;
                    rpc_ss_ndr_marsh_by_copying(B, base_type_size, array_addr,
                                                IDL_msp);
                }
                else
#endif
                {
                    rpc_ss_ndr_marsh_varying_arr(element_defn_index, array_addr,
                                                 NULL, NULL, 0, IDL_msp);
                }
                array_addr = (rpc_void_p_t)
                                    ((idl_byte *)(array_addr) + element_size);
                break;
#ifndef _KERNEL
            case IDL_DT_V1_STRING:
                rpc_ss_ndr_marsh_v1_string(array_addr, 0, IDL_msp);
                array_addr = (rpc_void_p_t)
                                    ((idl_byte *)(array_addr) + element_size);
                break;
#endif
            case IDL_DT_TRANSMIT_AS:
            case IDL_DT_REPRESENT_AS:
                rpc_ss_ndr_marsh_xmit_as( element_defn_index, array_addr,
                                                                    IDL_msp );
                array_addr = (rpc_void_p_t)
                                    ((idl_byte *)(array_addr) + element_size);
                break;
            case IDL_DT_INTERFACE:
                rpc_ss_ndr_marsh_interface( element_defn_index, array_addr,
                                                   IDL_msp );
                array_addr = (rpc_void_p_t)
                                    ((idl_byte *)(array_addr) + element_size);
                break;
            default:
#ifdef DEBUG_INTERP
                printf(
                      "rpc_ss_ndr_marsh_by_looping:unrecognized type %d\n",
                        base_type);
                exit(0);
#endif
                RAISE(rpc_x_coding_error);
        }
    }
}

/******************************************************************************/
/*                                                                            */
/* Marshall a fixed or conformant array                                       */
/*                                                                            */
/******************************************************************************/
void rpc_ss_ndr_m_fix_or_conf_arr
#ifdef IDL_PROTOTYPES
(
    /* [in] */  rpc_void_p_t array_addr,
    /* [in] */  idl_ulong_int dimensionality,
    /* [in] */  IDL_bound_pair_t *bounds_list,
    /* [in] */  idl_byte *defn_vec_ptr, /* Points at array base info */
    /* [in] */  idl_ulong_int flags,
    IDL_msp_t IDL_msp
)
#else
( array_addr, dimensionality, bounds_list, defn_vec_ptr, flags, IDL_msp )
    rpc_void_p_t array_addr;
    idl_ulong_int dimensionality;
    IDL_bound_pair_t *bounds_list;
    idl_byte *defn_vec_ptr;
    idl_ulong_int flags;
    IDL_msp_t IDL_msp;
#endif
{
    idl_ulong_int element_count;
    int i;
    idl_ulong_int element_defn_index;
    idl_ulong_int element_offset_index;
    idl_ulong_int element_size;
    idl_byte *element_defn_ptr;
    idl_byte base_type;
    idl_boolean marshall_by_pointing;
    IDL_bound_pair_t *bound_p;	    /* Steps through bounds */

    if ( (*defn_vec_ptr == IDL_DT_STRING)
        || (*defn_vec_ptr == IDL_DT_V1_STRING) )
    {
        /* Arrays of strings have a special representation */
        dimensionality--;
    }

    /* Calculate the total number of elements to be transmited */
    element_count = 1;
    bound_p = bounds_list;
    for (i=dimensionality; i>0; i--)
    {
        element_count *= (bound_p->upper - bound_p->lower + 1);
	bound_p++;
    }
    if (element_count == 0)
        return;

    rpc_ss_ndr_arr_align_and_opt( IDL_marshalling_k, dimensionality, 
                                  &base_type, defn_vec_ptr,
                                  &marshall_by_pointing, IDL_msp );

    if (base_type == IDL_DT_REF_PTR)
    {
        return;
    }


    /* Marshall by pointing if possible and enough elements or chunk of pipe of
         array (only case where array base type is DT_FIXED_ARRAY) */
    if (marshall_by_pointing)
    {
        element_size = rpc_ss_type_size( defn_vec_ptr, IDL_msp );
        if (base_type != IDL_DT_FIXED_STRUCT)
        {
	    /*
	     * 	Marshall by pointing only if the object is at least
	     * 	IDL_POINT_THRESHOLD items in size, or it is the base type of a
	     * 	pipe (array of fixed array).  Since there is only a single pipe
	     * 	chunk per transmit, may as well point at it no matter its size.
             *  Can never marshall by pointing when pickling
	     */
            if (((element_count >= IDL_POINT_THRESHOLD)
                                    || (base_type == IDL_DT_FIXED_ARRAY))
                    && (! IDL_M_FLAGS_TEST(flags, IDL_M_DO_NOT_POINT))
#ifndef _KERNEL
                    && (IDL_msp->IDL_pickling_handle == NULL)
#endif
                )
                rpc_ss_ndr_marsh_by_pointing(element_count, element_size,
                                         array_addr, IDL_msp);
            else
                rpc_ss_ndr_marsh_by_copying(element_count, element_size,
                                         array_addr, IDL_msp);
	    return;
	}
	else  /* For NDR-aligned fixed structures */
	{
            /*
	     * 	The last element of the structure cannot, in general, be
	     * 	marshalled via the point-at optimization because of trailing
	     * 	pad is returned from the sizeof() function that is not included
	     * 	in NDR.
	     */
            element_count--;        /* So do the last one separately */

	    /* If there are more that 1, do them by pointing */
	    if (element_count != 0)
	    {
		if (((element_count*element_size) >= IDL_POINT_THRESHOLD)
                        && (! IDL_M_FLAGS_TEST(flags, IDL_M_DO_NOT_POINT))
#ifndef _KERNEL
                        && (IDL_msp->IDL_pickling_handle == NULL)
#endif
                    )
		    rpc_ss_ndr_marsh_by_pointing(element_count, element_size,
					     array_addr, IDL_msp);
		else
		    rpc_ss_ndr_marsh_by_copying(element_count, element_size,
					     array_addr, IDL_msp);
	    }

            /* Marshall the last element separately */
            defn_vec_ptr += 2;  /* See comment below */
            IDL_GET_LONG_FROM_VECTOR( element_defn_index, defn_vec_ptr );
            rpc_ss_ndr_marsh_struct(base_type, element_defn_index,
                                    (rpc_void_p_t)((idl_byte *)array_addr
                                         + element_count * element_size),
                                    IDL_msp);
        }
        return;
    }

    if ( (base_type == IDL_DT_FIXED_STRUCT)
         || (base_type == IDL_DT_ENC_UNION)
         || (base_type == IDL_DT_TRANSMIT_AS)
         || (base_type == IDL_DT_REPRESENT_AS) )
    {
        defn_vec_ptr += 2;  /* Discard type and properties */
        /* If we are marshalling an array, not a pipe, defn_vec_ptr was
            4-byte aligned and DT_MODIFIED and modifier are discarded
            by the +=2 followed by GET_LONG */
        IDL_GET_LONG_FROM_VECTOR( element_defn_index, defn_vec_ptr );
        element_defn_ptr =  IDL_msp->IDL_type_vec + element_defn_index;
        IDL_GET_LONG_FROM_VECTOR(element_offset_index, element_defn_ptr);
        element_size = *(IDL_msp->IDL_offset_vec + element_offset_index);
    }
    else if ( (base_type == IDL_DT_STRING)
            || (base_type == IDL_DT_V1_STRING) )
    {
        rpc_ss_get_string_base_desc( defn_vec_ptr, &element_size,
                                               &element_defn_index, IDL_msp );
    }
    else if (base_type == IDL_DT_FIXED_ARRAY)
    {
        /* Base type of pipe is array */
        element_size = rpc_ss_type_size( defn_vec_ptr, IDL_msp );
        defn_vec_ptr += 2;  /* Base type and properties byte */
        IDL_DISCARD_LONG_FROM_VECTOR( defn_vec_ptr );   /* Full array defn */
        IDL_GET_LONG_FROM_VECTOR( element_defn_index, defn_vec_ptr );
    }
    else if (base_type == IDL_DT_INTERFACE)
    {
        element_size = sizeof(void *);
        defn_vec_ptr += 2;  /* Base type and properties byte */
        IDL_GET_LONG_FROM_VECTOR( element_defn_index, defn_vec_ptr );
    }

    /* element_index, element_defn_ptr may not be set, but in these cases
        the procedure calls following do not use them */
        rpc_ss_ndr_marsh_by_looping(element_count,
              base_type, array_addr, element_size, element_defn_index, IDL_msp);
}

/******************************************************************************/
/*                                                                            */
/*  Marshall a fixed array                                                    */
/*                                                                            */
/******************************************************************************/
void rpc_ss_ndr_marsh_fixed_arr
#ifdef IDL_PROTOTYPES
(
    /* [in] */  idl_ulong_int defn_index,
    /* [in] */  rpc_void_p_t array_addr,
    /* [in] */  idl_ulong_int flags,
    IDL_msp_t IDL_msp
)
#else
(defn_index, array_addr, flags, IDL_msp)
    idl_ulong_int defn_index;
    rpc_void_p_t array_addr;
    idl_ulong_int flags;
    IDL_msp_t IDL_msp;
#endif
{
    idl_byte *defn_vec_ptr;
    idl_ulong_int dimensionality;
    IDL_bound_pair_t *bounds_list;

    defn_vec_ptr = IDL_msp->IDL_type_vec + defn_index; 
    dimensionality = (idl_ulong_int)*defn_vec_ptr;
    defn_vec_ptr++;
    /* By design defn_vec_ptr is now aligned (0 mod 4) */
#if (NDR_LOCAL_INT_REP == ndr_c_int_big_endian)
    rpc_ss_fixed_bounds_from_vector(dimensionality, defn_vec_ptr, &bounds_list,
                                    IDL_msp);
#else
    bounds_list = (IDL_bound_pair_t *)defn_vec_ptr;
#endif
    rpc_ss_ndr_m_fix_or_conf_arr( array_addr, dimensionality, bounds_list,
                    defn_vec_ptr + dimensionality * IDL_FIXED_BOUND_PAIR_WIDTH,
                    flags, IDL_msp );
#if (NDR_LOCAL_INT_REP == ndr_c_int_big_endian)
    rpc_ss_mem_item_free(&IDL_msp->IDL_mem_handle, (byte_p_t)bounds_list);
#endif
}

/******************************************************************************/
/*                                                                            */
/* Marshall A,B pairs                                                         */
/*                                                                            */
/******************************************************************************/
static void rpc_ss_ndr_marsh_A_B_pairs
#ifdef IDL_PROTOTYPES
(
    /* [in] */  idl_ulong_int dimensionality,
    /* [in] */  IDL_bound_pair_t *range_list,
    IDL_msp_t IDL_msp
)
#else
(dimensionality, range_list, IDL_msp)
    idl_ulong_int dimensionality;
    IDL_bound_pair_t *range_list;
    IDL_msp_t IDL_msp;
#endif
{
    idl_ulong_int i;
    idl_long_int A,B;

    for (i=0; i<dimensionality; i++)
    {
        A = range_list[i].lower;
        IDL_MARSH_ULONG( &A );
        B = range_list[i].upper - A;
        IDL_MARSH_ULONG( &B );
    }
}

/******************************************************************************/
/*                                                                            */
/* Marshall a varying or open array                                           */
/*                                                                            */
/******************************************************************************/
static idl_byte null_byte = 0;

void rpc_ss_ndr_m_var_or_open_arr
#ifdef IDL_PROTOTYPES
(
    /* [in] */  rpc_void_p_t array_addr,
    /* [in] */  idl_ulong_int *Z_values,
    /* [in] */  idl_ulong_int dimensionality,
    /* [in] */  IDL_bound_pair_t *range_list,
    /* [in] */  idl_byte *defn_vec_ptr, /* On entry points at array base info */
    /* [in] */ idl_ulong_int flags,
    IDL_msp_t IDL_msp
)
#else
( array_addr, Z_values, dimensionality, range_list, defn_vec_ptr, flags,
 IDL_msp )
    rpc_void_p_t array_addr;
    idl_ulong_int *Z_values;
    idl_ulong_int dimensionality;
    IDL_bound_pair_t *range_list;
    idl_byte *defn_vec_ptr;
    idl_ulong_int flags;
    IDL_msp_t IDL_msp;
#endif
{
    idl_ulong_int element_defn_index;
    idl_ulong_int element_size;
    idl_byte base_type;
    IDL_varying_control_t *control_data;    /* List of data used to control
                                                marshalling loops */
    IDL_varying_control_t normal_control_data[IDL_NORMAL_DIMS];
    int i;
    idl_byte *inner_slice_address;  /* Address of 1-dim subset of array */
    int dim;    /* Index through array dimensions */
    idl_boolean contiguous;
    idl_ulong_int element_count;
    idl_boolean marshall_by_pointing;
    rpc_void_p_t point_addr;    /* Address to be used in marsh by pointing */
    idl_ushort_int v1_count;

    if ( (*defn_vec_ptr == IDL_DT_STRING)
        || (*defn_vec_ptr == IDL_DT_V1_STRING) )
    {
        /* Arrays of strings have a special representation */
        dimensionality--;
    }


#ifndef _KERNEL
    if (IDL_M_FLAGS_TEST(flags, IDL_M_V1_ARRAY))
    {
        /* Marshall one short word of varyingness info */
        v1_count =  1;
        for ( i=0; i<dimensionality; i++)
        {
            v1_count *= (idl_ushort_int)
                                  (range_list[i].upper - range_list[i].lower);
        }
        IDL_MARSH_CUSHORT( &v1_count );
        if (v1_count == 0)
        {
            if ( rpc_ss_bug_1_thru_31(IDL_BUG_1|IDL_BUG_2, IDL_msp) )
            {
                rpc_ss_ndr_arr_align_and_opt( IDL_marshalling_k, dimensionality,
                                              &base_type, defn_vec_ptr,
                                              &marshall_by_pointing, IDL_msp );
                if ( rpc_ss_bug_1_thru_31(IDL_BUG_1, IDL_msp) 
                        && ( (base_type == IDL_DT_FIXED_STRUCT)
                                || (base_type == IDL_DT_ENC_UNION)
                                || (base_type == IDL_DT_TRANSMIT_AS) ) )
                {
                    /* -bug 1 and non-scalar base type for [v1_array] */
                    idl_ulong_int bug_1_align;
                    bug_1_align = rpc_ss_ndr_bug_1_align(defn_vec_ptr, IDL_msp);
                    IDL_MARSH_ALIGN_MP( IDL_msp, bug_1_align );
                }
            }
            return;
        }
    }
    else
#endif
    {
        if (IDL_M_FLAGS_TEST(flags, IDL_M_ADD_NULL))
        {
            /* This is a one-dimensional array and the count of elements
                to be marshalled does not include the trailing null */
            (range_list[0].upper)++;
        }
        rpc_ss_ndr_marsh_A_B_pairs(dimensionality, range_list, IDL_msp);
        if (IDL_M_FLAGS_TEST(flags, IDL_M_ADD_NULL))
        {
            (range_list[0].upper)--;
        }
        for ( i=0; i<dimensionality; i++)
        {
            if (range_list[i].upper ==  range_list[i].lower)
            {
                /* No elements to marshall */
                return;
            }
        }
    }

    rpc_ss_ndr_arr_align_and_opt( IDL_marshalling_k, dimensionality, &base_type,
                                defn_vec_ptr, &marshall_by_pointing, IDL_msp );
    if ((IDL_M_FLAGS_TEST(flags, IDL_M_ADD_NULL))
                    || (IDL_M_FLAGS_TEST(flags, IDL_M_DO_NOT_POINT)))
        marshall_by_pointing = idl_false;

    if (base_type == IDL_DT_REF_PTR)
    {
        return;
    }
    else if ( (base_type == IDL_DT_STRING)
            || (base_type == IDL_DT_V1_STRING) )
        rpc_ss_get_string_base_desc(defn_vec_ptr, &element_size,
                                    &element_defn_index, IDL_msp);
    else
        element_size = rpc_ss_type_size(defn_vec_ptr, IDL_msp); 

    if (marshall_by_pointing)
    {
        point_addr = array_addr;
        rpc_ss_ndr_contiguous_elt( dimensionality, Z_values, range_list,
                                   element_size, &contiguous, &element_count,
                                   &point_addr );
        if (contiguous)
        {
            if (base_type != IDL_DT_FIXED_STRUCT)
            {
                if ((element_count >= IDL_POINT_THRESHOLD)
#ifndef _KERNEL
                            && (IDL_msp->IDL_pickling_handle == NULL)
#endif
                    )
                    rpc_ss_ndr_marsh_by_pointing(element_count, element_size,
                                             point_addr, IDL_msp);
                else
                    rpc_ss_ndr_marsh_by_copying(element_count, element_size,
                                             point_addr, IDL_msp);
		return;
	    }
	    else /* For NDR-aligned fixed structures */
	    {
		/*
		 * 	The last element of the structure cannot, in general, be
		 * 	marshalled via the point-at optimization because of trailing
		 * 	pad is returned from the sizeof() function that is not included
		 * 	in NDR.
		 */
                element_count--;        /* So do the last one separately */

		/* If there are more that 1, do them by pointing */
		if (element_count != 0)
		{
		    if (((element_count*element_size) >= IDL_POINT_THRESHOLD)
#ifndef _KERNEL
                            && (IDL_msp->IDL_pickling_handle == NULL)
#endif
                        )
			rpc_ss_ndr_marsh_by_pointing(element_count, element_size,
						 point_addr, IDL_msp);
		    else
			rpc_ss_ndr_marsh_by_copying(element_count, element_size,
						 point_addr, IDL_msp);
		}

                /* Marshall the last element separately */
                defn_vec_ptr += 4;  /* See comment below */
                IDL_GET_LONG_FROM_VECTOR( element_defn_index, defn_vec_ptr );
                rpc_ss_ndr_marsh_struct(base_type, element_defn_index,
                                    (rpc_void_p_t)((idl_byte *)point_addr
                                             + element_count * element_size),
                                    IDL_msp);
            }
            return;
        }
    }


    if ( (base_type == IDL_DT_FIXED_STRUCT)
         || (base_type == IDL_DT_ENC_UNION)
         || (base_type == IDL_DT_TRANSMIT_AS)
         || (base_type == IDL_DT_REPRESENT_AS) )
    {
        /* Discard any of DT_MODIFIED, type, modifier, properties
            which may be present, and any filler bytes */
        defn_vec_ptr += 4;
        IDL_GET_LONG_FROM_VECTOR( element_defn_index, defn_vec_ptr );
    }


    if (dimensionality > IDL_NORMAL_DIMS)
    {
        control_data = (IDL_varying_control_t *)rpc_ss_mem_alloc(
                                &IDL_msp->IDL_mem_handle,
                                dimensionality * sizeof(IDL_varying_control_t));
    }
    else
        control_data = normal_control_data;
    control_data[dimensionality-1].subslice_size = element_size;
    control_data[dimensionality-1].index_value = 
                                            range_list[dimensionality-1].lower;
    for (i=dimensionality-2; i>=0; i--)
    {
        control_data[i].index_value = range_list[i].lower;
        control_data[i].subslice_size = control_data[i+1].subslice_size
                                                            * Z_values[i+1];   
    }

    do {
        inner_slice_address = (idl_byte *)array_addr;
        for (i=0; i<dimensionality; i++)
        {
            inner_slice_address += control_data[i].index_value
                                        * control_data[i].subslice_size;
        }
        rpc_ss_ndr_marsh_by_looping(
                 range_list[dimensionality-1].upper
                                         - range_list[dimensionality-1].lower,
                 base_type, (rpc_void_p_t)inner_slice_address, 
                 element_size, element_defn_index, IDL_msp);
        dim = dimensionality - 2;
        while (dim >= 0)
        {
            control_data[dim].index_value++;
            if (control_data[dim].index_value < 
                                        (idl_ulong_int)range_list[dim].upper)
                break;
            control_data[dim].index_value = range_list[dim].lower;
            dim--;
        }
    } while (dim >= 0);

    if (dimensionality > IDL_NORMAL_DIMS)
        rpc_ss_mem_item_free(&IDL_msp->IDL_mem_handle, (byte_p_t)control_data);

    if (IDL_M_FLAGS_TEST(flags, IDL_M_ADD_NULL))
    {
        /* Number of null bytes required is character width */
        for (i=0; i<element_size; i++)
        {
            IDL_MARSH_BYTE( &null_byte );
        }
    }
}

/******************************************************************************/
/*                                                                            */
/*  Marshall a varying array                                                  */
/*                                                                            */
/******************************************************************************/
void rpc_ss_ndr_marsh_varying_arr
#ifdef IDL_PROTOTYPES
(
    /* [in] */  idl_ulong_int defn_index,
                                    /* Index of start of array description */
    /* [in] */  rpc_void_p_t array_addr,
    /* [in] */  rpc_void_p_t struct_addr,   /* Address of structure array is
                                part of. NULL if array is not structure field */
    /* [in] */  idl_ulong_int *struct_offset_vec_ptr,
                                      /* NULL if array is not structure field */
    /* [in] */ idl_ulong_int flags,
    IDL_msp_t IDL_msp
)
#else
(defn_index, array_addr, struct_addr, struct_offset_vec_ptr, flags,
 IDL_msp)
    idl_ulong_int defn_index;
    rpc_void_p_t array_addr;
    idl_ulong_int *struct_offset_vec_ptr;
    rpc_void_p_t struct_addr;
    idl_ulong_int flags;
    IDL_msp_t IDL_msp;
#endif
{
    idl_byte *defn_vec_ptr;
    idl_ulong_int dimensionality;
    IDL_bound_pair_t *bounds_list;
    idl_ulong_int *Z_values;
    idl_ulong_int normal_Z_values[IDL_NORMAL_DIMS];
    IDL_bound_pair_t *range_list;
    IDL_bound_pair_t normal_range_list[IDL_NORMAL_DIMS];
    idl_boolean add_null;
    rpc_void_p_t array_data_addr; /* May need to decode descriptor to get
                                        the address of the array data */

    defn_vec_ptr = IDL_msp->IDL_type_vec + defn_index; 
    dimensionality = (idl_ulong_int)*defn_vec_ptr;
    defn_vec_ptr++;
    /* By design we are now longword aligned */
#if (NDR_LOCAL_INT_REP == ndr_c_int_big_endian)
    rpc_ss_fixed_bounds_from_vector(dimensionality, defn_vec_ptr, &bounds_list,
                                    IDL_msp);
#else
    bounds_list = (IDL_bound_pair_t *)defn_vec_ptr;
#endif
    if (dimensionality > IDL_NORMAL_DIMS)
    {
        Z_values = NULL;
        range_list = NULL;
    }
    else
    {
        Z_values = normal_Z_values;
        range_list = normal_range_list;
    }
    rpc_ss_Z_values_from_bounds( bounds_list, dimensionality, &Z_values,
                                                                     IDL_msp );
    defn_vec_ptr += dimensionality * IDL_FIXED_BOUND_PAIR_WIDTH;
    /* The address of the array data is only needed when a range list is
        being built for [string] data */
        array_data_addr = array_addr;

    rpc_ss_build_range_list( &defn_vec_ptr, array_data_addr, struct_addr,
                        struct_offset_vec_ptr, dimensionality, bounds_list, 
                        &range_list, &add_null, IDL_msp );
    rpc_ss_ndr_m_var_or_open_arr( array_addr, Z_values, dimensionality,
                                  range_list, defn_vec_ptr,
                                  flags | (add_null ? IDL_M_ADD_NULL : 0),
                                  IDL_msp );
    if (dimensionality > IDL_NORMAL_DIMS)
    {
        rpc_ss_mem_item_free(&IDL_msp->IDL_mem_handle, (byte_p_t)range_list);
        rpc_ss_mem_item_free(&IDL_msp->IDL_mem_handle, (byte_p_t)Z_values);
    }
#if (NDR_LOCAL_INT_REP == ndr_c_int_big_endian)
    rpc_ss_mem_item_free(&IDL_msp->IDL_mem_handle, (byte_p_t)bounds_list);
#endif
}

/******************************************************************************/
/*                                                                            */
/*  Marshall the Z values for a conformant or open array                      */
/*                                                                            */
/******************************************************************************/
void rpc_ss_ndr_marsh_Z_values
#ifdef IDL_PROTOTYPES
(
    /* [in] */  idl_ulong_int dimensionality,
    /* [in] */  idl_ulong_int *Z_values,
    IDL_msp_t IDL_msp
)
#else
(dimensionality, Z_values, IDL_msp)
    idl_ulong_int dimensionality;
    idl_ulong_int *Z_values;
    IDL_msp_t IDL_msp;
#endif
{
    idl_ulong_int i;

    for (i=0; i<dimensionality; i++)
    {
        IDL_MARSH_ULONG( &Z_values[i] );
    }
}

/******************************************************************************/
/*                                                                            */
/*  Marshall a conformant array parameter                                     */
/*                                                                            */
/******************************************************************************/
void rpc_ss_ndr_marsh_conf_arr
#ifdef IDL_PROTOTYPES
(
    /* [in] */  idl_ulong_int defn_index,
    /* [in] */  rpc_void_p_t array_addr,
    IDL_msp_t IDL_msp
)
#else
(defn_index, array_addr, IDL_msp)
    idl_ulong_int defn_index;
    rpc_void_p_t array_addr;
    IDL_msp_t IDL_msp;
#endif
{
    idl_byte *defn_vec_ptr;
    idl_ulong_int dimensionality;
    IDL_bound_pair_t *bounds_list;
    IDL_bound_pair_t normal_bounds_list[IDL_NORMAL_DIMS];
    idl_ulong_int *Z_values;
    idl_ulong_int normal_Z_values[IDL_NORMAL_DIMS];

    defn_vec_ptr = IDL_msp->IDL_type_vec + defn_index; 
    dimensionality = (idl_ulong_int)*defn_vec_ptr;
    defn_vec_ptr++;
    if (dimensionality > IDL_NORMAL_DIMS)
    {
        bounds_list = NULL;
        Z_values = NULL;
    }
    else
    {
        bounds_list = normal_bounds_list;
        Z_values = normal_Z_values;
    }
    rpc_ss_build_bounds_list( &defn_vec_ptr, array_addr, NULL, NULL,
                              dimensionality, &bounds_list, IDL_msp );
    rpc_ss_Z_values_from_bounds(bounds_list, dimensionality, &Z_values,IDL_msp);
    rpc_ss_ndr_marsh_Z_values( dimensionality, Z_values, IDL_msp );
    rpc_ss_ndr_m_fix_or_conf_arr( array_addr, dimensionality, bounds_list,
                                defn_vec_ptr, IDL_M_IS_PARAM | IDL_M_CONF_ARRAY,
                                 IDL_msp );
    if (dimensionality > IDL_NORMAL_DIMS)
    {
        rpc_ss_mem_item_free(&IDL_msp->IDL_mem_handle, (byte_p_t)Z_values);
        rpc_ss_mem_item_free(&IDL_msp->IDL_mem_handle, (byte_p_t)bounds_list);
    }
}

/******************************************************************************/
/*                                                                            */
/*  Marshall an open array                                                    */
/*                                                                            */
/******************************************************************************/
void rpc_ss_ndr_marsh_open_arr
#ifdef IDL_PROTOTYPES
(
    /* [in] */  idl_ulong_int defn_index,
    /* [in] */  rpc_void_p_t array_addr,
    /* [in] */ idl_ulong_int flags,
    IDL_msp_t IDL_msp
)
#else
(defn_index, array_addr, flags, IDL_msp)
    idl_ulong_int defn_index;
    rpc_void_p_t array_addr;
    idl_ulong_int flags;
    IDL_msp_t IDL_msp;
#endif
{
    idl_byte *defn_vec_ptr;
    idl_ulong_int dimensionality;
    IDL_bound_pair_t *bounds_list;
    IDL_bound_pair_t normal_bounds_list[IDL_NORMAL_DIMS];
    idl_ulong_int *Z_values;
    idl_ulong_int normal_Z_values[IDL_NORMAL_DIMS];
    IDL_bound_pair_t *range_list;
    IDL_bound_pair_t normal_range_list[IDL_NORMAL_DIMS];
    idl_ushort_int v1_size;
    idl_boolean add_null;
    rpc_void_p_t array_data_addr; /* May need to decode descriptor to get
                                        the address of the array_data */
    idl_ulong_int i;

    defn_vec_ptr = IDL_msp->IDL_type_vec + defn_index; 
    dimensionality = (idl_ulong_int)*defn_vec_ptr;
    defn_vec_ptr++;
    if (dimensionality > IDL_NORMAL_DIMS)
    {
        bounds_list = NULL;
        Z_values = NULL;
        range_list = NULL;
    }
    else
    {
        bounds_list = normal_bounds_list;
        Z_values = normal_Z_values;
        range_list = normal_range_list;
    }
    {
        array_data_addr = array_addr;
    }
    rpc_ss_build_bounds_list( &defn_vec_ptr, array_data_addr, NULL, NULL,
                                    dimensionality, &bounds_list, IDL_msp );
    rpc_ss_Z_values_from_bounds( bounds_list, dimensionality, &Z_values,
                                                                     IDL_msp );
    rpc_ss_build_range_list( &defn_vec_ptr, array_data_addr, NULL,
                        NULL, dimensionality, bounds_list, 
                        &range_list, &add_null, IDL_msp );
#ifndef _KERNEL
    if (IDL_M_FLAGS_TEST(flags, IDL_M_V1_ARRAY))
    {
        v1_size = 1;
        for ( i=0; i<dimensionality; i++ )
            v1_size *= (idl_ushort_int)Z_values[i];
        IDL_MARSH_CUSHORT( &v1_size );
    }
    else
#endif
        rpc_ss_ndr_marsh_Z_values( dimensionality, Z_values, IDL_msp );
    rpc_ss_ndr_m_var_or_open_arr( array_addr, Z_values, dimensionality,
                                  range_list, defn_vec_ptr,
                                  flags | (add_null ? IDL_M_ADD_NULL : 0),
                                  IDL_msp );
    if (dimensionality > IDL_NORMAL_DIMS)
    {
        rpc_ss_mem_item_free(&IDL_msp->IDL_mem_handle, (byte_p_t)range_list);
        rpc_ss_mem_item_free(&IDL_msp->IDL_mem_handle, (byte_p_t)Z_values);
        rpc_ss_mem_item_free(&IDL_msp->IDL_mem_handle, (byte_p_t)bounds_list);
    }
}

/******************************************************************************/
/*                                                                            */
/*  Discard the description following DT_ALLOCATE_REF                         */
/*                                                                            */
/******************************************************************************/
void rpc_ss_discard_allocate_ref
#ifdef IDL_PROTOTYPES
(
    /* [in,out] */ idl_byte **p_type_vec_ptr
                            /* On entry type_vec_ptr points to type of parameter
                                         It is advanced over type definition */
)
#else
( p_type_vec_ptr )
    idl_byte **p_type_vec_ptr;
#endif
{
    idl_byte type_byte;
    idl_byte *type_vec_ptr = *p_type_vec_ptr;

    type_byte = *type_vec_ptr;
    type_vec_ptr++;
    switch (type_byte)
    {
        case IDL_DT_FIXED_STRUCT:
            type_vec_ptr++;     /* Ignore properties byte */
            IDL_DISCARD_LONG_FROM_VECTOR( type_vec_ptr );   /* Struct defn */
            break;
        case IDL_DT_FIXED_ARRAY:
        case IDL_DT_VARYING_ARRAY:
            type_vec_ptr++;     /* Ignore properties byte */
            IDL_DISCARD_LONG_FROM_VECTOR( type_vec_ptr );  /* Full array defn */
            IDL_DISCARD_LONG_FROM_VECTOR( type_vec_ptr );  /* Flat array defn */
            break;
        case IDL_DT_REF_PTR:
            type_vec_ptr++;     /* Ignore properties byte */
            IDL_DISCARD_LONG_FROM_VECTOR( type_vec_ptr );   /* Pointee defn */
            break;
        default:
#ifdef DEBUG_INTERP
            printf("rpc_ss_discard_allocate_ref:unrecognized type %d\n",
                        type_byte);
            exit(0);
#endif
            RAISE(rpc_x_coding_error);
    }
    *p_type_vec_ptr = type_vec_ptr;
}


/******************************************************************************/
/*                                                                            */
/*  Control for marshalling                                                   */
/*                                                                            */
/******************************************************************************/
void rpc_ss_ndr_marsh_interp
#ifdef IDL_PROTOTYPES
(
    idl_ulong_int IDL_parameter_count, /* [in] -- Number of parameters to   */
                                  /* marshall in this call to the           */
                                  /* interpreter                            */

    idl_ulong_int IDL_type_index,    /* [in] -- Offset into the type vector */
                               /* for the start of the parameter descriptions */

    rpc_void_p_t IDL_param_vector[], /* [in,out] -- The addresses of each of */
                                  /* the the parameters thus it's size is   */
                                  /* the number of parameters in the        */
                                  /* signature of the operation             */

    IDL_msp_t IDL_msp        /* [in,out] -- Pointer to marshalling state   */
)
#else
( IDL_parameter_count, IDL_type_index, IDL_param_vector, IDL_msp)
    idl_ulong_int IDL_parameter_count;
    idl_ulong_int IDL_type_index;
    rpc_void_p_t IDL_param_vector[];
    IDL_msp_t IDL_msp;
#endif
{
    idl_byte *type_vec_ptr;
    idl_byte type_byte;
    idl_ulong_int param_index;
    rpc_void_p_t param_addr;
    idl_ulong_int defn_index;
    idl_ulong_int rtn_index;
    idl_ulong_int iid_index;
    idl_boolean type_has_pointers;
    idl_boolean v1_array = idl_false;
    idl_boolean is_string = idl_false;
    idl_ulong_int node_number;
    long already_marshalled;
    idl_byte *pointee_defn_ptr;
    IDL_pointee_desc_t pointee_desc;    /* Description of pointee */
    idl_ulong_int switch_index; /* Index of switch param for non-encapsulated
                                                                        union */
    idl_ulong_int routine_index; /* Index of rtn to be used to free referents
                       of [in]-only [transmit_as] or [represent_as] parameter */
#ifndef _KERNEL
    IDL_cs_shadow_elt_t *cs_shadow;     /* cs-shadow for parameter list */
    idl_ulong_int shadow_length;
#endif

    IDL_msp->IDL_marsh_pipe = idl_false;
    IDL_msp->IDL_m_xmit_level = 0;
    type_vec_ptr = (IDL_msp->IDL_type_vec) + IDL_type_index;

    /* Loop over parameters */
    for ( ; IDL_parameter_count > 0; IDL_parameter_count -- )
    {
        IDL_GET_LONG_FROM_VECTOR(param_index,type_vec_ptr);
        param_addr = IDL_param_vector[param_index];
        do {
            type_byte = *type_vec_ptr;
            type_vec_ptr++;
            switch(type_byte)
            {
                case IDL_DT_BYTE:
                    IDL_MARSH_BYTE( param_addr );
                    break;
                case IDL_DT_CHAR:
                    IDL_MARSH_CHAR( param_addr );
                    break;
                case IDL_DT_BOOLEAN:
                    IDL_MARSH_BOOLEAN( param_addr );
                    break;
#ifndef _KERNEL
                case IDL_DT_DOUBLE:
                    IDL_MARSH_DOUBLE( param_addr );
                    break;
#endif
                case IDL_DT_ENUM:
                    IDL_MARSH_ENUM( param_addr );
                    break;
#ifndef _KERNEL
                case IDL_DT_FLOAT:
                    IDL_MARSH_FLOAT( param_addr );
                    break;
#endif
                case IDL_DT_SMALL:
                    IDL_MARSH_SMALL( param_addr );
                    break;
                case IDL_DT_SHORT:
                    IDL_MARSH_SHORT( param_addr );
                    break;
                case IDL_DT_LONG:
                    IDL_MARSH_LONG( param_addr );
                    break;
#ifndef _KERNEL
                case IDL_DT_HYPER:
                    IDL_MARSH_HYPER( param_addr );
                    break;
#endif
                case IDL_DT_USMALL:
                    IDL_MARSH_USMALL( param_addr );
                    break;
                case IDL_DT_USHORT:
                    IDL_MARSH_USHORT( param_addr );
                    break;
                case IDL_DT_ULONG:
                    IDL_MARSH_ULONG( param_addr );
                    break;
#ifndef _KERNEL
                case IDL_DT_UHYPER:
                    IDL_MARSH_UHYPER( param_addr );
                    break;
                case IDL_DT_V1_ENUM:
                    IDL_MARSH_V1_ENUM( param_addr );
                    break;
#endif
                case IDL_DT_ERROR_STATUS:
                    IDL_MARSH_ERROR_STATUS( param_addr );
                    break;
                case IDL_DT_FIXED_STRUCT:
                case IDL_DT_CONF_STRUCT:
                case IDL_DT_V1_CONF_STRUCT:
                    /* Properties byte */
                    type_has_pointers =
                             IDL_PROP_TEST(*type_vec_ptr, IDL_PROP_HAS_PTRS);
                    type_vec_ptr++;
                    IDL_GET_LONG_FROM_VECTOR(defn_index,type_vec_ptr);
                    rpc_ss_ndr_marsh_struct(type_byte, defn_index, param_addr,
                                                                     IDL_msp);
                    if (type_has_pointers)
                    {
                        rpc_ss_ndr_m_struct_pointees(type_byte, defn_index,
                                                     param_addr, IDL_msp);
                    }
                    break;
                case IDL_DT_FIXED_ARRAY:
                    /* Properties byte */
                    type_has_pointers =
                             IDL_PROP_TEST(*type_vec_ptr, IDL_PROP_HAS_PTRS);
                    type_vec_ptr++;
                    IDL_DISCARD_LONG_FROM_VECTOR(type_vec_ptr);
                                            /* Discard full array definition */
                    IDL_GET_LONG_FROM_VECTOR(defn_index,type_vec_ptr);
                    rpc_ss_ndr_marsh_fixed_arr(defn_index, param_addr, 
                                               IDL_M_IS_PARAM, IDL_msp);
                    if (type_has_pointers)
                    {
                        rpc_ss_ndr_m_dfc_arr_ptees(defn_index, param_addr,
                                                     NULL, NULL, 0, IDL_msp);
                    }
                    break;
                case IDL_DT_VARYING_ARRAY:
                    /* Properties byte */
                    type_has_pointers =
                             IDL_PROP_TEST(*type_vec_ptr, IDL_PROP_HAS_PTRS);
                    type_vec_ptr++;
                    IDL_DISCARD_LONG_FROM_VECTOR(type_vec_ptr);
                                            /* Discard full array definition */
                    IDL_GET_LONG_FROM_VECTOR(defn_index,type_vec_ptr);
                    rpc_ss_ndr_marsh_varying_arr(defn_index, param_addr,
                                         NULL, NULL,
                                         (v1_array ? IDL_M_V1_ARRAY : 0)
                                             | (is_string ? IDL_M_IS_STRING : 0)
                                             | IDL_M_IS_PARAM,
                                         IDL_msp);
                    if (type_has_pointers)
                    {
                        rpc_ss_ndr_m_dvo_arr_ptees(defn_index, param_addr,
                                                        NULL, NULL, 0, IDL_msp);
                    }
                    v1_array = idl_false;
                    is_string = idl_false;
                    break;
                case IDL_DT_CONF_ARRAY:
                    /* Properties byte */
                    type_has_pointers =
                             IDL_PROP_TEST(*type_vec_ptr, IDL_PROP_HAS_PTRS);
                    type_vec_ptr++;
                    IDL_DISCARD_LONG_FROM_VECTOR(type_vec_ptr);
                                            /* Discard full array definition */
                    IDL_GET_LONG_FROM_VECTOR(defn_index,type_vec_ptr);
                    rpc_ss_ndr_marsh_conf_arr(defn_index, param_addr,
                                                                     IDL_msp);
                    if (type_has_pointers)
                    {
                        rpc_ss_ndr_m_dfc_arr_ptees(defn_index, param_addr,
                                         NULL, NULL, IDL_M_CONF_ARRAY, IDL_msp);
                    }
                    break;
                case IDL_DT_OPEN_ARRAY:
                    /* Properties byte */
                    type_has_pointers =
                             IDL_PROP_TEST(*type_vec_ptr, IDL_PROP_HAS_PTRS);
                    type_vec_ptr++;
                    IDL_DISCARD_LONG_FROM_VECTOR(type_vec_ptr);
                                            /* Discard full array definition */
                    IDL_GET_LONG_FROM_VECTOR(defn_index,type_vec_ptr);
                    rpc_ss_ndr_marsh_open_arr(defn_index, param_addr,
                                         (v1_array ? IDL_M_V1_ARRAY : 0)
                                            | (is_string ? IDL_M_IS_STRING : 0)
                                            | IDL_M_IS_PARAM | IDL_M_CONF_ARRAY,
                                         IDL_msp);
                    if (type_has_pointers)
                    {
                        rpc_ss_ndr_m_dvo_arr_ptees(defn_index, param_addr,
                                         NULL, NULL, IDL_M_CONF_ARRAY, IDL_msp);
                    }
                    v1_array = idl_false;
                    is_string = idl_false;
                    break;
                case IDL_DT_ENC_UNION:
                    /* Properties byte */
                    type_has_pointers =
                             IDL_PROP_TEST(*type_vec_ptr, IDL_PROP_HAS_PTRS);
                    type_vec_ptr++;
                    IDL_GET_LONG_FROM_VECTOR(defn_index,type_vec_ptr);
                    rpc_ss_ndr_m_enc_union_or_ptees( param_addr, defn_index,
                                                     idl_false, IDL_msp);
                    if (type_has_pointers)
                    {
                        rpc_ss_ndr_m_enc_union_or_ptees( param_addr, defn_index,
                                                         idl_true, IDL_msp);
                    }
                    break;
#ifndef _KERNEL
                case IDL_DT_N_E_UNION:
                    /* Properties byte */
                    type_has_pointers =
                             IDL_PROP_TEST(*type_vec_ptr, IDL_PROP_HAS_PTRS);
                    type_vec_ptr++;
                    IDL_GET_LONG_FROM_VECTOR(switch_index,type_vec_ptr);
                    IDL_GET_LONG_FROM_VECTOR(defn_index,type_vec_ptr);
                    rpc_ss_ndr_m_n_e_union_or_ptees( param_addr, switch_index,
                                                        defn_index, NULL, NULL,
                                                        idl_false, IDL_msp);
                    if (type_has_pointers)
                    {
                        rpc_ss_ndr_m_n_e_union_or_ptees( param_addr, 
                                                 switch_index, defn_index, NULL,
                                                 NULL, idl_true, IDL_msp);
                    }
                    break;
#endif
                case IDL_DT_PASSED_BY_REF:
                    break;
                case IDL_DT_FULL_PTR:
                    type_vec_ptr++;     /* Properties byte */
                    IDL_GET_LONG_FROM_VECTOR(defn_index,type_vec_ptr);
                    node_number = rpc_ss_register_node( IDL_msp->IDL_node_table,
                                          (byte_p_t)*(rpc_void_p_t *)param_addr,
                                             idl_true, &already_marshalled );
                    IDL_MARSH_ULONG( &node_number );
                    if ( (*(rpc_void_p_t *)param_addr != NULL)
                                                     && !already_marshalled )
                    {

                        pointee_defn_ptr = IDL_msp->IDL_type_vec + defn_index;
                        pointee_desc.dimensionality = 0;
                        rpc_ss_pointee_desc_from_data( pointee_defn_ptr,
                                          (byte_p_t)*(rpc_void_p_t *)param_addr,
                                           NULL, NULL, &pointee_desc, IDL_msp );
                        rpc_ss_ndr_marsh_pointee( pointee_defn_ptr,
                                                    *(rpc_void_p_t *)param_addr,
                                                    idl_false, &pointee_desc,
                                                    IDL_msp );
                        rpc_ss_rlse_data_pointee_desc( &pointee_desc, IDL_msp );
                    }
                    break;
#ifndef _KERNEL
                case IDL_DT_UNIQUE_PTR:
                    type_vec_ptr++;     /* Properties byte */
                    IDL_GET_LONG_FROM_VECTOR(defn_index,type_vec_ptr);
                    node_number = (*(rpc_void_p_t *)param_addr != NULL);
                                                          /* 1 if not null */



                    IDL_MARSH_ULONG( &node_number );
                    if (*(rpc_void_p_t *)param_addr != NULL)
                    {
                        pointee_defn_ptr = IDL_msp->IDL_type_vec + defn_index;
                        pointee_desc.dimensionality = 0;
                        rpc_ss_pointee_desc_from_data( pointee_defn_ptr,
                                          (byte_p_t)*(rpc_void_p_t *)param_addr,
                                           NULL, NULL, &pointee_desc, IDL_msp );
                        rpc_ss_ndr_marsh_pointee( pointee_defn_ptr,
                                                    *(rpc_void_p_t *)param_addr,
                                                    idl_false, &pointee_desc,
                                                    IDL_msp );
                        rpc_ss_rlse_data_pointee_desc( &pointee_desc, IDL_msp );
                    }
                    break;
#endif
                case IDL_DT_REF_PTR:
                    type_vec_ptr++;     /* Properties byte */
                    IDL_GET_LONG_FROM_VECTOR(defn_index,type_vec_ptr);
                    pointee_defn_ptr = IDL_msp->IDL_type_vec + defn_index;
                    pointee_desc.dimensionality = 0;
                    rpc_ss_pointee_desc_from_data( pointee_defn_ptr,
                                          (byte_p_t)*(rpc_void_p_t *)param_addr,
                                           NULL, NULL, &pointee_desc, IDL_msp );
                    rpc_ss_ndr_marsh_pointee( pointee_defn_ptr,
                                                    *(rpc_void_p_t *)param_addr,
                                                    idl_false, &pointee_desc,
                                                    IDL_msp );
                    rpc_ss_rlse_data_pointee_desc( &pointee_desc, IDL_msp );
                    break;
                case IDL_DT_STRING:
                    /* Varying/open array code will do the right thing */
                    is_string = idl_true;
                    break;
                case IDL_DT_TRANSMIT_AS:
                case IDL_DT_REPRESENT_AS:
                    type_vec_ptr++;     /* Properties byte */
                    IDL_GET_LONG_FROM_VECTOR(defn_index,type_vec_ptr);
                    rpc_ss_ndr_marsh_xmit_as(defn_index, param_addr, IDL_msp);
                    break;
                case IDL_DT_INTERFACE:
                    type_vec_ptr++;     /* Properties byte */
                    IDL_GET_LONG_FROM_VECTOR(defn_index,type_vec_ptr);
                    rpc_ss_ndr_marsh_interface(defn_index, param_addr, IDL_msp);
                    break;
                case IDL_DT_DYN_INTERFACE:
                    type_vec_ptr++;     /* Properties byte */
                    IDL_GET_LONG_FROM_VECTOR(rtn_index,type_vec_ptr);
                    IDL_GET_LONG_FROM_VECTOR(iid_index,type_vec_ptr);
                    rpc_ss_ndr_marsh_dyn_interface(rtn_index, param_addr,
		   	 ((uuid_t*) IDL_param_vector[iid_index]), IDL_msp);
                    break;
                case IDL_DT_ALLOCATE:
                    /* Do nothing except move to next parameter */
                    if ((*type_vec_ptr == IDL_DT_STRING)
                                          || (*type_vec_ptr == IDL_DT_V1_ARRAY))
                        type_vec_ptr++;
                    type_vec_ptr += 2;     /* Array type and properties byte */
                    IDL_DISCARD_LONG_FROM_VECTOR(type_vec_ptr);
                                                        /* Full array defn */
                    IDL_DISCARD_LONG_FROM_VECTOR(type_vec_ptr);
                                                      /* Flattened array defn */
                    break;
                case IDL_DT_ALLOCATE_REF:
                    /* Do nothing except move to next parameter */
                    rpc_ss_discard_allocate_ref(&type_vec_ptr);
                    break;
                case IDL_DT_PIPE:
                    type_vec_ptr++;     /* Properties byte */
                    IDL_GET_LONG_FROM_VECTOR(defn_index,type_vec_ptr);
                    if (IDL_msp->IDL_side == IDL_client_side_k)
                        rpc_ss_ndr_marsh_pipe(defn_index, param_addr, IDL_msp);
                    break;
                case IDL_DT_IN_CONTEXT:
                case IDL_DT_IN_OUT_CONTEXT:
                case IDL_DT_OUT_CONTEXT:
                    rpc_ss_ndr_marsh_context(type_byte, param_addr, IDL_msp);
                    break;
#ifndef _KERNEL
                case IDL_DT_V1_ARRAY:
                    v1_array = idl_true;
                    break;
                case IDL_DT_V1_STRING:
                    type_vec_ptr += 2;  /* DT_VARYING_ARRAY and properties */
                    IDL_DISCARD_LONG_FROM_VECTOR(type_vec_ptr);
                                                        /* Full array defn */
                    IDL_DISCARD_LONG_FROM_VECTOR(type_vec_ptr);
                                                      /* Flattened array defn */
                    rpc_ss_ndr_marsh_v1_string(param_addr, IDL_M_IS_PARAM,
                                                                     IDL_msp);
                    break;
#endif
                case IDL_DT_FREE_REP:
                    IDL_GET_LONG_FROM_VECTOR(routine_index, type_vec_ptr);
                    if (IDL_msp->IDL_side == IDL_server_side_k)
                    {
                        /* Only applies on server side */
                        (*(IDL_msp->IDL_rtn_vec + routine_index))(param_addr);
                    }
                    break;
#ifndef _KERNEL
                case IDL_DT_DELETED_NODES:
                    rpc_ss_ndr_marsh_deletes(IDL_msp);
                    break;
                case IDL_DT_CS_ATTRIBUTE:
                    /* Is followed by the (integer) type of the attribute */
                    rpc_ss_ndr_marsh_scalar(*type_vec_ptr,
                            (rpc_void_p_t)&cs_shadow[param_index-1].IDL_data,
                            IDL_msp);
                    type_vec_ptr++;     /* Attribute type */
                    break;
                case IDL_DT_CS_TYPE:
                    type_vec_ptr++;     /* Properties byte */
                    IDL_GET_LONG_FROM_VECTOR(defn_index,type_vec_ptr);
                    rpc_ss_ndr_marsh_cs_char(param_addr, defn_index, IDL_msp);
                    break;
                case IDL_DT_CS_ARRAY:
                    /* Is followed by an array description */
                    rpc_ss_ndr_marsh_cs_array(param_addr, cs_shadow,
                            param_index-1, idl_false, &type_vec_ptr, IDL_msp);
                    break;

                case IDL_DT_CS_SHADOW:
                    IDL_GET_LONG_FROM_VECTOR(shadow_length, type_vec_ptr);
                    rpc_ss_ndr_m_param_cs_shadow(
                                type_vec_ptr, param_index, shadow_length,
                                &cs_shadow, IDL_msp);
                    break;
                case IDL_DT_CS_RLSE_SHADOW:
                    rpc_ss_ndr_m_rlse_cs_shadow(cs_shadow, IDL_parameter_count,
                                                                     IDL_msp);
                    break;
#endif
                case IDL_DT_EOL:
                    break;
                default:
#ifdef DEBUG_INTERP
                    printf("rpc_ss_ndr_marsh_interp:unrecognized type %d\n",
                        type_byte);
                    exit(0);
#endif
                    RAISE(rpc_x_coding_error);
            }
        } while (type_byte != IDL_DT_EOL);
    }

    /* For pickling, round the buffer out to a multiple of 8 bytes */
    if (IDL_msp->IDL_pickling_handle != NULL)
    {
        IDL_MARSH_ALIGN_MP(IDL_msp, 8);
    }

    /* If there is a current buffer, attach it to the iovector */
    if (IDL_msp->IDL_buff_addr != NULL)
    {
        rpc_ss_attach_buff_to_iovec( IDL_msp );
    }
    /* Make the iovector ready for the rpc_call_transceive call */
    IDL_msp->IDL_iovec.num_elt = (idl_ushort_int)IDL_msp->IDL_elts_in_use;
    /* And nothing else needs to be cleaned up */
    IDL_msp->IDL_elts_in_use = 0;
}
