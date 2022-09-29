/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: astp.h,v $
 * Revision 1.2  1994/02/21 19:01:11  jwag
 * add BB slot numbers; start support for interface slot numbers; lots of ksgen changes
 *
 * Revision 1.1  1993/08/31  06:48:01  jwag
 * Initial revision
 *
 * Revision 1.1.4.3  1993/01/03  21:37:39  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  14:32:02  bbelch]
 *
 * Revision 1.1.4.2  1992/12/23  18:42:32  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  00:59:01  zeliff]
 * 
 * Revision 1.1.2.2  1992/05/05  15:32:45  harrow
 * 	Prevent [ignore] on parameters and union members.
 * 	[1992/05/04  19:53:37  harrow]
 * 
 * Revision 1.1  1992/01/19  03:02:00  devrcs
 * 	Initial revision
 * 
 * $EndLog$
 */
/*
**
**  Copyright (c) 1989,1991 by
**      Hewlett-Packard Company, Palo Alto, Ca. &
**      Digital Equipment Corporation, Maynard, Mass.
**
**  NAME
**
**      ASTP.H
**
**  FACILITY:
**
**      Interface Definition Language (IDL) Compiler
**
**  ABSTRACT:
**
**      Header file for the ASTP.C
**
**  VERSION: DCE 1.0
**
*/

#ifndef ASTP_H
#define ASTP_H

#include <nidl.h>
#include <ast.h>

/* NDR Data Sizes for the Transmissable Types */
#define NDR_C_BOOLEAN_SIZE      1
#define NDR_C_BYTE_SIZE         1
#define NDR_C_CHARACTER_SIZE    1
#define NDR_C_SMALL_INT_SIZE    1
#define NDR_C_SHORT_INT_SIZE    2
#define NDR_C_LONG_INT_SIZE     4
#define NDR_C_HYPER_INT_SIZE    8
#define NDR_C_SHORT_FLOAT_SIZE  4
#define NDR_C_LONG_FLOAT_SIZE   8
#define NDR_C_POINTER_SIZE      4

/* MIN/MAX values for various integer sizes */
#define ASTP_C_SMALL_MAX        127
#define ASTP_C_SMALL_MIN        -128
#define ASTP_C_USMALL_MAX       255
#define ASTP_C_USMALL_MIN       0

#define ASTP_C_SHORT_MAX        32767
#define ASTP_C_SHORT_MIN        -32768
#define ASTP_C_USHORT_MAX       65535
#define ASTP_C_USHORT_MIN       0

#define ASTP_C_LONG_MAX         2147483647
#define ASTP_C_LONG_MIN         (-2147483647 - 1)
#define ASTP_C_ULONG_MAX        4294967295.0
#define ASTP_C_ULONG_MIN        0


/*
 *  Boolean Attributes Flags for Operations, Parameters, Types and Fields
 *
 *  broadcast,          - operation
 *  maybe,              - operation
 *  idempotent,         - operation
 *  in,                 - parameter
 *  out,                - parameter
 *  mutable,            - parameter
 *  context,            - parameter, type
 *  handle,             - type
 *  ignore,             - field, parameter, type
 *  ref,                - field, parameter, type
 *  small,              - field, parameter, type
 *  string,             - field, parameter, type
 *  string0,            - field, parameter, type
 *
 *  Boolean Attributes Flags for an Interface.
 *  Separated from the above attributes since
 *  their life time is the life of the interface.
 *
 *  npb         (non-parametric handle)
 *  local
 */

/*
 * ASTP (Private) definitions for the boolean attributes for operations,
 * parameters, fields and types.  Theses differ from the common definitions
 * defined in AST.H, since we don't need to create a separate operation
 * flags word.
 */

/* Operation Attributes */
#define ASTP_BROADCAST      0x00000001
#define ASTP_MAYBE          0x00000002
#define ASTP_IDEMPOTENT     0x00000004

/* Parameter-only Attributes */
#define ASTP_IN             0x00000008
#define ASTP_OUT            0x00000010
#define ASTP_IN_SHAPE       0x00000020
#define ASTP_OUT_SHAPE      0x00000040
#define ASTP_PTR            0x00000080

