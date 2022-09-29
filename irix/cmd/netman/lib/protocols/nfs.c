/*
 * Copyright 1990 Silicon Graphics, Inc.  All rights reserved.
 *
 * Sun Network File System (NFS).
 */
#include <stdio.h>
#include <rpc/types.h>
#include <sys/socket.h>
#include <rpc/xdr.h>
#ifdef sun
#include <errno.h>
#include <protocols/nfs.h>
#else
#include <sys/fs/nfs.h>
#endif
#include <netinet/in.h>		/* for ntohl */
#include "debug.h"
#include "enum.h"
#include "heap.h"
#include "protodefs.h"
#include "protocols/sunrpc.h"
#include "strings.h"

char	nfsname[] = "nfs";

enum nfsfid {
	PROC, FH, STATUS,
	TYPE, MODE, NLINK, UID, GID, SIZE, BLOCKSIZE, RDEV, BLOCKS, FSID,
	NODEID, ATIME, MTIME, CTIME,
	OFFSET, COUNT, TOTCOUNT, BEGOFF,
	TSIZE, BSIZE, BFREE, BAVAIL,
	NAME, PATH, TOFH, TONAME, NEOF
};

/*
 * Selected NFS fields.  Field offsets are not used, so we order by degree
 * of commonality and then by procedure number.
 */
#define	TVSIZE		sizeof(struct timeval)
#define	PFI_TIME	PFI_ADDR

static ProtoField nfsfields[] = {
/* proc is a pseudo-field used to re-decode the sunrpc proc number */
	PFI_VAR("proc",     "Procedure",	PROC,	0,	PV_TERSE),

/* the common arguments and results members */
	PFI_BYTE("fh",	    "File Handle",	FH,	NFS_FHSIZE,PV_DEFAULT),
	PFI_UINT("status",  "Status",		STATUS,	enum nfsstat,PV_TERSE),

/* nfsfattr and sattr members */
	PFI_UINT("type",    "File Type", 	TYPE,enum nfsftype,PV_DEFAULT),
	PFI_UINT("mode",    "Mode", 		MODE,	u_long,	PV_DEFAULT),
	PFI_UINT("nlink",   "Number of Links",	NLINK,	u_long,	PV_DEFAULT),
	PFI_SINT("uid",	    "User ID", 		UID,	long,	PV_DEFAULT),
	PFI_SINT("gid",	    "Group ID", 	GID,	long,	PV_DEFAULT),
	PFI_UINT("size",    "Size", 		SIZE,	u_long,	PV_DEFAULT),
	PFI_UINT("blocksize","Block Size",	BLOCKSIZE,u_long,PV_DEFAULT),
	PFI_UINT("rdev",    "Device Number",	RDEV,	u_long,	PV_DEFAULT),
	PFI_UINT("blocks",  "Blocks",		BLOCKS,	u_long,	PV_DEFAULT),
	PFI_UINT("fsid",    "File System ID",	FSID,	u_long,	PV_DEFAULT),
	PFI_UINT("nodeid",  "File Node ID",	NODEID,	u_long,	PV_DEFAULT),
	PFI_TIME("atime",   "Access Time",	ATIME,	TVSIZE,	PV_DEFAULT),
	PFI_TIME("mtime",   "Modification Time",MTIME,	TVSIZE,	PV_DEFAULT),
	PFI_TIME("ctime",   "Change Time",	CTIME,	TVSIZE,	PV_DEFAULT),

/* readargs and writeargs members */
	PFI_UINT("offset",  "Offset", 		OFFSET,	u_long,	PV_DEFAULT),
	PFI_UINT("count",   "Count",		COUNT,	u_long,	PV_DEFAULT),
	PFI_UINT("totcount","Total Count",	TOTCOUNT,u_long,PV_DEFAULT),
	PFI_UINT("begoff",  "Beginning Offset",	BEGOFF,	u_long,	PV_DEFAULT),

/* nfsstatfsok members, except for blocks */
	PFI_UINT("tsize",   "Transfer Size",	TSIZE,	u_long,	PV_DEFAULT),
	PFI_UINT("bsize",   "Block Size",	BSIZE,	u_long,	PV_DEFAULT),
	PFI_UINT("bfree",   "Blocks Free",	BFREE,	u_long,	PV_DEFAULT),
	PFI_UINT("bavail",  "Blocks Available",	BAVAIL,	u_long,	PV_DEFAULT),

/* diropargs, readlinkres, renameargs, etc. */
	PFI_VAR("name",	    "Filename",		NAME, NFS_MAXNAMLEN,PV_DEFAULT),
	PFI_VAR("path",	    "Pathname",		PATH,NFS_MAXPATHLEN,PV_DEFAULT),
	PFI_BYTE("tofh",    "Target File Handle",TOFH,NFS_FHSIZE,PV_DEFAULT),
	PFI_VAR("toname",   "Target Filename", TONAME,NFS_MAXNAMLEN,PV_DEFAULT),
	PFI_UINT("eof",	    "EOF",		NEOF, u_long,	PV_DEFAULT),
};

