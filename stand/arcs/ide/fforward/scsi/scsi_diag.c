#include "sys/types.h"
#include "sys/param.h"
#include "sys/cpu.h"
#include "sys/sbd.h"
#include "sys/scsi.h"
#include "sys/scsidev.h"
#include "sys/invent.h"
#include "sys/buf.h"
#include "saio.h"
#include "uif.h"
#include "libsc.h"
#include "libsk.h"
#include "scsi.h"

#ident "scsi/scsi_diag.c: $Revision: 1.71 $"

static int do_cmd(void);
extern void scsiinit(void);
extern void scsi_reset(int);
extern void scsi_reset(int);

/* the routine scsi() is the only non-static routine in this file */

/* these must be negative or > 16 to avoid confusion with valid
	device types from the inquiry */
#define	BAD_DEVICE		-1
#define	NO_DEVICE		-2
#define	UNKNOWN_DEVICE		-3


/* no defines for these 3 in invent.h */
#define HARD_DISK 0
#define SCTAPE 1
#define FLOPPY_DISK 0x80	/* pseudo type */

/* sense key definitions */
#define NOT_READY 2	/* drive not ready */
#define ILLEGAL_REQ 5	/* means cmd not supported, or invalid
	* params. For our purposes, we assume the first */
#define UNIT_ATN 6	/* device or bus was reset, etc. */

#define NO_MEDIA_PRESENT 0x3a	/* if addsense is set to this, no point
	in retrying testunityready, because no media is present. for 
	CDROM, WORM, M-O, etc. */

#define	TIME_OUT		(HZ * 60)

static IOBLOCK		iob;
static unsigned char	sense_key, addsense;
static int 		target;
static int		is_removable;	/* last target inq'ed have removable media? */

static buf_t scbuf;	/* for doscsi() */
scsisubchan_t *ide_sc;

static char *scdev = "\rSCSI device ";

/* understands all the cmds used in this file */
static char *
cmdname(unchar cmd)
{
	static char unknown[12];

	switch(cmd) {
	case 0:
		return "Test Unit Ready";
	case 3:
		return "Request Sense";
	case 0x1b:
		return "Start Unit";
	case 0x1d:
		return "Send Diagnostic";
	case 0x3c:
		return "Read Buffer";
	case 0x3b:
		return "Write Buffer";
	default:
		sprintf(unknown, "Command %#x", cmd);
		return unknown;
	}
	/* NOTREACHED */
}

static bool_t 
get_sense(void)
{
	static unsigned char 		request_sense[] = {
		0x03, 0x00, 0x00, 0x00, 0x10, 0x00 	};
	unsigned int *sense_data;
	unchar prevcmd = *ide_sc->s_cmd;

	sense_data = dmabuf_malloc(sizeof(unsigned int) * 4);
	scbuf.b_bcount = sizeof(unsigned int) * 4;
	scbuf.b_dmaaddr = (caddr_t)sense_data;
	scbuf.b_flags = B_READ;
	ide_sc->s_cmd = request_sense;
	ide_sc->s_len = sizeof(request_sense);
	ide_sc->s_timeoutval = 2*HZ;

	if(do_cmd() == FALSE) {
		msg_printf (ERR, "\rCouldn't determine cause of error for %s\n",
			cmdname(prevcmd));
		dmabuf_free(sense_data);
		return FALSE;	/* couldn't get the info */
	}

	sense_key = *((unsigned char *)sense_data + 2) & 0x0f;
	addsense = *((unsigned char *)sense_data + 7)>4 ?
		*((unsigned char *)sense_data + 12) : 0;
	msg_printf (DBG, "%s%d,%d got %s (asense=%x) during %s\n", scdev,
		ide_sc->s_adap, ide_sc->s_target,
	    scsi_key_msgtab[sense_key], addsense, cmdname(prevcmd));

	/* no error, or recovered error are OK */
	dmabuf_free(sense_data);
	return ((sense_key == 0x0) || (sense_key == 0x1)) ? TRUE : FALSE;
}


/* run a command, and do a sense if needed (unless it is a sense cmd!)
 * Do all the error checking and possible reporting.  */
