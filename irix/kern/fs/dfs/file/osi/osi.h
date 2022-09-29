/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/*
 *      Copyright (C) 1996, 1990 Transarc Corporation
 *      All rights reserved.
 */
/* $Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/osi/RCS/osi.h,v 65.8 1999/08/24 20:05:41 gwehrman Exp $ */
/*
 * osi.h -- defined all basic operating system independent facilities used by
 *     DFS and Episode.
 */
#ifndef TRANSARC_OSI_H
#define TRANSARC_OSI_H
#ifndef SGIMIPS
#define SGIMIPS
#endif /* !SGIMIPS */

#include <dcedfs/param.h>		/* Should be always first */
#include <dcedfs/stds.h>
#ifdef SGIMIPS
#include <dcedfs/sysincludes.h>
#include <dcedfs/debug.h>
#include <netinet/in.h>
#else
#include <dcedfs/debug.h>
#include <dcedfs/sysincludes.h>
#endif
#include <dcedfs/osi_port.h>
#include <dcedfs/osi_alloc.h>
#include <dcedfs/osi_cred.h>

#ifdef OSI_CALLTRACE
/*
 * Include support for function call tracing.
 */
#include <dcedfs/osi_calltrace.h>
#endif

/*
 * osi_File related macros
 */
#define	osi_SetFileProc(x, p)	((x)->proc = (p))
#define	osi_SetFileRock(x, r)	((x)->rock = (r))
#define	osi_GetFileProc(x)	((x)->proc)
#define	osi_GetFileRock(x)	((x)->rock)
#define	osi_Seek(x, p)		((x)->offset = (p))

struct osi_dev {	/* XXX obsolete */
    long dev;
};

/* Define a parameter that gives this platform's highest file address.  This
 * value will be returned as the value for AFS_CONN_PARAM_MAXFILE_CLIENT or
 * AFS_CONN_PARAM_MAXFILE_SERVER as appropriate.
 *
 * The parameter value stores two exponents that are used to determine the max
 * file size.  If 'a' is the lowest-order byte and 'b' is the
 * second-to-lowest-order byte in the 4-byte parameter value (top two bytes are
 * unused), the max file size is (2^a - 2^b).
 */
#define OSI_MAX_FILE_LIMIT(hiend, loend) \
    ((((loend) & 0xff)<<8) | ((hiend) & 0xff) | AFS_CONN_PARAM_SUPPORTS_64BITS)
#define OSI_MAX_FILE_GET_HIEND(val) ((val) & 0xff)
#define OSI_MAX_FILE_GET_LOEND(val) (((val) >> 8) & 0xff)

extern afs_hyper_t osi_maxFileSizeDefault; /* size if no response to queries */
extern afs_hyper_t osi_minMaxFileSizeSupported; /* smallest legal maxSize */

/* These are initialized in osi_InitGeneric() */
extern afs_hyper_t osi_maxFileClient;	/* from OSI_MAX_FILE_PARM_CLIENT */
extern afs_hyper_t osi_maxFileServer;	/* from OSI_MAX_FILE_PARM_SERVER */

extern afs_hyper_t osi_hZero;		/* 0,,0 */
extern afs_hyper_t osi_hMaxint32;	/* 2^31-1 */
extern afs_hyper_t osi_hMaxint64;	/* 2^63-1 */

extern afs_hyper_t osi_MaxFileParmToHyper(unsigned value);
extern void osi_InitGeneric(void);

/*
 * Fast buffer allocator
 */
struct osi_buffer {
    struct osi_buffer *next;
};

#define	osi_BUFFERSIZE	8192	/* Default buffer size */
#define	osi_MINBUFSIZE	256	/* Min buffer size */

#if defined(_KERNEL) && defined(SGIMIPS)
struct osi_WaitHandle {
    sv_t sv;
    lock_t mutex;
    caddr_t proc;
};
#else
struct osi_WaitHandle {
    caddr_t proc;			/* process waiting */
};
#endif

#if defined(_KERNEL)
/* This code happens to work on our current reference platforms: */
extern char *panicstr;
#define osi_IsKernelPanic() (panicstr != 0)
#else /* user mode */
#define osi_IsKernelPanic() 0	/* never true in user mode */
#endif /* defined(KERNEL) */

