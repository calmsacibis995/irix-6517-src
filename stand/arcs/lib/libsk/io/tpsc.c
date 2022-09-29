/**************************************************************************
 *									  *
 * 			Copyright (C) 1986, Silicon Graphics, Inc.	  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident	"io/tpsc.c: $Revision: 3.91 $"

#include "sys/debug.h"
#include "sys/sysmacros.h"
#include "sys/types.h"
#include "sys/param.h"
#include "sys/buf.h"
#include "sys/conf.h"
#include "sys/cmn_err.h"
#include "sys/errno.h"
#include "sys/sbd.h"
#include "sys/mtio.h"
#include "sys/scsi.h"
#include "sys/tpsc.h"
#include "sys/dump.h"
#include "sys/invent.h"
#include "sys/immu.h"	/* for btod */
#include <arcs/types.h>
#include <arcs/hinv.h>
#include <arcs/io.h>
#include <tpd.h>		/* boot tape filesystem stuff */
#include <libsc.h>
#include <libsk.h>

#define max(a, b) (a < b ? b : a)	/* Since XFER_TIMEO (sys/tpsc.h) uses max ... */


/*
 * tpsc.c
 *  This module is the driver for all SCSI tapes (well, there is jagtape
 *  now also, but...)
 *
 *  Reads and writes for the byte swapped device require byte swapping
 *  in software, so are somewhat slower (about 1 or 2 %).
 *
 *  If you want debugging you must define SCSICT_DEBUG.
 *  This can also be done via symmon or dbx -k.  Current
 *  values uses are 0 (none) through 4 (most verbose).  The
 *  debug stuff is enabled (but ctdebug is 0) if compiled -DDDEBUG

 *  We assume each SCSI Tape Controller supports one and only one
 *  logical unit.
 *
 *  Almost all the differences between the assorted drives are now defined
 *  as the MTCAN* bits in mtio.h, and are used as initializers in the
 *  master.d/tpsc file for the tpsc_types array.  Similarly for timeouts,
 *  default block sizes, etc.
 *
 *     Minor device layout used internally:
 *
 *         7    6      5        4       3       2     1    0
 *    +------+------+-------+--------+-------+-----+-----+-----+
 *    +      +      +       +        + not   +                 +
 *    +  not + Fix/ + Swap/ + Rewind?+ used  +   SCSI ID #     +
 *    + used + Vari.+ Noswap+        + (was  +                 +
 *    +      +      +       +        + adap) +                 +
 *    +------+------+-------+--------+-------+-----+-----+-----+
 *
 *    The externl representation (used with mknod) is given below
 *    NOTE: Drive specific used to be for LUN, but none of our
 *    supported drives allow LUN's other than 0.  Almost all uses
 *    of dev_t in this driver are the internal form.
 *
 *         7    6      5        4       3       2     1    0
 *    +------+------+-------+--------+-------+-----+-----+-----+
 *    +                     +                + fix/+     +     +
 *    +      SCSI ID #      + Drive specific + var.+swap?+ rew?+
 *    +                     +                + blks+     +     +
 *    +                     +                +     +     +     +
 *    +------+------+-------+--------+-------+-----+-----+-----+
 *
 *  NOTE: for the Kennedy tape drive, I the drive specific bits
 *  bits are the density bits; no other drives use them at this time.
 *
 *  It isn't necessary to check B_ERROR in b_flags in this
 *  driver, since anywhere that bit is set, b_error is also set, so
 *  you can just do if(bp->b_error) and skip the AND; it still hast
 *  to be set in some cases for upper layers, so when clearing b_error,
 *  for ignored 'errors', you may need to clear B_ERROR as well.
*/



/* prototypes for all functions defined in the driver */

/* entry points via devsw */
void tpscinit(void);
void tpscread(dev_t);
void tpscwrite(dev_t);
/* no immediate mode in standalone; simpler and more reliable in case one
 * standalone program exits or execs another */
int tpsc_immediate_rewind = 0;
void _tpscclose(dev_t, int);
void _tpscopen(dev_t, int);
#define FREAD	F_READ
#define FWRITE	F_WRITE
#undef dcache_wb
#undef dcache_inval
#undef dcache_wbinval
#define dcache_wb(a,b)
#define dcache_inval(a,b)
#define dcache_wbinval(a,b)
extern int cachewrback;

/* called directly from with the driver */

static void ctreport(ctinfo_t *, char *, ...);
static void ctstrategy(buf_t *);
static void ctdrivesense(ctinfo_t *);
static int	ctchkerr(ctinfo_t *);
static dev_t ctminor_remap(dev_t);
static int	ctreqsense(ctinfo_t *);
static ctinfo_t *ctsetup(dev_t, dev_t);
static int	ctgetblklen(ctinfo_t *, dev_t);
static void ct_badili(ctinfo_t *, int);
static void ct_shortili(ctinfo_t *, int, int);
static int ctloaded(ctinfo_t *);
static int ctresidOK(ctinfo_t *ctinfo);

static void ctcmd(ctinfo_t *, void (*)(register ctinfo_t *, int, uint, uint),
		  uint, int, uint);

/*	The rest of these are called only indirectly from ctcmd.
	Note that these commands should generally NOT clear any of the
	CTPOS bits on success or error, since ctchkerr already took
	care of that; if they may move the tape, CTPOS bits should
	usually be cleared in the setup.
*/
static void ctinq(ctinfo_t *, int, uint, uint);
static void ctmodesel(ctinfo_t *, int, uint, uint);
static void cttstrdy(ctinfo_t *, int, uint, uint);
static void ctmodesens(ctinfo_t *, int, uint, uint);
static void ctrewind(ctinfo_t *, int, uint, uint);
static void ctspace(ctinfo_t *, int, uint, uint);
static void ctxfer(ctinfo_t *, int, uint, uint);
static void ctwfm(ctinfo_t *, int, uint, uint);
static void ctload(ctinfo_t *, int, uint, uint);
static void ctprevmedremov(ctinfo_t *, int, uint, uint);
static void ctblklimit(ctinfo_t *, int, uint, uint);


static void swapbytes(u_char *ptr, unsigned n);

/* set if causes motion; MUST be updated if new commands are added */
static char ctmotion[] = {
	/* tst rdy */	0,
	/* rewind */	1,
	/* get blkaddr */	0,
	/* reqsense */	0,
	/* cmd 4  */	0,
	/* blklimits */	0,
	/* cmd 6  */	0,
	/* cmd 7  */	0,
	/* read */	1,
	/* cmd 9  */	0,
	/* write */	1,
	/* cmd b */	0,
	/* seek */	1,
	/* cmd d */	0,
	/* cmd e */	0,
	/* read rev */	1,
	/* wfm */	1,
	/* space */	1,
	/* inquiry */	0,
	/* verify */	1,
	/* recoverdata */	0,
	/* modesel */	0,
	/* reserve */	0,
	/* release */	0,
	/* copy */	1,
	/* erase */	1,
	/* modesense */	0,
	/* load */	1,
	/* rec diag */	0,
	/* send diag */	1,
	/* prevremov */	0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0,
	/* locate */ 1,
	0, 0, 0, 0, 0, 0, 0, 0,
	/* readpos */ 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0,
	/* change def */ 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0,
	/* log sel */ 0,
	/* log sense */ 0
};

extern struct tpsc_types tpsc_generic;

#if defined(DEBUG) && !defined(SCSICT_DEBUG)
# define SCSICT_DEBUG 0	/* debugs enabled, but off by default */
#endif
#ifdef SCSICT_DEBUG
/*	note that you may need braces around if(x) ct_dbg() ; else stuff
	because otherwise the else will go with the debug stuff... */
#define	ct_dbg(a)	if(ctdebug) cmn_err a
#define	ct_xdbg(lev, a)	if(ctdebug>lev) cmn_err a
#define	ct_cxdbg(cond, lev, a)	if((cond) && ctdebug>lev) cmn_err a
#define C CE_CONT	/* so lines aren't so long! */
# define ctdebug Debug
extern int Debug;

/* a place holder for undefined or unused cmds */
static char _ct_unk[] = "";

static char *ct_cmds[] = {
	"tst rdy",
	"rewind",
	"getblkaddr",
	"reqsense",
	"cmd 4 ",
	"blklimits",
	"cmd 6 ",
	"cmd 7 ",
	"read",
	"cmd 9 ",
	"write",
	"cmd b",
	"seek",
	"cmd d",
	"cmd e",
	"read rev",
	"wfm",
	"space",
	"inquiry",
	"verify",
	"recoverdata",
	"modesel",
	"reserve",
	"release",
	"copy",
	"erase",
	"modesense",
	"load",
	"rec diag",
	"send diag",
	"prevremov",
	_ct_unk, _ct_unk, _ct_unk, _ct_unk, _ct_unk, _ct_unk, _ct_unk, _ct_unk,
	_ct_unk, _ct_unk, _ct_unk, _ct_unk,
	"locate",
	_ct_unk, _ct_unk, _ct_unk, _ct_unk, _ct_unk, _ct_unk, _ct_unk, _ct_unk,
	"readpos",
	_ct_unk, _ct_unk, _ct_unk, _ct_unk, _ct_unk, _ct_unk, _ct_unk, _ct_unk,
	_ct_unk, _ct_unk, _ct_unk,
	"change def",
	_ct_unk, _ct_unk, _ct_unk, _ct_unk, _ct_unk, _ct_unk, _ct_unk, _ct_unk,
	_ct_unk, _ct_unk, _ct_unk,
	"log sel",
	"log sense"	/* 0x4D; see cmdnm */
};

/* types of space actions */
static char *ctspctyp[] = { 
	"blks",
	"fm",
	"seq fm",
	"eom",
	"setmk"
};

static char *
cmdnm(int x)
{
	static char cbuf[40];
	if((uint)x <= 0x4D && *ct_cmds[x])	/* 0x4D is last cmd in table */
		return ct_cmds[x];
	sprintf(cbuf, "cmd 0x%x not in table", x);
	return cbuf;
}

#else
#define	ct_dbg(a)
#define	ct_xdbg(lev, a)
#define	ct_cxdbg(cond, lev, a)
#endif

/* used to convert resid from scsi reqsense into an int */
#define MKINT(x) (((x)[0] << 24) + ((x)[1] << 16) + ((x)[2] << 8) + (x)[3])
/* used mainly for putting counts into scsi command form */
#define	HI8(x)	((u_char)((x)>>16))
#define	MD8(x)	((u_char)((x)>>8))
#define	LO8(x)	((u_char)(x))

extern int tpsc_extra_delay, tpsc_mindelay;	/* from lboot */

/*
 * allocate the pointers to the target info
*/
static ctinfo_t *ct_targets[SC_MAXADAP][SC_MAXTARG];

/*
 * the data represents a semaphore for each unit, so we can single thread
 * the open requests
*/
static sema_t	ct_opensema[SC_MAXADAP][SC_MAXTARG];


#include "saio.h"

int tpscstrategy(IOBLOCK *, int);
int tpscclose(IOBLOCK *);

int tpsc_install(void)	{return(1);}	/* needed for configuration */

static uint tpsc_blkno;


extern int scsimajor[];
#define scsi_ctlr(dev)          ((dev >> 9) & 0xff)
#define scsi_unit(dev)          ((dev >> 5) & 0xf)

/* this generates a 'dev_t with the correct values, so as little
 * code as possible changes. Assumes LUN always 0.  This is so
 * the standard macros can be used.  Major number has to match
 * the scsi_ctlr() macro, and the faked values used by it (in lboot.c).
 * Never variable mode, always byte swapped
*/
dev_t
tpsc_mkdev(IOBLOCK *iob)
{
	return  ((iob->Unit&7)<<5) | (iob->Controller<<9) | 1 ;
}

static int
ctseek(register ctinfo_t *ctinfo, uint blkno)
{
	if(blkno < tpsc_blkno) {	/* have to rewind first.  The
		assumption is that we are always dealing with the first tape
		file, and that we are working with least common denom, so 
		can't do space BSR. */
		ctcmd(ctinfo, ctrewind, 0, REWIND_TIMEO, 0);
		if(!(ctinfo->ct_state & CT_BOT))
			return 1;	/* an error... */
	}
	ctinfo->ct_subchan->s_buf = ctinfo->ct_bp;
	ctinfo->ct_bp->b_bcount = blkno - tpsc_blkno;
	ctcmd(ctinfo, ctspace, SPACE_BLKS_CODE,
			XFER_TIMEO(ctinfo->blkinfo.maxblksz * blkno), 0);
	if(!ctinfo->ct_bp->b_error)
		tpsc_blkno = blkno;
	return ctinfo->ct_bp->b_error;
}

int
tpscopen(IOBLOCK *iob)
{
	ctinfo_t *ctinfo;

	u.u_error = 0;

	/* this is needed because in some of the error cases in
	 * the standalone fs code, the close routine won't get
	 * called.  Therefore further opens would fail...  Should
	 * fix the fs code someday.  We also need to sanity check
	 * the ctlr and unit values, unlike the kernel. */
	if((uint)iob->Controller >= SC_MAXADAP || (uint)iob->Unit >= SC_MAXTARG)
		return(iob->ErrorNumber = ENODEV);

	if((ctinfo=ct_targets[iob->Controller][iob->Unit]) &&
		(ctinfo->ct_state & CT_OPEN))
		tpscclose(iob);

	_tpscopen(tpsc_mkdev(iob), iob->Flags);
	if(iob->ErrorNumber = u.u_error)
		return(iob->ErrorNumber);

	ctinfo = ct_targets[iob->Controller][iob->Unit];

	if(!(iob->ErrorNumber = u.u_error) && iob->Partition==0 &&
		(iob->Flags&F_READ)) {
		/*	figure out what type of file system is out there, if
			reading and first tape file.  */
	}
	/* older standalone driver recognized blk 0 in
	 * strat specially and saved and copied the dir block.  I
	 * think that was a bit silly.  rewind even if we are
	 * going to space, since we are spacing to an 'absolute'
	 * partition #. */
	ctcmd(ctinfo, ctrewind, 0, REWIND_TIMEO, 0);
	if(!iob->ErrorNumber && iob->Partition) {
		ctinfo->ct_bp->b_bcount = (int)iob->Partition;
		ctcmd(ctinfo, ctspace, SPACE_FM_CODE, SPACE_TIMEO, 0);
		iob->ErrorNumber = ctinfo->ct_bp->b_error;
	}
	if(iob->ErrorNumber) {
		/*	mainly to free swapbuffer; if not for that could just catch
			on the CT_OPEN check above on next open */
		tpscclose(iob);
		return -1;
	}
	return iob->ErrorNumber ? -1 : 0;
}

int
tpscclose(IOBLOCK *iob)
{
	u.u_error = 0;
	_tpscclose(tpsc_mkdev(iob), iob->Flags);
	tpsc_blkno = 0xffffffff;	/* for next fs type open */
	return (iob->ErrorNumber=u.u_error) ? -1 : 0;
}

/* func should be redundant, should come from i_flgs... */
int
tpscstrategy(IOBLOCK *iob, int func)
{
	/* never more than one request at a time in standalone, so
		having a static buf is no problem */
	static buf_t tpbuf;
	int ret;

	u.u_error = 0;
	tpbuf.b_dev = tpsc_mkdev(iob);
	tpbuf.b_flags = func==READ ? B_READ : B_WRITE;
	tpbuf.b_bcount = iob->Count;
	tpbuf.b_dmaaddr = (char *)iob->Address;
	tpbuf.b_blkno = iob->StartBlock;
	if(tpbuf.b_blkno != tpsc_blkno && /* need to seek */
		ctseek(ct_targets[iob->Controller][iob->Unit],
			iob->StartBlock)) {
		return(iob->ErrorNumber = EIO);
	}

	ctstrategy(&tpbuf);
	if (ret = iob->ErrorNumber = u.u_error) {
		tpsc_blkno = 0xffffffff; /* force a rewind and space if
			they try more 'filesystem' i/o. */
	}
	else
		iob->Count -= tpbuf.b_resid;
	return ret;
}

/*  If at EOM or FM, read will not be able to return any bytes.  It is
 * hard to determine this case though, and to be compatable with the
 * spec we need to READ to sense the FM or EOM.
 */
