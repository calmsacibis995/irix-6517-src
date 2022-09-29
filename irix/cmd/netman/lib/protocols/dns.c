/*
 * Copyright 1989 Silicon Graphics, Inc.  All rights reserved.
 *
 * Internet Domain Name Service (DNS) protocol, defined in RFC 1034 and 1035.
 */

#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/nameser.h>
#include <arpa/inet.h>
#include "enum.h"
#include "protodefs.h"
#include "protocols/ip.h"

/* These are for sun */
#ifndef T_TXT
#define T_TXT           16              /* text strings */
#endif
#ifndef T_RP
#define T_RP            17              /* responsible person */
#endif
#ifndef C_HS
#define C_HS            4               /* for Hesiod name server at MIT */
#endif

#ifdef sun
char *p_time(unsigned long);
#endif


char	dnsname[] = "dns";

#define	DNS_HDRSIZE	(sizeof(HEADER))	/* header length in bytes */

enum dnsfid {
    ID, QR, OPCODE, AA, TC, RD, RA, UNUSED, RCODE,
    QDCOUNT, ANCOUNT, NSCOUNT, ARCOUNT,
    NAME, TYPE, CLASS, TTL, DLEN,
    ADDRESS, PROTOCOL, PORT,
    NS,
    CNAME,
    DOMAIN, OWNER, SERIAL, REFRESH, RETRY, EXPIRE, MINTTL,
    MB, MG, MR,
    PTR,
    CPU, OS,
    MREQUESTS, MERRORS,
    MXPREF, MXNAME,
    TEXT,
    UINFO, UID, GID
};

static ProtoField dnsfields[] = {
/* Packet header: names from <arpa/nameser.h> */
    PFI_UBIT("id",	"Identifier",		ID,	   16,      PV_TERSE),
    PFI_UBIT("qr",	"Query/Response Flag",	QR,	   1,	    PV_TERSE),
    PFI_UBIT("opcode",	"Operation Code",	OPCODE,	   4,	    PV_DEFAULT),
    PFI_UBIT("aa",	"Authoritative Answer",	AA,	   1,	    PV_DEFAULT),
    PFI_UBIT("tc",	"Truncation",		TC,	   1,	    PV_VERBOSE),
    PFI_UBIT("rd",	"Recursion Desired",	RD,	   1,	    PV_VERBOSE),
    PFI_UBIT("ra",	"Recursion Available",	RA,	   1,	    PV_VERBOSE),
    PFI_UBIT("unused",	"Unused",		UNUSED,	   3,	    PV_VERBOSE),
    PFI_UBIT("rcode",	"Response Code",	RCODE,	   4,	    PV_TERSE),
    PFI_UBIT("qdcount",	"Question Count",	QDCOUNT,   16,      PV_DEFAULT),
    PFI_UBIT("ancount",	"Answer Count",		ANCOUNT,   16,      PV_DEFAULT),
    PFI_UBIT("nscount",	"Name Server Count",	NSCOUNT,   16,      PV_DEFAULT),
    PFI_UBIT("arcount",	"Additional Record Count",ARCOUNT, 16,      PV_DEFAULT),

/* Resource record header */
    PFI_VAR("name",	"Name",			NAME,	   60,	    PV_DEFAULT),
    PFI_UINT("type",	"Type",			TYPE,	   u_short, PV_DEFAULT),
    PFI_UINT("class",	"Class",		CLASS,	   u_short, PV_VERBOSE),
    PFI_UINT("ttl",	"Time to Live",		TTL,	   u_long,  PV_DEFAULT),
    PFI_UINT("dlen",	"Data Length",		DLEN,	   u_short, PV_VERBOSE),

/* Resource record data -- mostly listed in order by type */

    /* A, WKS */
    PFI_UINT("address",	"Address",		ADDRESS,   u_long,  PV_DEFAULT),
    PFI_UINT("protocol","Protocol",		PROTOCOL,  u_char,  PV_DEFAULT),
    PFI_UINT("port",	"Port",			PORT,	   u_short, PV_DEFAULT),

    PFI_VAR("ns",	"Name Server",		NS,	   60,	    PV_DEFAULT),
    PFI_VAR("cname",	"Canonical Name",	CNAME,	   60,	    PV_DEFAULT),

    /* SOA */
    PFI_VAR("domain",	"Domain Name",		DOMAIN,    60,	    PV_DEFAULT),
    PFI_VAR("owner",	"Domain Owner",		OWNER,	   60,	    PV_DEFAULT),
    PFI_UINT("serial",	"Serial Number",	SERIAL,    u_long,  PV_DEFAULT),
    PFI_UINT("refresh",	"Refresh Interval",	REFRESH,   u_long,  PV_DEFAULT),
    PFI_UINT("retry",	"Retry Interval",	RETRY,	   u_long,  PV_DEFAULT),
    PFI_UINT("expire",	"Expiration Time Limit",EXPIRE,    u_long,  PV_DEFAULT),
    PFI_UINT("minttl",	"Minimum Time to Live",	MINTTL,    u_long,  PV_DEFAULT),

    PFI_VAR("mb",	"Mailbox Host",		MB,	   60,	    PV_DEFAULT),
    PFI_VAR("mg",	"Mail Group",		MG,	   60,	    PV_DEFAULT),
    PFI_VAR("mr",	"Mailbox Rename",	MR,	   60,	    PV_DEFAULT),

    PFI_VAR("ptr",	"Domain Name Pointer",	PTR,	   60,	    PV_DEFAULT),

    /* HINFO */
    PFI_VAR("cpu",	"CPU Type",		CPU,	   60,	    PV_DEFAULT),
    PFI_VAR("os",	"Operating System Type",OS,	   60,	    PV_DEFAULT),

    /* MINFO */
    PFI_VAR("mrequests","Request Mailbox",	MREQUESTS, 60,	    PV_DEFAULT),
    PFI_VAR("merrors",	"Error Mailbox",	MERRORS,   60,	    PV_DEFAULT),

    /* MX */
    PFI_UINT("mxpref",	"Preference",		MXPREF,    u_short, PV_DEFAULT),
    PFI_VAR("mxname",	"Exchange Host",	MXNAME,    60,	    PV_DEFAULT),

    PFI_VAR("text",	"Text Data",		TEXT,	   60,	    PV_DEFAULT),

    PFI_VAR("uinfo",	"User Information",	UINFO,	   60,	    PV_DEFAULT),
    PFI_UINT("uid",	"User Identifier",	UID,	   u_long,  PV_DEFAULT),
    PFI_UINT("gid",	"Group Identifier",	GID,	   u_long,  PV_DEFAULT),
};

