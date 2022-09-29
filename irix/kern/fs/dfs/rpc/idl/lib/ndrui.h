/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1996 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: ndrui.h,v $
 * Revision 65.1  1997/10/20 19:15:39  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.8.2  1996/02/18  23:45:50  marty
 * 	Update OSF copyright years
 * 	[1996/02/18  22:44:38  marty]
 *
 * Revision 1.1.8.1  1995/12/07  22:30:35  root
 * 	Submit OSF/DCE 1.2.1
 * 
 * 	HP revision /main/HPDCE02/jrr_1.2_mothra/1  1995/11/20  23:19 UTC  jrr
 * 	Merge second XIDL drop for DCE 1.2.1
 * 	[1995/11/17  17:07 UTC  dat  /main/dat_xidl2/1]
 * 
 * 	oct 95 idl drop
 * 	[1995/10/22  23:36:13  bfc]
 * 	 *
 * 	may 95 idl drop
 * 	[1995/10/22  22:57:50  bfc]
 * 	 *
 * 	DCE for DEC OSF/1: populate from OSF DCE R1.1
 * 	[1995/10/21  18:25:54  bfc]
 * 	 *
 * 
 * 	HP revision /main/HPDCE02/1  1994/08/03  16:32 UTC  tatsu_s
 * 	Merged mothra up to dce 1.1.
 * 	[1995/12/07  21:16:04  root]
 * 
 * Revision 1.1.4.3  1994/05/11  17:37:38  rico
 * 	Fixes for [string] and [ptr] interactions.
 * 	[1994/05/10  14:21:27  rico]
 * 	HP revision /main/HPDCE01/2  1994/05/26  20:46 UTC  gaz
 * 	Merge IDL performance changes
 * 
 * 	HP revision /main/HPDCE01/gaz_kk_idl_merge/1  1994/05/06  19:35 UTC  gaz
 * 	Merge IDL performance enhancements with mainline
 * 
 * 	HP revision /main/HPDCE01/ajayb_idl/1  1994/04/12  18:55 UTC  ajayb
 * 	Malloc and full ptr optimizations
 * 
 * 	HP revision /main/HPDCE01/1  1994/01/21  22:48  lmm
 * 	merge kk and hpdce01
 * 
 * Revision 1.1.5.2  1993/11/11  15:35:04  tatsu_s
 * 	Fix OT7465.
 * 	In rpc_ss_ndr_unmar_check_buffer(), nullify
 * 	IDL_msp->IDL_elt_p->buff_dealloc after its use.
 * 	[1993/11/11  15:33:53  tatsu_s]
 * 
 * Revision 1.1.4.2  1994/02/18  13:09:49  rico
 * 	Fix for potential infinite loop when stub tries to unmarshall data that has
 * 	not been sent - CR 9391.
 * 	[1994/02/17  12:44:11  rico]
 * 
 * Revision 1.1.4.1  1994/01/21  22:30:41  cbrooks
 * 	RPC Code Cleanup - Initial Submission
 * 	[1994/01/21  21:51:24  cbrooks]
 * 
 * Revision 1.1.2.2  1993/07/07  20:09:48  ganni
 * 	reduced stub idl sources
 * 	[1993/07/07  19:38:43  ganni]
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
**      ndrui.h
**
**  FACILITY:
**
**      Interface Definition Language (IDL) Compiler
**
**  ABSTRACT:
**
**      Macros and prototypes for routines shared between ndrui*.c modules
**
*/

