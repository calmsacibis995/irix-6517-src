#ifndef __CKPT_PROCFS_H__
#define __CKPT_PROCFS_H__
/*
 * ckpt_procfs.h
 *
 *	Checkpoint/restart header file.  Contains defs for checkpoint procfs
 *	interfaces.
 *
 * Copyright 1995, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 *
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 *
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 *
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 */

#ident "$Revision: 1.70 $"

#if defined(_LANGUAGE_C) || defined(_LANGUAGE_C_PLUS_PLUS)

#include <sys/timers.h>
#include <sys/resource.h>
#include <sys/stat.h>
#if _MIPS_SIM == _ABIO32
#include <sys/kucontext.h>
#else
#include <sys/ucontext.h>
#endif
#include <sys/pmo.h>
#include <sys/prctl.h>

#define	PIOCCKPTSHM	(PIOC|180)	/* get shm ids */
#define	PIOCCKPTSTOP	(PIOC|181)	/* request/clear CHECKPOINT stop */
#define	PIOCCKPTUTINFO	(PIOC|182)	/* get misc threa info */
#define	PIOCCKPTSIGTHRD	(PIOC|183)	/* send a signal to a thread */
#define	PIOCCKPTSCHED	(PIOC|184)	/* set schedmode */
#define	PIOCCKPTFSWAP	(PIOC|185)	/* swap file handles with target */
#define PIOCCKPTPMPGINFO (PIOC|186)	/* get pm page info */

#define PIOCCKPTFSTAT	(PIOC|200)	/* get stat info on fd */
#define PIOCCKPTOPEN	(PIOC|201)      /* open using a file descriptor */
#define	PIOCCKPTGETMAP	(PIOC|202)	/* get ckpt mapping info */
#define	PIOCCKPTGETCTX	(PIOC|203)	/* get user context */
#define	PIOCCKPTSETCTX	(PIOC|204)	/* set user context */
#define	PIOCCKPTPSINFO	(PIOC|205)	/* get any other missing proc info */
#define	PIOCCKPTSETPI	(PIOC|206)	/* restore process info */
#define	PIOCCKPTGETSI	(PIOC|208)	/* get siginfo  */
#define	PIOCCKPTQSIG	(PIOC|209)	/* queue signal */
#define	PIOCCKPTSSIG	(PIOC|210)	/* set current signal */
#define	PIOCCKPTABORT	(PIOC|211)	/* abort syscall or jobstop */
#define	PIOCCKPTMARK	(PIOC|212)	/* mark a process for checkpoint warn */
#define	PIOCCKPTDUP	(PIOC|213)	/* dup a file descriptor */
#define PIOCCKPTOPENCWD	(PIOC|216)      /* open p_cdir */
#define PIOCCKPTOPENRT	(PIOC|217)      /* open p_rdir */
#define PIOCCKPTCHROOT	(PIOC|218)      /* chroot */
#define	PIOCCKPTMSTAT	(PIOC|219)	/* get stat info on mapped file */
#define	PIOCCKPTFORKMEM	(PIOC|220)	/* fork a memory area */
#define	PIOCCKPTUSAGE	(PIOC|221)	/* same as PIOCUSAGE, zombies allowed */
#define	PIOCCKPTGETITMRS (PIOC|222)	/* get interval timers */
#define	PIOCCKPTSETRITMR (PIOC|223)	/* set pocs real time itimer */
#define	PIOCCKPTPUSEMA	(PIOC|224)	/* get posix unamed sema state */
#define	PIOCCKPTPMOGETNEXT (PIOC|225)	/* get next vaild pm handle */
#define	PIOCCKPTPMGETALL (PIOC|226)	/* get policy module handles */
#define	PIOCCKPTPMINFO	(PIOC|227)	/* get policy module state */
#define	PIOCCKPTMLDINFO	(PIOC|228)	/* get mld info (radius, size) */
#define	PIOCCKPTMLDLINKINFO (PIOC|229)	/* get mldlink info (radius, size,..) */
#define PIOCCKPTMLDSETINFO (PIOC|230)	/* get mldset info */
#define	PIOCCKPTMLDPLACEINFO (PIOC|231)	/* get mldset placement info */
#define	PIOCCKPTRAFFOPEN (PIOC|232)	/* open rafflist node */
/*
 * 232 is highest currently reserved # for checkpoint ioctls!!!
 */