static int
tpscgrs(IOBLOCK *iob)
{
	dev_t dev = tpsc_mkdev(iob);
	register ctinfo_t *ctinfo;
	char *buf;
	int size;

	ctinfo = ct_targets[scsi_ctlr(dev)][SCSIID_DEV(ctminor_remap(dev))];
	if (ctinfo->ct_state & (CT_EOM|CT_EOD|CT_FM))
		return(iob->ErrorNumber = EAGAIN);

	size = ISVAR_DEV(dev) ? ctinfo->ct_cur_minsz : ctinfo->ct_blksz;
	if (!(buf = (char *)malloc(size)))
		return(iob->ErrorNumber = ENOMEM);

	iob->Address = buf;
	iob->Count = size;
	if (tpscstrategy(iob,READ) || !iob->Count) {
		free(buf);
		return(iob->ErrorNumber = EAGAIN);
	}

	free(buf);
	iob->Count = size;
	return(ESUCCESS);
}

/* ARCS - new stuff */

/*ARGSUSED*/
int
tpsc_strat(COMPONENT *self, IOBLOCK *iob)
{
        switch (iob->FunctionCode) {

        case    FC_INITIALIZE:
                tpscinit();
		return 0;

        case    FC_OPEN:
                return (tpscopen (iob));

        case    FC_READ:
                return (tpscstrategy (iob, READ));

        case    FC_WRITE:
                return (tpscstrategy (iob, WRITE));

        case    FC_CLOSE:
                return (tpscclose (iob));

        case FC_GETREADSTATUS:
                return(tpscgrs(iob));

        default:
                return(iob->ErrorNumber = EINVAL);
        };
}

/* ARCS - new stuff */

/* all the standalone entry points get redefined */
# define tpscopen _tpscopen
# define tpscclose _tpscclose


/*	determine which one of the supported drives we are looking at
	from the inquiry data.  This is needed for all the little
	idiosyncrasies...  Called from scsi.c during inventory setup, and
	from ctsetup().  The return value is the hinv type, for scsi.
*/
int
tpsctapetype(char *__inv, struct tpsc_types *tpp)
{
	ct_g0inq_data_t *inv = (ct_g0inq_data_t *)__inv;
	int i;
	struct tpsc_types *tpt;
	u_char *id = inv->id_vid;
	u_char *pid = inv->id_pid;
	extern struct tpsc_types tpsc_types[];
	extern int tpsc_numtypes;

	tpt = tpsc_types;
	if(inv->id_ailen == 0) {	/* hope only 540S... */
		id = (u_char *)"CIPHER";	/* fake it; must match master.d/tpsc */
		pid = (u_char *)"540S";	/* fake it; must match master.d/tpsc */
	}
	for(i=0; i<tpsc_numtypes; i++, tpt++) {
		if(strncmp((char *)tpt->tp_vendor, (char *)id, tpt->tp_vlen) == 0 &&
			strncmp((char *)tpt->tp_product, (char *)pid, tpt->tp_plen) == 0)
			break;
	}
	if(i == tpsc_numtypes)
		tpt = &tpsc_generic; 
	if(tpp)
		bcopy(tpt, tpp, sizeof(*tpt));
	return tpt->tp_hinv;
}

void
tpscinit()
{
	bzero(ct_targets, sizeof(ct_targets));	/* init_malloc again */
	tpsc_blkno = 0xffffffff;	/* can't initialize at definition,
		or it won't work right in the PROM */
}


/*	probe to see if the drive exists, and if so what type it is.
	Do the inquiry on each open, since it is possible
	to insert and remove drives in the IP6 while the system
	is up.  If we haven't yet allocated ctinfo yet,
	(very first open on this HA/target), allocate it.
	Allocate the scsi channel structure and initialize it.
	dev is remapped dev, odev is the original ('real') dev number.
	Can't set b_error on 'already in use' errors because that could
	mess up the program already using the drive.
*/
static ctinfo_t *
ctsetup(dev_t dev, dev_t odev)
{
	register buf_t *bp;
	register ctinfo_t *ctinfo;
	int ha, targ, firsttime;

	ha = scsi_ctlr(odev);
	targ = SCSIID_DEV(dev);
	ctinfo = ct_targets[ha][targ];

	if(ctinfo == NULL) {
		firsttime = 1;
		ct_dbg((C, "allocate info "));

		ctinfo = (ctinfo_t *)kern_calloc(1, sizeof(*ctinfo));
		if(ctinfo == NULL)	{ 	/* not supposed to happen... */
			ct_dbg((C, "kern_calloc fails!\n"));
			return ctinfo;
		}
		ct_targets[ha][targ] = ctinfo;

		/* allocate the buffer headers */
		ctinfo->ct_bp = (buf_t *)kern_calloc(1, sizeof(buf_t));
		ctinfo->ct_reqbp = (buf_t *)kern_calloc(1, sizeof(buf_t));

		/* allocate the region which may be DMAed into */
		ctinfo->ct_bufarea =
		    (union ct_bufarea *)dmabuf_malloc(sizeof(union ct_bufarea));

		/*
		** init the various locks and semaphores
		**   exclusive use semaphore
		**   buffer header semaphores
		*/
		init_sema(&ctinfo->ct_bp->b_lock, 1, "ctbpsp", 0);
		init_sema(&ctinfo->ct_bp->b_iodone, 0, "ctbpsl", 0);

		ctinfo->ct_lastdev = NODEV;

		/* so ctblklimit() will set them */
		ctinfo->blkinfo.minblksz = 0xffffffff;
		ctinfo->blkinfo.maxblksz = 0;
		/* for timeouts, etc. */
		bcopy(&tpsc_generic, &ctinfo->ct_typ, sizeof(tpsc_generic));
		ctinfo->ct_typ.tp_rewtimeo *= HZ;
		ctinfo->ct_typ.tp_spacetimeo *= HZ;
		ctinfo->ct_typ.tp_mintimeo *= HZ;
	}
	else {
		firsttime = 0;
		/* must check before any changes to ct_state, etc. */
		if(ctinfo->ct_state & CT_OPEN) {
			ct_dbg((C, "already open.  "));
			/* "mt exists" takes this to mean drive exists */
			u.u_error = EBUSY;
			return NULL;	/* indicate error; don't touch b_error */
		}
	}
	bp = ctinfo->ct_bp;
	bp->b_dev = dev;
	ctinfo->ct_reqbp->b_dev = dev;

	/*
	** allocate a subchannel, don't forget to use
	** the real host adaptor SCSI ID, not the logical id.
	*/
#ifdef IP22_WD95
	if (ha == 0) /* must be wd93 */
		ctinfo->ct_subchan = allocsubchannel(ha, targ, CT_LUNIT);
	else
		ctinfo->ct_subchan = wd95_allocsubchannel(ha, targ, CT_LUNIT);
#else
		ctinfo->ct_subchan = allocsubchannel(ha, targ, CT_LUNIT);
#endif
	if(ctinfo->ct_subchan == NULL) {
		bp->b_error = ENODEV;	/* bad adapter # (which we
			checked above), or in use by some other driver, so
			can't be tape. */
		return ctinfo;
	}

	ctinfo->ct_subchan->s_cmd = (u_char *)&ctinfo->ct_cmd;
	ctinfo->ct_subchan->s_buf = bp;
	/* s_notify remains null; no callbacks in this driver */
#ifdef SN0
	/*
 	 * This field is getting a bogus value after malloc on
	 * SN0. Make it explicitly NULL for now till the real
	 * cause is found.
	 */
	ctinfo->ct_subchan->s_notify = NULL ;
#endif

	/* find out what kind of drive we are dealing with (and 
	 * whether it exists; almost all scsi devices will respond
	 * to an inquiry even while they ignore/fail anything else).
	 * Since the data goes in a union, don't make other calls till
	 * done looking at the inquiry data.  Also want to get scsi
	 * version early on. */
	ctcmd(ctinfo, ctinq, MAX_INQUIRY_DATA, SHORT_TIMEO, 0);
	if(bp->b_error) {
		ct_dbg((C, "INQ failed (%d).  ", bp->b_error));
		bp->b_error = ENODEV;
		return ctinfo;
	}

	ctinfo->ct_scsiv = ctinfo->ct_inq_data.id_ansi;
	ctinfo->ct_cansync = ctinfo->ct_inq_data.id_respfmt==2 &&
		ctinfo->ct_inq_data.id_sync;

	/* Find out what kind of device we're using */
	(void)tpsctapetype((char *)&ctinfo->ct_inq_data, &ctinfo->ct_typ);

	ctinfo->blkinfo.recblksz = ctinfo->ct_typ.tp_recblksz;
	if(firsttime)	/* otherwise preserve setfixed; could potentially
		have a problem if someone pulls one tape drive out and connects
		another (with a different default fixedblocksize) at the same ID
		without rebooting, but that is pretty bogus; I'm probably the
		only person who ever does crazy things like that. */
		ctinfo->ct_fixblksz = ctinfo->ct_typ.tp_dfltblksz;

	/* multiply these here, so we don't need to do it everywhere they
	 * are used */
	ctinfo->ct_typ.tp_rewtimeo *= HZ;
	ctinfo->ct_typ.tp_spacetimeo *= HZ;
	ctinfo->ct_typ.tp_mintimeo *= HZ;

	/* CT_CHG may be set by either this or the inquiry above.
	 * It is only checked in the next block.
	 * If tstrdy successful, CT_ONL will be set; otherwise ONL gets
	 * set later via ctloaded.
	 * An EAGAIN return indicates drive not ready; i.e., no
	 * tape in drive (or tape not loaded on some drives).
	 * Note that the cipher will return ready even
	 * if a tape isn't in the drive...  Note that if the drive is
	 * re-opened while a previous immediate mode cmd is still running,
	 * this is where we block until it finishes. */
	ctcmd(ctinfo, cttstrdy, NULL, SHORT_TIMEO, 0);
	if(bp->b_error && bp->b_error != EAGAIN) {
		ct_dbg((C, "TST RDY fails.  "));
		return ctinfo;
	}

	/* Do this on not ready also, because the tape must
	 * have been unloaded (or device reset) if we aren't
	 * ready.  See this particularly on 9 track drives */
	if((ctinfo->ct_state&CT_CHG) || !(ctinfo->ct_state&CT_ONL)) {
		/* zero the read/write counts */
		ctinfo->ct_readcnt = ctinfo->ct_writecnt = 0;
		/* so ctblklimit() will set them */
		ctinfo->blkinfo.minblksz = 0xffffffff;
		ctinfo->blkinfo.maxblksz = ctinfo->blkinfo.curblksz = 0;
		if(ctinfo->ct_typ.tp_capablity & MTCAN_SILI)
			/* default to reporting ili on a media change */
			ctinfo->ct_sili = 0;
		ctinfo->ct_state &= ~CT_CHG;
	}

	if(firsttime) {
		if(IS_CIPHER(ctinfo))	/* only on first open; ioctl */
			/* settings persist after that */
			ctinfo->ct_cipherrec = 1;

		if(ctinfo->ct_typ.tp_capablity & MTCAN_SETSP)
			/* default to high speed on first open */
			ctinfo->ct_speed = 1;
	}

	if(ISVAR_DEV(dev))
		ctinfo->ct_blksz = 1;
	else /* preserves SETFIX'ed value if done.  We need to do this
		  * on every fixed open, in case we were in variable mode
		  * the last time, otherwise blocksize is 1, and things
		  * don't work too well.  Don't set curblksz, because for
		  * drives that support both fixed and variable mode, we
		  * only want to set the blocksize after a succesful read,
		  * since the media might still be variable, and we only
		  * call getblklen if we haven't yet done it on the BLKLIMITS
		  * ioctl. We zero curblksz on tape changes, so that will catch
		  * a change of media type in the case where we are only
		  * reading. */
		ctinfo->ct_blksz = ctinfo->ct_fixblksz;

	/* Note that both the HP and the Kennedy 9 track drives
	 * will continue to write at the previous density,
	 * if we don't force an error when the drive is opened at a different
	 * density, and we aren't at BOT and have done some i/o.
	 * It seems reasonable to do this, since both drives do autodensity
	 * selection on read anyway.
	 * We need to allow an 'mt rew' to work even if it was a different
	 * density device.  The HP seems to show the last selected density,
	 * rather than actual, so that is confusing.  We may want to change
	 * this to allow ioctls, but not i/o, but for now, we remove the old
	 * check that caused an open to fail in this case.  This issue came up
	 * while doing qual work on the HP 9 track drive.  Now we just always
	 * set the density here, if drive supports it, and worry about
	 * whether to actually send the modeselect in ctmodesel.
	*/
	if(ctinfo->ct_typ.tp_capablity & MTCAN_SETDEN)
		ctinfo->ct_density =
			ctinfo->ct_typ.tp_density[DENSITY_DEV(odev)];

	/* Do a modeselect to set density, drive specific options,
	 * etc.  Do on each open, since no reason not to do so.
	 * OK if fails on some drives, (e.g., Kennedy and Cipher) because the
	 * modeselect will be re-done when the tape is loaded,
	 * which happens only if the user gives us a command that
	 * would move the tape (or requires the tape to be at a
	 * known place).  Therefore we always ignore errors from this
	 * (who knows what tapes will be hooked up by our customers...).
	 * Always do a sense first, so things like audio vs non-audio don't
	 * get reset by a blind modeselect! */
	ctcmd(ctinfo, ctmodesens, MAX_MODE_SEL_DATA, SHORT_TIMEO, 0);
	bp->b_error = 0;
	ctcmd(ctinfo, ctmodesel, 0, SHORT_TIMEO, 0);
	bp->b_error = 0;
		
	return ctinfo;
}


/* Do a mode sense to chk state of drive, write protect,
 * etc.  Also deal with the QIC24 stuff.
 * Called only if online (ready).  If offline, opens and
 * some commands (ioctls) can still be done.
*/
static int
ctchkstate(register ctinfo_t *ctinfo)
{
	register buf_t		*bp; bp = ctinfo->ct_bp;

	ct_dbg((C, "chkstate "));
	ctcmd(ctinfo, ctmodesens, MAX_MODE_SEL_DATA, SHORT_TIMEO, 0);
	if(bp->b_error)
		return 1;

	/* If open for writing, HAVE to forget
	 * about QIC24 and QIC120, since the VIPER150 sense is for
	 * TAPE format, not CARTRIDGE format.  Otherwise if we read
	 * a 600* tape that was written in QIC24 format, then try to
	 * write it without removing the tape, we get an error.  Has
	 * to be done AFTER the modesense.  Fortunately mt
	 * opens it readonly, except when writing fm's, etc., so
	 * mt status will report 'correctly'.  
	 * Rev1.5 and earilier firmware on VIPER150 drives WILL NOT
	 * write a tape written in QIC24 format, regardless of cartridge
	 * type, if it has first been read.  The only remedy is to pop
	 * the cartridge out and then re-insert it; rev1.6 fixes it.
	 * Olson, 1/89 */
	if((ctinfo->ct_openflags & FWRITE) &&
		(ctinfo->ct_state & (CT_QIC24|CT_QIC120)))
		ctinfo->ct_state &= ~(CT_QIC24|CT_QIC120);
	return 0;
}


