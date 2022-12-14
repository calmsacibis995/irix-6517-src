/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1996 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: idlddefs.h,v $
 * Revision 65.3  1999/07/21 19:01:14  gwehrman
 * Defined SGIMIPS for this development header.  Fix for bug 657352.
 *
 * Revision 65.2  1997/10/20 19:16:18  jdoak
 * Initial IRIX 6.4 code merge
 *
 * Revision 1.1.443.2  1996/02/18  22:57:20  marty
 * 	Update OSF copyright years
 * 	[1996/02/18  22:16:15  marty]
 *
 * Revision 1.1.443.1  1995/12/08  00:23:13  root
 * 	Submit OSF/DCE 1.2.1
 * 
 * 	HP revision /main/HPDCE02/jrr_1.2_mothra/1  1995/11/20  22:09 UTC  jrr
 * 	Merge second XIDL drop for DCE 1.2.1
 * 	[1995/11/17  17:10 UTC  dat  /main/dat_xidl2/1]
 * 
 * 	[1994/08/29  15:38:16  rico  1.1.439.1]
 * 
 * 	 *
 * 
 * 	HP revision /main/HPDCE02/2  1995/09/06  18:46 UTC  gaz
 * 	remove DMS code
 * 
 * 	HP revision /main/HPDCE02/gaz_mothra_fcompat/1  1995/09/06  14:16 UTC  gaz
 * 	Mothra/WC DMS fcompat
 * 
 * 	HP revision /main/HPDCE02/1  1995/03/05  14:00 UTC  gaz
 * 	Merge DMS changes to mainline
 * 
 * 	HP revision /main/HPDCE02/lmm_mothra_instr/2  1995/02/28  21:43 UTC  gaz
 * 	Add conditional definition of DMS_TIMESTAMP
 * 
 * 	HP revision /main/HPDCE02/lmm_mothra_instr/1  1995/01/26  18:18 UTC  lmm
 * 	adding DMS support
 * 	[1995/12/08  00:01:03  root]
 * 
 * Revision 1.1.438.8  1994/08/12  20:57:28  tom
 * 	Bug 10411 - Remove unsigned from pipe_number, next_{in,out}_pipe
 * 	  arguments for rpc_ss_mts_init_callee_pipe.
 * 	[1994/08/12  20:57:10  tom]
 * 
 * Revision 1.1.438.7  1994/08/09  15:33:59  rico
 * 	Added volatile to argument IDL_ms_t * in function prototype for
 * 	rpc_ss_mts_client_estab_alloc().
 * 	[1994/08/09  15:23:32  rico]
 * 
 * Revision 1.1.438.6  1994/07/18  15:06:48  tom
 * 	Bug 10787 - Add typedef for IDL_rtn_vec.
 * 	[1994/07/15  20:20:06  tom]
 * 
 * Revision 1.1.438.5  1994/07/13  18:56:31  tom
 * 	Bug 10103 - Reduce little-endian bias of stubs
 * 	[1994/07/13  18:55:07  tom]
 * 
 * Revision 1.1.438.4  1994/06/22  20:41:39  tom
 * 	Bug 10979 - Change IDL_es_state_t struct definition to
 * 	have the tag idl_es_handle_s_t.  This allows the
 * 	definition of idl_es_handle_t to be opaque and look nice.
 * 	[1994/06/22  20:41:23  tom]
 * 
 * Revision 1.1.438.4  1994/06/22  20:41:39  tom
 * 	Bug 10979 - Change IDL_es_state_t struct definition to
 * 	have the tag idl_es_handle_s_t.  This allows the
 * 	definition of idl_es_handle_t to be opaque and look nice.
 * 	[1994/06/22  20:41:23  tom]
 * 
 * Revision 1.1.438.3  1994/05/11  17:37:52  rico
 * 	Fixes to [string] and [ptr] interactions.
 * 	[1994/05/11  17:36:44  rico]
 * 
 * Revision 1.1.438.2  1994/05/10  18:03:26  tom
 * 	Add IDL_es_flags field to IDL_es_state_t structure.
 * 	[1994/05/10  17:35:06  tom]
 * 
 * Revision 1.1.438.1  1994/01/21  22:40:01  cbrooks
 * 	RPC Code Cleanup - Initial Submission
 * 	[1994/01/21  20:30:52  cbrooks]
 * 
 * Revision 1.1.2.3  1993/08/20  15:54:43  hinxman
 * 	OT 8484 Marshalling of large arrays of [cs_char]s
 * 	[1993/08/20  15:53:55  hinxman]
 * 
 * Revision 1.1.2.2  1993/07/07  20:12:15  ganni
 * 	reduced stub idl sources
 * 	[1993/07/07  19:40:28  ganni]
 * 
 * $EndLog$
 */
/*
**
**  Copyright (c) 1991, 1992, 1993 by
**  Digital Equipment Corporation, Maynard, Mass.
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
**      idlddefs.h
**
**  FACILITY:
**
**      Interface Definition Language (IDL) Compiler
**
**  ABSTRACT:
**
**  Definitions used by stubs generated by MTS compiler
**
*/

#ifndef IDLDDEFS_H
#define IDLDDEFS_H	1
#ifndef SGIMIPS
#define SGIMIPS
#endif /* !SGIMIPS */

