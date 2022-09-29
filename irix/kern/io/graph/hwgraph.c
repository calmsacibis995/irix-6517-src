/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1992-1995 Silicon Graphics, Inc.           *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ident	"$Revision: 1.86 $"

#include <sys/types.h>
#include <sys/attributes.h>
#include <sys/cmn_err.h>
#include <sys/conf.h>
#include <sys/debug.h>
#include <sys/driver.h>
#include <sys/errno.h>
#include <sys/invent.h>
#include <sys/iograph.h>
#include <sys/nodepda.h>
#include <sys/pathname.h>
#include <sys/pda.h>
#include <sys/stat.h>
#include <sys/vnode.h>
#include <string.h>

#if !_USER_MODE_TEST
#include <sys/systm.h>
#include <sys/kmem.h>
#endif /* !_USER_MODE_TEST */

#include <ksys/hwg.h>

#define	NEW(ptr)	(ptr = kmem_zalloc(sizeof (*(ptr)), KM_NOSLEEP))
#define	DEL(ptr)	(kmem_free(ptr, sizeof (*(ptr))))

/*
** The "hwgraph" or "hardware graph" module provides a personality on top of the
** basic graph module that is particularly suited for the graph that represents
** devices, hardware, and inventory.  In general, there is a vertex in the hwgraph
** for every special file.  For example, a vertex for a disk slice, and for a
** console device.
**
** The hardware graph uses fast "indexed information" at each vertex to store the
** {b,c}devsw entry corresponding to the device (if any).  It also defines a
** "connect point" which is the place in the graph that the vertex was first
** connected to.  While somewhat arbitrary, it allows some convenient semantics
** for the hardware graph file system (hwgfs).
**
** A dev_t consists of a major and minor number.  If the major number is HWGRAPH_MAJOR
** then the minor number is actually a vertex handle!  Thus, from a dev_t we get
** to the vertex.  From a vertex, we get to generic information including the
** {b,c}devsw entry for the driver controlling that special file.
**
** Some basic design principles of the hwgraph layer:
**	-require no special locking above this layer
**	-minimize space usage.  Expect 1000's of vertices.
**	-allow drivers to manage a chunk of the name space.
**	-make no significant user-visible changes above the vnode layer
**	-continue to allow dev_t to be used for the same legitimate uses as
**	 it is currently used.
*/

mrlock_t	hwgraph_connectpt_mrlock;	/* avoid connectpt set/get races */
#define MRLOCK_INIT(lockp, name, seq)	mrlock_init(lockp, MRLOCK_BARRIER, name, seq)
#define MRLOCK_ACCESS(lockp)		{mraccess(lockp); s=s;}
#define MRLOCK_UPDATE(lockp)		{mrupdate(lockp); s=s;}
#define MRLOCK_DONE_ACCESS(lockp)	{mraccunlock(lockp); s=s;}
#define MRLOCK_DONE_UPDATE(lockp)	{mrunlock(lockp); s=s;}
#define MRLOCK_FREE(lockp)		mrfree(lockp)

static vertex_hdl_t do_hwgraph_connectpt_get(vertex_hdl_t vhdl);
static graph_error_t do_hwgraph_connectpt_set(vertex_hdl_t vhdl, vertex_hdl_t connect_vertex);

static mrlock_t	invent_mrlock;

graph_hdl_t hwgraph;
vertex_hdl_t hwgraph_root;
int hwgraph_initted = 0;

/*******************************************************/
/* Operations on the hwgraph: init, summary_get, visit */
/*******************************************************/

/*
** Initialize the hardware graph.  This is expected to be called once
** during startup.
*/
void
hwgraph_init(void)
{
	graph_attr_t graph_attr;

	ASSERT(!hwgraph_initted);

	/* Initialize special file graph */
	graph_attr.ga_name = "hwg";
	graph_attr.ga_separator = '/';

	graph_attr.ga_reserved_places = HWGRAPH_RESERVED_PLACES;

	graph_attr.ga_num_index = HWGRAPH_NUM_INDEX_INFO;
#if (MAXCPUS == 512)
	if (graph_create(&graph_attr, &hwgraph, xxl_hwgraph_num_dev)
		!= GRAPH_SUCCESS)
#else
	if (graph_create(&graph_attr, &hwgraph, hwgraph_num_dev)
		!= GRAPH_SUCCESS)
#endif
		cmn_err(CE_PANIC, "hwgraph_init graph_create failed");

	if (graph_vertex_create(hwgraph, &hwgraph_root) != GRAPH_SUCCESS)
		cmn_err(CE_PANIC, "hwgraph_init hwgraph_vertex_create failed");

	MRLOCK_INIT(&hwgraph_connectpt_mrlock, "hwgcp", -1);
	mrinit(&invent_mrlock, "inventory");

	hwgfs_earlyinit();

	hwgraph_initted = 1;
}



graph_error_t
hwgraph_summary_get(graph_attr_t *graph_attr)
{
	return(graph_summary_get(hwgraph, graph_attr));
}


graph_error_t
hwgraph_vertex_visit(	int (*func)(void *, vertex_hdl_t),
			void *arg,
			int *retval,
			vertex_hdl_t *end_handle)
{
	return(graph_vertex_visit(hwgraph, func, arg, retval, end_handle));
}


/************************************************************/
/* Operations on VERTICES: create, destroy, clone, get_next */
/************************************************************/

graph_error_t
hwgraph_vertex_create(vertex_hdl_t *vhdlp)
{
	graph_error_t rv;
	vertex_hdl_t vhdl;

	rv = graph_vertex_create(hwgraph, vhdlp);
	if (rv != GRAPH_SUCCESS)
		return(rv);

	vhdl = *vhdlp;
	/* (void)hwgraph_bdevsw_set(vhdl, NULL); */
	(void)hwgraph_cdevsw_set(vhdl, NULL);
	(void)hwgraph_connectpt_set(vhdl, GRAPH_VERTEX_NONE);
	return(GRAPH_SUCCESS);
}

#if DEBUG_REFCT
int
hwgraph_vertex_refct(vertex_hdl_t vhdl)
{
    extern int	graph_vertex_refct(graph_hdl_t, vertex_hdl_t);
    return graph_vertex_refct(hwgraph, vhdl);
}
#endif

/*
** Caller should insure that we're not attempting additional accesses
** while we're destroying this vertex: All edges that lead into this
** vertex should be removed before hwgraph_vertex_destroy is called.
**
** This call is how a driver lets the system know that it's done with
** this vertex.  The graph code may reject this destroy request if there
** are still references held to the vertex_hdl_t.  Until all reference
** to the vertex have been deleted and the vertex has been destroyed,
** it's possible that there will be additional attempts to operate on
** the device.
**
** Returns GRAPH_SUCCESS if the vertex was actually destroyed.
**         GRAPH_IN_USE if the vertex still has references to it.
*/
graph_error_t
hwgraph_vertex_destroy(vertex_hdl_t vhdl)
{
	vertex_hdl_t connect;
	graph_error_t rv;
	int s;

	/* Let the hwgfs file system know that this vertex is going away. */
	hwgfs_vertex_destroy(vhdl);

	MRLOCK_UPDATE(&hwgraph_connectpt_mrlock);
	connect = do_hwgraph_connectpt_get(vhdl);

	rv = graph_vertex_destroy(hwgraph, vhdl);

	if (connect != GRAPH_VERTEX_NONE) {
		/* Get rid of the reference we just obtained thru connectpt_get */
		(void)graph_vertex_unref(hwgraph, connect);

		if (rv == GRAPH_SUCCESS) {
			/* The connect point was destroyed, so drop a reference */
			(void)graph_vertex_unref(hwgraph, connect);
		}
	}
	MRLOCK_DONE_UPDATE(&hwgraph_connectpt_mrlock);

	return(rv);
}


graph_error_t
hwgraph_vertex_clone(vertex_hdl_t vhdl, vertex_hdl_t *vhdlp)
{
	graph_error_t rv;

	rv = graph_vertex_clone(hwgraph, vhdl, vhdlp);
	if (rv != GRAPH_SUCCESS)
		return(rv);

	/* Give hwgfs a chance to handle clones */
	hwgfs_vertex_clone(*vhdlp);

	/*
	 * Fetch connectpt in order to note the additional ref count.
	 */
	(void)hwgraph_connectpt_get(*vhdlp);
	return(GRAPH_SUCCESS);
}


graph_error_t
hwgraph_vertex_get_next(vertex_hdl_t *vhdlp, graph_vertex_place_t *placeptr)
{
	graph_error_t rv;

	rv = graph_vertex_get_next(hwgraph, vhdlp, placeptr);
	return(rv);
}


int
hwgraph_vertex_refcnt(vertex_hdl_t vhdl)
{
	return(graph_vertex_refcnt(hwgraph, vhdl));
}


graph_error_t
hwgraph_vertex_ref(vertex_hdl_t vhdl)
{
	return(graph_vertex_ref(hwgraph, vhdl));
}


graph_error_t
hwgraph_vertex_unref(vertex_hdl_t vhdl)
{
	return(graph_vertex_unref(hwgraph, vhdl));
}


/**********************************************/
/* Operations on EDGES: add, remove, get_next */
/**********************************************/

/*
 * Add an edge to a vertex.  If this is the first edge connected to a vertex
 * then record the source vertex as the destination's "connect point".  This
 * places a somewhat arbitrary hierarchy on the hardware graph which may be
 * useful for error reporting and for generally understanding what's connected
 * where.  It also supplies some default semantics for hwgfs concept of "..".
 *
 * If this vertex already has a bdevsw or cdevsw associated with it, then
 * don't allow outgoing edges to be added -- we need to work with hwgfs.
 */
