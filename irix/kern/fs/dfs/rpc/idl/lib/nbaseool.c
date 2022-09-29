/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(_KERNEL)
static const char *_identString = "$Id: nbaseool.c,v 65.4 1998/03/23 17:25:19 gwehrman Exp $";
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
 * $Log: nbaseool.c,v $
 * Revision 65.4  1998/03/23 17:25:19  gwehrman
 * Source Identification changed to bracket with SGIMIPS && !_KERNEL.
 *
 * Revision 65.3  1998/01/07 17:20:00  jdoak
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS
 *
 * Revision 65.2  1997/11/06  19:56:08  jdoak
 * Source Identification added.
 *
 * Revision 65.1  1997/10/20  19:15:37  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.8.2  1996/02/18  18:54:35  marty
 * 	Update OSF copyright years
 * 	[1996/02/18  18:07:01  marty]
 *
 * Revision 1.1.8.1  1995/12/07  22:29:05  root
 * 	Submit OSF/DCE 1.2.1
 * 	[1995/12/07  21:15:32  root]
 * 
 * Revision 1.1.4.2  1993/07/07  20:08:24  ganni
 * 	reduced stub idl sources
 * 	[1993/07/07  19:37:56  ganni]
 * 
 * $EndLog$
 */
/*
**
**  Copyright (c) 1990 by
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
**
**  NAME:
**
**      nbaseool.c
**
**  FACILITY:
**
**      IDL Stub Runtime Support
**
**  ABSTRACT:
**
**      Support routines to un/marshall uuid_t
**
**  VERSION: DCE 1.0
*/

#define IDL_CAUX
#define IDL_SAUX

/* The ordering of the following 3 includes should NOT be changed! */
#include <dce/rpc.h>
#include <dce/stubbase.h>
#include <lsysdep.h>

#ifdef PERFMON
#include <dce/idl_log.h>
#endif

void rpc_ss_m_uuid
#ifdef IDL_PROTOTYPES
(
uuid_t *p_node,
rpc_ss_marsh_state_t *NIDL_msp
)
#else
( p_node, NIDL_msp )
uuid_t *p_node;
rpc_ss_marsh_state_t *NIDL_msp;
#endif
{
  
  /* local variables */
  idl_byte *IDL_element_0;
  unsigned long space_for_node;
  rpc_mp_t mp;
  rpc_op_t op;
  
#ifdef PERFMON
    RPC_SS_M_UUID_N;
#endif

  space_for_node=((4)+(2)+(2)+(1)+(1)+(0)+(6*1))+7;
  if (space_for_node > NIDL_msp->space_in_buff)
  {
    rpc_ss_marsh_change_buff(NIDL_msp,space_for_node);
  }
  mp = NIDL_msp->mp;
  op = NIDL_msp->op;
  rpc_align_mop(mp, op, 4);
  rpc_marshall_ulong_int(mp, (*p_node).time_low);
  rpc_advance_mp(mp, 4);
  rpc_marshall_ushort_int(mp, (*p_node).time_mid);
  rpc_advance_mp(mp, 2);
  rpc_marshall_ushort_int(mp, (*p_node).time_hi_and_version);
  rpc_advance_mp(mp, 2);
  rpc_marshall_usmall_int(mp, (*p_node).clock_seq_hi_and_reserved);
  rpc_advance_mp(mp, 1);
  rpc_marshall_usmall_int(mp, (*p_node).clock_seq_low);
  rpc_advance_mp(mp, 1);
  
#ifdef PACKED_BYTE_ARRAYS
  memcpy((char *)mp, (char *)&(*p_node).node[0], (5-0+1)*1);
  rpc_advance_mp(mp, (5-0+1)*1);
#else
  IDL_element_0 = &(*p_node).node[0];
  rpc_marshall_byte(mp, IDL_element_0[0]); rpc_advance_mp(mp,1);
  rpc_marshall_byte(mp, IDL_element_0[1]); rpc_advance_mp(mp,1);
  rpc_marshall_byte(mp, IDL_element_0[2]); rpc_advance_mp(mp,1);
  rpc_marshall_byte(mp, IDL_element_0[3]); rpc_advance_mp(mp,1);
  rpc_marshall_byte(mp, IDL_element_0[4]); rpc_advance_mp(mp,1);
  rpc_marshall_byte(mp, IDL_element_0[5]); rpc_advance_mp(mp,1);
#endif
  rpc_advance_op(op, 16);
  NIDL_msp->space_in_buff -= (op - NIDL_msp->op);
  NIDL_msp->mp = mp;
  NIDL_msp->op = op;

#ifdef PERFMON
    RPC_SS_M_UUID_X;
#endif

}

