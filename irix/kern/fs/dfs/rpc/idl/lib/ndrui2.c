/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(_KERNEL)
static const char *_identString = "$Id: ndrui2.c,v 65.7 1998/11/18 22:05:00 bdr Exp $";
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
 * $Log: ndrui2.c,v $
 * Revision 65.7  1998/11/18 22:05:00  bdr
 * Modify the marshalling code to properly cast types to deal with
 * differing SGI ABI types.  This is a fix for PV 652837.
 *
 * Revision 65.6  1998/03/24 17:43:53  lmc
 * Add or change typecasting to be correct on 64bit machines.  Added end of
 * comment where it was missing.
 *
 * Revision 65.5  1998/03/23  17:24:53  gwehrman
 * Source Identification changed to bracket with SGIMIPS && !_KERNEL.
 *
 * Revision 65.4  1998/01/07 17:19:52  jdoak
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS
 *
 * Revision 65.3  1997/11/06  19:56:01  jdoak
 * Source Identification added.
 *
 * Revision 65.2  1997/10/20  19:15:40  jdoak
 * Initial IRIX 6.4 code merge
 *
 * Revision 1.1.15.1  1996/10/15  20:46:27  arvind
 * 	move initialization of Z_values further up to squelch a purify
 * 	warning.
 * 	[1996/10/10  23:57 UTC  sommerfeld  /main/sommerfeld_pk_kdc_2/1]
 *
 * Revision 1.1.13.2  1996/02/18  18:55:11  marty
 * 	Update OSF copyright years
 * 	[1996/02/18  18:07:25  marty]
 * 
 * Revision 1.1.13.1  1995/12/07  22:30:43  root
 * 	Submit OSF/DCE 1.2.1
 * 
 * 	HP revision /main/HPDCE02/jrr_1.2_mothra/1  1995/11/20  23:08 UTC  jrr
 * 	Merge second XIDL drop for DCE 1.2.1
 * 	[1995/11/17  17:08 UTC  dat  /main/dat_xidl2/1]
 * 
 * 	oct 95 idl drop
 * 	[1995/10/22  23:36:17  bfc]
 * 	 *
 * 	may 95 idl drop
 * 	[1995/10/22  22:57:55  bfc]
 * 	 *
 * 	DCE for DEC OSF/1: populate from OSF DCE R1.1
 * 	[1995/10/21  18:25:57  bfc]
 * 	 *
 * 
 * 	HP revision /main/HPDCE02/1  1994/08/03  16:33 UTC  tatsu_s
 * 	Merged mothra up to dce 1.1.
 * 	[1995/12/07  21:16:07  root]
 * 
 * Revision 1.1.8.2  1994/07/13  18:56:26  tom
 * 	Bug 10103 - Reduce little-endian bias of stubs
 * 	[1994/07/12  18:49:42  tom]
 * 
 * Revision 1.1.8.1  1994/05/11  17:37:41  rico
 * 	Fixes for [string] and [ptr] interactions.
 * 	[1994/05/10  14:22:01  rico]
 * 
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
 * 	HP revision /main/HPDCE01/lmm_idl_endian_fixes/1  1994/04/08  15:17 UTC  lmm
 * 	Support type vectors spelled in either endian
 * 
 * 	HP revision /main/HPDCE01/2  1994/03/10  21:26 UTC  lmm
 * 	changing idl endian bias
 * 
 * 	HP revision /main/HPDCE01/lmm_idl_endian/1  1994/03/10  21:10 UTC  lmm
 * 	changing idl endian bias
 * 
 * Revision 1.1.5.3  1993/11/11  22:04:46  sanders
 * 	removed statics so libbb could link to routines
 * 	[1993/11/11  22:02:10  sanders]
 * 
 * Revision 1.1.5.2  1993/10/15  17:16:28  lmm
 * 	Loading OSF bug fixes
 * 	[1993/10/15  15:16:07  lmm]
 * 
 * 	OT 9078 Fix pointer to structure containing conformant array of [cs_char]s
 * 	[1993/10/12  14:35:26  hinxman]
 * 
 * Revision 1.1.2.3  1993/07/14  21:53:21  pwang
 * 	Bug OT #8270, Fixed rpc_ss_ndr_unmar_struct() call by put
 * 	the 5th parameter struct_cs_shadow under "#ifndef _KERNEL"
 * 	which is the way it should have been
 * 	[1993/07/14  21:51:31  pwang]
 * 
 * Revision 1.1.2.2  1993/07/07  20:09:57  ganni
 * 	reduced stub idl sources
 * 	[1993/07/07  19:38:47  ganni]
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
**      ndrui2.c
**
**  FACILITY:
**
**      Interface Definition Language (IDL) Compiler
**
**  ABSTRACT:
**
**      NDR unmarshalling interpreter routines for - pointers
**
*/

#include <dce/idlddefs.h>
#include <ndrui.h>
#include <lsysdep.h>

/* this is done to get around having a variant for this file since
 * dce/nt v1.1 is still R1.0.3 based. 
 * \dw.  7/6/95
 */
#ifdef _DCE_PROTOTYPE_
byte_p_t IDL_ENTRY rpc_ss_return_pointer_to_node   _DCE_PROTOTYPE_ ((
    IDL_msp_t /* IDL_msp */,
    rpc_ss_node_table_t  /*tab*/,
    unsigned long  /*num*/,
    long  /*size*/,
    rpc_void_p_t (IDL_ALLOC_ENTRY *p_allocate) _DCE_PROTOTYPE_(( idl_size_t)),
    long * /*has_been_unmarshalled*/,
    long * /*new_node*/
));
#else
byte_p_t IDL_ENTRY rpc_ss_return_pointer_to_node
(
#ifdef IDL_PROTOTYPES
    IDL_msp_t IDL_msp,
    rpc_ss_node_table_t tab,
    unsigned long num,
    long size,
    rpc_void_p_t (IDL_ALLOC_ENTRY *p_allocate)(
        idl_size_t size
    ),
    long *has_been_unmarshalled,
    long *new_node
#endif
);

#endif



void rpc_ss_init_new_store_ptrs
(
#ifdef IDL_PROTOTYPES
    idl_byte storage_type,
    idl_byte *defn_vec_ptr,
    rpc_void_p_t storage_addr,
    idl_ulong_int *Z_values,
    IDL_msp_t IDL_msp
#endif
);

/******************************************************************************/
/*                                                                            */
/*  Unmarshall a pointer that is the target of a pointer                      */
/*                                                                            */
/******************************************************************************/
void rpc_ss_ndr_unmar_ptr_ptee
#ifdef IDL_PROTOTYPES
(
    /* [in] */ idl_byte pointee_type,
    /* [in] */ rpc_void_p_t p_node,    /* Pointer to storage for pointee */
    /* [in] */ idl_byte *defn_vec_ptr,  /* On entry points at pointee type */
    IDL_msp_t IDL_msp
)
#else
(pointee_type, p_node, defn_vec_ptr, IDL_msp)
    idl_byte pointee_type;
    rpc_void_p_t p_node;
    idl_byte *defn_vec_ptr;
    IDL_msp_t IDL_msp;
#endif
{
    IDL_pointee_desc_t pointee_desc;    /* Description of pointee's pointee */
    idl_ulong_int node_number;
    idl_ulong_int unique_flag;  /* Wire form of [unique] pointer */

    switch(pointee_type)
    {
        case IDL_DT_FULL_PTR:
            /* Unmarshall the node number */
            IDL_UNMAR_ULONG( &node_number );
#ifdef SGIMIPS
            *(rpc_void_p_t *)p_node = (rpc_void_p_t)(__psint_t)node_number;
#else
            *(rpc_void_p_t *)p_node = (rpc_void_p_t)node_number;
#endif /* SGIMIPS */ 
            defn_vec_ptr++;
            pointee_desc.dimensionality = 0;
            rpc_ss_ndr_unmar_pointee_desc( pointee_type, defn_vec_ptr,
                                              &pointee_desc, p_node, IDL_msp );
            rpc_ss_ndr_unmar_pointee( pointee_type, defn_vec_ptr, &pointee_desc,
                                                             p_node, IDL_msp );
            rpc_ss_ndr_u_rlse_pointee_desc( &pointee_desc, IDL_msp );
            break;
#ifndef _KERNEL
        case IDL_DT_UNIQUE_PTR:
            IDL_UNMAR_ULONG( &unique_flag );
            if (unique_flag == 0)
                *(rpc_void_p_t *)p_node = NULL;
            else
            {
                defn_vec_ptr++;
                pointee_desc.dimensionality = 0;
                rpc_ss_ndr_unmar_pointee_desc( pointee_type, defn_vec_ptr,
                                             &pointee_desc, p_node, IDL_msp );
                if (*(rpc_void_p_t *)p_node == NULL)
                {
                    *(rpc_void_p_t *)p_node = IDL_NEW_NODE;
                }
                rpc_ss_ndr_unmar_pointee( pointee_type, defn_vec_ptr,
                                             &pointee_desc, p_node, IDL_msp );
                rpc_ss_ndr_u_rlse_pointee_desc( &pointee_desc, IDL_msp );
            }
            break;
#endif
        case IDL_DT_REF_PTR:
            defn_vec_ptr++;
            pointee_desc.dimensionality = 0;
            rpc_ss_ndr_unmar_pointee_desc( pointee_type, defn_vec_ptr,
                                              &pointee_desc, p_node, IDL_msp );
            rpc_ss_ndr_unmar_pointee( pointee_type, defn_vec_ptr, &pointee_desc,
                                                             p_node, IDL_msp );
            rpc_ss_ndr_u_rlse_pointee_desc( &pointee_desc, IDL_msp );
            break;

	default:
            break;
    }
}

/******************************************************************************/
/*                                                                            */
/*  Unmarshall the target of a pointer                                        */
/*                                                                            */
/******************************************************************************/
void rpc_ss_ndr_unmar_pointee
#ifdef IDL_PROTOTYPES
(
    /* [in] */ idl_byte pointer_type,   /* [ptr], [unique] or [ref] */
    /* [in] */ idl_byte *defn_vec_ptr,  /* On entry points at pointee type */
    /* [in] */ IDL_pointee_desc_t *p_pointee_desc, /* Pointer to data structure
                                   containing pointee description.
                                 NULL if pointee cannot be a non-fixed array */
    /* [in,out] */ rpc_void_p_t *p_pointer,
                    /* Entry - for a full pointer points to a node number
                             - for non-full pointer
                                if points to NULL, storage must be allocated
                                otherwise points to address of storage for
                                                                    pointee
                       Exit - points to the address of the pointee */
    IDL_msp_t IDL_msp
)
#else
( pointer_type, defn_vec_ptr, p_pointee_desc, p_pointer, IDL_msp )
    idl_byte pointer_type;
    idl_byte *defn_vec_ptr;
    IDL_pointee_desc_t *p_pointee_desc;
    rpc_void_p_t *p_pointer;
    IDL_msp_t IDL_msp;
