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

#ident	"$Revision: 1.35 $"

#if _KERNEL
#include <sys/conf.h>
#include <sys/graph.h>
#include <sys/sysmacros.h>

/* Defines for dealing with fast "indexed" information associated with a vertex */

/* Reserve room in every vertex for 3 pieces of fast access indexed information */
#define HWGRAPH_NUM_INDEX_INFO	3

#define HWGRAPH_DEVSW		0	/* {b,c}devsw for this device */
#define HWGRAPH_CONNECTPT	1	/* connect point (parent) */
#define HWGRAPH_FASTINFO	2	/* reserved for creator of vertex */

typedef int (*traverse_fn_t)(	vertex_hdl_t from,
				char **remainder_buf,
				vertex_hdl_t *to);

/*
 * Interfaces to convert between graph vertex handles and dev_t's.
 * Major number 0 is reserved for use by hwgraph; this allows the
 * conversion between vhdl's and dev's to be a NULL conversion.
 * That simplifies things for driver writers -- they can pass dev_t's
 * directly to hwgraph interfaces.
 *
 * 	ASSERT(HWGRAPH_MAJOR == 0);
 * 	ASSERT(GRAPH_VERTEX_NONE == NODEV);
 */
#define vhdl_to_dev(vhdl)	((dev_t)(vhdl))
#define dev_to_vhdl(dev)	((vertex_hdl_t)(dev))
#define dev_is_vertex(dev)	(emajor((dev_t)(dev)) == 0)
#define HWGRAPH_STRING_DEV	makedev(0, 0)
#define IS_HWGRAPH_STRING_DEV(x) ((dev_t)(x)==HWGRAPH_STRING_DEV)

/*
 * Reserved edge_place_t values, used as the "place" parameter to edge_get_next.
 * Every vertex in the hwgraph has up to 2 *implicit* edges.  There is an implicit
 * edge called "." that points to the current vertex.  There is an implicit edge
 * called ".." that points to the vertex' connect point.
 */
#define EDGE_PLACE_WANT_CURRENT GRAPH_EDGE_PLACE_NONE
#define EDGE_PLACE_WANT_CONNECTPT 1
#define EDGE_PLACE_WANT_REAL_EDGES 2
#define HWGRAPH_RESERVED_PLACES 2


/* Root of hwgraph */
extern vertex_hdl_t hwgraph_root;



/* Operations on the hwgraph: init, summary_get, visit */
extern graph_error_t hwgraph_summary_get(graph_attr_t *graph_attr);

extern graph_error_t hwgraph_vertex_visit(	int (*)(void *, vertex_hdl_t),
						void *arg,
						int *retval,
						vertex_hdl_t *end_handle);



/* Operations on VERTICES: create, destroy, clone, get_next, ref, unref */

extern graph_error_t hwgraph_vertex_create(vertex_hdl_t *vhdlp);

extern graph_error_t hwgraph_vertex_destroy(vertex_hdl_t vhdl);

extern graph_error_t hwgraph_vertex_clone(	vertex_hdl_t src,
						vertex_hdl_t *clone_vhdl_p);

extern graph_error_t hwgraph_vertex_get_next(	vertex_hdl_t *vhdlp,
						graph_vertex_place_t *place_p);

extern int hwgraph_vertex_refcnt(vertex_hdl_t vhdl);

extern graph_error_t hwgraph_vertex_ref(vertex_hdl_t vhdl);

extern graph_error_t hwgraph_vertex_unref(vertex_hdl_t vhdl);




/* Operations on EDGES: add, remove, get_next */

extern graph_error_t hwgraph_edge_add(	vertex_hdl_t from,
					vertex_hdl_t to,
					char *edge_name);

extern graph_error_t hwgraph_edge_remove(	vertex_hdl_t from,
						char *edge_name,
						vertex_hdl_t *old_to_p);

extern graph_error_t hwgraph_edge_get(	vertex_hdl_t from,
					char *edge_name,
					vertex_hdl_t *to_p);

extern graph_error_t hwgraph_edge_get_next(	vertex_hdl_t srcv,
						char *edge_name_buf,
						vertex_hdl_t *targetv,
						graph_edge_place_t *place_p);




