/* Maintain EEPROM on E-Plex 8-port Ethernet board
 *
 * Copyright 1994 Silicon Graphics, Inc.  All rights reserved.
 */

#ident "$Revision: 1.13 $"

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <bstring.h>
#include <errno.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/capability.h>
#include <net/raw.h>
#include <net/if.h>
#include <netinet/if_ether.h>
#include <cap_net.h>
#include "if_ep.h"


char *pgmname;

static int verbose;
static int do_nothing;
static int quiet;
static int wflag;
static int force;

#define EP_NAME "ep"

enum bd_type {GOOD_DANG, BAD_DANG};

static struct etheraddr {
	u_char	ea_vec[6];
} eaddrs[EP_PORTS];

static void complain(char*,char*,char*);
static void ether_aton(char*, struct etheraddr*);
static int one_epb(char*,int);
static void ep_mac_print(char*,int,int);
static int ep_write(char*,int,int,enum bd_type);
static int ck_firm(char*,int,int,enum bd_type*, int);



/* do not document the MAC address writing facilities, to avoid
 * tempting customers to mess things up.
 */
static void
usage(void)
{
	(void)fprintf(stderr, "%s: %s\n", pgmname,
		      "usage: [-vnqf] [interface]");
	exit(1);
}


void
main(int argc,
     char* argv[])
{
	int i;
	char *p, *q;
	char ifname[sizeof(EP_NAME)+3];
	int exit_val = 0;


	pgmname = argv[0];

	while ((i = getopt(argc, argv, "vnqfw:")) != EOF)
		switch (i) {
		case 'v':
			verbose++;
			break;
		case 'n':
			do_nothing = 1;
			break;
		case 'q':
			quiet = 1;
			break;
		case 'f':
			force = 1;
			break;
		case 'w':
			wflag++;
			p = optarg;
			for (i = 0;
			     i < EP_PORTS && p != 0 && *p != '\0';
			     i++) {
				q = strpbrk(p,",");
				if (q != 0)
					*q++ = '\0';
				ether_aton(p, &eaddrs[i]);
				p = q;
			}
			if (i != EP_PORTS || p != 0) {
				(void)fprintf(stderr, "%s: need %d "
					      "comma-separated MAC "
					      "addresses\n",
					      pgmname, EP_PORTS);
				exit(1);
			}
			break;

		default:
			usage();
			break;
	}
	if (argc == optind) {
		if (wflag)
			usage();
		quiet = 1;
		for (i = 0; i < EP_PORTS_OCT*EP_MAXBD; i += EP_PORTS_OCT) {
			(void)sprintf(ifname, EP_NAME "%d", i);
			if (!one_epb(ifname,i))
				exit_val = 1;
		}
	} else {
		if (argc != optind+1)
			usage();
		if (1 != sscanf(argv[optind], EP_NAME "%d", &i)) {
			(void)fprintf(stderr,
				      "%s: unrecognized interface: %s\n",
				      pgmname, argv[optind]);
			exit(1);
		}
		if (i >= EP_MAXBD || 0 != (i % EP_PORTS_OCT)) {
			(void)fprintf(stderr, "%s: invalid interface unit "
				      "number %d\n",
				      pgmname, i);
			exit(1);
		}

		if (!one_epb(argv[optind],i))
			exit_val = 1;
	}

	exit(exit_val);
}


static int				/* 0=failed */
board_down(int unit,
	   int rawsoc,
	   int force_down)
{
	struct ifreq ifr;
	char ifname[sizeof(EP_NAME)+3];
	int i;

	unit -= unit % EP_PORTS_OCT;
	for (i = 0; i < EP_PORTS; i++) {
		(void)sprintf(ifname, EP_NAME "%d", i);
		strncpy(ifr.ifr_name, ifname, sizeof (ifr.ifr_name));
		if (ioctl(rawsoc, SIOCGIFFLAGS, &ifr) < 0) {
			complain(ifname,"","SIOCGIFFLAGS");
			return 0;
		}
		if (0 != (ifr.ifr_flags & (IFF_UP | IFF_RUNNING))
		    || (i == 0 && force_down)) {
			ifr.ifr_flags &= ~(IFF_UP | IFF_RUNNING);
			strncpy(ifr.ifr_name, ifname, sizeof (ifr.ifr_name));
			if (cap_network_ioctl(rawsoc, SIOCSIFFLAGS, &ifr) < 0) {
				complain(ifname,"","SIOCSIFFLAGS");
				return 0;
			}
		}
	}
	return 1;
}


/* deal with one board
 */
