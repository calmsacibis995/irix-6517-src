/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: flatten.c,v $
 * Revision 1.1  1993/08/31 06:48:01  jwag
 * Initial revision
 *
 * Revision 1.1.4.3  1993/01/03  21:39:28  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  14:34:56  bbelch]
 *
 * Revision 1.1.4.2  1992/12/23  18:46:45  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  01:02:12  zeliff]
 * 
 * Revision 1.1.2.5  1992/05/26  19:49:27  harrow
 * 	Correct code generation error (missing calls to represent_as conversion
 * 	routines) when using [out_of_line] and [represent_as] on the same type.
 * 	[1992/05/26  15:39:52  harrow]
 * 
 * Revision 1.1.2.4  1992/05/13  19:03:22  harrow
 * 	     Correct code generation when xmitted type of [transmit_as] is string and
 * 	     [transmit_as] on a field of an [out_of_line] structure.
 * 	     [1992/05/13  11:24:50  harrow]
 * 
 * Revision 1.1.2.3  1992/05/05  15:38:01  harrow
 * 	          Correct processing of [string] attribute when multiple levels of
 * 	          pointer are involved.
 * 	          [1992/05/05  13:29:50  harrow]
 * 
 * Revision 1.1.2.2  1992/03/24  14:10:04  harrow
 * 	               In DCE V1.0, arrays of [ref] pointers never had a representation on the
 * 	               wire.  For contained arrays of [ref] pointers, however, the
 * 	               correct NDR is to ship holes for the pointers.  If
 * 	               BE_bug_array_no_ref_hole is specified, the user has
 * 	               us to recreate that incorrect NDR.  Otherwise, suppress
 * 	               the hole only for those arrays that are not contained within
 * 	               other types (not a structure field).
 * 	               [1992/03/19  15:23:46  harrow]
 * 
 * Revision 1.1  1992/01/19  03:01:10  devrcs
 * 	               Initial revision
 * 
 * $EndLog$
 */
/*
**
**  Copyright (c) 1989, 1990, 1991 by
**      Hewlett-Packard Company, Palo Alto, Ca. &
**      Digital Equipment Corporation, Maynard, Mass.
**
**
**  NAME:
**
**      flatten.c
**
**  FACILITY:
**
**      IDL Compiler Backend
**
**  ABSTRACT:
**
**  Routines for preparing parameter lists for marshalling
**
**  VERSION: DCE 1.0
**
**  MODIFICATION HISTORY:
**   
**  13-Feb-92   harrow      Add support for bug 4, ship holes for arrays of
**                          ref pointers when contained within other types.
*/

#include <nidl.h>
#include <ast.h>
#include <bedeck.h>
#include <dutils.h>
#include <dflags.h>
#include <flatten.h>
#include <backend.h>
#include <localvar.h>
#include <oolrtns.h>
#include <dutils.h>

#define Append(list, item)\
    if (list == NULL) list = item;\
    else {list->last->next = item; list->last = list->last->next->last;}

#define Prepend(list, item)\
    if (list == NULL) list = item;\
    else {item->last = list->last; item->next = list; list = item;}

#define CTX_HANDLE_TYPE_NAME "ndr_context_handle" /* type name in nbase.idl */

/*
 * A space-saving macro hack for flatten_field_attributes:
 * The 'which' field here will take on the value
 * 'first_is_vec', 'last_is_vec', 'length_is_vec', 'min_is_vec', 'max_is_vec', or
 * 'size_is_vec'
 */
#define Flatten_Field_Ref(attrp, paramp, which, dims)\
if (paramp->field_attrs->which && paramp->field_attrs->which->valid)\
{\
    int i;\
    attrp->which = (AST_field_ref_n_t *)BE_ctx_malloc(dims*sizeof(AST_field_ref_n_t));\
    attrp->which->be_info = paramp->field_attrs->which->be_info;\
    attrp->which->valid = 1;\
    attrp->which->ref.p_ref =\
        (BE_PI_Flags(paramp) & BE_FIELD) ?\
        paramp->field_attrs->which->ref.f_ref->be_info.field->flat :\
        paramp->field_attrs->which->ref.p_ref->be_info.param->flat;\
    for (i=1; i < dims; i++) attrp->which[i].valid = 0;\
}

/*
 * copy_parameter
 *
 * Allocates and returns a copy of a parameter with name and type fields
 * as passed.
 */
static AST_parameter_n_t *copy_parameter
#ifdef PROTO
(
    AST_parameter_n_t *param,
    NAMETABLE_id_t name,
    AST_type_n_t *type
)
#else
(param, name, type)
    AST_parameter_n_t *param;
    NAMETABLE_id_t name;
    AST_type_n_t *type;
#endif
{
    AST_parameter_n_t *copy =
        (AST_parameter_n_t *)BE_ctx_malloc(sizeof(AST_parameter_n_t));
    *copy = *param;
    copy->name = name;
    copy->type = type;
    copy->next = NULL;
    copy->last = copy;
    return copy;
}

/*
 * new_param_info
 *
 * Allocates and initializes a BE_param_i_t
 */
static BE_param_i_t *new_param_info
#ifdef PROTO
(void)
#else
()
#endif
{
    BE_param_i_t *new = (BE_param_i_t *)BE_ctx_malloc(sizeof(BE_param_i_t));

    new->flags = 0;
    new->call_before = new->call_after = NULL;

    return new;
}

/*
 * BE_new_ool_info
 *
 */
BE_ool_i_t *BE_new_ool_info
#ifdef PROTO
(
    AST_parameter_n_t *param,
    AST_type_n_t *type,
    NAMETABLE_id_t name,
    boolean any_pointers,
    boolean use_P_rtn,
    BE_call_rec_t *calls_before,
    boolean top_level
)
#else
(param, type, name, any_pointers, use_P_rtn, calls_before, top_level)
    AST_parameter_n_t *param;
    AST_type_n_t *type;
    NAMETABLE_id_t name;
    boolean any_pointers;
    boolean use_P_rtn;
    BE_call_rec_t *calls_before;
    boolean top_level;
#endif
{
    BE_ool_i_t * ool_info_p;
    BE_call_rec_t *call;

    ool_info_p = (BE_ool_i_t *)BE_ctx_malloc(sizeof(BE_ool_i_t));
    ool_info_p->type = type;
    ool_info_p->name = name;

    ool_info_p->any_pointers = any_pointers;
    ool_info_p->use_P_rtn = use_P_rtn;
    ool_info_p->top_level = top_level;
    ool_info_p->has_calls_before = (calls_before != NULL);

    ool_info_p->call_type = NULL;
    ool_info_p->call_name = NULL;

    for (call = calls_before; call != NULL; call = call->next)
    {
        if (call->type == BE_call_xmit_as)
        {
            ool_info_p->call_type = call->call.xmit_as.xmit_type;
            ool_info_p->call_name = call->call.xmit_as.pxmit_name;
        }
        else if (call->type == BE_call_rep_as)
        {
            ool_info_p->call_type = call->call.rep_as.net_type;
            ool_info_p->call_name = call->call.rep_as.pnet_name;
        }
    }

    if (ool_info_p->call_type == NULL)
    {
        ool_info_p->call_type = type;
        ool_info_p->call_name = name;
    }

    return ool_info_p;
}

/*
 * BE_yank_pointers
 *
 * Scans a parameter list for pointers.  Copies and cons'es those parameters
 * together and returns them.  Marks the original fields with BE_DEFER.  Also
 * duplicates arrays containing pointers.  Marks the duplicate arrays
 * BE_REF_PASS.  The elements for arrays returned will only contain pointees
 * or BE_REF_PASS arrays.  Removes call_after stuff from the original and
 * call_before stuff from the referent.
 */
AST_parameter_n_t *BE_yank_pointers
#ifdef PROTO
(
    AST_parameter_n_t *list
)
#else
(list)
    AST_parameter_n_t *list;
