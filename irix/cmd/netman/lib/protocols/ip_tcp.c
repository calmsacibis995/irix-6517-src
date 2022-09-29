/*
 * Copyright 1990 Silicon Graphics, Inc.  All rights reserved.
 *
 * Transmission Control Protocol (TCP), defined in RFC 793.
 */
#include <bstring.h>
#include <netdb.h>
#include <stdio.h>
#ifndef sun
#include <stdlib.h>
#endif
#include <values.h>
#include <rpc/types.h>
#include <sys/socket.h>
#include <rpc/xdr.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include "cache.h"
#include "enum.h"
#include "heap.h"
#include "index.h"
#include "protodefs.h"
#include "protocols/ip.h"
#include "protocols/sunrpc.h"
#include "tcp.h"

char tcpname[] = "tcp";

/*
 * TCP field identifiers and descriptors.
 */
enum tcpfid {
	SPORT=IPFID_SPORT, DPORT=IPFID_DPORT,
	SEQ, ACK, OFF, X2, FLAGS, WIN, SUM, URP,
	OPT, OPTLEN, MAXSEG, WINSCALE, TSVAL, TSECR
};
#define	TCPF_ISOPTION(pf)	((pf)->pf_id >= (int) OPT)

static ProtoField tcpfields[] = {
	PFI_UINT("sport",  "Source Port",	   SPORT,  u_short, PV_TERSE),
	PFI_UINT("dport",  "Destination Port",	   DPORT,  u_short, PV_TERSE),
	PFI_UINT("seq",    "Sequence Number",	   SEQ,    tcp_seq, PV_DEFAULT),
	PFI_UINT("ack",    "Acknowledgment Number",ACK,    tcp_seq, PV_DEFAULT),
	PFI_UBIT("off",    "Data Offset",	   OFF,    4,	  PV_VERBOSE),
	PFI_UBIT("x2",     "Reserved",		   X2,     4,	  PV_VERBOSE+1),
	PFI_UINT("flags",  "Flags",		   FLAGS,  u_char,  PV_DEFAULT),
	PFI_UINT("win",    "Window",		   WIN,    u_short, PV_DEFAULT),
	PFI_UINT("sum",    "Checksum",		   SUM,    u_short, PV_VERBOSE),
	PFI_UINT("urp",    "Urgent Pointer",	   URP,    u_short, PV_DEFAULT),
	PFI_UINT("opt",    "Option Type",	   OPT,    u_char,  PV_VERBOSE),
	PFI_UINT("optlen", "Option Length",	   OPTLEN, u_char,  PV_VERBOSE),
	PFI_UINT("maxseg", "Maximum Segment Size", MAXSEG, u_short, PV_DEFAULT),
	PFI_UINT("winscale", "Window Scale",	   WINSCALE,u_char, PV_DEFAULT),
	PFI_UINT("tsval",  "Timestamp Value",	   TSVAL,  u_long,  PV_DEFAULT),
	PFI_UINT("tsecr",  "Timestamp Echo Reply", TSECR,  u_long,  PV_DEFAULT),
};


#define	TCPFID(pf)	((enum tcpfid) (pf)->pf_id)
#define	TCPFIELD(fid)	tcpfields[(int) fid]

static ProtoMacro tcpnicknames[] = {
	PMI("TCP",	tcpname),
};

static Enumeration tcpopt;
static Enumerator tcpoptvec[] = {
	EI_VAL("EOL",		TCPOPT_EOL),
	EI_VAL("NOP",		TCPOPT_NOP),
	EI_VAL("MAXSEG",	TCPOPT_MAXSEG),
	EI_VAL("WINSCALE",	TCPOPT_WINDOW),
	EI_VAL("TIMESTAMP",	TCPOPT_TIMESTAMP)
};

static Enumerator tcpflags[] = {
	EI_VAL("FIN",	TH_FIN),
	EI_VAL("SYN",	TH_SYN),
	EI_VAL("RST",	TH_RST),
	EI_VAL("PUSH",	TH_PUSH),
	EI_VAL("ACK",	TH_ACK),
	EI_VAL("URG",	TH_URG),
};

static ProtoMacro tcpmacros[] = {
	PMI("between",
	    "sport == $1 && dport == $2 || dport == $1 && sport == $2"),
	PMI("port",	"sport == $1 || dport == $1"),
};

