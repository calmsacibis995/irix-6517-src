/* if_fn.h - structures and defines for the Fujitsu mb86960 NICE device */

/* Copyright 1992-1991 Wind River Systems, Inc. */
/*
01d,22sep92,rrr  added support for c++
modification history
--------------------
01c,16jun92,jwt  passed through the ansification filter
		  -changed copyright notice
01b,21feb92,rfs  modified for Fujitsu SPARClite EVIL board specific.
01a,03feb92,rfs  created
*/

#ifndef __INCif_fnh
#define __INCif_fnh

#ifdef __cplusplus
extern "C" {
#endif

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * SECTION: Data Structures
 *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

typedef struct rx_hdr                   /* received packet header */
    {
    unsigned short status;              /* only upper byte used */
    unsigned short len;                 /* pkt length in little endian order */
    } RX_HDR;
#define RX_HDR_SIZ  sizeof(RX_HDR)


/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * SECTION: Register Addresses
 *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


/* NICE registers, defined as offsets from base addr */

    /* The statically mapped registers.  This group of registers is
     * always available to the CPU.
     * This first group are the data link control registers 0-7.
     */

#define NICE_STATUS     0x02            /* DLC0,1: Tx and Rx status */
#define NICE_INTRMASK   0x06            /* DLC2,3: Tx and Rx intr masks */
#define NICE_MODE       0x0a            /* DLC4,5: Tx and Rx modes */
#define NICE_CONFIG     0x0e            /* DLC6,7: configuration bits */


    /* The selectable mapped registers.  These groups can be mapped in
     * or out of the overall register set.  The selection of which group
     * is currently mapped in is done via control register 2 (DLCR7) in
     * the statically mapped register group above.
     */


    /* the secondary data link control regs */

#define NICE_ADDR1      0x12            /* DLC8,9: node addr high word */
#define NICE_ADDR2      0x16            /* DLC10,11: node addr mid word */
#define NICE_ADDR3      0x1a            /* DLC12,13: node addr low word */
#define NICE_TDR        0x1e            /* DLC14,15: time domain counter */

    /* the hash table registers */

#define NICE_HASH1      0x12
#define NICE_HASH2      0x16
#define NICE_HASH3      0x1a
#define NICE_HASH4      0x1e

    /* the buffer memory registers */

#define NICE_PORT       0x12            /* BMR8: access port to buffer */
#define NICE_TRANSMIT   0x16            /* BMR10,11: Tx cmd & coll ctrl */
#define NICE_DMA        0x1a            /* BMR12,13: DMA control */
#define NICE_STUFF      0x1e



/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * SECTION: bit defines for the device registers and receive header status
 *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

/* The status field of the receive header (RX_HDR above) */

#define RX_HDR_STAT_BIT_7       0x8000      /* unused; should read as 0 */
#define RX_HDR_STAT_BIT_6       0x4000      /* unused; should read as 0 */
#define RX_HDR_STAT_GOOD        0x2000      /* packet is good */
#define RX_HDR_STAT_RMT_RST     0x1000      /* packet type = 0x0900 */
#define RX_HDR_STAT_SHRT_ERR    0x0800      /* short packet error */
#define RX_HDR_STAT_ALGN_ERR    0x0400      /* alignment error */
#define RX_HDR_STAT_CRC_ERR     0x0200      /* CRC error */
#define RX_HDR_STAT_OVR_FLO     0x0100      /* overflow error */

    /* Note that the short error bit has been observed to be set sporadically
     * and is therefore not usable.  The overflow bit should never be set,
     * since the receiver should not be giving us incomplete packets!  The
     * lower byte of the status field is currently unused, but has been seen
     * to have various bits set.  It is probably wise to simply mask this
     * byte off to avoid any confusion.
     */


/* DLCR0,1 -- the receive and transmit status register */

#define DLCR0_TX_DONE       0x8000      /* transmit OK */
#define DLCR0_NET_BSY       0x4000      /* network is busy, carrier present */
#define DLCR0_TX_RX         0x2000      /* transmit received good */
#define DLCR0_CR_LOST       0x1000      /* short packet */
#define DLCR0_BIT_3         0x0800      /* unused bit; write 0 */
#define DLCR0_COL           0x0400      /* a collision occurred */
#define DLCR0_16_COL        0x0200      /* 16 collisions occurred */
#define DLCR0_BUS_WR_ERR    0x0100      /* buss write error */

#define DLCR1_RX_PKT        0x0080      /* received packet ready */
#define DLCR1_BUS_RD_ERR    0x0040      /* buss read error */
#define DLCR1_DMA_EOP       0x0020      /* DMA end operation interrupt status */
#define DLCR1_RMT_RST       0x0010      /* received a remote reset packet */
#define DLCR1_SHRT_ERR      0x0008      /* short packet error */
#define DLCR1_ALGN_ERR      0x0004      /* alignment error */
#define DLCR1_CRC_ERR       0x0002      /* CRC error, CRC does not match FCS */
#define DLCR1_OVR_FLO       0x0001      /* overflow, no space available */


/* DLCR2,3 -- interrupt masks for receive and transmit status register */

#define DLCR2_TX_DONE       0x8000      /* transmit OK intr mask */
#define DLCR2_BIT_6         0x4000      /* unused; write 0 */
#define DLCR2_TX_RX         0x2000      /* transmission received good mask */
#define DLCR2_BIT_4         0x1000      /* unused; write 0 */
#define DLCR2_BIT_3         0x0800      /* unused; write 0 */
#define DLCR2_COL           0x0400      /* collision intr mask */
#define DLCR2_16_COL        0x0200      /* 16 collisions intr mask */
#define DLCR2_BUS_WR_ERR    0x0100      /* buss write error intr mask */

#define DLCR3_RX_PKT        0x0080      /* received packet ready intr mask */
#define DLCR3_BUS_RD_ERR    0x0040      /* buss read error intr mask */
#define DLCR3_DMA_EOP       0x0020      /* DMA end operation intr mask */
#define DLCR3_RMT_RST       0x0010      /* remote reset intr mask */
#define DLCR3_SHRT_ERR      0x0008      /* short packet error intr mask */
#define DLCR3_ALGN_ERR      0x0004      /* alignment error intr mask */
#define DLCR3_CRC_ERR       0x0002      /* CRC error intr mask */
#define DLCR3_OVR_FLO       0x0001      /* overflow error intr mask */


/* DLCR4,5 -- receiver and transmitter modes register */

#define DLCR4_COL_CTR       0xf000      /* collision counter, 4 bits */
#define DLCR4_BIT_3         0x0800      /* unused; write 0 */
#define DLCR4_CNTRL         0x0400      /* drives the CNTRL pin; pin 95 */
#define DLCR4_LBC           0x0200      /* endec loopback; 0 selects */
#define DLCR4_TX_NO_DEFER   0x0100      /* disable carrier detect on Tx */

#define DLCR5_BIT_7         0x0080      /* unused; write 0 */
#define DLCR5_BUF_EMPTY     0x0040      /* buffer empty */
#define DLCR5_ACPT_BAD      0x0020      /* accept bad packets */
#define DLCR5_SHRT_ADD      0x0010      /* use short address, 5 bytes */
#define DLCR5_ENA_SHRT_PKT  0x0008      /* enable receipt of short packets */
#define DLCR5_BIT_2         0x0004      /* unused; write 1 */
#define DLCR5_AF1           0x0002      /* addr filter mode, bit 1 */
#define DLCR5_AF0           0x0001      /* addr filter mode, bit 0 */


/* DLCR6,7 -- configuration register */

#define DLCR6_DISABLE_DLC   0x8000      /* disable data link controller */
#define DLCR6_BIT_6         0x4000      /* unused; write 1 */
#define DLCR6_SYS_BUS       0x2000      /* selects width of system bus */
#define DLCR6_SYS_BUS_8     0x2000      /*   system bus is 8 bit */
#define DLCR6_SYS_BUS_16    0x0000      /*   system bus is 16 bit */
#define DLCR6_BUF_BUS       0x1000      /* selects width of buffer bus */
#define DLCR6_BUF_BUS_8     0x1000      /*   buffer bus is 8 bits */
#define DLCR6_BUF_BUS_16    0x0000      /*   buffer bus is 16 bits */
#define DLCR6_TBS           0x0c00      /* selects Tx buffer size, 2 bits */
#define DLCR6_TBS_2KB       0x0000      /*   Tx buffer size is 2 KB */
#define DLCR6_TBS_4KB       0x0400      /*   Tx buffer size is 4 KB */
#define DLCR6_TBS_8KB       0x0800      /*   Tx buffer size is 8 KB */
#define DLCR6_TBS_16KB      0x0c00      /*   Tx buffer size is 16 KB */
#define DLCR6_BS            0x0300      /* selects SRAM buffer size, 2 bits */
#define DLCR6_BS_8KB        0x0000      /*   SRAM buffer size is 8 KB */
#define DLCR6_BS_16KB       0x0100      /*   SRAM buffer size is 16 KB */
#define DLCR6_BS_32KB       0x0200      /*   SRAM buffer size is 32 KB */
#define DLCR6_BS_64KB       0x0300      /*   SRAM buffer size is 64 KB */

#define DLCR7_CNF           0x00c0      /* selects chip config, 2 bits */
#define DLCR7_CNF_NICE      0x0000      /*   normal NICE mode */
#define DLCR7_CNF_MONITOR   0x0040      /*   add monitor function */
#define DLCR7_CNF_BYPASS    0x0080      /*   bypass ENDEC */
#define DLCR7_CNF_TEST      0x00c0      /*   test ENDEC */
#define DLCR7_PWRDN         0x0020      /* selects standby power  mode */
#define DLCR7_PWRDN_ON      0x0000      /*   standby mode on */
#define DLCR7_PWRDN_OFF     0x0020      /*   standby mode off */
#define DLCR7_BIT_4         0x0010      /* unused; write 1 */
#define DLCR7_REG_BNK       0x000c      /* selects register bank, 2 bits */
#define DLCR7_REG_BNK_DLC   0x0000      /*   DLC group 00 is mapped in */
#define DLCR7_REG_BNK_HASH  0x0004      /*   HASH group 01 is mapped in */
#define DLCR7_REG_BNK_BMR   0x000c      /*   BMR group 10 is mapped in */
#define DLCR7_EOP_POL       0x0002      /* selects polarity of EOP pin */
#define DLCR7_ENDIAN        0x0001      /* selects endian mode of system bus */
#define DLCR7_ENDIAN_LITTLE 0x0000      /*   endian mode is little */
#define DLCR7_ENDIAN_BIG    0x0001      /*   endian mode is big */


/* DLCR8-DLCR13 are the node ID registers when register group 00 is mapped in.
 * There are no bit defines for these registers.  Given an Ethernet address
 * of 11:22:33:44:55:66, the following shows the relationship to these
 * registers.
 *   dlcr8  = 0x11;
 *   dlcr9  = 0x22;
 *       .....
 *   dlcr13 = 0x66;
 */


/* DLCR14,DLCR15 are the time domain reflectometer registers when register
 * group 00 is mapped in.  There are no bit defines for these registers.
 * R14 holds the LSB, R15 the MSB, both are read only.
 */


/* BMR8 -- buffer memory port.  There are no bit defines for this register. */

/* BMR10,11 -- transmit collision control and start command register */

#define BMR10_TMST          0x8000      /* transmit start command */
#define BMR10_PKTS          0x7f00      /* number of packets to transmit */

#define BMR11_BIT7          0x0080      /* unused, write with 0 */
#define BMR11_BIT6          0x0040      /* unused, write with 0 */
#define BMR11_BIT5          0x0020      /* unused, write with 0 */
#define BMR11_BIT4          0x0010      /* unused, write with 0 */
#define BMR11_BIT3          0x0008      /* unused, write with 0 */
#define BMR11_MASK16        0x0004      /* something to do with 16 colls */
#define BMR11_RST_TX16      0x0002      /*    "  */
#define BMR11_OPT_16_COLL   0x0001      /*    "  */


/* BMR12,13 -- DMA control register */

#define BMR12_BIT7          0x8000      /* unused, write with 0 */
#define BMR12_BIT6          0x4000      /* unused, write with 0 */
#define BMR12_BIT5          0x2000      /* unused, write with 0 */
#define BMR12_BIT4          0x1000      /* unused, write with 0 */
#define BMR12_BIT3          0x0800      /* unused, write with 0 */
#define BMR12_BIT2          0x0400      /* unused, write with 0 */
#define BMR12_DMA_RENA      0x0200      /* DMA read enable */
#define BMR12_DMA_TENA      0x0100      /* DMA write enable */


/* BMR14 -- to be done */

/* BMR15 -- currently no function */

#ifdef __cplusplus
}
#endif

#endif /* __INCif_fnh */
