/*
 * Copyright (c) 1991 by Sun Microsystems, Inc.
 */
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/sun/snoop/RCS/snoop_ether.c,v 1.4 1998/11/25 20:09:56 jlan Exp $"

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/dlpi.h>

#include <sys/socket.h>
#include <sys/sockio.h>
#include <net/if.h>
#include <net/raw.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/if_ether.h>

#include "snoop.h"

static 	u_int ether_header_len(char *), fddi_header_len(char *),
		tr_header_len(char *), loop_header_len();

static void interpret_ether(), interpret_fddi(), interpret_tr(), 
interpret_loop();
void addr_copy_swap(struct ether_addr *pd, struct ether_addr *ps);

interface_t *interface;
interface_t INTERFACES[] = {

	/* IEEE 802.3 CSMA/CD network */
	{ DL_CSMACD, 1550, ether_header_len, interpret_ether, IF_HDR_FIXED },

	/* Ethernet Bus */
	{ DL_ETHER, 1550, ether_header_len, interpret_ether, IF_HDR_FIXED },

	/* Fiber Distributed data interface */
	{ DL_FDDI, 4500, fddi_header_len, interpret_fddi, IF_HDR_VAR },

	/* Token Ring interface */
	{ DL_TPR, 17800, tr_header_len, interpret_tr, IF_HDR_VAR },

	/* Loopback */
	{ DL_OTHER, 8264, loop_header_len, interpret_loop, IF_HDR_FIXED },

	{ -1, 0, 0, 0, 0 }

};


/* externals */
extern char *dlc_header;
extern int pi_frame;
extern int pi_time_hour;
extern int pi_time_min;
extern int pi_time_sec;
extern int pi_time_usec;

char *printether();
char *print_ethertype();
char *print_etherinfo();

char *print_fc();
char *print_smttype();
char *print_smtclass();

struct ether_addr ether_broadcast = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

void
interpret_ether(flags, e, elen, origlen)
	int flags;
	struct etherheader *e;
	int elen, origlen;
{
	char *off;
	int len;
	int ieee8023 = 0;
	char data[1600];
	extern char *dst_name;
	int ethertype;

	if (origlen < 14) {
		if (flags & F_SUM)
			(void) sprintf(get_sum_line(),
			"RUNT (short packet - %d bytes)",
			origlen);
		if (flags & F_DTAIL)
			show_header("RUNT:  ", "Short packet", origlen);
		return;
	}
	if (elen < 14)
		return;

	if (memcmp((char *) &e->ether_dhost, (char *)&ether_broadcast,
		sizeof (struct ether_addr)) == 0)
		dst_name = "(broadcast)";
	else if (e->ether_dhost.ether_addr_octet[0] & 1)
		dst_name = "(multicast)";

	ethertype = ntohs(e->ether_type);

	/*
	 * The 14 byte ether header screws up alignment
	 * of the rest of the packet for 32 bit aligned
	 * architectures like SPARC. Alas, we have to copy
	 * the rest of the packet in order to align it.
	 */
	len = elen - sizeof (struct etherheader);
	off = (char *) (e + 1);
	if (ethertype <= 1514) {
		/*
		 * Fake out the IEEE 802.3 packets.
		 * Should be DSAP=170, SSAP=170, control=3
		 * then three padding bytes of zero,
		 * followed by a normal ethernet-type packet.
		 */
		ieee8023 = ntohs(e->ether_type);
		off += 8;
		ethertype = *(u_short *) (off + 6);
	}
	memcpy(data, off, len);

	if (flags & F_SUM) {
		(void) sprintf(get_sum_line(),
			"ETHER Type=%04X (%s), size = %d bytes",
			ethertype,
			print_ethertype(ethertype),
			origlen);
	}

	if (flags & F_DTAIL) {
	show_header("ETHER:  ", "Ether Header", elen);
	show_space();
	(void) sprintf(get_line(0, 0),
		"Packet %d arrived at %d:%02d:%d.%02d",
		pi_frame,
		pi_time_hour, pi_time_min, pi_time_sec,
		pi_time_usec / 10000);
	(void) sprintf(get_line(0, 0),
		"Packet size = %d bytes",
		elen);
	(void) sprintf(get_line(0, 6),
		"Destination = %s, %s",
		printether(&e->ether_dhost),
		print_etherinfo(&e->ether_dhost));
	(void) sprintf(get_line(6, 6),
		"Source      = %s, %s",
		printether(&e->ether_shost),
		print_etherinfo(&e->ether_shost));
	if (ieee8023 > 0)
		(void) sprintf(get_line(12, 2),
			"IEEE 802.3 length = %d bytes",
			ieee8023);
	(void) sprintf(get_line(12, 2),
		"Ethertype = %04X (%s)",
		ethertype, print_ethertype(ethertype));
	show_space();
	}

	/* go to the next protocol layer */

	switch (ethertype) {
	case ETHERTYPE_IP:
		interpret_ip(flags, data, len);
		break;
	case ETHERTYPE_ARP:
	case ETHERTYPE_REVARP:
		interpret_arp(flags, data, len);
		break;
	default:
		break;
	}

	return;
}

u_int
loop_header_len(char *e)
{
	return 4;
}

u_int
ether_header_len(e)
char *e;
{
	return (14);
}


/*
 * Table of Ethertypes.
 * Some of the more popular entries
 * are at the beginning of the table
 * to reduce search time.
 */
