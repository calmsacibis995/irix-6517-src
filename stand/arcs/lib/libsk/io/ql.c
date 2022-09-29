/**  Edit history:
**
**  01/04/95  robertn   started port to sgi.  This driver is base on
**                      the Qlogic SCO driver
*/

/*
** For initial SN0 bring up we use the wd93 chip on the HUB's
** JUNK Bus. This means that we cannot link ql.o as we need to
** have only 1 set of routines like alloc_subchannel, doscsi etc.
*/

/******************************************************************************/


/**********************************************************
**
**          Debug options
**
**********************************************************/
#if SN0
#define A64_BIT_OPERATION
#define SN0_PCI_64
#endif

#ifdef DEBUG
#define QL_DEBUG		/* include debug code */
#define MBOX_DEBUG		/* debug prints in the mbox routine */
#define QL_IOCTL		/* include code to support ha ioctls */

#define DUMP_REGS		/* dump ql registers to console */
/*#define DUMP_PARMS		   dump mail box type parms	*/

#define DUMP_IOCB		/* dump iocb to console */
#endif

/**********************************************************
**
**          Compile time options
*/

/*
** Some miscellaneous configuration parameters.
*/

#define ENABLE_BURST		/* define to enable PCI bursting */

/*
** Some general run-time options.
*/
#define TAGGED_CMDS		/* support tagged commands */

/*
** The following are initialization options.
** They are mainly used to assist anyone that is responsible for bringing
** up a ISP for the first time.  Once the driver is running, most of these
** should be removed (including the code).
*/

/*#define CLEAR_INTR_FIRST   clear the interrupt control register */
/*#define SOFT_RESET       issue a soft reset to the chip */
/*#define CHECK_HCCR_FIRST   check hccr before loading firmware */
#define LOAD_FIRMWARE		/* define to load new firmware */
/*#define SWAP_DPROM        swap the firmware array -- for dproms only */
#define MAILBOX_TEST		/* test mailboxes (firmware running?) */

#define CHECKSUM_TEST           /* verify the checksum we calulate with */
                                /* the checksum the ISP calulates       */

#ifdef IP30
#undef VERIFY_TEST		/* verify the RISC firmware word by word  */
#undef USE_MAPPED_CONTROL_STREAM
#define SWAP_DATA_STREAM
#define LOAD_VIA_DMA		/* load firmware with DMA else a word at a
				 * time */
#endif

#define SCSI_TIMEOUT_WAR

#if defined (SN0) && defined (SCSI_TIMEOUT_WAR)
#define SCSI_TO_MAX_RETRY 3
int first_adaptor = 1;
#endif

/*
** Leave normal_startup disabled and init_at_open enabled when first bringing
** up this driver.  This will allow you to run the init routine by opening
** the host adapter device instead of letting the system do it at start up
** time.  Doing it at system start up time can let debug text scroll off the
** screen and if it should fail, leave you with an unbootable system (yuck).
** I would also use poll mode.  This prevents interrupts from being enabled
** and polls when ever an interrupt is expected.  Once this is working
** successfully, enable the interrupts.
*/


#define NORMAL_STARTUP		/* continue normal init */


/*
** The following define the way the mailbox registers are handled.
** Once this is stable, delete code associated with options that are
** commented out and delete conditionals of enabled options.
*/
#define WAIT_SEMA		 /*wait for semaphore (else wait on
				 * interrupt) */

/*
** Enabling these options generates multiple adapter requests for a
** single device driver request.  The device driver's request is not
** returned until the last "copy" of the request is returned from the
** host adapter.  This allows us to trash the adapter (firmware) with
** minimal system overhead.  This also allows us to load up the adapter
** with a lot more requests than most systems could generate.
** XXX robertn - this has never been tested
*/
/*#define MAX_DUPS 32    issue 32 multiples of the same command */


/*
** Some hard coded parameters (use these to bring up driver).
*/
#define MEM_BASE_ADDRESS    0xbf500000

/**********************************************************
**
**          Include files
*/
#include "sys/types.h"
#include <sys/debug.h>
#include "sys/param.h"
#include "sys/scsi.h"
#include "sys/newscsi.h"
#include <sys/scsimsgs.h>       /* correct values of SC_NUMADDSENSE */

#include "sys/edt.h"
#include "sys/cpu.h"
#include "sys/PCI/PCI_defs.h"
#include "sys/time.h"

#include "sys/buf.h"
#include "sys/pfdat.h"
#include "sys/dkio.h"
#include <sys/invent.h>
#include <sys/mtio.h>
#include "sys/tpsc.h"
#include "sys/cmn_err.h"
#include "sys/sysmacros.h"
#include "sys/kmem.h"

#ifdef DEBUG
#include <sys/idbg.h>
#include <sys/debug.h>
#endif


#ifdef QL_IOCTL
#include "sys/dir.h"
#include "sys/xlate.h"
#include "sys/proc.h"
#endif


#include "sys/systm.h"
#include "sys/errno.h"

#include <arcs/hinv.h>
#include <arcs/cfgdata.h>
#include <sys/scsidev.h>
#include <sys/immu.h>

#include "sys/PCI/bridge.h"
#include <standcfg.h>

#ifdef SN0
#include <libkl.h>
#include <pgdrv.h>
#include <sys/SN/SN0/ip27config.h>
#include <sys/SN/addrs.h>
#include <sys/SN/kldiag.h>
#include <sys/SN/nvram.h>
#include <kllibc.h>

#if defined (SCSI_TIMEOUT_WAR)
#include <ksys/elsc.h>
#endif /* SCSI_TIMEOUT_WAR */

#endif

#include <libsc.h>
#include <libsk.h>
#include <sys/dmamap.h>

#if QL_MAXLUN > 8
#error too many luns for dev_init field!
#endif

#define initialized(ha,id,lun)	(ha->dev_init[id] & (1<<lun))

#undef MAX_ADAPTERS		/* standalone uses SC_MAXADAP */

union scsicfg
{
    struct scsidisk_data disk;
    struct floppy_data fd;
};

#ifdef SN0
void kl_qlogic_AddChild(COMPONENT*, union scsicfg *) ;
#endif
struct scsi_target_info *
ql_u_cmd(u_char, u_char, u_char,
         u_char, u_char, u_char) ;
void cdrom_revert(u_char, u_char, u_char) ;

#include <sys/pci_intf.h>

#include "sys/ql_firmware_standalone.h"	

#include "sys/ql_standalone.h"		/* structure definitions for adapter */

/****************************************************************/
/* Function Prototypes						*/
/*								*/

void            qlintr(pHA_INFO);

int             ql_entry(scsi_request_t *);

struct scsi_target_info tinfo[SC_MAXADAP][QL_MAXTARG][QL_MAXLUN];

int             qlreset(u_char adap);

struct scsi_target_info *qlinfo(u_char extctlr, u_char targ, u_char lun);

static void     ql_set_defaults(pHA_INFO ha);
static int      reqi_get_slot(pHA_INFO ha,int);
static int      ql_init_board(int, u_short, caddr_t, paddr_t, void *);
static int      ql_init_isp(pHA_INFO);
static void     ql_service_interrupt(pHA_INFO);
static void     ql_queue_cmd(pHA_INFO, scsi_request_t *);
static void     ql_start_scsi(pHA_INFO);
static int 
ql_mbox_cmd(pHA_INFO, u_short *, u_char, u_char,
	    u_short, u_short, u_short, u_short,
	    u_short, u_short, u_short, u_short);
void     munge(u_char * pointer, int count);
int             munge_bp(buf_t * bp, uint len);
int             make_sg(scatter * scatter, u_char * addr, int len, pHA_INFO);

int             ql_init(u_int slot, caddr_t config_addr, paddr_t mem_map_addr,
			void *bus_base);

#if HEART_COHERENCY_WAR
/* flush both memory reads and writes */
#define	MR_DCACHE_WB_INVAL(a,l)		heart_dcache_wb_inval(a,l)
#define	MRW_DCACHE_WB_INVAL(a,l)	heart_write_dcache_wb_inval(a,l)
#define	DCACHE_INVAL(a,l)		heart_dcache_inval(a,l)
#else
#define	MR_DCACHE_WB_INVAL(a,l)
#define	MRW_DCACHE_WB_INVAL(a,l)
#define	DCACHE_INVAL(a,l)
#endif	/* HEART_COHERENCY_WAR || HEART_WRITE_COHERENCY_WAR */


void            qlhinv(u_char * inv, u_char ctlr, u_char targ, u_char lun);

#ifdef DUMP_PARMS
static void     dump_parameters(int board);
#endif
#ifdef DUMP_IOCB
static void     dump_iocb(command_entry * q_ptr);
#endif
#ifdef QL_DEBUG
static void     dump_iorb(status_entry * q_ptr);
static void     dump_marker(marker_entry * q_ptr);
static void     dump_continuation(continuation_entry * q_ptr);
void            dump_area(char *ptr, int count);
#endif
void            add_deltatime(struct timeval * current_time, struct timeval * old_time, struct timeval * add_to_this);



#if 0				/* not used right now */
static void     dump_request(scsi_request_t * request);
#endif


#ifdef DUMP_REGS
static void     dump_registers(pISP_REGS isp);
#endif

void            freesubchannel(scsisubchan_t *);
void            ql_clear_structs(void);

#if defined(IP30)
void		qlconfigure(COMPONENT *,slotinfo_t *);
#elif defined(SN0)
#ifndef SN_PDI
void            qlconfigure(COMPONENT *, char *, char *);
#else
void 		qlconfigure(pd_inst_hdl_t *pdih, pd_inst_parm_t *pdip) ;
#endif
#else
void            qlconfigure(COMPONENT *);
#endif

#ifdef IP30
static void     stand_qlhinv(COMPONENT *, slotinfo_t *);
#endif
static void     ql_poll(void);
void            doscsi(scsisubchan_t * scsisubchan);
static caddr_t  scsi_ecmd(u_char, u_char, u_char, u_char *, int, void *, int);
static int      scsi_rc(u_char, u_char, u_char, struct scsidisk_data *);
scsisubchan_t  *allocsubchannel(int adap, int targ, int lun);
extern scsidev_t *scsidev;
extern int      scsicnt;	/* set to 1 in master.d/scsi, and changed in
				 * IP*.c for machines that * support more
				 * than one */
void            setgioconfig(int slot, int flags);


u_char          scsi_ha_id[SC_MAXADAP];	/* allows users in stand stuff to
					 * check if a particulary id is the
					 * host adapter, rather than
					 * embedding the id in every user of
					 * the driver. For now, host adapter
					 * id is always 0. */


/*
** Use these routines to print debug information.
** CMN_ERR information will print to the console but NOT immediately.
** This information is also printed into the system log.
** Use PRINTx for other (temporary) debug print statements.
** Check the initial value of the variables: ql_exterr and ql_debug.
** You can use a simple ioctl call to disable debug prints this way.
**
** Use PRINTD in initialization routines.
** PRINT1 will only print if the debug print level is 1.
** Use this to print a single character for the operation being perfomed.
** This prints a cryptic set of characters you can use to check flow
** of operations.  If nothing else, you can stare at it for hours and
** impress your friends (talk about a GURU).
** Use PRINT2 for more information (use sparingly).
** I used PRINT3 and 4 only for the mailbox routines (in case its hanging).
*/

#ifdef QL_DEBUG
static int      ql_exterr = 1;
#define CMN_ERR(x)	if (ql_exterr > 0) cmn_err x

int             ql_debug = 4;
int             ql_dump_control_block;

static int      ql_slot_number = 0;	/* this is the number of slots in the
					 * system. */
 /* incremented each call to ql_attach         */

#define PRINTD(x)	if (ql_debug) printf x
#define PRINT1(x)	if (ql_debug > 0) printf x
#define PRINT2(x)	if (ql_debug > 1) printf x
#define PRINT3(x)	if (ql_debug > 2) printf x
#define PRINT4(x)	if (ql_debug > 3) printf x
#define PRINT5(x)	if (ql_debug > 4) printf x

#else				/* !QL_DEBUG */

#define	ql_debug	0

#define CMN_ERR(x)
#define PRINTD(x)
#define PRINT1(x)
#define PRINT2(x)
#define PRINT3(x)
#define PRINT4(x)
#define PRINT5(x)

#endif				/* QL_DEBUG */

int do_verify;			/* keep in .bss for IP30 prom */

#if defined(IP30)
#define MAKE_DIRECT_MAPPED_2GIG(x)	kv_to_bridge32_dirmap((ha->bus_base),x)
#define CLEAR_DIRECT_MAPPED_2GIG(x)	bridge32_dirmap_to_phys((ha->bus_base),x)
#elif defined(SN0)
#define MAKE_DIRECT_MAPPED_2GIG(x)	((x) | 0x80000000 )
#define CLEAR_DIRECT_MAPPED_2GIG(x)	((x) & 0x7fffffff )
#else
#define MAKE_DIRECT_MAPPED_2GIG(x)	(x)
#endif



/**********************************************************
**
**          Global Kernel Information
*/


/**********************************************************
**
**          Local Information
*/

#if IP30
/* Keep malloc size down when we have numberous many xtalk cards, which
 * is reasonable if SC_MAXADAP < 32 or so
 */
#define _STATIC_HAINFO	1
static HA_INFO _ha_info[SC_MAXADAP];
#endif

pHA_INFO ha_info[SC_MAXADAP];
pHA_INFO ha_list;

/* holding place for sense data. */
/* auto sense data is placed here */
/* it is valid only for one command */
/* we can get away with this becuase the prom is single threaded */
static u_char last_sense[REQ_SENSE_DATA_LENGTH];

#ifdef SN0
/*
 * This is a data structure parallel to ha_info. It maintains
 * hardware specific info for SN0 to 'map' DMA and PIOs for
 * multiple Qlogic adapters. This info is filled in
 * during 'install'.
 */

static ULONG    ha_sn0_info[SC_MAXADAP];
int		dksc_map[SC_MAXADAP] ;
int             dksc_inv_map[SC_MAXADAP] ;

#endif


#define         INIT_SPECIFIC   0
#define         INIT_NEXT       1

int             sc_inq_stat;	/* status from scsi_inq() */


/******************************************************************************/


/* Entry-Point
** Function:    ql_init
**
** Description: Initialize all of the ISP's on the PCI bus.
*/
int
ql_init(u_int slot, caddr_t config_addr, paddr_t mem_map_addr, void *bus_base)
{
    static int      inited;
    int             cnt;

#ifdef QL_DEBUG
    PRINT1(("ql_init: entered slot %x config addr %x mem map addr %x\n",
	   slot, config_addr, mem_map_addr));
#endif

/*
** Start with no adapters in the system.
*/

    PRINT3(("ha_info %x\n", ha_info));
    if (!inited)
    {
	PRINT4(("initializing ha_info\n"));
	for (cnt = 0; cnt < SC_MAXADAP; cnt++)
	{
	    ha_info[cnt] = NULL;
	}
	ha_list = NULL;

	inited = 1;
    }
    /* go configured */


    /* * If this Adapter has already been configured in, then ignore * the
     * request. This will take care of duplicates. */
    if (!ha_info[slot])

    {
	/* * Initialize this board. * If this fails, then it won't be marked
	 * as valid (ignore). */
	if (ql_init_board(INIT_SPECIFIC,
			  slot,
			  config_addr,	/* confiuration space address */
			  mem_map_addr,
			  bus_base))
	{

	    cmn_err(CE_WARN, "Encountered a fatal error while initializing QL SCSI controller\n");
	    return (1);
	}
    }
#ifdef QL_DEBUG
    PRINT3(("ql_init: exiting normally\n"));	/* XXX */
    PRINT3(("ql_init: ha_info[0] %x ha_info[1] %x \n", ha_info[0], ha_info[1]));
#endif
    return (0);
}

/* this function was added because the malloc pool is erased at the end of */
/* the ponmore diags */

void 
ql_clear_structs(void)
{
    int             cnt;

#ifdef QL_DEBUG1
    ql_slot_number = 0;		/* clear out the number of slots */
#endif
    for (cnt = 0; cnt < SC_MAXADAP; cnt++)
    {
	PRINT1(("ql_clear_structs: CLEARING ha_info[%x]\n", cnt));
#ifndef _STATIC_HAINFO
	if (ha_info[cnt])
	    free(ha_info[cnt]);
#endif
	ha_info[cnt] = NULL;
    }
    ha_list = NULL;
}

/* Function:    ql_init_board
**
** Description: Initialize specific ISP board.
** Returns: 0 = success
**      1 = failure
*/

static int 
ql_init_board(int init_type,
	      u_short ha_index,
	      caddr_t config_addr,
	      paddr_t mem_map_addr,
	      void *bus_base)
{
    pHA_INFO        ha = NULL;
    paddr_t         Base_Address;
#ifdef QL_DEBUG
    PRINT3(("ql_init_board: init_type=%s, ha_index=%d\n",
	   (init_type == INIT_SPECIFIC) ? "SPECIFIC" : "NEXT", ha_index));
    PRINT1(("ql_init_board: config addr=0x%x, mem addr=0x%x bus_base=0x%x\n",
	   config_addr, mem_map_addr, bus_base));	/* XXX */
#endif

/*
** Make sure the adapter number is valid.
*/
    if (ha_index >= SC_MAXADAP)
    {
#ifdef QL_DEBUG
	CMN_ERR((CE_WARN,
		"ql_init - Invalid adapter number specified [%d]",
		ha_index));
#endif
	return (1);
    }
/*
** If they gave us an address, make sure its our device.
** If not, find the next board.
*/
    switch (init_type)
    {
/*
** Determine if the specified device is present.
*/
    case INIT_SPECIFIC:
	{
	    pISP_REGS       isp = (pISP_REGS) config_addr;
	    u_short         vendor_id = 0;
	    u_short         dev_id = 0;

/*
** Make sure this is an ISP.
*/
	    vendor_id = PCI_INH(&isp->bus_id_low);
	    dev_id = PCI_INH(&isp->bus_id_high);

#ifdef QL_DEBUG
	    PRINT3(("Data at isp address 0x%x: Vendor id=0x%x, Bus id=0x%x\n",
		   isp, vendor_id, dev_id));
#endif

	    /* * If this device is not ours, just leave. */
	    if (vendor_id != QLogic_VENDOR_ID ||
		dev_id != QLogic_DEVICE_ID)
	    {
		/* * Not our board! */
		return (1);
	    }
	    /* * Use the addresses they specified. * Note: this doesn't care
	     * whether this is a memory address * or an I/O address (its
	     * whatever they specified). */

	    Base_Address = mem_map_addr;
	}
	break;


    }


/*
** Allocate and init memory for the adapter structure.
*/
    {
	u_int           id, lun;
	u_int           req_cnt;
	pREQ_INFO       r_ptr;

#ifndef _STATIC_HAINFO
	PRINT1(("allocating ha struct for adapter adapter %x size %x\n", ha_index, sizeof(HA_INFO)));

	ha = (pHA_INFO) malloc(sizeof(HA_INFO));
#else
	ha = &_ha_info[ha_index];
#endif

	bzero(ha, sizeof(HA_INFO));
	ha_info[ha_index] = ha;

	ha->controller_number = ha_index;
	ha->ha_base = (caddr_t) Base_Address;
	PRINT3(("base address set up ha_base is %x\n", ha->ha_base));
	ha->ha_config_base = (caddr_t) config_addr;
#ifdef IP30
	ha->bus_base = bus_base;
#endif
/*
** Clear the state flags.
*/
	ha->flags = 0;

/*
** Put this adapter on the ha_list.
*/
	ha->next_ha = (ha_list) ? ha_list : NULL;
	ha_list = ha;

/*
** Init the raw request queue to empty.
*/
	ha->req_forw = NULL;
	ha->req_back = NULL;

/*
** Clear the device flags for all target lun combinations.
*/
	for (id = 0; id < QL_MAXTARG; id++)
	{
	    ha->dev_init[id] = 0;		/* clears all luns */
	    r_ptr = &ha->reqi_block[id][0];
	    ha->reqi_cntr[id] = 1;
	    for (req_cnt = 0; req_cnt <= (MAX_REQ_INFO); req_cnt++, r_ptr++)
	    {
	   	 r_ptr->req = NULL;
	    }
	}


/*
** Initialize the Req_Info.
*/
      for (id = 0; id < QL_MAXTARG; id++) {
      }
/*
** Note: if device is to be memory mapped, then we have to enable the
** memory (i.e. map it into the available address space).
** Either way, save the address for later accesses.
*/

    }


/*
** Read in the default parameters.
** Provide the memory to store the novram parameters.
*/
    ql_set_defaults(ha);

/*
** Go initialize the ISP.
*/

    if (ql_init_isp(ha))
    {
/*
** Failed to initialize the ISP!
*/
	cmn_err(CE_WARN, "QL SCSI controller initialization failed\n");
#ifndef _STATIC_HAINFO
	free(ha);
#endif
	ha_info[ha_index] = NULL;
	
	return (1);		/* fail */
    }
/*
** We added a parameter that can be used to LOGICALLY
** disable the board.  If they set this flag, then we shouldn't
** publish this adapter board to the SCSI drivers.  However,
** we do need to be able to open the device in order to update
** the parameters to re-enable this board.
** So say we failed the initialization, and check whether the
** actual initialization of the adapter failed (with INITIALIZED
** flag) in open routine.
*/
    if (!ha->ql_defaults.HA_Enable)
    {
	return (1);
    }

/*
** Indicate we found the board
*/
    PRINTD(("Out of ql_init_board\n"));
    return (0);
}



