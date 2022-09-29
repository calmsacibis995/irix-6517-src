/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: marshall.c,v $
 * Revision 1.1  1993/08/31 06:48:01  jwag
 * Initial revision
 *
 * Revision 1.1.4.3  1993/01/03  21:40:11  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  14:36:10  bbelch]
 *
 * Revision 1.1.4.2  1992/12/23  18:48:27  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  01:03:25  zeliff]
 * 
 * Revision 1.1.2.5  1992/06/11  18:46:42  harrow
 * 	Correct a missing copy from a local variable to the marshalling
 * 	state block to correct stub code generated for [in] pipes following
 * 	and [in] pointer-based data structure.
 * 	[1992/06/11  18:45:07  harrow]
 * 
 * Revision 1.1.2.4  1992/05/13  19:09:24  harrow
 * 	     Emit TYPE_free() calls on the server for any [out] or [in,out] transmit_as
 * 	     types and free_local_rep() calls for any [out] or [in,out] represent_as types.
 * 	     [1992/05/13  11:34:49  harrow]
 * 
 * Revision 1.1.2.3  1992/05/05  15:45:09  harrow
 * 	          Stop checking for drep matches at an [out_of_line] union arm
 * 	          as its drep will be checked in the ool routine generated for
 * 	          it.  This terminates the recursion and prevents an internal
 * 	          error message from being displayed.
 * 	          [1992/05/05  14:00:05  harrow]
 * 
 * Revision 1.1.2.2  1992/03/24  14:10:48  harrow
 * 	               On the callee, register context handles immediately
 * 	               after the manager call so that marshalling failures do
 * 	               not prevent rundown on contexts not yet marshalled.
 * 	               [1992/03/19  15:31:37  harrow]
 * 
 * Revision 1.1  1992/01/19  03:01:29  devrcs
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
**      marshall.c
**
**  FACILITY:
**
**      IDL Compiler Backend
**
**  ABSTRACT:
**
**  The interface to the IDL marshalling and unmarshalling routines
**
**  VERSION: DCE 1.0
**
*/

#include <nidl.h>
#include <ast.h>
#include <bedeck.h>
#include <blocks.h>
#include <decorate.h>
#include <mmarsh.h>
#include <munmarsh.h>
#include <marshall.h>
#include <dutils.h>
#include <mutils.h>

/*
 * The default names to use while unmarshalling
 */
BE_mn_t BE_def_mn = {"mp", "op", "drep", "elt", "st", "&mem_handle",
                         "call_h", "goto closedown;"};

/*
 * BE_declare_stack_vars
 *
 * Emits the declarations of the iovector and stack packet
 */
void BE_declare_stack_vars
#ifdef PROTO
(
    FILE *fid,
    AST_operation_n_t *oper,
    boolean *p_uses_packet
)
#else
(fid, oper, p_uses_packet)
    FILE *fid;
    AST_operation_n_t *oper;
    boolean *p_uses_packet;
#endif
{
    if (oper->be_info.oper->vec_size > 0)
        fprintf(fid, "IoVec_t(%d) iovec;\n",
            oper->be_info.oper->vec_size);
    if ( *p_uses_packet = (oper->be_info.oper->pkt_size > 0) )
    {
        fprintf(fid, "ndr_byte packet[%d];\n",
            oper->be_info.oper->pkt_size);
        fprintf(fid, "rpc_mp_t pmp = (rpc_mp_t)&packet[0];\n");
    }
}

/*
 * BE_marshall
 */
void BE_marshall
#ifdef PROTO
(
    FILE *fid,
    AST_operation_n_t *oper,
    BE_direction_t direction
)
#else
(fid, oper, direction)
    FILE *fid;
    AST_operation_n_t *oper;
    BE_direction_t direction;
