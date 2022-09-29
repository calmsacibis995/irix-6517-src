/*
 * Copyright (c) 1991 by Sun Microsystems, Inc.
 */
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/sun/snoop/RCS/snoop_nfs.c,v 1.3 1996/07/06 17:51:15 nn Exp $"

#include <sys/types.h>
#include <sys/errno.h>
#include <sys/tiuser.h>
#include <setjmp.h>

#include <rpc/types.h>
#include <rpc/xdr.h>
#include <rpc/auth.h>
#include <rpc/clnt.h>
#include <rpc/rpc_msg.h>
#include <string.h>
#include "snoop.h"

#include <sys/stat.h>
#include "nfs_prot.h"

extern char *get_sum_line();
extern void check_retransmit();
static char *perms();
static char *filetype();
char *sum_nfsfh();
static char *sum_readdirres();
static void detail_readdirres();
static void detail_diroparg();
static void nfscall2();
static void nfsreply2();
int sum_nfsstat();
int detail_nfsstat();
void detail_nfsfh();
static void detail_mode();
void detail_fattr();
static void detail_sattr();
extern jmp_buf xdr_err;
void skip_fattr();
void interpret_nfs2(int flags, int type, int xid, int vers, int proc, char *data, int len);


#define	NFS_OK			0	/* no error */
#define	NFSERR_PERM		1	/* Not owner */
#define	NFSERR_NOENT		2	/* No such file or directory */
#define	NFSERR_IO		5	/* I/O error */
#define	NFSERR_NXIO		6	/* No such device or address */
#define	NFSERR_ACCES		13	/* Permission denied */
#define	NFSERR_EXIST		17	/* File exists */
#define	NFSERR_XDEV		18	/* Cross-device link */
#define	NFSERR_NODEV		19	/* No such device */
#define	NFSERR_NOTDIR		20	/* Not a directory */
#define	NFSERR_ISDIR		21	/* Is a directory */
#define	NFSERR_INVAL		22	/* Invalid argument */
#define	NFSERR_FBIG		27	/* File too large */
#define	NFSERR_NOSPC		28	/* No space left on device */
#define	NFSERR_ROFS		30	/* Read-only file system */
#define	NFSERR_OPNOTSUPP	45	/* Operation not supported */
#define	NFSERR_NAMETOOLONG	63	/* File name too long */
#define	NFSERR_NOTEMPTY		66	/* Directory not empty */
#define	NFSERR_DQUOT		69	/* Disc quota exceeded */
#define	NFSERR_STALE		70	/* Stale NFS file handle */
#define	NFSERR_REMOTE		71	/* Object is remote */
#define	NFSERR_WFLUSH		72	/* write cache flushed */

static char *procnames_short[] = {
	"NULL2",	/*  0 */
	"GETATTR2",	/*  1 */
	"SETATTR2",	/*  2 */
	"ROOT2",	/*  3 */
	"LOOKUP2",	/*  4 */
	"READLINK2",	/*  5 */
	"READ2",	/*  6 */
	"WRITECACHE2",	/*  7 */
	"WRITE2",	/*  8 */
	"CREATE2",	/*  9 */
	"REMOVE2",	/* 10 */
	"RENAME2",	/* 11 */
	"LINK2",	/* 12 */
	"SYMLINK2",	/* 13 */
	"MKDIR2",	/* 14 */
	"RMDIR2",	/* 15 */
	"READDIR2",	/* 16 */
	"STATFS2",	/* 17 */
};

static char *procnames_long[] = {
	"Null procedure",		/*  0 */
	"Get file attributes",		/*  1 */
	"Set file attributes",		/*  2 */
	"Get root filehandle",		/*  3 */
	"Look up file name",		/*  4 */
	"Read from symbolic link",	/*  5 */
	"Read from file",		/*  6 */
	"Write to cache",		/*  7 */
	"Write to file",		/*  8 */
	"Create file",			/*  9 */
	"Remove file",			/* 10 */
	"Rename",			/* 11 */
	"Link",				/* 12 */
	"Make symbolic link",		/* 13 */
	"Make directory",		/* 14 */
	"Remove directory",		/* 15 */
	"Read from directory",		/* 16 */
	"Get filesystem attributes",	/* 17 */
};

