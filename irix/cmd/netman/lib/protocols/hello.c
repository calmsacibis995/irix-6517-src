/*
 * Copyright 1989 Silicon Graphics, Inc.  All rights reserved.
 *
 * Snoop on Distributed Computer Network (DCN) HELLO routing protocol.
 * An older version of HELLO is defined in RFC 891.
 */
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "enum.h"
#include "protodefs.h"
#include "protocols/hello.h"
#include "protocols/ip.h"

char	helloname[] = "hello";

/*
 * Datastream macros.
 */
#define	S_ALIGN	sizeof(u_short)		/* short word alignment */

#define	ds_inline_hellohdr(ds) \
	(struct hellohdr *) ds_inline(ds, sizeof(struct hellohdr), S_ALIGN)
#define	ds_inline_type0pair(ds) \
	(struct type0pair *) ds_inline(ds, sizeof(struct type0pair), S_ALIGN)
#define	ds_inline_type1pair(ds) \
	(struct type1pair *) ds_inline(ds, sizeof(struct type1pair), S_ALIGN)


/*
 * Hello field identifiers and descriptors.
 */
enum hellofid {
	CHECKSUM, DATE, TIME, TIMESTAMP, COUNT, TYPE,
	DST, DELAY, OFFSET, D0_DELAY, D0_OFFSET
};

#define HPFI_UINT(name, title, id, type, off, level) \
	PFI(name, title, id, DS_ZERO_EXTEND, sizeof(type), off, EXOP_NUMBER, \
	    level)

static ProtoField hellofields[] = {
/* hello header */
PFI_UINT("checksum",	"Checksum",	 CHECKSUM, u_short, PV_VERBOSE),
PFI_UINT("date",	"Date",		 DATE,     u_short, PV_TERSE),
PFI_UINT("time",	"Time",		 TIME,     u_long,  PV_DEFAULT),
PFI_UINT("timestamp",	"Timestamp",	 TIMESTAMP,u_short, PV_DEFAULT),
PFI_UINT("count",	"Count",	 COUNT,    u_char,  PV_TERSE),
PFI_UINT("type",	"Type",		 TYPE,     u_char,  PV_TERSE),

/* hello data (routing table entries) */
PFI_UINT("dst",		"Destination",	 DST,	   u_long,  PV_DEFAULT),
PFI_UINT("delay",	"Delay",	 DELAY,	   u_short, PV_DEFAULT),
PFI_UINT("offset",	"Offset",	 OFFSET,   u_short, PV_DEFAULT),
HPFI_UINT("d0_delay",	"Type 0 Delay",	 D0_DELAY, u_short,12,PV_DEFAULT),
HPFI_UINT("d0_offset",	"Type 0 Offset", D0_OFFSET,u_short,14,PV_DEFAULT),
};

#define	HELLOFID(pf)	((enum hellofid) (pf)->pf_id)
#define	HELLOFIELD(fid)	hellofields[(int) fid]

static ProtoMacro hellonicknames[] = {
	PMI("HELLO",	helloname),
};


/*
 * HELLO protocol interface.
 */
#define	hello_compile	pr_stub_compile
#define	hello_embed	pr_stub_embed
#define	hello_match	pr_stub_match
#define hello_resolve	pr_stub_resolve
#define	hello_setopt	pr_stub_setopt

DefineLeafProtocol(hello, helloname, "DCN HELLO Routing Protocol", PRID_HELLO,
		   DS_BIG_ENDIAN, HELLO_HDRSIZE);

static int
hello_init()
{
	if (!pr_register(&hello_proto, hellofields, lengthof(hellofields), 3))
		return 0;
	if (!pr_nest(&hello_proto, PRID_IP, IPPROTO_HELLO, hellonicknames,
		     lengthof(hellonicknames))) {
		return 0;
	}
	return 1;
} /* hello_init() */