void
tpscopen(dev_t dev, int flags)
{
	register ctinfo_t *ctinfo;
	register buf_t		*bp = NULL;
	sema_t			*opensema;
	int			h_adp, scsi_id;
	dev_t savdev = dev;	/* before remapping */

#ifdef BCOPYTEST
bcopytest();
#endif
	ct_dbg((C, "tpopen "));

	h_adp = scsi_ctlr(dev);
	dev = ctminor_remap(dev);	/* convert to internal form */

	scsi_id = SCSIID_DEV(dev);

	/* check for invalid device */
	if(h_adp > scsicnt || scsi_id > SC_MAXTARG) {
		ct_dbg((C, "invalid HA/targ value.  "));
		u.u_error = ENODEV;
		return;
	}

	/* single thread the opens at this point */
	opensema = &ct_opensema[h_adp][scsi_id];
	psema(opensema, PRIBIO);

	/* verify dev, chk for device existance, and allocate
	 * any needed data structures */
	ctinfo = ctsetup(dev, savdev);
	if(ctinfo == NULL) {
		if(!u.u_error) /* kern_calloc failed; don't want to use b_error */
			u.u_error = ENOMEM;
		goto erroropen;
	}

	bp = ctinfo->ct_bp;

	if(bp->b_error)
		goto erroropen;

	ctinfo->ct_openflags = flags;

	/* chk states, etc if online */
	if((ctinfo->ct_state & CT_ONL) && ctchkstate(ctinfo))
		goto erroropen;

	/* Ignore any errors for the next two cmds; they are 'cosmetic' */
	if(ctinfo->ct_typ.tp_capablity&MTCAN_PREV)
		/* turn on the front panel LED; disable tape removal for those
		 * with software controllable eject */
		ctcmd(ctinfo, ctprevmedremov, CT_PREV_REMOV, SHORT_TIMEO, 0);


	/* get the block limits for use in ctstrategy, etc */
	ctcmd(ctinfo, ctblklimit, 0, SHORT_TIMEO, 0);

	ctinfo->ct_lastdev = savdev;
	ctinfo->ct_state |= CT_OPEN;

	if((ctinfo->ct_typ.tp_capablity&MTCAN_SYNC) || ctinfo->ct_cansync)
#ifdef IP22_WD95
		if (ctinfo->ct_subchan->s_adap == 0) /* must be wd93 */
			scsi_setsyncmode(ctinfo->ct_subchan, 1);
		else
			wd95_scsi_setsyncmode(ctinfo->ct_subchan, 1);
#else
			scsi_setsyncmode(ctinfo->ct_subchan, 1);
#endif

	vsema(opensema);
	return;

erroropen:
	if(!u.u_error)
		u.u_error = bp->b_error;
	if(ctinfo && !(ctinfo->ct_state & CT_OPEN)) {
		/* only if someone else doesn't have it open! */
		ctinfo->ct_state &= CTKEEP;
		if(ctinfo->ct_subchan)
#ifdef IP22_WD95
			if (h_adp == 0) /* must be wd93 */
				freesubchannel(ctinfo->ct_subchan);
			else
				wd95_freesubchannel(ctinfo->ct_subchan);
#else
				freesubchannel(ctinfo->ct_subchan);
#endif
	}
	vsema(opensema);
	return;
}


/*ARGSUSED1*/
void
tpscclose(dev_t dev, int flags)
{
	register ctinfo_t *ctinfo;
	register buf_t		*bp;
	int			h_adp, scsi_id, fmcnt;
	sema_t			*opensema;
	ulong ostate;	/* state before close tape motions */

	h_adp = scsi_ctlr(dev);
	dev = ctminor_remap(dev);
	scsi_id = SCSIID_DEV(dev);

	ctinfo = ct_targets[h_adp][scsi_id];
	ct_dbg((C, "tpclose after %d/%d r/w st=%x. ",ctinfo->ct_readcnt,
		ctinfo->ct_writecnt, ctinfo->ct_state));
	opensema = &ct_opensema[h_adp][scsi_id];
	psema(opensema, PRIBIO); /* ensure no opens while closing,
		 * so state isn't inconsistent, and so we don't miss writing
		 * FM's, etc. */

	ctinfo->ct_subchan->s_buf = bp = ctinfo->ct_bp;
	bp->b_error = 0;

	/*
	** if tape is/was off-line, give up early.  Info is based on last
	** issued command, (read, write, ioctl)
	*/
	ostate = ctinfo->ct_state;
	if(!(ostate & CT_ONL)) {
		if(ostate&CT_MOTION)	/* only if they tried to move tape */
			bp->b_error = EIO;
		goto errorclose;
	}

	/* Do not write a FM at EW because multi-volume backups the SGI way
	 * assume that a failed read indicates the end of a volume, but a
	 * short read (as you would see at a FM) indicates a failure when
	 * reading.  We still try to flush any buffered data to tape, since
	 * successful status will have been returned to the program.
	 * If ANSI is set, we must write filemarks even at EW to comply with
	 * the ANSI multi-tape standars.  None of the SGI programs that do
	 * multivol (tar, cpio, bru) will set ANSI, so this works out OK.
	 * Note that for audio tapes, no fm's exist. */
	if((ostate & (CT_AUDIO|CT_AUD_MED|CT_WRITE|CT_WRP|CT_QIC24)) == CT_WRITE) {
		fmcnt=((ostate & (CT_EW|CT_ANSI)) == CT_EW && !ctresidOK(ctinfo))?0:1;
		if(fmcnt && (ctinfo->ct_typ.tp_capablity&MTCAN_LEOD)) {
			/* write 2 fm's, then if !rewind, bsf over one
			 * to be compatible with 1/2" tape drives,
			 * and xm in particular.  Also so MTAFILE will
			 * work.  This is needed because 9 track and similar
			 * drives don't have a well defined EOD, since they
			 * can overwrite.  Instead one has a 'logical' EOD,
			 * defined as 2 sequential FM's. Do as 2 seperate calls,
			 * in case past CT_EW; in this state only one can be
			 * written at a time, but we still need 2 to be ansi
			 * (and defacto standard) compatible. */
			ctcmd(ctinfo, ctwfm, 1, SHORT_TIMEO, 0);
			ctcmd(ctinfo, ctwfm, 1, SHORT_TIMEO, 0);
			if(!ISTPSCREWIND_DEV(dev) && !bp->b_error) {
				ctinfo->ct_subchan->s_buf = bp;	/* reset */
				bp->b_bcount = (uint)-1;
				ctcmd(ctinfo, ctspace, SPACE_FM_CODE,
					SPACE_TIMEO, 0);
			}
		}
		else {
			ctcmd(ctinfo, ctwfm, fmcnt, SHORT_TIMEO, 0);
			if(!ISTPSCREWIND_DEV(dev) && IS_EXABYTE(ctinfo)) {
				/* EXABYTE won't write out it's EOD marker unless 
				 * we do a reposition after writing the filemark. */
				bp->b_bcount = (uint)-1;
				ctcmd(ctinfo, ctspace, SPACE_BLKS_CODE, 
					XFER_TIMEO(ctinfo->blkinfo.maxblksz), 0);
				bp->b_bcount = (uint)1;
				ctcmd(ctinfo, ctspace, SPACE_BLKS_CODE, 
					XFER_TIMEO(ctinfo->blkinfo.maxblksz), 0);
			}
		}
	}

	/* START HISTORY:
	 * Since we are seeing more and more drives where unload actually
	 * does something, and there have been complaints that the mt status
	 * command returns "not ready" (because we did an unload) we now
	 * NEVER do an unload, but just a rewind if appropriate.  This entire
	 * comment is left so that future maintainers understand the history
	 * of this!  Dave Olson
	 * If ISTPSCREWIND device, then unload/rewind; even if no motion,
	 * since we no longer load/rewind the tape on opens.
	 * However, if it is a Kennedy, DAT, or an Exabyte, just rewind
	 * because unloading actually unloads the tape, and makes
	 * the drive go offline, and next access takes a long time.
	 * Don't unload the Cipher either, since it does so many
	 * stupid things when you do.  The only rationale for it was
	 * that it would turn the light off, but Cipher refused to
	 * change their firmware...
	 * END HISTORY.
	 *
	 * If immediate rewinds are enabled, we have to allow removal first,
	 * so cmd doesn't fail due to busy. It may block anyway, if there was
	 * already an immediate mode cmd running, as would the rewind in that
	 * case (for some drives; DAT allows * the prevremoval while the
	 * immediate cmd is running, QIC does not).  Since all the immediate
	 * mode cmds except seek return the drive to BOT, and one isn't likely
 	 * to do a seek and close on the rewind device anyway, it seems to
	 * be basicly a non-issue, except that if you do something like
	 * 'mt -t /dev/tape rewind', you will wait here until the ioctl
	 * rewind completes; again, not much of a * problem. */
	if(ISTPSCREWIND_DEV(dev)) {
	    if(tpsc_immediate_rewind &&
		(ctinfo->ct_typ.tp_capablity&MTCAN_PREV))
		ctcmd(ctinfo, ctprevmedremov, CT_ALLOW_REMOV, SHORT_TIMEO, 0);
	    ctcmd(ctinfo, ctrewind, 0, REWIND_TIMEO, 0);
	}

errorclose:
	/* turn off the front panel LED; enable tape removal for those
	 * with software controllable eject; may fail if still doing
	 * immediate rewind, but that is OK, already done in that
	 * case */
	if(ctinfo->ct_typ.tp_capablity&MTCAN_PREV)
	    ctcmd(ctinfo, ctprevmedremov, CT_ALLOW_REMOV, SHORT_TIMEO, 0);

	if(ctinfo->ct_recov_err && (ostate&CT_MOTION)) {
		/* only report if user moved tape or we loaded it for
		 * them, to prevent multiple recovered error messages
		 * if there is an error followed by multiple commands
		 * like 'mt status'.  Doesn't do anything for errors
		 * persisting across a reboot though... */
		/* use scsi2 logsense here eventually if scsiv >= 2 */
		ctreport(ctinfo, "had %d recoverable errors\n",
			ctinfo->ct_recov_err);
		ctinfo->ct_recov_err = 0; /*	clear after reporting */
	}

	ctinfo->ct_state &= CTKEEP; /* clear per-open states */

	if((ctinfo->ct_typ.tp_capablity&MTCAN_SYNC) || ctinfo->ct_cansync)
#ifdef IP22_WD95
		if (ctinfo->ct_subchan->s_adap == 0) /* must be wd93 */
			scsi_setsyncmode(ctinfo->ct_subchan, 0);
		else
			wd95_scsi_setsyncmode(ctinfo->ct_subchan, 0);
#else
			scsi_setsyncmode(ctinfo->ct_subchan, 0);
#endif

#ifdef IP22_WD95
	if (h_adp == 0) /* Must wd93 */
		freesubchannel(ctinfo->ct_subchan);
	else
		wd95_freesubchannel(ctinfo->ct_subchan);
#else
		freesubchannel(ctinfo->ct_subchan);
#endif
	/* let programs know if we couldn't close normally */
	u.u_error = bp->b_error;
	ct_dbg((C, "\n"));
	vsema(opensema);
}


