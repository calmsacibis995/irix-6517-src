#ifndef __XATTR_HEADER__
#define __XATTR_HEADER__

#ident "$Revision: 1.4 $"

#include <sys/attributes.h>
#include <sys/acl.h>
#include <sys/capability.h>
#include <sys/mac.h>
#include <sys/mac_label.h>

/* protocol revision independent constants */
#define XATTR_PROGRAM		((u_long) 391063)
#define XATTR_PORT		2049

/* protocol revision dependent constants */
#define XATTR1_VERSION		((u_long) 0)

/*
 * Version 1 declarations and definitions
 */
typedef char *xattrname1;

struct xattrdata1 {
	uint len;
	char *value;
};
typedef struct xattrdata1 xattrdata1;

struct GETXATTR1args {
	nfs_fh3 object;
	xattrname1 name;
	uint32 length;
	uint32 flags;
};
typedef struct GETXATTR1args GETXATTR1args;

struct GETXATTR1resok {
	post_op_attr obj_attributes;
	xattrdata1 data;
	u_int size;
};
typedef struct GETXATTR1resok GETXATTR1resok;

struct GETXATTR1resfail {
	post_op_attr obj_attributes;
};
typedef struct GETXATTR1resfail GETXATTR1resfail;

struct GETXATTR1res {
	nfsstat3 status;
	union {
		GETXATTR1resok ok;
		GETXATTR1resfail fail;
	} res_u;
};
typedef struct GETXATTR1res GETXATTR1res;

struct SETXATTR1args {
	nfs_fh3 object;
	xattrname1 name;
	xattrdata1 data;
	uint32 flags;
};
typedef struct SETXATTR1args SETXATTR1args;

struct SETXATTR1resok {
	wcc_data obj_wcc;
};
typedef struct SETXATTR1resok SETXATTR1resok;

struct SETXATTR1resfail {
	wcc_data obj_wcc;
};
typedef struct SETXATTR1resfail SETXATTR1resfail;

struct SETXATTR1res {
	nfsstat3 status;
	union {
		SETXATTR1resok ok;
		SETXATTR1resfail fail;
	} res_u;
};
typedef struct SETXATTR1res SETXATTR1res;

struct RMXATTR1args {
	nfs_fh3 object;
	xattrname1 name;
	uint32 flags;
};
typedef struct RMXATTR1args RMXATTR1args;

struct RMXATTR1resok {
	wcc_data obj_wcc;
};
typedef struct RMXATTR1resok RMXATTR1resok;

struct RMXATTR1resfail {
	wcc_data obj_wcc;
};
typedef struct RMXATTR1resfail RMXATTR1resfail;

struct RMXATTR1res {
	nfsstat3 status;
	union {
		RMXATTR1resok ok;
		RMXATTR1resfail fail;
	} res_u;
};
typedef struct RMXATTR1res RMXATTR1res;

struct LISTXATTR1args {
	nfs_fh3 object;
	uint32 length;
	uint32 flags;
	attrlist_cursor_kern_t cursor;
};
typedef struct LISTXATTR1args LISTXATTR1args;

struct LISTXATTR1resok {
	post_op_attr obj_attributes;
	xattrdata1 data;
	attrlist_cursor_kern_t cursor;
	u_int size;
};
typedef struct LISTXATTR1resok LISTXATTR1resok;

struct LISTXATTR1resfail {
	post_op_attr obj_attributes;
};
typedef struct LISTXATTR1resfail LISTXATTR1resfail;

struct LISTXATTR1res {
	nfsstat3 status;
	union {
		LISTXATTR1resok ok;
		LISTXATTR1resfail fail;
	} res_u;
};
typedef struct LISTXATTR1res LISTXATTR1res;

#define XATTRPROC1_NULL		((u_long) 0)
#define XATTRPROC1_GETXATTR	((u_long) 1)
#define XATTRPROC1_SETXATTR	((u_long) 2)
#define XATTRPROC1_RMXATTR	((u_long) 3)
#define XATTRPROC1_LISTXATTR	((u_long) 4)

#define XATTR_NPROC		5

#define XATTR_IDEMPOTENT	0x01

#ifdef __STDC__
extern bool_t xdr_GETXATTR1args(XDR *, caddr_t);
extern bool_t xdr_GETXATTR1res(XDR *, caddr_t);
extern bool_t xdr_SETXATTR1args(XDR *, caddr_t);
extern bool_t xdr_SETXATTR1res(XDR *, caddr_t);
extern bool_t xdr_RMXATTR1args(XDR *, caddr_t);
extern bool_t xdr_RMXATTR1res(XDR *, caddr_t);
extern bool_t xdr_LISTXATTR1args(XDR *, caddr_t);
extern bool_t xdr_LISTXATTR1res(XDR *, caddr_t);
#else
extern bool_t xdr_GETXATTR1args();
extern bool_t xdr_GETXATTR1res();
extern bool_t xdr_SETXATTR1args();
extern bool_t xdr_SETXATTR1res();
extern bool_t xdr_RMXATTR1args();
extern bool_t xdr_RMXATTR1res();
extern bool_t xdr_LISTXATTR1args();
extern bool_t xdr_LISTXATTR1res();
#endif /* !__STDC__ */

struct svc_req;
struct mntinfo;
struct cred;

extern int xattrcall (struct mntinfo *, int, xdrproc_ansi_t, caddr_t,
		      xdrproc_ansi_t, caddr_t, struct cred *, enum nfsstat3 *);
extern int xattr_dispatch (struct svc_req *, XDR *, caddr_t, caddr_t);

/*
 * Attribute cache definitions
 */

#ifdef _KERNEL
/*
 * The current values for extended attributes which are used
 * during pathname resolution or any other high frequency 
 * operation.
 */
typedef struct xattr_cache {
	mrlock_t		ea_lock;	/* reader/writer lock for
						   cache */
	int			ea_state;	/* status of these fields */
	mac_t			ea_mac;		/* Combined S&I Label */
	msen_t			ea_msen;	/* Sensitivity Label */
	mint_t			ea_mint;	/* Integrity Label */
	struct cap_set		ea_cap;		/* Capability Sets */
	struct acl		ea_acl;		/* Access Control List */
	struct acl		ea_def_acl;	/* Directory Default ACL */
} xattr_cache_t;

/*
 * struct xattr_cache state bits.
 */
#define XATTR_MAC	0x001
#define XATTR_MSEN	0x002
#define XATTR_MINT	0x004
#define XATTR_CAP	0x008
#define XATTR_ACL	0x010
#define XATTR_DEF_ACL	0x020

/*
 * Inline helper functions
 */
__inline static void
xattr_cache_mark (xattr_cache_t *eap, int arg)
{
	eap->ea_state |= arg;
}

__inline static int
xattr_cache_valid (xattr_cache_t *eap, int attr)
{
	return (eap != NULL && (eap->ea_state & attr) != 0);
}

__inline static void
xattr_cache_invalidate (xattr_cache_t *eap)
{
	if (eap != NULL)
		eap->ea_state = 0;
}

/*
 * Function prototypes for the kernel
 */
void xattr_cache_free (xattr_cache_t *);

int  getxattr_cache(bhv_desc_t *, char *, char *, int *, int, struct cred *,
		    int *);
int  getxattr_otw(bhv_desc_t *, char *, char *, int *, int, struct cred *);

#endif	/* _KERNEL */

#endif /* __XATTR_HEADER__ */
