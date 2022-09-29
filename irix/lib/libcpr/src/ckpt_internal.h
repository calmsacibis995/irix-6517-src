/*                                                                        *
 *               Copyright (C) 1996, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ifndef __CKPT_INTERNAL_H__
#define	__CKPT_INTERNAL_H__

#ifdef __cplusplus
extern "C" {
#endif

#ident "$Revision: 1.98 $"

#include <stdio.h>
#include <errno.h>
#include <limits.h>
#include <abi_mutex.h>
#include <sys/proc.h>
#include <sys/prctl.h>
#include <sys/fcntl.h>
#include <sys/param.h>
#include <sys/procfs.h>
#include <sys/ckpt.h>
#include <sys/ckpt_procfs.h>
#include <sys/EVEREST/ccsync.h>
#include <sys/hwperftypes.h>
#include <sys/usync.h>
#include <sys/pmo.h>
#include <sys/shm.h>
#include <sys/arsess.h>
/* #include <fetchop.h> */
#include <ckpt.h>

/***************************************************************************
 * 			For SGI internal only				   *
 ***************************************************************************/

#define MAGIC(a,b,c,d,e)	((((ckpt_magic_t)('C')&0xff)<<56)|\
	(((ckpt_magic_t)('P')&0xff)<<48)|(((ckpt_magic_t)('R')&0xff)<<40)|\
	(((ckpt_magic_t)(a)&0xff)<<32)|(((ckpt_magic_t)(b)&0xff)<<24)|\
	(((ckpt_magic_t)(c)&0xff)<<16)|(((ckpt_magic_t)(d)&0xff)<<8)|\
	((ckpt_magic_t)(e)&0xff))

/*
 * Common magics used for both per-cpr and per-proc properties
 */
#define CKPT_MAGIC_INDEX	MAGIC('i','n','d','e','x')
#define CKPT_MAGIC_CHILDINFO	MAGIC('c','i','n','f','o')

/*
 * Shared (per-cpr) property magics
 */
#define	CKPT_MAGIC_DEPEND	MAGIC('d','e','p','e','n')
#define CKPT_MAGIC_PRATTACH	MAGIC('a','t','t','c','h')
#define	CKPT_MAGIC_UNLINKED	MAGIC('u','n','l','i','n')
#define	CKPT_MAGIC_PUSEMA	MAGIC('p','u','s','e','m')
#define	CKPT_MAGIC_MLDLINK	MAGIC('m','l','d','l','i')
#define	CKPT_MAGIC_HINV		MAGIC('_','h','i','n','v')

/*
 * Per-proc property magics
 */
#define CKPT_MAGIC_PROCINFO	MAGIC('p','i','n','f','o')
#define	CKPT_MAGIC_CTXINFO	MAGIC('c','t','x','i','f')
#define CKPT_MAGIC_SIGINFO	MAGIC('s','i','n','f','o')
#define	CKPT_MAGIC_THRDSIGINFO	MAGIC('t','h','r','d','s')
#define CKPT_MAGIC_OPENSPECS	MAGIC('o','s','p','e','c')
#define CKPT_MAGIC_OPENPIPES	MAGIC('o','p','i','p','e')
#define CKPT_MAGIC_OPENFILES	MAGIC('o','f','i','l','e')
#define	CKPT_MAGIC_OTHER_OFDS	MAGIC('o','t','h','f','d')
#define CKPT_MAGIC_MAPFILE	MAGIC('m','f','i','l','e')
#define CKPT_MAGIC_MEMOBJ	MAGIC('m','e','m','o','b')
#define	CKPT_MAGIC_ZOMBIE	MAGIC('z','o','m','b','i')
#define	CKPT_MAGIC_PMDEFAULT	MAGIC('p','m','d','e','f')
#define	CKPT_MAGIC_PMODULE	MAGIC('p','o','l','i','c')
#define	CKPT_MAGIC_MLD		MAGIC('m','l','d','o','m')
#define	CKPT_MAGIC_MLDSET	MAGIC('m','l','d','s','e')
#define CKPT_MAGIC_PMI		MAGIC('p','m','i','p','m')
#define	CKPT_MAGIC_SHM		MAGIC('s','y','s','V','s')
#define	CKPT_MAGIC_THREAD	MAGIC('t','h','r','e','a')

/*
 * magics for objects that are part of a property above
 */
#define CKPT_MAGIC_SVR3PIPE	MAGIC('p','s','v','r','3')
#define CKPT_MAGIC_SVR4PIPE	MAGIC('p','s','v','r','4')
#define CKPT_MAGIC_MEMMAP	MAGIC('m','e','m','a','p')
/*
 * Some fundamental types  to insure sizes across 32 & 64 bit
 */
typedef long long ckpt_magic_t;
/*
 * Some defines for i/o
*/
#define	MAXIOV	16			/* gleaned from kernel code */
#define	CKPT_IOBSIZE	0x100000	/* preferred io buffer size */
/*
 * Property behavior (operator) discriptor. Used to define property table.
 */
typedef struct ckpt_prop_handle {
	char		prop_index;
	ckpt_magic_t	prop_magic;
	char		prop_name[16];
	int		(*prop_checkpoint)();
	int		(*prop_checkpoint_err)();
	int		(*prop_restart)();
	int		(*prop_restart_err)();
	int		(*prop_remove)();
	int		(*prop_stat)();
} ckpt_phandle_t;

/*
 * Definition for property object of each process. Each process is described
 * by a number of property objects such as files, memory etc.
 */
typedef struct ckpt_property {
	ckpt_magic_t	prop_magic;	/* magic to id this property */
	char		prop_name[16];	/* for debug purpose */
	int		prop_offset;	/* property's file offset in statef */
	int		prop_len;	/* total length of this type of property */
	int		prop_quantity;	/* total no. of this type of property */
	long		prop_reserve[4];
} ckpt_prop_t;

/*
 * Per-CPR index header (keep the 1st 3 fields the same with children header)
 */
typedef struct ckpt_per_cpr_index {
	ckpt_magic_t	pci_magic;	/* CKPT_MAGIC_INDEX */
	uid_t		pci_ruid;	/* ruid of person who checkpoints */
	gid_t		pci_rgid;	/* rgid of person who checkpoints */
	ckpt_rev_t	pci_revision;	/* CPR revision number */
	ckpt_id_t	pci_id;		/* POSIX and SGI id for cpr */
	ckpt_type_t	pci_type;	/* POSIX definitions of types */
	int		pci_nproc;	/* number of procs checkpointed */
	int		pci_ntrees;	/* number of proc trees */
	int		pci_flags;	/* generic flags */
	int		pci_prop_count;	/* number of shared properties */
	long long	pci_reserve[8];
} ckpt_pci_t;

/*
 * Per-proc index header
 */
typedef struct ckpt_per_proc_index {
	ckpt_magic_t	ppi_magic;
	pid_t		ppi_pid;
	int		ppi_prop_count;	/* total number of property slots saved */
	long		ppi_reserve[5];
} ckpt_ppi_t;

#define CWRITE(fd, p, len, quan, pp, rc)	{	\
	rc = write(fd, p, len);				\
	pp->prop_quantity += quan;			\
	pp->prop_len += len;}


/*
 * The following two sets of definitions are for cpr attributes
 *
 * definitions for func names
 */
#define CKPT_FILE	0	/* file policy */
#define CKPT_WILL	1	/* after death policy */
#define CKPT_CDIR	2	/* cwd policy */
#define CKPT_RDIR	3	/* root policy */
#define CKPT_FORK	4	/* fork/pidfork policy */
#define CKPT_RELOCDIR   5       /* file relocation directory */

/*
 * definitions of actions for each CPR attr record
 */
#define CKPT_MEGRE		0
#define CKPT_REPLACE		1
#define CKPT_SUBSTITUTE		2
#define CKPT_APPEND		3
#define CKPT_IGNORE		4
#define CKPT_RELOCATE           5
#define CKPT_EXIT               6
#define CKPT_CONT               7
#define CKPT_FORKORIG           8
#define CKPT_FORKANY            9

