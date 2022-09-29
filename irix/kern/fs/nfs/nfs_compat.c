/*
 * Miscellaneous NFS, SunOS, and 4.3BSD compatibilities.
 */
#include "types.h"
#include <sys/debug.h>
#include <sys/errno.h>
#include <sys/pda.h>
#include <sys/pfdat.h>
#include <sys/sema.h>
#include <sys/signal.h>
#include <sys/vnode.h>
#include <sys/vfs.h>
#include <sys/systm.h>
#include "nfs.h"
#include "nfs_clnt.h"	/* just for minormap, for vfs_{get,put}num */
#include "rnode.h"
#include "nfs_stat.h"

enum nfsstat nfs_statmap[] = {
	NFS_OK,			/* 0 */
	NFSERR_PERM,		/* EPERM = 1 */
	NFSERR_NOENT,		/* ENOENT = 2 */
	NFSERR_IO,		/* ESRCH = 3 */
	NFSERR_IO,		/* EINTR = 4 */
	NFSERR_IO,		/* EIO = 5 */
	NFSERR_NXIO,		/* ENXIO = 6 */
	NFSERR_IO,		/* E2BIG = 7 */
	NFSERR_IO,		/* ENOEXEC = 8 */
	NFSERR_IO,		/* EBADF = 9 */
	NFSERR_IO,		/* ECHILD = 10 */
	NFSERR_JUKEBOX,		/* EAGAIN = 11 */
	NFSERR_IO,		/* ENOMEM = 12 */
	NFSERR_ACCES,		/* EACCES = 13 */
	NFSERR_IO,		/* EFAULT = 14 */
	NFSERR_IO,		/* ENOTBLK = 15 */
	NFSERR_IO,		/* EBUSY = 16 */
	NFSERR_EXIST,		/* EEXIST = 17 */
	NFSERR_IO,		/* EXDEV = 18 */
	NFSERR_NODEV,		/* ENODEV = 19 */
	NFSERR_NOTDIR,		/* ENOTDIR = 20 */
	NFSERR_ISDIR,		/* EISDIR = 21 */
	NFSERR_INVAL,		/* EINVAL = 22 */
	NFSERR_IO,		/* ENFILE = 23 */
	NFSERR_IO,		/* EMFILE = 24 */
	NFSERR_IO,		/* ENOTTY = 25 */
	NFSERR_IO,		/* ETXTBSY = 26 */
	NFSERR_FBIG,		/* EFBIG = 27 */
	NFSERR_NOSPC,		/* ENOSPC = 28 */
	NFSERR_IO,		/* ESPIPE = 29 */
	NFSERR_ROFS,		/* EROFS = 30 */
	(enum nfsstat) 31,	/* EMLINK = 31 */
	NFSERR_IO,		/* EPIPE = 32 */
	NFSERR_IO,		/* EDOM = 33 */
	NFSERR_IO,		/* ERANGE = 34 */
	NFSERR_IO,		/* ENOMSG = 35 */
	NFSERR_IO,		/* EIDRM = 36 */
	NFSERR_IO,		/* ECHRNG = 37 */
	NFSERR_IO,		/* EL2NSYNC = 38 */
	NFSERR_IO,		/* EL3HLT = 39 */
	NFSERR_IO,		/* EL3RST = 40 */
	NFSERR_IO,		/* ELNRNG = 41 */
	NFSERR_IO,		/* EUNATCH = 42 */
	NFSERR_IO,		/* ENOCSI = 43 */
	NFSERR_IO,		/* EL2HLT = 44 */
	NFSERR_IO,		/* EDEADLK = 45 */
	NFSERR_IO,		/* ENOLCK = 46 */
	NFSERR_IO,		/* 47 */
	NFSERR_IO,		/* 48 */
	NFSERR_IO,		/* 49 */
	NFSERR_IO,		/* EBADE = 50 */
	NFSERR_IO,		/* EBADR = 51 */
	NFSERR_IO,		/* EXFULL = 52 */
	NFSERR_IO,		/* ENOANO = 53 */
	NFSERR_IO,		/* EBADRQC = 54 */
	NFSERR_IO,		/* EBADSLT = 55 */
	NFSERR_IO,		/* EDEADLOCK = 56 */
	NFSERR_IO,		/* EBFONT = 57 */
	NFSERR_IO,		/* 58 */
	NFSERR_IO,		/* 59 */
	NFSERR_IO,		/* ENOSTR = 60 */
	NFSERR_IO,		/* ENODATA = 61 */
	NFSERR_IO,		/* ETIME = 62 */
	NFSERR_IO,		/* ENOSR = 63 */
	NFSERR_IO,		/* ENONET = 64 */
	NFSERR_IO,		/* ENOPKG = 65 */
	NFSERR_IO,		/* EREMOTE = 66 */
	NFSERR_IO,		/* ENOLINK = 67 */
	NFSERR_IO,		/* EADV = 68 */
	NFSERR_IO,		/* ESRMNT = 69 */
	NFSERR_IO,		/* ECOMM = 70 */
	NFSERR_IO,		/* EPROTO = 71 */
	NFSERR_IO,		/* 72 */
	NFSERR_IO,		/* 73 */
	NFSERR_IO,		/* EMULTIHOP = 74 */
	NFSERR_IO,		/* ELBIN = 75 */
	NFSERR_IO,		/* EDOTDOT = 76 */
	NFSERR_IO,		/* EBADMSG = 77 */
	NFSERR_NAMETOOLONG,	/* ENAMETOOLONG = 78 */
	NFSERR_IO,		/* 79 */
	NFSERR_IO,		/* ENOTUNIQ = 80 */
	NFSERR_IO,		/* EBADFD = 81 */
	NFSERR_IO,		/* EREMCHG = 82 */
	NFSERR_IO,		/* ELIBACC = 83 */
	NFSERR_IO,		/* ELIBBAD = 84 */
	NFSERR_IO,		/* ELIBSCN = 85 */
	NFSERR_IO,		/* ELIBMAX = 86 */
	NFSERR_IO,		/* ELIBEXEC = 87 */
	NFSERR_IO,		/* 88 */
	NFSERR_IO,		/* 89 */
	NFSERR_IO,		/* ELOOP = 90 */
	NFSERR_IO,		/* 91 */
	NFSERR_IO,		/* 92 */
	NFSERR_NOTEMPTY,	/* ENOTEMPTY = 93 */
	NFSERR_IO,		/* 94 */
	NFSERR_IO,		/* ENOTSOCK = 95 */
	NFSERR_IO,		/* EDESTADDRREQ = 96 */
	NFSERR_IO,		/* EMSGSIZE = 97 */
	NFSERR_IO,		/* EPROTOTYPE = 98 */
	NFSERR_IO,		/* ENOPROTOOPT = 99 */
	NFSERR_IO,		/* 100 */
	NFSERR_IO,		/* 101 */
	NFSERR_IO,		/* 102 */
	NFSERR_IO,		/* 103 */
	NFSERR_IO,		/* 104 */
	NFSERR_IO,		/* 105 */
	NFSERR_IO,		/* 106 */
	NFSERR_IO,		/* 107 */
	NFSERR_IO,		/* 108 */
	NFSERR_IO,		/* 109 */
	NFSERR_IO,		/* 110 */
	NFSERR_IO,		/* 111 */
	NFSERR_IO,		/* 112 */
	NFSERR_IO,		/* 113 */
	NFSERR_IO,		/* 114 */
	NFSERR_IO,		/* 115 */
	NFSERR_IO,		/* 116 */
	NFSERR_IO,		/* 117 */
	NFSERR_IO,		/* 118 */
	NFSERR_IO,		/* 119 */
	NFSERR_IO,		/* EPROTONOSUPPORT = 120 */
	NFSERR_IO,		/* ESOCKTNOSUPPORT = 121 */
	NFSERR_IO,		/* EOPNOTSUPP = 122 */
	NFSERR_IO,		/* EPFNOSUPPORT = 123 */
	NFSERR_IO,		/* EAFNOSUPPORT = 124 */
	NFSERR_IO,		/* EADDRINUSE = 125 */
	NFSERR_IO,		/* EADDRNOTAVAIL = 126 */
	NFSERR_IO,		/* ENETDOWN = 127 */
	NFSERR_IO,		/* ENETUNREACH = 128 */
	NFSERR_IO,		/* ENETRESET = 129 */
	NFSERR_IO,		/* ECONNABORTED = 130 */
	NFSERR_IO,		/* ECONNRESET = 131 */
	NFSERR_IO,		/* ENOBUFS = 132 */
	NFSERR_IO,		/* EISCONN = 133 */
	NFSERR_IO,		/* ENOTCONN = 134 */
	NFSERR_IO,		/* Xenix = 135 */
	NFSERR_IO,		/* Xenix = 136 */
	NFSERR_IO,		/* Xenix = 137 */
	NFSERR_IO,		/* Xenix = 138 */
	NFSERR_IO,		/* Xenix = 139 */
	NFSERR_IO,		/* Xenix = 140 */
	NFSERR_IO,		/* Xenix = 141 */
	NFSERR_IO,		/* Xenix = 142 */
	NFSERR_IO,		/* ESHUTDOWN = 143 */
	NFSERR_IO,		/* ETOOMANYREFS = 144 */
	NFSERR_IO,		/* ETIMEDOUT = 145 */
	NFSERR_IO,		/* ECONNREFUSED = 146 */
	NFSERR_IO,		/* EHOSTDOWN = 147 */
	NFSERR_IO,		/* EHOSTUNREACH = 148 */
	NFSERR_IO,		/* EALREADY = 149 */
	NFSERR_IO,		/* EINPROGRESS = 150 */
	NFSERR_STALE,		/* ESTALE = 151 */
};

