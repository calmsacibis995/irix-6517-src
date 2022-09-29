/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: hdgen.c,v $
 * Revision 1.1  1993/08/31 06:48:01  jwag
 * Initial revision
 *
 * Revision 1.1.4.3  1993/01/03  21:39:47  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  14:35:30  bbelch]
 *
 * Revision 1.1.4.2  1992/12/23  18:47:33  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  01:02:44  zeliff]
 * 
 * Revision 1.1.2.3  1992/05/13  18:59:09  harrow
 * 	Do not require G Floating format for VAX architecture if interface
 * 	is [local].
 * 	[1992/05/13  11:31:56  harrow]
 * 
 * Revision 1.1.2.2  1992/03/24  14:10:28  harrow
 * 	Enclose value of integer constants in parens
 * 	to prevent precedence problems when the constant
 * 	is referenced in application code (specifically when
 * 	it is negative).
 * 	[1992/03/16  21:34:03  harrow]
 * 
 * Revision 1.1  1992/01/19  03:01:16  devrcs
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
**      hdgen.c
**
**  FACILITY:
**
**      Interface Definition Language (IDL) Compiler
**
**  ABSTRACT:
**
**      BE_gen_c_header which emits the C header file for an interface.
**
**  VERSION: DCE 1.0
**
*/

#include <nidl.h>
#include <ast.h>
#include <cspell.h>
#include <cspeldcl.h>
#include <files.h>
#include <genpipes.h>
#include <hdgen.h>

extern int yylineno;

/*
 * mapchar
 *
 * Maps a single character into a string suitable for emission
 */
char *mapchar
#ifdef PROTO
(
    AST_constant_n_t *cp,   /* Constant node with kind == AST_char_const_k */
    boolean warning_flag    /* unused */
)
#else
(cp, warning_flag)
    AST_constant_n_t *cp;   /* Constant node with kind == AST_char_const_k */
    boolean warning_flag;   /* unused */
#endif
{
    char c = cp->value.char_val;
    static char buf[10];

    switch (c)
    {
        case '\a': return "\\a";
        case '\b': return "\\b";
        case '\f': return "\\f";
        case '\n': return "\\n";
        case '\r': return "\\r";
        case '\t': return "\\t";
        case '\v': return "\\v";
        case '\\': return "\\\\";
        case '\'': return "\\\'";
        case '\"': return "\\\"";
        default:
            if (c >= ' ' && c <= '~')
                sprintf(buf, "%c", c);
            else
                sprintf(buf, "\\%03o", c);
            return buf;
    }
}

static void CSPELL_constant_def
#ifdef PROTO
(
    FILE *fid,
    AST_constant_n_t *cp,
    char *cast
)
#else
(fid, cp, cast)
    FILE *fid;
    AST_constant_n_t *cp;
    char *cast;
#endif
{
    int i;
    char *s;

    fprintf (fid, "#define ");
    spell_name (fid, cp->name);
    fprintf (fid, " %s", cast);
    if (cp->defined_as != NULL)
        spell_name (fid, cp->defined_as->name);
    else
        switch (cp->kind) {
            case AST_nil_const_k:
                fprintf (fid, "NULL");
                break;
            case AST_int_const_k:
                fprintf (fid, "(%ld)", cp->value.int_val);
                break;
            case AST_hyper_int_const_k:
                fprintf (fid, "{%ld,%lu}",
                        cp->value.hyper_int_val.high,
                        cp->value.hyper_int_val.low);
                break;
            case AST_char_const_k:
                fprintf (fid, "'%s'", mapchar(cp, TRUE));
                break;
            case AST_string_const_k:
                STRTAB_str_to_string (cp->value.string_val, &s);
                fprintf (fid, "\"%s\"", s);
                break;
            case AST_boolean_const_k:
                fprintf (fid, "%s",
                         cp->value.boolean_val ? "ndr_true" : "ndr_false");
                break;
        }
    fprintf (fid, "\n");
}


static void CSPELL_operation_def
#ifdef PROTO
(
    FILE *fid,
    AST_operation_n_t *op
)
#else
(fid, op)
    FILE *fid;
    AST_operation_n_t *op;
