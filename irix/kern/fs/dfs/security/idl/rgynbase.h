/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1996 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: rgynbase.h,v $
 * Revision 65.3  1999/02/03 22:44:21  mek
 * Convert from IDL to C style include format.
 *
 * Revision 65.2  1997/10/27 19:54:28  jdoak
 * 6.4 + 1.2.2 merge
 *
# Revision 65.1  1997/10/20  19:46:33  jdoak
# *** empty log message ***
#
 * Revision 1.1.12.1  1996/06/04  21:59:31  arvind
 * 	merge u2u addition of sec_rgy_acct_auth_user_to_user from mb_u2u
 * 	[1996/05/06  20:11 UTC  burati  /main/DCE_1.2/1]
 *
 * 	merge u2u work
 * 	[1996/04/29  19:09 UTC  burati  /main/mb_u2u/1]
 *
 * 	,
 * 	u2u work off Mothra, before 1.2 branch is available
 * 	[1996/04/29  19:05 UTC  burati  /main/mb_mothra8/1]
 *
 * Revision 1.1.10.2  1996/03/09  23:28:14  marty
 * 	Update OSF copyright year
 * 	[1996/03/09  22:43:42  marty]
 * 
 * Revision 1.1.10.1  1995/12/08  17:33:05  root
 * 	Submit OSF/DCE 1.2.1
 * 	[1995/12/08  16:55:15  root]
 * 
 * Revision 1.1.7.2  1994/08/29  15:51:33  hondo
 * 	OT 11919 --login activity idl change to support different address types
 * 	[1994/08/28  21:48:57  hondo]
 * 
 * Revision 1.1.7.1  1994/07/15  15:00:08  mdf
 * 	Hewlett Packard Security Code Drop
 * 	[1994/07/14  17:17:26  mdf]
 * 
 * Revision 1.1.5.2  1992/12/29  13:30:27  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/28  20:48:33  zeliff]
 * 
 * Revision 1.1.3.2  1992/06/02  21:01:58  sekhar
 * 	Drop the Unix interfaces changes made by frisco.
 * 	[1992/06/01  20:15:34  sekhar]
 * 
 * Revision 1.1.1.2  1992/05/22  19:03:06  frisco
 * 	Add Unix interface support
 * 
 * Revision 1.1  1992/01/19  14:54:58  devrcs
 * 	Initial revision
 * 
 * $EndLog$
 */
