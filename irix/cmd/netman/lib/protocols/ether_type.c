/*
 * Copyright 1989 Silicon Graphics, Inc.  All rights reserved.
 *
 * Ethernet typecode description table.
 */
#include "debug.h"
#include "heap.h"
#include "index.h"

static struct typeinfo {
	unsigned short	code;		/* Ethernet type code */
	unsigned short	highcode;	/* if !0, last code in dense range */
	char		*desc;		/* descriptive text, not too long */
} typeinfo[] = {
	{ 0x0600, 0,		"Xerox NS IDP" },
	{ 0x0800, 0,		"DoD Internet Protocol" },
	{ 0x0801, 0,		"X.75 Internet" },
	{ 0x0802, 0,		"NBS Internet" },
	{ 0x0803, 0,		"ECMA Internet" },
	{ 0x0804, 0,		"CHAOSnet" },
	{ 0x0805, 0,		"X.25 Level 3" },
	{ 0x0806, 0,		"Address Resolution Protocol (ARP)" },
	{ 0x0807, 0,		"XNS Compatibility" },
	{ 0x081C, 0,		"Symbolics Private" },
	{ 0x0888, 0x088A,	"Xyplex" },
	{ 0x0900, 0,		"Ungermann-Bass network debugger" },
	{ 0x0A00, 0,		"Xerox IEEE802.3 PUP" },
	{ 0x0A01, 0,		"Xerox IEEE802.3 PUP Address Translation" },
	{ 0x0BAD, 0,		"Banyan Systems" },
	{ 0x1000, 0,		"Berkeley Trailer negotiation" },
	{ 0x1001, 0x100F,	"Berkeley Trailer encapsulation for IP" },
	{ 0x1600, 0,		"VALID system protocol" },
	{ 0x5208, 0,		"BBN Simnet Private" },
	{ 0x6000, 0,		"DEC unassigned" },
	{ 0x6001, 0,		"DEC Maintenance Operation Protocol (MOP) Dump/Load Assistance" },
	{ 0x6002, 0,		"DEC Maintenance Operation Protocol (MOP) Remote Console" },
	{ 0x6003, 0,		"DECNET Phase IV" },
	{ 0x6004, 0,		"DEC Local Area Transport (LAT)" },
	{ 0x6005, 0,		"DEC diagnostic protocol" },
	{ 0x6006, 0,		"DEC customer protocol" },
	{ 0x6007, 0,		"DEC Local Area VAX Cluster (LAVC)" },
	{ 0x6008, 0x6009,	"DEC unassigned" },
	{ 0x6010, 0x6014,	"3Com Corporation" },
	{ 0x7000, 0,		"Ungermann-Bass download" },
	{ 0x7002, 0,		"Ungermann-Bass diagnostic/loopback" },
	{ 0x7020, 0x7029,	"LRT" },
	{ 0x8003, 0,		"Cronus VLN" },
	{ 0x8004, 0,		"Cronus Direct" },
	{ 0x8005, 0,		"HP Probe protocol" },
	{ 0x8006, 0,		"Nestar" },
	{ 0x8008, 0,		"AT&T" },
	{ 0x8010, 0,		"Excelan" },
	{ 0x8013, 0,		"Silicon Graphics diagnostic" },
	{ 0x8014, 0,		"Silicon Graphics network games" },
	{ 0x8015, 0,		"Silicon Graphics reserved" },
	{ 0x8016, 0,		"Silicon Graphics XNS NameServer, bounce server" },
	{ 0x8019, 0,		"Apollo DOMAIN" },
	{ 0x802E, 0,		"Tymshare" },
	{ 0x802F, 0,		"Tigan, Inc." },
	{ 0x8035, 0,		"Reverse Address Resolution Protocol (RARP)" },
	{ 0x8036, 0,		"Aeonic Systems" },
	{ 0x8038, 0,		"DEC LanBridge Management" },
	{ 0x8039, 0x803C,	"DEC unassigned" },
	{ 0x803D, 0,		"DEC Ethernet Encryption Protocol" },
	{ 0x803E, 0,		"DEC unassigned" },
	{ 0x803F, 0,		"DEC LAN Traffic Monitor Protocol" },
	{ 0x8040, 0x8042,	"DEC unassigned" },
	{ 0x8044, 0,		"Planning Research Corp." },
	{ 0x8046, 0x8047,	"AT&T" },
	{ 0x8049, 0,		"ExperData" },
	{ 0x805B, 0,		"Stanford V Kernel, experimental" },
	{ 0x805C, 0,		"Stanford V Kernel, production" },
	{ 0x805D, 0,		"Evans & Sutherland" },
	{ 0x8060, 0,		"Little Machines" },
	{ 0x8062, 0,		"Counterpoint Computers" },
	{ 0x8065, 0x8066,	"University of Mass. at Amherst" },
	{ 0x8067, 0,		"Veeco Integrated Automation" },
	{ 0x8068, 0,		"General Dynamics" },
	{ 0x8069, 0,		"AT&T" },
	{ 0x806A, 0,		"Autophon" },
	{ 0x806C, 0,		"ComDesign" },
	{ 0x806D, 0,		"Compugraphic Corporation" },
	{ 0x806E, 0x8077,	"Landmark Graphics Corporation" },
	{ 0x807A, 0,		"Matra" },
	{ 0x807B, 0,		"Dansk Data Elektronik A/S" },
	{ 0x807C, 0,		"Merit Internodal (University of Michigan)" },
	{ 0x807D, 0x807F,	"Vitalink Communications" },
	{ 0x8080, 0,		"Vitalink TransLAN III Management" },
	{ 0x809B, 0,		"EtherTalk (AppleTalk over Ethernet)" },
	{ 0x809C, 0x809E,	"Datability" },
	{ 0x809F, 0,		"Spider Systems Ltd." },
	{ 0x80A3, 0,		"Nixdorf Computers" },
	{ 0x80A4, 0x80B3,	"Siemens Gammasonics Inc." },
	{ 0x80C0, 0,		"Digital Comm. Assoc. Inc." },
	{ 0x80C1, 0,		"DCA Data Exchange Cluster" },
	{ 0x80C2, 0x80C3,	"Digital Comm. Assoc. Inc." },
	{ 0x80C6, 0,		"Pacer Software" },
	{ 0x80C7, 0,		"Applitek Corporation" },
	{ 0x80C8, 0x80CC,	"Intergraph Corporation" },
	{ 0x80CD, 0x80CE,	"Harris Corporation" },
	{ 0x80CF, 0x80D2,	"Taylor Instrument" },
	{ 0x80D3, 0x80D4,	"Rosemount Corporation" },
	{ 0x80DD, 0,		"Varian Associates" },
	{ 0x80DE, 0,		"TRFS (Integrated Solutions Transparent Remote File System)" },
	{ 0x80DF, 0,		"Integrated Solutions" },
	{ 0x80E0, 0x80E3,	"Allen-Bradley" },
	{ 0x80E4, 0x80F0,	"Datability" },
	{ 0x80F2, 0,		"Retix" },
	{ 0x80F3, 0,		"AppleTalk Address Resolution Protocol (AARP) (Kinetics)" },
	{ 0x80F4, 0x80F5,	"Kinetics" },
	{ 0x80F7, 0,		"Apollo Computer" },
	{ 0x80FF, 0x8103,	"Wellfleet Communications" },
	{ 0x8107, 0x8109,	"Symbolics Private" },
	{ 0x8130, 0,		"Waterloo Microsystems Inc." },
	{ 0x8131, 0,		"VG Laboratory Systems" },
	{ 0x8137, 0x8138,	"Novell, Inc." },
	{ 0x8139, 0x813D,	"KTI" },
	{ 0x9000, 0,		"Loopback (Configuration Test Protocol)" },
	{ 0x9001, 0,		"Bridge Communications XNS Systems Management" },
	{ 0x9002, 0,		"Bridge Communications TCP/IP Systems Management" },
	{ 0xFF00, 0,		"BBN VITAL-LanBridge cache wakeups" },
};
#define	NTYPES	(sizeof typeinfo / sizeof typeinfo[0])

char *
ether_typedesc(unsigned short type)
{
	static Index *typedesc;

	if (type < 0x0600)
		return "IEEE 802.3 Length";
	if (typedesc == 0) {
		int n, m;
		struct typeinfo *ti;
		unsigned short *t;

		n = NTYPES;
		typedesc = index(n, sizeof(unsigned short), 0);
		for (ti = typeinfo; --n >= 0; ti++) {
			in_enter(typedesc, &ti->code, ti->desc);
			if (ti->highcode == 0)
				continue;
			in_enter(typedesc, &ti->highcode, ti->desc);
			m = ti->highcode - ti->code - 1;
			if (m == 0)
				continue;
			assert(m > 0);
			t = vnew(m, unsigned short);
			do {
				*t = ti->code + m;
				in_enter(typedesc, t, ti->desc);
				--m, t++;
			} while (m > 0);
		}
	}
	return in_match(typedesc, &type);
}