#endif
{
    AST_type_n_t       func_type_node;

    func_type_node = *BE_function_p;
    func_type_node.type_structure.function = op;

    fprintf (fid, "extern ");
    CSPELL_typed_name (fid, &func_type_node, op->name, NULL, false, true);
    fprintf (fid, ";\n");
}


void CSPELL_type_def
#ifdef PROTO
(
    FILE *fid,
    AST_type_n_t *tp,
    boolean spell_tag
)
#else
(fid, tp, spell_tag)
    FILE *fid;
    AST_type_n_t *tp;
    boolean spell_tag;
#endif
{
    AST_type_n_t       *spelled_tp;
    char               *cast;
    AST_constant_n_t   *cp;

    fprintf (fid, "typedef ");
    CSPELL_typed_name (fid, tp, tp->name, tp, false, spell_tag);
    fprintf (fid, ";\n");

    /* declare the "bind" and "unbind" routines as extern's for [handle] types */
    if (AST_HANDLE_SET(tp) && (tp->kind != AST_handle_k)) {

        fprintf (fid, "handle_t ");
        spell_name (fid, tp->name);
        fprintf (fid, "_bind(\n#ifdef IDL_PROTOTYPES\n");
        spell_name (fid, tp->name);
        fprintf (fid, " h\n#endif\n);\n");

        fprintf (fid, "void ");
        spell_name (fid, tp->name);
        fprintf (fid, "_unbind(\n#ifdef IDL_PROTOTYPES\n");
        spell_name (fid, tp->name);
        fprintf (fid, " uh,\nhandle_t h\n#endif\n);\n");
    }
}


static void CSPELL_exports
#ifdef PROTO
(
    FILE *fid,
    AST_export_n_t *ep
)
#else
(fid, ep)
    FILE           *fid;
    AST_export_n_t *ep;
#endif
{
    for (; ep; ep = ep->next) {
        switch (ep->kind) {
            case AST_constant_k:
                CSPELL_constant_def (fid, ep->thing_p.exported_constant, "");
                break;
            case AST_operation_k:
                CSPELL_operation_def (fid, ep->thing_p.exported_operation);
                break;
            case AST_type_k:
                CSPELL_type_def (fid, ep->thing_p.exported_type, true);
                break;
            default:
               INTERNAL_ERROR( "Unknown export type in CSPELL_exports" );
               break;
            }
        }
}

static CSPELL_epv_field
#ifdef PROTO
(
    FILE *fid,
    AST_operation_n_t *op
)
#else
(fid, op)
    FILE   *fid;
    AST_operation_n_t *op;
#endif
{
    AST_type_n_t       type_node_a, type_node_b;
    AST_pointer_n_t    pointer_node;

    type_node_a = *BE_pointer_p;
    type_node_a.type_structure.pointer = &pointer_node;

    type_node_b = *BE_function_p;
    type_node_b.type_structure.function = op;

    pointer_node.fe_info = NULL;
    pointer_node.be_info.other = NULL;
    pointer_node.pointee_type = &type_node_b;

    CSPELL_typed_name (fid, &type_node_a, op->name, NULL, false, true);
    fprintf (fid, ";\n");
}


static void CSPELL_epv_type_and_var
#ifdef PROTO
(
    FILE *fid,
    NAMETABLE_id_t if_name,
    unsigned long int if_version,
    AST_export_n_t *ep,
    boolean declare_cepv
)
#else
(fid, if_name, if_version, ep, declare_cepv)
    FILE *fid;
    NAMETABLE_id_t if_name;
    unsigned long int if_version;
    AST_export_n_t *ep;
    boolean declare_cepv;
#endif
{

    /*  emit the declaration of the client/manager EPV type
        and, conditional on declare_cepv, an extern declaration
        for the client EPV
    */

    NAMETABLE_id_t    var_name;
    AST_operation_n_t *op;

    fprintf (fid, "typedef struct ");
    spell_name (fid, if_name);
    fprintf (fid, "_v%d_%d_epv_t {\n", (if_version%65536), (if_version/65536));
    for (; ep; ep = ep->next)
        if (ep->kind == AST_operation_k) {
            op = ep->thing_p.exported_operation;
            CSPELL_epv_field (fid, op);
        }
    fprintf (fid, "} ");
    spell_name (fid, if_name);
    fprintf (fid, "_v%d_%d_epv_t;\n", (if_version%65536), (if_version/65536));

    if (declare_cepv) {
        fprintf(fid, "extern ");
        spell_name(fid, if_name);
        fprintf(fid, "_v%d_%d_epv_t ", if_version%65536, if_version/65536);
        spell_name(fid, if_name);
        fprintf(fid, "_v%d_%d_c_epv;\n", if_version%65536, if_version/65536);
    }

}