#endif
{
    BE_param_blk_t *block;
    AST_parameter_n_t *param;
    BE_mflags_t flags, bflags;
    boolean spell_length;
    boolean all_ool, all_ool_recalc;
    int slot_num;
    int dummy_slots_used;
    boolean routine_mode = false;
#ifdef DEBUG_VERBOSE
    int block_count;
#endif

    /*
     *  Before doing any marshalling on the callee side, loop over the
     *  parameters and process any context handles that may need to be
     *  registered.  This call will cause the the IDL_context_XXX variable for
     *  the associated context handle to be initialized, which will be
     *  marshalled later using the existing code paths.  For the caller,
     *  context handles are processed by the normal calls-before marshalling
     *  mechanism.
     */
    for (block = BE_Sends(oper); block; block = block->next)
    {
        for (param = block->params; param; param = param->next)
        {
            if (BE_Calls_Before(param) && (direction == BE_out))
                BE_spell_ctx_to_wires(fid, param, BE_callee);
        }
    }

    /*
     * Init the offset pointer here unless there were just out pipes,
     * in which case it has already been initialized
     */
    if (direction != BE_out || !AST_HAS_OUT_PIPES_SET(oper))
        BE_spell_start_marshall(fid,
            oper->be_info.oper->flags & BE_MAINTAIN_OP ? BE_M_MAINTAIN_OP : 0);

#ifdef DEBUG_VERBOSE
    if (BE_dump_sends)
    {
        printf("\nsends dump (unpruned) : BE_marshall\n");
        printf(  "-----------------------------------\n");
        traverse_blocks(BE_Sends(oper));
    }
#endif

    BE_Sends(oper) = BE_prune_blocks(BE_Sends(oper));

#ifdef DEBUG_VERBOSE
    if (BE_dump_sends)
    {
        printf("\nsends dump (pruned) : BE_marshall\n");
        printf(  "---------------------------------\n");
        traverse_blocks(BE_Sends(oper));
    }
#endif

    for (block = BE_Sends(oper); block; block = block->next)
    {
        if (block->flags & BE_SP_BLOCK)
            BE_m_start_buff(fid, 0);

        bflags = (block->flags & BE_SP_BLOCK) ? BE_M_SP_BLOCK : 0;
        if (block->flags & BE_FIRST_BLOCK) bflags |= BE_M_FIRST_BLOCK;

        slot_num = -1;
        spell_length = false;
        all_ool = true;

        for (param = block->params; param; param = param->next)
        {
            /* As the block has been pruned there are no BE_SKIP parameters
                to be ignored */

            /*
             * if block has a param such that neither the param or it's
             * element type (if an array) is OOL, do in-line init for
             * this block
             */
            if (!all_ool)
                all_ool_recalc = false;
            else if (BE_PI_Flags(param) & BE_OOL)
                all_ool_recalc = true;
            else
                all_ool_recalc = false;

            if (all_ool && !all_ool_recalc)
            {
                BE_spell_start_block(fid, bflags, block->vec_size);
                all_ool = false;
            }

            flags = bflags;
            if (BE_PI_Flags(param) & BE_NEW_SLOT)
            {
                /*
                 * Fill in the length field for the previous slot if the
                 * previous parameter was not an array (since arrays fill in
                 * their own lengths) and this is not the first parameter in
                 * a block.
                 */
                if (spell_length)
                    BE_spell_slot_length(fid, slot_num);
                flags |= BE_M_NEW_SLOT;
                slot_num++;
            }

            if ( (BE_PI_Flags(param) & BE_HDRLESS)
                  && (BE_PI_Flags(param) & BE_ALIGN_MP) )
            {
                /* Extra slot needed for array alignment */
                slot_num++;
            }

            if (oper->be_info.oper->flags & BE_MAINTAIN_OP)
                flags |= BE_M_MAINTAIN_OP;
            flags |= (direction == BE_in) ? BE_M_CALLER : BE_M_CALLEE;
            if (block->flags & BE_SP_BLOCK)
                flags |= BE_M_BUFF_EXISTS;

            BE_marshall_param(fid, param, flags, slot_num, &dummy_slots_used,
                              &routine_mode, block->vec_size);

            if (param->type->kind == AST_disc_union_k)
                slot_num += BE_Du_Info(param)->vec_size - 1;

            if (BE_PI_Flags(param) & BE_OOL_HEADER)
                spell_length = true;
            else if (BE_PI_Flags(param) & BE_OOL)
                spell_length = false;
            else if (param->type->kind == AST_array_k)
            {
                if (!(BE_PI_Flags(param) & BE_ARRAY_HEADER))
                    spell_length = false;
                else if (BE_Is_Arr_of_Refs(param))
                    spell_length = true;
                else spell_length =
                    !!(BE_PI_Flags(BE_Array_Info(param)->flat_elt)
                       & (BE_OOL | BE_SKIP | BE_OOL_HEADER));
            }
            else if (param->type->kind == AST_disc_union_k)
                spell_length = false;
            else if (param->type->kind == AST_pointer_k)
                spell_length = !!(BE_PI_Flags(param) & BE_DEFER);
            else
                spell_length = true;
        }
        /*
         * Fill in the length field of the final slot if necessary and
         * transmit the block
         */
        if (spell_length) BE_spell_slot_length(fid, slot_num);

        if (!block->next && direction == BE_in && !AST_HAS_IN_PIPES_SET(oper))
            bflags |= BE_M_TRANSCEIVE;

        if (block->flags & BE_SP_BLOCK)
            BE_m_end_buff(fid, 0);

        BE_spell_end_block(fid, bflags);
    }
}