#define	MAXPROC	17

/* ARGSUSED */
void
interpret_nfs(flags, type, xid, vers, proc, data, len)
	int flags, type, xid, vers, proc;
	char *data;
	int len;
{

	if (vers == 2) {
		interpret_nfs2(flags, type, xid, vers, proc, data, len);
		return;
	}

	if (vers == 3) {
		interpret_nfs3(flags, type, xid, vers, proc, data, len);
		return;
	}
}

void
interpret_nfs2(flags, type, xid, vers, proc, data, len)
	int flags, type, xid, vers, proc;
	char *data;
	int len;
{
	char *line;
	char buff[2048];
	int off, sz;
	char *fh;

	if (proc < 0 || proc > MAXPROC)
		return;

	if (flags & F_SUM) {
		line = get_sum_line();

		if (type == CALL) {
			(void) sprintf(line,
				"NFS C %s",
				procnames_short[proc]);
			line += strlen(line);
			switch (proc) {
			case NFSPROC_GETATTR:
			case NFSPROC_READLINK:
			case NFSPROC_STATFS:
			case NFSPROC_SETATTR:
				(void) sprintf(line, sum_nfsfh());
				break;
			case NFSPROC_LOOKUP:
			case NFSPROC_REMOVE:
			case NFSPROC_RMDIR:
			case NFSPROC_CREATE:
			case NFSPROC_MKDIR:
				fh = sum_nfsfh();
				(void) sprintf(line, "%s %s",
					fh,
					getxdr_string(buff, 1024));
				break;
			case NFSPROC_WRITE:
				fh = sum_nfsfh();
				(void) getxdr_long();	/* beginoff */
				off = getxdr_long();
				(void) getxdr_long();	/* totalcount */
				sz  = getxdr_long();
				(void) sprintf(line, "%s at %d for %d",
					fh, off, sz);
				break;
			case NFSPROC_RENAME:
				fh = sum_nfsfh();
				(void) sprintf(line, "%s %s",
					fh,
					getxdr_string(buff, 1024));
				line += strlen(line);
				fh = sum_nfsfh();
				(void) sprintf(line, " to%s %s",
					fh,
					getxdr_string(buff, 1024));
				break;
			case NFSPROC_LINK:
				fh = sum_nfsfh();
				(void) sprintf(line, "%s", fh);
				line += strlen(line);
				fh = sum_nfsfh();
				(void) sprintf(line, " to%s %s",
					fh,
					getxdr_string(buff, 1024));
				break;
			case NFSPROC_SYMLINK:
				fh = sum_nfsfh();
				(void) sprintf(line, "%s %s",
					fh,
					getxdr_string(buff, 1024));
				line += strlen(line);
				(void) sprintf(line, " to %s",
					getxdr_string(buff, 1024));
				break;
			case NFSPROC_READDIR:
				fh = sum_nfsfh();
				(void) sprintf(line, "%s Cookie=%lu",
					fh, getxdr_u_long());
				break;
			case NFSPROC_READ:
				fh = sum_nfsfh();
				off = getxdr_long();
				sz  = getxdr_long();
				(void) sprintf(line, "%s at %d for %d",
					fh, off, sz);
				break;
			default:
				break;
			}

			check_retransmit(line, (u_long)xid);
		} else {
			(void) sprintf(line, "NFS R %s ",
				procnames_short[proc]);
			line += strlen(line);
			switch (proc) {
			case NFSPROC_CREATE:
			case NFSPROC_MKDIR:
			case NFSPROC_LOOKUP:
				if (sum_nfsstat(line) == 0) {
					line += strlen(line);
					(void) sprintf(line, sum_nfsfh());
				}
				break;
			case NFSPROC_READLINK:
				if (sum_nfsstat(line) == 0) {
					line += strlen(line);
					(void) sprintf(line, " (Path=%s)",
						getxdr_string(buff, 1024));
				}
				break;
			case NFSPROC_GETATTR:
			case NFSPROC_SYMLINK:
			case NFSPROC_STATFS:
			case NFSPROC_SETATTR:
			case NFSPROC_REMOVE:
			case NFSPROC_RMDIR:
			case NFSPROC_WRITE:
			case NFSPROC_RENAME:
			case NFSPROC_LINK:
				(void) sum_nfsstat(line);
				break;
			case NFSPROC_READDIR:
				if (sum_nfsstat(line) == 0) {
					line += strlen(line);
					(void) strcat(line, sum_readdirres());
				}
				break;
			case NFSPROC_READ:
				if (sum_nfsstat(line) == 0) {
					line += strlen(line);
					xdr_skip(68); /* fattrs */
					(void) sprintf(line, " (%ld bytes)",
						getxdr_long());
				}
				break;
			default:
				break;
			}
		}
	}

	if (flags & F_DTAIL) {
		show_header("NFS:  ", "Sun NFS", len);
		show_space();
		(void) sprintf(get_line(0, 0), "Proc = %d (%s)",
			proc, procnames_long[proc]);
		if (type == CALL)
			nfscall2(proc);
		else
			nfsreply2(proc);
		show_trailer();
	}
}

