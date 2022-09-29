#ifdef SABLE
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
#ident	"$Header: /proj/irix6.5.7m/isms/stand/arcs/lib/libsk/io/RCS/sabledsk.c,v 1.22 1997/07/04 08:44:27 sprasad Exp $"

/*
 * VMEBUS disk driver for sable virtual disk device.
 * System V version.
 */
#include "sys/types.h"
#include "sys/sbd.h"
#include "sys/immu.h"
#include "sys/param.h"
#include "sys/buf.h"
#include "sys/dir.h"
#include "sys/pcb.h"
#include "sys/signal.h"
#include "sys/conf.h"
#include "sys/errno.h"
#include "sys/cmn_err.h"
#include "sys/sysmacros.h"
#include "sys/debug.h"

#ifdef _STANDALONE
#include <sys/open.h>
#include <sys/graph.h>
#include <sys/hwgraph.h>
#include <arcs/types.h>
#include <arcs/hinv.h>
#include <arcs/io.h>
#include <libsc.h>
#include <libsk.h>
#include "saio.h"
#define lbolt 0

#ifdef SN0
extern graph_hdl_t prom_graph_hdl ;
#include <sys/SN/klconfig.h>
#endif /* SN0 */

#endif /* _STANDALONE */

#define DEBUG 1

#ifdef IP30
#define _SABLE_DISK_PART 1
#include "sys/cpu.h"
#endif

#ifndef _STANDALONE
#include "sys/dkio.h"
#include "sys/ddi.h"		/* Must be the last include file */
#endif

#include <sys/dvh.h>
#include <sys/dkio.h>

/* The following used to be in the file:  sys/sabledsk.h
 */

#define NSD	2
#define SD_NMAP		64
#define SD_PGOFFSET	0xfff
#define SD_PGSHIFT	12

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

#ifdef SN0
#include "sys/SN/agent.h"
#define SDISK_BASE	(HSPEC_BASE + 0x20010000ull)
#define _USE_HWGRAPH	1
#elif IP30
#define SDISK_XBASE	PHYS_TO_K1(0x19000000)
#define SDISK_BASE	PHYS_TO_K1(0x19001000)
#else
#define SDISK_BASE	0x1b000000 + K1BASE
#endif

/*
 *	Defining SABLE_ASYNCDISK makes the driver not finish I/O on
 *	blocks until sdservice is called from idle. This is
 *	done to allow reasonable simulation of a real disk with
 *	latency for testing the vm code.
 */

/*****#define SABLE_ASYNCDISK 1*****/


#define sdunit(dev) (minor(dev) >> 4)
#define sdslice(dev) (minor(dev) & 0x0f)
#define dkunit(bp) (sdunit((bp)->b_dev))  /* SP - was b_edev */
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
} sd_sizes[32]
#ifndef _SABLE_DISK_PART
= {
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
	    0,    0x20000,		/* partition 1: 64MB */
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
};
#else
;
#endif
/* END OF STUFF WHICH SHOULD BE READ IN PER DISK */


/* buffers for raw IO */
struct	buf	rsdbuf[NSD];
static struct sddevice *sdp = 0;
volatile int	sd_iodone_flag;
volatile int	sd_iodone_status;

static struct volume_header dvh[NSD];

#define	b_cylin b_resid

int logoffset, logblock, logcount;

/* Begin ARCS STUFF */

static int sdskopen(IOBLOCK *io);
static int sdskclose(IOBLOCK *io);
static int sdskstrategy(IOBLOCK *io, int func);
static int sdsk_strat(COMPONENT *self, IOBLOCK *iob);
static int sdskioctl( IOBLOCK * );
static int sdskgrs(IOBLOCK *iob);
static void sdintr(void);
static int sdopen(dev_t);
static int sd_rw(int drive,char *dmaaddr,int block_num,int byte_len,int cmd);
void sdstrategy(buf_t *);

#if _USE_HWGRAPH
#include <promgraph.h>
void sdsk_install(vertex_hdl_t, vertex_hdl_t); 
void sdsk_install_later(vertex_hdl_t, vertex_hdl_t) ;
int num_sbldsk ;
#endif

