/* This file is the 1394 lower-level driver file that supports
   the TI-LYNX chipset. */

#define _TI_LYNX

#include "sys/types.h"
#include "sys/ksynch.h"
#include "sys/ddi.h"
#include "sys/param.h"
#include "sys/systm.h"
#include "sys/edt.h"
#include "sys/sysmacros.h"
#include "sys/cmn_err.h"
#include "sys/debug.h"
#include "sys/kmem.h"
#include "sys/mman.h"
#include "ksys/ddmap.h"
#include "sys/errno.h"
#include "sys/immu.h"
#include "sys/invent.h"
#include "sys/kopt.h"
#include "sys/mbuf.h"
#include "sys/sbd.h"
#include "sys/socket.h"
#include "sys/cpu.h"
#include "sys/conf.h"
#include "sys/ddi.h"
#include "sys/mload.h"
#include <sys/cred.h>
#ifndef BONSAI
#include "sys/pci_intf.h"
#endif
#include "sys/PCI/pciio.h"
#include "sys/PCI/PCI_defs.h"
#include "sys/ktime.h"
#include "sys/sema.h"
#include "ksys/sthread.h"
#include "ieee1394.h"

#define DEBUG_ZERO_MAPPAGES
#define TI_LYNX_VNDID      0x104c
#define TI_LYNX_DEVID      0x8000

/* TCODES */
#define IEEE1394_WRITE_REQUEST_QUAD         0
#define IEEE1394_WRITE_REQUEST_BLOCK        1
#define IEEE1394_WRITE_RESPONSE             2
#define IEEE1394_READ_REQUEST_QUAD          4
#define IEEE1394_READ_REQUEST_BLOCK         5
#define IEEE1394_READ_RESPONSE_QUAD         6
#define IEEE1394_READ_RESPONSE_BLOCK        7
#define IEEE!394_CYCLE_START                8
#define IEEE1394_LOCK_REQUEST               9
#define IEEE1394_ISOCHRONOUS_BLOCK          0xa
#define IEEE1394_LOCK_RESPONSE              0xb

/* OPERATIONS (to get headerlist info & interrupt callback) */
#define SIDRCV 0
#define ISORCV 1
#define ISOXMT 2
#define ASYRCV 3
#define ASYXMT 4
#define ACKRCV 5
#define BUSRESET 6

#define ISORCV_NUMDATAPAGES    500
#define ISORCV_NUMDATAPACKETS   (ISORCV_NUMDATAPAGES*2)
#define SIDRCV_NUMDATAPAGES    5
#define SIDRCV_NUMDATAPACKETS   (SIDRCV_NUMDATAPAGES*2)
#define ASYRCV_NUMDATAPAGES    5
#define ASYRCV_NUMDATAPACKETS   (ASYRCV_NUMDATAPAGES*2)
#define ISOXMT_NLISTENTRIES     5120

#define ISOXMT_NUMDATAPACKETS  ISOXMT_NLISTENTRIES
#define ISOXMT_NUMDATAPAGES    (((ISOXMT_NLISTENTRIES)+1)/2)

/* Transaction states */
#define TQ_AWAITING_XMIT 1
#define TQ_AWAITING_ACK  2
#define TQ_AWAITING_CONF 3
#define TQ_FINISHED      4

/* DMA Channel Assignments */
#define FW_ISORCV_DMA_CHAN      0
#define FW_ISOXMT_DMA_CHAN      1
#define FW_SIDRCV_DMA_CHAN      2
#define FW_ASYRCV_DMA_CHAN      3
#define FW_ASYXMT_DMA_CHAN      4

#define FW_ISORCV_DMA_PCL_INT DMA0_PCL
#define FW_ISOXMT_DMA_PCL_INT DMA1_PCL
#define FW_SIDRCV_DMA_PCL_INT DMA2_PCL
#define FW_ASYRCV_DMA_PCL_INT DMA3_PCL
#define FW_ASYXMT_DMA_PCL_INT DMA4_PCL

#define FW_ISORCV_DMA_HLT_INT DMA0_HLT
#define FW_ISOXMT_DMA_HLT_INT DMA1_HLT
#define FW_SIDRCV_DMA_HLT_INT DMA2_HLT
#define FW_ASYRCV_DMA_HLT_INT DMA3_HLT
#define FW_ASYXMT_DMA_HLT_INT DMA4_HLT


