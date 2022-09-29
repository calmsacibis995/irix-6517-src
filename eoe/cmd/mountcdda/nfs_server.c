/*
 *  nfs_server.c
 *
 *  Description:
 *      NFS server for iso 9660 file system (CD-ROM)
 *
 *      This started out as the nfs_server.c from mountdos.  I gutted it
 *      and used its shell as the starting point for mount iso9660.
 *
 *      The stuff in this file is very straightforward, once you've read
 *      NFS documentation.  I referred constantly to
 *
 *      Network Working Group
 *      Request for Comments: 1094
 *      Sun Microsystems, Inc
 *      March, 1989
 *
 *      as I was writing this.
 *
 *  History:
 *      rogerc      12/18/90    Created
 */

#include <syslog.h>
#include <rpc/rpc.h>
#include <sys/errno.h>
#include "cdda.h"
#include "util.h"
#include "testcd_prot.h"

/*
 *  Simplify complex struct references
 */
#define as_attributes   attrstat_u.attributes
#define dr_attributes   diropres_u.diropres.attributes
#define dr_file     diropres_u.diropres.file
#define rlr_data    readlinkres_u.data
#define rr_data     readres_u.reply.data
#define rr_attributes   readres_u.reply.attributes
#define rdr_entries readdirres_u.reply.entries
#define rdr_eof     readdirres_u.reply.eof
#define sfr_reply   statfsres_u.reply

/*
 *  Convert #include <sys/errno.h> defines to NFS error codes
 */
#define errno_to_nfsstat(errno) \
	((unsigned)(errno) > LASTERRNO ? NFSERR_IO : nfs_statmap[errno])