#endif
{
    idl_ulong_int node_number;
    idl_ulong_int node_size;
    rpc_void_p_t p_node;    /* Pointer to storage for pointee */
    long already_unmarshalled;
    long new_node;
    idl_byte pointee_type;
    idl_boolean type_has_pointers;
    idl_ulong_int pointee_defn_index;
    idl_byte *struct_defn_ptr;
    idl_ulong_int offset_index;
    idl_ulong_int *struct_offset_vec_ptr; /* Start of offsets for this struct */
    idl_ulong_int array_defn_index;
    idl_byte *array_defn_ptr;
    idl_ulong_int conf_dims;    /* Number of dimensions of conformance info */
    idl_ulong_int *Z_values;
    idl_byte *constr_defn_ptr;   /* Pointer to definition of constructed type */
    idl_ulong_int switch_value;  /* Also used for [cs_char] machinery */
#ifndef _KERNEL
    IDL_cs_shadow_elt_t *struct_cs_shadow = NULL;
#endif

    if ( (pointer_type == IDL_DT_FULL_PTR) 
            || (pointer_type == IDL_DT_UNIQUE_PTR) )
    {
#if defined(SGIMIPS) && _MIPS_SZLONG == 64
	/*
         * For 64 bit code we have a 64 bit value that we need to stuff 
	 * into a 32 bit value.  The upper 32 bits are not used so we are
	 * safe tossing them.
	 */
	node_number = (idl_ulong_int)((unsigned long)*p_pointer & 0xffffffff);
#else
        node_number = (idl_ulong_int)*p_pointer;
#endif
        if (node_number == 0)
        {
            /* Pointee of a null [ptr] or [unique] pointer */
            return;
        }
    }

    pointee_type = *defn_vec_ptr;
    if (pointee_type == IDL_DT_STRING)
    {
        /* Varying/open array code will do the right thing */
        defn_vec_ptr++;
        pointee_type = *defn_vec_ptr;
    }

    Z_values = NULL;

    if (pointee_type == IDL_DT_CONF_STRUCT)
    {
        /*
         * Return if the pointer is a non-null full pointer and
         * the pointee has already been unmarshalled.
         */
        if (pointer_type == IDL_DT_FULL_PTR
            && p_pointee_desc->already_unmarshalled)
            return;
        constr_defn_ptr = defn_vec_ptr + 2; /* Next byte after properties */
        IDL_GET_LONG_FROM_VECTOR(pointee_defn_index,constr_defn_ptr);
        struct_defn_ptr = IDL_msp->IDL_type_vec + pointee_defn_index;
        IDL_GET_LONG_FROM_VECTOR(offset_index,struct_defn_ptr);
        struct_offset_vec_ptr = IDL_msp->IDL_offset_vec + offset_index;
        IDL_GET_LONG_FROM_VECTOR(array_defn_index,struct_defn_ptr);
        array_defn_ptr = IDL_msp->IDL_type_vec + array_defn_index;
        conf_dims = (idl_ulong_int)*array_defn_ptr;
        rpc_ss_ndr_unmar_Z_values( conf_dims, &Z_values, IDL_msp );
#ifndef _KERNEL
#define struct_shadow_length switch_value
        if (*struct_defn_ptr == IDL_DT_CS_SHADOW)
        {
            struct_defn_ptr++;
            IDL_GET_LONG_FROM_VECTOR(struct_shadow_length, struct_defn_ptr);
            struct_cs_shadow = (IDL_cs_shadow_elt_t *)
                                            rpc_ss_mem_alloc
                                             (&IDL_msp->IDL_mem_handle, 
                                              struct_shadow_length * 
                                                sizeof(IDL_cs_shadow_elt_t));
            if (*(array_defn_ptr + 1 + conf_dims * IDL_CONF_BOUND_PAIR_WIDTH)
                                                             == IDL_DT_CS_TYPE)
            {
                /* Conformant array part has [cs_char] base type */
                rpc_ss_ndr_u_conf_cs_struct_hdr(struct_defn_ptr, 
                                                array_defn_ptr + 1,
                                                Z_values,
                                                *struct_offset_vec_ptr,
                                                *(defn_vec_ptr + 1),
                                                struct_shadow_length - 1,
                                                idl_false,
                                                struct_cs_shadow,
                                                NULL,
                                                IDL_msp);
            }
        }
#undef struct_shadow_length
#endif
    }

    if ( (pointer_type == IDL_DT_FULL_PTR)
         || (*p_pointer == IDL_NEW_NODE) )
    {
        /* Calculate storage size for pointee */
        if (pointee_type == IDL_DT_CONF_STRUCT)
        {
            array_defn_ptr++;   /* Was pointing at dimensionality */
            /* Skip over the bounds in the array defn to get to the base type */
            IDL_ADV_DEFN_PTR_OVER_BOUNDS( array_defn_ptr, conf_dims );
#ifndef _KERNEL
#define sz_index switch_value
            if (*array_defn_ptr == IDL_DT_CS_TYPE)
            {
                /* There is one [size_is] variable. Get its index in the 
                                                                    cs-shadow */
                array_defn_ptr -= 4;
                IDL_GET_LONG_FROM_VECTOR(sz_index, array_defn_ptr);
                sz_index--;
                node_size = rpc_ss_ndr_allocation_size( *struct_offset_vec_ptr,
                                 conf_dims,
                                 &struct_cs_shadow[sz_index].IDL_data.IDL_value,
                                 array_defn_ptr, IDL_msp );
            }
            else
#undef sz_index
#endif
                node_size = rpc_ss_ndr_allocation_size( *struct_offset_vec_ptr,
                                 conf_dims, Z_values, array_defn_ptr, IDL_msp );
        }
        else if ( (pointee_type == IDL_DT_VARYING_ARRAY)
                 || (pointee_type == IDL_DT_CONF_ARRAY)
                 || (pointee_type == IDL_DT_OPEN_ARRAY) )
        {
            /*
             * The pointer is non-null and the pointee type is a non-fixed
             * array.  Return if the pointee has already been unmarshalled.
             */
            if (pointer_type == IDL_DT_FULL_PTR
                && p_pointee_desc->already_unmarshalled)
                return;
            node_size = rpc_ss_ndr_allocation_size( 0,
                                            p_pointee_desc->dimensionality,
                                            p_pointee_desc->Z_values,
                                            p_pointee_desc->array_base_defn_ptr,
                                            IDL_msp );
        }
        else
            node_size = rpc_ss_type_size(defn_vec_ptr, IDL_msp);
    }

    if (pointer_type == IDL_DT_FULL_PTR)
    {
        p_node = (rpc_void_p_t)rpc_ss_return_pointer_to_node(
		    IDL_msp,
                    IDL_msp->IDL_node_table,
                    node_number, node_size,
                    (IDL_msp->IDL_side == IDL_client_side_k)
                        ? IDL_msp->IDL_p_allocate : NULL,
                    &already_unmarshalled,
                    &new_node );
        if (p_node == NULL)
            RAISE( rpc_x_no_memory );
        *p_pointer = p_node;
        if ( already_unmarshalled )
            return;
    }
    else if (*p_pointer == IDL_NEW_NODE)
    {
        rpc_ss_ndr_alloc_storage( node_size, 0, NULL, NULL, &p_node, IDL_msp );
        *p_pointer = p_node;
        new_node = idl_true;
    }
    else
    {
        new_node = idl_false;
        p_node = *p_pointer;
    }

    if ( new_node )
    {
        /* Fill in any [ref] or [unique] pointers */
        switch (pointee_type)
        {
            case IDL_DT_FIXED_STRUCT:
                constr_defn_ptr = defn_vec_ptr + 1;    /* Point at properties */
                if ( IDL_PROP_TEST(*constr_defn_ptr, IDL_PROP_HAS_PTRS) )
                {
                    constr_defn_ptr++;
                    IDL_GET_LONG_FROM_VECTOR(pointee_defn_index,
                                                            constr_defn_ptr);
                    struct_defn_ptr = IDL_msp->IDL_type_vec
                                                         + pointee_defn_index;
                    rpc_ss_init_new_struct_ptrs( pointee_type, struct_defn_ptr,
                                             p_node, NULL, IDL_msp );
                }
                break;
            case IDL_DT_CONF_STRUCT:
                constr_defn_ptr = defn_vec_ptr + 1;    /* Point at properties */
                if ( IDL_PROP_TEST(*constr_defn_ptr, IDL_PROP_HAS_PTRS) )
                {
                    constr_defn_ptr++;
                    IDL_GET_LONG_FROM_VECTOR(pointee_defn_index,
                                                            constr_defn_ptr);
                    struct_defn_ptr = IDL_msp->IDL_type_vec
                                                         + pointee_defn_index;
                    rpc_ss_init_new_struct_ptrs( pointee_type, struct_defn_ptr,
                                             p_node, Z_values, IDL_msp );
                }
                break;
            case IDL_DT_FIXED_ARRAY:
                constr_defn_ptr = defn_vec_ptr + 1;    /* Point at properties */
                if ( IDL_PROP_TEST(*constr_defn_ptr, IDL_PROP_HAS_PTRS) )
                {
                    constr_defn_ptr++;
                    IDL_DISCARD_LONG_FROM_VECTOR( constr_defn_ptr );
                                                    /* Full array definition */
                    IDL_GET_LONG_FROM_VECTOR( pointee_defn_index,
                                                             constr_defn_ptr );
                    rpc_ss_init_new_store_ptrs( pointee_type,
                                     IDL_msp->IDL_type_vec + pointee_defn_index,
                                             p_node, NULL, IDL_msp );
                }
                break;
            case IDL_DT_VARYING_ARRAY:
            case IDL_DT_CONF_ARRAY:
            case IDL_DT_OPEN_ARRAY:
                if (p_pointee_desc->base_type_has_pointers)
                {
                    rpc_ss_init_new_array_ptrs( p_pointee_desc->dimensionality,
                                            p_pointee_desc->Z_values,
                                            p_pointee_desc->array_base_defn_ptr,
                                            p_node, IDL_msp );
                }
                break;
#ifndef _KERNEL
            case IDL_DT_UNIQUE_PTR:
                *(rpc_void_p_t *)p_node = IDL_NEW_NODE;
                break;
#endif
            case IDL_DT_REF_PTR:
                rpc_ss_alloc_pointer_target( defn_vec_ptr+1, p_node, IDL_msp );
                break;
            default:
                break;
        }
    }

    switch(pointee_type)
    {
        case IDL_DT_BYTE:
            IDL_UNMAR_BYTE( p_node );
            break;
        case IDL_DT_CHAR:
            IDL_UNMAR_CHAR( p_node );
            break;
        case IDL_DT_BOOLEAN:
            IDL_UNMAR_BOOLEAN( p_node );
            break;
#ifndef _KERNEL
        case IDL_DT_DOUBLE:
            IDL_UNMAR_DOUBLE( p_node );
            break;
#endif
        case IDL_DT_ENUM:
            IDL_UNMAR_ENUM( p_node );
            break;
#ifndef _KERNEL
        case IDL_DT_FLOAT:
            IDL_UNMAR_FLOAT( p_node );
            break;
#endif
        case IDL_DT_SMALL:
            IDL_UNMAR_SMALL( p_node );
            break;
        case IDL_DT_SHORT:
            IDL_UNMAR_SHORT( p_node );
            break;
        case IDL_DT_LONG:
            IDL_UNMAR_LONG( p_node );
            break;
#ifndef _KERNEL
        case IDL_DT_HYPER:
            IDL_UNMAR_HYPER( p_node );
            break;
#endif
        case IDL_DT_USMALL:
            IDL_UNMAR_USMALL( p_node );
            break;
        case IDL_DT_USHORT:
            IDL_UNMAR_USHORT( p_node );
            break;
        case IDL_DT_ULONG:
            IDL_UNMAR_ULONG( p_node );
            break;
#ifndef _KERNEL
        case IDL_DT_UHYPER:
            IDL_UNMAR_UHYPER( p_node );
            break;
#endif
        case IDL_DT_ERROR_STATUS:
            IDL_UNMAR_ERROR_STATUS( p_node );
            break;
        case IDL_DT_FIXED_STRUCT:
        case IDL_DT_CONF_STRUCT:
            /* Properties byte */
            defn_vec_ptr++;
            type_has_pointers = IDL_PROP_TEST(*defn_vec_ptr, IDL_PROP_HAS_PTRS);
            defn_vec_ptr++;
            IDL_GET_LONG_FROM_VECTOR(pointee_defn_index,defn_vec_ptr);
            rpc_ss_ndr_unmar_struct(pointee_type, 
                                   IDL_msp->IDL_type_vec+pointee_defn_index,
                                   p_node, Z_values, 
#ifndef _KERNEL
						struct_cs_shadow, 
#else
						NULL, 
#endif
								IDL_msp);
            if (type_has_pointers)
            {
                rpc_ss_ndr_u_struct_pointees( pointee_type, pointee_defn_index,
                                                p_node, Z_values, IDL_msp );
            }
            if (pointee_type == IDL_DT_CONF_STRUCT)
                rpc_ss_mem_item_free(&IDL_msp->IDL_mem_handle,
                                        (byte_p_t)Z_values);
            break;
        case IDL_DT_FIXED_ARRAY:
            /* Properties byte */
            defn_vec_ptr++;
            type_has_pointers = IDL_PROP_TEST(*defn_vec_ptr, IDL_PROP_HAS_PTRS);
            defn_vec_ptr++;
            IDL_DISCARD_LONG_FROM_VECTOR( defn_vec_ptr );
                                                    /* Full array definition */
            IDL_GET_LONG_FROM_VECTOR(pointee_defn_index, defn_vec_ptr);
            rpc_ss_ndr_unmar_fixed_arr( pointee_defn_index, p_node,
                                        0, IDL_msp );
            if (type_has_pointers)
            {
                rpc_ss_ndr_u_fixed_arr_ptees( pointee_defn_index, p_node,
                                                                     IDL_msp );
            }
            break;
        case IDL_DT_VARYING_ARRAY:
        case IDL_DT_OPEN_ARRAY:
            rpc_ss_ndr_u_var_or_open_arr( p_pointee_desc->dimensionality,
                                          p_pointee_desc->Z_values,
                                          p_pointee_desc->array_base_defn_ptr,
                                          p_node,
                                          p_pointee_desc->range_list, 
                                          0, IDL_msp );
            if (p_pointee_desc->base_type_has_pointers)
            {
                rpc_ss_ndr_u_v_or_o_arr_ptees( p_pointee_desc->dimensionality,
                                          p_pointee_desc->Z_values,
                                          p_pointee_desc->array_base_defn_ptr,
                                          p_node,
                                          p_pointee_desc->range_list, IDL_msp );
            }
            break;
        case IDL_DT_CONF_ARRAY:
            rpc_ss_ndr_u_fix_or_conf_arr( p_pointee_desc->dimensionality,
                                          p_pointee_desc->Z_values,
                                          p_pointee_desc->array_base_defn_ptr,
                                          p_node, IDL_M_CONF_ARRAY, IDL_msp );
            if (p_pointee_desc->base_type_has_pointers)
            {
                rpc_ss_ndr_u_f_or_c_arr_ptees( p_pointee_desc->dimensionality,
                                          p_pointee_desc->Z_values,
                                          p_pointee_desc->array_base_defn_ptr,
                                          p_node, IDL_msp );
            }
            break;
        case IDL_DT_ENC_UNION:
            /* Properties byte */
            defn_vec_ptr++;
            type_has_pointers = IDL_PROP_TEST(*defn_vec_ptr, IDL_PROP_HAS_PTRS);
            defn_vec_ptr++;
            IDL_GET_LONG_FROM_VECTOR(pointee_defn_index, defn_vec_ptr);
            rpc_ss_ndr_u_enc_union_or_ptees(p_node, pointee_defn_index,
                                            idl_false, IDL_msp);
            if (type_has_pointers)
            {
                rpc_ss_ndr_u_enc_union_or_ptees(p_node,
                                        pointee_defn_index, idl_true, IDL_msp);
            }
            break;
#ifndef _KERNEL
        case IDL_DT_N_E_UNION:
            /* Properties byte */
            defn_vec_ptr++;
            type_has_pointers = IDL_PROP_TEST(*defn_vec_ptr, IDL_PROP_HAS_PTRS);
            defn_vec_ptr++;
            IDL_DISCARD_LONG_FROM_VECTOR(defn_vec_ptr); /* Switch index */
            IDL_GET_LONG_FROM_VECTOR(pointee_defn_index,defn_vec_ptr);
            rpc_ss_ndr_unmar_n_e_union(p_node, pointee_defn_index,
                                       &switch_value, IDL_msp);
            if (type_has_pointers)
            {
                rpc_ss_ndr_u_n_e_union_ptees(p_node, switch_value, 0,
                                       pointee_defn_index, NULL, NULL, IDL_msp);
            }
            break;
#endif
        case IDL_DT_FULL_PTR:
#ifndef _KERNEL
        case IDL_DT_UNIQUE_PTR:
#endif
        case IDL_DT_REF_PTR:
            rpc_ss_ndr_unmar_ptr_ptee(pointee_type, p_node, defn_vec_ptr,
                                                                     IDL_msp);
            break;
        case IDL_DT_TRANSMIT_AS:
        case IDL_DT_REPRESENT_AS:
            defn_vec_ptr += 2;      /* Byte after properties byte */
            IDL_GET_LONG_FROM_VECTOR(pointee_defn_index, defn_vec_ptr);
            rpc_ss_ndr_unmar_xmit_as(pointee_defn_index, p_node, NULL, IDL_msp);
            break;
        case IDL_DT_INTERFACE:
            defn_vec_ptr++;     /* Properties byte */
            IDL_GET_LONG_FROM_VECTOR(pointee_defn_index, defn_vec_ptr);
            rpc_ss_ndr_unmar_interface(pointee_defn_index,p_node,NULL,IDL_msp);
            break;
#ifndef _KERNEL
        case IDL_DT_CS_ARRAY:
            defn_vec_ptr++;     /* Move to IDL_DT_FIXED_ARRAY */
            rpc_ss_ndr_unmar_cs_array(p_node, NULL, NULL, 0, &defn_vec_ptr,
                                                                       IDL_msp);
            break;
        case IDL_DT_CS_TYPE:
            defn_vec_ptr += 2;      /* Byte after properties byte */
            IDL_GET_LONG_FROM_VECTOR(pointee_defn_index, defn_vec_ptr);
            rpc_ss_ndr_unmar_cs_char(p_node, pointee_defn_index, IDL_msp);
            break;
#endif
        default:
#ifdef DEBUG_INTERP
             printf("rpc_ss_ndr_unmar_pointee:unrecognized type %d\n",
                        pointee_type);
             exit(0);
#endif
             RAISE(rpc_x_coding_error);
    }
}

