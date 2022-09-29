/**************************************************************************
 *									  *
 * 		 Copyright (C) 1989, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident "$Revision: 1.38 $"
#ifndef _XL_H_
#define _XL_H_

/******************************************************************************
DEFINE: misc_defs.h - miscellaneous definitions 
*******************************************************************************
DESCRIPTION:
         This file defines miscellaneous constants for code compatability
    and readability.

******************************************************************************/


/* Bit Definitions. */

#define BIT0		0x01
#define BIT1		0x02
#define BIT2		0x04
#define BIT3		0x08
#define BIT4		0x10
#define BIT5		0x20
#define BIT6		0x40
#define BIT7		0x80

#define B_RAW		B_MAP		/* Flag indicating if a buf is Raw */

/******************************************************************************
DEFINE: config.h - configurable constants 
*******************************************************************************
DESCRIPTION:
         This file defines the driver's configurable constants.  These
    constants  determine the  number of IOPBs and bufs the driver will
    have  available  for use,  plus the number of tables per type that
    will be allocated.

******************************************************************************/

#define	BUFS	10    /* Number of Bufs Driver will use */

#define POLLED		0	/* Interrupt level for polled commands	*/

/* Silicon Graphics specific constants:		*/

#define VOL_ADR		0	/* Logical block number for volume header */
				/* cyl = 0, head = 0, sector = 0	  */

/* The following indicate the possible states of a unit as specified in */
/* the Unit Table field u_status.					*/

#define NOT_ATT		0	/* Unit is not attached			*/
#define OPENED		0x01	/* Unit is open 			*/
#define ATTACHED	0x02	/* Unit has been attached		*/
#define VH_VALID	0x04	/* Unit has a valid volume header	*/

#define LNKLST		3	/* Shift count for link list length in IOPB */
#define NSCAT		64	/* Max number of S/G elements		*/
#define LSCAT           32      /* Number of S/G entries per list       */
#define NXTSGCNT        0       /* S/G entry with next count field      */


/******************************************************************************
DEFINE: constants.h - non-configurable constants 
*******************************************************************************
DESCRIPTION:
         This file includes the definition of the constants that are
    controller-IOPB  specific  and  driver  data structure specific. 
    The names for IOPB fields are  assigned in this file, along with
    the bit definitions  within a  field.  The values to be assigned
    to the different  IOPB  fields are defined  as determined by the
    754 controller.  The constants for manipulating the controller's
    registers are also declared in this file.

******************************************************************************/

              /* GENERIC DISK DRIVE STATUS */

/* IOPB COMMAND BYTE. */

#define SGM            BIT4          /* Scatter/Gather Enable. */
#define CHEN           BIT5          /* Chain Enable.          */
#define DONE_BIT       BIT6          /* Done Bit.              */
#define ERRS           BIT7          /* Error Summary Bit.     */

/* DRIVE PARAMETERS MODIFICATION - BYTE 6. */

#define EC32           BIT4          /* 48/32 Bit ECC.        */
#define SSF            BIT5          /* Soft sectored format. */
#define ESD            BIT6          /* Embedded Servo Drive. */
#define AFE            BIT7          /* Alternate Field Enable. */

/* CONTROLLER PARAMETERS MODIFICATION - BYTE 8. */

#define AIOR          (BIT0 | BIT1)  /* AIO Response Time.            */
#define NPRM           BIT2          /* Non-privilaged Register Mode. */
#define EDT            BIT3          /* Enable DMA Timeout.           */
#define ICS            BIT4          /* IOPB Checksum Enable.         */
#define DACF           BIT5          /* Disable ACFAIL.               */
#define TMOD           BIT6          /* Transfer Mode.                */
#define AUD            BIT7          /* Auto Update.                  */

/* CONTROLLER PARAMETERS MODIFICATION - BYTE 9. */

#define ROR            BIT4          /* Release On Request. */
#define TDT           (BIT6 | BIT7)  /* Throttle Dead Time. */

/* CONTROLLER PARAMETERS MODIFICATION - BYTE A. */

