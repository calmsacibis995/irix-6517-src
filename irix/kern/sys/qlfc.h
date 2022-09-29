#ifdef _KERNEL
#include <sys/ktypes.h>

/**********************************************************
**
**          Include files
*/
#include "sys/types.h"
#include <sys/debug.h>
#include "sys/param.h"
#include "sys/ddi.h"

#include "sys/sema.h"  
#include "sys/iograph.h"  

#include "sys/scsi.h"
#include "sys/edt.h"
#include "sys/cpu.h"
#include "sys/atomic_ops.h"
#include "sys/PCI/bridge.h"
#include "sys/PCI/PCI_defs.h"
#include "sys/PCI/pciio.h"
#include "sys/time.h"
#include "sys/scsi_stats.h"

#include <sys/conf.h>
#include <sys/var.h>

#include "sys/buf.h"
#include "sys/pfdat.h"
#include "sys/dkio.h"
#include <sys/invent.h>
#include <sys/mtio.h>
#include "sys/tpsc.h"
#include "sys/cmn_err.h"
#include "sys/sysmacros.h"
#include "sys/kmem.h"
#include "sys/failover.h"
#include "sys/driver.h"
#include "sys/immu.h"
#include "ksys/hwg.h"
#include "ksys/sthread.h"

#ifdef DEBUG
#include <sys/debug.h>
#endif


#include "sys/systm.h"
#include "sys/dir.h"
#include "ksys/vfile.h"
#include "sys/xlate.h"
#include "sys/immu.h"
#include "sys/errno.h"


#if defined(IP30) || defined(SN)
#include "sys/PCI/pcibr.h"
#endif

#include <sys/alenlist.h>
#include <sys/pci_intf.h>

/* IP30/SN0 gets the fcal.h info from the fcadp.a file and declares it extern.  
	O2 needs the stuff from fcal.h but doesnt have fcadp.a so including takes
	care of it.  This should be resolved for all platforms independent of the
	fcadp driver.
*/
#if defined(IP32) && !defined(QLFC_IDBG)
#include "sys/fcal.h"
#else
#define INVALID_ALPA    ((u_char) -1)
#define TID_2_ALPA(tid) (((tid) > 0x7E) ? INVALID_ALPA : tid_to_alpa[tid])
#define ALPA_2_TID(alpa) (((alpa) > 255) ? 0xFF : alpa_to_tid[alpa])

extern uint8_t tid_to_alpa[];
extern uint8_t alpa_to_tid[];
#endif /* IP32/SN/IP30 */

typedef int	STATUS;
#define OK	(0)
#define ERROR	(-1)
#define	REJECT	(-2)	/* used by send_sns only */

/**********************************************************
**
**      Debug definitions
*/

#define PIO_TRACE
#undef PIO_TRACE

#define TRACING_ENABLED
#ifdef  TRACING_ENABLED
#define TRACE(c,s,a1,a2,a3,a4,a5,a6)	trace(c,s,a1,a2,a3,a4,a5,a6)
#else
#define TRACE(c,s,a1,a2,a3,a4,a5,a6)
#endif
#define CTRACE0(s)				ctrace(ctlr,s,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0)
#define CTRACE1(s,a1)				ctrace(ctlr,s,(__psint_t)a1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0)
#define CTRACE2(s,a1,a2)			ctrace(ctlr,s,(__psint_t)a1,(__psint_t)a2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0)
#define CTRACE8(s,a1,a2,a3,a4,a5,a6,a7,a8)	ctrace(ctlr,s,(__psint_t)a1,(__psint_t)a2,(__psint_t)a3,(__psint_t)a4,\
							      (__psint_t)a5,(__psint_t)a6,(__psint_t)a7,(__psint_t)a8,\
							      0,0,0,0,0,0,0,0,0)
#define CTRACE9(s,a1,a2,a3,a4,a5,a6,a7,a8,a9)	ctrace(ctlr,s,(__psint_t)a1,(__psint_t)a2,(__psint_t)a3,(__psint_t)a4,\
							      (__psint_t)a5,(__psint_t)a6,(__psint_t)a7,(__psint_t)a8,\
							      (__psint_t)a9,0,0,0,0,0,0,0,0)

#define TRACE_ARGS	6	/* change this and trace.c and TRACE macro need fixs */
#define CTRACE_ARGS	9	/* change this and trace.c and CTRACE macro need fixs */

/**********************************************************
**
**      Macro Definitions
*/

#ifdef IP32
#define QLFC_INTR(x)		qlfc_intr((eframe_t *)NULL, x)
#else
#define QLFC_INTR(x)		qlfc_intr(x)
#endif /* IP32 */

#ifdef _STANDALONE
#define CRITICAL_BEGIN      {int  s = splhi();
#define CRITICAL_EXIT       splx(s)
#define CRITICAL_END        splx(s);}
#endif

#define ASSIGN_INT8(ptr, val) (*((uint8_t *)(ptr)) = (val))
#define ASSIGN_INT16(ptr, val) assign_int16((uint16_t *) (ptr), (val))
#define ASSIGN_INT32(ptr, val) assign_int32((uint32_t *) (ptr), (val))
#define ASSIGN_INT64(ptr, val) assign_int64((uint64_t *) (ptr), (val))

/****************************************************************/
/*								*/
/*          Compile time options                                */
/*								*/
/****************************************************************/

#define QLFC_ABORT_TASK_SET_WAR		0	/* work around abort task set not being lun specific */
#define QLFC_LOGIN_TIMEOUT_WAR		1	/* work around login mbox command response getting "lost" */

#if defined(SN) || defined(IP30)
#define BRIDGE_B_PREFETCH_DATACORR_WAR 1
#define A64_BIT_OPERATION			/* operate ISP in 64 bit mode */
#ifndef MAPPED_SIZE
#define	MAPPED_SIZE	0x4000
#endif
#else
#define	MAPPED_SIZE	IO_NBPC
#endif /* SN || IP30 */

#define LOAD_FIRMWARE				/* define to load new firmware */
#define MAILBOX_TEST				/* test mailboxes (firmware running?) */

#ifdef SN
#define  QL_SRAM_PARITY				/* enable sram parity checking */
#define USE_MAPPED_CONTROL_STREAM
#undef  USE_IOSPACE				/* Use MEM space for mapping QL registers */
#undef	FLUSH
#endif	/* SN */

#ifdef IP30
#define  QL_SRAM_PARITY				/* enable sram parity checking */
#define USE_MAPPED_CONTROL_STREAM
#define USE_IOSPACE				/* Use IO space for mapping QL registers */

#if HEART_COHERENCY_WAR
#define  FLUSH
#endif
#endif  /* IP30 */

#ifdef IP32
#define  QL_SRAM_PARITY				/* enable sram parity checking */
#define USE_IOSPACE				/* Use IO space for mapping QL registers */
#define USE_PCI_PIO
/* board is running in 64 bit mode, w 64 bit iocb's, but only stuffing 32 bits
   of address */
#endif  /* IP32 */

#ifdef USE_IOSPACE
#define WINDOW 0
#define ADDRESS_SPACE_TO_ENABLE PCI_CMD_IO_SPACE
#else
#define WINDOW 1
#define ADDRESS_SPACE_TO_ENABLE PCI_CMD_MEM_SPACE
#endif

#ifdef FLUSH
#include "ksys/cacheops.h"
#if HEART_COHERENCY_WAR
/* flush both memory reads and writes */
#define MR_DCACHE_WB_INVAL(a,l)		heart_dcache_wb_inval(a,l)
#define MRW_DCACHE_WB_INVAL(a,l)	heart_write_dcache_wb_inval(a,l)
#define DCACHE_INVAL(a,l)		heart_dcache_inval(a,l)
#else
#define MR_DCACHE_WB_INVAL(a,l)		__dcache_wb_inval(a,l)
#define MRW_DCACHE_WB_INVAL(a,l)	__dcache_wb_inval(a,l)
#define DCACHE_INVAL(a,l)		__dcache_inval(a,l)
#endif
#endif /* FLUSH */

/****************************************************************/
/*								*/
/* DEBUG Compile time options                                   */
/* Add the following to DBOPTS in klocaldefs                    */
/*    -DPIO_TRACE     - enable QL PIO tracing                   */
/*    -DSENSE_PRINT   - print sense following request sense     */
/*								*/
/****************************************************************/

/**********************************************************
**
**      General Information
*/

#define MBOX_REGS			8	/* how many mailbox registers */
#define IOCB_SEGS			2
#define CONTINUATION_SEGS		5
#define IOCB_SIZE			64

#define MAX_CONTINUATION_ENTRIES	254

#define MAX_ADAPTERS            128

/* soon to be obsolete constants */
#define QL_MAX_PROBED_LUN		16

/* soon to replace above obsolete constants! */
#define	QL_FABRIC_ID_HIGH	0xff
#define	QL_FABRIC_ID_LOW	0x00
#define QL_FABRIC_FL_PORT	0x7e
#define QL_FABRIC_RESERVED1	0x7f
#define QL_FABRIC_RESERVED2	0x80
#define QL_FABRIC_RESERVED3	0xff
#define	QL_MAX_LOOP_IDS		QL_FABRIC_ID_HIGH+1
#define QL_MAX_PORTS		QL_MAX_LOOP_IDS
#define	QL_MAXLUNS		2048


#ifdef _STANDALONE

#define REQUEST_QUEUE_DEPTH     8
#define RESPONSE_QUEUE_DEPTH    8

#else

#define REQUEST_QUEUE_DEPTH     2048
#define RESPONSE_QUEUE_DEPTH    256

#endif /* STANDALONE */

#define ALL_QUEUES		(-1)

/********************************************************
** NVRAM CONSTANTS
**	Used in file nv.c
*/

#ifdef SEVEN_BIT_ADDR
#define NV_NUM_ADDR_BITS 7
#else
#define NV_NUM_ADDR_BITS 8
#endif
#define NV_START_BIT    4
#define NV_WRITE_OP (NV_START_BIT + 1 << NV_NUM_ADDR_BITS)
#define NV_READ_OP  (NV_START_BIT + 2 << NV_NUM_ADDR_BITS)
#define NV_ERASE_OP (NV_START_BIT + 3 << NV_NUM_ADDR_BITS)
#define NV_DELAY_COUNT  100
#define NV_DESELECT 0
#define NV_CLOCK    1
#define NV_SELECT   2
#define NV_DATA_OUT 4
#define NV_DATA_IN  8
#define NVRAM_SIZE  256     /* Size in bytes. */

/****************************************************************
**
**  ISP Register Definitions
*/

#define	RISC_CLEAR_DMA	0x0004