/* Type, Field, Parameter Attributes */
#define ASTP_STRING0        0x00000100
#define ASTP_STRING         0x00000200
#define ASTP_IGNORE         0x00000400
#define ASTP_SMALL          0x00000800
#define ASTP_CONTEXT        0x00001000

/* Type-only Attribute(s) */
#define ASTP_REF            0x00002000
#define ASTP_UNIQUE         0x00004000
#define ASTP_HANDLE         0x00008000
#define ASTP_UNALIGN        0x00010000
#define ASTP_TRANSMIT_AS    0x00020000
#define ASTP_ALIGN_SMALL    0x00040000
#define ASTP_ALIGN_SHORT    0x00080000
#define ASTP_ALIGN_LONG     0x00100000
#define ASTP_ALIGN_HYPER    0x00200000
#define ASTP_V1_ENUM        0x00400000
#ifdef sgi
/* operation only */
#define ASTP_SLOT           0x00800000
#define ASTP_BB             0x01000000
#endif

/*
 * NOTE: This bit must correspond to the Highest Attribute bit used
 * above.  It is used to check which attributes are applicable.
 */
#ifdef sgi
#define ASTP_MAX_ATTRIBUTE  0x01000000
#else
#define ASTP_MAX_ATTRIBUTE  0x00400000
#endif

/*
 * Sets of valid flags for each node type.  Note, these are the flags that can
 * potentially be set in a node.  These do not always correspond as to what is
 * allowed in the source code get reflected in the AST node built and expected
 * by the backend.  For those attributes such as [unique] [ref] [ptr] which are
 * allowed in the source in multiple locations, but are only set on types, they
 * are not specified below, but handled explicity in the builder routines.
 */
#ifdef sgi
#define ASTP_OPERATION_FLAGS ASTP_BROADCAST | ASTP_MAYBE | ASTP_IDEMPOTENT |\
	ASTP_BB | ASTP_SLOT
#else
#define ASTP_OPERATION_FLAGS ASTP_BROADCAST | ASTP_MAYBE | ASTP_IDEMPOTENT
#endif

#define ASTP_PARAMETER_FLAGS ASTP_STRING | ASTP_STRING0 |   \
            ASTP_SMALL | ASTP_CONTEXT |       \
            ASTP_IN | ASTP_OUT | ASTP_IN_SHAPE | \
            ASTP_OUT_SHAPE |  ASTP_UNIQUE | \
            ASTP_REF | ASTP_PTR

#define ASTP_FIELD_FLAGS ASTP_STRING | ASTP_STRING0 | \
            ASTP_IGNORE | ASTP_SMALL | ASTP_CONTEXT |  ASTP_UNIQUE | \
            ASTP_REF | ASTP_PTR

#define ASTP_TYPE_FLAGS ASTP_STRING | ASTP_STRING0 |  ASTP_UNIQUE | \
            ASTP_REF | ASTP_SMALL | ASTP_CONTEXT |     \
            ASTP_HANDLE | ASTP_TRANSMIT_AS | ASTP_UNALIGN | \
            ASTP_ALIGN_SMALL | ASTP_ALIGN_SHORT | ASTP_ALIGN_LONG | \
            ASTP_ALIGN_HYPER | ASTP_UNIQUE | ASTP_PTR | ASTP_V1_ENUM

#define ASTP_ARM_FLAGS ASTP_STRING | ASTP_STRING0 |  \
            ASTP_SMALL | ASTP_CONTEXT |  ASTP_UNIQUE | \
            ASTP_REF | ASTP_PTR


/*
 * ASTP (Private) definitions for the boolean attributes for an interface.
 * These are identical to the common definitions defined in AST.H.
 *
 * Note, specifying the implicit_handle attribute in the IDL means that
 * a specific handle will be defined in the ACF, so we set the NPB flag
 * (non-parametric binding) in the interface.
 */

#define ASTP_IF_PORT            0x00000040
#define ASTP_IF_UUID            0x00000080
#define ASTP_IF_VERSION         0x00000100


/*
 * Macro for manipulating interface boolean attributes.
 */
#define ASTP_CLR_IF_AF(if)        \
    (if)->fe_info->type_specific.if_flags = 0

