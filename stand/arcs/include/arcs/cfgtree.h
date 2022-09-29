/* 
 *  cfgtree.h -- Internal configuration tree definitions.
 */

#ifndef _ARCS_CFGTREE_H
#define _ARCS_CFGTREE_H

#include <arcs/hinv.h>

#ident "$Revision: 1.9 $"

/* Actual node in the ARCS config tree -- CONFIG *must* be first */
typedef struct cfgnode_s {
	COMPONENT comp;			/* actual ARCS data */
	struct cfgnode_s *parent;	/* parent */
	struct cfgnode_s *peer;		/* next peer */
	struct cfgnode_s *child;	/* first child */
	STATUS (*driver)(COMPONENT*,IOBLOCK*);	/* driver strategy call */
	struct cfgdatahdr *cfgdata;	/* configuration data */
} cfgnode_t;

#endif