#endif
{
    AST_parameter_n_t *first, *prev, *param, *referent, *yanked = NULL;
    BE_arm_t *arm, *new_arm;
    boolean any_union_ptrs;

    first = list;

    for (param = list; param; param = param->next)
    {
        if (BE_PI_Flags(param) & BE_OOL_YANK_ME)
        {
            referent = copy_parameter(param, param->name, param->type);
            referent->be_info.param = new_param_info();
            *(referent->be_info.param) = *(param->be_info.param);
            BE_Ool_Info(referent) =
                BE_new_ool_info(param,
                    BE_Ool_Info(param)->type,
                    BE_Ool_Info(param)->name,
                    true, true, NULL, BE_Ool_Info(param)->top_level);
            BE_Calls_After(param) = NULL;
            BE_Calls_Before(referent) = NULL;
            Append(yanked, referent);
        }
        else if (param->type->kind == AST_pointer_k)
        {
            /*
             * Pointers: duplicate, marking the original BE_DEFER
             */
                referent = copy_parameter(param, param->name, param->type);

                /*
                 * Create a new be_info because the BE_DEFER flags will
                 * differ between the actual pointer and its yanked counterpart
                 */
                referent->be_info.param = new_param_info();
                *(referent->be_info.param) = *(param->be_info.param);
                BE_Calls_After(param) = NULL;
                BE_Calls_Before(referent) = NULL;
                Append(yanked, referent);
                BE_PI_Flags(param) |= BE_DEFER;
        }
        else if (param->type->kind == AST_array_k &&
                 !(BE_PI_Flags(param) & BE_ARRAY_HEADER) &&
                 BE_any_pointers(BE_Array_Info(param)->flat_elt))
        {
            /*
             * Arrays: duplicate, mark the copy BE_REF_PASS, and set the array
             * elements of the copy to the yanked elements of the original.
             */
            referent = copy_parameter(param, param->name, param->type);
            referent->be_info.param = new_param_info();
            *(referent->be_info.param) = *(param->be_info.param);
            BE_Calls_After(param) = NULL;
            BE_Calls_Before(referent) = NULL;
            BE_PI_Flags(referent) |= BE_REF_PASS;

            /*
             * Create a new array info because the flat_elt fields will
             * differ between the original and its duplicate
             */
            BE_Array_Info(referent) =
                (BE_array_i_t *)BE_ctx_malloc(sizeof(BE_array_i_t));
            *(BE_Array_Info(referent)) = *(BE_Array_Info(param));
	            BE_Array_Info(referent)->flat_elt =
                BE_yank_pointers(BE_Array_Info(param)->flat_elt);

            BE_Array_Info(referent)->original = param;
            Append(yanked, referent);

            /*
             * If this was an array of [ref] pointers, eliminate it from
             * the list by pointing the array header parameter (prev) at
             * the next parameter in the list
             */
            if (BE_Is_Arr_of_Refs(param))
            {
                /*
                 *  In DCE V1.0 the following code was unconditional, and
                 *  arrays of [ref] pointers never had a representation on the
                 *  wire.  For contained arrays of [ref] pointers, however, the
                 *  correct NDR is to ship holes for the pointers.  If
                 *  BE_bug_array_no_ref_hole is specified, the user has
                 *  us to recreate that incorrect NDR.  Otherwise, suppress
                 *  the hole only for those arrays that are not contained within
                 *  other types (not a structure field).
                 */
                if (BE_bug_array_no_ref_hole ||
                    !(BE_PI_Flags(param) & BE_FIELD))
                {
                    debug(("Removing body of array of [ref] pointers %s\n",
                          BE_get_name(param->name)));
                    BE_Array_Info(referent)->original = prev;
                    prev->next = param->next;
                    if (first->last == param) first->last = prev;
                }
            }
        }
        else if (param->type->kind == AST_disc_union_k)
        {
            /*
             * Unions: duplicate, mark the copy BE_REF_PASS, and set each of
             * the union arms of the second to the yanked arms of the first
             */
            any_union_ptrs = false;
            for (arm = BE_Du_Info(param)->arms; arm; arm = arm->next)
                if (BE_any_pointers(arm->flat)) any_union_ptrs = true;

            if (any_union_ptrs)
            {
                referent = copy_parameter(param, param->name, param->type);
                referent->be_info.param = new_param_info();
                *(referent->be_info.param) = *(param->be_info.param);
                BE_Calls_After(param) = NULL;
                BE_Calls_Before(referent) = NULL;
                BE_PI_Flags(referent) |= BE_REF_PASS;

                BE_Du_Info(referent) = (BE_du_i_t *)BE_ctx_malloc(sizeof(BE_du_i_t));
                *(BE_Du_Info(referent)) = *(BE_Du_Info(param));
                BE_Du_Info(referent)->arms = NULL;

                for (arm = BE_Du_Info(param)->arms; arm; arm = arm->next)
                {
                    new_arm = (BE_arm_t *)BE_ctx_malloc(sizeof(BE_arm_t));
                    new_arm->labels = arm->labels;
                    new_arm->referred_to_by = arm;
                    new_arm->next = NULL;
                    new_arm->last = new_arm;
                    new_arm->flat = BE_yank_pointers(arm->flat);
                    Append(BE_Du_Info(referent)->arms, new_arm);
                }
                Append(yanked, referent);
            }
        }
        prev = param;
    }
    return yanked;
}

/*
 * create_open_record_header
 *
 * Returns a magic open record header parameter for prepending onto
 * flattened conformant records
 */
static AST_parameter_n_t *create_open_record_header
#ifdef PROTO
(
    AST_parameter_n_t *param,
    NAMETABLE_id_t    name,
    AST_parameter_n_t *open_array
)
#else
(param, name, open_array)
    AST_parameter_n_t *param;
    NAMETABLE_id_t    name;
    AST_parameter_n_t *open_array;
#endif
{
    AST_parameter_n_t standin, *header;

    standin.be_info.param = new_param_info();
    standin.flags = param->flags | open_array->flags & AST_SMALL;

    /*
     * The name of the header parameter is never used
     */
    header = copy_parameter(&standin, NAMETABLE_add_id("ORHEADER"),
                            BE_ulong_int_p);
    BE_Orecord_Info(header) =
            (BE_orecord_i_t *)BE_ctx_malloc(sizeof(BE_orecord_i_t));
    BE_Orecord_Info(header)->alloc_var = NAMETABLE_NIL_ID;
    BE_Orecord_Info(header)->alloc_type = NULL;
    BE_Orecord_Info(header)->alloc_typep = NULL;
    BE_Open_Array(header) = open_array;

    BE_PI_Flags(header) = BE_OPEN_RECORD;

    /*
     *  If the parameter was a conformant net type for xmit/repas, set that on
     *  the header so that we know that we have to do allocation when
     *  unmarshalling on the client side.
     */
    if (BE_PI_Flags(param) & BE_XMITCFMT)
        BE_PI_Flags(header) |= BE_XMITCFMT;
    return header;
}

/*
 * flatten_structure
 *
 * Flattens structures by returning a list of parameter-ized copies
 * of their fields with appropriately cons'ed names.  Allocates
 * BE_field_i_t entries for the fields.  Allocates BE_param_i_t
 * entries for the new stand-in parameters.
 * Pointer fields are yanked and appended to the end of structures.
 * Magic open record header parameters are prepended to conformant records.
 */
static AST_parameter_n_t *flatten_structure
#ifdef PROTO
(
    AST_parameter_n_t *param,
    NAMETABLE_id_t name,
    AST_type_n_t *type,
    BE_side_t side,
    boolean top_level,  /* non-zero only if the routine is being invoked for
                         * a top-level parameter
                         */
    AST_parameter_n_t **open_array,
    BE_fflags_t flags
)
#else
(param, name, type, side, top_level, open_array, flags)
    AST_parameter_n_t *param;
    NAMETABLE_id_t name;
    AST_type_n_t *type;
    BE_side_t side;
    boolean top_level;
    AST_parameter_n_t **open_array;
    BE_fflags_t flags;
#endif
{
    AST_parameter_n_t *flat = NULL, standin, *or_header;
    AST_parameter_n_t *yanked,*pp;
    AST_parameter_n_t *location_for_calls_before,*location_for_calls_after;
    AST_type_n_t *null_type_node_p;
    AST_field_n_t *field, *first_field;
    NAMETABLE_id_t or_name = name;
    AST_type_n_t *or_type = type;
    BE_call_rec_t *calls;


    if (top_level && !(flags & BE_F_PA_RTN) && BE_Is_Open_Record(type) &&
        ((side == BE_server_side) || (BE_PI_Flags(param) & BE_XMITCFMT)))
    {
        /*
         * Stash away an allocation name and type for conformant record
         * parameters, and replace the '*' deliberately omitted by
         * flatten_pointer.  Self-pointing open records are handled
         * differently.
         */
        name = NAMETABLE_add_derived_name(name, "(*%s)");
    }

    /*
     * if the alignment of the structure as a whole exceeds that of the
     * first field insert a null field to express the greater alignment
     * required
     */
    if (type->alignment_size >
        (first_field = type->type_structure.structure->fields)->
        type->alignment_size)
    {
        standin.be_info.param = new_param_info();
        standin.be_info.param->flags = BE_FIELD;
        standin.field_attrs = first_field->field_attrs;
        standin.uplink = param->uplink;
        standin.flags = first_field->flags;
        standin.flags |= param->flags & (AST_IN | AST_OUT);

        switch (type->alignment_size) {
            case 2:
                null_type_node_p = BE_short_null_p;
                break;
            case 4:
                null_type_node_p = BE_long_null_p;
                break;
            case 8:
                null_type_node_p = BE_hyper_null_p;
        }

        flat =
            BE_flatten_parameter(&standin,
                NAMETABLE_NIL_ID,
                null_type_node_p, side, false, open_array, flags);
    }

    for (field = type->type_structure.structure->fields; field;
         field = field->next)
    {
        field->be_info.field =
            (BE_field_i_t *)BE_ctx_malloc(sizeof(BE_field_i_t));

        standin.be_info.param = new_param_info();
        standin.be_info.param->flags = BE_FIELD;
        standin.field_attrs = field->field_attrs;
        standin.uplink = param->uplink;
        standin.flags = field->flags;
        standin.flags |= param->flags & (AST_IN | AST_OUT);

        Append(flat,
               field->be_info.field->flat =
                   BE_flatten_parameter(&standin,
                       NAMETABLE_add_derived_name2
                           (name, field->name, "%s.%s"),
                       field->type, side, false, open_array, flags));
    }

    /*
     * Now re-wire the field attributes of any fields in the struct
     */
    BE_flatten_field_attrs(flat);

    if (top_level)
    {
        /*
         * Strop any pointer fields onto the end of the structure
         */
        if (BE_any_pointers(flat) && !(flags & BE_F_PA_RTN))
        {
            yanked = BE_yank_pointers(flat);
            Append(flat, yanked);
        }

        /*
         * Strop an open record header parameter on the beginning of
         * conformant records
         */
        if (BE_Is_Open_Record(type))
        {
            or_header = create_open_record_header(param, name, *open_array);

            if (top_level && !(flags & BE_F_PA_RTN) &&
                ((side == BE_server_side) || (BE_PI_Flags(param) & BE_XMITCFMT)))
            {
                BE_Orecord_Info(or_header)->alloc_var = or_name;
                BE_Orecord_Info(or_header)->alloc_type = or_type;
                BE_Orecord_Info(or_header)->alloc_typep =
                    BE_pointer_type_node(or_type);
            }
            Append(or_header, flat);
            flat = or_header;

            BE_PI_Flags(flat->last) |= BE_LAST_FIELD;
        }
    }

    /*
     * transmit_as: Propagate call-before stuff from the original type to the
     * first field of the flattened type; propagate call-after stuff to the
     * last field of the flattened type.
     */
    location_for_calls_before = flat;
    while (BE_PI_Flags(location_for_calls_before) & BE_SKIP)
    {
        location_for_calls_before = location_for_calls_before->next;
    }
    if (calls = BE_Calls_Before(param))
    {
        if (BE_Calls_Before(location_for_calls_before)) 
            Append(calls, BE_Calls_Before(location_for_calls_before));
        BE_Calls_Before(location_for_calls_before) = calls;
    }

    if (calls = BE_Calls_After(param))
    {
        for (pp = flat; pp != NULL; pp = pp->next)
        {
            if ( !(BE_PI_Flags(pp) & BE_SKIP) )
                location_for_calls_after = pp;
        }
        Append(BE_Calls_After(location_for_calls_after), calls);
    }

    return flat;
}

static AST_parameter_n_t *flatten_pointer
(
#ifdef PROTO
    AST_parameter_n_t *param,
    NAMETABLE_id_t name,
    AST_type_n_t *type,
    BE_side_t side,
    boolean top_level,
    AST_parameter_n_t **open_array,
    BE_fflags_t flags
#endif
);

