/*
 * Copyright 1989 Silicon Graphics, Inc.  All rights reserved.
 *
 * FDDI snoop module.
 */
#include <bstring.h>
#include <sys/types.h>
#include <netinet/in.h>		/* for ntohs() */
#include "index.h"
#include "enum.h"
#include "protodefs.h"
#include "strings.h"
#include "protocols/fddi.h"

char fddiname[] = "fddi";

/*
 * FDDI field identifiers and descriptors.
 */
enum fddifid {
	PAD,
	FS,
	FC,
	DST=FDDIFID_DST, SRC=FDDIFID_SRC,
	DISCRIM
};

#define FDDIADDRLEN	sizeof(LFDDI_ADDR)

static ProtoField fddifields[] = {
	PFI_BYTE("pad",	 "Padding",		PAD,FDDI_FILLER-1,PV_VERBOSE+1),
	PFI_UINT("status", "Frame Status",	FS,   u_char,	    PV_DEFAULT),
	PFI_UINT("control", "Frame Control",	FC,   u_char,	    PV_DEFAULT),
	PFI_ADDR("dst",	 "Destination Address",	DST,  FDDIADDRLEN,  PV_TERSE),
	PFI_ADDR("src",	 "Source Address",	SRC,  FDDIADDRLEN,  PV_TERSE),
};

static ProtoField discrim =
	PFI("discrim",   "Type Discriminant",   DISCRIM, (void*)DS_ZERO_EXTEND,
	    sizeof(u_char), 3, EXOP_NUMBER, PV_VERBOSE+1);

#define	FDDIFID(pf)	((enum fddifid) (pf)->pf_id)
#define	FDDIFIELD(fid)	fddifields[(int) fid]

static Enumerator fsbits[] = {
	EI_VAL("DA_MATCH",	MAC_DA_BIT),
	EI_VAL("BAD_PKT",	MAC_ERROR_BIT),
	EI_VAL("C_BIT",		MAC_C_BIT),
	EI_VAL("A_BIT",		MAC_A_BIT),
	EI_VAL("E_BIT",		MAC_E_BIT),
	EI_VAL("MTU",		4352),
};

/*
 * FDDI protocol macros and options.
 */
