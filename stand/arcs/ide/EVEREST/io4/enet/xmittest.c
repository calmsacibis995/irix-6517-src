/*
 * xmittest.c - EPC/SEEQ xmit test
 *	Transmits ethernet packets in loopback mode and verifies that the
 *	received packet is the same as the transmitted packet.
 *
 * WARNING!!! Due to dependencies on data structures hidden in the file
 *
 *		 arcs/lib/libsk/net/if_ee.c
 *
 * this test is rather fragile.  The data structures in the ide include
 * file epc_ide.h must mimic those in if_ee.c exactly or problems can
 * result.  *DO NOT CALL THE EXTERNALLY VISABLE ee_init() ROUTINE* - it
 * will do Bad Things by reinitializing needed data structures.
 *
 * The ee_clearhang() routine is a simplified version of that found in the
 * kernel - the original lives in irix/kern/bsd/mips/if_ee.c   This entire
 * section of the code is rather nasty, and empirically derived.  If the
 * clearhang routine is broken, this test may hang the ethernet badly enough
 * to require a system powercycle - not just a reset - to clear up.
 *
 * 		danac - 12/9/93
 */
#include <sys/types.h>
#include <sys/sbd.h>
#include <sys/EVEREST/epc.h>
#include <sys/EVEREST/io4.h>
#include <ide_msg.h>
#include <everr_hints.h>
#include <setjmp.h>
#include <sys/time.h>
#include <math.h>
#include <net/in.h>
#include <net/socket.h>
#include <net/arp.h>
#include <net/ei.h>
#include <epc_ide.h>
#include <net/seeq.h>

extern int *Reportlevel;
extern struct ee_info_ide *ei;
#define ei_if   ei_ac.ac_if
#define ei_addr ei_ac.ac_enaddr

/* WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING */
/*
 * XXX SEQ_RC_OFLOW works but has not had the same stress testing
 * XXX as the interrupts selected
 */
#define EE_RCMD (SEQ_RC_INTGOOD | SEQ_RC_INTEND | \
                SEQ_RC_INTSHORT | SEQ_RC_INTDRBL | SEQ_RC_INTCRC)



#define XMIT_TIMEOUT	0x8ffff
#define RCV_TIMEOUT	0x8ffff

#define MAX_TLIMIT	255

#define	EE_BUFSIZE	2048
#define	EE_TBUFS2	0
#define EADDR_SIZE	6

/* type of patterns to test */
#define WALK0		0
#define WALK1		1
#define AFIVEA		2	
#define FZEROF		3
#define ALLFIVE		4
#define	NUMPATTERNS	5	

#define INCRSE		5	
#define RNDOM		6
#define	NUMRANDOM	25 /* number of random patterns to test */
#define MAXNORMSIZE	1500 /* max normal size data bytes in packet */
#define LONGSIZE1	130 /* a packet size */

#define TINDEX_TOOBIG   0x69

/*
/* typedef struct ee_info {
/*        struct arpcom   ei_ac;
/*
/*        int             ei_rbufs;       /* # receive bufs (RLIMIT) */
/*        caddr_t         ei_rbase;       /* base of rbufs */
/*        int             ei_orindex;     /* prev RINDEX */
/*        caddr_t         ei_epcbase;
/*
/*        int             ei_tbufs;       /* # transmit bufs (TLIMIT) */
/*        caddr_t         ei_tbase;       /* base of tbufs */
/*        int             ei_tfree;       /* 1st open tbuf */
/*        int             ei_tdmawait;    /* dirty tbuf not queued for tmit */
/* } ee_info;
*/

char bufs[8*EE_BUFSIZE]; /* enough for 2 4K aligned pairs of bufs */

static char ee1[6] = { 0xff, 0xfe, 0xfd, 0xfb, 0xf7, 0xff };
static char ee5[6] = { 0x0, 0x1, 0x2, 0x3, 0x4, 0x5 };

char *bp, *rbp, *sbp, *rbuf, *tbuf;
int tlen;
int packetnum = 0;

/* copied from net/if_ee.c */
int ee_reset()
{
	long long t1, t2;
        /* XXX make sure DMA engines are off */
        /*     warning: "decrementing" R/TINDEX could hang dma engine(s) */

        /* assert reset to EDLC... */
        EPC_SETW(EPC_PRSTSET, EPC_PRST_EDLC);

	/* sync before starting delay */
	EPC_GETW(EPC_PRST);
	t1 = EPC_GETW(EPC_PRST);

        /* ...for 20us... */
        us_delay(20);

        /* ...then clear */
        EPC_SETW(EPC_PRSTCLR, EPC_PRST_EDLC);

	/* sync again before taking stats*/
	EPC_GETW(EPC_PRST);
	t2 = EPC_GETW(EPC_PRST);

	/*
	 * msg_printf calls done after to avoid mucking up timing
	 */

	/* check to see if reset to controller was set */
	msg_printf(DBG, "\nEthernet Controller %s reset\n",
		    (t1 & EPC_PRST_EDLC) ? "was" : "wasn't");

	/* check to see if reset to controller was set */
	msg_printf(DBG, "Ethernet Controller reset %s cleared on exit\n",
		    (t2 & EPC_PRST_EDLC) ? "wasn't" : "was");

}

