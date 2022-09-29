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

#ident	"$Revision: 1.18 $"

#ifndef _SYS_FDT_H
#define _SYS_FDT_H

struct proc;
struct shaddr_s;
struct vnode;
struct pollfd;
struct pollvobj;
struct vfile;
struct fdt;

/* File descriptor table routines */
extern void 		fdt_init(void);
extern struct fdt *	fdt0_init(void);
extern struct fdt *	fdt_create(int);
extern struct fdt *	fdt_fork(void);
extern void 		fdt_forkfree(struct fdt *);
extern void 		fdt_free(struct fdt *);
extern void 		fdt_exec(void);
extern void 		fdt_exit(void);
extern void 		fdt_closeall(void);
extern int		fdt_nofiles(void);
extern int  		fdt_gethi(void);
extern void 		fdt_share_alloc(struct shaddr_s *);
extern void 		fdt_share_dealloc(struct proc *, int);
extern void 		fdt_share_detach(struct proc *, int, int);
extern int 		fdt_alloc(struct vfile *, int *);
extern int 		fdt_alloc_many(int, struct vfile **, int *);
extern struct vfile *	fdt_alloc_undo(int);
extern int 		getf(int, struct vfile **);
extern int 		closefd(int, struct vfile **);
extern void 		fdt_release(void);
extern int 		fdt_dup(int, struct vfile *, int *);
extern char 		fdt_getflags(int);
extern void 		fdt_setflags(int, char);
extern int 		fdt_vnode_isopen(struct vnode *);
extern int 		fdt_fuser(struct proc *, struct vnode *, int);
extern struct vfile *	fdt_getref(struct proc *, int);
extern struct vfile *	fdt_getref_next(struct proc *, int, int *);
extern struct vfile *	fdt_swapref(struct proc *, int, struct vfile *);
extern int		fdt_select_convert(struct pollfd *, int,  
					   struct pollvobj *);
extern int		fdt_export (struct proc *, int, void *);
extern int		fdt_lookup_fdinproc(struct proc *, int, struct vnode **);
#endif /* _SYS_FDT_H */
