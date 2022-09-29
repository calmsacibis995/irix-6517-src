/*
 * Copyright 1990 Silicon Graphics, Inc.  All rights reserved.
 *
 * User Datagram Protocol (UDP), defined in RFC 768.
 */
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/udp.h>
#include "index.h"
#include "protodefs.h"
#include "protocols/ip.h"
#include "protocols/sunrpc.h"

char udpname[] = "udp";

/*
 * UDP field identifiers and descriptors.
 */
enum udpfid { SPORT=IPFID_SPORT, DPORT=IPFID_DPORT, LEN, SUM };

static ProtoField udpfields[] = {
	PFI_UINT("sport", "Source Port",      SPORT, u_short,	PV_TERSE),
	PFI_UINT("dport", "Destination Port", DPORT, u_short,	PV_TERSE),
	PFI_UINT("len",   "Length",	      LEN,   u_short,	PV_VERBOSE),
	PFI_UINT("sum",   "Checksum",	      SUM,   u_short,	PV_VERBOSE),
};

#define	UDPFID(pf)	((enum udpfid) (pf)->pf_id)
#define	UDPFIELD(fid)	udpfields[(int) fid]

static ProtoMacro udpnicknames[] = {
	PMI("UDP",	udpname),
	PMI("nfs",	"udp.sunrpc.nfs"),
	PMI("NFS",	"udp.sunrpc.nfs"),
};

static ProtoMacro udpmacros[] = {
	PMI("between",
	    "sport == $1 && dport == $2 || dport == $1 && sport == $2"),
	PMI("port",	"sport == $1 || dport == $1"),
};

static ProtOptDesc udpoptdesc[] = {
	POD("setport", UDP_PROPT_SETPORT,
		       "Map port number to protocol ([host:]port/protocol)"),
};

/*
 * UDP protocol interface and operations.
 */
DefineProtocol(udp, udpname, "User Datagram Protocol", PRID_UDP,
	       DS_BIG_ENDIAN, sizeof(struct udphdr), 0,
	       0, udpoptdesc, lengthof(udpoptdesc), 0);

static Index	*udpports;
static Protocol	*sunrpc;

/*
 * UDP protocol functions.
 */
/* ARGSUSED */
int
udp_func_badsum(Expr *argv, DataStream *ds, ProtoStack *ps, Expr *rex)
{
	struct udphdr *uh;
	u_short realsum, idealsum;
	struct ipframe *ipf;

	uh = (struct udphdr *) ds_inline(ds, sizeof *uh, IP_HDRGRAIN);
	if (uh == 0)
		return 0;
	ipf = PS_TOP(ps, struct ipframe);
	if (ipf == 0)
		return 0;
	realsum = uh->uh_sum;
	if (realsum == 0)
		return 0;
	uh->uh_sum = 0;
	if (!ip_checksum_pseudohdr(ipf, (char *) uh,
				   sizeof *uh + ds->ds_count, &idealsum)) {
		uh->uh_sum = realsum;
		return 0;
	}
	uh->uh_sum = realsum;
	if (idealsum == 0)
		idealsum = ~idealsum;
	rex->ex_op = EXOP_NUMBER;
	rex->ex_val = (ntohs(realsum) != idealsum);
	return 1;
}

static ProtoFuncDesc udpfunctions[] = {
	PFD("badsum", udp_func_badsum, 0,
	    "Match packet if UDP checksum is incorrect"),
};

static int
udp_init()
{
	if (!pr_register(&udp_proto, udpfields, lengthof(udpfields),
			 lengthof(udpmacros) + 12)) {
		return 0;
	}
	if (!pr_nest(&udp_proto, PRID_IP, IPPROTO_UDP, udpnicknames,
		     lengthof(udpnicknames))) {
		return 0;
	}
	in_create(10, sizeof(u_short), 0, &udpports);
	pr_addmacros(&udp_proto, udpmacros, lengthof(udpmacros));
	pr_addfunctions(&udp_proto, udpfunctions, lengthof(udpfunctions));
	return 1;
}

/* ARGSUSED */
static int
udp_setopt(int id, char *val)
{
	if ((enum udp_propt) id != UDP_PROPT_SETPORT)
		return 0;
	return ip_setport(val, udp_embed);
}

static void
udp_embed(Protocol *pr, long prototype, long qualifier)
{
	u_short port;

	port = prototype;
	if (pr->pr_id == PRID_SUNRPC)
		sunrpc = pr;
	in_enterqual(udpports, &port, pr, qualifier);
}

/* ARGSUSED */
static Expr *
udp_resolve(char *name, int len, struct snooper *sn)
{
	struct servent *sp;
	u_short port;
	Expr *ex;

	sp = getservbyname(name, udpname);
	if (sp)
		port = sp->s_port;
	else if (!sunrpc_getport(name, IPPROTO_UDP, &port))
		return 0;
	ex = expr(EXOP_NUMBER, EX_NULLARY, name);
	ex->ex_val = port;
	return ex;
}

