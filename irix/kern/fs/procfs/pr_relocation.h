/**************************************************************************
 *									  *
 *		 Copyright (C) 1995-1997 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ifndef	_PR_RELOCATE_H_
#define	_PR_RELOCATE_H_
#ident	"$Id: pr_relocation.h,v 1.3 1997/12/19 21:27:16 beck Exp $"

#include "fs/cfs/cfs_relocation.h"
#include "os/proc/pproc_private.h"
#include "fs/procfs/prsystm.h"
#include "fs/procfs/prdata.h"
#include "fs/procfs/procfs_private.h"

/*
 * Object bag tags.
 */
#define OBJ_TAG_CFS_PRNODE_INFO	OBJ_SVC_TAG(SVC_CFS, 10)
#define OBJ_TAG_CFS_PRNODE	OBJ_SVC_TAG(SVC_CFS, 11)

/* Data associated with OBJ_TAG_CFS_PRNODE_INFO: */
typedef struct {
	int	ntrace;
	int	npstrace;
} prnode_info_t;

/* Data associated with OBJ_TAG_CFS_PRNODE: */
typedef struct {
	pid_t		pr_tpid;	/* pid of traced process */
	prnodetype_t	pr_type;
	short		pr_mode;	/* file mode bits */
	short		pr_opens;	/* count of opens */
	short		pr_writers;	/* count of opens for writing */
	short		pr_pflags;	/* private flags */
} prnode_bag_t;

extern int	prnode_proc_emigrate_start(proc_t *, obj_manifest_t *);
extern int	prnode_proc_immigrate_start(proc_t *, obj_manifest_t *);
extern void	prnode_proc_emigrate(proc_t *, obj_bag_t); 
extern void	prnode_proc_immigrate(proc_t *, obj_bag_t);
extern void	prnode_proc_emigrate_end(proc_t *);
extern void	prtrmasks_bag(struct pr_tracemasks *, obj_bag_t);
extern void	prtrmasks_unbag(struct pr_tracemasks **, obj_bag_t);
extern void	prtrmasks_source_end(struct pr_tracemasks **);

extern cfs_vn_kore_if_t prnode_obj_bhv_iface;

#ifdef DEBUG
#define PRIVATE
#define ASSERT_NONZERO(x)	ASSERT(!x)
#else
#define PRIVATE static
#define ASSERT_NONZERO(x)	x = x
#endif

#endif	/* _PR_RELOCATE_H_ */
