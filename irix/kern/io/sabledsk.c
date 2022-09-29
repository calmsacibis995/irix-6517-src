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
#ident	"$Header: /proj/irix6.5.7m/isms/irix/kern/io/RCS/sabledsk.c,v 1.37 1997/03/24 18:06:24 udayh Exp $"

/*
 * disk driver for sable virtual disk device.
 * System V version.
 */
#include <sys/types.h>
#include <sys/sbd.h>
#include <sys/immu.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/dir.h>
#include <sys/pcb.h>
#include <sys/signal.h>
#include <sys/conf.h>
#include <sys/errno.h>
#include <sys/cmn_err.h>
#include <sys/sysmacros.h>
#include <sys/debug.h>
#include <sys/kmem.h>
#include <sys/vmevar.h>
#include <sys/cred.h>
#include <sys/atomic_ops.h>
#ifndef _STANDALONE
#include <sys/sema.h>
#include <sys/edt.h>
#include <sys/dkio.h>
#include <sys/ddi.h>		/* Must be the last include file */
#endif

#ifdef IP30
#include <sys/cpu.h>
#endif

#if _SYSTEM_SIMULATION || IP30
#define _SABLE_DISK_PART 1
#endif

#ifdef IPMHSIM
#include <sys/IPMHSIM.h>
#endif

#define	SABLEDSK_CACHE_WAR

#ifdef SABLEDSK_CACHE_WAR
#define SABLEDSK_REASON	CACH_FORCE | CACH_ALL_CACHES
/* ICACHE makes it flush other guys' caches. */
#else
#define SABLEDSK_REASON	CACH_IO_COHERENCY
#endif

#ifdef _SABLE_DISK_PART
#include <sys/kmem.h>
#include <sys/dvh.h>
#include <sys/kopt.h>
#endif /* _SABLE_DISK_PART */

#ifdef SN0
#include <sys/SN/agent.h>
#include <sys/pda.h>		/* For master_procid */
#endif	/* SN0 */

#if IP30 
#define	USE_XTALK_INFRASTRUCTURE	1
#define	SABLEDISK_WIDGET_PART_NUM	0xFEDC
#define	SABLEDISK_WIDGET_MFGR		0x001
#endif

#if USE_XTALK_INFRASTRUCTURE
#include <sys/iograph.h>
#include <sys/hwgraph.h>
#include <sys/xtalk/xtalk.h>
#endif

extern int showconfig;

/* The following used to be in the file:  sys/sabledsk.h
 */

#define NSD		2
#define SD_NMAP		64

struct sddevice {
	volatile __uint32_t	sd_status;
	volatile __uint32_t	sd_command;
	volatile __uint32_t	sd_blocknumber;
	volatile __uint32_t	sd_byteoffset;
	volatile __uint32_t	sd_bytecount;
	volatile __uint32_t	sd_statistics;
	volatile __uint32_t	sd_map[SD_NMAP];
};

#define	SD_ERR		1

#define	SD_READ		1		/* read data */
#define	SD_WRITE	2		/* write data */
#define	SD_IE		3		/* interrupt enable */

/*
 *	Defining SABLE_ASYNCDISK makes the driver not finish I/O on
 *	blocks until sdservice is called from idle. This is
 *	done to allow reasonable simulation of a real disk with
 *	latency for testing the vm code.
 */

/*****#define SABLE_ASYNCDISK 1*****/


#define sdunit(dev) (minor(dev) >> 4)
#define sdslice(dev) (minor(dev) & 0x0f)
#define dkunit(bp) (sdunit((bp)->b_edev))
#define dkslice(bp) (minor((bp)->b_edev)&0x0f)
#define dkblock(bp) ((bp)->b_blkno)

/* THIS SHOULD BE READ OFF THE PACK, PER DRIVE */
/* We index this table by minor device number.
 * We currently support a single partition, emulating a miniroot
 * on a scsi disk (unit 1).
 */
struct	size {
	daddr_t	blockoff;		/* starting block in slice */
	daddr_t	nblocks;		/* number of blocks in slice */
};

