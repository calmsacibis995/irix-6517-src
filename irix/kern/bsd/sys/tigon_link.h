/*
 * COPYRIGHT NOTICE
 * Copyright (c) Alteon Networks, Inc. 1996, 1997 
 * All rights reserved
 * 
 */
/*
 * HISTORY
 * $Log: tigon_link.h,v $
 * Revision 1.2  1999/05/18 21:44:02  trp
 * Upgrade to firmware level 12.3.9
 *
 * Revision 1.1.2.12  1999/01/27  19:09:39  hayes
 * 	incorporate autosense code
 * 	[1999/01/27  19:07:52  hayes]
 *
 * Revision 1.1.7.2  1998/11/07  20:59:55  hayes
 * 	link autosense mode now works
 * 
 * Revision 1.1.2.11  1998/03/09  17:32:08  hayes
 * 	add LNK_LOOPBACK
 * 	[1998/03/09  00:58:42  hayes]
 * 
 * Revision 1.1.2.10  1997/12/06  22:35:15  kyung
 * 	recheck in after merge
 * 	[1997/12/06  21:58:52  kyung]
 * 
 * 	added LNK_PREF for preferred phy selection
 * 	[1997/12/06  21:17:42  kyung]
 * 
 * Revision 1.1.2.9  1997/10/01  22:28:05  hayes
 * 	added LNK_MASK
 * 	[1997/10/01  22:27:52  hayes]
 * 
 * Revision 1.1.2.8  1997/09/23  20:27:05  hayes
 * 	added LNK_NEG_ADVANCED and LNK_NIC
 * 	[1997/09/23  20:26:52  hayes]
 * 
 * Revision 1.1.2.7  1997/09/10  03:55:08  hayes
 * 	remove LNK_XX_FLOW_CTL_N
 * 	[1997/09/10  03:49:43  hayes]
 * 
 * Revision 1.1.2.6  1997/08/12  02:53:54  hayes
 * 	add LNK_JUMBO and LNK_ALTEON definitions
 * 	[1997/08/12  02:51:38  hayes]
 * 
 * Revision 1.1.2.5  1997/05/19  18:31:29  hayes
 * 	add LNK_NEG_FCTL
 * 	[1997/05/19  18:31:19  hayes]
 * 
 * Revision 1.1.2.4  1997/05/01  18:30:24  hayes
 * 	added LNK_NEGOTIATE flag
 * 	[1997/05/01  18:30:16  hayes]
 * 
 * Revision 1.1.2.3  1997/03/04  19:23:07  hayes
 * 	Added LNK_ENABLE
 * 	[1997/03/04  19:22:50  hayes]
 * 
 * Revision 1.1.2.2  1997/02/28  00:20:28  wayne
 * 	Add #defines to help use bits in groups
 * 	[1997/02/28  00:20:17  wayne]
 * 
 * Revision 1.1.2.1  1997/02/26  21:39:43  wayne
 * 	Initial revision: link control flags, for gen_com.data.flags
 * 	[1997/02/26  21:39:37  wayne]
 * 
 * $EndLog$
 */

#ifndef _LINK_H_
#define _LINK_H_

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
/*
 *  Definition of LINK FLAGS for controlling link negotiation
 *
 *  "Autonegotiation" is specified by turning on the LNK_NEGOTIATE bit.
 *  This allows us to specify the method that we achieve a given state.
 *  For instance, if we need to set the link to 1000, FD, no FC we
 *  can get there either by setting it directly and bypassing all link
 *  negotiation or by setting these parameters in our negotiation
 *  register and negotiation to them.  In the future, all gig links
 *  should (when all vendors support it) only use link negotiation, but 
 *  until that time we must support both nogitiated and non-negotiated
 *  links.  10/100 will probably always have to support both due to the
 *  amount of older gear that we must work with.
 *
 *  The list of all possible things to be negotiated to is encoded in
 *  these flags.  If we are not negotiating, only 1 of each item should
 *  be selected.  Setting more than one will have unpredictable results.
 */
#define LNK_SENSE_NO_NEG   0x00002000	/* remote end does not autonegotiate */
#define LNK_LOOPBACK       0x00004000	/* use internal loopback */
#define LNK_PREF           0x00008000   /* preferred link */
#define LNK_10MB           0x00010000   /* 10 megabit data rate */
#define LNK_100MB          0x00020000   /* 100 megabit data rate */
#define LNK_1000MB         0x00040000   /* 1000 megabit data rate */
#define LNK_FULL_DUPLEX    0x00080000   /* full duplex */
#define LNK_HALF_DUPLEX    0x00100000   /* half duplex */
#define LNK_TX_FLOW_CTL_Y  0x00200000   /* do TX flow control */
#define LNK_NEG_ADVANCED   0x00400000   /* do advanced autonegotiation (NP) */
#define LNK_RX_FLOW_CTL_Y  0x00800000   /* do RX flow control */
#define LNK_NIC            0x01000000   /* remote device is a NIC (endpoint) */
#define LNK_JAM            0x02000000   /* generate JAM signals when needed */
#define LNK_JUMBO          0x04000000   /* jumbo frames */
#define LNK_ALTEON         0x08000000   /* remote end is an Alteon device */
#define LNK_NEG_FCTL	   0x10000000	/* enable/disable fctl negotiation */
#define LNK_NEGOTIATE      0x20000000   /* enable/disable autonegotiation */
#define LNK_ENABLE         0x40000000   /* enable/disable link */
#define LNK_UP             0x80000000   /* link is operational */


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
/*
 *  #defines to facilitate testing and formatting groups of LNK bits
 */
#define LNK_SPD_MASK   (LNK_10MB | LNK_100MB | LNK_1000MB)  /* bits in group */
#define LNK_SPD_SHIFT  16               /* right shift to get to units pos'n */
#define LNK_DPX_MASK   (LNK_HALF_DUPLEX | LNK_FULL_DUPLEX)
#define LNK_DPX_SHIFT  19
#define LNK_TFC_MASK   LNK_TX_FLOW_CTL_Y
#define LNK_TFC_SHIFT  21
#define LNK_RFC_MASK   LNK_RX_FLOW_CTL_Y
#define LNK_RFC_SHIFT  23

/*
 * and finally, a mask for everything...
 */
#define LNK_MASK	0xffff0000

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#endif /* _LINK_H_ */
