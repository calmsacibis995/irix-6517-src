/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1996, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ident  "$Revision: 1.12 $"

#include "sys/types.h"
#include "sys/cpu.h"
#include "sys/sbd.h"
#include <libsc.h>
#include <libsk.h>
#include "uif.h"

#include <sys/PCI/PCI_defs.h>
#include <sys/invent.h>
#include "sys/scsi.h"
#include "sys/newscsi.h"
#include <sys/ql_standalone.h>
#include "sys/buf.h"
#include "diag_scsi.h"

#define DIAG_NUM_ATTMPT		1000
#define PCI_DEVICE_SIZE		0x200000
#define DEV_ATTRIBUTES  (BRIDGE_DEV_COH | BRIDGE_DEV_SWAP_DIR | BRIDGE_DEV_DEV_IO_MEM )
#define NUMBER_OF_RRB   	4

/* these must be negative or > 16 to avoid confusion with valid
 * device types from the inquiry */
#define BAD_DEVICE              -1
#define NO_DEVICE               -2
#define UNKNOWN_DEVICE          -3
#define HARD_DISK		0
#define SCTAPE 			1
#define FLOPPY_DISK 		0x80        	/* pseudo type */

/* sense key definitions */
#define NOT_READY 		2     		/* drive not ready */
#define ILLEGAL_REQ 		5   		/* means cmd not supported */
#define NO_MEDIA_PRESENT 	0x3a
#define UNIT_ATN 		6      		/* device or bus was reset, etc. */

#define NOTFOUNDTRIES 		10

/* external functions */
extern int ql_init(u_int, caddr_t, paddr_t, void *);
extern int qlreset(u_char);
extern void ql_clear_structs(void);
extern void pon_us_delay(int);

/* external structs and variables */
extern int sc_inq_stat;
extern u_char scsi_ha_id[];
extern pHA_INFO ha_info[SC_MAXADAP];
extern pHA_INFO ha_list;
extern struct scsi_target_info tinfo[SC_MAXADAP][SCSI_MAXTARG][SCSI_MAXLUN];

/* static functions */
static int bridgeinit(int);
static int scsi(__uint32_t);
static bool_t scsi_unit_ready(void);
static int probe_scsi_device(int, int);
static int do_cmd(void);
static bool_t get_sense(void);
static char *cmdname(unchar);
static int scsi_startunit(void);
static void tinfo_clear(int);

/* static data */
static buf_t scbuf;
static unsigned char sense_key, addsense;
static int is_removable;   /* last target inq'ed have removable media? */
static scsisubchan_t *ide_sc;

/*
   This routine dispatches mail box commands. 
*/
   
bool_t
diag_ql_mbox_cmd(pISP_REGS diag_isp, u_char out_cnt, 
                 u_char in_cnt, u_short reg0, u_short reg1, 
                 u_short reg2, u_short reg3, u_short reg4, u_short reg5, 
                 u_short reg6, u_short reg7)
{

    int             count = 0;
    u_short         hccr = 0,isr,bus_sema;
	

    do
    {
        hccr = PCI_INH(&diag_isp->hccr);
        count++;

    } while ((hccr & HCCR_HOST_INT) && (count < 1000));

    if (count == 1000)
    {
        msg_printf(ERR,"Previous commands not being drained ...  \n");
        return(1);
    }

    switch (out_cnt)
    {
         case 8:
                PCI_OUTH(&diag_isp->mailbox7,  reg7);
         case 7:
                PCI_OUTH(&diag_isp->mailbox6,  reg6);
         case 6:
                PCI_OUTH(&diag_isp->mailbox5,  reg5);
         case 5:
                if (reg0 != 0x11)
                    PCI_OUTH(&diag_isp->mailbox4,  reg4);
         case 4:
                PCI_OUTH(&diag_isp->mailbox3,  reg3);
         case 3:
                PCI_OUTH(&diag_isp->mailbox2,  reg2);
         case 2:
                PCI_OUTH(&diag_isp->mailbox1,  reg1);
         case 1:
                PCI_OUTH(&diag_isp->mailbox0,  reg0);
    }
    PCI_OUTH(&diag_isp->hccr,  HCCR_CMD_SET_HOST_INT);


    /*
       Look for the status.
    */
    count = 0;
    bus_sema = 0;
    while ((!(bus_sema & BUS_SEMA_LOCK)) && (count < 10000))
    {
      bus_sema = PCI_INH(&diag_isp->bus_sema);
      count ++;
    }
    if (count == 100000)
    {
       msg_printf(ERR,"semaphore locked\n");
       return(1);
    }
    isr = 0;
    count = 0;
    while ((!(isr & BUS_ISR_RISC_INT)) && (count < 100000))
    {
        isr = PCI_INH(&diag_isp->bus_isr);
        pon_us_delay(10);
        count ++;
    }
    if (count == 100000)
    {
       msg_printf(ERR,"Interrupt failed to arrive\n");
       return(1);
    }
   PCI_OUTH(&diag_isp->hccr, HCCR_CMD_CLEAR_RISC_INT);
   PCI_OUTH(&diag_isp->bus_sema, 0);

   return(0);
}

