/**************************************************************************
 *									  *
 * 		 Copyright (C) 1989, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ifndef __SYS_CAPABILITY_H__
#define __SYS_CAPABILITY_H__

#ifdef __cplusplus
extern "C" {
#endif

#ident	"$Revision: 1.16 $"

/*
 * Data types for capability sets.
 * capabilities were called privileges prior to P1003.6D14
 *
 * XFS extended attribute names
 */
#define SGI_CAP_FILE		"SGI_CAP_FILE"
#define SGI_CAP_PROCESS		"SGI_CAP_PROCESS"
#define SGI_CAP_PROCESS_FLAGS	"SGI_CAP_PROCESS_FLAGS"
#define SGI_CAP_REQUEST		"SGI_CAP_REQUEST"
#define SGI_CAP_SURRENDER	"SGI_CAP_SURRENDER"
#define SGI_CAP_DISABLED	"SGI_CAP_DISABLED"
#define SGI_CAP_SUPERUSER	"SGI_CAP_SUPERUSER"
#define SGI_CAP_NO_SUPERUSER	"SGI_CAP_NO_SUPERUSER"

#define SGI_CAP_FILE_SIZE	(sizeof (SGI_CAP_FILE) - 1)
#define SGI_CAP_PROCESS_SIZE	(sizeof (SGI_CAP_PROCESS) - 1)
#define SGI_CAP_PROCESS_FLAGS_SIZE	(sizeof (SGI_CAP_PROCESS_FLAGS) - 1)
#define SGI_CAP_REQUEST_SIZE	(sizeof (SGI_CAP_REQUEST) - 1)
#define SGI_CAP_SURRENDER_SIZE	(sizeof (SGI_CAP_SURRENDER) - 1)

/*
 * System capability states
 */
#define CAP_SYS_DISABLED	0	/* Traditional SuperUser */
#define CAP_SYS_SUPERUSER	1	/* Caps, Plus Traditional SuperUser */
#define CAP_SYS_NO_SUPERUSER	2	/* Caps, no SuperUser */
/*
 * Capabilities required by P1003.6D16
 * Capabilities required by Appendix B of the CMW spec.
 */
#define CAP_NOT_A_CID		0LL
#define	CAP_CHOWN		(0x01LL << 1)
#define	CAP_DAC_WRITE		(0x01LL << 2)
#define	CAP_DAC_READ_SEARCH	(0x01LL << 3)
#define	CAP_FOWNER		(0x01LL << 4)
/*
 * XXX:casey
 * CAP_DAC_OVERRIDE was defined in P1003.6D14, so it got into some code.
 */
#define	CAP_DAC_OVERRIDE	(CAP_DAC_WRITE|CAP_DAC_READ_SEARCH|CAP_FOWNER)
#define	CAP_FSETID		(0x01LL << 5)
#define	CAP_KILL		(0x01LL << 6)
#define	CAP_LINK_DIR		(0x01LL << 7)
#define	CAP_SETFPRIV		(0x01LL << 8)
#define CAP_SETFCAP		CAP_SETFPRIV
#define	CAP_SETPPRIV		(0x01LL << 9)
#define CAP_SETPCAP		CAP_SETPPRIV
#define	CAP_SETGID		(0x01LL << 10) /* gid, group list, pgid */
#define	CAP_SETUID		(0x01LL << 11)

#define	CAP_MAC_DOWNGRADE	(0x01LL << 12)
#define	CAP_MAC_READ		(0x01LL << 13)
#define	CAP_MAC_RELABEL_SUBJ	(0x01LL << 14)
#define	CAP_MAC_WRITE		(0x01LL << 15)
#define	CAP_MAC_UPGRADE		(0x01LL << 16)

#define	CAP_INF_NOFLOAT_OBJ	(0x01LL << 17)	/* Currently unused */
#define	CAP_INF_NOFLOAT_SUBJ	(0x01LL << 18)	/* Currently unused */
#define	CAP_INF_DOWNGRADE	(0x01LL << 19)	/* Currently unused */
#define	CAP_INF_UPGRADE		(0x01LL << 20)	/* Currently unused */
#define	CAP_INF_RELABEL_SUBJ	(0x01LL << 21)	/* Currently unused */

#define	CAP_AUDIT_CONTROL	(0x01LL << 22)
#define	CAP_AUDIT_WRITE		(0x01LL << 23)

#define	CAP_MAC_MLD		(0x01LL << 24)
#define	CAP_MEMORY_MGT		(0x01LL << 25)
#define	CAP_SWAP_MGT		(0x01LL << 26)
#define	CAP_TIME_MGT		(0x01LL << 27)
#define	CAP_SYSINFO_MGT		(0x01LL << 28)
#define	CAP_NVRAM_MGT		CAP_SYSINFO_MGT
#define	CAP_MOUNT_MGT		(0x01LL << 29)
#define	CAP_QUOTA_MGT		(0x01LL << 30)
#define	CAP_PRIV_PORT		(0x01LL << 31)
#define	CAP_STREAMS_MGT		(0x01LL << 32)
#define	CAP_SCHED_MGT		(0x01LL << 33)
#define	CAP_PROC_MGT		(0x01LL << 34)
#define	CAP_SVIPC_MGT		(0x01LL << 35)
#define	CAP_NETWORK_MGT		(0x01LL << 36)
#define	CAP_DEVICE_MGT		(0x01LL << 37)
#define	CAP_MKNOD		CAP_DEVICE_MGT
#define	CAP_ACCT_MGT		(0x01LL << 38)
#define	CAP_SHUTDOWN		(0x01LL << 39)
#define	CAP_CHROOT		(0x01LL << 40)

