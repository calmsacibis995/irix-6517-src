/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1996 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: ndrmi.h,v $
 * Revision 65.1  1997/10/20 19:15:38  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.8.2  1996/02/18  23:45:49  marty
 * 	Update OSF copyright years
 * 	[1996/02/18  22:44:36  marty]
 *
 * Revision 1.1.8.1  1995/12/07  22:29:45  root
 * 	Submit OSF/DCE 1.2.1
 * 
 * 	HP revision /main/HPDCE02/jrr_1.2_mothra/2  1995/11/20  22:39 UTC  psn
 * 	Merge second XIDL drop for DCE 1.2.1
 * 	[1995/11/17  17:07 UTC  dat  /main/dat_xidl2/1]
 * 
 * 	oct 95 idl drop
 * 	[1995/10/22  23:36:03  bfc]
 * 	 *
 * 	may 95 idl drop
 * 	[1995/10/22  22:57:24  bfc]
 * 	 *
 * 	DCE for DEC OSF/1: populate from OSF DCE R1.1
 * 	[1995/10/21  18:25:34  bfc]
 * 	 *
 * 
 * 	HP revision /main/HPDCE02/1  1994/08/03  16:32 UTC  tatsu_s
 * 	Merged mothra up to dce 1.1.
 * 
 * 	HP revision /main/HPDCE01/1  1994/05/26  20:45 UTC  gaz
 * 	Merge IDL performance changes
 * 
 * 	HP revision /main/gaz_kk_idl_merge/2  1994/05/06  20:02 UTC  gaz
 * 	update from mainline
 * 	[1995/12/07  21:15:46  root]
 * 
 * Revision 1.1.4.1  1994/01/21  22:30:39  cbrooks
 * 	RPC Code Cleanup - Initial Submission
 * 	[1994/01/21  21:51:23  cbrooks]
 * 
 * 	HP revision /main/gaz_kk_idl_merge/1  1994/05/06  19:35 UTC  gaz
 * 	Merge IDL performance enhancements with mainline
 * 
 * 	HP revision /main/ajayb_idl/1  1994/04/12  18:53 UTC  ajayb
 * 	Malloc optimizations
 * 
 * Revision 1.1.2.2  1993/07/07  20:09:00  ganni
 * 	reduced stub idl sources
 * 	[1993/07/07  19:38:18  ganni]
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
**      ndrmi.h
**
**  FACILITY:
**
**      Interface Definition Language (IDL) Compiler
**
**  ABSTRACT:
**
**      Header file for macros and procedures shared between ndrmi*.c modules
**
*/

/*  Marshalling strategy
 *
 *  All marshalling is done into buffers, except when an array is encountered
 *  that is sufficiently large to justify the "point at array optimization"
 *  A standard size iovector is used. When all its elements point at buffers
 *  or, by the optimization, arrays, it is despatched by an rpc_call_transmit.
 *  At the end of marshalling there is usually a further transmit, to send the
 *  data not already sent. On the server side, if there is no unsent data,
 *  there is no rpc_call_transmit. On the client side, if there is no unsent
 *  data there is still a transceive, to turn the line round.
 *
 *  The following state block fields affect marshalling
 *  IDL_buff_addr       address of the current buffer
 *  IDL_data_addr       address of first byte in buffer to be transmitted
 *  IDL_mp_start_offset determines the position of the start of data in the
 *                      current buffer relative to the first (0 mod 8) address
 *                      in the buffer. This will be 0 unless marshalling the
 *                      buffer follows a "marshall array by pointing"
 *  IDL_mp              Address in buffer at which next data item can be placed
 *  IDL_left_in_buff    Number of bytes in current buffer still available for
 *                      marshalling.
 */


/******************************************************************************/
/*                                                                            */
/*  Check whether there is enough space in the current buffer, if one exists  */
/*  If no buffer, create one                                                  */
/*  If buffer is full, perform closure/despatch and start a new buffer        */
/*                                                                            */
/******************************************************************************/
#define rpc_ss_ndr_marsh_check_buffer( datum_size, IDL_msp ) \
{ \
    if (datum_size > IDL_msp->IDL_left_in_buff) \
    { \
        if (IDL_msp->IDL_buff_addr != NULL) \
        { \
            rpc_ss_attach_buff_to_iovec( IDL_msp ); \
            rpc_ss_xmit_iovec_if_necess( idl_false, IDL_msp ); \
            IDL_msp->IDL_mp_start_offset = 0; \
        } \
        rpc_ss_ndr_marsh_init_buffer( IDL_msp ); \
    } \
}


/******************************************************************************/
/*                                                                            */
/* Marshall a scalar                                                          */
/*                                                                            */
/******************************************************************************/