/*
 * new_call_rec
 *
 * Allocates and returns a new call record entry
 */
static BE_call_rec_t *new_call_rec
#ifdef PROTO
(
    BE_call_type_t type
)
#else
(type)
    BE_call_type_t type;
#endif
{
    BE_call_rec_t *new = (BE_call_rec_t *)BE_ctx_malloc(sizeof(BE_call_rec_t));

    new->type = type;
    new->next = NULL;
    new->last = new;

    return new;
}

/*
 * flatten_array
 *
 * Converts arrays of arrays into multi-dimensional arrays of a non-array
 * element type, e.g. array[2][3] of array[4] of int ==> array[2][3][4] of int
 * [string0] array elements do not get mashed.
 * Marks [string0] arrays small and varying; marks [string] arrays varying.
 */
static AST_parameter_n_t *flatten_array
#ifdef PROTO
(
    AST_parameter_n_t *param,
    NAMETABLE_id_t name,
    AST_type_n_t *type,
    BE_side_t side,
    boolean top_level,
    AST_parameter_n_t **open_array,
    BE_fflags_t flags
)
#else
(param, name, type, side, top_level, open_array, flags)
    AST_parameter_n_t *param;
    NAMETABLE_id_t name;
    AST_type_n_t *type;
    BE_side_t side;
    boolean top_level;
    AST_parameter_n_t **open_array;
    BE_fflags_t flags;
#endif
{
    int i, j, count, index_count, loop_nest_count;
    AST_type_n_t *elt_type, *parent_type, *new_type, *str_type, *ptr_type,
                 *base_type;
    AST_array_index_n_t *indices, *str_index;
    AST_array_n_t *new_array, *str_array;
    AST_parameter_n_t *flat, *header, standin, *yanked, *ptr_param;
    boolean string0 = false, ool_element = false, array_of_string = false;
    BE_call_rec_t *before;
    NAMETABLE_id_t ptr_name;

    /*
     * If we have an array parameter with the [ptr] attribute,
     * transmogrify it to a parameter whose type is pointer to
     * array.  Note that the pointer parameter will inherit the
     * AST_VARYING flag if it is set on the parameter.
     */

/* TBS -- is the top_level test on the next line ok? */
    if (!(flags & BE_F_PA_RTN) &&
        top_level && AST_PTR_SET(param) && (param->type->kind == AST_array_k))
    {
        ptr_type = BE_pointer_type_node (type);
        ptr_param = copy_parameter(param, name, ptr_type);
        BE_PI_Flags(ptr_param) |= BE_PTR_ARRAY;
        BE_Ptr_Info(ptr_param) = (BE_ptr_i_t *)BE_ctx_malloc(sizeof(BE_ptr_i_t));

        /*
         * Set up a local var to be the pointer to the actual array
         */
        ptr_name = BE_new_local_var_name("ptr2a");
        BE_declare_local_var(ptr_name, ptr_type, param->uplink, NULL, 0);

        /*
         * Stash away a flattened parameter of the original (unmodified)
         * array type so that we can have A, B, Z, and allocation expressions
         * later on.
         */
        AST_CLR_PTR(param);
        BE_Ptr_Info(ptr_param)->flat_array =
            BE_flatten_parameter(param,
                NAMETABLE_add_derived_name(ptr_name, "(*%s)"), type, side,
                top_level, open_array, flags);
        AST_SET_PTR(param);

        /*
         * Arrange for the initialization of the local to the address of
         * the array before marshalling
         */
        before = new_call_rec(BE_init_ptr_to_array);
        before->call.ptr_to_array.param_name = name;
        before->call.ptr_to_array.ptr_type = ptr_type;
        before->call.ptr_to_array.ptr_name = ptr_name;
        Append(BE_Calls_Before(param), before);

        return BE_flatten_parameter(ptr_param, ptr_name, ptr_type, side,
                                    top_level, open_array, flags);
    }

    /*
     * Determine whether we are dealing with a [v1_string] here
     */
    if (AST_STRING0_SET(param)) string0 = true;
    else
    {
        for (elt_type = type;
                elt_type->kind == AST_array_k;
                    elt_type = elt_type->type_structure.array->element_type)
            if (AST_STRING0_SET(elt_type)) string0 = true;
    }

    /*
     * Determine whether we are dealing with an arrays of [string]s
     */
    for (elt_type = type->type_structure.array->element_type;
            elt_type->kind == AST_array_k;
                elt_type = elt_type->type_structure.array->element_type)
        if (AST_STRING_SET(elt_type)) array_of_string = true;

    /*
     * Count the total number of dimensions in the array.
     */
    index_count = 0;
    for (elt_type = type;
            (elt_type->kind == AST_array_k);
                elt_type = elt_type->type_structure.array->element_type)
    {
        index_count += elt_type->type_structure.array->index_count;
    }
    base_type = elt_type;

    debug(("parameter %s ultimately has %d dimensions\n",
                        BE_get_name(name), index_count));

    /*
     * Setup a new index node vector
     */
    indices = (AST_array_index_n_t *)
                    BE_ctx_malloc(index_count*sizeof(AST_array_index_n_t));

    /*
     * cons up an single index vector for the total number of array
     * dimensions
     */
    for (elt_type = type, i = 0;
            elt_type->kind == AST_array_k;
                elt_type = elt_type->type_structure.array->element_type)
    {
        count = elt_type->type_structure.array->index_count;
        for (j=0; j < count; j++)
            indices[i++] = elt_type->type_structure.array->index_vec[j];
    }

    /*
     * Chase down to the ultimate element type of the array, i.e. the
     * type which the marshall loop needs to handle.  This may be an
     * OOL array type.
     */
    loop_nest_count = 0;
    for (elt_type = type;
            elt_type->kind == AST_array_k;
                elt_type = elt_type->type_structure.array->element_type)
    {
        parent_type = elt_type;
        if (AST_OUT_OF_LINE_SET(elt_type))
        {
            ool_element = true;
            break;
        }
        loop_nest_count += elt_type->type_structure.array->index_count;
    }

    /*
     * If we are dealing with [string0] on a multi-dimensional array,
     * rip out the string dimension and make it into a new one-dimensional
     * string element type
     * That is,
     *
     * [string0] (element type is char) p[30][20][10] =>
     *                     (element type is [string0] char[10]) p[30][20]
     */
    if (string0 && (index_count > 1))
    {
        debug(("Decomposing %d-d [v1_string]\n", index_count));

        str_type = (AST_type_n_t *)BE_ctx_malloc(sizeof(AST_type_n_t));
        str_array = (AST_array_n_t *)BE_ctx_malloc(sizeof(AST_array_n_t));
        str_index = (AST_array_index_n_t *) BE_ctx_malloc(sizeof(AST_array_index_n_t));

        *str_index = indices[index_count - 1];

        str_array->be_info.other = NULL;
        str_array->element_type = base_type;
        str_array->index_count = 1;
        str_array->index_vec = str_index;

        *str_type = *parent_type;
        str_type->type_structure.array = str_array;
        AST_SET_STRING0(str_type);

        /*
         * Note that later on, when conformance in multiple dimensions is
         * allowed, something will have to be done here about propagating
         * field attributes to this constructed type
         */
        if ( base_type == elt_type ) elt_type = str_type;
        base_type = str_type;
        index_count--;
        if ( !ool_element )
            loop_nest_count --;
    }

    /*
     * If we are dealing with an N-d array whose minor dim is a
     * [string] array, convert to a N-d array of 1-d [string] arrays.
     * This allows us to marshall N Z values, N-1 (A,B) pairs and
     * leverage the existing mechanism for handling 1-d [string]
     * to get the per/string (A,B) pair.
     */
    if (array_of_string)
    {
        debug(("Recomposing %d-d array with [string] minor dim\n", index_count));

        str_type = (AST_type_n_t *)BE_ctx_malloc(sizeof(AST_type_n_t));
        str_array = (AST_array_n_t *)BE_ctx_malloc(sizeof(AST_array_n_t));
        str_index = (AST_array_index_n_t *) BE_ctx_malloc(sizeof(AST_array_index_n_t));

        *str_index = indices[index_count - 1];

        str_array->be_info.other = NULL;
        str_array->element_type = base_type;
        str_array->index_count = 1;
        str_array->index_vec = str_index;

        *str_type = *parent_type;
        str_type->type_structure.array = str_array;
        AST_SET_STRING(str_type);

        /*
         * Note that later on, when conformance in multiple dimensions is
         * allowed, something will have to be done here about propagating
         * field attributes to this constructed type
         */

        if ( base_type == elt_type ) elt_type = str_type;
        base_type = str_type;
    }

    /*
     * Set up flattened parameter to return
     */
    new_array = (AST_array_n_t *)BE_ctx_malloc(sizeof(AST_array_n_t));
    new_type = (AST_type_n_t *)BE_ctx_malloc(sizeof(AST_type_n_t));

    new_array->be_info = type->type_structure.array->be_info;
    new_array->element_type = base_type;
    new_array->index_count = index_count;
    new_array->index_vec = indices;

    *new_type = *type;
    new_type->type_structure.array = new_array;

    flat = copy_parameter(param, name, new_type);

    /*
     * Mark one-dimensional arrays of char BE_STRING0 where appropriate
     */
    if (string0 && type->type_structure.array->index_count == 1 &&
        type->type_structure.array->element_type->kind == AST_character_k)
    {
        AST_SET_VARYING(flat);
        AST_SET_SMALL(flat);
        BE_PI_Flags(flat) |= BE_STRING0;
        debug(("Parameter %s is a string\n", BE_get_name(name)));
    }

    /*
     * Mark [string]s varying
     */
    if (AST_STRING_SET(param)) AST_SET_VARYING(flat);

    /*
     * Mark arrays of [string]s as such
     */
    if (array_of_string && !ool_element) BE_PI_Flags(flat) |= BE_ARR_OF_STR;

    if (AST_CONFORMANT_SET(type) && !top_level)
    {
        *open_array = flat;
        BE_PI_Flags(flat) |= BE_IN_ORECORD;
    }

    /*
     * Now setup and flatten the array element.  Note that the element_ptr_var,
     * which should more properly be created in the decoration phase, must be
     * setup here for successful flattening.
     */
    BE_Array_Info(param) = (BE_array_i_t *)BE_ctx_malloc(sizeof(BE_array_i_t));
    BE_Array_Info(param)->flags = 0;
    BE_Array_Info(param)->decorated = false;
    BE_Array_Info(param)->element_ptr_var_declared = false;
    BE_Array_Info(param)->element_ptr_var = BE_new_local_var_name("element");
    BE_Array_Info(param)->element_ptr_type = BE_pointer_type_node(elt_type);
    BE_Array_Info(param)->loop_nest_count = loop_nest_count;

    /*
     * Flatten the array element.
     */
    standin.be_info.param = NULL;
    standin.next = standin.last = NULL;
    standin.flags = param->flags & (AST_IN | AST_OUT | AST_STRING0);
    /* Move the pointer attributes to the new flat parameter */
    standin.flags |= elt_type->flags & (AST_REF | AST_UNIQUE | AST_PTR | AST_STRING | AST_VARYING);
    standin.field_attrs = NULL;
    standin.uplink = param->uplink;

    BE_Array_Info(param)->flat_elt =
        BE_flatten_parameter(&standin,
            NAMETABLE_add_derived_name(BE_Array_Info(param)->element_ptr_var,
                                       "(*%s)"),
            elt_type, side, false, open_array, 
            (flags & ~BE_F_OOL_RTN)|
            ((BE_PI_Flags(param) & BE_ARRAYIFIED) ? BE_F_ARRAYIFIED_ELT : 0));

    /*
     * propagate FIELDness to string elements so that NIDL_varying won't
     * be inappropriately queried in the marshalling code emitted into
     * "pointed at" type marshalling routines.
     */
    if (array_of_string && (BE_PI_Flags(flat) & BE_FIELD))
        BE_PI_Flags(BE_Array_Info(param)->flat_elt) |= BE_FIELD;

    if ( ((!string0 || !ool_element) && (loop_nest_count == index_count))
           || (string0 && ool_element && (loop_nest_count == index_count + 1)) )
        BE_Array_Info(param)->flat_base_elt = BE_Array_Info(param)->flat_elt;
    else
        BE_Array_Info(param)->flat_base_elt =
            BE_flatten_parameter(&standin,
            NAMETABLE_add_derived_name(BE_Array_Info(param)->element_ptr_var,
                                       "(*%s)"),
                base_type, side, false, open_array, (flags & ~BE_F_OOL_RTN));

    /*
     *  If called from a pointed-at marshalling routine, mark the array
     *  BE_PARRAY, unless this is the array for a pipe push/pull routine.
     */
    if (top_level && (flags & BE_F_PA_RTN) && !(flags & BE_F_PIPE_ROUTINE))
        BE_PI_Flags(flat) |= BE_PARRAY;

    /*
     * Split the array into a header and body part.  The header will be
     * a place holder in the iovector for alignment, conformance and
     * variance information, and synchronization.
     * The body will be in a separate iovector slot and will contain only
     * the array elements.  The header will be marked BE_ARRAY_HEADER.
     */
    header = copy_parameter(flat, flat->name, flat->type);

    header->be_info.param = new_param_info();
    *(header->be_info.param) = *(flat->be_info.param);

    BE_PI_Flags(header) |= BE_ARRAY_HEADER;
    if (AST_SMALL_SET(type))
    {
        /* [out_of_line] needs this property on the header and the array */
        AST_SET_SMALL(header);
        AST_SET_SMALL(flat);
    }

    Append(header, flat);

    /*
     * Propagate call-before stuff to the header and call-after to the body
     */
    BE_Calls_Before(header) = BE_Calls_Before(param);
    BE_Calls_After(header) = NULL;

    BE_Calls_Before(flat) = NULL;
    BE_Calls_After(flat) = BE_Calls_After(param);

    flat = header;

    if (top_level && BE_any_pointers(flat) && !(flags & BE_F_PA_RTN))
    {
        yanked = BE_yank_pointers(flat);
        Append(flat, yanked);
    }

    return flat;
}

