/*
 * Driver for the Adaptec 7800 family of SCSI adapters (sometimes
 * refered to as controllers.)
 *
 * This standalone version is derived from adp78.c,v 1.9 1995/09/07 from the
 * banyan tree.
 *
 * $Id: adp78.c,v 1.3 1997/12/20 02:30:32 philw Exp $
 */

#include <bstring.h>
#include <sys/param.h>
#include <sys/cpu.h>
#include <sys/sbd.h>
#include <sys/buf.h>
#include <sys/debug.h>
#include <sys/immu.h>
#include <sys/pfdat.h>
#include <sys/types.h>
#include <sys/edt.h>
#include <sys/scsi.h>
#include <sys/dvh.h>
#include <sys/newscsi.h>
#include <sys/mace.h>
#include <arcs/hinv.h>
#include <libsk.h>
#include <libsc.h>
#include <pci_intf.h>
#include <pci/PCI_defs.h>
#include "adphim/osm.h"
#include "adphim/him_scb.h"
#include "adphim/him_equ.h"
#include "adphim/seq_off.h"
#include "sys/adp78.h"
#ifdef IP32
#include "sys/IP32.h"
#ifdef MHSIM
#include "../../../IP32/include/DBCsim.h"
#endif
#endif

extern int cachewrback;

#ifdef MHSIM
/* the simuator does not like phys addresses. */
#define kvtophys(a)	((DWORD) a)
#else
#define kvtophys(a)	(KDM_TO_PHYS(a))
#endif

/*
 * PCI infrastructure interface functions.
 */
int adp78attach(struct pci_record *, struct edt *, COMPONENT *);
int adp78detach(struct pci_record *, struct edt *, int);
int adp78error(struct pci_record *, struct edt *, int);


/*
 * Internal functions
 */
static int	adp_poll_status(adp78_adap_t *);
static int	get_slot_number(u_int addr);
static int	get_adapter_number(int);
static int	init_adapter(adp78_adap_t *);
static void	init_pci_cfg_regs(char *, uint);
static int	init_cfp_struct(adp78_adap_t *, cfp_struct *);
static void	init_cfp_struct_user(int, cfp_struct *);
static void	init_scb(sp_struct *, cfp_struct *, scsisubchan_t *, SG_ELEMENT *, int);
static void	fill_scb(sp_struct *, scsisubchan_t *);
static int	fill_sglist(sp_struct *scbp, scsisubchan_t *sp);
static int	adp_dma_map(SG_ELEMENT *sgp, caddr_t addr, uint remain_bytes, uint view);
static int	convert_hastatus(UBYTE);
#ifdef LE_DISK
static void	spin_up(u_char, u_char, u_char);
static void	set_soft_swizzle(u_char, u_char, u_char);
#endif
static void	dump_hastat(UBYTE hastatus);
static void	dump_cdb(u_char *cdb, int len);


int adp_verbose=0;
UBYTE adp_curr_scb_num=0;


#ifdef LE_DISK
/*
 * Adaptec is a little endien chip, so no byte swapping (native view)
 * produces a little endien disk.  Assume a little endien disk initially
 * (soft_swizzle=1).  If set_soft_swizzle sees a big endien root disk
 * it will clear soft_swizzle.
 */
int soft_swizzle=1;
int root_adapter=0;
int root_device=1;
#endif

adp78_adap_t *adp78_array[ADP78_NUMADAPTERS];	/* array of pointers to adapters */
int adp78_failed_array[ADP78_NUMADAPTERS]={0,0,0,0,0,0,0,0,0,0};
int scsi_ha_id[ADP78_NUMADAPTERS]= {0,0,0,0,0,0,0,0,0,0};   /* adapter scsi id's - for scsi_common.c */

/*
 * No wide in the prom.  3 means - allow disconnect and enable sync
 * negotiation
 */
u_char adp_device_options = 0x3;

/*
 * PCI Master Latency Timer for each of the adapters.  Values must be in
 * multiples of 4.
 */
u_char adp_master_latency[ADP78_NUMADAPTERS] = {0x10, 0x10, 0x10, 0x10,
					      0x10, 0x10, 0x10, 0x10,
					      0x10, 0x10};



/*
 * Parity checking enabled (1) or disabled (0) for each of the adapters.
 */
u_char adp_parity[ADP78_NUMADAPTERS] = {1, 1, 1, 1, 1,
					1, 1, 1, 1, 1};



/*
 * Selection timeout for each adapter.
 * This is the length of time in milliseconds the host adapter will wait
 * for a target to respond to selection before aborting the selection
 * procedure.  Valid values are:
 * 
 *	0x0	256 milliseconds
 *	0x1	128 milliseconds
 *	0x2	 64 milliseconds
 *	0x3	 32 milliseconds
 *
 * (Is this really implemented? XXX)
 */
u_char adp_select_timeout[ADP78_NUMADAPTERS] = {0, 0, 0, 0, 0,
						0, 0, 0, 0, 0};


/*
 * From wd93:
 *
        If non-zero, the minimum number of seconds for any SCSI timeout
	(i.e., timeouts shorter than this value will be increased to
	this value.  If zero, whatever timeout the various upper level
	drivers specified is used.  This may be useful when disconnects
	are enabled, but some devices hold the bus for a very long
	period, and devices that had started commands are therefore not
	able to reconnect to complete;  that is, it may be an
	alternative to changing wd93_enable_disconnect[] below.
*/
uint adp78_mintimeout = 0;

pci_record_t rec1, rec2;

/*
 * Called from runstandcfg.  runstandcfg calls install functions from the
 * list generated from IPxxconf.cf  Just register the driver with the PCI
 * infrastructure.  The infr. will scan the PCI bus and call back to this
 * driver via adp78attach for each Adaptec chip found.
 */
adp78_earlyinit(COMPONENT *parent)
{
	int i;
	pci_record_t *recp;
	char *which;
	pci_ident_t adp_id;

	if (adp_verbose)
		PRINT("adp78_earlyinit: registering myself with PCI inf.\n");

	/*
	 * Allocate and fill in a pci_record for the pci infrastr.
	 * Motherboard 7880 chips will have device id 8078
	 * PCI card 7880 chips will have device id 8178
	 */
	for (i=0; i < 2; i++) {
		if (i == 0) {
			adp_id.device_id = 0x8078;
			which = "integral";
			recp = &rec1;
		} else {
			adp_id.device_id = 0x8178;
			which = "optional PCI";
			recp = &rec2;
		}

		adp_id.vendor_id = 0x9004;
		adp_id.revision_id = REV_MATCH_ANY;

		recp->pci_attach = adp78attach;
		recp->pci_detach = adp78detach;
		recp->pci_error = adp78error;
		recp->pci_id = adp_id;
		recp->pci_flags = 0;
		recp->io_footprint = 0;
		recp->mem_footprint = 0;
		if (pci_register(recp)) {
			printf("adp78_earlyinit: pci_register for %s scsi failed\n",
			       which);
			return -1;
		}
	}

	bzero(adp78_array, sizeof(adp78_adap_t *) * SC_MAXADAP);

	return 0;
}


/*
 * ==============================================================================
 * PCI Infrastructure Interface Functions.
 * ==============================================================================
 */

/*
 * One Adaptec chip has been found.  Initialize the data structures needed
 * to support it.  Make various calls to let the system know that we are here.
 *
 * Returns 0 if everything O.K., -1 otherwise.
 */