/*
 * ctstrategy
 *  Strategy routine for reads and writes. We do high level error
 *  checking here.  The buf_t * may be from physio, or ct_bp
 *	if called from within the driver
*/
static void
ctstrategy(register buf_t *bp)
{
	register ctinfo_t *ctinfo;
	int extraresid = 0;
	int hadp = scsi_ctlr(bp->b_dev);
	int skippedfm = 0;
	dev_t odev = bp->b_dev;

	bp->b_dev = ctminor_remap(bp->b_dev);
	ctinfo = ct_targets[hadp][SCSIID_DEV(bp->b_dev)];
	if(bp->b_bcount == 0)
		goto done; /* check here so we don't fail size chks */

	/* note that this will also skip over a FM when reading, if one was
	 * encountered on an earlier read, etc. */
	if(!ctloaded(ctinfo) || !(ctinfo->ct_state & CT_ONL)) {
		ct_dbg((C, "strat: offline (st %x).  ", ctinfo->ct_state));
		bp->b_error = EIO;
		goto errdone;
	}

	/* abort if we are positioned at EOM (not EW), or (EW and writing
	 * unless ansi has been set).  On reads we allow reading until
	 * we get EOD.  This is primarily because with buffered
	 * writes, some amount of data may be written past EW, the
	 * exact amount is unknown (and unknowable at read time).
	 * Even the QIC drives allow this.  Otherwise we can return
	 * a higher write count than can be read back. */
	if((ctinfo->ct_state & CT_EOM) ||
	    (ctinfo->ct_state & (CT_READ|CT_EOD)) == (CT_READ|CT_EOD) ||
	    (ctinfo->ct_state & (CT_WRITE|CT_EW|CT_ANSI)) == (CT_WRITE|CT_EW)) {
	    ct_dbg((C, "strat: at EOM.  "));
	    bp->b_error = ENOSPC;
	    goto errdone;
	}

	/* We have seen panics because of divide by 0 at the bcount % blksz
	 * below; I have yet to figure out how this could ever happen. It
	 * seems to be associated with scsi bus resets... */
	if(!ctinfo->ct_blksz) {
		ctreport(ctinfo, "Error, internal blksize is 0\n");
		goto badcnt;
	}

	if(ISVAR_DEV(bp->b_dev)) {
		/* check that request is within drive limits */
		if(bp->b_bcount < ctinfo->ct_cur_minsz)
			goto badcnt;
		/* catch attempts to open the variable mode device for 
		 * drives that don't support it. Be paranoid, in case
		 * minsz not set due to an error. */
		if(ctinfo->ct_cur_minsz &&
			ctinfo->ct_cur_minsz == ctinfo->ct_cur_maxsz &&
			(bp->b_bcount%ctinfo->ct_cur_minsz)) {
			ct_dbg((C, "min==max, and remainder;"
			    "prob var mode not allowed.  "));
			goto badcnt;
		}
		if(bp->b_bcount > ctinfo->ct_cur_maxsz) {
			if(bp->b_flags & B_READ) {	/* adjust down, this
				* allows determining the block size in the
				* standar unix method, by using a large read
				* request. */
				extraresid =  bp->b_bcount - ctinfo->ct_cur_maxsz;
				bp->b_bcount = ctinfo->ct_cur_maxsz;
			}
			else
				goto badcnt;
		}
	}
	else if(bp->b_bcount % ctinfo->ct_blksz)
		goto badcnt; /*	must be no remainder.  */

	/* If we are doing a read, we must check that the device has
	 * never done a write. All of our tapes fail an attempt to
	 * alternate reads and writes between filemarks.  intermixed
	 * reads and writes are allowed in audio mode to allow for editting.  */
	if(bp->b_flags & B_READ) {
		if((ctinfo->ct_state & (CT_AUDIO|CT_WRITE)) == CT_WRITE) {
			ct_dbg((C, "tried to read after writing.  "));
			bp->b_error = EINVAL;
			goto errdone;
		}

		/* If we are at a filemark and last read was short, but NOT 0,
		 * then set resid to count and return.  This can only happen in
		 * fixed block mode, and is necessary so that the program knows
		 * it encountered a FM.  */
		if(ctinfo->ct_state & CT_HITFMSHORT) {
			ct_dbg((C, "HITFM set on read; skip i/o. "));
			bp->b_resid = bp->b_bcount;
			/* also cleared on any tape motion via CTPOS in ctcmd */
			ctinfo->ct_state &= ~CT_HITFMSHORT;
			goto done;
		}

		ctinfo->ct_state |= CT_READ;
	}
	else {
		if((ctinfo->ct_state & (CT_AUDIO|CT_READ)) == CT_READ) {
			ct_dbg((C, "write tried after read.  "));
			bp->b_error = EINVAL;
			goto errdone;
		}

		if(ctinfo->ct_state & CT_WRP) {
			ct_dbg((C, "strat: WRP.  "));
			bp->b_error = EROFS;
			goto errdone;
		}

		/* VIPER 150 tape can read old 310 oersted tapes, but can't
		 * write them.  Only set if drive is Viper150. Olson, 11/88.  */
		if(ctinfo->ct_state & CT_QIC24) {
			ct_dbg((C, "strat: bad dens.  "));
			bp->b_error = EINVAL;	/* can't distinguish from write
			    after read; too bad, only error that makes sense */
			goto errdone;
		}
		ctinfo->ct_state |= CT_WRITE;
	}

	if(ISVAR_DEV(bp->b_dev)) {
		/* check that request is within drive limits */
		if(bp->b_bcount < ctinfo->ct_cur_minsz)
			goto badcnt;
		/* catch attempts to open the variable mode device for 
		 * drives that don't support it. Be paranoid, in case
		 * minsz not set due to an error. */
		if(ctinfo->ct_cur_minsz &&
			ctinfo->ct_cur_minsz == ctinfo->ct_cur_maxsz &&
			(bp->b_bcount%ctinfo->ct_cur_minsz)) {
			ct_dbg((C, "min==max, and remainder; prob var mode not allowed.  "));
			goto badcnt;
		}
		if(bp->b_bcount > ctinfo->ct_cur_maxsz) {
			if(bp->b_flags & B_READ) {	/* adjust down, this
				* allows determining the block size in the standard
				* unix method, by using a large read request. */
				extraresid =  bp->b_bcount - ctinfo->ct_cur_maxsz;
				bp->b_bcount = ctinfo->ct_cur_maxsz;
			}
			else
				goto badcnt;
		}
	}
	else if(bp->b_bcount % ctinfo->ct_blksz) {
		ct_dbg((C, "bcnt=%x, blksz=%x, bad cnt. ",
			bp->b_bcount, ctinfo->ct_blksz));
		/* if reading, and request size is multiple of actual block
		 * size on tape, allow it, even if not multiple of ct_blksz. */
		if(bp->b_flags & B_READ)
			goto badcnt; /*	must be no remainder.  */
		/* if at BOT, try to get actual blocksize first; unlikely
		 * to have done an ioctl at this point, particularly for
		 * standalone; if curblksz different, we must have
		 * already tryed to get/set it. */
		if((ctinfo->ct_state & CT_BOT) &&
			ctinfo->blkinfo.curblksz == ctinfo->ct_blksz) {
			ct_dbg((C, "chk actual size. "));
			ctgetblklen(ctinfo, odev);
			bp->b_error = 0;	/* ignore any errs */
		}
		else ct_dbg((C, "not at bot (%x), or blksz diff, no getblklen. ",
		ctinfo->ct_state));
		if(!ctinfo->blkinfo.curblksz ||
			(bp->b_bcount % ctinfo->blkinfo.curblksz))
			goto badcnt; /*	must be no remainder.  */
		else ct_dbg((C, "but mult of curblk=%x and reading. ",
			ctinfo->blkinfo.curblksz));
	}

	/* If we are doing a read, we must check that the device has
	 * never done a write. All of our tapes fail an attempt to
	 * alternate reads and writes between filemarks.  intermixed
	 * reads and writes are allowed in audio mode to allow for editting.  */

	/* if determining block size, never use swap buffer.  Otherwise,
	 * if writing, byte swap data first.  Note that when writing, we
	 * must byte swap again at end, in case user is still using the
	 * data.  This is wasteful with tar, etc., but much simpler than
	 * the old way of allocating a buffer and doing the i/o in chunks.
	 * The in place swap is about as fast as the swbcopy that we used
	 * to do (slightly faster), but on writes we have to do two of them.
	 * for a 200Kb buffer, this means about an extra 80ms per write on
	 * an IP10, and slightly reduces the time when reading.  Slow devices
	 * like the QIC tapes still have no trouble streaming on writes.
	 * Also, we don't beat up the cache as much, and we use less code
	 * and time dealing with the swap buffer.  Finally, it fixes the
	 * case of programs (and standalone) where a small i/o is done
	 * first, followed by larger i/o's later; the old way was very
	 * slow under these conditions. */
	if(ISSWAP_DEV(bp->b_dev) && !(ctinfo->ct_state&CT_GETBLKLEN) &&
		!(bp->b_flags & B_READ)) {
		swapbytes((u_char *)bp->b_dmaaddr, bp->b_bcount);
		dcache_wb(bp->b_dmaaddr, bp->b_bcount);
	}

	/* if doing io with audio media, but not audio mode, clear audio
	 * media.  Conversely, if audio mode, but not audio media, 
	 * set it.  In both cases, put drive into correct mode.
	 * This way audio media bit always correctly reflects the
	 * state of the information on the tape.
	 * This has to be checked on reads as well, so we get the
	 * incompatible media message if in the 'wrong' mode.
	 * Only other place aud_med cleared is on unit atn. */
	if((ctinfo->ct_state&(CT_AUDIO|CT_AUD_MED)) &&
	    (ctinfo->ct_state&(CT_AUDIO|CT_AUD_MED)) != (CT_AUDIO|CT_AUD_MED)) {
	    ct_xdbg(1,(C, "aud mode or media, but not both (%x); fix. ",
			ctinfo->ct_state));
		if(ctinfo->ct_state&CT_READ) {
			/* we can be sure we have already done the msense 
			 * by this point... */
			ctreport(ctinfo, "Incompatible %s when reading\n",
				(ctinfo->ct_state&CT_AUDIO) ? "tape mode" : "media");
			bp->b_error = EINVAL;
			goto errdone;
		}
		if(ctinfo->ct_state & CT_AUDIO)
		    ctinfo->ct_state |= CT_AUD_MED;
		else
		    ctinfo->ct_state &= ~CT_AUD_MED;
		ctcmd(ctinfo, ctmodesel, 0, SHORT_TIMEO, 0);
	    ct_xdbg(1,(C, "new state=%x. ", ctinfo->ct_state));
		bp->b_error = 0;	/* ignore errors */
		bp->b_flags &= ~B_ERROR;
	}

reread:
	ctinfo->ct_subchan->s_buf = bp;	/* setup for ctcmd */
	ctcmd(ctinfo, ctxfer, bp->b_bcount, XFER_TIMEO(bp->b_bcount),0); 

done:
	bp->b_resid += extraresid;	/* wasted if error, but most will
		succeed, so don't add an extra check */

	/* first read after open, and at FM means that we were at the BOT side
	 * of a FM, and the drive has skipped over it, so just retry the read.
	 * make sure that we only try to skip over one fm, in case there are
	 * sequential FM's, like on 9 track. */
	if(!skippedfm && !bp->b_error && bp->b_bcount == bp->b_resid &&
	    (ctinfo->ct_state & (CT_DIDIO|CT_READ|CT_FM)) == (CT_READ|CT_FM)) {
	    ct_dbg((C, "1st read, and at FM, so retry. "));
	    skippedfm = 1;
	    goto reread;
	}
	if(!bp->b_error && bp->b_bcount != bp->b_resid) {
	    ctinfo->ct_state |= CT_DIDIO;
    
	    if(bp->b_resid && !ISVAR_DEV(bp->b_dev) &&
		(ctinfo->ct_state & CT_FM)) {
		ct_dbg((C, "short read hit fm: sets HITFM. "));
		ctinfo->ct_state |= CT_HITFMSHORT;
	    }
	}
	iodone(bp);

	if(ISSWAP_DEV(bp->b_dev) && !(ctinfo->ct_state&CT_GETBLKLEN)) {
		/* read AND write */
		swapbytes((u_char *)bp->b_dmaaddr, bp->b_bcount);
#if VCE_WAR
		dcache_wbinval(bp->b_dmaaddr, bp->b_bcount);
#endif
	}

	tpsc_blkno += (bp->b_bcount-bp->b_resid) / ctinfo->ct_blksz;
	if(ctinfo->blkinfo.curblksz &&
		((bp->b_bcount-bp->b_resid) % ctinfo->blkinfo.curblksz))
		tpsc_blkno++;	/* one block further */
	return;

badcnt:
	if(!bp->b_error)	/* could be set in ctdosizes() */
		bp->b_error = EINVAL;
	ct_dbg((C, "bad xfer sz %x, err %d.  ", bp->b_bcount, bp->b_error));
errdone:	/* b_error already set */
	bp->b_flags |= B_ERROR;
	iodone(bp);
}


/*
 * ctcmd
 *   Issue a command to a SCSI target.
 *   `func' is a pointer to a function which sets up the specific command
 *   and will parse any errors as a result. `timeo'  is the total length
 *   of time to completion, including delays due to BUSY status.
 *   This time may be lengthed by ct_extrabusy if the previous command
 *   had the immediate bit set (rewinds, unload, etc.).
 *   Caller must set ct_subchan->s_buf to an appropriate buf_t *
 *   for most commands.  This is the only way that error information
 *   is returned...
 *  Needed a second arg for some functions, most don't use it.
*/
static void
ctcmd(ctinfo_t *ctinfo, void (*func)(register ctinfo_t *,
				     int, uint, uint),
      uint arg, int timeo, uint arg2)
{
	register buf_t		*bp;
	buf_t *sbp;
	int tries = 3;	/* up to 3 re-tries on unitatn, because
		some archive qic150 drives give us two in a row... */
	register scsisubchan_t *sbcp = ctinfo->ct_subchan;
	ulong savstate = ctinfo->ct_state;
	int extra,cnt=0;

	/* add interval for immediate mode busy.  Always allow at least
	 * 3 retries on busy; we used to only do this for specific commands.
	 * The exception is that if timeo is negative, we do not block on BUSY
	 * status.  This is currently needed only for the tstrdy from MTIOCGET;
	 * see the comments there for more info. */
	if(timeo >= 0)
		extra = ctinfo->ct_extrabusy + 3*BUSY_INTERVAL;
	else {
		extra = 0;
		timeo = -timeo;
	}

	sbp = sbcp->s_buf; /* for cmds like ctxfer() when retrying */
	while(extra >= 0) {
		/* always set to the original timeout val; inside loop
		 * in case we do a reqsense, etc. */
		sbcp->s_timeoutval = timeo;
		/* zero here so we don't have to do in each function */
		bzero(&ctinfo->ct_cmd, sizeof(ctinfo->ct_cmd));
		cnt = sbp->b_bcount;	/* only used for debug */
		(*func)(ctinfo, CTCMD_SETUP, arg, arg2); /* do command setup */

		/* these are done here, so not duplicated in each func */
		switch((ctinfo->ct_lastcmd=ctinfo->ct_g0cdb.g0_opcode) & 0x60) {
		case 0x20:	/* all group 1 and 2 commands */
		case 0x40:
			sbcp->s_len = SC_CLASS1_SZ;
			break;
		default:
			sbcp->s_len = SC_CLASS0_SZ;
		}
		bp = sbcp->s_buf; /* ct_bp, ct_reqbp, or from physio() */
		bp->b_error = 0;
		/* must have a valid state in ctmotion for every cmd issued. */
		ASSERT(sizeof(ctmotion) > ctinfo->ct_lastcmd);
		if((ctinfo->ct_state & CTPOS) && ctmotion[ctinfo->ct_lastcmd]) {
			/* Can't do this only if ! MOTION, because ctloaded()
			 * sets BOT|MOTION on success, and we need to be
			 * sure we clear BOT on subsequent motion cmds */
			ctinfo->ct_state &= ~CTPOS;
			ctinfo->ct_state |= CT_MOTION;
		}
		ct_xdbg(1,(C, "%s ", cmdnm(ctinfo->ct_lastcmd)));
		ct_cxdbg(func==ctspace, 2,(C, "(%s), cnt %d ",
		    ctspctyp[arg], cnt));
		ct_cxdbg(func!=ctspace, 2,(C, "%d %d ", arg, arg2));
		doscsi(sbcp);

		if(sbcp->s_error != SC_GOOD) {
			bp->b_flags |= B_ERROR;
			bp->b_error = EIO;
			ct_dbg((C, "%s fails: s_err=%x stat=%x st=%x.  ",
				cmdnm(ctinfo->ct_lastcmd), sbcp->s_error, sbcp->s_status,
				ctinfo->ct_state));
			(*func)(ctinfo, CTCMD_ERROR, arg, arg2);
			break;
		}

		if(sbcp->s_status == ST_GOOD) {
			if(func == ctxfer && (savstate &
			  (CT_AUDIO|CT_AUD_MED|CT_BOT|CT_READ)) == CT_BOT) {
				/* This hack is for detecting write protected tapes
				 * (mostly qic drives) when writing with small block
				 * sizes.  Otherwise backup programs may think their
				 * first write was OK, so there must have been a media
				 * error when they get an error on their 2nd or
				 * nth write.  The wfm of 0 flushes the tape
				 * buffer to the tape, at which point we detect
				 * the qic24... */
				ctinfo->ct_subchan->s_buf = ctinfo->ct_bp;
				ct_dbg((C, "flush buf on 1st write from BOT (%x sav=%x). ",
					ctinfo->ct_state, savstate));
				ctcmd(ctinfo, ctwfm, 0, SHORT_TIMEO, 0);
				if(ctinfo->ct_bp->b_error) {
					sbp->b_error = ctinfo->ct_bp->b_error;
					sbp->b_flags |= B_ERROR;
					(*func)(ctinfo, CTCMD_ERROR, arg, arg2);
					break;
				}
			}
			(*func)(ctinfo, CTCMD_NO_ERROR, arg, arg2);
			break;
		}
		else {
			/* If device is busy, and retries are indicated, we
			 * do the retries until they expire. If a check
			 * condition occurred we call ctchkerr to get the
			 * sense info, and set b_error, etc.  Clear xtrabusy
			 * here, as it is clear we are doing a new cmd now;
			 * if a second immediate mode cmd is being done, we
			 * will reset extrabusy when we go back through the
			 * loop.  This means if no other cmd gets issued and
			 * gets a busy, extrabusy remains set, but that is
			 * not really a problem, as they device shouldn't
			 * return BUSY for anything else, except maybe a BUS
			 * reset. */
			if(sbcp->s_status == ST_BUSY) {
				ctinfo->ct_extrabusy = 0;
				if(extra) {
				    ct_dbg((C, "BUSY:%d left",extra/HZ));
				    delay(BUSY_INTERVAL);
				    extra -= BUSY_INTERVAL;
				    sbcp->s_buf = sbp;
				    continue;
				}
				ct_dbg((C, "BUSY, giving up. "));
				sbp->b_error = EBUSY;
				/* and fall through to CTCMD_ERROR below */
			}
			else if(sbcp->s_status == ST_CHECK) {
				int ret;
				ret = ctchkerr(ctinfo);
				/* restore s_buf for both retries,
				 * and NO_ERROR callback */
				sbcp->s_buf = sbp;
				if(ret==SC_UNIT_ATTN || ret==SC_ERR_RECOVERED) {
				    if(--tries > 0) {
					    /* retry the cmd */
					    continue;
				    }
				    ct_dbg((C, "Too many retries.  "));
				    bp->b_flags |= B_ERROR;
				    bp->b_error = EIO;
				}
				else if(ret == 0) {
				    /* some cmds are ok on a chk! */
				    (*func)(ctinfo, CTCMD_NO_ERROR, arg, arg2);
				    break;
				}
			}

			ct_dbg((C, "%s fails, stat %x.  ",
				cmdnm(ctinfo->ct_lastcmd), sbcp->s_status));

			/* 'errors' other than busy and check just fail, (and
			 * busies when the max time expires) */
			(*func)(ctinfo, CTCMD_ERROR, arg, arg2);
			break;
		}
	}
}

/* Check for existence of the device and verify it is a tape drive.  */
/*ARGSUSED*/
static void
ctinq(ctinfo_t *ctinfo, int flag, uint arg, uint dummy)
{
	register ct_g0inq_data_t	*ptr;
	register buf_t *bp = ctinfo->ct_bp;

	ptr = &ctinfo->ct_inq_data;

	if(flag == CTCMD_SETUP) {
		ctinfo->ct_g0cdb.g0_opcode = INQUIRY_CMD;
		ctinfo->ct_g0cdb.g0_cmd3 = arg;
		bp->b_dmaaddr = (caddr_t)ptr;
		bp->b_bcount = arg;
		bp->b_flags = B_READ | B_BUSY;
		ctinfo->ct_subchan->s_buf = bp;
		if (cachewrback)
			dcache_inval(ptr, arg);
		return;
	}
	dcache_inval(ptr, arg);

	if(bp->b_error || flag == CTCMD_ERROR)
		return;

	/* now verify that this device is really a tape.
	 * check:
	 *   peripheral device type bit(s) (can't just and it; lots more types
	 *  than just 0 and 1 now! (mt exist on an optical drive (removable, and
	 *  type 7) returned sucessfully before!)
	 *	removeable media bit(s)
	 *	ansi standard bit(s)
	 */
	if(ptr->id_pdt!=INQ_PDT_CT || !ptr->id_rmb || !ptr->id_ansi) {
		ct_dbg((C, "ctinq: bits not good for tape.  "));
		bp->b_error = ENODEV;
	}
}



/* Set the mode select parameters.  The arg is the number of bytes of vendor
 * specific data we send (usually from ctinfo->ct_typ.tp_msl_pglen) */
