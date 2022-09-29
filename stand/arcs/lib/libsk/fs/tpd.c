#ident	"lib/libsk/fs/tpd.c:  $Revision: 1.36 $"

/*
 * tpd.c -- tape directory routines
 */

#include <arcs/types.h>
#include <arcs/hinv.h>
#include <arcs/io.h>
#include <arcs/dirent.h>
#include <arcs/errno.h>
#include <arcs/pvector.h>

#include <tpd.h>
#include <saio.h>
#include <libsc.h>
#include <libsk.h>

struct tpd_info {
	struct tp_entry *tpd_ent;
	union tp_header *tpd_hdr;
};
#define fsb_to_entry(fsb)     (((struct tpd_info *)((fsb)->FsPtr))->tpd_ent)
#define fsb_to_dir(fsb)  (&(((struct tpd_info *)((fsb)->FsPtr))->tpd_hdr->th_td))

void swap_tp_dir (struct tp_dir *td);

#define PROOT_LBN		-1
#define PROOT_NBYTES		0
#define PSEUDO_ROOT(x)		\
	((x)->te_lbn == (unsigned)PROOT_LBN && (x)->te_nbytes == PROOT_NBYTES)

int tpd_install(void);

static int _tpd_strat(FSBLOCK *);
static int _tpd_checkfs(FSBLOCK *);
static int _tpd_open(FSBLOCK *);
static int _tpd_close(FSBLOCK *);
static int _tpd_read(FSBLOCK *);
static int _tpd_getdirent(FSBLOCK *);
static int _tpd_grs(FSBLOCK *);
static int _tpd_getfileinfo(FSBLOCK *);

int tpd_match(char *, struct tp_dir *, struct tp_entry *);
int is_tpd(struct tp_dir *);

/*
 * tpd_install - register this filesystem to the standalone kernel
 * and initialize any variables
 */
int
tpd_install(void)
{
    return fs_register (_tpd_strat, "tpd", DTFS_TPD);
}

/*
 * _tpd_strat - 
 */
static int
_tpd_strat(FSBLOCK *fsb)
{
    /* don't support 64 bit offset (yet?) */
    if (fsb->IO->Offset.hi)
	return fsb->IO->ErrorNumber = EINVAL;

    switch (fsb->FunctionCode) {
    case FS_CHECK:
	return _tpd_checkfs(fsb);
    case FS_OPEN:
	return _tpd_open(fsb);
    case FS_CLOSE:
	return _tpd_close(fsb);
    case FS_READ:
	return _tpd_read(fsb);
    case FS_WRITE:
	return fsb->IO->ErrorNumber = EROFS;
    case FS_GETDIRENT:
	return _tpd_getdirent(fsb);
    case FS_GETREADSTATUS:
	return(_tpd_grs(fsb));
    case FS_GETFILEINFO:
	return _tpd_getfileinfo(fsb);
    default:
	return fsb->IO->ErrorNumber = ENXIO;
    }
}

static void *
_get_tpdinfo(void)
{
	struct tpd_info *tpd;

	if ((tpd = (struct tpd_info *)malloc(sizeof(*tpd))) == 0)
		return 0;

	tpd->tpd_ent = (struct tp_entry *)malloc(sizeof(struct tp_entry));
	if (tpd->tpd_ent == 0) {
		free (tpd);
		return 0;
	}

	tpd->tpd_hdr = (union tp_header *)dmabuf_malloc(sizeof(union tp_header));
	if (tpd->tpd_hdr == 0) {
		free (tpd->tpd_ent);
		free (tpd);
		return 0;
	}
	return (char *)tpd;
}

static void
_free_tpdinfo(void *tpd)
{
	free(((struct tpd_info *)tpd)->tpd_ent);
	dmabuf_free(((struct tpd_info *)tpd)->tpd_hdr);
	free(tpd);
}

/*
 * _tpd_checkfs - 
 */