/*
 * BE_within_drep_test
 * Returns TRUE if the current parameter should be unmarshalled within
 * a "do dreps match" clause
 */
static boolean BE_within_drep_test
#ifdef PROTO
(
    AST_parameter_n_t *param
)
#else
( param )
    AST_parameter_n_t *param;
#endif
{
    if ( BE_PI_Flags(param) & (BE_OOL | BE_REF_PASS) )
        return( false );
    else if ( (param->type->kind == AST_pointer_k)
                && (!(BE_PI_Flags(param) & BE_DEFER)) )
        return( false );
    else
        return( true );
}


/*
 * BE_depends_on_drep
 * Determine which dreps must match for a parameter in a flattened list
 * to be unmarshalled by the "drep matches path"
 */

static void BE_depends_on_drep
#ifdef PROTO
(
    AST_parameter_n_t *param,
    boolean *p_uses_char_drep,
    boolean *p_uses_int_drep,
    boolean *p_uses_float_drep
)
#else
( param, p_uses_char_drep, p_uses_int_drep, p_uses_float_drep )
    AST_parameter_n_t *param;
    boolean *p_uses_char_drep;
    boolean *p_uses_int_drep;
    boolean *p_uses_float_drep;
#endif
{
    BE_arm_t *arm;
    AST_parameter_n_t *pp;

    switch( param->type->kind )
    {
        case AST_null_k:        /* Alignment in flattened param */
        case AST_boolean_k:
        case AST_byte_k:
            return;
        case AST_character_k:
            *p_uses_char_drep = true;
            return;
        case AST_small_integer_k:
        case AST_short_integer_k:
        case AST_long_integer_k:
        case AST_hyper_integer_k:
        case AST_small_unsigned_k:
        case AST_short_unsigned_k:
        case AST_long_unsigned_k:
        case AST_hyper_unsigned_k:
        case AST_enum_k:
            *p_uses_int_drep = true;
            return;
        case AST_short_float_k:
        case AST_long_float_k:
            *p_uses_int_drep = true;
            *p_uses_float_drep = true;
            return;
        case AST_pointer_k:
            if ( BE_PI_Flags(param) & BE_DEFER )
            {
                if ( AST_PTR_SET(param) )
                {
                    /* Full pointers are represented as integers
                       Other pointers are uninterpreted space */
                    *p_uses_int_drep = true;
                }
            }
            else
            {
                INTERNAL_ERROR( "Pointee passed to BE_depends_on_drep" );
            }
            return;
        case AST_disc_union_k:
            if ( BE_PI_Flags(param) & BE_REF_PASS )
            {
                INTERNAL_ERROR( "Union pointees passed to BE_depends_on_drep" );
            }
            else
            {
                for (arm = BE_Du_Info(param)->arms;
                     arm != NULL;
                     arm = arm->next)
                {
                    for (pp = arm->flat; pp != NULL; pp = pp->next)
                    {
                        /* If the arm doesn't need to be tested, ignore it */
                        if (BE_within_drep_test( pp ))
                        {
                            BE_depends_on_drep( pp, p_uses_char_drep,
                                           p_uses_int_drep, p_uses_float_drep );
                        }
                    }
                }
            }
            return;
        case AST_array_k:
            /* Array within a union arm - be pessimistic */
            *p_uses_char_drep = true;
            *p_uses_int_drep = true;
            *p_uses_float_drep = true;
            return;
        default:
            INTERNAL_ERROR( "Unexpected type in BE_depends_on_drep" );
            return;
    }
}