/******************************************************************************/
/*                                                                            */
/*  Check whether there is an item to unmarshall in the existing buffer       */
/*  If we are correctly aligned, there is unless we are at buffer end         */
/*  If not, release the current buffer and get a new buffer                   */
/*                                                                            */
/*  Note. IDL_msp->IDL_elt_p->buff_dealloc = 0; before rpc_call_receive       */
/*  because DG runtime does not do this if receive fails                      */
/*                                                                            */
/******************************************************************************/
#ifdef _KERNEL
#define rpc_ss_ndr_unmar_check_buffer( IDL_msp ) \
{ \
    if (IDL_msp->IDL_left_in_buff == 0) \
    { \
        if (IDL_msp->IDL_elt_p->buff_dealloc \
                    && IDL_msp->IDL_elt_p->data_len != 0) \
        { \
           (*(IDL_msp->IDL_elt_p->buff_dealloc))(IDL_msp->IDL_elt_p->buff_addr); \
            IDL_msp->IDL_elt_p->buff_dealloc = 0; \
        } \
        rpc_call_receive( (rpc_call_handle_t)IDL_msp->IDL_call_h, IDL_msp->IDL_elt_p, \
                            (unsigned32 *)&IDL_msp->IDL_status ); \
        if (IDL_msp->IDL_status != error_status_ok) \
            RAISE( rpc_x_ss_pipe_comm_error ); \
        IDL_msp->IDL_mp = (idl_byte *)IDL_msp->IDL_elt_p->data_addr; \
        if (IDL_msp->IDL_mp == NULL) \
        { \
            IDL_msp->IDL_status = rpc_s_stub_protocol_error; \
            RAISE( rpc_x_ss_pipe_comm_error ); \
        } \
        IDL_msp->IDL_left_in_buff = IDL_msp->IDL_elt_p->data_len; \
    } \
}
#else
#define rpc_ss_ndr_unmar_check_buffer( IDL_msp ) \
{ \
    if (IDL_msp->IDL_left_in_buff == 0) \
    { \
        if (IDL_msp->IDL_pickling_handle != NULL) \
            idl_es_decode_check_buffer(IDL_msp);\
        else \
        { \
            if (IDL_msp->IDL_elt_p->buff_dealloc \
                    && IDL_msp->IDL_elt_p->data_len != 0) \
               (*(IDL_msp->IDL_elt_p->buff_dealloc))(IDL_msp->IDL_elt_p->buff_addr); \
            rpc_call_receive( (rpc_call_handle_t)IDL_msp->IDL_call_h, IDL_msp->IDL_elt_p, \
                            (unsigned32 *)&IDL_msp->IDL_status ); \
            if (IDL_msp->IDL_status != error_status_ok) \
                RAISE( rpc_x_ss_pipe_comm_error ); \
            IDL_msp->IDL_mp = (idl_byte *)IDL_msp->IDL_elt_p->data_addr; \
            if (IDL_msp->IDL_mp == NULL) \
            { \
                IDL_msp->IDL_status = rpc_s_stub_protocol_error; \
                RAISE( rpc_x_ss_pipe_comm_error ); \
            } \
            IDL_msp->IDL_left_in_buff = IDL_msp->IDL_elt_p->data_len; \
        } \
    } \
}
#endif

/******************************************************************************/
/*                                                                            */
/* Unmarshall a scalar                                                        */
/*                                                                            */
/******************************************************************************/

#define IDL_UNMAR_1_BYTE_SCALAR( marshalling_macro, type, param_addr ) \
{ \
    rpc_ss_ndr_unmar_check_buffer( IDL_msp ); \
    marshalling_macro( IDL_msp->IDL_drep, ndr_g_local_drep, \
                        IDL_msp->IDL_mp, *(type *)(param_addr)); \
    IDL_msp->IDL_mp += 1; \
    IDL_msp->IDL_left_in_buff -= 1; \
}


#ifndef _KERNEL
#define IDL_UNMAR_BOOLEAN( param_addr ) \
{ \
        IDL_UNMAR_1_BYTE_SCALAR( rpc_convert_boolean, idl_boolean, param_addr ); \
}
#else
#define IDL_UNMAR_BOOLEAN( param_addr ) \
{ \
        IDL_UNMAR_1_BYTE_SCALAR( rpc_convert_boolean, idl_boolean, param_addr ); \
}
#endif /* _KERNEL */


#define IDL_UNMAR_BYTE( param_addr ) \
    IDL_UNMAR_1_BYTE_SCALAR( rpc_convert_byte, idl_byte, param_addr )

#define IDL_UNMAR_CHAR( param_addr ) \
    IDL_UNMAR_1_BYTE_SCALAR( rpc_convert_char, idl_char, param_addr )

#define IDL_UNMAR_ALIGNED_SCALAR( marshalling_macro, size, type, param_addr ) \
{ \
    IDL_UNMAR_ALIGN_MP( IDL_msp, size ); \
    rpc_ss_ndr_unmar_check_buffer( IDL_msp ); \
    marshalling_macro( IDL_msp->IDL_drep, ndr_g_local_drep, \
                        IDL_msp->IDL_mp, *(type *)(param_addr)); \
    IDL_msp->IDL_mp += size; \
    IDL_msp->IDL_left_in_buff -= size; \
}

