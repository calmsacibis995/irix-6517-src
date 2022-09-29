#ifndef __SMTD_FS_H
#define __SMTD_FS_H
/*
 * Copyright 1990,1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	$Revision: 1.9 $
 */

/*
 * smtd_fs.c
 *
 *
 *	+---------------+---------------+---------------+
 *	| MAC HEADER    | SMT HEADER    | SMT INFO      |
 *	+---------------+---------------+---------------+
 *
 *	MAC HEADER: 13 octets
 *		FC:	1 octet	0100-0001 SMT INFO
 *				0100-1111 NSA
 *		DA:	6 octet Long address only for smt.
 *		SA:	6 octet
 *
 *	SMT HEADER: 20 octets
 *		Frame_Class	1 octet
 *		Frame_Type	1 octet
 *		Version_ID	2 octets
 *		Transaction_ID	4 octets
 *		Station_ID	8 octets
 *		pad		2 octets
 *		InfoField_len	2 octets
 */
#define	FC_SMTINFO	0x41	/* SMT Info FC type */
#define FC_NSA		0x4f	/* Next Station Addressing FC type */

typedef struct {
#define SMT_FC_NIF	0x01	/* Neighbor Info Frm */
#define SMT_FC_CONF_SIF	0x02	/* Configuration SIF */
#define SMT_FC_OP_SIF	0x03	/* Operation SIF */
#define SMT_FC_ECF	0x04	/* Echo Frame */
#define SMT_FC_RAF	0x05	/* Resource Allocation Frame */
#define SMT_FC_RDF	0x06	/* Request Denied Frame */
#define SMT_FC_SRF	0x07	/* Status Report Frame */
#define SMT_FC_GET_PMF	0x08	/* Get Parameter Frm */
#define SMT_FC_CHG_PMF	0x09	/* Change Parameter Frm */
#define SMT_FC_ADD_PMF	0x0a	/* Add Parameter Frm */
#define SMT_FC_RMV_PMF	0x0b	/* Remove Parameter Frm */
#define SMT_FC_ESF	0xff	/* Extended Service Frm */
#define SMT_NFRAMETYPE	12	/* num of frame types except ESF */
	u_char	fc;		/* Frame_Class		1 octet */

#define SMT_FT_ANNOUNCE	0x1	/* Announce */
#define SMT_FT_REQUEST	0x2	/* Request */
#define SMT_FT_RESPONSE	0x3	/* Responce */
	u_char	type;		/* Frame_Type		1 octet */

	/* version = 1 for SMT rev 6 */
	u_short	vers;		/* Version_ID		2 octets */
	u_long	xid;		/* Transaction_ID	4 octets */
	SMT_STATIONID sid;	/* Station_ID		8 octets */
	u_short	pad;		/* pad			2 octets */
#define SMT_MAXINFO	(4500-2-14-6-20)	/* see 8.6.1 */
	u_short	len;		/* InfoField_len	2 octets */
} smt_header;

typedef struct {
	struct lmac_hdr	mac_hdr;
	smt_header	smt_hdr;
	char		smt_info[SMT_MAXINFO];	/* give little more root */
} SMT_FB;

#define FBTOMH(x)	((struct lmac_hdr *)(x))
#define FBTOFP(x)	(&(((SMT_FB *)x)->smt_hdr))
#define FBTOINFO(x)	(((SMT_FB *)x)->smt_info)
#define FBMAXINFO	sizeof(((SMT_FB *)0)->smt_info)

extern void smtd_fs(SMT_MAC *);
extern int fs_request(SMT_MAC *, int, LFDDI_ADDR *, void *, int);
extern int fs_announce(SMT_MAC *, int, void *, int);
extern void fs_respond(SMT_MAC*, int, LFDDI_ADDR*, SMT_FB*, int, int);
#endif /* __SMTD_FS_H */
