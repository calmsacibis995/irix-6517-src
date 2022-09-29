#ifndef INCwan_controlh
#define INCwan_controlh

/*-----------------------------------------------------------------------------
 * wan_control.h - SpiderX25 Protocol Streams Component
 *
 *  Copyright 1991  Spider Systems Limited
 *
 * Copyrighted as an unpublished work.
 * (c) 1992-96 SBE, Inc. and by other vendors under license agreements.
 * All Rights Reserved.
 *-----------------------------------------------------------------------------
 * RCS info:
 * RCS revision: $Revision: 1.6 $
 * Last changed on $Date: 1996/07/15 18:14:03 $
 * Changed by $Author: wli $
 *-----------------------------------------------------------------------------
 * $Log: wan_control.h,v $
 * Revision 1.6  1996/07/15 18:14:03  wli
 * new header file to support SBE PCI board only.
 *
 * Revision 2.0  96/04/08  15:31:20  rickd
 * Updated to support PCI-360 WAN-Only using Shared Memory Buffers.
 * All chars/shorts promoted to longs to make Little Endian byte-swap
 * of cross-bus values (IE.ioctls) easier to maintain and to code for in the
 * PDM (Protocol Download Module).  Clean LOG messages. Redo Copyright notice.
 * 
 *-----------------------------------------------------------------------------
 */

#include <sys/types.h>

#define W_SETTUNE       ('W'<<8 | 1)    /* set tuning parameters (use
                                         * wan_tnioc) */
#define W_GETTUNE       ('W'<<8 | 2)    /* get tuning parameters (use
                                         * wan_tnioc) */
#define W_PUTWANMAP     ('W'<<8 | 3)
#define W_GETWANMAP     ('W'<<8 | 4)
#define W_DELWANMAP     ('W'<<8 | 5)
#define W_FORCEMOD      ('W'<<8 | 6)    /* SBE Force up/down all signals */
#define W_MOD_SIG       ('W'<<8 | 7)    /* SBE Get/Set Modem Signals */

#define LI_WANSTATS     0x34   /* Indicates 'struct wan_stioc' */
#define LI_WANTUNE      0x35   /* Indicates 'struct wan_tnioc' */
#define LI_WANMOD       0x36   /* Indicates 'struct wanmodctl' */
#define LI_WANMODSIG    0x37   /* Indicates 'struct wan_modsig' */
#define LI_WANMAP       0x38   /* Indicates 'struct wan_mpioc' */


#define FORCE_UP        1
#define FORCE_DOWN      0

#define GET_MOD_SIG     0      /* SBE Get Modem Signals [W_MOD_SIG] */
#define SET_MOD_SIG     1      /* SBE Set Modem Signals [W_MOD_SIG] */

/*
    HDLC statistics (one per channel)
*/
typedef struct hstats
{
    uint32_t hc_txgood;          /* Number of good frames transmitted    */
    uint32_t hc_txurun;          /* Number of transmit underruns         */
    uint32_t hc_txctslost;       /* Number of transmit cts lost          */

    uint32_t hc_rxgood;          /* Number of good frames received       */
    uint32_t hc_rxorun;          /* Number of receive overruns           */
    uint32_t hc_rxcrc;           /* Number of receive CRC/Framing errors */
    uint32_t hc_rxnobuf;         /* Number of Rx frames with no buffer   */
    uint32_t hc_rxnflow;         /* Number of Rx frames with no flow ctl */
    uint32_t hc_rxoflow;         /* Number of receive buffer overflows   */
    uint32_t hc_rxabort;         /* Number of receive aborts             */
    uint32_t hc_rxcnterr;        /* Rx Counter Errors for VCOM34         */
    uint32_t hc_rxcdlost;        /* Number of receive cd lost            */
    uint32_t hc_rxalignerr;      /* Number of receive align error        */
}  hdlcstats_t;

/*
    Ioctl block for WAN L_GETSTATS command
*/
struct wan_stioc
{
    uint32_t lli_type;           /* Table type = LI_WANSTATS      */
    uint32_t lli_snid;           /* Subnetwork ID character       */
    hdlcstats_t hdlc_stats;    /* Table of HDLC stats values    */
};



/*
    Values for WAN_options field
*/
#define TRANSLATE       1      /* Translate to interface address   */

/*
    Values for WAN_procs field

    N.b. these values are used to directly access the procedural interface
    table for all supported calling procedures. So do not change these values
    without changing the access method for the table.
*/
#define WAN_NONE        0      /* No calling procedures        */
#define WAN_X21P        1      /* X21 calling procedures       */
#define WAN_V25bis      2      /* V25bis calling procedures    */
#define WAN_TRAN      128      /* transparent mode - no call prod. */

/*
    Values for WAN_interface field
*/
#define WAN_X21         0
#define WAN_V28         1
#define WAN_V35         2

/*
    Values for WAN_phy_hw field
*/
#define PH_MASK                0x000f
#define PH_RES0                0x0000
#define PH_RES1                0x0001
#define PH_V_35                0x0002
#define PH_X_21                0x0003
#define PH_RS_232              0x0004
#define PH_EIA_530             0x0005
#define PH_NOT_AVAL            0x0006
#define PH_NONE                0x0007

