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
#ifndef _GRAPH_GRAPH_PRIVATE_H
#define _GRAPH_GRAPH_PRIVATE_H

#ident	"$Revision: 1.2 $"

#if _USER_MODE_TEST
#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#define graph_allocate(x) (malloc(x))
#define graph_free(x) (free(x))

#define atomicAddUlong(addr, val) *(addr) += (val)
#define atomicAddInt(addr, val) *(addr) += (val)
#else /* _USER_MODE_TEST */
#include <sys/atomic_ops.h>
/* 
** For now, graph_allocate will sleep and guarantee that allocations complete.
** Calling code always checks for NULL anyway and returns GRAPH_CANNOT_ALLOC
** just in case this changes.
*/
#define graph_allocate(x) (kern_calloc(x, 1))
#define graph_free(x) (kern_free(x))
#endif /* _USER_MODE_TEST */

/*
** Arbitrary information associated with each vertex under an arbitrary string 
** label.  Edges are stored as labelled information where the i_info field
** is a vertex handle.
*/
typedef struct graph_info_s {
	char			*i_label;
	arbitrary_info_t	i_info;
} graph_info_t;


/*
** Represents a vertex in a graph.  Arbitrary long-sized information can be 
** stored with and retrieved from a vertex.  There are two ways to store and 
** retrieve information: By label and by index.  Storing and retrieving by 
** index is faster than retrieving by label, but it reserves a small amount 
** of memory for each vertex (whether or not each vertex actually has indexed
** information).  Index'ed retrieval should be used when many nodes include 
** a particular piece of information that should be accessed quickly.  Storing 
** and retrieving by label is more generic, but it is somewhat slower.
**
** Current structures have been chosen based on the idea that there could be 
** lots of vertices, but each vertex will have relatively few outgoing edges 
** and relatively little "labelled information" associated with each vertex.  
** If these assumptions are violated, edge and "information" data should be 
** stored at each vertex using more sophisticated structures.
**
** When a vertex grows or shrinks due to changes in edge or LBL info, a new 
** labelled info area is allocated, and information is copied from the old 
** area to the new area.  Access to edges and labelled information therefore
** requires synchronization.  Access to indexed information, though, is
** lock-free.  Once space is allocated for indexed information it never moves; 
** labelled information, though, may be relocated as it grows and shrinks.
**
** Reference counts are kept in order to allow drivers to delete vertices
** even though there are potentially other modules that have attached arbitrary
** information to the vertex.  The general rules regarding vertex ref counts are:
**	-A vertex is created with an initial reference (the creation reference).
**	-Every edge that points to a vertex causes an additional reference on
**	 that vertex.
**	-Every operation which returns a vertex handle (or a dev) causes an 
**	 additional reference on the corresponding vertex.
**	-Every graph operation which accepts a vertex handle as input expects
**	 that the caller has acquired a reference to that vertex through the 
**	 graph operations; the caller must not relinquish the reference that
**	 allowed it to invoke the graph operation until the operation has .
**	 completed.
**	-The vertex iterator, graph_vertex_get_next, automatically unreferences
**	 the "previous" vertex and references the "next" vertex.  If iteration
**	 is terminated prematurely, it is the caller's responsibility to
**	 unreference the last vertex as appropriate.
**	-Commonly, a calling module creates its own data structure with its
**	 own reference count and stores a vertex handle.  In this case, there
**	 is actually only one additional reference on the vertex, though there
**	 may be many users of the module's data structure.  When the data
**	 structure is destroyed, the module must unreference the vertex once.
**	-When there are no references to a vertex, the system may reuse the 
**	 vertex and the corresponding handle for other purposes.
*/
typedef struct graph_vertex_s {
	int			v_refcnt;	/* number of references to this vertex */
	unsigned int		v_num_edge;	/* number of edges in this vertex */
	unsigned int		v_num_info_LBL;	/* number of labelled info's */
	graph_info_t		*v_info_list;	/* pointer to array of labelled info */
	/* arbitrary_info_t	v_info_list_IDX[];			*/
} graph_vertex_t;

#if DEBUG
extern vertex_hdl_t trace_vhdl;
static void VERTEX_REF(vertex_hdl_t, graph_vertex_t *);
static int VERTEX_UNREF(vertex_hdl_t, graph_vertex_t *);
#else
#define VERTEX_REF(vhdl, vertex_ptr) (void)atomicAddInt(&(vertex_ptr)->v_refcnt, 1)
#define VERTEX_UNREF(vhdl, vertex_ptr) atomicAddInt(&(vertex_ptr)->v_refcnt, -1)
#endif

/* Size of labelled info in a vertex */
#define VERTEX_INFO_SIZE(num_edge, num_info_LBL) \
	(((num_info_LBL) + (num_edge)) * sizeof(graph_info_t))

/* Pointer to first edge */
#define VERTEX_EDGE_PTR(vertex)  (&((vertex)->v_info_list[0]))

/* Pointer to first IDX info */
#define VERTEX_INFO_IDX_PTR(vertex) ((arbitrary_info_t *)((long)(vertex)+sizeof(graph_vertex_t)))

/*
** Pointer to first piece of labelled information
** WARNING: This macro counts on v_num_edge to have been set correctly BEFORE it's called!
*/
#define VERTEX_INFO_LBL_PTR(vertex) ((graph_info_t *)(&((vertex)->v_info_list[(vertex)->v_num_edge])))

