/**************************************************************************
 *									  *
 * 		 Copyright (C) 1986, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident "io/dksc.c $Revision: 3.129 $"

/*
 * dksc.c -- Scsi disk device driver
 * This driver talks to the scsi interface driver scsi.c.
 */

/* Standard name prefix for this device */
#define	STD_NAME_DEFINED	"dks"

#include <sys/param.h>
#include <sys/immu.h>
#include <sys/sysmacros.h>
#include <sys/sbd.h>


#include <sys/elog.h>
#include <sys/iobuf.h>
#include <sys/scsi.h>
#include <sys/scsimsgs.h>	/* correct values of SC_NUMADDSENSE */
#include <sys/dvh.h>


#include <sys/dkio.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <sys/cmn_err.h>
#include <sys/stdnames.h>
#include <sys/invent.h>
#include <sys/buf.h>
#include <sys/dksc.h>

#include <arcs/types.h>
#include <arcs/hinv.h>
#include <arcs/io.h>
#include <libsc.h>
#include <libsk.h>
#include "saio.h"

#define dprintf(x)	(Debug ? printf x : 0)
/* Scsi disk commands */
static u_char dk_inquiry[]	= {0x12, 0, 0, 0, 1, 0};	/* len in [4] before use */
#if 0
static u_char dk_seek[]	= {0x2b, 0, 0, 0, 0, 0, 0, 0, 0, 0};
#endif
static u_char dk_read[]	= {0x28, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static u_char dk_write[]	= {0x2a, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static u_char dk_readcapacity[]= {0x25, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static u_char dk_rdefects[]	= {0x37, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static u_char dk_add_defects[]	= {0x7, 0, 0, 0, 0, 0};
static u_char dk_req_sense[]	= {0x3,  0, 0, 0, 0x16, 0};
static u_char dk_tst_unit_rdy[]= {0,    0, 0, 0, 0,    0};
static u_char dk_send_diag[]	= {0x1d, 4, 0, 0, 0,    0};
static u_char dk_mode_sense[]	= {0x1a, 0, 0, 0, 0,	0};
static u_char dk_mode_select[]	= {0x15, 0, 0, 0, 0,	0};	/* 2nd byte can
	be set by DIOCSETFLAGS */
static u_char dk_format[]	= {0x4,  0, 0, 0, 0,	0};
static u_char dk_startunit[]	= {0x1B,  0, 0, 0, 1,	0};

extern int cachewrback;

/* The basic timeout for the disk driver seconds.  Note that it
is long to handle many units active at same time, with possible
disconnects during commands */
#define DKSC_TIMEOUT	(60 * HZ)
#undef dcache_wb
#undef dcache_inval
#undef dcache_wbinval
#define dcache_wb(a,b)
#define dcache_inval(a,b)
#define dcache_wbinval(a,b)

/* Maximum number of retrys on a failed request (request sense 
 * counts as one each time also). 
 */
#define DK_MAX_RETRY	4

struct dksoftc	*dksoftc [SC_MAXADAP][SC_MAXTARG][DK_MAXLU];
sema_t		dkopensem[SC_MAXADAP][SC_MAXTARG][DK_MAXLU];
u_char dkneedstart[SC_MAXADAP][SC_MAXTARG];
int dk_oktoclose(struct dksoftc *);

static void dk_intr(scsisubchan_t *);
static void dk_retry(struct dksoftc *dk, buf_t *bp);
static void dk_resense(scsisubchan_t *);
static void dk_checkintr(scsisubchan_t *);
static u_char dkstartunit(struct dksoftc *, int);
static int dk_readcap(struct dksoftc *dk);
static void dk_pmsg(dev_t, char *, ...);
static int dk_rw(struct dksoftc *dk, char *addr, int block_num, int block_len,
	u_char *cmd_buffer);
static int dk_cmd(struct dksoftc *dk, u_char *cmd_buffer, int cmd_size,
	u_int timeoutval, u_char *addr, size_t len, int rw);
static void dk_getblksz(struct dksoftc *dk);
static int dk_setblksz(register struct dksoftc *dk, int blksz);

extern int spl5(void);	/* for splockspl */
#if DEBUG
void dkpr(unsigned adap, unsigned unit);
#if defined(SN0)
void dump_buf(char *, char *, int) ;
#endif
#endif

void svpsema(lock_t, void *, int);
void freesplock(lock_t *);

/* ARCS - new stuff */

int dkscopen(IOBLOCK *io);
int dkscclose(IOBLOCK *io);
int dkscstrategy(IOBLOCK *io, int func);
int dksc_strat(COMPONENT *self, IOBLOCK *iob);
int dkscioctl( IOBLOCK * );
static int dkscgrs(IOBLOCK *iob);

int dksc_install(void)	{return(1);}	/* needed for configuration */

typedef struct {
	LONG	numcallbacks;
} SCSIADAPTERDETAILS;

typedef struct {
	void		*(*alloc)();
	int		(*_free)();
	COMPONENT	*(*regchild)();
	int		(*regdrvstrat)(COMPONENT *, int (*)(COMPONENT *,
							    IOBLOCK*));
} SCSIDRIVERHELPER;

/*VARARGS2*/
static void
timeout(void (*f)(void *,void *), void *sp, int ticks, void *bp)
{
        delay(ticks);
        f(sp,bp);
}

/*ARGSUSED*/
int
dksc_strat(COMPONENT *self, IOBLOCK *iob)
{
	switch (iob->FunctionCode) {

	case	FC_INITIALIZE:
		return(0);

	case	FC_OPEN:
		return (dkscopen (iob));

	case	FC_READ:
                return (dkscstrategy (iob, READ));                

	case	FC_WRITE:
                return (dkscstrategy (iob, WRITE));

	case	FC_CLOSE:
		return (dkscclose (iob));

	case	FC_IOCTL:
		return (dkscioctl(iob));

        case FC_GETREADSTATUS:
		return(dkscgrs(iob));

	default:
		return(iob->ErrorNumber = EINVAL);
	}
}

/* ARCS new stuff */

extern int scsimajor[];
#define scsi_ctlr(dev)       	((dev >> 9) & 0xff) 
#define scsi_unit(dev)          ((dev >> 5) & 0xf)


/* this generates a 'dev_t with the correct values, so as little
	code as possible changes. Assumes LUN always 0.  This is so
	the standard dk_part and scsi_* macros can be used.
	Major number has to match the scsi_ctlr() macro, and the faked
	values used by it (in lboot.c).
*/
dev_t
dk_mkdev(IOBLOCK *io)
{
	return io->Partition | ((io->Unit&0xf)<<5) | ((io->Controller)<<9);
}

int
dkscopen(IOBLOCK *io)
{
	u.u_error = 0;
	_dkscopen(dk_mkdev(io), io->Flags, OTYP_LYR);
	if((io->ErrorNumber = u.u_error) == EBUSY)/* stand thinks ebusy means */
		io->ErrorNumber = ENODEV;	/* tape is rewinding, etc. */
	return(io->ErrorNumber);
}

int
dkscclose(IOBLOCK *io)
{
	u.u_error = 0;
	_dkscclose(dk_mkdev(io), io->Flags, OTYP_LYR);
	io->ErrorNumber = u.u_error;
	return(io->ErrorNumber);
}

static int dot_cnt = 0;

int
dkscstrategy(IOBLOCK *io, int func)
{
	/* never more than one request at a time in standalone, so
		having a static buf is no problem */
	static buf_t dkbuf;
	int ret;
	void _dkscstrategy(buf_t *);

	dprintf(("dkscstrategy: func 0x%x address 0x%x count 0x%x blk 0x%x\n",
		func, io->Address, io->Count, io->StartBlock));

	dkbuf.b_dev = dk_mkdev(io);
	dkbuf.b_flags = func==READ ? B_READ|B_BUSY : B_WRITE|B_BUSY;
	dkbuf.b_bcount = io->Count;
	dkbuf.b_dmaaddr = (char *)io->Address;
	dkbuf.b_blkno = io->StartBlock;
	dkbuf.b_error = 0;
	u.u_error = 0;

	_dkscstrategy(&dkbuf);
	if(io->ErrorNumber = u.u_error)
		ret = io->ErrorNumber;
	else {
		io->Count -= dkbuf.b_resid;
		ret = ESUCCESS;
	}

	return ret;
}

int
dkscioctl( IOBLOCK *io )
{
	u.u_error = 0;
	_dkscioctl(dk_mkdev(io), (__psunsigned_t)io->IoctlCmd & 0xffffffff,
		   (caddr_t)io->IoctlArg, 0);
	io->ErrorNumber = u.u_error;
	return(io->ErrorNumber);
}

static int
dkscgrs(IOBLOCK *io)
{
	dev_t dev = dk_mkdev(io);
	register struct dksoftc	*dk = dev_to_softc(scsi_ctlr(dev), 
				scsi_unit(dev), scsi_lu(dev));

	io->Count = (dk->dk_vh.vh_pt[dk_part(dev)].pt_nblks - io->StartBlock) *
		    dk->dk_blksz;
	return(io->Count ? ESUCCESS : (io->ErrorNumber = EAGAIN));
}



/* all the standalone entry points get redefined */
#define dkscopen _dkscopen
#define dkscclose _dkscclose
#define dkscstrategy _dkscstrategy
#define dkscioctl _dkscioctl


/* set drive to do synchronouse i/o if it supports it */
void
dksyncmode(register struct dksoftc *dk, int enable)
{
	scsisubchan_t *sp = dk->dk_subchan;
	if(!sp)
		return;	/* on failed opens */
	/* Patch: Deny request_sync_nego for cdroms */
	if ((dk->dk_inqtyp[0]&0x3f) != 0x5) {
		drive_acquire(dk);
#ifdef IP22_WD95
		if (sp->s_adap == 0) /* must be wd93 */
			scsi_setsyncmode(sp, enable);
		else
			wd95_scsi_setsyncmode(sp, enable);
#else
			scsi_setsyncmode(sp, enable);
#endif
		drive_release(dk);
	}
}

/* dksc initialization routine */
void
dkscinit(void)
{
	u_char adap, targ, lu;
	char name[9];
	static int dkscinit_done;

	if (dkscinit_done)
		return;
	dkscinit_done++;

	for(adap = 0; adap < scsicnt; adap++)
	    for(targ = 0; targ < SC_MAXTARG; targ++)
		for(lu = 0; lu < DK_MAXLU; lu++) 
		    init_sema(&dkopensem[adap][targ][lu], 1,
			 "dko", (targ<<5)+(lu<<4));
}

struct dksoftc
*dk_alloc_softc(void)
{
	struct dksoftc *dk;

	dk = (struct dksoftc *)kern_calloc(sizeof(struct dksoftc), 1);

	/*
	 * Even though we only need a couple bytes, allocate them dynamically for
	 *   proper cache-line alignment.  This is important for all DMA buffers
	 *   on machines with a writeback cache (IP17).
	 * XXX Note that I assumed that no stores are done into the sense data, so
	 *   I don't do invalidates before a Request Sense.  -jh
	 */
	dk->dk_sense_data = dmabuf_malloc(DK_SENSE_DATA_SIZE);
	ASSERT(dk->dk_sense_data != NULL);
	dk->dk_inqtyp = dmabuf_malloc(DK_INQTYP_SIZE);
	ASSERT(dk->dk_inqtyp != NULL);
	dk->dk_drivecap = dmabuf_malloc(DK_DRIVECAP_SIZE);
	ASSERT(dk->dk_drivecap != NULL);

	return(dk);
}

void
dk_unalloc_softc(struct dksoftc *dk)
{
	dmabuf_free(dk->dk_sense_data);
	dmabuf_free(dk->dk_inqtyp);
	dmabuf_free(dk->dk_drivecap);

	kern_free(dk);
}

/* free all the data structures and semaphores, called on open errors,
	and also from smfd driver on last closes */
void
dkfreeall(struct dksoftc *dk)
{
	scsisubchan_t *sp = dk->dk_subchan;

	if(sp) {
		dksoftc[sp->s_adap][sp->s_target][sp->s_lu] = NULL;
#ifdef IP22_WD95
		if (sp->s_adap == 0) /* must be wd93 */
			freesubchannel(sp);
		else
			wd95_freesubchannel(sp);
#else
			freesubchannel(sp);
#endif
	}
	/* else failed opens */

	freesplock(&dk->dk_lock);
	freesema(&dk->dk_wait);
	dk_unalloc_softc(dk);
}

/*ARGSUSED*/
void
dkscopen(dev_t dev, int flag, int otyp)
{
	register struct dksoftc	*dk;
	register struct buf *bp;
	u_int	adap, targ, lu, part;
	u_char	stat_return;
	scsisubchan_t		*sp;
	caddr_t vol_hdr;
	char name[9];
	sema_t	*opensem;


	adap = scsi_ctlr(dev);
	targ = scsi_unit(dev);
	lu   = scsi_lu(dev);
	part = dk_part(dev);

	if ((adap >= scsicnt) || 
	    (targ >= SC_MAXTARG) || 
	    (lu >= DK_MAXLU)) { 
		u.u_error = ENXIO;
		return;
	}

	/*
	 * Single thread opens to a device
	 */
	opensem = &dkopensem[adap][targ][lu];
	psema(opensem, PRIBIO);

	/* Check if device already opened */
	if ((dk = dev_to_softc(adap, targ, lu)) == NULL) {
		/* First open */
		dk = dk_alloc_softc();
		if(!dk) {
			u.u_error = ENOMEM;
			goto done;
		}

		/*
		 * Check to see if we've got a disk drive out there.
		 * Initialize the spinlock and fill in the local buf
		 * structures with the necessary device data.
		 *
		 */
	
		init_spinlock(&dk->dk_lock, "dkl", minor(dev));
#ifdef IP22_WD95
		if (adap == 0) {/* must be wd93 */
			if((sp = allocsubchannel(adap, targ, lu)) == NULL) {
				u.u_error = EBUSY;	/* bad adapter # (which we
				checked above), or in use by some other driver */
				goto done;
			}
		}
		else {  /* must be wd95 */
			if((sp = wd95_allocsubchannel(adap, targ, lu)) == NULL) 			{
				u.u_error = EBUSY;	/* bad adapter # (which we
				checked above), or in use by some other driver */
				goto done;
			}
		}
#else
			if((sp = allocsubchannel(adap, targ, lu)) == NULL) {
				u.u_error = EBUSY;	/* bad adapter # (which we
				checked above), or in use by some other driver */
				goto done;
			}
#endif
		dk->dk_subchan = sp;
		dksoftc[adap][targ][lu] = dk;
		sp->s_caller = (caddr_t)dk;	/* for interrupt routines */
		bp = &dk->dk_buf;
		bp->b_dev = dev;

		init_sema(&dk->dk_wait, 0, "dkw", minor(dev));

		/*dk_inquiry[4] = 1; PROM is r/o */
		if (dk_cmd(dk, dk_inquiry, SC_CLASS0_SZ, DKSC_TIMEOUT,
			dk->dk_inqtyp, DK_INQTYP_SIZE, B_READ)) {
			u.u_error = EIO;
			goto done;
		}
		/*	should be direct access, (removable media should
			use the smfd driver; except that MO optical drives
			fit better with the hard disk driver); don't complain
			about WORMs or CDROMs either. Look at all but the
			vendor specific bit; clearly if the controller doesn't
			support the lun we want, or it isn't attached, then
			we want to fail right here. */

		switch(dk->dk_inqtyp[0]&0x3f) {
		case 0:	/* No define for disk... */
		case INV_WORM:
		case INV_CDROM:
		case INV_OPTICAL:
			break;
		default:
			u.u_error = ENODEV;
			goto done;
		}

		/* Check to see if the drive is ready for IO */
		stat_return = dk_chkready(dk, 0);
		if(stat_return == (u_char)-1) {
			u.u_error = EIO;
			goto done; /* Unmap since we couldn't open it */
		}
		else if (stat_return != 0) {
			u.u_error = ENXIO;
			goto done;
		}

		/* Read the volume header (1 block) */
		dk_getblksz(dk);	/* get blksz first! */
		vol_hdr = dmabuf_malloc(dk->dk_blksz);
		if (!dk_rw(dk, vol_hdr, 0, 1, dk_read)) {
			/* if we can't even ready the vh sector, they
			 * are probably in trouble, but it's always possible a
			 * subsequent write might fix the problem, so simply mark
			 * the vh as invalid so no i/o can be done until a SETVH
			 * has been done */

#ifdef DEBUG
			dk_pmsg(dev, "Can't read volume header\n");
#endif

		}
		else if (is_vh((struct volume_header*)vol_hdr)) {
			bcopy(vol_hdr, &dk->dk_vh, sizeof(struct volume_header));
		}
		else {
                        dk_pmsg(dev, "volume header not valid\n");
                        dk->dk_vh.vh_magic = 0; /* indicates invalid header */
		}
                dmabuf_free(vol_hdr);         
		dk->dk_selflags = 0x11;	/* default to PF and SP */
	}
	else if(dk_chkready(dk, 1))
		u.u_error = EIO;
		
	/*	The vh_magic check allows us to open a drive with either no
		partition table or a bad one more than once, which gives us
		a chance to do a SETVH even if not the very first open */
	else if(dk->dk_vh.vh_magic && dk->dk_vh.vh_pt[part].pt_nblks == 0) {
		dk_pmsg(dev, "invalid partition\n");
		u.u_error = ENXIO;
	}

done:
	if(!u.u_error) {
		/* On OK open, set up for sync mode scsi if we can */
		dksyncmode(dk, 1);
		dk->dk_openparts[otyp] |= 1<<part;
		if(otyp == OTYP_LYR)
			dk->dk_lyrcnt[part]++;
	}
	else if(dk && dk_oktoclose(dk)) {
		/* open failed, and no other opens */
		dksyncmode(dk, 0);	/* turn off sync mode, if on */
		dkfreeall(dk);
	}
	vsema(opensem);
}

/*
 * The close routine
 * We don't deallocate the softc structure on dksc close.  We could
 * keep reference counts and free when necessary, but then we'd always
 * have to reread volume header information, etc. upon opening again.
 */
/*ARGSUSED*/
void
dkscclose(dev_t dev, int flag, int otyp)
{
	struct dksoftc	*dk;
	u_int	adap, targ, lu, part;
	sema_t	*opensem;


	adap = scsi_ctlr(dev);
	targ = scsi_unit(dev);
	lu   = scsi_lu(dev);
	part = dk_part(dev);
	opensem = &dkopensem[adap][targ][lu];
	dk = dev_to_softc(adap, targ, lu);

	if (dk == NULL) {
		u.u_error = ENXIO;
		return;
	}
	psema(opensem, PRIBIO);	/* prevent opens on this device */

	if(otyp == OTYP_LYR && dk->dk_lyrcnt[part]) {
		if(--dk->dk_lyrcnt[part] == 0)
			dk->dk_openparts[otyp] &= ~(1<<part);
	}
	else
		dk->dk_openparts[otyp] &= ~(1<<part);
	if(dk_oktoclose(dk)) {
		dksyncmode(dk, 0);	/* turn off sync mode, if on */
		dkfreeall(dk);
	}
	vsema(opensem);
}


/*	dkscioctl -- io controls.  Note that because of the
	ioctl_data stuff, there must be only one return from
	this routine (or all the goto's for errors need to be removed,
	and the 'sync' routines must all break instead of return).
*/
void
dkscioctl(dev_t dev, unsigned int cmd, caddr_t arg, int flag)
{
	struct dksoftc	*dk = dev_to_softc(scsi_ctlr(dev), scsi_unit(dev),
				scsi_lu(dev));
	u_char *ioctl_data;

	switch (cmd) {
	case DIOCSELECT:
	case DIOCADDBB:
		if(!suser())	/* only superuser can modify drive characteristics */
			return;	/* or spare blocks! */
	case DIOCSENSE:
	case DIOCSETVH:
		ioctl_data = dmabuf_malloc(MAX_IOCTL_DATA);
		break;
	case DIOCFORMAT:
		if(!suser())	/* only superuser can format drives */
			return;
	default:
		ioctl_data = NULL;
		break;
	}

	switch (cmd) {

	case DIOCDRIVETYPE:
	{
	    /* return drive name string */
	    u_char cmdbuf[sizeof(dk_inquiry)];

	    /* flag = size of inquiry data plus size of inquiry header */
	    flag = 8 + SCSI_DEVICE_NAME_SIZE;
	    ioctl_data = dmabuf_malloc(flag);
	    bzero(ioctl_data, flag);

	    bcopy(dk_inquiry, cmdbuf, sizeof(dk_inquiry));
	    cmdbuf[4] = flag;
	    if (dk_cmd(dk, cmdbuf, SC_CLASS0_SZ, DKSC_TIMEOUT, ioctl_data,
		       flag, B_READ) != 0) {
		    goto ioerr;
	    }

	    if(copyout(ioctl_data + 8, arg, flag-8)) {
copyerr:
		    u.u_error = EFAULT;
	    }
	    break;
	}

	case DIOCGETVH:
		if(copyout(&dk->dk_vh, arg, sizeof(dk->dk_vh)))
			goto copyerr;
		break;

	case DIOCSETVH:
		if(copyin(arg, ioctl_data, sizeof(dk->dk_vh)))
			goto copyerr;
			
		if(is_vh((struct volume_header *)ioctl_data))
			bcopy(ioctl_data, (caddr_t)&dk->dk_vh, sizeof(dk->dk_vh));
		else
			u.u_error = EINVAL;
		break;

	case DIOCTEST:
		/* some drives (like siemens) take > 30 seconds... */
		if (dk_cmd(dk, dk_send_diag, SC_CLASS0_SZ, 2*DKSC_TIMEOUT,
			0, 0, 0))
ioerr:
			u.u_error = EIO;
		break;

	case DIOCFORMAT:
		/* note we can have a problem if we get the bus while some
		other unit has temporarily disconnected, since Toshiba
		with 23H firmware does NOT disconnect during a format */
		if (dk_cmd(dk, dk_format, SC_CLASS0_SZ, (240*60*HZ),
			0, 0, 0))
			goto ioerr;
		/* check partition sizes again, since some drives don't
		 * return new readcap until after format. */
		break;

	case DIOCSENSE:
	{
		struct dk_ioctl_data sense_args;
		u_char sense_cmd[SC_CLASS0_SZ];
		u_int *sense_data = (u_int *)ioctl_data;

		if(copyin(arg, &sense_args, sizeof(struct dk_ioctl_data)))
			goto copyerr;
		if (sense_args.i_len > MAX_IOCTL_DATA)
			goto invalerr;
		bcopy(dk_mode_sense, sense_cmd, sizeof(sense_cmd));
		sense_cmd[2] = sense_args.i_page;
		sense_cmd[4] = sense_args.i_len;
		if (dk_cmd(dk, sense_cmd, SC_CLASS0_SZ, DKSC_TIMEOUT,
			(u_char *)sense_data, sense_args.i_len, B_READ))
			goto ioerr;
		if(copyout(sense_data, sense_args.i_addr, 
			sense_args.i_len))
			goto copyerr;
		break;
	}

	case DIOCSELFLAGS:
	{
		u_char argbits = (u_char)((__psunsigned_t)arg & 0xff);

		if(argbits & 0xe0)	/* can't use to set LUN! */
			u.u_error = EINVAL;
		dk->dk_selflags = argbits;
		break;
	}
		
	case DIOCSELECT:
	{
		struct dk_ioctl_data select_args;
		u_char select_cmd[SC_CLASS0_SZ];
		u_int  *select_data = (u_int *)ioctl_data;
		u_int  status;

		if(copyin(arg, &select_args, sizeof(struct dk_ioctl_data)))
			goto copyerr;
		if (select_args.i_len > MAX_IOCTL_DATA)
			goto invalerr;
		bcopy(dk_mode_select, select_cmd, sizeof(select_cmd));
		select_cmd[4] = select_args.i_len;
		select_cmd[1] = dk->dk_selflags;
		if(copyin(select_args.i_addr, select_data, 
			select_args.i_len))
			goto copyerr;
		if (dk_cmd(dk, select_cmd, SC_CLASS0_SZ, DKSC_TIMEOUT, 
			(u_char *)select_data, select_args.i_len, B_WRITE))
			goto ioerr;
		status = dk_cmd(dk, dk_tst_unit_rdy, SC_CLASS0_SZ, 
			DKSC_TIMEOUT, 0, 0, B_READ);
		dk_getblksz(dk);	/* in case msel changed it */
		if ((status !=0) && (status != SC_RESET))
			goto ioerr;
		break;
	}

	case DIOCREADCAPACITY:
	{
		flag = dk_readcap(dk);
		if(copyout(&flag, arg, sizeof(u_int)))
			goto copyerr;
		break;
	}

	case DIOCRDEFECTS:
	{
		struct dk_ioctl_data defect_args;
		u_char defect_cmd[SC_CLASS1_SZ];

		if(copyin(arg, &defect_args, sizeof(struct dk_ioctl_data)))
			goto copyerr;

		/*	allocate amount user requested; drives have gotten bigger
			than will fit into MAX_IOCTL_DATA; should fix someday to
			just dma directly into users buffer */
		if(defect_args.i_len > (100*NBPC))
			goto invalerr;
		ioctl_data = dmabuf_malloc(defect_args.i_len);

		bcopy(dk_rdefects, defect_cmd, sizeof(defect_cmd));
		/*	used to be hardcoded to phys sect, and only wanted the
			MDL/GDL bits.  Now caller has to supply entire byte.
			Allows fx to request different formats.  Olson, 6/88
		*/
		defect_cmd[2] |= defect_args.i_page;
		defect_cmd[7] = defect_args.i_len >> 8;
		defect_cmd[8] = defect_args.i_len & 0xff;
		if (dk_cmd(dk, defect_cmd, SC_CLASS1_SZ, DKSC_TIMEOUT,
			ioctl_data, defect_args.i_len, B_READ))
			goto ioerr;
		if(copyout(ioctl_data, defect_args.i_addr, 
			defect_args.i_len))
			goto copyerr;
		break;
	}

	case DIOCADDBB:	/* spare a single (logical) block */
	{
		daddr_t bn = (daddr_t)arg;
		u_char *bbinfo = ioctl_data;
		bzero(bbinfo, sizeof(bbinfo));

		bbinfo[3] = 4;	/* one block (4 bytes) */
		bbinfo[4] = (u_char)(bn>>24);
		bbinfo[5] = (u_char)(bn>>16);
		bbinfo[6] = (u_char)(bn>>8);
		bbinfo[7] = (u_char)bn;
		if(dk_cmd(dk, dk_add_defects, SC_CLASS0_SZ, DKSC_TIMEOUT, bbinfo,
			8, B_WRITE))
			goto ioerr;
		break;
	}
#ifdef __CPQ
        case DIOCGETPARTYPE: /* get FAT partition type */
        {
                if (dk_part(dev))       /* if not raw record */
                   *arg = dk->dk_mbr.partition[dk_part(dev)-1].system_indicator;                else                    /* raw record       */
                    *arg =0;
        }
#endif

	case DIOCSETBLKSZ:
		if(copyin(arg, &flag, sizeof(int)))
			u.u_error = EFAULT;
		else if(!(flag = dk_setblksz(dk, flag)))
			u.u_error = EIO;
		else copyout(&flag, arg, sizeof(int));
		break;

	default:
invalerr:
		u.u_error = EINVAL;
		break;
	}

	if(ioctl_data)
		dmabuf_free(ioctl_data);
}




/*
 * queue device request 
 */
void
dkscstrategy(register buf_t *bp)
{
	register struct dksoftc	*dk = dev_to_softc(scsi_ctlr(bp->b_dev), 
				scsi_unit(bp->b_dev), scsi_lu(bp->b_dev));
	register struct iobuf *list_head;
	register struct partition_table *pt;
	register int sc, ioblocks;
	int s;

	/* Is device open, and is the vh valid? */

	if(dk == NULL || !dk->dk_vh.vh_magic)
		goto bad;
	/* 
	 * Compute number of blocks in transfer and make sure the request 
	 * is contained in the partition.  large reads/writes that could
	 * be partially satisfied will fail because of the difficulty of
	 * handling the residual count correctly.
	 */
	if(dkCHKBB(bp->b_bcount))
		goto bad;

	sc = dkBTOBB(bp->b_bcount);
	if(!sc) {
		bp->b_resid = bp->b_bcount;
		iodone(bp);
		return;
	}
	pt = &dk->dk_vh.vh_pt[dk_part(bp->b_dev)];

	if ((bp->b_blkno < 0) || (bp->b_blkno+sc) > pt->pt_nblks) {
			bp->b_error = ENOSPC;
			goto bad;
	}

	if(!(bp->b_flags & B_READ) && (dk->dk_flags & DK_WRTPROT)) {
			bp->b_error = EROFS;
			goto bad;
	}

	if(dkneedstart[scsi_ctlr(bp->b_dev)][scsi_unit(bp->b_dev)]) {
		/* drive was not ready at last intr; try to start it */
		dkneedstart[scsi_ctlr(bp->b_dev)][scsi_unit(bp->b_dev)] = 0;
		(void)dkstartunit(dk, 1);
	}

	/* set block number for disksort() and dkscstart() */
	bp->b_sort = bp->b_blkno + pt->pt_firstlbn;
	bp->av_forw = NULL;

	list_head = &dk->dk_tab;

	s = splockspl(dk->dk_lock, spl5);

	for(ioblocks=0; ioblocks < (NUM_BLK_STATS-1); ioblocks++)
		if(!(sc /= 2))
			break;
	if(!(bp->b_flags & B_READ))
		dk->dk_blkstats.bk_writes[ioblocks]++;
	else 
		dk->dk_blkstats.bk_reads[ioblocks]++;

	disksort(list_head, bp);
	if (list_head->io_state.active == 0) {
		list_head->io_state.active = 1;
		/* set here so doesn't get reset on re-tries, etc. */
		dk->dk_retry = 0;
		spunlockspl(dk->dk_lock, s);
		dkscstart(dk, list_head->io_head);
	}
	else
		spunlockspl(dk->dk_lock, s);

	return;
bad:
	bp->b_flags |= B_ERROR;
	if(!bp->b_error)
		bp->b_error = EIO;
	iodone(bp);
}

/*
 * setup a device operation for calls from outside the driver; upper
 * levels of kernel are assumed to do the dcache_{wb,inval} if needed.
 */
void
dkscstart(register struct dksoftc *dk, register struct buf *bp)
{
	register scsisubchan_t *sp = dk->dk_subchan;
	int sn, sc;

	/* Let the scsi driver do the transfer */
	bcopy((bp->b_flags & B_READ) ? dk_read : dk_write, dk->dk_cmd, 
		SC_CLASS1_SZ);

	sc = dkBTOBB(bp->b_bcount);
	sn = bp->b_sort;
	bp->b_error = 0; 	/* in case set on retry, etc */

	/*
	 * Stuff in the logical block addr. 
	 * Its spot in the cmd is on a short boundary.
	 */
	dk->dk_cmd[2] = (u_char)(sn >> 24);
	dk->dk_cmd[3] = (u_char)(sn >> 16);
	dk->dk_cmd[4] = (u_char)(sn >> 8);
	dk->dk_cmd[5] = (u_char)sn;
	dk->dk_cmd[7] = (u_char)(sc >> 8);
	dk->dk_cmd[8] = (u_char)sc;

	sp->s_cmd = dk->dk_cmd;
	sp->s_len = SC_CLASS1_SZ;
	sp->s_buf = bp;
	sp->s_notify = dk_intr;
	sp->s_timeoutval = DKSC_TIMEOUT;

#if defined(SN0)
        if (!(bp->b_flags & B_READ)) {
                munge((u_char *)bp->b_dmaaddr, bp->b_bcount);
		us_delay(500) ;
	}
#endif

#ifdef IP22_WD95
	if (sp->s_adap == 0) /* must be wd93 */
		doscsi(sp);
	else
		wd95_doscsi(sp);
#else
		doscsi(sp);
#endif
}

/*
 * dksc interrupt routine, called via scsisubrel().
 *
 */
static void
dk_intr(register scsisubchan_t *sp)
{
	register struct buf *bp = sp->s_buf;
	register struct dksoftc	*dk = (struct dksoftc *)sp->s_caller;
	register struct buf *abp;
	int error = 0;

	ASSERT(bp == dk->dk_tab.io_head);
	ASSERT(bp->b_flags & B_BUSY);

	/*
	 * If necessary, perform a request sense command to the drive.
	 * (This is basicly an asynchronous version of dk_check_condition()).
	 */
	switch(sp->s_error) {
	case SC_GOOD:
		if(sp->s_status == ST_BUSY) {
			if (dk->dk_retry++ < DK_MAX_RETRY) {
				timeout((void (*)())dk_retry, (void *)dk,
					HZ*4, bp);
				return;
			}
			else {
				error = EIO;
				goto intrdone;
			}
		}
		else if (sp->s_status == ST_CHECK) {
			/* do async request sense, dk_intrend() from
			 * dk_checkintr() does the cleanup 
			 */
			abp = &dk->dk_buf;
			abp->b_dmaaddr = (char *)dk->dk_sense_data;
			abp->b_bcount = DK_SENSE_DATA_SIZE;
			abp->b_flags = B_READ;
			abp->b_error = 0;
			bcopy(dk_req_sense, dk->dk_cmd, SC_CLASS0_SZ);
			sp->s_cmd = dk->dk_cmd;
			sp->s_len = SC_CLASS0_SZ;
			sp->s_buf = abp;
			sp->s_notify = dk_checkintr;
			sp->s_timeoutval = DKSC_TIMEOUT;
			if (cachewrback)
				dcache_inval(abp->b_dmaaddr, abp->b_bcount);
#ifdef IP22_WD95
			if (sp->s_adap == 0) /* must be wd93 */
				doscsi(sp);
			else
				wd95_doscsi(sp);
#else
				doscsi(sp);
#endif
			dcache_inval(abp->b_dmaaddr, abp->b_bcount);
			return;
		}
		else if (sp->s_status != ST_GOOD) {
			/* Strange but retry anyway, if retries left */
			dk_pmsg(bp->b_dev, "Unexpected status %d from scsi driver\n",
				sp->s_status);
			if (dk->dk_retry++ < DK_MAX_RETRY) {
				dkscstart(dk,bp);
				return;
			}
			error = EIO;
		}
		break;
	case SC_ALIGN:
		error = EFAULT;
		break;
	case SC_CMDTIME:
	case SC_HARDERR:
		if(dk->dk_retry++ < DK_MAX_RETRY) {
		/*	retry cmds that failed because of an timeout, or a
			scsi reset.  It may cause more timeouts, but that's better
			than just letting async writes fail completely, which
			otherwise is quite likely to happen */
			dkscstart(dk, bp);
			return;
		}
		/* else fall thru */
	default:
		dk_pmsg(bp->b_dev, "SCSI driver error %d\n", sp->s_error);
		error = EIO;
	}
intrdone:
	if (error) {
		bp->b_flags |= B_ERROR;
		bp->b_error = error;
	}
	dk_intrend(dk, bp);
}

/*
 * dksc interrupt handler for asynchronous request sense command.
 * called via s_notify from scsi.c
 */
static void
dk_checkintr(register scsisubchan_t *sp)
{
        struct buf *bp;
        struct dksoftc *dk = (struct dksoftc *)sp->s_caller;
        int error = 0;

        bp = dk->dk_tab.io_head;

        if(sp->s_error == SC_GOOD && sp->s_status == ST_GOOD) {
                /* if the request sense succeeded, see what happened; buffer
                 * was being invalidated at doscsi call, but that isn't right */
                dcache_inval(dk->dk_sense_data, DK_SENSE_DATA_SIZE);
                (void)dk_interpret_sense(dk, (char *)dk->dk_sense_data, bp->b_dev);

                /* retry original cmd on any failure; was only retrying on reset
                 * and writefault.  That was the last vestige of "don't want to                 * spend time retrying except on a few things".
                 * That was sort of silly; the user would really like us
                 * to try a bit harder, and we do tell them we are retrying.
                 * reqsense retries count against original cmd here also...
                */
                if (dk->dk_retry++ < DK_MAX_RETRY) {
                        /* can't test b_flags because of internal cmds, so
                         * don't bother to say what we are retrying, hopefully
                         * the earlier message from interpret makes it clear
                         * delay (more each time) before retrying, mostly because
                         * particularly bus resets; start with an immediate retry
                         */
                        dk_pmsg(bp->b_dev, "retrying\n");
                        if(dk->dk_retry>2)
                                timeout((void (*)())dk_retry, (void *)dk,
					dk->dk_retry * HZ, (void *)bp);
                        else
                                dkscstart(dk, bp);
                        return;
                }
                else
                        error = EIO;
        }
        else if (dk->dk_retry++ < DK_MAX_RETRY) {
                /* old code was going back to original cmd if we got a BUSY;
                 * we want to retry the request sense!  All fields should still                 * be OK on a busy (also on other reqsense errors).
                 * Note that this means that reqsense retries get counted
                 * against original command.  Not quite right, but this fixup
                 * is the best I can do for 405H w/o significant changes to
                 * structures; 5.0 will be better (don't need to bother to
                 * check for SC_ALIGN, since our buffer is always aligned OK) */
                char *msg = sp->s_status == ST_BUSY ? "device busy" :
                        "previous failed";
                dk_pmsg(bp->b_dev, "retrying request sense (%s)\n", msg);
                timeout((void (*)())dk_resense, (void *)sp, dk->dk_retry*HZ,
			0);
                return;
        }
        else
              dk_pmsg(bp->b_dev,
                "reqsense: SCSI error %d status %d; original cmd not retried\n",
                sp->s_error, sp->s_status);

        /* only get here on hard (no more retries) errors */
        bp->b_flags |= B_ERROR;
        bp->b_error = error;
        dk_intrend(dk, bp);
}

/*
 * dksc interrupt clean-up routine -- common wrapup code for
 *	dk_intr and dk_checkintr.
 */
void
dk_intrend(struct dksoftc *dk, struct buf *bp)
{
	register struct iobuf *list_head = &dk->dk_tab;
	register struct buf *nextbp;
	int s;

	s = splockspl(dk->dk_lock, spl5);	/* don't need the spl, but just using
		splock() forces it to splhi... */
	nextbp = bp->av_forw;	/* must be set while we hold dk_lock so
		some other CPU doesn't call disksort and change av_forw after
		we grabbed it */

	list_head->io_head = nextbp;

	/* Before possible dkscstart clears fields. */
	if (dk->dk_retry&&(bp->b_error || (bp->b_flags&B_ERROR)))
		dk_pmsg(bp->b_dev,"Exhausted retries, i/o failed\n");

	/*
	 * As synchronous requests are pretty rare, give them first crack.
	 * Otherwise start up the next asynchronous request, if any.
	 */
	if(cvsema(&dk->dk_wait) || !nextbp) {
		list_head->io_state.active = 0;
		spunlockspl(dk->dk_lock, s);
	}
	else {
		dk->dk_retry = 0;
		spunlockspl(dk->dk_lock, s);
		dkscstart(dk, nextbp); /* start next request */
	}

#if defined(SN0)
	/* Since B_WRITE is 0, we cannot test for the B_WRITE
	 * set flag. The if clause below may not be needed but
  	 * we are trying to do the munge for reads and writes only.
	 * if ((bp->b_flags & B_READ) || (!(bp->b_flags & B_READ)))
	 */
	munge((u_char *)bp->b_dmaaddr, bp->b_bcount);

#ifdef MSCSI_DEBUG
	dump_buf(	(char *)bp->b_dmaaddr, 
			"Dumping data in dk_intrend after munge ...\n",
			64) ;
#endif

#endif 	/* SN0 */

	iodone(bp);
}


/* Disk read/write command */
static int
dk_rw(register struct dksoftc *dk, caddr_t addr, int block_num, int block_len,
	u_char *cmd_buffer)
{
	register struct buf	*bp = &dk->dk_buf;
	register scsisubchan_t	*sp = dk->dk_subchan;
	u_char serr, sstat;

	drive_acquire(dk);

	bp->b_dmaaddr = addr;
	bp->b_bcount = dkBBTOB(block_len);
	if(cmd_buffer == dk_read)
		bp->b_flags = B_READ|B_BUSY;
	else {
		if(dk->dk_flags & DK_WRTPROT)
			return 1;
		bp->b_flags = B_WRITE|B_BUSY;
	}
	bp->b_error = 0;

	bcopy(cmd_buffer, dk->dk_cmd, SC_CLASS1_SZ);
	/*
	 * Stuff in the logical block addr. 
	 * Its spot in the cmd is on a short boundary.
	 */
	dk->dk_cmd[2] = (u_char)(block_num >> 24);
	dk->dk_cmd[3] = (u_char)(block_num >> 16);
	dk->dk_cmd[4] = (u_char)(block_num >> 8);
	dk->dk_cmd[5] = (u_char)block_num;
	dk->dk_cmd[7] = (u_char)(block_len >> 8);
	dk->dk_cmd[8] = (u_char)block_len;

	sp->s_cmd = dk->dk_cmd;
	sp->s_len = SC_CLASS1_SZ;
	sp->s_buf = bp;
	sp->s_notify = NULL;
	sp->s_timeoutval = DKSC_TIMEOUT;

#if defined(SN0)
        if (!(bp->b_flags & B_READ)) {
		munge((u_char *)bp->b_dmaaddr, bp->b_bcount);
		us_delay(10000) ;
	}
#endif

	/*
	 * Writeback needed before write.  Reads need only an invalidate, but
	 * writeback works, and dk_rw is not a performance path.
	 */
	dcache_wb(addr, bp->b_bcount);
#ifdef IP22_WD95
	if (sp->s_adap == 0) /* must be wd93 */
		doscsi(sp);
	else
		wd95_doscsi(sp);
#else
		doscsi(sp);
#endif

	if (bp->b_flags & B_READ) {
		dcache_inval(addr, bp->b_bcount);

#if defined(SN0)
		munge((u_char *)bp->b_dmaaddr, bp->b_bcount);
#endif

	}

	serr = sp->s_error;
	sstat = sp->s_status;	/* before release */
	drive_release(dk);

	return serr == SC_GOOD && !dk_check_condition(dk, sstat, 1);
}

/* General disk command for calls generated from within the driver */
static int
dk_cmd(register struct dksoftc *dk, u_char *cmd_buffer, int cmd_size,
	u_int timeoutval, u_char *addr, size_t len, int rw)
{
	register struct buf	*bp = &dk->dk_buf;
	register scsisubchan_t	*sp = dk->dk_subchan;
	int ret;
	u_char serr, sstat;

	dk->dk_retry = 0;	/* for s_error retries */
retry:
	drive_acquire(dk);

	bp->b_dmaaddr = (caddr_t)addr;
	bp->b_bcount = len;
	bp->b_flags = rw|B_BUSY;
	bp->b_error = 0;
	bcopy(cmd_buffer, dk->dk_cmd, cmd_size);

	sp->s_cmd = dk->dk_cmd;
	sp->s_len = cmd_size;
	sp->s_buf = bp;
	sp->s_notify = NULL;
	sp->s_timeoutval = timeoutval;

	/*
	 * Cache writeback needed before DMA on writes and to avoid writeback after
	 * DMA on reads.  Reads actually need only an invalidate, but a writeback
	 * works, and dk_cmd is not a performance path.
	 */
	if (len != 0)
		dcache_wb(addr, len);
#ifdef IP22_WD95
	if (sp->s_adap == 0) /* must be wd93 */
		doscsi(sp);
	else
		wd95_doscsi(sp);
#else
		doscsi(sp);
#endif
	if ((len != 0) && (rw & B_READ))
		dcache_inval(addr, len);

	bp->b_flags = 0;	/* done with it; bp not used by any caller */

	serr = sp->s_error;
	sstat = sp->s_status;	/* before release */
	drive_release(dk);

	if(serr != SC_GOOD) {
		/* this is for bus resets and parity errors; don't retry on a timeout
		 * of either type. */
		if(serr != SC_ALIGN && serr != SC_CMDTIME && serr != SC_TIMEOUT &&
			dk->dk_retry++ < DK_MAX_RETRY) {
			goto retry;
		}
		ret = (serr == 0) ? -1 : serr;
	}
	else if(ret=dk_check_condition(dk, sstat, 1)) {
		if(ret == SC_RESET)
			goto retry; /* cmd failed from reset, or unit attn, retry it */
		ret = (serr == 0) ? -1 : serr;
	}
	else 
		ret = 0;
	return ret;
}


/*
 * Check condition - Checks to see if the last command failed with check
 * condition status. If so it does a check condition command and prints
 * out condition info. We use non_extended sense data in this command.
 * Most error conditions are fatal. They return -1. Those that are not
 * fatal return the sense code from the non-extended sense request.
 * Non-errors return 0.
 */
int
dk_check_condition(struct dksoftc *dk, u_char status, int acq)
{
	register struct buf *bp;
	register scsisubchan_t *sp = dk->dk_subchan;

	if (status == ST_GOOD)
		return(0);
	if (status == ST_BUSY) {
		delay(HZ*4);	/* if we don't delay, we'll just get busy again */
		return(SC_RESET);	/* so command is re-tried.  We see this
		on some drives that take a long time to recover from a RESET */
	}
	else if (status != ST_CHECK) {
		dk_pmsg(dk->dk_buf.b_dev, "Bad status return. Status=%d\n", status);
		return(-1);
	}

	if(acq)
		drive_acquire(dk);

	bp = &dk->dk_buf;
	bp->b_dmaaddr = (char *)dk->dk_sense_data;
	bp->b_bcount = DK_SENSE_DATA_SIZE;
	bp->b_flags = B_READ|B_BUSY;
	bcopy(dk_req_sense, dk->dk_cmd, SC_CLASS0_SZ);

	sp->s_cmd = dk->dk_cmd;
	sp->s_len = SC_CLASS0_SZ;
	sp->s_buf = bp;
	sp->s_notify = NULL;
	sp->s_timeoutval = DKSC_TIMEOUT;

	if (cachewrback)
		dcache_inval(bp->b_dmaaddr, bp->b_bcount);
	bp->b_error = 0;
#ifdef IP22_WD95
	if (sp->s_adap == 0) /* must be wd93 */
		doscsi(sp);
	else
		wd95_doscsi(sp);
#else
		doscsi(sp);
#endif
	dcache_inval(bp->b_dmaaddr, bp->b_bcount);

	bp->b_flags = 0;	/* done with it */
	if ((sp->s_error != SC_GOOD) ||
	    (sp->s_status != ST_GOOD)) {
		dk_pmsg(bp->b_dev, "error %d status %d on disk status check\n",
			sp->s_error, sp->s_status);
		if(acq)
			drive_release(dk);
		return(-1);
	} else {
		if(acq)
			drive_release(dk);
		return(dk_interpret_sense(dk, (char *)dk->dk_sense_data, bp->b_dev));
	}
}


int
dk_interpret_sense(struct dksoftc *dk, char *sense_data, dev_t dev)
{
	register char *key_msg, *addit_msg = NULL;
	register u_int sense_key, addit_sense;
	char unknown[9];

	/* Extended sense data is all that is allowed */
	if((sense_data[0] & 0x70) != 0x70) {
		dk_pmsg(dev, "invalid sense data %x, can't determine cause of error\n",
			sense_data[0]);
		return SC_RESET;	/* an error, such as a scsi reset probably
			caused this.  return SC_RESET so cmd will be re-tried
			(usually). */
	}
	/* Extended sense format */
	sense_key = sense_data[2] & 0xf;
	if(!sense_key)	/* shouldn't happen... */
		return 0;
	addit_sense = sense_data[12];
	if (sense_data[2] & 0x20)
		dk_pmsg(dev, "unsupported block size requested\n");
	if(addit_sense == SC_NOTREADY) {
		/* can't do start from here because vsema(sp->s_sem) hasn't
			been done by the time this routine is called (when doing
			async cmds). */
		dkneedstart[scsi_ctlr(dev)][scsi_unit(dev)] = 1;
	}

	if(sense_key == SC_UNIT_ATTN) {	/* SC_RESET so cmd will be retried, */
		addit_sense = SC_RESET; /* and no message printed */
		key_msg = NULL;
	}
	else {
		key_msg = scsi_key_msgtab[sense_key];
		if (addit_sense < SC_NUMADDSENSE) {
			/* don't report 'recovered' errors on defect list; it
			   means the drive doesn't support the requested
			   format, and is responding with it's default format.
			   Also don't report resets, since it was usually due
			   to a timeout/phase error in  scsi.c.  If drive didn't
			   give us an additional sense byte, change to 0xff, because
			   upper layers key off the value we return, which is
			   addit_sense! */
			if(addit_sense == SC_RESET ||
				(addit_sense == SC_DEFECT_ERR && sense_key==SC_ERR_RECOVERED))
				key_msg = NULL;
			else if(addit_sense == SC_NO_ADD) {
				addit_sense = 0xff;
				addit_msg = NULL;
			}
			else
				addit_msg = scsi_addit_msgtab[addit_sense];
		}
		else {
			/* 0x89 is vendor specific; this is the Toshiba XM3332 meaning */
			if(dk->dk_inqtyp[0] == INV_CDROM && addit_sense == 0x89)
				addit_msg = "Tried to read audio track as data";
			else {
				/* print as a number if we don't know about it */
				addit_msg = unknown;
				sprintf(unknown, "ASC=0x%x", addit_sense&0xff);
			}
		}
	}

	if ((addit_sense == SC_NOTREADY) && (dk->dk_flags & DK_SILENT_NOTREADY))
		key_msg = NULL;
	if (key_msg != NULL) {
		int part = dk_part(dev);
		dk_pmsg(dev, "%s", key_msg);
		if (addit_msg != NULL) {
			cmn_err(CE_CONT, ": %s", addit_msg);
			/* print asq if non-zero */
			if(sense_data[13])
				cmn_err(CE_CONT, ", ASQ=0x%x",
					sense_data[13]);
		}
		if(sense_data[0] & 1) 	/* just in case buffer writes enabled! */
			cmn_err(CE_CONT, " (deferred error) ");
		if(sense_data[15] & 0x80) {
			/* SCSI-2 drives give more info if FPV bit set */
			cmn_err(CE_CONT, ", (%s byte %d)", 
			(sense_data[15] & 0x40) ? "cmd" : "data",
			(sense_data[16] << 8) | (sense_data[17]));
			if(sense_data[15] & 0x8)
				cmn_err(CE_CONT, " (bit %d)", 
					sense_data[15] & 0x7);
		}
		if(sense_data[0] & 0x80) {
			u_int check_status_lba, lba;
			check_status_lba = (sense_data[3] << 24) |
			   (sense_data[4] << 16) |
			   (sense_data[5] << 8)  |
			   (sense_data[6]);
			lba = check_status_lba - dk->dk_vh.vh_pt[part].pt_firstlbn;
			/* report both the block in the partition, and the
				disk block # (because fx wants to spare by
				disk block #, not relative to partition... */
			cmn_err(CE_CONT, ", block #%d", lba);
			if(lba != check_status_lba)
				cmn_err(CE_CONT, " (%d)\n", check_status_lba);
			else
				cmn_err(CE_CONT, "\n");
		}
		else
			cmn_err(CE_CONT, "\n");
		if (sense_key == SC_ERR_RECOVERED)
			addit_sense = 0;
	}
	return(addit_sense);
}

/*
 * Acquire use of the subchannel as to not collide with other SCSI requests
 * Don't do it if we are dumping though!
 *
 * If a transfer is currently in progress, simply go to sleep.
 * Otherwise, set io_state.active and the drive is ours!
 * Note that this is essentially done 'inline' in dkscstrategy;
 * The two routines must be kept in sync.
 */
void
drive_acquire(struct dksoftc *dk)
{
	register struct iobuf *list_head;
	int s;

	list_head = &dk->dk_tab;
	s = splockspl(dk->dk_lock, spl5);

	while (list_head->io_state.active) {
		/* This from the original interrupt based scheme.
		 * In stand we are poll based we should never be
		 * in this situation.
		 */
		panic("driver_acquire: io_state.active deadlock\n");
#if XXX
		svpsema(dk->dk_lock, &dk->dk_wait, PRIBIO);
		(void)splockspl(dk->dk_lock, spl5);	/* should still be at spl5 */
#endif /* XXX */
	}
	list_head->io_state.active = 1;
	spunlockspl(dk->dk_lock, s);
}

/*
 * Release the drive and start up someone waiting for it.
 * Don't do it if we are dumping though!
 *
 * Try to start up waiters with synchronous requests first since these should
 * be rare events.  If none, start up asynchronous requests, if any.
 */
void
drive_release(struct dksoftc *dk)
{
	register struct buf *nextbp;
	register struct iobuf *list_head;
	int s;

	list_head = &dk->dk_tab;

	s = splockspl(dk->dk_lock, spl5);
	ASSERT(list_head->io_state.active != 0);
	if(cvsema(&dk->dk_wait) || !(nextbp = list_head->io_head)) {
		list_head->io_state.active = 0;
		spunlockspl(dk->dk_lock, s);
	}
	else {
		dk->dk_retry = 0;
		spunlockspl(dk->dk_lock, s);
		dkscstart(dk, nextbp);	/* start next request */
	}
}


/*	Check to see if the drive is ready for IO.  If comes back with
	"drive not ready", send a start-unit and try again.  This is
	primarily for the Gigabox on IP6, since we jumper CDC drives
	to NOT spin-up until a start-unit.  Without the start-unit here,
	if the Gigabox is powered off, none of the drives can be accessed
	until the system is rebooted.  The drives are jumpered that way
	for two reasons: it reduces the load on the power supply to spin
	up one at a time, and a side-effect is that CDC drives won't 
	respond to SCSI commands during spinup and calibration (about 30
	seconds) without this jumper.

	The drive_acquire is only needed if this isn't first open.

	Have to chk for SC_RESET, since you normally get one when
	you start the drive, and on very first open.
*/
int
dk_chkready(struct dksoftc *dk, int acq)
{
	struct buf *bp = &dk->dk_buf;
	scsisubchan_t *sp = dk->dk_subchan;
	u_char	stat_return;
	u_char serr, sstat;
	int retries = 40;	/* allow for SC_RESET's and starts */

	dk->dk_flags |= DK_SILENT_NOTREADY;
notready:
	stat_return = 0xff;
	if(acq)
		drive_acquire(dk);
	bp->b_bcount = 0;
	bp->b_flags = B_BUSY;
	bp->b_error = 0;
	bcopy(dk_tst_unit_rdy, dk->dk_cmd, SC_CLASS0_SZ);
	sp->s_cmd = dk->dk_cmd;
	sp->s_len = SC_CLASS0_SZ;
	sp->s_buf = bp;
	sp->s_notify = NULL;
	sp->s_timeoutval = 5*HZ;
#ifdef IP22_WD95
	if (sp->s_adap == 0) /* must be wd93 */
		doscsi(sp);
	else
		wd95_doscsi(sp);
#else
		doscsi(sp);
#endif
	serr = sp->s_error;
	sstat = sp->s_status;	/* before release */
	if(acq)
		drive_release(dk);

#if defined(SN0) 
	/* XXX */
	stat_return = dkstartunit(dk, acq);
#endif	/* SN0 */
	
	if(serr != SC_GOOD) {
		if(retries-- > 0)
			goto notready;
	}
	else if((stat_return = dk_check_condition(dk, sstat, 1)) &&
		retries--) {
		if(stat_return == SC_RESET)
			goto notready;
		if(stat_return == SC_NOTSEL || stat_return == SC_NOTREADY) {
			stat_return = dkstartunit(dk, acq);
			if(!stat_return || stat_return == SC_RESET ||
				stat_return == SC_NOTREADY)
				goto notready;
		}
	}
	dk->dk_flags &= ~DK_SILENT_NOTREADY;
	if (stat_return)
		dk_pmsg(dk->dk_buf.b_dev, "drive is not ready\n");
	return (int)stat_return;
}


static u_char
dkstartunit(struct dksoftc *dk, int acq)
{
	struct buf *bp = &dk->dk_buf;
	scsisubchan_t *sp = dk->dk_subchan;
	u_char sstat, serr;

	if(acq)
		drive_acquire(dk);
	bp->b_bcount = 0;
	bp->b_flags = B_BUSY;
	bp->b_error = 0;
	bcopy(dk_startunit, dk->dk_cmd, SC_CLASS0_SZ);
	sp->s_cmd = dk->dk_cmd;
	sp->s_len = SC_CLASS0_SZ;
	sp->s_buf = bp;
	sp->s_notify = NULL;
	sp->s_timeoutval = 240*HZ;	/* could take a long time! */
#ifdef IP22_WD95
	if (sp->s_adap == 0) /* must be wd93 */
		doscsi(sp);
	else
		wd95_doscsi(sp);
#else
		doscsi(sp);
#endif
	sstat = sp->s_status;
	serr = sp->s_error;
	if(acq)
		drive_release(dk);
	if(serr != SC_GOOD)
		sstat = SC_RESET;
	else
		sstat = dk_check_condition(dk, sstat, acq);
	return sstat;
}


/*
 * info for "standardized device name for error reporting:
 *	1. This device name
 *	2. A macro for converting to "standard" format
 */

/*	print error messages using STDNAME to
	return the partion name from the dk struct info.  Saves code
	and head-scratching by just doing it in one place (not to 
	mention reducing the number of times the string "dks" appears
	in the data space, and the code required to do the shifts and
	masks).
*/
static void
dk_pmsg(dev_t dev, char *fmt, ...)
{
	STDNBUF();
	va_list ap;

	cmn_err(CE_CONT, "%s ",  STDNAME(scsi_ctlr(dev), scsi_unit(dev), dk_part(dev)));
	va_start(ap, fmt);
	vprintf(fmt, ap);	/* sorta like icmn_err(CE_CONT,fmt,ap) */
	va_end(ap);
}

/* this is used to retry a command when a drive returns busy; 
 * called from a timeout.
 */
static void
dk_retry(struct dksoftc *dk, buf_t *bp)
{
        dkscstart(dk, bp);
}

/* retry a request sense command after a delay (to give device time
 * to recover, we hope) */
static void
dk_resense(register scsisubchan_t *sp)
{
#ifdef IP22_WD95
	if (sp->s_adap == 0) /* must be wd93 */
		doscsi(sp);
	else
		wd95_doscsi(sp);
#else
		doscsi(sp);
#endif
}

/*	it's ok to really close a drive (release all it's resources
	only if none of it's partitions are in use for any type.
*/
int
dk_oktoclose(register struct dksoftc *dk)
{
	register int i;

	for(i=0; i<OTYPCNT; i++)
		if(dk->dk_openparts[i])
			return 0;
	return 1;
}

/* return drive capacity; used for validating vh.  Someday we might use
 * use the block size info also.
*/
static int
dk_readcap(register struct dksoftc *dk)
{
	int oldcap; 

	oldcap = dk->dk_drivecap[0];

	if(dk_cmd(dk, dk_readcapacity, SC_CLASS1_SZ, DKSC_TIMEOUT, 
		(u_char *)dk->dk_drivecap, DK_DRIVECAP_SIZE, B_READ)) {
		dk->dk_drivecap[0] = oldcap;	/* restore old, in case
			trash placed in capacity */
		return -1;
	}
#ifdef _MIPSEL
	swapl(dk->dk_drivecap,2);
#endif
	/* increment by one to get capacity, not last block */
	return ++dk->dk_drivecap[0];
}


/*	This is used at open time to get the blksize, before the vh is read.
	After the vh is read, or when the vh ioctl's are used, the blksz
	from the vh is used.
	Since not all devices support the FORMAT page, ask for all pages;
	Uses the value from the block descr if supplied, since that
	location is correct for all drives, not just ccs or scsi2.  If not,
	use the value from the format page.
	If modesense is successful, set or clear the WRTPROT bit.  This is
	mainly for early detection of devices that are either write protected,
	or don't support the write command (such as CDROM).  Otherwise may
	get SCSI errors.
*/
static void
dk_getblksz(register struct dksoftc *dk)
{
	struct mode_sense_data *msd;
	u_char sense_cmd[SC_CLASS0_SZ];

	dk->dk_blksz = 0;
	if((msd = (struct mode_sense_data *) dmabuf_malloc(sizeof (*msd))) == NULL)
		goto failure;
	bzero(msd, sizeof(*msd));
	
	bcopy(dk_mode_sense, sense_cmd, sizeof(sense_cmd));
	sense_cmd[2] = msd->dk_pages.common.pg_code = ALL|CURRENT;
	sense_cmd[4] =
		msd->dk_pages.common.pg_len = sizeof(msd->dk_pages.common.pg_maxlen);

	if(dk_cmd(dk, sense_cmd, SC_CLASS0_SZ, DKSC_TIMEOUT,
		(u_char *)msd, sense_cmd[4], B_READ))
		goto failure;
	if(msd->wprot)
		dk->dk_flags |= DK_WRTPROT;
	else
		dk->dk_flags &= ~DK_WRTPROT;
	if(msd->bd_len && (msd->block_descrip[5] ||
		msd->block_descrip[6] || msd->block_descrip[7]))
		dk->dk_blksz = ((unsigned)msd->block_descrip[5]<<16) +
			((unsigned)msd->block_descrip[6]<<8) + msd->block_descrip[7];
	else {	/* see if format page is there */
		u_char *d = (u_char *)msd, *maxd = d + msd->sense_len;
		d += msd->bd_len + 4;	/* after setting maxd! */
		while(d < maxd) {
			int i;
			if(((*d) & ALL) == DEV_FORMAT) {
				dk->dk_blksz =
					(((uint)((struct dev_format *)d)->f_bytes_sec[0])<<8) +
						((struct dev_format *)d)->f_bytes_sec[1];
				break;
			}
			i = d[1] + 2;	/* skip to next page; +2 for hdr */
			d += i;
		}
	}
failure:
	if(!dk->dk_blksz)
		dk->dk_blksz =  BBSIZE;	/* best guess... */
	if(msd)
		dmabuf_free(msd);
}


/* try to set the blksz.  Used on EFS opens, so that we can use
 * pretty much any reasonable CD-ROM to boot from.
 * does only the 4 byte header + 1 blkdescr, so don't need to
 * worry about which pages are supported.
 * Doesn't use the saved parameter page, so it is *NOT* saved,
 * (even for devices that support saving); main use is intended
 * for CD-ROM, which doesn't support saved pages. 
 * returns old blksz, for callers who want to restore it later.
*/
static int
dk_setblksz(register struct dksoftc *dk, int blksz)
{
	/* use sense struct because it has the blkdesc in it */
	struct mode_sense_data *msl;
	/* 4 byte header + 1 blkdesc;  PF, not SP */
	u_char select_cmd[SC_CLASS0_SZ] = {0x15, 0x10, 0, 0, 12 };
	int obsz = dk->dk_blksz;

	if(obsz == blksz)
		return blksz;

	if(!(msl = (struct mode_sense_data *) dmabuf_malloc(sizeof (*msl))))
		return 0;
	bzero(msl, sizeof(*msl));
	msl->bd_len = sizeof(msl->block_descrip);	/* 1 descrip */
	msl->block_descrip[5] = (u_char)(blksz >> 16);
	msl->block_descrip[6] = (u_char)(blksz >> 8);
	msl->block_descrip[7] = (u_char)blksz;

	(void)dk_cmd(dk, select_cmd, SC_CLASS0_SZ, DKSC_TIMEOUT, 
		(u_char *)msl, select_cmd[4], B_WRITE);
	dmabuf_free(msl);
	dk_getblksz(dk);	/* check to be sure it worked */
	if(blksz != dk->dk_blksz)	/* failed somehow... (may be because
		device doesn't support changing, or requires a format) */
		return 0;
	return obsz;
}


#ifdef DEBUG
#ifdef VH_DEBUG
/* dump the vh struct in a readable way */
void
dump_vh(struct volume_header *vh, char *head)
{
    int i;
	struct volume_directory *vd;

    printf("volhdr struct (%s):\n", head);
    printf("vh: magic=%x root=%x swap=%x csum=%x, bfile=%s.\n",
		vh->vh_magic, vh->vh_rootpt, vh->vh_swappt, vh->vh_csum, vh->vh_bootfile);
	for (vd = vh->vh_vd; vd < &vh->vh_vd[NVDIR]; vd++)
		if (vd->vd_nbytes && vd->vd_lbn != -1)
			printf("%-8.8s %-3d blks,  ", vd->vd_name, vd->vd_nbytes/BBSIZE);
	printf("\n");
    for(i=0; i<NPARTAB; i++)
	    if(vh->vh_pt[i].pt_nblks > 0)
		    printf("pnum %x: %x blks @ lbn=%x type=%x\n",
		      i, vh->vh_pt[i].pt_nblks,
		      vh->vh_pt[i].pt_firstlbn, vh->vh_pt[i].pt_type);
    printf("params: skew=%x spares/cyl=%x cyls=%x,%x hd0=%x trks0=%x secs=%x secbyte=%x ileave=%x\n",
	vh->vh_dp.dp_skew, vh->vh_dp.dp_spares_cyl, vh->vh_dp.dp_cylshi,
	vh->vh_dp.dp_cyls, vh->vh_dp.dp_shd0, vh->vh_dp.dp_trks0, vh->vh_dp.dp_secs,
	vh->vh_dp.dp_secbytes, vh->vh_dp.dp_interleave);
	printf("csum=%x, vhcsum=%x\n", vh->vh_csum, vh_checksum((int *)vh, sizeof *vh));
}
#endif /* VH_DEBUG */


/*	Dump the interesting parts of the dk structure for the requested drive.
	Use printf so output appears on correct screen when using debugger
	in the textport.
	If idbg not used, then no output, but that is a silly configuration!
*/
static void
_dk_pr(register struct dksoftc *dk, int unit)
{
	register i;
	int any;

	if(!dk)
		return;
	printf("dk%d, &dk %x, &lock %x, last cmd: ", unit, dk, &dk->dk_lock);
	for(i=0; i<sizeof(dk->dk_cmd); i++)
		printf("%x ", dk->dk_cmd[i]);
	printf("\n&vh %x, retry %d, &dk_wait %x, &dk_tab %x, &dk_buf %x\n",
		&dk->dk_vh, dk->dk_retry, &dk->dk_wait, &dk->dk_tab,
		&dk->dk_buf);
	printf("%u blocks, end blksz %u, blksz %u, ", dk->dk_drivecap[0],
		dk->dk_drivecap[1], dk->dk_blksz);
	printf("schan %x\n&sdata %x, ", dk->dk_subchan, dk->dk_sense_data);
	printf("opentyps: ");
	for(i=0; i<OTYPCNT; i++)
		printf("%u ", dk->dk_openparts[i]);
	printf("  ");
	any = 0;
	for(i=0; i<DK_MAX_PART; i++)
		if(dk->dk_lyrcnt[i]) {
			printf("lyr[%d] %x, ", i, dk->dk_lyrcnt[i]);
			any++;
		}
	if(any)
		printf("\n");

	for(i=0;i<NUM_BLK_STATS;i++)
		if(dk->dk_blkstats.bk_reads[i])
			printf("R%d = %u  ", (1<<i) * dk->dk_blksz,
				dk->dk_blkstats.bk_reads[i]);
	printf("\n");
	for(i=0;i<NUM_BLK_STATS;i++)
		if(dk->dk_blkstats.bk_writes[i])
			printf("W%d = %u  ", (1<<i) * dk->dk_blksz,
				dk->dk_blkstats.bk_writes[i]);
	printf("\n");
}

void
dkpr(unsigned adap, unsigned unit)
{
	if(unit)
		_dk_pr(dev_to_softc(adap&7, unit&7, 0), unit&7);
	else {	/* ignore adap arg if they want all units */
		for(adap=0; adap<scsicnt; adap++)
			for(unit=0; unit<SC_MAXTARG; unit++)
				_dk_pr(dev_to_softc(adap, unit, 0), unit);
	}
}

#if defined(SN0)
static void
dump_buf(char *buf, char *msg, int cnt)
{
	int i ;
	char *d = bp->b_dmaaddr ;

	printf(msg) ;

	for (i=0;i<cnt;i++)
        	printf("%x ", d[i]) ;
	printf("\n") ;
}
#endif
#endif /* DEBUG */
