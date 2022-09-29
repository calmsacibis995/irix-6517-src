/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(_KERNEL)
static const char *_identString = "$Id: ndrui.c,v 65.5 1998/03/23 17:24:42 gwehrman Exp $";
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
 * $Log: ndrui.c,v $
 * Revision 65.5  1998/03/23 17:24:42  gwehrman
 * Source Identification changed to bracket with SGIMIPS && !_KERNEL.
 *
 * Revision 65.4  1998/01/07 17:19:49  jdoak
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS
 *
 * Revision 65.3  1997/11/06  19:55:57  jdoak
 * Source Identification added.
 *
 * Revision 65.2  1997/10/20  19:15:39  jdoak
 * Initial IRIX 6.4 code merge
 *
 * Revision 1.1.13.1  1996/10/15  20:46:06  arvind
 * 	Reorder test to squelch purify warning (buff_dealloc is
 * 	sometimes uninitialized, but never is when data_len != 0)
 * 	[1996/10/10  23:56 UTC  sommerfeld  /main/sommerfeld_pk_kdc_2/1]
 *
 * Revision 1.1.11.2  1996/02/18  18:55:06  marty
 * 	Update OSF copyright years
 * 	[1996/02/18  18:07:21  marty]
 * 
 * Revision 1.1.11.1  1995/12/07  22:30:23  root
 * 	Submit OSF/DCE 1.2.1
 * 
 * 	HP revision /main/HPDCE02/jrr_1.2_mothra/2  1995/11/21  00:34 UTC  psn
 * 	Merge second XIDL drop for DCE 1.2.1
 * 	[1995/11/17  17:07 UTC  dat  /main/dat_xidl2/1]
 * 
 * 	oct 95 idl drop
 * 	[1995/10/22  23:36:11  bfc]
 * 	 *
 * 	may 95 idl drop
 * 	[1995/10/22  22:57:43  bfc]
 * 	 *
 * 	DCE for DEC OSF/1: populate from OSF DCE R1.1
 * 	[1995/10/21  18:25:50  bfc]
 * 	 *
 * 
 * 	HP revision /main/HPDCE02/4  1995/08/01  15:00 UTC  lmm
 * 	merge uninitialized variable fix
 * 
 * 	HP revision /main/HPDCE02/lmm_fix_idl_umrs/1  1995/08/01  14:30 UTC  lmm
 * 	fix uninit Z_values in rpc_ss_unmar_interp
 * 
 * 	HP revision /main/HPDCE02/3  1995/07/21  22:43 UTC  lmm
 * 	merge fix for memory leak in reduce stack fix
 * 
 * 	HP revision /main/HPDCE02/tatsu_s.local_rpc.b4/1  1995/07/12  16:48 UTC  tatsu_s
 * 	Fixed a memory leak.
 * 
 * 	HP revision /main/HPDCE02/2  1995/06/30  19:45 UTC  lmm
 * 	merge reduce stack fix
 * 
 * 	HP revision /main/HPDCE02/lmm_reduce_rpc_stack/5  1995/06/28  17:05 UTC  lmm
 * 	only enable stack saving mods in the kernel
 * 
 * 	HP revision /main/HPDCE02/lmm_reduce_rpc_stack/4  1995/06/27  21:57 UTC  lmm
 * 	free normal_range_list and correct ptr arithmetic in unmar_interp
 * 
 * 	HP revision /main/HPDCE02/lmm_reduce_rpc_stack/2  1995/06/21  17:23 UTC  lmm
 * 	malloc normal_range_list in rpc_ss_unmar_interp to save stack space
 * 
 * 	HP revision /main/HPDCE02/lmm_reduce_rpc_stack/1  1995/06/15  19:57 UTC  lmm
 * 	malloc pointee_Z_values in rpc_ss_ndr_unmar_interp to save stack space
 * 
 * 	HP revision /main/HPDCE02/1  1994/08/03  16:32 UTC  tatsu_s
 * 	Merged mothra up to dce 1.1.
 * 	[1995/12/07  21:15:59  root]
 * 
 * Revision 1.1.6.4  1994/07/13  18:56:24  tom
 * 	Bug 10103 - Reduce little-endian bias of stubs
 * 	[1994/07/12  18:49:38  tom]
 * 	HP revision /main/HPDCE01/4  1994/05/26  20:46 UTC  gaz
 * 	Merge IDL performance changes
 * 
 * 	HP revision /main/HPDCE01/gaz_kk_idl_merge/1  1994/05/06  19:35 UTC  gaz
 * 	Merge IDL performance enhancements with mainline
 * 
 * 	HP revision /main/HPDCE01/ajayb_idl/1  1994/04/12  18:55 UTC  ajayb
 * 	Malloc and full ptr optimizations
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
 * 	HP revision /main/HPDCE01/lmm_idl_endian/1  1994/03/10  21:10 UTC  lmm
 * 	changing idl endian bias
 * 
 * Revision 1.1.5.2  1993/11/11  22:04:38  sanders
 * 	removed statics so libbb could link to routines
 * 	[1993/11/11  22:01:46  sanders]
 * 
 * Revision 1.1.6.3  1994/05/31  18:56:43  tom
 * 	From DEC: unmarshalling security enhancement (OT 10807).
 * 	[1994/05/31  18:55:47  tom]
 * 
 * Revision 1.1.6.2  1994/05/11  17:37:35  rico
 * 	Fixes for [string] and [ptr] interactions.
 * 	[1994/05/10  14:20:59  rico]
 * 
 * Revision 1.1.6.1  1994/05/03  18:50:49  tom
 * 	From DEC: Add extended string support.
 * 	[1994/05/03  18:46:50  tom]
 * 
 * Revision 1.1.2.2  1993/07/07  20:09:39  ganni
 * 	reduced stub idl sources
 * 	[1993/07/07  19:38:38  ganni]
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
**      ndrui.c
**
**  FACILITY:
**
**      Interface Definition Language (IDL) Compiler
**
**  ABSTRACT:
**
**      NDR unmarshalling interpreter main module
**
*/

#include <dce/idlddefs.h>
#include <ndrui.h>
#include <lsysdep.h>

/*
 *  Forward function references
 */

/* None */

/******************************************************************************/
/*                                                                            */
/*  Calculate storage allocation size                                         */
/*                                                                            */
/******************************************************************************/
idl_ulong_int rpc_ss_ndr_allocation_size
#ifdef IDL_PROTOTYPES
(
    /* [in] */  idl_ulong_int fixed_part_size,
                    /* Size of the fixed part of the object to be allocated */
    /* [in] */  idl_ulong_int dimensionality,
                    /* 0 if object has no array part */
    /* [in] */  idl_ulong_int *Z_values,
                    /* Ignored if object has no array part */
    /* [in] */  idl_byte *array_defn_ptr,
                  /* If object has array part, points to array base info */
    IDL_msp_t IDL_msp
)
#else
( fixed_part_size, dimensionality, Z_values, array_defn_ptr, IDL_msp )
    idl_ulong_int fixed_part_size;
    idl_ulong_int dimensionality;
    idl_ulong_int *Z_values;
    idl_byte *array_defn_ptr;
    IDL_msp_t IDL_msp;
#endif
{
    idl_ulong_int allocation_size;
    idl_ulong_int i;
    idl_ulong_int dummy_defn_index; /* Throw-away array parameter */

    if (dimensionality == 0)
        allocation_size = 0;
    else
    {
        /* Get size of array base type */
        if (*array_defn_ptr == IDL_DT_STRING)
        {
            dimensionality--;
            rpc_ss_get_string_base_desc(array_defn_ptr, &allocation_size,
                                            &dummy_defn_index, IDL_msp);
        }
        else
            allocation_size = rpc_ss_type_size(array_defn_ptr, IDL_msp);
        /* Multiply by number of array elements */
        for (i=0; i<dimensionality; i++)
            allocation_size *= Z_values[i];
    }
    
    /* Add in the size of the fixed part */
    allocation_size += fixed_part_size;

    return(allocation_size);
}

/******************************************************************************/
/*                                                                            */
/*  Allocate storage                                                          */
/*                                                                            */
/******************************************************************************/
void rpc_ss_ndr_alloc_storage
#ifdef IDL_PROTOTYPES
(
    /* [in] */  idl_ulong_int fixed_part_size,
                    /* Size of the fixed part of the object to be allocated */
    /* [in] */  idl_ulong_int dimensionality,
                    /* 0 if object has no array part */
    /* [in] */  idl_ulong_int *Z_values,
                    /* Ignored if object has no array part */
    /* [in] */  idl_byte *array_defn_ptr,
                  /* If object has array part, points to array base type info */
    /* [out] */ rpc_void_p_t *p_storage_addr,
    IDL_msp_t IDL_msp
)
#else
( fixed_part_size, dimensionality, Z_values, array_defn_ptr,
  p_storage_addr, IDL_msp )
    idl_ulong_int fixed_part_size;
    idl_ulong_int dimensionality;
    idl_ulong_int *Z_values;
    idl_byte *array_defn_ptr;
    rpc_void_p_t *p_storage_addr;
    IDL_msp_t IDL_msp;
#endif
{
    idl_ulong_int allocation_size;

    allocation_size = rpc_ss_ndr_allocation_size( fixed_part_size,
                            dimensionality, Z_values, array_defn_ptr, IDL_msp );

    /* Allocate storage - mem_alloc will raise an exception if necessary */
    if (IDL_msp->IDL_side == IDL_server_side_k)
    {

        *p_storage_addr =
                 (rpc_void_p_t)rpc_ss_mem_alloc(&(IDL_msp->IDL_mem_handle),
                                                    allocation_size);
    }
    else
    {
        *p_storage_addr = (rpc_void_p_t)(*(IDL_msp->IDL_p_allocate))
                                                    (allocation_size);
        if (*p_storage_addr == NULL)
            RAISE(rpc_x_no_memory);
    }
            
}

/******************************************************************************/
/*                                                                            */
/*  Unmarshall a structure                                                    */
/*                                                                            */
/******************************************************************************/
void rpc_ss_ndr_unmar_struct
#ifdef IDL_PROTOTYPES
(
    /* [in] */  idl_byte struct_type,   /* DT_FIXED_STRUCT or DT_CONF_STRUCT */
    /* [in] */  idl_byte *defn_vec_ptr, /* Points to index into offset vector */
    /* [in] */  rpc_void_p_t struct_addr,
    /* [in] */  idl_ulong_int *Z_values,   /* Only used for DT_CONF_STRUCT */
    /* [in] */  IDL_cs_shadow_elt_t *cs_shadow,
                              /* NULL unless conformant struct with cs-shadow */
    IDL_msp_t IDL_msp
)
#else
(struct_type, defn_vec_ptr, struct_addr, Z_values, cs_shadow, IDL_msp)
    idl_byte struct_type;
    idl_byte *defn_vec_ptr;
    rpc_void_p_t struct_addr;
    idl_ulong_int *Z_values;
    IDL_cs_shadow_elt_t *cs_shadow;
    IDL_msp_t IDL_msp;
