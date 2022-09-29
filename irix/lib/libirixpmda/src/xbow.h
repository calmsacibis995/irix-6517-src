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
 * $Id: xbow.h,v 1.5 1997/07/02 04:27:10 chatz Exp $
 */

#if !defined(IRIX6_2) && !defined(IRIX6_3)

#include <sys/xbmon.h>

#define XB_VERSION	1		/* expected xbmon API version */

typedef struct {
    int			id;		/* internal instance identifier */
    char		*extname;	/* external id (compressed path) */
    xb_vcounter_t	*cntrs;		/* counters */
    char		fetched;	/* instance fetched */
} _pm_xbowport;

typedef struct {
    int			id;		/* internal instance identifier */
    char		*extname;	/* external id (compressed path) */
    char		*hwgname;	/* path to xbow in hwgfs */
    int			mapped;		/* monitoring this xbow */
    int			devfd;		/* device file descriptor */
    xb_vcntr_block_t	*block;		/* block of counters */
    __uint32_t		nports;		/* number of active ports */
    __uint32_t		gen;		/* generation number */
    char		fetched;	/* instance fetched */
} _pm_xbow;

extern _pm_xbow		*xbows;		/* the xbows */
extern _pm_xbowport	*xbowports;	/* the xbow ports */

#endif 

extern int		n_xbows;	/* number of xbows in hardware graph */
extern int		n_xbowports;	/* number of active xbow ports */
extern int		n_xbowportln;	/* length of xbow port array */

#define XBOW_ID(xbow_ptr)		((xbow_ptr)->id)
#define XBOWPORT_ID(xbowport_ptr)	((xbowport_ptr)->id)
#define XBOWPORT_HNDL(xbow, port)	\
    (&xbowports[(xbow * (XB_MON_LINK_MASK + 1)) + port])
#define XBOWPORT_ACTIVE(xbowport_ptr)	\
    (((xbowport_ptr)->cntrs != NULL) &&   \
     ((xbowport_ptr)->cntrs->flags & XB_VCNTR_LINK_ALIVE))

#define XBOW_BYTES_PACKET		16 /* bytes per packet */

extern int xbow_store(pmValueSet *);