static int
_tpd_checkfs(FSBLOCK *fsb)
{
	IOBLOCK *iob = fsb->IO;

	if ((fsb->FsPtr = (void *) _get_tpdinfo ()) == 0)
		return iob->ErrorNumber = ENOMEM;

	/*
	 * read tape directory
	 */
	iob->Address = (void *)fsb_to_dir(fsb);
	iob->Count = sizeof(union tp_header);
	iob->StartBlock = 0;
	if (DEVREAD(fsb) != sizeof(union tp_header)) {
		_free_tpdinfo (fsb->FsPtr);
		return iob->ErrorNumber = EIO;
	}
	if (!is_tpd(fsb_to_dir(fsb))) {
		_free_tpdinfo (fsb->FsPtr);
		return iob->ErrorNumber = EINVAL;
	}

	return ESUCCESS;
}

/*
 * _tpd_open -- lookup file in boot tape directory
 */
static int
_tpd_open(FSBLOCK *fsb)
{
	IOBLOCK *iob = fsb->IO;

	if (iob->Flags & F_WRITE)
		return iob->ErrorNumber = EROFS;

	/* since _tpd_checkfs should get called first, we can
	 * assume that io->i_fsptr has a valid tp_header
	 * this test is just a sanity check
	 */
	if (!fsb->FsPtr)
		return iob->ErrorNumber = EIO;

	/* max length of filename handled in tpd_match is 64.
	 */
	if (strlen(fsb->Filename) >= (64-1))
		return iob->ErrorNumber = ENAMETOOLONG;

	/*
	 * Search the tape directory for the requested file
	 */
	fsb_to_entry(fsb)->te_nbytes = 0;
	iob->StartBlock = -1;
	if (tpd_match((char *)fsb->Filename,
	    fsb_to_dir(fsb), fsb_to_entry(fsb))) {
		/*  Make pseudo root is opened as a directory, and a
		 * normal file is not opened as a directory.
		 */
		if (PSEUDO_ROOT(fsb_to_entry(fsb))) {
			if ((iob->Flags & F_DIR) == 0)
				return(iob->ErrorNumber = EISDIR);
		}
		else if (iob->Flags & F_DIR) {
			return(iob->ErrorNumber = ENOTDIR);
		}
		/*
		 * Allocate a buffer for block io
		 */
		fsb->Buffer = (CHAR *)_get_iobbuf();
		return ESUCCESS;
	}

	return iob->ErrorNumber = EIO;
}

#define RECLEN (((sizeof(struct tp_entry) + 1) + 3) & ~3)

static int
_tpd_getdirent(FSBLOCK *fsb)
{
    IOBLOCK *iob = fsb->IO;
    int entriesleft, count = iob->Count;
    DIRECTORYENTRY *udp = (DIRECTORYENTRY *)iob->Address;
    struct tp_entry *te;
    struct tp_dir *td = (struct tp_dir *)fsb->Buffer;

    entriesleft = count;
    if (entriesleft == 0)
	    return(iob->ErrorNumber = EINVAL);

    /* read tape header on first directory entry
     */
    if (iob->Offset.lo == 0) {
	iob->Address = (char *)fsb->Buffer;
	iob->Count = sizeof(union tp_header);
	iob->StartBlock = 0;
	if (DEVREAD(fsb) != sizeof(union tp_header))
		return(iob->ErrorNumber = EIO);
    }

    while (entriesleft && (iob->Offset.lo < TP_NENTRIES)) {
	te = &td->td_entry[iob->Offset.lo];
	if (te->te_nbytes == 0) {
	    /* empty directory slot */
	    iob->Offset.lo++;
	    continue;		
	}
	udp->FileAttribute = ReadOnlyFile;
	udp->FileNameLength = TP_NAMESIZE;
	iob->Offset.lo++;
	bcopy (te->te_name, udp->FileName, TP_NAMESIZE);
	udp->FileName[TP_NAMESIZE] = '\0';
	udp++;
	entriesleft--;
    }

    /*  No more entries found, and and EOF (offset == NVDIR).  Return
     * ENOTDIR (part of ARCS 1.1 GetDirectoryEntry() definition).
     */
    if (count == entriesleft)
	return(iob->ErrorNumber = ENOTDIR);

    iob->Count = count - entriesleft;
    return(ESUCCESS);
}

/*
 * _tpdread -- read from a boot tape file
 */
