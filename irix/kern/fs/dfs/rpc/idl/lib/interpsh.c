/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(_KERNEL)
static const char *_identString = "$Id: interpsh.c,v 65.5 1998/03/23 17:25:45 gwehrman Exp $";
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
 * $Log: interpsh.c,v $
 * Revision 65.5  1998/03/23 17:25:45  gwehrman
 * Source Identification changed to bracket with SGIMIPS && !_KERNEL.
 *
 * Revision 65.4  1998/01/07 17:20:06  jdoak
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS
 *
 * Revision 65.3  1997/11/06  19:56:14  jdoak
 * Source Identification added.
 *
 * Revision 65.2  1997/10/20  19:15:36  jdoak
 * Initial IRIX 6.4 code merge
 *
 * Revision 1.1.11.1  1996/10/03  14:40:50  arvind
 * 	Quiet purify.
 * 	[1996/09/16  19:56 UTC  sommerfeld  /main/sommerfeld_pk_kdc_1/1]
 *
 * Revision 1.1.9.2  1996/02/18  18:54:30  marty
 * 	Update OSF copyright years
 * 	[1996/02/18  18:06:57  marty]
 * 
 * Revision 1.1.9.1  1995/12/07  22:28:35  root
 * 	Submit OSF/DCE 1.2.1
 * 
 * 	HP revision /main/dat_xidl2/1  1995/11/17  17:06 UTC  dat
 * 	Merge second XIDL drop for DCE 1.2.1
 * 	[1995/12/07  21:15:23  root]
 * 
 * Revision 1.1.2.1  1995/10/23  01:49:05  bfc
 * 	oct 95 idl drop
 * 	[1995/10/22  23:35:55  bfc]
 * 
 * 	may 95 idl drop
 * 	[1995/10/22  22:57:07  bfc]
 * 
 * 	DCE for DEC OSF/1: populate from OSF DCE R1.1
 * 	[1995/10/21  18:25:25  bfc]
 * 
 * Revision 1.1.4.2  1994/08/26  17:02:44  tom
 * 	Bug 11901 - Accept version 3.2 in rpc_ss_type_vec_vers_check.
 * 	[1994/08/26  15:11:24  tom]
 * 
 * Revision 1.1.4.1  1994/07/13  18:56:11  tom
 * 	Bug 10103 - Reduce little-endian bias of stubs
 * 	[1994/07/12  18:49:28  tom]
 * 
 * Revision 1.1.2.2  1993/07/07  20:08:03  ganni
 * 	reduced stub idl sources
 * 	[1993/07/07  19:37:41  ganni]
 * 
 * $EndLog$
 */
/*
**  Copyright (c) Digital Equipment Corporation, 1991, 1993
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
**      interpsh.c
**
**  FACILITY:
**
**      Interface Definition Language (IDL) Compiler
**
**  ABSTRACT:
**
**      Procedures shared between interpreters
**
*/

#include <dce/idlddefs.h>
#include <lsysdep.h>

/******************************************************************************/
/*                                                                            */
/*  General comments about interpreter structure, variable name, etc.         */
/*                                                                            */
/******************************************************************************/

/*
 *  For a description of the layout of the vectors on which the interpreters
 *  rely, see the MTS*.SDML documents in the RPC$ROOT:[DOC.CMS] CMS library
 */

/*
    A range list is a set of [A, A+B] pairs for a varying array.
*/

/*
    p_...   Signifies a pointer to an object

    IDL_msp         Pointer to the marshalling state block
    add_null        TRUE => marshalling a string to which a null terminator
                        must be explicitly added
    arm_type_ptr    Pointer to union arm type definition
    array_addr      Address of an array
    array_defn_index Index into definition vector for array definition
    array_defn_ptr  Pointer to array definition in definition vector
    array_has_pointers TRUE if the base type of the array includes pointers
    bounds_list     List of lower and upper bounds for an array
    contiguous      TRUE if no gaps in set of array elements
    cs_type_defn_index Index into definition vector for [cs_char] type defn
    cs_type_defn_ptr Pointer to [cs_char] type definition in definition vector
    defn_index      Index into definition vector
    defn_vec_ptr    Pointer to current byte in definition vector
    dimensionality  Number of dimensions of an array
    element_count   Number of elements in an array or array portion
    element_defn_index Index into definition vector for description of array
                                                element which has complex type
    element_defn_ptr  Pointer to description in definition vector of array
                                                element which has complex type
    element_offset_vec_ptr Pointer to the start of the offset vector entries
                            for an array element of complex type
    element_size    Size of an array element 
    field_defn_index Index into definition vector for description of field
                                                        which has complex type
    field_defn_ptr  Pointer to description in definition vector of field
                                                        which has complex type
    marshall_by_pointing TRUE if array can be marshalled by pointing
    normal_...      Storage used for array description unless array has more
                        than IDL_NORMAL_DIMS dimensions
    offset_index    Index into offset vector
    offset_vec_ptr  Pointer to current element in offset vector
    param_addr      Address of parameter
    param_index     Index of parameter address in parameter vector
    range_list      List of (A,A+B) pairs determining an array's data limits
    pointee_defn_index Index into definition vector for pointee definition
    pointee_defn_ptr  Pointer to pointee definition in definition vector
    struct_addr     Address of a structure
    struct_defn_index Index into definition vector for structure definition
    struct_defn_ptr Pointer to structure definition in definition vector
    struct_offset_index Index of start of structure offsets in offset vector
    struct_offset_vec_ptr Pointer to the start of the offset vector entries
                            for a structure
    struct_size     Size of a structure
    switch_value    Value of union discriminant
    type_byte       Byte describing type of datum currently being handled
    type_is_modified TRUE if a modifier byte occurred before a type byte
    type_vec_ptr    Pointer to parameter type in type/definition vector
    unmarshall_by_copying TRUE if array can be unmarshalled by copying
    Z_values        List of dimension sizes for conformant array
*/

/******************************************************************************/
/*                                                                            */
/*  Function returning the local size of an object of a specified type        */
/*  For [transmit_as] returns the size of the presented type                  */
/*  For a varying array returns the storage size of the array                 */
/*  For a type which does not have a fixed size, returns 0                    */
/*                                                                            */
/******************************************************************************/
idl_ulong_int rpc_ss_type_size
#ifdef IDL_PROTOTYPES
(
    /* [in] */  idl_byte *defn_vec_ptr, /* Pointer to start of type 
                                        specification in definition vector */
    IDL_msp_t IDL_msp
)
#else
( defn_vec_ptr, IDL_msp )
    idl_byte *defn_vec_ptr;
    IDL_msp_t IDL_msp;