#endif
{
    idl_ulong_int offset_index;
    idl_ulong_int *struct_offset_vec_ptr; /* Start of offsets for this struct */
    idl_ulong_int *offset_vec_ptr;
    idl_byte type_byte;
    idl_ulong_int offset;
    idl_ulong_int field_defn_index;
    idl_byte *field_defn_ptr;
    idl_ulong_int conf_dims;    /* Number of dimensions of conformance info */
    IDL_bound_pair_t *range_list;
    IDL_bound_pair_t normal_range_list[IDL_NORMAL_DIMS];
    IDL_bound_pair_t *bounds_list;
    idl_ulong_int *varying_Z_values;    /* Used for varying array fields */
    idl_ulong_int normal_Z_values[IDL_NORMAL_DIMS];
    idl_ulong_int array_dims;   /* Number of dimensions of array */
    idl_ulong_int unique_flag;  /* Wire form of [unique] pointer */
    idl_ulong_int switch_value; /* Discarded [out] parameter */
    idl_ulong_int node_number;
#ifndef _KERNEL
    idl_ulong_int shadow_length;
    idl_byte *shadow_defn_ptr;  /* Position after shadow_length */
#endif

    IDL_GET_LONG_FROM_VECTOR(offset_index,defn_vec_ptr);
    struct_offset_vec_ptr = IDL_msp->IDL_offset_vec + offset_index;

    if ( (struct_type == IDL_DT_CONF_STRUCT)
        || (struct_type == IDL_DT_V1_CONF_STRUCT) )
    {
        /* Discard the conformance info */
        IDL_DISCARD_LONG_FROM_VECTOR(defn_vec_ptr);
    }

#ifndef _KERNEL
    if (*defn_vec_ptr == IDL_DT_CS_SHADOW)
    {
        defn_vec_ptr++;
        IDL_GET_LONG_FROM_VECTOR(shadow_length, defn_vec_ptr);
        if (cs_shadow == NULL)
        {
            cs_shadow = (IDL_cs_shadow_elt_t *) rpc_ss_mem_alloc
                              (&IDL_msp->IDL_mem_handle, 
                                   shadow_length * sizeof(IDL_cs_shadow_elt_t));
        }
        shadow_defn_ptr = defn_vec_ptr;
    }
#endif

    offset_vec_ptr = struct_offset_vec_ptr + 1;
                                        /* Skip over size at start of offsets */
    /* Loop over the fields of the structure */
    do {
        type_byte = *defn_vec_ptr;
        defn_vec_ptr++;
        switch(type_byte)
        {
            case IDL_DT_BYTE:
                offset = *offset_vec_ptr;
                offset_vec_ptr++;
                IDL_UNMAR_BYTE( (idl_byte *)struct_addr+offset );
                break;
            case IDL_DT_CHAR:
                offset = *offset_vec_ptr;
                offset_vec_ptr++;
                IDL_UNMAR_CHAR( (idl_byte *)struct_addr+offset );
                break;
            case IDL_DT_BOOLEAN:
                offset = *offset_vec_ptr;
                offset_vec_ptr++;
                IDL_UNMAR_BOOLEAN( (idl_byte *)struct_addr+offset );
                break;
#ifndef _KERNEL
            case IDL_DT_DOUBLE:
                offset = *offset_vec_ptr;
                offset_vec_ptr++;
                IDL_UNMAR_DOUBLE( (idl_byte *)struct_addr+offset );
                break;
#endif
            case IDL_DT_ENUM:
                offset = *offset_vec_ptr;
                offset_vec_ptr++;
                IDL_UNMAR_ENUM( (idl_byte *)struct_addr+offset );
                break;
#ifndef _KERNEL
            case IDL_DT_FLOAT:
                offset = *offset_vec_ptr;
                offset_vec_ptr++;
                IDL_UNMAR_FLOAT( (idl_byte *)struct_addr+offset );
                break;
#endif
            case IDL_DT_SMALL:
                offset = *offset_vec_ptr;
                offset_vec_ptr++;
                IDL_UNMAR_SMALL( (idl_byte *)struct_addr+offset );
                break;
            case IDL_DT_SHORT:
                offset = *offset_vec_ptr;
                offset_vec_ptr++;
                IDL_UNMAR_SHORT( (idl_byte *)struct_addr+offset );
                break;
            case IDL_DT_LONG:
                offset = *offset_vec_ptr;
                offset_vec_ptr++;
                IDL_UNMAR_LONG( (idl_byte *)struct_addr+offset );
                break;
#ifndef _KERNEL
            case IDL_DT_HYPER:
                offset = *offset_vec_ptr;
                offset_vec_ptr++;
                IDL_UNMAR_HYPER( (idl_byte *)struct_addr+offset );
                break;
#endif
            case IDL_DT_USMALL:
                offset = *offset_vec_ptr;
                offset_vec_ptr++;
                IDL_UNMAR_USMALL( (idl_byte *)struct_addr+offset );
                break;
            case IDL_DT_USHORT:
                offset = *offset_vec_ptr;
                offset_vec_ptr++;
                IDL_UNMAR_USHORT( (idl_byte *)struct_addr+offset );
                break;
            case IDL_DT_ULONG:
                offset = *offset_vec_ptr;
                offset_vec_ptr++;
                IDL_UNMAR_ULONG( (idl_byte *)struct_addr+offset );
                break;
#ifndef _KERNEL
            case IDL_DT_UHYPER:
                offset = *offset_vec_ptr;
                offset_vec_ptr++;
                IDL_UNMAR_UHYPER( (idl_byte *)struct_addr+offset );
                break;
            case IDL_DT_V1_ENUM:
                offset = *offset_vec_ptr;
                offset_vec_ptr++;
                IDL_UNMAR_V1_ENUM( (idl_byte *)struct_addr+offset );
                break;
#endif
            case IDL_DT_ERROR_STATUS:
                offset = *offset_vec_ptr;
                offset_vec_ptr++;
                IDL_UNMAR_ERROR_STATUS( (idl_byte *)struct_addr+offset );
                break;
            case IDL_DT_FIXED_ARRAY:
                defn_vec_ptr++;     /* Skip over properties byte */
                IDL_DISCARD_LONG_FROM_VECTOR(defn_vec_ptr);
                                                    /* Full array definition */
                IDL_GET_LONG_FROM_VECTOR(field_defn_index, defn_vec_ptr);
                offset = *offset_vec_ptr;
                offset_vec_ptr++;
                rpc_ss_ndr_unmar_fixed_arr(field_defn_index,
                                           (idl_byte *)struct_addr+offset,
                                           0, IDL_msp);
                break;
            case IDL_DT_VARYING_ARRAY:
                defn_vec_ptr++;     /* Skip over properties byte */
                IDL_DISCARD_LONG_FROM_VECTOR(defn_vec_ptr);
                                                    /* Full array definition */
                IDL_GET_LONG_FROM_VECTOR(field_defn_index, defn_vec_ptr);
                offset = *offset_vec_ptr;
                offset_vec_ptr++;
                field_defn_ptr = IDL_msp->IDL_type_vec + field_defn_index;
                array_dims = (idl_ulong_int)*field_defn_ptr;
                field_defn_ptr++;
                /* By design field_defn_ptr is now at (0 mod 4) */
		if (IDL_msp->IDL_type_vec[TVEC_INT_REP_OFFSET] != 
		    NDR_LOCAL_INT_REP)
                rpc_ss_fixed_bounds_from_vector(array_dims, field_defn_ptr, 
                                                &bounds_list, IDL_msp);
		else
                  bounds_list = (IDL_bound_pair_t *)field_defn_ptr;
                if (array_dims > IDL_NORMAL_DIMS)
                {
                    varying_Z_values = NULL;
                    range_list = NULL;
                }
                else
                {
                    varying_Z_values = normal_Z_values;
                    range_list = normal_range_list;
                }
                rpc_ss_Z_values_from_bounds(bounds_list, array_dims,
                                             &varying_Z_values, IDL_msp);
                field_defn_ptr += array_dims * 
                       (IDL_FIXED_BOUND_PAIR_WIDTH + IDL_DATA_LIMIT_PAIR_WIDTH),
                rpc_ss_ndr_unmar_range_list( array_dims, *field_defn_ptr,
                                             &range_list, IDL_msp );
                rpc_ss_ndr_u_var_or_open_arr( array_dims, varying_Z_values,
                    field_defn_ptr, (idl_byte *)struct_addr+offset,
                    range_list, 0, IDL_msp );
                if (array_dims > IDL_NORMAL_DIMS)
                {
                    rpc_ss_mem_item_free(&IDL_msp->IDL_mem_handle,
                                            (byte_p_t)range_list);
                    rpc_ss_mem_item_free(&IDL_msp->IDL_mem_handle,
                                            (byte_p_t)varying_Z_values);
                }
		if (IDL_msp->IDL_type_vec[TVEC_INT_REP_OFFSET] != 
		    NDR_LOCAL_INT_REP)
                rpc_ss_mem_item_free(&IDL_msp->IDL_mem_handle,
                                        (byte_p_t)bounds_list);
                break;
            case IDL_DT_CONF_ARRAY:
                defn_vec_ptr++;     /* Skip over properties byte */
                IDL_DISCARD_LONG_FROM_VECTOR(defn_vec_ptr);
                                                    /* Full array definition */
                IDL_GET_LONG_FROM_VECTOR(field_defn_index, defn_vec_ptr);
                offset = *offset_vec_ptr;   /* Must be last field of struct */
                field_defn_ptr = IDL_msp->IDL_type_vec + field_defn_index;
                conf_dims = (idl_ulong_int)*field_defn_ptr;
                field_defn_ptr++;
                IDL_ADV_DEFN_PTR_OVER_BOUNDS( field_defn_ptr, conf_dims );
                rpc_ss_ndr_u_fix_or_conf_arr( conf_dims, Z_values,
                         field_defn_ptr, (idl_byte *)struct_addr+offset,
                         IDL_M_CONF_ARRAY, IDL_msp );
                break;
            case IDL_DT_OPEN_ARRAY:
                defn_vec_ptr++;     /* Skip over properties byte */
                IDL_DISCARD_LONG_FROM_VECTOR(defn_vec_ptr);
                                                    /* Full array definition */
                IDL_GET_LONG_FROM_VECTOR(field_defn_index, defn_vec_ptr);
                offset = *offset_vec_ptr;   /* Must be last field of struct */
                field_defn_ptr = IDL_msp->IDL_type_vec + field_defn_index;
                conf_dims = (idl_ulong_int)*field_defn_ptr;
                field_defn_ptr++;
                IDL_ADV_DEFN_PTR_OVER_BOUNDS( field_defn_ptr, conf_dims );
                /* Advance array defn ptr over data limit info */
                field_defn_ptr += conf_dims *  IDL_DATA_LIMIT_PAIR_WIDTH;
                if (conf_dims > IDL_NORMAL_DIMS)
                    range_list = NULL;
                else
                    range_list = normal_range_list;
                rpc_ss_ndr_unmar_range_list( conf_dims, *field_defn_ptr,
                                             &range_list, IDL_msp );
                rpc_ss_ndr_u_var_or_open_arr( conf_dims, Z_values,
                         field_defn_ptr, (idl_byte *)struct_addr+offset,
                         range_list, IDL_M_CONF_ARRAY, IDL_msp );
                if (conf_dims > IDL_NORMAL_DIMS)
                    rpc_ss_mem_item_free(&IDL_msp->IDL_mem_handle,
                                            (byte_p_t)range_list);
                break;
            case IDL_DT_ENC_UNION:
                defn_vec_ptr++;     /* Skip over properties byte */
                IDL_GET_LONG_FROM_VECTOR(field_defn_index, defn_vec_ptr);
                offset = *offset_vec_ptr;
                offset_vec_ptr++;
                rpc_ss_ndr_u_enc_union_or_ptees( (idl_byte *)struct_addr+offset,
                                          field_defn_index, idl_false, IDL_msp);
                break;
#ifndef _KERNEL
            case IDL_DT_N_E_UNION:
                defn_vec_ptr++;     /* Skip over properties byte */
                IDL_DISCARD_LONG_FROM_VECTOR( defn_vec_ptr ); /* Switch index */
                IDL_GET_LONG_FROM_VECTOR(field_defn_index, defn_vec_ptr);
                offset = *offset_vec_ptr;
                offset_vec_ptr++;
                rpc_ss_ndr_unmar_n_e_union( (idl_byte *)struct_addr+offset,
                                      field_defn_index, &switch_value, IDL_msp);
                break;
#endif
            case IDL_DT_FULL_PTR:
                defn_vec_ptr++;     /* Properties byte */
                /* Unmarshall the node number into the space for the pointer */
                offset = *offset_vec_ptr;
                offset_vec_ptr++;
                IDL_UNMAR_ULONG( &node_number );

#if defined(SGIMIPS)

                *(rpc_void_p_t *)((idl_byte *)struct_addr+offset)
                                               = (rpc_void_p_t)(__psint_t)node_number;
#else /* SGIMIPS */
                *(rpc_void_p_t *)((idl_byte *)struct_addr+offset)
                                                    = (rpc_void_p_t)node_number;
#endif /* SGIMIPS */ 
                IDL_DISCARD_LONG_FROM_VECTOR( defn_vec_ptr );
                                        /* Indirection to pointee type */
                break;
#ifndef _KERNEL
            case IDL_DT_UNIQUE_PTR:
                defn_vec_ptr++;     /* Properties byte */
                IDL_UNMAR_ULONG( &unique_flag );
                IDL_DISCARD_LONG_FROM_VECTOR( defn_vec_ptr );
                                        /* Indirection to pointee type */
                offset = *offset_vec_ptr;
                offset_vec_ptr++;
                if (unique_flag == 0)
                {
                    *(rpc_void_p_t *)((idl_byte *)struct_addr+offset) = NULL;
                }
                else if ( *(rpc_void_p_t *)((idl_byte *)struct_addr+offset) 
                                                                    == NULL )
                {
                    *(rpc_void_p_t *)((idl_byte *)struct_addr+offset)
                                         = IDL_NEW_NODE;
                }
                break;
#endif
            case IDL_DT_REF_PTR:
                defn_vec_ptr++;     /* Properties byte */
                /* Skip over the longword place holder */
                IDL_UNMAR_ALIGN_MP( IDL_msp, 4 );
                rpc_ss_ndr_unmar_check_buffer( IDL_msp );
                IDL_msp->IDL_mp += 4;
                IDL_msp->IDL_left_in_buff -= 4;
                IDL_DISCARD_LONG_FROM_VECTOR( defn_vec_ptr );
                                        /* Indirection to pointee type */
                offset_vec_ptr++;
                break;
            case IDL_DT_IGNORE:
                /* Skip over the longword place holder */
                IDL_UNMAR_ALIGN_MP( IDL_msp, 4 );
                rpc_ss_ndr_unmar_check_buffer( IDL_msp );
                IDL_msp->IDL_mp += 4;
                IDL_msp->IDL_left_in_buff -= 4;
                offset_vec_ptr++;
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
                rpc_ss_ndr_unmar_xmit_as(field_defn_index,
                                 (idl_byte *)struct_addr+offset, NULL, IDL_msp);
                break;
            case IDL_DT_INTERFACE:
                defn_vec_ptr++;     /* Skip over properties byte */
                IDL_GET_LONG_FROM_VECTOR(field_defn_index, defn_vec_ptr);
                offset = *offset_vec_ptr;
                offset_vec_ptr++;
                rpc_ss_ndr_unmar_interface(field_defn_index,
                                 (idl_byte *)struct_addr+offset, NULL, IDL_msp);
                break;
#ifndef _KERNEL
            case IDL_DT_V1_ARRAY:
                type_byte = *defn_vec_ptr;  /* DT_VARYING or DT_OPEN */
                defn_vec_ptr += 2;   /* DT_VARYING/DT_OPEN and properties */
                IDL_DISCARD_LONG_FROM_VECTOR(defn_vec_ptr);
                                                    /* Full array definition */
                IDL_GET_LONG_FROM_VECTOR(field_defn_index, defn_vec_ptr);
                offset = *offset_vec_ptr;
                offset_vec_ptr++;
                field_defn_ptr = IDL_msp->IDL_type_vec + field_defn_index;
                array_dims = (idl_ulong_int)*field_defn_ptr;
                field_defn_ptr++;
                /* By design field_defn_ptr is now at (0 mod 4) */
                if (type_byte == IDL_DT_VARYING_ARRAY)
                    field_defn_ptr += array_dims * (IDL_FIXED_BOUND_PAIR_WIDTH
                                                   + IDL_DATA_LIMIT_PAIR_WIDTH);
                else
                {
                    IDL_ADV_DEFN_PTR_OVER_BOUNDS( field_defn_ptr, array_dims );
                    /* Advance array defn ptr over data limit info */
                    field_defn_ptr += array_dims *  IDL_DATA_LIMIT_PAIR_WIDTH;
                }
                rpc_ss_ndr_u_v1_varying_arr( (idl_byte *)struct_addr+offset,
                                             field_defn_ptr, 0, IDL_msp );
                break;
            case IDL_DT_V1_STRING:
                defn_vec_ptr += 2;  /* DT_VARYING_ARRAY and properties */
                IDL_DISCARD_LONG_FROM_VECTOR(defn_vec_ptr);
                                                        /* Full array defn */
                IDL_DISCARD_LONG_FROM_VECTOR(defn_vec_ptr);
                                                      /* Flattened array defn */
                offset = *offset_vec_ptr;
                offset_vec_ptr++;
                rpc_ss_ndr_unmar_v1_string((idl_byte *)struct_addr+offset,
                                           0, IDL_msp);
                break;
            case IDL_DT_CS_ATTRIBUTE:
                /* Discard the value on the wire. Later a correct value will
                    be written from the cs-shadow */
                rpc_ss_ndr_unmar_scalar(*defn_vec_ptr, 
                                        (rpc_void_p_t)&switch_value, IDL_msp);
                defn_vec_ptr++;     /* Type of attribute */
                offset_vec_ptr++;
                break;
            case IDL_DT_CS_ARRAY:
                offset = *offset_vec_ptr;
                rpc_ss_ndr_unmar_cs_array(
                        (rpc_void_p_t)((idl_byte *)struct_addr+offset),
                        cs_shadow, Z_values,
                        offset_vec_ptr - (struct_offset_vec_ptr + 1),
                        &defn_vec_ptr, IDL_msp);
                offset_vec_ptr++;
                break;
            case IDL_DT_CS_TYPE:
                defn_vec_ptr++;     /* Skip over properties byte */
                IDL_GET_LONG_FROM_VECTOR(field_defn_index, defn_vec_ptr);
                offset = *offset_vec_ptr;
                offset_vec_ptr++;
                rpc_ss_ndr_unmar_cs_char(
                                 (rpc_void_p_t)((idl_byte *)struct_addr+offset),
                                 field_defn_index, IDL_msp);
                break;
            case IDL_DT_CS_RLSE_SHADOW:
                rpc_ss_ndr_u_struct_cs_shadow(struct_addr, struct_type,
                            offset_index, shadow_defn_ptr, cs_shadow, IDL_msp);
                break;
#endif
            case IDL_DT_NDR_ALIGN_2:
                IDL_UNMAR_ALIGN_MP( IDL_msp, 2 );
                break;
            case IDL_DT_NDR_ALIGN_4:
                IDL_UNMAR_ALIGN_MP( IDL_msp, 4 );
                break;
            case IDL_DT_NDR_ALIGN_8:
                IDL_UNMAR_ALIGN_MP( IDL_msp, 8 );
                break;
            case IDL_DT_BEGIN_NESTED_STRUCT:
            case IDL_DT_END_NESTED_STRUCT:
            case IDL_DT_EOL:
                break;
            default:
#ifdef DEBUG_INTERP
                printf("rpc_ss_ndr_unmar_struct:unrecognized type %d\n",
                        type_byte);
                exit(0);
#endif
                RAISE(rpc_x_coding_error);
        }
    } while (type_byte != IDL_DT_EOL);

}

