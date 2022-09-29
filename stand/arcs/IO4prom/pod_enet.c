/*
 * Everest enet bring-up toys
 *
 * several of these functions are called from main.c's command_table
 * Some of this might make into ide or production pod
 * 
 * $Revision: 1.2 $
 *
 * XXX
 * XXX ETHERNET PACKETS MUST BE 60 or more bytes
 * XXX
 */

#include <sys/types.h>
#include <sys/sbd.h>
#include <net/socket.h>
#include <net/in.h>
#include <net/arp.h>

#include <sys/EVEREST/io4.h>
#include <sys/EVEREST/epc.h>
#include <sys/EVEREST/addrs.h>
#include <net/seeq.h>


static int lmap[8] = { 2, 4, 8, 16, 32, 64, 128, 256 };

#define	TBASE_PHYS	ENETBUFS_BASE	/* 1mb aligned */
#define	TBUFS		EPC_256BUFS	/* watch alignment */
#define	RBASE_PHYS	(ENETBUFS_BASE + 256 * EPC_ENET_BUFSZ)
#define	RBUFS		EPC_256BUFS

static u_char eaddr0[6] = { 0x08, 0x00, 0x69, 0x02, 0x23, 0x01 };
static u_char eaddrb[6] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

#ifdef	MIPSEB
static u_char *epcbase  = (u_char *)(SWIN_BASE(EPC_REGION, EPC_ADAPTER) + 4);
#endif

extern int Verbose; 	/* VERBOSE -- print stuff for each reg access +++ */
extern int Debug; 	/* DEBUG -- don't really read or write regs */

#define	SET(r,v)	_eeset(epcbase, (r), (v))
#define	GET(r)		_eeget(epcbase, (r))
/* or...
#define	SET(r,v)	EPC_SET(EPC_REGION, EPC_ADAPTER, r, v)
#define	GET(r)		EPC_GET(EPC_REGION, EPC_ADAPTER, r)
*/

static
_eeset(u_char *epc, int reg, u_int val)
{
	u_int *addr = (u_int *)(epc + reg);


	if (Verbose)
		ttyprintf("0x%x (0x%x) <- 0x%x (%d)\n", addr, reg, val, val);
	if (!Debug)
		*addr = val;
}

static
_eeget(u_char *epc, int reg)
{
	u_int tmp = atoi(getenv("EEGET")), *addr = (u_int *)(epc + reg);

	if (!Debug)
		tmp = *addr;
	if (Verbose)
		ttyprintf("0x%x (0x%x): 0x%x (%d)\n", addr, reg, tmp, tmp);

	return tmp;
}

static u_char e0[6] = { 0x08, 0x00, 0x69, 0x0f, 0x8f, 0x27 }; /* nissen */

static
cpu_get_eaddr(u_char eaddr[])
{
	bcopy(e0, eaddr, sizeof(e0));
}

static u_char eb[6] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

static
cpu_get_baddr(u_char eaddr[])
{
	bcopy(eb, eaddr, sizeof(eb));
}

static pod_enet_transmit(char *);
static u_char *pod_enet_rcv(int rbuf);
static pr_ebuf(u_char *bp, int len);

pod_enet_reset()
{
	SET(EPC_PRSTSET, EPC_ENETRST);
	SET(EPC_PRSTSET, 0);

	us_delay(10);

	SET(EPC_PRSTCLR, EPC_ENETRST);
	SET(EPC_PRSTCLR, 0);

	return 0;
}


static
pod_enet_tinit(u_char *eaddr)
{
	int i;
	/*
	 * set up enough of the hardware to transmit
	 */

	/* set station address */
#define	EPC_PAD	8
	for (i = 0; i < 6; i++)
		SET(EPC_EADDR_BASE + EPC_PAD * i, eaddr[i]);

	/* set address and number of transmit buffers */
	SET(EPC_TBASEHI, 0);
	SET(EPC_TBASELO, TBASE_PHYS);
	SET(EPC_TLIMIT, TBUFS);

	/* notify EPC of any possible EDLC transmit condition */
	SET(EPC_TCMD, SEQ_XC_INTGOOD | SEQ_XC_INT16TRY | SEQ_XC_INTCOLL |
			SEQ_XC_INTUFLOW);
}

/*
 * poke at some EPC/EDLC regs
 * write/read the station address regs of the EDLC to check
 * the command path out there and back
 */