#define	NFSFID(pf)	((enum nfsfid) (pf)->pf_id)
#define	NFSFIELD(fid)	nfsfields[(int) fid]

/*
 * Nickname macros defined in the sunrpc protocol's scope.
 */
static ProtoMacro nfsnicknames[] = {
	PMI("NFS",	nfsname),
};

/*
 * Some enumerations.  First, NFS procedure dispatch numbers.
 */
static Enumeration nfsproc;
static Enumerator nfsprocvec[] = {
	EI_VAL("NULL",		RFS_NULL),
	EI_VAL("GETATTR",	RFS_GETATTR),
	EI_VAL("SETATTR",	RFS_SETATTR),
	EI_VAL("ROOT",		RFS_ROOT),
	EI_VAL("LOOKUP",	RFS_LOOKUP),
	EI_VAL("READLINK",	RFS_READLINK),
	EI_VAL("READ",		RFS_READ),
	EI_VAL("WRITECACHE",	RFS_WRITECACHE),
	EI_VAL("WRITE",		RFS_WRITE),
	EI_VAL("CREATE",	RFS_CREATE),
	EI_VAL("REMOVE",	RFS_REMOVE),
	EI_VAL("RENAME",	RFS_RENAME),
	EI_VAL("LINK",		RFS_LINK),
	EI_VAL("SYMLINK",	RFS_SYMLINK),
	EI_VAL("MKDIR",		RFS_MKDIR),
	EI_VAL("RMDIR",		RFS_RMDIR),
	EI_VAL("READDIR",	RFS_READDIR),
	EI_VAL("STATFS",	RFS_STATFS)
};

/*
 * NFS error status.
 */
static Enumeration nfsstat;
static Enumerator nfsstatvec[] = {
	EI(NFS_OK),
	EI(NFSERR_PERM),
	EI(NFSERR_NOENT),
	EI(NFSERR_IO),
	EI(NFSERR_NXIO),
	EI(NFSERR_ACCES),
	EI(NFSERR_EXIST),
	EI(NFSERR_NODEV),
	EI(NFSERR_NOTDIR),
	EI(NFSERR_ISDIR),
	EI(NFSERR_FBIG),
	EI(NFSERR_NOSPC),
	EI(NFSERR_ROFS),
	EI(NFSERR_NAMETOOLONG),
	EI(NFSERR_NOTEMPTY),
	EI(NFSERR_DQUOT),
	EI(NFSERR_STALE),
	EI_VAL("NFSERR_WFLUSH", 99)	/* according to NFS4.0 nfs_prot.x */
};

/*
 * NFS file types.
 */
static Enumeration nfsftype;
static Enumerator nfsftypevec[] = {
	EI(NFNON),
	EI(NFREG),
	EI(NFDIR),
	EI(NFBLK),
	EI(NFCHR),
	EI(NFLNK)
};

/*
 * Decode state struct and xdr filter for nfs-specific state.
 */
struct nfsdecodestate {
	ProtoDecodeState  state;	/* base class state */
	u_long		  vers;		/* NFS protocol version */
	u_long		  proc;		/* and procedure number */
	enum msg_type	  direction;	/* call or reply */
	u_int		  fragno;	/* fragment number */
};

static int
xdr_nfsstate(xdr, nds)
	XDR *xdr;
	struct nfsdecodestate *nds;
{
	return xdr_u_long(xdr, &nds->vers)
	    && xdr_u_long(xdr, &nds->proc)
	    && xdr_enum(xdr, (enum_t *) &nds->direction)
	    && xdr_u_int(xdr, &nds->fragno);
}

/*
 * Most of the NFS protocol ops can be stubbed.
 */
#define	nfs_setopt	pr_stub_setopt
#define	nfs_embed	pr_stub_embed
#define	nfs_compile	pr_stub_compile
#define	nfs_match	pr_stub_match

DefineProtocol(nfs, nfsname, "Sun Network File System", PRID_NFS,
	       DS_BIG_ENDIAN, NFS_MAXDATA, 0, 0, 0, 0, xdr_nfsstate);

static int
nfs_init()
{
	if (!pr_register(&nfs_proto, nfsfields, lengthof(nfsfields),
			 lengthof(nfsprocvec) + lengthof(nfsstatvec)
			 + lengthof(nfsftypevec))) {
		return 0;
	}
	if (!pr_nest(&nfs_proto, PRID_SUNRPC, NFS_PROGRAM, nfsnicknames,
		     lengthof(nfsnicknames))) {
		return 0;
	}
	en_init(&nfsproc, nfsprocvec, lengthof(nfsprocvec), &nfs_proto);
	en_init(&nfsstat, nfsstatvec, lengthof(nfsstatvec), &nfs_proto);
	en_init(&nfsftype, nfsftypevec, lengthof(nfsftypevec), &nfs_proto);
	pr_addconstmacro(&nfs_proto, "isfifo", "type == NFCHR && rdev == ~0");
	return 1;
}