#define IDL_MARSH_1_BYTE_SCALAR( marshalling_macro, type, param_addr ) \
{ \
    rpc_ss_ndr_marsh_check_buffer( 1, IDL_msp ); \
    marshalling_macro(IDL_msp->IDL_mp, *(type *)(param_addr)); \
    IDL_msp->IDL_mp += 1; \
    IDL_msp->IDL_left_in_buff -= 1; \
}


#ifndef _KERNEL
#define IDL_MARSH_BOOLEAN( param_addr ) \
{ \
        IDL_MARSH_1_BYTE_SCALAR( rpc_marshall_boolean, idl_boolean, param_addr ); \
}
#else
#define IDL_MARSH_BOOLEAN( param_addr ) \
{ \
        IDL_MARSH_1_BYTE_SCALAR( rpc_marshall_boolean, idl_boolean, param_addr ); \
}
#endif /* _KERNEL */


#define IDL_MARSH_BYTE( param_addr ) \
    IDL_MARSH_1_BYTE_SCALAR( rpc_marshall_byte, idl_byte, param_addr )

#define IDL_MARSH_CHAR( param_addr ) \
    IDL_MARSH_1_BYTE_SCALAR( rpc_marshall_char, idl_char, param_addr )

#define IDL_MARSH_ALIGNED_SCALAR( marshalling_macro, size, type, param_addr ) \
{ \
    IDL_MARSH_ALIGN_MP( IDL_msp, size ); \
    rpc_ss_ndr_marsh_check_buffer( size, IDL_msp ); \
    marshalling_macro(IDL_msp->IDL_mp, *(type *)(param_addr)); \
    IDL_msp->IDL_mp += size; \
    IDL_msp->IDL_left_in_buff -= size; \
}

#define IDL_MARSH_DOUBLE( param_addr ) \
    IDL_MARSH_ALIGNED_SCALAR( rpc_marshall_long_float, 8, idl_long_float, param_addr )

#define IDL_MARSH_ENUM( param_addr ) \
    IDL_MARSH_ALIGNED_SCALAR( rpc_marshall_enum, 2, int, param_addr )

#define IDL_MARSH_FLOAT( param_addr ) \
    IDL_MARSH_ALIGNED_SCALAR( rpc_marshall_short_float, 4, idl_short_float, param_addr )



#ifndef _KERNEL
#define IDL_MARSH_SMALL( param_addr ) \
{ \
        IDL_MARSH_1_BYTE_SCALAR( rpc_marshall_small_int, idl_small_int, param_addr ); \
}
#else
#define IDL_MARSH_SMALL( param_addr ) \
{ \
        IDL_MARSH_1_BYTE_SCALAR( rpc_marshall_small_int, idl_small_int, param_addr ); \
}
#endif /* _KERNEL */


#define IDL_MARSH_SHORT( param_addr ) \
    IDL_MARSH_ALIGNED_SCALAR( rpc_marshall_short_int, 2, idl_short_int, param_addr )

#define IDL_MARSH_LONG( param_addr ) \
    IDL_MARSH_ALIGNED_SCALAR( rpc_marshall_long_int, 4, idl_long_int, param_addr )

#define IDL_MARSH_HYPER( param_addr ) \
    IDL_MARSH_ALIGNED_SCALAR( rpc_marshall_hyper_int, 8, idl_hyper_int, param_addr )


#ifndef _KERNEL
#define IDL_MARSH_USMALL( param_addr ) \
{ \
        IDL_MARSH_1_BYTE_SCALAR( rpc_marshall_usmall_int, idl_usmall_int, param_addr ); \
}
#else
#define IDL_MARSH_USMALL( param_addr ) \
{ \
        IDL_MARSH_1_BYTE_SCALAR( rpc_marshall_usmall_int, idl_usmall_int, param_addr ); \
}
#endif /* _KERNEL */


#ifndef _KERNEL
#define IDL_MARSH_USHORT( param_addr ) \
{ \
        IDL_MARSH_ALIGNED_SCALAR( rpc_marshall_ushort_int, 2, idl_ushort_int, param_addr ); \
}
#else
#define IDL_MARSH_USHORT( param_addr ) \
{ \
        IDL_MARSH_ALIGNED_SCALAR( rpc_marshall_ushort_int, 2, idl_ushort_int, param_addr ); \
}
#endif /* _KERNEL */


#define IDL_MARSH_ULONG( param_addr ) \
    IDL_MARSH_ALIGNED_SCALAR( rpc_marshall_ulong_int, 4, idl_ulong_int, param_addr )