/******************************************************************************/
/*                                                                            */
/*  Unmarshall an array by copying from the receive buffer                    */
/*  On entry IDL_mp is already aligned to the base type requirement           */
/*                                                                            */
/******************************************************************************/
void rpc_ss_ndr_unmar_by_copying
#ifdef IDL_PROTOTYPES
(
    /* [in] */  idl_ulong_int element_count,
    /* [in] */  idl_ulong_int element_size,
    /* [in] */  rpc_void_p_t array_addr,
    IDL_msp_t IDL_msp
)
#else
( element_count, element_size, array_addr, IDL_msp )
    idl_ulong_int element_count;
    idl_ulong_int element_size;
    rpc_void_p_t array_addr;
    IDL_msp_t IDL_msp;
#endif
{
    idl_ulong_int bytes_required;   /* Number of bytes left to copy */
    idl_ulong_int bytes_to_copy;  /* Number of bytes to copy from this buffer */

    bytes_required = element_count * element_size;
    while (bytes_required != 0)
    {
        rpc_ss_ndr_unmar_check_buffer( IDL_msp );
        if (bytes_required > IDL_msp->IDL_left_in_buff)
            bytes_to_copy = IDL_msp->IDL_left_in_buff;
        else
            bytes_to_copy = bytes_required;
        memcpy(array_addr, IDL_msp->IDL_mp, bytes_to_copy);
        IDL_msp->IDL_mp += bytes_to_copy;
        IDL_msp->IDL_left_in_buff -= bytes_to_copy;
        bytes_required -= bytes_to_copy;
        array_addr = (rpc_void_p_t)((idl_byte *)array_addr + bytes_to_copy);
    }
}