short nfs_errmap[] = {
	0,		/* NFS_OK = 0 */
	EPERM,		/* NFSERR_PERM = 1 */
	ENOENT,		/* NFSERR_NOENT = 2 */
	EIO,		/* 3 */
	EIO,		/* 4 */
	EIO,		/* NFSERR_IO = 5 */
	ENXIO,		/* NFSERR_NXIO = 6 */
	EIO,		/* 7 */
	EIO,		/* 8 */
	EIO,		/* 9 */
	EIO,		/* 10 */
	EIO,		/* 11 */
	EIO,		/* 12 */
	EACCES,		/* NFSERR_ACCES = 13 */
	EIO,		/* 14 */
	EIO,		/* 15 */
	EIO,		/* 16 */
	EEXIST,		/* NFSERR_EXIST = 17 */
	EIO,		/* 18 */
	ENODEV,		/* NFSERR_NODEV = 19 */
	ENOTDIR,	/* NFSERR_NOTDIR = 20 */
	EISDIR,		/* NFSERR_ISDIR = 21 */
	EINVAL,		/* 22 */
	EIO,		/* 23 */
	EIO,		/* 24 */
	EIO,		/* 25 */
	EIO,		/* 26 */
	EFBIG,		/* NFSERR_FBIG = 27 */
	ENOSPC,		/* NFSERR_NOSPC = 28 */
	EIO,		/* 29 */
	EROFS,		/* NFSERR_ROFS = 30 */
	EMLINK,		/* 31 */
	EIO,		/* 32 */
	EIO,		/* 33 */
	EIO,		/* 34 */
	EIO,		/* 35 */
	EIO,		/* 36 */
	EIO,		/* 37 */
	EIO,		/* 38 */
	EIO,		/* 39 */
	EIO,		/* 40 */
	EIO,		/* 41 */
	EIO,		/* 42 */
	EIO,		/* 43 */
	EIO,		/* 44 */
	EIO,		/* 45 */
	EIO,		/* 46 */
	EIO,		/* 47 */
	EIO,		/* 48 */
	EIO,		/* 49 */
	EIO,		/* 50 */
	EIO,		/* 51 */
	EIO,		/* 52 */
	EIO,		/* 53 */
	EIO,		/* 54 */
	EIO,		/* 55 */
	EIO,		/* 56 */
	EIO,		/* 57 */
	EIO,		/* 58 */
	EIO,		/* 59 */
	EIO,		/* 60 */
	EIO,		/* 61 */
	EIO,		/* 62 */
	ENAMETOOLONG,	/* NFSERR_NAMETOOLONG = 63 */
	EIO,		/* 64 */
	EIO,		/* 65 */
	ENOTEMPTY,	/* NFSERR_NOTEMPTY = 66 */
	EIO,		/* 67 */
	EIO,		/* 68 */
	EDQUOT,		/* NFSERR_DQUOT = 69 */
	ESTALE,		/* NFSERR_STALE = 70 */
	ENFSREMOTE,	/* NFSERR_REMOTE = 71 */
	EIO,		/* NFSERR_WFLUSH = 72 */
};