graph_error_t
hwgraph_edge_add(vertex_hdl_t from, vertex_hdl_t to, char *name)
{
	graph_error_t rv;
	arbitrary_info_t connect;
	arbitrary_info_t devsw_entry = NULL;
	int s;

	(void)graph_info_get_IDX(hwgraph, from, HWGRAPH_DEVSW, &devsw_entry);
	if (devsw_entry != NULL)
		return(GRAPH_ILLEGAL_REQUEST);

	rv = graph_edge_add(hwgraph, from, to, name);
	if (rv != GRAPH_SUCCESS)
		return(rv);

	/*
	 * If the destination vertex doesn't already have a connect point
	 * defined, try to give it one that points *to* the *source* vertex.
	 */
	MRLOCK_UPDATE(&hwgraph_connectpt_mrlock);
	connect = do_hwgraph_connectpt_get(to);
	if (connect == GRAPH_VERTEX_NONE)
		(void)do_hwgraph_connectpt_set(to, from);
	else
		graph_vertex_unref(hwgraph, connect);
	MRLOCK_DONE_UPDATE(&hwgraph_connectpt_mrlock);
	
	device_admin_info_update(to);	/* add device admin info if any */
	return(GRAPH_SUCCESS);
}



/*
** Remove an edge from a vertex.  If the destination vertex that this edge
** pointed to considered this vertex to be its connect point, then change
** the destination's connect point to GRAPH_VERTEX_NONE.
*/
graph_error_t
hwgraph_edge_remove(vertex_hdl_t from, char *name, vertex_hdl_t *toptr)
{
	graph_error_t rv;
	vertex_hdl_t dest_vertex, connect;
	int s;

	rv = graph_edge_remove(hwgraph, from, name, &dest_vertex);
	if (rv != GRAPH_SUCCESS)
		return(rv);

	MRLOCK_UPDATE(&hwgraph_connectpt_mrlock);
	connect = do_hwgraph_connectpt_get(dest_vertex);
	if (connect == from)
		(void)do_hwgraph_connectpt_set(dest_vertex, GRAPH_VERTEX_NONE);
	MRLOCK_DONE_UPDATE(&hwgraph_connectpt_mrlock);

	if (connect != GRAPH_VERTEX_NONE)
		(void)graph_vertex_unref(hwgraph, connect);

	if (toptr)
		*toptr = dest_vertex;
	else
		(void)graph_vertex_unref(hwgraph, dest_vertex);

	return(rv);
}



/*
** See if a vertex has an outgoing edge with a specified name.
** Vertices in the hwgraph *implicitly* contain these edges:
**	"." 	refers to "current vertex"
**	".." 	refers to "connect point vertex"
**	"char"	refers to current vertex (character device access)
**	"block"	refers to current vertex (block device access)
*/
graph_error_t
hwgraph_edge_get(vertex_hdl_t from, char *name, vertex_hdl_t *toptr)
{
	graph_error_t rv = GRAPH_NOT_FOUND;
	vertex_hdl_t target = GRAPH_VERTEX_NONE;

	if (name == NULL)
		return(GRAPH_BAD_PARAM);

	if (!strcmp(name, HWGRAPH_EDGELBL_DOT)) {
		if (toptr) {
			(void)graph_vertex_ref(hwgraph, from);
			*toptr = from;
		}
		return(GRAPH_SUCCESS);
	} else if (!strcmp(name, HWGRAPH_EDGELBL_DOTDOT)) {
		target = hwgraph_connectpt_get(from);
		if (target == GRAPH_VERTEX_NONE)
			return(GRAPH_NOT_FOUND);
		if (toptr)
			*toptr = target;
		return(GRAPH_SUCCESS);
	} else {
		rv = graph_edge_get(hwgraph, from, name, toptr);
		return(rv);
	}
}



/*
 * Allow caller to walk through edges of a hwgraph vertex.  This routine
 * first returns the implicit edges and then the explicit ("real") edges.
 */
graph_error_t
hwgraph_edge_get_next(	vertex_hdl_t source,
			char *buf,
			vertex_hdl_t *target,
			graph_edge_place_t *placeptr)
{
	graph_edge_place_t which_place;

	if (placeptr == NULL)
		return(GRAPH_BAD_PARAM);

	which_place = *placeptr;

again:
	if (which_place <= HWGRAPH_RESERVED_PLACES) {
		if (which_place == EDGE_PLACE_WANT_CURRENT) {
			if (buf != NULL)
				strcpy(buf, HWGRAPH_EDGELBL_DOT);
	
			if (target != NULL) {
				*target = source;
				(void)graph_vertex_ref(hwgraph, source);
			}
	
		} else if (which_place == EDGE_PLACE_WANT_CONNECTPT) {
			vertex_hdl_t connect_point = hwgraph_connectpt_get(source);
	
			if (connect_point == GRAPH_VERTEX_NONE) {
				which_place++;
				goto again;
			}
	
			if (buf != NULL)
				strcpy(buf, HWGRAPH_EDGELBL_DOTDOT);
	
			if (target != NULL)
				*target = connect_point;
			else
				(void)graph_vertex_unref(hwgraph, connect_point);
	
		} else if (which_place == EDGE_PLACE_WANT_REAL_EDGES) {
			graph_error_t rv;
			graph_edge_place_t locplace = GRAPH_EDGE_PLACE_NONE;
	
			rv = graph_edge_get_next(hwgraph, source, buf, target, &locplace);
			if (rv != GRAPH_SUCCESS)
				return(rv);
	
			*placeptr = locplace;
			return(GRAPH_SUCCESS);
		}

		*placeptr = which_place+1;
		return(GRAPH_SUCCESS);
	}

	return(graph_edge_get_next(hwgraph, source, buf, target, placeptr));
}



/***************************************************************************/
/* Operations on LABELLED INFORMATION: add, remove, replace, get, get_next */
/***************************************************************************/

graph_error_t
hwgraph_info_add_LBL(	vertex_hdl_t vhdl,
			char *name,
			arbitrary_info_t info)
{
	return(graph_info_add_LBL(hwgraph, vhdl, name, INFO_DESC_PRIVATE, info));
}



graph_error_t
hwgraph_info_remove_LBL(	vertex_hdl_t vhdl,
				char *name,
				arbitrary_info_t *old_info)
{
	return(graph_info_remove_LBL(hwgraph, vhdl, name, NULL, old_info));
}



graph_error_t
hwgraph_info_replace_LBL(	vertex_hdl_t vhdl,
				char *name,
				arbitrary_info_t info,
				arbitrary_info_t *old_info)
{
	return(graph_info_replace_LBL(hwgraph, vhdl, name,
			INFO_DESC_PRIVATE, info,
			NULL, old_info));
}



graph_error_t
hwgraph_info_get_LBL(	vertex_hdl_t vhdl,
			char *name,
			arbitrary_info_t *infop)
{
	return(graph_info_get_LBL(hwgraph, vhdl, name, NULL, infop));
}


graph_error_t
hwgraph_info_get_exported_LBL(	vertex_hdl_t vhdl,
				char *name,
				int *export_info,
				arbitrary_info_t *infop)
{
	graph_error_t rc;
	arb_info_desc_t info_desc;

	rc = graph_info_get_LBL(hwgraph, vhdl, name, &info_desc, infop);
	if (rc == GRAPH_SUCCESS)
		*export_info = (int)info_desc;

	return(rc);
}



graph_error_t
hwgraph_info_get_next_LBL(	vertex_hdl_t source,
				char *buf,
				arbitrary_info_t *infop,
				graph_info_place_t *place)
{
	return(graph_info_get_next_LBL(hwgraph, source, buf, NULL, infop, place));
}


/* ARGSUSED */
graph_error_t
hwgraph_info_get_next_exported_LBL(	vertex_hdl_t vhdl,
					char *info_name,
					int *export_info,
					arbitrary_info_t *infop,
					graph_info_place_t *placep)
{
	/* TBD, if we ever choose to support attr_list in hwgfs */
	return(GRAPH_ILLEGAL_REQUEST);
}


graph_error_t
hwgraph_info_export_LBL(vertex_hdl_t vhdl, char *name, int nbytes)
{
	arbitrary_info_t info;
	graph_error_t rc;

	if (nbytes == 0)
		nbytes = INFO_DESC_EXPORT;

	if (nbytes < 0)
		return(GRAPH_BAD_PARAM);

	rc = graph_info_get_LBL(hwgraph, vhdl, name, NULL, &info);
	if (rc != GRAPH_SUCCESS)
		return(rc);

	rc = graph_info_replace_LBL(hwgraph, vhdl, name,
				nbytes, info, NULL, NULL);

	return(rc);
}


graph_error_t
hwgraph_info_unexport_LBL(vertex_hdl_t vhdl, char *name)
{
	arbitrary_info_t info;
	graph_error_t rc;

	rc = graph_info_get_LBL(hwgraph, vhdl, name, NULL, &info);
	if (rc != GRAPH_SUCCESS)
		return(rc);

	rc = graph_info_replace_LBL(hwgraph, vhdl, name,
				INFO_DESC_PRIVATE, info, NULL, NULL);

	return(rc);
}


/******************************************************************/
/* Operations on locator STRINGS: get_component, lookup, path_add,*/
/* traverse, path_to_dev, add_device                              */
/******************************************************************/

graph_error_t
hwgraph_path_get_component(	char *path,
				char *component,
				int *separator_length,
				int *component_length)
{
	return(graph_path_get_component(hwgraph, path, component,
				separator_length, component_length));
}



/*
** This routine handles basic path traversal.
**
** Trace the path specified by components of lookup_path starting
** at start_vertex and following edge labels.  If the path cannot be
** completely resolved, return the remainder of the lookup_path and
** the last vertex reached in the lookup.  Components of lookup_path
** must be separated by the separator character specified when the
** graph was created.
*/
graph_error_t
hwgraph_path_lookup(	vertex_hdl_t start_vertex_handle,
			char *lookup_path,
			vertex_hdl_t *vertex_handle_ptr,
			char **remainder)
{
	graph_error_t rv;
	vertex_hdl_t new_vertex_handle;

	ASSERT(hwgraph_initted);

	if (start_vertex_handle == GRAPH_VERTEX_NONE)
		return(GRAPH_BAD_PARAM);

 	new_vertex_handle = start_vertex_handle;
	graph_vertex_ref(hwgraph, new_vertex_handle);

	if (lookup_path == NULL) {
		rv = GRAPH_SUCCESS;
		goto out;
	}

	/*
	 * Walk through lookup_path traversing edges along the way.
	 */
	for(;;) {
		char component[LABEL_LENGTH_MAX];
		int separator_length;
		int component_length;

		/*
		 * Pull out the next piece of the lookup_path.
		 */
		rv = graph_path_get_component(	hwgraph,
						lookup_path,
						component,
						&separator_length,
						&component_length);
		if (rv != GRAPH_SUCCESS) {
			graph_vertex_unref(hwgraph, new_vertex_handle);
			return(rv);
		}

		lookup_path += separator_length;

		/*
		 * If finished parsing path, exit loop.
		 */
		if (component_length == 0) {
			break;
		}

		/*
		 * Otherwise, try to follow the edge.
		 */
		rv = hwgraph_edge_get(	start_vertex_handle,
		   			component,
		   			&new_vertex_handle);

		if (rv != GRAPH_SUCCESS)
			break;

		graph_vertex_unref(hwgraph, start_vertex_handle);

		start_vertex_handle = new_vertex_handle;
		lookup_path += component_length;
	}

	if (remainder != NULL)
		*remainder = lookup_path;

out:
	if (vertex_handle_ptr != NULL)
		*vertex_handle_ptr = new_vertex_handle;
	else
		(void)graph_vertex_unref(hwgraph, new_vertex_handle);

	return(rv);
}