/*
 * Resolve a timeval or file handle expression into an expr node.
 * We assume interesting file handles on a given Unix server can be
 * distinguished by fsid and nodeid (dev&ino).
 */
#define	EXOP_FHANDLE	EXOP_ADDRESS
#define	EXOP_TIMEVAL	EXOP_ADDRESS

struct fhandle {
	u_long	fsid;
	u_long	nodeid;
};

#define	ex_fh(ex)	A_CAST(&(ex)->ex_addr, struct fhandle)
#define	ex_tv(ex)	A_CAST(&(ex)->ex_addr, struct timeval)

/* ARGSUSED */
static Expr *
nfs_resolve(char *str, int len, struct snooper *sn)
{
	Expr *ex;
	struct fhandle fh;
	struct timeval tv;

	if (sscanf(str, "%lx:%lu", &fh.fsid, &fh.nodeid) == 2) {
		ex = expr(EXOP_FHANDLE, EX_NULLARY, str);
		*ex_fh(ex) = fh;
		return ex;
	}
	if (sscanf(str, "%ld.%ld", &tv.tv_sec, &tv.tv_usec) == 2) {
		ex = expr(EXOP_TIMEVAL, EX_NULLARY, str);
		*ex_tv(ex) = tv;
		return ex;
	}
	return ex_string(str, len);
}

/*
 * Template for the front part of an SGI server's file handle.
 */
struct sgifhfront {
	u_short	pad;		/* pad to align file id */
	dev_t	fsid;		/* filesystem id */
	u_short len;		/* file id length */
	u_short	pad2;		/* pad to align nodeid */
	/*hack, hack; irix6.4 coredump here ino_t */unsigned	nodeid;		/* node id */
};
#define	sgifh(fh)	((struct sgifhfront *)(fh))

/*
 * Macros used by nfs_fetch and its kids.  The caller must declare ds, rex,
 * and fid appropriately.
 */
#define	NFS_FETCH_U_LONG(longfid) { \
	if (!ds_u_long(ds, &rex->ex_val)) \
		return 0; \
	if (fid == (longfid)) { \
		rex->ex_op = EXOP_NUMBER; \
		return 1; \
	} \
}

fhandle_t *nfs_fetch_fh(DataStream *, u_int *);
int	  nfs_fetch_fattr(enum nfsfid, DataStream *, Expr *);
int	  nfs_fetch_sattr(enum nfsfid, DataStream *, Expr *);
int	  nfs_fetch_string(DataStream *, long, struct string *, long *, long *);

#define	NFS_FETCH_FH(fhfid) { \
	fhandle_t *fh = nfs_fetch_fh(ds, 0); \
	if (fh == 0) \
		return 0; \
	if (fid == (fhfid)) { \
		rex->ex_op = EXOP_FHANDLE; \
		ex_fh(rex)->fsid = sgifh(fh)->fsid; \
		ex_fh(rex)->nodeid = sgifh(fh)->nodeid; \
		return 1; \
	} \
}

#define	NFS_FETCH_TIMEVAL(tvfid) { \
	struct timeval *tv = ex_tv(rex); \
	if (!ds_long(ds, &tv->tv_sec) || !ds_long(ds, &tv->tv_usec)) \
		return 0; \
	if (fid == (tvfid)) { \
		rex->ex_op = EXOP_TIMEVAL; \
		return 1; \
	} \
}

#define	NFS_FETCH_STRING(sfid) { \
	if (!nfs_fetch_string(ds, NFSFIELD(sfid).pf_cookie, &rex->ex_str, \
			      0, 0)) { \
		return 0; \
	} \
	if (fid == sfid) { \
		rex->ex_op = EXOP_STRING; \
		return 1; \
	} \
}