void rpc_ss_u_uuid
#ifdef IDL_PROTOTYPES
(
uuid_t *p_node,
rpc_ss_marsh_state_t *p_unmar_params
)
#else
( p_node, p_unmar_params )
uuid_t *p_node;
rpc_ss_marsh_state_t *p_unmar_params;
#endif
{
  
  /* local variables */
  idl_byte *IDL_element_1;

#ifdef PERFMON
    RPC_SS_U_UUID_N;
#endif

  rpc_align_mop(p_unmar_params->mp, p_unmar_params->op, 4);
  if ((byte_p_t)p_unmar_params->mp - p_unmar_params->p_rcvd_data->data_addr >= 
p_unmar_params->p_rcvd_data->data_len)
  {
    rpc_ss_new_recv_buff(p_unmar_params->p_rcvd_data, p_unmar_params->call_h, 
&(p_unmar_params->mp), &(*p_unmar_params->p_st));
  }
  rpc_convert_ulong_int(p_unmar_params->src_drep, ndr_g_local_drep, 
p_unmar_params->mp, (*p_node).time_low);
  rpc_advance_mp(p_unmar_params->mp, 4);

  if ((byte_p_t)p_unmar_params->mp - p_unmar_params->p_rcvd_data->data_addr >= 
p_unmar_params->p_rcvd_data->data_len)
  {
    rpc_ss_new_recv_buff(p_unmar_params->p_rcvd_data, p_unmar_params->call_h, 
&(p_unmar_params->mp), &(*p_unmar_params->p_st));
  }
  rpc_convert_ushort_int(p_unmar_params->src_drep, ndr_g_local_drep, 
p_unmar_params->mp, (*p_node).time_mid);
  rpc_advance_mp(p_unmar_params->mp, 2);
  rpc_convert_ushort_int(p_unmar_params->src_drep, ndr_g_local_drep, 
p_unmar_params->mp, (*p_node).time_hi_and_version);
  rpc_advance_mp(p_unmar_params->mp, 2);

  if ((byte_p_t)p_unmar_params->mp - p_unmar_params->p_rcvd_data->data_addr >= 
p_unmar_params->p_rcvd_data->data_len)
  {
    rpc_ss_new_recv_buff(p_unmar_params->p_rcvd_data, p_unmar_params->call_h, 
&(p_unmar_params->mp), &(*p_unmar_params->p_st));
  }
  rpc_convert_usmall_int(p_unmar_params->src_drep, ndr_g_local_drep, 
p_unmar_params->mp, (*p_node).clock_seq_hi_and_reserved);
  rpc_advance_mp(p_unmar_params->mp, 1);
  rpc_convert_usmall_int(p_unmar_params->src_drep, ndr_g_local_drep, 
p_unmar_params->mp, (*p_node).clock_seq_low);
  rpc_advance_mp(p_unmar_params->mp, 1);
  IDL_element_1 = &(*p_node).node[0];
  rpc_convert_byte(p_unmar_params->src_drep, ndr_g_local_drep, 
p_unmar_params->mp, IDL_element_1[0]);
  rpc_advance_mp(p_unmar_params->mp, 1);
  rpc_convert_byte(p_unmar_params->src_drep, ndr_g_local_drep, 
p_unmar_params->mp, IDL_element_1[1]);
  rpc_advance_mp(p_unmar_params->mp, 1);

  if ((byte_p_t)p_unmar_params->mp - p_unmar_params->p_rcvd_data->data_addr >= 
p_unmar_params->p_rcvd_data->data_len)
  {
    rpc_ss_new_recv_buff(p_unmar_params->p_rcvd_data, p_unmar_params->call_h, 
&(p_unmar_params->mp), &(*p_unmar_params->p_st));
  }
  rpc_convert_byte(p_unmar_params->src_drep, ndr_g_local_drep, 
p_unmar_params->mp, IDL_element_1[2]);
  rpc_advance_mp(p_unmar_params->mp, 1);
  rpc_convert_byte(p_unmar_params->src_drep, ndr_g_local_drep, 
p_unmar_params->mp, IDL_element_1[3]);
  rpc_advance_mp(p_unmar_params->mp, 1);
  rpc_convert_byte(p_unmar_params->src_drep, ndr_g_local_drep, 
p_unmar_params->mp, IDL_element_1[4]);
  rpc_advance_mp(p_unmar_params->mp, 1);
  rpc_convert_byte(p_unmar_params->src_drep, ndr_g_local_drep, 
p_unmar_params->mp, IDL_element_1[5]);
  rpc_advance_mp(p_unmar_params->mp, 1);

  rpc_advance_op(p_unmar_params->op, 16);

#ifdef PERFMON
    RPC_SS_U_UUID_X;
#endif

}