#endif
{
    idl_byte type;
    idl_ulong_int complex_defn_index; /* Index to description of complex type */
    idl_byte *complex_defn_ptr;     /* Pointer to description of complex type */
    idl_ulong_int offset_index;
    idl_ulong_int array_defn_index;
    idl_byte *array_defn_ptr;
    idl_ulong_int dimensionality;
    idl_ulong_int i;
    idl_ulong_int array_element_count;
    idl_long_int lower_bound;
    idl_long_int upper_bound;
    idl_byte modified_type;

    type = *defn_vec_ptr;
    switch (type)
    {
        case IDL_DT_BOOLEAN:
            return(sizeof(idl_boolean));
        case IDL_DT_BYTE:
            return(sizeof(idl_byte));
        case IDL_DT_CHAR:
            return(sizeof(idl_char));
        case IDL_DT_DOUBLE:
            return(sizeof(idl_long_float));
        case IDL_DT_ENUM:
#ifndef _KERNEL
        case IDL_DT_V1_ENUM:
#endif
            return(sizeof(int));
        case IDL_DT_FLOAT:
            return(sizeof(idl_short_float));
        case IDL_DT_SMALL:
                return(sizeof(idl_small_int));
        case IDL_DT_SHORT:
            return(sizeof(idl_short_int));
        case IDL_DT_LONG:
            return(sizeof(idl_long_int));
#ifndef _KERNEL
        case IDL_DT_HYPER:
            return(sizeof(idl_hyper_int));
#endif
        case IDL_DT_USMALL:
                return(sizeof(idl_usmall_int));
        case IDL_DT_USHORT:
                return(sizeof(idl_ushort_int));
        case IDL_DT_ULONG:
        case IDL_DT_ERROR_STATUS:
            return(sizeof(idl_ulong_int));
#ifndef _KERNEL
        case IDL_DT_UHYPER:
            return(sizeof(idl_uhyper_int));
#endif
        case IDL_DT_FIXED_STRUCT:
        case IDL_DT_ENC_UNION:
        case IDL_DT_TRANSMIT_AS:
        case IDL_DT_REPRESENT_AS:
#ifndef _KERNEL
        case IDL_DT_CS_TYPE:
#endif
            defn_vec_ptr += 2;      /* After properties byte */
            IDL_GET_LONG_FROM_VECTOR(complex_defn_index, defn_vec_ptr);
            complex_defn_ptr = IDL_msp->IDL_type_vec + complex_defn_index;
            IDL_GET_LONG_FROM_VECTOR(offset_index, complex_defn_ptr);
            return(*(IDL_msp->IDL_offset_vec + offset_index));
#ifndef _KERNEL
        case IDL_DT_N_E_UNION:
            defn_vec_ptr += 2;      /* After properties byte */
            IDL_DISCARD_LONG_FROM_VECTOR(defn_vec_ptr);  /* Switch index */
            IDL_GET_LONG_FROM_VECTOR(complex_defn_index, defn_vec_ptr);
            complex_defn_ptr = IDL_msp->IDL_type_vec + complex_defn_index;
            IDL_GET_LONG_FROM_VECTOR(offset_index, complex_defn_ptr);
            return(*(IDL_msp->IDL_offset_vec + offset_index));
#endif
        case IDL_DT_STRING:
#ifndef _KERNEL
        case IDL_DT_V1_STRING:
#endif
            defn_vec_ptr++;
            /* Next byte is DT_VARYING_ARRAY or DT_OPEN_ARRAY */
            type = *defn_vec_ptr;
            /* If it is not DT_OPEN_ARRAY drop through to process it */
            if (type == IDL_DT_OPEN_ARRAY)
                return 0;
        case IDL_DT_FIXED_ARRAY:
        case IDL_DT_VARYING_ARRAY:
            defn_vec_ptr += 2;      /* After properties byte */
            IDL_DISCARD_LONG_FROM_VECTOR(defn_vec_ptr);
            /* Ignore the full array definition and look at the flattened one */
            IDL_GET_LONG_FROM_VECTOR(array_defn_index, defn_vec_ptr);
            array_defn_ptr = IDL_msp->IDL_type_vec + array_defn_index;
            dimensionality = (idl_ulong_int)*array_defn_ptr;
            array_defn_ptr++;
            array_element_count = 1;
            for (i=0; i<dimensionality; i++)
            {
                IDL_GET_LONG_FROM_VECTOR(lower_bound, array_defn_ptr);
                IDL_GET_LONG_FROM_VECTOR(upper_bound, array_defn_ptr);
                array_element_count *= (upper_bound - lower_bound + 1);
            }
            if (type == IDL_DT_VARYING_ARRAY)
                array_defn_ptr += dimensionality * IDL_DATA_LIMIT_PAIR_WIDTH;
            return(array_element_count 
                    * rpc_ss_type_size(array_defn_ptr, IDL_msp));
        case IDL_DT_FULL_PTR:
#ifndef _KERNEL
        case IDL_DT_UNIQUE_PTR:
#endif
        case IDL_DT_REF_PTR:
            return(sizeof(rpc_void_p_t));
#ifndef _KERNEL
        case IDL_DT_CS_ARRAY:
            /* The size of the array this modifier is attached to */
            return(rpc_ss_type_size(defn_vec_ptr + 1, IDL_msp));
#endif
        default:
            return 0;
    }
}

/******************************************************************************/
/*                                                                            */
/*  Clean up after NDR marshalling                                            */
/*                                                                            */
/******************************************************************************/
void rpc_ss_ndr_clean_up
#ifdef IDL_PROTOTYPES
(
    IDL_ms_t *IDL_msp
)
#else
( IDL_msp )
    IDL_ms_t *IDL_msp;
#endif
{
    idl_ulong_int i;

    /* Clean up marshalling state */
    if (IDL_msp->IDL_buff_addr != NULL
        && IDL_msp->IDL_stack_packet_status != IDL_stack_packet_in_use_k)
    {
        free(IDL_msp->IDL_buff_addr);
        IDL_msp->IDL_buff_addr = NULL;
    }
    for (i=0; i<IDL_msp->IDL_elts_in_use; i++)
    {
        if (IDL_msp->IDL_iovec.elt[i].buff_dealloc != NULL)
        {
            (*IDL_msp->IDL_iovec.elt[i].buff_dealloc)
                                (IDL_msp->IDL_iovec.elt[i].buff_addr);
            IDL_msp->IDL_iovec.elt[i].buff_dealloc = NULL;
        }
    }
    
    /* Clean up unmarshalling state */
    if (IDL_msp->IDL_elt_p != NULL)
    {
        if ((IDL_msp->IDL_elt_p->buff_dealloc != NULL)
                && (IDL_msp->IDL_elt_p->data_len != 0))
            (*IDL_msp->IDL_elt_p->buff_dealloc)(IDL_msp->IDL_elt_p->buff_addr);
        IDL_msp->IDL_elt_p = NULL;
    }

    /* Clean up marshalling state if it's an auto-handle restart */
    if ( (IDL_msp->IDL_restartable == idl_true) && (IDL_msp->IDL_node_table) )
    {
	if (IDL_msp->IDL_mem_handle.memory)
	{
 	   rpc_ss_mem_free(&IDL_msp->IDL_mem_handle);
	}
	rpc_ss_init_node_table(&IDL_msp->IDL_node_table,&IDL_msp->IDL_mem_handle);
    }

}

/******************************************************************************/
/*                                                                            */
/*  Create a set of Z values from a list of bounds values                     */
/*                                                                            */
/******************************************************************************/
void rpc_ss_Z_values_from_bounds
#ifdef IDL_PROTOTYPES
(
    /* [in] */  IDL_bound_pair_t *bounds_list,
    /* [in] */  idl_ulong_int dimensionality,
    /* [out] */ idl_ulong_int **p_Z_values,
    IDL_msp_t IDL_msp
)
#else
( bounds_list, dimensionality, p_Z_values, IDL_msp )
    IDL_bound_pair_t *bounds_list;
    idl_ulong_int dimensionality;
    idl_ulong_int **p_Z_values;
    IDL_msp_t IDL_msp;
#endif
{
    idl_ulong_int *Z_values;
    idl_ulong_int i;

    if (*p_Z_values == NULL)
    {
        Z_values = (idl_ulong_int *)rpc_ss_mem_alloc
             (&IDL_msp->IDL_mem_handle, dimensionality * sizeof(idl_ulong_int));
        *p_Z_values = Z_values;
    }
    else
        Z_values = *p_Z_values;
    for (i=0; i<dimensionality; i++)
    {
        if (bounds_list[i].upper >= bounds_list[i].lower)
            Z_values[i] = bounds_list[i].upper - bounds_list[i].lower + 1;
        else
            Z_values[i] = 0;
    }
}

/******************************************************************************/
/*                                                                            */
/*  Determine whether the elements of a varying or conformant array to be     */
/*  un/marshalled are contiguous. If they are, also return the total number   */
/*  of elements, and their starting address                                   */
/*                                                                            */
/******************************************************************************/
void rpc_ss_ndr_contiguous_elt
#ifdef IDL_PROTOTYPES
(
    /* [in] */  idl_ulong_int dimensionality,
    /* [in] */  idl_ulong_int *Z_values,
    /* [in] */  IDL_bound_pair_t *range_list,
    /* [in] */  idl_ulong_int element_size,
    /* [out] */ idl_boolean *p_contiguous,
    /* [out] */ idl_ulong_int *p_element_count,
    /* [out] */ rpc_void_p_t *p_array_addr
)
#else
(dimensionality, Z_values, range_list, element_size, p_contiguous,
 p_element_count, p_array_addr)
    idl_ulong_int dimensionality;
    idl_ulong_int *Z_values;
    IDL_bound_pair_t *range_list;
    idl_ulong_int element_size;
    idl_boolean *p_contiguous;
    idl_ulong_int *p_element_count;
    rpc_void_p_t *p_array_addr;