int xmit(int slot, int adap)
{
	int i;
	int fail = 0;
	long long imset, rcmd_orig, tcmd_orig, tbasehi_orig, tbaselo_orig;
	long long tlimit_orig, rbasehi_orig, rbaselo_orig, rlimit_orig;
	long long rtop_orig;
	long long eaddr_orig[EADDR_SIZE], eaddr2_orig[EADDR_SIZE];

	/* 2 bufs requires 4k alignment */
	rbuf = (char*)((((paddr_t)bufs + 2 * EE_BUFSIZE) >> 12 ) << 12);
	tbuf = (char*)((paddr_t)rbuf + 2 * EE_BUFSIZE); 
/*
	rbuf = (char*)K1_TO_K0((((paddr_t)bufs + 2 * EE_BUFSIZE) >> 12 ) << 12);
	tbuf = (char*)((paddr_t)rbuf + 2 * EE_BUFSIZE); 
*/

	msg_printf(DBG, "rbuf: 0x%x, tbuf: 0x%x\n", rbuf, tbuf);

	for (i = 0; i < 2* EE_BUFSIZE; i++) {
		rbuf[i] = 0x67;
		tbuf[i] = 0x98;
	}

	ee_reset();

	/* make sure IMSET[6] is not set -- for EPC rcv interrupt bug */
	imset = EPC_GET(EPC_REGION, EPC_ADAPTER, EPC_IMSET);
	if (imset & EPC_INTR_UNUSED) {
		msg_printf(DBG,"imset bit 6 is set, imset is 0x%llx\n",imset);
		EPC_SET(EPC_REGION, EPC_ADAPTER, EPC_IMSET, imset & ~EPC_INTR_UNUSED);
		EPC_GET(EPC_REGION, EPC_ADAPTER, EPC_IMSET);
	}

	packetnum = 0; /* reset each time xmit is run */

	/*
	 * Configure ee1 as for transmitting (only?)
	 */

	/* get enet address of machine running this test */
	cpu_get_eaddr(ee1);

	/* set station address */
	for (i = 0; i < EADDR_SIZE; i++) {
		eaddr_orig[i] = EPC_GET(1, 1, EPC_EADDR_BASE + 8*i);
		EPC_SET(1, 1, EPC_EADDR_BASE + 8*i, ee1[i]);
	}

	/* set address of transmit buffers */
	/* we just know this is so */
	tbasehi_orig = EPC_GET(EPC_REGION, EPC_ADAPTER, EPC_TBASEHI); 
	tbaselo_orig = EPC_GET(EPC_REGION, EPC_ADAPTER, EPC_TBASELO);
	EPC_SET(EPC_REGION, EPC_ADAPTER, EPC_TBASEHI, 0x0); 
	EPC_SET(EPC_REGION, EPC_ADAPTER, EPC_TBASELO, 
			K1_TO_PHYS(tbuf) & 0xffffffff);

	/* set number of transmit buffers */
	tlimit_orig = EPC_GET(EPC_REGION, EPC_ADAPTER, EPC_TLIMIT);
	EPC_SET(EPC_REGION, EPC_ADAPTER, EPC_TLIMIT, EE_TBUFS2);

	/*
	 * Configure ee5 for receiving (only?)
	 */

	/* get address of 'destination' */
	cpu_get_eaddr(ee5);

	/* 	- set station address */
	for (i = 0; i < EADDR_SIZE; i++) {
	   eaddr2_orig[i] =EPC_GET(EPC_REGION,EPC_ADAPTER,EPC_EADDR_BASE+8*i);
	   EPC_SET(EPC_REGION, EPC_ADAPTER, EPC_EADDR_BASE + 8*i, ee5[i]);
	}

	/* 	- set address of receive buffers */
	rbasehi_orig = EPC_GET(EPC_REGION, EPC_ADAPTER, EPC_RBASEHI); 
	rbaselo_orig = EPC_GET(EPC_REGION, EPC_ADAPTER, EPC_RBASELO);
	EPC_SET(EPC_REGION, EPC_ADAPTER, EPC_RBASEHI, 0x0); 
	EPC_SET(EPC_REGION, EPC_ADAPTER, EPC_RBASELO, 
			K1_TO_PHYS(rbuf) & 0xffffffff);

	/* 	 - set number of receive buffers */
	rlimit_orig = EPC_GET(EPC_REGION, EPC_ADAPTER, EPC_RLIMIT);
	EPC_SET(EPC_REGION, EPC_ADAPTER, EPC_RLIMIT, EE_TBUFS2);

 	/* 	- set RTOP to RLIMIT to open all buffers */
	rtop_orig = EPC_GET(EPC_REGION, EPC_ADAPTER, EPC_RTOP);
	EPC_SET(EPC_REGION, EPC_ADAPTER, EPC_RTOP, 2);

	/*
	 * 	- tell SEEQ to rcv station or broadcast frames (normal) 
	 * 	and intr (seeq->epc) on any possible frame condition
	 *
	 *	RCV Command Reg Format: 0xbf = match1, all intrs on
	 *		bit 7:	match mode 1
	 *		bit 6:	match mode 0 
	 *		bit 5:	intr on good frame
	 *		bit 4:	intr on eof
	 *		bit 3:	intr on short frame
	 *		bit 2:	intr on dribble err
	 *		bit 1:	intr on crc err
	 *		bit 0:	intr on overflow
	 */
	rcmd_orig = EPC_GET(EPC_REGION, EPC_ADAPTER, EPC_RCMD); 
	EPC_SET(EPC_REGION, EPC_ADAPTER, EPC_RCMD, 0xbf); 
	/* match1 == 1, match0 == 0, all intrs on*/

	/* set up the TCMD register to interrupt on successful xmit */
	tcmd_orig = EPC_GET(EPC_REGION, EPC_ADAPTER, EPC_TCMD);
	EPC_SET(EPC_REGION, EPC_ADAPTER, EPC_TCMD, 0x8);

	/* test the 8 RXTXD bits on EPC->EDLC  with 
	 * 	1. walking 0
	 *	2. walking 1
	 * 	3. a5a's
	 * 	4. 0F0's
	 *	5. all 5's
	 *	6. numerically increasing data: 0, 1, 2 .. 2^8
	 *
	 */
	for (i = 0; i < NUMPATTERNS; i++) {
		setup_xmit(i, 0);
		fail += doxmit(slot);
		if (fail)
			goto TIMEDOUT;
		fail += verify_data(slot);
		packetnum++;
	}

	/* test 4 additional patterns */
	for (i = 0; i < 4; i++) {
		setup_xmit(INCRSE, i);
                fail += doxmit(slot);
		if (fail)
			goto TIMEDOUT;
                fail += verify_data(slot);
		packetnum++;
        }

TIMEDOUT:
	if (fail) {
		msg_printf(ERR, 
	    		"\nUnable to transmit and/or receive. ");
		msg_printf(ERR, "Reset the system and try again\n");
	}

	/* clean up and restore original enet state */
	

	for (i = 0; i < EADDR_SIZE; i++) {
	   EPC_SET(1, 1, EPC_EADDR_BASE + 8*i, eaddr_orig[i]);
	}
	for (i = 0; i < EADDR_SIZE; i++) {
	   EPC_SET(EPC_REGION,EPC_ADAPTER,EPC_EADDR_BASE+8*i,eaddr2_orig[i]);
	}
	EPC_SET(EPC_REGION, EPC_ADAPTER, EPC_TBASEHI, tbasehi_orig);
        EPC_SET(EPC_REGION, EPC_ADAPTER, EPC_TBASELO, tbaselo_orig);
	EPC_SET(EPC_REGION, EPC_ADAPTER, EPC_TLIMIT, tlimit_orig);
	EPC_SET(EPC_REGION, EPC_ADAPTER, EPC_RBASEHI, rbasehi_orig);
	EPC_SET(EPC_REGION, EPC_ADAPTER, EPC_RBASELO, rbaselo_orig);
	EPC_SET(EPC_REGION, EPC_ADAPTER, EPC_RLIMIT, rlimit_orig);
	EPC_SET(EPC_REGION, EPC_ADAPTER, EPC_RCMD, rcmd_orig); 
	EPC_SET(EPC_REGION, EPC_ADAPTER, EPC_TCMD, tcmd_orig);

	/*
	 * clear_hang code
	 */
	ee_clearhang();

	setup_err_intr(slot, adap);
	clear_err_intr(slot, adap);

	return (fail);

} /* xmit */

