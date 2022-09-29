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
#ifndef _HWGRAPH_H
#define _HWGRAPH_H

#ident	"$Revision: 1.3 $"

#if _KERNEL
#include <sys/conf.h>
#include <sys/graph.h>
#include <sys/sysmacros.h>

/* Defines for dealing with fast indexed information associated with a vertex */

/* Reserve room in every vertex for 4 pieces of fast access indexed information */
#define HWGRAPH_NUM_INDEX_INFO	4	

#define HWGRAPH_BDEVSW		0	/* bdevsw for this device */
#define HWGRAPH_CDEVSW		1	/* cdevsw for this device */
#define HWGRAPH_CONNECTPT	2	/* connect point (parent) */
#define HWGRAPH_FASTINFO	3	/* reserved for creator of vertex */

typedef int (*traverse_fn_t)(vertex_hdl_t, char **, vertex_hdl_t *);

/* 
 * Interfaces to convert between graph vertex handles and dev_t's.
 * Major number 0 is reserved for use by hwgraph; this allows the
 * conversion between vhdl's and dev's to be a NULL conversion.
 * That simplifies things for driver writers -- they can pass dev_t's
 * directly to hwgraph interfaces.
 */
#if TBD
 #include <sys/major.h>
 #assert(HWGRAPH_MAJOR == 0)
#endif

#define vhdl_to_dev(vhdl)	((dev_t)(vhdl))
#define dev_to_vhdl(dev)	((vertex_hdl_t)(dev))
#define dev_is_vertex(dev)	(emajor((dev_t)(dev)) == 0)
#define HWGRAPH_STRING_DEV	makedev(0, 0)
#define IS_HWGRAPH_STRING_DEV(x) ((dev_t)(x)==HWGRAPH_STRING_DEV)

/* 
 * Reserved edge_place_t values, used as the "place" parameter to edge_get_next.
 * Every vertex in the hwgraph has up to 4 *implicit* edges.  There is an implicit
 * edge called "." that points to the current vertex.  There is an implicit edge
 * called ".." that points to the vertex' connect point.  There are implicit edges
 * called "block" and "char" that point to special files that represent block and 
 * character access to the special file.  The "block" and "char" implicit edges 
 * exist if and only if the vertex has defined bdevsw and cdevsw, respectively.
 */
#define EDGE_PLACE_WANT_CURRENT GRAPH_EDGE_PLACE_NONE
#define EDGE_PLACE_WANT_CONNECTPT 1
#define EDGE_PLACE_WANT_BDEVSW 2
#define EDGE_PLACE_WANT_CDEVSW 3
#define EDGE_PLACE_WANT_REAL_EDGES 4
#define HWGRAPH_RESERVED_PLACES 4

/* Root of hwgraph */
extern vertex_hdl_t hwgraph_root;

/* Operations on the hwgraph: init, summary_get, visit */
extern void hwgraph_init(void);
extern graph_error_t hwgraph_summary_get(graph_attr_t *);
extern graph_error_t hwgraph_vertex_visit(int (*)(void *, vertex_hdl_t), void *, int *, vertex_hdl_t *);

/* Operations on VERTICES: create, destroy, combine, clone, get_next, ref, unref */
extern graph_error_t hwgraph_vertex_create(vertex_hdl_t *);
extern graph_error_t hwgraph_vertex_destroy(vertex_hdl_t);
#if NOTDEF
extern graph_error_t hwgraph_vertex_combine(vertex_hdl_t, vertex_hdl_t);
#endif /* NOTDEF */
extern graph_error_t hwgraph_vertex_clone(vertex_hdl_t, vertex_hdl_t *);
extern graph_error_t hwgraph_vertex_get_next(vertex_hdl_t *, graph_vertex_place_t *);
extern graph_error_t hwgraph_vertex_ref(vertex_hdl_t);
extern graph_error_t hwgraph_vertex_unref(vertex_hdl_t);

/* Operations on EDGES: add, remove, get_next */
extern graph_error_t hwgraph_edge_add(vertex_hdl_t, vertex_hdl_t, char *);
extern graph_error_t hwgraph_edge_remove(vertex_hdl_t, char *, vertex_hdl_t *);
extern graph_error_t hwgraph_edge_get(vertex_hdl_t, char *, vertex_hdl_t *);
extern graph_error_t hwgraph_edge_get_next(vertex_hdl_t, char *, vertex_hdl_t *, graph_edge_place_t *);

/* Special pre-defined edge labels */
#define HWGRAPH_EDGELBL_HW 	"hw"
#define HWGRAPH_EDGELBL_DOT 	"."
#define HWGRAPH_EDGELBL_DOTDOT 	".."
#define HWGRAPH_EDGELBL_BLK	_HWGRAPH_NAME_BLOCK
#define HWGRAPH_EDGELBL_CHAR	_HWGRAPH_NAME_CHAR

