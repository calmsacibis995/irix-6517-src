/*************************************************************************
*                                                                        *
*               Copyright (C) 1992,1994 Silicon Graphics, Inc.       	 *
*                                                                        *
*  These coded instructions, statements, and computer programs  contain  *
*  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
*  are protected by Federal copyright law.  They  may  not be disclosed  *
*  to  third  parties  or copied or duplicated in any form, in whole or  *
*  in part, without the prior written consent of Silicon Graphics, Inc.  *
*                                                                        *
**************************************************************************/
#ident  "$Revision: 1.14 $ $Author: wombat $"

#ifndef __KAIO_H__
#define __KAIO_H__

#include <standards.h>
#include <sys/types.h>
#include <sys/buf.h>
#include <sys/signal.h>
#include <sys/timers.h>
#include <sys/fcntl.h>
#include <sys/uio.h>
#include <sys/kabi.h>
#include <sys/poll.h>

/*
 * aio - POSIX 1003.1b-1993
 */

#ifdef _KERNEL
/* Clean-up tasks to be done when kaiocb structs are re-used */
#define KAIO_IOUNMAP		0x001
#define KAIO_UNDMA		0x002
#define KAIO_NOTALIGNED		0x004
#define KAIO_UNMAPUTOKV		0x008
#define KAIO_PUTPHYSBUF		0x010
#define KAIO_UNLOCKBUF		0x020
#define KAIO_UNLOCKAIOCB	0x040
#define KAIO_DO_NOT_UNALLOC	0x080
#define KAIO_UNPIN		0x100
#define KAIO_UNLOCKUTBL		0x200

#ifndef KAIOUSER_1_1	/* July 1998 */
#define KAIOUSER_1_1	3
#endif

/* Problem: the kernel needs to write to the actual user-space aiocb.
 * We can't really use the standard COPYIN_XLATE stuff because it would create
 * a copy for the kernel to write into, and the user wouldn't see those writes
 * until much later.
 * So, the kernel will need a way to access the relevant fields in each ABI
 * flavor of the struct, and all kernel-related access (nobytes, errno, etc.) will
 * need to go through that mechanism, with casts for the proper size.
 * Bleah.
 * Do this via macros, to hide the sliminess.
 * NEVER DIRECTLY READ OR WRITE STRUCTURE ELEMENTS, always use an access macro.
 * THIS MUST STAY IN SYNCH WITH /usr/include/aio.h.
 */

typedef union useraio {
     /* good for off32_abi32 and off64_abi32 */
     struct useraio_abi32 {
	  __int32_t     aio_fildes; /* file descriptor to perform aio on */
	  __int32_t     aio_buf; /* Data buffer */
	  __uint32_t    aio_nbytes; /* number of bytes of data */
	  __uint32_t    aio_offset; /* file offset position for off32_abi32 */
	  __int32_t     aio_reqprio; /* aio priority, (Currently must be 0) */
	  struct /* sigevent */ {
	       __int32_t	  sigev_notify;
	       /*    union notifyinfo */
	       __int32_t     nifunc;	/* callback data */
	       /*    notifyinfo_t */
	       /*    union sigval */
	       __int32_t	  sival_ptr;
	       /*    sigval_t; */
	       __uint32_t	  sigev_reserved[13];
	       __uint32_t	  sigev_pad[6];
	  } aio_sigevent;
	  __int32_t     aio_lio_opcode; /* opcode for lio_listio() call */
	  __uint32_t    aio_reserved[7]; /* reserved for internal use */
	  __uint32_t    aio_pad[6];
	  __uint64_t	  aio_offset64;	/* file offset position for off64_abi32, ignore for off32_abi32 */
     } ua_abi32;

     /* N32 ABI - differs only in size of aio_offset */
     struct useraio_abin32 {
	  __int32_t     aio_fildes; /* file descriptor to perform aio on */
	  __int32_t     aio_buf; /* Data buffer */
	  __uint32_t    aio_nbytes; /* number of bytes of data */
	  __uint64_t    aio_offset; /* file offset position - 64bit in N32 */
	  __int32_t     aio_reqprio; /* aio priority, (Currently must be 0) */
	  struct /* sigevent_t */ {
	       __int32_t	  sigev_notify;
	       /*    union notifyinfo */
	       __int32_t     nifunc;	/* callback data */
	       /*    notifyinfo_t */
	       /*    union sigval */
	       __int32_t	  sival_ptr;
	       /*    sigval_t; */
	       __uint32_t	  sigev_reserved[13];
	       __uint32_t	  sigev_pad[6];
	  } aio_sigevent;
	  __int32_t     aio_lio_opcode; /* opcode for lio_listio() call */
	  __uint32_t    aio_reserved[7]; /* reserved for internal use */
	  __uint32_t    aio_pad[6];
     } ua_abin32;

     /* All pointers are 64-bit, plus offset is 64-bit */
     struct useraio_abi64 {
	  __int32_t     aio_fildes; /* file descriptor to perform aio on */
	  __int64_t     aio_buf; /* Data buffer */
	  __uint64_t    aio_nbytes; /* number of bytes of data */
	  __uint64_t    aio_offset; /* file offset position */
	  __int32_t     aio_reqprio; /* aio priority, (Currently must be 0) */
	  struct /* sigevent_t */ {
	       __int32_t	  sigev_notify;
	       /*    union notifyinfo */
	       __int64_t     nifunc;	/* callback data */
	       /*    notifyinfo_t */
	       /*    union sigval */
	       __int64_t	  sival_ptr;
	       /*    sigval_t; */
	       __uint64_t	  sigev_reserved[13];
	       __uint64_t	  sigev_pad[6];
	  } aio_sigevent;
	  __int32_t     aio_lio_opcode; /* opcode for lio_listio() call */
	  __uint64_t    aio_reserved[7]; /* reserved for internal use */
	  __uint64_t    aio_pad[6];
     } ua_abi64;
} useraio_t;

