/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(_KERNEL)
static const char *_identString = "$Id: ctxeectx.c,v 65.4 1998/03/23 17:25:32 gwehrman Exp $";
static void _identFunc(const char *x) {if(!x)_identFunc(_identString);}
#endif



/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1996 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: ctxeectx.c,v $
 * Revision 65.4  1998/03/23 17:25:32  gwehrman
 * Source Identification changed to bracket with SGIMIPS && !_KERNEL.
 *
 * Revision 65.3  1998/01/07 17:20:03  jdoak
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS
 *
 * Revision 65.2  1997/11/06  19:56:12  jdoak
 * Source Identification added.
 *
 * Revision 65.1  1997/10/20  19:15:31  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.12.2  1996/02/18  18:52:59  marty
 * 	Update OSF copyright years
 * 	[1996/02/18  18:06:06  marty]
 *
 * Revision 1.1.12.1  1995/12/07  22:24:19  root
 * 	Submit OSF/DCE 1.2.1
 * 
 * 	HP revision /main/HPDCE02/jrr_1.2_mothra/1  1995/11/20  21:38 UTC  psn
 * 	Merge second XIDL drop for DCE 1.2.1
 * 	[1995/11/17  17:06 UTC  dat  /main/dat_xidl2/1]
 * 
 * 	oct 95 idl drop
 * 	[1995/10/22  23:35:47  bfc]
 * 	 *
 * 	may 95 idl drop
 * 	[1995/10/22  22:56:47  bfc]
 * 	 *
 * 	DCE for DEC OSF/1: populate from OSF DCE R1.1
 * 	[1995/10/21  18:25:15  bfc]
 * 	 *
 * 
 * 	HP revision /main/HPDCE02/1  1995/08/25  14:59 UTC  tatsu_s
 * 	Submitted the fix for CHFts16000/OT13023.
 * 
 * 	HP revision /main/tatsu_s.psock_timeout.b0/1  1995/08/16  18:25 UTC  tatsu_s
 * 	Avoid the recursive mutex unlock.
 * 	[1995/12/07  21:13:49  root]
 * 
 * Revision 1.1.8.2  1994/07/28  20:56:03  tom
 * 	Bug 9689 - use ctx_rundown_fn_p_t (from stubbase.h).
 * 	[1994/07/28  20:55:42  tom]
 * 
 * Revision 1.1.8.1  1994/01/21  22:30:31  cbrooks
 * 	RPC Code Cleanup - Initial Submission
 * 	[1994/01/21  21:51:17  cbrooks]
 * 
 * Revision 1.1.6.2  1993/07/07  20:04:49  ganni
 * 	reduced stub idl sources
 * 	[1993/07/07  19:35:29  ganni]
 * 
 * $EndLog$
 */
