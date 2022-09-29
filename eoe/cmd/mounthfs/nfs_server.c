/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1993 Silicon Graphics, Inc.                *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ident "$Revision: 1.8 $"

/*
 * User-level NFS server for HFS filesystems.
 */
#include <syslog.h>
#include <rpc/rpc.h>
#include <sys/errno.h>
#include "hfs.h"

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


/********
 * fhtohp	Convert NFS Filehandle to hnode pointer
 */
static hnode_t * fhtohp(nfs_fh *nfh, nfsstat *status){
  fhandle_t *fh = (fhandle_t *)nfh;
  hfs_t *hfs;
  hnode_t *hp;
  hno_t hno;
  int error;
  
  if ((hfs = fs_get(fh->fh_fsid)) == NULL) {
    *status = NFSERR_STALE;
    return 0;
  }

  if (FS_ISSET(hfs,CORRUPTED)){
    *status = errno_to_nfsstat(ENXIO);
    return 0;
  }

  hno = fh->fh_fid.lfid_fno;

  /*
   * HACK: Set NFS_MAP to indicate that failure to find an entry does not
   *       mean the file system is corrupted.  If NFSMNT_PRIVATE is set,
   *       a NFS IOs can occur *after* a file has been deleted. (Mostly because
   *       deleting the data fork deletes the resource fork but NFS doesn't
   *       know that.)  Normally, failure to find an entry would indicate
   *       a error but not here.
   */
  FS_SET(hfs,NFS_MAP);
  error = hn_get(hfs, hno, &hp);
  FS_CLR(hfs,NFS_MAP);
  if (error) {
    *status = errno_to_nfsstat(error);
    return 0;
  }
  return hp;
}


/********
 * makefh	Generate a file handle for an hnode
 */
static void makefh(hnode_t *hp, nfs_fh *nfh){
  fhandle_t *fh;
  
  fh = (fhandle_t *) nfh;
  bzero(fh, sizeof *fh);
  fh->fh_fsid = hp->h_hfs->fs_fsid;
  fh->fh_fid.lfid_len = sizeof fh->fh_fid - sizeof fh->fh_fid.lfid_len;
  fh->fh_fid.lfid_fno = hp->h_hno;
}


/************
 * makerootfh	Generates a root file header.
 *
 * :randyh We set the fsid field to zero.  This is one of the areas that
 * may prevent us from mounting multiple partitions.
 */
void makerootfh(nfs_fh *nfh){
  fhandle_t *fh;
  
  fh = (fhandle_t *) nfh;
  bzero(fh, sizeof *fh);
  fh->fh_fid.lfid_len = sizeof fh->fh_fid - sizeof fh->fh_fid.lfid_len;
  fh->fh_fid.lfid_fno = ROOTHNO;
}


/**********
 * nfs_null	Does nothing.
 */
static void nfs_null(){}


/*************
 * nfs_getattr	Get file attributes
 */
static void nfs_getattr(nfs_fh *fh, attrstat *as){
  hnode_t *hp;
  int error;
  
  hp = fhtohp(fh, &as->status);
  if (hp == 0)
    return;
  error = hfs_getattr(hp, &as->as_attributes);
  hn_put(hp);
  as->status = errno_to_nfsstat(error);
}


/*************
 * nfs_setattr	Set file attributes.
 */
static void nfs_setattr(sattrargs *sa, attrstat *as){
  hnode_t *hp;
  int error;
  
  hp = fhtohp(&sa->file, &as->status);
  if (hp == NULL)
    return;
  if (FS_ISSET(hp->h_hfs,RDONLY))
    error = EROFS;
  else {
    error = hfs_setattr(hp, &sa->attributes);
    if (!error)
      error = hfs_getattr(hp, &as->as_attributes);
  }
  hn_put(hp);
  as->status = errno_to_nfsstat(error);
}


/************
 * nfs_lookup	Find a file by name
 */
static void nfs_lookup(diropargs *da,diropres *dr){
  hnode_t *hp;
  hnode_t *chp;
  int error;

  hp = fhtohp(&da->dir, &dr->status);
  if (hp == NULL)
    return;
  error = hfs_lookup(hp, da->name, &chp);
  if (!error) {
    error = hfs_getattr(chp, &dr->dr_attributes);
    if (!error)
      makefh(chp, &dr->dr_file);
    hn_put(chp);
  }

  hn_put(hp);
  dr->status = errno_to_nfsstat(error);
}


/**************
 * nfs_readlink
 */
static void nfs_readlink(nfs_fh *fh, readlinkres *rlr){
  hnode_t *hp;
  int error;
  
  hp = fhtohp(fh, &rlr->status);
  if (hp == NULL)
    return;
  if (hp->h_ftype != NFLNK)
    error = ENXIO;
  else {
    rlr->rlr_data = (nfspath)safe_malloc(NFS_MAXPATHLEN);
    error = hfs_readlink(hp, NFS_MAXPATHLEN, rlr->rlr_data);
  }

  rlr->status = errno_to_nfsstat(error);
}


