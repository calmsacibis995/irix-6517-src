#define _KERNEL 1
#include <sys/types.h>
#include <sys/sema.h>
#include <sys/buf.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/pfdat.h>
#include <sys/sysmacros.h>
#include <sys/debug.h>
#include <ksys/kern_heap.h>
#include <ksys/kqueue.h>
#include <sys/acl.h>
#include <sys/capability.h>
#include <sys/mac_label.h>
#undef _KERNEL
#include <sys/sysinfo.h>
#include <sys/ksa.h>
#include <sys/tuneable.h>
#include <sys/var.h>
#include <sys/kmem.h>
#include <sys/stat.h>
#include <sys/dkio.h>
#include <sys/uuid.h>
#include <sys/xlv_base.h>
#include <sys/xlv_tab.h>
#include <sys/ustat.h>
#include <sys/cred.h>
#include <stdarg.h>
#include <stdio.h>
#include <malloc.h>
#include <bstring.h>
#include <string.h>
#include <stdlib.h>
#include <diskinfo.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/fs/xfs_macros.h>
#include <sys/fs/xfs_types.h>
#include <sys/fs/xfs_inum.h>
#include <sys/fs/xfs_sb.h>
#include <sys/fs/xfs_ag.h>
#include <sys/fs/xfs_log.h>
#include <sys/fs/xfs_trans.h>
#include <sys/fs/xfs_dir.h>
#include <sys/fs/xfs_mount.h>
#include <sys/fs/xfs_bit.h>
#include "sim.h"



clock_t			lbolt;
struct syswait		syswait;
struct getblkstats	BUFINFO;
struct sysinfo		SYSINFO;
struct vnodestats	VOPINFO;
struct xfsstats		XFSSTATS;
tune_t			tune;
struct var		v;
struct cred		*credp;
struct ncstats		NCSTATS;
int			maxsymlinks = 8;
int			restricted_chown = 0;
int			ncsize = 128;
int			mac_enabled;
int			acl_enabled;
#ifdef DEBUG
int			doass = 1;
#endif
int			kheap_initialized;
mpkqueuehead_t		mutexinfolist[1];
mpkqueuehead_t		svinfolist[1];
mpkqueuehead_t		semainfolist[1];
mpkqueuehead_t		mrinfolist[1];

STATIC int check_ismounted(char *name, char *block);

int
xfs_sim_logstat(sim_init_t *si)
{
	char		curdir[MAXPATHLEN];
	char		*rawfile;
	struct stat64	statbuf;
	int		fd;
	char		buf[BBSIZE];
	xfs_sb_t	*sb;
	xfs_mount_t	mp;
	int		is_volume = 0;

	(void)getwd(curdir);
	if (si->logname) {
	    if (si->lisfile) {
		if (fstat64(bmajor(si->logdev), &statbuf) == -1) {
		    fprintf(stderr, "fstat of dev_t: %d failed\n",
			    bmajor(si->logdev));
		    goto error;
		}
		si->logBBsize = (long long)BTOBB(statbuf.st_size);
		si->logBBstart = 0;
	    } else {
		if (! (rawfile = findrawpath(si->logname))) {
		    fprintf(stderr, "no raw device for %s\n", si->logname);
		    goto error;
		}
		if ((fd = open(rawfile, O_RDONLY)) == -1) {
		    fprintf(stderr, "Can't open file %s\n", rawfile);
		    exit(1);
		}
		goto read_sb;
	    }
	} else {	/* is a volume */
	    fd = major(si->ddev);
	    is_volume = 1;

read_sb:
	    lseek64(fd, 0, SEEK_SET);
	    if (read(fd, buf, sizeof(buf)) != sizeof(buf)) {
		fprintf(stderr, "read of superblock failed\n");
		exit(1);
	    }
	    sb = (xfs_sb_t *)buf;

	    /* Conjure up a mount structure */
	    bcopy(sb, &(mp.m_sb), sizeof *sb);
	    mp.m_blkbb_log = sb->sb_blocklog - BBSHIFT;

	    si->logBBsize = XFS_FSB_TO_BB(&mp, sb->sb_logblocks);
	    si->logBBstart = XFS_FSB_TO_DADDR(&mp, sb->sb_logstart);

	    if (is_volume) {
		if (si->logdev == 0)		/* Internal log? */
		    si->logdev = si->ddev;
		lseek(fd, 0, SEEK_SET);
	    } else {
		close(fd);
	    }
	}

	chdir(curdir);
	return 0;

error:
	chdir(curdir);
	return -1;
}	/* xfs_sim_logstat */


