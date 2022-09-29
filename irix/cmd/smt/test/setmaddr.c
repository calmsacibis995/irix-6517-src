/* Read and write the MAC address.
 *
 * Copyright 1992 Silicon Graphics, Inc.  All rights reserved.
 */

#ident "$Revision: 1.14 $"

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <bstring.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/param.h>
#include <net/raw.h>
#include <net/if.h>
#include <sys/fddi.h>
#include <sys/smt.h>
#include <sys/if_ipg.h>
#include "if_xpi.h"

extern int getopt();
extern char *optarg;
extern int optind;


char *pgmname;

static int verbose;
static int wflag;

static char *ifname;

static u_char eaddr[6];
static int rawsoc;
static struct sockaddr_raw sr;


#define USAGE "usage: [-v] [-w xx:xx:xx:yy:yy:yy] interface"

static void
usage()
{
	(void)fprintf(stderr, "%s: %s\n", pgmname, USAGE);
	exit(1);
}


static void complain(char*,char*);
static void ether_aton(char*, u_char[6]);
static int xpi_io(void);
static void ipg_nvram_print(IPG_NVRAM*);
static int ipg_io(void);


main(int argc,
     char* argv[])
{
	int i;
	struct ifreq ifr;


	pgmname = argv[0];

	while ((i = getopt(argc, argv, "vw:")) != EOF)
		switch (i) {
		case 'v':
			verbose++;
			break;
		case 'w':
			ether_aton(optarg, eaddr);
			wflag++;
			break;

		default:
			usage();
			break;
	}
	if (argc != optind+1)
		usage();
	ifname = argv[optind];


	rawsoc = socket(AF_RAW, SOCK_RAW, RAWPROTO_SNOOP);
	if (rawsoc < 0)
		complain("","socket");

	bzero(&sr, sizeof(sr));
	(void)strncpy(sr.sr_ifname, ifname, sizeof(sr.sr_ifname));
	sr.sr_family = AF_RAW;
	sr.sr_port = 0;
	if (0 > bind(rawsoc, &sr, sizeof(sr))) {
		complain("","bind");
	}


	/* turn off the interrface if we are going to change anything
	 */
	if (wflag) {
		strncpy(ifr.ifr_name, ifname, sizeof (ifr.ifr_name));
		if (ioctl(rawsoc, SIOCGIFFLAGS, &ifr) < 0)
			complain("","SIOCGIFFLAGS");

		if (0 != (ifr.ifr_flags & (IFF_UP | IFF_RUNNING))) {
			ifr.ifr_flags &= ~(IFF_UP | IFF_RUNNING);
			strncpy(ifr.ifr_name, ifname, sizeof (ifr.ifr_name));
			if (ioctl(rawsoc, SIOCSIFFLAGS, &ifr) < 0)
				complain("","SOICSIFFLAGS");
		}
	}

	i = strlen(ifname);
	if (!strncmp(ifname, "ipg", i-1)) {
		i = ipg_io();

	} else if (!strncmp(ifname, "xpi", i-1)) {
		i = xpi_io();

	} else {
		(void)fprintf(stderr,
			      "smtconfig: unrecognized interface: %s\n",
			      ifname);
		i = 2;
	}
	exit(i);
}



#include "../apps/xpi/lc_flash.firm"
#include "../apps/xpi/mez_flash.firm"

static struct xpi_dwn xpi_dwn;

static void
xpi_downflush()
{
	if (0 > ioctl(rawsoc, SIOC_XPI_STO, &xpi_dwn)) {
		complain("\n","SIOC_XPI_STO");
	}
	xpi_dwn.addr += xpi_dwn.cnt*4;
	xpi_dwn.cnt = 0;
}