static ProtoMacro fddimacros[] = {
	PMI("between",  "src == $1 && dst == $2 || dst == $1 && src == $2"),
	PMI("host",     "src == $1 || dst == $1"),
	PMI("arp",	"llc.arp"),
    	PMI("arpip",	"llc.arpip"),
	PMI("ip",	"llc.ip"),
	PMI("hello",	"llc.hello"),
	PMI("icmp",	"llc.icmp"),
	PMI("igmp",	"llc.igmp"),
	PMI("rarp",	"llc.rarp"),
	PMI("rarpip",	"llc.rarpip"),
	PMI("rip",	"llc.rip"),
	PMI("sunrpc",	"llc.sunrpc"),
	PMI("nfs",	"llc.nfs"),
	PMI("tcp",	"llc.tcp"),
	PMI("udp",	"llc.udp"),
	PMI("bootp",	"llc.bootp"),
	PMI("dns",	"llc.dns"),
	PMI("snmp",	"llc.snmp"),
	PMI("tftp",	"llc.tftp"),
	PMI("aarp",	"aarp2"),
	PMI("ddp",	"ddp2"),
	PMI("rtmp",	"rtmp2"),
	PMI("aep",	"aep2"),
	PMI("adsp",	"adsp2"),
	PMI("zip",	"zip2"),
	PMI("nbp",	"nbp2"),
	PMI("atp",	"atp2"),
	PMI("pap",	"pap2"),
	PMI("asp",	"asp2"),
	PMI("afp",	"afp2"),
	PMI("idp",	"llc.idp"),
	PMI("ipx",	"llc.ipx"),
	PMI("echo",	"llc.echo"),
	PMI("error",	"llc.error"),
	PMI("spp",	"llc.spp"),
	PMI("xnsrip",	"llc.xnsrip"),
	PMI("pep",	"llc.pep"),
	PMI("decnet",	"llc.decnet"),
	PMI("nsp",	"llc.nsp"),
	PMI("lat",	"llc.lat"),
	PMI("xtp",	"llc.xtp"),
	PMI("ftp",	"llc.ftp"),
	PMI("netbios",	"llc.netbios"),
	PMI("scp",	"llc.scp"),
	PMI("smtp",	"llc.smtp"),
	PMI("spx",	"llc.spx"),
	PMI("telnet",	"llc.telnet"),
	PMI("timed",	"llc.timed"),
	PMI("tsp",	"llc.tsp"),
	PMI("ftp",	"llc.ftp"),
	PMI("novellecho","llc.novellecho"),
	PMI("novellerror","llc.novellerror"),
	PMI("novellrip","llc.novellrip"),
	PMI("osi",	"llc.osi"),
	PMI("rcp",	"llc.rcp"),
	PMI("rlogin",	"llc.rlogin"),
	PMI("sna",	"llc.sna"),
	PMI("vines",	"llc.vines"),
	PMI("x11",	"llc.x11"),
	PMI("ARP",	"llc.ARP"),
    	PMI("ARPIP",	"llc.ARPIP"),
	PMI("HELLO",	"llc.HELLO"),
	PMI("IP",	"llc.IP"),
	PMI("ICMP",	"llc.ICMP"),
	PMI("IGMP",	"llc.IGMP"),
	PMI("RARP",	"llc.RARPIP"),
	PMI("RARPIP",	"llc.RARPIP"),
	PMI("RIP",	"llc.RIP"),
	PMI("SUNRPC",	"llc.SUNRPC"),
	PMI("NFS",	"llc.NFS"),
	PMI("TCP",	"llc.TCP"),
	PMI("UDP",	"llc.UDP"),
	PMI("BOOTP",	"llc.BOOTP"),
	PMI("DNS",	"llc.DNS"),
	PMI("SNMP",	"llc.SNMP"),
	PMI("TFTP",	"llc.TFTP"),
	PMI("AARP",	"AARP2"),
	PMI("DDP",	"DDP2"),
	PMI("RTMP",	"RTMP2"),
	PMI("AEP",	"AEP2"),
	PMI("ADSP",	"ADSP2"),
	PMI("ZIP",	"ZIP2"),
	PMI("NBP",	"NBP2"),
	PMI("ATP",	"ATP2"),
	PMI("PAP",	"PAP2"),
	PMI("ASP",	"ASP2"),
	PMI("AFP",	"AFP2"),
	PMI("IDP",	"llc.IDP"),
	PMI("IPX",	"llc.IPX"),
/*	PMI("ECHO",	"llc.ECHO"),	Don't cover icmp ECHO constant */
	PMI("ERROR",	"llc.ERROR"),
	PMI("SPP",	"llc.SPP"),
	PMI("XNSRIP",	"llc.XNSRIP"),
	PMI("PEP",	"llc.PEP"),
	PMI("DECNET",	"llc.DECNET"),
	PMI("NSP",	"llc.NSP"),
	PMI("LAT",	"llc.LAT"),
	PMI("XTP",	"llc.XTP"),
	PMI("FTP",	"llc.FTP"),
	PMI("NETBIOS",	"llc.NETBIOS"),
	PMI("SCP",	"llc.SCP"),
	PMI("SMTP",	"llc.SMTP"),
	PMI("SPX",	"llc.SPX"),
	PMI("TELNET",	"llc.TELNET"),
	PMI("TIMED",	"llc.TIMED"),
	PMI("TSP",	"llc.TSP"),
	PMI("FTP",	"llc.FTP"),
	PMI("NOVELLECHO","llc.NOVELLECHO"),
	PMI("NOVELLERROR","llc.NOVELLERROR"),
	PMI("NOVELLRIP","llc.NOVELLRIP"),
	PMI("OSI",	"llc.OSI"),
	PMI("RCP",	"llc.RCP"),
	PMI("RLOGIN",	"llc.RLOGIN"),
	PMI("SNA",	"llc.SNA"),
	PMI("VINES",	"llc.VINES"),
	PMI("X11",	"llc.X11"),
	PMI("multicast",
	    "(dst & 80-0-0-0-0-0) == 80-0-0-0-0-0 && dst != BROADCAST"),
};

static ProtOptDesc fddioptdesc[] = {
    POD("hostbyname", FDDI_PROPT_HOSTBYNAME,
		      "Decode MAC addresses into hostnames"),
    POD("nativeorder", FDDI_PROPT_NATIVEORDER,
		      "Show MAC addresses using FDDI bit ordering"),
};

LFDDI_ADDR fddibroadcastaddr = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

DefineProtocol(fddi, fddiname, "Fiber Distributed Data Interface", PRID_FDDI,
	       DS_BIG_ENDIAN, sizeof(struct lmac_hdr), &discrim, 0,
	       fddioptdesc, lengthof(fddioptdesc), 0);

