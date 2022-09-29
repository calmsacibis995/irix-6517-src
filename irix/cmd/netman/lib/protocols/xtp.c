/*
 * Copyright 1990 Silicon Graphics, Inc.  All rights reserved.
 *
 * Protocol Engines Xpress Transfer Protocol (XTP).
 */

#define  XTP

#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>		/* struct for in_addr */
#include <netinet/if_ether.h>
#include "protocols/ip.h"	/* enum parameter to pass to ip_hostname */
#include <sys/socket.h>		/* need PF_XTP defined */
#include "enum.h"
#include "index.h"
#include "protodefs.h"
#include "protocols/xtp.h"
#include "snooper.h"		/* reqd to find XTP trailer */

/*
 * Current limitations:
 *	can fetch only header fields (not trailer or control segment fields)
 *	can't decode any protocols embedded in an XTP packet
 *	  (i.e., XTP is a leaf protocol, with no match or embed function)
 *	doesn't do any byte-swapping of XTP header, trailer, or control segment
 *	only decodes addresses of type IP (address format = XTPAF_INET)
 *	doesn't cache key/MAC_address associations
 */
char xtpname[] = "xtp";

/*
 * XTP field identifiers and descriptors.
 */
enum xtpfid {
    OPTIONS, OFFSET, LITTLE, VERSION, TYPE, KEY, SORT, HRSVD, SEQ, ROUTE,
    	A_LEN, A_FMT, A_RATE, A_ID, A_RSVD,
		    AIP_DST, AIP_SRC, AIP_DPORT, AIP_SPORT, AIP_PRO,
		    AD_KEY, AD_MAC,
	    RATE, BURST, SYNC, XECHO, TIME, TECHO, XKEY, XROUTE, CRSVD, ALLOC,
	    RSEQ, NSPAN, SPAN,
    	DIAG_C, DIAG_V,
    	ROUTE_C, ROUTE_V,
    DCHK, DSEQ, FLAGS, ALIGN, TTL, HTCHK
};

#define	XTPF_INHDR(pf)	((pf)->pf_id <= (int) ROUTE)
#define	XTPF_INASEG(pf)	((pf)->pf_id >= (int) A_LEN && \
			 (pf)->pf_id <= (int) AIP_PRO)
#define	XTPF_INCSEG(pf)	((pf)->pf_id >= (int) RATE && \
			 (pf)->pf_id <= (int) SPAN)
#define	XTPF_INTRLR(pf)	((pf)->pf_id >= (int) DCHK)

#define SPAN_SIZE	(2 * sizeof(seq_t))

