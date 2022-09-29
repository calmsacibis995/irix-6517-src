#ident	"lib/libsc/fs/sdvh.c:  $Revision: 1.2 $"

/*
 * sdvh.c -- disk volume header routines for sash.  Unlike the prom
 * dvh.c, it supports writes, but it does *NOT* support extending
 * the file, only rewriting it in place.  Not in prom because we want
 * to support this on all system types, so we always have to have it
 * in sash until no old machines are left.  new inst "automatic install"
 * needs this ability.  Based on rev 1.43 of dvh.c
 */
#undef _KERNEL
#include <arcs/types.h>
#include <arcs/hinv.h>
#include <arcs/io.h>
#include <arcs/pvector.h>
#include <arcs/dirent.h>
#include <libsc.h>
#include <libsk.h>

#include <sys/types.h>
#include <sys/fs/efs.h>
#include <sys/dir.h>
#include <sys/errno.h>
#include <sys/dvh.h>
#include <sys/dkio.h>
#include <saio.h>

# define dprintf(x)	(Debug?printf x : 0)

int sdvh_install(void);

static int _sdvh_strat(FSBLOCK *);
static int _sdvh_checkfs(FSBLOCK *);
static int _sdvh_open(FSBLOCK *);
static int _sdvh_close(FSBLOCK *);
static int _sdvh_read(FSBLOCK *);
static int _sdvh_write(FSBLOCK *);
static int _sdvh_getdirent(FSBLOCK *);
static int _sdvh_grs(FSBLOCK *);
static int _sdvh_getfileinfo(FSBLOCK *);

#define cvd(x)	((struct volume_directory *)((x)->FsPtr))

#define PROOT_LBN		-1
#define PROOT_NBYTES		0

/*
 * sdvh_install - register this filesystem to the standalone kernel
 * and initialize any variables
 */
int
sdvh_install(void)
{
    return fs_register (_sdvh_strat, "sdvh", DTFS_DVH);
}


/* same as _get_iobbuf(), basicly */
CHAR *
getbuffer(void)
{
	CHAR *cp = (char *)dmabuf_malloc(BLKSIZE*2);
	if ( cp )
		return cp;
	panic("out of io buffers");
	/*NOTREACHED*/
}


/*
 * _sdvh_strat - 
 */
static int
_sdvh_strat(FSBLOCK *fsb)
{
    /* don't support 64 bit offsets (yet?) */
    if (fsb->IO->Offset.hi)
	return fsb->IO->ErrorNumber = EINVAL;

    switch (fsb->FunctionCode) {
    case FS_CHECK:
	return _sdvh_checkfs(fsb);
    case FS_OPEN:
	return _sdvh_open(fsb);
    case FS_CLOSE:
	return _sdvh_close(fsb);
    case FS_READ:
	return _sdvh_read(fsb);
    case FS_WRITE:
	return _sdvh_write(fsb);
    case FS_GETDIRENT:
	return _sdvh_getdirent(fsb);
    case FS_GETREADSTATUS:
	return(_sdvh_grs(fsb));
    case FS_GETFILEINFO:
	return _sdvh_getfileinfo(fsb);
    default:
	return fsb->IO->ErrorNumber = EINVAL;
    }
}

/* check if dvh filesystem; otherwise open searches all filesys types, and
 * then comes back to here...
*/
static int
_sdvh_checkfs(FSBLOCK *fsb)
{
	static struct volume_header vh;
	IOBLOCK *iob = fsb->IO;

	/* Get volume directory from device driver. no reason to do is_vh,
	 * because driver does it for us, and fails ioctl if not. 
	 */
	iob->FunctionCode = FC_IOCTL;
	iob->IoctlCmd = (IOCTL_CMD)(__psint_t)DIOCGETVH;
	iob->IoctlArg = (IOCTL_ARG)&vh;
	fsb->DeviceStrategy (fsb->Device, iob);
	if(iob->ErrorNumber)
		return iob->ErrorNumber;	/* no point in looking */
	return (vh.vh_pt[iob->Partition].pt_type == PTYPE_VOLHDR &&
		vh.vh_pt[iob->Partition].pt_nblks > 0 &&
		vh.vh_pt[iob->Partition].pt_firstlbn >= 0) ? ESUCCESS : EINVAL;
}

/*
 * _sdvh_open -- lookup file in volume header directory
 */
