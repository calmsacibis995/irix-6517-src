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

#ident	"$Revision: 1.5 $"

#include <sys/types.h>
#include <sys/debug.h>
#include <sys/idbg.h>
#include <sys/idbgentry.h>
#include <sys/sema.h>
#include <sys/graph.h>
#include <sys/kmem.h>
#include <sys/systm.h>
#include "strtbl.h"
#include "graph_private.h"

extern atomicAddUlong(unsigned long *, unsigned long) ;
extern atomicAddInt(int *, int64_t) ;

/*
** Generic graph library.
** 
** Supports directed graphs consisting of vertices and named edges.  At any
** vertex, the caller can store two kinds of information: "labelled" and
** "indexed".  In both cases, information is a single long (which may hold a
** pointer to an arbitrary structure).  Labelled information provides arbitrary
** flexibility to the caller; but to retrieve labelled information the graph
** module must do label comparisons.  Indexed information can be used in cases
** where retrieval time is important; but indexed information preallocates a 
** small amount of space for every vertex.  The current implementation of 
** indexed information is fast, non-blocking, and lock-free.
**
** Routines for accessing the graph are grouped into the following categories:
**	operations on GRAPHS
**	operations on VERTICES
**	operations on EDGES
**	operations on LABELLED INFORMATION
**	operations on INDEXED INFORMATION
**	operations on strings
**
** The graph module deals with strings that look like pathnames, but each
** component in the string represents an edge name.  Given a starting vertex
** and a string, one can follow edges to a target vertex.
**
** One important goal of the implementation is to conserve space -- especially
** space that scales with the size of the graph (number of vertices).
**
** All structures are expected to be vastly read-only, and all accesses are
** brief, so locking is handled with multi-reader spinlocks.  If locking is 
** modified, it should be kept in mind that we must be able to *retrieve* 
** information at interrupt level.
*/



		/*****************************/
		/* INTERNAL support routines */
		/*****************************/

/********************************************************************/
/* Internal routines for dealing with vertices and vertex handles:  */
/* vhandle_get_vertex, vertex_allocate, vertex_free                 */
/********************************************************************/

/*
** graph_vhandle_get_vertex perform basic validity checks on a vertex handle.  
** If it's valid, return a pointer to the vertex, a pointer to the vertex group 
** that it's in, and the groupid and grpidx.
**
** Locking is handled by caller.
*/
static graph_error_t
graph_vhandle_get_vertex(graph_t *graph,
			vertex_hdl_t vertex_handle,
			graph_vertex_t **vertex,
			graph_vertex_group_t **vertex_group,
			int *groupid,
			int *grpidx)
{
	graph_vertex_t *loc_vertex;
	int loc_groupid, loc_grpidx;
	graph_vertex_group_t *loc_vertex_group;

	if (graph == NULL)
		return(GRAPH_BAD_PARAM);

	if (vertex_handle == GRAPH_VERTEX_NONE)
		return(GRAPH_BAD_PARAM);

	if (vertex_handle > graph->graph_vertex_handle_limit)
		return(GRAPH_HIT_LIMIT);

	loc_groupid = HANDLE_TO_GROUPID(graph, vertex_handle);
	loc_grpidx = HANDLE_TO_GRPIDX(graph, vertex_handle);
	loc_vertex_group = &graph->graph_vertex_group[loc_groupid];
	loc_vertex = GRPIDX_TO_VERTEX(graph, loc_groupid, loc_grpidx);

	if (VERTEX_ISFREE(loc_vertex) || VERTEX_ISNEW(loc_vertex))
		return(GRAPH_NOT_FOUND);

	*vertex = loc_vertex;
	*vertex_group = loc_vertex_group;
	*groupid = loc_groupid;
	*grpidx = loc_grpidx;

	return(GRAPH_SUCCESS);
}


#define NOT_REACHED 0

