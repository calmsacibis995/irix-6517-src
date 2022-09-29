/* if_egl.h - Interphase Eagle 4207 Ethernet driver header */

/*
modification history
--------------------
02c,22sep92,rrr  added support for c++
02b,26may92,rrr  the tree shuffle
02a,27dec91,gae  cleanup, removal of bitfields, structure simplification.
01b,04oct91,rrr  passed through the ansification filter
		  -fixed missing ;
		  -fixed #else and #endif
		  -changed TINY and UTINY to INT8 and UINT8
		  -changed copyright notice
01a,27nov90,ajm  adapted from MIPS 4.3 driver.
*/

/*
 * if_egl.h : V/Ethernet 4207 EAGLE.
 * Created  : 11/17/87 by Interphase Corp.
 * Author   : Manlio D. Marquez
 */

#ifndef __INCif_eglh
#define __INCif_eglh

#ifdef __cplusplus
extern "C" {
#endif

#define S_SHORTIO		2048	/* total short i/o space */
#define S_HUS			1812	/* total host usable space */

#define N_SCRTCH		24
#define S_SCRTCH		(sizeof(USHORT)*N_SCRTCH)

#define MAX_CQE			(S_HUS - sizeof(IOPB) - S_SCRTCH) / \
				    (sizeof (CQE) + sizeof(IOPB))

#define MAX_IOPB		MAX_CQE

#define O_MCE_IOPB		(sizeof(MCSB)+sizeof(CQE)+ \
				    (sizeof(CQE)+sizeof(IOPB))*MAX_CQE)
/* offset of MCE_IOPB */


/* Master Control Status Block (MCSB) */

/* Master Status Register
    pad:13 reserved bits
    QFC:1  Queue Flush Complete
    BOK:1  Board OK
    CNA:1  Controller Not Available
*/

#define M_MSR_QFC	0x0004
#define M_MSR_BOK	0x0002
#define M_MSR_CNA	0x0001


/* Master Control Register
    pad:2  reserved bits
    SFEN:1 Sysfail Enable
    RES:1  Reset controller
    FLQ:1  Flush Queue
    pad:8  reserved bits
    FLQR:1 Flush Queue and Report
    pad:1  reserved bits
    SQM:1  Start Queue Mode
*/

#define M_MCR_SFEN	0x2000
#define M_MCR_RES	0x1000
#define M_MCR_FLQ	0x0800
#define M_MCR_FLQR	0x0004
#define M_MCR_SQM	0x0001


/* Interrupt on Queue Available Register
    IQEA:1 Intr on Queue entry Available
    IQEH:1 Intr on Queue Half Empty Enable
    pad:3  Reserved bits
    ILVL:3 Intr Level on Queue Available
    IVCT:8 Intr Vector on Queue Available
*/

#define M_IQAR_IQEA	0x8000
#define M_IQAR_IQEH	0x4000
#define M_IQAR_ILVL	0x0700
#define M_IQAR_IVCT	0x00FF

typedef struct {		/* Master control/Status Block */
    USHORT	mcsb_MSR;	/* Master status register */
    USHORT	mcsb_RES0;	/* Reserved word 0 */
    USHORT	mcsb_MCR;	/* Master Control register */
    USHORT	mcsb_IQAR;	/* Interrupt on Queue Available Reg */
    USHORT	mcsb_QHDP;	/* Queue head pointer */
    USHORT	mcsb_RES1;	/* Reserved word 1 */
    USHORT	mcsb_RES2;	/* Reserved word 2 */
    USHORT	mcsb_RES3;	/* Reserved word 3 */
} MCSB;


/* Interface Specific Mode Physical Interface Mode
    PRM:1   Promiscuous Mode
    INL:1   Internal Loopback
    DRY:1   Disable Retry
    COL:1   Force Collision
    DTC:1   Disable Transmit CRC
    LP:1    Loopback
    DTX:1   Disable Transmit
    DRX:1   Disable Receive
    pad:4   Reserved
    PX25:1  X.25 (not supported)
    PSTR:1  Starlan (not supported)
    PIEEE:1 IEEE 802.3
    PE:1    Ethernet
*/