#if _SYSTEM_SIMULATION
#define SD0S0_SIZE 0x22000
#define SD0S1_SIZE 0x1E000
#elif IP30
#define SD0S0_SIZE 0x02000		/* really volume header size */
#define SD0S1_SIZE 0x20000
#define is_enabled(X)	1		/* do not check partitoning */
#else
#define SD0S0_SIZE 0
#define SD0S1_SIZE 0x20000
#endif

struct size sd_sizes[32] = {
	/* unit 0 (16 partitions) */
	    0,	        0,
	    0, 		0,
	    0,	        0,
	    0,		0,
	    0,          0,
	    0,	        0,
	    0,          0,
	    0,		0,
	    0,          0,
	    0,	        0,
	    0,          0,
	    0,		0,
	    0,          0,
	    0,	        0,
	    0,          0,
	    0,          0,
	/* unit 1 (16 partitions) */
	    0,	        0,
#ifndef _SABLE_DISK_PART
	    0,	  SD0S1_SIZE,				/* part 1 */
#else
	    0,		0,
#endif
	    0,          0,
	    0,	        0,
	    0,          0,
	    0,		0,
	    0,          0,
	    0,	        0,
#ifndef _SABLE_DISK_PART
	    0,	        0,
#else
	    0,    	SD0S0_SIZE + SD0S1_SIZE,	/* part 8 */
#endif
	    0,		0,
	    0,          0,
	    0,	        0,
	    0,          0,
	    0,          0,
};
/* END OF STUFF WHICH SHOULD BE READ IN PER DISK */

#ifdef _SABLE_DISK_PART
struct volume_header *vol_hdr[NSD];
#endif /* _SABLE_DISK_PART */


/* buffers for raw IO */
struct	buf	rsdbuf[NSD];
static struct	sddevice *sdp = 0;
volatile int	sd_iodone_flag;
volatile int	sd_iodone_status;

#define	b_cylin b_resid

int logoffset, logblock, logcount;
static mutex_t sdlock;

#if defined(SN0) && 0
int sddevflag = 0;
#else
int sddevflag = D_MP;
#endif

static char *sd_transfer_buf;

static int _sd_rw(int drive,char *dmaaddr,int block_num,int byte_len,int cmd);
int sd_rw(int,char *,int,int,int);
int sdtotalsize(dev_t dev);
int sdsize(dev_t dev);

void
sdprint(dev_t dev, char *str)
{
	cmn_err(CE_NOTE,"sd: dev %d, %s\n",dev, str);
	cmn_err(CE_CONT,"last offset: %d, blk: %d, cnt: %d\n",logoffset,
		logblock, logcount);
}

/* ARGSUSED */
void
sdintr(intr_arg_t arg)
{
	ASSERT(sdp != NULL);
	sd_iodone_status = sdp->sd_status;
	sd_iodone_flag = 1;
}

