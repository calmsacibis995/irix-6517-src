/**************************************************************************
 *									  *
 *		 Copyright (C) 1995-1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ifndef	_CFS_CFS_H_
#define	_CFS_CFS_H_
#ident	"$Id: cfs.h,v 1.12 1997/08/26 17:59:18 dnoveck Exp $"

/*
 * General Cell File System header file that may be included 
 * by external code.
 */
#include <ksys/cell/handle.h>
#include <sys/vnode.h>

/*
 * CFS handle.
 */
typedef __uint32_t	cfs_gen_t;
typedef struct cfs_handle {
	obj_handle_t	ch_objhand;		/* generic object handle */
	cfs_gen_t	ch_gen;			/* generation number */
} cfs_handle_t;

#define CFS_HANDLE_MAKE_NULL(handle)	HANDLE_MAKE_NULL((handle).ch_objhand)
#define CFS_HANDLE_IS_NULL(handle)	HANDLE_IS_NULL((handle).ch_objhand)
#define CFS_HANDLE_EQU(h1, h2)						\
        (HANDLE_EQU((h1).ch_objhand, (h2).ch_objhand) &&		\
	 (h1).ch_gen == (h2).ch_gen)

/*
 * Externs.
 */
struct vfs;
extern void		cfs_vnexport(vnode_t *, cfs_handle_t *);
extern void		cfs_vnimport(cfs_handle_t *, vnode_t **);
extern cell_t		cfs_vnode_to_cell(vnode_t *);

extern void             cfs_dummy_vfs(struct vfs *);

#endif	/* _CFS_CFS_H_ */
