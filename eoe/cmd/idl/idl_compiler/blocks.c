/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: blocks.c,v $
 * Revision 1.1  1993/08/31 06:48:01  jwag
 * Initial revision
 *
 * Revision 1.1.2.3  1993/01/03  21:38:09  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  14:32:51  bbelch]
 *
 * Revision 1.1.2.2  1992/12/23  18:43:47  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  00:59:50  zeliff]
 * 
 * Revision 1.1  1992/01/19  03:00:42  devrcs
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
**      blocks.c
**
**  FACILITY:
**
**      Interface Definition Language (IDL) Compiler
**
**  ABSTRACT:
**
**  Routines for grouping flattened parameters into blocks.
**
**  VERSION: DCE 1.0
**
*/

#include <nidl.h>
#include <ast.h>
#include <bedeck.h>
#include <dutils.h>
#include <blocks.h>
#include <backend.h>
#include <bounds.h>

#define Append(list, item)\
    if (list == NULL) list = item;\
    else {list->last->next = item; list->last = list->last->next->last;}

/*
 * slot_params
 *
 * Determine which parameters in the flattened list passed require a
 * new slot; mark them with  BE_NEW_SLOT as appropriate
 */
static void slot_params
#ifdef PROTO
(
    AST_parameter_n_t *params,
    boolean within_arm
)
#else
(params, within_arm)
    AST_parameter_n_t *params;
    boolean within_arm;
#endif
{
    BE_arm_t *arm;
    AST_parameter_n_t *param, *pp;
    boolean new_slot = false;

    for (param = params; param; param = param->next)
    {
        if (BE_PI_Flags(param) & BE_SKIP)
            continue;

        if (new_slot ||
                (BE_PI_Flags(param) & BE_NEW_BLOCK) ||
                (param->type->kind == AST_array_k &&
                    !(BE_PI_Flags(param) & BE_ARRAY_HEADER))||
                param->type->kind == AST_disc_union_k)
            BE_PI_Flags(param) |= BE_NEW_SLOT;

        if (param->type->kind == AST_disc_union_k)
            for (arm = BE_Du_Info(param)->arms; arm; arm = arm->next)
                if (arm->flat)
                {
                    for (pp = arm->flat; pp != NULL; pp = pp->next)
                    {
                        if ( !(BE_PI_Flags(pp) & BE_SKIP) )
                        {
                            BE_PI_Flags(pp) |= BE_NEW_SLOT;
                            slot_params(pp, true);
                            break;
                        }
                    }
                }
#if 0
        else if (param->type->kind == AST_pointer_k)
            slot_params(BE_Ptr_Info(param)->pointee);
#endif
        new_slot = ((param->type->kind == AST_array_k) &&
                            !(BE_PI_Flags(param) & BE_ARRAY_HEADER))
                   || (param->type->kind == AST_disc_union_k)
                   || (within_arm && (BE_PI_Flags(param) & BE_OOL));
    }
}


/*
 * BE_create_blocks
 *
 * Arranges the parameter list passed in "blocks" corresponding to
 * the groups in which they will be transmitted; calls slot_params to
 * further subdivide parameters into iovector slots.
 */

typedef enum {bk_none, bk_sp, bk_inline, bk_prune} block_kind_t;

BE_param_blk_t *BE_create_blocks
#ifdef PROTO
(
    AST_parameter_n_t *params,
    BE_side_t side
)
#else
(params, side)
    AST_parameter_n_t *params;
    BE_side_t side;
