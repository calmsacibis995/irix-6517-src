/*
 * Post 3 diags
 * For now, only SCSI
 *
 * $Id: post3diags.c,v 1.1 1997/08/18 20:48:58 philw Exp $
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/buf.h>
#include <sys/scsi.h>
#include <sys/newscsi.h>
#include <sys/edt.h>
#include <arcs/hinv.h>
#include <pci_intf.h>
#include <pci/PCI_defs.h>
#include <libsk.h>
#include <libsc.h>
#include "sys/IP32.h"

#include "adphim/him_scb.h"
#include "adphim/him_equ.h"
#include "sys/adp78.h"


UBYTE adp_curr_scb_num=0;
int scsi_test(void);

/*
 * Local function declarations
 */
static int find_scsi_devices(adp78_adap_t *);
static void init_pci(void);
static void init_adapter(adp78_adap_t *);
static void init_scb(sp_struct *, cfp_struct *, scsisubchan_t *, SG_ELEMENT *, int);
static void init_pci_cfg_regs(char *, uint);
static void fill_scb(sp_struct *, scsisubchan_t *);
static int adp_poll_status(adp78_adap_t *);
static int convert_hastatus(UBYTE);
void PH_ScbCompleted(sp_struct *);

/*
 * Local variables.
 */
static int
(*post3_test_table[])() = {
	scsi_test,
};

/*
 * Test adaptec scsi controllers 0 and 1.
 * Download the sequencer microcode to see if the controller is OK.
 * Probe for devices on the SCSI bus.
 *
 * Assume the HIM library is linked with this file.  This file is 
 * basically a very stripped down version of the firmware adp78.c
 *
 * Return values: 0 if test passed, -1 if test failed.
 */

#define POST_NUM_ADAPTERS	2
#define PCI_CFG_SIZE		(1 << CFG1_DEVICE_SHIFT)
#define FIRST_CFG_SLOT		((1 << 31) | (1 << CFG1_DEVICE_SHIFT))

adp78_adap_t *adp78_array[POST_NUM_ADAPTERS];

int
scsi_test()
{
	int i, rval, devmap, diskless;
	adp78_adap_t *adp;
	char *ep;

	init_pci();

	/* 
	 * Set up data structures for the HIM.
	 */
	for (i=0; i < POST_NUM_ADAPTERS; i++) {
		adp78_array[i] = malloc(sizeof(adp78_adap_t));
		adp = adp78_array[i];
		bzero(adp, sizeof(adp78_adap_t));

		adp->ad_ctlrid = i;
		adp->ad_pcibusnum = 0;
		adp->ad_pcidevnum = i+1;
		adp->ad_flags = 0;
		adp->ad_scsiid = 0;
		init_adapter(adp);
	}

	/*
	 * Download the sequencer.  If this works, then assume the
	 * controller is working.
	 */
	for (i=0; i < POST_NUM_ADAPTERS; i++) {
		rval = PH_init_him((adp78_array[i])->ad_himcfg);
		if (rval != 0) {
			printf("SCSI controller %d failed\n", i);
			return -1;
		}
	}

	/*
	 * Probe for devices.  devmap is a bitmap of devices found on
	 * controller 0 and 1.  I use a bitmap because I don't care where
	 * the devices are, just whether any are found.  If this is not
	 * diskless, and no devices are found, then signal error.
	 */
	for (i=0; i < POST_NUM_ADAPTERS; i++)
		devmap |= find_scsi_devices(adp78_array[i]);

	if ((ep = getenv("diskless")) != NULL)
		diskless = atoi(ep);
	else
		diskless = 0;

	if ((devmap == 0) && (!diskless)) {
		printf("No devices found, cannot boot system\n");
		return -1;
	}

	return 0;
}

/*
 * Probe the given SCSI bus.  Return a bitmap of devices found.
 * This is basically stolen from scsi_common.c in libsk/io
 */
static int
find_scsi_devices(adp78_adap_t *adp)
{
	int adap, targ, maxtarg, devmap=0, lu=0;
	int verbose, showconfig;
	char id[26];
	char *extra, *inv, *ep;

	if ((ep = getenv("verbose")) != NULL)
		verbose = atoi(ep);
	else
		verbose = 0;

	if ((ep = getenv("showconfig")) != NULL)
		showconfig = atoi(ep);
	else
		showconfig = 0;


	if (adp->ad_ctlrid == 0)
		maxtarg = 5;
	else
		maxtarg = 16;

	for (targ=0; targ < maxtarg; targ++) {
		if (!(inv = (char *) scsi_inq(adap, targ, lu)) ||
		    (inv[0] & 0x10) == 0x10)
			continue;

		if (showconfig || verbose) {
			strncpy(id,inv+8,8); id[8] = '\0';
			for (extra=id+strlen(id)-1; *extra == ' '; *extra-- = '\0') ;
			strcat(id," ");
			strncat(id,inv+16,16); id[25] = '\0';
			for (extra=id+strlen(id)-1; *extra == ' '; *extra-- = '\0') ;
			printf("(%d,%d) %s\n", adap, targ, id);
		}

		devmap |= 1 << targ;
	}

	return devmap;
}

