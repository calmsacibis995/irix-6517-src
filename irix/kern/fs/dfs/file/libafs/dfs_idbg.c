/*
 * dfsidbg.c
 *
 *      idbg entry points for dfs
 *
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
 * $Log: dfs_idbg.c,v $
 * Revision 65.2  1998/09/22 20:34:01  gwehrman
 * Replaced extern function declarations with include directives for cm_idbg.h,
 * common_data.h, and icl_idbg.h.  Added entries for dfslogs and dfstrace to
 * idbg_dfs_help().  Added entries for dfslogs and dfstrace to dfsidbginit().
 *
 */

#ident "$Revision: 65.2 $"


#include <sys/types.h>
#include <sys/idbg.h>
#include <sys/idbgentry.h>

#include <dcedfs/cm_idbg.h>
#include <dcedfs/common_data.h>
#include <dcedfs/icl_idbg.h>


void
idbg_dfs_help(void)
{
        qprintf("\n\n");
        qprintf("dcp <addr>             Print single dcache entry\n");
        qprintf("dcp 0                  Print all dcache entries\n");
        qprintf("dcplist                Print all dcp *\n");
        qprintf("dcplock                Print all locked dcps *\n");
        qprintf("scp <addr>             Print single scache entry\n");
        qprintf("scp 0                  Print all scache entries\n");
        qprintf("scplist                Print all scp *, vnode *, v_count\n");
        qprintf("scplock                Print all locked scps \n");
        qprintf("fcache <addr>          Print fcache entry\n");
        qprintf("dfs_conn <addr>        Print single cm_conn entry\n");
        qprintf("dfs_conn 0             Print all cm_conn entries\n");
        qprintf("dfs_server <addr>      Print single cm_server entry\n");
        qprintf("dfs_server 0           Print all cm_server entries\n");
        qprintf("dfs_volume <addr>      Print single cm_volume entry\n");
        qprintf("dfs_volume 0           Print all cm_volume entries\n");
	qprintf("dfslogs                List all dfstrace logs\n");
	qprintf("dfstrace <log>         Dump dfstrace log\n");
	qprintf("dfstrace 0             Dump all dfstrace logs\n");
        qprintf("\n\n");
}



void
dfsidbginit(void)
{
    idbg_addfunc("dcp", idbg_dcache);
    idbg_addfunc("dcplock", idbg_dcache_lock);
    idbg_addfunc("scplock", idbg_scache_lock);
    idbg_addfunc("scp", idbg_scache);
    idbg_addfunc("scplist", idbg_scache_list);
    idbg_addfunc("dcplist", idbg_dcache_list);
    idbg_addfunc("fcache", idbg_fcache);
    idbg_addfunc("dfs_conn", idbg_cm_conn);
    idbg_addfunc("dfs_server", idbg_cm_servers);
    idbg_addfunc("dfs_volume", idbg_cm_volumes);
    idbg_addfunc("dfs_help", idbg_dfs_help);
    idbg_addfunc("dfslogs", idbg_dfslogs);
    idbg_addfunc("dfstrace", idbg_dfstrace);
}