static void CSPELL_if_spec_refs
#ifdef PROTO
(
    FILE *fid,
    NAMETABLE_id_t if_name,
    unsigned long int if_version
)
#else
(fid, if_name, if_version)
    FILE *fid;
    NAMETABLE_id_t if_name;
    unsigned long int if_version;
#endif
{
    fprintf (fid, "extern rpc_if_handle_t ");
    spell_name (fid, if_name);
    fprintf (fid, "_v%d_%d_c_ifspec;\n",(if_version%65536),(if_version/65536));

    fprintf (fid, "extern rpc_if_handle_t ");
    spell_name (fid, if_name);
    fprintf (fid, "_v%d_%d_s_ifspec;\n",(if_version%65536),(if_version/65536));
}

static void CSPELL_user_prototypes
#ifdef PROTO
(
    FILE *fid,
    AST_interface_n_t *ifp
)
#else
(fid, ifp)
    FILE *fid;
    AST_interface_n_t *ifp;
#endif
{
    AST_type_p_n_t *tpp;
    AST_type_n_t *tp;

    /*
     * declare context handle rundown routines
     */
    for (tpp = ifp->ch_types; tpp; tpp = tpp->next)
    {
        fprintf(fid, "void ");
        spell_name(fid, tpp->type->name);
        fprintf(fid, "_rundown(\n#ifdef IDL_PROTOTYPES\n");
        fprintf(fid, "    rpc_ss_context_t context_handle\n#endif\n);\n");
    }

    /*
     * declare the "from_xmit", "to_xmit", "free_xmit", and "free"
     * routines as extern's for types with the [transmit_as()] attribute
     */
    for (tpp = ifp->xa_types; tpp; tpp = tpp->next)
    {
        tp = tpp->type;

        fprintf (fid, "void ");
        spell_name (fid, tp->name);
        fprintf (fid, "_from_xmit(\n#ifdef IDL_PROTOTYPES\n");
        CSPELL_type_exp_simple (fid, tp->xmit_as_type);
        fprintf (fid, " *xmit_object,\n");
        spell_name (fid, tp->name);
        fprintf (fid, " *object\n#endif\n);\n");

        fprintf (fid, "void ");
        spell_name (fid, tp->name);
        fprintf (fid, "_to_xmit(\n#ifdef IDL_PROTOTYPES\n");
        spell_name (fid, tp->name);
        fprintf (fid, " *object,\n");
        CSPELL_type_exp_simple (fid, tp->xmit_as_type);
        fprintf (fid, " **xmit_object\n#endif\n);\n");

        fprintf (fid, "void ");
        spell_name (fid, tp->name);
        fprintf (fid, "_free_inst(\n#ifdef IDL_PROTOTYPES\n");
        spell_name (fid, tp->name);
        fprintf (fid, " *object\n#endif\n);\n");

        fprintf (fid, "void ");
        spell_name (fid, tp->name);
        fprintf (fid, "_free_xmit(\n#ifdef IDL_PROTOTYPES\n");
        CSPELL_type_exp_simple (fid, tp->xmit_as_type);
        fprintf (fid, " *xmit_object\n#endif\n);\n");
    }

    /*
     * declare the "from_local", "to_local", "free_local", and
     * "free" routines as extern's for types with the [represent_as()]
     * attribute
     */
    for (tpp = ifp->ra_types; tpp; tpp = tpp->next)
    {
        tp = tpp->type;

        fprintf (fid, "void ");
        spell_name (fid, tp->name);
        fprintf (fid, "_from_local(\n#ifdef IDL_PROTOTYPES\n");
        spell_name (fid, tp->rep_as_type->type_name);
        fprintf (fid, " *local_object,\n");
        spell_name (fid, tp->name);
        fprintf (fid, " **net_object\n#endif\n);\n");

        fprintf (fid, "void ");
        spell_name (fid, tp->name);
        fprintf (fid, "_to_local(\n#ifdef IDL_PROTOTYPES\n");
        spell_name (fid, tp->name);
        fprintf (fid, " *net_object,\n");
        spell_name (fid, tp->rep_as_type->type_name);
        fprintf (fid, " *local_object\n#endif\n);\n");

        fprintf (fid, "void ");
        spell_name (fid, tp->name);
        fprintf (fid, "_free_local(\n#ifdef IDL_PROTOTYPES\n");
        spell_name (fid, tp->rep_as_type->type_name);
        fprintf (fid, " *local_object\n#endif\n);\n");

        fprintf (fid, "void ");
        spell_name (fid, tp->name);
        fprintf (fid, "_free_inst(\n#ifdef IDL_PROTOTYPES\n");
        spell_name (fid, tp->name);
        fprintf (fid, " *net_object\n#endif\n);\n");
    }
}

