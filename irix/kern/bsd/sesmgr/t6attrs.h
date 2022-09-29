#ifndef __T6ATTRS_H__
#define __T6ATTRS_H__

#ifdef	__cplusplus
extern "C" {
#endif

/*
 *   <tsix/t6attrs.h>
 *   This file contains external definitions for the libt6 library.
 */

/*
 *  Includes 
 */
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/param.h>

/*
 *  Typedefs
 */
typedef	void	*t6attr_t;		/* attribute control structure */
typedef __uint32_t	t6mask_t;	/* 32 bit attribute mask */

typedef struct t6groups {
	__uint32_t ngroups;
	gid_t groups[NGROUPS_UMAX];
} t6groups_t;

/*
 *   Constants passed in as the attribute type (t6attr_id_t).
 *   They are also used in the t6_mask() macro defined below.
 *   Note: some implementations may use them in accessing subject
 *   related attributes contained within a t6attr_t structure.
 */
typedef enum t6attr_id {
	T6_SL,			/* Sensitivity label */
	T6_NAT_CAVEATS,		/* Nationality caveats */
	T6_INTEG_LABEL,		/* Integrity label */
	T6_SESSION_ID,		/* Session ID */
	T6_CLEARANCE,		/* Clearance */
	T6_ACL,			/* Access Control List */
	T6_IL,			/* Information label */
	T6_PRIVILEGES,		/* Privileges */
	T6_AUDIT_ID,		/* Audit ID */
	T6_PID,			/* Process ID */
	T6_RESV10,		/* This field is reserved. It was used for
				   discretionary IDs by SAMP */
	T6_AUDIT_INFO,		/* Additional audit info */
	T6_UID,			/* Discretionary User ID */
	T6_GID,			/* Discretionary Group ID */
	T6_GROUPS,		/* Discretionary Supplementary Group IDs */
	T6_PROC_ATTR		/* Process Attribute (from Sun) */
} t6attr_id_t;

#define	T6_LOCAL_BEGIN	24	/* vendor's local defines from 24 - 31 */

/*
 *    Constants passed in as mask values (t6mask_t).
 *    Note:  These mask values match those defined by
 *           the TSIX(RE) 1.1 SAMP specification.
 */
#define	t6_mask(value)		((t6mask_t) (1<<(value)))

#define	T6M_SL		t6_mask(T6_SL)
#define	T6M_NAT_CAVEATS	t6_mask(T6_NAT_CAVEATS)
#define	T6M_INTEG_LABEL	t6_mask(T6_INTEG_LABEL)
#define	T6M_SESSION_ID	t6_mask(T6_SESSION_ID)
#define	T6M_CLEARANCE	t6_mask(T6_CLEARANCE)
#define T6M_ACL		t6_mask(T6_ACL)
#define	T6M_IL		t6_mask(T6_IL)
#define	T6M_PRIVILEGES	t6_mask(T6_PRIVILEGES)
#define	T6M_AUDIT_ID	t6_mask(T6_AUDIT_ID)
#define	T6M_PID		t6_mask(T6_PID)
#define T6M_RESV10	t6_mask(T6_RESV10)
#define	T6M_AUDIT_INFO	t6_mask(T6_AUDIT_INFO)
#define	T6M_UID		t6_mask(T6_UID)
#define	T6M_GID		t6_mask(T6_GID)
#define	T6M_GROUPS	t6_mask(T6_GROUPS)
#define T6M_PROC_ATTR	t6_mask(T6_PROC_ATTR)

#define	T6M_NO_ATTRS	((t6mask_t) 0)
#define T6M_ALL_ATTRS	((t6mask_t) ~0)

/* Attributes supported by SGI Trusted Irix */
#define T6M_TRIX_ATTRS	(T6M_SL | T6M_NAT_CAVEATS | T6M_INTEG_LABEL | \
			 T6M_SESSION_ID | T6M_ACL | T6M_PRIVILEGES |  \
			 T6M_AUDIT_ID | T6M_PID | T6M_AUDIT_INFO |  \
			 T6M_UID | T6M_GID | T6M_GROUPS)

/*
 *  Constants used as parameters to the library functions
 */
typedef enum t6cmd {T6_OFF, T6_ON} t6cmd_t;

/*
 *  libt6 interface function prototypes
 */

