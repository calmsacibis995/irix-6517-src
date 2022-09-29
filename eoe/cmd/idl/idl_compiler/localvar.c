/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: localvar.c,v $
 * Revision 1.1  1993/08/31 06:48:01  jwag
 * Initial revision
 *
 * Revision 1.1.2.3  1993/01/03  21:40:04  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  14:35:59  bbelch]
 *
 * Revision 1.1.2.2  1992/12/23  18:48:09  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  01:03:14  zeliff]
 * 
 * Revision 1.1  1992/01/19  03:01:23  devrcs
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
**      localvar.c
**
**  FACILITY:
**
**      Interface Definition Language (IDL) Compiler
**
**  ABSTRACT:
**
**      Routines for managing stub local variables
**
**  VERSION: DCE 1.0
**
*/

#include <nidl.h>
#include <ast.h>
#include <bedeck.h>
#include <cspell.h>
#include <localvar.h>
#include <backend.h>

/******************************************************************************/
/*                                                                            */
/*    BE_DECLARE_LOCAL_VAR                                                    */
/*                                                                            */
/******************************************************************************/
void BE_declare_local_var
#ifdef PROTO
(
    NAMETABLE_id_t name,
    AST_type_n_t *type,
    AST_operation_n_t *rp,
    char *comment,
    boolean volatility
)
#else
(name, type, rp, comment, volatility)
    NAMETABLE_id_t     name;
    AST_type_n_t      *type;
    AST_operation_n_t *rp;
    char              *comment;
    boolean            volatility;
#endif
{
    BE_local_var_t *vp;

    vp = (BE_local_var_t *)BE_ctx_malloc(sizeof(BE_local_var_t));
    vp->name = name;
    vp->type = type;
    vp->next = rp->be_info.oper->local_vars;
    vp->comment = comment;
    vp->volatility = volatility;
    rp->be_info.oper->local_vars = vp;

}

#if 0
/******************************************************************************/
/*                                                                            */
/*    BE_DECLARE_SHORT_LOCAL_VAR                                              */
/*                                                                            */
/******************************************************************************/
NAMETABLE_id_t BE_declare_short_local_var
#ifdef PROTO
(
    NAMETABLE_id_t name,
    AST_type_n_t *type,
    AST_operation_n_t *rp,
    boolean volatility
)
#else
(name, type, rp, volatility)
    NAMETABLE_id_t name;
    AST_type_n_t      *type;
    AST_operation_n_t *rp;
    boolean            volatility;
#endif
{
    NAMETABLE_id_t short_name;
    char           str [sizeof ("NIDL_ffffffff")];
    char          *resolved_name;

    sprintf (str, "NIDL_%x", name);
    short_name = NAMETABLE_add_id (str);
    NAMETABLE_id_to_string ( name, &resolved_name );
    BE_declare_local_var (short_name, type, rp, resolved_name, volatility);
    return short_name;
}
#endif

/******************************************************************************/
/*                                                                            */
/*    CSPELL_LOCAL_VAR_DECLS                                                  */
/*                                                                            */
/******************************************************************************/
void CSPELL_local_var_decls
#ifdef PROTO
(
    FILE *fid,
    AST_operation_n_t *rp
)
#else
(fid, rp)
    FILE      *fid;
    AST_operation_n_t *rp;
#endif
{
    BE_local_var_t        *vp;
    boolean            first = true;

    for (vp = rp->be_info.oper->local_vars; vp; vp = vp->next) {

        if (first) {
            first = false;
            fprintf (fid, "\n/* local variables */\n");
            }
#if 0
        if (vp->volatility)
            fprintf (fid, "Volatile ");
#endif
        CSPELL_typed_name (fid, vp->type, vp->name, NULL, false, true);
#if 0
        if (vp->comment) {
            fprintf (fid, " /* %s */ ", vp->comment);
            }
#endif
        fprintf (fid, ";\n");
        }
}