#define PH_CAB_MASK            0xf0000
#define PH_CAB_RES0            0x00000
#define PH_CAB_RES1            0x10000
#define PH_CAB_V_35            0x20000
#define PH_CAB_X_21            0x30000
#define PH_CAB_RS_232          0x40000
#define PH_CAB_EIA_530         0x50000
#define PH_CAB_RES6            0x60000
#define PH_CAB_NONE            0x70000


/* defines for WAN_scc_opts in wantune_t WAN_hddef_t */
#define SCC_NRZ         0
#define SCC_NRZI        1
#define SCC_NRZI_MARK   1
#define SCC_NRZI_SPACE  2



/*
 * Transparent Mode Parameters
 */
struct WAN_tpx
{
    uint32_t WAN_cptype;         /* Variant type. (WAN_TRAN)  */
    uint32_t tp_txidlp;          /* tx idle pattern */
    uint32_t tp_txsynp;          /* tx synchronize pattern */
    uint32_t tp_txsynl;          /* tx synchronize pattern bit len */
    uint32_t tp_txflag;          /* tx flags */
    uint32_t tp_rxidlp;          /* rx idle pattern */
    uint32_t tp_rxsynp;          /* rx synchronize pattern */
    uint32_t tp_rxsynl;          /* rx synchronize pattern bit len */
    uint32_t tp_rxflag;          /* rx flags */
};



/*
        This contains all of the national network specific timeouts.
*/
struct WAN_x21
{
    uint32_t WAN_cptype;         /* Variant type.                        */
    uint32_t T1;                 /* Timer for call request state.        */
    uint32_t T2;                 /* Timer for EOS to data transfer.      */
    uint32_t T3A;                /* Timer for call progress signals.     */
    uint32_t T4B;                /* Timer for DCE provided info.         */
    uint32_t T5;                 /* Timer for DTE clear request.         */
    uint32_t T6;                 /* Timer for DTE clear conf. state.     */
};

/*
        This contains all of the national network specific timeouts.
*/
struct WAN_v25
{
    uint32_t WAN_cptype;         /* Variant type.        */
    uint32_t callreq;            /* Abort time for call request command if
                                * network does not support CFI.     */
};


/*
        This is the structure which contains all tunable information
        which is passed from user space to HDLC.
*/
#define CP_PADSIZE   64

struct WAN_hddef
{
    uint32_t WAN_baud;           /* WAN baud rate                */
    uint32_t WAN_maxframe;       /* WAN maximum frame size       */
    uint32_t WAN_interface;      /* WAN physical interface       */
    uint32_t WAN_phy_if;         /* DTE or DCE                   */
    uint32_t WAN_loopback;       /* loopback                     */
    uint32_t WAN_phy_hw;         /* Phy. Hw 232/449/X.21 etc     */
    uint32_t WAN_split_chn;      /* Spilt Channel for E1 support */
    uint32_t WAN_scc_opts;       /* SCC/USCC options NRZ[I]      */
    union
    {
        uint32_t WAN_cptype;     /* Variant type.                */
        struct WAN_x21 WAN_x21def;
        struct WAN_v25 WAN_v25def;
        struct WAN_tpx WAN_tran;        /* Transparent mode configuration */
        uint8_t  WAN__pad[CP_PADSIZE];     /* pad - allows for new defs */
    }  WAN_cpdef;              /* WAN call procedural definition for hardware
                                * interface.      */
};

typedef struct WAN_hddef WAN_hddef_t;

/*
    WAN tuning structure
*/
typedef struct wantune
{
    uint32_t WAN_options;        /* WAN options           */
    struct WAN_hddef WAN_hd;   /* HD information.       */
}  wantune_t;

/*
    Ioctl block for WAN W_SETTUNE command
*/
struct wan_tnioc
{
    uint32_t lli_type;           /* Table type = LI_WANTUNE                */
    uint32_t lli_snid;           /* subnetwork id character ('*' => 'all') */
    wantune_t wan_tune;        /* Table of tuning values                 */
};

/*
    WAN address mapping structure
*/
typedef struct wanmapf
{
    uint32_t remsize;            /* Rem address size (octets)        */
    uint8_t remaddr[20];         /* Remote address                   */
    uint32_t infsize;            /* Interface address size  (octets) */
    uint8_t infaddr[30];         /* Interface address                */
}  wanmap_t;

#define WAN_MAP_SIZE   (MAXIOCBSZ - 8)
#define MAX_WAN_ENTS   (WAN_MAP_SIZE / sizeof(wanmap_t))
#define MAX_WAN_ADDRS   20
#define NWANS           8
#define MAX_SUBNETS     NWANS

/*
    Ioctl block for WAN GET MAP command
*/
typedef struct wangetf
{
    uint32_t first_ent;          /* Where to start search        */
    uint32_t num_ent;            /* Number entries returned      */
    wanmap_t entries[MAX_WAN_ENTS];     /* Data buffer      */
}  wanget_t;