/******************************************************************************/
/*                                                                            */
/*  Unmarshall a structure's pointees                                         */
/*                                                                            */
/******************************************************************************/
void rpc_ss_ndr_u_struct_pointees
#ifdef IDL_PROTOTYPES
(
    /* [in] */  idl_byte struct_type,   /* DT_FIXED_STRUCT or DT_CONF_STRUCT */
    /* [in] */  idl_ulong_int defn_index,   /* Index of structure description */
    /* [in] */  rpc_void_p_t struct_addr,
    /* [in] */  idl_ulong_int *Z_values,    /* If conformant structure, the
                 Z values for the conformant or open field, otherwise ignored */
    IDL_msp_t IDL_msp
)
#else
(struct_type, defn_index, struct_addr, Z_values, IDL_msp)
    idl_byte struct_type;
    idl_ulong_int defn_index;
    rpc_void_p_t struct_addr;
    idl_ulong_int *Z_values;
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
    idl_boolean type_has_pointers;
    IDL_bound_pair_t *bounds_list;
    idl_ulong_int *varying_Z_values;    /* Z_values for a varying array field */
    IDL_bound_pair_t *range_list;
    idl_ulong_int array_dims;    /* Number of dimensions of array */
    IDL_pointee_desc_t pointee_desc;    /* Description of pointee */
    idl_ulong_int switch_index; /* Index of switch field for non-encapsulated
                                                                        union */

    idl_boolean add_null;       /* Dummy argument in procedure call */

    defn_vec_ptr = IDL_msp->IDL_type_vec + defn_index;
    IDL_GET_LONG_FROM_VECTOR(offset_index,defn_vec_ptr);
    struct_offset_vec_ptr = IDL_msp->IDL_offset_vec + offset_index;

    if (struct_type == IDL_DT_CONF_STRUCT)
    {
        IDL_DISCARD_LONG_FROM_VECTOR(defn_vec_ptr);
    }

    offset_vec_ptr = struct_offset_vec_ptr + 1;
                                        /* Skip over size at start of offsets */
    pointee_desc.dimensionality = 0;
    /* Loop over the fields of the structure */
    do {
        type_byte = *defn_vec_ptr;
        defn_vec_ptr++;
        switch(type_byte)
        {
            /* For fields that do not include pointers, just advance the
                definition and offset pointers in step */
#ifndef _KERNEL
            case IDL_DT_CS_SHADOW:
                IDL_DISCARD_LONG_FROM_VECTOR(defn_vec_ptr);
                break;
#endif
            case IDL_DT_BYTE:
            case IDL_DT_CHAR:
            case IDL_DT_BOOLEAN:
            case IDL_DT_DOUBLE:
            case IDL_DT_ENUM:
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
#endif
            case IDL_DT_IGNORE:
#ifndef _KERNEL
            case IDL_DT_V1_ENUM:
#endif
            case IDL_DT_ERROR_STATUS:
                offset_vec_ptr++;
                break;
            case IDL_DT_FIXED_ARRAY:
                /* Properties byte */
                type_has_pointers =
                             IDL_PROP_TEST(*defn_vec_ptr, IDL_PROP_HAS_PTRS);
                defn_vec_ptr++;
                IDL_DISCARD_LONG_FROM_VECTOR(defn_vec_ptr);
                                                    /* Full array definition */
                IDL_GET_LONG_FROM_VECTOR(field_defn_index, defn_vec_ptr);
                if (type_has_pointers)
                {
                    offset = *offset_vec_ptr;
                    rpc_ss_ndr_u_fixed_arr_ptees( field_defn_index,
                                      (idl_byte *)struct_addr+offset, IDL_msp);
                }
                offset_vec_ptr++;
                break;
            case IDL_DT_VARYING_ARRAY:
                /* Properties byte */
                type_has_pointers =
                             IDL_PROP_TEST(*defn_vec_ptr, IDL_PROP_HAS_PTRS);
                defn_vec_ptr++;
                IDL_DISCARD_LONG_FROM_VECTOR(defn_vec_ptr);
                                                    /* Full array definition */
                IDL_GET_LONG_FROM_VECTOR(field_defn_index, defn_vec_ptr);
                if (type_has_pointers)
                {
                    offset = *offset_vec_ptr;
                    field_defn_ptr = IDL_msp->IDL_type_vec + field_defn_index; 
                    array_dims = (idl_ulong_int)*field_defn_ptr;
                    field_defn_ptr++;
		    if (IDL_msp->IDL_type_vec[TVEC_INT_REP_OFFSET] != NDR_LOCAL_INT_REP)
		        rpc_ss_fixed_bounds_from_vector(array_dims,
							field_defn_ptr, 
							&bounds_list, IDL_msp);
		    else
		      bounds_list = (IDL_bound_pair_t *)field_defn_ptr;
                    varying_Z_values = NULL;
                    range_list = NULL;
                    rpc_ss_Z_values_from_bounds( bounds_list, array_dims,
                                                 &varying_Z_values, IDL_msp );
                    /* Advance definition pointer over bounds */
                    field_defn_ptr += array_dims * IDL_FIXED_BOUND_PAIR_WIDTH;
                    rpc_ss_build_range_list(&field_defn_ptr,
                                (rpc_void_p_t)((idl_byte *)struct_addr+offset),
                                struct_addr, struct_offset_vec_ptr, array_dims,
                                bounds_list, &range_list, &add_null, IDL_msp);
                    rpc_ss_ndr_u_v_or_o_arr_ptees( array_dims, varying_Z_values,
                            field_defn_ptr,
                            (rpc_void_p_t)((idl_byte *)struct_addr+offset),
                            range_list, IDL_msp );
                    rpc_ss_mem_item_free(&IDL_msp->IDL_mem_handle,
                                            (byte_p_t)range_list);
                    rpc_ss_mem_item_free(&IDL_msp->IDL_mem_handle,
                                            (byte_p_t)varying_Z_values);
		    if (IDL_msp->IDL_type_vec[TVEC_INT_REP_OFFSET] != NDR_LOCAL_INT_REP)
		      rpc_ss_mem_item_free(&IDL_msp->IDL_mem_handle,
                                            (byte_p_t)bounds_list);
                }                                   
                offset_vec_ptr++;
                break;
            case IDL_DT_CONF_ARRAY:
                /* Properties byte */
                type_has_pointers =
                             IDL_PROP_TEST(*defn_vec_ptr, IDL_PROP_HAS_PTRS);
                defn_vec_ptr++;
                IDL_DISCARD_LONG_FROM_VECTOR(defn_vec_ptr);
                                                    /* Full array definition */
                IDL_GET_LONG_FROM_VECTOR(field_defn_index, defn_vec_ptr);
                if (type_has_pointers)
                {
                    /* Conformant array must be last field of struct */
                    offset = *offset_vec_ptr;
                    field_defn_ptr = IDL_msp->IDL_type_vec + field_defn_index;
                    array_dims = (idl_ulong_int)*field_defn_ptr;
                    field_defn_ptr++;   /* Now at (0 mod 4) */
                    IDL_ADV_DEFN_PTR_OVER_BOUNDS( field_defn_ptr, array_dims );
                    rpc_ss_ndr_u_f_or_c_arr_ptees( array_dims, Z_values,
                             field_defn_ptr, (idl_byte *)struct_addr+offset,
                             IDL_msp );
                }
                break;
            case IDL_DT_OPEN_ARRAY:
                /* Properties byte */
                type_has_pointers =
                             IDL_PROP_TEST(*defn_vec_ptr, IDL_PROP_HAS_PTRS);
                defn_vec_ptr++;
                IDL_DISCARD_LONG_FROM_VECTOR(defn_vec_ptr);
                                                    /* Full array definition */
                IDL_GET_LONG_FROM_VECTOR(field_defn_index, defn_vec_ptr);
                if (type_has_pointers)
                {
                    /* Open array must be last field of struct */
                    offset = *offset_vec_ptr;
                    field_defn_ptr = IDL_msp->IDL_type_vec + field_defn_index;
                    array_dims = (idl_ulong_int)*field_defn_ptr;
                    field_defn_ptr++;   /* Now at (0 mod 4) */
                    bounds_list = NULL;
                    range_list = NULL;
                    rpc_ss_build_bounds_list(&field_defn_ptr,
                                (rpc_void_p_t)((idl_byte *)struct_addr+offset),
                                struct_addr, struct_offset_vec_ptr, array_dims,
                                            &bounds_list, IDL_msp);
                    rpc_ss_build_range_list(&field_defn_ptr,
                                (rpc_void_p_t)((idl_byte *)struct_addr+offset),
                                struct_addr, struct_offset_vec_ptr, array_dims,
                                bounds_list, &range_list, &add_null, IDL_msp);
                    rpc_ss_ndr_u_v_or_o_arr_ptees( array_dims, Z_values,
                         field_defn_ptr, (idl_byte *)struct_addr+offset,
                         range_list, IDL_msp );
                    rpc_ss_mem_item_free(&IDL_msp->IDL_mem_handle,
                                            (byte_p_t)range_list);
                    rpc_ss_mem_item_free(&IDL_msp->IDL_mem_handle,
                                            (byte_p_t)bounds_list);
                }
                break;
            case IDL_DT_ENC_UNION:
                /* Properties byte */
                type_has_pointers =
                             IDL_PROP_TEST(*defn_vec_ptr, IDL_PROP_HAS_PTRS);
                defn_vec_ptr++;
                IDL_GET_LONG_FROM_VECTOR(field_defn_index, defn_vec_ptr);
                if (type_has_pointers)
                {
                    offset = *offset_vec_ptr;
                    rpc_ss_ndr_u_enc_union_or_ptees(
                                              (idl_byte *)struct_addr+offset,
                                           field_defn_index, idl_true, IDL_msp);
                }
                offset_vec_ptr++;
                break;
#ifndef _KERNEL
            case IDL_DT_N_E_UNION:
                /* Properties byte */
                type_has_pointers =
                             IDL_PROP_TEST(*defn_vec_ptr, IDL_PROP_HAS_PTRS);
                defn_vec_ptr++;
                IDL_GET_LONG_FROM_VECTOR(switch_index, defn_vec_ptr);
                IDL_GET_LONG_FROM_VECTOR(field_defn_index, defn_vec_ptr);
                if (type_has_pointers)
                {
                    offset = *offset_vec_ptr;
                    rpc_ss_ndr_u_n_e_union_ptees(
                                    (idl_byte *)struct_addr+offset, 0,
                                    switch_index, field_defn_index, struct_addr,
                                    struct_offset_vec_ptr, IDL_msp);
                }
                offset_vec_ptr++;
                break;
#endif
            case IDL_DT_FULL_PTR:
                defn_vec_ptr++;     /* Properties byte */
                IDL_GET_LONG_FROM_VECTOR(field_defn_index, defn_vec_ptr);
                offset = *offset_vec_ptr;
                offset_vec_ptr++;
                if (*(rpc_void_p_t *)((idl_byte *)struct_addr + offset)
                                                                     != NULL)
                {
                    field_defn_ptr = IDL_msp->IDL_type_vec + field_defn_index;
                    rpc_ss_ndr_unmar_pointee_desc( type_byte,
                            field_defn_ptr, &pointee_desc,
                            (rpc_void_p_t *)((idl_byte *)struct_addr + offset),
                            IDL_msp );
                    rpc_ss_ndr_unmar_pointee( type_byte,
                            field_defn_ptr, &pointee_desc,
                            (rpc_void_p_t *)((idl_byte *)struct_addr + offset),
                            IDL_msp );
                }
                break;
#ifndef _KERNEL
            case IDL_DT_UNIQUE_PTR:
                defn_vec_ptr++;     /* Properties byte */
                IDL_GET_LONG_FROM_VECTOR(field_defn_index, defn_vec_ptr);
                offset = *offset_vec_ptr;
                offset_vec_ptr++;
                if (*(rpc_void_p_t *)((idl_byte *)struct_addr + offset)
                                                                     != NULL)
                {
                    field_defn_ptr = IDL_msp->IDL_type_vec
                                                    + field_defn_index;
                    rpc_ss_ndr_unmar_pointee_desc( type_byte, field_defn_ptr,
                            &pointee_desc,
                            (rpc_void_p_t *)((idl_byte *)struct_addr + offset),
                            IDL_msp );
                    rpc_ss_ndr_unmar_pointee( type_byte, field_defn_ptr,
                            &pointee_desc,
                            (rpc_void_p_t *)((idl_byte *)struct_addr + offset),
                            IDL_msp );
                }
                break;
#endif
            case IDL_DT_REF_PTR:
                defn_vec_ptr++;     /* Properties byte */
                IDL_GET_LONG_FROM_VECTOR(field_defn_index, defn_vec_ptr);
                field_defn_ptr = IDL_msp->IDL_type_vec+field_defn_index;
                offset = *offset_vec_ptr;
                offset_vec_ptr++;
                rpc_ss_ndr_unmar_pointee_desc( type_byte, field_defn_ptr,
                        &pointee_desc,
                        (rpc_void_p_t *)((idl_byte *)struct_addr + offset),
                        IDL_msp );
                rpc_ss_ndr_unmar_pointee( type_byte, field_defn_ptr,
                        &pointee_desc,
                        (rpc_void_p_t *)((idl_byte *)struct_addr + offset),
                        IDL_msp );
                break;
            case IDL_DT_TRANSMIT_AS:
            case IDL_DT_REPRESENT_AS:
#ifndef _KERNEL
            case IDL_DT_CS_TYPE:
#endif
                defn_vec_ptr++;     /* Properties byte */
                IDL_DISCARD_LONG_FROM_VECTOR( defn_vec_ptr );
                offset = *offset_vec_ptr;
                offset_vec_ptr++;
                break;
            case IDL_DT_STRING:
            case IDL_DT_NDR_ALIGN_2:
            case IDL_DT_NDR_ALIGN_4:
            case IDL_DT_NDR_ALIGN_8:
            case IDL_DT_BEGIN_NESTED_STRUCT:
            case IDL_DT_END_NESTED_STRUCT:
#ifndef _KERNEL
            case IDL_DT_V1_ARRAY:
            case IDL_DT_V1_STRING:
            case IDL_DT_CS_ATTRIBUTE:
            case IDL_DT_CS_ARRAY:
            case IDL_DT_CS_RLSE_SHADOW:
#endif
            case IDL_DT_EOL:
                break;
            default:
#ifdef DEBUG_INTERP
                printf("rpc_ss_ndr_u_struct_pointees:unrecognized type %d\n",
                        type_byte);
                exit(0);
#endif
                RAISE(rpc_x_coding_error);
        }
    } while (type_byte != IDL_DT_EOL);

    rpc_ss_ndr_u_rlse_pointee_desc( &pointee_desc, IDL_msp ); 
}