/*
** graph_vertex_allocate finds an unused vertex handle to represent the specified 
** vertex.  If we're out of handle slots (or close to it), increase the size of the 
** graph.
*/
static graph_error_t
graph_vertex_allocate(	graph_t *graph,
			vertex_hdl_t *new_vertex_handle,
			graph_vertex_t **new_vertex)
{
	int grpidx, groupid;
	int i, num_idx;
	int group_rotor, group_limit;
	graph_vertex_group_t *vertex_group;
	graph_vertex_t *new_vertex_array, *vertex;
	int old_vertex_limit, new_vertex_limit;
	arbitrary_info_t *info_IDX_ptr;

	if (graph == NULL)
		return(GRAPH_BAD_PARAM);

	if ((new_vertex_handle == NULL) && (new_vertex == NULL))
		return(GRAPH_BAD_PARAM);

startover:
	old_vertex_limit = graph->graph_vertex_handle_limit;

	/*
	 * Search for unused vertex_handle.
	 */
	group_rotor = graph->graph_group_rotor;
	group_limit = HANDLE_TO_GROUPID(graph, old_vertex_limit-1);

	for (groupid=group_rotor; groupid<group_limit; groupid++) {

		vertex_group = &graph->graph_vertex_group[groupid];

		/* Peek to see if this group looks full */
		if (vertex_group->group_count == GRAPH_VERTEX_PER_GROUP)
			continue;

		mrlock(&vertex_group->group_mrlock, MR_UPDATE, PZERO);
		if (vertex_group->group_count == GRAPH_VERTEX_PER_GROUP) {
			mrunlock(&vertex_group->group_mrlock);
			continue;
		}

		/* We're committed to allocating a vertex from this group. */
		for (grpidx=0; grpidx<GRAPH_VERTEX_PER_GROUP; grpidx++) {
			vertex = GRPIDX_TO_VERTEX(graph, groupid, grpidx);
			if (VERTEX_ISFREE(vertex))
				goto found;
		}

		ASSERT(NOT_REACHED);
	}

	for (groupid=0; groupid<group_rotor; groupid++) {

		vertex_group = &graph->graph_vertex_group[groupid];

		/* Peek to see if this group looks full */
		if (vertex_group->group_count == GRAPH_VERTEX_PER_GROUP)
			continue;

		mrlock(&vertex_group->group_mrlock, MR_UPDATE, PZERO);
		if (vertex_group->group_count == GRAPH_VERTEX_PER_GROUP) {
			mrunlock(&vertex_group->group_mrlock);
			continue;
		}

		/* We're commiteed to allocating a vertex from this group. */
		for (grpidx=0; grpidx<GRAPH_VERTEX_PER_GROUP; grpidx++) {
			vertex = GRPIDX_TO_VERTEX(graph, groupid, grpidx);
			if (VERTEX_ISFREE(vertex))
				goto found;
		}

		ASSERT(NOT_REACHED);
	}

	/*
	 * If we're trying to add a new vertex and we've already reached the
	 * current limit (or we're at least close to the limit), we need to 
	 * expand the graph by adding a new group of vertices.  The graph 
	 * might not actually be full, since we cheated and peeked at some
	 * fields without holding a lock.  Close enough, though -- in the
	 * worst case we'll allocate a new group when we didn't really need 
	 * to.
	 */
	new_vertex_limit = old_vertex_limit + GRAPH_VERTEX_PER_GROUP;

	/*
	 * Guard against unbounded growth of the graph.  Something's wrong.
	 */
	if (new_vertex_limit > graph->graph_num_group*GRAPH_VERTEX_PER_GROUP)
		return(GRAPH_HIT_LIMIT);

	new_vertex_array = graph_allocate(SIZE_GROUP_VERTEX_ARRAY(graph));
	if (new_vertex_array == NULL)
		return(GRAPH_CANNOT_ALLOC);

	mrlock(&graph->graph_mrlock, MR_UPDATE, PZERO);

	if (graph->graph_vertex_handle_limit != old_vertex_limit) {
		/* 
		 * Things changed pretty drastically while we worked.
		 * We might not need a new group after all, so free up
		 * the space we just allocated and start over.
		 */
		mrunlock(&graph->graph_mrlock);
		graph_free(new_vertex_array);
		goto startover;
	}

	/*
	 * At this point: 
	 * 	we have the graph mrlock 
	 *	we know that we had some trouble allocating a handle, 
	 *	we know that the graph hasn't grown at all since we had that trouble, 
	 *	and we've already allocated space for a new group of vertices
	 *
	 * Go ahead and add the new group!
	 */

	groupid = HANDLE_TO_GROUPID(graph, old_vertex_limit);
	vertex_group = &(graph->graph_vertex_group[groupid]);
	vertex_group->group_vertex_array = new_vertex_array;

	graph->graph_vertex_handle_limit = new_vertex_limit;
	graph->graph_group_rotor = groupid;
	atomicAddUlong(&graph->graph_generation, 1);

	mrunlock(&graph->graph_mrlock);
	/* 
	 * Now that we've increased the number of vertices that can be accomodated,
	 * start from the beginning and see if we can find a free one.
	 */
	goto startover;


found:
	/* 
	 * We found a free vertex: It's the "grpidx" vertex in group "groupid".
	 * Initialize it and return its handle.
	 */
	ASSERT(&graph->graph_vertex_group[groupid] == vertex_group);

	graph->graph_group_rotor = groupid;

	vertex->v_num_edge = 0;
	vertex->v_num_info_LBL = 0;
	vertex->v_refcnt = 1;

	num_idx = graph->graph_num_index;
	info_IDX_ptr = VERTEX_INFO_IDX_PTR(vertex);
	for (i=0; i<num_idx; i++, info_IDX_ptr++)
		*info_IDX_ptr = 0;

	vertex->v_info_list = GRAPH_VERTEX_NEW;

	vertex_group->group_count++;
	atomicAddInt(&graph->graph_num_vertex, 1);
	atomicAddUlong(&graph->graph_generation, 1);

	mrunlock(&vertex_group->group_mrlock);

	if (new_vertex_handle != NULL)
		*new_vertex_handle = GRPIDX_TO_HANDLE(groupid, grpidx);

	if (new_vertex != NULL)
		*new_vertex = vertex;

	return(GRAPH_SUCCESS);
}




