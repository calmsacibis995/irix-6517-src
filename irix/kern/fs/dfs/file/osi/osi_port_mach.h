/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/*
 * HISTORY
 * $Log: osi_port_mach.h,v $
 * Revision 65.8  1999/02/05 16:54:31  mek
 * Cleanup build warnings for IRIX kernel integration.
 *
 * Revision 65.7  1998/06/18 19:06:58  lmc
 * Adds a prototype for osi_afsGetTime() which is new.  osi_GetTime should
 * be used for filling in timeval structures.  osi_afsGetTime should be
 * used for filling in afsTimeval structures.
 *
 * Revision 65.6  1998/03/19  23:47:26  bdr
 * Misc 64 bit file changes and/or code cleanup for compiler warnings/remarks.
 *
 * Revision 65.5  1998/03/06  19:46:08  gwehrman
 * Defined osi_printf and osi_vprintf for the kernel.
 *
 * Revision 65.4  1998/03/04 21:11:31  lmc
 * Add #define for osi_GetTimeForSeconds as osi_GetTIme.  Add #define
 * for osi_Memset to be memset.  Add #define for OSI_VN_HOLD to be VN_HOLD.
 *
 * Revision 65.1  1997/10/24  14:29:51  jdoak
 * *** empty log message ***
 *
 * Revision 1.1  1997/10/21  15:59:44  jdoak
 * Initial revision
 *
 * Revision 64.2  1997/03/05  19:18:23  lord
 * Changed how we identify threads in the kernel so that kthreads can
 * enter dfs code without panicing.
 *
 * Revision 64.1  1997/02/14  19:46:51  dce
 * *** empty log message ***
 *
 * Revision 1.12  1996/06/07  17:56:09  bhim
 * Changed definition of osi_curproc to curprocp from u.u_procp.
 *
 * Revision 1.11  1996/05/10  17:25:47  bhim
 * Modified osi_EncodeDeviceNumber to deal in emajor/eminor numbers.
 *
 * Revision 1.10  1996/05/09  18:15:58  bhim
 * Modified parameters to vn_rdwr in osi_rdwr to pass ulimit in case of UIO_WRITE.
 *
 * Revision 1.9  1996/05/02  00:38:57  bhim
 * Removed macro defines for osi_Sleep, osi_Wakeup and osi_SleepInterruptably.
 *
 * Revision 1.8  1996/04/12  00:29:35  bhim
 * Changed type of osi_timeout_t to void.
 *
 * Revision 1.7  1996/04/10  18:09:24  bhim
 * Prototype problems with vprintf fixed.
 *
 * Revision 1.6  1996/04/10  17:26:11  bhim
 * Added uprintf defined as printf.
 *
 * Revision 1.5  1996/04/10  17:10:28  bhim
 * Fixed prototypes for printf/vprintf.
 *
 * Revision 1.4  1996/04/10  00:05:18  bhim
 * Added extra parameter for osi_unlockvp() special case of SGIMIPS.
 *
 * Revision 1.3  1996/04/07  00:39:30  bhim
 * Changed prototypes for bcmp, bzero, bcopy.
 *
 * Revision 1.2  1996/04/06  00:17:48  bhim
 * No Message Supplied
 *
 * Revision 1.1.119.3  1994/07/13  22:05:29  devsrc
 * 	merged with bl-10 
 * 	[1994/06/28  17:34:30  devsrc]
 *
 * 	Removed the #define of osi_caller to 0.  Substituted
 * 	an ansi function prototype.
 * 	[1994/04/06  16:20:07  mbs]
 *
 * 	Commented out the #define of osi_caller to 0.
 * 	[1994/04/04  17:52:48  mbs]
 *
 * Revision 1.1.119.2  1994/06/09  14:15:14  annie
 * 	fixed copyright in src/file
 * 	[1994/06/09  13:27:45  annie]
 * 
 * Revision 1.1.119.1  1994/02/04  20:24:14  devsrc
 * 	Merged from 1.0.3a to 1.1
 * 	[1994/02/04  15:15:31  devsrc]
 * 
 * Revision 1.1.117.1  1993/12/07  17:29:37  jaffe
 * 	1.0.3a update from Transarc
 * 	[1993/12/03  15:59:38  jaffe]
 * 
 * Revision 1.1.2.4  1993/01/21  14:50:10  cjd
 * 	embedded copyright notice
 * 	[1993/01/20  14:52:03  cjd]
 * 
 * Revision 1.1.2.3  1992/09/25  18:32:12  jaffe
 * 	Remove duplicate HEADER and LOG entries
 * 	[1992/09/25  12:27:44  jaffe]
 * 
 * Revision 1.1.2.2  1992/08/31  20:23:47  jaffe
 * 	Transarc delta: jaffe-ot4719-cleanup-gcc-Wall-in-osi 1.3
 * 	  Selected comments:
 * 	    Fixed many compiler warnings in the osi directory.
 * 	    Reworked ALL of the header files.  All files in the osi directory now
 * 	    have NO machine specific ifdefs.  All machine specific code is in the
 * 	    machine specific subdirectories.  To make this work, additional flags
 * 	    were added to the afs/param.h file so that we can tell if a particular
 * 	    platform has any additional changes for a given osi header file.
 * 	    moved from HP800/osi_port_hpux.h
 * 	    Corrected errors that appeared while trying to build everything on AIX3.2
 * 	    cleanup for OSF1 compilation
 * 	[1992/08/30  03:13:44  jaffe]
 * 
 * Revision 1.1.1.2  1992/08/30  03:13:44  jaffe
 * 	Transarc delta: jaffe-ot4719-cleanup-gcc-Wall-in-osi 1.3
 * 	  Selected comments:
 * 	    Fixed many compiler warnings in the osi directory.
 * 	    Reworked ALL of the header files.  All files in the osi directory now
 * 	    have NO machine specific ifdefs.  All machine specific code is in the
 * 	    machine specific subdirectories.  To make this work, additional flags
 * 	    were added to the afs/param.h file so that we can tell if a particular
 * 	    platform has any additional changes for a given osi header file.
 * 	    moved from HP800/osi_port_hpux.h
 * 	    Corrected errors that appeared while trying to build everything on AIX3.2
 * 	    cleanup for OSF1 compilation
 * 
 * $EndLog$
 */