#endif
{
    idl_ulong_int i;
    idl_ulong_int element_count;

    element_count = 1;

    for (i=1; i<dimensionality; i++)
    {
        if ((idl_ulong_int)(range_list[i].upper - range_list[i].lower) 
            != Z_values[i] )
        {
            *p_contiguous = idl_false;
            return;
        }
        element_count *= Z_values[i];
    }

    *p_contiguous = idl_true;
    *p_array_addr = (rpc_void_p_t)((idl_byte *)*p_array_addr
                         + range_list[0].lower * element_count * element_size);
    *p_element_count = element_count
                             * (range_list[0].upper - range_list[0].lower);
}

/******************************************************************************/
/*                                                                            */
/*  Alignment operations and optimizing flag for array un/marshalling         */
/*                                                                            */
/******************************************************************************/
void rpc_ss_ndr_arr_align_and_opt
#ifdef IDL_PROTOTYPES
(
    /* [in] */  IDL_mar_or_unmar_k_t marsh_or_unmar,
    /* [in] */  idl_ulong_int dimensionality,
    /* [out] */ idl_byte *p_base_type,
    /* [in] */ idl_byte *defn_vec_ptr,   /* Points to array base info */
    /* [out] */ idl_boolean *p_optimize,
    IDL_msp_t IDL_msp
)
#else
( marsh_or_unmar, dimensionality, p_base_type, defn_vec_ptr, p_optimize,
  IDL_msp )
    IDL_mar_or_unmar_k_t marsh_or_unmar;
    idl_ulong_int dimensionality;
    idl_byte *p_base_type;
    idl_byte *defn_vec_ptr;
    idl_boolean *p_optimize;
    IDL_msp_t IDL_msp;
#endif
{
    idl_byte base_type;
    idl_ulong_int defn_index;
    idl_ulong_int pipe_arr_dims;
                /* Number of dims of array which is base type of a pipe */
    idl_ulong_int struct_defn_index;
    idl_byte *struct_defn_ptr;

    *p_optimize = idl_false;
    *p_base_type = *defn_vec_ptr;

    base_type = *p_base_type;
    if (base_type == IDL_DT_FIXED_ARRAY)
    {
        /* Base type of a pipe is an array. Find its base type */
        IDL_DISCARD_LONG_FROM_VECTOR( defn_vec_ptr );    /* Full array defn */
        IDL_GET_LONG_FROM_VECTOR( defn_index, defn_vec_ptr );
        defn_vec_ptr = IDL_msp->IDL_type_vec + defn_index;
        pipe_arr_dims = (idl_ulong_int)*defn_vec_ptr;
        defn_vec_ptr++;     /* By design this achieves 4-byte alignment */
        defn_vec_ptr += pipe_arr_dims * IDL_FIXED_BOUND_PAIR_WIDTH;
        base_type = *defn_vec_ptr;
    }


    switch( base_type )
    {
#ifdef PACKED_BYTE_ARRAYS
        case IDL_DT_BYTE:
            *p_optimize = idl_true;
            break;
#endif
#ifdef PACKED_CHAR_ARRAYS
        case IDL_DT_CHAR:
            if (marsh_or_unmar == IDL_marshalling_k)
                *p_optimize = idl_true;
            else
                *p_optimize =
                     (IDL_msp->IDL_drep.char_rep == ndr_g_local_drep.char_rep);
            break;
#endif
#ifdef PACKED_SCALAR_ARRAYS
        case IDL_DT_BOOLEAN:
            *p_optimize = idl_true;
            break;
        case IDL_DT_DOUBLE:
            if (marsh_or_unmar == IDL_marshalling_k)
            {
                IDL_MARSH_ALIGN_MP(IDL_msp, 8);
                *p_optimize = idl_true;
            }
            else
            {
                IDL_UNMAR_ALIGN_MP(IDL_msp, 8);
                *p_optimize =
                    ((IDL_msp->IDL_drep.float_rep == ndr_g_local_drep.float_rep)
                    && (IDL_msp->IDL_drep.int_rep == ndr_g_local_drep.int_rep));
            }
            break;
        case IDL_DT_FLOAT:
            if (marsh_or_unmar == IDL_marshalling_k)
            {
                IDL_MARSH_ALIGN_MP(IDL_msp, 4);
                *p_optimize = idl_true;
            }
            else
            {
                IDL_UNMAR_ALIGN_MP(IDL_msp, 4);
                *p_optimize =
                    ((IDL_msp->IDL_drep.float_rep == ndr_g_local_drep.float_rep)
                    && (IDL_msp->IDL_drep.int_rep == ndr_g_local_drep.int_rep));
            }
            break;
        case IDL_DT_SMALL:
        case IDL_DT_USMALL:
            if (marsh_or_unmar == IDL_marshalling_k)
                *p_optimize = idl_true;
            else
                *p_optimize =
                     (IDL_msp->IDL_drep.int_rep == ndr_g_local_drep.int_rep);
            break;
        case IDL_DT_USHORT:
            /* If language is C, drop through */
        case IDL_DT_SHORT:
            if (marsh_or_unmar == IDL_marshalling_k)
            {
                IDL_MARSH_ALIGN_MP(IDL_msp, 2);
                *p_optimize = idl_true;
            }
            else
            {
                IDL_UNMAR_ALIGN_MP(IDL_msp, 2);
                *p_optimize =
                    (IDL_msp->IDL_drep.int_rep == ndr_g_local_drep.int_rep);
            }
            break;
        case IDL_DT_ERROR_STATUS:
            /* If ifdef not active, drop through to next case */
#ifdef IDL_ENABLE_STATUS_MAPPING
            if (marsh_or_unmar == IDL_marshalling_k)
            {
                IDL_MARSH_ALIGN_MP(IDL_msp, 4);
            }
            else
            {
                IDL_UNMAR_ALIGN_MP(IDL_msp, 4);
            }
            break;
#endif
        case IDL_DT_LONG:
        case IDL_DT_ULONG:
            if (marsh_or_unmar == IDL_marshalling_k)
            {
                IDL_MARSH_ALIGN_MP(IDL_msp, 4);
                *p_optimize = idl_true;
            }
            else
            {
                IDL_UNMAR_ALIGN_MP(IDL_msp, 4);
                *p_optimize =
                    (IDL_msp->IDL_drep.int_rep == ndr_g_local_drep.int_rep);
            }
            break;
#ifndef _KERNEL
        case IDL_DT_HYPER:
        case IDL_DT_UHYPER:
            if (marsh_or_unmar == IDL_marshalling_k)
            {
                IDL_MARSH_ALIGN_MP(IDL_msp, 8);
                *p_optimize = idl_true;
            }
            else
            {
                IDL_UNMAR_ALIGN_MP(IDL_msp, 8);
                *p_optimize =
                    (IDL_msp->IDL_drep.int_rep == ndr_g_local_drep.int_rep);
            }
            break;
#endif
        case IDL_DT_ENUM:
            if (marsh_or_unmar == IDL_marshalling_k)
            {
                IDL_MARSH_ALIGN_MP(IDL_msp, 2);
                *p_optimize = (sizeof(int) == sizeof(idl_short_int));
            }
            else
            {
                IDL_UNMAR_ALIGN_MP(IDL_msp, 2);
                *p_optimize = (sizeof(int) == sizeof(idl_short_int)) &&
                    (IDL_msp->IDL_drep.int_rep == ndr_g_local_drep.int_rep);
            }
            break;
#ifndef _KERNEL
        case IDL_DT_V1_ENUM:
            if (marsh_or_unmar == IDL_marshalling_k)
            {
                IDL_MARSH_ALIGN_MP(IDL_msp, 4);
                *p_optimize = (sizeof(int) == sizeof(idl_long_int));
            }
            else
            {
                IDL_UNMAR_ALIGN_MP(IDL_msp, 4);
                *p_optimize = (sizeof(int) == sizeof(idl_long_int)) &&
                    (IDL_msp->IDL_drep.int_rep == ndr_g_local_drep.int_rep);
            }
            break;
#endif
#endif
        case IDL_DT_FIXED_STRUCT:
            defn_vec_ptr++;     /* Point at properties byte */
            /* Could the structure be relied to be correctly laid out
                in memory on some architecture? */
            if ( ! IDL_PROP_TEST(*defn_vec_ptr, IDL_PROP_MAYBE_WIRE_ALIGNED))
                break;
            if (marsh_or_unmar == IDL_unmarshalling_k)
            {
                /* Can only unmarshall by copying if the drep's for all the
                    fields match */
                if ( IDL_PROP_TEST(*defn_vec_ptr, IDL_PROP_DEP_CHAR)
                        && (IDL_msp->IDL_drep.char_rep
                                                 != ndr_g_local_drep.char_rep) )
                    break;
                if ( IDL_PROP_TEST(*defn_vec_ptr, IDL_PROP_DEP_INT)
                        && (IDL_msp->IDL_drep.int_rep
                                                 != ndr_g_local_drep.int_rep) )
                    break;
                /* For floating point numbers we need the same encoding
                        and the same endianness */
                if ( IDL_PROP_TEST(*defn_vec_ptr, IDL_PROP_DEP_FLOAT)
                        && ((IDL_msp->IDL_drep.float_rep
                                                 != ndr_g_local_drep.float_rep)
                            || (IDL_msp->IDL_drep.int_rep
                                                != ndr_g_local_drep.int_rep)) )
                    break;
            }
            defn_vec_ptr++;     /* Point after properties byte */
            IDL_GET_LONG_FROM_VECTOR(struct_defn_index, defn_vec_ptr);
            struct_defn_ptr = IDL_msp->IDL_type_vec + struct_defn_index;
            IDL_DISCARD_LONG_FROM_VECTOR(struct_defn_ptr);
                                          /* Discard index into offset vector */
            /* Because we can't get here for a [v1_struct], the next byte
                    is an alignment byte unless all the fields are 1-byte */
            switch (*struct_defn_ptr)
            {
                case IDL_DT_NDR_ALIGN_8:
#ifdef IDL_NATURAL_ALIGN_8
                    if (marsh_or_unmar == IDL_marshalling_k)
                    {
                        IDL_MARSH_ALIGN_MP(IDL_msp, 8);
                    }
                    else
                    {
                        IDL_UNMAR_ALIGN_MP(IDL_msp, 8);
                    }
                    *p_optimize = idl_true;
#endif
                    break;
                case IDL_DT_NDR_ALIGN_4:
#if defined(IDL_NATURAL_ALIGN_8) || defined(IDL_NATURAL_ALIGN_4)
                    if (marsh_or_unmar == IDL_marshalling_k)
                    {
                        IDL_MARSH_ALIGN_MP(IDL_msp, 4);
                    }
                    else
                    {
                        IDL_UNMAR_ALIGN_MP(IDL_msp, 4);
                    }
                    *p_optimize = idl_true;
#endif
                    break;
                case IDL_DT_NDR_ALIGN_2:
#if defined(IDL_NATURAL_ALIGN_8) || defined(IDL_NATURAL_ALIGN_4)
                    if (marsh_or_unmar == IDL_marshalling_k)
                    {
                        IDL_MARSH_ALIGN_MP(IDL_msp, 2);
                    }
                    else
                    {
                        IDL_UNMAR_ALIGN_MP(IDL_msp, 2);
                    }
                    *p_optimize = idl_true;
#endif
                    break;
                default:
#if defined(IDL_NATURAL_ALIGN_8) || defined(IDL_NATURAL_ALIGN_4) || defined(IDL_NATURAL_ALIGN_1)
                    *p_optimize = idl_true;
#endif
                    break;
            }
            break;
        default:
            break;
    }


}

