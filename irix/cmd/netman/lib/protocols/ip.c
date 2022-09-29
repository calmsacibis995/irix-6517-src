/*
 * Copyright 1988 Silicon Graphics, Inc.  All rights reserved.
 *
 * Internet Protocol (IP), defined in RFCs 791 and 760.
 */
#include <bstring.h>
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#ifdef __sgi
#include <sys/cfeiioctl.h>
#endif
#include "cache.h"
#include "debug.h"
#include "enum.h"
#include "heap.h"
#include "protodefs.h"
#include "snooper.h"
#include "protocols/ether.h"
#include "protocols/ip.h"

char ipname[] = "ip";

/*
 * IP field identifiers and descriptors.
 */
enum ipfid {
	V, HL, TOS, LEN, ID, OFF, TTL, P, SUM,
	SRC=IPFID_SRC, DST=IPFID_DST,
	OPT, OPTLEN,
	    RTOFF, RTHOP,
	    TSPTR, TSOFLW, TSFLG, TSTIME, TSADDR
};
#define	IPF_ISOPTION(pf)	((pf)->pf_id >= (int) OPT)

#define	PFI_OINT(name, title, id, type, off, level) \
	PFI(name, title, id, DS_ZERO_EXTEND, sizeof(type), off, EXOP_NUMBER, \
	    level)
#define	PFI_OBIT(name, title, id, bits, off, level) \
	PFI(name, title, id, DS_ZERO_EXTEND, -(bits), off, EXOP_NUMBER, level)

static ProtoField ipfields[] = {
	PFI_UBIT("v",	"Version",		 V,    4,	 PV_VERBOSE),
	PFI_UBIT("hl",	"Header Length",	 HL,   4,	 PV_VERBOSE),
	PFI_UINT("tos",	"Type of Service",	 TOS,  u_char,   PV_VERBOSE),
	PFI_UINT("len",	"Total Length",		 LEN,  u_short,  PV_DEFAULT),
	PFI_UINT("id",	"Identification",	 ID,   u_short,  PV_DEFAULT),
	PFI_UINT("off",	"Fragment Offset",	 OFF,  u_short,  PV_DEFAULT),
	PFI_UINT("ttl",	"Time to Live",		 TTL,  u_char,   PV_VERBOSE),
	PFI_UINT("p",	"Protocol",		 P,    u_char,   PV_VERBOSE),
	PFI_UINT("sum",	"Header Checksum",	 SUM,  u_short,  PV_VERBOSE),
	PFI_UINT("src",	"Source Address",	 SRC,  u_long,   PV_TERSE),
	PFI_UINT("dst",	"Destination Address",   DST,  u_long,   PV_TERSE),

	PFI_OINT("opt",	   "Option Type",	 OPT,    u_char, 0, PV_DEFAULT),
	PFI_OINT("optlen", "Option Length",	 OPTLEN, u_char, 1, PV_VERBOSE),
	PFI_OINT("rtoff",  "Route Offset",	 RTOFF,	 u_char, 2, PV_VERBOSE),
	PFI_OINT("rthop",  "Route Address",	 RTHOP,	 u_long, 0, PV_TERSE),
	PFI_OINT("tsptr",  "Timestamp Pointer",  TSPTR,	 u_char, 2, PV_VERBOSE),
	PFI_OBIT("tsoflw", "Timestamp Overflow", TSOFLW, 4,	 24,PV_VERBOSE),
	PFI_OBIT("tsflg",  "Timestamp Flag",	 TSFLG,	 4,	 28,PV_VERBOSE),
	PFI_OINT("tstime", "Timestamp Time",	 TSTIME, n_long, 0, PV_DEFAULT),
	PFI_OINT("tsaddr", "Timestamp Address",	 TSADDR, u_long, 0, PV_TERSE),
};

#define	IPFID(pf)	((enum ipfid) (pf)->pf_id)
#define	IPFIELD(fid)	ipfields[(int) fid]

/*
 * Nicknames for IP-based protocols.
 */
