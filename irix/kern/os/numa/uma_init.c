/*
 * os/numa/uma_init.c
 *
 *
 * Copyright 1996, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 *
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 *
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 *
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 */

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/pda.h>
#include <sys/nodepda.h>
#include <sys/sysmacros.h>
#include <sys/pmo.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include "uma_init.h"

typedef struct uma_init_table {
        int   (*init_function)(void);
        char* name;
} uma_init_table_t;

uma_init_table_t uma_init_table[] = {
        plac_policy_uma_init,             "PlacementUma",
        fbck_policy_uma_init,             "FallbackUma",
        migr_policy_uma_init,             "MigrationUma",
        repl_policy_uma_init,             "ReplicationUma",
        pm_policy_stubs_init,             "PolicyStubsInit",
        0,                                NULL
};
 
/*
 * This method takes care of initializing the base
 * mmci module. 
 */

void
uma_init()
{
        uma_init_table_t* pt = uma_init_table;
        int errcode;

        while (pt->init_function) {
#ifdef DEBUG_PM              
                printf("INITIALIZING {%s}\n", pt->name);
#endif                
                if ((errcode =  (*pt->init_function)()) != 0) {
                        cmn_err(CE_PANIC,
                                "[uma_init]: Module <%s> initialization failed (%d)",
                                pt->name, errcode);
                }
                pt++;
        }

}