static int				/* 0=failed */
one_epb(char *ifname,
	int unit)
{
	int rawsoc;
	struct sockaddr_raw sr;
	enum bd_type bd_type;
	int res = 1;


	rawsoc = cap_socket(AF_RAW, SOCK_RAW, RAWPROTO_SNOOP);
	if (rawsoc < 0) {
		complain(ifname,"","socket");
		return 0;
	}

	bzero((char*)&sr, sizeof(sr));
	(void)strncpy(sr.sr_ifname, ifname, sizeof(sr.sr_ifname));
	sr.sr_family = AF_RAW;
	sr.sr_port = 0;
	if (0 > bind(rawsoc, &sr, sizeof(sr))) {
		if (errno != EADDRNOTAVAIL
		    || !quiet
		    || verbose > 1)
			complain(ifname,"","bind");
		(void)close(rawsoc);
		return (errno == EADDRNOTAVAIL);
	}



	if ((ck_firm(ifname,unit,rawsoc,&bd_type,
		     force ? MAX(2,verbose) : verbose)
	     || wflag || force)
	    && !do_nothing) {
		if (verbose > 2)
			ep_mac_print(ifname,unit,rawsoc);
		res = ep_write(ifname,unit,rawsoc,bd_type);
		if (verbose > 1)
			ep_mac_print(ifname,unit,rawsoc);

	} else if (verbose > 1) {
		ep_mac_print(ifname,unit,rawsoc);
	}

	(void)close(rawsoc);

	return res;
}


#include "firm/ep_flash.firm"
#include "firm/ep_b.firm"
#include "firm/ep_c.firm"

static struct ep_dwn ep_dwn;

static int				/* 0 = failed */
ep_downflush(char *ifname,
	     int rawsoc)
{
	if (ep_dwn.cnt == 0)
		return 1;

	if (0 > cap_network_ioctl(rawsoc, SIOC_EP_STO, &ep_dwn)) {
		complain(ifname,"\n","SIOC_EP_STO");
		return 0;
	}

	ep_dwn.addr += ep_dwn.cnt*4;
	ep_dwn.cnt = 0;
	return 1;
}


static int				/* 0=failed */
ep_download(char *ifname, int rawsoc,
	    u_long firm_min,
	    u_long addr,		/* start here */
	    u_long firm_max,		/* fill this much */
	    u_char* fp,			/* bytes of firmware */
	    u_char* fp_lim)
{
	int j;
	u_char op;
	u_long v;


	if (firm_min > addr
	    || addr+fp_lim-fp > firm_max) {
		fflush(stdout);
		(void)fprintf(stderr, "%s: invalid firmware;"
			      " should have %x<=%x and %x<=%x",
			      pgmname,
			      firm_min, addr,
			      addr+fp_lim-fp, firm_max);
		fflush(stderr);
		return 0;
	}

	ep_dwn.addr = firm_min;
	ep_dwn.cnt = 0;

	/* fill initial part with zeros
	 */
	while (ep_dwn.addr+ep_dwn.cnt*4 < addr) {
		if (ep_dwn.cnt >= EP_DWN_LEN
		    && !ep_downflush(ifname,rawsoc))
			return 0;
		ep_dwn.val.l[ep_dwn.cnt++] = 0;
	}

	/* install the firmware
	 */
	while (fp != fp_lim) {
		op = *fp++;
		if (op < ep_c_DZERO) {
			j = op-ep_c_DDATA+1;
			while (j-- != 0) {
				if (ep_dwn.cnt >= EP_DWN_LEN
				    && !ep_downflush(ifname,rawsoc))
					return 0;
				ep_dwn.val.c[ep_dwn.cnt*4+0] = *fp++;
				ep_dwn.val.c[ep_dwn.cnt*4+1] = *fp++;
				ep_dwn.val.c[ep_dwn.cnt*4+2] = *fp++;
				ep_dwn.val.c[ep_dwn.cnt*4+3] = *fp++;
				ep_dwn.cnt++;
			}
		} else {
			if (op < ep_c_DNOP) {
				j = op-ep_c_DZERO+1;
				v = 0;
			} else {
				j = op-ep_c_DNOP+1;
				v = 0x70400101;
			}
			while (j-- != 0) {
				if (ep_dwn.cnt >= EP_DWN_LEN
				    && !ep_downflush(ifname,rawsoc))
					return 0;
				ep_dwn.val.l[ep_dwn.cnt++] = v;
			}
		}
	}

	/* fill rest with 0s to make checksum right
	 */
	while (ep_dwn.addr+ep_dwn.cnt*4 < firm_max) {
		if (ep_dwn.cnt >= EP_DWN_LEN
		    && !ep_downflush(ifname,rawsoc))
			return 0;
		ep_dwn.val.l[ep_dwn.cnt++] = 0;
	}

	return ep_downflush(ifname,rawsoc);
}



/* print MAC address area of EP EEPROM
 */