/*
 * Hardware inventory object
 */
typedef struct ckpt_hinv {
	ckpt_magic_t	ch_magic;	/* CKPT_MAGIC_HINV */
	int		ch_board;	/* processor board type */
	int		ch_cpu;		/* cpu type */
} ckpt_hinv_t;

/*
 * Basic structure to descibe the process
 */
#define	PSCOMSIZ	32
#define	PSARGSZ		80

typedef struct ckpt_procinfo {
	ckpt_magic_t	cp_magic;		/* CKPT_MAGIC_PROCINFO */

	/* u area related info */
	char		cp_comm[PSCOMSIZ];	/* process name */
	char		cp_psargs[PSARGSZ];	/* and arguments */
	char		cp_cdir[PATH_MAX];	/* current directory */
        char		cp_rdir[PATH_MAX];	/* root directory */

        u_long          cp_gbread;		/* # of of gigabytes of input */
        u_long          cp_bread;		/* # of bytes of input */
        u_long          cp_gbwrit;		/* # of of gigabytes of output */
        u_long          cp_bwrit;		/* # of bytes of output */
	u_long          cp_syscr;		/* # of read system calls */
	u_long          cp_syscw;		/* # of write system calls */

	/* struct proc related stuff */
	long		cp_stat;		/* process state */
        char            cp_cpu;			/* cpu usage for scheduling */
        char            cp_nice;		/* nice for cpu usage */
        char            cp_time;		/* resident time for scheduling */
        pid_t           cp_pid;			/* not used since pidfork is used */
        pid_t           cp_ppid;		/* process id of parent */
        pid_t		cp_pgrp;		/* name of process group leader */
	pid_t           cp_sid;			/* session id */
	cpuid_t		cp_mustrun;		/* mustrun cpu if any */
	ushort          cp_tslice;		/* process time slice size */
	dev_t		cp_ttydev;		/* controlling tty */
	hwperf_eventctrl_t	cp_hwpctl;	/* hwperf control info */
	hwperf_eventctrlaux_t	cp_hwpctlaux;	/* hwperf aux control info */
	hwperf_profevctraux_t	cp_hwpaux;	/* hwperf aux counter info */

	/* array related info */
	prid_t		cp_prid;		/* array session project id */
	char		cp_spi[MAX_SPI_LEN];	/* array service provider */
	int		cp_spilen;		/* length of SPI */

	/*
	 * XXX we need to see if we can get by without saving kthread info
	 * we need to exam p_time, p_utime, p_slptime
	 */
	timespec_t	cp_birth_time;		/* start time since 1970 */
	timespec_t	cp_ckpt_time;		/* when the proc was checkpointed */

	struct itimerval cp_itimer[ITIMER_MAX];	/* interval timers */
	prcred_t	cp_cred;		/* process credentials */
	/*
	 * ckpt_psi_t is all of the other info we cannot get using existing
	 * interfaces.
	 */
	ckpt_psi_t	cp_psi;
	int		cp_nofiles;		/* # of openfiles */
	int		cp_maxfd;		/* highest # fd */
	int		cp_npfds;		/* max # of prototype pipes */
	uint_t		cp_flags;		/* flags, see below */
	int		cp_schedmode;		/* sproc sched mode */
	char		cp_clname[4];		/* scheduler class */
	int		cp_schedpolicy;		/* scheduler policy */
	int		cp_schedpriority;	/* scheduler priority */
	int		cp_pads[10];		/* enough space for future exp. */
} ckpt_pi_t;

/* cp_flags */
#define	CKPT_PI_ROOT		0x1		/* is proctree root */
#define	CKPT_PI_NOCAFFINE	0x2		/* no cache affinity */

/*
 * Thread info
 */
/*
 * Thread args
 */
typedef struct ckpt_thread_inst {
	ckpt_magic_t	cti_magic;	/* CKPT_MAGIC_THREAD */
	tid_t		cti_tid;	/* threda id */
	ckpt_uti_t	cti_uti;	/* thread info */
} ckpt_thread_inst_t;

/*
 * thread context
 */
typedef struct ckpt_context {
	ckpt_magic_t	cc_magic;	/* CKPT_MAGIC_CTXINFO */
	tid_t		cc_tid;		/* thread id */
	uint		cc_flags;	/* context flags */
	ckpt_ctxt_t	cc_ctxt;	/* context */
} ckpt_context_t;
#define	CKPT_CTXT_REISSUE	0x1	/* reissuing syscall */

/*
 * Per CPR request linked list for open files. The list is used to avoid
 * duplicated saving of open files by different processes. vnode being unique 
 * per system guarantees the need. We need a simple mutex lock to protect the list 
 * if we go multi-threaded impl.
 */
typedef struct ckpt_flist {
	struct ckpt_flist *cl_next;	/* next open file for this cpr request */
	dev_t	cl_dev;			/* dev # of this file */
	ino_t	cl_ino;			/* inode # of this file */
	pid_t	cl_pid;			/* handling pid */
	int	cl_fd;			/* handling pids fd */
	caddr_t	cl_id;			/* file table instance identifier */
	int	cl_mode;		/* handle action (merge, append, ...) */
	char	*cl_wildpath;		/* only for recording wild cards */
} ckpt_flist_t;


typedef struct ckpt_flock {
	struct ckpt_flock *fl_next;	/* file locks owned by this proc */
	flock_t	fl_lock;		/* saved lock */
} ckpt_fl_t;

typedef struct stropts {
	int	str_sigevents;	/* SIGPOLL events */
	int	str_rdopts;	/* read options */
	int	str_wropts;	/* write options */
} ckpt_str_t;

/*
 * Per open file header saved in each CPR targeted process's statefile
 */
typedef struct ckpt_openfile {
	ckpt_magic_t cf_magic;		/* CKPT_MAGIC_OPENFILES */
	int	cf_flags;		/* indicate this file's status to this proc */
	int	cf_mode;		/* see below for definitions */
	int	cf_fd;			/* open file descriptor */
	int	cf_dupfd;		/* fd it's a dup of, if applicable */
	pid_t	cf_duppid;		/* proc to dup from, if applicable */
	ushort_t cf_fflags;		/* file flags */
	int	cf_fdflags;		/* fd flags ala fcntl */
	off_t	cf_offset;		/* file offset */
	off_t	cf_length;		/* file length */
	mode_t	cf_fmode;		/* file mode */
	dev_t   cf_rdev;		/* for spec file only */
	void	*cf_auxptr;		/* for spec file only */
	int	cf_auxsize;		/* for spec file only */
	int	cf_psemavalue;		/* if it's a psema, the sema value */
	ckpt_str_t cf_stropts;		/* streams options */
	/*
	 * time fields used only for regular files and named pipes
	 * with mode REPLACE
	 */
	timespec_t cf_atime;		/* access time */
	timespec_t cf_mtime;		/* mod time */
	ckpt_fl_t *cf_flp;		/* file lock chain */
	char	cf_path[CPATHLEN];	/* orig. file path */
	char	cf_newpath[CPATHLEN];	/* new path to save this file, if any */ 
} ckpt_f_t;

/* cf_flags definitions */
#define	CKPT_FLAGS_OPENFD	0x01	/* process needs to open the file */
#define	CKPT_FLAGS_DUPFD	0x02	/* dup from another fd */
#define	CKPT_FLAGS_INHERIT	0x04	/* process inherited this file */
#define	CKPT_FLAGS_UNLINKED	0x08	/* the file is unlinked */
#define	CKPT_FLAGS_MAPFILE	0x10	/* mapped rather than open file */
#define CKPT_FLAGS_LOCKED       0x20    /* we own the flock of this file */
#define	CKPT_FLAGS_PSEMA	0x40	/* this file is named psema */

/* cf_mode definitions */
#define	CKPT_MODE_MERGE		CKPT_MEGRE	/* default action for open files */
#define	CKPT_MODE_REPLACE	CKPT_REPLACE
#define	CKPT_MODE_SUBSTITUTE	CKPT_SUBSTITUTE
#define	CKPT_MODE_APPEND	CKPT_APPEND
#define	CKPT_MODE_IGNORE	CKPT_IGNORE
#define CKPT_MODE_RELOCATE      CKPT_RELOCATE