/*
**
**  Copyright (c) 1990, 1991,1992 by
**      Hewlett-Packard Company, Palo Alto, Ca. &
**  Digital Equipment Corporation, Maynard, Mass.
**  All Rights Reserved.  Unpublished rights reserved
**  under the copyright laws of the United States.
**
**  The software contained on this media is proprietary
**  to and embodies the confidential technology of
**  Digital Equipment Corporation.  Possession, use,
**  duplication or dissemination of the software and
**  media is authorized only pursuant to a valid written
**  license from Digital Equipment Corporation.
**
**  RESTRICTED RIGHTS LEGEND   Use, duplication, or
**  disclosure by the U.S. Government is subject to
**  restrictions as set forth in Subparagraph (c)(1)(ii)
**  of DFARS 252.227-7013, or in FAR 52.227-19, as
**  applicable.
**
**  NAME:
**
**      ctxeectx.c
**
**  FACILITY:
**
**      IDL Stub Runtime Support
**
**  ABSTRACT:
**
**      Maintain callee stub's table of contexts
**
**  MODIFICATION HISTORY:
**
**  01-Apr-92 A.I.Hinxman   Release mutex before raising mismatch exception
**  25-Mar-92 A.I.Hinxman   Context handle code review
**  04-Mar-92 A.I.Hinxman   Prevent context rundown while manager active
**  13-Sep-91 Annicchiarico change #include ordering
**  28-Aug-91 Al Simons     Init context table w/ a memset.
**  05-Aug-91 A.I.Hinxman   Return error statuses from rpc_ss_ee_ctx_to_wire
**  29-Jul-91 A.I.Hinxman   Once-block performance improvements
**  13-May-91 tbl           Allocate client context table at run time.
**  10-May-91 tbl           Fix _to_wire() and _from_wire() headers.
**                          Cast status args from volatile where necessary.
**  09-May-91 tbl           NIDL_PROTOTYPES => IDL_PROTOTYPES
**  07-May-91 tbl           Include lsysdep.h.
**  29-Apr-91 tbl           Handle NULL to non-NULL transition
**                          for [in,out] context handle.
**  23-Apr-91 A.I.Hinxman   Release mutex after exception
**  02-Apr-91 A.I.Hinxman   Fix hash table bug and null attributes.
**  02-Apr-91 Annicchiarico Put in context handle debugging support.
**  21-Mar-91 A.I.Hinxman   Improved hash table and mutex management
**  04-Mar-91 A.I.Hinxman   rpc_ss_status_all_t -> error_status_t
**  27-Feb-91 Annicchiarico Change status return values to parameters
**  12-feb-91 labossiere    ifdef stdio.h for kernel
**  17-Dec-90 A.I.Hinxman   Correct handling of failure of rpc_binding_inq_client
**  26-Jul-90 A.I.Hinxman   [out] NULL context handle
**  04-Jul-90 wen           wire_to_ctx=>ctx_from_wire
**  25-Apr-90 A.I.Hinxman   Enhanced functionality
**  06-Mar-90 wen           status_ok=>error_status_ok
**  22-Feb-90 A.I.Hinxman   Original version.
**
*/

/* The ordering of the following 3 includes should NOT be changed! */
#include <dce/rpc.h>
#include <dce/stubbase.h>
#include <lsysdep.h>

#include <dce/uuid.h>

#include <ctxeertl.h>

#ifndef DEBUG_VERBOSE
#   define NDEBUG
#endif
#include <assert.h>

#ifdef PERFMON
#include <dce/idl_log.h>
#endif

#ifdef CTXEETEST
#   include <stdio.h>
#   define DPRINT(ARGS) printf ARGS
#else
#   define DPRINT(ARGS)
#endif

/******************************************************************************/
/*                                                                            */
/*    Set up CMA machinery required by context tables                         */
/*                                                                            */
/******************************************************************************/

#ifndef VMS
    ndr_boolean rpc_ss_context_is_set_up = ndr_false;
#endif

static RPC_SS_THREADS_ONCE_T context_once = RPC_SS_THREADS_ONCE_INIT;

RPC_SS_THREADS_MUTEX_T rpc_ss_context_table_mutex;

static void rpc_ss_init_context(
#ifdef IDL_PROTOTYPES
    void
#endif
)
{
    /* Create mutex for context handle tables */
    RPC_SS_THREADS_MUTEX_CREATE( &rpc_ss_context_table_mutex );
    /* And initialize the tables */
    rpc_ss_init_callee_ctx_tables();
}

void rpc_ss_init_context_once(
#ifdef IDL_PROTOTYPES
    void
#endif
)
{
    RPC_SS_THREADS_INIT;
    RPC_SS_THREADS_ONCE( &context_once, rpc_ss_init_context );
#ifndef VMS
    rpc_ss_context_is_set_up = ndr_true;
#endif

}

/*  Number of context slots in hash table.
*/
#define CALLEE_CONTEXT_TABLE_SIZE 256

static callee_context_entry_t *context_table = NULL;

/*  Allocate and initialize callee context and client lookup tables.
*/
void rpc_ss_init_callee_ctx_tables(
#ifdef IDL_PROTOTYPES
    void
#endif
)
{
    int i = 0;
    error_status_t status = 0;

#ifdef PERFMON
    RPC_SS_INIT_CALLEE_CTX_TABLES_N;
#endif

    assert(!context_table);     /* Must not be called more than once. */

    context_table = (callee_context_entry_t *)malloc(
        CALLEE_CONTEXT_TABLE_SIZE * sizeof(callee_context_entry_t)
    );

    if (!context_table)
        RAISE(rpc_x_no_memory);

/*****************
**
** The memset below has the same effect as this more descriptive loop.
** 
**     for (i = 0; i < CALLEE_CONTEXT_TABLE_SIZE; i++) {
**         uuid_create_nil(&context_table[i].uuid, &status);
**         context_table[i].next_context = NULL;
**     }
** 
******************/

    memset (context_table, 0, 
            CALLEE_CONTEXT_TABLE_SIZE * sizeof(callee_context_entry_t));
    rpc_ss_init_callee_client_table();

#ifdef PERFMON
    RPC_SS_INIT_CALLEE_CTX_TABLES_X;
#endif

}