/******************************************************************************/
/*                                                                            */
/* Unmarshall the pointees of a fixed or conformant array                     */
/*                                                                            */
/******************************************************************************/
void rpc_ss_ndr_u_f_or_c_arr_ptees
#ifdef IDL_PROTOTYPES
(
    /* [in] */  idl_ulong_int dimensionality,
    /* [in] */  idl_ulong_int *Z_values,
    /* [in] */  idl_byte *defn_vec_ptr, /* On entry points at array base info */
    /* [in] */  rpc_void_p_t array_addr,
    IDL_msp_t IDL_msp
)
#else
( dimensionality, Z_values, defn_vec_ptr, array_addr, IDL_msp )
    idl_ulong_int dimensionality;
    idl_ulong_int *Z_values;
    idl_byte *defn_vec_ptr;
    rpc_void_p_t array_addr;
    IDL_msp_t IDL_msp;
#endif
{
    idl_ulong_int element_count;
    idl_ulong_int i;
    idl_byte base_type;
    idl_ulong_int element_size;
    idl_ulong_int element_defn_index;
    idl_ulong_int element_offset_index;
    idl_byte *element_defn_ptr;
    rpc_void_p_t *array_elt_addr;
    IDL_pointee_desc_t pointee_desc;

    element_count = 1;
    for (i=0; i<dimensionality; i++)
    {
        element_count *= Z_values[i];
    }

    base_type = *defn_vec_ptr;

    if (base_type == IDL_DT_FIXED_STRUCT)
    {
        defn_vec_ptr += 2;  /* DT_FIXED_STRUCT and properties byte */
        /* Array of structures containing pointers */
        IDL_GET_LONG_FROM_VECTOR(element_defn_index, defn_vec_ptr);
        element_defn_ptr = IDL_msp->IDL_type_vec + element_defn_index;
        IDL_GET_LONG_FROM_VECTOR(element_offset_index, element_defn_ptr);
        element_size = *(IDL_msp->IDL_offset_vec + element_offset_index);
        for (i=0; i<element_count; i++)
        {
            rpc_ss_ndr_u_struct_pointees( base_type, element_defn_index,
                                            array_addr, NULL, IDL_msp );
            array_addr = (rpc_void_p_t)((idl_byte *)array_addr + element_size);
        }
    }
    else if (base_type == IDL_DT_ENC_UNION)
    {
        defn_vec_ptr += 2;  /* DT_ENC_UNION and properties byte */
        /* Array of unions containing pointers */
        IDL_GET_LONG_FROM_VECTOR(element_defn_index, defn_vec_ptr);
        element_defn_ptr = IDL_msp->IDL_type_vec + element_defn_index;
        IDL_GET_LONG_FROM_VECTOR(element_offset_index, element_defn_ptr);
        element_size = *(IDL_msp->IDL_offset_vec + element_offset_index);
        for (i=0; i<element_count; i++)
        {
            rpc_ss_ndr_u_enc_union_or_ptees( array_addr, element_defn_index,
                                             idl_true, IDL_msp );
            array_addr = (rpc_void_p_t)((idl_byte *)array_addr + element_size);
        }
    }
    else
    {
        defn_vec_ptr++;     /* Move to pointee type */
        /* Array of pointers */
        array_elt_addr = (rpc_void_p_t *)(array_addr);
        pointee_desc.dimensionality = 0;
        for (i=0; i<element_count; i++)
        {
            rpc_ss_ndr_unmar_pointee_desc( base_type, defn_vec_ptr,
                          &pointee_desc, array_elt_addr, IDL_msp );
            rpc_ss_ndr_unmar_pointee( base_type, defn_vec_ptr, &pointee_desc,
                                         array_elt_addr, IDL_msp );
            array_elt_addr++;
        }
        rpc_ss_ndr_u_rlse_pointee_desc( &pointee_desc, IDL_msp );
    }
}

