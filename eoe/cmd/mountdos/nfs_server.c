/*
 * User-level NFS server for DOS filesystems.
 */
#include <syslog.h>
#include <rpc/rpc.h>
#include <sys/errno.h>
#include "dos_fs.h"

#define	as_attributes	attrstat_u.attributes
#define	dr_attributes	diropres_u.diropres.attributes
#define	dr_file		diropres_u.diropres.file
#define	rlr_data	readlinkres_u.data
#define	rr_data		readres_u.reply.data
#define	rr_attributes	readres_u.reply.attributes
#define	rdr_entries	readdirres_u.reply.entries
#define	rdr_eof		readdirres_u.reply.eof
#define	sfr_reply	statfsres_u.reply

#define	AUP(rq)	((struct authunix_parms *) (rq)->rq_clntcred)

#define	errno_to_nfsstat(errno) \
	((unsigned)(errno) > LASTERRNO ? NFSERR_IO : nfs_statmap[errno])

static enum nfsstat nfs_statmap[] = {
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
	NFSERR_IO,		/* EAGAIN = 11 */
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
	NFSERR_IO,		/* EINVAL = 22 */
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
	NFSERR_IO,		/* 78 */
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
	NFSERR_IO,		/* 90 */
	NFSERR_IO,		/* 91 */
	NFSERR_IO,		/* 92 */
	NFSERR_IO,		/* 93 */
	NFSERR_IO,		/* 94 */
	NFSERR_IO,		/* 95 */
	NFSERR_IO,		/* 96 */
	NFSERR_IO,		/* 97 */
	NFSERR_IO,		/* 98 */
	NFSERR_IO,		/* 99 */
	NFSERR_IO,		/* 100 */
	NFSERR_IO,		/* EWOULDBLOCK = 101 */
	NFSERR_IO,		/* EINPROGRESS = 102 */
	NFSERR_IO,		/* EALREADY = 103 */
	NFSERR_IO,		/* ENOTSOCK = 104 */
	NFSERR_IO,		/* EDESTADDRREQ = 105 */
	NFSERR_IO,		/* EMSGSIZE = 106 */
	NFSERR_IO,		/* EPROTOTYPE = 107 */
	NFSERR_IO,		/* ENOPROTOOPT = 108 */
	NFSERR_IO,		/* EPROTONOSUPPORT = 109 */
	NFSERR_IO,		/* ESOCKTNOSUPPORT = 110 */
	NFSERR_IO,		/* EOPNOTSUPP = 111 */
	NFSERR_IO,		/* EPFNOSUPPORT = 112 */
	NFSERR_IO,		/* EAFNOSUPPORT = 113 */
	NFSERR_IO,		/* EADDRINUSE = 114 */
	NFSERR_IO,		/* EADDRNOTAVAIL = 115 */
	NFSERR_IO,		/* ENETDOWN = 116 */
	NFSERR_IO,		/* ENETUNREACH = 117 */
	NFSERR_IO,		/* ENETRESET = 118 */
	NFSERR_IO,		/* ECONNABORTED = 119 */
	NFSERR_IO,		/* ECONNRESET = 120 */
	NFSERR_IO,		/* ENOBUFS = 121 */
	NFSERR_IO,		/* EISCONN = 122 */
	NFSERR_IO,		/* ENOTCONN = 123 */
	NFSERR_IO,		/* ESHUTDOWN = 124 */
	NFSERR_IO,		/* ETOOMANYREFS = 125 */
	NFSERR_IO,		/* ETIMEDOUT = 126 */
	NFSERR_IO,		/* ECONNREFUSED = 127 */
	NFSERR_IO,		/* EHOSTDOWN = 128 */
	NFSERR_IO,		/* EHOSTUNREACH = 129 */
	NFSERR_IO,		/* ELOOP = 130 */
	NFSERR_NAMETOOLONG,	/* ENAMETOOLONG = 131 */
	NFSERR_NOTEMPTY,	/* ENOTEMPTY = 132 */
	NFSERR_DQUOT,		/* EDQUOT = 133 */
	NFSERR_STALE,		/* ESTALE = 134 */
	NFSERR_IO,		/* ENFSREMOTE = 135 */
};

