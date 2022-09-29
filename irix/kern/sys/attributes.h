/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef _ATTRIBUTES_H
#define	_ATTRIBUTES_H
#ident	"$Revision: 1.11 $"
#ifdef __cplusplus
extern "C" {
#endif

/*
 * sys/attributes.h
 */

/*
 * The maximum size (into the kernel or returned from the kernel) of an
 * attribute value or the buffer used for an attr_list() call.  Larger
 * sizes will result in an E2BIG return code.
 */
#define	ATTR_MAX_VALUELEN	(64*1024)	/* max length of a value */

/*
 * Flags that can be used with any of the simple attribute calls.
 * All desired flags should be bit-wise OR'ed together.
 */
#define ATTR_DONTFOLLOW	0x0001	/* do not follow symlinks for a pathname */
#define ATTR_ROOT	0x0002	/* use root-defined attrs in op, not user */
#define ATTR_TRUST	0x0004	/* tell server we can be trusted to properly
				   handle extended attributes */

/*
 * Additional flags that can be used with the set() attribute call.
 * All desired flags (from both lists) should be bit-wise OR'ed together.
 */
#define ATTR_CREATE	0x0010	/* pure create: fail if attr already exists */
#define ATTR_REPLACE	0x0020	/* pure set: fail if attr does not exist */

#ifdef _KERNEL
/*
 * The DMI needs a way to update attributes without affecting the inode
 * timestamps.  Note that this flag is not settable from user mode, it is
 * kernel internal only, but it must not conflict with the above flags either.
 */
#define ATTR_KERNOTIME	0x1000	/* don't update the inode timestamps */
#endif /* _KERNEL */

/*
 * Define how lists of attribute names are returned to the user from
 * the attr_list() syscall.  A large, 32bit aligned, buffer is passed in
 * along with its size.  We put an array of offsets at the top that each
 * reference an attrlist_ent_t and pack the attrlist_ent_t's at the bottom.
 */
struct attrlist {
	__int32_t	al_count;	/* number of entries in attrlist */
	__int32_t	al_more;	/* T/F: more attrs (do syscall again) */
	__int32_t	al_offset[1];	/* byte offsets of attrs [var-sized] */
};
typedef struct attrlist attrlist_t;

/*
 * Show the interesting info about one attribute.  This is what the
 * al_offset[i] entry points to.
 */
struct attrlist_ent {			/* data from attr_list() */
	u_int32_t	a_valuelen;	/* number bytes in value of attr */
	char		a_name[1];	/* attr name (NULL terminated) */
};
typedef struct attrlist_ent	attrlist_ent_t;

/*
 * Given a pointer to the (char*) buffer containing the attr_list() result,
 * and an index, return a pointer to the indicated attribute in the buffer.
 */
#define	ATTR_ENTRY(buffer, index)		\
	((attrlist_ent_t *)			\
	 &((char *)buffer)[ ((attrlist_t *)(buffer))->al_offset[index] ])

/*
 * Implement a "cursor" for use in successive attr_list() system calls.
 * It provides a way to find the last attribute that was returned in the
 * last attr_list() syscall so that we can get the next one without missing
 * any.  This should be bzero()ed before use and whenever it is desired to
 * start over from the beginning of the attribute list.  The only valid
 * operation on a cursor is to bzero() it.
 */
struct attrlist_cursor {
	u_int32_t	opaque[4];	/* an opaque cookie */
};
typedef struct attrlist_cursor attrlist_cursor_t;

#ifdef _KERNEL
/*
 * Kernel-internal version of the attrlist cursor.
 */
struct attrlist_cursor_kern {
	u_int32_t	hashval;	/* hash value of next entry to add */
	u_int32_t	blkno;		/* block containing entry (suggestion)*/
	u_int32_t	offset;		/* offset in list of equal-hashvals */
	u_int16_t	pad1;		/* padding to match user-level */
	u_int8_t	pad2;		/* padding to match user-level */
	u_int8_t	initted;	/* T/F: cursor has been initialized */
};
typedef struct attrlist_cursor_kern attrlist_cursor_kern_t;
#endif /* _KERNEL */

/*
 * Multi-attribute operation vector.
 */
struct attr_multiop {
	int	am_opcode;	/* which operation to perform (see below) */
	int	am_error;	/* [out arg] result of this sub-op (an errno) */
	char	*am_attrname;	/* attribute name to work with */
	char	*am_attrvalue;	/* [in/out arg] attribute value (raw bytes) */
	int	am_length;	/* [in/out arg] length of value */
	int	am_flags;	/* flags (bit-wise OR of #defines above) */
};
typedef struct attr_multiop attr_multiop_t;
#define	ATTR_MAX_MULTIOPS	128	/* max number ops in an oplist array */

#ifdef _KERNEL
/*
 * Kernel versions of the multi-attribute operation structure,
 * one for each of the 32bit and 64bit ABIs.
 */
struct attr_multiop_kern_32 {
	__int32_t	am_opcode;	/* which operation to perform */
	__int32_t	am_error;	/* [out arg] result of this sub-op */
	app32_ptr_t	am_attrname;	/* attribute name to work with */
	app32_ptr_t	am_attrvalue;	/* [in/out arg] attribute value */
	__int32_t	am_length;	/* [in/out arg] length of value */
	__int32_t	am_flags;	/* flags (bit-wise OR of lists below) */
	
};
struct attr_multiop_kern_64 {
	__int32_t	am_opcode;	/* which operation to perform */
	__int32_t	am_error;	/* [out arg] result of this sub-op */
	app64_ptr_t	am_attrname;	/* attribute name to work with */
	app64_ptr_t	am_attrvalue;	/* [in/out arg] attribute value */
	__int32_t	am_length;	/* [in/out arg] length of value */
	__int32_t	am_flags;	/* flags (bit-wise OR of lists below) */
	
};
#endif /* _KERNEL */

/*
 * Valid values of am_opcode.
 */
#define ATTR_OP_GET	1	/* return the indicated attr's value */
#define ATTR_OP_SET	2	/* set/create the indicated attr/value pair */
#define ATTR_OP_REMOVE	3	/* remove the indicated attr */


#ifndef _KERNEL
/*
 * Get the value of an attribute.
 * Valuelength must be set to the maximum size of the value buffer, it will
 * be set to the actual number of bytes used in the value buffer upon return.
 * The return value is -1 on error (w/errno set appropriately), 0 on success.
 */
int attr_get(const char *path, const char *attrname, const char *attrvalue,
		   int *valuelength, int flags);
int attr_getf(int fd, const char *attrname, char *attrvalue, int *valuelength,
		  int flags);

/*
 * Set the value of an attribute, creating the attribute if necessary.
 * The return value is -1 on error (w/errno set appropriately), 0 on success.
 */
int attr_set(const char *path, const char *attrname, const char *attrvalue,
		   const int valuelength, int flags);
int attr_setf(int fd, const char *attrname, const char *attrvalue,
		  const int valuelength, int flags);

/*
 * Remove an attribute.
 * The return value is -1 on error (w/errno set appropriately), 0 on success.
 */
int attr_remove(const char *path, const char *attrname, int flags);
int attr_removef(int fd, const char *attrname, int flags);

/*
 * List the names and sizes of the values of all the attributes of an object.
 * "Cursor" must be allocated and zeroed before the first call, it is used
 * to maintain context between system calls if all the attribute names won't
 * fit into the buffer on the first system call.
 * The return value is -1 on error (w/errno set appropriately), 0 on success.
 */
int attr_list(const char *path, char *buffer, const int buffersize, int flags,
		    attrlist_cursor_t *cursor);
int attr_listf(int fd, char *buffer, const int buffersize, int flags,
		   attrlist_cursor_t *cursor);

/*
 * Operate on multiple attributes of the same object simultaneously.
 *
 * This call will save on system call overhead when many attributes are
 * going to be operated on.
 *
 * The return value is -1 on error (w/errno set appropriately), 0 on success.
 * Note that this call will not return -1 as a result of failure of any
 * of the sub-operations, their return value is stored in each element
 * of the operation array.  This call will return -1 for a failure of the
 * call as a whole, eg: if the pathname doesn't exist, or the fd is bad.
 *
 * The semantics and allowable values for the fields in a attr_multiop_t
 * are the same as the semantics and allowable values for the arguments to
 * the corresponding "simple" attribute interface.  For example: the args
 * to a ATTR_OP_GET are the same as the args to an attr_get() call.
 */
int attr_multi(const char *path, attr_multiop_t *oplist, int count, int flags);
int attr_multif(int fd, attr_multiop_t *oplist, int count, int flags);

/*
 * Given two pathnames or two file descriptors, copy all of the Extended
 * User Attributes from one to the other.  Do this for both the User Namespace
 * and the Root-Only namespace.  If we are not running as root, then the
 * root-only namespace will (silently) not be copied.
 *
 * The "flags" argument is parallel to the "flags" argument to the attribute
 * system calls.  It allows the caller to specify whether to follow symlinks
 * or not.
 *
 * The caller can distinguish between read failures and write failures.
 * Failures while reading an attribute from the source file return 1 and
 * errno is set accordingly by the attr_get(2) or attr_list(2) syscalls.
 * Failures while writing an attribute to the destination file return -1 and
 * errno is set accordingly by the attr_set(2) syscall.
 *
 * A return value of zero means that all attributes were copied successfully
 * (except possibly the root-only attributes, see above).
 */
int attr_clone(const char *srcpath, const char *dstpath, int flags);
int attr_clonef(int srcfd, int dstfd, int flags);
#endif /* !_KERNEL */

#ifdef	_KERNEL

struct	vnode;
struct	cred;
union	rval;

/*	Kernel internal procedures for attr_list and attr_multi.	*/

int
cattr_list(struct vnode  *vp, char *bufferp, int bufsize, int flags,
                   attrlist_cursor_kern_t *cursorp,
                   struct cred *cred, union rval *rvp);

int
cattr_multi(struct vnode  *vp, caddr_t *oplistp, int count, int flags,
                    struct cred *cred, union rval *rvp);
#endif	/* _KERNEL */

#ifdef __cplusplus
}
#endif

#endif	/* !ATTRIBUTES_H */