/*ARGSUSED*/
static void
ctmodesel(ctinfo_t *ctinfo, int flag, uint arg, uint dummy)
{
	register ct_g0msl_data_t	*ptr;

	if(flag != CTCMD_SETUP)
		return;

	ptr = &ctinfo->ct_msl;
	/* by zeroing the independent portion, we set default density, etc.
	 * Bytes past arg in vendor specific area are left alone; and often are
	 * set from a preceding mode sense.  */
	bzero(ptr, MSD_IND_SIZE);

	/* set data always required, unless data being supplied by caller */
	if(!arg && ctinfo->ct_typ.tp_msl_pglen) {
		bcopy(ctinfo->ct_typ.tp_msl_data, ptr->msld_vend,
			ctinfo->ct_typ.tp_msl_pglen);
		arg = ctinfo->ct_typ.tp_msl_pglen;
	}

	/* change size, set params, etc, as required.  DAT fills in the
	 * msld_vend for some actions before calling us, but we currently
	 * don't do that for any other drives.  */
	if(IS_CIPHER(ctinfo))
		ptr->msld_vend[0] = ctinfo->ct_cipherrec;

	/* Note that if/when we support different densities (compression
	  maybe?) on DAT, that this code may have to be modified to work
	  with the audio stuff below */
	if(ctinfo->ct_typ.tp_capablity & MTCAN_SETDEN)
		ptr->msld_descode = ctinfo->ct_density;

	if(IS_DATTAPE(ctinfo)) {
	if(ctinfo->ct_state & CT_AUD_MED)
		/* force to audio mode, only if already aud media set.;
		 * that is, modesense said so, or we are in audio mode,
		 * and have written to tape. */
		ptr->msld_descode = 0x80;
	}

	ct_cxdbg(ptr->msld_descode==0x80, 3,(C, "force aud_med. "));
	ct_cxdbg(ptr->msld_descode!=0x80, 3,(C, "force NO aud_med (dens=%x). ",
		ptr->msld_descode));

	if(ctinfo->ct_typ.tp_capablity & MTCAN_SETSP)
		ptr->msld_speed = ctinfo->ct_speed ? 2 : 1;

	if(ctinfo->ct_scsiv > 1)
		ctinfo->ct_g0cdb.g0_cmd0 = 0x10;	/* SCSI 2 PF bit */

	ctinfo->ct_g0cdb.g0_opcode = MODE_SEL_CMD;
	ctinfo->ct_bp->b_bcount = ctinfo->ct_g0cdb.g0_cmd3 =
		arg + MSD_IND_SIZE;

	ctinfo->ct_bp->b_dmaaddr = (caddr_t)ptr;
	ctinfo->ct_bp->b_flags = B_WRITE | B_BUSY;

	ctinfo->ct_subchan->s_buf = ctinfo->ct_bp;

	ptr->msld_bfm = 1;		/* buffered mode */
	ptr->msld_bdlen = MSLD_BDLEN;

	/* if getting blklen, force variable mode */
	if(!(ctinfo->ct_state&CT_GETBLKLEN) &&
		!ISVAR_DEV(ctinfo->ct_bp->b_dev)) {
		ptr->msld_blklen[2] = (u_char) ctinfo->ct_blksz;
		ptr->msld_blklen[1] = (u_char)(ctinfo->ct_blksz >> 8);
		ptr->msld_blklen[0] = (u_char)(ctinfo->ct_blksz >> 16);
	}
	/* else 0 indicates variable size blocks */

#ifdef SCSICT_DEBUG
	if(ctdebug>3) {
		int i;
		unchar *d = (unchar *)ptr;
		printf("msel hdr: ");
		for(i=0; i<4;i++)
			printf("%x ", d[i]);
		printf(" blk descr: ");
		for(; i<MSD_IND_SIZE;i++)
			printf("%x ", d[i]);
		if(arg) {
			printf(" page data: ");
			for(i=0; i<arg;i++)
				printf("%x ", ptr->msld_vend[i]);
			printf("\n");
		}
	}
#endif /* SCSICT_DEBUG */

	dcache_wb(ctinfo->ct_bp->b_dmaaddr, ctinfo->ct_bp->b_bcount);
}

/* Test to see if the device is ready to accept media access commands.  */
/*ARGSUSED2*/
static void
cttstrdy(register ctinfo_t *ctinfo, int flag, uint arg, uint dummy)
{
	if(flag == CTCMD_SETUP) {
		ctinfo->ct_g0cdb.g0_opcode = TST_UNIT_RDY_CMD;
		ctinfo->ct_bp->b_bcount = 0;
		ctinfo->ct_bp->b_flags = B_BUSY;
		ctinfo->ct_subchan->s_buf = ctinfo->ct_bp;
	}
	else if(flag == CTCMD_NO_ERROR) {
		if(ctinfo->ct_typ.tp_capablity&MTCAN_CHKRDY)
			ctinfo->ct_state |= CT_ONL; /* ready for other cmds */
	}
	else if(ctinfo->ct_bp->b_error != EBUSY)
		/* in case the error wasn't one which clears ONL; EBUSY
		 * means we must be in the middle of a seek or rewind
		 * with immediate bit set, so don't clear ONL */
		ctinfo->ct_state &= ~CT_ONL;
}

/*
 *   Get the mode sense info from the device.
 *   Set/clear state bits as appropriate, including WRP, QIC24, and QIC120.
*/
static void
ctmodesens(ctinfo_t *ctinfo, int flag, uint arg, uint arg2)
{
	register ct_g0ms_data_t	*ptr;

	ptr = &ctinfo->ct_ms;

	if(flag == CTCMD_SETUP) {
		ctinfo->ct_g0cdb.g0_opcode = MODE_SENS_CMD;
		ctinfo->ct_g0cdb.g0_cmd3 = arg;
		ctinfo->ct_g0cdb.g0_cmd1 = arg2; /* which page */

		ctinfo->ct_bp->b_dmaaddr = (caddr_t)ptr;
		ctinfo->ct_bp->b_bcount = arg;
		ctinfo->ct_bp->b_flags = B_READ | B_BUSY;
		ctinfo->ct_subchan->s_buf = ctinfo->ct_bp;

		if (cachewrback)
			dcache_inval(ptr, arg);
		return;
	}
	dcache_inval(ptr, arg);

	if(flag == CTCMD_ERROR)
		return;

	/* Successful */
	if(ptr->msd_wrp) {
		ct_dbg((C, "modesens: wrp set.  "));
		ctinfo->ct_state |= CT_WRP;
	}
	else
		ctinfo->ct_state &= ~CT_WRP;
	if(IS_VIPER150(ctinfo) || IS_TANDBERG(ctinfo)) {
		/* note we check density only for the viper150; QIC24 'always'
		* the case for older drives, since we don't support QIC11, and
		* only the 150 cares.
		* Density has following meaning on VIPER 150:
		*	0x10 - QIC150(18 tracks); 0xf - QIC120 (15 tracks);
		* 	5 - QIC24 (9 tracks); a QIC24 tape is readonly
		* 	0 - if set idrive should adapt to type of tape present;
		* 	otherwise attempt indicated format only.
		*
		* Some other drives:
		* 	Cipher 540s: 5 - QIC24 9trk; 4 - QIC11 4trk;
		*	0x84 - QIC11 9track.
		* 	Cipher 150S: 0,5 QIC24 9 track; 0xf - QIC120 15 track;
		* 		0x10 - QIC150 18 track (not qualified yet).
		*
		* Unfortunately, the density code is set ONLY after a successful
		* read or space cmd.  A firmware change has been requested,
		* but we still have to live with the older drives...
		*
		* Worse yet, the density code refers to the TAPE format,
		* NOT the cartridge format.  This means that if someone
		* reads a high density tape that was written in QIC24
		* format, we refuse to let them write it unless they
		* first remove and then re-insert the tape. BAD!  So we
		* have to clear the QIC24 bits in open after this sense
		* is done, if open for writing...  */
		if(ptr->msd_descode) {
			/* if not default, turn off all density info first, in
			 * case opposite set, or now QIC150 and either set. */
			ctinfo->ct_state &= ~(CT_QIC120|CT_QIC24);
			if(ptr->msd_descode == 5)
				ctinfo->ct_state |= CT_QIC24;
			else if(ptr->msd_descode == 0xf)
				ctinfo->ct_state |= CT_QIC120;
		}
	}
	if(IS_DATTAPE(ctinfo) && ptr->msd_descode == 0x80) {
			ctinfo->ct_state |= CT_AUD_MED;
			ct_xdbg(1, (C, "set aud_med on sense. "));
	}
	else {
		ctinfo->ct_state &= ~CT_AUD_MED;
		ct_xdbg(1, (C, "clr aud_med on sense. "));
	}

#ifdef SCSICT_DEBUG
	if(ctdebug>3) {
		int i; unchar *d = (unchar *)ptr;
		i = (arg < MSD_IND_SIZE) ? arg : MSD_IND_SIZE;
		printf("msen hdr: ");
		while(i-->0)
			printf("%x ", *d++);
		if(arg>MSD_IND_SIZE && ptr->msd_len>MSD_IND_SIZE) {
			printf("; data: ");
			for(i=0; i< (ptr->msd_len-MSD_IND_SIZE);i++)
				printf("%x ", ptr->msd_vend[i]);
		}
		printf("\n");
	}
#endif /* SCSICT_DEBUG */
}


/* Rewind the tape.  */
/*ARGSUSED*/
static void
ctrewind(ctinfo_t *ctinfo, int flag, uint dummy1, uint dummy2)
{
	if(flag == CTCMD_SETUP) {
		ctinfo->ct_g0cdb.g0_opcode = REWIND_CMD;
		ctinfo->ct_bp->b_bcount = 0;
		ctinfo->ct_bp->b_flags = B_BUSY;
		ctinfo->ct_subchan->s_buf = ctinfo->ct_bp;
		if((ctinfo->ct_state & CT_AUDIO) || tpsc_immediate_rewind) {
			/* return successful status immediately, so readposn,
			 * etc can be done to monitor position during the
			 * operation (in audio mode).  */
			ctinfo->ct_g0cdb.g0_cmd0 = 1;
			ctinfo->ct_extrabusy = ctinfo->ct_subchan->s_timeoutval;
		}
	}
	else if(flag == CTCMD_NO_ERROR) {
		/* if tape is rewound OK, forget about reads and
		 * writes; this also prevents us from writing a FM
		 * at BOT if programs rewinds prior to closing;
		 * unfortunately, in this case the tape won't have
		 * a FM at the end of the data just written, it's
		 * assumed the program knows what it's doing... */
		ctinfo->ct_state &= ~(BACKSTATES|CT_READ|CT_WRITE);
		ctinfo->ct_state |= CT_BOT|CT_ONL;
		(void)ctloaded(ctinfo);	/* this sets the MOTION bit and
			* does the modesel if needed; if we just set the MOTION
			* bit here, then the modesel might never get done */
		tpsc_blkno = 0;
	}
}

/*
 *  Position the tape using the space command.  Note that not all
 *	drives support all ops; they should probably be checked before
 *  this routine is called.
 *  If successful, and not a space blk cmd, forget reads or writes.
 */
/*ARGSUSED*/
static void
ctspace(register ctinfo_t *ctinfo, int flag, uint arg, uint dummy)
{
	buf_t *bp = ctinfo->ct_subchan->s_buf;

	if(flag == CTCMD_SETUP) {
		ctinfo->ct_g0cdb.g0_opcode = SPACE_CMD;
		ctinfo->ct_g0cdb.g0_cmd0 = arg;	/* sub-function */
		ctinfo->ct_g0cdb.g0_cmd1 = HI8(bp->b_bcount);
		ctinfo->ct_g0cdb.g0_cmd2 = MD8(bp->b_bcount);
		ctinfo->ct_g0cdb.g0_cmd3 = LO8(bp->b_bcount);
		bp->b_bcount = 0;	/* for doscsi */
		bp->b_flags = B_BUSY;
		/* clear these when we start, since we don't know
		 * where we will be on an error, unless perhaps
		 * the request sense tells us.  Even on failure,
		 * we should allow reads if we were writing, and
		 * vice-versa (subject to drive constraints later)
		 * since they have attempted the required action. */
		ctinfo->ct_state &= ~(CT_DIDIO|CT_READ|CT_WRITE);
	}
	else if(flag == CTCMD_ERROR) {
		ct_dbg((C, "space fails.  "));
	}
	else {
		if(arg == SPACE_FM_CODE) {
			ctinfo->ct_state |= CT_FM;
			if((int)bp->b_bcount < 0) /* so that a read after an
				* mt bsf will skip to the EOT side of a FM in
				* ctloaded(), or MTFSF */
				ctinfo->ct_state |= CT_DIDIO;
		}
		else if(arg == SPACE_EOM_CODE)
			ctinfo->ct_state |= CT_EOD;
		else if(arg == SPACE_SETM_CODE)
			ctinfo->ct_state |= CT_SMK;
	}
}

/*
 *   Performs the cdb setup and low level error checking for reads and
 *   writes.
 */
/*ARGSUSED*/
static void
ctxfer(register ctinfo_t *ctinfo, int flag, uint arg, uint dummy)
{
	buf_t *bp = ctinfo->ct_subchan->s_buf;

	if(flag == CTCMD_SETUP) {

		ctinfo->ct_g0cdb.g0_opcode = (bp->b_flags & B_READ) ?
			READ_CMD : WRITE_CMD;

		if(!(ctinfo->ct_state & CT_GETBLKLEN)) {
			if(ctinfo->ct_sili) /* suppress ILI */
				ctinfo->ct_g0cdb.g0_cmd0 |= 2;
			if(!ISVAR_DEV(bp->b_dev)) {
				/* for resid cnt chks; curblksz set back to 0 on errs */
				if(!(bp->b_flags & B_READ))
					ctinfo->blkinfo.curblksz = ctinfo->ct_blksz;
				arg /= ctinfo->ct_blksz;
				ctinfo->ct_g0cdb.g0_cmd0 = 1;
				ct_xdbg(2,(C, "fixed blksz=%d/%d blks. ",
					ctinfo->ct_blksz, arg));
			}
		}
		/* else determining blk len, using variable mode, and
		 * want ILI's */

		ctinfo->ct_g0cdb.g0_cmd1 = HI8(arg);
		ctinfo->ct_g0cdb.g0_cmd2 = MD8(arg);
		ctinfo->ct_g0cdb.g0_cmd3 = LO8(arg);
	}
	else if(flag == CTCMD_NO_ERROR) {
		/* ACTUAL bytes done, not requested size! */
		if(ISVAR_DEV(bp->b_dev))
			ctinfo->blkinfo.curblksz = bp->b_bcount -
				bp->b_resid;
		else	/* necessary to do it here to get it right */
			ctinfo->blkinfo.curblksz = ctinfo->ct_blksz;
		if(bp->b_flags & B_READ)
			ctinfo->ct_readcnt++;
		else
			ctinfo->ct_writecnt++;
	}
	else	/* error; need to figure it on on next ioctl request */
		ctinfo->blkinfo.curblksz = 0;
}




/* Write File Marks */
static void
ctwfm(register ctinfo_t *ctinfo, int flag, uint arg, uint arg2)
{
#ifndef PROM  /* never used in PROM */
	if(flag == CTCMD_SETUP) {
		ctinfo->ct_g0cdb.g0_opcode = WFM_CMD;
		/* if arg2 is 2, then set marks, not file marks */
		ctinfo->ct_g0cdb.g0_cmd0 = arg2;
		ctinfo->ct_g0cdb.g0_cmd1 = HI8(arg);
		ctinfo->ct_g0cdb.g0_cmd2 = MD8(arg);
		ctinfo->ct_g0cdb.g0_cmd3 = LO8(arg);

		ctinfo->ct_bp->b_bcount = 0;
		ctinfo->ct_bp->b_flags = B_BUSY;

		ctinfo->ct_subchan->s_buf = ctinfo->ct_bp;
	}
	else if(flag == CTCMD_NO_ERROR) {
		ctinfo->ct_state &= ~CT_DIDIO;
		ctinfo->ct_state |= arg2==0?CT_FM:CT_SMK;
	}
#endif /* PROM */
}


/*
 *   Load, unload or retension.  The `arg' is the specific action to take.
 *   Note that the CT_MOTION bit is never set here, because then we might
 *   not do a needed modeselect.
 */