/* Function:    ql_init_isp()
**
** Description: Initialize the ISP chip on a specific board.
** Returns: 0 = success
**      1 = failure
*/
static int 
ql_init_isp(pHA_INFO ha)
{

    u_short         mbox_sts[8];
    pISP_REGS       isp = (pISP_REGS) ha->ha_base;
    pPCI_REG        isp_config = (pPCI_REG) ha->ha_config_base;
    paddr_t         p_ptr;
    u_short         checksum;
    u_short         mailbox0;
    int		    i;
#ifdef IP30
    int             tmp;
#endif
    int             count;
    /*REFERENCED*/
    int             mem_map = 0;
    /*REFERENCED*/
    int		    command = 0;
#ifdef IP30
    bridge_t       *bridge = BRIDGE_K1PTR;
    ate_t           ate;
#endif
    u_short        *w_ptr;

        PCI_OUTH(&isp->bus_icr, 0); /* disable interrupts */
        PCI_OUTH(&isp->hccr, HCCR_CMD_PAUSE);
        PCI_OUTH(&isp->bus_icr,  ICR_SOFT_RESET);
	flushbus();
	DELAY(2);
        PCI_OUTH(&isp->control_dma_control,  DMA_CON_RESET_INT|DMA_CON_CLEAR_CHAN);
        PCI_OUTH(&isp->data_dma_control,  DMA_CON_RESET_INT|DMA_CON_CLEAR_CHAN);
        for (i = 0;i < 1000; i++) {
        	mailbox0 = PCI_INH(&isp->mailbox0);
        	if (mailbox0 != MBOX_STS_BUSY)
                	break;
        	DELAY(1000);
        }

/*
** Setup the busy and idle timers
*/

#ifdef QL_DEBUG
    PRINT3(("ql_init_isp: isp=0x%x\n", isp));	/* XXX */
    PRINT3(("ql_init_isp: host adapter structure address %x\n", ha));
    PRINT3(("ql_init_isp: host adapter base address %x\n", ha->ha_base));
    PRINT3(("ql_init_isp: host adapter configuration base address %x\n",
	   isp_config));
    PRINT3(("ql_init_isp: isp addresses at %x\n", isp));
#endif

#ifndef EVEREST
    /* write the mapping register, enable mapping */
#ifndef IP30
    PCI_OUTW(&isp_config->Memory_Base_Address,
	     (int) ((kvtophys) (ha->ha_base)));
#endif
#endif				/* EVEREST */
    mem_map = PCI_INW(&isp_config->Memory_Base_Address);

    PRINT1(("memory mapped to %x at %x\n", mem_map,
	   &isp_config->Memory_Base_Address));

    PCI_OUTH(&isp_config->Command, MEMORY_SPACE_ENABLE | BUS_MASTER_ENABLE);
    command = PCI_INH(&isp_config->Command);
    PRINT3(("Command reg set to %x at memory address %x\n",
	   command, &isp_config->Command));

/* set data directions, burst enable, interrupt enable */
/*	PRINT2(("data_dma_config at %x\n",&isp->data_dma_config)); */
/* PCI_OUTH(&isp->data_dma_config,DMA_DATA_DIRECTION|BURST_ENABLE|INTERRUPT_ENABLE);*/
    /* data_dma = PCI_INH(&isp->data_dma_config); */

    PCI_OUTB(&isp_config->Latency_Timer, 0x40);
    PCI_OUTB(&isp_config->Cache_Line_Size, 0x40);

    PRINT3(("&isp_config->Latency_Timer %x\n", &isp_config->Latency_Timer));


    PCI_OUTH(&isp->bus_icr, ICR_ENABLE_RISC_INT | ICR_SOFT_RESET);
    DELAY(1000);
    PRINTD(("reset the controller\n"));
    PCI_OUTH(&isp->hccr, HCCR_CMD_RESET);
    PCI_OUTH(&isp->hccr, HCCR_CMD_RELEASE);
    PCI_OUTH(&isp->hccr, HCCR_WRITE_BIOS_ENABLE);


#ifdef CLEAR_INTR_FIRST

    {
#ifdef QL_DEBUG
	PRINTD(("ql_init_isp: Disabling hardware interrupts ...\n"));
#endif

/*
** Clear the interrupt allows (don't want any interrupts yet).
** We do this by only enabling the RISC interrupts (and not
** enabling the other interrupts).
*/
	PCI_OUTH(&isp->bus_icr, ICR_ENABLE_RISC_INT);
    }

#endif				/* CLEAR_INTR_FIRST */

#ifdef SOFT_RESET

/*
** Perform a soft reset of the ISP chip.
** If there is any firmware running, the soft reset will stop
** its execution (this would be firmware loaded at POST time).
*/
    {
	u_short         mailbox0;

#ifdef QL_DEBUG
	PRINTD(("ql_init_isp: Issuing a soft reset ...\n"));
#endif
/*
** To do a soft reset we reset the interface,
** reset the RISC, and then release the RISC.
** We also have to disable the BIOS (manual not clear about this).
*/
	PCI_OUTH(&isp->bus_icr, ICR_ENABLE_RISC_INT | ICR_SOFT_RESET);
	/* XXX need delay here */
	PRINTD(("enabled risc interrupt and soft reset %x\n",
	       PCI_INH(&isp->bus_icr)));
	PCI_OUTH(&isp->hccr, HCCR_CMD_RESET);
	PCI_OUTH(&isp->hccr, HCCR_CMD_RELEASE);
	PCI_OUTH(&isp->hccr, HCCR_WRITE_BIOS_ENABLE);

#ifdef QL_DEBUG
	PRINTD(("ql_init_isp: ISP reset ...\n"));
#endif
/*
** Wait for mailbox 0 to clear.
** This means that the self test code has completed
** and its ready to accept mailbox commands.
*/
	do
	{
/*
** Get a copy of the mailbox0.
*/
	    mailbox0 = PCI_INH(&isp->mailbox0);

	} while (mailbox0);


#ifdef QL_DEBUG
	PRINTD(("ql_init_isp: Starting firmware ...\n"));
#endif
/*
** Start ISP firmware.
*/
	if (ql_mbox_cmd(ha, mbox_sts, 2, 1,
			MBOX_CMD_EXECUTE_FIRMWARE,
			risc_code_addr01,
			0, 0, 0, 0, 0, 0))
	{
#ifdef QL_DEBUG
	    CMN_ERR((CE_WARN,
		    "ql_init_isp - EXECUTE FIRMWARE Command Failed\n"));
#endif
	    return (1);
	}
#ifdef QL_DEBUG
	PRINTD(("ql_init_isp: Firmware running ...\n"));
#endif
    }

#else				/* SOFT_RESET */

/*
** If we're NOT going to do a soft reset, then we need to
** load up some new firmware of our own.  This is the perferred
** choice in that we can update the firmware by shipping a new
** software driver (i.e. the new firmware is imbedded in this
** code).
*/

#ifdef CHECK_HCCR_FIRST

/*
** Check the HCCR to see if there is any firmware running.
** I'm not sure if this is really necessary.
*/
    {
	u_short         hccr;

/*
** Get a copy of the Host Command and Config register.
** Is there firmware running?
*/
	hccr = PCI_INH(&isp->hccr);

/*
** If the RISC is not paused and not reset, then its running.
*/
	if (!(hccr & (HCCR_RESET | HCCR_PAUSE)))
	{
#ifdef QL_DEBUG
	    PRINTD(("ql_init_isp: RISC running, stopping ...\n"));
#endif

/*
** Stop the current ISP firmware and begin execution
** in the internal ISP ROM firmware.
*/
	    PRINT1(("about to stop firmware\n"));
	    (void) ql_mbox_cmd(ha, mbox_sts, 1, 6,
			       MBOX_CMD_STOP_FIRMWARE,
			       0, 0, 0, 0, 0, 0, 0);
#ifdef QL_DEBUG
	    PRINTD(("ql_init_isp: Firmware stopped [%x, %x]\n",
		   mbox_sts[1], mbox_sts[2]));
#endif
	}
    }

#endif				/* CHECK_HCCR_FIRST */

#ifdef LOAD_FIRMWARE

/*
** If we're going to load our own firmware, we need to
** stop any currently running firmware (if any).
*/
    {
#ifdef QL_DEBUG
	PRINTD(("ql_init_isp: Stopping firmware if running any...\n"));
	PRINT2(("ql_init_isp: Error will occur if no firmware is running\n"));

#endif

#ifdef MAILBOX_TEST
	PRINTD(("doing the mbox test: "));
	if (ql_mbox_cmd(ha, mbox_sts, 8, 8,
			MBOX_CMD_MAILBOX_REGISTER_TEST,
			(ushort) 0x1111,
			(ushort) 0x2222,
			(ushort) 0x3333,
			(ushort) 0x4444,
			(ushort) 0x5555,
			(ushort) 0x6666,
			(ushort) 0x7777))
	{
	    CMN_ERR((CE_WARN,
		    "ql_init_isp - MAILBOX REGISTER TEST Command Failed"));
	    return (1);

	} else
        {
/*
** Do they match?
*/
            if (    mbox_sts[1] != (ushort)0x1111 ||
                    mbox_sts[2] != (ushort)0x2222 ||
                    mbox_sts[3] != (ushort)0x3333 ||
                    mbox_sts[4] != (ushort)0x4444 ||
                    mbox_sts[5] != (ushort)0x5555 ||
	            mbox_sts[6] != (ushort)0x6666 )

            {
                 PRINTD(("qlIS_init_isp - mailbox compare error\n"));
                 PRINTD(("    %x, %x, %x, %x, %x, %x\n",
                         mbox_sts[0], mbox_sts[1], mbox_sts[2],
                         mbox_sts[3], mbox_sts[4], mbox_sts[5]));
		 return(1);
            }
            PRINTD(("qlIS_init_isp - mailbox compare test passed!!\n"));
	}

#endif

/*
** Stop the current ISP firmware and begin execution
** in the internal ISP ROM firmware.
*/
/*		(void)ql_mbox_cmd(ha, mbox_sts, 1, 6,
		    MBOX_CMD_STOP_FIRMWARE,
		    0,0,0,0,0,0,0);
*/
#ifdef QL_DEBUG
	PRINTD(("ql_init_isp: Firmware stopped [%x, %x]\n",
	       mbox_sts[1], mbox_sts[2]));
#endif
    }
#endif				/* LOAD_FIRMWARE */

#endif				/* SOFT_RESET */

/*
** At this point, the RISC is running, but not running any loaded firmware.
** We can now either load new firmware or restart the previously loaded
** firmware.
*/
#ifdef DUMP_REGS
    /* dump_registers(isp);  XXX */
#endif


/*
** Initialize the Control Parameter configuration.
*/
    {
	unsigned int    Config_Reg;

/*
** Build the value for the ISP Configuration 1 Register.
*/
	Config_Reg = ha->ql_defaults.Fifo_Threshold;

#ifdef ENABLE_BURST
/*
** always set burst enable
*/
	Config_Reg |= CONF_1_BURST_ENABLE;
#endif	/* ENABLE_BURST */

#ifdef QL_DEBUG
	PRINT3(("ql_init_isp: Initializing the hardware registers ...\n"));
#endif

	PCI_OUTH(&isp->hccr, HCCR_CMD_PAUSE);	/* do I need to do this? */
	PCI_OUTH(&isp->bus_config1, Config_Reg);
	PRINT3(("set config for burst i think config=%x at %x\n", Config_Reg, &isp->bus_config1));
	/* PCI_OUTH(&isp->bus_icr, 0); XXX */
	PCI_OUTH(&isp->bus_sema, 0);

	PCI_OUTH(&isp->hccr, HCCR_CMD_RELEASE);	/* and this? */
    }

#ifdef LOAD_FIRMWARE

/*
** Okay, we're ready to load our own firmware.
** We have two ways of doing this: DMA or word by word.
** I've left both version in because it may help to bring
** up a a new system.
*/
    {
#ifdef QL_DEBUG
	PRINTD(("ql_init_isp: Loading firmware ...\n"));
#endif

#ifdef LOAD_VIA_DMA
	{


/*XXX for emulation - load the code from the prom into physical memory */

	    ushort         *firmware, *prom_firmware;
	    /*REFERENCED*/
	    ushort	   *tmp;
	    ushort          count;
#ifdef SWAP_DATA_STREAM
	    u_char         *charp;
	    ushort          holder;
#endif

#ifdef SWAP_DPROM
	    static int      swapped = 0;
	    ushort          temp;

	    PRINT1(("before swap %x \n", *((int *) risc_code01)));
	    firmware = (ushort *) risc_code01;
	    tmp = firmware;
	    tmp++;
	    PRINT1(("prom_array = %x sizeof(risc code) %x\n",
		   firmware, sizeof(risc_code01)));

	    for (count = 0; count < sizeof(risc_code01) / 2; count++)
	    {
		if (swapped)
		    continue;
		temp = *firmware;
		*firmware = *tmp;
		*tmp = temp;
		firmware++;
		firmware++;
		tmp++;
		tmp++;
	    }

	    swapped = 1;
	    tmp = risc_code01;
	    PRINT1(("after swap %x \n", *((int *) risc_code01)));

	    firmware = risc_code01;

#else				/* copy it locally and then swap it */

	    prom_firmware = (ushort *) risc_code01;
	    firmware = (ushort *) dmabuf_malloc(4 + sizeof(risc_code01));
	    tmp = firmware;
	    PRINT1(("risc_code %x firmware %x tmp %x prom_f %x count %x\n",
		   risc_code01, firmware, tmp, prom_firmware,
		   risc_code_length01 / sizeof(ushort)));

#ifndef SWAP_DATA_STREAM
	    for (count = 0; count < 2 + risc_code_length01 / sizeof(ushort); count++)
	    {
		*(tmp + 1) = *(prom_firmware);
		*(tmp) = *(prom_firmware + 1);
		tmp++;
		tmp++;
		prom_firmware++;
		prom_firmware++;
	    }
#else
	    charp = (u_char *) firmware;
	    for (count = 0; count < 2 + risc_code_length01; count++)
	    {
		holder = *(prom_firmware++);
		*charp++ = (holder & 0xff);
		*charp++ = ((holder >> 8) & 0xff);
	    }

#endif				/* SWAP_DATA_STREAM */

	    tmp = firmware;
	    PRINT1(("\n"));

	    for (count = 0; count < 24; count++)
		PRINT1(("%x ", *((ushort *) tmp++)));

	    PRINT1(("\ncopy complete, first word dma copy %x prom copy %x\n",
		   *(int *) firmware, *(int *) risc_code01));
	    tmp = firmware;

#endif

	    MR_DCACHE_WB_INVAL(firmware, 4 + sizeof(risc_code01));

/*
** Load my version of the executable firmware by DMA.
*/
	    p_ptr = MAKE_DIRECT_MAPPED_2GIG(kvtophys(firmware));
#if 0
#ifdef SN0_PCI_64
	    p_ptr = get_pci64_dma_addr(firmware,
				       ha_sn0_info[ha->controller_number]);
#endif
#endif

	    PRINTD(("loading via DMA at %x\n", p_ptr));

#ifdef A64_BIT_OPERATION1 
	    if (ql_mbox_cmd(ha, mbox_sts, 5, 5,
			    MBOX_CMD_LOAD_RAM_64,
			    (short) risc_code_addr01,
			    (short) ((u_long) p_ptr >> 16),
			    (short) ((u_long) p_ptr & 0xFFFF),
			    risc_code_length01,
			    0,
			    (short) ((u_long) p_ptr >> 48),
			    (short) ((u_long) p_ptr >> 32)))
#else 
	    if (ql_mbox_cmd(ha, mbox_sts, 5, 5,
			    MBOX_CMD_LOAD_RAM,
			    (short) risc_code_addr01,
			    (short) ((u_int) p_ptr >> 16),
			    (short) ((u_int) p_ptr & 0xFFFF),
			    risc_code_length01,
			    0, 0, 0))
#endif				/* A64_BIT_OPERATION */
	    {
		CMN_ERR((CE_WARN,
			"ql_init_isp - LOAD RAM Command Failed"));
		debug("mailbox command for dma load");
		return (1);
	    } else
		PRINT1(("DMA of code complete\n"));
	    PRINT1(("after dma\n"));
	    free(firmware);
	}


/* go get the first 10 and print them out */
	if(ql_debug) for (count = 0; count < 10; count++)
	{
	    if (ql_mbox_cmd(ha, mbox_sts, 2, 3,
			    MBOX_CMD_READ_RAM_WORD,
			    (u_short) (risc_code_addr01 + count),	/* risc addr */
			    0, 0, 0, 0, 0, 0))
	    {
		CMN_ERR((CE_WARN,
			"ql_init_isp - READ RAM WORD Command Failed [%x]",
			risc_code_addr01 + count));
		break;
	    } else
		PRINTD(("%x:%x  ", count, mbox_sts[2]));

	}
	PRINTD(("\n"));


#else				/* LOAD_VIA_DMA */

/*
** Load my version of the executable firmware one word at a time.
*/
	w_ptr = &risc_code01[0];
	checksum = 0;
	PRINT3(("load one word at a time\n"));
	for (count = 0; count < risc_code_length01; count++)
	{
/*XXX for emulation */
#ifdef QL_DEBUG
	    if (!(count % 0x100))
		PRINT1(("%x ", count));
	    /* PRINTD(("%x\r", count)); */
#endif
	    if (ql_mbox_cmd(ha, mbox_sts, 3, 3,
			    MBOX_CMD_WRITE_RAM_WORD,
			    (u_short) (risc_code_addr01 + count),	/* risc addr */
			    *w_ptr,	/* risc data */
			    0, 0, 0, 0, 0))
	    {
		printf("ql_init_isp - WRITE RAM WORD Command Failed [%x]:%x",
		       risc_code_addr01 + count, *w_ptr);
		return (1);
	    }
/*
** Total up the checksum as we go.
*/
	    checksum += *w_ptr++;
	}

#endif				/* LOAD_VIA_DMA */

#ifdef QL_DEBUG
	PRINTD(("\nql_init_isp: Firmware loaded ...\n"));
	PRINTD(("ql_init_isp: Starting firmware ...\n"));
#endif
	/* dump_registers(isp); */
/*
** Start ISP firmware.
*/
	if (ql_mbox_cmd(ha, mbox_sts, 2, 1,
			MBOX_CMD_EXECUTE_FIRMWARE,
			risc_code_addr01,
			0, 0, 0, 0, 0, 0))
	{
	    CMN_ERR((CE_WARN,
		    "ql_init_isp - EXECUTE FIRMWARE Command Failed"));
	    return (1);
	}
	if (ql_mbox_cmd(ha, mbox_sts, 1, 3,
			MBOX_CMD_ABOUT_FIRMWARE,
			0, 0, 0, 0, 0, 0, 0))
	{
	    CMN_ERR((CE_WARN,
		    "ql_init_isp - EXECUTE FIRMWARE Command Failed"));
	}
	PRINTD(("\nql_init_isp: Firmware running : version %d.%d\n",
	       mbox_sts[1], mbox_sts[2]));
    }
#endif				/* LOAD_FIRMWARE */

#if 0
    {

	ushort         *firmware, *tmp;
	paddr_t         p_ptr;
	int             count;
	firmware = (ushort *) dmabuf_malloc(4 + sizeof(risc_code01));
	DCACHE_INVAL(firmware, 4 + sizeof(risc_code01));

	p_ptr = MAKE_DIRECT_MAPPED_2GIG(kvtophys(firmware));
#ifdef SN0_PCI_64
	p_ptr = get_pci64_dma_addr(firmware,ha_info[ha->controller_number]);
#endif

	PRINTD(("getting firmware via DMA at %x\n", p_ptr));
#ifdef A64_BIT_OPERATION
	if (ql_mbox_cmd(ha, mbox_sts, 5, 5,
			MBOX_CMD_DUMP_RAM,
			(short) risc_code_addr01,
			(short) ((u_long) p_ptr >> 16),
			(short) ((u_long) p_ptr & 0xFFFF),
			risc_code_length01,
			0,
			(short) ((u_long) p_ptr >> 48),
			(short) ((u_long) p_ptr >> 32)))
#else
	if (ql_mbox_cmd(ha, mbox_sts, 5, 5,
			MBOX_CMD_DUMP_RAM,
			(short) risc_code_addr01,
			(short) ((u_int) p_ptr >> 16),
			(short) ((u_int) p_ptr & 0xFFFF),
			risc_code_length01,
			0, 0, 0))
#endif				/* A64_BIT_OPERATION */
	{
	    CMN_ERR((CE_WARN,
		    "ql_init_isp - DUMP RAM Command Failed"));
	    return (1);
	} else
	    PRINT1(("DMA DUMP of code complete\n"));
	tmp = firmware;
	for (count = 0; count < 2 + sizeof(risc_code01) / sizeof(short); count++)
	    PRINT1(("%x ", *((ushort *) tmp++)));

    }

#endif

#ifdef MAILBOX_TEST

/*
** This is a test to be used to bring up a new implementation.
** It determines if we can play with the mailbox registers.
**
** Can we write something into the mailbox registers and
** get the (now running) firmware to copyit back?
*/
    {
	if (ql_mbox_cmd(ha, mbox_sts, 8, 8,
			MBOX_CMD_MAILBOX_REGISTER_TEST,
			(ushort) 0x1111,
			(ushort) 0x2222,
			(ushort) 0x3333,
			(ushort) 0x4444,
			(ushort) 0x5555,
			(ushort) 0x6666,
			(ushort) 0x0))
	{
	    CMN_ERR((CE_WARN,
		    "ql_init_isp - MAILBOX REGISTER TEST Command Failed"));
	    return (1);
	} else
	{
/*
** Do they match?
*/
	    if (mbox_sts[1] != (ushort) 0x1111 ||
		mbox_sts[2] != (ushort) 0x2222 ||
		mbox_sts[3] != (ushort) 0x3333 ||
		mbox_sts[4] != (ushort) 0x4444 ||
		mbox_sts[5] != (ushort) 0x5555 ||
		mbox_sts[6] != (ushort) 0x6666 )
	    {
		printf("ql_init_isp - mailbox compare error\n");
		printf("    %x, %x, %x, %x, %x, %x, %x, %x\n",
		       mbox_sts[0], mbox_sts[1], mbox_sts[2],
		       mbox_sts[3], mbox_sts[4], mbox_sts[5],
		       mbox_sts[6]);
		return(1);
	    }
	    PRINTD(("ql_init_isp - mailbox compare test passed!!\n"));
	}
    }

#endif				/* MAILBOX_TEST */

#ifdef CHECKSUM_TEST
/* calculate the checksum and compare it with what the controller       */
/* thinks its calculated checksum is.					*/
/* if it is not the same then check the firmware word by word until     */
/* a mismatch is found							*/
/*									*/
/* I suppose that it is conceivable that a word by word check could	*/
/* succeed and the MBOX_CMD_VERIFY_CHECKSUM command could fail		*/
/* in this case believe the word by word check and continue without   	*/
/* failing the controller setup						*/

    {
        w_ptr = &risc_code01[0];
        checksum = 0;
        count = 0;

        for (count = 0; count < risc_code_length01; count++)
        {
            checksum += *w_ptr++;
        }


/*
** Get the current checksum value.
*/
        if (ql_mbox_cmd(ha, mbox_sts, 2, 3,
                        MBOX_CMD_VERIFY_CHECKSUM,
                        0x1000, /* start at location 0 */
                        0, 0, 0, 0, 0, 0))
        {
            CMN_ERR((CE_WARN,
                    "ql_init_isp - VERIFY CHECKSUM Command Failed"));
            return(1);
        }
	if( mbox_sts[2] != checksum) 
	{
            printf("ISP Firmware Checksum mismatch: theirs %x, mine = %x\n",
               mbox_sts[2], checksum);
	    do_verify = 1;
	}
	else
	{
	    PRINTD(("PRINTD checksum is equal theirs %x, mine = %x\n",
               mbox_sts[2], checksum));
	}
    }

#endif /* CHECKSUM_TEST */

#if defined(VERIFY_TEST) || defined(CHECKSUM_TEST)

/*
** This test can used to determine if the firmware loaded
** matches what we tried to load.
** This would be used to bring up a new implementation.
*/
#ifndef VERIFY_TEST
	if(do_verify)
#endif
	{
/*
** Dump the executable firmware.
*/
	w_ptr = &risc_code01[0];
	count = 0;

	PRINTD(("VERIFY THE FIRWARE WITH LOCAL COPY\n"));
	for (count = 0; count < risc_code_length01; count++)
	{

	    if (ql_mbox_cmd(ha, mbox_sts, 2, 3,
			    MBOX_CMD_READ_RAM_WORD,
			    (u_short) (risc_code_addr01 + count),	/* risc addr */
			    0, 0, 0, 0, 0, 0))
	    {
#ifdef QL_DEBUG
		CMN_ERR((CE_WARN,
			"ql_init_isp - READ RAM WORD Command Failed [%x]",
			risc_code_addr01 + count));
#endif
		break;
	    }
	    if (mbox_sts[2] != *w_ptr++) {
                printf("Firmware Data Mismatch  [0x%x]: his = %x, mine = %x\n", count, mbox_sts[2], *w_ptr);
		return(1);
	    }
	}
    }

#endif				/* VERIFY_TEST */



/*
** At this point, the firmware is up and running and we're ready to
** initialze for normal startup.  If you are bringing up the ISP
** device for the first time, leave normal_startup undefined until
** you have worked out the bugs of getting the firmware loaded and
** running.
*/

#ifdef NORMAL_STARTUP

/*
** Set Bus Control Parameters.
*/
    if (ql_mbox_cmd(ha,
		    mbox_sts, 3, 3,
		    MBOX_CMD_SET_BUS_CONTROL_PARAMETERS,
     (u_short) (ha->ql_defaults.Capability.bits.Data_DMA_Burst_Enable << 1),
		    (u_short) (ha->ql_defaults.Capability.bits.Command_DMA_Burst_Enable << 1),
		    0, 0, 0, 0, 0))
    {
	CMN_ERR((CE_WARN,
		"ql_init_isp - SET BUS CONTROL PARAMETERS Command Failed"));
	return (1);
    }
/* Set the Clock Rate for the board */

    {
	u_short         clock_rate;

	clock_rate = FAST20_CLOCK_RATE;

	PRINTD(("ql_init_isp - Informing board of %d Mhz CLOCK RATE \n",
	       clock_rate));

	if (ql_mbox_cmd(ha,
			mbox_sts, 2, 2,
			MBOX_CMD_SET_CLOCK_RATE,
			clock_rate,
			0, 0, 0, 0, 0, 0))
	{
	    CMN_ERR((CE_WARN,
		    "ql_init_isp - SET CLOCK RATE Command Failed"));
	    return (1);
	}
    }


/*
** Initialize response queue indexes.
*/
#ifdef USE_MAPPED_CONTROL_STREAM
#define BUFFER_MALLOC(size)	align_malloc(size,MIN(size,1024*16))
#else
#define BUFFER_MALLOC(size)	dmabuf_malloc(size)
#endif
    ha->response_base = (queue_entry *)
	BUFFER_MALLOC(sizeof(queue_entry) * RESPONSE_QUEUE_DEPTH);
    if (ha->response_base == NULL)
    {
	cmn_err(CE_WARN, "could not malloc space for response queue\n");
	return (1);		/* FAIL */
    }
    PRINT1(("response_base is %x\n", ha->response_base));
    ha->response_ptr = ha->response_base;
    PRINTD(("response queue base %x phys is %x\n", ha->response_base,
	   kvtophys(ha->response_base)));	/* XXX */

    ha->response_out = 0;
#ifdef USE_MAPPED_CONTROL_STREAM
/* XXX hack for emulation */
/* this is for programming the bridge to put the control stream in mapped
** space
**   step 1: make p_ptr an address that will access mapped area
**   step 2: make the Address Translation Entry (ATE) and put it in first entry
**   note: this is a hack for now because no routines are written yet.
**         this is designed to work on the emulator (one ql card)
*/
/* put the ATE entry number ored in with the start of mapped space */

#ifdef IP30			/* XXX this is only for emulation and should
				 * be taken out */
    /* XXX as soon at the PCI infratructure stuff goes in */
#define MAKE_PMU_ADDRESS(entry_number) ((entry_number<<14) | 0x40000000)
#define MAKE_ATE(mode,pfn) ((mode)|(pfn<<12)|(0x8<<8))
#endif				/* IP30 */
#ifndef SN0
    p_ptr = MAKE_PMU_ADDRESS(0x0);
    PRINT1(("response base for controller is %x\n", p_ptr));
    PRINT1(("setting up ATE for response buffer\n"));
    {
	volatile bridge_ate_t   *wrp = &bridge->b_int_ate_ram[0].wr;
#if 0
	volatile bridgereg_t    *lop = &bridge->b_int_ate_ram_lo[0].rd;
	volatile bridgereg_t    *hip = &bridge->b_int_ate_ram[0].hi.rd;
	PRINT1(("Value (at 0x%x and 0x%x) before writing:", hip, lop));
	PRINT1((" hi=0x%x lo=0x%x\n", *hip, *lop));
	PRINT1(("about to write ATE at 0x%x\n", wrp));
#endif
	*wrp = MAKE_ATE(ATE_V | ATE_CO, kvtophys(ha->response_base) >> 12);
#if 0
	PRINT1(("Wrote ATE at 0x%x; now reading back from 0x%x and 0x%x:", wrp, hip, lop));
	PRINT1((" hi=0x%x lo=0x%x\n", *hip, *lop));
#endif
    }
*/
#else
    p_ptr = MAKE_DIRECT_MAPPED_2GIG(kvtophys(ha->response_ptr));
#endif				/* SN0 */
#else
    p_ptr = MAKE_DIRECT_MAPPED_2GIG(kvtophys(ha->response_ptr));

#ifdef SN0_PCI_64
    p_ptr = (paddr_t) get_pci64_dma_addr((__psunsigned_t) ha->response_ptr,
				      ha_sn0_info[ha->controller_number]);
#endif
    PRINTD(("response q base : %x\n", p_ptr));

#endif				/* IP30 */
    PRINTD(("response queue base %x, kvtophys %x\n", ha->response_base, p_ptr));

    bzero(ha->response_base, RESPONSE_QUEUE_DEPTH * sizeof(queue_entry));
    MRW_DCACHE_WB_INVAL(ha->response_base,
	RESPONSE_QUEUE_DEPTH * sizeof(queue_entry));



/*
** Initialize response queue.
*/
#ifdef A64_BIT_OPERATION
    if (ql_mbox_cmd(ha, mbox_sts, 8, 8,
		    MBOX_CMD_INIT_RESPONSE_QUEUE_64,
		    RESPONSE_QUEUE_DEPTH,
		    (short) ((u_long) p_ptr >> 16),
		    (short) ((u_long) p_ptr & 0xFFFF),
		    0,
		    ha->response_out,
		    (short) ((u_long) p_ptr >> 48),
		    (short) ((u_long) p_ptr >> 32)))
#else
    if (ql_mbox_cmd(ha, mbox_sts, 6, 6,
		    MBOX_CMD_INIT_RESPONSE_QUEUE,
		    RESPONSE_QUEUE_DEPTH,
		    (short) ((u_int) p_ptr >> 16),
		    (short) ((u_int) p_ptr & 0xFFFF),
		    0,
		    ha->response_out, 0, 0))
#endif				/* A64_BIT_OPERATION */
    {
	CMN_ERR((CE_WARN,
		"ql_init_isp - INIT RESPONSE QUEUE Command Failed"));
	return (1);
    }
#ifdef QL_DEBUG
    PRINTD(("ql_init_isp - INIT RESPONSE QUEUE results\n"));
    PRINTD(("    %x, %x, %x, %x, %x, %x %x %x\n",
	   mbox_sts[0], mbox_sts[1], mbox_sts[2], mbox_sts[3],
	   mbox_sts[4], mbox_sts[5], mbox_sts[6], mbox_sts[7]));	/* XXX */
#endif

/*
** Initialize request queue indexes.
*/

    ha->queue_space = 0;
    ha->request_in = 0;
    ha->request_out = 0;

    ha->request_base = (queue_entry *)
	BUFFER_MALLOC(sizeof(queue_entry) * REQUEST_QUEUE_DEPTH);
    if (ha->request_base == NULL)
    {
	cmn_err(CE_WARN, "could not malloc space for request queue\n");
	return (1);		/* FAIL */
    }
    PRINT1(("request_base %x\n", ha->request_base));
    ha->request_ptr = ha->request_base;
    PRINT1(("request queue base %x phys is %x\n", ha->request_base,
	   kvtophys(ha->request_base)));	/* XXX */

    ha->request_out = 0;

#if defined(USE_MAPPED_CONTROL_STREAM) && defined(IP30)
/* XXX hack for emulation */
/* this is for programming the bridge to put the control stream in mapped
** space
**   step 1: make p_ptr an address that will access mapped area
**   step 2: make the Address Translation Entry (ATE) and put it in first entry
**   note: this is a hack for now because no routines are written yet.
**         this is designed to work on the emulator (one ql card)
*/
/* put the ATE entry number ored in with the start of mapped space */


    p_ptr = MAKE_PMU_ADDRESS(0x1);
    PRINT1(("request base for controller is %x\n", p_ptr));
    PRINT1(("setting up ATE for request buffer\n"));
    {
	volatile bridge_ate_t   *wrp = &bridge->b_int_ate_ram[1].wr;
#if 0
	volatile bridgereg_t    *lop = &bridge->b_int_ate_ram_lo[1].rd;
	volatile bridgereg_t    *hip = &bridge->b_int_ate_ram[1].hi.rd;
	PRINT1(("Value (at 0x%x and 0x%x) before writing:\n", hip, lop));
	PRINT1(("\thi=0x%x lo=0x%x\n", *hip, *lop));
	PRINT1(("about to write ATE at 0x%x\n", wrp));
#endif
	*wrp = MAKE_ATE(ATE_V | ATE_CO, kvtophys(ha->request_base) >> 12);;
#if 0
	PRINT1(("Wrote ATE at 0x%x; now reading back from 0x%x and 0x%x:\n", wrp, hip, lop));
	PRINT1(("\thi=0x%x lo=0x%x\n", *hip, *lop));
#endif
    }
#else
    p_ptr = MAKE_DIRECT_MAPPED_2GIG(kvtophys(ha->request_base));

#ifdef SN0_PCI_64
    p_ptr = (paddr_t) get_pci64_dma_addr((__psunsigned_t) ha->request_base,
				      ha_sn0_info[ha->controller_number]);
#endif
    PRINTD(("mapped request q base : %x\n", p_ptr));

#endif				/* IP30 */

    PRINT3(("request queue base %x, kvtophys %x\n", ha->request_base, p_ptr));
    bzero(ha->request_base, REQUEST_QUEUE_DEPTH * sizeof(queue_entry));

/*
** Initialize request queue.
*/
#ifdef A64_BIT_OPERATION
    if (ql_mbox_cmd(ha, mbox_sts, 8, 8,
		    MBOX_CMD_INIT_REQUEST_QUEUE_64,
		    REQUEST_QUEUE_DEPTH,
		    (short) ((u_int) p_ptr >> 16),
		    (short) ((u_int) p_ptr & 0xFFFF),
                    0,
                    ha->request_in,
                    (short) ((u_long) p_ptr >> 48),
                    (short) ((u_long) p_ptr >> 32)))

#else
    if (ql_mbox_cmd(ha, mbox_sts, 5, 6,
		    MBOX_CMD_INIT_REQUEST_QUEUE,
		    REQUEST_QUEUE_DEPTH,
		    (short) ((u_long) p_ptr >> 16),
		    (short) ((u_long) p_ptr & 0xFFFF),
		    ha->request_in,
		    0,
		    (short) ((u_long) p_ptr >> 48),
		    (short) ((u_long) p_ptr >> 32)))
#endif				/* A64_BIT_OPERATION */

    {
	CMN_ERR((CE_WARN,
		"ql_init_isp - INIT REQUEST QUEUE Command Failed"));
	return (1);
    }
#ifdef QL_DEBUG
    PRINTD(("ql_init_isp - INIT REQUEST QUEUE results\n"));
    PRINTD(("    %x, %x, %x, %x, %x, %x %x %x\n",
	   mbox_sts[0], mbox_sts[1], mbox_sts[2], mbox_sts[3],
	   mbox_sts[4], mbox_sts[5], mbox_sts[6], mbox_sts[7]));	/* XXX */
#endif


/*
** Set Initiator SCSI ID.
*/
    if (ql_mbox_cmd(ha, mbox_sts, 2, 2,
		    MBOX_CMD_SET_INITIATOR_SCSI_ID,
		    (u_short) ha->ql_defaults.Initiator_SCSI_Id,
		    0, 0, 0, 0, 0, 0))
    {
	CMN_ERR((CE_WARN,
		"ql_init_isp - SET INITIATOR SCSI ID Command Failed"));
	return (1);
    }

/*
** Set Selection Timeout.
*/
    if (ql_mbox_cmd(ha, mbox_sts, 2, 2,
		    MBOX_CMD_SET_SELECTION_TIMEOUT, 
		    (u_short) ha->ql_defaults.Selection_Timeout,
		    0, 0, 0, 0, 0, 0))
    {
	cmn_err(CE_WARN,
		"ql_init_isp - SET_SELECTION_TIMEOUT Command Failed");
    } else
    {
	PRINTD(("scsi_seltimeout:  selection timeout adapter %d is now %dmS, was %dmS\n", adap, timeout, mbox_sts[1]));
    }

/*
** Set RETRY limits.
*/
    if (ql_mbox_cmd(ha, mbox_sts, 3, 3,
		    MBOX_CMD_SET_RETRY_COUNT,
		    ha->ql_defaults.Retry_Count,
		    ha->ql_defaults.Retry_Delay,
		    0, 0, 0, 0, 0))
    {
	CMN_ERR((CE_WARN,
		"ql_init_isp - SET RETRY COUNT Command Failed"));
	return (1);
    }
#ifdef  TAGGED_CMDS

/*
** Set Tag Age Limits.
*/
    if (ql_mbox_cmd(ha, mbox_sts, 2, 2,
		    MBOX_CMD_SET_TAG_AGE_LIMIT,
		    ha->ql_defaults.Tag_Age_Limit,
		    0, 0, 0, 0, 0, 0))
    {
	CMN_ERR((CE_WARN,
		"ql_init_isp - SET TAG AGE LIMIT Command Failed"));
	return (1);
    }
#endif				/* TAGGED_CMDS */


/*
** Set Active Negation.
*/
    if (ql_mbox_cmd(ha,
		    mbox_sts, 2, 2,
		    MBOX_CMD_SET_ACTIVE_NEGATION_STATE,
    (u_short) ((ha->ql_defaults.Capability.bits.REQ_ACK_Active_Negation << 5)
	| (ha->ql_defaults.Capability.bits.DATA_Line_Active_Negation << 4)),
		    0, 0, 0, 0, 0, 0))
    {
#ifdef QL_DEBUG
	CMN_ERR((CE_WARN,
		"ql_init_isp - SET ACTIVE NEGATION STATE Command Failed"));
#endif
	return (1);
    }
/*
** Set Asynchronous Data Setup Time.
*/
    if (ql_mbox_cmd(ha,
		    mbox_sts, 2, 2,
		    MBOX_CMD_SET_ASYNC_DATA_SETUP_TIME,
	    (u_short) ha->ql_defaults.Capability.bits.ASync_Data_Setup_Time,
		    0, 0, 0, 0, 0, 0))
    {
	CMN_ERR((CE_WARN,
		"ql_init_isp - SET ASYNC DATA SETUP TIME Command Failed"));
	return (1);
    }
#else				/* NORMAL_STARTUP */

/*
** Indicate that we did NOT perform a full initialization.
*/
    return (1);

#endif				/* NORMAL_STARTUP */

#ifdef DUMP_REGS
    /* dump_registers(isp);  XXX */
#endif

/*
** Indicate we successfully initialized this ISP!
*/
    PRINTD(("Out of ql_init_isp\n"));

    return (0);
}