/******************************************************************************/
/*                                                                            */
/*  Function whose result is the integer that is of the given type at the     */
/*   given address. For use in unions, boolean's, char's and enum's are       */
/*   treated as integers.                                                     */
/*                                                                            */
/******************************************************************************/
idl_long_int rpc_ss_get_typed_integer
#ifdef IDL_PROTOTYPES
(
    /* [in] */  idl_byte type,
    /* [in] */  rpc_void_p_t address,
    IDL_msp_t IDL_msp
)
#else
(type, address, IDL_msp)
    idl_byte type;
    rpc_void_p_t address;
    IDL_msp_t IDL_msp;
#endif
{
    switch (type)
    {
        case IDL_DT_BOOLEAN:
            return( (idl_long_int)(*(idl_boolean *)address) );
        case IDL_DT_CHAR:
            return( (idl_long_int)(*(idl_char *)address) );
        case IDL_DT_ENUM:
#ifndef _KERNEL
        case IDL_DT_V1_ENUM:
#endif
            return( (idl_long_int)(*(int *)address) );
        case IDL_DT_SMALL:
                return( (idl_long_int)(*(idl_small_int *)address) );
        case IDL_DT_SHORT:
            return( (idl_long_int)(*(idl_short_int *)address) );
        case IDL_DT_LONG:
            return( (idl_long_int)(*(idl_long_int *)address) );
        case IDL_DT_USMALL:
                return( (idl_long_int)(*(idl_usmall_int *)address) );
        case IDL_DT_USHORT:
                return( (idl_long_int)(*(idl_ushort_int *)address) );
        case IDL_DT_ERROR_STATUS:
        case IDL_DT_ULONG:
            /* !!! Overflow here if value > 2**31 - 1 !!! */
            return( (idl_long_int)(*(idl_ulong_int *)address) );
        default:
            RAISE( rpc_x_coding_error );
    }
#ifdef SGIMIPS
    return 0; /* hush up the ficus 6.4 mongoose compiler */
#endif /* SGIMIPS */
}

/******************************************************************************/
/*                                                                            */
/*  Build a bounds list for a conformant or open array                        */
/*                                                                            */
/******************************************************************************/
void rpc_ss_build_bounds_list
#ifdef IDL_PROTOTYPES
(
    /* [in,out] */ idl_byte **p_defn_vec_ptr,
                        /* On entry defn_vec_ptr points to bounds info */
                        /* On exit it points at 
                                    conformant array - base type info
                                    open array - data limit info */
    /* [in] */  rpc_void_p_t array_addr,    /* NULL when building bounds list
                                                for a conformant structure */
    /* [in] */  rpc_void_p_t struct_addr,   /* Address of structure that array 
                                   is field of. NULL if array is a parameter */
    /* [in] */  idl_ulong_int *struct_offset_vec_ptr,
                                           /*  NULL if array is a parameter */
    /* [in] */  idl_ulong_int dimensionality,
    /* [out] */ IDL_bound_pair_t **p_bounds_list,
    IDL_msp_t IDL_msp
)
#else
(p_defn_vec_ptr, array_addr, struct_addr, struct_offset_vec_ptr, dimensionality,
  p_bounds_list, IDL_msp)
    idl_byte **p_defn_vec_ptr;
    rpc_void_p_t array_addr;
    rpc_void_p_t struct_addr;
    idl_ulong_int *struct_offset_vec_ptr;
    idl_ulong_int dimensionality;
    IDL_bound_pair_t **p_bounds_list;
    IDL_msp_t IDL_msp;