int setup_xmit(int testpattern, int pattern)
{
	int i;

	/*
	 * Set up the transmit buffer in tbuf 0
	 */
/*
	bp = (char *)K1_TO_K0(tbuf); 
	rbp = (char *)K1_TO_K0(rbuf); 
*/
	
	bp = tbuf;
	rbp = rbuf;

	msg_printf(DBG, "bp: 0x%x, rbp: 0x%x\n", bp, rbp);
	
	for (i = 0; i < 2* EE_BUFSIZE; i++) {
		rbp[i] = 0xaa;
		bp[i] = 0x55;
	}

	/* common stuff for all packets */
	/* - the actual packet starts at bp[EPC_ENET_DATA] with dest addr */
	for (i = 0; i < 6; i++)
		bp[EPC_ENET_DATA + i] = ee5[i];

	/* 	- next comes src addr */
	for (i = 0; i < 6; i++)
		bp[8 + i] = ee1[i];

	/*      - 2 bytes of type -- unused at this level */
        bp[14] = 0xaa;
        bp[15] = 0x55;
	
	/* this part takes care of the specific data and length info */
	switch (testpattern) {
		case WALK0:  setup_walk(WALK0); break;
		case WALK1:  setup_walk(WALK1); break;
		case AFIVEA: setup_afivea(0); break;
		case FZEROF: setup_fzerof(); break;
		case ALLFIVE: setup_afivea(1); break;
		case INCRSE: setup_incrse(pattern); break;
		default: msg_printf(ERR, "Unknown data pattern requested\n");
			 return 1;
	} /* switch */

	bp[0] = tlen;	/* LSB */
	bp[1] = 0;	/* MSB */

	if (DBG <= *Reportlevel) 
		for (i = 0; i < tlen; i++)
			msg_printf(DBG, "%d:%x, ", i, bp[i]);

} /* setup_xmit */

