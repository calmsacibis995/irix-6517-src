
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
#ifndef _HWG_H
#define _HWG_H

#ident	"$Revision: 1.7 $"

#include <sys/hwgraph.h>

struct vnode;

extern void hwgraph_print(void);

extern dev_t hwgraph_vnode_to_dev(	struct vnode *vnode,
					struct cred *cred);

extern int device_master_set(	vertex_hdl_t vhdl,
				vertex_hdl_t master);

extern void mark_nodevertex_as_node(	vertex_hdl_t vhdl,
					cnodeid_t cnodeid);

extern void mark_cpuvertex_as_cpu(	vertex_hdl_t vhdl,
					cpuid_t cpuid);

extern void hwgfs_earlyinit(void);

extern void hwgfs_vertex_destroy(vertex_hdl_t vhdl);

extern void hwgfs_vertex_clone(vertex_hdl_t vhdl);

extern void hwgfs_chmod(vertex_hdl_t vhdl, mode_t mode);

extern void mlreset_mark(	vertex_hdl_t vhdl,
				arbitrary_info_t info);

extern graph_error_t mlreset_check(	vertex_hdl_t vhdl,
					arbitrary_info_t *infop);

extern void hwgraph_link_add(	char *dest_path,
				char *src_path,
				char *edge_name);



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

extern graph_error_t hwgraph_info_get_exported_LBL(	vertex_hdl_t vhdl,
							char *info_name,
							int *export_info,
							arbitrary_info_t *infop);

extern graph_error_t hwgraph_info_get_next_exported_LBL(vertex_hdl_t vhdl,
							char *info_name,
							int *export_info,
							arbitrary_info_t *infop,
							graph_info_place_t *placep);





/*
 * Support for upgraph for IP2X systems
 */
extern void uphwg_init(vertex_hdl_t);
extern void add_cpu(vertex_hdl_t);

/*
 * Support for conversion from arcs to hwgraph
 */
extern void devnamefromarcs(char *);

#endif /* _HWG_H */
