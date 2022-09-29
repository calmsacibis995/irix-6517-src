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
#ifndef _GRAPH_H
#define _GRAPH_H

#ident	"$Revision: 1.9 $"

#if _KERNEL || _USER_MODE_TEST

/* 
** Maximum number of characters in a label, including a terminating 
** null character.
*/
#define LABEL_LENGTH_MAX 256 

/*
** Public interfaces to graph module.
*/
typedef long		arbitrary_info_t;
typedef struct graph_s	*graph_hdl_t;
typedef long		graph_edge_place_t;
typedef long		graph_info_place_t;
typedef long		graph_vertex_place_t;

/*
** Possible return values from graph routines.
*/
typedef enum graph_error_e {	GRAPH_SUCCESS,		/* 0 */
				GRAPH_DUP,		/* 1 */
				GRAPH_NOT_FOUND,	/* 2 */
				GRAPH_BAD_PARAM,	/* 3 */
				GRAPH_HIT_LIMIT,	/* 4 */
				GRAPH_CANNOT_ALLOC,	/* 5 */
				GRAPH_ILLEGAL_REQUEST,	/* 6 */
				GRAPH_IN_USE		/* 7 */
				} graph_error_t;

typedef struct graph_attr {
	/* INPUT TO graph_create */
	char	*ga_name;		/* name of graph 				*/

	char	ga_separator;		/* separator char - (e.g. '/') 			*/

	int	ga_num_index;		/* number of indexed infos per vertex 		*/

	int	ga_reserved_places;	/* reserved edge/info "place" handles 		*/
					/* starting with handle=1.  Graph code will 	*/
					/* avoid using 1..ga_reserved_places as 	*/
					/* edge/info handles				*/

	/* ADDITIONAL FIELDS RETURNED FROM graph_summary_get */
	long	ga_generation;		/* generation number.  Incremented every 	*/
					/* time any change is made to the graph. 	*/

	int	ga_num_vertex;		/* count of the current number of vertices 	*/
					/* in this graph.				*/

	int	ga_num_vertex_max;	/* maximum number of vertices supported		*/
} graph_attr_t;

#define GRAPH_VERTEX_NONE ((vertex_hdl_t)-1)
#define GRAPH_EDGE_PLACE_NONE ((graph_edge_place_t)0)
#define GRAPH_INFO_PLACE_NONE ((graph_info_place_t)0)
#define GRAPH_VERTEX_PLACE_NONE ((graph_vertex_place_t)0)

/* Operations on GRAPHS: create, destroy, summary_get, visit */
extern graph_error_t graph_create(graph_attr_t *, graph_hdl_t *, int);
extern graph_error_t graph_destroy(graph_hdl_t);
extern graph_error_t graph_summary_get(graph_hdl_t, graph_attr_t *);
extern graph_error_t graph_vertex_visit(graph_hdl_t, int (*)(void *, vertex_hdl_t), void *, int *, vertex_hdl_t *);

/* Operations on VERTICES: create, destroy, combine, clone, get_next, ref, unref */
extern graph_error_t graph_vertex_create(graph_hdl_t, vertex_hdl_t *);
extern graph_error_t graph_vertex_destroy(graph_hdl_t, vertex_hdl_t);
#if NOTDEF
extern graph_error_t graph_vertex_combine(graph_hdl_t, vertex_hdl_t, vertex_hdl_t);
#endif /* NOTDEF */
extern graph_error_t graph_vertex_clone(graph_hdl_t, vertex_hdl_t, vertex_hdl_t *);
extern graph_error_t graph_vertex_get_next(graph_hdl_t, vertex_hdl_t *, graph_vertex_place_t *);
extern int graph_vertex_refcnt(graph_hdl_t, vertex_hdl_t);
extern graph_error_t graph_vertex_ref(graph_hdl_t, vertex_hdl_t);
extern graph_error_t graph_vertex_unref(graph_hdl_t, vertex_hdl_t);

/* Operations on EDGES: add, remove, get_next */
extern graph_error_t graph_edge_add(graph_hdl_t, vertex_hdl_t, vertex_hdl_t, char *);
extern graph_error_t graph_edge_remove(graph_hdl_t, vertex_hdl_t, char *, vertex_hdl_t *);
extern graph_error_t graph_edge_get(graph_hdl_t, vertex_hdl_t, char *, vertex_hdl_t *);
extern graph_error_t graph_edge_get_next(graph_hdl_t, vertex_hdl_t, char *, vertex_hdl_t *, graph_edge_place_t *);

/* Operations on LABELLED INFORMATION: add, remove, replace, get, get_next */
typedef uintptr_t arb_info_desc_t; /* Abstract descriptor for arbitrary_info_t */
extern graph_error_t graph_info_add_LBL(graph_hdl_t, vertex_hdl_t, char *, arb_info_desc_t, arbitrary_info_t);

extern graph_error_t graph_info_remove_LBL(graph_hdl_t, vertex_hdl_t, char *, arb_info_desc_t *, arbitrary_info_t *);

extern graph_error_t graph_info_replace_LBL(graph_hdl_t, vertex_hdl_t, char *, 
			arb_info_desc_t, arbitrary_info_t, arb_info_desc_t *, arbitrary_info_t *);

extern graph_error_t graph_info_get_LBL(graph_hdl_t, vertex_hdl_t, char *, arb_info_desc_t *, arbitrary_info_t *);

extern graph_error_t graph_info_get_next_LBL(graph_hdl_t, vertex_hdl_t, char *, 
			arb_info_desc_t *, arbitrary_info_t *, graph_info_place_t *);

/* Operations on INDEXED INFORMATION: replace, get */
extern graph_error_t graph_info_replace_IDX(graph_hdl_t, vertex_hdl_t, unsigned int, arbitrary_info_t, arbitrary_info_t *);
extern graph_error_t graph_info_get_IDX(graph_hdl_t, vertex_hdl_t, unsigned int, arbitrary_info_t *);

/* Operations on paths: get_component */
extern graph_error_t graph_path_get_component(graph_hdl_t, char *, char *, int *, int *);
#endif /* _KERNEL || _USER_MODE_TEST */

#endif /* _GRAPH_H */