static int
do_cmd(void)
{
	unchar cmd = *ide_sc->s_cmd;

	sense_key = 0;	/* for checking after a FALSE return */

	doscsi(ide_sc);

	if (ide_sc->s_error != SC_GOOD) {
		msg_printf (ERR,
			"%s%d,%d: bad driver status %#x during %s\n",
			  scdev, ide_sc->s_adap, ide_sc->s_target, ide_sc->s_error,
			  cmdname(cmd));
		return (FALSE);
	}

	switch (ide_sc->s_status) {
	case ST_GOOD:
		break;

	case ST_BUSY:	/* treat like unit atn; caller usually retrys,
			* in cases where we are likely to get a busy */
		addsense = 0;
		sense_key = UNIT_ATN;
		return FALSE;

	case ST_CHECK:
		if(cmd == 3) { /* don't do getsense on getsense errors */
			msg_printf (ERR, "%s%d,%d: failure on %s\n", scdev,
			    cmdname(cmd));
			return FALSE;
		}
		else if(get_sense() == FALSE) {
#ifndef PROM
			if(sense_key == ILLEGAL_REQ)
				msg_printf (DBG,
					"%s%d,%d: doesn't support %s\n", scdev,
					ide_sc->s_adap, ide_sc->s_target, cmdname(cmd));
#endif
			/* caller can check sense_key for specific actions */
			return (FALSE);
		}
		break;

	default:
		msg_printf (ERR,
			"%s%d,%d: bad status %#x on %s\n", scdev,
			ide_sc->s_adap, ide_sc->s_target, ide_sc->s_status,
			cmdname(cmd));
		return (FALSE);
	}   /* switch */
	return TRUE;
}


#define	BUFFER_SIZE	0x2004

static bool_t 
scsi_dma(void)
{
	unsigned int			*buffer;
	int				index;
	bool_t				passed = TRUE;
	static unsigned char		read_buffer[] = {
		0x3c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		BUFFER_SIZE >> 8, BUFFER_SIZE & 0x00ff, 0x00 	};
	static unsigned char		write_buffer[] = {
		0x3b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		BUFFER_SIZE >> 8, BUFFER_SIZE & 0x00ff, 0x00 	};

	buffer = dmabuf_malloc(BUFFER_SIZE);
	buffer[0] = 0x00000000;
	for (index = 1; index < (BUFFER_SIZE / 4); index++)
		buffer[index] = KDM_TO_MCPHYS(&buffer[index]);

	scbuf.b_bcount = BUFFER_SIZE;
	scbuf.b_dmaaddr = (caddr_t)buffer;
	scbuf.b_flags = B_WRITE;
	ide_sc->s_cmd = write_buffer;
	ide_sc->s_len = sizeof(write_buffer);
	ide_sc->s_timeoutval = 5*HZ;
	if(do_cmd() == FALSE) {
		/* write buffer not supported, or device saying not ready for
                 * DMA (as some CD-ROM does when no media present) is OK */
		dmabuf_free(buffer);
		return (sense_key == ILLEGAL_REQ || sense_key == NOT_READY)
			? TRUE : FALSE;
	}

	bzero(buffer, BUFFER_SIZE);

	scbuf.b_flags = B_READ;
	ide_sc->s_cmd = read_buffer;
	if(do_cmd() == FALSE) {
		/* read buffer not supported, or device saying not ready for
                 * DMA (as some CD-ROM does when no media present) is OK, 
		 * although strange, if write buffer is... 
		 */
		dmabuf_free(buffer);
		return (sense_key == ILLEGAL_REQ || sense_key == NOT_READY)
			 ? TRUE : FALSE;
	}

	for (index = 1; index < (BUFFER_SIZE / 4); index++)
		if (buffer[index] != KDM_TO_MCPHYS(&buffer[index])) {
			passed = FALSE;
			msg_printf (DBG,
				"Address: 0x%04x, Expected: 0x%08x, "
				"Actual: 0x%08x\n",
				index-1, KDM_TO_MCPHYS(&buffer[index]),
				buffer[index]);
		}

	dmabuf_free(buffer);
	return (passed);
}


static bool_t 
scsi_self_test(void)
{
	static unsigned char 		send_diagnostics[] = {
		0x1d, 0x04, 0x00, 0x00, 0x00, 0x00 };
	int first = 1;
	bool_t ret;

again:
	scbuf.b_bcount = 0;
	scbuf.b_dmaaddr = NULL;
	scbuf.b_flags = B_READ;
	ide_sc->s_cmd = send_diagnostics;
	ide_sc->s_len = sizeof(send_diagnostics);
	ide_sc->s_timeoutval = 70*HZ;	/* some devices take a long time */
	if((ret=do_cmd()) == FALSE && first && sense_key == UNIT_ATN) {
		/* unit atn, do it again */
		first = 0;
		goto again;	/* sense overwrite struct, reset it */
	}
	return (ret==FALSE && sense_key==ILLEGAL_REQ)
		? TRUE : ret;
}