#define ASTP_SET_IF_AF(if,attribute)       \
    (if)->fe_info->type_specific.if_flags |= (attribute)

#define ASTP_IF_AF_SET(if,attribute)       \
    (((if)->fe_info->type_specific.if_flags & (attribute)) != 0)


/*
 * Macros for checking and clearing attributes in an ASTP_attr_flag_t structure.
 */
#define ASTP_TEST_ATTR(__attr_ptr,__flags)   \
    ((((__attr_ptr)->attr_flags) & (__flags)) != 0)

#define ASTP_CLR_ATTR(__attr_ptr,__flags)   \
    (__attr_ptr)->attr_flags &= (~(__flags))


/* AST Private structure types */

/*
 * Structure for attribute flags
 */
typedef unsigned long   ASTP_attr_flag_t;

/*
 * Structure for handling both array_bounds and attribute flags in the
 * grammar as a unit.
 */
typedef struct ASTP_attributes_t
{
    struct ASTP_type_attr_n_t       *bounds;
    ASTP_attr_flag_t                attr_flags; /* values are the ASTP_xxx  */
                                                /* flags above              */
} ASTP_attributes_t;


/*
 * Structure for the singly-linked list used to build the AST.
 *
 * Note, since next and last are not guarenteed for all nodes,
 * but fe_info, and be_info are, fe_info and be_info are always
 * the first two fields, followed by the next and last fields
 * when present.
 */

typedef struct ASTP_node_t
{
    fe_info_t           *fe_info;
    be_info_t           *be_info;
    struct ASTP_node_t  *next;
    struct ASTP_node_t  *last;
} ASTP_node_t, *ASTP_node_t_p;


typedef enum
{
    ASTP_constant_bound,
    ASTP_default_bound,
    ASTP_open_bound
} ASTP_bound_t;

/*
 * Array Index private node.
 *
 * Keeps the lower and upper bounds
 * to a dimension of an array as well
 * the type of each bound.
 */
typedef struct ASTP_array_index_n_t
{
    fe_info_t           *fe_info;       /* Must be here, but unused */
    be_info_t           *be_info;       /* Must be here, but unused */
    struct ASTP_array_index_n_t *next;
    struct ASTP_array_index_n_t *last;
    ASTP_bound_t     lower_bound_type;
    ASTP_bound_t     upper_bound_type;
    AST_constant_n_t *lower_bound;
    AST_constant_n_t *upper_bound;
} ASTP_array_index_n_t;

/*
 *  Type attribute node.
 *
 *  A Type attribute node is used to hold the
 *  attributes for a type during the parse.
 */

typedef enum
{
    last_is_k,
    first_is_k,
    max_is_k,
    min_is_k,
    length_is_k,
    size_is_k
} ASTP_attr_k_t;

typedef struct ASTP_type_attr_n_t
{
    fe_info_t           *fe_info;       /* Must be here, but unused */
    be_info_t           *be_info;       /* Must be here, but unused */
    struct ASTP_type_attr_n_t *next;
    struct ASTP_type_attr_n_t *last;
    int             source_line;
    ASTP_attr_k_t   kind;
    NAMETABLE_id_t  name;
    boolean         pointer;
} ASTP_type_attr_n_t;

/*
 * Context block to save the field attribute
 * reference information.
 * This is used when processing a parameter
 * field attribute and the referent parameter
 * is not yet defined.
 */

typedef struct ASTP_field_ref_ctx_t
{
    fe_info_t           *fe_info;       /* Must be here, but unused */
    be_info_t           *be_info;       /* Must be here, but unused */
    struct ASTP_field_ref_ctx_t *next;
    struct ASTP_field_ref_ctx_t *last;
    NAMETABLE_id_t    name;             /* Saved parameter reference */
    AST_field_ref_n_t *field_ref_ptr;   /* Address to field ref needing patch */
} ASTP_field_ref_ctx_t;

/*
 *  A declarator node is used hold the information about a IDL/C declarator
 *  which will eventually be merged with the type node and turned into an
 *  export.  A declarator node is private to the AST builder which is why it is
 *  not declared in AST.H.  If the next_op field is NULL then this is a simple
 *  declarator otherwise the various operations must be applied to the base type to
 *  yeild the result type described via this declarator.
 */