/******************************************************************************/
/*                                                                            */
/*    Add an entry to the callee context lookup table                         */
/*                                                                            */
/******************************************************************************/
void rpc_ss_create_callee_context
#ifdef IDL_PROTOTYPES
(
    rpc_ss_context_t callee_context,/* The user's local form of the context */
    uuid_t    *p_uuid,              /* Pointer to the equivalent UUID */
    handle_t h,                     /* Binding handle */
    void (*ctx_rundown)(rpc_ss_context_t), /* Pointer to context rundown routine */
    error_status_t *result     /* Function result */
)
#else
(callee_context, p_uuid, h, ctx_rundown, result)
    rpc_ss_context_t callee_context;/* The user's local form of the context */
    uuid_t    *p_uuid;              /* Pointer to the equivalent UUID */
    handle_t h;                     /* Binding handle */
    void (*ctx_rundown)(rpc_ss_context_t);          /* Pointer to context rundown routine */
    error_status_t *result;    /* Function result */
#endif
{
    rpc_client_handle_t  ctx_client;         /* ID of client owning context */
    callee_context_entry_t *this_link, *next_link, *new_link;
    ndr_boolean is_new_client;

#ifdef PERFMON
    RPC_SS_CREATE_CALLEE_CONTEXT_N;
#endif

    /* If this is the first context to be created, initialization is needed */
    RPC_SS_INIT_CONTEXT

    rpc_binding_inq_client(h, &ctx_client, (error_status_t *) result);
    if (*result != error_status_ok) return;

    RPC_SS_THREADS_MUTEX_LOCK(&rpc_ss_context_table_mutex);
    DPRINT(("Seized context tables\n"));
    this_link = &context_table[uuid_hash((uuid_p_t)p_uuid,(error_status_t *)result)
                                               % CALLEE_CONTEXT_TABLE_SIZE];
    if ( uuid_is_nil((uuid_p_t)&this_link->uuid, (error_status_t *)result) )
    {
        /* Home slot in the hash table is empty */
        new_link = this_link;
        next_link = NULL;
    }
    else
    {
        /* Put the new item at the head of the overflow chain */
        new_link = (callee_context_entry_t *)
                             malloc(sizeof(callee_context_entry_t));
        if (new_link == NULL)
        {
            RPC_SS_THREADS_MUTEX_UNLOCK(&rpc_ss_context_table_mutex);
            DPRINT(("Released context tables\n"));
            RAISE( rpc_x_no_memory );
        }
        next_link = this_link->next_context;
        this_link->next_context = new_link;
    }

    /* Fill in fields of context entry */
    memcpy(
        (char *)&new_link->uuid,
        (char *)p_uuid,
        sizeof(uuid_t)
    );
    new_link->user_context = callee_context;
    new_link->rundown = ctx_rundown;
    new_link->next_context = next_link;

    TRY
    rpc_ss_add_to_callee_client(ctx_client,new_link,&is_new_client,
                                result);
    /*
     * rpc_ss_add_to_callee_client() releases the context
     * table mutex, before raising rpc_x_no_memory,
     *
     * Note: Using FINALLY with CATCH clause is not
     * supported, but works.
     */
    CATCH( rpc_x_no_memory )
	RERAISE;
    FINALLY
    RPC_SS_THREADS_MUTEX_UNLOCK(&rpc_ss_context_table_mutex);
    DPRINT(("Released context tables\n"));
    ENDTRY
    if ((*result == error_status_ok) && is_new_client)
    {
        rpc_network_monitor_liveness( h, ctx_client,
                             rpc_ss_rundown_client,
                             (error_status_t *) result );
#ifdef PERFMON
    RPC_SS_CREATE_CALLEE_CONTEXT_X;
#endif
    }
}