/******************************************************************************/
/*                                                                            */
/*  Unmarshall the pointees of a fixed array                                  */
/*                                                                            */
/******************************************************************************/
void rpc_ss_ndr_u_fixed_arr_ptees
#ifdef IDL_PROTOTYPES
(
    /* [in] */  idl_ulong_int defn_index,   /* Index of array description */
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
    idl_ulong_int *Z_values;
    IDL_bound_pair_t *bounds_list;

    defn_vec_ptr = IDL_msp->IDL_type_vec + defn_index; 
    dimensionality = (idl_ulong_int)*defn_vec_ptr;
    defn_vec_ptr++;     /* By design alignment is now (0 mod 4) */
      Z_values = NULL;
    if (IDL_msp->IDL_type_vec[TVEC_INT_REP_OFFSET] != NDR_LOCAL_INT_REP)
      rpc_ss_fixed_bounds_from_vector(dimensionality, defn_vec_ptr, &bounds_list,
				      IDL_msp);
    else
      bounds_list = (IDL_bound_pair_t *)defn_vec_ptr;
    rpc_ss_Z_values_from_bounds( bounds_list,
                                     dimensionality, &Z_values, IDL_msp );
    defn_vec_ptr += dimensionality * IDL_FIXED_BOUND_PAIR_WIDTH;
    rpc_ss_ndr_u_f_or_c_arr_ptees( dimensionality, Z_values, defn_vec_ptr,
                                     array_addr, IDL_msp);
    rpc_ss_mem_item_free(&IDL_msp->IDL_mem_handle, (byte_p_t)Z_values);
    if (IDL_msp->IDL_type_vec[TVEC_INT_REP_OFFSET] != NDR_LOCAL_INT_REP)
      rpc_ss_mem_item_free(&IDL_msp->IDL_mem_handle, (byte_p_t)bounds_list);
}

/******************************************************************************/
/*                                                                            */
/*  Unmarshall the pointees of a varying or open array                        */
/*                                                                            */
/******************************************************************************/
void rpc_ss_ndr_u_v_or_o_arr_ptees
#ifdef IDL_PROTOTYPES
(
    /* [in] */  idl_ulong_int dimensionality,
    /* [in] */  idl_ulong_int *Z_values,
    /* [in] */  idl_byte *defn_vec_ptr, /* On entry points to array base info */
    /* [in] */  rpc_void_p_t array_addr,
    /* [in] */  IDL_bound_pair_t *range_list,
    IDL_msp_t IDL_msp
)
#else
( dimensionality, Z_values, defn_vec_ptr, array_addr, range_list, IDL_msp )
    idl_ulong_int dimensionality;
    idl_ulong_int *Z_values;
    idl_byte *defn_vec_ptr;
    rpc_void_p_t array_addr;
    IDL_bound_pair_t *range_list;
    IDL_msp_t IDL_msp;