static int
nfs_fetch(ProtoField *pf, DataStream *ds, ProtoStack *ps, Expr *rex)
{
	enum nfsfid fid;
	struct sunrpcframe *srf;

	fid = NFSFID(pf);
	srf = PS_TOP(ps, struct sunrpcframe);
	if (fid == PROC) {
		rex->ex_op = EXOP_NUMBER;
		rex->ex_val = srf->srf_proc;
		return 1;
	}

	/*
	 * NFS arguments (well, who knows about ROOT and WRITECACHE?) begin
	 * with a file handle.  Results begin with an nfsstat.
	 */
	if (srf->srf_direction == CALL) {
		NFS_FETCH_FH(FH);
	} else {
		NFS_FETCH_U_LONG(STATUS);
	}

	switch (srf->srf_proc) {
	  case RFS_NULL:
	  case RFS_ROOT:
	  case RFS_WRITECACHE:
		break;

	  case RFS_GETATTR:
		if (srf->srf_direction == REPLY)
			return nfs_fetch_fattr(fid, ds, rex);
		break;

	  case RFS_SETATTR:
		if (srf->srf_direction == CALL)
			return nfs_fetch_sattr(fid, ds, rex);
		return nfs_fetch_fattr(fid, ds, rex);

	  case RFS_LOOKUP:
		if (srf->srf_direction == CALL) {
		          NFS_FETCH_FH(FH);
			NFS_FETCH_STRING(NAME);
		} else {
			NFS_FETCH_FH(FH);
			return nfs_fetch_fattr(fid, ds, rex);
		}
		break;

	  case RFS_READLINK:
		if (srf->srf_direction == REPLY) {
			NFS_FETCH_STRING(PATH);
		}
		break;

	  case RFS_READ:
		if (srf->srf_direction == CALL) {
			NFS_FETCH_U_LONG(OFFSET);
			NFS_FETCH_U_LONG(COUNT);
			NFS_FETCH_U_LONG(TOTCOUNT);
			return 0;
		}
		return nfs_fetch_fattr(fid, ds, rex);

	  case RFS_WRITE:
		if (srf->srf_direction == CALL) {
			NFS_FETCH_U_LONG(BEGOFF);
			NFS_FETCH_U_LONG(OFFSET);
			NFS_FETCH_U_LONG(TOTCOUNT);
			NFS_FETCH_U_LONG(COUNT);
			return 0;
		}
		return nfs_fetch_fattr(fid, ds, rex);

	  case RFS_CREATE:
	  case RFS_MKDIR:
		if (srf->srf_direction == CALL) {
			NFS_FETCH_STRING(NAME);
			return nfs_fetch_sattr(fid, ds, rex);
		}
		NFS_FETCH_FH(FH);
		return nfs_fetch_fattr(fid, ds, rex);

	  case RFS_REMOVE:
	  case RFS_RMDIR:
		if (srf->srf_direction == CALL) {
			NFS_FETCH_STRING(NAME);
		}
		break;

	  case RFS_RENAME:
		if (srf->srf_direction == CALL) {
			NFS_FETCH_STRING(NAME);
			NFS_FETCH_FH(TOFH);
			NFS_FETCH_STRING(TONAME);
		}
		break;

	  case RFS_LINK:
		if (srf->srf_direction == CALL) {
			NFS_FETCH_FH(TOFH);
			NFS_FETCH_STRING(TONAME);
		}
		break;

	  case RFS_SYMLINK:
		if (srf->srf_direction == CALL) {
			NFS_FETCH_STRING(NAME);
			NFS_FETCH_STRING(PATH);
			return nfs_fetch_sattr(fid, ds, rex);
		}
		break;

	  case RFS_READDIR:
		if (srf->srf_direction == CALL) {
			NFS_FETCH_U_LONG(OFFSET);
			NFS_FETCH_U_LONG(COUNT);
		}
		break;

	  case RFS_STATFS:
		if (srf->srf_direction == REPLY) {
			NFS_FETCH_U_LONG(TSIZE);
			NFS_FETCH_U_LONG(BSIZE);
			NFS_FETCH_U_LONG(BLOCKS);
			NFS_FETCH_U_LONG(BFREE);
			NFS_FETCH_U_LONG(BAVAIL);
		}
	}
	return 0;
}

static fhandle_t *
nfs_fetch_fh(DataStream *ds, u_int *sizep)
{
	fhandle_t *fh;
	u_int size;

	fh = (fhandle_t *) ds_inline(ds, sizeof *fh, BYTES_PER_XDR_UNIT);
	if (fh) {
		if (sizep)
			*sizep = sizeof *fh;
		return fh;
	}
	assert(ds->ds_count < sizeof *fh);
	size = MAX(ds->ds_count, sizeof(struct sgifhfront));
	if (sizep)
		*sizep = size;
	return (fhandle_t *) ds_inline(ds, size, BYTES_PER_XDR_UNIT);
}

static int
nfs_fetch_fattr(enum nfsfid fid, DataStream *ds, Expr *rex)
{
	struct nfsfattr *na;

	na = (struct nfsfattr *) ds_inline(ds, sizeof *na, BYTES_PER_XDR_UNIT);
	if (na) {
		rex->ex_op = EXOP_NUMBER;
		switch (fid) {
		  case TYPE:
			rex->ex_val = ntohl((u_long)na->na_type);
			break;
		  case MODE:
			rex->ex_val = ntohl(na->na_mode);
			break;
		  case NLINK:
			rex->ex_val = ntohl(na->na_nlink);
			break;
		  case UID:
			rex->ex_val = ntohl(na->na_uid);
			break;
		  case GID:
			rex->ex_val = ntohl(na->na_gid);
			break;
		  case SIZE:
			rex->ex_val = ntohl(na->na_size);
			break;
		  case BLOCKSIZE:
			rex->ex_val = ntohl(na->na_blocksize);
			break;
		  case RDEV:
			rex->ex_val = ntohl(na->na_rdev);
			break;
		  case BLOCKS:
			rex->ex_val = ntohl(na->na_blocks);
			break;
		  case FSID:
			rex->ex_val = ntohl(na->na_fsid);
			break;
		  case NODEID:
			rex->ex_val = ntohl(na->na_nodeid);
			break;
		  case ATIME:
			rex->ex_op = EXOP_TIMEVAL;
			ex_tv(rex)->tv_sec = ntohl(na->na_atime.tv_sec);
			ex_tv(rex)->tv_usec = ntohl(na->na_atime.tv_usec);
			break;
		  case MTIME:
			rex->ex_op = EXOP_TIMEVAL;
			ex_tv(rex)->tv_sec = ntohl(na->na_mtime.tv_sec);
			ex_tv(rex)->tv_usec = ntohl(na->na_mtime.tv_usec);
			break;
		  case CTIME:
			rex->ex_op = EXOP_TIMEVAL;
			ex_tv(rex)->tv_sec = ntohl(na->na_ctime.tv_sec);
			ex_tv(rex)->tv_usec = ntohl(na->na_ctime.tv_usec);
		}
		return 1;
	}
	NFS_FETCH_U_LONG(TYPE);
	NFS_FETCH_U_LONG(MODE);
	NFS_FETCH_U_LONG(NLINK);
	NFS_FETCH_U_LONG(UID);
	NFS_FETCH_U_LONG(GID);
	NFS_FETCH_U_LONG(SIZE);
	NFS_FETCH_U_LONG(BLOCKSIZE);
	NFS_FETCH_U_LONG(RDEV);
	NFS_FETCH_U_LONG(BLOCKS);
	NFS_FETCH_U_LONG(FSID);
	NFS_FETCH_U_LONG(NODEID);
	NFS_FETCH_TIMEVAL(ATIME);
	NFS_FETCH_TIMEVAL(MTIME);
	NFS_FETCH_TIMEVAL(CTIME);
	return 0;
}

