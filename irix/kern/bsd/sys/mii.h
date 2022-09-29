#ifndef	__MII_H__
#define	__MII_H__
#ifdef __cplusplus
extern "C" {
#endif
/*
 * Ethernet 802.3u Media Independent Interface (MII) register definitions.
 */

/* Register 0 - Control Register (RW) */
#define MII_R0_RESET            0x8000          /* PHY reset */
#define MII_R0_LOOPBACK         0x4000          /* loopback enable */
#define MII_R0_SPEEDSEL         0x2000          /* speed select (100mbps) */
#define MII_R0_AUTOEN           0x1000          /* auto negotiation enable */
#define MII_R0_POWERDOWN        0x0800          /* power down mode */
#define MII_R0_ISOLATE          0x0400          /* elec. isolate phy */
#define MII_R0_RESTARTAUTO      0x0200          /* restart auto nego */
#define MII_R0_DUPLEX           0x0100          /* full duplex mode */
#define MII_R0_COLLTEST         0x0080          /* test COL signal */

/* Register 1 - Status Register (RO) */
#define MII_R1_100BASET4        0x8000          /* phy T4 able */
#define MII_R1_100TXFD          0x4000          /* phy 100 full duplex */
#define MII_R1_100TXHD          0x2000          /* phy 100 half duplex */
#define MII_R1_10TXFD           0x1000          /* phy 10 full duplex */
#define MII_R1_10TXHD           0x0800          /* phy 10 half duplex */
#define MII_R1_AUTODONE         0x0020          /* phy auto nego done */
#define MII_R1_REMFAULT         0x0010          /* remote fault detected */
#define MII_R1_AUTOCONFIG       0x0008          /* phy auto nego able */
#define MII_R1_LINKSTAT         0x0004          /* 1=good link status */
#define MII_R1_JABBER           0x0002          /* 1=jabber detect */
#define MII_R1_EXTCAP           0x0001          /* extended reg capabilities */

/* Register 2 - PHY Identifier Register1 (RO) */
#define MII_R2_OUIMSB           0xffff          /* IEEE OUI MSB */

/* Register 3 - PHY Identifier Register2 (RO) */
#define MII_R3_OUILSB           0xfc00          /* IEEE OUI LSB */
#define MII_R3_VNDRMDL          0x03f0          /* vendor model id */
#define MII_R3_MDLREV           0x000f          /* vendor model rev */

/* Register 4 - Auto-Negotiation Advertisement Register (RW) */
#define MII_R4_NP               0x8000          /* next page bit */
#define MII_R4_ACK              0x4000          /* ack link partner data */
#define MII_R4_REMFAULT         0x2000          /* signal rem. fault to partner  */
#define MII_R4_T4               0x0200          /* 100BASE-T4 cap. */
#define MII_R4_TXFD             0x0100          /* 100BASE-TX full duplex */
#define MII_R4_TX               0x0080          /* 100BASE-TX cap. */
#define MII_R4_10FD             0x0040          /* 10BASE-T full duplex */
#define MII_R4_10               0x0020          /* 10BASE-T cap. */
#define MII_R4_SELECTOR         0x001f          /* encoded selector cap. */

/* Register 5 - Auto-Negotiation Link Partner Ability Register (RO) */
#define MII_R5_NP               0x8000          /* next page capable */
#define MII_R5_ACK              0x4000          /* ack link parter data */
#define MII_R5_REMFAULT         0x2000          /* rem. fault received */
#define MII_R5_T4               0x0200          /* 100BASE-T4 cap. */
#define MII_R5_TXFD             0x0100          /* 100BASE-TX full duplex */
#define MII_R5_TX               0x0080          /* 100BASE-TX cap. */
#define MII_R5_10FD             0x0040          /* 10BASE-T full duplex */
#define MII_R5_10               0x0020          /* 10BASE-T cap. */
#define MII_R5_SELECTOR         0x001f          /* encoded selector cap. */
#define MII_R5_8023             0x1             /* IEEE 802.3 link tech */
#define MII_R5_8029A            0x2             /* IEEE 802.9a link tech */

/* Register 6 - Auto-Negotiation Expansion Register (RW) */
#define MII_R6_MLF              0x0010          /* mult. link fault */
#define MII_R6_LPNPABLE         0x0008          /* partner nextpage cap. */
#define MII_R6_NPABLE           0x0004          /* nextpage cap. */
#define MII_R6_PAGERECVD        0x0002          /* code word received. */
#define MII_R6_LPNWABLE         0x0001          /* partner is NWay cap. */

/* Register 7 - Auto-Negotiation Next Page transmit Register (RW) */
#ifdef __cplusplus
}
#endif
#endif	/* __MII_H__ */