static ProtOptDesc tcpoptdesc[] = {
	POD("seqcomma", TCP_PROPT_SEQCOMMA,
			"Sequence number digit-triad separator"),
	POD("setport",  TCP_PROPT_SETPORT,
			"Map port number to protocol ([host:]port/protocol)"),
	POD("zeroseq",  TCP_PROPT_ZEROSEQ,
			"Start connection sequence numbers at zero"),
};

/*
 * TCP protocol interface and operations.
 */
DefineProtocol(tcp, tcpname, "Transmission Control Protocol", PRID_TCP,
	       DS_BIG_ENDIAN, sizeof(struct tcphdr), 0,
	       0, tcpoptdesc, lengthof(tcpoptdesc), 0);

static Index	*tcpports;
static Protocol	*sunrpc;

/*
 * TCP protocol functions.
 */
/* ARGSUSED */
int
tcp_func_badsum(Expr *argv, DataStream *ds, ProtoStack *ps, Expr *rex)
{
	struct tcphdr *th;
	u_short realsum, idealsum;
	struct ipframe *ipf;

	th = (struct tcphdr *) ds_inline(ds, sizeof *th, IP_HDRGRAIN);
	if (th == 0)
		return 0;
	ipf = PS_TOP(ps, struct ipframe);
	if (ipf == 0)
		return 0;
	realsum = th->th_sum;
	th->th_sum = 0;
	if (!ip_checksum_pseudohdr(PS_TOP(ps, struct ipframe), (char *) th,
				   sizeof *th + ds->ds_count, &idealsum)) {
		th->th_sum = realsum;
		return 0;
	}
	th->th_sum = realsum;
	rex->ex_op = EXOP_NUMBER;
	rex->ex_val = (ntohs(realsum) != idealsum);
	return 1;
}

static ProtoFuncDesc tcpfunctions[] = {
	PFD("badsum", tcp_func_badsum, 0,
	    "Match packet if TCP checksum is incorrect"),
};

static int
tcp_init()
{
	if (!pr_register(&tcp_proto, tcpfields, lengthof(tcpfields),
			 lengthof(tcpoptvec) + lengthof(tcpflags)
			 + lengthof(tcpmacros) + 8)) {
		return 0;
	}
	if (!pr_nest(&tcp_proto, PRID_IP, IPPROTO_TCP, tcpnicknames,
		     lengthof(tcpnicknames))) {
		return 0;
	}
	in_create(10, sizeof(u_short), 0, &tcpports);
	en_init(&tcpopt, tcpoptvec, lengthof(tcpoptvec), &tcp_proto);
	pr_addnumbers(&tcp_proto, tcpflags, lengthof(tcpflags));
	pr_addmacros(&tcp_proto, tcpmacros, lengthof(tcpmacros));
	pr_addfunctions(&tcp_proto, tcpfunctions, lengthof(tcpfunctions));
	return 1;
}

/*
 * Option processing state.
 */
static char	tcpseqcomma = ',';
static Cache	*tcpzeroseq;

struct connkey {
	struct in_addr	src;	/* SYN sender */
	struct in_addr	dst;	/* SYN receiver */
	u_short		sport;	/* ports */
	u_short		dport;
};

struct connval {
	bool_t		sroute;	/* source route flag */
	tcp_seq		seq0;	/* initial SYN seq */
	tcp_seq		ack0;	/* SYN ACK seq */
	tcp_seq		fin1;	/* first FIN seq */
	tcp_seq		fin2;	/* second FIN seq */
};

static unsigned int
tcp_hashconn(struct connkey *ck)
{
	return ck->src.s_addr + ck->dst.s_addr + ck->dport + ck->sport;
}

static int
tcp_cmpconn(struct connkey *ck1, struct connkey *ck2)
{
	int srcsame, dstsame;

	srcsame = (ck1->src.s_addr == ck2->src.s_addr);
	if (!srcsame && ck1->src.s_addr != ck2->dst.s_addr)
		return 1;
	dstsame = (ck1->dst.s_addr == ck2->dst.s_addr);
	if (!dstsame && ck1->dst.s_addr != ck2->src.s_addr)
		return 1;
	if (ck1->sport != (srcsame ? ck2->sport : ck2->dport))
		return 1;
	if (ck1->dport != (dstsame ? ck2->dport : ck2->sport))
		return 1;
	return 0;
}

#define	xdr_tcp_seq(xdr, seqp)	xdr_u_long(xdr, (u_long *)seqp)