/*
 * flatten_pointer
 *
 * Ref pointer machinations: On the client side, flatten them out while cons'ing
 * asterisks onto their names; on the server side allocate and declare
 * surrogates and flatten the ultimate resolvent of the pointer.
 */
static AST_parameter_n_t *flatten_pointer
#ifdef PROTO
(
    AST_parameter_n_t *param,
    NAMETABLE_id_t name,
    AST_type_n_t *type,
    BE_side_t side,
    boolean top_level,
    AST_parameter_n_t **open_array,
    BE_fflags_t flags
)
#else
(param, name, type, side, top_level, open_array, flags)
    AST_parameter_n_t *param;
    NAMETABLE_id_t name;
    AST_type_n_t *type;
    BE_side_t side;
    boolean top_level;
    AST_parameter_n_t **open_array;
    BE_fflags_t flags;
#endif
{
    AST_operation_n_t *oper = param->uplink;
    BE_ptr_init_t *ptr_init;
    AST_type_n_t *pointee_type, *array_rep_type;
    NAMETABLE_id_t pointee_name;
    AST_parameter_n_t *flat, *yanked;
    AST_type_n_t *new_ptr_type;
    BE_call_rec_t *after;
    AST_parameter_n_t pointee_standin;  /* copy of parameter with correct */
                                        /* pointer attributes for pointee */

    /*
     * Pointers that have been "arrayified".  The frontend defines the
     * array_rep_type field of AST_pointer_n_t nodes that denote pointer
     * type instances that have been arrayified by the application of
     * certain array attributes.  Here there are two cases: top level
     * arrayified ref pointers which can be coerced into arrays and all
     * others which can be coerced into pointers to arrays.  Note that
     * if we hit another arrayified pointer while processing the element
     * type of an arrayified pointer, we want to treat it simply as a pointer
     * and not as an array.  Because it really behaves like a pointer it is
     * un/marshalled by a pa routine which will flatten the pointee when 
     * needed.  Without this s self-pointing arrayified pointer will cause
     * an infinite recursion during flattening.
     */
    array_rep_type = type->type_structure.pointer->pointee_type->array_rep_type;
    if (BE_Is_Arrayified(param,type) && !(flags & BE_F_ARRAYIFIED_ELT))
    {
        if (top_level && AST_REF_SET(param))
        {
            return BE_flatten_parameter(param, name, array_rep_type,
                                        side, top_level, open_array, flags);
        }
        else
        {
            BE_PI_Flags(param) |= BE_ARRAYIFIED;
            BE_Ptr_Info(param) = (BE_ptr_i_t *)BE_ctx_malloc(sizeof(BE_ptr_i_t));
            BE_Ptr_Info(param)->flat_array =
                BE_flatten_parameter(param, name, array_rep_type,
                                     side, top_level, open_array, flags);
            new_ptr_type = BE_pointer_type_node(array_rep_type);
            new_ptr_type->flags = type->flags & (AST_REF | AST_UNIQUE);

            return BE_flatten_parameter(param, name, new_ptr_type,
                                        side, top_level, open_array, flags);
        }
    }

    /*
     * Set up variables to process pointee.  Because all routines look at
     * the param now for pointer attributes, propagate the attributes
     * on the type up to the param.  AST_ADD_COMM_STATUS is noticed to
     * suppress marshalling of ACF parameters via BE_SKIP mechanism.
     */
    pointee_type = type->type_structure.pointer->pointee_type;
    pointee_standin = *param;
    pointee_standin.flags = param->flags
        & (AST_IN | AST_OUT | AST_CONTEXT | AST_ADD_COMM_STATUS |
           AST_ADD_FAULT_STATUS);
    if (AST_ADD_COMM_STATUS_SET(param) || AST_ADD_FAULT_STATUS_SET(param))
        BE_PI_Flags(&pointee_standin) |= BE_SKIP;
    pointee_standin.flags |= pointee_type->flags & (AST_REF | AST_UNIQUE |
          AST_PTR | AST_STRING | AST_VARYING);

    /*
     * [ref] Pointers
     */
    if (top_level && !(flags & BE_F_PA_RTN) && AST_REF_SET(param))
    {
        /*
         * Client side: just strop a '*' onto the name and flatten the pointee
         * except in the case of an xmitas open record in which the * is added
         * in flatten_xmit_as/flatten_rep_as.
         */
        if (side == BE_client_side)
        {
            if ((BE_Is_Open_Record(pointee_type) && pointee_type->rep_as_type) ||
                (pointee_type->xmit_as_type &&
                 BE_Is_Open_Record(pointee_type->xmit_as_type)))
            {
                /* [ref] pointer to conformant net type for xmit */
                return BE_flatten_parameter(&pointee_standin,
                    name, pointee_type, side, top_level, open_array, flags);
            }
            else
            {
                /* Normal case */
                return BE_flatten_parameter(&pointee_standin,
                    NAMETABLE_add_derived_name(name, "(*%s)"), pointee_type,
                    side, top_level, open_array, flags);
            }
        }

        /*
         * Server side:
         *
         * [in, ref] type_t *param;
         *
         * If param is not [heap] declare a surrogate NIDL_ptr_n of type
         * type_t on the stack and marshall into and out of this surrogate.
         * Set up an initialization record to emit "param = &NIDL_ptr_n;"
         * at code generation time.
         *
         * If param is [heap] strop a '*' onto the name as on the client side,
         * above.  Also set up an initialization record to emit
         * "param = (type_t *)BE_ctx_malloc(sizeof(type_t));" at code generation time.
         *
         * Pass open records through with names unscathed; the '*'s for these
         * parameters will be stropped on later in flatten_structure
         */
        if (BE_Is_Open_Record(pointee_type) || (pointee_type->xmit_as_type &&
             AST_CONFORMANT_SET(pointee_type->xmit_as_type)))
        {
            /*
             *  If the pointee is a rep_as or xmit_as type, then we before we
             *  call the application translation routine, we must declare a
             *  surrogate for the local rep and initialize the local rep
             *  pointer to the surrogate, because the to_local routine does not
             *  do allocation.
             */
            if (pointee_type->rep_as_type || pointee_type->xmit_as_type)
            {
                /*
                 *  Declare a surrogate for the local represenation and assign
                 *  it's address to the parameter's server surrogate
                 */
                BE_declare_local_var(
                    pointee_name = BE_new_local_var_name("ptr"),
                    pointee_type,
                    oper,
                    NULL,
                    0);
                ptr_init = BE_new_ptr_init(name, NULL,
                          pointee_name, NULL, false);
                Append(BE_Pointers(oper), ptr_init);
            }
            return BE_flatten_parameter(&pointee_standin, name, pointee_type,
                                        side, top_level, open_array, flags);
        }
        else
        {
            if (AST_HEAP_SET(param))
            {
                debug(("Allocating %s on the heap\n", BE_get_name(name)));
                ptr_init = BE_new_ptr_init(name, type, NAMETABLE_NIL_ID,
                                           pointee_type, true);
                Append(BE_Pointers(oper), ptr_init);

                return BE_flatten_parameter(&pointee_standin,
                            NAMETABLE_add_derived_name(name, "(*%s)"),
                            pointee_type, side, top_level, open_array, flags);
            }
            else
            if ((pointee_type->kind == AST_array_k &&
                  AST_CONFORMANT_SET(pointee_type)) ||
                (pointee_type->xmit_as_type &&
                 pointee_type->xmit_as_type->kind == AST_array_k &&
                  AST_CONFORMANT_SET(pointee_type->xmit_as_type)))
            {
                /*
                 * If a top level param is a pointer to a conformant array
                 * (a string, for example) declare a helper var whose type
                 * is the slice type of the pointed at array and arrange for
                 * its value (after allocation) to be assigned to the parameter
                 */

                BE_declare_local_var(
                    pointee_name = BE_new_local_var_name("ptr"),
                    BE_pointer_type_node(BE_slice_type_node(pointee_type)),
                    oper,
                    NULL,
                    0);

                /*
                 * Arrange for the initialization of the server parameter
                 * surrogate to the cast value of the helper var after
                 * unmarshalling (and allocation)
                 */
                after = new_call_rec(BE_init_ptr_to_conf);
                after->call.ptr_to_conf.param_name = name;
                after->call.ptr_to_conf.param_type = type;
                after->call.ptr_to_conf.pointee_name = pointee_name;
                Append(BE_Calls_After(param), after);

                return BE_flatten_parameter(&pointee_standin, pointee_name, pointee_type,
                                            side, top_level, open_array, flags);
            }
            else
            {
                /*
                 * Declare a surrogate for the pointee type on the stack
                 */
                BE_declare_local_var(
                    pointee_name = BE_new_local_var_name("ptr"),
                    pointee_type,
                    oper,
                    NULL,
                    0);

                /*
                 * Assign its address to the parameter's server surrogate
                 */
                ptr_init = BE_new_ptr_init(name, NULL, pointee_name, NULL,
                                           false);
                Append(BE_Pointers(oper), ptr_init);

                return BE_flatten_parameter(&pointee_standin, pointee_name, pointee_type,
                                            side, top_level, open_array, flags);
            }
        }
    }
    else
    {
        /*
         *  If the pointee type has [string] and the pointee is not a pointer
         *  then this is an array of string.  If the pointee is a pointer with
         *  [string] it means we have a pointer to a pointer to a string.
         */
        if ((AST_STRING_SET(pointee_type) &&
             (pointee_type->kind != AST_pointer_k)) ||
            (pointee_type->xmit_as_type &&
             (pointee_type->xmit_as_type->kind != AST_pointer_k) &&
             AST_STRING_SET(pointee_type->xmit_as_type)))
        {
            BE_PI_Flags(param) |= BE_PTR2STR;
            BE_Ptr_Info(param) = (BE_ptr_i_t *)BE_ctx_malloc(sizeof(BE_ptr_i_t));
            BE_Ptr_Info(param)->flat_array = BE_flatten_parameter(param,
                NAMETABLE_add_derived_name(name, "(*%s)"), pointee_type, side,
                top_level, open_array, flags | BE_F_POINTEE);
            AST_SET_STRING(BE_Ptr_Info(param)->flat_array);
            AST_SET_VARYING(BE_Ptr_Info(param)->flat_array);
        }

        /*
         * Non-[ref] Pointers: Duplicate the parameter, marking the first
         * BE_DEFER
         */
        flat = copy_parameter(param, name, type);

        if ( AST_IGNORE_SET(param) )
        {
            BE_PI_Flags(flat) |= BE_DEFER;
        }
        else if (top_level && !(flags & BE_F_PA_RTN))
        {
            yanked = BE_yank_pointers(flat);
            Append(flat, yanked);
        }
        return flat;
    }
}