#endif
{
    AST_parameter_n_t *param, *this_param;
    BE_param_blk_t *block_list, *this_block, *bp;
    block_kind_t current_block_kind = bk_none, next_block_kind;

    /* cons up a block list */
    block_list = NULL;

    for (param = params; param; param = param->next)
    {
        if (param->type->kind == AST_pipe_k) continue;

        if ((side == BE_client_side && !AST_IN_SET(param)) ||
            (side == BE_server_side && !AST_OUT_SET(param))) continue;

        if (BE_PI_Flags(param) & BE_SKIP)
            next_block_kind = (current_block_kind == bk_none)
                              ? bk_inline : current_block_kind;
        else if (param->type->kind == AST_pointer_k
             && !(BE_PI_Flags(param) & BE_DEFER))
            next_block_kind = bk_sp;
        else if (BE_PI_Flags(param) & BE_REF_PASS)
            next_block_kind = bk_sp;
        else if (BE_PI_Flags(param) & BE_OOL)
            next_block_kind = bk_sp;
        else if (BE_PI_Flags(param) & BE_ARRAY_HEADER)
        {
            /* conformance always marshalled in-line */
            if (AST_CONFORMANT_SET(param->type))
                next_block_kind = bk_inline;
            /*  prune header if param is not varying */
            else if (!AST_VARYING_SET(param))
            {
                next_block_kind = bk_prune;
                /*
                    If this block is to be pruned because its only param is an
                    array header that needs no marshalling, mark the following
                    array param as BE_HDRLESS and move any call_before info
                    from the array header param onto the following array param.
                    Assumption: an array body cannot have any call_before info.
                */
                BE_PI_Flags(param->next) |= BE_HDRLESS;
                BE_Calls_Before(param->next) = BE_Calls_Before(param);
            }
            /* otherwise A,B marshalled in-line */
            else
                next_block_kind = bk_inline;
        }
        else
            next_block_kind = bk_inline;

        if (current_block_kind != next_block_kind)
        {
            current_block_kind = next_block_kind;
            this_block = (BE_param_blk_t *)BE_ctx_malloc(sizeof(BE_param_blk_t));
            this_block->params = NULL;
            this_block->next = NULL;
            this_block->last = this_block;

            switch (next_block_kind)
            {
                case bk_sp:
                    this_block->flags = BE_SP_BLOCK;
                    break;
                case bk_prune:
                    this_block->flags = BE_PRUNED_BLOCK;
                    break;
                case bk_inline:
                    this_block->flags = 0;
            }

            Append(block_list, this_block);
        }

        this_param = (AST_parameter_n_t *)BE_ctx_malloc(sizeof(AST_parameter_n_t));
        *this_param = *param;
        this_param->next = NULL;
        this_param->last = this_param;

        Append(this_block->params, this_param);
    }

    for (this_block = block_list; this_block; this_block = this_block->next)
    {
        for (param = this_block->params; param != NULL; param = param->next)
            if (!(BE_PI_Flags(param) & BE_SKIP))
            {
                BE_PI_Flags(param) |= BE_NEW_BLOCK;
                break;
            }
        slot_params(this_block->params, false);
    }

    /*
     * Mark the first block with BE_FIRST_BLOCK
     */
    if (block_list != NULL) block_list->flags |= BE_FIRST_BLOCK;

    return block_list;
}

/*
 * BE_prune_blocks
 *
 * Removes params marked BE_SKIP from the param list of each block in a
 * block list.  Removes any block that does not have any non-BE_SKIP params.
 * Also elides blocks marked BE_PRUNED_BLOCK in BE_create_blocks above.
 */
BE_param_blk_t *BE_prune_blocks
#ifdef PROTO
(
    BE_param_blk_t *blocks
)
#else
(blocks)
    BE_param_blk_t *blocks;
#endif
{
    AST_parameter_n_t *param, *next_param, *new_params;
    BE_param_blk_t *block, *next_block, *new_blocks;

    new_blocks = NULL;
    for (block = blocks; block != NULL; block = next_block)
    {
        next_block = block->next;

        new_params = NULL;
        if (!(block->flags & BE_PRUNED_BLOCK))
        {
            for (param = block->params; param != NULL; param = next_param)
            {
                next_param = param->next;
                if (!(BE_PI_Flags(param) & BE_SKIP))
                {
                    param->next = NULL;
                    param->last = param;
                    if (new_params == NULL)
                        new_params = param;
                    else
                        Append(new_params, param);
                }
            }
        }

        if (new_params != NULL)
        {
            block->next = NULL;
            block->last = block;
            block->params = new_params;
            if (new_blocks == NULL)
                new_blocks = block;
            else
                Append(new_blocks, block);
        }
    }

    return new_blocks;
}


