/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: backend.c,v $
 * Revision 1.3  1993/09/13 16:11:00  jwag
 * split out typedef changes from ksgen changes
 *
 * Revision 1.2  1993/09/08  20:15:18  jwag
 * first ksgen hack
 *
 * Revision 1.1  1993/08/31  06:48:01  jwag
 * Initial revision
 *
 * Revision 1.1.4.3  1993/01/03  21:38:01  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  14:32:36  bbelch]
 *
 * Revision 1.1.4.2  1992/12/23  18:43:26  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  00:59:36  zeliff]
 * 
 * Revision 1.1.2.2  1992/03/24  14:08:48  harrow
 * 	Add support for bug 4, ship holes for arrays of ref pointers when
 * 	contained within other types.
 * 	[1992/03/19  15:05:09  harrow]
 * 
 * Revision 1.1  1992/01/19  03:00:41  devrcs
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
**      backend.c
**
**  FACILITY:
**
**      Interface Definition Language (IDL) Compiler
**
**  ABSTRACT:
**
**  This module contains the frontend interface to the backend.
**
**  VERSION: DCE 1.0
**
*/

#include <nidl.h>
#include <ast.h>
#include <bedeck.h>
#include <dutils.h>
#include <hdgen.h>
#include <cstubgen.h>
#include <sstubgen.h>
#include <command.h>
#include <nodesupp.h>
#include <nodmarsh.h>
#include <nodunmar.h>
#include <oolrtns.h>
#include <backend.h>
#include <cspell.h>
#include <genpipes.h>

/*
 * Global type instances and boolean
 */
AST_type_n_t *BE_ulong_int_p, *BE_ushort_int_p, *BE_pointer_p, *BE_function_p;
AST_type_n_t *BE_short_null_p, *BE_long_null_p, *BE_hyper_null_p;
boolean BE_bug_array_no_ref_hole;
boolean BE_space_opt, BE_bug_array_align, BE_bug_array_align2, BE_bug_boolean_def;
#ifdef DUMPERS
boolean BE_dump_debug, BE_dump_flat, BE_dump_mnode, BE_dump_mool,
        BE_dump_recs, BE_dump_sends, BE_dump_unode, BE_dump_uool,
        BE_zero_mem;
#endif
#ifdef __sgi
boolean BE_ksgen;
boolean BE_kstypes;
#endif

/*
 *  be_init
 *
 * Initialize the various backend globals
 */
static void be_init
#ifdef PROTO
(
    boolean *cmd_opt,
    void **cmd_val
)
#else
(cmd_opt, cmd_val)
    boolean *cmd_opt;
    void **cmd_val;
#endif
{
    boolean *bugs;

    /*
     * initialize various options-related globals
     */
    BE_space_opt = cmd_opt[opt_space_opt];
#ifdef DUMPERS
    BE_dump_debug = cmd_opt[opt_dump_debug];
    BE_dump_flat  = cmd_opt[opt_dump_flat];
    BE_dump_mnode = cmd_opt[opt_dump_mnode];
    BE_dump_mool  = cmd_opt[opt_dump_mool];
    BE_dump_recs  = cmd_opt[opt_dump_recs];
    BE_dump_sends = cmd_opt[opt_dump_sends];
    BE_dump_unode = cmd_opt[opt_dump_unode];
    BE_dump_uool  = cmd_opt[opt_dump_uool];
    BE_zero_mem   = (getenv("IDL_zero_mem") != NULL);
#endif

    bugs = (boolean *)cmd_val[opt_do_bug];
    BE_bug_array_align = bugs[bug_array_align];
    BE_bug_array_align2 = bugs[bug_array_align2];
    BE_bug_boolean_def = bugs[bug_boolean_def];
    BE_bug_array_no_ref_hole = bugs[bug_array_no_ref_hole];

    /*
     * initialize global type instances
     */
    BE_ulong_int_p = BE_get_type_node (AST_long_unsigned_k);
    BE_ulong_int_p->alignment_size = 4;
    BE_ulong_int_p->ndr_size = 4;

    BE_ushort_int_p = BE_get_type_node (AST_short_unsigned_k);
    BE_ushort_int_p->alignment_size = 2;
    BE_ushort_int_p->ndr_size = 2;

    BE_pointer_p = BE_get_type_node (AST_pointer_k);
    BE_pointer_p->alignment_size = 4;
    BE_pointer_p->ndr_size = 4;

    BE_function_p = BE_get_type_node (AST_function_k);
    BE_function_p->alignment_size = 0;
    BE_function_p->ndr_size = 0;

    BE_short_null_p = BE_get_type_node (AST_null_k);
    BE_short_null_p->alignment_size = 2;
    BE_short_null_p->ndr_size = 0;

    BE_long_null_p = BE_get_type_node (AST_null_k);
    BE_long_null_p->alignment_size = 4;
    BE_long_null_p->ndr_size = 0;

    BE_hyper_null_p = BE_get_type_node (AST_null_k);
    BE_hyper_null_p->alignment_size = 8;
    BE_hyper_null_p->ndr_size = 0;

#ifdef __sgi
    BE_ksgen = cmd_opt[opt_ksgen];
    BE_kstypes = cmd_opt[opt_kstypes];
#endif
}

