/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1996 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: prvnbase.h,v $
 * Revision 65.2  1999/02/03 22:44:21  mek
 * Convert from IDL to C style include format.
 *
 * Revision 65.1  1997/10/20 19:46:32  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.6.2  1996/03/09  23:28:07  marty
 * 	Update OSF copyright year
 * 	[1996/03/09  22:43:35  marty]
 *
 * Revision 1.1.6.1  1995/12/08  17:32:32  root
 * 	Submit OSF/DCE 1.2.1
 * 	[1995/12/08  16:55:08  root]
 * 
 * Revision 1.1.4.2  1994/08/18  20:25:29  greg
 * 	Add delegation token hooks for "confidential bytes."  They
 * 	aren't implemented in 1.1, but at least implementing them
 * 	later won't require a protocol change.
 * 	[1994/08/16  19:12:49  greg]
 * 
 * Revision 1.1.4.1  1994/05/11  19:10:57  ahop
 * 	hp_sec_to_osf_2 drop
 * 	[1994/04/29  21:05:36  ahop]
 * 
 * Revision 1.1.2.2  1992/12/29  13:30:09  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/28  20:17:09  zeliff]
 * 
 * Revision 1.1  1992/01/19  14:54:56  devrcs
 * 	Initial revision
 * 
 * $EndLog$
 */
/* Generated by IDL compiler version OSF DCE T1.2.0-09 */
#ifndef prvnbase_v0_0_included
#define prvnbase_v0_0_included
#ifndef IDLBASE_H
#include <dce/idlbase.h>
#endif

#ifdef __cplusplus
    extern "C" {
#endif

#ifndef nbase_v0_0_included
#include <dce/nbase.h>
#endif
#ifndef secsts_v0_0_included
#include <dce/secsts.h>
#endif
#ifndef sec_attr_base_v0_0_included
#include <dce/sec_attr_base.h>
#endif
#ifndef sec_id_epac_base_v0_0_included
#include <dce/id_epac.h>
#endif
#ifndef rgynbase_v0_0_included
#include <dce/rgynbase.h>
#endif
typedef struct  {
unsigned32 num_bytes;
idl_byte bytes[1];
} rpriv_pickle_t;
typedef struct  {
unsigned32 num_aux_attr_keys;
sec_attr_t *aux_attr_keys;
} rpriv_attr_request_t;
typedef struct  {
error_status_t status;
unsigned32 num_attrs;
sec_attr_t *attrs;
} rpriv_attr_result_t;
typedef struct  {
error_status_t status;
rpriv_pickle_t *app_tkt_rep;
} rpriv_app_tkt_result_t;
#define sec_dlg_token_v0_conf_bytes (1)
typedef struct  {
unsigned32 expiration;
sec_bytes_t token_bytes;
} sec_dlg_token_t;
typedef struct  {
unsigned32 num_tokens;
sec_dlg_token_t *tokens;
} sec_dlg_token_set_t;

#ifdef __cplusplus
    }

#endif
#endif