static ProtoMacro ipnicknames[] = {
	PMI("IP",	ipname),
	PMI("bootp",	"ip.udp.bootp"),
	PMI("BOOTP",	"ip.udp.bootp"),
	PMI("hello",	"ip.hello"),
	PMI("HELLO",	"ip.hello"),
	PMI("icmp",	"ip.icmp"),
	PMI("ICMP",	"ip.icmp"),
	PMI("igmp",	"ip.igmp"),
	PMI("IGMP",	"ip.igmp"),
	PMI("rip",	"ip.udp.rip"),
	PMI("RIP",	"ip.udp.rip"),
	PMI("tcp",	"ip.tcp"),
	PMI("TCP",	"ip.tcp"),
	PMI("udp",	"ip.udp"),
	PMI("UDP",	"ip.udp"),
	PMI("dns",	"ip.udp.dns"),
	PMI("DNS",	"ip.udp.dns"),
	PMI("sunrpc",	"ip.udp.sunrpc"),
	PMI("SUNRPC",	"ip.udp.sunrpc"),
	PMI("tftp",	"ip.udp.tftp"),
	PMI("TFTP",	"ip.udp.tftp"),
	PMI("nfs",	"ip.udp.sunrpc.nfs"),
	PMI("NFS",	"ip.udp.sunrpc.nfs"),
	PMI("snmp",	"ip.udp.snmp"),
	PMI("SNMP",	"ip.udp.snmp"),
	PMI("ftp",	"ip.tcp.ftp"),
	PMI("FTP",	"ip.tcp.ftp"),
	PMI("telnet",	"ip.tcp.telnet"),
	PMI("TELNET",	"ip.tcp.telnet"),
	PMI("smtp",	"ip.tcp.smtp"),
	PMI("SMTP",	"ip.tcp.smtp"),
	PMI("netbios",	"ip.tcp.netbios"),
	PMI("NETBIOS",	"ip.tcp.netbios"),
	PMI("tsp",	"ip.udp.tsp"),
	PMI("TSP",	"ip.udp.tsp"),
	PMI("timed",	"ip.udp.tsp"),
	PMI("TIMED",	"ip.udp.tsp"),
	PMI("x11",	"ip.tcp.x11"),
	PMI("X11",	"ip.tcp.x11"),
	PMI("rlogin",	"ip.tcp.rlogin"),
	PMI("RLOGIN",	"ip.tcp.rlogin"),
	PMI("rcp",	"ip.tcp.rcp"),
	PMI("RCP",	"ip.tcp.rcp"),
};


/*
 * Enumerated IP option type.
 */
static Enumeration ipopt;
static Enumerator ipoptvec[] = {
	EI_VAL("EOL",		IPOPT_EOL),
	EI_VAL("NOP",		IPOPT_NOP),
	EI_VAL("RR",		IPOPT_RR),
	EI_VAL("TS",		IPOPT_TS),
	EI_VAL("SECURITY",	IPOPT_SECURITY),
	EI_VAL("LSRR",		IPOPT_LSRR),
	EI_VAL("SATID",		IPOPT_SATID),
	EI_VAL("SSRR",		IPOPT_SSRR),
};

/*
 * IP constants and macros.
 */
static Enumerator ipconsts[] = {
	EI_VAL("MINHL",		IP_MINHDRLEN),
	EI_VAL("DF",		IP_DF),
	EI_VAL("MF",		IP_MF),
	EI_VAL("MSS",		IP_MSS),
	EI_VAL("MAXPRIVPORT",	IPPORT_RESERVED-1),
	EI_VAL("BROADCAST",	INADDR_BROADCAST),
};

static ProtoMacro ipmacros[] = {
	PMI("between",	"src == $1 && dst == $2 || dst == $1 && src == $2"),
	PMI("host",	"src == $1 || dst == $1"),
};

/*
 * IP protocol options.
 */
static ProtOptDesc ipoptdesc[] = {
	POD("etherupdate",  IP_PROPT_ETHERUPDATE,
			    "Update Ethernet hostname/address cache"),
	POD("hostbyname",   IP_PROPT_HOSTBYNAME,
			    "Decode IP addresses into hostnames"),
	POD("hostresorder", IP_PROPT_HOSTRESORDER,
			    "Hostname resolution order; see resolver(4)"),
};

/*
 * IP protocol interface.
 */
DefineProtocol(ip, ipname, "Internet Protocol", PRID_IP,
	       DS_BIG_ENDIAN, IP_MAXHDRLEN, &IPFIELD(P),
	       0, ipoptdesc, lengthof(ipoptdesc), 0);

/*
 * IP fragment matching and decoding caches.
 */
#define	MAXFRAGS	16	/* maximum fragment cache size */

static Cache *ipfragmatches;
static Cache *ipfragdecodes;

static unsigned int
ip_fraghash(struct ipfragkey *ipk)
{
	return ipk->ipk_id ^ ipk->ipk_src.s_addr;
}

static int
ip_fragcmp(struct ipfragkey *ipk1, struct ipfragkey *ipk2)
{
	return ipk1->ipk_id != ipk2->ipk_id
	    || ipk1->ipk_prototype != ipk2->ipk_prototype
	    || ipk1->ipk_src.s_addr != ipk2->ipk_src.s_addr
	    || ipk1->ipk_dst.s_addr != ipk2->ipk_dst.s_addr;
}