/*ARGSUSED*/
static void
ctload(register ctinfo_t *ctinfo, int flag, uint arg, uint dummy)
{
	if(flag == CTCMD_SETUP) {
		ctinfo->ct_g0cdb.g0_opcode = L_UL_CMD;
		ctinfo->ct_g0cdb.g0_cmd3 = arg & 3;
		ctinfo->ct_bp->b_bcount = 0;
		ctinfo->ct_bp->b_flags = B_BUSY;
		ctinfo->ct_subchan->s_buf = ctinfo->ct_bp;
		if(!(arg&L_UL_LOAD) && tpsc_immediate_rewind) {
		    /* when unloading, or retensioning */
		    ctinfo->ct_g0cdb.g0_cmd0 = 1;
		    ctinfo->ct_extrabusy = ctinfo->ct_subchan->s_timeoutval;
		}
	}
	else if(flag == CTCMD_NO_ERROR) {
		/* if we completed successfully, we are either at
		 * PBOT, or LBOT, depending on the cmd.  If at
		 * PBOT, we are effectively offline, and need a
		 * load cmd before next tape motion (and some other
		 * cmds, depending on drive type).  This is almost
		 * the same as changing a tape, except we can
		 * remember the QIC* bits.  If tape is
		 * loaded/unloaded OK, forget about reads and
		 * writes; this also prevents us from writing a FM
		 * at BOT if programs rewinds/offlines prior to
		 * closing; unfortunately, in this case the tape
		 * won't have a FM at the end of the data just
		 * written, it's assumed the program knows what
		 * it's doing... */
		ctinfo->ct_state &= ~(BACKSTATES|CT_ONL|CT_READ|CT_WRITE);
		if(arg & L_UL_LOAD) {
			ctinfo->ct_state |= CT_ONL|CT_LOADED|CT_BOT;
			tpsc_blkno = 0;
		}
	}
}

/*
 *   Request Sense command
 * NOTE: this uses a different bp from all other cmds in the driver
 * so that we won't trash the counts, flags, etc. in the 'real' one.
 * This isn't a problem for ctstrategy() calls since they use a bp
 * physio() allocates, but it is for all other calls.
 * ONLY called from ctchkerr(), so has to do some things normally done
 * in ctcmd().
*/
static int
ctreqsense(ctinfo)
register ctinfo_t *ctinfo;
{
	buf_t *bp = ctinfo->ct_reqbp;
	register scsisubchan_t *sp = ctinfo->ct_subchan;

	/* done by ctcmd for other cmds */
	bzero(&ctinfo->ct_g0cdb, sizeof(ct_g0cdb_t));
	bp->b_error = 0;
	sp->s_timeoutval = SHORT_TIMEO;

	ctinfo->ct_g0cdb.g0_opcode = REQ_SENSE_CMD;
	ctinfo->ct_g0cdb.g0_cmd3 = sizeof(ctinfo->ct_es_data);

	bp->b_dmaaddr = (caddr_t)&ctinfo->ct_es_data;
	bp->b_bcount = sizeof(ctinfo->ct_es_data);
	bp->b_flags = B_READ | B_BUSY;

	if (cachewrback)
		dcache_inval(bp->b_dmaaddr, bp->b_bcount);
	sp->s_buf = bp;
	doscsi(sp);
	dcache_inval(bp->b_dmaaddr, bp->b_bcount);

	/* if the request sense failed for any reason,
		* we just give up */
	if(sp->s_error != SC_GOOD || sp->s_status != ST_GOOD) {
		ct_dbg((C, "REQSENSE failed, err %x, stat %x.  ",
			sp->s_error, sp->s_status));
		return -1;
	}
	return 0;
}

/* Prevent/Allow  Media removal command */
/*ARGSUSED*/
static void
ctprevmedremov(register ctinfo_t *ctinfo, int flag, uint arg, uint dummy)
{
	if(flag == CTCMD_SETUP) {
		ctinfo->ct_g0cdb.g0_opcode = PREV_MED_REMOV_CMD;
		ctinfo->ct_g0cdb.g0_cmd3 = arg;

		ctinfo->ct_bp->b_bcount = 0;
		ctinfo->ct_bp->b_flags = B_BUSY;

		ctinfo->ct_subchan->s_buf = ctinfo->ct_bp;
	}
}

/*	get the block limits, for checking in ctstrategy, and for
 *	the MTIOCGETBLKINFO ioctl.
*/
/*ARGSUSED2*/
static void
ctblklimit(register ctinfo_t *ctinfo, int flag, uint arg, uint dummy)
{
	if(flag == CTCMD_SETUP) {
		buf_t *bp = ctinfo->ct_bp;
		ctinfo->ct_g0cdb.g0_opcode = REQ_BLKLIM_CMD;

		bp->b_dmaaddr = (caddr_t)ctinfo->ct_reqblkinfo;
		bp->b_bcount = sizeof(ctinfo->ct_reqblkinfo);
		bp->b_flags = B_READ | B_BUSY;

		if (cachewrback)
			dcache_inval(bp->b_dmaaddr, bp->b_bcount);

		ctinfo->ct_subchan->s_buf = bp;
	}
	else if(flag == CTCMD_NO_ERROR) {
		u_char *info = ctinfo->ct_reqblkinfo;

		dcache_inval(info, sizeof(ctinfo->ct_reqblkinfo));
		ctinfo->ct_cur_minsz = ((ulong)(info[4]) << 8) | info[5];
		ctinfo->ct_cur_maxsz = ((ulong)(info[1]) << 16) |
			((ulong)(info[2]) << 8) | info[3];
		if(ctinfo->ct_cur_minsz == 0)
		/* on Kennedy in fixed mode, if blksz is 0x10000, then
		 * min is 0, since it only has 2 bytes.  This means the
		 * Kennedy can't read tapes w blksizes > 64k in fixed
		 * mode.  It can't in variable mode either, but
		 * theoretically it could.  minsz==maxsz used to
		 * catch invalid i/o sizes in strat.  This would be
		 * true of any device with 64k or larger blocks when
		 * in fixed mode, so just do it. */
			ctinfo->ct_cur_minsz = ctinfo->ct_cur_maxsz;

		/* save the absolute max and min for the BLKINFO ioctl.
		 * Kennedy only one affected by this for now.  It
		 * retuns the min and max based on the values last set,
		 * if not at BOT */
		if(ctinfo->blkinfo.minblksz > ctinfo->ct_cur_minsz)
			ctinfo->blkinfo.minblksz = ctinfo->ct_cur_minsz;
		if(ctinfo->blkinfo.maxblksz < ctinfo->ct_cur_maxsz)
			ctinfo->blkinfo.maxblksz = ctinfo->ct_cur_maxsz;
	}
	else ct_dbg((C, "getblklimits fails.  "));
}


/*	get all the drive specific info after a request sense. Don't
 *	bother doing stuff that will be done anyway because of the
 *	primary sense key.
*/
static void
ctdrivesense(register ctinfo_t *ctinfo)
{
	ct_g0es_data_t	*esdptr = &ctinfo->ct_es_data;
	u_char addsense0 = 0, addsense1 = 0;
	uint recovers = 0;

	switch(ctinfo->ct_typ.tp_hinv) {
	case TP8MM_8200:
	case TP8MM_8500:
		recovers = (esdptr->exab.errs[0] << 16) |
			(esdptr->exab.errs[1] << 8) | esdptr->exab.errs[2];
		addsense0 = esdptr->exab.as;
		addsense1 = esdptr->exab.aq;
		if(esdptr->exab.bot) {
			ctinfo->ct_state &= ~BACKSTATES;
			ctinfo->ct_state |= CT_BOT;
		}
		if(esdptr->exab.wp)
			ctinfo->ct_state |= CT_WRP;
		if(esdptr->exab.tnp)
			ctinfo->ct_state &= ~CT_ONL;
		if(esdptr->exab.pf)
			ct_dbg((C, "powerfail/reset.  "));
		if(esdptr->exab.bpe)
			ct_dbg((C, "scsi bus parity error.  "));
		if(esdptr->exab.fpe)
			ct_dbg((C, "buffer parity error.  "));
		if(esdptr->exab.me)
			/* tell the user; it's better than std error msg */
			ctreport(ctinfo, "Uncorrectable media error\n");
		if(esdptr->exab.eco)
			ct_dbg((C, "retry ctr overflow.  "));
		if(esdptr->exab.tme)
			ct_dbg((C, "tape motion error.  "));
		if(esdptr->exab.fmke)
			ct_dbg((C, "fmk write error.  "));
		if(esdptr->exab.ure)
			ct_dbg((C, "underrun error.  "));
		if(esdptr->exab.we1)
			ct_dbg((C, "max write retries error.  "));
		if(esdptr->exab.sse)
			/* this is difficult for user to diagnose, and we
			 * have seen a fair number of them, so
			 * actually tell the user about this */
			ctreport(ctinfo, "Exabyte servo failure\n");
		if(esdptr->exab.fe)
			ctreport(ctinfo, "Exabyte data formatter failure\n");
		if(esdptr->exab.tmd)
			ct_dbg((C, "TMD set.  "));
		if(esdptr->exab.xfr)
			ct_dbg((C, "XFR set.  "));
		if(esdptr->exab.wseb || esdptr->exab.wse0)
			ct_dbg((C, "write splice error.  "));
		ct_xdbg(2, (C, "%dK left.  ",
			(((ulong)esdptr->exab.left[0]) << 16) |
			(((ulong)esdptr->exab.left[1]) << 8) |
			(ulong)esdptr->exab.left[2]));
		break;
	case TP9TRACK:
		if(esdptr->esd_defer)
			ct_dbg((C, "DEFERed error: "));
		/* Kennedy is more like the disk drives; it provides
		 * additional sense bytes, but no recovered error cnt. */
		addsense0 = esdptr->kenn.as;
		addsense1 = esdptr->kenn.aq;
		if(addsense0 == 0) {
			switch(addsense1) {
			case 1:
				ctinfo->ct_state |= CT_FM;
				break;
			case 2:
				ctinfo->ct_state &= ~CTPOS;
				ctinfo->ct_state |= CT_EW;
				break;
			case 4:
				ctinfo->ct_state &= ~BACKSTATES;
				ctinfo->ct_state |= CT_BOT;
				break;
			case 5:
				ctinfo->ct_state &= ~CTPOS;
				ctinfo->ct_state |= CT_EOD;
				break;
			}
		}
		else if(addsense0 == 0x11 && addsense1 == 1 && 
			(ctinfo->ct_state & CT_EW))
			/* this happens when data was written past the
			eot marker almost to the very end of the tape */
			ctinfo->ct_state |= CT_EOM;
		else if(addsense0 == 0x27)
			ctinfo->ct_state |= CT_WRP;
		else if(addsense0 == 0x2E)
			ctinfo->ct_state |= CT_EOD;
		break;
	case TPQIC24:
		switch(ctinfo->ct_typ.tp_type) {
		case CIPHER540:
		    if(esdptr->esd_aslen) {	/* older firmware may have 0! */
			addsense0 = esdptr->ciph.as;
			if(addsense0 == 0x17)
			    ctinfo->ct_state |= CT_WRP;
			if(addsense0 == 0x34)
			    ctinfo->ct_state |= CT_EOD;
			ct_dbg((CE_NOTE, "Cipher, aslen %d, recerr %x,%x.  ",
			    esdptr->esd_aslen, esdptr->ciph.errs[0],
			    esdptr->ciph.errs[1]));
			recovers = (esdptr->ciph.errs[0] << 8) |
			    esdptr->ciph.errs[1];
		    }
		    break;
		case VIPER60:
		    recovers =
			(esdptr->viper.errs[0] << 8) | esdptr->viper.errs[1];
		    /* no additional sense. */
		    break;
		}
		break;
	case TPQIC150:
		switch(ctinfo->ct_typ.tp_type) {
		case TANDBERG3660:
		    recovers =
			    (esdptr->tand.errs[0] << 8) | esdptr->tand.errs[1];
		    addsense0 = esdptr->tand.ercl;
		    addsense1 = esdptr->tand.ercd;
		    switch(addsense0) {
		    case 0xc: case 0x11: case 0x31: case 0x34:
		    case 0x35: case 0x38: case 0x39: case 0x3A:
		    case 0x3B: case 0x3C: case 0x3D: case 0x3e: case 0x3F:
		    /* we report the ones that the manual claims aren't cleared
		     * by a reset or power cycle, to make life easier for
		     * support people */
			ctreport(ctinfo, "Unrecoverable drive error cl=%x,"
			    "cd=%x, exercd %x\n", addsense0, addsense1,
			    esdptr->tand.exercd);
			break;
		    case 0x1d:	/* writing and early warning */
			ctinfo->ct_state |= CT_EW;
			break;
		    case 0x17:	/* read LEOT (you get this on end of data;
			i.e. at a blank chk; not at EW, like most other mfg.) */
			ctinfo->ct_state |= CT_EOD;
			break;
		    case 0x18:	/* read PEOT */
		    case 0x1e:	/* writing */
			ctinfo->ct_state &= CT_BOT;	/* paranoia */
			ctinfo->ct_state |= CT_EW;
			break;
		    case 0x1f:
			ctinfo->ct_state |= CT_WRP;
			break;
		    }
		    break;
		case VIPER150:
		    recovers =
			(esdptr->viper.errs[0] << 8) | esdptr->viper.errs[1];
		    /* no additional sense. */
		    break;
		}
		break;
	case TPUNKNOWN:
		if(ctinfo->ct_scsiv > 1)
			goto scsi2as;	/* unknown, but scsi 2 */
		break;
	case TPDAT:
		if(ctinfo->ct_scsiv < 2)
		    /* scsi1 mode (we don't ship this, but customers...) */
		    recovers =
			(esdptr->viper.errs[0] << 8) | esdptr->viper.errs[1];
		else {
scsi2as:
			addsense1 = esdptr->rsscsi2.aq;
			/* these are only the 'interesting' info states, in that
			 * we either report them, or set a state flag, or set
			 * to report some different kind of error.  */
			switch(addsense0 = esdptr->rsscsi2.as) {
			case 0: switch(addsense1) {
			    case 1: ctinfo->ct_state |= CT_FM; break;
			    case 2: ctinfo->ct_state |= CT_EOM; break;
			    case 3: ctinfo->ct_state |= CT_SMK; break;
			    case 4: ctinfo->ct_state |= CT_BOT; break;
			    case 5: ctinfo->ct_state |= CT_EOD; break;
			    }
			    break;
			case 3:
			    if(addsense1 == 2)
				ctreport(ctinfo, "Excessive write errors\n");
			    break;
			case 0x1a:
			    ct_dbg((C, "invalid length for param list. "));
			    break;
			case 0x20:
			case 0x24:
			    ct_dbg((C, "invalid CDB. "));
			    break;
			case 0x26:
			    ct_dbg((C, "invalid param list. "));
			    break;
			case 0x27:
			    ctinfo->ct_state |= CT_WRP;
			    break;
			case 0x30:
			    if(addsense1 == 3)
				ctreport(ctinfo,
				    "Cleaning cartridge in drive\n");
			    if(addsense1 == 1)
				ctreport(ctinfo,
				    "Unknown physical tape format\n");
			    else
				ctreport(ctinfo,
				    "Incompatible media in drive\n");
			    break;
			case 0x31:
			    ctreport(ctinfo, "Media format is corrupted\n");
			    break;
			case 0x3a:
			    ctinfo->ct_state &= ~CT_ONL;
			    break;
			case 0x40:
			    ctreport(ctinfo,
				"Drive diagnostic failure: code 0x%x\n",
				addsense1);
			    break;
			}

			switch(esdptr->esd_sensekey) {
			case SC_ERR_RECOVERED:
			case SC_HARDW_ERR:
			case SC_MEDIA_ERR:
			    /* may want to jerk this when we go to logsense */
			    recovers +=
				(esdptr->rsscsi2.sensespecific[0] << 8)
				| esdptr->rsscsi2.sensespecific[1];
			    break;
			case SC_ILLEGALREQ:
			    if(esdptr->rsscsi2.sksv)
				ct_dbg((C, "illegal byte 0x%x in %s, bit %d\n",
				    (esdptr->rsscsi2.sensespecific[0] << 8)
				    | esdptr->rsscsi2.sensespecific[1],
				    esdptr->rsscsi2.cd?"cmd":"data",
				    esdptr->rsscsi2.bitp));
			    break;
			}
		}
	}
	if(addsense0 || addsense1)
		ct_dbg((C, "as0=%x as1=%x, ", addsense0, addsense1));
	ct_cxdbg(ctinfo->ct_scsiv >= 2 && esdptr->rsscsi2.sksv, 0,
		(C, "c/d=%x bpv=%x bit=%x byte=%x. ", esdptr->rsscsi2.cd,
		esdptr->rsscsi2.bpv, esdptr->rsscsi2.bitp,
		((uint)esdptr->rsscsi2.sensespecific[0]<<8) +
			esdptr->rsscsi2.sensespecific[1]));

	/* get bogus counts for 9 track, and others sometimes
	 * while we are opening, so just check here, rather
	 * than adding check at each drive type. */
	if(ctinfo->ct_state & CT_OPEN)
		ctinfo->ct_recov_err += recovers;
}