#define dns_pushconst(pv, label) \
	if (pv->pv_level > PV_TERSE) pv_pushconst(pv, &dns_proto, label, label)
#define dns_popconst(pv) \
	if (pv->pv_level > PV_TERSE) pv_pop(pv)

#define	DNSFID(pf)	((enum dnsfid) (pf)->pf_id)
#define	DNSFIELD(fid)	dnsfields[(int) fid]

static ProtoMacro dnsnicknames[] = {
    PMI("DNS",	dnsname),
};

static Enumeration opcodes;
static Enumerator opcodevec[] = {
    EI(QUERY), EI(IQUERY), EI(STATUS),
};

static Enumeration errcodes;
static Enumerator errcodevec[] = {
    EI(NOERROR), EI(FORMERR), EI(SERVFAIL), EI(NXDOMAIN),
    EI(NOTIMP), EI(REFUSED),
};

static Enumeration types;
static Enumerator typevec[] = {
    EI_VAL("A", T_A),
    EI_VAL("NS", T_NS),
    EI_VAL("MD", T_MD),
    EI_VAL("MF", T_MF),
    EI_VAL("CNAME", T_CNAME),
    EI_VAL("SOA", T_SOA),
    EI_VAL("MB", T_MB),
    EI_VAL("MG", T_MG),
    EI_VAL("MR", T_MR),
    EI_VAL("NULL", T_NULL),
    EI_VAL("WKS", T_WKS),
    EI_VAL("PTR", T_PTR),
    EI_VAL("HINFO", T_HINFO),
    EI_VAL("MINFO", T_MINFO),
    EI_VAL("MX", T_MX),
    EI_VAL("TXT", T_TXT),
    EI_VAL("UINFO", T_UINFO),
    EI_VAL("UID", T_UID),
    EI_VAL("GID", T_GID),
    EI_VAL("UNSPEC", T_UNSPEC),
    EI_VAL("AXFR", T_AXFR),
    EI_VAL("MAILB", T_MAILB),
    EI_VAL("MAILA", T_MAILA),
    EI_VAL("ANY", T_ANY),
};

static Enumeration classes;
static Enumerator classvec[] = {
    EI_VAL("IN", C_IN),
    EI_VAL("CHAOS", C_CHAOS),
    EI_VAL("HESIOD", C_HS),
    EI_VAL("ANY", C_ANY),
};