static int
xdr_connval(XDR *xdr, struct connval **cvp)
{
	struct connval *cv;

	cv = *cvp;
	switch (xdr->x_op) {
	  case XDR_DECODE:
		if (cv == 0)
			*cvp = cv = new(struct connval);
		/* FALL THROUGH */
	  case XDR_ENCODE:
		return xdr_bool(xdr, &cv->sroute)
		    && xdr_tcp_seq(xdr, &cv->seq0)
		    && xdr_tcp_seq(xdr, &cv->ack0)
		    && xdr_tcp_seq(xdr, &cv->fin1)
		    && xdr_tcp_seq(xdr, &cv->fin2);
	  case XDR_FREE:
		delete(cv);
		*cvp = 0;
	}
	return TRUE;
}

static void
tcp_dumpconn(struct connkey *ck, struct connval *cv)
{
	printf("(%s.%u,%s.%u) -> (%s,%u,%u,%u,%u)\n",
		ip_hostname(ck->src, IP_HOST), ck->sport,
		ip_hostname(ck->dst, IP_HOST), ck->dport,
		(cv->sroute) ? "source-route" : "direct",
		cv->seq0, cv->ack0, cv->fin1, cv->fin2);
}

struct cacheops tcpzeroseq_cacheops =
	{ { tcp_hashconn, tcp_cmpconn, xdr_connval }, 0, tcp_dumpconn };

static int
tcp_setopt(int id, char *val)
{
	switch ((enum tcp_propt) id) {
	  case TCP_PROPT_SEQCOMMA:
		if (*val == '0')
			tcpseqcomma = '\0';
		else if (*val == '1')
			tcpseqcomma = ',';
		else
			tcpseqcomma = *val;
		break;
	  case TCP_PROPT_SETPORT:
		if (!ip_setport(val, tcp_embed))
			return 0;
		break;
	  case TCP_PROPT_ZEROSEQ:
		c_create("tcp.zeroseq", 64, sizeof(struct connkey), 1000 HOURS,
			 &tcpzeroseq_cacheops, &tcpzeroseq);
		break;
	  default:
		return 0;
	}
	return 1;
}

static void
tcp_embed(Protocol *pr, long prototype, long qualifier)
{
	u_short port;

	port = prototype;
	if (pr->pr_id == PRID_SUNRPC)
		sunrpc = pr;
	in_enterqual(tcpports, &port, pr, qualifier);
}

/* ARGSUSED */
static Expr *
tcp_resolve(char *name, int len, struct snooper *sn)
{
	struct servent *sp;
	u_short port;
	Expr *ex;

	sp = getservbyname(name, tcpname);
	if (sp)
		port = sp->s_port;
	else if (!sunrpc_getport(name, IPPROTO_TCP, &port))
		return 0;
	ex = expr(EXOP_NUMBER, EX_NULLARY, name);
	ex->ex_val = port;
	return ex;
}

static ExprType
tcp_compile(ProtoField *pf, Expr *mex, Expr *tex, ProtoCompiler *pc)
{
	long mask;

	if (TCPF_ISOPTION(pf))
		return ET_COMPLEX;
	if (!pc_intmask(pc, mex, &mask))
		return ET_ERROR;
	if (tex->ex_op != EXOP_NUMBER) {
		pc_badop(pc, tex);
		return ET_ERROR;
	}
	if (!pc_intfield(pc, pf, mask, tex->ex_val, sizeof(struct tcphdr)))
		return ET_COMPLEX;
	return ET_SIMPLE;
}

/*
 * Lookup port's protocol in tcpports.
 */
static Protocol *
tcp_findproto(u_short port, struct in_addr addr)
{
	Protocol *pr;

	pr = in_matchqual(tcpports, &port, addr.s_addr);
	if (pr)
		return pr;
	if (sunrpc_ismapped(port, addr, IPPROTO_TCP))
		return sunrpc;
	return 0;
}