int
adp78attach(struct pci_record *recp, struct edt *e, COMPONENT *pci_node)
{
	extern void scsiinstall_adapter(struct component *, char *, int);
	adp78_adap_t *adp;
	int adap, targ, lun, slot;
	COMPONENT *adapter_node;

	if (adp_verbose)
		PRINT("adp78attach: e_base3 (config) 0x%x e_base (device regs) 0x%x\n",
		       e->e_base3, e->e_base);

	/* the adapter/controller number is a function of the slot number */
	slot = get_slot_number((u_int) e->e_base3);
	adap = get_adapter_number(slot);

	if (adp_verbose)
		PRINT("adaptec found at slot %d; assigning controller number %d\n",
		       slot, adap);

	if (adp78_array[adap] != NULL) {
		PRINT("adp78attach: adap %d already attached\n", adap);
		return -1;
	}

	adp = malloc(sizeof(adp78_adap_t));
	ASSERT(adp);
	bzero(adp, sizeof(adp78_adap_t));

	/*
	 * Initialize adapter variables.
	 * The PCI infrastructure is supposed to program the base addr into
	 * the chip.  It does not do that yet. (10/18/95).
	 */
	adp->ad_ctlrid = adap;
	adp->ad_pcibusnum = 0;
	adp->ad_pcidevnum = slot;
	adp->ad_baseaddr = (char *) PHYS_TO_K1(e->e_base + PCI_LOW_MEMORY); 
	adp->ad_cfgaddr = (char *) e->e_base3;
	adp->ad_rev = pci_get_cfg(adp->ad_cfgaddr, PCI_CFG_REV_ID);

	if (recp->pci_id.device_id == 0x8178)
		adp->ad_flags |= ADF_OPTIONAL_PCI;

	if (init_adapter(adp)) {
		free(adp);
		PRINT("adp78edtinit: init_adapter failed");
		adp78_failed_array[adap] = 1;
		return -1;
	}

	adp->ad_scsiid = adp->ad_himcfg->Cf_ScsiId;

	/*
	 * Everything is OK now.  Add the adapter structure to the array
	 * scsiinstall will add the adapter and all the devices
	 * on this scsi bus to the config. tree.
	 */
	adp78_array[adap] = adp;

#ifdef LE_DISK
	/*
	 * On Moosehead, check the volume header of the disk at
	 * adapter 0 target 1 and see if it is a big endien
	 * or little endien disk.  Set soft_swizzle accordingly.
	 */
	if (adp->ad_ctlrid == root_adapter) {
		printf("doing set_soft_swizzle for ctlr %d dev %d\n",
		       root_adapter, root_device);
		set_soft_swizzle(adap, root_device, 0);
	}
#endif

	scsiinstall_adapter(pci_node, "ADP7880", adap);
	return 0;
}

/*
 * This shouldn't even happen in standalone.
 */
int
adp78detach(struct pci_record *recp, struct edt *ep, int something)
{
	PRINT("adp78detach: don't know how to detach\n");
	return -1;
}

int
adp78error(struct pci_record *recp, struct edt *ep, int something)
{
	PRINT("adp78error\n");
	return -1;
}



/*
 * ==============================================================================
 * Various stuff needed by the rest of the prom (dksc.c tpsc.c scsi_common.c)
 * Prototypes are in scsi.h and libsk.h
 * ==============================================================================
 */

scsisubchan_t *
allocsubchannel(int adap, int targ, int lun)
{
	adp78_adap_t *adp;
	scsisubchan_t *sp;
	sp_struct *scbp;

	if ((adap >= SC_MAXADAP)  || (targ >= SC_MAXTARG) || (lun >= SC_MAXLUN)) {
		PRINT("allocsubchannel: adap %d targ %d lun %d out of range\n",
		       adap, targ, lun);
		return NULL;
	}

	adp = adp78_array[adap];
	if (adp == NULL) {
		PRINT("allocsubchannel: bad adapter number %d\n", adap);
		return NULL;
	}

	if ((adp->ad_subchan[targ][lun] != NULL) ||
	    (adp->ad_scb[targ][lun] != NULL)) {
		PRINT("allocsubchannel: adapter %d targ %d lun %d is busy\n",
		       adap, targ, lun);		
		return NULL;
	}

	sp = malloc(sizeof(scsisubchan_t));
	ASSERT(sp);
	bzero(sp, sizeof(scsisubchan_t));
	adp->ad_subchan[targ][lun] = sp;

	scbp = malloc(sizeof(sp_struct));
	ASSERT(scbp);
	bzero(scbp, sizeof(sp_struct));
	init_scb(scbp, adp->ad_himcfg, sp, adp->ad_sg, targ);
	adp->ad_scb[targ][lun] = scbp;

	sp->s_adap = adap;
	sp->s_target = targ;
	sp->s_lu = lun;

	/* check to see if the device is in sync mode.  If so, 
	 * sp->s_flags |= S_SYNC_ON;
	 */

	return(sp);
}

/*
 * Deallocate the scsisubchan structure from the adapter structure.
 */
void
freesubchannel(scsisubchan_t *sp)
{
	adp78_adap_t *adp;

	if (sp == NULL) {
		PRINT("freesubchannel: got null sp\n");
		return;
	}

	adp = adp78_array[sp->s_adap];
	if ((adp == NULL) ||
	    (adp->ad_subchan[sp->s_target][sp->s_lu] == NULL) ||
	    (adp->ad_scb[sp->s_target][sp->s_lu] == NULL)) {
		PRINT("freesubchannel: bad free adap %d targ %d\n",
		       sp->s_adap, sp->s_target);
		return;
	}

	/* again, alot of concern about setting the sync mode just right.
	 * I'll have to ask Dave about this.  XXX
	 */
	
	/* free semaphores and maps, which I didn't allocate */

	free(adp->ad_subchan[sp->s_target][sp->s_lu]);
	free(adp->ad_scb[sp->s_target][sp->s_lu]);
	adp->ad_subchan[sp->s_target][sp->s_lu] = NULL;
	adp->ad_scb[sp->s_target][sp->s_lu] = NULL;
}

int do_scsi_count=0;

/*
 * Make a scb which contains the required scsi info from scsisubchan_t.  Send
 * the scb to the HIM.  Since we are in prom/standalone, issue 1 command,
 * poll for a "completed" status, and then return.
 */
