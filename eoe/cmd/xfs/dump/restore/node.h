#ifndef NODE_H
#define NODE_H

#ident "$Header: /proj/irix6.5.7m/isms/eoe/cmd/xfs/dump/restore/RCS/node.h,v 1.3 1995/07/20 22:59:40 cbullis Exp $"

/* node.[ch] - abstract pool of nodes
 *
 * operators alloc, free, map, and unmap nodes.
 */

typedef size32_t nh_t;
#define NH_NULL	SIZE32MAX

/* node_init - creates a new node abstraction.
 * user reserves one byte per node for use by the node abstraction
 */
extern bool_t node_init( intgen_t fd,		/* backing store */
		         off64_t off,		/* offset into backing store */
		         size_t nodesz,		/* node size */
		         ix_t nodehkix,		/* my housekeeping byte */
		         size_t alignsz,	/* node alignment requirement */
		         size64_t vmsz,		/* abstractions's share of VM */
			 size64_t mincnt );	/* min. est. nodes needed */

/* node_sync - syncs up with existing node abstraction persistent state
 */
extern bool_t node_sync( intgen_t fd, off64_t off );

/* node_alloc - allocates a node, returning a handle.
 * returns NULL handle if no space left.
 */
extern nh_t node_alloc( void );

/* node_map - returns a pointer to the node identified by the node handle.
 * pointer remains valid until node_unmap called.
 */
extern void *node_map( nh_t node_handle );

/* node_unmap - unmaps the node.
 * SIDE-EFFECT: sets *nodepp to 0.
 */
extern void node_unmap( nh_t node_handle, void **nodepp );

/* node_free - frees a previously allocated node.
 * SIDE-EFFECT: sets *node_handlep to NODE_HANDLE_NULL.
 */
extern void node_free( nh_t *node_handlep );

#endif /* NODE_H */