/* 
 * The user struct aiocb has reserved space for internal (libc) use.
 * Here we map fields from the reserved space to names that have meaning in 
 * our library version (from irix/lib/libc/src/aio/local.h). MUST STAY IN
 * SYNCH WITH LIBC!
 */
#define aio_nobytes aio_reserved[0]	/* return bytes */
#define aio_errno aio_reserved[1]	/* return error from this aio op */
#define aio_ret aio_reserved[2]		/* returned status */
#define aio_op aio_reserved[3]		/* What operation is this doing? */
#define aio_waithelp aio_reserved[4]	/* semaphore struct-aio_fsync,lio_listio */
#define aio_64bit aio_reserved[5]       /* used to decide where the offset lives */
#define aio_kaio aio_reserved[6]	/* used by kernel async i/o */

/* Do not put () around member; compiler does not like that */
#define GET_KUAIOCB_MEMBER(ku,abi,member,dest,dcast,scast) \
     if (ABI_IS_IRIX5(abi)) { \
	  (dest) = (dcast)(scast)(ku)->ua_abi32.member; \
     } else if (ABI_IS_IRIX5_64(abi)) { \
	  (dest) = (dcast)(scast)(ku)->ua_abi64.member; \
     } else /* n32 */ { \
	  (dest) = (dcast)(scast)(ku)->ua_abin32.member; \
     }

#define SET_KUAIOCB_MEMBER(ku,abi,member,src32,src64) \
     if (ABI_IS_IRIX5(abi)) { \
	  (ku)->ua_abi32.member = (src32); \
     } else if (ABI_IS_IRIX5_64(abi)) { \
	  (ku)->ua_abi64.member = (src64); \
     } else /* n32 */ { \
	  (ku)->ua_abin32.member = (src32); \
     }

/* Get a field from the user-space AIOCB -- aio_offset is special */
#define GET_KUAIO_OFFSET(ku,abi,dest) \
     if (ABI_IS_IRIX5(abi)) { \
	  if (((ku)->ua_abi32.aio_offset == -1) \
	      && (ku)->ua_abi32.aio_64bit == 0xdeadbeef) \
	       (dest) = (ku)->ua_abi32.aio_offset64; \
	  else \
	       (dest) = (long)(ku)->ua_abi32.aio_offset; \
     } else if (ABI_IS_IRIX5_64(abi)) { \
	  (dest) = (ku)->ua_abi64.aio_offset; \
     } else /* n32 */ { \
	  (dest) = (ku)->ua_abin32.aio_offset; \
     }

/* Get a field from the aio_reserved area of a user-space AIOCB - also special */
#define GET_KUAIOCB_RSVMEMBER(ku,abi,member,dest,dcast) \
     if (ABI_IS_IRIX5(abi)) { \
	   (dest) = (dcast)(int32_t)(ku)->ua_abi32.member; \
     } else if (ABI_IS_IRIX5_64(abi)) { \
	  (dest) = (dcast)(int64_t)(ku)->ua_abi64.member; \
     } else /* n32 */ { \
	  (dest) = (dcast)(int32_t)(ku)->ua_abin32.member; \
     }

#define GET_KUAIO_FILDES(ku,abi,d) \
	GET_KUAIOCB_MEMBER((ku),(abi),aio_fildes,(d),int,int)
#define GET_KUAIO_BUF(ku,abi,d) \
	GET_KUAIOCB_MEMBER((ku),(abi),aio_buf,(d),caddr_t,__psint_t)
#define GET_KUAIO_NBYTES(ku,abi,d) \
	GET_KUAIOCB_MEMBER((ku),(abi),aio_nbytes,(d),int,int)