#if defined (SN0)  && defined (SCSI_TIMEOUT_WAR)

int
ql_to_war_reset(void)
{
	char times;
	int r;
	char ll ;

	times = get_scsi_war_value();
	if (times > SCSI_TO_MAX_RETRY) 
	    times = 0;

	if (times == SCSI_TO_MAX_RETRY)
	    return -1;

	if (times)
	    printf("\nSCSI adaptor timeout: resetting machine (attempt %d).\n",
		   times);
	else
	    printf("\nSCSI adaptor timeout: resetting machine.\n");
	
	times++;
	set_scsi_war_value(times);
	/*
 	 * Bug PV 464448
	 * We need to remember additional state - the value
	 * of the reboot variable. The value of this register
	 * is clobbered when we do a hard reset from here.
 	 */

	ll = diag_get_reboot() ;
	set_scsi_to_ll_value(ll);

	if (elsc_system_reset(get_elsc())) {
		printf("SCSI adaptor timeout: Unable to reset system\n");
		return -1;
	}
	

	return 0;
}


void
ql_to_war_fine(void)
{
	char times;

	times = get_scsi_war_value();

	if (times) {
		set_scsi_war_value(0);
	}
	
		
}

#endif /* defined (SN0) && defined (SCSI_TIMEOUT_WAR) */


/* Entry-Point
** Function:    ql_entry
**
** Description: Execute a SCSI request.
**
** Returns:  0 = success (i.e. I understood the request)
**      -1 = failure (what the heck was that?)
*/
int 
ql_entry(scsi_request_t * request)
{
    int             i = 0;
    unsigned long   tm0, tm1, timeout;
    u_short         id = request->sr_target;
    u_short         lun = request->sr_lun;
    pHA_INFO        ha = ha_info[request->sr_ctlr];

    PRINT3(("sr_ctlr %x\n", request->sr_ctlr));
    PRINT2(("ha_info[0] %x ha_info[1] %x\n", ha_info[0], ha_info[1]));

#ifdef QL_DEBUG
    PRINT2(("ql_entry: request=0x%x, id=%x,lun=%x, cmd=%x\n",
	   request, id, lun, *request->sr_command));
#endif
/*
** Before processing the request, assume bad status and change when we know
** the command was successfully completed
*/
    request->sr_status = SC_HARDERR;
    request->sr_scsi_status = 0;

/*
** Error if the adapter specified is invalid.
*/
    if (ha == NULL)
    {
#ifdef QL_DEBUG
	cmn_err(CE_WARN, "ql_entry: Invalid device specified");
#endif
/*
** Tell them that they specified an illegal host adapter.
*/
	request->sr_status = SC_REQUEST;

/*
** call back
*/

	request->sr_notify(request);
	return (0);
    }

    if(ha->flags & AFLG_SENSE_HELD) {
	ha->flags &= ~AFLG_SENSE_HELD; /* reset the flag */
	if (request->sr_command[0] == 0x03) {
	    PRINTD(("did get sense command\n"));
	    bcopy(last_sense, request->sr_buffer,REQ_SENSE_DATA_LENGTH);
	    request->sr_status=SC_GOOD;
	    request->sr_scsi_status=ST_GOOD;
	    return(0);
	}
	else {
	    PRINTD(("did not get sense command after autosense\n"));
	}
	
    }

/*
** Check that the device has been or is being initialized.
*/
    if (!initialized(ha,id,lun))

    {
#ifdef QL_DEBUG
	PRINT3(("ql_entry: Uninitialized device -  initializing adapter %x, target %x\n", request->sr_ctlr, id));
#endif

#ifdef QL_DEBUG
	PRINT3(("ql_entry: SCSI_INIT ha=%d, id=%d, lun=%d\n",
	       request->sr_ctlr, id, lun));	/* XXX */
#endif

/*
** If the device hasn't been initialized, do it now!
*/
	if (!initialized(ha,id,lun))
	{
	    u_short         mbox_sts[8];

/*
** Set Device Queue Parameters.
*/
	    if (ql_mbox_cmd(ha, mbox_sts, 4, 1,
			    MBOX_CMD_SET_DEVICE_QUEUE_PARAMETERS,
			    (u_short) ((id << 8) + lun),
			    ha->ql_defaults.Max_Queue_Depth,
			    ha->ql_defaults.Id[id].Throttle,
			    0, 0, 0, 0))
	    {
#ifdef QL_DEBUG
		cmn_err(CE_WARN,
		   "ql_entry - SET DEVICE QUEUE PARAMETERS Command Failed");
#endif
	    }
/*
** Set Target Parameters.
*/

	    PRINT5(("MBOX_CMD_SET_TARGET_PARAMETERS 0:%x 1:%x 2:%x 3:%x\n", MBOX_CMD_SET_TARGET_PARAMETERS, (u_short) (id
														      << 8), (u_short) (ha->ql_defaults.Id[id].Capability.byte << 8), (u_short) ((ha->ql_defaults.Id[id].Sync_Offset
								     << 8) |
				     (ha->ql_defaults.Id[id].Sync_Period))));
	    if (ql_mbox_cmd(
			    ha, mbox_sts, 4, 4,
			    MBOX_CMD_SET_TARGET_PARAMETERS,
			    (u_short) (id << 8),
		    (u_short) (ha->ql_defaults.Id[id].Capability.byte << 8),
		      (u_short) ((ha->ql_defaults.Id[id].Sync_Offset << 8) |
				 (ha->ql_defaults.Id[id].Sync_Period)),
			    0, 0, 0, 0))
	    {
#ifdef QL_DEBUG
		cmn_err(CE_WARN,
			"ql_entry - SET TARGET PARAMETERS Command Failed");
#endif
	    }
/*
** Device has now been initialized!
*/
	    ha->dev_init[id] |= (1<<lun);
	}
    }
/*
** Go handle the request.
*/

/*
** Queue a SCSI request.
** A scatter/gather request is processed the same here
** but different when it is actually issued to the adapter.
*/
/*
** Indicate that we're waiting for a fake interrupt.
*/
    ha->flags |= AFLG_POLL_ISR;

/*
** Issue the new command.
*/
#ifdef QL_DEBUG1
    PRINTD(("queue commands at 0x%x\n", request));
#endif
    ql_queue_cmd(ha, request);


/*
** If we're polling, then wait for it to complete.
*/
    timeout = request->sr_timeout / HZ + 1;

#ifdef HEART1_TIMEOUT_WAR
    /* Try to avoid heart PIO timeout bug when reading the clock */
    if (heart_rev() == HEART_REV_A) {
    	tm0 = timeout * 1000;
    	tm1 = 0;
    }
    else
#endif
    	tm0 = get_tod();
    do
    {
	ql_poll();

#ifdef HEART1_TIMEOUT_WAR
	if (heart_rev() == HEART_REV_A) {
	    us_delay(1000);
	    tm0--;
	    if (tm0 <= 0)
		break;
	}
	else
#endif
	{
	    tm1 = get_tod();
	    if (tm1 - tm0 >= timeout) {
	        break;
		}
	}
	us_delay(500) ; /* DELAY for .5 millisecond */
    } while (ha->flags & AFLG_POLL_ISR);

#if 0
/*
 * This section of code was used to debug 464448.
 * It will be around till we are confident that
 * the problem is fixed.
 * If you define a nvram variable 'checkto', and
 * set it to 1 in the ioprom, and boot unix from the net, it
 * will 'create' a scsi to war reset when a reboot
 * is done. This happens only once.
 */
    {
	char *y ;
	if (y = getenv("checkto")) {
		if (*y != '0') {
			ha->flags |= AFLG_POLL_ISR ;
			first_adaptor = 1 ;
		}
		setenv("checkto", "0") ;
	}
    }
#endif

    if (ha->flags & AFLG_POLL_ISR)
    {
#if defined (SN0) && defined (SCSI_TIMEOUT_WAR)
        if (first_adaptor) ql_to_war_reset();
#endif /* defined (SN0) && defined (SCSI_TIMEOUT_WAR) */
	
	printf("timeout on adapter %x target %x\n", request->sr_ctlr, request->sr_target);
	printf("   tm0=0x%x, tm1=0x%x, timeout=0x%x\n", tm0, tm1, timeout);
        request->sr_status = SC_HARDERR;
	/*dump_registers((pISP_REGS) ha->ha_base);*/
    }
#if defined (SN0) && defined (SCSI_TIMEOUT_WAR)
    else {
	first_adaptor = 0;
	ql_to_war_fine();
    }
#endif /* defined (SN0) && defined (SCSI_TIMEOUT_WAR) */

/*
** Return a status to indicate we understood the request.
*/
    return (0);
}



/* Entry-Point
** Function:    qlintr
**
** Description: This is the interrupt service routine.  This is called
**      by the system to process a hardware interrupt for our
**      board, or by one of our internal routines that need to
**      check for an interrupt by polling.  These routines MUST
**      lock out interrupts (splx) before calling here.
*/
void 
qlintr(pHA_INFO ha)
{
    PRINT3(("qlintr: pHA_INFO %x\n", ha));

    {
	pISP_REGS       isp = (pISP_REGS) ha->ha_base;
	u_short         isr = 0;

/*
** Determine if this board caused the (an) interrupt.
*/
	isr = PCI_INH(&isp->bus_isr);

	if (isr & BUS_ISR_RISC_INT)
	{
/*
** We have the interrupter...try to service the request.
*/
	    PRINT3(("service int with ha at %x \n", ha));

	    ql_service_interrupt(ha);
	}
    }
}




/*
** Function:    ql_poll
**
** Description: This is an alternative version of the interrupt service
**      routine.  It is for debug only in that its static so
**      we don't have to worry about anyone but us calling it.
**      It helps to bring up new implementations (Unixes) to
**      not worry about interrupts while trying to get a command
**      executed.  This is nearly identical to the real interrupt
**      service routine -- it just makes it easier to "play" with
**      the code here.  Understand?
*/
static void 
ql_poll(void)
{
    int             interrupt_seen;

#if 0
    printf("enter ql_poll loop.  isp=0x%x\n", (pISP_REGS) ha_list->ha_base);
    {
	pISP_REGS       isp = (pISP_REGS) ha_list->ha_base;
	printf("isr=0x%x\n", PCI_INH(&isp->bus_isr));
    }
#endif
    PRINT5(("+"));
    do
    {
	pHA_INFO        ha = ha_list;

/*
** Assume that we won't find an interrupt.
*/
	interrupt_seen = 0;

/*
** Always try all boards to locate the interrupt.
*/
	while (ha != 0)
	{
	    pISP_REGS       isp = (pISP_REGS) ha->ha_base;
	    u_short         isr = 0;

/*
** Determine if this board caused the interrupt.
*/
	    isr = PCI_INH(&isp->bus_isr);

	    if (isr & (BUS_ISR_RISC_INT))
	    {
/*
** We have the interrupter...try to service the request.
*/
		PRINT3(("service from ql \n"));
		ql_service_interrupt(ha);

/*
** If we saw one interrupt, maybe theres another.
*/
		interrupt_seen = 1;
	    }
/*
** Try the next board.
*/
	    ha = ha->next_ha;
	}

    } while (interrupt_seen);
}