#define ECCM          (BIT0 | BIT1)  /* ECC Mode.                */
#define RBC            BIT2          /* Retry Before Correction. */
#define ASR            BIT4          /* Auto Seek Retry.         */
#define OVS            BIT7          /* Overlap Seek Enable.     */

#define ECC_M0         0             /* ECC Mode 0 */
#define ECC_M1         1             /* ECC Mode 1 */
#define ECC_M2         2             /* ECC Mode 2 */
   
/* CONTROL/STATUS REGISTER. */

#define CLRBS    BIT0    /* (W) Clear Register Busy Semaphore.  */ 
#define RBS      BIT0    /* (R) Register Busy Semaphore.        */
#define CLRIO    BIT1    /* (W) Clear Remove IOPB Bit.          */
#define RIO      BIT1    /* (R) Remove IOPB Status.             */
#define AIO      BIT2    /* (W) Add IOPB.                       */
#define AIOP     BIT2    /* (R) Add IOPB Pending Status.        */
#define RESET    BIT3    /* (W) Controller Reset.               */
#define RESETA   BIT3    /* (R) Controller Reset Active.        */
#define MM       BIT5    /* (W) Maintenance Mode Bit.           */
#define MMA      BIT5    /* (R) Maintenance Mode Active Status. */
#define FERR     BIT6    /* (R) Fatal Error Status.             */
#define BUSY     BIT7    /* (R) Ctlr Busy Processng IOPB.       */


/* IOPB RELATED CONSTANTS */

#define	IOPB_SIZE      0x1d

#define COMMAND        0x00          /* Command codes.                 */
#define COMPCODE       0x01          /* Completion codes.              */
#define DISKSTAT       0x02          /* Disk status.                   */
#define STAT3          0x03          /* Status 3 byte.                 */
#define SUBFUNC        0x04          /* Subfunction byte.              */
#define UNIT           0x05          /* Unit number.                   */
#define LEVEL          0x06          /* Interrupt level.               */
#define DRV_PARM       0x06          /* Drive parameters modification. */
#define INTERLEAVE     0x06          /* Format parameters interleave.  */
#define VECTOR         0x07          /* Interrupt vector.              */
#define COUNTH         0x08          /* Byte count high.               */
#define CTLRPAR1       0x08          /* Controller parameters 1.       */
#define SECTS_LH       0x08          /* Sectors on last head.          */
#define FLD_1          0x08          /* Format parameters field 1.     */
#define COUNTL         0x09          /* Byte count low.                */
#define CTLRPAR2       0x09          /* Controller parameters 2.       */
#define HDOFFSET       0x09          /* Head offset.                   */
#define FLD_2          0x09          /* Format parameters field 2.     */
#define CYLH           0x0a          /* Cylinder number high.          */
#define CTLRPAR3       0x0a          /* Controller parameters 3.       */
#define FLD_3          0x0a          /* Format parameters field 3.     */
#define CYLL           0x0b          /* Cylinder number low.           */
#define THROTTLE       0x0b          /* Controller parms throttle.     */
#define FLD_4          0x0b          /* Format parameters field 4.     */
#define HEAD           0x0c          /* Head number.                   */
#define FLD_5H         0x0c          /* Format parameters field 5 Hi.  */
#define FW_REL         0x0c          /* Firmware Revision Number       */
#define AUTOSCT	       0x0c	     /* Auto configure sector size (word) */
#define SECTOR         0x0d          /* Sector number.                 */
#define FLD_5L         0x0d          /* Format parameters field 5 Lo.  */
#define DATA_MOD       0x0e          /* Data address modifier.         */
#define ACT_SECTORS    0x0e          /* Actual no. of sectors/track.   */
#define CTLR_TYPE      0x0e          /* Controller Type                */
#define FLD_12         0x0e          /* Format parameters field 12.    */
#define IOPB_MOD       0x0f          /* IOPB address modifier.         */
#define PNUMH          0x10          /* Eprom Part Number-High Byte    */
#define DATA1          0x10          /* Data address byte 1.           */
#define FLD_6          0x10          /* Format parameters field 6.     */
#define STATUSH	       0x10	     /* ESDI command status.	       */
#define PNUML          0x11          /* Eprom Part Number-Low Byte     */
#define DATA2          0x11          /* Data address byte 2.           */
#define FLD_7          0x11          /* Format parameters field 7.     */
#define DATA3          0x12          /* Data address byte 3.           */
#define FLD_5AH        0x12          /* Format parameters field 5 Alt Hi*/
#define REVISION       0x12          /* Revision number of controller.  */
#define ESDI_CMD       0x12          /* ESDI command code. 		*/
#define DATA4          0x13          /* Data address byte 4.           */
#define FLD_5AL        0x13          /* Format parameters field 5 Alt Lo*/
#define SUBREV         0x13          /* Sub-revision number of ctlr.   */
#define NIOPB1         0x14          /* Next IOPB address byte 1.      */
#define NIOPB2         0x15          /* Next Iopb address Byte 2.      */
#define NIOPB3         0x16          /* Next Iopb address Byte 3.      */
#define NIOPB4         0x17          /* Next Iopb address Byte 4.      */
#define CHECKH         0x18          /* Checksum high byte.            */
#define CHECKL         0x19          /* Checksum low byte.             */
#define ECCPH          0x1a          /* ECC pattern word high.         */
#define ECCPL          0x1b          /* ECC pattern word low.          */
#define ECCOH          0x1c          /* ECC offset word high.          */
#define ECCOL          0x1d          /* ECC offset word low.           */


       /* 754 CONTROLLER DISK COMMANDS AND SUBFUNCTIONS */

                  /* NO OPERATION */