static char questions[]	  = "questions";
static char answers[]	  = "answers";
static char authorities[] = "authorities";
static char additional[]  = "additional";

static ProtoMacro dnsmacros[] = {
    PMI(questions,	"qdcount != 0"),
    PMI(answers,	"ancount != 0"),
    PMI(authorities,	"nscount != 0"),
    PMI(additional,	"arcount != 0"),
};

#define dns_setopt	pr_stub_setopt
#define dns_embed	pr_stub_embed
#define dns_resolve	pr_stub_resolve
#define dns_compile	pr_stub_compile
#define dns_match	pr_stub_match

DefineLeafProtocol(dns, dnsname, "Domain Name Service", PRID_DNS,
		   DS_BIG_ENDIAN, DNS_HDRSIZE);

static int
dns_init()
{
	int nested;
	struct servent *sp;

	if (!pr_register(&dns_proto, dnsfields, lengthof(dnsfields),
			 lengthof(opcodevec) + lengthof(errcodevec)
			 + lengthof(typevec) + lengthof(classvec)
			 + lengthof(dnsmacros))) {
		return 0;
	}

	nested = 0;
	sp = getservbyname(dnsname, "udp");
	if (pr_nest(&dns_proto, PRID_UDP,
		    (sp ? sp->s_port : NAMESERVER_PORT),
		    dnsnicknames, lengthof(dnsnicknames))) {
		nested = 1;
	}
#ifdef NOT_YET
	sp = getservbyname(dnsname, "tcp");
	if (pr_nest(&dns_proto, PRID_TCP,
		    (sp ? sp->s_port : NAMESERVER_PORT),
		    dnsnicknames, lengthof(dnsnicknames))) {
		nested = 1;
	}
#endif
	if (!nested)
		return 0;
	en_init(&opcodes,  opcodevec,  lengthof(opcodevec),  &dns_proto);
	en_init(&errcodes, errcodevec, lengthof(errcodevec), &dns_proto);
	en_init(&types,    typevec,    lengthof(typevec),    &dns_proto);
	en_init(&classes,  classvec,   lengthof(classvec),   &dns_proto);
	pr_addmacros(&dns_proto, dnsmacros, lengthof(dnsmacros));
	return 1;
}

static void
dns_decode_name(DataStream *ds, PacketView *pv, char *msg, enum dnsfid fid)
{
	char name[MAXDNAME];
	int len;
	ProtoField *pf;

	/*
	 * Fill name with question marks in case dn_expand fails but stuffs
	 * part of the compressed name.
	 */
	memset(name, '?', sizeof name);
	len = dn_expand(msg, ds->ds_next+ds->ds_count, ds->ds_next,
			name, sizeof(name));
	if (len < 0)
		len = ds->ds_count;
	else if (name[0] == '\0')
		(void) strcpy(name, "(root)");

	pf = &DNSFIELD(fid);
	pv_showvarfield(pv, pf, ds->ds_next, len,
			"%-*.*s", pf->pf_cookie, pf->pf_cookie, name);
	(void) ds_seek(ds, len, DS_RELATIVE);
}

static void
dns_decode_time(DataStream *ds, PacketView *pv, enum dnsfid fid)
{
	u_long time;
	
	(void) ds_u_long(ds, &time);
	pv_showfield(pv, &DNSFIELD(fid), &time,
		     "%-lu (%s)", time, p_time(time));
}