/*
 * In SunOS, we define a "preemption lock" to protect ourselves against
 *  multi-processor safety problems in DFS.  It is at a lower level than
 * all our other locks and has to be dropped any time we wait for some
 * other lock.  All of the lock_* functions know about the preemption lock.
 * But the osi_mutex_, osi_cv_, and osi_rwlock_ functions do not.  Code
 * that uses those functions directly must explicitly drop the preemption
 * lock.  These macros are for entry to, and exit from, such code.  Some
 * examples can be found in the icl code and in the implementation of
 * osi_Alloc and osi_Free.
 *
 * XXX Eventually, we should rid ourselves of the preemption lock.
 * XXX The names of these interfaces could be much improved.
 */
#if defined(AFS_SUNOS5_ENV) && defined(KERNEL)

/* This only applies to the SUNOS5 kernel (at present). */

#define osi_MakePreemptionRight()	osi_RestorePreemption(0)
#define osi_UnmakePreemptionRight()	((void) osi_PreemptionOff())

extern int osi_Wait_r(long ams, struct osi_WaitHandle *ahandle, int aintok);

#else
/* For non-preemption-safe kernels (e.g. AIX) or where preemption is normally
 * on (e.g. user space). */

#define osi_MakePreemptionRight()
#define osi_UnmakePreemptionRight()

/* These are miscellaneous "_r" macros that are mapped straight across */

#define osi_AllocPinned_r(asize) osi_AllocPinned(asize)
#define osi_FreePinned_r(ptr, asize) osi_FreePinned(ptr, asize)
#define osi_lockvp_r(vp) osi_lockvp(vp)
#define osi_unlockvp_r(vp) osi_unlockvp(vp)

#define osi_copyin_r(src, dst, len) osi_copyin(src, dst, len)
#define osi_copyinstr_r(src, dst, len, copied) \
    osi_copyinstr(src, dst, len, copied)
#define osi_copyout_r(src, dst, len) osi_copyout(src, dst, len)

#define osi_dnlc_enter_r(dvp, fname, vp) osi_dnlc_enter(dvp, fname, vp)
#define osi_dnlc_lookup_r(dvp, fname) osi_dnlc_lookup(dvp, fname)
#define osi_dnlc_fs_purge1_r(vop) osi_dnlc_fs_purge1(vop)
#define osi_dnlc_fs_purge_r(vop, level) osi_dnlc_fs_purge(vop, level)
#define osi_dnlc_iter_purge_r(func, arg) osi_dnlc_iter_purge(func, arg)
#define osi_dnlc_purge_vp_r(vp) osi_dnlc_purge_vp(vp)
#define osi_dnlc_purge_vfsp_r(vfs, count) osi_dnlc_purge_vfsp(vfs, count)
#define osi_dnlc_remove_r(dvp, fname) osi_dnlc_remove(dvp, fname)

#define osi_Wait_r(ams, ahandle, aintok) osi_Wait(ams, ahandle, aintok)
#define osi_Pause_r(seconds) osi_Pause(seconds)
#endif

/*
 * Kernel versions of the following definitions reside in osi_port_mach.h.
 */
#if !defined(_KERNEL)

#define panic(msg)		abort()
#define osi_printf		printf
#define osi_vprintf		vprintf
#define osi_uprintf		printf

/*
 * XXX Global locking and unlocking
 */
#define osi_PreemptionOff()		0
#define osi_RestorePreemption(x)	/* void */

/*
 * Sleep/wakeup
 */
extern int osi_Sleep(opaque event);
extern int osi_SleepInterruptably(opaque event); /* XXX misspelled */
extern int osi_Wakeup(opaque event);

/*
 * Suspend execution for a given number of seconds
 */
extern void osi_Pause(long seconds);

/*
 * process ID stuff.
 */
#define osi_GetPid()		getpid()
#define osi_Reschedule()	pthread_yield()
extern long osi_ThreadUnique(void);
#define osi_ThreadID()		osi_ThreadUnique()