/*ARGSUSED*/
static int
sdsk_strat(COMPONENT *self, IOBLOCK *iob)
{
	switch (iob->FunctionCode) {

	case	FC_INITIALIZE:
#ifdef SN0
	printf("Sable Disk: Initing controller %d ...\n", iob->Controller) ;
#endif
#ifdef IP30
	{
		/* wire interrupt to something random to avoid widget errors */
		widget_cfg_t *w = (widget_cfg_t *)SDISK_XBASE;

		w->w_intdest_upper_addr = INT_DEST_UPPER(5);
		w->w_intdest_lower_addr = INT_DEST_LOWER;
	}
#endif
#ifdef _SABLE_DISK_PART
	{
		int i;

		/* only read in for sable disk 1 now */
		if (sd_rw(1,(caddr_t)&dvh[1],0,BBSIZE,1) == 0) {
			printf("sd: cannot read volume header\n");
			return 0;
		}

		if (dvh[1].vh_magic != VHMAGIC) {
			printf("sd: invalid magic 0x%x\n",dvh[1].vh_magic);
			return(0);
		}

		for (i = 0; i < 16; i++) {
			sd_sizes[16+i].blockoff = dvh[1].vh_pt[i].pt_firstlbn;
			sd_sizes[16+i].nblocks = dvh[1].vh_pt[i].pt_nblks;
		}
	}
#endif
		return(0);

	case	FC_OPEN:
		return (sdskopen (iob));

	case	FC_READ:
                return (sdskstrategy (iob, READ));                

	case	FC_WRITE:
                return (sdskstrategy (iob, WRITE));

	case	FC_CLOSE:
		return (sdskclose (iob));

	case	FC_IOCTL:
		return (sdskioctl(iob));

        case FC_GETREADSTATUS:
		return(sdskgrs(iob));

	default:
		return(iob->ErrorNumber = EINVAL);
	}
}

static dev_t
sd_mkdev(IOBLOCK *io)
{
        return ((dev_t)(io->Partition | 
		 	((io->Unit&0xf)<<4) | ((io->Controller)<<18)));
}


static int
sdskopen(IOBLOCK *io)
{
	u.u_error = 0;
	sdopen(sd_mkdev(io)) ;
	if((io->ErrorNumber = u.u_error) == EBUSY)/* stand thinks ebusy means */
		io->ErrorNumber = ENODEV;	/* tape is rewinding, etc. */
	return(io->ErrorNumber);
}

static int
sdskclose(IOBLOCK *io)
{
	u.u_error = 0;
	/* sdclose(sd_mkdev(io), io->Flags, OTYP_LYR); */
	io->ErrorNumber = u.u_error;
	return(io->ErrorNumber);
}

static int
sdskstrategy(IOBLOCK *io, int func)
{
	/* never more than one request at a time in standalone, so
		having a static buf is no problem */
	static buf_t dkbuf;
	int ret;

	/* printf("sdskstrategy: func 0x%x address 0x%x count 0x%x blk 0x%x\n",
		func, io->Address, io->Count, io->StartBlock); */
#if 0 /* SN0 */
	io->Partition = 1 ;
#endif
	dkbuf.b_dev = sd_mkdev(io);
	dkbuf.b_flags = func==READ ? B_READ|B_BUSY : B_WRITE|B_BUSY;
	dkbuf.b_bcount = (unsigned)io->Count;
	dkbuf.b_dmaaddr = (char *)io->Address;
	dkbuf.b_blkno = io->StartBlock;
	dkbuf.b_error = 0;
	u.u_error = 0;
	sdstrategy(&dkbuf);
	if(io->ErrorNumber = u.u_error)
		ret = io->ErrorNumber;
	else {
		io->Count -= dkbuf.b_resid;
		ret = ESUCCESS;
	}
/* 	printf("sdskstrategy: return code 0x%x\n", ret); */
	return ret;
}