void
doscsi(scsisubchan_t *sp)
{
	adp78_adap_t *adp;
	sp_struct *scbp;

	adp = adp78_array[sp->s_adap];
	if (adp == NULL) {
		PRINT("doscsi: bad adapter number %d\n", sp->s_adap);
		return;
	}

#ifdef LE_DISK
	/*
	 * Always swizzle mode select (0x15) data.  Hmm, I swizzle the data
	 * buffer passed to me (seems dangerous) and I don't swizzle
	 * the data back, I probably should.
	 */
	if ((soft_swizzle) && (sp->s_cmd[0] == 0x15)) {
		int rounded_len = ((sp->s_buf->b_bcount + 3) / 4) * 4;
		if (sp->s_buf->b_flags & SRF_MAPBP) {
			PRINT("got a mapbp req. flag 0x%x",
			       sp->s_buf->b_flags);
			ASSERT(0);
		}
		
		if (osm_swizzle(sp->s_buf->b_dmaaddr, rounded_len))
			PRINT("adp78_command: unable to swizzle data");
		DCACHE_WBINVAL(sp->s_buf->b_dmaaddr, rounded_len);
	}

	/*
	 * If we are doing software swizzle, save the original
	 * CDB, and use the extra ha buffer for the swizzled CDB.
	 */
	if (soft_swizzle) {
		int rlen = ((sp->s_len+3)/4)*4;
		unsigned char *tmp;
		tmp = sp->s_cmd;
		sp->s_cmd = adp->ad_ebuf;
		adp->ad_ebuf = tmp;
		bcopy(tmp, sp->s_cmd, sp->s_len);
		osm_swizzle(sp->s_cmd, rlen);
	}
#endif

	scbp = adp->ad_scb[sp->s_target][sp->s_lu];
	if (scbp->Sp_OSspecific != sp)
		PRINT("scb is corrupted.  is 0x%x should be 0x%x\n",
		      scbp->Sp_OSspecific, sp);

	fill_scb(scbp, sp);
	adp->ad_currcmd = sp;
	adp->ad_timeoutval = sp->s_timeoutval / HZ;
	if (adp->ad_timeoutval < ADP_MIN_TIMEOUT)
		adp->ad_timeoutval = ADP_MIN_TIMEOUT;

	adp_curr_scb_num = scbp->Sp_scb_num;

	PH_ScbSend(scbp);
	if (adp_poll_status(adp)) {
		sp->s_error = SC_HARDERR;
		sp->s_status = ST_GOOD;
	} else {
		/* PH_ScbCompleted has set the fields */
	};

	/* clear the status fields of the scb */
	scbp->SP_Stat = 0;
	scbp->SP_Cmd = 0;
	scbp->SP_MgrStat = 0;
	bzero(scbp->Sp_CDB, MAX_CDB_LEN);

	adp->ad_currcmd = NULL;
	adp->ad_timeoutval = 0;
}


/*
 * Reset the given controller
 */
void
scsi_reset(int adap)
{
	if (adp78_array[adap] != NULL)
		Ph_HardReset(adp78_array[adap]->ad_himcfg, 1);
}

/*
 * Returns 1 if the controller is present
 */
int
scsi_controller_present(int adap)
{
	if ((adp78_array[adap] != NULL) || (adp78_failed_array[adap] == 1))
		return 1;
	else
		return 0;
}

/*
 * Returns 1 if the controller is present but failed initialization
 */
int
scsi_controller_failed(int adap)
{
	if (adp78_failed_array[adap] == 1)
		return 1;
	else
		return 0;
}


/* A NOTE FROM MIKE ABOUT scsi_setsyncmode() AND scsi_seltimeout():
 * I needed to have these entry points present in my driver because other
 * parts of the prom calls them.  Their use is for slight performance 
 * improvements during kernel loading.  They were low on my list of
 * priorities, and as you can see, I never got back to them.
 *
 * ...You can fill in setsyncmode if you want.  Its a bit tricky,
 * there is a lot of little endien bit mask stuff you have to do.
 *
 * I would not touch seltimeout because some of the newer drives
 * (IBM 4GB, I think) need a long time to respond to a select.  So if you
 * set a shorter selection timeout, the prom will not wait long enough for
 * the disk to respond.
 */

/*
 * Enable synchronous transfers for this target if enable == 1.
 * Otherwise, disable synchronous transfers.
 */
void
scsi_setsyncmode(scsisubchan_t *sp, int enable)
{
#if 0
	adp78_adap_t *adp;

	adp = adp78_array[sp->s_adap];
	if (adp == NULL) {
		printf("scsi_setsyncmode: bad adap %d\n", sp->s_adap);
		return;
	}

	if (adp_verbose)
		if (enable)
			PRINT("scsi_setsyncmode: enabling syncmode for (%d,%d)\n",
			       sp->s_adap, sp->s_target);
		else
			PRINT("scsi_setsyncmode: disabling syncmode for (%d,%d)\n",
			       sp->s_adap, sp->s_target);

	if (enable)
		adp->ad_himcfg->Cf_AllowSyncMask |= (1 << sp->s_target);
	else
		adp->ad_himcfg->Cf_AllowSyncMask &= ~(1 << sp->s_target);

	/* tell the sequencer that this target needs to be renegotiated */
	Ph_SetNeedNego(sp->s_target, adp->ad_himcfg->CFP_Base);
#endif
}


/*
 * Shorten the selection timeout for this adapter if short_timeout == 1.
 * Otherwise, lengthen selection timeout.
 */
void
scsi_seltimeout(u_char adap, int short_timeout)
{
#if 0
	adp78_adap_t *adp;

	/*
	 * According to the SCSI spec, the seltimeout should be
	 * 250ms.  By default, the Adaptec timeout is 256ms.
	 */

	adp = adp78_array[adap];
	if (adp == NULL) {
		printf("scsi_seltimeout: bad adap %d\n", adap);
		return;
	}

	if (short_timeout) {
		/*set to short timeout */
		PH_Set_Selto(adp->ad_himcfg, STIMESEL0);
	} else {
		/* set to normal timeout */
		PH_Set_Selto(adp->ad_himcfg, 0);
	}
#endif
}


/*
 * Do an inquiry and return an array of info.  This array should be part
 * of a structure or something, since it is never freed by the caller.
 * scsiinstall in scsi_common.c needs this.
 */
u_char *
scsi_inq(u_char adap, u_char targ, u_char lun)
{
	adp78_adap_t *adp;
	scsisubchan_t *sp;
	char *inq_buf;
	buf_t buf;
	int tries;
	static char scsi_inquiry_cmd[6] = {0x12, 0, 0, 0, SCSI_INQUIRY_LEN, 0};

	adp = adp78_array[adap];
	if (adp == NULL) {
		PRINT("scsi_inq: bad adapter number %d\n", adap);
		return NULL;
	}

	if ((inq_buf = adp->ad_inq[targ][lun]) == NULL) {
		inq_buf = malloc(SCSI_INQUIRY_LEN);
		adp->ad_inq[targ][lun] = inq_buf;
	}
	bzero(inq_buf, SCSI_INQUIRY_LEN);

	/*
	 * Allocate the subchannel, this has the effect of making sure that
	 * a scsisubchan_t and scb has been allocated.  Fill in the buf_t
	 * and scsi command.  Send it out.
	 */
	sp = allocsubchannel(adap, targ, lun);

	bzero(&buf, sizeof(buf_t));
	buf.b_dmaaddr = (caddr_t) inq_buf;
	buf.b_bcount = SCSI_INQUIRY_LEN;
	buf.b_flags = B_BUSY|B_READ;

	scsi_inquiry_cmd[1] = lun << 5;

	sp->s_cmd = scsi_inquiry_cmd;
	sp->s_len = sizeof(scsi_inquiry_cmd);
	sp->s_buf = &buf;
	sp->s_notify = NULL;
	sp->s_timeoutval = 10 * HZ;
	sp->s_caller = NULL;

	tries = 1;
	do {
		doscsi(sp);
		tries++;
	} while ((tries < 2) && (sp->s_status == ST_CHECK));

	if (sp->s_error || sp->s_status) {
		if (adp_verbose)
			PRINT("scsi_inq: error 0x%x status 0x%x\n",
			      sp->s_error, sp->s_status);
		freesubchannel(sp);
		return NULL;
	}

	freesubchannel(sp);
	return inq_buf;
}

/*
 * Send out a modeselect to set a 512 block size on the CDROM.
 * This is needed to do software installs from CDROM at the prom level.
 */