/*
 * Intialize the MACE PCI bridge, clear all base registers, and program
 * the base registers that we need for SCSI.
 */
static void
init_pci()
{
	int slot, cbase;
	unsigned char *cfg_p;
	adp78_adap_t *adp;
	volatile uint *pci_error_flags = (uint *) PHYS_TO_K1(PCI_ERROR_FLAGS);
	volatile uint *pci_config_reg = (uint *) PHYS_TO_K1(PCI_CONTROL);

	*pci_error_flags = 0;
	*pci_config_reg = PCI_CONFIG_BITS;

	/*
	 * Turn off bus mastership in the command register and then
	 * wipe out all the base addresses in the system.
	 */
	cfg_p = (char *) FIRST_CFG_SLOT;
	for (slot = 1; slot < 3; slot++) {
		pci_put_cfg(cfg_p, PCI_CFG_COMMAND, 0);
		for (cbase = PCI_CFG_BASE_ADDR_0;
		     cbase <= PCI_CFG_BASE_ADDR_5;
		     cbase += 4)
			pci_put_cfg(cfg_p, cbase, 0xffffffff);
		cfg_p += PCI_CFG_SIZE;
	}

	/*
	 * Program the base registers that I need.
	 */
	adp = adp78_array[0];
	adp->ad_baseaddr = 0;
	adp->ad_cfgaddr = (char *) 0x8000800;
	pci_put_cfg(adp->ad_cfgaddr, PCI_CFG_BASE_ADDR_1, (int) adp->ad_baseaddr);

	adp = adp78_array[1];
	adp->ad_baseaddr = (char *) 0x1000;
	adp->ad_cfgaddr = (char *) 0x8001000;
	pci_put_cfg(adp->ad_cfgaddr, PCI_CFG_BASE_ADDR_1, (int) adp->ad_baseaddr);
}


static void
init_adapter(adp78_adap_t *adp)
{
	int i;
	cfp_struct *cfgp;
	uint real_base = KDM_TO_PHYS(adp->ad_baseaddr) - PCI_LOW_MEMORY;
	hsp_struct *hspp;
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

	cfgp->Cf_BusNumber = adp->ad_pcibusnum;
	cfgp->Cf_DeviceNumber = adp->ad_pcidevnum;
	cfgp->Cf_MaxNonTagScbs = 2;
	cfgp->Cf_NumberScbs = 1;
	cfgp->Cf_MaxTagScbs = 1;
	cfgp->CFP_SuppressNego = 0;
	cfgp->CFP_Base = (struct aic7870_reg *) adp->ad_baseaddr;
	cfgp->Cf_AdapterId = pci_get_cfg(adp->ad_cfgaddr, PCI_CFG_DEVICE_ID);
	/*
	 * No fast 20 in the prom!  No wide in the prom.
	 */
	cfgp->CFP_EnableFast20 = 0;
	cfgp->Cf_MaxTargets = 16;

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

	cfgp->Cf_BusRelease = 0x10;
	cfgp->CFP_ScsiParity = 1;
	cfgp->CFP_ScsiTimeOut = 0;
	cfgp->Cf_AllowSyncMask = 0xffff;
	cfgp->CFP_AllowDscnt = 0xffff;
	for (i=0; i < 16; i++)
		cfgp->Cf_ScsiOption[i] = 0x3;

	hspp = malloc(sizeof(hsp_struct));
	bzero(hspp, sizeof(hsp_struct));
	cfgp->CFP_HaDataPtr = hspp;
	cfgp->Cf_HaDataPhy = KDM_TO_PHYS(hspp);

	adp->ad_sg = malloc(NUMSG_SMALL * sizeof(SG_ELEMENT));
	bzero(adp->ad_sg, (NUMSG_SMALL * sizeof(SG_ELEMENT)));

	adp->ad_himcfg = cfgp;

	/*
	 * One last check before returning
	 */
	pci_err_stat = * (uint *) (PHYS_TO_K1(PCI_ERROR_FLAGS));
	pci_err_addr = * (uint *) (PHYS_TO_K1(PCI_ERROR_ADDR));

	* (uint *) (PHYS_TO_K1(PCI_ERROR_FLAGS)) = 0;
	* (uint *) (PHYS_TO_K1(PCI_ERROR_ADDR)) = 0;
}