/*
** Function:    ql_service_interrupt
**
** Description: This routine actually services the interrupt.
**      There are two kinds of interrupts: 1) Responses available
**      and 2) a Mailbox request completed.  ALWAYS service the
**      Mailbox interrupts first.  Then process all responses we
**      have (may be more than one).  Do this by pulling off all
**      the completed responses and putting them in a linked list.
**      Then after we have them all, tell the adapter we have them
**      and do completion processing on the list.  This limits
**      the amount of time the adapter has to wait to post new
**      responses.
*/
static void 
ql_service_interrupt(pHA_INFO ha)
{
    status_entry   *q_ptr;
    status_entry    tmp_status_entry;
    scsi_request_t *request;
    pREQ_INFO       r_ptr;
    pISP_REGS       isp = (pISP_REGS) ha->ha_base;

    scsi_request_t *cmp_forw = NULL;
    scsi_request_t *cmp_back = NULL;
    int	tgt,idx;
    u_short         isr = 0;


/*
** Process all the mailbox interrupts and all responses.
*/
    do
    {
	u_short         response_in = 0;
	u_short         bus_sema = 0;
	u_short         mailbox0 = 0;
	PRINT5(("ql_service_interrupt: got one\n"));
/*
** Get the current "response in" pointer.
** I will use this after I check the mailbox.
*/
	response_in = PCI_INH(&isp->mailbox5);
	PRINT3(("ql_service_interrupt: response_in %x\n", response_in));
/*
** If we weren't issuing a MAILBOX command,
** then check for an asynchronous mailbox interrupt.

*/

#if 0

	if (!(ha->flags & AFLG_MAIL_BOX_EXPECTED))
	{
	    PRINT3(("not AFLG_MAIL_BOX_EXPECTED in interrupt service\n"));
#endif
/*
** Get a copy of the semaphore and mailbox 0 registers.
*/

	    bus_sema = PCI_INH(&isp->bus_sema);
	    mailbox0 = PCI_INH(&isp->mailbox0);
	    PRINT3(("bus_sema %x\t mailbox0 %x\n", bus_sema, mailbox0));

/*
** Check for async mailbox events.
*/
    if (bus_sema & BUS_SEMA_LOCK)
    {
	if ((mailbox0 >= 0x4000) && (mailbox0 <= 0x7fff) ) {

	} else {
/*
** Determine what mailbox request is here.
*/
		switch (mailbox0)
		{
/*
** A system error?  WOW!  What can I say.
*/
		case MBOX_ASTS_SYSTEM_ERROR:
		    {
#ifdef QL_DEBUG
			u_short         mailbox1 = 0;
			mailbox1 = PCI_INH(&isp->mailbox1);

			cmn_err(CE_WARN,
				"ql_isr - ISP Fatal Firmware Error: ");
			cmn_err(CE_WARN,
			      "ql_isr - Mailbox 0 = 0x%x, Mailbox 1 = 0x%x",
				mailbox0, mailbox1);
			dump_registers(isp);
#endif
		    }
		    break;

/*
** Some joker reset the SCSI bus.
** We need to issue a Marker before the next
** request or the adapter will not execute it.
*/
		case MBOX_ASTS_SCSI_BUS_RESET:
		    {
			ha->flags |= AFLG_SEND_MARKER;
			PRINTD(("SCSI bus was reset. Send marker before next command\n"));
		    }
		    break;

/*
** What the heck was that interrupt?
*/
		default:
		    {
#ifdef QL_DEBUG
			cmn_err(CE_WARN,
				"ql_isr - Unexpected MailBox Response (mailbox0 = 0x%x)",
				mailbox0);
#endif
		    }
		} /* case */

	   } /* if */

/*
** Clear the semaphore lock.
*/
	PCI_OUTH(&isp->bus_sema, 0);
	PRINT3(("cleared the bus_sema\n"));
    }

#if 0
/*
** Clear the RISC interrupt. I clear it here, but will check
** it again in case another response gets posted.
*/
	PRINT3(("clear the risk interrupt %x %x\n",
	       PCI_INH(&isp->hccr), &isp->hccr));

	PCI_OUTH(&isp->hccr, HCCR_CMD_CLEAR_RISC_INT);

#endif

/*
** If there aren't any completed responses then don't do any
** of the following. i.e. it was just a mailbox interrupt or spurious becuase right now I poll on mailbox commands.
*/
	if (response_in == ha->response_out)
	{
/*
** We can do one of three things here:
** continue - that will try for another interrupt
** jump out with a goto
** or break and perform completion processing.
** I left them all here because I'm not sure which I want yet.
*/
	    PRINTD(("spurious interrupt\n"));

	    /* continue;         XXX */
	    /* goto no_responses;    XXX */
	    /* break; */

	 PRINT3(("clear the risk interrupt %x %x\n",
	       PCI_INH(&isp->hccr), &isp->hccr));

	 PCI_OUTH(&isp->hccr, HCCR_CMD_CLEAR_RISC_INT);

	 return;
 
	}
/*
** Remove all of the responses from the queue.
*/
	while (ha->response_out != response_in)
	{

/*
** Get a pointer to the next response entry.
*/
	    tmp_status_entry = *(status_entry *)ha->response_ptr;
	    q_ptr = &tmp_status_entry;
	    DCACHE_INVAL(
		(void *)((__psunsigned_t)ha->response_ptr & CACHE_SLINE_MASK),
		CACHE_SLINE_SIZE);

#if defined(IP30) && !defined(USE_MAPPED_CONTROL_STREAM)
	    PRINT3(("MUNGING IORB\n"));
	    munge((u_char *) q_ptr, 64);
#endif
#ifdef QL_DEBUG
	    if (ql_dump_control_block)
		dump_iorb((status_entry *) q_ptr);
	    if (ql_debug >= 3)
	    {
		dump_iorb((status_entry *) q_ptr);

	    }
#endif

/*
** Move the pointers for the next entry.
*/
	    if (ha->response_out == (RESPONSE_QUEUE_DEPTH - 1))
	    {
		ha->response_out = 0;
		ha->response_ptr = ha->response_base;
		PRINT5(("wrapped the response pointer\n"));
	    } else
	    {
		ha->response_out++;
		ha->response_ptr++;
	    }



/* assert that the IORB that we are processing is really a request */
	    ASSERT((q_ptr->handle > 0) && (q_ptr->handle <=  MAX_HANDLE_NO)); 

	    /* Determine which request it was.  */
	    tgt = q_ptr->handle / (MAX_REQ_INFO +1);
	    idx = q_ptr->handle % (MAX_REQ_INFO +1) ;
	    r_ptr = &ha->reqi_block[tgt][idx];
	    PRINT3(("r_ptr %x\n", r_ptr));

/*
** Make sure the driver issued the command right.
*/
	    if (q_ptr->hdr.entry_type != ET_STATUS ||
		q_ptr->hdr.flags & EF_ERROR_MASK)
	    {

		printf("\nql_isr - Invalid driver command issued  ");
		printf("entry_cnt=0x%x, entry_type=0x%x\n",
		       q_ptr->hdr.entry_cnt, q_ptr->hdr.entry_type);
		printf("flags=0x%x, sys_def_1=0x%x\n",
		       q_ptr->hdr.flags, q_ptr->hdr.sys_def_1);
		PCI_OUTH(&isp->mailbox5, ha->response_out);
		return;
	    }
	    request = r_ptr->req;

/*
** Put the request info structure back on the free list.
*/
	    r_ptr->req = NULL;

/* start out with good status */
/* it will be changed if required */
	    request->sr_status = SC_GOOD;
/* pass on scsi status to the dksc driver */
	    request->sr_scsi_status = q_ptr->scsi_status;
/* pass on the residual count */
	    request->sr_resid = q_ptr->residual;
	    PRINT3(("residual count %d\n", request->sr_resid));
	    PRINT3(("sr_buflen %d, buffer: 0x%lx\n", request->sr_buflen, request->sr_buffer));



#ifdef QL_DEBUG

/* what do we report to dksc under these conditions */

/*
case SCS_INCOMPLETE: SC_HARDERR
case SCS_DMA_ERROR: SC_HARDERR
case SCS_TRANSPORT_ERROR: SC_HARDERR
case SCS_RESET_OCCURRED: SC_ATTN
case SCS_ABORTED: SC_ATTN
case SCS_TIMEOUT: SC_CMDTIME
case SCS_DATA_OVERRUN: SC_HARDERR
case SCS_COMMAND_OVERRUN: SC_HARDERR
case SCS_STATUS_OVERRUN: SC_HARDERR
case SCS_BAD_MESSAGE: SC_HARDERR
case SCS_NO_MESSAGE_OUT: SC_HARDERR
case SCS_IDE_MSG_FAILED: SC_HARDERR
case SCS_ABORT_MSG_FAILED: SC_HARDERR
case SCS_REJECT_MSG_FAILED: SC_HARDERR
case SCS_NOP_MSG_FAILED: SC_HARDERR
case SCS_PARITY_ERROR_MSG_FAILED: SC_HARDERR
case SCS_ID_MSG_FAILED:	SC_HARDERR
case SCS_UNEXPECTED_BUS_FREE: SC_HARDERR
case SCS_DATA_UNDERRUN: SC_HARDERR  happens all the time with mode sense
				    for now don't report them
*/



/*
** Print out any status errors.
*/
	    /* if (q_ptr->completion_status) */
/* do this all the time for now */
#endif				/* QL_DEBUG */

	    {
		PRINT3(("ql_service_interrupt - completion status error [0x%x]\n",
		       q_ptr->completion_status));

		PRINTD(("scsi status[%x], state flags[%x], status flags[%x], completion_status[%x]\n",
		       q_ptr->scsi_status,
		       q_ptr->state_flags,
		       q_ptr->status_flags,
		       q_ptr->completion_status));

		switch (q_ptr->completion_status)
		{
		case SCS_COMPLETE:
		    {
			PRINT1(("completion status: GOOD\n"));
			break;
		    }
		case SCS_INCOMPLETE:
		    {
			PRINT1(("completion status: INCOMPLETE\n"));
			request->sr_status = SC_HARDERR;
			break;
		    }
		case SCS_DMA_ERROR:
		    {
			PRINT1(("completion status: DMA_ERROR\n"));
			request->sr_status = SC_HARDERR;

			break;

		    }
		case SCS_TRANSPORT_ERROR:
		    {
			PRINT1(("completion status: SCS_TRANSPORT_ERROR\n"));
			request->sr_status = SC_HARDERR;

			break;

		    }
		case SCS_RESET_OCCURRED:
		    {
			PRINT1(("completion status: SCS_RESET_OCCURRED\n"));
			request->sr_status = SC_ATTN;

			break;

		    }
		case SCS_ABORTED:
		    {
			PRINT1(("completion status: SCS_ABORTED\n"));
			request->sr_status = SC_ATTN;

			break;

		    }
		case SCS_TIMEOUT:
		    {
			PRINT1(("completion status: SCS_TIMEOUT\n"));
			request->sr_status = SC_CMDTIME;

			break;

		    }
		case SCS_DATA_OVERRUN:
		    {
			PRINT1(("completion status: SCS_DATA_OVERRUN\n"));
			request->sr_status = SC_HARDERR;
			break;

		    }
		case SCS_COMMAND_OVERRUN:
		    {
			PRINT1(("completion status: SCS_COMMAND_OVERRUN\n"));
			request->sr_status = SC_HARDERR;

			break;

		    }
		case SCS_STATUS_OVERRUN:
		    {
			PRINT1(("completion status: SCS_STATUS_OVERRUN\n"));
			request->sr_status = SC_HARDERR;

			break;

		    }

		case SCS_BAD_MESSAGE:
		    {
			PRINT1(("completion status: SCS_BAD_MESSAGE\n"));
			request->sr_status = SC_HARDERR;

			break;

		    }
		case SCS_NO_MESSAGE_OUT:
		    {
			PRINT1(("completion status: SCS_NO_MESSAGE_OUT\n"));
			request->sr_status = SC_HARDERR;

			break;

		    }
		case SCS_EXT_ID_FAILED:
		    {
			PRINT1(("completion status: SCS_EXT_ID_FAILED\n"));
			request->sr_status = SC_HARDERR;

			break;

		    }
		case SCS_IDE_MSG_FAILED:
		    {
			PRINT1(("completion status: SCS_IDE_MSG_FAILED\n"));
			request->sr_status = SC_HARDERR;

			break;

		    }
		case SCS_ABORT_MSG_FAILED:
		    {
			PRINT1(("completion status: SCS_ABORT_MSG_FAILED\n"));
			request->sr_status = SC_HARDERR;

			break;

		    }
		case SCS_REJECT_MSG_FAILED:
		    {
			PRINT1(("completion status: SCS_REJECT_MSG_FAILED\n"));
			request->sr_status = SC_HARDERR;

			break;

		    }
		case SCS_NOP_MSG_FAILED:
		    {
			PRINT1(("completion status: SCS_NOP_MSG_FAILED\n"));
			request->sr_status = SC_HARDERR;

			break;

		    }
		case SCS_PARITY_ERROR_MSG_FAILED:
		    {
			PRINT1(("completion status: SCS_DEVICE_RESET_MSG_FAILED\n"));
			request->sr_status = SC_HARDERR;

			break;

		    }
		case SCS_ID_MSG_FAILED:
		    {
			PRINT1(("completion status: SCS_ID_MSG_FAILED\n"));
			request->sr_status = SC_HARDERR;

			break;

		    }

		case SCS_UNEXPECTED_BUS_FREE:
		    {
			PRINT1(("completion status: SCS_UNEXPECTED_BUS_FREE\n"));
			request->sr_status = SC_HARDERR;

			break;

		    }
		case SCS_DATA_UNDERRUN:
/* this happens all the time when we ask for for mode sense than the drive
   can offer
*/
		    {
			PRINT1(("completion status: SCS_DATA_UNDERRUN\n"));
			/* request->sr_status=SC_HARDERR; */
			/* should reset bus too */
			break;

		    }

		default:
		    PRINTD(("DONT KNOW THE COMPLETION STATUS\n"));
		}
		if (q_ptr->completion_status)

		{

		    if (q_ptr->state_flags & SS_GOT_BUS)
			PRINT3(("state_flag: SS_GOT_BUS\n"));
		    if (q_ptr->state_flags & SS_GOT_TARGET)
			PRINT3(("state_flag: SS_GOT_TARGET\n"));
		    if (q_ptr->state_flags & SS_SENT_CDB)
			PRINT3(("state_flag: SS_SENT_CDB\n"));
		    if (q_ptr->state_flags & SS_TRANSFERRED_DATA)
			PRINT3(("state_flag: SS_TRANSFERRED_DATA\n"));
		    if (q_ptr->state_flags & SS_GOT_STATUS)
			PRINT3(("state_flag: SS_GOT_STATUS\n"));
		    if (q_ptr->state_flags & SS_GOT_SENSE)
			PRINT3(("state_flag: SS_GOT_SENSE\n"));
		    if (q_ptr->state_flags & SS_TRANSFER_COMPLETE)
			PRINT3(("state_flag: SS_TRANSFER_COMPLETE\n"));
		    if (q_ptr->status_flags & SST_DISCONNECT)
			PRINT3(("status_flag: SST_DISCONNECT\n"));
		    if (q_ptr->status_flags & SST_SYNCHRONOUS)
			PRINT3(("status_flag: SST_SYNCHRONOUS\n"));
		    if (q_ptr->status_flags & SST_PARITY_ERROR)
		    {
			request->sr_status |= SC_PARITY;
			PRINT3(("status_flag: SST_PARITY_ERROR\n"));
		    }
		    if (q_ptr->status_flags & SST_BUS_RESET)
		    {
/*					request->sr_status|=SC_ATTN;*/
			PRINT3(("status_flag: SST_BUS_RESET\n"));
		    }
		    if (q_ptr->status_flags & SST_DEVICE_RESET)
			PRINT3(("status_flag: SST_DEVICE_RESET\n"));
		    if (q_ptr->status_flags & SST_ABORTED)
			PRINT3(("status_flag: SST_ABORTED\n"));
		    if (q_ptr->status_flags & SST_TIMEOUT)
		    {
			PRINT3(("status_flag: SST_TIMEOUT\n"));
			request->sr_status |= SC_TIMEOUT;
		    }
		    if (q_ptr->status_flags & SST_NEGOTIATION)
			PRINT3(("status_flag: SST_NEGOTIATION\n"));


		}
#ifdef DUMP_IOCB
		/* dump_iocb(q_ptr); XXX */
#endif
	    }





	    PRINT2(("ql_service_interrupt: IORB %x  request %x buffer %x bufferlen %x\n", q_ptr, request, request->sr_buffer, request->sr_buflen));
	    PRINT4(("completion status %x scsi status[%x], state flags[%x], status flags[%x]\n", q_ptr->completion_status,
		   q_ptr->scsi_status,
		   q_ptr->state_flags,
		   q_ptr->status_flags));

	    if (request->sr_buflen)

	    {
		{
		    if ((request->sr_command[0] != READ_CMD_28) && (request->sr_command[0] != WRITE_CMD_2A))
		    {
#ifndef SWAP_DATA_STREAM
			munge(request->sr_buffer, request->sr_buflen);
			PRINT2(("munged data from command %x\n", request->sr_command[0]));
#endif
		    }
		}
		if (request->sr_command[0] == 0x28)
		{
		    int             i;
		    /*REFERENCED*/
		    u_char         *p;
		    p = request->sr_command;
		    for (i = 0; i < 10; i++)
			PRINT4(("%x ", *(p++)));
		    PRINT4(("\n"));
		}
/*       if (q_ptr->completion_status)debug("status error");*/

	    }
	    if (q_ptr->completion_status == SCS_DATA_UNDERRUN)
	    {
/* allow underruns for now.  This happens all the time with mode sense */
		PRINT1(("saw underrun.... not reporting it though\n"));
		request->sr_status = 0;
	    }
/* send zero status for now... this should fix the underrun condition */
/* on mode sense commands */
	    if (request->sr_status != 0)
		PRINT1(("sr_status not zero -- %x \n", request->sr_status));

	    request->sr_scsi_status = q_ptr->scsi_status;

/*
** If a SCSI bus reset occurred, then we need to
** issue a marker before issuing any new requests.
*/
	    if (q_ptr->completion_status == SCS_RESET_OCCURRED)
	    {
		ha->flags |= AFLG_SEND_MARKER;
		/* when this happens the command never got executed		 */
		/* mark this a SC_BUS_RESET so the command will be retried */
		request->sr_status = SC_BUS_RESET;
		PRINT2(("SCS_RESET_OCCURRED occured\n"));
	    }
/*
** Save any request sense info.
*/
	    if (q_ptr->state_flags & SS_GOT_SENSE)
	    {
		u_char          cnt;
		u_char         *dest;
#ifdef QL_DEBUG
		if (ql_debug)
		    dump_iorb((status_entry *) q_ptr);
#endif
		if (request->sr_sense == NULL)
		{
		    PRINTD(("no place to put the sense data in the scsi request struct\n"));
		    munge(&q_ptr->req_sense_data[0], REQ_SENSE_DATA_LENGTH);
#ifdef QL_DEBUG
		    dump_area((char *) &q_ptr->req_sense_data[0], REQ_SENSE_DATA_LENGTH);
#endif
		    bcopy(q_ptr->req_sense_data,last_sense,REQ_SENSE_DATA_LENGTH);
		    ha->flags |= AFLG_SENSE_HELD;
		    PRINTD(("AFLG_SENSE_HELD\n"));



		} else
		{
		    dest = &request->sr_sense[0];
		    PRINTD(("sense: adapter %x, target %x", request->sr_ctlr, request->sr_target));
		    for (cnt = 0; cnt < REQ_SENSE_DATA_LENGTH; cnt++)
		    {
			*dest++ = q_ptr->req_sense_data[cnt];
		    }
#ifndef SWAP_DATA_STREAM
		    munge(&request->sr_sense[0], REQ_SENSE_DATA_LENGTH);
#endif
#ifdef QL_DEBUG
		    dump_area((char *) &request->sr_sense[0], REQ_SENSE_DATA_LENGTH);
#endif
		    request->sr_sensegotten = REQ_SENSE_DATA_LENGTH;
		    bcopy(request->sr_sense, &tinfo[request->sr_ctlr][request->sr_target][request->sr_lun].si_sense, request->sr_sensegotten);

		}
	    }
/*
** Link this request and return them later.
*/
	    if (cmp_forw)
	    {
		cmp_back->req_forw = request;
		cmp_back = request;
		request->req_forw = NULL;
	    } else
	    {
		cmp_forw = request;
		cmp_back = request;
	    }
	    request->req_forw = NULL;
	}

/*
** I think I have all of the responses.  Update the ISP.
*/
	PRINT3(("retrieved all reponses from controller. put %x ha->response_out\n", ha->response_out));
	PCI_OUTH(&isp->mailbox5, ha->response_out);
	PCI_OUTH(&isp->hccr, HCCR_CMD_CLEAR_RISC_INT);

/*
** Get a new copy of the ISR.
*/
	isr = PCI_INH(&isp->bus_isr);
	PRINT3(("Get the isr again.  it is %x mask = %x\n", isr, BUS_ISR_RISC_INT));

    } while (isr & BUS_ISR_RISC_INT);	/* XXX */

/*no_responses:*/

/*
** Send all completion information back to the host.
*/
    while (request = cmp_forw)
    {
	cmp_forw = request->req_forw;

/* zero out the buffer length */
	request->sr_buflen = 0;
	request->sr_flags = 0;
	request->sr_buffer = 0;

	/* callback now */

#ifdef QL_DEBUG
	PRINT3(("ql_service_interrupt: completed request=0x%x\n", request));
#endif
    }

/*
** If we're in polling mode, then turn this flag off.
*/
    {
	PRINT3(("clearing AFLG_POLL_ISR\n"));
	ha->flags &= ~AFLG_POLL_ISR;
    }

/*
** Some resources may have become available.
** See if there are any requests to send.
*/
    ql_start_scsi(ha);
}



/* Function:    ql_queue_cmd
**
** Description: This routine queues up a new request on our internal
**      queue list.  We then call start to see if it can start
**      the new request.  If not, it'll get back to it later.
*/
static void 
ql_queue_cmd(pHA_INFO ha, scsi_request_t * request)
{

    PRINT3(("ql_queue_cmd: ha %x scsi_request %x\n", ha, request));
    CRITICAL_BEGIN
    {

/*
** Link the request onto the pending queue.
*/
	if (ha->req_forw)
	{
	    ha->req_back->req_forw = request;
	} else
	{
	    ha->req_forw = request;
	}
	ha->req_back = request;
	request->req_forw = NULL;

/*
** Start the command (if possible).
*/
	ql_start_scsi(ha);
    }
    CRITICAL_END
}

/* Function:    ql_start_scsi
**
** Description: This routine tries to start up any new requests we have
**      pending on our internal queue.  This may be new commands
**      or it may be ones that we couldn't start berfore because
**      the adapter queues were full.
*/
static void 
ql_start_scsi(pHA_INFO ha)
{
    pREQ_INFO       r_ptr;
    scsi_request_t *request;
    pISP_REGS       isp = (pISP_REGS) ha->ha_base;
    p_scatter       scat = &ha->scatter_base;

    int             entries = 0, index, ri_slot;

#if SN0 && HUB1_II_TIMEOUT_WAR
    /* Fix for HUB TIMEOUT BUG This routine needs to be called max 20 secs
     * before a DMA is done. It is here right now but it can be called later
     * in this routine if required. */

    hub_dmatimeout_kick((__psunsigned_t) ha->ha_base);

#endif				/* SN0 && HUB1_II_TIMEOUT_WAR */

#ifdef QLREVA_STALE_RRB_WAR
/* The ql controller sometimes asks for things it doesn't consume, which
 * can lead the bridge to be stuck with stale buffers in the current
 * bridge non-prefetch non-percise mode.  This is fixed in rev 0x4.
 */
{
    void bridge_inval_rrbs(__psunsigned_t);

    if (ha->revision < REV_ISP1040B)
    	bridge_inval_rrbs((__psunsigned_t)ha->ha_base);
}
#endif

/*
** If there is nothing on the list for us to do,
** then get out now.
*/


    PRINT3(("\n\n\nABOUT TO SEND COMMAND\n"));


    if ((request = ha->req_forw) == NULL)
    {
#ifdef QL_DEBUG
	PRINT3(("ql_start_scsi: Nothing to start\n"));
#endif
	return;
    }
    PRINT3(("ql_start_scsi: request: 0x%x ha: 0x%x\n", request, ha));

/*
** If we're going to do a scatter/gather operation we may need
** to use more than one queue entry (one for the request and one
** or more for the scatter/gather table).  So, if we have less than
** two queue slots available (might need one for a marker too)
** then zap the number of queue entries so we'll go look again.
** This is kindof a kludge, but ...  we's does whats we needs to do.
*/
    if (ha->queue_space <= 2)
    {
	ha->queue_space = 0;	/* go look for "number queue slots" */
    }
/*
** Determine how many entries are available on the request queue.
**
** This is kinda neat -- instead of always checking if there are
** any new slots RIGHT NOW, we keep track of how many slots there
** were the last time we checked.  Since we're the only one filling
** these, there will always be AT LEAST that many still available.
** Once we reach that number, theres no sense in issuing any more
** until he's taken some.  So, when we filled up the number of slots
** available on our last check, THEN we check again (to see if there
** is anymore slots available).
**
** If we filled all the slots on the last pass, then check to see
** how many slots there are NOW!
*/
/* took this out because sometimes we have huge request and run out of entries*/
    /* if (ha->queue_space == 0) */
    {
	u_short         mailbox4 = 0;


	mailbox4 = PCI_INH(&isp->mailbox4);

	ha->request_out = mailbox4;

/*
** If the pointers are the same, then ALL the slots are available.
** We have to account for wrap condition, so check both conditions.
*/
	if (ha->request_in == ha->request_out)
	{
	    ha->queue_space = REQUEST_QUEUE_DEPTH - 1;
	} else if (ha->request_in > ha->request_out)
	{
	    ha->queue_space = ((REQUEST_QUEUE_DEPTH - 1) -
			       (ha->request_in - ha->request_out));
	} else
	{
	    ha->queue_space = (ha->request_out - ha->request_in) - 1;
	}
    }

    {
	/*REFERENCED*/
	u_short         mailbox4 = 0;
	/*REFERENCED*/
	u_short         mailbox5 = 0;

	mailbox4 = PCI_INH(&isp->mailbox4);
	mailbox5 = PCI_INH(&isp->mailbox5);

	PRINT4(("\nmbox 4 %d  mbox 5 %d \n", mailbox4, mailbox5));
    }
/*
** If we need to send a Marker do it now...
** We only have to send MARKER TO ALL type requests.
*/
    if (ha->queue_space != 0 && ha->flags & AFLG_SEND_MARKER)
    {
	marker_entry   *q_ptr;

/*
** Reset the flag so we don't send another marker.
*/
	ha->flags &= ~AFLG_SEND_MARKER;

/*
** Get a pointer to the queue entry for the marker.
*/
	q_ptr = (marker_entry *) ha->request_ptr;

/*
** Move the internal pointers for the request queue.
*/
	if (ha->request_in == (REQUEST_QUEUE_DEPTH - 1))
	{
	    ha->request_in = 0;
	    ha->request_ptr = ha->request_base;
	    PRINT4(("wrapped the request pointer\n"));
	} else
	{
	    ha->request_in++;
	    ha->request_ptr++;
	}

	bzero(q_ptr, sizeof(marker_entry));

/*
** Put the marker in the request queue.
** NOTE: Always use modifier of "2" to marker all devices.
*/
	q_ptr->hdr.entry_type = ET_MARKER;
	q_ptr->hdr.entry_cnt = 1;
	q_ptr->hdr.flags = 0;
	q_ptr->hdr.sys_def_1 = ha->request_in;

	q_ptr->reserved0 = 0;	/* just in case */
	q_ptr->target_id = 0;
	q_ptr->target_lun = 0;
	q_ptr->reserved1 = 0;	/* just in case */
	q_ptr->modifier = 2;

#ifdef QL_DEBUG
	dump_marker(q_ptr);
#endif

#if defined(IP30) && !defined(USE_MAPPED_CONTROL_STREAM)
	munge((u_char *) q_ptr, 64);
#endif
	/* flush the entry out of the cache */
	MR_DCACHE_WB_INVAL(q_ptr, sizeof(marker_entry));

#ifdef QL_DEBUG
	PRINT2(("ql_start_scsi: Marker command given to adapter\n"));
#endif
/*
** Tell isp it's got a new I/O request...
*/
	PRINT2(("writing %x to mbox 4\n", ha->request_in));
	PCI_OUTH(&isp->mailbox4, ha->request_in);


/*
** One less I/O slot.
*/
	ha->queue_space--;
    }
/*
** See if we can start one or more requests.
** This means there's room in the queue, we have another request
** to start, and we have a request structure for the request.
*/
    while (ha->queue_space != 0 &&
	   (request = ha->req_forw) != NULL &&
	   (ri_slot = reqi_get_slot(ha,(int)request->sr_target)) != 0)
    {
	int             i;
	command_entry  *q_ptr;

	r_ptr = &ha->reqi_block[request->sr_target][ri_slot % (MAX_REQ_INFO +1 )];

/*
** In order to perform a scatter/gather request, we need to
** determine how many items there are in the scatter list
** We need to do this first because this is the only case where
** we might need more than one queue entry (one for the request
** and more for the continuation of the scatter list).
** If there won't be enough queue entries to process this
** request, then we can't issue the request now.
** We'll just drop out now and let the next interrupt start us
** up again (which should work because we'll re-check the
** number of queue entries).
**
** data_len specifies the total bytes to transfer.
** data_ptr points to an array of address values.
** link_ptr points to an array of length values.
*/
	if (request->sr_buflen)
	{
	    /*REFERENCED*/
	    uint            len;
	    len = make_sg(scat, request->sr_buffer, request->sr_buflen, ha);

	    entries = scat->num_pages;
#ifdef QL_DEBUG
	    PRINT3(("ql_start_scsi: return from make_sg or make_sgbp, len = %x, num_pages = %x\n", len, scat->num_pages));	/* XXX */
	    PRINT3(("ql_start_scsi: S/G xfer has %d entries\n", entries));	/* XXX */
	    PRINT3(("ql_start_scsi: queue_space is %d\n", ha->queue_space));	/* XXX */
#endif

/*
** If there aren't enough queue entries to support this
** command then bye bye ... Clear the number of entries
** in the queue so we'll check with the controller on
** the next call to this routine.
** There is room for four entries in with the command.
** There is room for 7 entries in each continuation entry.
** So, excluding the four in the command entry, divide by
** seven to determine the number of continuations.  Add
** one to count the command entry.
**
** Note: remember that there MUST be some command outstanding
** or the number of queue entries would be greater than 2
** (at the top of this function we ensure that there is
** more than two entries).
*/
	    if ((((entries - IOCB_SEGS) / CONTINUATION_SEGS) + 1) >= ha->queue_space)
	    {
		ha->queue_space = 0;
#ifdef QL_DEBUG
		cmn_err(CE_WARN, "ql_start_scsi: No room for continue pkt\n");
#endif
		break;
	    }
	}
/*
** If we got this far, then we have enough entries in the queue
** to satisfy the request.  All we need to do is grab the next
** entry, fill it in, and tell the controller that there are
** new entries.
*/

/*
** Unhook the request and the request info structure.
*/
	ha->req_forw = request->req_forw;
/*
** Initialize the request info structure for this request.
*/
	r_ptr->req = request;
	PRINT1(("r_ptr = 0x%x request is %x\n", r_ptr, request));

	request->sr_status = 0;	/* this is counted down on retrun */
	request->sr_scsi_status = 0;	/* this is how many we issued */


/*
** Get a pointer to the queue entry for the command.
*/
	q_ptr = (command_entry *) ha->request_ptr;

/*
** Move the internal pointers for the request queue.
*/
	if (ha->request_in == (REQUEST_QUEUE_DEPTH - 1))
	{
	    ha->request_in = 0;
	    ha->request_ptr = ha->request_base;
	    PRINT3(("wrapped the request pointer\n"));

	} else
	{
	    ha->request_in++;
	    ha->request_ptr++;
	}
	/* bzero out the entry.  makes it easier for debug.  might get ride
	 * of it later */
	bzero(q_ptr, sizeof(queue_entry));
/*
** Fill in the command header.
*/
	q_ptr->hdr.entry_type = ET_COMMAND;
	q_ptr->hdr.entry_cnt = 1;
	q_ptr->hdr.flags = 0;
	q_ptr->hdr.sys_def_1 = ha->request_in;

/*
** Fill in the reset of the command data.
*/
	q_ptr->handle = ri_slot;
	q_ptr->target_id = request->sr_target;
	ASSERT(request->sr_target != ha->ql_defaults.Initiator_SCSI_Id); 
	q_ptr->target_lun = request->sr_lun;
	q_ptr->control_flags = 0;
	q_ptr->reserved = 0;
/* do not let the conroller timeout commands */
/* this driver will poll on command completion and timeout */
/* based on sr_timeout */
	q_ptr->time_out = 0; 

#ifdef QL_DEBUG
	for (i = 0; i < request->sr_cmdlen; i++)
	    PRINT2(("%x ", request->sr_command[i]));

	if (request->sr_command[0] == READ_CMD_28)
	{
	    PRINT2(("READ %x bytes\n", request->sr_buflen));
	} else if (request->sr_command[0] == WRITE_CMD_2A)
	{
	    PRINT2(("WRITE %x bytes\n", request->sr_buflen));
	} else
	{
	    int             i;
	    /* dump the cdb */
	    PRINT2(("\nnot a READ or a WRITE command  "));
	    PRINT2(("request->sr_command  %x :", request->sr_command));


	    for (i = 0; i < request->sr_cmdlen; i++)
	    {
		PRINT2((" %x", request->sr_command[i]));
	    }
	    PRINT2(("  sr_flags %x\n", q_ptr->control_flags));

	}
#endif


/*
** Copy over the requests SCSI cdb.
*/
	q_ptr->cdb_length = request->sr_cmdlen;

	switch (request->sr_cmdlen)
	{
	case SC_CLASS2_SZ:
	    q_ptr->cdb11 = request->sr_command[11];
	    q_ptr->cdb10 = request->sr_command[10];
	case SC_CLASS1_SZ:
	    q_ptr->cdb9 = request->sr_command[9];
	    q_ptr->cdb8 = request->sr_command[8];
	    q_ptr->cdb7 = request->sr_command[7];
	    q_ptr->cdb6 = request->sr_command[6];
	case SC_CLASS0_SZ:
	    q_ptr->cdb5 = request->sr_command[5];
	    q_ptr->cdb4 = request->sr_command[4];
	    q_ptr->cdb3 = request->sr_command[3];
	    q_ptr->cdb2 = request->sr_command[2];
	    q_ptr->cdb1 = request->sr_command[1];
	    q_ptr->cdb0 = request->sr_command[0];
	}

	PRINT1(("SCSI COMMAND: %x %x %x %x %x %x byte count %x for targ %x lun %x\n",
	       request->sr_command[0],
	       request->sr_command[1],
	       request->sr_command[2],
	       request->sr_command[3],
	       request->sr_command[4],
	       request->sr_command[5],
	       request->sr_buflen,
	       request->sr_target,
	       request->sr_lun));
/*
** Setup to transfer data.
*/
	if (q_ptr->dseg[0].count = request->sr_buflen)
	{
	    q_ptr->segment_cnt = 1;
	    q_ptr->dseg[0].base_low_32 = (u_int) MAKE_DIRECT_MAPPED_2GIG(kvtophys(request->sr_buffer));

#ifdef SN0_PCI_64 
		{
			paddr_t p_ptr;
	    		p_ptr =  get_pci64_dma_addr(
					(__psunsigned_t) request->sr_buffer,
				      ha_sn0_info[ha->controller_number]);
	    		q_ptr->dseg[0].base_low_32 = p_ptr;
	    		q_ptr->dseg[0].base_high_32 = p_ptr>> 32;
		/*printf("l32 %x h32 %x\n",q_ptr->dseg[0].base_low_32,q_ptr->dseg[0].base_high_32);*/
		}
#endif

	    if (!(request->sr_flags & SRF_DIR_IN))
	    {
		q_ptr->control_flags |= CF_WRITE;
		PRINT3(("WRITE to memory direction\n"));
		if (request->sr_buflen)
		{
		    {
			if ((request->sr_command[0] != READ_CMD_28) && (request->sr_command[0] != WRITE_CMD_2A))
			{
#ifndef SWAP_DATA_STREAM
			    munge(request->sr_buffer, request->sr_buflen);
			    PRINT3(("munge before controller DMA reads\n"));
#endif
			}
			if (request->sr_buffer)
			    MR_DCACHE_WB_INVAL(request->sr_buffer, request->sr_buflen);
		    }
		}
	    } else
	    {
		q_ptr->control_flags |= CF_READ;
		if (request->sr_buffer)
		    DCACHE_INVAL(request->sr_buffer, request->sr_buflen);
	    }
	} else
	{
	    q_ptr->segment_cnt = 0;
	}


/*
** If this is a scatter/gather request, then we need to adjust the
** data address and lengths for the scatter entries.
*/
/*
** If there are more than IOCB_SEGS entries in the scatter
** gather list, then we're going to be using some contiuation
** queue entries.  So, tell the command entry how many
** continuations to expect.  Add two to include the command.
*/
	if (entries > IOCB_SEGS)
	{
	    int             i = 1;
	    if ((entries - IOCB_SEGS) % CONTINUATION_SEGS)
		i = 2;
	    q_ptr->hdr.entry_cnt = ((entries - IOCB_SEGS) / CONTINUATION_SEGS) + i;
	}
/*
** Also, update the segment count with the number of entries
** in the scatter/gather list.
*/
	q_ptr->segment_cnt = entries;

/*
** The command queue entry has room for four scatter/gather
** entries.  Fill them in.
*/
	for (i = 0; i < IOCB_SEGS && entries; i++, entries--)
	{
	    q_ptr->dseg[i].base_low_32 =  scat->dseg[i].base;
#ifdef SN0_PCI_64
	    q_ptr->dseg[i].base_high_32 =  scat->dseg[i].base >> 32;
#endif
	    q_ptr->dseg[i].count = scat->dseg[i].count;
	}
/*
** If there is still more entries in the scatter/gather
** list, then we need to use some continuation queue entries.
**
** While there are more entries in the scatter/gather
** table, allocate a continuation queue entry and fill it in.
*/

	index = IOCB_SEGS;	/* set the index for the */
	/* next sg element pair  */
	while (entries)
	{
	    continuation_entry *c_ptr;

/*
** Get a pointer to the next queue entry for the
** continuation of the command.
*/
	    c_ptr = (continuation_entry *) ha->request_ptr;

/*
** Move the internal pointers for the request queue.
*/
	    if (ha->request_in == (REQUEST_QUEUE_DEPTH - 1))
	    {
		ha->request_in = 0;
		ha->request_ptr = ha->request_base;
		PRINT3(("wrapped the request pointer\n"));

	    } else
	    {
		ha->request_in++;
		ha->request_ptr++;
	    }

/*
** Fill in the continuation header.
*/
/*zero out the entry. might take out later */
	    bzero(c_ptr, sizeof(continuation_entry));
	    c_ptr->hdr.entry_type = ET_CONTINUATION;
	    c_ptr->hdr.entry_cnt = 1;
	    c_ptr->hdr.flags = 0;
	    c_ptr->hdr.sys_def_1 = 0;
#ifndef SN0_PCI_64
	    c_ptr->reserved = 0;
#endif
/*
** Fill in the scatter entries for this continuation.
*/
	    for (i = 0; i < CONTINUATION_SEGS && entries; i++, entries--)
	    {
		c_ptr->dseg[i].base_low_32 =  scat->dseg[index].base;
#ifdef SN0_PCI_64
		c_ptr->dseg[i].base_high_32 =  scat->dseg[index].base >> 32;
#endif
		c_ptr->dseg[i].count = scat->dseg[index].count;
		index++;
	    }

/*
** One less I/O slot.
*/
	    ha->queue_space--;
	    /* flush the entry out of the cache */
#if defined(IP30) && !defined(USE_MAPPED_CONTROL_STREAM)
	    munge((u_char *) c_ptr, 64);	/* XXX for emulation */
#endif
#ifdef QL_DEBUG
	    if (ql_debug >= 3)
	    {
		dump_continuation((continuation_entry *) c_ptr);
	    }
#endif

	    MR_DCACHE_WB_INVAL(c_ptr, sizeof(continuation_entry));
	    PRINT3(("continuation entry\n"));
	}

#ifdef QL_DEBUG
	PRINT3(("ql_start_scsi: New command given to adapter\n"));
	if (ql_debug >= 3)
	{
	    dump_iocb((command_entry *) q_ptr);	/* XXX */
	}
#endif

#ifdef DUMP_IOCB
	if (ql_dump_control_block)
	    dump_iocb((command_entry *) q_ptr);	/* XXX */
#endif
#if defined(IP30) && !defined(USE_MAPPED_CONTROL_STREAM)

	PRINT3(("MUNGING IOCB\n"));
	munge((u_char *) q_ptr, 64);
#endif
	MR_DCACHE_WB_INVAL((void *)q_ptr, 64);


/*
** Tell isp it's got a new I/O request...
** Wake up
*/
	PRINT2(("writing %x to mbox4 at %x\n", ha->request_in,
	       &isp->mailbox4));
	PCI_OUTH(&isp->mailbox4, ha->request_in);



/*
** One less I/O slot.
*/
	ha->queue_space--;



    }
}


/* Function:    ql_mbox_cmd()
**
** Return:  0   no error
**      non-0   error
**
** Description: This routine issues a mailbox command.
**      We're passed a list of mailboxes and the number of
**      mailboxes to read and the number to write.
**
** Note: I've added a special case (that may not be any problem)
** to skip loading mailbox 4 if the command is INITIALIZE RESPONSE QUEUE.
*/
static int 
ql_mbox_cmd(pHA_INFO ha,
	    u_short * mbox_sts,
	    u_char out_cnt, u_char in_cnt,
	    u_short reg0, u_short reg1, u_short reg2, u_short reg3,
	    u_short reg4, u_short reg5, u_short reg6, u_short reg7)
{
    pISP_REGS       isp = (pISP_REGS) ha->ha_base;

    int             complete = 0;
    int             retry_cnt = 1;
    int             timeout_error = 0;

/*XXX  a guess at a timeout value */
#define MBOX_TIMEOUT_VALUE 20000000
    long            timeout = 1;
/*
** Copies of ISP registers (for register accesses).
*/
    u_short         hccr = 0;
    u_short         icr = 0;
    u_short         bus_sema = 0;
    int             disable = 0;

    if ((reg0 == MBOX_CMD_WRITE_RAM_WORD) || (reg0 == MBOX_CMD_READ_RAM_WORD))
	disable = 1;

#ifdef MBOX_DEBUG
    /* dump_registers(isp); */
    if (!disable)
	PRINT5(("\n\nql_mbox_cmd - new cmd 0x%x (out=%d, in=%d)\n",
	       reg0, out_cnt, in_cnt));
    if (!disable)
	PRINT3(("out %x: [%x %x %x %x %x %x %x %x]  ",
	       out_cnt, reg0, reg1, reg2, reg3, reg4, reg5, reg6, reg7));	/* XXX */
#endif

#ifdef MBOX_DEBUG
    if (!disable)
	PRINT5(("P1 "));		/* XXX */
#endif

/*
** Just for grins, clear out ALL the return status.
** This might catch a bug later where I try to use a return
** parameter that I wasn't given (would be left over from previous).
*/
    mbox_sts[0] = 0;
    mbox_sts[1] = 0;
    mbox_sts[2] = 0;
    mbox_sts[3] = 0;
    mbox_sts[4] = 0;
    mbox_sts[5] = 0;
    mbox_sts[6] = 0;
    mbox_sts[7] = 0;


/*
** Host interrupt should be clear unless we just started.
** Wait for the bit to clear.
** Note: Later, be sure to timeout (in case of dead hardware).
*/

    do
    {
/*
** Get a new copy of the HCCR.
*/
	hccr = PCI_INH(&isp->hccr);
	if (!disable)
	    PRINT5(("hccr %x\n", hccr));

    } while (hccr & HCCR_HOST_INT);


#ifdef QL_DEBUG
    if (!disable)
	PRINT5(("P2 "));		/* XXX */
#endif

    /* CRITICAL_BEGIN   XXX */
    {

/*
** Get a copy of the ICR.  Are interrupts enabled (yet)?
** We'll use it later to determine if we need to clear
** the interrupt status.
*/
	icr = PCI_INH(&isp->bus_icr);
	if (!disable)
	    PRINT5(("icr is %x\n", icr));

/*
** Okay, now try to send the command.
*/
	do
	{
/*
** Move the data to the mailbox registers.
*/
	    if (!disable)
		PRINT5(("out_cnt is %x\n", out_cnt));
	    switch (out_cnt)
	    {
	    case 8:
		PCI_OUTH(&isp->mailbox7, reg7);
	    case 7:
		PCI_OUTH(&isp->mailbox6, reg6);
	    case 6:
		PCI_OUTH(&isp->mailbox5, reg5);
	    case 5:
		if (reg0 != 0x11)
		    PCI_OUTH(&isp->mailbox4, reg4);
	    case 4:
		PCI_OUTH(&isp->mailbox3, reg3);
	    case 3:
		PCI_OUTH(&isp->mailbox2, reg2);
	    case 2:
		PCI_OUTH(&isp->mailbox1, reg1);
	    case 1:
		PCI_OUTH(&isp->mailbox0, reg0);
		if (!disable)
		    PRINT3(("all mailbox registers loaded cmd:%x %x %x %x %x %x %x %x\n", reg0, reg1, reg2, reg3, reg4, reg5, reg6, reg7));
	    }

#ifdef QL_DEBUG
	    if (!disable)
		PRINT5(("P3 "));	/* XXX */
#endif

#ifdef DUMP_REGS
	    /* dump_registers(isp); XXX */
#endif

/*
** Let the interrupt service routine know we were
** expecting an interrupt, so okay to ignore it.
*/
	    ha->flags |= AFLG_MAIL_BOX_EXPECTED;
	    if (!disable)
		PRINT3(("set AFLG_MAIL_BOX_EXPECTED\n"));


/*
** Wake up the isp.
*/
	    PCI_OUTH(&isp->hccr, HCCR_CMD_SET_HOST_INT);


#ifdef QL_DEBUG
	    if (!disable)
		PRINT5(("P4 "));	/* XXX */
#endif

#ifdef WAIT_SEMA

/*
** Wait for the command to complete
** Note: Later, be sure to timeout (in case of dead hardware).
*/
	    if (!disable)
		PRINT5(("waiting for sema\n"));

	    do
	    {
/*
** Get a new copy of the semaphore register.
*/
		bus_sema = PCI_INH(&isp->bus_sema);
		if (!(timeout % 100))
		{
		    PRINT3(("reading sema %x timeout %d\n", bus_sema, timeout));
		    /* dump_registers(isp);	 */
		}
		if (++timeout >= MBOX_TIMEOUT_VALUE)
		{
		    PRINTD(("mbox_cmd timeout waiting for sema at %x=%x\n", &isp->bus_sema, PCI_INH(&isp->bus_sema)));
		    timeout_error = 1;
		    break;
		}
	    } while (!(bus_sema & BUS_SEMA_LOCK));

#ifdef QL_DEBUG
	    if (!disable)
		PRINT5(("P5A "));	/* XXX */
#endif

#else				/* WAIT_SEMA */

/*
** Wait for the RISC INTERRUPT to arrive.
*/
	    PRINT5(("waiting for interrupt\n"));

	    do
	    {
/*
** Get a new copy of the ISR.
*/
		isr = PCI_INH(&isp->bus_isr);
		if (!(++timeout % 100))
		{
		    PRINTD(("reading isr %x timeout %d\n", isr, timeout));
		    /* dump_registers(isp); */
		}
	    } while (!(isr & BUS_ISR_RISC_INT));

#ifdef QL_DEBUG
	    if (!disable)
		PRINT5(("P5B "));	/* XXX */
#endif

#endif				/* WAIT_SEMA */

/*
** Let the interrupt service routine know we
** aren't expecting an interrut anymore.
*/
	    ha->flags &= ~AFLG_MAIL_BOX_EXPECTED;
	    if (!disable)
		PRINT3(("cleared AFLG_MAIL_BOX_EXPECTED\n"));

/*
** Process the mailbox response.
*/

	    if (timeout_error)
		goto finishup;
	    {
		u_short         mailbox0 = 0;

/*
** Get a new copy of mailbox 0.
*/
		mailbox0 = PCI_INH(&isp->mailbox0);

/*
** Get the response to our command.
*/
		switch (mailbox0)
		{
		case MBOX_STS_BUSY:
		    {
/*
** Retry forever on BUSY.
*/
			retry_cnt++;

/*
** Print a message every 10 retry.
*/
			if (retry_cnt % 10 == 0)
			{
#ifdef QL_DEBUG
			    cmn_err(CE_WARN,
				    "ql_mbox_cmd - %d Retries", retry_cnt);
#endif
			}
		    }
		    break;

		case MBOX_ASTS_SYSTEM_ERROR:
		    {
#ifdef QL_DEBUG
			u_short         mailbox1 = 0;
			u_short         mailbox2 = 0;

			mailbox1 = PCI_INH(&isp->mailbox1);
			mailbox2 = PCI_INH(&isp->mailbox2);

			cmn_err(CE_WARN,
				"ql_mbox_cmd - ISP Fatal Firmware Error");
			cmn_err(CE_WARN,
			  "ql_mbox_cmd - mailbox: 0=0x%x, 1=0x%x, 2=0x%x\n",
				mailbox0, mailbox1, mailbox2);
#endif
		    }
		    break;

		case MBOX_ASTS_SCSI_BUS_RESET:
		    {
			ha->flags |= AFLG_SEND_MARKER;
			cmn_err(CE_NOTE,
				"ql_mbox_cmd - ISP SCSI bus RESET seen");
		    }
		    break;

		case MBOX_STS_INVALID_COMMAND:
		    {
			PRINT2(("ql_mbox_cmd - ISP BOGUS Command Error cmd %d\n", reg0));
			PRINT2(("ql_mbox_cmd - mailbox: 0=0x%x, \n", mailbox0));
		    }
		    break;

		default:
		    {
/*
** Save away status registers.
*/
			switch (in_cnt)
			{

			case 8:
			    mbox_sts[7] = PCI_INH(&isp->mailbox7);

			case 7:
			    mbox_sts[6] = PCI_INH(&isp->mailbox6);

			case 6:
			    mbox_sts[5] = PCI_INH(&isp->mailbox5);
			case 5:
			    if (reg0 != 0x11)
				mbox_sts[4] = PCI_INH(&isp->mailbox4);
			case 4:
			    mbox_sts[3] = PCI_INH(&isp->mailbox3);
			case 3:
			    mbox_sts[2] = PCI_INH(&isp->mailbox2);
			case 2:
			    mbox_sts[1] = PCI_INH(&isp->mailbox1);
			case 1:
			    mbox_sts[0] = PCI_INH(&isp->mailbox0);
			}

/*
** Indicate that we're done processing.
*/
			complete = 1;
		    }
		}
	    }

#ifdef QL_DEBUG
	    if (!disable)
		PRINT5(("P6 "));	/* XXX */
#endif

/*
** Clear the semaphore lock.
*/

    finishup:

	    PCI_OUTH(&isp->bus_sema, 0);

/*
** If interrupts aren't enabled, then we need to
** clear the interrupt ourselves (interrupt service
** won't ever get it).  This is in INIT stage.
*/
	    if (!(icr & (ICR_ENABLE_ALL_INTS | ICR_ENABLE_RISC_INT)))
	    {
/*
** Clear the risc interrupt.
*/
		PCI_OUTH(&isp->hccr, HCCR_CMD_CLEAR_RISC_INT);
	    }
	} while ((!complete) && (retry_cnt-- > 0));

    }
    /* CRITICAL_END XXX */

#ifdef QL_DEBUG
    if (!disable)
	PRINT5(("P7\n "));	/* XXX */
#endif

#ifdef MBOX_DEBUG
    if (!disable)
	PRINT3(("in %d: [(%x) %x %x %x %x %x %x %x]\n",
	       in_cnt, mbox_sts[0], mbox_sts[1], mbox_sts[2],
	   mbox_sts[3], mbox_sts[4], mbox_sts[5], mbox_sts[6], mbox_sts[7]));	/* XXX */
#endif

#ifndef WAIT_SEMA

/*
** If we weren't waiting on the semaphore flag,
** I may have cleared an interrupt,
** so call the service routine just in case.
**  THIS MAY CAUSE STATISTIC TO BE WEIRD - we count calls to qlintr
*/
    PRINT2(("calling qlintr from ql\n"));
    PRINTD(("extra call to qlintr"));
    qlintr(ha);

#endif				/* WAIT_SEMA */
    return (!complete);
}


#ifdef QL_DEBUG

#ifdef DUMP_REGS

/*
** Dump the registers for this ISP.
*/
static void 
dump_registers(pISP_REGS isp)
{
    u_short         bus_id_low, bus_id_high, bus_config0, bus_config1;
    u_short         bus_icr = 0;
    u_short         bus_isr = 0;
    u_short         bus_sema;
    u_short         hccr = 0;
    u_short         risc_pc;
    u_short         mbox0, mbox1, mbox2, mbox3, mbox4, mbox5;

    u_short         control_dma_control;
    u_short         control_dma_config;
    u_short         control_dma_fifo_status;
    u_short         control_dma_status;
    u_short         control_dma_counter;
    u_short control_dma_address_counter_1;
    u_short control_dma_address_counter_0;
    u_short control_dma_address_counter_3;
    u_short control_dma_address_counter_2;


    u_short         data_dma_control;
    u_short         data_dma_config;
    u_short         data_dma_fifo_status;
    u_short         data_dma_status;
    u_short         data_dma_xfer_counter_high;
    u_short         data_dma_xfer_counter_low;
    u_short data_dma_address_counter_1;
    u_short data_dma_address_counter_0;
    u_short data_dma_address_counter_3;
    u_short data_dma_address_counter_2;


    /* Get copies of the registers.  */
    mbox0 = PCI_INH(&isp->mailbox0);
    mbox1 = PCI_INH(&isp->mailbox1);
    mbox2 = PCI_INH(&isp->mailbox2);
    mbox3 = PCI_INH(&isp->mailbox3);
    mbox4 = PCI_INH(&isp->mailbox4);
    mbox5 = PCI_INH(&isp->mailbox5);

    bus_id_low = PCI_INH(&isp->bus_id_low);
    bus_id_high = PCI_INH(&isp->bus_id_high);
    bus_config0 = PCI_INH(&isp->bus_config0);
    bus_config1 = PCI_INH(&isp->bus_config1);
    bus_icr = PCI_INH(&isp->bus_icr);
    bus_isr = PCI_INH(&isp->bus_isr);
    bus_sema = PCI_INH(&isp->bus_sema);
    hccr = PCI_INH(&isp->hccr);
    PCI_OUTH(&isp->hccr, HCCR_CMD_PAUSE);
    control_dma_control = PCI_INH(&isp->control_dma_control);
    control_dma_config = PCI_INH(&isp->control_dma_config);
    control_dma_fifo_status = PCI_INH(&isp->control_dma_fifo_status);
    control_dma_status = PCI_INH(&isp->control_dma_status);
    control_dma_counter = PCI_INH(&isp->control_dma_counter);
    control_dma_address_counter_1 = PCI_INH(&isp->control_dma_address_counter_1);
    control_dma_address_counter_0 = PCI_INH(&isp->control_dma_address_counter_0);
    control_dma_address_counter_3 = PCI_INH(&isp->control_dma_address_counter_3);
    control_dma_address_counter_2 = PCI_INH(&isp->control_dma_address_counter_2);


    data_dma_config = PCI_INH(&isp->data_dma_config);
    data_dma_fifo_status = PCI_INH(&isp->data_dma_fifo_status);
    data_dma_status = PCI_INH(&isp->data_dma_status);
    data_dma_control = PCI_INH(&isp->data_dma_control);
    data_dma_xfer_counter_high = PCI_INH(&isp->data_dma_xfer_counter_high);
    data_dma_xfer_counter_low = PCI_INH(&isp->data_dma_xfer_counter_low);
    data_dma_address_counter_1 = PCI_INH(&isp->data_dma_address_counter_1);
    data_dma_address_counter_0 = PCI_INH(&isp->data_dma_address_counter_0);
    data_dma_address_counter_3 = PCI_INH(&isp->data_dma_address_counter_3);
    data_dma_address_counter_2 = PCI_INH(&isp->data_dma_address_counter_1);


    risc_pc = PCI_INH(&isp->risc_pc);

    PCI_OUTH(&isp->hccr, HCCR_CMD_RELEASE);
    printf("\ndump_regs: mbox0 = %x, mbox1 = %x\n", mbox0, mbox1);
    printf("dump_regs: mbox2 = %x, mbox3 = %x\n", mbox2, mbox3);
    printf("dump_regs: mbox4 = %x, mbox5 = %x\n", mbox4, mbox5);
    printf("dump_regs: BUS_ID_LOW=%x, BUS_ID_HIGH=%x\n",
	   bus_id_low, bus_id_high);
    printf("dump_regs: BUS_CONFIG0=%x, BUS_CONFIG1=%x\n",
	   bus_config0, bus_config1);
    printf("dump_regs: BUS_ICR=%x, BUS_ISR=%x, BUS_SEMA=%x\n",
	   bus_icr, bus_isr, bus_sema);
    printf("dump_regs: HCCR=%x: %x  \n", &isp->hccr, hccr);
    printf("risc PC : %x\n", risc_pc);
    printf("dump_regs: control_dma_control %x: %x\n",
	   &isp->control_dma_control, control_dma_control);
    printf("dump_regs: control_dma_config %x: %x\n",
	   &isp->control_dma_config, control_dma_config);
    printf("dump_regs: control_dma_fifo_status %x: %x\n",
	   &isp->control_dma_fifo_status, control_dma_fifo_status);
    printf("dump_regs: control_dma_status %x: %x\n",
	   &isp->control_dma_status, control_dma_status);
    printf("dump_regs: control_dma_counter %x: %x\n",
	   &isp->control_dma_counter, control_dma_counter);
    printf("dump_regs: control_dma_address_counter_1 %x: %x\n",
        &isp->control_dma_address_counter_1, control_dma_address_counter_1);
    printf("dump_regs: control_dma_address_counter_0 %x: %x\n",
        &isp->control_dma_address_counter_0, control_dma_address_counter_0);
    printf("dump_regs: control_dma_address_counter_3 %x: %x\n",
        &isp->control_dma_address_counter_3, control_dma_address_counter_3);
    printf("dump_regs: control_dma_address_counter_0 %x: %x\n",
        &isp->control_dma_address_counter_2, control_dma_address_counter_2);
    printf("dump_regs: data_dma_config %x: %x\n",
	   &isp->data_dma_config, data_dma_config);
    printf("dump_regs: data_dma_status %x: %x\n",
	   &isp->data_dma_status, data_dma_status);
    printf("dump_regs: data_dma_fifo_status %x: %x\n",
	   &isp->data_dma_fifo_status, data_dma_fifo_status);
    printf("dump_regs: data_dma_control %x: %x\n",
	   &isp->data_dma_control, data_dma_control);
    printf("dump_regs: data_dma_xfer_counter_high %x: %x\n",
	   &isp->data_dma_xfer_counter_high, data_dma_xfer_counter_high);
    printf("dump_regs: data_dma_xfer_counter_low %x: %x\n",
	   &isp->data_dma_xfer_counter_low, data_dma_xfer_counter_low);
    printf("dump_regs: data_dma_address_counter_1 %x: %x\n",
        &isp->data_dma_address_counter_1,
        data_dma_address_counter_1);
    printf("dump_regs: data_dma_address_counter_0 %x: %x\n",
        &isp->data_dma_address_counter_0,
        data_dma_address_counter_0);
    printf("dump_regs: data_dma_address_counter_3 %x: %x\n",
        &isp->data_dma_address_counter_3,
        data_dma_address_counter_3);
    printf("dump_regs: data_dma_address_counter_2 %x: %x\n",
        &isp->data_dma_address_counter_2,
        data_dma_address_counter_2);

}

#endif				/* DUMP_REGS */

#ifdef DUMP_PARMS

/*
** Dump FIRMWARE parameters.
*/
static void 
dump_parameters(int board)
{
    pHA_INFO        ha = ha_info[board];
    u_short         mbox_sts[8];

    if(!ha) {
    	printf("no host adapter structure for board %x\n",board);
	return;
    }

    PRINTD(("dump_parameters for board %x\n",board));

    if (ql_mbox_cmd(ha, mbox_sts, 1, 6,
		    MBOX_CMD_GET_FIRMWARE_STATUS,
		    0, 0, 0, 0, 0, 0, 0))
    {
	cmn_err(CE_WARN,
		"ql_dump_parameters - GET_FIRMWARE_STATUS Command Failed");
    } else
    {
	PRINTD(("ql_dump_parameters - GET_FIRMWARE_STATUS\n"));
	PRINTD(("    %x, %x, %x, %x, %x, %x, %x, %x\n",
	       mbox_sts[0], mbox_sts[1], mbox_sts[2], mbox_sts[3],
	       mbox_sts[4], mbox_sts[5], mbox_sts[6], mbox_sts[7]));
    }

    if (ql_mbox_cmd(ha, mbox_sts, 1, 6,
		    MBOX_CMD_GET_INITIATOR_SCSI_ID,
		    0, 0, 0, 0, 0, 0, 0))
    {
	cmn_err(CE_WARN,
		"ql_dump_parameters - GET_INITIATOR_SCSI_ID Command Failed");
    } else
    {
	PRINTD(("ql_dump_parameters - GET_INITIATOR_SCSI_ID\n"));
	PRINTD(("    %x, %x, %x, %x, %x, %x\n",
	       mbox_sts[0], mbox_sts[1], mbox_sts[2],
	       mbox_sts[3], mbox_sts[4], mbox_sts[5]));
    }


    if (ql_mbox_cmd(ha, mbox_sts, 1, 6,
		    MBOX_CMD_GET_SELECTION_TIMEOUT,
		    0, 0, 0, 0, 0, 0, 0))
    {
	cmn_err(CE_WARN,
		"ql_dump_parameters - GET_SELECTION_TIMEOUT Command Failed");
    } else
    {
	PRINTD(("ql_dump_parameters - GET_SELECTION_TIMEOUT\n"));
	PRINTD(("    %x, %x, %x, %x, %x, %x\n",
	       mbox_sts[0], mbox_sts[1], mbox_sts[2],
	       mbox_sts[3], mbox_sts[4], mbox_sts[5]));
    }


    if (ql_mbox_cmd(ha, mbox_sts, 1, 6,
		    MBOX_CMD_GET_RETRY_COUNT,
		    0, 0, 0, 0, 0, 0, 0))
    {
	cmn_err(CE_WARN,
		"ql_dump_parameters - GET_RETRY_COUNT Command Failed");
    } else
    {
	PRINTD(("ql_dump_parameters - GET_RETRY_COUNT\n"));
	PRINTD(("    %x, %x, %x, %x, %x, %x\n",
	       mbox_sts[0], mbox_sts[1], mbox_sts[2],
	       mbox_sts[3], mbox_sts[4], mbox_sts[5]));
    }


    if (ql_mbox_cmd(ha, mbox_sts, 1, 6,
		    MBOX_CMD_GET_TAG_AGE_LIMIT,
		    0, 0, 0, 0, 0, 0, 0))
    {
	cmn_err(CE_WARN,
		"ql_dump_parameters - GET_TAG_AGE_LIMIT Command Failed");
    } else
    {
	PRINTD(("ql_dump_parameters - GET_TAG_AGE_LIMIT\n"));
	PRINTD(("    %x, %x, %x, %x, %x, %x\n",
	       mbox_sts[0], mbox_sts[1], mbox_sts[2],
	       mbox_sts[3], mbox_sts[4], mbox_sts[5]));
    }


    if (ql_mbox_cmd(ha, mbox_sts, 1, 6,
		    MBOX_CMD_GET_CLOCK_RATE,
		    0, 0, 0, 0, 0, 0, 0))
    {
	cmn_err(CE_WARN,
		"ql_dump_parameters - GET_CLOCK_RATE Command Failed");
    } else
    {
	PRINTD(("ql_dump_parameters - GET_CLOCK_RATE\n"));
	PRINTD(("    %x, %x, %x, %x, %x, %x\n",
	       mbox_sts[0], mbox_sts[1], mbox_sts[2],
	       mbox_sts[3], mbox_sts[4], mbox_sts[5]));
    }


    if (ql_mbox_cmd(ha, mbox_sts, 1, 6,
		    MBOX_CMD_GET_ACTIVE_NEGATION_STATE,
		    0, 0, 0, 0, 0, 0, 0))
    {
	cmn_err(CE_WARN,
	   "ql_dump_parameters - GET_ACTIVE_NEGATION_STATE Command Failed");
    } else
    {
	PRINTD(("ql_dump_parameters - GET_ACTIVE_NEGATION_STATE\n"));
	PRINTD(("    %x, %x, %x, %x, %x, %x\n",
	       mbox_sts[0], mbox_sts[1], mbox_sts[2],
	       mbox_sts[3], mbox_sts[4], mbox_sts[5]));
    }


    if (ql_mbox_cmd(ha, mbox_sts, 1, 6,
		    MBOX_CMD_GET_ASYNC_DATA_SETUP_TIME,
		    0, 0, 0, 0, 0, 0, 0))
    {
	cmn_err(CE_WARN,
	   "ql_dump_parameters - GET_ASYNC_DATA_SETUP_TIME Command Failed");
    } else
    {
	PRINTD(("ql_dump_parameters - GET_ASYNC_DATA_SETUP_TIME\n"));
	PRINTD(("    %x, %x, %x, %x, %x, %x\n",
	       mbox_sts[0], mbox_sts[1], mbox_sts[2],
	       mbox_sts[3], mbox_sts[4], mbox_sts[5]));
    }


    if (ql_mbox_cmd(ha, mbox_sts, 1, 6,
		    MBOX_CMD_GET_BUS_CONTROL_PARAMETERS,
		    0, 0, 0, 0, 0, 0, 0))
    {
	cmn_err(CE_WARN,
	  "ql_dump_parameters - GET_BUS_CONTROL_PARAMETERS Command Failed");
    } else
    {
	PRINTD(("ql_dump_parameters - GET_BUS_CONTROL_PARAMETERS\n"));
	PRINTD(("    %x, %x, %x, %x, %x, %x\n",
	       mbox_sts[0], mbox_sts[1], mbox_sts[2],
	       mbox_sts[3], mbox_sts[4], mbox_sts[5]));
    }


    if (ql_mbox_cmd(ha, mbox_sts, 2, 6,
		    MBOX_CMD_GET_TARGET_PARAMETERS,
		    0,		/* SCSI id */
		    0, 0, 0, 0, 0, 0))
    {
	cmn_err(CE_WARN,
		"ql_dump_parameters - GET_TARGET_PARAMETERS Command Failed");
    } else
    {
	PRINTD(("ql_dump_parameters - GET_TARGET_PARAMETERS\n"));
	PRINTD(("    %x, %x, %x, %x, %x, %x\n",
	       mbox_sts[0], mbox_sts[1], mbox_sts[2],
	       mbox_sts[3], mbox_sts[4], mbox_sts[5]));
    }


    if (ql_mbox_cmd(ha, mbox_sts, 2, 6,
		    MBOX_CMD_GET_DEVICE_QUEUE_PARAMETERS,
		    0,		/* SCSI id */
		    0, 0, 0, 0, 0, 0))
    {
	cmn_err(CE_WARN,
	 "ql_dump_parameters - GET_DEVICE_QUEUE_PARAMETERS Command Failed");
    } else
    {
	PRINTD(("ql_dump_parameters - GET_DEVICE_QUEUE_PARAMETERS\n"));
	PRINTD(("    %x, %x, %x, %x, %x, %x\n",
	       mbox_sts[0], mbox_sts[1], mbox_sts[2],
	       mbox_sts[3], mbox_sts[4], mbox_sts[5]));
    }


}

#endif				/* DUMP_PARMS */


#ifdef DUMP_IOCB

static void 
dump_iocb(command_entry * q_ptr)
{
    int             i;
    char           *ptr;

    ptr = (char *) q_ptr;
    printf("RAW data: ");
    for (i = 0; i <= 64; i++)
	printf(" %x", *ptr++);
    printf("\n");


    printf("------------- IOCB -------------  %x\n", q_ptr);
    printf("\n");
    printf("Entry Count\t:\t%x\n", q_ptr->hdr.entry_cnt);
    printf("Entry Type\t:\t%x\n", q_ptr->hdr.entry_type);
    printf("Flags\t\t: \t%x\n", q_ptr->hdr.flags);
    printf("System Defined\t:\t%x\n", q_ptr->hdr.sys_def_1);
    printf("Handle\t\t:\t%x\n", q_ptr->handle);
    printf("Target ID\t:\t%x\n", q_ptr->target_id);
    printf("LUN\t\t:\t%x\n", q_ptr->target_lun);
    printf("CDB Length\t:\t%x\n", q_ptr->cdb_length);
    printf("Control Flags\t:\t%x\n", q_ptr->control_flags);
    printf("Timeout\t\t:\t%x\n", q_ptr->time_out);
    printf("Data Seg Count\t:\t%x\n", q_ptr->segment_cnt);
    switch (q_ptr->cdb_length)
    {
    case SC_CLASS0_SZ:
	printf("CDB\t:\t%x %x %x %x %x %x\n", q_ptr->cdb0, q_ptr->cdb1, q_ptr->cdb2, q_ptr->cdb3, q_ptr->cdb4, q_ptr->cdb5);
	break;

    case SC_CLASS2_SZ:
	printf("CDB\t:\t%x %x %x %x %x %x %x %x\n", q_ptr->cdb0, q_ptr->cdb1, q_ptr->cdb2, q_ptr->cdb3, q_ptr->cdb4, q_ptr->cdb5, q_ptr->cdb6, q_ptr->cdb7);
	break;
    case SC_CLASS1_SZ:
	printf("CDB\t:\t%x %x %x %x %x %x %x %x %x %x %x %x\n", q_ptr->cdb0, q_ptr->cdb1, q_ptr->cdb2, q_ptr->cdb3, q_ptr->cdb4, q_ptr->cdb5, q_ptr->cdb6, q_ptr->cdb7, q_ptr->cdb8, q_ptr->cdb9, q_ptr->cdb10, q_ptr->cdb11);
	break;
    }
    printf("Data Segment 1:  Address %x\tCount %x\n", q_ptr->dseg[0].base_low_32, q_ptr->dseg[0].count);
    printf("Data Segment 2:  Address %x\tCount %x\n", q_ptr->dseg[1].base_low_32, q_ptr->dseg[1].count);
    printf("Data Segment 3:  Address %x\tCount %x\n", q_ptr->dseg[2].base_low_32, q_ptr->dseg[2].count);
    printf("Data Segment 4:  Address %x\tCount %x\n", q_ptr->dseg[3].base_low_32, q_ptr->dseg[3].count);


}
#endif

#ifdef QL_DEBUG
static void 
dump_iorb(status_entry * q_ptr)
{
    int             i;
    char           *ptr;

    ptr = (char *) q_ptr;
    printf("RAW data: ");
    for (i = 0; i <= 64; i++)
	printf(" %x", *ptr++);
    printf("\n");

    printf("------------- IORB -------------  %x\n", q_ptr);
    printf("Entry Count\t:\t%x\n", q_ptr->hdr.entry_cnt);
    printf("Entry Type\t:\t%x\n", q_ptr->hdr.entry_type);
    printf("Flags\t\t:\t%x\n", q_ptr->hdr.flags);
    printf("System Defined\t:\t%x\n", q_ptr->hdr.sys_def_1);
    printf("Handle\t\t:\t%x\n", q_ptr->handle);
    printf("Scsi Status\t:\t%x\n", q_ptr->scsi_status);
    printf("Comp. Status\t:\t%x\n", q_ptr->completion_status);
    printf("State Flags\t:\t%x\n", q_ptr->state_flags);
    printf("Status Flags\t:\t%x\n", q_ptr->status_flags);
    printf("Time\t\t:\t%x\n", q_ptr->time);
    printf("Sense Length\t:\t%x\n", q_ptr->req_sense_length);
    printf("Timeout\t\t:\t%x\n", q_ptr->time);
    printf("Residual\t:\t%x\n", q_ptr->residual);
    printf("Request Sense : %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x\n", q_ptr->req_sense_data[3], q_ptr->req_sense_data[2], q_ptr->req_sense_data[1], q_ptr->req_sense_data[0], q_ptr->req_sense_data[7], q_ptr->req_sense_data[6], q_ptr->req_sense_data[5], q_ptr->req_sense_data[4], q_ptr->req_sense_data[11], q_ptr->req_sense_data[10], q_ptr->req_sense_data[9], q_ptr->req_sense_data[8], q_ptr->req_sense_data[15], q_ptr->req_sense_data[14], q_ptr->req_sense_data[13], q_ptr->req_sense_data[12]);
    printf("Request Sense : %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x\n", q_ptr->req_sense_data[19], q_ptr->req_sense_data[18], q_ptr->req_sense_data[17], q_ptr->req_sense_data[16], q_ptr->req_sense_data[23], q_ptr->req_sense_data[22], q_ptr->req_sense_data[21], q_ptr->req_sense_data[20], q_ptr->req_sense_data[27], q_ptr->req_sense_data[26], q_ptr->req_sense_data[25], q_ptr->req_sense_data[24], q_ptr->req_sense_data[31], q_ptr->req_sense_data[30], q_ptr->req_sense_data[29], q_ptr->req_sense_data[28]);


}
static void 
dump_continuation(continuation_entry * q_ptr)
{
    int             i;
    char           *ptr;

    ptr = (char *) q_ptr;
    PRINT3(("RAW data: "));
    for (i = 0; i <= 64; i++)
	PRINT3((" %x", *ptr++));
    PRINT3(("\n"));

    PRINT3(("------------- CONTINUATION -------------  %x\n", q_ptr));
    PRINT3(("Entry Count\t:\t%x\n", q_ptr->hdr.entry_cnt));
    PRINT3(("Entry Type\t:\t%x\n", q_ptr->hdr.entry_type));
    PRINT3(("Flags\t\t:\t%x\n", q_ptr->hdr.flags));
    PRINT3(("System Defined\t:\t%x\n", q_ptr->hdr.sys_def_1));
    PRINT3(("Data Segment 1:  Address %x\tCount %x\n", q_ptr->dseg[0].base_low_32, q_ptr->dseg[0].count));
    PRINT3(("Data Segment 2:  Address %x\tCount %x\n", q_ptr->dseg[1].base_low_32, q_ptr->dseg[1].count));
    PRINT3(("Data Segment 3:  Address %x\tCount %x\n", q_ptr->dseg[2].base_low_32, q_ptr->dseg[2].count));
    PRINT3(("Data Segment 4:  Address %x\tCount %x\n", q_ptr->dseg[3].base_low_32, q_ptr->dseg[3].count));
    PRINT3(("Data Segment 5:  Address %x\tCount %x\n", q_ptr->dseg[4].base_low_32, q_ptr->dseg[4].count));
    PRINT3(("Data Segment 6:  Address %x\tCount %x\n", q_ptr->dseg[5].base_low_32, q_ptr->dseg[5].count));
    PRINT3(("Data Segment 7:  Address %x\tCount %x\n", q_ptr->dseg[6].base_low_32, q_ptr->dseg[6].count));

}



static void 
dump_marker(marker_entry * q_ptr)
{
    PRINT3(("------------- MARKER -------------  %x\n", q_ptr));
    PRINT3(("Entry Count\t:\t%x\n", q_ptr->hdr.entry_cnt));
    PRINT3(("Entry Type\t:\t%x\n", q_ptr->hdr.entry_type));
    PRINT3(("Flags\t\t:\t%x\n", q_ptr->hdr.flags));
    PRINT3(("System Defined\t:\t%x\n", q_ptr->hdr.sys_def_1));
    PRINT3(("Target\t\t:\t%x\n", q_ptr->target_id));
    PRINT3(("Lun\t\t:\t%x\n", q_ptr->target_lun));
    PRINT3(("Modifier\t:\t%x\n", q_ptr->modifier));


}

#endif				/* DUMP_IOCB */

#if 0				/* not used right now.  avoid compiler
				 * warning */
static void 
dump_request(scsi_request_t * request)
{
    int             i;
    char           *ptr;
    printf("------------- SCSI REQUEST -------------  %x\n", request);
    printf("Controller\t:\t%x\n", request->sr_ctlr);
    printf("Target\t\t:\t%x\n", request->sr_target);
    printf("Lun\t\t:\t\t%x\n", request->sr_lun);
    printf("Tag\t\t:\t%x\n", request->sr_tag);
    printf("Command\t\t:\t");
    ptr = (char *) request->sr_command;
    for (i = 0; i < request->sr_cmdlen; i++)
	printf("%x ", *ptr++);
    printf("\n");

    printf("Command Length\t:\t%x\n", request->sr_cmdlen);
    printf("Flags\t\t:\t%x\n", request->sr_flags);
    printf("Timeout\t\t:\t%x\n", request->sr_timeout);
    printf("Buffer pointer\t:\t%x\n", request->sr_buffer);
    printf("Buffer length\t:\t%x\n", request->sr_buflen);
    printf("Sense pointer\t:\t%x\n", request->sr_sense);
    printf("Sense length\t:\t%x\n", request->sr_senselen);
    printf("buf_t pointer\t:\t%x\n", request->sr_bp);


}

#endif				/* 0 */
#endif				/* QL_DEBUG */

void 
qlcommand(struct scsi_request * req)
{
/* zero out the status flags and sensegotten stuff */
/* this fixes the case when we do retries and these fields are */
/* left from the previous IO */
    req->sr_status = SC_HARDERR;
    req->sr_scsi_status = req->sr_ha_flags = req->sr_sensegotten = 0;
    req->sr_spare = NULL;
    req->sr_ha = NULL;

    PRINT3(("qlcommand: request for slot %x\n", req->sr_ctlr));
    ql_entry(req);
}



struct scsi_target_info *
qlinfo(u_char extctlr, u_char targ, u_char lun)
{
    struct scsi_request *req;
    int             retry = 1;
    static u_char   scsi[6];
    char            name[9];
    struct scsi_target_info *retval;
    scsi_target_info_t *info;
    u_char          board;

    board = extctlr;

    PRINT3(("qlinfo: extctlr %x, targ %x, lun %x\n", extctlr, targ, lun));

    info = &tinfo[board][targ][lun];
    if (info->si_inq == NULL)
    {
	info->si_inq = (u_char *) dmabuf_malloc(SCSI_INQUIRY_LEN);
	info->si_sense = (u_char *) dmabuf_malloc(SCSI_SENSE_LEN);
	PRINT3(("\ninquiry and sense allocated for board %x, targ %x lun %x inquire %x sense %x\n", board, targ, lun, info->si_inq, info->si_sense));
    }
    bzero(info->si_inq, SCSI_INQUIRY_LEN);

    req = malloc(sizeof(*req));

retryit:

    bzero(req, sizeof(*req));

    scsi[0] = 0x12;		/* Inquiry command */
    scsi[1] = scsi[2] = scsi[3] = scsi[5] = 0;
    scsi[4] = SCSI_INQUIRY_LEN;
    req->sr_ctlr = extctlr;
    req->sr_target = targ;
    req->sr_lun = lun;
    req->sr_command = scsi;
    req->sr_cmdlen = 6;		/* Inquiry command length */
    req->sr_flags = SRF_DIR_IN | SRF_FLUSH | SRF_AEN_ACK;
    req->sr_timeout = 10 * HZ;
    req->sr_buffer = info->si_inq;
    req->sr_buflen = SCSI_INQUIRY_LEN;
    req->sr_sense = NULL;
    req->sr_dev = 0;

    DCACHE_INVAL(info->si_inq, SCSI_INQUIRY_LEN);

    qlcommand(req);

    if (req->sr_status == SC_GOOD && req->sr_scsi_status == ST_GOOD)
    {

	PRINT3(("qlinfo: good status on inquiry sr_status %x sr_scsi_status %x\n", req->sr_status, req->sr_scsi_status));
	retval = info;
	PRINTD(("qlinfo: REGISTER device inquire data %x controller %x target %x\n", info->si_inq, extctlr, targ));

    } else
    {

	PRINT3(("qlinfo failed:  sr_status %x, sr_scsi_status %x\n",
	       req->sr_status, req->sr_scsi_status));
	if (retry-- && req->sr_status == SC_BUS_RESET)
	{
	    PRINT2(("qlinfo: retrying because of bus reset\n"));
	    goto retryit;
	}
	retval = NULL;

    }

    free(req);
    return retval;
}

int
qlalloc(u_char extctlr, u_char targ, u_char lun, int opt,
	void (*cb) ())
{

    int             adap = extctlr;
    int             retval = SCSIALLOCOK;

    pHA_INFO        ha = ha_info[adap];
    PRINT3(("qlalloc : controller %x, target %x, lun %x, option %x, callback %x\n", extctlr, targ, lun, opt, cb));

    if (qlinfo(extctlr, targ, lun) == NULL)
    {
	PRINTD(("qlalloc: qlinfo returned null -- FAIL\n"));
	return 0;
    }
    if (adap >= SC_MAXADAP || targ >= QL_MAXTARG || lun >= QL_MAXLUN ||
	ha == NULL ||
	targ == ha->ql_defaults.Initiator_SCSI_Id)
	return ENODEV;

#if 0	/* XXX ref_count never decremented.  No need for stand exclusive
	 * open now either, so ref_count has been removed, and dev_flags
	 * has just been made a flag for initialized...(jeffs)
	 */
    if (ha->dev_flags[targ][lun] & DFLG_EXCLUSIVE)
    {
	retval = EBUSY;
	PRINTD(("ha->dev_flags[targ][lun] %x\n", ha->dev_flags[targ][lun]));
	goto _allocout;
    }
    if (opt & SCSIALLOC_EXCLUSIVE)
    {
	if (ha->ref_count[targ][lun] != 0)
	{
	    retval = EBUSY;
	    PRINTD(("qlalloc: ref_count is %x\n", ha->ref_count[targ][lun]));
	    goto _allocout;
	} else
	    ha->dev_flags[targ][lun] |= DFLG_EXCLUSIVE;
    }
    ha->ref_count[targ][lun]++;
#endif

_allocout:
    PRINT3(("qlalloc: extctlr %x, targ %x, lun %x opt %x,callback %x\n", extctlr, targ, lun, opt, cb));
    return retval;
}

int 
qlreset(u_char adap)
{
    pHA_INFO        ha = ha_info[adap];

    u_short         mbox_sts[8];

    PRINT1(("qlreset: about to issue a bus reset for adapter %x\n", adap));

    if (ha == NULL)
	return (0);		/* fail */

    if (ql_mbox_cmd(ha, mbox_sts, 2, 2,
		    MBOX_CMD_BUS_RESET, ha->ql_defaults.Delay_after_reset
		    ,0, 0, 0, 0, 0, 0))
    {
	cmn_err(CE_WARN,
		"qlreset - SCSI BUS RESET Command Failed");
	return (0);		/* fail */
    } else
    {
	PRINTD(("qlreset - SCSI BUS RESET FIRMWARE Command succeeded\n"));
	return (1);		/* success */
    }

}


#ifdef IP30
int
qlinstall(COMPONENT *c,slotinfo_t *slot)
{
    bridge_t       *bridge;
    bridgereg_t     dev_reg;
    __uint32_t      mem_base;
    volatile __uint32_t *mem_base_ptr;
    int tmp;

#ifndef SWAP_DATA_STREAM
#define	DEV_ATTRIBUTES	(BRIDGE_DEV_PREF |		\
			 BRIDGE_DEV_COH |		\
			 BRIDGE_DEV_DEV_IO_MEM)
#else
#define	DEV_ATTRIBUTES	(BRIDGE_DEV_PREF |		\
			 BRIDGE_DEV_SWAP_DIR |          \
			 BRIDGE_DEV_COH |		\
			 BRIDGE_DEV_DEV_IO_MEM)
