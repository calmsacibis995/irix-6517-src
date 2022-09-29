/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1996 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: comtwrref.h,v $
 * Revision 65.1  1997/10/20 19:16:51  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.315.2  1996/02/18  22:55:58  marty
 * 	Update OSF copyright years
 * 	[1996/02/18  22:15:08  marty]
 *
 * Revision 1.1.315.1  1995/12/08  00:18:55  root
 * 	Submit OSF/DCE 1.2.1
 * 
 * 	HP revision /main/HPDCE02/jrr_1.2_mothra/1  1995/11/16  21:31 UTC  jrr
 * 	Remove Mothra specific code
 * 
 * 	HP revision /main/HPDCE02/1  1994/12/09  19:18 UTC  tatsu_s
 * 	Submitted rfc31.0: Single-threaded DG client and RPC_PREFERRED_PROTSEQ.
 * 
 * 	HP revision /main/tatsu_s.st_dg.b0/1  1994/12/05  18:53 UTC  tatsu_s
 * 	RPC_PREFERRED_PROTSEQ: Initial version.
 * 	[1995/12/07  23:58:30  root]
 * 
 * Revision 1.1.313.1  1994/01/21  22:36:08  cbrooks
 * 	RPC Code Cleanyp - Initial Submission
 * 	[1994/01/21  21:56:15  cbrooks]
 * 
 * Revision 1.1.2.3  1993/01/03  23:23:09  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  20:03:32  bbelch]
 * 
 * Revision 1.1.2.2  1992/12/23  20:46:00  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  15:35:17  zeliff]
 * 
 * Revision 1.1  1992/01/19  03:12:49  devrcs
 * 	Initial revision
 * 
 * $EndLog$
 */
#ifndef _COMTWRREF_H
#define _COMTWRREF_H
/*
**  Copyright (c) 1990 by
**  
**      Hewlett-Packard Company, Palo Alto, Ca. &
**      Digital Equipment Corporation, Maynard, Mass.
**
**
**  NAME:
**
**      nsrttwr.h
**
**  FACILITY:
**
**      Remote Procedure Call (RPC)
**  
**  ABSTRACT:
**
**      Header file containing macros, definitions, typedefs and prototypes of
**      exported routines from the nsrttwr.c module.
**  
**
**/

#ifndef _DCE_PROTOTYPE_
#include <dce/dce.h>
#endif


/*
 * Type Definitions
 */

/*
 * Protocol Sequence ID Table
 * 
 * This table contains the valid combination of protocol ids
 * for upper floor 3 and the lower tower floors.
 * This table maps each combination to the appropriate 
 * RPC protocol id sequence.  
 * The field num_floors provides for the number of significant
 * floors comprising the RPC protocol sequence.
 */
typedef struct
{
    unsigned8               prefix;
    uuid_t                  uuid;
} rpc_flr_prot_id_t, *rpc_flr_prot_id_p_t;

typedef struct
{
    rpc_protseq_id_t        rpc_protseq_id;
    unsigned8               num_floors;
    rpc_flr_prot_id_t       floor_prot_ids[RPC_C_MAX_NUM_NETWORK_FLOORS + 1];
} rpc_tower_prot_ids_t, *rpc_tower_prot_ids_p_t;

/*
 * Prototypes
 */

#ifdef __cplusplus
extern "C" {
#endif


PRIVATE void rpc__tower_ref_add_floor _DCE_PROTOTYPE_ ((
    unsigned32           /*floor_number*/,
    rpc_tower_floor_p_t  /*floor*/,
    rpc_tower_ref_t     * /*tower_ref*/,
    unsigned32          * /*status*/ 
));

PRIVATE void rpc__tower_ref_alloc _DCE_PROTOTYPE_ ((
    unsigned8           * /*tower_octet_string*/,
    unsigned32           /*num_flrs*/,
    unsigned32           /*start_flr*/,
    rpc_tower_ref_p_t   * /*tower_ref*/,
    unsigned32          * /*status*/ 
));

PRIVATE void rpc__tower_ref_copy _DCE_PROTOTYPE_ ((
    rpc_tower_ref_p_t    /*source_tower*/,
    rpc_tower_ref_p_t   * /*dest_tower*/,
    unsigned32          * /*status*/ 
));

PRIVATE void rpc__tower_ref_free _DCE_PROTOTYPE_ ((
    rpc_tower_ref_p_t       * /*tower_ref*/,
    unsigned32              * /*status*/ 
));

PRIVATE void rpc__tower_ref_inq_protseq_id _DCE_PROTOTYPE_ ((
    rpc_tower_ref_p_t    /*tower_ref*/,
    rpc_protseq_id_t    * /*protseq_id*/,
    unsigned32          * /*status*/ 
));

PRIVATE boolean rpc__tower_ref_is_compatible _DCE_PROTOTYPE_ ((
    rpc_if_rep_p_t           /*if_spec*/,
    rpc_tower_ref_p_t        /*tower_ref*/,
    unsigned32              * /*status*/ 
));

PRIVATE void rpc__tower_ref_vec_free _DCE_PROTOTYPE_ ((
    rpc_tower_ref_vector_p_t    * /*tower_vector*/,
    unsigned32                  * /*status*/ 
));

PRIVATE void rpc__tower_ref_vec_from_binding _DCE_PROTOTYPE_ ((
    rpc_if_rep_p_t               /*if_spec*/,
    rpc_binding_handle_t         /*binding*/,
    rpc_tower_ref_vector_p_t    * /*tower_vector*/,
    unsigned32                  * /*status*/ 
));

#endif /* _COMTWRREF_H */