static struct dnode *
fhtodp(nfs_fh *nfh, nfsstat *status)
{
	fhandle_t *fh;
	struct dfs *dfs;
	struct dnode *dp;
	int error;

	fh = (fhandle_t *) nfh;
	dfs = dos_getfs(fh->fh_dev);
	if (dfs == 0) {
		*status = NFSERR_STALE;
		return 0;
	}
	error = dos_getnode(dfs, fh->fh_fno, &dp);
	if (error) {
		*status = errno_to_nfsstat(error);
		return 0;
	}
	return dp;
}

static void
makefh(struct dnode *dp, nfs_fh *nfh)
{
	fhandle_t *fh;

	fh = (fhandle_t *) nfh;
	bzero(fh, sizeof *fh);
	fh->fh_dev = dp->d_dfs->dfs_dev;
	fh->fh_fid.lfid_len = sizeof fh->fh_fid - sizeof fh->fh_fid.lfid_len;
	fh->fh_fid.lfid_fno = dp->d_fno;
}

static void
nfs_null()
{
}

static void
nfs_getattr(fh, as)
	nfs_fh *fh;
	attrstat *as;
{
	struct dnode *dp;
	int error;

	dp = fhtodp(fh, &as->status);
	if (dp == 0)
		return;
	error = dos_getattr(dp, &as->as_attributes);
	if (error == 134){
		printf(" Stale File Handle \n");
        }
	dos_putnode(dp);
	as->status = errno_to_nfsstat(error);
}

static void
nfs_setattr(sa, as)
	sattrargs *sa;
	attrstat *as;
{
	struct dnode *dp;
	int error;

	dp = fhtodp(&sa->file, &as->status);
	if (dp == 0)
		return;
	if (dp->d_dfs->dfs_flags & DFS_RDONLY)
		error = EROFS;
	else {
		error = dos_setattr(dp, &sa->attributes);
		if (!error)
			error = dos_getattr(dp, &as->as_attributes);
	}
	dos_putnode(dp);
	as->status = errno_to_nfsstat(error);
}

static void
nfs_lookup(da, dr)
	diropargs *da;
	diropres *dr;
{
	struct dnode *dp;
	struct dnode *cdp;
	int error;

	dp = fhtodp(&da->dir, &dr->status);
	if (dp == 0)
		return;
	error = dos_lookup(dp, da->name, &cdp);
	if (!error) {
		error = dos_getattr(cdp, &dr->dr_attributes);
		if (!error)
			makefh(cdp, &dr->dr_file);
		dos_putnode(cdp);
	}
	dos_putnode(dp);
	dr->status = errno_to_nfsstat(error);
}

static void
nfs_readlink(fh, rlr)
	nfs_fh *fh;
	readlinkres *rlr;
{
	struct dnode *dp;
	int error;

	dp = fhtodp(fh, &rlr->status);
	if (dp == 0)
		return;
	if (dp->d_ftype != NFLNK)
		error = ENXIO;
	else {
		rlr->rlr_data = safe_malloc(NFS_MAXPATHLEN);
		error = dos_readlink(dp, NFS_MAXPATHLEN, rlr->rlr_data);
	}
	dos_putnode(dp);
	rlr->status = errno_to_nfsstat(error);
}

static void
freereadlinkres(rlr)
	readlinkres *rlr;
{
	if (rlr->rlr_data)
		free(rlr->rlr_data);
}

static void
nfs_read(ra, rr, rq)
	readargs *ra;
	readres *rr;
	struct svc_req *rq;
{
	struct dnode *dp;
	int error;

	dp = fhtodp(&ra->file, &rr->status);
	if (dp == 0)
		return;
	if (dp->d_ftype != NFREG)
		error = EISDIR;
	else {
		error = dos_access(dp, AUP(rq), AREAD);
		if (!error) {
			rr->rr_data.data_val = safe_malloc(ra->count);
			error = dos_read(dp, ra->offset, ra->count,
					 rr->rr_data.data_val,
					 &rr->rr_data.data_len);
			if (!error)
				error = dos_getattr(dp, &rr->rr_attributes);
		}
	}
	dos_putnode(dp);
	rr->status = errno_to_nfsstat(error);
}