static int
probe_scsi_device(void)
{
	char *inquiry_data, *msg;

	inquiry_data = (char *)scsi_inq((int)iob.Controller,(int)iob.Unit,0);
	if(inquiry_data == NULL) {	/* fatal errors reported already */
		extern int sc_inq_stat;
		if(sc_inq_stat == SC_TIMEOUT)
			return  NO_DEVICE;
		else
			return BAD_DEVICE;
	}

	is_removable = inquiry_data[1] & 0x80;

	switch (inquiry_data[0]) {
	case HARD_DISK:	/* no define for this in invent.h */
		if (is_removable) {
			/* might actually be an M-O or CD-ROM */
			inquiry_data[0] = FLOPPY_DISK;
			msg = "a removable disk drive";
		}
		else 
			msg = "a hard disk drive";
		break;
	case SCTAPE:
		msg = "a tape drive";
		break;
	case INV_PRINTER:
		msg = "a printer";
		break;
	case INV_CPU:
		msg = "a CPU";
		break;
	case INV_WORM:
		msg = "a WORM";
		break;
	case INV_CDROM:
		msg = "a CDROM";
		break;
	case INV_SCANNER:
		msg = "a scanner";
		break;
	case INV_OPTICAL:
		msg = "an optical drive";
		break;
	case INV_CHANGER:
		msg = "a changer";
		break;
	case INV_COMM:
		msg = "a comm device";
		break;
	default:
		msg_printf (VRB, "\rDevice %d is unknown type %d, %s\n",
		    target, inquiry_data[0], &inquiry_data[8]);
		return (UNKNOWN_DEVICE);
	}

	msg_printf (VRB, "Device %d is %s, %s\n", iob.Unit, msg,
	    &inquiry_data[8]);
	return  inquiry_data[0];
}


static bool_t
scsi_device_ok(void)
{
	bool_t				passed = TRUE;

	/* Toshiba CD-ROM drives fail send diag when media not present... */
	if(!scsi_self_test() && (addsense != NO_MEDIA_PRESENT || !is_removable)) {
		passed = FALSE;
		msg_printf (SUM, "\rDevice %d failed self diagnostics test\n", target);
	}

	if (!scsi_dma ()) {
		passed = FALSE;
		msg_printf (SUM, "\rDevice %d failed DMA test\n", target);
	}

	return (passed);
}   /* scsi_device_ok */


static bool_t
scsi_unit_ready(void)
{
	static unsigned char test_unit_ready[6];	/* all zeros */
	int first = 1;
	bool_t ret;

again:
	scbuf.b_bcount = 0;
	scbuf.b_dmaaddr = NULL;
	scbuf.b_flags = B_READ;
	ide_sc->s_cmd = test_unit_ready;
	ide_sc->s_len = sizeof(test_unit_ready);
	/* turns out that when a tstrdy is done after the startunit, the
	 * st41200 disconnects, and takes over 2 seconds to reconnect.
	 * the 380 HH (94241) takes up to 5 seconds.  strange, but true...;
	 * give it 4 seconds.  It won't hurt on drives that are reasonable...
	 * It appears that they are returning early from the startunit, and
	 * making up the difference if a tstrdy follows. */
	ide_sc->s_timeoutval = 10*HZ;
	
	if((ret=do_cmd()) == FALSE && first && sense_key == UNIT_ATN) {
		/* unit atn, do it again */
		first = 0;
		goto again;	/* sense overwrote struct, reset it */
	}
	return ret;
}

