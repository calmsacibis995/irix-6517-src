/*
 * Copyright 1996 Silicon Graphics, Inc.  All rights reserved.
 *
 * ATM snoop module.
 */
#include <bstring.h>
#include <sys/types.h>
#include "index.h"
#include "enum.h"
#include "protodefs.h"
#include "strings.h"

/*
 * This exists only to kludge llc so that it will work as the outer most
 * protocol.
 */


struct atmframe {
	ProtoStackFrame	    af_frame;	/* base class state */
};

char atmname[] = "atm";

/*
 * ATM protocol macros and options.
 */
static ProtoMacro atmmacros[] = {
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
};

#define atm_setopt pr_stub_setopt
#define atm_compile pr_stub_compile
#define atm_resolve pr_stub_resolve
#define atm_fetch pr_stub_fetch

DefineProtocol(atm, atmname, "AAL5 ATM",
	       PRID_ATM, DS_BIG_ENDIAN, 0, 0, 0, 0, 0, 0);

static Protocol *llc;


static int
atm_init(void)
{
	if (!pr_register(&atm_proto, 0, 0, lengthof(atmmacros) + 10)) {
		return 0;
	}
	pr_addmacros(&atm_proto, atmmacros, lengthof(atmmacros));
	pr_addconstnumber(&atm_proto, "MTU", 9180);
	return 1;
}


/* ARGSUSED */
static void
atm_embed(Protocol *pr, long prototype, long qualifier)
{
	if (pr->pr_id == PRID_LLC)
		llc = pr;
}


static void
atm_decode(DataStream *ds, ProtoStack *ps, PacketView *pv)
{
	Protocol *pr;
	struct atmframe af;
	u_short key;

	PS_PUSH(ps, &af.af_frame, &atm_proto);
	key = PRID_LLC;
	pv_decodeframe(pv, llc, ds, ps);
	PS_POP(ps);
}


static int
atm_match(Expr *pex, DataStream *ds, ProtoStack *ps, Expr *rex)
{
	struct atmframe af;
	int matched;

	PS_PUSH(ps, &af.af_frame, &atm_proto);
	matched = ex_match(pex, ds, ps, rex);
	PS_POP(ps);
	return matched;
}
