/* if_elt.h - 3Com EtherLink III network interface header*/ 

/* Copyright 1993 Wind River Systems, Inc. */

/*
modification history
--------------------
01e,07feb94,bcs  added net job queue statistics
01d,23jan94,bcs  added timing statistics
01c,16jan94,bcs  added rxearly to statistics
01b,08jan94,bcs  added statistics structure
01a,09dec93,bcs  written. 
*/


#ifndef __INCif_elth
#define __INCif_elth

#ifdef __cplusplus
extern "C" {
#endif


#define EA_SIZE         6               /* one Ethernet address */
#define EH_SIZE         14              /* avoid structure padding issues */
#define MAX_FRAME_SIZE  (EH_SIZE + ETHERMTU)    /* capacity of buffer */

/* ELT driver data structures */

typedef struct elt_frame                /* frame buffer, list element */
    {
    struct elt_frame *  lNext;          /* linked list pointer */
    u_char              refCnt;         /* reference count when loaned out */
    UINT16              count;          /* bytes left to transmit or */
                                        /* received so far; initially */
                                        /* length rounded up to dword */
                                        /* plus size of "preamble" */
    char *              nextByte;       /* next byte to transmit or */
                                        /* next byte to receive into */
                                        /* Following fields map to FIFO */
                                        /* transmit packet structure; */
                                        /* used same way for receive */
    UINT16              length;         /* length of contained frame */
                                        /* received frame includes error */
    UINT16              unassigned;     /* pad "preamble" to dword size */
    char                header [EH_SIZE];
    char                data [ETHERMTU];
    char                padToDword [MAX_FRAME_SIZE % sizeof (long)];
    } ELT_FRAME;


/* statistics block */

typedef struct 
    {
#define collisions      stat[0]
#define crcs            stat[1]
#define aligns          stat[2]
#define rxnobuffers     stat[3]
#define rxoverruns      stat[4]
#define disabled        stat[5]
#define deferring       stat[6]
#define txunderruns     stat[7]
#define aborts          stat[8]
#define outofwindow     stat[9]
#define heartbeats      stat[10]
#define badPacket       stat[11]
#define shortPacket     stat[12]
#define txnoerror       stat[13]
#define rxnoerror       stat[14]
#define txerror         stat[15]
#define rxerror         stat[16]
#define overwrite       stat[17]
#define wrapped         stat[18]
#define interrupts      stat[19]
#define reset           stat[20]
#define strayint        stat[21]
#define multcollisions  stat[22]
#define latecollisions  stat[23]
#define nocarriers      stat[24]
#define jabbers         stat[25]
#define txoverruns      stat[26]
#define rxunderruns     stat[27]
#define rxearly         stat[28]
#ifdef ELT_TIMING
#define timerUpdates    stat[29]
#define timerOverflow   stat[30]
#define timerInvalid    stat[31]
#define maxRxLatency    stat[32]
#define minRxLatency    stat[33]
#define maxIntLatency   stat[34]
#define taskQRxOuts     stat[35]
#define maxRxTaskQ      stat[36]
#define taskQTxOuts     stat[37]
#define maxTxTaskQ      stat[38]
    UINT stat[40];
#else
    UINT stat[30];
#endif /* ELT_TIMING */
    } ELT_STAT;

/* ELT register offsets, grouped by window number */

/* all windows */

#define ELT_COMMAND     0x0e            /* command register */
#define ELT_STATUS      0x0e            /* status register */

/* window number symbols for selection command */

#define WIN_CONFIG      0x0000
#define WIN_OPERATING   0x0001
#define WIN_ADDRESS     0x0002
#define WIN_FIFO        0x0003
#define WIN_DIAGNOSTIC  0x0004
#define WIN_RESULTS     0x0005
#define WIN_STATISTICS  0x0006

/* window 0, configuration and EEPROM */

#define MANUF_ID        0x00            /* manufacturer ID */
#define PRODUCT_ID      0x02            /* product ID/MCA adapter ID */
#define CONFIG_CONTROL  0x04            /* configuration control */
#define ADDRESS_CONFIG  0x06            /* address configuration */
#define RESOURCE_CONFIG 0x08            /* resource configuration */
#define EEPROM_CONTROL  0x0a            /* EEPROM control */
#define EEPROM_DATA     0x0c            /* EEPROM data in/out */

/* window 1, operating set */

#define DATA_REGISTER   0x00            /* data (low word) in/out */
#define DATA_HIGH       0x02            /* data (high word) in/out */
#define RX_STATUS       0x08            /* received packet status */
#define TIMER           0x0a            /* (byte) interrupt latency timer */
#define TX_STATUS       0x0b            /* (byte) transmit status */
#define TX_FREE_BYTES   0x0c            /* free bytes available in tx FIFO */
                                        /* also appears in window 3 */

/* window 2, station address setup/read */

#define ADDRESS_0       0x00            /* (byte) station address 0 */
#define ADDRESS_1       0x01            /* (byte) station address 1 */
#define ADDRESS_2       0x02            /* (byte) station address 2 */
#define ADDRESS_3       0x03            /* (byte) station address 3 */
#define ADDRESS_4       0x04            /* (byte) station address 4 */
#define ADDRESS_5       0x05            /* (byte) station address 5 */

/* window 3, FIFO management */

#define TX_RECLAIM      0x08            /* MCA tx reclaim threshold */
#define RX_FREE_BYTES   0x0a            /* free bytes available in rx FIFO */
#define TX_FREE_BYTES_3 0x0c            /* free bytes available in tx FIFO */
                                        /* also appears in window 1 */
                                        /* same offset so can use same macro */

/* window 4, diagnostics */

#define TX_DIAGNOSTIC   0x00            /* tx diagnostic */
#define HOST_DIAGNOSTIC 0x02            /* host diagnostic */
#define FIFO_DIAGNOSTIC 0x04            /* FIFO diagnostic */
#define NET_DIAGNOSTIC  0x06            /* net diagnostic */
#define ETHERNET_STATUS 0x08            /* ethernet controller status */
#define MEDIA_STATUS    0x0a            /* media type and status */

/* window 5, command results and internal state (read only) */

#define TX_START_THRESH 0x00            /* tx start threshold + 4 */
#define TX_AVAIL_THRESH 0x02            /* tx available threshold */
#define RX_EARLY_THRESH 0x06            /* rx early threshold */
#define RX_FILTER       0x08            /* rx filter lower 4 bits */
#define INTERRUPT_MASK  0x0a            /* interrupt mask */
#define READ_ZERO_MASK  0x0c            /* read zero mask */

/* window 6, statistics registers (byte regs. MUST be read/written as bytes) */

#define CARRIER_LOSTS   0x00            /* (byte) carrier loss during tx */
#define SQE_FAILURES    0x01            /* (byte) heartbeat loss during tx */
#define MULT_COLLISIONS 0x02            /* (byte) tx with multiple collis. */
#define ONE_COLLISIONS  0x03            /* (byte) tx with one collision */
#define LATE_COLLISIONS 0x04            /* (byte) tx with late collision */
#define RECV_OVERRUNS   0x05            /* (byte) receive overruns */
#define GOOD_TRANSMITS  0x06            /* (byte) frames transmitted OK */
#define GOOD_RECEIVES   0x07            /* (byte) frames received OK */
#define TX_DEFERRALS    0x08            /* (byte) transmit deferrals */
#define BYTES_RECEIVED  0x0a            /* total bytes received OK */
#define BYTES_TRANSMITTED       0x0c    /* total bytes transmitted OK */

/* ISA ID sequence state machine commands */

#define ID_PORT         0x0100          /* I/O port to access ID sequence */
                                        /* could be 0x110..0x1f0 but this */
                                        /* is a highly unlikely address to */
                                        /* be used by anything else */
#define ID_RESET        0x00            /* reset ID state machine */
#define ID_EEPROM_READ  0x80            /* read EEPROM register addressed by */
                                        /* last 6 bits of this command */
#define ID_EEPROM_MASK  0x3f            /* these bits here */
#define ID_SET_TAG      0xd0            /* set tag register to last 3 bits */
#define ID_TAG_MASK     0x07            /* values 1..7 take tagged adapter */
                                        /* out of the contention process */
#define ID_ACTIVATE     0xff            /* activate adapter as pre-configured */

#define ID_SEQUENCE_INITIAL     0xff    /* initial value in ID sequence */
#define ID_SEQUENCE_LENGTH      255     /* iteration count */
#define ID_CARRY_BIT    0x100           /* carry out of 8-bit ID value shift */
#define ID_POLYNOMIAL   0xcf            /* XOR this with ID value */
#define ID_REGISTER_SIZE        16      /* bits in an EEPROM register */

/* important EEPROM addresses */

#define EE_A_PRODUCT_ID 0x03            /* 3C5xx product id */
#define EE_A_MANUFACTURER       0x07    /* 3Com mfg. id == 0x6d50 */
#define EE_A_ADDRESS    0x08            /* address configuration */
#define EE_A_RESOURCE   0x09            /* resource configuration */
#define EE_A_OEM_NODE_0 0x0a            /* word 0 of OEM Ethernet address */
#define EE_A_OEM_NODE_1 0x0b            /* word 1 of OEM Ethernet address */
#define EE_A_OEM_NODE_2 0x0c            /* word 2 of OEM Ethernet address */

#define MANUFACTURER_ID 0x6d50          /* 3Com id code in EEPROM */

/* address configuration register fields and values */

#define AC_IO_BASE_MASK 0x001f          /* I/O base address encoding */
                                        /* 0-30 select n * 0x10 + 0x200 */
#define AC_IO_BASE_ZERO 0x0200          /* first encoded address */
#define AC_IO_BASE_FACTOR       0x10    /* convert code to address range */
#define AC_IO_BASE_EISA 0x001f          /* EISA addressing indicator */
#define AC_XCVR_MASK    0xc000          /* transciever selection field */
#define AC_XCVR_TPE     0x0000          /* select twisted-pair (10BASE-T) */
#define AC_XCVR_AUI     0x4000          /* select AUI (external transceiver) */
#define AC_XCVR_BNC     0xc000          /* select BNC (10BASE-2); must also */
                                        /* activate with START_COAX command */
/* configuration control register bits */

#define CC_ENABLE       0x0001          /* set to enable IRQ driver */
#define CC_RESET        0x0004          /* reset adapter to POR state */
#define CC_POR_INTERNAL_ENDEC   0x0100  /* use internal encoder/decoder */
#define CC_POR_10BASETAVAIL     0x0200  /* on-board TPE transceiver available */
#define CC_POR_TEST_MASK        0x0c00  /* test mode bits */
#define CC_POR_TEST_RECEIVE     0x0400  /* receive test mode */
#define CC_POR_TEST_TRANSMIT    0x0800  /* transmit test mode */
#define CC_POR_TEST_NORMAL      0x0c00  /* normal operation mode */
#define CC_POR_10BASE2_AVAIL    0x1000  /* on-board coax transceiver available*/
#define CC_POR_AUI_AVAIL        0x2000  /* on-board AUI connector evailable */

/* FIFO diagnostic register bits */

#define FD_TX_BC        0x0001          /* TX BIST is complete */
#define FD_TX_BF        0x0002          /* TX BIST has failed */
#define FD_TX_BFC       0x0004          /* sets TX BF to test for stuck-at */
#define FD_TX_BIST      0x0008          /* enable BIST in TX FIFO RAM */
#define FD_RX_BC        0x0010          /* TRX BIST is complete */
#define FD_RX_BF        0x0020          /* RX BIST has failed */
#define FD_RX_BFC       0x0040          /* sets RX BF to test for stuck-at */
#define FD_RX_BIST      0x0080          /* enable BIST in RX FIFO RAM */
#define FD_TX_OVERRUN   0x0400          /* host wrote too much data */
#define FD_RX_OVERRUN   0x0800          /* RX overrun (not necessarily */
                                        /* packet overrun yet) */
#define FD_RX_STATUS_OVERRUN    0x1000  /* already 8 packets in RX FIFO */
#define FD_RX_UNDERRUN  0x2000          /* host read past end of packet */
#define FD_RX_RECEIVING 0x8000          /* packet being received now */

/* media type and status bits */

#define MT_S_SQE_ENABLE 0x0008          /* enable SQE error detection */
#define MT_S_COLLISION  0x0010          /* (?) collision in progress */
#define MT_S_CARRIER    0x0020          /* (?) carrier sensed currently */
#define MT_S_JABBER_ENABLE      0x0040  /* enable jabber and polarity detect */
#define MT_S_LINK_BEAT_ENABLE   0x0080  /* enable TPE link beat */
#define MT_S_UNSQUELCH  0x0100          /* unsquelch status */
#define MT_S_JABBER     0x0200          /* jabber status */
#define MT_S_POLARITY   0x0400          /* polarity swap status */
#define MT_S_LINK_BEAT  0x0800          /* link beat correct status */
#define MT_S_SQE        0x1000          /* SQE present status */
#define MT_S_INTERNAL_ENDEC     0x2000  /* internal encoder/decoder in use */
#define MT_S_BNC        0x4000          /* BNC transceiver in use */
#define MT_S_AUI_DISABLE        0x8000  /* AUI interface disabled */

/* command opcodes (uppper 5 bits of command register) */

#define GLOBAL_RESET    0x0000          /* global reset (powerup equiv.) */
#define SELECT_WINDOW   0x0800          /* select register window */
#define START_COAX      0x1000          /* start coaxial transciever */
#define RX_DISABLE      0x1800          /* disable ethernet receiver */
#define RX_ENABLE       0x2000          /* enable ethernet receiver */
#define RX_RESET        0x2800          /* reset ethernet receiver */
#define RX_DISCARD      0x4000          /* discard remainder of top FIFO pkt */
#define TX_ENABLE       0x4800          /* enable ethernet transmitter */
#define TX_DISABLE      0x5000          /* disable ethernet transmitter */
#define TX_RESET        0x5800          /* reset ethernet transmitter */
#define REQ_INTERRUPT   0x6000          /* set interrupt requested bit */
#define ACK_INTERRUPT   0x6800          /* acknowledge interrupt conditions */
#define MASK_INTERRUPT  0x7000          /* set interrupt mask bits */
#define MASK_STATUS     0x7800          /* set status mask bits */
                                        /* the "read zero mask" */
#define SET_RX_FILTER   0x8000          /* set receive filter bits */
#define SET_RX_THRESH   0x8800          /* set receive early threshold */
#define SET_TX_AVAIL    0x9000          /* set transmit available threshold */
#define SET_TX_START    0x9800          /* set transmit start threshold */
#define STATS_ENABLE    0xa800          /* enable statistics collection */
#define STATS_DISABLE   0xb000          /* disable statistics collection */
#define STOP_COAX       0xb800          /* stop coaxial transciever */
#define SET_TX_RECLAIM  0xc000          /* (MCA) set tx reclaim threshold */

/* receive filter command argument bits */

#define RX_F_STATION_IA 0x01            /* accept individual address */
#define RX_F_MULTICAST  0x02            /* accept multicast (group) addresses */
#define RX_F_BROADCAST  0x04            /* accept broadcast address */
#define RX_F_PROMISCUOUS        0x08    /* accept all addresses */
#define RX_F_NORMAL     (RX_F_STATION_IA | RX_F_BROADCAST)

/* board status bits (also acknowledge and mask bits) */

#define INTERRUPT_LATCH 0x0001          /* raise interrupt to host */
#define ADAPTER_FAILURE 0x0002          /* error unrecoverable by adapter */
                                        /* tx overrun, rx underrun, or */
                                        /* hypothetical hardware errors */
#define TX_COMPLETE     0x0004          /* tx finished with error or */
                                        /* packet interrupt bit was set */
#define TX_AVAILABLE    0x0008          /* tx available threshold exceeded */
#define RX_COMPLETE     0x0010          /* a complete packet is available */
#define RX_EARLY        0x0020          /* rx early threshold exceeded */
#define INTERRUPT_REQ   0x0040          /* set by request interrupt command */
#define UPDATE_STATS    0x0080          /* one or more statistics counters */
                                        /* needs to be flushed */
#define STATUS_MASK     0x00ff          /* pick status bits out of register */
#define COMMAND_IN_PROGRESS     0x1000  /* last command still being processed */
#define WINDOW_MASK     0xe000          /* current register window selection */

/* transmit status register bits */

#define TX_S_RECLAIM    0x02            /* tx reclaim (MCA only) */
#define TX_S_OVERFLOW   0x04            /* tx status overflow (lost info) */
#define TX_S_MAX_COLL   0x08            /* maximum collisions reached */
#define TX_S_UNDERRUN   0x10            /* underrun, tx reset required */
#define TX_S_JABBER     0x20            /* jabber error, tx reset required */
#define TX_S_INTERRUPT  0x40            /* interrupt on successful xmit */
#define TX_S_COMPLETE   0x80            /* transmission complete */
                                        /* This bit denotes a valid status */
                                        /* when set; valid status is popped */
                                        /* off the stack by a write cycle to */
                                        /* the transmit status register */

/* transmit free bytes constants */

#define TX_IDLE_COUNT   0x7f8           /* 2,040; supposed to be 2,044 */
                                        /* use a >= test to see if tx idle */

/* receive status register bits */

#define RX_S_CNT_MASK   0x07ff          /* received byte count field */
#define RX_S_CODE_MASK  0x3800          /* error code field */
#define RX_S_OVERRUN    0x0000          /* overrun error */
#define RX_S_RUNT       0x1800          /* runt packet error */
#define RX_S_ALIGN      0x2000          /* alignment (framing) error */
#define RX_S_CRC        0x2800          /* CRC error */
#define RX_S_OVERSIZE   0x0800          /* oversize packet error (>1514) */
#define RX_S_DRIBBLE    0x1000          /* dribble bit(s); this code is */
                                        /* valid when error bit is not set */
#define RX_S_ERROR      0x4000          /* error in rx packet, code above */
#define RX_S_INCOMPLETE 0x8000          /* packet is incomplete or FIFO empty */
                                        /* opposite of transmit status! */

/* transmit packet preamble bits and fields */

#define TX_F_LEN_MASK   0x07ff          /* length field */
#define TX_F_INTERRUPT  0x8000          /* interrupt on transmit complete */
#define TX_F_DWORD_MASK 0xfffc          /* mask to round lengths to dword */
#define TX_F_PREAMBLE_SIZE      4       /* size of packet length "preamble" */

#ifdef __cplusplus
}
#endif

#endif  /* __INCif_elth */