struct wanmapgf
{
    uint32_t lli_type;           /* Always LI_WANMAP        */
    uint32_t lli_snid;           /* Subnetwork id character */
    wanget_t wan_ents;         /* Returned structure      */
};

/*
    Ioctl block for WAN PUT MAP command
*/
struct wanmappf
{
    uint32_t lli_type;           /* Always LI_WANMAP        */
    uint32_t lli_snid;           /* Subnetwork id character */
    wanmap_t wan_ent;          /* Mapping entry           */
};

/*
    Ioctl block for WAN DEL MAP command
*/
struct wanmapdf
{
    uint32_t lli_type;           /* Always LI_WANMAP        */
    uint32_t lli_snid;           /* Subnetwork id character */
};

/*
 *  SBE - Ioctl block for WAN FORCE MODEM CONTROL command
 */
struct wanmodctl
{
    uint32_t lli_type;           /* Always LI_WANMOD        */
    uint32_t ctrl;               /* Up or Down              */
    uint32_t lli_snid;           /* Subnetwork id character */
};

/*
 *  SBE - Ioctl block for WAN GET / SET MODEM Signals command
 *      Added by SBE for Modem Support.
 *      Note:   All Modem signals are NOT supported on all boards.
 *              It is up to the application to know supported signals
 *              and signal direction based on the ports DTE/DCE
 *              configuration.
 */
/* Bit map for modem_sigs and modem_mask */
#define M_TI    0x0001         /* Test Indicator */
#define M_LL    0x0002         /* Local Loopback */
#define M_MB    0x0004         /* Make Busy */
#define M_RLSD  0x0008         /* Rx Sig. Detect */
#define M_RI    0x0010         /* Ring Indicator */
#define M_DTR   0x0020         /* Data Term Ready */
#define M_DSR   0x0040         /* Data Set Ready */
#define M_SP    0x0080         /* Spare */
#define M_RTS   0x0100         /* Request to Send */
#define M_CTS   0x0200         /* Clear to Send */
#define M_DCD   0x0400         /* Carrier Detect */
#define M_DCE   0x8000         /* DCE Port PCI-360 Read Only */
struct wan_modsig
{
    uint32_t          lli_type;       /* Always LI_WANMODSIG        */
    uint32_t          ctrl;           /* GET_MOD_SIG or SET_MOD_SIG */
    uint32_t          modem_sigs,     /* See above M_??? for bit map */
                    modem_mask;     /* Modem signals we care about */
    uint32_t          lli_snid;       /* Subnetwork id character */
};

/*
 * modem signal masks for ISU44 board -
 * the ISU44 has the advantage of knowing the selected
 * electrical interface type for each channel and may
 * therefore use the appropriate mask
 */
#define M_ISU44_MASK_X_21       M_CTS + M_RTS
#define M_ISU44_MASK_V_35       M_CTS + M_DCD + M_DSR + M_RTS + M_DTR
#define M_ISU44_MASK_EIA_232    M_CTS + M_DCD + M_DSR + M_RI + M_RTS + M_DTR

/*
 * VCOM-25 Modem masks
 * Note:  - Since LIM/SCI are designed to be transparent, it is impossible
 *          to distinguish the physical interface (LIM/SCI) on any
 *          VCOM board.  Therefore the modem signal interface is generic
 *          as possible to accommodate all situations.
 *        - On the VCOM-25, RTS and CTS are only controllable if the XCF
 *          parameter AUTO_RTS = OFF      (See xcf configuration files.)
 */
#define M_VC25_SIO              M_TI + M_LL + M_MB + M_RLSD + M_RI + M_DTR + M_DSR + M_SP
#define M_VC25_SET_DTE          M_VC25_SIO + M_RTS
#define M_VC25_SET_DCE          M_VC25_SIO + M_CTS + M_DCD
#define M_VC25_GET_DTE          M_VC25_SIO + M_CTS + M_DCD
#define M_VC25_GET_DCE          M_VC25_SIO + M_RTS


/*
 * PCI-360 Modem masks
 * Note:  - This board can detect phy hardware for DTE or DCE operation
 *          to distinguish the physical interface (LIM/SCI) on any
 *          VCOM board.  Therefore the modem signal interface is generic
 *          as possible to accommodate all situations.
 *        - On the VCOM-25, RTS and CTS are only controllable if the XCF
 *          parameter AUTO_RTS = OFF      (See xcf configuration files.)
 */
#define M_VC25_SIO              M_TI + M_LL + M_MB + M_RLSD + M_RI + M_DTR + M_DSR + M_SP
#define M_VC25_SET_DTE          M_VC25_SIO + M_RTS
#define M_VC25_SET_DCE          M_VC25_SIO + M_CTS + M_DCD
#define M_VC25_GET_DTE          M_VC25_SIO + M_CTS + M_DCD
#define M_VC25_GET_DCE          M_VC25_SIO + M_RTS

#endif                              /* INCwan_controlh */

/*
 *  End Of File
 */