/*
 * BE_range_of_matched_dreps
 * Returns a pointer to the last parameter that can be unmarshalled within
 * the current "do dreps match" test, also which dreps must match
 */
static void BE_range_of_matched_dreps
#ifdef PROTO
(
    AST_parameter_n_t *start_param,  /* The first parameter within the test */
    AST_parameter_n_t **p_end_param,
    boolean *p_uses_char_drep,
    boolean *p_uses_int_drep,
    boolean *p_uses_float_drep
)
#else
( start_param, p_end_param, p_uses_char_drep, p_uses_int_drep,
  p_uses_float_drep )
    AST_parameter_n_t *start_param;
    AST_parameter_n_t **p_end_param;
    boolean *p_uses_char_drep;
    boolean *p_uses_int_drep;
    boolean *p_uses_float_drep;
#endif
{
    AST_parameter_n_t *pp, *eltp;
    boolean elt_uses_char_drep;
    boolean elt_uses_int_drep;
    boolean elt_uses_float_drep;
    boolean first_non_skipped;

    *p_uses_char_drep = false;
    *p_uses_int_drep = false;
    *p_uses_float_drep = false;
    *p_end_param = start_param;
    first_non_skipped = true;

    for (pp = start_param;
         (pp != NULL) && BE_within_drep_test( pp );
         pp = pp->next)
    {
        if ( ( BE_PI_Flags(pp) & BE_SKIP )
              || AST_ADD_COMM_STATUS_SET(pp) )
        {
            *p_end_param = pp;
            continue;
        }
        if ( BE_PI_Flags(pp) & (BE_OPEN_RECORD | BE_ARRAY_HEADER) )
        {
            if ( (BE_PI_Flags(pp) & BE_OOL_HEADER)
                 && ( ! AST_VARYING_SET(pp) )
                 && ( ! AST_CONFORMANT_SET(pp->type) ) ) continue;
            *p_uses_int_drep = true;
            *p_end_param = pp;
        }
        else if ( pp->type->kind == AST_array_k )
        {
            elt_uses_char_drep = false;
            elt_uses_int_drep = false;
            elt_uses_float_drep = false;
            for (eltp = BE_Array_Info(pp)->flat_elt;
                 eltp != NULL;
                 eltp = eltp->next)
            {
                if ( ! (BE_PI_Flags(eltp) & (BE_OOL | BE_SKIP)) )
                    BE_depends_on_drep( eltp, &elt_uses_char_drep,
                                     &elt_uses_int_drep, &elt_uses_float_drep );
            }
            if ( first_non_skipped )
            {
                *p_uses_char_drep = elt_uses_char_drep;
                *p_uses_int_drep = elt_uses_int_drep;
                *p_uses_float_drep = elt_uses_float_drep;
            }
            else
            {
                if ( elt_uses_char_drep && ( ! *p_uses_char_drep ) ) return;
                if ( elt_uses_int_drep && ( ! *p_uses_int_drep ) ) return;
                if ( elt_uses_float_drep && ( ! *p_uses_float_drep ) ) return;
            }
            *p_end_param = pp;
        }
        else
        {
            BE_depends_on_drep( pp, p_uses_char_drep, p_uses_int_drep,
                                p_uses_float_drep );
            *p_end_param = pp;
        }
        first_non_skipped = false;
    }
}

