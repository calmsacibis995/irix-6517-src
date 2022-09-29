#ifndef _SIM_H
#define _SIM_H

struct bdevsw;
struct xfs_mount;

#define	NBUF	256
#define	HBUF	8
#define HSHIFT	3

#define	time	gtime()

extern clock_t			lbolt;
extern struct syswait		syswait;
#undef BUFINFO
extern struct getblkstats	BUFINFO;
#undef SYSINFO
extern struct sysinfo		SYSINFO;
#undef VOPINFO
extern struct vnodestats	VOPINFO;
extern struct var		v;
extern struct cred		*credp;
extern int			npages;
#undef NCSTATS
extern struct ncstats		NCSTATS;
#undef XFSSTATS
extern struct xfsstats		XFSSTATS;


#undef	PREEMPT
#define	PREEMPT()

#undef	SYNCHRONIZE
#define	SYNCHRONIZE()

#define	crhold(c)
#define	crfree(c)
#define crsuser(c) 1
#define	crdup(c)	credp

#define	suser()		(credp->cr_uid == 0)

#define	get_bdevsw(dev)	((struct bdevsw *)(__psint_t)(dev))

#define	sharedd_lock()
#define	sharedd_unlock()
#define	setshdsync(a,b,c)

#define	ovbcopy(from,to,count)	memmove(to,from,count)

#define XFS_SIM_ISREADONLY	0x69	/* some wierd value */
#ifndef MAXDEVNAME
#define	MAXDEVNAME	1024		/* from sys/conf.h _KERNEL */
#endif
#define	BDSTRAT_SIZE	(256 * 1024)

struct buf;
struct vnode;
struct xfs_mount;

/*
 * Argument structure for xfs_sim_init().
 */
typedef struct sim_init {
				/* input parameters */
	char		*volname;	/* pathname of volume */
	char		*dname;		/* pathname of data "subvolume" */
	char		*logname;	/* pathname of log "subvolume" */
	char		*rtname;	/* pathname of realtime "subvolume" */
	int		isreadonly;	/* filesystem is only read in applic */
	int		disfile;	/* data "subvolume" is a regular file */
	int		dcreat;		/* try to create data subvolume */
	int		lisfile;	/* log "subvolume" is a regular file */
	int		lcreat;		/* try to create log subvolume */
	int		risfile;	/* realtime "subvolume" is a reg file */
	int		rcreat;		/* try to create realtime subvolume */
	char		*notvolmsg;	/* format string for not XLV message */
	int		notvolok;	/* set if not XLV => try data */
				/* output results */
	dev_t		ddev;		/* device for data subvolume */
	dev_t		logdev;		/* device for log subvolume */
	dev_t		rtdev;		/* device for realtime subvolume */
	long long	dsize;		/* size of data subvolume (BBs) */
	long long	logBBsize;	/* size of log subvolume (BBs) */
	long long	logBBstart;	/* start block of log subvolume (BBs) */
	long long	rtsize;		/* size of realtime subvolume (BBs) */
	int		dfd;		/* data subvolume file descriptor */
	int		logfd;		/* log subvolume file descriptor */
	int		rtfd;		/* realtime subvolume file descriptor */
} sim_init_t;

extern int	xfs_sim_init(sim_init_t *);
extern int	xfs_sim_logstat(sim_init_t *);

extern time_t	gtime(void);
extern int	splhi(void);
#ifdef XFSDEBUG
extern void	kmem_check(void);
#endif
extern void	wakeup(void *);
extern void	delay(long);
extern void	xfs_cmn_err(uint64_t, int, struct xfs_mount *, char *, ...);
extern void	cmn_err(int, char *, ...);
extern void	bunlocked(void);
extern void	remapf(struct vnode *, off_t, int);
extern int	atomicIncWithWrap(int *a, int b);

extern struct xfs_mount	*xfs_mount_init(void);

extern dev_t	dev_open(char *, int, int, int);
extern void	dev_close(dev_t);
extern int	dev_grow(dev_t, uint);
extern int	dev_zero(dev_t, daddr_t, uint);
extern void	bdstrat(struct bdevsw *, struct buf *);
extern void	bdopen(int, dev_t *, int, int, int);
extern void	bdclose(int, dev_t, int, int, int);
extern int	xreadb(int, void *, daddr_t, int, int);
extern int	xwriteb(int, void *, daddr_t, int, int);

extern void	initnlock(lock_t *, char *);
extern int	copyin(void *, void *, int);
extern int	copyout(void *, void *, int);
extern void	prdev(char *, int, ...);
extern char	*dev_to_name(dev_t, char *, uint);

#endif /* _SIM_H */
