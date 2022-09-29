/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: cspell.c,v $
 * Revision 1.4  1994/02/21 19:01:51  jwag
 * add BB slot numbers; start support for interface slot numbers; lots of ksgen changes
 *
 * Revision 1.3  1993/09/13  16:11:00  jwag
 * split out typedef changes from ksgen changes
 *
 * Revision 1.2  1993/09/08  20:15:18  jwag
 * first ksgen hack
 *
 * Revision 1.1  1993/08/31  06:48:01  jwag
 * Initial revision
 *
 * Revision 1.1.2.3  1993/01/03  21:38:46  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  14:33:49  bbelch]
 *
 * Revision 1.1.2.2  1992/12/23  18:45:06  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  01:01:01  zeliff]
 * 
 * Revision 1.1  1992/01/19  03:00:56  devrcs
 * 	Initial revision
 * 
 * $EndLog$
 */
/*
**
**  Copyright (c) 1990 by
**      Hewlett-Packard Company, Palo Alto, Ca. &
**      Digital Equipment Corporation, Maynard, Mass.
**
**
**  NAME:
**
**      cspell.c
**
**  FACILITY:
**
**      Interface Definition Language (IDL) Compiler
**
**  ABSTRACT:
**
**      Routines needed by more than one module to output C source
**
**  VERSION: DCE 1.0
**
*/

#include <nidl.h>
#include <ast.h>
#include <cspell.h>
#include <cspeldcl.h>
#include <genpipes.h>
#include <bedeck.h>


/*
 * The following define, typedef, and four static functions are the
 * rep for the type_tail_t data type which represents the residual suffix
 * which must be emitted in a declarator after the name being typed.
 */

#define MAX_TAIL_LEN 15

typedef enum {p_k, a_k, f_k} type_kind_t;
typedef struct {
    int len;
    struct {
        type_kind_t kind;
        union {
            struct {
                AST_array_n_t *array;
                boolean in_typedef_or_struct;
            } array_info;
            struct {
                AST_parameter_n_t *param_list;
                boolean function_def;
            } function_info;
        } content;
    }  vec[MAX_TAIL_LEN];
} type_tail_t;

static void CSPELL_add_paren_to_tail
#ifdef PROTO
(
    type_tail_t *tail
)
#else
(tail)
type_tail_t *tail;
#endif
{
    int i;

    i = (tail->len) ++;
    if (tail->len > MAX_TAIL_LEN) INTERNAL_ERROR("Data structure too compilicated; Tail array overflow");
    (tail->vec)[i].kind = p_k;
}

static void CSPELL_add_array_to_tail
#ifdef PROTO
(
    type_tail_t *tail,
    AST_array_n_t *array,
    boolean in_typedef_or_struct
)
#else
(tail, array, in_typedef_or_struct)
type_tail_t *tail;
AST_array_n_t *array;
boolean in_typedef_or_struct;
#endif
{
    int i;

    i = (tail->len) ++;
    if (tail->len > MAX_TAIL_LEN) INTERNAL_ERROR("Data structure too compilicated; Tail array overflow");
    (tail->vec)[i].kind = a_k;
    (tail->vec)[i].content.array_info.array = array;
    (tail->vec)[i].content.array_info.in_typedef_or_struct =
        in_typedef_or_struct;
}

static void CSPELL_add_func_to_tail
#ifdef PROTO
(
    type_tail_t *tail,
    AST_parameter_n_t *pl,
    boolean function_def
)
#else
(tail, pl, function_def)
type_tail_t *tail;
AST_parameter_n_t *pl;
boolean function_def;
#endif
{
    int i;

    i = (tail->len) ++;
    if (tail->len > MAX_TAIL_LEN) INTERNAL_ERROR("Data structure too compilicated; Tail array overflow");
    (tail->vec)[i].kind = f_k;
    (tail->vec)[i].content.function_info.param_list = pl;
    (tail->vec)[i].content.function_info.function_def = function_def;
}

static void CSPELL_array_bounds (
#ifdef PROTO
    FILE *fid,
    AST_array_n_t *array,
    boolean in_typedef_or_struct
#endif
);

static void CSPELL_function_sig (
#ifdef PROTO
    FILE *fid,
    AST_parameter_n_t *pp,
    boolean func_def
#endif
);