/*
 *      Copyright (C) 1992 Transarc Corporation
 *      All rights reserved.
 */

/* SGIMIPS port information */

#ifndef TRANSARC_OSI_PORT_MACH_H
#define	TRANSARC_OSI_PORT_MACH_H

#include <dcedfs/stds.h>
#include <dcedfs/sysincludes.h>
#include <dcedfs/common_data.h>
#ifndef	_KERNEL
#include <sys/dirent.h>
#else
#include <sys/proc.h>
#include <os/proc/pproc_private.h>
#include <ksys/vproc.h>
#endif
#ifdef SGIMIPS
#include <sys/vfs.h>
#endif
#include <sys/statvfs.h>
#include <sys/flock.h>

typedef off_t 		osi_off_t;
#ifdef SGIMIPS
typedef void   		osi_timeout_t;
#else 
typedef int   		osi_timeout_t;
#endif
typedef struct fid 	osi_fid_t;
typedef struct osi_handle {
    struct fid fh_fid;	/* the only field we really need */
} osi_fhandle_t;
typedef struct buf osi_buf_t;
typedef struct statvfs osi_statfs_t;

#define ulong 		u_long
#define ushort 		u_short

#define osi_statfs_devnum(svfs) ((svfs)->f_fsid)
#define osi_statfs_vfstype(svfs) (0)

#ifndef	KERNEL
typedef struct dirent osi_direct_t;
#endif