struct ether_type {
	int   e_type;
	char *e_name;
} ether_type [] = {
0x0800, "IP",
0x0806, "ARP",
0x8035, "RARP",
/* end of popular entries */
0x0200, "Xerox PUP",
0x0201, "Xerox PUP",
0x0400, "Nixdorf",
0x0600, "Xerox NS IDP",
0x0601, "XNS Translation",
0x0801, "X.75 Internet",
0x0802, "NBS Internet",
0x0803, "ECMA Internet",
0x0804, "CHAOSnet",
0x0805, "X.25 Level 3",
0x0807, "XNS Compatibility",
0x081C, "Symbolics Private",
0x0888, "Xyplex",
0x0889, "Xyplex",
0x088A, "Xyplex",
0x0900, "Ungermann-Bass network debugger",
0x0A00, "Xerox IEEE802.3 PUP",
0x0A01, "Xerox IEEE802.3 PUP Address Translation",
0x0BAD, "Banyan Systems",
0x0BAF, "Banyon VINES Echo",
0x1000, "Berkeley Trailer negotiation",
0x1000,	"IP trailer (0)",
0x1001,	"IP trailer (1)",
0x1002,	"IP trailer (2)",
0x1003,	"IP trailer (3)",
0x1004,	"IP trailer (4)",
0x1005,	"IP trailer (5)",
0x1006,	"IP trailer (6)",
0x1007,	"IP trailer (7)",
0x1008,	"IP trailer (8)",
0x1009,	"IP trailer (9)",
0x100a,	"IP trailer (10)",
0x100b,	"IP trailer (11)",
0x100c,	"IP trailer (12)",
0x100d,	"IP trailer (13)",
0x100e,	"IP trailer (14)",
0x100f,	"IP trailer (15)",
0x1234, "DCA - Multicast",
0x1600, "VALID system protocol",
0x1989, "Aviator",
0x3C00, "3Com NBP virtual circuit datagram",
0x3C01, "3Com NBP System control datagram",
0x3C02, "3Com NBP Connect request (virtual cct)",
0x3C03, "3Com NBP Connect response",
0x3C04, "3Com NBP Connect complete",
0x3C05, "3Com NBP Close request (virtual cct)",
0x3C06, "3Com NBP Close response",
0x3C07, "3Com NBP Datagram (like XNS IDP)",
0x3C08, "3Com NBP Datagram broadcast",
0x3C09, "3Com NBP Claim NetBIOS name",
0x3C0A, "3Com NBP Delete Netbios name",
0x3C0B, "3Com NBP Remote adaptor status request",
0x3C0C, "3Com NBP Remote adaptor response",
0x3C0D, "3Com NBP Reset",
0x4242, "PCS Basic Block Protocol",
0x4321, "THD - Diddle",
0x5208, "BBN Simnet Private",
0x6000, "DEC unass, experimental",
0x6001, "DEC Dump/Load",
0x6002, "DEC Remote Console",
0x6003, "DECNET Phase IV, DNA Routing",
0x6004, "DEC LAT",
0x6005, "DEC Diagnostic",
0x6006, "DEC customer protocol",
0x6007, "DEC Local Area VAX Cluster (LAVC)",
0x6008, "DEC unass (AMBER?)",
0x6009, "DEC unass (MUMPS?)",
0x6010, "3Com",
0x6011, "3Com",
0x6012, "3Com",
0x6013, "3Com",
0x6014, "3Com",
0x7000, "Ungermann-Bass download",
0x7001, "Ungermann-Bass NIUs",
0x7002, "Ungermann-Bass diagnostic/loopback",
0x7003, "Ungermann-Bass ? (NMC to/from UB Bridge)",
0x7005, "Ungermann-Bass Bridge Spanning Tree",
0x7007, "OS/9 Microware",
0x7009, "OS/9 Net?",
0x7020, "Sintrom",
0x7021, "Sintrom",
0x7022, "Sintrom",
0x7023, "Sintrom",
0x7024, "Sintrom",
0x7025, "Sintrom",
0x7026, "Sintrom",
0x7027, "Sintrom",
0x7028, "Sintrom",
0x7029, "Sintrom",
0x8003, "Cronus VLN",
0x8004, "Cronus Direct",
0x8005, "HP Probe protocol",
0x8006, "Nestar",
0x8008, "AT&T/Stanford Univ",
0x8010, "Excelan",
0x8013, "SGI diagnostic",
0x8014, "SGI network games",
0x8015, "SGI reserved",
0x8016, "SGI XNS NameServer, bounce server",
0x8019, "Apollo DOMAIN",
0x802E, "Tymshare",
0x802F, "Tigan,",
0x8036, "Aeonic Systems",
0x8037, "IPX (Novell Netware)",
0x8038, "DEC LanBridge Management",
0x8039, "DEC unass (DSM/DTP?)",
0x803A, "DEC unass (Argonaut Console?)",
0x803B, "DEC unass (VAXELN?)",
0x803C, "DEC unass (NMSV? DNA Naming Service?)",
0x803D, "DEC Ethernet CSMA/CD Encryption Protocol",
0x803E, "DEC unass (DNA Time Service?)",
0x803F, "DEC LAN Traffic Monitor Protocol",
0x8040, "DEC unass (NetBios Emulator?)",
0x8041, "DEC unass (MS/DOS?, Local Area System Transport?)",
0x8042, "DEC unass",
0x8044, "Planning Research Corp.",
0x8046, "AT&T",
0x8047, "AT&T",
0x8049, "ExperData",
0x805B, "VMTP",
0x805C, "Stanford V Kernel, version 6.0",
0x805D, "Evans & Sutherland",
0x8060, "Little Machines",
0x8062, "Counterpoint",
0x8065, "University of Mass. at Amherst",
0x8066, "University of Mass. at Amherst",
0x8067, "Veeco Integrated Automation",
0x8068, "General Dynamics",
0x8069, "AT&T",
0x806A, "Autophon",
0x806C, "ComDesign",
0x806D, "Compugraphic Corp",
0x806E, "Landmark",
0x806F, "Landmark",
0x8070, "Landmark",
0x8071, "Landmark",
0x8072, "Landmark",
0x8073, "Landmark",
0x8074, "Landmark",
0x8075, "Landmark",
0x8076, "Landmark",
0x8077, "Landmark",
0x807A, "Matra",
0x807B, "Dansk Data Elektronik",
0x807C, "Merit Internodal",
0x807D, "Vitalink",
0x807E, "Vitalink",
0x807F, "Vitalink",
0x8080, "Vitalink TransLAN III Management",
0x8081, "Counterpoint",
0x8082, "Counterpoint",
0x8083, "Counterpoint",
0x8088, "Xyplex",
0x8089, "Xyplex",
0x808A, "Xyplex",
0x809B, "EtherTalk (AppleTalk over Ethernet)",
0x809C, "Datability",
0x809D, "Datability",
0x809E, "Datability",
0x809F, "Spider Systems",
0x80A3, "Nixdorf",
0x80A4, "Siemens Gammasonics",
0x80C0, "DCA Data Exchange Cluster",
0x80C6, "Pacer Software",
0x80C7, "Applitek Corp",
0x80C8, "Intergraph",
0x80C9, "Intergraph",
0x80CB, "Intergraph",
0x80CC, "Intergraph",
0x80CA, "Intergraph",
0x80CD, "Harris Corp",
0x80CE, "Harris Corp",
0x80CF, "Taylor Instrument",
0x80D0, "Taylor Instrument",
0x80D1, "Taylor Instrument",
0x80D2, "Taylor Instrument",
0x80D3, "Rosemount Corp",
0x80D4, "Rosemount Corp",
0x80D5, "IBM SNA Services over Ethernet",
0x80DD, "Varian Associates",
0x80DE, "TRFS",
0x80DF, "TRFS",
0x80E0, "Allen-Bradley",
0x80E1, "Allen-Bradley",
0x80E2, "Allen-Bradley",
0x80E3, "Allen-Bradley",
0x80E4, "Datability",
0x80F2, "Retix",
0x80F3, "AARP (Appletalk)",
0x80F4, "Kinetics",
0x80F5, "Kinetics",
0x80F7, "Apollo",
0x80FF, "Wellfleet Communications",
0x8102, "Wellfleet Communications",
0x8107, "Symbolics Private",
0x8108, "Symbolics Private",
0x8109, "Symbolics Private",
0x812B, "Talaris",
0x8130, "Waterloo",
0x8131, "VG Lab",
0x8137, "Novell (old) NetWare IPX",
0x8138, "Novell",
0x814C, "SNMP over Ethernet",
0x817D, "XTP",
0x81D6, "Lantastic",
0x8888, "HP LanProbe test?",
0x9000, "Loopback",
0x9001, "3Com, XNS Systems Management",
0x9002, "3Com, TCP/IP Systems Management",
0x9003, "3Com, loopback detection",
0xAAAA, "DECNET	(VAX 6220 DEBNI)",
0xFF00, "BBN VITAL-LanBridge cache wakeups",
0,	"",
};