static void CSPELL_type_tail
#ifdef PROTO
(
    FILE *fid,
    type_tail_t *tail
)
#else
(fid, tail)
FILE *fid;
type_tail_t *tail;
#endif
{
    int i;

    for (i = 0; i < tail->len; i++)
        switch (tail->vec[i].kind) {
            case p_k:
                fprintf (fid, ")");
                break;
            case a_k:
                CSPELL_array_bounds (
                    fid,
                    tail->vec[i].content.array_info.array,
                    tail->vec[i].content.array_info.in_typedef_or_struct);
                break;
            case f_k:
                CSPELL_function_sig (
                    fid,
                    tail->vec[i].content.function_info.param_list,
                    tail->vec[i].content.function_info.function_def);
                break;
            default:
                INTERNAL_ERROR("Invalid tail kind");
        }
}


/*
 * spell_name
 *
 * Output an identifier to the file specified
 */
void spell_name
#ifdef PROTO
(
    FILE *fid,
    NAMETABLE_id_t name
)
#else
(fid, name)
    FILE *fid;
    NAMETABLE_id_t name;
#endif
{
    char *str;

    NAMETABLE_id_to_string (name, &str);
    fprintf (fid, "%s", str);
}


static void CSPELL_type_exp (
#ifdef PROTO
    FILE *fid,
    AST_type_n_t *tp,
    type_tail_t *tail,
    AST_type_n_t *in_typedef,
    boolean in_struct,
    boolean func_def,
    boolean spell_tag
#endif
);

/*
 * C S P E L L _ s c a l a r _ t y p e _ s u f f i x
 */
boolean CSPELL_scalar_type_suffix
#ifdef PROTO
(
    FILE *fid,
    AST_type_n_t *tp
)
#else
(fid, tp)
    FILE         *fid;
    AST_type_n_t *tp;
#endif
{
    boolean result = true;

    /* prepend a 'u' for unsigned types */
    switch (tp->kind) {
        case AST_small_unsigned_k:
        case AST_short_unsigned_k:
        case AST_long_unsigned_k:
        case AST_hyper_unsigned_k:
            fprintf (fid, "u");
    }

    switch (tp->kind) {
        case AST_boolean_k:
            fprintf (fid, "boolean");
            break;

        case AST_byte_k:
            fprintf (fid, "byte");
            break;

        case AST_character_k:
            fprintf (fid, "char");
            break;

        case AST_small_integer_k:
        case AST_small_unsigned_k:
            fprintf (fid, "small_int");
            break;

        case AST_short_integer_k:
        case AST_short_unsigned_k:
            fprintf (fid, "short_int");
            break;

        case AST_long_integer_k:
        case AST_long_unsigned_k:
            fprintf (fid, "long_int");
            break;

        case AST_hyper_integer_k:
        case AST_hyper_unsigned_k:
            fprintf (fid, "hyper_int");
            break;

        case AST_short_float_k:
            fprintf (fid, "short_float");
            break;

        case AST_long_float_k:
            fprintf (fid, "long_float");
            break;

        case AST_enum_k:
            fprintf (fid, "enum");
            break;

        default:
            result = false;
    }
#ifdef __sgi
    if (BE_kstypes)
        fprintf(fid, "_t");
#endif
    return result;
}

/*
 * s p e l l _ i d l _ s c a l a r _ t y p e _ n a m e
 */
static void spell_idl_scalar_type_name
#ifdef PROTO
(
    FILE *fid,
    AST_type_n_t *tp
)
#else
(fid, tp)
    FILE         *fid;
    AST_type_n_t *tp;
#endif
{

#ifdef __sgi
    if (BE_kstypes)
	fprintf(fid, "ks_");
    else
#endif
    fprintf(fid, "idl_");

    if (CSPELL_scalar_type_suffix(fid, tp))
        fprintf(fid, " ");
    else
        INTERNAL_ERROR("Invalid type kind");
}

/*
 * CSPELL_function_sig
 *
 * Spell a function's parameter list
 */
static void CSPELL_function_sig
#ifdef PROTO
(
    FILE *fid,
    AST_parameter_n_t *pp,
    boolean func_def
)
#else
(fid, pp, func_def)
    FILE *fid;
    AST_parameter_n_t *pp;
    boolean func_def;
