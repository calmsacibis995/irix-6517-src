/* if_ene.h - Novell/Eagle 2000 network interface header */ 

/* Copyright 1994 Wind River Systems, Inc. */

/*
modification history
--------------------
01a,01jan94,bcs  written. 
*/


#ifndef __INCif_eneh
#define __INCif_eneh

#ifdef __cplusplus
extern "C" {
#endif

#define EA_SIZE         6               /* one Ethernet address */
#define EH_SIZE         14              /* Ethernet header size */
#define MAX_FRAME_SIZE  (EH_SIZE + ETHERMTU)

/* NE 2000 on-board memory page addresses */

#define NE2000_CONFIG_PAGE      0x00    /* where the Ethernet address is */
#define NE2000_EADDR_LOC        0x00    /* location within config. page */
#define NE2000_TSTART           0x40
#define NE2000_PSTART           0x46
#define NE2000_PSTOP            0x80

#define ENE_PAGESIZE            256

typedef struct 
    {
    UCHAR rstat;
    UCHAR next;
    UCHAR lowByteCnt;
    UCHAR uppByteCnt;
    } ENE_HEADER;

typedef struct 
    {
#define collisions      stat[0]
#define crcs            stat[1]
#define aligns          stat[2]
#define missed          stat[3]
#define overruns        stat[4]
#define disabled        stat[5]
#define deferring       stat[6]
#define underruns       stat[7]
#define aborts          stat[8]
#define outofwindow     stat[9]
#define heartbeats      stat[10]
#define badPacket       stat[11]
#define shortPacket     stat[12]
#define tnoerror        stat[13]
#define rnoerror        stat[14]
#define terror          stat[15]
#define rerror          stat[16]
#define overwrite       stat[17]
#define wrapped         stat[18]
#define interrupts      stat[19]
#define reset           stat[20]
#define strayint        stat[21]
#define jabber          stat[22]
    UINT stat[24];
    } ENE_STAT;


#define CAST

/* NS 8390 (Ethernet LAN Controller) */ 

#define ENE_REG_ADDR_INTERVAL   1
#define ENE_ADRS(base,reg)   (CAST (base+(reg*ENE_REG_ADDR_INTERVAL)))

/* page-0, read */

#define ENE_CMD(base)           ENE_ADRS(base,0x00)     /* command */
#define ENE_TRINCRL(base)       ENE_ADRS(base,0x01)     /* TRDMA incrementer */
#define ENE_TRINCRH(base)       ENE_ADRS(base,0x02)     /* TRDMA incrementer */
#define ENE_BOUND(base)         ENE_ADRS(base,0x03)     /* boundary page */
#define ENE_TSTAT(base)         ENE_ADRS(base,0x04)     /* transmit status */
#define ENE_COLCNT(base)        ENE_ADRS(base,0x05)     /* collision error */
#define ENE_INTSTAT(base)       ENE_ADRS(base,0x07)     /* interrupt status */
#define ENE_RSTAT(base)         ENE_ADRS(base,0x0c)     /* receive status */
#define ENE_ALICNT(base)        ENE_ADRS(base,0x0d)     /* alignment error */
#define ENE_CRCCNT(base)        ENE_ADRS(base,0x0e)     /* crc error */
#define ENE_MPCNT(base)         ENE_ADRS(base,0x0f)     /* missed packet */

/* page-0, write */

#define ENE_RSTART(base)        ENE_ADRS(base,0x01)     /* receive start */
#define ENE_RSTOP(base)         ENE_ADRS(base,0x02)     /* receive stop */
#define ENE_TSTART(base)        ENE_ADRS(base,0x04)     /* transmit start */
#define ENE_TCNTL(base)         ENE_ADRS(base,0x05)     /* transmit counter */
#define ENE_TCNTH(base)         ENE_ADRS(base,0x06)     /* transmit counter */
#define ENE_RSAR0(base)         ENE_ADRS(base,0x08)     /* remote address */
#define ENE_RSAR1(base)         ENE_ADRS(base,0x09)     /* remote address */
#define ENE_RBCR0(base)         ENE_ADRS(base,0x0a)     /* remote byte count */
#define ENE_RBCR1(base)         ENE_ADRS(base,0x0b)     /* remote byte count */
#define ENE_RCON(base)          ENE_ADRS(base,0x0c)     /* receive config */
#define ENE_TCON(base)          ENE_ADRS(base,0x0d)     /* transmit config */
#define ENE_DCON(base)          ENE_ADRS(base,0x0e)     /* data config */
#define ENE_INTMASK(base)       ENE_ADRS(base,0x0f)     /* interrupt mask */

/* page-1, read and write */

#define ENE_STA0(base)          ENE_ADRS(base,0x01)     /* station address */
#define ENE_STA1(base)          ENE_ADRS(base,0x02)     /* station address */
#define ENE_STA2(base)          ENE_ADRS(base,0x03)     /* station address */
#define ENE_STA3(base)          ENE_ADRS(base,0x04)     /* station address */
#define ENE_STA4(base)          ENE_ADRS(base,0x05)     /* station address */
#define ENE_STA5(base)          ENE_ADRS(base,0x06)     /* station address */
#define ENE_CURR(base)          ENE_ADRS(base,0x07)     /* current page */
#define ENE_MAR0(base)          ENE_ADRS(base,0x08)     /* multicast address */
#define ENE_MAR1(base)          ENE_ADRS(base,0x09)     /* multicast address */
#define ENE_MAR2(base)          ENE_ADRS(base,0x0a)     /* multicast address */
#define ENE_MAR3(base)          ENE_ADRS(base,0x0b)     /* multicast address */
#define ENE_MAR4(base)          ENE_ADRS(base,0x0c)     /* multicast address */
#define ENE_MAR5(base)          ENE_ADRS(base,0x0d)     /* multicast address */
#define ENE_MAR6(base)          ENE_ADRS(base,0x0e)     /* multicast address */
#define ENE_MAR7(base)          ENE_ADRS(base,0x0f)     /* multicast address */

/* page-2, read and write */

#define ENE_NEXT(base)          ENE_ADRS(base,0x05)     /* next page */
#define ENE_BLOCK(base)         ENE_ADRS(base,0x06)     /* block address */
#define ENE_ENH(base)           ENE_ADRS(base,0x07)     /* enable features */

/* NE2000 board registers */

#define ENE_DATA(base)          ENE_ADRS(base,0x10)     /* data window */
#define ENE_RESET(base)         ENE_ADRS(base,0x1f)     /* read here to reset */


/* COMMAND: Command Register */
#define CMD_PAGE2               0x80
#define CMD_PAGE1               0x40
#define CMD_PAGE0               0x00
#define CMD_NODMA               0x20
#define CMD_RWRITE              0x10
#define CMD_RREAD               0x08
#define CMD_TXP                 0x04
#define CMD_START               0x02
#define CMD_STOP                0x01

/* RCON: Receive Configuration Register */
#define RCON_MON                0x20
#define RCON_PROM               0x10
#define RCON_GROUP              0x08
#define RCON_BROAD              0x04
#define RCON_RUNTS              0x02
#define RCON_SEP                0x01

/* TCON: Transmit Configuration Register */
#define TCON_LB1                0x04
#define TCON_LB0                0x02
#define TCON_CRCN               0x01

/* DCON: Data Configuration Register */
#define DCON_BSIZE1             0x40
#define DCON_BSIZE0             0x20
#define DCON_BUS16              0x01

/* INTMASK: Interrupt Mask Register */
#define IM_XDCE                 0x40
#define IM_CNTE                 0x20
#define IM_OVWE                 0x10
#define IM_TXEE                 0x08
#define IM_RXEE                 0x04
#define IM_PTXE                 0x02
#define IM_PRXE                 0x01

/* INTSTAT: Interrupt Status Register */
#define ISTAT_RST               0x80
#define ISTAT_RDC               0x40            /* remote DMA complete */
#define ISTAT_CNT               0x20
#define ISTAT_OVW               0x10
#define ISTAT_TXE               0x08
#define ISTAT_RXE               0x04
#define ISTAT_PTX               0x02
#define ISTAT_PRX               0x01

/* TSTAT: Transmit Status Register */
#define TSTAT_OWC               0x80
#define TSTAT_CDH               0x40
#define TSTAT_UNDER             0x20
#define TSTAT_CRL               0x10
#define TSTAT_ABORT             0x08
#define TSTAT_TWC               0x04
#define TSTAT_NDT               0x02
#define TSTAT_PTX               0x01

/* RSTAT: Receive Status Register */
#define RSTAT_DFR               0x80
#define RSTAT_DIS               0x40
#define RSTAT_GROUP             0x20
#define RSTAT_MPA               0x10
#define RSTAT_OVER              0x08
#define RSTAT_FAE               0x04
#define RSTAT_CRC               0x02
#define RSTAT_PRX               0x01

/* ENH: Enable Features */
#define ENH_WAIT1               0x80
#define ENH_WAIT0               0x40
#define ENH_SLOT1               0x10
#define ENH_SLOT0               0x08

/* flags - software synchronize bit definitions */
#define TXING                   0x01
#define RXING                   0x02
#define TXREQ                   0x04

#ifdef __cplusplus
}
#endif

#endif  /* __INCif_eneh */
