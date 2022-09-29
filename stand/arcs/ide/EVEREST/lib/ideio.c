/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1992, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
  **************************************************************************/

#ifdef EVEREST	/* whole file */

#ifndef ARCS_SA
#define ARCS_SA  /* whole file - define here instead of in the Makefile */

#include <sys/types.h>
#include <errno.h>
#include <sys/dvh.h>
#include <sys/dkio.h>
#include <everr_hints.h>
#include "uif.h"
#include <ide_msg.h>
#include <arcs/io.h>
#include <arcs/signal.h>
#include <arcs/spb.h>
#include <arcs/tvectors.h>
#include <arcs/pvector.h>
#include <arcs/fat.h>
#include <ide_s1chip.h>

#ident	"arcs/ide/EVEREST/lib/ideio.c:  $Revision: 1.10 $"


/* globals */
uint drivernum;         /* Internal driver num */
uint ctlrtype;  /* needed to distinguish 3201 & 4201 */
uint driveno;           /* drive number */
uint partnum;   /* partition to use */
int fds[MAX_DRIVE];                             /* fd's, per drive */
struct volume_header vh;
int ioretries = 3;                              /* error retries */
int exercising = 0;
int errno;
static int wd95loc[2];

static char *cylsub(daddr_t);

clearvh()
{
	bzero(&vh, sizeof(vh));
}

int
chkfatvh(void *vhp)
{
        if(((struct master_boot_rec_type *)vhp)->signature == BOOT_SIGNATURE)
                return 1;
        return 0;
}

int
checkvh(struct volume_header *vhp)
{

        if ((vhp->vh_magic == VHMAGIC
            && vh_checksum(vhp) == 0) ||
            (vhp->vh_magic == BOOT_SIGNATURE) || chkfatvh(vhp))
                return 0;

	msg_printf(DBG,"vhp->vh_magic: 0x%x (VHMAGIC: 0x%x, BOOT_SIG: 0x%x)\n",
		 vhp->vh_magic, VHMAGIC, BOOT_SIGNATURE);
	msg_printf(DBG,"chkfatvh(vhp) returns %d\n", chkfatvh(vhp));	
	msg_printf(DBG,"vh_checksum returns %d\n", vh_checksum(vhp));
	
        /* bzero(vhp, sizeof *vhp); */
        return -1;
}

/*
 * get volume header from driver
 */
void
get_vh(struct volume_header *vp, int lfd)
{
        if( gioctl(lfd, DIOCGETVH, vp) < 0 )
                msg_printf(ERR,"No volume header available from driver");
	msg_printf(DBG,"vp->vh_magic is 0x%x\n", vp->vh_magic);
}

int
readdvh(struct volume_header *vhp, int lfd)
{
        get_vh(vhp, lfd);
        if (checkvh(vhp) < 0 ) {
		msg_printf(ERR,"Invalid label on disk\n");
		msg_printf(DBG,"Listing volume header partition info:\n");
                if (DBG <= *Reportlevel)
			ide_show_pts(vhp);
		return (1);
	}
	msg_printf(DBG,"Listing volume header partition info:\n");
        if (DBG <= *Reportlevel)
		ide_show_pts(vhp);

        return 0;
}

init_label(int fd)
{
	int lfd;

	lfd = fd;
	if (readdvh(&vh, lfd) < 0 ) {
		return (1);
	}
}

/*
 * Returns the first block of the given partition, checks block range 
 * Based on arcs/fx/pt.c, show_pts() 
 */
int
firstblock_ofpart(int partition, int force, int *fblock)
{
	register struct partition_table *p;

	p = &PT((struct volume_header *)&vh)[partition];
	if (DFLT_BLK_NUM > p->pt_nblks) {
		msg_printf(ERR,
		  "WARNING: Specified number of blocks is greater than ");
		msg_printf(ERR,"the number of blocks in the partition. ");
		msg_printf(ERR,"Specified: %d, blocks in partition %d: %d\n", 
			DFLT_BLK_NUM, partition, p->pt_nblks);
		msg_printf(ERR,"Skipping...\n");
		return (-1);
	}
	msg_printf(DBG,"partition is %d, firstblock is %d\n",
		partition, p->pt_firstlbn);
	*fblock = p->pt_firstlbn;
	return(0);
}

/* round # of blocks to to number of megabytes; since we are getting
 * close to 4 Gb disks, do in such a way that we don't overflow an
 * unsigned 32bit value.
*/
uint
mbytes(uint blocks)
{
        blocks += ((1<<19)-1) / DP(&vh)->dp_secbytes;
        return blocks / ((1<<20) /  DP(&vh)->dp_secbytes);
}


