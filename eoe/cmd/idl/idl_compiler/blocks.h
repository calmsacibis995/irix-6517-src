/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: blocks.h,v $
 * Revision 1.1  1993/08/31 06:48:01  jwag
 * Initial revision
 *
 * Revision 1.1.2.3  1993/01/03  21:38:11  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  14:32:54  bbelch]
 *
 * Revision 1.1.2.2  1992/12/23  18:43:51  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  00:59:54  zeliff]
 * 
 * Revision 1.1  1992/01/19  03:02:08  devrcs
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
**      blocks.h
**
**  FACILITY:
**
**      Interface Definition Language (IDL) Compiler
**
**  ABSTRACT:
**
**  Header file for blocks.c
**
**  VERSION: DCE 1.0
**
*/
#ifndef BLOCKS_H
#define BLOCKS_H

BE_param_blk_t *BE_create_blocks
(
#ifdef PROTO
    AST_parameter_n_t *param,
    BE_side_t side
#endif
);

BE_param_blk_t *BE_prune_blocks
(
#ifdef PROTO
    BE_param_blk_t *blocks
#endif
);

int BE_max_vec_size
(
#ifdef PROTO
    BE_param_blk_t *blocks
#endif
);


int BE_sum_pkt_size
(
#ifdef PROTO
    BE_param_blk_t *blocks,
    long * current_alignment
#endif
);

#endif
