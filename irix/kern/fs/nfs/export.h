/*	@(#)export.h	1.6 D/NFS */
/*      @(#)export.h 1.4 88/02/08 SMI      */

#ifdef _KERNEL
#include "netinet/in.h"
#include "sys/socketvar.h"
#include "sys/socket.h"
#else
#include <netinet/in.h>
#include <sys/socketvar.h>
#include <sys/socket.h>
#endif

/*
 * exported vfs flags.
 */
#ifndef EX_RDONLY
#define EX_RDONLY	0x01		/* exported read only */
#endif
#define EX_RDMOSTLY	0x02		/* exported read mostly */
#define	EX_NOHIDE	0x04		/* visible from upper exported fs */
#define	EX_WSYNC	0x08		/* write synchronously to disk */
#define	EX_B32CLNT	0x10		/* mask off high 32bits of xfs cookie */
					/* so client won't reject xdr */
#define EX_NOXATTR	0x20		/* reject extended attribute requests
					   unless client is trusted */
#define	EX_ACCESS	0x40		/* exported access */

#define	EX_ALLFLAGS	0x7f

#define EXMAXADDRS	8192		/* max number in address list */

struct exaddrlist {
      unsigned naddrs;			/* number of addresses */
      struct sockaddr *addrvec;		/* pointer to array of addresses */
};

/*
 * Associated with AUTH_UNIX is an array of internet addresses
 * to check root permission.
 */
#define EXMAXROOTADDRS	8192		/* should be config option */
struct unixexport {
	struct exaddrlist rootaddrs;
};


/*
 * The export information passed to exportfs()
 */
struct export {
	int ex_flags;	/* flags */
	int ex_anon;	/* uid for unauthenticated requests */
	int ex_auth;	/* switch */
	union {
		struct unixexport exunix;	/* case AUTH_UNIX */
	} ex_u;
	struct exaddrlist ex_writeaddrs;
	struct exaddrlist ex_accessaddrs;
};
#define ex_unix ex_u.exunix

#ifdef _KERNEL
/*
 * Closed hash table of pointers into exaddrlist's addrvec array.
 */
struct exaddrhashtable {
      unsigned tablesize;		/* physical size of table */
      unsigned hashmask;		/* strength-reduced division hash */
      struct sockaddr **table;		/* pointers into addrvec */
      struct sockaddr **endtable;	/* one beyond last table element */
};

#define	exaddrhash(ht, sa) \
	(&(ht)->table[((struct sockaddr_in *)(sa))->sin_addr.s_addr \
		      & (ht)->hashmask])
/*
 * A node associated with an export entry on the list of exported
 * filesystems.
 */


struct exportinfo {
	struct	exportinfo *exi_next;
	fsid_t	exi_fsid;
	struct fid *exi_fid;
	int exi_loopback;
	struct vfs *exi_vfsp;
	struct export exi_export;
	struct exaddrhashtable exi_roothash;
	struct exaddrhashtable exi_writehash;
	struct exaddrhashtable exi_accesshash;
	/*
	 * NFS3 only
	 */
	fhandle_t	exi_fh;
};

struct irix5_exaddrlist {
      unsigned naddrs;			/* number of addresses */
      app32_ptr_t addrvec;		/* pointer to array of addresses */
};

struct irix5_unixexport {
	struct irix5_exaddrlist rootaddrs;
};

struct irix5_export {
	int ex_flags;	/* flags */
	int ex_anon;	/* uid for unauthenticated requests */
	int ex_auth;	/* switch */
	union {
		struct irix5_unixexport exunix;	/* case AUTH_UNIX */
	} ex_u;
	struct irix5_exaddrlist ex_writeaddrs;
	struct irix5_exaddrlist ex_accessaddrs;
};
extern	struct	exportinfo *exihashtab[];
#define	EXIHASHTABSIZE	256
#define	EXIHASH(fsid)	(exihashtab[(fsid)->val[0] % EXIHASHTABSIZE])

extern struct exportinfo *findexport(fsid_t *, struct fid *);
extern int	nfs_exportinit(void);
extern int	findexivp(struct exportinfo **, struct vnode *, struct vnode *);
extern int	makefh(fhandle_t *, struct vnode *, struct exportinfo *);
extern int      unexport(fsid_t *, struct fid *);
extern void	export(struct exportinfo*);
extern void     exportfree(struct exportinfo *);
extern int      loadaddrs(struct exaddrlist *, int, struct exaddrhashtable *);
extern int	eqaddr(struct sockaddr *, struct sockaddr *);
extern int      hostintable(struct sockaddr *, struct exaddrhashtable *);
extern int	makefh3(nfs_fh3 *, struct vnode *, struct exportinfo *);
extern void	nfs_purge_vfsp(struct vfs *);

extern mrlock_t exported_lock;
#define EXPORTED_MRRLOCK()	mrlock(&exported_lock, MR_ACCESS, PZERO-1)
#define EXPORTED_MRWLOCK()	mrlock(&exported_lock, MR_UPDATE, PZERO-1)
#define EXPORTED_MRUNLOCK()	mrunlock(&exported_lock)
#define EXPORTED_MRRHELD()	(RW_READ_HELD(&exported_lock))

#endif	/* _KERNEL */
