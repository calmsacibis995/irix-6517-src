/* Poke at E-Plex memory
 *
 * Copyright 1994 Silicon Graphics, Inc.  All rights reserved.
 */

#ident "$Revision: 1.9 $"

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <bstring.h>
#include <errno.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/param.h>
#include <net/raw.h>
#include <net/if.h>
#include <netinet/if_ether.h>
#include <cap_net.h>
#include "if_ep.h"

char *pgmname;

#define EP_NAME "ep"
static char ifname[sizeof(EP_NAME)+3];
static int ifnum;
static struct sockaddr_raw sr;
static int rawsoc;

static u_int snum, base;

/* values copied manually from ep.lst
 */
#define sregs0 0x86bf0
#define _SIZE_SGRAM 0x1000

#define NUM_RX_BUFS 64
#define _RXpkt	    0x14
#define _RXrsrc	    0x00834
#define _TXbufs	    0x18ab8
#define TX_PAD	    2
#define _SIZE_TXpkt 0x20
#define TXBUF_SIZE  0x60c


#define GIORAM	0x080000
#define SONICRAM_BASE 0x100000
#define SONICRAM_SIZE 0x20000

/* common 29K registers */
static struct {
	u_int isr;
	u_int rda;
	u_int rx_link;
	u_int rrp;
	u_int rbuf;
	u_int d2b_len;
	u_int d2b_head;
	u_int d2b_tail;
	u_int obf;
	u_int obf_avail;
	u_int obf_end;
	u_int tx_head;
	u_int tx_link;
	u_int out_done;
	u_int sn_st;
} r29k;

/* common SONIC registers */
static struct {
	u_int rda;
	u_int rwp;
	u_int rrp;
	u_int tda;
} rson;


static void
usage(void)
{
	fflush(stdout);
	(void)fprintf(stderr, "%s: %s\n", pgmname,
		      "usage: {-g addr[,cnt[,width]] | -p addr,val\n"
		      "\t| -s n | -r n | -t n}"
		      " [interface]");
	exit(1);
}

static void
complain(char *s1, char* s2)
{
	fflush(stdout);
	(void)fprintf(stderr, "%s%s: %s %s: %s\n",
		      s1, pgmname, s2, ifname, strerror(errno));
	fflush(stderr);
}


static u_int
fetch(u_int addr)
{
	static struct ep_dwn ep_dwn;

	if (addr >= ep_dwn.addr
	    && addr < ep_dwn.addr + ep_dwn.cnt*4)
		return ep_dwn.val.l[(addr-ep_dwn.addr)/4];

	bzero(&ep_dwn, sizeof(ep_dwn));

	ep_dwn.addr = addr;
	/* fetch extra bytes if not a hardware register
	 */
	ep_dwn.cnt = ((addr >= GIORAM
		       && addr+EP_DWN_LEN*4 < SONICRAM_BASE+EP_PORTS*0x20000)
		      ? EP_DWN_LEN
		      : 1);
	if (0 > cap_network_ioctl(rawsoc, SIOC_EP_FET, &ep_dwn)) {
		complain("","SIOC_EP_FET");
		exit(1);
	}
	return ep_dwn.val.l[0];
}

static u_int
fetch16(u_int addr)
{
	return fetch(addr) & 0xffff;
}

static u_int
fetch2w(u_int addr1,
	u_int addr2)
{
	return ((fetch(addr1) & 0xffff)*0x10000
		+ (fetch(addr2) & 0xffff));
}


/* fetch common values at once to make them more consistent
 */
static void
fetchr29k(void)
{
	r29k.isr = fetch(0*4+snum*_SIZE_SGRAM+sregs0);
	r29k.rda = fetch(1*4+snum*_SIZE_SGRAM+sregs0);
	r29k.rx_link = fetch(2*4+snum*_SIZE_SGRAM+sregs0);
	r29k.rrp = fetch(3*4+snum*_SIZE_SGRAM+sregs0);
	r29k.rbuf = fetch(4*4+snum*_SIZE_SGRAM+sregs0);
	r29k.d2b_len = fetch(5*4+snum*_SIZE_SGRAM+sregs0);
	r29k.d2b_head = fetch(6*4+snum*_SIZE_SGRAM+sregs0);
	r29k.d2b_tail = fetch(7*4+snum*_SIZE_SGRAM+sregs0);
	r29k.obf = fetch(8*4+snum*_SIZE_SGRAM+sregs0);
	r29k.obf_avail = fetch(9*4+snum*_SIZE_SGRAM+sregs0);
	r29k.tx_head = fetch(10*4+snum*_SIZE_SGRAM+sregs0);
	r29k.tx_link = fetch(11*4+snum*_SIZE_SGRAM+sregs0);
	r29k.out_done = fetch(12*4+snum*_SIZE_SGRAM+sregs0);
	r29k.sn_st = fetch(13*4+snum*_SIZE_SGRAM+sregs0);

	r29k.obf_end = fetch(15*4+snum*_SIZE_SGRAM+sregs0);
}