pod_enet_cmd()
{
	int i, tmp;

	for (i = 0; i < 6; i++)
		SET(EPC_EADDR_BASE + EPC_PAD * i, eaddr0[i]);

	for (i = 0; i < 6; i++) {
		tmp = GET(EPC_EADDR_BASE + EPC_PAD * i);
		if (tmp != eaddr0[i])
			ttyprintf("oops! eaddr[%d]=0x%x should be=0x%x\n",
					i, tmp, eaddr0[i]);
	}
	return 0;
}

/*
 * Say hello, charlie
 * (Send garbage out by diddling enet regs directly.)
 * Assumes 1) big endian, 2) IO4 at EPC_REGION, 3) EPC at EPC_ADAPTER,
 * 4) EPC just reset (TTOP == TINDEX == 0)
 */


int nloops;

pod_enet_hello()
{
	nloops = 32;
	pod_enet_tinit(eaddr0);
	pod_enet_transmit("Hallo, enet verden!\n");

	return 0;
}

pod_enet_hellon(int argc, char **argv)
{
	int count;
	char buf[128];

	if (argc == 2)
		count = atoi(*++argv);
	else
		return;

	pod_enet_reset();
	pod_enet_tinit(eaddr0);
	while (count--) {
		sprintf(buf, "Hallo igjen %d\n", count);
		nloops = 32;
		pod_enet_transmit(buf);
	}
	return 0;
}


static
pod_enet_transmit(char *data)
{
	u_char *tbuf;
	struct ether_header *eh;
	int len, count, i;
	int ttop, tindex, tbptr;
	
	/*
	 * let the previous transmit drain (save the fancy stuff for
	 * the driver)
	 */
	while ((ttop = GET(EPC_TTOP)) != (tindex = GET(EPC_TINDEX))) {
		ttyprintf("transmit dma running... (ttop=%d tindex=%d tbptr=%d)\n",
			ttop, tindex, GET(EPC_TBPTR));
		if (nloops-- == 0)
			return;
	}

	/*
	 * find the next buffer
	 */
	tbuf = (u_char*)(PHYS_TO_K0(TBASE_PHYS) + tindex * EPC_ENET_BUFSZ);
	if (Verbose)
		ttyprintf("tindex=%d tbuf=0x%x\n", tindex, tbuf);
	/*
	 * Create an ethernet packet.  Yes, it's too short and of
	 * no known higher level protocol...
	 */

	/*	- first, an ethernet header */
	eh = (struct ether_header *)(tbuf + EPC_ENET_DATA);
	bcopy(eaddrb, eh->ether_dhost, sizeof(ETHADDR));
	bcopy(eaddr0, eh->ether_shost, sizeof(ETHADDR));
	eh->ether_type = 0xaa55;	/* some random garbage */

	/*	- then, some data */
	strcpy(tbuf + EPC_ENET_DATA + sizeof(struct ether_header), data);

	/* Write in packet len for EPC */
	len = strlen(data) + sizeof(struct ether_header);
	tbuf[0] = len & 0xff;		/* LSB */
	tbuf[1] = (len >> 8) & 0xff;	/* MSB */

	/* XXX write back and invalidate if needed to debug... */
	/* __dcache_wb(tbuf, EPC_ENET_BUFSZ); */

	/*
	 * Kick off the DMA!
	 */
	if (Verbose)
		ttyprintf("len=%d TBPTR=%d\n", len, GET(EPC_TBPTR));
	if (++tindex == EPC_LIMTON(TBUFS))
		tindex = 0;
	SET(EPC_TTOP, tindex);

	if (Debug)	/* ain't nuttin' gonna happen... */
		return;

	/* Spin watching it drain out... */
	while ((count = GET(EPC_TBPTR)) < len + 2)
		if (nloops-- == 0)	/* ... but only for a while */
			return;
	while (GET(EPC_TINDEX) == tindex)
		;

	/* Snarf out the transmit buffer status */
	ttyprintf("tbuf stat=%x\n", *(u_int *)(tbuf + EPC_ENET_STAT));
}

