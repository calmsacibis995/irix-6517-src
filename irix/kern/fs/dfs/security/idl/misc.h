/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1996 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: misc.h,v $
 * Revision 65.2  1999/02/03 22:44:20  mek
 * Convert from IDL to C style include format.
 *
 * Revision 65.1  1997/10/20 19:46:31  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.4.2  1996/03/09  23:27:57  marty
 * 	Update OSF copyright year
 * 	[1996/03/09  22:43:26  marty]
 *
 * Revision 1.1.4.1  1995/12/08  17:31:44  root
 * 	Submit OSF/DCE 1.2.1
 * 
 * 	HP revision /main/HPDCE02/1  1994/12/08  17:29 UTC  hanfei
 * 	merge
 * 
 * 	HP revision /main/hanfei_cpi_bl2/1  1994/12/07  19:31 UTC  hanfei
 * 	merge
 * 
 * 	HP revision /main/hanfei_cpi_bl1/2  1994/12/07  17:09 UTC  hanfei
 * 	change sec_rgy_checkpt_reset_interval interface
 * 
 * 	HP revision /main/hanfei_cpi_bl1/1  1994/12/05  14:48 UTC  hanfei
 * 	work for rgy checkpoint to be configurable
 * 	[1995/12/08  16:54:59  root]
 * 
 * Revision 1.1.2.2  1992/12/29  13:29:24  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/28  20:16:44  zeliff]
 * 
 * Revision 1.1  1992/01/19  14:55:32  devrcs
 * 	Initial revision
 * 
 * $EndLog$
 */
/* Generated by IDL compiler version OSF DCE T1.2.0-09 */
#ifndef sec_rgy_misc_v0_0_included
#define sec_rgy_misc_v0_0_included
#ifndef IDLBASE_H
#include <dce/idlbase.h>
#endif

#ifdef __cplusplus
    extern "C" {
#endif

#ifndef nbase_v0_0_included
#include <dce/nbase.h>
#endif
#ifndef rgybase_v0_0_included
#include <dce/rgybase.h>
#endif
#ifndef oride_base_v0_0_included
#include <dce/oride_base.h>
#endif
extern void sec_rgy_login_get_info(
#ifdef IDL_PROTOTYPES
    /* [in] */ sec_rgy_handle_t context,
    /* [in, out] */ sec_rgy_login_name_t *login_name,
    /* [out] */ sec_rgy_acct_key_t *key_parts,
    /* [out] */ sec_rgy_sid_t *sid,
    /* [out] */ sec_rgy_unix_sid_t *unix_sid,
    /* [out] */ sec_rgy_acct_user_t *user_part,
    /* [out] */ sec_rgy_acct_admin_t *admin_part,
    /* [out] */ sec_rgy_plcy_t *policy_data,
    /* [in] */ signed32 max_number,
    /* [out] */ signed32 *supplied_number,
    /* [out] */ uuid_t id_projlist[],
    /* [out] */ signed32 unix_projlist[],
    /* [out] */ signed32 *num_projects,
    /* [out] */ sec_rgy_name_t cell_name,
    /* [out] */ uuid_t *cell_uuid,
    /* [out] */ error_status_t *status
#endif
);
extern void sec_rgy_login_get_effective(
#ifdef IDL_PROTOTYPES
    /* [in] */ sec_rgy_handle_t context,
    /* [in, out] */ sec_rgy_login_name_t *login_name,
    /* [out] */ sec_rgy_acct_key_t *key_parts,
    /* [out] */ sec_rgy_sid_t *sid,
    /* [out] */ sec_rgy_unix_sid_t *unix_sid,
    /* [out] */ sec_rgy_acct_user_t *user_part,
    /* [out] */ sec_rgy_acct_admin_t *admin_part,
    /* [out] */ sec_rgy_plcy_t *policy_data,
    /* [in] */ signed32 max_number,
    /* [out] */ signed32 *supplied_number,
    /* [out] */ uuid_t id_projlist[],
    /* [out] */ signed32 unix_projlist[],
    /* [out] */ signed32 *num_projects,
    /* [out] */ sec_rgy_name_t cell_name,
    /* [out] */ uuid_t *cell_uuid,
    /* [out] */ sec_override_fields_t *overridden,
    /* [out] */ error_status_t *status
#endif
);
extern boolean32 sec_rgy_wait_until_consistent(
#ifdef IDL_PROTOTYPES
    /* [in] */ sec_rgy_handle_t context,
    /* [out] */ error_status_t *status
#endif
);
extern void sec_rgy_cursor_reset(
#ifdef IDL_PROTOTYPES
    /* [in, out] */ sec_rgy_cursor_t *cursor
#endif
);
extern void sec_rgy_checkpt_reset_interval(
#ifdef IDL_PROTOTYPES
    /* [in] */ sec_rgy_handle_t h,
    /* [in] */ boolean32 do_checkpt_now,
    /* [in] */ signed32 new_checkpt_interval,
    /* [in] */ idl_char *at_time_str,
    /* [out] */ error_status_t *status
#endif
);

#ifdef __cplusplus
    }

#endif
#endif