/*
 * Translate a vnode file type into an nfs file type.
 */
enum nfsftype
vtype_to_ntype(enum vtype vtype)
{
	static u_short vtypemap[] = {
		NFNON,	/* VNON=0 */
		NFREG,	/* VREG=1 - regular file */
		NFDIR,	/* VDIR=2 - directory */
		NFBLK,	/* VBLK=3 - block special */
		NFCHR,	/* VCHR=4 - character special */
		NFLNK,	/* VLNK=5 - symbolic link */
		NFCHR,	/* VFIFO=6 - named pipe */
		NFNON,	/* VBAD=7 - invalid vnode type */
		NFSOCK	/* VSOCK=8 - named socket */
	};

	if (vtype > (u_short) VSOCK)
		return NFNON;
	return (enum nfsftype)vtypemap[vtype];
}

/*
 * Translate an nfs file type into a vnode file type.
 */
enum vtype
ntype_to_vtype(enum nfsftype type)
{
	static u_short ntypemap[] = {
		VNON,	/* NFNON=0 */
		VREG,	/* NFREG=1 - regular file */
		VDIR,	/* NFDIR=2 - directory */
		VBLK,	/* NFBLK=3 - block special */
		VCHR,	/* NFCHR=4 - character special */
		VLNK,	/* NFLNK=5 - symbolic link */
		VSOCK   /* NFSOCK=6 - named socket */
	};
	short t = (u_short) type;

	if (t > (u_short) NFSOCK)
		return VNON;
	return (enum vtype)ntypemap[t];
}