static void
ip_fragmatchdump(struct ipfragkey *ipk, Expr *ex)
{
	printf("(%u,%u,%s,%s) -> ",
		ipk->ipk_id, ipk->ipk_prototype,
		ip_hostname(ipk->ipk_src, IP_HOST),
		ip_hostname(ipk->ipk_dst, IP_HOST));
	switch (ex->ex_op) {
	  case EXOP_NUMBER:
		printf("%ld\n", ex->ex_val);
		break;
	  case EXOP_ADDRESS:
		printf("%x:%x:%x:%x:%x:%x:%x:%x\n",
			ex->ex_addr.a_vec[0], ex->ex_addr.a_vec[1],
			ex->ex_addr.a_vec[2], ex->ex_addr.a_vec[3],
			ex->ex_addr.a_vec[4], ex->ex_addr.a_vec[5],
			ex->ex_addr.a_vec[6], ex->ex_addr.a_vec[7]);
		break;
	  case EXOP_STRING:
		printf("\"%s\" (%d)\n",
			ex->ex_str.s_ptr, ex->ex_str.s_len);
	}
}

static void
ip_fragdecodedump(struct ipfragkey *ipk, ProtoDecodeState *pds)
{
	printf("(%u,%u,%s,%s) -> (%u,%s)\n",
		ipk->ipk_id, ipk->ipk_prototype,
		ip_hostname(ipk->ipk_src, IP_HOST),
		ip_hostname(ipk->ipk_dst, IP_HOST),
		pds->pds_mysize, pds->pds_proto->pr_name);
}

static struct cacheops ipfragmatchops =
    { { ip_fraghash, ip_fragcmp, xdr_matchresult }, 0, ip_fragmatchdump };
static struct cacheops ipfragdecodeops =
    { { ip_fraghash, ip_fragcmp, xdr_decodestate }, 0, ip_fragdecodedump };

/*
 * IP protocol functions.
 */
/* ARGSUSED */
int
ip_func_badsum(Expr *argv, DataStream *ds, ProtoStack *ps, Expr *rex)
{
	struct ip *ip;
	int hdrlen, optlen;
	u_short realsum, idealsum;

	ip = (struct ip *) ds_inline(ds, sizeof *ip, IP_HDRGRAIN);
	if (ip == 0)
		return 0;
	hdrlen = IP_HDRLEN(ip->ip_hl);
	optlen = hdrlen - IP_MINHDRLEN;
	if (optlen > ds->ds_count)
		return 0;
	realsum = ip->ip_sum;
	ip->ip_sum = 0;
	(void) ip_checksum((char *) ip, hdrlen, &idealsum);
	ip->ip_sum = realsum;
	rex->ex_op = EXOP_NUMBER;
	rex->ex_val = (ntohs(realsum) != idealsum);
	return 1;
}

static ProtoFuncDesc ipfunctions[] = {
	PFD("badsum", ip_func_badsum, 0,
	    "Match packet if header checksum is incorrect"),
};

/*
 * IP protocol operations.
 */
static Index *ipprotos;

static int
ip_init()
{
	/*
	 * Register IP as a known protocol.
	 */
	if (!pr_register(&ip_proto, ipfields, lengthof(ipfields),
			 lengthof(ipoptvec) + lengthof(ipconsts)
			 + lengthof(ipmacros) + lengthof(ipfunctions)
			 + 12)) {
		return 0;
	}

	/*
	 * Nest IP in loopback, Ethernet, LLC, etc.
	 */
	(void) pr_nest(&ip_proto, PRID_LOOP, AF_INET, ipnicknames,
		       lengthof(ipnicknames));
	(void) pr_nest(&ip_proto, PRID_ETHER, ETHERTYPE_IP, ipnicknames,
		       lengthof(ipnicknames));
	(void) pr_nest(&ip_proto, PRID_LLC, ETHERTYPE_IP, ipnicknames,
		       lengthof(ipnicknames));
#ifdef __sgi
	(void) pr_nest(&ip_proto, PRID_CRAYIO, CY_IPLINK, ipnicknames,
		       lengthof(ipnicknames));
#endif

	/*
	 * Always define symbols, in case some were redefined as macros.
	 */
	en_init(&ipopt, ipoptvec, lengthof(ipoptvec), &ip_proto);
	pr_addnumbers(&ip_proto, ipconsts, lengthof(ipconsts));
	pr_addmacros(&ip_proto, ipmacros, lengthof(ipmacros));
	pr_addfunctions(&ip_proto, ipfunctions, lengthof(ipfunctions));

	/*
	 * Initialize or reinitialize the fragment and protocol hash tables.
	 */
	c_create("ip.fragmatches", MAXFRAGS, sizeof(struct ipfragkey), MAXTTL,
		 &ipfragmatchops, &ipfragmatches);
	c_create("ip.fragdecodes", MAXFRAGS, sizeof(struct ipfragkey), MAXTTL,
		 &ipfragdecodeops, &ipfragdecodes);
	in_create(3, sizeof(u_char), 0, &ipprotos);

	/*
	 * Keep /etc/networks and /etc/protocols open for speedier queries.
	 * Note: ip_host.c and ip_service.c make equivalent calls for their
	 * respective /etc files.
	 */
	setnetent(1);
	setprotoent(1);
	return 1;
}

static int	etherupdate;
extern int	_iphostbyname;