static ProtoField xtpfields[] = {

    /* Header Fields */
    PFI_UINT("options", "Command Options",      OPTIONS,u_short,PV_DEFAULT),
    PFI_UINT("offset",  "Data Offset",          OFFSET, u_char, PV_VERBOSE),
    PFI_UBIT("little",  "Little Endian",        LITTLE, 1,      PV_VERBOSE),
    PFI_UBIT("version", "Version",              VERSION,2,      PV_VERBOSE),
    PFI_UBIT("type",    "Packet Type",          TYPE,   5,      PV_TERSE),
    PFI_UINT("key",     "Key",                  KEY,    u_int,  PV_TERSE),
    PFI_UINT("sort",    "Priority Sort",        SORT,   u_int,  PV_DEFAULT),
    PFI_UINT("hrsvd",   "Reserved Header Field",HRSVD,  u_int,  PV_VERBOSE+1),
    PFI_UINT("seq",     "Sequence Number",      SEQ,    u_int,  PV_TERSE),
    PFI_UINT("route",   "Route",                ROUTE,  u_int,  PV_DEFAULT),

    /* Address Segment Fields */
    PFI_UINT("alength",	"Address Segment Length", A_LEN,u_short,PV_VERBOSE),
    PFI_UINT("aformat",	"Address Format",	A_FMT,	u_short,PV_VERBOSE),
    PFI_UINT("arate",	"Address Reqested Rate",A_RATE,	u_int,	PV_VERBOSE),
    PFI_ADDR("aid",	"Address Identification",A_ID,	6,	PV_VERBOSE),
    PFI_UINT("arsvd",	"Address Reserved Field",A_RSVD,u_short,PV_VERBOSE+1),
    PFI_UINT("dsthost",	"Destination IP Address", AIP_DST, u_long,PV_TERSE),
    PFI_UINT("srchost",	"Source IP Address",	AIP_SRC, u_long,PV_TERSE),
    PFI_UINT("dstport",	"Destination Port",	AIP_DPORT,u_short,PV_TERSE),
    PFI_UINT("srcport",	"Source Port",		AIP_SPORT,u_short,PV_TERSE),
    PFI_UINT("ipproto",	"IP Protocol",		AIP_PRO, u_char,PV_DEFAULT),

    PFI_UINT("daddr",	"Direct Address",	AD_KEY, u_long,	PV_TERSE),
    PFI_UINT("daddrmac","Direct Address MAC",	AD_MAC, 6,	PV_DEFAULT),

    /* Control Segment Fields  */
    PFI_UINT("rate", "Max Send Rate",		RATE, u_long,	PV_DEFAULT),
    PFI_UINT("burst","Max Send Burst Size",	BURST, u_long,	PV_DEFAULT),
    PFI_UINT("sync", "Synchonization ID",	SYNC, u_long,	PV_DEFAULT),
    PFI_UINT("echo", "Returned Sync Field",	XECHO, u_long,	PV_DEFAULT),
    PFI_UINT("time", "Time Packet Sent",	TIME, u_long,	PV_DEFAULT),
    PFI_UINT("techo","Returned Time Field",	TECHO, u_long,	PV_DEFAULT),
    PFI_UINT("xkey", "Exchanged Key",		XKEY, u_long,	PV_DEFAULT),
    PFI_UINT("xroute","Exchanged Route",	XROUTE, u_long,	PV_DEFAULT),
    PFI_UINT("crsvd","Reserved Control Field",	CRSVD, u_long,	PV_VERBOSE+1),
    PFI_UINT("alloc","Sending Allocation ",	ALLOC, u_long,	PV_TERSE),
    PFI_UINT("rseq", "Max Rcvd Sequence No",	RSEQ, u_long,	PV_TERSE),
    PFI_UINT("nspan","Number Rexmt Spans",	NSPAN, u_long,	PV_TERSE),
    PFI_BYTE("span", "Retransmission Span",	SPAN, SPAN_SIZE, PV_TERSE),

    /* Diag Segment Fields */
    PFI_UINT("dcode", "Diagnostic Code",        DIAG_C, u_long, PV_TERSE),
    PFI_UINT("dvalue","Diagnostic Value",       DIAG_V, u_long, PV_TERSE),

    /* Route Segment Fields */
    PFI_UINT("rcode",   "Route Code",           ROUTE_C, u_long, PV_TERSE),
    PFI_UINT("rvalue",  "Route Value",          ROUTE_V, u_long, PV_TERSE),

    /* Trailer Fields */
    PFI_UINT("dcheck",	"Data Checksum",	DCHK,  u_long,	PV_VERBOSE),
    PFI_UINT("dseq",	"Delivered Seq No",	DSEQ,  u_long,	PV_TERSE),
    PFI_UBIT("tflags",	"Trailer Flags",	FLAGS, 10,	PV_TERSE),
    PFI_UBIT("align",	"Trailer Alignment",	ALIGN, 6,	PV_VERBOSE),
    PFI_UINT("ttl",	"Time To Live",		TTL,   u_short,	PV_DEFAULT),
    PFI_UINT("htcheck",	"Header/Trailer Checksum",HTCHK, u_long,PV_VERBOSE),
};

#define	XTPFID(pf)	((enum xtpfid) (pf)->pf_id)
#define	XTPFIELD(fid)	xtpfields[(int) fid]

static ProtoMacro xtpnicknames[] = {
	PMI("XTP",	xtpname),
};

/*
 * XTP enumerated types.
 */
