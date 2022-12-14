/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994, 1996 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/*
 * HISTORY
 * $Log: id_epac.h,v $
 * Revision 65.2  1999/02/03 22:44:20  mek
 * Convert from IDL to C style include format.
 *
 * Revision 65.1  1997/10/20 19:46:30  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.4.2  1996/03/09  23:27:55  marty
 * 	Update OSF copyright year
 * 	[1996/03/09  22:43:23  marty]
 *
 * Revision 1.1.4.1  1995/12/08  17:31:32  root
 * 	Submit OSF/DCE 1.2.1
 * 	[1995/12/08  16:54:57  root]
 * 
 * Revision 1.1.2.4  1994/08/04  16:12:55  mdf
 * 	Hewlett Packard Security Code Drop
 * 	[1994/07/26  19:09:45  mdf]
 * 
 * Revision 1.1.2.3  1994/06/17  18:42:19  devsrc
 * 	cr10872 - fix copyright
 * 	[1994/06/17  18:08:58  devsrc]
 * 
 * Revision 1.1.2.2  1994/05/11  19:10:42  ahop
 * 	hp_sec_to_osf_2 drop
 * 	[1994/04/29  21:05:08  ahop]
 * 
 * Revision 1.1.2.1  1994/01/28  23:10:47  burati
 * 	Initial version (dlg_bl1)
 * 	[1994/01/18  20:52:37  burati]
 * 
 * $EndLog$
 */
/* Generated by IDL compiler version OSF DCE T1.2.0-09 */
#ifndef sec_id_epac_base_v0_0_included
#define sec_id_epac_base_v0_0_included
#ifndef IDLBASE_H
#include <dce/idlbase.h>
#endif

#ifdef __cplusplus
    extern "C" {
#endif

#ifndef nbase_v0_0_included
#include <dce/nbase.h>
#endif
#ifndef sec_base_v0_0_included
#include <dce/sec_base.h>
#endif
#ifndef sec_id_base_v0_0_included
#include <dce/id_base.h>
#endif
#ifndef sec_attr_base_v0_0_included
#include <dce/sec_attr_base.h>
#endif
typedef struct  {
unsigned16 restriction_len;
idl_byte *restrictions;
} sec_id_opt_req_t;
typedef enum {sec_rstr_e_type_user=0,
sec_rstr_e_type_group=1,
sec_rstr_e_type_foreign_user=2,
sec_rstr_e_type_foreign_group=3,
sec_rstr_e_type_foreign_other=4,
sec_rstr_e_type_any_other=5,
sec_rstr_e_type_no_other=6} sec_rstr_entry_type_t;
typedef struct  {
struct sec_id_entry_u {
sec_rstr_entry_type_t entry_type;
union  {
/* case(s): 5, 6 */
/* Empty arm */
/* case(s): 0, 1, 4 */
sec_id_t id;
/* case(s): 2, 3 */
sec_id_foreign_t foreign_id;
} tagged_union;
} entry_info;
} sec_id_restriction_t;
typedef struct  {
unsigned16 num_restrictions;
sec_id_restriction_t *restrictions;
} sec_id_restriction_set_t;
typedef unsigned16 sec_id_compatibility_mode_t;
#define sec_id_compat_mode_none (0)
#define sec_id_compat_mode_initiator (1)
#define sec_id_compat_mode_caller (2)
typedef unsigned16 sec_id_delegation_type_t;
#define sec_id_deleg_type_none (0)
#define sec_id_deleg_type_traced (1)
#define sec_id_deleg_type_impersonation (2)
typedef unsigned16 sec_id_seal_type_t;
#define sec_id_seal_type_none (0)
#define sec_id_seal_type_md5_des (1)
#define sec_id_seal_type_md5 (2)
typedef struct  {
sec_id_seal_type_t seal_type;
unsigned16 seal_len;
idl_byte *seal_data;
} sec_id_seal_t;
typedef struct  {
sec_id_t realm;
sec_id_t principal;
sec_id_t group;
unsigned16 num_groups;
sec_id_t *groups;
unsigned16 num_foreign_groupsets;
sec_id_foreign_groupset_t *foreign_groupsets;
} sec_id_pa_t;
typedef idl_void_p_t sec_cred_pa_handle_t;
typedef idl_void_p_t sec_cred_cursor_t;
typedef idl_void_p_t sec_cred_attr_cursor_t;
typedef struct  {
sec_id_pa_t pa;
sec_id_compatibility_mode_t compat_mode;
sec_id_delegation_type_t deleg_type;
sec_id_opt_req_t opt_restrictions;
sec_id_opt_req_t req_restrictions;
unsigned32 num_attrs;
sec_attr_t *attrs;
sec_id_restriction_set_t deleg_restrictions;
sec_id_restriction_set_t target_restrictions;
} sec_id_epac_data_t;
typedef struct  {
unsigned32 num_seals;
sec_id_seal_t *seals;
} sec_id_seal_set_t;
typedef sec_id_seal_set_t *sec_id_seal_set_p_t;
typedef struct  {
sec_bytes_t pickled_epac_data;
sec_id_seal_set_t *seals;
} sec_id_epac_t;
typedef struct  {
unsigned32 num_epacs;
sec_id_epac_t *epacs;
} sec_id_epac_set_t;

#ifdef __cplusplus
    }

#endif
#endif