#endif
{
    idl_byte base_type;
    idl_ulong_int element_defn_index;
    idl_ulong_int element_size;
    idl_byte *element_defn_ptr;
    idl_ulong_int element_offset_index;
    IDL_varying_control_t *control_data;
    int i;
    idl_byte *inner_slice_address;  /* Address of 1-dim subset of array */
    int dim;    /* Index through array dimensions */
    IDL_pointee_desc_t pointee_desc;

    base_type = *defn_vec_ptr;

    element_size = rpc_ss_type_size(defn_vec_ptr, IDL_msp); 
    defn_vec_ptr++;

    if ( (base_type == IDL_DT_FIXED_STRUCT) || (base_type == IDL_DT_ENC_UNION) )
    {
        IDL_GET_LONG_FROM_VECTOR( element_defn_index, defn_vec_ptr );
        element_defn_ptr = IDL_msp->IDL_type_vec + element_defn_index;
        IDL_GET_LONG_FROM_VECTOR(element_offset_index, element_defn_ptr);
        element_size = *(IDL_msp->IDL_offset_vec + element_offset_index);
    }


    control_data = (IDL_varying_control_t *)rpc_ss_mem_alloc(
                                &IDL_msp->IDL_mem_handle,
                                dimensionality * sizeof(IDL_varying_control_t));
    control_data[dimensionality-1].subslice_size = element_size;
    control_data[dimensionality-1].index_value = 
                                            range_list[dimensionality-1].lower;
    for (i=dimensionality-2; i>=0; i--)
    {
        control_data[i].index_value = range_list[i].lower;
        control_data[i].subslice_size = control_data[i+1].subslice_size
                                                            * Z_values[i+1];   
    }

    pointee_desc.dimensionality = 0;
    do {
        inner_slice_address = (idl_byte *)array_addr;
        for (i=0; i<dimensionality; i++)
        {
            inner_slice_address += control_data[i].index_value
                                        * control_data[i].subslice_size;
        }
        for (i = range_list[dimensionality-1].lower;
             i < (idl_ulong_int)range_list[dimensionality-1].upper;
             i++)
        {
            if (base_type == IDL_DT_FIXED_STRUCT)
            {
                rpc_ss_ndr_u_struct_pointees(base_type, element_defn_index,
                                            inner_slice_address, NULL, IDL_msp);
                inner_slice_address += element_size;
            }
            else if (base_type == IDL_DT_ENC_UNION)
            {
                rpc_ss_ndr_u_enc_union_or_ptees(inner_slice_address,
                                                element_defn_index,
                                                idl_true, IDL_msp);
                inner_slice_address += element_size;
            }
            else
            {
                rpc_ss_ndr_unmar_pointee_desc(base_type, defn_vec_ptr,
                                          &pointee_desc,
                                          (rpc_void_p_t *)inner_slice_address,
                                                                      IDL_msp );
                rpc_ss_ndr_unmar_pointee(base_type, defn_vec_ptr, &pointee_desc,
                                          (rpc_void_p_t *)inner_slice_address,
                                                                      IDL_msp );
                inner_slice_address += sizeof(rpc_void_p_t *);
            }
        }
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

    rpc_ss_ndr_u_rlse_pointee_desc(&pointee_desc, IDL_msp);
    rpc_ss_mem_item_free(&IDL_msp->IDL_mem_handle, (byte_p_t)control_data);
}

/******************************************************************************/
/*                                                                            */
/*  Allocate the target of a [ref] pointer                                    */
/*                                                                            */
/******************************************************************************/
void rpc_ss_alloc_pointer_target
#ifdef IDL_PROTOTYPES
(
    /* [in] */ idl_byte *defn_vec_ptr,  /* On entry points at pointee type */
    /* [out] */ rpc_void_p_t *p_pointer,
                      /* Exit - points to the address of the pointee */
    IDL_msp_t IDL_msp
)
#else
( defn_vec_ptr, p_pointer, IDL_msp )
    idl_byte *defn_vec_ptr;
    rpc_void_p_t *p_pointer;
    IDL_msp_t IDL_msp;
#endif
{
    idl_ulong_int node_size;
    idl_byte pointee_type;
    idl_boolean type_has_pointers;
    idl_ulong_int pointee_defn_index;

    pointee_type = *defn_vec_ptr;
    if ( (pointee_type == IDL_DT_CONF_STRUCT)
        || (pointee_type == IDL_DT_STRING)
        || (pointee_type == IDL_DT_CONF_ARRAY)
        || (pointee_type == IDL_DT_OPEN_ARRAY) )
    {
        *p_pointer = IDL_NEW_NODE;  /* Must allocate when Z-values available */
        return;
    }
    else
        node_size = rpc_ss_type_size(defn_vec_ptr, IDL_msp);

    rpc_ss_ndr_alloc_storage( node_size, 0, NULL, NULL, p_pointer, IDL_msp );

    switch(pointee_type)
    {
        case IDL_DT_BYTE:
        case IDL_DT_CHAR:
        case IDL_DT_BOOLEAN:
        case IDL_DT_DOUBLE:
        case IDL_DT_ENUM:
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
        case IDL_DT_FIXED_STRUCT:
        case IDL_DT_FIXED_ARRAY:
            /* Properties byte */
            defn_vec_ptr++;
            type_has_pointers = IDL_PROP_TEST(*defn_vec_ptr, IDL_PROP_HAS_PTRS);
            if (type_has_pointers)
            {
                defn_vec_ptr++;
                IDL_GET_LONG_FROM_VECTOR(pointee_defn_index,defn_vec_ptr);
                rpc_ss_init_new_store_ptrs(pointee_type,
                                       IDL_msp->IDL_type_vec+pointee_defn_index,
                                       *p_pointer, NULL,
                                       IDL_msp);
            }
            break;
        case IDL_DT_ENC_UNION:
        case IDL_DT_FULL_PTR:
        case IDL_DT_TRANSMIT_AS:
        case IDL_DT_REPRESENT_AS:
#ifndef _KERNEL
        case IDL_DT_CS_TYPE:
        case IDL_DT_CS_ARRAY:
#endif
            break;
#ifndef _KERNEL
        case IDL_DT_UNIQUE_PTR:
#endif
        case IDL_DT_REF_PTR:
            rpc_ss_init_new_store_ptrs(pointee_type,
                                       defn_vec_ptr+1,
                                       *p_pointer, NULL,
                                       IDL_msp);
            break;
        default:
#ifdef DEBUG_INTERP
             printf("rpc_ss_alloc_pointer_target:unrecognized type %d\n",
                        pointee_type);
             exit(0);
#endif
             RAISE(rpc_x_coding_error);
    }
}

/******************************************************************************/
/*                                                                            */
/*  Initialize the [ref] and [unique] pointers in structure not allocated by  */
/*      the application                                                       */
/*                                                                            */
/******************************************************************************/
void rpc_ss_init_new_struct_ptrs
#ifdef IDL_PROTOTYPES
(
    /* [in] */  idl_byte struct_type,   /* DT_FIXED_STRUCT or DT_CONF_STRUCT */
    /* [in] */  idl_byte *defn_vec_ptr, /* Points at structure definition */
    /* [in] */  rpc_void_p_t struct_addr,
    /* [in] */  idl_ulong_int *conf_Z_values,   /* If conformant structure, the
                 Z values for the conformant or open field, otherwise ignored */
    IDL_msp_t IDL_msp
)
#else
(struct_type, defn_vec_ptr, struct_addr, conf_Z_values, IDL_msp)
    idl_byte struct_type;
    idl_byte *defn_vec_ptr;
    rpc_void_p_t struct_addr;
    idl_ulong_int *conf_Z_values;
    IDL_msp_t IDL_msp;
#endif
{
    idl_ulong_int offset_index;
    idl_ulong_int *struct_offset_vec_ptr; /* Start of offsets for this struct */
    idl_ulong_int *offset_vec_ptr;
    idl_byte type_byte;
    idl_ulong_int offset;
    idl_ulong_int array_defn_index;
    idl_byte *array_defn_ptr;
    idl_boolean type_has_pointers;
    idl_ulong_int pointee_defn_index;
    IDL_bound_pair_t *bounds_list;
    idl_ulong_int *Z_values;
    idl_ulong_int array_dims;    /* Number of dimensions of array */

    IDL_GET_LONG_FROM_VECTOR(offset_index,defn_vec_ptr);
    struct_offset_vec_ptr = IDL_msp->IDL_offset_vec + offset_index;

    if (struct_type == IDL_DT_CONF_STRUCT)
    {
        IDL_DISCARD_LONG_FROM_VECTOR(defn_vec_ptr);
    }

    offset_vec_ptr = struct_offset_vec_ptr + 1;
                                        /* Skip over size at start of offsets */
    /* Loop over the fields of the structure */
    do {
        type_byte = *defn_vec_ptr;
        defn_vec_ptr++;
        switch(type_byte)
        {
            /* For fields that do not include pointers, just advance the
                definition and offset pointers in step */
            case IDL_DT_BYTE:
            case IDL_DT_CHAR:
            case IDL_DT_BOOLEAN:
            case IDL_DT_DOUBLE:
            case IDL_DT_ENUM:
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
#endif
            case IDL_DT_IGNORE:
#ifndef _KERNEL
            case IDL_DT_V1_ENUM:
#endif
            case IDL_DT_ERROR_STATUS:
                offset_vec_ptr++;
                break;
            case IDL_DT_FIXED_ARRAY:
                /* Properties byte */
                type_has_pointers =
                             IDL_PROP_TEST(*defn_vec_ptr, IDL_PROP_HAS_PTRS);
                defn_vec_ptr++;
                IDL_DISCARD_LONG_FROM_VECTOR(defn_vec_ptr);
                                                    /* Full array definition */
                IDL_GET_LONG_FROM_VECTOR(array_defn_index, defn_vec_ptr);
                if (type_has_pointers)
                {
                    offset = *offset_vec_ptr;
                    array_defn_ptr = IDL_msp->IDL_type_vec + array_defn_index; 
                    array_dims = (idl_ulong_int)*array_defn_ptr;
                    array_defn_ptr++;
		    if (IDL_msp->IDL_type_vec[TVEC_INT_REP_OFFSET] != NDR_LOCAL_INT_REP)
		        rpc_ss_fixed_bounds_from_vector(array_dims,
							array_defn_ptr, 
							&bounds_list, IDL_msp);
		    else
		      bounds_list = (IDL_bound_pair_t *)array_defn_ptr;
                    Z_values = NULL;
                    rpc_ss_Z_values_from_bounds( bounds_list, array_dims,
                                                 &Z_values, IDL_msp );
                    /* Advance definition pointer over bounds */
                    array_defn_ptr += array_dims * IDL_FIXED_BOUND_PAIR_WIDTH;
                    rpc_ss_init_new_array_ptrs( array_dims, Z_values,
                            array_defn_ptr,
                            (rpc_void_p_t)((idl_byte *)struct_addr+offset),
                            IDL_msp );
                    rpc_ss_mem_item_free(&IDL_msp->IDL_mem_handle,
                                         (byte_p_t)Z_values);
		    if (IDL_msp->IDL_type_vec[TVEC_INT_REP_OFFSET] != NDR_LOCAL_INT_REP)
                    rpc_ss_mem_item_free(&IDL_msp->IDL_mem_handle,
					 (byte_p_t)bounds_list);
                }
                offset_vec_ptr++;
                break;
            case IDL_DT_VARYING_ARRAY:
                /* Properties byte */
                type_has_pointers =
                             IDL_PROP_TEST(*defn_vec_ptr, IDL_PROP_HAS_PTRS);
                defn_vec_ptr++;
                IDL_DISCARD_LONG_FROM_VECTOR(defn_vec_ptr);
                                                    /* Full array definition */
                IDL_GET_LONG_FROM_VECTOR(array_defn_index, defn_vec_ptr);
                if (type_has_pointers)
                {
                    offset = *offset_vec_ptr;
                    array_defn_ptr = IDL_msp->IDL_type_vec + array_defn_index; 
                    array_dims = (idl_ulong_int)*array_defn_ptr;
                    array_defn_ptr++;
		    if (IDL_msp->IDL_type_vec[TVEC_INT_REP_OFFSET] != NDR_LOCAL_INT_REP)
		        rpc_ss_fixed_bounds_from_vector(array_dims,
							array_defn_ptr, 
							&bounds_list, IDL_msp);
		    else
		      bounds_list = (IDL_bound_pair_t *)array_defn_ptr;
                    Z_values = NULL;
                    rpc_ss_Z_values_from_bounds( bounds_list, array_dims,
                                                 &Z_values, IDL_msp );
                    /* Advance definition pointer over bounds */
                    array_defn_ptr += 
                                array_dims * (IDL_FIXED_BOUND_PAIR_WIDTH
                                                 + IDL_DATA_LIMIT_PAIR_WIDTH);
                    rpc_ss_init_new_array_ptrs( array_dims, Z_values,
                            array_defn_ptr,
                            (rpc_void_p_t)((idl_byte *)struct_addr+offset),
                            IDL_msp );
                    rpc_ss_mem_item_free(&IDL_msp->IDL_mem_handle,
                                            (byte_p_t)Z_values);
		    if (IDL_msp->IDL_type_vec[TVEC_INT_REP_OFFSET] != NDR_LOCAL_INT_REP)
		      rpc_ss_mem_item_free(&IDL_msp->IDL_mem_handle,
					   (byte_p_t)bounds_list);
                }                                   
                offset_vec_ptr++;
                break;
            case IDL_DT_CONF_ARRAY:
                /* Properties byte */
                type_has_pointers =
                             IDL_PROP_TEST(*defn_vec_ptr, IDL_PROP_HAS_PTRS);
                defn_vec_ptr++;
                IDL_DISCARD_LONG_FROM_VECTOR(defn_vec_ptr);
                                                    /* Full array definition */
                IDL_GET_LONG_FROM_VECTOR(array_defn_index, defn_vec_ptr);
                if (type_has_pointers)
                {
                    /* Conformant array must be last field of struct */
                    offset = *offset_vec_ptr;
                    array_defn_ptr = IDL_msp->IDL_type_vec + array_defn_index;
                    array_dims = (idl_ulong_int)*array_defn_ptr;
                    array_defn_ptr++;   /* Now at (0 mod 4) */
                    IDL_ADV_DEFN_PTR_OVER_BOUNDS( array_defn_ptr, array_dims );
                    rpc_ss_init_new_array_ptrs( array_dims, conf_Z_values,
                            array_defn_ptr,
                            (rpc_void_p_t)((idl_byte *)struct_addr+offset),
                            IDL_msp );
                }
                break;
            case IDL_DT_OPEN_ARRAY:
                /* Properties byte */
                type_has_pointers =
                             IDL_PROP_TEST(*defn_vec_ptr, IDL_PROP_HAS_PTRS);
                defn_vec_ptr++;
                IDL_DISCARD_LONG_FROM_VECTOR(defn_vec_ptr);
                                                    /* Full array definition */
                IDL_GET_LONG_FROM_VECTOR(array_defn_index, defn_vec_ptr);
                if (type_has_pointers)
                {
                    /* Open array must be last field of struct */
                    offset = *offset_vec_ptr;
                    array_defn_ptr = IDL_msp->IDL_type_vec + array_defn_index;
                    array_dims = (idl_ulong_int)*array_defn_ptr;
                    array_defn_ptr++;   /* Now at (0 mod 4) */
                    IDL_ADV_DEFN_PTR_OVER_BOUNDS( array_defn_ptr, array_dims );
                    array_defn_ptr += array_dims * IDL_DATA_LIMIT_PAIR_WIDTH;
                    rpc_ss_init_new_array_ptrs( array_dims, conf_Z_values,
                            array_defn_ptr,
                            (rpc_void_p_t)((idl_byte *)struct_addr+offset),
                            IDL_msp );
                }
                break;
#ifndef _KERNEL
            case IDL_DT_N_E_UNION:
                defn_vec_ptr++;     /* Discard properties byte */
                IDL_DISCARD_LONG_FROM_VECTOR(defn_vec_ptr);  /* Switch index */
                IDL_DISCARD_LONG_FROM_VECTOR(defn_vec_ptr);  /* Defn index */
                offset_vec_ptr++;
                break;
#endif
            case IDL_DT_ENC_UNION:
            case IDL_DT_FULL_PTR:
            case IDL_DT_TRANSMIT_AS:
            case IDL_DT_REPRESENT_AS:
#ifndef _KERNEL
            case IDL_DT_CS_TYPE:
#endif
                defn_vec_ptr++;     /* Discard properties byte */
                IDL_DISCARD_LONG_FROM_VECTOR(defn_vec_ptr);  /* Defn index */
                offset_vec_ptr++;
                break;
#ifndef _KERNEL
            case IDL_DT_UNIQUE_PTR:
                defn_vec_ptr++;     /* Discard properties byte */
                IDL_DISCARD_LONG_FROM_VECTOR(defn_vec_ptr);  /* Pointee defn */
                offset = *offset_vec_ptr;
                offset_vec_ptr++;
                *(rpc_void_p_t *)((idl_byte *)struct_addr+offset) = NULL;
                break;
#endif
            case IDL_DT_REF_PTR:
                defn_vec_ptr++;     /* Discard properties byte */
                IDL_GET_LONG_FROM_VECTOR(pointee_defn_index, defn_vec_ptr);
                offset = *offset_vec_ptr;
                offset_vec_ptr++;
                rpc_ss_alloc_pointer_target(
                               IDL_msp->IDL_type_vec+pointee_defn_index,
                               (rpc_void_p_t *)((idl_byte *)struct_addr+offset),
                               IDL_msp );
                break;
#ifndef _KERNEL
            case IDL_DT_CS_SHADOW:
                IDL_DISCARD_LONG_FROM_VECTOR(defn_vec_ptr); /* Shadow length */
                break;
#endif
            case IDL_DT_STRING:
            case IDL_DT_NDR_ALIGN_2:
            case IDL_DT_NDR_ALIGN_4:
            case IDL_DT_NDR_ALIGN_8:
            case IDL_DT_BEGIN_NESTED_STRUCT:
            case IDL_DT_END_NESTED_STRUCT:
#ifndef _KERNEL
            case IDL_DT_CS_ATTRIBUTE:
            case IDL_DT_CS_ARRAY:
            case IDL_DT_CS_RLSE_SHADOW:
            case IDL_DT_V1_ARRAY:
            case IDL_DT_V1_STRING:
#endif
            case IDL_DT_EOL:
                break;
            default:
#ifdef DEBUG_INTERP
                printf("rpc_ss_init_new_struct_ptrs:unrecognized type %d\n",
                        type_byte);
                exit(0);
#endif
                RAISE(rpc_x_coding_error);
        }
    } while (type_byte != IDL_DT_EOL);

}

/******************************************************************************/
/*                                                                            */
/*  Initialize the [ref] and [unique] pointers in arrays not allocated by     */
/*      the application                                                       */
/*                                                                            */
/******************************************************************************/
void rpc_ss_init_new_array_ptrs
#ifdef IDL_PROTOTYPES
(
    /* [in] */  idl_ulong_int dimensionality,
    /* [in] */  idl_ulong_int *Z_values,
    /* [in] */  idl_byte *defn_vec_ptr, /* On entry points at array base info */
    /* [in] */  rpc_void_p_t array_addr,
    IDL_msp_t IDL_msp
)
#else
( dimensionality, Z_values, defn_vec_ptr, array_addr, IDL_msp )
    idl_ulong_int dimensionality;
    idl_ulong_int *Z_values;
    idl_byte *defn_vec_ptr;
    rpc_void_p_t array_addr;
    IDL_msp_t IDL_msp;
#endif
{
    idl_ulong_int element_count;
    idl_ulong_int i;
    idl_byte base_type;
    idl_ulong_int element_size;
    idl_ulong_int struct_defn_index;
    idl_ulong_int struct_offset_index;
    idl_byte *struct_defn_ptr;
    rpc_void_p_t *array_elt_addr;

    element_count = 1;
    for (i=0; i<dimensionality; i++)
    {
        element_count *= Z_values[i];
    }

    base_type = *defn_vec_ptr;
    defn_vec_ptr++;
    if (base_type == IDL_DT_FIXED_STRUCT)
    {
        /* Array of structures containing pointers */
        IDL_GET_LONG_FROM_VECTOR(struct_defn_index, defn_vec_ptr);
        struct_defn_ptr = IDL_msp->IDL_type_vec + struct_defn_index;
        IDL_GET_LONG_FROM_VECTOR(struct_offset_index, struct_defn_ptr);
        element_size = *(IDL_msp->IDL_offset_vec + struct_offset_index);
        /* Reset pointer to start of structure definition */
        struct_defn_ptr = IDL_msp->IDL_type_vec + struct_defn_index;
        for (i=0; i<element_count; i++)
        {
            rpc_ss_init_new_struct_ptrs( base_type, struct_defn_ptr,
                                            array_addr, NULL, IDL_msp );
            array_addr = (rpc_void_p_t)((idl_byte *)array_addr + element_size);
        }
    }
    else if ( (base_type != IDL_DT_FULL_PTR)
                && (base_type != IDL_DT_ENC_UNION) )
    {
        /* Array of [ref] or [unique] pointers */
        array_elt_addr = (rpc_void_p_t *)(array_addr);
        for (i=0; i<element_count; i++)
        {
            rpc_ss_init_new_store_ptrs( base_type, defn_vec_ptr,
                                         array_elt_addr, NULL, IDL_msp );
            array_elt_addr++;
        }
    }
}

/******************************************************************************/
/*                                                                            */
/*  Initialize the [ref] and [unique] pointers in storage not allocated by    */
/*      the application                                                       */
/*                                                                            */
/******************************************************************************/
void rpc_ss_init_new_store_ptrs
#ifdef IDL_PROTOTYPES
(
    /* [in] */ idl_byte storage_type,      /* Data type of allocated storage */
    /* [in] */ idl_byte *defn_vec_ptr,
                    /* For pointers - points to pointee definition */
                    /* For struct - points to structure definition */
                    /* For array - points to array definition */
    /* [in] */ rpc_void_p_t storage_addr,   /* Address of allocated storage */
    /* [in] */ idl_ulong_int *conf_Z_values,
                    /* Conformance information for conformant type, else NULL */
    IDL_msp_t IDL_msp
)
#else
( storage_type, defn_vec_ptr, storage_addr, conf_Z_values, IDL_msp )
    idl_byte storage_type;
    idl_byte *defn_vec_ptr;
    rpc_void_p_t storage_addr;
    idl_ulong_int *conf_Z_values;
    IDL_msp_t IDL_msp;
#endif
{
    idl_ulong_int dimensionality;
    idl_ulong_int *Z_values;
    IDL_bound_pair_t *bounds_list;

    switch (storage_type)
    {
        case IDL_DT_FIXED_STRUCT:
        case IDL_DT_CONF_STRUCT:
            rpc_ss_init_new_struct_ptrs( storage_type, defn_vec_ptr,
                                         storage_addr, conf_Z_values, IDL_msp );
            break;
        case IDL_DT_FIXED_ARRAY:
        case IDL_DT_VARYING_ARRAY:
            dimensionality = (idl_ulong_int)*defn_vec_ptr;
            defn_vec_ptr++;
	    if (IDL_msp->IDL_type_vec[TVEC_INT_REP_OFFSET] != NDR_LOCAL_INT_REP)
	        rpc_ss_fixed_bounds_from_vector(dimensionality, defn_vec_ptr,
						&bounds_list, IDL_msp);
	    else
	      bounds_list = (IDL_bound_pair_t *)defn_vec_ptr;
            Z_values = NULL;
            rpc_ss_Z_values_from_bounds( bounds_list,
                                           dimensionality, &Z_values, IDL_msp );
            defn_vec_ptr += dimensionality * IDL_FIXED_BOUND_PAIR_WIDTH;
            if (storage_type == IDL_DT_VARYING_ARRAY)
                defn_vec_ptr += dimensionality * IDL_DATA_LIMIT_PAIR_WIDTH;
            rpc_ss_init_new_array_ptrs( dimensionality, Z_values, defn_vec_ptr,
                storage_addr, IDL_msp );
            rpc_ss_mem_item_free(&IDL_msp->IDL_mem_handle, (byte_p_t)Z_values);
	    if (IDL_msp->IDL_type_vec[TVEC_INT_REP_OFFSET] != NDR_LOCAL_INT_REP)
	      rpc_ss_mem_item_free(&IDL_msp->IDL_mem_handle,
				   (byte_p_t)bounds_list);
            break;
#ifndef _KERNEL
        case IDL_DT_UNIQUE_PTR:
            *(rpc_void_p_t *)storage_addr = NULL;
            break;
#endif
        case IDL_DT_REF_PTR:
            rpc_ss_alloc_pointer_target( defn_vec_ptr,
                                      (rpc_void_p_t *)storage_addr, IDL_msp );
            break;
        default:
#ifdef DEBUG_INTERP
            printf("rpc_ss_init_new_store_ptrs:unrecognized type %d\n",
                        storage_type);
            exit(0);
#endif
            RAISE(rpc_x_coding_error);
    }
}

/******************************************************************************/
/*                                                                            */
/*  Initialize the [ref] pointers in [out] only parameters                    */
/*                                                                            */
/******************************************************************************/
void rpc_ss_init_out_ref_ptrs
#ifdef IDL_PROTOTYPES
(
    /* [in,out] */ idl_byte **p_type_vec_ptr,
                            /* On entry type_vec_ptr points to type of parameter
                                         It is advanced over type definition */
    /* [in] */ rpc_void_p_t param_addr,
    IDL_msp_t IDL_msp
)
#else
( p_type_vec_ptr, param_addr, IDL_msp )
    idl_byte **p_type_vec_ptr;
    rpc_void_p_t param_addr;
    IDL_msp_t IDL_msp;
#endif
{
    idl_byte type_byte;
    idl_byte *type_vec_ptr = *p_type_vec_ptr;
    idl_ulong_int defn_index;

    type_byte = *type_vec_ptr;
    type_vec_ptr++;
    switch (type_byte)
    {
        case IDL_DT_FIXED_STRUCT:
            type_vec_ptr++;     /* Ignore properties byte */
            IDL_GET_LONG_FROM_VECTOR( defn_index, type_vec_ptr );
            break;
        case IDL_DT_FIXED_ARRAY:
        case IDL_DT_VARYING_ARRAY:
            type_vec_ptr++;     /* Ignore properties byte */
            IDL_DISCARD_LONG_FROM_VECTOR( type_vec_ptr );  /* Full array defn */
            IDL_GET_LONG_FROM_VECTOR( defn_index, type_vec_ptr );
            break;
        case IDL_DT_REF_PTR:
            type_vec_ptr++;     /* Ignore properties byte */
            IDL_GET_LONG_FROM_VECTOR( defn_index, type_vec_ptr );
            break;
        default:
#ifdef DEBUG_INTERP
            printf("rpc_ss_init_out_ref_ptrs:unrecognized type %d\n",
                        type_byte);
            exit(0);
#endif
            RAISE(rpc_x_coding_error);
    }
    rpc_ss_init_new_store_ptrs( type_byte, IDL_msp->IDL_type_vec+defn_index,
                                param_addr, NULL, IDL_msp );

    *p_type_vec_ptr = type_vec_ptr;
}

/******************************************************************************/
/*                                                                            */
/*  Find out if a pointee to be unmarshalled is a non-fixed array or a        */
/*  conformant struct.  If so, check if pointee already unmarshalled and      */
/*  save this in p_pointee_desc->already_unmarshalled.  For the array case,   */
/*  build description of the bounds and, if necessary, data limits.           */
/*                                                                            */
/*  p_pointee_desc->dimensionality must be set on entry for the array case.   */
/*      0 indicates that no storage for array pointee data has been           */
/*      allocated.  If the pointee is an array with more dimensions than      */
/*      signified by this field, the current array pointee data storage is    */
/*      released and new storage allocated.                                   */
/*                                                                            */
/******************************************************************************/
void rpc_ss_ndr_unmar_pointee_desc
#ifdef IDL_PROTOTYPES
(
    /* [in] */ idl_byte pointer_type,   /* [ptr], [unique] or [ref] */
    /* [in] */  idl_byte *defn_vec_ptr, /* Points at definition of pointee */
    /* [out] */ IDL_pointee_desc_t *p_pointee_desc, /* Pointer to data structure
                                     to be filled in with pointee description */
    /* [in,out] */ rpc_void_p_t *p_pointer,
                    /* Entry - for a full pointer points to a node number
                       Exit - for a full pointer, if pointee has already been
                              unmarshalled, points to the address of the pointee
                       Not used for non-full pointer */
    IDL_msp_t IDL_msp
)
#else
( pointer_type, defn_vec_ptr, p_pointee_desc, p_pointer, IDL_msp )
    idl_byte pointer_type;
    idl_byte *defn_vec_ptr;
    IDL_pointee_desc_t *p_pointee_desc;
    rpc_void_p_t *p_pointer;
    IDL_msp_t IDL_msp;
#endif
{
    idl_ulong_int array_defn_index;
    idl_byte *array_defn_ptr;
    IDL_bound_pair_t *bounds_list;
    idl_ulong_int dimensionality;
    rpc_void_p_t p_node;
    long already_unmarshalled;

    if (*defn_vec_ptr == IDL_DT_STRING)
        defn_vec_ptr++;
    p_pointee_desc->pointee_type = *defn_vec_ptr;
    if ( (p_pointee_desc->pointee_type != IDL_DT_VARYING_ARRAY)
        && (p_pointee_desc->pointee_type != IDL_DT_CONF_ARRAY)
        && (p_pointee_desc->pointee_type != IDL_DT_OPEN_ARRAY)
        && (p_pointee_desc->pointee_type != IDL_DT_CONF_STRUCT) )
    {
        return;
    }

    /*
     * The pointee type is a non-fixed array or a conformant struct.  See if
     * the pointer is either NULL or the pointee has already been unmarshalled
     * so we know whether to unmarshall the Z and/or AB values for the pointee
     * construct.  Save the already unmarshalled flag for subsequent routine.
     */
    if (pointer_type == IDL_DT_FULL_PTR)
    {
        if (*p_pointer == NULL)
            return;
        p_node = (rpc_void_p_t)rpc_ss_inquire_pointer_to_node(
                                                    IDL_msp->IDL_node_table,
#ifdef SGIMIPS
                                                    (idl_ulong_int)(__psint_t)*p_pointer,
#else
                                                    *(idl_ulong_int * )p_pointer,
#endif /* SGIMIPS */ 
                                                    &already_unmarshalled);
        p_pointee_desc->already_unmarshalled =(idl_boolean)already_unmarshalled;
        if (p_pointee_desc->already_unmarshalled)
        {
            *(rpc_void_p_t *)p_pointer = p_node;
            return;
        }
    }
    /* Conformant struct Z value is unmarshalled in rpc_ss_ndr_unmar_pointee */
    if (p_pointee_desc->pointee_type == IDL_DT_CONF_STRUCT)
        return;

    defn_vec_ptr++;
    p_pointee_desc->base_type_has_pointers =
                            IDL_PROP_TEST( *defn_vec_ptr, IDL_PROP_HAS_PTRS );
    defn_vec_ptr++;
    IDL_DISCARD_LONG_FROM_VECTOR( defn_vec_ptr );   /* Full array definition */
    IDL_GET_LONG_FROM_VECTOR( array_defn_index, defn_vec_ptr );
    array_defn_ptr = IDL_msp->IDL_type_vec + array_defn_index;
    dimensionality = (idl_ulong_int)*array_defn_ptr;
    array_defn_ptr++;

    /* Minimize the number of malloc's used to build the data structure */
    /* In code that follows, range list  will be allocated
        out of this memory, if necessary */
    if (dimensionality > p_pointee_desc->dimensionality)
    {
        if (p_pointee_desc->dimensionality > 0)
        {
            /* Some array description storage already exists, release it */
            rpc_ss_mem_item_free(&IDL_msp->IDL_mem_handle,
                                            (byte_p_t)p_pointee_desc->Z_values);
        }    
        p_pointee_desc->Z_values = (idl_ulong_int *) rpc_ss_mem_alloc(
                        &IDL_msp->IDL_mem_handle,
                         dimensionality
                           * ( sizeof(idl_ulong_int) /* Z_values */
                               + sizeof(IDL_bound_pair_t) /*  range list */ ) );
    }
    p_pointee_desc->dimensionality = dimensionality;

    switch(p_pointee_desc->pointee_type)
    {
        case IDL_DT_VARYING_ARRAY:
            /* Build the range list in store allocated for the pointee desc */
            p_pointee_desc->range_list =  (IDL_bound_pair_t *)
                    (p_pointee_desc->Z_values + p_pointee_desc->dimensionality);
	    if (IDL_msp->IDL_type_vec[TVEC_INT_REP_OFFSET] != NDR_LOCAL_INT_REP)
	      rpc_ss_fixed_bounds_from_vector(p_pointee_desc->dimensionality,
                                         array_defn_ptr, &bounds_list, IDL_msp);
	    else
	      bounds_list = (IDL_bound_pair_t *)array_defn_ptr;
            rpc_ss_Z_values_from_bounds( bounds_list,
                                         p_pointee_desc->dimensionality,
                                         &(p_pointee_desc->Z_values),
                                         IDL_msp );
	    if (IDL_msp->IDL_type_vec[TVEC_INT_REP_OFFSET] != NDR_LOCAL_INT_REP)
	      rpc_ss_mem_item_free(&IDL_msp->IDL_mem_handle,
				   (byte_p_t)bounds_list);
            array_defn_ptr += p_pointee_desc->dimensionality
                                    * (IDL_FIXED_BOUND_PAIR_WIDTH
                                        + IDL_DATA_LIMIT_PAIR_WIDTH);
            rpc_ss_ndr_unmar_range_list( p_pointee_desc->dimensionality,
                                     *array_defn_ptr,
                                     &p_pointee_desc->range_list, IDL_msp );
            break;
        case IDL_DT_CONF_ARRAY:
            rpc_ss_ndr_unmar_Z_values( p_pointee_desc->dimensionality,
                                     &p_pointee_desc->Z_values, IDL_msp );
            IDL_ADV_DEFN_PTR_OVER_BOUNDS( array_defn_ptr,
                                            p_pointee_desc->dimensionality );
            break;
        case IDL_DT_OPEN_ARRAY:
            /* Build the range list in store allocated for the pointee desc */
            p_pointee_desc->range_list =  (IDL_bound_pair_t *)
                    (p_pointee_desc->Z_values + p_pointee_desc->dimensionality);
            rpc_ss_ndr_unmar_Z_values( p_pointee_desc->dimensionality,
                                     &p_pointee_desc->Z_values, IDL_msp );
            IDL_ADV_DEFN_PTR_OVER_BOUNDS( array_defn_ptr,
                                            p_pointee_desc->dimensionality );
            array_defn_ptr += p_pointee_desc->dimensionality
                                    * IDL_DATA_LIMIT_PAIR_WIDTH;
            rpc_ss_ndr_unmar_range_list( p_pointee_desc->dimensionality,
                                     *array_defn_ptr,
                                     &p_pointee_desc->range_list, IDL_msp );
            break;
        default:
            RAISE( rpc_x_coding_error );
    }
    p_pointee_desc->array_base_defn_ptr = array_defn_ptr;
}