static int
ip_setopt(int id, char *val)
{
	switch ((enum ip_propt) id) {
	  case IP_PROPT_ETHERUPDATE:
		etherupdate = (*val != '0');
		break;
	  case IP_PROPT_HOSTBYNAME:
		_iphostbyname = (*val != '0');
		break;
	  case IP_PROPT_HOSTRESORDER:
#ifdef __sgi
		sethostresorder(val);
#endif
		break;
	   default:
		return 0;
	}
	return 1;
}

/* ARGSUSED */
static void
ip_embed(Protocol *pr, long prototype, long qualifier)
{
	u_char p;

	p = prototype;
	in_enter(ipprotos, &p, pr);
}

Expr *
ip_resolve(char *name, int len, Snooper *sn)
{
	struct in_addr addr;
	u_long val;
	Expr *ex;

	if (ip_hostaddr(name, IP_HOST, &addr))
		val = addr.s_addr;
	else {
		int cmd;
		struct sockaddr_in sin;

		if (len == 9 && !strcmp(name, "broadcast"))
			cmd = SIOCGIFBRDADDR;
		else if (len == 7 && !strcmp(name, "netmask"))
			cmd = SIOCGIFNETMASK;
		else {
			struct netent *np;

			np = getnetbyname(name);
			if (np == 0 || np->n_addrtype != AF_INET)
				return 0;
			cmd = 0;
			val = np->n_net;
		}
		if (cmd) {
			sin.sin_family = AF_INET;
			if (!sn_getaddr(sn, cmd, (struct sockaddr *) &sin))
				return 0;
			val = ntohl(sin.sin_addr.s_addr);
		}
	}

	ex = expr(EXOP_NUMBER, EX_NULLARY, name);
	ex->ex_val = val;
	return ex;
}

static ExprType
ip_compile(ProtoField *pf, Expr *mex, Expr *tex, ProtoCompiler *pc)
{
	long mask, match;

	if (IPF_ISOPTION(pf))
		return ET_COMPLEX;
	if (!pc_intmask(pc, mex, &mask))
		return ET_ERROR;
	switch (tex->ex_op) {
	  case EXOP_PROTOCOL:
		if (IPFID(pf) != P) {
			pc_badop(pc, tex);
			return ET_ERROR;
		}
		match = tex->ex_prsym->sym_prototype;
		break;
	  case EXOP_NUMBER:
		match = tex->ex_val;
		break;
	  default:
		pc_badop(pc, tex);
		return ET_ERROR;
	}
	if (!pc_intfield(pc, pf, mask, match, IP_MINHDRLEN))
		return ET_COMPLEX;
	pc_stop(pc);
	return ET_SIMPLE;
}

/*
 * Fix ds so that we don't fetch or decode Ethernet minimum packet padding.
 */
static void
ip_trim_ether_padding(int iplen, DataStream *ds)
{
	int ipsize, padding;

	if (iplen < IP_MINHDRLEN)
		return;
	ipsize = ds->ds_size - sizeof(struct etherhdr);
	padding = MIN(ipsize, ETHERMIN) - iplen;
	if (padding > 0) {
		ds->ds_count -= padding;
		ds->ds_size -= padding;
	}
}

static int
ip_match(Expr *pex, DataStream *ds, ProtoStack *ps, Expr *rex)
{
	struct ip *ip;
	int hdrlen, optlen, matched;
	struct ipframe ipf;
	Entry **ep, *ent;
	Expr *mex;

	ip = (struct ip *) ds_inline(ds, sizeof *ip, IP_HDRGRAIN);
	if (ip == 0 || ip->ip_p != pex->ex_prsym->sym_prototype)
		return 0;

	/*
	 * Trim minimum data link packet padding.
	 */
	if (ps->ps_top && ps->ps_top->psf_proto->pr_id == PRID_ETHER)
		ip_trim_ether_padding((u_short)ip->ip_len, ds);

	/*
	 * Initialize an IP protocol stack frame.
	 */
	ipf.ipf_prototype = ip->ip_p;
	hdrlen = IP_HDRLEN(ip->ip_hl);
	ipf.ipf_hdrlen = hdrlen;
	ipf.ipf_len = ntohs((u_short)ip->ip_len);
	ipf.ipf_fragoffset = (ntohs(ip->ip_off) & ~(IP_DF|IP_MF)) << 3;
	ipf.ipf_morefrags = (ntohs(ip->ip_off) & IP_MF) != 0;
	ipf.ipf_src.s_addr = ntohl(ip->ip_src.s_addr);
	ipf.ipf_dst.s_addr = ipf.ipf_rdst.s_addr = ntohl(ip->ip_dst.s_addr);

	/*
	 * Skip over any IP options, but save the last hop in a source route
	 * in ipf.ipf_rdst.
	 */
	optlen = hdrlen - IP_MINHDRLEN;
	if (optlen > 0) {
		struct ip_route *ipr;
		char optbuf[IP_MAXOPTLEN];

		ipr = (struct ip_route *) optbuf;
		while (optlen > 0) {
			if (!ds_ip_opt(ds, &ipr->ipr_hdr) && ipr->ipr_len == 0)
				break;
			if (ipr->ipr_opt == IPOPT_EOL)
				break;
			if ((ipr->ipr_opt == IPOPT_LSRR
			    || ipr->ipr_opt == IPOPT_SSRR)
			    && ipr->ipr_off < ipr->ipr_len) {
				ipf.ipf_rdst = ipr->ipr_addr[ipr->ipr_cnt-1];
				break;
			}
			optlen -= ipr->ipr_len;
		}
	}

	/*
	 * If this packet is a fragment, check whether we already matched
	 * the first fragment of an IP datagram.  If we matched, the cache
	 * holds a copy of the result expression.
	 */
	if (ipf.ipf_fragoffset != 0) {
		mex = c_match(ipfragmatches, &ipf.ipf_fragkey);
		if (mex)
			*rex = *mex;
		return mex != 0;
	}

	/*
	 * Evaluate the rest of the path expression.  If there are more
	 * fragments, remember whether this one matched or not, and if so,
	 * save the result expression.
	 */
	PS_PUSH(ps, &ipf.ipf_frame, &ip_proto);
	matched = ex_match(pex, ds, ps, rex);
	PS_POP(ps);
	if (ipf.ipf_morefrags) {
		if (matched)
			mex = savematchresult(rex);
		else
			mex = 0;
		c_enter(ipfragmatches, &ipf.ipf_fragkey, mex);
	}
	return matched;
}

