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
#ifndef _SYS_FDT_PRIVATE_H
#define _SYS_FDT_PRIVATE_H

#ident	"$Revision: 1.14 $"

/*
 * Per-process file descriptor table definitions.
 */
#include <sys/sema.h> 	/* for mrlock_t, sema_t */

/*
 * User file descriptors are dynamically allocated and expanded
 * as required.
 */

typedef struct ufchunk {
	struct vfile	*uf_ofile;
	char		uf_pofile;
	cnt_t		uf_inuse;
        int             uf_waiting;
} ufchunk_t;

/* We separate these from the above because we do a lot of bcopy with
 * the above data structure.
 */
typedef struct fd_syncs {
        sv_t            fd_fwait;
        mutex_t         fd_mutex;
} fdt_syncs_t;

/*
 * File descriptor table.
 */
typedef struct fdt {
	struct ufchunk	*fd_flist;	/* open file list */
	mrlock_t	fd_lock;	/* syncing file descriptors */
	int		fd_nofiles;	/* number of open file slots */
        fdt_syncs_t     *fd_syncs;      /* per fd synchronization */
} fdt_t;

/* 
 * Each ufchunk lock manipulation
 */

#define FDT_FD_INIT(fdt, fd, name) { \
     mutex_init(&(fdt)->fd_syncs[fd].fd_mutex, MUTEX_DEFAULT, name); \
     sv_init(&(fdt)->fd_syncs[fd].fd_fwait, SV_DEFAULT, name); \
}

#define FDT_FD_LOCK(fdt, fd) { \
    ASSERT(!mutex_mine(&(fdt)->fd_syncs[fd].fd_mutex)); \
    ASSERT(fd < fdt->fd_nofiles); \
    mutex_lock(&(fdt)->fd_syncs[fd].fd_mutex, PZERO); \
}
	
#define FDT_FD_UNLOCK(fdt, fd) { \
    ASSERT(mutex_mine(&(fdt)->fd_syncs[fd].fd_mutex)); \
    ASSERT(fd < fdt->fd_nofiles); \
    mutex_unlock(&(fdt)->fd_syncs[fd].fd_mutex); \
}

#define FDT_WAKE(fdt, fd) \
     sv_broadcast(&(fdt)->fd_syncs[fd].fd_fwait)

#define FDT_WAIT(fdt, fd) { \
    ASSERT(mutex_mine(&(fdt)->fd_syncs[fd].fd_mutex)); \
    sv_wait_sig(&(fdt)->fd_syncs[fd].fd_fwait, PZERO, \
		&(fdt)->fd_syncs[fd].fd_mutex, 0); \
}

/*
 * fdt lock manipulation.
 */
#define	FDT_READ_LOCK(fdt)	mraccess(&(fdt)->fd_lock)
#define	FDT_WRITE_LOCK(fdt)	mrupdate(&(fdt)->fd_lock)
#define	FDT_UNLOCK(fdt)		mrunlock(&(fdt)->fd_lock)
#define FDT_INIT_LOCK(fdt)	mrinit(&(fdt)->fd_lock, "fdt_lock")
#define FDT_FREE_LOCK(fdt)	mrfree(&(fdt)->fd_lock)

#ifdef _KERNEL
extern struct vfile *	fdt_get_idbg(fdt_t *, int fd);
#endif

#endif /* _SYS_FDT_PRIVATE_H */