/*
 *  Print out version 2 NFS call packets
 */
static void
nfscall2(proc)
	int proc;
{
	switch (proc) {
	case NFSPROC_GETATTR:
	case NFSPROC_READLINK:
	case NFSPROC_STATFS:
		detail_nfsfh();
		break;
	case NFSPROC_SETATTR:
		detail_nfsfh();
		detail_sattr();
		break;
	case NFSPROC_LOOKUP:
	case NFSPROC_REMOVE:
	case NFSPROC_RMDIR:
		detail_diroparg();
		break;
	case NFSPROC_MKDIR:
	case NFSPROC_CREATE:
		detail_diroparg();
		detail_sattr();
		break;
	case NFSPROC_WRITE:
		detail_nfsfh();
		(void) getxdr_long();	/* begoff */
		(void) showxdr_long("Offset = %d");
		(void) getxdr_long();	/* totalcount */
		(void) showxdr_long("(%d bytes(s) of data)");
		break;
	case NFSPROC_RENAME:
		detail_diroparg();
		detail_diroparg();
		break;
	case NFSPROC_LINK:
		detail_nfsfh();
		detail_diroparg();
		break;
	case NFSPROC_SYMLINK:
		detail_diroparg();
		(void) showxdr_string(NFS_MAXPATHLEN, "Path = %s");
		detail_sattr();
		break;
	case NFSPROC_READDIR:
		detail_nfsfh();
		(void) showxdr_u_long("Cookie = %lu");
		(void) showxdr_long("Count = %d");
		break;
	case NFSPROC_READ:
		detail_nfsfh();
		(void) showxdr_long("Offset = %d");
		(void) showxdr_long("Count = %d");
		break;
	default:
		break;
	}
}

/*
 *  Print out version 2 NFS reply packets
 */
static void
nfsreply2(proc)
	int proc;
{
	switch (proc) {
	    case NFSPROC_GETATTR:
	    case NFSPROC_SETATTR:
	    case NFSPROC_WRITE:
		/* attrstat */
		if (detail_nfsstat() == 0) {
			detail_fattr();
		}
		break;
	    case NFSPROC_LOOKUP:
	    case NFSPROC_CREATE:
	    case NFSPROC_MKDIR:
		/* diropres */
		if (detail_nfsstat() == 0) {
			detail_nfsfh();
			detail_fattr();
		}
		break;
	    case NFSPROC_READLINK:
		/* readlinkres */
		if (detail_nfsstat() == 0) {
			(void) showxdr_string(NFS_MAXPATHLEN, "Path = %s");
		}
		break;
	    case NFSPROC_READ:
		/* readres */
		if (detail_nfsstat() == 0) {
			detail_fattr();
			(void) showxdr_long("(%d byte(s) of data)");
		}
		break;
	    case NFSPROC_REMOVE:
	    case NFSPROC_RENAME:
	    case NFSPROC_LINK:
	    case NFSPROC_SYMLINK:
	    case NFSPROC_RMDIR:
		/* stat */
		detail_nfsstat();
		break;
	    case NFSPROC_READDIR:
		/* readdirres */
		if (detail_nfsstat() == 0)
			detail_readdirres();
		break;
	    case NFSPROC_STATFS:
		/* statfsres */
		if (detail_nfsstat() == 0) {
			(void) showxdr_long("Transfer size = %d");
			(void) showxdr_long("Block size = %d");
			(void) showxdr_long("Total blocks = %d");
			(void) showxdr_long("Free blocks = %d");
			(void) showxdr_long("Available blocks = %d");
		}
		break;
	    default:
		break;
	}
}