/*
 * Checkpoint file stat buffer
 *
 */
typedef struct ckpt_statbuf {
	int		ckpt_fd;	/* file descriptor */
	int		ckpt_fflags;	/* file flags, see ksys/vfile.h */
	off64_t		ckpt_offset;	/* file offset */
	mode_t		ckpt_mode;	/* original file mode */
	size_t		ckpt_size;	/* file size */
	dev_t		ckpt_dev;	/* file fsid */
	dev_t		ckpt_rdev;	/* device file */
	ino_t		ckpt_ino;	/* file ino number */
	caddr_t		ckpt_fid;	/* internal file id */
	timespec_t	ckpt_atime;	/* access time */
	timespec_t	ckpt_mtime;	/* mod time */
	char		ckpt_fstype[_ST_FSTYPSZ];
} ckpt_statbuf_t;

/*
 * Checkpoint open buffer
 *
 */
typedef struct ckpt_open {
	int	ckpt_fd;		/* source file descriptor */
	int	ckpt_oflag;		/* open flags, same as open(2) */
	u_short	ckpt_mode_req;		/* vmode required for CPR purpose */
} ckpt_open_t;

/*
 * Checkpoint mapping info buffer.  Used in conjunction with PIOCMAP_SGI
 * to get all mapping info.
 */
typedef struct ckpt_getmap {
	caddr_t		m_mapid;	/* unique map id */
	caddr_t		m_vaddr;	/* pregion start address */
	long		m_size;		/* pregion size (bytes) */
	off_t		m_maxoff;	/* max file offset */
	int		m_shmid;	/* shared mem id, if applicable */
	ushort_t	m_flags;	/* flags (see below) */
	uchar_t		m_prots;	/* max prots */
	caddr_t		m_physaddr;	/* physical mapping */
} ckpt_getmap_t;

/*
 * Mapping info flags
 */
#define	CKPT_AUTOGROW	0x0001		/* MAP_AUTOGROW */
#define	CKPT_LOCAL	0x0002		/* MAP_LOCAL */
#define	CKPT_AUTORESRV	0x0004		/* MAP_AUTORESRV */
#define	CKPT_PRDA	0x0008		/* PRDA */
#define CKPT_MAPFILE	0x0010		/* mmap */
#define	CKPT_EXECFILE	0x0020		/* executible */
#define	CKPT_STACK	0x0040		/* stack */
#define	CKPT_MEM	0x0080		/* generic memory */
#define	CKPT_DUP	0x0100		/* dup on fork */
#define	CKPT_SHMID	0x0200		/* valid shmid */
#define	CKPT_PUSEMA	0x0400		/* Posix smaphore */
#define CKPT_MAPZERO	0x0800		/* mapping is /dev/zero */
#define	CKPT_MAPPRIVATE	0x1000		/* MAP_PRIVATE mapping */

/*
 * This structure contains any process info we cannot get from other existing
 * interfaces.
 */
typedef struct ckpt_psinfo {
	caddr_t		ps_execid;	/* memory id of exec file */
	char		ps_acflag;	/* for accounting */
	char		ps_state;	/* proc state */
	uchar_t		ps_ckptflag;	/* process ckpt flags */
	char		ps_lock;	/* process/text locking flags */
	caddr_t		ps_brkbase;
	size_t		ps_brksize;
	caddr_t		ps_stackbase;
	size_t		ps_stacksize;
	/* signal stuff */
	caddr_t		ps_sigtramp;	/* process sigtramp address */

	short           ps_xstat;	/* exit status if a zombie */

	/* partial accouting info missing from the ckpt_pi_t */
	u_long		ps_mem;
	u_long		ps_ioch;
	struct rusage   ps_ru;		/* stats for this proc */
        struct rusage   ps_cru;		/* sum of stats for reaped children */
	ktimerpkg_t	ps_timers;	/* accting states timer */
	struct rlimit   ps_rlimit[RLIM_NLIMITS]; /* u: 4.3BSD resource limits */
	mode_t		ps_cmask;		/* u: mask for file creation */
	/* sproc info */
	void		*ps_shaddr;		/* is this an sproc? */
	uint		ps_shmask;		/* share group share mask */
	ushort_t	ps_shrefcnt;		/* # procs in group */
	uchar_t		ps_shflags;		/* share flags (below) */
	/* blockproc info */
	pid_t		ps_unblkonexecpid;	/* name says it all */
	/* misc stuff */
	char		ps_exitsig;		/* proc exitsig */
	/* misc flags */
	ulong_t		ps_flags;		/* misc flags...see below */
	/* numa stuff */
	caddr_t		ps_mldlink;		/* id of linked mld */

} ckpt_psi_t;
/*
 * ps_ckptflag
 * 0x00 - 0x04 reserved
 */