/*
** graph_vertex_free frees a previously-used vertex.  The vertex can no longer
** be referenced.  The graph module may reallocate the associated handle for 
** new vertices.  This routine is called only when the vertex' reference count
** drops to 0.
**
** Locking is handled by caller.
*/
static void
graph_vertex_free(	graph_t *graph,
			vertex_hdl_t vhdl)
{
	graph_vertex_t *vertex;
	graph_vertex_group_t *vertex_group;
	graph_info_t *info_to_free;
	graph_error_t status;
	graph_info_t *edge_list;
	int i, num_edge;
	int junk_groupid;
	int junk_grpidx;

	ASSERT(graph != NULL);

	status = graph_vhandle_get_vertex(graph, vhdl, &vertex,
		&vertex_group, &junk_groupid, &junk_grpidx);

	if (status != GRAPH_SUCCESS)
		return;

	ASSERT(vertex != NULL);
	ASSERT(vertex->v_refcnt == 0);

	/*
	** Free any lingering outgoing edges simply by handling reference counts.
	** Memory consumed by all edges (and LBL info) is released en masse, below.
	*/
	edge_list = VERTEX_EDGE_PTR(vertex);
	num_edge = vertex->v_num_edge;
	for (i=0; i<num_edge; i++) {
		vertex_hdl_t target_vertex = (vertex_hdl_t)edge_list[i].i_info;
		graph_vertex_unref(graph, target_vertex);
	}
	vertex->v_num_edge = 0; /* sanity */

	if (VERTEX_HASINFO(vertex))
		graph_free(vertex->v_info_list);

	vertex->v_info_list = GRAPH_VERTEX_FREE;

	ASSERT(vertex_group->group_count > 0);
	vertex_group->group_count--;
	atomicAddInt(&graph->graph_num_vertex, -1);
	atomicAddUlong(&graph->graph_generation, 1);
}




		/***********************************/
		/* EXPORTED graph module functions */
		/***********************************/


/*************************************************************/
/* Operations on GRAPHS: create, destroy, summary_get, visit */
/*************************************************************/

/*
** graph_create instantiates a new instance of a graph with the specified attributes.
*/
/* ARGSUSED */
graph_error_t 
graph_create(	graph_attr_t *graph_attributes,
		graph_t **graph,
		int num_vertices)
{
	char *name;
	unsigned int num_idx;
	graph_t *new_graph;
	graph_vertex_group_t *vertex_group;
	graph_vertex_t *new_vertex_array;
	int groupid;
	uint graph_num_group;

	if (graph_attributes == NULL)
		return(GRAPH_BAD_PARAM);

	if (graph == NULL)
		return(GRAPH_BAD_PARAM);

	if (num_vertices <= 0)
		graph_num_group = DEFAULT_GRAPH_NUM_GROUP;
	else
		graph_num_group = (num_vertices+GRAPH_VERTEX_PER_GROUP-1) / GRAPH_VERTEX_PER_GROUP;

	new_graph = graph_allocate(sizeof(graph_t) + (graph_num_group-1)*sizeof(graph_vertex_group_t));
	if (new_graph == NULL)
		return(GRAPH_CANNOT_ALLOC);

	string_table_init(&new_graph->graph_string_table);
	name = string_table_insert(&new_graph->graph_string_table, graph_attributes->ga_name);

	/* Now fill in fields of graph structure */
	new_graph->graph_name = name;
	new_graph->graph_generation = 0;
	new_graph->graph_separator_char = graph_attributes->ga_separator;
	new_graph->graph_num_group = graph_num_group;
	num_idx = new_graph->graph_num_index = graph_attributes->ga_num_index;
	new_graph->graph_reserved_places = graph_attributes->ga_reserved_places;

	mrinit(&new_graph->graph_mrlock, name);

	new_graph->graph_num_vertex = 1; /* burn handle 0 */
	new_graph->graph_vertex_handle_limit = GRAPH_VERTEX_PER_GROUP;

	new_graph->graph_vertex_size = 
		sizeof(graph_vertex_t) + num_idx * sizeof(arbitrary_info_t);

	/* Allocate space for group 0. */
	new_vertex_array = graph_allocate(SIZE_GROUP_VERTEX_ARRAY(new_graph));
	if (new_vertex_array == NULL) {
		graph_free(new_graph);
		return(GRAPH_CANNOT_ALLOC);
	}

	/* 
	 * We count on the zero'ed allocation above to zero the v_info_list
	 * field of each vertex, and we count on such a zero value to
	 * indicate that the vertex is free.
	 */
	ASSERT(GRAPH_VERTEX_FREE == 0L); /* check just to catch dumb mistakes */

	vertex_group = &(new_graph->graph_vertex_group[0]);
	mrinit(&vertex_group->group_mrlock, name);

	/* Prevent use of the vertex with handle 0 (GRAPH_VERTEX_NONE) */
	new_vertex_array[0].v_info_list = GRAPH_VERTEX_NEW;
	vertex_group->group_count = 1;
	vertex_group->group_vertex_array = new_vertex_array; /* Allocated above */


	for (groupid=1; groupid<graph_num_group; groupid++) {
		vertex_group = &(new_graph->graph_vertex_group[groupid]);
		mrinit(&vertex_group->group_mrlock, name);
		vertex_group->group_count = 0;
		vertex_group->group_vertex_array = NULL;
	}

	new_graph->graph_group_rotor = 0;

	*graph = new_graph;

	return(GRAPH_SUCCESS);
}




/*
** graph_destroy frees all memory associated with a graph.  After calling this
** routine, it is illegal to reference this graph.
**
** The caller must insure that there is currently no one referencing
** this graph and that no one will reference it in the future.
*/
graph_error_t 
graph_destroy(graph_t *graph)
{
	graph_vertex_group_t *vertex_group;
	int groupid;
	unsigned int graph_num_group;

	if (graph == NULL)
		return(GRAPH_BAD_PARAM);

	if (graph->graph_num_vertex != 1) /* Allow unused handle 0 */
		return(GRAPH_ILLEGAL_REQUEST);

	graph_num_group = graph->graph_num_group;

	for (groupid=0; groupid<graph_num_group; groupid++) {
		vertex_group = &graph->graph_vertex_group[groupid];
		mrfree(&vertex_group->group_mrlock);
		if (vertex_group->group_vertex_array != NULL)
			graph_free(vertex_group->group_vertex_array);
	}

	mrfree(&graph->graph_mrlock);
	string_table_destroy(&graph->graph_string_table);

	graph_free(graph);

	return(GRAPH_SUCCESS);
}


