#ifndef SMT_H
#define SMT_H
/*
 * Copyright 1990 Silicon Graphics, Inc.  All rights reserved.
 *
 * SMT definitions.
 */
#include <sys/types.h>
#include <sys/fddi.h>
#include <sys/smt.h>
#include "protocols/ether.h"

/*
 * SMT protocol name.
 */
extern char	smtname[];

#define SIDLEN	8
typedef struct {
	u_char	fclass;		/* Frame_Class		1 octet */
	u_char	ftype;		/* Frame_Type		1 octet */
	u_short	vers;		/* Version_ID		2 octets */
	u_long	xid;		/* Transaction_ID	4 octets */
	u_char	sid[SIDLEN];	/* Station_ID		8 octets */
	u_short	pad;		/* pad			2 octets */
	u_short	ilen;		/* InfoField_len	2 octets */
} smt_header;

struct smtframe {
	ProtoStackFrame		sf_frame;	/* base class state */
	smt_header		sf_hdr;
};

#define CL_NIF		0x01
#define CL_CSIF		0x02
#define CL_OSIF		0x03
#define CL_ECF		0x04
#define CL_RAF		0x05
#define CL_RDF		0x06
#define CL_SRF		0x07
#define CL_GETPMF	0x08
#define CL_CHPMF	0x09
#define CL_ADDPMF	0x0A
#define CL_RMPMF	0x0B
#define CL_XSF		0xFF

#define TY_ANNOUNCE	0x01
#define TY_REQ		0x02
#define TY_RESP		0x03

#define UNALEN		6
#define PTY_UNA		0x01		/* upstream neighbor address */
#define PTY_SD		0x02		/* station descriptor */
#define PTY_SS		0x03		/* station state */
#define PTY_TS		0x04		/* msg timestamp */
#define PTY_SP		0x05		/* station policies */
#define PTY_MACNBR	0x07		/* mac neighbors */
#define PTY_PD		0x08		/* path descriptor */
#define PTY_MACST	0x09		/* mac status */
#define PTY_PLER	0x0A		/* PORT link error rate status */
#define PTY_FRCNT	0x0B		/* MAC frame counters */
#define PTY_FRNTCP	0x0C		/* frame not copied */
#define PTY_PRI		0x0D		/* priority */
#define PTY_PORTEB	0x0E		/* PORT elast. */
#define PTY_MANUF	0x0F		/* manufacturer */
#define PTY_USER	0x10		/* user */
#define PTY_ECHO	0x11		/* echo data */
#define PTY_REAS	0x12		/* reason code */
#define PTY_REJF	0x13		/* rejected frame beginning */
#define PTY_SUPVER	0x14		/* supported versions */
#define PTY_FCS		0x200B		/* mac frame status capabilities */
#define PTY_ESFID	0xffff		/* ESF ID */

#define TPL_STWRAP	0x01
#define TPL_UNRCON	0x02
#define TPL_AATW	0x04
#define TPL_BBTW	0x08
#define TPL_RTST	0x10
#define TPL_SR		0x20

#define DUP_MYDUP	0x01
#define DUP_MYUNADUP	0x02

#endif
