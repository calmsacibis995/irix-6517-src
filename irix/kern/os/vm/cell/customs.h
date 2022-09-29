#ifndef __CUSTOMS_H__
#define __CUSTOMS_H__

#ifdef __cplusplus
extern "C" {
#endif

/*
 * vm/cell/customs.h
 *
 *
 * Copyright 1997, Silicon Graphics, Inc.
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

#ident	"$Revision: 1.3 $"


/*
 * These are internal definitions for the implementation of import/export.
 * Not to be used outside of the files that implement this functionality!
 */

#include <sys/types.h>
#include <sys/pfdat.h>


/*
 * The proxy pfdat.  A proxy is created for each imported page.  It is
 * simply a regular pfdat structure along with some extra fields.  These
 * extra fields basically allow us to implement pfntopfdat and pfdattopfn
 * for proxies.  To implement pfntopfdat, proxies are maintained in a
 * search structure indexed by pfn.  To implement pfdattopfn, we need
 * to know the pfn for a given proxy.  Note that the pr_pfd field must
 * be first in the proxy struct in order for the pfdattoproxy() macro
 * to work.
 */

struct proxy_pfdat {
	struct pfdat		 pr_pfd;	/* the proxy's pfdat       */
	cell_t			 pr_export_cell;/* cell page imported from */
	pfn_t			 pr_pfn;	/* the pfn of this proxy   */
	struct proxy_pfdat	*pr_next;	/* link for proxy searches */
};


typedef struct proxy_pfdat	proxy_pfd_t;


/*
 * Given a pfd pointer for a proxy, turn it back into the proxy pointer.
 * Since the pfdat is the first thing in the proxy struct, this is
 * simply a matter of casting the pointer.
 */

#define pfdattoproxy(pfd)	((struct proxy_pfdat *) pfd)



/*
 * The following constants define default values for the export/import
 * daemon. Every UNIMPORT_INTERVAL seconds the daemon wakes up. All
 * imported pages that are invalid (not associated with a vnode) are 
 * unimported. In addition, pages that are still valid but have had 
 * a pf_use == 0 for greater than UNIMPORT_AGE_SEC seconds are also 
 * unimported.
 */

#define UNIMPORT_INTERVAL	1	/* daemon run frequency in seconds */
#define UNIMPORT_AGE_SEC	60	/* number of sec to keep imported pages with
					 * zero use count before unimporting them.
					 */

#ifdef __cplusplus
}
#endif

#endif /* __CUSTOMS_H__ */