/*
** graph_vertex_visit executes an arbitrary function on every vertex in the graph
** If the vertex_visit_fn returns non-zero, traversal is halted.  The vertex_visit 
** function takes two arguments.  The first arg is passed through from the call to 
** graph_vertex_visit.  The second arg is a vertex handle (representing the vertex 
** being visited).
** 
** This routine grabs a reference to the vertex before calling the specified function
** and it unreferences the vertex after return from the function.
*/
graph_error_t 
graph_vertex_visit(	graph_t *graph,
			int (*vertex_visit_fn)(void *, vertex_hdl_t),
			void *arg,
			int *retvalp,
			vertex_hdl_t *end_vertex_handle)
{
	vertex_hdl_t next_vertex_handle;
	int rv;
	graph_error_t status;
	graph_vertex_place_t place = GRAPH_VERTEX_PLACE_NONE;

	if (graph == NULL)
		return(GRAPH_BAD_PARAM);

	while ( (status = graph_vertex_get_next(graph, &next_vertex_handle, 
				&place)) == GRAPH_SUCCESS) {

		if (rv = vertex_visit_fn(arg, next_vertex_handle)) {
			if (retvalp != NULL)
				*retvalp = rv;

			if (end_vertex_handle != NULL)
				*end_vertex_handle = next_vertex_handle;

			return(GRAPH_SUCCESS);
		}
	}

	ASSERT(status == GRAPH_HIT_LIMIT);
	return(status);
}



/*********************************************************************************/
/* Operations on VERTICES: create, destroy, combine, clone, get_next, ref, unref */
/*********************************************************************************/


/*
** graph_vertex_create instantiates a vertex in the specified graph.  Initially, 
** the vertex is disconnected from the rest of the graph.
*/
graph_error_t
graph_vertex_create(	graph_t *graph,
			vertex_hdl_t *new_vertex_handle)
{
	graph_error_t status;
	vertex_hdl_t vertex_handle;
	graph_vertex_t *vertex;

	if (graph == NULL)
		return(GRAPH_BAD_PARAM);

	if (new_vertex_handle == NULL)
		return(GRAPH_BAD_PARAM);

	/*
	 * Find a handle for a new vertex.
	 */
	status = graph_vertex_allocate(graph, &vertex_handle, &vertex);
	if (status != GRAPH_SUCCESS)
		return(status);

	vertex->v_info_list = GRAPH_VERTEX_NOINFO;

	*new_vertex_handle = vertex_handle;

	return(GRAPH_SUCCESS);
}


/*
** graph_vertex_destroy is used in conjunction with graph_vertex_create. It
** indicates that the caller no longer has any use for this vertex and would
** like to destroy it.  If there are outstanding references to this vertex
** other than the original "creation reference", the destroy request is rejeted.
**
** Returns GRAPH_SUCCESS if the vertex was actually destroyed.
**         GRAPH_IN_USE if the vertex still has references to it.
*/
graph_error_t 
graph_vertex_destroy(	graph_t *graph, 
			vertex_hdl_t vhdl)
{
	graph_vertex_t *vertex;
	graph_error_t status;
	graph_vertex_group_t *vertex_group;
	int junk_groupid;
	int junk_grpidx;
	int refcnt;

	if (graph == NULL)
		return(GRAPH_BAD_PARAM);

	status = graph_vhandle_get_vertex(graph, vhdl, &vertex,
		&vertex_group, &junk_groupid, &junk_grpidx);

	if (status != GRAPH_SUCCESS)
		return(status);

	mrlock(&vertex_group->group_mrlock, MR_UPDATE, PZERO);
	refcnt = vertex->v_refcnt;
	ASSERT(refcnt >= 1);

	if (refcnt == 1) {
		(void)(VERTEX_UNREF(vhdl, vertex));
		graph_vertex_free(graph, vhdl);
		mrunlock(&vertex_group->group_mrlock);
		return(GRAPH_SUCCESS);
	}

	mrunlock(&vertex_group->group_mrlock);

	return(GRAPH_IN_USE);
}


