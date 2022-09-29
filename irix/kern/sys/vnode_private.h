/**************************************************************************
 *									  *
 * 		 Copyright (C) 1986-1995 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ifndef _SYS_PRIVATE_VNODE_H	/* wrapper symbol for kernel use */
#define _SYS_PRIVATE_VNODE_H	/* subject to change without notice */

#ident	"$Revision: 1.6 $"

#include <sys/vnode.h>

/*
 * There are multiple vnode freelists to allow multiple,
 * simultaneous accesses on multi-processor systems.
 * Each list contains its own spinlock; a free-list size counter;
 * a list identifier; and a (circular) pointer to the next list.
 */
typedef struct vfreelist_s {
	vnlist_t	vf_freelist;	/* free list pointers */
	lock_t		vf_lock;
	int		vf_lsize;	/* # of vnodes on list */
	int		vf_listid;
	struct vfreelist_s *vf_next;
} vfreelist_t;
#if defined(MP)
#pragma set type attribute vfreelist_s align=128
#endif

#define VKEY(vp)	((vp)->v_number)

/*
 * Private vnode spinlock manipulation.
 */
#define	NESTED_VN_LOCK(vp)	nested_bitlock(&(vp)->v_flag, VLOCK)
#define	NESTED_VN_UNLOCK(vp)	nested_bitunlock(&(vp)->v_flag, VLOCK)

#endif	/* _SYS_PRIVATE_VNODE_H */