#define M_IMOD_PRM	0x8000
#define M_IMOD_INL	0x4000
#define M_IMOD_DRY	0x2000
#define M_IMOD_COL	0x1000
#define M_IMOD_DTC	0x0800
#define M_IMOD_LP	0x0400
#define M_IMOD_DTX	0x0200
#define M_IMOD_DRX	0x0100
#define M_IMOD_PX	0x0008
#define M_IMOD_PS	0x0004
#define M_IMOD_PIEEE	0x0002
#define M_IMOD_PE	0x0001

/* Vector structure
    pad:5	Reserved bits
    ILVL:3 Interrupt Level
    IVCT:8 Interrupt Vector
*/

#define M_VECT_ILVL	0x0700
#define M_VECT_IVCT	0x00FF

#define LVL_VCT(l,v)	((((l) << 8) & M_VECT_ILVL) | ((v) & M_VECT_IVCT))

typedef struct {		/* Controller Initialization Block */
    UINT8	cib_RES0;	/* Reserved byte 0 */
    UINT8	cib_NCQE;	/* Number of Command Queue Entries */
    USHORT 	cib_IMOD;	/* Interface Modes */
    UINT8	cib_NTXR;	/* Num LANCE Tx Rings */
    UINT8	cib_NRXR;	/* Num LANCE Rx Rings */
    UINT8	cib_PHY[6];	/* Ethernet Physical Address */
    UINT8	cib_FILT[8];	/* Ethernet Logical Address Filter */
    USHORT	cib_RXSIZ;	/* Rx Buffer Size / Length */
    USHORT	cib_NRBUF;	/* Number of Rx buffers	 */
    USHORT	cib_TXSIZ;	/* Tx Buffer Size / Length */
    USHORT	cib_NIBUF;	/* Number of Internal Tx Buffers */
    USHORT	cib_NHBUF;	/* Number of Host Managed Tx Buffers */
    USHORT	cib_NVECT;	/* Normal Completion Vector */
    USHORT	cib_EVECT;	/* Error Completion Vector */
    USHORT 	cib_BURST;	/* DMA Burst count */
    USHORT	cib_RES1[4];	/* Reserved words */
} CIB;

/* Work queue Status
    IACT:1  Initialized Active
    INACT:1 Initialized Inactive
    pad:14  Reserved Bits
*/

#define M_WQS_IACT	0x8000
#define M_WQS_INACT	0x4000

typedef struct {		/* Work Queue Information Block */
	USHORT wqib_WQS;	/* Work Queue Status */
	ULONG wqib_MAXENT;	/* Work Queue Max Entries */
	ULONG wqib_CURENT;	/* Work Queue Current Entries */
	USHORT wqib_REF;	/* Work Queue Reference Number */
	USHORT wqib_PRIO;	/* Work Queue Priority */
	USHORT wqib_WDIV;	/* Work Queue Work division */
} WQIB;

/* Command Queue Entry (CQE) */

/* Queue Entry Control Register
    pad:13 Reserved bits
    HPC:1  High Priority Command
    AA:1   Abort Acknowledge
    GO:1   Go/Busy
*/

#define M_QECR_HPC	0x0004
#define M_QECR_AA	0x0002
#define M_QECR_GO	0x0001

typedef struct {		/* Command Queue Entry */
    USHORT cqe_QECR;		/* Queue Entry Control Register */
    USHORT cqe_IOPB_ADDR;	/* IOPB Address */
    ULONG cqe_CTAG;		/* Command Tag */
    UINT8 cqe_RES0;		/* Reserved */
    UINT8 cqe_WORK_QUEUE;	/* Work Queue Number */
    USHORT cqe_RES1;		/* Reserved word */
} CQE;

