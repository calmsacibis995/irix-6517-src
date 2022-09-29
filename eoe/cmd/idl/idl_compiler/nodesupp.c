/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: nodesupp.c,v $
 * Revision 1.1  1993/08/31 06:48:01  jwag
 * Initial revision
 *
 * Revision 1.1.2.3  1993/01/03  21:41:01  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  14:37:29  bbelch]
 *
 * Revision 1.1.2.2  1992/12/23  18:50:12  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  01:04:48  zeliff]
 * 
 * Revision 1.1  1992/01/19  03:01:42  devrcs
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
**      nodesupp.c
**
**  FACILITY:
**
**      Interface Definition Language (IDL) Compiler
**
**  ABSTRACT:
**
**      Routines supporting pointed_at, non-self_pointing nodes
**
**  VERSION: DCE 1.0
**
*/

#include <nidl.h>
#include <ast.h>
#include <checker.h>    /* for type_is macros */
#include <bedeck.h>
#include <dutils.h>
#include <cspell.h>
#include <hdgen.h>
#include <nodmarsh.h>
#include <nodunmar.h>
#include <nodesupp.h>
#include <backend.h>

/******************************************************************************/
/*                                                                            */
/*    Spell the name of a routine for un/marshalling a pointed_at node        */
/*                                                                            */
/******************************************************************************/
void CSPELL_pa_routine_name
#ifdef PROTO
(
    FILE *fid,
    AST_type_n_t *p_type,
    BE_call_side_t call_side,
    BE_marshalling_k_t to_or_from_wire,
    boolean varying
)
#else
(fid, p_type, call_side, to_or_from_wire, varying)
    FILE *fid;
    AST_type_n_t *p_type;
    BE_call_side_t call_side;
    BE_marshalling_k_t to_or_from_wire;
    boolean varying;
#endif
{
    if (p_type->name != NAMETABLE_NIL_ID
        && !(type_is_scalar(p_type) && p_type->xmit_as_type == NULL))
    {
        spell_name( fid, p_type->name );
        fprintf( fid, "_%c%c",
                      (to_or_from_wire == BE_marshalling_k) ? 'm' : 'u',
                      (call_side == BE_caller) ? 'c' : 's' );
    }
    else
    {
        switch (p_type->kind)
        {
            case AST_boolean_k:
            case AST_byte_k:
            case AST_character_k:
            case AST_enum_k:
            case AST_small_integer_k:
            case AST_short_integer_k:
            case AST_long_integer_k:
            case AST_hyper_integer_k:
            case AST_small_unsigned_k:
            case AST_short_unsigned_k:
            case AST_long_unsigned_k:
            case AST_hyper_unsigned_k:
            case AST_short_float_k:
            case AST_long_float_k:
                fprintf( fid, "rpc_ss_%c%c_",
                      (to_or_from_wire == BE_marshalling_k) ? 'm' : 'u',
                      (call_side == BE_caller) ? 'r' : 'e' );
                break;
            default:
                break;
        }
        if (!(CSPELL_scalar_type_suffix(fid, p_type)))
        {
            if (p_type->be_info.type == NULL)
            {
#ifdef DUMPERS
                printf ("Anonymous type without generated name in CSPELL_pa_routine_name\n");
#endif
                error(NIDL_INTERNAL_ERROR, __FILE__, __LINE__);
            }
            spell_name(fid, p_type->be_info.type->clone->name);
            fprintf( fid, "_%c%c",
                  (to_or_from_wire == BE_marshalling_k) ? 'm' : 'u',
                  (call_side == BE_caller) ? 'c' : 's' );
        }
    }
}

/******************************************************************************/
/*                                                                            */
/*    Spell the routines for un/marshalling a pointed_at node                 */
/*                                                                            */
/******************************************************************************/
void BE_pointed_at_routines
#ifdef PROTO
(
    FILE *fid,
    AST_interface_n_t *p_interface,
    BE_call_side_t call_side
)
#else
( fid, p_interface, call_side )
    FILE *fid;
    AST_interface_n_t *p_interface;
    BE_call_side_t call_side;