#define NOP             0x0

                    /* WRITE */

#define WRITE           0x1

                    /* READ */

#define READ            0x2

                    /* SEEK */

#define SEEK            0x3

#define REPORT          0x00  /* Report Current Address */
#define SEEK_REPORT     0x01  /* Seek & Report Current Address */
#define SEEK_START      0x02  /* Seek Start */

                    /* RESET */

#define DRIVE_RESET     0x4   /* Drive Reset */

               /* WRITE PARAMETERS */

#define WRITE_PAR       0x5

#define W_CONTROLLER    0x00  /* Write Control Parameters */
#define W_DRIVE         0x80  /* Write Drive Parameters */
#define W_FORMAT        0x81  /* Write Drive Format Parameters */
#define W_CONFIG        0xb0  /* ESDI - Write Drive Configuration */
#define W_AUTOCNF       0xb2  /* ESDI - Auto Configure */

               /* READ PARAMETERS */

#define READ_PAR        0x6

#define R_CONTROLLER    0x00  /* Read Control Parameters */
#define R_DRIVE         0x80  /* Read Drive Parameters */
#define R_FORMAT        0x81  /* Read Controller Format Parameters */
#define R_STATUS_SMD    0xa0  /* SMD - Read Drive Status Extended */
#define R_STATUS_ESDI   0xb0  /* ESDI - Read Drive Status Extended */
#define R_CONFIG        0xb1  /* ESDI - Read drive configuration */

               /* WRITE/READ PARAMETERS */
        /* Used when direction doesn't matter */

#define CTLRP           0x00  /* Control Parameters */
#define DRV             0x80  /* Drive Parameters */
#define MEDIA           0x81  /* Media Format Parameters */

               /* EXTENDED WRITE */

#define EXT_WRITE       0x7

#define W_TRK_HDR       0x80  /* Write Track Headers */
/* #define W_FORMAT     0x81  Write Format is already defined in Write Parms */
#define W_HDE           0x82  /* Write Hdr, Hdr ECC, Data & Data ECC */
#define W_DEF_SMD       0xa0  /* SMD - Write Defect Map */
#define W_DEF_EXT_SMD   0xa1  /* SMD - Write Defect Map extended */
#define W_DEF_ESDI      0xb0  /* ESDI - Write Defect Map */

               /* EXTENDED READ */

#define EXT_READ        0x8