/* Command Response Block (CRB) */

/* CRSW Command Response Status Word
    pad:9  Reserved bits
    QEA:1  Queue Entry Available
    QMS:1  Queue Mode Started
    AQ:1   Abort Queue
    EX:1   Exception
    ER:1   Error
    CC:1   Command Complete
    CRBV:1 Command Response Block Valid/Clear
*/

#define M_CRSW_QEA	0x0040
#define M_CRSW_QMS	0x0020
#define M_CRSW_AQ	0x0010
#define M_CRSW_EX	0x0008
#define M_CRSW_ER	0x0004
#define M_CRSW_CC	0x0002
#define M_CRSW_CRBV	0x0001


typedef struct {		/* Command Response Block */
    USHORT crb_CRSW;		/* Command Response Status Word */
    USHORT crb_RES0;		/* Reserved word */
    ULONG crb_CTAG;		/* Command Tag */
    UINT8 crb_RES1;		/* Reserved byte */
    UINT8 crb_WORK_QUEUE;	/* Work Queue Number */
    USHORT crb_RES2;		/* Reserved word */
} CRB;


/* Configuration Status Block (CSTB) */

typedef struct {		/* Configuration Status Block 72 bytes */
    UINT8 cstb_RES0;		/* Reserved byte */
    UINT8 cstb_PCODE[3];	/* Product Code */
    UINT8 cstb_RES1;		/* Reserved byte */
    UINT8 cstb_PVAR;		/* Product Variation */
    UINT8 cstb_RES2;		/* Reserved byte */
    UINT8 cstb_FREV[3];		/* Firmware Revision level */
    UINT8 cstb_FDATE[8];	/* Firmware Release date */
    UINT8 cstb_RES3;		/* Reserved word */
    UINT8 cstb_SREV[3];		/* Software Revision Level */
    UINT8 cstb_SDATE[8];	/* Software Release Date */
    USHORT cstb_BSIZE;		/* Buffer size in Kbytes */
    ULONG cstb_HUSO;		/* Host Usable Space Offset */
    ULONG cstb_RBPO; 		/* Internal Transmit Buffer Offset */
    ULONG cstb_HMTBO;		/* Host Transmit Buffer Offset */
    ULONG cstb_HUBO;		/* Host Usable Buffer Memory Offset */
    UINT8 cstb_PHY[6];		/* Current Physical Node Address */
    UINT8 cstb_FILT[8];		/* Current Logical Address Filter */
    UINT8 cstb_RES4[10];	/* Reserved */
} CSTB;

/* Controller Statistics Block (CSB) */

typedef struct {	/* Controller Statistics Block */
#define	csb_TXATT	csb_stat[0]	/* Number of tx attempts */
#define	csb_TXDMACMP	csb_stat[1]	/* Number of tx DMA completions */
#define	csb_LTXINTS	csb_stat[2]	/* Number of LANCE tx ints */
#define	csb_LGDTXS	csb_stat[3]	/* Number of good LANCE tx */
#define	csb_TXERRS	csb_stat[4]	/* Number of LANCE tx errors */
#define	csb_TXDONES	csb_stat[5]	/* Number of LANCE tx dones */
#define	csb_RXATMPS	csb_stat[6]	/* Number of LANCE rx attempts */
#define	csb_RXSTARVE	csb_stat[7]	/* Number of LANCE rx starvations */
#define	csb_RXLINTS	csb_stat[8]	/* Number of LANCE rx interrupts */
#define	csb_RXGOOD	csb_stat[9]	/* Number of good LANCE rx */
#define	csb_RXERROR	csb_stat[10]	/* Number of LANCE rx errors */
#define	csb_RXDMACMP	csb_stat[11]	/* Number of LANCE rx DMA completions */
#define	csb_RXDONE	csb_stat[12]	/* Number of LANCE rx done */
#define	csb_MISCDMA	csb_stat[13]	/* Number of miscellaneous DMA ints */
#define	csb_TOTLINTS	csb_stat[14]	/* total LANCE interrupts */
#define	csb_CSR0INIT	csb_stat[15]	/* init done for LANCE */
#define	csb_CSR0BABB	csb_stat[16]	/* csr0 babble packets */
#define	csb_CSR0CERR	csb_stat[17]	/* csr0 collisions */
#define	csb_CSR0MISS	csb_stat[18]	/* csr0 Miss */
#define	csb_CSR0MEM	csb_stat[19]	/* csr0 Memory errors */
    ULONG csb_stat [20];
} CSB;