/* Generated by IDL compiler version OSF DCE T1.2.0-09 */
#ifndef rgynbase_v0_0_included
#define rgynbase_v0_0_included
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
#ifndef passwd_v0_0_included
#include <dce/passwd.h>
#endif
#define sec_rgy_status_ok (0)
typedef unsigned32 bitset;
typedef signed32 sec_timeval_sec_t;
typedef struct  {
signed32 sec;
signed32 usec;
} sec_timeval_t;
typedef signed32 sec_timeval_period_t;
typedef signed32 sec_rgy_acct_key_t;
#define sec_rgy_acct_key_none (0)
#define sec_rgy_acct_key_person (1)
#define sec_rgy_acct_key_group (2)
#define sec_rgy_acct_key_org (3)
#define sec_rgy_acct_key_last (4)
typedef struct  {
uuid_t source;
signed32 handle;
boolean32 valid;
} sec_rgy_cursor_t;
#define sec_rgy_pname_max_len (256)
#define sec_rgy_pname_t_size (257)
typedef idl_char sec_rgy_pname_t[257];
#define sec_rgy_name_max_len (1024)
#define sec_rgy_name_t_size (1025)
typedef idl_char sec_rgy_name_t[1025];
typedef struct  {
sec_rgy_name_t pname;
sec_rgy_name_t gname;
sec_rgy_name_t oname;
} sec_rgy_login_name_t;
typedef idl_char sec_rgy_member_t[1025];
#define sec_rgy_max_unix_passwd_len (16)
typedef idl_char sec_rgy_unix_passwd_buf_t[16];
#define sec_rgy_uxid_unknown (-1)
typedef struct sec_rgy_sid_t {
uuid_t person;
uuid_t group;
uuid_t org;
} sec_rgy_sid_t;
typedef struct  {
signed32 person;
signed32 group;
signed32 org;
} sec_rgy_unix_sid_t;
typedef struct sec_rgy_foreign_id_t {
uuid_t principal;
uuid_t cell;
} sec_rgy_foreign_id_t;
typedef signed32 sec_rgy_domain_t;
#define sec_rgy_domain_person (0)
#define sec_rgy_domain_group (1)
#define sec_rgy_domain_org (2)
#define sec_rgy_domain_last (3)
typedef bitset sec_rgy_pgo_flags_t;
#define sec_rgy_pgo_is_an_alias (1)
#define sec_rgy_pgo_is_required (2)
#define sec_rgy_pgo_projlist_ok (4)
#define sec_rgy_pgo_flags_none (0)
typedef struct  {
uuid_t id;
signed32 unix_num;
signed32 quota;
sec_rgy_pgo_flags_t flags;
sec_rgy_pname_t fullname;
} sec_rgy_pgo_item_t;
#define sec_rgy_quota_unlimited (-1)
typedef bitset sec_rgy_acct_admin_flags_t;
#define sec_rgy_acct_admin_valid (1)
#define sec_rgy_acct_admin_audit (2)
#define sec_rgy_acct_admin_server (4)
#define sec_rgy_acct_admin_client (8)
#define sec_rgy_acct_admin_flags_none (0)
typedef bitset sec_rgy_acct_auth_flags_t;
#define sec_rgy_acct_auth_post_dated (1)
#define sec_rgy_acct_auth_forwardable (2)
#define sec_rgy_acct_auth_tgt (4)
#define sec_rgy_acct_auth_renewable (8)
#define sec_rgy_acct_auth_proxiable (16)
#define sec_rgy_acct_auth_dup_skey (32)
#define sec_rgy_acct_auth_user_to_user (64)
#define sec_rgy_acct_auth_flags_none (0)
typedef struct  {
sec_rgy_foreign_id_t creator;
sec_timeval_sec_t creation_date;
sec_rgy_foreign_id_t last_changer;
sec_timeval_sec_t change_date;
sec_timeval_sec_t expiration_date;
sec_timeval_sec_t good_since_date;
sec_rgy_acct_admin_flags_t flags;
sec_rgy_acct_auth_flags_t authentication_flags;
} sec_rgy_acct_admin_t;
typedef bitset sec_rgy_acct_user_flags_t;
#define sec_rgy_acct_user_passwd_valid (1)
#define sec_rgy_acct_user_flags_none (0)
typedef struct  {
sec_rgy_pname_t gecos;
sec_rgy_pname_t homedir;
sec_rgy_pname_t shell;
sec_passwd_version_t passwd_version_number;
sec_timeval_sec_t passwd_dtm;
sec_rgy_acct_user_flags_t flags;
sec_rgy_unix_passwd_buf_t passwd;
} sec_rgy_acct_user_t;
typedef bitset sec_rgy_plcy_pwd_flags_t;
#define sec_rgy_plcy_pwd_no_spaces (1)
#define sec_rgy_plcy_pwd_non_alpha (2)
#define sec_rgy_plcy_pwd_flags_none (0)
typedef struct  {
signed32 passwd_min_len;
sec_timeval_period_t passwd_lifetime;
sec_timeval_sec_t passwd_exp_date;
sec_timeval_period_t acct_lifespan;
sec_rgy_plcy_pwd_flags_t passwd_flags;
} sec_rgy_plcy_t;
typedef struct  {
sec_timeval_period_t max_ticket_lifetime;
sec_timeval_period_t max_renewable_lifetime;
} sec_rgy_plcy_auth_t;
typedef bitset sec_rgy_properties_flags_t;
#define sec_rgy_prop_readonly (1)
#define sec_rgy_prop_auth_cert_unbound (2)
#define sec_rgy_prop_shadow_passwd (4)
#define sec_rgy_prop_embedded_unix_id (8)
#define sec_rgy_properties_none (0)
typedef struct  {
signed32 read_version;
signed32 write_version;
sec_timeval_period_t minimum_ticket_lifetime;
sec_timeval_period_t default_certificate_lifetime;
unsigned32 low_unix_id_person;
unsigned32 low_unix_id_group;
unsigned32 low_unix_id_org;
unsigned32 max_unix_id;
sec_rgy_properties_flags_t flags;
sec_rgy_name_t realm;
uuid_t realm_uuid;
signed32 unauthenticated_quota;
} sec_rgy_properties_t;
#define sec_rgy_wildcard_name "*"
#define sec_rgy_wildcard_sid "*.*.*"
typedef signed32 sec_rgy_override_t;
#define sec_rgy_no_override (0)
#define sec_rgy_override (1)
typedef signed32 sec_rgy_mode_resolve_t;
#define sec_rgy_resolve_pname (0)
#define sec_rgy_no_resolve_pname (1)
typedef idl_char sec_rgy_unix_gecos_t[292];
typedef idl_char sec_rgy_unix_login_name_t[1025];
typedef struct  {
sec_rgy_unix_login_name_t name;
sec_rgy_unix_passwd_buf_t passwd;
signed32 uid;
signed32 gid;
signed32 oid;
sec_rgy_unix_gecos_t gecos;
sec_rgy_pname_t homedir;
sec_rgy_pname_t shell;
} sec_rgy_unix_passwd_t;
typedef idl_char sec_rgy_member_buf_t[10250];
typedef struct  {
sec_rgy_name_t name;
signed32 gid;
sec_rgy_member_buf_t members;
} sec_rgy_unix_group_t;

#ifdef __cplusplus
    }

#endif
#endif