static int
_tpd_read(FSBLOCK *fsb)
{
    int bn, bcnt, ocnt;
    IOBLOCK *iob = fsb->IO;
    char *buf = iob->Address;
    int cnt = iob->Count;
    int err = 0;

    /*
     * Make sure we don't read past end of file
     */
    if (iob->Offset.lo + cnt > fsb_to_entry(fsb)->te_nbytes) {
	cnt = fsb_to_entry(fsb)->te_nbytes - iob->Offset.lo;
	if (cnt < 0)
	    cnt = 0;
    }
    ocnt = cnt;

    /* read in data */
    while (cnt > 0) {

	/* If we did some small reads, or a seek, or our
	 * last read was long, but past a block boundary, or
	 * we are reading less than one block, then read into
	 * our private buffer and copy.  This is also used if
	 * there is less than a full block at the tail end of the
	 * request.
	 * This is also used if the passed in buffer isn't dma aligned.
	 */
	if(cnt<BLKSIZE || (iob->Offset.lo % BLKSIZE) || !IS_DMAALIGNED(buf)) {
		bcnt = _min(BLKSIZE - (iob->Offset.lo % BLKSIZE), cnt);
		/* get block containing current offset in buffer if necessary */
		bn = (iob->Offset.lo / BLKSIZE) + fsb_to_entry(fsb)->te_lbn;
		if (iob->StartBlock != bn || !(iob->Offset.lo%BLKSIZE)) {
			iob->StartBlock = bn;
			iob->Address = fsb->Buffer;
			iob->Count = BLKSIZE;
			if(DEVREAD(fsb) != iob->Count) {
				err = 1;
				break;
			}
		}
		bcopy(&fsb->Buffer[iob->Offset.lo % BLKSIZE], buf, bcnt);
		buf += bcnt;
		iob->Offset.lo += bcnt;
		cnt -= bcnt;
	}

	/* Read as much as we can in one request. forget about 
	 * buffering!
	 * This is the preferred method if possible.  This can only
	 * be done if the buffer is dma aligned.
	 */
	if(cnt >= BLKSIZE && IS_DMAALIGNED(buf)) {
		bn = (iob->Offset.lo / BLKSIZE) + fsb_to_entry(fsb)->te_lbn;
		iob->Address = buf;
		iob->StartBlock = bn;
		bcnt = cnt/BLKSIZE;	/* number of blocks */
		iob->Count =  bcnt * BLKSIZE;
		if(DEVREAD(fsb) != iob->Count) {
			err = 1;
			break;
		}
		cnt -= iob->Count;
		iob->Offset.lo += iob->Count;
		buf += iob->Count;
		iob->StartBlock += bcnt;
	}
    }

    if (err) {
        printf("error on tape read\n");
        if(!iob->ErrorNumber)
	    iob->ErrorNumber = EIO;
        return(iob->ErrorNumber);
    }

    iob->Count = ocnt;
    return(ESUCCESS);
}

/* GetReadStatus: If at/past EOF return EAGAIN, else return ESUCCESS
 */
static int
_tpd_grs(FSBLOCK *fsb)
{
	IOBLOCK *iob = fsb->IO;

	if (iob->Offset.lo > fsb_to_entry(fsb)->te_nbytes)
		return(iob->ErrorNumber = EAGAIN);
	iob->Count = fsb_to_entry(fsb)->te_nbytes - iob->Offset.lo;
	return(ESUCCESS);
}

/*
 * _tpdclose -- free the buffer
 */
static int
_tpd_close(FSBLOCK *fsb)
{
	_free_iobbuf(fsb->Buffer);
	_free_tpdinfo (fsb->FsPtr);
	fsb->FsPtr = 0;

	return(ESUCCESS);
}
/*
 * _tpdgetfileinfo -- Get information about a file
 */
static int
_tpd_getfileinfo(FSBLOCK *fsb)
{
	IOBLOCK *iob = fsb->IO;
	FILEINFORMATION fi;

	bzero(&fi, sizeof fi);

	fi.StartingAddress.lo = 0;
	fi.EndingAddress.lo   = fsb_to_entry(fsb)->te_nbytes;
	fi.CurrentAddress.lo  = iob->Offset.lo;
	strncpy(fi.FileName, fsb_to_entry(fsb)->te_name, 31);
	fi.FilenameLength     = strlen(fi.FileName);
	fi.Type               = fsb->Device->Type;
	fi.Attributes         = ReadOnlyFile;
	if ( iob->Flags & F_DIR )
		fi.Attributes |= DirectoryFile;

	bcopy(&fi, iob->Address, sizeof fi);

	return(ESUCCESS);
}