#endif				/* SWAP_DATA_STREAM */

#define	NUMBER_OF_RRB	2
#if BASEIO_PCI_BUS_ONLY
    {
	__psunsigned_t	bus_base_phys;
	/*
	 * While our base addresses have been made
	 * available in the slot structure, they are
	 * ignored and the rest of the driver uses
	 * hardwired addresses corresponding to the PCI
	 * bus on the IP30 motherboard. If someone plugs
	 * in a PCI bus that has a QL on it (like an IO6
	 * card), the driver does the wrong thing.
	 * Until this is made to work, we ignore any QL
	 * controllers found that are not on the baseio
	 * PCI bus.
	 *
	 * The dependency is due to the use of
	 * BRIDGE_BASE in qlconfigure.
	 */

	/* UNKNOWN_BUS_BASE is an arbitrary value
	 * that we know will never really be a
	 * physical address for bus_base.
	 * If we start getting "unable to translate"
	 * we should extend the cascaded IFs to
	 * handle whatever virtual address is coming in.
	 */
#define	UNKNOWN_BUS_BASE	0xDEADBEEF

	/*
	 * PHYS_TO_XTALK_SLOT tries to change a physical
	 * address into the crosstalk slot number.
	 * Currently, it knows that physical addresses
	 * in the second 128MB map 16M each for slots
	 * eight through fifteen.
	 * Tune this, of course, as necessary.
	 */
#define	PHYS_TO_XTALK_SLOT(a)	((((a)>>27)==3) ? (((a)>>24)&0xF) : -1)

	/* XXX can we simplify this code on IP30? */
	if (IS_COMPAT_PHYS(slot->bus_base))
	    bus_base_phys = COMPAT_TO_PHYS(slot->bus_base);
#ifdef IS_XKPHYS
	else if (IS_XKPHYS(slot->bus_base))
	    bus_base_phys = XKPHYS_TO_PHYS(slot->bus_base);
#endif
	else if (IS_KSEG0(slot->bus_base))
	    bus_base_phys = K0_TO_PHYS(slot->bus_base);
	else if (IS_KSEG1(slot->bus_base))
	    bus_base_phys = K1_TO_PHYS(slot->bus_base);
	else if (IS_KSEG2(slot->bus_base))
	    bus_base_phys = kvtophys(slot->bus_base);
	else if (IS_KPHYS(slot->bus_base))
	    bus_base_phys = (__psunsigned_t)(slot->bus_base);
	else
	    bus_base_phys = UNKNOWN_BUS_BASE;

	if (bus_base_phys != BRIDGE_BASE) {
if (bus_base_phys == UNKNOWN_BUS_BASE)
printf("ql_install WARNING:"
       " unable to support QL in slot %d of PCI Bus"
       " at KVA 0x%X\n",
       slot->bus_slot, slot->bus_base);
#ifdef	PHYS_TO_XTALK_SLOT
else if (PHYS_TO_XTALK_SLOT(bus_base_phys) != -1)
printf("ql_install WARNING:"
       " unable to support QL in slot %d of PCI Bus"
       " in XTalk port %X\n",
       slot->bus_slot, PHYS_TO_XTALK_SLOT(bus_base_phys));
#endif
else
printf("ql_install WARNING:"
       " unable to support QL in slot %d of PCI Bus"
       " at PHYS 0x%X\n",
       slot->bus_slot, bus_base_phys);
	    return 1;
	}
    }
