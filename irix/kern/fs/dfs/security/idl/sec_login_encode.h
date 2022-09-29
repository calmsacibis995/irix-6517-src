/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994, 1996 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/*
 * HISTORY
 * $Log: sec_login_encode.h,v $
 * Revision 65.2  1999/02/03 22:44:23  mek
 * Convert from IDL to C style include format.
 *
 * Revision 65.1  1997/10/20 19:46:40  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.4.2  1996/03/09  23:29:17  marty
 * 	Update OSF copyright year
 * 	[1996/03/09  22:44:40  marty]
 *
 * Revision 1.1.4.1  1995/12/08  17:37:59  root
 * 	Submit OSF/DCE 1.2.1
 * 
 * 	HP revision /main/HPDCE02/1  1995/04/03  22:08 UTC  mullan_s
 * 	Binary Compatibility Merge
 * 
 * 	HP revision /main/mullan_mothra_bin_compat/mullan_mothra_bin_compat2/3  1995/03/20  14:54 UTC  mullan_s
 * 	Add gen_v1_pac flag to v1_1_info.
 * 
 * 	HP revision /main/mullan_mothra_bin_compat/mullan_mothra_bin_compat2/2  1995/03/08  19:57 UTC  mullan_s
 * 	Change a few field names in db_authdata.
 * 
 * 	HP revision /main/mullan_mothra_bin_compat/mullan_mothra_bin_compat2/1  1995/01/26  21:03 UTC  mullan_s
 * 	Add new db type for storing EPAC.
 * 
 * 	HP revision /main/mullan_mothra_bin_compat/3  1995/01/11  18:17 UTC  mullan_s
 * 	Remove definition of sec__login_v1_1_info_encode.
 * 
 * 	HP revision /main/mullan_mothra_bin_compat/1  1995/01/04  15:40 UTC  mullan_s
 * 	Binary Compatibility Work.
 * 	[1995/12/08  16:56:11  root]
 * 
 * Revision 1.1.2.4  1994/09/16  21:51:42  sommerfeld
 * 	[OT11915] add pag to info.
 * 	[1994/09/16  21:48:43  sommerfeld]
 * 
 * Revision 1.1.2.3  1994/06/17  18:42:34  devsrc
 * 	cr10872 - fix copyright
 * 	[1994/06/17  18:10:39  devsrc]
 * 
 * Revision 1.1.2.2  1994/06/10  15:07:18  greg
 * 	intercell fixes for DCE1.1 beta
 * 	[1994/06/03  20:49:18  greg]
 * 
 * Revision 1.1.2.1  1994/05/11  19:14:06  ahop
 * 	hp_sec_to_osf_2 drop
 * 	internal login context IDL encoding interfaces
 * 	[1994/04/29  21:09:30  ahop]
 * 
 * $EndLog$
 */
/* Generated by IDL compiler version OSF DCE T1.2.0-09 */
#ifndef sec_login_encode_v1_0_included
#define sec_login_encode_v1_0_included
#ifndef IDLBASE_H
#include <dce/idlbase.h>
#endif
#include <dce/rpc.h>
#include <dce/idl_es.h>

#ifdef __cplusplus
    extern "C" {
#endif

#ifndef nbase_v0_0_included
#include <dce/nbase.h>
#endif
#ifndef sec_id_epac_base_v0_0_included
#include <dce/id_epac.h>
#endif
#ifndef prvnbase_v0_0_included
#include <dce/prvnbase.h>
#endif
#ifndef sec_login_base_v0_0_included
#include <dce/sec_login_base.h>
#endif
typedef unsigned32 sec__login_dlg_req_type_t;
#define sec__login_c_dlg_req_none (0)
#define sec__login_c_dlg_req_init (1)
#define sec__login_c_dlg_req_traced (2)
#define sec__login_c_dlg_req_imp (4)
typedef struct  {
sec_bytes_t dlg_chain;
sec_dlg_token_set_t dlg_token_set;
sec_id_delegation_type_t dlg_type;
sec_id_restriction_set_t dlg_rstrs;
sec_id_restriction_set_t tgt_rstrs;
sec_id_opt_req_t opt_rstrs;
sec_id_opt_req_t req_rstrs;
} sec__login_dlg_req_info_t;
typedef struct  {
sec_bytes_t epac_chain;
sec_id_compatibility_mode_t compat_mode;
sec__login_dlg_req_type_t dlg_req_type;
union  {
/* case(s): 0, 1 */
/* Empty arm */
/* case(s): 2, 4 */
sec__login_dlg_req_info_t info;
} dlg_req_info;
unsigned32 pag;
boolean32 gen_v1_pac;
} sec_login__v1_1_info_t;
typedef struct  {
idl_char *cell;
signed32 endtime;
sec_bytes_t epac_chain;
sec_bytes_t authbytes;
} sec_db_authdata_contents_t;
typedef struct  {
unsigned32 num_ad;
sec_db_authdata_contents_t ad[1];
} sec_db_authdata_t;
typedef enum {generation_id_enum=0,
v1_1_info_enum=1,
epac_enum=2} sec_login_db_enum_t;
typedef struct  {
sec_login_db_enum_t entry_type;
union  {
/* case(s): 0 */
uuid_t generation_id;
/* case(s): 1 */
sec_login__v1_1_info_t v1_1_info;
/* case(s): 2 */
sec_db_authdata_t *v1_authdata;
} contents;
} sec_login_db_entry_t;
extern void sec_login_db_entry_encode(
#ifdef IDL_PROTOTYPES
    /* [in] */ idl_es_handle_t h,
    /* [in, out] */ sec_login_db_entry_t *entry
#endif
);
typedef struct sec_login_encode_v1_0_epv_t {
void (*sec_login_db_entry_encode)(
#ifdef IDL_PROTOTYPES
    /* [in] */ idl_es_handle_t h,
    /* [in, out] */ sec_login_db_entry_t *entry
#endif
);
} sec_login_encode_v1_0_epv_t;
extern rpc_if_handle_t sec_login_encode_v1_0_c_ifspec;
extern rpc_if_handle_t sec_login_encode_v1_0_s_ifspec;

#ifdef __cplusplus
    }

#else
#endif
#endif