typedef volatile struct {

	/*
	** Bus interface registers.
	*/              	        /* Offsets  */


	uint16_t flash_data;		/* 0002 */
	uint16_t flash_addr;		/* 0000 */

	uint16_t icsr;			/* 0006 */	/* control_status */
	uint16_t r04;			/* 0004 */

	uint16_t isr;			/* 000A */	/* bus_isr */
	uint16_t icr;			/* 0008 */	/* bus_icr */

	uint16_t isp_nvram;		/* 000E */
	uint16_t bus_sema;		/* 000C */

								
	/*
	** Mailbox registers.
	*/
	uint16_t mailbox1;		/* 0012     */
	uint16_t mailbox0;		/* 0010     */

	uint16_t mailbox3;		/* 0016     */
	uint16_t mailbox2;		/* 0014     */

	uint16_t mailbox5;		/* 001A     */
	uint16_t mailbox4;		/* 0018     */

	uint16_t mailbox7;		/* 001E     */
	uint16_t mailbox6;		/* 001C     */

								
	/*
	** Skip over Registers not used by the driver.
	** Command/transmit/Rcv DMA registers
	*/

	uint16_t r22;	/* offset 0x22 */
	uint16_t cmd_dma;	/* offset 0x20 */	/* command dma control */

	uint16_t r26;
	uint16_t r24;

	uint16_t r2a;
	uint16_t r28;

	uint16_t r2e;
	uint16_t r2c;

	uint16_t r32;
	uint16_t r30;

	uint16_t r36;
	uint16_t r34;

	uint16_t r3a;
	uint16_t r38;

	uint16_t r3e;
	uint16_t r3c;

	uint16_t r42;
	uint16_t xmit_dma;				/* transmit dma control */

	uint16_t r46;
	uint16_t r44;

	uint16_t r4a;
	uint16_t r48;

	uint16_t r4e;
	uint16_t r4c;

	uint16_t r52;
	uint16_t r50;

	uint16_t r56;
	uint16_t r54;

	uint16_t r5a;
	uint16_t r58;

	uint16_t r5e;
	uint16_t r5c;

	uint16_t r62;
	uint16_t recv_dma;

	uint16_t r66;
	uint16_t r64;

	uint16_t r6a;
	uint16_t r68;

	uint16_t r6e;
	uint16_t r6c;

	uint16_t r72;
	uint16_t r70;

	uint16_t r76;
	uint16_t r74;

	uint16_t r7a;
	uint16_t r78;

	uint16_t r7e;
	uint16_t r7c;

	/*
	** Skip over Registers not used by the driver.
	** Risc registers
	*/
	uint16_t r82;
	uint16_t r80;

	uint16_t r86;
	uint16_t r84;

	uint16_t r8a;
	uint16_t r88;

	uint16_t r8e;
	uint16_t r8c;

	uint16_t r92;
	uint16_t r90;

	uint16_t r96;
	uint16_t r94;

	uint16_t r9a;
	uint16_t r98;

	uint16_t r9e;
	uint16_t r9c;

	uint16_t ra2;
	uint16_t ra0;

	uint16_t ra6;			/* a6   */
	uint16_t pcr;			/* a4   */

	uint16_t raa;
	uint16_t ra8;

	uint16_t rae;
	uint16_t rac;

	uint16_t rb2;
	uint16_t mctr;

	uint16_t rb6;
	uint16_t rb4;

	uint16_t rba;
	uint16_t rb8;

	uint16_t rbe;
	uint16_t rbc;

	/*
	** Host Configuration and Control register.
	*/
	uint16_t rc2;			/* c2   */
	uint16_t hccr;			/* c0   */

	uint16_t rc6;			/* c6   */
	uint16_t rc4;			/* c4   */

	uint16_t rca;			/* ca   */
	uint16_t rc8;			/* c8   */

	uint16_t rce;			/* ce   */
	uint16_t rcc;			/* cc	*/

	uint16_t rd2;			/* d2   */
	uint16_t rd0;			/* d0   */

	uint16_t rd6;			/* d6   */
	uint16_t rd4;			/* d4   */

	uint16_t rda;			/* da   */
	uint16_t rd8;			/* d8   */

	uint16_t rde;			/* de   */
	uint16_t rdc;			/* dc	*/

	uint16_t re2;			/* e2   */
	uint16_t re0;			/* e0   */

	uint16_t re6;			/* e6   */
	uint16_t re4;			/* e4   */

	uint16_t rea;			/* ea   */
	uint16_t re8;			/* e8   */

	uint16_t ree;			/* ee   */
	uint16_t rec;			/* ec	*/

	uint16_t rf2;			/* f2   */
	uint16_t rf0;			/* f0   */

	uint16_t rf6;			/* f6   */
	uint16_t rf4;			/* f4   */

	uint16_t rfa;			/* fa   */
	uint16_t rf8;			/* f8   */

	uint16_t rfe;			/* fe   */
	uint16_t rfc;			/* fc	*/


} CONTROLLER_REGS;


/**********************************************************
**
**      Internal Command Information
*/

#define READ_CAP_DATA_LENGTH        8
#ifdef _STANDALONE
#define REQ_SENSE_DATA_LENGTH       18
#else
#define REQ_SENSE_DATA_LENGTH       32	/* max in a single status iocb */
#endif

/*********************************************************
**
**      Host Adapter Information
*/


/*
**	Data segment descriptor for 32/64 bit addressing.
*/
typedef struct {
	__psint_t	base;
	uint32_t	count;
} data_seg;


/*
**	REQ_COOKIE
**	SR_REQ_COOKIE
**
**		REQ_COOKIE is the definition of the cookie placed into
**		iocbs given to the fc firmware.  It is returned
**		on iocb completion within either mailbox registers
**		(fastpost) or status iocbs.
**
**		The cookie is mapped into the sr_spare element of
**		the scsi_request_t via the SR_SPARE structure.
*/

#define MIN_COOKIE		(1)
#define MIN_COOKIE_INDEX	(1)			/* subtract 1 for index */
#define MAX_COOKIE_INDEX	(QL_FABRIC_ID_HIGH)	/* HIGH is reserved! */
#define MAX_COOKIE		(0x7ff)

#define COOKIE_MAGIC	(0xe)
typedef union {
	struct {
		uint32_t index:9,
			 magic:4,
			 unique:8,
			 value:11;
	} field;
	uint32_t data;
} REQ_COOKIE;

typedef union {
	struct {
		int32_t		timeout;
		REQ_COOKIE	cookie;
	} field;
	uint64_t data;
} SR_SPARE;

/* WARNING:  On a 32 bit system, the above use of the sr_spare field will
	clobber the next 32 bits after it in the scsi_request data structure.
	In this case, sr_status, which is harmless.  But keep an eye on sys/scsi.h
*/

#pragma pack(1)

typedef struct {
	uint8_t	version;			/* Risc parameter Block Version number */
	uint8_t	reserved0;
	union {
		uint8_t  byte;
		struct {				   /* bits 7-0*/
			uint8_t	enableTGTDVCtype :1;    /* Enable Target Device Type ** NOT USED */
			uint8_t	enableADISC      :1;    /* Enable Hard Address Discovery */
			uint8_t	disableInitator  :1;    /* Disbale Initator Mode ** NOT USED */
			uint8_t	enableTargetMode :1;    /* Enable Target mode ** NOT USED */
			uint8_t	enableFastposting:1;    /* Enable fastposting protocol */
			uint8_t	enableFullDuplex :1;    /* Enable full duplex for ISP2200 */
			uint8_t	enableFairness   :1;    /* Enable fairness on the loop */
			uint8_t	enableHardLoopID :1;	/* Enable Hard ID on the adapter */
		}bits;
	}fwOption0;
	union {
		uint8_t  byte;
		struct {				   /* bits 15-8*/
			uint8_t extendedCB	 :1;	/* extended control block, ISP 2200 only */
			uint8_t nameOption       :1;	/* portname field is also nodename */
			uint8_t	fullLoginOnLIP   :1;    /* Enable Full Login (PDISC) on all LIPs */

			uint8_t stopPortQueueFull:1;	/* stop port queue on full status */
			uint8_t previousLoopId   :1;	/* use hard/previous id during LIPA */

			uint8_t	descendIDSearch  :1;    /* F/W to acquire Descending Loop ID */
			uint8_t	disbaleInitialLIP:1;    /* Disbale Initial LIP during init f/w command **NOT USED */
			uint8_t	enablePDBChange  :1;	/* Enable Port Data Base Change AEN */
		}bits;
	}fwOption1;
	uint16_t	maxFrameSize;                   /* Maximum Frame size for receive frames */
	uint16_t maxIOCBperPort;                 /* Maximum IOCBS allocation per Port */
	uint16_t executionThrottle;              /* Execution throttle per FC port */
	uint8_t  retryCount;                     /* Retry Count */
	uint8_t  retryDelay;                     /* Retry Delay */

	uint8_t  portName[8];                    /* FC Node Name for this adapter must be unique on the loop */

	uint8_t  hardID;                         /* Hard ID for this adapter */
	uint8_t  reserved1;                      /* upper byte for Hard ID not used */

	uint8_t  inquiry_data;			/* reserved at this time */
	uint8_t  loginTimeout;                   /* Login Timeout in Second, if 0 it is 2*RTOV */
	uint8_t  nodeName[8];                   /* Node name if not using port name for both */
} nvram_risc_param;


typedef struct {
	char    id[4];              /* "ISP "   */
	uint8_t  version;
	uint8_t  reserved0;
	nvram_risc_param fw_params;   /* Firmware Parameter Block */

	/* Host Parameter Block in NVRAM */
	uint16_t host_options;         /* not used by this driver */
	uint8_t	boot_node_name[8];    /* not used by this driver */
	uint8_t  boot_lun;             /* not used by this driver */
	uint8_t  Delay_after_reset;    /* not used by this driver */
	uint8_t  portdownretrycount;   /* not used by this driver */
	uint8_t  reserved11;           /* not used by this driver */
	uint8_t  unused[198];          /* Defined as Reserved by Qlogic */
	uint16_t	sub_system_vendor_ID; /* Subsystem vendor ID  Offset 250 */
	uint16_t	sub_system_device_ID; /* Subsystem device ID  Offset 252 */
	uint8_t  reserved22;
	uint8_t  checksum;             /* NVRAM Checksum */
} Default_parameters;

/*
**	Firmware Initialization Control Block.
**  First 32 bytes are same as nvram_risc_param structure but bytes
**  are swapped ready for DMA.
*/

typedef struct {
	uint8_t		version;
	uint8_t		reserved0;
	uint8_t		fwOption0, fwOption1;
	uint16_t	maxFrameSize;
	uint16_t	maxIOCBperPort;

	uint16_t	executionThrottle;
	uint8_t		retryCount;
	uint8_t		retryDelay;
	uint8_t		portName0, portName1, portName2, portName3;

	uint8_t		portName4, portName5, portName6, portName7;
	uint8_t		hardID;
	uint8_t		reserved1;
	uint8_t		inquiry_data;
	uint8_t		loginTimeout;

	uint8_t		nodeName0;
	uint8_t		nodeName1;
	uint8_t		nodeName2;
	uint8_t		nodeName3;
	uint8_t		nodeName4;
	uint8_t		nodeName5;
	uint8_t		nodeName6;
	uint8_t		nodeName7;

	uint16_t	request_out;		/* Request Queue Pointer (Offset) */
	uint16_t	response_in;		/* Response Queue Pointer (Offset) */
	uint16_t	requestQ_depth;		/* Request Queue size */
	uint16_t	responseQ_depth;	/* Reseponse Queue size */

	uint32_t	requestQ_base_addrlow;	/* lower part of 64 bit Request Queue Physical address */
	uint32_t	requestQ_base_addrhigh;	/* higher part of 64 bit Request Queue Physical address */

	uint32_t	responseQ_base_addrlow; /* lower part of 64 bit Response Queue Physical address */
	uint32_t	responseQ_base_addrhigh;/* higher part of 64 bit Response Queue Physical address */

	uint16_t	lun_enables;		
	uint8_t		command_resource_count;
	uint8_t		immediate_notify_resource_count;
	uint16_t	lun_timeout;
	uint16_t	reserved4;

	struct {
		uint16_t enableFCTape:1;			/* FC-Tape support */
		uint16_t connectionOptions:3;			/* 0 - loop only
								** 1 - point to point only
								** 2 - loop preferred
								** 3 - point to point preferred
								** 4-f - reserved
								*/
		uint16_t operationMode:4;			/* 0 - no multiple response posts
								** 1 - combine multiple responses, 16 bit handles, interrupt
								** 2 - combine multiple responses, 32 bit handles, interrupt
								** 3 - multiple responses, 16 bit, delay interrupt
								** 4 - multiple responses, 32 bit, delay interrupt
								** 5-1f - reserved
								*/

		uint16_t reserved:2;				/* must be zero */
		uint16_t enableReadXFR_RDY:1;			/* scsi target mode */
		uint16_t nonParticipateHardAddressFail:1;	/* can't get specified hard address go non-participating */
		uint16_t enableCommandReferenceNumber:1;	/* FC-Tape support */
		uint16_t enableACK0:1;				/* Class 2 support */
		uint16_t enableFibreChannelConfirm:1;		/* FC-Tape support */
		uint16_t enableClass2:1;			/* Class 2 support */
	} additional_fw_opts;
	uint8_t		response_accumulation_timer;
	uint8_t		interrupt_delay_timer;
	uint32_t	reserved5a;

	uint8_t		reserved5b[0x18];

} init_fw_param_block; 

#pragma pack(0)
typedef struct {
	timespec_t	timestamp;
	void		*ctlr;
	char		*format;
	__psint_t	arg0;
	__psint_t	arg1;
	__psint_t	arg2;
	__psint_t	arg3;
	__psint_t	arg4;
	__psint_t	arg5;
} trace_entry_t;

typedef struct {
	timespec_t	timestamp;
	void		*ctlr;
	char		*format;
	__psint_t	arg0;
	__psint_t	arg1;
	__psint_t	arg2;
	__psint_t	arg3;
	__psint_t	arg4;
	__psint_t	arg5;
	__psint_t	arg6;
	__psint_t	arg7;
	__psint_t	arg8;
} dump_entry_t;