#define IDL_UNMAR_DOUBLE( param_addr ) \
    IDL_UNMAR_ALIGNED_SCALAR( rpc_convert_long_float, 8, idl_long_float, param_addr )

#define IDL_UNMAR_ENUM( param_addr ) \
    IDL_UNMAR_ALIGNED_SCALAR( rpc_convert_enum, 2, int, param_addr )

#define IDL_UNMAR_FLOAT( param_addr ) \
    IDL_UNMAR_ALIGNED_SCALAR( rpc_convert_short_float, 4, idl_short_float, param_addr )


#ifndef _KERNEL
#define IDL_UNMAR_SMALL( param_addr ) \
{ \
        IDL_UNMAR_1_BYTE_SCALAR( rpc_convert_small_int, idl_small_int, param_addr ); \
}
#else
#define IDL_UNMAR_SMALL( param_addr ) \
{ \
        IDL_UNMAR_1_BYTE_SCALAR( rpc_convert_small_int, idl_small_int, param_addr ); \
}
#endif /* _KERNEL */


#define IDL_UNMAR_SHORT( param_addr ) \
    IDL_UNMAR_ALIGNED_SCALAR( rpc_convert_short_int, 2, idl_short_int, param_addr )

#define IDL_UNMAR_LONG( param_addr ) \
    IDL_UNMAR_ALIGNED_SCALAR( rpc_convert_long_int, 4, idl_long_int, param_addr )

#define IDL_UNMAR_HYPER( param_addr ) \
    IDL_UNMAR_ALIGNED_SCALAR( rpc_convert_hyper_int, 8, idl_hyper_int, param_addr )


#ifndef _KERNEL
#define IDL_UNMAR_USMALL( param_addr ) \
{ \
        IDL_UNMAR_1_BYTE_SCALAR( rpc_convert_usmall_int, idl_usmall_int, param_addr ); \
}
#else
#define IDL_UNMAR_USMALL( param_addr ) \
{ \
        IDL_UNMAR_1_BYTE_SCALAR( rpc_convert_usmall_int, idl_usmall_int, param_addr ); \
}
#endif /* _KERNEL */


#ifndef _KERNEL
#define IDL_UNMAR_USHORT( param_addr ) \
{ \
        IDL_UNMAR_ALIGNED_SCALAR( rpc_convert_ushort_int, 2, idl_ushort_int, param_addr ); \
}
#else
#define IDL_UNMAR_USHORT( param_addr ) \
{ \
        IDL_UNMAR_ALIGNED_SCALAR( rpc_convert_ushort_int, 2, idl_ushort_int, param_addr ); \
}
#endif /* _KERNEL */



#define IDL_UNMAR_ULONG( param_addr ) \
    IDL_UNMAR_ALIGNED_SCALAR( rpc_convert_ulong_int, 4, idl_ulong_int, param_addr )

#define IDL_UNMAR_UHYPER( param_addr ) \
    IDL_UNMAR_ALIGNED_SCALAR( rpc_convert_uhyper_int, 8, idl_uhyper_int, param_addr )

#define IDL_UNMAR_V1_ENUM( param_addr ) \
    IDL_UNMAR_ALIGNED_SCALAR( rpc_convert_v1_enum, 4, int, param_addr )

#ifdef IDL_ENABLE_STATUS_MAPPING
#define IDL_UNMAR_ERROR_STATUS( param_addr ) \
{ \
    IDL_UNMAR_ALIGN_MP( IDL_msp, 4 ); \
    rpc_ss_ndr_unmar_check_buffer( IDL_msp ); \
    rpc_convert_ulong_int( IDL_msp->IDL_drep, ndr_g_local_drep, \
                        IDL_msp->IDL_mp, *(idl_ulong_int *)(param_addr)); \
    rpc_ss_map_dce_to_local_status((error_status_t *)(param_addr)); \
    IDL_msp->IDL_mp += 4; \
    IDL_msp->IDL_left_in_buff -= 4; \
}
#else
#define IDL_UNMAR_ERROR_STATUS( param_addr ) \
{ \
    IDL_UNMAR_ALIGN_MP( IDL_msp, 4 ); \
    rpc_ss_ndr_unmar_check_buffer( IDL_msp ); \
    rpc_convert_ulong_int( IDL_msp->IDL_drep, ndr_g_local_drep, \
                        IDL_msp->IDL_mp, *(idl_ulong_int *)(param_addr)); \
    IDL_msp->IDL_mp += 4; \
    IDL_msp->IDL_left_in_buff -= 4; \
}
#endif

