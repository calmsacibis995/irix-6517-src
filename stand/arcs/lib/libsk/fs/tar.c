/* [posix] tar pseudo-filesystem for SGI sash + PROM.
 */
#ident "$Revision: 1.31 $"

#include <arcs/types.h>
#include <arcs/errno.h>
#include <arcs/io.h>
#include <arcs/dirent.h>

#include <sys/param.h>
#include <saio.h>
#include <tar.h>
#include <libsc.h>
#include <libsk.h>

struct tardata {
	char _pthbuf[TARBSIZE+1];	/* single block for headers */
	int _fsize;			/* file size in bytes */
	int _fblkno;			/* file starting block no */
};
#define TARDATA		((struct tardata *)(fsb->FsPtr))
#define fsize		TARDATA->_fsize
#define fblkno		TARDATA->_fblkno
#define pthbuf		TARDATA->_pthbuf

#define blkrnd(n)	(((n)+(TARBSIZE-1))/TARBSIZE)

#define TAR_LIST	0x0001
#define TAR_OPEN	0x0002

static int tar_walk(FSBLOCK *,char *,int,int);
static int atoo(char *,int);

int tar_install(void);

static int _tar_strat(FSBLOCK *);
static int _tar_checkfs(FSBLOCK *);
static int _tar_open(FSBLOCK *);
static int _tar_close(FSBLOCK *);
static int _tar_read(FSBLOCK *);
static int _tar_getdirent(FSBLOCK *);
static int _tar_grs(FSBLOCK *);

/*
 * tar_install - register this filesystem to the standalone kernel
 * and initialize any variables
 */
int
tar_install(void)
{
    return fs_register (_tar_strat, "tar", DTFS_TAR);
}

/*
 * _tar_strat - 
 */
static int
_tar_strat(FSBLOCK *fsb)
{
    /* don't support 64 bit offset (yet?) */
    if (fsb->IO->Offset.hi)
	return fsb->IO->ErrorNumber = EINVAL;

    switch (fsb->FunctionCode) {
    case FS_CHECK:
	return _tar_checkfs(fsb);
    case FS_OPEN:
	return _tar_open(fsb);
    case FS_CLOSE:
	return _tar_close(fsb);
    case FS_READ:
	return _tar_read(fsb);
    case FS_WRITE:
	return fsb->IO->ErrorNumber = EROFS;
    case FS_GETDIRENT:
	return _tar_getdirent(fsb);
    case FS_GETREADSTATUS:
	return _tar_grs(fsb);
    default:
	return fsb->IO->ErrorNumber = ENXIO;
    }
}

static int
_tar_grs(FSBLOCK *fsb)
{
	IOBLOCK *iob = fsb->IO;

	/* If at or past EOF, read will return 0 bytes (tar never blocks) */
	if (iob->Offset.lo >= fsize)
		return(iob->ErrorNumber = EAGAIN);
	iob->Count = fsize - iob->Offset.lo;
	return(ESUCCESS);
}

static int
is_tar(FSBLOCK *fsb)
{
	pth_t *pth = (pth_t *) pthbuf;
	int i,size,cs,ecs,posix;
	char *p;

	/* Based on magic, use tar or posix tar. */
	if (strncmp(pth->pt_magic,TMAGIC,TMAGLEN)) {
		/* standard tar */
		size  = THSZ;
		posix = 0;
	}
	else {
		/* Posix tar */
		size = PTHSZ;
		posix = 1;
	}

	/* checksumming */
	ecs = atoo(pth->pt_chksum,8);
	bcopy("        ",pth->pt_chksum,8);

	for (i=cs=0, p=(char *)pth ; i < size ; i++, cs += *p++) ;

	if (cs != ecs)
		return(0);

	/* kludge to pass back the type header we have */
	pth->pt_chksum[0] = posix;

	return(1);			/* found a tar header! */
}

static int
_tar_checkfs(FSBLOCK *fsb)
{
	IOBLOCK *iob = fsb->IO;

	if (!(fsb->FsPtr=(void *)dmabuf_malloc(sizeof(struct tardata))))
		return(iob->ErrorNumber = ENOMEM);

	iob->Address = pthbuf;
	iob->Count = TARBSIZE;
	iob->StartBlock = 0;
	if (DEVREAD(fsb) != TARBSIZE) {
		return(iob->ErrorNumber = EIO);
	}
	if (!is_tar(fsb)) {
		return(iob->ErrorNumber = EINVAL);
	}
	return(ESUCCESS);
}