/*
 * flatten_union
 *
 * Flatten the discriminator and arms of a discriminated union.
 */
static AST_parameter_n_t *flatten_union
#ifdef PROTO
(
    AST_parameter_n_t *param,
    NAMETABLE_id_t name,
    AST_type_n_t *type,
    BE_side_t side,
    boolean top_level,
    AST_parameter_n_t **open_array,
    BE_fflags_t flags
)
#else
(param, name, type, side, top_level, open_array, flags)
    AST_parameter_n_t *param;
    NAMETABLE_id_t name;
    AST_type_n_t *type;
    BE_side_t side;
    boolean top_level;
    AST_parameter_n_t **open_array;
    BE_fflags_t flags;
#endif
{
    AST_parameter_n_t *discrim, *uswitch, standin, *yanked;
    BE_arm_t *flat_arm;
    AST_arm_n_t *arm;
    NAMETABLE_id_t union_name;

    /* Create a flattened parameter for the discriminator */
    standin.be_info.param = new_param_info();
    standin.uplink = param->uplink;
    standin.flags = param->flags & (AST_IN | AST_OUT);
    standin.flags |= type->type_structure.disc_union->discrim_type->flags & (AST_REF | AST_UNIQUE | AST_PTR);

    discrim = copy_parameter(&standin, NAMETABLE_add_derived_name2(name,
                  type->type_structure.disc_union->discrim_name, "%s.%s"),
                  type->type_structure.disc_union->discrim_type);

    /*
     * transmit_as: Attach call-before information to the discriminant
     */
    BE_Calls_Before(discrim) = BE_Calls_Before(param);

    /*
     * Create a flattened parameter for the union part whose type kind is
     * AST_disc_union_k
     */
    uswitch = copy_parameter(param, name, type);
    uswitch->be_info.param = new_param_info();
    BE_Du_Info(uswitch) = (BE_du_i_t *)BE_ctx_malloc(sizeof(BE_du_i_t));
    BE_Du_Info(uswitch)->discrim = discrim;

    /*
     * transmit_as: Attach call-after information to the union part
     */
    BE_Calls_After(uswitch) = BE_Calls_After(param);

    BE_Du_Info(uswitch)->arms = NULL;

    /* Flatten each of the arms of the union */
    union_name = NAMETABLE_add_derived_name2(name,
                     type->type_structure.disc_union->union_name, "%s.%s");

    for (arm = uswitch->type->type_structure.disc_union->arms; arm;
         arm = arm->next)
    {
        flat_arm = (BE_arm_t *)BE_ctx_malloc(sizeof(BE_arm_t));
        flat_arm->labels = arm->labels;
        flat_arm->next = NULL;
        flat_arm->last = flat_arm;

        if (arm->type)
        {
            standin.be_info.param = new_param_info();
            standin.uplink = param->uplink;
            standin.flags = (param->flags & (AST_IN | AST_OUT)) | arm->flags;
            standin.field_attrs = NULL;

            flat_arm->flat =
                BE_flatten_parameter(&standin,
                    NAMETABLE_add_derived_name2(union_name, arm->name, "%s.%s"),
                    arm->type, side, false, open_array, flags);
        }
        else flat_arm->flat = NULL;

        Append(BE_Du_Info(uswitch)->arms, flat_arm);
    }

    if (top_level && BE_any_pointers(uswitch) && !(flags & BE_F_PA_RTN))
    {
        yanked = BE_yank_pointers(uswitch);
        Append(uswitch, yanked);
    }

    /*
     * Return a list consisting of the discriminated part followed by
     * the union part
     */
    Append(discrim, uswitch);

    return discrim;
}

/*
 * flatten_pipe
 *
 * Strap the conformant array incarnation of a pipe into its be_info structure
 */
static AST_parameter_n_t *flatten_pipe
#ifdef PROTO
(
    AST_parameter_n_t *param,
    NAMETABLE_id_t name,
    AST_type_n_t *type,
    BE_side_t side,
    boolean top_level,
    AST_parameter_n_t **open_array,
    BE_fflags_t flags
)
#else
(param, name, type, side, top_level, open_array, flags)
    AST_parameter_n_t *param;
    NAMETABLE_id_t name;
    AST_type_n_t *type;
    BE_side_t side;
    boolean top_level;
    AST_parameter_n_t **open_array;
    BE_fflags_t flags;
