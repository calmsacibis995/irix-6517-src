/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1986-1995 Silicon Graphics, Inc.           *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ifndef __SYS_HWGRAPH_INTF_H__
#define __SYS_HWGRAPH_INTF_H__

/************************   Hwgraph VPath Interfaces ****************************/

/* For testing, we assume vhdls go from 0 to MAX_PATHS * PATH_LEN -1.
 * All vpaths are currently disjoint. VPath 1 - vhdls 0 -4, VPath 2 - vhdls 5-9
 * usw... Given the src and sink (which are restricted to above path defns),
 * we just use modulo arithmetic to figure out the vpath.
 */

#define HWGRAPH_VPATH_LEN_MAX    50       

#ifdef USER_LEVEL_TEST

#define MAX_VPATHS	10
#define MAX_LBL_SIZE	15
#define MAX_VERTICES	100

typedef struct vertex_s {
	vertex_hdl_t vhdl;
        char lbl[MAX_LBL_SIZE];
        arbitrary_info_t infop;
} vertex_t;

#endif /* USER_LEVEL_TEST */

typedef struct hwgraph_vpath_s {
        vertex_hdl_t vhdl_array[MAX_VPATH_LEN_MAX];
	int pathlen;
} *hwgraph_vpath_t;

typedef struct hwgraph_vpath_cursor_s {
	hwgraph_vpath_t path;
	int cursor;
} *hwgraph_vpath_cursor_t;


/*
 * Create a new vpath that holds vertices from source to destination.
 */
hwgraph_vpath_t
hwgraph_vpath_create(vertex_hdl_t src, vertex_hdl_t sink);


/*
 * Destroy a hardware graph vpath.
 */
void
hwgraph_vpath_destroy(hwgraph_vpath_t path);


/*
 * Given a cursor into a hardware graph vpath, return the selected vertex.
 * May return GRAPH_VERTEX_NONE if cursor points beyond either end of the
 * vpath.
 */
vertex_hdl_t
hwgraph_vpath_vertex_get(hwgraph_vpath_cursor_t cursor);


/*
 * Given a cursor, move forward to the next element in the hardware graph vpath.
 */
void
hwgraph_vpath_cursor_next(hwgraph_vpath_cursor_t cursor);


/*
 * Given a cursor, move backwards to the previous element in the hardware graph vpath.
 */
void
hwgraph_vpath_cursor_prev(hwgraph_vpath_cursor_t cursor);


/*
 * Initialize a cursor to point to the start of the indicated hardware graph vpath.
 */
void
hwgraph_vpath_cursor_init(hwgraph_vpath_cursor_t cursor, hwgraph_vpath_t path);


/*
 * Clone a hardware graph vpath cursor.
 */
void
hwgraph_vpath_cursor_clone(hwgraph_vpath_cursor_t source_cursor, 
                          hwgraph_vpath_cursor_t dest_cursor);

/*
 * Create a hardware graph vpath cursor
 */
hwgraph_vpath_cursor_t
hwgraph_vpath_cursor_create(void);

/*
 * Destroy a hardware graph vpath cursor.
 */
void
hwgraph_vpath_cursor_destroy(hwgraph_vpath_cursor_t cursor);


/* Utility functions */
void hwgraph_vpath_find_common_parent(char *, char *, char *);

graph_error_t hwgraph_vpath_setup(char *, char *, char *, hwgraph_vpath_t );

graph_error_t hwgraph_vpath_get_vertices(char *, char *, vertex_hdl_t *, int *);

#ifdef DEBUG
void hwgraph_vpath_display( hwgraph_vpath_t);
#endif /* DEBUG */

#ifdef USER_LEVEL_TEST
graph_error_t hwgraph_path_get_component( char *, char *, int *, int *);
#endif /* USER_LEVEL_TEST */

#endif /*__SYS_HWGRAPH_INTF_H__ */