int setup_walk(int pattern)
{
	/* walk1 => bp[16]=1, bp[17]=2, bp[18]=4, bp[19]=8...
	 * walk0 => bp[16]=0xfe, bp[17]=0xfd, bp[18]=0xfb, bp[19]=0xf7...
	 */

	int i, j, walklength;

	j = 1;
	walklength = 50;

	tlen = 2 * 6 + 4 + walklength; /* 2 * 6 + 2+length_of_walk_patterns */
	
	for (i = 0; i < walklength; i++) 
		bp[16 + i] = (j << (i % 8));

	
	if (pattern == WALK0)
		for (i = 0; i < walklength; i++)
			bp[16 + i] = ~bp[16 + i];
	
} /* setup_walk */

int setup_afivea(int allfive)
{
	/* bp[16]=aa, bp[17]=55, bp[18]=aa... */

	int i, fivealength;
	
	fivealength = 50;
	
	tlen = 2 * 6 + 4 + fivealength; /* 2 * 6 + 2+fivealength */
	for (i = 0; i < fivealength; i++) {
		if ((i % 2) == 0)
			bp[16 + i] = 0x55;
		else
			bp[16 + i] = (allfive ? 0x55 : 0xaa);
	}

} /* setup_fiveafive */

int setup_fzerof()
{
	/* bp[16]=ff, bp[17]=0, bp[18]=ff... */

	int i, fzeroflength;

	fzeroflength = 60;

	tlen = 2 * 6 + 4 + fzeroflength;
	for (i = 0; i < fzeroflength; i++) {
                if ((i % 2) == 0)
                        bp[16 + i] = 0;
                else
                        bp[16 + i] = 0xff;
        }

} /* setup_fzerof */
		
int setup_incrse(int pattern)
{
	/* 2 small packets, 2 long packets */

	/* pattern == 0: bp[16]=0
 	 * pattern == 1: bp[16]=ff, bp[17]=fe, ... bp[LONGSIZE1-1] = n;
 	 * pattern == 2: bp[16]=0, bp[17]=1, bp[18]=2
	 * pattern == 3: bp[16]=cc, bp[17]=cd, bp[18]=ce, .. bp[1560]=n;
 	 */

	int i, j;

	if ((pattern == 0) | (pattern == 2)) {
		tlen = 2 * 6 + 4 + (pattern + 1);

		for (i = 0; i < pattern+1; i++)
			bp[16 + i] = i;
	}
	else if (pattern == 1) {
		j = 0xff;
		tlen = LONGSIZE1;
		for (i = 16; i < LONGSIZE1; i++) {
                        bp[i] = j;
			if (j == 0)
				j = 0xff;
			else
				j--;
		}
	}
	else if (pattern == 3) {
		j = 0xcc;
		tlen = LONGSIZE1;
			/* 1 more than the allowed normal size +16 */
                for (i = 16; i < LONGSIZE1; i++) {
                        bp[i] = j;
                        if (j == 0xff)
                                j = 0xcc;
			else
                        	j++;
		}
        }
}