/*
 * Per proc per fd entry 
 * purpose is to allow mmap/execmap to use existing fd if one is available for 
 * a file.  Required due to file locking concerns (closing a file removes
 * file locks, even if proc has another open of the file)
 */
typedef struct ckpt_proc_fd {
	int	pfd_fd;		/* open fd */
	ushort	pfd_fflags;	/* open mode flags */
	dev_t	pfd_dev;	/* device */
	ino_t	pfd_ino;	/* inode */
} ckpt_pfd_t;

/*
 * Unlink file data struct used during restart to identify files that should
 * be unlinked
 */
typedef struct ckpt_unlink {
	ckpt_magic_t		ul_magic;		/* CKPT_MAGIC_UNLINKED */
	char			ul_path[CPATHLEN];	/* pathname to unlink */
	struct ckpt_unlink	*ul_next;
} ckpt_ul_t;

/*
 * Open pipe fd header saved in each CPR targeted process's statefile
 */
typedef struct ckpt_pipe {
	ckpt_magic_t cp_magic;
	int	cp_fd;			/* file descriptor */
	uchar_t	cp_flags;		/* flags, see below */
	int	cp_fdflags;		/* fd flags ala fcntl */
	pid_t	cp_duppid;		/* pid to dup from */
	int	cp_dupfd;		/* fd to dup from */
	int	cp_protofd[2];		/* prototype pipe fds */
	caddr_t	cp_fdid;		/* identifier for which end of pipe */
	dev_t	cp_dev;			/* dev # for identifying pipe */
	ino_t	cp_ino;			/* ino # for identifying pipe */
	size_t	cp_size;		/* # bytes buffered in kernel */
	ckpt_str_t cp_stropts;		/* streams options */
	char	cp_path[PATH_MAX];	/* name of file storing pipe data */
	struct ckpt_pipe *cp_next;	/* used for restart */
} ckpt_p_t;

/*
 * Flags
 */
#define	CKPT_PIPE_HANDLED	0x1
/*
 * Pipe instance data structure.
 *
 * Dynamically allocated as we discover a new pipe instance (identified by
 * dev, ino pair).
 *
 * Maps pipes to file descriptors.
 */
typedef struct ckpt_pipemap {
	struct ckpt_pipemap *cpo_link;	/* link to next pipe */
	dev_t	cpo_dev;		/* original pipe dev */
	ino_t	cpo_ino;		/* original pipe ino */
	pid_t	cpo_pid;		/* pid of handling process */
	struct {
		uchar_t	flags;		/* flags, see below */
		int	fd;		/* pipefd */
		caddr_t	id;		/* fd identifier */
	}	cpo_inst[2];
} ckpt_po_t;
/*
 * Flags
 */
#define	CKPT_PIPE_DUMPED	0x1	/* pipe instance has been dumped */

/*
 * Defines for svr3 instances 
 */
#define	PIPEREAD	0
#define PIPEWRITE	1

/*
 * Macros
 */
#define SVR4PIPEMATE(p)	((p)^1)

/*
 * Define and structs for children
 */
typedef struct ckpt_childinfo {
	ckpt_magic_t	ci_magic;		/* CKPT_MAGIC_CHILDINFO */
	int		ci_nchild;		/* number of children or trees */
} ckpt_ci_t;

/*
 * Defines and structs for signal state
 */
/*
 * Process signal state
 */
typedef struct ckpt_sig {
	ckpt_magic_t	cs_magic;	/* CKPT_MAGIC_THRDSIGINFO */
	int		cs_nsigs;	/* Number of signals */
	int		cs_ninfo;	/* Number of info structures */
	struct sigaltstack cs_altstack;	/* alternate stack info */
	sigset_t	cs_pending;	/* proc async signals */
	caddr_t		cs_sigtramp;	/* process trampoline address */
} ckpt_sig_t;

/*
 * Thread signal state
 */
typedef struct ckpt_thread_sig {
	ckpt_magic_t	cts_magic;	/* CKPT_MAGIC_THRDSIG */
	short		cts_tid;	/* thread id */
	short		cts_cursig;	/* current signal */
	int		cts_ninfo;	/* # of info structs */
	sigset_t	cts_pending;	/* pending signals */
} ckpt_thread_sig_t;
/*
 * Defines and structs for memory image
 */

/*
 * Attribute segment
 */
/*
 * checkpoint rev 1.2 version
 */
typedef struct ckpt_seg {
	void		*cs_vaddr;	/* virtual address */
	ulong_t		cs_len;		/* length (in bytes) */
	uint_t		cs_prots;	/* page protections */
	ushort_t	cs_lockcnt;	/* lock count */
	uchar_t		cs_notcached;	/* not cacheable */
	pmo_handle_t	cs_pmhandle;	/* policy module handle */
} ckpt_seg_t;

#define	CKPT_NONCACHED		0x1	/* regular non-cached */
#define	CKPT_NONCACHED_FETCHOP	0x2	/* non-cached fetchop */

/*
 * Pregion info
 */
typedef struct ckpt_memmap {
	ckpt_magic_t	cm_magic;		/* CKPT_MEMAPMAGIC */
	caddr_t		cm_mapid;		/* unique map id */
	void		*cm_vaddr;		/* virtual address */
	size_t		cm_len;			/* length (in bytes) */
	int		cm_maxprots;		/* original protections */
	uint		cm_mflags;		/* mapping flags */
	uint		cm_flags;		/* flags, from ckpt_getmap_t */
	uint		cm_xflags;		/* extended flags */
	uint		cm_cflags;		/* checkpoint flags */
	int		cm_nsegs;		/* number of segments */
	long		cm_nmodmin;		/* min # of mod'd pages */
	union {
		/*
		 * The next 2 fields are mutully exclusive.  One
		 * is used for save-modified, the other for save-all
		 */
		long	u_nmod;			/* number of modified pages */
		size_t	u_savelen;		/* length actually saved */
	}		cm_u;
	caddr_t		*cm_modlist;		/* list of modified addresses */
	int		cm_pagesize;		/* page size */
	pid_t		cm_pid;			/* pid that must map */
	tid_t		cm_tid;			/* for private maps, what tid */
	dev_t		cm_rdev;		/* dev, for spec file maps */
	int		cm_shmid;		/* shmid, for sysV shm */
	off_t		cm_foff;		/* file offset */
	off_t		cm_maxoff;		/* max file offset */
	void		*cm_auxptr;		/* auxilliary attributes */
	size_t		cm_auxsize;		/* size of auxilliary attrs */
	int		cm_fd;			/* fd to use for mapping */
	int		cm_spare[4];		/* spares */
	char		cm_path[PATH_MAX];	/* mapped file path name */
} ckpt_memmap_t;

#define	cm_nmod		cm_u.u_nmod
#define	cm_savelen	cm_u.u_savelen

/*
 * Extended flags
 */
#define	CKPT_BREAK	0x001
#define	CKPT_SHMEM	0x002
#define	CKPT_UNLINKED	0x004
#define	CKPT_PHYS	0x008		/* physical device mapping */
#define	CKPT_SKIP	0x010		/* skip this one */
#define	CKPT_SPECIAL	0x020		/* requires special handling */
#define	CKPT_VNODE	0x040		/* has a vnode */
#define	CKPT_PRATTACH	0x080		/* was prattach'd */
#define	CKPT_ISMEM	0x100		/* inherited shared memory */
#define	CKPT_FETCHOP	0x200		/* mapped for fetchop */
/*
 * Checkpoint flags
 */
#define	CKPT_SAVEALL	0x01		/* save all pages */
#define	CKPT_SAVEMOD	0x02		/* save modified pages */
#define	CKPT_REMAP	0x04		/* remap file at restart */
#define	CKPT_ANON	0x08		/* map /dev/zero at restart */
#define	CKPT_SAVESPEC	0x10		/* save special info */
#define	CKPT_DEPENDENT	0x20		/* another proc is restoring */