#endif
{
    IDL_bound_pair_t *bounds_list;
    idl_byte *defn_vec_ptr = *p_defn_vec_ptr;
    idl_byte bound_kind, bound_type;
    idl_ulong_int i;
    idl_long_int size;
    idl_ulong_int element_size;     /* Size of base type of string */
    idl_ulong_int attribute_index;
    rpc_void_p_t bound_addr;
    idl_ulong_int string_field_offset;  /* Offset index for string field */

    if (*p_bounds_list == NULL)
    {
        bounds_list = (IDL_bound_pair_t *)rpc_ss_mem_alloc
          (&IDL_msp->IDL_mem_handle, dimensionality * sizeof(IDL_bound_pair_t));
        *p_bounds_list = bounds_list;
    }
    else
        bounds_list = *p_bounds_list;

    for (i=0; i<dimensionality; i++)
    {
        /* Get lower bound */
        bound_kind = *defn_vec_ptr;
        defn_vec_ptr++;
        if (bound_kind == IDL_BOUND_FIXED)
        {
            IDL_GET_LONG_FROM_VECTOR(bounds_list[i].lower, defn_vec_ptr);
        }
        else
        {
            /* Lower bound is [min_is] */
            bound_type = *defn_vec_ptr;
            defn_vec_ptr++;
            IDL_GET_LONG_FROM_VECTOR(attribute_index, defn_vec_ptr);
            if (struct_addr == NULL)
                bound_addr = IDL_msp->IDL_param_vec[attribute_index];
            else
            {
                bound_addr = (rpc_void_p_t)
                        ((idl_byte *)struct_addr
                                 + struct_offset_vec_ptr[attribute_index]);
             }
            bounds_list[i].lower =
                         rpc_ss_get_typed_integer( bound_type, bound_addr,
                                                     IDL_msp );
        }

        /* Get upper bound */
        bound_kind = *defn_vec_ptr;
        defn_vec_ptr++;
        if (bound_kind == IDL_BOUND_FIXED)
        {
            IDL_GET_LONG_FROM_VECTOR(bounds_list[i].upper, defn_vec_ptr);
        }
        else if (bound_kind == IDL_BOUND_STRING)
        {
            element_size = (idl_ulong_int)*defn_vec_ptr;
            defn_vec_ptr++;
            IDL_GET_LONG_FROM_VECTOR(string_field_offset, defn_vec_ptr);
                        /* Value is only used in conformant struct size case */
            if (array_addr == NULL)
            {
                /* Conformant field of conformant structure
                    has size determined by [string] */
                array_addr = (rpc_void_p_t)((idl_byte *)struct_addr
                                                + *(struct_offset_vec_ptr 
                                                        + string_field_offset));
            }
            if (element_size == 1)
                bounds_list[i].upper = bounds_list[i].lower
                                                         + strlen(array_addr);
            else
            {
                size = rpc_ss_strsiz( (idl_char *)array_addr, element_size );
                bounds_list[i].upper = bounds_list[i].lower + size - 1;
            }
        }
        else
        {
            /* Upper bound is [max_is] or [size_is] */
            bound_type = *defn_vec_ptr;
            defn_vec_ptr++;
            IDL_GET_LONG_FROM_VECTOR(attribute_index, defn_vec_ptr);
            if (struct_addr == NULL)
                bound_addr = IDL_msp->IDL_param_vec[attribute_index];
            else
            {
                bound_addr = (rpc_void_p_t)
                        ((idl_byte *)struct_addr
                                 + struct_offset_vec_ptr[attribute_index]);
            }
            if (bound_kind == IDL_BOUND_MAX_IS)
                bounds_list[i].upper =
                         rpc_ss_get_typed_integer( bound_type, bound_addr,
                                                    IDL_msp );
            else
            {
                size = rpc_ss_get_typed_integer( bound_type, bound_addr,
                                                    IDL_msp );
                bounds_list[i].upper = bounds_list[i].lower + size - 1;
            }
        }
        /* Inside out bounds mean "no elements". Store value which will
            yield 0 in subsequent calculations */
        if (bounds_list[i].upper < bounds_list[i].lower)
            bounds_list[i].upper = bounds_list[i].lower - 1;
    }

    *p_defn_vec_ptr = defn_vec_ptr;
}



/******************************************************************************/
/*                                                                            */
/*  A range list is a set of [A, A+B] pairs for a varying array. Build one.   */
/*  Note that A is the difference between the lower bound and the index of    */
/*  the first element in the dimension to be shipped. B is the number of      */
/*  elements in that dimension to be shipped.                                 */
/*                                                                            */
/******************************************************************************/
void rpc_ss_build_range_list
#ifdef IDL_PROTOTYPES
(
    /* [in,out] */ idl_byte **p_defn_vec_ptr,
    /* [in] */  rpc_void_p_t array_addr,  /* Only used if [string] data limit */
    /* [in] */  rpc_void_p_t struct_addr,   /* Address of structure array is a
                                 field of. NULL if array is a parameter */
    /* [in] */  idl_ulong_int *struct_offset_vec_ptr,
                                           /*  NULL if array is a parameter */
    /* [in] */  idl_ulong_int dimensionality,
    /* [in] */  IDL_bound_pair_t *bounds_list,
    /* [out] */ IDL_bound_pair_t **p_range_list,
    /* [out] */ idl_boolean *p_add_null,
    IDL_msp_t IDL_msp
)
#else
(p_defn_vec_ptr, array_addr, struct_addr, struct_offset_vec_ptr, dimensionality,
  bounds_list, p_range_list, p_add_null, IDL_msp)
    idl_byte **p_defn_vec_ptr;
    rpc_void_p_t array_addr;
    rpc_void_p_t struct_addr;
    idl_ulong_int *struct_offset_vec_ptr;
    idl_ulong_int dimensionality;
    IDL_bound_pair_t *bounds_list;
    IDL_bound_pair_t **p_range_list;
    idl_boolean *p_add_null;
    IDL_msp_t IDL_msp;
#endif
{
    IDL_bound_pair_t *range_list;
    idl_byte *defn_vec_ptr = *p_defn_vec_ptr;
    idl_byte limit_kind, limit_type;
    idl_ulong_int i;
    idl_long_int data_limit;
    idl_ulong_int element_size;     /* Size of base type of string */
    idl_ulong_int attribute_index;
    rpc_void_p_t limit_addr;

    *p_add_null = idl_false;
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
        /* Get lower data limit */
        limit_kind = *defn_vec_ptr;
        defn_vec_ptr++;
        if (limit_kind == IDL_LIMIT_FIXED)
        {
            IDL_GET_LONG_FROM_VECTOR(data_limit, defn_vec_ptr);
        }
        else
        {
            /* Lower data limit is [first_is] */
            limit_type = *defn_vec_ptr;
            defn_vec_ptr++;
            IDL_GET_LONG_FROM_VECTOR(attribute_index, defn_vec_ptr);
            if (struct_addr == NULL)
                limit_addr = IDL_msp->IDL_param_vec[attribute_index];
            else
            {
                limit_addr = (rpc_void_p_t)
                        ((idl_byte *)struct_addr
                                 + struct_offset_vec_ptr[attribute_index]);
             }
            data_limit = rpc_ss_get_typed_integer( limit_type, limit_addr,
                                                    IDL_msp );
        }
        range_list[i].lower = data_limit - bounds_list[i].lower;
        if (range_list[i].lower < 0)
            RAISE( rpc_x_invalid_bound );

        /* Get upper data limit */
        limit_kind = *defn_vec_ptr;
        defn_vec_ptr++;
        if (limit_kind == IDL_LIMIT_FIXED)
        {
            IDL_GET_LONG_FROM_VECTOR(data_limit, defn_vec_ptr);
            range_list[i].upper = data_limit + 1 - bounds_list[i].lower;
        }
        else if (limit_kind == IDL_LIMIT_STRING)
        {
            element_size = (idl_ulong_int)*defn_vec_ptr;
            /* Element size byte is discarded as we align to discard dummy
                longword */
            IDL_DISCARD_LONG_FROM_VECTOR(defn_vec_ptr);
            {
                if (element_size == 1)
                    data_limit = strlen(array_addr) + 1;
                else
                    data_limit = rpc_ss_strsiz( (idl_char *)array_addr,
                                                                 element_size );
            }
            range_list[i].upper = range_list[i].lower + data_limit;
            if ( ! (*p_add_null) )
            {
                if ( range_list[i].upper > (bounds_list[i].upper
                                        - bounds_list[i].lower + 1) )
                    RAISE( rpc_x_invalid_bound );
            }
        }
        else if (limit_kind == IDL_LIMIT_UPPER_CONF)
        {
            /* Dummy type byte is discarded as we align to discard dummy
                longword */
            IDL_DISCARD_LONG_FROM_VECTOR( defn_vec_ptr );
            range_list[i].upper =
                                bounds_list[i].upper - bounds_list[i].lower + 1;
        }
        else
        {
            /* Upper data limit is [last_is] or [length_is] */
            limit_type = *defn_vec_ptr;
            defn_vec_ptr++;
            IDL_GET_LONG_FROM_VECTOR(attribute_index, defn_vec_ptr);
            if (struct_addr == NULL)
                limit_addr = IDL_msp->IDL_param_vec[attribute_index];
            else
            {
                limit_addr = (rpc_void_p_t)
                        ((idl_byte *)struct_addr
                                 + struct_offset_vec_ptr[attribute_index]);
            }
            data_limit = rpc_ss_get_typed_integer( limit_type, limit_addr,
                                                    IDL_msp );
            if (limit_kind == IDL_LIMIT_LENGTH_IS)
                range_list[i].upper = range_list[i].lower + data_limit;
            else
                range_list[i].upper = data_limit - bounds_list[i].lower + 1;
            if ( range_list[i].upper > (bounds_list[i].upper
                                        - bounds_list[i].lower + 1) )
                RAISE( rpc_x_invalid_bound );
        }
        /* Inside out limits mean "transmit no elements" */
        if (range_list[i].upper < range_list[i].lower)
            range_list[i].upper = range_list[i].lower;
    }

    *p_defn_vec_ptr = defn_vec_ptr;
}