static int
_sdvh_open(FSBLOCK *fsb)
{
#define DVHMAXPATH	64
	static struct volume_header vh;
	register struct volume_directory *vd;
	char filename[DVHMAXPATH];
	IOBLOCK *iob = fsb->IO;
	char *file = (char *) fsb->Filename;

	/*
	 * Get volume directory from device driver
	 */
	iob->FunctionCode = FC_IOCTL;
	iob->IoctlCmd = (IOCTL_CMD)(__psint_t)DIOCGETVH;
	iob->IoctlArg = (IOCTL_ARG)&vh;
	if (fsb->DeviceStrategy (fsb->Device, iob))
		return iob->ErrorNumber;	/* no point in looking */

	if (strlen(file) >= (DVHMAXPATH-1))
		return iob->ErrorNumber = ENAMETOOLONG;

	/*
	 * If a file of the form "(filename)" is given,
	 * strip off the parens; just like bootp and tpd do.
	 * This is done primarily so CDROM auto-installs work,
	 * where the mr name is passed as "(mr)"
	 */
	if(*file == '(') {
		char *cp;
		cp = file = strcpy(filename, file+1);
		for( ; *cp && *cp != ')'; cp++)
			;
		*cp = '\0';
	}
	if(*file == '/' && file[1] != '\0') {
		char *cp;
		for(cp = file+1; *cp; cp++)
			cp[-1] = *cp;
		cp[-1] = '\0';
	}

	fsb->FsPtr = (void *)malloc(sizeof(struct volume_directory));
	if (!fsb->FsPtr) {
		printf ("dvh: couldn't allocate space for volume\n");
		return ENOMEM;
	}

	/*  Check for OpenDirectory with pseudo-root filename "/".
	 */
	if (iob->Flags&F_DIR) {
		if (strcmp(file,"/")) {
			return(iob->ErrorNumber = ENOTDIR);
		}
		cvd(fsb)->vd_name[0] = '/';
		cvd(fsb)->vd_name[1] = '\0';
		cvd(fsb)->vd_lbn = PROOT_LBN;
		cvd(fsb)->vd_nbytes = PROOT_NBYTES;
		fsb->Buffer = getbuffer();
		return ESUCCESS;
	}

	/*
	 * Search the volume directory for the requested file
	 */
	cvd(fsb)->vd_nbytes = 0;
	iob->StartBlock = -1;
	/*
	 * checking lbn != -1 is crock for dvhtool
	 */
	for (vd = vh.vh_vd; vd < &vh.vh_vd[NVDIR]; vd++) {
		if (strncmp(file, vd->vd_name, VDNAMESIZE) == 0
		    && vd->vd_lbn != -1) {
			*cvd(fsb) = *vd;
			fsb->Buffer = getbuffer();
			return ESUCCESS;
		}
	}

	free (fsb->FsPtr);

	return iob->ErrorNumber = ENOENT;
}

/* 
 * _sdvh_getdirent -
 *
 * Return errno, saving entries read in iob->Count.
 */
static int
_sdvh_getdirent(FSBLOCK *fsb)
{
    IOBLOCK *iob = fsb->IO;
    int entriesleft = iob->Count;
    DIRECTORYENTRY *dp = (DIRECTORYENTRY *)iob->Address;
    struct volume_directory *vd;
    struct volume_header *vh = (struct volume_header *)fsb->Buffer;

    if (entriesleft == 0)
	return(iob->ErrorNumber = EINVAL);

    /* read volume header on first directory entry
     */
    if (iob->Offset.lo == 0) {
	iob->FunctionCode = FC_IOCTL;
	iob->IoctlCmd = (IOCTL_CMD)(__psint_t)DIOCGETVH;
	iob->IoctlArg = (IOCTL_ARG)fsb->Buffer;
	if (fsb->DeviceStrategy (fsb->Device, iob))
	    return(iob->ErrorNumber);
    }
    iob->Count = entriesleft;		/* restore damaged count */

    while (entriesleft && (iob->Offset.lo < NVDIR)) {
	vd = &vh->vh_vd[iob->Offset.lo];
	if (vd->vd_nbytes <= 0 || vd->vd_lbn == -1) {
	    /* empty directory slot */
	    iob->Offset.lo += 1;
	    continue;		
	}
	dp->FileAttribute = ReadOnlyFile;
	dp->FileNameLength = VDNAMESIZE;
	iob->Offset.lo += 1;
	bcopy (vd->vd_name, dp->FileName, VDNAMESIZE);
	dp->FileName[VDNAMESIZE] = '\0';
	dp++;
	entriesleft -= 1;
    }

    /*  No more entries found, and and EOF (offset == NVDIR).  Return
     * ENOTDIR (part of ARCS 1.1 GetDirectoryEntry() definition).
     */
    if (iob->Count == entriesleft)
	return(iob->ErrorNumber = ENOTDIR);

    iob->Count -= entriesleft;
    return(ESUCCESS);
}