#define	CKPT_REFCNT(prmap)	((prmap)->pr_mflags >> MA_REFCNT_SHIFT)

/*
 * Dependency struct...identify 1 procs depndency on another
 */
          
/* dependency...disk version */
typedef struct ckpt_depend {
	ckpt_magic_t	dp_magic;		/* CKPT_MAGIC_DEPEND */
        pid_t   	dp_spid;                /* source pid */
        pid_t   	dp_dpid;                /* dependent pid */
} ckpt_depend_t;

/* dependency...memory version */
#define CKPT_DPLCHUNK   10
typedef struct ckpt_dplist {
        ckpt_depend_t           dpl_depend[CKPT_DPLCHUNK];
        int                     dpl_count;
        struct ckpt_dplist      *dpl_next;
} ckpt_dpl_t;

/*
 * Inherited shared memory handling (/dev/zero MAP_SHARED)
 */
typedef struct ckpt_ismmem {
        caddr_t                 ism_mapid;      /* mem unique id */
        pid_t                   ism_pid;        /* source pid */
        caddr_t                 ism_vaddr;      /* source vaddr */
        size_t                  ism_len;        /* length */
        struct ckpt_ismmem      *ism_next;
} ckpt_ismmem_t;

/*
 * Data struct for tracking PR_ATTACHADDR memory
 */
typedef struct ckpt_prattach {
	ckpt_magic_t		pr_magic;	/* CKPT_MAGIC_ATTACH */
	caddr_t			pr_mapid;	/* mem unique id */
	void			*pr_shaddr;	/* shared addr id */
	caddr_t			pr_vaddr;	/* owners vaddr */
	pid_t			pr_owner;	/* pid claiming ownership */
	pid_t			pr_attach;	/* pid of attaching proc */
	caddr_t			pr_localvaddr;	/* attached address */
	int			pr_obsolete;	/* OBSOLETE...used in rev */
						/* 1.0, ignored in 1.1 */
						/* retained to avoid having */
						/* to xlate 1.0 structs */
	int			pr_prots;	/* attached prots */
	struct ckpt_prattach	*pr_next;
} ckpt_prattach_t;
/*
 * Data structs for NUMA support
 */
typedef struct ckpt_pmo_default {
	ckpt_magic_t	pmo_magic;	/* CKPT_MAGIC_PMDEFAULT */
	mem_type_t	pmo_memtype;	/* memory type */
	pmo_handle_t	pmo_handle;	/* policy module handle */
} ckpt_pmo_default_t;

typedef struct ckpt_pmo {
	ckpt_magic_t	pmo_magic;	/* CKPT_MAGIC_PMODULE */
	pmo_handle_t	pmo_handle;	/* handle */
	pm_info_t	pmo_info;	/* policy set parameters */
	pmo_handle_t	pmo_pmo;	/* policy module placement handle */
} ckpt_pmo_t;

typedef struct ckpt_mld {
	ckpt_magic_t	mld_magic;		/* CKPT_MAGIC_MLD */
	pmo_handle_t	mld_handle;		/* mld handle */
	ckpt_mldinfo_t	mld_info;		/* mld state */
	char		mld_hwpath[PATH_MAX];	/* hwgraph pathname */
} ckpt_mld_t;

typedef struct ckpt_mldset {
	ckpt_magic_t	mldset_magic;	/* CKPT_MAGIC_MLDSET */
	pmo_handle_t	mldset_handle;
	int		mldset_count;	/* # of mld handles to follow */
	topology_type_t	mldset_topology; /* topology, if placed */
	rqmode_t	mldset_rqmode;	/* mandatory/advisory */
	int		mldset_raffcnt;	/* # of raff entries to follow */
} ckpt_mldset_t;

/*
 * page migration info for shared pages.
 */
typedef struct ckpt_pmilist {
	ckpt_magic_t	pmi_magic;	/* CKPT_MAGIC_PMI */
	pid_t		pmi_owner;	/* pid that own tis */
	caddr_t		pmi_base;	/* base addr for the segment */
	caddr_t		pmi_vaddr;	/* starting vaddr for this part */
	caddr_t		pmi_eaddr;	/* end addr */
	cnodeid_t	pmi_nodeid;	/* nodeid for this */
	pmo_handle_t	pmi_phandle;	/* handle used by */
	struct ckpt_pmilist *pmi_next;	/* next */
} ckpt_pmilist_t;

/*
 * shared state file struct for process_mldlink
 *
 * used when a process is linked to an mld *not* in it's own namespace
 */
typedef struct ckpt_mldlink {
	ckpt_magic_t	mlink_magic;	/* CKPT_MAGIC_MLDLINK */
	pid_t		mlink_owner;	/* pid that "owns" mld */
	pmo_handle_t	mlink_mld;	/* handle for mld */
	pid_t		mlink_link;	/* linked pid */
} ckpt_mldlink_t;

/*
 * supporting data struct for assembling the above during checkpoint
 */
typedef struct ckpt_mldlist {
	pid_t			mld_owner;	/* pid that "owns" mld */
	pmo_handle_t		mld_handle;	/* mld handle */
	caddr_t			mld_id;		/* unique system wide mld id */
	int			mld_link_proc;	/* proc linked to this mld */
	int			mld_nodeid;	/* cnode of this mld */	
	struct ckpt_mldlist	*mld_next;	/* next pointer */
} ckpt_mldlist_t;

typedef struct ckpt_mldlog {
	int			ml_count;      /* # of mld handles this entry */
	pmo_handle_t		*ml_list;	/* list of mld handles */
	struct ckpt_mldlog	*ml_next;	/* next in chain */
} ckpt_mldlog_t;

typedef struct ckpt_pmolist_s {
	pmo_handle_t		pmo_handle;
	char			pmo_name[PM_NAME_SIZE + 1];
	struct ckpt_pmolist_s	*pmo_next;
} ckpt_pmolist_t;

/*
 * Data structure for tracking Posix unamed semaphores
 *
 * These are identified by user virtual address
 */
typedef struct ckpt_pusema {
	ckpt_magic_t	pu_magic;	/* CKPT_MAGIC_PUSEMA */
	pid_t		pu_owner;	/* pid whose addrspace we found it in */
	caddr_t		pu_uvaddr;	/* user virtual address */
	usync_ckpt_t	pu_usync;	/* semaphore state */
} ckpt_pusema_t;

typedef struct ckpt_pusema_list {
	ckpt_pusema_t		pul_pusema;
	caddr_t			pul_mapid;
	struct ckpt_pusema_list	*pul_next;
} ckpt_pul_t;

/*
 * Process memory object
 */
typedef struct ckpt_memobj {
	ckpt_magic_t	cmo_magic;		/* CKPT_MAGIC_MEMOBJ */
	caddr_t		cmo_brkbase;
	size_t		cmo_brksize;
	caddr_t		cmo_stackbase;
	size_t		cmo_stacksize;
	int		cmo_nmaps;
	int		cmo_obsolete0;
	ckpt_memmap_t	*cmo_obsolete1;
	ckpt_seg_t	**cmo_obsolete2;
} ckpt_memobj_t;

#define	FOR_EACH_PROC(first, co)	for (co = first; co; co = co->co_next)

#define CKPT_MAXRESERVE	(caddr_t)0x400000L	/* max reserved ckpt address */
/* librestart text starts at 0x3b0000L */
#ifdef DEBUG_FDS
#define	CKPT_SFDRESERVE	(caddr_t)0x320000L	/* sfd list */
#endif
#define	CKPT_SIDRESERVE	(caddr_t)0x31c000L	/* shared id list */
#define CKPT_PLRESERVE	(caddr_t)0x318000L	/* proc list */
#define CKPT_SHMRESERVE (caddr_t)0x314000L      /* shared mem id map list */
#define CKPT_SADDRRSRV  (caddr_t)0x310000L      /* sproc shared addr list */
#define CKPT_PTHRDRSRV  CKPT_SADDRRSRV          /* pthread addr list */
						/* pthreads & sprocs mutually */
						/* exclusive */