/*ARGSUSED*/
int
sdopen(dev_t *devp, int flag, int otyp, cred_t *crp)
{
	register int unit = sdunit(*devp);

	mutex_lock(&sdlock,PRIBIO);

	if (sd_transfer_buf == NULL) {
		sd_transfer_buf = (char *)
			kmem_alloc(BBSIZE,VM_CACHEALIGN|VM_DIRECT);
	}

#ifdef _SABLE_DISK_PART
	if (vol_hdr[unit] == NULL && is_enabled(arg_sablediskpart)) {
		struct volume_header *new_vol_hdr;
		int	i;
		struct size *spt;
	
#ifdef _SYSTEM_SIMULATION
		int	dsize;

		dsize = sdtotalsize(*devp);
		if (dsize <= 0)
			goto open_error;
#endif
		/* read the volume header */
		new_vol_hdr = (struct volume_header *) 
			kmem_alloc(BBSIZE,
				   VM_CACHEALIGN|VM_DIRECT);
		cache_operation((void *)new_vol_hdr,
				BBSIZE,
				CACH_DCACHE|CACH_INVAL|SABLEDSK_REASON);
		if (!_sd_rw(unit, (char *) new_vol_hdr, 0, BBSIZE, B_READ))
			goto open_error;
		if (vol_hdr[unit] != NULL) {
			kern_free((caddr_t) new_vol_hdr);
		}
		else
#ifdef IP30	/* compat with pre-partitions */
		if (new_vol_hdr->vh_magic != VHMAGIC &&
		    !sd_sizes[16+1].nblocks) {
			printf("sd: no vh -- using compat part\n");
			sd_sizes[16+1].nblocks = SD0S1_SIZE;
			sd_sizes[16+8].nblocks = 0;
			vol_hdr[unit] = new_vol_hdr;
		}
		else
#endif
		{
#if 0
			printf("sd: updating spt from vol_hdr\n");
#endif
			vol_hdr[unit] = new_vol_hdr;
			spt = &sd_sizes[unit * 16];
			for (i = 0; i < 16; i++) {
				spt[i].blockoff =
					vol_hdr[unit]->vh_pt[i].pt_firstlbn;
				spt[i].nblocks =
					vol_hdr[unit]->vh_pt[i].pt_nblks;
				if (showconfig &&
				    spt[i].nblocks > 0)
					printf("sd%ds%d: %d blocks at %d\n",
					       unit,
					       i,
					       spt[i].nblocks,
					       spt[i].blockoff);
			}
		}
	}
#endif /* _SABLE_DISK_PART */	
	if (sdsize(*devp) <= 0) { /* range checks unit number */
#ifdef _SABLE_DISK_PART
open_error:
#endif
		mutex_unlock(&sdlock);

		if (showconfig)
			printf("sd%ds%d: open failed\n", unit, *devp & 0xf); 

		return (ENXIO);
	}
	if (showconfig) {
		printf("sd%ds%d: open\n", unit, *devp & 0xf); 
		/* printf("  Base address == 0x%x\n", sdp); */
	}

	mutex_unlock(&sdlock);

	return(0);
}

#ifdef SABLE_ASYNCDISK
struct buf sdtab;
extern int idleflag;
int sdserv = 0;
#endif

int
sdstrategy(struct buf *bp)
{
	register int drive;
	int blocknumber, sz;

	ASSERT(sdp != NULL);

	/* check for valid drive number of the controller */
	drive = dkunit(bp);
	if (drive >= NSD)
		goto bad;

	/* calc number of 512byte blocks in transfer */
	/* make sure the request is contained in the partition */
	sz = BTOBB(bp->b_bcount);
	if (bp->b_blkno < 0 ||
	    (blocknumber = dkblock(bp))+sz > sd_sizes[minor(bp->b_edev)].nblocks)
		goto bad;

	bp_mapin(bp);   /* just in case it's PAGEIO */

	if (! sd_rw(drive,
		    bp->b_dmaaddr,
		    (blocknumber + sd_sizes[minor(bp->b_edev)].blockoff),
		    bp->b_bcount,
		    (bp->b_flags & B_READ)))
		goto bad;

#ifdef SABLE_ASYNCDISK
	/*
	 * Link buf onto list of outstanding blocks 
	 * iodone will be called by sdservice from idle()
	 * (Note that they are currently linked in backwards order)
	 */
	bp->av_forw = sdtab.av_forw;
	sdtab.av_forw = bp;
	sdserv = 1;
#else
	bp_mapout(bp);
	iodone(bp);
#endif /* SABLE_ASYNCDISK */
	return 0;
bad:
	bp->b_flags |= B_ERROR;
	bp->b_error = ENXIO;
	cmn_err(CE_CONT,"sable_dsk I/O error\n");
	iodone(bp);
	return 0;
}

#ifdef SABLE_ASYNCDISK
sdservice(void)
{
	struct buf *p, *q;
	for(p = sdtab.av_forw; p ; p = q) {
		q = p->av_forw; /* have to get it before iodone clobbers */
		bp_mapout(p);
		iodone(p);
	}
	sdserv = 0;
	sdtab.av_forw = 0;
}
#endif

