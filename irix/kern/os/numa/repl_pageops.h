#ifndef	_NUMA_REPL_PAGEOPS_H
#define	_NUMA_REPL_PAGEOPS_H

/******************************************************************************
 * repl_pageops.h
 *
 * Copyright 1995, Silicon Graphics, Inc.
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
 ******************************************************************************/

#ident "$Revision: 1.2 $"

#if defined(_LANGUAGE_C) || defined(_LANGUAGE_C_PLUS_PLUS)

/*
 * Extended Page cache operations needed from page replication perspective.
 *
 * This Interface is used within replication related files.
 * No one outside replication specific files would need this interface.
 */
/*
 * Insert a replicated page onto hash chain.
 * This is responsible for calling the right routine to insert page.
 * If the new page was inserted, it has to do the additional work needed
 * to maintain the state of page properly. 
 */
extern pfd_t *page_replica_insert(pfd_t *, pfd_t *);

/*
 * pfind_node
 * Check if any page exists on a specific node for specified tag, 
 * and pageno.
 */
extern pfd_t *pfind_node(struct vnode *, pgno_t, cnodeid_t, int);

#endif /* C || C++ */
#endif /* _NUMA_REPL_PAGEOPS_H */
