#include <nidl.h>
#include <ast.h>
#include <cspell.h>
#include <cspeldcl.h>
#include <files.h>
#include <genpipes.h>

extern int yylineno;
extern char *strutol(), *strltou();
char *user_include_template = USER_INCLUDE_TEMPLATE;

/*
 * mapchar
 *
 * Maps a single character into a string suitable for emission
 */
static char *mapchar
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

    fprintf (fid, "\nextern ");
    CSPELL_typed_name (fid, &func_type_node, op->name, NULL, false, true);
    fprintf (fid, ";\n");
}


static void CSPELL_type_def
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
	        /* Only permit access via macros
                CSPELL_operation_def (fid, ep->thing_p.exported_operation);
	        */
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

static CSPELL_epv_macro
#ifdef PROTO
(
    FILE *fid,
    AST_operation_n_t *op,
    NAMETABLE_id_t if_name,
    unsigned long int if_version
)
#else
(fid, op, if_name, if_version)
    FILE   *fid;
    AST_operation_n_t *op;
    NAMETABLE_id_t if_name;
    unsigned long int if_version;
#endif
{
    AST_parameter_n_t *pp;
    int first, i, nargs;
    char *ifname, *ifuname, *opname, *opuname;

    NAMETABLE_id_to_string(if_name, &ifname);
    ifuname = strltou(ifname);
    NAMETABLE_id_to_string(op->name, &opname);
    opuname = strltou(opname);

    for (pp = op->parameters, nargs = 0; pp; pp = pp->next, nargs++)
	;
    fprintf(fid, "\n#define KSOP_%s_%s\t%d\n", ifuname, opuname, op->op_number);
    if (op->flags & AST_BB) {
	    fprintf(fid, "#define KSBB_%s_%s\t%d\n", ifuname, opuname,
						op->op_bb);
	    /*
	     * XXX this is really a private definition ...
	    fprintf(fid, "#define SET_KSBB_%s_%s(bbp,v,t)\\\n", ifuname, opuname);
	    fprintf(fid, "\t*(((t) *)&((bbp)[KSBB_%s_%s])) = v\n",
					ifuname, opuname);
	     */
    }

    fprintf(fid, "\n#define KS_%s_%s(h", ifname, opname);
    /* the 1st arg is replaced by handle */
    for (i = 1; i < nargs; i++)
	fprintf(fid, ", a%d", i);
    fprintf(fid, ") \\\n");

    if (op->flags & AST_BB) {
	    AST_parameter_n_t *pp2nd;

	    pp2nd = op->parameters->next;
	    fprintf(fid, "\t(h)->ks_usebb ? \\\n");
	    fprintf(fid, "\t*(a1) = *(");
	    CSPELL_cast_exp(fid, pp2nd->type);
	    fprintf(fid, "&((h)->ks_bb[KSBB_%s_%s])) : \\\n",
				ifuname, opuname);
    } else {
	    fprintf(fid, "\t(h)->ks_useops ? \\\n");
	    fprintf(fid, "\t((%s", ifname);
	    fprintf(fid, "_v%d_%d_epv_t", (if_version%65536), (if_version/65536));
	    fprintf(fid, " *)((h)->ks_ops))->%s( \\\n", opname);
	    fprintf(fid, "\t\t(h)->ks_handle");
	    /* the 1st arg is replaced by handle */
	    for (i = 1; i < nargs; i++)
		fprintf(fid, ", a%d", i);
	    fprintf(fid, ") : \\\n");
    }

    fprintf(fid, "\tks_send(h, KSOP_%s_%s", ifuname, opuname);
    /*
     * output formatting array - "sz_arg1;sz_arg2;..;"
     */
    fprintf(fid, ", \"");
    for (pp = op->parameters, first = 1; pp; pp = pp->next) {
	if (first)
		first = 0;
	else
		fprintf(fid, "%d;", pp->type->ndr_size);
    }
    fprintf(fid, "\"");
    /* the 1st arg is replaced by handle */
    for (i = 1; i < nargs; i++)
	fprintf(fid, ", a%d", i);
    fprintf(fid, ")\n");
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
    AST_export_n_t *sep;

    /*
     * Output macros that dereference through epv table
     */
    for (sep = ep; sep; sep = sep->next)
        if (sep->kind == AST_operation_k) {
            op = sep->thing_p.exported_operation;
            CSPELL_epv_macro (fid, op, if_name, if_version);
        }

    fprintf (fid, "\n\ntypedef struct {\n");
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

void ksgen_header
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
    char        *fn_str, *if_name, *if_uname;

    NAMETABLE_id_to_string(ifp->name, &if_name);
    if_uname = strltou(if_name);
    sprintf (include_var_name, "__%s_v%d_%d_included", if_name,
                               (ifp->version%65536), (ifp->version/65536));
    fprintf (fid, "#ifndef %s\n#define %s\n\n",
             include_var_name, include_var_name);

    if (AST_DOUBLE_USED_SET(ifp) && !AST_LOCAL_SET(ifp))
        fprintf(fid, "#ifndef IDL_DOUBLE_USED\n#define IDL_DOUBLE_USED\n#endif\n");

    if (BE_bug_boolean_def)
        fprintf(fid, "#ifndef NIDL_bug_boolean_def\n#define NIDL_bug_boolean_def\n#endif\n");

    fprintf (fid, user_include_template, "ks/ks_types.h");

#ifdef NEVER
    if (!AST_LOCAL_SET(ifp) && (ifp->op_count > 0))
        fprintf (fid, INCLUDE_TEMPLATE, "rpc.h");
#endif

    for (incp = ifp->includes; incp; incp = incp->next) {
        STRTAB_str_to_string (incp->file_name, &fn_str);
        fprintf (fid, user_include_template, fn_str);
    }

    fprintf (fid, "\n#ifdef __cplusplus\n    extern \"C\" {\n#endif\n\n");

    for (impp = ifp->imports; impp; impp=impp->next) {
        STRTAB_str_to_string (impp->file_name, &fn_str);
        FILE_form_filespec((char *)NULL, (char *)NULL, ".h", fn_str,
                            include_var_name);
        fprintf (fid, user_include_template, include_var_name);
    }

    /*
     * output CPP form of version #
     */
    fprintf(fid, "#define KS_%s_VERSION 0x%x\n", if_uname, ifp->version);
    CSPELL_exports (fid, ifp->exports);
    /* XX ksgen should never have these */
    BE_gen_pipe_routine_decls (fid, ifp);
    /* XX ksgen should never have these */
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

    if ((ifp->op_count > 0)) {
        CSPELL_epv_type_and_var(fid, ifp->name, ifp->version, ifp->exports,
            cepv_opt);
    }

    fprintf (fid, "\n#ifdef __cplusplus\n    }\n#endif\n\n");

    fprintf (fid, "#endif\n");
}

char *
strltou(char *str)
{
	char *ds, *dsp;

	dsp = ds = malloc(strlen(str) + 1);
	while (*str) {
		*dsp++ = toupper(*str++);
	}
	*dsp = '\0';
	return ds;
}

char *
strutol(char *str)
{
	char *ds, *dsp;

	dsp = ds = malloc(strlen(str) + 1);
	while (*str) {
		*dsp++ = tolower(*str++);
	}
	*dsp = '\0';
	return ds;
}