#endif

    PRINT1(("ql_install: QL SCSI controller found in PCI slot %d\n",
	   slot->bus_slot));
    PRINT2(("DEV_ATTRIBUTES are %x\n",DEV_ATTRIBUTES));


    /* use the mem address set up by init_bridge_pci() earlier */
    mem_base_ptr = (volatile __uint32_t *) slot->cfg_base +
	PCI_CFG_BASE_ADDR_1 / sizeof(__uint32_t);
    mem_base = (*mem_base_ptr & ~0xfffff) >> 20;

    bridge = (bridge_t *) slot->bus_base;
    dev_reg = bridge->b_device[slot->bus_slot].reg & ~BRIDGE_DEV_OFF_MASK;
    dev_reg |= mem_base | DEV_ATTRIBUTES;
    bridge->b_device[slot->bus_slot].reg = dev_reg;

    tmp = alloc_bridge_rrb((void *) bridge, slot->bus_slot, 0, NUMBER_OF_RRB);
    if (tmp != NUMBER_OF_RRB) {
	((tmp == 0) ? panic : (void(*)(char *,...))printf)
		("Cannot allocate %d RRBs for ql %d\n",NUMBER_OF_RRB,
		 slot->bus_slot);
    }

    stand_qlhinv(c,slot);

    return 0;
}