#endif
{
    AST_parameter_n_t standin, *flat, *size;
    AST_type_n_t array_type;
    AST_array_n_t array;
    AST_array_index_n_t index;
    AST_constant_n_t *lower_bound;
    AST_field_attr_n_t *attrs;
    AST_field_ref_n_t *ref;
    BE_param_i_t *saved_info;
    unsigned short index_count, i;


    /*
     * NOTE: This is one of the few cases where context is placed in the
     *   AST with scope beyond a single operation.  The
     */
    if (type->type_structure.pipe->be_info.pipe == NULL)
    {
        BE_push_perm_malloc_ctx();
        NAMETABLE_set_perm_name_mode();

        /*
         *  Set up a the pipe info.  It is an array of fixed lower bound of
         *  zero, this hangs as state in the AST, so use BE_ctx_malloc with the
         *  current perm context.
         */
        type->type_structure.pipe->be_info.pipe =
            (BE_pipe_i_t *)BE_ctx_malloc(sizeof(BE_pipe_i_t));
        type->type_structure.pipe->be_info.pipe->decorated = false;
        lower_bound = (AST_constant_n_t *)BE_ctx_malloc(sizeof(AST_constant_n_t));
        lower_bound->be_info.other = NULL;
        lower_bound->name = NAMETABLE_NIL_ID;
        lower_bound->defined_as = NULL;
        lower_bound->kind = AST_int_const_k;
        lower_bound->value.int_val = 0;

        /*
         * Set up the array index node.  Do not BE_ctx_malloc(), as it will be
         * copied during flattening.
         */
        index.be_info.other = NULL;
        index.lower_bound = lower_bound;
        index.upper_bound = NULL;
        index.flags = AST_FIXED_LOWER;

        /*
         * Set up an array node.  Do not BE_ctx_malloc(), as it will be
         * copied during flattening.
         */
        array.be_info.other = NULL;
        array.index_count = 1;
        array.index_vec = &index;
        array.element_type = type->type_structure.pipe->base_type;

        /*
         * Set up a type node.  Do not BE_ctx_malloc(), as it will be
         * copied during flattening.
         */
        array_type.be_info.type = NULL;
        array_type.name = NAMETABLE_NIL_ID;
        array_type.defined_as = NULL;
        array_type.kind = AST_array_k;
        array_type.type_structure.array = &array;
        array_type.xmit_as_type = NULL;
        array_type.rep_as_type = NULL;
        array_type.flags = AST_CONFORMANT;
        array_type.array_rep_type = NULL;
        array_type.alignment_size = type->type_structure.pipe->base_type->alignment_size;
        array_type.ndr_size = type->type_structure.pipe->base_type->ndr_size;


        standin.be_info.param = new_param_info();
        standin.uplink = param->uplink;
        standin.flags = 0;

        flat = type->type_structure.pipe->be_info.pipe->flat =
            BE_flatten_parameter(&standin, NAMETABLE_add_id("Draz"),
                                 &array_type, side, top_level, open_array,
                                 flags);
        /*
         * Create a size_is pseudo-parameter
         */
        size = copy_parameter(&standin, NAMETABLE_add_id("pipe_chunk_size"),
                              BE_ulong_int_p);

        /*
         * Set up a field reference node to point at size
         */
        index_count = flat->type->type_structure.array->index_count;
        ref = (AST_field_ref_n_t *)
                    BE_ctx_malloc(index_count * sizeof(AST_field_ref_n_t));
        ref[0].be_info.other = NULL;
        ref[0].valid = true;
        ref[0].ref.p_ref = size;
        for( i=1; i<index_count; i++) ref[i].valid = false;


        /*
         * Set up a field attributes node to point at ref
         */
        attrs = (AST_field_attr_n_t *)BE_ctx_malloc(sizeof(AST_field_attr_n_t));
        attrs->be_info.other = NULL;
        attrs->first_is_vec = attrs->last_is_vec = attrs->length_is_vec =
        attrs->min_is_vec = attrs->max_is_vec = NULL;
        attrs->size_is_vec = ref;

        type->type_structure.pipe->be_info.pipe->flat->field_attrs = attrs;
        NAMETABLE_set_temp_name_mode();
        BE_pop_malloc_ctx();
    }

    /*
     * If we are in the process of emitting pipe marshalling code, return
     * the conformant array parameter.
     */
    if (flags & BE_F_PA_RTN)
    {
        flat = copy_parameter(
            type->type_structure.pipe->be_info.pipe->flat, name,
            type->type_structure.pipe->be_info.pipe->flat->type);

        saved_info = flat->be_info.param;

        flat->be_info.param = new_param_info();
        *(flat->be_info.param) = *saved_info;
        BE_PI_Flags(flat) &= ~BE_OPEN_RECORD;

        flat->field_attrs =
            type->type_structure.pipe->be_info.pipe->flat->field_attrs;
    }
    /*
     * Otherwise, just return a copy of the pipe
     */
    else flat = copy_parameter(param, name, type);


    /*
     * Fix up the returned flattened parameter to have the correct uplink.
     */
    flat->uplink = param->uplink;
    return flat;
}

/*
 * flatten_xmit_as
 *
 * Cook up and declare a local variable of type xmit_type for unmarshalling,
 * and pointer-to-xmit_type for marshalling; Strop pre- and post-un/marshall-
 * ing records onto the parameter to control _to_xmit_rep invocations (before
 * marshalling), _from_xmit_rep (after marshalling), and _free_xmit_rep
 * (after unmarshalling) call emission.  All TYPE_free routines are invoked
 * at the close of the server stub.
 */
static AST_parameter_n_t *flatten_xmit_as
#ifdef PROTO
(
    AST_parameter_n_t *param,
    NAMETABLE_id_t name,
    AST_type_n_t *type,
    BE_side_t side,
    boolean top_level,
    AST_parameter_n_t **open_array,
    BE_fflags_t flags
)
#else
(param, name, type, side, top_level, open_array, flags)
    AST_parameter_n_t *param;
    NAMETABLE_id_t name;
    AST_type_n_t *type;
    BE_side_t side;
    boolean top_level;
    AST_parameter_n_t **open_array;
    BE_fflags_t flags;
#endif
{
    BE_call_rec_t *before, *after;
    NAMETABLE_id_t xmit_name, pxmit_name, star_pxmit_name, usage_name;
    AST_parameter_n_t *flat_param;
    boolean conformant_client = false;

    if ((flags & BE_F_UNDER_OOL) && (BE_PI_Flags(param) & BE_FIELD))
    {
        /* Field of entity that is being [out_of_line]d */
        /* Treat field as being of transmitted type */
        flat_param = BE_flatten_parameter(param, name, type->xmit_as_type,
                                side, top_level, open_array, flags);
        return flat_param;
    }

    /*
     * xmit_name/xmit_type are the name and type of the stack surrogate
     * for unmarshalling
     */
    xmit_name = BE_new_local_var_name("xmit");

    /*
     * pxmit_name is the name of the stack surrogate for marshalling;
     * its type is actually ptr-to-xmit_type, but it will be declared
     * via star_pxmit_name/xmit_type below
     */
    pxmit_name = BE_new_local_var_name("pxmit");

    /*
     * star_pxmit_name/xmit_type are a standin to convince the marshalling
     * mechanism to do the right thing.
     */
    star_pxmit_name = NAMETABLE_add_derived_name(pxmit_name, "(*%s)");

    debug(("Flattening xmit_as (%s --> %s, %s)\n", BE_get_name(name),
           BE_get_name(xmit_name), BE_get_name(pxmit_name)));

    before = new_call_rec(BE_call_xmit_as);
    before->call.xmit_as.native_name = name;
    before->call.xmit_as.native_type = type;
    before->call.xmit_as.xmit_name = xmit_name;
    before->call.xmit_as.pxmit_name = pxmit_name;
    before->call.xmit_as.xmit_type = type->xmit_as_type;
    Append(BE_Calls_Before(param), before);

    after = new_call_rec(BE_call_xmit_as);
    after->call.xmit_as.native_name = name;
    after->call.xmit_as.native_type = type;
    after->call.xmit_as.xmit_name = xmit_name;
    after->call.xmit_as.pxmit_name = pxmit_name;
    after->call.xmit_as.xmit_type = type->xmit_as_type;
    Prepend(BE_Calls_After(param), after);

    /*
     *  Reference the input variable with a star so that we can use
     *  structure field references (*NIDL_pxmit_0).size.  The exception
     *  is when we are doing a non-pointed-at routine for an open
     *  record.
     */
    usage_name = star_pxmit_name;


    /*
     * For conformant xmit types, don't need a stack surrogate because
     * the surrogate must be dynamically allocated.
     */
    if ( (side == BE_server_side) &&
          top_level && AST_CONFORMANT_SET(type->xmit_as_type) )
    {
        /*
         * Since there is no stack surrogate, NULL out the name so that
         * it is not initialized in MUTILS routine BE_spell_assign_xmits
         */
        before->call.xmit_as.xmit_name = NAMETABLE_NIL_ID;
        after->call.xmit_as.xmit_name = NAMETABLE_NIL_ID;

        if (!(flags & BE_F_PA_RTN))
        {
            /*
             *  Reference the surrogate using the simple name because it is
             *  directly pointed at by the pxmit_name because it is dynamically
             *  allocated.
             */
            usage_name = pxmit_name;

            /*
             * Open Records do not have the * added to them in flatten_pointer,
             * so do it now such that the to_local routine is called correctly.
             */
            name = NAMETABLE_add_derived_name(name, "(*%s)");
            before->call.xmit_as.native_name = name;
            after->call.xmit_as.native_name = name;
        }
    }
    else if ( (side == BE_client_side) &&
          top_level && AST_CONFORMANT_SET(type->xmit_as_type) )
    {
        /*
         * Since there is no stack surrogate, NULL out the name so that
         * it is not initialized in MUTILS routine BE_spell_assign_xmits
         */
        before->call.xmit_as.xmit_name = NAMETABLE_NIL_ID;
        after->call.xmit_as.xmit_name = NAMETABLE_NIL_ID;

        if (!(flags & BE_F_PA_RTN))
        {
            /*
             * The usage name should not include the * because it is added on
             * by flatten_structure.
             */
            usage_name = pxmit_name;

            /*
             * Open Records do not have the * added to them in flatten_pointer,
             * so do it now such that the to_local routine is called correctly.
             */
            if ( BE_Is_Open_Record(type->xmit_as_type) )
            {
                name = NAMETABLE_add_derived_name(name, "(*%s)");
                before->call.xmit_as.native_name = name;
                after->call.xmit_as.native_name = name;
            }
        }

        /* Cause allocation on client side of [out] xmitas type */
        conformant_client = true;
    }
    else
    {
        BE_declare_local_var(xmit_name, type->xmit_as_type, param->uplink,
                         NULL, 0);
    }

    BE_declare_local_var(star_pxmit_name, type->xmit_as_type, param->uplink,
                         NULL, 0);

    /*
     *  Set the flag that indicates that when unmarshalling on the client side,
     *  allocate the net type because the memory does not exist.  Then after
     *  unmarshalling, it is converted into the destination type.
     */
    if (conformant_client)  BE_PI_Flags(param) |= BE_XMITCFMT;
    if ( AST_STRING_SET(type->xmit_as_type) )
    {
        /* Set "string" property on constructed parameter */
        AST_SET_STRING(param);
    }
    flat_param = BE_flatten_parameter(param, usage_name, type->xmit_as_type,
                                side, top_level, open_array, flags);
    return flat_param;
}

/*
 * flatten_rep_as
 *
 * This is just like xmit_as, above.
 */
static AST_parameter_n_t *flatten_rep_as
#ifdef PROTO
(
    AST_parameter_n_t *param,
    NAMETABLE_id_t name,
    AST_type_n_t *type,
    BE_side_t side,
    boolean top_level,
    AST_parameter_n_t **open_array,
    BE_fflags_t flags
)
#else
(param, name, type, side, top_level, open_array, flags)
    AST_parameter_n_t *param;
    NAMETABLE_id_t name;
    AST_type_n_t *type;
    BE_side_t side;
    boolean top_level;
    AST_parameter_n_t **open_array;
    BE_fflags_t flags;