/*
 * BE_spell_drep_test
 * Generate the test to use the fast path if the dreps for a set of parameters
 * match
 */
static void BE_spell_drep_test
#ifdef PROTO
(
    FILE *fid,
    boolean uses_char_drep,
    boolean uses_int_drep,
    boolean uses_float_drep
)
#else
( fid, uses_char_drep, uses_int_drep, uses_float_drep )
    FILE *fid;
    boolean uses_char_drep;
    boolean uses_int_drep;
    boolean uses_float_drep;
#endif
{
    boolean first_predicate = true;

    fprintf( fid, "if(" );
    if ( uses_char_drep )
    {
        fprintf( fid, "(%s.char_rep==ndr_g_local_drep.char_rep)",
                 BE_def_mn.drep );
        first_predicate = false;
    }
    if ( uses_int_drep )
    {
        if ( ! first_predicate )
            fprintf( fid, "\n&&" );
        fprintf( fid, "(%s.int_rep==ndr_g_local_drep.int_rep)",
                 BE_def_mn.drep );
        first_predicate = false;
    }
    if ( uses_float_drep )
    {
        if ( ! first_predicate )
            fprintf( fid, "\n&&" );
        fprintf( fid, "(%s.float_rep==ndr_g_local_drep.float_rep)",
                 BE_def_mn.drep );
    }
    fprintf( fid, ")\n" );
}

/*
 * BE_unmarshall
 */
void BE_unmarshall
#ifdef PROTO
(
    FILE *fid,
    AST_operation_n_t *oper,
    BE_direction_t direction
)
#else
(fid, oper, direction)
    FILE *fid;
    AST_operation_n_t *oper;
    BE_direction_t direction;