#define R_TRK_HDR       0x80  /* Read Track Headers */
#define RW_CHECK        0x81  /* Read/Write Check Data (Verify) */
#define R_HDE           0x82  /* Read Hdr, Hdr ECC, Data & Data ECC */
#define R_DEF_SMD       0xa0  /* SMD - Read Defect Map */
#define R_DEF_EXT_SMD   0xa1  /* SMD - Read Defect Map extended */
#define R_DEF_ESDI      0xb0  /* ESDI - Read Defect Map */
#define R_DEF_IPI	0xc4  /* IPI - Read Defect Map */

		/* PASSTHROUGH */
#define XL_PASSTHRU	0xa

#define XL_READFORMAT	0x42
#define XL_READCONFIG	0x41

               /* SEND OPTIONAL */

#define SEND_OPT        0xc 

/*  COMPLETION CODE ERROR TYPES - UPPER BYTE MASKS */

#define STATUS_ONLY          0x0   /* 0x00 - 0x0F */
#define PROGRAMMING_ERROR    0x1   /* 0x10 - 0x1F */
#define NO_RETRY             0x2 
#define SOFT_ERROR	     0x3
#define RETRY     	     0x4
#define RESET_RETRY	     0x6
#define FATAL_HW	     0x7
#define MISC_ERROR	     0x8
#define INTERVENTION	     0x9
#define CTLR_FATAL_ERROR     0xd   /* 0xD0 - 0xDF */

          /*  SIGNIFICANT COMPLETION CODES FOR THE DRIVER */

#define SW_TIMEOUT	    0xD0 /* Driver Timeouted the Iopb */
#define SW_HW_FATAL	    0xD1 /* Fatal Hardware Error caused Task to Abort */

                     /* HEXIDECIMAL MASKS */

#define CYL_MASK	0xffff0000 /* Used in conjunction with Disk */
#define HD_MASK		0x0000ff00 /* Addresses packed into 32 Bit  */
#define SEC_MASK	0x000000ff /* Integers                      */

#define HI_NIBBLE	0xf0
#define LO_NIBBLE	0x0f

            /* SOFTWARE TABLE FEATURES CONSTANTS */

#define	DEBUG_MSG	BIT0    /* Enables More Extensive Error Message */
#define	RETRY_MSG	BIT1    /* Enables Retry Message */
#define STATUS_MSG	BIT2    /* Enables Status Only Error Message */
#define BIG_BOOT_MSG	BIT3    /* Enables More Extensive Boot Message */
#define DISABLE_MSG	BIT4    /* Disables all Run-Time Error Messages */
#define QUEUE_MSG	BIT7    /* Enables Warning Messages from Queueing Fns */

                   /* MISC. CONSTANTS */

#define SEC_HDR_SIZE	0x4  /* No. of Bytes in a Sector's Track Header */
#define DEFECT_MAP_SIZE	0x18 /* No. of Bytes in a Track's Defect Map */
#define REMAP_SECTOR    0xcc
#define SPARE_SECTOR    0xdd
#define BAD_SECTOR      0xee

typedef struct xylhbuf {
	sv_t	b_bufsem;		/* To wait for a free buffer */
	lock_t	b_splock;		/* Locks the buf en_q, de_q routines */
	struct	buf	*b_forw;
	struct	buf	*b_back;
} BUFHD;

/*	
 *				REGISTERS
 *	
 *	The Controller's Register Set is Memeory Mapped, so to the Driver it 
 *	appeares as a simple local data structure.  At system Boot the Kernel's
 *	configured address is set up in the Driver and all subsequent Register
 *	Accesses Effect the actual Controller.
 */	

struct xylregs {
	
	unchar	fill_1;     /* Simple Place Holder */
	unchar	addr_1;	    /* Least Significant Address Register */
	unchar	fill_2;
	unchar	addr_2;     /* Next Least Significant Address Register */
	unchar	fill_3;
	unchar	addr_3;     /* More Significant Address Register */
	unchar	fill_4;
	unchar	addr_4;     /* Most Significant Address Register */
	unchar	fill_5;
	unchar	modifier;   /* Address Modifier Register */
	unchar	fill_6;
	unchar	csr;        /* Control/Status Register */
	unchar	fill_7;
	unchar	fatal;      /* Fatal Error Register */
	};