static void
dns_decode_rdata(DataStream *ds, PacketView *pv, char *msg, u_short dlen,
		 u_short type, u_short class)
{
	struct in_addr addr;
	struct in_addr netaddr;
	char *cp;

	/*
	 * Print type specific data, if appropriate
	 */
	switch (type) {
	case T_A:
		switch (class) {
		case C_IN:
			(void) ds_in_addr(ds, &addr);
			netaddr.s_addr = htonl(addr.s_addr);
			pv_showfield(pv, &DNSFIELD(ADDRESS), &addr,
				     "%-15.15s", inet_ntoa(netaddr));
			if (dlen > sizeof addr) {
				u_char proto;
				u_short port;

				(void) ds_u_char(ds, &proto);
				pv_showfield(pv, &DNSFIELD(PROTOCOL), &proto,
					     "%-6.6s", ip_protoname(proto));
				(void) ds_u_short(ds, &port);
				pv_showfield(pv, &DNSFIELD(PORT), &port,
					     "%-4d", port);
			}
			break;
		default:
			(void) ds_seek(ds, dlen, DS_RELATIVE);
			pv->pv_off += dlen;
		}
		break;

#define NAMEFIELD(x) \
	case T_##x : \
		dns_decode_name(ds, pv, msg, x); \
		break;

	NAMEFIELD(NS)
	NAMEFIELD(CNAME)
	NAMEFIELD(MB)
	NAMEFIELD(MG)
	NAMEFIELD(MR)
	NAMEFIELD(PTR)

#undef NAMEFIELD

	case T_SOA:
	    {
		u_long serial;

		dns_decode_name(ds, pv, msg, DOMAIN);
		dns_decode_name(ds, pv, msg, OWNER);

		(void) ds_u_long(ds, &serial);
		pv_showfield(pv, &DNSFIELD(SERIAL), &serial,
			     "%-10lu", serial);
		dns_decode_time(ds, pv, REFRESH);
		dns_decode_time(ds, pv, RETRY);
		dns_decode_time(ds, pv, EXPIRE);
		dns_decode_time(ds, pv, MINTTL);
		break;
	    }

	case T_WKS:
	    {
		u_char *limit;
		u_char proto;
		char *protoname;
		int n = 0, col = 0;
		static char LABEL[] = "services";
#define MAXCOL	(constrlen(LABEL) + 35)

		limit = ds->ds_next + MIN(dlen, ds->ds_count);
		(void) ds_in_addr(ds, &addr);
		netaddr.s_addr = htonl(addr.s_addr);
		pv_showfield(pv, &DNSFIELD(ADDRESS), &addr,
			     "%-15.15s", inet_ntoa(netaddr));

		(void) ds_u_char(ds, &proto);
		protoname = ip_protoname(proto);
		pv_showfield(pv, &DNSFIELD(PROTOCOL), &proto,
			     "%-6.6s", protoname);

		pv_printf(pv, "\n%s", LABEL);
		while (ds->ds_next < limit) {
		    u_char c;

		    (void) ds_u_char(ds, &c);
		    do {
			if (c & 0x80) {
			    col += pv_printf(pv, "%s %s",
					     col > 0 ? "," : "",
					     ip_service(n, protoname, NULL));
			    if (col >= MAXCOL) {
				pv_printf(pv, "\n%*s", sizeof(LABEL)-1, " ");
				col = 0;
			    }
			}
			c <<= 1;
		    } while (++n & 07);
		    pv->pv_off++;
		}
		break;
	    }

	case T_HINFO:
	    {
		u_char len;

		if (ds_u_char(ds, &len)) {
		    pv->pv_off++;
		    len = MIN(len, ds->ds_count);
		    cp = ds_inline(ds, len, 1);
		    pv_showvarfield(pv, &DNSFIELD(CPU), cp, len, 
				    "%-15.*s", len, cp);
		}
		if (ds_u_char(ds, &len)) {
		    pv->pv_off++;
		    len = MIN(len, ds->ds_count);
		    cp = ds_inline(ds, len, 1);
		    pv_showvarfield(pv, &DNSFIELD(OS), cp, len, 
				    "%-10.*s", len, cp);
		}
		break;
	    }

	case T_MINFO:
		dns_decode_name(ds, pv, msg, MREQUESTS);
		dns_decode_name(ds, pv, msg, MERRORS);
		break;

	case T_MX:
	    {
		u_short mxpref;

		(void) ds_u_short(ds, &mxpref);
		pv_showfield(pv, &DNSFIELD(MXPREF), &mxpref,
			     "%-3d", mxpref);

		dns_decode_name(ds, pv, msg, MXNAME);
		break;
	    }

	case T_TXT:
	    {
		ProtoField *pf = &DNSFIELD(TEXT);

		cp = ds_inline(ds, MIN(dlen, ds->ds_count), 1);
		pv_showvarfield(pv, pf, cp, dlen,
				"%-*.*s", pf->pf_cookie, *cp, cp + 1);
		break;
	    }

	case T_UINFO:
	    {
		ProtoField *pf = &DNSFIELD(UINFO);

		cp = ds_inline(ds, MIN(dlen, ds->ds_count), 1);
		pv_showvarfield(pv, pf, cp, dlen,
				"%-*.*s", pf->pf_cookie, pf->pf_cookie, cp);
		break;
	    }

	case T_UID:
		if (dlen == 4) {
			u_long uid;

			(void) ds_u_long(ds, &uid);
			pv_showfield(pv, &DNSFIELD(UID), &uid,
				     "%-6d", uid);
		}
		break;

	case T_GID:
		if (dlen == 4) {
			u_long gid;

			(void) ds_u_long(ds, &gid);
			pv_showfield(pv, &DNSFIELD(GID), &gid,
				     "%-6d", gid);
		}
		break;

	}
}

