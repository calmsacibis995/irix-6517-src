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
 * $Id: hwrouter.h,v 1.10 1997/03/27 02:50:17 chatz Exp $
 */

#if !defined(IRIX6_4)
#define SN0	1
#endif

#include <sys/syssgi.h>

#if defined(IRIX6_4)

#include <sys/SN0/router.h>
#include <sys/SN0/routerdrv.h>

#else /* IRIX6_5 and later */

#include <sys/SN/router.h>
#include <sys/SN/routerdrv.h>

#endif

typedef struct {
    char		*hwgprefix;	/* path to router directory in hardware graph */
    int			fetched;	/* flags fetched status */
    int			id;		/* internl instance id */
    char		*name;		/* external instance id */
    int			inc;		/* nonzero => included in profile */
    int			stats_size;	/* size of stats from ROUTERINFO_GETSZ*/
    router_info_t	*stats;		/* router info/stats */
} _pm_router;

typedef struct {
    char		*hwgsuffix;	/* suffix for port name */
    char		*hwglink;	/* destination node or router */
    int			routerindex;	/* index in _pm_router table[] */
    int			portindex;	/* index in _pm_router[router].stats->ri_port[] */
    int			id;		/* internl instance id */
    char		*name;		/* external instance id */
					/* i.e. routerNrMpP where N, M and P are small ints */
    int			active;		/* nonzero => active port */
    int			inc;		/* nonzero => included in profile */
} _pm_router_port;