/* For unmarshalling interpreter internal values, which are always C format */
#define IDL_UNMAR_CUSMALL( param_addr ) \
    IDL_UNMAR_1_BYTE_SCALAR( rpc_convert_usmall_int, idl_usmall_int, param_addr )

#define IDL_UNMAR_CUSHORT( param_addr ) \
    IDL_UNMAR_ALIGNED_SCALAR( rpc_convert_short_int, 2, idl_ushort_int, param_addr )

/* Function prototypes */

void rpc_ss_alloc_out_cs_conf_array
(
#ifdef IDL_PROTOTYPES
    IDL_cs_shadow_elt_t *cs_shadow,
    idl_byte **p_type_vec_ptr,
    rpc_void_p_t *p_array_addr,
    IDL_msp_t IDL_msp
#endif
);

void rpc_ss_alloc_pointer_target
(
#ifdef IDL_PROTOTYPES
    idl_byte *defn_vec_ptr,
    rpc_void_p_t *p_pointer,
    IDL_msp_t IDL_msp
#endif
);

void rpc_ss_init_new_array_ptrs
(
#ifdef IDL_PROTOTYPES
    idl_ulong_int dimensionality,
    idl_ulong_int *Z_values,
    idl_byte *defn_vec_ptr,
    rpc_void_p_t array_addr,
    IDL_msp_t IDL_msp
#endif
);

void rpc_ss_init_new_struct_ptrs
(
#ifdef IDL_PROTOTYPES
    idl_byte struct_type,
    idl_byte *defn_vec_ptr,
    rpc_void_p_t struct_addr,
    idl_ulong_int *conf_Z_values,
    IDL_msp_t IDL_msp
#endif
);

void rpc_ss_init_out_ref_ptrs
(
#ifdef IDL_PROTOTYPES
    idl_byte **p_type_vec_ptr,
    rpc_void_p_t param_addr,
    IDL_msp_t IDL_msp
#endif
);

void rpc_ss_ndr_alloc_storage
(
#ifdef IDL_PROTOTYPES
    idl_ulong_int fixed_part_size,
    idl_ulong_int dimensionality,
    idl_ulong_int *Z_values,
    idl_byte *array_defn_ptr,
    rpc_void_p_t *p_storage_addr,
    IDL_msp_t IDL_msp
#endif
);

idl_ulong_int rpc_ss_ndr_allocation_size
(
#ifdef IDL_PROTOTYPES
    idl_ulong_int fixed_part_size,
    idl_ulong_int dimensionality,
    idl_ulong_int *Z_values,
    idl_byte *array_defn_ptr,
    IDL_msp_t IDL_msp
#endif
);

void rpc_ss_ndr_u_conf_cs_struct_hdr
(
#ifdef IDL_PROTOTYPES
    idl_byte *struct_defn_ptr,
    idl_byte *array_defn_ptr,
    idl_ulong_int *Z_values,
    idl_ulong_int fixed_part_size,
    idl_boolean type_has_pointers,
    idl_ulong_int conf_arr_shadow_index,
    idl_boolean allocate,
    IDL_cs_shadow_elt_t *cs_shadow,
    rpc_void_p_t *p_param_addr,
    IDL_msp_t IDL_msp
#endif
);

void rpc_ss_ndr_u_struct_cs_shadow
(
#ifdef IDL_PROTOTYPES
    rpc_void_p_t struct_addr,
    idl_byte struct_type,
    idl_ulong_int offset_index,
    idl_byte *defn_vec_ptr,
    IDL_cs_shadow_elt_t *cs_shadow,
    IDL_msp_t IDL_msp
#endif
);

void rpc_ss_ndr_u_cs_array_param
(
#ifdef IDL_PROTOTYPES
    idl_byte **p_type_vec_ptr,
    IDL_cs_shadow_elt_t *param_cs_shadow,
    idl_ulong_int param_index,
    IDL_msp_t IDL_msp
#endif
);