#endif
{
    AST_parameter_n_t *param, *start_param;
    AST_parameter_n_t *end_param;   /* Pointer to last parameter to be
                                    unmarshalled within current drep test */
    BE_mflags_t flags;
    boolean wrote_if_clause;
    boolean routine_mode = false;
    boolean start_routine_mode;
    boolean uses_char_drep;
    boolean uses_int_drep;
    boolean uses_float_drep;

    flags = (oper->be_info.oper->flags & BE_MAINTAIN_OP) ? BE_M_MAINTAIN_OP : 0;

    BE_def_mn.drep = (direction == BE_in ? "(*drep)" : "drep");

    /*
     * Init the offset pointer here unless there were just out pipes,
     * in which case it has already been initialized
     */
    if (direction != BE_out || !AST_HAS_OUT_PIPES_SET(oper))
        BE_spell_start_unmarshall(fid, flags, &BE_def_mn);

#ifdef DEBUG_VERBOSE
    if (BE_dump_recs)
    {
        printf("\nrecs dump : BE_unmarshall\n");
        printf(  "-------------------------\n");
        traverse(BE_Recs(oper), 4);
    }
#endif

    param = BE_Recs(oper);
    while (param != NULL)
    {
        /*
         * ool types don't need a separate code path based on drep match.
         * Generate unconditional code until next non-ool type.
         */
        for ( ;
             (param != NULL) && (! BE_within_drep_test(param));
             param = param->next)
        {
            if (BE_PI_Flags(param) & BE_SKIP || AST_ADD_COMM_STATUS_SET(param))
                continue;

            flags = 0;

            if (oper->be_info.oper->flags & BE_MAINTAIN_OP)
                flags |= BE_M_MAINTAIN_OP;
            flags |= (direction == BE_in) ? BE_M_CALLEE : BE_M_CALLER;

            BE_unmarshall_param(fid, param, flags, &BE_def_mn, &routine_mode);
        }
        if ( param == NULL ) break;

        start_param = param;
        start_routine_mode = routine_mode;
        BE_range_of_matched_dreps( start_param, &end_param, &uses_char_drep,
                                    &uses_int_drep, &uses_float_drep );
        wrote_if_clause = FALSE;
        if ( !BE_space_opt
             && (uses_char_drep || uses_int_drep || uses_float_drep) )
        {
            /*
             * Write the "if dreps match" code
             */
            for (param = start_param
                ; param != NULL
                ; param = param->next)
            {
                if (
                        BE_PI_Flags(param) & BE_SKIP
                    ||  AST_ADD_COMM_STATUS_SET(param)
                )
                {
                    if ( param == end_param)
                        break;
                    else
                        continue;
                }

                flags = 0;

                if (oper->be_info.oper->flags & BE_MAINTAIN_OP)
                    flags |= BE_M_MAINTAIN_OP;
                flags |= (direction == BE_in) ? BE_M_CALLEE : BE_M_CALLER;

                if (!wrote_if_clause)
                {
                    BE_spell_drep_test( fid, uses_char_drep, uses_int_drep,
                                        uses_float_drep );
                    fprintf(fid, "{\n");
                    wrote_if_clause = TRUE;
                }
                BE_unmarshall_param(fid, param, flags, &BE_def_mn,
                                    &routine_mode);
                if ( param == end_param )
                    break;
            }
        }

        if (wrote_if_clause)
            fprintf(fid, "}\nelse\n{\n");

        /*
         * Write the "if dreps don't match" code
         */
        routine_mode = start_routine_mode;
        for (param = start_param
            ; param != NULL
            ; param = param->next)
        {
            if (BE_PI_Flags(param) & BE_SKIP || AST_ADD_COMM_STATUS_SET(param))
            {
                if ( param == end_param)
                {
                    param = param->next;
                    break;
                }
                else
                    continue;
            }

            flags = BE_M_CONVERT;

            if (oper->be_info.oper->flags & BE_MAINTAIN_OP)
                flags |= BE_M_MAINTAIN_OP;
            flags |= (direction == BE_in) ? BE_M_CALLEE : BE_M_CALLER;

            BE_unmarshall_param(fid, param, flags, &BE_def_mn, &routine_mode);
            if ( param == end_param )
            {
                param = param->next;
                break;
            }
        }
        /* routine_mode should have the same value from each unmarshalling
            path. The value of  param  at the end of the "dreps don't match"
            path is carried forward into control of the outer loop */

        if (wrote_if_clause) fprintf(fid, "}\n");
    }   /* while (param != NULL) */

    if (direction == BE_in && AST_HAS_IN_PIPES_SET(oper))
        BE_u_match_routine_mode(fid, 0, false, &routine_mode);
    else
        BE_spell_end_unmarshall(fid, &BE_def_mn);
}

/*
 * BE_marshall_param
 */
void BE_marshall_param
#ifdef PROTO
(
    FILE *fid,
    AST_parameter_n_t *param,
    BE_mflags_t flags, /* BE_M_NEW_SLOT|BE_M_MAINTAIN_OP */
    int slot_num,      /* iff flags & BE_NEW_SLOT */
    int *p_slots_used, /* return number of slots used */
    boolean *p_routine_mode,     /* Pointer to TRUE if currently using
                                    routine based marshalling */
    int total_slots
)
#else
(fid, param, flags, slot_num, p_slots_used, p_routine_mode, total_slots)
    FILE *fid;
    AST_parameter_n_t *param;
    BE_mflags_t flags;
    int slot_num;
    int *p_slots_used;
    boolean *p_routine_mode;
    int total_slots;