/******************************************************************************/
/*                                                                            */
/*  Unmarshall a contiguous set of elements one by one                        */
/*                                                                            */
/******************************************************************************/
void rpc_ss_ndr_unmar_by_looping
#ifdef IDL_PROTOTYPES
(
    /* [in] */  idl_ulong_int element_count,
    /* [in] */  idl_byte base_type,
    /* [in] */  rpc_void_p_t array_addr,
    /* [in] */  idl_ulong_int element_size,
                         /* Used if array of struct, union or array of string */
    /* [in] */  idl_ulong_int element_defn_index,
                                /* Used if array of struct or array of string */
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
    idl_ulong_int unique_flag;  /* Wire form of [unique] pointer */
    idl_ulong_int element_byte_count;   /* Number of bytes elements occupy */
    idl_ulong_int node_number;
    idl_byte *element_defn_ptr;
    unsigned long xmit_data_size;   /* [transmit_as] - size of xmitted type */
    rpc_void_p_t xmit_data_buff = NULL;     /* Address of storage [transmit_as]
                                                type can be unmarshalled into */

    if (base_type == IDL_DT_REF_PTR)
    {
        /* Unmarshalling field which is array of [ref] pointers */
        IDL_UNMAR_ALIGN_MP( IDL_msp, 4 );
        while (element_count > 0)
        {
            element_byte_count = 4 * element_count;
            if ( element_byte_count > IDL_msp->IDL_left_in_buff )
            {
                element_count -= IDL_msp->IDL_left_in_buff / 4;
                /* Force a buffer change */
                rpc_ss_ndr_unmar_check_buffer( IDL_msp );
            }
            else
            {
                IDL_msp->IDL_mp += element_byte_count;
                IDL_msp->IDL_left_in_buff -= element_byte_count;
                element_count = 0;
            }
        }
        return;
    }

    if ( (base_type == IDL_DT_TRANSMIT_AS)
        || (base_type == IDL_DT_REPRESENT_AS) )
    {
        /* If possible, allocate a buffer into which elements of the transmitted
            type can be unmarshalled */
        element_defn_ptr = IDL_msp->IDL_type_vec + element_defn_index;
        IDL_DISCARD_LONG_FROM_VECTOR( element_defn_ptr );
                                             /* Presented type size index */
        IDL_DISCARD_LONG_FROM_VECTOR( element_defn_ptr );   /* Rtn vec index */
        if (*element_defn_ptr == IDL_DT_STRING)
            element_defn_ptr++;
        xmit_data_size = rpc_ss_type_size(element_defn_ptr, IDL_msp);
        if (xmit_data_size != 0)
            xmit_data_buff = (rpc_void_p_t)rpc_ss_mem_alloc
                                    (&IDL_msp->IDL_mem_handle, xmit_data_size);
    }

    for (i=0; i<element_count; i++)
    {
        switch (base_type)
        {
            case IDL_DT_BOOLEAN:
                IDL_UNMAR_BOOLEAN( array_addr );
                array_addr = (rpc_void_p_t)((idl_boolean *)(array_addr) + 1);
                break;
            case IDL_DT_BYTE:
                IDL_UNMAR_BYTE( array_addr );
                array_addr = (rpc_void_p_t)((idl_byte *)(array_addr) + 1);
                break;
            case IDL_DT_CHAR:
                IDL_UNMAR_CHAR( array_addr );
                array_addr = (rpc_void_p_t)((idl_char *)(array_addr) + 1);
                break;
#ifndef _KERNEL
            case IDL_DT_DOUBLE:
                IDL_UNMAR_DOUBLE( array_addr );
                array_addr = (rpc_void_p_t)((idl_long_float *)(array_addr) + 1);
                break;
#endif
            case IDL_DT_ENUM:
                IDL_UNMAR_ENUM( array_addr );
                array_addr = (rpc_void_p_t)((int *)(array_addr) + 1);
                break;
#ifndef _KERNEL
            case IDL_DT_V1_ENUM:
                IDL_UNMAR_V1_ENUM( array_addr );
                array_addr = (rpc_void_p_t)((int *)(array_addr) + 1);
                break;
            case IDL_DT_FLOAT:
                IDL_UNMAR_FLOAT( array_addr );
                array_addr = (rpc_void_p_t)((idl_short_float *)(array_addr) + 1);
                break;
#endif
            case IDL_DT_SMALL:
                IDL_UNMAR_SMALL( array_addr );
                array_addr = (rpc_void_p_t)((idl_small_int *)(array_addr) + 1);
                break;
            case IDL_DT_SHORT:
                IDL_UNMAR_SHORT( array_addr );
                array_addr = (rpc_void_p_t)((idl_short_int *)(array_addr) + 1);
                break;
            case IDL_DT_LONG:
                IDL_UNMAR_LONG( array_addr );
                array_addr = (rpc_void_p_t)((idl_long_int *)(array_addr) + 1);
                break;
#ifndef _KERNEL
            case IDL_DT_HYPER:
                IDL_UNMAR_HYPER( array_addr );
                array_addr = (rpc_void_p_t)((idl_hyper_int *)(array_addr) + 1);
                break;
#endif
            case IDL_DT_USMALL:
                IDL_UNMAR_USMALL( array_addr );
                array_addr = (rpc_void_p_t)((idl_usmall_int *)(array_addr) + 1);
                break;
            case IDL_DT_USHORT:
                IDL_UNMAR_USHORT( array_addr );
                array_addr = (rpc_void_p_t)((idl_ushort_int *)(array_addr) + 1);
                break;
            case IDL_DT_ULONG:
                IDL_UNMAR_ULONG( array_addr );
                array_addr = (rpc_void_p_t)((idl_ulong_int *)(array_addr) + 1);
                break;
            case IDL_DT_ERROR_STATUS:
                IDL_UNMAR_ERROR_STATUS( array_addr );
                array_addr = (rpc_void_p_t)((idl_ulong_int *)(array_addr) + 1);
                break;
#ifndef _KERNEL
            case IDL_DT_UHYPER:
                IDL_UNMAR_UHYPER( array_addr );
                array_addr = (rpc_void_p_t)((idl_uhyper_int *)(array_addr) + 1);
                break;
#endif
            case IDL_DT_FIXED_STRUCT:
                rpc_ss_ndr_unmar_struct( base_type,
                                     IDL_msp->IDL_type_vec + element_defn_index,
                                         array_addr, NULL, NULL, IDL_msp );
                array_addr = (rpc_void_p_t)
                                      ((idl_byte *)(array_addr) + element_size);
                break;
            case IDL_DT_FIXED_ARRAY:
                /* Base type of pipe is array */
                rpc_ss_ndr_unmar_fixed_arr( element_defn_index, array_addr,
                                            0, IDL_msp );
                array_addr = (rpc_void_p_t)
                                      ((idl_byte *)(array_addr) + element_size);
                break;
            case IDL_DT_ENC_UNION:
                rpc_ss_ndr_u_enc_union_or_ptees( array_addr, element_defn_index,
                                                    idl_false, IDL_msp );
                array_addr = (rpc_void_p_t)
                                    ((idl_byte *)(array_addr) + element_size);
                break;
            case IDL_DT_FULL_PTR:
                /* Unmarshall the node number into the space for the pointer */
                IDL_UNMAR_ULONG( &node_number );

#ifdef SGIMIPS
                *(rpc_void_p_t *)(array_addr) = 
				(rpc_void_p_t)(__psint_t)node_number;
#else /* SGIMIPS */
                *(rpc_void_p_t *)(array_addr) = (rpc_void_p_t)node_number;
#endif /* SGIMIPS */

                array_addr = (rpc_void_p_t)((rpc_void_p_t *)(array_addr) + 1);
                break;
#ifndef _KERNEL
            case IDL_DT_UNIQUE_PTR:
                IDL_UNMAR_ULONG( &unique_flag );
                if (unique_flag == 0)
                    *(rpc_void_p_t *)array_addr = NULL;
                else if ( *(rpc_void_p_t *)array_addr == NULL )
                    *(rpc_void_p_t *)array_addr = IDL_NEW_NODE;
                array_addr = (rpc_void_p_t)((rpc_void_p_t *)(array_addr) + 1);
                break;
#endif
            case IDL_DT_STRING:
                {
                    idl_byte *element_defn_ptr;
                    idl_ulong_int A, B;
                    idl_ulong_int base_type_size;

                    element_defn_ptr = IDL_msp->IDL_type_vec
                                        + element_defn_index
                                        + 1     /* Dimensionality */
                                        + IDL_FIXED_BOUND_PAIR_WIDTH
                                        + IDL_DATA_LIMIT_PAIR_WIDTH;
                    /* Now pointing at base type of string */
                    base_type_size = rpc_ss_type_size(element_defn_ptr,
                                                                     IDL_msp);
                    IDL_UNMAR_ALIGN_MP( IDL_msp, 4 );
                    rpc_ss_ndr_unmar_check_buffer( IDL_msp );
                    rpc_convert_ulong_int(IDL_msp->IDL_drep, ndr_g_local_drep,
                                 IDL_msp->IDL_mp, A);
                    IDL_msp->IDL_mp += 4;
                    IDL_msp->IDL_left_in_buff -= 4;
                    rpc_ss_ndr_unmar_check_buffer( IDL_msp );
                    rpc_convert_ulong_int(IDL_msp->IDL_drep, ndr_g_local_drep,
                                 IDL_msp->IDL_mp, B);
                    IDL_msp->IDL_mp += 4;
                    IDL_msp->IDL_left_in_buff -= 4;
                    if ( ( (*element_defn_ptr == IDL_DT_CHAR)
                          && (IDL_msp->IDL_drep.char_rep
                                                 != ndr_g_local_drep.char_rep) )
                        ||
                           ( (*element_defn_ptr == IDL_DT_USHORT
                              || *element_defn_ptr == IDL_DT_ULONG)
                            && (IDL_msp->IDL_drep.int_rep
                                                != ndr_g_local_drep.int_rep) ) )
                    {
                        rpc_ss_ndr_unmar_by_looping(B, *element_defn_ptr,
                                                     array_addr, 1, 0, IDL_msp);
                    }
                    else
                        rpc_ss_ndr_unmar_by_copying(B, base_type_size,
                                                    array_addr, IDL_msp);
                }
                array_addr = (rpc_void_p_t)
                                      ((idl_byte *)(array_addr) + element_size);
                break;
            case IDL_DT_TRANSMIT_AS:
            case IDL_DT_REPRESENT_AS:
                rpc_ss_ndr_unmar_xmit_as(element_defn_index, array_addr,
                                                       xmit_data_buff, IDL_msp);
                array_addr = (rpc_void_p_t)
                                      ((idl_byte *)(array_addr) + element_size);
                break;
            case IDL_DT_INTERFACE:
                rpc_ss_ndr_unmar_interface(element_defn_index, array_addr,
                                                       xmit_data_buff, IDL_msp);
                array_addr = (rpc_void_p_t)
                                      ((idl_byte *)(array_addr) + element_size);
                break;
#ifndef _KERNEL
            case IDL_DT_V1_STRING:
                rpc_ss_ndr_unmar_v1_string(array_addr, 0, IDL_msp);
                array_addr = (rpc_void_p_t)
                                      ((idl_byte *)(array_addr) + element_size);
                break;
#endif
            default:
#ifdef DEBUG_INTERP
                printf(
                      "rpc_ss_ndr_unmar_by_looping:unrecognized type %d\n",
                        base_type);
                exit(0);
#endif
                RAISE(rpc_x_coding_error);
        }
    }
    if (xmit_data_buff != NULL)
        rpc_ss_mem_item_free(&IDL_msp->IDL_mem_handle,(byte_p_t)xmit_data_buff);
}

/******************************************************************************/
/*                                                                            */
/* Unmarshall a fixed or conformant array                                     */
/*                                                                            */
/******************************************************************************/
void rpc_ss_ndr_u_fix_or_conf_arr
#ifdef IDL_PROTOTYPES
(
    /* [in] */  idl_ulong_int dimensionality,
    /* [in] */  idl_ulong_int *Z_values,
    /* [in] */  idl_byte *defn_vec_ptr, /* On entry points to array base info */
    /* [in] */  rpc_void_p_t array_addr,
    /* [in] */  idl_ulong_int flags,
    IDL_msp_t IDL_msp
)
#else
( dimensionality, Z_values, defn_vec_ptr, array_addr, flags, IDL_msp )
    idl_ulong_int dimensionality;
    idl_ulong_int *Z_values;
    idl_byte *defn_vec_ptr;
    rpc_void_p_t array_addr;
    idl_ulong_int flags;
    IDL_msp_t IDL_msp;
#endif
{
    idl_ulong_int element_count;
    idl_ulong_int i;
    idl_byte base_type;
    idl_boolean unmarshall_by_copying;
    idl_ulong_int element_size;
    idl_ulong_int element_defn_index;
    idl_ulong_int struct_offset_index;
    idl_byte *struct_defn_ptr;

    if ( (*defn_vec_ptr == IDL_DT_STRING)
        || (*defn_vec_ptr == IDL_DT_V1_STRING) )
    {
        /* Arrays of strings have a special representation */
        dimensionality--;
    }

    element_count = 1;
    for (i=0; i<dimensionality; i++)
    {
        element_count *= Z_values[i];
    }

    if (element_count == 0)
        return;

    rpc_ss_ndr_arr_align_and_opt( IDL_unmarshalling_k, dimensionality,
                                    &base_type, defn_vec_ptr,
                                    &unmarshall_by_copying, IDL_msp );


    if (base_type == IDL_DT_REF_PTR)
    {
        /* Arrays of [ref] pointers are not marshalled */
        return;
    }

    if (unmarshall_by_copying)
    {
        element_size = rpc_ss_type_size(defn_vec_ptr, IDL_msp);
        if (base_type == IDL_DT_FIXED_STRUCT)
        {
            /* There is a problem because for the last element of the
              structure we may not want to unmarshall sizeof(structure) bytes */
            element_count--;        /* So do the last one separately */
        }
        if (element_count != 0)
        {
            rpc_ss_ndr_unmar_by_copying(element_count, element_size, array_addr,
                                                                     IDL_msp);
        }
        if (base_type == IDL_DT_FIXED_STRUCT)
        {
            /* Marshall the last element separately */
            defn_vec_ptr += 2;  /* See comment below */
            IDL_GET_LONG_FROM_VECTOR( element_defn_index, defn_vec_ptr );
            rpc_ss_ndr_unmar_struct(base_type,
                                    IDL_msp->IDL_type_vec + element_defn_index,
                                    (rpc_void_p_t)((idl_byte *)array_addr
                                                + element_count * element_size),
                                    NULL, NULL, IDL_msp);
        }
        return;
    }

    if ( (base_type == IDL_DT_FIXED_STRUCT)
         || (base_type == IDL_DT_ENC_UNION)
         || (base_type == IDL_DT_TRANSMIT_AS)
         || (base_type == IDL_DT_REPRESENT_AS) )
    {
        defn_vec_ptr += 2;  /* Discard type and properties */
        /* If we are unmarshalling an array, not a pipe, defn_vec_ptr was
            4-byte aligned and DT_MODIFIED and modifier are discarded
            by the +=2 followed by GET_LONG */
        IDL_GET_LONG_FROM_VECTOR( element_defn_index, defn_vec_ptr );
        struct_defn_ptr = IDL_msp->IDL_type_vec + element_defn_index;
        IDL_GET_LONG_FROM_VECTOR(struct_offset_index, struct_defn_ptr);
        element_size = *(IDL_msp->IDL_offset_vec + struct_offset_index);
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

    else element_defn_index = 0;

        rpc_ss_ndr_unmar_by_looping( element_count,
                                    base_type, array_addr, element_size,
                                    element_defn_index, IDL_msp);
}

/******************************************************************************/
/*                                                                            */
/*  Unmarshall a fixed array                                                  */
/*                                                                            */
/******************************************************************************/
void rpc_ss_ndr_unmar_fixed_arr
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
    idl_ulong_int *Z_values;
    idl_ulong_int normal_Z_values[IDL_NORMAL_DIMS];
    IDL_bound_pair_t *bounds_list;

    defn_vec_ptr = IDL_msp->IDL_type_vec + defn_index; 
    dimensionality = (idl_ulong_int)*defn_vec_ptr;
    defn_vec_ptr++;     /* By design, alignment is now (0 mod 4) */
    if (dimensionality > IDL_NORMAL_DIMS)
        Z_values = NULL;
    else
        Z_values = normal_Z_values;
    if (IDL_msp->IDL_type_vec[TVEC_INT_REP_OFFSET] != NDR_LOCAL_INT_REP)
      rpc_ss_fixed_bounds_from_vector(dimensionality, defn_vec_ptr, &bounds_list,
                                    IDL_msp);
    else
      bounds_list = (IDL_bound_pair_t *)defn_vec_ptr;
    rpc_ss_Z_values_from_bounds( bounds_list, dimensionality, &Z_values,
                                                                   IDL_msp );
    defn_vec_ptr += dimensionality * IDL_FIXED_BOUND_PAIR_WIDTH;
    rpc_ss_ndr_u_fix_or_conf_arr( dimensionality, Z_values, defn_vec_ptr,
                                     array_addr, flags, IDL_msp);
    if (dimensionality > IDL_NORMAL_DIMS)
        rpc_ss_mem_item_free(&IDL_msp->IDL_mem_handle, (byte_p_t)Z_values);
    if (IDL_msp->IDL_type_vec[TVEC_INT_REP_OFFSET] != NDR_LOCAL_INT_REP)
      rpc_ss_mem_item_free(&IDL_msp->IDL_mem_handle, (byte_p_t)bounds_list);
}