/******************************************************************************/
/* BE Temporary-Memory Management                                             */
/******************************************************************************/
/* ABSTRACT:                                                                  */
/*   Special BE memory management routines.  The following three routines     */
/*   provides memory management in contexts (heap zones).  When entering a    */
/*   context the BE_push_malloc_ctx routine is called, upon entering a        */
/*   context all calls to BE_ctx_malloc to allocate memory will be associated */
/*   with the context.  When the context is exited by calling                 */
/*   BE_pop_malloc_ctx, all memory allocated with BE_ctx_malloc, is freed.    */
/*                                                                            */
/* NOTE:                                                                      */
/*   Memory allocated via BE_ctx_malloc, cannot be freed other than by        */
/*   exiting the current malloc context as it adds a header to the memory     */
/*   in order to keep a list of active allocations.  Calls to free() with     */
/*   allocations returned from BE_ctx_malloc, will cause heap corruption.     */
/*                                                                            */
/* ROUTINES:                                                                  */
/*   BE_cxt_malloc  -- Same interface as MALLOC, if not within a context      */
/*                     returns memory directly from malloc()                  */
/*   BE_push_malloc_ctx -- Start new temporary context                        */
/*                         is used directly and the memory never freed        */
/*   BE_push_perm_malloc_ctx -- Start new temporary context in which MALLOC   */
/*   BE_pop_malloc_ctx -- Free all memory allocated since start of context    */
/*                                                                            */
/******************************************************************************/

/*
 * Type used to add our context header to allocations returned from MALLOC
 */
typedef struct malloc_t {
      struct malloc_t *next;    /* Pointer to next allocation on this chain */
      char *data;               /* Start of memory returned to caller */
      } malloc_t;

/*
 * Type used to maintain a list of allocation contexts.
 */
typedef struct malloc_ctx_t {
      struct malloc_ctx_t *next;    /* Pointer to next context */
      malloc_t *list;               /* Head of allocation chain for this context. */
      boolean permanent;            /* If true, this is a permanent context */
      } malloc_ctx_t;

/*
 * Current malloc context, initially NULL so normal MALLOC is used
 */
static malloc_ctx_t *malloc_ctx = NULL;

/*
** BE_ctx_malloc: allocate memory in the current context.
*/
heap_mem *BE_ctx_malloc
#ifdef PROTO
(
    int size
)
#else
(size)
    int size;
#endif
{
      malloc_t *new;

      /* If no malloc context, just return memory */
      if (malloc_ctx == NULL) return MALLOC(size);

      /* If current malloc context is permanent, then just return memory */
      if (malloc_ctx->permanent == true) return MALLOC(size);

      /* Allocate memory with our context header */
      new = (malloc_t *)MALLOC(size + sizeof(malloc_t));

      /* Link the new allocation on the current context list */
      new->next = malloc_ctx->list;
      malloc_ctx->list = new;

#ifdef DUMPERS
      /* If BE_zero_mem set, zero out the allocated memory to help find bugs */
      if (BE_zero_mem) memset(&new->data, 0, size);
#endif

      /* Return the value after our header for use by the caller */
      return (heap_mem*)&new->data;
}

/*
** BE_push_malloc_ctx: Push a new context in which memory is allocated
*/
void BE_push_malloc_ctx
#ifdef PROTO
(
      void
)
#else
()
#endif
{
      /*
       * Allocate a malloc context block to hang allocations made in this
       * context off of.
       */
      malloc_ctx_t *new = (malloc_ctx_t*)MALLOC(sizeof(malloc_ctx_t));

      /* Link new context on the top of the context stack */
      new->next = malloc_ctx;
      new->list = (malloc_t*)NULL;
      new->permanent = false;
      malloc_ctx = new;
}