/* PCL Transfer Commands */

#define FW_XFER_CMD_RCV                         (1<<24)
#define FW_XFER_CMD_RCV_AND_UPDATE              (10<<24)
#define FW_XFER_CMD_XMT                         (2<<24)
#define FW_XFER_CMD_UNFORMATTED_XMIT            (12<<24)
#define FW_XFER_CMD_PCI_TO_LBUS                 (8<<24)
#define FW_XFER_CMD_LBUS_TO_PCI                 (9<<24)

#define FW_XFER_CMD_BIGENDIAN                   (1<<16)
#define FW_XFER_CMD_WAITFORSTATUS               (1<<17)
#define FW_XFER_CMD_LAST_BUF                    (1<<18)
#define FW_XFER_CMD_INT                         (1<<19)

#define FW_XFER_CMD_ISO                         (1<<12)
#define FW_XFER_CMD_MULTIISO                    (1<<13)
#define FW_XFER_CMD_100MBPS                     (0<<14)
#define FW_XFER_CMD_200MBPS                     (1<<14)
#define FW_XFER_CMD_400MBPS                     (2<<14)

/* PCL Auxiliary Commands */

#define FW_AUX_CMD_NOP                          (0<<24)
#define FW_AUX_CMD_LOAD                         (3<<24)
#define FW_AUX_CMD_STORE_QUAD                   (4<<24)
#define FW_AUX_CMD_STORE_DOUBLE                 (11<<24)
#define FW_AUX_CMD_STORE_0                      (5<<24)
#define FW_AUX_CMD_STORE_1                      (6<<24)
#define FW_AUX_CMD_BRANCH_NEVER                 ((7<<24) | (0<<20))
#define FW_AUX_CMD_BRANCH_DMAREADY              ((7<<24) | (1<<20))
#define FW_AUX_CMD_BRANCH_NOTDMAREADY           ((7<<24) | (2<<20))
#define FW_AUX_CMD_BRANCH_EXTREADY              ((7<<24) | (3<<20))
#define FW_AUX_CMD_BRANCH_NOTEXTREADY           ((7<<24) | (4<<20))
#define FW_AUX_CMD_BRANCH_GPIO2                 ((7<<24) | (5<<20))
#define FW_AUX_CMD_BRANCH_GPIO3                 ((7<<24) | (6<<20))
#define FW_AUX_CMD_INT                          (1<<19)
#define FW_AUX_CMD_LAST                         (1<<18)
#define FW_AUX_CMD_ADD                          (13<<24)
#define FW_AUX_CMD_COMPARE                      (14<<24)

/* PCI Interrupt Status Bits
   used for both the PCI Interrupt Status Register @ 0x48 and the
   PCI Interrupt Enable Register @ 0x4C */

#define INT_PEND        (1<<31)
#define FRC_INT         (1<<30)
#define SLV_ADR_PERR    (1<<28)
#define SLV_DAT_PERR    (1<<27)
#define MST_DAT_PERR    (1<<26)
#define MST_DEV_TO      (1<<25)
#define MST_RETRY-TO    (1<<24)
#define INTERNAL_SLV_TO (1<<23)
#define AUX_TO          (1<<18)
#define AUX_INT         (1<<17)
#define P1394_INT       (1<<16)
#define DMA4_PCL        (1<<9)
#define DMA4_HLT        (1<<8)
#define DMA3_PCL        (1<<7)
#define DMA3_HLT        (1<<6)
#define DMA2_PCL        (1<<5)
#define DMA2_HLT        (1<<4)
#define DMA1_PCL        (1<<3)
#define DMA1_HLT        (1<<2)
#define DMA0_PCL        (1<<1)
#define DMA0_HLT        (1<<0)

/* Link Layer Interrupt Interrupt Status Bits
   used for both the Link Layer Interrupt Enable Register @ 0xF18 and the
   Link Layer Interrupt Status Register # 0xF14 */