typedef struct {
	uint8_t	some_data_elements;
} IOCB;

/*
** local info specific to the ql lun vertex (qli)
*/

struct qlfc_local_lun_info {
	mutex_t			qli_open_mutex;	/* open lock */
	mutex_t			qli_lun_mutex;	/* LUN lock */
	scsi_request_t *	qli_awaitf;     /* fwd pointer for commands awaiting abort complete */
	scsi_request_t *	qli_awaitb;     /* back pointer for commands awaiting abort complete */
	scsi_request_t *	qli_iwaitf;     /* fwd pointer for commands awaiting init. complete */
	scsi_request_t *	qli_iwaitb;     /* back pointer for commands awaiting init. complete */
	scsi_request_t *	qli_abt_rq;	/* scsi request to be aborted via abort iocb */
	scsi_target_info_t	*qli_tinfo;	/* scsi target info */
	void			(*qli_sense_callback)();	/* Sense callback routine */
	struct qlfc_local_lun_info	*next;	/* pointer to next local lun info */
	int			qli_cmd_awcnt;  /* abort waiting command count for this LUN */
	int			qli_cmd_iwcnt;  /* init waiting command count for this LUN */
	int			qli_ref_count;	/* no. of references */
	int			qli_cmd_rcnt;   /* issued command count to this LUN */
	volatile uint32_t	qli_dev_flags;	/* lun device flags */
	int			qli_recovery_step;	/* current recovery step */
	uint16_t		qli_lun;	/* this should be obvious! */
	uint8_t			qli_crn;	/* command reference number, for fc-tape */
};
typedef struct qlfc_local_lun_info qlfc_local_lun_info_t;

/*
** local info specific to the qlfc target vertex (qti)
*/

struct qlfc_local_targ_info {
	scsi_request_t *req_active;			/* active requests */
	scsi_request_t *req_last;			/* last active request on queue */
	int		req_count;			/* number of commands */
	uint16_t	target;
	int		recovery_step;			/* next error recovery step */
	mutex_t		target_mutex;
	vertex_hdl_t	targ_vertex;
	qlfc_local_lun_info_t *local_lun_info;		/* qli: first one on the queue */
	qlfc_local_lun_info_t *last_local_lun_info;
	int		awol_timeout;			/* how many seconds to wait before retry login */
	int		awol_retries;			/* how many times to try and get it back */
	int		awol_giveup_retries;		/* how many retries before cannot proceed */
	int		awol_stop;			/* stop at time, in seconds */
	uint16_t	awol_step;			/* where we are in recovery */
	uint8_t		lunmask[QL_MAXLUNS/8];
	uint8_t		unique_cookie;
};
typedef struct qlfc_local_targ_info qlfc_local_targ_info_t;

#define	TM_FABRIC_DEVICE	0x001
#define	TM_LOOP_DEVICE		0x002
#define	TM_LOGGED_IN		0x004
#define	TM_REPROBE		0x008	/* being reprobed (update?) */
#define	TM_SINGLE_STEP		0x010	/* single step commands to this target */
#define	TM_AWOL			0x020	/* device HAS skipped on us */
#define	TM_AWOL_PENDING		0x040	/* device may have skipped on us */
#define	TM_AWOL_DEMON_NOTIFIED	0x080	/* demon notified of potential device skip */
#define TM_QUEUE_FULL		0x100	/* device reported queue full */
#define TM_RESERVED		0x80000000

/*
** target map, one per loop_id, statically allocated array
**	in the controller structure
*/
struct target_map {
	uint32_t		tm_port_id;
	volatile uint32_t	tm_flags;
	uint64_t		tm_node_name;
	uint64_t		tm_port_name;
	qlfc_local_targ_info_t	*tm_qti;
	mutex_t			tm_mutex;	/* LUN lock */
	uint16_t		tm_maxq;				/* port queue parameter */
	uint16_t		tm_exec_throttle;		/* port queue parameter */
};
typedef struct target_map target_map_t;

/*****************************************************
*
*	POSITION_MAP
*
*****************************************************/

struct positionMap {			/* requires qlfc_swap64() */
	uint8_t		pm_count;	/* count of ports reporting */
	uint8_t		pm_alpa[127];	/* alpa of ports reporting, starting with loop master */
};
typedef struct positionMap positionMap_t;


/*
** Demon constants
*/

#define	DEMON_MAX_EVENTS	(512)

/*
** Demon process control flags
*/

#define	DEMON_SHUTDOWN		0000001
#define	DEMON_TIMEOUT		0000002
#define	DEMON_QUEUE_FULL	0000004
#define	DEMON_ACTIVE		0000010		/* set by i/h, cleared by demon */
#define	DEMON_NEW_EVENT		0000020		/* set by i/h, cleared by demon */

/*
** Demon messages.
*/

enum demon_messages {
	DEMON_NO_MSG			= 0,
	DEMON_MSG_LIP			= 1,
	DEMON_MSG_LIP_RESET		= 2,
	DEMON_MSG_PDBC			= 3,
	DEMON_MSG_LOOP_UP		= 4,
	DEMON_MSG_LOOP_DOWN		= 5,
	DEMON_MSG_LINK_UP		= 6,
	DEMON_MSG_SCN			= 7,
	DEMON_MSG_SYSTEM_ERROR		= 8,
	DEMON_MSG_ERROR_RECOVERY	= 9,
	DEMON_MSG_SET_TARGET_AWOL	= 10,
	DEMON_MSG_TARGET_AWOL		= 11,
	DEMON_MSG_ABORT_DEVICE		= 12,
	DEMON_MSG_RESET_CONTROLLER	= 13,
	DEMON_MSG_PROBE_BUS		= 14,
	DEMON_MSG_DUMP_CONTROLLER	= 15,
	DEMON_MSG_ABORT_IOCB		= 16,
	DEMON_MSG_ABORT_ALL_IOCBS	= 17
};
typedef enum demon_messages demon_message_t;

/*
** Pointers to firmware values.
*/

struct qLogicFirmware {
	uint16_t *extended_lun;
	uint16_t *fc_tape;
	uint16_t *risc_code_version;
	uint8_t  *firmware_version;
	uint16_t *risc_code_addr01;
	uint16_t *risc_code01;
	uint16_t *risc_code_length01;
};
typedef struct qLogicFirmware qLogicFirmware_t;

struct qlfc_controller {

	/*
	** Linked list of structures used by timeout processing and a few
	** other misc. functions that have to touch all adapters.
	*/
	char		controller_ec[8];
	char		hwgname [LABEL_LENGTH_MAX];

	void		*next;			/* pointer to next controller structure */

	int		ctlr_type_index;	/* for controller specific functions, if any */
	int		ctlr_number;		/* globally encountered controllers */
	int		ctlr_which;		/* which controller on board */

	/*
	** Firmware loaded into this controller.
	*/

	qLogicFirmware_t	*qlfw;

	/*
	** Base I/O Address for this card.
	** Make sure to convert this to a usable virtual address.
	*/
	char		controller_regs_ec[8];
	CONTROLLER_REGS	*base_address;

	uint64_t	port_name;		/* from nvram */

	vertex_hdl_t	ctlr_vhdl;
	vertex_hdl_t	pci_vhdl;

	char		quiesce_ec[8];
	int		quiesce_in_progress_id; /* how long to wait for quiesce to   */
						/* complete			     */
	toid_t		quiesce_id;             /* id for quiesce_time	 	     */
	int		quiesce_time;           /* how long to keep the bus quiesced */

	char		flags_ec[8];
	volatile uint32_t	flags;			/* general flags for this adapter */
	uint16_t	revision;		/* PCI Revision ID Register */
#if defined(SN) || defined(IP30)
	uint16_t	bridge_revnum;	        /* Bridge rev number */
#endif

	/*
	** Request and Response pointers
	*/
	char		reqrsp_ptrs_ec[8];
	uint16_t	request_in;		/* request in pointer */
	uint16_t	request_out;		/* request out pointer */
	uint16_t	response_out;		/* response out pointer (copy of mailbox 5 OUT) */
	uint16_t	response_in;	        /* response in pointer (copy of mailbox 5 IN) */

	int		queue_space;

	/*
	** note: request_misc and response_misc data area cannot be
	**	changed except during the processing of the mailbox
	**	command.  These pointers are only used during mailbox
	**	commands.
	*/
	char		dma_ptrs_ec[8];
	uint8_t		*request_base;		/* base address of page for requests to controller */
	uint8_t		*response_base;		/* base address of page for responses from controller */
	uint8_t		*request_ptr;		/* start of iocb request queue */
	uint8_t		*response_ptr;		/* start of iocb response queue */
	void		*request_misc;		/* start of misc. requests to controller */
	void		*response_misc;		/* start of misc. responses from controller */
	size_t		request_size;
	size_t		response_size;
	__psunsigned_t	request_dmaptr;		/* translated address for request_ptr */
	__psunsigned_t	response_dmaptr;	/* translated address for response_ptr */

	__psunsigned_t	request_misc_dmaptr;
	__psunsigned_t	response_misc_dmaptr;

	char		queue_depth_ec[8];
	int		ql_request_queue_depth;
	int		ql_response_queue_depth;

	/*
	** Queued requests, waiting activiation.
	*/
	char		queues_ec[8];
	scsi_request_t	*req_forw;
	scsi_request_t	*req_back;
	scsi_request_t	*waitf;
	scsi_request_t	*waitb;

	/*
	** Completed requests, waiting notify
	*/
	scsi_request_t	*compl_forw;
	scsi_request_t	*compl_back;

	/*
	** Current loop position maps.
	*/

	char		target_map_ec[8];
	target_map_t	target_map[QL_MAX_LOOP_IDS];	/* current assignments */

	char		defaults_ec[8];

	Default_parameters	ql_defaults;

	union   {
		uint8_t    b[NVRAM_SIZE];
		uint16_t   w[NVRAM_SIZE / 2];
		uint32_t   l[NVRAM_SIZE / 4];
		uint64_t   d[NVRAM_SIZE / 8];
		Default_parameters	nvram_data;
	} nvram_buf;


	/*
	** scatter gather holding place
	*/
	char		alen_ec[8];
	alenlist_t	alen_p;

	/*
	** Trace buffer information.
	*/

	char		trace_ec[8];

	trace_entry_t	*first_trace_entry;	/* base of allocated trace buffer */
	int		trace_index;		/* slot for next trace entry */
	trace_entry_t	*last_trace_entry;	/* last storable entry */
	int32_t		trace_code;		/* used with traceif() */
	uint16_t	trace_enabled;
	dump_entry_t	*dump_buffer;
	int		dump_index;
	dump_entry_t	*dump_last_entry;
	
	/*
	** mutual exclusion for mp work
	*/

	char		mutex_ec[8];

	mutex_t		res_lock;	/* response lock, incoming interrupt responses */
	mutex_t		req_lock;	/* request lock, outgoing requests */
	mutex_t		mbox_lock;
	mutex_t		waitQ_lock;
	mutex_t		probe_lock;
	sema_t		mbox_done_sema;
	sema_t		reset_sema;
	sema_t		probe_sema;

	/*
	** demon interface variables
	*/

	char		demon_queue_ec[8];

	struct {
				uint64_t	msg;
				__psunsigned_t	arg;

	}		demon_msg_queue [DEMON_MAX_EVENTS];

	char		demon_ec[8];

	int		demon_msg_count;
	int		demon_msg_in;
	int		demon_msg_out;
	int		demon_flags;
	int		demon_timeout;
	int		demon_pdbc_count;	/* current count of pdbc events */
	int		demon_scn_count;	/* current count of scn events */
	
	mutex_t		demon_lock;
	sema_t		demon_sema;


	/*
	** timeout and error recovery stuff
	*/

	char		timeout_ec[8];

	int		ql_ncmd;
	int		ql_awol_count;
	int		ha_last_intr;
	int		drain_count;
	int		drain_timeout;
	int		mbox_timeout_info;
	uint16_t	mailbox[MBOX_REGS];
	uint16_t	mailbox_out[MBOX_REGS];		/* for debugging */
	int		recovery_step;			/* next error recovery step */

	/*
	** Counters.
	*/

	int		lip_resets;	/* back to back */
	int		undelivered_interrupt_count;

