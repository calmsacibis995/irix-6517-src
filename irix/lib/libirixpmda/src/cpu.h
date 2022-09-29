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
 * $Id: cpu.h,v 1.4 1997/03/27 02:50:59 chatz Exp $
 */

#include <sys/sysinfo.h>

#define CPU_EXTNAME_LEN 16	/* size of cpu external instance name */

typedef struct {
    int			id;			/* internal instance id */
    char		extname[CPU_EXTNAME_LEN];  /* external instance name */
    char		*hwgname;		/* hardware graph path to cpu */
    int			freq;			/* CPU frequency */
    int			sdcache;		/* size of secondary cache */
    char		*type;			/* CPU type */
    int			map;			/* mapping to phyical id */
    struct sysinfo	sysinfo;		/* sysinfo data for cpu */
} _pm_cpu;

/* internal instance identifier */
#define CPU_ID(p) ((p)->id)