#endif
{
    boolean            first = true;

#ifdef __sgi
    if (BE_kstypes) {
	    fprintf (fid, "(\n");
	    CSPELL_parameter_list (fid, pp);
	    fprintf (fid, ")");
	    return;
    }
#endif
    if (!func_def) {
        fprintf (fid, "(\n#ifdef IDL_PROTOTYPES\n");
        CSPELL_parameter_list (fid, pp);
        fprintf (fid, "\n#endif\n)");
    } else {
        fprintf (fid, "\n#ifdef IDL_PROTOTYPES\n(\n");
        CSPELL_parameter_list (fid, pp);
        fprintf (fid, "\n)\n#else\n(");
        for (; pp != NULL; pp = pp->next) {
            if (first)
                first = false;
            else
                fprintf (fid, ", ");
            if (pp->be_info.param)
                spell_name(fid, pp->be_info.param->name);
            else spell_name (fid, pp->name);
            }
        fprintf (fid, ")\n#endif\n");
    }
}



/*
 * CSPELL_array_bounds
 *
 * Spell an arrays's bounds
 *
 */
static void CSPELL_array_bounds
#ifdef PROTO
(
    FILE *fid,
    AST_array_n_t *array,
    boolean in_typedef_or_struct
)
#else
(fid, array, in_typedef_or_struct)
    FILE *fid;
    AST_array_n_t *array;
    boolean in_typedef_or_struct;
#endif
{
    int i;
    AST_array_index_n_t *index_array_ptr;
    long array_dim_size;    /* Size of array in current dimension */

    index_array_ptr = array->index_vec;
    for (i = 0; i < array->index_count; i++) {
        if ( (AST_FIXED_LOWER_SET(index_array_ptr))
             && (AST_FIXED_UPPER_SET(index_array_ptr)) ) {
            /* Fixed bounds, convert to C syntax */
            array_dim_size = index_array_ptr->upper_bound->value.int_val
                             - index_array_ptr->lower_bound->value.int_val + 1;
            fprintf (fid, "[%ld]\0", array_dim_size);
            }
        else {
            /* Varying bounds */
            if ((i == 0) && ( ! in_typedef_or_struct )) fprintf (fid, "[]\0");
            else fprintf (fid, "[1]\0");
            /* Tell lies as C does not understand conformant non-first bound
               or conformant bounds within structures or typedefs  */
            }
        index_array_ptr ++;
    }
}


/*
 * CSPELL_pipe_struct_routine_decl
 *
 * Spell one function pointer type of a pipe's rep type
 */
static void CSPELL_pipe_struct_routine_decl
#ifdef PROTO
(
    FILE *fid,
    AST_type_n_t *p_pipe_type,
    BE_pipe_routine_k_t routine_kind
)
#else
( fid, p_pipe_type, routine_kind )
    FILE *fid;
    AST_type_n_t *p_pipe_type;
    BE_pipe_routine_k_t routine_kind;
#endif
{
    type_tail_t tail;

    fprintf( fid, "void (* %s)(\n\0",
                  (routine_kind == BE_pipe_push_k) ? "push" :
                  ((routine_kind == BE_pipe_pull_k) ? "pull" : "alloc") );
    fprintf (fid, "#ifdef IDL_PROTOTYPES\n" );
    fprintf (fid, "rpc_ss_pipe_state_t state,\n" );
    if ( routine_kind == BE_pipe_alloc_k )
    {
        fprintf( fid, "idl_ulong_int bsize,\n" );
    }

    tail.len = 0;
    CSPELL_type_exp (fid, p_pipe_type->type_structure.pipe->base_type,
                     &tail, NULL, false, false, true);
    fprintf( fid, "%s*buf,\n\0",
                  (routine_kind == BE_pipe_alloc_k) ? "*" : "");
    CSPELL_type_tail (fid, &tail);

    if ( routine_kind == BE_pipe_pull_k )
    {
        fprintf( fid, "idl_ulong_int esize,\n" );
    }
    fprintf( fid, "idl_ulong_int %c%ccount\n\0",
                   ((routine_kind == BE_pipe_push_k) ? ' ' : '*'),
                   ((routine_kind == BE_pipe_alloc_k) ? 'b' : 'e') );
    fprintf (fid, "#endif\n" );
    fprintf (fid, ");\n");
}

/*
 * CSPELL_pipe_def
 *
 * Spell a pipe's concrete rep type
 */
static void CSPELL_pipe_def
#ifdef PROTO
(
    FILE *fid,
    AST_type_n_t *p_pipe_type
)
#else
( fid, p_pipe_type )
    FILE *fid;
    AST_type_n_t *p_pipe_type;