void BE_gen_c_header
#ifdef PROTO
(
    FILE *fid,              /* Handle for emitted C text */
    AST_interface_n_t *ifp, /* Ptr to AST interface node */
    boolean bugs[],         /* List of backward compatibility "bugs" */
    boolean cepv_opt        /* -cepv option present */
)
#else
( fid, ifp, bugs, cepv_opt)
FILE                *fid;
AST_interface_n_t   *ifp;
boolean             bugs[];
boolean             cepv_opt;
#endif
{
    AST_import_n_t    *impp;
    AST_include_n_t   *incp;
    char        include_var_name[max_string_len];
    char        *fn_str, *if_name;

    NAMETABLE_id_to_string(ifp->name, &if_name);
    sprintf (include_var_name, "%s_v%d_%d_included", if_name,
                               (ifp->version%65536), (ifp->version/65536));
    fprintf (fid, "#ifndef %s\n#define %s\n",
             include_var_name, include_var_name);

    if (AST_DOUBLE_USED_SET(ifp) && !AST_LOCAL_SET(ifp))
        fprintf(fid, "#ifndef IDL_DOUBLE_USED\n#define IDL_DOUBLE_USED\n#endif\n");

    if (BE_bug_boolean_def)
        fprintf(fid, "#ifndef NIDL_bug_boolean_def\n#define NIDL_bug_boolean_def\n#endif\n");

    fprintf (fid, INCLUDE_TEMPLATE, "idlbase.h");

    if (!AST_LOCAL_SET(ifp) && (ifp->op_count > 0))
        fprintf (fid, INCLUDE_TEMPLATE, "rpc.h");

    for (incp = ifp->includes; incp; incp = incp->next) {
        STRTAB_str_to_string (incp->file_name, &fn_str);
        fprintf (fid, USER_INCLUDE_TEMPLATE, fn_str);
    }

    fprintf (fid, "\n#ifdef __cplusplus\n    extern \"C\" {\n#endif\n\n");

    for (impp = ifp->imports; impp; impp=impp->next) {
        STRTAB_str_to_string (impp->file_name, &fn_str);
        FILE_form_filespec((char *)NULL, (char *)NULL, ".h", fn_str,
                            include_var_name);
        fprintf (fid, USER_INCLUDE_TEMPLATE, include_var_name);
    }

    CSPELL_exports (fid, ifp->exports);
    BE_gen_pipe_routine_decls (fid, ifp);
    CSPELL_user_prototypes (fid, ifp);

    /* emit declarations of implicit handle variable and epv's */
    if (ifp->implicit_handle_name != NAMETABLE_NIL_ID) {
        fprintf (fid, "globalref ");
        if ( ! AST_IMPLICIT_HANDLE_G_SET(ifp) )
        {
            fprintf(fid, "handle_t");
        }
        else
        {
            spell_name (fid, ifp->implicit_handle_type_name);
        }
        fprintf(fid, " ");
        spell_name (fid, ifp->implicit_handle_name);
        fprintf (fid, ";\n");
        }

    if (!AST_LOCAL_SET(ifp) && (ifp->op_count > 0)) {
        CSPELL_epv_type_and_var(fid, ifp->name, ifp->version, ifp->exports,
            cepv_opt);
        CSPELL_if_spec_refs (fid, ifp->name, ifp->version);
    }

    fprintf (fid, "\n#ifdef __cplusplus\n    }\n#endif\n\n");

    fprintf (fid, "#endif\n");
}