static slotdrvr_id_t qlids[] = {
	{BUS_PCI, QLogic_VENDOR_ID, QLogic_DEVICE_ID, REV_ANY},
	{BUS_NONE, 0, 0, 0}
};
drvrinfo_t ql_drvrinfo = { qlids, "ql" };

#endif				/* IP30 */

/* if we get to this routine then the Adaptor has been probed succesfully */
/* first register the adaptor then go walk that bus looking for devices   */
static COMPONENT scsiadaptmpl = {
    AdapterClass,		/* Class */
    SCSIAdapter,		/* Type */
    0,				/* Flags */
    SGI_ARCS_VERS,		/* Version */
    SGI_ARCS_REV,		/* Revision */
    0,				/* Key */
    0x01,			/* Affinity */
    0,				/* ConfigurationDataSize */
    10,				/* IdentifierLength */
    "QLISP1040"			/* Identifier */
};
static COMPONENT scsitmpl, scsictrltmpl;

#ifdef IP30
static void
stand_qlhinv(COMPONENT *c, slotinfo_t *slot)
{
    static int	adap;		/* incremented every call to this routine */

    PRINT2(("stand_qlhinv: registering host adaptor %x\n", adap));

    c = AddChild(c, &scsiadaptmpl, (void *) NULL);
    if (c == (COMPONENT *) NULL)
	cpu_acpanic("scsi adapter");

    c->Key = adap;

    /* fill in unchanged parts of templates */
    bzero(&scsictrltmpl, sizeof(COMPONENT));
    scsictrltmpl.Class = ControllerClass;
    scsictrltmpl.Version = SGI_ARCS_VERS;
    scsictrltmpl.Revision = SGI_ARCS_REV;
    scsitmpl.Class = PeripheralClass;
    scsitmpl.Version = SGI_ARCS_VERS;
    scsitmpl.Revision = SGI_ARCS_REV;
    scsitmpl.AffinityMask = 0x01;

    PRINT2(("calling bus walk for adapter %x\n", c->Key));
    qlconfigure(c,slot);
    adap++;
}
#endif

#ifdef SN0 /* for MSCSI bringup only */

static void
mscsi_debug(int adap, __psunsigned_t ql_register_base)
{
	short	*sp ;
	short	stmp ;

	sp = (short *)ql_register_base ;
	printf("A = %d, HA = %x, \n", adap, sp) ;

	us_delay(10000) ;
	*(sp + 0xc2/2) = 0x2000 ; /* pause risc */
	printf("C2 %x ", *(sp + 0xc2/2)) ;
	us_delay(10000) ;
	*(sp + 0x4/2)  = 0x4c ;   /* select SXP */
	printf("4 %x ", *(sp + 0x4/2)) ;
	us_delay(10000) ;

	stmp =  *(sp + 0xf4/2) ;
	printf("\nDiff Ctrl Reg = %x, ", stmp) ;
	us_delay(10000) ;
	stmp = *(sp + 0xB4/2) ;
	printf("GPIO Enable = %x, ", stmp) ;
	us_delay(10000) ;
	stmp = *(sp + 0xB6/2) ;
	printf("GPIO Data = %x\n", stmp) ;

	us_delay(10000) ;
	*(sp + 0x4/2)  = 0x44 ;   /* select SXP */
	us_delay(10000) ;
	*(sp + 0xc2/2) = 0 ; /* pause risc */
	us_delay(10000) ;

}
#endif /* for MSCSI bringup only */

void
#if defined(IP30)
qlconfigure(COMPONENT * c,slotinfo_t *slot)
#elif defined(SN0)
#ifdef SN_PDI
qlconfigure(pd_inst_hdl_t *pdih, pd_inst_parm_t *pdip)
#else /* SN_PDI */
qlconfigure(COMPONENT * c, char *hwgpath, char *nicstr)
#endif /* SN_PDI */
#else
qlconfigure(COMPONENT * c)
#endif
{
#ifdef SN_PDI
    	int             adap ;
    	char		*hwgpath ;
	char 		*nicstr = NULL ;
#else
    int             adap = (int) (c->Key & 0xff);
#endif
    int             num_disks = 0;
    int             type;
    int		    max_targets= QL_MAXTARG;
    char           *extra, *inv;
    union scsicfg   cfgd;
    char            id[26];	/* 8 + ' ' + 16 + null */
    u_char          targ, lu;
    COMPONENT      *tmp, *tmp2;
    pHA_INFO        ha;
    pPCI_REG        isp_config;
    scsi_target_info_t *info;

    __psunsigned_t  ql_config_base,	/* PCI Config space for this device */
                    ql_register_base;	/* PIO space for qlogic registers */

#ifdef SN0
	int             i, rc;
	lboard_t        *brd_ptr;
	klinfo_t        *comp_ptr = NULL;
	ULONG		Key ;

#ifdef SN_PDI
	hwgpath = pdipHwg(pdip) ;
	nicstr = get_nicstr_from_devinfo(pdihPdi(pdih)->pdi_lb) ;
#endif

#ifndef SN_PDI
    Key = c->Key ;
#else
    Key = pdipDmaParm(pdip) ;
#endif
    if (brd_ptr = brd_from_key(Key))
        comp_ptr = find_first_component(brd_ptr, KLSTRUCT_SCSI);

    /* Run SCSI diags before configure devices */
    rc = diag_scsi_mem(diag_get_mode(), GET_WIDBASE_FROM_KEY(Key),
                  GET_PCIID_FROM_KEY(Key));

    /* 
     * This complex bitshifting is here to record failures for as many pci
     * ids as possible.  Currently NUM_SCSI_FAIL_CODES is 3 and diagval is
     * 16 bits.  The 16 bits are used as follows:
     * Bits 0-2 contain diag results for pci id 0
     * 	- bit 0 is result of mem test
     * 	- bit 1 is result of dma test
     * 	- bit 2 is result of self test
     * Bits 3-5 contain diag results for pci id 1 in the same fashion
     * Bits 6-8 contain diag results for pci id 2
     * Bits 9-11 contain diag results for pci id 3
     * Bits 12-14 contain diag results for pci id 4
     * Anything with a pci id > 4 is discarded.
     * This is fine for now because we have a max of 4 pci ids on any 
     * board (MSCSI board has 4).  If the size of the diagval field or the 
     * number of SCSI diagnostic tests changes, the recorded results will 
     * change accordingly.
     */

    if (comp_ptr && rc)
	if (GET_PCIID_FROM_KEY(Key) < (sizeof(comp_ptr->diagval) * NBBY /
				       NUM_SCSI_FAIL_CODES))
	    comp_ptr->diagval |= SCSI_MEM_FAIL_MASK <<
		(GET_PCIID_FROM_KEY(Key) * NUM_SCSI_FAIL_CODES);


    if (brd_ptr && (rc != KLDIAG_PASSED))
        printf("diag_scsi_mem: /hw/module/%d/slot/io%d/ql: FAILED\n",
            brd_ptr->brd_module, SLOTNUM_GETSLOT(brd_ptr->brd_slot));

    rc = diag_scsi_dma(diag_get_mode(), GET_WIDBASE_FROM_KEY(Key),
                  GET_PCIID_FROM_KEY(Key));

    if (comp_ptr && rc)
	if (GET_PCIID_FROM_KEY(Key) < (sizeof(comp_ptr->diagval) * NBBY /
				       NUM_SCSI_FAIL_CODES))
	    comp_ptr->diagval |= SCSI_DMA_FAIL_MASK <<
		(GET_PCIID_FROM_KEY(Key) * NUM_SCSI_FAIL_CODES);

    if (brd_ptr && (rc != KLDIAG_PASSED))
        printf("diag_scsi_dma: /hw/module/%d/slot/io%d/ql: FAILED\n",
            brd_ptr->brd_module, SLOTNUM_GETSLOT(brd_ptr->brd_slot));

    rc = diag_scsi_selftest(diag_get_mode(), GET_WIDBASE_FROM_KEY(Key),
                  GET_PCIID_FROM_KEY(Key));

    if (comp_ptr && rc)
	if (GET_PCIID_FROM_KEY(Key) < (sizeof(comp_ptr->diagval) * NBBY /
				       NUM_SCSI_FAIL_CODES))
	    comp_ptr->diagval |= SCSI_STEST_FAIL_MASK <<
		(GET_PCIID_FROM_KEY(Key) * NUM_SCSI_FAIL_CODES);

    if (brd_ptr && (rc != KLDIAG_PASSED))
        printf("diag_scsi_selftest: /hw/module/%d/slot/io%d/ql: FAILED\n",
            brd_ptr->brd_module, SLOTNUM_GETSLOT(brd_ptr->brd_slot));

#ifndef SN_PDI
    /* Get the qlogic base address from the c->Key field */
    ql_config_base = GET_PCICFGBASE_FROM_KEY(c->Key);
    ql_register_base = GET_PCIBASE_FROM_KEY(c->Key);
    /* store the TARGET_ID field required for DMA in ha_sn0_dma */
    ha_sn0_info[adap] = c->Key;
#else
	adap 			= pdipCtrlId(pdip)  ;
	ql_config_base 		= pdipCfgBase(pdip) ;
	ql_register_base 	= pdipMemBase(pdip) ;
    	ha_sn0_info[adap] 	= pdipDmaParm(pdip) ;
#endif /* SN_PDI */
    PRINTD(("QL will be initted with \
config = %x, registers = %x, hubwid = %d, hstnasid = %d\n",
	   ql_config_base, ql_register_base,
	   GET_HUBWID_FROM_KEY(ha_sn0_info[adap]),
	   GET_HSTNASID_FROM_KEY(ha_sn0_info[adap])));

    scsicnt = SCSI_MAXCTLR;
#endif

#if IP30
    ql_config_base = (__psunsigned_t)slot->cfg_base;
    ql_register_base = (__psunsigned_t)slot->mem_base;
    PRINTD(("slot %x\tql_config_base %x\tql_register_base %x\n",slot->bus_slot, ql_config_base, ql_register_base));

    if (ql_init(adap, (caddr_t)ql_config_base, (paddr_t)ql_register_base,
	        slot->bus_base))
	PRINTD(("Initialization at slot %x failed\n", 0x0));
#endif				/* IP30 */

#ifdef SN0

#if 0 /* Use this when u need to debug mscsi boards */
mscsi_debug(adap, ql_register_base) ;
#endif

    if (ql_init(adap, (caddr_t) ql_config_base, ql_register_base,0))
    {
	PRINTD(("SN0: QL init at slot %d failed\n", adap));
	return;
    }

#endif				/* SN0 */

/* now walk the bus and register the devices found */

    ha = ha_info[adap];

    if (ha == NULL)
    {
	cmn_err(CE_WARN, "qlconfigure: failed to configure adapter %x\n", adap);
	return;
    }

#ifdef SN0
    if (SN00) {
	    /*REFERENCED*/
	    u_short	gpio_val;
	    int		pci_slot;

    	    pISP_REGS  	    isp = (pISP_REGS) ha->ha_base;
	    gpio_val = PCI_INH(&isp->b4); 
	    if (gpio_val & 0x000c) {
		pci_slot = GET_PCIID_FROM_KEY(ha_sn0_info[adap]) ;
		if (pci_slot == 1) {
			ha->flags |= AFLG_FAST_NARROW;
			max_targets = 8;
			ql_set_defaults(ha);
		}
	    }
	    PCI_OUTH(&isp->b6,ENABLE_WRITE_SN00);
	    PCI_OUTH(&isp->b4, 0);
	    DELAY(100000);
    }
#endif

/* Lego doesn't use slotinfo_t so just go read the rev_id from config space */
    isp_config = (pPCI_REG) ha->ha_config_base;
    ha->revision = PCI_INB(&isp_config->Rev_Id);

    if (!qlreset(adap))
    {
	cmn_err(CE_WARN, "qlconfigure: Unable to reset bus %d\n", adap);
    } else
    {
	DELAY(1000);
    }



#if IP30 && VERBOSE
    printf("\nWalking SCSI Adapter %d\n", adap);
#endif

#ifdef SN0

#if 0
    if (nicstr)
    	printf("%s\n", nicstr) ;
#endif

    if ((adap == 0) || (adap == 1))
        printf("\nWalking SCSI Adapter %d (%s), (pci id %d)\n",
                adap, hwgpath, GET_PCIID_FROM_KEY(Key));
    else
        printf("\nWalking SCSI Adapter %s (pci id %d)\n",
                hwgpath, GET_PCIID_FROM_KEY(Key));

#endif

    if (ha->ql_defaults.Initiator_SCSI_Id != INITIATOR_SCSI_ID)
	printf("Non-default scsihostid is %d\n",
	       ha->ql_defaults.Initiator_SCSI_Id);

#ifdef IP30 /* only three targets (1,2,3) on internal controller */
	    /* for speedracer 					   */
    if( adap == 0) max_targets = 4;
#endif
    for (lu = targ = 0; targ < max_targets; targ++)
    {
	if (ha->ql_defaults.Initiator_SCSI_Id == targ)
	    continue;
#if SN0 || VERBOSE
	printf("%d", targ);
#endif
	if (!(inv = (char *) scsi_inq(adap, targ, lu)) || 
	     (inv[0] & 0x10) == 0x10)
	{
#if SN0 || VERBOSE
	    printf("- ");
#endif
	    continue;		/* LUN not supported, or not attached */
	} else
	    PRINT2(("found dksc(%d,%d)\n", adap, targ));
#if SN0 || VERBOSE
	printf("+ ");
	num_disks++;
#endif

	/* do not keep data for all scsi targets around as they are
	 * not likely to be used in standalone.  If the drive is used
	 * again we will reallocate the space.
	 */
	info = &tinfo[adap][targ][lu];
	if (info->si_inq != NULL) {
		dmabuf_free(info->si_inq);
		info->si_inq = NULL;
	}
	if (info->si_sense != NULL) {
		dmabuf_free(info->si_sense);
		info->si_sense = NULL;
	}

	/* ARCS Identifier is "Vendor Product" */
	strncpy(id, inv + 8, 8);
	id[8] = '\0';
	for (extra = id + strlen(id) - 1; *extra == ' '; *extra-- = '\0');
	strcat(id, " ");
	strncat(id, inv + 16, 16);
	id[25] = '\0';
	for (extra = id + strlen(id) - 1; *extra == ' '; *extra-- = '\0');
	scsictrltmpl.IdentifierLength = strlen(id);
	scsictrltmpl.Identifier = id;

	if ((inv[0] == 0) && 
	    ((!strncmp(id, "TOSHIBA CD", 10)) ||
	     (!strncmp(id, "SONY    CD", 10)) ||
	     (!strncmp(id, "LMS     CD", 10)))) {
		cdrom_revert(adap, targ, 0) ;
		if (!(inv = (char *) scsi_inq(adap, targ, lu)) || 
			     (inv[0] & 0x10) == 0x10) {
			PRINTD(("second inq on cd failed\n")) ;
		}
#if 0 /* Debug */
		for (i=0;i<25;i++)
			printf("0x%x ", inv[i]) ;
		printf("\n") ;
#endif

		/* THis is a copy of the same lines above.
		   Got to change this to a function call. */
        	/* ARCS Identifier is "Vendor Product" */
        	strncpy(id, inv + 8, 8);
        	id[8] = '\0';
        	for (extra = id + strlen(id) - 1; *extra == ' '; *extra-- = '\0');
        	strcat(id, " ");
        	strncat(id, inv + 16, 16);
        	id[25] = '\0';
        	for (extra = id + strlen(id) - 1; *extra == ' '; *extra-- = '\0');
        	scsictrltmpl.IdentifierLength = strlen(id);
        	scsictrltmpl.Identifier = id;

	}
	switch (inv[0])
	{
	case 0:		/* hard disk */
	    extra = "disk";

	    scsictrltmpl.Type = DiskController;
	    scsictrltmpl.Key = targ;
#ifndef SN0
	    tmp = AddChild(c, &scsictrltmpl, (void *) NULL);
	    PRINT3(("ADDED CHILD scsictrltmpl\n"));
	    if (tmp == (COMPONENT *) NULL)
		cpu_acpanic("disk ctrl");
#endif

	    if (inv[1] & 0x80)
	    {
		scsitmpl.Type = FloppyDiskPeripheral;
		scsitmpl.Flags = Removable | Input | Output;
		scsitmpl.ConfigurationDataSize =
		    sizeof(struct floppy_data);
		bzero(&cfgd.fd, sizeof(struct floppy_data));
		cfgd.fd.Version = SGI_ARCS_VERS;
		cfgd.fd.Revision = SGI_ARCS_REV;
		cfgd.fd.Type = FLOPPY_TYPE;
	    } else
	    {
		scsitmpl.Type = DiskPeripheral;
		scsitmpl.Flags = Input | Output;
		scsitmpl.ConfigurationDataSize =
		    sizeof(struct scsidisk_data);
		bzero(&cfgd.disk, sizeof(struct scsidisk_data));
		scsi_rc(adap, targ, lu, &cfgd.disk);
		cfgd.disk.Version = SGI_ARCS_VERS;
		cfgd.disk.Revision = SGI_ARCS_REV;
		cfgd.disk.Type = SCSIDISK_TYPE;
	    }
	    scsitmpl.Key = lu;

#ifndef SN0
	    tmp = AddChild(tmp, &scsitmpl, (void *) &cfgd);
	    PRINT3(("ADDED CHILD scsitmpl\n"));

	    if (tmp == (COMPONENT *) NULL)
		cpu_acpanic("scsi disk");
	    RegisterDriverStrategy(tmp, dksc_strat);
#endif
	    break;

	case 1:		/* sequential == tape */
	    type = tpsctapetype(inv, 0);
	    extra = inv;
	    sprintf(inv, " type %d", type);

	    scsictrltmpl.Type = TapeController;
	    scsictrltmpl.Key = targ;
#ifndef SN0
	    tmp = AddChild(c, &scsictrltmpl, (void *) NULL);
	    if (tmp == (COMPONENT *) NULL)
		cpu_acpanic("tape ctrl");
#endif

	    scsitmpl.Type = TapePeripheral;
	    scsitmpl.Flags = Removable | Input | Output;
	    scsitmpl.ConfigurationDataSize = 0;
	    scsitmpl.Key = lu;

#ifndef SN0
	    tmp = AddChild(tmp, &scsitmpl, (void *) NULL);
	    if (tmp == (COMPONENT *) NULL)
		cpu_acpanic("scsi tape");
	    RegisterDriverStrategy(tmp, tpsc_strat);
#endif
	    break;
	default:

	    type = inv[1] & INV_REMOVE;
	    /* ANSI level: SCSI 1, 2, etc. */
	    type |= inv[2] & INV_SCSI_MASK;

	    if ((inv[0] & 0x1f) == INV_CDROM)
	    {
		scsictrltmpl.Type = CDROMController;
		scsitmpl.Type = DiskPeripheral;
		scsitmpl.Flags = ReadOnly | Removable | Input;
	    } else
	    {
		scsictrltmpl.Type = OtherController;
		scsitmpl.Type = OtherPeripheral;
		scsitmpl.Flags = 0;
	    }

	    scsitmpl.ConfigurationDataSize = 0;
	    scsictrltmpl.Key = targ;

#ifndef SN0
	    tmp = AddChild(c, &scsictrltmpl, (void *) NULL);
	    if (tmp == (COMPONENT *) NULL)
		cpu_acpanic("other ctrl");
#endif

	    scsitmpl.Key = lu;

#ifndef SN0
	    tmp2 = AddChild(tmp, &scsitmpl, (void *) NULL);
	    if (tmp2 == (COMPONENT *) NULL)
		cpu_acpanic("scsi device");

	    if (tmp->Type == CDROMController)
		RegisterDriverStrategy(tmp2, dksc_strat);
#endif

	    extra = "";
	    break;
	}

#ifdef SN0			/* && SN0_USE_HWGRAPH */
	{
#ifdef SN_PDI
	COMPONENT 	*c ;
	c = &pdipDevC(pdip) ;
#endif /* SN_PDI */
	/* fill c->* with all relevant info, call kl_AddChild */
	c->IdentifierLength = scsictrltmpl.IdentifierLength;
	c->Identifier = scsictrltmpl.Identifier;
	if (scsictrltmpl.Type == CDROMController)
		c->Type = CDROMController ; /* XXX define CDROMperipheral ? */
	else
		c->Type = scsitmpl.Type;
	c->Flags = scsitmpl.Flags;
	c->Key = (adap << 8) | scsictrltmpl.Key;
	c->ConfigurationDataSize = scsitmpl.ConfigurationDataSize;
#ifdef SN_PDI
	pdipDevUnit(pdip) 	= scsictrltmpl.Key ;
	pdipDevLun(pdip)  	= 0 ;

	snAddChild(pdih, pdip) ; /* XXX check cfgd */
#else
	kl_qlogic_AddChild(c, &cfgd);
#endif
	}
#endif

	PRINT3(("LEAVING qlinstall\n"));
    }
#if SN0 || VERBOSE
    printf("= %d device(s)\n\n", num_disks);
#endif
}

/* 
 * This function is used to select between "short" and "long"
 * selection timeouts. QL has only a limited number of selection
 * timeouts to choose from. The next shorter value below 250 mS is 100
 * uS and some devices have problems with such a short value. Hence
 * this function is essentially a NOP and is provided only to avoid
 * link errors since scsi.c makes calls to it. 
 */
/* ARGSUSED */
void 
scsi_seltimeout(u_char adap, int makeshort)
{
  return;
}

static int 
scsi_rc(u_char adap, u_char targ, u_char lun, struct scsidisk_data * d)
{
    unsigned int   *dc;
    u_char          read_capacity[10];


    PRINT3(("scsi_rc: read capacity for target %x on adapter %x\n", targ, adap));
    bzero(read_capacity, 10);
    read_capacity[0] = 0x25;
    dc = dmabuf_malloc(sizeof(unsigned int) * 2);

    scsi_ecmd(adap, targ, lun, read_capacity, 10, dc, 8);

#ifdef _MIPSEL
    swapl(dc, 2);
#endif
    d->MaxBlocks = dc[0];
    d->BlockSize = dc[1];

    dmabuf_free(dc);
    return (0);
}