static void
freereadres(rr)
	readres *rr;
{
	if (rr->rr_data.data_val)
		free(rr->rr_data.data_val);
}

static void
nfs_write(wa, as, rq)
	writeargs *wa;
	attrstat *as;
	struct svc_req *rq;
{
	struct dnode *dp;
	int error=0;

	dp = fhtodp(&wa->file, &as->status);
	if (dp == 0)
		return;
	if (dp->d_dfs->dfs_flags & DFS_RDONLY)
		error = EROFS;
	else if (dp->d_ftype != NFREG)
		error = EISDIR;
	else {
		/* kludge to allow creator of readonly file to write */
		if (AUP(rq)->aup_uid != dp->d_uid)
			error = dos_access(dp, AUP(rq), AWRITE);
		if (!error) {
			error = dos_write(dp, wa->offset, wa->data.data_len,
					  wa->data.data_val);
			if (!error)
				error = dos_getattr(dp, &as->as_attributes);
		}
	}
	dos_putnode(dp);
	as->status = errno_to_nfsstat(error);
}

static void
nfs_create(ca, dr, rq)
	createargs *ca;
	diropres *dr;
	struct svc_req *rq;
{
	struct dnode *dp;
	struct dnode *cdp;
	int error;

	dp = fhtodp(&ca->where.dir, &dr->status);
	if (dp == 0)
		return;
	if (dp->d_dfs->dfs_flags & DFS_RDONLY)
		error = EROFS;
	else {
		error = dos_create(dp, ca->where.name, &ca->attributes, &cdp, AUP(rq));
		if (!error) {
			error = dos_getattr(cdp, &dr->dr_attributes);
			if (!error)
				makefh(cdp, &dr->dr_file);
			dos_putnode(cdp);
		}
	}
	dos_putnode(dp);
	dr->status = errno_to_nfsstat(error);
}

static void
nfs_remove(da, ns)
	diropargs *da;
	nfsstat *ns;
{
	struct dnode *dp;
	int error;

	dp = fhtodp(&da->dir, ns);
	if (dp == 0)
		return;
	if (dp->d_dfs->dfs_flags & DFS_RDONLY)
		error = EROFS;
	else
		error = dos_remove(dp, da->name);
	dos_putnode(dp);
	*ns = errno_to_nfsstat(error);
}

static void
nfs_rename(rna, ns)
	renameargs *rna;
	nfsstat *ns;
{
	struct dnode *dp, *tdp;
	int error;

	dp = fhtodp(&rna->from.dir, ns);
	if (dp == 0)
		return;
	tdp = fhtodp(&rna->to.dir, ns);
	if (tdp == 0) {
		dos_putnode(dp);
		return;
	}
	if ((dp->d_dfs->dfs_flags & DFS_RDONLY)
	    || (tdp->d_dfs->dfs_flags & DFS_RDONLY)) {
		error = EROFS;
	} else
		error = dos_rename(dp, rna->from.name, tdp, rna->to.name);
	dos_putnode(dp);
	dos_putnode(tdp);
	*ns = errno_to_nfsstat(error);
}

static void
nfs_link(la, ns)
	linkargs *la;
	nfsstat *ns;
{
	struct dnode *dp, *tdp;
	int error;

	dp = fhtodp(&la->from, ns);
	if (dp == 0)
		return;
	tdp = fhtodp(&la->to.dir, ns);
	if (tdp == 0) {
		dos_putnode(dp);
		return;
	}
	if ((dp->d_dfs->dfs_flags & DFS_RDONLY)
	    || (tdp->d_dfs->dfs_flags & DFS_RDONLY)) {
		error = EROFS;
	} else
		error = dos_link(dp, tdp, la->to.name);
	dos_putnode(dp);
	dos_putnode(tdp);
	*ns = errno_to_nfsstat(error);
}