#endif
{
    BE_call_rec_t *before, *after;
    NAMETABLE_id_t net_name, pnet_name, star_pnet_name, usage_name;
    AST_type_n_t *net_type;
    boolean conformant_client = false;

    if ((flags & BE_F_UNDER_OOL) && (BE_PI_Flags(param) & BE_FIELD))
    {
        /* Field of entity that is being [out_of_line]d */
        /* Treat field as being of network type */
        net_type = (AST_type_n_t *)BE_ctx_malloc(sizeof(AST_type_n_t));
        *net_type = *type;
        net_type->rep_as_type = NULL;
        return BE_flatten_parameter(param, name, net_type, side,
                      top_level, open_array, flags);
    }

    /*
     * net_name/net_type are the name and type of the stack surrogate
     * for unmarshalling
     */
    net_name = BE_new_local_var_name("net");

    /*
     * pnet_name is the name of the stack surrogate for marshalling;
     * its type is actually ptr-to-net_type, but it will be declared
     * via star_pnet_name/net_type below
     */
    pnet_name = BE_new_local_var_name("pnet");

    /*
     * star_pnet_name/net_type are a standin to convince the marshalling
     * mechanism to do the right thing.
     */
    star_pnet_name = NAMETABLE_add_derived_name(pnet_name, "(*%s)");

    debug(("Flattening rep_as (%s --> %s, %s)\n", BE_get_name(name),
           BE_get_name(net_name), BE_get_name(pnet_name)));

    /*
     * Clear the net type rep_as_type field to avoid infinite recursion
     */
    net_type = (AST_type_n_t *)BE_ctx_malloc(sizeof(AST_type_n_t));
    *net_type = *type;
    net_type->rep_as_type = NULL;

    before = new_call_rec(BE_call_rep_as);
    before->call.rep_as.local_name = name;
    before->call.rep_as.net_name = net_name;
    before->call.rep_as.pnet_name = pnet_name;
    before->call.rep_as.net_type = net_type;
    Append(BE_Calls_Before(param), before);

    after = new_call_rec(BE_call_rep_as);
    after->call.rep_as.local_name = name;
    after->call.rep_as.net_name = net_name;
    after->call.rep_as.pnet_name = pnet_name;
    after->call.rep_as.net_type = net_type;
    Append(BE_Calls_After(param), after);

    /*
     *  Reference the input variable with a star so that we can use
     *  structure field references (*NIDL_pnet_0).size.  The exception
     *  is when we are doing a non-pointed-at routine for an open
     *  record.
     */
    usage_name = star_pnet_name;

    /*
     * For conformant net types, don't need a stack surrogate because
     * the surrogate must be dynamically allocated.
     */
    if ((side == BE_server_side) &&
        top_level && BE_Is_Open_Record(type))
    {
        /*
         * Since there is no stack surrogate, NULL out the name so that
         * it is not initialized in MUTILS routine BE_spell_assign_nets
         */
        before->call.rep_as.net_name = NAMETABLE_NIL_ID;
        after->call.rep_as.net_name = NAMETABLE_NIL_ID;

        if (!(flags & BE_F_PA_RTN))
        {
            /*
             *  Reference the surrogate using the simple name because it is
             *  directly pointed at by the pnet_name because it is dynamically
             *  allocated.
             */
            usage_name = pnet_name;

            /*
             * Open Records do not have the * added to them in flatten_pointer,
             * so do it now such that the to_local routine is called correctly.
             */
            name = NAMETABLE_add_derived_name(name, "(*%s)");
            before->call.rep_as.local_name = name;
            after->call.rep_as.local_name = name;
        }
    }
    else if ((side == BE_client_side) &&
        top_level && BE_Is_Open_Record(type))
    {
        /*
         * Since there is no stack surrogate, NULL out the name so that
         * it is not initialized in MUTILS routine BE_spell_assign_xmits
         */
        before->call.rep_as.net_name = NAMETABLE_NIL_ID;
        after->call.rep_as.net_name = NAMETABLE_NIL_ID;

        if (!(flags & BE_F_PA_RTN))
        {
            /*
             * The usage name should not include the * because it is added on
             * by flatten_structure.
             */
            usage_name = pnet_name;

            /*
             * Open Records do not have the * added to them in flatten_pointer,
             * so do it now such that the to_local routine is called correctly.
             */
            name = NAMETABLE_add_derived_name(name, "(*%s)");
            before->call.rep_as.local_name = name;
            after->call.rep_as.local_name = name;
        }

        /* Cause allocation on client side of [out] xmitas type */
        conformant_client = true;
    }
   else
    {
        BE_declare_local_var(net_name, net_type, param->uplink, NULL, 0);
    }


    /* Declare the pointer to the net type */
    BE_declare_local_var(star_pnet_name, net_type, param->uplink, NULL, 0);


    /*
     *  Set the flag that indicates that when unmarshalling on the client side,
     *  allocate the net type because the memory does not exist.  Then after
     *  unmarshalling, it is converted into the destination type.
     */
    if (conformant_client)  BE_PI_Flags(param) |= BE_XMITCFMT;
    return BE_flatten_parameter(param, usage_name, net_type, side,
                      top_level, open_array, flags);

}

/*
 * flatten_ctx_handle
 *
 * Replace the type of a parameter with the [context_handle] attribute
 * with that of the context handle we know the frontend parsed out of
 * nbase.idl;
 */
static AST_parameter_n_t *flatten_ctx_handle
#ifdef PROTO
(
    AST_parameter_n_t *param,
    NAMETABLE_id_t name,
    AST_type_n_t *type,
    BE_side_t side,
    boolean top_level,
    AST_parameter_n_t **open_array,
    BE_fflags_t flags
)
#else
(param, name, type, side, top_level, open_array, flags)
    AST_parameter_n_t *param;
    NAMETABLE_id_t name;
    AST_type_n_t *type;
    BE_side_t side;
    boolean top_level;
    AST_parameter_n_t **open_array;
    BE_fflags_t flags;
#endif
{
    NAMETABLE_id_t type_id, new_name;
    AST_type_n_t *new_type;
    AST_parameter_n_t *flat;
    BE_call_rec_t *before, *after;

    new_name = BE_new_local_var_name("context");
    type_id = NAMETABLE_lookup_id(CTX_HANDLE_TYPE_NAME);

    new_type = (AST_type_n_t *)NAMETABLE_lookup_binding(type_id);

    /*
     * Sanity check.  This should never happen.
     */
    if (type_id == NAMETABLE_NIL_ID)
    {
        INTERNAL_ERROR("Context handle type ndr_context_handle is not defined");
    }

    BE_declare_local_var(new_name, new_type, param->uplink, NULL, 0);

    before = new_call_rec(BE_call_ctx_handle);
    before->call.ctx_handle.native_name = name;
    before->call.ctx_handle.native_type = type;
    before->call.ctx_handle.wire_name = new_name;
    if (AST_CONTEXT_RD_SET(type)) before->call.ctx_handle.rundown = true;
    Append(BE_Calls_Before(param), before);

    after = new_call_rec(BE_call_ctx_handle);
    after->call.ctx_handle.native_name = name;
    after->call.ctx_handle.native_type = type;
    after->call.ctx_handle.wire_name = new_name;
    if (AST_CONTEXT_RD_SET(type)) after->call.ctx_handle.rundown = true;
    Append(BE_Calls_After(param), after);

    flat = copy_parameter(param, name, type);
    AST_CLR_CONTEXT(flat);
    return BE_flatten_parameter(flat, new_name, new_type, side, top_level,
                                open_array, flags);
}

/*
 * flatten_ool
 *
 */
static AST_parameter_n_t *flatten_ool
#ifdef PROTO
(
    AST_parameter_n_t *param,
    NAMETABLE_id_t name,
    AST_type_n_t *type,
    BE_side_t side,
    boolean top_level,
    AST_parameter_n_t **open_array,
    BE_fflags_t flags
)
#else
(param, name, type, side, top_level, open_array, flags)
    AST_parameter_n_t *param;
    NAMETABLE_id_t name;
    AST_type_n_t *type;
    BE_side_t side;
    boolean top_level;
    AST_parameter_n_t **open_array;
    BE_fflags_t flags;
