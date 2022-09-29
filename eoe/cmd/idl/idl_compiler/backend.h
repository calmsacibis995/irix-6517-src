/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: backend.h,v $
 * Revision 1.1  1993/08/31 06:48:01  jwag
 * Initial revision
 *
 * Revision 1.1.2.3  1993/01/03  21:38:03  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  14:32:39  bbelch]
 *
 * Revision 1.1.2.2  1992/12/23  18:43:31  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  00:59:39  zeliff]
 * 
 * Revision 1.1  1992/01/19  03:02:03  devrcs
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
**      backend.h
**
**  FACILITY:
**
**      Interface Definition Language (IDL) Compiler
**
**  ABSTRACT:
**
**  Header for backend.c
**
**  VERSION: DCE 1.0
**
*/
boolean BE_main
(
#ifdef PROTO
    boolean             *cmd_opt,   /* [in] array of cmd option flags */
    void                **cmd_val,  /* [in] array of cmd option values */
    FILE                *h_fid,     /* [in] header file handle, or NULL */
    FILE                *caux_fid,  /* [in] client aux file handle, or NULL */
    FILE                *saux_fid,  /* [in] server aux file handle, or NULL */
    FILE                *cstub_fid, /* [in] cstub file handle, or NULL */
    FILE                *sstub_fid, /* [in] sstub file handle, or NULL */
    AST_interface_n_t   *int_p      /* [in] ptr to interface node */
#endif
);

void BE_push_malloc_ctx
(
#ifdef PROTO
      void
#endif
);

void BE_push_perm_malloc_ctx
(
#ifdef PROTO
      void
#endif
);

void BE_pop_malloc_ctx
(
#ifdef PROTO
      void
#endif
);

heap_mem *BE_ctx_malloc
(
#ifdef PROTO
      int size
#endif
);
