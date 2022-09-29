/*
 * cm_idbg.h
 *
 *      Declaration of functions defined in cm_idbg.c
 *
 * Copyright  1998 Silicon Graphics, Inc.  ALL RIGHTS RESERVED
 * 
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND
 * 
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the 
 * Rights in Technical Data and Computer Software clause at DFARS 252.227-7013 
 * and/or in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement. Unpublished -- rights reserved under the Copyright Laws 
 * of the United States. Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd., Mountain View, CA 94039-7311.
 * 
 * THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF SGI
 * 
 * The copyright notice above does not evidence any actual or intended 
 * publication or disclosure of this source code, which includes information 
 * that is the confidential and/or proprietary, and is a trade secret,
 * of Silicon Graphics, Inc. Any use, duplication or disclosure not 
 * specifically authorized in writing by Silicon Graphics is strictly 
 * prohibited. ANY DUPLICATION, MODIFICATION, DISTRIBUTION,PUBLIC PERFORMANCE,
 * OR PUBLIC DISPLAY OF THIS SOURCE CODE WITHOUT THE EXPRESS WRITTEN CONSENT 
 * OF SILICON GRAPHICS, INC. IS STRICTLY PROHIBITED.  THE RECEIPT OR POSSESSION
 * OF THIS SOURCE CODE AND/OR INFORMATION DOES NOT CONVEY ANY RIGHTS 
 * TO REPRODUCE, DISCLOSE OR DISTRIBUTE ITS CONTENTS, OR TO MANUFACTURE, USE, 
 * OR SELL ANYTHING THAT IT MAY DESCRIBE, IN WHOLE OR IN PART.
 *
 * $Log: cm_idbg.h,v $
 * Revision 65.1  1998/09/22 20:30:33  gwehrman
 * Declarations of functions defined in cm_idbg.c.
 *
 */

#ident "$Revision: 65.1 $"

#ifndef __CM_IDBG_H__
#define __CM_IDBG_H__


#include <dcedfs/common_data.h>

/* external cm data structures */
struct lock_data;
struct cm_dcache;
struct cm_server;
struct cm_conn;
struct cm_scache;
struct cm_attrs;
struct cm_vdirent;
struct squeue;
struct cm_cookieList;

/* Functions defined in cm_idbg.c */
extern void pr_lock_data (struct lock_data *, int, char *);
extern void print_afsfid(afsFid*, char *);
extern void pr_uuid_ent(afsUUID *, int, char *);
extern void pr_cm_dcache_entry (struct cm_dcache *, int, char *);
extern void pr_all_cm_dcache_entries (int, char *);
extern void pr_cm_fcache_entry (long , int , char *);
extern void pr_cm_server_entry(struct cm_server *, int, char *);
extern void pr_cm_conn_entry(struct cm_conn *, int, char *);
extern int pr_cm_scache_entry (long, struct cm_scache *, int, char *);
extern void pr_all_cm_scache_entries (int, char *);
extern void prcmattr(struct cm_attrs *, struct cm_attrs *, int , char *);
extern void prvdirent(struct cm_vdirent *, int, char *);
extern void pr_cm_tokenlist (struct squeue *, int, char *);
extern void prcookielist(struct cm_cookieList *, int, char *);
extern void idbg_cm_conn(__psint_t);
extern void idbg_cm_servers(__psint_t);
extern void idbg_cm_volumes(__psint_t);
extern void idbg_dcache(long, int);
extern void idbg_dcache_list (void);
extern void idbg_dcache_lock (void);
extern void idbg_fcache(long);
extern void idbg_scache(long, int);
extern void idbg_scache_list (long);
extern void idbg_scache_lock (long);
#endif /* !__CM_IDBG_H__ */