/* IOPB Format (IOPB) */

/* IOPB Option word
    pad:6	Reserved bits
    DATA:1	Transmit - pointer to data only
    BABL:1	Transmit - ignore babble errors
    pad:5	Reserved bits
    DMA:1	DMA Enable
    SG:1	Scatter/Gather bit
    IE:1	Interrupt Enable
*/

#define M_OPT_DATA	0x0200
#define M_OPT_BABL	0x0100
#define M_OPT_DMA	0x0004
#define M_OPT_SG	0x0002
#define M_OPT_IE	0x0001


/* VME bus transfer options
    pad:3   Reserved bits
    DIR:1   Direction
    TRANS:2 Transfer Type
    MEMT:2  Memory Type
    pad:2   Reserved bits
    ADRM:6  Address modifier
*/

#define M_TOPT_DIR	0x1000
#define M_TOPT_TRANS	0x0C00
#define M_TOPT_MEMT	0x0300
#define M_TOPT_MOD	0x003F

typedef struct {	/* Transmit / Receive IOPB */
    USHORT iopb_CMD;	/* IOPB Command code 0x50 / 0x60 */
    USHORT iopb_OPTION;	/* IOPB Option word */
    USHORT iopb_STATUS;	/* IOPB Return Status word */
    USHORT iopb_NVCT;	/* IOPB Normal completion Vector */
    USHORT iopb_EVCT;	/* IOPB Error completion Vector */
    USHORT iopb_TOPT;	/* IOPB VME bus transfer options */
    ULONG iopb_BUFF;	/* IOPB Buffer Address */
    ULONG iopb_LENGTH;	/* IOPB Max-Transfer Length */
    USHORT iopb_HBUF;	/* IOPB Host Managed Buffer Number */
    USHORT iopb_PTLF;	/* IOPB Packet Type / Length Field */
    UINT8 iopb_NODE[6];	/* IOPB Node Address */
    USHORT iopb_SGEC;	/* IOPB Scatter / Gather Element Count */
    USHORT iopb_LAN1;	/* IOPB LANCE Descriptor Word 1 */
    USHORT iopb_LAN3;	/* IOPB LANCE Descriptor Word 3 */
} IOPB;

typedef struct {		/* DMA IOPB */
    USHORT iopb_CMD;		/* IOPB Command code 0x70 */
    USHORT iopb_OPTION;		/* IOPB Option word */
    USHORT iopb_STATUS;		/* IOPB Return Status word */
    USHORT iopb_NVCT;		/* IOPB Normal completion Vector */
    USHORT iopb_EVCT;		/* IOPB Error completion Vector */
    USHORT iopb_BUFF_HI;	/* IOPB Error completion Vector */
    USHORT iopb_BUFF_LO;	/* IOPB VME Buffer Address */
    USHORT iopb_IBUFF_HI;	/* IOPB Host Managed Buffer Address */
    USHORT iopb_IBUFF_LO;	/* IOPB Host Managed Buffer Address */
    USHORT iopb_LENGTH_HI;	/* IOPB Transfer Length */
    USHORT iopb_LENGTH_LO;	/* IOPB Transfer Length */
    USHORT iopb_TOPT;		/* IOPB VME bus transfer options */
} DMA_IOPB;