	char		stats_ec[8];

	struct adapter_stats    *adp_stats;

};
typedef struct qlfc_controller QLFC_CONTROLLER;

/*
** Step recovery.
*/

struct recovery_step {
	STATUS	(*step_func)(QLFC_CONTROLLER *,__psunsigned_t);
	int	step_drain_timeout;
};
typedef struct recovery_step recovery_step_t;



/*
** Host adapter flags.
**	Set and cleared via atomic funcs.
0x20008481
*/


#define	CFLG_INITIALIZED		0x00000001	/* controls interrupt, and timeout processing */
#define	CFLG_SEND_MARKER		0x00000002
#define CFLG_ISP_PANIC			0x00000004
#define	CFLG_QUIESCE_IN_PROGRESS	0x00000008

#define	CFLG_QUIESCE_COMPLETE		0x00000010
#define	CFLG_DRAIN_IO			0x00000020
#define	CFLG_MAIL_BOX_DONE		0x00000040
#define	CFLG_MAIL_BOX_WAITING		0x00000080

#define	CFLG_MAIL_BOX_POLLING		0x00000100
#define	CFLG_SHUTDOWN			0x00000200
#define CFLG_EXTENDED_LUN		0x00000400
#define	CFLG_PROBE_ACTIVE		0x00000800

#define	CFLG_IN_INTR			0x00001000
#define	CFLG_DUMPING			0x00002000
#define	CFLG_RESPONSE_QUEUE_UPDATE	0x00004000
#define	CFLG_LOOP_UP			0x00008000

#define	CFLG_PDBC			0x00010000
#define	CFLG_LIP			0x00020000
#define	CFLG_LIP_RESET			0x00040000
#define	CFLG_SCN			0x00080000

#define	CFLG_STOP_PROBE			0x00100000
#define	CFLG_SOLID_LIP			0x00200000
#define	CFLG_PDBC_ACTIVE		0x00400000
#define	CFLG_STEP_RECOVERY_ACTIVE	0x00800000

#define CFLG_SCN_ENABLED		0x01000000
#define	CFLG_SCN_PENDING		0x02000000
#define CFLG_DEMON_NOT_READY		0x04000000
#define CFLG_BAD_STATUS			0x08000000

#define CFLG_START_THROTTLE		0x10000000
#define CFLG_LINK_MODE			0x20000000
#define CFLG_PROBE_WAITING		0x40000000
#define CFLG_FABRIC			0x80000000

/* NOTE: only one flag remaining */

#define	CFLG_NO_START			(CFLG_PDBC | \
					 CFLG_LIP | \
					 CFLG_LIP_RESET | \
					 CFLG_SCN | \
					 CFLG_ISP_PANIC | \
					 CFLG_START_THROTTLE | \
					 CFLG_QUIESCE_IN_PROGRESS | \
					 CFLG_QUIESCE_COMPLETE)
/*
** Device flags.
**	Set and cleared via atomic funcs.
*/

#define DFLG_INITIALIZED		0x0001
#define DFLG_INIT_IN_PROGRESS		0x0002 /* LUN Init in progress -- pause LUN queue */
#define DFLG_EXCLUSIVE			0x0004
#define DFLG_CONTINGENT_ALLEGIANCE	0x0008
#define DFLG_ABORT_IN_PROGRESS		0x0010 /* LUN Abort IOCBs in progress -- pause LUN queue */
#define DFLG_SEND_MARKER		0x0020 /* Send LUN marker */
#define DFLG_BEHAVE_QERR1		0x0040 /* QERR=1 for this device */
#define	DFLG_USE_CRN			0x0080 /* Use command reference numbers */
#define	DFLG_DEMON_RECOVERY_NOTIFIED	0x0100

/*
** Request packet host status errors.
*/

#define HSTS_NO_ERR			0x0000
#define HSTS_NO_DEV			0x1001
#define HSTS_NO_INIT			0x1002
#define HSTS_BAD_PARAMETER		0x1003

#define	QLOGIC_DEV_DESC_NAME	"qLogic Fibre Channel"

/****************************************************************
**
**  ISP Firmware Definitions
*/

/*
** Firmware State Definitions after Get f/w state command.
*/
#define FW_CONFIG_WAIT  0x0000
#define FW_WAIT_AL_PA   0x0001
#define FW_WAIT_LOGIN   0x0002
#define FW_READY        0x0003
#define FW_LOSS_OF_SYNC 0x0004
#define FW_ERROR        0x0005
#define FW_REINIT       0x0006
#define FW_NON_PART     0x0007

/*
** Entry Header Type Defintions.
*/
#define ET_COMMAND              0x19
#define ET_CONTINUATION         0x0a
/* NOTE on above:  To do 32 bit op properly, we'd want type 2 iocb.  However,
	we're stuffing 32 bit addrs into a card running in 64 bit mode.
	So since we dont want to declare 64 bit mode, but want 64 bit addrs... 
*/
#define ET_STATUS               0x03
#define ET_MARKER               0x04
#define ET_EXTENDED_COMMAND     0x05
#define ET_STATUS_CONTINUATION  0x10

/*
** Entry Header Flag Definitions.
*/
#define EF_CONTINUATION			0x01
#define EF_BUSY				0x02
#define EF_INVALID_ENTRY_TYPE		0x04
#define EF_INVALID_ENTRY_PARAMETER	0x08
#define EF_INVALID_ENTRY_COUNT		0x10
#define	EF_INVALID_ENTRY_ORDER		0x20
#define EF_ERROR_MASK   (EF_BUSY | EF_INVALID_ENTRY_TYPE | EF_INVALID_ENTRY_PARAMETER | EF_INVALID_ENTRY_COUNT | EF_INVALID_ENTRY_ORDER)


/***
**** ENTRY HEADER structure.
***/
typedef struct
{
	uint8_t	entry_type;
	uint8_t	entry_cnt;
	uint8_t	sys_def_1;
	uint8_t	flags;
} entry_header;



/***
**** COMMAND ENTRY structure.
**** This structure is defined for 64 bit PCI system. Bytes are swapped  
**** for 8 bytes.
***/
typedef struct {
	entry_header	hdr;
	uint8_t		handle_0, handle_1, handle_2, handle_3;

	uint8_t		target_lun;
	uint8_t		target_id;
	uint8_t		target_lun_extended_0, target_lun_extended_1;
	uint8_t		control_flags_0, control_flags_1;
	uint8_t		command_reference_number;
	uint8_t		reserved1;

	uint8_t		time_out_0, time_out_1;
	uint8_t		segment_cnt_0, segment_cnt_1;
	uint8_t		cdb0, cdb1, cdb2, cdb3;

	uint8_t		cdb4, cdb5, cdb6, cdb7, cdb8, cdb9, cdb10, cdb11;

	uint8_t		cdb12, cdb13, cdb14, cdb15;
	uint8_t		total_xfer_count_0, total_xfer_count_1, total_xfer_count_2, total_xfer_count_3;

	uint8_t		base0_0, base0_1, base0_2, base0_3, base0_4, base0_5, base0_6, base0_7;

	uint8_t		count0_0, count0_1, count0_2, count0_3;
	uint8_t		base1_0, base1_1, base1_2, base1_3;
	
	uint8_t		base1_4, base1_5, base1_6, base1_7;
	uint8_t		count1_0, count1_1, count1_2, count1_3;
} command_entry;


/*
** Command Entry Control Flag Definitions.
*/
#define CF_RESERVED_0               0x01
#define CF_HEAD_TAG                 0x02
#define CF_ORDERED_TAG              0x04
#define CF_SIMPLE_TAG               0x08
#define CF_RESERVED_4               0x10
#define CF_READ                     0x20
#define CF_WRITE                    0x40
#define CF_RESERVED_7       	    0x80
#define CF1_RESERVED_8       	    0x01
#define CF1_RESERVED_9       	    0x02
#define CF1_RESERVED_10       	    0x04
#define CF1_RESERVED_11       	    0x08
#define CF1_RESERVED_12       	    0x10
#define CF1_RESERVED_13       	    0x20
#define CF1_RESERVED_14       	    0x40
#define CF1_RESERVED_15       	    0x80




/***
**** CONTINUATION ENTRY structures.
**** This structure is defined for 64 bit PCI system. Bytes are swapped  
**** for 8 bytes.
***/
typedef struct {
	entry_header	hdr;
	uint8_t		base0_0, base0_1, base0_2, base0_3;

	uint8_t		base0_4, base0_5, base0_6, base0_7;
	uint8_t		count0_0, count0_1, count0_2, count0_3;

	uint8_t		base1_0, base1_1, base1_2, base1_3;
	uint8_t		base1_4, base1_5, base1_6, base1_7;

	uint8_t		count1_0, count1_1, count1_2, count1_3;
	uint8_t		base2_0, base2_1, base2_2, base2_3;

	uint8_t		base2_4, base2_5, base2_6, base2_7;
	uint8_t		count2_0, count2_1, count2_2, count2_3;

	uint8_t		base3_0, base3_1, base3_2, base3_3;
	uint8_t		base3_4, base3_5, base3_6, base3_7;

	uint8_t		count3_0, count3_1, count3_2, count3_3;
	uint8_t		base4_0, base4_1, base4_2, base4_3;

	uint8_t		base4_4, base4_5, base4_6, base4_7;
	uint8_t		count4_0, count4_1, count4_2, count4_3;
} continuation_entry;


/***
**** MARKER ENTRY structure.
**** This structure is defined for 64 bit PCI system. Bytes are swapped  
**** for 8 bytes.
***/

typedef struct
{
	entry_header	hdr;
	uint8_t		reserved0[4];
	uint8_t		target_lun;
	uint8_t		target_id;
	uint8_t		modifier;
	uint8_t		reserved1;
	uint8_t		reserved2[2];
	uint8_t		target_lun_extended_0;	/* LSB */
	uint8_t		target_lun_extended_1;	/* MSB */
	uint8_t		reserved3[48];
} marker_entry;


/*
** Marker Entry Modifier Definitions.
*/
#define MM_SYNC_DEVICE          0
#define MM_SYNC_TARGET          1
#define MM_SYNC_ALL             2


/***
**** STATUS ENTRY structure.
***/

typedef struct
{
	entry_header	hdr;
	uint32_t	handle;

	uint16_t	fc_scsi_status;
	uint16_t	completion_status;
	uint16_t	state_flags;
	uint16_t	status_flags;

	uint16_t	resp_info_length;
	uint16_t	req_sense_length;
	uint32_t	residual;

	uint8_t		fcp_response_info[8];

	uint8_t		req_sense_data[32];
} status_entry;

/*
** Status Entry: COMPLETION STATUS.
*/
#define SCS_COMPLETE                	0x0000
#define SCS_INCOMPLETE              	0x0001
#define SCS_DMA_ERROR               	0x0002
#define SCS_TRANSPORT_ERROR         	0x0003
#define SCS_RESET_OCCURRED          	0x0004
#define SCS_ABORTED                 	0x0005
#define SCS_TIMEOUT                 	0x0006
#define SCS_DATA_OVERRUN            	0x0007
#define SCS_ABORT_MSG_FAILED            0x000E
#define SCS_DEVICE_RESET_MSG_FAILED     0x0012
#define SCS_DATA_UNDERRUN           	0x0015

#define SCS_INVALID_TYPE_ENTRY		    0x001B
#define SCS_DEVICE_QUEUE_FULL           0x001C
#define SCS_PORT_UNAVAILABLE            0x0028
#define SCS_PORT_LOGGED_OUT             0x0029
#define SCS_PORT_CONFIGURATION_CHANGED  0x002A

/*
** Status IOCB: STATE FLAGS.
*/
#define SS_SENT_CDB             	0x0400	/* 2 ** 10 */
#define SS_TRANSFERRED_DATA         	0x0800	/* 2 ** 11 */
#define SS_GOT_STATUS               	0x1000	/* 2 ** 12 */
#define SS_TRANSFER_COMPLETE            0x4000	/* 2 ** 14 */

/*
** fc_status field Definitions.
*/
#define SCSI_STATUS_MASK		0x00FF
#define FCS_RESPONSE_VALID		0x0100
#define FCS_SENSE_LENGTH_VALID		0x0200
#define FCS_RESIDUAL_OVERRUN		0x0400
#define FCS_RESIDUAL_UNDERRUN		0x0800


/*
** Mailbox Command Definitions.
*/

