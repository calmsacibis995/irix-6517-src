#if !defined(EVEREST) && !defined(IP30) && !defined(SN0)
#ident "io/scsi.c: $Revision: 3.143 $"

/**************************************************************************
 *									  *
 * 		 Copyright (C) 1986,1988, Silicon Graphics, Inc.	  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#include <sys/param.h>
#include <sys/cpu.h>
#include <sys/sbd.h>
#include <sys/buf.h>
#include <sys/debug.h>
#include <sys/kopt.h>
#include <sys/cmn_err.h>
#include <sys/dmamap.h>
#include <sys/scsi.h>
#include <sys/scsidev.h>
#include <sys/invent.h>

#include <arcs/hinv.h>
#include <arcs/cfgdata.h>
#include <libsk.h>
#include <libsc.h>

#define splockspl(x,y) 0 
#define spunlockspl(x,y)
#define splock(x) 0
#define spunlock(x,y)
#define initnlock(x,y)
#define spsema(x)
#define svsema(x)
#define svpsema(x,y,z)

extern int cachewrback;

#define DMAMAP_FAIL	((dmamap_t *)-1L)

/*
 * SCSI command interface.  These are the externally callable routines
 * for use by the various scsi drivers.
 *
 * scsisubchan_t *allocsubchannel(u_char adap, u_char target, u_char lu)
 * void freesubchannel(scsisubchan_t *)
 * void doscsi(scsisubchan_t *)
 * u_char *scsi_inq(u_char adap, u_char targ, u_char lun)
 * void scsi_setsyncmode(scsisubchan_t *, enable)
 * scsidump(int adap)
 * scsi_sendabort(scsisubchan_t *);	(deliberately not documented)
 *
 * The driver will allocate a subchannel with allocsubchannel().
 * To perform a SCSI request, it will fills in the command, length, buffer,
 * notify routine, and timeout value for the request.  If the command
 * is to simply return on completion, s_notify must be NULL; otherwise
 * the notify routine will be called when the command completes.
 * When the request is completed, s_error will be zero for a successful
 * SCSI interaction.  The SCSI status byte is stored in s_status.
 * NOTE: notify routines may start new scsi commands, but they MUST
 * be commands with notify routines, or we could sleep at interrupt
 * time, since it is entirely possible that another command has
 * started before the notify routine is called.  There is also the
 * case of a non-notify command not being able to be executed because
 * the WD chip has another interrupt pending.
 *
 * Philosophy of the module:
 *
 * Each possible logicial unit number on a SCSI device is associated with
 * a subchannel.  Only one process can be using a subchannel at any one
 * time, since the subchannel represents a SCSI "thread" between the
 * host and a logical unit.  IT IS THE CALLING DRIVER'S RESPONSIBLITY
 * TO ENSURE THIS.  A "channel" is associated with a SCSI
 * controller on the bus, and is often referred to as a "target" or a
 * "formatter". A process controls the channel as long as it's "thread"
 * is the one using the channel at the moment.  Each possible possible
 * SCSI bus that may be controlled by the host is called a "host adapter",
 * and (probably) is associated with a unique piece of hardware.
 *
 * The formatter driver is responsible for subchannel and channel management,
 * i.e. that requests for the same subchannel are single threaded.
 * This module only manages use of the host adapter.
 * 
 * When the formatter driver makes a SCSI request, the host adapter must
 * be acquired.  When this has been accomplished, a request to the logical
 * unit may be made and the transfer begun.  At some point during the
 * transfer, the formatter may request to disconnect from the bus while
 * it performs the actual I/O.  At this point, the host is prompted by
 * interrupt to save the current state of the transaction, and the host
 * adapter is free to handle other requests.  When the formatter is ready
 * to acquire the bus again, it prompts the host to reselect the logical unit.
 * The transfer will be allowed to continue, eventually to completion.
 * an interrupt from the chip.
 *
 * NOTE: on the 93A, the single byte transfer bit should never be
 * set if the syncreg has a non-zero value (see tech brief
 * TB-JL0002 from WD)
 */

union scsicfg {
	struct scsidisk_data disk;
	struct floppy_data fd;
};


#define INTR_ERR 0x1000

extern uint scsi_syncenable[];	/* per adapter bitmap of target ID's
	for which sync can be negotiated (from master.d/scsi) */
extern u_char scsi_syncperiod[];	/* per adapter bitmap of sync period */
extern int scsiburstdma;	/* set nonzero in IP*.c if CPU and/or IO
	board supports the burst mode DMA of the 93A */
extern scsidev_t *scsidev;
extern int scsicnt;	/* set to 1 in master.d/scsi, and changed in IP*.c for
	machines that * support more than one */
extern scuzzy_t scuzzy[];

/* mostly for post-mortem analysis.  Was DEBUG only, but useful on
	non-debug crash files */
static uint scaborts, scresets, sctimeouts;

int sc_inq_stat;	/* status from scsi_inq() */
/* default scsi host adapter id is 0, array is in bss */
u_char scsi_ha_id[SC_MAXADAP];	/* allows users in stand
	stuff to check if a particulary id is the host adapter, rather than
	embedding the id in every user of the driver. For now, host adapter
	id is always 0. */

/*	these are used with the new sash,etc. , that may already have one
	of the targets in sync mode.  If we don't deal with it
	correctly, then we will get a bus timeout when we first
	attempt i/o.  After that, they are unused.
*/
static u_char scsisyncreg[SC_MAXADAP], scsisyncdest[SC_MAXADAP];
static int sc_wasreset[SC_MAXADAP];

/*	no_nextcmd serves as a flag to identify when the scsirelease
	routine should NOT start any waiting commands.  It is set when
	we have been put into dump mode by a device driver
	(Dump mode is for panic() dumping), or we are shutting down.
	In standalone, we don't have intr's, so it is always set, but
	uses a different value than dumping, so callback routines
	still get called. */
#define idbg_scsi(n)
static int no_nextcmd = 2;	/* never any interrupts in standalone */
static int scsi_rc(u_char, u_char, u_char, struct scsidisk_data *);

/* routines called from outside the driver (other than those 
	declared in scsi.h) */
int scsiunit_init(int);	/* called from IP?.c before main() */
void scsi_nosync(int);	/* turn off all sync devices before reboot */

/* local on non-Everest, extern on Everest */
#ifdef EVEREST
int scsi_int_present(scsidev_t *);
#else
static int scsi_int_present(scsidev_t *);
#endif

/* local routines */
static u_char transval(u_char);
static int scsi_present(scsidev_t *);
static int do_trinfo(scsidev_t *, u_char *, long, uint usecs);
static int sc_busycnt(scsidev_t *);
static int do_select(scsisubchan_t *, scsidev_t *);
static int scsiacquire(scsidev_t *, scsisubchan_t *, int);
static int unex_info(scsidev_t *, u_char, u_char);
static int scsi_dma(scsidev_t *, scsisubchan_t *sp, int);
static uint wait_scintr(scsidev_t *, unsigned);
static int do_inquiry(scsidev_t *);
static caddr_t scsi_ecmd(u_char, u_char, u_char, u_char *, int, void *, int);
static void sync_setup(scsidev_t *, scsisubchan_t *, int, int);
static void scsi_timeout(scsisubchan_t *);
static void wd93reset( scsidev_t *, int);
static void scsirelease(scsidev_t *, int);
static void handle_intr(scsidev_t *);
static void handle_reset(scsidev_t *);
static void scsisubrel(scsidev_t *, scsisubchan_t *, int);
static void save_datap(scsidev_t *, scsisubchan_t *, int);
static void startscsi(scsidev_t *, scsisubchan_t *);
static void restartcmd(scsidev_t *, scsisubchan_t *);
static void sc_continue(scsidev_t *, scsisubchan_t *, u_char);
static void setdest(scsidev_t *, scsisubchan_t *);
	/* we don't support stdargs.h in the kernel, so can't use ...  */
static void scsi_cmdabort(scsidev_t *, scsisubchan_t *, char *, __psint_t,
			  __psint_t);
static void sccmd_lci(scsidev_t *, unchar);
static void sc_waitonly(scsidev_t *dp);

extern int timeout(void (*)(), void *, int);

#define putreg93(r,v)	{ *dp->d_addr=(r); *dp->d_data=(v); }
#define getreg93(r)	( *dp->d_addr=(r), *dp->d_data )
#define	getauxstat()	(*dp->d_addr)
#define putcmd93(v)	putreg93(WD93CMD, (v))
#define	putdata(v)	putreg93(WD93DATA, (v))
#define putcnt(h, m, l) { putreg93(WD93CNTHI, (h))  *dp->d_data=(m);  \
	 *dp->d_data=(l); } /* use the auto address incr feature */
#define getcnt(h,m,l) { h = getreg93(WD93CNTHI); m= *dp->d_data;  \
	l= *dp->d_data; } /* use the auto address incr feature */
#define	getstat()	getreg93(WD93STAT)
#define	getphase()	getreg93(WD93PHASE)
#define	getdata()	getreg93(WD93DATA)
/* note that wd93busy() shouldn't be used if there is a possiblity
	of scsi dma in progress (at least if you care about the data),
	because it causes problems on the IP4 */
#define wd93busy()  (getauxstat() & AUX_CIP)

#define ifintclear(x)  if(scsi_int_present(dp)) { \
		state=getstat(); x;}

/*	NOTE: these tables are used by the smfd, tpsc, and dksc drivers,
	and will probably be used by other SCSI drivers in the future.
	That is why they are now in scsi.c, even though not referenced here.
*/
#ifndef IP22_WD95 /* Defined in wd95.c */
char *scsi_key_msgtab[SC_NUMSENSE] = {
	"No sense",					/* 0x0 */
	"Recovered Error",			/* 0x1 */
	"Device not ready",			/* 0x2 */
	"Media error",				/* 0x3 */
	"Device hardware error",			/* 0x4 */
	"Illegal request",			/* 0x5 */
	"Unit Attention",			/* 0x6 */
	"Data protect error",			/* 0x7 */
	"Unexpected blank media",			/* 0x8 */
	"Vendor Unique error",			/* 0x9 */
	"Copy Aborted",			/* 0xa */
	"Aborted command",			/* 0xb */
	"Search data successful",			/* 0xc */
	"Volume overflow",			/* 0xd */
	"Reserved (0xE)",					/* 0xe */
	"Reserved (0xF)",					/* 0xf */
};
#endif /* !IP22_WD95 */


/*	a number of previously unspec'ed errors were spec'ed in SCSI 2;
	these were added from App I, SCSI-2, Rev 7.  The additional
	sense qualifier adds more info on some of these... */
static char emptymsg[1];

#ifndef IP22_WD95 /* Defined in wd95.c */
char *scsi_addit_msgtab[SC_NUMADDSENSE]
#ifndef PROM	/* save some prom space by making all NULL and BSS */
	= {
	NULL,	/* 0x00 */
	"No index/sector signal",	/* 0x01 */
	"No seek complete",	/* 0x02 */
	"Write fault",	/* 0x03 */
	"Unit not ready",	/* 0x04 */
	"Unit does not respond to selection",	/* 0x05 */
	"No reference position",	/* 0x06 */
	"Multiple drives selected",	/* 0x07 */
	"LUN communication error",	/* 0x08 */
	"Track error",	/* 0x09 */
	"Error log overflow",	/* 0x0a */
	NULL,	/* 0x0b */
	"Write error",	/* 0x0c */
	NULL,	/* 0x0d */
	NULL,	/* 0x0e */
	NULL,	/* 0x0f */
	"ID CRC or ECC error",	/* 0x10  */
	"Unrecovered data block read error",    /* 0x11 */
	"No addr mark found in ID field",       /* 0x12 */
	"No addr mark found in Data field",	/* 0x13 */
	"No record found",	/* 0x14 */
	"Seek position error",	/* 0x15 */
	"Data sync mark error",	/* 0x16 */
	"Read data recovered with retries",	/* 0x17 */
	"Read data recovered with ECC",	/* 0x18 */
	"Defect list error",	/* 0x19 */
	"Parameter overrun",	/* 0x1a */
	"Synchronous transfer error",	/* 0x1b */
	"Defect list not found",	/* 0x1c */
	"Compare error",	/* 0x1d */
	"Recovered ID with ECC",	/* 0x1e */
	NULL,	/* 0x1f */
	"Invalid command code",	/* 0x20 */
	"Illegal logical block address",	/* 0x21 */
	"Illegal function",	/* 0x22 */
	NULL,	/* 0x23 */
	"Illegal field in CDB",	/* 0x24 */
	"Invalid LUN",	/* 0x25 */
	"Invalid field in parameter list",	/* 0x26 */
	"Media write protected",	/* 0x27 */
	"Media change",	/* 0x28 */
	"Device reset",	/* 0x29  */
	"Log parameters changed",	/* 0x2a */
	"Copy requires disconnect",	/* 0x2b */
	"Command sequence error",	/* 0x2c  */
	"Update in place error",	/* 0x2d */
	NULL,	/* 0x2e */
	"Tagged commands cleared",	/* 0x2f */
	"Incompatible media",	/* 0x30 */
	"Media format corrupted",	/* 0x31 */
	"No defect spare location available",	/* 0x32 */
	"Media length error",	/* 0x33 (spec'ed as tape only) */
	NULL,	/* 0x34 */
	NULL,	/* 0x35 */
	"Toner/ink error",	/* 0x36 */
	"Parameter rounded",	/* 0x37 */
	NULL,	/* 0x38 */
	"Saved parameters not supported",	/* 0x39 */
	"Medium not present",	/* 0x3a */
	"Forms error",	/* 0x3b */
	NULL,	/* 0x3c */
	"Invalid ID msg",	/* 0x3d */
	"Self config in progress",	/* 0x3e */
	"Device config has changed",	/* 0x3f */
	"RAM failure",	/* 0x40 */
	"Data path diagnostic failure",	/* 0x41 */
	"Power on diagnostic failure",	/* 0x42 */
	"Message reject error",	/* 0x43 */
	"Internal controller error",	/* 0x44 */
	"Select/Reselect failed",	/* 0x45 */
	"Soft reset failure",	/* 0x46 */
	"SCSI interface parity error",	/* 0x47 */
	"Initiator detected error",	/* 0x48 */
	"Inappropriate/Illegal message",	/* 0x49 */
#if 0 /* other standalone only uses 0x4A for SC_NUMADDSENSE */
	"Command phase error", /* 0x4a */
	"Data phase error", /* 0x4b */
	"Failed self configuration", /* 0x4c */
	emptymsg, /* 0x4d */
	"Overlapped cmds attempted", /* 0x4e */
	emptymsg, /* 0x4f */
	emptymsg, /* (tape only) 0x50 */
	emptymsg, /* (tape only) 0x51 */
	emptymsg, /* (tape only) 0x52 */
	"Media load/unload failure", /* 0x53 */
	emptymsg, /* 0x54 */
	emptymsg, /* 0x55 */
	emptymsg, /* 0x56 */
	"Unable to read table of contents", /* 0x57 */
	"Generation (optical device) bad", /* 0x58 */
	"Updated block read (optical device)", /* 0x59 */
	"Operator request or state change", /* 0x5a */
	"Logging exception", /* 0x5a */
	"RPL status change", /* 0x5c */
	emptymsg, /* 0x5d */
	emptymsg, /* 0x5e */
	emptymsg, /* 0x5f */
	"Lamp failure", /* 0x60 */
	"Video acquisition error/focus problem", /* 0x61 */
	"Scan head positioning error", /* 0x62 */
	"End of user area on track", /* 0x63 */
	"Illegal mode for this track", /* 0x64 */
#endif /* 0 */
}
#endif /* PROM */
;/* no secondary messages in PROM; just a bss array */
#endif /* !IP22_WD95 */