/* Called when we get a short count at EOT.  In an ideal
 * world, this would not be an error if some data was read/written,
 * because then there is no way to know that some of the i/o was done. 
 * However, due to the braindead we implemented multivolume tapes, we
 * need to treat a short i/o as an error so multi-vol tapes can be
 * exchanged with older OS releases.  Only QIC and exabyte have this
 * problem; all newer drives do the 'right thing' (except that 9 track is
 * a bit difficult to deal with, because the drive doesn't enforce EOT;
 * tar, bru, cpio, etc. have been fixed to treat ENOSPC, or i/o return 0,
 * and at EW/EOT as multi-vol for IRIX 4.0.
 * Deliberately code this way so all new drives added will
 * work 'right', without code changes.
 * Also used at close to decide whether to write a filemark if we saw EW.
*/
static int
ctresidOK(ctinfo_t *ctinfo)
{
	switch(ctinfo->ct_typ.tp_hinv) {
		case TPQIC24:
		case TPQIC150:
		case TP8MM_8200:
		case TP8MM_8500:
			ct_xdbg(1,(C, "ret ERR on resid. "));
			return 0;
		default:
			ct_xdbg(1,(C, "ret OK on resid. "));
			return 1;
	}
}


/*
 *  This common routine parses the sense key and other status bits
 *  and returns a PASS/FAIL indication.  As a side effect,
 *  it will correctly indicate (and possibly report) the error
 * (if any).  It is called only from ctcmd, and on status == ST_CHECK
*/
static int
ctchkerr(register ctinfo_t *ctinfo)
{
	register u_char	sensekey;
	register buf_t	*bp;
	u_char opcode = ctinfo->ct_lastcmd;
	ct_g0es_data_t	*esdptr;
	u_char cmd0 = ctinfo->ct_g0cdb.g0_cmd0;	/* for SPACE_EOM_CODE */
	int residbytes, resid, reqcnt;

	esdptr = &ctinfo->ct_es_data;

	/* thse are set before ctreqsense() changes them */
	bp = ctinfo->ct_subchan->s_buf;

	/* for determining if fw/bk tape motion; also for ILI error reports.
	 * Only needed for read, write, space cmds; (want sign extension
	 * on high byte, but compiler won't do it for us...) */
	sensekey = (ctinfo->ct_g0cdb.g0_cmd1 & 0x80) ? 0xff : 0;
	reqcnt = (sensekey<<24) | (((int)ctinfo->ct_g0cdb.g0_cmd1) << 16) |
		(((ulong)ctinfo->ct_g0cdb.g0_cmd2) << 8) |
		(ulong)ctinfo->ct_g0cdb.g0_cmd3;

	if(ctreqsense(ctinfo) == -1 || esdptr->esd_errclass != 7) {
		if(esdptr->esd_errclass != 7)
		    ct_dbg((CE_WARN, "bad class (%d) on reqsense.  ",
			esdptr->esd_errclass));
		goto anerror;
	}

	if(esdptr->esd_valid)	/* get resid count if valid */
		resid = MKINT(esdptr->esd_resid);
	else
		resid = 0;
	if(ctinfo->ct_state & CT_GETBLKLEN)
		ctinfo->ct_resid = resid;
	if(resid) {
		if(ISVAR_DEV(bp->b_dev))
			residbytes = resid;
		else	/* curblksz could be 0 if very first read is
			a short read */
			residbytes = resid * (ctinfo->blkinfo.curblksz ?
				ctinfo->blkinfo.curblksz :ctinfo->ct_blksz);
		ct_dbg((C, "resid %d/%d (%d), ", resid, reqcnt, residbytes));
	}
	else
		residbytes = 0;

	/* get drive specific stuff before chking for FM, etc., since
	 * they make get set there. */
	ctdrivesense(ctinfo);

	sensekey = esdptr->esd_sensekey;

	ct_dbg((C, "%s [%s]: ",
		sensekey?scsi_key_msgtab[sensekey]:"no sense", cmdnm(opcode)));

	if(esdptr->esd_eom||(IS_CIPHER(ctinfo) && sensekey == SC_VOL_OVERFL)) {
		/* may be converted from EOM to BOT in some special cases */
		ctinfo->ct_state |= CT_EW;
		ct_dbg((C, "at EW/BOT, "));
	}
	if(esdptr->esd_fm) {
		/* for general use; also needed by several checks below */
		ctinfo->ct_state |= CT_FM;
		ct_dbg((C, "at FM (esd_fm), "));
	}

	/* If the residual is larger than the request, we can't recover data
	 * at this stage of the driver's life.  Punt.  */
	if((opcode == READ_CMD || opcode == WRITE_CMD) &&
		residbytes > (int)bp->b_bcount)
		goto anerror;
	
	/* Now handle the sense key based on the command issued.
	 * The QIC-104 spec gives a priority to each sense key, in case
	 * 2 or more occur at the same time, only the highest priority
	 * gets indicated.  */
	switch(sensekey) {
	/* Illegal Request:
	 *   means we screwed up some values in the command descriptor
	 *   block or associated parameter list.  This is a driver error,
	 *	or some new type of drive.
	 *   Happens with a modesel on a Kennedy drive that
	 *	isn't at BOT, and when trying to set variable mode
	 *	on drives that don't support it.  */	
	case SC_ILLEGALREQ:
		if(IS_TANDBERG(ctinfo) && opcode == WRITE_CMD &&
			esdptr->tand.ercl == 0x10) {
			/* we get this on Tandberg 3660 when we try to write on
				QIC24 tapes */
			ctinfo->ct_state |= CT_QIC24;
			bp->b_error = EINVAL;
		}

		if(!(ctinfo->ct_state & CT_OPEN)) {
			ct_dbg((C, "during open "));
			bp->b_error = ENODEV;	/* most likely tried to open
				a device like Archive in variable mode */
		}
		goto anerror;

	/* Hardware Error:
	 *   Non-recoverable hardware error (parity, etc...). We just
	 *   punt!!!! */
	case SC_HARDW_ERR:
		ctreport(ctinfo, "Hardware error, Non-recoverable\n");
		goto anerror;

	/* Aborted Command:
	 *   Drive aborted the command, the initiator could try
	 *   again, but we don't! */
	case SC_CMD_ABORT:
		ctreport(ctinfo, "Command aborted by target\n");
		goto anerror;

	/* Media Error:
	 *   Associated with the READ, WRITE, WFM and SPACE commands.
	 *   Cannot read, write or space over data (bad data), or
	 *	SPACE command hit EOM.  */
	case SC_MEDIA_ERR:
		ctreport(ctinfo, "Unrecoverable error\n");
		goto anerror;

	/* some vendors report by default (modesel can change); some vendors
	 * report on specific kinds of errors regardless. */
	case SC_ERR_RECOVERED:
		/* count recoverable errors (may be overridden by code above
		 * on later call to this routine) */
		ctinfo->ct_recov_err++;
		/* Kennedy gives this on some occasions, even when switches
		 * set correctly (to not report recovered errors.  If
		 * deferred is set, command hasn't yet been executed,
		 * so simply re-issue it from ctcmd. */
		if(esdptr->esd_defer) {
			ct_dbg((C, "re-issue cmd; defer set. "));
			return sensekey;
		}
		break;

	/* Data Protect:
	 *   A command was issued, usually write, and the tape was
	 *   write protected.  This is a driver error, because during
	 *   the open (for writing) we confirm that write protect is off.
	 *   For now, on viper 2150 this happens when we try to write a
	 *   low density (310 oersted) QIC24 tape;  set bit indicating
	 *   it for the GET ioctl; See comments at end of ctmodesense()
	 *   To ensure the writeprotect didn't get missed we do
	 *   another modesense here to be sure.  Sort of kludgy, but it
	 *   happens rarely, and only on fatalerrors, so not too bad...  */
	case SC_DATA_PROT:
		if(IS_VIPER60(ctinfo) || IS_VIPER150(ctinfo) ||
		    IS_TANDBERG(ctinfo)) {
			if(!(ctinfo->ct_state & (CT_QIC24|CT_WRP))) {
			    buf_t sbp;
			    /* we don't know which; go find out, have to
			     * preserve bp contents first */
			    bcopy(bp, &sbp, sizeof(sbp));
			    ctcmd(ctinfo, ctmodesens, MAX_MODE_SEL_DATA,
					    SHORT_TIMEO, 0);
			    if(!(ctinfo->ct_bp->b_error) &&
				    !(ctinfo->ct_state & CT_WRP)) {
				    ctinfo->ct_state |= CT_QIC24;
			    }
			    bcopy(&sbp, bp, sizeof(sbp));
			}
			if(!(ctinfo->ct_state & CT_WRP))
			    ctinfo->ct_state |= CT_QIC24;
			bp->b_error = ctinfo->ct_state&CT_QIC24 ? EINVAL :
			    EROFS;
		}
		else  {
			ct_dbg((C, "WRP, but not WP in modesense.  "));
			bp->b_error = EROFS;
		}
		break;

	/* Indicates that the tape cannot be accessed, usually
	 *  offline or no tape in drive.  ctsetup() treats this
	 *  error specially on a tstunitready.  audio mode is
	 *  NOT cleared, audio media is. */
	case SC_NOT_READY:
		ctinfo->ct_state &= ~CHGSTATES|CT_AUDIO;
		bp->b_error = EAGAIN;
		break;

	/* Unit Attention:
	 *   Indicates that the cartridge has been changed or the
	 *   drive reset. ctcmd will normally retry the cmd.
	 *	Always indicate a media change, regardless of cmd;
	 *	if there has been a bus reset, many drives will forget
	 *	their parameters, require re-load, etc.
	 *  audio mode is NOT cleared, audio media is. */
	case SC_UNIT_ATTN:
		ctinfo->ct_state &= ~CHGSTATES|CT_AUDIO;
		ctinfo->ct_state |= CT_CHG;
		if(opcode == SPACE_CMD || opcode == WRITE_CMD ||
			opcode == READ_CMD || opcode == WFM_CMD) {
			/* do NOT retry these; since we don't know
			 * for sure if we are on same tape or at same
			 * place */
			bp->b_error = EIO;
			goto anerror;
		}
		return SC_UNIT_ATTN;

	/* Blank Check:
	 *   Associated with READ and SPACE commands.
	 *   could be either No data or
	 *   Logical Endof Media (or BOT if going backwards).
	 *   Also get this on seek block if block would be past EOD. */
	case SC_BLANKCHK:
		if(!(ctinfo->ct_state & CT_FM)) {	/* not at a FM */
			ct_dbg((C, reqcnt<0 ? "BOT.  ":"EODATA\n"));
			if(reqcnt<0) {	/* space back, at BOT */
				ctinfo->ct_state &= ~BACKSTATES;
				ctinfo->ct_state |= CT_BOT;
			}
			else /* spaced forwards */
				ctinfo->ct_state |= CT_EOD;
		}
		if(opcode == SPACE_CMD && cmd0 == SPACE_EOM_CODE &&
			(ctinfo->ct_state & CT_EOD))
			break;	/* being at EOD on a space to eom is OK! */
		else if(residbytes) {
			if(opcode == READ_CMD && residbytes <= bp->b_bcount &&
				ctresidOK(ctinfo))
				/* note that b_resid could be > b_count, due
				 * to buffering */
				bp->b_resid = residbytes;
			else {
				bp->b_error = ENOSPC;
				goto anerror;
			}
		}
		/* else not an error if no resid, error will occur on next op */
		break;
	/* Volume Overflow:
	 *   Associated with WRITE and possibly? WFM commands */
	case SC_VOL_OVERFL:
		if(esdptr->esd_eom || IS_CIPHER(ctinfo)) { /* check for EOM */
			/* EOM: physical or logical EOM or BOT if backwards.
			 * Since we don't support read/write reverse, we don't
			 * need to worry about the BOT case here, but code it
			 * right just in case.  */
			ct_dbg((C, reqcnt<0 ? "BOT.  ":"EW\n"));
			if(reqcnt<0) {	/* back motion, at BOT */
				ctinfo->ct_state &= ~BACKSTATES;
				ctinfo->ct_state |= CT_BOT;
			}
			else switch(ctinfo->ct_typ.tp_hinv) {
			    case TPQIC24:
			    case TPQIC150:
			    case TP8MM_8200:
		     	    case TP8MM_8500:
				/* really does mean phys EOM for these.  For
					* 9track and DAT it means logical eot.
					* CT_EW was set above. */
				ctinfo->ct_state |= CT_EOM;
			}
			if(residbytes) {
				/* note that b_resid could be > b_count,
				 * due to buffering */
				if(opcode == WRITE_CMD &&
					residbytes <= bp->b_bcount &&
					ctresidOK(ctinfo))
					bp->b_resid = residbytes;
				else {
					bp->b_error = ENOSPC;
					goto anerror;
				}
			}
			else if(esdptr->esd_defer) {
				/* this happens on deferred error reporting on
				 * writes, when the data was all written, but
				 * EOM was encountered.  So far only seen on
				 * Kennedy */
				bp->b_error = ENOSPC;
				goto anerror;
			}
			/* else resid==0 means i/o ok, get error on next i/o */
		}
		else
			ct_dbg((C, " EOM not set!.  "));
		break;

	/* No Sense:
	 *   Associated with READ, WRITE, WFM and SPACE commands.
	 *   Check condition occured because of the state of
	 *   the FM or EOM or ILI bits or no sense is available.
	 *   Also because of deferred error reporting on Kennedy.
	 *   if because eom, it's logical eom (CT_EW) */
	case SC_NOSENSE:
		if(esdptr->esd_eom) {
			ct_dbg((C, reqcnt<0 ? "BOT.  ":"EOM\n"));
			/* EOM means logical EOM; or BOT if back.  */
			if(reqcnt<0) {	/* back motion, at BOT */
				ctinfo->ct_state &= ~BACKSTATES;
				ctinfo->ct_state |= CT_BOT;
			}
			else if(opcode == SPACE_CMD && cmd0 == SPACE_EOM_CODE &&
				(ctinfo->ct_state & CT_EOD)) {
				break;	/* at EOD on a space to eom is OK! */
			}
			if(residbytes) {
				/* not an error! otherwise program has no
				 * way to know that some of the i/o was done.
				 * NOTE: resid returned may be greater than
				 * most recent request when buffered mode
				 * is enabled!  For compat with 3.1 releases
				 * this is treated as an error for all except
				 * Kennedy (for multi-vol)
				 */
				/* note that b_resid could be > b_count,
				 * due to buffering */
				if((opcode == WRITE_CMD || opcode == READ_CMD)
					&& residbytes <= bp->b_bcount &&
					ctresidOK(ctinfo))
					bp->b_resid = residbytes;
				else {
					bp->b_error = ENOSPC;
					goto anerror;
				}
			}
			else if(esdptr->esd_defer) {
				/* this happens on deferred error reporting on
				 * writes, when the data for the PREVIOUS write
				 * was all written, but EOM was encountered.
				 * So far only seen on Kennedy.  The current
				 * command wasn't even done in this case. */
				bp->b_error = ENOSPC;
				goto anerror;
			}
			/* else resid==0 means i/o ok, get error on next i/o */
		}
		/* If FM is set, we just mark that we hit a FM and update the
		 * residual count (if any) and higher level stuff handle it.  */
		else if(esdptr->esd_fm)
			bp->b_resid = residbytes;
		else if(esdptr->esd_ili) {
			ct_dbg((C, "ILI.  "));
			if(esdptr->esd_valid) {
				/* resid < 0 indicates read request less than
				 * size of block on tape.  The next read starts
				 * at the beginning of the next block, so data
				 * will be skipped over.  We return a unique
				 * error so programs that care can tell:
				 * "Not enough space" is the perror() msg.
				 *
				 * resid > 0 will return a short count, which is
				 * how programs like tar traditionally get the
				 * block size of a tape */
				if(residbytes < 0) {
					bp->b_error = ENOMEM;
					ct_shortili(ctinfo, reqcnt, residbytes);
					goto anerror;
				}
				else {
					bp->b_resid = residbytes;
				}
			}
			else
				ct_badili(ctinfo, reqcnt);
		}
		else {
			/* If the FM or EOM or ILI bits are not set, we got a
			 * NO SENSE code and we don't know why; ignore it */
			ctreport(ctinfo, "Check Condition with unknown cause\n");
		}
		break;

	case SC_VENDUNIQ:
		/*for Exabyte, this can happen under some circumstances
		 * during a space; if xfr bit is set it's not recoverable,
		 * if tmd is set we could recover;  we don't bother since
		 * this is so unusual, it would be hard to test the
		 * recover code. */
		ct_dbg((C, "Vendor unique error on %s.  ", cmdnm(opcode)));
		goto anerror;
		
	default:
		ctreport(ctinfo, "Unexpected sense key %s (%x)\n",
			scsi_key_msgtab[sensekey], sensekey);
		goto anerror;
	}	/* end switch on sense key	*/

	ct_cxdbg(bp->b_error, 0, (C, "ctchkerr: b_err=%x ", bp->b_error));
	return bp->b_error ? -1 : 0;
anerror:
	bp->b_flags |= B_ERROR;
	if(!bp->b_error)
		bp->b_error = EIO;
	ct_cxdbg(bp->b_error, 0, (C, "anerror: b_err=%x ", bp->b_error));
	return -1;
}