#ifdef __cplusplus
    extern "C" {
#endif

#include <dce/dce.h>
#include <dce/rpc.h>
#include <dce/idl_es.h>
#include <dce/stubbase.h>

/*
 * 1/10/94 CBrooks 
 * 
 * for DCE1.1, we remove the definitions for the following 
 *
 * Machines on which only byte fields are guaranteed naturally aligned 
 * #define IDL_NATURAL_ALIGN_1
 *
 * Machines on which 1, 2 and 4 byte fields are naturally aligned 
 * #define IDL_NATURAL_ALIGN_4
 *
 * Machines on which 1, 2, 4 and 8 byte fields are naturally aligned  
 * #define IDL_NATURAL_ALIGN_8
 *
 * These definitions must now be included in the ndr_rep.h file 
 *
 * N.B. As of this date, it doesn't appear that any of the above 
 * macros are *required* to be defined. 
 */

/*
 * IDL general defines 
 */

#define IDL_BUFF_SIZE 2048

/* 
 *The number of iovector elements in the iovector the NDR marshalling
 * interpreter uses for transmits 
 */

#define IDL_IOVECTOR_SIZE 10

/* 
 * Arrays with the less than this number of elements are not NDR
 *  marshalled by pointing 
 */

#define IDL_POINT_THRESHOLD 256

/* 
 * Arrays with more than this number of dimensions require interpreter
 * data structures to be dynamically allocated 
*/
 
#define IDL_NORMAL_DIMS 7
 

/* Always try and get storage for at least this many pipe elements */

#define IDL_MIN_PIPE_CHUNK_SIZE 8


/*
 * 'Address of array' macro - use to spell or not to spell '&' before array
 * reference reliably depending on whether compiler is ANSI Standard C.
 *
 * 1/10/94 CBrooks 
 *
 * The following definition had previously been the covered by the __STDC__
 * macro. We make it the default, since we require an ANSI compiler for 
 * DCE 1.1
 */

#define IDL_AOA &

/*
 * Macro used to spell or not to spell 'const' keyword before data definition
 * depending on whether compiler is ANSI Standard C.
 *
 * 1/10/94 
 * Strictly speaking, if the ANSI C Compiler supports "const" correctly
 */

#ifdef CONST_NOT_SUPPORTED
#define IDL_CONST 
#else
#define IDL_CONST const 
#endif


/*
 *  Flag types
 *  "Client or server?", "Marshalling or Unmarshalling?"
 */
typedef enum {IDL_client_side_k, IDL_server_side_k} IDL_side_k_t;
typedef enum {IDL_marshalling_k, IDL_unmarshalling_k} IDL_mar_or_unmar_k_t;
typedef enum {IDL_lang_c_k
			    , IDL_lang_cxx_k
                                                    } IDL_language_k_t;


/* Structure with pointers to I-char tags */

typedef struct IDL_cs_tags_t {
    idl_ulong_int *p_marsh_tag;     /* Pointer to tag used for marshalling */
    idl_ulong_int *p_unmar_tag;     /* Pointer to tag used for unmarshalling */
} IDL_cs_tags_t;

#define IDL_STACK_PACKET_SIZE 256

typedef enum {
    IDL_stack_packet_unused_k,  /* The marshalling interpeter's stack packet
                                        has not been used for marshalling */
    IDL_stack_packet_in_use_k,  /* The marshalling interpeter's stack packet
                                        is the current marshalling buffer */
    IDL_stack_packet_used_k,    /* The marshalling interpeter's stack packet
                                        has been used and despatched */
    IDL_stack_packet_part_used_k  /* The stack packet was being used for
                               marshalling before a "marshall by pointing at" */
} IDL_stack_packet_use_k_t;


typedef void (IDL_ENTRY *IDL_rtn_func_t)  _DCE_PROTOTYPE_ ((rpc_void_p_t ));

/*
 *  Interpreter state block
 */
typedef struct IDL_ms_t {
    idl_byte *IDL_type_vec;   /* The type vector for this interface       */
    idl_ulong_int *IDL_offset_vec; /* The offset vector for this interface */
    IDL_rtn_func_t *IDL_rtn_vec; /* The routine vector for this interface */
    rpc_void_p_t *IDL_param_vec; /* The parameter vector for this operation */
    IoVec_t(IDL_IOVECTOR_SIZE) IDL_iovec; /* RPC I/O vec pointing to buffers */
    idl_byte *IDL_buff_addr; /* Address of current marshalling buffer */
    idl_byte *IDL_data_addr; /* Address of start of data in current buff */
    idl_byte *IDL_mp;   /* Pointer to first free location in current buffer */
    rpc_iovector_elt_p_t IDL_elt_p;
                        /* Address of iovector_elt being used for receives */
    rpc_ss_mem_handle IDL_mem_handle;   /* Stub memory management handle */
    rpc_void_p_t (IDL_ALLOC_ENTRY *IDL_p_allocate) _DCE_PROTOTYPE_ ((idl_size_t));
    void (IDL_ALLOC_ENTRY *IDL_p_free) _DCE_PROTOTYPE_ ((rpc_void_p_t ));
    rpc_ss_node_table_t IDL_node_table;   /* node number to pointer table */
    volatile rpc_call_handle_t IDL_call_h;  /* Call handle for RPC call */
    handle_t IDL_h;  /* Client's binding handle - used with contexts
                                                    and I-char machinery */
    rpc_void_p_t IDL_pickling_handle;   /* Address of pickling state block */
    volatile error_status_t IDL_status;
                               /* Current Error status of last failing op  */
    volatile idl_long_int IDL_fault;
                                /* Current Fault status of last failing op  */
    idl_ulong_int IDL_elts_in_use;   /* Number of I/O vec element in use     */
    ndr_format_t IDL_drep;      /* Data rep for NDR transfer syntax */
    idl_ulong_int IDL_mp_start_offset;  /* Offset of start of data from first
                                        (0 mod 8) address in current buffer */
    idl_ulong_int IDL_left_in_buff; /* Unused space in curr buff (in bytes) */
    IDL_side_k_t IDL_side;      /* Client or server side? */
    idl_ulong_int IDL_m_xmit_level; /* Level of [transmit_as] nesting when
                                  marshalling. 0 when no [transmit_as] active */
    IDL_language_k_t IDL_language;  /* Programming language of appl code */
    idl_boolean IDL_marsh_pipe;     /* TRUE while a pipe is being marshalled */
    volatile idl_boolean IDL_restartable;
                           /* [auto_handle] TRUE => operation is restartable */
    idl_byte IDL_ms_res_byte_1;
    idl_byte IDL_ms_res_byte_2;
    IDL_stack_packet_use_k_t IDL_stack_packet_status;
    idl_byte *IDL_stack_packet_addr;
    IDL_cs_tags_t *IDL_cs_tags_p;
    rpc_void_p_t IDL_ms_res_ptr_4;
    rpc_void_p_t IDL_ms_res_ptr_5;

} IDL_ms_t, *IDL_msp_t;

void IDL_ENTRY rpc_ss_report_error_2  _DCE_PROTOTYPE_ ( 
  (
    ndr_ulong_int ,
    ndr_ulong_int ,
    ndr_ulong_int ,
    RPC_SS_THREADS_CANCEL_STATE_T *,
    error_status_t *,
    error_status_t *,
    EXCEPTION *[],
    IDL_msp_t 
  )
);

void IDL_ENTRY rpc_ss_send_server_exception_2  _DCE_PROTOTYPE_ ( 
  (
    rpc_call_handle_t ,
    EXCEPTION *,
    idl_long_int ,
    EXCEPTION *[],
    IDL_msp_t 
  )
);

void IDL_ENTRY rpc_ss_type_vec_vers_check  _DCE_PROTOTYPE_ ( 
 (  IDL_msp_t   )
);

void IDL_ENTRY rpc_ss_init_marsh_state  _DCE_PROTOTYPE_ ( 
  ( idl_byte [], IDL_msp_t  )
);



void IDL_ENTRY rpc_ss_ndr_clean_up  _DCE_PROTOTYPE_ ( 
  ( IDL_ms_t * )
);



/* Interpreter header access */

#define IDL_INTERP_ENCODE_MAJOR     8
#define IDL_INTERP_ENCODE_MINOR    10

/* type vector offsets */
#define TVEC_INT_REP_OFFSET 4
#define IDL_VERSION_NUMBER(versno_offset)\
    (IDL_msp->IDL_type_vec[TVEC_INT_REP_OFFSET] != NDR_LOCAL_INT_REP) ? \
       (((*(idl_byte *)(IDL_msp->IDL_type_vec+versno_offset+1)) << 8) | \
       *(idl_byte *)(IDL_msp->IDL_type_vec+versno_offset)) : \
       (*(idl_short_int *)(IDL_msp->IDL_type_vec+versno_offset))

/* Interpreter machinery - values of type byte */

#define IDL_DT_NULL                   0
#define IDL_DT_BOOLEAN                1
#define IDL_DT_BYTE                   2
#define IDL_DT_CHAR                   3
#define IDL_DT_DOUBLE                 4
#define IDL_DT_ENUM                   5
#define IDL_DT_FLOAT                  6
#define IDL_DT_SMALL                  7
#define IDL_DT_SHORT                  8
#define IDL_DT_LONG                   9
#define IDL_DT_HYPER                 10
#define IDL_DT_USMALL                11
#define IDL_DT_USHORT                12
#define IDL_DT_ULONG                 13
#define IDL_DT_UHYPER                14
#define IDL_DT_FIXED_STRUCT          15
#define IDL_DT_CONF_STRUCT           16
#define IDL_DT_BEGIN_NESTED_STRUCT   18
#define IDL_DT_END_NESTED_STRUCT     19
#define IDL_DT_FIXED_ARRAY           20
#define IDL_DT_VARYING_ARRAY         21
#define IDL_DT_CONF_ARRAY            22
#define IDL_DT_OPEN_ARRAY            23
#define IDL_DT_ENC_UNION             24
#define IDL_DT_N_E_UNION             25
#define IDL_DT_DOES_NOT_EXIST        26     /* Union has no default case */
#define IDL_DT_VOID                  27     /* Empty union arm */
#define IDL_DT_PASSED_BY_REF         28
#define IDL_DT_FULL_PTR              29
#define IDL_DT_UNIQUE_PTR            30
#define IDL_DT_REF_PTR               31
#define IDL_DT_IGNORE                32
#define IDL_DT_STRING                33
#define IDL_DT_TRANSMIT_AS           34
#define IDL_DT_REPRESENT_AS          35
#define IDL_DT_ALLOCATE              36
#define IDL_DT_ALLOCATE_REF          37
#define IDL_DT_NDR_ALIGN_2           38
#define IDL_DT_NDR_ALIGN_4           39
#define IDL_DT_NDR_ALIGN_8           40
#define IDL_DT_PIPE                  41
#define IDL_DT_IN_CONTEXT            42
#define IDL_DT_IN_OUT_CONTEXT        43
#define IDL_DT_OUT_CONTEXT           44
#define IDL_DT_V1_ENUM               45
#define IDL_DT_V1_ARRAY              46
#define IDL_DT_V1_STRING             47
#define IDL_DT_V1_CONF_STRUCT        48
#define IDL_DT_FREE_REP              49
#define IDL_DT_ERROR_STATUS          50
#define IDL_DT_DELETED_NODES         51
#define IDL_DT_CS_TYPE               52
#define IDL_DT_CS_SHADOW             53
#define IDL_DT_CS_ARRAY              54
#define IDL_DT_CS_ATTRIBUTE          55
#define IDL_DT_CS_RLSE_SHADOW        56
#define IDL_DT_INTERFACE             57
#define IDL_DT_DYN_INTERFACE         58
#define IDL_DT_EOL                  254
/* Reserved as filler byte          255 */

#define IDL_TYPE_IS_NDR_ALIGN(type_byte)\
    ((type_byte >= IDL_DT_NDR_ALIGN_2) && \
        (type_byte <= IDL_DT_NDR_ALIGN_8))

/* Get a long integer from the type/definition vector */

#define IDL_GET_LONG_FROM_VECTOR(value,address)\
    if (IDL_msp->IDL_type_vec[TVEC_INT_REP_OFFSET] != NDR_LOCAL_INT_REP){\
    idl_byte *_d = (idl_byte*)&value; \
    idl_byte *_s = (idl_byte*)(((address-(idl_byte *)0)+3) & (~3)); \
     _d[0]=_s[3]; _d[1]=_s[2]; _d[2]=_s[1]; _d[3]=_s[0]; \
    address = _s + 4;\
    }\
    else{\
    address = (idl_byte *)(((address-(idl_byte *)0)+3) & (~3)); \
    value = *(idl_ulong_int *)address; \
    address += 4; }


/* Step over a long integer from the type/definition vector */

#define IDL_DISCARD_LONG_FROM_VECTOR(address)\
{   address = (idl_byte *)(((address-(idl_byte *)0)+3) & (~3)); \
    address += 4; }

/*
 *  Signatures of interpreter modules
 */

void IDL_ENTRY rpc_ss_ndr_marsh_interp _DCE_PROTOTYPE_ ((
    idl_ulong_int, 
    idl_ulong_int, 
    rpc_void_p_t [], 
    IDL_msp_t
));

void IDL_ENTRY rpc_ss_ndr_unmar_interp _DCE_PROTOTYPE_ ((
    idl_ulong_int,
    idl_ulong_int,
    rpc_void_p_t [],
    IDL_msp_t
));


/*
 *  Operation flags (32 bits)
 *  The value of these flags must not change to guarantee upward compatibility.
 *  To guarantee endianness compatibility, IDL does not spell these values
 *  symbolically, thus IDL code must agree with these constants.
 *  The following constants are defined in rpcbase.idl:
 *      rpc_c_call_brdcst       = 0x00000001;
 *      rpc_c_call_idempotent   = 0x00000002;
 *      rpc_c_call_maybe        = 0x00000004;
 */

/*
 *  Array machinery
 */
/*  Data limit type literals and machinery */

#define IDL_LIMIT_FIXED         0
#define IDL_LIMIT_FIRST_IS      1
#define IDL_LIMIT_LAST_IS       2
#define IDL_LIMIT_LENGTH_IS     3
#define IDL_LIMIT_STRING        4
#define IDL_LIMIT_UPPER_CONF    5

#define IDL_DATA_LIMIT_PAIR_WIDTH 16

typedef struct IDL_varying_control_t {
    idl_ulong_int subslice_size;    /* Size in bytes of slice designated
                                        by foo[I1][I2]...[Im] when array was
                                        declared as foo[S1][S2]...[Sn] */
    idl_ulong_int index_value;      /* Value of Im */
} IDL_varying_control_t;

/* Bound type literals and machinery */

#define IDL_BOUND_FIXED     0
#define IDL_BOUND_MIN_IS    1
#define IDL_BOUND_MAX_IS    2
#define IDL_BOUND_SIZE_IS   3
#define IDL_BOUND_STRING    4

typedef struct IDL_bound_pair_t {
    idl_long_int lower;
    idl_long_int upper;
} IDL_bound_pair_t;

#define IDL_ALIGN_POINTER_TO_LONG(address)\
    address = (idl_byte *)(((address-(idl_byte *)0)+3) & (~3));

#define IDL_FIXED_BOUND_PAIR_WIDTH 8

#define IDL_CONF_BOUND_PAIR_WIDTH 16

/* Macro to advance a pointer over the bounds info for a conformant or open
    array. Assumes that array dimenesionality is at (3 mod 4) in
    the definition vector */

#define IDL_ADV_DEFN_PTR_OVER_BOUNDS(defn_ptr,dimensionality)\
    {defn_ptr += 16 * dimensionality;}

/* Size of base type of string - assuming defn_ptr is pointing at start of
    an appropriate data limit pair */

#define IDL_DATA_LIMIT_PAIR_CHAR_SIZE(defn_ptr)\
    ((idl_ulong_int)*(defn_ptr+9))

/*
 *  Routine vector index literals
 */

#define IDL_RTN_TO_XMIT_INDEX   0
#define IDL_RTN_FROM_XMIT_INDEX 1
#define IDL_RTN_FREE_INST_INDEX 2
#define IDL_RTN_FREE_XMIT_INDEX 3
#define IDL_RTN_NET_SIZE_INDEX      0
#define IDL_RTN_TO_NETCS_INDEX      1
#define IDL_RTN_LOCAL_SIZE_INDEX    2
#define IDL_RTN_FROM_NETCS_INDEX    3
#define IDL_RTN_OREF_TO_WIRE 0
#define IDL_RTN_WIRE_TO_OREF 1

/*
 *  Values and test for properties byte
 */

#define IDL_PROP_DEP_CHAR       0x01
#define IDL_PROP_DEP_INT        0x02
#define IDL_PROP_DEP_FLOAT      0x04
#define IDL_PROP_DEP_ALL        0x07
#define IDL_PROP_HAS_PTRS       0x08
#define IDL_PROP_MAYBE_WIRE_ALIGNED 0x10
#define IDL_PROP_TEST(prop_byte, property)\
    ((prop_byte & property) != 0)

/*
 *  Union machinery
 */

#define IDL_UNION_ARM_DESC_WIDTH 16

idl_ulong_int IDL_ENTRY rpc_ss_arm_switch_value   _DCE_PROTOTYPE_ ((
    idl_byte *,
    idl_long_int,
    IDL_msp_t
));

#define IDL_ARM_SWITCH_VALUE(defn_vec_ptr, index)\
   (IDL_msp->IDL_type_vec[TVEC_INT_REP_OFFSET] == NDR_LOCAL_INT_REP) ? \
      (*(idl_ulong_int *)(defn_vec_ptr + index * IDL_UNION_ARM_DESC_WIDTH)) : \
      rpc_ss_arm_switch_value(defn_vec_ptr, index, IDL_msp)

/*
 *  Alignment macros for NDR marshalling
 *  Note that the end of a marshalling buffer is always 8-byte aligned.
 *  Therefore if a buffer exists align bytes can always be written into it
 */

#define IDL_MARSH_ALIGN_MP(IDL_msp, alignment)\
    if (IDL_msp->IDL_buff_addr == NULL)\
        rpc_ss_ndr_marsh_init_buffer(IDL_msp);\
    IDL_msp->IDL_mp = (idl_byte *)\
        (((IDL_msp->IDL_mp - (idl_byte *)0) + (alignment-1)) & ~(alignment-1));\
    IDL_msp->IDL_left_in_buff = (IDL_msp->IDL_left_in_buff & ~(alignment-1))

/*
 *  Buffers delivered by the runtime are always 8-byte aligned multiples of
 *  8 bytes. So alignment operations never switch buffers. Note that if the
 *  buffer is not full IDL_left_in_buff will not share (mod 8) alignment
 *  characteristics with IDL_mp.
 */

#define IDL_UNMAR_ALIGN_MP(IDL_msp, alignment)\
{ \
    int advance;\
    advance = (idl_byte *)\
        (((IDL_msp->IDL_mp - (idl_byte *)0) + (alignment-1)) & ~(alignment-1)) \
            - IDL_msp->IDL_mp; \
    IDL_msp->IDL_mp += advance; \
    IDL_msp->IDL_left_in_buff -= advance;\
}

/*
 *  Description of a pointee
 */

typedef struct IDL_pointee_desc_t {
    idl_byte pointee_type;
    /* Fields only used if pointee is non-fixed array */
    idl_boolean base_type_has_pointers; /* TRUE => array of pointers or array
                                            of type containing pointers */
    idl_boolean already_unmarshalled;   /* T => pointee already unmarshalled */
                                        /* Also used for conformant struct   */
    idl_byte *array_base_defn_ptr;  /* Points at array base info */
    idl_ulong_int dimensionality;
    IDL_bound_pair_t *bounds_list;
    idl_ulong_int *Z_values;
    IDL_bound_pair_t *range_list;  /* Used only if pointee is varying or
                                                                open array */
    /* Fields used only if pointee is non-encapsulated union */
    rpc_void_p_t struct_addr;
    idl_ulong_int *struct_offset_vec_ptr;
} IDL_pointee_desc_t;

/*
 *  Literal to flag a new [unique] node or a [ref] conformant structure
 */


#ifdef SGIMIPS
#define IDL_NEW_NODE ((rpc_void_p_t)(__psint_t)(-1))
#else
#define IDL_NEW_NODE ((rpc_void_p_t)(-1))
#endif /* SGIMIPS */ 

/*
 *  MTS versions of storage management routines
 */

void IDL_ENTRY rpc_ss_mts_client_estab_alloc _DCE_PROTOTYPE_ (( volatile IDL_ms_t * ));

/*
 * Masks for testing -bug flags
 */

#define IDL_BUG_1 0x00000002
#define IDL_BUG_2 0x00000004
#define IDL_BUG_4 0x00000010

/*
 *  Machinery for pipes
 */

typedef struct IDL_pipe {
    void (IDL_ENTRY * pull)  _DCE_PROTOTYPE_ ( 
      (
       rpc_ss_pipe_state_t,
       rpc_void_p_t ,
       idl_ulong_int,
       idl_ulong_int *
      )
    );
    void (IDL_ENTRY * push)  _DCE_PROTOTYPE_ ( 
      ( 
       rpc_ss_pipe_state_t ,
       rpc_void_p_t ,
       idl_ulong_int  
      )
    );
    void (IDL_ENTRY * alloc)  _DCE_PROTOTYPE_ ( 
      ( 
       rpc_ss_pipe_state_t ,
       idl_ulong_int ,
       rpc_void_p_t *,
       idl_ulong_int *
       )
    );
    rpc_ss_pipe_state_t state;
} IDL_pipe;

/*
 *  Callee stub representation of context handle
 */

typedef struct IDL_ee_context_t {
    rpc_ss_context_t local;
    ndr_context_handle wire;
} IDL_ee_context_t;

/*
 *  Flags used in routine calls
 */

#define IDL_M_V1_ARRAY      0x01
                                /* Array with [v1_array] attribute */
#define IDL_M_IS_PARAM      0x02
                                /* Array is a parameter */
#define IDL_M_IS_STRING     0x04
                                /* Array with [string] attribute */
#define IDL_M_ADD_NULL      0x08
                          /* Explicit null terminator must be added to string */
#define IDL_M_CONF_ARRAY    0x10
                                /* Array is conformant */
#define IDL_M_DO_NOT_POINT  0x20
                                /* Do not marshall array by pointing */
#define IDL_M_FLAGS_TEST(flags_word, property)\
    ((flags_word & property) != 0)

/*
 *  Routines referenced from more than one interpreter module
 */

idl_ulong_int IDL_ENTRY rpc_ss_type_size  _DCE_PROTOTYPE_ ( 
  ( idl_byte *,IDL_msp_t )
);

void IDL_ENTRY rpc_ss_Z_values_from_bounds  _DCE_PROTOTYPE_ ( 
  (
    IDL_bound_pair_t *,
    idl_ulong_int ,
    idl_ulong_int **,
    IDL_msp_t 
  )
);

void IDL_ENTRY rpc_ss_ndr_contiguous_elt  _DCE_PROTOTYPE_ ( 
  (
    idl_ulong_int ,
    idl_ulong_int *,
    IDL_bound_pair_t *,
    idl_ulong_int ,
    idl_boolean *,
    idl_ulong_int *,
    rpc_void_p_t *
  )
);

void IDL_ENTRY rpc_ss_ndr_arr_align_and_opt  _DCE_PROTOTYPE_ ( 
  (
    IDL_mar_or_unmar_k_t ,
    idl_ulong_int ,
    idl_byte *,
    idl_byte *,
    idl_boolean *,
    IDL_msp_t 
  )
);

void IDL_ENTRY rpc_ss_ndr_marsh_init_buffer  _DCE_PROTOTYPE_ ( 
   (IDL_msp_t  )
);

idl_long_int IDL_ENTRY rpc_ss_get_typed_integer  _DCE_PROTOTYPE_ ( 
  (
    idl_byte ,
    rpc_void_p_t ,
    IDL_msp_t 
  )
);

void IDL_ENTRY rpc_ss_build_bounds_list  _DCE_PROTOTYPE_ ( 
  (            
    idl_byte **,
    rpc_void_p_t ,
    rpc_void_p_t ,
    idl_ulong_int *,
    idl_ulong_int ,
    IDL_bound_pair_t **,
    IDL_msp_t 
  )
);




void IDL_ENTRY rpc_ss_build_range_list  _DCE_PROTOTYPE_ ( 
  (
    idl_byte **,
    rpc_void_p_t ,
    rpc_void_p_t ,
    idl_ulong_int *,
    idl_ulong_int ,
    IDL_bound_pair_t *,
    IDL_bound_pair_t **,
    idl_boolean *,
    IDL_msp_t 
  )
);

void IDL_ENTRY rpc_ss_get_string_base_desc  _DCE_PROTOTYPE_ ( 
 (
    idl_byte * ,
    idl_ulong_int *, 
    idl_ulong_int *,
    IDL_msp_t 
  )
);

idl_boolean IDL_ENTRY rpc_ss_find_union_arm_defn  _DCE_PROTOTYPE_ ( 
  (
    idl_byte *_vec_ptr,
    idl_ulong_int ,
    idl_ulong_int ,
    idl_byte **   ,
    IDL_msp_t 
  )
);

void IDL_ENTRY rpc_ss_get_switch_from_data  _DCE_PROTOTYPE_ ( 
  (
    idl_ulong_int ,
    idl_byte ,
    rpc_void_p_t ,
    idl_ulong_int *,
    idl_ulong_int *,
    IDL_msp_t
  )
);

idl_boolean IDL_ENTRY rpc_ss_bug_1_thru_31  _DCE_PROTOTYPE_ ( 
  (
    idl_ulong_int ,
    IDL_msp_t 
  )
);

idl_ulong_int IDL_ENTRY rpc_ss_ndr_bug_1_align  _DCE_PROTOTYPE_ ( 
  (
    idl_byte *,
    IDL_msp_t 
  )
);

/*
 * MTS pipe state block
 */

typedef struct rpc_ss_mts_ee_pipe_state_t
{
    long pipe_number;
    long next_in_pipe;               /* if -ve, next pipe is [out] */
    long next_out_pipe;
    long *p_current_pipe;            /* +ve curr pipe is [in], -ve for [out] */
    ndr_ulong_int left_in_wire_array;
    struct IDL_ms_t *IDL_msp;        /* Pointer to interpreter state block */
    unsigned long IDL_base_type_offset;  /* Offset of pipe base type definition
                                            in type vector */
    ndr_boolean pipe_drained;         /* used only when pipe is [in] */
    ndr_boolean pipe_filled;          /* used only when pipe is [out] */
} rpc_ss_mts_ee_pipe_state_t;

void IDL_ENTRY rpc_ss_mts_init_callee_pipe  _DCE_PROTOTYPE_ ( 
  (
    long ,    /* Index of pipe in set of pipes in the
                            operation's parameter list */
    long ,     /* Index of next [in] pipe to process */
    long ,     /* Index of next [out] pipe to process */
    long *,    /* Ptr to index num and dirn of curr active pipe */
    struct IDL_ms_t *,       /* Pointer to interpreter state block */
    unsigned long ,  /* Offset of pipe base type definition
                                            in type vector */
    rpc_ss_mts_ee_pipe_state_t **
                                           /* Addr of ptr to pipe state block */
  )
);

void IDL_ENTRY rpc_ss_ndr_ee_marsh_pipe_chunk  _DCE_PROTOTYPE_ (
   (
    rpc_ss_pipe_state_t,
    rpc_void_p_t,
    idl_ulong_int
   )
);

void IDL_ENTRY rpc_ss_ndr_ee_unmar_pipe_chunk  _DCE_PROTOTYPE_ (
   (
    rpc_ss_pipe_state_t,
    rpc_void_p_t,
    idl_ulong_int,
    idl_ulong_int *
   )
);





void IDL_ENTRY rpc_ss_fixed_bounds_from_vector   _DCE_PROTOTYPE_ ((
    idl_ulong_int ,
    idl_byte *,
    IDL_bound_pair_t **,
    IDL_msp_t 
));

idl_ulong_int rpc_ss_arm_switch_value   _DCE_PROTOTYPE_ ((
    idl_byte *,
    idl_long_int,
    IDL_msp_t
));

/*
 *  IDL encoding services
 */
/* Version number of the encoding state block pointed to by a handle */

#define IDL_ES_STATE_VERSION 1

/* Version of the pickle header stored with the pickle */

#define IDL_ES_HEADER_VERSION 1

/* A encoding services handle is either "encoding" or "decoding",
    but a stub may be [encode, decode] */

typedef enum {
   IDL_encoding_k, IDL_decoding_k, IDL_both_k
} IDL_es_action_type_k_t;

/* Type of buffering - for decode non-dynamic = fixed */

typedef enum {
IDL_incremental_k, IDL_fixed_k, IDL_dynamic_k
} IDL_es_action_style_k_t;

/* An encoding dynamic buffer is built using a chain of rpc_iovector_elt's
    which describe the intermediate buffers used during encoding */

typedef struct IDL_dyn_buff_link_t {
    rpc_iovector_elt_t *IDL_p_iovec_elt;
    struct IDL_dyn_buff_link_t *IDL_next;
} IDL_dyn_buff_link_t;

/* Pickle header as a C structure */

typedef struct idl_es_pvt_header_t {
    idl_usmall_int IDL_version;	/* pickle header version (now version=1) */
    idl_usmall_int IDL_int_drep;     /* drep of pickle header */
    idl_usmall_int IDL_fill2;
    idl_usmall_int IDL_fill3;
    rpc_syntax_id_t IDL_syntax_id; /* encoding syntax used */
    rpc_if_id_t IDL_if_id;    /* uuid/version of interface */
    idl_ulong_int IDL_op_num;      /* operation number of encoding */
} idl_es_pvt_header_t;

/* If op_num in pickle header has the following value, the pickle header is
    invalid */

#define IDL_INVALID_OP_NUM 0xFFFFFFFF

typedef struct idl_es_handle_s_t
{
    idl_ulong_int IDL_version;        /* This longword must appear first,
                                        for compatibility with an existing form
                                        of pickling used by MIA */
    IDL_es_action_type_k_t IDL_action;  /* Encode or decode */
    IDL_es_action_style_k_t IDL_style;  /* Type of buffering */
    idl_boolean IDL_pickle_header_read; /* During decoding, TRUE => header for 
                                        current pickle has been read */
    idl_es_pvt_header_t IDL_pickle_header; 
            /* For encoding, the transfer syntax is set when the state block is
                created. The if_id and op_num are valid after an encoding has
                been done.
               During decoding this is a local copy of the pickle header */
    IDL_msp_t IDL_msp;              /* For encode/decode the marshalling state
                                        block is dynamically allocated */
    idl_ulong_int IDL_es_flags;     /* attribute flags */
/* Fields used for incremental buffering */

    idl_void_p_t IDL_state;  /* User state */
    idl_es_allocate_fn_t IDL_alloc;  /* User allocate routine */
    idl_es_write_fn_t IDL_write;  /* User write routine */
    idl_es_read_fn_t IDL_read;   /* User read routine */

/* Fields used for fixed buffering */

    idl_byte *IDL_buff_addr;            /* Address of user buffer */
    idl_ulong_int IDL_bsize;  /* Size of user buffer */
    idl_byte *IDL_align_buff_addr;    /* For a fixed decode, NULL unless we had 
                                        to allocate aligned storage */
/* Field used for fixed and dynamic buffering */

    idl_ulong_int *IDL_esize; /* Address to write pickle size to */

/* Fields used for dynamic buffering */

    idl_byte **IDL_p_buff_addr;	/* Address to write address of pickle to */
    IDL_dyn_buff_link_t *IDL_dyn_buff_chain_head;
                                    /* Head of chain of intermediate buffers */
    IDL_dyn_buff_link_t *IDL_dyn_buff_chain_tail;
                                    /* Tail of chain of intermediate buffers */
} IDL_es_state_t;

void IDL_ENTRY idl_es_encode_init_buffer   _DCE_PROTOTYPE_ ( 
  (
    idl_ulong_int *,
    IDL_msp_t 
  )
);

void IDL_ENTRY idl_es_encode_attach_buff  _DCE_PROTOTYPE_ ( 
  (
    IDL_msp_t 
  )
);

error_status_t IDL_ENTRY idl_es_encode_new_dyn_buff  _DCE_PROTOTYPE_ ( 
  (
    idl_ulong_int *,
    IDL_msp_t 
  )
);

void IDL_ENTRY idl_es_before_interp_call  _DCE_PROTOTYPE_ ( 
  (
    idl_es_handle_t ,      /* [in] User's encoding handle */
    rpc_if_handle_t ,    /* [in] Pointer to stub's ifspec */
    idl_byte [],    /* [in] Stub's type vector */
    idl_ulong_int ,   /* [in] operation number */
    IDL_es_action_type_k_t , /* [in] Is this operation labelled
                                            [encode], [decode] */
    idl_es_transfer_syntax_t *,
                            /* [out] Transfer syntax to use for encoding */
    IDL_msp_t
  )
);

void IDL_ENTRY idl_es_after_interp_call  _DCE_PROTOTYPE_ ( 
  (IDL_msp_t )
);

void IDL_ENTRY idl_es_clean_up  _DCE_PROTOTYPE_ ( 
  ( IDL_msp_t )
);

void IDL_ENTRY idl_es_decode_check_buffer  _DCE_PROTOTYPE_ ( 
  (  IDL_msp_t )
);

/*
 *  International character support
 */
/* Layout of element of the cs_shadow for a structure or parameter list */

typedef struct IDL_cs_shadow_elt_t {
    idl_boolean IDL_release;    /* Marshalling only - data field is pointer to
                                    storage which should be released */
    idl_cs_convert_t IDL_convert_type; /* Unmarshalling only
                                                         - conversion type */
    union {
        rpc_void_p_t IDL_storage_p; /* Pointer to wire form of [cs_char] or
                                        array of [cs_char] */
        idl_ulong_int IDL_value;    /* Transformed value of [size_is] or
                                        [length_is] of [cs_char] array */
    } IDL_data;
} IDL_cs_shadow_elt_t;

void IDL_ENTRY rpc_ss_put_typed_integer _DCE_PROTOTYPE_ (
  (
    /* in */ idl_ulong_int,
    /* in */ idl_byte,
    /* in */ rpc_void_p_t,
    IDL_msp_t
  )
);




#define IDL_GET_UUID_FROM_VECTOR(u, ptr) \
	(u).time_low =			* (unsigned32 *) ((ptr) + 0);  \
	(u).time_mid =			* (unsigned16 *) ((ptr) + 4);  \
	(u).time_hi_and_version =	* (unsigned16 *) ((ptr) + 6);  \
	(u).clock_seq_hi_and_reserved =	* (unsigned8 *)  ((ptr) + 8);  \
	(u).clock_seq_low =		* (unsigned8 *)  ((ptr) + 9);  \
	(u).node[0] =			* (idl_byte *)   ((ptr) + 10); \
	(u).node[1] =			* (idl_byte *)   ((ptr) + 11); \
	(u).node[2] =			* (idl_byte *)   ((ptr) + 12); \
	(u).node[3] =			* (idl_byte *)   ((ptr) + 13); \
	(u).node[4] =			* (idl_byte *)   ((ptr) + 14); \
	(u).node[5] =			* (idl_byte *)   ((ptr) + 15); \
	ptr += sizeof(uuid_t);


#ifdef __cplusplus
    }

/*
 * C++ support routines.
 */
void IDL_ENTRY rpc_ss_ndr_oref_to_wire_rep
(
    void        *pOref,      	    /* [in] Pointer to an object reference */
    void	*pTransmitted_data, /* [out] Pointer to wire data 	   */
    IDL_msp_t   IDL_msp     	    /* [in] Marshalling state 	 	   */
);

void IDL_ENTRY rpc_ss_ndr_wire_rep_to_oref
(
    void	*pTransmitted_data, /* [in] Pointer to wire data 	   */
    void        *pOref      	    /* [out] Object reference 		   */
);


#endif  /* __cplusplus */

enum {
	RPC_SS_NDR_MARSH_INTERFACE,
	RPC_SS_NDR_MARSH_DYN_INTERFACE,
	RPC_SS_NDR_UNMAR_INTERFACE,
	RPC_SS_NDR_UNMAR_DYN_INTERFACE,
	RPC_SS_NDR_LAST_EPV_INDEX
};


#endif /* _IDLDDEFS_H */