/* Special pre-defined edge labels */
#define HWGRAPH_EDGELBL_HW 	"hw"
#define HWGRAPH_EDGELBL_DOT 	"."
#define HWGRAPH_EDGELBL_DOTDOT 	".."

/* 
** Operations on LABELLED INFORMATION: add, remove, replace, 
** get, get_next, export, unexport
*/

extern graph_error_t hwgraph_info_add_LBL(	vertex_hdl_t vhdl,
						char *info_name,
						arbitrary_info_t info);

extern graph_error_t hwgraph_info_remove_LBL(	vertex_hdl_t vhdl,
						char *info_name,
						arbitrary_info_t *old_info_p);

extern graph_error_t hwgraph_info_replace_LBL(	vertex_hdl_t vhdl,
						char *info_name,
						arbitrary_info_t new_info,
						arbitrary_info_t *old_info_p);

extern graph_error_t hwgraph_info_get_LBL(	vertex_hdl_t vhdl,
						char *info_name,
						arbitrary_info_t *info_p);

extern graph_error_t hwgraph_info_get_next_LBL(	vertex_hdl_t srcv,
						char *info_name_buf,
						arbitrary_info_t *info_p,
						graph_info_place_t *place_p);

extern graph_error_t hwgraph_info_export_LBL(	vertex_hdl_t vhdl,
						char *info_name,
						int export_info);

extern graph_error_t hwgraph_info_unexport_LBL(	vertex_hdl_t vhdl,
						char *info_name);





/*
** Operations on paths (strings): get_component, path_add,
** traverse, path_to_dev, device_add, add_link, device_get
*/

extern graph_error_t hwgraph_path_get_component(char *path,
						char *component,
						int *separator_length,
						int *component_length);

extern graph_error_t hwgraph_path_lookup(	vertex_hdl_t startv,
						char *path,
						vertex_hdl_t *endv_p,
						char **remainder_p);

extern graph_error_t hwgraph_path_add(	vertex_hdl_t from,
					char *path,
					vertex_hdl_t *newv_p);

extern graph_error_t hwgraph_traverse(	vertex_hdl_t from,
					char *path,
					vertex_hdl_t *to_p);

extern vertex_hdl_t hwgraph_path_to_vertex(char *path);

extern dev_t hwgraph_path_to_dev(char *path);

extern graph_error_t hwgraph_block_device_add(	vertex_hdl_t from,
						char *path,
						char *driver_prefix,
						vertex_hdl_t *bdev);

extern graph_error_t hwgraph_char_device_add(	vertex_hdl_t from,
						char *path,
						char *driver_prefix,
						vertex_hdl_t *cdev);

extern void hwgraph_device_add(	vertex_hdl_t from,
				char *path,
				char *driver_prefix,
				vertex_hdl_t *dev,
				vertex_hdl_t *block_dev,
				vertex_hdl_t *char_dev);

extern vertex_hdl_t hwgraph_block_device_get(vertex_hdl_t vhdl);

extern vertex_hdl_t hwgraph_char_device_get(vertex_hdl_t vhdl);




/*
** set/get routines for indexed information and predefined labels:
** cdevsw, bdevsw, traverse routine
*/
extern int hwgraph_cdevsw_set(	vertex_hdl_t vhdl,
				struct cdevsw *cdevsw);

extern struct cdevsw * hwgraph_cdevsw_get(vertex_hdl_t vhdl);

extern int hwgraph_bdevsw_set(	vertex_hdl_t vhdl,
				struct bdevsw *bdevsw);

extern struct bdevsw * hwgraph_bdevsw_get(vertex_hdl_t vhdl);

extern graph_error_t hwgraph_traverse_set(	vertex_hdl_t vhdl,
						traverse_fn_t func);

extern traverse_fn_t hwgraph_traverse_get(vertex_hdl_t vhdl);

extern void hwgraph_fastinfo_set(	vertex_hdl_t vhdl,
					arbitrary_info_t info);

extern arbitrary_info_t hwgraph_fastinfo_get(vertex_hdl_t vhdl);

extern void device_info_set(	dev_t device,
				void *info);

extern void *device_info_get(dev_t device);




/* Support for INVENTORY */
struct inventory_s;
struct invplace_s;
extern struct invplace_s invplace_none;