/*
 * Low level malloc/free definitions; used only by osi_misc.c:osi_Alloc.  See
 * osi_Alloc_r for explanation of "_r" suffix.
 */
#define osi_kalloc_r(size)	malloc(size)
#define osi_kfree_r(ptr, size)	free(ptr)

/*
 * Allocate/deallocate non pageable memory. Just allocate/deallocate memory
 * for now.
 */
#define osi_AllocPinned(asize)		osi_Alloc(asize)
#define osi_FreePinned(ptr, asize)	osi_Free(ptr, asize)

/*
 * Prevent paging out of memory. Noops in user space for now.
 */
#define osi_Pin(ptr, asize)	/* NULL */
#define osi_Unpin(ptr, asize)	/* NULL */

/*
 * Set memory block
 */
#define osi_Memset(ptr, val, len)	memset((ptr), (val), (len))
#endif /* !_KERNEL */

/*
 * Useful alignment macro(?); note this assumes sizeof(ptr) ==
 * sizeof(unsigned long)and ALIGN_SIZE is a power-of-two.
 *
 * XXX The purpose of this is obscure at best.
 */
#ifndef ALIGN_SIZE
#define ALIGN_SIZE 4
#endif /* ALIGN_SIZE */
#define osi_Align(type, ptr) \
    (type)((u_long)((ptr) + (ALIGN_SIZE - 1)) & \
		(u_long)(~(ALIGN_SIZE - 1)))

#ifndef	KERNEL
#define	osi_direct direct
#endif

/*
 * Status representation of a file object: We only utilize the following
 * stat related fields
 */
struct osi_stat {
    long size;		/* file size in bytes */
#ifdef SGIMIPS
    unsigned long blocksize;	/* optimal transfer size in bytes */
#else
    long blocksize;	/* optimal transfer size in bytes */
#endif
    long mtime;		/* modification date */
    long atime;		/* access time */
};

/*
 * Representation of a local file system file object
 */
struct osi_file {
    struct vnode *vnode;	/* Vnode associated with file */
    osi_off_t offset;		/* offset in the file */
    int	(*proc)(struct osi_file *, int); /* called on writes, if not null */
    char *rock;		/* rock passed to proc XXX not used */
};

#if defined(COMPAT_43) && BYTE_ORDER != BIG_ENDIAN

/* convert pre-bsd4.4 into bsd4.4 style sockaddr */
#define osi_KernelSockaddr(sa) \
    if (((struct sockaddr_in *)sa)->sin_family == 0 && \
	((struct sockaddr_in *)sa)->sin_len < AF_MAX) { \
	((struct sockaddr_in *)sa)->sin_family = \
		((struct sockaddr_in *)sa)->sin_len; \
	((struct sockaddr_in *)sa)->sin_len  = \
		sizeof (struct sockaddr_in); \
    }

/*
 * This is pretty ugly, since it relies on the known byte layout, but it will
 * work for the OSF/1 port, which is the target here.
 */


/* Work around a bug in the kernel space assembler that clobbers sa_len by
   not inserting a nop ager the lb (delayed load byte) before storing a zero
   in sa_family. Really want to use this line:

   #define osi_UserSockaddr(saP) (saP)->sa_len = (saP)->sa_family;  \
                                 (saP)->sa_family = 0

*/
#define osi_UserSockaddr(saP) (saP)->sa_len = (saP)->sa_family;  \
                              (saP)->sa_family -= (saP)->sa_len


#else
#define osi_KernelSockaddr(sa)
#define osi_UserSockaddr(sa)
#endif

/*
 * Extern globals/functions by module
 */
extern int osi_getvdev(caddr_t fspec, dev_t *devp, struct vnode **vp);
#define	osi_getmdev(fspec, devp) osi_getvdev(fspec, devp, 0)

#ifdef KERNEL

extern struct osi_file *osi_Open(struct osi_vfs *, osi_fid_t *);