#define LINK_INT        (1<<31)
#define PHY_TIME_OUT    (1<<30)
#define PHY_REG_RCVD    (1<<29)
#define PHY_BUSRESET    (1<<28)
#define TX_RDY          (1<<26)
#define RX_DATA_RDY     (1<<25)
#define IT_STUCK        (1<<20)
#define AT_STUCK        (1<<19)
#define SNTRJ           (1<<17)
#define HDR_ERR         (1<<16)
#define TC_ERR          (1<<15)
#define CYC_SEC         (1<<11)
#define SCY_STRT        (1<<10)
#define CYC_DONE        (1<<9)
#define CYC_PEND        (1<<8)
#define CYC_LOST        (1<<7)
#define CYC_ARB_FAILED  (1<<6)
#define GRF_OVER_FLOW   (1<<5)
#define ITF_UNDER_FLOW  (1<<4)
#define ATF_UNDER_FLOW  (1<<3)
#define IARB_FAILED     (1)

/* Link Layer Control Bits, for the Link Layer Control Register at 0xF04 */

#define BUSY_CNTRL      (1<<29)
#define TX_ISO_EN       (1<<26)
#define RX_ISO_EN       (1<<25)
#define TX_ASYNC_EN     (1<<24)
#define RX_ASYNC_EN     (1<<23)
#define RSTTX           (1<<21)
#define RSTRX           (1<<20)
#define CYCMASTER       (1<<11)
#define CYCSOURCE       (1<<10)
#define CYCTIMEREN      (1<<9)
#define RCV_COMP_VALID  (1<<7)

#define INT(x) ((int)(&(x)))
#define OFFSET(x) (INT(ti_lynx_mem->x)-INT(*ti_lynx_mem))
#define PAGEOFFSET(x) ( OFFSET(x) >> 12 )
#define BYTEOFFSET(x) ( OFFSET(x) & 0xfff )
#define PCI_ADDR(x) (BYTEOFFSET(x) + ti_lynx_state_ptr->pciaddrs[PAGEOFFSET(x)])
#define PCI_ADDR_SWAPPED(x) (PCI_ADDR(x)-PCI_NATIVE_VIEW)
#define PHYS_TO_PCI_NATIVE(p) ((p)+PCI_NATIVE_VIEW)

/* These structures describe the data shared between the MIPS and the
   TI Lynx chip. */

/* PCL (Packet Control List) struct */

typedef struct ti_lynx_pcl_s {
    int    next;                /* 0x00 */
    int    error_next;          /* 0x04 */
    int    sw;                  /* 0x08 */
    int    status;              /* 0x0c */
    int    remainder;           /* 0x10 */
    int    nextbuf;             /* 0x14 */
    int    buf_0_ctl;           /* 0x18 */
    int    buf_0;               /* 0x1c */
    int    buf_1_ctl;           /* 0x20 */
    int    buf_1;               /* 0x24 */
    int    buf_2_ctl;           /* 0x28 */
    int    buf_2;               /* 0x2c */
    int    buf_3_ctl;           /* 0x30 */
    int    buf_3;               /* 0x34 */
    int    buf_4_ctl;           /* 0x38 */
    int    buf_4;               /* 0x3c */
    int    buf_5_ctl;           /* 0x40 */
    int    buf_5;               /* 0x44 */
    int    buf_6_ctl;           /* 0x48 */
    int    buf_6;               /* 0x4c */
    int    buf_7_ctl;           /* 0x50 */
    int    buf_7;               /* 0x54 */
    int    buf_8_ctl;           /* 0x58 */
    int    buf_8;               /* 0x5c */
    int    buf_9_ctl;           /* 0x60 */
    int    buf_9;               /* 0x64 */
    int    buf_a_ctl;           /* 0x68 */
    int    buf_a;               /* 0x6c */
    int    buf_b_ctl;           /* 0x70 */
    int    buf_b;               /* 0x74 */
    int    buf_c_ctl;           /* 0x78 */
    int    buf_c;               /* 0x7c */

} ti_lynx_pcl_t;

/* Memory layout for the memory shared between the Lynx chip and the OS */

typedef struct WorkListEntry_s {
    int hdrloc;  /* PCI address of entry in iso_rcv.headerlist */
    int dataloc; /* PCI address of entry in iso_rcv.datapages */
    int index;   /* Index (1, 2, 3, of this entry) */
    int next;    /* PCI address of next entry in iso_rcv.worklist */
} WorkListEntry_t;

typedef struct HeaderListEntry_s {
    int hdr;
    int cyclecounter;
    int len;
    int dataoffset;
} HeaderListEntry_t;