/*
** graph_vertex_get_next is used to walk the entire list of vertices.
**
** It's possible for vertices to be added after we've already skipped over them.  
** The caller must deal with this (possibly be using the graph generation number
** information).
**
** Along the way, vertices are automatically referenced and unreferenced.  If the
** caller needs to retain a reference to a vertex obtained through get_next, he
** should use graph_vertex_ref explicitly.
**
** When there are no more vertex handles to return, get_next returns GRAPH_HIT_LIMIT.
*/
graph_error_t
graph_vertex_get_next(	graph_t *graph,
			vertex_hdl_t *vertex_found,
			graph_vertex_place_t *placeptr)
{
	int groupid, grpidx, junk_groupid, junk_grpidx;
	unsigned int graph_num_group;
	graph_vertex_t *vertex;
	graph_vertex_group_t *vertex_group, *junk_vertex_group;
	graph_error_t status;
	vertex_hdl_t vertex_handle, old_vertex_handle;

	if (graph == NULL)
		return(GRAPH_BAD_PARAM);

	if (vertex_found == NULL)
		return(GRAPH_BAD_PARAM);

	old_vertex_handle = (vertex_hdl_t)*placeptr;

	if (old_vertex_handle != GRAPH_VERTEX_PLACE_NONE) {
		vertex_handle = old_vertex_handle+1; /* Advance to next vertex handle */
	} else
		vertex_handle = 1;

	groupid = HANDLE_TO_GROUPID(graph, vertex_handle);
	grpidx = HANDLE_TO_GRPIDX(graph, vertex_handle);
	graph_num_group = graph->graph_num_group;
	for (; groupid < graph_num_group; groupid++) {
		vertex_group = GROUPID_TO_GROUP(graph, groupid);
		mrlock(&vertex_group->group_mrlock, MR_ACCESS, PZERO);
		vertex_handle = GRPIDX_TO_HANDLE(groupid, grpidx);
		if (GROUPID_TO_GROUP(graph, groupid)->group_count != 0) {
			for (; grpidx < GRAPH_VERTEX_PER_GROUP; grpidx++, vertex_handle++) {

				status = graph_vhandle_get_vertex(graph, vertex_handle, &vertex,
						&junk_vertex_group, &junk_groupid, &junk_grpidx);

				if (status == GRAPH_SUCCESS) {
					(VERTEX_REF(vertex_handle, vertex));
					mrunlock(&vertex_group->group_mrlock);
					*vertex_found = vertex_handle;
					*placeptr = (graph_vertex_place_t)vertex_handle;
					return(GRAPH_SUCCESS);
				}

				if (status == GRAPH_HIT_LIMIT) {
					mrunlock(&vertex_group->group_mrlock);
					return(GRAPH_HIT_LIMIT);
				}
			}
		}
		mrunlock(&vertex_group->group_mrlock);
		grpidx = 0;
	}

	return(GRAPH_HIT_LIMIT);
}



/*
** graph_vertex_ref is used to indicate that there is an additional
** reference to the specified vertex (increments a reference count).
** Caller should graph_vertex_unref the vertex when he's done with it.
*/
graph_error_t
graph_vertex_ref(graph_t *graph, vertex_hdl_t vhdl)
{
	graph_vertex_t *vertex;
	graph_error_t status;
	graph_vertex_group_t *vertex_group;
	int junk_groupid;
	int junk_grpidx;

	if (graph == NULL)
		return(GRAPH_BAD_PARAM);

	status = graph_vhandle_get_vertex(graph, vhdl, &vertex,
		&vertex_group, &junk_groupid, &junk_grpidx);

	if (status != GRAPH_SUCCESS)
		return(status);

	ASSERT(vertex->v_refcnt > 0);
	(VERTEX_REF(vhdl, vertex));

	return(GRAPH_SUCCESS);
}


/*
** graph_vertex_unref is used to indicate that a vertex is no longer
** in use by a particular user (decrements a reference count).  When 
** the vertex is no longer in use by anyone, it is freed.
**
** Returns GRAPH_SUCCESS if the vertex was actually destroyed.
**         GRAPH_IN_USE if the vertex still has references to it.
*/
graph_error_t
graph_vertex_unref(graph_t *graph, vertex_hdl_t vhdl)
{
	graph_vertex_t *vertex;
	graph_error_t status;
	graph_vertex_group_t *vertex_group;
	int junk_groupid;
	int junk_grpidx;
	int refcnt;

	if (graph == NULL)
		return(GRAPH_BAD_PARAM);

	status = graph_vhandle_get_vertex(graph, vhdl, &vertex,
		&vertex_group, &junk_groupid, &junk_grpidx);

	if (status != GRAPH_SUCCESS)
		return(status);

	refcnt = (VERTEX_UNREF(vhdl, vertex));
	ASSERT(refcnt >= 0);
	if (refcnt == 0) {
		mrlock(&vertex_group->group_mrlock, MR_UPDATE, PZERO);
		graph_vertex_free(graph, vhdl);
		mrunlock(&vertex_group->group_mrlock);
	}

	return(GRAPH_SUCCESS);
}


/**********************************************/
/* Operations on EDGES: add, remove, get_next */
/**********************************************/