static void
dns_decode_rr(DataStream *ds, PacketView *pv, char *msg, int is_question)
{
	u_short type, class, dlen;
	int ok;

	dns_decode_name(ds, pv, msg, NAME);

	(void) ds_u_short(ds, &type);
	pv_showfield(pv, &DNSFIELD(TYPE), &type,
		     "%-6.6s", en_name(&types, type));

	(void) ds_u_short(ds, &class);
	pv_showfield(pv, &DNSFIELD(CLASS), &class,
		     "%-5.5s", en_name(&classes, class));

	if (is_question)
		return;
	dns_decode_time(ds, pv, TTL);

	ok = ds_u_short(ds, &dlen);
	pv_showfield(pv, &DNSFIELD(DLEN), &dlen, "%-3d", dlen);
	if (ok && dlen > 0) {
		pv_break(pv);
		dns_decode_rdata(ds, pv, msg, dlen, type, class);
	}
}

static HEADER *
dns_inline_header(DataStream *ds, ProtoStack *ps)
{
#ifdef NOT_YET
	struct ipframe *ipf;
	u_short *lenp;

	ipf = PS_TOP(ps, struct ipframe);
	if (ipf->ipf_prototype == IPPROTO_TCP) {
	    lenp = (u_short *) ds_inline(ds, sizeof(u_short), sizeof(short));
	    if (lenp == NULL)
		    return NULL;
	}
#endif
	return (HEADER *) ds_inline(ds, sizeof(HEADER), sizeof(short));
}

static void
dns_decode(DataStream *ds, ProtoStack *ps, PacketView *pv)
{
	char *msg, *ptr;
	HEADER *h, hdr;
	int count;

	msg = (char *) ds->ds_next;
	h = dns_inline_header(ds, ps);
	if (h == NULL) {
		h = &hdr;
		(void) ds_bytes(ds, h, MIN(sizeof *h, ds->ds_count));
	}
	pv_showfield(pv, &DNSFIELD(ID), h, "%-5u", h->id);
	/* ptr = (char *)(&h->id + 1); */
	ptr = ((char *)h) + 2;  /* since arpa/nameser.h has changed --vaz */
	pv_showfield(pv, &DNSFIELD(QR), ptr,
		     "%1d %-10.10s", h->qr, h->qr ? "(response)" : "(query)");
	pv_showfield(pv, &DNSFIELD(OPCODE), ptr,
		     "%-8.8s", en_name(&opcodes, h->opcode));
	pv_showfield(pv, &DNSFIELD(AA), ptr, "%1d", h->aa);
	pv_showfield(pv, &DNSFIELD(TC), ptr, "%1d", h->tc);
	pv_showfield(pv, &DNSFIELD(RD), ptr, "%1d", h->rd);
	ptr++;
	pv_showfield(pv, &DNSFIELD(RA), ptr, "%1d", h->ra);
	/* pv_showfield(pv, &DNSFIELD(PR), ptr, "%1d", h->pr); */
	pv_showfield(pv, &DNSFIELD(UNUSED), ptr, "%#1x", h->unused);

	/* If terse, print the response code only for responses. */
	pv_showfield(pv, &DNSFIELD(RCODE), ptr,
		     h->qr ? "%-8.8s" : "@d%-8.8s",
		     en_name(&errcodes, h->rcode));

/*
	pv_showfield(pv, &DNSFIELD(QDCOUNT), &h->qdcount,
		     "%-2d", h->qdcount);
	pv_showfield(pv, &DNSFIELD(ANCOUNT), &h->ancount,
		     "%-2d", h->ancount);
	pv_showfield(pv, &DNSFIELD(NSCOUNT), &h->nscount,
		     "%-2d", h->nscount);
	pv_showfield(pv, &DNSFIELD(ARCOUNT), &h->arcount,
		     "%-2d", h->arcount);
*/
	ptr++;
	pv_showfield(pv, &DNSFIELD(QDCOUNT), ptr,
		     "%-2d", h->qdcount);
	ptr += 2;
	pv_showfield(pv, &DNSFIELD(ANCOUNT), ptr,
		     "%-2d", h->ancount);
	ptr += 2;
	pv_showfield(pv, &DNSFIELD(NSCOUNT), ptr,
		     "%-2d", h->nscount);
	ptr += 2;
	pv_showfield(pv, &DNSFIELD(ARCOUNT), ptr,
		     "%-2d", h->arcount);


	count = ntohs(h->qdcount);
	if (count > 0) {
		dns_pushconst(pv, questions);
		while (--count >= 0)
			dns_decode_rr(ds, pv, msg, 1);
		dns_popconst(pv);
	}
	count = ntohs(h->ancount);
	if (count > 0) {
		dns_pushconst(pv, answers);
		while (--count >= 0)
			dns_decode_rr(ds, pv, msg, 0);
		dns_popconst(pv);
	}
	count = ntohs(h->nscount);
	if (count > 0) {
		dns_pushconst(pv, authorities);
		while (--count >= 0)
			dns_decode_rr(ds, pv, msg, 0);
		dns_popconst(pv);
	}
	count = ntohs(h->arcount);
	if (count > 0) {
		dns_pushconst(pv, additional);
		while (--count >= 0)
			dns_decode_rr(ds, pv, msg, 0);
		dns_popconst(pv);
	}
}

