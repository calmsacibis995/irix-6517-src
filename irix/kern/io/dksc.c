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

#ident "io/dksc.c $Revision: 3.302 $"

/*
 * dksc.c -- Scsi disk device driver
 * This driver talks to the scsi interface driver scsi.c.
 */
#define SAMMY 0
#define SAVESTAT 0
	/*  should fix stats to use high res counters! */
#define DKSCDEBUG 0

#include <sys/types.h>
#include <bstring.h>
#include <stdarg.h>
#include <sys/param.h>
#include <sys/immu.h>
#include <sys/kabi.h>
#include <sys/kmem.h>
#include <sys/sema.h>
#include <sys/systm.h>
#include <sys/sysmacros.h>
#include <sys/sbd.h>
#include <sys/signal.h>
#include <sys/buf.h>
#include <sys/iobuf.h>
#include <sys/elog.h>
#include <sys/scsi.h>
#include <sys/failover.h>
#include <sys/dvh.h>
#include <sys/ioctl.h>
#include <sys/dkio.h>
#include <sys/debug.h>
#include <sys/dump.h>
#include <sys/dksc.h>
#include <sys/errno.h>
#include <sys/cmn_err.h>
#include <sys/invent.h>
#include <sys/cred.h>
#include <sys/uio.h>
#include <sys/conf.h>
#include <sys/ddi.h>
#include <sys/capability.h>
#include <sys/par.h>
#include <sys/pda.h>
#include <sys/rtmon.h>
#include <sys/iograph.h>
#include <sys/grio.h>
#include <sys/alenlist.h>
#include <sys/lio.h>

#if SAVESTAT 
#include <sys/idbg.h>
#include <sys/idbgentry.h>
#endif

#ifdef DEBUG
int dkdebug = 0;
#endif

#define	DKSCLOCK(p) mutex_lock(p, PZERO)
#define DKSCUNLOCK(p)	mutex_unlock(p)

#if SAVESTAT
int dkseq = 0, dktot = 0;
struct dkstat {
	uint		bn;
	dev_t	dev;
	uint64_t flags;	/* b_flags */
	unsigned short	count;
	unsigned queue;
	unsigned start;
	unsigned finish;
};
#define DKSEARCH 12
#define DKSTATSIZE (1<<DKSEARCH)
unsigned dkthresh = 25;
unsigned dk_bytesper10ms = 37000;
struct dkstat dkbuffer[DKSTATSIZE];
unsigned dkloc = 0;
unsigned statadap = 0;
unsigned stattarg = 1;	/* ignore this target; typically where SYSLOG lives */
#endif

int dkscdevflag = D_MP;

/* len must be initialized the way dkscopen uses it so PROM will work;
 * all other users copy and modify the copy, setting the len as needed */