extern int osi_Stat(struct osi_file *afile, struct osi_stat *astat);
extern int osi_Close(struct osi_file *afile);
extern int osi_Truncate(struct osi_file *afile, long asize);
extern int osi_Read(struct osi_file *afile, char *aptr, long asize);
extern int osi_Write(struct osi_file *afile, char *aptr, long asize);
extern int osi_RDWR(struct osi_file *afile, enum uio_rw arw, struct uio *auio);

#ifdef AFS_SUNOS5_ENV
extern long osi_MapStrategy(int (*aproc)(), osi_buf_t *bp, osi_cred_t *credp);
#else
extern long osi_MapStrategy(int (*aproc)(), osi_buf_t *bp);
#endif /* AFS_SUNOS5_ENV */
extern void osi_Invisible(void);
extern int osi_CallProc_r(osi_timeout_t (*aproc)(), char *arock, long ams);
extern void osi_CancelProc_r(osi_timeout_t (*aproc)(), char *arock, int cookie);
extern int osi_CallProc(osi_timeout_t (*aproc)(), char *arock, long ams);
extern void osi_CancelProc(osi_timeout_t (*aproc)(), char *arock, int cookie);
#ifdef SGIMIPS
extern time_t osi_Time(void);
#else
extern long osi_Time(void);
#endif
extern long osi_SetTime(struct timeval *atv);

#endif /* KERNEL */

/*
 * Exported by osi_misc.c
 */
extern int osi_nfs_gateway_loaded;
extern void (*osi_nfs_gateway_gc_p)();

extern opaque osi_Alloc(size_t x);
extern void osi_Free(opaque x, size_t asize);
extern opaque osi_Zalloc(size_t asize);

/* We adopt the convention of labeling routines that *must* run in a preemptive
 * environment with an "_r" suffix (at least on platforms that support this, in
 * this case only SUNOS5).  Any "_r" routine can only be called from an "_r"
 * routine or from a routine that explicitly drops the preemption lock using
 * the Make/UnmakePreemptionRight macros.  Once we drop it we must leave it
 * dropped. */

/* Another convention for the use of the "_r" suffix is for functions that are
 * reentrant.  The above convention is similar in spirit. */

extern opaque osi_Alloc_r(size_t x);
extern void osi_Free_r(opaque x, size_t asize);
extern opaque osi_AllocCaller_r (size_t asize, caddr_t caller, int type);
extern opaque osi_Zalloc_r(size_t asize);

extern void osi_Dump(void);
extern void osi_printIPaddr(char *preamble, u_long address, char *postamble);
extern void osi_cv2string(u_long avalue, char *buffer);
#ifdef SGIMIPS
extern void osi_cv2hstring(u_long avalue, char *buffer);
#endif /* SGIMIPS */
extern u_int osi_log2(u_int n);

extern opaque osi_AllocBufferSpace(void);
extern void osi_FreeBufferSpace(struct osi_buffer *bufp);
extern struct buf *osi_GetBuf(void);
extern void osi_ReleaseBuf(struct buf *);

extern opaque osi_AllocBufferSpace_r(void);
extern void osi_FreeBufferSpace_r(struct osi_buffer *bufp);
extern struct buf *osi_GetBuf_r(void);
extern void osi_ReleaseBuf_r(struct buf *);
#ifdef SGIMIPS
extern int osi_inet_aton(char *, struct in_addr *);
extern char * osi_inet_ntoa_buf(struct in_addr, char *);
#endif /* SGIMIPS */

#ifdef KERNEL
extern int osi_ZeroUData_r(char *buf, long len);
#endif

#ifndef KERNEL
extern int osi_atoi(char *str, int * err);
#endif

/*
 * Exported by osi_net.c
 */
#ifdef KERNEL
extern void osi_InitWaitHandle(struct osi_WaitHandle *achandle);
extern int osi_Wait(long ams, struct osi_WaitHandle *ahandle, int aintok);
extern void osi_CancelWait(struct osi_WaitHandle *achandle);
#endif /* KERNEL */

/* Exported by osi_printf.c */
#ifdef KERNEL
extern int osi_fprf(char *bufp, char *fmt, va_list ap);
#endif /* KERNEL */

/*
 * The following are various levels of afs debugging
 */