#ifdef SGIMIPS
#define DEFINE_OSI_UERROR	/* REFERENCED */  int	osi_user_error = 0 
#else
#define DEFINE_OSI_UERROR
#endif

/*
 * define macros for encoding major/minor devices for DFS protocol
 *
 *	The DFS protocol encodes a device with major number J minor number N
 *	in two words.  deviceNumber contains the lower 16 bits of J in its
 *	upper 16 bits and the lower 16 bits of N in the lower 16 bits.  
 *	deviceNumberHighBits contains the upper 16 bits of J in its 
 *	upper 16 bits and the upper 16 bits of N in the lower 16 bits. 
 *
 *	osi_EncodeDeviceNumber(statP, dev) places dev into statP status structure
 *	osi_DecodeDeviceNumber(statP, devP) takes major/minor info from stat
 *		structure and puts it in '*devP'.  This returns 1 is the device
 *		is representable on the local architecture.
 *
 *      For SGIMIPS, major number is 14 bits and minor number is 18 bits if
 *      SVR4, and major number is 7 bits and minor number is 8 bits if SVR3.
 *      Although 14 bits are reserved for the major number, they are
 *      currently restricted to 9 bits. (see sys/sysmacros.h).
 */

#define osi_EncodeDeviceNumber(statP, dev)      \
MACRO_BEGIN \
    unsigned int _maj = (unsigned)(emajor(dev)); \
    unsigned int _min = (unsigned)(eminor(dev)); \
    (statP)->deviceNumber = (_maj << 16) | (_min & 0xffff); \
    (statP)->deviceNumberHighBits = (_min >> 16); \
MACRO_END

/* check for 18 bits of minor and 14 bits of major */
#define osi_DecodeDeviceNumber(statP, devP) \
    ((((statP)->deviceNumberHighBits & 0xfffffffc) || \
      ((statP)->deviceNumber & 0xc0000000)) ? ((*(devP) = 0), 0) : \
     ((*(devP) = makedev(((statP)->deviceNumber >> 16), \
                        ((statP)->deviceNumberHighBits << 16)|((statP)->deviceNumber & 0xffff))), 1))

#define	osi_hz			HZ
#define osi_sec			tv_sec
#define osi_subsec		tv_nsec	/* subsecond field of osi_timeval_t */
typedef struct timestruc osi_timeval_t;	/* the one with subsec as a field */
#ifdef SGIMIPS
#define osi_UTimeFromSub(a,b)	((a).sec = (unsigned32)(b).tv_sec, \
			        (a).usec = (unsigned32)(b).tv_nsec/1000)
#else  /* SGIMIPS */
#define osi_UTimeFromSub(a,b)	((a).sec = (b).tv_sec, (a).usec = (b).tv_nsec/1000)
#endif /* SGIMIPS */
#define osi_SubFromUTime(a,b)	((a).tv_sec = (b).sec, (a).tv_nsec = (b).usec*1000)
#define osi_TvalFromSub(a,b)	((a).tv_sec = (b).tv_sec, (a).tv_usec = (b).tv_nsec/1000)
#define osi_SubFromTval(a,b) 	((a).tv_sec = (b).tv_sec, (a).tv_nsec = (b).tv_usec*1000)
#define osi_ToUTime(a)		((a).tv_nsec/1000)
#define osi_ToSubTime(a)	((a).usec*1000)
#define osi_SubUnit		1000000000
#define osi_SubIncr		1	/* min increment for version # */

#define osi_GetTimeForSeconds(x) osi_GetTime(x)
#define osi_Memset(ptr, val, len)       memset((ptr), (val), (len))

/*
 * BSD-style timeout()
 */
#define	OSI_BSD_TIMEOUT		0

/*
 * Traditional single-threaded sleep/wakeup synchronization
 */
#define OSI_SINGLE_THREADED	0

#define OSI_MAXNAMLEN		MAXNAMLEN
#define OSI_MAXHOSTNAMELEN	MAXHOSTNAMELEN