typedef struct XmitListEntry_s {
    int cmd;
    int dataloc;
    int index;
    int next;
}XmitListEntry_t;

typedef struct ti_lynx_mem_layout_s {
    struct {
        ti_lynx_pcl_t        pcl[32];                      /* Page 0, 0-4095  */
        int               StartPCLAddress;              /* Page 1, 0-3     */
        int               TailP;                        /* Page 1, 4-7     */
        int               WorkListP;                    /* Page 1, 8-11    */
        char              pad1[4096-12];                /* Page 1, 12-4095 */
        WorkListEntry_t   worklist[ISORCV_NUMDATAPACKETS]; /* Page 2-33    */
        char              pad2[(4096-((ISORCV_NUMDATAPACKETS*16)%4096))%4096];
        HeaderListEntry_t headerlist[ISORCV_NUMDATAPACKETS]; /* Page 34-65 */
        char              pad3[(4096-((ISORCV_NUMDATAPACKETS*16)%4096))%4096];
        int               datapages[1024*ISORCV_NUMDATAPAGES];/* Page 66-4065*/
    } isorcv;
    struct {
        ti_lynx_pcl_t        pcl[32];                      /* Page 4066, 0-4095 */
        int               StartPCLAddress;              /* Page 4067, 0-3    */
        int               TailP;                        /* Page 4067, 4-7    */
        int               WorkListP;                    /* Page 4067, 8-11   */
        char              pad1[4096-12];                /* Page 4067, 12-4095*/
        WorkListEntry_t   worklist[SIDRCV_NUMDATAPACKETS]; /* Page 4068      */
        char              pad2[(4096-((SIDRCV_NUMDATAPACKETS*16)%4096))%4096];
        HeaderListEntry_t headerlist[SIDRCV_NUMDATAPACKETS]; /* Page 4069    */
        char              pad3[(4096-((SIDRCV_NUMDATAPACKETS*16)%4096))%4096];
        int          datapages[1024*SIDRCV_NUMDATAPAGES]; /* Page 4070-4074  */
    } sidrcv;
    struct {
        ti_lynx_pcl_t        pcl[32];                      /* Page 4075, 0-4095 */
        int               StartPCLAddress;              /* Page 4076 0-3     */
        int               TailP;                        /* Page 4076 4-7     */
        int               WorkListP;                    /* Page 4076 8-11     */
	char              pad1[4096-12];                /* Page 4076 12-4095 */
        WorkListEntry_t   worklist[ASYRCV_NUMDATAPACKETS]; /* Page 4077 0-159 */
	char              pad2[(4096-((ASYRCV_NUMDATAPACKETS*16)%4096))%4096];
        HeaderListEntry_t headerlist[ASYRCV_NUMDATAPACKETS]; /* Page 4078 0-159 */
        char              pad3[(4096-((ASYRCV_NUMDATAPACKETS*16)%4096))%4096];
        int          datapages[1024*ASYRCV_NUMDATAPAGES]; /* Page 4079-4083*/
    } asyrcv;
    struct {
        ti_lynx_pcl_t     pcl[32];                      /* Page 4084 */
        int               StartPCLAddress;              /* Page 4085 0-3 */
        int               buf[1023];                    /* Page 4085 4-4095 */
    } asyxmt;
    struct {
        ti_lynx_pcl_t     pcl[32];                      /* Page 4086 */
        int               StartPCLAddress;              /* Page 4087 0-3 */
        int               HeadP;
        int               XmitListP;
        char              pad1[4096-12];                /* Page 4087 4-4095 */
        XmitListEntry_t   xmitlist[ISOXMT_NLISTENTRIES];
/*
        char              pad2[(4096-((ISOXMT_NUMDATAPACKETS*16)%4096))%4096];
*/
        int               datapages[1024*ISOXMT_NUMDATAPAGES];
    } isoxmt;
} ti_lynx_mem_layout_t;

#define NMAPPAGES ((sizeof(ti_lynx_mem_layout_t)+4095)/4096)


/* Representation of a transaction */
typedef struct ieee1394_transaction_s {
    long long destination_offset;
    int       destination_id;
    int       source_id;
    int       *block_data_p;
    struct    ieee1394_transaction_s *next;
    struct    ieee1394_transaction_s *prev;
    int       stat;
    int       busy;
    int       state;
    int       data_length;
    int       tlabel;
    int       tcode;
    int       priority;
    int       extended_tcode;
    int       ntimeout;
} ieee1394_transaction_t;


