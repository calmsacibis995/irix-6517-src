/*
 * Copyright (c) 1989 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution is only permitted until one year after the first shipment
 * of 4.4BSD by the Regents.  Otherwise, redistribution and use in source and
 * binary forms are permitted provided that: (1) source distributions retain
 * this entire copyright notice and comment, and (2) distributions including
 * binaries display the following acknowledgement:  This product includes
 * software developed by the University of California, Berkeley and its
 * contributors'' in the documentation or other materials provided with the
 * distribution and in all advertising materials mentioning features or use
 * of this software.  Neither the name of the University nor the names of
 * its contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 *	@(#)if_types.h	7.3 (Berkeley) 6/28/90
 */
#ifdef __cplusplus
extern "C" {
#endif


/* interface types for benefit of parsing media address headers */
#define IFT_OTHER	0x1		/* none of the following */
#define IFT_1822	0x2		/* old-style arpanet imp */
#define IFT_HDH1822	0x3		/* HDH arpanet imp */
#define IFT_X25DDN	0x4		/* x25 to imp */
#define IFT_X25		0x5		/* PDN X25 interface */
#define	IFT_ETHER	0x6		/* Ethernet I or II */
#define	IFT_ISO88023	0x7		/* CMSA CD */
#define	IFT_ISO88024	0x8		/* Token Bus */
#define	IFT_ISO88025	0x9		/* Token Ring */
#define	IFT_ISO88026	0xa		/* MAN */
#define	IFT_STARLAN	0xb
#define	IFT_P10		0xc		/* Proteon 10MBit ring */
#define	IFT_P80		0xd		/* Proteon 10MBit ring */
#define IFT_HY		0xe		/* Hyperchannel */
#define IFT_FDDI	0xf
#define IFT_LAPB	0x10
#define IFT_SDLC	0x11
#define IFT_T1		0x12
#define IFT_CEPT	0x13
#define IFT_ISDNBASIC	0x14
#define IFT_ISDNPRIMARY	0x15
#define IFT_PTPSERIAL	0x16
#define	IFT_LOOP	0x18		/* loopback */
#define IFT_EON		0x19		/* ISO over IP */
#define	IFT_XETHER	0x1a		/* obsolete 3MB experimental ethernet */
#define	IFT_NSIP	0x1b		/* XNS over IP */
#define	IFT_SLIP	0x1c		/* IP over generic TTY */
#define IFT_ULTRA	0x1d		/* Ultra Technologies */
#define	IFT_DS3		0x1e		/* Generic T3 */
#define	IFT_SIP		0x1f		/* SMDS */
#define	IFT_FRELAY	0x20		/* Frame Relay DTE only */
#define	IFT_RS232	0x21
#define	IFT_PARA	0x22		/* parallel-port */
#define	IFT_ARCNET	0x23
#define	IFT_ARCNETPLUS	0x24
#define IFT_ATM		0x25		/* ATM cells */
#define	IFT_MIOX25	0x26
#define	IFT_SONET	0x27		/* SONET or SDH */
#define	IFT_X25PLE	0x28
#define	IFT_ISO88022LLC	0x29
#define	IFT_LOCALTALK	0x2a
#define	IFT_SMDSDXI	0x2b
#define	IFT_FRELAYDCE	0x2c		/* Frame Relay DCE */
#define	IFT_V35		0x2d
#define	IFT_HSSI	0x2e
#define	IFT_HIPPI	0x2f
#define	IFT_MODEM	0x30		/* Generic Modem */
#define	IFT_AAL5	0x31		/* AAL5 over ATM */
#define	IFT_SONETPATH	0x32
#define	IFT_SONETVT	0x33
#define	IFT_SMDSICIP	0x34		/* SMDS InterCarrier Interface */
#define	IFT_PROPVIRTUAL	0x35		/* Proprietary Virtual/internal */
#define	IFT_PROPMUX	0x36		/* Proprietary Multiplexing */
#define	IFT_CLXP	0x37		/* Cell Layer cross pttn driver */
#define IFT_LS		0x38		/* load sharing network interface */
#define IFT_GSN		0x91		/* IANA ifType if-gsn aka HIPPI-6400 */

#define	IFT_PVC		0xfd		/* (Cray) ATM - PVC interface */
#define	IFT_Q2931	0xfe		/* (Cray) ATM - UNI interface */
#define	IFT_SPANS	0xff		/* (Cray) ATM - SPANS interface */

#ifdef __cplusplus
}
#endif