static void
xpi_download(u_long addr,		/* start here */
	     u_char* fp,		/* bytes of firmware */
	     u_char* fp_lim)
{
	int j;
	u_char op;
	u_long v;


	xpi_dwn.addr = addr;
	xpi_dwn.cnt = 0;

	while (fp < fp_lim) {
		op = *fp++;
		if (op < lc_flash_DZERO) {
			j = op-lc_flash_DDATA+1;
			while (j-- != 0) {
				if (xpi_dwn.cnt >= XPI_DWN_LEN)
					xpi_downflush();
				xpi_dwn.val.c[xpi_dwn.cnt*4+0] = *fp++;
				xpi_dwn.val.c[xpi_dwn.cnt*4+1] = *fp++;
				xpi_dwn.val.c[xpi_dwn.cnt*4+2] = *fp++;
				xpi_dwn.val.c[xpi_dwn.cnt*4+3] = *fp++;
				xpi_dwn.cnt++;
			}
		} else {
			if (op < lc_flash_DNOP) {
				j = op-lc_flash_DZERO+1;
				v = 0;
			} else {
				j = op-lc_flash_DNOP+1;
				v = 0x70400101;
			}
			while (j-- != 0) {
				if (xpi_dwn.cnt >= XPI_DWN_LEN)
					xpi_downflush();
				xpi_dwn.val.l[xpi_dwn.cnt++] = v;
			}
		}
	}

	if (0 != xpi_dwn.cnt)
		xpi_downflush();
}


/* print MAC address area of xpi EEPROM
 */
static void
xpi_print(void)
{
	int i;
	u_long cksum;


	xpi_dwn.addr = XPI_MAC_ADDR;
	xpi_dwn.cnt = XPI_MAC_SIZE/4;
	if (0 > ioctl(rawsoc, SIOC_XPI_FET, &xpi_dwn)) {
		complain("","SIOC_XPI_FET");
	}

	for (i = 0; i < 6-1; i++)
		printf("%x:", xpi_dwn.val.c[i]);
	printf("%x", xpi_dwn.val.c[5]);

	for (i = 6; i < XPI_MAC_SIZE-4; i++) {
		if (xpi_dwn.val.c[i] != 0)
			break;
	}
	XPI_MAC_SUM(cksum, xpi_dwn.val.l);
	if (cksum != 0) {
		printf("   bad checksum  ");
		i = 0;
	}

	if (i < XPI_MAC_SIZE-4 || verbose > 1) {
		for (i = 6; i < XPI_MAC_SIZE; i++)
			printf(" %02x", xpi_dwn.val.c[i]);
	}
	printf("\n");
}



/* work on an xpi board
 */
static int
xpi_io(void)
{
	int i;
	u_long x;
	u_long cksum;
	struct xpi_vers vers;
#if XPI_MAC_SIZE > XPI_DWN_LEN*4
	? ? ? error ? ? ?
#endif

	if (wflag) {
		fflush(stdout);
		if (0 > ioctl(rawsoc, SIOC_XPI_VERS, &vers))
			complain("", "SIOC_XPI_VERS");
		if (vers.type != XPI_TYPE_LC
		    && vers.type != XPI_TYPE_MEZ_S
		    && vers.type != XPI_TYPE_MEZ_D) {
			(void)fprintf(stderr,
				      "unrecognized XPI board type %d\n",
				      vers.type);
			exit(1);
		}

		if (0 != verbose) {
			printf("\n\tChanging from ");
			xpi_print();
			fflush(stdout);
		}
		fflush(stderr);
		(void)fprintf(stdout,"WARNING: Writing %s EEPROM.\n", ifname);
		(void)fprintf(stderr, "\tDo not reset the system until it "
			      "is finished");
		fflush(stdout);
		/* pause in case the operator does try to abort the process */
		sleep(1);
		fputc(' ',stdout);
		fflush(stdout);
		for (i = 0; i < 2*2; i++) {
			sginap(HZ/2);
			fputc("   ."[i],stdout);
			fflush(stdout);
		}

		/* download the MAC address
		 */
		bzero(&xpi_dwn.val, sizeof(xpi_dwn.val));
		bcopy(&eaddr[0], xpi_dwn.val.l, sizeof(eaddr));
		XPI_MAC_SUM(cksum, xpi_dwn.val.l);
		xpi_dwn.val.l[XPI_MAC_SIZE/4-1] = -cksum;
		xpi_dwn.cnt = XPI_MAC_SIZE/4;
		xpi_dwn.addr = XPI_MAC_ADDR + XPI_DOWN;
		xpi_downflush();

		/* download the code to program the EEPROM
		 */
		if (vers.type == XPI_TYPE_LC) {
			xpi_download(lc_flash_minaddr,
				     &lc_flash_txt[0],
				     &lc_flash_txt[sizeof(lc_flash_txt)]);
		} else {
			xpi_download(mez_flash_minaddr,
				     &mez_flash_txt[0],
				     &mez_flash_txt[sizeof(mez_flash_txt)]);
		}

		/* zap it */
		xpi_dwn.addr = XPI_PROG;
		if (0 > ioctl(rawsoc, SIOC_XPI_EXEC, &xpi_dwn))
			complain("\n","SIOC_XPI_EXEC");

		/* wait until the chip is written
		 */
		for (i = (20*HZ)/(HZ/4); i != 0; i--) {
			x = 0;
			if (0 <= ioctl(rawsoc,SIOC_XPI_SIGNAL,&x))
				break;
			if (errno != EIO)
				complain("\n","SIOC_XPI_SIGNAL");
			fputc('.',stdout);
			fflush(stdout);
			sginap(HZ/4);
		}
		fputc('\n',stdout);
		if (!i)
			complain("","failed to start\n");
		(void)fprintf(stdout,"\tFinished.\n");

		return(0);
	}

	/* must be reading it */
	xpi_print();
	return(0);
}



