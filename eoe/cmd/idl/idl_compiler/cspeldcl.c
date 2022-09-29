/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: cspeldcl.c,v $
 * Revision 1.1  1993/08/31 06:48:01  jwag
 * Initial revision
 *
 * Revision 1.1.2.3  1993/01/03  21:38:41  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  14:33:41  bbelch]
 *
 * Revision 1.1.2.2  1992/12/23  18:44:54  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  01:00:53  zeliff]
 * 
 * Revision 1.1  1992/01/19  03:00:53  devrcs
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
**  NAME:
**
**      cspeldcl.c
**
**  FACILITY:
**
**      IDL Compiler Backend
**
**  ABSTRACT:
**
**  Routines to spell declaration related material to C source
**
**  VERSION: DCE 1.0
*/

#include <nidl.h>
#include <ast.h>
#include <bedeck.h>
#include <cspell.h>
#include <cspeldcl.h>
#include <hdgen.h>

/******************************************************************************/
/*                                                                            */
/*    Build text string for constant value                                    */
/*                                                                            */
/******************************************************************************/
void CSPELL_constant_val_to_string
#ifdef PROTO
(
    AST_constant_n_t *cp,
    char *str
)
#else
(cp, str)
    AST_constant_n_t *cp;
    char *str;
#endif
{
    char       *str2;

    switch (cp->kind) {
        case AST_nil_const_k:
            sprintf (str, "NULL");
            break;
        case AST_boolean_const_k:
            if (cp->value.boolean_val)
                sprintf (str, "ndr_true");
            else
                sprintf (str, "ndr_false");
            break;
        case AST_int_const_k:
            sprintf (str, "%ld", cp->value.int_val);
            break;
        case AST_string_const_k:
            STRTAB_str_to_string (cp->value.string_val, &str2);
            sprintf (str, "\"%s\"", str2);
            break;
        case AST_char_const_k:
            sprintf (str, "'%s'", mapchar(cp, FALSE));
            break;
        default:
            INTERNAL_ERROR("Unsupported tag in CSPELL_constant_val_to_string");
            break;
        }
}

/******************************************************************************/
/*                                                                            */
/*    Routine to spell constant to C source                                   */
/*                                                                            */
/******************************************************************************/
void CSPELL_constant_val
#ifdef PROTO
(
    FILE *fid,
    AST_constant_n_t *cp
)
#else
(fid, cp)
    FILE *fid;
    AST_constant_n_t *cp;
#endif
{
    char str[max_string_len];

    CSPELL_constant_val_to_string (cp, str);
    fprintf (fid, "%s", str);
}

/******************************************************************************/
/*                                                                            */
/*    Routine to spell union case label comment to C source                   */
/*                                                                            */
/******************************************************************************/
void CSPELL_labels
#ifdef PROTO
(
    FILE *fid,
    AST_case_label_n_t *clp
)
#else
(fid, clp)
    FILE  *fid;
    AST_case_label_n_t *clp;
#endif
{
    boolean first = true;

    fprintf (fid, "/* case(s): ");
    for (; clp; clp = clp->next) {
        if (first)
            first = false;
        else
            fprintf (fid, ", ");
        if (clp->default_label)
            fprintf (fid, "default");
        else
            CSPELL_constant_val (fid, clp->value);
        };
    fprintf (fid, " */\n");
}

/******************************************************************************/
/*                                                                            */
/*    Routine to spell function parameter list to C source                    */
/*                                                                            */
/******************************************************************************/
void CSPELL_parameter_list
#ifdef PROTO
(
    FILE *fid,
    AST_parameter_n_t *pp
)
#else
(fid, pp)
    FILE *fid;
    AST_parameter_n_t *pp;
#endif
{
    boolean            first = true;

    if (pp)
        for (; pp; pp = pp->next) {
            if (first)
                first = false;
            else
                fprintf (fid, ",\n");
            fprintf (fid, "    /* [");
            if (AST_IN_SET(pp))
            {
                fprintf(fid, "in");
                if (AST_OUT_SET(pp)) fprintf (fid, ", out");
            }
            else fprintf  (fid, "out");
            fprintf (fid, "] */ ");
            if (pp->be_info.param)
                CSPELL_typed_name (fid, pp->type,
                    pp->be_info.param->name, NULL, false, true);
            else
                 CSPELL_typed_name (fid, pp->type, pp->name, NULL, false, true);
        }
    else
        fprintf (fid, "    void");
}


/******************************************************************************/
/*                                                                            */
/*    Spell new and old style parameter lists                                 */
/*                                                                            */
/******************************************************************************/
void CSPELL_finish_synopsis
#ifdef PROTO
(
    FILE *fid,
    AST_parameter_n_t *paramlist
)
#else
(fid, paramlist)
    FILE *fid;
    AST_parameter_n_t *paramlist;
#endif
{
    AST_parameter_n_t *pp;

    fprintf (fid, "\n#ifndef IDL_PROTOTYPES\n");
    for (pp = paramlist; pp != NULL; pp = pp->next)
        if (pp->be_info.param)
            CSPELL_var_decl(fid, pp->type,
                pp->be_info.param->name);
        else CSPELL_var_decl (fid, pp->type, pp->name);
    fprintf (fid, "#endif\n");
}