/* IO_APPEND and similar flags */
#define OSI_FAPPEND	IO_APPEND
#if 0
#define OSI_FEXEC	0	/* disables it */
#endif
#define OSI_FSYNC	IO_SYNC

/*
 * Macros that deal with vfs/vnode fields
 */

/* Leave it empty until we have some locks to init. */
#define osi_vInitLocks(vc)

#define osi_vfs			vfs
#define osi_vfs_op		vfs_fbhv->bd_ops
#define osi_vSetVfsp(vc,vfsp)	(vc)->v_vfsp = (vfsp)
#define osi_vnodeops		vnodeops	/* type for v_op */
#define osi_ufsVnodeops		ufs_vnodeops	/* ufs's vnode operations */
#define osi_vfsops		vfsops
#define	osi_vType(vc)		(vc)->v_type
#define	osi_vSetType(vc,type)   (vc)->v_type = (type)
#define	osi_IsAfsVnode(vc)	((vc)->v_fops == (struct osi_vnodeops *) xvfs_ops)
#if 0
#define	osi_SetAfsVnode(vc)	(vc)->v_op = xvfs_ops
#endif
#define osi_SetVnodeOps(vc,op)	((struct vnode *)(vc))->v_fops = (struct osi_vnodeops *)(op)

#define OSI_VP_TO_VFSP(VP)	(VP)->v_vfsp
#define OSI_IS_MOUNTED_ON(VP)	((VP)->v_vfsmountedhere != (struct osi_vfs *) 0)
#define OSI_ISFIFO(VP)		((VP)->v_type == VFIFO)
#define OSI_ISVDEV(VP)		ISVDEV(VP)
#define OSI_ISDIR(VP)		((VP)->v_type == VDIR)
#define OSI_ISREG(VP)		((VP)->v_type == VREG)
#define OSI_ISLNK(VP)		((VP)->v_type == VLNK)
#define OSI_VN_RELE(VP)		VN_RELE(VP)
#ifdef SGIMIPS
#define OSI_VN_HOLD(VP)		VN_HOLD(VP)
#endif /* SGIMIPS */

/*
 * Locking for R/W vnode members (v_count, v_flags)
 * spinlock manipulation.
 */
#define osi_lockvp(vp)		VN_LOCK(vp)
#define osi_unlockvp(vp, lck)	VN_UNLOCK(vp, lck)

/*
 * dnlc interfaces
 */
#ifdef _KERNEL
#define osi_dnlc_enter(dvp, fname, vp)	dnlc_enter(dvp, fname, vp, NULL)
#define osi_dnlc_lookup(dvp, fname)	dnlc_lookup(dvp, fname, NULL)
#define osi_dnlc_fs_purge(vop, level)	dnlc_purge()
#define osi_dnlc_fs_purge1(vop)		dnlc_purge1()
#define osi_dnlc_purge_vp(vp)		dnlc_purge_vp(vp)
#define osi_dnlc_purge_vfsp(vfs, count)	dnlc_purge(vfs, count)
#define osi_dnlc_remove(dvp, fname)	dnlc_remove(dvp, fname)
#else
#define osi_dnlc_enter(dvp, fname, vp)
#define osi_dnlc_lookup(dvp, fname)	((struct vnode *)0)
#define osi_dnlc_fs_purge1(vop)		0
#define osi_dnlc_fs_purge(vop, level)   0
#define osi_dnlc_purge_vp(vp)
#define osi_dnlc_purge_vfsp(vfs, count)	0
#define osi_dnlc_remove(dvp, fname)
#endif

/* Define parameters that gives this platform's highest file address: make ours
 * 0x7ffffffffffffffe */

#define OSI_MAX_FILE_PARM_CLIENT        OSI_MAX_FILE_LIMIT(0x3f,01)
#define OSI_MAX_FILE_PARM_SERVER        OSI_MAX_FILE_LIMIT(0x3f,01)


/*
 * Generic calls to vnode kernel routines
 */