typedef struct xylregs REGISTERS;

/*
 *				IOPB
 *	
 *	The Iopb is the structure used to pass commands to the Controller.  The
 *	Driver's main function is to translate 'bufs' into Iopbs and make the
 *	Controller execute them.
 */

typedef struct
	{
	short	     count;		/* DMA transfer count for entry */
        unchar       nxt_sg;            /* Next S/G list length         */
	unchar	     adr_mod;		/* Address modifier		*/
	long	     dma_adr;		/* DMA address			*/
	} SGTYPE;

typedef struct iopb
	{
	unchar       byte[IOPB_SIZE]; /* Bytes the Controller Knows About */
	buf_t       *buf;             /* Address of Associated buf */
	short	     ctlr;	      /* Controller currently using this iopb */
	short        cycles;          /* Time-Out Counter */
	struct iopb *q_front;	      /* Double-linked list of active iopb's */
	struct iopb *q_back;	      /* Double-linked list of active iopb's */
#if defined(EVEREST)
	dmamap_t    *u_dmamap;        /* dmamap used for scatter/gather */
#else
        SGTYPE	     sglist[NSCAT];   /* Scatter/Gather list	*/
#endif
#if EVEREST
	long	     dma_addr;	      /* iopb's dma address */
#endif /* EVEREST */
	} IOPB, *IOPBPTR;


/*	
 *				HEAD_NODE
 *	
 *  	This head node is used for the Free IOPB queue.  The buf queues
 *	use the system defined 'bufhd' as their head nodes.	
 */

typedef struct
	{
	IOPBPTR	back;		/* Address of Last Node On Queue */
	IOPBPTR	front;		/* Address of 1st Node On Queue */
	IOPBPTR base;	  	/* start of iopbs for this ctlr (KSEG1) */
	lock_t	iopblock;	/* Lock access to this struct */
	} HEAD_NODE;

/*	
 *			IOCTL REQUEST TABLE
 *	
 *	This table is used to pass information between the Driver and the
 *	User during Special Function Requests.  What the table contains is
 *	directly related to the specific task(s) involved.
 */
typedef struct
{
	unchar i_status;/* Used to pass Iopb Comp. Code between memory spaces */
	short  par_1;   /* Used to pass simple integers between memory spaces */
	short  par_2;   /*             "                     "                */
	short  par_3;   /*             "                     "                */
	long   par_4;   /*             "                     "                */
	unchar   *ptr_1;  /* Used to Pass Addresses between memory spaces     */
	unchar   *ptr_2;  /*             "                     "              */
	unchar   *ptr_3;  /*             "                     "              */
} IOCTL_TABLE;

/*
 *	CONTROLLER PARAMETERS TABLE
 *	
 *	This table is used by the Driver to reflect the actually or potential
 *	Controller parameters.
 *      NOTE: See 754 Controller User's Manual - Set/Read Controller Parameters
 */
typedef struct
{
	unchar	   c_status;/* Debug Use Only */   
	unchar	   c_par1;  /* Corresponds to Iopb Byte 0x8 */
	unchar	   c_par2;  /* Corresponds to Iopb Byte 0x9 */
	unchar	   c_par3;  /* Corresponds to Iopb Byte 0xa */
	unchar	   c_throt; /* Corresponds to Iopb Byte 0xb */
	unchar	   c_type;  /* Corresponds to Iopb Byte 0xe */
	char      *c_name;  /* String used when displaying Ctlr related info. */
        unchar	   c_level; /* Controller interrupt level   */
        unchar	   c_vector;/* Controller interrupt vector  */
        REGISTERS *c_adr;   /* Registers base address       */
        short	   c_eprom; /* Firmware part number         */
        short      c_fwrel; /* Firmware release number      */
        short      c_fwrev; /* Firmware revision number     */ 
#if EVEREST
	dmamap_t  *c_iopbmap; /* dmamap used for accessing iopb */
#endif /* EVEREST */
} CONTROLLER_TABLE;