static void
nfs_symlink(sla, ns)
	symlinkargs *sla;
	nfsstat *ns;
{
	struct dnode *dp;
	int error;

	dp = fhtodp(&sla->from.dir, ns);
	if (dp == 0)
		return;
	if (dp->d_dfs->dfs_flags & DFS_RDONLY)
		error = EROFS;
	else {
		error = dos_symlink(dp, sla->from.name, &sla->attributes,
				    sla->to);
	}
	dos_putnode(dp);
	*ns = errno_to_nfsstat(error);
}

static void
nfs_mkdir(ca, dr, rq)
	createargs *ca;
	diropres *dr;
	struct svc_req *rq;
{
	struct dnode *dp;
	struct dnode *cdp;
	int error;

	dp = fhtodp(&ca->where.dir, &dr->status);
	if (dp == 0)
		return;
	if (dp->d_dfs->dfs_flags & DFS_RDONLY)
		error = EROFS;
	else {
		error = dos_mkdir(dp, ca->where.name, &ca->attributes, &cdp, AUP(rq));
		if (!error) {
			error = dos_getattr(cdp, &dr->dr_attributes);
			if (!error)
				makefh(cdp, &dr->dr_file);
			dos_putnode(cdp);
		}
	}
	dos_putnode(dp);
	dr->status = errno_to_nfsstat(error);
}

static void
nfs_rmdir(da, ns)
	diropargs *da;
	nfsstat *ns;
{
	struct dnode *dp;
	int error;

	dp = fhtodp(&da->dir, ns);
	if (dp == 0)
		return;
	if (dp->d_dfs->dfs_flags & DFS_RDONLY)
		error = EROFS;
	else
		error = dos_rmdir(dp, da->name);
	dos_putnode(dp);
	*ns = errno_to_nfsstat(error);
}

static void
nfs_readdir(rda, rdr, rq)
	readdirargs *rda;
	readdirres *rdr;
	struct svc_req *rq;
{
	struct dnode *dp;
	int error;
	u_int nread;

	dp = fhtodp(&rda->dir, &rdr->status);
	if (dp == 0)
		return;
	if (dp->d_ftype != NFDIR)
		error = ENOTDIR;
	else {
		error = dos_access(dp, AUP(rq), AREAD);
		if (!error) {
			rdr->rdr_entries = (entry *) safe_malloc(rda->count);
			error = dos_readdir(dp, rda->cookie, rda->count,
					    rdr->rdr_entries, &rdr->rdr_eof, &nread);
			if (nread == 0) {
				free(rdr->rdr_entries);
				rdr->rdr_entries = 0;
			}
		}
	}
	dos_putnode(dp);
	rdr->status = errno_to_nfsstat(error);
}

static void
freereaddirres(rdr)
	readdirres *rdr;
{
	if (rdr->rdr_entries)
		free(rdr->rdr_entries);
}

static void
nfs_statfs(fh, sfr)
	nfs_fh *fh;
	statfsres *sfr;
{
	struct dnode *dp;
	int error;

	dp = fhtodp(fh, &sfr->status);
	if (dp == 0)
		return;
	sfr->sfr_reply.tsize = NFS_MAXDATA;
	error = dos_statfs(dp->d_dfs, &sfr->sfr_reply);
	dos_putnode(dp);
	sfr->status = errno_to_nfsstat(error);
}