char *
print_fc(type)
u_int type;
{

	switch (type) {
		case 0x50: return ("LLC");
		case 0x4f: return ("SMT NSA");
		case 0x41: return ("SMT Info");
		default: return ("Unknown");
	}
}

char *
print_smtclass(type)
u_int type;
{
	switch (type) {
		case 0x01: return ("NIF");
		case 0x02: return ("SIF Conf");
		case 0x03: return ("SIF Oper");
		case 0x04: return ("ECF");
		case 0x05: return ("RAF");
		case 0x06: return ("RDF");
		case 0x07: return ("SRF");
		case 0x08: return ("PMF Get");
		case 0x09: return ("PMF Change");
		case 0x0a: return ("PMF Add");
		case 0x0b: return ("PMF Remove");
		case 0xff: return ("ESF");
		default: return ("Unknown");
	}

}
char *
print_smttype(type)
u_int type;
{
	switch (type) {
		case 0x01: return ("Announce");
		case 0x02: return ("Request");
		case 0x03: return ("Response");
		default: return ("Unknown");
	}

}
char *
print_ethertype(type)
	int type;
{
	int i;

	for (i = 0; ether_type[i].e_type; i++)
		if (type == ether_type[i].e_type)
			return (ether_type[i].e_name);
	if (type < 1500)
		return ("LLC/802.3");

	return ("Unknown");
}

#define	MAX_RDFLDS	14		/* changed to 14 from 8 as per IEEE */
#define	TR_FN_ADDR	0x80		/* dest addr is functional */
#define	TR_SR_ADDR	0x80		/* MAC utilizes source route */
#define	ACFCDASA_LEN	14		/* length of AC|FC|DA|SA */
#define	TR_MAC_MASK	0xc0
#define	TR_AC		0x00		/* Token Ring access control */
#define	TR_LLC_FC	0x40		/* Token Ring llc frame control */
#define	LSAP_SNAP	0xaa
#define	LLC_SNAP_HDR_LEN	8
#define	LLC_HDR1_LEN	3		/* DON'T use sizeof(struct llc_hdr1) */
#define	CNTL_LLC_UI	0x03		/* un-numbered information packet */

/*
 * Source Routing Route Information field.
 */
struct tr_ri {
	u_char  rt:3;			/* routing type */
	u_char  len:5;			/* length */
	u_char  dir:1;			/* direction bit */
	u_char  mtu:3;			/* largest frame */
	u_char  res:4;			/* reserved */
	struct tr_rd {			/* route designator fields */
		u_short ring:12;
		u_short bridge:4;
	} rd[MAX_RDFLDS];
};