#define OSI_VN_INIT(VP, VFSP, TYPE, DEV, EVN)	{ \
	(VP)->v_flag = 0; \
	(VP)->v_count = 1; \
	osi_vInitLocks(VP); \
	osi_vSetVfsp(VP, (VFSP)); \
	osi_vSetType(VP, (TYPE)); \
	(VP)->v_rdev = (DEV); \
}

#define	osi_lookupname(pathname, segment, followlink, parentvp, compvp) \
    lookupname(pathname, segment, followlink, parentvp, compvp, NULL)

extern flid_t *sys_flid;

/*  Because this is used for writing the cache file, I think it is
	okay to always pass RLIM_INFINITY for the file offset limit. */
#define osi_rdwr(rw,vp,base,len,offset,segflg,unit,resid) \
	 vn_rdwr(rw,vp,base,len,offset,segflg,unit,RLIM_INFINITY,get_current_cred(),resid,sys_flid)

#ifdef _KERNEL
#define osi_vattr_null(vap)		bzero(vap, sizeof(vattr_t))
#endif
#define osi_vattr_init(vap, mask)	((vap)->va_mask = mask)	
#define osi_vattr_add(vap, mask)	((vap)->va_mask |= mask)
#define osi_vattr_sub(vap, mask)	((vap)->va_mask &= ~(mask))

/*
 * SGIMIPS : symbols defined from vnode.h
 */
#define	OSI_VA_TYPE		AT_TYPE
#define OSI_VA_MODE		AT_MODE
#define OSI_VA_UID		AT_UID
#define OSI_VA_GID		AT_GID
#define OSI_VA_FSID		AT_FSID
#define OSI_VA_NODEID		AT_NODEID
#define OSI_VA_NLINK		AT_NLINK
#define OSI_VA_SIZE		AT_SIZE
#define OSI_VA_ATIME		AT_ATIME
#define OSI_VA_MTIME		AT_MTIME
#define OSI_VA_CTIME		AT_CTIME
#define OSI_VA_RDEV		AT_RDEV
#define OSI_VA_BLKSIZE		AT_BLKSIZE
#define OSI_VA_NBLOCKS		AT_NBLOCKS
#define OSI_VA_VCODE		AT_VCODE
#define OSI_VA_MAC		AT_MAC
#define OSI_VA_UPDATIME		AT_UPDATIME
#define OSI_VA_UPDMTIME		AT_UPDMTIME
#define OSI_VA_UPDCTIME		AT_UPDCTIME
#define OSI_VA_ACL		AT_ACL
#define OSI_VA_CAP		AT_CAP
#define OSI_VA_INF		AT_INF
#define OSI_VA_XFLAGS		AT_FLAGS
#define OSI_VA_EXTSIZE		AT_EXTSIZE
#define OSI_VA_NEXTENTS		AT_NEXTENTS
#define OSI_VA_ANEXTENTS	AT_ANEXTENTS

#define	OSI_VA_ALL		AT_ALL
#define OSI_VA_STAT		AT_STAT
#define OSI_VA_TIMES		AT_TIMES
#define OSI_VA_UPDTIMES		AT_UPDTIMES
#define OSI_VA_NOSET		AT_NOSET

#define osi_setting_mode(vap)		((vap)->va_mask & OSI_VA_MODE)
#define osi_set_mode_flag(vap)		osi_vattr_add(vap, OSI_VA_MODE)
#define osi_clear_mode_flag(vap)	osi_vattr_sub(vap, OSI_VA_MODE)

#define osi_setting_uid(vap)		((vap)->va_mask & OSI_VA_UID)
#define osi_set_uid_flag(vap)		osi_vattr_add(vap, OSI_VA_UID)
#define osi_clear_uid_flag(vap)		osi_vattr_sub(vap, OSI_VA_UID)