#endif
{
        char *name_str;

        /* Declare the structure that represents the pipe */
        fprintf( fid, "struct " );
        spell_name (fid, p_pipe_type->name);
        fprintf( fid, " {\n" );
        CSPELL_pipe_struct_routine_decl( fid, p_pipe_type, BE_pipe_pull_k );
        CSPELL_pipe_struct_routine_decl( fid, p_pipe_type, BE_pipe_push_k );
        CSPELL_pipe_struct_routine_decl( fid, p_pipe_type, BE_pipe_alloc_k );
        fprintf( fid, "rpc_ss_pipe_state_t state;\n" );
        fprintf( fid, "} " );
}


/*
 * CSPELL_type_exp
 *
 * Spell a type exp by writing its prefix (i.e. the portion before the
 * name in a declarator) to file fid and by building a rep of its suffix
 * in the tail data structure.
 */
static void CSPELL_type_exp
#ifdef PROTO
(
    FILE *fid,
    AST_type_n_t *tp,
    type_tail_t *tail,
    AST_type_n_t *in_typedef,
    boolean in_struct,
    boolean func_def,
    boolean spell_tag
)
#else
(fid, tp, tail, in_typedef, in_struct, func_def, spell_tag)
    FILE *fid;
    AST_type_n_t *tp;
    type_tail_t *tail;
    AST_type_n_t *in_typedef;
    boolean in_struct;
    boolean func_def;
    boolean spell_tag;