/*****************
 * freereadlinkres	Frees the storage allocated for nfs_readlink
 */
static void freereadlinkres(readlinkres *rlr){
  if (rlr->rlr_data)
    free(rlr->rlr_data);
}


/**********
 * nfs_read	Read data from a file
 */
static void nfs_read(readargs *ra, readres *rr,	struct svc_req *rq){
  hnode_t *hp;
  int error;
  
  hp = fhtohp(&ra->file, &rr->status);
  if (hp == NULL)
    return;
  if (hp->h_ftype != NFREG)
    error = EISDIR;
  else {
    error = hfs_access(hp, AUP(rq), AREAD);
    if (!error) {
      rr->rr_data.data_val = (char*)safe_malloc(ra->count);
      error = hfs_read(hp, ra->offset, ra->count,
		       rr->rr_data.data_val,
		       &rr->rr_data.data_len);
      if (!error)
	error = hfs_getattr(hp, &rr->rr_attributes);
    }
  }
  hn_put(hp);
  rr->status = errno_to_nfsstat(error);
}


/*************
 * freereadres	Free storage allocated for nfs_read
 */
static void freereadres(readres *rr){
  if (rr->rr_data.data_val)
    free(rr->rr_data.data_val);
}


/***********
 * nfs_write	Write data to a file
 */
static void nfs_write(writeargs *wa, attrstat *as, struct svc_req *rq){
  hnode_t *hp;
  int error=0;

  hp = fhtohp(&wa->file, &as->status);
  if (hp == NULL){
    /*
     * If we didn't find the node we assume that its because of it's
     * a Handle pointing to a deleted file.  We set a success
     * status and this results in the data being quietly discarded.
     */
    if (as->status == NFSERR_NOENT)
      as->status = NFS_OK;
    return;
  }
  if (FS_ISSET(hp->h_hfs,RDONLY))
    error = EROFS;
  else if (hp->h_ftype != NFREG)
	  error = EISDIR;
  else {
    /*
     * HACK:  We need to allow the creator of a readonly file to write
     *        it following a creat().  Normally we would allow the caller
     *        to write if it is the owner of the file.  However, we don't
     *        have ownership in HFS.  For now, we skip the access check
     *        if the CREATE flag is set, indicating that we created the
     *        file.  This isn't safe but will have to do until a better
     *        scheme is implemented.
     */
    if (FR_ISCLR(hp->h_frec,CREATE))
      error = hfs_access(hp, AUP(rq), AWRITE);
    if (!error) {
      error = hfs_write(hp, wa->offset, wa->data.data_len,
			wa->data.data_val);
      if (!error)
	error = hfs_getattr(hp, &as->as_attributes);
    }
  }
  hn_put(hp);
  as->status = errno_to_nfsstat(error);
}


/************
 * nfs_create	Create a new file
 */
static void nfs_create(createargs *ca, diropres *dr, struct svc_req *rq){
  hnode_t *hp;
  hnode_t *chp;
  int error;
  
  hp = fhtohp(&ca->where.dir, &dr->status);
  if (hp == NULL)
    return;
  if (FS_ISSET(hp->h_hfs,RDONLY))
    error = EROFS;
  else {
    error = hfs_create(hp, ca->where.name, &ca->attributes, &chp, AUP(rq));
    if (!error) {
      error = hfs_getattr(chp, &dr->dr_attributes);
      if (!error)
	makefh(chp, &dr->dr_file);
      hn_put(chp);
    }
  }
  hn_put(hp);
  dr->status = errno_to_nfsstat(error);
}


/************
 * nfs_remove	Remove a file from a directory
 */
static void nfs_remove(diropargs *da, nfsstat *ns){
  hnode_t *hp;
  int error;
  
  hp = fhtohp(&da->dir, ns);
  if (hp == NULL)
    return;
  if (FS_ISSET(hp->h_hfs,RDONLY))
    error = EROFS;
  else
    error = hfs_remove(hp, da->name);
  hn_put(hp);
  *ns = errno_to_nfsstat(error);
}


/************
 * nfs_rename	Rename a file
 */
static void nfs_rename(renameargs *rna,	nfsstat *ns){
  hnode_t *hp, *thp;
  int error;
  
  hp = fhtohp(&rna->from.dir, ns);
  if (hp == NULL)
    return;
  thp = fhtohp(&rna->to.dir, ns);
  if (thp == NULL) {
    hn_put(hp);
    return;
  }
  if (FS_ISSET(hp->h_hfs,RDONLY) || FS_ISSET(thp->h_hfs,RDONLY)){
    error = EROFS;
  } else
    error = hfs_rename(hp, rna->from.name, thp, rna->to.name);
  hn_put(hp);
  hn_put(thp);
  *ns = errno_to_nfsstat(error);
}