static void
ep_mac_print(char *ifname, int unit, int rawsoc)
{
	int port;
	int i;
	u_long cksum;



	for (port = 0; port < EP_PORTS; port++) {
		ep_dwn.addr = EP_MACS_ADDR + port*EP_MAC_SIZE;
		ep_dwn.cnt = (EP_MAC_SIZE+3)/4;
		if (0 > cap_network_ioctl(rawsoc, SIOC_EP_FET, &ep_dwn)) {
			complain(ifname,"","SIOC_EP_FET");
			return;
		}

		printf("ep%d: %x:%x:%x:%x:%x:%x",
		       port+unit,
		       ep_dwn.val.c[0],
		       ep_dwn.val.c[1],
		       ep_dwn.val.c[2],
		       ep_dwn.val.c[3],
		       ep_dwn.val.c[4],
		       ep_dwn.val.c[5]);

		for (i = 6; i < EP_MAC_SIZE-4; i++) {
			if (ep_dwn.val.c[i] != 0)
				break;
		}
		EP_MAC_SUM(cksum, ep_dwn.val.l);
		if (cksum != 0) {
			printf("  bad checksum  ");
			i = 0;
		}

		if (i < EP_MAC_SIZE-4 || verbose > 2) {
			for (i = 6; i < EP_MAC_SIZE; i++)
				printf(" %02x", ep_dwn.val.c[i]);
		}
		printf("\n");
	}
}



/* Write everything to the board.
 */
static int				/* 0=failed */
ep_write(char *ifname,
	 int unit,
	 int rawsoc,
	 enum bd_type bd_type)
{
	int i;
	int port;
	u_long x;
	u_long cksum;
	int res = 0;


	/* stop compiler complaints about unused variables */
	i = ep_flash_vers+ep_b_maxaddr+ep_c_maxaddr;

	if (!board_down(unit,rawsoc,0))
		return res;

	fflush(stderr);
	fprintf(stdout, "%s: WARNING: Writing %s EEPROM.\n",
		pgmname, ifname);
	fprintf(stdout,"\tDo not reset the system until finished");
	fflush(stdout);
	/* pause in case the operator does try to abort the process */
	for (i = 0; i < 2*2; i++) {
		sginap(HZ/2);
		fputc("   ."[i],stdout);
		fflush(stdout);
	}

	if (wflag) {
		/* download the MAC addresses
		 */
		for (port = 0; port < EP_PORTS; port++) {
			bzero(&ep_dwn.val, sizeof(ep_dwn.val));
			bcopy(&eaddrs[port], ep_dwn.val.l,
			      sizeof(eaddrs[port]));
			EP_MAC_SUM(cksum, ep_dwn.val.l);
			ep_dwn.val.l[EP_MAC_SIZE/4-1] = -cksum;
			ep_dwn.cnt = EP_MAC_SIZE/4;
			ep_dwn.addr = (EP_MACS_ADDR + port*EP_MAC_SIZE
				       + EP_DOWN);
			if (!ep_downflush(ifname,rawsoc))
				return res;
		}
	}

	/* download the firmware
	 */
	if (bd_type == GOOD_DANG) {
		if (!ep_download(ifname,rawsoc,
				 EP_FIRM + EP_DOWN,
				 EP_FIRM + EP_DOWN + ep_c_minaddr-EP_GIOSRAM,
				 EP_FIRM + EP_FIRM_SIZE + EP_DOWN,
				 &ep_c_txt[0],
				 &ep_c_txt[sizeof(ep_c_txt)]))
			return res;
	} else {
		if (!ep_download(ifname,rawsoc,
				 EP_FIRM + EP_DOWN,
				 EP_FIRM + EP_DOWN + ep_b_minaddr-EP_GIOSRAM,
				 EP_FIRM + EP_FIRM_SIZE + EP_DOWN,
				 &ep_b_txt[0],
				 &ep_b_txt[sizeof(ep_b_txt)]))
			return res;
	}

	/* Clear the diagnostics for now.
	 */
	if (!ep_download(ifname,rawsoc,
			 EP_DIAG + EP_DOWN,
			 EP_DIAG + EP_DOWN,
			 EP_DIAG + EP_DIAG_SIZE + EP_DOWN, 0,0))
		return res;

	/* Clear the rest.
	 */
	if (!ep_download(ifname,rawsoc,
			 EP_SPARE + EP_DOWN,
			 EP_SPARE + EP_DOWN,
			 EP_SPARE + EP_SPARE_SIZE + EP_DOWN, 0,0))
		return res;

	/* download the code to program the EEPROM
	 */
	if (!ep_download(ifname,rawsoc,
			 ep_flash_minaddr,
			 ep_flash_minaddr,
			 ep_flash_maxaddr,
			 &ep_flash_txt[0],
			 &ep_flash_txt[sizeof(ep_flash_txt)]))
		return res;