#define MBOX_CMD_NOP				0x00
#define MBOX_CMD_LOAD_RAM			0x01
#define MBOX_CMD_EXECUTE_FIRMWARE		0x02
#define MBOX_CMD_DUMP_RAM			0x03
#define MBOX_CMD_WRITE_RAM_WORD			0x04
#define MBOX_CMD_READ_RAM_WORD			0x05
#define MBOX_CMD_MAILBOX_REGISTER_TEST		0x06
#define MBOX_CMD_VERIFY_CHECKSUM		0x07
#define MBOX_CMD_ABOUT_FIRMWARE			0x08
#define MBOX_CMD_LOAD_RISC_RAM			0x09
#define MBOX_CMD_DUMP_RISC_RAM			0x0A

#define	MBOX_CMD_CHECKSUM_FIRMWARE		0x0e

#define MBOX_CMD_INIT_REQUEST_QUEUE		0x10
#define MBOX_CMD_INIT_RESPONSE_QUEUE		0x11
#define MBOX_CMD_EXECUTE_IOCB			0x12
#define MBOX_CMD_WAKE_UP			0x13
#define MBOX_CMD_STOP_FIRMWARE			0x14
#define MBOX_CMD_ABORT_IOCB			0x15
#define MBOX_CMD_ABORT_DEVICE			0x16
#define MBOX_CMD_ABORT_TARGET			0x17
#define MBOX_CMD_RESET				0x18
#define	MBOX_CMD_STOP_QUEUE			0x19
#define	MBOX_CMD_START_QUEUE			0x1a
#define	MBOX_CMD_STEP_QUEUE			0x1b
#define	MBOX_CMD_ABORT_QUEUE			0x1c
#define	MBOX_CMD_GET_QUEUE_STATUS		0x1d

#define MBOX_CMD_GET_FIRMWARE_STATUS    	0x1F
#define MBOX_CMD_GET_LOOP_ID		  	0x20

#define MBOX_CMD_GET_RETRY_COUNT        	0x22

#define MBOX_CMD_GET_PORT_QUEUE_PARAMETERS	0x29

#define MBOX_CMD_SET_RETRY_COUNT        	0x32
#define MBOX_CMD_SET_PORT_QUEUE_PARAMETERS	0x39

#define MBOX_CMD_LOAD_RAM_64			0x50
#define MBOX_CMD_DUMP_RAM_64			0x51

#define MBOX_CMD_EXECUTE_IOCB_64		0x54

#define MBOX_CMD_INITIALIZE_FIRMWARE		0x60
#define MBOX_CMD_GET_INIT_CONTROL_BLOCK		0x61
#define MBOX_CMD_INITIATE_LIP			0x62
#define MBOX_CMD_GET_POSITION_MAP		0x63
#define MBOX_CMD_GET_PORT_DATABASE		0x64
#define MBOX_CMD_CLEAR_ACA			0x65
#define MBOX_CMD_SEND_TARGET_RESET		0x66
#define MBOX_CMD_CLEAR_TASK_SET			0x67
#define MBOX_CMD_ABORT_TASK_SET			0x68
#define MBOX_CMD_GET_FIRMWARE_STATE		0x69
#define	MBOX_CMD_GET_PORT_NAME			0x6a
#define MBOX_CMD_GET_LINK_STATUS		0x6B
#define	MBOX_CMD_INITIATE_LIP_RESET		0x6c

#define	MBOX_CMD_SEND_SNS			0x6e
#define	MBOX_CMD_LOGIN_FABRIC_PORT		0x6f
#define	MBOX_CMD_SEND_CHANGE_REQUEST		0x70
#define	MBOX_CMD_LOGOUT_FABRIC_PORT		0x71

#define MBOX_CMD_LIP_FOLLOWED_BY_LOGIN		0x72
#define MBOX_CMD_LOGIN_LOOP_PORT		0x74
#define	MBOX_CMD_GET_PORT_NODE_NAME_LIST	0x75
#define MBOX_CMD_SEND_LFA			0x7d

/*
** Mailbox Status Definitions.
*/
#define MBOX_STS_FIRMWARE_ALIVE         	0x0000
#define MBOX_STS_CHECKSUM_ERROR         	0x0001
#define MBOX_STS_SHADOW_LOAD_ERROR      	0x0002
#define MBOX_STS_BUSY               		0x0004

#define MBOX_STS_COMPLETION_LOW			0x4000
#define MBOX_STS_COMPLETION_HIGH		0x7fff

/* general completion codes */
#define MBOX_STS_COMMAND_COMPLETE       	0x4000
#define MBOX_STS_INVALID_COMMAND        	0x4001
#define MBOX_STS_HOST_INTERFACE_ERROR   	0x4002
#define MBOX_STS_TEST_FAILED            	0x4003
#define MBOX_STS_COMMAND_ERROR          	0x4005
#define MBOX_STS_COMMAND_PARAMETER_ERROR    	0x4006

/* fabric login completion codes */
#define MBOX_STS_PORT_ID_USED			0x4007
#define MBOX_STS_LOOP_ID_USED			0x4008
#define MBOX_STS_ALL_IDS_IN_USE			0x4009

/* async event codes */
#define MBOX_ASTS_SYSTEM_ERROR          	0x8002
#define MBOX_ASTS_REQUEST_TRANSFER_ERROR    	0x8003
#define MBOX_ASTS_RESPONSE_TRANSFER_ERROR   	0x8004
#define MBOX_ASTS_REQUEST_QUEUE_WAKEUP      	0x8005

#define MBOX_ASTS_LIP_OCCURRED			0x8010
#define MBOX_ASTS_LOOP_UP			0x8011
#define MBOX_ASTS_LINK_UP			MBOX_ASTS_LOOP_UP
#define MBOX_ASTS_LOOP_DOWN			0x8012
#define MBOX_ASTS_LINK_DOWN			MBOX_ASTS_LOOP_DOWN
#define MBOX_ASTS_LIP_RESET			0x8013
#define MBOX_ASTS_PORT_DATABASE_CHANGE		0x8014
#define MBOX_ASTS_CHANGE_NOTIFICATION		0x8015
#define MBOX_ASTS_COMMAND_COMPLETE		0x8020
#define MBOX_ASTS_CTIO_COMPLETE			0x8021

/* to be defined */
#define MBOX_ASTS_LINK_MODE_IS_UP		0x8030
#define MBOX_ASTS_CONFIGURATION_CHANGE		0x8036




/*
** ISP Control/Status Register	(offset 6)
*/

#define ICSR_RESERVED		0177710
#define ICSR_MODULE_SELECT	0000060
#define ICSR_MODULE_FPM_140	0000060
#define ICSR_MODULE_FPM_100	0000040
#define ICSR_MODULE_FB		0000020
#define ICSR_MODULE_RISC	0000000
#define ICSR_PCI_SLOT_64_BIT	0000004
#define ICSR_FLASH_ENABLE	0000002
#define ICSR_SOFT_RESET		0000001

/*
** ISP Interrupt Control Register (offset 8)
*/

#define ICR_INTERRUPTS_ENABLED	0100000
#define ICR_RESERVED		0077700
#define ICR_ENABLE_FPM		0000040
#define ICR_ENABLE_FB		0000020
#define ICR_ENABLE_RISC		0000010
#define ICR_ENABLE_CMD_DMA	0000004
#define ICR_ENABLE_RCV_DMA	0000002
#define ICR_ENABLE_XMIT_DMA	0000001
#define ICR_DISABLE_ALL_INTS	0000000

/*
** ISP interrupt Status Register (offset 10)
*/

#define ISR_INTERRUPT		ICR_INTERRUPTS_ENABLED
#define ISR_FPM_INTERRUPT	ICR_ENABLE_FPM
#define ISR_FB_INTERRUPT	ICR_ENABLE_FB
#define ISR_RISC_INTERRUPT	ICR_ENABLE_RISC
#define ISR_CMD_DMA_INTERRUPT	ICR_ENABLE_CMD_DMA
#define ISR_RCV_DMA_INTERRUPT	ICR_ENABLE_RCV_DMA
#define ISR_XMIT_DMA_INTERRUPT	ICR_ENABLE_XMIT_DMA

/*
** Bus Semaphore Register Definitions.
*/

#define ISEM_RELEASE		000000
#define ISEM_LOCK		000001
#define ISEM_LOCK_MASK		000001
#define ISEM_STATUS_MASK	000002
#define ISEM_STATUS_GOT_LOCK	000000
#define ISEM_STATUS_ALREADY_LOCKED	000002


/*
** Host Command and Control Register Definitions.
*/

        /* Command Definitions */

#define HCCR_CMD_NOP			0x0000	/* NOOP */
#define HCCR_CMD_RESET			0x1000  /* reset RISC */
#define HCCR_CMD_PAUSE			0x2000  /* pause RISC */
#define HCCR_CMD_RELEASE		0x3000  /* release paused RISC */
#define HCCR_CMD_MASK_FBPE		0x4001	/* mask frame buffer parity error */
#define HCCR_CMD_SET_HOST_INT		0x5000  /* set host interrupt */
#define HCCR_CMD_CLEAR_HOST_INT		0x6000	/* clear host interrupt */
#define HCCR_CMD_CLEAR_RISC_INT		0x7000  /* clear RISC interrupt */

#define HCCR_SRAM_PARITY_ENABLE		0xA000  /* enable sram parity */
#define HCCR_FORCE_PARITY_ERROR		0xE000  /* force sram parity error*/

	/* Sram parity enable bits */

#define HCCR_SRAM_PARITY_BANK2		0x0400  /* R: bank two enable */
#define HCCR_SRAM_PARITY_BANK1		0x0200  /* R: bank one enable */
#define HCCR_SRAM_PARITY_BANK0		0x0100  /* R: bank zero enable */


        /* Status Defintions */

#define HCCR_RISC_PARITY_ERROR	0x0400  /* R: parity error detected */

#define HCCR_HOST_INT		000200  /* R: host interrupt set */
#define HCCR_RESET		000100  /* R: RISC reset in progress */
#define HCCR_PAUSE		000040  /* R: RISC paused */
#define HCCR_EXTERN_BP_ENABLE	000020  /* R: enable external breakpoint */
#define HCCR_BP1_ENABLE		000010  /* R: enable external breakpoint 1 */
#define HCCR_BP0_ENABLE		000004  /* R: enable external breakpoint 2 */
#define HCCR_ENEABLE_INT_BP	000002  /* R: enable interrupt on breakpoint */
#define HCCR_FORCE_PARITY	000001  /* R: force an sram parity errror  */



/****************************************************************
** Temporary definitions (DEBUG)
*/


#define READ_CMD_28 	0x28
#define WRITE_CMD_2A 	0x2a
#define SCSI_DISK_INFO 	0x25 /*read capacity */
#define SCSI_TAPE_INFO 	0xff /* not implemented */
#define SCSI_SET_ASYNC 	0xfe /* set sync via mailbox command */
#define SCSI_INFO 	0xfd /* do an info to see what is set */
#define INQUIRY_CMD	0x12



/********************************************************************
**
**                           Structures
*/

/*
** Host Adapter defaults.
*/

#define INITIATOR_HARD_ID		0
#define MBOX_DELAY_TIME       		5	/* target reset, lip reset delay, in seconds */
#define RETRY_COUNT           		4	/* login retries */
#define RETRY_DELAY           		15	/* time between retries, .1 secs */
#define MAX_QUEUE_DEPTH       		256	/* 256 commands */
#define DELAY_AFTER_RESET      		5	/* seconds */

/*
** FC Port related defaults.
*/
#ifdef _STANDALONE
#define EXECUTION_THROTTLE        16  
#else /* !_STANDALONE */
#define EXECUTION_THROTTLE        255
#endif  /* _STANDALONE  */

#define DEF_FRAME_SIZE            2048
#define DEFAULT_TIMEOUT           10 


/**********************************************************
**
**          PCI Service Information
*/

#define PCI_CONFIG          0xCF8
#define PCI_ENABLE          0x80

#define PCI_CONFIG_START        0xC000
#define PCI_CONFIG_LAST         0xCFFF

#define PCI_FUNCTION_ID         0xB100