#define	OSIDEB_GENERAL		1	/* Standard debugging */
#define	OSIDEB_NETWORK		2	/* low level afs networking */
#define	OSIDEB_VNLAYER		8	/* interface layer to AFS (aixops, gfsops, etc) */

/* Debug mask bits for osi modules */
#define OSI_DEBUG_LOCK	1
#define OSI_DEBUG_MISC	2
#define OSI_DEBUG_NET	3
#define OSI_DEBUG_UFS	4

#define OSI_DEBUG_SLEEP	32	/* all calls to sleep */

extern int afsdb_osi;

#define osi_DBprint(p) dmprintf (afsdb_osi, DEBUG_THIS_FILE, p)
#define osi_SleepPrint(p) \
    dprintf(afsdb_osi & \
	((1u << (DEBUG_THIS_FILE - 1)) | \
	 (1u << (OSI_DEBUG_SLEEP - 1))), p)

/* afsl_TraceEnabled -- conditionally prints tracing messages.  Each call
 *     specifies the type of tracing being done, an enabling expression,
 *     condition which is evaluated only if the call is enabled and must be
 *     true for the trace output to be produced and a parenthesized printf-type
 *     argument list.
 *
 * PARAMETERS -- The first parameter is one of the following constants:
 *         AFSL_TR_ALWAYS -- always produce traces.
 *         AFSL_TR_ENTRY -- procedure entry
 *         AFSL_TR_EXIT -- procedure exit
 *         AFSL_TR_SLEEP -- thread about to block
 *         AFSL_TR_UNUSUAL -- some unusual condidtion occured
 *         AFSL_TR_ERRMAP -- one error code being mapped to another
 *	   AFSL_TR_ASSERT -- a fatal assertion error occured
 *
 *     The second parameter is an enabling expression which must be true for
 *     the call to produce output.  Typically this expression is produced by
 *     the AFSL_TR_ENABLED macro which evaluates the global and per package
 *     variables to determine whether to proceed.  For unconditional tracing or
 *     asserts this can just be "1".
 *
 *     The third parameter is a boolean condition that is evaluated only if the
 *     enabling predicate is true.  If it is true the trace output is produced.
 *
 *     The fourth "parameter" is actually a parameter list suitable for calling
 *     printf and is used to actually format the output.
 */
#ifdef AFSL_USE_RCS_ID

#define afsl_TraceEnabled(enabled, cond, type, args) \
    ((afsl_trace.control || \
      afsl_InitTraceControl (afsl_Fileid(), &afsl_trace)), \
     (enabled && cond && \
      (afsl_TracePrintProlog (type, &afsl_trace, __LINE__), \
       afsl_TracePrint args)))

/* afsl_TraceCond -- is like afsl_TraceEnabled, except that the enabling
 *     expression is always derived from the AFSL_TR_ENABLED macro.  It takes a
 *     type, a condition and a printf arg-list.  If the type is enabled then
 *     the condition is evaluated and if try the output is produced. */

#define afsl_TraceCond(type, cond, args) \
    afsl_TraceEnabled(AFSL_TR_ENABLED(type), cond, type, args)

/* afsl_Trace -- is like afsl_TraceCond except that the condition is assumed to
 *     be true.  The ouptput is produced whenever the type is enabled. */

#define afsl_Trace(type, args) \
    afsl_TraceEnabled(AFSL_TR_ENABLED(type), 1, type, args)

#else  /* AFSL_USE_RCS_ID */

#define afsl_TraceEnabled(enabled, cond, type, args)
#define afsl_TraceCond(type, cond, args)
#define afsl_Trace(type, args)

#endif /* AFSL_USE_RCS_ID */

/* Types of trace calls.  Controls identifying prefix string and some special
 * actions (e.g. flush if assert). */
#define AFSL_TR_ENTRY	0
#define AFSL_TR_EXIT	1
#define AFSL_TR_SLEEP	2
#define AFSL_TR_UNUSUAL	3
#define AFSL_TR_ERRMAP	4
#define AFSL_TR_ALWAYS	5
#define AFSL_TR_ASSERT	6