bool_t
pon_scsi(__uint32_t npci)
{
    int                     count;
    int			    scsictrlr=0, scsidev=0;
    pISP_REGS               diag_isp;
    volatile __psunsigned_t scsi_base;

    msg_printf (VRB, "\rSCSI controller %d /devices test\n", npci);

    /* Note: code assumes SCSI0 and SCSI 1 are next to each other */
    scsi_base = PHYS_TO_K1(SCSI0_PCI_DEVIO_BASE + (npci * PCI_DEVICE_SIZE));
    diag_isp = (pISP_REGS) scsi_base;

    PCI_OUTH(&diag_isp->bus_icr,  ICR_ENABLE_RISC_INT | ICR_SOFT_RESET);
    pon_us_delay(10);
    PCI_OUTH(&diag_isp->hccr, HCCR_CMD_RESET);
    PCI_OUTH(&diag_isp->hccr, HCCR_CMD_RELEASE);
    PCI_OUTH(&diag_isp->hccr, HCCR_WRITE_BIOS_ENABLE);
    PCI_OUTH(&diag_isp->bus_config1, CONF_1_BURST_ENABLE);
    PCI_OUTH(&diag_isp->bus_sema, 0x0);

    for (count = 0; count < diag_risc_code_length01;count++)
    {
        if (diag_ql_mbox_cmd(diag_isp, 3, 3, MBOX_CMD_WRITE_RAM_WORD,
                            (u_short) (diag_risc_code_addr01 + count), 
                             diag_risc_code01[count], 0, 0, 0, 0, 0))
        {
             msg_printf(ERR,"QL: write data failure at %d word\n",count);
	     scsictrlr |= 0x100000;
	     goto failed;
        }
    }

    if (diag_ql_mbox_cmd(diag_isp, 2, 1,
                        MBOX_CMD_EXECUTE_FIRMWARE,
                        diag_risc_code_addr01,
                        0, 0, 0, 0, 0, 0))
    {
	msg_printf(ERR,"QL: EXECUTE FIRMWARE Command Failed.\n");
	scsictrlr |= 0x100000;
	goto failed;
    }

    count =  0;
    while ((PCI_INH(&diag_isp->mailbox0) != 0x4000) && (count < DIAG_NUM_ATTMPT))
    {
        pon_us_delay(50);
        count++;
    }
    
    if (count == DIAG_NUM_ATTMPT)
    {
       msg_printf(ERR,"SCSI self test failed (mbox val %x).\n",
		  diag_isp->mailbox0);
       scsictrlr |= 0x100000;
       goto failed;
    }

    /* do scsi device test */
    scsidev = scsi(npci);
    ql_clear_structs();		/* free host adaptor structures */
    tinfo_clear(npci);		/* free target device info structures */
    if ( scsidev < 0 ) {
	scsictrlr = 0x100000;
  	scsidev = 0;
	goto failed;
    }
    if ( scsidev != 0 ) goto failed;

    okydoky();
    return(0);

failed:
    /* error is summarized by pon_more */
    return (scsictrlr | scsidev);
}

