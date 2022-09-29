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
/*
 * imon.h - inode monitor definitions
 *
 * $Revision: 1.11 $
 * $Date: 1996/06/20 23:59:58 $
 */

#ifndef _SYS_IMON_H
#define _SYS_IMON_H

#ifdef __cplusplus
extern "C" {
#endif

typedef ushort intmask_t;

/* Interest mask bits */
#define IMON_CONTENT	(1 << 0)	/* contents or size have changed */
#define IMON_ATTRIBUTE	(1 << 1)	/* mode or ownership have changed */
#define IMON_DELETE	(1 << 2)	/* last link has gone away */
#define IMON_EXEC	(1 << 3)	/* process executing */
#define IMON_EXIT	(1 << 4)	/* last process exited */
#define IMON_RENAME	(1 << 5)	/* inode has been renamed */
#ifdef _KERNEL
#define IMON_UNMOUNT	(1 << 13)	/* filesystem has been unmounted */
#define IMON_EXECUTING	(1 << 14)	/* internal file-in-execution flag */
#define IMON_COLLISION	(1 << 15)	/* internal hash collision flag */
#endif
#define IMON_CONTAINED	(1 << 15)	/* monitor all contained files in */
					/* a filesystem device */

#define IMON_OVER	0xff		/* queue has overflowed */
					/* XXX should be 0xffff */
#define IMON_EVENTMASK	0x3fff		/* interest bits for all events */
#define IMON_ALL	IMON_EVENTMASK	/* XXX archaic name */

/* User-specifiable mask bits */
#define	IMON_USERMASK	(IMON_CONTAINED|IMON_EVENTMASK)

/* Event queue element */
typedef struct qelem {
	ino_t		qe_inode;	/* inode number of file */
	dev_t		qe_dev;		/* device of file */
	intmask_t	qe_what;	/* what events occurred */
} qelem_t;

#ifdef _KERNEL

typedef struct {
	app32_ulong_t	qe_inode;	/* inode number of file */
	app32_ulong_t	qe_dev;		/* device of file */
	intmask_t	qe_what;	/* what events occurred */
} irix5_o32_qelem_t;

typedef struct {
	__uint64_t	qe_inode;	/* inode number of file */
	app32_ulong_t	qe_dev;		/* device of file */
	intmask_t	qe_what;	/* what events occurred */
} irix5_n32_qelem_t;

#if _MIPS_SIM == _ABI64
typedef struct {
	app64_ulong_t	qe_inode;	/* inode number of file */
	__uint32_t	qe_dev;		/* device of file */
	intmask_t	qe_what;	/* what events occurred */
} irix5_64_qelem_t;
#endif

#endif /* _KERNEL */

#define IMON_ANYINODE	((ino_t) 0)	/* inumber wildcard */

/* Imon ioctls */
/* #define oIMONIOC_EXPRESS	(('i' << 8) | 1)	old stat struct */
/* #define oIMONIOC_REVOKE	(('i' << 8) | 2)	old stat struct */
#define IMONIOC_QTEST	(('i' << 8) | 3)
#define IMONIOC_EXPRESS	(('i' << 8) | 4)
#define IMONIOC_REVOKE	(('i' << 8) | 5)
#define IMONIOC_REVOKDI (('i' << 8) | 6)

/* Interest structure for express/delete */
typedef struct interest {
	char		*in_fname;	/* pathname */
	struct stat	*in_sb;		/* optional status return buffer */
	intmask_t	in_what;	/* what types of events to send */
} interest_t;

#ifdef _KERNEL

/* Compile mode-independent version of the interest structure */
typedef struct {
	app32_ptr_t	in_fname;	/* pathname */
	app32_ptr_t	in_sb;		/* optional status return buffer */
	intmask_t	in_what;	/* what types of events to send */
} irix5_32_interest_t;

#if _MIPS_SIM == _ABI64
typedef struct {
	app64_ptr_t	in_fname;	/* pathname */
	app64_ptr_t	in_sb;		/* optional status return buffer */
	intmask_t	in_what;	/* what types of events to send */
} irix5_64_interest_t;
#endif

#endif /* _KERNEL */

/* arg structure for IMONIOC_REVOKDI */
typedef struct revokdi {
	dev_t		rv_dev;
	ino_t		rv_ino;
	intmask_t	rv_what;
} revokdi_t;

#ifdef _KERNEL

/* Compile mode-independent version of the revokdi structure */
typedef struct {
	app32_ulong_t	rv_dev;		/* device of file */
	app32_ulong_t	rv_ino;		/* inode number of file */
	intmask_t	rv_what;	/* what events to revoke */
} irix5_o32_revokdi_t;

typedef struct {
	app32_ulong_t	rv_dev;		/* device of file */
	__uint64_t	rv_ino;		/* inode number of file */
	intmask_t	rv_what;	/* what events to revoke */
} irix5_n32_revokdi_t;

#if _MIPS_SIM == _ABI64
typedef struct {
	__uint32_t	rv_dev;		/* device of file */
	app64_ulong_t	rv_ino;		/* inode number of file */
	intmask_t	rv_what;	/* what events to revoke */
} irix5_64_revokdi_t;
#endif
 
extern void (*imon_hook)(struct vnode *, dev_t, ino_t);
extern void (*imon_event)(struct vnode *, struct cred *cr, int);
extern void (*imon_broadcast)(dev_t, int);
extern int imon_enabled;

#define IMON_EVENT(vp,cr,ev)   if (imon_enabled) { (*imon_event)(vp,cr,ev);  }
#define	IMON_CHECK(vp,dev,ino) if (imon_enabled) { (*imon_hook)(vp,dev,ino); }
#define IMON_BROADCAST(dev,ev) if (imon_enabled) { (*imon_broadcast)(dev,ev);}

#endif /* _KERNEL */

#ifdef __cplusplus
}
#endif

#endif /* _SYS_IMON_H */