int	ip_fetchopt(ProtoField *, int, DataStream *, Expr *);

static int
ip_fetch(ProtoField *pf, DataStream *ds, ProtoStack *ps, Expr *rex)
{
	struct ip *ip;
	int skip;

	ip = (struct ip *) ds_inline(ds, sizeof *ip, IP_HDRGRAIN);
	if (ip) {
		skip = ip->ip_hl;
		switch (IPFID(pf)) {
		  case V:
			rex->ex_val = ip->ip_v;
			break;
		  case HL:
			rex->ex_val = skip;
			break;
		  case TOS:
			rex->ex_val = ip->ip_tos;
			break;
		  case LEN:
			rex->ex_val = ntohs((u_short)ip->ip_len);
			break;
		  case ID:
			rex->ex_val = ntohs(ip->ip_id);
			break;
		  case OFF:
			rex->ex_val = ntohs(ip->ip_off);
			break;
		  case TTL:
			rex->ex_val = ip->ip_ttl;
			break;
		  case P:
			rex->ex_val = ip->ip_p;
			break;
		  case SUM:
			rex->ex_val = ntohs(ip->ip_sum);
			break;
		  case SRC:
			rex->ex_val = ntohl(ip->ip_src.s_addr);
			break;
		  case DST:
			rex->ex_val = ntohl(ip->ip_dst.s_addr);
			break;
		}
		rex->ex_op = EXOP_NUMBER;
	} else {
		if (!IPF_ISOPTION(pf)
		    && !ds_field(ds, pf, IP_MINHDRLEN, rex)) {
			return 0;
		}
		return 1;
	}

	/*
	 * Trim data link padding so our caller doesn't try to decode it.
	 */
	if (ps->ps_top && ps->ps_top->psf_proto->pr_id == PRID_ETHER)
		ip_trim_ether_padding((u_short)ip->ip_len, ds);

	skip = IP_HDRLEN(skip) - IP_MINHDRLEN;
	if (skip > 0) {
		if (IPF_ISOPTION(pf))
			return ip_fetchopt(pf, skip, ds, rex);
		(void) ds_seek(ds, skip, DS_RELATIVE);
	}
	return 1;
}