ide_show_pts(struct volume_header *vp)
{
        register int i, first=1, j;
        register struct partition_table *p;
        register maxpart;

        if (VP(&vh)->vh_magic == BOOT_SIGNATURE) {
                maxpart = MAX_FAT_PAR+1;
                j = 1;
        }
        else {
                maxpart = NPARTAB;
                j = 0;
        }

        p = &PT(vp)[j];
	msg_printf(DBG,"i is %d, j is %d, maxpart is %d\n", i,j,maxpart);
        for( i = j; i < maxpart; i++,p++ ) {
            if(first) {
                       msg_printf(DBG,"partitions\n");
                       msg_printf(DBG,"part  blocks       (base+size)\n");
                       first=0;
            }
            msg_printf(DBG," %2d: %7ld + %-7ld\n",
                           i, p->pt_firstlbn, p->pt_nblks);
        }
}

/* copied from arcs/fx/arcs.c -- it might be nice to eventually put this
 * somewhere more general
 */
open(char *file, int how)
{
        unsigned long ofd;

        if (how == 2) 
		how = OpenReadWrite;
        else if (how == 0) 
		how = OpenReadOnly;
        else if (how == 1) 
		how = OpenWriteOnly;
        /* else leave it alone, who knows? ... */
	msg_printf(DBG,"file in open call is %s\n", file);
	/* errno = Open(file,how,&ofd); */
	errno = Open(file,how,&ofd); 
	/* Open defined in libsk/lib/arcsio.c */
        if (errno)
                ofd = -1;
	msg_printf(DBG,"ofd in open returns %d, errno = %d\n", ofd, errno);
        return (int)ofd;
}

/* copied from arcs/fx/io.c -- it might be nice to eventually put this
 * somewhere more general
 */
/*
 * open the device file associated with the given
 * driveno and parition.  driver and ctlrno are globals.
 */
int
gopen(int ctlrno, uint d, uint p, char *driver, int mode)
{
       	IDESTRBUF s;
        ulong gofd;

        *s.c = 0;
#ifdef ARCS_SA
        if(!strcmp(driver, DKIP_NAME) ||
            !strcmp(driver, XYL_NAME)  ||
            !strcmp(driver, IPI_NAME))
                sprintf(s.c, "%s(%d)disk(%d)part(%d)", driver, ctlrno, d, p);
        else if (!strcmp(driver, SCSI_NAME))
                sprintf(s.c, "scsi(%d)disk(%d)part(%d)", ctlrno, d, p); 
	/*sprintf(s.c, "scsi(0)disk(1)rdisk(0)partition(10)");*/
	msg_printf(DBG,"device: %s\n", s.c);
#elif _STANDALONE        /* 'old' standalone */
        sprintf(s.c, "%s(%d,%d,%d)", driver, ctlrno, d, p);
#else
        if(!strcmp(driver, DKIP_NAME))
                sprintf(s.c, "/dev/rdsk/ips%ud%uvol", ctlrno, d);
        else if(!strcmp(driver, XYL_NAME))
                sprintf(s.c, "/dev/rdsk/xyl%ud%uvol", ctlrno, d);
        else if(!strcmp(driver, IPI_NAME))
                sprintf(s.c, "/dev/rdsk/ipi%ud%uvol", ctlrno, d);
        else if(!strcmp(driver, JAG_NAME))
                sprintf(s.c, "/dev/rdsk/jag%dd%dvol", ctlrno, d);
        else if(!strcmp(driver, SCSI_NAME)) {
                if (p == PTNUM_VOLUME)
                        sprintf(s.c, "/dev/rdsk/dks%ud%uvol", ctlrno, d);
                else
                        sprintf(s.c, "/dev/rdsk/dks%ud%us%u", ctlrno, d, p);
        }
#ifdef SMFD_NAME
        else if(!strcmp(driver, SMFD_NAME)) {
                char *type = smfd_partname(p);
                if(!type)
                        return -1;
                sprintf(s.c, "/dev/rdsk/fds%ud%u.%s", ctlrno, d, type);
        }
#endif /* SMFD_NAME */
#endif /* _STANDALONE */
        if(!s.c) {
                msg_printf(ERR, "unknown driver type \"%s\"\n", driver);
                return -1;
        }
        msg_printf(VRB,"...opening %s\n", s.c);
        fds[d] = gofd = open(s.c, mode);
        if(gofd == -1 && errno == EACCES && 
			(fds[d] = gofd = open(s.c, 0)) != -1){
                msg_printf(ERR, "Notice: read only device\n");
        }
        if((int)gofd > 0) {
                /* if (!strcmp(driver, "dkip"))
                        fxihandshake();
                else if (!strcmp(driver, "xyl") || !strcmp(driver, "ipi"))
                        fxxhandshake(); */
        	fds[d] = gofd;
		msg_printf(INFO, 
		"\nWarning: this disk probably has mounted filesystems.\n"
                "\t Don't do anything destructive, unless you are sure\n"
                "\t nothing is really mounted on this disk.\n\n");
        }
        else
          msg_printf(ERR, "Failed to open %s\n", s.c); 
	/* different than what we
                            told them we were opening, so give them a hint */
        return (int)gofd;
}