void
adp_cdrom_modeselect(int adap, int targ, int lun)
{
	adp78_adap_t *adp;
	scsisubchan_t *sp;
	char *modesel_buf;
	buf_t buf;
	int tries;
	static char scsi_modesel_cmd[6] = {0x15, 0x10, 0, 0, 0xc, 0};

	adp = adp78_array[adap];
	if (adp == NULL) {
		PRINT("scsi_inq: bad adapter number %d\n", adap);
		return;
	}

	modesel_buf = malloc(SCSI_MODESEL_PARAMLIST_LEN);
	bzero(modesel_buf, SCSI_MODESEL_PARAMLIST_LEN);
	modesel_buf[3] = 8;
	modesel_buf[10] = 2;
	DCACHE_WBINVAL(modesel_buf, SCSI_MODESEL_PARAMLIST_LEN);
	

	/*
	 * Allocate the subchannel, this has the effect of making sure that
	 * a scsisubchan_t and scb has been allocated.  Fill in the buf_t
	 * and scsi command.  Send it out.
	 */
	sp = allocsubchannel(adap, targ, lun);

	bzero(&buf, sizeof(buf_t));
	buf.b_dmaaddr = (caddr_t) modesel_buf;
	buf.b_bcount = SCSI_MODESEL_PARAMLIST_LEN;
	buf.b_flags = B_BUSY;

	sp->s_cmd = scsi_modesel_cmd;
	sp->s_len = 6;
	sp->s_buf = &buf;
	sp->s_notify = NULL;
	sp->s_timeoutval = 10 * HZ;
	sp->s_caller = NULL;

	tries = 1;
	do {
		doscsi(sp);
		tries++;
	} while ((tries < 3) && (sp->s_status == ST_CHECK));

	if (sp->s_error || sp->s_status) {
		if (adp_verbose)
			PRINT("scsi_modesel: error 0x%x status 0x%x\n",
			      sp->s_error, sp->s_status);
		freesubchannel(sp);
		return;
	}

	freesubchannel(sp);
	free(modesel_buf);
}


#ifdef LE_DISK

#define SCSI_READBUF_LEN	512

/*
 * Tell the disk to spin up.
 */
static void
spin_up(u_char adap, u_char targ, u_char lun)
{
	scsisubchan_t *sp;
	char *read_buf, *sense_buf;
	static char scsi_spinup_cmd[6] = {0x1B, 1, 0, 0, 1, 0};
	buf_t buf;

	sp = allocsubchannel(adap, targ, lun);

	bzero(&buf, sizeof(buf_t));
	buf.b_dmaaddr = 0;
	buf.b_bcount = 0;
	buf.b_flags = B_BUSY;

	scsi_spinup_cmd[1] = lun << 5;

	sp->s_cmd = scsi_spinup_cmd;
	sp->s_len = sizeof(scsi_spinup_cmd);
	sp->s_buf = &buf;
	sp->s_notify = NULL;
	sp->s_timeoutval = 60 * HZ;
	sp->s_caller = NULL;

	doscsi(sp);

	freesubchannel(sp);
}

/*
 * Do a read from block 0 to see if the disk is big endien or little
 * endien.  The the soft_swizzle bit accordingly.
 */
static void
set_soft_swizzle(u_char adap, u_char targ, u_char lun)
{
	scsisubchan_t *sp;
	char *read_buf;
	int tries=4;
	struct volume_header *vhp;
	buf_t buf;
	static char scsi_read_cmd[] = {0x28, 0, 0, 0, 0,
					  0, 0, 0, 1, 0};

	/* first spin up will always fail with check condition */
	spin_up(adap, targ, lun);
	delay(HZ);
	spin_up(adap, targ, lun);

	sp = allocsubchannel(adap, targ, lun);

	/*
	 * Get a read buffer for 1 block (512 bytes).
	 */
	read_buf = malloc(SCSI_READBUF_LEN);
	bzero(read_buf, SCSI_READBUF_LEN);
	DCACHE_INVAL(read_buf, SCSI_READBUF_LEN);

	bzero(&buf, sizeof(buf_t));
	buf.b_dmaaddr = (caddr_t) read_buf;
	buf.b_bcount = SCSI_READBUF_LEN;
	buf.b_flags = B_BUSY;

	scsi_read_cmd[1] = lun << 5;

	sp->s_cmd = scsi_read_cmd;
	sp->s_len = sizeof(scsi_read_cmd);
	sp->s_buf = &buf;
	sp->s_notify = NULL;
	sp->s_timeoutval = 30 * HZ;
	sp->s_caller = NULL;

read_again:

	doscsi(sp);

	if(sp->s_error == 0 && sp->s_status == 0) {
		vhp = (struct volume_header *) read_buf;
		if (vhp->vh_magic == VHMAGIC) {
			printf("native read got VH magic number match.  Little endien disk.  soft_swizzle remains 1\n");
			soft_swizzle = 1;
		} else {
			printf("native read does not match magic number (0x%x).  Blank or Big Endien disk.  Clear soft_swizzle\n");
			soft_swizzle = 0;
		}

	} else if (tries-- &&
		   sp->s_error != SC_TIMEOUT &&
		   sp->s_error != SC_CMDTIME) {
		if (tries <= 1)
			us_delay(30 * 1000000);   /* might be spinning up */
		else
			us_delay(1 * 1000000);
			
		goto read_again;
	}

	free(read_buf);
	freesubchannel(sp);
}

#endif /* LE_DISK */

/*
 * =============================================================================
 * Internal functions.
 * =============================================================================
 */


/*
 * Since there are no interrupts in standalone mode, I must check for
 * the completion of a command, or some status which requires the HIM
 * to do stuff, by polling the INTSTAT register.
 *
 * This function will return a 0 if the command was completed successfully.
 * -1 if not.
 */

/* PCI errors which we really care about */

#define PCI_BAD_ERRORS	(PERR_MASTER_ABORT | \
			 PERR_TARGET_ABORT | \
			 PERR_DATA_PARITY_ERR | \
			 PERR_RETRY_ERR | \
			 PERR_ILLEGAL_CMD | \
			 PERR_SYSTEM_ERR | \
			 PERR_PARITY_ERR)

int
adp_poll_status(adp78_adap_t *ad_p)
{
	unsigned char rval, intstat;
	unsigned long count=0;
	uint pci_err_stat, pci_err_addr;
	uint status;
	volatile char *h, hv;

	if (adp_verbose)
		PRINT("polling for command complete\n");
	
	while (1) {
		Ph_Pause(ad_p->ad_himcfg->CFP_Base);
		intstat = Ph_ReadIntstat(ad_p->ad_himcfg->CFP_Base);

		if (intstat & CMDCMPLT) {
			us_delay(POLL_WAIT*3); /* wait for DMA */
			rval = PH_IntHandler(ad_p->ad_himcfg);
		} else if (intstat & ANYPAUSE) {
			rval = PH_IntHandler_abn(ad_p->ad_himcfg, intstat);
		} else {
			rval = 0;
		}

		Ph_UnPause(ad_p->ad_himcfg->CFP_Base);

		/*
		 * If the command is completed, then break,
		 * If the command had a scsi int, meaning selection
		 * timeout, unexpected bus free, parity error, then
		 * break.
		 * otherwise keep polling.
		 */
		if ((rval == CMDCMPLT) || (rval == SCSIINT))
			break;

		pci_err_stat = * (uint *) (PHYS_TO_K1(PCI_ERROR_FLAGS));
		if (pci_err_stat & PCI_BAD_ERRORS) {
			pci_err_addr = * (uint *) (PHYS_TO_K1(PCI_ERROR_ADDR));
			printf("PCI error during SCSI command on controller %d count %d\n",
			       ad_p->ad_ctlrid, count);
			printf("pci_err_stat 0x%x pci_error_addr 0x%x\n",
			       pci_err_stat, pci_err_addr);
			*(uint *) (PHYS_TO_K1(PCI_ERROR_FLAGS)) = 0;
			status = pci_get_cfg(ad_p->ad_cfgaddr, PCI_CFG_STATUS);
			printf("Adaptec PCI status 0x%x\n", status);
			printf("Attempt to clear error\n");
			pci_put_cfg(ad_p->ad_cfgaddr, PCI_CFG_STATUS, 0xe900);
			h = (char *) ((int) ad_p->ad_baseaddr + CLRINT);
			*h = 0x1f;
			status = pci_get_cfg(ad_p->ad_cfgaddr, PCI_CFG_STATUS);
			printf("Adaptec PCI status 0x%x\n", status);
			return -1;
		}
		
		us_delay(POLL_WAIT);
		count++;

		if (count > (US_IN_SEC / POLL_WAIT) * ad_p->ad_timeoutval) {
			printf("SCSI command timed out on (%d, %d).\n",
			       ad_p->ad_ctlrid, ad_p->ad_currcmd->s_target);
			dump_cdb(ad_p->ad_currcmd->s_cmd, ad_p->ad_currcmd->s_len);
			printf("count %d ad_timeoutval %d intstat 0x%x\n",
			       count, ad_p->ad_timeoutval, intstat);
			return -1;
		}
	}

	if (adp_verbose)
		PRINT("finished polling\n");

	return 0;
}