/*
** SCSI Device Test 
**   -- finds out what devices are attached by probing SCSI controller 0 and 1.
**   -- If a device is attached, it makes sure that the device is ready
**      by issuing a startunit command.
*/
static int
scsi(__uint32_t npci)
{
    int            max_targets= SCSI_MAXTARG;
    int		   bad_scsi_devices = 0;
    int            device_type;
    bool_t         no_dev_present = TRUE;
    char 	   *diskless;
    char	   *cp;
    pHA_INFO       ha;
    __psunsigned_t  ql_config_base,     /* PCI Config space for this device */
    		    ql_register_base;   /* PIO space for qlogic registers */
    void 	    *bus_base; 
    int first = NOTFOUNDTRIES;         /* try for up to 5 seconds */
    int target;

    if ( bridgeinit(npci)) {
	msg_printf(ERR, "Bridge Initialization failed\n");
	return(-1);
    }

    bus_base = (void *)PHYS_TO_K1(BRIDGE_BASE);
    ql_config_base = (__psunsigned_t)PHYS_TO_K1(BRIDGE_BASE +
				BRIDGE_TYPE0_CFG_DEV(npci));
    ql_register_base = (__psunsigned_t)PHYS_TO_K1(SCSI0_PCI_DEVIO_BASE +
				(npci * PCI_DEVICE_SIZE));


    if (ql_init(npci,(caddr_t)ql_config_base,(paddr_t)ql_register_base, bus_base)){
        msg_printf(ERR, "Initialization at slot %x failed\n", npci);
	return(-1);
    }

    /* now walk the bus and register the devices found */

    ha = ha_info[npci];

    if (ha == NULL)
    {
        msg_printf(ERR, "Failed to configure adapter %x, host adapter structure\n", npci);

        return(-1);
    }

    if (!qlreset(npci))
    {
        msg_printf(ERR, "Unable to reset bus %d\n", npci);
	return(-1);
    } else
        pon_us_delay(1000);


    if (first < 0 || first > NOTFOUNDTRIES)
	first = NOTFOUNDTRIES;

    scsi_seltimeout(npci, 1);      /* shorten selection timeout */

again:

    /* For IP30 adapter 0 needs only 1,2,3
    */
    if (npci == 0) max_targets = 4;

    for (target = 0; target < max_targets; target++)
    {
	if(target == scsi_ha_id[npci])
		continue;       /* ignore the host adapter. */

	/* at some time, for some devices, errors were encountered
	if testready was done before an inquiry... */
	device_type = probe_scsi_device (npci, target);
	if (device_type == BAD_DEVICE) {
	   no_dev_present = FALSE;
           bad_scsi_devices |= 1 << target;
           continue;
        }
        else if (device_type == NO_DEVICE)
           continue;

        no_dev_present = FALSE;
        ide_sc = allocsubchannel((int)npci, target, 0);
        if(ide_sc == NULL)      /* should never happen */
           continue;
        ide_sc->s_buf = &scbuf;
        ide_sc->s_notify = NULL;        /* no callbacks */

        /* clear possible UNIT ATTENTION */
        (void)scsi_unit_ready();

        if(device_type == HARD_DISK && scsi_startunit() == FALSE) {
           /*
           * *NOT* an error in the diags sense if drives with
           * removable media don't have the media present!
           */
           if(addsense == NO_MEDIA_PRESENT && is_removable)
             	msg_printf(ERR,
		 "\rNo media present in sc%d,%d, not ready\n", npci, target);
           else
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
         diskless = getenv("diskless");

	 if(!diskless || (*diskless == '0')) {
             if(first && npci == 0) {
                 if(first == NOTFOUNDTRIES){
                 	msg_printf(VRB, "Reset SCSI controller %d and retry\n",
				   npci);
                        qlreset(npci);
                  }
                  pon_us_delay(500000);  /* wait a half second */
                  first--;
                  goto again;
              } else {
		  /* set for all ctrls, pon_more sorts it out */
	          bad_scsi_devices |= 0x10000;
	      }
	 }
         msg_printf (VRB, "\rNo SCSI %d device available\n", npci);
    }

    scsi_seltimeout(npci, 0);      /* standard selection timeout */
    return(bad_scsi_devices);
}

static int
probe_scsi_device(int ctrlr, int unit)
{
    char *inquiry_data, *msg;

    inquiry_data = (char *)scsi_inq((int)ctrlr,(int)unit,0);
    if( (inquiry_data == NULL) || ((inquiry_data[0] & 0x10) == 0x10)) { 
	return NO_DEVICE;
    }

    is_removable = inquiry_data[1] & 0x80;

    switch (inquiry_data[0]) {
        case HARD_DISK: /* no define for this in invent.h */
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
                    unit, inquiry_data[0], &inquiry_data[8]);
                return (UNKNOWN_DEVICE);
    }

    msg_printf (VRB, "Device %d is %s, %s\n", unit, msg, &inquiry_data[8]);
    return  inquiry_data[0];
}