/* Operations on LABELLED INFORMATION: add, remove, replace, get, get_next */
extern graph_error_t hwgraph_info_add_LBL(vertex_hdl_t, char *, arbitrary_info_t);
extern graph_error_t hwgraph_info_remove_LBL(vertex_hdl_t, char *, arbitrary_info_t *);
extern graph_error_t hwgraph_info_replace_LBL(vertex_hdl_t, char *, arbitrary_info_t, arbitrary_info_t *);
extern graph_error_t hwgraph_info_get_LBL(vertex_hdl_t, char *, arbitrary_info_t *);
extern graph_error_t hwgraph_info_get_next(vertex_hdl_t, char *, arbitrary_info_t *, graph_info_place_t *);
extern graph_error_t hwgraph_info_export_LBL(vertex_hdl_t, char *, short);
extern graph_error_t hwgraph_info_unexport_LBL(vertex_hdl_t, char *);

/*
** Operations on locator STRINGS: get_component, path_add,
** traverse, string_to_dev, add_device
*/
extern graph_error_t hwgraph_string_get_component(char *, char *, int *, int *);
extern graph_error_t hwgraph_path_add(vertex_hdl_t, vertex_hdl_t, char *, vertex_hdl_t *);
extern graph_error_t hwgraph_traverse(vertex_hdl_t, char *, vertex_hdl_t *);
extern vertex_hdl_t hwgraph_string_to_vertex(char *);
extern dev_t hwgraph_string_to_dev(char *);
extern graph_error_t hwgraph_device_add(vertex_hdl_t, char *, char *, vertex_hdl_t *);

/*
** set/get routines for indexed information and predefined labels:
** cdevsw, bdevsw, traverse routine
*/
extern int hwgraph_cdevsw_set(vertex_hdl_t, struct cdevsw *);
extern struct cdevsw * hwgraph_cdevsw_get(vertex_hdl_t);
extern int hwgraph_bdevsw_set(vertex_hdl_t, struct bdevsw *);
extern struct bdevsw * hwgraph_bdevsw_get(vertex_hdl_t);
extern graph_error_t hwgraph_traverse_set(vertex_hdl_t, traverse_fn_t);
extern traverse_fn_t hwgraph_traverse_get(vertex_hdl_t);
extern void hwgraph_fastinfo_set(vertex_hdl_t, arbitrary_info_t);
extern arbitrary_info_t hwgraph_fastinfo_get(vertex_hdl_t);
extern void device_info_set(dev_t device, void *info);
extern void *device_info_get(dev_t device);

/* Support for INVENTORY */
struct inventory_s;
struct invplace_s;
extern struct invplace_s invplace_none;
extern graph_error_t hwgraph_inventory_add(vertex_hdl_t, 
	int class, int type, char ctlr, char unit, int state);

extern graph_error_t hwgraph_inventory_get_next(struct invplace_s *, struct inventory_s **);

/* Support for file systems */
extern graph_error_t hwgraph_connectpt_set(vertex_hdl_t, vertex_hdl_t);
extern vertex_hdl_t hwgraph_connectpt_get(vertex_hdl_t);

/* Support for general topology manipulation */
extern graph_error_t hwgraph_vertex_name_get(vertex_hdl_t, char *, uint);
extern char *vertex_to_name(vertex_hdl_t, char *, uint);
extern void vertex_msg(vertex_hdl_t vhdl, int level, char *fmt, ...);
extern vertex_hdl_t device_master_get(vertex_hdl_t);
extern cnodeid_t master_node_get(vertex_hdl_t);
extern cpuid_t cpuvertex_to_cpuid(vertex_hdl_t);
extern cnodeid_t nodevertex_to_cnodeid(vertex_hdl_t);

/* Not for use by drivers */
struct vnode;
extern dev_t hwgraph_vnode_to_dev(struct vnode *, struct cred *);
extern int device_master_set(vertex_hdl_t vhdl, vertex_hdl_t master);
extern void mark_nodevertex_as_node(vertex_hdl_t vhdl, cnodeid_t cnodeid);
extern void mark_cpuvertex_as_cpu(vertex_hdl_t vhdl, cpuid_t cpuid);
extern void hwgfs_vertex_destroy(vertex_hdl_t vhdl);

/* 
 * The generic graph code defines "information descriptors" that are associated
 * with each piece of labelled information.  hwgraph layer chooses to use these
 * info descriptors to indicate whether or not the associated labelled information 
 * should be exported through hwgfs attr_get calls.  If the information should
 * NOT be exported, the descriptor is set to PRIVATE.  If the information should
 * be exported, the descriptor is set to EXPORT.  If the information is a pointer
 * to information that should be exported, the descriptor is set to the number
 * of bytes that should be exported.
 *
 * It is left to user-level code to interpret exported information.
 *
 * By default, labelled information is PRIVATE.  A caller explicitly
 * exports any information it wants to via hwgraph_info_export_LBL.
 * If information is ever hwgraph_info_replace_LBL'ed, it must be 
 * re-exported.
 */
#define INFO_DESC_PRIVATE	-1		/* default */
#define INFO_DESC_EXPORT	0		/* export info itself */
extern graph_error_t hwgraph_info_get_exported_LBL(vertex_hdl_t, char *, 
			short *, arbitrary_info_t *);

extern graph_error_t hwgraph_info_get_next_exported_LBL(vertex_hdl_t, char *, 
			short *, arbitrary_info_t *, graph_info_place_t *);


#endif /* _KERNEL */

#define _HWGRAPH_NAME_BLOCK 	"block"
#define _HWGRAPH_NAME_CHAR 	"char"

#endif /* _HWGRAPH_H */