typedef struct ASTP_declarator_n_t
{
    fe_info_t           *fe_info;       /* Points to fe_info of parent node */
                                        /* which determines the type of     */
                                        /* the expected referent            */
    be_info_t           *be_info;       /* Must be here, but unused */
    struct ASTP_declarator_n_t *next;
    struct ASTP_declarator_n_t *last;
    NAMETABLE_id_t      name;
    struct ASTP_declarator_op_n_t   *next_op;
    struct ASTP_declarator_op_n_t   *last_op;
} ASTP_declarator_n_t;


/*
 * Representation of operations that can be performed on declarators to produce
 * various types (pointer, array, function_pointer).
 */
typedef struct ASTP_declarator_op_n_t
{
    struct ASTP_declarator_op_n_t *next_op;
    AST_type_k_t        op_kind;    /* Valid operators are: AST_array_k,    */
                                    /* AST_pointer_k, or AST_function_k */
    union
    {
        ASTP_node_t             *node;
        ASTP_array_index_n_t    *indices;       /* Set if op_kind == AST_array_k */
        AST_type_n_t            *type_node_ptr;
        AST_parameter_n_t       *routine_params;/* Set if op_kind == AST_function_k */
        int                     pointer_count;  /* Set if op_kind == AST_pointer_k */
    } op_info;
} ASTP_declarator_op_n_t;


/*
 * A Tag ref node is created for each forward reference to a
 * struct/union via it's tag name (e.g.  struct foo).  It
 * provides enough information to patch references to the
 * type after the parse has completed.
 */
typedef struct ASTP_tag_ref_n_t
{
    fe_info_t           *fe_info;       /* Must be here, but unused */
    be_info_t           *be_info;       /* Must be here, but unused */
    struct ASTP_tag_ref_n_t *next;
    struct ASTP_tag_ref_n_t *last;
    NAMETABLE_id_t      name;           /* Tag name referenced */
    AST_type_k_t        ref_kind;       /* AST_struct_k or AST_disc_union_k */
    AST_type_n_t        *type_node_ptr; /* Type node that referenced tag */
} ASTP_tag_ref_n_t;


/*
 * Structure to hold count of input/output parameters needing marshalling
 * used to formulate operation synthesized attributes AST_HAS_INS/AST_HAS_OUTS.
 */
typedef struct ASTP_parameter_count_t
{
    int  in_params;
    int  out_params;
} ASTP_parameter_count_t;


/*
 * Type used to evaluate integer constant expressions
 */
typedef struct ASTP_exp_n_t {
    AST_constant_k_t    type;     /* datatype of the expression */
    union {
        int              integer;         /* Integer value         */
        AST_constant_n_t *other;          /* Constant node         */
    } val;
} ASTP_exp_n_t;   /* const expression block */



/*
 *  Interface Attributes
 */
extern int              interface_pointer_class;


/*
 *  Operation, Parameter, Type Attributes
 */
extern AST_type_n_t         *ASTP_transmit_as_type;


/*
 *  Interface just parsed
 */
extern AST_interface_n_t *the_interface;

/*
 * List head for saved context for field
 * attributes forward referenced parameters.
 */
extern ASTP_field_ref_ctx_t *ASTP_field_ref_ctx_list;

/*
 * List head for referenced struct/union tags.
 */
extern ASTP_tag_ref_n_t *ASTP_tag_ref_list;

/*
 *  Control for parser
 */
extern boolean ASTP_parsing_main_idl;   /* True when parsing the main idl */
extern int  yylineno;                   /* Current line number */

/*
 *  Builtin in constants
 */

extern AST_constant_n_t *zero_constant_p;

/*
 * Builtin base types
 */
extern AST_type_n_t     *ASTP_char_ptr,
                        *ASTP_boolean_ptr,
                        *ASTP_byte_ptr,
                        *ASTP_void_ptr,
                        *ASTP_handle_ptr,
                        *ASTP_short_float_ptr,
                        *ASTP_long_float_ptr,
                        *ASTP_small_int_ptr,
                        *ASTP_short_int_ptr,
                        *ASTP_long_int_ptr,
                        *ASTP_hyper_int_ptr,
                        *ASTP_small_unsigned_ptr,
                        *ASTP_short_unsigned_ptr,
                        *ASTP_long_unsigned_ptr,
                        *ASTP_hyper_unsigned_ptr;