static Enumeration types;
static Enumerator typesvec[] = {
	EI_VAL("DATA", HT_DATA),
	EI_VAL("CNTL", HT_CNTL),
	EI_VAL("FIRST", HT_FIRST),
	EI_VAL("PATH", HT_PATH),
	EI_VAL("DIAG", HT_DIAG),
	EI_VAL("MAINT", HT_MAINT),
	EI_VAL("MGMT", HT_MGMT),
	EI_VAL("SUPER", HT_SUPER),
	EI_VAL("ROUTE", HT_ROUTE),
	EI_VAL("RCNTL", HT_RCNTL),
};

static Enumeration dcodes;
static Enumerator dcodesvec[] = {
	EI_VAL("D_UNKNOWN_KEY",	DIAG_UNKNOWN_KEY),
	EI_VAL("D_REFUSED",	DIAG_CONTEXT_REFUSED),
	EI_VAL("D_UNKNOWN_DEST",DIAG_UNKNOWN_DEST),
	EI_VAL("D_DEAD_HOST",	DIAG_DEAD_HOST),
	EI_VAL("D_INVALID_ROUTE",DIAG_INVALID_ROUTE),
	EI_VAL("D_REDIRECT",	DIAG_REDIRECT),
	EI_VAL("D_NOROUTE",	DIAG_NOROUTE),
	EI_VAL("D_NORESOURCE",	DIAG_NORESOURCE),
	EI_VAL("D_PROTERR",	DIAG_PROTERR),
};

static Enumeration rcodes;
static Enumerator rcodesvec[] = {
	EI_VAL("R_RELEASE",	RTE_RELEASE),
	EI_VAL("R_RELACK",	RTE_RELACK),
};

/*
 * XTP constants.
 */
static Enumerator xtpopts[] = { 
	EI_VAL("BTAG",	HO_BTAG),
	EI_VAL("FASTNAK",HO_FASTNAK),
	EI_VAL("DLINE", HO_DEADLINE),
	EI_VAL("SORT",	HO_SORT),
	EI_VAL("RES",	HO_RES),
	EI_VAL("MULTI", HO_MULTI),
	EI_VAL("NOERR", HO_NOERR),
	EI_VAL("DADDR", HO_DADDR),
	EI_VAL("NOCHK", HO_NOCHECK),
	EI_VAL("LITTLE",HO_LITTLE),
};

static Enumerator xtpflags[] = { 
	EI_VAL("SREQ",	TO_SREQ),
	EI_VAL("DREQ",	TO_DREQ),
	EI_VAL("RCLOSE",TO_RCLOSE),
	EI_VAL("WCLOSE",TO_WCLOSE),
	EI_VAL("DEFERCK",TO_DEFERCHK),
	EI_VAL("ETAG",	TO_ETAG),
	EI_VAL("EOM",	TO_EOM),
	EI_VAL("END",	TO_END),
};

/*
 * XTP protocol interfaces.
 */
#define	xtp_setopt	pr_stub_setopt
#define	xtp_resolve	pr_stub_resolve
#define xtp_embed	pr_stub_embed	/* XXX */
#define xtp_compile	pr_stub_compile	/* XXX */
#define xtp_match	pr_stub_match	/* XXX */

static void xtp_decode_cseg(DataStream *ds, PacketView *pv);
static void xtp_decode_aseg(DataStream *ds, PacketView *pv);
static void xtp_decode_trailer(DataStream *ds, ProtoStack *ps, PacketView *pv,
			       xtp_header *h, u_char *dataptr, int datalen);

DefineLeafProtocol(xtp, xtpname, "Xpress Transfer Protocol", PRID_XTP,
		   DS_BIG_ENDIAN, sizeof(xtp_header));

/*
 * XTP operations.
 */
static Index *xtptypes;