struct tr_header {
	u_char		ac;
	u_char		fc;
	struct ether_addr dhost;
	struct ether_addr shost;
	struct tr_ri	ri;
};

struct llc_snap_hdr {
	u_char  d_lsap;			/* destination service access point */
	u_char  s_lsap;			/* source link service access point */
	u_char  control;		/* short control field */
	u_char  org[3];			/* Ethernet style organization field */
	u_short type;			/* Ethernet style type field */
};

struct ether_addr tokenbroadcastaddr2 = {
	0xc0, 0x00, 0xff, 0xff, 0xff, 0xff
};

int Mtutab[] = {516, 1470, 2052, 4472, 8144, 11407, 17800};

char *
print_sr(struct tr_ri *rh)
{
	int hops, ii;
	static char line[512];

	sprintf(line, "TR Source Route dir=%d, mtu=%d",
			rh->dir, Mtutab[rh->mtu]);

	hops = (int) (rh->len - 2) / (int)2;

	if (hops) {
		sprintf(line+strlen(line), ", Route: ");
		for (ii = 0; ii < hops; ii++)
			if (!rh->rd[ii].bridge)
				sprintf(line+strlen(line), "(%d)",
						rh->rd[ii].ring);
			else
				sprintf(line+strlen(line), "(%d)%d",
						rh->rd[ii].ring,
						rh->rd[ii].bridge);
	}
	return (&line[0]);
}

void
interpret_tr(flags, e, elen, origlen)
	int flags;
	caddr_t	e;
	int elen, origlen;
{
	struct tr_header *mh;
	struct tr_ri *rh;
	u_char fc;
	struct llc_snap_hdr *snaphdr;
	char *off;
	int maclen, len;
	int ieee8023 = 0;
	char data[17800];
	extern char *dst_name, *src_name;
	int ethertype;
	int is_llc = 0, is_snap = 0, source_routing = 0;
	int tr_machdr_len(char *, int *, int *);

	if (origlen < ACFCDASA_LEN) {
		if (flags & F_SUM)
			(void) sprintf(get_sum_line(),
			"RUNT (short packet - %d bytes)",
			origlen);
		if (flags & F_DTAIL)
			show_header("RUNT:  ", "Short packet", origlen);
		return;
	}
	if (elen < ACFCDASA_LEN)
		return;

	mh = (struct tr_header *) e;
	rh = (struct tr_ri *) &mh->ri;
	fc = mh->fc;

	if (is_llc = tr_machdr_len(e, &maclen, &source_routing)) {
		snaphdr = (struct llc_snap_hdr *) (e + maclen);
		if (snaphdr->d_lsap == LSAP_SNAP &&
			snaphdr->s_lsap == LSAP_SNAP &&
			snaphdr->control == CNTL_LLC_UI) {
			is_snap = 1;
		}
	}

	if (memcmp((char *) &mh->dhost, (char *)&ether_broadcast,
		sizeof (struct ether_addr)) == 0)
		dst_name = "(broadcast)";
	else if (memcmp((char *) &mh->dhost, (char *)&tokenbroadcastaddr2,
		sizeof (struct ether_addr)) == 0)
		dst_name = "(mac broadcast)";
	else if (mh->dhost.ether_addr_octet[0] & TR_FN_ADDR)
		dst_name = "(functional)";

	if (is_snap)
		ethertype = ntohs(snaphdr->type);
	else {
		src_name =  print_etherinfo(&mh->shost);
		dst_name =  print_etherinfo(&mh->dhost);
	}

	/*
	 * The 14 byte ether header screws up alignment
	 * of the rest of the packet for 32 bit aligned
	 * architectures like SPARC. Alas, we have to copy
	 * the rest of the packet in order to align it.
	 */
	if (is_llc) {
		if (is_snap) {
			len = elen - (maclen + LLC_SNAP_HDR_LEN);
			off = (char *) (e + maclen + LLC_SNAP_HDR_LEN);
		} else {
			len = elen - (maclen + LLC_HDR1_LEN);
			off = (char *) (e + maclen + LLC_HDR1_LEN);
		}
	} else {
		len = elen - maclen;
		off = (char *) (e + maclen);
	}

	memcpy(data, off, len);

	if (flags & F_SUM) {
		if (source_routing)
			sprintf(get_sum_line(), print_sr(rh));

		if (is_llc) {
			if (is_snap) {
				(void) sprintf(get_sum_line(),
				"TR LLC w/SNAP Type=%04X (%s), size=%d bytes",
				ethertype,
				print_ethertype(ethertype),
				origlen);
			} else {
				(void) sprintf(get_sum_line(),
				"TR LLC, but no SNAP encoding, size = %d bytes",
				origlen);
			}
		} else {
			(void) sprintf(get_sum_line(),
				"TR MAC FC=%02X (%s), size = %d bytes",
				fc, print_fc(fc), origlen);
		}
	}

	if (flags & F_DTAIL) {
	show_header("TR:  ", "TR Header", elen);
	show_space();
	(void) sprintf(get_line(0, 0),
		"Packet %d arrived at %d:%02d:%d.%02d",
		pi_frame,
		pi_time_hour, pi_time_min, pi_time_sec,
		pi_time_usec / 10000);
	(void) sprintf(get_line(0, 0),
		"Packet size = %d bytes",
		elen);
	(void) sprintf(get_line(0, 1),
		"Frame Control = %02x (%s)",
		fc, print_fc(fc));
	(void) sprintf(get_line(2, 6),
		"Destination = %s, %s",
		printether(&mh->dhost),
		print_etherinfo(&mh->dhost));
	(void) sprintf(get_line(8, 6),
		"Source      = %s, %s",
		printether(&mh->shost),
		print_etherinfo(&mh->shost));

	if (source_routing)
		sprintf(get_line(ACFCDASA_LEN, rh->len), print_sr(rh));

	if (is_llc) {
		(void) sprintf(get_line(maclen, 1),
			"Dest   Service Access Point = %02x",
			snaphdr->d_lsap);
		(void) sprintf(get_line(maclen+1, 1),
			"Source Service Access Point = %02x",
			snaphdr->s_lsap);
		(void) sprintf(get_line(maclen+2, 1),
			"Control = %02x",
			snaphdr->control);
		if (is_snap)
			(void) sprintf(get_line(maclen+3, 3),
				"SNAP Protocol Id = %02x%02x%02x",
				snaphdr->org[0], snaphdr->org[1],
				snaphdr->org[2]);
	}

	if (is_snap)
		(void) sprintf(get_line(maclen+6, 2),
		"SNAP Type = %04X (%s)",
		ethertype, print_ethertype(ethertype));

	show_space();
	}

	/* go to the next protocol layer */
	if (is_snap) {

	switch (ethertype) {
	case ETHERTYPE_IP:
		interpret_ip(flags, data, len);
		break;
	case ETHERTYPE_ARP:
	case ETHERTYPE_REVARP:
		interpret_arp(flags, data, len);
		break;
	default:
		break;
	}

	} /* is_llc */

	return;
}

