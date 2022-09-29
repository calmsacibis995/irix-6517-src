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

#ifndef __CKPT_REVISION_H__
#define	__CKPT_REVISION_H__

#ifdef __cplusplus
extern "C" {
#endif

#ident "$Revision: 1.8 $"

#include <sys/cred.h>
/*
 * old ckpt_procfs process info struct
 */
/*
 * ktimer_t went away in 6.5
 */
typedef struct ktimer {
        unsigned        low_bits;
        unsigned        high_bits;
        unsigned        high_bits_check;
        unsigned        tstamp;
} ktimer_t;
#define OLD_RLIM_NLIMITS 8

typedef struct old_ktimerpkg {
        ktimer_t kp_timertab[MAX_PROCTIMER];
        unsigned kp_secsnap;
        int kp_whichtimer;              /* index into kp_timertab */
} old_ktimerpkg_t;

typedef struct old_cred {
	ushort	cr_ref;			/* reference count */
	ushort	cr_ngroups;		/* number of groups in cr_groups */
	uid_t	cr_uid;			/* effective user id */
	gid_t	cr_gid;			/* effective group id */
	uid_t	cr_ruid;		/* real user id */
	gid_t	cr_rgid;		/* real group id */
	uid_t	cr_suid;		/* "saved" user id (from exec) */
	gid_t	cr_sgid;		/* "saved" group id (from exec) */
	struct mac_label *cr_mac;	/* MAC label for B1 and beyond */
	cap_set_t cr_cap;		/* capability (privilege) sets */
	gid_t	cr_groups[1];		/* supplementary group list */
} old_cred_t;

typedef struct ckpt_psinfo_old {
	caddr_t		ps_execid;	/* memory id of exec file */
	uint		ps_flag;	/* PIOCPSINFO only sends 8 bits to userland */
	uint		ps_flag2;	/* also for kthread k_flag2 */
	char		ps_acflag;	/* for accounting */
	char		ps_state;	/* proc state */
	ushort		ps_whystop;	/* again PIOCSTATUS changes the orig. values and */
	ushort		ps_whatstop;	/* caused inconsist. behavior before and after stop */
	uchar_t		ps_ckptflag;	/* process ckpt flags */
	char		ps_lock;	/* process/text locking flags */
	caddr_t		ps_brkbase;
	size_t		ps_brksize;
	caddr_t		ps_stackbase;
	size_t		ps_stacksize;
	/* signal stuff */
	sigset_t	ps_obsolete;	/* OBSOLETE...kept for binary */
					/* compatibility with 6.4-ssg */
	caddr_t		ps_sigtramp;	/* process sigtramp address */

	short           ps_xstat;	/* exit status if a zombie */
	old_cred_t	ps_cred;	/* credential info */

	/* partial accouting info missing from the ckpt_pi_t */
	u_long		ps_mem;
	u_long		ps_ioch;
	struct rusage   ps_ru;		/* stats for this proc */
        struct rusage   ps_cru;		/* sum of stats for reaped children */
	old_ktimerpkg_t	ps_timers;	/* accting states timer */
        ktimer_t        ps_ctimertab[MAX_PROCTIMER]; /*         "       */

	/*
	 * IMPORTANT NOTE:
	 *
	 * The ps_itimer field below is maintained in this data structure 
	 * in order to preserve compatibility with 6.4-ssg!!!  (To
	 * avoid bumping rev number)
	 * Once we bite the bullet and bump the rev number, this should be
	 * yanked and stored separately.
	 */
	struct itimerval ps_itimer[ITIMER_MAX]; /* alarm clock info */


	struct rlimit   ps_rlimit[OLD_RLIM_NLIMITS]; /* u: 4.3BSD resource limits */
	mode_t		ps_cmask;		/* u: mask for file creation */
	/* sproc info */
	void		*ps_shaddr;		/* is this an sproc? */
	uint		ps_shmask;		/* share group share mask */
	ushort_t	ps_shrefcnt;		/* # procs in group */
	uchar_t		ps_shflags;		/* share flags (below) */
	/* blockproc info */
	pid_t		ps_unblkonexecpid;	/* name says it all */
	short		ps_blkcnt;
	uchar_t		ps_isblocked;		/* is proc actually blocked? */
	/* misc stuff */
	char		ps_exitsig;		/* proc exitsig */
	/* misc flags */
	uchar_t		ps_flags;		/* misc flags...see below */

} ckpt_psi_old_t;

/*
 * Old per process info
 */
#define	OLD_PSCOMSIZ	32
#define	OLD_PSARGSZ	80

#define OLD_SCEXIT      0x00000002
#define OLD_SCOREPID    0x00800000

typedef struct ckpt_procinfo_old {
	long		cp_magic;		/* CKPT_MAGIC_PROCINFO */

	/* u area related info */
	char		cp_comm[OLD_PSCOMSIZ];	/* process name */
	char		cp_psargs[OLD_PSARGSZ];	/* and arguments */
	char		cp_cdir[MAXPATHLEN];	/* current directory */
        char		cp_rdir[MAXPATHLEN];	/* root directory */

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
	pid_t           cp_epid;		/* effective pid */
	cpuid_t		cp_mustrun;		/* mustrun cpu if any */
	ushort          cp_tslice;		/* process time slice size */
	dev_t		cp_ttydev;		/* controlling tty */
	hwperf_eventctrl_t	cp_hwpctl;	/* hwperf control info */
	hwperf_eventctrlaux_t	cp_hwpctlaux;	/* hwperf aux control info */
	hwperf_profevctraux_t	cp_hwpaux;	/* hwperf aux counter info */

	/* array related info */
	acct_spi_t	cp_spi;			/* array service provider */
	prid_t		cp_prid;		/* array session project id */

	/*
	 * XXX we need to see if we can get by without saving kthread info
	 * we need to exam p_time, p_utime, p_slptime
	 */
	timespec_t	cp_birth_time;		/* start time since 1970 */
	timespec_t	cp_ckpt_time;		/* when the proc was checkpointed */

	/*
	 * ckpt_psi_t is all of the other info we cannot get using existing
	 * interfaces.
	 */
	ckpt_psi_old_t	cp_psi;
	int		cp_nofiles;		/* # of openfiles */
	int		cp_maxfd;		/* highest # fd */
	int		cp_npfds;		/* max # of prototype pipes */
	uint_t		cp_flags;		/* flags, see below */
	int		cp_mldlink;
	int		cp_schedmode;		 /* sproc sched mode */
	char		cp_clname[4];		/* sched class name */
	int		cp_pads[12];		/* enough space for future exp. */
} ckpt_pi_old_t;

typedef struct ckpt_context_old {
        ucontext_t      cc_ctxt;        /* machine context */
        u_char          cc_abi;         /* process abi */
        u_char          cc_usermask;    /* user level signal mask */
        char            cc_fpflags;     /* process floating point flags */
} ckpt_ctxt_old_t;

/*
 * Process signal state
 */
typedef struct ckpt_sig_old {
	ckpt_magic_t	cs_magic;	/* CKPT_MAGIC_SIGINFO */
	int		cs_nsigs;	/* Number of signals */
	int		cs_ninfo;	/* Number of info structures */
	short		cs_cursig;	/* current signal */
	struct sigaltstack cs_altstack;	/* alternate stack info */
	sigset_t	cs_pending;	/* signals pending */
	sigset_t	cs_async;	/* async signals pending */
	caddr_t		cs_sigtramp;	/* process trampoline address */
} ckpt_sig_old_t;

typedef struct ckpt_memmap_old {
        long            cm_magic;               /* CKPT_MEMAPMAGIC */
        caddr_t         cm_mapid;               /* unique map id */
        void            *cm_vaddr;              /* virtual address */
        size_t          cm_len;                 /* length (in bytes) */
        int             cm_maxprots;            /* original protections */
        uint            cm_mflags;              /* mapping flags */
        uint            cm_flags;               /* flags, from ckpt_getmap_t */
        uint            cm_xflags;              /* extended flags */
        uint            cm_cflags;              /* checkpoint flags */
        int             cm_nsegs;               /* number of segments */
        long            cm_nmodmin;             /* min # of mod'd pages */
        long            cm_nmodold;               /* number of modified pages */
        caddr_t         *cm_modlist;            /* list of modified addresses */        int             cm_pagesize;            /* page size */
        pid_t           cm_pid;                 /* pid that must map */
        dev_t           cm_rdev;                /* dev, for spec file maps */
        int             cm_shmid;               /* shmid, for sysV shm */
        off_t           cm_foff;                /* file offset */
        off_t           cm_maxoff;              /* max file offset */
        void            *cm_auxptr;             /* auxilliary attributes */
        size_t          cm_auxsize;             /* size of auxilliary attrs */
        int             cm_fd;                  /* fd to use for mapping */
        char            cm_path[MAXPATHLEN];    /* mapped file path name */
} ckpt_memmap_old_t;
/*
 * Old xflags
 */
#define	OLD_CKPT_PRIMARY	0x200		/* maps to mflags MAP_PRIMARY */

typedef struct ckpt_seg_old {
	void		*cs_vaddr;	/* virtual address */
	ulong_t		cs_len;		/* length (in bytes) */
	uint_t		cs_prots;	/* page protections */
	ushort_t	cs_lockcnt;	/* lock count */
	uchar_t		cs_notcached;	/* not cacheable */
} ckpt_seg_old_t;

typedef struct ussemastate_old_s {
        int     npid;           /* Input: number of pid structs */
        int     nfilled;        /* Return: number filled */
        int     nproc;          /* Return: number of processes */
        struct ussemapidstate_old_s {
                pid_t   pid;
                int     count;
        } *pidinfo;
} ussemastate_old_t;
/*
 * 6.4 pusema structures
 */
typedef struct usync_ckpt_old {
        off_t   off;            /* semaphore region offset */
        off_t   voff;           /* offset relative to uvaddr */
        pid_t   notify;         /* pid to get notification */
        int     value;          /* semaphore value */
} usync_ckpt_old_t;

typedef struct ckpt_pusema_old {
        ckpt_magic_t    pu_magic;       /* CKPT_MAGIC_PUSEMA */
        pid_t           pu_owner;       /* pid whose addrspace we found it in */        caddr_t         pu_uvaddr;      /* user virtual address */
        usync_ckpt_old_t pu_usync;      /* semaphore state */
} ckpt_pusema_old_t;

extern int ckpt_revision_check(ckpt_rev_t);
extern off_t ckpt_segobj_size(ckpt_rev_t);
extern int ckpt_rxlate_siginfo(ckpt_rev_t, ckpt_ta_t *, ckpt_magic_t);
extern int ckpt_rxlate_context(ckpt_rev_t, ckpt_ta_t *, ckpt_magic_t, ckpt_context_t *);
extern int ckpt_rxlate_procinfo(ckpt_rev_t, ckpt_ta_t *, ckpt_magic_t, ckpt_pi_t *);
extern int ckpt_rxlate_segobjs(ckpt_rev_t, int, ckpt_seg_t *, int);
extern int ckpt_rxlate_memmap(ckpt_rev_t, int, ckpt_memmap_t *);
extern int ckpt_rxlate_pusema(ckpt_rev_t, int, ckpt_pusema_t *);

#endif /* !__CKPT_REVISION_H__ */