#define GET_KUAIO_SELBYTE(ku,abi,d) \
	GET_KUAIOCB_RSVMEMBER((ku),(abi),aio_kaio,(d),long)

/* Changing values in the user AIOCB makes sense only for a few fields */
#define SET_KUAIO_ERRNO(ku,abi,s) \
	SET_KUAIOCB_MEMBER((ku),(abi),aio_errno,(s),(s))
/* Library code messes with aio_ret, and kaio should not */
#define SET_KUAIO_RET(ku,abi,s32,s64)
#define SET_KUAIO_NOBYTES(ku,abi,s32,s64) \
	SET_KUAIOCB_MEMBER((ku),(abi),aio_nobytes,(s32),(s64))
#define SET_KUAIO_SELBYTE(ku,abi,d) \
	SET_KUAIOCB_MEMBER((ku),(abi),aio_kaio,(d),(d))

typedef struct kaiocb {
     struct kaiocb *aio_next_free;
     struct kaiocbhd *aio_hd;
     struct vfile *aio_fp;	/* file pointer to perform aio on */
     int	aio_tasks;	/* clean-up tasks to perform when re-allocated */
     int	aio_stat;
     int	aio_ioflag;	/* save area of bp flags */
     useraio_t	*aio_kuaiocb;	/* kv address for user aiocb */
     useraio_t	*aio_uaiocb;	/* for future unlock? in what proc context? */
     pid_t	aio_proc;	/* process that requested this i/o */
     int	aio_p_abi;	/* Abi of process 'abi_proc' */
     struct uio	aio_uio;	/* for the real uio */
     struct iovec aio_iov;	/* for the real iov */
     off_t	aio_offset;	/* for debugging */
     struct vnode *aio_vp;	/* for debugging */
     opaque_t	aio_undma_cookie;	/* state saved by fast_undma */
     opaque_t	aio_uiocb_cookie;	/* state saved by fast_useracc */
     char	*aio_acompp;	/* copy of (kaiocbhd)->acomplete */
     char	*aio_awaitp;	/* copy of (kaiocbhd)->awaiting */
     int	aio_maxnaio;	/* copy of (kaiocbhd)->maxaiosz */
     struct pollhead	*aio_ph;	/* ptr to (kaiocbhd)->ph */
     lock_t	*aio_susplock;	/* pointer to (kaiocbhd)->susplock */
} kaiocb_t;

/* defines for aio_stat */
#define	AIOCB_FREE		1
#define	AIOCB_INPROGRESS	2
#define	AIOCB_DONE		3

#define KAIO_QFREE	0
#define KAIO_QCLEANUP	1
#define KAIO_NUMQ	2
struct kaio_susp {
	char *kaddr;		/* user-space table for marking launch/done */
	char *uaddr;		/* Save user-space addr, for unpin */
	int cookie;		/* to pin/unpin comp/wait tables */
	int tasks;		/* Things to do on exit */
};
struct kaiocbhd {
     lock_t	aiohd_lock;
     struct kaiocb *aio_hdfree[KAIO_NUMQ]; /* lists of free kaiocbs allocated to process */
     int	num_aio;		/* number of aio allocated */

/*BEGIN NOTUSED*/
     char	*user_aio_resp;		/* response areas in user space */
     struct kaiocb_resp *aio_resp;	/* response areas mapped to kernel address */ 
     int	num_aio_resp;		/* size of response area */
/*END NOTUSED*/
     int	max_num_aio;		/* max allowable number of aios (not used) */
     int	num_aio_inprogress;	/* number of aio in progress */
     int	proc_stat;		/* defines waitsema purpose */
     sema_t	aio_waitsema;		/* wait for all in-use to finish (exit) */
     struct kaio_susp susp[2];		/* user-space tbls to mark done/launch */
#define KAIO_COMPL	0
#define KAIO_WAIT	1
#define KAIO_MAXUTABLES	2
     int	maxnaio;		/* size of each of those 2 tables */
     sema_t	suspense;		/* NOT USED */
     struct pollhead ph;		/* more for select */
     pid_t	khpid;
     /* END OF STUFF PRECOMPILED IN IRIX 6.5 MR */
     lock_t	susplock;	/* lock manipulation of compl/wait bits */
};

#define	AIOHD_WAITIO		1

#endif /* _KERNEL */

#ifdef _KERNEL

#include "sys/dbacc.h"

extern void kaio_done(struct buf *); /* interrupt-level completion */
extern void kaio_exit(void);		/* wait for and clean up outstanding i/o */
extern int kaio_rw(useraio_t *, int);
extern int kaio_user_init(void *, long);
extern void kaio_stats(dba_stat_t *);
extern void kaio_clearstats(void);
extern int kaio_suspend(int *, int);
extern int kaio_installed(void);
extern void kaio_getparams(struct dba_conf *);

#endif /* _KERNEL */

#endif /* __KAIO_H__ */