static int
xtp_init()
{
	int nested;

	if (!pr_register(&xtp_proto, xtpfields, lengthof(xtpfields),
			 lengthof(typesvec) + lengthof(dcodesvec)
			 + lengthof(rcodesvec) + lengthof(xtpopts)
			 + lengthof(xtpflags) + 5)) {
		return 0;
	}
	nested = 0;
	if (pr_nest(&xtp_proto, PRID_LOOP, PF_XTP, xtpnicknames,
		    lengthof(xtpnicknames))) {
		nested = 1;
	}
	if (pr_nest(&xtp_proto, PRID_ETHER, ETHERTYPE_XTP, xtpnicknames,
		    lengthof(xtpnicknames))) {
		nested = 1;
	}
	if (pr_nest(&xtp_proto, PRID_LLC, ETHERTYPE_XTP, xtpnicknames,
		    lengthof(xtpnicknames))) {
		nested = 1;
	}
	if (!nested)
		return 0;

	in_create(1, sizeof(u_short), 0, &xtptypes);

	en_init(&types, typesvec, lengthof(typesvec), &xtp_proto);
	en_init(&dcodes, dcodesvec, lengthof(dcodesvec), &xtp_proto);
	en_init(&rcodes, rcodesvec, lengthof(rcodesvec), &xtp_proto);
	pr_addnumbers(&xtp_proto, xtpopts, lengthof(xtpopts));
	pr_addnumbers(&xtp_proto, xtpflags, lengthof(xtpflags));

	return 1;
}

/* ARGSUSED */
static int
xtp_fetch(ProtoField *pf, DataStream *ds, ProtoStack *ps, Expr *rex)
{
	xtp_header *h;

	h = (xtp_header *) ds_inline(ds, sizeof *h, sizeof(u_long));
	if (h == 0) {
		if (XTPF_INHDR(pf))
			return ds_field(ds, pf, sizeof *h, rex);
		return 0;
	}

	switch (XTPFID(pf)) {
	  case OPTIONS:
		rex->ex_val = h->options;
		break;
	  case OFFSET:
		rex->ex_val = h->offset;
		break;
	  case LITTLE:
		rex->ex_val = H_LITTLE(h->type);
		break;
	  case VERSION:
		rex->ex_val = H_VERSION(h->type);
		break;
	  case TYPE:
		rex->ex_val = H_TYPE(h->type);
		break;
	  case KEY:
		rex->ex_val = h->key;
		break;
	  case SORT:
		rex->ex_val = h->sort;
		break;
	  case SEQ:
		rex->ex_val = h->seq;
		break;
	  case ROUTE:
		rex->ex_val = h->route;
		break;
	  default:
		return 0;
	}
	rex->ex_op = EXOP_NUMBER;
	return 1;
}