static u_char dk_read[]	= {0x28, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static u_char dk_write[]	= {0x2a, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static u_char dk_readcapacity[] = {0x25, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static u_char dk_rdefects[]	= {0x37, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static u_char dk_add_defects[]	= {0x7, 0, 0, 0, 0, 0};
static u_char dk_tst_unit_rdy[] = {0, 0, 0, 0, 0, 0};
static u_char dk_send_diag[]	= {0x1d, 4, 0, 0, 0,    0};
static u_char dk_mode_sense[]	= {0x1a, 0, 0, 0, 0,	0};
static u_char dk_mode_select[]	= {0x15, 0, 0, 0, 0,	0};	/* 2nd byte can
	be set by DIOCSETFLAGS */
#ifdef notused
static u_char dk_prevrem[]	= {0x1e, 0, 0, 0, 0,	0};
#endif
static u_char dk_format[]	= {0x4, 0x18, 0, 0, 0,	0};
static u_char dk_startunit[]	= {0x1B,  0, 0, 0, 1,	0};

#ifdef NOTDEF
static u_char dk_erase[]	= {0x2c, 0, 0, 0, 0, 0, 0, 0, 0, 0};
#endif

#if defined(INDUCE_IO_ERROR)
/* 
 * To induce I/O error, set insert_error to 1, and error_dev to the dev_t.
 * This is mainly used for XFS filesystem error handling testing.
 */
int	dksc_insert_error = 0;	/* whether to enable fault insertion. */
dev_t	dksc_error_dev = 0;	/* device to fail */
int	dksc_error_freq = 20;	/* frequency at which errors are induced */

int	dksc_error_count = 0;	/* counter to determine which request to
				   fail (if xlv_insert_error is 1). */
#endif

/*
 * The basic timeout for the disk driver.  Note that it is long to handle
 * many units active at same time, with possible disconnects during commands.
 */
#define DKSC_TIMEOUT		(60 * HZ)

/*
 * The short timeout is used when the disk device is marked DK_NO_RETRY.
 */
#define DKSC_SHORT_TIMEOUT	(10 * HZ)

/* Maximum number of retries on a failed request */
#define DK_MAX_RETRY	15
/* retry on BUSY long enough to allow up to 4 minutes; same as startunit */
#define DK_MAX_BUSY	16
#define DK_MAX_BUSY_INTV	16

/* Size of a physical disk block (default, if can't get) */
#define DK_BLK_SZ	BBSIZE

/*
 * The default max queue per disk
 */
#define DFMAXQ                  1

/*
 * This word serves as a flag to identify when the dksc driver has been
 * put into dump mode by a call to dkscdump(). Dump mode is for panic() dumping.
 * Once in dump mode, the flag identifies the device that the dump is to.
 */
dev_t dksc_dumpdev = 0;


/* function prototypes */
#ifdef DEBUG
void dump_vh(struct volume_header *, char *);
#endif
static void  dk_intr(scsi_request_t *);
static int   dk_chkcond(struct dksoftc *, scsi_request_t *, int, int);
static void  dk_do_retry(scsi_request_t *);
static int   dk_readcap(struct dksoftc *);
static int   dk_chkptsize(struct dksoftc *dk, struct volume_header *);
static int   dk_rw(struct dksoftc *dk, char *addr, uint32_t block_num, int block_len,
                   char *cmd_buffer, int);
static int   dk_cmd(struct dksoftc *dk, u_char *cmd_buffer, int cmd_size,
                    u_int timeoutval, caddr_t addr, size_t len, int rw);
static void  dk_checkqueue(struct dksoftc *,uint);
static int   dk_getblksz(struct dksoftc *dk);
static void  dkscstart(struct dksoftc *dk, buf_t *bp, scsi_request_t *sp);
static scsi_request_t *drive_acquire(struct dksoftc *dk, int);
static void  drive_release(struct dksoftc *dk, scsi_request_t *sp);
static void  dkrelsubchan(struct dksoftc *dk, scsi_request_t *sp);
static void  dk_print_sense(struct dksoftc *dk, struct scsi_request *sp, dev_t dev);
void	     dk_unalloc_softc(struct dksoftc *dk);
int          dk_alloc_ioreq(struct dksoftc *, vertex_hdl_t disk_vhdl,int newqueue);
void         dk_callback_frontend(vertex_hdl_t lun_vhdl, char *sense);
static int   dk_callback_backend(register struct dksoftc *dk, int wait);
static void  dk_callback_intr(scsi_request_t *);
static int   get_hw_scsi_info(dev_t, char *, char *, char *, char *, char *, char *);
void         dk_alias_make(dev_t);
int          dk_user_printable_sense(struct dksoftc *, register scsi_request_t *, int);

#define NUMOFCHARPERINT	10

#if _MIPS_SIM == _ABI64
static void  irix5_dkioctl_to_dkioctl(struct irix5_dk_ioctl_data *, 
					struct dk_ioctl_data *);
#endif /* _ABI64 */
static void
/*VARARGS2*/
dk_pmsg(dev_t, char *, ...);

/* OK; so I should just declare ctstrat the way uiophysio prototypes
 * the function argument, but I feel that is simply the wrong way
 * to do it, since the function is in fact void, and the *physio
 * functions are documented as ignoring the return value.  Stupid
 * SVR4 declarations... */
typedef int (*physiofunc)(buf_t *);
void dkscstrategy(buf_t *);

#ifdef EVEREST
extern int dma_usertophys(struct buf *);
extern void dma_userfree(struct buf *);
#endif


/* dksc initialization routine */
void
dkscinit()
{
	static int dkscinit_done;

	if (dkscinit_done)
		return;
	dkscinit_done++;
}


/*
 * allocate the various dynamic data, initialize locks and semaphores, etc.
 */
struct dksoftc *
dk_alloc_softc(vertex_hdl_t disk_vhdl)
{
	struct dksoftc 		*dk;
	register 		i;
	u_char 			*cmddata;
	scsi_request_t 		*sp;
	scsi_disk_info_t	*disk_info;


	/* SC_CLASS1_SZ has to be the longest cmd len we use in the driver */
	ASSERT((sizeof(scsi_request_t) % 4) == 0);
	dk = (struct dksoftc *) kern_calloc(sizeof(struct dksoftc) +
		DFMAXQ * sizeof(scsi_request_t) +
		roundup(DFMAXQ * SC_CLASS1_SZ, 4), 1);

	dk->dk_freereq = dk->dk_subchan = sp =
				(scsi_request_t *) (&dk[1]);
	cmddata = (u_char *) &dk->dk_subchan[DFMAXQ];
	for(i = 0; i < DFMAXQ; i++) {
		sp[i].sr_dev = &sp[i+1];
		sp[i].sr_sense = kmem_alloc(SCSI_SENSE_LEN, KM_CACHEALIGN);
		sp[i].sr_senselen = SCSI_SENSE_LEN;
		sp[i].sr_command = cmddata;
		/* for interrupt routines */
		cmddata += SC_CLASS1_SZ;
	}
	sp[DFMAXQ - 1].sr_dev = NULL;
	dk->dk_ioreq = dk->dk_qdepth = DFMAXQ;

	/*
	 * Even though we only need a couple bytes, allocate them dynamically
	 * for proper cache-line alignment.  This is important for all DMA
	 * buffers on machines with a writeback cache (IP17).
	 */
	dk->dk_drivecap = kmem_alloc(DK_DRIVECAP_SIZE, KM_CACHEALIGN);
#ifdef DEBUG
	dk->dk_blkstats = kmem_alloc(sizeof(*dk->dk_blkstats), 0);
#endif

	disk_info 	= scsi_disk_info_get(disk_vhdl);
	dk->dk_disk_vhdl= disk_vhdl;
	dk->dk_lun_vhdl = SDI_LUN_VHDL(disk_info);
	i 		= disk_vhdl;

	init_mutex(&dk->dk_lock, MUTEX_DEFAULT,"dklock", i);
	init_sv(&dk->dk_wait, SV_DEFAULT,"dkwait",i);
	init_mutex(&dk->dk_sema, MUTEX_DEFAULT,"dksema",i);
	init_sema(&dk->dk_done, 0,"dkdone",i);
	init_sv(&dk->dk_hold, SV_DEFAULT,"dkhold",i);
	subchaninit(disk_vhdl,dk->dk_subchan, DFMAXQ);

	/*
	 * add the pointers to the driver's state & iotime structures
	 * as labelled info associated with this particular one's 
	 * vertex in the hardware graph
	 */
	SDI_DKSOFTC(disk_info) = dk;
	return(dk);
}


void
dk_unalloc_softc(struct dksoftc *dk)
{
	scsi_request_t *sp = dk->dk_subchan;
	uint i;
	
	kmem_free(dk->dk_drivecap, DK_DRIVECAP_SIZE);
#ifdef DEBUG
	kmem_free(dk->dk_blkstats, sizeof(*dk->dk_blkstats));
#endif
	
	for (i=0; i<DFMAXQ; i++)
		kmem_free(sp[i].sr_sense, sp[i].sr_senselen);

	sv_destroy(&dk->dk_wait);
	mutex_destroy(&dk->dk_lock);
	mutex_destroy(&dk->dk_sema);
	freesema(&dk->dk_done);
	sv_destroy(&dk->dk_hold);
	kern_free(dk);
}

void
subchaninit(vertex_hdl_t 	disk_vhdl, 
	    scsi_request_t 	*sp,
	    uint 		numsp)
{
	int 			s;
	scsi_disk_info_t	*disk_info;
	disk_info 		= scsi_disk_info_get(disk_vhdl);

        for(s = 0; s < numsp; s++, sp++) {
		sp->sr_lun_vhdl	= SDI_LUN_VHDL(disk_info);
		sp->sr_dev_vhdl	= disk_vhdl;
		sp->sr_ctlr		= SDI_ADAP(disk_info);
                sp->sr_target 		= SDI_TARG(disk_info);
                sp->sr_lun    		= SDI_LUN(disk_info);
        }
}


/*
 * modify the name space of the dksc hardware graph. this happens whenever
 * the in-memory volume header information is modified (i.e whenever the
 * volume header is read from the disk)
 */
void
dksc_disk_part_hwg_update(scsi_part_info_t	*part_info,
			  struct volume_header 	*vol_hdr)
{
	u_char 			part;
	vertex_hdl_t 		disk_vhdl, p_vhdl;
	
	ASSERT(vol_hdr);
	
	disk_vhdl = SPI_DISK_VHDL(part_info);


	/* 
	 * go through each entry of the partition table of the volume header
	 */
	for(part = 0; part < DK_MAX_PART; part++) {

		/* 
		 * check if the vertex for this partition already exists in the 
		 * hardware graph
		 */
		if (!scsi_part_vertex_exist(disk_vhdl,part,&p_vhdl)) {
			
			/*
			 * vertex for this partition doesnot exist
			 */
			
			if (vol_hdr->vh_pt[part].pt_nblks > 0 &&
			    vol_hdr->vh_pt[part].pt_firstlbn >= 0)
			{
				/* 
				 * create the vertex for this partition as it was
				 * not found in the graph 
				 */
				
				(void)scsi_part_vertex_add(disk_vhdl,
							   part, B_TRUE);
				
			}
		} else {
			
			/* 
			 * vertex for this partition has already been 
			 * created
			 */
			
			if ((vol_hdr->vh_pt[part].pt_nblks <= 0 ||
			     vol_hdr->vh_pt[part].pt_firstlbn < 0) &&
			    (part != SCSI_DISK_VOL_PARTITION &&
			     part != SCSI_DISK_VH_PARTITION))
			{
				/* 
				 * remove the vertex and the incoming edge for it
				 * as this partition no longer exists in the 
				 * volume header
				 */
				scsi_part_vertex_remove(disk_vhdl,part);
				
			}
		}
	}
}


/*
 * Check for any exisiting partitions that would be deleted, and ask
 * specfs to see if that partition is "in-use".
 */
int
dksc_disk_part_hwg_check(scsi_part_info_t	*part_info,
			  struct volume_header 	*vol_hdr)
{
	extern u_char	miniroot;

	u_char 			part;
	vertex_hdl_t 		disk_vhdl, p_vhdl, b_vhdl, c_vhdl;
	

	/*
	 * In-use partition deletion is not checked
	 * if we're a "miniroot" kernel.  There are
	 * times where it can be done.
	 */
	if (miniroot)
		return 0;

	ASSERT(vol_hdr);
	
	disk_vhdl = SPI_DISK_VHDL(part_info);


	/* 
	 * go through each entry of the partition table of the volume header
	 */
	for(part = 0; part < DK_MAX_PART; part++) {

		/* 
		 * check if the vertex for this partition already exists in the 
		 * hardware graph
		 */
		if (scsi_part_vertex_exist(disk_vhdl, part, &p_vhdl)) {

			/* 
			 * a vertex for this partition already exists;
			 * check the "new" vh to see if it would be deleted.
			 */
			
			if (   (   vol_hdr->vh_pt[part].pt_nblks    <= 0
			        || vol_hdr->vh_pt[part].pt_firstlbn <  0)
			    && (   part != SCSI_DISK_VOL_PARTITION
				&& part != SCSI_DISK_VH_PARTITION) )
			{
				extern int spec_dev_is_inuse(dev_t dev);


				/* 
				 * Ask specfs to see if the VBLK/VCHR vertex
				 * is in-use by a file system, or XLV, in
				 * which case deleting it would be dangerous.
				 */
				b_vhdl = hwgraph_block_device_get(p_vhdl);

				if (spec_dev_is_inuse(vhdl_to_dev(b_vhdl)))
					return EBUSY;

				c_vhdl = hwgraph_char_device_get(p_vhdl);

				if (spec_dev_is_inuse(vhdl_to_dev(c_vhdl)))
					return EBUSY;
			}
		}
	}

	return 0;
}


/* ARGSUSED */
dkscopen(dev_t *devp, int flag, int otyp, cred_t *crp)
{
	register struct dksoftc	*dk;
	u_int			part;
	caddr_t 		vol_hdr;
	mutex_t			*dksema;
	int 			error = 0, tmp, alloced = 0, scsialloced = 0;
	int			need_getblksz = 0;
	int			cmd_qdpth;
	int			already_open = 0;
	struct scsi_target_info	*info;

	scsi_part_info_t	*part_info;
	scsi_disk_info_t	*disk_info;
	vertex_hdl_t 		lun_vhdl,disk_vhdl;
	char			contr[NUMOFCHARPERINT];
	/* REFERENCED */
	graph_error_t		rv;
	vertex_hdl_t		dev_vhdl;

	if (!dev_is_vertex(*devp))
		return ENXIO;

	dev_vhdl	= *devp;
	part_info 	= scsi_part_info_get(dev_vhdl);
	part		= SPI_PART(part_info);
	disk_info	= SPI_DISK_INFO(part_info);
	lun_vhdl	= SDI_LUN_VHDL(disk_info);
	disk_vhdl 	= SDI_DISK_VHDL(disk_info);

	/*
	 * Single thread device functions like open, certain ioctl, etc.
	 */
	dksema = &(SDI_DKSEMA(disk_info));
	mutex_lock(dksema, PRIBIO);

	if (SDI_DKSOFTC(disk_info) == NULL)
	{ /* First open */
		dk = dk_alloc_softc(disk_vhdl);
		alloced = 1;
		dk->dk_selflags = 0x11;	/* default to PF and SP; */
		dk->dk_buf.b_edev = *devp;	/* used only for error messages */
		need_getblksz = 1;
	}
	else 
		dk = SDI_DKSOFTC(disk_info);

	info = SDI_INQ(disk_info)(lun_vhdl);
	if(info == SCSIDRIVER_NULL) {
		error = ENODEV;	/* shouldn't happen! */
#ifdef DEBUG
		dk_pmsg(*devp, "scsi_info failed, driver=%d, ad=%d \n",
			dk->dk_driver, lun_vhdl);
#endif
		goto done;
	}

	DKSCLOCK(&dk->dk_lock);

	/* used here, and also in reqsense handler */
	dk->dk_inqtyp = info->si_inq;
	dk->dk_flags &= ~DK_WONT_START;
#if defined(EVEREST) || defined(SN) || defined(IP30)
	if (info->si_ha_status & SRH_MAPUSER)
		dk->dk_flags |= DK_MAPUSER;
	if (info->si_ha_status & SRH_ALENLIST)
		dk->dk_flags |= DK_ALENLIST;
#endif
	DKSCUNLOCK(&dk->dk_lock);

	/*	should be direct access.  Removable media OK. 
		Look at all but the vendor specific bit; clearly
		if the controller doesn't support the lun we want,
		or it isn't attached, then we want to fail right here. */
	switch(dk->dk_inqtyp[0] & 0x1f) {
	case 0:	/* No define for disk... */
	case INV_WORM:
	case INV_CDROM:
	case INV_OPTICAL:
		break;
	default:
		dk_pmsg(*devp, "not a disk drive, device type == 0x%x\n",
			dk->dk_inqtyp[0] & 0x1f);
		error = ENODEV;
		goto done;
	}

	for (tmp = 0; tmp < OTYPCNT; tmp++) {
		if (dk->dk_openparts[tmp] != 0)
			already_open = 1;
	}
	if (!already_open) {
		/*
		 * Only add a sense callback for CDROM devices.
		 */ 
		if (cdrom_inquiry_test(&dk->dk_inqtyp[8]))
			dk->dk_cb = dk_callback_frontend;
		else
			dk->dk_cb = NULL;

		if ((tmp= SDI_ALLOC(disk_info)(lun_vhdl, dk->dk_qdepth, dk->dk_cb)) != SCSIALLOCOK)
		{
#ifdef DEBUG
			dk_pmsg(*devp, "scsi_alloc failed\n");
#endif
			error = tmp ? tmp : ENODEV;
			goto done;
		}
		scsialloced = 1;
	}
	
	/* Check to see if the drive is ready for IO */
	if (dk_cmd(dk, dk_tst_unit_rdy, SC_CLASS0_SZ, DKSC_TIMEOUT, NULL, 0, 0)) {
		error = EIO;
		goto done; /* Unmap since we couldn't open it */
	}
		
	/* If this is the first open, we need to read the blocksize of the device
         * Removeable media device should check on every open, in case it has
         * changed since the last open (via /dev/scsi, for example).
         */
	if (need_getblksz || (dk->dk_inqtyp[1] & 0x80)/* removable media */) {
		dk_getblksz(dk);
	}

	/* Read the volume header (1 block) */
	vol_hdr = kmem_alloc(dk->dk_blksz, KM_CACHEALIGN);

	if (dk_rw(dk, vol_hdr, 0, 1, (caddr_t) dk_read, 1)) {
		/*
		 * If we can't even read the vh sector, they are probably in
		 * trouble, but it's always possible a subsequent write might
		 * fix the problem, so simply mark the vh as invalid so no I/O
		 * can be done until a SETVH has been done.
		 * If the volume header was previously valid, and the device is
		 * not removeable media, then don't clear vh_magic, because the
		 * volume header read failure may be transient, or due to a bad
		 * volume header block.  We want to allow other users of the disk
		 * as many chances as possible to access their data.  If the
		 * device is removeable media, then media may have been removed,
		 * so we'll zero vhmagic.
		 */
#ifdef DEBUG
		dk_pmsg(*devp, "unable to read volume header\n");
#endif
		if ((dk->dk_inqtyp[1] & 0x80 /* removable media */) ||
		    (dk->dk_vh.vh_magic != VHMAGIC)) {
			dk->dk_vh.vh_magic = 0;
		}
		if (get_hw_scsi_info(*devp, contr, NULL, NULL, NULL, NULL, NULL) != -1)
			dk_alias_make(*devp);
	}
	else if (!is_vh((struct volume_header*)vol_hdr)) {
		__uint64_t	byte_cap;
#ifdef DEBUG
		dk_pmsg(*devp, "volume header not valid\n");
		dump_vh((struct volume_header *) vol_hdr, "on open");
#endif
		/* if there is no volhdr on the drive, and we don't 
		 * already have one in memory, or we have one in memory, but the
		 * capacity is different, invalidate the one in
		 * the dk struct as above.   Otherwise, if the one in memory
		 * is already valid, leave it that way; that allows people
		 * to do a DIOCSETVH, and use foreign media without having 
		 * a subsequent attempt to open invalidating it, as long as
		 * the capacity continues to match.
		 */
		byte_cap = (__uint64_t) dk_readcap(dk) * dk->dk_blksz;
		if  ((dk->dk_vh.vh_magic != VHMAGIC) ||
 		     (byte_cap != dk->dk_bytecap)) {
		    dk->dk_vh.vh_magic = 0;
		}
		if (get_hw_scsi_info(*devp, contr, NULL, NULL, NULL, NULL, NULL) != -1)
			dk_alias_make(*devp);
	} else {
		bcopy(vol_hdr, &dk->dk_vh, sizeof(struct volume_header));

		/*
		 * Use what's there even if partition sizes are larger than
		 * drive, but mark the size down in the incore copy (which is
		 * returned by GETVH)
		 */
		(void)dk_chkptsize(dk, (struct volume_header *)0);

		/*
		 * in-memory volume header has changed. make the relevant
		 * changes to the hardware graph
		 */
		dksc_disk_part_hwg_update(part_info, (struct volume_header *) vol_hdr);
	}
	kern_free(vol_hdr);
	
	/*
	 * The vh_magic check allows us to open a drive with either no
	 * partition table or a bad one more than once, which gives us a
	 * chance to do a SETVH even if not the very first open.
	 */
	if (dk->dk_vh.vh_magic && dk->dk_vh.vh_pt[part].pt_nblks == 0) {
#ifdef DEBUG
		dk_pmsg(*devp, "invalid partition, part = %d\n", part);
#endif
		error = ENXIO;
		goto done;
	}

	/*
	 * If queue parameters have been changed, check for validity and adjust
	 * if necessary.
	 */
	if (dk->dk_vh.vh_dp.dp_flags & DP_CTQ_EN) {
		cmd_qdpth = dk->dk_vh.vh_dp.dp_ctq_depth ? dk->dk_vh.vh_dp.dp_ctq_depth : 1;
		tmp = DK_QUEUE;
	}
	else {
		cmd_qdpth = 1;
		tmp = 0;
	}
	if (tmp != (dk->dk_flags & DK_QUEUE) || cmd_qdpth != dk->dk_qdepth) {
		if (dk->dk_vh.vh_dp.dp_flags & DP_CTQ_EN)
			dk_checkqueue(dk, info->si_ha_status);
		else
			dk->dk_flags &= ~DK_QUEUE;

		if (dk->dk_flags & DK_QUEUE)
		{
			if (cmd_qdpth > dk->dk_ioreq &&
			    dk_alloc_ioreq(dk, disk_vhdl, cmd_qdpth) != SCSIALLOCOK)
			{
				cmd_qdpth = dk->dk_qdepth;
			}
		}
		else
			cmd_qdpth = 1;
		dk->dk_qdepth = cmd_qdpth;
	}

	if (otyp == OTYP_LYR)
		dk->dk_lyrcount[part]++;
	dk->dk_openparts[otyp] |= 1<<part;

done:
	if(error) {
		if(scsialloced) {
			dk->dk_openparts[otyp] &= ~(1<<part);
			/*
		 	 * NOTE:
		 	 * need to add the callback function, whene all the
		 	 * HBA drivers will have HW-graph support.
		 	 */ 
			SDI_FREE(disk_info)(lun_vhdl, dk->dk_cb);
		}
		if(alloced) {
			/* we do this to free up memory, etc., allocated on attempts
			 * to open non-disk devices.  Don't care one way or the other
			 * for 'real' disks, as we will reallocate anyway, in almost
			 * all cases; we don't free up the base dksoftc allocations,
			 * as that involves too much bookkeeping. */
			dk_unalloc_softc(dk);
			SDI_DKSOFTC(disk_info) = NULL;
		}
	}
	mutex_unlock(dksema);

	return error;
}

/*
 * The close routine
 * We don't deallocate the softc structure on dksc close.  We could
 * keep reference counts and free when necessary, but it seems unnecessary.
 * 
 * no reason to free up anything any more, since multiple upper
 * layer devices can all have the same channel open; unfortunately,
 * in order for the exclusive open stuff to work right, we still
 * have to handle the scsi_free[] stuff.
 */
/* ARGSUSED */
dkscclose(dev_t dev, int flag, int otyp, cred_t *crp)
{
	struct dksoftc		*dk;
	mutex_t			*dksema;
	int			tmp;
	int			still_open=0;
	scsi_disk_info_t	*disk_info;
	scsi_part_info_t	*part_info;

	uchar_t 		part;
	vertex_hdl_t		lun_vhdl;
	vertex_hdl_t		dev_vhdl;

	dev_vhdl	= dev;
	part_info 	= scsi_part_info_get(dev_vhdl);
	if (part_info == NULL)		/* XXXjtk - TEMPORARY hack	*/
		return ENODEV;		/* ref. bug #523240		*/

	part		= SPI_PART(part_info);
	disk_info	= SPI_DISK_INFO(part_info);
	if (disk_info == NULL)		/* XXXjtk - TEMPORARY hack	*/
		return ENODEV;		/* ref. bug #523240		*/

	lun_vhdl	= SDI_LUN_VHDL(disk_info);
	if (lun_vhdl == NULL)		/* XXXjtk - TEMPORARY hack	*/
		return ENODEV;		/* ref. bug #523240		*/

	dksema = &(SDI_DKSEMA(disk_info));

	mutex_lock(dksema, PRIBIO);
	/* be paranoid */

	if (!disk_info || !(SDI_DKSOFTC(disk_info))) {
		mutex_unlock(dksema);
		return EIO;
	}
	dk = SDI_DKSOFTC(disk_info);

	if (otyp == OTYP_LYR) {
		dk->dk_lyrcount[part]--;
		ASSERT(dk->dk_lyrcount[part] >= 0);
		if (dk->dk_lyrcount[part] == 0)
			dk->dk_openparts[OTYP_LYR] &= ~(1<<part);
	}
	else
		dk->dk_openparts[otyp] &= ~(1<<part);

	for (tmp = 0; tmp < OTYPCNT; tmp++) {
		if (dk->dk_openparts[tmp] != 0)
			still_open = 1;
	}
	if (!still_open)
		/*
		 * NOTE:
		 * need to add the callback function, whene all the
		 * HBA drivers will have HW-graph support.
		 */ 
		(SDI_FREE(disk_info))(lun_vhdl, dk->dk_cb);

	mutex_unlock(dksema);
	return 0;
}


/*	dkscioctl -- io controls.  Note that because of the
	ioctl_data stuff, there must be only one return from
	this routine.
*/
/* ARGSUSED */
dkscioctl( dev_t dev, unsigned int cmd, caddr_t arg, int mode,
	cred_t *crp, int *rvalp)
{
	caddr_t ioctl_data;
	int flag;
	int error = 0;
	mutex_t *dksema;
#if _MIPS_SIM == _ABI64
	int convert_abi = !ABI_IS_IRIX5_64(get_current_abi());
#endif /* _ABI64 */
	scsi_disk_info_t		*disk_info;
	scsi_part_info_t		*part_info;

	struct dksoftc			*dk;
	vertex_hdl_t			lun_vhdl;
	vertex_hdl_t			dev_vhdl;
	
	dev_vhdl	= dev;
	part_info 	= scsi_part_info_get(dev_vhdl);
	if (part_info == NULL)		/* XXXjtk - TEMPORARY hack	*/
		return ENODEV;		/* ref. bug #523240		*/

	disk_info	= SPI_DISK_INFO(part_info);
	if (disk_info == NULL)		/* XXXjtk - TEMPORARY hack	*/
		return ENODEV;		/* ref. bug #523240		*/

	lun_vhdl	= SDI_LUN_VHDL(disk_info);
	if (lun_vhdl == NULL)		/* XXXjtk - TEMPORARY hack	*/
		return ENODEV;		/* ref. bug #523240		*/

	dk		= SDI_DKSOFTC(disk_info);
	if (dk == NULL)			/* XXXjtk - TEMPORARY hack	*/
		return ENODEV;		/* ref. bug #523240		*/

	dksema = &(SDI_DKSEMA(disk_info));

	switch (cmd) {
	case DIOCFOUPDATE:
	case DIOCSELECT:
	case DIOCADDBB:
	case DIOCFORMAT:
		/*
		 * only MGR can modify drive characteristics,
		 * spare blocks, or format drives
		 */
		if(!_CAP_CRABLE(crp, CAP_DEVICE_MGT)) {
			return EPERM;
		}
		/* fall through */
	case DIOCSENSE:
	case DIOCSETVH:
	case DIOCSETVH | DIOCFORCE:
		ioctl_data = kmem_alloc(MAX_IOCTL_DATA, KM_CACHEALIGN); 
		break;
	default:
		ioctl_data = NULL;
		break;
	}

	switch (cmd) {
	case DIOCNORETRY:
	{
		DKSCLOCK(&dk->dk_lock);

		if( (__psunsigned_t)arg ) {
			dk->dk_flags |= DK_NO_RETRY;
		} else {
			dk->dk_flags &= ~DK_NO_RETRY;
		}

		DKSCUNLOCK(&dk->dk_lock);
		break;
	}

	case DIOCNOSORT:
	{
		DKSCLOCK(&dk->dk_lock);

		if( (__psunsigned_t)arg ) {
			dk->dk_flags |= DK_NO_SORT;
		} else {
			dk->dk_flags &= ~DK_NO_SORT;
		}

		DKSCUNLOCK(&dk->dk_lock);
		break;
	}

	case DIOCDRIVETYPE:
	{	/* actually do inquiry command each time, in case drive
		 * is changed, etc. (quite common during eval, but customers
		 * do it also) */
		struct scsi_target_info	*info;

		info = SDI_INQ(disk_info)(lun_vhdl);
		if(info == SCSIDRIVER_NULL)
		    goto ioerr;

	    if(gencopy(info->si_inq+8, arg, SCSI_DEVICE_NAME_SIZE, 0)) {
copyerr:
		    error = EFAULT;
		}
	    break;
	}
	case DIOCFOUPDATE:
	{
		user_fo_generic_info_t *fgi=(void *)ioctl_data;
		int i;

		if(gencopy(arg, ioctl_data, sizeof(dk->dk_vh), 0))
			goto copyerr;

		/*
		** Confirm that at least one of the lvh in the info
		** corresponds to this lvh.  If not, return EINVAL.
		*/

		for (i=0; i<MAX_FO_PATHS; i++) {
			if (fgi->fgi_lun_vhdl[i] == lun_vhdl) {
				/*
				** Call into the failover software.
				*/
				error = fo_scsi_device_update_generic (fgi);
				break;
			}
		}
		if (i == MAX_FO_PATHS) error = EINVAL;
		break;
	}
	case DIOCGETVH:
	{
		int rval;

		mutex_lock(dksema, PRIBIO);
		rval = gencopy(&dk->dk_vh, arg, sizeof(dk->dk_vh), 0);
		mutex_unlock(dksema);

		if (rval)
			goto copyerr;
		break;
	}

	case DIOCSETVH:
	case DIOCSETVH | DIOCFORCE:
		if (gencopy(arg, ioctl_data, sizeof(dk->dk_vh), 0))
			goto copyerr;

		mutex_lock(dksema, PRIBIO);

		if (is_vh((struct volume_header *)ioctl_data) &&
		    dk_chkptsize(dk, (struct volume_header *)ioctl_data))
		{
			error = 0;

			/*
			 * If this isn't being FORCE'd, make sure that
			 * any existing partitions that would be deleted
			 * aren't already in use by a file system.
			 */
			if (cmd == DIOCSETVH) {
				error = dksc_disk_part_hwg_check(part_info,
					    (struct volume_header *)ioctl_data);
			}

			if ( ! error) {
				/*
				 * Copy the "new" volume header over the
				 * existing one.
				 */
				bcopy(ioctl_data, (caddr_t)&dk->dk_vh,
							sizeof(dk->dk_vh));
				/*
				 * some old vh didn't have the sector size set,
				 * so keep what we have in that case
				 */
				if (dk->dk_vh.vh_dp.dp_secbytes)
					dk->dk_blksz =
						dk->dk_vh.vh_dp.dp_secbytes;
				/* 
				 * dksc_dev_mod might involve calls to
				 * hwgraph_vertex_destroy which wants the
				 * caller to insure that he is the only one
				 * accessing this vertex, which we are.
				 */
				dksc_disk_part_hwg_update(part_info,
					    (struct volume_header *)ioctl_data);
			}

		}
		else
			error = EINVAL;

		mutex_unlock(dksema);
		break;

	case DIOCTEST:
		if (dk_cmd(dk, dk_send_diag, SC_CLASS0_SZ,
			   120*HZ, (caddr_t)0, 0, 0))
ioerr:
			error = EIO;
		break;

	case DIOCFORMAT:
		/*
		 * Note we can have a problem if we get the bus while some
		 * other unit has temporarily disconnected, since some very old
		 * drives do NOT disconnect during a format.  We set a timeout
		 * of 3 hrs for capacity of < 5GB, 8 hours if we
		 * can't determine the capacity, otherwise the N*(3/4) hours where
		 * N is the (int) number of GB.  We assume the blocksize is 512 if
		 * we don't know it already; 9GB drives are at 3+ hrs, 18GB
		 * drives are at 6+ hours.  Hopefully this algorithm will give
		 * us enough margin, and keep us from having to change this
		 * ever again (yeah, right).
		 */
		mutex_lock(dksema, PRIBIO);

		flag = dk_readcap(dk);
		if(flag > 0) {
			uint64_t sz;
			sz = (dk->dk_bytecap)
				? dk->dk_bytecap
				: (uint64_t)flag * (uint64_t)(dk->dk_blksz > 0
							? dk->dk_blksz : DK_BLK_SZ);
			sz /= (1024ULL * 1024ULL * 1024ULL);
			flag = (sz<5ULL) ? 3*60*60*HZ : (int)sz*45*60*HZ;
		}
		else
			flag = 8*60*60*HZ;
				
		(*(int *) ioctl_data) = 0;

		if (dk_cmd(dk, dk_format, SC_CLASS0_SZ, flag,
			(caddr_t) ioctl_data, 4, B_WRITE))
			goto ioerr;
		/* check partition sizes again, since some drives don't
		 * return new readcap until after format. */
		(void)dk_chkptsize(dk, (struct volume_header *)0);

		mutex_unlock(dksema);
		break;

	case DIOCSENSE:
	{
		struct dk_ioctl_data sense_args;
		u_char sense_cmd[SC_CLASS0_SZ];
		u_int *sense_data = (u_int *) ioctl_data;
		u_int alloclen;

#if _MIPS_SIM == _ABI64
		if (convert_abi) {
			struct irix5_dk_ioctl_data i5_sense_args;

			error = gencopy(arg, &i5_sense_args, 
				sizeof(struct irix5_dk_ioctl_data),0);
			if (!error)
				irix5_dkioctl_to_dkioctl(&i5_sense_args, 
					&sense_args);
		} else
#endif /* _ABI64 */
			error = gencopy(arg, &sense_args, 
				sizeof(struct dk_ioctl_data), 0);
		if (error)
			goto copyerr;
		alloclen = sense_args.i_len;
		if (alloclen > MAX_IOCTL_DATA)
			goto invalerr;
		bcopy(dk_mode_sense, sense_cmd, sizeof(sense_cmd));
		sense_cmd[2] = sense_args.i_page;
		sense_cmd[4] = sense_args.i_len;
		if (dk_cmd(dk, sense_cmd, SC_CLASS0_SZ, DKSC_TIMEOUT,
			(caddr_t) sense_data, alloclen, B_READ))
			goto ioerr;
		if (gencopy(sense_data, sense_args.i_addr, sense_args.i_len, 0))
			goto copyerr;
		break;
	}

	case DIOCSELFLAGS:
		if((__psunsigned_t)arg & 0xe0)	/* can't use to set LUN! */
			error = EINVAL;
		dk->dk_selflags = (__psunsigned_t)arg;
		break;
		
	case DIOCSELECT:
	{
		struct dk_ioctl_data select_args;
		u_char select_cmd[SC_CLASS0_SZ];
		u_int  *select_data = (u_int *)ioctl_data;

#if _MIPS_SIM == _ABI64
		if (convert_abi) {
			struct irix5_dk_ioctl_data i5_select_args;

			error = gencopy(arg, &i5_select_args, 
				sizeof(struct irix5_dk_ioctl_data),0);
			if (!error)
				irix5_dkioctl_to_dkioctl(&i5_select_args, 
					&select_args);
		} else
#endif /* _ABI64 */
			error = gencopy(arg, &select_args, 
				sizeof(struct dk_ioctl_data), 0);
		if (error)
			goto copyerr;
		if (select_args.i_len > MAX_IOCTL_DATA)
			goto invalerr;
		bcopy(dk_mode_select, select_cmd, sizeof(select_cmd));
		select_cmd[4] = select_args.i_len;
		select_cmd[1] = dk->dk_selflags;
		if(gencopy(select_args.i_addr, select_data, 
			select_args.i_len, 0))
			goto copyerr;

		mutex_lock(dksema, PRIBIO);
		if (dk_cmd(dk, select_cmd, SC_CLASS0_SZ, DKSC_TIMEOUT, 
			   (caddr_t) select_data, select_args.i_len, B_WRITE)) {
			mutex_unlock(dksema);
			goto ioerr;
		}
		if (dk_cmd(dk, dk_tst_unit_rdy, SC_CLASS0_SZ,
			   DKSC_TIMEOUT, (caddr_t) 0, 0, B_READ)) {
			mutex_unlock(dksema);
			goto ioerr;
		}
		/*
		 * Mode select may have changed the block size --
		 * check it again.
		 */
		(void) dk_getblksz(dk);

		/*
		 * Not clear if it would ever be useful to update
		 * drivecap here if blocksize changed.
		*/

		mutex_unlock(dksema);
		break;
	}

	case DIOCREADCAPACITY:
	{
		mutex_lock(dksema, PRIBIO);
		flag = dk_readcap(dk);
		mutex_unlock(dksema);
		if(gencopy(&flag, arg, sizeof(u_int), 0))
			goto copyerr;
		break;
	}

	case DIOCRDEFECTS:
	{
		struct dk_ioctl_data defect_args;
		u_char defect_cmd[SC_CLASS1_SZ];
		uint copylen;

#if _MIPS_SIM == _ABI64
		if (convert_abi) {
			struct irix5_dk_ioctl_data i5_defect_args;

			error = gencopy(arg, &i5_defect_args, 
				sizeof(struct irix5_dk_ioctl_data),0);
			if (!error)
				irix5_dkioctl_to_dkioctl(&i5_defect_args, 
					&defect_args);
		} else
#endif /* _ABI64 */
			error = gencopy(arg, &defect_args, 
				sizeof(struct dk_ioctl_data), 0);
		if(error)
			goto copyerr;
		copylen = defect_args.i_len;

		/*
		 * Allocate amount user requested; drives have gotten bigger
		 * than will fit into MAX_IOCTL_DATA; should fix someday to
		 * just dma directly into users buffer
		 */
		if (defect_args.i_len > (100*NBPC))
			goto invalerr;
		ioctl_data = kmem_alloc(defect_args.i_len, KM_CACHEALIGN);

		bcopy(dk_rdefects, defect_cmd, sizeof(defect_cmd));
		defect_cmd[2] |= defect_args.i_page;
		defect_cmd[7] = defect_args.i_len >> 8;
		defect_cmd[8] = defect_args.i_len & 0xff;
		if (dk_cmd(dk, defect_cmd, SC_CLASS1_SZ, DKSC_TIMEOUT,
			   ioctl_data, defect_args.i_len, B_READ))
			goto ioerr;
		if (gencopy(ioctl_data, defect_args.i_addr, copylen, 0))
			goto copyerr;
		break;
	}

	case DIOCADDBB:	/* spare a single (logical) block */
	{
		daddr_t bn = (daddr_t)arg;
		u_char *bbinfo = (u_char *)ioctl_data;
		bzero(bbinfo, sizeof(bbinfo));

		bbinfo[3] = 4;	/* one block (4 bytes) */
		bbinfo[4] = (u_char)(bn>>24);
		bbinfo[5] = (u_char)(bn>>16);
		bbinfo[6] = (u_char)(bn>>8);
		bbinfo[7] = (u_char)bn;
		if(dk_cmd(dk, dk_add_defects, SC_CLASS0_SZ,
			  DKSC_TIMEOUT, (caddr_t)bbinfo, 8, B_WRITE))
			goto ioerr;
		break;
	}

#ifdef DEBUG
	case _DIOC_('s'):	/* return stats on i/o sizes */
		error = copyout(dk->dk_blkstats, arg, sizeof(*dk->dk_blkstats));
		break;
#endif

	case DIOCMKALIAS:
		device_controller_num_set(SDI_DISK_VHDL(disk_info),
			device_controller_num_get(SDI_CTLR_VHDL(disk_info)));
		dk_alias_make(dev);
		error = 0;
		break;

	default:

invalerr:
		error = EINVAL;
		break;
	}

	if(ioctl_data)
		kern_free(ioctl_data);
	return error;
}



/*
 * read or write device
 * physiock() speaks in units of 512-byte blocks, while the partition
 * table uses native blocksizes, whatever they may be, so we have
 * to convert here and in strategy.
 */
/* ARGSUSED */
dkscread(dev_t dev, uio_t *uiop, cred_t *crp)
{
	register int	 nblks;
	register struct dksoftc	*dk;
	register uint64_t flag = B_READ;
	buf_t	*bp;
	int rc;

	scsi_disk_info_t		*disk_info;
	scsi_part_info_t		*part_info;
	uchar_t		   	 	part;
	vertex_hdl_t			dev_vhdl;
	int				pio = ((uiop->uio_pio == KAIO)
					       || (uiop->uio_pio == KLISTIO));

	dev_vhdl	= dev;
	part_info 	= scsi_part_info_get(dev_vhdl);
	part		= SPI_PART(part_info);
	disk_info	= SPI_DISK_INFO(part_info);
	dk		= SDI_DKSOFTC(disk_info);

	if(dkCHKBB(uiop->uio_offset))
		return EIO;
#if defined(EVEREST) || defined(SN) || defined(IP30)
	if (dk->dk_flags & DK_MAPUSER)
		flag |= B_MAPUSER;
#endif
	nblks = dk->dk_vh.vh_pt[part].pt_nblks;
	if (!pio) {
		bp = getrbuf(KM_SLEEP);
		bp->b_edev = dev;
	} else {
		/* Can't let this be freed for async i/o */
		bp = 0;
	}
	rc = physiock((physiofunc)dkscstrategy, bp, dev, flag,
		      nblks * (dk->dk_blksz / NBPSCTR), uiop);
	if (!pio) {
		freerbuf(bp);
	}
	return(rc);
}

/* ARGSUSED */
dkscwrite(register dev_t dev, uio_t *uiop, cred_t *crp)
{
	register int	 	nblks;
	register struct dksoftc	*dk;
	register uint64_t 	flag = B_WRITE;
	buf_t			*bp;
	int			rc;

	scsi_disk_info_t	*disk_info;
	scsi_part_info_t	*part_info;
	uchar_t		   	 part;
	vertex_hdl_t		dev_vhdl;

	int				pio = ((uiop->uio_pio == KAIO)
					       || (uiop->uio_pio == KLISTIO));

	dev_vhdl	= dev;
	part_info 	= scsi_part_info_get(dev_vhdl);
	part		= SPI_PART(part_info);
	disk_info	= SPI_DISK_INFO(part_info);
	dk 		= SDI_DKSOFTC(disk_info);

	if(dkCHKBB(uiop->uio_offset))
		return EIO;
#if defined(EVEREST) || defined(SN) || defined(IP30)
	if (dk->dk_flags & DK_MAPUSER)
		flag |= B_MAPUSER;
#endif
	nblks = dk->dk_vh.vh_pt[part].pt_nblks;
	if (!pio) {
		bp = getrbuf(KM_SLEEP);
		bp->b_edev = dev;
	} else {
		/* Can't let this be freed for async i/o */
		bp = 0;
	}
	rc = physiock((physiofunc)dkscstrategy, bp, dev, flag,
		      nblks * (dk->dk_blksz / NBPSCTR), uiop);
	if (!pio) {
		freerbuf(bp);
	}
	return(rc);
}

static void
dkscpurgebp(register struct dksoftc *dk, register struct buf *bp)
{
#if defined(EVEREST)
	if (bp->b_flags & B_MAPUSER)
		dma_userfree(bp);
#endif
#if defined(SN) || defined(IP30)
        if ((bp->b_flags & B_MAPUSER) && (dk->dk_flags & DK_ALENLIST)) {
		alenlist_destroy((alenlist_t)bp->b_private);
		bp->b_private = NULL;
	}
#endif
	bp->b_flags |= B_ERROR;
	if (!bp->b_error)
		bp->b_error = EIO;
	DISKSTATE(DISK_EVENT_DONE,bp);
	biodone(bp);
}

static void
dkscpurgequeues(register struct dksoftc *dk)
{
	register struct buf *bp;

	/*
	** Clear the dk_queue.prio_head.
	*/
	while (bp=dk->dk_queue.prio_head) {
		dk->dk_queue.io_head = bp->av_forw;
		if (bp->av_forw == NULL)
			dk->dk_queue.io_tail = NULL;
		dkscpurgebp (dk,bp);
	}

	/*
	** Clear the dk_queue.io_head.
	*/
	while (bp=dk->dk_queue.io_head) {
		dk->dk_queue.io_head = bp->av_forw;
		if (bp->av_forw == NULL)
			dk->dk_queue.io_tail = NULL;
		dkscpurgebp (dk,bp);
	}

	/*
	** Clear the dk_retry queue.
	*/
	while (bp=dk->dk_retry) {
		dk->dk_retry = bp->av_forw;
		dkscpurgebp (dk,bp);
	}

	return;
}

static void
dksccommand(register struct dksoftc *dk)
{
	register struct scsi_request	*sp;
	register struct buf		*bp;
#ifdef DEBUG
	int				 ioblocks;
#endif

	while ((bp = (dk->dk_retry ? dk->dk_retry :
		      dk->dk_queue.prio_head ? dk->dk_queue.prio_head :
		      dk->dk_queue.io_head)) != NULL)
       {

		/*
		 * If in the process of issuing a callback command,
		 * hold all commands. We will restart the issuing
		 * of commands when the callback command completes.
		 */
		if (dk->dk_flags & DK_HOLD)
			break;
		
		/* 
		 * Check whether a callback needs to be issued.
		 * Since the "wait" argument is 0, DK_HOLD and
		 * DK_NEED_CB will be set by the callback function
		 * if appropriate.
		 */
		if (dk->dk_flags & DK_NEED_CB) {
			dk_callback_backend(dk, 0);
			break;
		}

		/*
		 * See if the queue needs to be purged.
		 */
		if (dk->dk_flags & (DK_FAILOVER | DK_WONT_START)) {
			dkscpurgequeues (dk);
			break;
		}

		/*
		 * Get an sp, and use simple tag, if queueing enabled.
		 */
		if ((sp = drive_acquire(dk, 0)) == NULL)
			break;
		if (dk->dk_flags & DK_QUEUE)
			sp->sr_tag = SC_TAG_SIMPLE;

#if SAVESTAT
		if (sp->sr_ctlr == statadap && sp->sr_target != stattarg)
		{
			if (bp->b_sort <= dk->dk_queue.io_state.prevblk + 512 &&
			    bp->b_sort > dk->dk_queue.io_state.prevblk)
				dkseq++;
			dktot++;
			dkbuffer[dkloc].bn = bp->b_sort;
			dkbuffer[dkloc].flags = bp->b_flags;
			dkbuffer[dkloc].count = dkBTOBB(bp->b_bcount);
			dkbuffer[dkloc].dev = bp->b_edev;
			dkbuffer[dkloc].start = lbolt;
			dkbuffer[dkloc].queue = bp->b_start;
			dkbuffer[dkloc].finish = -1;
			if (++dkloc == DKSTATSIZE)
				dkloc = 0;
		}
#endif

		/* 
		 * Check the retry queue first -- it has priority.
		 */
		if (dk->dk_retry) 
			dk->dk_retry = bp->av_forw;
		else if (dk->dk_flags & DK_NO_SORT)
		{
			dk->dk_queue.io_head = bp->av_forw;
			if (bp->av_forw == NULL)
				dk->dk_queue.io_tail = NULL;
		}
		else
		{
			bp = fair_disk_dequeue(&(dk->dk_queue));
		}
		if (dk->dk_active == NULL) {
			dk->dk_active = bp;
			bp->av_forw = NULL;
		}
		else {
			bp->av_forw = dk->dk_active;
			bp->av_forw->av_back = bp;
			dk->dk_active = bp;
		}
		bp->av_back = NULL;

		/*
		 * Update sar statistics
		 */
		if (dk->dk_curq == 1)
			dk->dk_iostart = lbolt;

#ifdef DEBUG
		/* Internal activity statistics */
		{
		int	bbcnt = bp->b_bcount >> BBSHIFT;
		for(ioblocks=0; ioblocks < (NUM_BLK_BUCKETS-1); ioblocks++)
			if(!(bbcnt /= 2))
				break;
		if(bp->b_flags & B_READ)
			dk->dk_blkstats->bk_reads[ioblocks]++;
		else 
			dk->dk_blkstats->bk_writes[ioblocks]++;
		}
#endif

		/* start things up */
		DISKSTATE(DISK_EVENT_START,bp);
		DKSCUNLOCK(&dk->dk_lock);
		dkscstart(dk, bp, sp);
		DKSCLOCK(&dk->dk_lock);
	}
}


/*
 * queue device request 
 */
void
dkscstrategy(register buf_t *bp)
{
	register struct partition_table *pt;
	int				sc;
	int 				blocknum;
	int 				error = 0;
	scsi_disk_info_t		*disk_info;
	scsi_part_info_t		*part_info;
	register struct dksoftc 	*dk;
	struct iotime			*dkiot;
	uchar_t 			part;
	vertex_hdl_t			dev_vhdl;
	
	dev_vhdl	= bp->b_edev;

	part_info 	= scsi_part_info_get(dev_vhdl);
	part		= SPI_PART(part_info);
	disk_info	= SPI_DISK_INFO(part_info);

	dk 		= SDI_DKSOFTC(disk_info);

	/* Is device open, and is the vh valid? */
	if(dk == NULL || !dk->dk_vh.vh_magic)
		goto bad;

#if defined(INDUCE_IO_ERROR)
	if (dksc_insert_error && 
	    bp->b_edev == dksc_error_dev) {
		if (++dksc_error_count > dksc_error_freq) {
			dksc_error_count = 0;
			goto bad;
		}
	}
#endif
	if (bp->b_blkno % (dk->dk_blksz / NBPSCTR))
		goto bad;
	blocknum = bp->b_blkno / (dk->dk_blksz / NBPSCTR);

	/* 
	 * Compute number of blocks in transfer and make sure the request 
	 * is contained in the partition.  large reads/writes that could
	 * be partially satisfied will fail because of the difficulty of
	 * handling the residual count correctly.
	 */
	if(dkCHKBB(bp->b_bcount))
		goto bad;
	sc = dkBTOBB(bp->b_bcount);
	if (sc > 0xFFFF) {
		error = EINVAL;
		goto bad;
	}
	if (sc == 0) {
		bp->b_resid = bp->b_bcount;
		biodone(bp);
		return;
	}
	pt = &dk->dk_vh.vh_pt[part];
	if ((blocknum < 0) || (blocknum + sc) > pt->pt_nblks) {
		error = ENOSPC;
		goto bad;
	}

	if(!(bp->b_flags & B_READ) && (dk->dk_flags & DK_WRTPROT)) {
		error = EROFS;
		goto bad;
	}

	/*
	 * Reject the io if the device has failed over.
	 */
	if (dk->dk_flags & DK_FAILOVER) {
		error = EIO;
		goto bad;
	}

#if defined(EVEREST)
	if (bp->b_flags & B_MAPUSER) {
		if (dma_usertophys(bp))
			goto bad;
	}
#endif
#if defined(SN) || defined(IP30)
	if ((bp->b_flags & B_MAPUSER) && (dk->dk_flags & DK_ALENLIST)) {
		alenlist_t alen_p;

		if ((alen_p = alenlist_create(0)) == NULL)
			goto bad;
		bp->b_private = (alenlist_t)alen_p;
		if (uvaddr_to_alenlist(alen_p, bp->b_un.b_addr, bp->b_bcount, 0) == NULL)
			goto badfree;
	}

#endif

	/* set block number for dkscstart() */
	bp->b_sort = blocknum + pt->pt_firstlbn;
	bp->av_forw = NULL;
	bp->b_start = lbolt;

	DKSCLOCK(&dk->dk_lock);

	dkiot = SDI_DKIOTIME(disk_info);
	dkiot->ios.io_qcnt++;
	DISKSTATE(DISK_EVENT_QUEUED,bp);

	if (dk->dk_flags & DK_NO_SORT) {
		nosort(&dk->dk_queue, bp);
	} else {
		macsisort(&dk->dk_queue, bp);
	}
	dksccommand(dk);
	DKSCUNLOCK(&dk->dk_lock);
	return;

badfree:
#if defined(EVEREST)
	if (bp->b_flags & B_MAPUSER)
		dma_userfree(bp);
#endif
#if defined(SN) || defined(IP30)
        if ((bp->b_flags & B_MAPUSER) && (dk->dk_flags & DK_ALENLIST)) {
		alenlist_destroy((alenlist_t)bp->b_private);
		bp->b_private = NULL;
	}
#endif
bad:
	bp->b_flags |= B_ERROR;
	if (error != 0)
		bp->b_error = error;
	else if (!bp->b_error)
		bp->b_error = EIO;
	biodone(bp);
}

/*
 * setup a device operation for calls from outside the driver; upper
 * levels of kernel are assumed to do the dki_dcache_{wb,inval} if needed.
 */
static void
dkscstart(struct dksoftc *dk, buf_t *bp, scsi_request_t *sp)
{
	int sn, sc;
	scsi_lun_info_t		*lun_info;

	lun_info = scsi_lun_info_get(sp->sr_lun_vhdl);

	/* Don't start up any disk requests if we are doing a panic dump */
	if (dksc_dumpdev)
		return;

	/* Let the scsi driver do the transfer */
	if( bp->b_flags & B_READ )
		bcopy(dk_read, sp->sr_command, SC_CLASS1_SZ);
	else
		bcopy(dk_write, sp->sr_command, SC_CLASS1_SZ);

	sc = dkBTOBB(bp->b_bcount);
	sn = bp->b_sort;

	/*
	 * Stuff in the logical block addr. 
	 * Its spot in the cmd is on a short boundary.
	 */
	sp->sr_command[2] = (u_char)(sn >> 24);
	sp->sr_command[3] = (u_char)(sn >> 16);
	sp->sr_command[4] = (u_char)(sn >> 8);
	sp->sr_command[5] = (u_char)sn;
	sp->sr_command[7] = (u_char)(sc >> 8);
	sp->sr_command[8] = (u_char)sc;

	sp->sr_cmdlen = SC_CLASS1_SZ;

	sp->sr_notify = dk_intr;
	if (dk->dk_flags & DK_NO_RETRY) 
		sp->sr_timeout = DKSC_SHORT_TIMEOUT;
	else
		sp->sr_timeout = DKSC_TIMEOUT;

	if (bp->b_flags & B_READ)
		sp->sr_flags |= SRF_DIR_IN;
	else
		sp->sr_flags &= ~SRF_DIR_IN;
#if defined(EVEREST) || defined(SN) || defined(IP30)
	if (bp->b_flags & B_MAPUSER) {
		sp->sr_flags |= SRF_MAPUSER;
		sp->sr_buffer = (u_char *)bp->b_un.b_addr;
		if (dk->dk_flags & DK_ALENLIST)
			sp->sr_flags |= SRF_ALENLIST;
	}
	else
#endif
	if (BP_ISMAPPED(bp)) {
		sp->sr_flags |= SRF_MAP;
		sp->sr_buffer = (u_char *)bp->b_dmaaddr;
	}
	else
		sp->sr_flags |= SRF_MAPBP;

	sp->sr_buflen = bp->b_bcount;
	sp->sr_bp = (void *) bp;
	SLI_COMMAND(lun_info)(sp);
}


void
dk_queue_retry(struct dksoftc *dk, struct buf *bp, struct scsi_request *sp)
{
	DKSCLOCK(&dk->dk_lock);
	/*
	 * Take command off active queue
	 */
	if (bp->av_back != NULL)
		bp->av_back->av_forw = bp->av_forw;
	if (bp->av_forw != NULL)
		bp->av_forw->av_back = bp->av_back;
	if (dk->dk_active == bp)
		dk->dk_active = bp->av_forw;

	/* 
	 * .... and add it to the retry queue.
	 */
	if (dk->dk_retry == NULL) {
		bp->av_forw = NULL;
		dk->dk_retry = bp;
	}
	else {
		bp->av_forw = dk->dk_retry;
		dk->dk_retry = bp;
	}
	bp->b_resid = bp->b_bcount;
	dkrelsubchan(dk, sp);
	DKSCUNLOCK(&dk->dk_lock);
}


/*
 * dksc interrupt routine, called via scsisubrel().
 *
 */
static void
dk_intr(register scsi_request_t *sp)
{
	register struct buf		*bp;
	register struct dksoftc		*dk;
	register struct iotime		*dkiot;
	int				 error = 0;
	scsi_disk_info_t		*disk_info;
	int				 bbcnt;
	ASSERT(sp->sr_bp);

	bp = (buf_t *) sp->sr_bp;

	if(dksc_dumpdev && dksc_dumpdev != bp->b_edev)
		return; /* don't do anything if not dumpdev */
	disk_info = scsi_disk_info_get(sp->sr_dev_vhdl);
	dk = SDI_DKSOFTC(disk_info);  /* get the dkdrv info */

	/* 
	 * If a callback is in progress, don't retry the command
	 * now. "Return" it to the retry queue and it will get
	 * scheduled when the callback is complete.  
	 */
	if (dk->dk_flags & (DK_NEED_CB | DK_HOLD)) {
		dk_queue_retry(dk, bp, sp);
		return;
	}

	if (dk_chkcond(dk, sp, 1, 0) == 1)
		return;

	switch(sp->sr_status) {
	case SC_GOOD:
		if (sp->sr_scsi_status != ST_GOOD) {
			if (sp->sr_scsi_status == ST_BUSY)
				error = EBUSY;
			else
				error = EIO;
		}
		break;

	case SC_ALIGN:
		error = EFAULT;
		/* deliberate fall thru */

	default:
		{
		extern char *scsi_adaperrs_tab[];
		if(sp->sr_status < NUM_ADAP_ERRS)
			dk_pmsg(bp->b_edev, "SCSI driver error: %s\n",
				scsi_adaperrs_tab[sp->sr_status]);
		else
			dk_pmsg(bp->b_edev, "Unknown SCSI driver error %d\n", sp->sr_status);
		error = EIO;
		}
	}

	if (error) {
		bp->b_flags |= B_ERROR;
		bp->b_error = error;
	}
	bp->b_resid = sp->sr_resid;

	/* don't need the spl, but just using splock() forces it to splhi... */
	DKSCLOCK(&dk->dk_lock);

	/* update accounting info */
	bbcnt = bp->b_bcount >> BBSHIFT;
	dkiot = SDI_DKIOTIME(disk_info);
	dkiot->io_cnt++;
	dkiot->io_bcnt += bbcnt;
	if(!(bp->b_flags & B_READ)) {
		dkiot->io_wops++;
		dkiot->io_wbcnt += bbcnt;
	}
	dkiot->io_resp += lbolt - bp->b_start;
	dkiot->io_act += lbolt - dk->dk_iostart;
	dkiot->ios.io_misc += dkiot->ios.io_qcnt;
	dkiot->ios.io_qcnt--;
#if SAVESTAT
	{
	int i;
	int j = dkloc;

	if (sp->sr_ctlr == statadap && sp->sr_target != stattarg)
		for (i = 0; i < DKSEARCH; i++) {
			if (--j < 1) j = DKSTATSIZE - 1;
			if (dkbuffer[j].bn == bp->b_sort &&
			    dkbuffer[j].dev == bp->b_edev) {
				dkbuffer[j].finish = lbolt;
				break;
			}
		}
	i = lbolt - dk->dk_iostart;
	if (dkthresh > 0 && i > dkthresh + bp->b_bcount / dk_bytesper10ms)
		dk_pmsg(bp->b_edev, "%d0 ms to do %dK command\n",
			i, bp->b_bcount / 1024);
	}
#endif
	if (dk->dk_curq > 1)
		dk->dk_iostart = lbolt;

	/*
	 * update active queues
	 */
	if (bp->av_back != NULL)
		bp->av_back->av_forw = bp->av_forw;
	if (bp->av_forw != NULL)
		bp->av_forw->av_back = bp->av_back;
	if (dk->dk_active == bp)
		dk->dk_active = bp->av_forw;

	dkrelsubchan(dk, sp);
	DKSCUNLOCK(&dk->dk_lock);
#if defined(EVEREST)
	if (bp->b_flags & B_MAPUSER)
		dma_userfree(bp);
#endif
#if defined(SN) || defined(IP30)
        if ((bp->b_flags & B_MAPUSER) && (dk->dk_flags & DK_ALENLIST)) {
		alenlist_destroy((alenlist_t)bp->b_private);
		bp->b_private = NULL;
	}
#endif
	DISKSTATE(DISK_EVENT_DONE,bp);					
	biodone(bp);
}


/*
 * Retry requests when they get Busy statuses or certain Not Ready
 * sense codes.
 * Return whether or not the retry should be done now, has been
 * scheduled for the future (via timeout), or is not to be done
 * (either because retry count was exceeded or because retries are
 * not to be done).
 * Retval:
 *	0 = no retry
 *	> 0 = retry necessary
 *	< 0 = retry scheduled
 */
int
dk_busy_retry(struct dksoftc *dk, struct scsi_request *sp, int async)
{
	int	retry_cmd;

	/*
	 * If async is set, we are called from interrupt,
	 * and must use timeout(), rather than delay().
	 */
	retry_cmd = (int) (intptr_t) sp->sr_dev;
	if ((retry_cmd < DK_MAX_BUSY) && (!(dk->dk_flags & DK_NO_RETRY)))
	{
		int intv = 1 << retry_cmd;
		if (intv > DK_MAX_BUSY_INTV)
			intv = DK_MAX_BUSY_INTV * HZ;
		else
			intv *= HZ;

		if (async)
		{
			sp->sr_dev = (void *) (intptr_t) (retry_cmd + 1);
			sp->sr_tag = 0;
			timeout(dk_do_retry, sp, intv);
			return -1;
		}
		else
		{
			delay(intv);
			return 1;
		}
	}
	return 0;
}

/*
 * Return values:
 *	0: command should not be retried.
 *	1: command should be/has been retried, depending on value of async
 * If async set, command retry is scheduled if not too many retries.
 * If not async, return value indicates whether to retry or not.
 */
static int
dk_chkcond(struct dksoftc *dk, register scsi_request_t *sp, int async, int mute)
{
	dev_t dev;
	int retval = 0;
	int retry_cmd = 0;
	int serious_err = 1;
	char key = SC_NOSENSE;

	/* so the error message in dk_pmsg are correct */
	if (async)
		dev = ((buf_t *) sp->sr_bp)->b_edev;
	else
		dev = dk->dk_buf.b_edev;

	switch (sp->sr_status)
	{
	case SC_CMDTIME:
	case SC_HARDERR:
	case SC_PARITY:
		/*
		 * Retry cmds that fail because of timeouts, hardware errors,
		 * or parity errors.
		 */
		retry_cmd = 1;
		break;

	case SC_GOOD:
		if (sp->sr_sensegotten > 0)
		{
		    if ((sp->sr_sense[0] & 0x70) != 0x70) {
			    dk_pmsg(dev,
			       "invalid sense data, error cause unknown\n");
			    retry_cmd = 1;
		    }
		    else if (sp->sr_sensegotten <= 2) {
			    dk_pmsg(dev,
			       "not enough sense data\n");
			    retry_cmd = 1;
		    }
		    else {
			key = sp->sr_sense[2] & 0xF;
			if (key == SC_UNIT_ATTN)
			    serious_err = 0;
			else if (dk_user_printable_sense(dk, sp, mute))
			    dk_print_sense(dk, sp, dev);

			if (key == SC_NOT_READY)
			{
			    /*
			     * ASC and ASQ are only valid if we received at
			     * least 14 bytes of sense data.
			     * If ASC is SC_NOTREADY (4) and ASQ is 0 or 2,
			     *   then set the Need Start Unit flag, as long
			     *   as the error is not for a Start Unit command.
			     * If the ASC/ASQ is 4/1, that means that the device
			     *   is spinning up, so we treat the same as a BUSY
			     *   SCSI status.
			     */
			    if (sp->sr_sensegotten > 13 &&
				sp->sr_sense[12] == SC_NOTREADY)
			    {
				uint8_t asq = sp->sr_sense[13];

				if ((sp->sr_command[0] != 0x1B) &&
				    (asq == 0 || asq == 2))
				{
				    DKSCLOCK(&dk->dk_lock);
				    dk->dk_flags |= DK_NEEDSTART;
				    DKSCUNLOCK(&dk->dk_lock);
				    if (async)
					dk_queue_retry(dk, sp->sr_bp, sp);
				    return 1;
				}
				else if (asq == 1)
				{
				    /*
				     * Some drives, the TOSHIBA CDROM
				     * in particular, will return
				     * NOT_READY to an I/O command for
				     * about 4 seconds following CD
				     * load or bus reset.  
				     */
				    retry_cmd = dk_busy_retry(dk, sp, async);
				    /* report 2nd, then every 4 times */
				    if (retry_cmd && ((intptr_t) sp->sr_dev % 4 == 2))
					dk_pmsg(dev, "NOT READY; retrying\n");
				    if (retry_cmd > 0)
					serious_err = 0;
				    else if (retry_cmd < 0)
					return 1;
				    else
					dk_pmsg(dev, "NOT READY; giving up\n");
				}
			    }
			}

			/*
			 * Retry commands that failed because of a unit
			 * attention, media error, hardware error, aborted
			 * command, or deferred errors.
			 * Retrying on unit attention may cause timeouts,
			 * but that's better than just letting async writes
			 * fail completely, which otherwise is quite likely
			 * to happen.
			 */
			else if (key == SC_MEDIA_ERR  || key == SC_HARDW_ERR ||
			         key == SC_CMD_ABORT  || key == SC_UNIT_ATTN ||
			         (sp->sr_sense[0] & 1))
			{
				retry_cmd = 1;
			}
		    }

		    /*
		     * To ensure that sense key errors cause an error to
		     * be returned, we set SCSI status to what it really
		     * was if sense data indicates a serious problem.
		     * otherwise, if we recovered, we want to clear the
		     * scsi status, so we don't consider it an error in
		     * dk_intr().
		     */
		    sp->sr_scsi_status = (key==SC_ERR_RECOVERED)?ST_GOOD:ST_CHECK;
		}
		else if (sp->sr_scsi_status == ST_BUSY) {
			/* retry up to 4 minutes, same reason as for startunit */
			retry_cmd = dk_busy_retry(dk, sp, async);

			/* report 2nd, then every 4 times */
			if (retry_cmd && ((intptr_t) sp->sr_dev % 4 == 2))
				dk_pmsg(dev, "BUSY; retrying\n");
			if (retry_cmd > 0)
				serious_err = 0;
			else if (retry_cmd < 0)
				return 1;
			else {	/* give up, busy too long */
				if (dk->dk_flags & DK_NO_RETRY)
					dk_pmsg(dev, "Error: BUSY not retrying\n");
				else
					dk_pmsg(dev, "Error: BUSY more than 4 minutes\n");
			}
		}
		else if (sp->sr_scsi_status == ST_CHECK) {
			dk_pmsg(dev, "unexpected SCSI check condition status "
				"(request sense failed)\n");
			retry_cmd = 1;
		}
		else if (sp->sr_scsi_status != ST_GOOD) {
			dk_pmsg(dev, "unexpected SCSI status byte 0x%x\n",
				sp->sr_scsi_status);
			retry_cmd = 1;
		}
		break;

	case SC_ATTN:
		/*
		 * We don't want to count the SC_ATTN against the retry limit,
		 * so retries are handled here.
		 */
#if DKSCDEBUG
		dk_pmsg(dev, "SC_ATTN: retrying request %d\n", sp->sr_dev);
#endif
		if(async)
			dkscstart(dk, sp->sr_bp, sp);
		retval = 1;
		break;
	}

	if ((retry_cmd) && (!(dk->dk_flags & DK_NO_RETRY))) {
		if (serious_err)
			retry_cmd = (__psint_t) sp->sr_dev + 4;
		else
			retry_cmd = (__psint_t) sp->sr_dev + 1;
		if (retry_cmd <= DK_MAX_RETRY) {
			sp->sr_dev = (void *)(__psint_t)retry_cmd;
			sp->sr_tag = 0;
#if DKSCDEBUG
			dk_pmsg(dev, "  retrying request %d\n", retry_cmd);
#else
			if ((sp->sr_status == SC_GOOD) && serious_err)
			    dk_pmsg(dev, "  retrying request\n");
#endif
			if(async)
				dkscstart(dk, sp->sr_bp, sp);
			retval = 1;
		}
		else if (sp->sr_status == SC_GOOD) {
			if (serious_err)
			    dk_pmsg(dev, "  retries exhausted\n");
			else
			    dk_pmsg(dev, "  failure, retries exhausted - key %d"
					 " status %d\n", key, sp->sr_scsi_status);
		}
	} else if (retry_cmd) {
		dk_pmsg(dev, " Error: not retrying \n");
	}
        return retval;
}


/*
 * print controller/unit/partition number for a dksc device
 */
void
dkscprint(dev_t dev, char *str)
{
	dk_pmsg(dev, "%s\n", str);
}

/*
 * return partition size, in blocks 
 */
int
dkscsize(dev_t dev)
{
	register struct dksoftc	*dk;
	int			didopen = 0;

	scsi_disk_info_t	*disk_info;
	scsi_part_info_t	*part_info;	
	uchar_t 		part;
	vertex_hdl_t		dev_vhdl;

	dev_vhdl	= dev;
	part_info 	= scsi_part_info_get(dev_vhdl);
	part		= SPI_PART(part_info);
	disk_info	= SPI_DISK_INFO(part_info);

	if (!disk_info || !SDI_DKSOFTC(disk_info)) {
		dkscopen(&dev, 0, OTYP_LYR, (cred_t *)NULL);
		dk = SDI_DKSOFTC(disk_info); /* get the dkdrv info */
		if (dk == NULL) {
			return(-1);
		}
		didopen = 1;
	}
	else
		dk = SDI_DKSOFTC(disk_info);
	if(didopen)
		dkscclose(dev, 0, OTYP_LYR, (cred_t *)NULL);
	return (dk->dk_vh.vh_pt[part].pt_nblks);

}


/*
 * Flag to tell whether we had to do an open in the scsi dump routine.
 */
static int dumpopen = 0;

/*
 * Dump data to disk.
 */
int
dkscdump(dev, flag, bn, physaddr, sc)
	dev_t 	dev;
	int 	flag;
	daddr_t bn;
	caddr_t physaddr;
	int 	sc;
{
	struct dksoftc		*dk;
	struct partition_table 	*pt;
	uint32_t 		sn;


	scsi_disk_info_t	*disk_info;
	scsi_part_info_t	*part_info;

	uchar_t 		part;
	vertex_hdl_t		lun_vhdl;
	vertex_hdl_t		dev_vhdl;

	dev_vhdl	= dev;	

	part_info 	= scsi_part_info_get(dev_vhdl);
	part		= SPI_PART(part_info);
	disk_info	= SPI_DISK_INFO(part_info);
	lun_vhdl	= SDI_LUN_VHDL(disk_info);

	if (flag == DUMP_OPEN) {
		dksc_dumpdev = dev;
		if ((!disk_info) || (!SDI_DKSOFTC(disk_info))) {
			dkscopen(&dev, 0, OTYP_LYR, (cred_t *)NULL);
			disk_info 	= scsi_disk_info_get(lun_vhdl);
			if ((dk = SDI_DKSOFTC(disk_info)) == NULL)
				return (ENXIO);
			dumpopen = 1;
			if (SDI_DUMP(disk_info)(SDI_CTLR_VHDL(disk_info)) == 0)
				return (EIO);
		}
		else {
			dk = SDI_DKSOFTC(disk_info);

			/* initialize device */
			if (SDI_DUMP(disk_info)(SDI_CTLR_VHDL(disk_info)) == 0)
				return (EIO);
			/*
			 * clobber the semaphores so we don't get stuck waiting;
			 * this is a panic situation anyway, so no more disk i/o
			 * will be done after the dump...  Should abort/ignore
			 * any pending i/o on this device also, so it doesn't
			 * confuse things.
			 */
			sv_destroy(&dk->dk_wait);
			mutex_destroy(&dk->dk_lock);
			mutex_destroy(&dk->dk_sema);
			freesema(&dk->dk_done);
			sv_destroy(&dk->dk_hold);
			mutex_init(&dk->dk_lock, MUTEX_DEFAULT, "dkdumpl");
			sv_init(&dk->dk_wait, SV_DEFAULT, "dkdumpw");
			mutex_init(&dk->dk_sema, MUTEX_DEFAULT, "dkdumps");
			initnsema(&dk->dk_done, 0, "dkdumpd");
			sv_init(&dk->dk_hold, SV_DEFAULT, "dkdumph");
		}
		return (0);
	}
	if (flag == DUMP_CLOSE) {
		if (dumpopen) {
			dumpopen = 0;
			dkscclose(dev, 0, 0, (cred_t *)NULL);
		}
		return (0);
	}
	if ((!disk_info) || ((dk = SDI_DKSOFTC(disk_info))== NULL))
		return(ENXIO);
	pt = &dk->dk_vh.vh_pt[part];		
	if ((bn < 0) || (bn + sc > pt->pt_nblks))
		return (EINVAL);
	sn = bn + pt->pt_firstlbn;
	if (dk_rw(dk, (caddr_t)PHYS_TO_K0(physaddr), sn, sc, (caddr_t)dk_write, 0))
		return(EIO);
	else
		return(0);
}


/*
 * These functions start and get completion of driver generated commands.
 */
void
dk_rcvcmd(struct scsi_request *sp)
{
	struct dksoftc 		*dk;
	scsi_disk_info_t	*disk_info;

	disk_info 	= scsi_disk_info_get(sp->sr_dev_vhdl);
	dk		= SDI_DKSOFTC(disk_info);
	
	if(!dksc_dumpdev) 	/* don't use semaphores when dumping */
		vsema(&dk->dk_done);
}

void
dk_sendcmd(register struct dksoftc *dk, struct scsi_request *sp)
{
	scsi_lun_info_t		*lun_info;

	lun_info 		= scsi_lun_info_get(sp->sr_lun_vhdl);
	if (!dksc_dumpdev)		/* don't use semaphores when dumping */
		mutex_lock(&dk->dk_sema, PRIBIO);
	sp->sr_notify = dk_rcvcmd;	/* need this even if dumping */
	SLI_COMMAND(lun_info)(sp);
	if (!dksc_dumpdev) {
		psema(&dk->dk_done, PRIBIO);
		mutex_unlock(&dk->dk_sema);
	}
}


/*
 * Disk read/write command
 *
 * Returns 1 if unsuccessful;
 *         0 if successful
 */
static int
dk_rw(register struct dksoftc *dk, caddr_t addr, uint32_t block_num, int block_len,
	char *cmd_buffer, int mute)
{
	register scsi_request_t		*sp;
	u_char				serr;
	int				count = dkBBTOB(block_len);

	/*
	 * Wait if commands are to be held. Check twice, in case
	 * someone beats us to it.
	 */
 again:
	if (dk->dk_flags & DK_HOLD) {
		DKSCLOCK(&dk->dk_lock);
		if (dk->dk_flags & DK_HOLD)
			sv_wait(&dk->dk_hold, PRIBIO, &dk->dk_lock, 0);
		else
			DKSCUNLOCK(&dk->dk_lock);
	}
	/*
	 * Check if callback is needed. If so, attempt to do
	 * it unless someone already beat us to it.
	 */
	if (dk->dk_flags & DK_NEED_CB) {
		DKSCLOCK(&dk->dk_lock);
		if (dk->dk_flags & DK_NEED_CB) {
			dk_callback_backend(dk, 1);
			/* DKSCUNLOCK done by backend */
		}
		else {
			DKSCUNLOCK(&dk->dk_lock);
			goto again;
		}
	}
	if (dk->dk_flags & DK_WONT_START)
		return 1;

	sp = drive_acquire(dk, 1);

	do {
		/*
		 * If a callback is in progress, don't (re)try the
		 * command now. Try to start the callback or wait for
		 * it to complete before trying again.
		 */
		if (dk->dk_flags & (DK_NEED_CB | DK_HOLD)) {
			drive_release(dk, sp);
			goto again;
		}

		bcopy(cmd_buffer, sp->sr_command, SC_CLASS1_SZ);
		/*
		 * Stuff in the logical block addr. 
		 * Its spot in the cmd is on a short boundary.
		 */
		sp->sr_command[2] = (u_char)(block_num >> 24);
		sp->sr_command[3] = (u_char)(block_num >> 16);
		sp->sr_command[4] = (u_char)(block_num >> 8);
		sp->sr_command[5] = (u_char)block_num;
		sp->sr_command[7] = (u_char)(block_len >> 8);
		sp->sr_command[8] = (u_char)block_len;

		if (cmd_buffer == (caddr_t)dk_read) {
			sp->sr_flags |= SRF_DIR_IN;
			/* make sure dirty cache lines don't clobber our i/o
			 * by getting flushed by other cache misses after our
			 * DMA has happened and before the inval below. */
			dki_dcache_inval(addr, count);
		}
		else {
			sp->sr_flags &= ~SRF_DIR_IN;
			dki_dcache_wb(addr, count);
		}
		sp->sr_flags |= SRF_MAP;
		sp->sr_buflen = count;
		sp->sr_buffer = (u_char *)addr;
		sp->sr_cmdlen = SC_CLASS1_SZ;
		sp->sr_timeout = DKSC_TIMEOUT;
		dk_sendcmd(dk, sp);
	} while (dk_chkcond(dk, sp, 0, mute) == 1);

	if(cmd_buffer == (caddr_t)dk_read)
		/* have to do after i/o for IP12; shouldn't have to if we 
		 * flush above, now that we have KM_CACHEALIGN doing the
		 * right thing for IP12/IP9, but fails if we don't; didn't
		 * have time to figure out why for 5.1....  Since we
		 * have to do this one here, only do the one before read
		 * for r4ks, so avoid the extra flush before on the slower
		 * machines */
		dki_dcache_inval(addr, count);
	else if(dk->dk_flags & DK_WRTPROT)
		return 1;

	serr = (sp->sr_status || sp->sr_scsi_status);	/* before release */
	drive_release(dk, sp);
	return serr;
}


/*
 * General disk command.
 *
 * Returns 1 if unsuccessful;
 *         0 if successful
 */
static int
dk_cmd(register struct dksoftc *dk, u_char *cmd_buffer, int cmd_size,
	u_int timeoutval, caddr_t addr, size_t len, int rw)
{
	register scsi_request_t		*sp;
	int				stat;

	ASSERT(!addr == !len);

	/*
	 * Wait if commands are to be held. Check twice, in case
	 * someone beats us to it.
	 */
 again:
	if (dk->dk_flags & DK_HOLD) {
		DKSCLOCK(&dk->dk_lock);
		if (dk->dk_flags & DK_HOLD)
			sv_wait(&dk->dk_hold, PRIBIO, &dk->dk_lock, 0);
		else
			DKSCUNLOCK(&dk->dk_lock);
	}
	/*
	 * Check if callback is needed. If so, attempt to do
	 * it unless someone already beat us to it.
	 */
	if (dk->dk_flags & DK_NEED_CB) {
		DKSCLOCK(&dk->dk_lock);
		if (dk->dk_flags & DK_NEED_CB) {
			dk_callback_backend(dk, 1);
			/* DKSCUNLOCK done by backend */
		}
		else {
			DKSCUNLOCK(&dk->dk_lock);
			goto again;
		}
	}
	if (dk->dk_flags & DK_WONT_START)
		return 1;

	sp = drive_acquire(dk, 1);

	do {
		/*
		 * If a callback is in progress, don't (re)try the
		 * command now. Try to start the callback or wait for
		 * it to complete before trying again.
		 */
		if (dk->dk_flags & (DK_NEED_CB | DK_HOLD)) {
			drive_release(dk, sp);
			goto again;
		}

		bcopy(cmd_buffer, sp->sr_command, cmd_size);
		sp->sr_cmdlen = cmd_size;
		if (rw & B_READ)
			sp->sr_flags |= SRF_DIR_IN;
		if (addr != NULL) {
			sp->sr_flags |= SRF_MAP;
			if (rw & B_READ)
				dki_dcache_inval(addr, len);
			else
				dki_dcache_wb(addr, len);
		}
		sp->sr_timeout = timeoutval;
		sp->sr_buffer = (u_char *)addr;
		sp->sr_buflen = len;
		dk_sendcmd(dk, sp);
		
	} while (dk_chkcond(dk, sp, 0, 0) == 1);

	stat = (sp->sr_status || sp->sr_scsi_status);
	drive_release(dk, sp);
	return stat;
}

/*
 * Print interesting sense data.
 * Extended sense data is all that is allowed.
 */
static void
dk_print_sense(struct dksoftc *dk, struct scsi_request *sp, dev_t dev)
{
	register char	*key_msg, *add_msg;
	register u_int	 sense_key, add_sense = 0;
	u_int	 	 asq = 0;
	u_char   	*sense_data = sp->sr_sense;
	u_char		 sense_length = sp->sr_sensegotten;
	scsi_part_info_t *part_info;
	u_int		 sense_lba, lba;
	int 		 part;

	/* Extended sense format */
	sense_key = sense_data[2] & 0xf;
	if (sense_data[2] & 0x20)
		dk_pmsg(dev, "unsupported block size requested\n");

	if (sense_length > 12)
		add_sense = sense_data[12];
	if (sense_length > 13)
		asq = sense_data[13];

	key_msg = scsi_key_msgtab[sense_key];
	if (add_sense == 0)
		add_msg = NULL;
	else if (add_sense < SC_NUMADDSENSE)
		add_msg = scsi_addit_msgtab[add_sense];
	else {
		/*
		 * 0x89 is vendor specific;
		 * This is the Toshiba XM3332 meaning.
		 */
		if(dk->dk_inqtyp[0] == INV_CDROM && add_sense == 0x89)
			add_msg = "Tried to read audio track as data";
		else
			add_msg = NULL;
	}

	if (sense_key != SC_NOSENSE && sense_key != SC_UNIT_ATTN &&
	    sense_key != SC_ERR_RECOVERED && sense_key != SC_NOT_READY)
		dk_pmsg(dev, "[Alert] %s", key_msg);
	else
		dk_pmsg(dev, "%s", key_msg);
	cmn_err(CE_CONT, ": %s (asc=0x%x, asq=0x%x)", add_msg ? add_msg : "",
		add_sense, asq);
	if(sense_data[0] & 1) 	/* in case buffer writes enabled! */
		cmn_err(CE_CONT, " (deferred error)");

	/* SCSI-2 drives give more info if SKSV bit set */
	if(sense_length > 17 && sense_data[15] & 0x80) {
		cmn_err(CE_CONT, ", (%s byte %d)", 
			(sense_data[15] & 0x40) ? "cmd" : "data",
			(sense_data[16] << 8) | (sense_data[17]));
		if(sense_data[15] & 0x8)
			cmn_err(CE_CONT, " (bit %d)", 
				sense_data[15] & 0x7);
	}

	/* If the valid bit is set, bytes 3-6 give addition info */
	if((sense_data[0] & 0x80) && (sense_length > 6)) {
		vertex_hdl_t	dev_vhdl = dev;
		part_info = scsi_part_info_get(dev_vhdl);
		part = SPI_PART(part_info);

		sense_lba = (sense_data[3] << 24) |
			    (sense_data[4] << 16) |
			    (sense_data[5] << 8)  |
			    (sense_data[6]);
		
		lba = sense_lba -
		      dk->dk_vh.vh_pt[part].pt_firstlbn;
		/*
		 * Report both the block in the partition, and the
		 * disk block #, because fx wants to spare by disk
		 * block #, not relative to partition...
		 */
		cmn_err(CE_CONT, ", Block #%d", lba);
		if(lba != sense_lba)
			cmn_err(CE_CONT, " (%d)", sense_lba);
	}
#if SAMMY
	if (sense_length > 31) {
		printf(" addsense 18-31:");
		for (asq = 18; asq < 32; asq++)
			printf(" 0x%x", sense_data[asq]);
	}
#endif

	if (sense_key == SC_ILLEGALREQ) {
		cmn_err(CE_CONT, "  CDB:");
		for (part = 0; part < sp->sr_cmdlen; part++)
			cmn_err(CE_CONT, " %x", sp->sr_command[part]);
	}
	cmn_err(CE_CONT, "\n");
}

/*
 * Acquire use of the drive info structure and allocate a channel structure
 * Don't do it if we are dumping though!
 */
static scsi_request_t *
drive_acquire(struct dksoftc *dk, int sleep)
{
	scsi_request_t *sp;

	if(dksc_dumpdev) {
		sp = dk->dk_subchan;
		sp->sr_flags = SRF_AEN_ACK; 
		sp->sr_dev = 0;
		sp->sr_buffer = NULL;
		return sp;
	}

	if (sleep)
		DKSCLOCK(&dk->dk_lock);
	while (dk->dk_curq >= dk->dk_qdepth) {  
#ifdef DEBUG
		if (dkdebug) printf("dk%x qfull. ", dk->dk_buf.b_edev);
#endif
		if (!sleep)
			return NULL;
		sv_wait(&dk->dk_wait, PRIBIO, &dk->dk_lock, 0);
		DKSCLOCK(&dk->dk_lock);
	}
	dk->dk_curq++;
	if(dk->dk_curq > dk->dk_maxq) {
		dk->dk_maxq = dk->dk_curq;
#ifdef DEBUG
		if(dkdebug) printf("newmaxq=%d ", dk->dk_maxq);
#endif
	}
	sp = dk->dk_freereq;
	dk->dk_freereq = sp->sr_dev;
	if (sleep)
		DKSCUNLOCK(&dk->dk_lock);

	sp->sr_flags = SRF_AEN_ACK; 
	sp->sr_dev = 0;		/* we use sr_dev for retry count */
	sp->sr_tag = 0;
	sp->sr_bp = NULL;
	sp->sr_buffer = NULL;

	return sp;
}


/*
 * Release the drive and start up someone waiting for it.
 */
static void
drive_release(struct dksoftc *dk, scsi_request_t *sp)
{
	DKSCLOCK(&dk->dk_lock);
	dkrelsubchan(dk, sp);
	DKSCUNLOCK(&dk->dk_lock);
}


/* must be called with dk_lock held!, caller is responsible for
 * releasing the lock */
static void
dkrelsubchan(struct dksoftc *dk, scsi_request_t *sp)
{
	sp->sr_dev = dk->dk_freereq;
	dk->dk_freereq = sp;
	dk->dk_curq--;

	/* if someone waiting for a subchan struct, wake them up */
	if (!sv_signal(&dk->dk_wait))  {
	        /*
		 * If the DK_RECURSE bit is set, then the last time we
		 * were here and called dksccommand, that command short
		 * circuited and got us back here. If this is allowed to
		 * continue we will overflow the interrupt stack. So if
		 * recursion is taking place, don't call command
		 */
	        if ((dk->dk_flags & DK_RECURSE) == 0) {
		    dk->dk_flags |= DK_RECURSE;
 		    dksccommand(dk);
		    dk->dk_flags &= ~DK_RECURSE;  /* turn off recurse bit */
		}
	 }
}


/*ARGSUSED*/
static void
/*VARARGS2*/
dk_pmsg(dev_t dev, char *fmt, ...)
{
	va_list		  ap;
	inventory_t      *pinv;
	uint64_t	  node = 0, port;
	scsi_part_info_t *part_info;
        vertex_hdl_t      disk_vhdl;
	vertex_hdl_t	  dev_vhdl;
	int		  rc;
	char		  part[4];
	uchar_t		  partnum;

	dev_vhdl  = dev;
	part_info = scsi_part_info_get(dev_vhdl);
	disk_vhdl = SDI_DISK_VHDL(SPI_DISK_INFO(part_info));

	rc = hwgraph_info_get_LBL(disk_vhdl, INFO_LBL_INVENT, (arbitrary_info_t *) &pinv);
	if (rc != GRAPH_SUCCESS || pinv->inv_controller == (major_t) -1)
	{
		cmn_err(CE_CONT, "%v: ", dev_vhdl);
		goto output_message;
	}
	if (pinv->inv_class == INV_FCNODE)
	{
		node = ((uint64_t) pinv->inv_type << 32) | (uint32_t) pinv->inv_controller;
		port = ((uint64_t) pinv->inv_unit << 32) | (uint32_t) pinv->inv_state;
		pinv = pinv->inv_next;
	}

	/*
	 * Find the partition number
	 */
	partnum	= SPI_PART(part_info);
	switch(partnum)
	{
	case SCSI_DISK_VH_PARTITION:
		sprintf(part, "vh");
		break;
	case SCSI_DISK_VOL_PARTITION:
		sprintf(part, "vol");
		break;
	default:
		sprintf(part,"s%d",partnum);
	}

	if (node == 0)
	{
		if (pinv->inv_state & INV_RAID5_LUN)
			cmn_err(CE_CONT, "dksc%dd%dl%d%s: ", 
				pinv->inv_controller,
				pinv->inv_unit,
				pinv->inv_state & 0xff,
				part);
		else
			cmn_err(CE_CONT, "dksc%dd%d%s: ", 
				pinv->inv_controller,
				pinv->inv_unit,
				part);
	}
	else
		cmn_err(CE_CONT, "dksc %llx/lun%d%s/c%dp%llx: ",
			node,
			pinv->inv_state & 0xff,
			part,
			pinv->inv_controller,
			port);

output_message:
	va_start(ap,fmt);
	icmn_err(CE_CONT, fmt, ap);
	va_end(ap);
}

/* this is used to retry a command when a drive returns busy; called from
     a timeout.
*/
static void
dk_do_retry(register scsi_request_t *sp)
{

	scsi_disk_info_t	*disk_info;
	struct dksoftc 		*dk;

	disk_info	= scsi_disk_info_get(sp->sr_dev_vhdl);
	dk 		= SDI_DKSOFTC(disk_info);
	dkscstart(dk, (buf_t *)sp->sr_bp, sp);
}


/* return drive capacity in blocks; used for validating vh.
 * Someday we might use use the block size info also.
 */
static int
dk_readcap(register struct dksoftc *dk)
{
    int oldcap; 

    oldcap = dk->dk_drivecap[0];

    /* dk_drivecap is dynamically allocated for proper cache-alignment
       since it is used as a DMA buffer below */

    if(dk_cmd(dk, dk_readcapacity, SC_CLASS1_SZ, DKSC_TIMEOUT, 
	    (caddr_t)dk->dk_drivecap, DK_DRIVECAP_SIZE, B_READ)) {
	    dk->dk_drivecap[0] = oldcap;	/* restore old, in case
		    trash placed in capacity */
	    return -1;
    }

    /* increment by one to get capacity, not last block */
    return ++dk->dk_drivecap[0];
}

/*
 *	check vh for validity in terms of partitions not exceeding drive
 *	capacity.  is_vh() should already be successful.  This is basicly
 *	a sanity check, and deals with the old format Toshiba drives that
 *	claimed a size too large, which mucks up the 3.3 mkfs/fsck that
 *	puts the new bitmaps at the end of the partition.
 *
 *	This routine is single-threaded.
 */
static
dk_chkptsize(struct dksoftc *dk, struct volume_header *vh)
{
	__uint64_t cap;
	int pn, change = 0;
	register struct volume_header *cvh;
	register struct partition_table *pt;

	cvh = vh ? vh: &dk->dk_vh;
	pt = &cvh->vh_pt[0];

	if((cap=dk_readcap(dk)) == -1) {
		    return 0;
	}
	dk->dk_bytecap = cap * dk->dk_blksz;

	/* drivecap never used to be filled in (new in kudzu), and we want it
	 * always be right, so set it here; also simplifies life for programs
	 * do DIOCGETVH at some point. */
	if (cap != cvh->vh_dp.dp_drivecap) {
		cvh->vh_dp.dp_drivecap = cap;
		/*
		 * Change whichever volume header is being examined here because
		 * it's clearly not current and if it's not the master dk version,
		 * it's likely one that will be copied onto the master version if
		 * this routine approves of the volume header.
		*/
		change = 1;
	}

	if (!vh) {
		/*
		 * Grab the lock if we're really going to start messing
		 * around with the real dk vh partition info.
		 */
		DKSCLOCK(&dk->dk_lock);
	}

	for(pn=0; pn<DK_MAX_PART; pn++,pt++) {
		if (pt->pt_firstlbn < 0 || pt->pt_nblks < 0) {
			if (!vh) {
				change = 1;
				pt->pt_firstlbn = 0;
				pt->pt_nblks = 0;
			}
			else
				return 0;
		}
		if ((pt->pt_firstlbn + pt->pt_nblks) > cap) {
			if(!vh) {
				change = 1;
				if(pt->pt_firstlbn > cap) 
					pt->pt_firstlbn = cap;
				if(pt->pt_nblks) 
					pt->pt_nblks = cap - pt->pt_firstlbn;
			}
			else
				return 0;	/* checking validity for a setvh */
		}
	}
	if (cvh->vh_pt[SCSI_DISK_VOL_PARTITION].pt_firstlbn != 0) {
		if (!vh) {
			change = 1;
			cvh->vh_pt[SCSI_DISK_VOL_PARTITION].pt_firstlbn = 0;
		}
		else
			return 0;
	}

	if (!vh) {
		DKSCUNLOCK(&dk->dk_lock);
	}

	if(change) 	/* need to recalc the chksum */
	{
		cvh->vh_csum = 0;
		cvh->vh_csum = -vh_checksum(cvh);
	}
	return 1;
}


/*
 * This routine is used at open time if queueing is enabled for this drive.  
 * We check to make sure that the QERR bit is set.  If it isn't, we clear
 * the queueing bit.
 */
static void
dk_checkqueue(register struct dksoftc *dk, uint qflag)
{
	struct mode_sense_data *msd;
	u_char sense_cmd[SC_CLASS0_SZ];

	msd = (struct mode_sense_data *)
		kmem_zalloc(sizeof(*msd), KM_SLEEP | KM_CACHEALIGN);
	
	bcopy(dk_mode_sense, sense_cmd, sizeof(sense_cmd));
	sense_cmd[2] = QUEUE_PARAM | CURRENT;
	sense_cmd[4] = sizeof(struct queueparam) + 12;

	if (dk_cmd(dk, sense_cmd, SC_CLASS0_SZ, DKSC_TIMEOUT,
	           (caddr_t)msd, sense_cmd[4], B_READ))
	{
		goto out;
	}

        /* Drives for pre 6.2 systems have qerr=1.
         * If the low level driver (wd95, scip) support this setting, allow
         * queuing.
         * Drives for post 6.2 systems will have qerr=0.
         * If the low level driver (adp78, ql) support this setting, allow
         * queuing.
	 * If a driver allows either setting of QERR, tell the driver
	 * the setting of qerr for the drive so that it handles errors
	 * correctly.  
	 */
        if (((msd->dk_pages.queueparam.q_err == 0)  &&
             ((qflag & (SRH_TAGQ|SRH_QERR0)) == (SRH_TAGQ|SRH_QERR0))) ||
            ((msd->dk_pages.queueparam.q_err == 1) &&
             ((qflag & (SRH_TAGQ|SRH_QERR1)) == (SRH_TAGQ|SRH_QERR1))))
	{
		DKSCLOCK(&dk->dk_lock);
		dk->dk_flags |= DK_QUEUE;
		DKSCUNLOCK(&dk->dk_lock);

		if ((qflag & (SRH_TAGQ|SRH_QERR0|SRH_QERR1)) == (SRH_TAGQ|SRH_QERR0|SRH_QERR1))
		{
			scsi_lun_info_t   *lun_info = scsi_lun_info_get(dk->dk_lun_vhdl);
			struct scsi_ha_op *ha_ioctl;
	  
			ha_ioctl = kmem_zalloc(sizeof(*ha_ioctl), KM_SLEEP);
			ha_ioctl->sb_opt = LUN_QERR;
			ha_ioctl->sb_arg = (SLI_TARG(lun_info) << SOP_QERR_TARGET_SHFT) | 
					   (SLI_LUN(lun_info) << SOP_QERR_LUN_SHFT) | 
					   (msd->dk_pages.queueparam.q_err << SOP_QERR_VAL_SHFT);
			SLI_IOCTL(lun_info)(SLI_CTLR_VHDL(lun_info), SOP_SET_BEHAVE, ha_ioctl);
			kern_free(ha_ioctl);
		}
        }
	else
	{
		DKSCLOCK(&dk->dk_lock);
		dk->dk_flags &= ~DK_QUEUE;
		DKSCUNLOCK(&dk->dk_lock);

		if (qflag & SRH_TAGQ)
			dk_pmsg(dk->dk_buf.b_edev,
				"low level driver does not support QERR=%d\n",
				msd->dk_pages.queueparam.q_err);
        }

out:
	kern_free(msd);
}

/*
 *  This is used at open time to get the blksize, before the vh is read.
 *  After the vh is read, or when the vh ioctl's are used, the blksz
 *  from the vh is used.  This routine is also called after the blocksize
 *  is potentially changed via ioctl.
 *  Since not all devices support the FORMAT page, ask for all pages;
 *  Uses the value from the block descr if supplied, since that
 *  location is correct for all drives, not just ccs or scsi2.  If not,
 *  use the value from the format page.
 *  If modesense is successful, set or clear the WRTPROT bit.  This is
 *  mainly for early detection of devices that are either write protected,
 *  or don't support the write command (such as CDROM).  Otherwise may
 *  get SCSI errors.
 *
 *  The return value is whether or not the actual blocksize changed from the
 *  blocksize value kept in the vh, even though at this actual moment it
 *  is not used. -mac
*/
static int
dk_getblksz(register struct dksoftc *dk)
{
    struct mode_sense_data *msd;
    u_char sense_cmd[SC_CLASS0_SZ];
    int	blocksize = 0;
    int return_value;

    msd = (struct mode_sense_data *)
	    kmem_zalloc(sizeof(*msd), KM_SLEEP | KM_CACHEALIGN);
    
    bcopy(dk_mode_sense, sense_cmd, sizeof(sense_cmd));
    sense_cmd[2] = msd->dk_pages.common.pg_code = ALL|CURRENT;
    sense_cmd[4] = msd->dk_pages.common.pg_len =
	    sizeof(msd->dk_pages.common.pg_maxlen);

    if(dk_cmd(dk, sense_cmd, SC_CLASS0_SZ, DKSC_TIMEOUT,
	    (caddr_t)msd, sense_cmd[4], B_READ))
	    goto failure;

    DKSCLOCK(&dk->dk_lock);
    if(msd->wprot)
	    dk->dk_flags |= DK_WRTPROT;
    else
	    dk->dk_flags &= ~DK_WRTPROT;
    DKSCUNLOCK(&dk->dk_lock);

    if(msd->bd_len && (msd->block_descrip[5] ||
	    msd->block_descrip[6] || msd->block_descrip[7]))
	    blocksize = ((unsigned)msd->block_descrip[5]<<16) +
		    ((unsigned)msd->block_descrip[6]<<8) + msd->block_descrip[7];
    else {	/* see if format page is there */
	    u_char *d = (u_char *)msd, *maxd = d + msd->sense_len;
	    d += msd->bd_len + 4;	/* after setting maxd! */
	    while(d < maxd) {
		    int i;
		    if(((*d) & ALL) == DEV_FORMAT) {
			    blocksize =
				    (((uint)((struct dev_format *)d)->f_bytes_sec[0])<<8) +
					    ((struct dev_format *)d)->f_bytes_sec[1];
			    break;
		    }
		    i = d[1] + 2;	/* skip to next page; +2 for hdr */
		    d += i;
	    }
    }
failure:
    if(blocksize == 0) {
	    blocksize = DK_BLK_SZ;	/* best guess... */
    }
    if (return_value = (dk->dk_blksz != blocksize)) {
#ifdef DEBUG
	    if (dk->dk_blksz != 0)
		    dk_pmsg(dk->dk_buf.b_edev, "blocksize changed from %d to %d\n",dk->dk_blksz,blocksize);
#endif
	    dk->dk_blksz = blocksize;
    }
    if(msd)
	    kern_free(msd);

    return return_value;
}


#ifdef DEBUG
/* dump the vh struct in a readable way */
/* ARGSUSED */
void
dump_vh(struct volume_header *vh, char *head)
{
#ifdef VH_DEBUG
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
    printf("params: drivecap=%u secbyte=%x flags=%x, qdepth=%d ",
		vh->vh_dp.dp_drivecap, vh->vh_dp.dp_secbytes,
		vh->vh_dp.dp_flags, vh->vh_dp.dp_ctq_depth);
    printf("csum=%x, vhcsum=%x\n", vh->vh_csum, vh_checksum(vh));
#endif /* VH_DEBUG */
}

#endif /* DEBUG */

#if SAVESTAT
void
dkstat(int amount)
{
	int i;
	qprintf("adapter %d:  %d total seeks, %d total r+w, %d/%d entries units=%d ms\n",
		statadap, dkseq, dktot, amount, DKSTATSIZE, 1000/HZ);
	for (i = 1; i < amount; i++)
	{
		qprintf("%d ", dkbuffer[i].dev);
		if (dkbuffer[i].queue > dkbuffer[i-1].finish + 1)
			qprintf("I");
		else
			qprintf(".");
		if (dkbuffer[i-1].bn + dkbuffer[i-1].count != dkbuffer[i].bn)
			qprintf("S");
		else
			qprintf(".");
		if (dkbuffer[i].bn < dkbuffer[i-1].bn)
			qprintf("Q");
		else
			qprintf(".");
		if(dkbuffer[i].flags & B_READ)
			qprintf("R");
		else
			qprintf("W");
		if(dkbuffer[i].queue == dkbuffer[i].start)
			qprintf(" queue=start=%d, ", dkbuffer[i].queue);
		else
			qprintf(" wait=%d queue=%d ",
				dkbuffer[i].start - dkbuffer[i].queue, dkbuffer[i].queue);
		qprintf("active=%d bn=%d count=%d\n",
		       dkbuffer[i].finish-dkbuffer[i].start, 
		       dkbuffer[i].bn, dkbuffer[i].count);
	}
}

void
dkstatpr()
{
	dkstat(dkloc);
}
#else
/* dummy for idbg */
void
dkstatpr()
{
}
#endif

#if _MIPS_SIM == _ABI64
static void
irix5_dkioctl_to_dkioctl(struct irix5_dk_ioctl_data *i5dp, 
	struct dk_ioctl_data *dp)
{
	dp->i_addr = (caddr_t)(__psint_t)i5dp->i_addr;
	dp->i_len = i5dp->i_len;
	dp->i_page = i5dp->i_page;
}
#endif /* _ABI64 */


/*
** dk_clr_flags and dk_set_flags used by failover.c
*/

int
dk_clr_flags (vertex_hdl_t lun_vhdl, int dk_flags)
{
	int error = -1;
	vertex_hdl_t disk_vhdl;
	scsi_disk_info_t *disk_info;
	struct dksoftc *dk;

	if (hwgraph_traverse (lun_vhdl,EDGE_LBL_DISK,&disk_vhdl) == GRAPH_SUCCESS) {
		disk_info = scsi_disk_info_get (disk_vhdl);
		if (disk_info) {
			dk = SDI_DKSOFTC(disk_info);
			if (dk) {
				DKSCLOCK(&dk->dk_lock);
				dk->dk_flags &= ~dk_flags;
				DKSCUNLOCK(&dk->dk_lock);
				error = 0;
			}
		}
		hwgraph_vertex_unref (disk_vhdl);
	}
	return error;
}

int
dk_set_flags (vertex_hdl_t lun_vhdl, int dk_flags)
{
	int error = -1;
	vertex_hdl_t disk_vhdl;
	scsi_disk_info_t *disk_info;
	struct dksoftc *dk;

	if (hwgraph_traverse (lun_vhdl,EDGE_LBL_DISK,&disk_vhdl) == GRAPH_SUCCESS) {
		disk_info = scsi_disk_info_get (disk_vhdl);
		if (disk_info) {
			dk = SDI_DKSOFTC(disk_info);
			if (dk) {
				DKSCLOCK(&dk->dk_lock);
				dk->dk_flags |= dk_flags;
				DKSCUNLOCK(&dk->dk_lock);
				error = 0;
			}
		}
		hwgraph_vertex_unref (disk_vhdl);
	}
	return error;
}


/*
 * allocate dynamic request queue data
 */
int
dk_alloc_ioreq(struct dksoftc *dk, vertex_hdl_t disk_vhdl, int newqueue)
{
	register 		 i;
	u_char 			*cmddata;
	scsi_request_t 		*sp;
	int			 err = 0;
	vertex_hdl_t		 lun_vhdl;
	scsi_disk_info_t	*disk_info;
	int			 add_ioreq;

	disk_info = scsi_disk_info_get(disk_vhdl);
	lun_vhdl = SDI_LUN_VHDL(disk_info);
	if ((err = SDI_ALLOC(disk_info)(lun_vhdl, newqueue, NULL)) != SCSIALLOCOK){
		return err;
	} else {
		/*
		 * We had previously done a successful alloc, so if the one just above
		 * succeeds, we will have done two.  So we need to do a free to maintain
		 * proper reference counting.
		 */
		(SDI_FREE(disk_info))(lun_vhdl,NULL);
	}

	/* SC_CLASS1_SZ has to be the longest cmd len we use in the driver */
	ASSERT((sizeof(scsi_request_t) % 4) == 0);
	add_ioreq = newqueue - dk->dk_ioreq;
	sp = ( scsi_request_t *) kern_calloc(
		add_ioreq * sizeof(scsi_request_t) +
		roundup(add_ioreq * SC_CLASS1_SZ, 4), 1);

	cmddata = (u_char *) &sp[add_ioreq];
	for(i = 0; i < add_ioreq; i++) {
		sp[i].sr_dev = &sp[i+1];
		sp[i].sr_sense = kmem_alloc(SCSI_SENSE_LEN, KM_CACHEALIGN);
		sp[i].sr_senselen = SCSI_SENSE_LEN;
		sp[i].sr_command = cmddata;
		/* for interrupt routines */
		cmddata += SC_CLASS1_SZ;
	}
	subchaninit(disk_vhdl, sp, add_ioreq);
	dk->dk_ioreq += add_ioreq;

	DKSCLOCK(&dk->dk_lock);
	sp[add_ioreq - 1].sr_dev = dk->dk_freereq;
	dk->dk_freereq = sp;
	DKSCUNLOCK(&dk->dk_lock);

	return(err);
}


/*
 * Receives callback notification. Currently only registered for CDROM
 * devices.  
 */
void
dk_callback_frontend(vertex_hdl_t lun_vhdl, char *sense)
{
	struct dksoftc 		*dk;
	scsi_disk_info_t	*disk_info;
	vertex_hdl_t		disk_vhdl;
	/* REFERENCED */
	int			rc;
	
	/*
	 * We only care about UNIT ATTENTION
	 */
	if ((sense[2] & 0x0f) == 0x06)
	{	
		rc = hwgraph_traverse(lun_vhdl, EDGE_LBL_DISK, &disk_vhdl);
		ASSERT(rc == GRAPH_SUCCESS);
		disk_info = scsi_disk_info_get(disk_vhdl);
		dk = SDI_DKSOFTC(disk_info);
		hwgraph_vertex_unref(disk_vhdl);
		
		/* 
		 * Make sure block size has been determined. If the
		 * blocksize is unknown at this point, don't do a
		 * callback !!  
		 */
		if (dk->dk_blksz == 0)
			return;
		
		/* 
		 * Turn on the "need to set blocksize" flag;
		 * We take the lazy approach in that the command
		 * will be issued prior to the next regular command
		 * that would be issued.  
		 */
		DKSCLOCK(&dk->dk_lock);
		dk->dk_flags |= DK_NEED_BLKSZ;
		DKSCUNLOCK(&dk->dk_lock);
	}
}

/* 
 * This routine is the backend of the lazy callback implementation,
 * and is used for two purposes:
 * 1. to set CD-ROM blocksize back to what it was prior to Unit Attention
 * 2. to issue a start unit to a drive that is spun down
 *
 * Called with the dk_lock held.  If wait is set, the lock will be released
 * prior to calling drive_acquire, and will return with the lock released.
 *
 * If wait is set, we sleep until command is complete; otherwise command
 * is scheduled and we return.
 * Return Value:
 *   1 - the command was successfully scheduled/completed
 *   0 - the command was not scheduled/completed.  
 */
static int
dk_callback_backend(register struct dksoftc *dk, int wait)
{
	struct mode_sense_data	*msd;
	u_char			select_cmd[SC_CLASS0_SZ] = {0x15, 0x10, 0, 0, 12, 0 };
	struct scsi_request	*sp;
	scsi_lun_info_t		*lun_info;
	int			temp;
	int			blksz = dk->dk_blksz;
	ushort			action;

	if (dk->dk_flags & DK_NEEDSTART)
		action = DK_NEEDSTART;
	else
		action = DK_NEED_BLKSZ;

	if (wait) {
		dk->dk_flags |= DK_HOLD;
		dk->dk_flags &= ~action;
		DKSCUNLOCK(&dk->dk_lock);
	}

	if ((sp = drive_acquire(dk, wait)) == NULL)
		return 0;

	/*
	 * There is a 12-byte buffer in the dksoftc structure for this
	 * mode select.  The mode_sense_data structure is bigger, but no
	 * pages are used in this case, so only 12 bytes are required.
	 */
	if (action == DK_NEED_BLKSZ) {
		msd = (struct mode_sense_data *) dk->msel_bd; /* only 12 bytes used */
		msd->bd_len = sizeof(msd->block_descrip);       /* 1 descrip */
		msd->block_descrip[5] = (u_char)(blksz >> 16);
		msd->block_descrip[6] = (u_char)(blksz >> 8);
		msd->block_descrip[7] = (u_char)blksz;
	}

	temp = 0;
	do {
		sp->sr_cmdlen = SC_CLASS0_SZ;

		if (action == DK_NEEDSTART) {
			bcopy(dk_startunit, sp->sr_command, SC_CLASS0_SZ);
			sp->sr_buflen = 0;
			/*
			 * Allow 240 seconds; based on drives that may have spinup delays
			 * set based on ID; ID 15 waits 15 * intv, where intv is typically
			 * 10-15 seconds; also allow a bit of slop.
			 */
			sp->sr_timeout = 240 * HZ;
			if (temp++)
				dk_pmsg(dk->dk_buf.b_edev, "Error on device spin up,"
					" retrying\n");
			else
				dk_pmsg(dk->dk_buf.b_edev, "Device not ready, "
					"spinning up\n");
		}
		else /* action == DK_NEED_BLKSZ */ {
			bcopy(select_cmd, sp->sr_command, sp->sr_cmdlen);
			sp->sr_buffer   = (u_char *)msd;
			sp->sr_buflen   = select_cmd[4];
			sp->sr_flags   |= SRF_FLUSH;
			sp->sr_timeout  = DKSC_TIMEOUT;
		}

		if (wait)
			dk_sendcmd(dk, sp);
		else {
			dk->dk_flags |= DK_HOLD;
			dk->dk_flags &= ~action;
			DKSCUNLOCK(&dk->dk_lock);
			lun_info = scsi_lun_info_get(sp->sr_lun_vhdl);
			sp->sr_notify = dk_callback_intr;
			SLI_COMMAND(lun_info)(sp);
			DKSCLOCK(&dk->dk_lock);
			return 1;
		}
	} while (dk_chkcond(dk, sp, 0, 0) == 1);

	temp = !(sp->sr_status || sp->sr_scsi_status);

	/*
	 * Release those waiting for callback to complete
	 */
	DKSCLOCK(&dk->dk_lock);
	if (action == DK_NEEDSTART) {
		if (temp) {
			dk->dk_flags &= ~DK_WONT_START;
			dk_pmsg(dk->dk_buf.b_edev, "Device spun up successfully\n");
		}
		else {
			dk_pmsg(dk->dk_buf.b_edev, "Device spin up failed, unable"
				" to use device -- corrective action necessary\n");
			dk->dk_flags |= DK_WONT_START;
			dkscpurgequeues(dk);
		}
	}
	dk->dk_flags &= ~DK_HOLD;
	dkrelsubchan(dk, sp);
	sv_broadcast(&dk->dk_hold);
	DKSCUNLOCK(&dk->dk_lock);

	return temp;
}

/* ARGSUSED */
static void
dk_callback_intr(scsi_request_t *sp)
{
	register struct dksoftc	*dk;
	scsi_disk_info_t	*disk_info;
	scsi_lun_info_t		*lun_info;
	ushort			action;

	if (sp->sr_command[0] == 0x1B)
		action = DK_NEEDSTART;
	else
		action = DK_NEED_BLKSZ;

	disk_info = scsi_disk_info_get(sp->sr_dev_vhdl);
	dk = SDI_DKSOFTC(disk_info);
	if (dk_chkcond(dk, sp, 0, 0) == 1) {
		lun_info = scsi_lun_info_get(sp->sr_lun_vhdl);
		if (action == DK_NEEDSTART)
			dk_pmsg(dk->dk_buf.b_edev,
				"Error spinning up device, retrying\n");
		SLI_COMMAND(lun_info)(sp);
		return;
	}

	DKSCLOCK(&dk->dk_lock);
	/*
	 * Check to see if callback is for Start Unit or Mode Select.
	 */
	if (action == DK_NEEDSTART) {
		if (sp->sr_status || sp->sr_scsi_status) {
			dk_pmsg(dk->dk_buf.b_edev, "Unable to spin up device, "
				"device unusable -- corrective action necessary\n");
			dk->dk_flags |= DK_WONT_START;
			dkscpurgequeues(dk);
		}
		else {
			dk->dk_flags &= ~DK_WONT_START;
			dk_pmsg(dk->dk_buf.b_edev, "Device successfully spun up\n");
		}
	}

	/*
	 * Release those waiting for callback to complete
	 */
	dk->dk_flags &= ~DK_HOLD;
	dkrelsubchan(dk, sp);
	sv_broadcast(&dk->dk_hold);
	DKSCUNLOCK(&dk->dk_lock);
	return;
}

/*
 * The incoming name designates a disk, or a partition on a
 * disk. Return a name for the designated special and type on
 * the same disk. NOTE: reuses the name buffer, which needs
 * to be at least MAXDEVNAME bytes long.
 *
 * GRAPH BASED ALGORITHM: find the vertex corresponding to
 * "name", then hop up connection points until you get a vertex
 * that looks like a disk. Chose it or its parent, depending on
 * "withtype"; grab the canonical name of that vertex, and tack
 * on "special" and "type" appropriately.
 */
char *
dksc_partition_to_canonical_special(char *name, char *special)
{
#if DEBUG
	static int callcount = 0;
#endif
	graph_error_t	rc;
	vertex_hdl_t	base_vhdl;
	vertex_hdl_t	disk_vhdl;
	vertex_hdl_t	conn_vhdl;
	char	       *n;
	char	       *s;
	char	       *r;

#if DEBUG
	callcount ++;
#endif

	n = name;
	if (n[0] == '/')
		n += 1;
	if ((n[0] == 'h') && (n[1] == 'w') && (n[2] == '/'))
		n += 3;

	/* find the last vertex in name that exists.
	 * the first time we open a disk, the partition
	 * vertex does not exist, and this will hand us
	 * the disk vertex directly; afterward, we will
	 * get the partition vertex here. But, we don't
	 * depend on this ...
	 */
	base_vhdl = hwgraph_root;
	rc = hwgraph_path_lookup(hwgraph_root, n, &base_vhdl, &r);
	if ((rc != GRAPH_SUCCESS) && (rc != GRAPH_NOT_FOUND)) {
#if DEBUG
		if (callcount == 1)
			cmn_err(CE_ALERT,
				"partitiontospecial(%v): "
				"hwgraph_path_lookup(%s) returns %d",
				base_vhdl, name, rc);
#endif
		cmn_err(CE_ALERT,
			"partitiontospecial: bad name '%s' (r=%s)",
			name, r);
		return NULL;
	}
#if DEBUG && HWG_DEBUG
	if (callcount == 1)
		cmn_err(CE_CONT,
			"partitiontospecial(%v) found me via '%s', "
			"remainder: %s\n",
			base_vhdl, name, r);
#endif

	/* work back up the graph until we find the
	 * vertex that has the EDGE_LBL_DISK edge.
	 */
	conn_vhdl = base_vhdl;
	hwgraph_vertex_unref(base_vhdl);
	while (GRAPH_SUCCESS != hwgraph_edge_get(conn_vhdl, EDGE_LBL_DISK, &disk_vhdl)) {
		if (GRAPH_VERTEX_NONE == (conn_vhdl = hwgraph_connectpt_get(conn_vhdl))) {
#if DEBUG
			if (callcount == 1)
				cmn_err(CE_ALERT,
					"partitiontospecial(%v): never saw "
					EDGE_LBL_DISK " edge.",
					base_vhdl);
#endif
			cmn_err(CE_ALERT,
				"partitiontospecial: bad name '%s'",
				name);
			return NULL;
		} else
			hwgraph_vertex_unref(conn_vhdl);
	}
#if DEBUG && HWG_DEBUG
	if (callcount == 1)
		cmn_err(CE_CONT,
			"partitiontospecial(%v) sees my "
			EDGE_LBL_DISK " edge.\n",
			conn_vhdl);
#endif
	n = vertex_to_name(disk_vhdl, name, MAXDEVNAME);
	hwgraph_vertex_unref(disk_vhdl);
	s = n + strlen(n);
	sprintf(s, "/%s/%s", special, EDGE_LBL_CHAR);

	return n;
}


/*
 * This is a hack until the iograph program is working so that we have
 * enough /hw/disk hwgraph entries for the miniroot to do its stuff.
 * It takes a hwgraph path for the root disk and turns it into /hw/disk entries
 */
static int
get_hw_scsi_info(dev_t vertex, char *contr, char *targ, char *node,
		 char *port, char *lun, char *part)
{
	scsi_part_info_t	*part_info;
	scsi_disk_info_t	*disk_info;
	int			 partnum, targnum, lunnum;
	uint64_t		 nodenum, portnum;
	vertex_hdl_t		 dev_vhdl;
	
	dev_vhdl 	= vertex;
	part_info 	= scsi_part_info_get(dev_vhdl);
	if(!part_info){
		contr[0] = '\0';
		return -1;
	}
	disk_info	= SPI_DISK_INFO(part_info);
	if (contr) {
		dev_t ctl;
		int cntnum;
		/*
		 * Find the controler number
		 */
		ctl = SDI_CTLR_VHDL(disk_info);
		/*
		 * No need for a loop as scsi controllers only have one invent struct
		 */

		if ((cntnum = device_controller_num_get(ctl)) >= 0){
			sprintf(contr,"%d", cntnum);
		} else {
			return -1;
		}
	}
	if (targ) {
		/*
		 * Find the target number
		 */
		targnum	= SDI_TARG(disk_info);
		sprintf(targ,"%d",targnum);
	}
	if (node && port) {
		nodenum = SDI_NODE(disk_info);
		sprintf(node, "%llx", nodenum);
		portnum = SDI_PORT(disk_info);
		sprintf(port, "p%llx", portnum);
	}
	if (lun) {
		/*
		 * Find the lun number
		 */
		lunnum = SDI_LUN(disk_info);
		sprintf(lun,"%d",lunnum);
	}
	if (part) {
		/*
		 * Find the partition number
		 */
		partnum	= SPI_PART(part_info);
		switch(partnum) {
		case SCSI_DISK_VH_PARTITION:
			sprintf(part,"vh");
			break;
		case SCSI_DISK_VOL_PARTITION:
			sprintf(part,"vol");
			break;
		default:
			sprintf(part,"s%d",partnum);
		}
	}
	return 0;
}

/*
 * dk_disk_stat_info_add
 *	Attaches labelled information for the disk io statistics
 *	at the disk vertex. This information is actually stored
 *	also can be accessed through the  indexed information 
 *	for the same disk vertex.
 */
static void
dk_disk_stat_info_add(vertex_hdl_t disk_vhdl) 
{
	scsi_disk_info_t	*disk_info = scsi_disk_info_get(disk_vhdl);
	struct iotime 		*dkiotime;
	vertex_hdl_t		 vol_vhdl;
	graph_error_t		 rv;

	if (SDI_DKIOTIME(disk_info) != NULL)
		return;

	/* for the volume/char vertex attach labelled
	 * information for the dkiotime . this is useful
	 * for programs like sar , rpc.rstatd which need
	 * the dkiotime info. export of this dkiotime info
	 * is also done so that user programs can do an
	 * attr_get to access this easily .
	 */
	dkiotime = kmem_zalloc(sizeof(struct iotime), VM_DIRECT);
	SDI_DKIOTIME(disk_info) = dkiotime;
	rv = hwgraph_traverse(disk_vhdl,
			      EDGE_LBL_VOLUME"/"EDGE_LBL_CHAR,
			      &vol_vhdl);
	ASSERT_ALWAYS(rv == GRAPH_SUCCESS);
	rv = hwgraph_info_add_LBL(vol_vhdl,
				  INFO_LBL_DKIOTIME,
				  (arbitrary_info_t) dkiotime);
	ASSERT_ALWAYS(rv == GRAPH_SUCCESS);
	rv = hwgraph_info_export_LBL(vol_vhdl,
				     INFO_LBL_DKIOTIME,
				     sizeof(struct iotime));
	ASSERT_ALWAYS(rv == GRAPH_SUCCESS);
	hwgraph_vertex_unref(vol_vhdl);
}

static void
dk_disk_stat_info_remove(vertex_hdl_t disk_vhdl)
{
	scsi_disk_info_t	*disk_info = scsi_disk_info_get(disk_vhdl);
	struct iotime 		*dkiotime; 
	vertex_hdl_t		 vol_vhdl;
	/* REFERENCED */
	graph_error_t		 rv;

	rv = hwgraph_traverse(disk_vhdl,
			      EDGE_LBL_VOLUME"/"EDGE_LBL_CHAR,
			      &vol_vhdl);
	ASSERT(rv == GRAPH_SUCCESS);
	rv = hwgraph_info_unexport_LBL(vol_vhdl,
				       INFO_LBL_DKIOTIME);
	ASSERT(rv == GRAPH_SUCCESS);
	rv = hwgraph_info_remove_LBL(vol_vhdl,
				     INFO_LBL_DKIOTIME,
				     (arbitrary_info_t *) &dkiotime);
	ASSERT(rv == GRAPH_SUCCESS);

	ASSERT(dkiotime == (struct iotime *) SDI_DKIOTIME(disk_info));
	SDI_DKIOTIME(disk_info) = NULL;
	kern_free((void *) dkiotime);
	hwgraph_vertex_unref(vol_vhdl);
}

void
dk_add_hw_disk_alias(dev_t vertex, boolean_t block)
{
	char devnm[MAXDEVNAME];
	char contr[NUMOFCHARPERINT];
	char targ[NUMOFCHARPERINT];
	char node[17];
	char port[17];
	char lun[NUMOFCHARPERINT];
	char part[NUMOFCHARPERINT];
	dev_t hwdisk, to;
	int rc;

	if (get_hw_scsi_info(vertex, contr, targ, node, port, lun, part) == -1) {
		hwgraph_vertex_unref(vertex);
		return;
	}
	if (contr[0] == NULL) {
		hwgraph_vertex_unref(vertex);
		return;
	}
	/*
	 *	It is possible to be called for things other then disk partitions.
	 */

	if (block == B_TRUE) {
		if ((hwdisk = hwgraph_path_to_dev("/hw/" EDGE_LBL_DISK)) == NODEV) {
			rc = hwgraph_path_add(hwgraph_root, EDGE_LBL_DISK , &hwdisk);
			ASSERT(rc == GRAPH_SUCCESS); rc = rc;
		}
	} else {
		if ((hwdisk = hwgraph_path_to_dev("/hw/" EDGE_LBL_RDISK)) == NODEV) {
			rc = hwgraph_path_add(hwgraph_root, EDGE_LBL_RDISK , &hwdisk);
			ASSERT(rc == GRAPH_SUCCESS); rc = rc;
		}
	}

	SCSI_ALIAS_LOCK();
	if (strcmp(node, "0") == 0) {
		if (strcmp(lun,"0"))
			sprintf(devnm, "dks%sd%sl%s%s", contr, targ,lun, part);
		else
			sprintf(devnm, "dks%sd%s%s", contr, targ, part);

		/* Grab the disk alias mutex to prevent someone else from 
		 * modifying the "/hw/disk" & "/hw/rdisk" directories
		 * at the same time.
		 */
		if (hwgraph_traverse(hwdisk,
				     devnm,
				     &to) == GRAPH_NOT_FOUND) {
#if DEBUG && HWG_DEBUG
			cmn_err(CE_CONT, "%v: alias /%s/ will point to\n\t%v\n",
				hwdisk,devnm,vertex);
#endif
			rc = hwgraph_edge_add(hwdisk, vertex, devnm);
			ASSERT(rc == GRAPH_SUCCESS); rc = rc;
		} else
			hwgraph_vertex_unref(to);
	}
	else {
		sprintf(devnm, "%s/lun%s%s", node, lun, part);
		rc = hwgraph_path_add(hwdisk, devnm, &to);
		sprintf(devnm, "c%s%s", contr, port);
		hwgraph_edge_add(to, vertex, devnm);
		hwgraph_vertex_unref(to);
	}
	hwgraph_vertex_unref(vertex);
	hwgraph_vertex_unref(hwdisk);

	/* Release the mutex grabbed at the beginning of this routine */
	SCSI_ALIAS_UNLOCK();
}


void
dk_remove_hw_disk_alias(dev_t vertex)
{
	char	contr[NUMOFCHARPERINT];
	char	targ[NUMOFCHARPERINT];
	char	nodename[17];
	char	port[17];
	char	lun[NUMOFCHARPERINT];
	char	part[NUMOFCHARPERINT];
	char	modename[20];
	char	pathname[40];
	vertex_hdl_t alias_base_vhdl;

	if (get_hw_scsi_info(vertex, contr, targ, nodename, port, lun, part) == -1) {
		hwgraph_vertex_unref(vertex);
		return;
	}

	if (strcmp(nodename, "0") != 0)
	{
	    sprintf(modename, "lun%s%s", lun, part);
	    sprintf(pathname, "c%s%s", contr, port);
	    if (part[0] == 's')
	    {
		alias_base_vhdl = hwgraph_path_to_dev("/hw/" EDGE_LBL_DISK);
		if (alias_base_vhdl != NODEV)
		{
		    scsi_alias_edge_remove(alias_base_vhdl, nodename, modename, pathname);
		    hwgraph_vertex_unref(alias_base_vhdl);
		}
	    }
	    alias_base_vhdl = hwgraph_path_to_dev("/hw/" EDGE_LBL_RDISK);
	    if (alias_base_vhdl != NODEV)
	    {
		scsi_alias_edge_remove(alias_base_vhdl, nodename, modename, pathname);
		hwgraph_vertex_unref(alias_base_vhdl);
	    }
	}
	else
	{
	    if (strcmp(lun, "0"))
		sprintf(pathname, "dks%sd%sl%s%s", contr, targ, lun, part);
	    else
		sprintf(pathname, "dks%sd%s%s", contr, targ, part);
	    if (part[0] == 's')
	    {
		alias_base_vhdl = hwgraph_path_to_dev("/hw/" EDGE_LBL_DISK);
		if (alias_base_vhdl != NODEV)
		{
		    hwgraph_edge_remove(alias_base_vhdl, pathname, NULL);
		    hwgraph_vertex_unref(alias_base_vhdl);
		}
	    }
	    alias_base_vhdl = hwgraph_path_to_dev("/hw/" EDGE_LBL_RDISK);
	    if (alias_base_vhdl != NODEV)
	    {
		hwgraph_edge_remove(alias_base_vhdl, pathname, NULL);
		hwgraph_vertex_unref(alias_base_vhdl);
	    }
	}
}


void
dk_alias_make(dev_t part_dev)	
{
	int			part;
	char			name[MAXDEVNAME];
	char			contr[NUMOFCHARPERINT];
	vertex_hdl_t		disk_vhdl = GRAPH_VERTEX_NONE;

	/* get the logical controller number , target & lun info 
	 * from the hardware graph
	 */
	if (get_hw_scsi_info(part_dev, contr, NULL, NULL, NULL, NULL, NULL) == -1)
		return;

	disk_vhdl = SPI_DISK_VHDL(scsi_part_info_get(part_dev));
	ASSERT(disk_vhdl != GRAPH_VERTEX_NONE);
	for(part = 0 ; part < DK_MAX_PART ; part++) {
		vertex_hdl_t	part_vhdl;

		SPRINTF_PART(part,name);
		if (hwgraph_traverse(disk_vhdl,name,&part_vhdl) == GRAPH_SUCCESS) {
			/* the partition is a valid one . go ahead and
			 * create the aliases in the /hw/disk and
			 * /hw/rdisk directories
			 */
			if ((part != SCSI_DISK_VOL_PARTITION) &&
			    (part != SCSI_DISK_VH_PARTITION))
				dk_add_hw_disk_alias(hwgraph_block_device_get(part_vhdl), B_TRUE);
			dk_add_hw_disk_alias(hwgraph_char_device_get(part_vhdl), B_FALSE);
			hwgraph_vertex_unref(part_vhdl);
		}
	}
}

/*
 * Remove partititions and associated info from the graph
 */
void
dk_namespace_remove(vertex_hdl_t disk_vhdl)
{
	int		partition;
	vertex_hdl_t	p_vhdl;

	dk_disk_stat_info_remove(disk_vhdl);
	for(partition = 0; partition < DK_MAX_PART; partition++)
	{
	    if (scsi_part_vertex_exist(disk_vhdl, partition, &p_vhdl))
		scsi_part_vertex_remove(disk_vhdl, partition);
	}
}

/*
 * Add base partitions and associated info to the graph
 */
void
dk_base_namespace_add(vertex_hdl_t disk_vhdl, boolean_t add_aliases)
{
	scsi_part_vertex_add(disk_vhdl, SCSI_DISK_VOL_PARTITION, add_aliases);
	scsi_part_vertex_add(disk_vhdl, SCSI_DISK_VH_PARTITION, add_aliases);
	dk_disk_stat_info_add(disk_vhdl);
}


/*
 * In some cases we don't want the sense to be printed, e.g. a "medium
 * not present" when the device is a CDROM.
 */
int 
dk_user_printable_sense(struct dksoftc *dk, register scsi_request_t *sp, int mute)
{
#if DKSCDEBUG
	return 1;
#else
	int rc = 1;
	u_char key = sp->sr_sense[2] & 0xF;
	u_char addsense = sp->sr_sense[12];
	uint   slen = sp->sr_sensegotten;

	if (dk->dk_inqtyp[1] & 0x80) { /* is removeable */
		switch (key) {
			case SC_NOT_READY: 
				if (slen > 12 && addsense == SC_MEDIA_ABSENT) { /* Medium not present */
					rc = 0;
				}
				break;	
			case SC_BLANKCHK:
				/* This is to suppress sense data printout associated 
				   with read command issued internally by dksc from 
				   dkscopen while attempting to read volume header 
				 */
				if (addsense == 0x64 && mute) {
					rc = 0;
				}
				break;
			default:
				break;
		}
	}
	/*
	 * Don't report 'recovered' errors on defect list; it means the drive doesn't
	 * support the requested format, and is responding with it's default format.
	 * We see this when we ask for logical block format, but drive doesn't support
	 * it, for example.  We don't want to print that; fx will just fall back on
	 * alternate format.  See bug #554444.  IBM  DDRS-39130W is one of those drives.
	 */
	else if (key==SC_ERR_RECOVERED && (addsense == 0x1c || addsense == SC_DEFECT_ERR))
		rc = 0;
	/*
	 * Don't report 'drive not ready' if the command issued was
	 *   a 'test unit ready'.
	 */
	else if (sp->sr_command[0] == 0 && addsense == SC_NOTREADY)
		rc = 0;

	return(rc);
#endif
}