void rpc_ss_ndr_u_enc_union_or_ptees
(
#ifdef IDL_PROTOTYPES
    rpc_void_p_t param_addr,
    idl_ulong_int defn_index,
    idl_boolean pointees,
    IDL_msp_t IDL_msp
#endif
);

void rpc_ss_ndr_u_f_or_c_arr_ptees
(
#ifdef IDL_PROTOTYPES
    idl_ulong_int dimensionality,
    idl_ulong_int *Z_values,
    idl_byte *defn_vec_ptr,
    rpc_void_p_t array_addr,
    IDL_msp_t IDL_msp
#endif
);


void rpc_ss_ndr_u_fix_or_conf_arr
(
#ifdef IDL_PROTOTYPES
    idl_ulong_int dimensionality,
    idl_ulong_int *Z_values,
    idl_byte *defn_vec_ptr,
    rpc_void_p_t array_addr,
    idl_ulong_int flags,
    IDL_msp_t IDL_msp
#endif
);

void rpc_ss_ndr_u_fixed_arr_ptees
(
#ifdef IDL_PROTOTYPES
    idl_ulong_int defn_index,
    rpc_void_p_t array_addr,
    IDL_msp_t IDL_msp
#endif
);

void rpc_ss_ndr_u_for_foc_arr
(
#ifdef IDL_PROTOTYPES
    rpc_void_p_t array_addr,
    idl_ulong_int dimensionality,
    idl_ulong_int *Z_values,
    idl_byte base_type,
    idl_ulong_int element_size,
    idl_ulong_int element_defn_index,
    IDL_msp_t IDL_msp
#endif
);

void rpc_ss_ndr_u_for_foc_arr_ptees
(
#ifdef IDL_PROTOTYPES
    rpc_void_p_t array_addr,
    idl_ulong_int dimensionality,
    idl_ulong_int *Z_values,
    idl_byte base_type,
    idl_ulong_int element_size,
    idl_ulong_int element_defn_index,
    idl_byte *defn_vec_ptr,
    IDL_msp_t IDL_msp
#endif
);


void rpc_ss_ndr_u_n_e_union_ptees
(
#ifdef IDL_PROTOTYPES
    rpc_void_p_t param_addr,
    idl_ulong_int switch_value,
    idl_ulong_int switch_index,
    idl_ulong_int defn_index,
    rpc_void_p_t struct_addr,
    idl_ulong_int *struct_offset_vec_ptr,
    IDL_msp_t IDL_msp
#endif
);

void rpc_ss_ndr_u_param_cs_shadow
(
#ifdef IDL_PROTOTYPES
    idl_ulong_int type_index,
    IDL_cs_shadow_elt_t *cs_shadow,
    IDL_msp_t IDL_msp
#endif
);

#define rpc_ss_ndr_u_rlse_pointee_desc( p_pointee_desc, IDL_msp ) \
    if ((p_pointee_desc)->dimensionality > 0) \
    { \
        rpc_ss_mem_item_free(&IDL_msp->IDL_mem_handle, \
                                      (byte_p_t)((p_pointee_desc)->Z_values)); \
    }

void rpc_ss_ndr_u_v_or_o_arr_ptees
(
#ifdef IDL_PROTOTYPES
    idl_ulong_int dimensionality,
    idl_ulong_int *Z_values,
    idl_byte *defn_vec_ptr,
    rpc_void_p_t array_addr,
    IDL_bound_pair_t *range_list,
    IDL_msp_t IDL_msp
#endif
);

void rpc_ss_ndr_u_v1_varying_arr
(
#ifdef IDL_PROTOTYPES
    rpc_void_p_t array_addr,
    idl_byte *array_defn_ptr,
    idl_ulong_int flags,
    IDL_msp_t IDL_msp
#endif
);

void rpc_ss_ndr_u_var_or_open_arr
(
#ifdef IDL_PROTOTYPES
    idl_ulong_int dimensionality,
    idl_ulong_int *Z_values,
    idl_byte *defn_vec_ptr,
    rpc_void_p_t array_addr,
    IDL_bound_pair_t *range_list,
    idl_ulong_int flags,
    IDL_msp_t IDL_msp
#endif
);

void rpc_ss_ndr_unmar_by_copying
(
#ifdef IDL_PROTOTYPES
    idl_ulong_int element_count,
    idl_ulong_int element_size,
    rpc_void_p_t array_addr,
    IDL_msp_t IDL_msp
#endif
);