#define	CKPT_STKRESERVE	(caddr_t)0x310000L	/* ckpt stack base */
#define	CKPT_MINRESERVE	(caddr_t)0x300000L	/* min addr reserved for ckpt */
#define	IS_CKPTRESERVED(V)	\
	(((caddr_t)(V) >= CKPT_MINRESERVE)&&((caddr_t)(V) < CKPT_MAXRESERVE))

/*
 * pidlist is created to keep track of pid's at restart time. We may not get the
 * exact pid's at restart because of possible duplication.
 */
typedef struct pidlist {
	pid_t	pl_pid;			/* process pid (the new physical pid) */
	pid_t	pl_opid;		/* original pid (virtual pid) */
	uint_t	pl_states;		/* process states */
	int	pl_prfd;		/* /proc fd */
	int	pl_errno;		/* errno, in case of error */
} ckpt_pl_t;
/*
 * pl_status values
 */
#define	CKPT_PL_CREATE		0x01	/* process has been created */
#define	CKPT_PL_DONE		0x02	/* process is ready to go */
#define	CKPT_PL_ERR		0x04	/* process terminated in error */
#define	CKPT_PL_FIRSTSP		0x08	/* restore root for this proc */
#define	CKPT_PL_ZOMBIE		0x10	/* proc is becoming a zombie */
#define	CKPT_PL_RESTARTWARN	0x20	/* proc has restart signal */

/*
 * main handle per restart request
 */
typedef struct ckpt_restart {
	ckpt_pci_t	cr_pci;		/* per-cpr info */
	pid_t		*cr_roots;	/* the tree roots */
	int		cr_ifd;		/* fd for the per-cpr index file */
	int		cr_sfd;		/* fd for the per-cpr state file */
	int		cr_rsfd;	/* recieve status fd */
	int		cr_ssfd;	/* send status fd */
	ckpt_prop_t	*cr_index;	/* property index cache */
	pid_t		cr_waitor;	/* waitor process */
	uid_t		cr_ruid;        /* real uid of restarter */
	int		cr_handle_count;	/* no. of share prop handles */
	int		cr_procprop_count;	/* no. of proc prop handles */
	char		cr_path[CPATHLEN];	/* statefile directory path */
} cr_t;



/*
 * Argument structure for creating restarted process
 */
typedef struct targargs {
	cr_t		*crp;		/* used by librestart */
	unsigned long	pindex;		/* proclist index */
	int		ifd;		/* indexfile fd */
	int		sfd;		/* statefile fd */
	int		mfd;		/* memory image fd */
	int		errfd;		/* stderr fd */
	int		sifd;		/* shared (per-cpr) index file */
	int		ssfd;		/* shared (per-cpr) state file */
	int		cprfd;		/* cpr proc fd */
	tid_t		tid;		/* pthread id */
	ckpt_ppi_t	ixheader;	/* indexfile header */
	ckpt_prop_t	*index;		/* property index cache */
	ckpt_phandle_t	*share_prop_tab;/* low memory share prop table */
	ckpt_phandle_t	*proc_prop_tab;	/* low memory proc prop table */
	int		nchild;		/* # of children to create */
	int		blkcnt;		/* block count to set */
	pid_t		*cpid;		/* child pid list */
	ckpt_pi_t	pi;		/* proc info */
	ckpt_sig_t	sighdr;		/* signal header */
	siginfo_t	*sip;		/* queued siginfo */
	sigaction_t	*sap;		/* signal action */
	ckpt_p_t	*pp;		/* pipe header */
	ckpt_po_t	*pipeobj;	/* pipe object */
	ckpt_pmo_t	*pmo;		/* pointer to policy module */
	ckpt_memobj_t	memobj;		/* memory object */
	off_t		memmap_offset;	/* memmap offset in statefile */
	char		procpath[32];	/* proc path for the restored proc */
} ckpt_ta_t;

/*
 * Data structs for saving sprocs
 */
/*
 * Sproc data struct.  Used to track handling of shared address space and
 * to verify all members being checkpointed
 */
typedef struct ckpt_sproc {
	struct ckpt_sproc *sp_next;	/* next in chain */
	void		  *sp_sproc;	/* sproc id */
	unsigned	  sp_flags;	/* flags (see below) */
	pid_t		  sp_pid;	/* pid for id purposes */
	int		  sp_refcnt;	/* shaddr refcnt */
	int		  sp_proccnt;	/* # procs reffing shaddr found */
} ckpt_sp_t;

#define	CKPT_SPROC_SADDR	0x01	/* saddr handled */
#define	CKPT_SPROC_PMDEFAULTS	0x02	/* policy module defaults */
#define	CKPT_SPROC_PMODULES	0x04	/* policy modules */
#define	CKPT_SPROC_MLDS		0x08	/* memory locality domains */
#define	CKPT_SPROC_MLDSET	0x10	/* memory locality domain sets */
#define CKPT_PM_HANDLE_BASE	2	/* PMO Handle offset */
/*
 * Data structs for restoration of sprocs
 */
/* C library mutual exclusion */
typedef struct liblock {
	abilock_t	ll_spinlock;
	pid_t		ll_owner;
	int		ll_count;	/* nested lock count */
	pid_t		*ll_waiters;
	int		ll_nwaiters;	/* # procs waiting */
	int		ll_next;	/* head of wait list */
	int		ll_ffree;	/* first free wait slot */
	int		ll_nentry;	/* # entries in list */
} ckpt_liblock_t;
/*
 * librestart mutex
 */
typedef struct {
	unsigned long	lock;		/* spinlock */
	int		busy;		/* is mutex busy */
	int		waiters;	/* number of waiters */
} ckpt_mutex_t;

#define	CKPT_MUTEX_INIT(mp)	\
	(mp)->lock = 0;	\
	(mp)->busy = 0;	\
	(mp)->waiters = 0
	
/* shared address */
typedef struct saddrlist {
	ckpt_liblock_t	*saddr_liblock;	/* C lib mutual exclusion */
	unsigned long	saddr_count;	/* number of addr sharing members */
	unsigned long	saddr_sync;	/* number at sync point */
	size_t		saddr_size;	/* size, for unmapping */
	pid_t		*saddr_pid;	/* pids of addr sharing members */
} ckpt_saddr_t;

/* shared fds */
typedef struct sfdlist {
	unsigned long	sfd_count;	/* number of fd sharing members */
	unsigned long	sfd_sync;	/* number at sync point */
	size_t		sfd_size;	/* for unmapping */
	pid_t		*sfd_pid;	/* pids of all procs sharing fds */
} ckpt_sfd_t;

/* shared ids */
typedef struct sidlist {
	ckpt_mutex_t	sid_uidlock;	/* lock word for uid change sync */
	unsigned long	sid_count;	/* number of id sharing members */
	unsigned long	sid_sync;	/* # at sync point */
	size_t		sid_size;	/* for unmapping */
	pid_t		sid_assigned;	/* share member to change ids */
	pid_t		*sid_pid;	/* pids of all procs sharing ids */
} ckpt_sid_t;

/* shared dirs */
typedef struct sdirlist {
	unsigned long	sdir_count;	/* number of dir sharing members */
	unsigned long	sdir_csync;	/* # at cwd sync point */
	size_t		sdir_size;	/* for unmapping */
	pid_t		*sdir_pid;	/* pids of blocked procs */
} ckpt_sdir_t;

/*
 * CCSYNC data struct
 */
#define EVC_OFFSET      0x100
typedef struct ckpt_ccsync {
	int		cc_fd;		/* file fd to map (-1 if fd closed) */
	ccsyncinfo_t    cc_info;	/* dev info */
} ckpt_ccsync_t;

/*
 * SVR4 shared mem structs
 */
typedef struct ckpt_shm {
        caddr_t         shm_mapid;      /* shm map identifier */
	int		shm_id;		/* shmid */
        pid_t           shm_pid;        /* handling pid */
        struct ckpt_shm *shm_next;
} ckpt_shm_t;