static Index *fdditypes;

static int
fddi_init()
{
	if (!pr_register(&fddi_proto, fddifields, lengthof(fddifields),
			 lengthof(fsbits) + lengthof(fddimacros) + 10)) {
		return 0;
	}
	pr_addmacros(&fddi_proto, fddimacros, lengthof(fddimacros));
	pr_addconstaddress(&fddi_proto, "BROADCAST",
		(char *)&fddibroadcastaddr, sizeof(fddibroadcastaddr));
	pr_addnumbers(&fddi_proto, fsbits, lengthof(fsbits));
	in_create(3, sizeof(u_short), 0, &fdditypes);
	return(1);
}

/* ARGSUSED */
static void
fddi_embed(Protocol *pr, long prototype, long qualifier)
{
	u_short type = prototype;

	in_enter(fdditypes, &type, pr);
}

static ExprType
fddi_compile(ProtoField *pf, Expr *mex, Expr *tex, ProtoCompiler *pc)
{
	enum fddifid fid;

	fid = FDDIFID(pf);
	switch (fid) {
	  case PAD:
		return ET_ERROR;

	  case FS:
	  case FC:
	  case DISCRIM: {
		long mask, match;

		if (!pc_intmask(pc, mex, &mask))
			return ET_ERROR;
		switch (tex->ex_op) {
		  case EXOP_PROTOCOL:
			switch (fid) {
			  case FS:
				pc_badop(pc, tex);
				return ET_ERROR;
			  case FC:
				match = tex->ex_prsym->sym_prototype;
				break;
			  case DISCRIM:
				switch (tex->ex_prsym->sym_prototype) {
				  case PRID_MAC:
					mask &= 0xb0;
					match = MAC_FC_MAC_FF;
					break;
				  case PRID_SMT:
					mask &= 0xb0;
					match = MAC_FC_SMT_FF;
					break;
				  case PRID_LLC:
					mask &= 0x78;
					match = MAC_FC_LLC_FF | MAC_FC_ALEN;
					break;
				  case PRID_FIMPL:
					mask &= 0x38;
					match = MAC_FC_IMP_FF;
					break;
				  default:
					mask = match = 0;
				}
			}
			break;
		  case EXOP_NUMBER:
			match = tex->ex_val;
			break;
		  default:
			pc_badop(pc, tex);
			return ET_ERROR;
		}
		if (!pc_intfield(pc, pf, mask, match, sizeof(struct lmac_hdr)))
			return ET_COMPLEX;
		break;
	  }

	  case DST:
	  case SRC: {
		u_char *mask;

		if (mex == 0)
			mask = (u_char *) PC_ALLBYTES;
		else {
			if (mex->ex_op != EXOP_ADDRESS) {
				pc_badop(pc, mex);
				return ET_ERROR;
			}
			mask = A_BASE(&mex->ex_addr, pf->pf_size);
		}
		if (tex->ex_op != EXOP_ADDRESS) {
			pc_badop(pc, tex);
			return ET_ERROR;
		}
		if (!pc_bytefield(pc, pf, (char *) mask,
				  (char *) A_BASE(&tex->ex_addr, pf->pf_size),
				  sizeof(struct lmac_hdr))) {
			return ET_COMPLEX;
		}
		break;
	  }
	}
	return ET_SIMPLE;
}

static u_short
fddi_fctoid(register u_short key)
{
	if (MAC_FC_IS_MAC(key))
		key = PRID_MAC;
	else if (MAC_FC_IS_SMT(key))
		key = PRID_SMT;
	else if (MAC_FC_IS_LLC(key))
		key = PRID_LLC;
	else if (MAC_FC_IS_IMP(key))
		key = PRID_FIMPL;
	return key;
}

static int
fddi_match(Expr *pex, DataStream *ds, ProtoStack *ps, Expr *rex)
{
	struct lmac_hdr *fh;
	struct fddiframe ff;
	int matched;

	fh = (struct lmac_hdr *)ds_inline(ds, sizeof(*fh), sizeof(u_long));
	if (fh == 0 || fddi_fctoid(fh->mac_fc) != pex->ex_prsym->sym_prototype)
		return 0;
	PS_PUSH(ps, &ff.ff_frame, &fddi_proto);
	ff.ff_hdr = *fh;
	matched = ex_match(pex, ds, ps, rex);
	PS_POP(ps);
	return matched;
}