#endif
{
    boolean using_routine_marshalling;

    if (BE_PI_Flags(param) & BE_SYNC_MP) flags |= BE_M_SYNC_MP;
    if (BE_PI_Flags(param) & BE_ALIGN_MP) flags |= BE_M_ALIGN_MP;
    if (BE_PI_Flags(param) & BE_ADVANCE_MP) flags |= BE_M_ADVANCE_MP;
    if (BE_PI_Flags(param) & BE_DEFER) flags |= BE_M_DEFER;

    /*
     * Emit the before calls for a parameter
     */
    if (BE_Calls_Before(param))
    {
        if (!(flags & BE_M_XMIT_CVT_DONE))
        {
             BE_spell_from_locals(fid, param);     
             BE_spell_to_xmits(fid, param);
        }
        if (flags & BE_M_CALLER)
        {
            /* 
             *  On the callee, context handle setup has been moved to
             *  BE_marshall() so it is performed before all other actions.
             *  Thus this call need only be done for the caller.
             */
            BE_spell_ctx_to_wires(fid, param, BE_caller);
            BE_spell_ptrs_to_array_init(fid, param);
        }
    }

    /* In all cases except a union, the number of slots used will be 0 or 1 */
    *p_slots_used = (flags & BE_M_NEW_SLOT) ? 1 : 0;

    /* In pointed-at and [out_of_line] routines and when looping over the
        elements of an array, we need to manage transitions
        between in-line and routine-based marshalling */
    if (flags & (BE_M_NODE | BE_M_ARRAY_BUFF))
    {
        if (BE_PI_Flags(param) & (BE_OOL | BE_REF_PASS))
            using_routine_marshalling = true;
        else if ( (param->type->kind == AST_pointer_k)
                    && (!(BE_PI_Flags(param) & BE_DEFER)) )
            using_routine_marshalling = true;
        else if ( (param->type->kind == AST_array_k)
                    && (!(BE_PI_Flags(param) & BE_ARRAY_HEADER)) )
            using_routine_marshalling =
                !!(BE_PI_Flags(BE_Array_Info(param)->flat_elt)
                & (BE_OOL | BE_SKIP | BE_OOL_HEADER));
        else
            using_routine_marshalling = false;

        if (*p_routine_mode != using_routine_marshalling)
        {
            BE_m_flip_routine_mode( fid, *p_routine_mode );
           *p_routine_mode = !(*p_routine_mode);
        }
    }

    if (BE_PI_Flags(param) & BE_OOL)
        BE_spell_ool_marshall(fid, param, flags, slot_num);

    else if (BE_PI_Flags(param) & BE_OPEN_RECORD)
        BE_spell_open_record_marshall(fid, param, flags, slot_num);

    else if (BE_PI_Flags(param) & BE_ARRAY_HEADER)
        BE_spell_array_hdr_marshall(fid, param, flags, slot_num);

    else switch (param->type->kind)
    {
        case AST_array_k:
            BE_spell_array_marshall(fid, param, flags, slot_num, p_routine_mode,
                                    total_slots);
            break;
        case AST_disc_union_k:
            BE_spell_union_marshall(fid, param, flags, slot_num, p_routine_mode,
                                    total_slots);
            if ( !(flags & (BE_M_NODE | BE_M_OOL_RTN)) )
                *p_slots_used = BE_Du_Info(param)->vec_size;
            break;
        case AST_pointer_k:
            BE_spell_pointer_marshall(fid, param, flags, slot_num);
            break;
        case AST_null_k:
            BE_spell_null_marshall(fid, param, flags, slot_num);
            break;
        default:
            BE_spell_scalar_marshall(fid, param, flags, slot_num);
    }

    /*
     * Emit the after calls for a parameter
     */
    if (BE_Calls_After(param))
    {
        BE_spell_free_xmits(fid, param);
        BE_spell_free_nets(fid, param);

        /*
         *  Emit TYPE_free() calls on the server for any [out] transmit_as
         *  types and free_local_rep() calls for any [out] represent_as types
         */
        if (flags & BE_M_CALLEE)
        {
            BE_spell_type_frees(fid, param, FALSE);
            BE_spell_free_locals(fid, param, FALSE);
        }
    }
}