void
interpret_loop(flags, e, elen, origlen)
	int flags;
	caddr_t	e;
	int elen, origlen;
{
	struct loopheader *lo = (struct loopheader *)e;
	int len = elen - sizeof(*lo);
	caddr_t data = e + sizeof(*lo);

	if (lo->loop_family == AF_INET) {
		interpret_ip(flags, data, len);
	}

	return;
}


/*
	stuffs length of mac and ri fields into *lenp
	returns:
		0: mac frame
		1: llc frame
*/
int
tr_machdr_len(char *e, int *lenp, int *source_routing)
{
	struct tr_header *mh;
	struct tr_ri *rh;
	u_char fc;

	mh = (struct tr_header *) e;
	rh = (struct tr_ri *) &mh->ri;
	fc = mh->fc;

	if (mh->shost.ether_addr_octet[0] & TR_SR_ADDR) {
		*lenp = ACFCDASA_LEN + rh->len;
		*source_routing = 1;
	} else {
		*lenp = ACFCDASA_LEN;
		*source_routing = 0;
	}

	if ((fc & TR_MAC_MASK) == 0)
		return (0);		/* it's a MAC frame */
	else
		return (1);		/* it's an LLC frame */
}

u_int
tr_header_len(e)
char	*e;
{
	struct llc_snap_hdr *snaphdr;
	int len = 0, source_routing;

	if (tr_machdr_len(e, &len, &source_routing) == 0)
		return (len);		/* it's a MAC frame */

	snaphdr = (struct llc_snap_hdr *) (e + len);
	if (snaphdr->d_lsap == LSAP_SNAP &&
			snaphdr->s_lsap == LSAP_SNAP &&
			snaphdr->control == CNTL_LLC_UI)
		len += LLC_SNAP_HDR_LEN;	/* it's a SNAP frame */
	else
		len += LLC_HDR1_LEN;

	return (len);
}

struct fddi_header {
	u_char fc;
	struct ether_addr dhost, shost;
	u_char dsap, ssap, ctl, proto_id[3];
	u_short	type;
};

