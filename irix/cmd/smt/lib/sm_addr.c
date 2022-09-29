/*
 * Copyright 1989,1990,1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	$Revision: 1.13 $
 */

/*
 * Routines to deal with FDDI addresses.  The /etc/ethers file
 * contains mappings from 48 bit fddi MAC addresses to their corresponding
 * hosts name.  The addresses have an ascii representation of the form
 * "x:x:x:x:x:x" where x is a hex number between 0x00 and 0xff;  the
 * bytes are always in network order.
 */

#include <stdio.h>
#include <string.h>
#include <bstring.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <rpcsvc/ypclnt.h>

#include <sm_types.h>
#include <sm_addr.h>
extern void skipblank(char**);
extern char **getline(char*, int, FILE*);


LFDDI_ADDR zero_mac_addr = {0};
LFDDI_ADDR unknown_mac_addr;

typedef struct vendor {
	u_char	code[3];
	char	*abbrev;
} VENDOR;
VENDOR *vendors = 0;
VENDOR blt_vendors[] = {
	{ {0x00,0x00,0x0C},	"Cisco" },
	{ {0x00,0x00,0x0F},	"NeXT" },
	{ {0x00,0x00,0x10},	"Sytek" },
	{ {0x00,0x00,0x1D},	"Cabletron" },
	{ {0x00,0x00,0x20},	"DIAB" },
	{ {0x00,0x00,0x22},	"VisualTech" },
	{ {0x00,0x00,0x2A},	"TRW" },
	{ {0x00,0x00,0x5A},	"S&Koch" },
	{ {0x00,0x00,0x5E},	"IANA" },
	{ {0x00,0x00,0x65},	"NetworkGen" },
	{ {0x00,0x00,0x6B},	"MIPS" },
	{ {0x00,0x00,0x77},	"MIPS" },
	{ {0x00,0x00,0x7A},	"Ardent" },
	{ {0x00,0x00,0x89},	"Cayman" },
	{ {0x00,0x00,0x93},	"Proteon" },
	{ {0x00,0x00,0x9F},	"Ameristar" },
	{ {0x00,0x00,0xA2},	"Wellfleet" },
	{ {0x00,0x00,0xA7},	"NCD" },
	{ {0x00,0x00,0xA9},	"NetworkSys" },
	{ {0x00,0x00,0xAA},	"Xerox" },
	{ {0x00,0x00,0xB3},	"CIMLinc" },
	{ {0x00,0x00,0xB7},	"Dove" },
	{ {0x00,0x00,0xBC},	"Allen-Brad" },
	{ {0x00,0x00,0xC0},	"WesternDig" },
	{ {0x00,0x00,0xC6},	"H-P" },
	{ {0x00,0x00,0xC8},	"Altos" },
	{ {0x00,0x00,0xC9},	"Emulex" },
	{ {0x00,0x00,0xD7},	"Dartmouth" },
	{ {0x00,0x00,0xD8},	"PS2" },
	{ {0x00,0x00,0xDD},	"Gould" },
	{ {0x00,0x00,0xDE},	"Unigraph" },
	{ {0x00,0x00,0xE2},	"Acer" },
	{ {0x00,0x00,0xEF},	"Alantec" },
	{ {0x00,0x00,0xFD},	"Orion" },
	{ {0x00,0x01,0x02},	"BBN" },
	{ {0x00,0x17,0x00},	"Kabel" },
	{ {0x00,0x80,0x2D},	"Xylogics" },
	{ {0x00,0x80,0x8C},	"Frontier" },
	{ {0x00,0xAA,0x00},	"Intel" },
	{ {0x00,0xDD,0x00},	"Ung-Bass" },
	{ {0x00,0xDD,0x01},	"Ung-Bass" },
	{ {0x01,0x00,0x5E},	"IP-mcast" },
	{ {0x01,0x80,0xC2},	"SMT" },
	{ {0x02,0x07,0x01},	"Interlan" },
	{ {0x02,0x04,0x06},	"BBN" },
	{ {0x02,0x60,0x86},	"Satelcom" },
	{ {0x02,0x60,0x8C},	"3Com" },
	{ {0x02,0xCF,0x1F},	"CMC" },
	{ {0x08,0x00,0x02},	"3Com" },
	{ {0x08,0x00,0x03},	"ACC" },
	{ {0x08,0x00,0x05},	"Symbolics" },
	{ {0x08,0x00,0x08},	"BBN" },
	{ {0x08,0x00,0x09},	"H-P" },
	{ {0x08,0x00,0x0A},	"NestarSys" },
	{ {0x08,0x00,0x0B},	"Unisys" },
	{ {0x08,0x00,0x10},	"AT&T" },
	{ {0x08,0x00,0x11},	"Tektronix" },
	{ {0x08,0x00,0x14},	"Excelan" },
	{ {0x08,0x00,0x17},	"NSC" },
	{ {0x08,0x00,0x1A},	"D-G" },
	{ {0x08,0x00,0x1B},	"D-G" },
	{ {0x08,0x00,0x1E},	"Apollo" },
	{ {0x08,0x00,0x20},	"Sun" },
	{ {0x08,0x00,0x22},	"NBI" },
	{ {0x08,0x00,0x25},	"CDC" },
	{ {0x08,0x00,0x28},	"TI" },
	{ {0x08,0x00,0x2B},	"DEC" },
	{ {0x08,0x00,0x2E},	"Metaphor" },
	{ {0x08,0x00,0x2F},	"Prime" },
	{ {0x08,0x00,0x36},	"Intergraph" },
	{ {0x08,0x00,0x37},	"Fujitsu" },
	{ {0x08,0x00,0x38},	"Bull" },
	{ {0x08,0x00,0x39},	"SpiderSys" },
	{ {0x08,0x00,0x41},	"DCA" },
	{ {0x08,0x00,0x45},	"Xylogics" },
	{ {0x08,0x00,0x46},	"Sony" },
	{ {0x08,0x00,0x47},	"Sequent" },
	{ {0x08,0x00,0x49},	"Univation" },
	{ {0x08,0x00,0x4C},	"Encore" },
	{ {0x08,0x00,0x4E},	"BICC" },
	{ {0x08,0x00,0x56},	"Stanford" },
	{ {0x08,0x00,0x5A},	"IBM" },
	{ {0x08,0x00,0x67},	"Comdesign" },
	{ {0x08,0x00,0x68},	"Ridge" },
	{ {0x08,0x00,0x69},	"SGI" },
	{ {0x08,0x00,0x6E},	"Excelan" },
	{ {0x08,0x00,0x75},	"DDE" },
	{ {0x08,0x00,0x7C},	"Vitalink" },
	{ {0x08,0x00,0x80},	"XIOS" },
	{ {0x08,0x00,0x86},	"Imagen" },
	{ {0x08,0x00,0x87},	"Xyplex" },
	{ {0x08,0x00,0x89},	"Kinetics" },
	{ {0x08,0x00,0x8B},	"Pyramid" },
	{ {0x08,0x00,0x8D},	"XyVision" },
	{ {0x08,0x00,0x90},	"Retix" },
	{ {0x09,0x00,0x2B},	"DEC-LAT" },
	{ {0x80,0x00,0x10},	"AT&T" },	/* illicit, transposed 0x08 */
	{ {0xAA,0x00,0x03},	"DEC" },
	{ {0xAA,0x00,0x04},	"DEC" },
	{ {0xAB,0x00,0x00},	"DECnet" },
};
#define NBLTVENDORS (sizeof(blt_vendors)/sizeof(blt_vendors[0]))
#define NVENDORS (NBLTVENDORS*2)

