/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994, 1996 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/*
 * HISTORY
 * $Log: dce_cf.h,v $
 * Revision 65.1  1997/10/20 19:15:25  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.18.2  1996/02/18  23:32:56  marty
 * 	Update OSF copyright years
 * 	[1996/02/18  22:20:55  marty]
 *
 * Revision 1.1.18.1  1995/12/08  21:36:50  root
 * 	Submit OSF/DCE 1.2.1
 * 
 * 	HP revision /main/HPDCE02/1  1995/07/21  18:07 UTC  robinson
 * 	C++ ifdefs to parallel those made for KK+ patch
 * 	[1995/12/08  18:08:39  root]
 * 
 * Revision 1.1.16.5  1994/08/24  14:38:35  bowe
 * 	Prototypes for dce_cf_get_cell_aliases(), dce_cf_free_cell_aliases(),
 * 	and dce_cf_same_cell_name().  [CR 11843]
 * 	[1994/08/24  14:37:18  bowe]
 * 
 * Revision 1.1.16.4  1994/06/09  16:05:42  devsrc
 * 	cr10892 - fix copyright
 * 	[1994/06/09  15:50:19  devsrc]
 * 
 * Revision 1.1.16.3  1994/02/25  20:57:14  pwang
 * 	Added prototype for dce_cf_dced_entry_from_host
 * 	[1994/02/25  20:56:32  pwang]
 * 
 * Revision 1.1.16.2  1994/01/10  20:04:24  rsalz
 * 	Remove dcecfg.{cat,msg} (OT CR 9692).
 * 	Write dce_cf_get_csrgy_filename (OT CR 9696).
 * 	[1994/01/10  19:55:26  rsalz]
 * 
 * Revision 1.1.16.1  1993/12/29  19:22:29  rsalz
 * 	Move dce_cf_XXX from config to dce/utils/dcecf (OT CR 9663).
 * 	Rewrite to fix various bugs (OT CR 9665).
 * 	[1993/12/29  16:17:55  rsalz]
 * 
 * $EndLog$
 */

/*
**  Routines to get cell and host names from a local database.
*/
#if	!defined(_DCE_CF_H)
#define _DCE_CF_H

#include <dce/dce_cf_const.h>
#include <dce/dcecfgmsg.h>

#ifdef __cplusplus
extern "C" {
#endif


extern void dce_cf_get_host_name(
    char**			/* hostname */,
    error_status_t*		/* status */
);

extern void dce_cf_get_cell_name(
    char**			/* cellname */,
    error_status_t*		/* status */
);

extern void dce_cf_get_csrgy_filename(
    char**			/* filename */,
    error_status_t*		/* status */
);

extern void dce_cf_find_name_by_key(
    FILE*			/* F */,
    char*			/* key */,
    char**			/* value */,
    error_status_t*		/* status */
);

extern void dce_cf_find_names_by_key(
    FILE*		/* F */,
    char*		/* key */,
    char***		/* values */,
    error_status_t*	/* status */
);

extern void dce_cf_prin_name_from_host(
    char*			/* hostname */,
    char**			/* value */,
    error_status_t*		/* status */
);

extern void dce_cf_profile_entry_from_host(
    char*			/* hostname */,
    char**			/* value */,
    error_status_t*		/* status */
);

extern void dce_cf_binding_entry_from_host(
    char*			/* hostname */,
    char**			/* value */,
    error_status_t*		/* status */
);

extern void dce_cf_dced_entry_from_host(
    char*			/* hostname */,
    char**			/* value */,
    error_status_t*		/* status */
);

extern void dce_cf_get_cell_aliases(
    char***			/* aliases */,
    error_status_t*		/* status */
);

extern void dce_cf_free_cell_aliases(
    char**			/* aliases */,
    error_status_t*		/* status */
);

extern void dce_cf_same_cell_name(
    char*			/* cellname1 */,
    char*			/* cellname2 */,
    boolean32*			/* result */,
    error_status_t*		/* status */
);


#ifdef __cplusplus
}
#endif

#endif	/* !defined(_DCE_CF_H) */
