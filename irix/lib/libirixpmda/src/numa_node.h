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
 * $Id: numa_node.h,v 1.9 1997/10/07 06:16:56 chatz Exp $
 */

#include <sys/types.h>
#include <sys/sysmp.h>
#include <sys/syssgi.h>
#include <sys/sysinfo.h>
#include <sys/numa_stats.h>
#include <sys/hwperfmacros.h>

#if defined(IRIX6_4)
#include <sys/SN0/hubnuma.h>
#include <sys/SN0/hubstat.h>
#include <sys/SN0/sn0drv.h>
#else
#include <sys/SN/SN0/arch.h>
#include <sys/SN/arch.h>
#include <sys/SN/router.h>
#include <sys/SN/SN0/hubstat.h>
#include <sys/SN/SN0/sn0drv.h>
#endif

typedef struct {
    int			id;		/* internal instance identifier */
    char		*extname;	/* external id (compressed path) */
    char		*hwgname;	/* path to node in hwgfs */
    numa_stats_t	numa;		/* numa info/stats for this node */
    nodeinfo_t		*mem;		/* node memory stats */
    int			hubfd;		/* file descriptor to hub in hwg */
    hubstat_t		hub;		/* hub stats */
} _pm_node;

typedef struct {
    unsigned int	module:16;
    unsigned int	slot:8;
    unsigned int	pad:8;
} _pm_node_id;

/* internal instance identifier */
#define NODE_ID(p) ((p)->id)