static int
nfs_fetch_sattr(enum nfsfid fid, DataStream *ds, Expr *rex)
{
	struct nfssattr *sa;

	sa = (struct nfssattr *) ds_inline(ds, sizeof *sa, BYTES_PER_XDR_UNIT);
	if (sa) {
		rex->ex_op = EXOP_NUMBER;
		switch (fid) {
		  case MODE:
			rex->ex_val = ntohl(sa->sa_mode);
			break;
		  case UID:
			rex->ex_val = ntohl(sa->sa_uid);
			break;
		  case GID:
			rex->ex_val = ntohl(sa->sa_gid);
			break;
		  case SIZE:
			rex->ex_val = ntohl(sa->sa_size);
			break;
		  case ATIME:
			rex->ex_op = EXOP_TIMEVAL;
			ex_tv(rex)->tv_sec = ntohl(sa->sa_atime.tv_sec);
			ex_tv(rex)->tv_usec = ntohl(sa->sa_atime.tv_usec);
			break;
		  case MTIME:
			rex->ex_op = EXOP_TIMEVAL;
			ex_tv(rex)->tv_sec = ntohl(sa->sa_mtime.tv_sec);
			ex_tv(rex)->tv_usec = ntohl(sa->sa_mtime.tv_usec);
		}
		return 1;
	}
	NFS_FETCH_U_LONG(MODE);
	NFS_FETCH_U_LONG(UID);
	NFS_FETCH_U_LONG(GID);
	NFS_FETCH_U_LONG(SIZE);
	NFS_FETCH_TIMEVAL(ATIME);
	NFS_FETCH_TIMEVAL(MTIME);
	return 0;
}

#undef	NFS_FETCH_U_LONG
#undef	NFS_FETCH_FH
#undef 	NFS_FETCH_TIMEVAL
#undef	NFS_FETCH_STRING

static int
nfs_fetch_string(DataStream *ds, long maxlen, struct string *sp, long *xdrlen,
		 long *size)
{
	long len;

	if (!ds_long(ds, &len))
		return 0;
	if (len < 0) {
	    len = 0;
	    return 0;
	}
	if (xdrlen)
		*xdrlen = len;
	if (maxlen > ds->ds_count);
		maxlen = ds->ds_count;
	if (len > maxlen)
		len = maxlen;
	sp->s_len = len;
	sp->s_ptr = (char *) ds->ds_next;
	len = RNDUP(len);
	if (size)
		*size = sizeof len + len;
	ds->ds_count -= len;
	ds->ds_next += len;
	return 1;
}

#define	NFS_DECODE_U_LONG(fid) { \
	u_long val; \
	(void) ds_u_long(ds, &val); \
	pv_showfield(pv, &NFSFIELD(fid), &val, "%-10lu", val); \
}

void	nfs_decode_dirents(DataStream *, PacketView *);
void	nfs_decode_fh(enum nfsfid, DataStream *, PacketView *);
void	nfs_decode_fattr(DataStream *, PacketView *);
void	nfs_decode_sattr(DataStream *, PacketView *);
void	nfs_decode_string(enum nfsfid, DataStream *, PacketView *);

