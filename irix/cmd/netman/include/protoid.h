#ifndef PROTOID_H
#define PROTOID_H
/*
 * Copyright 1990 Silicon Graphics, Inc.  All rights reserved.
 *
 * Enumeration of all known protocols, for use as a faster key than
 * protocol name string.  Add new protocols at the end, and maintain
 * PRID_MAX as one more than the highest ID.
 */

#define	PRID_NULL	0	/* invalid ID */
#define	PRID_SNOOP	1	/* snoop input pseudo-protocol */
#define	PRID_LOOP	2	/* loopback pseudo-protocol */
#define	PRID_ETHER	3	/* 10 MHz and 100 MHz Ethernet */
#define	PRID_FDDI	4	/* Fiber Distributed Data Interface */
#define	PRID_CRAYIO	5	/* Cray Input/Output subsystem */
#define	PRID_ARP	6	/* Address Resolution Protocol */
#define	PRID_RARP	7	/* Reverse Address Resolution Protocol */
#define	PRID_ARPIP	8	/* IP version of ARP & RARP */
#define	PRID_IP		9	/* Internet Protocol */
#define	PRID_EGP	10	/* Exterior Gateway Protocol */
#define	PRID_HELLO	11	/* DCN HELLO routing protocol */
#define	PRID_ICMP	12	/* Internet Control Message Protocol */
#define	PRID_IGMP	13	/* Internet Group Management Protocol */
#define	PRID_TCP	14	/* Transmission Control Protocol */
#define	PRID_UDP	15	/* User Datagram Protocol */
#define	PRID_BOOTP	16	/* Bootstrap Protocol */
#define	PRID_DNS	17	/* Internet Domain Name Service */
#define	PRID_RIP	18	/* Routing Information Protocol */
#define	PRID_SUNRPC	19	/* Sun Remote Procedure Call protocol */
#define	PRID_MOUNT	20	/* Sun RPC Mount protocol */
#define	PRID_NFS	21	/* Sun RPC Network File System */
#define	PRID_PMAP	22	/* Sun RPC Portmap */
#define	PRID_YP		23	/* Sun NIS service */
#define	PRID_YPBIND	24	/* Sun NIS Binding service */
#define	PRID_TFTP	25	/* Trivial File Transfer Protocol */
#define	PRID_DNRP	26	/* DECnet Routing Protocol */
#define	PRID_DNNSP	27	/* DECnet Network Services Protocol */
#define	PRID_XTP	28	/* eXpress Transfer Protocol */
#define	PRID_SMT	29	/* FDDI Station ManagemenT protocol */
#define	PRID_MAC	30	/* FDDI Media Access Control protocol */
#define	PRID_LLC	31	/* Link Level Control protocol */
#define PRID_FIMPL      32      /* FDDI "reserved for implementer" protocol */
#define PRID_IDP	33	/* xns Internet Datagram Protocol */
#define PRID_XNSRIP	34	/* xns Routing Information Protocol */
#define	PRID_ECHO	35	/* xns Echo Protocol */
#define	PRID_ERROR	36	/* xns Error Protocol */
#define	PRID_SPP	37	/* xns Sequenced Packet Protocol */
#define	PRID_PEP	38	/* xns Packet Exchange Protocol */
#define PRID_ELAP	39	/* (AT) EtherTalk Link Access Protocol */
#define PRID_AARP	40	/* AT Address Resolution Protocol */
#define PRID_DDP	41	/* AT Datagrapm Delivery Protocol */
#define PRID_RTMP	42	/* AT Routing Table Maintenance Protocol */
#define PRID_AEP	43	/* AT Echo Protocol */
#define PRID_ATP	44	/* AT Transaction Protocol */
#define PRID_NBP	45	/* AT Name Binding Protocol */
#define PRID_ADSP	46	/* AT Data Stream Protocol */
#define PRID_ZIP	47	/* AT Zone Information Protocol */
#define PRID_ASP	48	/* AT Session Protocol */
#define PRID_PAP	49	/* AT Printer Access Protocol */
#define PRID_AFP	50	/* AT Filing Protocol */
#define	PRID_SNMP	51	/* Simple Network Management Protocol */
#define	PRID_LAT	52	/* (DEC) Local Area Transport */
#define	PRID_NLM	53	/* Sun Network Lock Manager Protocol */
#define PRID_TOKENRING	54	/* Tokenring. This should always be 54. */
#define PRID_TOKENMAC	55	/* Tokenring Media Access Control Protocol */
#define PRID_TELNET	56	/* Telnet Protocol */
#define PRID_FTP	57	/* File Transfer Protocol */
#define PRID_SMTP	58	/* Simple Mail Transfer protocol */
#define PRID_OSI	59	/* OSI Network Layer */
#define PRID_SNA	60	/* SNA Header */
#define PRID_NETBIOS	61	/* NetBIOS Service Protocol over TCP */
#define PRID_VINES	62	/* Banyan Vines Protocol */
#define PRID_TSP	63	/* Time Synchronization Protocol */
#define PRID_X11	64	/* X11R4 */
#define PRID_RLOGIN	65	/* Remote Login Protocol */
#define PRID_RCP	66	/* Remote Copy Protocol */
#define PRID_IPX	67      /* Novell IPX protocol  */
#define PRID_SPX	68      /* Novell SPX protocol  */
#define PRID_NOVELLRIP  69      /* Novell RIP protocol  */
#define PRID_HIPPI	70
#define PRID_ATM	71

#define	PRID_MAX	72

#endif