static enum nfsstat nfs_statmap[] = {
    NFS_OK,			/* 0 */
    NFSERR_PERM,		/* EPERM = 1 */
    NFSERR_NOENT,		/* ENOENT = 2 */
    NFSERR_IO,			/* ESRCH = 3 */
    NFSERR_IO,			/* EINTR = 4 */
    NFSERR_IO,			/* EIO = 5 */
    NFSERR_NXIO,		/* ENXIO = 6 */
    NFSERR_IO,			/* E2BIG = 7 */
    NFSERR_IO,			/* ENOEXEC = 8 */
    NFSERR_IO,			/* EBADF = 9 */
    NFSERR_IO,			/* ECHILD = 10 */
    NFSERR_IO,			/* EAGAIN = 11 */
    NFSERR_IO,			/* ENOMEM = 12 */
    NFSERR_ACCES,		/* EACCES = 13 */
    NFSERR_IO,			/* EFAULT = 14 */
    NFSERR_IO,			/* ENOTBLK = 15 */
    NFSERR_IO,			/* EBUSY = 16 */
    NFSERR_EXIST,		/* EEXIST = 17 */
    NFSERR_IO,			/* EXDEV = 18 */
    NFSERR_NODEV,		/* ENODEV = 19 */
    NFSERR_NOTDIR,		/* ENOTDIR = 20 */
    NFSERR_ISDIR,		/* EISDIR = 21 */
    NFSERR_IO,			/* EINVAL = 22 */
    NFSERR_IO,			/* ENFILE = 23 */
    NFSERR_IO,			/* EMFILE = 24 */
    NFSERR_IO,			/* ENOTTY = 25 */
    NFSERR_IO,			/* ETXTBSY = 26 */
    NFSERR_FBIG,		/* EFBIG = 27 */
    NFSERR_NOSPC,		/* ENOSPC = 28 */
    NFSERR_IO,			/* ESPIPE = 29 */
    NFSERR_ROFS,		/* EROFS = 30 */
    (enum nfsstat) 31,		/* EMLINK = 31 */
    NFSERR_IO,			/* EPIPE = 32 */
    NFSERR_IO,			/* EDOM = 33 */
    NFSERR_IO,			/* ERANGE = 34 */
    NFSERR_IO,			/* ENOMSG = 35 */
    NFSERR_IO,			/* EIDRM = 36 */
    NFSERR_IO,			/* ECHRNG = 37 */
    NFSERR_IO,			/* EL2NSYNC = 38 */
    NFSERR_IO,			/* EL3HLT = 39 */
    NFSERR_IO,			/* EL3RST = 40 */
    NFSERR_IO,			/* ELNRNG = 41 */
    NFSERR_IO,			/* EUNATCH = 42 */
    NFSERR_IO,			/* ENOCSI = 43 */
    NFSERR_IO,			/* EL2HLT = 44 */
    NFSERR_IO,			/* EDEADLK = 45 */
    NFSERR_IO,			/* ENOLCK = 46 */
    NFSERR_IO,			/* 47 */
    NFSERR_IO,			/* 48 */
    NFSERR_IO,			/* 49 */
    NFSERR_IO,			/* EBADE = 50 */
    NFSERR_IO,			/* EBADR = 51 */
    NFSERR_IO,			/* EXFULL = 52 */
    NFSERR_IO,			/* ENOANO = 53 */
    NFSERR_IO,			/* EBADRQC = 54 */
    NFSERR_IO,			/* EBADSLT = 55 */
    NFSERR_IO,			/* EDEADLOCK = 56 */
    NFSERR_IO,			/* EBFONT = 57 */
    NFSERR_IO,			/* 58 */
    NFSERR_IO,			/* 59 */
    NFSERR_IO,			/* ENOSTR = 60 */
    NFSERR_IO,			/* ENODATA = 61 */
    NFSERR_IO,			/* ETIME = 62 */
    NFSERR_IO,			/* ENOSR = 63 */
    NFSERR_IO,			/* ENONET = 64 */
    NFSERR_IO,			/* ENOPKG = 65 */
    NFSERR_IO,			/* EREMOTE = 66 */
    NFSERR_IO,			/* ENOLINK = 67 */
    NFSERR_IO,			/* EADV = 68 */
    NFSERR_IO,			/* ESRMNT = 69 */
    NFSERR_IO,			/* ECOMM = 70 */
    NFSERR_IO,			/* EPROTO = 71 */
    NFSERR_IO,			/* 72 */
    NFSERR_IO,			/* 73 */
    NFSERR_IO,			/* EMULTIHOP = 74 */
    NFSERR_IO,			/* ELBIN = 75 */
    NFSERR_IO,			/* EDOTDOT = 76 */
    NFSERR_IO,			/* EBADMSG = 77 */
    NFSERR_IO,			/* 78 */
    NFSERR_IO,			/* 79 */
    NFSERR_IO,			/* ENOTUNIQ = 80 */
    NFSERR_IO,			/* EBADFD = 81 */
    NFSERR_IO,			/* EREMCHG = 82 */
    NFSERR_IO,			/* ELIBACC = 83 */
    NFSERR_IO,			/* ELIBBAD = 84 */
    NFSERR_IO,			/* ELIBSCN = 85 */
    NFSERR_IO,			/* ELIBMAX = 86 */
    NFSERR_IO,			/* ELIBEXEC = 87 */
    NFSERR_IO,			/* 88 */
    NFSERR_IO,			/* 89 */
    NFSERR_IO,			/* 90 */
    NFSERR_IO,			/* 91 */
    NFSERR_IO,			/* 92 */
    NFSERR_IO,			/* 93 */
    NFSERR_IO,			/* 94 */
    NFSERR_IO,			/* 95 */
    NFSERR_IO,			/* 96 */
    NFSERR_IO,			/* 97 */
    NFSERR_IO,			/* 98 */
    NFSERR_IO,			/* 99 */
    NFSERR_IO,			/* 100 */
    NFSERR_IO,			/* EWOULDBLOCK = 101 */
    NFSERR_IO,			/* EINPROGRESS = 102 */
    NFSERR_IO,			/* EALREADY = 103 */
    NFSERR_IO,			/* ENOTSOCK = 104 */
    NFSERR_IO,			/* EDESTADDRREQ = 105 */
    NFSERR_IO,			/* EMSGSIZE = 106 */
    NFSERR_IO,			/* EPROTOTYPE = 107 */
    NFSERR_IO,			/* ENOPROTOOPT = 108 */
    NFSERR_IO,			/* EPROTONOSUPPORT = 109 */
    NFSERR_IO,			/* ESOCKTNOSUPPORT = 110 */
    NFSERR_IO,			/* EOPNOTSUPP = 111 */
    NFSERR_IO,			/* EPFNOSUPPORT = 112 */
    NFSERR_IO,			/* EAFNOSUPPORT = 113 */
    NFSERR_IO,			/* EADDRINUSE = 114 */
    NFSERR_IO,			/* EADDRNOTAVAIL = 115 */
    NFSERR_IO,			/* ENETDOWN = 116 */
    NFSERR_IO,			/* ENETUNREACH = 117 */
    NFSERR_IO,			/* ENETRESET = 118 */
    NFSERR_IO,			/* ECONNABORTED = 119 */
    NFSERR_IO,			/* ECONNRESET = 120 */
    NFSERR_IO,			/* ENOBUFS = 121 */
    NFSERR_IO,			/* EISCONN = 122 */
    NFSERR_IO,			/* ENOTCONN = 123 */
    NFSERR_IO,			/* ESHUTDOWN = 124 */
    NFSERR_IO,			/* ETOOMANYREFS = 125 */
    NFSERR_IO,			/* ETIMEDOUT = 126 */
    NFSERR_IO,			/* ECONNREFUSED = 127 */
    NFSERR_IO,			/* EHOSTDOWN = 128 */
    NFSERR_IO,			/* EHOSTUNREACH = 129 */
    NFSERR_IO,			/* ELOOP = 130 */
    NFSERR_NAMETOOLONG,		/* ENAMETOOLONG = 131 */
    NFSERR_NOTEMPTY,		/* ENOTEMPTY = 132 */
    NFSERR_DQUOT,		/* EDQUOT = 133 */
    NFSERR_STALE,		/* ESTALE = 134 */
    NFSERR_IO,			/* ENFSREMOTE = 135 */
};