static int
sdskioctl( IOBLOCK *io )
{
	if (((__psunsigned_t)io->IoctlCmd & 0xffffffff) == DIOCGETVH) {
		struct volume_header *uvh= (struct volume_header *)io->IoctlArg;
		struct volume_header *vh = &dvh[sdunit(sd_mkdev(io))];

		io->ErrorNumber = 0;

		bcopy(vh,uvh,sizeof(*vh));
	}
	else
		io->ErrorNumber = EINVAL;
	return(io->ErrorNumber);
}

static int
sdskgrs(IOBLOCK *io)
{
	/* dev_t dev = sd_mkdev(io);
	register struct dksoftc	*dk ; */

/* = dev_to_softc(scsi_ctlr(dev), scsi_unit(dev), scsi_lu(dev));

	io->Count = (dk->dk_vh.vh_pt[dk_part(dev)].pt_nblks - io->StartBlock) *
		    dk->dk_blksz; */
	io->Count = 1 ;
	return(io->Count ? ESUCCESS : (io->ErrorNumber = EAGAIN));
}

#if _USE_HWGRAPH		/* XXX turn this on for Racer! */
#include <pgdrv.h>
#ifndef SN_PDI
void
sdsk_install(vertex_hdl_t hw_vhdl, vertex_hdl_t devctlr_vhdl)
{
	kl_register_install(hw_vhdl, devctlr_vhdl, sdsk_install_later) ;
}

void
sdsk_install_later(vertex_hdl_t hw_vhdl, vertex_hdl_t devctlr_vhdl)
{
        klscdev_t       *scdev_ptr ;
        COMPONENT       *arcs_compt ;
	char 		*ctmp ;
	prom_dev_info_t	*dev_info ;
	graph_error_t	graph_err ;

	create_graph_path(devctlr_vhdl, "sbldsk/target", 1) ;

	graph_err = graph_info_add_LBL(prom_graph_hdl, devctlr_vhdl, 
             				INFO_LBL_LINK, NULL,
					(arbitrary_info_t)"sbldsk/target") ;

	if (graph_err != GRAPH_SUCCESS) {
		printf("kl_ql_inst: addLBL error %d\n", graph_err) ;
		return ;
	}

        graph_err = graph_info_get_LBL(prom_graph_hdl, devctlr_vhdl,
                                       INFO_LBL_DEV_INFO,NULL,
                                       (arbitrary_info_t *)&dev_info) ;
        if (graph_err != GRAPH_SUCCESS) {
          	printf("sbldsk install: cannot get dev_info\n");
          	return ;
        }

	((klinfo_t *)dev_info->kl_comp)->virtid = num_sbldsk++ ;

        kl_reg_drv_strat(dev_info, sdsk_strat) ;

        printf("Sable Disk: install...\n") ;

	/* At this point we have found a disk. Need to init its klcfg */

	scdev_ptr = (klscdev_t *)init_device_graph(devctlr_vhdl, 
						   KLSTRUCT_DISK) ;
        if (!scdev_ptr)
                panic("sdsk_install_later: Cannot allocate any more memory\n") ;

	/* disk target id = 1 */

	scdev_ptr->scdev_info.physid = 1 ;
	
	/* Fill up ARCS info and put it in klinfo_t of the device */

        arcs_compt = (COMPONENT *)malloc(sizeof(COMPONENT)) ;
        if (arcs_compt == NULL) {
                printf("sdsk inst : malloc failed \n") ;
                return ;
        }

        arcs_compt->Class = PeripheralClass ;
        arcs_compt->Version = SGI_ARCS_VERS ;
        arcs_compt->Revision = SGI_ARCS_REV ;
        arcs_compt->AffinityMask = 0x01 ;
        arcs_compt->Flags = Input|Output ;
        arcs_compt->Type = DiskPeripheral ;
        arcs_compt->ConfigurationDataSize = 0 ;

	arcs_compt->IdentifierLength = 11 ;
	arcs_compt->Identifier = "Sable Disk" ;

	ctmp = malloc(arcs_compt->IdentifierLength + 1) ;
	strncpy(ctmp, arcs_compt->Identifier, arcs_compt->IdentifierLength) ;
	arcs_compt->Identifier = ctmp ;

        scdev_ptr->scdev_info.arcs_compt = arcs_compt ;

	link_device_to_graph(hw_vhdl, devctlr_vhdl,
				(klinfo_t *)scdev_ptr, sdsk_strat) ;
}
#endif /* !SN_PDI */
#else /* _USE_HWGRAPH */
/* XXX for RACER, use slotdriver ? */
static COMPONENT sd_adapter = {
	AdapterClass,
	SCSIAdapter,
	0,
	SGI_ARCS_VERS,
	SGI_ARCS_REV,
	0,			/* Key */
	0x01,
	0,
	0,
	0
};
static COMPONENT sd_controller = {
	ControllerClass,
	DiskController,
	0,
	SGI_ARCS_VERS,
	SGI_ARCS_REV,
	1,			/* Key */
	0x01,
	0,
	5,
	"sdsk"
};
static COMPONENT sd_peripheral = {
	PeripheralClass,
	DiskPeripheral,
	Input|Output,
	SGI_ARCS_VERS,
	SGI_ARCS_REV,
	0,			/* Key */
	0x01,
	0,
	0,
	0
};
int
sdsk_install(COMPONENT *c)
{
	/* Add scsi adapter (fake), disk controller, disk peripheral.
	 * This allows hinv, dksc aliases to work
	 */
	c = AddChild(c,&sd_adapter,(void *)NULL);
	if (c == (COMPONENT *)NULL) cpu_acpanic("sdsk1");
	c = AddChild(c,&sd_controller,(void *)NULL);
	if (c == (COMPONENT *)NULL) cpu_acpanic("sdsk2");
	c = AddChild(c,&sd_peripheral,(void *)NULL);
	if (c == (COMPONENT *)NULL) cpu_acpanic("sdsk3");
	RegisterDriverStrategy(c,sdsk_strat);
	return 0;
}
#endif