/*
** Add vertices with proper edge labels so that path reaches a
** vertex if the search starts at ovhdl.
*/
graph_error_t
hwgraph_path_add(	vertex_hdl_t fromv,
			char *path,
			vertex_hdl_t *new_vhdl)
{
	vertex_hdl_t ovhdl;
	int separator_length, component_length;
	char name[LABEL_LENGTH_MAX];
	graph_error_t rv;

	ASSERT(hwgraph_initted);

	if (path == NULL)
		return(GRAPH_BAD_PARAM);

	if (fromv == GRAPH_VERTEX_NONE)
		fromv = hwgraph_root;

	hwgraph_vertex_ref(fromv);

	while (1) {
		/* Loop invariant: fromv's reference count is incremented */
		ovhdl = fromv;
		rv=graph_path_get_component(hwgraph, path, name,
					      &separator_length, &component_length);

		if (rv != GRAPH_SUCCESS) {
			hwgraph_vertex_unref(ovhdl);
			return(rv);
		}

		/* Done with path! */
		if (component_length == 0)
			break;

		path += separator_length;
		path += component_length;
		/*
		 * if this edge already exists,
		 * don't bother creating the
		 * new vertex and edge.
		 */
		
		if (hwgraph_traverse(ovhdl, name, &fromv) == GRAPH_SUCCESS) {
			hwgraph_vertex_unref(ovhdl);
			continue;
		}

		/*
		 * Create a vertex for the next component in the path.
		 */
		if ((rv=hwgraph_vertex_create(&fromv)) != GRAPH_SUCCESS) {
			hwgraph_vertex_unref(ovhdl);
			return(rv);
		}

		if ((rv = hwgraph_edge_add(ovhdl, fromv, name)) != GRAPH_SUCCESS) {


			(void)hwgraph_vertex_destroy(fromv);

			/*
			 * It is entirely possible that someone else
			 * added this edge while we were creating our
			 * vertex. If it exists, step onward to
			 * the vertex they put there and continue
			 * parsing the path.
			 */
		
			if (hwgraph_traverse(ovhdl, name, &fromv) == GRAPH_SUCCESS) {
				hwgraph_vertex_unref(ovhdl);
				continue;
			}

			/* Something strange went wrong. Return error. */
			hwgraph_vertex_unref(ovhdl);
			return(rv);
		} else {
			hwgraph_vertex_unref(ovhdl);
			hwgraph_vertex_ref(fromv); /* preserve loop invariant */
		}
	}

	if (new_vhdl != NULL)
		*new_vhdl = fromv;
	else
		hwgraph_vertex_unref(fromv);

	return(GRAPH_SUCCESS);
}



/*
** Given a starting vertex in the graph and a path, traverse
** the graph according to components in the path.  If we get through
** the whole path, return GRAPH_SUCCESS with the vertex we ended at
** in "found".  If we don't get through the whole path, return
** GRAPH_NOT_FOUND. NOTE: if we do not succeed, the word at "found"
** is left entirely unmodified.
**
** Traversal is accomplished using path_lookup.  When path_lookup gets
** stuck, we check for a registered traverse routine, and continue the
** graph traversal if we can.  This allows device drivers to control a
** portion of the hwgraph name space.
**
** XXX: We don't currently handle unloadable modules with a traverse fn.
** (If the driver wants to unload, it needs to handle the traverse fn.)
*/
graph_error_t
hwgraph_traverse(vertex_hdl_t vhdl, char *path, vertex_hdl_t *found)
{
	char *remainder = path;
	graph_error_t rv;
	traverse_fn_t traverse_fn;
	vertex_hdl_t ovhdl = GRAPH_VERTEX_NONE;
	int cutoff = 100;

	ASSERT(hwgraph_initted);
	ASSERT(found != NULL);

	if (vhdl == GRAPH_VERTEX_NONE)
		vhdl = hwgraph_root;

	for (;;) {
		char *oremainder;

		if ((rv = hwgraph_path_lookup(vhdl, remainder, &vhdl, &remainder)) == GRAPH_SUCCESS) {
			/* made it all the way through */
			ASSERT(vhdl != GRAPH_VERTEX_NONE);
			*found = vhdl;
			rv = GRAPH_SUCCESS;
			break;
		}

		if (rv != GRAPH_NOT_FOUND) /* unexpected error */
			break;

		ovhdl = vhdl;

		/*
		 * We couldn't get all the way through the path, so see if
		 * a traverse function has been registered.  If so, give
		 * it a chance to continue the path traversal.
		 */
		rv = graph_info_get_LBL(hwgraph, vhdl, INFO_LBL_TRAVERSE,
				NULL, (arbitrary_info_t *)&traverse_fn);
		if (rv != GRAPH_SUCCESS) {
			rv = GRAPH_NOT_FOUND;
			break;
		}

		oremainder = remainder;
		if ((!traverse_fn(ovhdl, &remainder, &vhdl)) ||
		    (vhdl == GRAPH_VERTEX_NONE)) {
			rv = GRAPH_NOT_FOUND;
			break;
		}

		/* Assure forward progress */
		if (((ovhdl == vhdl) && (oremainder == remainder)) ||
		    (cutoff-- == 0)) {
			cmn_err(CE_WARN, "hwgraph_traverse: traversal halted due to improper traverse routine (0x%lx)\n", traverse_fn);
			cmn_err(CE_WARN, "vhdl=0x%x remainder=%s\n", vhdl, remainder);
			rv = GRAPH_NOT_FOUND;
			break;
		}

		ovhdl = vhdl;
	}

	if (ovhdl != GRAPH_VERTEX_NONE)
		(void)graph_vertex_unref(hwgraph, ovhdl);
	return(rv);
}


/*
** Translate a path to a vertex_hdl_t.
**
** Given a path, return the corresponding vertex or GRAPH_VERTEX_NONE
** if there is no corresponding vertex.
*/
vertex_hdl_t
hwgraph_path_to_vertex(char *path)
{
	vertex_hdl_t vhdl;
	graph_error_t rv;
	char component[LABEL_LENGTH_MAX];
	int separator_length;
	int component_length;

	/*
	 * If path begins with "/hw", strip it off.
	 * By convention, hwgraph paths begin with "/hw".
	 */
	rv = graph_path_get_component(hwgraph, path, component,
					&separator_length, &component_length);

	if (rv != GRAPH_SUCCESS)
		return(GRAPH_VERTEX_NONE);

	if (!strcmp(component, HWGRAPH_EDGELBL_HW))
		path += separator_length + component_length;

	if (hwgraph_traverse(hwgraph_root, path, &vhdl) == GRAPH_SUCCESS) {
		return(vhdl);
	} else
		return(GRAPH_VERTEX_NONE);
}


/*
** Translate a path to a dev_t.
**
** Given a path, return the corresponding dev or NODEV if the path
** cannot be fully resolved.
*/
dev_t
hwgraph_path_to_dev(char *path)
{
	vertex_hdl_t vhdl;

	vhdl = hwgraph_path_to_vertex(path);

	if (vhdl == GRAPH_VERTEX_NONE)
		return(NODEV);
	else
		return(vhdl_to_dev(vhdl));
}


/*
 * hwgraph_link_add: if "dst" is a path to a
 * valid vertex, find (or create) the vertex
 * at "src" and add an edge called "edge" from
 * "src" to "dst".
 *
 * In other words, set up a convenience link
 * if the target exists.
 */
void
hwgraph_link_add(char *dst, char *src, char *edge)
{
	vertex_hdl_t		dvtx;
	vertex_hdl_t		svtx;

	if ((GRAPH_SUCCESS ==
	     hwgraph_traverse(hwgraph_root, dst, &dvtx)) &&
	    (GRAPH_SUCCESS ==
	     hwgraph_path_add(hwgraph_root, src, &svtx)))
		(void)hwgraph_edge_add(svtx, dvtx, edge);
}



/*
** Add block and/or char devices -- whatever the driver supports -- at a
** specified vertex.  Returns void; caller can tell what failed/succeeded
** by checking "out" values (devhdl, blockv, charv).  This is really just
** a convenience wrapper.
*/
void
hwgraph_device_add(	vertex_hdl_t from,
			char *path,
			char *prefix,
			vertex_hdl_t *devhdl,
			vertex_hdl_t *blockv,
			vertex_hdl_t *charv)
{
	vertex_hdl_t nvhdl;


	if (hwgraph_path_add(from, path, &nvhdl) != GRAPH_SUCCESS)
		return;

	if (devhdl)
		*devhdl = nvhdl;



	hwgraph_block_device_add(nvhdl, EDGE_LBL_BLOCK, prefix, blockv);
	hwgraph_char_device_add(nvhdl, EDGE_LBL_CHAR, prefix, charv);

	return;
}

