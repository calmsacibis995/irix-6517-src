/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef _FS_PROCFS_PRDATA_H	/* wrapper symbol for kernel use */
#define _FS_PROCFS_PRDATA_H	/* subject to change without notice */

/*#ident	"@(#)uts-comm:fs/procfs/prdata.h	1.9"*/
#ident  "$Revision: 1.76 $"

#include <sys/types.h>
#include <sys/sema.h>
#include <sys/poll.h>

/* XXX duplicated here & in fdfs, in place of param.h's BSD macros */
#define	min(a,b)	((a) <= (b) ? (a) : (b))
#define	max(a,b)	((a) >= (b) ? (a) : (b))
#define	round(r)	(((r)+sizeof(off_t)-1)&(~(sizeof(off_t)-1)))

#define	PNSIZ	10			/* size of /proc name entries */

/*
 * Macros for mapping between i-numbers and pids.
 */
#define	PRBIAS	64
#define	itop(n)	((int)((n)-PRBIAS))	/* i-number to pid */
#define	ptoi(n)	((int)((n)+PRBIAS))	/* pid to i-number */

typedef enum prnodetype {
	PR_PROCDIR,	/* /proc */
	PR_PSINFO	/* /proc/pinfo */
} prnodetype_t;

/*
 * MP locking protocols:
 *	pr_free				splock/spunlock prfreelock
 *	pr_next				splock/spunlock pr_proc->p_siglck
 *	pr_vnode, etc.			prlock/prunlock
 */
typedef struct prnode {
	struct prnode	*pr_free;	/* freelist pointer */
	struct prnode	*pr_next;	/* linked list of invalid prnodes */
	struct vnode	*pr_vnode;	/* associated vnode */
	bhv_desc_t	pr_bhv_desc;	/* procfs behavior descriptor */
	struct proc	*pr_proc;	/* process being traced */
	struct pollhead	*pr_pollhead;
	pid_t		pr_tpid;	/* pid of traced  process - used
					 * for lookup */
#ifdef PRCLEAR
	struct proc     *pr_listproc;	
#else
	struct proc 	*pr_notused;	/* make sure we don't change the size of */
					/* this struct				 */
#endif
	prnodetype_t	pr_type;
	short		pr_mode;	/* file mode bits */
	short		pr_opens;	/* count of opens */
	short		pr_writers;	/* count of opens for writing */
	short		pr_pflags;	/* private flags */
	mutex_t		pr_lock;	/* mutual exclusion */
	short		pr_nested;	/* count of nested prlocks */
} prnode_t;


/*
 * Directory characteristics (patterned after the s5 file system).
 * (Moved from prvnops.c)
 */
#define	PRROOTINO	2
#define PRPINFOINO	3
#define PRGRPINO	4

#define	PRDIRSIZE	14
struct prdirect {
	u_short	d_ino;
	char	d_name[PRDIRSIZE];
};

#define	PRSDSIZE	(sizeof(struct prdirect))

/*
 * Conversion macros.
 */
#define	BHVTOPRNODE(bdp)	((prnode_t *)BHV_PDATA((bdp)))
#define	PRNODETOBHV(pnp)	(&((pnp)->pr_bhv_desc))
#define	PRNODETOV(pnp)		((pnp)->pr_vnode)

/*
 * Flags for pr_pflags.
 */
#define	PREXCL		0x01	/* Exclusive-use (disallow opens for write) */
#define	PRINVAL		0x02	/* vnode is invalid (security provision) */

/*
 * Flags to prlock()
 */
#define PRNULLOK	0	/* null proc ok */
#define PRNONULL	0x1	/* null proc not ok */

/*
 * Uthread type (pointer-based, for now -- convert to id)
 */
#define	PR_NOTHREAD	(struct uthread_s *)NULL

/*
 * Assign one set to another (possible different sizes).
 *
 * Assigning to a smaller set causes members to be lost.
 * Assigning to a larger set causes extra members to be cleared.
 */
#define	prassignset(ap, sp)					\
{								\
	register int _i_ = sizeof(*(ap))/sizeof(__uint32_t);	\
	while (--_i_ >= 0)					\
		((__uint32_t*)(ap))[_i_] =			\
		  (_i_ >= sizeof(*(sp))/sizeof(__uint32_t)) ?	\
		  0L : ((__uint32_t*)(sp))[_i_];		\
}