/*
 * _dvhread -- read from a volume header file
 */
static int
_sdvh_read(FSBLOCK *fsb)
{
	int bn, ocnt, i;
	char * dmaaddr;
	IOBLOCK *iob = fsb->IO;
	char *buf = iob->Address;
	int cnt = iob->Count;
	int x = 0;

	dprintf(("dvhread: ctlr %d, buf %x, count %d\n",
		fsb->IO->Controller, buf, cnt));

	/*
	 * Make sure we don't read past end of file
	 */
	if (iob->Offset.lo + cnt > cvd(fsb)->vd_nbytes) {
		cnt = cvd(fsb)->vd_nbytes - iob->Offset.lo;
		if (cnt < 0)
			cnt = 0;
	}
	ocnt = cnt;
	dprintf(("dvhread: Offset.lo 0x%x StartBlock 0x%x vd_bytes 0x%x cnt 0x%x\n",
		 iob->Offset.lo, iob->StartBlock, cvd(fsb)->vd_nbytes, cnt));
	while (cnt) {
		if(cnt > BBSIZE && !((__scunsigned_t)buf&3) && !(iob->Offset.lo&BBMASK)) {
			dmaaddr = buf;
			i = BBTOB(BTOBBT(cnt));
		}
		else {
			dmaaddr = (char *) fsb->Buffer;
			i = BBSIZE;
		}

		/*
		 * get block containing current offset in buffer if necessary
		 */
		bn = (iob->Offset.lo / BBSIZE) + cvd(fsb)->vd_lbn;
again:
		/* It's not sufficient to just check the StartBlock to
		 * decide if the transfer is currently in the buffer.  When
		 * loading "elf" standalones, higher level code first reads
		 * a block from the volume header, then does a large read of
		 * the entire file with the same StartBlock but a large count.
		 * Since we don't save the count, only bypass the read if
		 * it's for a single block.
		 */
		if ((iob->StartBlock != bn) || (i != BBSIZE)) {
			iob->StartBlock = bn;
			iob->Address = dmaaddr;
			iob->Count = i;
			dprintf(("dvhread: DEVREAD bn 0x%x dmaaddr 0x%x, count 0x%x\n",
				bn, dmaaddr, i));
			if (DEVREAD (fsb) != i) {
			    /* if it fails, and we haven't yet
			     * changed the blksz, try changing the device
			     * blksz, and then retry.  Don't restore if fails,
			     * since we know we are definitly a dvh fs
			     * at this point  */
			    if(!x) {
				x = BBSIZE;
				fsb->IO->FunctionCode = FC_IOCTL;
				fsb->IO->IoctlCmd = (IOCTL_CMD)
						(__psint_t)DIOCSETBLKSZ;
				fsb->IO->IoctlArg = (IOCTL_ARG)&x;
				if(!fsb->DeviceStrategy(fsb->Device, fsb->IO))
					goto again;
			    }

			    printf("error on vh read\n");
			    return(iob->ErrorNumber = EIO);
			}
		} else dprintf(("dvhread: NO DEVREAD bn 0x%x iob_count 0x%x i 0x%x\n", bn, iob->Count, i));
		if(dmaaddr != buf) {	/* copy from 'block buffer' */
			int off = iob->Offset.lo % BBSIZE;
			i = _min(BBSIZE - off, cnt);
			bcopy(dmaaddr+off, buf, i);
		}
		else	/* force next read from disk; in case */
			iob->StartBlock = -1; /* next read is small, and same ex_bn */
		iob->Offset.lo += i;
		buf += i;
		cnt -= i;
	}
	iob->Count = ocnt;
	return(ESUCCESS);
}