/*
** Add a block device -- if the driver supports it -- at a specified vertex.
*/
graph_error_t
hwgraph_block_device_add(	vertex_hdl_t from,
				char *path,
				char *prefix,
				vertex_hdl_t *devhdl)
{
	graph_error_t rv;
	vertex_hdl_t nvhdl;
	device_driver_t driver;
	struct bdevsw *my_bdevsw;
	struct cdevsw *my_cdevsw;

	ASSERT(hwgraph_initted);

	/* See if this driver provides a block interface */
	driver = device_driver_get(prefix);
	device_driver_devsw_get(driver, &my_bdevsw, &my_cdevsw);
	if (!my_bdevsw)
		return(GRAPH_ILLEGAL_REQUEST);

	if (from == GRAPH_VERTEX_NONE)
		from = hwgraph_root;

	/* Add the device to the name space (or find it) */
	if (path == NULL)
		nvhdl = from;
	else {
		rv = hwgraph_path_add(from, path, &nvhdl);
		if (rv != GRAPH_SUCCESS) {
			if (devhdl)
				*devhdl = GRAPH_VERTEX_NONE;
			return(rv);
		}
	}

	/* Verify that this vertex has no children */
	{
		vertex_hdl_t foundv;

		graph_edge_place_t place = EDGE_PLACE_WANT_REAL_EDGES;
		rv = hwgraph_edge_get_next(nvhdl, NULL, &foundv, &place);
		if (rv == GRAPH_SUCCESS)
			graph_vertex_unref(hwgraph, foundv);
		if (rv != GRAPH_HIT_LIMIT) {
			/* Unref the reference  got thru hwgraph_path_add
			 *  above
			 */
			if (path != NULL)
				graph_vertex_unref(hwgraph, nvhdl);
			return(GRAPH_ILLEGAL_REQUEST);
		}
	}

	/* Hook the specified bdevsw into the new vertex */
	(void)hwgraph_bdevsw_set(nvhdl, my_bdevsw);

	if (devhdl) {
		*devhdl = nvhdl;
		(void)graph_vertex_ref(hwgraph, nvhdl);
	}
	/* Unref the reference  got thru hwgraph_path_add
	 *  above
	 */
	if (path != NULL)
		graph_vertex_unref(hwgraph, nvhdl);

#if DEBUG && HWG_DEBUG
cmn_err(CE_CONT, "%v is block driver '%s'\n",
	nvhdl, prefix ? prefix : "(null)");
#endif

	return(GRAPH_SUCCESS);
}



/*
** Add a char device -- if the driver supports it -- at a specified vertex.
*/
graph_error_t
hwgraph_char_device_add(	vertex_hdl_t from,
				char *path,
				char *prefix,
				vertex_hdl_t *devhdl)
{
	graph_error_t rv;
	vertex_hdl_t nvhdl;
	device_driver_t driver;
	struct bdevsw *my_bdevsw;
	struct cdevsw *my_cdevsw;
	
	ASSERT(hwgraph_initted);



	/* See if this driver provides a char interface */
	driver = device_driver_get(prefix);
	device_driver_devsw_get(driver, &my_bdevsw, &my_cdevsw);
	if (!my_cdevsw)
		return(GRAPH_ILLEGAL_REQUEST);

	if (from == GRAPH_VERTEX_NONE)
		from = hwgraph_root;

	/* Add the device to the name space (or find it) */
	if (path == NULL)
		nvhdl = from;
	else {
		rv = hwgraph_path_add(from, path, &nvhdl);
		if (rv != GRAPH_SUCCESS) {
			if (devhdl)
				*devhdl = GRAPH_VERTEX_NONE;
			return(rv);
		}
	}

	/* Verify that this vertex has no children */
	{
		vertex_hdl_t foundv;

		graph_edge_place_t place = EDGE_PLACE_WANT_REAL_EDGES;
		rv = hwgraph_edge_get_next(nvhdl, NULL, &foundv, &place);
		if (rv == GRAPH_SUCCESS)
			graph_vertex_unref(hwgraph, foundv);
		if (rv != GRAPH_HIT_LIMIT) {
			/* Unref the reference  got thru hwgraph_path_add
			 *  above
			 */
			if (path != NULL)
				graph_vertex_unref(hwgraph, nvhdl);
			return(GRAPH_ILLEGAL_REQUEST);
		}
	}

	/* Hook the specified cdevsw into the new vertex */
	(void)hwgraph_cdevsw_set(nvhdl, my_cdevsw);
	
	if (devhdl) {
		*devhdl = nvhdl;
		(void)graph_vertex_ref(hwgraph, nvhdl);
	}
	/* Unref the reference  got thru hwgraph_path_add above */
	if (path != NULL)
		graph_vertex_unref(hwgraph,nvhdl);
#if DEBUG && HWG_DEBUG
cmn_err(CE_CONT, "%v is char driver '%s'\n",
	nvhdl, prefix ? prefix : "(null)");
#endif

	return(GRAPH_SUCCESS);
}



/*
** Given a vertex, return the associated block device, if one exists.
*/
vertex_hdl_t
hwgraph_block_device_get(vertex_hdl_t vhdl)
{
	vertex_hdl_t bvhdl;

	if (hwgraph_bdevsw_get(vhdl) != NULL)
		return(vhdl);

	if (hwgraph_edge_get(vhdl, EDGE_LBL_BLOCK, &bvhdl) == GRAPH_SUCCESS)
		return(bvhdl);
	else
		return(GRAPH_VERTEX_NONE);
}


/*
** Given a vertex, return the associated char device, if one exists.
*/
vertex_hdl_t
hwgraph_char_device_get(vertex_hdl_t vhdl)
{
	vertex_hdl_t cvhdl;

	if (hwgraph_cdevsw_get(vhdl) != NULL)
		return(vhdl);

	if (hwgraph_edge_get(vhdl, EDGE_LBL_CHAR, &cvhdl) == GRAPH_SUCCESS)
		return(cvhdl);
	else
		return(GRAPH_VERTEX_NONE);
}





/*******************************************************************/
/* set/get routines for indexed information and predefined labels: */
/* devsw, traverse routine, and fastinfo                           */
/*******************************************************************/

#define CDEVSW_TO_INFO(ptr) ((arbitrary_info_t)(ptr))
#define INFO_TO_CDEVSW(info) ((struct cdevsw *)(((uintptr_t)(info) & 1) ? NULL : info))

#define BDEVSW_TO_INFO(ptr) ((arbitrary_info_t)((uintptr_t)(ptr) | 1))
#define INFO_TO_BDEVSW(info) ((struct bdevsw *)(((uintptr_t)(info) & 1) ? \
					((uintptr_t)(info) & ~1L) : NULL))

/*
** Associate the specified cdevsw entry with the specified device.
*/
int
hwgraph_cdevsw_set(vertex_hdl_t vhdl, struct cdevsw *devsw_entry)
{
	ASSERT(hwgraph_initted);

	/* Ensure the dev switch has a valid CPU */
	if (devsw_entry && !cpu_isvalid(devsw_entry->d_cpulock))
		devsw_entry->d_cpulock = masterpda->p_cpuid;

	if (graph_info_replace_IDX(hwgraph,  vhdl,
		HWGRAPH_DEVSW, CDEVSW_TO_INFO(devsw_entry), NULL) != GRAPH_SUCCESS)
		return(-1);

	return(0);
}

/*
** Retrieve the cdevsw entry for a given vertex.
*/
struct cdevsw *
hwgraph_cdevsw_get(vertex_hdl_t vhdl)
{
	arbitrary_info_t devsw_entry;
	graph_error_t rv;

	ASSERT(hwgraph_initted);

	rv = graph_info_get_IDX(hwgraph, vhdl, HWGRAPH_DEVSW, &devsw_entry);

	if (rv == GRAPH_SUCCESS)
		return(INFO_TO_CDEVSW(devsw_entry));
	else
		return(NULL);
}


/*
** Associate the specified bdevsw entry with the specified device.
*/
int
hwgraph_bdevsw_set(vertex_hdl_t vhdl, struct bdevsw *devsw_entry)
{
	ASSERT(hwgraph_initted);

	/* Ensure the dev switch has a valid CPU */
	if (devsw_entry && !cpu_isvalid(devsw_entry->d_cpulock))
		devsw_entry->d_cpulock = masterpda->p_cpuid;

	if (graph_info_replace_IDX(hwgraph,  vhdl,
		HWGRAPH_DEVSW, BDEVSW_TO_INFO(devsw_entry), NULL) != GRAPH_SUCCESS)
		return(-1);

	return(0);
}

/*
** Retrieve the bdevsw entry for a given vertex.
*/
struct bdevsw *
hwgraph_bdevsw_get(vertex_hdl_t vhdl)
{
	arbitrary_info_t devsw_entry;
	graph_error_t rv;

	ASSERT(hwgraph_initted);

	rv = graph_info_get_IDX(hwgraph, vhdl, HWGRAPH_DEVSW, &devsw_entry);

	if (rv == GRAPH_SUCCESS)
		return(INFO_TO_BDEVSW(devsw_entry));
	else
		return(NULL);
}


/*
** Establish a traverse function for a specified device.  If traverse_fn
** is NULL, remove the traverse function.  The caller should insure that
** there is no race between multiple processes trying to establish a
** traverse routine for a given vertex.  If there is such a race, it's
** possible that this routine will fail and return an error.
*/
graph_error_t
hwgraph_traverse_set(vertex_hdl_t vhdl, traverse_fn_t traverse_fn)
{
	graph_error_t rv;

	ASSERT(hwgraph_initted);

	if (traverse_fn == NULL)
		rv = graph_info_remove_LBL(hwgraph, vhdl, INFO_LBL_TRAVERSE, NULL, NULL);
	else {
		rv = graph_info_add_LBL(hwgraph, vhdl,
			INFO_LBL_TRAVERSE, INFO_DESC_PRIVATE,
			(arbitrary_info_t)traverse_fn);

		if (rv == GRAPH_DUP) {
			/* Already had a traverse, so replace it */
			rv = graph_info_replace_LBL( hwgraph, vhdl,
					INFO_LBL_TRAVERSE,
					INFO_DESC_PRIVATE, (arbitrary_info_t)traverse_fn,
					NULL, NULL);
		}
	}

	return(rv);
}

traverse_fn_t
hwgraph_traverse_get(vertex_hdl_t vhdl)
{
	traverse_fn_t traverse_fn;
	graph_error_t rv;

	ASSERT(hwgraph_initted);

	rv = graph_info_get_LBL(hwgraph, vhdl, INFO_LBL_TRAVERSE,
			NULL, (arbitrary_info_t *)&traverse_fn);
	if (rv != GRAPH_SUCCESS)
		return(NULL);

	return(traverse_fn);
}


