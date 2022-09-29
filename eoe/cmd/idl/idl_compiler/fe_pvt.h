/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: fe_pvt.h,v $
 * Revision 1.1  1993/08/31 06:48:01  jwag
 * Initial revision
 *
 * Revision 1.1.2.3  1993/01/03  21:39:20  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  14:34:43  bbelch]
 *
 * Revision 1.1.2.2  1992/12/23  18:46:28  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  01:01:59  zeliff]
 * 
 * Revision 1.1  1992/01/19  03:02:32  devrcs
 * 	Initial revision
 * 
 * $EndLog$
 */
/*
**
**  Copyright (c) 1989 by
**      Hewlett-Packard Company, Palo Alto, Ca. &
**      Digital Equipment Corporation, Maynard, Mass.
**
**
**  NAME:
**
**      fe_pvt.h
**
**  FACILITY:
**
**      Interface Definition Language (IDL) Compiler
**
**  ABSTRACT:
**
**  Header file containing defining front-end private data structures
**  for data that is kept in the fe_info_t field of the AST nodes.
**
**  VERSION: DCE 1.0
**
*/

#ifndef fe_pvth_incl
#define fe_pvth_incl

#include <nametbl.h>

/*
 * The frontend structure describes information private to
 * the frontend portion of the compiler.
 */

typedef enum
{
    fe_bitset_info,
    fe_const_info,
    fe_source_only,
    fe_tag_fwd_ref,
    fe_clone_info,
    fe_if_info,
    fe_ptr_info
} fe_type_k_t;

typedef enum            /* Integer constant kinds */
{
    fe_int_const_k,
    fe_bitset_const_k,
    fe_enum_const_k
}   fe_const_k_t;

typedef enum            /* AST node kinds */
{
    fe_interface_n_k,
    fe_type_p_n_k,
    fe_import_n_k,
    fe_export_n_k,
    fe_constant_n_k,
    fe_type_n_k,
    fe_rep_as_n_k,
    fe_array_n_k,
    fe_array_index_n_k,
    fe_bitset_n_k,
    fe_disc_union_n_k,
    fe_arm_n_k,
    fe_case_label_n_k,
    fe_enumeration_n_k,
    fe_pipe_n_k,
    fe_pointer_n_k,
    fe_structure_n_k,
    fe_field_n_k,
    fe_field_attr_n_k,
    fe_field_ref_n_k,
    fe_parameter_n_k,
    fe_operation_n_k,
    fe_include_n_k
} fe_node_k_t;

/*
 * Bit names contained in fe_info flags word
 */

#define FE_SELF_POINTING        0x00000001      /* True if this type node   */
                                                /* is on the sp_types list. */
                                                /* Needed because non self- */
                                                /* pointing types can be on */
                                                /* the list also            */

#define FE_POINTED_AT           0x00000002      /* True if this type node   */
                                                /* is on the pa_types list  */

#define FE_HAS_PTR_ARRAY        0x00000004      /* True if type contains a  */
                                                /* [ptr] array used as other*/
                                                /* than a top-level param   */

#define FE_INCOMPLETE           0x00000008      /* True if this             */
                                                /* struct/union is not yet  */
                                                /* complete due to a        */
                                                /* forward reference        */

#define FE_HAS_UNIQUE_PTR       0x00000010      /* True if this type or any */
                                                /* types it pts to have any */
                                                /* [unique] ptrs            */

#define FE_HAS_PTR              0x00000020      /* True if this type or any */
                                                /* contained types are ptrs */

#define FE_HAS_CFMT_ARR         0x00000040      /* True if this param       */
                                                /* contains a non-top-level */
                                                /* conformant array that is */
                                                /* not under a full pointer */

#define FE_PROP_TYPE_DONE       0x00000080      /* True if this item has    */
                                                /* had type propagation     */
                                                /* completed                */

#define FE_HAS_REF_PTR          0x00000100      /* True if this type or any */
                                                /* types it pts to have any */
                                                /* [ref] ptrs               */

#define FE_PROP_IN_PARAM_DONE   0x00000200      /* True if this item has    */
                                                /* had [in] param           */
                                                /* propagation completed    */

#define FE_HAS_REP_AS           0x00000400      /* True if this item has    */
                                                /* or contains a type with  */
                                                /* [represent_as] on it     */

#define FE_OOL                  0x00000800      /* True if this item has    */
                                                /* been put on the ool list */

#define FE_HAS_VAL_FLOAT        0x00001000      /* True if operation has    */
                                                /* float passed by value    */

#define FE_HAS_V1_ATTR          0x00002000      /* True if type has or cts. */
                                                /* a V1-specific attribute  */

#define FE_HAS_V2_ATTR          0x00004000      /* True if type has or cts. */
                                                /* a V2-specific attribute  */

#define FE_PROP_OUT_PARAM_DONE  0x00008000      /* True if this item has    */
                                                /* had [out] param          */
                                                /* propagation completed    */

#define FE_HAS_FULL_PTR         0x00010000      /* True if this type or any */
                                                /* types it pts to have any */
                                                /* [ref] ptrs               */

/*
 * Macros to set, clear, and test fe_info flags word
 */
#define FE_SET(word,bit)    ((word) |= (bit))
#define FE_TEST(word,bit)   (((word) & (bit)) != 0)
#define FE_CLEAR(word,bit)  ((word) &= ~(bit))


/*
 * Info in the AST used only by the frontend.
 */
typedef struct fe_info_t
{
    struct fe_info_t *next;
    struct fe_info_t *last;
    fe_node_k_t      node_kind;         /* AST node kind */
    STRTAB_str_t     file;
    int              source_line;
    STRTAB_str_t     acf_file;          /* ACF file for this node or NIL */
    int              acf_source_line;   /* ACF line number or 0 if none */
    fe_type_k_t      fe_type_id;
    union
    {
        int             cardinal;       /* For bitsets and enumerations */
        fe_const_k_t    const_kind;     /* fe_type_id == fe_const_info */
        struct AST_type_p_n_t *clone;   /* fe_type_id == fe_clone_info */
        int             if_flags;       /* fe_type_id == fe_if_info */
        struct AST_type_n_t *pointer_type; /* fe_type_id == fe_ptr_info */
    } type_specific;

    struct AST_type_n_t *tag_ptr;       /* Type node for the tag from which */
                                        /* this type was derived.           */
    NAMETABLE_id_t      tag_name;       /* Tag name from which this type is */
                                        /* derived.                         */
    unsigned long int   gen_index;      /* Index used in gen'd names        */
    short int           pointer_count;  /* The number of pointers on a      */
                                        /* array bound reference.           */
    unsigned long int  flags;           /* A bitvector of flags */

    struct AST_type_n_t *original;      /* Type node for a type with        */
                                        /* DEF_AS_TAG set                   */
} fe_info_t;

#endif