static void
xtp_decode(DataStream *ds, ProtoStack *ps, PacketView *pv)
{
	xtp_header *h, hdr;
	u_char *dataptr;
	int datalen;

	h = (xtp_header *) ds_inline(ds, sizeof *h, sizeof(u_long));
	if (h == 0) {
		h = &hdr;
		(void) ds_bytes(ds, h, MIN(sizeof *h, ds->ds_count));
	}

	pv_showfield(pv, &XTPFIELD(OPTIONS), &h->options,
		     "%-10s", en_bitset(xtpopts, lengthof(xtpopts),
					h->options));
	pv_showfield(pv, &XTPFIELD(OFFSET), &h->offset,
		     "%-1u", h->offset);
	pv_showfield(pv, &XTPFIELD(LITTLE), &h->type,
		     "%-1u", H_LITTLE(h->type));
	pv_showfield(pv, &XTPFIELD(VERSION), &h->type,
		     "%-2u", H_VERSION(h->type));
	if (pv->pv_level >= PV_VERBOSE)
		pv_break(pv);
	pv_showfield(pv, &XTPFIELD(TYPE), &h->type,
		     "%-6.6s", en_name(&types, H_TYPE(h->type)));
	pv_showfield(pv, &XTPFIELD(KEY), &h->key,
		     "%-#8x", h->key);
	pv_showfield(pv, &XTPFIELD(SORT), &h->sort,
		     "%-10u", h->sort);
	pv_showfield(pv, &XTPFIELD(HRSVD), &h->reserved,
		     "%-10u", h->reserved);
	pv_showfield(pv, &XTPFIELD(SEQ), &h->seq,
		     "%-10u", h->seq);
	pv_showfield(pv, &XTPFIELD(ROUTE), &h->route,
		     "%-#8x", h->route);
	pv_break(pv);

	/*
	 * Skip over padding between XTP header and information segment
	 * XXX perhaps later make this a "triple verbose" field?
	 */
	if (h->offset) {
		ds_seek(ds, h->offset, DS_RELATIVE);
		pv->pv_off += h->offset;
	}

	/*
	 * Save some info so I can calculate checksums over addr & cntl segs
	 */
	dataptr = ds->ds_next;
	datalen = ds->ds_count - sizeof (xtp_trailer);

	switch (H_TYPE(h->type)) {
	  case HT_FIRST: 
	  case HT_PATH:
		xtp_decode_aseg(ds, pv);
		break;
	  case HT_CNTL:
	  case HT_RCNTL:
		xtp_decode_cseg(ds, pv);
		break;

	  case HT_DIAG: {
	  	xtp_diag *dseg, diag_seg;

		dseg = (xtp_diag *)
			ds_inline(ds, sizeof *dseg, sizeof(u_long));
		if (dseg == 0) {
			dseg = &diag_seg;
			(void) ds_bytes(ds, dseg,
					MIN(sizeof *dseg, ds->ds_count));
		}
		pv_showfield(pv, &XTPFIELD(DIAG_C), &dseg->code,
			     "%-15.15s", en_name(&dcodes, dseg->code));
		pv_showfield(pv, &XTPFIELD(DIAG_V), &dseg->value,
			     "%-8d", dseg->value);
/* XXX what is format of dseg->msg ? */
		break;
	  }

	  case HT_ROUTE: {
	  	xtp_route *rseg, route_seg;

		rseg = (xtp_route *)
			ds_inline(ds, sizeof *rseg, sizeof(u_long));
		if (rseg == 0) {
			printf ("ds_inline failed\n");
			rseg = &route_seg;
			(void) ds_bytes(ds, rseg,
				MIN(sizeof *rseg, ds->ds_count));
		}
		pv_showfield(pv, &XTPFIELD(ROUTE_C), &rseg->code,
				"%-15.15s", en_name(&rcodes, rseg->code));
		pv_showfield(pv, &XTPFIELD(ROUTE_V), &rseg->value,
				"%-8d", rseg->value);
/* XXX what is format of rseg->msg ? */
		break;
	  }

	}

	pv_decodeframe(pv, NULL, ds, ps);
	xtp_decode_trailer(ds, ps, pv, h, dataptr, datalen);
}