static int
tcp_match(Expr *pex, DataStream *ds, ProtoStack *ps, Expr *rex)
{
	struct tcphdr *th;
	struct ipframe *ipf;
	int skip;
	Protocol *pr;

	th = (struct tcphdr *) ds_inline(ds, sizeof *th, IP_HDRGRAIN);
	if (th == 0)
		return 0;
	ipf = PS_TOP(ps, struct ipframe);
	ipf->ipf_proto = &tcp_proto;
	ipf->ipf_sport = ntohs(th->th_sport);
	ipf->ipf_dport = ntohs(th->th_dport);
	skip = IP_HDRLEN(th->th_off) - sizeof *th;
	if (skip > 0)
		(void) ds_seek(ds, skip, DS_RELATIVE);
	if ((pr = tcp_findproto(ipf->ipf_sport, ipf->ipf_src)) == 0
	    && (pr = tcp_findproto(ipf->ipf_dport, ipf->ipf_rdst)) == 0
	    || pr != pex->ex_prsym->sym_proto) {
		return 0;
	}
	return ex_match(pex, ds, ps, rex);
}

/* ARGSUSED */
static int
tcp_fetch(ProtoField *pf, DataStream *ds, ProtoStack *ps, Expr *rex)
{
	struct tcphdr *th;
	int skip;

	th = (struct tcphdr *) ds_inline(ds, sizeof *th, IP_HDRGRAIN);
	if (th) {
		skip = th->th_off;
		switch (TCPFID(pf)) {
		  case SPORT:
			rex->ex_val = ntohs(th->th_sport);
			break;
		  case DPORT:
			rex->ex_val = ntohs(th->th_dport);
			break;
		  case SEQ:
			rex->ex_val = ntohl(th->th_seq);
			break;
		  case ACK:
			rex->ex_val = ntohl(th->th_ack);
			break;
		  case OFF:
			rex->ex_val = th->th_off;
			break;
		  case X2:
			rex->ex_val = th->th_x2;
			break;
		  case FLAGS:
			rex->ex_val = th->th_flags;
			break;
		  case WIN:
			rex->ex_val = ntohs(th->th_win);
			break;
		  case SUM:
			rex->ex_val = ntohs(th->th_sum);
			break;
		  case URP:
			rex->ex_val = ntohs(th->th_urp);
			break;
		  default: {
			struct tcp_opt *to;
			u_long ts;

			if (skip <= sizeof *th / IP_HDRGRAIN)
				return 0;
			to = (struct tcp_opt *)
				ds_inline(ds, sizeof to->to_opt +
					  sizeof to->to_len, IP_HDRGRAIN);
			if (to == 0 || to->to_len < 2 || to->to_len > 32)
				return 0;
			if (!ds_inline(ds, to->to_len - 2, 1))
				return 0;
			skip -= to->to_len / IP_HDRGRAIN;
			switch (TCPFID(pf)) {
			  case OPT:
				rex->ex_val = to->to_opt;
				break;
			  case OPTLEN:
				if (to->to_opt < TCPOPT_MAXSEG)
					return 0;
				rex->ex_val = to->to_len;
				break;
			  case MAXSEG:
				if (to->to_opt != TCPOPT_MAXSEG)
					return 0;
				rex->ex_val = ntohs(to->to_maxseg);
				break;
			  case WINSCALE:
				if (to->to_opt != TCPOPT_WINDOW)
					return 0;
				rex->ex_val = to->to_winscale;
				break;
			  case TSVAL:
				if (to->to_opt != TCPOPT_TIMESTAMP)
					return 0;
				bcopy(to->to_tsval, &ts, sizeof ts);
				rex->ex_val = ntohl(ts);
				break;
			  case TSECR:
				if (to->to_opt != TCPOPT_TIMESTAMP)
					return 0;
				bcopy(to->to_tsecr, &ts, sizeof ts);
				rex->ex_val = ntohl(ts);
				break;
			}
		  }
		}
		rex->ex_op = EXOP_NUMBER;
	} else {
		if (TCPFID(pf) != OFF) {
			if (!ds_field(ds, &TCPFIELD(OFF), 0, rex)) {
				if (TCPFID(pf) > OFF)
					return 0;
				skip = sizeof *th / IP_HDRGRAIN;
			} else
				skip = rex->ex_val;
		}
		if (!ds_field(ds, pf, sizeof *th, rex))
			return 0;
		if (TCPFID(pf) == OFF)
			skip = rex->ex_val;
	}
	skip = IP_HDRLEN(skip) - sizeof *th;
	if (skip > 0)
		(void) ds_seek(ds, skip, DS_RELATIVE);
	return 1;
}

void	tcp_seqadj(struct tcphdr *, struct ipframe *);
char	*tcp_seqstr(tcp_seq);