/* ARGSUSED */
static int
fddi_fetch(ProtoField *pf, DataStream *ds, ProtoStack *ps, Expr *rex)
{
	struct lmac_hdr *fh;

	fh = (struct lmac_hdr *)ds_inline(ds, sizeof(*fh), sizeof(u_long));
	if (fh == 0)
		return 0;
	switch (FDDIFID(pf)) {
	  case PAD:
		return 0;
	  case FS:
		rex->ex_val = fh->mac_bits;
		rex->ex_op = EXOP_NUMBER;
		break;
	  case FC:
		rex->ex_val = fh->mac_fc;
		rex->ex_op = EXOP_NUMBER;
		break;
	  case DST:
		*A_CAST(&rex->ex_addr, LFDDI_ADDR) = fh->mac_da;
		rex->ex_op = EXOP_ADDRESS;
		break;
	  case SRC:
		*A_CAST(&rex->ex_addr, LFDDI_ADDR) = fh->mac_sa;
		rex->ex_op = EXOP_ADDRESS;
		break;
	  case DISCRIM:
		rex->ex_val = fddi_fctoid(fh->mac_fc);
		rex->ex_op = EXOP_NUMBER;
	}
	return 1;
}

static void
fddi_decode(DataStream *ds, ProtoStack *ps, PacketView *pv)
{
	struct lmac_hdr *fh;
	Protocol *pr;
	struct fddiframe ff;
	u_short key;
	int offset;

	fh = (struct lmac_hdr *)ds_inline(ds, sizeof *fh, sizeof(u_long));
	if (fh == 0)
		return;
	pv->pv_off += (FDDI_FILLER-1);
	pv_showfield(pv, &FDDIFIELD(FS), &fh->mac_bits,
		    "%-10s", en_bitset(fsbits, lengthof(fsbits), fh->mac_bits));
	pv_showfield(pv, &FDDIFIELD(FC), &fh->mac_fc, "%#02x", fh->mac_fc);
	pv_break(pv);

	/*
	 * Show src before dst so that higher layer protocols' source and
	 * destination fields line up.
	 */
	offset = pv->pv_off = FDDIFIELD(SRC).pf_off;
	pv_showfield(pv, &FDDIFIELD(SRC), &fh->mac_sa,
		     "%-25.25s", fddi_hostname(&fh->mac_sa));
	pv->pv_off = FDDIFIELD(DST).pf_off;
	pv_showfield(pv, &FDDIFIELD(DST), &fh->mac_da,
		     "%-25.25s", fddi_hostname(&fh->mac_da));
	pv->pv_off = offset + FDDIFIELD(SRC).pf_size;

	/*
	 * If we can't decode the network protocol, then promote type level
	 * to terse and try to print a description of it.
	 */
	key = fddi_fctoid(fh->mac_fc);
	pr = in_match(fdditypes, &key);
	PS_PUSH(ps, &ff.ff_frame, &fddi_proto);
	ff.ff_hdr = *fh;
	pv_decodeframe(pv, pr, ds, ps);
	PS_POP(ps);
}

/* ARGSUSED */
Expr *
fddi_resolve(char *name, int len, struct snooper *sn)
{
	extern LFDDI_ADDR *fddi_hostaddr(char *);
	LFDDI_ADDR *fa;
	LFDDI_ADDR addr;
	Expr *ex;

	fa = fddi_hostaddr(name);
	if (fa == 0) {
		int n, v[5];
		char *bp, *ep;
		unsigned char *vp;

		n = 0;
		bp = name;
		for (;;) {
			v[n] = strtol(bp, &ep, 16);
			if (bp == ep)
				return 0;
			if (*ep == '\0')
				break;
			if (*ep != '-' || ++n >= 5)
				return 0;
			bp = ep + 1;
		}
		fa = &addr;
		vp = &fa->b[sizeof(*fa)];
		while (--n >= 0)
			*--vp = v[n];
		while (vp > &fa->b[0])
			*--vp = 0;
	}
	ex = expr(EXOP_ADDRESS, EX_NULLARY, name);
	*A_CAST(&ex->ex_addr, LFDDI_ADDR) = *fa;
	return ex;
}

int
fddi_setopt(int id, char *val)
{
	extern int _fddihostbyname, _fddibitorder;

	switch ((enum fddi_propt) id) {
	    case FDDI_PROPT_HOSTBYNAME:
		_fddihostbyname = (*val != '0');
		break;
	    case FDDI_PROPT_NATIVEORDER:
		_fddibitorder = (*val != '0');
		break;
	    default:
		return 0;
	}
	return 1;
}