static struct dispatch {
	char		*name;
	void		(*proc)();
	unsigned	argsize;
	xdrproc_t	xdrargs;
	unsigned	resultsize;
	xdrproc_t	xdrresults;
	void		(*freeres)();
} dispatch[] = {
	{ "NULL", nfs_null,
		0, xdr_void, 0, xdr_void, 0 },
	{ "GETATTR", nfs_getattr,
		sizeof(nfs_fh), xdr_nfs_fh,
		sizeof(attrstat), xdr_attrstat, 0 },
	{ "SETATTR", nfs_setattr,
		sizeof(sattrargs), xdr_sattrargs,
		sizeof(attrstat), xdr_attrstat, 0 },
	{ "ROOT", nfs_null,
		0, xdr_void, 0, xdr_void, 0 },
	{ "LOOKUP", nfs_lookup,
		sizeof(diropargs), xdr_diropargs,
		sizeof(diropres), xdr_diropres, 0 },
	{ "READLINK", nfs_readlink,
		sizeof(nfs_fh), xdr_nfs_fh,
		sizeof(readlinkres), xdr_readlinkres,
		freereadlinkres },
	{ "READ", nfs_read,
		sizeof(readargs), xdr_readargs,
		sizeof(readres), xdr_readres,
		freereadres },
	{ "WRITECACHE", nfs_null,
		0, xdr_void, 0, xdr_void, 0 },
	{ "WRITE", nfs_write,
		sizeof(writeargs), xdr_writeargs,
		sizeof(attrstat), xdr_attrstat, 0 },
	{ "CREATE", nfs_create,
		sizeof(createargs), xdr_createargs,
		sizeof(diropres), xdr_diropres, 0 },
	{ "REMOVE", nfs_remove,
		sizeof(diropargs), xdr_diropargs,
		sizeof(nfsstat), xdr_nfsstat, 0 },
	{ "RENAME", nfs_rename,
		sizeof(renameargs), xdr_renameargs,
		sizeof(nfsstat), xdr_nfsstat, 0 },
	{ "LINK", nfs_link,
		sizeof(linkargs), xdr_linkargs,
		sizeof(nfsstat), xdr_nfsstat, 0 },
	{ "SYMLINK", nfs_symlink,
		sizeof(symlinkargs), xdr_symlinkargs,
		sizeof(nfsstat), xdr_nfsstat, 0 },
	{ "MKDIR", nfs_mkdir,
		sizeof(createargs), xdr_createargs,
		sizeof(diropres), xdr_diropres, 0 },
	{ "RMDIR", nfs_rmdir,
		sizeof(diropargs), xdr_diropargs,
		sizeof(nfsstat), xdr_nfsstat, 0 },
	{ "READDIR", nfs_readdir,
		sizeof(readdirargs), xdr_readdirargs,
		sizeof(readdirres), xdr_readdirres,
		freereaddirres },
	{ "STATFS", nfs_statfs,
		sizeof(nfs_fh), xdr_nfs_fh,
		sizeof(statfsres), xdr_statfsres, 0 }
};

union nfsargs {
	nfs_fh		nfs_fh;
	sattrargs	sattrargs;
	diropargs	diropargs;
	readargs	readargs;
	writeargs	writeargs;
	createargs	createargs;
	renameargs	renameargs;
	linkargs	linkargs;
	readdirargs	readdirargs;
};

union nfsresults {
	attrstat	attrstat;
	diropres	diropres;
	readlinkres	readlinkres;
	readres		readres;
	nfsstat		nfsstat;
	readdirres	readdirres;
	statfsres	statfsres;
};

void
nfs_service(rq, transp)
	struct svc_req *rq;
	SVCXPRT *transp;
{
	u_long which;
	struct dispatch *disp;
	char args[sizeof(union nfsargs)];
	char results[sizeof(union nfsresults)];

	which = rq->rq_proc;
	if (which >= sizeof dispatch / sizeof dispatch[0]) {
		svcerr_noproc(transp);
		return;
	}
	disp = &dispatch[which];

	bzero(args, disp->argsize);
	if (!svc_getargs(transp, disp->xdrargs, args)) {
		svcerr_decode(transp);
		return;
	}

	if (which != NFSPROC_NULL) {
		if (rq->rq_cred.oa_flavor != AUTH_UNIX) {
			svcerr_weakauth(transp);
			return;
		}
		/* authenticate ? */
	}

	trace("calling %s\n", disp->name);
	bzero(results, disp->resultsize);
	(*disp->proc)(args, results, rq);
	if (!svc_sendreply(transp, disp->xdrresults, results))
		svcerr_systemerr(transp);
	if (!svc_freeargs(transp, disp->xdrargs, args)) {
		syslog(LOG_ERR, "can't free %s arguments", disp->name);
		terminate(1);
	}
	if (disp->freeres)
		(*disp->freeres)(results);
}