#endif
{
    AST_field_n_t           *fp;
    AST_arm_n_t             *cp;
    AST_disc_union_n_t      *vp;
    AST_constant_n_t        *ecp;
    int                     pointer_count;
    AST_type_n_t            *pointee_tp;
    boolean                 parenthesized;
    boolean                 first = true;

    /*
     * If we are in the process of defining a type which was defined as
     * another type, emit the definiend.
     */
    if (in_typedef == tp && tp->defined_as)
        CSPELL_type_exp (
            fid,
            tp->defined_as,
            tail,
            in_typedef,
            in_struct,
            func_def,
            spell_tag);

    /*
     * If the type has the [represent_as] attribute, emit its local rep name.
     */
    else if (in_typedef != tp && tp->rep_as_type
            && !(in_typedef != NULL && in_typedef->name == tp->name))
    {
        fprintf( fid, "/* Type must appear in user header or IDL */ " );
        spell_name (fid, tp->rep_as_type->type_name);
        fprintf (fid, " ");
    }

    /*
     * If we are spelling a type which was expressed simply as "struct foo"
     * or "union foo" then just echo its original expression
     */
    else if (AST_DEF_AS_TAG_SET(tp))
    {
        fprintf (fid, "struct ");
        if (tp->kind == AST_structure_k)
            spell_name (fid, tp->type_structure.structure->tag_name);
        else if (tp->kind == AST_disc_union_k)
            spell_name (fid, tp->type_structure.disc_union->tag_name);
        fprintf (fid, " ");
    }

    /*
     * If we are not in the process of defining the type and it has a name,
     * emit it.
     */
    else if (in_typedef != tp && tp->name != NAMETABLE_NIL_ID)
    {
        spell_name (fid, tp->name);
        fprintf (fid, " ");
    }

    /*
     * The DEF_AS_TAG case above took care of in_typedef cases where only a
     * tag name should be emitted.  If we have gotten this far and we are still
     * in_typedef, then the complete structure or union body must be emitted.
     * However, if we are not in_typedef, then we should just use a tagname
     * here if one exists.
     */
    else if (in_typedef == NULL && tp->kind == AST_structure_k &&
             tp->type_structure.structure->tag_name != NAMETABLE_NIL_ID)
    {
        fprintf (fid, "struct ");
        spell_name (fid, tp->type_structure.structure->tag_name);
        fprintf (fid, " ");
    }

    else if (in_typedef == NULL && tp->kind == AST_disc_union_k &&
             tp->type_structure.disc_union->tag_name != NAMETABLE_NIL_ID)
    {
        fprintf (fid, "struct ");
        spell_name (fid, tp->type_structure.disc_union->tag_name);
        fprintf (fid, " ");
    }

    else switch (tp->kind) {

        case AST_boolean_k:
        case AST_byte_k:
        case AST_small_integer_k:
        case AST_small_unsigned_k:
        case AST_short_integer_k:
        case AST_short_unsigned_k:
        case AST_long_integer_k:
        case AST_long_unsigned_k:
        case AST_hyper_integer_k:
        case AST_hyper_unsigned_k:
        case AST_character_k:
        case AST_short_float_k:
        case AST_long_float_k:
            spell_idl_scalar_type_name (fid, tp);
            break;

        case AST_handle_k:
            fprintf (fid, "handle_t ");
            break;

        case AST_enum_k:
            fprintf (fid, "enum {");
            for (ecp = tp->type_structure.enumeration->enum_constants;
                 ecp != NULL; ecp = ecp->next) {
                if (first)
                    first = false;
                else
                    fprintf (fid, ",\n");
                spell_name (fid, ecp->name);
            }
            fprintf (fid, "} ");
            break;

        case AST_structure_k:
            fprintf (fid, "struct ");
            if ( spell_tag )
                spell_name (fid, tp->type_structure.structure->tag_name);
            fprintf (fid, " {\n");
            for (fp = tp->type_structure.structure->fields; fp != NULL;
                 fp = fp->next) {
                CSPELL_typed_name (
                    fid,
                    fp->type,
                    fp->name,
                    in_typedef,
                    true,
                    true);
                fprintf (fid, ";\n");
            }
            fprintf (fid, "} ");
            break;

        case AST_disc_union_k:
            fprintf (fid, "struct ");
            if ( spell_tag )
                spell_name (fid, tp->type_structure.disc_union->tag_name);
            fprintf (fid, " {\n");
            vp = tp->type_structure.disc_union;
            /*
             * Use the parameters the procedure was called with
             * We know we are declaring a scalar and they will be set null
             */
            CSPELL_typed_name (
                fid,
                vp->discrim_type,
                vp->discrim_name,
                in_typedef,
                true,
                true);
            fprintf (fid, ";\nunion {\n");
            for (cp = vp->arms; cp != NULL; cp = cp->next)
            {
                CSPELL_labels (fid, cp->labels);
                if (cp->type == NULL)
                    fprintf (fid, "/* Empty arm */\n");
                else
                {
                    CSPELL_typed_name (
                        fid,
                        cp->type,
                        cp->name,
                        in_typedef,
                        true,
                        true);
                    fprintf (fid, ";\n");
                }
            }
            fprintf (fid, "} ");
            spell_name (fid, vp->union_name);
            fprintf (fid, ";\n");
            fprintf (fid, "} ");
            break;

        case AST_pipe_k:
            CSPELL_pipe_def(fid, tp);
            break;

        case AST_void_k:
            fprintf (fid, "void ");
            break;

        case AST_array_k:
            CSPELL_add_array_to_tail (
                tail,
                tp->type_structure.array,
                (tp == in_typedef) || in_struct);
            CSPELL_type_exp (
                fid,
                tp->type_structure.array->element_type,
                tail,
                in_typedef,
                in_struct,
                false,
                spell_tag);
            break;

        case AST_pointer_k:
            pointer_count = 0;
            pointee_tp = tp;
            while (pointee_tp->kind == AST_pointer_k
                   && (in_typedef == pointee_tp ||
                       pointee_tp->name == NAMETABLE_NIL_ID))
            {
                pointee_tp = pointee_tp->type_structure.pointer->pointee_type;
                pointer_count++;
            }
            if ( parenthesized = (pointee_tp->kind == AST_array_k
                               || pointee_tp->kind == AST_function_k))
                CSPELL_add_paren_to_tail (tail);

#ifdef __sgi
	    /* why have a special void *?? */
            if (!BE_kstypes && pointee_tp->kind == AST_void_k) {
#else
            if (pointee_tp->kind == AST_void_k) {
#endif
                fprintf (fid, "idl_void_p_t ");
                pointer_count--;
                }
            else
                CSPELL_type_exp (
                    fid,
                    pointee_tp,
                    tail,
                    in_typedef,
                    in_struct,
                    false,
                    spell_tag);

            if (parenthesized)
                fprintf (fid, "(");
            for (; pointer_count; pointer_count--)
                fprintf (fid, "*");
            break;

        case AST_function_k:
            CSPELL_add_func_to_tail(
                tail,
                tp->type_structure.function->parameters,
                func_def);
            CSPELL_type_exp(
                fid,
                tp->type_structure.function->result->type,
                tail,
                in_typedef,
                in_struct,
                false,
                true);
            break;

        default:
            INTERNAL_ERROR("Unknown type kind in CSPELL_type_exp");
    }

}


/*
 * CSPELL_typed_name
 */
void CSPELL_typed_name
#ifdef PROTO
(
    FILE *fid,
    AST_type_n_t *type,
    NAMETABLE_id_t name,
    AST_type_n_t *in_typedef,
    boolean in_struct,
    boolean spell_tag
)
#else
(fid, type, name, in_typedef, in_struct, spell_tag)
    FILE   *fid;
    AST_type_n_t *type;
    NAMETABLE_id_t name;
    AST_type_n_t *in_typedef;
    boolean in_struct;
    boolean spell_tag;
#endif
{
    type_tail_t tail;

    tail.len = 0;
    CSPELL_type_exp (fid, type, &tail, in_typedef, in_struct, false, spell_tag);
    spell_name (fid, name);
    CSPELL_type_tail (fid, &tail);
}


/*
 * CSPELL_function_def_header
 */
void CSPELL_function_def_header
#ifdef PROTO
(
    FILE *fid,
    AST_operation_n_t *oper,
    NAMETABLE_id_t name
)
#else
(fid, oper, name)
    FILE   *fid;
    AST_operation_n_t *oper;
    NAMETABLE_id_t name;
#endif
{
    type_tail_t tail;
    AST_type_n_t func_type_node;

    func_type_node = *BE_function_p;
    func_type_node.type_structure.function = oper;

    tail.len = 0;
    CSPELL_type_exp (fid, &func_type_node, &tail, NULL, false, true, true);
    spell_name (fid, name);
    CSPELL_type_tail (fid, &tail);
}


/*
 * CSPELL_type_exp_simple
 */
void CSPELL_type_exp_simple
#ifdef PROTO
(
    FILE *fid,
    AST_type_n_t *tp
)
#else
(fid, tp)
    FILE *fid;
    AST_type_n_t *tp;
#endif
{
    CSPELL_typed_name(fid, tp, NAMETABLE_NIL_ID, NULL, false, true);
}

/*
 * CSPELL_var_decl
 *
 * Spell a variable declaration
 */
void CSPELL_var_decl
#ifdef PROTO
(
    FILE *fid,
    AST_type_n_t *type,
    NAMETABLE_id_t name
)
#else
(fid, type, name)
    FILE *fid;
    AST_type_n_t *type;
    NAMETABLE_id_t name;
#endif
{
    CSPELL_typed_name (fid, type, name, NULL, false, true);
    fprintf (fid, ";\n");
}


/*
 * CSPELL_cast_exp
 */
void CSPELL_cast_exp
#ifdef PROTO
(
    FILE *fid,
    AST_type_n_t *tp
)
#else
(fid, tp)
    FILE   *fid;
    AST_type_n_t *tp;
#endif
{
    fprintf (fid, "(");
    CSPELL_typed_name (fid, tp, NAMETABLE_NIL_ID, NULL, false, true);
    fprintf (fid, ")");
}

/*
 * CSPELL_std_include
 *
 * Output #defines and #includes needed at the start of generated stubs
 */
void CSPELL_std_include
#ifdef PROTO
(
    FILE *fid,
    char header_name[],
    BE_output_k_t filetype,
    int op_count
)
#else
(fid, header_name, filetype, op_count)
    FILE *fid;
    char header_name[];
    BE_output_k_t filetype;
    int op_count;
#endif
{
    fprintf (fid, "#define IDL_GENERATED\n#define IDL_");
    switch (filetype) {
        case BE_client_stub_k:
            fprintf (fid, "CSTUB\n");
            break;
        case BE_server_stub_k:
            fprintf (fid, "SSTUB\n");
            break;
        case BE_server_aux_k:
            fprintf (fid, "SAUX\n");
            break;
        case BE_client_aux_k:
            fprintf (fid, "CAUX\n");
            break;
        default:
            INTERNAL_ERROR("Invalid file type");
        }

    fprintf (fid, USER_INCLUDE_TEMPLATE, header_name);

    if (op_count == 0 && (filetype == BE_server_aux_k || filetype == BE_client_aux_k))
        fprintf (fid, INCLUDE_TEMPLATE, "rpc.h");

    fprintf (fid, INCLUDE_TEMPLATE, "stubbase.h");
}