/*
** BE_push_perm_malloc_ctx: Push a new context in which memory is allocated
** such that the next call to BE_pop_malloc_ctx does not free it.  MALLOC
** is used directly.
*/
void BE_push_perm_malloc_ctx
#ifdef PROTO
(
      void
)
#else
()
#endif
{
      /*
       * Allocate a malloc context block to hang allocations made in this
       * context off of.
       */
      malloc_ctx_t *new = (malloc_ctx_t*)MALLOC(sizeof(malloc_ctx_t));

      /* Link new context on the top of the context stack */
      new->next = malloc_ctx;
      new->list = (malloc_t*)NULL;
      new->permanent = true;
      malloc_ctx = new;
}


/*
** BE_pop_malloc_ctx: Pop the current context, freeing all memory allocated
** within this context (unless it was a permanent context).
*/
void BE_pop_malloc_ctx
#ifdef PROTO
(
    void
)
#else
()
#endif
{
      malloc_t *list,*curr;
      malloc_ctx_t *ctx;

      /* If we are called with an empty stack, then abort */
      if (malloc_ctx == NULL)
          error(NIDL_INTERNAL_ERROR,__FILE__,__LINE__);

      /* Loop through the context freeing all memory */
      list = malloc_ctx->list;
      while (list != NULL)
      {
          curr = list;
          list = list->next;
          free(curr);
      }

      /* Remove context from the stack, and free the context header */
      ctx = malloc_ctx;
      malloc_ctx = malloc_ctx->next;
      free(ctx);
}


/*
 *  BE_main
 */
boolean BE_main              /* returns true on successful completion */
#ifdef PROTO
(
    boolean             *cmd_opt,   /* [in] array of cmd option flags */
    void                **cmd_val,  /* [in] array of cmd option values */
    FILE                *h_fid,     /* [in] header file handle, or NULL */
    FILE                *caux_fid,  /* [in] client aux file handle, or NULL */
    FILE                *saux_fid,  /* [in] server aux file handle, or NULL */
    FILE                *cstub_fid, /* [in] cstub file handle, or NULL */
    FILE                *sstub_fid, /* [in] sstub file handle, or NULL */
    AST_interface_n_t   *int_p      /* [in] ptr to interface node */
)
#else
(cmd_opt, cmd_val, h_fid, caux_fid, saux_fid, cstub_fid, sstub_fid, int_p)
    boolean *cmd_opt;
    void **cmd_val;
    FILE *h_fid;
    FILE *caux_fid;
    FILE *saux_fid;
    FILE *cstub_fid;
    FILE *sstub_fid;
    AST_interface_n_t *int_p;
#endif
{
    be_init (cmd_opt, cmd_val);

#ifdef __sgi
    if (BE_ksgen) {
	if (h_fid)
		ksgen_header(h_fid, int_p,
		    (boolean *)cmd_val[opt_do_bug], cmd_opt[opt_cepv]);
	return true;
    }
#endif
    if (h_fid)
        BE_gen_c_header(h_fid, int_p,
            (boolean *)cmd_val[opt_do_bug], cmd_opt[opt_cepv]);

    BE_clone_anonymous_pa_types (int_p);

    /*
     * emit client stub file if requested
     */
    if (cstub_fid)
        BE_gen_cstub (cstub_fid, int_p, lang_c_k,
            (char *)cmd_val[opt_header], (boolean *)cmd_val[opt_do_bug],
            cmd_opt[opt_cepv]);

    /*
     * emit server stub file if requested
     */
    if (sstub_fid)
        BE_gen_sstub (sstub_fid, int_p, lang_c_k, (char *)cmd_val[opt_header],
                 (boolean *)cmd_val[opt_do_bug], cmd_opt[opt_mepv]);

    /*
     * emit server auxilliary file if requested
     */
    if (saux_fid)
    {
        CSPELL_std_include (saux_fid, (char *)cmd_val[opt_header],
            BE_server_aux_k, int_p->op_count);
        BE_gen_node_marshalling (saux_fid, int_p, BE_callee);
        BE_gen_node_unmarshalling (saux_fid, int_p, BE_callee);
        BE_gen_pipe_routines (saux_fid, int_p);
        BE_out_of_line_routines (saux_fid, int_p, BE_callee);
    }

    /*
     * emit client auxilliary file if requested
     */
    if (caux_fid)
    {
        CSPELL_std_include (caux_fid, (char *)cmd_val[opt_header],
            BE_client_aux_k, int_p->op_count);
        BE_gen_node_marshalling (caux_fid, int_p, BE_caller);
        BE_gen_node_unmarshalling (caux_fid, int_p, BE_caller);
        BE_out_of_line_routines (caux_fid, int_p, BE_caller);
    }

    return true;
}
