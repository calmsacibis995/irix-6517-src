/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1996 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: ctxeertl.h,v $
 * Revision 65.1  1997/10/20 19:15:31  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.12.2  1996/02/18  23:45:42  marty
 * 	Update OSF copyright years
 * 	[1996/02/18  22:44:32  marty]
 *
 * Revision 1.1.12.1  1995/12/07  22:24:26  root
 * 	Submit OSF/DCE 1.2.1
 * 
 * 	HP revision /main/dat_xidl2/1  1995/11/17  17:06 UTC  dat
 * 	Merge second XIDL drop for DCE 1.2.1
 * 	[1995/12/07  21:13:52  root]
 * 
 * Revision 1.1.2.1  1995/10/23  01:48:58  bfc
 * 	oct 95 idl drop
 * 	[1995/10/22  23:35:49  bfc]
 * 
 * 	may 95 idl drop
 * 	[1995/10/22  22:56:53  bfc]
 * 
 * 	DCE for DEC OSF/1: populate from OSF DCE R1.1
 * 	[1995/10/21  18:25:18  bfc]
 * 
 * Revision 1.1.8.2  1994/07/28  20:56:05  tom
 * 	Bug 9689 - use ctx_rundown_fn_p_t (from stubbase.h).
 * 	[1994/07/28  20:55:46  tom]
 * 
 * Revision 1.1.8.1  1994/01/21  22:30:33  cbrooks
 * 	RPC Code Cleanup - Initial Submission
 * 	[1994/01/21  21:51:18  cbrooks]
 * 
 * Revision 1.1.6.2  1993/07/07  20:04:54  ganni
 * 	reduced stub idl sources
 * 	[1993/07/07  19:35:33  ganni]
 * 
 * $EndLog$
 */
/*
**  @OSF_COPYRIGHT@
**
**  Copyright (c) 1990, 1991, 1992 by
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
**      ctxeertl.h
**
**  FACILITY:
**
**      IDL Stub Runtime Support
**
**  ABSTRACT:
**
**      Definitions for callee stub's tables of contexts and clients
**
**  VERSION: DCE 1.0
**
**  MODIFICATION HISTORY:
**
**  04-Mar-92 A.I.Hinxman   Prevent context rundown while manager active
**
*/

/*  The structure of the callee client lookup table
**  is overflow hash with double chaining.
*/
typedef struct callee_client_entry_t {
    rpc_client_handle_t client;
    long int count;    /* Number of contexts for this client */
    struct callee_context_entry_t *first_context; /* First in chain of
                                                     contexts for this client */
    struct callee_context_entry_t *last_context;  /* Last in chain of
                                                     contexts for this client */
    struct callee_client_entry_t *prev_h_client;    /* Previous in chain of
                                                clients with same hash value */
    struct callee_client_entry_t *next_h_client;    /* Next in chain of
                                                clients with same hash value */
    long int ref_count;  /* The number of threads currently using contexts
                                of this client */
    RPC_SS_THREADS_CONDITION_T cond_var;  /* Used to signal when ref_count
                                                has been decremented */
    idl_boolean rundown_pending;    /* TRUE if a rundown request for this
                                        client has been received */
} callee_client_entry_t;

/*  The structure of the callee context lookup table
**  is overflow hash with chaining.
*/
typedef struct callee_context_entry_t {
    uuid_t uuid;
    rpc_ss_context_t user_context;
    void (*rundown)(rpc_ss_context_t); /* Ptr to rundown routine for context */
    callee_client_entry_t *p_client_entry;  /* Client this context belongs to */
    struct callee_context_entry_t *prev_in_client;  /* Previous in chain of
                                                        contexts for client */
    struct callee_context_entry_t *next_in_client;  /* Next in chain of
                                                        contexts for client */
    struct callee_context_entry_t *next_context;    /* Next in chain of
                                            contexts with the same hash value */
} callee_context_entry_t;

/**************** Function prototypes *******************************/

void rpc_ss_rundown_client
(
#ifdef IDL_PROTOTYPES
    rpc_client_handle_t failed_client
#endif
);

void rpc_ss_add_to_callee_client
(
#ifdef IDL_PROTOTYPES
    rpc_client_handle_t ctx_client,     /* Client for whom there is another context */
    callee_context_entry_t *p_context,  /* Pointer to the context */
    ndr_boolean *p_is_new_client,       /* Pointer to TRUE if new client */
    error_status_t *result         /* Function result */
#endif
);

void rpc_ss_take_from_callee_client
(
#ifdef IDL_PROTOTYPES
    callee_context_entry_t *p_context,  /* Pointer to the context */
    rpc_client_handle_t *p_close_client,
                                  /* Ptr to NULL or client to stop monitoring */
    error_status_t *result         /* Function result */
#endif
);


void rpc_ss_lkddest_callee_context
(
#ifdef IDL_PROTOTYPES
    uuid_t *p_uuid,    /* Pointer to UUID of context to be destroyed */
    rpc_client_handle_t *p_close_client,
                         /* Ptr to NULL or client to stop monitoring */
    error_status_t *result         /* Function result */
#endif
);    /* Returns SUCCESS unless the UUID is not in the lookup table */

void rpc_ss_init_callee_client_table(
#ifdef IDL_PROTOTYPES
    void
#endif
);

void rpc_ss_create_callee_context
(
#ifdef IDL_PROTOTYPES
    rpc_ss_context_t callee_context,       /* user's local form of the ctx */
    uuid_t    *p_uuid,                     /* pointer to the equivalent UUID */
    handle_t  h,                           /* Binding handle */
    void (*ctx_rundown)(rpc_ss_context_t), /* ptr to context rundown routine */
    error_status_t *result                 /* Function result */
#endif
);

/* Returns status_ok unless the UUID is not in the lookup table */
void rpc_ss_update_callee_context
(
#ifdef IDL_PROTOTYPES
    rpc_ss_context_t callee_context, /* user's local form of the context */
    uuid_t    *p_uuid,               /* pointer to the equivalent UUID */
    error_status_t *result      /* Function result */
#endif
);

/* Returns status_ok unless the UUID is not in the lookup table */
void rpc_ss_destroy_callee_context
(
#ifdef IDL_PROTOTYPES
    uuid_t *p_uuid,          /* pointer to UUID of context to be destroyed */
    handle_t  h,                    /* Binding handle */
    error_status_t *result     /* Function result */
#endif
);