typedef struct ckpt_shmlist {
        ulong_t cshm_count;             /* number of shared mem segs */
        struct ckpt_shmmap_s {
                caddr_t mapid;          /* original mapid */
                int     shmid;          /* restarted shmid */
                int     rmid;           /* remove after restart? */
        } *cshm_map;
} ckpt_shmlist_t;

typedef struct ckpt_shmobj {
	ckpt_magic_t	shm_magic;		/* CKPT_MAGIC_MEMOBJ */
	int		shm_id;			/* segment id */
	char		shm_path[PATH_MAX];	/* memory image */
	struct shmid_ds	shm_ds;			/* info about segment */
} ckpt_shmobj_t;

struct strmap {
	int		id;
	char		*s_id;
};

/*
 * One of these attached to ckpt_obj_t for each thread
 */
typedef struct ckpt_thread {
	int		ct_id;		/* thread id */
	unsigned int	ct_flags;	/* thread ckptlags */
	ckpt_ctxt_t	ct_ctxt;	/* context (register sets) */
	ckpt_uti_t	ct_uti;		/* info (sched, block count, ...) */
	int		ct_nmemmap;	/* number of thread memory maps */
	ckpt_memmap_t	*ct_memmap;	/* thread memory maps */
	ckpt_seg_t	**ct_mapseg;	/* map segments */
	struct ckpt_thread *ct_next;	/* next ptr */
} ckpt_thread_t;

#define	CKPT_THREAD_VALID	0x01	/* thread is valid */
#define	CKPT_THREAD_WARN	0x02	/* thread has been ckpt warned */
#define	CKPT_THREAD_WARNRET	0x04	/* thread has returnd from ckpt warn */
#define	CKPT_THREAD_CTXT	0x08	/* have thread context */
#define	CKPT_THREAD_REISSUE	0x10	/* thread is reissuing syscall */
/*
 * Basic CPR object per process
 */
typedef struct ckpt_obj {
	struct ckpt_obj	*co_next;	/* next in list */
	struct ckpt_obj	*co_parent;	/* pointer to parent, if ckpt'ing it */
	struct ckpt_obj	*co_children;	/* pointer to children, if ckpt'ing */
	struct ckpt_obj	*co_sibling;	/* pointer to siblings, if ckpt'ing */
	pid_t		co_pid;		/* proc id */
	int		co_nchild;	/* number of children */
	int		co_prfd;	/* procfs fd */
	int		co_ifd;		/* fd for the index file */
	int		co_mfd;		/* fd for the memory image */
	int		co_sfd;		/* fd for the state file */
	int		co_nprops;	/* no. of properties saved */
	uint_t		co_flags;	/* flags, see below */
	int		co_nsigs;	/* number of signals */
	int		co_nofiles;	/* # of open files (used for GUI only */
	prpsinfo_t	*co_psinfo;	/* process info (PIOCPSINFO) */
	ckpt_pi_t	*co_pinfo;	/* a link to the proc info header */
	sigaction_t	*co_sap;	/* link to proc sigaction info */
	ckpt_pfd_t	*co_pfd;	/* pointer to proc fd table */
	int		co_pfdidx;	/* index of current open fd */
	ckpt_thread_t	*co_thread;	/* pointer to list of threads */
	ckpt_memobj_t	co_memobj;	/* memory info main header */
	ckpt_mldlog_t	*co_mldlog;	/* log of mlds in mldsets */
	struct ckpt_handle *co_ch;	/* pointer back to the handle */
} ckpt_obj_t;

/*
 * ckpt_obj_t flags
 */
#define	CKPT_CO_WARN		0x001	/* send checkpoint warning */
#define	CKPT_CO_SPROCFD_HANDLED	0x004	/* a handled fd sharing sproc */
#define	CKPT_CO_ZOPEN		0x008	/* zombie open */
#define	CKPT_CO_ZOMBIE		0x010	/* is a zombie */
#define	CKPT_CO_STOPPED		0x020	/* proc has been stopped */
#define	CKPT_CO_EXITED		0x040	/* exited between open & stop */
#define	CKPT_CO_CTXT		0x080	/* fetched procs context */
#define	CKPT_CO_SADDR		0x100	/* sproc handling shared address */
					/* space */

/*
 * sproc macros related to ckpt_obj_t
 */
#undef	IS_SPROC	/* don't want kernels idea of this */
#define IS_SPROC(P)	((P)->cp_psi.ps_shaddr)
#define IS_SADDR(P)	((P)->cp_psi.ps_shmask & PR_SADDR)
#define IS_SFDS(P)	((P)->cp_psi.ps_shmask & PR_SFDS)
#define IS_SID(P)	((P)->cp_psi.ps_shmask & PR_SID)
#define	IS_SDIR(P)	((P)->cp_psi.ps_shmask & PR_SDIR)

/*
 * array session ckpt
 */

#define CKPT_IS_GASH(type,flags) \
	(CKPT_IS_ASH(type)&& (!(flags&CKPT_PCI_LOCALASH)))

#define CKPT_IS_ARSERVER(type,flags) \
	(CKPT_IS_GASH(type,flags) && !((type) & CKPT_TYPE_ARCLIENT))

/*
 * definitions for ch_flags and pci_flags
 */
#define	CKPT_PCI_ABORT			0x1	/* CPR fatal error; abort */
#define	CKPT_PCI_DISTRIBUTE		0x2	/* distribute files */
#define	CKPT_PCI_UNCACHEABLE		0x4
#define	CKPT_PCI_FORKANY		0x8	/* user doesn't care the orig. pid */
#define	CKPT_PCI_INTERACTIVE		0x10	/* interactive job */
#define CKPT_PCI_DEFAULTCWD             0x20    /* use default cwd */
#define CKPT_PCI_DEFAULTROOT            0x40    /* use default root */
#define	CKPT_PCI_CREATE			0x80	/* statefile was created */
#define CKPT_PCI_LOCALASH		0x100	/* all ash's in local mach */

/*
 * SN0 fetchop defines
 */
#define FETCHOP_VAR_SIZE        64
/*
 * main handle per checkpoint request
 */
typedef struct ckpt_handle {
	ckpt_obj_t	*ch_first;	/* first obj of the proc list */
	ckpt_obj_t	**ch_ptrees;	/* a list of tree roots */
	ckpt_id_t	ch_id;		/* can be pid, gid, sid, or arsid */
	int		ch_nproc;	/* number of procsses checkpointed */
	int		ch_ntrees;	/* number of proc hierarchies */
	ckpt_type_t	ch_type;	/* POSIX defs of types (ckpt_type_t) */
	int		ch_propoffset;	/* current global prop offset */
	int		ch_ifd;		/* fd for the per-cpr index file */
	int		ch_sfd;		/* fd for the per-cpr state file */
	int		ch_nprops;	/* no. of properties saved */
	int		ch_flags;	/* various misc per-cpr states */
	ckpt_flist_t 	*ch_filelist;	/* per-cpr openfile list */
	ckpt_po_t 	*ch_pipelist;	/* per-cpr openpipe list */
	ckpt_sp_t	*ch_sproclist;	/* per-cpr shared fd sproc list */
	ckpt_ul_t	*ch_ulp;	/* per-cpr unlinked file list */
	ckpt_mldlist_t	*ch_mldlist;	/* per-cpr mld link list */
	ckpt_pmolist_t	*ch_pmolist;	/* per-cpr pmo link list */
	ckpt_pmilist_t	*ch_pmilist;	/* per-cpr pmi (pm seg replace) list */
	ckpt_dpl_t	ch_depend;	/* per-cpr dependency list */
	ckpt_prattach_t	*ch_prattach;	/* per-cpr prattached memory list */
	ckpt_pul_t	*ch_pusema;	/* per-cpr unamed sema list */
	char		ch_path[CPATHLEN];	/* statefile directory path */
	char		ch_relocdir[CPATHLEN];  /* relocate directory path */
} ch_t;

typedef uid_t nqe_t;

#define	CKPT_CLOSE_AND_RETURN(fd, rc)	{close(fd); return (rc);}
/*
 * External defs
 */