/******************************************************************************/
/*                                                                            */
/*  Unmarshall A,B pairs and build a range list from them                     */
/*                                                                            */
/******************************************************************************/
void rpc_ss_ndr_unmar_range_list
#ifdef IDL_PROTOTYPES
(
    /* [in] */  idl_ulong_int dimensionality,
    /* [in] */  idl_byte base_type,     /* Base type of the varying array */
    /* [out] */ IDL_bound_pair_t **p_range_list,
    IDL_msp_t IDL_msp
)
#else
( dimensionality, base_type, p_range_list, IDL_msp )
    idl_ulong_int dimensionality;
    idl_byte base_type;
    IDL_bound_pair_t **p_range_list;
    IDL_msp_t IDL_msp;
#endif
{
    IDL_bound_pair_t *range_list;
    idl_ulong_int i;
    idl_ulong_int A,B;

    if (base_type == IDL_DT_STRING)
        dimensionality--;
    if (*p_range_list == NULL)
    {
        range_list = (IDL_bound_pair_t *)rpc_ss_mem_alloc
          (&IDL_msp->IDL_mem_handle, dimensionality * sizeof(IDL_bound_pair_t));
        *p_range_list = range_list;
    }
    else
        range_list = *p_range_list;

    for (i=0; i<dimensionality; i++)
    {
        IDL_UNMAR_ULONG( &A );
        range_list[i].lower = A;
        IDL_UNMAR_ULONG( &B );
        range_list[i].upper = A + B;
    }
}

/******************************************************************************/
/*                                                                            */
/*  Unmarshall a varying or open array                                        */
/*                                                                            */
/******************************************************************************/
void rpc_ss_ndr_u_var_or_open_arr
#ifdef IDL_PROTOTYPES
(
    /* [in] */  idl_ulong_int dimensionality,
    /* [in] */  idl_ulong_int *Z_values,
    /* [in] */  idl_byte *defn_vec_ptr, /* On entry points at array base info */
    /* [in] */  rpc_void_p_t array_addr,
    /* [in] */ IDL_bound_pair_t *range_list,
    /* [in] */ idl_ulong_int flags,
    IDL_msp_t IDL_msp
)
#else
( dimensionality, Z_values, defn_vec_ptr, array_addr, range_list, flags,
  IDL_msp )
    idl_ulong_int dimensionality;
    idl_ulong_int *Z_values;
    idl_byte *defn_vec_ptr;
    rpc_void_p_t array_addr;
    IDL_bound_pair_t *range_list;
    idl_ulong_int flags;
    IDL_msp_t IDL_msp;