int doxmit(uint slot)
{
	int count, i, j, k;
	long long tindex_big, rindxcnt, lxt_orig, edlc_orig, tstat_orig, stat;
	int fail = 0;

	/* flush the io cache */
	msg_printf(DBG, "flush at beginning of doxmit \n");
	flush_iocache(slot);	

	/* reset TTOP for next packet if TINDEX = TINDEX_TOOBIG */
        tindex_big = EPC_GET(EPC_REGION, EPC_ADAPTER, EPC_TINDEX);
        msg_printf(VRB,"TINDEX at start of doxmit is 0x%llx\n", tindex_big);
        if (tindex_big == TINDEX_TOOBIG) {
           msg_printf(VRB, "Decrementing TINDEX\n");
           EPC_SET(EPC_REGION,EPC_ADAPTER,EPC_TTOP,EPC_TBN_DECR(EPC_TINDEX));
        }


	/* set the LXT into loopback mode */
	/* EPC_SET(EPC_REGION, EPC_ADAPTER, EPC_LXT_LOOP, 1); */
	lxt_orig = EPC_GET(EPC_REGION, EPC_ADAPTER, EPC_LXT_LOOP);  
	EPC_SETB(EPC_LXT_LOOP, 1);  
	EPC_GET(EPC_REGION, EPC_ADAPTER, EPC_LXT_LOOP); 

	/* set the EDLC into self-receive mode */
	/* EPC_SET(EPC_REGION, EPC_ADAPTER, EPC_EDLC_SELF, 1); */
	edlc_orig = EPC_GET(EPC_REGION, EPC_ADAPTER, EPC_EDLC_SELF); 
	EPC_SETB(EPC_EDLC_SELF, 1); 
	EPC_GET(EPC_REGION, EPC_ADAPTER, EPC_EDLC_SELF);

	rindxcnt = EPC_GET(EPC_REGION, EPC_ADAPTER, EPC_RINDEX);
	msg_printf(DBG, "\nEPC_RINDEX is 0x%llx\n", rindxcnt);

	/*
	 * Transmit buffer 1
	 */

	/* 	- enable seeq->epc intr on good xmt */
	tstat_orig = EPC_GET(EPC_REGION, EPC_ADAPTER, EPC_TSTAT); 
	EPC_SET(EPC_REGION, EPC_ADAPTER, EPC_TSTAT, 0x8);

	/* running the following section causes xmits to hang */
	/* msg_printf(DBG, "\nEPC_TLIMIT is 0x%llx\n", 
		EPC_GET(EPC_REGION,EPC_ADAPTER, EPC_TLIMIT));
	msg_printf(DBG, "\nEPC_TTOP is 0x%llx\n", 
		EPC_GET(EPC_REGION,EPC_ADAPTER, EPC_TTOP));
	*/
	msg_printf(DBG, "EPC_TINDEX before starting is 0x%llx\n",
			EPC_GET(EPC_REGION, EPC_ADAPTER, EPC_TINDEX));

	/* 	 - bump TOP away from INDEX to kick off the dma engine */
	EPC_SET(EPC_REGION, EPC_ADAPTER, EPC_TTOP, EPC_TBN_INCR(EPC_TINDEX));
	/* msg_printf(DBG, "\nEPC_TTOP after incrementing is 0x%llx\n", 
		EPC_GET(EPC_REGION,EPC_ADAPTER, EPC_TTOP));
	*/

	/* 
	 * Watch ee1 transmit the packet -- done when TBPTR = tlen + 2
	 */
	msg_printf(SUM, "Sending packet %d...\n", packetnum); 

	i = 0; k = 0;
	while((count=EPC_GET(EPC_REGION,EPC_ADAPTER,EPC_TBPTR)) < tlen + 2) {
		i++;
		if (i == XMIT_TIMEOUT) {
			msg_printf(DBG, 
				"\nCompleted %d loops of waiting\n",i);
			err_msg(ENET_XMIT, &slot);
			stat = EPC_GET(EPC_REGION, EPC_ADAPTER, EPC_TSTAT);
			msg_printf(ERR, 
			   "Transmit packet count: 0x%x, ", count);
			msg_printf(ERR, "Transmit status: 0x%llx\n", stat);
			stat = (stat >> 16);
			if (stat & 0x8) {
				msg_printf(ERR, "Transmit status reports success, but not enough bytes were sent.\n");
				msg_printf(ERR, "Check EDLC CDST signals\n");
			}
			else 
				msg_printf(ERR, "Transmit unsuccessful\n");
			if (stat & 0x4)
				msg_printf(ERR, "16 transmission attempts\n");	
			if (stat & 0x2)
				msg_printf(ERR, "Transmit collision\n");	
			if (stat & 0x1)
				msg_printf(ERR, "Transmit underflow\n");	
			fail++;
			i = 0;
			k++;
			if (k == 1) {
				/* jump out of the code */
				return (fail);
			}
		}
	}
	msg_printf(DBG, "\nCompleted xmit packet count: 0x%x\n",count);
	msg_printf(DBG, "Completed TSTAT: 0x%llx\n",
                        EPC_GET(EPC_REGION, EPC_ADAPTER, EPC_TSTAT));
	msg_printf(DBG, "EPC_TINDEX is now 0x%llx\n",
			EPC_GET(EPC_REGION, EPC_ADAPTER, EPC_TINDEX));
	
	/*
 	 * Wait for ee5 to receive -- spin on EPC_RINDEX (should go 0 -> 1)
	 * XXX turn on EPC/SEEQ intrs and verify in R4000 C0_CAUSE
	 */
	i = 0; k= 0;
	while(EPC_GET(EPC_REGION, EPC_ADAPTER, EPC_RINDEX) == rindxcnt) {
		i++;
		if (i == RCV_TIMEOUT) {
			msg_printf(DBG, 
				"\nCompleted %d loops of waiting\n",i);
			err_msg(ENET_RCV, &slot);
			msg_printf(ERR, "RBPTR: 0x%llx, ", 
				EPC_GET(EPC_REGION, EPC_ADAPTER, EPC_RBPTR));
			stat = EPC_GET(EPC_REGION, EPC_ADAPTER, EPC_RSTAT);
			msg_printf(ERR, "RSTAT: 0x%llx\n", stat);
			stat = (stat >> 16);
			if ((stat & 0x10) || (stat & 0x20)) {
				if (stat & 0x20)
					msg_printf(ERR, "Received Good Frame");
				else	
				   msg_printf(ERR, "Received End of Frame");
				msg_printf(ERR, ", but did not receive enough bytes. Check EDLC CDST signals\n"); 
			}
			if ((stat & 0x10) == 0)
			   msg_printf(ERR, "Did not receive end of frame\n");
			if ((stat & 0x20) == 0)
			   msg_printf(ERR, "Did not receive good frame\n");
			if (stat & 0x8)
				msg_printf(ERR, "Received short frame\n");
			if (stat & 0x4)
			 msg_printf(ERR, "Received frame with dribble error\n");
			if (stat & 0x2)
			   msg_printf(ERR, "Received frame with CRC error\n");
			if (stat & 0x1)
				msg_printf(ERR,
				   "Received frame with overflow error\n");
			fail++;
                        i = 0;
			k++;
			if (k == 1) {
				/* jump out of the code */
				return (fail);	
			}
                }
	}

	/* okay! packet's in. print some status */
	msg_printf(DBG, "Completed RBPTR: 0x%llx\n", 
                        EPC_GET(EPC_REGION, EPC_ADAPTER, EPC_RBPTR));
	msg_printf(DBG, "Completed RSTAT: 0x%llx\n", 
                        EPC_GET(EPC_REGION, EPC_ADAPTER, EPC_RSTAT));
	/* end of frame */
	
	/* flush the io cache */
	msg_printf(DBG, "1st flush at end of doxmit\n");
	flush_iocache(slot);
	us_delay(2000); /* 2 ms */
	msg_printf(DBG, "2nd flush at end of doxmit after delay\n");
	flush_iocache(slot);

	/* reset orig values for lxt, edlc */
	
	EPC_SET(EPC_REGION, EPC_ADAPTER, EPC_TSTAT, tstat_orig); 
	EPC_SETB(EPC_LXT_LOOP, lxt_orig);  
	EPC_SETB(EPC_EDLC_SELF, edlc_orig);  

	return (fail);

} /* do xmit */