/*
 * This will be called from PH_IntHandler if it finds out that a
 * scb is done.
 */
void
PH_ScbCompleted(sp_struct *scb_p)
{
	scsisubchan_t *sp;
	adp78_adap_t *ad_p;
	int rcode;

	sp = scb_p->Sp_OSspecific;
	ASSERT(sp);

	if (adp_verbose)
		PRINT("PH_ScbCompleted: ctrl %d target %d\n",
		       sp->s_adap, sp->s_target);

	ad_p = adp78_array[sp->s_adap];
	ASSERT(ad_p);

	switch(scb_p->SP_Stat) {
	case SCB_PENDING:
		PRINT("ScbCompleted: SP_Stat SCB_PENDING");
		return;

	case SCB_COMP	:
		sp->s_error = SC_GOOD;
		sp->s_status = ST_GOOD;
		break;

	case SCB_ABORTED:
		sp->s_error = SC_HARDERR;
		PRINT("scb command aborted ctrl %d target %d\n",
		       sp->s_adap, sp->s_target);

	case SCB_ERR:
		sp->s_error= convert_hastatus(scb_p->SP_HaStat);
		sp->s_status = ST_GOOD;

		if (scb_p->SP_TargStat == UNIT_CHECK) {
			sp->s_error = SC_GOOD;
			sp->s_status = ST_CHECK;
			break;
		}

		/*
		 * For the Dec Tape drive which returns the BUSY status.
		 */
		if ((sp->s_error == SC_HARDERR) &&
		    (scb_p->SP_TargStat == ST_BUSY)) {
 			sp->s_error = SC_GOOD;
 			sp->s_status = ST_BUSY;
 			break;
		}

		if ((sp->s_error == SC_HARDERR) &&
		    (scb_p->SP_HaStat == HOST_DU_DO)) {
			
			if ((scb_p->Sp_CDB[0] == 0x28 || scb_p->Sp_CDB[0] == 8) &&
			    (ad_p->ad_sg[0].data_len & SG_LAST_ELEMENT) &&
			    (ad_p->ad_sg[0].data_len & ~SG_LAST_ELEMENT) < 512) {
				sp->s_error = SC_GOOD;
				sp->s_status = ST_GOOD;
				break;
			}

			printf("SCSI overflow or underflow on (%d,%d).",
			       sp->s_adap, sp->s_target);
			break;
		}

		if (sp->s_error == SC_HARDERR) {
			printf("SCSI Hard Error on (%d,%d) ",
			       sp->s_adap, sp->s_target);
			dump_hastat(scb_p->SP_HaStat);
			PRINT("\n");
#ifdef LE_DISK
			if (soft_swizzle) {
				PRINT("swizzled cdb (len %d): ", sp->s_len);
				dump_cdb(sp->s_cmd, ((sp->s_len+3)/4)*4);
				PRINT("original cdb         : ");
				dump_cdb(ad_p->ad_ebuf, sp->s_len);
			}
			else
#endif
			{
				PRINT("cdb: ");
				dump_cdb(sp->s_cmd, sp->s_len);
			}

		}

		break;

	case INV_SCB_CMD:
		sp->s_error = SC_HARDERR;
		sp->s_status = ST_GOOD;
		PRINT("PH_ScbCompleted: SP_Stat INV_SCB_CMD");
		break;

	default:
		sp->s_error = SC_HARDERR;
		sp->s_status = ST_GOOD;
		PRINT("PH_ScbCompleted: unknown code 0x%x\n",
		       scb_p->SP_Stat);
		return;
	}

#ifdef LE_DISK
	/*
	 * For initial bringup, I will not be using the swizzle hardware
	 * on Mace.  So the disk will be little endien, i.e. all data that goes
	 * to and from the disk media will not be swizzled.  But I do need
	 * to swizzle data that goes to and from the drive firmware.
	 */

	/*
	 * swap back the original CDB
	 *
	 * swizzle drive firmware data:
	 * inquiry (0x12) read capacity (0x25) and  mode sense (0x1a).
	 * Swizzle back mode select (0x15)
	 */
	if (soft_swizzle) {
		unsigned char *tmp;

		tmp = sp->s_cmd;
		sp->s_cmd = ad_p->ad_ebuf;
		ad_p->ad_ebuf = tmp;

		if ((sp->s_cmd[0] == 0x12 ||
		     sp->s_cmd[0] == 0x15 ||
		     sp->s_cmd[0] == 0x25 ||
		     sp->s_cmd[0] == 0x1a)) { 
			if (!BP_ISMAPPED(sp->s_buf)) {
				PRINT("got a mapbp req. flags 0x%x can't handle",
				       sp->s_buf->b_flags);
				ASSERT(0);
			}

			if (osm_swizzle(sp->s_buf->b_dmaaddr,
					((sp->s_buf->b_bcount+3)/4)*4))
				PRINT("PH_ScbCompleted: unable to swizzle data");
		}
	}
#endif  /* LE_DISK */

	if (sp->s_notify)
		(*sp->s_notify)(sp);
}



/*
 * Return the PCI slot number based on the config address passed back
 * by the PCI infrastructure.
 * If this function is useful to other drivers, it should be moved to
 * pci_intf.c
 */
static int
get_slot_number(u_int addr)
{
#ifdef SIM_PCICFG
	extern char *pci_config_space;
	if (pci_config_space == (char *) addr)
		addr = ((1 << 31) | (1 << CFG1_DEVICE_SHIFT));
	else
		addr = ((1 << 31) | (2 << CFG1_DEVICE_SHIFT));
#endif

	/* decode the slot id from the address. */
	addr &= CFG1_ADDR_DEVICE_MASK;
	addr = addr >> CFG1_DEVICE_SHIFT;
	return addr;
}

/*
 * Return the scsi controller number based on the PCI slot number.
 * There is a one-to-one mapping between the PCI slot number and
 * the device number.
 */
static int
get_adapter_number(int slot)
{
	/*
	 * I assume that on Moosehead and Roadrunner, the pci devices
	 * are numbered 0 for bridge, 1 and 2 for the Adaptec
	 * scsi controllers.
	 */
	return (slot-1);
}