/*ARGSUSED*/
int
sdread(dev_t dev, uio_t *uiop, cred_t *crp)
{
	register int unit = sdunit(dev);
	daddr_t nblks;

	if (unit >= NSD)
		return ENXIO;
	nblks = sd_sizes[minor(dev)].nblocks;
	return physiock(sdstrategy, &rsdbuf[unit], dev, B_READ,
		nblks, uiop);
}

/*ARGSUSED*/
int
sdwrite(dev_t dev, uio_t *uiop, cred_t *crp)
{
	register int unit = sdunit(dev);
	daddr_t nblks;

	if (unit >= NSD)
		return ENXIO;
	nblks = sd_sizes[minor(dev)].nblocks;
	return physiock(sdstrategy, &rsdbuf[unit], dev, B_WRITE,
		nblks, uiop);
}


#ifdef _SABLE_DISK_PART
int
sdtotalsize(dev_t dev)
{
	register int unit = sdunit(dev);
	
	if (unit >= NSD)
		return (-1);

#if _SYSTEM_SIMULATION
	if (! is_enabled(arg_sableio)) {
		int size;

		/*
		 * Fill in the disk controller registers
		 */
		*(int *)(MHSIM_DISK_PROBE_UNIT) = unit;
		*(int *)(MHSIM_DISK_OPERATION) = MHSIM_DISK_PROBE;
		size = *(int *)(MHSIM_DISK_SIZE);
		if (size > 0) {
			sd_sizes[(unit * 16) + 1].nblocks = BBTOB(size);
			sd_sizes[(unit * 16) + 10].nblocks = BBTOB(size);
		}
		return ((size <= 0) ? -1 : size);
	} else
#endif /* _SYSTEM_SIMULATION */
	return(sd_sizes[(unit * 16) + 10].nblocks);
}
#endif /* _SABLE_DISK_PART */

int
sdsize(dev_t dev)
{
	register int unit = sdunit(dev);

	if (unit >= NSD)
		return (-1);
	return (sd_sizes[minor(dev)].nblocks);
}


int
sd_rw(int drive,char *dmaaddr,int block_num,int byte_len,int cmd)
{
	int rc;

	mutex_lock(&sdlock,PRIBIO);
	rc = _sd_rw(drive,dmaaddr,block_num,byte_len,cmd);
	mutex_unlock(&sdlock);

	return rc;
}