#endif
{
    AST_type_p_n_t *curr_pa_type_link;  /* Position in list of node types */


    /*
     * Generate the mini prototype (static void name();) for pointed-at routines
     */
    for( curr_pa_type_link = p_interface->pa_types;
         curr_pa_type_link != NULL;
         curr_pa_type_link = curr_pa_type_link->next )
    {
        /*
         *  For each non-self pointing type generate the pointed-at static void
         *  prototype for it.
         */
        if ( ! AST_SELF_POINTER_SET(curr_pa_type_link->type)  ||
            (curr_pa_type_link->type->name == NAMETABLE_NIL_ID))
        {
            if ( ( (call_side == BE_caller)
                    && AST_IN_SET(curr_pa_type_link->type)
                    && AST_IN_PA_STUB_SET(curr_pa_type_link->type) )
                || ( (call_side == BE_callee)
                    && AST_OUT_SET(curr_pa_type_link->type)
                    && AST_OUT_PA_STUB_SET(curr_pa_type_link->type) ) )
            {
               fprintf(fid,"static void ");
               CSPELL_pa_routine_name( fid, curr_pa_type_link->type, call_side,
                    BE_marshalling_k, false );
               fprintf(fid,"();\n");
            }
            if ( ( (call_side == BE_caller)
                    && AST_OUT_SET(curr_pa_type_link->type)
                    && AST_OUT_PA_STUB_SET(curr_pa_type_link->type) )
                || ( (call_side == BE_callee)
                    && ( ( AST_IN_SET(curr_pa_type_link->type) &&
                           AST_IN_PA_STUB_SET(curr_pa_type_link->type) )
                       || AST_OUT_PA_REF_SET(curr_pa_type_link->type) ) ) )
            {
               fprintf(fid,"static void ");
               CSPELL_pa_routine_name( fid, curr_pa_type_link->type, call_side,
                    BE_unmarshalling_k, false );
               fprintf(fid,"();\n");
            }
        }
    }


    /*
     * Generate the pointed-at routines necessary
     */
    for( curr_pa_type_link = p_interface->pa_types;
         curr_pa_type_link != NULL;
         curr_pa_type_link = curr_pa_type_link->next )
    {
        /*
         * For each non-self pointing generate the pointed-at routine
         * for it.  Likewise if it is an anonymous then we have to generate
         * a pointed-at routine for it because it will be static in the aux
         * file, so we have to emit a copy here as well.
         */
        if ( ! AST_SELF_POINTER_SET(curr_pa_type_link->type)  ||
            (curr_pa_type_link->type->name == NAMETABLE_NIL_ID))
        {
            BE_push_malloc_ctx();
            NAMETABLE_set_temp_name_mode();
            if (curr_pa_type_link->type->name == NAMETABLE_NIL_ID)
            {
                CSPELL_type_def( fid,
                    curr_pa_type_link->type->be_info.type->clone, true );
                if ( ( (call_side == BE_caller)
                        && AST_IN_SET(curr_pa_type_link->type)
                        && AST_IN_PA_STUB_SET(curr_pa_type_link->type) )
                    || ( (call_side == BE_callee)
                        && AST_OUT_SET(curr_pa_type_link->type)
                        && AST_OUT_PA_STUB_SET(curr_pa_type_link->type) ) )
                {
                    CSPELL_marshall_node( fid,
                        curr_pa_type_link->type->be_info.type->clone,
                        call_side, true,
                        AST_SELF_POINTER_SET(curr_pa_type_link->type));
                }
                if ( ( (call_side == BE_caller)
                        && AST_OUT_SET(curr_pa_type_link->type)
                        && AST_OUT_PA_STUB_SET(curr_pa_type_link->type) )
                    || ( (call_side == BE_callee)
                        && ( ( AST_IN_SET(curr_pa_type_link->type) &&
                               AST_IN_PA_STUB_SET(curr_pa_type_link->type) )
                           || AST_OUT_PA_REF_SET(curr_pa_type_link->type) ) ) )
                {
                    CSPELL_unmarshall_node( fid,
                        curr_pa_type_link->type->be_info.type->clone,
                        call_side, true,
                        AST_SELF_POINTER_SET(curr_pa_type_link->type));
                }
            }
            else
            {
                if ( ( (call_side == BE_caller)
                        && AST_IN_SET(curr_pa_type_link->type)
                        && AST_IN_PA_STUB_SET(curr_pa_type_link->type) )
                    || ( (call_side == BE_callee)
                        && AST_OUT_SET(curr_pa_type_link->type)
                        && AST_OUT_PA_STUB_SET(curr_pa_type_link->type) ) )
                {
                   CSPELL_marshall_node( fid, curr_pa_type_link->type, call_side,
                                          true, false );
                }
                if ( ( (call_side == BE_caller)
                        && AST_OUT_SET(curr_pa_type_link->type)
                        && AST_OUT_PA_STUB_SET(curr_pa_type_link->type) )
                    || ( (call_side == BE_callee)
                        && ( ( AST_IN_SET(curr_pa_type_link->type) &&
                               AST_IN_PA_STUB_SET(curr_pa_type_link->type) )
                           || AST_OUT_PA_REF_SET(curr_pa_type_link->type) ) ) )
                {
                    CSPELL_unmarshall_node( fid, curr_pa_type_link->type,
                                            call_side, true, false );
                }
            }
            NAMETABLE_clear_temp_name_mode();
            BE_pop_malloc_ctx();
        }
    }
}