scsisubchan_t *
allocsubchannel(int adap, int target, int lu)
{
	register scsidev_t *dp;
	register scsisubchan_t *sp;
	char			name[9];
	dmamap_t *scsi_getchanmap(dmamap_t *);
	int s;

	if(adap >= scsicnt ||
		(dp = &scsidev[adap])->d_subchan[target][lu] ||
		!dp->d_map || dp->d_map == DMAMAP_FAIL ||
		target == scsi_ha_id[adap])
		return NULL;	/* illegal adapter or target #, or this
			unit/lu already allocated to someone else, or the
			dma map couldn't be allocated for some reason, or
			the target is the host adapter ID. */
	

	/* calloc so we don't need to zero things */
	sp = (scsisubchan_t *)kern_calloc(sizeof(*sp), 1);
	if(sp == NULL)
		return NULL;
	if((sp->s_map = scsi_getchanmap(dp->d_map)) == NULL) {
		kern_free((caddr_t)sp);
		return NULL;
	}
	s = splockspl(dp->d_qlock, spl5);
	if(dp->d_subchan[target][lu]) {
		/* recheck after getting lock, and after all
		 * potential sleeps */
		kern_free((caddr_t)sp);
		spunlockspl(dp->d_qlock, s);
		return NULL;
	}
	dp->d_subchan[target][lu] = sp;
	spunlockspl(dp->d_qlock, s);
	sp->s_adap   = adap;
	sp->s_target = target;
	sp->s_lu     = lu;
	init_sema(&sp->s_sem, 0, "sub", target*SC_MAXTARG+lu);

	/* this happens when sync mode is still on from the prom/sash */
	if(scsisyncreg[adap] && scsisyncreg[adap] != 0xff &&
		target == scsisyncdest[adap]) {
		sp->s_syncreg = scsisyncreg[adap];
		sp->s_flags |= S_SYNC_ON;
		scsisyncreg[adap] = 0;
	}
	else if(getreg93(WD93SYNC) && (getreg93(WD93DESTID) & (SC_MAXTARG-1))
		== target)  {
		/* this happens when symmon is loaded from the kernel, when both
		 * the kernel and symmon are on the disk. */
		sp->s_syncreg = getreg93(WD93SYNC);
		sp->s_flags |= S_SYNC_ON;
		scsisyncreg[adap] = 0;
	}
	/* allow booting from drives that will not
	handle sync scsi on machines that support it (or even force
	sync scsi on machines that don't normally support it. */
	{ char *smode; uint sync;
	if((smode=getenv("scsi_syncenable")) && atobu(smode, &sync))
		scsi_syncenable[adap] = sync;
	}
		
	return(sp);
}

void
freesubchannel(register scsisubchan_t *sp)
{
	register scsidev_t *dp;

	if(sp == NULL)
		return;	/* paranoia; caller probably failed allocsubchan ... */
	dp = &scsidev[sp->s_adap];
	/*	this is here because the sync flag may be set in allocsubchan
		when we are just doing inq's, so no one will turn it off. */
	if((sp->s_flags & S_SYNC_ON) &&
		scsisyncdest[sp->s_adap] == sp->s_target) {
		scsisyncdest[sp->s_adap] = 0xff; /* no further matches */
		scsi_setsyncmode(sp, 0);
	}

	/* do this because otherwise device would still be in sync mode on
	 * next access, because upper level driver doesn't know device is
	 * in sync mode. */
	if((sp->s_flags&(S_SYNC_ON|S_TARGSYNC)) == (S_SYNC_ON|S_TARGSYNC))
		scsi_setsyncmode(sp, 0);

#if !defined(IP28) && !defined(IP20) && !defined(IP22) && !defined(IP26)
	/*	can't leave advanced mode enabled from sash, fx, etc. if we
		might make a call to the PROM, for systems whose proms do no
		understand sync mode.  This happens, e.g., when
		symmon is read in by the kernel using the prom */
	wd93reset(dp, 1);
#endif
	dp->d_subchan[sp->s_target][sp->s_lu] = NULL;
	freesema(&sp->s_sem);
	scsi_freechanmap(sp->s_map);
	kern_free((caddr_t)sp);
}