typedef struct {		/* Initialize Controller IOPB */
    USHORT ic_iopb_CMD;		/* IOPB Command code 0x41 */
    USHORT ic_iopb_OPTION;	/* IOPB Option word */
    USHORT ic_iopb_STATUS;	/* IOPB Return Status word */
    USHORT ic_iopb_NVCT;	/* IOPB Normal completion Vector */
    USHORT ic_iopb_EVCT;	/* IOPB Error completion Vector */
    USHORT ic_iopb_RES0;	/* IOPB Reserved word */
    ULONG ic_iopb_BUFF;		/* IOPB Buffer Address */
    ULONG ic_iopb_RES1[5];	/* IOPB Reserved words */
} IC_IOPB;

typedef struct {		/* Change Node Address IOPB */
    USHORT cn_iopb_CMD;		/* IOPB Command code 0x45 */
    USHORT cn_iopb_OPTION;	/* IOPB Option word */
    USHORT cn_iopb_STATUS;	/* IOPB Return Status word */
    USHORT cn_iopb_NVCT;	/* IOPB Normal completion Vector */
    USHORT cn_iopb_EVCT;	/* IOPB Error completion Vector */
    USHORT cn_iopb_RES0[5];	/* IOPB Reserved word	 */
    UINT8 cn_iopb_PHY[6];	/* IOPB New Node Address */
    USHORT cn_iopb_RES1[5];	/* IOPB Reserved words	 */
} CN_IOPB;

typedef struct {		/* Change Address Filter IOPB  */
    USHORT cf_iopb_CMD;		/* IOPB Command code 0x46 */
    USHORT cf_iopb_OPTION;	/* IOPB Option word */
    USHORT cf_iopb_STATUS;	/* IOPB Return Status word */
    USHORT cf_iopb_NVCT;	/* IOPB Normal completion Vector */
    USHORT cf_iopb_EVCT;	/* IOPB Error completion Vector */
    USHORT cf_iopb_RES0[5];	/* IOPB Reserved word */
    UINT8 cf_iopb_FILT[8];	/* IOPB New Logical Address Filter */
    USHORT cf_iopb_RES1[4];	/* IOPB Reserved words */
} CF_IOPB;

/* Initialize Work Queue Command Format (WQCF) */

/* Work Queue Options
    IWQ:1  Initialize Work Queue
    pad:14 Reserved bits
    AE:1   Abort Enable
*/

#define M_WOPT_IWQ	0x8000
#define M_WOPT_AE	0x0001

#define EGL_MISCQ	0x00	/* Miscellaneous Work queue */
#define EGL_DMAQ	0x01	/* DMA Work Queue */
#define EGL_RECVQ	0x02	/* Receive Work Queue */
#define EGL_XMTQN(x)	((x) + 0x03)	/* Transmit Work Queue N */


typedef struct {		/* Initialize Work Queue Command Format */
    USHORT wqcf_CMD;		/* Command Normally (0x42) */
    USHORT wqcf_OPTION;		/* Command Options */
    USHORT wqcf_STATUS;		/* Return Status */
    USHORT wqcf_NVCT;		/* Normal Completion Vector */
    USHORT wqcf_EVCT;		/* Error Completion Vector */
    USHORT wqcf_RES0[5];	/* Reserved Words */
    USHORT wqcf_WORKQ;		/* Work Queue Number */
    USHORT wqcf_WOPT;		/* Work Queue Options */
    USHORT wqcf_SLOTS;		/* Number of slots in the Work Queues */
    USHORT wqcf_PRIORITY;	/* Priority Level */
    USHORT wqcf_WDIV;		/* Work Division */
    USHORT wqcf_RES1[3];	/* Reserved words */
} WQCF;

/* Short I/O Format */