static void
store(u_int addr, u_int val)
{
	struct ep_dwn ep_dwn;

	bzero(&ep_dwn, sizeof(ep_dwn));

	ep_dwn.addr = addr;
	ep_dwn.cnt =1;
	ep_dwn.val.l[0] = val;

	if (0 > cap_network_ioctl(rawsoc, SIOC_EP_POKE, &ep_dwn)) {
		complain("","SIOC_EP_POKE");
		exit(1);
	}
}


static u_int				/* 0 or next address */
print_txpkt(u_int addr)
{
	u_int stat, conf, psize, fcnt, fptr, fsize, link;
	char fcnt_str[10], fptr_str[10], fsize_str[10];
	u_int nxt;

	stat = fetch(addr+0x00);	/* _TXpkt_status */
	conf = fetch(addr+0x04);	/* _TXpkt_config */
	psize = fetch(addr+0x08);	/* _TXpkt_psize */
	fcnt = fetch(addr+0x0c);	/* _TXpkt_frags */
	sprintf(fcnt_str,"%d?",fcnt);
	fptr = fetch2w(addr+0x14,	/* _TXpkt_ptr_hi */
		       addr+0x10),	/* _TXpkt_ptr_lo */
	sprintf(fptr_str,"%x?",fptr);
	fsize = fetch(addr+0x18);	/* _TXpkt_fsize */
	sprintf(fsize_str,"%x?",fsize);
	link = fetch(addr+0x1c);	/* _TXpkt_link */

#if 0
#define PTST && 0
#else
#define PTST
#endif
#define TXPKT_HEADING "		   stat conf siz fcnt fptr fsiz    link"
	printf("%3s"	      " %6x: %4x %4x %3x%c %3s %5s" " %4s",
	       addr == (rson.tda & ~1) ? "TDA" : "",
	       addr, stat, conf, psize,
	       ((psize >= 60 && psize <= 1514 PTST) ? ' ' : '?'),
	       fcnt == 1 PTST ? "" : fcnt_str,
	       fptr == addr+_SIZE_TXpkt+TX_PAD PTST ? "" : fptr_str,
	       fsize == psize PTST ? "" : fsize_str);

	nxt = (link & 0xfffe) + (base & ~0xffff);
	if (nxt != addr + ((psize+_SIZE_TXpkt+TX_PAD+3) & ~3)
	    && nxt != base) {
		printf(" %6x? bad link address\n", link);
		return 0;
	}

	if (nxt >= r29k.obf_end) {
		printf(" %6x? link address past end\n", link);
		return 0;
	}

	if ((link & 1) != 0) {
		printf(" %6x EOL\n", link);
		if (r29k.tx_link != addr+0x1c)
			printf("\ntx_link=%x, end=%x\n",
			       r29k.tx_link, addr);
		return 0;
	}

	printf("\n");
	return nxt;
}


