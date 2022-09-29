/*
 * Copyright (c) 1994 Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED.
 *
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND
 * Use, duplication or disclosure by the Government is subject to
 * restrictions as set forth in FAR 52.227.19(c)(2) or subparagraph
 * (c)(1)(ii) of the Rights in Technical Data and Computer Software clause
 * at DFARS 252.227-7013 and/or similar or successor clauses in the FAR,
 * or the DOD or NASA FAR Supplement.  Contractor/manufacturer is Silicon
 * Graphics, Inc., 2011 N. Shoreline Blvd., Mountain View, CA 94039-7311.
 *
 * THIS SOFTWARE CONTAINS CONFIDENTIAL AND PROPRIETARY INFORMATION OF
 * SILICON GRAPHICS, INC.  ANY DUPLICATION, MODIFICATION, DISTRIBUTION, OR
 * DISCLOSURE IS STRICTLY PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN
 * PERMISSION OF SILICON GRAPHICS, INC.
 */

/*
 * $Id: xlv.h,v 1.3 1998/06/15 05:51:55 tes Exp $
 */

#if !BEFORE_IRIX6_4

#include <unistd.h>
#include <sys/types.h>
#include <sys/syssgi.h>
#include <sys/uuid.h>
#include <sys/xlv_base.h>
#include <sys/xlv_tab.h>

#if defined(IRIX6_4)
#include "./xlv_attr.h"
#include "./xlv_stat.h"
#else
#include <sys/xlv_attr.h>
#include <sys/xlv_stat.h>
#endif

typedef struct {
    int         subvol;
    uuid_t      uuid;
    int         id;
    char        *extname;
    int		fetched;
    xlv_stat_t	stat;
} pm_xlv_inst;

extern pm_xlv_inst	*xlv_inst;
extern int		xlv_instLen;
extern int		xlv_initDone;

extern int xlv_setup(void);

#endif /* IRIX6.4 and later */