/******************************************************************************/
/*                                                                            */
/*  Get the size of string storage for the base type of an array of string    */
/*  and the index value of the start of the string's array description        */
/*                                                                            */
/******************************************************************************/
void rpc_ss_get_string_base_desc
#ifdef IDL_PROTOTYPES
(
    /* [in] */  idl_byte *defn_vec_ptr, /* Pointer to DT_STRING in base type
                                        specification in definition vector */
    /* [out] */ idl_ulong_int *p_array_size,
    /* [out] */ idl_ulong_int *p_array_defn_index,
    IDL_msp_t IDL_msp
)
#else
( defn_vec_ptr, p_array_size, p_array_defn_index, IDL_msp )
    idl_byte *defn_vec_ptr;
    idl_ulong_int *p_array_size;
    idl_ulong_int *p_array_defn_index;
    IDL_msp_t IDL_msp;
#endif
{
    idl_byte *array_defn_ptr;
    idl_long_int string_lower_bound, string_upper_bound;
    idl_ulong_int array_element_count;

    defn_vec_ptr += 3;  /* DT_STRING, DT_VARYING_ARRAY, properties */
    IDL_DISCARD_LONG_FROM_VECTOR(defn_vec_ptr);
    /* Ignore the full array definition and look at the flattened one */
    IDL_GET_LONG_FROM_VECTOR(*p_array_defn_index, defn_vec_ptr);
    array_defn_ptr = IDL_msp->IDL_type_vec + *p_array_defn_index;
    array_defn_ptr++;   /* Discard dimensionality */
    IDL_GET_LONG_FROM_VECTOR(string_lower_bound, array_defn_ptr);
    IDL_GET_LONG_FROM_VECTOR(string_upper_bound, array_defn_ptr);
    array_element_count = string_upper_bound - string_lower_bound + 1;
    *p_array_size =
            array_element_count * IDL_DATA_LIMIT_PAIR_CHAR_SIZE(array_defn_ptr);
}

/******************************************************************************/
/*                                                                            */
/*  Point at the union arm definition for the supplied switch value           */
/*  Returns FALSE if there is no match, TRUE otherwise                        */
/*                                                                            */
/******************************************************************************/
idl_boolean rpc_ss_find_union_arm_defn
#ifdef IDL_PROTOTYPES
(
    /* [in] */ idl_byte *defn_vec_ptr,    /* Points at first union arm defn */
    /* [in] */ idl_ulong_int arm_count,   /* Number of non-default arms */
    /* [in] */ idl_ulong_int switch_value,  /* to be matched */
    /* [out] */ idl_byte **p_arm_type_ptr,
                    /* Pointer to type part of arm with matching switch value */
    /* [in] */ IDL_msp_t IDL_msp 
                    /* marshall state ptr needed for IDL_ARM macro */
)
#else
( defn_vec_ptr, arm_count, switch_value, p_arm_type_ptr )
    idl_byte *defn_vec_ptr;
    idl_ulong_int arm_count;
    idl_ulong_int switch_value;
    idl_byte **p_arm_type_ptr;
    IDL_msp_t IDL_msp;
#endif
{
    idl_long_int low, mid, high;         /* Element indices */
    idl_ulong_int arm_switch_value;       /* To test supplied value against */

    /* If there are no elements to search, then return no match found. */

    if (arm_count == 0)
        return(idl_false);

    /* Initialize the search interval to include all elements. */

    low = 0;
    high = arm_count - 1;

    /*  Loop, computing the midpoint element and comparing it to the desired
         element until a match is found or all elements are exhausted.
         Note that the midpoint index by convention is always
         rounded down if there is a remainder after division.
     */
    do {
        mid = (low + high) / 2;         /* Compute midpoint element */
        arm_switch_value = IDL_ARM_SWITCH_VALUE(defn_vec_ptr, mid);
        if (switch_value > arm_switch_value)
        {
            /* If element is beyond the midpoint */
            low = mid + 1;              /*  adjust low index past midpoint */
        }
        else if (switch_value < arm_switch_value)
        {
            /* If element is prior to the midpoint */
            high = mid - 1;         /*  adjust high index below midpoint */
        }
        else
            break;                  /* Otherwise, element was found! */
    } while (high >= low);

    if (high >= low)
    {
        /* Match was found - point at arm that matched */
        *p_arm_type_ptr = defn_vec_ptr + mid * IDL_UNION_ARM_DESC_WIDTH;
        /* Advance pointer over switch value */
        IDL_DISCARD_LONG_FROM_VECTOR( *p_arm_type_ptr );
        return(idl_true);
    }
    else
        return(idl_false);
}

/******************************************************************************/
/*                                                                            */
/*  Get, from data in memory, the value of the discriminant of a              */
/*      non-encapsulated union                                                */
/*                                                                            */
/******************************************************************************/
void rpc_ss_get_switch_from_data
#ifdef IDL_PROTOTYPES
(
    /* [in] */ idl_ulong_int switch_index,
                /* If union is parameter, index in param list of discriminant */
                /* If union is field, index in offset list for discriminant */
    /* [in] */ idl_byte switch_type,
    /* [in] */ rpc_void_p_t struct_addr,     /* Address of structure union is
                                        field of. NULL if union is parameter */
    /* [in] */ idl_ulong_int *struct_offset_vec_ptr,
                                                /* NULL if union is parameter */
    /* [out] */ idl_ulong_int *p_switch_value,
    IDL_msp_t IDL_msp
)
#else
(switch_index, switch_type, struct_addr, struct_offset_vec_ptr, p_switch_value,
 IDL_msp)
    idl_ulong_int switch_index;
    idl_byte switch_type;
    rpc_void_p_t struct_addr;
    idl_ulong_int *struct_offset_vec_ptr;
    idl_ulong_int *p_switch_value;
    IDL_msp_t IDL_msp;
#endif
{
    rpc_void_p_t switch_addr;

    if ( struct_addr == NULL )
        switch_addr = IDL_msp->IDL_param_vec[switch_index];
    else
        switch_addr = (rpc_void_p_t)((idl_byte *)struct_addr
                                       + struct_offset_vec_ptr[switch_index]);

    *p_switch_value = rpc_ss_get_typed_integer( switch_type, switch_addr,
                                                    IDL_msp );
}

/******************************************************************************/
/*                                                                            */
/*  Return TRUE if a -bug flag in the range 1-31 is set                       */
/*                                                                            */
/******************************************************************************/
idl_boolean rpc_ss_bug_1_thru_31
#ifdef IDL_PROTOTYPES
(
    /* [in] */ idl_ulong_int bug_mask,
    IDL_msp_t IDL_msp
)
#else
( bug_mask, IDL_msp )
    idl_ulong_int bug_mask;
    IDL_msp_t IDL_msp;