static void
xtp_decode_cseg(DataStream *ds, PacketView *pv)
{
	xtp_cseg *cseg, cntl_seg;
	int i, skiplen;
	seq_t *xspan;
	seq_t skipdata[2*MAX_SPAN];

	cseg = (xtp_cseg *) ds_inline(ds, CSEG_MINLEN, sizeof(u_long));
	if (cseg == 0) {
		cseg = &cntl_seg;
		(void) ds_bytes(ds, cseg, MIN(CSEG_MINLEN, ds->ds_count));
	}

	pv_showfield(pv, &XTPFIELD(RATE), &cseg->rate,
		     "%-10u", cseg->rate);
	pv_showfield(pv, &XTPFIELD(BURST), &cseg->burst,
			"%-10u", cseg->burst);
	pv_showfield(pv, &XTPFIELD(SYNC), &cseg->sync,
			"%-10u", cseg->sync);
	pv_showfield(pv, &XTPFIELD(XECHO), &cseg->echo,
			"%-10u", cseg->echo);
	pv_showfield(pv, &XTPFIELD(TIME), &cseg->time,
			"%-10u", cseg->time);
	pv_showfield(pv, &XTPFIELD(TECHO), &cseg->timeecho,
			"%-10u", cseg->timeecho);
	pv_showfield(pv, &XTPFIELD(XKEY), &cseg->xkey,
			"%-#8x", cseg->xkey);
	pv_showfield(pv, &XTPFIELD(XROUTE), &cseg->xroute,
			"%-#8x", cseg->xroute);
	pv_showfield(pv, &XTPFIELD(CRSVD), &cseg->reserved,
			"%-10u", cseg->reserved);
	pv_showfield(pv, &XTPFIELD(ALLOC), &cseg->alloc,
			"%-10u", cseg->alloc);
	pv_showfield(pv, &XTPFIELD(RSEQ), &cseg->rseq,
			"%-10u", cseg->rseq);
	pv_showfield(pv, &XTPFIELD(NSPAN), &cseg->nspan,
			"%-10u", cseg->nspan);

	for (i = MIN(cseg->nspan, MAX_SPAN); i > 0; --i) {
		xspan = (seq_t *)
			ds_inline(ds, 2*sizeof(seq_t), sizeof(u_long));
		if (xspan != 0) {
			pv_showfield(pv, &XTPFIELD(SPAN), xspan,
				     "%u:%u", xspan[0], xspan[1]);
		}
	}

	/*
	 * There may be more spans in the packet format than indicated by
	 * nspan. So skip to the XTP trailer so these unused spans aren't
	 * interpreted as data. 
	 * XXX Perhaps later make this a "triple verbose" field? 
	 */

	if (pv->pv_level >= PV_VERBOSE+1) {
		skiplen = ds->ds_size - sizeof (xtp_trailer) - DS_TELL(ds);
		if (skiplen > 0)
			pv_decodehex(pv, ds, 0, skiplen);
	} else {
		ds_seek(ds, ds->ds_size - sizeof (xtp_trailer), DS_ABSOLUTE);
		pv->pv_off = DS_TELL(ds);
	}
}

static void
xtp_decode_aseg(DataStream *ds, PacketView *pv)
{
	xtp_addr *aseg, addr_seg;

	aseg = (xtp_addr *) ds_inline(ds, ADDRSEG_MINLEN, sizeof(u_long));
	if (aseg == 0) {
		aseg = &addr_seg;
		(void) ds_bytes(ds, aseg,
			MIN(ADDRSEG_MINLEN, ds->ds_count));
	}

	(void) ds_bytes(ds, &aseg->addr,
		MIN(aseg->length - ADDRSEG_MINLEN, ds->ds_count));

	pv_showfield(pv, &XTPFIELD(A_LEN), &aseg->length,
		     "%-5u", aseg->length);
	pv_showfield(pv, &XTPFIELD(A_FMT), &aseg->format,
			"%#04x", aseg->format);
	pv_showfield(pv, &XTPFIELD(A_RATE), &aseg->ratereq,
			"%-10u", aseg->ratereq);
	pv_showfield(pv, &XTPFIELD(A_ID), aseg->id, 
		     "%-25.25s", ether_hostname(aseg->id));
	pv_showfield(pv, &XTPFIELD(A_RSVD), &aseg->reserved,
			"%-5u", aseg->reserved);

	if (pv->pv_level >= PV_VERBOSE)
		pv_break(pv);

	switch (aseg->format) {
	  case XTPAF_INET: {
		char		*protoname;
		struct in_addr	tmp_addr;
		Protocol 	*spr, *dpr;

/* XXX can clean up this junk if IP addresses in xtp.h are type in_addr */
		tmp_addr.s_addr = aseg->addr.inet.dstaddr;
		pv_showfield(pv, &XTPFIELD(AIP_DST), &aseg->addr.inet.dstaddr,
			     "%-21.21s", ip_hostname(tmp_addr, IP_HOST));

		tmp_addr.s_addr = aseg->addr.inet.srcaddr;
		pv_showfield(pv, &XTPFIELD(AIP_SRC), &aseg->addr.inet.srcaddr,
			     "%-21.21s", ip_hostname(tmp_addr, IP_HOST));

		pv_break(pv);	/* get ports to line up w/ IP addresses */

		protoname = ip_protoname(aseg->addr.inet.ipproto);
		spr = dpr = NULL;
		pv_showfield(pv, &XTPFIELD(AIP_DPORT), &aseg->addr.inet.dstport,
			     "%-21.21s", ip_service(aseg->addr.inet.dstport,
						    protoname, spr));
		pv_showfield(pv, &XTPFIELD(AIP_SPORT), &aseg->addr.inet.srcport,
			     "%-21.21s", ip_service(aseg->addr.inet.srcport,
						    protoname, dpr));
		pv_showfield(pv, &XTPFIELD(AIP_PRO), &aseg->addr.inet.ipproto,
			     "%-6s", protoname);
		break;
	  } /* case of XTPAF_INET */

	  case XTPAF_DADDR:
		pv_showfield(pv, &XTPFIELD(AD_KEY), &aseg->addr.daddr.key,
			"%-#8x", aseg->addr.daddr.key);
		break;

	  case XTPAF_ISO:
	  case XTPAF_XNS:
	  default:
		pv_showhex(pv, (unsigned char *) &aseg,
			   ADDRSEG_MINLEN, aseg->length);
		break;
	}
}