#define IDL_MARSH_UHYPER( param_addr ) \
    IDL_MARSH_ALIGNED_SCALAR( rpc_marshall_uhyper_int, 8, idl_uhyper_int, param_addr )

#define IDL_MARSH_V1_ENUM( param_addr ) \
    IDL_MARSH_ALIGNED_SCALAR( rpc_marshall_v1_enum, 4, int, param_addr )

#ifdef IDL_ENABLE_STATUS_MAPPING
#define IDL_MARSH_ERROR_STATUS( param_addr ) \
{ \
    IDL_MARSH_ALIGN_MP( IDL_msp, 4 ); \
    rpc_ss_ndr_marsh_check_buffer( 4, IDL_msp ); \
    rpc_marshall_ulong_int(IDL_msp->IDL_mp, *(idl_ulong_int *)(param_addr)); \
    rpc_ss_map_local_to_dce_status((error_status_t *)(IDL_msp->IDL_mp)); \
    IDL_msp->IDL_mp += 4; \
    IDL_msp->IDL_left_in_buff -= 4; \
}
#else
#define IDL_MARSH_ERROR_STATUS( param_addr ) \
{ \
    IDL_MARSH_ALIGN_MP( IDL_msp, 4 ); \
    rpc_ss_ndr_marsh_check_buffer( 4, IDL_msp ); \
    rpc_marshall_ulong_int(IDL_msp->IDL_mp, *(idl_ulong_int *)(param_addr)); \
    IDL_msp->IDL_mp += 4; \
    IDL_msp->IDL_left_in_buff -= 4; \
}
#endif

/* For marshalling interpreter internal variables, which are always C format */
#define IDL_MARSH_CUSHORT( param_addr ) \
    IDL_MARSH_ALIGNED_SCALAR( rpc_marshall_ushort_int, 2, idl_ushort_int, param_addr )

#define IDL_MARSH_CUSMALL( param_addr ) \
    IDL_MARSH_1_BYTE_SCALAR( rpc_marshall_usmall_int, idl_usmall_int, param_addr )


/* Function prototypes */



void rpc_ss_attach_buff_to_iovec
(
#ifdef IDL_PROTOTYPES
    IDL_msp_t IDL_msp
#endif
);

void rpc_ss_conf_struct_cs_bounds
(
#ifdef IDL_PROTOTYPES
    idl_byte *defn_vec_ptr,
    IDL_cs_shadow_elt_t *cs_shadow,
    IDL_bound_pair_t *bounds_list,
    IDL_msp_t IDL_msp
#endif
);

void rpc_ss_discard_allocate_ref
(
#ifdef IDL_PROTOTYPES
    idl_byte **p_type_vec_ptr
#endif
);

void rpc_ss_ndr_m_dfc_arr_ptees
(
#ifdef IDL_PROTOTYPES
    idl_ulong_int defn_index,
    rpc_void_p_t array_addr,
    rpc_void_p_t struct_addr,
    idl_ulong_int *struct_offset_vec_ptr,
    idl_ulong_int flags,
    IDL_msp_t IDL_msp
#endif
);

void rpc_ss_ndr_m_dvo_arr_ptees
(
#ifdef IDL_PROTOTYPES
    idl_ulong_int defn_index,
    rpc_void_p_t array_addr,
    rpc_void_p_t struct_addr,
    idl_ulong_int *struct_offset_vec_ptr,
    idl_ulong_int flags,
    IDL_msp_t IDL_msp
#endif
);

void rpc_ss_ndr_m_enc_union_or_ptees
(
#ifdef IDL_PROTOTYPES
    rpc_void_p_t param_addr,
    idl_ulong_int defn_index,
    idl_boolean pointees,
    IDL_msp_t IDL_msp
#endif
);

void rpc_ss_ndr_m_fix_or_conf_arr
(
#ifdef IDL_PROTOTYPES
    rpc_void_p_t array_addr,
    idl_ulong_int dimensionality,
    IDL_bound_pair_t *bounds_list,
    idl_byte *defn_vec_ptr,
    idl_ulong_int flags,
    IDL_msp_t IDL_msp
#endif
);

void rpc_ss_ndr_m_fixed_cs_array
(
#ifdef IDL_PROTOTYPES
    rpc_void_p_t array_addr,
    idl_byte **p_defn_vec_ptr,
    IDL_msp_t IDL_msp
#endif
);


void rpc_ss_ndr_m_n_e_union_or_ptees
(
#ifdef IDL_PROTOTYPES
    rpc_void_p_t param_addr,
    idl_ulong_int switch_index,
    idl_ulong_int defn_index,
    rpc_void_p_t struct_addr,
    idl_ulong_int *struct_offset_vec_ptr,
    idl_boolean pointees,
    IDL_msp_t IDL_msp
#endif
);