static void
nfs_decode(DataStream *ds, ProtoStack *ps, PacketView *pv)
{
	struct nfsdecodestate *nds;
	struct sunrpcframe *srf;

	nds = PS_STATE(ps, struct nfsdecodestate);
	if (nds) {
		nds->fragno++;
		pv_printf(pv, "(%s %s, %u%s captured fragment)\n",
			  en_name(&nfsproc, nds->proc),
			  nds->direction == CALL ? "call" : "reply",
			  nds->fragno, (nds->fragno == 2) ? "nd" :
			  (nds->fragno == 3) ? "rd" : "th");
		switch (nds->proc) {
		  case RFS_READ:
		  case RFS_WRITE:
			if (pv->pv_level >= PV_VERBOSE)
				pv_decodehex(pv, ds, 0, ds->ds_count);
			break;
		  case RFS_READDIR:
			/* XXX find valid dirent and resume decoding */;
		}
		return;
	}

	/*
	 * Show the NFS procedure name.
	 */
	srf = PS_TOP(ps, struct sunrpcframe);
	pv_replayfield(pv, &NFSFIELD(PROC), &srf->srf_proc,
		       srf->srf_procoff, sizeof(u_long),
		       "%-10.10s", en_name(&nfsproc, srf->srf_proc));

	if (srf->srf_proc >= RFS_NPROC)
		return;

	/*
	 * If there are more fragments, save decode state.
	 */
	if (srf->srf_morefrags) {
		nds = new(struct nfsdecodestate);
		nds->state.pds_mysize = sizeof *nds;
		nds->state.pds_proto = &nfs_proto;
		nds->vers = srf->srf_vers;
		nds->proc = srf->srf_proc;
		nds->direction = srf->srf_direction;
		nds->fragno = 1;
		ps->ps_state = &nds->state;
	}

	/*
	 * Show common call or reply fields.
	 */
	if (srf->srf_direction == CALL)
		nfs_decode_fh(FH, ds, pv);
	else {
		enum nfsstat status;

		(void) ds_long(ds, (long *) &status);
		pv_showfield(pv, &NFSFIELD(STATUS), &status,
			     "%-18.18s", en_name(&nfsstat, (int) status));
		if (status != NFS_OK)
			return;
	}
	pv_break(pv);

	switch (srf->srf_proc) {
	  case RFS_NULL:
	  case RFS_ROOT:
	  case RFS_WRITECACHE:
		break;

	  case RFS_GETATTR:
		if (srf->srf_direction == REPLY)
			nfs_decode_fattr(ds, pv);
		break;

	  case RFS_SETATTR:
		if (srf->srf_direction == CALL)
			nfs_decode_sattr(ds, pv);
		else
			nfs_decode_fattr(ds, pv);
		break;

	  case RFS_LOOKUP:
		if (srf->srf_direction == CALL)   {
		          nfs_decode_fh(FH, ds, pv);
			nfs_decode_string(NAME, ds, pv);
		        }
		else {
			nfs_decode_fh(FH, ds, pv);
			nfs_decode_fattr(ds, pv);
		}
		break;

	  case RFS_READLINK:
		if (srf->srf_direction == REPLY)
			nfs_decode_string(PATH, ds, pv);
		break;

	  case RFS_READ:
		if (srf->srf_direction == CALL) {
			NFS_DECODE_U_LONG(OFFSET);
			NFS_DECODE_U_LONG(COUNT);
			NFS_DECODE_U_LONG(TOTCOUNT);
		} else {
			nfs_decode_fattr(ds, pv);
			NFS_DECODE_U_LONG(COUNT);
			if (pv->pv_level >= PV_VERBOSE)
				pv_decodehex(pv, ds, 0, ds->ds_count);
		}
		break;

	  case RFS_WRITE:
		if (srf->srf_direction == CALL) {
			NFS_DECODE_U_LONG(BEGOFF);
			NFS_DECODE_U_LONG(OFFSET);
			NFS_DECODE_U_LONG(TOTCOUNT);
			NFS_DECODE_U_LONG(COUNT);
			if (pv->pv_level >= PV_VERBOSE)
				pv_decodehex(pv, ds, 0, ds->ds_count);
		} else
			nfs_decode_fattr(ds, pv);
		break;

	  case RFS_CREATE:
	  case RFS_MKDIR:
		if (srf->srf_direction == CALL) {
			nfs_decode_string(NAME, ds, pv);
			nfs_decode_sattr(ds, pv);
		} else {
			nfs_decode_fh(FH, ds, pv);
			nfs_decode_fattr(ds, pv);
		}
		break;

	  case RFS_REMOVE:
	  case RFS_RMDIR:
		if (srf->srf_direction == CALL)
			nfs_decode_string(NAME, ds, pv);
		break;

	  case RFS_RENAME:
		if (srf->srf_direction == CALL) {
			nfs_decode_string(NAME, ds, pv);
			nfs_decode_fh(TOFH, ds, pv);
			nfs_decode_string(TONAME, ds, pv);
		}
		break;

	  case RFS_LINK:
		if (srf->srf_direction == CALL) {
			nfs_decode_fh(TOFH, ds, pv);
			nfs_decode_string(TONAME, ds, pv);
		}
		break;

	  case RFS_SYMLINK:
		if (srf->srf_direction == CALL) {
			nfs_decode_string(NAME, ds, pv);
			nfs_decode_string(PATH, ds, pv);
			nfs_decode_sattr(ds, pv);
		}
		break;

	  case RFS_READDIR:
		if (srf->srf_direction == CALL) {
			NFS_DECODE_U_LONG(OFFSET);
			NFS_DECODE_U_LONG(COUNT);
		} else
			nfs_decode_dirents(ds, pv);
		break;

	  case RFS_STATFS:
		if (srf->srf_direction == REPLY) {
			NFS_DECODE_U_LONG(TSIZE);
			NFS_DECODE_U_LONG(BSIZE);
			NFS_DECODE_U_LONG(BLOCKS);
			NFS_DECODE_U_LONG(BFREE);
			NFS_DECODE_U_LONG(BAVAIL);
		}
		break;
	}
}