static int
_tar_open(FSBLOCK *fsb)
{
	int rc=0;

	if (fsb->IO->Flags & F_WRITE)
		rc = EROFS;
	else if (TARDATA == NULL)		/* this shouldnt happen */
		rc = EIO;
	else {
		/* try to open */

		fsb->Buffer = NULL;
		fsb->IO->StartBlock = 0;
		if (fsb->IO->Flags & F_DIR) {
			/* Only support pseudo-root of "/" */
			if (!strcmp(fsb->Filename,"/"))
				return(ESUCCESS);
			else
				rc = ENOTDIR;
		}
		else {
			if (tar_walk(fsb,(char *)fsb->Filename,0,TAR_OPEN)) {
				dmabuf_free(TARDATA);
				return(fsb->IO->ErrorNumber);
			}
			return(ESUCCESS);
		}
	}

	/* return error code */
	return(fsb->IO->ErrorNumber = rc);
}

static int
_tar_close(FSBLOCK *fsb)
{
	if (fsb->Buffer != NULL) _free_iobbuf(fsb->Buffer);
	dmabuf_free(TARDATA);
	return(ESUCCESS);
}

static int
_tar_read(FSBLOCK *fsb)
{
	IOBLOCK *iob = fsb->IO;
	char *buf = (char *)iob->Address;
	int cnt = iob->Count;
	int lcnt,scnt=cnt,rc=0;

	/* make sure we have a open file */
	if (!fsize) {
		rc = EBADF;
		goto rerr;
	}

	/* make sure read doesn't go past EOF */
	if ((iob->Offset.lo + scnt) > fsize) {
		cnt = scnt = fsize - iob->Offset.lo;
		if (scnt <= 0) return(iob->Count = 0);
	}

	/* check for a partial read */
	if ((scnt < TARBSIZE) || (iob->Offset.lo % TARBSIZE)) {
tail:
		lcnt = _min(TARBSIZE - (iob->Offset.lo % TARBSIZE), scnt);
		iob->StartBlock = (iob->Offset.lo / TARBSIZE) + fblkno;
		iob->Address = pthbuf;
		iob->Count = TARBSIZE;

		if (DEVREAD(fsb) != iob->Count) {
			rc = iob->ErrorNumber;
			goto rerr;
		}
		bcopy(pthbuf+(iob->Offset.lo%TARBSIZE),buf,lcnt);

		buf += lcnt;
		iob->Offset.lo += lcnt;
		scnt -= lcnt;
	}

	/* read all full blocks */
	if (scnt / TARBSIZE) {
		/* set-up read blocked read */
		lcnt = scnt & ~(TARBSIZE-1);
		iob->StartBlock = (iob->Offset.lo / TARBSIZE) + fblkno;
		iob->Address = buf;
		iob->Count = lcnt;

		/* read data */
		rc = DEVREAD(fsb);
		if (rc == -1) {
			rc = iob->ErrorNumber;
			goto rerr;
		}
		if (rc < lcnt) {
			rc = EIO;
			goto rerr;
		}

		/* update offsets */
		buf += lcnt;
		iob->Offset.lo += lcnt;
		iob->StartBlock += lcnt/TARBSIZE;
		scnt -= lcnt;
	}

	if (scnt)
		goto tail;

	iob->Count = cnt;
	return(ESUCCESS);

rerr:
	return(iob->ErrorNumber = rc);
}

/*  Ascii to octal.  Make sure number has a leading 0 to force octal, which
 * happens on old style tar, and potentially with large values.
 */
static int
atoo(char *str, int max)
{
	char tmp[16], *p;
	int rc;

	tmp[0] = '0';
	strncpy(tmp+1,str,max);
	*(tmp+1+max) = '\0';

	for (p=tmp+1; *p == ' '; *p = '0', p++) ;

	(void) atob(tmp,&rc);
	return(rc);
}

static int
_tar_getdirent(FSBLOCK *fsb)
{
	int rc;

	if (fsb->IO->Count == 0)
		return(fsb->IO->ErrorNumber = EINVAL);

	/* If Offset is 0, then reset start block to 0 to "rewinddir()".
	 */
	if (fsb->IO->Offset.lo == 0 && fsb->IO->Offset.hi == 0)
		fsb->IO->StartBlock = 0;

	rc = tar_walk(fsb,(char *)fsb->IO->Address,fsb->IO->Count,TAR_LIST);

	fsb->IO->Offset.lo++;		/* one directory entry farther */

	if (rc > 0) {
		fsb->IO->Count = rc;
		return(ESUCCESS);
	}

	/* ARCS specifies ENOTDIR when there are no more entries to be read.
	 */
	if (rc == 0)
		fsb->IO->ErrorNumber = ENOTDIR;

	return(fsb->IO->ErrorNumber);
}