/******************************************************************************/
/*                                                                            */
/*  Update an entry in the callee context lookup table                        */
/*  *result is error_status_ok unless the UUID is not in the lookup table.    */
/*                                                                            */
/******************************************************************************/
void rpc_ss_update_callee_context
#ifdef IDL_PROTOTYPES
(
    rpc_ss_context_t    callee_context, /* The user's local form of the context */
    uuid_t              *p_uuid,        /* Pointer to the equivalent UUID */
    error_status_t      *result         /* Function result */
)
#else
(callee_context, p_uuid, result)
    rpc_ss_context_t    callee_context; /* The user's local form of the context */
    uuid_t              *p_uuid;        /* Pointer to the equivalent UUID */
    error_status_t      *result;        /* Function result */
#endif
{
    callee_context_entry_t *this_link;

#ifdef PERFMON
    RPC_SS_UPDATE_CALLEE_CONTEXT_N;
#endif

    RPC_SS_THREADS_MUTEX_LOCK(&rpc_ss_context_table_mutex);
    DPRINT(("Seized context tables\n"));
    this_link = &context_table[uuid_hash((uuid_p_t)p_uuid,result)
                                               % CALLEE_CONTEXT_TABLE_SIZE];
    while ( ! uuid_equal((uuid_p_t)p_uuid,(uuid_p_t)&this_link->uuid,result) )
    {
        this_link = this_link->next_context;
        if (this_link == NULL)
        {
            RPC_SS_THREADS_MUTEX_UNLOCK(&rpc_ss_context_table_mutex);
            DPRINT(("Released context tables\n"));
            RAISE( rpc_x_ss_context_mismatch);
        }
    }
    this_link->user_context = callee_context;
    RPC_SS_THREADS_MUTEX_UNLOCK(&rpc_ss_context_table_mutex);
    DPRINT(("Released context tables\n"));
    *result = error_status_ok;

#ifdef PERFMON
    RPC_SS_UPDATE_CALLEE_CONTEXT_X;
#endif

}

/******************************************************************************/
/*                                                                            */
/*    Find the local context that corresponds to a supplied UUID              */
/*                                                                            */
/*  ENTRY POINT INTO LIBIDL FROM STUB                                         */
/*                                                                            */
/******************************************************************************/
void rpc_ss_ee_ctx_from_wire
#ifdef IDL_PROTOTYPES
(
    ndr_context_handle      *p_wire_context,
    rpc_ss_context_t        *p_context,         /* The application context */
    volatile error_status_t *p_st
)
#else
(p_wire_context, p_context, p_st)
    ndr_context_handle      *p_wire_context;
    rpc_ss_context_t        *p_context;         /* The application context */
    volatile error_status_t *p_st;
#endif
{
    uuid_p_t p_uuid;    /* Pointer to the UUID that has come off the wire */
    callee_context_entry_t *this_link;

#ifdef PERFMON
    RPC_SS_EE_CTX_FROM_WIRE_N;
#endif

    p_uuid = &p_wire_context->context_handle_uuid;

#ifdef DEBUGCTX
    debug_context_lookup(p_uuid);
    debug_context_table();
#endif

    *p_st = error_status_ok;
    if ( uuid_is_nil(p_uuid, (error_status_t *)p_st) )
    {
        *p_context = NULL;

#ifdef PERFMON
        RPC_SS_EE_CTX_FROM_WIRE_X;
#endif

        return;
    }
    RPC_SS_THREADS_MUTEX_LOCK(&rpc_ss_context_table_mutex);
    DPRINT(("Seized context tables\n"));
    this_link = &context_table[uuid_hash((uuid_p_t)p_uuid, 
                     (error_status_t *)p_st) % CALLEE_CONTEXT_TABLE_SIZE];
    while ( ! uuid_equal((uuid_p_t)p_uuid,(uuid_p_t)&this_link->uuid, 
                        (error_status_t *)p_st) )
    {
        this_link = this_link->next_context;
        if (this_link == NULL)
        {
            RPC_SS_THREADS_MUTEX_UNLOCK(&rpc_ss_context_table_mutex);
            DPRINT(("Released context tables\n"));

#ifdef PERFMON
            RPC_SS_EE_CTX_FROM_WIRE_X;
#endif

            RAISE( rpc_x_ss_context_mismatch );
            return;
        }
    }
    *p_context = this_link->user_context;
    RPC_SS_THREADS_MUTEX_UNLOCK(&rpc_ss_context_table_mutex);
    DPRINT(("Released context tables\n"));

#ifdef PERFMON
    RPC_SS_EE_CTX_FROM_WIRE_X;
#endif

    return;
}

