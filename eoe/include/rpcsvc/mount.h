#ifndef __RPCSVC_MOUNT_H__
#define __RPCSVC_MOUNT_H__
#ifdef __cplusplus
extern "C" {
#endif
#ident "$Revision: 1.15 $"
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
#define MOUNTVERS3      3
#define MOUNTVERS_SGI_ORIG_3    3

#define MNTPATHLEN 1024
#define MNTNAMLEN 255
#define FHSIZE 32
#define FHSIZE3 64

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

typedef char fhandle[FHSIZE];

typedef struct {
	u_int fhandle3_len;
	char *fhandle3_val;
} fhandle3;

enum mountstat3 {
	MNT_OK = 0,
	MNT3ERR_PERM = 1,
	MNT3ERR_NOENT = 2,
	MNT3ERR_IO = 5,
	MNT3ERR_ACCES = 13,
	MNT3ERR_NOTDIR = 20,
	MNT3ERR_INVAL = 22,
	MNT3ERR_NAMETOOLONG = 63,
	MNT3ERR_NOTSUPP = 10004,
	MNT3ERR_SERVERFAULT = 10006
};
typedef enum mountstat3 mountstat3;

struct mountres3_ok {
	fhandle3 fhandle;
	struct {
		u_int auth_flavors_len;
		int *auth_flavors_val;
	} auth_flavors;
};
typedef struct mountres3_ok mountres3_ok;

struct mountres3 {
	mountstat3 fhs_status;
	union {
		mountres3_ok mountinfo;
	} mountres3_u;
};
typedef struct mountres3 mountres3;

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
extern bool_t xdr_fhandle3(XDR *, fhandle3 *);
extern bool_t xdr_mountres3_ok(XDR *, mountres3_ok *);
extern bool_t xdr_mountres3(XDR *, mountres3 *);
extern bool_t xdr_mountstat3(XDR *, mountstat3 *);

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

struct exportlist {
	struct exportentry {
		char		*ee_dirname;
		char		*ee_options;
	} el_entry;
	struct exportlist	*el_next;
};

/*
 * MOUNTPROC_STATVFS returns the a statvfs(2)-like structure for
 * the mount point.  This can be used to setup constant values
 * for statvfs on the client side.  This function was not in the
 * original MOUNTPROG_SGI, but instead of using a new version
 * number, we depend on the server to reply with RPC_PROCUNAVAIL
 * to avoid the undesirable behaviour of portmap/rpcbind/svc_getreq
 * version matching.
 */
#define MOUNTPROC_STATVFS	100

struct mntrpc_statvfs {
	u_long	f_bsize;	/* fundamental file system block size */
	u_long	f_frsize;	/* fragment size */
	u_long	f_blocks;	/* total # of blocks of f_frsize on fs */
	u_long	f_bfree;	/* total # of free blocks of f_frsize */
	u_long	f_bavail;	/* # of free blocks avail to non-superuser */
	u_long	f_files;	/* total # of file nodes (inodes) */
	u_long	f_ffree;	/* total # of free file nodes */
	u_long	f_favail;	/* # of free nodes avail to non-superuser */
	u_long	f_fsid;		/* file system id (dev for now) */
	char	f_basetype[16]; /* target fs type name, null-terminated */
	u_long	f_flag;		/* bit-mask of flags */
	u_long	f_namemax;	/* maximum file name length */
	char	f_fstr[32];	/* filesystem-specific string */
};

extern bool_t xdr_statvfs(XDR *, struct mntrpc_statvfs *);

#ifdef __cplusplus
}
#endif
#endif /* !__RPCSVC_MOUNT_H__ */