#endif
{
    AST_parameter_n_t *flatp, *pp, *yanked, *location_for_calls_after;
    boolean has_ptrs;
    BE_call_rec_t *calls_before;
    BE_call_rec_t *calls_after; /* Procedure calls after marshalling must
                                   appear on a parameter that will be
                                   marshalled, not one that is marked BE_SKIP */
    BE_fflags_t down_flags;     /* Flags to be passed for further flattening */

    if ( flags & (BE_F_OOL_RTN | BE_F_PA_RTN) )
        down_flags = 0;
    else
        down_flags = BE_F_UNDER_OOL;
    /*
     * whenever we're flattening the formal parameter of an OOL
     * routine we also want the consequences of viewing it as the
     * formal parameter of a pointed_at routine.
     */
    if (BE_F_OOL_RTN & flags) flags |= BE_F_PA_RTN;

    /*
     * Flatten the parameter as though it were not OOL,
     * but retain [out_of_line]ness of substructures
     */
    if ( (type->rep_as_type != NULL) || (type->xmit_as_type != NULL) )
        down_flags |= flags;
    else
        down_flags |= (flags & ~BE_F_OOL_RTN);
    AST_CLR_OUT_OF_LINE(type);
    pp = flatp = BE_flatten_parameter(param, name, type, side, top_level,
                                open_array, down_flags);
    AST_SET_OUT_OF_LINE(type);

    /*
     * If we're flattening the formal parameter of an OOL routine, or a pointee
     * then we want to leave it as is.
     */
    if ( (flags & BE_F_OOL_RTN) || (flags & BE_F_POINTEE) )
        return flatp;

    /* Ignore [out_of_line] on a [transmit_as] */
    if (type->xmit_as_type != NULL)
        return flatp;

    /* The pointer for a [ptr] [out_of_line] array parameter is not ool */
    if ( BE_PI_Flags(flatp) & BE_PTR_ARRAY )
        return flatp;

    calls_before = BE_Calls_Before( flatp );

    /*
     * Otherwise, leave header pseudo-params alone ...
     */
    if ( (BE_PI_Flags(flatp) & (BE_ARRAY_HEADER | BE_OPEN_RECORD))
            && (type->kind != AST_pipe_k) )
    {
        BE_PI_Flags(flatp) |= BE_OOL_HEADER;
        pp = flatp->next;
    }

    /*
     * skip over struct aligning pseudo-params (they're handled in
     * the OOL routines) ...
     */
    if (pp->type->kind == AST_null_k)
    {
        BE_PI_Flags(pp) |= BE_SKIP;
        BE_Calls_Before(pp->next) = BE_Calls_Before(pp);
        pp = pp->next;
    }

    /*
     * copy open_record header info reference into the node that will
     * be marked BE_OOL so that this info is available at marshalling
     * time to spell the params to the OOL routine
     */
    if (BE_PI_Flags(flatp) & BE_OPEN_RECORD)
        BE_Orecord_Info(pp) = BE_Orecord_Info(flatp);

    /*
     * mark the resulting first flattened param as BE_OOL to trigger
     * marshalling via call to an appriately named OOL routine.
     */
    has_ptrs = BE_type_has_pointers(type);
    if ( has_ptrs )
        BE_PI_Flags(pp) |= BE_OOL_YANK_ME;
    BE_PI_Flags(pp) |= BE_OOL;
    if (calls_before != NULL)
    {
        if (calls_before->type == BE_call_ctx_handle)
            calls_before = NULL;
    }
    BE_Ool_Info(pp) = BE_new_ool_info(flatp, type, name, has_ptrs, false,
                                      calls_before, top_level);

    location_for_calls_after = pp;
    calls_after = BE_Calls_After(pp);
    for (pp = pp->next; pp != NULL; pp = pp->next)
    {
        BE_PI_Flags(pp) |= BE_SKIP;
        calls_after = BE_Calls_After(pp);
    }

    if (top_level && BE_any_pointers(flatp) && !(flags & BE_F_PA_RTN))
    {
        yanked = BE_yank_pointers(flatp);
        location_for_calls_after = yanked;
        Append(flatp, yanked);
    }
    BE_Calls_After(location_for_calls_after) = calls_after;

    return flatp;
}

/*
 * BE_flatten_parameter
 *
 * Allocates and initializes BE_param_i_t fields for parameters and dispatches
 * to the appropriate flattening routine.
 */
AST_parameter_n_t *BE_flatten_parameter
#ifdef PROTO
(
    AST_parameter_n_t *param,
    NAMETABLE_id_t name,
    AST_type_n_t *type,
    BE_side_t side,
    boolean top_level,  /* non-zero only if the routine is being invoked for
                         * a top-level parameter
                         */
    AST_parameter_n_t **open_array,
    BE_fflags_t flags
)
#else
(param, name, type, side, top_level, open_array, flags)
    AST_parameter_n_t *param;
    NAMETABLE_id_t name;
    AST_type_n_t *type;
    BE_side_t side;
    boolean top_level;
    AST_parameter_n_t **open_array;
    BE_fflags_t flags;
#endif
{
    if (param == NULL) return NULL;

    if (param->be_info.param == NULL) param->be_info.param = new_param_info();

    /*
     * Allow pointer flattening to take care of references to context handles
     */
    if ((AST_CONTEXT_RD_SET(type) || AST_CONTEXT_SET(param)) &&
        type->type_structure.pointer->pointee_type->kind == AST_void_k)
        return flatten_ctx_handle(param, name, type, side, top_level,
                                  open_array, flags);
    else if (AST_OUT_OF_LINE_SET(type))
        return flatten_ool(param, name, type, side, top_level, open_array,
                           flags);
    else if ( (type->rep_as_type != NULL) && (!(flags & BE_F_OOL_RTN)) )
        return flatten_rep_as(param, name, type, side, top_level,
                              open_array, flags);
    else if ( (type->xmit_as_type != NULL) && (!(flags & BE_F_OOL_RTN)) )
        return flatten_xmit_as(param, name, type, side, top_level,
                               open_array, flags);
    else if (type->kind == AST_structure_k)
        return flatten_structure(param, name, type, side, top_level,
                                 open_array, flags);
    else if (type->kind == AST_array_k)
        return flatten_array(param, name, type, side, top_level, open_array,
                             flags);
    else if (type->kind == AST_pointer_k)
    {
        AST_parameter_n_t *result, *original = *open_array;

        result = flatten_pointer(param, name, type, side, top_level, open_array,
                               flags);
        *open_array = original;
        BE_PI_Flags(result) &= ~BE_IN_ORECORD;
        return result;
    }
    else if (type->kind == AST_disc_union_k)
        return flatten_union(param, name, type, side, top_level, open_array,
                             flags);
    else if (type->kind == AST_pipe_k)
        return flatten_pipe(param, name, type, side, top_level, open_array,
                            flags);
    /*
     * all other parameters: just copy them
     */
    else return copy_parameter(param, name, type);
}

/*
 * flatten_field_attributes
 *
 * Allocate a new field attribute structure with first/last/length/min/max/size
 * references pointing at their flattened counterparts.
 * For now, ignore all attributes not on the first dimension.
 */
AST_field_attr_n_t *flatten_field_attributes
#ifdef PROTO
(
    AST_parameter_n_t *param
)
#else
(param)
    AST_parameter_n_t *param;
#endif
{
    AST_field_attr_n_t *new_attrs;
    int index_count = param->type->type_structure.array->index_count;

    if (param->field_attrs == NULL) return NULL;

    new_attrs = (AST_field_attr_n_t *)BE_ctx_malloc(sizeof(AST_field_attr_n_t));
    new_attrs->be_info = param->field_attrs->be_info;
    new_attrs->first_is_vec = new_attrs->last_is_vec = new_attrs->length_is_vec =
    new_attrs->min_is_vec = new_attrs->max_is_vec = new_attrs->size_is_vec = NULL;


    Flatten_Field_Ref(new_attrs, param, first_is_vec, index_count);
    Flatten_Field_Ref(new_attrs, param, last_is_vec, index_count);
    Flatten_Field_Ref(new_attrs, param, length_is_vec, index_count);
    Flatten_Field_Ref(new_attrs, param, min_is_vec, index_count);
    Flatten_Field_Ref(new_attrs, param, max_is_vec, index_count);
    Flatten_Field_Ref(new_attrs, param, size_is_vec, index_count);

    return new_attrs;
}

/*
 * BE_flatten_field_attrs
 */
void BE_flatten_field_attrs
#ifdef PROTO
(
    AST_parameter_n_t *params
)
#else
(params)
    AST_parameter_n_t *params;
#endif
{
    AST_parameter_n_t *param;

    for (param = params; param; param = param->next)
    {
        if (!(BE_PI_Flags(param) & BE_FATTRS_FLAT))
        {
            if (param->type->kind == AST_array_k)
            {
                debug(("Flattening field attributes for %s\n",
                       BE_get_name(param->name)));
                param->field_attrs = flatten_field_attributes(param);
                BE_PI_Flags(param) |= BE_FATTRS_FLAT;
            }
            else if (param->type->kind == AST_pointer_k &&
                     BE_PI_Flags(param) & (BE_PTR_ARRAY | BE_ARRAYIFIED))
            {
                debug(("Flattening field attributes for [ptr] array %s\n",
                       BE_get_name(param->name)));
                BE_Ptr_Info(param)->flat_array->field_attrs =
                    flatten_field_attributes(BE_Ptr_Info(param)->flat_array);
                BE_PI_Flags(param) |= BE_FATTRS_FLAT;
            }
        }
    }
}

/*
 * BE_propagate_orecord_info
 *
 * If the flattened parameter is of open record type, propagate the Orecord_Info
 * onto all the elements in the flattened chain.
 */
void BE_propagate_orecord_info
#ifdef PROTO
(
    AST_parameter_n_t *flat
)
#else
( flat )
    AST_parameter_n_t *flat;
#endif
{
    AST_parameter_n_t *pp;

    if ( BE_PI_Flags(flat) & BE_OPEN_RECORD )
    {
        for (pp = flat; pp->next != NULL; pp = pp->next)
        {
            BE_Orecord_Info(pp->next) = BE_Orecord_Info(pp);
        }
    }
}


/*
 * BE_flatten_parameters
 *
 * Flattens the parameters of an operation for the direction passed.
 * Strops '_' onto parameter names.  Allocates and initializes BE_param_i_t
 * entries for the original and the flattened parameters.
 */
void BE_flatten_parameters
#ifdef PROTO
(
    AST_operation_n_t *oper,
    BE_side_t side
)
#else
(oper, side)
    AST_operation_n_t *oper;
    BE_side_t side;
#endif
{
    AST_parameter_n_t *param, *open_array = NULL;
    NAMETABLE_id_t name;

    /*
     * Pass 1: Flatten the parameters
     */
    for (param = oper->parameters; param; param = param->next)
    {
        param->be_info.param = new_param_info();

        name = param->be_info.param->name =
            NAMETABLE_add_derived_name(param->name, "%s_");

        if (param->type->kind == AST_handle_k ||
            param->type->kind == AST_pointer_k &&
                param->type->type_structure.pointer->pointee_type->kind ==
                    AST_handle_k) continue;

        param->be_info.param->flat =
           BE_flatten_parameter(param, name, param->type, side, true,
                                &open_array, 0);
        BE_propagate_orecord_info( param->be_info.param->flat );
        Append(BE_Flat_Params(oper), param->be_info.param->flat);
    }

    if (oper->result->type->kind != AST_void_k)
    {
        param = oper->result;

        param->be_info.param = new_param_info();

        name = param->be_info.param->name =
            NAMETABLE_add_derived_name(param->name, "%s_");

        param->be_info.param->flat =
           BE_flatten_parameter(param, name, param->type, side, true,
                                &open_array, 0);
        Append(BE_Flat_Params(oper), param->be_info.param->flat);
    }

    /*
     * Pass 2: Fix field attributes
     */
    BE_flatten_field_attrs(BE_Flat_Params(oper));

#ifdef DEBUG_VERBOSE
    if (BE_dump_flat)
    {
        printf("\nflat dump : BE_flatten_parameters\n");
        printf(  "---------------------------------\n");
        traverse(BE_Flat_Params(oper), 2);
    }
#endif

}