/******************************************************************************/
/*                                                                            */
/*    Take a lock on the context tables and destroy a context entry           */
/*                                                                            */
/******************************************************************************/
void rpc_ss_destroy_callee_context
#ifdef IDL_PROTOTYPES
(
    uuid_t *p_uuid,             /* Pointer to UUID of context to be destroyed */
    handle_t  h,                /* Binding handle */
    error_status_t *result /* Function result */
)    /* Returns error_status_ok unless the UUID is not in the lookup table */
#else
(p_uuid, h, result)
    uuid_t *p_uuid;             /* Pointer to UUID of context to be destroyed */
    handle_t  h;                /* Binding handle */
    error_status_t *result;/* Function result */
#endif
{
    rpc_client_handle_t close_client;   /* NULL or client to stop monitoring */

#ifdef PERFMON
    RPC_SS_DESTROY_CALLEE_CONTEXT_N;
#endif

    RPC_SS_THREADS_MUTEX_LOCK(&rpc_ss_context_table_mutex);
    DPRINT(("Seized context tables\n"));
    rpc_ss_lkddest_callee_context(p_uuid,&close_client,result);
    RPC_SS_THREADS_MUTEX_UNLOCK(&rpc_ss_context_table_mutex);
    DPRINT(("Released context tables\n"));
    if ((*result == error_status_ok) && (close_client != NULL))
    {
        rpc_network_stop_monitoring(h, close_client, (error_status_t *) result);
    }
#ifdef PERFMON
    RPC_SS_DESTROY_CALLEE_CONTEXT_X;
#endif
}

/******************************************************************************/
/*                                                                            */
/*    Destroy an entry in the context table                                   */
/*                                                                            */
/*  Assumes that the context table mutex is locked                            */
/*                                                                            */
/******************************************************************************/
void rpc_ss_lkddest_callee_context
#ifdef IDL_PROTOTYPES
(
    uuid_t *p_uuid,    /* Pointer to UUID of context to be destroyed */
    rpc_client_handle_t *p_close_client,
                                /* Ptr to NULL or client to stop monitoring */
    error_status_t *result /* Function result */
)    /* Returns error_status_ok unless the UUID is not in the lookup table */
#else
(p_uuid, p_close_client, result)
    uuid_t *p_uuid;    /* Pointer to UUID of context to be destroyed */
    rpc_client_handle_t *p_close_client;
                                /* Ptr to NULL or client to stop monitoring */
    error_status_t *result;/* Function result */