/*
 *  static void
 *  nfs_null()
 *
 *  Description:
 *      Do nothing
 */

static void
nfs_null()
{
}

/*
 *  static void
 *  nfs_getattr(nfs_fh *fh, attrstat *as)
 *
 *  Description:
 *      Get the attributes of fh
 *
 *  Parameters:
 *      fh  handle for file we're interested int
 *      as  receive attributes
 */

static void
nfs_getattr(nfs_fh *fh, attrstat *as)
{
    fhandle_t       *fp;
    int             error;

    fp = (fhandle_t *)fh;

    error = cdda_getattr(fp, &as->as_attributes);
    as->status = errno_to_nfsstat(error);
}

/*
 *  static void
 *  nfs_setattr(sattrargs *sa, attrstat *as)
 *
 *  Description:
 *      Read-only file system
 *
 *  Parameters:
 *      sa
 *      as
 */

static void
nfs_setattr(sattrargs *sa, attrstat *as)
{
  as->status = NFSERR_ROFS;

}

/*
 *  static void
 *  nfs_root(void)
 *
 *  Description:
 *      Obsolete
 */

static void
nfs_root(void)
{
}

/*
 *  static void
 *  nfs_lookup(diropargs *da, diropres *dr)
 *
 *  Description:
 *      Look up a file by name in a directory
 *
 *  Parameters:
 *      da
 *      dr
 *
 */

static void
nfs_lookup(diropargs *da, diropres *dr)
{
    int         error;
    fhandle_t   *fp = (fhandle_t *)&da->dir;

    error = cdda_lookup(fp, da->name, (fhandle_t *)&dr->dr_file);
    if (!error)
	error = cdda_getattr((fhandle_t *)&dr->dr_file,
			    &dr->dr_attributes);
    dr->status = errno_to_nfsstat(error);

}

/*
 *  static void
 *  nfs_readlink(nfs_fh *fh, readlinkres *rr)
 *
 *  Description:
 *      ISO 9660 doesn't this
 *
 *  Parameters:
 *      fh
 *      rr
 */

static void
nfs_readlink(nfs_fh *fh, readlinkres *rr)
{
    int error;

    error = cdda_readlink((fhandle_t *)fh, &rr->rlr_data);
    rr->status = errno_to_nfsstat(error);
}

/*
 *  static void 
 *  nfs_read(readargs *ra, readres *rr)
 *
 *  Description:
 *      Read from a file
 *
 *  Parameters:
 *      ra
 *      rr
 */

static void 
nfs_read(readargs *ra, readres *rr)
{
    fhandle_t *fp = (fhandle_t *)&ra->file;
    int error;
    static fattr cache_attr;
    static int lastfd = -1;
    
    rr->rr_data.data_val = safe_malloc(ra->count);
    error = cdda_read(fp, ra->offset, ra->count,
		     rr->rr_data.data_val, &rr->rr_data.data_len);
    if (!error) {
	/*
	 * Stating the file every time we read from it can bog us
	 * down; for sequential reads of large files, we cache the
	 * last attributes.
	 */
	if (lastfd != fp->fh_fno) {
	  error = cdda_getattr(fp, &cache_attr);
	}

	if (!error) {
	    bcopy(&cache_attr, &rr->rr_attributes, sizeof rr->rr_attributes);
	    lastfd = fp->fh_fno;
	} else {
	    lastfd = -1;
	}
    }
    if (error) {
	free(rr->rr_data.data_val);
	rr->rr_data.data_val = 0;
    }
    rr->status = errno_to_nfsstat(error);
}

