/* if_eex.h - Intel EtherExpress 16 interface header */

/* Copyright 1990-1993 Wind River Systems, Inc. */
/*
modification history
--------------------
01d,25jan95,hdn  added RB_OFFSET and RB_LINK for the Simpact's patch.
01c,20feb94,bcs  relabel, recomment extended control register, define AUTODETECT
		 and remove old connector type symbols
01b,04dec93,bcs  made functional, added AL_LOC compile-time option
01a,28nov93,bcs  created from if_ei.h (rev. 02h,22sep92)
*/

#ifndef __INCif_eexh
#define __INCif_eexh

#ifdef __cplusplus
extern "C" {
#endif

#if CPU_FAMILY==I960
#pragma align 1                 /* tell gcc960 not to optimize alignments */
#endif  /* CPU_FAMILY==I960 */

/* constants needed within this file */

#define EEX_AL_LOC                      /* define to use header-in-data */
                                        /* feature of 82586 */

#define EA_SIZE         6
#define EH_SIZE         14              /* avoid structure padding issues */
#define N_MCAST         12

/* Size of frame buffers in this driver
 * Made even to simplify structure alignment in code */

#define FRAME_SIZE      ( ( ETHERMTU + EH_SIZE + 1 ) & ~1 )

/* Intel EtherExpress 16 board port and bit definitions */

/* I/O ports relative to board base port address */
/* Word registers unless otherwise noted */

#define DXREG           0x00            /* Data transfer register */
#define WRPTR           0x02            /* Write address pointer */
#define RDPTR           0x04            /* Read address pointer */
#define CA_CTRL         0x06            /* (byte) Channel Attention */
#define SEL_IRQ         0x07            /* (byte) IRQ select and enable */
#define SMB_PTR         0x08            /* Shadow memory bank pointer */
#define MEMDEC          0x0a            /* (byte) Memory address decode */
#define MEMCTRL         0x0b            /* (byte) Memory mapped control */
#define MEMPC           0x0c            /* (byte) MEMCS16 page control */
#define CONFIG          0x0d            /* (byte) Configuration test */
#define EE_CTRL         0x0e            /* (byte) EEPROM control and reset */
#define MEMECTRL        0x0f            /* (byte) Memory control 0xE000 seg*/
#define AUTOID          0x0f            /* (byte) Auto ID register */

#define ECR1            0x300e          /* (byte) extended control: conn.type */
                                        /* only exists on newer boards */
#define SHADOWID        0x300f          /* (byte) board subtype/rev. */

#define SCB_STATUS      0xc008          /* SCB status word */
#define SCB_COMMAND     0xc00a          /* SCB command word */
#define SCB_CBL         0xc00c          /* SCB command block list head */
#define SCB_RFA         0xc00e          /* SCB received frame area */

/* Register bits and bit masks */

#define IRQ_SEL         0x07            /* SEL_IRQ: interrupt line code bits */
#define IRQ_ENB         0x08            /* SEL_IRQ: interrupt enable */

#define EEPROM_CLOCK    0x01            /* EE_CTRL: Shift clock pin */
#define EEPROM_CHIPSEL  0x02            /* EE_CTRL: Chip select for EEPROM */
#define EEPROM_OUTPUT   0x04            /* EE_CTRL: Data out to EEPROM */
#define EEPROM_INPUT    0x08            /* EE_CTRL: Data in from EEPROM */
#define RESET_ASIC      0x40            /* EE_CTRL: Reset board ASIC */
#define RESET_82586     0x80            /* EE_CTRL: Reset pin of 82586 */

/* defined bits in ECR1 port; leave others alone! */

#define CONN_INTEGRITY  0x02            /* bit in ECR1 to enable */
                                        /* link integrity for RJ-45 */
#define CONN_TRANSCEIVER 0x80           /* write-only bit to enable */
                                        /* on-board transceiver power */
                                        /* for BNC or RJ-45 connection */

/* Hyundai serial EEPROM operation codes */

#define EEPROM_OP_READ  0x06            /* 3 bits, read data */
#define EEPROM_OP_WRITE 0x05            /* 3 bits, write data */
#define EEPROM_OP_ERASE 0x07            /* 3 bits, erase data */
#define EEPROM_OP_EWEN  0x13            /* 5 bits, erase/write enable */
#define EEPROM_OP_EWDIS 0x10            /* 5 bits, erase/write enable */

/* EtherExpress 16 EEPROM memory registers */

#define EEX_EEPROM_SETUP        0       /* I/O, IRQ, AUI setup etc. */
#define EEX_EEPROM_MEMPAGE      1       /* mempage bits, base and select bits */
#define EEX_EEPROM_EA_LOW       2       /* low-order 16 bits of Ethernet addr.*/
#define EEX_EEPROM_EA_MID       3       /* middle 16 bits */
#define EEX_EEPROM_EA_HIGH      4       /* high-order 16 bits */
#define EEX_EEPROM_TPE_BIT      5       /* bit 0 sez if TPE in use */
#define EEX_EEPROM_MEMDECODE    6       /* mem decode bits, page 0xe000 bits */

/* Important bits in the EERPOM words */

#define SETUP_BNC               0x1000  /* set for BNC, clear for AUI/TPE */
#define MEMPAGE_AUTODETECT      0x0080  /* set for auto-detect of attachment */
#define TPE_BIT                 0x0001  /* set for TPE, clear for other */

/* data structure to convert memory setup EEPROM words into register bytes */

typedef union mem_setup
    {
    struct
        {
        UINT16  memPage;                /* from EEX_EEPROM_MEMPAGE */
        UINT16  memDecode;              /* from EEX_EEPROM_MEMDECODE */
        } wordView;
    struct
        {
        UINT8   memCtrl;                /* to MEMCTRL register */
        UINT8   memPC;                  /* to MEMPC register */
        UINT8   memDec;                 /* to MEMDEC register */
        UINT8   memECtrl;               /* to MEMECTRL register */
        } byteView;
    } MEM_SETUP;

/* Intel 82586 endian safe link macros and structure definitions */

#define LINK_WR(pLink,value)                                            \
        (((pLink)->lsw = (UINT16)((UINT32)(value) & 0x0000ffff)),       \
        ((pLink)->msw  = (UINT16)(((UINT32)(value) >> 16) & 0x0000ffff)))

#define LINK_RD(pLink) ((pLink)->lsw | ((pLink)->msw << 16))

#define STAT_RD LINK_RD                 /* statistic read is a link read */
#define STAT_WR LINK_WR                 /* statistic write is a link write */

typedef UINT16 EEX_SHORTLINK;           /* 82586 "offset" field */
typedef struct eex_link                         /* EEX_LINK - endian resolvable link */
    {
    UINT16              lsw;            /* least significant word */
    UINT16              msw;            /* most significant word */
    } EEX_LINK;

typedef struct eex_node                         /* EEX_NODE - common linked list object */
    {
    UINT16                              field1;
    UINT16                              field2;
    EEX_LINK                            lNext;                                          /* link to next node */
    EEX_LINK                            field4;
    UINT16                              field5;
    UINT16                              field6;
    UINT8                               field7 [EH_SIZE];
    char                                field8 [ETHERMTU];
        struct eex_node         *pNext;                                         /* ptr to next node */
    } EEX_NODE;

typedef UINT16 EEX_STAT;        /* EEX_STAT - 82586 error statistic */



/* Intel 82586 structure and bit mask definitions */

/* System Configuration Pointer and bit field defines */

typedef struct scp              /* SCP - System Configuration Pointer */
    {
    UINT16 scpSysbus;                   /* SYSBUS */
    UINT16 scpRsv1;                     /* reserved */
    UINT16 scpRsv2;                     /* reserved */
    EEX_LINK pIscp;                     /* ISCP address */
    } SCP;


/* Intermediate System Configuration Pointer */

typedef struct iscp             /* ISCP - Intermediate System Config. Ptr. */
    {
    volatile UINT16     iscpBusy;       /* i82586 is being initialized */
    EEX_SHORTLINK       offsetScb;      /* SCB offset */
    EEX_LINK            pScb;           /* SCB address */
    } ISCP;


/* System Control Block and bit field defines */

typedef struct scb              /* SCB - System Control Block */
    {
    volatile UINT16     scbStatus;      /* Status Word */
    volatile UINT16     scbCommand;     /* Command Word */
    EEX_SHORTLINK       pCB;            /* command block address */
    EEX_SHORTLINK       pRF;            /* receive frame area address */
    EEX_STAT            crcErr;         /* CRC error count */
    EEX_STAT            allignErr;      /* frames misaligned and CRC err cnt */
    EEX_STAT            noResErr;       /* no resources error count */
    EEX_STAT            ovErr;          /* overrun error count */
    } SCB;

#define SCB_S_RUMASK    0x00f0          /* state mask */
#define SCB_S_RUIDLE    0x0000          /* RU is idle */
#define SCB_S_RUSUSP    0x0010          /* RU is suspended */
#define SCB_S_RUNORSRC  0x0020          /* RU has no resources */
#define SCB_S_RURSV1    0x0030          /* reserved */
#define SCB_S_RUREADY   0x0040          /* RU is ready */
#define SCB_S_CUMASK    0x0f00          /* state mask */
#define SCB_S_CUIDLE    0x0000          /* CU is idle */
#define SCB_S_CUSUSP    0x0100          /* CU is suspended */
#define SCB_S_CUACTIVE  0x0200          /* CU is active */
#define SCB_S_CURSV1    0x0300          /* reserved */
#define SCB_S_CURSV2    0x0400          /* reserved */
#define SCB_S_CURSV3    0x0500          /* reserved */
#define SCB_S_CURSV4    0x0600          /* reserved */
#define SCB_S_CURSV5    0x0700          /* reserved */
#define SCB_S_XMASK     0xf000          /* state mask */
#define SCB_S_RNR       0x1000          /* RU left the ready state */
#define SCB_S_CNA       0x2000          /* CU left the active state */
#define SCB_S_FR        0x4000          /* RU finished receiveing a frame */
#define SCB_S_CX        0x8000          /* CU finished a cmd with I bit set */

#define SCB_C_RUNOP     0x0000          /* NOP */
#define SCB_C_RUSTART   0x0010          /* start reception of frames */
#define SCB_C_RURESUME  0x0020          /* resume reception of frames */
#define SCB_C_RUSUSPEND 0x0030          /* suspend reception of frames */
#define SCB_C_RUABORT   0x0040          /* abort receiver immediately */
#define SCB_C_RURSV1    0x0050          /* reserved */
#define SCB_C_RURSV2    0x0060          /* reserved */
#define SCB_C_RURSV3    0x0070          /* reserved */
#define SCB_C_RESET     0x0080          /* reset chip */
#define SCB_C_CUNOP     0x0000          /* NOP */
#define SCB_C_CUSTART   0x0100          /* start execution */
#define SCB_C_CURESUME  0x0200          /* resume execution */
#define SCB_C_CUSUSPEND 0x0300          /* suspend execution after cur. cmd */
#define SCB_C_CUABORT   0x0400          /* abort current cmd immediately */
#define SCB_C_CURSV1    0x0700          /* reserved */
#define SCB_C_ACK_RNR   0x1000          /* ACK that RU became not ready */
#define SCB_C_ACK_CNA   0x2000          /* ACK that CU bacame not active */
#define SCB_C_ACK_FR    0x4000          /* ACK that RU received a frame */
#define SCB_C_ACK_CX    0x8000          /* ACK that CU completed an action */


/* Action Command Descriptions */

typedef struct ac_iasetup       /* AC_IASETUP - Individual Address Setup */
    {
    UINT8  ciAddress[EA_SIZE];          /* local ethernet address */
    UINT16 ciFill;
    } AC_IASETUP;

typedef struct ac_config        /* AC_CONFIG - i82586 Configure */
    {
    UINT8 byteCount;                    /* byte count */
    UINT8 fifoLimit;                    /* FIFO limit */
    UINT8 srdy_saveBad;                 /* SRD/~ARDY, save bad frames */
    UINT8 addrLen_loopback;             /* address length, loopback */
    UINT8 backoff;                      /* backoff method */
    UINT8 interframe;                   /* interframe spacing */
    UINT8 slotTimeLow;                  /* slot time -low byte */
    UINT8 slotTimeHi_retry;             /* slot time -upper 3 bits, max retry */
    UINT8 promiscuous;                  /* promiscuous mode, other stuff */
    UINT8 carrier_collision;            /* carrier sense, collision detect */
    UINT8 minFrame;                     /* minimum frame length */
    UINT8 notUsed;
    UINT16 ccFill;
    } AC_CONFIG;

typedef struct ac_mcast         /* AC_MCAST - Multicast Setup */
    {
    UINT16 cmMcCount;                   /* the number of bytes in MC list */
    UINT8  cmAddrList[6 * N_MCAST];     /* mulitcast address list */
    } AC_MCAST;

typedef struct ac_tdr           /* AC_TDR - Time Domain Reflectometry */
    {
    UINT16 ctInfo;                      /* time, link OK, tx err, line err */
    UINT16 ctReserve1;                  /* reserved */
    } AC_TDR;

typedef struct ac_dump          /* AC_DUMP - Dump */
    {
    EEX_SHORTLINK bufAddr;              /* address of dump buffer */
    } AC_DUMP;


/* Command Frame Description and defines */

typedef struct cfd              /* CFD - Command Frame Descriptor */
    {
    volatile UINT16     cfdStatus;      /* command status */
    UINT16              cfdCommand;     /* command */
    EEX_SHORTLINK       link;           /* address of next CB */
    union                               /* command dependent section */
        {
        struct ac_iasetup       cfd_iasetup;    /* IA setup */
        struct ac_config        cfd_config;     /* config */
        struct ac_mcast         cfd_mcast;      /* multicast setup */
        struct ac_tdr           cfd_tdr;        /* TDR */
        struct ac_dump          cfd_dump;       /* dump */
        } cfd_cmd;
    } CFD;

#define cfdIASetup      cfd_cmd.cfd_iasetup
#define cfdConfig       cfd_cmd.cfd_config
#define cfdMcast        cfd_cmd.cfd_mcast
#define cfdTransmit     cfd_cmd.cfd_transmit
#define cfdTDR          cfd_cmd.cfd_tdr
#define cfdDump         cfd_cmd.cfd_dump

#define CFD_C_NOP       0x0000          /* No Operation */
#define CFD_C_IASETUP   0x0001          /* Individual Address Setup */
#define CFD_C_CONFIG    0x0002          /* Configure Chip */
#define CFD_C_MASETUP   0x0003          /* Multicast Setup */
#define CFD_C_XMIT      0x0004          /* Transmit (see below too ...) */
#define CFD_C_TDR       0x0005          /* Time Domain Reflectometry */
#define CFD_C_DUMP      0x0006          /* Dump Registers */
#define CFD_C_DIAG      0x0007          /* Diagnose */
#define CFD_C_INT       0x2000          /* 586 interrupts CPU after execution */
#define CFD_C_SE        0x4000          /* CU should suspend after execution */
#define CFD_C_EL        0x8000          /* End of command list */

#define CFD_S_ABORTED   0x1000          /* Command was aborted via CU Abort */
#define CFD_S_OK        0x2000          /* Command completed successfully */
#define CFD_S_BUSY      0x4000          /* CU is executing this command */
#define CFD_S_COMPLETE  0x8000          /* Command complete */


/* 82586 Transmit/Receive Frames */

typedef struct tfd              /* TFD - Transmit Frame Descriptor */
    {
    volatile UINT16     status;                 /* status field */
    UINT16              command;                /* command field */
    EEX_SHORTLINK       lNext;                  /* link to next desc. */
    EEX_SHORTLINK       lBufDesc;               /* link to buf desc. */
    } TFD;

/* special TFD specific command block bit masks */

#define CFD_S_COLL_MASK 0x000f          /* to access number of collisions */
#define CFD_S_RETRY     0x0020          /* reached the max number of retries */
#define CFD_S_HBEAT     0x0040          /* Heartbeat Indicator */
#define CFD_S_TRDEF     0x0080          /* Transmission Deferred */
#define CFD_S_DMA_UNDR  0x0100          /* DMA Underrun (no data) */
#define CFD_S_NO_CTS    0x0200          /* Lost Clear To Send signal */
#define CFD_S_NO_CRS    0x0400          /* No Carrier Sense */


typedef struct tbd              /* TBD - Transmit Buffer Descriptor */
    {
    volatile UINT16     actCount;       /* Actual byte count */
    EEX_SHORTLINK       lNext;          /* Address of next buffer descr. */
    EEX_LINK            lBufAddr;       /* Address of this data buffer */
    } TBD;

/* TBD bits */

#define ACT_COUNT_MASK  ~0xc000         /* length fields are 14 bits */
#define TBD_S_EOF       0x8000          /* End-of-frame bit */

#ifdef  EEX_AL_LOC
typedef struct tframe           /* TFRAME - all-in-one transmit frame */
    {
    TFD                 tfd;
    TBD                 tbd;
    char                buffer [FRAME_SIZE];
    } TFRAME;
#else
typedef struct tframe           /* TFRAME - all-in-one transmit frame */
    {
    TFD                 tfd;
    union
        {
        struct
            {
            char                header [EH_SIZE];
            char                data [ETHERMTU];
            } buff_struct;
        char                buffer [FRAME_SIZE];
        } buff_union;
    TBD                 tbd;
    } TFRAME;
#endif  /* EEX_AL_LOC */


typedef struct rfd              /* RFD - Receive Frame Descriptor */
    {
    volatile UINT16     status;                 /* status field */
    UINT16              command;                /* command field */
    EEX_SHORTLINK       lNext;                  /* link to next desc. */
    EEX_SHORTLINK       lBufDesc;               /* link to buf desc. */
    } RFD;

/* RFD bit masks */

#define RFD_S_EOP       0x0040          /* no EOP flag */
#define RFD_S_SHORT     0x0080          /* fewer bytes than configured min. */
#define RFD_S_DMA       0x0100          /* DMA Overrun failure to get bus */
#define RFD_S_RSRC      0x0200          /* received, but ran out of buffers */
#define RFD_S_ALGN      0x0400          /* received misaligned with CRC error */
#define RFD_S_CRC       0x0800          /* received with CRC error */
#define RFD_S_OK        0x2000          /* frame received successfully */
#define RFD_S_BUSY      0x4000          /* frame reception ready/in progress */
#define RFD_S_COMPLETE  0x8000          /* frame reception complete */

#define RFD_M_SUSPEND   0x4000          /* suspend RU after receiving frame */
#define RFD_M_EL        0x8000          /* end of RFD list */

typedef struct rbd              /* RBD - Receive Buffer Descriptor */
    {
    volatile UINT16     actCount;       /* Actual byte count */
    EEX_SHORTLINK       lNext;          /* Address of next buffer descr. */
    EEX_LINK            lBufAddr;       /* Address of this data buffer */
    UINT16              bufSize;        /* Size of data buffer */
    } RBD;

#define RBD_S_CNT_MASK  0x3fff          /* Mask for actual byte count */
#define RBD_S_F_BIT     0x4000          /* actual count is valid */
#define RBD_S_EOF       0x8000          /* end of frame */

#define RBD_M_EL        0x8000          /* end of RBD list */

#ifdef  EEX_AL_LOC
typedef struct rframe           /* RFRAME - all-in-one received frame */
    {
    RFD                 rfd;
    RBD                 rbd;
    char                buffer [FRAME_SIZE];
    } RFRAME;
#else
typedef struct rframe           /* RFRAME - all-in-one received frame */
    {
    RFD                 rfd;
    union
        {
        struct
            {
            char                header [EH_SIZE];
            char                data [ETHERMTU];
            } buff_struct;
        char                buffer [FRAME_SIZE];
        } buff_union;
    RBD                 rbd;
    } RFRAME;
#endif  /* EEX_AL_LOC */

/* Offsets into xFRAME structures to compute board emmory offsets */

#define RF_COMMAND      2               /* command word */
#define RF_LINK         4               /* link to next RFD */

#define RB_OFFSET       6               /* dhe 10/24/94 offset of first RBD */
#define RB_LINK         2               /* dhe 10/24/94 link to next RBD */

#ifdef  EEX_AL_LOC
#define RF_ACT_COUNT    8               /* actual count in RBD */
#define RF_BUFFER       18              /* frame data buffer */
#else
#define RF_BUFFER       8               /* frame data buffer */
#define RF_ACT_COUNT    (RF_BUFFER + FRAME_SIZE)
#endif  /* EEX_AL_LOC */

#define TF_COMMAND      2               /* command word */
#define TF_LINK         4               /* link to next TFD */

#ifdef  EEX_AL_LOC
#define TF_ACT_COUNT    8               /* actual count in TBD */
#define TF_BUFFER       16              /* frame data buffer */
#else
#define TF_BUFFER       8               /* frame header starts here */
#define TF_OLDLENGTH    20              /* Take length from here */
#define TF_NEWLENGTH    14              /* put it here to be part of TFD */
#define TF_ACT_COUNT    (RF_BUFFER + FRAME_SIZE)
#endif  /* EEX_AL_LOC */


#if CPU_FAMILY==I960
#pragma align 0                         /* turn off alignment requirement */
#endif  /* CPU_FAMILY==I960 */

#ifdef __cplusplus
}
#endif

#endif /* __INCif_eexh */