void rpc_ss_ndr_unmar_by_looping
(
#ifdef IDL_PROTOTYPES
    idl_ulong_int element_count,
    idl_byte base_type,
    rpc_void_p_t array_addr,
    idl_ulong_int element_size,
    idl_ulong_int element_defn_index,
    IDL_msp_t IDL_msp
#endif
);

void rpc_ss_ndr_unmar_context
(
#ifdef IDL_PROTOTYPES
    idl_byte context_type,
    rpc_void_p_t param_addr,
    IDL_msp_t IDL_msp
#endif
);

void rpc_ss_ndr_unmar_cs_array
(
#ifdef IDL_PROTOTYPES
    rpc_void_p_t array_addr,
    IDL_cs_shadow_elt_t *cs_shadow,
    idl_ulong_int *Z_values,
    idl_ulong_int array_shadow_index,
    idl_byte **p_defn_vec_ptr,
    IDL_msp_t IDL_msp
#endif
);
void rpc_ss_ndr_unmar_cs_char
(
#ifdef IDL_PROTOTYPES
    rpc_void_p_t data_addr,
    idl_ulong_int cs_type_defn_index,
    IDL_msp_t IDL_msp
#endif
);

void rpc_ss_ndr_unmar_deletes
(
#ifdef IDL_PROTOTYPES
    IDL_msp_t IDL_msp
#endif
);

void rpc_ss_ndr_unmar_fixed_arr
(
#ifdef IDL_PROTOTYPES
    idl_ulong_int defn_index,
    rpc_void_p_t array_addr,
    idl_ulong_int flags,
    IDL_msp_t IDL_msp
#endif
);


void rpc_ss_ndr_unmar_n_e_union
(
#ifdef IDL_PROTOTYPES
    rpc_void_p_t param_addr,
    idl_ulong_int defn_index,
    idl_ulong_int *p_switch_value,
    IDL_msp_t IDL_msp
#endif
);

void rpc_ss_ndr_unmar_pipe
(
#ifdef IDL_PROTOTYPES
    idl_ulong_int defn_index,
    rpc_void_p_t param_addr,
    IDL_msp_t IDL_msp
#endif
);

void rpc_ss_ndr_unmar_pointee
(
#ifdef IDL_PROTOTYPES
    idl_byte pointer_type,
    idl_byte *defn_vec_ptr,
    IDL_pointee_desc_t *p_pointee_desc,
    rpc_void_p_t *p_pointer,
    IDL_msp_t IDL_msp
#endif
);

void rpc_ss_ndr_unmar_pointee_desc
(
#ifdef IDL_PROTOTYPES
    idl_byte pointer_type,
    idl_byte *defn_vec_ptr,
    IDL_pointee_desc_t *p_pointee_desc,
    rpc_void_p_t *p_pointer,
    IDL_msp_t IDL_msp
#endif
);

void rpc_ss_ndr_unmar_range_list
(
#ifdef IDL_PROTOTYPES
    idl_ulong_int dimensionality,
    idl_byte base_type,
    IDL_bound_pair_t **p_range_list,
    IDL_msp_t IDL_msp
#endif
);

void rpc_ss_ndr_unmar_scalar
(
#ifdef IDL_PROTOTYPES
    idl_byte type_byte,
    rpc_void_p_t param_addr,
    IDL_msp_t IDL_msp
#endif
);

#ifdef __cplusplus

extern "C" {

void rpc_ss_ndr_unmar_struct
(
#ifdef IDL_PROTOTYPES
    idl_byte struct_type,
    idl_byte *defn_vec_ptr,
    rpc_void_p_t struct_addr,
    idl_ulong_int *Z_values,
    IDL_cs_shadow_elt_t *cs_shadow,
    IDL_msp_t IDL_msp
#endif
);

void rpc_ss_ndr_u_struct_pointees
(
#ifdef IDL_PROTOTYPES
    idl_byte struct_type,
    idl_ulong_int defn_index,
    rpc_void_p_t struct_addr,
    idl_ulong_int *Z_values,
    IDL_msp_t IDL_msp
#endif
);

void rpc_ss_ndr_unmar_interface(
#ifdef IDL_PROTOTYPES
    idl_ulong_int defn_index,
    void         *param_addr,
    void         *xmit_data_buff,
    IDL_msp_t     IDL_msp
#endif
);

void rpc_ss_ndr_unmar_dyn_interface(
#ifdef IDL_PROTOTYPES
    idl_ulong_int rtn_index,
    void         *param_addr,
    uuid_t       *piid,
    void         *xmit_data_buff,
    IDL_msp_t     IDL_msp
#endif
);

}