static
pod_enet_rinit(u_char *eaddr)
{
	int i;

	/* program EDLC: set station address */
	for (i = 0; i < 6; i++)
		SET(EPC_EADDR_BASE + EPC_PAD * i, eaddr[i]);

	/* program rcv DMA engine: set address and number of rbufs */
	SET(EPC_RBASEHI, 0);
	SET(EPC_RBASELO, RBASE_PHYS);
	SET(EPC_RLIMIT, RBUFS);

	/* program EDLC->EPC rcv interrupts */
	SET(EPC_RCMD, SEQ_RC_INTGOOD | SEQ_RC_INTEND | SEQ_RC_INTSHORT |
                SEQ_RC_INTDRBL | SEQ_RC_INTCRC | SEQ_RC_RSB);

	/* open the flood gates! */
	SET(EPC_RTOP, EPC_LIMTON(RBUFS) - 1);
}

/*
 * Wait for the specified rbuf to arrive
 */
static u_char *
pod_enet_rcv(int rbuf)
{
	u_char *bp = (u_char *)(PHYS_TO_K0(RBASE_PHYS) + rbuf * EPC_ENET_BUFSZ);

	if (Verbose)
		ttyprintf("waiting for rbuf %d, rindex = %d ...\n",
			rbuf, GET(EPC_RINDEX));
	if (Debug)
		return bp;
	
	while (GET(EPC_RINDEX) != rbuf + 1) {
		if (Verbose)
			ttyprintf("rbptr=%d\n", GET(EPC_RBPTR));
	}
	return bp;
}

/*
 * Init the rcv DMA engine, listen for one packet, print its contents
 */
pod_enet_read()
{
	u_char *rbuf;

	pod_enet_rinit(eaddr0);
/*
	ttyprintf("pod_enet_rcv() listening...\n");
	rbuf = pod_enet_rcv(0);
	pr_ebuf(rbuf, 0);
*/

	return 0;
}

pod_enet_readn(int argc, char **argv)
{
	int index, count;
	u_char *rbuf;

	if (argc == 2)
		count = atoi(*++argv);
	else
		return;

	pod_enet_reset();
	pod_enet_rinit(eaddr0);

	for (index = 0; index < count; index++) {
		rbuf = pod_enet_rcv(index);
		pr_ebuf(rbuf, 0);
	}
	return 0;
}

pod_pr_ebuf(int argc, char **argv)
{
	u_char *addr;
	int len = 0;

	if (argc > 1) {
		atohex(*++argv, &addr);
		if (argc == 3)
			len = atoi(*++argv);
	} else
		return;

	pr_ebuf(addr, len);

	return 0;
}

static
pr_ebuf(u_char *bp, int alen)
{
	struct ether_header *eh;
	char white;
	u_int rstat;
	int len, col;

	ttyprintf("0x%x:\n", bp);
	rstat = *(u_int *)(bp + EPC_ENET_STAT);
	if (alen)
		len = alen;
	else
		len = rstat & EPC_ENET_LENMASK;
	ttyprintf("rstat=0x%x (len=%d)\n", rstat, len);

	eh = (struct ether_header *)(bp + EPC_ENET_DATA);
	ttyprintf("dst %s\t", ether_sprintf(eh->ether_dhost));
	ttyprintf("src %s\t", ether_sprintf(eh->ether_shost));
	ttyprintf("type %x\n", eh->ether_type);

	bp = bp + EPC_ENET_DATA + sizeof(struct ether_header);
	white = ' ';
	for (col = 1; len; bp++, len--, col++) {
		if (col % 16 == 0)
			white = '\n';
		if (*bp < 0x10)	/* i.e., 1 hex digit... */
			ttyprintf("0%x%c", *bp, white);
		else
			ttyprintf("%x%c", *bp, white);
		if (white == '\n')
			white = ' ';
	}
	if (white == ' ')
		ttyprintf("\n");
}


/*
 * dump out all the EPC enet regs
 */
pod_enet_stat()
{
	GET(EPC_EADDR0);
	GET(EPC_EADDR1);
	GET(EPC_EADDR2);
	GET(EPC_EADDR3);
	GET(EPC_EADDR4);
	GET(EPC_EADDR5);
	GET(EPC_TCMD);
	GET(EPC_RCMD);
	ttyprintf("\n");

	GET(EPC_TBASELO);
	GET(EPC_TBASEHI);
	GET(EPC_TLIMIT);
	GET(EPC_TINDEX);
	GET(EPC_TTOP);
	GET(EPC_TBPTR);
	GET(EPC_TSTAT);
	GET(EPC_TITIMER);
	ttyprintf("\n");

	GET(EPC_RBASELO);
	GET(EPC_RBASEHI);
	GET(EPC_RLIMIT);
	GET(EPC_RINDEX);
	GET(EPC_RTOP);
	GET(EPC_RBPTR);
	GET(EPC_RSTAT);
	GET(EPC_RITIMER);
	ttyprintf("\n");

	GET(EPC_LXT_STAT);
	GET(EPC_LXT_COLL);
	GET(EPC_LXT_XCOLL);
	GET(EPC_LXT_SQCOLL);
	ttyprintf("\n");

	GET(EPC_IERR);
	return 0;
}