typedef unsigned long hash_t;
static unsigned int vn_shift;
static unsigned int vn_count;

#define	PHI	((hash_t) 2654435769)
#define	RHO	6
#define BITS(x)	(sizeof(x)*8)

static unsigned int
vn_hash(VENDOR *v)
{
	register hash_t hash;
	register int len = sizeof(v->code);
	register u_char *name = v->code;

	for (hash = 0; --len >= 0; name++)
		hash = (hash << RHO) + (hash >> BITS(hash_t) - RHO) + *name;
	return((PHI*hash)>>vn_shift);
}

static VENDOR *
vn_search(VENDOR *v)
{
	register VENDOR *sym = &vendors[vn_hash(v)];
	while (sym->abbrev != 0 && bcmp(v->code, sym->code, 3) != 0) {
		if (--sym < vendors)
			sym += vn_count;
	}
	return(sym);
}

char *
vn_lookup(LFDDI_ADDR *a)
{
	VENDOR *sym;

	if (!vendors)
		return(0);
	/* trust a != 0 */
	sym = vn_search((VENDOR*)a);
	return(sym->abbrev);
}

static int vn_addcnt = 0;
static void
vn_add(u_char *code, char *name)
{
	VENDOR *sym;
	if (vn_addcnt >= vn_count-10)
		return;
	vn_addcnt++;

	sym = vn_search((VENDOR*)code);
	sym->code[0] = code[0];
	sym->code[1] = code[1];
	sym->code[2] = code[2];
	sym->abbrev = name;
}