/*
** PCI Function List.
*/
#define PCI_BIOS_PRESENT        0xB101
#define FIND_PCI_DEVICE         0xB102
#define FIND_PCI_CLASS_CODE     0xB103
#define GENERATE_SPECIAL_CYCLE  0xB106
#define READ_CONFIG_BYTE        0xB108
#define READ_CONFIG_WORD        0xB109
#define READ_CONFIG_DWORD       0xB10A
#define WRITE_CONFIG_BYTE       0xB10B
#define WRITE_CONFIG_WORD       0xB10C
#define WRITE_CONFIG_DWORD      0xB10D

/*
** PCI Return Code List.
*/
#define SUCCESSFUL         	0x00
#define FUNC_NOT_SUPPORTED      0x81
#define BAD_VENDOR_ID           0x83
#define DEVICE_NOT_FOUND        0x86
#define BAD_REGISTER_NUMBER     0x87



/**********************************************************
**
**          our PCI Information
*/

/*
** PCI Bus configuration space registers.
*/
typedef struct PCI_Config_Registers
{
    uint16_t Device_Id;
    uint16_t Vendor_Id;      /* 00 */

    uint16_t Status;
    uint16_t Command;        /* 04 */

    uint8_t  Class_Code[3];
    uint8_t  Rev_Id;         /* 08 */


    uint8_t  Bits;
    uint8_t  Header_Type;
    uint8_t  Latency_Timer;
    uint8_t  Cache_Line_Size;    /* 0C */


    uint32_t   IO_Base_Address;    /* 10 */

    uint32_t   Memory_Base_Address;    /* 14 */

    uint32_t   Not_Used1[6];       /* 18 - 2F */

    uint32_t   ROM_Base_Address;   /* 30 */

    uint32_t   Not_Used2[2];       /* 34 - 3B */

    uint8_t  Interrupt_Line;     /* 3C */
    uint8_t  Interrupt_Pin;
    uint8_t  Minimum_Grant;
    uint8_t  Maximum_Latency;

    uint16_t ISP_Config_1;       /* 40 */
    uint16_t Not_Used3;

} PCI_REG;


/*
** Configuration space offsets:
*/

#define CONF_VENDOR_ID_OFFSET           0x00
#define     QLOGIC_VENDOR_ID            0x1077

#define CONF_DEVICE_ID_OFFSET           0x02
#define		QLOGIC_2100		0x2100
#define		QLOGIC_2200		0x2200

#define CONF_COMMAND_OFFSET         	0x04
#define     CONF_COMMAND_VALUE          0x0156

/*
**  0x0156 =    IO Space Disabled
**          Memory Space Enabled
**          PCI Master Capable
**          Memory Write & Invalidate Enabled
**          Parity Error Response Enabled
**          SERR# Enabled
**          Back to Back Transfers Disabled
*/

#define CONF_IO_BASE_OFFSET         0x10
#define CONF_MEM_BASE_OFFSET        0x14
#define MEMORY_SPACE_INDICATOR_MASK 0x00000001
#define IO_BASE_MASK                0xF800
#define MEMORY_BASE_MASK            0xFFFFF800


#define CONF_INTERRUPT_LINE_OFFSET  0x3C


typedef struct
{
	int commands_issued;
	int responses_handled;	
	int errors;
} dev_stats;


/*
** Structure to return for IOCSTATS.
*/
typedef struct stats {
	int commands;            /* # of commands processed */
	int responses;           /* # of responses processed */
	int interrupts;          /* # of interrupts processed */
	int intrs_processed;     /* # of interrupts ACTUALLY processed */
	int responses_handled;   /* # IORB processed */
	int markers;         	 /* # of markers issued */
	dev_stats dev[QL_MAX_LOOP_IDS];
} stats, *ha_stats;


/*
** Structure to peek and poke risc memory.
*/
struct addr_data {
    uint16_t address;     /* word address in risc memory   */
    uint16_t data;        /* word data to write (if write) */
};


/*
 * macros associated with this structure	
 */
#define QLI_DEVFLAGS(p)		(p->qli_dev_flags)
#define QLI_OPENMUTEX(p)	(p->qli_open_mutex)
#define QLI_REFCOUNT(p)		(p->qli_ref_count)
#define QLI_TINFO(p)		(p->qli_tinfo)

/*
 * macros associated with the scsi lun info structure specific
 * to ql
 */

#define QL_DEVFLAGS(p)		QLI_DEVFLAGS(((qlfc_local_lun_info_t *)p))
#define QL_OPENMUTEX(p)		QLI_OPENMUTEX(((qlfc_local_lun_info_t *)p))
#define QL_LUNMUTEX(p)		QLI_LUNMUTEX(((qlfc_local_lun_info_t *)p))
#define QL_REFCOUNT(p)		QLI_REFCOUNT(((qlfc_local_lun_info_t *)p))
#define QL_TINFO(p)		QLI_TINFO(((qlfc_local_lun_info_t *)p))

#define DEV_FLAGS(p)		QL_DEVFLAGS(SLI_INFO(p))
#define OPEN_MUTEX(p)		QL_OPENMUTEX(SLI_INFO(p))
#define LUN_MUTEX(p)		QL_LUNMUTEX(SLI_INFO(p))
#define REF_COUNT(p)		QL_REFCOUNT(SLI_INFO(p))
#define TINFO(p)		QL_TINFO(SLI_INFO(p))

#define SCI_CTLR(p)		((QLFC_CONTROLLER *)(SCI_INFO(p)))
#define SLI_CTLR(p)		((QLFC_CONTROLLER *)(SCI_INFO(SLI_CTLR_INFO(p))))

#endif /* KERNEL */

/*
** Timeout values for the watchdog timer, error recovery, etc.
*/

#define AWOL_MIN_TIMEOUT		(10)			/* minimum timeout before target is awol */
#define MAILBOX_COMMAND_TIMEOUT		(20)
#define MAILBOX_COMMAND_LOGIN_TIMEOUT	(95)
#define	STEP_ABORT_DEVICE_TIMEOUT	(5)
#define STEP_RESET_TARGET_TIMEOUT	(10)
#define STEP_RESET_TARGETS_TIMEOUT	(10)
#define STEP_LIP_RESET_TARGET_TIMEOUT	(10)
#define STEP_LIP_WITH_LOGIN_TIMEOUT	(10)

/*
** Some miscellaneous configuration parameters.
*/

#define QL_RESET		1000
#define SCSI_INFO_BYTE		7
#define SCSI_CTQ_BIT		2

#define QLPRI			0	/* Doesn't matter, since pri isn't used anymore */

#define INITLOCK(l,n,s)		init_mutex(l, MUTEX_DEFAULT, n, s)
#define IOLOCK(l)		mutex_lock(&(l), QLPRI)
#define IOUNLOCK(l)		mutex_unlock(&(l))

#define QL_MUTEX_ENTER(ctlr)	IOLOCK(ctlr->res_lock), \
				IOLOCK(ctlr->req_lock)

#define QL_MUTEX_EXIT(ctlr)	IOUNLOCK(ctlr->res_lock), \
				IOUNLOCK(ctlr->req_lock)

#undef LOCK
#undef UNLOCK



#define QL_CPMSGS   if (showconfig) qlfc_cpmsg
#define QL_TPMSGS   if (showconfig) qlfc_tpmsg

#ifdef SN
#define TIMEOUT_SPL     plbase
#else /* !SN */
#define TIMEOUT_SPL     plhi
#endif /* !SN */

/* extern from master.d/qlfc_ */
extern int	qlfc_bus_reset_on_boot;
extern int	qlfc_allow_negotiation_on_cdroms;

/* extern from os/kopt.c */
extern char	arg_qlfc_hostid[];


#pragma pack(1)

/**************************************************************
*
*	FABRIC LOOP ADDRESS (LFA) COMMANDS and COMMON HEADERS
*
**************************************************************/

#define LFA_BASIC_HEADER_SZ		20	/* bytes */

#define	LFA_LINIT			0x7000
#define		LINIT_FORCE_FLOGI	1
#define		LINIT_NO_FLOGI		0
#define LFA_LPC				0x7100
#define	LFA_LSTS			0x7200

typedef struct {
	uint16_t	lfa_response_length;
	uint16_t	lfa_reserved1;
	uint32_t	lfa_response_buffer_addr_u;
	uint32_t	lfa_response_buffer_addr_l;
	uint16_t	lfa_subcommand_length;
	uint16_t	lfa_reserved2;
	uint32_t	lfa_loop_fabric_address;
	uint16_t	lfa_subcommand;
	uint16_t	lfa_reserved3;
} lfa_cmd_hdr_t;

typedef struct {
	uint32_t	lfa_response_code;	/* 0x0[12]000000 (rjt, acc) */
} lfa_rsp_hdr_t;

	/******************************************************
	*
	*	LFA_LINIT (loop initialize)
	*
	******************************************************/

	typedef struct {
		lfa_cmd_hdr_t	linit_cmd_hdr;
		uint16_t	linit_init_func;
		uint8_t		linit_lip4;
		uint8_t		linit_lip3;
		uint32_t	linit_fill;
	} lfa_linit_cmd_t;

	typedef struct {
		lfa_rsp_hdr_t	linit_rsp_hdr;
		uint8_t		linit_rsp_reserved[3];
		uint8_t		linit_status;
	} lfa_linit_rsp_t;

	/******************************************************
	*
	*	LFA_LPC (loop port control)
	*
	******************************************************/

	typedef struct {
		lfa_cmd_hdr_t	lpc_cmd_hdr;
		uint16_t	lpc_reserved1;
		uint16_t	lpc_port_control_function;

		/* watch out for byte swapping here, untested */

		uint8_t		lpc_lpb_alpa_bitmap_1;
		uint8_t		lpc_lpb_alpa_bitmap_2;
		uint8_t		lpc_lpb_alpa_bitmap_3;
		uint8_t		lpc_lpb_alpa_bitmap_4;
		uint8_t		lpc_lpb_alpa_bitmap_5;
		uint8_t		lpc_lpb_alpa_bitmap_6;
		uint8_t		lpc_lpb_alpa_bitmap_7;
		uint8_t		lpc_lpb_alpa_bitmap_8;

		uint8_t		lpc_lpb_alpa_bitmap_9;
		uint8_t		lpc_lpb_alpa_bitmap_10;
		uint8_t		lpc_lpb_alpa_bitmap_11;
		uint8_t		lpc_lpb_alpa_bitmap_12;
		uint8_t		lpc_lpb_alpa_bitmap_13;
		uint8_t		lpc_lpb_alpa_bitmap_14;
		uint8_t		lpc_lpb_alpa_bitmap_15;
		uint8_t		lpc_lpb_alpa_bitmap_16;

		uint8_t		lpc_lpe_alpa_bitmap_1;
		uint8_t		lpc_lpe_alpa_bitmap_2;
		uint8_t		lpc_lpe_alpa_bitmap_3;
		uint8_t		lpc_lpe_alpa_bitmap_4;
		uint8_t		lpc_lpe_alpa_bitmap_5;
		uint8_t		lpc_lpe_alpa_bitmap_6;
		uint8_t		lpc_lpe_alpa_bitmap_7;
		uint8_t		lpc_lpe_alpa_bitmap_8;

		uint8_t		lpc_lpe_alpa_bitmap_9;
		uint8_t		lpc_lpe_alpa_bitmap_10;
		uint8_t		lpc_lpe_alpa_bitmap_11;
		uint8_t		lpc_lpe_alpa_bitmap_12;
		uint8_t		lpc_lpe_alpa_bitmap_13;
		uint8_t		lpc_lpe_alpa_bitmap_14;
		uint8_t		lpc_lpe_alpa_bitmap_15;
		uint8_t		lpc_lpe_alpa_bitmap_16;
	} lfa_lpc_cmd_t;

	typedef struct {
		lfa_rsp_hdr_t	lpc_rsp_hdr;
		uint8_t		lpc_rsp_reserved[3];
		uint8_t		lpc_status;
	} lfa_lpc_rsp_t;

	/******************************************************
	*
	*	LFA_LSTS (loop status)
	*
	******************************************************/

	typedef struct {
		lfa_cmd_hdr_t	lsts_cmd_hdr;	/* no command specific data */
	} lfa_lsts_cmd_t;

	typedef struct {
		lfa_rsp_hdr_t	lsts_rsp_hdr;
		uint8_t		lsts_rsp_reserved;
		uint8_t		lsts_failed_receiver;
		uint8_t		lsts_fc_fla_compliance_level;
		uint8_t		lsts_loop_state;
		uint8_t		lsts_public_loop_bitmap[16];
		uint8_t		lsts_private_loop_bitmap[16];
		uint8_t		lsts_alpa_position_map[128];
	} lfa_lsts_rsp_t;