/*
 * vec_size
 *
 * Returns the number of iovector slots required to marshall a given parameter
 * In general, this number is 0 unless the BE_NEW_SLOT flag is set, in which
 * case it is 1.  For unions, however, it is the maximum of the slots required
 * for any arm of the union.
 */
static int vec_size
#ifdef PROTO
(
    AST_parameter_n_t *param
)
#else
(param)
    AST_parameter_n_t *param;
#endif
{
    int this_arm, pointee, max_slots;
    BE_arm_t *arm;
    AST_parameter_n_t *pp;

    if (param->type->kind == AST_disc_union_k)
    {
        /*
         * Find the maximum of the number of slots required to marshall each
         * of the arms of the union
         */
        max_slots = 0;
        for (arm = BE_Du_Info(param)->arms; arm; arm = arm->next)
        {
            this_arm = 0;
            for (pp = arm->flat; pp; pp = pp->next)
            {
                if ( ! ( BE_PI_Flags(pp) & (BE_OOL | BE_SKIP) ) )
                    this_arm += vec_size(pp);
            }
            if (this_arm > max_slots) max_slots = this_arm;
            debug(("arm slots required: %d\n", this_arm));
        }
        BE_Du_Info(param)->vec_size = max_slots;
        debug(("%s max slots required: %d\n", BE_get_name(param->name),
              max_slots));

        return max_slots;
    }
#if 0
    else if (param->type->kind == AST_pointer_k &&
             !(BE_PI_Flags(param) & BE_DEFER) && !BE_Is_SP_Pointer(param))
    {
        pointee = 0;
        for (pp = BE_Ptr_Info(param)->pointee; pp; pp = pp->next)
            pointee += vec_size(pp);
        BE_Ptr_Info(param)->pointee_slots = pointee;
        if (BE_PI_Flags(param) & BE_NEW_SLOT) return pointee + 1;
        else return pointee;
    }
#endif
    else if (BE_PI_Flags(param) & BE_NEW_SLOT)
    {
        if ( (BE_PI_Flags(param) & BE_HDRLESS)
             && (BE_PI_Flags(param) & BE_ALIGN_MP) )
            /* Need a slot for the array alignment */
            return 2;
        else
            return 1;
    }
    else return 0;
}

/*
 * BE_max_vec_size
 *
 * Returns the max number of iovector slots needed by any of a list of blocks
 */
int BE_max_vec_size
#ifdef PROTO
(
    BE_param_blk_t *blocks
)
#else
(blocks)
    BE_param_blk_t *blocks;
#endif
{
    AST_parameter_n_t *param;
    BE_param_blk_t *block;
    int max = 0;

    for (block = blocks; block; block = block->next)
    {
        block->vec_size = 0;

        for (param = block->params; param; param = param->next)
            if (!(BE_PI_Flags(param) & BE_SKIP))
                block->vec_size += vec_size(param);

        debug(("Block size: %d\n", block->vec_size));
        if (block->vec_size > max) max = block->vec_size;
    }
    return max;
}

/*
 * BE_sum_pkt_size
 *
 * Returns the sum of the packet sizes required by all of the parameters
 */
int BE_sum_pkt_size
#ifdef PROTO
(
    BE_param_blk_t *blocks,
    long * current_alignment
)
#else
(blocks, current_alignment)
    BE_param_blk_t *blocks;
    long * current_alignment;
#endif
{
    AST_parameter_n_t *param;
    BE_param_blk_t *block;
    int this_block, sum;

    if (blocks == NULL) return 0;

    /*
     * Leave 7 bytes so that the packet pointer can be initialized
     * to an 8-byte boundary
     */
    sum = 7;

    for (block = blocks; block; block = block->next)
        for (param = block->params; param; param = param->next)
            sum += BE_pkt_size(param, current_alignment);

    return sum;
}