#endif
{
    idl_byte base_type;
    idl_ulong_int element_defn_index;
    idl_ulong_int element_size;
    idl_boolean unmarshall_by_copying;
    rpc_void_p_t copy_addr;    /* Address to be used in unmar by copying */
    idl_boolean contiguous;
    idl_ulong_int element_count;
    IDL_varying_control_t *control_data;
    IDL_varying_control_t normal_control_data[IDL_NORMAL_DIMS];
    int i;
    idl_byte *inner_slice_address;  /* Address of 1-dim subset of array */
    int dim;    /* Index through array dimensions */

    if ( (*defn_vec_ptr == IDL_DT_STRING)
        || (*defn_vec_ptr == IDL_DT_V1_STRING) )
    {
        /* Arrays of strings have a special representation */
        dimensionality--;
    }

    if (Z_values != NULL)   /* NULL possible for transmit_as case */
    {
        for (i=0; i<dimensionality; i++)
        {
            if ((idl_ulong_int)(range_list[i].upper - range_list[i].lower) > 
                Z_values[i])
            {
                /* Bogus data stream with A,B values outside of Z bound value */
                RAISE(rpc_x_invalid_bound);
            }
        }
    }
    for (i=0; i<dimensionality; i++)
    {
        if (range_list[i].upper == range_list[i].lower)
        {
            /* No elements to unmarshall */
            return;
        }
    }

    rpc_ss_ndr_arr_align_and_opt( IDL_unmarshalling_k, dimensionality,
                                  &base_type, defn_vec_ptr,
                                  &unmarshall_by_copying, IDL_msp );


    if (base_type == IDL_DT_REF_PTR)
    {
        /* Arrays of [ref] pointers are not marshalled */
        return;
    }

    if ( (base_type == IDL_DT_STRING)
        || (base_type == IDL_DT_V1_STRING) )
    {
        /* Arrays of strings have a special representation */
        rpc_ss_get_string_base_desc( defn_vec_ptr, &element_size,
                                        &element_defn_index, IDL_msp );
    }
    else
        element_size = rpc_ss_type_size(defn_vec_ptr, IDL_msp);    

    if (unmarshall_by_copying)
    {
        copy_addr = array_addr;
        rpc_ss_ndr_contiguous_elt( dimensionality, Z_values, range_list,
                       element_size, &contiguous, &element_count, &copy_addr );
        if (contiguous)
        {
            if (base_type == IDL_DT_FIXED_STRUCT)
            {
                /* There is a problem because for the last element of the
                structure we may not want to unmarshall sizeof(struct) bytes */
                element_count--;        /* So do the last one separately */
            }
            if (element_count != 0)
            {
                rpc_ss_ndr_unmar_by_copying( element_count, element_size,
                                         copy_addr, IDL_msp );
            }
            if (base_type == IDL_DT_FIXED_STRUCT)
            {
                /* Marshall the last element separately */
                defn_vec_ptr += 4;  /* See comment below */
                IDL_GET_LONG_FROM_VECTOR( element_defn_index, defn_vec_ptr );
                rpc_ss_ndr_unmar_struct(base_type,
                                    IDL_msp->IDL_type_vec + element_defn_index,
                                    (rpc_void_p_t)((idl_byte *)copy_addr
                                                + element_count * element_size),
                                    NULL, NULL, IDL_msp);
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
        rpc_ss_ndr_unmar_by_looping(
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
}

/******************************************************************************/
/*                                                                            */
/*  Unmarshall the Z values for a conformant or open array                    */
/*                                                                            */
/******************************************************************************/
void rpc_ss_ndr_unmar_Z_values
#ifdef IDL_PROTOTYPES
(
    /* [in] */  idl_ulong_int dimensionality,
    /* [out] */ idl_ulong_int **p_Z_values,
    IDL_msp_t IDL_msp
)
#else
( dimensionality, p_Z_values, IDL_msp )
    idl_ulong_int dimensionality;
    idl_ulong_int **p_Z_values;
    IDL_msp_t IDL_msp;
#endif
{
    idl_ulong_int *Z_values;
    idl_ulong_int i;

    if (*p_Z_values == NULL)
    {
        Z_values = (idl_ulong_int *)rpc_ss_mem_alloc(&IDL_msp->IDL_mem_handle,
                                        dimensionality * sizeof(idl_ulong_int));
        *p_Z_values = Z_values;
    }
    else
        Z_values = *p_Z_values;

    for (i=0; i<dimensionality; i++)
    {
        IDL_UNMAR_ULONG( &Z_values[i] );
    }
}

/******************************************************************************/
/*                                                                            */
/*  Unmarshall a varying array                                                */
/*                                                                            */
/******************************************************************************/
void rpc_ss_ndr_unmar_varying_arr
#ifdef IDL_PROTOTYPES
(
    /* [in] */ idl_byte *array_defn_ptr,
                                       /* On entry, points at dimensionality */
    /* [in] */ idl_boolean type_has_pointers,
    /* [in] */ rpc_void_p_t param_addr,
    /* [in] */ idl_ulong_int flags,
    IDL_msp_t IDL_msp
)
#else
( array_defn_ptr, type_has_pointers, param_addr, flags, IDL_msp )
    idl_byte *array_defn_ptr;
    idl_boolean type_has_pointers;
    rpc_void_p_t param_addr;
    idl_ulong_int flags;
    IDL_msp_t IDL_msp;
#endif
{
    idl_ulong_int array_dims;
    IDL_bound_pair_t *bounds_list;
    idl_ulong_int *Z_values;
    idl_ulong_int normal_Z_values[IDL_NORMAL_DIMS];
    IDL_bound_pair_t *range_list;
    IDL_bound_pair_t normal_range_list[IDL_NORMAL_DIMS];

    array_dims = (idl_ulong_int)*array_defn_ptr;
    array_defn_ptr++;
    /* By design array_defn_ptr is now at (0 mod 4) */
    if (IDL_msp->IDL_type_vec[TVEC_INT_REP_OFFSET] != NDR_LOCAL_INT_REP)
      rpc_ss_fixed_bounds_from_vector(array_dims, array_defn_ptr, &bounds_list,
                                    IDL_msp);
    else
      bounds_list = (IDL_bound_pair_t *)array_defn_ptr;
    /* Advance it to point at base type info */
    array_defn_ptr += array_dims * (IDL_FIXED_BOUND_PAIR_WIDTH
                                                + IDL_DATA_LIMIT_PAIR_WIDTH);
    if (array_dims > IDL_NORMAL_DIMS)
    {
        Z_values = NULL;
        range_list = NULL;
    }
    else
    {
        Z_values = normal_Z_values;
        range_list = normal_range_list;
    }
    rpc_ss_Z_values_from_bounds( bounds_list, array_dims, &Z_values, IDL_msp );
    if ( type_has_pointers && (IDL_msp->IDL_side == IDL_server_side_k) )
    {
        rpc_ss_init_new_array_ptrs( array_dims, Z_values,
                                      array_defn_ptr, param_addr, IDL_msp );
    }
    rpc_ss_ndr_unmar_range_list( array_dims, *array_defn_ptr, &range_list,
                                                                     IDL_msp );
    rpc_ss_ndr_u_var_or_open_arr( array_dims, Z_values,
                                  array_defn_ptr, param_addr,
                                  range_list, flags, IDL_msp );
    if (type_has_pointers)
    {
        /* By design array_defn_ptr is now at (0 mod 4) */
        rpc_ss_ndr_u_v_or_o_arr_ptees( array_dims, Z_values,
                                       array_defn_ptr, param_addr,
                                       range_list, IDL_msp );
    }
    if (array_dims > IDL_NORMAL_DIMS)
    {
        rpc_ss_mem_item_free(&IDL_msp->IDL_mem_handle, (byte_p_t)range_list);
        rpc_ss_mem_item_free(&IDL_msp->IDL_mem_handle, (byte_p_t)Z_values);
    }
    if (IDL_msp->IDL_type_vec[TVEC_INT_REP_OFFSET] != NDR_LOCAL_INT_REP)
    rpc_ss_mem_item_free(&IDL_msp->IDL_mem_handle, (byte_p_t)bounds_list);
}

/******************************************************************************/
/*                                                                            */
/*  Allocate an [out]-only conformant/open array                              */
/*                                                                            */
/******************************************************************************/
void rpc_ss_alloc_out_conf_array
#ifdef IDL_PROTOTYPES
(
    /* [in,out] */ idl_byte **p_type_vec_ptr,
                   /* Pointer to type vec pointer, which this routine updates */
    /* [out] */ rpc_void_p_t *p_array_addr,
                   /* Where to return address of allocated array */
    /* [in,out] */ idl_ulong_int *p_num_conf_char_arrays,
                                         /* Number of conformant char arrays */
    IDL_msp_t IDL_msp
)
#else
( p_type_vec_ptr, p_array_addr, p_num_conf_char_arrays, IDL_msp )
    idl_byte **p_type_vec_ptr;
    rpc_void_p_t *p_array_addr;
    idl_ulong_int *p_num_conf_char_arrays;
    IDL_msp_t IDL_msp;
#endif
{
    idl_byte *type_vec_ptr = *p_type_vec_ptr;
    IDL_bound_pair_t *bounds_list;
    IDL_bound_pair_t normal_bounds_list[IDL_NORMAL_DIMS];
    idl_byte array_type;        /* DT_CONF_ARRAY or DT_OPEN_ARRAY */
    idl_boolean type_has_pointers;
    idl_ulong_int array_defn_index;
    idl_byte *array_defn_ptr;
    idl_ulong_int dimensionality;
    idl_ulong_int *Z_values;
    idl_ulong_int normal_Z_values[IDL_NORMAL_DIMS];
    idl_boolean is_string;

    array_type = *type_vec_ptr;
    type_vec_ptr++;
    is_string = (array_type == IDL_DT_STRING);
    if ( is_string || (array_type == IDL_DT_V1_ARRAY) )
    {
        /* Get array type following qualifier */
        array_type = *type_vec_ptr;
        type_vec_ptr++;
    }    
    /* Properties byte */
    type_has_pointers = IDL_PROP_TEST(*type_vec_ptr, IDL_PROP_HAS_PTRS);
    type_vec_ptr++;
    IDL_DISCARD_LONG_FROM_VECTOR(type_vec_ptr);
                                            /* Discard full array definition */
    IDL_GET_LONG_FROM_VECTOR(array_defn_index,type_vec_ptr);
    array_defn_ptr = IDL_msp->IDL_type_vec + array_defn_index;
    dimensionality = (idl_ulong_int)*array_defn_ptr;
    array_defn_ptr++;
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
    rpc_ss_build_bounds_list( &array_defn_ptr, NULL, NULL, NULL, dimensionality,
                              &bounds_list, IDL_msp );
    rpc_ss_Z_values_from_bounds( bounds_list, dimensionality, &Z_values,
                                                                   IDL_msp );
    if (array_type == IDL_DT_OPEN_ARRAY)
    {
        /* Ignore the "varying" information */
        array_defn_ptr += dimensionality * IDL_DATA_LIMIT_PAIR_WIDTH;
    }
    rpc_ss_ndr_alloc_storage( 0, dimensionality, Z_values, array_defn_ptr,
                              p_array_addr,
                              IDL_msp );

    if (type_has_pointers)
    {
        rpc_ss_init_new_array_ptrs( dimensionality, Z_values, array_defn_ptr,
                                    *p_array_addr, IDL_msp );
    }

    if (dimensionality > IDL_NORMAL_DIMS)
    {
        rpc_ss_mem_item_free(&IDL_msp->IDL_mem_handle, (byte_p_t)Z_values);
        rpc_ss_mem_item_free(&IDL_msp->IDL_mem_handle, (byte_p_t)bounds_list);
    }
    *p_type_vec_ptr = type_vec_ptr;
}

/******************************************************************************/
/*                                                                            */
/*  Control for unmarshalling                                                 */
/*                                                                            */
/******************************************************************************/
void rpc_ss_ndr_unmar_interp
#ifdef IDL_PROTOTYPES
(
    idl_ulong_int IDL_parameter_count, /* [in] -- Number of parameters to   */
                                  /* marshall in this call to the           */
                                  /* interpreter                            */

    idl_ulong_int IDL_type_index,    /* [in] -- Offset into the type vector */
                                  /* for the description of the type to be  */
                                  /* marshalled                             */

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
    rpc_void_p_t *p_param_addr;
    idl_ulong_int defn_index;
    idl_ulong_int rtn_index;
    idl_ulong_int iid_index;
    idl_boolean type_has_pointers;
    idl_boolean is_string = idl_false;
    idl_byte *struct_defn_ptr;
    idl_ulong_int offset_index;
    idl_ulong_int *struct_offset_vec_ptr; /* Start of offsets for this struct */
    idl_ulong_int array_defn_index;
    idl_byte *array_defn_ptr;
    idl_ulong_int conf_dims;    /* Number of dimensions of conformance info */
    idl_ulong_int *Z_values=NULL;
    idl_ulong_int normal_Z_values[IDL_NORMAL_DIMS];
    IDL_bound_pair_t *range_list;
    IDL_bound_pair_t normal_range_list[IDL_NORMAL_DIMS];
    IDL_bound_pair_t *bounds_list;
    idl_ulong_int array_dims;   /* Number of dimensions of array */
    idl_ulong_int unique_flag;  /* Wire form of [unique] pointer */
    idl_byte *pointee_defn_ptr;
    IDL_pointee_desc_t pointee_desc;    /* Description of pointee */
    idl_ulong_int switch_value;     /* Discriminant of non-encapsulated union */
    idl_ulong_int node_number;
    idl_ushort_int v1_size;
    idl_ulong_int pseudo_Z_value;   /* v1 conformant size */
    idl_ulong_int num_conf_char_arrays; /* Number of conformant char arrays */
    idl_byte **conf_char_array_list;    /* List of pointers to type vec
                entries for conformant character arrays. Note that this list
                is only allocated if the language is FORTRAN. In this case
                it is written to for all parameters, but the list pointer
                is only advanced for conformant character arrays */
#ifndef _KERNEL
    IDL_cs_shadow_elt_t *param_cs_shadow;
                                         /* cs-shadow for the parameter list */
    idl_ulong_int param_shadow_length;
                                /* Length of the cs-shadow for parameter list */
#endif
    IDL_cs_shadow_elt_t *struct_cs_shadow;
                                    /* cs-shadow for the conformant structure */
    idl_ulong_int struct_shadow_length;
                        /* Length of the cs-shadow for a conformant structure */

     
    if (IDL_msp->IDL_pickling_handle == NULL)
    {
        /* If pickling these variables are already set up */
        IDL_msp->IDL_mp = (idl_byte *)IDL_msp->IDL_elt_p->data_addr;
        IDL_msp->IDL_left_in_buff = IDL_msp->IDL_elt_p->data_len;
    }

    num_conf_char_arrays = 0;
    if ((IDL_msp->IDL_language != IDL_lang_c_k ||
         IDL_msp->IDL_language == IDL_lang_cxx_k
        )
        && (IDL_parameter_count != 0))
    {
        conf_char_array_list = (idl_byte **)rpc_ss_mem_alloc(
                                    &IDL_msp->IDL_mem_handle,
                                    IDL_parameter_count * sizeof(idl_byte *));
    }

    /* Loop over parameters */


    type_vec_ptr = (IDL_msp->IDL_type_vec) + IDL_type_index;
    for ( ; IDL_parameter_count > 0; IDL_parameter_count -- )
    {
        if (IDL_msp->IDL_language != IDL_lang_c_k &&
            IDL_msp->IDL_language != IDL_lang_cxx_k
	)
            conf_char_array_list[num_conf_char_arrays] = type_vec_ptr;
        IDL_GET_LONG_FROM_VECTOR(param_index,type_vec_ptr);

        param_addr = IDL_param_vector[param_index];
        do {
            type_byte = *type_vec_ptr;
            type_vec_ptr++;
            switch(type_byte)
            {
                case IDL_DT_BYTE:
                    IDL_UNMAR_BYTE( param_addr );
                    break;
                case IDL_DT_CHAR:
                    IDL_UNMAR_CHAR( param_addr );
                    break;
                case IDL_DT_BOOLEAN:
                    IDL_UNMAR_BOOLEAN( param_addr );
                    break;
#ifndef _KERNEL
                case IDL_DT_DOUBLE:
                    IDL_UNMAR_DOUBLE( param_addr );
                    break;
#endif
                case IDL_DT_ENUM:
                    IDL_UNMAR_ENUM( param_addr );
                    break;
#ifndef _KERNEL
                case IDL_DT_FLOAT:
                    IDL_UNMAR_FLOAT( param_addr );
                    break;
#endif
                case IDL_DT_SMALL:
                    IDL_UNMAR_SMALL( param_addr );
                    break;
                case IDL_DT_SHORT:
                    IDL_UNMAR_SHORT( param_addr );
                    break;
                case IDL_DT_LONG:
                    IDL_UNMAR_LONG( param_addr );
                    break;
#ifndef _KERNEL
                case IDL_DT_HYPER:
                    IDL_UNMAR_HYPER( param_addr );
                    break;
#endif
                case IDL_DT_USMALL:
                    IDL_UNMAR_USMALL( param_addr );
                    break;
                case IDL_DT_USHORT:
                    IDL_UNMAR_USHORT( param_addr );
                    break;
                case IDL_DT_ULONG:
                    IDL_UNMAR_ULONG( param_addr );
                    break;
#ifndef _KERNEL
                case IDL_DT_UHYPER:
                    IDL_UNMAR_UHYPER( param_addr );
                    break;
                case IDL_DT_V1_ENUM:
                    IDL_UNMAR_V1_ENUM( param_addr );
                    break;
#endif
                case IDL_DT_ERROR_STATUS:
                    IDL_UNMAR_ERROR_STATUS( param_addr );
                    break;
                case IDL_DT_FIXED_STRUCT:
                    /* Properties byte */
                    type_has_pointers =
                             IDL_PROP_TEST(*type_vec_ptr, IDL_PROP_HAS_PTRS);
                    type_vec_ptr++;
                    IDL_GET_LONG_FROM_VECTOR(defn_index,type_vec_ptr);
                    if ( type_has_pointers 
                            && (IDL_msp->IDL_side == IDL_server_side_k) )
                    {
                        rpc_ss_init_new_struct_ptrs( type_byte,
                                            IDL_msp->IDL_type_vec+defn_index,
                                            param_addr, NULL, IDL_msp );
                    }
                    rpc_ss_ndr_unmar_struct(type_byte,
                                            IDL_msp->IDL_type_vec+defn_index,
                                            param_addr, NULL, NULL, IDL_msp);
                    if (type_has_pointers)
                    {
                        rpc_ss_ndr_u_struct_pointees(type_byte, defn_index,
                                                param_addr, Z_values, IDL_msp);
                    }
                    break;
                case IDL_DT_CONF_STRUCT:
#ifndef _KERNEL
                case IDL_DT_V1_CONF_STRUCT:
#endif
                    /* Properties byte */
                    type_has_pointers =
                             IDL_PROP_TEST(*type_vec_ptr, IDL_PROP_HAS_PTRS);
                    type_vec_ptr++;
                    IDL_GET_LONG_FROM_VECTOR(defn_index, type_vec_ptr);
                    struct_defn_ptr = IDL_msp->IDL_type_vec + defn_index;
                    IDL_GET_LONG_FROM_VECTOR(offset_index,struct_defn_ptr);
                    struct_offset_vec_ptr = IDL_msp->IDL_offset_vec
                                                                 + offset_index;
                    IDL_GET_LONG_FROM_VECTOR(array_defn_index,struct_defn_ptr);
                    array_defn_ptr = IDL_msp->IDL_type_vec + array_defn_index;
#ifndef _KERNEL
                    if (*struct_defn_ptr == IDL_DT_CS_SHADOW)
                    {
                        struct_defn_ptr++;
                        IDL_GET_LONG_FROM_VECTOR(struct_shadow_length,
                                                            struct_defn_ptr);
                        struct_cs_shadow = (IDL_cs_shadow_elt_t *)
                                            rpc_ss_mem_alloc
                                             (&IDL_msp->IDL_mem_handle, 
                                              struct_shadow_length * 
                                                sizeof(IDL_cs_shadow_elt_t));
                    }
                    else
#endif
                        struct_cs_shadow = NULL;
                    conf_dims = (idl_ulong_int)*array_defn_ptr;
                    array_defn_ptr++;
                    if (conf_dims > IDL_NORMAL_DIMS)
                        Z_values = NULL;
                    else
                        Z_values = normal_Z_values;
#ifndef _KERNEL
                    if (type_byte == IDL_DT_V1_CONF_STRUCT)
                    {
                        IDL_UNMAR_CUSHORT( &v1_size );
                        *Z_values = (idl_ulong_int)v1_size;
                    }
                    else
#endif
                        rpc_ss_ndr_unmar_Z_values( conf_dims, &Z_values,
                                                                     IDL_msp );
                    /* Skip over the bounds in the array defn
                                                 to get to the base type */
                    IDL_ADV_DEFN_PTR_OVER_BOUNDS( array_defn_ptr, conf_dims );
#ifndef _KERNEL
                    if (*array_defn_ptr == IDL_DT_CS_TYPE)
                    {
                        rpc_ss_ndr_u_conf_cs_struct_hdr(
                          IDL_msp->IDL_type_vec + defn_index,
                          array_defn_ptr - IDL_CONF_BOUND_PAIR_WIDTH,
                          Z_values, *struct_offset_vec_ptr,
                          type_has_pointers,
                          struct_shadow_length - 1,
                          (idl_boolean)(IDL_msp->IDL_side == IDL_server_side_k),
                          struct_cs_shadow,
                          &IDL_param_vector[param_index], IDL_msp);
                    }
                    else
#endif
                    {
                        if (IDL_msp->IDL_side == IDL_server_side_k)
                        {
#ifndef _KERNEL
                            if (type_byte == IDL_DT_V1_CONF_STRUCT)
                                rpc_ss_ndr_alloc_storage(*struct_offset_vec_ptr,
                                    1, Z_values, array_defn_ptr,
                                    &IDL_param_vector[param_index], IDL_msp);
                            else
#endif
                                rpc_ss_ndr_alloc_storage(*struct_offset_vec_ptr,
                                    conf_dims, Z_values, array_defn_ptr,
                                    &IDL_param_vector[param_index], IDL_msp);
                            if (type_has_pointers)
                            {
                                rpc_ss_init_new_struct_ptrs( type_byte,
                                            IDL_msp->IDL_type_vec+defn_index,
                                            IDL_param_vector[param_index],
                                            Z_values, IDL_msp);
                            }
                        }
                    }
                    rpc_ss_ndr_unmar_struct(type_byte, 
                                            IDL_msp->IDL_type_vec+defn_index,
                                             IDL_param_vector[param_index],
                                             Z_values, struct_cs_shadow,
                                             IDL_msp);
                    if (type_has_pointers)
                    {
                        rpc_ss_ndr_u_struct_pointees(type_byte, defn_index,
                                 IDL_param_vector[param_index],
                                 Z_values, IDL_msp);
                    }
                    if (conf_dims > IDL_NORMAL_DIMS)
                        rpc_ss_mem_item_free(&IDL_msp->IDL_mem_handle,
                                                (byte_p_t)Z_values);
                    break;
                case IDL_DT_FIXED_ARRAY:
                    /* Properties byte */
                    type_has_pointers =
                             IDL_PROP_TEST(*type_vec_ptr, IDL_PROP_HAS_PTRS);
                    type_vec_ptr++;
                    IDL_DISCARD_LONG_FROM_VECTOR(type_vec_ptr);
                                            /* Discard full array definition */
                    IDL_GET_LONG_FROM_VECTOR(defn_index,type_vec_ptr);
                    if ( type_has_pointers 
                            && (IDL_msp->IDL_side == IDL_server_side_k) )
                    {
                        array_defn_ptr = IDL_msp->IDL_type_vec + defn_index;
                        array_dims = (idl_ulong_int)*array_defn_ptr;
                        array_defn_ptr++;
                        /* By design array_defn_ptr is now at (0 mod 4) */
			if (IDL_msp->IDL_type_vec[TVEC_INT_REP_OFFSET] !=
			    NDR_LOCAL_INT_REP)
                          rpc_ss_fixed_bounds_from_vector(array_dims,
                                                          array_defn_ptr, 
                                                          &bounds_list, 
                                                          IDL_msp);
			else
                          bounds_list = (IDL_bound_pair_t *)array_defn_ptr;
                        /* Advance it to point at base type info */
                        array_defn_ptr += array_dims
                                                 * IDL_FIXED_BOUND_PAIR_WIDTH;
                        if (array_dims > IDL_NORMAL_DIMS)
                            Z_values = NULL;
                        else
                            Z_values = normal_Z_values;
                        rpc_ss_Z_values_from_bounds( bounds_list, array_dims,
                                                           &Z_values, IDL_msp );
                        rpc_ss_init_new_array_ptrs( array_dims, Z_values,
                                          array_defn_ptr, param_addr, IDL_msp );
                        if (array_dims > IDL_NORMAL_DIMS)
                            rpc_ss_mem_item_free(&IDL_msp->IDL_mem_handle,
                                                            (byte_p_t)Z_values);
			if (IDL_msp->IDL_type_vec[TVEC_INT_REP_OFFSET] != 
			    NDR_LOCAL_INT_REP)
			rpc_ss_mem_item_free(&IDL_msp->IDL_mem_handle,
                                                         (byte_p_t)bounds_list);
                    }
                    rpc_ss_ndr_unmar_fixed_arr(defn_index, param_addr,
                                               IDL_M_IS_PARAM, IDL_msp);
                    if (type_has_pointers)
                    {
                        rpc_ss_ndr_u_fixed_arr_ptees(defn_index, param_addr,
                                                                       IDL_msp);
                    }
                    break;
                case IDL_DT_VARYING_ARRAY:
                    /* Properties byte */
                    type_has_pointers = IDL_PROP_TEST(*type_vec_ptr,
                                                             IDL_PROP_HAS_PTRS);
                    type_vec_ptr++;
                    IDL_DISCARD_LONG_FROM_VECTOR(type_vec_ptr);
                                            /* Discard full array definition */
                    IDL_GET_LONG_FROM_VECTOR(array_defn_index, type_vec_ptr);
                    array_defn_ptr = IDL_msp->IDL_type_vec + array_defn_index;
                    rpc_ss_ndr_unmar_varying_arr( array_defn_ptr,
                             type_has_pointers, param_addr,
                             IDL_M_IS_PARAM | (is_string ? IDL_M_IS_STRING : 0),
                             IDL_msp );
                    is_string = idl_false;
                    break;
                case IDL_DT_CONF_ARRAY:
                    /* Properties byte */
                    type_has_pointers =
                             IDL_PROP_TEST(*type_vec_ptr, IDL_PROP_HAS_PTRS);
                    type_vec_ptr++;
                    IDL_DISCARD_LONG_FROM_VECTOR(type_vec_ptr);
                                            /* Discard full array definition */
                    IDL_GET_LONG_FROM_VECTOR(array_defn_index,type_vec_ptr);
                    array_defn_ptr = IDL_msp->IDL_type_vec + array_defn_index; 
                    conf_dims = (idl_ulong_int)*array_defn_ptr;
                    array_defn_ptr++;
                    IDL_ADV_DEFN_PTR_OVER_BOUNDS( array_defn_ptr, conf_dims );
                    if (conf_dims > IDL_NORMAL_DIMS)
                        Z_values = NULL;
                    else
                        Z_values = normal_Z_values;
                    rpc_ss_ndr_unmar_Z_values( conf_dims, &Z_values, IDL_msp );
                    if (IDL_msp->IDL_side == IDL_server_side_k)
                    {
                        p_param_addr = &IDL_param_vector[param_index];
                        rpc_ss_ndr_alloc_storage( 0, conf_dims, Z_values,
                                                 array_defn_ptr, p_param_addr,
                                                 IDL_msp );
                        if (type_has_pointers)
                        {
                            rpc_ss_init_new_array_ptrs( conf_dims, Z_values,
                                                 array_defn_ptr,
                                                 IDL_param_vector[param_index],
                                                 IDL_msp );
                        }
                    }
                    rpc_ss_ndr_u_fix_or_conf_arr( conf_dims, Z_values,
                                              array_defn_ptr,
                                              IDL_param_vector[param_index],
                                              IDL_M_IS_PARAM | IDL_M_CONF_ARRAY,
                                              IDL_msp );
                    if (type_has_pointers)
                    {
                        rpc_ss_ndr_u_f_or_c_arr_ptees( conf_dims, Z_values,
                                                 array_defn_ptr,
                                                 IDL_param_vector[param_index],
                                                 IDL_msp );
                    }
                    if (conf_dims > IDL_NORMAL_DIMS)
                        rpc_ss_mem_item_free(&IDL_msp->IDL_mem_handle,
                                                (byte_p_t)Z_values);
                    break;
                case IDL_DT_OPEN_ARRAY:
                    /* Properties byte */
                    type_has_pointers =
                             IDL_PROP_TEST(*type_vec_ptr, IDL_PROP_HAS_PTRS);
                    type_vec_ptr++;
                    IDL_DISCARD_LONG_FROM_VECTOR(type_vec_ptr);
                                            /* Discard full array definition */
                    IDL_GET_LONG_FROM_VECTOR(defn_index,type_vec_ptr);
                    array_defn_ptr = IDL_msp->IDL_type_vec + defn_index; 
                    conf_dims = (idl_ulong_int)*array_defn_ptr;
                    array_defn_ptr++;
                    IDL_ADV_DEFN_PTR_OVER_BOUNDS( array_defn_ptr, conf_dims );
                    if (conf_dims > IDL_NORMAL_DIMS)
                    {
                        Z_values = NULL;
                        range_list = NULL;
                    }
                    else
                    {
                        Z_values = normal_Z_values;
                        range_list = normal_range_list;
                    }
                    rpc_ss_ndr_unmar_Z_values( conf_dims, &Z_values, IDL_msp );
                    /* Advance array_defn_ptr over data limit info */
                    array_defn_ptr += conf_dims * IDL_DATA_LIMIT_PAIR_WIDTH;
                    if (IDL_msp->IDL_side == IDL_server_side_k)
                    {
                            p_param_addr = &IDL_param_vector[param_index];
                        rpc_ss_ndr_alloc_storage( 0, conf_dims, Z_values,
                                                 array_defn_ptr, p_param_addr,
                                                 IDL_msp );
                        if (type_has_pointers)
                        {
                            rpc_ss_init_new_array_ptrs( conf_dims, Z_values,
                                                 array_defn_ptr,
                                                 IDL_param_vector[param_index],
                                                 IDL_msp );
                        }
                    }
                    rpc_ss_ndr_unmar_range_list( conf_dims, *array_defn_ptr,
                                                 &range_list, IDL_msp );
                    rpc_ss_ndr_u_var_or_open_arr( conf_dims, Z_values,
                             array_defn_ptr, IDL_param_vector[param_index],
                             range_list, IDL_M_CONF_ARRAY | IDL_M_IS_PARAM
                                           | (is_string ? IDL_M_IS_STRING : 0),
                             IDL_msp );
                    if (type_has_pointers)
                    {
                        rpc_ss_ndr_u_v_or_o_arr_ptees( conf_dims, Z_values,
                                                 array_defn_ptr,
                                                 IDL_param_vector[param_index],
                                                 range_list, IDL_msp );
                    }
                    if (conf_dims > IDL_NORMAL_DIMS)
                    {
                        rpc_ss_mem_item_free(&IDL_msp->IDL_mem_handle,
                                                (byte_p_t)Z_values);
                        rpc_ss_mem_item_free(&IDL_msp->IDL_mem_handle,
                                                (byte_p_t)range_list);
                    }
                    is_string = idl_false;
                    break;
                case IDL_DT_ENC_UNION:
                    /* Properties byte */
                    type_has_pointers =
                             IDL_PROP_TEST(*type_vec_ptr, IDL_PROP_HAS_PTRS);
                    type_vec_ptr++;
                    IDL_GET_LONG_FROM_VECTOR(defn_index,type_vec_ptr);
                    rpc_ss_ndr_u_enc_union_or_ptees( param_addr, defn_index,
                                                     idl_false, IDL_msp);
                    if (type_has_pointers)
                    {
                        rpc_ss_ndr_u_enc_union_or_ptees( param_addr, defn_index,
                                                         idl_true, IDL_msp);
                    }
                    break;
#ifndef _KERNEL
                case IDL_DT_N_E_UNION:
                    /* Properties byte */
                    type_has_pointers =
                             IDL_PROP_TEST(*type_vec_ptr, IDL_PROP_HAS_PTRS);
                    type_vec_ptr++;
                    IDL_DISCARD_LONG_FROM_VECTOR(type_vec_ptr);
                                                     /* Discard switch index */
                    IDL_GET_LONG_FROM_VECTOR(defn_index,type_vec_ptr);
                    rpc_ss_ndr_unmar_n_e_union( param_addr, defn_index,
                                                     &switch_value, IDL_msp);
                    if (type_has_pointers)
                    {
                        rpc_ss_ndr_u_n_e_union_ptees( param_addr, switch_value,
                                            0, defn_index, NULL, NULL, IDL_msp);
                    }
                    break;
#endif
                case IDL_DT_PASSED_BY_REF:
                    break;
                case IDL_DT_FULL_PTR:
                    type_vec_ptr++;     /* Properties byte */
                    IDL_GET_LONG_FROM_VECTOR(defn_index,type_vec_ptr);
                    /* Unmarshall the node number */
                    IDL_UNMAR_ULONG( &node_number );

#ifdef SGIMIPS
                    *(rpc_void_p_t *)(IDL_param_vector[param_index])
				      = (rpc_void_p_t)(__psint_t)node_number;
#else
                    *(rpc_void_p_t *)(IDL_param_vector[param_index])
                                                    = (rpc_void_p_t)node_number;
#endif /* SGIMIPS */ 
                    if (node_number != 0)
                    {
                        pointee_defn_ptr = IDL_msp->IDL_type_vec+defn_index;
                        pointee_desc.dimensionality = 0;
                        rpc_ss_ndr_unmar_pointee_desc( type_byte,
                                                pointee_defn_ptr,
                                                &pointee_desc,
                                                IDL_param_vector[param_index],
                                                IDL_msp );
                        rpc_ss_ndr_unmar_pointee( type_byte,
                                                pointee_defn_ptr,
                                                &pointee_desc,
                                                IDL_param_vector[param_index],
                                                IDL_msp );
                        rpc_ss_ndr_u_rlse_pointee_desc(&pointee_desc, IDL_msp);
                    }
                    break;
#ifndef _KERNEL
                case IDL_DT_UNIQUE_PTR:
                    type_vec_ptr++;     /* Properties byte */
                    IDL_GET_LONG_FROM_VECTOR(defn_index,type_vec_ptr);
                    IDL_UNMAR_ULONG( &unique_flag );
                    if (unique_flag == 0)
                    {
                        *(rpc_void_p_t *)(IDL_param_vector[param_index])
                                                                         = NULL;
                    }
                    else
                    {
                        pointee_defn_ptr = IDL_msp->IDL_type_vec + defn_index;
                        pointee_desc.dimensionality = 0;
                        rpc_ss_ndr_unmar_pointee_desc( type_byte,
                                                pointee_defn_ptr,
                                                &pointee_desc,
                                                IDL_param_vector[param_index],
                                                IDL_msp );
                        if ((IDL_msp->IDL_side == IDL_server_side_k)
                           || (*(rpc_void_p_t *)(IDL_param_vector[param_index])
                                                                       == NULL))
                        {
                            *(rpc_void_p_t *)(IDL_param_vector[param_index])
                                                                 = IDL_NEW_NODE;
                        }
                        rpc_ss_ndr_unmar_pointee( type_byte, pointee_defn_ptr,
                                                &pointee_desc,
                                                IDL_param_vector[param_index],
                                                IDL_msp );
                        rpc_ss_ndr_u_rlse_pointee_desc( &pointee_desc,
                                                                IDL_msp );
                    }
                    break;
#endif
                case IDL_DT_REF_PTR:
                    type_vec_ptr++;     /* Properties byte */
                    IDL_GET_LONG_FROM_VECTOR(defn_index,type_vec_ptr);
                    pointee_defn_ptr = IDL_msp->IDL_type_vec+defn_index;
                    if (IDL_msp->IDL_side == IDL_server_side_k)
                    {
                        rpc_ss_alloc_pointer_target(pointee_defn_ptr,
                                               IDL_param_vector[param_index],
                                               IDL_msp );
                    }
                    pointee_desc.dimensionality = 0;
                    rpc_ss_ndr_unmar_pointee_desc( type_byte, pointee_defn_ptr,
                                                &pointee_desc,
                                                IDL_param_vector[param_index],
                                                IDL_msp );
                    rpc_ss_ndr_unmar_pointee( type_byte, pointee_defn_ptr,
                                                &pointee_desc,
                                                IDL_param_vector[param_index],
                                                IDL_msp );
                    rpc_ss_ndr_u_rlse_pointee_desc( &pointee_desc, IDL_msp );
                    break;
                case IDL_DT_STRING:
                    /* Varying/open array code will do the right thing */
                    is_string = idl_true;
                    break;
                case IDL_DT_TRANSMIT_AS:
                case IDL_DT_REPRESENT_AS:
                    type_vec_ptr++;     /* Properties byte */
                    IDL_GET_LONG_FROM_VECTOR(defn_index, type_vec_ptr);
                    rpc_ss_ndr_unmar_xmit_as(defn_index,
                                             IDL_param_vector[param_index],
                                             NULL, IDL_msp );
                    break;
                case IDL_DT_INTERFACE:
                    type_vec_ptr++;     /* Properties byte */
                    IDL_GET_LONG_FROM_VECTOR(defn_index, type_vec_ptr);
                    rpc_ss_ndr_unmar_interface(defn_index,
                        		       IDL_param_vector[param_index],
                        		       NULL,
					       IDL_msp );
                    break;
                case IDL_DT_DYN_INTERFACE:
                    type_vec_ptr++;     /* Properties byte */
                    IDL_GET_LONG_FROM_VECTOR(rtn_index, type_vec_ptr);
                    IDL_GET_LONG_FROM_VECTOR(iid_index, type_vec_ptr);
                    rpc_ss_ndr_unmar_dyn_interface(rtn_index,
                        		       IDL_param_vector[param_index],
                        		       ((uuid_t *) IDL_param_vector[iid_index]),
                        		       NULL,
					       IDL_msp );
                    break;
                case IDL_DT_ALLOCATE:
                    /* Indicates an [out]-only conformant or open array must
                        be allocated. Should only appear on server side */
                    rpc_ss_alloc_out_conf_array( &type_vec_ptr, 
                                                 &IDL_param_vector[param_index],
                                                 &num_conf_char_arrays,
                                                 IDL_msp );
                    break;
                case IDL_DT_ALLOCATE_REF:
                    /* Indicates that the pointees of [ref] pointers in an [out]
                        only parameter must be allocated. Should only appear
                        on server side */
                    rpc_ss_init_out_ref_ptrs( &type_vec_ptr, param_addr,
                                                                     IDL_msp );
                    break;
                case IDL_DT_PIPE:
                    if (IDL_msp->IDL_side == IDL_server_side_k)
                    {
                        /* Rest of unmarshalling is done by manager calls */
                        return;
                    }
                    else
                    {
                        type_vec_ptr++;     /* Properties byte */
                        IDL_GET_LONG_FROM_VECTOR(defn_index,type_vec_ptr);
                        rpc_ss_ndr_unmar_pipe(defn_index, param_addr, IDL_msp);
                    }
                    break;
                case IDL_DT_IN_CONTEXT:
                case IDL_DT_IN_OUT_CONTEXT:
                case IDL_DT_OUT_CONTEXT:
                    rpc_ss_ndr_unmar_context(type_byte, param_addr, IDL_msp);
                    break;
#ifndef _KERNEL
                case IDL_DT_V1_ARRAY:
                    type_byte = *type_vec_ptr;  /* DT_VARYING or DT_OPEN */
                    type_vec_ptr += 2;       /* Skip over properties byte */
                    IDL_DISCARD_LONG_FROM_VECTOR(type_vec_ptr);
                                            /* Discard full array definition */
                    IDL_GET_LONG_FROM_VECTOR(defn_index,type_vec_ptr);
                    array_defn_ptr = IDL_msp->IDL_type_vec + defn_index; 
                    array_dims = (idl_ulong_int)*array_defn_ptr;
                    array_defn_ptr++;
                    if (type_byte == IDL_DT_OPEN_ARRAY)
                    {
                        IDL_ADV_DEFN_PTR_OVER_BOUNDS( array_defn_ptr,
                                                                  array_dims );
                        /* Advance array_defn_ptr over data limit info */
                        array_defn_ptr += array_dims
                                                    * IDL_DATA_LIMIT_PAIR_WIDTH;
                        IDL_UNMAR_CUSHORT( &v1_size );
                        if (IDL_msp->IDL_side == IDL_server_side_k)
                        {
                            pseudo_Z_value = (idl_ulong_int)v1_size;
                            rpc_ss_ndr_alloc_storage( 0, 1, &pseudo_Z_value,
                                                 array_defn_ptr,
                                                 &IDL_param_vector[param_index],
                                                 IDL_msp );
                        }
                    }
                    else    /* IDL_DT_VARYING_ARRAY */
                    {
                        array_defn_ptr += array_dims
                                             * (IDL_FIXED_BOUND_PAIR_WIDTH
                                                + IDL_DATA_LIMIT_PAIR_WIDTH);
                    }
                    rpc_ss_ndr_u_v1_varying_arr(IDL_param_vector[param_index],
                                                array_defn_ptr, IDL_M_IS_PARAM,
                                                IDL_msp );
                    break;
#endif
#ifndef _KERNEL
                case IDL_DT_V1_STRING:
                    type_vec_ptr += 2;  /* DT_VARYING_ARRAY and properties */
                    IDL_DISCARD_LONG_FROM_VECTOR(type_vec_ptr);
                                                        /* Full array defn */
                    IDL_DISCARD_LONG_FROM_VECTOR(type_vec_ptr);
                                                      /* Flattened array defn */
                    rpc_ss_ndr_unmar_v1_string(param_addr, IDL_M_IS_PARAM,
                                                                     IDL_msp);
                    break;
#endif
                case IDL_DT_FREE_REP:
                    /* Just needs skipping over */
                    IDL_DISCARD_LONG_FROM_VECTOR(type_vec_ptr);
                    break;
#ifndef _KERNEL
                case IDL_DT_DELETED_NODES:
                    rpc_ss_ndr_unmar_deletes(IDL_msp);
                    break;
                case IDL_DT_CS_SHADOW:
                    IDL_GET_LONG_FROM_VECTOR(param_shadow_length, type_vec_ptr);
                    param_cs_shadow = (IDL_cs_shadow_elt_t *)
                                            rpc_ss_mem_alloc
                                             (&IDL_msp->IDL_mem_handle, 
                                              param_shadow_length *
                                                sizeof(IDL_cs_shadow_elt_t));
                    break;
                case IDL_DT_CS_ARRAY:
                    if (*type_vec_ptr == IDL_DT_ALLOCATE)
                    {
                        rpc_ss_alloc_out_cs_conf_array(param_cs_shadow,
                                                 &type_vec_ptr,
                                                 &IDL_param_vector[param_index],
                                                 IDL_msp);
                    }
                    else
                    {
                        rpc_ss_ndr_u_cs_array_param(&type_vec_ptr,
                                         param_cs_shadow, param_index, IDL_msp);
                    }
                    break;
                case IDL_DT_CS_ATTRIBUTE:
                    /* Unmarshall the parameter normally. It will get
                        overwritten later */
                    break;
                case IDL_DT_CS_TYPE:
                    type_vec_ptr++;     /* Properties byte */
                    IDL_GET_LONG_FROM_VECTOR(defn_index, type_vec_ptr);
                    rpc_ss_ndr_unmar_cs_char(param_addr, defn_index, IDL_msp);
                    break;
                case IDL_DT_CS_RLSE_SHADOW:
                    rpc_ss_ndr_u_param_cs_shadow(
                                      IDL_type_index, param_cs_shadow, IDL_msp);
                    break;
#endif
                case IDL_DT_EOL:
                    break;
                default:
#ifdef DEBUG_INTERP
                    printf("rpc_ss_ndr_unmar_interp:unrecognized type %d\n",
                        type_byte);
                    exit(0);
#endif
                    RAISE(rpc_x_coding_error);
            }
        } while (type_byte != IDL_DT_EOL);
    }
    if ((IDL_msp->IDL_language != IDL_lang_c_k &&
         IDL_msp->IDL_language != IDL_lang_cxx_k
    )
        && (IDL_parameter_count != 0))
    {
        rpc_ss_mem_item_free(&IDL_msp->IDL_mem_handle, 
                                (byte_p_t)conf_char_array_list);
    }

    if (IDL_msp->IDL_pickling_handle == NULL)
    {
        if ((IDL_msp->IDL_elt_p->data_len != 0) &&
	    (IDL_msp->IDL_elt_p->buff_dealloc != NULL))
            (*(IDL_msp->IDL_elt_p->buff_dealloc))
                                                (IDL_msp->IDL_elt_p->buff_addr);
        IDL_msp->IDL_elt_p = NULL;
    }
    else
    {
        /* For pickling, buffer was rounded out to a multiple of 8 bytes */
        IDL_UNMAR_ALIGN_MP(IDL_msp, 8);
    }
}