/*
 * Simulation initialization.
 * Caller gets a 0 on failure (and we print a message), 1 on success.
 */
int
xfs_sim_init(sim_init_t	*a)
{
	char		*blockfile;
	char		curdir[MAXPATHLEN];
	char		*dname;
	char		dpath[25];
	int		fd;
	xlv_getdev_t	getdev;
	char		*logname;
	char		logpath[25];
	int		needcd;
	char		*rawfile;
	char		*rtname;
	char		rtpath[25];
	int		rval = 0;
	int		readonly;
	struct stat64	stbuf;
	extern void	vn_init(void);
	extern int	xfs_init(vfssw_t *, int);

	dpath[0] = logpath[0] = rtpath[0] = '\0';
	dname = a->dname;
	logname = a->logname;
	rtname = a->rtname;
	a->ddev = a->logdev = a->rtdev = 0;
	a->dfd = a->logfd = a->rtfd = -1;
	(void)getwd(curdir);
	needcd = 0;
	fd = -1;
	readonly = a->isreadonly == XFS_SIM_ISREADONLY;
	if (a->volname) {
		if (stat64(a->volname, &stbuf) < 0) {
			perror(a->volname);
			goto done;
		}
		if (!(rawfile = findrawpath(a->volname))) {
			fprintf(stderr,
				"can't find a character device matching %s\n",
				a->volname);
			goto done;
		}
		if (!(blockfile = findblockpath(a->volname))) {
			fprintf(stderr,
				"can't find a block device matching %s\n",
				a->volname);
			goto done;
		}
		if (!readonly && check_ismounted(a->volname, blockfile))
			goto done;
		needcd = 1;
		fd = open(rawfile, O_RDONLY);
		if (ioctl(fd, DIOCGETXLVDEV, &getdev) < 0) {
			if (a->notvolok) {
				dname = a->dname = a->volname;
				a->volname = NULL;
				goto voldone;
			}
			fprintf(stderr,
				"%s is not an XLV volume device name\n",
				a->volname);
			if (a->notvolmsg)
				fprintf(stderr, a->notvolmsg, a->volname);
			goto done;
		}
		if (getdev.data_subvol_dev && dname) {
			fprintf(stderr,
				"%s has a data subvolume, cannot specify %s\n",
				a->volname, dname);
			goto done;
		}
		if (getdev.log_subvol_dev && logname) {
			fprintf(stderr,
				"%s has a log subvolume, cannot specify %s\n",
				a->volname, logname);
			goto done;
		}
		if (getdev.rt_subvol_dev && rtname) {
			fprintf(stderr,
			"%s has a realtime subvolume, cannot specify %s\n",
				a->volname, rtname);
			goto done;
		}
		if (!dname && getdev.data_subvol_dev) {
			strcpy(dpath, "/tmp/xfs_simdXXXXXX");
			(void)mktemp(dpath);
			if (mknod(dpath, S_IFCHR | 0600,
				  getdev.data_subvol_dev) < 0) {
				perror("mknod");
				goto done;
			}
			dname = dpath;
		}
		if (!logname && getdev.log_subvol_dev) {
			strcpy(logpath, "/tmp/xfs_simlXXXXXX");
			(void)mktemp(logpath);
			if (mknod(logpath, S_IFCHR | 0600,
				  getdev.log_subvol_dev) < 0) {
				perror("mknod");
				goto done;
			}
			logname = logpath;
		}
		if (!rtname && getdev.rt_subvol_dev) {
			strcpy(rtpath, "/tmp/xfs_simrXXXXXX");
			(void)mktemp(rtpath);
			if (mknod(rtpath, S_IFCHR | 0600,
				  getdev.rt_subvol_dev) < 0) {
				perror("mknod");
				goto done;
			}
			rtname = rtpath;
		}
	}
voldone:
	if (dname) {
		if (dname[0] != '/' && needcd)
			chdir(curdir);
		if (a->disfile) {
			a->ddev = dev_open(dname, a->dcreat, readonly, 0);
			a->dfd = bmajor(a->ddev);
		} else {
			if (stat64(dname, &stbuf) < 0) {
				perror(dname);
				goto done;
			}
			if (!(rawfile = findrawpath(dname))) {
				fprintf(stderr,
				"can't find a character device matching %s\n",
					dname);
				goto done;
			}
			if (!(blockfile = findblockpath(dname))) {
				fprintf(stderr,
				"can't find a block device matching %s\n",
					dname);
				goto done;
			}
			if (!readonly && check_ismounted(dname, blockfile))
				goto done;
			a->ddev = dev_open(rawfile, a->dcreat, readonly, 1);
			a->dfd = bmajor(a->ddev);
			a->dsize = findsize(rawfile);
		}
		needcd = 1;
	} else
		a->dsize = 0;
	if (logname) {
		if (logname[0] != '/' && needcd)
			chdir(curdir);
		if (a->lisfile) {
			a->logdev = dev_open(logname, a->lcreat, readonly, 0);
			a->logfd = bmajor(a->logdev);
		} else {
			if (stat64(logname, &stbuf) < 0) {
				perror(logname);
				goto done;
			}
			if (!(rawfile = findrawpath(logname))) {
				fprintf(stderr,
				"can't find a character device matching %s\n",
					logname);
				goto done;
			}
			if (!(blockfile = findblockpath(logname))) {
				fprintf(stderr,
				"can't find a block device matching %s\n",
					logname);
				goto done;
			}
			if (!readonly && check_ismounted(logname, blockfile))
				goto done;
			a->logdev = dev_open(rawfile, a->lcreat, readonly, 1);
			a->logfd = bmajor(a->logdev);
			a->logBBsize = findsize(rawfile);
		}
		needcd = 1;
	} else
		a->logBBsize = 0;
	if (rtname) {
		if (rtname[0] != '/' && needcd)
			chdir(curdir);
		if (a->risfile) {
			a->rtdev = dev_open(rtname, a->rcreat, readonly, 0);
			a->rtfd = bmajor(a->rtdev);
		} else {
			if (stat64(rtname, &stbuf) < 0) {
				perror(rtname);
				goto done;
			}
			if (!(rawfile = findrawpath(rtname))) {
				fprintf(stderr,
				"can't find a character device matching %s\n",
					rtname);
				goto done;
			}
			if (!(blockfile = findblockpath(rtname))) {
				fprintf(stderr,
				"can't find a block device matching %s\n",
					rtname);
				goto done;
			}
			if (!readonly && check_ismounted(rtname, blockfile))
				goto done;
			a->rtdev = dev_open(rawfile, a->rcreat, readonly, 1);
			a->rtfd = bmajor(a->rtdev);
			a->rtsize = findsize(rawfile);
		}
		needcd = 1;
	} else
		a->rtsize = 0;
	if (a->dsize < 0) {
		fprintf(stderr, "can't get size for data subvolume\n");
		goto done;
	}
	if (a->logBBsize < 0) {
		fprintf(stderr, "can't get size for log subvolume\n");
		goto done;
	}
	if (a->rtsize < 0) {
		fprintf(stderr, "can't get size for realtime subvolume\n");
		goto done;
	}
	if (needcd)
		chdir(curdir);
	credp = malloc(sizeof(struct cred) + sizeof(gid_t)*(NGROUPS_UMAX-1));
	v.v_buf = NBUF;
	v.v_hbuf = HBUF;
	binit();
	vn_init();
	xfs_init(NULL, 1);
	rval = 1;
done:
	if (dpath[0])
		unlink(dpath);
	if (logpath[0])
		unlink(logpath);
	if (rtpath[0])
		unlink(rtpath);
	if (fd >= 0)
		close(fd);
	if (!rval && a->ddev)
		dev_close(a->ddev);
	if (!rval && a->logdev)
		dev_close(a->logdev);
	if (!rval && a->rtdev)
		dev_close(a->rtdev);
	return rval;
}