/* print ipg NVRAM contents
 */
static void
ipg_nvram_print(IPG_NVRAM *nvramp)
{
	int i;
	u_char cksum;


	for (i = 0; i < sizeof(nvramp->addr)-1; i++)
		printf("%x:", nvramp->addr[i]);
	printf("%x", nvramp->addr[i]);

	for (i = 0; i < sizeof(nvramp->zeros); i++) {
		if (nvramp->zeros[i] != 0)
			break;
	}
	IPG_NVRAM_SUM(cksum,nvramp);
	if (cksum != IPG_NVRAM_CKSUM) {
		printf(" bad checksum");
		i = 0;
	}

	if (i < sizeof(nvramp->zeros) || verbose > 1) {
		printf(" cksum=%02X ", nvramp->cksum);
		for (i = 0; i < sizeof(nvramp->zeros); i++)
			printf("%02x", nvramp->zeros[i]);
	}
	printf("\n");
}


/* work on an ipg board
 */
static int
ipg_io(void)
{
	IPG_NVRAM onvram;
	IPG_NVRAM nnvram;
	u_char cksum;
	struct ifreq ifr;

	if (0 > ioctl(rawsoc, SIOC_IPG_NVRAM_GET, &onvram)) {
		complain("","SIOC_IPG_NVRAM_GET");
	}

	/* set the NVRAM if asked */
	if (wflag) {
		if (0 != verbose) {
			printf("changing ");
			ipg_nvram_print(&onvram);
			fflush(stdout);
		}
		bzero(&nnvram, sizeof(nnvram));
		bcopy(&eaddr[0], nnvram.addr,
		       sizeof(nnvram.addr));
		IPG_NVRAM_SUM(cksum,&nnvram);
		nnvram.cksum = IPG_NVRAM_CKSUM-cksum;
		if (0 > ioctl(rawsoc, SIOC_IPG_NVRAM_SET, &nnvram)) {
			complain("","SOIC_IPG_NVRAM_SET");
		}
		sginap(10);		/* let the chip cool */

		if (0 > ioctl(rawsoc, SIOC_IPG_NVRAM_GET, &onvram))
			complain("","SIOC_IPG_NVRAM_GET");

		if (bcmp(&nnvram, &onvram, sizeof(nnvram))) {
			ipg_nvram_print(&onvram);
			(void)fprintf(stderr, "failed to set NVRAM\n");
			return(1);
		}
		return(0);
	}

	/* must be reading it */
	ipg_nvram_print(&onvram);
	return(0);
}



static void
complain(char *s1, char* s2)
{
	fflush(stdout);
	(void)fprintf(stderr, "%s%s: %s: %s\n",
		      s1, pgmname, s2, strerror(errno));
	exit(1);
}


static void
ether_aton(char *a,
	   u_char n[6])
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
		n[i] = o[i];
}