/**************************************************************
*
*	NAME SERVICE COMMANDS and COMMON HEADERS
*
**************************************************************/

#define SNS_GET_ALL_NEXT		0x100
#define	SNS_GET_PORT_NAME		0x112
#define	SNS_GET_NODE_NAME_1		0x113
#define	SNS_GET_CLASS_OF_SERVICE	0x114
#define SNS_GET_FC4_TYPES		0x117
#define	SNS_GET_PORT_TYPE		0x11a
#define	SNS_GET_PORT_ID_1		0x121
#define	SNS_GET_PORT_ID_2		0x131
#define	SNS_GET_PORT_ID_3		0x171
#define	SNS_GET_PORT_ID_AND_NODE_NAME	0x173	/* possibly simulated! */
#define	SNS_GET_PORT_ID_4		0x1a1
#define	SNS_REGISTER_PORT_NAME		0x212
#define	SNS_REGISTER_NODE_NAME		0x213
#define	SNS_REGISTER_CLASS_OF_SERVICE	0x214
#define	SNS_REGISTER_FC4_TYPES		0x217
#define	SNS_REGISTER_PORT_TYPE		0x21a
#define	SNS_REMOVE_ALL			0x300

#define	FL_PORT_ID			0xfffffe

#define	SNS_BASIC_HEADER_SZ		16

typedef struct {
	uint16_t	sns_response_length;			/* 00 - 01 */
	uint16_t	reserved1;				/* 02 - 03 */
	uint32_t	sns_response_buffer_addr_u;		/* 04 - 07 */
	uint32_t	sns_response_buffer_addr_l;		/* 08 - 0b */
	uint16_t	sns_subcommand_length;			/* 0c - 0d */
	uint16_t	reserved2;				/* 0e - 0f */
	uint16_t	sns_subcommand;				/* 10 - 11 */
	uint16_t	sns_response_limit;	/* uint32_ts */	/* 12 - 13 */
	uint32_t	reserved4;				/* 14 - 17 */
} sns_cmd_hdr_t;

typedef struct {
	uint8_t		ct_revision;		/* 0x01 */
	uint8_t		ct_n_port_id[3];	/* 0x00 */
	uint8_t		ct_fcs_type;		/* 0xfc */
	uint8_t		ct_fcs_subtype;		/* 0x02 */
	uint8_t		ct_options;		/* 0x00 */
	uint8_t		ct_reserved1;
	uint16_t	ct_response_code;	/* 0x800[1,2] (acc, rjt) */
	uint16_t	ct_residual;		/* 32 bit words */
	uint8_t		ct_reserved2;
	uint8_t		ct_reason_code;		/* from FC-GS-2 */
	uint8_t		ct_explanation_code;	/* from FC-GS-2 */
	uint8_t		ct_vendor_unique;
} sns_rsp_cthdr_t;

	
	/******************************************************
	*
	*	SNS_GET_ALL_NEXT
	*
	******************************************************/

	typedef struct {
		sns_cmd_hdr_t	gan_cmd_hdr;
		uint32_t	gan_port_id;			/* bits 23-0 */
	} sns_get_all_next_cmd_t;

	typedef struct {
		sns_rsp_cthdr_t	gan_rsp_hdr;
		uint8_t		gan_port_type;
		uint8_t		gan_port_id[3];
		uint64_t	gan_port_name;
		uint8_t		gan_symbolic_port_name_length;
		uint8_t		gan_symbolic_port_name[255];
		uint64_t	gan_node_name;
		uint8_t		gan_symbolic_node_name_length;
		uint8_t		gan_symbolic_node_name[255];
		uint64_t	gan_initial_process_associator;
		uint8_t		gan_ip_address[16];
		uint32_t	gan_service_class;
		uint8_t		gan_fc4_types[32];
	} sns_get_all_next_rsp_t;


	/*****************************************************
	*
	*	SNS_GET_PORT_NAME
	*
	*****************************************************/

	typedef struct {
		sns_cmd_hdr_t	gpn_cmd_hdr;
		uint32_t	gpn_port_id;	/* bits 23-0 */
	} sns_get_port_name_cmd_t;

	typedef struct {
		sns_rsp_cthdr_t	gpn_rsp_hdr;
		uint64_t	gpn_port_name;
	} sns_get_port_name_rsp_t;


	/*****************************************************
	*
	*	SNS_GET_NODE_NAME_1
	*
	*****************************************************/

	typedef struct {
		sns_cmd_hdr_t	gnn1_cmd_hdr;
		uint32_t	gnn1_port_id;	/* bits 23-0 */
	} sns_get_node_name_1_cmd_t;

	typedef struct {
		sns_rsp_cthdr_t	gnn1_rsp_hdr;
		uint64_t	gnn1_node_name;
	} sns_get_node_name_1_rsp_t;


	/*****************************************************
	*
	*	SNS_GET_CLASS_OF_SERVICE
	*
	*****************************************************/

	typedef struct {
		sns_cmd_hdr_t	gcos_cmd_hdr;
		uint32_t	gcos_port_id;	/* bits 23-0 */
	} sns_get_class_of_service_cmd_t;

	typedef struct {
		sns_rsp_cthdr_t	gcos_rsp_hdr;
		uint32_t	gcos_class_of_service;
	} sns_get_class_of_service_rsp_t;


	/*****************************************************
	*
	*	SNS_GET_FC4_TYPES
	*
	*****************************************************/

	typedef struct {
		sns_cmd_hdr_t	gft_cmd_hdr;
		uint32_t	gft_port_id;	/* bits 23-0 */
	} sns_get_fc4_types_cmd_t;

	typedef struct {
		sns_rsp_cthdr_t	gft_rsp_hdr;
		uint32_t	gft_fc4_type[8];	/* bit masks */
	} sns_get_fc4_types_rsp_t;


	/*****************************************************
	*
	*	SNS_GET_PORT_TYPE
	*
	*****************************************************/

	typedef struct {
		sns_cmd_hdr_t	gpt_cmd_hdr;
		uint32_t	gpt_port_id;	/* bits 23-0 */
	} sns_get_port_type_cmd_t;


	typedef struct {
		sns_rsp_cthdr_t	gpt_rsp_hdr;
		uint8_t		gpt_port_type;
		uint8_t		gpt_reserved[3];
	} sns_get_port_type_rsp_t;


	/*****************************************************
	*
	*	SNS_GET_PORT_ID_1
	*
	*****************************************************/

	typedef struct {
		sns_cmd_hdr_t	gpid1_cmd_hdr;
		uint64_t	gpid1_port_name;
	} sns_get_port_id1_cmd_t;


	typedef struct {
		sns_rsp_cthdr_t	gpid1_rsp_hdr;
		uint8_t		gpid1_reserved;
		uint8_t		gpid1_port_id[3];
	} sns_get_port_id1_rsp_t;


	/*****************************************************
	*
	*	SNS_GET_PORT_ID_2
	*
	*****************************************************/

	#define	GPID2_MAX_PORTS	256

	typedef struct {
		sns_cmd_hdr_t	gpid2_cmd_hdr;
		uint64_t	gpid2_node_name;
	} sns_get_port_id2_cmd_t;

	typedef struct {
		sns_rsp_cthdr_t	gpid2_rsp_hdr;
		struct {
			uint8_t	gpid2_control_byte;
			uint8_t	gpid2_port_id[3];
		} 		gpid2_port[GPID2_MAX_PORTS];	/* max 512 ports per node name */
	} sns_get_port_id2_rsp_t;


	/*****************************************************
	*
	*	SNS_GET_PORT_ID_3
	*
	*****************************************************/

	#define	GPID3_MAX_PORTS	GPID2_MAX_PORTS

	typedef struct {
		sns_cmd_hdr_t	gpid3_cmd_hdr;
		uint32_t	gpid3_fc4_protocol;
	} sns_get_port_id3_cmd_t;

	typedef struct {
		sns_rsp_cthdr_t	gpid3_rsp_hdr;
		struct {
			uint8_t	gpid3_control_byte;
			uint8_t	gpid3_port_id[3];
		} 		gpid3_port[GPID3_MAX_PORTS];	/* max 512 ports per node name */
	} sns_get_port_id3_rsp_t;


	/*****************************************************
	*
	*	SNS_GET_PORT_ID_AND_NODE_NAME
	*
	*****************************************************/

	#define	GPIDNN_MAX_PORTS	GPID3_MAX_PORTS

	typedef struct {
		sns_cmd_hdr_t	gpidnn_cmd_hdr;
		uint32_t	gpidnn_fc4_protocol;
	} sns_get_port_id_node_name_cmd_t;

	typedef struct {
		sns_rsp_cthdr_t	gpidnn_rsp_hdr;
		struct {
			uint8_t		gpidnn_control_byte;
			uint8_t		gpidnn_port_id[3];
			uint32_t	gpidnn_reserved;
			uint64_t	gpidnn_node_name;
		} 		gpidnn_port[GPIDNN_MAX_PORTS];	/* max 512 ports per node name */
	} sns_get_port_id_node_name_rsp_t;

	/*****************************************************
	*
	*	SNS_GET_PORT_ID_AND_PORT_NAME (local hack!)
	*
	*****************************************************/

	#define	GPIDPN_MAX_PORTS	GPID3_MAX_PORTS

	typedef struct {
		sns_cmd_hdr_t	gpidpn_cmd_hdr;
		uint32_t	gpidpn_fc4_protocol;
	} sns_get_port_id_port_name_cmd_t;

	typedef struct {
		sns_rsp_cthdr_t	gpidpn_rsp_hdr;
		struct {
			uint8_t		gpidpn_control_byte;
			uint8_t		gpidpn_port_id[3];
			uint32_t	gpidpn_reserved;
			uint64_t	gpidpn_port_name;
		} 		gpidpn_port[GPIDPN_MAX_PORTS];	/* max 512 ports per node name */
	} sns_get_port_id_port_name_rsp_t;


	/*****************************************************
	*
	*	SNS_GET_PORT_ID_4
	*
	*****************************************************/

	#define	GPID4_MAX_PORTS	GPID3_MAX_PORTS

	typedef struct {
		sns_cmd_hdr_t	gpid4_cmd_hdr;
		uint32_t	gpid4_port_type;	/* bits 7-0 */
	} sns_get_port_id4_cmd_t;

	typedef struct {
		sns_rsp_cthdr_t	gpid4_rsp_hdr;
		struct {
			uint8_t	gpid4_control_byte;
			uint8_t	gpid4_port_id[3];
		} 		gpid4_port[GPID4_MAX_PORTS];	/* max 512 ports per node name */
	} sns_get_port_id4_rsp_t;


	/*****************************************************
	*
	*	SNS_REGISTER_PORT_NAME
	*
	*****************************************************/

	typedef struct {
		sns_cmd_hdr_t	rpn_cmd_hdr;
		uint32_t	rpn_port_id;	/* bits 23-0 */
		uint64_t	rpn_port_name;
	} sns_regsiter_port_name_cmd_t;

	typedef struct {
		sns_rsp_cthdr_t	rpn_rsp_hdr;
	} sns_register_port_name_rsp_t;


	/*****************************************************
	*
	*	SNS_REGISTER_NODE_NAME
	*
	*****************************************************/

	typedef struct {
		sns_cmd_hdr_t	rnn_cmd_hdr;
		uint32_t	rnn_port_id;	/* bits 23-0 */
		uint64_t	rnn_node_name;
	} sns_regsiter_node_name_cmd_t;

	typedef struct {
		sns_rsp_cthdr_t	rnn_rsp_hdr;
	} sns_register_node_name_rsp_t;


	/*****************************************************
	*
	*	SNS_REGISTER_CLASS_OF_SERVICE
	*
	*****************************************************/

	typedef struct {
		sns_cmd_hdr_t	rcos_cmd_hdr;
		uint32_t	rcos_reserved;
		uint32_t	rcos_port_id;	/* bits 23-0 */
		uint32_t	rcos_class_of_service;
	} sns_regsiter_cos_cmd_t;

	typedef struct {
		sns_rsp_cthdr_t	rcos_rsp_hdr;
	} sns_register_cos_rsp_t;


	/*****************************************************
	*
	*	SNS_REGISTER_FC4_TYPES
	*
	*****************************************************/

	typedef struct {
		sns_cmd_hdr_t	rft_cmd_hdr;
		uint32_t	rft_port_id;	/* bits 23-0 */
		uint32_t	rft_fc4_type[8];
	} sns_regsiter_fc4_types_cmd_t;

	typedef struct {
		sns_rsp_cthdr_t	rft_rsp_hdr;
	} sns_register_fc4_types_rsp_t;


	/*****************************************************
	*
	*	SNS_REGISTER_PORT_TYPE
	*
	*****************************************************/

	typedef struct {
		sns_cmd_hdr_t	rpt_cmd_hdr;
		uint32_t	rpt_port_id;	/* bits 23-0 */
		uint32_t	rpt_port_type;
	} sns_regsiter_port_type_cmd_t;

	typedef struct {
		sns_rsp_cthdr_t	rft_rsp_hdr;
	} sns_register_port_type_rsp_t;


	/*****************************************************
	*
	*	SNS_REMOVE_ALL
	*
	*****************************************************/

	typedef struct {
		sns_cmd_hdr_t	ra_cmd_hdr;
		uint32_t	ra_port_id;	/* bits 23-0 */
	} sns_remove_all_cmd_t;

	typedef struct {
		sns_rsp_cthdr_t	ra_rsp_hdr;
	} sns_remove_all_rsp_t;