/* Default tag for union */
extern NAMETABLE_id_t   ASTP_tagged_union_id;

/*
 * Function prototypes exported by ASTP_{COM/GBL/DMP/CPX/SIM}.C
 */
ASTP_type_attr_n_t *AST_array_bound_info(
#ifdef PROTO
    NAMETABLE_id_t name,
    ASTP_attr_k_t kind,
    boolean is_pointer
#endif
);

void AST_capture_operation_attrs(
#ifdef PROTO
    void
#endif
);

AST_constant_n_t *AST_clone_constant(
#ifdef PROTO
    AST_constant_n_t *constant_node_p
#endif
);

ASTP_node_t *AST_concat_element(
#ifdef PROTO
    ASTP_node_t *list_head,
    ASTP_node_t *element
#endif
);

AST_constant_n_t *AST_constant_node(
#ifdef PROTO
    AST_constant_k_t kind
#endif
);

ASTP_declarator_n_t *AST_declarator_node(
#ifdef PROTO
    NAMETABLE_id_t name
#endif
);

void AST_declarator_operation(
#ifdef PROTO
    ASTP_declarator_n_t     *declarator,
    AST_type_k_t            op_kind,
    ASTP_node_t             *op_info,
    int                     pointer_count
#endif
);

AST_parameter_n_t *AST_declarator_to_param(
#ifdef PROTO
    ASTP_attributes_t   *attributes,
    AST_type_n_t *type,
    ASTP_declarator_n_t *declarator
#endif
);

AST_type_p_n_t *AST_declarators_to_types(
#ifdef PROTO
    AST_type_n_t        *type_ptr,
    ASTP_declarator_n_t *declarators_ptr,
    ASTP_attributes_t   *attributes
#endif
);

AST_export_n_t *AST_export_node(
#ifdef PROTO
    ASTP_node_t *export_ptr,
    AST_export_k_t kind
#endif
);

AST_include_n_t *AST_include_node(
#ifdef PROTO
    STRTAB_str_t include_file
#endif
);

AST_import_n_t *AST_import_node(
#ifdef PROTO
    STRTAB_str_t imported_file
#endif
);

void AST_init(
#ifdef PROTO
        void
#endif
);

AST_constant_n_t *AST_integer_constant(
#ifdef PROTO
    long int value
#endif
);

AST_interface_n_t *AST_interface_node(
#ifdef PROTO
      void
#endif
);

void AST_finish_interface_node(
#ifdef PROTO
    AST_interface_n_t *interface_node
#endif
);

AST_operation_n_t *AST_operation_node(
#ifdef PROTO
    AST_type_n_t *result_type,
    ASTP_declarator_n_t *declarator,
    ASTP_attributes_t   *attributes
#endif
);

AST_parameter_n_t * AST_parameter_node(
#ifdef PROTO
    NAMETABLE_id_t identifier
#endif
);

AST_rep_as_n_t *AST_represent_as_node(
#ifdef PROTO
    NAMETABLE_id_t name
#endif
);


AST_type_n_t *AST_set_type_attrs(
#ifdef PROTO
    AST_type_n_t        *type_node_ptr,
    ASTP_attributes_t   *attributes
#endif
);

void AST_set_ports(
#ifdef PROTO
    AST_interface_n_t *interface_node_p
#endif
);

AST_type_n_t *AST_type_node(
#ifdef PROTO
    AST_type_k_t kind
#endif
);

AST_type_p_n_t *AST_type_ptr_node(
#ifdef PROTO
    void
#endif
);

AST_export_n_t *AST_types_to_exports(
#ifdef PROTO
    AST_type_p_n_t *type_p_list
#endif
);


AST_array_n_t *AST_array_node(
#ifdef PROTO
    AST_type_n_t *element_type_ptr
#endif
);

AST_array_index_n_t *AST_array_index_node(
#ifdef PROTO
    unsigned short array_size
#endif
);