static ExprType
udp_compile(ProtoField *pf, Expr *mex, Expr *tex, ProtoCompiler *pc)
{
	long mask;

	if (!pc_intmask(pc, mex, &mask))
		return ET_ERROR;
	if (tex->ex_op != EXOP_NUMBER) {
		pc_badop(pc, tex);
		return ET_ERROR;
	}
	if (!pc_intfield(pc, pf, mask, tex->ex_val, sizeof(struct udphdr)))
		return ET_COMPLEX;
	return ET_SIMPLE;
}

/*
 * Lookup port's protocol in udpports.
 */
static Protocol *
udp_findproto(u_short port, struct in_addr addr)
{
	Protocol *pr;

	pr = in_matchqual(udpports, &port, addr.s_addr);
	if (pr)
		return pr;
	if (sunrpc_ismapped(port, addr, IPPROTO_UDP))
		return sunrpc;
	return 0;
}

static int
udp_match(Expr *pex, DataStream *ds, ProtoStack *ps, Expr *rex)
{
	struct udphdr *uh;
	struct ipframe *ipf;
	Protocol *pr;

	uh = (struct udphdr *) ds_inline(ds, sizeof *uh, IP_HDRGRAIN);
	if (uh == 0)
		return 0;
	ipf = PS_TOP(ps, struct ipframe);
	ipf->ipf_proto = &udp_proto;
	ipf->ipf_sport = ntohs(uh->uh_sport);
	ipf->ipf_dport = ntohs(uh->uh_dport);
	if ((pr = udp_findproto(ipf->ipf_sport, ipf->ipf_src)) == 0
	    && (pr = udp_findproto(ipf->ipf_dport, ipf->ipf_rdst)) == 0
	    || pr != pex->ex_prsym->sym_proto) {
		return 0;
	}
	return ex_match(pex, ds, ps, rex);
}

/* ARGSUSED */
static int
udp_fetch(ProtoField *pf, DataStream *ds, ProtoStack *ps, Expr *rex)
{
	struct udphdr *uh;

	uh = (struct udphdr *) ds_inline(ds, sizeof *uh, IP_HDRGRAIN);
	if (uh) {
		switch (UDPFID(pf)) {
		  case SPORT:
			rex->ex_val = ntohs(uh->uh_sport);
			break;
		  case DPORT:
			rex->ex_val = ntohs(uh->uh_dport);
			break;
		  case LEN:
			rex->ex_val = ntohs((u_short) uh->uh_ulen);
			break;
		  case SUM:
			rex->ex_val = ntohs(uh->uh_sum);
			break;
		}
		rex->ex_op = EXOP_NUMBER;
		return 1;
	}
	return ds_field(ds, pf, sizeof *uh, rex);
}

static void
udp_decode(DataStream *ds, ProtoStack *ps, PacketView *pv)
{
	struct ipframe *ipf;
	struct udphdr *uh, hdr;
	u_short sum;
	Protocol *spr, *dpr;

	ipf = PS_TOP(ps, struct ipframe);
	uh = (struct udphdr *) ds_inline(ds, sizeof *uh, IP_HDRGRAIN);
	if (uh == 0) {
		sum = 0;
		uh = &hdr;
		ds_udphdr(ds, uh);
	} else {
		u_short len, udpsum;

		len = ntohs((u_short) uh->uh_ulen);
		udpsum = uh->uh_sum;
		uh->uh_sum = 0;
		if (udpsum == 0
		    || !ip_checksum_pseudohdr(ipf, (char *) uh,
					      sizeof *uh + ds->ds_count,
					      &sum)) {
			sum = 0;
		} else if (sum == 0) {
			sum = ~sum;
		}
		uh->uh_sum = ntohs(udpsum);
		uh->uh_sport = ntohs(uh->uh_sport);
		uh->uh_dport = ntohs(uh->uh_dport);
		uh->uh_ulen = len;
	}

	spr = udp_findproto(uh->uh_sport, ipf->ipf_src);
	dpr = udp_findproto(uh->uh_dport, ipf->ipf_rdst);

	pv_showfield(pv, &UDPFIELD(SPORT), &uh->uh_sport,
		     "%-22.22s", ip_service(uh->uh_sport, udpname, spr));
	pv_showfield(pv, &UDPFIELD(DPORT), &uh->uh_dport,
		     "%-22.22s", ip_service(uh->uh_dport, udpname, dpr));
	pv_showfield(pv, &UDPFIELD(LEN), &uh->uh_ulen,
		     "%-5u", (u_short) uh->uh_ulen);
	if (sum != 0 && uh->uh_sum != sum) {
		pv_showfield(pv, &UDPFIELD(SUM), &uh->uh_sum,
			     "%#x [%#x]", uh->uh_sum, sum);
	} else {
		pv_showfield(pv, &UDPFIELD(SUM), &uh->uh_sum,
			     "%#-6x", uh->uh_sum);
	}

	ipf->ipf_proto = &udp_proto;
	ipf->ipf_sport = uh->uh_sport;
	ipf->ipf_dport = uh->uh_dport;
	pv_decodeframe(pv, spr ? spr : dpr, ds, ps);
}