static int
ip_fetchopt(ProtoField *pf, int limit, DataStream *ds, Expr *rex)
{
	char optbuf[IP_MAXOPTLEN];
	struct ip_opt *ipo;

	limit += DS_TELL(ds);
	ipo = (struct ip_opt *) optbuf;
	if (!ds_ip_opt(ds, ipo))
		return 0;

	switch (IPFID(pf)) {
	  case OPT:
		rex->ex_val = ipo->ipo_opt;
		break;
	  case OPTLEN:
		rex->ex_val = ipo->ipo_len;
		break;

	  case RTOFF:
	  case RTHOP: {
		struct ip_route *ipr;

		if (ipo->ipo_opt != IPOPT_RR
		    && ipo->ipo_opt != IPOPT_LSRR
		    && ipo->ipo_opt != IPOPT_SSRR) {
			return 0;
		}
		ipr = (struct ip_route *) ipo;
		if (ipr->ipr_off <= IPOPT_MINOFF || limit < ipr->ipr_off)
			return 0;
		if (IPFID(pf) == RTOFF)
			rex->ex_val = ipr->ipr_off;
		else {
			int index;	/* index of route addr to fetch */

			index = (ipr->ipr_off - IPOPT_MINOFF)
				/ sizeof ipr->ipr_addr[0];
			if (ipo->ipo_opt == IPOPT_RR)
				--index;
			rex->ex_val = ntohl(ipr->ipr_addr[index].s_addr);
		}
		break;
	  }

	  default: {
		struct ip_timestamp *ipt;
#define	lastset(ipt,vecname) \
	ipt->ipt_timestamp.vecname[(ipt->ipt_ptr - (IPOPT_MINOFF+1) \
				    - sizeof ipt->ipt_timestamp.vecname[0]) \
				   / sizeof ipt->ipt_timestamp.vecname[0]]

		if (ipo->ipo_opt != IPOPT_TS)
			return 0;
		ipt = (struct ip_timestamp *) ipo;
		switch (IPFID(pf)) {
		  case TSPTR:
			rex->ex_val = ipt->ipt_ptr;
			break;
		  case TSOFLW:
			rex->ex_val = ipt->ipt_oflw;
			break;
		  case TSFLG:
			rex->ex_val = ipt->ipt_flg;
			break;
		  case TSTIME:
		  case TSADDR:
			if (ipt->ipt_ptr <= IPOPT_MINOFF + 1
			    || limit < ipt->ipt_ptr) {
				return 0;
			}
			switch (ipt->ipt_flg) {
			  case IPOPT_TS_TSONLY:
				if (IPFID(pf) != TSTIME)
					return 0;
				rex->ex_val = lastset(ipt, ipt_time);
				break;
			  case IPOPT_TS_TSANDADDR:
			  case IPOPT_TS_PRESPEC: {
				struct ipt_ta *ta;

				ta = &lastset(ipt, ipt_ta);
				rex->ex_val = (IPFID(pf) == TSADDR) ?
					ta->ipt_addr.s_addr : ta->ipt_time;
				break;
			  }
			  default:
				return 0;
			}
			rex->ex_val = ntohl(rex->ex_val);
			break;
		}
		rex->ex_op = EXOP_NUMBER;
#undef lastset
	  }
	}
	(void) ds_seek(ds, limit, DS_ABSOLUTE);
	return 1;
}

/*
 * For IP over Ethernet, update ether hostname info based on ip.
 */
static void
ip_update_ether_hosts(struct ip *ip, ProtoStack *ps, char *srcname,
		      char *dstname)
{
	struct sockaddr *sa;
	int localnet;
	struct etherframe *ef;

	sa = &ps->ps_snoop->sn_ifaddr;
	if (sa->sa_family != AF_INET) {
		sa->sa_family = AF_INET;
		if (!sn_getaddr(ps->ps_snoop, SIOCGIFADDR, sa))
			bzero(sa, sizeof *sa);
	}
	localnet = inet_netof(((struct sockaddr_in *)sa)->sin_addr);
	ef = PS_TOP(ps, struct etherframe);
	if (inet_netof(ip->ip_src.s_addr) == localnet)
		(void) ether_addhost(srcname, &ef->ef_src);
	if (inet_netof(ip->ip_dst.s_addr) == localnet
	    && bcmp(&ef->ef_dst, &etherbroadcastaddr, sizeof ef->ef_dst)) {
		(void) ether_addhost(dstname, &ef->ef_dst);
	}
}

void	ip_decodeopt(struct ip_opt *, struct ipframe *, PacketView *);
void	ip_decodeopts(int, DataStream *, struct ipframe *, PacketView *);