static void
xtp_decode_trailer(DataStream *ds, ProtoStack *ps, PacketView *pv,
		   xtp_header *h, u_char *dataptr, int datalen)
{
	xtp_trailer *t;
	u_long chksum;
	int saved_offset, saved_pvoff, skip;

	/*
	 * If we didn't capture entire XTP trailer, don't try to decode it
	 */

	if (ps->ps_snoop->sn_rawproto->pr_maxhdrlen + ds->ds_size
	    < ps->ps_snhdr->snoop_packetlen)
		return;

	saved_offset = DS_TELL(ds);
	saved_pvoff = pv->pv_off;
	ds_seek(ds, ds->ds_size - sizeof *t, DS_ABSOLUTE);
	pv->pv_off = DS_TELL(ds);
	t = (xtp_trailer *) ds_inline(ds, sizeof *t, sizeof(u_long));
	if (t == 0)
		skip = 0;
	else {
		/* earlier check should be sufficient, but let's be sure */
		pv_break(pv);

		if (t->flags & TO_DEFERCHK) {
			chksum = t->dcheck;
		} else {
			chksum = xtp_long_catxor(dataptr, datalen, 0);
		}
		if (t->dcheck == chksum) {
			pv_showfield(pv, &XTPFIELD(DCHK), &t->dcheck,
				     "%#010x", t->dcheck);
		} else {
			pv_showfield(pv, &XTPFIELD(DCHK), &t->dcheck,
				     "%#010x [!= %#010x]", t->dcheck, chksum);
		}
		pv_showfield(pv, &XTPFIELD(DSEQ), &t->dseq, "%-10u", t->dseq);

		/* can't pass addr of bit field */
		pv_showfield(pv, &XTPFIELD(FLAGS), &t->dseq + 1,
			     "%-18s", en_bitset(xtpflags, lengthof(xtpflags),
						t->flags));

		if (pv->pv_level >= PV_VERBOSE)
			pv_break(pv);
		pv_showfield(pv, &XTPFIELD(ALIGN), &t->dseq + 1,
			     "%-2u", t->align);
		pv_showfield(pv, &XTPFIELD(TTL), &t->ttl, "%-5u", t->ttl);

		if (h->options & HO_NOCHECK) {
			chksum = t->htcheck;
		} else {
			chksum = xtp_htcheck(h, t);
		}
		if (t->htcheck != chksum) {
			pv_showfield(pv, &XTPFIELD(HTCHK), &t->htcheck,
				     "%#010x [!= %#010x]", t->htcheck, chksum);
		} else {
			pv_showfield(pv, &XTPFIELD(HTCHK), &t->htcheck,
				     "%#010x", t->htcheck);
		}

		skip = sizeof *t + t->align;
	}
	/*
	 * Restore datastream so we can decode info between XTP hdr and trlr
	 * But reduce count so we don't interpret XTP trailer as data.
	 */
	ds_seek(ds, saved_offset, DS_ABSOLUTE);
	ds->ds_count -= skip;
	ds->ds_size -= skip;
	pv->pv_off = saved_pvoff;
}