/*
 * copied from stand/fx/arcs.c
 */

/*
 * read from disk at block bn, with retries
 *
 * For non ESDI/SMD, don't retry on multiple block reads or report errors
 * or add them to the error log if we are exercising the
 * disk AND this is a multiple block read.  The exercisor
 * will retry each block separately when it gets errors
 * on multiple block reads.  Otherwise we confuse the
 * user by reporting errors as the first block of the read, with
 * some drives and firmware revs.
 * For ESDI & SMD, though, we do NOT do that, because they only spare
 * by mapping whole tracks.
 * ARCS already has some provision for large block #'s, so i/o always
 * uses same calls, unlike 'normal' version.
 */
int
gread(daddr_t bn, void *buf, uint count, int slot, int adap, int fd)
{
        int i;
        int secsz = DP(&vh)->dp_secbytes;
        ULONG bcount, syscnt;
        LARGE offset;
	int lfd;

	lfd = fd;

	wd95loc[0] = slot;
	wd95loc[1] = adap;

	msg_printf(DBG,"Reading from device... \n");

        /* secsz is manually set to 512 if zero. This is needed to check
           the integrity of partition zero which is the entire disk if
           ARCS.
        */
        if (secsz == 0)
                secsz = 512;
        syscnt = count * secsz;

        for(i = 0; i <= ioretries; i++) {
            ULONG blk = bn;

            /* have to set offset inside loop, since Seek returns
             * the new offset in the passed struct; we *hope* this
             * doesn't loop too many times; after all, it is only
             * one pass for a 4GB drive...  Could be handled better
             * with long long support, but not in bvd.  Assumes
             * sector size is even, which is not unreasonable. */
            offset.hi = 0;
            while((0x80000000u/(secsz/2)) <= blk) {
                    offset.hi++;
                    blk -= 0x80000000u/(secsz/2);
            }

            offset.lo = (ULONG)blk*secsz;

            if(i)
                msg_printf(INFO, "retry #%d\n", i+1);
            if(errno=Seek((LONG)lfd, &offset, SeekAbsolute))  {
                    /* should have some way to suppress trying to add
                     * a bad block for this one, but with the check above
                     * for out of range, this should be quite rare... */
		    err_msg(WD95_DMA_SK, wd95loc, blk); 
                    return -1;
            }
		 msg_printf(DBG,"gread loc 1\n");
            errno = Read((LONG)lfd,(CHAR *)buf,(LONG)syscnt, 
			&bcount);

	    if(!errno && syscnt == bcount) {
		msg_printf(DBG,"gread loc 2, fd:%d, lfd: %d\n", fd,lfd);
                    return 0;
	    }

            if(errno == -1) {
                if(count == 1)
			err_msg(WD95_DMA_RD, wd95loc, cylsub(bn));
                else {
                   if(exercising && drivernum != DKIP_NUMBER &&
                        drivernum != XYL_NUMBER)
                        break;  /* scanchunk retries sector by sector, and
                                 * reports error */
			err_msg(WD95_DMA_RD, wd95loc, cylsub(bn));
                }
                /* adderr(0, bn, "sr"); */
            }
            else {
		err_msg(WD95_DMA_RDCNT,wd95loc,count,bcount/secsz,cylsub(blk));
            }
        }

        /* adderr(0, bn, "hr"); */
        return -1;
} /* gread */


/*
 * write to disk at block bn, with retries; see comments at gread()
 */