static int
hello_fetch(ProtoField *pf, DataStream *ds, ProtoStack *ps, Expr *rex)
{
	struct hellohdr *hi;

	if (pf->pf_id > (int) TYPE) {	/* should not fetch data fields */
		return 0;
	}

	hi = ds_inline_hellohdr(ds);
	if (hi == 0) {			/* fetch what you can */
		return ds_field(ds, pf, HELLO_HDRSIZE, rex);
	}

	rex->ex_op = EXOP_NUMBER;
	switch (HELLOFID(pf)) {
	  case CHECKSUM:
		rex->ex_val = ntohs(hi->h_cksum);
		break;
	  case DATE:
		rex->ex_val = ntohs(hi->h_date);
		break;
	  case TIME:
		rex->ex_val = ntohl(hi->h_time);
		break;
	  case TIMESTAMP:
		rex->ex_val = ntohs(hi->h_tstp);
		break;
	  case COUNT:
		rex->ex_val = hi->h_count;
		break;
	  case TYPE:
		rex->ex_val = hi->h_type;
		break;
	  default:
		return 0;
	}
	return 1;
} /* hello_fetch() */

static void
hello_decode(DataStream *ds, ProtoStack *ps, PacketView *pv)
{
	struct hellohdr *hi;

	hi = ds_inline_hellohdr(ds);
	if ( hi == 0 )
		return;
	pv_showfield(pv, &HELLOFIELD(CHECKSUM), &hi->h_cksum,
		"%#-04x", ntohs(hi->h_cksum));
	pv_showfield(pv, &HELLOFIELD(DATE), &hi->h_date,
		"%#-04x (sync: %1d, yr:%4d, day:%2d, mo:%2d)",
		ntohs(hi->h_date),
		(ntohs(hi->h_date)>>15 & 0x01),		/* ck valid */
		(ntohs(hi->h_date) & 037) + 1972,	/* year */
		(ntohs(hi->h_date)>>5 & 037),		/* day */
		(ntohs(hi->h_date)>>10 & 037) );	/* month */
	pv_showfield(pv, &HELLOFIELD(TIME), &hi->h_time,
		"%-10d (%-12.12s)", ntohl(hi->h_time),
		ip_timestamp(ntohl(hi->h_time)));
	pv_showfield(pv, &HELLOFIELD(TIMESTAMP), &hi->h_tstp,
		"%-5d", ntohs(hi->h_tstp));
	pv_showfield(pv, &HELLOFIELD(COUNT), &hi->h_count,
		"%-3d", hi->h_count);
	pv_showfield(pv, &HELLOFIELD(TYPE), &hi->h_type,
		"%-1d", hi->h_type);

	switch (hi->h_type) {
	  case 1: {
		struct type1pair *n;
		while ((n = ds_inline_type1pair(ds)) != 0) {
			n->d1_dst = ntohl(n->d1_dst);
			n->d1_delay= ntohs(n->d1_delay);
			n->d1_offset= ntohs(n->d1_offset);

			pv_break(pv);
			pv_showfield(pv, &HELLOFIELD(DST), &n->d1_dst,
				     "%-15.15s", inet_ntoa(n->d1_dst));
			pv_showfield(pv, &HELLOFIELD(DELAY), &n->d1_delay,
				     "%-5d", n->d1_delay);
			pv_showfield(pv, &HELLOFIELD(OFFSET), &n->d1_offset,
				     "%-5d", n->d1_offset);
			}
		break;
		} /* case of type 1 pair */
	  case 0: {
		struct type0pair *n;
		while ((n = ds_inline_type0pair(ds)) != 0) {
			n->d0_delay= ntohs(n->d0_delay);
			n->d0_offset= ntohs(n->d0_offset);

			pv_break(pv);
			pv_showfield(pv, &HELLOFIELD(D0_DELAY), &n->d0_delay,
				     "%-5d", n->d0_delay);
			pv_showfield(pv, &HELLOFIELD(D0_OFFSET), &n->d0_offset,
				     "%-5d", n->d0_offset);
			}
		} /* case of type 0 pair */
	}
} /* hello_decode() */
