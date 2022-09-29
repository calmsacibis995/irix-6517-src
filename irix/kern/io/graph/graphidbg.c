/**************************************************************************
 *									  *
 *	 	Copyright (C) 1994-1995 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded	instructions, statements, and computer programs	 contain  *
 *  unpublished	 proprietary  information of Silicon Graphics, Inc., and  *
 *  are	protected by Federal copyright law.  They  may	not be disclosed  *
 *  to	third  parties	or copied or duplicated	in any form, in	whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ident	"$Revision: 1.10 $"

#include <sys/types.h>
#include <sys/idbg.h>
#include <sys/idbgentry.h>
#include <sys/kmem.h>
#include <sys/systm.h>
#include <sys/graph.h>
#include <sys/strtbl.h>
#include <sys/hwgraph.h>
#include "graph_private.h"


extern void idbg_graph_help(graph_t *);
extern void idbg_graph_summ(graph_t *);
extern void idbg_graph_handles(vertex_hdl_t);
extern void idbg_graph_vertex(graph_vertex_t *);
extern void idbg_graph_vertex_name(vertex_hdl_t);
#if defined(DEBUG)
extern void idbg_graph_reference_count(vertex_hdl_t);
#endif

#define	VD	(void (*)())

static struct graphif {
	char	*name;
	void	(*func)(int);
	char	*help;
} graphidbg_funcs[] = {
    "graph", VD idbg_graph_help, "Show graph debug routines",
    "gsumm", VD idbg_graph_summ, "gsumm graph: Summarize a graph",
    "ghdls", VD idbg_graph_handles, "ghdl handleid: Show handle-->vertex",
    "gvertex", VD idbg_graph_vertex, "gvertex vertexid: Summarize a vertex",
    "gname", VD idbg_graph_vertex_name, "show hwgraph vertex name",
#if defined(DEBUG)
    "gref", VD idbg_graph_reference_count,"show reference counts",
#endif
    0,		0,	0
};

/*
 * Initialization routine.
 */
void
graphidbginit(void)
{
	struct graphif	*p;

	for (p = graphidbg_funcs; p->func; p++)
		idbg_addfunc(p->name, (void (*)())(p->func));
}

/*
 * Print out the help messages for these functions.
 */
/* ARGSUSED */
void
idbg_graph_help(graph_t *graph)
{
	struct graphif	*p;

	if (graph == (graph_t *)-1L) {
		qprintf("Graph debugging functions:\n");
		for (p = graphidbg_funcs; p->name; p++)
			qprintf("%s %s\n", padstr(p->name, 16), p->help);

		qprintf("Use 'graph graph-address' to change current graph\n");
	} else
		current_graph = graph;

	if (current_graph != NULL)
		qprintf("Current graph is 0x%lx\n", current_graph);
}

/*
 * Print out a summary of a specified graph.
 */
/* ARGSUSED */
void
idbg_graph_summ(graph_t *graph)
{
	if (graph == (graph_t *)-1L)
		graph = current_graph;
	else
		current_graph = graph;

	if (graph == NULL) {
		qprintf("Print summary for which graph?\n");
		qprintf("See 'graph' command for help\n");
		return;
	} 

	qprintf("Summary for graph '%s':\n", graph->graph_name);
	qprintf("generation num 0x%lx sep_char='%c' rsrved=0x%x\n", 
			graph->graph_generation, 
			graph->graph_separator_char,
			graph->graph_reserved_places);

	qprintf("numIDX=0x%x num_vertex=0x%x  handle_limit=0x%x\n",
			graph->graph_num_index,
			graph->graph_num_vertex,
			graph->graph_vertex_handle_limit);

	qprintf("mrlock at 0x%lx  ", &graph->graph_mrlock);

	qprintf("string table at 0x%lx\n", &graph->graph_string_table);

	qprintf("maximum possible vertices=0x%x\n", graph->graph_num_group*GRAPH_VERTEX_PER_GROUP);
}

/*
 * Print a list of all handles in a specified graph.
 */
/* ARGSUSED */
void
idbg_graph_handles(vertex_hdl_t handle)
{
	int groupid, grpidx;
	graph_vertex_group_t *vertex_group;
	graph_vertex_t *vertex;
	unsigned int graph_num_group;

	if (current_graph == NULL) {
		qprintf("List handles for which graph?\n");
		qprintf("See 'graph' command for help\n");
		return;
	}

	if (handle != (vertex_hdl_t)-1L) {
		groupid = HANDLE_TO_GROUPID(current_graph, handle);
		grpidx = HANDLE_TO_GRPIDX(current_graph, handle);
		vertex_group = &current_graph->graph_vertex_group[groupid];
		vertex = GRPIDX_TO_VERTEX(current_graph, groupid, grpidx);

		qprintf("vertex with handle 0x%lx (0x%x/0x%x) at 0x%lx is ",
			handle, groupid, grpidx, vertex);

		if (VERTEX_ISFREE(vertex))
			qprintf("free\n");
		else if (VERTEX_ISNEW(vertex))
			qprintf("new\n");
		else
			qprintf("allocated\n");
		return;
	}

	qprintf("List of vertex handles and vertices '%s' graph:\n",
			current_graph->graph_name);

	graph_num_group = current_graph->graph_num_group;
	for (groupid=0; groupid<graph_num_group; groupid++) {

		vertex_group=&current_graph->graph_vertex_group[groupid];
		if (vertex_group->group_count == 0)
			continue;

		for (grpidx=0; grpidx < GRAPH_VERTEX_PER_GROUP; grpidx++) {
			vertex = GRPIDX_TO_VERTEX(current_graph, groupid, grpidx);
			if (!VERTEX_ISFREE(vertex))
				qprintf("\t0x%x (0x%x/0x%x):\t0x%lx\n",
						GRPIDX_TO_HANDLE(groupid, grpidx),
						groupid, grpidx, vertex);
		}
	}
}


