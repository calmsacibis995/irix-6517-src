#ifndef __SYS_NFS_STAT_H__
#define __SYS_NFS_STAT_H__

/*
 * Server statistics
 */

#ident "$Revision: 1.8 $"

struct rsstat {
	u_int	rscalls;
	u_int	rsbadcalls;
	u_int	rsnullrecv;
	u_int	rsbadlen;
	u_int	rsxdrcall;
	u_int	rsduphits;
	u_int	rsdupage;
};

struct rcstat {
	int	rccalls;
	int	rcbadcalls;
	int	rcretrans;
	int	rcbadxids;
	int	rctimeouts;
	int	rcwaits;
	int	rcnewcreds;
	int	rcbadverfs;
};

/*
 * client side statistics
 */
struct clstat {
	int	nclsleeps;		/* client handle waits */
	int	nclgets;		/* client handle gets */
	int	ncalls;			/* client requests */
	int	nbadcalls;		/* rpc failures */
	int	reqs[32];		/* count of each request */
};

struct svstat {
	int	ncalls;		/* number of calls received */
	int	nbadcalls;	/* calls that failed */
	int	reqs[32];	/* count for each request */
};

struct nfs_stat {
	struct rcstat rcstat;
	struct clstat clstat;
	struct rsstat rsstat;
	struct svstat svstat;
	struct clstat clstat3;
	struct svstat svstat3;
};
#define RSSTAT	(((struct nfs_stat *)private.nfsstat)->rsstat)
#define RCSTAT	(((struct nfs_stat *)private.nfsstat)->rcstat)
#define CLSTAT	(((struct nfs_stat *)private.nfsstat)->clstat)
#define CLSTAT3	(((struct nfs_stat *)private.nfsstat)->clstat3)
#define SVSTAT	(((struct nfs_stat *)private.nfsstat)->svstat)
#define SVSTAT3	(((struct nfs_stat *)private.nfsstat)->svstat3)

#ifdef _KERNEL
extern int     nfs_clearstat(void);
extern caddr_t nfs_getclstat(int);
extern caddr_t nfs_getclstat3(int);
extern caddr_t nfs_getrcstat(int);
extern caddr_t nfs_getrsstat(int);
extern caddr_t nfs_getsvstat(int);
extern caddr_t nfs_getsvstat3(int);
#endif	/* _KERNEL */

#endif /* !__SYS_NFS_STAT_H__ */