void
interpret_fddi(flags, e, elen, origlen)
	int flags;
	caddr_t	e;
	int elen, origlen;
{
	struct fddi_header fhdr, *f = &fhdr;
	char *off;
	int len;
	int ieee8023 = 0;
	char data[4500];
	extern char *dst_name, *src_name;
	int ethertype;
	int is_llc = 0, is_smt = 0, is_snap = 0;

	if (origlen < 13) {
		if (flags & F_SUM)
			(void) sprintf(get_sum_line(),
			"RUNT (short packet - %d bytes)",
			origlen);
		if (flags & F_DTAIL)
			show_header("RUNT:  ", "Short packet", origlen);
		return;
	}
	if (elen < 13)
		return;

	memcpy(&f->fc, e, sizeof (f->fc));
	addr_copy_swap(&f->dhost, (struct ether_addr*) (e+1));
	addr_copy_swap(&f->shost, (struct ether_addr*) (e+7));

	if ((f->fc&0x50) == 0x50) {
		is_llc = 1;
		memcpy(&f->dsap, e+13, sizeof (f->dsap));
		memcpy(&f->ssap, e+14, sizeof (f->ssap));
		memcpy(&f->ctl, e+15, sizeof (f->ctl));
		if (f->dsap == 0xaa && f->ssap == 0xaa) {
			is_snap = 1;
			memcpy(&f->proto_id, e+16, sizeof (f->proto_id));
			memcpy(&f->type, e+19, sizeof (f->type));
		}
	} else {
		if ((f->fc&0x41) == 0x41 || (f->fc&0x4f) == 0x4f) {
			is_smt = 1;
		}
	}


	if (memcmp((char *) &f->dhost, (char *)&ether_broadcast,
		sizeof (struct ether_addr)) == 0)
		dst_name = "(broadcast)";
	else if (f->dhost.ether_addr_octet[0] & 0x01)
		dst_name = "(multicast)";

	if (is_snap)
		ethertype = ntohs(f->type);
	else {
		src_name = 	print_etherinfo(&f->shost);
		dst_name =  print_etherinfo(&f->dhost);
	}

	/*
	 * The 14 byte ether header screws up alignment
	 * of the rest of the packet for 32 bit aligned
	 * architectures like SPARC. Alas, we have to copy
	 * the rest of the packet in order to align it.
	 */
	if (is_llc) {
		if (is_snap) {
			len = elen - 21;
			off = (char *) (e + 21);
		} else {
			len = elen - 16;
			off = (char *) (e + 16);
		}
	} else {
		len = elen - 13;
		off = (char *) (e + 13);
	}

	memcpy(data, off, len);

	if (flags & F_SUM) {
		if (is_llc) {
			if (is_snap) {
				(void) sprintf(get_sum_line(),
				"FDDI LLC Type=%04X (%s), size = %d bytes",
				ethertype,
				print_ethertype(ethertype),
				origlen);
			} else {
				(void) sprintf(get_sum_line(),
				"LLC, but no SNAP encoding, size = %d bytes",
				origlen);
			}
		} else if (is_smt) {
			(void) sprintf(get_sum_line(),
		"SMT Type=%02X (%s), Class = %02X (%s), size = %d bytes",
			*(u_char*)(data+1), print_smttype(*(data+1)), *data,
			print_smtclass(*data), origlen);
		} else {
			(void) sprintf(get_sum_line(),
				"FC=%02X (%s), size = %d bytes",
				f->fc, print_fc(f->fc), origlen);
		}
	}

	if (flags & F_DTAIL) {
	show_header("FDDI:  ", "FDDI Header", elen);
	show_space();
	(void) sprintf(get_line(0, 0),
		"Packet %d arrived at %d:%02d:%d.%02d",
		pi_frame,
		pi_time_hour, pi_time_min, pi_time_sec,
		pi_time_usec / 10000);
	(void) sprintf(get_line(0, 0),
		"Packet size = %d bytes",
		elen);
	(void) sprintf(get_line(0, 6),
		"Destination = %s, %s",
		printether(&f->dhost),
		print_etherinfo(&f->dhost));
	(void) sprintf(get_line(6, 6),
		"Source      = %s, %s",
		printether(&f->shost),
		print_etherinfo(&f->shost));

	if (is_llc) {
		(void) sprintf(get_line(12, 2),
			"Frame Control = %02x (%s)",
			f->fc, print_fc(f->fc));
		(void) sprintf(get_line(12, 2),
			"Dest   Service Access Point = %02x",
			f->dsap);
		(void) sprintf(get_line(12, 2),
			"Source Service Access Point = %02x",
			f->ssap);
		(void) sprintf(get_line(12, 2),
			"Control = %02x",
			f->ctl);
		if (is_snap)
			(void) sprintf(get_line(12, 2),
				"Protocol Id = %02x%02x%02x",
				f->proto_id[0], f->proto_id[1], f->proto_id[2]);
	} else if (is_smt) {
		(void) sprintf(get_line(12, 2),
			"Frame Control = %02x (%s)",
			f->fc, print_fc(f->fc));
		(void) sprintf(get_line(12, 2),
			"Class = %02x (%s)",
			(u_char)*data, print_smtclass(*data));
		(void) sprintf(get_line(12, 2),
			"Type = %02x (%s)",
			*(u_char*)(data+1), print_smttype(*(data+1)));
	} else {
		(void) sprintf(get_line(12, 2),
			"FC=%02X (%s), size = %d bytes",
			f->fc, print_fc(f->fc), origlen);
	}

	if (is_snap)
		(void) sprintf(get_line(12, 2),
		"LLC Type = %04X (%s)",
		ethertype, print_ethertype(ethertype));

	show_space();
	}

	/* go to the next protocol layer */
	if (is_llc && is_snap && f->ctl == 0x03) {

	switch (ethertype) {
	case ETHERTYPE_IP:
		interpret_ip(flags, data, len);
		break;
	case ETHERTYPE_ARP:
	case ETHERTYPE_REVARP:
		interpret_arp(flags, data, len);
		break;
	default:
		break;
	}

	} /* is_llc */

	return;
}

u_int
fddi_header_len(e)
char	*e;
{
	struct fddi_header fhdr, *f = &fhdr;

	memcpy(&f->fc, e, sizeof (f->fc));
	memcpy(&f->dhost, e+1, sizeof (struct ether_addr));
	memcpy(&f->shost, e+7, sizeof (struct ether_addr));

	if ((f->fc&0x50) == 0x50) {
		memcpy(&f->dsap, e+13, sizeof (f->dsap));
		memcpy(&f->ssap, e+14, sizeof (f->ssap));
		memcpy(&f->ctl, e+15, sizeof (f->ctl));
		if (f->dsap == 0xaa && f->ssap == 0xaa) {
			return (21);
		}
		return (16);
	} else {
		if ((f->fc&0x41) == 0x41 || (f->fc&0x4f) == 0x4f) {
			return (13);
		}
	}
	return (0);
}

/*
 * Print the given Ethernet address
 */
char *
printether(p)
	struct ether_addr *p;
{
	static char buf[256];

	sprintf(buf, "%x:%x:%x:%x:%x:%x",
		p->ether_addr_octet[0],
		p->ether_addr_octet[1],
		p->ether_addr_octet[2],
		p->ether_addr_octet[3],
		p->ether_addr_octet[4],
		p->ether_addr_octet[5]);

	return (buf);
}

/*
 * Table of Ethernet Address Assignments
 * Some of the more popular entries
 * are at the beginning of the table
 * to reduce search time
 */