#endif
{
    callee_context_entry_t *this_link, *next_link, *last_link;

#ifdef PERFMON
    RPC_SS_LKDDEST_CALLEE_CONTEXT_N;
#endif

    this_link = &context_table[uuid_hash((uuid_p_t)p_uuid,
                       (error_status_t *) result) % CALLEE_CONTEXT_TABLE_SIZE];
    next_link = this_link->next_context;
    if ( uuid_equal((uuid_p_t)p_uuid, (uuid_p_t)&this_link->uuid, 
                   (error_status_t *) result) )
    {
        /* Context to be destroyed is in home slot */
        rpc_ss_take_from_callee_client(this_link,p_close_client,result);
        if (next_link == NULL)
        {
            /* There is no chain from the home slot */
            uuid_create_nil((uuid_p_t)&this_link->uuid, 
                            (error_status_t *) result);
        }
        else
        {
            /* Move the second item in the chain to the home slot */
            memcpy(
                (char *)&this_link->uuid,
                (char *)&next_link->uuid,
                sizeof(uuid_t)
            );
            this_link->user_context = next_link->user_context;
            this_link->rundown = next_link->rundown;
            this_link->p_client_entry = next_link->p_client_entry;
            this_link->prev_in_client = next_link->prev_in_client;
            if (this_link->prev_in_client == NULL)
            {
                (this_link->p_client_entry)->first_context = this_link;
            }
            else
            {
                (this_link->prev_in_client)->next_in_client = this_link;
            }
            this_link->next_in_client = next_link->next_in_client;
            if (this_link->next_in_client == NULL)
            {
                (this_link->p_client_entry)->last_context = this_link;
            }
            else
            {
                (this_link->next_in_client)->prev_in_client = this_link;
            }
            this_link->next_context = next_link->next_context;
            /* And release the memory it was in */
            free((char_p_t)next_link);
        }

#ifdef PERFMON
        RPC_SS_LKDDEST_CALLEE_CONTEXT_X;
#endif

        return;
    }
    else    /* Context is further down chain */
    {
        while (next_link != NULL)
        {
            last_link = this_link;
            this_link = next_link;
            next_link = this_link->next_context;
            if ( uuid_equal((uuid_p_t)p_uuid, (uuid_p_t)&this_link->uuid, 
                           (error_status_t *)result) )
            {
                rpc_ss_take_from_callee_client(this_link,p_close_client,result);
                /* Relink chain to omit found entry */
                last_link->next_context = next_link;
                /* And free the memory it occupied */
                free((char_p_t)this_link);

#ifdef PERFMON
                RPC_SS_LKDDEST_CALLEE_CONTEXT_X;
#endif

                return;
            }
        }
        RPC_SS_THREADS_MUTEX_UNLOCK(&rpc_ss_context_table_mutex);
        DPRINT(("Released context tables\n"));
        RAISE( rpc_x_ss_context_mismatch);
    }
}


#ifdef CTXEETEST
void dump_context_table()
{
    int i;
    callee_context_entry_t *this_link;
    callee_client_entry_t *this_client;
    error_status_t status;

    for (i=0; i<CALLEE_CONTEXT_TABLE_SIZE; i++)
    {
        if ( ! uuid_is_nil(&context_table[i].uuid, &status) )
        {
            this_link = &context_table[i];
            printf("Context chain for context_slot %d\n",i);
            while (this_link != NULL)
            {
                printf("\t %s %lx ",
                        &this_link->uuid,
                        this_link->user_context);
                this_client = this_link->p_client_entry;
                printf("Client %lx %d\n",
                        this_client->client,
                        this_client->count);
                this_link = this_link->next_context;
            }
        }
    }
}
#endif

#ifdef DEBUGCTX
static ndr_boolean debug_file_open = ndr_false;
static char *debug_file = "ctxee.dmp";
static FILE *debug_fid;

static int debug_context_lookup(uuid_p)
    unsigned char *uuid_p;
{
    int j;
    unsigned long k;

    if (!debug_file_open)
    {
        debug_fid = fopen(debug_file, "w");
        debug_file_open = ndr_true;
    }

    fprintf(debug_fid, "L");
    for (j=0; j<sizeof(uuid_t); j++)
    {
        k = *uuid_p++;
        fprintf(debug_fid, " %02x", k);
    }
    fprintf(debug_fid, "\n");
}

static int debug_context_add(user_context)
    long user_context;
{
    if (!debug_file_open)
    {
        debug_fid = fopen(debug_file, "w");
        debug_file_open = ndr_true;
    }

    fprintf(debug_fid, "N %lx\n", user_context);
}

static int debug_context_table()
{
    int i, j;
    unsigned long k;
    ndr_boolean at_home;
    unsigned char *uuid_p;
    callee_context_entry_t *this_link;
    callee_client_entry_t *this_client;
    error_status_t status;

    if (!debug_file_open)
    {
        debug_fid = fopen(debug_file, "w");
        debug_file_open = ndr_true;
    }

    for (i=0; i<CALLEE_CONTEXT_TABLE_SIZE; i++)
    {
        if ( ! uuid_is_nil(&context_table[i].uuid, &status) )
        {
            at_home = ndr_true;
            this_link = &context_table[i];
            while (this_link != NULL)
            {
                if (at_home)
                {
                    at_home = ndr_false;
                    fprintf(debug_fid, "%4d:", i);
                }
                else
                    fprintf(debug_fid, "   C:", i);
                uuid_p = (unsigned char *)&this_link->uuid;
                for (j=0; j<sizeof(this_link->uuid); j++)
                {
                    k = *uuid_p++;
                    fprintf(debug_fid, " %02x", k);
                }
                this_client = this_link->p_client_entry;
                fprintf(debug_fid, " client %lx %d\n",
                        this_client->client,
                        this_client->count);
                this_link = this_link->next_context;
            }
        }
    }
}
#endif