#endif
{
    idl_byte *defn_vec_ptr;
    idl_ulong_int defn_index;
    idl_ulong_int flags;

    /* Get position of bug flags */
    defn_vec_ptr = IDL_msp->IDL_type_vec + 40;
    IDL_GET_LONG_FROM_VECTOR( defn_index, defn_vec_ptr );

    /* Get flags longword for bugs 1-31 */
    defn_vec_ptr = IDL_msp->IDL_type_vec + defn_index;
    IDL_GET_LONG_FROM_VECTOR( flags, defn_vec_ptr );

    return( (flags & bug_mask) != 0 );
}

#ifndef _KERNEL
/******************************************************************************/
/*                                                                            */
/*  If -bug 1 is set and we are marshalling a [v1_array] with no elements,    */
/*  alignment must be forced to the last scalar that would have been          */
/*  marshalled if we had marshalled an array element.                         */
/*  This function returns the alignment to be applied.                        */
/*                                                                            */
/******************************************************************************/
idl_ulong_int rpc_ss_ndr_bug_1_align
#ifdef IDL_PROTOTYPES
(
    /* [in] */ idl_byte *defn_vec_ptr,     /* Points at array base type unless
                        in recursive call. In recursive call, points at defn
                        of structure field, union arm, or transmitted type */
    IDL_msp_t IDL_msp
)
#else
(defn_vec_ptr, IDL_msp)
    idl_byte *defn_vec_ptr;
    IDL_msp_t IDL_msp;
#endif
{
    idl_ulong_int defn_index;
    idl_byte *struct_defn_ptr;
    idl_byte *last_field_defn_ptr;  /* Points to the definition of the last
                                        field of a structure */
    idl_byte type_byte;
    idl_ulong_int array_defn_index;
    idl_byte *array_defn_ptr;
    idl_ulong_int dimensionality;
    idl_byte *union_defn_ptr;
    idl_ulong_int arm_count;
    idl_ulong_int min_union_align;  /* Minimum alignment forced by any
                                                                union arm */
    idl_ulong_int arm_align;       /* Alignment for last field of current arm */
    idl_ulong_int i;

    type_byte = *defn_vec_ptr;
    switch (type_byte)
    {
        case IDL_DT_BYTE:
        case IDL_DT_CHAR:
        case IDL_DT_BOOLEAN:
        case IDL_DT_SMALL:
        case IDL_DT_USMALL:
            return 1;
        case IDL_DT_SHORT:
        case IDL_DT_USHORT:
            return 2;
        case IDL_DT_FLOAT:
        case IDL_DT_LONG:
        case IDL_DT_ULONG:
        case IDL_DT_V1_ENUM:
        case IDL_DT_ERROR_STATUS:
            return 4;
        case IDL_DT_DOUBLE:
#ifndef _KERNEL
        case IDL_DT_HYPER:
        case IDL_DT_UHYPER:
            return 8;
#endif
        case IDL_DT_FIXED_STRUCT:
            /* Structure analysis follows switch statement */
            break;
        case IDL_DT_FIXED_ARRAY:
        case IDL_DT_VARYING_ARRAY:
        case IDL_DT_OPEN_ARRAY:
            /* Alignment forced by array base type */
            defn_vec_ptr += 2;   /* DT_ARRAY and properties byte */
            IDL_DISCARD_LONG_FROM_VECTOR( defn_vec_ptr );
                                            /* Full array definition */
            IDL_GET_LONG_FROM_VECTOR( array_defn_index, defn_vec_ptr );
            array_defn_ptr = IDL_msp->IDL_type_vec + array_defn_index;
            dimensionality = (idl_ulong_int)*array_defn_ptr;
            if (type_byte == IDL_DT_FIXED_ARRAY)
                array_defn_ptr += dimensionality * sizeof(IDL_bound_pair_t);
            else if (type_byte == IDL_DT_VARYING_ARRAY)
            {
                array_defn_ptr += dimensionality * IDL_FIXED_BOUND_PAIR_WIDTH;
                array_defn_ptr += dimensionality * IDL_DATA_LIMIT_PAIR_WIDTH;
            }
            else /* type_byte == IDL_OPEN_VARYING_ARRAY */
            {
                IDL_ADV_DEFN_PTR_OVER_BOUNDS( array_defn_ptr, dimensionality );
                array_defn_ptr += dimensionality * IDL_DATA_LIMIT_PAIR_WIDTH;
            }
            return rpc_ss_ndr_bug_1_align(array_defn_ptr, IDL_msp);
        case IDL_DT_V1_ARRAY:
        case IDL_DT_V1_STRING:
            /* Discard modiier - do array alignment */
            defn_vec_ptr++;
            return rpc_ss_ndr_bug_1_align(defn_vec_ptr, IDL_msp);
        case IDL_DT_ENC_UNION:
            /* Weakest alignment forced by any union arm */
            defn_vec_ptr += 2;   /* DT_ENC_UNION and properties byte */
            IDL_GET_LONG_FROM_VECTOR( defn_index, defn_vec_ptr );
            union_defn_ptr = IDL_msp->IDL_type_vec + defn_index;
            IDL_GET_LONG_FROM_VECTOR( arm_count, union_defn_ptr );
            IDL_DISCARD_LONG_FROM_VECTOR( union_defn_ptr );
                                            /* Point at type of first arm */
            min_union_align = 8;    /* Maximum alignment required by NDR */
            for (i=0; i<arm_count+1; i++)
            {
                if (*union_defn_ptr != IDL_DT_VOID)
                {
                    arm_align = rpc_ss_ndr_bug_1_align(union_defn_ptr, IDL_msp);
                    if (arm_align < min_union_align)
                        min_union_align = arm_align;
                }
                union_defn_ptr += IDL_UNION_ARM_DESC_WIDTH;
            }
            return min_union_align;
        case IDL_DT_TRANSMIT_AS:
            /* Resolve down to non-[transmit_as] type */
            while (*defn_vec_ptr == IDL_DT_TRANSMIT_AS)
            {
                defn_vec_ptr += 2;   /* DT_TRANSMIT_AS and properties byte */
                IDL_GET_LONG_FROM_VECTOR( defn_index, defn_vec_ptr );
                defn_vec_ptr = IDL_msp->IDL_type_vec + defn_index;
            }
            return rpc_ss_ndr_bug_1_align(defn_vec_ptr, IDL_msp);
        default:
            /* Other flags cannot occur in a [v1_struct] */
#ifdef DEBUG_INTERP
            printf(
               "rpc_ss_ndr_bug_1_struct_align:unrecognized alignment type %d\n",
                        type_byte);
            exit(0);
#endif
            RAISE(rpc_x_coding_error);
    }

    /* Return alignment required for last field of structure */
    defn_vec_ptr += 2;     /* DT_FIXED_STRUCT and properties byte */
    IDL_GET_LONG_FROM_VECTOR(defn_index, defn_vec_ptr);
    struct_defn_ptr = IDL_msp->IDL_type_vec + defn_index;
 
    /* Find the last field of the structure */
    do {
        type_byte = *struct_defn_ptr;
        if (type_byte != IDL_DT_EOL)
            last_field_defn_ptr = struct_defn_ptr;
        struct_defn_ptr++;
        switch(type_byte)
        {
            case IDL_DT_BYTE:
            case IDL_DT_CHAR:
            case IDL_DT_BOOLEAN:
            case IDL_DT_DOUBLE:
            case IDL_DT_FLOAT:
            case IDL_DT_SMALL:
            case IDL_DT_SHORT:
            case IDL_DT_LONG:
#ifndef _KERNEL
            case IDL_DT_HYPER:
#endif
            case IDL_DT_USMALL:
            case IDL_DT_USHORT:
            case IDL_DT_ULONG:
#ifndef _KERNEL
            case IDL_DT_UHYPER:
            case IDL_DT_V1_ENUM:
#endif
            case IDL_DT_ERROR_STATUS:
                break;
            case IDL_DT_FIXED_ARRAY:
                struct_defn_ptr++;     /* Skip over properties byte */
                IDL_DISCARD_LONG_FROM_VECTOR(struct_defn_ptr);
                                                    /* Full array definition */
                IDL_DISCARD_LONG_FROM_VECTOR(struct_defn_ptr);
                                                /* Flattened array definition */
                break;
            case IDL_DT_VARYING_ARRAY:
                struct_defn_ptr++;     /* Skip over properties byte */
                IDL_DISCARD_LONG_FROM_VECTOR(struct_defn_ptr);
                                                    /* Full array definition */
                IDL_DISCARD_LONG_FROM_VECTOR(struct_defn_ptr);
                                                /* Flattened array definition */
                break;
            case IDL_DT_OPEN_ARRAY:
                struct_defn_ptr++;     /* Skip over properties byte */
                IDL_DISCARD_LONG_FROM_VECTOR(struct_defn_ptr);
                                                    /* Full array definition */
                IDL_DISCARD_LONG_FROM_VECTOR(struct_defn_ptr);
                                                /* Flattened array definition */
                break;
            case IDL_DT_ENC_UNION:
                struct_defn_ptr++;     /* Skip over properties byte */
                IDL_DISCARD_LONG_FROM_VECTOR(struct_defn_ptr);
                                                /* Union definition */
                break;
            case IDL_DT_TRANSMIT_AS:
            case IDL_DT_REPRESENT_AS:
                struct_defn_ptr++;     /* Skip over properties byte */
                IDL_DISCARD_LONG_FROM_VECTOR(struct_defn_ptr);
                                                /* [transmit_as] definition */
                break;
            case IDL_DT_V1_ARRAY:
                break;
            case IDL_DT_V1_STRING:
                struct_defn_ptr += 2;  /* DT_VARYING_ARRAY and properties */
                IDL_DISCARD_LONG_FROM_VECTOR(struct_defn_ptr);
                                                        /* Full array defn */
                IDL_DISCARD_LONG_FROM_VECTOR(struct_defn_ptr);
                                                      /* Flattened array defn */
                break;
            case IDL_DT_EOL:
                break;
            default:
                /* Other flags cannot occur in a [v1_struct] */
#ifdef DEBUG_INTERP
                printf(
                   "rpc_ss_ndr_bug_1_struct_align:unrecognized field type %d\n",
                        type_byte);
                exit(0);
#endif
                RAISE(rpc_x_coding_error);
        }
    } while (type_byte != IDL_DT_EOL);

    /* Do alignment for last field */
    return rpc_ss_ndr_bug_1_align(last_field_defn_ptr, IDL_msp);
}
#endif