	/* zap it */
	ep_dwn.addr = EP_PROG;
	if (0 > cap_network_ioctl(rawsoc, SIOC_EP_EXEC, &ep_dwn))
		complain(ifname,"\n","SIOC_EP_EXEC");
	else
		res = 1;

	/* Wait until the EEPROM chip is written, even if we are not
	 * sure it is working, since the worst thing we can do
	 * is interrupt it before the chip has been written.
	 */
	for (i = (20*HZ)/(HZ/2); i != 0; i--) {
		x = 0;
		if (0 <= cap_network_ioctl(rawsoc,SIOC_EP_SIGNAL,&x))
			break;
		if (errno != EIO)
			complain(ifname,"\n","SIOC_EP_SIGNAL");
		fputc('.',stdout);
		fflush(stdout);
		sginap(HZ/2);
	}
	if (!i) {
		complain(ifname,"\n","failed to start");
		res = 0;
	}

	(void)fprintf(stdout,"\n\tFinished.\n");
	if (!board_down(unit,rawsoc,1))
		res = 0;

	if (ck_firm(ifname,unit,rawsoc,&bd_type,0)) {
		(void)fprintf(stderr, "%s: firmware did not stick in %s\n",
			      pgmname, ifname);
		res = 0;
	}

	return res;
}



/* check the firmware on the FDDI board
 *	If it is old or bad, download a new version
 */
/* ARGSUSED */
static int				/* 1=need to update EEPROM */
ck_firm(char *ifname,
	int unit,
	int rawsoc,
	enum bd_type *bd_type,
	int verb)
{
	struct ep_vers bd_vers;
	int new_vers, n;

	/* Get and compare versions.
	 *	Update the board if it differs from us,
	 *	even if it is newer.  This minimizes problems
	 *	with version skew.
	 */
	if (0 > ioctl(rawsoc, SIOC_EP_VERS, &bd_vers)) {
		n = errno;
		(void)board_down(unit,rawsoc,1);
		errno = n;
		complain(ifname,"","SIOC_EP_VERS");
		return 1;
	}

	if (bd_vers.vers & EP_VERS_BAD_DANG) {
		*bd_type = BAD_DANG;
		new_vers = ep_b_vers;
	} else {
		*bd_type = GOOD_DANG;
		new_vers = ep_c_vers;
	}
	bd_vers.vers &= (EP_VERS_CKSUM | EP_VERS_DATE_M | EP_VERS_FAM_M);

	/* display version */
	if (verb) {
		n = bd_vers.vers & EP_VERS_DATE_M;
		printf("ep%d-ep%d current firmware version"
		       " %d-%d%02d%02d%02d00%s\n",
		       unit, unit+EP_PORTS-1,
		       (bd_vers.vers & EP_VERS_FAM_M)/EP_VERS_FAM_S,
		       (n/(24*32*13)) + 92,
		       (n/(24*32)) % 13,
		       (n/24) % 32,
		       n % 24,
		       *bd_type == BAD_DANG ? " rev.B DANG chip" : "");
	}

	if (verb > 1 || new_vers != bd_vers.vers) {
		n = new_vers & EP_VERS_DATE_M;
		printf("	    %snew firmware version"
		       " %d-%d%02d%02d%02d00\n",
		       unit > 10 ? "  " : "",
		       (new_vers & EP_VERS_FAM_M)/EP_VERS_FAM_S,
		       (n/(24*32*13)) + 92,
		       (n/(24*32)) % 13,
		       (n/24) % 32,
		       n % 24);
	}

	return (new_vers != bd_vers.vers);
}


static void
complain(char *ifname, char *s1, char* s2)
{
	fflush(stdout);
	(void)fprintf(stderr, "%s%s: %s %s: %s\n",
		      s1, pgmname, s2, ifname, strerror(errno));
	fflush(stderr);
}


static void
ether_aton(char *a,
	   struct etheraddr *n)
{
	int len;
	int colons;
	int i;
	int o[6];

	len = strspn(a,"0123456789abcdefABCDEF:xX");
	colons = 0;
	for (i = 0; a[i] != '\0'; i++) {
		if (a[i] == ':')
			colons++;
	}
	if (len != strlen(a)
	    || colons != 5
	    || 6 != sscanf(a, "%x:%x:%x:%x:%x:%x", &o[0], &o[1], &o[2],
			   &o[3], &o[4], &o[5])) {
		(void)fprintf(stderr, "%s: invalid MAC address '%s'\n",
			      pgmname, a);
		exit(1);
	}
	for (i=0; i<6; i++)
		n->ea_vec[i] = o[i];
}