/*
 * Set vertex-specific "fast information".
 */
void
hwgraph_fastinfo_set(vertex_hdl_t vhdl, arbitrary_info_t fastinfo)
{
	ASSERT(hwgraph_initted);

	graph_info_replace_IDX(hwgraph, vhdl, HWGRAPH_FASTINFO, fastinfo, NULL);
}


/*
 * Get vertex-specific "fast information".
 */
arbitrary_info_t
hwgraph_fastinfo_get(vertex_hdl_t vhdl)
{
	arbitrary_info_t fastinfo;
	graph_error_t rv;

	ASSERT(hwgraph_initted);

	rv = graph_info_get_IDX(hwgraph, vhdl, HWGRAPH_FASTINFO, &fastinfo);
	if (rv == GRAPH_SUCCESS)
		return(fastinfo);

	return(NULL);
}


/*************************/
/* Support for INVENTORY */
/*************************/

invplace_t invplace_none = {
	GRAPH_VERTEX_NONE,
	GRAPH_VERTEX_PLACE_NONE,
	NULL
};

/*
** Inventory is now associated with a vertex in the graph.  For items that
** belong in the inventory but have no vertex (e.g. old non-graph-aware drivers),
** we create a bogus vertex under the INFO_LBL_INVENT name.
**
** For historical reasons, we prevent exact duplicate entries from being added
** to a single vertex.
*/
graph_error_t
hwgraph_inventory_add(	vertex_hdl_t vhdl,
			int class,
			int type,
			major_t controller,
			minor_t unit,
			int state)
{
	inventory_t *pinv = NULL, *old_pinv = NULL, *last_pinv = NULL;
	graph_error_t rv;

	ASSERT(hwgraph_initted);

	if (vhdl == GRAPH_VERTEX_NONE) {
		if ((rv=hwgraph_path_add(hwgraph_root, ".invent", &vhdl)) != GRAPH_SUCCESS)
			return(rv);
		else
			(void)graph_vertex_unref(hwgraph, vhdl);
	}

	/*
	 * Add our inventory data to the list of inventory data
	 * associated with this vertex.
	 */
again:
	mrupdate(&invent_mrlock);
	rv = graph_info_get_LBL(hwgraph, vhdl,
			INFO_LBL_INVENT,
			NULL, (arbitrary_info_t *)&old_pinv);
	if ((rv != GRAPH_SUCCESS) && (rv != GRAPH_NOT_FOUND))
		goto failure;

	ASSERT((rv == GRAPH_NOT_FOUND) || (old_pinv != NULL));

	/*
	 * Seek to end of inventory items associated with this
	 * vertex.  Along the way, make sure we're not duplicating
	 * an inventory item (for compatibility with old add_to_inventory)
	 */
	for (;old_pinv; last_pinv = old_pinv, old_pinv = old_pinv->inv_next) {
		if ((int)class != -1 && old_pinv->inv_class != class)
			continue;
		if ((int)type != -1 && old_pinv->inv_type != type)
			continue;
		if ((int)state != -1 && old_pinv->inv_state != state)
			continue;
		if ((int)controller != -1
		    && old_pinv->inv_controller != controller)
			continue;
		if ((int)unit != -1 && old_pinv->inv_unit != unit)
			continue;

		/* exact duplicate of previously-added inventory item */
		rv = GRAPH_DUP;
		goto failure;
	}

	/* Not a duplicate, so we know that we need to add something. */
	if (pinv == NULL) {
		/* Release lock while we wait for memory. */
		mrunlock(&invent_mrlock);
		pinv = (inventory_t *)kern_malloc(sizeof(inventory_t));
		replace_in_inventory(pinv, class, type, controller, unit, state);
		goto again;
	}

	pinv->inv_next = NULL;
	if (last_pinv) {
		last_pinv->inv_next = pinv;
	} else {
		rv = graph_info_add_LBL(hwgraph, vhdl,
			INFO_LBL_INVENT, sizeof(inventory_t),
			(arbitrary_info_t)pinv);

		if (rv != GRAPH_SUCCESS)
			goto failure;
	}

	mrunlock(&invent_mrlock);
	return(GRAPH_SUCCESS);

failure:
	mrunlock(&invent_mrlock);
	if (pinv)
		kern_free(pinv);
	return(rv);
}


/*
** Remove an inventory item associated with a vertex.   It is the caller's
** responsibility to make sure that there are no races between removing
** inventory from a vertex and simultaneously removing that vertex.
*/
graph_error_t
hwgraph_inventory_remove(	vertex_hdl_t vhdl,
				int class,
				int type,
				major_t controller,
				minor_t unit,
				int state)
{
	inventory_t *pinv = NULL, *last_pinv = NULL, *next_pinv = NULL;
	graph_error_t rv;

	/*
	 * We only allow inventory removal if the item was added to a
	 * particular vertex.  We'll never remove stuff from ".invent".
	 */
	ASSERT(vhdl != GRAPH_VERTEX_NONE);

	/*
	 * Remove our inventory data to the list of inventory data
	 * associated with this vertex.
	 */
	mrupdate(&invent_mrlock);
	rv = graph_info_get_LBL(hwgraph, vhdl,
			INFO_LBL_INVENT,
			NULL, (arbitrary_info_t *)&pinv);
	if (rv != GRAPH_SUCCESS)
		goto failure;

	/*
	 * Search through inventory items associated with this
	 * vertex, looking for a match.
	 */
	for (;pinv; pinv = next_pinv) {
		next_pinv = pinv->inv_next;

		if(((int)class == -1 || pinv->inv_class == class) &&
		   ((int)type == -1 || pinv->inv_type == type) &&
		   ((int)state == -1 || pinv->inv_state == state) &&
		   ((int)controller == -1 || pinv->inv_controller == controller) &&
		   ((int)unit == -1 || pinv->inv_unit == unit)) {

			/* Found a matching inventory item. Remove it. */
			if (last_pinv) {
				last_pinv->inv_next = pinv->inv_next;
			} else {
				rv = hwgraph_info_replace_LBL(vhdl, INFO_LBL_INVENT, (arbitrary_info_t)pinv->inv_next, NULL);
				if (rv != GRAPH_SUCCESS)
					goto failure;
			}

			pinv->inv_next = NULL; /* sanity */
			kern_free(pinv);
		} else
			last_pinv = pinv;
	}

	if (last_pinv == NULL) {
		rv = hwgraph_info_remove_LBL(vhdl, INFO_LBL_INVENT, NULL);
		if (rv != GRAPH_SUCCESS)
			goto failure;
	}

	rv = GRAPH_SUCCESS;

failure:
	mrunlock(&invent_mrlock);
	return(rv);
}

/*
** Get next inventory item associated with the specified vertex.
**
** No locking is really needed.  We don't yet have the ability
** to remove inventory items, and new items are always added to
** the end of a vertex' inventory list.
**
** TBD: Handle locking for case when a vertex is removed in the
** middle of reading its inventory.
*/
graph_error_t
hwgraph_inventory_get_next(vertex_hdl_t vhdl, invplace_t *place, inventory_t **ppinv)
{
	inventory_t *pinv;
	graph_error_t rv;

	ASSERT(hwgraph_initted);
	ASSERT(ppinv != NULL);
	ASSERT(place != NULL);

	if (vhdl == GRAPH_VERTEX_NONE)
		return(GRAPH_BAD_PARAM);

	if (place->invplace_vhdl == GRAPH_VERTEX_NONE) {
		place->invplace_vhdl = vhdl;
		place->invplace_inv = NULL;
	}

	if (vhdl != place->invplace_vhdl)
		return(GRAPH_BAD_PARAM);

	if (place->invplace_inv == NULL) {
		/* Just starting on this vertex */
		rv = graph_info_get_LBL(hwgraph, vhdl, INFO_LBL_INVENT,
						NULL, (arbitrary_info_t *)&pinv);
		if (rv != GRAPH_SUCCESS)
			return(GRAPH_NOT_FOUND);

	} else {
		/* Advance to next item on this vertex */
		pinv = place->invplace_inv->inv_next;
	}
	place->invplace_inv = pinv;
	*ppinv = pinv;

	return(GRAPH_SUCCESS);
}

int
hwgraph_controller_num_get(dev_t device)
{
	inventory_t *pinv;
	invplace_t invplace = INVPLACE_NONE;
	int val = -1;
	if ((pinv = device_inventory_get_next(device, &invplace)) != NULL) {
		val = (pinv->inv_class == INV_NETWORK)? pinv->inv_unit: pinv->inv_controller;
	}
#ifdef DEBUG
	/*
	 * It does not make any sense to call this on vertexes with multiple
	 * inventory structs chained together
	 */
	ASSERT(device_inventory_get_next(device, &invplace) == NULL);
#endif
	return (val);	
}

void
hwgraph_controller_num_set(dev_t device, int contr_num)
{
	inventory_t *pinv;
	invplace_t invplace = INVPLACE_NONE;
	if ((pinv = device_inventory_get_next(device, &invplace)) != NULL) {
		if (pinv->inv_class == INV_NETWORK)
			pinv->inv_unit = contr_num;
		else {
			if (pinv->inv_class == INV_FCNODE)
				pinv = device_inventory_get_next(device, &invplace);
			if (pinv != NULL)
				pinv->inv_controller = contr_num;
		}
	}
#ifdef DEBUG
	/*
	 * It does not make any sense to call this on vertexes with multiple
	 * inventory structs chained together
	 */
	if(pinv != NULL)
		ASSERT(device_inventory_get_next(device, &invplace) == NULL);
#endif
}

/****************************/
/* Support for file systems */
/****************************/