/* ARGSUSED */
static int
dns_fetch(ProtoField *pf, DataStream *ds, ProtoStack *ps, Expr *rex)
{
	register HEADER *h;

	h = dns_inline_header(ds, ps);
	if (h == NULL) {
		if (pf->pf_id > (int) ARCOUNT)
			return 0;
		return ds_field(ds, pf, sizeof *h, rex);
	}
	switch (DNSFID(pf)) {
	case ID:
		rex->ex_val = ntohs(h->id);
		break;
	case QR:
		rex->ex_val = h->qr;
		break;
	case OPCODE:
		rex->ex_val = h->opcode;
		break;
	case AA:
		rex->ex_val = h->aa;
		break;
	case TC:
		rex->ex_val = h->tc;
		break;
	case RD:
		rex->ex_val = h->rd;
		break;
	case RA:
		rex->ex_val = h->ra;
		break;
	/*
	case PR:
		rex->ex_val = h->pr;
		break;
	*/
	case RCODE:
		rex->ex_val = h->rcode;
		break;
	case QDCOUNT:
		rex->ex_val = ntohs(h->qdcount);
		break;
	case ANCOUNT:
		rex->ex_val = ntohs(h->ancount);
		break;
	case NSCOUNT:
		rex->ex_val = ntohs(h->nscount);
		break;
	case ARCOUNT:
		rex->ex_val = ntohs(h->arcount);
		break;

	case TYPE: /* Find the type for a question */
		if (ntohs(h->qdcount) > 0) {
			unsigned char *name;
			int len;
			u_short type;

			name = ds->ds_next;
			len = dn_skipname(name, name + ds->ds_count);
			if (len < 0
			    || !ds_seek(ds, len, DS_RELATIVE)
			    || !ds_u_short(ds, &type)) {
				return 0;
			}
			rex->ex_val = type;
		}
		break;
	}
	rex->ex_op = EXOP_NUMBER;
	return 1;
}

#ifdef sun
/*
 * Return a mnemonic for a time to live
 */
char *
p_time(value)
	u_long value;
{
	int secs, mins, hours;
	static char nbuf[40];
	register char *p;

	if (value == 0) {
		strcpy(nbuf, "0 secs");
		return(nbuf);
	}

	secs = value % 60;
	value /= 60;
	mins = value % 60;
	value /= 60;
	hours = value % 24;
	value /= 24;

#define	PLURALIZE(x)	x, (x == 1) ? "" : "s"
	p = nbuf;
	if (value) {
		(void)sprintf(p, "%d day%s", PLURALIZE(value));
		while (*++p);
	}
	if (hours) {
		if (value)
			*p++ = ' ';
		(void)sprintf(p, "%d hour%s", PLURALIZE(hours));
		while (*++p);
	}
	if (mins) {
		if (value || hours)
			*p++ = ' ';
		(void)sprintf(p, "%d min%s", PLURALIZE(mins));
		while (*++p);
	}
	if (secs || ! (value || hours || mins)) {
		if (value || hours || mins)
			*p++ = ' ';
		(void)sprintf(p, "%d sec%s", PLURALIZE(secs));
	}
	return(nbuf);
}
#endif
