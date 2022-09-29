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

#ident	"$Revision: 1.9 $"

#include <sys/types.h>
#include <sys/cmn_err.h>
#include <sys/conf.h>
#include <sys/debug.h>
#include <sys/driver.h>
#include <sys/errno.h>
#include <sys/invent.h>
#include <sys/pathname.h>
#include <sys/vnode.h>
#include <string.h>
#include <sys/systm.h>
#include <sys/kmem.h>
#include <sys/hwgraph.h>
#include <sys/iograph.h>

/*
** The "hwgraph" or "hardware graph" module provides a personality on top of the
** basic graph module that is particularly suited for the graph that represents
** devices, hardware, and inventory.  In general, there is a vertex in the hwgraph
** for every special file.  For example, a vertex for a disk slice, and for a 
** console device.
**
** The hardware graph uses fast "indexed information" at each vertex to store the
** character and block device switch (cdevsw and bdevsw) entries corresponding to
** the device (if any).  It also defines a "connect point" which is the place
** in the graph that the vertex was first connected to.  While somewhat arbitrary,
** it allows some convenient semantics for the hardware graph file system (hwgfs).
**
** A dev_t consists of a major and minor number.  If the major number is L_MAXMAJ,
** then the minor number is actually a vertex handle!  Thus, from a dev_t we get
** to the vertex.  From a vertex, we get to generic information including cdevsw
** and bdevsw entries for the driver.
**
** Some basic design principles of the hwgraph layer:
**	-require no special locking above this layer
**	-minimize space usage.  Expect 1000's of vertices.
**	-allow drivers to manage a chunk of the name space.
**	-make no significant user-visible changes above the vnode layer
**	-continue to allow dev_t to be used for the same uses as it is 
**	 currently used.
*/


graph_hdl_t hwgraph;
vertex_hdl_t hwgraph_root;
int hwgraph_initted = 0;
mrlock_t hwgraph_connectpt_mrlock;	/* avoid connectpt set/get races */

static vertex_hdl_t do_hwgraph_connectpt_get(vertex_hdl_t);
graph_error_t graph_string_get_component(graph_hdl_t, 
					 char *, char *,
					 int *, int *) ;

/**********************************************/
/* Operations on EDGES: add, remove, get_next */
/**********************************************/

/*
** See if a vertex has an outgoing edge with a specified name.
** Vertices in the hwgraph *implicitly* contain these edges:
**	"." 	refers to "current vertex"
**	".." 	refers to "connect point vertex"
**	"c"	refers to current vertex (character device access)
**	"b"	refers to current vertex (block device access)
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


/***************************************************************************/
/* Operations on LABELLED INFORMATION: add, remove, replace, get, get_next */
/***************************************************************************/

graph_error_t
hwgraph_info_add_LBL(	vertex_hdl_t vhdl, 
			char *name, 
			arbitrary_info_t info)
{
	return(graph_info_add_LBL(hwgraph, vhdl, name, NULL, info));
}

graph_error_t
hwgraph_info_get_LBL(	vertex_hdl_t vhdl, 
			char *name, 
			arbitrary_info_t *infop)
{
	return(graph_info_get_LBL(hwgraph, vhdl, name, NULL, infop));
}

/****************************/
/* Support for file systems */
/****************************/

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

vertex_hdl_t
hwgraph_connectpt_get(vertex_hdl_t vhdl)
{
	vertex_hdl_t connect;

	mrlock(&hwgraph_connectpt_mrlock, MR_ACCESS, PZERO);
	connect = do_hwgraph_connectpt_get(vhdl);
	mrunlock(&hwgraph_connectpt_mrlock);
	return(connect);
}


graph_error_t
hwgraph_path_lookup(    vertex_hdl_t start_vertex_handle,
                        char *lookup_string,
                        vertex_hdl_t *vertex_handle_ptr,
                        char **remainder)
{
        graph_error_t rv;
        graph_error_t status;

        ASSERT(hwgraph_initted);

        if (lookup_string == NULL)
                return(GRAPH_BAD_PARAM);

        /*
         * Walk through lookup_string "path" traversing edges along the way.
         */
        for(;;) {
                char component[LABEL_LENGTH_MAX];
                int separator_length;
                int component_length;

                /*
                 * Pull out the next piece of the lookup_string.
                 */
                status = graph_string_get_component(    hwgraph,
                                                        lookup_string,
                                                        component,
                                                        &separator_length,
                                                        &component_length);
                if (status != GRAPH_SUCCESS)
                        return(status);

                lookup_string += separator_length;

                /*
                 * If finished parsing string, exit loop.
                 */
                if (component_length == 0)
                        break;

                /*
                 * Otherwise, try to follow the edge.
                 */
                rv = hwgraph_edge_get(  start_vertex_handle,
                                        component,
                                        &start_vertex_handle);

                if (rv != GRAPH_SUCCESS)
                        break;

                lookup_string += component_length;
        }

        if (remainder != NULL)
                *remainder = lookup_string;

        if (vertex_handle_ptr != NULL)
                *vertex_handle_ptr = start_vertex_handle;

        return(rv);
}