struct block_type {
	int	e_block;
	char	*e_name;
} ether_block [] = {
0x080020,	"Sun",
0x0000C6,	"HP",
0x08002B,	"DEC",
0x00000F,	"NeXT",
0x00000C,	"Cisco",
0x080069,	"SGI",
0x000069,	"SGI",
0x0000A7,	"Network Computing Devices (NCD	X-terminal)",
0x08005A,	"IBM",
0x0000AC,	"Apollo",
/* end of popular entries */
0x000002,	"BBN",
0x000010,	"Sytek",
0x000011,	"Tektronix",
0x000018,	"Webster (?)",
0x00001B,	"Novell",
0x00001D,	"Cabletron",
0x000020,	"DIAB (Data Intdustrier AB)",
0x000021,	"SC&C",
0x000022,	"Visual Technology",
0x000029,	"IMC",
0x00002A,	"TRW",
0x00003D,	"AT&T",
0x000049,	"Apricot Ltd.",
0x000055,	"AT&T",
0x00005A,	"S & Koch",
0x00005A,	"Xerox 806 (unregistered)",
0x00005E,	"U.S. Department of Defense (IANA)",
0x000065,	"Network General",
0x00006B,	"MIPS",
0x000077,	"MIPS",
0x000079,	"NetWare (?)",
0x00007A,	"Ardent",
0x00007B,	"Research Machines",
0x00007D,	"Harris (3M) (old)",
0x000080,	"Imagen(?)",
0x000081,	"Synoptics",
0x000084,	"Aquila (?)",
0x000086,	"Gateway (?)",
0x000089,	"Cayman Systems	Gatorbox",
0x000093,	"Proteon",
0x000094,	"Asante",
0x000098,	"Cross Com",
0x00009F,	"Ameristar Technology",
0x0000A2,	"Wellfleet",
0x0000A3,	"Network Application Technology",
0x0000A4,	"Acorn",
0x0000A6,	"Network General",
0x0000A7,	"Network Computing Devices (NCD	X-terminal)",
0x0000A9,	"Network Systems",
0x0000AA,	"Xerox",
0x0000B3,	"CIMLinc",
0x0000B5,	"Datability Terminal Server",
0x0000B7,	"Dove Fastnet",
0x0000BC,	"Allen-Bradley",
0x0000C0,	"Western Digital",
0x0000C8,	"Altos",
0x0000C9,	"Emulex Terminal Server",
0x0000D0,	"Develcon Electronics, Ltd.",
0x0000D1,	"Adaptec Inc. Nodem product",
0x0000D7,	"Dartmouth College (NED Router)",
0x0000DD,	"Gould",
0x0000DE,	"Unigraph",
0x0000E2,	"Acer Counterpoint",
0x0000E8,	"Accton Technology Corporation",
0x0000EE,	"Network Designers Limited(?)",
0x0000EF,	"Alantec",
0x0000F3,	"Gandalf",
0x0000FD,	"High Level Hardware (Orion, UK)",
0x000143,	"IEEE 802",
0x001700,	"Kabel",
0x00608C,	"3Com",
0x00800F,	"SMC",
0x008019,	"Dayna Communications Etherprint product",
0x00802D,	"Xylogics, Inc.	Annex terminal servers",
0x008035,	"Technology Works",
0x008087,	"Okidata",
0x00808C,	"Frontier Software Development",
0x0080C7,	"Xircom Inc.",
0x0080D0,	"Computer Products International",
0x0080D3,	"Shiva Appletalk-Ethernet interface",
0x0080D4,	"Chase Limited",
0x0080F1,	"Opus",
0x00AA00,	"Intel",
0x00B0D0,	"Computer Products International",
0x00DD00,	"Ungermann-Bass",
0x00DD01,	"Ungermann-Bass",
0x00EFE5,	"IBM (3Com card)",
0x020406,	"BBN",
0x026060,	"3Com",
0x026086,	"Satelcom MegaPac (UK)",
0x02E6D3,	"Bus-Tech, Inc. (BTI)",
0x080001,	"Computer Vision",
0x080002,	"3Com (Formerly Bridge)",
0x080003,	"ACC (Advanced Computer Communications)",
0x080005,	"Symbolics",
0x080007,	"Apple",
0x080008,	"BBN",
0x080009,	"Hewlett-Packard",
0x08000A,	"Nestar Systems",
0x08000B,	"Unisys",
0x08000D,	"ICL",
0x08000E,	"NCR",
0x080010,	"AT&T",
0x080011,	"Tektronix, Inc.",
0x080017,	"NSC",
0x08001A,	"Data General",
0x08001B,	"Data General",
0x08001E,	"Apollo",
0x080022,	"NBI",
0x080025,	"CDC",
0x080026,	"Norsk Data (Nord)",
0x080027,	"PCS Computer Systems GmbH",
0x080028,	"TI Explorer",
0x08002E,	"Metaphor",
0x08002F,	"Prime Computer",
0x080036,	"Intergraph CAE stations",
0x080037,	"Fujitsu-Xerox",
0x080038,	"Bull",
0x080039,	"Spider Systems",
0x08003B,	"Torus Systems",
0x08003E,	"Motorola VME bus processor module",
0x080041,	"DCA Digital Comm. Assoc.",
0x080046,	"Sony",
0x080047,	"Sequent",
0x080049,	"Univation",
0x08004C,	"Encore",
0x08004E,	"BICC",
0x080056,	"Stanford University",
0x080057,	"Evans & Sutherland (?)",
0x080067,	"Comdesign",
0x080068,	"Ridge",
0x08006A,	"ATTst (?)",
0x08006E,	"Excelan",
0x080075,	"DDE (Danish Data Elektronik A/S)",
0x080077,	"TSL (now Retix)",
0x08007C,	"Vitalink TransLAN III",
0x080080,	"XIOS",
0x080081,	"Crosfield Electronics",
0x080086,	"Imagen/QMS",
0x080087,	"Xyplex	terminal server",
0x080089,	"Kinetics AppleTalk-Ethernet interface",
0x08008B,	"Pyramid",
0x08008D,	"XyVision",
0x080090,	"Retix Inc Bridge",
0x10005A,	"IBM",
0x1000D4,	"DEC",
0x400003,	"NetWare",
0x800010,	"AT&T",
0xAA0004,	"DEC (DECNET)",
0xC00000,	"SMC",
0,		"",
};