/******************************************************************************/
/*                                                                            */
/*  Check type vector consistent with interpreter version                     */
/*                                                                            */
/******************************************************************************/
void rpc_ss_type_vec_vers_check
#ifdef IDL_PROTOTYPES
(
    IDL_msp_t IDL_msp
)
#else
( IDL_msp )
    IDL_msp_t IDL_msp;
#endif
{
    idl_short_int interp_major_version, interp_minor_version;

    interp_major_version = IDL_VERSION_NUMBER(IDL_INTERP_ENCODE_MAJOR);
    interp_minor_version = IDL_VERSION_NUMBER(IDL_INTERP_ENCODE_MINOR);
    if ((interp_major_version != 3) || (interp_minor_version > 2))
    {
#ifdef DEBUG_INTERP
        printf("Expecting data structure version 3.0, 3.1 or 3.2 - found %d.%d\n",
                interp_major_version, interp_minor_version);
        exit(0);
#endif
        RAISE( rpc_x_unknown_stub_rtl_if_vers );
    }
}

/******************************************************************************/
/*                                                                            */
/*  Initialize the marshalling state block                                    */
/*                                                                            */
/******************************************************************************/
void rpc_ss_init_marsh_state
#ifdef IDL_PROTOTYPES
(
    idl_byte IDL_type_vec[],
    IDL_msp_t IDL_msp
)
#else
( IDL_type_vec, IDL_msp )
    idl_byte IDL_type_vec[];
    IDL_msp_t IDL_msp;
#endif
{
    IDL_msp->IDL_mem_handle.memory = (rpc_void_p_t)NULL;
    IDL_msp->IDL_mem_handle.node_table = (rpc_ss_node_table_t)NULL;
    IDL_msp->IDL_status = error_status_ok;
    IDL_msp->IDL_elts_in_use = 0;
    IDL_msp->IDL_buff_addr = NULL;
    IDL_msp->IDL_left_in_buff = 0;
    IDL_msp->IDL_mp_start_offset = 0;
    IDL_msp->IDL_type_vec = IDL_type_vec;
    IDL_msp->IDL_pickling_handle = NULL;
    /* Protect against old stubs that haven't allocated a stack packet */
    IDL_msp->IDL_stack_packet_addr = NULL;
    IDL_msp->IDL_stack_packet_status = IDL_stack_packet_used_k;
    IDL_msp->IDL_restartable = idl_false;

    if (IDL_type_vec == NULL)
    {
        /* Pickling call, version check will be done later */
        return;
    }
    rpc_ss_type_vec_vers_check( IDL_msp );
}


/* always include these routines to read type vecs in either endian */

/******************************************************************************/
/*                                                                            */
/*  Build a bounds list from fixed bounds in the definition vector            */
/*                                                                            */
/******************************************************************************/
void rpc_ss_fixed_bounds_from_vector
(
    /* [in] */  idl_ulong_int dimensionality,
    /* [in] */  idl_byte *array_defn_ptr,   /* Points at array bounds */
    /* [out] */ IDL_bound_pair_t **p_bounds_list,
    IDL_msp_t IDL_msp
)
{
    IDL_bound_pair_t *bounds_list;
    idl_ulong_int i;

    bounds_list = (IDL_bound_pair_t *)rpc_ss_mem_alloc
        (&IDL_msp->IDL_mem_handle, (dimensionality * sizeof(IDL_bound_pair_t)));
    for (i=0; i<dimensionality; i++)
    {
        IDL_GET_LONG_FROM_VECTOR(bounds_list[i].lower,array_defn_ptr);
        IDL_GET_LONG_FROM_VECTOR(bounds_list[i].upper,array_defn_ptr);
    }
    *p_bounds_list = bounds_list;
}

/******************************************************************************/
/*                                                                            */
/*  Return the switch value for the specified union arm                       */
/*                                                                            */
/******************************************************************************/
idl_ulong_int rpc_ss_arm_switch_value
(
    /* [in] */  idl_byte *defn_vec_ptr,     /* Points at first union arm defn */
    /* [in] */ idl_long_int index,          /* Index of union arm desc */
    /* [in] */ IDL_msp_t IDL_msp            /* Needed for GET_LONG macro */
)
{
    idl_byte *switch_value_ptr;     /* Points to switch value in defn vec */
    idl_ulong_int switch_value;

    switch_value_ptr = defn_vec_ptr + index * IDL_UNION_ARM_DESC_WIDTH;
    IDL_GET_LONG_FROM_VECTOR(switch_value, switch_value_ptr);
    return(switch_value);
}


#ifndef _KERNEL

/******************************************************************************/
/*                                                                            */
/*  Write an unsigned value of specified integer type to an untyped location  */
/*                                                                            */
/******************************************************************************/
void rpc_ss_put_typed_integer
#ifdef IDL_PROTOTYPES
(
    /* in */ idl_ulong_int value,
    /* in */ idl_byte type,
    /* in */ rpc_void_p_t address,
    IDL_msp_t IDL_msp
)
#else
(value, type, address, IDL_msp)
    idl_ulong_int value;
    idl_byte type;
    rpc_void_p_t address;
    IDL_msp_t IDL_msp;
#endif
{
    switch (type)
    {
        case IDL_DT_SMALL:
                *(idl_small_int *)address = (idl_small_int)value;
            break;
        case IDL_DT_USMALL:
                *(idl_usmall_int *)address = (idl_usmall_int)value;
            break;
        case IDL_DT_SHORT:
            *(idl_short_int *)address = (idl_short_int)value;
            break;
        case IDL_DT_USHORT:
                *(idl_ushort_int *)address = (idl_ushort_int)value;
            break;
        case IDL_DT_LONG:
            *(idl_long_int *)address = (idl_long_int)value;
            break;
        case IDL_DT_ULONG:
            *(idl_ulong_int *)address = (idl_ulong_int)value;
            break;
        default:
            RAISE(rpc_x_coding_error);
    }
}

#endif