void rpc_ss_ndr_m_param_cs_shadow
(
#ifdef IDL_PROTOTYPES
    idl_byte *type_vec_ptr,
    idl_ulong_int param_index,
    idl_ulong_int shadow_length,
    IDL_cs_shadow_elt_t **p_cs_shadow,
    IDL_msp_t IDL_msp
#endif
);

void rpc_ss_ndr_m_rlse_cs_shadow
(
#ifdef IDL_PROTOTYPES
    IDL_cs_shadow_elt_t *cs_shadow,
    idl_ulong_int shadow_length,
    IDL_msp_t IDL_msp
#endif
);

void rpc_ss_ndr_m_struct_cs_shadow
(
#ifdef IDL_PROTOTYPES
    rpc_void_p_t struct_addr,
    idl_byte struct_type,
    idl_ulong_int shadow_length,
    idl_ulong_int offset_index,
    idl_byte *defn_vec_ptr,
    IDL_cs_shadow_elt_t **p_cs_shadow,
    IDL_msp_t IDL_msp
#endif
);

void rpc_ss_ndr_m_var_or_open_arr
(
#ifdef IDL_PROTOTYPES
    rpc_void_p_t array_addr,
    idl_ulong_int *Z_values,
    idl_ulong_int dimensionality,
    IDL_bound_pair_t *range_list,
    idl_byte *defn_vec_ptr,
    idl_ulong_int flags,
    IDL_msp_t IDL_msp
#endif
);

void rpc_ss_ndr_marsh_by_copying
(
#ifdef IDL_PROTOTYPES
    idl_ulong_int element_count,
    idl_ulong_int element_size,
    rpc_void_p_t array_addr,
    IDL_msp_t IDL_msp
#endif
);

void rpc_ss_ndr_marsh_by_looping
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

void rpc_ss_ndr_marsh_by_pointing
(
#ifdef IDL_PROTOTYPES
    idl_ulong_int element_count,
    idl_ulong_int element_size,
    rpc_void_p_t array_addr,
    IDL_msp_t IDL_msp
#endif
);

void rpc_ss_ndr_marsh_context
(
#ifdef IDL_PROTOTYPES
    idl_byte context_type,
    rpc_void_p_t param_addr,
    IDL_msp_t IDL_msp
#endif
);

void rpc_ss_ndr_marsh_cs_array
(
#ifdef IDL_PROTOTYPES
    rpc_void_p_t array_addr,
    IDL_cs_shadow_elt_t *cs_shadow,
    idl_ulong_int shadow_index,
    idl_boolean in_struct,
    idl_byte **p_defn_vec_ptr,
    IDL_msp_t IDL_msp
#endif
);

void rpc_ss_ndr_marsh_cs_char
(
#ifdef IDL_PROTOTYPES
    rpc_void_p_t char_addr,
    idl_ulong_int cs_type_defn_index,
    IDL_msp_t IDL_msp
#endif
);

void rpc_ss_ndr_marsh_deletes
(
#ifdef IDL_PROTOTYPES
    IDL_msp_t IDL_msp
#endif
);

void rpc_ss_ndr_marsh_fixed_arr
(
#ifdef IDL_PROTOTYPES
    idl_ulong_int defn_index,
    rpc_void_p_t array_addr,
    idl_ulong_int flags,
    IDL_msp_t IDL_msp
#endif
);


void rpc_ss_ndr_marsh_open_arr
(
#ifdef IDL_PROTOTYPES
    idl_ulong_int defn_index,
    rpc_void_p_t array_addr,
    idl_ulong_int flags,
    IDL_msp_t IDL_msp
#endif
);

void rpc_ss_ndr_marsh_pipe
(
#ifdef IDL_PROTOTYPES
    idl_ulong_int defn_index,
    rpc_void_p_t param_addr,
    IDL_msp_t IDL_msp
#endif
);

void rpc_ss_ndr_marsh_pointee
(
#ifdef IDL_PROTOTYPES
    idl_byte *defn_vec_ptr,
    rpc_void_p_t pointee_addr,
    idl_boolean register_node,
    IDL_pointee_desc_t *p_pointee_desc,
    IDL_msp_t IDL_msp
#endif
);

void rpc_ss_ndr_marsh_scalar
(
#ifdef IDL_PROTOTYPES
    idl_byte type_byte,
    rpc_void_p_t param_addr,
    IDL_msp_t IDL_msp
#endif
);

#ifdef __cplusplus