/* Per-controller software status and locking structure */
typedef struct xyl_ctlrinfo
{
	sema_t	      cl_stsem;	/* Semaphores the various data structures */
	lock_t	      cl_lock;	/* Locks ctlr registers and buffer queues */
	int	      cl_toid;	/* Timeout id, 0 when no timeout pending */
	struct iobuf *c_qfront;	/* pointer to first active unit queue */
	struct iobuf *c_qback;	/* pointer to last active unit queue */
} XYL_CTLRINFO;

/*	MEDIA FORMAT PARAMETERS TABLE
 *	
 *     This table is used by the Driver to reflect the actually or potential
 *     Media Format parameters. (ie. the way actual sectors look on the disk)
 *     NOTE: See 754 Controller User's Manual - Set/Read Media Format Parameters
 */
typedef struct
{
	unchar	interleave; /* Corresponds to Iopb Byte 0x6  */
	unchar	fld_1;      /* Corresponds to Iopb Byte 0x8  */
	unchar	fld_2;      /* Corresponds to Iopb Byte 0x9  */
	unchar	fld_3;      /* Corresponds to Iopb Byte 0xa  */
	unchar	fld_4;      /* Corresponds to Iopb Byte 0xb  */
	unchar	fld_5H;     /* Corresponds to Iopb Byte 0xc  */
	unchar	fld_5L;     /* Corresponds to Iopb Byte 0xd  */
	unchar	fld_12;     /* Corresponds to Iopb Byte 0xe  */ 
	unchar	fill_2;     /* Not Applicable to 754 Ctlr    */
	unchar	fld_6;      /* Corresponds to Iopb Byte 0x10 */
	unchar	fld_7;      /* Corresponds to Iopb Byte 0x11 */
	unchar	fld_5AH;    /* Corresponds to Iopb Byte 0x12 */
	unchar	fld_5AL;    /* Corresponds to Iopb Byte 0x13 */
} MEDIA_FORMAT_TABLE;

/*
 *	PARTITION TABLE
 *	
 *	This table is used by the Driver to reflect the actually or potential
 *	Logical Partition Boundries for a particular physical Unit.
 */
typedef struct
{
	unchar	p_status;  /* Debug Use Only */
	long	first;     /* Physical Sector corresponding to Sector '0' in */
			   /* the Logical Partition of the Drive             */
	long	length;    /* Number of Sectors encompassed by the Partition */
} PARTITION_TABLE;

/*
 *	UNIT PARAMETERS TABLE
 *	
 *	This table is used by the Driver to reflect the actually or potential
 *	Drive parameters for a particular Unit.
 *      NOTE: See 754 Controller User's Manual - Set/Read Drive Parameters
 */
typedef struct
{
	unchar	            u_status;    /* Used to Store OPEN/CLOSED Status  */
	unchar	            drv_param;   /* Corresponds to Iopb Byte 0x6      */
	unchar              sects_lh;    /* Sectors Configured on Last Tracks */
	short	            cylinders;   /* Cylinders Configured              */
	unchar	            heads;       /* Heads Configured                  */
	unchar	            sectors;     /* Sectors Configured                */
	unchar	            act_sects;   /* Actual Number of Secs on a track  */
	unchar	            track_spares;/* Sectors Set Aside for Slipping    */
	unchar 	            alt_tracks;  /* Tracks per Cylinder Set Asside    */
	short		    secs_per_cyl;/* Configured Sectors Per Cylinder   */
	MEDIA_FORMAT_TABLE *media_format;/* Address of Associated Format Tab  */
	PARTITION_TABLE	   *partitions;  /* Address of Associated Part. Tab   */
	char		   *name;        /* String used in Unit Spec. msgs.   */
	short		   *sect_size;   /* Pointer into Media Format Table's
				              applicable Sector Size Fields   */
   	short		    bps;	 /* Bytes per sector (ESDI status)    */
	short		    registered;  /* 1 = registered in SGI hinv        */
} UNIT_TABLE;

