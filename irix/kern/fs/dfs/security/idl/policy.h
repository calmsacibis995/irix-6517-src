/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1996 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: policy.h,v $
 * Revision 65.2  1999/02/03 22:44:20  mek
 * Convert from IDL to C style include format.
 *
 * Revision 65.1  1997/10/20 19:46:32  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.4.2  1996/03/09  23:28:04  marty
 * 	Update OSF copyright year
 * 	[1996/03/09  22:43:32  marty]
 *
 * Revision 1.1.4.1  1995/12/08  17:32:15  root
 * 	Submit OSF/DCE 1.2.1
 * 	[1995/12/08  16:55:06  root]
 * 
 * Revision 1.1.2.2  1992/12/29  13:29:59  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/28  20:17:02  zeliff]
 * 
 * Revision 1.1  1992/01/19  14:54:41  devrcs
 * 	Initial revision
 * 
 * $EndLog$
 */
/* Generated by IDL compiler version OSF DCE T1.2.0-09 */
#ifndef sec_rgy_plcy_v0_0_included
#define sec_rgy_plcy_v0_0_included
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
extern void sec_rgy_properties_get_info(
#ifdef IDL_PROTOTYPES
    /* [in] */ sec_rgy_handle_t context,
    /* [out] */ sec_rgy_properties_t *properties,
    /* [out] */ error_status_t *status
#endif
);
extern void sec_rgy_properties_set_info(
#ifdef IDL_PROTOTYPES
    /* [in] */ sec_rgy_handle_t context,
    /* [in] */ sec_rgy_properties_t *properties,
    /* [out] */ error_status_t *status
#endif
);
extern void sec_rgy_plcy_get_info(
#ifdef IDL_PROTOTYPES
    /* [in] */ sec_rgy_handle_t context,
    /* [in] */ sec_rgy_name_t organization,
    /* [out] */ sec_rgy_plcy_t *policy_data,
    /* [out] */ error_status_t *status
#endif
);
extern void sec_rgy_plcy_get_effective(
#ifdef IDL_PROTOTYPES
    /* [in] */ sec_rgy_handle_t context,
    /* [in] */ sec_rgy_name_t organization,
    /* [out] */ sec_rgy_plcy_t *policy_data,
    /* [out] */ error_status_t *status
#endif
);
extern void sec_rgy_plcy_set_info(
#ifdef IDL_PROTOTYPES
    /* [in] */ sec_rgy_handle_t context,
    /* [in] */ sec_rgy_name_t organization,
    /* [in] */ sec_rgy_plcy_t *policy_data,
    /* [out] */ error_status_t *status
#endif
);
extern void sec_rgy_plcy_get_override_info(
#ifdef IDL_PROTOTYPES
    /* [in] */ sec_rgy_handle_t context,
    /* [in] */ sec_rgy_name_t policy_category,
    /* [out] */ boolean32 *exclude_passwd,
    /* [out] */ boolean32 *root_passwd,
    /* [out] */ boolean32 *other_passwd,
    /* [out] */ boolean32 *custom_data,
    /* [out] */ error_status_t *status
#endif
);
extern void sec_rgy_plcy_set_override_info(
#ifdef IDL_PROTOTYPES
    /* [in] */ sec_rgy_handle_t context,
    /* [in] */ sec_rgy_name_t policy_category,
    /* [in] */ boolean32 exclude_passwd,
    /* [in] */ boolean32 root_passwd,
    /* [in] */ boolean32 other_passwd,
    /* [in] */ boolean32 custom_data,
    /* [out] */ error_status_t *status
#endif
);
extern void sec_rgy_auth_plcy_get_info(
#ifdef IDL_PROTOTYPES
    /* [in] */ sec_rgy_handle_t context,
    /* [in] */ sec_rgy_login_name_t *account,
    /* [out] */ sec_rgy_plcy_auth_t *auth_policy,
    /* [out] */ error_status_t *status
#endif
);
extern void sec_rgy_auth_plcy_get_effective(
#ifdef IDL_PROTOTYPES
    /* [in] */ sec_rgy_handle_t context,
    /* [in] */ sec_rgy_login_name_t *account,
    /* [out] */ sec_rgy_plcy_auth_t *auth_policy,
    /* [out] */ error_status_t *status
#endif
);
extern void sec_rgy_auth_plcy_set_info(
#ifdef IDL_PROTOTYPES
    /* [in] */ sec_rgy_handle_t context,
    /* [in] */ sec_rgy_login_name_t *account,
    /* [in] */ sec_rgy_plcy_auth_t *auth_policy,
    /* [out] */ error_status_t *status
#endif
);

#ifdef __cplusplus
    }

#endif
#endif