extern "C" {

void rpc_ss_ndr_marsh_struct
(
#ifdef IDL_PROTOTYPES
    idl_byte struct_type,
    idl_ulong_int defn_index,
    rpc_void_p_t struct_addr,
    IDL_msp_t IDL_msp
#endif
);

void rpc_ss_ndr_m_struct_pointees
(
#ifdef IDL_PROTOTYPES
    idl_byte struct_type,
    idl_ulong_int defn_index,
    rpc_void_p_t struct_addr,
    IDL_msp_t IDL_msp
#endif
);

void rpc_ss_ndr_marsh_interface
(
#ifdef IDL_PROTOTYPES
    idl_ulong_int defn_index,
    void 	  *param_addr,
    IDL_msp_t     IDL_msp
#endif
);

void rpc_ss_ndr_marsh_dyn_interface
(
#ifdef IDL_PROTOTYPES
    idl_ulong_int rtn_index,
    void 	  *param_addr,
    uuid_t	  *piid,
    IDL_msp_t     IDL_msp
#endif
);

}

#else

void rpc_ss_ndr_marsh_struct
(
#ifdef IDL_PROTOTYPES
    idl_byte struct_type,
    idl_ulong_int defn_index,
    rpc_void_p_t struct_addr,
    IDL_msp_t IDL_msp
#endif
);

void rpc_ss_ndr_m_struct_pointees
(
#ifdef IDL_PROTOTYPES
    idl_byte struct_type,
    idl_ulong_int defn_index,
    rpc_void_p_t struct_addr,
    IDL_msp_t IDL_msp
#endif
);

void rpc_ss_ndr_marsh_interface
(
#ifdef IDL_PROTOTYPES
    idl_ulong_int defn_index,
    void 	  *param_addr,
    IDL_msp_t     IDL_msp
#endif
);

void rpc_ss_ndr_marsh_dyn_interface
(
#ifdef IDL_PROTOTYPES
    idl_ulong_int rtn_index,
    void 	  *param_addr,
    uuid_t	  *piid,
    IDL_msp_t     IDL_msp
#endif
);

#endif

void rpc_ss_ndr_marsh_v1_string
(
#ifdef IDL_PROTOTYPES
    rpc_void_p_t param_addr,
    idl_ulong_int flags,
    IDL_msp_t IDL_msp
#endif
);

void rpc_ss_ndr_marsh_varying_arr
(
#ifdef IDL_PROTOTYPES
    idl_ulong_int defn_index,
    rpc_void_p_t array_addr,
    rpc_void_p_t struct_addr,
    idl_ulong_int *struct_offset_vec_ptr,
    idl_ulong_int flags,
    IDL_msp_t IDL_msp
#endif
);

void rpc_ss_ndr_marsh_xmit_as
(
#ifdef IDL_PROTOTYPES
    idl_ulong_int defn_index,
    rpc_void_p_t param_addr,
    IDL_msp_t IDL_msp
#endif
);

void rpc_ss_ndr_marsh_Z_values
(
#ifdef IDL_PROTOTYPES
    idl_ulong_int dimensionality,
    idl_ulong_int *Z_values,
    IDL_msp_t IDL_msp
#endif
);

void rpc_ss_pointee_desc_from_data
(
#ifdef IDL_PROTOTYPES
    idl_byte *defn_vec_ptr,
    rpc_void_p_t array_addr,
    rpc_void_p_t struct_addr,
    idl_ulong_int *struct_offset_vec_ptr,
    IDL_pointee_desc_t *p_pointee_desc,
    IDL_msp_t IDL_msp
#endif
);

#define rpc_ss_rlse_data_pointee_desc( p_pointee_desc, IDL_msp ) \
    if ((p_pointee_desc)->dimensionality > 0) \
    { \
        rpc_ss_mem_item_free(&IDL_msp->IDL_mem_handle, \
                                      (byte_p_t)((p_pointee_desc)->Z_values)); \
    }

void rpc_ss_xmit_iovec_if_necess
(
#ifdef IDL_PROTOTYPES
    idl_boolean attached_pointed_at,
    IDL_msp_t IDL_msp
#endif
);

void rpc_ss_ndr_m_for_conf_string(
#ifdef IDL_PROTOTYPES
    idl_byte **p_type_vec_ptr,
    rpc_void_p_t param_addr,
    idl_boolean *p_is_string,
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

#define rpc_ss_ndr_marsh_interface \
	(*rpc_ss_idl_mi_epv[RPC_SS_NDR_MARSH_INTERFACE])

#define rpc_ss_ndr_marsh_dyn_interface \
	(*rpc_ss_idl_mi_epv[RPC_SS_NDR_MARSH_DYN_INTERFACE]) 