#define osi_setting_gid(vap)		((vap)->va_mask & OSI_VA_GID)
#define osi_set_gid_flag(vap)		osi_vattr_add(vap, OSI_VA_GID)
#define osi_clear_gid_flag(vap)		osi_vattr_sub(vap, OSI_VA_GID)

#define osi_setting_uid_or_gid(vap) 	((vap)->va_mask & (OSI_VA_UID | OSI_VA_GID))

#define osi_setting_size(vap)		((vap)->va_mask & OSI_VA_SIZE)
#define osi_set_size_flag(vap)		osi_vattr_add(vap, OSI_VA_SIZE)
#define osi_clear_size_flag(vap)	osi_vattr_sub(vap, OSI_VA_SIZE)

#define osi_setting_atime(vap)		((vap)->va_mask & OSI_VA_ATIME)
#define osi_set_atime_flag(vap)		osi_vattr_add(vap, OSI_VA_ATIME)
#define osi_clear_atime_flag(vap)	osi_vattr_sub(vap, OSI_VA_ATIME)

#define osi_setting_mtime(vap)		((vap)->va_mask & OSI_VA_MTIME)
#define osi_set_mtime_flag(vap)		osi_vattr_add(vap, OSI_VA_MTIME)
#define osi_clear_mtime_flag(vap)	osi_vattr_sub(vap, OSI_VA_MTIME)

#if 0
#define osi_setting_times_now(vap)	((vap)->va_mtime.tv_sec == 0 && \
					 (vap)->va_mtime.tv_usec == -1)
#define osi_set_times_now_flag(vap)
#define osi_clear_times_now_flag(vap)
#else 
/* cannot capture the full semantics of the above definition for SGIMIPS */

#define osi_setting_times_now(vap)	osi_setting_mtime(vap)
#define osi_set_times_now_flag(vap)	osi_set_mtime_flag(vap)
#define osi_clear_times_now_flag(vap)	osi_clear_mtime_flag(vap)
#endif

#define osi_setting_ctime(vap)		((vap)->va_mask & AT_CTIME)
#define osi_set_ctime_flag(vap)		osi_vattr_add(vap, OSI_VA_CTIME)
#define osi_clear_ctime_flag(vap)	osi_vattr_sub(vap, OSI_VA_CTIME)

#define osi_setting_link(vap)		((vap)->va_mask & AT_NLINK)
#define osi_set_link_flag(vap)		osi_vattr_add(vap, OSI_VA_NLINK)
#define osi_clear_link_flag(vap)	osi_vattr_sub(vap, OSI_VA_NLINK)

#ifdef _KERNEL
/* defined in osi_port_os.c */
extern int osi_GetMachineName(char *buf, int len);

#ifdef SGIMIPS
#define osi_setuerror(e)	osi_user_error = e 
#define osi_getuerror() 	osi_user_error
#else
#define osi_setuerror(e) u.u_error = e
#define osi_getuerror() u.u_error
#endif

/*
 * Default macros for sleep/wakeup calls on osi_WaitHandle structures
 * (these just take pointers, not wait handles -ota 900316)
 */
#ifdef SGIMIPS
extern int osi_Sleep(opaque);
extern int osi_SleepInterruptably(opaque);
extern int osi_Wakeup(opaque);
#else
#define	osi_Sleep(x)		sleep((caddr_t) (x), PZERO - 2)
#define osi_SleepInterruptably(x) sleep((caddr_t) (x), PCATCH | (PZERO + 1))
#define	osi_Wakeup(x)		wakeup((caddr_t) (x))
#endif

/*
 * Macros for suspending execution for a given number of seconds
 */
#define osi_Pause(seconds)	\
  (void) osi_Wait(((seconds) * 1000), \
	(struct osi_WaitHandle *)NULL, 0 /* =>no interrupts */)

/*
 * Generic process stuff
 */
#define osi_curproc()		(curprocp)
#define osi_ThreadUnique()	((long)curthreadp)
#define osi_GetPid()		current_pid()
#define osi_ThreadID()	  	osi_GetPid()