int verify_data(uint slot)
{
	int i, high_nibble;
	int fail = 0;
	char tmp;

	high_nibble = 0;

	/*
	 * Verify that the data arrived properly (again, no cache flushing)
	 */

	/* Dump out the rbuf */
	/* rbp = (char *)K1_TO_K0(&rbuf[0]);  */
	/* rbp = &rbuf[0]; */

	/* flush the io cache */
	msg_printf(DBG,"flush at begin of verify\n");
	flush_iocache(slot);	

	/* check the xmit status bytes */
/*	sbp = (char *)K1_TO_K0((&tbuf[0]) + EPC_ENET_STAT);
*/
	sbp = &tbuf[0] + EPC_ENET_STAT;
	for (i = 0; i < 4; i++) {
		msg_printf(DBG, "xmit status byte %d: 0x%x\n", i, sbp[i]);
		switch (i) {
			case 0: if (sbp[i]) {
				   err_msg(ENET_TSTAT,&slot,31,24,0,sbp[i]); 
				   fail++;
				}
				break;
			case 1: if ((sbp[i] & 0xf) != 0x8) {
				   err_msg(ENET_TSTAT, &slot,23,16,0x8, sbp[i]); 
				   fail++;
				} 
				break;
			case 2: if ((sbp[i] & 0xf8) != 0x10) {
				   err_msg(ENET_TSTAT,&slot,15,11,0x10, sbp[i]); 
				   fail++;
				} 
				high_nibble = sbp[i] & 0x7;
				break;
			case 3: if ((sbp[i] + (high_nibble << 8)) != tlen) {
				   err_msg(ENET_TSTAT,&slot,10,0,tlen, sbp[i] + (high_nibble << 8)); 
				   fail++;
				}
				break;
		} 
	}

	if (fail) {
		msg_printf(ERR, "Transmit error. Check EPC control signals\n");
	}

	high_nibble = 0;
	/* Check receive status placed in rbuf 0 after rmit */
	/* sbp = (char *)K1_TO_K0(&rbuf[0]) + EPC_ENET_STAT; */
	sbp = (&rbuf[0]) + EPC_ENET_STAT;
	for (i = 0; i < 4; i++) {
	   	msg_printf(DBG, "rcv status byte %d: 0x%x\n", i, sbp[i]);
		switch (i) {
                        case 0: 
				if (sbp[i]) {
                                     err_msg(ENET_RSTAT,&slot,31,24,0,sbp[i]); 
				     fail++;
                                }
                                break;
                        case 1: 
				if (packetnum == 5 || packetnum == 7) {
			    	     if ((sbp[i]) != 0x18) {
                                        err_msg(ENET_RSTAT,&slot,23,16,0x18,sbp[i]); 
				        fail++;
				     }
                                }
				else {
				     if ((sbp[i]) != 0x30) {
				        err_msg(ENET_RSTAT,&slot,23,16,0x30,sbp[i]); 
                                     	fail++;	
				     }
				}
                                break;
                        case 2: 
				if ((sbp[i] & 0xf8) != 0) {
                                     err_msg(ENET_RSTAT, &slot,15, 11, 0,sbp[i]); 
				     fail++;
                                }
                                high_nibble = sbp[i] & 0x7;
                                break;
                        case 3: 
				tmp = sbp[i] + (high_nibble << 8);
				if (tmp != tlen) {
				/* retry since we seem to get bogus data */
				   flush_iocache(slot);
				   us_delay(1000);
				   flush_iocache(slot);
				   us_delay(6000);
				   tmp = sbp[i];
			           tmp += (high_nibble << 8);
				   msg_printf(VRB, "RSTAT is 0x%x\n", tmp);
			/* only works for packet sizes < 0xff */
				   if ((tmp != tlen) && 
				     ((sbp[i]+(high_nibble << 8)) != tlen) &&
				     (sbp[i] != tlen))
				   {
                                     err_msg(ENET_RSTAT,&slot,10,0,tlen,tmp);
				     fail++;
				   } 
                                }
                                break;
                }
	} 

	if (fail) {
		msg_printf(ERR, 
		   "Check SEEQ_INTR and other SEEQ/EPC control signals\n");
	}

	/* check the data in the buffers */	
	for (i = 2; i < tlen; i++) { /* data starts at byte 2 */
		tmp = rbuf[i];
		if (tmp != tbuf[i]) {
		/* retry since we get bogus data occasionally */
			flush_iocache(slot);
			us_delay(1000);
			flush_iocache(slot);
			us_delay(4000);
			tmp = rbuf[i];
			msg_printf(VRB, "Data retry is 0x%x\n", tmp);
			if ((tmp != tbuf[i]) && (rbuf[i] != tbuf[i])) {
			   err_msg(ENET_DATA, &slot,i,tlen,tbuf[i], rbuf[i]);
			   msg_printf(ERR, 
			     "Check the RXTXD lines of the EPC and SEEQ\n");
			   fail++;
			}
		}
	}

	/* Dump out tmit status placed in tbuf 0 after tmit */
        if ((fail != 0) || (DBG <= *Reportlevel)) {
                sbp = (&tbuf[0]) + EPC_ENET_STAT;
                for (i = 0; i < 4; i++)
                   msg_printf(ERR, 
			"Transmit status byte %d: 0x%x\n", i, sbp[i]);

                /* Dump out rmit status placed in rbuf 0 after rmit */
                sbp = (&rbuf[0]) + EPC_ENET_STAT;
                for (i = 0; i < 4; i++)
                   msg_printf(ERR, 
			"Receive status byte %d: 0x%x\n", i, sbp[i]);
		msg_printf(ERR, "\n");

		/* print out the receive buffer for debugging */
		if (DBG <= *Reportlevel) {
		   	msg_printf(DBG, "Receive buffer: \n");
		   	for (i = 0; i < tlen; i++)
				msg_printf(DBG, "%d:%x, ", i, rbuf[i]);
		   	msg_printf(DBG, "\n");
		}
         }

	/* reset TTOP for next packet */
	EPC_SET(EPC_REGION, EPC_ADAPTER, EPC_TTOP, EPC_TBN_DECR(EPC_TINDEX));

	return(fail);

} /* verify_data */