/*
** Convert from a vnode to a dev_t by looking up the path associated with
** the vnode.  A path is stored in a file-system dependent way; but filesystems
** must support attr_get and attr_set for the _DEVNAME_ATTR attribute.
*/
dev_t
hwgraph_vnode_to_dev(struct vnode *vp, struct cred *cr)
{
	dev_t dev = NODEV;
	register int error;
	int maxsz = MAXDEVNAME;

	/*
	 * If allocating MAXDEVNAME bytes of stack space in this routine
	 * turns out to be a problem, we can revert to dynamic allocation.
	 * It's more efficient this way, though.
	 */
	char linkpath[MAXDEVNAME];


	ASSERT((vp->v_type == VCHR) || (vp->v_type == VBLK));

	/*
	 * The device path for a special file is stored as a user-visible attribute.
	 */
	VOP_RWLOCK(vp, VRWLOCK_READ);
	VOP_ATTR_GET(vp, _DEVNAME_ATTR, linkpath, &maxsz, ATTR_ROOT, cr, error);
	VOP_RWUNLOCK(vp, VRWLOCK_READ);

	if (!error)
		dev = hwgraph_path_to_dev(linkpath);

	return(dev);
}



/*
** Associate the specified vertex as the connect point for a source vertex.
*/
static graph_error_t
do_hwgraph_connectpt_set(vertex_hdl_t vhdl, vertex_hdl_t connect_vertex)
{
	arbitrary_info_t old_info;
	vertex_hdl_t old_vertex;
	graph_error_t rv;

	ASSERT(hwgraph_initted);

	if (vhdl == hwgraph_root)
		return(GRAPH_ILLEGAL_REQUEST);

	rv = graph_info_replace_IDX(hwgraph, vhdl, HWGRAPH_CONNECTPT,
			(arbitrary_info_t)connect_vertex, &old_info);

	if (rv != GRAPH_SUCCESS) {
		return(rv);
	}

	if (connect_vertex != GRAPH_VERTEX_NONE)
		(void)graph_vertex_ref(hwgraph, connect_vertex);

	old_vertex = (vertex_hdl_t)old_info;
	if (old_vertex != GRAPH_VERTEX_NONE)
		(void)graph_vertex_unref(hwgraph, old_vertex);

	return(GRAPH_SUCCESS);
}


/*
** Retrieve the connect point for a vertex.
*/
static vertex_hdl_t
do_hwgraph_connectpt_get(vertex_hdl_t vhdl)
{
	graph_error_t rv;
	arbitrary_info_t info;
	vertex_hdl_t connect;

	if (vhdl == hwgraph_root)
		return(GRAPH_VERTEX_NONE);

	rv = graph_info_get_IDX(hwgraph, vhdl, HWGRAPH_CONNECTPT, &info);
	if (rv != GRAPH_SUCCESS) {
		return(GRAPH_VERTEX_NONE);
	}

	connect = (vertex_hdl_t)info;
	(void)graph_vertex_ref(hwgraph, connect);
	return(connect);
}

/*
** Internal versions of connectpt_set and connectpt_get are above; these are
** the externally-visible versions, which simply handle locking.
*/
graph_error_t
hwgraph_connectpt_set(vertex_hdl_t vhdl, vertex_hdl_t connect_vertex)
{
	graph_error_t rv;
	int s;

	MRLOCK_UPDATE(&hwgraph_connectpt_mrlock);
	rv = do_hwgraph_connectpt_set(vhdl, connect_vertex);
	MRLOCK_DONE_UPDATE(&hwgraph_connectpt_mrlock);

	return(rv);
}

vertex_hdl_t
hwgraph_connectpt_get(vertex_hdl_t vhdl)
{
	vertex_hdl_t connect;
	int s;

	MRLOCK_ACCESS(&hwgraph_connectpt_mrlock);
	connect = do_hwgraph_connectpt_get(vhdl);
	MRLOCK_DONE_ACCESS(&hwgraph_connectpt_mrlock);
	return(connect);
}


/*
** Change the file permissions associated with a hwgfs file.
*/
void
hwgraph_chmod(vertex_hdl_t vhdl, mode_t mode)
{
	hwgfs_chmod(vhdl, mode);
}


/****************************************/
/* General toplogical support services. */
/****************************************/

/*
** Find the canonical name for a given vertex by walking back through
** connectpt's until we hit the hwgraph root vertex (or until we run
** out of buffer space or until something goes wrong).
**
** On success, fills in the caller's buffer with a canonical name,
** and returns GRAPH_SUCCESS.  On failure, return GRAPH_NOT_FOUND.
*/
graph_error_t
hwgraph_vertex_name_get(vertex_hdl_t vhdl, char *buf, uint buflen)
{
	char component[LABEL_LENGTH_MAX];
	vertex_hdl_t connect, target;
	graph_edge_place_t place;
	graph_error_t rv;
	int component_length;
	char *locbuf, *bufptr;
	int s;

	if (buflen < 1)
		return(GRAPH_BAD_PARAM);

	buflen = min(MAXDEVNAME, buflen);
	locbuf = kern_malloc(buflen);
	bufptr = locbuf+buflen;		/* start at end of buffer */

	/* Insert string terminator */
	bufptr--;
	*bufptr = '\0';

	/*
	 * Set up a loop-invarient: At the start of loop, we have one
	 * extra reference to vhdl.
	 */
	(void)graph_vertex_ref(hwgraph, vhdl);

	MRLOCK_ACCESS(&hwgraph_connectpt_mrlock);
	for(;;) {
		connect = do_hwgraph_connectpt_get(vhdl);
		if (connect == GRAPH_VERTEX_NONE) {
			/*
			 * Upward traversal is complete.  If there's still room
			 * in the buffer, tack a "/hw" onto the beginning.
			 */
			component_length = strlen(HWGRAPH_EDGELBL_HW);
			if (bufptr-locbuf > component_length+1) {
				bufptr -= component_length;
				strncpy(bufptr, HWGRAPH_EDGELBL_HW, component_length);
				bufptr--;
				*bufptr = '/';
				/* Success! */
			} else
				bufptr = NULL;

			break;
		}

		/*
		 * If we encounter a vertex that is its own parent, stop.
		 */
		if (connect == vhdl) {
			(void)graph_vertex_unref(hwgraph, vhdl);
			bufptr = NULL;
			break;
		}

		/*
		 * Iterate through outgoing edges 'till we find a
		 * name that points to vhdl.
		 */
		place = GRAPH_EDGE_PLACE_NONE;
		while ((rv=graph_edge_get_next(hwgraph, connect, component,
				&target, &place)) == GRAPH_SUCCESS) {
			if (rv == GRAPH_SUCCESS)
				(void)graph_vertex_unref(hwgraph, target);
			if (target == vhdl)
				break; /* stop looking through edges */
		}

		if (rv != GRAPH_SUCCESS) {
			/*
			 * Something's strange.  vhdl claimed connect
			 * to be its connectpt, but it wasn't really.
			 * A driver may have screwed up.  We'll stop
			 * here and return an error.
			 */
			(void)graph_vertex_unref(hwgraph, connect);
			bufptr = NULL;
			break;
		}

		/*
		 * If this component name fits into our buffer, tack it
		 * on to the beginning of the path along with a
		 * preceding '/'.
		 */
		component_length = strlen(component);
		if (bufptr-locbuf >= component_length+1) {
			bufptr -= component_length;
			strncpy(bufptr, component, component_length);
			bufptr--;
			*bufptr = '/';
		} else {
			(void)graph_vertex_unref(hwgraph, connect);
			bufptr = NULL;
			break;
		}

		(void)graph_vertex_unref(hwgraph, vhdl);
		vhdl = connect;
	}
	MRLOCK_DONE_ACCESS(&hwgraph_connectpt_mrlock);

	(void)graph_vertex_unref(hwgraph, vhdl);

	if (bufptr == NULL) {
		kern_free(locbuf);
		return(GRAPH_NOT_FOUND);
	}

	strcpy(buf, bufptr);
	kern_free(locbuf);
	return(GRAPH_SUCCESS);
}

#define DEVNAME_UNKNOWN "UnknownDevice"

/*
** vertex_to_name converts a vertex into a canonical name by walking
** back through connect points until we hit the hwgraph root (or until
** we run out of buffer space).
**
** Usually returns a pointer to the original buffer, filled in as
** appropriate.  If the buffer is too small to hold the entire name,
** or if anything goes wrong while determining the name, vertex_to_name
** returns "UnknownDevice".
*/
char *
vertex_to_name(vertex_hdl_t vhdl, char *buf, uint buflen)
{
	if (hwgraph_vertex_name_get(vhdl, buf, buflen) == GRAPH_SUCCESS)
		return(buf);
	else
		return(DEVNAME_UNKNOWN);
}

/*
** dev_to_name converts a dev_t into a canonical name.  If the dev_t
** represents a vertex in the hardware graph, it is converted in the
** normal way for vertices.  If the dev_t is an old dev_t (one which
** does not represent a hwgraph vertex), we synthesize a name based
** on major/minor number.
**
** Usually returns a pointer to the original buffer, filled in as
** appropriate.  If the buffer is too small to hold the entire name,
** or if anything goes wrong while determining the name, dev_to_name
** returns "UnknownDevice".
*/
char *
dev_to_name(dev_t dev, char *buf, uint buflen)
{
	if (dev_is_vertex(dev))
		return(vertex_to_name(dev_to_vhdl(dev), buf, buflen));

#define SYNTH_NAME "/hw/major/%d/minor/%d"
#define SYNTH_SIZE strlen(SYNTH_NAME)+2*8
	/* If the buffer is really miniature, give up. */
	if (buflen < SYNTH_SIZE)
		return(DEVNAME_UNKNOWN);
	sprintf(buf, SYNTH_NAME, emajor(dev), eminor(dev));
	return(buf);
}

/*
** Return the "master" for a given vertex.  A master vertex is a
** controller or adapter or other piece of hardware that the given
** vertex passes through on the way to the rest of the system.
*/
vertex_hdl_t
device_master_get(vertex_hdl_t vhdl)
{
	graph_error_t rc;
	vertex_hdl_t master;

	rc = hwgraph_edge_get(vhdl, EDGE_LBL_MASTER, &master);
	if (rc == GRAPH_SUCCESS)
		return(master);
	else
		return(GRAPH_VERTEX_NONE);
}

/*
** Set the master for a given vertex.
** Returns 0 on success, non-0 indicates failure
*/
int
device_master_set(vertex_hdl_t vhdl, vertex_hdl_t master)
{
	graph_error_t rc;

	rc = hwgraph_edge_add(vhdl, master, EDGE_LBL_MASTER);
	return(rc != GRAPH_SUCCESS);
}