/* ARGSUSED */
void *
kmem_alloc(size_t size, int flags)
{
	void *rval;

#ifdef XFSDEBUG
	mallopt(M_DEBUG, 1);
#endif
	rval = malloc(size);
	ASSERT(rval || !(flags & KM_SLEEP));
#ifdef XFSDEBUG
	mallopt(M_DEBUG, 0);
#endif
	return rval;
}

/* ARGSUSED */
void *
kmem_zalloc(size_t size, int flags)
{
	void	*ptr;

	ptr = kmem_alloc(size, flags);
	bzero((char *)ptr, (int)size);
	return (ptr);
}

/* ARGSUSED */
void *
kmem_realloc(void *ptr, size_t size, int flags)
{
	void *rval;

#ifdef XFSDEBUG
	mallopt(M_DEBUG, 1);
#endif
	rval = realloc(ptr, size);
	ASSERT(rval || size == 0 || !(flags & KM_SLEEP));
#ifdef XFSDEBUG
	mallopt(M_DEBUG, 0);
#endif
	return rval;
}

/* ARGSUSED */
void
kmem_free(void *ptr, size_t size)
{
#ifdef XFSDEBUG
	mallopt(M_DEBUG, 1);
#endif
	free(ptr);
#ifdef XFSDEBUG
	mallopt(M_DEBUG, 0);
#endif
}