static void
detail_diroparg()
{
	detail_nfsfh();
	(void) showxdr_string(NFS_MAXPATHLEN, "File name = %s");
}

/*
 * V2 NFS protocol was implicitly linked with SunOS errnos.
 * Some of the errno values changed in SVr4.
 * Need to map errno value so that SVr4 snoop will interpret
 * them correctly.
 */
static char *
statusmsg(status)
	u_long status;
{
	char *errstr;

	switch (status) {
	case NFS_OK: return ("OK");
	case NFSERR_PERM: return ("Not owner");
	case NFSERR_NOENT: return ("No such file or directory");
	case NFSERR_IO: return ("I/O error");
	case NFSERR_NXIO: return ("No such device or address");
	case NFSERR_ACCES: return ("Permission denied");
	case NFSERR_EXIST: return ("File exists");
	case NFSERR_XDEV: return ("Cross-device link");
	case NFSERR_NODEV: return ("No such device");
	case NFSERR_NOTDIR: return ("Not a directory");
	case NFSERR_ISDIR: return ("Is a directory");
	case NFSERR_INVAL: return ("Invalid argument");
	case NFSERR_FBIG: return ("File too large");
	case NFSERR_NOSPC: return ("No space left on device");
	case NFSERR_ROFS: return ("Read-only file system");
	case NFSERR_OPNOTSUPP: return ("Operation not supported");
	case NFSERR_NAMETOOLONG: return ("File name too long");
	case NFSERR_NOTEMPTY: return ("Directory not empty");
	case NFSERR_DQUOT: return ("Disc quota exceeded");
	case NFSERR_STALE: return ("Stale NFS file handle");
	case NFSERR_REMOTE: return ("Object is remote");
	case NFSERR_WFLUSH: return ("write cache flushed");
	default: return ("(unknown error)");
	}
	/* NOTREACHED */
}

int
sum_nfsstat(line)
	char *line;
{
	u_long status;

	status = getxdr_long();
	(void) strcpy(line, statusmsg(status));
	return (status);
}

int
detail_nfsstat()
{
	u_long status;
	int pos;

	pos = getxdr_pos();
	status = getxdr_long();
	(void) sprintf(get_line(pos, getxdr_pos()),
		"Status = %lu (%s)",
		status, statusmsg(status));

	return ((int) status);
}

char *
sum_nfsfh()
{
	int i, l;
	int fh = 0;
	static char buff[16];

	for (i = 0; i < NFS_FHSIZE; i += 4) {
		l =  getxdr_long();
		fh ^= (l >> 16) ^ l;
	}
	(void) sprintf(buff, " FH=%04X", fh & 0xFFFF);
	return (buff);
}

void
detail_nfsfh()
{
	(void) showxdr_hex(NFS_FHSIZE / 2, "File handle = %s");
	(void) showxdr_hex(NFS_FHSIZE / 2, "              %s");
}

static void
detail_mode(mode)
	int mode;
{
	char *str;

	switch (mode & S_IFMT) {
	case S_IFDIR: str = "Directory";	break;
	case S_IFCHR: str = "Character";	break;
	case S_IFBLK: str = "Block";		break;
	case S_IFREG: str = "Regular file";	break;
	case S_IFLNK: str = "Link";		break;
	case S_IFSOCK: str = "Socket";		break;
	case S_IFIFO: str = "Fifo";		break;
	default: str = "?";			break;
	}

	(void) sprintf(get_line(0, 0), "Mode = 0%o", mode);
	(void) sprintf(get_line(0, 0), " Type = %s", str);
	(void) sprintf(get_line(0, 0),
		" Setuid = %d, Setgid = %d, Sticky = %d",
		(mode & S_ISUID) != 0,
		(mode & S_ISGID) != 0,
		(mode & S_ISVTX) != 0);
	(void) sprintf(get_line(0, 0), " Owner's permissions = %s",
		perms(mode >> 6 & 0x7));
	(void) sprintf(get_line(0, 0), " Group's permissions = %s",
		perms(mode >> 3 & 0x7));
	(void) sprintf(get_line(0, 0), " Other's permissions = %s",
		perms(mode & 0x7));
}