/*
 * BE_unmarshall_param
 */
void BE_unmarshall_param
#ifdef PROTO
(
    FILE *fid,
    AST_parameter_n_t *param,
    BE_mflags_t flags, /* BE_M_MAINTAIN_OP */
    BE_mn_t *mn,       /* marshalling names to be used */
    boolean *p_routine_mode     /* Pointer to TRUE if currently using
                                    routine based marshalling */
)
#else
(fid, param, flags, mn, p_routine_mode)
    FILE *fid;
    AST_parameter_n_t *param;
    BE_mflags_t flags;
    BE_mn_t *mn;
    boolean *p_routine_mode;
#endif
{
    boolean using_routine_marshalling;

    if (BE_PI_Flags(param) & BE_ALIGN_MP) flags |= BE_M_ALIGN_MP;
    if (BE_PI_Flags(param) & BE_CHECK_BUFFER) flags |= BE_M_CHECK_BUFFER;
    if (BE_PI_Flags(param) & BE_ADVANCE_MP) flags |= BE_M_ADVANCE_MP;
    if (BE_PI_Flags(param) & BE_DEFER) flags |= BE_M_DEFER;

    /*
     * Assign transmit_as and represent_as unmarshalling surrogates here
     */
    if ( (BE_Calls_Before(param)) && (!(flags & BE_M_XMIT_CVT_DONE)) )
    {
        BE_spell_assign_xmits(fid, param);
        BE_spell_assign_nets(fid, param);
    }

    /* At top level, we need to manage transitions
        between in-line and routine-based marshalling */
    if (!(flags & BE_M_NODE))
    {
        if (BE_PI_Flags(param) & (BE_OOL | BE_REF_PASS))
            using_routine_marshalling = true;
        else if ( (param->type->kind == AST_pointer_k)
                    && (!(BE_PI_Flags(param) & BE_DEFER)) )
            using_routine_marshalling = true;
        else
            using_routine_marshalling = false;

        BE_u_match_routine_mode( fid, flags, using_routine_marshalling,
                                 p_routine_mode );
    }
    if (BE_PI_Flags(param) & BE_OOL)
        BE_spell_ool_unmarshall(fid, param, flags, mn);

    else if (BE_PI_Flags(param) & BE_OPEN_RECORD)
        BE_spell_open_record_unmarshall(fid, param, flags, mn);

    else if (BE_PI_Flags(param) & BE_ARRAY_HEADER)
        BE_spell_array_hdr_unmarshall(fid, param, flags, mn);

    else switch (param->type->kind)
    {
        case AST_array_k:
            BE_spell_array_unmarshall(fid, param, flags, mn, p_routine_mode);
            break;
        case AST_disc_union_k:
            BE_spell_union_unmarshall(fid, param, flags, mn, p_routine_mode);
            break;
        case AST_pointer_k:
            BE_spell_pointer_unmarshall(fid, param, flags, mn);
            break;
        case AST_null_k:
            BE_spell_null_unmarshall(fid, param, flags, mn);
            break;
        default:
            BE_spell_scalar_unmarshall(fid, param, flags, mn);
    }

    /*
     * Emit the after calls for a parameter
     */
    if ( (BE_Calls_After(param)) && (!(flags & BE_M_XMIT_CVT_DONE)) )
    {
        BE_spell_from_xmits(fid, param);
        BE_spell_to_locals(fid, param);
        BE_spell_ctx_from_wires(fid, param,
                                flags & BE_M_CALLER ? BE_caller : BE_callee);
        if (flags & BE_M_CALLEE)
            BE_spell_ptrs_to_conf_init (fid, param);
    }
}