/*
 * Print out a summary of a specified vertex.
 */
/* ARGSUSED */
void
idbg_graph_vertex(graph_vertex_t *vertex)
{
	int i;
	ulong handle = (ulong)vertex;

	if (current_graph == NULL) {
		qprintf("Show vertex for which graph?\n");
		return;
	}

	if (handle < current_graph->graph_num_group*GRAPH_VERTEX_PER_GROUP)
		vertex = HANDLE_TO_VERTEX(current_graph, handle);

	qprintf("Summary of vertex at 0x%lx:\n", vertex);
	if (VERTEX_ISFREE(vertex))
		qprintf("(vertex is currently FREE)\n");
	qprintf("num_edge=0x%x,  num_info_LBL=0x%x  refcnt=%d\n",
		vertex->v_num_edge, vertex->v_num_info_LBL, vertex->v_refcnt);
	if (vertex->v_num_edge > 0) {
		graph_edge_t *edge;

		qprintf("edges:\n");
		edge = VERTEX_EDGE_PTR(vertex);
		for (i=0; i<vertex->v_num_edge; i++, edge++) {
			qprintf("\t%s --> 0x%x", edge->e_label, edge->e_vertex);

			if (current_graph) {
				qprintf(" (0x%lx)\n", 
					HANDLE_TO_VERTEX(current_graph, edge->e_vertex));
				  
			} else {
				qprintf("\n");
			}
		}
	}

	if (vertex->v_num_info_LBL > 0) {
		graph_info_t *info_LBL;

		qprintf("labelled info:\n");
		info_LBL = VERTEX_INFO_LBL_PTR(vertex);
		for (i=0; i<vertex->v_num_info_LBL; i++, info_LBL++)
			qprintf("\t%s: 0x%x 0x%lx\n", info_LBL->i_label, 
				info_LBL->i_info_desc, info_LBL->i_info);
	}

	if (current_graph && current_graph->graph_num_index) {
		arbitrary_info_t *info_IDX;

		qprintf("indexed info:\n");
		info_IDX = VERTEX_INFO_IDX_PTR(vertex);
		for (i=0; i<current_graph->graph_num_index; i++, info_IDX++)
			qprintf("\t0x%x)\t0x%lx\n", i, *info_IDX);
	}
}

void
idbg_graph_vertex_name(vertex_hdl_t vhdl)
{
	int	rc;
	char	name[MAXDEVNAME];

	rc = hwgraph_vertex_name_get(vhdl, name, MAXDEVNAME);
	qprintf("\tvertex 0x%x", vhdl);
	if (rc == GRAPH_SUCCESS) {
		qprintf(" name %s\n", name);
	} else {
		qprintf(" could not find name\n");
	}
}

#if defined(DEBUG)

/* Randomly selected maximum number of vertices */

vertex_hdl_t	visited[1000];
int		num_visited = 0;

/* Check if a vertex has been visited already*/
int
is_vertex_visited(vertex_hdl_t v)
{	
	int	i;

	for(i = 0 ; i < num_visited; i++)
		if (v == visited[i])
			return(1);

	return(0);
}

/* Remember that we have visited this vertex */
#define vertex_visit(_v)	(visited[num_visited++] = _v)

/* Reset the visited vertex info */
#define visited_info_reset()	num_visited = 0

#define vertex_visit_limit_reached()	(num_visited == 1000)
void
do_hwg_walk(vertex_hdl_t	root,int prefix_len)
{
	graph_edge_place_t	place = EDGE_PLACE_WANT_REAL_EDGES;
	char			edge_name[LABEL_LENGTH_MAX];
	vertex_hdl_t		child = GRAPH_VERTEX_NONE;
	char			prefix[100];
	int			i;

	if (vertex_visit_limit_reached())
		return;

	/* Check if the vertex has been visited */
	if (is_vertex_visited(root))
		return;

	/* Remember that this vertex has been visited */
	vertex_visit(root);

	if (root == GRAPH_VERTEX_NONE)
		return;
	
	/* Prefix len is used to print the reference counts of
	 * vertices at the same level to be printed in an aligned
	 * manner on the screen for easy readability.
	 */
	for(i = 0 ; i < prefix_len ; i++)
		prefix[i] = ' ';
	prefix[i] = 0;

	while(hwgraph_edge_get_next(root,edge_name,
				    &child,&place) == GRAPH_SUCCESS) {
		/* Decrement the reference got due to the
		 * above edge_get_next()
		 */
		hwgraph_vertex_unref(child);
		if (!is_vertex_visited(child))
			qprintf("%s%s:0x%x[%d]\n",prefix,edge_name,child,
				hwgraph_vertex_refcnt(child));	
		do_hwg_walk(child,prefix_len + 4);
	}
}


void
idbg_graph_reference_count(vertex_hdl_t root)
{
	int	prefix_len = 0,rc;
	char	name[MAXDEVNAME];

	/* Reset the visited information on the vertices */
	visited_info_reset();

	/* Try to find the canonical name of this vertex */
	rc = hwgraph_vertex_name_get(root, name, MAXDEVNAME);

	if (rc != GRAPH_SUCCESS)
		qprintf("Cannot get the name for vertex %d\n",root);

	qprintf("\t\tSubgraph under %s:0x%x[%d]\n\n",
		name,
		root,
		hwgraph_vertex_refcnt(root));

	/* Do the actual vertex visiting */
	do_hwg_walk(root,prefix_len);

	qprintf("LEGEND: Line Format = "
		" <space*><edge_name>:<vhdl>[<ref count>]\n");

}

#endif /* DEBUG */