#ifdef XFSDEBUG
void
kmem_check()
{
	void *ptr;

	ptr = kmem_alloc(sizeof(ptr), KM_SLEEP);
	ASSERT(ptr);
	kmem_free(ptr, sizeof(ptr));
}
#endif

zone_t *
kmem_zone_init(int size, char *zone_name)
{
	zone_t *rval;

	rval = kmem_alloc(sizeof(zone_t), KM_SLEEP);
	rval->zone_unitsize = size;
	rval->zone_name = zone_name;
	return rval;
}

void *
kmem_zone_alloc(zone_t *zone, int flags)
{
	void *ptr;

	ptr = kmem_alloc(zone->zone_unitsize, flags);
	return ptr;
}

void *
kmem_zone_zalloc(zone_t *zone, int flags)
{
	void	*ptr;

	ptr = kmem_zalloc(zone->zone_unitsize, flags);
	return ptr;
}

void
kmem_zone_free(zone_t *zone, void *ptr)
{
	kmem_free(ptr, zone->zone_unitsize);
}

/* ARGSUSED */
void
wakeup(void *addr)
{
	return;
}

/* ARGSUSED */
void
delay(long ticks)
{
	return;
}

#ifdef DEBUG
void
assfail(char *m, char *f, int l)
{
	fprintf(stderr, "Assertion failed: %s, file %s, line %d\n", m, f, l);
	abort();
}
#endif

/*
 * Can't include sys/time.h for this prototype since it has
 * a duplicate definition of the timespec structure.
 */
extern int gettimeofday(struct timeval *tp, ...);

time_t
gtime(void)
{
	struct timeval tv;

	gettimeofday(&tv, (struct timezone *)0);
	return tv.tv_sec;
}

void
nanotime_syscall(timespec_t *tvp)
{
	nanotime(tvp);
}