/* scsi early command */
static          caddr_t
scsi_ecmd(u_char adap, u_char targ, u_char lun,
	  u_char * inquiry, int il,
	  void *data, int dl)
{
    register scsisubchan_t *sp;
    int             first = 2;
    buf_t          *bp;
    caddr_t         ret;

    PRINT3(("scsi_ecmd: adap %x, targ %x, lun %x, inquiry* %x, il %x, data %x, dl %x\n", adap, targ, lun, inquiry, il, data, dl));

    if ((sp = allocsubchannel(adap, targ, lun)) == NULL ||
	(bp = (buf_t *) malloc(sizeof(*bp))) == NULL)
    {
	PRINTD(("bailing on scsi_ecmd because allocsubchannel was null, sp %x - bp %x\n", sp, bp));
	return NULL;
    }
    PRINT1(("scsi_ecmd: subchannel block is at %x\n", sp));

    sp->s_notify = NULL;
    sp->s_cmd = inquiry;
    sp->s_len = il;
    sp->s_buf = bp;
    /* SCSI 2 spec strongly recommends all devices be able to respond
     * successfully to inq within 2 secs of power on. some ST4 1200N (1.2)
     * drives occasionally accept the cmd and disc, but don't come back
     * within 1 second, even when motor start jumper is on, at powerup.
     * Only seen on IP12, probably because it gets to the diag faster.
     */
    sp->s_timeoutval = 2 * HZ;
again:
    /* So we don't keep data left from an earlier call.  Also insures null
     * termination. */
    bzero(data, dl);

    bp->b_dmaaddr = (caddr_t) data;
    bp->b_bcount = dl;
    bp->b_flags = B_READ | B_BUSY;
    PRINTD(("calling doscsi\n"));
    doscsi(sp);
    PRINTD(("back from doscsi\n"));
    if (sp->s_error == SC_GOOD && sp->s_status == ST_GOOD)
    {
	PRINT3(("scsi_ecmd: returning good status\n"));
	ret = (caddr_t) data;
    } else if (first-- && sp->s_error != SC_TIMEOUT)
    {
	/* CMDTIME can also indicate a bus reset; could be bus problems,                 *
	 * or sync vs async wd problem; at least it responded to selection,
	 * so retry it. */
	if (sp->s_error == SC_HARDERR || sp->s_error == SC_CMDTIME || sp->s_error == SC_BUS_RESET)
	{
	    PRINTD(("RETRY\n"));
	    goto again;		/* one retry, if it not a timeout. */

	}
    } else
    {
	ret = NULL;
    }
    sc_inq_stat = sp->s_error;	/* mainly for standalone ide */
    freesubchannel(sp);
    free((caddr_t) bp);

    return (ret);
}


/*
 *      Map `len' bytes from starting address `addr' for a SCSI DMA op.
 *      Returns the actual number of bytes mapped.
 */
int 
make_sg(scatter * scatter, u_char * addr, int len, pHA_INFO ha)
{

    unsigned        incr, bytes, dotrans, npages, partial, index;
    pde_t          *pde;
    PRINT3(("make_sg: *scatter %x addr %x len %x\n", scatter, addr, len));
    /* can only deal with word aligned addresses */
    if ((ulong) addr & 0x3)
    {
	PRINTD(("make_sg: not word alligned\n"));
	return (0);
    }
    if (bytes = (int) poff(addr))
    {
	if ((bytes = (NBPC - bytes)) > len)
	{
	    bytes = len;
	    npages = 1;
	    PRINT3(("make_sg: bytes = poff(addr)\n"));
	} else
	    npages = (int) btoc(len - bytes) + 1;
    } else
    {
	bytes = 0;
	npages = (int) btoc(len);
	PRINT3(("make_sg: not bytes = poff(addr)\n"));
    }
    if (bytes == len)
    {
	partial = bytes;	/* only one descr, and that is part of a page */
	PRINT3(("partial in one page\n"));
    } else
    {
	partial = (len - bytes) % NBPC;	/* partial page in last descr? */
	PRINT3(("partial in the last page \n"));
    }

    /* We either have KSEG2 or KSEG0/1/Phys, figure out now */
    {
	/* We either have KSEG0/1 or Physical */
	if (IS_KSEGDM(addr))
	{
	    addr = (u_char *) KDM_TO_PHYS(addr);
	}
	dotrans = 0;
    }
    /* put the number of pages needed into the structure */
    scatter->num_pages = npages;
    PRINT3(("number of pages %x\n", npages));
    /* Generate SCSI dma descriptors for this request */
    index = 0;
    if (bytes)
    {

	scatter->dseg[index].base =
#if IP30
	    MAKE_DIRECT_MAPPED_2GIG(kvtophys(addr));
#elif SN0
#ifdef SN0_PCI_64
	get_pci64_dma_addr((__psunsigned_t) addr,
			   ha_sn0_info[ha->controller_number]);
#else
	MAKE_DIRECT_MAPPED_2GIG((int) kvtophys(addr));
#endif				/* SN0_PCI_64 */
#else
	    KDM_TO_PHYS(addr);
#endif


	scatter->dseg[index].count = bytes;
	PRINT3(("index %x  base %x  count %x\n", index, scatter->dseg[index].base, scatter->dseg[index].count));
	addr += bytes;
	npages--;
	++index;
    }
    while (npages--)
    {
	scatter->dseg[index].base = dotrans ? (int) pdetophys(pde++) :
#if IP30
	    MAKE_DIRECT_MAPPED_2GIG(kvtophys(addr));
#elif SN0
#ifdef SN0_PCI_64
	get_pci64_dma_addr((__psunsigned_t) addr,
			   ha_sn0_info[ha->controller_number]);
#else
	MAKE_DIRECT_MAPPED_2GIG((int) kvtophys(addr));
#endif				/* SN0_PCI_64 */
#else
	    KDM_TO_PHYS(addr);
#endif

	incr = (!npages && partial) ? partial : NBPC;
	addr += incr;
	scatter->dseg[index].count = incr;
	PRINT3(("index %x  base %x  count %x\n", index, scatter->dseg[index].base, scatter->dseg[index].count));

	++index;
    }

    return len;
}

void 
munge(u_char * pointer, int count)
{
    /* munge 4 bytes at a time */
    int             i;
    u_char         *cpointer, *copyptr;
    int             temp;
    if (count % 4)
    {
	PRINT2(("munge: dont do non word aligned stuff\n"));
	PRINT2(("munge: but I'll do it anyway %x\n", pointer));
    }
    cpointer = copyptr = pointer;
    PRINT4(("munge: %x bytes from address %x : ", count, cpointer));
    for (i = 0; i < 16; i++)
	PRINT4(("%x ", *cpointer++));
    PRINT4(("\n"));
    cpointer = pointer;
    copyptr = pointer;
    for (i = 0; i < count / 4; i++)
    {
	temp = *((int *) copyptr);
	cpointer = (u_char *) & temp;

	*(copyptr + 3) = *(cpointer + 0);
	*(copyptr + 2) = *(cpointer + 1);
	*(copyptr + 1) = *(cpointer + 2);
	*(copyptr + 0) = *(cpointer + 3);
	copyptr = copyptr + 4;
    }
    cpointer = pointer;
    PRINT4(("munge: %x bytes from address %x : ", count, cpointer));
    for (i = 0; i < 16; i++)
	PRINT4(("%x ", *cpointer++));
    PRINT4(("\n"));
}



/*
** Function:    ql_set_defaults
**
** Description: Sets the default parameters.
*/
static void 
ql_set_defaults(pHA_INFO ha)
{
    Default_parameters *n_ptr = &ha->ql_defaults;

    int             index;
    /* * Use the defaults! */
    n_ptr->Fifo_Threshold = BURST_128;
    n_ptr->HA_Enable = HA_ENABLE;
    n_ptr->Initiator_SCSI_Id = INITIATOR_SCSI_ID;
#ifndef EVEREST
    { char *cp;
      
#ifdef SN0
	if ((cp = getenv("scsihostid")) != NULL) {
		int	scsi_id = INITIATOR_SCSI_ID ;
		if (isxdigit(*cp)) {
			if (isdigit(*cp))
				scsi_id = *cp - '0' ;
			else
				scsi_id = 10 + tolower(*cp) - 'a' ;
		}
		n_ptr->Initiator_SCSI_Id = scsi_id ;
	}
#else
      if ((cp = getenv("scsihostid")) != NULL)
	n_ptr->Initiator_SCSI_Id = atoi(cp);
#endif
      if (n_ptr->Initiator_SCSI_Id > 7) 
	n_ptr->Initiator_SCSI_Id = INITIATOR_SCSI_ID;
    }
#endif
    n_ptr->Bus_Reset_Delay = BUS_RESET_DELAY;
#ifdef SN0
    n_ptr->Retry_Count = 0; 
    /* this was tried in the kernel and walking the scsi bus is pretty fast. */
#else
    n_ptr->Retry_Count = RETRY_COUNT;
#endif
    n_ptr->Retry_Delay = RETRY_DELAY;

    n_ptr->Capability.bits.ASync_Data_Setup_Time =
	ASYNC_DATA_SETUP_TIME;
    n_ptr->Capability.bits.REQ_ACK_Active_Negation =
	REQ_ACK_ACTIVE_NEGATION;
    n_ptr->Capability.bits.DATA_Line_Active_Negation =
	DATA_ACTIVE_NEGATION;
    n_ptr->Capability.bits.Data_DMA_Burst_Enable =
	DATA_DMA_BURST_ENABLE;
    n_ptr->Capability.bits.Command_DMA_Burst_Enable =
	CMD_DMA_BURST_ENABLE;

    n_ptr->Tag_Age_Limit = TAG_AGE_LIMIT;
    n_ptr->Selection_Timeout = SELECTION_TIMEOUT;
    n_ptr->Max_Queue_Depth = MAX_QUEUE_DEPTH;
    n_ptr->Delay_after_reset = DELAY_AFTER_RESET;

    for (index = 0; index < QL_MAXTARG; index++)
    {
	n_ptr->Id[index].Capability.bits.Renegotiate_on_Error =
	    RENEGOTIATE_ON_ERROR;
	n_ptr->Id[index].Capability.bits.Stop_Queue_on_Check =
	    STOP_QUEUE_ON_CHECK;
	n_ptr->Id[index].Capability.bits.Auto_Request_Sense =
	    AUTO_REQUEST_SENSE;
#ifdef  TAGGED_CMDS
	n_ptr->Id[index].Capability.bits.Tagged_Queuing =
	    TAGGED_QUEUING;
#endif
	n_ptr->Id[index].Capability.bits.Sync_Data_Transfers =
	    SYNC_DATA_TRANSFERS;
	n_ptr->Id[index].Capability.bits.Parity_Checking =
	    PARITY_CHECKING;
	n_ptr->Id[index].Capability.bits.Disconnect_Allowed =
	    DISCONNECT_ALLOWED;
        if (ha->flags & AFLG_FAST_NARROW) {
            n_ptr->Id[index].Sync_Period   = SYNC_PERIOD;
        } else {
            n_ptr->Id[index].Capability.bits.Wide_Data_Transfers  =
            WIDE_DATA_TRANSFERS;
            n_ptr->Id[index].Sync_Period   = SYNC_PERIOD_FAST20;
        }

	n_ptr->Id[index].Throttle = EXECUTION_THROTTLE;





	n_ptr->Id[index].Sync_Offset = SYNC_OFFSET;

#if 0
	/* * If sync or wide transfers, make sure to renegotiate on errors. */
	if (n_ptr->Id[index].Sync_Data_Transfers ||
	    n_ptr->Id[index].Wide_Data_Transfers)
	{
	    /* * Override the default. */
	    n_ptr->Id[index].Capability.bits.Renegotiate_on_Error = 1;
	}
#endif

    }
    PRINTD(("FAST/FAST20  period set to %d offset %x\n", n_ptr->Id[index - 1].Sync_Period, n_ptr->Id[index - 1].Sync_Offset));

}


/* this is the command entry point for the standalone driver */

/* make a scsi_request structure from a scsisubchan sturcutre     */
/* this is in hopes of using the same code for the kernel driver  */
/* and the standalone driver					  */

void 
doscsi(scsisubchan_t * scsisubchan)
{
    static scsi_request_t s_request;
    scsi_request_t *scsi_request;
    scsi_request = &s_request;

    PRINT3(("doscsi: scsisubchan is at %x, scsi_request is at %x\n", scsisubchan, scsi_request));

    bzero(scsi_request, sizeof(scsi_request_t));

    scsi_request->sr_ctlr = scsisubchan->s_adap;;
    scsi_request->sr_target = scsisubchan->s_target;
    scsi_request->sr_lun = scsisubchan->s_lu;
    scsi_request->sr_command = scsisubchan->s_cmd;
    scsi_request->sr_cmdlen = scsisubchan->s_len;

/* flags are weird.  in standalone they are in the buf stucture */
/* need to preserve the B_BUSY flag and pass on the B_READ      */
/* which becomes SRF_DIR_IN					*/
/* remember to not kill the B_BUSY on the way back              */

    scsi_request->sr_flags = (scsisubchan->s_buf->b_flags) & SRF_DIR_IN;

    scsi_request->sr_timeout = scsisubchan->s_timeoutval;
    scsi_request->sr_buffer = (u_char *) scsisubchan->s_buf->b_dmaaddr;
    scsi_request->sr_buflen = scsisubchan->s_buf->b_bcount;
    /* too many places don't flush, so in standalone, do the flush here.  */
    if (scsisubchan->s_buf->b_bcount) {
	if (scsisubchan->s_buf->b_flags & B_READ)
	    DCACHE_INVAL(scsisubchan->s_buf->b_dmaaddr,
		scsisubchan->s_buf->b_bcount);
	else
	    MRW_DCACHE_WB_INVAL(scsisubchan->s_buf->b_dmaaddr,
		scsisubchan->s_buf->b_bcount);
    }
    ql_entry(scsi_request);
    scsisubchan->s_error = scsi_request->sr_status;
    scsisubchan->s_status = scsi_request->sr_scsi_status;
    PRINT3(("s_error is %x and s_status is %x\n", scsisubchan->s_error, scsisubchan->s_status));
    if (scsisubchan->s_notify)
	(*scsisubchan->s_notify) (scsisubchan);
}

scsisubchan_t  *subchan[SCSI_MAXCTLR][QL_MAXTARG];

scsisubchan_t  *
allocsubchannel(int adap, int target, int lu)
{
    scsisubchan_t  *sp;
    pHA_INFO        ha = ha_info[adap];

    PRINT3(("inside allocsubchannel, adap %x target %x lun %x\n", adap, target, lu));
/* abort allocating subchannel if no ha configured for this adaptor */
    if (!ha)
    {
	PRINTD(("cannot allocate subchannel for target %x, no adaptor %x\n", target, adap));
	return NULL;
    }
    if (adap >= SCSI_MAXCTLR || target >= QL_MAXTARG || lu != 0)
    {
	PRINTD(("failed to allocate subchannel\nadap >= SCSI_MAXCTLR || target >= QL_MAXTARG || lu != 0\n"));
	return NULL;
    }
    if (subchan[adap][target] != NULL)
    {
	PRINTD(("failed to allocate subchannel\nsubchan[adap][target] != NULL\n"));
	return NULL;
    }
    if (qlalloc(adap, target, lu, 1, NULL) == 0)
    {
	PRINTD(("failed to allocate subchannel\nqlalloc(adap, target, lu, 1, NULL) == 0\n"));
	return NULL;
    }
    subchan[adap][target] = malloc(sizeof(scsisubchan_t));
    PRINT3(("allocated subachan address is %x\n", &subchan[adap][target]));
    sp = subchan[adap][target];
    sp->s_adap = adap;
    sp->s_target = target;
    sp->s_lu = lu;
    PRINT3(("subchannel succesfully allocated\n"));

    return sp;
}

/* this routine is used to do the hardware inventory, etc.  It
 * returns NULL on errors, else a ptr to 37 bytes of null
 * terminated inquiry data, which is enough for type info, and mfg
 * name, product name, and rev.  Only valid to call at init time,
 * when no driver open routines have yet been called.   It isn't
 * static, because ide uses it also.
*/

#define SCSI_IDATA_SIZE 37

u_char         *
scsi_inq(u_char adap, u_char targ, u_char lun)
{
    static u_char   idata[SCSI_IDATA_SIZE];
    static u_char   inquiry[6];	/* can't initialize for PROM */
    u_char         *buf;
    PRINT3(("inside scsi_inq\n"));
    buf = dmabuf_malloc(SCSI_IDATA_SIZE);
    bzero(buf, SCSI_IDATA_SIZE);
    inquiry[0] = 0x12;
    inquiry[1] = lun << 5;
    inquiry[4] = SCSI_IDATA_SIZE - 1;

    if (scsi_ecmd(adap, targ, lun, inquiry, 6, buf, SCSI_IDATA_SIZE - 1) != 0)
    {
	bcopy(buf, idata, SCSI_IDATA_SIZE);
	dmabuf_free(buf);
	return idata;
    }

    dmabuf_free(buf);
    return 0;
}


void 
scsi_setsyncmode(scsisubchan_t * sp, int enable)
{
    PRINT3(("scsi_setsyncmode called: does nothing - subchannel %x enable %x\n", sp, enable));
}



void 
freesubchannel(register scsisubchan_t * sp)
{

    PRINT3(("FREEing Subchannel adap %x, targ %x, lun %x\n", sp->s_adap, sp->s_target, sp->s_lu));
    subchan[sp->s_adap][sp->s_target] = NULL;
    free(sp);

}

/*
 * NOTE:
 * The defines SC_NUMSENSE and SC_NUMADDSENSE are in scsi.h but they
 * are overridden by new values in newscsi.h. These arrays are
 * defined in controller specific files like ql.c, wd95.c which
 * use values in newscsi.h. Including newscsi.h in dksc.c may cause other
 * problems. So only these controller file dependent information
 * is changed. If any of these change in newscsi.h it should
 * be reflected in include/sys/scsimsgs.h
 */

char           *scsi_key_msgtab[SC_NUMSENSE] = {
    "No sense",			/* 0x0 */
    "Recovered Error",		/* 0x1 */
    "Drive not ready",		/* 0x2 */
    "Media error",		/* 0x3 */
    "Unrecoverable device error",	/* 0x4 */
    "Illegal request",		/* 0x5 */
    "Unit Attention",		/* 0x6 */
    "Data protect error",	/* 0x7 */
    "Unexpected blank media",	/* 0x8 */
    "Vendor Unique error",	/* 0x9 */
    "Copy Aborted",		/* 0xa */
    "Aborted command",		/* 0xb */
    "Search data successful",	/* 0xc */
    "Volume overflow",		/* 0xd */
    "Reserved (0xE)",		/* 0xe */
    "Reserved (0xF)",		/* 0xf */
};
char           *scsi_addit_msgtab[SC_NUMADDSENSE] = {
    NULL,			/* 0x00 */
    "No index/sector signal",	/* 0x01 */
    "No seek complete",		/* 0x02 */
    "Write fault",		/* 0x03 */
    "Driver not ready",		/* 0x04 */
    "Drive not selected",	/* 0x05 */
    "No track 0",		/* 0x06 */
    "Multiple drives selected",	/* 0x07 */
    "LUN communication error",	/* 0x08 */
    "Track error",		/* 0x09 */
    "Error log overflow",	/* 0x0a */
    NULL,			/* 0x0b */
    "Write error",		/* 0x0c */
    NULL,			/* 0x0d */
    NULL,			/* 0x0e */
    NULL,			/* 0x0f */
    "ID CRC or ECC error",	/* 0x10  */
    "Unrecovered data block read error",	/* 0x11 */
    "No addr mark found in ID field",	/* 0x12 */
    "No addr mark found in Data field",	/* 0x13 */
    "No record found",		/* 0x14 */
    "Seek position error",	/* 0x15 */
    "Data sync mark error",	/* 0x16 */
    "Read data recovered with retries",	/* 0x17 */
    "Read data recovered with ECC",	/* 0x18 */
    "Defect list error",	/* 0x19 */
    "Parameter overrun",	/* 0x1a */
    "Synchronous transfer error",	/* 0x1b */
    "Defect list not found",	/* 0x1c */
    "Compare error",		/* 0x1d */
    "Recovered ID with ECC",	/* 0x1e */
    NULL,			/* 0x1f */
    "Invalid command code",	/* 0x20 */
    "Illegal logical block address",	/* 0x21 */
    "Illegal function",		/* 0x22 */
    NULL,			/* 0x23 */
    "Illegal field in CDB",	/* 0x24 */
    "Invalid LUN",		/* 0x25 */
    "Invalid field in parameter list",	/* 0x26 */
    "Media write protected",	/* 0x27 */
    "Media change",		/* 0x28 */
    "Device reset",		/* 0x29 */
    "Log parameters changed",	/* 0x2a */
    "Copy requires disconnect",	/* 0x2b */
    "Command sequence error",	/* 0x2c */
    "Update in place error",	/* 0x2d */
    NULL,			/* 0x2e */
    "Tagged commands cleared",	/* 0x2f */
    "Incompatible media",	/* 0x30 */
    "Media format corrupted",	/* 0x31 */
    "No defect spare location available",	/* 0x32 */
    "Media length error",	/* 0x33 (tape only) */
    NULL,			/* 0x34 */
    NULL,			/* 0x35 */
    "Toner/ink error",		/* 0x36 */
    "Parameter rounded",	/* 0x37 */
    NULL,			/* 0x38 */
    "Saved parameters not supported",	/* 0x39 */
    "Medium not present",	/* 0x3a */
    "Forms error",		/* 0x3b */
    NULL,			/* 0x3c */
    "Invalid ID msg",		/* 0x3d */
    "Self config in progress",	/* 0x3e */
    "Device config has changed",/* 0x3f */
    "RAM failure",		/* 0x40 */
    "Data path diagnostic failure",	/* 0x41 */
    "Power on diagnostic failure",	/* 0x42 */
    "Message reject error",	/* 0x43 */
    "Internal controller error",/* 0x44 */
    "Select/Reselect failed",	/* 0x45 */
    "Soft reset failure",	/* 0x46 */
    "SCSI interface parity error",	/* 0x47 */
    "Initiator detected error",	/* 0x48 */
    "Inappropriate/Illegal message",	/* 0x49 */
};

#ifdef QL_DEBUG
void 
dump_area(char *ptr, int count)
{
    int             i;
    for (i = 0; i < count; i++)
    {
	if (!(i % 0x20))
	    PRINTD(("\n%x: ", ptr));
	PRINTD(("%x ", *ptr++));
    }
    PRINTD(("\n"));
}
#endif


static int
reqi_get_slot(pHA_INFO ha,int tgt)
{
	int		i = ha->reqi_cntr[tgt];
	pREQ_INFO	r_ptr = &ha->reqi_block[tgt][i];
	int		slot;
	int		found = 0;
	
	do {
		i++;
		if (i > MAX_REQ_INFO ) {
			i = 1;
		} 
		r_ptr = &ha->reqi_block[tgt][i];
		if (r_ptr->req == NULL) {
			found = 1;
			break;
		}
			
	} while (i != ha->reqi_cntr[tgt]);
	if (found) {
		ha->reqi_cntr[tgt] = i;
		slot = (tgt * (MAX_REQ_INFO +1)) | i;
	} else {
		slot = found;
	}
	return slot;
}

#ifdef IP30
qldebug_cmd(int argc, char **argv)
{
#ifdef QL_DEBUG
	int new;

	if (argc == 2) {
		new = atoi(argv[1]);

		if (new >= 0 && new <= 5)
			ql_debug = new;
		else
			printf("invalid level %d\n",new);
	}

	printf("current ql_debug level %d\n",ql_debug);
#else
	printf("QL_DEBUG is not enabled.\n");
#endif
	return 0;
}
#endif

/* execute a user command given code, cdblen, and datalen */
struct scsi_target_info *
ql_u_cmd(u_char extctlr, u_char targ, u_char lun,
	 u_char cmd,u_char clen, u_char dlen)
{
    struct scsi_request *req;
    int             retry = 1;
    static u_char   scsi[10];
    char            name[9];
    struct scsi_target_info *retval;
    scsi_target_info_t *info;
    u_char          board;

    board = extctlr;

    PRINT3(("ql_u_cmd: extctlr %x, targ %x, lun %x\n", extctlr, targ, lun));

    info = &tinfo[board][targ][lun];
    if ((info->si_inq == NULL) && (dlen))
    {
	info->si_inq = (u_char *) dmabuf_malloc(dlen);
	info->si_sense = (u_char *) dmabuf_malloc(SCSI_SENSE_LEN);
	PRINT3(("\ninquiry and sense allocated for board %x, targ %x lun %x inquire %x sense %x\n", board, targ, lun, info->si_inq, info->si_sense));
        bzero(info->si_inq, dlen);
    }

    req = malloc(sizeof(*req));

retryit:

    bzero(req, sizeof(*req));

    scsi[0] = cmd;
    scsi[1] = scsi[2] = scsi[3] = scsi[5] = 0;
    scsi[4] = dlen;
    req->sr_ctlr = extctlr;
    req->sr_target = targ;
    req->sr_lun = lun;
    req->sr_command = scsi;
    req->sr_cmdlen = clen;		/* command length */
    req->sr_flags = SRF_DIR_IN | SRF_FLUSH | SRF_AEN_ACK;
    req->sr_timeout = 10 * HZ;
    req->sr_buffer = info->si_inq;
    req->sr_buflen = dlen;
    req->sr_sense = NULL;
    req->sr_dev = 0;

    DCACHE_INVAL(info->si_inq, clen);

    qlcommand(req);

    if (req->sr_status == SC_GOOD && req->sr_scsi_status == ST_GOOD)
    {

	PRINT3(("qlinfo: good status on inquiry sr_status %x sr_scsi_status %x\n", req->sr_status, req->sr_scsi_status));
	retval = info;
	PRINTD(("qlinfo: REGISTER device inquire data %x controller %x target %x\n", info->si_inq, extctlr, targ));

    } else
    {

	PRINT3(("ql_u_cmd failed:  sr_status %x, sr_scsi_status %x\n",
	       req->sr_status, req->sr_scsi_status));
	if (retry-- && req->sr_status == SC_BUS_RESET)
	{
	    PRINT2(("ql_u_cmd: retrying because of bus reset\n"));
	    goto retryit;
	}
	retval = NULL;

    }

    free(req);
    return retval;
}

void
cdrom_revert(u_char adap, u_char unit, u_char lun)
{
	/* request sense, code, cdblen, datalen */
	ql_u_cmd(adap, unit, lun, 3, 6, SCSI_SENSE_LEN) ;
	/* TOSHIBA CD vendor specific command */
	ql_u_cmd(adap, unit, lun, 0xc9, 10, 0) ;
}