static void
ip_decode(DataStream *ds, ProtoStack *ps, PacketView *pv)
{
	struct ip *ip, hdr;
	u_long idealsum, src, dst;
	int hdrlen, optlen;
	Protocol *pr;
	char *srcname, *dstname;
	struct ipframe ipf;
	Entry **ep, *ent;
	ProtoDecodeState *pds;

	ip = (struct ip *) ds_inline(ds, sizeof *ip, IP_HDRGRAIN);
	if (ip == 0) {
		idealsum = 0;
		ip = &hdr;
		ds_ip(ds, ip);
		/*
		 * Keep src and dst in net order for convenience.
		 */
		ip->ip_src.s_addr = htonl(ip->ip_src.s_addr);
		ip->ip_dst.s_addr = htonl(ip->ip_dst.s_addr);
	} else {
		u_short realsum;

		realsum = ip->ip_sum;
		hdrlen = IP_HDRLEN(ip->ip_hl);
		optlen = hdrlen - IP_MINHDRLEN;
		if (optlen > ds->ds_count)
			idealsum = 0;
		else {
			u_short *ips;

			/*
			 * Optimized in-line expansion of ip_checksum.
			 */
			ip->ip_sum = 0;
			ips = (u_short *) ip;
			idealsum = ips[0] + ips[1] + ips[2] + ips[3] + ips[4]
				 + ips[6] + ips[7] + ips[8] + ips[9];
			if (optlen > 0) {
				optlen /= sizeof *ips;
				ips += sizeof *ip / sizeof *ips;
				while (--optlen >= 0)
					idealsum += *ips++;
			}
			idealsum = IP_FOLD_CHECKSUM_CARRY(idealsum);
		}
		ip->ip_len = ntohs((u_short)ip->ip_len);
		ip->ip_id = ntohs(ip->ip_id);
		ip->ip_off = ntohs(ip->ip_off);
		ip->ip_sum = ntohs(realsum);
	}

	pv_showfield(pv, &IPFIELD(V), &ip, "%-2d", ip->ip_v);
	pv_showfield(pv, &IPFIELD(HL), &ip, "%-2d", ip->ip_hl);
	pv_showfield(pv, &IPFIELD(TOS), &ip->ip_tos, "%#-4x", ip->ip_tos);
	pv_showfield(pv, &IPFIELD(LEN), &ip->ip_len,
		     "%-5u", (u_short) ip->ip_len);
	pv_showfield(pv, &IPFIELD(ID), &ip->ip_id, "%-5u", ip->ip_id);

	pv_showfield(pv, &IPFIELD(OFF), &ip->ip_off,
		     "%d%s%s", ip->ip_off & ~(IP_DF|IP_MF),
		     (ip->ip_off & IP_DF) ? " DF" : "",
		     (ip->ip_off & IP_MF) ? " MF" : "");
	if (pv->pv_level >= PV_VERBOSE)
		pv_break(pv);

	pv_showfield(pv, &IPFIELD(TTL), &ip->ip_ttl,
		     "%-3d", ip->ip_ttl);
	pr = in_match(ipprotos, &ip->ip_p);
	if (pr) {
		pv_showfield(pv, &IPFIELD(P), &ip->ip_p, "%-6s", pr->pr_name);
	} else {
		pv_showfield(pv, &IPFIELD(P), &ip->ip_p,
			     "@t%-6s", ip_protoname(ip->ip_p));
	}

	if (idealsum && ip->ip_sum != idealsum) {
		pv_showfield(pv, &IPFIELD(SUM), &ip->ip_sum,
			     "%#x [!= %#x]", ip->ip_sum, idealsum);
	} else {
		pv_showfield(pv, &IPFIELD(SUM), &ip->ip_sum,
			     "%#-6x", ip->ip_sum);
	}
	pv_break(pv);

	src = ntohl(ip->ip_src.s_addr);
	srcname = ip_hostname(ip->ip_src, IP_NET);
	pv_showfield(pv, &IPFIELD(SRC), &src, "%-25.25s", srcname);
	dst = ntohl(ip->ip_dst.s_addr);
	dstname = ip_hostname(ip->ip_dst, IP_NET);
	pv_showfield(pv, &IPFIELD(DST), &dst, "%-25.25s", dstname);

	/*
	 * If this packet is local IP traffic over Ethernet, update the
	 * Ethernet host name/address mappings with IP hostnames.  Also
	 * trim Ethernet padding.
	 */
	if (ps->ps_top) {
		int linkprid;

		linkprid = ps->ps_top->psf_proto->pr_id;
		if (linkprid == PRID_ETHER) {
			if (etherupdate)
				ip_update_ether_hosts(ip, ps, srcname, dstname);
			ip_trim_ether_padding((u_short)ip->ip_len, ds);
		}
	}

	/*
	 * Initialize an ipframe for higher layers.
	 */
	ipf.ipf_prototype = ip->ip_p;
	ipf.ipf_hdrlen = hdrlen;
	ipf.ipf_len = (u_short)ip->ip_len;
	ipf.ipf_id = ip->ip_id;
	ipf.ipf_fragoffset = (ip->ip_off & ~(IP_DF|IP_MF)) << 3;
	ipf.ipf_morefrags = (ip->ip_off & IP_MF) != 0;
	ipf.ipf_src.s_addr = src;
	ipf.ipf_dst.s_addr = ipf.ipf_rdst.s_addr = dst;

	/*
	 * Decode IP options, if any.
	 */
	hdrlen = IP_HDRLEN(ip->ip_hl);
	optlen = hdrlen - IP_MINHDRLEN;
	if (optlen > 0)
		ip_decodeopts(optlen, ds, &ipf, pv);

	/*
	 * If this packet contains a non-initial IP fragment, try to find
	 * the IP datagram's protocol decode state.  Higher layer protocols
	 * will set ps->ps_state to point at dynamically allocated decode
	 * state when they see an initial fragment.
	 */
	if (ipf.ipf_fragoffset != 0) {
		pds = c_match(ipfragdecodes, &ipf.ipf_fragkey);
		if (pds) {
			ps->ps_state = pds;
			pr = pds->pds_proto;
		} else
			pr = 0;
		pv_decodeframe(pv, pr, ds, ps);
		return;
	}

	/*
	 * Decode the first or only fragment of an IP datagram.  If a higher
	 * layer set some protocol decode state and there are more fragments,
	 * associate the state with ipf_fragkey.
	 */
	PS_PUSH(ps, &ipf.ipf_frame, &ip_proto);
	pv_decodeframe(pv, pr, ds, ps);
	PS_POP(ps);
	pds = ps->ps_state;
	if (pds && ipf.ipf_morefrags)
		c_enter(ipfragdecodes, &ipf.ipf_fragkey, pds);
}