static int
tar_walk(FSBLOCK *fsb,char *file,int count,int type)
{
	pth_t *pth = (pth_t *)pthbuf;
	int i,posix,size,rc=0;
	IOBLOCK *iob = fsb->IO;
	char *p;

	/* scan tar archive */
	for (;;) {
		/* read a block */
		iob->Address = (char *)pth;
		iob->Count = TARBSIZE;
		if ((size=DEVREAD(fsb)) != TARBSIZE) {
			if (!size)		/* hit EOF before null block */
				return(0);
			/* read error from driver */
			rc = iob->ErrorNumber;
			goto bad;
		}
		iob->StartBlock++;

		/* dig through the header */

		/*  Look for a null terminating block.  The Posix
		 * spec specifies two null blocks, but if the name
		 * of this header block is "", then we have an error.
		 */
		if (!pth->pt_name[0]) {
			register long *p = (long *)pth;

			for (i=0; !(*p) && (i < (PTHSZ/sizeof(long))) ;
			     i++,p++) ;

			/*  We are at the end of the archive and we
			 * didn't find our file -> open failed, or
			 * getdents done.
			 */
			if (i == (PTHSZ/sizeof(long))) {
				if (type&TAR_LIST)
					return(0);
				rc = ENOENT;
				goto bad;
			}
			else {
				/* mangled header */
				rc = EIO;
				goto bad;
			}
		}

		/* calc checksums */
		if (!is_tar(fsb)) {
			/* incorrect header checksum */
			rc = EIO;
			goto bad;
		}
		posix = pth->pt_chksum[0];

		/* save size of this file */
		size = atoo(pth->pt_size,12);

		/* print out file info */
		if (type & TAR_LIST) {
			DIRECTORYENTRY *d = (DIRECTORYENTRY *)file;
			int i=0,max;
			CHAR *p;

			d->FileAttribute = ReadOnlyFile;
			if (pth->pt_typeflag == DIRTYPE)
				d->FileAttribute |= DirectoryFile;

			/* how much space can we use? */
			max = FileNameLengthMax + ((count-1) *
				sizeof(DIRECTORYENTRY)) - 1;
			p = d->FileName;
			if (posix && pth->pt_prefix[0]) {
				pth->pt_prefix[PREFSIZE-1] = '\0';
				i = strlen(pth->pt_prefix);
				i = (i < max-1) ? i : max-1;
				strncpy(p,pth->pt_prefix,i);
				*(p += i) = '/';
				p++;
				max -= i;
			}
			pth->pt_name[NAMESIZE-1] = '\0'; /* kils pt_mode! */
			i = strlen(pth->pt_name)+1;
			i = (i < max) ? i : max;
			strncpy(p,pth->pt_name,i);
			p[i] = '\0';

			if ((d->FileNameLength=strlen(p)) > FileNameLengthMax)
				d->FileNameLength = FileNameLengthMax;
		}

		if (type & TAR_OPEN) {
			/* Check if this is our man */
			p = file;
			if (posix && pth->pt_prefix[0]) {
				int n;

				pth->pt_prefix[PREFSIZE-1] = '\0';
				n = strlen(pth->pt_prefix);

				if (!strncmp(p,pth->pt_prefix,n) &&
				    (file[n] == '/'))
					p = &file[n+1];
				else
					p = &file[1];	/* force failure */
			}
			if (!strncmp(p,pth->pt_name,NAMESIZE)) {
				/* lastly make sure it's a regular file */
				if ((pth->pt_typeflag != REGTYPE) &&
				    (pth->pt_typeflag != AREGTYPE)) {
					/* not a regular file */
					rc = EIO;
					goto bad;
				}
				if (!size) {
					/* zero byte file */
					rc = EIO;
					goto bad;
				}
				/*  Yep, we're home.  Save the block number,
				 * size and then allocate a bio buf.
				 */
				fblkno = iob->StartBlock;
				fsize = size;
				fsb->Buffer = (CHAR *)_get_iobbuf();
				return(0);
			}
		}
			
		/* "seek" to bn of next tar header */
		iob->StartBlock += blkrnd(size);

		if (type & TAR_LIST)
			return(1);
	}

bad:
	iob->ErrorNumber = rc;
	return(-1);
}