/* State struct for TI LYNX board */
typedef struct ti_lynx_state_s {
    __psunsigned_t      p_mem0;
    __psunsigned_t      p_mem1;
    __psunsigned_t      p_mem2;
    char                *mappages;
    ieee1394_transaction_t  *tlist;
    int                 sidrcv_HeadP;
    int                 isorcv_HeadP;
    int                 asyrcv_HeadP;
    int                 rcvquad;
    sema_t              Physema;
    int                 pciaddrs[NMAPPAGES+1];
} ti_lynx_state_t;


/* Register Layout for the TI LYNX chip */
typedef struct ti_lynx_s
{
    int         DeviceVendor;           /* 0x000 */
    int         StatusCommand;          /* 0x004 */
    int         ClassRevision;          /* 0x008 */
    int         BISTHeaderLatencyCache; /* 0x00c */
    int         Base0;                  /* 0x010 */
    int         Base1;                  /* 0x014 */
    int         Base2;                  /* 0x018 */
    int         reserved_01c;           /* 0x01c */
    int         reserved_020;           /* 0x020 */
    int         reserved_024;           /* 0x024 */
    int         reserved_028;           /* 0x028 */
    int         SubsystemVendorID;      /* 0x02c */
    int         RPLROMBase;             /* 0x030 */
    int         reserved_034;           /* 0x034 */
    int         reserved_038;           /* 0x038 */
    int         LatencyGrantPinLine;    /* 0x03c */
    int         MiscControl;            /* 0x040 */
    int         EEPROMControl;          /* 0x044 */
    int         PCIIntStatus;           /* 0x048 */
    int         PCIIntEnable;           /* 0x04c */
    int         PCITest;                /* 0x050 */
    int         reserved_054_0ac[23];   /* 0x054-0x0ac */
    int         LocalBusControl;        /* 0x0b0 */
    int         LocalBusAddress;        /* 0x0b4 */
    int         PCI_GPIO10Control;      /* 0x0b8 */
    int         PCI_GPIO32Control;      /* 0x0bc */
    int         PCI_GPIODATA_0000;      /* 0x0c0 */
    int         PCI_GPIODATA_0001;      /* 0x0c4 */
    int         PCI_GPIODATA_0010;      /* 0x0c8 */
    int         PCI_GPIODATA_0011;      /* 0x0cc */
    int         PCI_GPIODATA_0100;      /* 0x0d0 */
    int         PCI_GPIODATA_0101;      /* 0x0d4 */
    int         PCI_GPIODATA_0110;      /* 0x0d8 */
    int         PCI_GPIODATA_0111;      /* 0x0dc */
    int         PCI_GPIODATA_1000;      /* 0x0e0 */
    int         PCI_GPIODATA_1001;      /* 0x0e4 */
    int         PCI_GPIODATA_1010;      /* 0x0e8 */
    int         PCI_GPIODATA_1011;      /* 0x0ec */
    int         PCI_GPIODATA_1100;      /* 0x0f0 */
    int         PCI_GPIODATA_1101;      /* 0x0f4 */
    int         PCI_GPIODATA_1110;      /* 0x0f8 */
    int         PCI_GPIODATA_1111;      /* 0x0fc */
    struct {
        int         DMA_Temp;              /* 0x100 */
        int         DMA_PCLAddress;        /* 0x104 */
        int         DMA_BufferAddress;     /* 0x108 */
        int         DMA_Status;            /* 0x10c */
        int         DMA_Control;           /* 0x110 */
        int         DMA_Ready;             /* 0x114 */
        int         DMA_State;             /* 0x118 */
        int         reserved_11c;           /* 0x11c */
    }dmaregs1[5];
    int         reserved_120_8fc[472];  /* 0x19c-0x8fc */
    int         DMADiagnosticTest;      /* 0x900 */
    int         DMARxPacketCount;       /* 0x904 */
    int         DMAGlobal;              /* 0x908 */
    int         reserved_90c_9fc[61];   /* 0x90c-0x9fc */
    int         FIFO_Size;              /* 0xa00 */
    int         FIFO_PCISidePointer;    /* 0xa04 */
    int         FIFO_LinkSidePointer;   /* 0xa08 */
    int         FIFO_TokenStatus;       /* 0xa0c */
    int         FIFO_Control;           /* 0xa10 */
    int         FIFO_Threshold;         /* 0xa14 */
    int         reserved_a18;           /* 0xa18 */
    int         reserved_a1c;           /* 0xa1c */
    int         FIFO_GRF;               /* 0xa20 */
    int         FIFO_GRF_Ctl;           /* 0xa24 */
    int         reserved_a28;           /* 0xa28 */
    int         reserved_a2c;           /* 0xa2c */
    int         FIFO_ATF;               /* 0xa30 */
    int         FIFO_ATF_Ctl;           /* 0xa34 */
    int         reserved_a38;           /* 0xa38 */
    int         reserved_a3c;           /* 0xa3c */
    int         FIFO_ITF;               /* 0xa40 */
    int         FIFO_ITF_Ctl;           /* 0xa44 */
    int         reserved_a48_afc[46];   /* 0xa48-0xafc */
    struct {
        int         DMA_W0_Value;          /* 0xb00 */
        int         DMA_W0_Mask;           /* 0xb04 */
        int         DMA_W1_Value;          /* 0xb08 */
        int         DMA_W1_Mask;           /* 0xb0c */
    }dmaregs2[5];
    int         reserved_b50_efc[236];  /* 0xbf0-0xefc */
    int         BusNodeNumber;          /* 0xf00 */
    int         LLControl;              /* 0xf04 */
    int         CycleTimer;             /* 0xf08 */
    int         PHYAccess;              /* 0xf0c */
    int         DiagnosticTest;         /* 0xf10 */
    int         LLIntStatus;            /* 0xf14 */
    int         LLIntEnable;            /* 0xf18 */
    int         LLRetry;                /* 0xf1c */
    int         LLStateMachine;         /* 0xf20 */
    int         LLErrorCounters;        /* 0xf24 */
    int         reserved_f28_ffc[54];  /* 0xf28-0xffc */
} ti_lynx_t;