static void
init_scb(sp_struct *scb_p, cfp_struct *cfg_p, scsisubchan_t *sp, SG_ELEMENT *sgp, int scb_num)
{
	scb_p->Sp_scb_num = scb_num;
	scb_p->SP_ConfigPtr = cfg_p;
	scb_p->Sp_OSspecific = sp;
	scb_p->SP_Cmd = EXEC_SCB;
	scb_p->Sp_SenseLen = 0;
	scb_p->SP_CDBPtr = KDM_TO_PHYS(scb_p->Sp_CDB);
	scb_p->SP_AutoSense = 0;	/* no Auto Request Sense */

	/* Never swizzle the sg list pointers */
	scb_p->SP_SegPtr = KDM_TO_PHYS(sgp) | PCI_NATIVE_VIEW; 
	scb_p->Sp_Sensesgp =KDM_TO_PHYS(&(scb_p->Sp_SensePtr)) | PCI_NATIVE_VIEW;
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
		freesubchannel(sp);
		return NULL;
	}

	freesubchannel(sp);
	return inq_buf;
}


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
		return;
	}

	scbp = adp->ad_scb[sp->s_target][sp->s_lu];

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
 * Takes a scsisubchan_t, which is what IRIX uses to convey its scsi
 * requests, and a sp_struct, which is what Adaptec sequencer
 * understands.  Fills out the sp_struct from the scsisubchan_t
 */
static void
fill_scb(sp_struct *scb_p, scsisubchan_t *sp)
{
	int rcode, rounded_len, segcnt;
	u_char cmd_buf[MAX_CDB_LEN];
	adp78_adap_t *adp;
	SG_ELEMENT *sgp;

	/*
	 * I know that all we do in post3 is inquiry, which has a SG list
	 * of 1.  So just set it up here instead of calling fill_sglist.
	 * Also wbinval the data buffer and wb the scatter gather list.
	 */
	adp = adp78_array[sp->s_adap];
	sgp = adp->ad_sg;
	sgp[0].data_ptr = (char *) KDM_TO_PHYS(sp->s_buf->b_bcount);
	sgp[0].data_len = sp->s_buf->b_bcount | SG_LAST_ELEMENT;
	DCACHE_WBINVAL(sp->s_buf->b_dmaaddr, sp->s_buf->b_bcount);
	DCACHE_WB(sgp, sizeof(SG_ELEMENT));

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
}


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

static int
adp_poll_status(adp78_adap_t *ad_p)
{
	unsigned char rval, intstat;
	unsigned long count=0;
	uint pci_err_stat, pci_err_addr;
	uint status;
	volatile char *h, hv;

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
			*(uint *) (PHYS_TO_K1(PCI_ERROR_FLAGS)) = 0;
			status = pci_get_cfg(ad_p->ad_cfgaddr, PCI_CFG_STATUS);
			pci_put_cfg(ad_p->ad_cfgaddr, PCI_CFG_STATUS, 0xe900);
			h = (char *) ((int) ad_p->ad_baseaddr + CLRINT);
			*h = 0x1f;
			status = pci_get_cfg(ad_p->ad_cfgaddr, PCI_CFG_STATUS);
			return -1;
		}
		
		us_delay(POLL_WAIT);
		count++;

		if (count > (US_IN_SEC / POLL_WAIT) * ad_p->ad_timeoutval) {
			return -1;
		}
	}

	return 0;
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
		stat = SC_HARDERR;
		break;
		
	}

	return (stat);
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

	ad_p = adp78_array[sp->s_adap];

	switch(scb_p->SP_Stat) {
	case SCB_PENDING:
		return;

	case SCB_COMP	:
		sp->s_error = SC_GOOD;
		sp->s_status = ST_GOOD;
		break;

	case SCB_ABORTED:
		sp->s_error = SC_HARDERR;

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

			break;
		}

		break;

	case INV_SCB_CMD:
		sp->s_error = SC_HARDERR;
		sp->s_status = ST_GOOD;
		break;

	default:
		sp->s_error = SC_HARDERR;
		sp->s_status = ST_GOOD;
		break;
	}

	if (sp->s_notify)
		(*sp->s_notify)(sp);
}


scsisubchan_t *
allocsubchannel(int adap, int targ, int lun)
{
	adp78_adap_t *adp;
	scsisubchan_t *sp;
	sp_struct *scbp;

	adp = adp78_array[adap];

	sp = malloc(sizeof(scsisubchan_t));
	bzero(sp, sizeof(scsisubchan_t));
	adp->ad_subchan[targ][lun] = sp;

	scbp = malloc(sizeof(sp_struct));
	bzero(scbp, sizeof(sp_struct));
	init_scb(scbp, adp->ad_himcfg, sp, adp->ad_sg, targ);
	adp->ad_scb[targ][lun] = scbp;

	sp->s_adap = adap;
	sp->s_target = targ;
	sp->s_lu = lun;

	return(sp);
}

/*
 * Deallocate the scsisubchan structure from the adapter structure.
 */
void
freesubchannel(scsisubchan_t *sp)
{
	adp78_adap_t *adp;

	adp = adp78_array[sp->s_adap];

	free(adp->ad_subchan[sp->s_target][sp->s_lu]);
	free(adp->ad_scb[sp->s_target][sp->s_lu]);
	adp->ad_subchan[sp->s_target][sp->s_lu] = NULL;
	adp->ad_scb[sp->s_target][sp->s_lu] = NULL;
}