/*
 *	SOFTWARE PARAMETER TABLE
 *	
 *	This table is used to store and control parameters applicable to the
 *	Device Driver.  Some fields such as 'features' are changeable while
 *	the Driver is active, others such as 'iopbs' are changeable when the
 *	Driver is Installed. 
 *      NOTE: See 754 Controller User's Manual - Set/Read Controller Parameters
 */
typedef struct
{
        unchar	   ctlrs;      /* Number of controllers		*/
	unchar	   units;      /* # of Unit Tables Configured */
	unchar	   partitions; /* # of Partition Tables per Unit Configured */
	unchar	   iopbs;      /* # of Iopbs Configured */
	unchar	   bufs;       /* # of Bufs Configured */
	unchar	   retries;    /* # of Command Retries Configured */
	unchar	   cycles;     /* # of Timeout() Cycles Iopb is allowed */
	unchar	   seconds;    /* # of Seconds in each Timeout() Cycle */
	long	   polls;      /* # of milliseconds to poll for iopb */
	unchar	   features;   /* Flags Dis/En-abling Driver Features */
	unchar	   modifier;   /* VME Address Modifier used in DMA */
} SOFTWARE_TABLE;


/* The following are Silicon Graphics specific structures:	*/
typedef struct
{
	unchar	code;			/* 754 completion code  */
	char	*msg;			/* Corresponding message for code */
} ERRTAB;


/******************************************************************************
DEFINE: ioctl_cmds.h - ioctl function constants 
*******************************************************************************
DESCRIPTION:
         This file defines the constants for the ioctl functions.

******************************************************************************/

#define	CTLRRESET	0x21	/* Controller Reset */		
#define XYLBOTIOCTL     CTLRRESET /* This should always be defined to be the */
                                  /* first xylogics ioctl.                   */ 
#define	DRVRESET	0x22	/* Drive Reset */			
#define	SETCTLRPAR	0x23	/* Set Controller Parameters */	
#define	RDCTLRPAR	0x24	/* Read Controller Parameters */	
#define	SETDRVPAR	0x25	/* Set Drive Parameters */		
#define	RDDRVPAR	0x26	/* Read Drive Parameters */		
#define	SETPARTS	0x27	/* Set Partition Parameters */	
#define	RDPARTS		0x28	/* Read Partition Parameters */	
#define	SETFORMPAR	0x29	/* Set Media Format Parameters */
#define	RDFORMPAR	0x2a	/* Read Media Format Parameters */	
#define	SETSWPAR	0x2b	/* Set Software Parameters */		
#define	RDSWPAR		0x2c	/* Read Software Parameters */	
#define	WRTTRKHDR	0x2d	/* Write Track Header */		
#define	RDTRKHDR	0x2e	/* Read Track Header */		
#define	WRTDATA		0x2f	/* Write Data */			
#define	RDDATA		0x30	/* Read Data */		
#define	RDIOPB		0x31	/* Read Iopbs */			
#define	FORMAT		0x32	/* Format Tracks */			
#define	WRTDEFECTS	0x33	/* Write Defect Map */		
#define	RDDEFECTS	0x34	/* Read Defect Map */			
#define	RDDEFEXT	0x35	/* Read Extended Defect Map */			
#define DRVCONFIG	0x36	/* Show drive configuration */
#define OPTIONAL	0x37	/* ESDI optional command    */ 
#define AUTOCONFG	0x38 	/* Auto Configuration command    */ 
#define WRTVOLHDR	0x39    /* Write Volume Header	*/
#define XYLTOPIOCTL     WRTVOLHDR  /* This should always be defined to be */
                                   /* the highest xylogics ioctl cmd.     */


/*			ERROR_TYPE()
 *
 * IOPB completion codes follow a convention that indicates the action
 * required for error recovery.  The byte's upper nibble is the
 * recovery code and the lower nibble is the actual error code.
 */
#define ERROR_TYPE(comp_code)	((comp_code >> 4) & 0x0f)

/*                       ASSIGN()
 *
 * Assigns a 32 bit value from one address to another
 */
#define ASSIGN(to,from)    *((long *)to) = *((long *)from) 

