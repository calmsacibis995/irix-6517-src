/**************************************************************************
 *									  *
 *		 Copyright (C) 1997 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ifndef	_CXFS_SINFO_H_
#define	_CXFS_SINFO_H_

#ident	"$Id: cxfs_sinfo.h,v 1.1 1997/08/26 18:22:56 dnoveck Exp $"

/*
 * fs/xfs/cell/v1/cxfs_sinfo.h -- Server info for cxfs file systems
 *
 * This header file defines the cxfs server info structure in which we 
 * maintain information regarding the current and potential servers 
 * for a cxfs file system in the Enterprise configuration.
 * 
 * It should only be included in code within the cxfs (fs/xfs/cell/v1)
 * directory and should never be included in non CELL_ARRAY configurations.
 */
 
#ifndef CELL_ARRAY
#error included by non-CELL_ARRAY configuration
#endif

#include <sys/types.h>
#include <sys/uuid.h>
#include <sys/vfs.h>
#include <ksys/kqueue.h>
#include <fs/cell/fsc_types.h>
#include <fs/xfs/xfs_mount.h>
#include <fs/xfs/xfs_clnt.h>

struct cxfs_sinfo {
        kqueue_t        cxs_queue;      /* Doubly-link list of all sinfo's. */
        uuid_t          cxs_uuid;       /* Id for this filesystem. */
        ulong_t         cxs_digest;     /* Id digest for quicker scanning. */
	size_t		cxs_length;	/* Total length of data area. */
        int             cxs_count;      /* Count of potential servers. */
        char            **cxs_addrs;    /* Table of server names. */
        char            *cxs_strings;   /* Server name string area. */
        int             cxs_order;      /* Our order in server list. */
        xfs_mount_t     *cxs_mount;     /* Associated xfs_mount address. */
        sv_t            cxs_sv;         /* An sv for status changes. */
        char            cxs_server;     /* Set if we are the server */
        char            cxs_trying;     /* Trying to become the server. */
        char            cxs_waiting;    /* Someone waiting for status change. */
        char            cxs_inlist;     /* Set if we are in the list. */
        struct xfs_args cxs_args;       /* Canonical version of mount 
                                           arguments. */
  
};

extern cxfs_sinfo_t *	cxfs_sinfo_create(struct xfs_args *ap,
					  xfs_mount_t *mp,
					  size_t stringlen);
extern cxfs_sinfo_t *	cxfs_sinfo_unmount(xfs_mount_t *mp);
extern void             cxfs_sinfo_free(cxfs_sinfo_t *cxsp);
extern void             cxfs_sinfo_install(cxfs_sinfo_t *cxsp);
extern void             cxfs_sinfo_extract(cxfs_sinfo_t *cxsp);
extern void             cxfs_sinfo_client(cxfs_sinfo_t *cxsp);
extern void             cxfs_sinfo_server(cxfs_sinfo_t *cxsp);
extern void             cxfs_find_self(cxfs_sinfo_t *cxsp);
extern int              cxfs_sinfo_findvfs(uuid_t *idp, 
					   struct xfs_args *ap,
					   vfs_t **);
extern void             cxfs_sinfo_init(void);
extern ulong_t          cxfs_sinfo_digest(uuid_t *idp);

#endif /* _CXFS_SINFO_H_ */