#define	CAP_DAC_EXECUTE		(0x01LL << 41)
#define	CAP_MAC_RELABEL_OPEN	(0x01LL << 42)

#define	CAP_SIGMASK		(0x01LL << 43)	/* not implemented */

#define	CAP_XTCB		(0x01LL << 44)	/* X11 Trusted Clients */

#define	CAP_MAX_ID		44

#define CAP_FLAG_PURE_RECALC	(0x01LL << 63)
/*
 * The least significant bit indicates an invalid capability_t
 * The 4 most significant bits are researved for "flags".
 */
#define CAP_FLAGS	0xf000000000000000LL
#define CAP_ALL_ON	0x0ffffffffffffffeLL
#define CAP_INVALID	0x0000000000000001LL
#define CAP_ALL_OFF	0x0000000000000000LL

typedef __uint64_t cap_value_t;
typedef	__uint64_t cap_flag_t;
typedef __uint64_t cap_flag_value_t;

struct cap_set {
	cap_value_t	cap_effective;	/* use in capability checks */
	cap_value_t	cap_permitted;	/* combined with file attrs */
	cap_value_t	cap_inheritable;/* pass through exec */
};
typedef struct cap_set cap_set_t;
typedef struct cap_set * cap_t;

/*
 * cap_flag_t Values
 */
#define CAP_EFFECTIVE	0
#define CAP_PERMITTED	1
#define CAP_INHERITABLE	2

/*
 * cap_flag_value_t Values
 */
#define CAP_CLEAR	0
#define CAP_SET		1

/*
 * CAP_ID_ISSET compares against "c" so that constructs like
 * CAP_DAC_OVERRIDE can be used.
 */
#define CAP_ID_CLEAR(c,s) ((s) &= (~(c)))
#define CAP_ID_SET(c,s) ((s) |= (c))
#define CAP_ID_ISSET(c,s) (((s) & (c)) == c)

#define _CAP_NUM(c) (sizeof(c)/sizeof(cap_value_t))

/*
 * flags for cap_envp and cap_envl
 */
#define CAP_ENV_SETUID		0x001
#define CAP_ENV_RECALC		0x002

/* function prototypes */

#ifdef _KERNEL
struct vfs;
struct vnode;
struct proc;
struct cred;
struct xfs_inode;

extern void cap_empower_cred( struct cred * );

extern int cap_able( cap_value_t );
extern int cap_request( cap_value_t );
extern int cap_surrender( cap_value_t );
extern int cap_able_cred( struct cred *, cap_value_t );
extern int cap_able_any( struct cred *);
extern int cap_recalc( const struct cap_set * );
extern int cap_vtocap( struct vnode *, cap_t );
extern int cap_setpcap( cap_t, cap_value_t *);
extern int cap_get( char *, int, cap_t );
extern int cap_set( char *, int, cap_t );
extern int cap_style( int );

/* Define macros choosing stub functions or real functions here */
extern int cap_enabled;

#define _CAP_ABLE(c)		(cap_able(c))
#define _CAP_CRABLE(cr,c)	(cap_able_cred(cr,c))

#else  /* _KERNEL */

/* POSIX.6 Capability Functions, in alphabetical order */
int cap_clear (cap_t);
ssize_t cap_copy_ext (void *, cap_t, ssize_t);
cap_t cap_copy_int (const void *);
cap_t cap_dup (cap_t);
int cap_free (void *);
cap_t cap_from_text (const char *);
cap_t cap_get_fd (int);
cap_t cap_get_file (const char *);
int cap_get_flag (cap_t, cap_value_t, cap_flag_t, cap_flag_value_t *);
cap_t cap_get_proc (void);
cap_t cap_init (void);
int cap_set_fd (int, cap_t);
int cap_set_file (const char *, cap_t);
int cap_set_flag (cap_t, cap_flag_t, int, cap_value_t *, cap_flag_value_t);
int cap_set_proc (cap_t);
int cap_set_proc_flags (cap_value_t);
ssize_t cap_size (cap_t);
char *cap_to_text (cap_t, size_t *);

/* convenience functions */
cap_t cap_acquire (int, const cap_value_t *);
void cap_surrender (cap_t);
char *cap_value_to_text (cap_value_t);
int cap_envl (int, ...);
int cap_envp (int, size_t, const cap_value_t *);

#endif /* _KERNEL */

#ifdef __cplusplus
}
#endif

#endif	/* __SYS_CAPABILITY_H_ */