/*
 * Initialze the adapter structure.  Allocate scb's, adp78_ha_t's, and the
 * HIM cfp_struct and hsp_structs.  Chain them all up appropriately.
 *
 * Returns 0 if all O.K. -1 on error.
 */
static int
init_adapter(adp78_adap_t *adp)
{
	int initerror;
	cfp_struct *cfgp;
	uint real_base = KDM_TO_PHYS(adp->ad_baseaddr) - PCI_LOW_MEMORY;
	volatile char *h, hv;
	uint pci_err_stat, pci_err_addr;
	int count=0, status;

	init_pci_cfg_regs(adp->ad_cfgaddr, real_base);

	/* reset myself */
	h = (char *) ((int) adp->ad_baseaddr + HCNTRL);
	*h = 0x1;
	us_delay(POLL_WAIT * 10);

	/* turn reset off - put it in pause state */
	*h = 4;
	us_delay(POLL_WAIT * 10);

	h = (char *) ((int) adp->ad_baseaddr + HCNTRL);
	while (count < 5) {
		hv = *h;
		if (hv & 0x1) {
			printf("reset not turned off hv=0x%x\n", hv);
			us_delay(POLL_WAIT);
		} else {
			break;
		}
		count++;
	}

	h = (char *) ((int) adp->ad_baseaddr + CLRINT);
	*h = 0x1f;

	pci_put_cfg(adp->ad_cfgaddr, PCI_CFG_STATUS, 0xe900);

	status = pci_get_cfg(adp->ad_cfgaddr, PCI_CFG_STATUS);
	h = (char *) ((int)adp->ad_baseaddr + INTSTAT);

	/*
	 * Clear errors.
	 */
	* (uint *) (PHYS_TO_K1(PCI_ERROR_FLAGS)) = 0;
	* (uint *) (PHYS_TO_K1(PCI_ERROR_ADDR)) = 0;


	cfgp = malloc(sizeof(cfp_struct));
	bzero(cfgp, sizeof(cfp_struct));

	initerror = init_cfp_struct(adp, cfgp);
	if (initerror) {
		free(cfgp);
		return(initerror);
	}

	adp->ad_ebuf = malloc(MAX_CDB_LEN);

	/*
	 * Simplify the sg list logic by just using a big one.
	 */
	adp->ad_sg = malloc(NUMSG_BIG * sizeof(SG_ELEMENT));
	bzero(adp->ad_sg, (NUMSG_BIG * sizeof(SG_ELEMENT)));

	adp->ad_himcfg = cfgp;

	/*
	 * One last check before returning
	 */
	pci_err_stat = * (uint *) (PHYS_TO_K1(PCI_ERROR_FLAGS));
	pci_err_addr = * (uint *) (PHYS_TO_K1(PCI_ERROR_ADDR));

	* (uint *) (PHYS_TO_K1(PCI_ERROR_FLAGS)) = 0;
	* (uint *) (PHYS_TO_K1(PCI_ERROR_ADDR)) = 0;

	return 0;
}


/*
 * Set up the PCI memory map and write the configuration registers.
 * The base register has been programmed by pci_intf.c
 */
static void
init_pci_cfg_regs(char *cfg_addr, uint mapped_addr)
{
	uint base_addr;

	mapped_addr = mapped_addr | PCI_IO;

	pci_put_cfg(cfg_addr, PCI_CFG_STATUS, 0xe900);

	pci_put_cfg(cfg_addr, PCI_CFG_COMMAND,
		    (PCI_CMD_PAR_ERR_RESP |
		     PCI_CMD_BUS_MASTER |
		     PCI_CMD_MEM_SPACE));

	/* set up the cache line and latency timer */
	pci_put_cfg(cfg_addr, PCI_CFG_CACHE_LINE, 0x20);
	pci_put_cfg(cfg_addr, PCI_CFG_LATENCY_TIMER, 0x10);
}


/*
 * Initialize the cfp struct for the adaptec.  This structure is used by
 * the HIM.  Fill it in with some info. here, then call the HIM function
 * PH_GetConfig to do the rest.
 */
static int
init_cfp_struct(adp78_adap_t *adp, cfp_struct *cfgp)
{
	int adap, targ, initerror, i;
	u_int val;
	hsp_struct *hspp;
	unsigned short disconn_map=0;

	adap = adp->ad_ctlrid;

	ASSERT(adap < ADP78_NUMADAPTERS);

	/* initialize number of scbs before calling PH_GetConfig */
	cfgp->Cf_BusNumber = adp->ad_pcibusnum;
	cfgp->Cf_DeviceNumber = adp->ad_pcidevnum;
	cfgp->Cf_MaxNonTagScbs = 2;
	cfgp->Cf_NumberScbs = ADP78_NUMSCBS;
	cfgp->Cf_MaxTagScbs = ADP78_NUMSCBS;
	cfgp->CFP_SuppressNego = 0;
	cfgp->CFP_Base = (struct aic7870_reg *) adp->ad_baseaddr;
	cfgp->Cf_AdapterId = pci_get_cfg(adp->ad_cfgaddr, PCI_CFG_DEVICE_ID);
	/*
	 * No fast 20 in the prom!  No wide in the prom.
	 */
	cfgp->CFP_EnableFast20 = 0;
	cfgp->Cf_MaxTargets = 16;
	cfgp->Cf_AllowSyncMask = 0xffff;


	cfgp->Cf_IrqChannel = 0;	/* not used in irix */
	cfgp->CFP_InitNeeded = 1;
	cfgp->CFP_ResetBus = 1;
	cfgp->CFP_DriverIdle = 1;
	cfgp->CFP_BiosActive = 0;
	cfgp->Cf_AccessMode = 2;
	cfgp->CFP_MultiTaskLun = 1;
	cfgp->CFP_ConfigFlags |= RESET_BUS + SCSI_PARITY;
	cfgp->CFP_TerminationLow = 1; 
	cfgp->CFP_TerminationHigh = 1;

	init_cfp_struct_user(adap, cfgp);

	hspp = malloc(sizeof(hsp_struct));
	bzero(hspp, sizeof(hsp_struct));
	cfgp->CFP_HaDataPtr = hspp;
	cfgp->Cf_HaDataPhy = kvtophys(hspp);

	initerror = PH_init_him(cfgp);

	if (adp_verbose)
		printf("init_cfp_struct: completed\n");

	return initerror;
}