static bool_t
scsi_unit_ready(void)
{
    static unsigned char test_unit_ready[6];        /* all zeros */
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
	goto again;     /* sense overwrote struct, reset it */
    }
    return ret;
}

/*      Make sure device is ready, by issuing a startunit command, and
 *  then repeatedly issuing testunitready until it becomes ready.
 *      The repeated tstrdy is mainly in case someone forgot to
 *      set the motor start jumper/switch 'correctly', in which case
 *      most drives won't accept commands while they are spinning up.
 *  The timeout is total time in this routine, including the startunit,
 *  because the timeouts for testrdy have been bumped so much.
*/
static int
scsi_startunit(void)
{
    bool_t                          unit_ready;
    static unsigned char            start_unit[] = {
                0x1b, 0x00, 0x00, 0x00, 0x01, 0x00 };


    scbuf.b_bcount = 0;
    scbuf.b_dmaaddr = NULL;
    scbuf.b_flags = B_READ;
    ide_sc->s_cmd = start_unit;
    ide_sc->s_len = sizeof(start_unit);
    ide_sc->s_timeoutval = 240*HZ;   /* can take a long time, esp. on power-up */
    if(do_cmd() == FALSE) {
	if(sense_key != ILLEGAL_REQ) {
		msg_printf (ERR, "\rSCSI device %d,%d fails start cmd\n",
                                ide_sc->s_adap, ide_sc->s_target);
		return FALSE;   /* supports start, but it failed!? */
	}
        /* else assume that startunit isn't supported, and wait for
        * it to become ready */
    }

    while((unit_ready=scsi_unit_ready())==FALSE && sense_key==NOT_READY
                && addsense != NO_MEDIA_PRESENT)
	pon_us_delay(500000);
    if(unit_ready == FALSE)
	msg_printf (ERR, "\rSCSI device %d,%d is not ready for use\n",
                        ide_sc->s_adap, ide_sc->s_target);
    return unit_ready;
}

/* run a command, and do a sense if needed (unless it is a sense cmd!)
 * Do all the error checking and possible reporting.  */