#define CKPT_PROC_EXIT          0x08    /* process has completed exit */

/*
 * ps_flags
 */
#define	CKPT_HWPERF	0x01		/* using hwperf counters */
#define	CKPT_TERMCHILD	0x02		/* prctl(PR_TERMCHILD) */
#define	CKPT_COREPID	0x04		/* prctl(PR_COREPID) */
#define	CKPT_PMO	0x08		/* process has created a pmo namespace */

typedef struct ckpt_uti {
	unsigned	uti_flags;
	ushort		uti_whystop;
	ushort		uti_whatstop;
	short		uti_blkcnt;
	uchar_t		uti_isblocked;		/* is proc actually blocked? */
	prsched_t	uti_sched;
} ckpt_uti_t;
/*
 * flags values
 */
#define	CKPT_UTI_BLKONENTRY	0x01		/* blocked at syscall entry */
#define	CKPT_UTI_SCHED		0x02		/* non-default scheduling */
#define	CKPT_UTI_PTHREAD	0x04		/* registered for pthread */

/*
 * Share flags
 */
#define	CKPT_PS_MASTER		0x01	/* is master of group */
#define	CKPT_PS_CRED		0x02	/* holds shared cred pointer */
#define	CKPT_PS_SPIDEXIT	0x04	/* holds shared pid exit status */
/*
 * For restoring proc struct state
 */
#define	PSCOMSIZ	32
#define	PSARGSZ		80
typedef struct ckpt_spi {
	char		spi_comm[PSCOMSIZ];	/* process name */
	char		spi_psargs[PSARGSZ];	/* and arguments */
	char		spi_acflag;		/* for accounting */
	u_long	  	spi_gbread;		/* # of of gigabytes of input */
	u_long	  	spi_bread;		/* # of bytes of input */
	u_long	  	spi_gbwrit;		/* # of of gigabytes of output */
	u_long	  	spi_bwrit;		/* # of bytes of output */
	u_long	  	spi_syscr;		/* # of read system calls */
	u_long	  	spi_syscw;		/* # of write system calls */
	char	    	spi_time;		/* resident time for scheduling */
	uchar_t		spi_shflags;		/* share flags (below) */
	char		spi_exitsig;		/* exit signal */
	long		spi_stat;		/* status at checkpoint */
	timespec_t      spi_birth_time;		/* start time since 1970 */
	u_long		spi_mem;
	u_long		spi_ioch;
	struct rusage	spi_ru;			/* stats for this proc */
	struct rusage	spi_cru;		/* stats for reaped children */
	ktimerpkg_t	spi_timers;		/* accting states timer */
	mode_t		spi_cmask;		/* u: mask for file creation */
	short		spi_xstat;		/* spid exit status */
} ckpt_spi_t;

typedef struct ckpt_ctxt {
#if _MIPS_SIM != _ABIO32
	ucontext_t		cc_ctxt;	/* machine context */
#else
	irix5_n32_ucontext_t	cc_ctxt;
#endif
	u_char			cc_abi;		/* process abi */
	char			cc_fpflags;	/* process floating point flags */
} ckpt_ctxt_t;

/*
 * Args to PIOCCKPTGETSI
 */
typedef struct ckpt_siginfo_arg {
	uchar_t	 csa_async;		/* sync & async ? */
	int	 csa_count;		/* number of entries in buffer */
	caddr_t	 csa_buffer;		/* siginfo buffer */
	caddr_t	 csa_curinfo;		/* curinfo buffer */
	int	 csa_cursig;		/* current signal */
	sigset_t csa_pending;		/* pending signals */
	sigset_t csa_asyncsigs;		/* posted async signals */
} ckpt_siginfo_arg_t;
/*
 * Args to PIOCCKPTSSIG
 */