/*
 * _dvhwrite -- write to a volume header file (but does not extend,
 * only does rewrites).  No coherency with read buffering; forces next
 * readk, if any to go to disk.  Requires block boundary alignment.
 */
static int
_sdvh_write(FSBLOCK *fsb)
{
	int bn, ocnt, i;
	char * dmaaddr;
	IOBLOCK *iob = fsb->IO;
	char *buf = iob->Address;
	int cnt = iob->Count;
	int x = 0;

	dprintf(("dvhwrite: ctlr %d, buf %x, count %d\n",
		fsb->IO->Controller, buf, cnt));

	/*
	 * Make sure we don't write past current end of file;
	 * rewrite only.
	 */
	if (iob->Offset.lo + cnt > cvd(fsb)->vd_nbytes) {
		cnt = cvd(fsb)->vd_nbytes - iob->Offset.lo;
		if (cnt <= 0) {
			printf("vhwrite can't be used to extend files, only to rewrite\n");
			goto err;
		}
	}

	if(!cnt || (iob->Offset.lo % BBSIZE) || (cnt % BBSIZE)) {
		printf("vhwrite at offset 0x%x for 0x%x bytes illegal alignment\n",
			iob->Offset.lo, iob->Count);
err:
		return(iob->ErrorNumber = EINVAL);
	}

	ocnt = cnt;
	while (cnt) {
		dmaaddr = buf;
		i = BBTOB(BTOBBT(cnt));

		iob->StartBlock = (iob->Offset.lo / BBSIZE) + cvd(fsb)->vd_lbn;
again:
		iob->Address = dmaaddr;
		iob->Count = i;
		dprintf(("dvhwrite: DEVWRITE bn 0x%x dmaaddr 0x%x, count 0x%x\n",
			bn, dmaaddr, i));

		if (DEVWRITE (fsb) != i) {
			/* if it fails, and we haven't yet
			 * changed the blksz, try changing the device
			 * blksz, and then retry.  Don't restore if fails,
			 * since we know we are definitly a dvh fs
			 * at this point  */
			if(!x) {
			x = BBSIZE;
			fsb->IO->FunctionCode = FC_IOCTL;
			fsb->IO->IoctlCmd = (IOCTL_CMD)
					(__psint_t)DIOCSETBLKSZ;
			fsb->IO->IoctlArg = (IOCTL_ARG)&x;
			if(!fsb->DeviceStrategy(fsb->Device, fsb->IO))
				goto again;
			}

			iob->StartBlock = -1;	/* force future reads to come from disk;
				we don't do any buffering coherency */

			printf("error on vh write\n");
			return(iob->ErrorNumber = EIO);
		}
		iob->Offset.lo += i;
		buf += i;
		cnt -= i;
	}
	iob->StartBlock = -1;	/* force future reads to come from disk;
		we don't do any buffering coherency */
	iob->Count = ocnt;
	return(ESUCCESS);
}


/* GetReadStatus: If at or past EOF return EAGAIN, else return ESUCCESS.
 */
static int
_sdvh_grs(FSBLOCK *fsb)
{
	IOBLOCK *iob = fsb->IO;

	if (iob->Offset.lo >= cvd(fsb)->vd_nbytes)
		return(iob->ErrorNumber = EAGAIN);
	iob->Count = cvd(fsb)->vd_nbytes - iob->Offset.lo;
	return(ESUCCESS);
}

/*
 * _dvhclose -- free the buffer
 */
static int
_sdvh_close(FSBLOCK *fsb)
{
	dmabuf_free(fsb->Buffer);
	free (fsb->FsPtr);
	return ESUCCESS;
}

static int
_sdvh_getfileinfo(FSBLOCK *fsb)
{
	FILEINFORMATION fi;

	bzero(&fi, sizeof fi);

	strncpy(fi.FileName, cvd(fsb)->vd_name, 31);
	fi.EndingAddress.lo   = cvd(fsb)->vd_nbytes;
	fi.CurrentAddress.lo  = fsb->IO->Offset.lo;
	fi.Attributes         = ReadOnlyFile;
	fi.Type               = fsb->Device->Type;

	if ( fsb->IO->Flags & F_DIR )
		fi.Attributes |= DirectoryFile;

	bcopy(&fi, fsb->IO->Address, sizeof fi);

	return ESUCCESS;
}