/*
 *   Remap the old style minor bits to the new style.
 *   Preserves the major # so the scsi_ctlr stuff will work.
 *   Don't use major() macro, because we want the upper byte to be
 *   exactly preserved; don't want to remap thru the MAJOR[] array.
*/
static dev_t
ctminor_remap(register dev_t dev)
{
	register dev_t	rv;

	rv = dev & (~0xff);
	rv |= scsi_unit(dev);	/* SCSI ID	*/

	if(dev & 1)
		rv |= REWIND_MSK;

	if(!(dev & 2)) /* swap/noswap bit sense is reversed */
		rv |= SWAP_MSK;

	if(dev & 4)
		rv |= VAR_MSK;
	return rv;
}


#ifndef PROM
/* Try to read 4 bytes in variable mode.  When done, we
 * subtract the residual which is saved as a special case
 * in ctchkerr(). from 4 to get the actual blocksize.
 * Shouldn't be called if we have already started writing.
 * Return -1 on any errors.  Saves and restores all ct_state
 * in case they are going to be writing, etc.  Only exception is
 * if we don't think we got back to where we started.
*/
static int
ctgetblklen(register ctinfo_t *ctinfo, dev_t odev)
{
	register buf_t	*bp;
	ulong		save_blksz;
	ulong		save_state;
	ushort		fm_or_bot;
	int		blocksize = 0;
	int isvar;
	dev_t save_dev;
	ulong save_min;

	/* Fail if we've written, or if at EOM, or not opened for reading,
	 * or if variable mode not supported */
	if((ctinfo->ct_state & (CT_WRITE|CT_EOM|CT_EW|CT_EOD)) ||
		!(ctinfo->ct_openflags & FREAD) ||
		!(ctinfo->ct_typ.tp_capablity&MTCAN_VAR))
		return -1;

	if(!ctloaded(ctinfo) || !(ctinfo->ct_state & CT_ONL))
		return -1; /* load failure, or offline */

	bp = ctinfo->ct_bp;
	isvar = ISVAR_DEV(bp->b_dev);

	/* Some drives, like Kennedy, can only switch to var mode
	 * at BOT */
	if(isvar || (ctinfo->ct_typ.tp_capablity&MTCAN_CHTYPEANY))
		fm_or_bot = CT_BOT|CT_FM;
	else
		fm_or_bot = CT_BOT;

	/* check to be sure loaded first, then must be at FM or BOT */
	if(!(ctinfo->ct_state & fm_or_bot)) {
		ct_dbg((C, "no len because not at BOT/FM.  "));
		return -1;
	}

	save_blksz = ctinfo->ct_blksz;

	/* everything but position related info */
	save_state = ctinfo->ct_state & ~CTPOS;
	fm_or_bot = ctinfo->ct_state & (CT_BOT|CT_FM);

	ctinfo->ct_state |= CT_GETBLKLEN;
	ctinfo->ct_blksz = 1;

	/* be sure in variable mode, and sili bits */
	ctcmd(ctinfo, ctmodesel, 0, SHORT_TIMEO, 0);
	if(bp->b_error) {
		ctinfo->ct_state &= ~CT_GETBLKLEN;
		goto restore;
	}

	save_dev = bp->b_dev;
	save_min = ctinfo->ct_cur_minsz;
	bp->b_dmaaddr = (caddr_t)ctinfo->ct_blklenbuf;
	bp->b_bcount = sizeof(ctinfo->ct_blklenbuf);
	bp->b_flags = B_BUSY | B_READ;

	ctinfo->ct_cur_minsz = bp->b_bcount;
	bp->b_dev = odev | 1<<2;	/* force to variable for strat, etc;
		strat now remaps, so feed it the original */

	ctstrategy(bp);

	bp->b_dev = save_dev;
	ctinfo->ct_cur_minsz = save_min;
	ctinfo->ct_state &= ~CT_GETBLKLEN;

	if(bp->b_error && (!ctinfo->ct_es_data.esd_ili ||
		ctinfo->ct_es_data.esd_sensekey != SC_NOSENSE)) {
		ct_dbg((C, "strat error, other than ili/no sense "
			"(sensekey %d)  ", ctinfo->ct_es_data.esd_sensekey)); 
		blocksize = -1;
		goto getblklen_err;
	}

	blocksize = sizeof(ctinfo->ct_blklenbuf) - ctinfo->ct_resid;
	ct_xdbg(1, (C, "block size is %d (%x).  ", blocksize, blocksize));

	if(blocksize <= 0)	/* paranoia check */
		blocksize = -1;
	else {
		if(!isvar) {
			/* if in fixed mode, set the blksize for further i/o. 
			 * Note that this may be reset if writing */
			ct_dbg((C, "fixed blksiz set to %d.  ", blocksize));
			save_blksz = blocksize;
			/* reset recommended size to ensure it's a multiple */
			ctinfo->blkinfo.recblksz /= blocksize;
			ctinfo->blkinfo.recblksz *= blocksize;
		}
		else
			ctinfo->blkinfo.recblksz = blocksize;
		ctinfo->blkinfo.curblksz = blocksize;
	}

getblklen_err:	/* try to reposition to same place */
	if(fm_or_bot & CT_BOT)
		ctcmd(ctinfo, ctrewind, 0, REWIND_TIMEO, 0);
	else {
		/* this is the buf struct checked by ctspace, not the one
		 * that bp points to!  We space back to prev fm, then
		 * forward over it.  This leaves FM correctly set, 
		 * DIDIO cleared, and us on the 'correct' side of the FM.
		 * If we just spaced back, and then cleared DIDIO, the code
		 * in MTFSF wouldn't work correctly, if someone happened
		 * to do an MTFSF after the getblklen.
		 * BSR would be better, but I don't want to test to be
		 * sure all the drives do it right and set the correct
		 * status flags in the sense routines. */
		ctinfo->ct_subchan->s_buf->b_bcount = (unsigned)-1;
		ctcmd(ctinfo, ctspace, SPACE_FM_CODE, SPACE_TIMEO, 0);
		ctinfo->ct_subchan->s_buf->b_bcount = 1;
		ctcmd(ctinfo, ctspace, SPACE_FM_CODE, SPACE_TIMEO, 0);
	}

restore:	/* restore changed fields */
	ctinfo->ct_blksz = save_blksz;

	/* be sure in correct mode and sili bits */
	ctcmd(ctinfo, ctmodesel, 0, SHORT_TIMEO, 0);
	ctinfo->ct_state &= ~CT_READ;	/* forget read, but keep
		position/offline, etc */
	ctinfo->ct_state |= save_state;
	return bp->b_error ? -1 : blocksize;
}

/* fake a space to EOM.  Some drives, like the Exabyte, do not
 * support a direct space to EOM.  For Exabyte, this doesn't
 * work correctly if already at the FM at EOD as would be
 * used by MTAFILE, instead, you get TMD errors.
 */
#endif /* PROM */


/* check to see if we the tape is loaded and online.
 * If not, load it now.  We do this in ctstrategy, and in ctioctl
 * for cmds that(might) move the tape, or 'assume' the
 * tape is already loaded.  This used to be done in
 * open, but drives like the Exabyte and the Kennedy
 * take a long time to load and unload, so it is done
 * only when first needed.
 * May be marked offline before this, and online after.
*/
static int
ctloaded(register ctinfo_t *ctinfo)
{
	if((ctinfo->ct_state & (CT_MOTION|CT_ONL)) != (CT_MOTION|CT_ONL)) {
		ct_dbg((C, "loading tape st%x.  ", ctinfo->ct_state));
		ctcmd(ctinfo, ctload, L_UL_LOAD, REWIND_TIMEO, 0);
		if(ctinfo->ct_bp->b_error) {
			ct_dbg((C, "tape load failed.  "));
			return 0;
		}
		else {
			ct_dbg((C, "tape load OK.  "));
			/* do modesel after load to set dens, etc.  If tape
			 * wasn't loaded, the modesel in open fails for some
			 * drives, such as Kennedy and Cipher 540.
			 * Always do a sense first, so things like audio vs
			 * non-audio don't * get reset by a blind modeselect! */
			ctcmd(ctinfo, ctmodesens, MAX_MODE_SEL_DATA,
			    SHORT_TIMEO, 0);
			ctcmd(ctinfo, ctmodesel, 0, SHORT_TIMEO, 0);
			if(ctinfo->ct_bp->b_error) {
				/* shouldn't ever happen if load was OK! */
				ct_dbg((C, "mode sel failed.  "));
				return 0;
			}
		}
		if(ctinfo->ct_typ.tp_capablity&MTCAN_PART) {
			/* check to see if it is partitioned whenever we load */
			ctcmd(ctinfo, ctmodesens, MSD_IND_SIZE+0xa,
			    SHORT_TIMEO, 0x11);
			/* in case of failures */
			ctinfo->ct_state &= ~CT_MULTPART;
			if(!ctinfo->ct_bp->b_error && ctinfo->ct_ms.msd_vend[3])
				ctinfo->ct_state |= CT_MULTPART;
		}
		/* online, loaded, AND modesel done.  set CT_ONL and BOT
		 * here because with kennedy, we sometimes get a unit atn
		 * on the modeselect, if the drive was offline, since
		 * the load changed it from offline to online... */
		ctinfo->ct_state |= CT_MOTION|CT_ONL|CT_BOT;
	}
	return 1;
}


/* We got an ILI on a request sense, but the valid bit
 * wasn't set.  This shouldn't happen!  It does happen
 * with tape block sizes > 160K on the Exabyte,
 * (1/89), and it might happen with others also.
 * Don't report if we were trying to determine
 * the block length for an ioctl or it wasn't a read.
 * Separate routine because indent level was ridculous.
*/
static void
ct_badili(register ctinfo_t *ctinfo, int reqcnt)
{
	ct_dbg((C, "ILI, but resid not valid!.  "));
	if(ctinfo->ct_lastcmd!=READ_CMD ||
		(ctinfo->ct_state & CT_GETBLKLEN))
		return;
	ctreport(ctinfo, "blocksize on tape larger than request of %d bytes\n\
But actual blocksize couldn't be determined\n",
		reqcnt *= ctinfo->ct_blksz);
}


/* A read was done with a block size smaller than the current
 * block size on the tape.
 * Don't report if we were trying to determine
 * the block length for an ioctl or it wasn't a read.
 * Separate routine because indent level was ridculous.
 * There is some argument about whether the user should be told.
*/
static void
ct_shortili(register ctinfo_t *ctinfo, int reqcnt,  int resid)
{
	ct_dbg((C, "req blk size too small.  "));
	if(ctinfo->ct_lastcmd!=READ_CMD ||
		(ctinfo->ct_state & CT_GETBLKLEN))
		return;
	ctreport(ctinfo, "blocksize mismatch; blocksize on tape is %d bytes\n",
		(reqcnt - resid) * ctinfo->ct_blksz);
}



/* Here so all errors reported in a standard way. Can't use CE_NOTE because
 * you get extra newlines that way. Can't use protoype because of varargs,
 * and we don't have a vprintf in the kernel.
 */
static void
ctreport(ctinfo_t *ctinfo, char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	cmn_err(CE_CONT, "NOTICE: SCSI tape #%d,%d ",
		scsi_ctlr(ctinfo->ct_bp->b_dev),
		SCSIID_DEV(ctinfo->ct_bp->b_dev));
	cmn_err(CE_CONT, fmt, ap);
	va_end(ap);
}


static int _fastick, fastclock;
#define fastick _fastick

#ifdef SN0
#define PREEMPT_MULT 80 /* ??? */
#endif
#ifdef EVEREST
#define PREEMPT_MULT 80
#endif
#if IP20 || IP22 || IP26 || IP28 || IP30 || IP32
#define PREEMPT_MULT 80
#endif

/* swap bytes in place.
 * Unroll a bit since we are typically doing many blocks.
 * if count is odd, last byte is left untouched, unlike
 * dd conv=swab.
 * It turns out that the byte swapping in memory is faster
 * for non-IP5 than executing the extra instructions
 * to load a register, shift and mask, and then write it back
 * (particularly true on IP20, with partial cache stores on).
 * HOWEVER, on machines like the IP7 the register method
 * is about 20% faster, presumably because of their cache
 * system.  Not much point in unrolling the loop for the register
 * method; the loop overhead is relatively small.
 * We check for preemption aprox each fastick usecs if fastclock
 * is on, otherwise each sched clock tick.  We may check first time
 * through after fewer iterations, but thats OK; better than the
 * overhead of maintaining 2 counters.
*/

static void
swapbytes(u_char *ptr, unsigned n)
{
	uint cnt, preempt;


#define SW_LOOPUNROLL 16
	unchar tmp;
	
	if(fastclock)
		preempt = (PREEMPT_MULT * fastick) / SW_LOOPUNROLL;
	else
		preempt = (PREEMPT_MULT * 1000 * 1000) / (SW_LOOPUNROLL * HZ);
	cnt = n / SW_LOOPUNROLL;
	n -= cnt * SW_LOOPUNROLL;	/* for resid, if any */
	while(cnt) {
		tmp = ptr[0]; ptr[0] = ptr[1]; ptr[1] = tmp;
		tmp = ptr[2]; ptr[2] = ptr[3]; ptr[3] = tmp;
		tmp = ptr[4]; ptr[4] = ptr[5]; ptr[5] = tmp;
		tmp = ptr[6]; ptr[6] = ptr[7]; ptr[7] = tmp;
		tmp = ptr[8]; ptr[8] = ptr[9]; ptr[9] = tmp;
		tmp = ptr[10]; ptr[10] = ptr[11]; ptr[11] = tmp;
		tmp = ptr[12]; ptr[12] = ptr[13]; ptr[13] = tmp;
		tmp = ptr[14]; ptr[14] = ptr[15]; ptr[15] = tmp;
		ptr += SW_LOOPUNROLL;
		cnt--;
		if(cnt % preempt == 0)
			preemptchk();
	}

	while(n>1) {	/* needed for both cases */
		unchar etmp = ptr[0]; ptr[0] = ptr[1]; ptr[1] = etmp;
		ptr += 2;
		n -= 2;
	}
}