/*
 * Determine whether or not a set (of arbitrary size) is empty.
 */
#define prisempty(sp)	\
	setisempty((__uint32_t *)(sp), sizeof(*(sp))/sizeof(__uint32_t))

/* This header file exists in kernel source only. 
 * Commands that include this header must be
 * built like kernel, thus passing in _KMEMUSER flag.
 */
#ifdef _KERNEL

/*
 * This macro defines whether the process can get back into user
 * space or not.  It does NOT guarantee that the process will not
 * wakeup inside the kernel and continue running inside the kernel.
 * It must NOT be used as a substitute for locking.
 */
#define isstopped(ut)	\
	(((ut)->ut_flags & UT_STOP && (ut)->ut_whystop != JOBCONTROL) || \
	 ((ut)->ut_flags & UT_PRSTOPBITS && UT_TO_KT(ut)->k_flags & KT_SLEEP))

#include <sys/uio.h>		/* XXX for enum uio_rw */
#include <ksys/as.h>

enum uio_rw;
struct cred;
struct pgd;
struct prpgd_sgi;
struct proc;
struct prpsinfo;
struct prstatus;
struct sigaction;
struct uio;
struct vopbd;
struct prio;

int		prusrio(struct proc *, struct uthread_s *, enum uio_rw, struct uio *, int);
int		prthreadio(struct proc *, struct uthread_s *, enum uio_rw, struct prio *);
int		prisreadable(struct proc *, struct cred *);
int		prlock(struct prnode *, int, int);
void		prunlock(struct prnode *);
void		prgetaction(struct proc *, u_int, struct sigaction *);
int		prnsegs(vasid_t);
int		prgetmap(caddr_t, int, vasid_t);
int		prgetmap_sgi(caddr_t, int *, int, vasid_t);
int		prgetpgd_sgi(caddr_t, vasid_t, struct prpgd_sgi *);
int		prgetpsinfo(struct uthread_s *, struct prnode *, int,
			    struct prpsinfo *);
int		setisempty(__uint32_t *, unsigned);
int		prwstop_ut(struct uthread_s *, struct prnode *);
int             prwstop_proc(struct proc *, struct prnode *);
void		prstop_ut(struct uthread_s *);
void            prstop_proc(struct proc *, int);
void		pr_start(struct uthread_s *);
int             prletrun(struct proc *);
int		prioctl(bhv_desc_t *, int, void *, int, struct cred *, int *,
			struct vopbd *);
struct uthread_s *prchoosethread(struct proc *);

#if (_MIPS_SIM == _MIPS_SIM_ABI32)
struct irix5_n32_prstatus;
extern int prgetstatus(struct uthread_s *, int, struct irix5_n32_prstatus *);
#else
extern int prgetstatus(struct uthread_s *, int, struct prstatus *);
#endif

/*
 * Machine-dependent routines (defined in prmachdep.c).
 */
#include <sys/ksignal.h>	/* XXX for sigset_t, required by ucontext.h */
#include <sys/kucontext.h>

struct prusage;
void		prgetregs(struct uthread_s *, greg_t *);
void		prsetregs(struct uthread_s *, greg_t *);
#if (_MIPS_SIM == _MIPS_SIM_ABI32)
void		prgetregs_irix5_64(struct uthread_s *, irix5_64_greg_t *);
void		prsetregs_irix5_64(struct uthread_s *, irix5_64_greg_t *);
int		prgetfpregs_irix5_64(struct uthread_s *, irix5_64_fpregset_t *);
int		prsetfpregs_irix5_64(struct uthread_s *, irix5_64_fpregset_t *);
#endif
int		prhasfp(void);
int		prgetfpregs(struct uthread_s *, fpregset_t *);
int		prsetfpregs(struct uthread_s *, fpregset_t *);
void		prsvaddr(struct uthread_s *, caddr_t);
caddr_t		prmapin(struct proc *, caddr_t, int);
void		prmapout(struct proc *, caddr_t, caddr_t, int);
caddr_t		prfastmapin(struct proc *, caddr_t, int);
void		prfastmapout(struct proc *, caddr_t, caddr_t, int);
void		prgetptimer(struct proc *, timespec_t *);
int		watchcopy(caddr_t, ulong, ulong, caddr_t *);
void		prgetusage(struct proc *, struct prusage *);

#endif /* _KERNEL */

#endif	/* _FS_PROCFS_PRDATA_H */