static int
_sd_rw(int drive,char *dmaaddr,int block_num,int byte_len,int cmd)
{
	int byte_offset;
	int i, npf;
	int cur_byte_len;
	int backup;
	int s;

	byte_offset = io_poff(dmaaddr);
	logoffset = byte_offset;
	logblock = block_num;
	logcount = byte_len;

	if ((byte_len & BBMASK) != 0) {
		return(0);
	}

#if _SYSTEM_SIMULATION 
	if (! is_enabled(arg_sableio)) {
		int err, bytes;

		if (byte_len > MHSIM_DISK_MAX_TRANSFER_SIZE) {
			return(0);
		}

		if (! cmd) {
			/* Write */
			bcopy(dmaaddr, (char *) MHSIM_DISK_DATA, byte_len);
#ifdef SABLEDSK_CACHE_WAR
                        cache_operation((void *) dmaaddr, byte_len,
                                CACH_DCACHE|CACH_WBACK|CACH_FORCE);
		} else {
			/* Read */
                        cache_operation((void *) dmaaddr, byte_len,
                                CACH_DCACHE|CACH_INVAL|CACH_FORCE);
#endif /* SABLEDSK_CACHE_WAR */
		}


		/* fill in the disk controller registers */
		*(int *)MHSIM_DISK_DISKNUM = drive;
		*(int *)MHSIM_DISK_SECTORNUM = block_num;
		*(int *)MHSIM_DISK_SECTORCOUNT = BTOBB(byte_len);
		if (cmd) {
			*(int *)MHSIM_DISK_OPERATION = MHSIM_DISK_READ;
		} else {
			*(int *)MHSIM_DISK_OPERATION = MHSIM_DISK_WRITE;
		}
	
		err = *(int *)MHSIM_DISK_STATUS;
		bytes = *(int *)MHSIM_DISK_BYTES_TRANSFERRED;
		if (err || bytes == 0) {
			return(0);
		}

		if (cmd)
			bcopy((char *) MHSIM_DISK_DATA, dmaaddr, bytes);

		return(1);
	}
#endif /* _SYSTEM_SIMULATION */
	if (drive != 1) {
		return(0);
	}

	ASSERT(sdp != NULL);

	if (
#if defined(SN0)
	    (maxcpus > 2) ||
#endif
	    ((byte_offset & BBMASK) != 0)) {

		/* unaligned transfers are done block-at-a-time */
		while (byte_len > 0) {
			if (! cmd) {
				bcopy(dmaaddr, sd_transfer_buf, BBSIZE);
				cache_operation((void *) sd_transfer_buf,
						BBSIZE,
						CACH_DCACHE |
						CACH_WBACK |
						SABLEDSK_REASON);
			} else {
				cache_operation((void *) sd_transfer_buf,
						BBSIZE,
						CACH_DCACHE |
						CACH_INVAL |
						SABLEDSK_REASON);
			}

			s = spl6();
			sd_iodone_flag = 0;
#if NBPP == IO_NBPP
			sdp->sd_map[0] = ctob(kvatopfn(sd_transfer_buf));
#else
			sdp->sd_map[0] = ctob(kvatopfn(sd_transfer_buf)) + 
				(poff(sd_transfer_buf) ^
				 io_poff(sd_transfer_buf));
#endif

			/* fill in the disk controller registers */
			sdp->sd_blocknumber = block_num;
			sdp->sd_byteoffset = io_poff(sd_transfer_buf);
			sdp->sd_bytecount = BBSIZE;
			sdp->sd_command = (cmd ? SD_READ : SD_WRITE);
			splx(s);

			while (sd_iodone_flag == 0)
				;

			if (sd_iodone_status == SD_ERR) {
				return(0);
			}

			if (cmd)
				bcopy(sd_transfer_buf, dmaaddr, BBSIZE);
			
			dmaaddr += BBSIZE;
			block_num++;
			byte_len -= BBSIZE;
		}
	} else {
		while (byte_len > 0) {
			npf = io_btoc(byte_len + byte_offset);
			cur_byte_len = byte_len;
			if (npf > SD_NMAP) {
				cur_byte_len = io_ctob(SD_NMAP - 1);
				npf = io_btoc(cur_byte_len + byte_offset);
				backup = npf != (SD_NMAP - 1);
			}
			else
				backup = 0;
			s = spl6();
			sd_iodone_flag = 0;

#ifdef SABLEDSK_CACHE_WAR
#if 0
			if (! cmd) {
				/* Write */
				cache_operation((void *) dmaaddr,
						cur_byte_len,
						CACH_DCACHE|CACH_WBACK|CACH_FORCE);
			} else {
				/* Read */
				cache_operation((void *) dmaaddr,
						cur_byte_len,
						CACH_DCACHE|CACH_INVAL|CACH_FORCE);
			}
#else
			if (! cmd) {
				/* Write */
				cache_operation((void *) dmaaddr,
						cur_byte_len,
						CACH_ALL_CACHES|CACH_WBACK|CACH_FORCE);
			} else {
				/* Read */
				cache_operation((void *) dmaaddr,
						cur_byte_len,
						CACH_ALL_CACHES|CACH_INVAL|CACH_FORCE);
			}
#endif
#endif /* SABLEDSK_CACHE_WAR */

			for(i = 0; i < npf; i++){
				__psunsigned_t p;
#if NBPP == IO_NBPP
				p = ctob(kvatopfn(dmaaddr));
				sdp->sd_map[i] = p;
				ASSERT_ALWAYS(p == (p & 0xffffffffUL));
#else
				p = ctob(kvatopfn(dmaaddr)) +
					(poff(dmaaddr) ^ io_poff(dmaaddr));
				sdp->sd_map[i] = p;
				ASSERT_ALWAYS(p == (p & 0xffffffffUL));
#endif
				dmaaddr += IO_NBPP;
			}

			/* fill in the disk controller registers */
			sdp->sd_blocknumber = block_num;
			sdp->sd_byteoffset = byte_offset;
			sdp->sd_bytecount = cur_byte_len;
			sdp->sd_command = (cmd ? SD_READ : SD_WRITE);
			splx(s);

			/*
			 * Previously the IO was instantaneous.
			 * Now, wait for the interrupt to be processed.
			 */
			while (sd_iodone_flag == 0)
				;

			if (sd_iodone_status == SD_ERR) {
				return(0);
			}

			if (backup)
				dmaaddr -= IO_NBPP;

			block_num += BTOBB(cur_byte_len);
			byte_len -= cur_byte_len;
		}
	}

	return(1);
}