extern int ksigaction(int , const struct sigaction *, struct sigaction *,
	void (*)(int ,...));
extern int xstat(int, const char *, struct stat *);
extern int procblk(unsigned int, pid_t, int);

extern int ckpt_run_checkpoint_arserver(const char *, ash_t, u_long,
	struct ckpt_args *[], size_t);
extern int ckpt_run_restart_arserver(const char *, ckpt_id_t);
extern int ckpt_pathname(ckpt_obj_t *, int, char **);
extern int ckpt_create_statefile(ch_t *);
extern int ckpt_attr_get_mode(ch_t *, char *, dev_t, ino_t, mode_t, int, int);
extern pid_t ckpt_file_is_handled(ckpt_obj_t *, dev_t, ino_t, int, caddr_t);
extern int ckpt_exam_fdsharing_sproc(ckpt_obj_t *);
extern ckpt_flist_t *ckpt_file_lookup(dev_t dev, ino_t ino, int *found);
extern int ckpt_dump_one_ofile(ckpt_f_t *, int);
extern int ckpt_sfd_sync(ckpt_pi_t *);
extern int ckpt_move_fd(int *, int);
extern int ckpt_get_private_fds(ckpt_ta_t *, cr_t *);
extern int ckpt_close_inherited_fds(ckpt_ta_t *, cr_t *, pid_t, int);
extern int ckpt_restore_one_ofile(ckpt_ta_t *, ckpt_f_t *);
extern void ckpt_unlink_files(ckpt_ul_t *);
extern int ckpt_continue_proc(ckpt_obj_t *, int);
extern int ckpt_continue_procs(ckpt_obj_t *, int);
extern int ckpt_attr_use_default(ch_t *, int, int);
extern void ckpt_setpaths(ckpt_obj_t *, ckpt_f_t *, char *, dev_t, ino_t, 
				mode_t, pid_t, int);
extern void ckpt_set_shmpath(ckpt_obj_t *, char *, int);
extern void ckpt_setmempath(char *, ckpt_f_t *);
extern void ckpt_setpipepath(ckpt_p_t *, const char *, int);
extern ckpt_po_t *ckpt_pipe_lookup(dev_t , ino_t);
extern ckpt_po_t *ckpt_pipe_add(ckpt_obj_t *, ckpt_p_t *, int *);
extern void ckpt_pipe_cleanup(ckpt_ta_t *);
extern int ckpt_save_svr3fifo_data(char *, int);
extern int ckpt_save_svr4fifo_data(char *, int);
extern int ckpt_restore_pipe_data(char *, int, int, int);
extern int ckpt_restore_pipefds(ckpt_ta_t *);
extern void ckpt_get_flocks(ckpt_f_t *, int, pid_t);
extern int ckpt_perm_check(ckpt_obj_t *);
extern int ckpt_check_directory(const char *);
extern int ckpt_restart_perm_check1(uid_t, gid_t);
extern int ckpt_restart_perm_check2(ckpt_pi_t *, int);
extern int ckpt_load_target_info(ckpt_ta_t *, pid_t);
extern int ckpt_load_shareindex(const char *);
extern int ckpt_myself(ckpt_obj_t *);
extern int ckpt_stop_procs(ch_t *);
extern void ckpt_signal_procs(ckpt_obj_t *, int);
extern int ckpt_create_proc_list(ch_t *);
extern int ckpt_update_proc_list(ch_t *);
extern char *ckpt_get_dirname(ckpt_obj_t *, int);
extern int ckpt_saddr_handled(ckpt_obj_t *);
extern int ckpt_pmdefaults_handled(ckpt_obj_t *);
extern int ckpt_pmodules_handled(ckpt_obj_t *);
extern int ckpt_mlds_handled(ckpt_obj_t *);
extern int ckpt_mldset_handled(ckpt_obj_t *);
extern int ckpt_sproc_block(pid_t, pid_t *, unsigned long, unsigned long *);
extern int ckpt_sproc_unblock(pid_t, unsigned long, pid_t *);
extern int ckpt_get_localpids(ckpt_id_t, pid_t **, int *);
extern int ckpt_fdattr_special(ckpt_obj_t *, ckpt_f_t *, dev_t, ino_t, int);
extern int ckpt_xlate_fdattr_special(ckpt_f_t *, int);
extern int ckpt_restore_fdattr_special(ckpt_f_t *, int);
extern int ckpt_mapattr_special(ckpt_obj_t *, ch_t *, ckpt_memmap_t *,
	struct stat *);
extern int ckpt_handle_mapspecial(ckpt_obj_t *, ch_t *, ckpt_memmap_t *);
extern int ckpt_remap_disp_special(ckpt_memmap_t *);
extern int ckpt_save_all(ckpt_obj_t *, ckpt_thread_t *, ckpt_memmap_t *);
extern int ckpt_save_special(ckpt_obj_t *, ckpt_memmap_t *, ckpt_seg_t *);
extern void ckpt_warn_unsupported(ckpt_obj_t *, ckpt_f_t *, int, void *);
extern void ckpt_catch_alarm();
extern void cdebug(const char *, ...);
extern ckpt_prattach_t *ckpt_get_prattach(ckpt_prattach_t *);
extern int ckpt_resolve_prattach(ch_t *);
extern ckpt_dpl_t *ckpt_get_depend(ckpt_dpl_t *);
extern ckpt_pul_t *ckpt_get_pusema(ckpt_pul_t *);
extern int ckpt_is_special(dev_t);