int
gwrite(daddr_t bn, void *buf, uint count, int slot, int adap, int fd)
{
        int i;
        int secsz = DP(&vh)->dp_secbytes;
        ULONG bcount, syscnt;
        LARGE offset;
	int lfd;

	lfd = fd;

	wd95loc[0] = slot;
	wd95loc[1] = adap;

	msg_printf(DBG,"Writing to device ... \n");

        /* secsz is manually set to 512 if zero. This is needed to check
           the integrity of partition zero which is the entire disk if
           ARCS.
        */
        if (secsz == 0)
                secsz = 512;
        syscnt = count * secsz;

        for(i = 0; i <= ioretries; i++) {
            ULONG blk = bn;

            /* have to set offset inside loop, since Seek returns
             * the new offset in the passed struct; we *hope* this
             * doesn't loop too many times; after all, it is only
             * one pass for a 4GB drive...  Could be handled better
             * with long long support, but not in bvd.  Assumes
             * sector size is even, which is not unreasonable. */
            offset.hi = 0;
            while((0x80000000u/(secsz/2)) <= blk) {
                    offset.hi++;
                    blk -= 0x80000000u/(secsz/2);
            }

            offset.lo = (ULONG)blk*secsz;

            if(i)
                msg_printf(INFO, "retry #%d\n", i+1);
	    msg_printf(DBG,"About to call Seek()\n");
            if(errno=Seek((LONG)lfd, &offset, SeekAbsolute))  {
                    /* should have some way to suppress trying to add
                     * a bad block for this one, but with the check above
                     * for out of range, this should be quite rare... */
		    err_msg(WD95_DMA_SK, wd95loc, blk); 
                    return -1;
            }
	    msg_printf(DBG,"About to call Write()\n");
            errno = 
	     Write((LONG)lfd,(CHAR *)buf,(LONG)syscnt,&bcount);
            if(!errno && syscnt == bcount)
                    return 0;

            if(errno == -1) {
                if(errno == EROFS) {    /* no point in retrying! */
                      /* print message once here, instead of each caller */
                      msg_printf(ERR,"Drive appears to be write protected\n");
                      break;
                }
                if(count == 1)
			err_msg(WD95_DMA_WR, wd95loc, cylsub(bn));
                else {
                   if(exercising && drivernum != DKIP_NUMBER &&
                        drivernum != XYL_NUMBER)
                        break;  /* scanchunk retries sector by sector, and
                                 * reports error */
			err_msg(WD95_DMA_WR, wd95loc, cylsub(bn));
                }
            }   /* adderr(0, bn, "sr"); */
            else {
	      err_msg(WD95_DMA_WRCNT,wd95loc,count,bcount/secsz,cylsub(blk));
            }
        }

        /* adderr(0, bn, "hr"); */
        return -1;
} /* gwrite */


/*
 * ioctl on the disk, with error reporting
 */
gioctl(int lfd,  int cmd, void *ptr)
{

	/* libsc/lib/privstub.c */
        errno = ioctl((long)lfd, (long)cmd, (long)ptr);
	return (errno);
}


/*
 * close all open disks
 */
void
gclose(int fd)
{
	int lfd;

	lfd = fd;
	if (lfd > 0) {
		msg_printf(DBG,"Drive (lfd: %d) being closed ...\n", lfd);
        	Close((long)fd);
	}
	else
		 msg_printf(DBG,"skipping closing.... lfd is %d\n", lfd);
}

scsi_dname(char *s, int len, int lfd)
{
        char p[SCSI_DEVICE_NAME_SIZE];
        uint istoshiba156, sel_bits;
	int i;

        istoshiba156 = 0;
        if(gioctl(lfd, DIOCDRIVETYPE, p) < 0) {
                msg_printf(ERR, "error getting drivetype");
                *s = '\0';
		return (1);
        }
        else {  /* copy and ensure null-termination */
		msg_printf(DBG,"len: %d, size(p): %d\n", len, sizeof(p));
                if(len > sizeof(p))
                        len =  sizeof(p) + 1;
                strncpy(s, p, len);
                s[len-4] = '\0'; 
		for (i = 0; i < SCSI_DEVICE_NAME_SIZE; i++)
			msg_printf(DBG,"string[%d]: %c\n", i, s[i]);
                /* Toshiba can't handle the PF bit on mode selects; everyone
                        else can, and some drives require it.  All of them
                        allow saved params so far; WORMs, etc. may not, so
                        this may need to be modified later. */
                if(bcmp("TOSHIBA MK156FB", p, 15) == 0) {
                        istoshiba156 = 1;
                        sel_bits = 1; 
                }
#ifdef SMFD_NUMBER
                else if(drivernum == SMFD_NUMBER)
                        sel_bits = 0;   /* floppies support neither */
#endif
                else
                        sel_bits = 0x11; 
                /* EINVAL means we are really running under 3.2, so don't
                complain */
                if(
		gioctl(lfd,DIOCSELFLAGS,(void *)sel_bits)<0 &&errno !=EINVAL) {
                        msg_printf(ERR, "unable to set modeselect flags");
                        sel_bits = 0;
			return (1);
                }
        }
	return (0);
}

/* easier to do this (like in old fx) than to fix the callers, given
 * the err_msg() mess */
char *
cylsub(daddr_t bn)
{
	static char blkbuf[32];
	sprintf(blkbuf, "%u", bn);
	return blkbuf;
}

#endif /* ARCS_SA */
#endif /* EVEREST */
