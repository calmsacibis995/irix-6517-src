/*
 * readcollide.c - Read collision counters. Send packet to specified host.
 * 	Verifies whether the collision counters are working.
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

extern int *Reportlevel;
extern struct ee_info_ide *ei;
extern char *atob();

#define XMIT_TIMEOUT    0x8ffff
#define RCV_TIMEOUT     0x8ffff

#define MAX_TLIMIT      255

#define EE_BUFSIZE      2048
#define EE_TBUFS2       0
#define EADDR_SIZE      6

/* type of patterns to test */
#define WALK0           0
#define WALK1           1
#define AFIVEA          2
#define FZEROF          3
#define ALLFIVE         4
#define NUMPATTERNS     5

#define INCRSE          5
#define RNDOM           6
#define NUMRANDOM       25 /* number of random patterns to test */
#define MAXNORMSIZE     1500 /* max normal size data bytes in packet */
#define LONGSIZE1       130 /* a packet size */

#define TINDEX_TOOBIG   0x69

static char sender[6] = { 0xff, 0xfe, 0xfd, 0xfb, 0xf7, 0xff };
static char receiver[6] = { 0x0, 0x1, 0x2, 0x3, 0x4, 0x5 };

char bufs_ctr[8*EE_BUFSIZE]; /* enough for 2 4K aligned pairs of bufs */

char *bp_ctr, *rbp_ctr, *sbp_ctr, *rbuf_ctr, *tbuf_ctr;
int tlen_ctr;
int packetnum_ctr = 0;