/*
** Return the compact node id of the node that ultimately "owns" the specified
** vertex.  In order to do this, we walk back through masters and connect points
** until we reach a vertex that represents a node.
*/
cnodeid_t
master_node_get(vertex_hdl_t vhdl)
{
	cnodeid_t cnodeid;
	vertex_hdl_t master;

	for (;;) {
		cnodeid = nodevertex_to_cnodeid(vhdl);
		if (cnodeid != CNODEID_NONE)
			return(cnodeid);

		master = device_master_get(vhdl);

		/* Check for exceptional cases */
		if (master == vhdl) {
			/* Since we got a reference to the "master" thru
			 * device_master_get() we should decrement
			 * its reference count by 1
			 */
			hwgraph_vertex_unref(master);
			return(CNODEID_NONE);
		}

		if (master == GRAPH_VERTEX_NONE) {
			master = hwgraph_connectpt_get(vhdl);
			if ((master == GRAPH_VERTEX_NONE) ||
			    (master == vhdl)) {
				if (master == vhdl)
					/* Since we got a reference to the
					 * "master" thru
					 * hwgraph_connectpt_get() we should
					 * decrement its reference count by 1
					 */
					hwgraph_vertex_unref(master);
				return(CNODEID_NONE);
			}
		}
		
		vhdl = master;
		/* Decrement the reference to "master" which was got
		 * either thru device_master_get() or hwgraph_connectpt_get()
		 * above.
		 */
		hwgraph_vertex_unref(master);
	}
}


static vertex_hdl_t hwgraph_all_cpuids = GRAPH_VERTEX_NONE;
/*
** If the specified device represents a CPU, return its cpuid;
** otherwise, return CPU_NONE.
*/
cpuid_t
cpuvertex_to_cpuid(vertex_hdl_t vhdl)
{
	arbitrary_info_t cpuid = CPU_NONE;

	(void)graph_info_get_LBL(hwgraph, vhdl, INFO_LBL_CPUID, NULL, &cpuid);

	return((cpuid_t)cpuid);
}

void
mark_cpuvertex_as_cpu(vertex_hdl_t vhdl, cpuid_t cpuid)
{
	if (cpuid == CPU_NONE)
		return;

	ASSERT(cpuid >= 0);
	ASSERT(cpuid < maxcpus);

	if(cpu_enabled(cpuid))
	  pdaindr[cpuid].pda->p_vertex = vhdl;

	(void)graph_info_add_LBL(hwgraph, vhdl, INFO_LBL_CPUID, INFO_DESC_EXPORT,
			(arbitrary_info_t)cpuid);
	{
		char cpuid_buffer[10];

		if (hwgraph_all_cpuids == GRAPH_VERTEX_NONE) {
			(void)hwgraph_path_add(	hwgraph_root,
						EDGE_LBL_CPUNUM,
						&hwgraph_all_cpuids);
		}

		sprintf(cpuid_buffer, "%d", cpuid);
		(void)hwgraph_edge_add(	hwgraph_all_cpuids,
					vhdl,
					cpuid_buffer);
	}

	if(cpu_enabled(cpuid))
	  if (device_admin_info_get(vhdl, "NOINTR")) {
	    int s;
	    pda_t *npda;

	    npda = pdaindr[cpuid].pda;

	    s = mutex_spinlock_spl(&npda->p_special, spl7);
	    npda->p_flags |= PDAF_NOINTR;
	    mutex_spinunlock(&npda->p_special, s);
	  }
}


static vertex_hdl_t hwgraph_all_cnodes = GRAPH_VERTEX_NONE;

/*
** If the specified device represents a node, return its
** compact node ID; otherwise, return CNODEID_NONE.
*/
cnodeid_t
nodevertex_to_cnodeid(vertex_hdl_t vhdl)
{
	arbitrary_info_t cnodeid = CNODEID_NONE;

	(void)graph_info_get_LBL(hwgraph, vhdl, INFO_LBL_CNODEID, NULL, &cnodeid);

	return((cnodeid_t)cnodeid);
}

void
mark_nodevertex_as_node(vertex_hdl_t vhdl, cnodeid_t cnodeid)
{
	if (cnodeid == CNODEID_NONE)
		return;

	cnodeid_to_vertex(cnodeid) = vhdl;
	(void)graph_info_add_LBL(hwgraph, vhdl, INFO_LBL_CNODEID,
		INFO_DESC_EXPORT, (arbitrary_info_t)cnodeid);

	{
		char cnodeid_buffer[10];

		if (hwgraph_all_cnodes == GRAPH_VERTEX_NONE) {
			(void)hwgraph_path_add( hwgraph_root,
						EDGE_LBL_NODENUM,
						&hwgraph_all_cnodes);
		}

		sprintf(cnodeid_buffer, "%d", cnodeid);
		(void)hwgraph_edge_add(	hwgraph_all_cnodes,
					vhdl,
					cnodeid_buffer);
	}
}


/*
 * Code for HWGRAPH vpath support
 */


#if DEBUG && HWG_DEBUG

void
hwgraph_vpath_display(hwgraph_vpath_t vpath)
{
	char		name[MAXDEVNAME];
	vertex_hdl_t	vhdl;
	int		i;

	dprintf("hwgraph_vpath at 0x%X: %d verticies\n",
		vpath, vpath->pathlen);
	for (i = 0; i < vpath->pathlen; i++) {
		vhdl = vpath->vhdl_array[i];
		vertex_to_name(vhdl, name, MAXDEVNAME);
		dprintf("\t%3d: %5u (%s)\n",
			i, vhdl, name);
	}
	dprintf("\n");
}

#endif /* DEBUG */


/*
 * Create a new vpath that holds vertices from src to dst.
 */
hwgraph_vpath_t
hwgraph_vpath_create(vertex_hdl_t src, vertex_hdl_t dst)
{
	hwgraph_vpath_t	vpath;
	vertex_hdl_t	dsta[HWGRAPH_VPATH_LEN_MAX];
	int		srcc;
	int		dstc;
	vertex_hdl_t	cmn;

	/* generate the vpath from the source to the
	 * root of the hardware graph along the connection
	 * points of the verticies. Stop short of adding the
	 * root of the graph to the vpath.
	 *
	 * hwgraph_connectpt_get increments the reference
	 * count on the vertex it returns; we need to hit
	 * the src vertex explicitly -- thus the slightly
	 * odd loop structure.
	 */
	NEW(vpath);
	srcc = 0;
	if ((src != hwgraph_root) &&
	    (src != GRAPH_VERTEX_NONE)) {
		hwgraph_vertex_ref(src);
		do {
#if DEBUG && HWG_DEBUG
cmn_err(CE_CONT, "%v src ...\n", src);
#endif
			ASSERT(srcc < HWGRAPH_VPATH_LEN_MAX);
			vpath->vhdl_array[srcc++] = src;
			src = hwgraph_connectpt_get(src);
		} while ((src != hwgraph_root) &&
			 (src != GRAPH_VERTEX_NONE));
	}

	/* generate the vpath from the destination to the
	 * root of the hardware graph along the connection
	 * points of the verticies. Stop short of adding the
	 * root of the graph to the path.
	 *
	 * hwgraph_connectpt_get increments the reference
	 * count on the vertex it returns; we need to hit
	 * the dst vertex explicitly -- thus the slightly
	 * odd loop structure.
	 */
	dstc = 0;
	if ((dst != hwgraph_root) &&
	    (dst != GRAPH_VERTEX_NONE)) {
		hwgraph_vertex_ref(dst);
		do {
#if DEBUG && HWG_DEBUG
cmn_err(CE_CONT, "%v dst ...\n", dst);
#endif
			ASSERT(dstc < HWGRAPH_VPATH_LEN_MAX);
			dsta[dstc++] = dst;
			dst = hwgraph_connectpt_get(dst);
		} while ((dst != hwgraph_root) &&
			 (dst != GRAPH_VERTEX_NONE));
	}

	/* Trim off the common tail of the vpaths, so they
	 * end at verticies whose connection points are the
	 * "cmn" vertex. Note that if either vertex is along
	 * the connecton path of the other, its list will
	 * become empty and cmn will be the "elder" vertex;
	 * if src and dst are initially the same vertex,
	 * both lists will be empty and cmn will be the
	 * vertex of interest.
	 *
	 * We incremented the reference counts for all the
	 * verticies on both paths by one above; so, as we
	 * discard references, decrement the reference
	 * counts. We want our net effect to be to increment
	 * the reference counts for all the verticies on the
	 * final list by one (including the source and the
	 * destination). This will be reversed when we
	 * destroy the path later.
	 */
	cmn = hwgraph_root;
	while (1) {
		if (srcc < 1) break;
		if (dstc < 1) break;
		src = vpath->vhdl_array[srcc-1];
		dst = dsta[dstc-1];
		if (src != dst) break;
		srcc --;
		dstc --;
		hwgraph_vertex_unref(cmn);
		hwgraph_vertex_unref(dst);
		cmn = src;
#if DEBUG && HWG_DEBUG
cmn_err(CE_CONT, "cmn ...\n",cmn);
#endif
	}

	/* Now add the common vertex, and the reverse of the
	 * destination vpath, to the source vpath -- giving us
	 * the canonical path from the source to the
	 * destination.
	 */
	ASSERT(srcc < HWGRAPH_VPATH_LEN_MAX);
	vpath->vhdl_array[srcc++] = cmn;
	while (dstc-- > 0) {
		ASSERT(srcc < HWGRAPH_VPATH_LEN_MAX);
		vpath->vhdl_array[srcc++] = dsta[dstc];
	}

	vpath->pathlen = srcc;

#if DEBUG && HWG_DEBUG
	hwgraph_vpath_display(vpath);
#endif /* DEBUG */

	return (vpath);
}


/*
 * Destroy a hardware graph vpath.
 */
void
hwgraph_vpath_destroy(hwgraph_vpath_t vpath)
{
	int i;

	/* decrement the reference counts for the verticies
	 * in the vpath.
	 */
	for (i=0; i<vpath->pathlen; i++)
		hwgraph_vertex_unref(vpath->vhdl_array[i]);

	kmem_free(vpath, sizeof(struct hwgraph_vpath_s));
}