extern graph_error_t hwgraph_inventory_add(	vertex_hdl_t vhdl,
						int class,
						int type,
						major_t ctlr,
						minor_t unit,
						int state);

extern graph_error_t hwgraph_inventory_remove(	vertex_hdl_t vhdl,
						int class,
						int type,
						major_t ctlr,
						minor_t unit,
						int state);

extern graph_error_t hwgraph_inventory_get_next(vertex_hdl_t vhdl,
						struct invplace_s *place_p,
						struct inventory_s **inventory_p);


extern int hwgraph_controller_num_get(dev_t);
extern void hwgraph_controller_num_set(dev_t, int);


/* Support for file systems */

extern graph_error_t hwgraph_connectpt_set(	vertex_hdl_t vhdl,
						vertex_hdl_t connectpt);

extern vertex_hdl_t hwgraph_connectpt_get(vertex_hdl_t vhdl);

extern void hwgraph_chmod(vertex_hdl_t vhdl, mode_t mode);

/* default permissions for various hwg node types */
#define HWGRAPH_PERM_EXTERNAL_INT	0444
#define HWGRAPH_PERM_TTY		0666
#define HWGRAPH_PERM_TABLET		0644
#define HWGRAPH_PERM_DIALS		0666
#define HWGRAPH_PERM_KBD		0666
#define HWGRAPH_PERM_MOUSE		0666
#define HWGRAPH_PERM_CONSOLE		0644
#define HWGRAPH_PERM_TPSC		0666
#define HWGRAPH_PERM_SMFD		0666
#define HWGRAPH_PERM_MMSC_CONTROL	0600

/* Support for general topology manipulation */

extern graph_error_t hwgraph_vertex_name_get(	vertex_hdl_t vhdl,
						char *name_buf,
						uint name_length);

extern char *vertex_to_name(	vertex_hdl_t vhdl,
				char *name_buf,
				uint name_length);

extern vertex_hdl_t device_master_get(vertex_hdl_t vhdl);

extern cnodeid_t master_node_get(vertex_hdl_t vhdl);

extern cpuid_t cpuvertex_to_cpuid(vertex_hdl_t cpu_vhdl);

extern cnodeid_t nodevertex_to_cnodeid(vertex_hdl_t node_vhdl);



/*
 * Support for "vpath".    A "vpath", or "vertex path" is an
 * ordered list of vertices such that adjacent vertices in the
 * vpath are connected by edges.
 */

/* Maximum hwgraph vpath length (number of vertices) */
#define HWGRAPH_VPATH_LEN_MAX          50

typedef struct hwgraph_vpath_s {
        vertex_hdl_t vhdl_array[HWGRAPH_VPATH_LEN_MAX];
        int pathlen;
} *hwgraph_vpath_t;

typedef struct hwgraph_vpath_cursor_s {
        hwgraph_vpath_t path;
        int cursor;
} *hwgraph_vpath_cursor_t;

extern hwgraph_vpath_t hwgraph_vpath_create(	vertex_hdl_t src,
						vertex_hdl_t sink);

extern void hwgraph_vpath_destroy(hwgraph_vpath_t vpath);

extern vertex_hdl_t hwgraph_vpath_vertex_get(hwgraph_vpath_cursor_t cursor);

extern void hwgraph_vpath_cursor_next(hwgraph_vpath_cursor_t cursor);

extern void hwgraph_vpath_cursor_prev(hwgraph_vpath_cursor_t cursor);

extern void hwgraph_vpath_cursor_init(	hwgraph_vpath_cursor_t cursor,
					hwgraph_vpath_t vpath);

extern void hwgraph_vpath_cursor_clone(	hwgraph_vpath_cursor_t source_cursor,
                          		hwgraph_vpath_cursor_t dest_cursor);

extern hwgraph_vpath_cursor_t hwgraph_vpath_cursor_create(void);

extern void hwgraph_vpath_cursor_destroy(hwgraph_vpath_cursor_t cursor);

extern vertex_hdl_t mem_vhdl_get(vertex_hdl_t drv_vhdl);

#if (MAXCPUS == 512)
extern int xxl_hwgraph_num_dev;
#else
extern int hwgraph_num_dev;
#endif

#endif /* _KERNEL */

#endif /* _HWGRAPH_H */