/*
** graph_edge_add adds a labelled edge from source_vertex to target_vertex.
*/
graph_error_t 
graph_edge_add(	graph_t *graph,
		vertex_hdl_t source_vertex_handle,
		vertex_hdl_t target_vertex_handle,
		char *edge_name)
{
	graph_error_t status;
	graph_vertex_t *source_vertex, *target_vertex;
	graph_vertex_group_t *source_vertex_group, *target_vertex_group;
	int source_groupid, source_grpidx, target_groupid, target_grpidx;
	int num_edge, num_info_LBL;
	int new_vinfo_size, save_vinfo_size = 0;
	graph_info_t *new_list = GRAPH_VERTEX_NOINFO;
	graph_info_t *old_edges, *old_info_LBL, *new_edges, *new_info_LBL;
	graph_info_t *info_to_free;
	char *name;
	int i;

	if (graph == NULL)
		return(GRAPH_BAD_PARAM);

	if (edge_name == NULL)
		return(GRAPH_BAD_PARAM);

	name = string_table_insert(&graph->graph_string_table, edge_name);
	ASSERT(name != NULL);

	/*
	 * Use vertex handle to find target vertex.
	 */
	status = graph_vhandle_get_vertex(graph, target_vertex_handle, &target_vertex,
				&target_vertex_group, &target_groupid, &target_grpidx);
	if (status != GRAPH_SUCCESS)
		return(status);

	/*
	 * Use vertex handle to find source vertex.
	 */
	status = graph_vhandle_get_vertex(graph, source_vertex_handle, &source_vertex,
				&source_vertex_group, &source_groupid, &source_grpidx);
	if (status != GRAPH_SUCCESS)
		return(status);

	mrlock(&source_vertex_group->group_mrlock, MR_UPDATE, PZERO);
again:
	num_edge = source_vertex->v_num_edge;
	num_info_LBL = source_vertex->v_num_info_LBL;

	new_vinfo_size = VERTEX_INFO_SIZE(num_edge+1, num_info_LBL);

	/*
	 * If we haven't already allocated an info area, or if the one we
	 * allocated last time through this section isn't the right size,
	 * we need to allocate a new one.
	 */
	if (new_vinfo_size != save_vinfo_size) {
		unsigned long old_generation = graph->graph_generation;

		mrunlock(&source_vertex_group->group_mrlock);

		/*
		 * If we allocated one earlier, it's the wrong size.  Free it.
		 */
		if (save_vinfo_size)
			graph_free(new_list);

		/*
		 * Create a new info area.
		 */
		save_vinfo_size = new_vinfo_size;
		if (new_vinfo_size != 0) {
			new_list = graph_allocate(new_vinfo_size);
			if (new_list == NULL)
				return(GRAPH_CANNOT_ALLOC);
		} else
			new_list = GRAPH_VERTEX_NOINFO;

		mrlock(&source_vertex_group->group_mrlock, MR_UPDATE, PZERO);

		/*
		 * If graph changed while we waited to allocate, start again.
		 */
		if (old_generation != graph->graph_generation)
			goto again;
	}

	/*
	 * At this point, we are committed to adding the edge, if there isn't
	 * already one there with the same name.
	 */
	old_edges = VERTEX_EDGE_PTR(source_vertex);
	new_edges = new_list;

	/* 
	 * Look for matching edge name.
	 */
	for (i=0; i<num_edge; i++) {
		if (!strcmp(edge_name, old_edges[i].i_label)) {
			/* Not allowed to add duplicate edge names. */
			mrunlock(&source_vertex_group->group_mrlock);
			graph_free(new_list);
			return(GRAPH_DUP);
		}
		new_edges[i] = old_edges[i]; /* structure copy */
	}

	if (num_info_LBL != 0) {
		old_info_LBL = VERTEX_INFO_LBL_PTR(source_vertex);
		new_info_LBL = &new_list[num_edge+1];

		bcopy(old_info_LBL, new_info_LBL, num_info_LBL * sizeof(graph_info_t));
	}

	new_edges[num_edge].i_label = name;
	new_edges[num_edge].i_info = target_vertex_handle;

	(void)(VERTEX_REF(target_vertex_handle, target_vertex));

	if (VERTEX_HASINFO(source_vertex))
		info_to_free = source_vertex->v_info_list;
	else
		info_to_free = NULL;

	source_vertex->v_info_list = new_list;
	source_vertex->v_num_edge = num_edge+1;

	atomicAddUlong(&graph->graph_generation, 1);
	mrunlock(&source_vertex_group->group_mrlock);

	if (info_to_free != NULL)
		graph_free(info_to_free);

	return(GRAPH_SUCCESS);
}

/*
** graph_edge_get finds which vertex the specified edge points to.
*/
graph_error_t 
graph_edge_get(	graph_t *graph,
		vertex_hdl_t vertex_handle,
		char *edge_name,
		vertex_hdl_t *target_handle)
{
	graph_error_t status;
	graph_vertex_t *vertex;
	graph_vertex_group_t *vertex_group;
	int groupid, grpidx;
	graph_info_t *edge_list;
	int num_edge;
	int i;

	if (graph == NULL)
		return(GRAPH_BAD_PARAM);

	if (edge_name == NULL)
		return(GRAPH_BAD_PARAM);

	/*
	 * Use vertex handle to find vertex.
	 */
	status = graph_vhandle_get_vertex(graph, vertex_handle, &vertex,
						&vertex_group, &groupid, &grpidx);
	if (status != GRAPH_SUCCESS)
		return(status);

	mrlock(&vertex_group->group_mrlock, MR_ACCESS, PZERO);

	edge_list = VERTEX_EDGE_PTR(vertex);
	num_edge = vertex->v_num_edge;

	/* 
	 * See if edge under edge_name exists.
	 */
	for (i=0; i<num_edge; i++)
		if (!strcmp(edge_name, edge_list[i].i_label)) {
			if (target_handle != NULL) {
				*target_handle = edge_list[i].i_info;
				(void)graph_vertex_ref(graph, *target_handle);
			}

			mrunlock(&vertex_group->group_mrlock);
			return(GRAPH_SUCCESS);
		}

	mrunlock(&vertex_group->group_mrlock);
	return(GRAPH_NOT_FOUND);
}