/* Hey, we could do a PCI-based version, too ... */

#ifdef	USE_XTALK_INFRASTRUCTURE

void
sdsetwidint(xtalk_intr_t intr)
{
	vertex_hdl_t		vhdl = xtalk_intr_dev_get(intr);
	xwidgetnum_t		targ = xtalk_intr_target_get(intr);
	iopaddr_t		addr = xtalk_intr_addr_get(intr);
	xtalk_intr_vector_t	vect = xtalk_intr_vector_get(intr);

	widget_cfg_t   	       *w = (widget_cfg_t *)
		xtalk_piotrans_addr(vhdl, NULL, 
				    0, sizeof (widget_cfg_t), 0);

	w->w_intdest_upper_addr = ((0xFF000000 & (vect << 24)) |
				   (0x000F0000 & (targ << 16)) |
				   XTALK_ADDR_TO_UPPER(addr));
	w->w_intdest_lower_addr = XTALK_ADDR_TO_LOWER(addr);
}

int
sdattach(vertex_hdl_t vhdl)
{
	xtalk_intr_t	intr;
	widget_cfg_t   *w;
	device_desc_t	dev_desc;

	w = (widget_cfg_t *)
		xtalk_piotrans_addr(vhdl, NULL, 
				    0, sizeof (widget_cfg_t), 0);

	sdp = (struct sddevice *)
		xtalk_piotrans_addr(vhdl, NULL, 
				    0x1000, sizeof (struct sddevice), 0);

	/* force "swlevel 2" until the ADMIN interface
	 * sets it up for us.
	 */
	dev_desc = device_desc_dup(vhdl);
	device_desc_intr_swlevel_set(dev_desc, 2);

	intr = xtalk_intr_alloc(vhdl, dev_desc, vhdl);
	device_desc_free(dev_desc);

	xtalk_intr_connect(intr, 
			   (intr_func_t)sdintr, (intr_arg_t)0,
			   (xtalk_intr_setfunc_t)sdsetwidint, (void *)w,
			   (void *)0);

	printf("sdattach complete\n");
	return 0;
}

void
sdinit(void)
{
	mutex_init(&sdlock, MUTEX_DEFAULT, "sd");
	xwidget_driver_register(SABLEDISK_WIDGET_PART_NUM,
				SABLEDISK_WIDGET_MFGR,
				"sd",
				0);
}

#else	/* USE_XTALK_INFRASTRUCTURE */

#if _SYSTEM_SIMULATION
#define SDISK_BASE	(0x18800000+K1BASE)
#elif	IP30
#define SDISK_XBASE     PHYS_TO_K1(0x19000000)
#define SDISK_BASE      PHYS_TO_K1(0x19001000)
#elif SN0
#define SDISK_BASE      (NODE_HSPEC_BASE(0) + 0x20010000ull)
#else	/* IPHSIM */
#define SDISK_BASE	(0x1b000000+K1BASE)
#endif /* _SYSTEM_SIMULATION */

/*ARGSUSED*/
void
sdedtinit(struct edt *edt)
{
	mutex_init(&sdlock, MUTEX_DEFAULT, "sd");
	sdp = (struct sddevice *)(SDISK_BASE);
}

#endif	/* USE_XTALK_INFRASTRUCTURE */