/* AFSL_TR_ENABLED -- produces a boolean expression which takes a type and
 *     returns true if the type is enabled.  In more detail it checks three
 *     types of variables: a global mask of enabled types, a mask of enabled
 *     types for the current package and a mask of enabled files within the
 *     current package.  If the current file doesn't have a fileMask
 *     established by a call to afsl_SetFileNumber then the file is always
 *     enabled.  This macro is used by afsl_TraceCond and afsl_Trace. */

#ifdef AFSL_USE_RCS_ID

#define AFSL_TR_ENABLED(type) \
    (((afsl_trace.control->enabledTypes | afsl_tr_global.enabledTypes) \
      & (1u << type)) && \
     (afsl_trace.fileMask == 0 || \
      (afsl_trace.control->enabledFiles & afsl_trace.fileMask)))

/* afsl_SetFileNumber -- takes no arguments and sets the file number as
 *     specified by the DEBUG_THIS_FILE macro.  If it isn't invoked the file is
 *     enabled by default. */

#define afsl_SetFileNumber() \
    (afsl_trace.fileMask = (1u << (afsl_trace.file = (DEBUG_THIS_FILE)) - 1))

#else /* AFSL_USE_RCS_ID */
#define AFSL_TR_ENABLED(type)
#define afsl_SetFileNumber()
#endif /* AFSL_USE_RCS_ID */

/* Now the prototypes for the actual parameters */

extern int afsl_InitTraceControl(
  char *rcsid,				/* caller's rcsid string */
  struct afsl_trace *trace);		/* caller's local trace info */

extern void afsl_TracePrintProlog (
  int type,
  struct afsl_trace *trace,
  int line);

/* PRINTFLIKE1 */
extern int afsl_TracePrint(char *fmt, ...);

extern struct afsl_tracePackage afsl_tr_global;

extern void afsl_AddPackage(
    char *packageName,
    struct afsl_tracePackage *package
);

#if 0
#define AFSL_TR_ENTRY   AFSL_TR_ENABLED(AFSL_TR_TYPE_ENTRY)
#define AFSL_TR_EXIT	AFSL_TR_ENABLED(AFSL_TR_TYPE_EXIT)
#define AFSL_TR_SLEEP	AFSL_TR_ENABLED(AFSL_TR_TYPE_SLEEP)
#define AFSL_TR_UNUSUAL	AFSL_TR_ENABLED(AFSL_TR_TYPE_UNUSUAL)
#define AFSL_TR_ERRMAP	AFSL_TR_ENABLED(AFSL_TR_TYPE_ERRMAP)

#define AFSL_MAX_TRACE_PARAMS 15
struct afsl_traceParameters {
    int params[AFSL_MAX_TRACE_PARAMS];
};
extern struct afsl_traceParameters afsl_TraceArgs();

#define AFSL_TR_STREAM (&afsl_trace), __LINE__
#endif

/*
 * Run-time assertions are divided into three categories, reflecting
 * differing levels of cost vs. expected benefit.  The curiously named
 * afsl_AssertTruth macro is used for assertions that we always want
 * to check; plain afsl_Assert represents an intermediate level of
 * desirability, and afsl_AssertCond is used for assertions that are
 * regarded as not very important.  The treatment of these macros
 * depends on two preprocessor symbols, AFSL_DEBUG_LEVEL and AFS_DEBUG.
 * The first of these lets you specify the desired level of checking:
 *
 *     level  name	explanation
 *      >=0   Truth	Crucial invariant required for correct operation.
 *			(e.g. check code from a procedure that should not fail)
 *      >=2   <none>    Importance/expense ratio fairly high. (e.g. boolean
 *			expression of simple variables)
 *	>=5   Cond      Expensive consistency checking code. (e.g. function
 *			call or complex macro)
 *
 * The AFS_DEBUG symbol provides a second, non-orthogonal knob, with the
 * following rules: if AFS_DEBUG is defined and AFSL_DEBUG_LEVEL is not,
 * then we set the level to three; if neither is defined, we set the
 * level to zero; if both are defined, then we require that the level
 * be nonzero.  The following table summarizes these rather confusing
 * dependencies:
 *
 * LEVEL DEBUG       LEVEL DEBUG
 *  nil   nil          0
 *  nil    t           3
 *   0    nil
 *   0     t           ?     ?
 *  >=1   nil                t
 *  >=1    t
 */