typedef struct {
    MCSB sh_MCSB;		/* Master Control / Status Block */
    CQE sh_MCE;			/* Master Command Entry */
    CQE sh_CQE[MAX_CQE];	/* Command Queue Entry */
    IOPB sh_IOPB[MAX_IOPB];	/* Host IOPB Space */
    IOPB sh_MCE_IOPB;		/* Host MCE IOPB Space */
    USHORT sh_SCRTCH[N_SCRTCH];	/* Scratch Area for Parameter IOPBS */
    CRB sh_CRB;			/* Command Response Block */
    USHORT sh_FILL[2];		/* filler */
    IOPB sh_RET_IOPB;		/* Returned IOPB */
    CSTB sh_CSTB;		/* Controller Status Block */
    CSB sh_CSB;			/* Controller Statistics Block */
    USHORT sh_PSEM;		/* Printf Semaphores	 */
    USHORT sh_VMEIV;		/* VME bus Interrupt Vector */
} SHIO;


/* Eagle Control IOPB's */

#define CNTR_DIAG		0x40	/* Perform Diagnostics              */
#define CNTR_INIT		0x41	/* Initialize Controller            */
#define CNTR_INIT_WORKQ		0x42	/* Initialize Work Queue            */
#define CNTR_DUMP_INIT		0x43	/* Dump Initialization Parameters   */
#define CNTR_REPORT_STATS	0x44	/* Report Statistics		    */
#define CNTR_CHANGE_NODE	0x45	/* Change Default Node Address	    */
#define CNTR_CHANGE_FILTER	0x46	/* Change Def. Logical Address Filter*/
#define CNTR_FLUSH_WORKQ	0x49	/* Flush Work Queue                 */
#define CNTR_TRANSMIT		0x50	/* Transmit			    */
#define CNTR_RECEIVE		0x60	/* Receive			    */
#define CNTR_DMA		0x70	/* DMA			            */

/* Memory types */
#define MEMT_16BIT		0x0100	/* 16 Bit Memory type */
#define MEMT_32BIT		0x0200	/* 32 Bit Memory type */
#define MEMT_SHIO 		0x0300	/* Short I/O Memory   */

/* Transfer types  */
#define TT_NORMAL		0x0000	/* Normal Mode */
#define TT_BLOCK		0x0400	/* Block  Mode */
#define TT_DISABLE_INC_ADDR	0x0800	/* Disable Incrementing Addresses */

#define DIR_READ		0x1000	/* Read from Eagle */
#define DIR_WRITE		0x0000	/* Write to Eagle  */

/* LANCE error bits */
#define LANCE_RERR		0x4000	/* Receive Error	*/
#define LAN1_FRAM		0x2000	/* Framing error 	*/
#define LAN1_OFLO		0x1000	/* Overflow error	*/
#define LAN1_CRC		0x0800	/* CRC error		*/
#define LAN1_BUFF		0x0400	/* Buffer error		*/
#define LANCE_RENP		0x0100	/* End of Packet	*/
#define LAN1_RBITS "\20\16OWN\15ERR\14FRAM\13OFLO\12CRC\11BUFF\10STP\09ENDP"

#define LANCE_TERR		0x4000	/* Transmit Error	*/
#define LANCE_TMORE		0x1000	/* More Than One Retry	*/
#define LANCE_TONE		0x0800	/* Exactly One Retry	*/
#define LAN3_BUFF		0x8000	/* Transmit Buffer error*/
#define LAN3_UFLO		0x4000	/* Underflow error	*/
#define LAN3_LCOL		0x1000	/* Late collision	*/
#define LAN3_LCAR		0x0800	/* Loss of carrier	*/
#define LAN3_RTRY		0x0400	/* Retry error		*/
#define LAN1_TBITS "\20\16OWN\15ERR\13MORE\12ONE\11DEF\10STP\09ENDP"
#define LAN3_TBITS "\20\16BUFF\15UFLO\13LCOL\12LCAR\11RTRY"

#ifdef __cplusplus
}
#endif

#endif /* __INCif_eglh */