ASTP_array_index_n_t *ASTP_array_index_node(
#ifdef PROTO
    AST_constant_n_t *lower_bound,
    ASTP_bound_t lower_bound_type,
    AST_constant_n_t *upper_bound,
    ASTP_bound_t upper_bound_type
#endif
);

AST_constant_n_t *AST_boolean_constant(
#ifdef PROTO
    boolean value
#endif
);

AST_case_label_n_t *AST_case_label_node(
#ifdef PROTO
    AST_constant_n_t *case_label
#endif
);

AST_constant_n_t *AST_char_constant(
#ifdef PROTO
    char value
#endif
);


AST_arm_n_t *AST_declarator_to_arm(
#ifdef PROTO
    AST_type_n_t *type_ptr,
    ASTP_declarator_n_t *declarator,
    ASTP_attributes_t   *attributes
#endif
);

AST_field_n_t *AST_declarators_to_fields(
#ifdef PROTO
    ASTP_declarator_n_t *declarators_ptr,
    AST_type_n_t        *type_ptr,
    ASTP_attributes_t   *attributes
#endif
);

AST_case_label_n_t *AST_default_case_label_node(
#ifdef PROTO
    void
#endif
);

AST_type_n_t *AST_disc_union_node(
#ifdef PROTO
    NAMETABLE_id_t tag_name,
    NAMETABLE_id_t union_name,
    NAMETABLE_id_t disc_name,
    AST_type_n_t *disc_type,
    AST_arm_n_t *arms_list
#endif
);

AST_type_n_t *AST_enumerator_node(
#ifdef PROTO
    AST_constant_n_t *constant_list,
    AST_type_k_t size
#endif
);

AST_constant_n_t *AST_enum_constant(
#ifdef PROTO
    NAMETABLE_id_t identifier
#endif
);


AST_constant_n_t *AST_finish_constant_node(
#ifdef PROTO
    AST_constant_n_t *constant_ptr,
    ASTP_declarator_n_t *declarator,
    AST_type_n_t *type_ptr
#endif
);

AST_field_attr_n_t *AST_field_attr_node(
#ifdef PROTO
    void
#endif
);

AST_field_ref_n_t *AST_field_ref_node(
#ifdef PROTO
    unsigned short dimension
#endif
);

AST_operation_n_t *AST_function_node(
#ifdef PROTO
    AST_type_n_t          *result_type,
    NAMETABLE_id_t        op_name,
    AST_parameter_n_t *parameters
#endif
);

AST_arm_n_t *AST_label_arm(
#ifdef PROTO
    AST_arm_n_t *member,
    AST_case_label_n_t *case_labels
#endif
);

AST_type_n_t *AST_lookup_integer_type_node(
#ifdef PROTO
      AST_type_k_t    int_size,
      int             int_signed
#endif
);

AST_type_n_t *AST_lookup_type_node(
#ifdef PROTO
    AST_type_k_t kind
#endif
);

AST_type_n_t *AST_lookup_named_type(
#ifdef PROTO
    NAMETABLE_id_t type_name
#endif
);

AST_constant_n_t *AST_named_constant(
#ifdef PROTO
    NAMETABLE_id_t const_name
#endif
);

AST_constant_n_t *AST_null_constant(
#ifdef PROTO
    void
#endif
);

AST_type_n_t *AST_pipe_node(
#ifdef PROTO
    AST_type_n_t *pipe_type
#endif
);

AST_pointer_n_t *AST_pointer_node(
#ifdef PROTO
    AST_type_n_t * pointee
#endif
);

AST_type_n_t *AST_set_union_arms(
#ifdef PROTO
    AST_type_n_t *union_type_ptr,
    AST_arm_n_t *member_list
#endif
);

AST_constant_n_t *AST_string_constant(
#ifdef PROTO
    STRTAB_str_t value
#endif
);

AST_type_n_t *AST_structure_node(
#ifdef PROTO
    AST_field_n_t *field_list,
    NAMETABLE_id_t identifier
#endif
);