/*
** graph_edge_get_next is used to walk all edges of a given vertex.  On entry, 
** if *placeptr is EDGE_PLACE_NONE, this routine returns the first edge in this 
** vertex.
**
** This routine returns both the name of the edge and the vertex it targets.
** The caller holds a reference to the target vertex, so he must graph_vertex_unref 
** the target when he's done with it.
*/
graph_error_t
graph_edge_get_next(	graph_t *graph,
			vertex_hdl_t vertex_handle,
			char *buffer,
			vertex_hdl_t *targetp,
			graph_edge_place_t *placeptr)
{
	graph_error_t status;
	uint which_edge;
	graph_vertex_t *vertex;
	graph_vertex_group_t *vertex_group;
	int groupid, grpidx;
	graph_info_t *edge_list;

	if (graph == NULL)
		return(GRAPH_BAD_PARAM);

	if (placeptr == NULL)
		return(GRAPH_BAD_PARAM);

	if (*placeptr == GRAPH_EDGE_PLACE_NONE)
		which_edge = 0;
	else
		which_edge = *placeptr - graph->graph_reserved_places;

	status = graph_vhandle_get_vertex(graph, vertex_handle, &vertex,
					&vertex_group, &groupid, &grpidx);
	if (status != GRAPH_SUCCESS)
		return(status);

	mrlock(&vertex_group->group_mrlock, MR_ACCESS, PZERO);

	if (which_edge >= vertex->v_num_edge) {
		mrunlock(&vertex_group->group_mrlock);
		return(GRAPH_HIT_LIMIT);
	}

	edge_list = VERTEX_EDGE_PTR(vertex);

	if (buffer)
		strcpy(buffer, edge_list[which_edge].i_label);

	if (targetp) {
		*targetp = edge_list[which_edge].i_info;
		(void)graph_vertex_ref(graph, *targetp);
	}

	mrunlock(&vertex_group->group_mrlock);

	*placeptr = which_edge + 1 + graph->graph_reserved_places;

	return(GRAPH_SUCCESS);
}



/***************************************************************************/
/* Operations on LABELLED INFORMATION: add, remove, replace, get, get_next */
/***************************************************************************/

/*
** graph_info_add_LBL associates the specified arbitrary information with the 
** specified vertex and information label.
*/
graph_error_t 
graph_info_add_LBL(	graph_t *graph,
			vertex_hdl_t vertex_handle,
			char *info_name,
                        arb_info_desc_t info_desc,
			arbitrary_info_t info)
{
	graph_error_t status;
	graph_vertex_t *vertex;
	graph_vertex_group_t *vertex_group;
	int groupid, grpidx;
	int num_edge, num_info_LBL;
	int new_vinfo_size, save_vinfo_size = 0;
	graph_info_t *new_list = GRAPH_VERTEX_NOINFO;
	graph_info_t *old_edges, *old_info_LBL, *new_edges, *new_info_LBL;
	graph_info_t *info_to_free;
	char *name;
	int i;

	if (graph == NULL)
		return(GRAPH_BAD_PARAM);

	if (info_name == NULL)
		return(GRAPH_BAD_PARAM);

	name = string_table_insert(&graph->graph_string_table, info_name);
	ASSERT(name != NULL);

	/*
	 * Use vertex handle to find vertex.
	 */
	status = graph_vhandle_get_vertex(graph, vertex_handle, &vertex,
						&vertex_group, &groupid, &grpidx);
	if (status != GRAPH_SUCCESS)
		return(status);

	mrlock(&vertex_group->group_mrlock, MR_UPDATE, PZERO);
again:
	num_edge = vertex->v_num_edge;
	num_info_LBL = vertex->v_num_info_LBL;

	new_vinfo_size = VERTEX_INFO_SIZE(num_edge, num_info_LBL+1);

	/*
	 * If we haven't already allocated an info area, or if the one we
	 * allocated last time through this section isn't the right size,
	 * we need to allocate a new one.
	 */
	if (new_vinfo_size != save_vinfo_size) {
		unsigned long old_generation = graph->graph_generation;

		mrunlock(&vertex_group->group_mrlock);

		/*
		 * If we allocated one earlier, it's the wrong size.  Free it.
		 */
		if (save_vinfo_size)
			graph_free(new_list);

		/*
		 * Create a new info area.
		 */
		save_vinfo_size = new_vinfo_size;
		if (new_vinfo_size != 0) {
			new_list = graph_allocate(new_vinfo_size);
			if (new_list == NULL)
				return(GRAPH_CANNOT_ALLOC);
		} else
			new_list = GRAPH_VERTEX_NOINFO;

		mrlock(&vertex_group->group_mrlock, MR_UPDATE, PZERO);

		/*
		 * If graph changed while we waited to allocate, start again.
		 */
		if (old_generation != graph->graph_generation)
			goto again;
	}

	/*
	 * At this point, we are committed to adding the labelled info, if there 
	 * isn't already information there with the same name.
	 */
	old_info_LBL = VERTEX_INFO_LBL_PTR(vertex);
	new_info_LBL = &new_list[num_edge];

	/* 
	 * Look for matching info name.
	 */
	for (i=0; i<num_info_LBL; i++) {
		if (!strcmp(info_name, old_info_LBL[i].i_label)) {
			/* Not allowed to add duplicate labelled info names. */
			mrunlock(&vertex_group->group_mrlock);
			graph_free(new_list);
			return(GRAPH_DUP);
		}
		new_info_LBL[i] = old_info_LBL[i]; /* structure copy */
	}

	if (num_edge != 0) {
		old_edges = VERTEX_EDGE_PTR(vertex);
		new_edges = new_list;

		bcopy(old_edges, new_edges, num_edge * sizeof(graph_info_t));
	}

	new_info_LBL[num_info_LBL].i_label = name;
	new_info_LBL[num_info_LBL].i_info = info;

	if (VERTEX_HASINFO(vertex))
		info_to_free = vertex->v_info_list;
	else
		info_to_free = NULL;

	vertex->v_info_list = new_list;
	vertex->v_num_info_LBL = num_info_LBL+1;

	atomicAddUlong(&graph->graph_generation, 1);
	mrunlock(&vertex_group->group_mrlock);

	if (info_to_free != NULL)
		graph_free(info_to_free);

	return(GRAPH_SUCCESS);
}