void
nanotime(timespec_t *tvp)
{
	struct timeval tv;

        gettimeofday(&tv, (struct timezone *)0);

        tvp->tv_sec = tv.tv_sec;
        tvp->tv_nsec = tv.tv_usec * 1000;
}


void
panic(char *fmt, ...)
{
	fprintf(stderr, fmt);
	fprintf(stderr, "\n");
	abort();
}

int
groupmember(gid_t gid, cred_t *cr)
{
        register gid_t *gp, *endgp;

        if (gid == cr->cr_gid)
                return 1;
        endgp = &cr->cr_groups[cr->cr_ngroups];
        for (gp = cr->cr_groups; gp < endgp; gp++)
                if (*gp == gid)
                        return 1;
        return 0;
}

int
copyin(void *from, void *to, int len)
{
	bcopy(from, to, len);
	return 0;
}

int
copyout(void *from, void *to, int len)
{
	bcopy(from, to, len);
	return 0;
}

void
prdev(char *fmt, int dev, ...)
{
	va_list ap;

	printf("%x: ", dev);
	va_start(ap, dev);
	vprintf(fmt, ap);
	va_end(ap);
	printf("\n");
}

/* ARGSUSED */
struct vnode *
specvp(struct vnode *vp, dev_t dev, vtype_t type, struct cred *cr)
{
	ASSERT (0);
	return (struct vnode *)0;
}

STATIC int
check_ismounted(char *name, char *block)
{
	struct ustat ust;
	struct stat64 st;
	extern int ustat(dev_t, struct ustat *);

	if (stat64(block, &st) < 0)
		return 0;
	if ((st.st_mode & S_IFMT) != S_IFBLK)
		return 0;
	if (ustat(st.st_rdev, &ust) >= 0) {
		fprintf(stderr, "%s contains a mounted filesystem\n", name);
		return 1;
	}
	return 0;
}

int
dm_data_event(void)
{
	return 0;
}

int
dm_namesp_event(void)
{
	return 0;
}

/* ARGSUSED */
void
uuid_getnodeuniq(uuid_t *uuid, int fsid [2])
{
	fsid [0] = 0;
	fsid [1] = 0;
}

/* ARGSUSED */
void
remapf(vnode_t *vp, off_t size, int flush)
{
}

/* ARGSUSED */
void
xfs_fs_cmn_err(int level, xfs_mount_t *mp, char *fmt, ...)
{
	va_list ap;

	printf("Filesystem \"%s\": ", mp->m_fsname);
	va_start(ap, fmt);
	(void)vprintf(fmt, ap);
	va_end(ap);
}

/* ARGSUSED */
void
xfs_cmn_err(uint64_t panic_tag, int level, xfs_mount_t *mp, char *fmt, ...)
{
	va_list ap;

	printf("Filesystem \"%s\": ", mp->m_fsname);
	va_start(ap, fmt);
	(void)vprintf(fmt, ap);
	va_end(ap);
}

/* ARGSUSED */
void
cmn_err(int level, char *format, ...)
{
	va_list ap;

	va_start(ap, format);
	(void)vprintf(format, ap);
	va_end(ap);
}

uint64_t
get_thread_id(void)
{
	return 0;
}

cred_t *
get_current_cred(void)
{
	return credp;
}

/* ARGSUSED */
char *
dev_to_name(dev_t dev, char *buf, uint buflen)
{
	sprintf(buf, "/hw/major/%d/minor/%d", bmajor(dev), minor(dev));
	return buf;
}

/* ARGSUSED */
int
mac_xfs_iaccess(struct xfs_inode *ip, mode_t mode, struct cred *cr)
{
	return 0;
}

/* ARGSUSED */
int
acl_xfs_iaccess(struct xfs_inode *ip, mode_t mode, struct cred *cr)
{
	return 0;
}

/* ARGSUSED */
int
cap_able_cred(struct cred *cr, cap_value_t cid)
{
	return 0;
}