/**********
 * nfs_link	Create a link to a file
 */
static void nfs_link(linkargs *la, nfsstat *ns){
  hnode_t *hp, *thp;
  int error;
  
  hp = fhtohp(&la->from, ns);
  if (hp == NULL)
    return;
  thp = fhtohp(&la->to.dir, ns);
  if (thp == NULL) {
    hn_put(hp);
    return;
  }
  if (FS_ISSET(hp->h_hfs,RDONLY) || FS_ISSET(thp->h_hfs,RDONLY)) {
    error = EROFS;
  } else
    error = hfs_link(hp, thp, la->to.name);
  hn_put(hp);
  hn_put(thp);
  *ns = errno_to_nfsstat(error);
}


/*************
 * nfs_symlink	Create a symbolic link
 */
static void nfs_symlink(symlinkargs *sla, nfsstat *ns){
  hnode_t *hp;
  int error;
  
  hp = fhtohp(&sla->from.dir, ns);
  if (hp == NULL)
    return;
  if (FS_ISSET(hp->h_hfs,RDONLY))
    error = EROFS;
  else {
    error = hfs_symlink(hp, sla->from.name, &sla->attributes,
			sla->to);
  }
  hn_put(hp);
  *ns = errno_to_nfsstat(error);
}


/************
  * nfs_mkdir	Create a new directory
  */
static void nfs_mkdir(createargs *ca, diropres *dr, struct svc_req *rq){
  hnode_t *hp;
  hnode_t *chp;
  int error;
  
  hp = fhtohp(&ca->where.dir, &dr->status);
  if (hp == NULL)
    return;
  if (FS_ISSET(hp->h_hfs,RDONLY))
    error = EROFS;
  else {
    error = hfs_mkdir(hp, ca->where.name, &ca->attributes, &chp, AUP(rq));
    if (!error) {
      error = hfs_getattr(chp, &dr->dr_attributes);
      if (!error)
	makefh(chp, &dr->dr_file);
      hn_put(chp);
    }
  }
  hn_put(hp);
  dr->status = errno_to_nfsstat(error);
}


/***********
 * nfs_rmdir	Remove a directory
 */
static void nfs_rmdir(diropargs *da, nfsstat *ns){
  hnode_t *hp;
  int error;
  
  hp = fhtohp(&da->dir, ns);
  if (hp == NULL)
    return;
  if (FS_ISSET(hp->h_hfs,RDONLY))
    error = EROFS;
  else
    error = hfs_rmdir(hp, da->name);
  hn_put(hp);
  *ns = errno_to_nfsstat(error);
}


/*************
 * nfs_readdir	Read a stream of directory entries
 */
static void nfs_readdir(readdirargs *rda, readdirres *rdr, struct svc_req *rq){
  hnode_t *hp;
  int error;
  u_int nread;
  
  hp = fhtohp(&rda->dir, &rdr->status);
  if (hp == NULL)
    return;
  if (hp->h_ftype != NFDIR)
    error = ENOTDIR;
  else {
    error = hfs_access(hp, AUP(rq), AREAD);
    if (!error) {
      rdr->rdr_entries = (entry *) safe_malloc(rda->count);
      error = hfs_readdir(hp, rda->cookie, rda->count,
			  rdr->rdr_entries, &rdr->rdr_eof, &nread);
      if (nread == 0) {
	free(rdr->rdr_entries);
	rdr->rdr_entries = 0;
      }
    }
  }
  hn_put(hp);
  rdr->status = errno_to_nfsstat(error);
}


/***************
 * frereaddirres	Free storage allocated by nfs_readdir
 */
static void freereaddirres(readdirres *rdr){
  if (rdr->rdr_entries)
    free(rdr->rdr_entries);
}


/************
 * nfs_statfs	Get status of a file system
 */
static void nfs_statfs(nfs_fh *fh, statfsres *sfr){
  hnode_t *hp;
  int error;
  
  hp = fhtohp(fh, &sfr->status);
  if (hp == NULL)
    return;
  sfr->sfr_reply.tsize = NFS_MAXDATA;
  error = hfs_statfs(hp->h_hfs, &sfr->sfr_reply);
  hn_put(hp);
  sfr->status = errno_to_nfsstat(error);
}


/*
 * Nfs dispatch vector
 */
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
  nfs_fh	nfs_fh;
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
  readres	readres;
  nfsstat	nfsstat;
  readdirres	readdirres;
  statfsres	statfsres;
};


/*************
 * nfs_service	Service NFS reqests
 */
void nfs_service(struct svc_req *rq, SVCXPRT *transp){
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

  
  trace(" %s\n", disp->name);

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