static void
ip_decodeopts(int optlen, DataStream *ds, struct ipframe *ipf, PacketView *pv)
{
	struct ip_opt *ipo;
	char optbuf[IP_MAXOPTLEN];
	int off, len;

	ipo = (struct ip_opt *) optbuf;
	while (optlen > 0) {
		if (!ds_ip_opt(ds, ipo) && ipo->ipo_len == 0)
			break;
		optlen -= ipo->ipo_len;
		off = pv->pv_off + ipo->ipo_len;
		ip_decodeopt(ipo, ipf, pv);
		len = off - pv->pv_off;
		if (len > 0) {
			pv_decodehex(pv, ds, pv->pv_off, len);
			pv->pv_off = off;
		}
	}
}

static void
ip_decodeopt(struct ip_opt *ipo, struct ipframe *ipf, PacketView *pv)
{
	int len;

	pv_break(pv);
	pv_showfield(pv, &IPFIELD(OPT), &ipo->ipo_opt,
		     "%-11s", en_name(&ipopt, ipo->ipo_opt));

	switch (ipo->ipo_opt) {
	  case IPOPT_EOL:
	  case IPOPT_NOP:
		break;

	  case IPOPT_RR:
	  case IPOPT_LSRR:
	  case IPOPT_SSRR: {
		struct ip_route *ipr;
		struct in_addr *hop;

		ipr = (struct ip_route *) ipo;
		pv_showfield(pv, &IPFIELD(OPTLEN), &ipr->ipr_len,
			     "%-3u", ipr->ipr_len);
		pv_showfield(pv, &IPFIELD(RTOFF), &ipr->ipr_off,
			     "%-3u", ipr->ipr_off);
		pv_break(pv);
		len = ipr->ipr_cnt;
		if (ipr->ipr_opt != IPOPT_RR && ipr->ipr_off < ipr->ipr_len)
			ipf->ipf_rdst = ipr->ipr_addr[len-1];
		for (hop = ipr->ipr_addr; --len >= 0; hop++) {
			pv_showfield(pv, &IPFIELD(RTHOP), hop,
				     "%-25.25s", ip_hostname(*hop, IP_HOST));
		}
		break;
	  }

	  case IPOPT_TS: {
		struct ip_timestamp *ipt;

		ipt = (struct ip_timestamp *) ipo;
		pv_showfield(pv, &IPFIELD(OPTLEN), &ipt->ipt_len,
			     "%-3u", ipt->ipt_len);
		pv_showfield(pv, &IPFIELD(TSPTR), &ipt->ipt_ptr,
			     "%-3u", ipt->ipt_ptr);
		pv_showfield(pv, &IPFIELD(TSOFLW), &ipt->ipt_ptr + 1,
			     "%u", ipt->ipt_oflw);
		pv_showfield(pv, &IPFIELD(TSFLG), &ipt->ipt_ptr + 1,
			     "%u", ipt->ipt_flg);
		len = ipt->ipt_ptr;
		if (len > IP_MAXOPTLEN)
			len = IP_MAXOPTLEN;
		len -= IPOPT_MINOFF + 1;
		pv_break(pv);
		switch (ipt->ipt_flg) {
		  case IPOPT_TS_TSONLY: {
			n_long *np;

			len /= sizeof *np;
			for (np = ipt->ipt_timestamp.ipt_time; --len >= 0;
			     np++) {
				pv_showfield(pv, &IPFIELD(TSTIME), np,
					     "%-12.12s", ip_timestamp(*np));
			}
			break;
		  }
		  case IPOPT_TS_TSANDADDR:
		  case IPOPT_TS_PRESPEC: {
			struct ipt_ta *ta;

			len /= sizeof *ta;
			for (ta = ipt->ipt_timestamp.ipt_ta; --len >= 0;
			     ta++) {
				pv_showfield(pv, &IPFIELD(TSADDR),
					     &ta->ipt_addr, "%-25.25s",
					     ip_hostname(ta->ipt_addr,IP_HOST));
				pv_showfield(pv, &IPFIELD(TSTIME),
					     &ta->ipt_time, "%-12.12s",
					     ip_timestamp(ta->ipt_time));
			}
		  }
		}
		break;
	  }

	  default:
		if (pv->pv_level < PV_VERBOSE)
			break;
		len = MIN(ipo->ipo_len, IP_MAXOPTLEN) - 1;
		if (len > 0)
			pv_showhex(pv, (u_char*)&ipo->ipo_len, pv->pv_off, len);
	}
}