/* End of ARCS stuff */

#if 0
static void
sdprint(dev_t dev,char *str)
{
	cmn_err(CE_NOTE,"sd: dev %d, %s\n",dev, str);
	cmn_err(CE_CONT,"last offset: %d, blk: %d, cnt: %d\n",logoffset,
		logblock, logcount);
}
#endif

static void
sdintr(void)
{
        sdp = (struct sddevice *)(SDISK_BASE);
	sd_iodone_flag = 1;
	sd_iodone_status = sdp->sd_status;
}

static char *sd_transfer_buf;

static int
sdopen(dev_t dev)
{
	register int unit = sdunit(dev);

	/* printf ("sd%d: open\n", unit);  JRH */
	if (unit >= NSD)
		return (ENXIO);

	return (0);
}

#ifdef SABLE_ASYNCDISK
struct buf sdtab;
extern int idleflag;
int sdserv = 0;
#endif

static int
sd_rw(int drive,char *dmaaddr,int block_num,int byte_len,int cmd)
{
	int byte_offset;
	int i, npf;
	int cur_byte_len;
	int backup;

	byte_offset = io_poff(dmaaddr);
	logoffset = byte_offset;
	logblock = block_num;
	logcount = byte_len;

	if ((byte_len & BBMASK) != 0) {
		return(0);
	}

	if (drive != 1) {
		return(0);
	}

        sdp = (struct sddevice *)(SDISK_BASE);

	if ((byte_offset & BBMASK) != 0) {
		/* unaligned transfers are done block-at-a-time */
		if (sd_transfer_buf == NULL) {
			sd_transfer_buf = (char *)align_malloc(BBSIZE,BBSIZE);
			if (!sd_transfer_buf) {
				printf("sd: cannot allocate buffer.\n");
				return 0;
			}
		}

		while (byte_len > 0) {
			if (! cmd) {
				bcopy(dmaaddr, sd_transfer_buf, BBSIZE);
			}
			clear_cache(sd_transfer_buf,byte_len);

			sd_iodone_flag = 0;
			sdp->sd_map[0] = KDM_TO_PHYS(sd_transfer_buf)  & ~SD_PGOFFSET;

			/* fill in the disk controller registers */
			sdp->sd_blocknumber = block_num;
			sdp->sd_byteoffset = io_poff(sd_transfer_buf);
			sdp->sd_bytecount = BBSIZE;
			sdp->sd_command = (cmd ? SD_READ : SD_WRITE);

			us_delay(1);

			while (sd_iodone_flag == 0)
				sdintr();

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
			sd_iodone_flag = 0;

			clear_cache(dmaaddr, cur_byte_len);

			for(i = 0; i < npf; i++) {
				sdp->sd_map[i] = KDM_TO_PHYS(dmaaddr) & ~SD_PGOFFSET;
				dmaaddr += IO_NBPP;
			}

			/* fill in the disk controller registers */
			sdp->sd_blocknumber = block_num;
			sdp->sd_byteoffset = byte_offset;
			sdp->sd_bytecount = cur_byte_len;
			sdp->sd_command = (cmd ? SD_READ : SD_WRITE);

			/*
			 * Previously the IO was instantaneous.
			 * Now, wait for the interrupt to be processed.
			 */
			us_delay(1);
			while (sd_iodone_flag == 0)
				sdintr();

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

void
sdstrategy(bp)
	register struct buf *bp;
{
	int drive;
	long blocknumber, sz;

        sdp = (struct sddevice *)(SDISK_BASE);

	/* check for valid drive number of the controller */
	drive = dkunit(bp);

	if (drive >= NSD) {
#ifdef DEBUG
		printf("sd: invalid disk %d\n",drive);
#endif
		goto bad;
	}

	/* calc number of 512byte blocks in transfer */
	/* make sure the request is contained in the partition */
	sz = (bp->b_bcount+511) >> 9;
	if (bp->b_blkno < 0 ||
	    (blocknumber = dkblock(bp))+sz > sd_sizes[minor(bp->b_dev)].nblocks)
	{
#ifdef DEBUG
		printf("sd: invalid blkno %d\n",bp->b_blkno);
#endif
		goto bad;
	}


	if (sd_rw(drive, bp->b_dmaaddr,
		    (blocknumber + sd_sizes[minor(bp->b_dev)].blockoff),
		    bp->b_bcount, (bp->b_flags & B_READ)))
		return;

	/*FALLSTHROUGH*/
bad:
	bp->b_flags |= B_ERROR;
	bp->b_error = ENXIO;
	cmn_err(CE_CONT,"sable_dsk I/O error\n");
	iodone(bp);
	return;
}

#ifdef SABLE_ASYNCDISK
void
sdservice(void)
{
	struct buf *p, *q;
	for(p = sdtab.av_forw; p ; p = q) {
		q = p->av_forw; /* have to get it before iodone clobbers */
		/* bp_mapout(p); */
		iodone(p);
	}
	sdserv = 0;
	sdtab.av_forw = 0;
}
#endif

#if 0
/*ARGSUSED*/
static void
sdread(dev_t dev, struct uio *uio)
{
	register int unit = sdunit(dev);

	if (unit >= NSD) {
		/* curthreadp->k_error = ENXIO; */
		return ;
	}
	/* physio((int (*)())sdstrategy, &rsdbuf[unit], dev, B_READ); */
}

/*ARGSUSED*/
static void
sdwrite(dev_t dev, struct uio *uio)
{
	register int unit = sdunit(dev);

	if (unit >= NSD) {
		/* curthreadp->k_error = ENXIO; */
		return ; 
	}
	/* physio((int (*)())sdstrategy, &rsdbuf[unit], dev, B_WRITE); */
}

static int
sdsize(dev_t dev)
{
	register int unit = sdunit(dev);

	if (unit >= NSD)
		return (-1);
	return ((int)sd_sizes[minor(dev)].nblocks);
}
#endif /* 0 */
#endif