void
detail_fattr()
{
	int fltype, mode, nlinks, uid, gid, size, blksz;
	int blocks, fsid, fileid;

	fltype  = getxdr_long();
	mode	= getxdr_long();
	nlinks	= getxdr_long();
	uid	= getxdr_long();
	gid	= getxdr_long();
	size	= getxdr_long();
	blksz	= getxdr_long();
	(void) getxdr_long();		/* rdev */
	blocks	= getxdr_long();
	fsid	= getxdr_long();
	fileid	= getxdr_long();

	(void) sprintf(get_line(0, 0),
		"File type = %d (%s)",
		fltype, filetype(fltype));
	detail_mode(mode);
	(void) sprintf(get_line(0, 0),
		"Link count = %d, UID = %d, GID = %d",
		nlinks, uid, gid);
	(void) sprintf(get_line(0, 0),
		"File size = %d, Block size = %d, No. of blocks = %d",
		size, blksz, blocks);
	(void) sprintf(get_line(0, 0),
		"File system id = %d, File id = %d",
		fsid, fileid);
	(void) showxdr_date("Access time       = %s");
	(void) showxdr_date("Modification time = %s");
	(void) showxdr_date("Inode change time = %s");
}

static void
detail_sattr()
{
	int mode;

	mode = getxdr_long();
	detail_mode(mode);
	(void) showxdr_long("UID = %d");
	(void) showxdr_long("GID = %d");
	(void) showxdr_long("Size = %d");
	(void) showxdr_date("Access time       = %s");
	(void) showxdr_date("Modification time = %s");
}

static char *
filetype(n)
	int n;
{
	switch (n) {
	    case NFREG: return ("Regular File");
	    case NFDIR: return ("Directory");
	    case NFBLK: return ("Block special");
	    case NFCHR: return ("Character special");
	    case NFLNK: return ("Symbolic Link");
	    default:	return ("?");
	}
}

static char *
perms(n)
	int n;
{
	static char buff[4];

	buff[0] = n & 4 ? 'r' : '-';
	buff[1] = n & 2 ? 'w' : '-';
	buff[2] = n & 1 ? 'x' : '-';
	buff[3] = '\0';
	return (buff);
}

static char *
sum_readdirres()
{
	static char buff[1024];
	int entries = 0;

	if (setjmp(xdr_err)) {
		(void) sprintf(buff, " %d+ entries (incomplete)", entries);
		return (buff);
	}
	while (getxdr_long()) {
		entries++;
		(void) getxdr_long();			/* fileid */
		(void) getxdr_string(buff, 1024);	/* name */
		(void) getxdr_u_long();			/* cookie */
	}

	(void) sprintf(buff, " %d entries (%s)",
		entries,
		getxdr_long() ? "No more" : "More");
	return (buff);
}

static void
detail_readdirres()
{
	u_long fileid, cookie;
	int entries = 0;
	char *name;
	char buff[1024];

	(void) sprintf(get_line(0, 0), " File id  Cookie Name");

	if (setjmp(xdr_err)) {
		(void) sprintf(get_line(0, 0),
			"  %d+ entries. (Frame is incomplete)",
			entries);
		return;
	}
	while (getxdr_long()) {
		entries++;
		fileid = getxdr_long();
		name = (char *) getxdr_string(buff, 1024);
		cookie = getxdr_u_long();
		(void) sprintf(get_line(0, 0),
			" %7lu%8lu %s",
			fileid, cookie, name);
	}

	(void) sprintf(get_line(0, 0), "  %d entries", entries);
	(void) showxdr_long("EOF = %d");
}

void
skip_fattr()
{

	xdr_skip(17 * 4);	/* XDR sizeof nfsfattr */
}