#else
void rpc_ss_ndr_unmar_struct
(
#ifdef IDL_PROTOTYPES
    idl_byte struct_type,
    idl_byte *defn_vec_ptr,
    rpc_void_p_t struct_addr,
    idl_ulong_int *Z_values,
    IDL_cs_shadow_elt_t *cs_shadow,
    IDL_msp_t IDL_msp
#endif
);

void rpc_ss_ndr_u_struct_pointees
(
#ifdef IDL_PROTOTYPES
    idl_byte struct_type,
    idl_ulong_int defn_index,
    rpc_void_p_t struct_addr,
    idl_ulong_int *Z_values,
    IDL_msp_t IDL_msp
#endif
);

void rpc_ss_ndr_unmar_interface(
#ifdef IDL_PROTOTYPES
    idl_ulong_int defn_index,
    void         *param_addr,
    void         *xmit_data_buff,
    IDL_msp_t     IDL_msp
#endif
);

void rpc_ss_ndr_unmar_dyn_interface(
#ifdef IDL_PROTOTYPES
    idl_ulong_int rtn_index,
    void         *param_addr,
    uuid_t       *piid,
    void         *xmit_data_buff,
    IDL_msp_t     IDL_msp
#endif
);

#endif

void rpc_ss_ndr_unmar_v1_string
(
#ifdef IDL_PROTOTYPES
    rpc_void_p_t param_addr,
    idl_ulong_int flags,
    IDL_msp_t IDL_msp
#endif
);

void rpc_ss_discard_allocate_ref
(
#ifdef IDL_PROTOTYPES
    idl_byte **p_type_vec_ptr
#endif
);

void rpc_ss_ndr_unmar_varying_arr
(
#ifdef IDL_PROTOTYPES
    idl_byte *array_defn_ptr,
    idl_boolean type_has_pointers,
    rpc_void_p_t param_addr,
    idl_ulong_int flags,
    IDL_msp_t IDL_msp
#endif
);

void rpc_ss_ndr_unmar_xmit_as
(
#ifdef IDL_PROTOTYPES
    idl_ulong_int defn_index,
    rpc_void_p_t param_addr,
    rpc_void_p_t xmit_data_buff,
    IDL_msp_t IDL_msp
#endif
);

void rpc_ss_ndr_unmar_Z_values
(
#ifdef IDL_PROTOTYPES
    idl_ulong_int dimensionality,
    idl_ulong_int **p_Z_values,
    IDL_msp_t IDL_msp
#endif
);

void rpc_ss_complete_conf_arr_descs(
#ifdef IDL_PROTOTYPES
    idl_ulong_int num_conf_char_arrays,
    idl_byte **conf_char_array_list,
    rpc_void_p_t IDL_param_vector[],
    IDL_msp_t IDL_msp
#endif
);

#ifdef __cplusplus
extern "C" {
#endif

/*
** The following types are used to build an entry point
** vector containing the addresses of the c++ entry points
** that are accessed by the c modules in the idl runtime library.
*/

/* ptr to function returning void     */
#ifndef rpc_ss_ndr_func_p_t_DEFINED
typedef void (*rpc_ss_ndr_func_p_t)();	
#define rpc_ss_ndr_func_p_t_DEFINED
#endif

/* the entry point vector	      */
extern rpc_ss_ndr_func_p_t rpc_ss_idl_mi_epv[];	

#ifdef __cplusplus
}
#endif

/*
** C++ routines called from C 
** The names are changed here so that they look normal
** in the calling modules.
*/

#define rpc_ss_ndr_unmar_interface \
	(*rpc_ss_idl_mi_epv[RPC_SS_NDR_UNMAR_INTERFACE])

#define rpc_ss_ndr_unmar_dyn_interface \
	(*rpc_ss_idl_mi_epv[RPC_SS_NDR_UNMAR_DYN_INTERFACE]) 