static void
tcp_decode(DataStream *ds, ProtoStack *ps, PacketView *pv)
{
	struct ipframe *ipf;
	struct tcphdr *th, hdr;
	u_short sum;
	Protocol *spr;
	Protocol *dpr;
	int optlen;
	struct tcp tcpf;

	ipf = PS_TOP(ps, struct ipframe);
	th = (struct tcphdr *) ds_inline(ds, sizeof *th, IP_HDRGRAIN);
	if (th == 0) {
		sum = 0;
		th = &hdr;
		ds_tcphdr(ds, th);
	} else {
		u_short tcpsum;

		tcpsum = th->th_sum;
		th->th_sum = 0;
		if (!ip_checksum_pseudohdr(ipf, (char *) th,
					   sizeof *th + ds->ds_count, &sum)) {
			sum = 0;
		}
		th->th_sum = ntohs(tcpsum);
		th->th_sport = ntohs(th->th_sport);
		th->th_dport = ntohs(th->th_dport);
		th->th_seq = ntohl(th->th_seq);
		th->th_ack = ntohl(th->th_ack);
		th->th_win = ntohs(th->th_win);
		th->th_sum = ntohs(th->th_sum);
		th->th_urp = ntohs(th->th_urp);
	}

	spr = tcp_findproto(th->th_sport, ipf->ipf_src);
	dpr = tcp_findproto(th->th_dport, ipf->ipf_rdst);

	pv_showfield(pv, &TCPFIELD(SPORT), &th->th_sport,
		     "%-22.22s", ip_service(th->th_sport, tcpname, spr));
	pv_showfield(pv, &TCPFIELD(DPORT), &th->th_dport,
		     "%-22.22s", ip_service(th->th_dport, tcpname, dpr));
	pv_break(pv);

	if (tcpzeroseq)
		tcp_seqadj(th, ipf);
	pv_showfield(pv, &TCPFIELD(SEQ), &th->th_seq,
		     "%-13s", tcp_seqstr(th->th_seq));
	pv_showfield(pv, &TCPFIELD(ACK), &th->th_ack,
		     "%-13s", tcp_seqstr(th->th_ack));
	pv_showfield(pv, &TCPFIELD(OFF), &th->th_ack + 1,
		     "%-2d", th->th_off);
	pv_showfield(pv, &TCPFIELD(X2), &th->th_ack + 1,
		     "%#-3x", th->th_x2);
	pv_showfield(pv, &TCPFIELD(FLAGS), &th->th_flags,
		     "%-10s", en_bitset(tcpflags, lengthof(tcpflags),
					th->th_flags));

	pv_showfield(pv, &TCPFIELD(WIN), &th->th_win,
		     "%-13s", tcp_seqstr(th->th_win));
	if (sum && th->th_sum != sum) {
		pv_showfield(pv, &TCPFIELD(SUM), &th->th_sum,
			     "%#x [!= %#x]", th->th_sum, sum);
	} else {
		pv_showfield(pv, &TCPFIELD(SUM), &th->th_sum,
			     "%#-6x", th->th_sum);
	}
	pv_showfield(pv, &TCPFIELD(URP), &th->th_urp,
		     "%u", th->th_urp);

	/*
	 * Decode TCP options.
	 */
	optlen = th->th_off - sizeof *th / IP_HDRGRAIN;
	if (optlen > 0) {
		int len;
		struct tcp_opt *to;
		u_short maxseg;
		u_long ts;

		pv_break(pv);
		optlen *= IP_HDRGRAIN;
		do {
			to = (struct tcp_opt *)
				ds_inline(ds, sizeof to->to_opt, 1);
			if (to == 0)
				return;
			pv_showfield(pv, &TCPFIELD(OPT), &to->to_opt,
				     "%-6s", en_name(&tcpopt, to->to_opt));
			if (to->to_opt < TCPOPT_MAXSEG) {
				len = 1;
				continue;
			}
			if (!ds_inline(ds, sizeof to->to_len, 1))
				break;
			pv_showfield(pv, &TCPFIELD(OPTLEN), &to->to_len,
				     "%-3u", to->to_len);
			len = to->to_len;
			if (!ds_inline(ds, len - 2, 1))
				break;
			switch (to->to_opt) {
			  case TCPOPT_MAXSEG:
				bcopy(&to->to_maxseg, &maxseg, sizeof maxseg);
				maxseg = ntohs(maxseg);
				pv_showfield(pv, &TCPFIELD(MAXSEG), &maxseg,
					     "%-5u", maxseg);
				break;
			  case TCPOPT_WINDOW:
				pv_showfield(pv, &TCPFIELD(WINSCALE),
					     &to->to_winscale,
					     "%-5u", to->to_winscale);
				break;
			  case TCPOPT_TIMESTAMP:
				bcopy(to->to_tsval, &ts, sizeof ts);
				ts = ntohl(ts);
				pv_showfield(pv, &TCPFIELD(TSVAL), &ts,
					     "%-5u", ts);
				bcopy(to->to_tsecr, &ts, sizeof ts);
				ts = ntohl(ts);
				pv_showfield(pv, &TCPFIELD(TSECR), &ts,
					     "%-5u", ts);
				break;
			  default:
				/* Hexdump? */
				break;
			}
		} while ((optlen -= len) > 0);
	}

	ipf->ipf_proto = &tcp_proto;
	ipf->ipf_sport = th->th_sport;
	ipf->ipf_dport = th->th_dport;
	PS_PUSH(ps, &tcpf._psf, &tcp_proto);
	tcpf.urp = th->th_urp;
	tcpf.sport = th->th_sport;
	tcpf.dport = th->th_dport;
	pv_decodeframe(pv, spr ? spr : dpr, ds, ps);
	PS_POP(ps);
}