#include <arcs/folder.h> /* IOBLOCK */
#include <net/mbuf.h>
#include <net/ei.h>

pod_eehello()
{
	IOBLOCK io0;
	struct ether_info ei;
	struct ifnet ifn;
	struct mbuf *m1, *m2, *m3, *m4, *m5;
	struct sockaddr dst;
	struct ether_header *eh;

	if (Verbose) ttyprintf("pod_eehello()\n");

	m1 = _m_get(0,0);
	m1->m_len = 5; /* yes, too short, but the h/w will still xmt and rcv */
	m1->m_off = 0;
	strcpy(m1->m_dat, "hello");

	m2 = _m_get(0,0);
	m2->m_len = 129;	/* 4 * 32 + 1 */
	m2->m_off = 0;
	strcpy(m2->m_dat, "enet");
	m2->m_dat[128] = '.';

	m3 = _m_get(0,0);
	m3->m_len = 100;	/* 3 * 32 + 4 */
	m3->m_off = 0;
	strcpy(m3->m_dat, "world");
	m3->m_dat[99] = ',';

	m4 = _m_get(0,0);
	m4->m_len = 99;		/* 3 * 32 + 3 */
	m4->m_off = 0;	
	strcpy(m4->m_dat, "how is the air out there?");
	m4->m_dat[98] = ':';

	m5 = _m_get(0,0);
	m5->m_len = 46;		/* 1 * 32 + 24 */
	m5->m_off = 0;
	strcpy(m5->m_dat, "the end");
	m5->m_dat[45] = 'X';

	dst.sa_family = AF_UNSPEC;	/* XXX skips ARP */
	eh = (struct ether_header *)dst.sa_data;
	eh->ether_type = ETHERTYPE_IP;
	cpu_get_baddr(eh->ether_dhost);
	cpu_get_eaddr(eh->ether_shost);

	if (Verbose) ttyprintf("ee_init()\n");
	ee_init();
	io0.Unit = 0;
	io0.DevPtr = &ei;	/* normally malloc'ed in if.c */
	if (Verbose) ttyprintf("ee_open()\n");
        ee_open(&io0);

	ifn.if_unit = 0;
	if (Verbose) ttyprintf("ee_output()\n");
	ei.ei_acp->ac_if.if_output(&ifn, m1, &dst);
	if (Verbose) ttyprintf("ee_output()\n");
	ei.ei_acp->ac_if.if_output(&ifn, m2, &dst);
	if (Verbose) ttyprintf("ee_output()\n");
	ei.ei_acp->ac_if.if_output(&ifn, m3, &dst);
	if (Verbose) ttyprintf("ee_output()\n");
	ei.ei_acp->ac_if.if_output(&ifn, m4, &dst);
	if (Verbose) ttyprintf("ee_output()\n");
	ei.ei_acp->ac_if.if_output(&ifn, m5, &dst);

	return 0;
}

/*
 * Send an ARP request.  Wait around for an ARP response.
 * Call driver directly.  (Call arp.c code instead?)
 */
pod_arp(int argc, char **argv)
{
	IOBLOCK io0;
	struct ether_info ei;
	struct ether_header *eh;
	struct in_addr ip, inet_addr();

	if (Verbose) ttyprintf("pod_arp()\n");

	if (argc != 2)
		return 1;
	
	++argv;

	if (Verbose) 	ttyprintf("ipaddr=%s\n", *argv);
	ip = inet_addr(*argv);
	if (Verbose) ttyprintf("... = 0x%x\n", ip.s_addr);

	if (Verbose) ttyprintf("ee_init()\n");
	ee_init();
	io0.Unit = 0;
	io0.DevPtr = &ei;	/* normally malloc'ed in _ifopen() */
	if (Verbose) ttyprintf("ee_open()\n");
        ee_open(&io0);

	if (Verbose) ttyprintf("arpwhohas()\n");
	arpwhohas(ei.ei_acp, &ip);

	return 0;
}

pod_putc(char c)
{
	if (c == '\n')
		pul_cc_putc('\r');

	pul_cc_putc(c);
}