static void
nfs_decode_dirents(DataStream *ds, PacketView *pv)
{
	long more;

	for (;;) {
		pv->pv_off += sizeof(more);	/* should we show more? */
		if (!ds_long(ds, &more))
			return;
		if (!more)
			break;
		NFS_DECODE_U_LONG(NODEID);
		nfs_decode_string(NAME, ds, pv);
		NFS_DECODE_U_LONG(OFFSET);
		pv_break(pv);
	}
	NFS_DECODE_U_LONG(NEOF);
}

#undef	NFS_DECODE_U_LONG

static void
nfs_decode_fh(enum nfsfid fid, DataStream *ds, PacketView *pv)
{
	fhandle_t *fh;
	u_int size;
	int cc, n;
	char buf[72];
		
	fh = nfs_fetch_fh(ds, &size);
	if (fh == 0)
		return;
	if (size < sizeof *fh) {
		cc = 0;
		size /= sizeof fh->fh_hash[0];
		for (n = 0; n < size; n++) {
			cc += nsprintf(&buf[cc], sizeof buf - cc, "%s%lx",
				       (cc == 0) ? "" : " ", fh->fh_hash[n]);
		}
		pv_showfield(pv, &NFSFIELD(fid), fh, "%x:%lu [%s ???]",
			     sgifh(fh)->fsid, sgifh(fh)->nodeid, buf);
	} else {
		pv_showfield(pv, &NFSFIELD(fid), fh,
			     "%x:%lu [%lx %lx %lx %lx %lx %lx %lx %lx]",
			     sgifh(fh)->fsid, sgifh(fh)->nodeid,
			     fh->fh_hash[0], fh->fh_hash[1],
			     fh->fh_hash[2], fh->fh_hash[3],
			     fh->fh_hash[4], fh->fh_hash[5],
			     fh->fh_hash[6], fh->fh_hash[7]);
	}
}

/*
 * Macros used by nfs_decode_fattr and nfs_decode_sattr.
 */
#define	pv_show_nfs_mode(pv, mode) \
	pv_showfield(pv, &NFSFIELD(MODE), &(mode), "%#-5lo", mode)

#define	pv_show_nfs_uid(pv, uid) \
	pv_showfield(pv, &NFSFIELD(UID), &(uid), "%-5ld", uid)

#define	pv_show_nfs_gid(pv, gid) \
	pv_showfield(pv, &NFSFIELD(GID), &(gid), "%-5ld", gid)

#define	pv_show_nfs_size(pv, size) \
	pv_showfield(pv, &NFSFIELD(SIZE), &(size), "%-10lu", size)

#define	pv_show_nfs_timeval(pv, fid, tv) \
	pv_showfield(pv, &NFSFIELD(fid), (tv), \
		     "%ld.%ld", (tv)->tv_sec, (tv)->tv_usec)

