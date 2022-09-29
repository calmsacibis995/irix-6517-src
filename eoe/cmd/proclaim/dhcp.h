#ifndef __PROTOCOLS_BOOTP_H__
#define __PROTOCOLS_BOOTP_H__
#ifdef __cplusplus
extern "C" {
#endif
#ident "$Revision: 1.1 $"
/*
*
* Copyright 1992, Silicon Graphics, Inc.
* All Rights Reserved.
*
* This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
* the contents of this file may not be disclosed to third parties, copied or
* duplicated in any form, in whole or in part, without the prior written
* permission of Silicon Graphics, Inc.
*
* RESTRICTED RIGHTS LEGEND:
* Use, duplication or disclosure by the Government is subject to restrictions
* as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
* and Computer Software clause at DFARS 252.227-7013, and/or in similar or
* successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
* rights reserved under the Copyright Laws of the United States.
*/
/*
 * "vendor" data permitted for Stanford boot clients.
 */

struct vend {
	u_int	v_magic;	/* magic number */		/* 4 */
	u_long	v_flags;	/* flags/opcodes, etc. */	/* 4 */
	u_char	v_unused[304];	/* currently unused */		/* 304 */
};

#define	iaddr_t struct in_addr

#define	BOOTREQUEST	1
#define	BOOTREPLY	2

#define DHCP_OPTS_SIZE	308

struct sgi_vend {				/* 312 */
	u_int	v_magic;			/* 004 */
	u_char  dhcp_opts[DHCP_OPTS_SIZE];	/* 308 */
};

struct bootp {
	u_char	bp_op;		/* packet opcode type */	/* 1  = 1   */
	u_char	bp_htype;	/* hardware addr type */	/* 1  = 2   */
	u_char	bp_hlen;	/* hardware addr length */	/* 1  = 3   */
	u_char	bp_hops;	/* gateway hops */		/* 1  = 4   */
	u_long	bp_xid;		/* transaction ID */		/* 4  = 8   */
	u_short	bp_secs;	/* seconds since boot began */	/* 2  = 10  */
	u_short	bp_flags;					/* 2  = 12  */
	iaddr_t	bp_ciaddr;	/* client IP address */		/* 4  = 16  */
	iaddr_t	bp_yiaddr;	/* 'your' IP address */		/* 4  = 20  */
	iaddr_t	bp_siaddr;	/* server IP address */		/* 4  = 24  */
	iaddr_t	bp_giaddr;	/* gateway IP address */	/* 4  = 28  */
	u_char	bp_chaddr[16];	/* client hardware address */	/* 16 = 44  */
	u_char	bp_sname[64];	/* server host name */		/* 64 = 108 */
	u_char	bp_file[128];	/* boot file name */		/* 128= 236 */
	union {							/* 312= 548 */
		struct	vend 		bp_options;
		struct	sgi_vend	sgi_dhcp;
	} options;
};

#define vd_magic	options.bp_options.v_magic	/* magic #	*/
#define	vd_flags	options.bp_options.v_flags	/* opcodes	*/
#define	vd_clntname	options.bp_options.v_unused	/* client name	*/

#define	dh_magic	options.sgi_dhcp.v_magic
#define	dh_opts		options.sgi_dhcp.dhcp_opts

/*
 * UDP port numbers, server and client.
 */
#define	IPPORT_BOOTPS		67
#define	IPPORT_BOOTPC		68

#define	VM_DHCP		0x63825363	/* dhcp magic cookie: 99.130.83.99 */
#define	VM_SGI		0xc01a3309	/* v_magic SGI: 192.26.51.9 */
#define	VM_AUTOREG	0xc01a330a	/* v_magic SGI: 192.26.51.10 */

/* v_flags values */
#define	VF_PCBOOT	1	/* an IBMPC or Mac wants environment info */
#define	VF_HELP		2	/* help me, I'm not registered */
#define	VF_GET_IPADDR	3	/* Request for client IP address */
#define	VF_RET_IPADDR	4	/* Response for client IP address */
#define	VF_NEW_IPADDR	5	/* Response for client IP address */
#ifdef __cplusplus
}
#endif
#endif /* !__PROTOCOLS_BOOTP_H__ */