/* 
 *                      RAW
 *
 * Returns TRUE if the buf is Raw (a locally generated buf) and FALSE
 * if it is a system generated buf.
 */
#define RAW(bufptr)     (bufptr->b_flags & B_RAW)

#define XYL_CTLRS       4  /* max number of SMD controllers */
#define XYL_CTLRUNITS   4  /* max units per SMD controller */
#define IPI_CTLRS       4  /* max number of IPI controllers */
#define IPI_CTLRUNITS  16  /* max units per IPI controller */
#define IPI_IOPBS      16
#define	PARTS	       16  /* partitions per unit */


/*
 * SMD positional macros
 */

/*			 SMD_CTLR()
 *
 * This macro returns the controller number to which a minor device
 * number is attached.  The system macro MINOR() is used to mask off
 * the major device number.
 */
#define SMD_CTLR(dev)	((minor(dev) / PARTS) / XYL_CTLRUNITS)

/*                       SMD_UNIT()
 *
 * This macro is used to calculate the physical unit number for a device.
 * The system macro MINOR() is used to mask off the major device number.
 * The physical unit number is the actual unit number.
 */
#define SMD_UNIT(dev)	((minor(dev) / PARTS) % XYL_CTLRUNITS)

/*			  SMD_PPOS()
 *
 * This macro is used to calculate the position in the Unit Tables for a
 * device number.  The system macro MINOR() is used to mask off the major
 * device number.  This is the position in the table for a physical unit
 * for a specific controller.
 */
#define SMD_PPOS(dev)	(minor(dev) / PARTS)

/*			SMD_CTLRDEV()
 *
 * This macro returns the first physical unit attached to a controller.
 */
#define SMD_CTLRDEV(ctlr)	(ctlr * XYL_CTLRUNITS * PARTS)


/*
 * IPI positional macros
 * IPI positional macros assume that the top 6 bits of the major number
 * are the same and that the bottom two are the same as the controller
 * number.  (i.e. (major_for_ctlr_2 & 3) == 2 and
 * (major_for_ctlr_2 & 0xFC) == (major_for_ctlr_0 & 0xFC))
 */
#define IPI_SIGDEV	0x3FF	/* significant bits for macros */

/*			IPI_CTLR()
 *
 * This macro returns the controller number to which a minor device
 * number is attached.  IPI_SIGDEV is used to mask off the unused bits of
 * the major device number.
 */
#define IPI_CTLR(dev)	(getemajor(dev) & 0x3)

/*                      IPI_UNIT()
 *
 * This macro is used to calculate the physical unit number for a device.
 * IPI_SIGDEV is used to mask off the unused bits of the major device number.
 * The physical unit number is the actual unit number.
 */
#define IPI_UNIT(dev)	(((dev & IPI_SIGDEV) / PARTS) % IPI_CTLRUNITS)

/*			IPI_PPOS()
 *
 * This macro is used to calculate the position in the Unit Tables for a
 * device number.  IPI_SIGDEV is used to mask off the unused bits of the major
 * device number.  This is the position in the table for a physical unit
 * for a specific controller.
 */
#define IPI_PPOS(dev)	(IPI_CTLR(dev)*16+IPI_UNIT(dev))

/*			IPI_CTLRDEV()
 *
 * This macro returns the first physical unit attached to a controller.
 */
#define IPI_CTLRDEV(ctlr)	(makedevice(ctlr,0))


/*			FS()
 *
 * Returns the partition number for a device.
 * Assumes that the number of partitions is a factor of two.
 */
#define FS(dev)		(dev & (PARTS - 1))

/*
 *                      SMD()
 *
 * Returns TRUE if the controller is a 752/754, and FALSE if it is
 * a 712/714.
 */
#define SMD_TYPE        0x50    /* SMD controller type (752 or 754)       */
#define ESDI_TYPE       0x10    /* ESDI controller type (712 or 714)      */
#define SMD(ctlr)       ((xylcontroller[ctlr].c_type & SMD_TYPE) == SMD_TYPE)

#endif /* _XL_H_ */