static void
init_cfp_struct_user(int adap, cfp_struct *cfgp)
{
	int targ, i;
	unsigned short disconn_map=0;
	char *cp;
	unsigned int sync_enmask;
	char *shared_disk_string;
	int shared_disk_num;

	/*
	 * Look for the scsihostid and scsi_syncenable variable.
	 */
	if((cp = getenv("scsihostid")) != NULL) {
		cfgp->Cf_ScsiId = atoi(cp);
		if (cfgp->Cf_ScsiId != 0) {
			printf("bad scsihostid?? %d.  Set to default of 0\n",
			       cfgp->Cf_ScsiId);
			cfgp->Cf_ScsiId = 0;
		}
		for (i=0; i < ADP78_NUMADAPTERS; i++)
			scsi_ha_id[i] = cfgp->Cf_ScsiId;
	} else {
		cfgp->Cf_ScsiId = scsi_ha_id[adap];
	}

	if (adap == 1) {
		shared_disk_string = getenv("shared_disk_hack");
		if (shared_disk_string != NULL) {
			printf("shared_disk_hack = %s\n",
			       shared_disk_string);
			shared_disk_num = atoi(shared_disk_string);
			if (shared_disk_num) {
				printf("setting adap %d scsiid to 7\n",
				       adap);
				cfgp->Cf_ScsiId = 7;
			}
		}
	}


	if ((cp = getenv("scsi_syncenable")) != NULL) {
		atobu(cp, &sync_enmask);
	} else {
		sync_enmask = 0;
	}

	cfgp->Cf_BusRelease = adp_master_latency[adap];
	cfgp->CFP_ScsiParity = adp_parity[adap];
	cfgp->CFP_ScsiTimeOut = adp_select_timeout[adap];

	for(targ = 0; targ < cfgp->Cf_MaxTargets; targ++) {
		if (adp_device_options & ADP_ALLOW_DISCONNECT) {
			disconn_map |= 1 << targ;
		}
		if (sync_enmask & (1 << targ)) {
			cfgp->Cf_AllowSyncMask |= (1 << targ);
			cfgp->Cf_ScsiOption[targ] =
				adp_device_options & HIM_OPTIONS;
		} else {
			cfgp->Cf_AllowSyncMask &= (~(1 << targ));
			cfgp->Cf_ScsiOption[targ] = 0;
		}

	}

	cfgp->CFP_AllowDscnt = disconn_map;
}


static void
init_scb(sp_struct *scb_p, cfp_struct *cfg_p, scsisubchan_t *sp, SG_ELEMENT *sgp, int scb_num)
{
	scb_p->Sp_scb_num = scb_num;
	scb_p->SP_ConfigPtr = cfg_p;
	scb_p->Sp_OSspecific = sp;
	scb_p->SP_Cmd = EXEC_SCB;
	scb_p->Sp_SenseLen = 0;

#ifdef LE_DISK
	if (soft_swizzle)
		scb_p->SP_CDBPtr = kvtophys(scb_p->Sp_CDB) | PCI_NATIVE_VIEW;
	else
#endif
		scb_p->SP_CDBPtr = kvtophys(scb_p->Sp_CDB);
	scb_p->SP_AutoSense = 0;	/* no Auto Request Sense */
	/* Never swizzle the sg list pointers */
	scb_p->SP_SegPtr = kvtophys(sgp) | PCI_NATIVE_VIEW; 
	scb_p->Sp_Sensesgp = kvtophys(&(scb_p->Sp_SensePtr)) | PCI_NATIVE_VIEW;
}

/*
 * Takes a scsisubchan_t, which is what IRIX uses to convey its scsi
 * requests, and a sp_struct, which is what Adaptec sequencer
 * understands.  Fills out the sp_struct from the scsisubchan_t
 */
static void
fill_scb(sp_struct *scb_p, scsisubchan_t *sp)
{
	int rcode, rounded_len, segcnt;
	u_char cmd_buf[MAX_CDB_LEN];

	segcnt = fill_sglist(scb_p, sp);
	if (segcnt == -1) 
		return;  /* XXX should I panic or something? */

	if (segcnt > 255)
		scb_p->SP_SegCnt = 255;
	else
		scb_p->SP_SegCnt = segcnt;

	scb_p->SP_NoUnderrun = 0;
	scb_p->SP_NegoInProg = 0;
	scb_p->SP_ResCnt = 0;
	scb_p->SP_HaStat = 0;
	scb_p->SP_TargStat = 0;
	scb_p->SP_RejectMDP = 0;
	scb_p->SP_DisEnable = 1;
	scb_p->SP_TagEnable = 0;
	scb_p->SP_TagType = 0;
	scb_p->SP_Tarlun = (sp->s_target << 4) | (sp->s_lu);
	scb_p->SP_CDBLen = sp->s_len;
	scb_p->SP_AutoSense = 0;	/* don't automatically do sense */
	scb_p->Sp_SensePtr = 0;
	scb_p->Sp_SenseLen = 0;
	scb_p->Sp_Sensebuf = 0;

	rounded_len = ((sp->s_len + 3) / 4) * 4;     /* round to nearest word */
	bcopy(sp->s_cmd, scb_p->Sp_CDB, rounded_len);

	/* Write back and invalidate the buffer for all reads */
        if (sp->s_buf->b_bcount) {
		int rlen = sp->s_buf->b_bcount;
		DCACHE_WBINVAL(sp->s_buf->b_dmaaddr, rlen);      
        }
}


static int
fill_sglist(sp_struct *scbp, scsisubchan_t *sp)
{
	adp78_adap_t *adp;
	SG_ELEMENT *sgp;
	int segcnt=0;
	uint view;

	adp = adp78_array[sp->s_adap];
	sgp = adp->ad_sg;
	ASSERT(sgp);

#ifdef LE_DISK
	/*
	 * If soft swizzle is set, that means we are dealing with a 
	 * little endien disk.  Use the native view so all data remain
	 * little endien.
	 */
	if (soft_swizzle)
		view = PCI_NATIVE_VIEW;
	else 
#endif
		view = 0;

	if (sp->s_buf && sp->s_buf->b_bcount > 0) {
		if (BP_ISMAPPED(sp->s_buf)) {
			segcnt = adp_dma_map(sgp, sp->s_buf->b_dmaaddr, sp->s_buf->b_bcount, view);
		} else {
			PRINT("*** don't know how to dma_mapbp ***\n");
			return -1;
		}
	}

	/*
	 * Harry Yang says this must be at least 1.  If no segments are
	 * there (for commands such as Test Unit Ready, the len in the
	 * s/g list should be 0.
	 */
	if (segcnt == 0) {
		segcnt = 1;
		sgp[0].data_ptr = 0;
		sgp[0].data_len = SG_LAST_ELEMENT;
		DCACHE_WBINVAL(sgp, sizeof(SG_ELEMENT));
	}

	/* Write back and invalidate the buffer for all reads */
        if (sp->s_buf->b_bcount) {
		int rlen = sp->s_buf->b_bcount;
		DCACHE_WBINVAL(sp->s_buf->b_dmaaddr, rlen);      
        }

	return segcnt;
}

/*
 * Do some macro/casting magic to make the compiler happy
 */
#define SET_VIEW(a)	((uint)(a) | view)
#define UNSET_VIEW(a)	((uint)(a) & ~view)

static int
adp_dma_map(SG_ELEMENT *sgp, caddr_t addr, uint remain_bytes, uint view)
{
	uint actual_bytes, len_in_page, do_trans, segcnt=0;

	if (adp_verbose)
		PRINT("adp_dma_map: sgp 0x%x addr 0x%x bytes 0x%x view 0x%x\n",
		      sgp, addr, remain_bytes, view);

	if (IS_KSEG2(addr)) {
		PRINT("adp_dma_map: KSEG2 addr? 0x%x", addr);
		return -1;
	}

#ifndef SIMHIM
	if (IS_KSEGDM(addr))
		addr = (caddr_t) KDM_TO_PHYS(addr);
#endif

	/*
	 * map the first pageful of data, which may not be page aligned.
	 */
	len_in_page = NBPC - poff(addr);
	actual_bytes = MIN(remain_bytes, len_in_page);

	sgp[segcnt].data_ptr = (void *) SET_VIEW(addr);
	sgp[segcnt].data_len = actual_bytes;

	if (adp_verbose)
		PRINT("adp_dma_map: sg %d addr 0x%x len 0x%x\n",
		      segcnt, sgp[segcnt].data_ptr, sgp[segcnt].data_len);

	remain_bytes -= actual_bytes;
	addr += actual_bytes;
	segcnt++;

	/*
	 * map the rest of the pages, which are page aligned.
	 */
	while (remain_bytes > 0) {
		actual_bytes = MIN(remain_bytes, NBPC);

#ifdef SIMHIM
		sgp[segcnt].data_ptr = (void *) SET_VIEW(addr);
#else
		sgp[segcnt].data_ptr = (void *) SET_VIEW(KDM_TO_PHYS(addr));
#endif
		sgp[segcnt].data_len = actual_bytes;

		if (adp_verbose)
			PRINT("adp_dma_map: sg %d addr 0x%x len 0x%x\n",
			      segcnt, sgp[segcnt].data_ptr, sgp[segcnt].data_len);

		remain_bytes -= actual_bytes;
		addr += actual_bytes;
		segcnt++;

		if (segcnt >= NUMSG_BIG)
			return -1;
	}

	sgp[segcnt - 1].data_len |= SG_LAST_ELEMENT;

	DCACHE_WBINVAL(sgp, segcnt * sizeof(SG_ELEMENT));
	return segcnt;
}