/******************************************************************************/
/*                                                                            */
/*  Loop over the types on the pa & sp lists and generate names for any       */
/*  that are anonymous.                                                       */
/*                                                                            */
/******************************************************************************/
void BE_clone_anonymous_pa_types
#ifdef PROTO
(
    AST_interface_n_t *ifp
)
#else
(ifp)
    AST_interface_n_t *ifp;
#endif
{
    AST_type_p_n_t *pa;
    AST_type_n_t *clone;


    /*
     * The pa list may contain anonymous types so give them names such
     * that we can generate routines for them.  The names are not put into
     * the name field in the type because this would change the header file
     * generation.  For anonymous types, the generated name is stored in
     * a copy of the type with the name field filled in.
     */
    for (pa = ifp->pa_types; pa; pa = pa->next)
    {
        if (pa->type->name == NAMETABLE_NIL_ID)
        {
            /*
             * Copy the type and add a generated name
             */
            clone = (AST_type_n_t *)MALLOC(sizeof(AST_type_n_t));
            *clone = *pa->type;
            clone->name = BE_new_local_var_name("anon");

            pa->type->be_info.type = (BE_type_i_t *)MALLOC(sizeof(BE_type_i_t));
            pa->type->be_info.type->clone = clone;
        }
    }


    /*
     * Now do the same for the sp types list.  The only reason that there
     * should be an anonymous type on this list is if it is on this list
     * only because it is pointed-at by a self-pointing type.  These routines
     * should be static to the aux file so as to not conflict with the name
     * space of any other aux file.
     */
    for (pa = ifp->sp_types; pa; pa = pa->next)
    {
        if ((pa->type->name == NAMETABLE_NIL_ID) &&
            (pa->type->be_info.type == NULL))
        {
            /*
             * Copy the type and add a generated name
             */
            clone = (AST_type_n_t *)MALLOC(sizeof(AST_type_n_t));
            *clone = *pa->type;
            clone->name = BE_new_local_var_name("anon");

            pa->type->be_info.type = (BE_type_i_t *)MALLOC(sizeof(BE_type_i_t));
            pa->type->be_info.type->clone = clone;
        }
    }
}