/*****************************************************
*
*	PORT_DATABASE
*
*****************************************************/

typedef struct {			/* requires qlfc_swap64() */
	uint8_t		pd_options;
	uint8_t		pd_control;

	uint8_t		pd_master_state;
	uint8_t		pd_slave_state;

	uint32_t	pd_port_id;

	uint32_t	pd_M_hard_address;

	uint64_t	pd_node_name;

	uint64_t	pd_port_name;

	uint16_t	pd_M_execution_throttle;

	uint16_t	pd_M_execution_count;

	uint8_t		pd_retry_count;	/* swapped to avoid _M_ definition */
	uint8_t		reserved;	/* swapped to avoid _M_ definition */

	uint16_t	pd_M_resource_allocation;

	uint16_t	pd_M_current_allocation;

	uint16_t	pd_queue_head;
	uint16_t	pd_queue_tail;
	uint16_t	pd_xmit_exec_list_next;
	uint16_t	pd_xmit_exec_list_prev;
	uint16_t	pd_M_common_features;
	uint16_t	pd_M_total_concurrent_sequences;
	uint16_t	pd_M_rel_offset_info_catagory_flags;
	uint16_t	pd_M_initiator_receipt_control_flags;
	uint16_t	pd_M_receive_data_size;
	uint16_t	pd_M_num_concurrent_sequences;
	uint16_t	pd_M_num_open_sequences;

	uint16_t	pd_M_lun_abort_flags;
	uint16_t	pd_M_lun_stop_flags;

	uint16_t	pd_stop_queue_head;
	uint16_t	pd_stop_queue_tail;
	uint16_t	pd_M_port_retry_timer;
	uint16_t	pd_M_next_sequence_id;
	uint16_t	pd_M_frame_count;

	uint16_t	pd_M_prli_payload_length;
	uint16_t	pd_M_w0_prli_service_parameters;
	uint16_t	pd_M_w3_prli_service_parameters;
	uint16_t	pd_M_loop_id;
	uint16_t	pd_extended_lun_info_pointer;
	uint16_t	pd_extended_lun_info_stop_pointer;

	uint16_t	pd_FILL;	/* round to multiple of uint64_t size */

} portDatabase_t;

#define PORT_NODE_NAME_LIST_ENTRIES	256

struct	port_node_name_list_entry {
	uint64_t	node_name;
	uint16_t	loop_id;
};
typedef struct port_node_name_list_entry port_node_name_list_entry_t;

struct	port_node_name_list {
	port_node_name_list_entry_t entry[PORT_NODE_NAME_LIST_ENTRIES];
};

typedef struct port_node_name_list port_node_name_list_t;

#pragma pack(0)


/****************************************************************/
/*								*/
/* Function Prototypes						*/
/*								*/
/****************************************************************/

/* Primary entry points */

#if !defined(QLFC_IDBG) && !defined(QLFC_FIRMWARE)

int			qlfc_alloc(vertex_hdl_t, int, void (*)());
void			qlfc_command(struct scsi_request *req);
void			qlfc_free(vertex_hdl_t, void (*)());
int			qlfc_dump(vertex_hdl_t);
struct scsi_target_info	*qlfc_info(vertex_hdl_t ql_lun_vhdl);
int			qlfc_ioctl(vertex_hdl_t qlfc_ctlr_vhdl, uint cmd, scsi_ha_op_t *op);
int			qlfc_abort(scsi_request_t *scsi_request);
#ifdef IP30
int			qlfc_reset(vertex_hdl_t qlfc_ctlr_vhdl);
#endif
#ifdef IP32
int			qlfc_intr(eframe_t *qlfc_foo, QLFC_CONTROLLER *);
#elif defined(SN)||defined(IP30)
int			qlfc_intr(QLFC_CONTROLLER *);
#endif
void			qlfc_init(void);
int			qlfc_attach(vertex_hdl_t);
int			qlfc_detach(struct pci_record *, edt_t *, __int32_t);
int			qlfc_error(struct pci_record *, edt_t *, __int32_t);
void			qlfc_halt(void);

/* Locally used routines */
static STATUS			qlfc_send_get_port_name (QLFC_CONTROLLER *, uint32_t, sns_get_port_name_rsp_t *);
static STATUS			qlfc_send_get_node_name (QLFC_CONTROLLER *, uint32_t, sns_get_node_name_1_rsp_t *);
static int			qlfc_drain_io (QLFC_CONTROLLER *ctlr);
static STATUS			qlfc_get_port_name (QLFC_CONTROLLER *ctlr, unsigned loop_id, uint64_t *port_name, uint16_t *mbox_regs);
static void			qlfc_start_target_queue (QLFC_CONTROLLER *ctlr, int loop_id);
static STATUS			qlfc_login_fabric_port (QLFC_CONTROLLER *, int, uint32_t, uint16_t *);
static STATUS			qlfc_login_loop_port (QLFC_CONTROLLER *, int, int);
static STATUS			qlfc_demon_msg_enqueue (QLFC_CONTROLLER *ctlr, int msg, __psunsigned_t arg);
static void			qlfc_stop_target_queue (QLFC_CONTROLLER *ctlr, int loop_id);
static STATUS			qlfc_logout_fabric_port (QLFC_CONTROLLER *ctlr, int loop_id);
static scsi_target_info_t *	qlfc_scsi_target_info_init(void);
static qlfc_local_lun_info_t *	qlfc_local_lun_info_init(void);
static qlfc_local_targ_info_t *	qlfc_local_targ_info_init(vertex_hdl_t);
static vertex_hdl_t		qlfc_device_add(QLFC_CONTROLLER *, int, uint64_t, uint64_t, int);
static void			qlfc_device_remove(QLFC_CONTROLLER *, int, uint64_t, uint64_t, int);
QLFC_CONTROLLER *		qlfc_ctlr_info_from_ctlr_get(vertex_hdl_t);
static qlfc_local_targ_info_t *	qlfc_target_info_from_lun_vhdl (vertex_hdl_t);
static QLFC_CONTROLLER *	qlfc_ctlr_from_lun_vhdl_get(vertex_hdl_t);
static scsi_target_info_t *	qlfc_lun_target_info_get(vertex_hdl_t);
#if 0
static int			qlfcInit(vertex_hdl_t, vertex_hdl_t, PCI_REG *, CONTROLLER_REGS *);
static int			qlfcInitBoard(vertex_hdl_t, vertex_hdl_t, PCI_REG *, CONTROLLER_REGS *);
#endif
static int			qlfcInitIsp(QLFC_CONTROLLER *);
static void			qlfc_enable_intrs(QLFC_CONTROLLER *);
#ifdef QL_SRAM_PARITY
static void			qlfc_enable_sram_parity(QLFC_CONTROLLER *, int);
#endif
static int			qlfc_extra_response_space(void);
static int			qlfc_extra_request_space(void);
static void			qlfc_service_interrupt(QLFC_CONTROLLER *, uint16_t);
static void			qlfc_notify_responses(QLFC_CONTROLLER *, scsi_request_t *);
static void			qlfc_start_scsi(QLFC_CONTROLLER *);
static void			qlfc_queue_space(QLFC_CONTROLLER *, CONTROLLER_REGS *);
static int			qlfc_mbox_dma_cmd_rsp (QLFC_CONTROLLER *, uint16_t *, uint8_t, uint8_t,
					uint16_t, uint16_t,
					uint8_t *, int,
					uint8_t *, int);
static int			qlfc_mbox_dma_rsp (QLFC_CONTROLLER *, uint16_t *, uint8_t, uint8_t,
					uint16_t, uint16_t,
					uint8_t *, int);
static int			qlfc_mbox_dma_cmd (QLFC_CONTROLLER *, uint16_t *, uint8_t, uint8_t,
					uint16_t, uint16_t,
					uint8_t *, int);
static int			qlfc_do_mbox_cmd(QLFC_CONTROLLER *, uint16_t *, uint8_t, uint8_t,
					    uint16_t, uint16_t, uint16_t, uint16_t,
					    uint16_t, uint16_t, uint16_t, uint16_t);
static int			qlfc_mbox_cmd(QLFC_CONTROLLER *, uint16_t *, uint8_t, uint8_t,
					    uint16_t, uint16_t, uint16_t, uint16_t,
					    uint16_t, uint16_t, uint16_t, uint16_t);
static void			qlfc_done(scsi_request_t *);
static void			qlfc_dump_controller ( QLFC_CONTROLLER *pDevExt, uint16_t code);
static int			qlfc_stop_risc (QLFC_CONTROLLER *);
static int			qlfc_do_lip(vertex_hdl_t);
static int			qlfc_do_reset(vertex_hdl_t);
static scsi_request_t * 	qlfc_deque_sr (QLFC_CONTROLLER *, int );
static void 			qlfc_queue_sr (QLFC_CONTROLLER *, scsi_request_t *);
static void			qlfc_swap16 (uint8_t *, int);
static void			qlfc_swap32 (uint8_t *, int);
static void			qlfc_set_defaults(QLFC_CONTROLLER *);
#ifdef QL_STATS
static void			add_deltatime(struct timeval *, struct timeval *, struct timeval *);
#endif
static void			scan_bus(vertex_hdl_t);
static void			kill_quiesce(QLFC_CONTROLLER *);
static int			qlfc_load_firmware (QLFC_CONTROLLER *);
static int			qlfc_reset_interface(QLFC_CONTROLLER *, int);
static void			qlfc_flush_queue(QLFC_CONTROLLER *, uint, int);
static void			qlfc_flush_active_requests(QLFC_CONTROLLER *, uint, int);
static void			qlfc_watchdog_timer(QLFC_CONTROLLER *);
static void			qlfc_service_mbox_interrupt(QLFC_CONTROLLER *);
static int			qlfc_probe_bus(QLFC_CONTROLLER *);
static void			qlfc_cpmsg(dev_t, char *, ...);
static void			qlfc_tpmsg(dev_t, uint8_t, char *, ...);
static void			qlfc_lpmsg(dev_t, uint8_t, uint8_t, char *, ...);
static void			DBG(int level, char *format, ...);
static char *			qlfc_completion_status_msg(uint completion_status);
#ifdef SENSE_PRINT
static void			qlfc_sensepr(vertex_hdl_t, int, int, int, int);
#endif
#ifdef BRIDGE_B_DATACORR_WAR
int				qlfc_bridge_rev_b_war(vertex_hdl_t);
#endif

static void nv_delay(void);
static void write_nvram( QLFC_CONTROLLER *ctlr, uint8_t addr, uint16_t data );
static uint16_t read_nvram( QLFC_CONTROLLER *ctlr, uint8_t addr );
static void nvram_rd( QLFC_CONTROLLER *ctlr );
static void nv_rdy( QLFC_CONTROLLER *ctlr );
static void nv_cmd( QLFC_CONTROLLER *ctlr, uint8_t count, uint32_t ww0 );
static void nv_deselect( QLFC_CONTROLLER *ctlr );
static void nv_write( QLFC_CONTROLLER *ctlr, uint16_t w0 );

#endif