#define osi_copyin(src, dst, len)		copyin(src, dst, len)
#define osi_copyinstr(src, dst, len, copied) 	copyinstr(src, dst, len, copied)
#define osi_copyout(src, dst, len)		copyout(src, dst, len)

/*
 * OSI malloc/free definitions
 */
#define	osi_kalloc_r(size)	kern_malloc((size_t) size)
#define	osi_kfree_r(ptr,size)	kern_free(ptr)


/*
 * osi_GetTime (Get system time) definitions
 */

void osi_GetTime (struct timeval *tv);
void osi_afsGetTime (struct afsTimeval *tv);

/*
 * #include <sys/pstat.h>
 */

extern void osi_RestorePreemption(int val);
extern int osi_PreemptionOff(void);

#define osi_suser(cr)	_CAP_CRABLE(cr, CAP_MOUNT_MGT)
#else /* !_KERNEL */
/*
 * osi_GetTime (Get system time) definitions
 */
#define osi_GetTime(x)		gettimeofday(x, NULL)
#define	osi_Time()		time((time_t *)0)
#define osi_gettimeofday(tp,tzp) gettimeofday((tp),(tzp))

/* some architectures don't support wait3 */

#define osi_wait3(st, opt, ru) 	wait3((int *)(st), opt, (struct rusage *)(ru))

#define osi_GetMaxNumberFiles() getdtablesize()

#define osi_suser(cr)	crsuser(cr)
#endif /* _KERNEL */


#ifndef _KERNEL
#define osi_ExclusiveLockNoBlock(fid)	lockf((fid), F_TLOCK, 0)
#define osi_UnLock(fid)			lockf((fid), F_ULOCK, 0)
#endif /* !_KERNEL */

/* defined in osi_fio.c */
extern int osi_vptofid(struct vnode *vp, osi_fid_t *fidp);

#define osi_caller() ((caddr_t) (inst_t *)__return_address)

#define osi_MakeInitChild()

/*
 * This macro uses symbols defined in <dce/ker/pthread.h>.
 */
#define osi_ThreadCreate(start_routine,args,blockPreemption,restorePreemption,threadname,code) \
	{ pthread_t new_thread; \
	code = pthread_create(&new_thread, pthread_attr_default, \
			      (pthread_startroutine_t)start_routine, args); }

/* uprintf */
#define uprintf	printf

/*
 * make a declaration of these functions available at any place that might use
 * the above macros
 */
extern int bcmp _TAKES((const void *, const void *, size_t));
extern void bcopy _TAKES((const void *, void *, size_t));
extern void bzero _TAKES((void *, size_t));

#ifndef KERNEL                          /* some things don't exist in kernel */
extern void *malloc _TAKES((size_t size));
extern void abort _TAKES(( void ));
extern void exit _TAKES(( int status ));
#endif

/* Required in osi_misc.c */
extern toid_t timeout _TAKES((void (*fun)(), void *arg, long tim, ...));
extern int untimeout  _TAKES((toid_t id));
extern void *kern_malloc _TAKES((size_t));
extern void kern_free _TAKES((void *));
extern int copyout _TAKES((void *, void *, int));
extern int copyin _TAKES((void *, void *, int));
extern int vprintf  _TAKES((const char *, char *));
extern void panic _TAKES((char *fmt, ...));
extern void settime _TAKES((long, long));
extern void microtime _TAKES((struct timeval *));

#ifndef KERNEL
extern int printf _TAKES((const char *, ...));
#else
#define osi_printf	printf
#define osi_vprintf	vprintf
 /* To match declaration in sys/systm.h */
extern void printf _TAKES((char *, ...));
#endif /* KERNEL */

/* Required in osi_net.c */
#ifdef KERNEL
extern void wakeup _TAKES((void *));
extern int sleep _TAKES((void *, int));
#endif

#endif /* TRANSARC_OSI_PORT_MACH_H */