static void
tcp_seqadj(struct tcphdr *th, struct ipframe *ipf)
{
	struct connkey key;
	Entry **ep, *ent;
	u_short flags;
	struct connval *cv;
	tcp_seq seq, ack;

	/*
	 * Look for an entry associated with this connection.
	 */
	key.src = ipf->ipf_src;
	key.dst = ipf->ipf_rdst;
	key.sport = th->th_sport;
	key.dport = th->th_dport;
lookup:
	ep = c_lookup(tcpzeroseq, &key);
	ent = *ep;

	/*
	 * If not found, check for SYN and return early if we missed it,
	 * otherwise make a new entry.  If we hit the cache on a SYN, the
	 * entry must be left over from a previous partial trace or from
	 * the first hop of a source route.
	 */
	flags = th->th_flags;
	if (ent == 0) {
		if ((flags & (TH_SYN|TH_ACK)) != TH_SYN)
			return;
		cv = new(struct connval);
		cv->sroute = (ipf->ipf_dst.s_addr != ipf->ipf_rdst.s_addr);
		cv->seq0 = th->th_seq;
		cv->ack0 = cv->fin1 = cv->fin2 = 0;
		c_add(tcpzeroseq, &key, cv, ep);
		ent = *ep;
	} else {
		cv = ent->ent_value;
		if ((flags & (TH_SYN|TH_ACK)) == TH_SYN && !cv->sroute) {
			c_delete(tcpzeroseq, ep);
			goto lookup;
		}
		if ((flags & TH_ACK) && cv->ack0 == 0)
			cv->ack0 = th->th_seq;
	}

	/*
	 * Adjust th's sequence numbers, saving seq&ack first for the FIN
	 * code below.  Notice how the entry's key is used instead of key,
	 * so that sport and dport are the SYN and SYN|ACK senders.
	 */
	seq = th->th_seq;
	ack = th->th_ack;
	if (th->th_sport == ((struct connkey *)ent->ent_key)->sport) {
		th->th_seq -= cv->seq0;
		th->th_ack -= cv->ack0;
	} else {
		th->th_seq -= cv->ack0;
		th->th_ack -= cv->seq0;
	}

	/*
	 * Check for a FIN or an ACK of a FIN.  Upon the ACK of the second
	 * FIN, delete the cache entry.  Make sure we're inspecting the last
	 * hop of a source route.
	 */
	if (ipf->ipf_dst.s_addr != ipf->ipf_rdst.s_addr)
		return;
	if (flags & TH_FIN) {
		if (cv->fin1 == 0)
			cv->fin1 = seq;
		else
			cv->fin2 = seq;
	}
}

static char *
tcp_seqstr(tcp_seq seq)
{
	char *bp;
	int digit;
	static char buf[sizeof "d,ddd,ddd,ddd"];

	bp = &buf[sizeof buf - 1];
	digit = 0;
	for (;;) {
		*--bp = '0' + seq % 10;
		seq /= 10;
		if (seq == 0)
			break;
		digit++;
		if (digit == 3) {
			if (tcpseqcomma != '\0')
				*--bp = tcpseqcomma;
			digit = 0;
		}
	}
	return bp;
}
