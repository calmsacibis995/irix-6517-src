/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: commstat.h,v $
 * Revision 1.1  1993/08/31 06:48:01  jwag
 * Initial revision
 *
 * Revision 1.1.2.3  1993/01/03  21:38:40  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  14:33:38  bbelch]
 *
 * Revision 1.1.2.2  1992/12/23  18:44:50  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  01:00:49  zeliff]
 * 
 * Revision 1.1  1992/01/19  03:02:16  devrcs
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
**      commstat.h
**
**  FACILITY:
**
**      Interface Definition Language (IDL) Compiler
**
**  ABSTRACT:
**
**      Data types and function prototypes for commstat.c
**
**  VERSION: DCE 1.0
**
*/

#ifndef COMMSTAT_H
#define COMMSTAT_H

typedef enum {BE_stat_param_k, BE_stat_result_k, BE_stat_addl_k,
              BE_stat_except_k} BE_stat_k_t;

typedef struct BE_stat_info_t {
    BE_stat_k_t type;
    NAMETABLE_id_t name;
} BE_stat_info_t;

void BE_get_comm_stat_info
(
#ifdef PROTO
    AST_operation_n_t *p_operation,
    BE_stat_info_t *p_comm_stat_info
#endif
);

void BE_get_fault_stat_info
(
#ifdef PROTO
    AST_operation_n_t *p_operation,
    BE_stat_info_t *p_fault_stat_info
#endif
);

void CSPELL_receive_fault
(
#ifdef PROTO
    FILE *fid
#endif
);

void CSPELL_return_status
(
#ifdef PROTO
    FILE *fid,
    BE_stat_info_t *p_comm_stat_info,
    BE_stat_info_t *p_fault_stat_info,
    char *status_var_name,
    char *result_param_name
#endif
);

#endif