/* main entry point.  Do the housekeeping, start (or queue) the request,
 * and wait for it to complete if no notifier routine was specified.
 *
 * If no_nextcmd is set, we are either standalone, or doing a panic
 * dump, so spin waiting for completion (no interrupts.
 * Use the real time clock to wait the right length of time
 * so that we are independant of clock speed.
 * For the standalone code, we need to catch user interrupts so we can
 * cleanup.  Otherwise next scsi access usually hangs the program.
*/
void
doscsi(register scsisubchan_t *sp)
{
	register scsidev_t *dp = &scsidev[sp->s_adap];
	int done = 0;
	int s;

	/*	zero b_resid so doesn't have to be done by all scsi drivers
		(needs to be 0 for other parts of the resid code, even if
		count is 0) */
	sp->s_buf->b_resid = 0;
	sp->s_tentative = 0;
	/* too many places don't flush, so in standalone, do the
	 * flush here.  */
	if (sp->s_buf->b_bcount) {
		if (cachewrback || (sp->s_buf->b_flags & B_READ))
			vflush(sp->s_buf->b_dmaaddr, sp->s_buf->b_bcount);
	}
	if(sp->s_buf->b_flags & B_READ)
		sp->s_dstid = sp->s_target|SCDMA_IN;
	else {
		sp->s_dstid = sp->s_target;
	}

	/* clear these here.  If the transaction survives to
	 * scsisubrel() without somebody setting s_error, there was no error.
	 */
	sp->s_status = sp->s_error = 0;

	if(no_nextcmd) {
		unsigned long timeo_time;	/* needs to match get_tod() */
#ifdef EVEREST
		uint loops;
#endif
restart:
		s = splockspl(dp->d_qlock, spl5);
		startscsi(dp, sp);
		spunlockspl(dp->d_qlock, s);

#ifdef EVEREST
		/*
		 * We convert sp->s_timeoutval (which is in HZ) to number of
		 * loops through the wait-for-interrupt while loop.  SCSI_DELAY
		 * (defined immediately below) is in microseconds.
		 */
#define SCSI_DELAY 25
		timeo_time = sp->s_timeoutval * ((1000000 / HZ) / SCSI_DELAY);
		loops = 0;
		while(sp->s_flags&S_BUSY) {
			while(!scsi_int_present(dp)) {
				us_delay(SCSI_DELAY);
				if (++loops == timeo_time)
					break;
			}
			s1dma_flush(sp->s_map);
#else
		/* want to round up, and use an extra second, since we don't
		 * know just where in the one second interval we are. */
		timeo_time = get_tod() + (sp->s_timeoutval+2*HZ-1)/HZ;
		while(sp->s_flags&S_BUSY) {
			while(!scsi_int_present(dp) && get_tod() < timeo_time)
				DELAY(25);
#endif
			if(scsi_int_present(dp))
				scsiintr(dp->d_adap); /* handle the intr */
			else
				scsi_timeout(sp);
			if(sp->s_flags & S_RESTART)
				goto restart;	/* probably due to AUX_LCI */
		}
	} else {
		while(!done) {
		/*	Start hardware rolling.  On requests with no
			notifier routine, returning here means that the
			cmd failed and was detected at the next interrupt (which
			SHOULD be a re-selection).  */
			s = splockspl(dp->d_qlock, spl5);
			startscsi(dp, sp);
			spunlockspl(dp->d_qlock, s);
			if(sp->s_notify)
				done = 1;
			else {
				psema(&sp->s_sem, PSCSI);
				/* no spl needed here, since the semaphore usage
				 * guarantees that channel isn't active (no need
				 * to worry about timeout either). */
				done = !(sp->s_flags & (S_BUSY|S_RESTART));
			}
		}
	}
}

/* used by scsihinv so we don't waste lots of time doing hinv during boot,
 * etc; no reason to wait so long, almost all devices faster than this.  Fo
 * 'real' accesses, give the full 250 ms selection timeout still, so callers
 * of this routine are expected to reset to the correct value.
 * Called from some scsi diags to reduce selection timeout also.
 */
void
scsi_seltimeout(u_char adap, int makeshort)
{
	scsidev_t *dp = &scsidev[adap];
	u_char toutval;

	if(makeshort)
		toutval = T93SATN_SHORT;
	else
		toutval = T93SATN;
	putreg93(WD93TOUT, toutval)
}

static COMPONENT scsiadaptmpl = {
	AdapterClass,			/* Class */
	SCSIAdapter,			/* Type */
	0,				/* Flags */
	SGI_ARCS_VERS,			/* Version */
	SGI_ARCS_REV,			/* Revision */
	0,				/* Key */
	0x01,				/* Affinity */
	0,				/* ConfigurationDataSize */
	8,				/* IdentifierLength */
	"WD33C93\000"			/* Identifier */
};
static COMPONENT scsitmpl, scsictrltmpl;

/* probe if controller chip is present
 */
static void
scsihinv(COMPONENT *c, u_char adap)
{
	scsidev_t *dp = &scsidev[adap];

	if(!(dp->d_flags & (D_WD93|D_WD93A|D_WD93B))) {
	    if (showconfig)
		cmn_err(CE_CONT,"sc%d: controller not present at addr %x\n",
		    adap, dp->d_addr);
	}
	else {
	    c = AddChild(c,&scsiadaptmpl,(void *)NULL);
	    if (c == (COMPONENT *)NULL) cpu_acpanic("scsi adapter");
	    if (dp->d_flags&D_WD93A) {		/* got a 93A -- fix up cfg */
	    	c->IdentifierLength = 9;
		c->Identifier[7] = 'A';
	    } else if (dp->d_flags&D_WD93B) {	/* got a 93B -- fix up cfg */
	    	c->IdentifierLength = 9;
		c->Identifier[7] = 'B';
	    }
	    c->Key = adap;

	    /* fill in unchanged parts of templates */
	    bzero(&scsictrltmpl,sizeof(COMPONENT));
	    scsictrltmpl.Class = ControllerClass;
	    scsictrltmpl.Version = SGI_ARCS_VERS;
	    scsictrltmpl.Revision = SGI_ARCS_REV;
	    scsitmpl.Class = PeripheralClass;
	    scsitmpl.Version = SGI_ARCS_VERS;
	    scsitmpl.Revision = SGI_ARCS_REV;
	    scsitmpl.AffinityMask = 0x01;

	    RegisterInstall(c,scsiinstall);
	}
}

/*  Run configuration for the scsi adpater "c", registering all child
 * devices.
 */
void
scsiinstall(COMPONENT *c)
{
	int adap = c->Key;
	char *which, *extra, *inv;
	u_char targ, lu;
	COMPONENT *tmp, *tmp2;
	union scsicfg cfgd;
	char id[26];		/* 8 + ' ' + 16 + null */
	int type;

        scsi_seltimeout(adap, 1);	/* shorten selection timeout */

	/* now add all the scsi devices for this adapter to the inventory.
	 * It is now back in scsi.c, rather than scattered in the different
	 * SCSI drivers.  This speeds things up a bit, since we scan once
	 * per adapter, rather once for all adapters in each SCSI driver.
	 * rather than once per driver, and reduces code size a bit also.
	 * For kernel, it means we always inventory all the devices,
	 * even if the driver isn't present.  This will allow us to
	 * add 'probing' for SCSI drivers to lboot.  For now, do NOT probe
	 * for LUN's other than 0; it turns out that some devices (including
	 * the Tandberg QIC tape drive) do not handle it correctly) ...*/
	for(lu=targ=0; targ < SC_MAXTARG; targ++) {
		if(scsi_ha_id[adap] == targ)
			continue;
		if(!(inv = (char *)scsi_inq(adap, targ, lu)) ||
			(inv[0]&0x10) == 0x10) 
			continue; /* LUN not supported, or not attached */

		/* ARCS Identifier is "Vendor Product" */
		strncpy(id,inv+8,8); id[8] = '\0';
		for (extra=id+strlen(id)-1; *extra == ' '; *extra-- = '\0') ;
		strcat(id," ");
		strncat(id,inv+16,16); id[25] = '\0';
		for (extra=id+strlen(id)-1; *extra == ' '; *extra-- = '\0') ;
		scsictrltmpl.IdentifierLength = strlen(id);
		scsictrltmpl.Identifier = id;

		switch(inv[0]) {
		case 0:	/* hard disk */
		    extra = "disk";
		    which = (inv[1] & 0x80) ? "removable " : "";

		    scsictrltmpl.Type = DiskController;
		    scsictrltmpl.Key = targ;
		    tmp = AddChild(c,&scsictrltmpl,(void *)NULL);
		    if (tmp == (COMPONENT *)NULL) cpu_acpanic("disk ctrl");

		    if (inv[1]&0x80) {
			    scsitmpl.Type = FloppyDiskPeripheral;
			    scsitmpl.Flags = Removable|Input|Output;
			    scsitmpl.ConfigurationDataSize =
				    sizeof(struct floppy_data);
			    bzero(&cfgd.fd,sizeof(struct floppy_data));
			    cfgd.fd.Version = SGI_ARCS_VERS;
			    cfgd.fd.Revision = SGI_ARCS_REV;
			    cfgd.fd.Type = FLOPPY_TYPE;
		    }
		    else {
			    scsitmpl.Type = DiskPeripheral;
			    scsitmpl.Flags = Input|Output;
			    scsitmpl.ConfigurationDataSize =
				    sizeof(struct scsidisk_data);
			    bzero(&cfgd.disk,sizeof(struct scsidisk_data));
			    scsi_rc(adap,targ,lu,&cfgd.disk);
			    cfgd.disk.Version = SGI_ARCS_VERS;
			    cfgd.disk.Revision = SGI_ARCS_REV;
			    cfgd.disk.Type = SCSIDISK_TYPE;
		    }
		    scsitmpl.Key = lu;

		    tmp = AddChild(tmp,&scsitmpl,(void *)&cfgd);
		    if (tmp == (COMPONENT *)NULL) cpu_acpanic("scsi disk");
		    RegisterDriverStrategy(tmp,dksc_strat);
		    break;

		case 1:	/* sequential == tape */
		    type = tpsctapetype(inv, 0);
		    which = "tape";
		    extra = inv;
		    sprintf(inv, " type %d", type);

		    scsictrltmpl.Type = TapeController;
		    scsictrltmpl.Key = targ;
		    tmp = AddChild(c,&scsictrltmpl,(void *)NULL);
		    if (tmp == (COMPONENT *)NULL) cpu_acpanic("tape ctrl");

		    scsitmpl.Type = TapePeripheral;
		    scsitmpl.Flags = Removable|Input|Output;
		    scsitmpl.ConfigurationDataSize = 0;
		    scsitmpl.Key = lu;

		    tmp = AddChild(tmp,&scsitmpl,(void *)NULL);
		    if (tmp == (COMPONENT *)NULL) cpu_acpanic("scsi tape");
		    RegisterDriverStrategy(tmp,tpsc_strat);
		    break;
		default:
		    type = inv[1] & INV_REMOVE;
		    /* ANSI level: SCSI 1, 2, etc.*/
		    type |= inv[2] & INV_SCSI_MASK;

		    if ((inv[0]&0x1f) == INV_CDROM) {
			    scsictrltmpl.Type = CDROMController;
			    scsitmpl.Type = DiskPeripheral;
			    scsitmpl.Flags = ReadOnly|Removable|Input;
			    which = "CDROM";
		    }
		    else {
			    scsictrltmpl.Type = OtherController;
			    scsitmpl.Type = OtherPeripheral;
			    scsitmpl.Flags = 0;
			    sprintf(which=inv, " device type %u",
					inv[0]&0x1f);
		    }

		    scsitmpl.ConfigurationDataSize = 0;
		    scsictrltmpl.Key = targ;
		    tmp = AddChild(c,&scsictrltmpl,(void *)NULL);
		    if (tmp == (COMPONENT *)NULL) cpu_acpanic("other ctrl");

		    scsitmpl.Key = lu;

		    tmp2 = AddChild(tmp,&scsitmpl,(void *)NULL);
		    if (tmp2 == (COMPONENT *)NULL) cpu_acpanic("scsi device");

		    if (tmp->Type == CDROMController)
			RegisterDriverStrategy(tmp2,dksc_strat);

		    extra = "";
		    break;
		}
		if(showconfig) {
		    cmn_err(CE_CONT,"SCSI %s%s (%d,%d", which,
			extra?extra:"", adap, targ);
		    if(lu)
			cmn_err(CE_CONT, ",%d", lu);
		    cmn_err(CE_CONT, ")\n");
		}
	}

	scsi_seltimeout(adap, 0);       /* standard selection timeout */

	/* revert the Toshiba CDROM to claiming to be a CDROM.  This is so
	 * newer proms correctly show it in the hardware inventory, and also
	 * so sash can do auto-installation from it.  This requires the
	 * SCSI driver, but is not a driver in and of itself. */
	revertcdrom();
}

#define SCSI_IDATA_SIZE 37

/*
 * SCSI edt init routine.  Note that it is called AFTER the scsi driver
 * init routines that are called from io_init. 
 * Only do once; then do all adapters;
 */
void
scsiedtinit(COMPONENT *c)
{
	u_char adap;

#ifdef IP22_WD95  /* Farid- Correct this later */
		if(scsidev[0].d_map && (__psint_t)scsidev[0].d_map != -1)
			scsihinv(c,0);
		/* else failed to find/init SCSI chip for this adapter */
#else
	for(adap=0; adap<scsicnt; adap++)
		if(scsidev[adap].d_map && (__psint_t)scsidev[adap].d_map != -1)
			scsihinv(c,adap);
		/* else failed to find/init SCSI chip for this adapter */
#endif
}


/*	prevent chip from interrupting during rest of boot till ready,
	and do any initialization that is needed prior to use of
	the driver.  Called before main().  Needs to return a value
	for standalone.
*/
int
scsiunit_init(adap)
{
	scsidev_t *dp; 
	char name[9];

	/* allocate array on first call; scsicnt may be changed in
	 * IP*.c before the first call to this routine. */
	if(!scsidev &&
		!(scsidev=(scsidev_t*)kern_calloc(sizeof(*scsidev), scsicnt)))
		cmn_err(CE_PANIC, "no memory for scsi device array");

	dp = &scsidev[adap]; 
	dp->d_addr  = scuzzy[adap].d_addr;
	dp->d_data  = scuzzy[adap].d_data;
	dp->d_adap = adap;
	dp->d_map = DMAMAP_FAIL; /* in case not present, for allocsubchan() */

	if(!scsi_present(dp))
		return -1;

	dp->d_map = dma_mapalloc(DMA_SCSI, adap, 0, 0);
	if(dp->d_map == DMAMAP_FAIL)
		return -1;
	init_sema(&dp->d_sleepsem, 0, "dps", adap);
	init_sema(&dp->d_onlysem, 0, "dpo", adap);
	init_spinlock(&dp->d_qlock, "dpq", adap);

	wd93reset(dp, 0);	/* reset so we can tell if 93A */
	return 0;	/* for ide; */
}

/*
 * Detects if there is a scsi chip present.
 *
 * Just write some numbers in a register
 * and make sure they read back the same.
 */
static int
scsi_present(scsidev_t *dp)
{
	int i, retval;

	for (i = 0; i < SC_MAXTARG; i++) {
		putreg93(WD93ID, i)
		retval = getreg93(WD93ID);
		if (retval != i)
			return(0);
	}
	return(1);
}

/* this routine is used to do the hardware inventory, etc.  It
 * returns NULL on errors, else a ptr to 37 bytes of null
 * terminated inquiry data, which is enough for type info, and mfg
 * name, product name, and rev.  Only valid to call at init time,
 * when no driver open routines have yet been called.   It isn't
 * static, because ide uses it also.
*/
u_char *
scsi_inq(u_char adap, u_char targ, u_char lun)
{
        static u_char idata[SCSI_IDATA_SIZE];
	static u_char inquiry[6];	/* can't initialize for PROM */
	u_char *buf;

	buf = dmabuf_malloc(SCSI_IDATA_SIZE);
	bzero(buf, SCSI_IDATA_SIZE);
	inquiry[0] = 0x12;
	inquiry[1] = lun<<5;
	inquiry[4] = SCSI_IDATA_SIZE-1;

	if (scsi_ecmd(adap,targ,lun,inquiry,6,buf,SCSI_IDATA_SIZE-1) != 0) {
		bcopy(buf, idata, SCSI_IDATA_SIZE);
		dmabuf_free(buf);
		return idata;
	} else {
		dmabuf_free(buf);
		return 0;
	}
}

static int
scsi_rc(u_char adap, u_char targ, u_char lun, struct scsidisk_data *d)
{
	unsigned int *dc;
	u_char inq[10];

	bzero(inq,10);
	inq[0] = 0x25;
	dc = dmabuf_malloc(sizeof(unsigned int) * 2);

	scsi_ecmd(adap,targ,lun,inq,10,dc,8);

#ifdef _MIPSEL
	swapl(dc,2);
#endif
	d->MaxBlocks = dc[0];
	d->BlockSize = dc[1];

	dmabuf_free(dc);
	return(0);
}

/* scsi early command */
static caddr_t
scsi_ecmd(u_char adap, u_char targ, u_char lun,
	  u_char *inquiry, int il,
	  void *data, int dl)
{
	register scsisubchan_t *sp;
	int first = 2;
	buf_t *bp;
	caddr_t ret;

	if((sp = allocsubchannel(adap, targ, lun)) == NULL ||
		(bp = (buf_t *)kern_calloc(sizeof(*bp), 1)) == NULL)
		return NULL;

	sp->s_notify = NULL;
	sp->s_cmd = inquiry;
	sp->s_len = il;
	sp->s_buf = bp;
	/* SCSI 2 spec strongly recommends all devices be able to
	 * respond successfully to inq within 2 secs of power on.
	 * some ST4 1200N (1.2) drives occasionally accept the cmd
	 * and disc, but don't come back within 1 second, even when
	 * motor start jumper is on, at powerup.   Only seen on IP12,
	 * probably because it gets to the diag faster.  */
	sp->s_timeoutval = 2*HZ;
again:
	/*  So we don't keep data left from an earlier call.  Also
	 * insures null termination. */
	bzero(data, dl);

	bp->b_dmaaddr = (caddr_t) data;
	bp->b_bcount = dl;
	bp->b_flags = B_READ|B_BUSY;
	doscsi(sp);
	if(sp->s_error == SC_GOOD && sp->s_status == ST_GOOD)
		ret = (caddr_t) data;
	else if(first-- && sp->s_error != SC_TIMEOUT) {
		/* CMDTIME can also indicate a bus reset; could be bus problems,
		 * or sync vs async wd problem; at least it responded to selection,
		 * so retry it. */
		if(sp->s_error == SC_HARDERR || sp->s_error == SC_CMDTIME)
			us_delay(500000);	/* give it some recovery time */
		else
			us_delay(10000);
		goto again;     /* one retry, if it not a timeout. */
	}
	else
		ret = NULL;
	sc_inq_stat = sp->s_error;	/* mainly for standalone ide */
	freesubchannel(sp);
	kern_free((caddr_t)bp);

	return(ret);
}


/*
 * scsiintr - the SCSI interrupt handler.
 */
void
scsiintr(int adap)
{
	register scsidev_t *dp = &scsidev[adap];

	spsema(dp->d_qlock);

	if(!sc_wasreset[adap]) {
		if(getauxstat() & AUX_INT) {
			dp->d_flags |= D_BUSY;	/* busy now */
			handle_intr(dp);
		}
		/* else intr already handled, maybe by sync code */
	}
	svsema(dp->d_qlock);
}


/*	Setup for synchronous mode.  Can only be done on one device
	at a time, because we handle the phases 'manually' and poll
	the interrupts.  Furthermore, it's not reliable if any other
	channels are busy, since they might re-select.
	We have to be prepared to 'negotiate' the values
	used for the transfer period and the ack offset. We give them
	our highest values; if they come back with lower ones, we use
	those. 
*/
void
scsi_setsyncmode(scsisubchan_t *sp, int enable)
{
	scsidev_t *dp;
	int s;
	int nonly = 0;

	/*	If enabling, and it's on, or we've decided the device
		can't support it, or kernel is configured to not try
		for this device, return; also if disabling, and sync not
		enabled on this device, return */
	if((enable && ((sp->s_flags & (S_SYNC_ON|S_CANTSYNC)) ||
		!(scsi_syncenable[sp->s_adap] & (1 << sp->s_target)))) ||
		(!enable && !((sp->s_flags & (S_SYNC_ON|S_RESYNC)) ||
		sp->s_syncs)))
	{
		return;
	}	

	dp = &scsidev[sp->s_adap];
	s = splockspl(dp->d_qlock, spl5);

	/*	don't return from scsiacquire till we have the adapter */
	sp->s_notify = NULL;

re_acqit:
	(void)scsiacquire(dp, sp, nonly);
	/*	we don't set S_BUSY, because handle_reset() would
		call scsisubrel, which would do a vsema on the semaphore;
		that wouldn't be good... All the code from here on checks
		s_error where appropriate. */
	if(sp->s_error) {
		if(!enable) {	/* force syncreg, etc off; if there's been
			* a reset, we don't want low level code to
			* re-negotiate sync! */
			sp->s_flags |= S_CANTSYNC;
			sp->s_syncs = 0;
			sp->s_syncreg &= 0xf0;
		}
		goto release;	/* probably a scsi bus reset */
	}
	if(sc_busycnt(dp)) {
		sc_waitonly(dp);
		nonly = 2;	/* to turn off D_ONLY */
		goto re_acqit;
	}

	sync_setup(dp, sp, enable, 0);

release:
	if(dp->d_consubchan == sp) {
		sp->s_flags &= ~(S_BUSY|S_DISCONNECT|S_SAVEDP|S_RESTART);
		scsirelease(dp, 1);
	}
	/* else has been reset; handle_reset() has already called scsirelease(). */

	spunlockspl(dp->d_qlock, s);
}

/* send an abort message to a device, expecting it to go to bus free
 * afterwards.  Perhaps should have this a more general interface
 * and document it, but for now this is all we need, and easier
 * to maintain.  Used in tpsc.c.  Deliberately NOT documented.
 * follows same model as scsi_setsyncmode.
*/
void
scsi_sendabort(scsisubchan_t *sp)
{
	scsidev_t *dp;
	int s, nonly=0;
	u_char msg[2], state;

	dp = &scsidev[sp->s_adap];
	s = splockspl(dp->d_qlock, spl5);

	/*	don't return from scsiacquire till we have the adapter */
	sp->s_notify = NULL;

re_acqit:
	(void)scsiacquire(dp, sp, nonly);
	/*	we don't set S_BUSY, because handle_reset() would
		call scsisubrel, which would do a vsema on the semaphore;
		that wouldn't be good... All the code from here on checks
		s_error where appropriate. */
	if(sp->s_error)
		goto release;	/* probably a scsi bus reset */
	if(sc_busycnt(dp)) {
		sc_waitonly(dp);
		nonly = 2;	/* to turn off D_ONLY */
		goto re_acqit;
	}

	/* ID msg (no disc) followed by the abort msg byte */
	msg[0] = 0x80; msg[1] = 6;

	/* Now send the message */
	putreg93(WD93LUN, sp->s_lu)
	putreg93(WD93DESTID, sp->s_target)

	/*	get to initiator state. */
	if(do_select(sp, dp))
		goto error;
	if(do_trinfo(dp, msg, 2, 75000))
		goto error;
	state = wait_scintr(dp, 50000);
	if(state != ST_DISCONNECT) {
		sccmd_lci(dp, C93DISC);
		state = wait_scintr(dp, 2000);
	}

	if(!(getauxstat() & AUX_BSY))
		goto release;
	/* else fall through to error case */

error:
	sccmd_lci(dp, C93DISC);	/* try to ensure disconnected on errors */
	ifintclear( ; )
	if(!(getauxstat() & AUX_BSY)) {
		/* All done, cleaned up OK */
		putreg93(WD93PHASE, PH_NOSELECT)
	}
	else { /* chip still busy, blow it all away */
		scsi_reset(dp->d_adap);
		handle_reset(dp);
		cmn_err(CE_NOTE,
			"sc%d,%d,%d: error during abort message, resetting SCSI bus\n",
			sp->s_adap, sp->s_target, sp->s_lu);
		/* no callers check today, but some might someday */
		sp->s_error = SC_HARDERR;
	}

release:
	if(dp->d_consubchan == sp) {
		sp->s_flags &= ~(S_BUSY|S_DISCONNECT|S_SAVEDP|S_RESTART);
		scsirelease(dp, 1);
	}
	/* else has been reset; handle_reset() has already called scsirelease(). */

	spunlockspl(dp->d_qlock, s);
}



/*	End of externally visible routines.  All routines after this are
	local to this module */


/*	do sync negotiations.  If enable is non-zero, we are setting it
	up, if zero, we want to turn it off (like for large transfers where
	we can't map the whole thing).  Must already own the adapter.
	must at at spl5 or above.
	This routine intermittently fails if other channels are busy
	and try to re-select.  Therefore it must only be called
	when no other channels are busy. 

	This process typically takes 2-5 ms to complete, since we have
	to handle all the phases.  Someday the driver should be fixed to
	set a flag that says to ignore scsi interrupts, so we can run at a
	lower spl.  Fortunately, this doesn't happen very often.

	selected is 1 if the target initiated the negotiation, otherwise 0.
*/
static void
sync_setup(register scsidev_t *dp, register scsisubchan_t *sp,
	int enable, int selected)
{
	uint istate;
	u_char transperiod, ackoff;
	u_char state, cnt;
	u_char msg[6], msgin[5], *mptr;
	int ret, didmsgin;
	
	/* syncreg in case fail to enable sync, and for disables.  Clear
	 * targsync for freesubchan; if we are called, driver knows target
	 * is in sync mode (except for handle_extmsgin, which sets targsync
	 * at end. */
	sp->s_error = sp->s_status = sp->s_syncreg = sp->s_syncs = 0;
	/* for disable, or failed enable */
	sp->s_flags &= ~(S_SYNC_ON|S_RESYNC|S_TARGSYNC|S_CANTSYNC);

	if(enable) {
		/* done here also, so it can be changed on the fly with the
			kernel debugger or dbx */
		/* for now, the 93A isn't working in sync mode... */
		if(!(scsi_syncenable[sp->s_adap] & (1 << sp->s_target)))
			return;
		/* 93A has a 12 deep fifo, 93 has 5, but Tech Brief JL0014
			from John Laube at WD says to use at most 4 on the 93.
			Turns out that you get about 7% better throughput on
			the 93A (at least on IOC2 on IP6), if you use 10 (there is
			no improvement at 12 on an IP12, so just leae it at 10). */
		ackoff = (dp->d_flags&(D_WD93A|D_WD93B)) ? 10 : 4;
	}
	else
		ackoff = 0;

	/* ID byte followed by 'sync' mesg; these were static initializers,
	 * but that won't work for PROM */
	transperiod = scsi_syncperiod[sp->s_adap];
	if(!(dp->d_flags & D_WD93B) && transperiod<50)
		transperiod = 50;	/* only 93B can do fast scsi */
	msg[0] = 0x80; msg[1] = 1; msg[2] = 3; msg[3] = 1;
	msg[5] = ackoff;
	msg[4] = transperiod;

	putreg93(WD93SYNC, 0)	/* don't want to try to transfer
		any data synchronously, in case previous device was in
		sync mode. */
	putreg93(WD93LUN, sp->s_lu)
	putreg93(WD93DESTID, sp->s_target)

	/*	get to initiator state. This generates 2 interrupts, at
		least with CDC 94171.  first is status ST_SELECT,
		2nd is status ST_REQ_SMESGOUT */
	if(!selected && do_select(sp, dp))
		goto error;

	if(!selected)
		mptr=msg, cnt=sizeof(msg);
	else
		mptr=msg+1, cnt=sizeof(msg)-1;	/* no 0x80 */
	if(ret = do_trinfo(dp, mptr, cnt, 10000)) {
		if(ret == -1) {
			istate = do_trinfo(dp, msgin, -1, 10000);
			if(getstat() == ST_TRANPAUSE)
				sccmd_lci(dp, C93NACK);	/* negate the ack */
			/* should get phase change */
			(void)wait_scintr(dp, 1000);
			if(istate == 7 || (!istate && *msgin == 7)) {
				msgin[4] = 0; /* can't sync */
				msgin[3] = 0xff; /* 800 ns */
				sp->s_flags |= S_CANTSYNC;
				goto done;	/* handle cmd, etc. */
			}
		}
		goto error;	/* other fail, or not rej msg in */
	}

	/* wait for req msg in, then read the response; in some cases,
	 * such as a target initiated sync msg, we may not get an
	 * interrupt here. */
	istate = wait_scintr(dp, 2000);
	if((istate & 7) == 2) { /* get 1a on Toshiba 156FB,
		Most devices wait for the msg out phase ... */
		sp->s_flags |= S_CANTSYNC;
		if(do_inquiry(dp))
			goto inq_err;
		return;
	}

	if((istate & 7) == 7) {	/* only if bus phase is msgin */
		istate = do_trinfo(dp, msgin, -sizeof(msgin), 10000);
		didmsgin = 1;
		if(getstat() == ST_TRANPAUSE)
			sccmd_lci(dp, C93NACK);	/* negate the ack */
		/* should get an intr, with state != pause */
		(void)wait_scintr(dp, 5000);
		if(istate) {
			if(istate == 7) {
				msgin[4] = 0; /* can't sync */
				msgin[3] = 0xff; /* 800 ns */
				sp->s_flags |= S_CANTSYNC;
			}
			else
				goto error;
		}
		else if(msgin[2]!=1 || msgin[1]!=3) {
			/* oops, whatever we got back, it wasn't a sync msg! */
			sp->s_flags |= S_CANTSYNC;
			goto error;
		}
	}
	else if(!selected && (istate & INTR_ERR))
		/* didn't get message out phase, and we initiated sync neg */
		goto error;
	else /* often happens on target initiated sync msg; since our
	 * limits are fairly low.  target accepted our reply to its
	 * sync msg, and we are probably at data in or our */
		didmsgin = 0;

done:
	ifintclear(;);

	state = getstat();	/* after the intr is cleared */
	if((state & 7) == 6) {
		/* message out; see this sometimes on sony m/o that does a
		 * msg reject after first byte of xtd msg out, then after we
		 * read the msg byte, it goes to msg out again; send an
		 * abort message */
		*msg = 0x6;
		(void)do_trinfo(dp, msg, 1, 10000);
	}

	/* MCI == cmd and 0x8 bit set means device wants a cmd; chk state again,
	 * in case it changed from the nack, etc.  0x40 is because we
	 * sometimes get this on msg rej back, and the wd won't change
	 * the state until another phase change; this is a bit of a hack... */
	if((state & 7) == 2 || state == 0x40)
		if(do_inquiry(dp))
			goto inq_err;
	if(enable && didmsgin && msgin[4] < ackoff) {
		ackoff = msgin[4];	/* use targets value */
		if(ackoff == 0)	/* so we won't try again */
			sp->s_flags |= S_CANTSYNC;
	}

	/* on 93A, the transfer period affects async also, so check
		even if ackoff is 0 */
	if(didmsgin && msgin[3] > transperiod)
		transperiod = msgin[3];

	if(enable) {
		transperiod = transval(transperiod);
		if(dp->d_flags & D_WD93)
			transperiod++;	/* 93 is much more limited when
				using DMA, but that's life... */
		sp->s_syncreg = (transperiod << 4) | (ackoff & 0xf);
		if(ackoff)
			sp->s_flags |= S_SYNC_ON;
	}
	/* else leave 0 if disabled */

	/* clear possible disc intr (observed on some Wren III disks) */
	ifintclear(;);
	if(!(getauxstat() & AUX_BSY))
		return;

error:
	if(sp->s_error == SC_TIMEOUT)
		sp->s_flags |= S_CANTSYNC;
	sccmd_lci(dp, C93DISC);	/* try to ensure disconnected on errors */
	ifintclear( ; )
	if(!(getauxstat() & AUX_BSY)) {
		/* All done, cleaned up OK */
		putreg93(WD93PHASE, PH_NOSELECT)
		return;
	}
	/* else chip still busy, blow it all away */

inq_err:
	scsi_reset(dp->d_adap);
	handle_reset(dp);
	cmn_err(CE_NOTE,
		"sc%d,%d,%d: SYNC negotiation error, resetting SCSI bus\n",
		sp->s_adap, sp->s_target, sp->s_lu);
	/* no callers check today, but some might someday */
	sp->s_error = SC_HARDERR;
}


/*	return the correct bits to put in the sync register given
	a tranfer period in ns/4, assuming WD clocked at 10Mhz.
	Works same at 20 Mhz with divisor of 4.
	For fast SCSI, we set one more bit, which doubles frequency,
	but doesn't give us a very wide range (100ns, 
*/
static u_char
transval(u_char period)
{
#define FSS_BIT 8
	u_char ret;
	if(period > 175)
		ret = 0;	/* slowest we can do is 800 ns, so use async */
	else if(period < 50) {	/* fast SCSI; slightly different skew
		timings, etc., so can't use FSS bit with periods > 50, even
		though it would give us 250 and 350 ns transfer periods, which
		we have wanted... */
		if(period <= 25)
			ret = FSS_BIT|2;
		else if(period <= 38)
			ret = FSS_BIT|3;
		else
			ret = FSS_BIT|4;
	}
	else
		ret = (period+24) / 25;
	return ret;
}


/*	this routine returns the # of busy channels other than the
	current owner of the adapter (if any).  Must be at spl5.
*/
static int
sc_busycnt(register scsidev_t *dp)
{
	register int i, j, cnt;
	register scsisubchan_t *sp;

	for (cnt=0, i=0; i < SC_MAXTARG; i++) {
		if(i==scsi_ha_id[dp->d_adap])
			continue;
		for(j = 0; j < SC_MAXLUN; j++) {
			sp = dp->d_subchan[i][j];
			if(sp && sp != dp->d_consubchan &&
				(sp->s_flags & S_BUSY))
				cnt++;
		}
	}
	return cnt;
}


/* wait until no other devices are active on this controller.  A seperate
 * routine because we do this in several places.  Generally done becase
 * we are about to negotiate sync scsi, we run into problems if some
 * device tries to reconnect while we are doing stuff.
*/
static void
sc_waitonly(scsidev_t *dp)
{
	dp->d_onlycount++;
	scsirelease(dp, 0);
	svpsema(dp->d_qlock, &dp->d_onlysem, PSCSI);
	spsema(dp->d_qlock);
}

/* raise and drop ack for a byte we got from the data register.
 * SHOULD be able to use trinfo, but apparently there is some
 * kind of bug in the WD chip (that I haven't yet figured a
 * way around), if req is already asserted on msg in, and is
 * reasserted as quickly as possible after ack drops.  the
 * trinfo cmd doesn't complete, even when the SBT bit is set.
 * So, the hack is to issue a trinfo cmd, then an abort.
*/
static void
ack_msgin(scsidev_t *dp)
{
	unchar state;

	sccmd_lci(dp, C93TRINFO|0x80);	/* single byte */
	DELAY(1);	/* be sure ack actually gets asserted */
	sccmd_lci(dp, C93ABORT);

	while(getauxstat() & AUX_DBR)
		(void)getdata(); /* flush the fifo on the wd */
	(void)wait_scintr(dp, 2000);	/* should always get intr; after
		the fifo drained, not before... */

	sccmd_lci(dp, C93NACK);	/* negate the ack */
	DELAY(10);	/* give time for phase to change and chip to notice;
		for xtmsgin, may not even get phase change. */
	ifintclear(;) ;
}

/*	do a trinfo phase; if cnt is < 0, it's a transfer in, otherwise
	out.  The dest register should already be set up.
	Assumes used ONLY for msgin and msgout phases.
*/
static int
do_trinfo(scsidev_t *dp, u_char *ibuf, long cnt, uint usecs)
{
	int dirin = cnt < 0;
	register int i;
	u_char state;

	if(dirin)
		cnt = -cnt;

	putcnt(0,0,cnt);
	sccmd_lci(dp, C93TRINFO);

#define TRMSGWAIT 700	/* give them up to 5 ms total for actual'
	bytes to be transferred; delay for phase change from usecs */
	ifintclear(; );
	for(i=0; i<cnt; i++, ibuf++) {
		register int wait;
		/* if we get AUX_INT, phase must have changed; it shouldn't */
		for(wait=0; !((state=getauxstat()) & (AUX_DBR|AUX_INT)) &&
			wait < TRMSGWAIT; wait++)
			DELAY(7);
		if(wait == TRMSGWAIT)
			goto error;
		if(dirin) {
			*ibuf = getdata();
			if(i==0 && cnt != 1 && *ibuf == 7) {
				cnt = 0;	/* flag as reject msg */
				goto error;
			}
		}
		else {
			putdata(*ibuf);
		}
		if(state & AUX_INT)
			goto error;
	}
	/* should always get intr. usually 10 ms is enough even for
	 * slow drives like hitachi, which  takes a LONG
	 * time when the inq cmd is sent from do_inq().  It turns out
	 * that when we are finishing up the last bytes of a non-6 byte
	 * command for the 93, that some devices take a LONG time to
	 * go to the next bus phase.  In particular, the Toshiba CDROM
	 * takes up to ~450 ms from last byte of the c0 'play' cmd to
	 * the next phase (status, in this case).  This is pretty
	 * gross...  It could come up with some 3rd party devices
	 * also, so even though the toshiba firmware will be fixed,
	 * this must remain. */
	(void)wait_scintr(dp, usecs);
	if(getauxstat() & AUX_BSY) {
		wd93reset(dp, 0);
		return 1;	/* cmdabort on return for most callers */
	}
	return 0;

error:
	ifintclear(; );
	if(state == ST_TRANPAUSE) /* abort doesn't work if ack pending */
		sccmd_lci(dp, C93NACK);
	/* no intr from nack, but should go to new bus phase, causing intr */
	i = wait_scintr(dp, 1000);
	if(!dirin && (getstat()&0x7)==7) {
		return -1;	/* if doing msgout and get msgin, caller may want
		to see if msgin is reject */
	}
	ifintclear(; );
	sccmd_lci(dp, C93ABORT);
	while(getauxstat() & AUX_DBR)
		state = getdata(); /* flush the fifo on the wd */
	i = wait_scintr(dp, 1000);	/* should always get intr */
	return cnt ? 1 : 7;
}


/*	send an inquirycmd because in process of setting up sync
	mode, the CDC drives at least want a message; sending an
	abort msg or disconnecting doesn't work.
	An inquiry cmd is chosen because it works even with an
	ATN pending, and won't clear the ATN, so the upper level
	drivers don't lose anything.
*/
static int
do_inquiry(scsidev_t *dp)
{
	static u_char rdy[SC_CLASS0_SZ] = { 0x12 };	/* no data */
	u_char aux, state;
	register int wait;

	if(do_trinfo(dp, rdy, sizeof(rdy), 50000))
		goto error;
	state = getstat();
	if(state != ST_TR_STATIN) {
		if((wait_scintr(dp, 50000) & INTR_ERR) ||
			getstat() != ST_TR_STATIN)
			goto error;
	}

	if(do_trinfo(dp, &aux, -1, 10000))	/* status byte */
		goto error;

	if((state=getstat()) != ST_TR_MSGIN) {
		(void)wait_scintr(dp, 10000);
		if(getstat() != ST_TR_MSGIN)
			goto error;
	}

	if(do_trinfo(dp, &aux, -1, 10000))	/* command complete */
		goto error;

	sccmd_lci(dp, C93NACK);	/* negate the ack */
	state = wait_scintr(dp, 5000);

	if(state != ST_DISCONNECT) {
		while(wd93busy())
			;
		for(wait=0; !(getauxstat()&AUX_INT) && wait<100000; wait++)
			;
		state = getstat();
	}
	sccmd_lci(dp, C93DISC);
	state = wait_scintr(dp, 2000);
	return 0;
error:
	return 1;
}


/*
 * scsi_timeout	- called if SCSI request takes too long to complete
 *
 * If the request is still in progress (the subchannel is still busy)
 * then abort it.  Called at high enough priority to hold off scsi
 * interrupts.
*/
static void
scsi_timeout(register scsisubchan_t *sp)
{
	register scsidev_t *dp;
	int tval;
	char *ms;
	int s;

	dp = &scsidev[sp->s_adap];
	s = splockspl(dp->d_qlock, spl5);
	if(sp->s_flags & S_BUSY) {
		sctimeouts++;
		dp->d_flags |= D_BUSY;
		tval = sp->s_timeoutval;
		if((tval % HZ) == 0)
			ms = "", tval /= HZ;
		else
			ms = "m", tval *= 1000/HZ;
		sp->s_error = SC_CMDTIME;	/* primarily for devscsi */
		scsi_cmdabort(dp, sp, "timeout after %d %ssec", tval,
			      (__psint_t)ms);
	}
	spunlockspl(dp->d_qlock, s);
}


static void
wd93reset(scsidev_t *dp, int noadv)
{
	/* in BSS, so if an 'init' is done, or we return to the PROM, 
	 * we check the sync register again.
	 */
	static int firsttime[SC_MAXADAP];
	u_char state;
	uint wstat;
	extern int scsi_enable_disconnect[];	/* from master.d/scsi */
	u_char wd93freq = WD93FREQVAL;

	if(!noadv)	/* only for standalone freesubchan, if not IP12 */
		wd93freq |= 0x20|8;	/* 8 will enabled advanced features for 93A,
			giving a status of 1 after 93RESET, instead of 0.
			0x20 is RAF (really advanced features) for 93B. */

	while(wd93busy())	/* ensure the reset cmd isn't ignored */
		;
#ifndef EVEREST
	/*	might be syncmode still from the prom or sash, so save
		the info if we were; see allocsubchan and freesubchan
		for actual use */
	if(firsttime[dp->d_adap] == 0) {
		firsttime[dp->d_adap] = 1;
		state = getreg93(WD93SYNC);
		if(state) {
			scsisyncreg[dp->d_adap] = state;
			scsisyncdest[dp->d_adap] = getreg93(WD93DESTID) & (SC_MAXTARG-1);
		}
	}
#endif
	putreg93(WD93ID, wd93freq|scsi_ha_id[dp->d_adap]);

	/* clear possible interrupt to ensure chip accepts the command */
	ifintclear(; );
	putcmd93(C93RESET)
	wstat = wait_scintr(dp, 1000);	/* not too long; some systems have
		more than one scsi chip; others don't */
	if(wstat & INTR_ERR) {
		wstat &= ~INTR_ERR;
		cmn_err(CE_WARN, "sc%d controller didn't reset correctly\n",
			dp->d_adap);
	}
	if(wstat == 1) {	/* A or B */
		if(scsiburstdma)	/* use burst mode DMA */
			dp->d_ctrlreg = 0x2d;
		else
			dp->d_ctrlreg = 0x8d;
		wstat = getreg93(WD93C1);	/* microcode rev on 93 A and B */
		dp->d_ucode = wstat;	/* set it for A also; A has revs 0-A,
			B has (as of 10/91) revs B and C. */
		if(wstat >= 0xB)
			state = D_WD93B;
		else
			state = D_WD93A;
	}
	else {
		dp->d_ctrlreg = 0x8d;
		state = D_WD93;
	}
	dp->d_flags = scuzzy[dp->d_adap].d_initflags | state;

	if(scsi_enable_disconnect[dp->d_adap])
		putreg93(WD93SRCID, 0x80) /* enable reselection */
	else
		putreg93(WD93SRCID, 0x0) /* no disconnects; for devices
		like Sony C501 that support disconnect, but don't work
		correctly (as of 9/89) */
	putreg93(WD93CTRL, dp->d_ctrlreg)
	scsi_seltimeout(dp-scsidev, 0);	/* standard selection timeout */
}


/*	Reset the scsi bus, and cleanup all pending commands.
	There are some cases where an abort message could be sent, but
	not in general.  The few exceptions aren't worth the effort.
	Now prints the messages before dumping scsi info (for debug
	kernels), then do the reset, so it is easier to see what is
	going on in SYSLOG files.
*/
static void
scsi_cmdabort(register scsidev_t *dp, register scsisubchan_t *sp,
	char *msg, __psint_t a1, __psint_t a2)
{
	scaborts++;
	if(sp) {
		cmn_err(CE_CONT, "sc%d,%d,%d: cmd=", dp->d_adap,
			sp->s_target, sp->s_lu);
		if(getenv("diagmode"))
		{
			/* be more verbose; so customer bugs can be diagnosed better */
			int i;
			for(i=0; i<sp->s_len; i++)
				printf("%x ", sp->s_cmd[i]);
			printf( " len=%#x resid=%#x\n\t", sp->s_buf->b_bcount,
				sp->s_buf->b_resid);
		}
		else
			cmn_err(CE_CONT, "%#x ", *sp->s_cmd);
	}
	else
		cmn_err(CE_CONT, "sc%d: ", dp->d_adap);
	cmn_err(CE_CONT, msg, a1, a2);
	cmn_err(CE_CONT, ".  Resetting SCSI bus\n");
#if DEBUG	/* print 'important' registers for better debugging */
	{	u_char state, phase, aux, lun, cmd, srcid, sdata;
   	register uint count_remain;
	register uint count_xferd;
	u_char rhi, rmd;

		cmd = getreg93(WD93CMD);
		lun = getreg93(WD93LUN);
		phase = *dp->d_data;	/* phase follows LUN */
		srcid = getreg93(WD93SRCID);
		state = *dp->d_data;	/* state follows SRCID */
		sdata = getdata();
		aux = getauxstat();
                getcnt(rhi, rmd, count_remain)
                count_remain += ((uint)rmd)<< 8;
                count_remain += ((uint)rhi)<< 16;

		printf("WDregs: cmd=%x aux=%x ph=%x st=%x srcid=%x lun=%x scdata=%x, count_remain=0x%x\n",
			cmd, aux, phase, state, srcid, lun, sdata,count_remain);
	}
#endif	/* DEBUG */
	idbg_scsi(1);	/* stubbed if idbg.o not linked in; allows customers
		to get more info about scsi failures if they want, as well as
		SGI development; should modify someday to dump only the adapter
		we are resetting... */
	scsi_reset(dp->d_adap);	/* toggle scsi bus reset line */
	handle_reset(dp);	/* now clean everything up */
}

/*	set the phase register and start a select & transfer.
	Also release the chip register lock.  This shouldn't fail due
	to a simultaneous re-select, since we should always be connected as
	an initiator when this is called.
*/
static void
scsi_transfer(register scsidev_t *dp, u_char phase)
{
	putreg93(WD93DESTID, dp->d_consubchan->s_dstid)
	putreg93(WD93PHASE, phase)
	putcmd93(C93SELATNTR)
}


/*	start a transfer going, being sure the destination register
	has the correct direction bit set if we are using a 93A.
	Similar to scsi_transfer, but we set the dest
	register, so we use s_target.
	The own ID register has to be set correctly for 93A.  It should
	be 0 for all cases except when the cmd bytes are to be transferred
	(startscsi only ).

	Note that this will sometimes fail because of a pending
	interrupt (or simultaneous reselect).  This should happen only
	when first starting a cmd, since we should be connected as an
	initiator everywhere else.  The cmd is then re-queued at the
	next interrupt (which SHOULD be a re-selection interrupt).
*/
static void
setdest(register scsidev_t *dp, register scsisubchan_t *sp)
{
	if(!sp)
		scsi_cmdabort(dp, sp,
		    "Spurious scsi interrupt, no connected channel", 0, 0);
	putreg93(WD93DESTID, sp->s_dstid)
	putreg93(WD93LUN, sp->s_lu)
	putcmd93(C93SELATNTR)

}



/*	NOTE: late 3.2 hack; on an MP system, which for now only
	has 93, not  93A, forget the 'performance enhancent'
	and 93A bug work-around, and just plow on through programming
	the chip.  Olson, 22 Aug 89
*/

/*
 * startscsi - grab the adapter and start I/O for the desired command.
 */
static void
startscsi(register scsidev_t *dp, register scsisubchan_t *sp)
{
	uint len;
	volatile unchar	*data;
	u_char *cmd;
	int nonly = 0;

	sp->s_flags &= ~S_RESTART;	/* for doscsi() restarts */
re_acq:
	/* Acquire the host adapter.  If unsuccessful, request has been
	 * queued and will be restarted out of scsirelease.
	 */
	if(!scsiacquire(dp, sp, nonly))
		return;
	nonly = 0;

	/* for non-mapped i/o, we only use xferaddr to determine the offset
	 * within the page (always 0 initially) in the scsi_go() routines. */
	sp->s_xferaddr = BP_ISMAPPED(sp->s_buf) ? sp->s_buf->b_dmaaddr : 0;
	cmd = sp->s_cmd;

	if(sp->s_tlen = sp->s_buf->b_bcount) {
		/* if we have to resync, and we are starting a cmd other than
		 * request sense or inquiry, then renegotiate sync, if no
		 * other device is busy.  Also note that we don't re-enable
		 * on request sense or inquiry, primarily because we may
		 * may be recovering from an error caused by a failed
		 * sync negotiation.  This must precede the dma setup,
		 * or we sometimes confuse the WD chip. */
		 if((sp->s_flags & S_RESYNC) && !sp->s_syncs && *cmd != 3 &&
			*cmd != 0x12 && !sc_busycnt(dp)) {
				/*	have to be only busy chan, so 
					if others busy, try later */
				sync_setup(dp, sp, 1, 0); /* re-enable sync */
				sp->s_error = 0;
		}
		if(scsi_dma(dp, sp, 1) <= 0) {
			scsisubrel(dp, sp, 1);
			return;
		}

		/* There is a bug in the 93 part that causes outstanding
		 * REQ's to be lost when in states ST_UNEX_[RS]DATA
		 * which only happens when we program the chip for
		 * a count less than that in the scsi cmd block. The
		 * only problem is that we also need to re-negotiate
		 * for nosync mode for this to work.... This is done
		 * ONLY when we are starting a cmd, and must be done
		 * before any other registers are programmed, since
		 * the process changes a number of registers.
		 * s_syncs is set so we don't get into a state where
		 * we are continually negotiating back and forth
		 * if we are getting a mixture of filesystem (small
		 * transfers) and swapping/benchmark activity.  */
		if((dp->d_flags & D_WD93) && (sp->s_flags & S_SYNC_ON) &&
			sp->s_xferlen != sp->s_buf->b_bcount) {
			if(sc_busycnt(dp)) {
				/* need to turn off DMA on some machines */
				(void)scsidma_flush(sp->s_map,
					sp->s_dstid&SCDMA_IN, 0);
				sc_waitonly(dp);
				nonly = 2;	/* to turn off D_ONLY */
				goto re_acq;
			}
			sync_setup(dp, sp, 0, 0);
			sp->s_syncs = 2;
			sp->s_error = 0;
		}
	}
	else {	/* be sure no cnt left from previous cmd */
		putcnt(0, 0, 0);
		sp->s_xferlen = 0;
	}

	sp->s_flags |= S_BUSY;
	sp->s_timeid = timeout(scsi_timeout, sp, sp->s_timeoutval);

	putreg93(WD93SYNC, sp->s_syncreg)

	/*	this allows sending cmds not in groups 0, 1, or 5 successfully;
	*	if D_WD93, it simply assumes cmds in unrecognized
	*	groups are 6 bytes; if device thinks it's longer, you
	*	get an unexpected cmd phase; at which point you can do
	*	a resume SAT. If and when we get a device that needs
	*	this, we'll think about implementing for the plain 93
	*/
	len = sp->s_len;
	if(dp->d_flags & (D_WD93A|D_WD93B))
		putreg93(WD93ID, len)

	/*	put the cmd bytes into the chip registers. The address
		register auto increments, so we don't have to reload it
		each time. */
	*dp->d_addr = WD93C1;
	data = dp->d_data;
	if(len > 12) /* program cnt reg right, but extra bytes have to */
		len = 12;	/* be handled in same way as on 93 */
	for( ; len>0; len--)
		*data = *cmd++;

	/* Start the I/O operation going with a select-and-transfer command.
	 * The interrupt handler drives the state machine from here.  */
	putreg93(WD93PHASE, PH_NOSELECT)
	dp->d_retry = T93SRETRY;	/* number of times to attempt select */
	sc_wasreset[sp->s_adap] = 0;	/* must handle interrupts.  No reason
		to do the test first, it just slows things down. */
	setdest(dp, sp);
#ifdef EVEREST
        if (sp->s_tlen)
		scsi_go(sp->s_map, sp->s_xferaddr, sp->s_dstid&SCDMA_IN);
#endif
}


/*	Restart a cmd; called when the SAT cmd is ignored because we
	tried to issue it (LCI bit gets set).  We need
	to reschedule the transfer that was just started and service
	the reselection interrupt.  The AUX_LCI bit will be normally be
	set (but may not be if the intr was caused by a reselect.  There
	is some discrepancy between the 93 and 93A documentation here.
	the asynchronous case, just queue it up.
	In the synchronous case, simply wake up the upper half to
	try again. ALWAYS called at interupt time.
	The adapter is NOT released, since the re-selecting channel
	will be processed on return from this routine.
*/
static void
restartcmd(register scsidev_t *dp, register scsisubchan_t *sp)
{
	untimeout( sp->s_timeid );
	/* need to turn off DMA on some machines */
	(void)scsidma_flush(sp->s_map, sp->s_dstid&SCDMA_IN, 0);
	sp->s_flags &= ~S_BUSY;	/* for onlysem stuff */
	if(sp->s_notify) {
		scsisubchan_t *subp;
		/* these are the queueing lines from scsiacquire */
		if(subp = dp->d_waittail) /* put on end of queue. */
			subp->s_link = dp->d_waittail = sp;
		else
			dp->d_waithead = dp->d_waittail = sp;
	}
	else {
		sp->s_flags |= S_RESTART;
		vsema(&sp->s_sem);
	}
}

/*
 * scsiacquire - acquire a the host adapter
 *
 * NOTE: At interrupt time,  this routine may be called to
 * reschedule a request.  This won't deadlock because D_BUSY is set),
 * and we the interrupt handler is never re-entered.
 *
 * If no_nextcmd is set, we are dumping or shutting down, so don't
 * queue or wait.
 */
static int
scsiacquire(register scsidev_t *dp, register scsisubchan_t *sp, int relse)
{
	register scsisubchan_t *subp;

	if(relse == 2)		/* a cmd waiting to be only one */
		dp->d_flags &= ~D_ONLY;

	while(!no_nextcmd && (dp->d_flags & (D_ONLY|D_BUSY))) {
		if(sp->s_notify) {
			if(subp = dp->d_waittail) /* put on end of queue. */
				subp->s_link = dp->d_waittail = sp;
			else
				dp->d_waithead = dp->d_waittail = sp;
			if(relse) { /* for resched from startscsi; do first
				* part of scsirelease().  Has to be done while
				* qlock held, or another cpu could pick up
				* request before scsirelease() could be called,
				* leading to problems later because of a null
				* d_consubchan! */
				dp->d_flags &= ~D_BUSY;
				dp->d_consubchan = (scsisubchan_t *)NULL;
			}
			return(0);
		} else {	/* wait until we can acquire */
			dp->d_sleepcount++;
			svpsema(dp->d_qlock, &dp->d_sleepsem, PSCSI);
			spsema(dp->d_qlock);
		}
	}
	dp->d_flags |= D_BUSY;
	dp->d_consubchan = sp;

	return(1);
}


/*	Release the host adapter, and start next cmd, if any.

	If a request without a notifier is waiting, handle it.
	These are usually short, except that the tpsc driver
	does all requests this way.  The tape device is very
	unlikely to hog the scsi bus this way, since tapes
	are so slow they will likely disconnect (or the process
	using the tape will have to read the disk, or at least wait
	for some kind of input, except when benchmarking).

	If action is >0, check to see if a process is waiting
	to be the only active user of the bus (currently just
	sync setup).
*/
static void
scsirelease(register scsidev_t *dp, int action)
{
	register scsisubchan_t *sp;

	dp->d_flags &= ~D_BUSY;
	dp->d_consubchan = (scsisubchan_t *)NULL;

	/* if dumping, shutting down, we don't want other cmds */
	if(no_nextcmd)
		return;

	if(dp->d_onlycount && action>0 && !sc_busycnt(dp)) {
		dp->d_onlycount--;
		dp->d_flags |= D_ONLY;	/* prevent cmds that might be
			started from scsisubrel from acquiring */
		vsema(&dp->d_onlysem);
	}
	else if(dp->d_sleepcount) {
		dp->d_sleepcount--;
		vsema(&dp->d_sleepsem);
	}
	else if(sp=dp->d_waithead) {
		dp->d_waithead = sp->s_link;
		if (dp->d_waithead == NULL)
			dp->d_waittail = NULL;
		sp->s_link = NULL;
		startscsi(dp, sp);
	}
}

/*
 * cleanup and release subchannel on scsi cmd completion
 * also for convenience release the adapter (always except
 * from handle_reset() ).
 */

static void
scsisubrel(register scsidev_t *dp, register scsisubchan_t *sp,
	int reladap)
{
	if(!(sp->s_flags & S_BUSY)) {
		/* completion intr after a a reset, 'real' notify routine
		 * already called; the scsirelease above still needs to be
		 * done to possibly start next cmd */
		if(reladap)
			scsirelease(dp, 1);
		return;
	}

	untimeout(sp->s_timeid); /* Cancel the timeout */

	/* clear busy, and other connection related bits before *
		scsirelease is called. */
	sp->s_flags &= ~(S_BUSY|S_DISCONNECT|S_SAVEDP|S_RESTART);
	sp->s_link = (scsisubchan_t *)NULL;

	if(reladap)
		scsirelease(dp, 1);

	/* don't retry here, it's not 'safe' for some devices; leave
		it up to the driver. */
	if(sp->s_error == SC_PARITY)
		cmn_err(CE_CONT, "sc%d,%d: SCSI parity error\n",
		    sp->s_adap, sp->s_target);
	if(sp->s_error == SC_MEMERR)
		cmn_err(CE_CONT,
			"sc%d,%d: host memory parity error during DMA\n",
			sp->s_adap, sp->s_target);

	if(sp->s_syncs && !sp->s_status && !sp->s_error)
		sp->s_syncs--;
	if(no_nextcmd != 1) {
		if(sp->s_tlen) { /* not all data transferred; happens
			most often on tapes when reading with larger blk
			size than written with */
			sp->s_buf->b_resid = sp->s_tlen;
		}
		if (sp->s_notify) {
			svsema(dp->d_qlock);
			(*sp->s_notify)(sp);
			spsema(dp->d_qlock);
		}
		else
			vsema(&sp->s_sem);
	}
}

#if DEBUG
static printintr;
#endif

/*	actually do all the work of figuring out what the interrupt
	means and handle it.
*/
static void
handle_intr(register scsidev_t *dp)
{
	u_char state, phase, aux, lun, cmd, srcid;
	register scsisubchan_t *sp;

	/* null if not D_BUSY; probably not correct if a re-select */
	sp = dp->d_consubchan;

	aux = getauxstat();
	if(sp && (sp->s_flags & S_BUSY)) {
		if(aux & AUX_PE)
			sp->s_error = SC_PARITY;
		if(aux & AUX_LCI)	/* the previous SAT cmd failed due to an
			intr.  queue it to restart later.  Should only happen
			when first starting a cmd, since other times we
			issue an SAT, we should already be connected
			as an initiator. */
			restartcmd(dp, sp);
	}

	/*	Read all the registers BEFORE the status register,
		since the chip can re-interrupt after that, possibly
		changing the lun/srcid registers; we also get some
		benefit from the auto register incr.  The count registers
		will be OK since neither we nor the chip will change them.
		NOTE: the XC rev of the 93A has a bug that if an
		unexpected phase change occured during an SAT, the
		incorrect value may be in the phase register,
		so don't count on it too much... */
	cmd = getreg93(WD93CMD);
	lun = getreg93(WD93LUN);
	phase = *dp->d_data;	/* phase follows LUN */
	srcid = getreg93(WD93SRCID);
	state = *dp->d_data;	/* state follows SRCID */

	/* only read if in these states; hopefully it won't change after
		reading the state register; we don't have much choice. */
	if(state == ST_93A_RESEL || state == ST_UNEX_SMESGIN ||
		state == ST_REQ_SMESGIN)
		dp->d_tdata = getdata();
#ifdef DEBUG
	if(printintr) {
		printintr--;
		printf("cmd=%x lun=%x ph=%x srcid=%x st=%x data=%x ",
			cmd, lun, phase, srcid, state, dp->d_tdata);
		if(sp) printf("targ=%d s_fl=%x. ", sp->s_target, sp->s_flags);
	}
#endif /* DEBUG */

	switch(state) {
	case  ST_SATOK: /* Command completed successfully.  */
		/* not really saving the ptrs, just updating xferlen, tlen, and
		 * doing dma stuff; rather than doing it all in line.  */
		save_datap(dp, sp, 0);
		sp->s_status = lun;	/* target status byte */
		scsisubrel(dp, sp, 1); /* Release the subchannel. */
		break;

	case ST_DISCONNECT:
		if (phase == PH_DISCONNECT && sp) {
			/* target is disconnecting; Save the data pointers, but
			 * set the tentative flag, because we don't know if
			 * this transfer is really valid yet.  It may be the
			 * last in a command, but it may also be a precursor
			 * to a restore pointers. */
			save_datap(dp, sp, 1);
			sp->s_flags |= S_DISCONNECT;
			scsirelease(dp, 1);
		}
		else if(cmd != C93DISC || phase != PH_NOSELECT)
			scsi_cmdabort(dp, sp,
			   "illegal disconnection interrupt: phase %x",
			    phase, 0);
		break;

	case ST_RESELECT:	/* 93, or 93A without advanced features;
		SAT ignored due to a reselect; LCI may not be set, due to
		a problem in the chip. */
		if(phase == PH_NOSELECT && sp && (sp->s_flags & S_BUSY) &&
			!(aux & AUX_LCI))	/* else handled above */
			restartcmd(dp, sp);
		/* no LUN till msg in phase, can't set d_consubchan.
		 * Wait for ID message on next interrupt. This occasionally
		 * fails due to an interrupt, but the chip seems to handle
		 * it OK (or perhaps the device is re-trying the selection.
		 * There isn't anything to reschedule if it fails... Don't
		 * want a scsi_transfer here, just update the phase
		 * register.  */
		putreg93(WD93PHASE, PH_RESELECT)
		break;

	case ST_93A_RESEL: /* handle the reselect on 93A; has one less phase
		and interrupt.  Any pending SAT ignored due to a reselect;
		LCI may not be set, due to a problem in the chip.  The 'sp'
		in the test is the CURRENT sp, not the one reselecting. */
		if(!(srcid & 0x8))
			scsi_cmdabort(dp, sp, "reselect without ID", 0, 0);
		else {
			if(phase == PH_NOSELECT && sp &&
				(sp->s_flags & S_BUSY) && !(aux & AUX_LCI))
				restartcmd(dp, sp);	/* else handled above */
			sp = dp->d_subchan[srcid&SCTMSK][dp->d_tdata&SCLMSK];
			sc_continue(dp, sp, PH_IDENTRECV);
		}
		break;

	case ST_SAVEDP:
		/*
		 * Save data pointer requested by target -
		 * he's disconnecting to reselect later.
		 * Save pointers, then wait for next phase.
		 * Note that the channel is still busy, and virtually
		 * connected to the target.
		 */
		save_datap(dp, sp, 0);
		scsi_transfer(dp, PH_SAVEDP);	/* resume to next phase */
		break;

	case ST_REQ_SMESGIN:
		if(phase==PH_RESELECT || (phase==0 && (dp->d_flags&(D_WD93A|D_WD93B)))) {
			/* We received the identify message after reselection.
			 * We can restart the select and xfer at this point. */
			sp = dp->d_subchan[srcid&SCTMSK][dp->d_tdata&SCLMSK];
			sc_continue(dp, sp, PH_RESELECT);
		}
		else {
			if(unex_info(dp, phase, state)) {
				/* not handling some state that we should be,
				 * or a device that isn't following protocol */
				scsi_cmdabort(dp, sp,
					"unexpected message in %x, phase %x", 
					dp->d_tdata, phase);
			}
		}
		break;

	case ST_TIMEOUT:
		if(phase != PH_NOSELECT)
			scsi_cmdabort(dp, sp, "Hardware error", 0, 0);
		else if(--dp->d_retry <= 0) {
			/* Timed out attempting to select the device.
			 * Try the desired number of times before giving up.  */
			sp->s_error = SC_TIMEOUT;
			goto dodisc;
		}
		else /* Restart the select and transfer.  The phase and ID */
			setdest(dp, sp); /* registers are still correct */
		break;

	case ST_UNEXPDISC:
		/* device disconnected unexpectedly.  This happens sometimes
		when a device with removable media (like a floppy) has the
		media removed during i/o */
dodisc:	
		if(sp) /* need to turn off DMA on some machines */
		    (void)scsidma_flush(sp->s_map, sp->s_dstid&SCDMA_IN, 0);
		sccmd_lci(dp, C93DISC);
		scsisubrel(dp, sp, 1);
		break;

	case ST_RESET:
		handle_reset(dp);
		break;
	
	case ST_INCORR_DATA:	/* another 93A bug... */
		scsi_transfer(dp, phase); /* resume SAT */
		break;

	case ST_PARITY:
	case ST_PARITY_ATN:
		if(sp)
			sp->s_error = SC_PARITY;
		/* and continue the phase... Had to be a msg in if ATN,
		 * otherwise a read. IDENTRECV also does a NACK for us; which
		 * is needed, since ACK is left asserted to prevent the
		 * transfer from continuing.  We might want to put some
		 * kind of counter here and abort; on the other hand the
		 * transfer will probably time out,  since if every byte
		 * gets a parity error, it drops to about 1.1Kbytes/sec */
		scsi_transfer(dp, PH_IDENTRECV);
		break;

	case ST_TR_DATAOUT:
	case ST_TR_DATAIN:
	case ST_TR_STATIN:
	case ST_TR_MSGIN:
		/* happens on 93, and 93A if not in advanced mode, when 
		 * a group 2,3,4,6, or 7 cmd is issued with more than 6
		 * cmd bytes, and we transferred the remaining bytes manually.
		 * Simply continue to the next phase.
		 * Have to re-put cnt and phase in case interrupt was
		 * pending at trinfo, and they were ignored at the
		 * PH_CDB_6 intr */
		if(sp) {
			uint len = sp->s_xferlen;
			putcnt((u_char)(len>>16), (u_char)(len>>8), (u_char)len)
			putreg93(WD93PHASE, PH_SAVEDP)
			setdest(dp, sp);
			break;
		}
		/* else fall through */

	default:
		if((state & 0x48) != 0x48 || unex_info(dp, phase, state)) {
			/* we aren't handling some state that we should be... */
			if((state&0xf) == 0xf)
				scsi_cmdabort(dp, sp,
					"Unexpected msg in %x, phase %x",
					dp->d_tdata, phase);
			else
				scsi_cmdabort(dp, sp,
					"Unexpected info phase %x, state %x",
					phase, state);
		}
	}
}


/*
 * reset all software state after a reset (interrupt or internal)
 * This is normally called from scsi_cmdabort() rather than as the
 * result of an interrupt.  If called as direct result of an 
 * interrupt, it was probably from an externally caused reset,
 * such powering on or off a scsi device on the bus.
 *
 * Release all busy subchannels associated with
 * an adapter since we have reset it's SCSI bus.
 * If a target's not busy, it's queued waiting for the
 * adapter so leave it alone.
 * Setup to re-enable sync mode on devices that had it also.
 * If a callback routine tries to do another cmd (from subrel),
 * it will  block or be queued in doscsi() since we haven't yet
 * released the adapter. 
 */
static void
handle_reset(register scsidev_t *dp)
{
	register scsisubchan_t **sp;
	int waits = 0;
	u_char myha = scsi_ha_id[dp->d_adap];

	scresets++;

	wd93reset(dp, 0);	/* reset advanced features, etc */

	if(dp->d_consubchan) /* need to turn off DMA on some machines */
		(void)scsidma_flush(dp->d_consubchan->s_map,
			dp->d_consubchan->s_dstid&SCDMA_IN, 0);

	/*	set flags for 2nd for loop below.  We want to determine
		everything that's active before releasing the channel.
		Otherwise we can have problems because drivers like 
		dksc can start a request for a DIFFERENT target when
		the notify routine is called.  If it happened to start
		a target # we hadn't yet processed, we would set an error
		and release it bogusly! */
	for(sp = &dp->d_subchan[0][0];
		sp <= &dp->d_subchan[SC_MAXTARG-1][SC_MAXLUN-1]; sp++) {
		if(!*sp || myha == (*sp)->s_target)
			continue;
		waits++;
		if((*sp)->s_flags & S_SYNC_ON) {
			(*sp)->s_flags = S_RESYNC|((*sp)->s_flags & ~S_SYNC_ON);
			(*sp)->s_syncs = 10;
			(*sp)->s_syncreg &= 0xf0;
		}
		if((*sp)->s_flags & S_BUSY)
			(*sp)->s_error = (*sp)->s_error ? -(*sp)->s_error :
				-SC_HARDERR;
		/* else doing sync negotiation, or haven't yet started cmd */
	}

	/* wait (a short while) for an interrupt from the adapter and each
		active device. */
	while(waits-- >= 0)
		(void)wait_scintr(dp, 1000);

	/* cleared on next cmd sent, and checked in scsiintr */
	sc_wasreset[dp->d_adap] = 1;

	if(dp->d_onlycount) {	/* if anything waiting on this adapter,
		we had better wake it up! */
		dp->d_onlycount--;
		vsema(&dp->d_onlysem);
	}

	scsirelease(dp, 1);	/* done with the adapter */

	for(sp = &dp->d_subchan[0][0];
		sp <= &dp->d_subchan[SC_MAXTARG-1][SC_MAXLUN-1]; sp++)
		if(*sp && ((*sp)->s_error < 0)) {
			/* was busy, so do callback/vsema */
			(*sp)->s_error = -(*sp)->s_error;
			scsisubrel(dp, (*sp), 0);
		}
}


/* handle target initiated sync negotiations.  First byte
 * handled by ack_msgin.  NOTE: this won't work on the 93,
 * only on the 93A.  The 93 never gives us the interrupt
 * for the unexpected msgin (although it does raise ACK)...
*/
static int
handle_extmsgin(scsidev_t *dp, scsisubchan_t *sp)
{
	u_char msgin[255];
	int i;

	if(!sp)	/* shouldn't be here if not connected! */
		return 1;

	/* WD hasn't yet even raised ack for the msg; this gets the msg
	 * byte acked; need to turn off dma first, with no data transfered.
	 * Ask for 2 bytes, since at a minimum, we get '1',len,1st byte. */
	putcnt(0,0,0);	/* reset counter */
	scsidma_flush(sp->s_map, sp->s_dstid&SCDMA_IN, 0);
	ack_msgin(dp);
	*msgin = 1;
	if(do_trinfo(dp, &msgin[1], -2, 10000))
		return 1;	/* not much we can do... */
	(void)wait_scintr(dp, 1000);	/* should be tranpause intr */
	/* should cause an intr, with state != pause */
	sccmd_lci(dp, C93NACK);
	(void)wait_scintr(dp, 1000);

	if(msgin[1] > 1)
		i = do_trinfo(dp, &msgin[3], -(msgin[1]-1), 10000);

	sccmd_lci(dp, C93ATN); /* go to msg out before last byte nak'ed */
	sccmd_lci(dp, C93NACK);
	i = wait_scintr(dp, 1000);

	if(msgin[2] != 1) {	/* not sync */
		msgin[0] = 7;	/* message reject */
		i = do_trinfo(dp, msgin, 1, 10000);
		cmn_err(CE_WARN,
			"sc%d,%d: Unexpected extended msgin type %x, len %x\n",
			sp->s_adap, sp->s_target, msgin[2], msgin[1]);
		if(i)
			return 1;	/* send reject didn't work */
	}
	else {
		/* accept or reject the sync negotiation, based on
		 * scsi_syncenable.  The target should not go to a cmd
		 * phase in sync_setup, so we should be able to simply
		 * continue the original command afterwards */
		sync_setup(dp, sp,
			scsi_syncenable[sp->s_adap]&(1<<sp->s_target), 1);
		if(sp->s_flags & S_SYNC_ON)
			sp->s_flags |= S_TARGSYNC;
	}

	if(sp->s_tlen)
		/* must reprogram count, and re-program dma */
		i = scsi_dma(dp, sp, 0) <= 0 ? 1 : 0;
	else
		i = 0;
	if(!i) {
		/* reprogram dma, reset phase, etc. */
		sp->s_flags |= S_DISCONNECT;	/* for test in cont */
		sc_continue(dp, sp, PH_IDENTRECV);
	}
	/* else will do cmdabort on return */
	return i;
}


/* we had an unexpected information phase */
static int
unex_info(register scsidev_t *dp, u_char phase, u_char state)
{
	register scsisubchan_t *sp = dp->d_consubchan;
	int i;

	if((state == ST_UNEX_SSTATUS) &&
		(phase >= PH_CDB_6) && (phase <= PH_IDENTRECV)) {
	/*	Data transfer command exited unexpectedly, this happens
		mostly with things like request sense, inquiry, and
		mode sense, where we ask for more data than this
		particular device returns.  In case this was a read/write
		we first set b_resid to the remainder; this is added to
		count_remain in save_datap().  Continue S&T at
		get status phase (by forcing transfer cnt to 0). The next phase
		SHOULD be PH_COMPLETE/SATOK. */
		if(sp && (sp->s_flags & S_BUSY))
			/* update tlen, etc, so we can return resid at SATOK */
			save_datap(dp, sp, 0);
		putcnt(0, 0, 0)	/* so wd accepts new phase */
		scsi_transfer(dp, PH_DATA);
	}
	else if(state == ST_UNEX_SMESGIN) { 
		if((phase == PH_DATA || phase == PH_IDENTRECV) &&
			(dp->d_tdata == 2 || dp->d_tdata == 4)) {
			/* target is disconnecting, continue at next phase
			 * save ptrs and flush dma; note this is an abornmal
			 * case for 93A, normal for 93; usually the PH_SAVEDP
			 * code in handle_intr is executed on a disconnect for
			 * the 93A. */
			if(dp->d_tdata == 2 && sp)
				save_datap(dp, sp, 0);
			else
				scsidma_flush(sp->s_map, sp->s_dstid&SCDMA_IN, 0);
			scsi_transfer(dp, PH_SAVEDP);
		}
		else if(dp->d_tdata == 1)	/* extended msg */
			return handle_extmsgin(dp, sp);
		else if(dp->d_tdata == 3) {
			/*
			 * Restore Pointers; WD hasn't yet raised ack for the
			 * msg; this gets the msg byte acked; need to turn
			 * off dma first, with no data transfered.
			 */
			sp->s_tlen += sp->s_tentative;
			sp->s_xferaddr -= sp->s_tentative;

			(void)scsidma_flush(sp->s_map, sp->s_dstid&SCDMA_IN, 0);
			ack_msgin(dp);
			/*
			 * For some reason, the WD needs longer to see this
			 * transition than for an xtmsgin, so put a longer
			 * delay here.
			 */
			(void)wait_scintr(dp, 200);
			putcnt(0,0,0);

			/* Lifted from sc_continue */
			putreg93(WD93SYNC, sp->s_syncreg)
			if (sp->s_tlen && scsi_dma(dp, sp, 1) <= 0)
				return 0;
			putreg93(WD93PHASE, PH_IDENTRECV)
			setdest(dp, sp);
#ifdef EVEREST
			if (sp->s_tlen)
				scsi_go(sp->s_map, sp->s_xferaddr,
				        sp->s_dstid&SCDMA_IN);
#endif
			return 0;
		}
		else if((dp->d_flags & (D_WD93A|D_WD93B)) && phase != PH_NOSELECT) {
		/* 	jltb5 says could be a parity error and an
			trinfo cmd should be issued to get the data byte;
			will either get the message or a 43 intr indicating
			the parity error; not getting correct state due to
			bug in XC rev of 93A.  Go to phase identrecv so we
			can get the data or msg.  */
			scsi_transfer(dp, PH_IDENTRECV);
		}
		else if(sp && phase == PH_NOSELECT && dp->d_tdata == 0x80 &&
			(sp->s_flags & S_BUSY)) {
			/* See this sometimes on devices with a LONG reselection
			 * sequence, as with Sony CDROM, where from start of arb
			 * to targ+init ID takes ~55 usecs, when they reselect at
			 * the 'same' time we start a new cmd to a different device.
			 * Chip doesn't give us our 'normal' indication in this case. */
			restartcmd(dp, sp);	/* resched 'interrupted' cmd */
			phase = getreg93(WD93SRCID);
			sp = dp->d_subchan[phase&SCTMSK][dp->d_tdata&SCLMSK];
			if(sp && (sp->s_flags & S_DISCONNECT))
				sc_continue(dp, sp, PH_SAVEDP);
			else
				return 1;	/* oops; somethings wrong... */
		}
		else
			return 1;
	}
	else if(state == ST_UNEX_SDATA &&	/* long read */
		(phase == PH_DATA || phase == PH_IDENTRECV) &&
		(sp->s_dstid&SCDMA_IN) ||
		(state == ST_UNEX_RDATA &&	/* long write */
		(phase == PH_DATA || phase == PH_IDENTRECV) &&
		!(sp->s_dstid&SCDMA_IN))) {
		/* Continue a transfer in which we couldn't program the dma map
		 * for a large enough transfer for the requested command.
		 * We are already selected in this case, so we just
		 * continue the transfer.  93A wants IDENTRECV,
		 * 93 wants RESELECT...  */
		save_datap(dp, sp, 0); /* setup data pointers for new xfer */

		if(sp->s_tlen) { /* Continue at the data xfer phase */
			if((i=scsi_dma(dp, sp, 1)) <= 0)
				return i; /* abort on return if not already done */
			if(dp->d_flags & (D_WD93A|D_WD93B)) {
				putreg93(WD93PHASE, PH_IDENTRECV)
			}
			else
				putreg93(WD93PHASE, PH_RESELECT)
			setdest(dp, sp);
#ifdef EVEREST
			if (sp->s_tlen)
				scsi_go(sp->s_map, sp->s_xferaddr,
				        sp->s_dstid&SCDMA_IN);
#endif
			return 0;
		}
		else { /* no more data to transfer, but target still thinks
			so.  Mainly happens due to device firmware bugs.
			This also happens with devscsi programs if the user
			has the cmd format wrong, or when driver writers make a
			mistake.  */
			return 1;
		}
	}
	else if(state == ST_UNEX_CMDPH) {
		if(phase == PH_SELECT) {
			/* apple writer II SC, goes from select to cmdphase;
			 * it doesn't support disconnect/reselect */
			putreg93(WD93PHASE, 0x20)
			setdest(dp, sp);
		}
		else if(phase == PH_CDB_6 && sp && sp->s_len > 6) {
			/* happens on 93, and 93A if not in advanced mode, when 
			 * a group 2,3,4,6, or 7 cmd is issued with more than 6
			 * cmd bytes.  groups 0,1, and 5 are all the chip knows
			 * for sure, others are assumed to be 6 bytes.   So
			 * transfer the next N bytes of the cmd. trinfo trashes
			 * chip cnt, so restore it afterwords.  allow for 'bad' lens
			 * in known groups also, by figuring out just where we are.
			 * We should get an ST_TR* interrupt after this.  Because
			 * some devices might not connect, tell trinfo to wait
			 * up to the entire spec'ed timeout... Toshiba CDROM
			 * has this problem today. */
			uint len = sp->s_xferlen;
			if(do_trinfo(dp, &sp->s_cmd[6],
				sp->s_len-(phase-PH_CDB_START),
				sp->s_timeoutval*(1000000/HZ)))
				return 1;
			putcnt((u_char)(len>>16), (u_char)(len>>8), (u_char)len)
			putreg93(WD93PHASE, PH_SAVEDP)
			setdest(dp, sp);
		}
	}
	else /* This is either the result of bad hardware,
		 * or a programming error here or in the controller. */
		return 1;
	return 0;
}

/*	we are continuing a transfer after being re-selected.  A separate
	routine because the 93A doesn't need the intermediate step
	of resetting the phase register and waiting for another
	interrupt.
*/
static void
sc_continue(scsidev_t *dp, register scsisubchan_t *sp, u_char phase)
{

	if(sp == NULL || !(sp->s_flags & S_DISCONNECT)) {
		/* Didn't expect reselection, since we don't think we're
		 * talking to the device.  Blow off the reselection.  */
		scsi_cmdabort(dp, sp, "unexpected reselection", 0, 0);
		return;
	}
	sp->s_flags &= ~S_DISCONNECT;
	dp->d_consubchan = sp;
	putreg93(WD93SYNC, sp->s_syncreg)

	if(sp->s_tlen && scsi_dma(dp, sp, 0) <= 0)
		return;

	putreg93(WD93PHASE, phase)
	/* Continue at the next phase.  LCI should never be set here,
		since are are already in the reselect phase! */
	setdest(dp, sp);
#ifdef EVEREST
        if (sp->s_tlen)
		scsi_go(sp->s_map, sp->s_xferaddr, sp->s_dstid&SCDMA_IN);
#endif
}

/*
 * save_datap -
 * Remember where we are so we can pick up where we left off when we are
 * reselected.  Also used to flush dma and update tlen and xferlen when cmds
 * done.  The tentative parameter is used to support devices that disconnect
 * without a SAVE DATA POINTERS at the end of the last data transfer phase.
 * If tentative is 1, that means that we are calling from disconnect.
 */
static void
save_datap(register scsidev_t *dp, register scsisubchan_t *sp, int tentative)
{
   	register uint count_remain;
	register uint count_xferd;
	u_char rhi, rmd;

	if(!sp)
		scsi_cmdabort(dp, sp,
		    "Spurious scsi interrupt, no connected channel", 0, 0);
	if(sp->s_flags & S_SAVEDP)
		return;	/* already done */

	sp->s_flags |= S_SAVEDP;
	sp->s_tentative = 0;

	/* Don't bother updating pointers if there wasn't any data transfer. */
	if(sp->s_xferlen) {
		getcnt(rhi, rmd, count_remain)
		count_remain += ((uint)rmd)<< 8;
		count_remain += ((uint)rhi)<< 16;

		/* if count_remain == s_xferlen, then the device disconnected
		   immediately after the command was issued, without doing any
		   data transfer */
		if (count_xferd = sp->s_xferlen - count_remain) {
			sp->s_tlen -= count_xferd;
			sp->s_xferaddr += count_xferd;
			sp->s_xferlen = count_remain;
			if (tentative)
				sp->s_tentative = count_xferd;
		}
	}
	else
		count_xferd = 0;

	/* Flush the DMA channel, and check for parity error; D_MAPRETAINED
	 * count of bytes transfered, when writing. */
	if(scsidma_flush(sp->s_map, sp->s_dstid&SCDMA_IN, count_xferd))
		sp->s_error = SC_MEMERR;	/* dma parity error? */
}


#ifndef EVEREST
/*
 * scsi_int_present - is a scsi interrupt present
 *
 * Can't read the SCSI chip directly in the IP4
 * case since it may interfere with ongoing SCSI DMA.
 */
static int
scsi_int_present(register scsidev_t *dp)
{
	return(getauxstat() & AUX_INT);
}
#endif


/*	Select a device, and wait for the selection interrupt.
	If we don't get the right phases on the interrupts, then
	some other device tried to re-select, or the WD was reset.
	If it was a reselect, we lost the interrupt, but all the
	drives so far will retry several times before giving up
	on a reselect.  Trying to fake the interrupt at this
	point leads to assorted problems (even if we pass the
	info back up and retry at a higher level).
*/
static int
do_select(scsisubchan_t *sp, scsidev_t *dp)
{
	uint iaux;

	putcmd93(C93SELATN)

	if((iaux = wait_scintr(dp, 250000)) & INTR_ERR)
		return sp->s_error = SC_TIMEOUT;
	else if(iaux != ST_SELECT) {
		if(iaux == ST_REQ_SMESGOUT)
			return 0;	/* shouldn't happen... */
		return sp->s_error = SC_HARDERR;
	}
	if((iaux = wait_scintr(dp, 5000)) & INTR_ERR)
		sp->s_error = SC_TIMEOUT;
	if(iaux != ST_REQ_SMESGOUT)
		return sp->s_error = SC_HARDERR;
	return 0;
}


/*	wait until chip interrupts, or 'timer' elapses.  then
	clear the interrupt and return the status.  returns 
	status OR'ed with INTR_ERR on timeouts.
	If the aux status register shows the cmd was ignored,
	it clears the interrupt condition and re-submits the
	command. This is a form of polling for the scsi interrupt.
	if aux register shows the cmd was ignored,
	it clears the interrupt condition and re-submits the
	command.

	Note that we probably allow twice the usecs spec'ed, due to
	instructions that are used, but be generous.
*/
static uint
wait_scintr(scsidev_t *dp, uint usecs)
{
	register int wait;
	u_char state;

	/* wait till WDC isn't busy so registers are accessible */
again:
	for(wait=0; wait<usecs && wd93busy(); wait++)
		DELAY(1);
	if(((state=getauxstat()) & AUX_LCI) && wait < usecs) {
		if(state & AUX_INT) {
			u_char cmd = getreg93(WD93CMD);
			state = getstat();
			putcmd93(cmd)	/* re-issue it */
			goto again;
		}
	}

	usecs /= 8;
	while(wait++<usecs) {
		ifintclear( return (uint)state; )
		DELAY(8);	/* fast polling is a problem for 93 */
	}

	state = getstat();
	return  state | INTR_ERR;	/* timed out waiting */
}


/*	Do dma_map call, and handle case where len returned is 0, meaning that
	either some device disconnected on a non-word boundary, or the initial
	address passed in was not properly aligned.
	If buffer is not mapped, then use the dma_mapbp routine instead.
	Returns 0 on errors, otherwise length mapped.

	If new is 0, we are continuing a transfer that was setup earlier.
	This only matters if D_MAPRETAINED is set, then we increment the map in
	scsidma_flush, rather than re-programming it.  Still need to put the
	cnt in the chip and tell the HPC which maps to use.

	Also note that we need to be sure that we never try to program the
	24 bit counter in the WD chip to > 16 MB-4, so it won't overflow.
	None of the machines dma_map* routines do this today, but to be
	paranoid about future machines, we drop the count here, if needed.
*/
static int
scsi_dma(scsidev_t *dp, scsisubchan_t *sp, int new)
{
	uint len;

	/* if xlen==0, we must have completed the first part of a long
	 * transfer, so we need to re-map.  This is normally handled
	 * in unex_info, but if the device disconnects at just the
	 * 'wrong' point, we can get here with xlen == 0. */
	if(!new && sp->s_xferlen && (dp->d_flags&D_MAPRETAIN)) {
		if(!IS_DMAALIGNED(sp->s_xferaddr))
			/* have to handle disconnects not on 32 bit boundary
			 * here, since we don't remap! */
			len = 0;
		else
			len = sp->s_xferlen;
	}
	else {
		/* don't want mod 16 Mbytes in 24 bit WD count regs!
		 * do it before dma_map*() so that we don't waste mapping registers
		 * and time figuring out addresses that we can't use; 0xC instead of
		 * 0xF so we don't have alignment problems on remap. */
		len = sp->s_tlen > 0xfffffc ? 0xfffffc : sp->s_tlen;
		if(BP_ISMAPPED(sp->s_buf))
			len = dma_map(sp->s_map, sp->s_xferaddr, len);
		else
			len = dma_mapbp(sp->s_map, sp->s_buf, len);
		sp->s_xferlen = len;
	}
	putcnt((u_char)(len>>16), (u_char)(len>>8), (u_char)len)

	if(len == 0) {	/* oops, not aligned right */
		sp->s_error = SC_HARDERR;
		if(sp->s_tlen == sp->s_buf->b_bcount) {
			/* initial address bad.  This can only happen from
			 * startscsi, so no bus reset is required.  (well,
			 * theoretically, it could happen after a disc/reconnect
			 * before any i/o, but the address would have been
			 * caught the first time)  */
			cmn_err(CE_WARN, "sc%d,%d:  I/O address %x not "
				"correctly aligned\n",
				sp->s_adap, sp->s_target, sp->s_xferaddr);
			return -1; /* no abort */
		}
		else
			scsi_cmdabort(dp, sp,
				"disconnected on non-word boundary (addr=%x, 0x%x left)",
				(__psint_t)sp->s_xferaddr, sp->s_xferlen);
	}
	else {
#ifndef EVEREST
		scsi_go(sp->s_map, sp->s_xferaddr, sp->s_dstid&SCDMA_IN);
#endif
		sp->s_flags &= ~S_SAVEDP;	/* need to save at next disc; also
			* causes a flush even if no data transferred for
			* the machines that need it. */
	}
	return len;
}


/* issue a cmd repeatedly if LCI bit is set.  This is mostly used as
 * part of sync negotiations.
*/
static void
sccmd_lci(scsidev_t *dp, unchar cmd)
{
	unchar state;
	int i = 0;

	while(wd93busy())	/* shouldn't be normally */
		;
	do {	/* negate the ack */
		/* on 2nd thru nth clear possible intr */
		if(i++) ifintclear(; );
		putcmd93(cmd)
		while(wd93busy())
			;
	} while(getauxstat() & AUX_LCI);
}

#endif /* !EVEREST  && !IP30 */
