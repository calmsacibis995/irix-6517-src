/* Copyright 1986, Silicon Graphics Inc., Mountain View, CA. */
#ident	"$Revision: 3.19 $"

/*
 * VMEBUS disk driver for ram disk device.
 * System V version.
 */
#include "bstring.h"
#include "sys/sbd.h"
#include "sys/types.h"
#include "sys/immu.h"
#include "sys/param.h"
#include "sys/systm.h"
#include "sys/buf.h"
#include "sys/errno.h"
#include "sys/cmn_err.h"
#include "sys/sysmacros.h"
#include "sys/debug.h"
#include "sys/vmevar.h"

#define NRD	1
#define NRDMAP	64

#ifdef notdef
struct vme_ctlr *rcinfo[NRC];
struct vme_device *rdinfo[NRD];
int *rcstd[] = { (int *)0 };
int rdprobe(), rdslave(), rdattach();

struct vme_driver rcdriver = {
	rdprobe, rdslave, rdattach, (unsigned *)rcstd, "rd", rdinfo, "rc", rcinfo
};
#endif

struct vme_driver scdriver;

/* like on 4.2, define minor to be   unit:5, slice:3 */
#define dkunit(bp) (minor((bp)->b_edev)>>5)
#define dkslice(bp) (minor((bp)->b_edev)&07)
#define dkblock(bp) ((bp)->b_blkno)

/* THIS SHOULD BE READ OFF THE PACK, PER DRIVE */
struct	size {
	daddr_t	blockoff;		/* starting block in slice */
	daddr_t	nblocks;		/* number of blocks in slice */
} rd_sizes[8] = {
	    0,	     2048,		/* A=cyl   0 thru 397 */
	 2048,	     2048,		/* B=cyl 398 thru 510 */
	    0,	     4096,		/* C=cyl   0 thru 511 */
	    0,		0,		/* D=cyl 398 thru 510 */
	    0,          0,		/* F= Not Defined     */
	    0,	        0,		/* G=cyl   0 thru 510 */
	    0,          0,		/* H= Not Defined     */
};
/* END OF STUFF WHICH SHOULD BE READ IN PER DISK */


/* buffers for raw IO */
struct	buf	rrdbuf[NRD];

#define RDISK_BASE	0xbc000000

#define	b_cylin b_resid

rdprint(dev,str)
dev_t dev;
char * str;
{
	cmn_err(CE_NOTE,"rd: dev %d, %s\n",dev, str);
}

rdintr()
{
	cmn_err(CE_PANIC,"rdintr");
}

void
rdopen(dev_t dev)
{
	register int unit = minor(dev) >> 3;

	if (unit >= NRD) {
		curthreadp->k_error = ENXIO;
		return ;
	}
	return ;
}

void
rdstrategy(register struct buf *bp)
{
	register int drive, i;
	int partition = minor(bp->b_edev) & 07, s;
	long blocknumber, sz;
	unsigned base, offset, v;
	int resid,npf, err;
	register pde_t *pdeptr;
	pde_t rd_map[NRDMAP];
	char *dbaddr, *phaddr;
	int count;

	/* check for valid drive number of the controller */
	drive = dkunit(bp);
	if (drive >= NRD)
		goto bad;

	/* calc number of 512byte blocks in transfer */
	/* make sure the request is contained in the partition */
	sz = (bp->b_bcount+511) >> 9;
	if (bp->b_blkno < 0 ||
	    (blocknumber = dkblock(bp))+sz > rd_sizes[partition].nblocks)
		goto bad;

	blocknumber += rd_sizes[partition].blockoff;
	dbaddr = (char *)RDISK_BASE + (blocknumber*512);

	/* calc number of 4k pages in transfer */
	v = btoc(bp->b_un.b_addr);

#if IP20
	phaddr = (char *)(bp->b_dmaaddr);

	if (bp->b_flags & B_READ) {
		bcopy(dbaddr, phaddr, bp->b_bcount);
	} else {
		bcopy(phaddr, dbaddr, bp->b_bcount);
	}
	resid = 0;
#endif

	iodone(bp);
	return;
bad:
	bp->b_flags |= B_ERROR;
	iodone(bp);
	return;
}


void
rdread(dev_t dev)
{
	register int unit = minor(dev) >> 3;

	if (unit >= NRD) {
		curthreadp->k_error = ENXIO;
		return;
	}
	physio((int (*)())rdstrategy, &rrdbuf[unit], dev, B_READ);
}

void
rdwrite(dev_t dev)
{
	register int unit = minor(dev) >> 3;

	if (unit >= NRD) {
		curthreadp->k_error = ENXIO;
		return;
	}
	physio((int (*)())rdstrategy, &rrdbuf[unit], dev, B_WRITE);
}

rdsize(dev_t dev)
{
	register int unit = minor(dev) >> 3;

	if (unit >= NRD)
		return (-1);
	return (rd_sizes[minor(dev) & 07].nblocks);
}