/*
 * Set and return the first free position from the bitmap "map".
 * Return -1 if no position found.
 */
int
vfs_getnum(map)
	struct minormap *map;
{
	int i, j, s;
	u_char *mp, m;
	u_long new_size;

	i = map->size;
	s = splock(map->lock);
	for (mp = map->vec; --i >= 0; mp++) {
		m = *mp;
		if (m == (u_char)0xff)
			continue;
		for (j = NBBY; --j >= 0; m >>= 1) {
			if (!(m & 01)) {
				j = NBBY - 1 - j;
				*mp |= 1 << j;
				spunlock(map->lock, s);
				return (mp - map->vec) * NBBY  + j;
			}
		}
	}
	/*
	 * we are full, try to grow map.
	 */
	new_size = map->size + MINORMAP_GROW;
	if (new_size <= MINORMAP_MAX) {
		u_char	*new_vec;

		if ((new_vec = kmem_alloc(new_size, KM_NOSLEEP)) != NULL) {
			/*
			 * copy the old table, and zero the new part
			 */
			bcopy(map->vec, new_vec, map->size);
			bzero(new_vec + map->size, new_size - map->size);
			kmem_free(map->vec, map->size);
			map->vec = new_vec;
			/*
			 * setup the new size and return the first free bit
			 */
			i = map->size;		/* old size */
			map->size = new_size;
			map->vec[i] |= 0x1;
			spunlock(map->lock, s);
			return i * NBBY;
		}
	}
	spunlock(map->lock, s);
	return -1;
}

/*
 * Clear the designated position "n" in bitmap "map".
 */
void
vfs_putnum(map, n)
	struct minormap *map;
	int n;
{
	int s;

	if (n < 0)
		return;
	s = splock(map->lock);
	map->vec[n / NBBY] &= ~(1 << (n & (NBBY-1)));
	spunlock(map->lock, s);
}

/*
 * return rcstat struct pointer
 */
caddr_t 
nfs_getrcstat(int cpu)
{
	struct nfs_stat *ptr = (struct nfs_stat *)pdaindr[cpu].pda->nfsstat;
	if (ptr)
		return((caddr_t)&ptr->rcstat);
	return(0);
}

/*
 * return clstat struct pointer
 */
caddr_t
nfs_getclstat(int cpu)
{
	struct nfs_stat *ptr = (struct nfs_stat *)pdaindr[cpu].pda->nfsstat;
	if (ptr)
		return((caddr_t)&(ptr->clstat));
	return(0);
}

/*
 * return clstat3 struct pointer
 */
caddr_t
nfs_getclstat3(int cpu)
{
	struct nfs_stat *ptr = (struct nfs_stat *)pdaindr[cpu].pda->nfsstat;
	if (ptr)
		return((caddr_t)&(ptr->clstat3));
	return(0);
}

/*
 * return rsstat struct pointer
 */
caddr_t
nfs_getrsstat(int cpu)
{
	struct nfs_stat *ptr = (struct nfs_stat *)pdaindr[cpu].pda->nfsstat;
	if (ptr)
		return((caddr_t)&(ptr->rsstat));
	return(0);
}

/*
 * return svstat3 struct pointer
 */
caddr_t
nfs_getsvstat3(int cpu)
{
	struct nfs_stat *ptr = (struct nfs_stat *)pdaindr[cpu].pda->nfsstat;
	if (ptr)
		return((caddr_t)&(ptr->svstat3));
	return(0);
}

/*
 * return svstat struct pointer
 */
caddr_t
nfs_getsvstat(int cpu)
{
	struct nfs_stat *ptr = (struct nfs_stat *)pdaindr[cpu].pda->nfsstat;
	if (ptr)
		return((caddr_t)&(ptr->svstat));
	return(0);
}

/*
 * clear all nfsstat
 */
int
nfs_clearstat(void)
{
	int i;
	caddr_t ptr;

	/*
	 * make sure we have the correct permissions
	 */
	if (!_CAP_ABLE(CAP_DAC_OVERRIDE) || !_CAP_ABLE(CAP_MAC_READ) ||
		!_CAP_ABLE(CAP_MAC_WRITE) || !_CAP_ABLE(CAP_NETWORK_MGT)) {
			return(EPERM);
	}
	for (i = 0; i < maxcpus; i++) {
		if ((pdaindr[i].CpuId == -1) ||
			!(ptr = pdaindr[i].pda->nfsstat))
			continue;
		bzero(ptr,sizeof(struct nfs_stat));
	}
	return(0);
}