/*
 * Given a cursor into a hardware graph vpath, return the selected vertex.
 * May return GRAPH_VERTEX_NONE if cursor points beyond either end of the
 * path.
 */
vertex_hdl_t
hwgraph_vpath_vertex_get(hwgraph_vpath_cursor_t cursor)
{
	hwgraph_vpath_t vpath;

	if ((NULL == cursor) ||
	    (NULL == (vpath = cursor->path)) ||
	    (cursor->cursor < 0) ||
	    (cursor->cursor >= vpath->pathlen))
		return GRAPH_VERTEX_NONE;

	return vpath->vhdl_array[cursor->cursor];
}


/*
 * Given a cursor, move forward to the next element in the hardware graph vpath.
 */
void
hwgraph_vpath_cursor_next(hwgraph_vpath_cursor_t cursor)
{
	if (cursor->cursor < cursor->path->pathlen)
		cursor->cursor ++;
}


/*
 * Given a cursor, move backwards to the previous element in the hardware graph vpath.
 */
void
hwgraph_vpath_cursor_prev(hwgraph_vpath_cursor_t cursor)
{
	if (cursor->cursor > 0)
		cursor->cursor --;
}


/*
 * Initialize a cursor to point to the start of the indicated hardware graph vpath.
 */
void
hwgraph_vpath_cursor_init(hwgraph_vpath_cursor_t cursor, hwgraph_vpath_t vpath)
{
	cursor->path = vpath;
	cursor->cursor = 0;
}


/*
 * Clone a hardware graph vpath cursor.
 */
void
hwgraph_vpath_cursor_clone(hwgraph_vpath_cursor_t source_cursor,
                          hwgraph_vpath_cursor_t dest_cursor)
{
	dest_cursor->path = source_cursor->path;
	dest_cursor->cursor = source_cursor->cursor;
}


/*
 * Create a hardware graph path cursor
 */
hwgraph_vpath_cursor_t
hwgraph_vpath_cursor_create(void)
{
	hwgraph_vpath_cursor_t cursor;

	NEW(cursor);
	return(cursor);
}

/*
 * Destroy a hardware graph vpath cursor.
 */
void
hwgraph_vpath_cursor_destroy(hwgraph_vpath_cursor_t cursor)
{
	DEL(cursor);
}





#if HWG_PRINT
/*ARGSUSED1*/
static int
hwgraph_print_leaf (void *arg, vertex_hdl_t vtx)
{
	/* REFERENCED */
	graph_error_t		rc;
	char			vnbuf[1024];
	char			enbuf[1024];
	vertex_hdl_t		dst;
	vertex_hdl_t		back;
	graph_edge_place_t	place;
	int			kids = 0;

	/*
	 * ignore vertexes without proper canonical
	 * names, and verticies whose names do not
	 * land at a vertex, and verticies whose names
	 * land you on the wrong vertex.
	 */

	rc = hwgraph_vertex_name_get(vtx, vnbuf, sizeof vnbuf);
	if (rc != GRAPH_SUCCESS)
		return 0;

	if (strlen(vnbuf) < 5)
		return 0;

	rc = hwgraph_traverse(hwgraph_root, vnbuf+4, &dst);
	if (rc != GRAPH_SUCCESS)
		return 0;

	if (dst != vtx)
		return 0;

	place = GRAPH_EDGE_PLACE_NONE;
	while (hwgraph_edge_get_next(vtx, enbuf, &dst, &place) == GRAPH_SUCCESS) {
		graph_vertex_unref(hwgraph, dst);
		if (strcmp(enbuf, ".") &&
		    strcmp(enbuf, "..")) {
			rc = hwgraph_traverse(dst, "..", &back);
			if ((rc == GRAPH_SUCCESS) && (back == vtx))
				kids ++;
		}
	}

	if (!kids)
		printf("\t%u\t%s\n", vtx, vnbuf);

	return 0;
}

/*ARGSUSED1*/
static int
hwgraph_print_edge (void *arg, vertex_hdl_t vtx)
{
	/* REFERENCED */
	graph_error_t		rc;
	char			vnbuf[1024];
	char			enbuf[1024];
	char			tnbuf[1024];
	vertex_hdl_t		dst;
	vertex_hdl_t		back;
	graph_edge_place_t	place;

	/*
	 * ignore vertexes without proper canonical
	 * names, and verticies whose names do not
	 * land at a vertex, and verticies whose names
	 * land you on the wrong vertex.
	 */

	rc = hwgraph_vertex_name_get(vtx, vnbuf, sizeof vnbuf);
	if (rc != GRAPH_SUCCESS)
		return 0;

	if (strlen(vnbuf) < 5)
		return 0;

	rc = hwgraph_traverse(hwgraph_root, vnbuf+4, &dst);
	if (rc != GRAPH_SUCCESS)
		return 0;

	if (dst != vtx)
		return 0;

	place = GRAPH_EDGE_PLACE_NONE;
	while (hwgraph_edge_get_next(vtx, enbuf, &dst, &place) == GRAPH_SUCCESS) {
		graph_vertex_unref(hwgraph, dst);
		if (strcmp(enbuf, ".") &&
		    strcmp(enbuf, "..")) {
			rc = hwgraph_traverse(dst, "..", &back);
			if ((rc != GRAPH_SUCCESS) || (back != vtx)) {
				rc = hwgraph_vertex_name_get(dst, tnbuf, sizeof tnbuf);
				ASSERT(rc == GRAPH_SUCCESS);
				printf("\t%s/%s => (%u)%s\n", vnbuf, enbuf, dst, tnbuf);
			}
		}
	}

	return 0;
}

#if DEBUG && HWG_DEBUG
/*ARGSUSED1*/
static int
hwgraph_print_ugly (void *arg, vertex_hdl_t vtx)
{
	/* REFERENCED */
	graph_error_t		rc;
	char			vnbuf[1024];
	vertex_hdl_t		dst;

	/*
	 * print vertexes without proper canonical
	 * names, and verticies whose names do not
	 * land at a vertex, and verticies whose names
	 * land you on the wrong vertex.
	 */

	if (vtx == hwgraph_root)
		return 0;

	rc = hwgraph_vertex_name_get(vtx, vnbuf, sizeof vnbuf);
	if (rc != GRAPH_SUCCESS) {
		printf("\tvertex %u has no name\n", vtx);
		return 0;
	}

	if (strlen(vnbuf) < 5) {
		printf("\tvertex %u's name '%s' is impossibly short\n", vtx, vnbuf);
		return 0;
	}

	rc = hwgraph_traverse(hwgraph_root, vnbuf+4, &dst);
	if (rc != GRAPH_SUCCESS) {
		printf("\tvertex %u's name '%s' does not name a vertex\n", vtx, vnbuf);
		return 0;
	}

	if (dst != vtx) {
		printf("\tvertex %u's name '%s' lands at vertex %u\n", vtx, vnbuf, dst);
		return 0;
	}

	return 0;
}
#endif

void
hwgraph_print(void)
{
	printf("HWGRAPH leaf verticies:\n");
	hwgraph_vertex_visit(hwgraph_print_leaf, 0, 0, 0);
	printf("HWGRAPH non-canonical edges:\n");
	hwgraph_vertex_visit(hwgraph_print_edge, 0, 0, 0);
#if DEBUG && HWG_DEBUG
	printf("HWGRAPH orphan verticies:\n");
	hwgraph_vertex_visit(hwgraph_print_ugly, 0, 0, 0);
#endif
	printf("\n");
}
#endif /* HWG_PRINT */

/*
 * mem_vhdl_get returns the vertex handle for the xbow widget corresponding to
 * the heart/hub which controls the memory associated with the given driver.
 * Given the vertex handle for the device, the above described vertex handle
 * is returned. This is used by drivers which support priority IO.
 */

/*
 * Using the canonical path name to get hold of the desired vertex handle will
 * not work on multi-hub sn0 nodes. Hence, we use the following (slightly
 * convoluted) algorithm.
 *
 * - Start at the vertex corresponding to the driver (provided as input parameter)
 * - Loop till you reach a vertex which has EDGE_LBL_MEMORY
 *    - If EDGE_LBL_CONN exists, follow that up.
 *      else if EDGE_LBL_MASTER exists, follow that up.
 *      else follow EDGE_LBL_DOTDOT up.
 *
 * * We should be at desired hub/heart vertex now *
 * - Follow EDGE_LBL_CONN to the widget vertex.
 *
 * - return vertex handle of this widget.
 */
vertex_hdl_t
mem_vhdl_get(vertex_hdl_t drv_vhdl)
{
vertex_hdl_t cur_vhdl, cur_upper_vhdl;
vertex_hdl_t tmp_mem_vhdl, mem_vhdl;
graph_error_t loop_rv;

  /* Initializations */
  cur_vhdl = drv_vhdl;
  loop_rv = ~GRAPH_SUCCESS;

  /* Loop till current vertex has EDGE_LBL_MEMORY */
  while (loop_rv != GRAPH_SUCCESS) {

    if ((hwgraph_edge_get(cur_vhdl, EDGE_LBL_CONN, &cur_upper_vhdl)) == GRAPH_SUCCESS) {

    } else if ((hwgraph_edge_get(cur_vhdl, EDGE_LBL_MASTER, &cur_upper_vhdl)) == GRAPH_SUCCESS) {
      } else { /* Follow HWGRAPH_EDGELBL_DOTDOT up */
           (void) hwgraph_edge_get(cur_vhdl, HWGRAPH_EDGELBL_DOTDOT, &cur_upper_vhdl);
        }

    cur_vhdl = cur_upper_vhdl;

#if DEBUG && HWG_DEBUG
    printf("Current vhdl %d \n", cur_vhdl);
#endif /* DEBUG */

    loop_rv = hwgraph_edge_get(cur_vhdl, EDGE_LBL_MEMORY, &tmp_mem_vhdl);
  }

  /* We should be at desired hub/heart vertex now */
  if ((hwgraph_edge_get(cur_vhdl, EDGE_LBL_CONN, &mem_vhdl)) != GRAPH_SUCCESS)
    return (GRAPH_VERTEX_NONE);

  return (mem_vhdl);
}