/*
 * Convert a UBYTE from scb->HaStat to a uint that can be put in 
 * sp->s_error;
 */
static int
convert_hastatus(UBYTE hastatus)
{
	uint stat;

	switch(hastatus) {
	case HOST_ABT_HOST:
	case HOST_ABT_HA:
	case HOST_INV_LINK:
	case HOST_SNS_FAIL:
	case HOST_TAG_REJ:
	case HOST_NOAVL_INDEX:
	case HOST_NO_STATUS:
	case HOST_BUS_FREE:
	case HOST_PHASE_ERR:
	case HOST_HW_ERROR:
	case HOST_RST_HA:
	case HOST_RST_OTHER:
	case HOST_DU_DO:
		stat = SC_HARDERR;
		break;

	case HOST_SEL_TO:
		stat = SC_TIMEOUT;
		break;

	case HOST_ABT_FAIL:
		stat = SC_CMDTIME;
		break;

	default:
		PRINT("convert_hastatus: unknown status 0x%x\n",
			hastatus);
		stat = SC_HARDERR;
		break;
		
	}

	return (stat);
}

/*
 * Debug functions
 */
static void
dump_hastat(UBYTE hastatus)
{
	switch(hastatus) {
	case HOST_BUS_FREE:
		PRINT("unexpected bus free (hastat 0x%x) -- check cabling", hastatus);
		break;

	case HOST_PHASE_ERR:
		PRINT("phase error (hastat 0x%x) -- check cabling", hastatus);
		break;

	default:
		PRINT("unknown harderror (hastat 0x%x)", hastatus);
		break;
	}
}

static void
dump_cdb(u_char *cdb, int len)
{
	int i;

	for (i=0; i < len; i++)
		PRINT("%x ", cdb[i]);
	PRINT("\n");
}

#ifdef PCICFG_DEBUG

verify_pci_cfg(char *base)
{
	int rdata;

	PRINT("--- Verify reads (base addr 0x%x) ---\n", base);
	rdata = pci_get_cfg(base, PCI_CFG_VENDOR_ID);
	PRINT("vendor id : 0x%x\n", rdata);
	rdata = pci_get_cfg(base, PCI_CFG_DEVICE_ID);
	PRINT("device id : 0x%x\n", rdata);
	rdata = pci_get_cfg(base, PCI_CFG_COMMAND);
	PRINT("command :  0x%x\n", rdata);
	rdata = pci_get_cfg(base, PCI_CFG_STATUS);
	PRINT("status : 0x%x\n", rdata);
	rdata = pci_get_cfg(base, PCI_CFG_REV_ID);
	PRINT("rev id : 0x%x\n", rdata);
	rdata = pci_get_cfg(base, PCI_CFG_BASE_CLASS);
	PRINT("base class : 0x%x\n", rdata);
	rdata = pci_get_cfg(base, PCI_CFG_SUB_CLASS);
	PRINT("sub class : 0x%x\n", rdata);
	rdata = pci_get_cfg(base, PCI_CFG_PROG_IF);
	PRINT("prog if : 0x%x\n", rdata);
	rdata = pci_get_cfg(base, PCI_CFG_CACHE_LINE);
	PRINT("cache line : 0x%x\n", rdata);
	rdata = pci_get_cfg(base, PCI_CFG_LATENCY_TIMER);
	PRINT("latency timer : 0x%x\n", rdata);
	rdata = pci_get_cfg(base, PCI_CFG_HEADER_TYPE);
	PRINT("header type : 0x%x\n", rdata);
	rdata = pci_get_cfg(base, PCI_CFG_BIST);
	PRINT("bist 0x%x\n", rdata);
	rdata = pci_get_cfg(base, PCI_CFG_BASE_ADDR_0);
	PRINT("base addr 0 : 0x%x\n", rdata);
	rdata = pci_get_cfg(base, PCI_CFG_BASE_ADDR_1);
	PRINT("base addr 1 : 0x%x\n", rdata);

	PRINT("\n--- Writing to selected registers and read them back ---\n");
	PRINT("writing 0x789a to command\n");
	pci_put_cfg(base, PCI_CFG_COMMAND, 0x789a);
	rdata = pci_get_cfg(base, PCI_CFG_COMMAND);
	PRINT("read back 0x%x\n", rdata);

	PRINT("writing 0xbcde to status\n");
	pci_put_cfg(base, PCI_CFG_STATUS, 0xbcde);
	rdata = pci_get_cfg(base, PCI_CFG_STATUS);
	PRINT("read back 0x%x\n", rdata);

	PRINT("writing 0x56 to cache line\n");
	pci_put_cfg(base, PCI_CFG_CACHE_LINE, 0x56);
	rdata = pci_get_cfg(base, PCI_CFG_CACHE_LINE);
	PRINT("read back 0x%x\n", rdata);

	PRINT("writing 0x78 to latency timer\n");
	pci_put_cfg(base, PCI_CFG_LATENCY_TIMER, 0x78);
	rdata = pci_get_cfg(base, PCI_CFG_LATENCY_TIMER);
	PRINT("read back 0x%x\n", rdata);

	/*
	 * header type is a read-only register.  You have to
	 * modify pci_intf.c if you really want this write to work.
	 * Otherwise, its a good test for writing to a read-only location.
	 */
	PRINT("writing 0x9a to header type\n");
	pci_put_cfg(base, PCI_CFG_HEADER_TYPE, 0x9a);
	rdata = pci_get_cfg(base, PCI_CFG_HEADER_TYPE);
	PRINT("read back 0x%x\n", rdata);

	PRINT("writing 0xbc to BIST\n");
	pci_put_cfg(base, PCI_CFG_BIST, 0xbc);
	rdata = pci_get_cfg(base, PCI_CFG_BIST);
	PRINT("read back 0x%x\n", rdata);

	PRINT("writing 0xf1f2f3f4 to base addr 1\n");
	pci_put_cfg(base, PCI_CFG_BASE_ADDR_1, 0xf1f2f3f4);
	rdata = pci_get_cfg(base, PCI_CFG_BASE_ADDR_1);
	PRINT("read back 0x%x\n", rdata);

	PRINT("writing 0xff to REV_ID, which is read-only\n");
	pci_put_cfg(base, PCI_CFG_REV_ID, 0xff);
	rdata = pci_get_cfg(base, PCI_CFG_REV_ID);
	PRINT("read back 0x%x\n", rdata);

}

#endif /* PCICFG_DEBUG */
