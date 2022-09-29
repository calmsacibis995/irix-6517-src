#ifndef __RPCSVC_MOUNT_H__
#define __RPCSVC_MOUNT_H__
/* @(#)mount.h	1.3 87/06/23 3.2/4.3NFSSRC */
/*      mount.h     1.1     86/09/25     */

/*
 * Copyright (c) 1984 Sun Microsystems, Inc.
 */

#define MOUNTPROG 100005
#define MOUNTPROC_MNT 1
#define MOUNTPROC_DUMP 2
#define MOUNTPROC_UMNT 3
#define MOUNTPROC_UMNTALL 4
#define MOUNTPROC_EXPORT 5
#define MOUNTPROC_EXPORTALL 6
#define MOUNTVERS_ORIG 1
#define MOUNTVERS 1

#ifndef svc_getcaller
#define svc_getcaller(x) (&(x)->xp_raddr)
#endif


struct mountlist {		/* what is mounted */
	char *ml_name;
	char *ml_path;
	struct mountlist *ml_nxt;
};

struct fhstatus {
	int fhs_status;
	fhandle_t fhs_fh;
};

/*
 * List of exported directories
 * An export entry with ex_groups
 * NULL indicates an entry which is exported to the world.
 */
struct exports {
	dev_t		  ex_dev;	/* dev of directory */
	char		 *ex_name;	/* name of directory */
	struct groups	 *ex_groups;	/* groups allowed to mount this entry */
	struct exports	 *ex_next;
	short		  ex_rootmap;	/* id to map root requests to */
	short		  ex_flags;	/* bits to mask off file mode */
};

struct groups {
	char		*g_name;
	struct groups	*g_next;
};
extern bool_t xdr_path(XDR *, char **);
extern bool_t xdr_fhandle(XDR *, fhandle_t *);
extern bool_t xdr_fhstatus(XDR *, struct fhstatus *);
extern bool_t xdr_mountlist(XDR *, struct mountlist **);
extern bool_t xdr_mountbody(XDR *, struct mountlist *);
extern bool_t xdr_groups(XDR *, struct groups *);
extern bool_t xdr_exports(XDR *, struct exports **);
extern bool_t xdr_exports(XDR *, struct exports **);

/*
 * Copyright 1990 Silicon Graphics, Inc.  All rights reserved.
 *
 * New mount protocol version which supports all current and any future
 * exports(4) options.
 */
#define MOUNTPROG_SGI 391004
#define MOUNTVERS_SGI_ORIG 1

/*
 * MOUNTPROC_EXPORTLIST returns a list of /etc/exports entries.
 * It is up to the client to interpret el_entry with getexportopt(3).
 * This procedure allows future options to be added to exportfs, 
 * without requiring a protocol turn.
 */
#define	MOUNTPROC_EXPORTLIST	99

struct exportlist;
struct exportentry;
extern bool_t xdr_exportlist(XDR *, struct exportlist **);
extern bool_t xdr_exportentry(XDR *, struct exportentry *);

extern void	bsd_init(void);
extern char	*get_class(char *, char *);


struct exportlist {
	struct exportentry {
		char		*ee_dirname;
		char		*ee_options;
	} el_entry;
	struct exportlist	*el_next;
};

#endif	/* !__RPCSVC_MOUNT_H__ */