/*
 * Print the additional Ethernet address info
 */
char *
print_etherinfo(eaddr)
	struct ether_addr *eaddr;
{
	u_int addr = 0;
	char *p = (char *)&addr + 1;
	int i;


	memcpy(p, (char *) eaddr, 3);

	if (memcmp((char *)eaddr, (char *)&ether_broadcast,
		sizeof (struct ether_addr)) == 0)
		return ("(broadcast)");

	if (eaddr->ether_addr_octet[0] & 1)
		return ("(multicast)");

	addr = ntohl(addr);	/* make it right for little-endians */

	for (i = 0; ether_block[i].e_block; i++)
		if (addr == ether_block[i].e_block)
			return (ether_block[i].e_name);

	return ("");
}

static u_char	endianswap[] = {
	0x00, 0x80, 0x40, 0xc0, 0x20, 0xa0, 0x60, 0xe0,
	0x10, 0x90, 0x50, 0xd0, 0x30, 0xb0, 0x70, 0xf0,
	0x08, 0x88, 0x48, 0xc8, 0x28, 0xa8, 0x68, 0xe8,
	0x18, 0x98, 0x58, 0xd8, 0x38, 0xb8, 0x78, 0xf8,
	0x04, 0x84, 0x44, 0xc4, 0x24, 0xa4, 0x64, 0xe4,
	0x14, 0x94, 0x54, 0xd4, 0x34, 0xb4, 0x74, 0xf4,
	0x0c, 0x8c, 0x4c, 0xcc, 0x2c, 0xac, 0x6c, 0xec,
	0x1c, 0x9c, 0x5c, 0xdc, 0x3c, 0xbc, 0x7c, 0xfc,
	0x02, 0x82, 0x42, 0xc2, 0x22, 0xa2, 0x62, 0xe2,
	0x12, 0x92, 0x52, 0xd2, 0x32, 0xb2, 0x72, 0xf2,
	0x0a, 0x8a, 0x4a, 0xca, 0x2a, 0xaa, 0x6a, 0xea,
	0x1a, 0x9a, 0x5a, 0xda, 0x3a, 0xba, 0x7a, 0xfa,
	0x06, 0x86, 0x46, 0xc6, 0x26, 0xa6, 0x66, 0xe6,
	0x16, 0x96, 0x56, 0xd6, 0x36, 0xb6, 0x76, 0xf6,
	0x0e, 0x8e, 0x4e, 0xce, 0x2e, 0xae, 0x6e, 0xee,
	0x1e, 0x9e, 0x5e, 0xde, 0x3e, 0xbe, 0x7e, 0xfe,
	0x01, 0x81, 0x41, 0xc1, 0x21, 0xa1, 0x61, 0xe1,
	0x11, 0x91, 0x51, 0xd1, 0x31, 0xb1, 0x71, 0xf1,
	0x09, 0x89, 0x49, 0xc9, 0x29, 0xa9, 0x69, 0xe9,
	0x19, 0x99, 0x59, 0xd9, 0x39, 0xb9, 0x79, 0xf9,
	0x05, 0x85, 0x45, 0xc5, 0x25, 0xa5, 0x65, 0xe5,
	0x15, 0x95, 0x55, 0xd5, 0x35, 0xb5, 0x75, 0xf5,
	0x0d, 0x8d, 0x4d, 0xcd, 0x2d, 0xad, 0x6d, 0xed,
	0x1d, 0x9d, 0x5d, 0xdd, 0x3d, 0xbd, 0x7d, 0xfd,
	0x03, 0x83, 0x43, 0xc3, 0x23, 0xa3, 0x63, 0xe3,
	0x13, 0x93, 0x53, 0xd3, 0x33, 0xb3, 0x73, 0xf3,
	0x0b, 0x8b, 0x4b, 0xcb, 0x2b, 0xab, 0x6b, 0xeb,
	0x1b, 0x9b, 0x5b, 0xdb, 0x3b, 0xbb, 0x7b, 0xfb,
	0x07, 0x87, 0x47, 0xc7, 0x27, 0xa7, 0x67, 0xe7,
	0x17, 0x97, 0x57, 0xd7, 0x37, 0xb7, 0x77, 0xf7,
	0x0f, 0x8f, 0x4f, 0xcf, 0x2f, 0xaf, 0x6f, 0xef,
	0x1f, 0x9f, 0x5f, 0xdf, 0x3f, 0xbf, 0x7f, 0xff,
};

void
addr_copy_swap(pd, ps)
	struct ether_addr	*pd;
	struct ether_addr	*ps;
{
	pd->ether_addr_octet[0] = endianswap[ps->ether_addr_octet[0]];
	pd->ether_addr_octet[1] = endianswap[ps->ether_addr_octet[1]];
	pd->ether_addr_octet[2] = endianswap[ps->ether_addr_octet[2]];
	pd->ether_addr_octet[3] = endianswap[ps->ether_addr_octet[3]];
	pd->ether_addr_octet[4] = endianswap[ps->ether_addr_octet[4]];
	pd->ether_addr_octet[5] = endianswap[ps->ether_addr_octet[5]];
}