static int
do_cmd(void)
{
    unchar cmd = *ide_sc->s_cmd;

    sense_key = 0;  /* for checking after a FALSE return */

    doscsi(ide_sc);

    if (ide_sc->s_error != SC_GOOD) {
	msg_printf (ERR,
		"\rSCSI device %d,%d: bad driver status %#x during %s\n",
                ide_sc->s_adap, ide_sc->s_target, ide_sc->s_error,
                cmdname(cmd));
	return (FALSE);
    }

    switch (ide_sc->s_status) {
        case ST_GOOD:
                break;

        case ST_BUSY:   /* treat like unit atn; caller usually retrys,
                        * in cases where we are likely to get a busy */
                addsense = 0;
                sense_key = UNIT_ATN;
                return FALSE;

        case ST_CHECK:
                if(cmd == 3) { /* don't do getsense on getsense errors */
                        msg_printf (ERR, "\rSCSI device d,%d: failure on %s\n",
                            cmdname(cmd));
                        return FALSE;
                }
                else if(get_sense() == FALSE) {
                        /* caller can check sense_key for specific actions */
                        return (FALSE);
                }
                break;

        default:
                msg_printf (ERR,
                        "\rSCSI device %d,%d: bad status %#x on %s\n",
                        ide_sc->s_adap, ide_sc->s_target, ide_sc->s_status,
                        cmdname(cmd));
                return (FALSE);
     }   /* switch */
     return TRUE;
}

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
    static unsigned char            request_sense[] = {
                0x03, 0x00, 0x00, 0x00, 0x14, 0x00      };
    unsigned int *sense_data;
    unchar prevcmd = *ide_sc->s_cmd;


    sense_data = dmabuf_malloc(0x14);
    scbuf.b_bcount = sizeof(0x14);
    scbuf.b_dmaaddr = (caddr_t)sense_data;
    scbuf.b_flags = B_READ;
    ide_sc->s_cmd = request_sense;
    ide_sc->s_len = sizeof(request_sense);
    ide_sc->s_timeoutval = 2*HZ;
    if(do_cmd() == FALSE) {
	msg_printf (ERR, "\rCouldn't determine cause of error for %s\n",
		cmdname(prevcmd));
	dmabuf_free(sense_data);
	return FALSE;   /* couldn't get the info */
    }

    sense_key = *((unsigned char *)sense_data + 2) & 0x0f;
    addsense = *((unsigned char *)sense_data + 7)>4 ?
                *((unsigned char *)sense_data + 12) : 0;
    msg_printf (DBG, "\rSCSI device %d,%d got %s (asense=%x) during %s\n",
                ide_sc->s_adap, ide_sc->s_target,
            scsi_key_msgtab[sense_key], addsense, cmdname(prevcmd));

    /* no error, or recovered error are OK */
    dmabuf_free(sense_data);
    return ((sense_key == 0x0) || (sense_key == 0x1)) ? TRUE : FALSE;
}

static void
tinfo_clear(int ctrlr)
{
    int            max_targets= SCSI_MAXTARG;
    scsi_target_info_t *info;
    int target;

    if( ctrlr == 0) max_targets = 4;
    /* For IP30 adapter 0 needs only 1,2,3
    */
    for (target = 0; target < max_targets; target++)
    {
        if(target == scsi_ha_id[ctrlr])
                continue;
        info = &tinfo[ctrlr][target][0];
        if (info->si_inq != NULL) {
                dmabuf_free(info->si_inq);
                info->si_inq = NULL;
        }

        if (info->si_sense != NULL) {
                dmabuf_free(info->si_sense);
                info->si_sense = NULL;
        }
    }

}

static int
bridgeinit(int slot)
{
    bridge_t       *bridge;
    bridgereg_t     dev_reg;
    __uint32_t      mem_base;
    volatile __uint32_t *mem_base_ptr;
    int tmp;

    mem_base_ptr = (volatile __uint32_t *)PHYS_TO_K1(BRIDGE_BASE+BRIDGE_TYPE0_CFG_DEV(slot)) + PCI_CFG_BASE_ADDR_1 / sizeof(__uint32_t);
    mem_base = (*mem_base_ptr & ~0xfffff) >> 20;

    bridge = (bridge_t *)PHYS_TO_K1(BRIDGE_BASE);
    dev_reg = bridge->b_device[slot].reg & ~BRIDGE_DEV_OFF_MASK;
    dev_reg |= mem_base | DEV_ATTRIBUTES;
    bridge->b_device[slot].reg = dev_reg;

    tmp = alloc_bridge_rrb((void *) bridge, slot, 0, NUMBER_OF_RRB);
    if (tmp != NUMBER_OF_RRB) {
        if(tmp == 0) 
	   msg_printf(ERR, "alloc_bridge_rrb failed\n");
	else
	   msg_printf(ERR, "Cannot allocate %d RRBs for ql %d\n",NUMBER_OF_RRB, slot);
	return(1);
    }
    return(0);
}