/*
** Sentinel values used in place of a v_info_list pointer to indicate
** that a new vertex is free or that a vertex has no labelled information.
*/
#define GRAPH_VERTEX_FREE	(graph_info_t *)0L
#define GRAPH_VERTEX_NOINFO	(graph_info_t *)1L
#define GRAPH_VERTEX_NEW	(graph_info_t *)2L
#define GRAPH_VERTEX_SPECIAL	2L

#define VERTEX_ISFREE(vertex) ((vertex)->v_info_list == GRAPH_VERTEX_FREE)
#define VERTEX_ISNEW(vertex) ((vertex)->v_info_list == GRAPH_VERTEX_NEW)
#define VERTEX_HASINFO(vertex) ((ulong)(vertex)->v_info_list > GRAPH_VERTEX_SPECIAL)


/* 
** Vertices within a graph are arranged into "groups" in order to save space and
** to reduce locking contention.  When a new vertex is allocated, we try to find 
** space in the existing groups; but if all current groups are full, we start a 
** new group.  It is at group creation time that space is pre-allocated for indexed 
** information for every vertex in the group.  The total number of vertices that 
** can be supported in a graph is limited by the number of groups and the number 
** of vertices per group.
*/

/* The default number of groups of vertices in a graph */
#define DEFAULT_GRAPH_NUM_GROUP 128

/* 
** The number of vertices per group.
** For now, this count is the same for every graph in the system.
** (If we ever have multiple graphs, this value could become per-graph.)
*/
#define GRAPH_VERTEX_PER_GROUP 128
#define GRAPH_VERTEX_PER_GROUP_LOG2 7
#define GRAPH_VERTEX_PER_GROUP_MASK 0x7f

/*
** Description of a group of GRAPH_VERTEX_PER_GROUP vertices.
** Vertices are allocated to the graph one group at a time.
*/
typedef struct graph_vertex_group_s {
	mrlock_t		group_mrlock;			/* lock for this group */
								/* guards changes to edges */
								/* guards changes to LBL info */
								/* guards addition/deletion of vertices */

	int			group_count;			/* count of vertices in group */

	graph_vertex_t		*group_vertex_array;		/* array of vertices */
} graph_vertex_group_t;

/* Size of the group_vertex_array.  Assumes graph_vertex_size has already been filled in */
#define SIZE_GROUP_VERTEX_ARRAY(graph) (GRAPH_VERTEX_PER_GROUP*(graph)->graph_vertex_size)

/*
** Represents a directed graph.  A graph consists of a collection of 0 or more vertices 
** connected by labelled edges to other vertices.  Arbitrary information is associated 
** with each vertex of the graph.
*/
typedef struct graph_s {
	char			*graph_name;			/* for debuging */

	unsigned long		graph_generation;		/* incremented whenever the */
								/* graph is changed in a */
								/* significant way */

	char			graph_separator_char;		/* char used to separate */
								/* components in strings */

	unsigned int		graph_reserved_places;		/* count of small integer values */
								/* reserved by caller for his */
								/* own use as special "place" */
								/* parameters */

	unsigned int		graph_num_group;		/* number of groups of vertices */

	unsigned int		graph_num_index;		/* number of pieces of indexed */
								/* information per vertex */

	int			graph_num_vertex;		/* current number of vertices */

	mrlock_t		graph_mrlock;			/* lock for graph. */
								/* guards addition of new groups */
								/* guards handle_limit */

	unsigned int		graph_vertex_handle_limit;	/* maximum number of vertex */
								/* handles that can fit in */
								/* the graph (without growth) */

	unsigned int		graph_vertex_size;		/* for perf: size of a vertex */

	unsigned int		graph_group_rotor;		/* for perf: group rotor value */

	struct string_table	graph_string_table;		/* permanent (non-stack) */
								/* storage for labels */

	/* Must be last */
	graph_vertex_group_t	graph_vertex_group[1];		/* groups of vertices */
} graph_t;

/* Convert handle into group ID */
#define HANDLE_TO_GROUPID(graph, h) ((h)>>GRAPH_VERTEX_PER_GROUP_LOG2)

/* Convert handle into an index into the group  */
#define HANDLE_TO_GRPIDX(graph, h) ((h) & GRAPH_VERTEX_PER_GROUP_MASK)

/* Convert a groupid/grpidx pair into a handle */
#define GRPIDX_TO_HANDLE(groupid, grpidx) \
	(((groupid)<<GRAPH_VERTEX_PER_GROUP_LOG2) | (grpidx))

/* Convert (groupid, group index) pair into a vertex pointer */
#define GRPIDX_TO_VERTEX(graph, groupid, grpidx) \
	((graph_vertex_t *)((char *)((graph)->graph_vertex_group[groupid].group_vertex_array) + \
	(grpidx)*(graph)->graph_vertex_size))

/* Convert groupid to group pointer */
#define GROUPID_TO_GROUP(graph, groupid) \
	(&(graph)->graph_vertex_group[(groupid)])

/* Convert handle into a vertex pointer */
#define HANDLE_TO_VERTEX(graph, h) \
	GRPIDX_TO_VERTEX((graph), HANDLE_TO_GROUPID((graph), (h)), HANDLE_TO_GRPIDX((graph), (h)))

#endif /* _GRAPH_GRAPH_PRIVATE_H */