/******************************************************************************/
/*                                                                            */
/*    Routine to be called when a callee argument of type  context_t  is to   */
/*    be marshalled                                                           */
/*                                                                            */
/*  ENTRY POINT INTO LIBIDL FROM STUB                                         */
/*                                                                            */
/******************************************************************************/
void rpc_ss_ee_ctx_to_wire
#ifdef IDL_PROTOTYPES
(
    rpc_ss_context_t        callee_context,   /* The application context */
    ndr_context_handle      *p_wire_context,  /* Pointer to wire form of context */
    handle_t                h,                /* Binding handle */
    void                    (*ctx_rundown)(rpc_ss_context_t), /* Pointer to context rundown routine */
    ndr_boolean             in_out,           /* TRUE for [in,out], FALSE for [out] */
    volatile error_status_t *p_st
)
#else
(callee_context,p_wire_context,h,ctx_rundown,in_out,p_st)
    rpc_ss_context_t        callee_context;
    ndr_context_handle      *p_wire_context;
    handle_t                h;
    void                    (*ctx_rundown)(rpc_ss_context_t);
    ndr_boolean             in_out;
    error_status_t          *p_st;
#endif
{
#ifdef DEBUGCTX
    debug_context_add(callee_context);
#endif

    int wire, context;
    p_wire_context->context_handle_attributes = 0;  /* Only defined value. */

    /*  Boolean conditions are
     *      wire:    UUID from wire is valid (not nil) -- in_out must be true.
     *      context: callee context from manager is valid (not NULL).
     *      in_out:  context handle parameter is [in,out] (not just [out]).
     *  Mapping of conditions onto actions implemented in the following switch.
     *      wire  context  in_out  condition  Action
     *       0       0       0         0      create nil UUID
     *       0       0       1         1      do nothing -- use old (nil) UUID
     *       0       1       0         2      create new UUID and new context
     *       0       1       1         3      create new UUID and new context
     *       1       0       0         4      impossible
     *       1       0       1         5      destroy existing context, make UUID nil
     *       1       1       0         6      impossible
     *       1       1       1         7      update existing context
     */

#ifdef PERFMON
    RPC_SS_EE_CTX_TO_WIRE_N;
#endif

    wire    = in_out && !uuid_is_nil(
                &p_wire_context->context_handle_uuid, (error_status_t *)p_st
            )? 4: 0;
    context = callee_context? 2: 0;
    in_out  = in_out? 1: 0;

    switch (wire | context | in_out) {
    case 0:
        uuid_create_nil(
            &p_wire_context->context_handle_uuid, (error_status_t *)p_st
        );
        break;
    case 1:
        *p_st = error_status_ok;
        break;
    case 2:
    case 3:
        uuid_create(
            &p_wire_context->context_handle_uuid, (error_status_t *)p_st
        );
        rpc_ss_create_callee_context(
            callee_context, (uuid_t *)&p_wire_context->context_handle_uuid, h,
            ctx_rundown, (error_status_t *)p_st
        );
        break;
    case 5:
        rpc_ss_destroy_callee_context(
            (uuid_t *)&p_wire_context->context_handle_uuid, h, 
            (error_status_t *)p_st
        );
        if (*p_st != error_status_ok) break;
        uuid_create_nil(
            &p_wire_context->context_handle_uuid, (error_status_t *)p_st
        );
        break;
    case 7:
        rpc_ss_update_callee_context(
            callee_context, (uuid_t *)&p_wire_context->context_handle_uuid,
            (error_status_t *)p_st
        );
        break;
    default:    /* All invalid conditions evaluate true. */
        assert(!(wire | context | in_out));
        break;
    }

#ifdef DEBUGCTX
    debug_context_table();
#endif

#ifdef PERFMON
    RPC_SS_EE_CTX_TO_WIRE_X;
#endif

}