AST_field_n_t *AST_tag_declarators_to_fields(
#ifdef PROTO
    ASTP_declarator_n_t *declarators,
    NAMETABLE_id_t      identifier,
    ASTP_attributes_t   *attributes,
    AST_type_k_t        kind
#endif
);

AST_type_n_t *AST_type_from_tag(
#ifdef PROTO
    AST_type_k_t kind,
    NAMETABLE_id_t identifier
#endif
);

void AST_set_type_boolean_attrs(
#ifdef PROTO
    AST_type_n_t *type_node
#endif
);

void AST_clear_type_attrs(
#ifdef PROTO
    void
#endif
);

AST_field_attr_n_t *AST_set_field_attrs(
#ifdef PROTO
    ASTP_attributes_t  *attributes,
    ASTP_node_t *parent_node,
    AST_type_n_t *type_node
#endif
);

void ASTP_free_declarators(
#ifdef PROTO
    ASTP_declarator_n_t *declarators_ptr
#endif
);

void ASTP_free_simple_list(
#ifdef PROTO
    ASTP_node_t *list_ptr
#endif
);

AST_type_n_t *AST_propagate_type(
#ifdef PROTO
    AST_type_n_t *type_node_ptr,
    ASTP_declarator_n_t *declarator_ptr,
    ASTP_attributes_t *attributes,
    ASTP_node_t *parent_node
#endif
);

void ASTP_add_name_binding(
#ifdef PROTO
    NAMETABLE_id_t name,
    char *AST_node
#endif
);

AST_type_n_t *ASTP_chase_ptr_to_kind(
#ifdef PROTO
    AST_type_n_t *type_node,
    AST_type_k_t kind
#endif
);

AST_type_n_t *ASTP_chase_ptr_to_type(
#ifdef PROTO
    AST_type_n_t *type_node
#endif
);

ASTP_node_t *ASTP_lookup_binding(
#ifdef PROTO
    NAMETABLE_id_t      name,
    fe_node_k_t         node_kind,
    boolean             noforward_ref
#endif
);

void ASTP_patch_tag_references(
#ifdef PROTO
    AST_interface_n_t *interface_node_ptr
#endif
);

void ASTP_patch_field_reference(
#ifdef PROTO
    void
#endif
);

void ASTP_set_fe_info(
#ifdef PROTO
    ASTP_node_t *node_ptr,
    fe_node_k_t fe_node_kind
#endif
);

void ASTP_save_tag_ref(
#ifdef PROTO
    NAMETABLE_id_t      identifier,
    AST_type_k_t        kind,
    AST_type_n_t        *type_node_ptr
#endif
);

void ASTP_process_pa_type(
#ifdef PROTO
    AST_type_n_t *type_node_ptr
#endif
);

void AST_set_flags(
#ifdef PROTO
    AST_flags_t         *flags,
    ASTP_node_t         *node_ptr,
    ASTP_attributes_t   *attributes
#endif
);

long AST_attribute_to_token(
#ifdef PROTO
    ASTP_attr_flag_t    *attribute
#endif
);

AST_arm_n_t *AST_arm_node(
#ifdef PROTO
    NAMETABLE_id_t name,
    AST_case_label_n_t *label,
    AST_type_n_t *type
#endif
);

void ASTP_parse_port(
#ifdef PROTO
      AST_interface_n_t   *interface_p,
      STRTAB_str_t        port_string
#endif
);

void ASTP_validate_forward_ref(
#ifdef PROTO
    AST_type_n_t *type
#endif
);

NAMETABLE_id_t AST_generate_name(
#ifdef PROTO
    AST_interface_n_t *int_p,
    char              *suffix
#endif
);

void ASTP_validate_integer(
#ifdef PROTO
      ASTP_exp_n_t *expression
#endif
);

void KEYWORDS_init(
#ifdef PROTO
    void
#endif
);

char *KEYWORDS_lookup_text(
#ifdef PROTO
    long token
#endif
);

int KEYWORDS_screen(
#ifdef PROTO
    char * identifier,
    NAMETABLE_id_t * id
#endif
);

void ASTP_set_array_rep_type
(
#ifdef PROTO
    AST_type_n_t        *type_node_ptr,
    AST_type_n_t        *array_base_type,
    boolean             is_varying
#endif
);

#endif