/* This structure contains the self id data for one phy_ID */
typedef struct self_id_record_s {
    int link_active;
    int gap_cnt;
    int sp;
    int del;
    int c;
    int pwr;
    signed char port_status[27];
    int i;
    int m;
    signed char port_connection[27];
    long long Vendor_ID;
    long long Device_ID;
    long long UID;
}self_id_record_t;

/* This structure contains the self id data for an entire device tree */
typedef struct self_id_db_s {
    long long           bus_reset_sequence_number;
    int                 n_self_ids;
    int                 self_id_invalid;
    int                 our_phy_ID;
    int                 IRM_phy_ID; /* The current IRM phy_ID */
    self_id_record_t    self_id_tab[64];
} self_id_db_t;

/* Info stored for each vertex attached */
typedef struct info_s {
    vertex_hdl_t parent_vertex;
} info_t;

int      ti_lynx_devflag = D_MP;
char    *ti_lynx_mversion = M_VERSION;

ti_lynx_state_t *ti_lynx_state_ptr;

/**********************************************************
         Shared variables with ieee1394 driver
**********************************************************/

/* Required to dynamically call the correct lower-level driver  */
extern int (*board_driver_readreg)(int offset);
extern  void (*board_driver_writereg)(int offset, int val);
extern  int (*board_driver_get_mmapsize)(void);
extern  void (*board_driver_link_request)(ieee1394_transaction_t *t);
extern int (*board_driver_get_headerlist_info)(int op, int index, int **hdr,
                int **cyclecounter, int **len, int **datap);
extern int (*board_driver_get_isorcv_info)(int *tailp);
extern int (*board_driver_get_isoxmt_info)(int *headp);
extern  void (*board_driver_ReadOurSelfIDInfo)(int *PhyID, int *gap_cnt,
                int *sp, int *c, signed char port_status[27]);

/* XXX: experimental. please delete */
extern void (*board_driver_ioctl_test_isoxmt)(void *arg);

extern int shared_init_called;
extern mutex_t shared_init_lock;
extern int     reset_state;
extern int     bus_reset_state;
extern mutex_t bus_reset_lock;
extern sv_t    bus_reset_sv;
extern self_id_db_t tree_db;
extern mutex_t selfid_thread_started_lock;
extern sv_t    selfid_thread_started_sv;
extern int     selfid_thread_started;