extern ckpt_prop_t *ckpt_find_property(ckpt_ppi_t *, ckpt_prop_t *, ckpt_magic_t);
extern int ckpt_write_propindex(int, ckpt_prop_t *, int *, int *);
extern int ckpt_write_share_properties(ch_t *);
extern int ckpt_write_pci(ch_t *);
extern int ckpt_write_proc_properties(ckpt_obj_t *);
extern int ckpt_write_one_share_prop	(ch_t *, ckpt_magic_t);
extern int ckpt_write_treeinfo		(ch_t *, ckpt_prop_t *);
extern int ckpt_write_prattach		(ch_t *, ckpt_prop_t *);
extern int ckpt_write_depend		(ch_t *, ckpt_prop_t *);
extern int ckpt_write_pusema		(ch_t *, ckpt_prop_t *);
extern int ckpt_save_mldlink		(ch_t *, ckpt_prop_t *);
extern int ckpt_save_hinv		(ch_t *, ckpt_prop_t *);
extern int ckpt_save_pmi		(ch_t *, ckpt_prop_t *);
extern int ckpt_write_childinfo		(ckpt_obj_t *, ckpt_prop_t *);
extern int ckpt_write_procinfo		(ckpt_obj_t *, ckpt_prop_t *);
extern int ckpt_write_context		(ckpt_obj_t *, ckpt_prop_t *);
extern int ckpt_write_siginfo		(ckpt_obj_t *, ckpt_prop_t *);
extern int ckpt_write_openpipes		(ckpt_obj_t *, ckpt_prop_t *);
extern int ckpt_write_openspecs		(ckpt_obj_t *, ckpt_prop_t *);
extern int ckpt_write_openfiles		(ckpt_obj_t *, ckpt_prop_t *);
extern int ckpt_write_fds		(ckpt_obj_t *, ckpt_prop_t *);
extern int ckpt_write_mapfiles		(ckpt_obj_t *, ckpt_prop_t *);
extern int ckpt_write_memobj		(ckpt_obj_t *, ckpt_prop_t *);
extern int ckpt_save_shm		(ckpt_obj_t *, ckpt_prop_t *);
extern int ckpt_write_thread		(ckpt_obj_t *, ckpt_prop_t *);
extern int ckpt_write_thread_siginfo	(ckpt_obj_t *, ckpt_prop_t *);
extern void ckpt_share_prop_table_fixup	(ckpt_magic_t, int (*)());
extern void ckpt_proc_prop_table_fixup	(ckpt_magic_t, int (*)());
extern int ckpt_read_share_property	(cr_t *, ckpt_ta_t *, ckpt_magic_t, int *);
extern int ckpt_read_proc_property	(ckpt_ta_t *, ckpt_magic_t);
extern int ckpt_remove_proc_property	(ckpt_ta_t *, ckpt_magic_t);
extern int ckpt_stat_proc_property	(ckpt_ta_t *, ckpt_magic_t, ckpt_stat_t *);
extern int ckpt_read_treeinfo		(cr_t *, ckpt_ta_t *, ckpt_magic_t);
extern int ckpt_restore_prattach	(cr_t *, ckpt_ta_t *, ckpt_magic_t, int *);
extern int ckpt_restore_depend		(cr_t *, ckpt_ta_t *, ckpt_magic_t, int *);
extern int ckpt_restore_pusema		(cr_t *, ckpt_ta_t *, ckpt_magic_t, int *);
extern int ckpt_restore_mldlink		(cr_t *, ckpt_ta_t *, ckpt_magic_t, int *);
extern int ckpt_read_hinv		(cr_t *, ckpt_ta_t *, ckpt_magic_t, int *);
extern int ckpt_restore_pmi		(cr_t *, ckpt_ta_t *, ckpt_magic_t, int *);
extern int ckpt_read_childinfo		(ckpt_ta_t *, ckpt_magic_t);
extern int ckpt_read_procinfo		(ckpt_ta_t *, ckpt_magic_t);
extern int ckpt_read_context		(ckpt_ta_t *, ckpt_magic_t);
extern int ckpt_read_siginfo		(ckpt_ta_t *, ckpt_magic_t);
extern int ckpt_restore_files		(ckpt_ta_t *, ckpt_magic_t);
extern int ckpt_restore_pipes		(ckpt_ta_t *, ckpt_magic_t);
extern int ckpt_restore_fds		(ckpt_ta_t *, ckpt_magic_t);
extern int ckpt_restore_memobj		(ckpt_ta_t *, ckpt_magic_t);
extern int ckpt_restore_shm		(ckpt_ta_t *, ckpt_magic_t);
extern int ckpt_read_thread		(ckpt_ta_t *, ckpt_magic_t);
extern int ckpt_read_thread_siginfo	(ckpt_ta_t *, ckpt_magic_t);
extern int ckpt_remove_files		(ckpt_ta_t *, ckpt_magic_t);
extern int ckpt_stat_procinfo		(ckpt_ta_t *, ckpt_magic_t, ckpt_stat_t *);
extern int ckpt_stat_files		(ckpt_ta_t *, ckpt_magic_t, ckpt_stat_t *);
#ifdef DEBUG_MEMOBJ
extern int ckpt_stat_memobj		(ckpt_ta_t *, ckpt_magic_t, ckpt_stat_t *);
#endif
extern int ckpt_stat_context          (ckpt_ta_t *, ckpt_magic_t, ckpt_stat_t *);
extern void ckpt_acquire_mutex		(ckpt_liblock_t *, pid_t);
extern void ckpt_release_mutex		(ckpt_liblock_t *, pid_t);
extern int ckpt_add_unlinked		(ckpt_obj_t *, char *);
extern int ckpt_write_unlinked		(ch_t *, ckpt_prop_t *);
extern int ckpt_restore_unlinked	(cr_t *, ckpt_ta_t *, ckpt_magic_t);
extern int ckpt_restore_pmdefault	(ckpt_ta_t *, ckpt_magic_t);
extern int ckpt_save_pmdefault		(ckpt_obj_t *, ckpt_prop_t *);
extern int ckpt_restore_pmodule		(ckpt_ta_t *, ckpt_magic_t);
extern int ckpt_save_pmodule		(ckpt_obj_t *, ckpt_prop_t *);
extern int ckpt_get_pmodule		(ckpt_obj_t *, ckpt_seg_t *, int );
extern int ckpt_restore_mld		(ckpt_ta_t *, ckpt_magic_t);
extern int ckpt_save_mld		(ckpt_obj_t *, ckpt_prop_t *);
extern int ckpt_restore_mldset		(ckpt_ta_t *, ckpt_magic_t);
extern int ckpt_save_mldset		(ckpt_obj_t *, ckpt_prop_t *);
extern int ckpt_license_valid		(void);
extern id_t vtoppid			(pid_t);
extern int ckpt_pfd_init		(ckpt_obj_t *, int);
extern void ckpt_pfd_add		(ckpt_obj_t *, ckpt_f_t *, dev_t, ino_t);
extern int ckpt_pfd_lookup		(ckpt_obj_t *, dev_t, ino_t, int);
extern int ckpt_display_openfiles(ckpt_obj_t *);
extern int ckpt_count_openfiles(ckpt_obj_t *);
extern int ckpt_display_mapfiles(ckpt_obj_t *, int);
extern int ckpt_close_ta_fds(ckpt_ta_t *);
extern void ckpt_get_stropts(int, ckpt_str_t *, int);
extern int ckpt_set_stropts(int, ckpt_str_t *);
extern void ckpt_get_psemavalue(int, ckpt_f_t *);
extern int ckpt_thread_ioctl(ckpt_obj_t *, tid_t, int, int, void *);
extern int ckpt_batch_scheduled(void);
extern int ckpt_allow_uncached(void);
extern char *ckpt_cpuname(int);
extern char *ckpt_boardname(int);
extern int ckpt_create_remove(const char *);
extern char *ckpt_unsupported_fdtype(ckpt_statbuf_t *);
extern int restart_ioctl(int, int, void *);
extern int restart_thread_ioctl(int, tid_t, int, void *);
extern int ckpt_get_proclist_pindex(pid_t);
extern ckpt_pl_t *ckpt_get_proclist_entry(pid_t);
extern uint_t ckpt_fetch_proclist_states(unsigned long);
extern void ckpt_update_proclist_pid(unsigned long, pid_t);
extern void ckpt_update_proclist_states(unsigned long, uint_t);
extern int ckpt_restore_sigstate(ckpt_sig_t *);
extern int ckpt_requeue_signals(ckpt_ta_t *);

extern void ckpt_pthread_wait(void);
extern void ckpt_pthread0_check(void);
extern int ckpt_restore_mappings(ckpt_ta_t *, pid_t, tid_t, int, int *, int *, int *);
extern void ckpt_stack_switch(void(*)(), ckpt_ta_t *);
extern int ckpt_seteuid(ckpt_ta_t *, uid_t *, int);
extern int ckpt_open(ckpt_ta_t *, char *, int, int);
extern int ckpt_numa_init(ckpt_ta_t *);
extern void ckpt_mldlog_free(ckpt_obj_t *);
extern char *ckpt_path_to_dirname(char *);
extern char *ckpt_path_to_basename(char *);
extern void ckpt_bump_limits(void);
extern void ckpt_restore_limits(void);
extern int ckpt_is_ash_local(void);

extern int shmget_id(key_t, size_t, int, int);
extern pid_t nsproctid(void (*)(void *, size_t), unsigned, prthread_t *,
			prsched_t *, tid_t);

extern ch_t cpr;
extern cr_t cr;

/*
 * Old define for pthreads
 */
#define PR_PTHREAD      0x04000000

#ifdef DEBUG_EBUSY
void ckpt_close_all(void);
#endif


extern void restartreturn(int, int, int);

#ifdef DEBUG
extern void ckpt_hexdump(ulong_t);
extern void chksum(int, ckpt_memmap_t *);
#endif

extern ckpt_saddr_t *saddrlist;
extern ckpt_sfd_t *sfdlist;
extern ckpt_pl_t *proclist;

#define	min(a, b)	(((a)<(b))?(a):(b))
#define	max(a, b)	(((a)>(b))?(a):(b))
/*
 * Placeholder until pthread on uthread is sorted out
 */
#define	get_mypid	getpid

#ifdef DEBUG_MEMALLOC
extern void _free(void *);
extern int touch(void *);
#define	free(X)		{(void)touch(X); _free(X); X = (void *)(-1L); }
#endif
#endif /* !__CKPT_INTERNAL_H__ */