/*
** graph_info_get_LBL retrieves arbitrary labelled information from the specicified 
** vertex.
*/
graph_error_t 
graph_info_get_LBL(	graph_t *graph,
			vertex_hdl_t vertex_handle,
			char *info_name,
                        arb_info_desc_t *info_desc,
			arbitrary_info_t *info)
{
	graph_error_t status;
	graph_vertex_t *vertex;
	graph_vertex_group_t *vertex_group;
	int groupid, grpidx;
	unsigned int num_info_LBL;
	graph_info_t *info_list_LBL;
	int i;

	if (graph == NULL)
		return(GRAPH_BAD_PARAM);

	if (info_name == NULL)
		return(GRAPH_BAD_PARAM);

	/*
	 * Use vertex handle to find vertex.
	 */
	status = graph_vhandle_get_vertex(graph, vertex_handle, &vertex,
					&vertex_group, &groupid, &grpidx);
	if (status != GRAPH_SUCCESS)
		return(GRAPH_BAD_PARAM);

	mrlock(&vertex_group->group_mrlock, MR_ACCESS, PZERO);

	num_info_LBL = vertex->v_num_info_LBL;
	info_list_LBL = VERTEX_INFO_LBL_PTR(vertex);

	/* 
	 * Find information under info_name.
	 */
	for (i=0; i<num_info_LBL; i++)
		if (!strcmp(info_name, info_list_LBL[i].i_label)) {
			if (info != NULL)
				*info = info_list_LBL[i].i_info;

			mrunlock(&vertex_group->group_mrlock);
			return(GRAPH_SUCCESS);
		}

	mrunlock(&vertex_group->group_mrlock);
	return(GRAPH_NOT_FOUND);
}


/***************************************************/
/* Operations on INDEXED INFORMATION: replace, get */
/***************************************************/

/*
** graph_info_get_IDX retrieves arbitrary information from the specicified vertex and 
** index and places it into the caller's buffer.  The index must be less than the 
** num_idx_info attribute specified to create_graph.
**
** No locking is required since we're atomically accessing a single longword.
*/
graph_error_t 
graph_info_get_IDX(	graph_t *graph,
			vertex_hdl_t vertex_handle,
			unsigned int index,
			arbitrary_info_t *info)
{
	graph_error_t status;
	graph_vertex_t *vertex;
	graph_vertex_group_t *vertex_group;
	int groupid, grpidx;
	arbitrary_info_t *info_list_IDX;

	if (graph == NULL)
		return(GRAPH_BAD_PARAM);

	if (index >= graph->graph_num_index)
		return(GRAPH_BAD_PARAM);

	/*
	 * Use vertex handle to find vertex.
	 */
	status = graph_vhandle_get_vertex(graph, vertex_handle, &vertex,
					&vertex_group, &groupid, &grpidx);
	if (status != GRAPH_SUCCESS)
		return(status);

	/*
	 * Return information at the appropriate index in this vertex.
	 */
	info_list_IDX = VERTEX_INFO_IDX_PTR(vertex);
	if (info != NULL)
		*info = info_list_IDX[index];

	return(GRAPH_SUCCESS);
}


/****************************************/
/* Operations on STRINGS: get_component */
/****************************************/

/*
** graph_string_get_component extracts the next component from lookup_string.  Return 
** the component and its length.  Components in lookup_string are separated by the
** separator character specified to graph_create.  The length returned is the number of
** real characters in the string (does NOT include separator or terminator).  Also 
** returned is the number of separator characters skipped over before a component was 
** found.
*/
graph_error_t
graph_string_get_component(	graph_t *graph,
				char *lookup_string,
				char *component,
				int *separator_length,
				int *component_length)
{
	char separator;
	int separator_count;
	int i;

	if (graph == NULL)
		return(GRAPH_BAD_PARAM);

	if (lookup_string == NULL)
		return(GRAPH_BAD_PARAM);

	if (component == NULL)
		return(GRAPH_BAD_PARAM);

	/*
	 * Skip over redundant separator characters.
	 */
	separator_count = 0;
	separator = graph->graph_separator_char;
	while (lookup_string[separator_count] == separator)
		separator_count++;
	lookup_string += separator_count;

	/*
	 * Now, copy component to caller's buffer.
	 */
	for (i=0; i<LABEL_LENGTH_MAX; i++) {
		if ((lookup_string[i] == '\0') ||
		    (lookup_string[i] == separator)) {
			component[i] = 0;
			break;
		} else
			component[i] = lookup_string[i];
	}

	if (i == LABEL_LENGTH_MAX)
		return(GRAPH_HIT_LIMIT);

	if (separator_length != NULL)
		*separator_length = separator_count;

	if (component_length != NULL)
		*component_length = i;

	return(GRAPH_SUCCESS);
}