/*
 *  static void
 *  freereadres(readres *rr)
 *
 *  Description:
 *      Free previously allocate memory; This is used as the xdr destructor
 *
 *  Parameters:
 *      rr
 */

static void
freereadres(readres *rr)
{
    if (rr->rr_data.data_val)
	free(rr->rr_data.data_val);
}

/*
 *  static void
 *  nfs_write(writeargs *wa, attrstat *as)
 *
 *  Description:
 *      Read only file system
 *
 *  Parameters:
 *      wa
 *      as
 */

static void
nfs_write(writeargs *wa, attrstat *as)
{
    as->status = errno_to_nfsstat(EROFS);
}

/*
 *  static void
 *  nfs_create(createargs *ca, diropres *dr)
 *
 *  Description:
 *      Read only file system
 *
 *  Parameters:
 *      ca
 *      dr
 */

static void
nfs_create(createargs *ca, diropres *dr)
{
  dr->status = errno_to_nfsstat(EROFS);
}

/*
 *  static void
 *  nfs_remove(diropargs *da, nfsstat *ns)
 *
 *  Description:
 *      Read only file system
 *
 *  Parameters:
 *      da
 *      ns
 *
 */

static void
nfs_remove(diropargs *da, nfsstat *ns)
{
  *ns = errno_to_nfsstat(EROFS);
}

/*
 *  static void
 *  nfs_rename(renameargs *ra, nfsstat *ns)
 *
 *  Description:
 *      Read only file system
 *
 *  Parameters:
 *      ra
 *      ns
 */

static void
nfs_rename(renameargs *ra, nfsstat *ns)
{
    *ns = errno_to_nfsstat(EROFS);
}

/*
 *  static void
 *  nfs_link(linkargs *la, nfsstat *ns)
 *
 *  Description:
 *      Read only file system
 *
 *  Parameters:
 *      la
 *      ns
 *
 */

static void
nfs_link(linkargs *la, nfsstat *ns)
{
  *ns = errno_to_nfsstat(EROFS);
}

/*
 *  static void
 *  nfs_symlink(symlinkargs *sa, nfsstat *ns)
 *
 *  Description:
 *      Read only file system
 *
 *  Parameters:
 *      sa
 *      ns
 */

static void
nfs_symlink(symlinkargs *sa, nfsstat *ns)
{
  *ns = errno_to_nfsstat(EROFS);
}

/*
 *  static void
 *  nfs_mkdir(createargs *ca, diropres *dr)
 *
 *  Description:
 *      Read only file system
 *
 *  Parameters:
 *      ca
 *      dr
 */

static void
nfs_mkdir(createargs *ca, diropres *dr)
{
  dr->status = errno_to_nfsstat(EROFS);
}

/*
 *  static void
 *  nfs_rmdir(diropargs *da, nfsstat *ns)
 *
 *  Description:
 *      Read only file system
 *
 *  Parameters:
 *      da
 *      ns
 */

static void
nfs_rmdir(diropargs *da, nfsstat *ns)
{
  *ns = errno_to_nfsstat(EROFS);
}


/*
 *  static void
 *  nfs_readdir(readdirargs *rda, readdirres *rdr)
 *
 *  Description:
 *      Read directory entries from a directory
 *
 *  Parameters:
 *      rda
 *      rdr
 */
static void nfs_readdir(readdirargs *rda, readdirres *rdr)
{
  int         error;
  fhandle_t   *fp = (fhandle_t *)&rda->dir;
  int         nread;

  rdr->rdr_entries = safe_malloc(rda->count);
  error = cdda_readdir(fp, rdr->rdr_entries, rda->count, 
 		       *(int*)rda->cookie, &rdr->rdr_eof, &nread); 
  if (error) {
    free(rdr->rdr_entries);
    rdr->rdr_entries = 0;
  }
  else if (nread == 0) {
    /*
     *  This is done because the nfs client doesn't seem to believe
     *  us when we set rdr->rdr_eof = 1; we have to actually pass
     *  a null pointer as rdr->rdr_entries.
     */
    free(rdr->rdr_entries);
    rdr->rdr_entries = 0;
  }
  rdr->status = errno_to_nfsstat(error);
}