int enet_xmit(int argc, char **argv)
{
	int fail = 0;
	char answ[4];

	msg_printf(INFO, "enet_xmit - Ethernet transmit packet test, internal loopback on\n\n");
	/* msg_printf(SUM, "Unplug the ethernet cable for this test, ");
	msg_printf(SUM, "otherwise the results may be wrong.\n\n");
questloop:
	msg_printf(SUM, 
	  "If you wish to continue, unplug your ethernet cable and type 1.\n");
	msg_printf(SUM, "Or type 0 to exit from this test now: ");
	gets(answ);
        if (!strcmp(answ, "1") || !strcmp(answ, "0")) {
        	if (!strcmp(answ, "0")) {
                	msg_printf(SUM,"Exiting now...\n");
                        return (0);
                }
        }
        else {
        	msg_printf(SUM,"You did not enter a 1 or a 0\n\n");
                goto questloop;
        }
*/
	fail = test_enet(argc, argv, xmit);
	return(fail);
}

/*
 * this routine stolen from the kernel - for the original version with all
 * the kernel hooks, see comments at head of file
 */
int
ee_clearhang(void)
{
        int rtop, rindex, i, match;

        /*
         * Prevent receiver overflow hang.  If RINDEX catches RTOP the EPC
         * will hang if we then bump RTOP while the EDLC is asserting
         * any recv interrupt to the EPC.  Before bumping RTOP make sure
         * RINDEX hasn't caught up (or won't before we bump RTOP).
         */
        rindex = EPC_GETW(EPC_RINDEX);
        rtop = EPC_GETW(EPC_RTOP);

#define LEADPKTS 64 /* if RINDEX far enough away we're safe in bumping RTOP */
        if (rtop < rindex)
                rtop += ei->ei_rbufs;   /* keep subtraction positive */
        if (rtop - rindex > LEADPKTS) {
                EPC_SETW(EPC_RTOP, 0);
                return;
        }
        /* Okay, we're behind: initiate hang avoidance sequence captain! */

        /*
         * Step 1       EDLC must be isolated from the wire when setting
         *              its registers: set loopback mode on the LXT901.
         */
        ee_set_loopback(ei);

        /*
         * Step 2       Shut off transmitter by turning off the EPC
         *              transmit DMA engine.  Yes, this tosses packets
         *              already queued for output... we're here to fix
         *              the rcv side. XXX  (We _could_ spin waiting for
         *              TINDEX to reach TTOP if we properly bullet-proof
         *              such a loop).
         */
        EPC_SETW(EPC_TTOP, EPC_GETW(EPC_TINDEX));       /* XXX racy ? */

        /*
         * Step 3       Hardware reset of EDLC chip.
         */
        ee_reset();


        /*
         * Step 4       Turn on transmit side of the world.   Turns on the
         *              transmit interrupts (EDLC->EPC).
         */
        EPC_SETW(EPC_TCMD, SEQ_XC_INTGOOD | SEQ_XC_INT16TRY | SEQ_XC_INTCOLL |
                SEQ_XC_INTUFLOW);

        /*
         * Step 5       Program and turn on receive side of the world.
         *              Reprogram station address. Note that we're still
         *              in LXT901 loopback.. i.e., we're disconnected
         *              from the wire and inactive.
         */
#define EV_PAD  8
#define EV_PAD  8
        for (i = 0; i < 6; i++)
                EPC_SETW(EPC_EADDR_BASE + EV_PAD * i, ei->ei_addr[i]);

	match = SEQ_RC_RSB;
        EPC_SETW(EPC_RCMD, EE_RCMD | match);

        ei->ei_orindex = EPC_GETW(EPC_RINDEX);
        EPC_SETW(EPC_RTOP, EPC_RBN_DECR(ei->ei_orindex));
        EPC_GETW(EPC_RTOP);     /* sync up the write to h/w */

        /*
         * Step 6       Disable loopback.  (Patch us back out to the wire).
         */
        ee_clear_loopback(ei);

}

ee_set_loopback(struct ee_info_ide *ei)
{
        EPC_SETB(EPC_LXT_LOOP, 1); /* this pbus "dev" must be byte-accessed */
        EPC_GETW(EPC_LXT_LOOP); /* sync to h/w */
}

ee_clear_loopback(struct ee_info_ide *ei)
{
        EPC_SETB(EPC_LXT_LOOP, 0);
}