#ifdef __STDC__
/* new t6 control block interfaces */
extern t6attr_t	t6alloc_blk(t6mask_t);
extern void	t6free_blk(t6attr_t);
extern t6attr_t	t6dup_blk(const t6attr_t);
extern void	t6copy_blk(const t6attr_t, t6attr_t);
extern void	t6clear_blk(t6attr_t, t6mask_t);

/* old t6 control block interfaces */
extern t6attr_t	t6attr_alloc(void);
extern t6attr_t	t6create_attr(t6mask_t);
extern void	t6free_attr(t6attr_t);
extern t6attr_t	t6dup_attr(const t6attr_t);
extern void	t6copy_attr(const t6attr_t, t6attr_t);

extern void * 	t6get_attr(t6attr_id_t, const t6attr_t);
extern int 	t6set_attr(t6attr_id_t, const void *, t6attr_t);
extern size_t 	t6size_attr(t6attr_id_t, const t6attr_t);
extern int 	t6ext_attr(int, t6cmd_t);
extern int 	t6new_attr(int, t6cmd_t);
extern int 	t6get_endpt_mask(int, t6mask_t *);
extern int 	t6set_endpt_mask(int, t6mask_t);
extern int	t6get_endpt_default(int, t6mask_t *, t6attr_t);
extern int 	t6set_endpt_default(int, t6mask_t, const t6attr_t);
extern int 	t6sendto(int, const char *, int, int,
			 const struct sockaddr *, int, const t6attr_t);
extern int 	t6recvfrom(int, char *, int, int, struct sockaddr *,
			   int *, t6attr_t, t6mask_t *);
extern int 	t6peek_attr(int, t6attr_t, t6mask_t *);
extern int 	t6last_attr(int, t6attr_t, t6mask_t *);
extern int	t6mls_socket(int, t6cmd_t);
#else  /* !__STDC__ */
/* new t6 control block interfaces */
extern t6attr_t	t6alloc_blk(/* t6mask_t */);
extern void	t6free_blk(/* t6attr_t */);
extern t6attr_t	t6dup_blk(/* const t6attr_t */);
extern void	t6copy_blk(/* const t6attr_t, t6attr_t */);
extern void	t6clear_blk(/* t6attr_t, t6mask_t */);

/* old t6 control block interfaces */
extern t6attr_t	t6attr_alloc(/* void */);
extern t6attr_t	t6create_attr(/* t6mask_t */);
extern void	t6free_attr(/* t6attr_t */);
extern t6attr_t	t6dup_attr(/* const t6attr_t */);
extern void	t6copy_attr(/* const t6attr_t, t6attr_t */);

extern void * 	t6get_attr(/* t6attr_id_t attr_type, const t6attr_t t6ctl */);
extern int 	t6set_attr(/* t6attr_id_t attr_type, const void * attr, 
			   t6attr_t t6ctl */);
extern size_t 	t6size_attr(/* t6attr_id_t attr_type, const t6attr_t t6ctl */);
extern int 	t6ext_attr(/* int fd, t6cmd_t cmd */);
extern int 	t6new_attr(/* int fd, t6cmd_t cmd */);
extern int 	t6get_endpt_mask(/* int fd, t6mask_t *mask */);
extern int 	t6set_endpt_mask(/* int fd, t6mask_t mask */);
extern int	t6get_endpt_default(/* int fd, t6mask_t *mask,
			   t6attr_t attr */);
extern int 	t6set_endpt_default(/* int fd, t6mask_t mask, 
			   const t6attr_t attr_ptr */);
extern int 	t6sendto(/* int fd, const char *msg, int len, int flags,
		           const struct sockaddr *to, int tolen, 
			   const t6attr_t attr_ptr */);
extern int 	t6recvfrom(/* int fd, char *buf, int len, int flags, 
		           struct sockaddr *from, int *fromlen, 
			   t6attr_t attr_ptr, t6mask_t *new_attrs */);
extern int 	t6peek_attr(/* int fd, t6attr_t attr_ptr, 
			   t6mask_t *new_attrs */);
extern int 	t6last_attr(/* int fd, t6attr_t attr_ptr, 
			   t6mask_t *new_attrs */);
extern int	t6mls_socket(/* int, t6cmd_t */);
#endif /* __STDC__ */

/*
 *   Local definitions - vendors can add their definitions specific
 *   to their own implementation of TSIX library functions at the
 *   end of this include file.
 */

#ifdef	__cplusplus
}
#endif

#endif	/* __T6ATTRS_H__ */