void
vn_file()
{
	FILE *fp;
	char line[128];
	char name[128];
	u_char cd[3];
	int c0, c1, c2;
	char **lp;

	if ((fp = fopen("/usr/etc/vendors", "r")) == NULL)
		return;

	while (lp = getline(line, sizeof(line), fp)) {
		skipblank(lp);
		if (**lp == '#')
			continue;
		if (sscanf(*lp,"%x:%x:%x%s",&c0,&c1,&c2,name) != 4)
			continue;

		cd[0] = c0; cd[1] = c1; cd[2] = c2;
		bitswapcopy(cd, cd, sizeof(cd));
		vn_add(cd, strdup(name));
	}
	fclose(fp);
}

void
vn_init()
{
	VENDOR *vn;
	unsigned int log2, count;
	register int i;

	if (vendors)
		return;

	count = NVENDORS;
	log2 = (count & count-1) ? 1 : 0;
	if (count & 0xffff0000)
		log2 += 16, count &= 0xffff0000;
	if (count & 0xff00ff00)
		log2 += 8, count &= 0xff00ff00;
	if (count & 0xf0f0f0f0)
		log2 += 4, count &= 0xf0f0f0f0;
	if (count & 0xcccccccc)
		log2 += 2, count &= 0xcccccccc;
	if (count & 0xaaaaaaaa)
		log2 += 1;
	vn_count = 1 << log2;
	vn_shift = BITS(hash_t) - log2;
	vendors = (VENDOR *)Malloc(vn_count*sizeof(VENDOR));
	for (i = 0, vn = blt_vendors; i < NBLTVENDORS; i++, vn++) {
		bitswapcopy(vn->code, vn->code, sizeof(vn->code));
		vn_add(vn->code, vn->abbrev);
	}

	(void)fddi_aton(SMT_UNKNOWN_ADDRESS, &unknown_mac_addr);

	vn_file();
}

/*
 * Converts fddi SID to its string representation.
 */
char *
fddi_sidtoa(SMT_STATIONID *s, char *a)
{
	char e[sizeof(SMT_STATIONID)];

	bitswapcopy(s, e, sizeof(e));
	sprintf(a, "%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x",
			e[0], e[1], e[2], e[3], e[4], e[5], e[6], e[7]);
	return(a);
}

/*
 * Converts a 48 bit fddi MAC address to its string representation.
 */
char *
fddi_ntoa(LFDDI_ADDR *n, char *a)
{
	LFDDI_ADDR e;
	char *v;

	v = vn_lookup(n);
	bitswapcopy(n, &e, sizeof(e));
	if (v != 0) {
		sprintf(a, "%02x:%02x:%02x:%02x:%02x:%02x/%s",
			e.b[0], e.b[1], e.b[2], e.b[3], e.b[4], e.b[5], v);
	} else {
		sprintf(a, "%02x:%02x:%02x:%02x:%02x:%02x",
			e.b[0], e.b[1], e.b[2], e.b[3], e.b[4], e.b[5]);
	}
	return(a);
}

/*
 * Converts a ASCII FDDI MAC address representation back into its 48 bits.
 */
LFDDI_ADDR *
fddi_aton(char *a, LFDDI_ADDR *n)
{
	LFDDI_ADDR *e;

	if ((e = (LFDDI_ADDR *)ether_aton(a)) == 0)
		return(0);

	bitswapcopy(e, n, sizeof(*n));
	return(n);
}

/*
 * Given a host's name, this routine returns its 48 bit ethernet address.
 * Returns zero if successful, non-zero otherwise.
 */
int
fddi_hostton(char *host, LFDDI_ADDR *n)
{
	if (!ether_hostton(host, n)) {
		bitswapcopy(n, n, sizeof(*n));
		return(0);
	}
	return(-1);
}

/*
 * Given a 48 bit fddi MAC address, this routine return its host name.
 * Returns zero if successful, non-zero otherwise.
 */
int
fddi_ntohost(LFDDI_ADDR *n, char *host)
{
	LFDDI_ADDR t;

	bitswapcopy(n, &t, sizeof(*n));

	if (!ether_ntohost(host, &t)) {
		/* found it */
		return(0);
	}

	/* see if it is one of the unknown addresses */
	if (!bcmp(&t,&zero_mac_addr,sizeof(t))) {
		(void)strcpy(host,"(\?\?)");
		return(0);
	}
	if (!bcmp(&t,&unknown_mac_addr,sizeof(t))) {
		(void)strcpy(host,"(\?)");
		return(0);
	}

	return(-1);
}