/*	Make sure device is ready, by issuing a startunit command, and
 *  then repeatedly issuing testunitready until it becomes ready.
 *	The repeated tstrdy is mainly in case someone forgot to
 *	set the motor start jumper/switch 'correctly', in which case
 *	most drives won't accept commands while they are spinning up.
 *  The timeout is total time in this routine, including the startunit,
 *  because the timeouts for testrdy have been bumped so much.
*/
static int
scsi_startunit(void)
{
	bool_t				unit_ready;
	static unsigned char 		start_unit[] = {
		0x1b, 0x00, 0x00, 0x00, 0x01, 0x00 };
	uint timeo_time;

	timeo_time = (uint)get_tod() + 70;	/* 70 seconds */

	scbuf.b_bcount = 0;
	scbuf.b_dmaaddr = NULL;
	scbuf.b_flags = B_READ;
	ide_sc->s_cmd = start_unit;
	ide_sc->s_len = sizeof(start_unit);
	ide_sc->s_timeoutval = 60*HZ;	/* 20-40 is typical */
	if(do_cmd() == FALSE) {
		if(sense_key != ILLEGAL_REQ) {
			msg_printf (ERR, "%s%d,%d fails start cmd\n", scdev,
				ide_sc->s_adap, ide_sc->s_target);
			return FALSE;	/* supports start, but it failed!? */
		}
		/* else assume that startunit isn't supported, and wait for
		 * it to become ready */
	}

	while((unit_ready=scsi_unit_ready())==FALSE && sense_key==NOT_READY
		&& addsense != NO_MEDIA_PRESENT && (uint)get_tod() < timeo_time)
		DELAY (500000);
	if(unit_ready == FALSE)
		msg_printf (ERR, "%s%d,%d is not ready for use\n", scdev,
			ide_sc->s_adap, ide_sc->s_target);
	return unit_ready;
}

#define NOTFOUNDTRIES 10

#if IP22 || IP26 || IP28
int ctrlr1ok;		/* for retry */
#endif