extern unsigned long afsl_tr_mbz_error;

#if !defined(AFSL_DEBUG_LEVEL)
# if !defined(AFS_DEBUG)
#  define AFSL_DEBUG_LEVEL 0
# else
#  define AFSL_DEBUG_LEVEL 3
# endif
#elif (AFSL_DEBUG_LEVEL == 0)
# if defined(AFS_DEBUG)
#  error inconsistent DEBUG settings
# endif
#elif !defined(AFS_DEBUG)
# define AFS_DEBUG 1
#endif

#ifdef KERNEL

#define afsl_AssertTruth(cond)		osi_assert(cond)
#define afsl_PAssertTruth(cond, str)	afsl_AssertTruth(cond)

#else /* !KERNEL */

#ifdef AFSL_USE_RCS_ID
#define afsl_AssertTruth(cond) \
    (!(cond) ? \
     (afsl_TraceEnabled (1, 1, AFSL_TR_ASSERT, ("assert failed")), \
      abort(), 0) : 0)
#define afsl_PAssertTruth(cond, str) \
    (!(cond) ? \
     (afsl_TraceEnabled (1, 1, AFSL_TR_ASSERT, str), abort(), 0) : 0)
#else
#define afsl_AssertTruth(cond)		osi_assert(cond)
#define afsl_PAssertTruth(cond, str)	osi_assert(cond)
#endif /* AFSL_USE_RCS_ID */

#endif /* KERNEL */


#if AFSL_DEBUG_LEVEL >= 2
#define afsl_Assert(cond)		afsl_AssertTruth(cond)
#define afsl_PAssert(cond, str)		afsl_PAssertTruth(cond, str)
#else
#ifdef SGIMIPS
#define afsl_Assert(cond)		((void)0)
#define afsl_PAssert(cond, str)		((void)0)
#else
#define afsl_Assert(cond)		0
#define afsl_PAssert(cond, str)		0
#endif
#endif

#if AFSL_DEBUG_LEVEL >= 5
#define afsl_AssertCond(cond)		afsl_AssertTruth(cond)
#define afsl_PAssertCond(cond, str)	afsl_PAssertTruth(cond, str)
#else
#ifdef SGIMIPS
#define afsl_AssertCond(cond)		((void)0)
#define afsl_PAssertCond(cond, str)	((void)0)
#else
#define afsl_AssertCond(cond)		0
#define afsl_PAssertCond(cond, str)	0
#endif
#endif


/* Some higher level asserts.  Maybe the lock related ones should be defined in
 * the lock package? */
#define afsl_MBZ(n) \
    (((n) == 0) || \
     ((afsl_tr_mbz_error = (unsigned long) (n)), \
      (afsl_PAssertTruth(0, ("afsl_MBZ: code(%d) was not zero", (n))))))

#define afsl_AssertWriteLocked(l)	afsl_Assert(lock_Check(l) == -1)
#define afsl_AssertReadLocked(l)	afsl_Assert(lock_Check(l) > 0)
#define afsl_AssertUnlocked(l)		afsl_Assert(lock_Check(l) == 0)
#define afsl_AssertLocked(l)		afsl_Assert(lock_Check(l) != 0)

/*
 * XXX Synonyms for compatibility
 */
#define MBZ(exp)		afsl_MBZ(exp)	/* Must Be Zero */
#define AssertWriteLocked(l)	afsl_AssertWriteLocked(l)
#define AssertReadLocked(l)	afsl_AssertReadLocked(l)
#define AssertUnlocked(l)	afsl_AssertUnlocked(l)
#define AssertLocked(l)		afsl_AssertLocked(l)

/* Trace initialization calls used in kernel extension initialization
 * routines
 */
#if (defined(KERNEL) && defined(TRACE_TYPE_ICL))
#define	OSI_TRACE_INIT() osi_trace_init()
#else
#define	OSI_TRACE_INIT()    /* NULL */
#endif

#endif /* TRANSARC_OSI_H */