typedef struct ckpt_cursig_arg {
	int	signo;			/* signal number */
	caddr_t	infop;			/* sig info struct pointer */
} ckpt_cursig_arg_t;

/* Return value defines for PIOCCKPTABORT */
#define	SYSABORT	1	/* aborted syscall */
#define	STOPABORT	2	/* aborted jobstop */
#define	SYSEND		3	/* completed syscall */
#define	SYSTRAMP	4	/* on it's way to sigtramp */

/*
 * Args to PIOCCKPTMSTAT
 */
typedef struct ckpt_mstat_arg {
	sysarg_t statvers;	/* stat version */
	caddr_t	vaddr;		/* virtual address */
	struct stat *sbp;	/* stat buffer */
} ckpt_mstat_arg_t;
/*
 * Args to PIOCCKPTFORKMEM
 */
typedef struct ckpt_forkmem_arg {
	caddr_t	vaddr;
	ssize_t	len;
	int	local;
} ckpt_forkmem_arg_t;

/*
 * Args to PIOCCKPTPUSEMA
 */
typedef struct ckpt_pusema_arg {
	int		count;		/* number of entries in the buffer */
	caddr_t		uvaddr;		/* user vaddr of region */
	caddr_t		bufaddr;	/* address of sema state buffer */
} ckpt_pusema_arg_t;
/*
 * PIOCCKPTPMOGETNEXT
 */
typedef struct ckpt_getpmonext {
	pmo_type_t	pmo_type;		/* what kind of pm object */
	pmo_handle_t	pmo_handle;	/* handle index */
} ckpt_getpmonext_t;

/*
 * PIOCCKPTPMGETALL
 */
typedef struct ckpt_pmgetall_arg {
	vrange_t		*ckpt_vrange;	/* base vaddr & length */
	pmo_handle_list_t	*ckpt_handles;	/* return list of handles */
} ckpt_pmgetall_arg_t;

/*
 * PIOCCKPTPMINFO
 */
typedef struct ckpt_pminfo_arg {
	pmo_handle_t	ckpt_pmhandle;		/* policy module handle */
	pm_info_t	*ckpt_pminfo;		/* policy module info struct */
	pmo_handle_t	*ckpt_pmo;		/* pm->pmo handle */
} ckpt_pminfo_arg_t;

#endif /* C || C++ */

/*
 * PIOCCKPTMLDINFO
 */
typedef struct ckpt_mldinfo {
	int		mld_radius;
	pgno_t		mld_size;
	cnodeid_t	mld_nodeid;
	caddr_t		mld_id;
} ckpt_mldinfo_t;

typedef struct ckpt_mldinfo_arg {
	pmo_handle_t	ckpt_mldhandle;
	ckpt_mldinfo_t	*ckpt_mldinfo;
} ckpt_mldinfo_arg_t;

typedef struct ckpt_mldset_info_arg {
	pmo_handle_t	mldset_handle;
	mldset_info_t	mldset_info;
} ckpt_mldset_info_arg_t;

typedef struct ckpt_raffopen_t {
	pmo_handle_t	mldset_handle;
	int		mldset_element;	/* element in raff list */
} ckpt_raffopen_t;
/*
 * PIOCCKPTSHM
 */
typedef struct ckpt_shm_arg {
	int	*shmid;		/* buf for list of shmids */
	int	count;		/* # o entries tht can fit in buf */
} ckpt_shm_arg_t;

/*
 * PIOCCKPTFSWAP
 */
typedef struct ckpt_fswap_arg {
	int	srcfd;		/* fd of current process */
	int	targfd;		/* fd of target process */
} ckpt_fswap_t;

/*
 * PIOCCKPTPMPGINFO
 */

typedef struct ckpt_pm_pginfo {
	vrange_t 		vrange;		/* virtual addr range */
	pm_pginfo_list_t 	pginfo_list;	/* page info list */
} ckpt_pm_pginfo_t;

#endif /* !__CKPT_PROCFS_H__ */