static int
_1scsi(int ctrlr)
{
	extern scsidev_t *scsidev;
	int					bad_scsi_devices = 0;
	int					device_type;
	bool_t 				no_dev_present = TRUE;
	char *cp, *getenv(const char *);
	int first = NOTFOUNDTRIES;	/* try for up to 5 seconds; what
		else are we going to do anyway; if no devices, we
		aren't going to be doing much booting... */

	msg_printf (VRB, "\rSCSI controller %d /devices test\n", ctrlr);

#ifndef PCI2GIO /* scsidev not used with PCI to GIO backplane */

	if ((__psint_t)scsidev[ctrlr].d_map == -1)
		return(1);
#endif
	if (cp = getenv ("scsiretries"))
	    atob (cp, &first);
	if (first < 0 || first > NOTFOUNDTRIES)
	    first = NOTFOUNDTRIES;
	iob.Controller = ctrlr;

	scsi_seltimeout(ctrlr, 1);	/* shorten selection timeout */

again:
	/* go from 0, to hide knowledge of host-adapter id; let
		driver worry about that */
	for (target = 0; target < SC_MAXTARG; target++) {
		extern u_char scsi_ha_id[];
		if(target == scsi_ha_id[iob.Controller])
			continue;	/* ignore the host adapter. */
		iob.Unit = target;

		/* at some time, for some devices, errors were encountered
			if testready was done before an inquiry... */
		device_type = probe_scsi_device ();
		if (device_type == BAD_DEVICE) {
			no_dev_present = FALSE;
			bad_scsi_devices |= 1 << target;
			continue;
		}
		else if (device_type == NO_DEVICE)
			continue;

		no_dev_present = FALSE;

		ide_sc = allocsubchannel((int)iob.Controller, target, 0);
		if(ide_sc == NULL)	/* should never happen */
			continue;
		ide_sc->s_buf = &scbuf;
		ide_sc->s_notify = NULL;	/* no callbacks */

		/* clear possible UNIT ATTENTION */
		(void)scsi_unit_ready();

		if(device_type == HARD_DISK && scsi_startunit() == FALSE) {
			/*
			 * *NOT* an error in the diags sense if drives with
			 * removable media don't have the media present!
			 */
			if(addsense == NO_MEDIA_PRESENT && is_removable)
				msg_printf(ERR,
					"\rNo media present in sc%d,%d, not ready\n", ctrlr, target);
				else
					bad_scsi_devices |= 1 << target;
		}

		{
#ifdef PROM	/* only run for diagmode v; much faster boot without
	it for most devices. */
		char *diagmode, diagchar;
		diagmode = getenv("diagmode");
		diagchar = diagmode ? *diagmode : 0;
		if(diagchar == 'v' && !scsi_device_ok())
#else 	/* test is always done for IDE */
		if(!scsi_device_ok())
#endif /* PROM */
			bad_scsi_devices |= 1 << target;
		}

		freesubchannel(ide_sc);
		ide_sc = 0;
	}

	if (no_dev_present) {
		/* If we aren't diskless, and we don't find any devices,
		 * then something is wrong, and we want to be sure that
		 * the user sees the message and that we don't try to
		 * autoboot.  Otherwise they may simply see the
		 * 'Press ESC' message when they look at the screen.
		 * Ideally, we should check to be sure we saw at least one
		 * disk device, but this is sufficient to catch cabling
		 * problems, etc. 
		 * This is also seen with slow devices like the
		 * CDC 94161 155 Mb drive.  So try again. if ctlr 0.
		 * The very first time, reset the bus in case device
		 * is hung up.  This is sometimes seen when a ^c is
		 * done in ide. */
		char *diskless = getenv("diskless");
#if IP22 || IP26 || IP28
		char *diagmode = getenv("diagmode");
		char diagchar = diagmode ? *diagmode : 0;


		if((!diskless || (*diskless == '0')) && (diagchar == 'v')) {
			if ((first && ctrlr == 0) && (!ctrlr1ok)) {
#endif
#if IP20
		if(!diskless || (*diskless == '0')) {
			if(first && ctrlr == 0) {
#endif
				if(first == NOTFOUNDTRIES){
					printf("Reset SCSI controller %d and retry\n",ctrlr);
					scsi_reset(ctrlr);
				}
				DELAY(500000);	/* wait a bit */
				first--;
				goto again;
			}
			bad_scsi_devices |= 0x100;
		}
		msg_printf (VRB, "\rNo SCSI %d device available\n", ctrlr);
	}
#if IP20
	else if (!bad_scsi_devices)
		okydoky();
#endif

	scsi_seltimeout(ctrlr, 0);      /* standard selection timeout */
	return(bad_scsi_devices);
}

int _scsi_diag_fail;	/* for emfail */

/* this is the only externally callable routine.  Note that the field
 * script assumes a return value of 1 indicates a chip or cabling failure
 * (i.e., no devices found), otherwise it reports drive problems by bit
 * position.  If we ever change the host adapter ID to be other than 0,
 * the assumption that low bit set means missing or not-working SCSI chip
 * will have to be revisited.
 * Power on diags only care about zero/non-zero.
*/
int
scsi(void)
{
	int ret0, ret1, c1;

	if(ide_sc) {
		/* this is done to handle case where we had the chan
		 * allocated, and got interrupted.  (for ide) */
		freesubchannel(ide_sc);
		ide_sc = 0;
	}

	scsiinit();

#if IP22 || IP26 || IP28
	/* We have 2 scsi cntrls on Indigo2, 1 on Indy */
	if (scsicnt == 2) {
		ret1 = _1scsi(1);
	}
	else
		ret1 = 0;

	if (ret1&0x100) ctrlr1ok=0;
	else ctrlr1ok=1;
#endif

	ret0 = _1scsi(0);

#if IP20
#if 0
	/* THIS CAUSES PROM ESS TEST TO HANG */
	/* only look for second scsi if the 2nd HPC is there (it won't b
	 * now, but we may have something similar later.
	 */
	ret1 = (c1=wbadaddr(PHYS_TO_K1(SCSI1_CTRL_ADDR),sizeof(int))) ?
		0 : _1scsi(1);
#else
	c1 = 0;
	ret1 = 0;
#endif
#endif

#if IP22 || IP26 || IP28
	c1=(ret0 & (~0x100)) + ((ret1 & (~0x100))<<8); 
#endif

	/* controller 0 should always be there, although there might not
	 * be any devices on it.  If controller one doesn't have any
	 * devices, but controller 0 does, that is probably OK.  Complain
	 * (by returning non-zero) if _1scsi says that no devices exist
	 * on either and diskless not defined. */
#if IP22 || IP26 || IP28
	if((ret0&0x100) && (ret1&0x100)) {
#elif IP20
	if ((!c1 && (ret0&0x100)) || ((ret0&0x100) && (ret1&0x100))) {
#endif
		/* no devices found and not diskless.  Probably
		* a chip problem, a cabling problem, or one of the
		* attached devices isn't powered up and doesn't tri-state
		* the bus signals when not powered.  Also seen with slow
		* devices like the CDC 94161 155 Mb drive.  So try again.
		*/
		return _scsi_diag_fail = 1<<16;	/* unique for better emfail message */
	}
#if IP22 || IP26 || IP28
	else if (!c1) okydoky();

	return _scsi_diag_fail = (ret0 & (~0x100)) + ((ret1 & (~0x100))<<8); 
#endif
#if IP20
	return _scsi_diag_fail = ret0 + ((ret1 & (~0x100))<<8);
#endif
}