static void
nfs_decode_fattr(DataStream *ds, PacketView *pv)
{
	struct nfsfattr *na, fattr;

	na = (struct nfsfattr *) ds_inline(ds, sizeof *na, BYTES_PER_XDR_UNIT);
	if (na) {
		na->na_type = (enum nfsftype) ntohl((u_long)na->na_type);
		na->na_mode = ntohl(na->na_mode);
		na->na_nlink = ntohl(na->na_nlink);
		na->na_uid = ntohl(na->na_uid);
		na->na_gid = ntohl(na->na_gid);
		na->na_size = ntohl(na->na_size);
		na->na_blocksize = ntohl(na->na_blocksize);
		na->na_rdev = ntohl(na->na_rdev);
		na->na_blocks = ntohl(na->na_blocks);
		na->na_fsid = ntohl(na->na_fsid);
		na->na_nodeid = ntohl(na->na_nodeid);
		na->na_atime.tv_sec = ntohl(na->na_atime.tv_sec);
		na->na_atime.tv_usec = ntohl(na->na_atime.tv_usec);
		na->na_mtime.tv_sec = ntohl(na->na_mtime.tv_sec);
		na->na_mtime.tv_usec = ntohl(na->na_mtime.tv_usec);
		na->na_ctime.tv_sec = ntohl(na->na_ctime.tv_sec);
		na->na_ctime.tv_usec = ntohl(na->na_ctime.tv_usec);
	} else {
		na = &fattr;
		(void)(ds_long(ds, (long *) &na->na_type)
		    && ds_u_long(ds, &na->na_mode)
		    && ds_u_long(ds, &na->na_nlink)
		    && ds_u_long(ds, &na->na_uid)
		    && ds_u_long(ds, &na->na_gid)
		    && ds_u_long(ds, &na->na_size)
		    && ds_u_long(ds, &na->na_blocksize)
		    && ds_u_long(ds, &na->na_rdev)
		    && ds_u_long(ds, &na->na_blocks)
		    && ds_u_long(ds, &na->na_fsid)
		    && ds_u_long(ds, &na->na_nodeid)
		    && ds_long(ds, &na->na_atime.tv_sec)
		    && ds_long(ds, &na->na_atime.tv_usec)
		    && ds_long(ds, &na->na_mtime.tv_sec)
		    && ds_long(ds, &na->na_mtime.tv_usec)
		    && ds_long(ds, &na->na_ctime.tv_sec)
		    && ds_long(ds, &na->na_ctime.tv_usec));
	}

	pv_break(pv);
	pv_showfield(pv, &NFSFIELD(TYPE), &na->na_type,
		     "%-5.5s", en_name(&nfsftype, (int) na->na_type));
	pv_show_nfs_mode(pv, na->na_mode);
	pv_showfield(pv, &NFSFIELD(NLINK), &na->na_nlink,
		     "%-5lu", na->na_nlink);
	pv_show_nfs_uid(pv, na->na_uid);
	pv_show_nfs_gid(pv, na->na_gid);
	pv_show_nfs_size(pv, na->na_size);
	pv_showfield(pv, &NFSFIELD(BLOCKSIZE), &na->na_blocksize,
		     "%-5lu", na->na_blocksize);
	pv_showfield(pv, &NFSFIELD(RDEV), &na->na_rdev,
		     "%#-6lx", na->na_rdev);
	pv_showfield(pv, &NFSFIELD(BLOCKS), &na->na_blocks,
		     "%-10lu", na->na_blocks);
	pv_showfield(pv, &NFSFIELD(FSID), &na->na_fsid,
		     "%#-6lx", na->na_fsid);
	pv_showfield(pv, &NFSFIELD(NODEID), &na->na_nodeid,
		     "%-10lu", na->na_nodeid);
	pv_show_nfs_timeval(pv, ATIME, &na->na_atime);
	pv_show_nfs_timeval(pv, MTIME, &na->na_mtime);
	pv_show_nfs_timeval(pv, CTIME, &na->na_ctime);
}

static void
nfs_decode_sattr(DataStream *ds, PacketView *pv)
{
	struct nfssattr *sa, sattr;

	sa = (struct nfssattr *) ds_inline(ds, sizeof *sa, BYTES_PER_XDR_UNIT);
	if (sa) {
		sa->sa_mode = ntohl(sa->sa_mode);
		sa->sa_uid = ntohl(sa->sa_uid);
		sa->sa_gid = ntohl(sa->sa_gid);
		sa->sa_size = ntohl(sa->sa_size);
		sa->sa_atime.tv_sec = ntohl(sa->sa_atime.tv_sec);
		sa->sa_atime.tv_usec = ntohl(sa->sa_atime.tv_usec);
		sa->sa_mtime.tv_sec = ntohl(sa->sa_mtime.tv_sec);
		sa->sa_mtime.tv_usec = ntohl(sa->sa_mtime.tv_usec);
	} else {
		sa = &sattr;
		(void)(ds_u_long(ds, &sa->sa_mode)
		    && ds_u_long(ds, &sa->sa_uid)
		    && ds_u_long(ds, &sa->sa_gid)
		    && ds_u_long(ds, &sa->sa_size)
		    && ds_long(ds, &sa->sa_atime.tv_sec)
		    && ds_long(ds, &sa->sa_atime.tv_usec)
		    && ds_long(ds, &sa->sa_mtime.tv_sec)
		    && ds_long(ds, &sa->sa_mtime.tv_usec));
	}

	pv_break(pv);
	pv_show_nfs_mode(pv, sa->sa_mode);
	pv_show_nfs_uid(pv, sa->sa_uid);
	pv_show_nfs_gid(pv, sa->sa_gid);
	pv_show_nfs_size(pv, sa->sa_size);
	pv_show_nfs_timeval(pv, ATIME, &sa->sa_atime);
	pv_show_nfs_timeval(pv, MTIME, &sa->sa_mtime);
}

static void
nfs_decode_string(enum nfsfid fid, DataStream *ds, PacketView *pv)
{
	ProtoField *pf;
	unsigned char *base;
	char buf[2000];
	struct string s;
	long xdrlen, size;

	pf = &NFSFIELD(fid);
	base = ds->ds_next;
	if (!nfs_fetch_string(ds, pf->pf_cookie, &s, &xdrlen, &size))
		size = 0;
	else {
		(void) nsprintf(buf, sizeof buf, "\"%.*s%s\" (%ld)",
				s.s_len, s.s_ptr,
				(s.s_len < xdrlen) ? "..." : "",
				xdrlen);
	}
	pv_showvarfield(pv, pf, base, size, "%-20s", buf);
}