/*
 *  static void
 *  freereaddirres(readdires *rdr)
 *
 *  Description:
 *      xdr destructor to free the entries allocated in nfs_readdir()
 *
 *  Parameters:
 *      rdr
 */

static void freereaddirres(readdirres *rdr)
{
  if (rdr->rdr_entries)
    free(rdr->rdr_entries);
}

/*
 *  static void
 *  nfs_statfs(nfs_fh *fh, statfsres *sfr)
 *
 *  Description:
 *      Get stats for the file system
 *
 *  Parameters:
 *      fh
 *      sfr
 */

static void nfs_statfs(nfs_fh *fh, statfsres *sfr)
{
    fhandle_t   *fp = (fhandle_t *)fh;
    int         error;
    
    sfr->sfr_reply.tsize = NFS_MAXDATA;
    sfr->sfr_reply.bsize = cdda_getblksize();
    sfr->sfr_reply.bfree = 0;
    sfr->sfr_reply.bavail = 0;
    error = cdda_numblocks(&sfr->sfr_reply.blocks);
    sfr->status = errno_to_nfsstat(error);
}

static void
sgi_nfs_null(void)
{
}

static void
sgi_nfs_root(nfspath *path, rootres *rr, struct svc_req *rq)
{
    int         error;
    fhandle_t   *fh;
    
    fh = (fhandle_t *)rr->rootres_u.file.data;
    
    error = cdda_getfs(*path, fh, rq);
    rr->status = errno_to_nfsstat(error);
}

static void
testcd_readblock(breadargs *ra, breadres *rr)
{
    int     error;

    error = cdda_readraw(ra->block, rr->breadres_u.block,
			sizeof (rr->breadres_u.block));

    rr->status = errno_to_nfsstat(error);
}

/*
 *  Table for de-multiplexing NFS calls.  The index into this table is
 *  the same as the NFS procedure number for the relevant function
 */
static struct dispatch {
    char        *name;
    void        (*proc)();
    unsigned    argsize;
    xdrproc_t   xdrargs;
    unsigned    resultsize;
    xdrproc_t   xdrresults;
    void        (*freeres)();
} dispatch[] = {
    { "NULL", nfs_null,
	  0, xdr_void, 0, xdr_void, 0 },
    { "GETATTR", nfs_getattr,
	  sizeof(nfs_fh), xdr_nfs_fh,
	  sizeof(attrstat), xdr_attrstat, 0 },
    { "SETATTR", nfs_setattr,
	  sizeof(sattrargs), xdr_sattrargs,
	  sizeof(attrstat), xdr_attrstat, 0 },
    /*
     *  Note that this is a departure from the NFS protocol; this is
     *  "SGI_NFS".
     */
    { "SGI_ROOT", sgi_nfs_root,
	  sizeof (nfspath), xdr_nfspath,
	  sizeof (rootres), xdr_rootres, 0 },
    { "LOOKUP", nfs_lookup,
	  sizeof(diropargs), xdr_diropargs,
	  sizeof(diropres), xdr_diropres, 0 },
    { "READLINK", nfs_readlink,
	  sizeof(nfs_fh), xdr_nfs_fh,
	  sizeof(readlinkres), xdr_readlinkres,
	  0 },
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
	  sizeof(statfsres), xdr_statfsres, 0 },
    { "READBLOCK", testcd_readblock,
	   sizeof(breadargs), xdr_breadargs,
	   sizeof(breadres), xdr_breadres, 0 }
};

union nfsargs {
    nfs_fh      nfs_fh;
    sattrargs   sattrargs;
    diropargs   diropargs;
    readargs    readargs;
    writeargs   writeargs;
    createargs  createargs;
    renameargs  renameargs;
    linkargs    linkargs;
    readdirargs readdirargs;
    nfspath     nfspath;
    breadargs   breadargs;
};

union nfsresults {
    attrstat    attrstat;
    diropres    diropres;
    readlinkres readlinkres;
    readres     readres;
    nfsstat     nfsstat;
    readdirres  readdirres;
    statfsres   statfsres;
    rootres     rootres;
    breadres    breadres;
};

/*
 *  void
 *  nfs_service(struct svc_req *rq, SVCXPRT *transp)
 *
 *  Description:
 *      Handle NFS service requests
 *
 *  Parameters:
 *      rq
 *      transp
 */

void
nfs_service(struct svc_req *rq, SVCXPRT *transp)
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