void
main(int argc,
     char* argv[])
{
	u_int addr, val, sreg, cur, hi;
	int cnt;
	int width;
	enum {
		UNSPEC,
		GET,			/* dump memory */
		PUT,			/* change memory */
		SONIC,			/* SONIC registers */
		RX,			/* input */
		TX			/* output */
	} type;
	int i, j;
	char *p;
	char obuf[BUFSIZ + 8];

	/* Try write everything in a single block to minimize time
	 * spent between fetches from the board.
	 */
	setvbuf(stdout, obuf, _IOFBF, BUFSIZ);

	pgmname = argv[0];

	type = UNSPEC;
	while ((i = getopt(argc, argv, "d:g:p:s:r:t:")) != EOF)
		switch (i) {
		case 'd':
		case 'g':
			type = GET;
			addr = strtoul(optarg,&p,16);
			cnt = 1;	/* default count and width */
			width = 8;
			if (*p == '\0')
				break;
			if (*p != ',')
				usage();
			cnt = (int)strtoul(p+1,&p,16);
			if (cnt <= 0)
				usage();
			if (*p == '\0')
				break;
			if (*p != ',')
				usage();
			width = (int)strtoul(p+1,&p,16);
			if (width <= 0 || *p != '\0')
				usage();
			break;

		case 'p':
			type = PUT;
			addr = strtoul(optarg,&p,16);
			if (*p != ',')
				usage();
			val = strtoul(p+1,&p,16);
			if (*p != '\0')
				usage();
			break;

		case 's':
			type = SONIC;
			snum = strtoul(optarg,&p,0);
			if (*p != '\0' || snum > EP_PORTS)
				usage();
			break;

		case 'r':
			type = RX;
			snum = strtoul(optarg,&p,0);
			if (*p != '\0' || snum > EP_PORTS)
				usage();
			break;

		case 't':
			type = TX;
			snum = strtoul(optarg,&p,0);
			if (*p != '\0' || snum > EP_PORTS)
				usage();
			break;

		default:
			usage();
			break;
	}
	if (argc != optind) {
		if (argc != optind+1)
			usage();
		if (1 != sscanf(argv[optind], EP_NAME "%d", &ifnum)) {
			(void)fprintf(stderr,
				      "%s: unrecognized interface: %s\n",
				      pgmname, argv[optind]);
			exit(1);
		}
		if (ifnum >= EP_MAXBD
		    || 0 != (ifnum % EP_PORTS_OCT)) {
			(void)fprintf(stderr, "%s: invalid interface unit "
				      "number %d\n",
				      pgmname, ifnum);
			exit(1);
		}
	}
	if (type == UNSPEC)
		usage();

	(void)sprintf(ifname, EP_NAME "%d", ifnum);

	/* open the raw socket to talk to the board.
	 */
	rawsoc = cap_socket(AF_RAW, SOCK_RAW, RAWPROTO_SNOOP);
	if (rawsoc < 0) {
		complain("","socket");
		return;
	}

	bzero((char*)&sr, sizeof(sr));
	(void)strncpy(sr.sr_ifname, ifname, sizeof(sr.sr_ifname));
	sr.sr_family = AF_RAW;
	sr.sr_port = 0;
	if (0 > bind(rawsoc, &sr, sizeof(sr))) {
		complain("","bind");
		(void)close(rawsoc);
		return;
	}

	switch (type) {
	case GET:
		/* fetch words
		 */
		j = 0;
		while (cnt-- > 0) {
			if ((j % width) == 0) {
				if (j != 0)
					putc('\n',stdout);
				printf("%6x:", addr);
			}
			printf("%9x", fetch(addr));

			j++;
			addr += 4;
		}
		putc('\n',stdout);
		break;

	case PUT:
		/* store a word
		 */
		store(addr,val);
		break;

	case SONIC:
		sreg = 0x050000+snum*0x100; /* base of SONIC registers */
		printf("GSOR=%08x"
		       "\n  CR=%04x  RCR=%04x  TCR=%04x  IMR=%04x  ISR=%04x",
		       fetch(0x04001c),
		       fetch(sreg+0x00*4),
		       fetch(sreg+0x02*4),
		       fetch(sreg+0x03*4),
		       fetch(sreg+0x04*4),
		       fetch(sreg+0x05*4));

		printf("\nUTDA=%04x CTDA=%04x",
		       fetch(sreg+0x06*4),
		       fetch(sreg+0x07*4));

		printf("\nURDA=%04x CRDA=%04x URRA=%04x  RSA=%04x  "
		       "REA=%04x  RRP=%04x  RWP=%04x  RSC=%04x",
		       fetch(sreg+0x0d*4),
		       fetch(sreg+0x0e*4),
		       fetch(sreg+0x14*4),
		       fetch(sreg+0x15*4),
		       fetch(sreg+0x16*4),
		       fetch(sreg+0x17*4),
		       fetch(sreg+0x18*4),
		       fetch(sreg+0x2b*4));

#ifdef NOTDEF
		printf("\n CEP=%04x CAP2=%04x CAP1=%04x CAP0=%04x"
		       "   CE=%04x  CDP=%04x  CDC=%04x CRCT=%04x ",
		       fetch(sreg+0x23*4),
		       fetch(sreg+0x21*4),
		       fetch(sreg+0x22*4),
		       fetch(sreg+0x24*4),
		       fetch(sreg+0x25*4),
		       fetch(sreg+0x26*4),
		       fetch(sreg+0x27*4),
		       fetch(sreg+0x2c*4));
#endif

		printf("\nFAET=%04x  MPT=%04x ",
		       fetch(sreg+0x2d*4),
		       fetch(sreg+0x2e*4));
#ifdef NOTDEF
		printf(" WT0=%04x  WT1=%04x   SR=%04x ",
		       fetch(sreg+0x29*4),
		       fetch(sreg+0x2a*4),
		       fetch(sreg+0x28*4));
#endif
		putc('\n',stdout);
		break;


	case RX:
		sreg = 0x050000+snum*0x100; /* base of SONIC registers */

		fetchr29k();
		hi = fetch16(sreg+0x0d*4)*0x10000;
		rson.rda = hi+fetch16(sreg+0x0e*4);

		printf("29K   isr=%04x  rda=%x rx_link=%x rbuf=%x"
		       " sn_st=%04x\n",
		       r29k.isr,r29k.rda,r29k.rx_link,r29k.rbuf, r29k.sn_st);
		printf("SONIC RCR=%04x  RDA=%x\n",
		       fetch(sreg+0x02*4), rson.rda);

		base = _RXpkt+snum*SONICRAM_SIZE+SONICRAM_BASE;
		addr = base;
		printf("	 stat  len ptr    seq      \t"
		       "    stat  len ptr    seq      \n");
		for (i = 0; i < NUM_RX_BUFS; i++) {
			if (!(i&1))
				printf("%3x: ", addr % 0x1000);
			if (rson.rda == addr)
				fputs("  s>", stdout);
			else if (r29k.rda == addr)
				fputs("29k>", stdout);
			else
				fputs("    ", stdout);

			printf("%4x %4x %6x %4x ",
			       fetch16(addr+0*4),
			       fetch16(addr+1*4),
			       fetch2w(addr+3*4, addr+2*4),
			       fetch16(addr+4*4));

			cur = hi+fetch16(addr+5*4);
			if ((cur & 0xfffffe) != addr+7*4
			    && ((cur & 0xfffffe) != base
				&& i == NUM_RX_BUFS-1))
				printf("%x ", cur);

			fputs((cur & 1) ? "EOL " : "    ", stdout);

			switch (cur = fetch16(addr+6*4)) {
#ifdef NOTDEF
			case 0:  putc('B',stdout); break;
			case 1:  putc('F',stdout); break;
#endif
			default: printf("%x", cur); break;
			}

			addr += 8*4;
			fputc(((i % 2) == 1) ? '\n' : '\t',
			      stdout);
		}

		hi = fetch(sreg+0x14*4)*0x10000;
		printf("\nRSA=%x REA=%x   SONIC RRP=%x RWP=%x   29K rrp=%x\n",
		       hi+fetch(sreg+0x15*4),
		       hi+fetch(sreg+0x16*4),
		       rson.rrp = hi+fetch(sreg+0x17*4),
		       rson.rwp = hi+fetch(sreg+0x18*4),
		       r29k.rrp);
		addr = _RXrsrc+snum*SONICRAM_SIZE+SONICRAM_BASE;
		for (i = 0; i < NUM_RX_BUFS; i++) {
			if (rson.rrp == addr && rson.rwp == addr)
				fputs(" rw",stdout);
			else if (rson.rrp == addr)
				fputs("  r",stdout);
			else if (rson.rwp == addr)
				fputs("  w",stdout);
			else
				fputs("   ",stdout);

#ifdef NOTDEF
			cur = fetch16(addr+1*4)*0x10000;
			printf("%4x",
			       fetch16(addr+0*4) + ((cur != hi) ? cur : 0));
#else
			printf("%6x", fetch2w(addr+1*4,addr+0*4));
#endif

			cur = fetch2w(addr+3*4,addr+2*4);
			if (cur != 0x2fd)
				printf(" %4x  ", cur);

			addr += 4*4;
			if ((i % 8) == 7)
				fputc('\n', stdout);
		}
		break;

	case TX:
		sreg = 0x050000+snum*0x100; /* base of SONIC registers */

		fetchr29k();
		rson.tda = fetch(sreg+0x06*4)*0x10000+fetch(sreg+0x07*4);

		printf("29K    isr=%04x sn_st=%04x"
		       "   d2b_len=%x d2b_head=%x d2b_tail=%x\n",
		       r29k.isr, r29k.sn_st,
		       r29k.d2b_len, r29k.d2b_head, r29k.d2b_tail);
		printf("       obf=%x obf_avail=%x obf_end=%x\n",
		       r29k.obf, r29k.obf_avail, r29k.obf_end);
		printf("       tx_head=%x tx_link=%x out_done=%x\n",
		       r29k.tx_head, r29k.tx_link, r29k.out_done);
		printf("SONIC TCR=%04x   TDA=%-6x\n",
		       fetch(sreg+0x03*4), rson.tda);

		base = _TXbufs+snum*SONICRAM_SIZE+SONICRAM_BASE;
		printf("\n" TXPKT_HEADING "\n");
		addr = base;
		do {
			addr = print_txpkt(addr);
		} while (addr != 0 && addr != base && addr != r29k.obf);

		if ((rson.tda & ~1) > r29k.obf) {
			printf("\n");
			addr = (rson.tda & ~1);
			if (addr < base || addr >= base+SONICRAM_SIZE) {
				printf("invalid TDA=%x\n", rson.tda);
				break;
			}
			do {
				addr = print_txpkt(addr);
			} while (addr != 0 && addr != base);
		}
		break;
	}
}