int read_counter(int slot, int adap)
{
	int i;
	int fail = 0;
	long long imset, rcmd_orig, tcmd_orig, tbasehi_orig, tbaselo_orig;
	long long tlimit_orig, rbasehi_orig, rbaselo_orig, rlimit_orig;
	long long rtop_orig;
	long long eaddr_orig[EADDR_SIZE], eaddr2_orig[EADDR_SIZE];
	char answ[4];
	uint value;

	msg_printf(SUM, "\n\n\n");
	msg_printf(SUM,"WARNING: Running this test for a long time may eventually wedge your EPC chip\n");
	msg_printf(SUM,"and you will have to reset your system.\n\n\n");
	msg_printf(SUM, 
	   "In order to run this test, you need to do the following: \n");
	msg_printf(SUM, 
	   "\n1. Find 2 machines on this network which have /usr/etc/ttcp\n");
	msg_printf(SUM, 
	   "2. On the first machine type 'netstat -ia' and write down the 6\n");
	msg_printf(SUM,
	   "numbers starting with 08 under the Address column. \n");
	msg_printf(SUM,"This is the ethernet address of machine #1.\n");
	msg_printf(SUM,
	   "(e.g. The set of 6 numbers will look something like 08:00:69:02:00:44)\n");
QUESTLOOP:
	msg_printf(SUM,
	   "\nEnter the first number from step #2 preceded by 0x (e.g. 0x08): ");
	gets(answ);
	atob(answ, &value);
	receiver[0] = value;
	for (i = 2; i <= 6; i++) {
	  	msg_printf(SUM,"Now enter number #%d preceded by 0x: ", i);
		gets(answ);
		atob(answ, &value);
		receiver[i-1] = value;
	}

	msg_printf(SUM,"\nHere's what I have for the ethernet address: ");
	for (i=0; i< 5; i++)
		msg_printf(SUM, "%x:", receiver[i]);
	msg_printf(SUM, "%x\n", receiver[5]);

	msg_printf(SUM, "Does this look right? (Yes = 1 or No = 0): ");
	gets(answ);
	if (!strcmp(answ, "1") || !strcmp(answ, "0")) {
                if (!strcmp(answ, "0")) 
			goto QUESTLOOP;
        }
        else {
                msg_printf(SUM,"You did not enter a 1 or a 0\n\n");
                goto QUESTLOOP;
        }
	
	msg_printf(SUM,
	   "3. Still on the first machine, type '/usr/etc/ttcp -r -s' \n");
	msg_printf(SUM,
	   "4. On the second machine, type '/usr/etc/ttcp -t -s <name of first machine>' \n\n\n");
	msg_printf(SUM, "Hit return when you're ready to continue: ");
	gets(answ);

	/* 2 bufs requires 4k alignment */
	rbuf_ctr = (char*)((((paddr_t)bufs_ctr + 2 * EE_BUFSIZE) >> 12 ) << 12);
	tbuf_ctr = (char*)((paddr_t)rbuf_ctr + 2 * EE_BUFSIZE); 

	msg_printf(DBG, "rbuf_ctr: 0x%x, tbuf_ctr: 0x%x\n", rbuf_ctr, tbuf_ctr);

	for (i = 0; i < 2* EE_BUFSIZE; i++) {
		rbuf_ctr[i] = 0x67;
		tbuf_ctr[i] = 0x98;
	}

	ee_reset();
	ee_init();

	/* make sure IMSET[6] is not set -- for EPC rcv interrupt bug */
	imset = EPC_GET(EPC_REGION, EPC_ADAPTER, EPC_IMSET);
	if (imset & EPC_INTR_UNUSED) {
		msg_printf(DBG,"imset bit 6 is set, imset is 0x%llx\n",imset);
		EPC_SET(EPC_REGION, EPC_ADAPTER, EPC_IMSET, imset & ~EPC_INTR_UNUSED);
		EPC_GET(EPC_REGION, EPC_ADAPTER, EPC_IMSET);
	}

	packetnum_ctr = 0; /* reset each time xmit is run */

	/*
	 * Configure sender as for transmitting (only?)
	 */

	/* get enet address of machine running this test */
	cpu_get_eaddr(sender);

	/* set station address */
	for (i = 0; i < EADDR_SIZE; i++) {
		eaddr_orig[i] = EPC_GET(1, 1, EPC_EADDR_BASE + 8*i);
		EPC_SET(1, 1, EPC_EADDR_BASE + 8*i, sender[i]);
	}

	/* set address of transmit buffers */
	/* we just know this is so */
	tbasehi_orig = EPC_GET(EPC_REGION, EPC_ADAPTER, EPC_TBASEHI); 
	tbaselo_orig = EPC_GET(EPC_REGION, EPC_ADAPTER, EPC_TBASELO);
#ifndef TFP
	EPC_SET(EPC_REGION, EPC_ADAPTER, EPC_TBASEHI, 0x0); 
	EPC_SET(EPC_REGION, EPC_ADAPTER, EPC_TBASELO, 
			K1_TO_PHYS(tbuf_ctr) & 0xffffffff);
#else
	EPC_SET(EPC_REGION, EPC_ADAPTER, EPC_TBASEHI,
			K1_TO_PHYS(tbuf_ctr) >> 32);
	EPC_SET(EPC_REGION, EPC_ADAPTER, EPC_TBASELO, 
			K1_TO_PHYS(tbuf_ctr) & 0xffffffff);
#endif

	/* set number of transmit buffers */
	/* tlimit_orig = EPC_GET(EPC_REGION, EPC_ADAPTER, EPC_TLIMIT); */
	EPC_SET(EPC_REGION, EPC_ADAPTER, EPC_TLIMIT, EE_TBUFS2);

	/*
	 * Configure receiver for receiving (only?)
	 */

	/* 	- set station address */
	for (i = 0; i < EADDR_SIZE; i++) {
	   eaddr2_orig[i] =EPC_GET(EPC_REGION,EPC_ADAPTER,EPC_EADDR_BASE+8*i);
	   EPC_SET(EPC_REGION, EPC_ADAPTER, EPC_EADDR_BASE + 8*i, receiver[i]);
	}

	/* 	- set address of receive buffers */
	rbasehi_orig = EPC_GET(EPC_REGION, EPC_ADAPTER, EPC_RBASEHI); 
	rbaselo_orig = EPC_GET(EPC_REGION, EPC_ADAPTER, EPC_RBASELO);
#ifndef TFP
	EPC_SET(EPC_REGION, EPC_ADAPTER, EPC_RBASEHI, 0x0); 
	EPC_SET(EPC_REGION, EPC_ADAPTER, EPC_RBASELO, 
			K1_TO_PHYS(rbuf_ctr) & 0xffffffff);
#else
	EPC_SET(EPC_REGION, EPC_ADAPTER, EPC_RBASEHI,
			K1_TO_PHYS(rbuf_ctr) >> 32);
	EPC_SET(EPC_REGION, EPC_ADAPTER, EPC_RBASELO, 
			K1_TO_PHYS(rbuf_ctr) & 0xffffffff);
#endif

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
REPEATLOOP:
	for (i = 0; i < NUMPATTERNS; i++) {
		setup_xmit_ctr(i, 0);
		fail += doxmit_cntr(slot);
		if (fail)
			goto TIMEDOUT;
		output_counters();
		packetnum_ctr++;
	}

	/* test 4 additional patterns */
	for (i = 0; i < 4; i++) {
		setup_xmit_ctr(INCRSE, i);
                fail += doxmit_cntr(slot);
		if (fail)
			goto TIMEDOUT;
		output_counters();
		packetnum_ctr++;
        }

TIMEDOUT:
	if (fail) {
		msg_printf(ERR, 
	    		"Unable to transmit or receive. ");
		msg_printf(ERR, "Reset the system and try again\n");
	}
questloop2:	
	msg_printf(SUM, 
		"\nIf the collision counts are not changing, check the \n");
	msg_printf(SUM,"collision PALs and counters\n\n");
	msg_printf(SUM, "Should I keep sending packets? (Yes = 1, no = 0): ");
	gets(answ);
	if (!strcmp(answ, "1") || !strcmp(answ, "0")) {
                if (!strcmp(answ, "1")) 
			goto REPEATLOOP;
        }
        else {
                msg_printf(SUM,"You did not enter a 1 or a 0\n\n");
                goto questloop2;
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
	/* EPC_SET(EPC_REGION, EPC_ADAPTER, EPC_TLIMIT, tlimit_orig); */
	EPC_SET(EPC_REGION, EPC_ADAPTER, EPC_RBASEHI, rbasehi_orig);
	EPC_SET(EPC_REGION, EPC_ADAPTER, EPC_RBASELO, rbaselo_orig);
	EPC_SET(EPC_REGION, EPC_ADAPTER, EPC_RLIMIT, rlimit_orig);
	EPC_SET(EPC_REGION, EPC_ADAPTER, EPC_RTOP, rtop_orig);

	EPC_SET(EPC_REGION, EPC_ADAPTER, EPC_RCMD, rcmd_orig); 
	EPC_SET(EPC_REGION, EPC_ADAPTER, EPC_TCMD, tcmd_orig);

	ee_reset();
	setup_err_intr(slot, adap);
	clear_err_intr(slot, adap);

	msg_printf("Exiting read_counter() . . .\n");

	return (fail);

} /* read_counter */

int setup_xmit_ctr(int testpattern, int pattern)
{
        int i;

        /*
         * Set up the transmit buffer in tbuf 0
         */
/*
        bp = (char *)K1_TO_K0(tbuf);
        rbp = (char *)K1_TO_K0(rbuf);
*/

        bp_ctr = tbuf_ctr;
        rbp_ctr = rbuf_ctr;

        msg_printf(DBG, "bp_ctr: 0x%x, rbp_ctr: 0x%x\n", bp_ctr, rbp_ctr);

        for (i = 0; i < 2* EE_BUFSIZE; i++) {
                rbp_ctr[i] = 0xaa;
                bp_ctr[i] = 0x55;
        }

        /* common stuff for all packets */
        /* - the actual packet starts at bp[EPC_ENET_DATA] with dest addr */
        for (i = 0; i < 6; i++)
                bp_ctr[EPC_ENET_DATA + i] = receiver[i];

        /*      - next comes src addr */
        for (i = 0; i < 6; i++)
                bp_ctr[8 + i] = sender[i];

        /*      - 2 bytes of type -- unused at this level */
        bp_ctr[14] = 0xaa;
        bp_ctr[15] = 0x55;

        /* this part takes care of the specific data and length info */
        switch (testpattern) {
                case WALK0:  setup_walk_ctr(WALK0); break;
                case WALK1:  setup_walk_ctr(WALK1); break;

                case AFIVEA: setup_afivea_ctr(0); break;
                case FZEROF: setup_fzerof_ctr(); break;
                case ALLFIVE: setup_afivea_ctr(1); break;
                case INCRSE: setup_incrse_ctr(pattern); break;
                default: msg_printf(ERR, "Unknown data pattern requested\n");
                         return 1;
        } /* switch */

        bp_ctr[0] = tlen_ctr;   /* LSB */
        bp_ctr[1] = 0;      /* MSB */

        if (DBG <= *Reportlevel)
                for (i = 0; i < tlen_ctr; i++)
                        msg_printf(DBG, "%d:%x, ", i, bp_ctr[i]);

} /* setup_xmit_ctr */

int setup_walk_ctr(int pattern)
{
        /* walk1 => bp[16]=1, bp[17]=2, bp[18]=4, bp[19]=8...
         * walk0 => bp[16]=0xfe, bp[17]=0xfd, bp[18]=0xfb, bp[19]=0xf7...
         */

        int i, j, walklength;

        j = 1;
        walklength = 50;

        tlen_ctr = 2 * 6 + 4 + walklength; /* 2 * 6 + 2+length_of_walk_patterns */

        for (i = 0; i < walklength; i++)
                bp_ctr[16 + i] = (j << (i % 8));


        if (pattern == WALK0)
                for (i = 0; i < walklength; i++)
                        bp_ctr[16 + i] = ~bp_ctr[16 + i];

} /* setup_walk_ctr */

int setup_afivea_ctr(int allfive)
{
        /* bp[16]=aa, bp[17]=55, bp[18]=aa... */

        int i, fivealength;

        fivealength = 50;

        tlen_ctr = 2 * 6 + 4 + fivealength; /* 2 * 6 + 2+fivealength */
        for (i = 0; i < fivealength; i++) {
                if ((i % 2) == 0)
                        bp_ctr[16 + i] = 0x55;
                else
                        bp_ctr[16 + i] = (allfive ? 0x55 : 0xaa);
        }

} /* setup_fiveafive */

int setup_fzerof_ctr()
{
        /* bp[16]=ff, bp[17]=0, bp[18]=ff... */

        int i, fzeroflength;

        fzeroflength = 60;

        tlen_ctr = 2 * 6 + 4 + fzeroflength;
        for (i = 0; i < fzeroflength; i++) {
                if ((i % 2) == 0)
                        bp_ctr[16 + i] = 0;
                else
                        bp_ctr[16 + i] = 0xff;
        }


} /* setup_fzerof_ctr */

int setup_incrse_ctr(int pattern)
{
        /* 2 small packets, 2 long packets */

        /* pattern == 0: bp[16]=0
         * pattern == 1: bp[16]=ff, bp[17]=fe, ... bp[LONGSIZE1-1] = n;
         * pattern == 2: bp[16]=0, bp[17]=1, bp[18]=2
         * pattern == 3: bp[16]=cc, bp[17]=cd, bp[18]=ce, .. bp[1560]=n;
         */

        int i, j;

        if ((pattern == 0) | (pattern == 2)) {
                tlen_ctr = 2 * 6 + 4 + (pattern + 1);

                for (i = 0; i < pattern+1; i++)
                        bp_ctr[16 + i] = i;
        }
        else if (pattern == 1) {
                j = 0xff;
                tlen_ctr = LONGSIZE1;
                for (i = 16; i < LONGSIZE1; i++) {
                        bp_ctr[i] = j;
                        if (j == 0)
                                j = 0xff;
                        else
                                j--;
                }
        }
        else if (pattern == 3) {
                j = 0xcc;
                tlen_ctr = LONGSIZE1;
                        /* 1 more than the allowed normal size +16 */
                for (i = 16; i < LONGSIZE1; i++) {
                        bp_ctr[i] = j;

                        if (j == 0xff)
                                j = 0xcc;
                        else
                                j++;
                }
        }
}

int output_counters()
{
	unsigned int late, early, tot;


        late  = EPC_GETW(EPC_LATE_COL) & EPC_LATE_COL_MASK;
        early = EPC_GETW(EPC_EARLY_COL) & EPC_EARLY_COL_MASK;
        tot   = EPC_GETW(EPC_TOT_COL) & EPC_TOT_COL_MASK;

        msg_printf(SUM, "\t\tLate  collisions: %d\n", late);
        msg_printf(SUM, "\t\tEarly collisions: %d\n", early);
        msg_printf(SUM, "\t\tTotal collisions: %d\n", tot);
}


int doxmit_cntr(uint slot)
{
	int count, i, j, k;
	long long tindex_big, rindxcnt, lxt_orig, edlc_orig, tstat_orig;
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


	/* set the LXT into normal mode */
	/* EPC_SET(EPC_REGION, EPC_ADAPTER, EPC_LXT_LOOP, 1); */
	lxt_orig = EPC_GET(EPC_REGION, EPC_ADAPTER, EPC_LXT_LOOP);  
	EPC_SETW(EPC_LXT_LOOP, 0);  
	EPC_GET(EPC_REGION, EPC_ADAPTER, EPC_LXT_LOOP); 

	/* set the EDLC into normal mode */
	/* EPC_SET(EPC_REGION, EPC_ADAPTER, EPC_EDLC_SELF, 1); */
	edlc_orig = EPC_GET(EPC_REGION, EPC_ADAPTER, EPC_EDLC_SELF); 
	EPC_SETW(EPC_EDLC_SELF, 0); 
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
	 * Watch sender transmit the packet -- done when TBPTR = tlen + 2
	 */
	msg_printf(SUM, "Sending packet %d...\n", packetnum_ctr); 

	i = 0; k = 0;
	while((count=EPC_GET(EPC_REGION,EPC_ADAPTER,EPC_TBPTR)) < tlen_ctr + 2) {
		i++;
		if (i == XMIT_TIMEOUT) {
			msg_printf(DBG, 
				"\nCompleted %d loops of waiting\n",i);
			err_msg(ENET_XMIT, &slot);
			msg_printf(ERR, 
				"Transmit packet count: 0x%x, ", count);
			msg_printf(ERR, "TSTAT: 0x%llx\n",
				EPC_GET(EPC_REGION, EPC_ADAPTER, EPC_TSTAT));
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

} /* doxmit_cntr */

int enet_colctr(int argc, char **argv)
{
	int fail = 0;

	fail = test_enet(argc, argv, read_counter);
	return(fail);
}
