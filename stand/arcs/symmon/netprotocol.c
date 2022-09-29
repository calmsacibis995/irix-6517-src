#ifdef NETDBX

/*
 * protocol.c -- udp protocol
 */


#ident "$Revision: 1.1 $"

#include <arcs/types.h>
#include "protocol.h"
#include "protoio.h"
#include "dbgmon.h"		/* for prototypes */
#include <setjmp.h>
#include <saioctl.h>
#include <libsc.h>
#include <libsk.h>

#include <arcs/types.h>
#include <arcs/signal.h>
#include <errno.h>

#ifndef NULL
#define	NULL	0
#endif

#define	CONTROL(x)	(x&0x1f)

char *str_buf(char *, int);

int protocol_debug = 0;
int bad_msg_debug = 1;

#define bprintf(x)  if (bad_msg_debug) printf("R");

#define DEBUG
#ifdef DEBUG
#define dprintf(x)  if (protocol_debug) printf x
#else
#define dprintf(x)
#endif

/*
 * UDP protocol description:
 *	This protocol is a ack/nack, byte-stuffing, half-duplex protocol.
 *	The protocol may be used for bidirectional communication, but the
 *	hand-shaking necessary to turn the line around is the responsibility
 *	of the user of the protocol.
 *
 *	SYN characters are used purely to synchonize the protocol and
 *	always resync the protocol when received.
 *
 * serial line protocol format:
 * <SYN><len><seq><data0>...<data_len>
 * 
 * <SYN>       ::=	Ascii sync character
 *
 * <type_len0> ::=	Packet type and high order bits of data length
 *			Length is data length.
 *			Bit 6 is set to 1 to avoid inadvertent SYN character
 *			Bit 5 == 1 => Data packet (DATA_PKTTYPE)
 *			Bit 5 == 0 => Acknowledgment packet (ACK_PKTTYPE)
 *			Bits 4 -- 0 => Bits 10 -- 6 of data length
 *
 * <len1>      ::=	Low order data length bits.
 *			Bit 6 is set to 1 to avoid inadvertent SYN character
 *			Bits 5 -- 0 => Bits 5 -- 0 of data length
 *
 * <seq>       ::=	6 bit sequence number, encoded as
 *			low 6 bits of 1 character.
 *			Character is OR'ed with 0x40 to
 *			avoid conflict with SYN character.
 *			ACK_PKTTYPE packets carry sequence number
 *			of NEXT expected DATA_PKTTYPE packet.
 *
 * <dataN>     ::=	Body of message
 *
 *
 */

static unsigned next_getseq;		/* next expected receive seq number */
static unsigned current_putseq;		/* seq number for current xmit packet */
static unsigned acked_putseq;		/* last acknowledged xmit packet seq */
static int n_acked;			/* number of ACK pkts received */
static int n_xmited;			/* number of transmitted pkts */

#ifdef DEBUG
char *
str_buf(char *buf, int len)
{
	static char tmpbuf[64];
	int i;

	if (len == 0)
		strcpy(tmpbuf, "\"\"");
	else {
		tmpbuf[0] = '"';
		for (i = 0; i < len && i < 20; i++)
			tmpbuf[i+1] = buf[i];
		tmpbuf[++i] = '"';
		tmpbuf[++i] = 0;
	}
	return(tmpbuf);
}
#endif

/*
 * netinit_proto -- reset protocol state info
 */
/* ARGSUSED */
void
netinit_proto(ULONG fd)
{
	PINIT(fd);
	next_getseq = current_putseq = 0;
	acked_putseq = (unsigned int) -1;
	n_acked = n_xmited = 0;
}


static jmp_buf resync_buf;

static char		putbuftmp[MAXPACKET];
static char		getbuftmp[MAXPACKET];

/*
 * netgetpkt -- unwrap incoming packet, check sequence number and checksum
 * and send appropriate acknowledgment.
 * Returns data length.
 *
 * fd		- device to receive from
 * buf		- buf for received packet
 * cnt		- max data len
 * pkt_typep	- in: pointer to desired packet type
 *		  out: pointer to received packet type
 */
int
netgetpkt(ULONG fd, char *buf, ULONG cnt, int *pkt_typep)
{
	register char *cp;
	register int i;
	unsigned seq;
	int pkt_type;
	int new_packet, type_len, len;
 	unsigned nseq;
	extern int Debug;
	char 	*ip;
	char 	*ibuf = &getbuftmp[0];

	if (*pkt_typep != DATA_PKTTYPE && *pkt_typep != ACK_PKTTYPE &&
	    *pkt_typep != ANY_PKTTYPE) {
		printf("*** netgetpkt: BAD PKTTYPE\n");
		return(0);
	}
	new_packet = 0;
	while (!new_packet)
	{
		
		dprintf(("netgetpkt: waiting on read\n"));
		Read(fd, ibuf, MAXPACKET, &cnt);
		dprintf(("netgetpkt: read %d bytes\n", cnt));

		if (ibuf[0] != SYN) {
			printf("netgetpkt: Warning! no SYN at start of msg. (0x%x), discard %d bytes\n", ibuf[0], cnt);
			continue;
		}
		if (cnt < 4) {
			printf("netgetpkt: Warning! Expected 4, got %d bytes; discarding\n", cnt);
			continue;
		}

		setjmp(resync_buf);		/* longjmp here on SYN */

		type_len = ibuf[1];

		pkt_type = type_len & MASK_PKTTYPE;
		if (pkt_type != *pkt_typep && *pkt_typep != ANY_PKTTYPE) {
			bprintf(("bad type, got %s, seq: got %d, expect = %d\n",
			    (long)(pkt_type == DATA_PKTTYPE ? "data" : "ack"),
			(ibuf[3] & 0x3f), (pkt_type == DATA_PKTTYPE) ?
				next_getseq : (current_putseq + 1) & 0x3f));
			goto sendack;
		}

		len = (type_len & 0x1f) << 6;
		len |= ibuf[2] & 0x3f;
		if (len > cnt) {	/* don't accept long packets */
			printf("bad len\n",0,0);
			goto sendack;
		}

		seq = (pkt_type == DATA_PKTTYPE)
		    ? next_getseq		    /* next expected data seq */
		    : (current_putseq + 1) & 0x3f;  /* next expected ack seq */
		if ((nseq = (ibuf[3] & 0x3f)) != seq) {
			bprintf(("bad seq, got 0x%x wanted 0x%x type = %s\n",
				nseq, seq,
			    (long)(pkt_type == DATA_PKTTYPE ? "data" : "ack")));
			goto sendack;
		}

		cp = buf;
		i = len;
		ip = &ibuf[4];

		if (len != (cnt - 4)) {
			printf("netgetpkt: Warning! Expected %d, got %d bytes; discarding\n", len, cnt);
			continue;
		}

			
		while (i-- > 0)
			*cp++ = *ip++;

		new_packet = 1;		/* got a good packet */
		if (pkt_type == DATA_PKTTYPE)
			next_getseq = (next_getseq + 1) & 0x3f;
		else
			acked_putseq = current_putseq;

sendack:
		dprintf(("netgetpkt len=%d seq=0x%x type %s\r\n",
		    len, nseq, pkt_type == ACK_PKTTYPE ? "ACK" : "DATA"));

		dprintf(("netgetpkt, buf = %s\n", str_buf(buf, len)));

		/*
		 * Don't send ACKs to ACKs
		 */
		if (pkt_type != ACK_PKTTYPE)
			netputpkt(fd, NULL, 0, ACK_PKTTYPE);
	}

	*pkt_typep = pkt_type;
	return(len);
}


jmp_buf rexmit_buf;

static void
rexmit_handler(void)
{
	longjmp (rexmit_buf, 1);
}


/*
 * netputpkt -- wrap data in packet and transmit.
 * Waits for acknowledgment and retransmits as necessary
 */
void
netputpkt(ULONG fd, char *buf, int cnt, int pkt_type)
{
	register char *cp;
	register int i, cc;
	ULONG wcc;
	int ack_type;
	unsigned seq;
	char type_len;
	extern int Debug;
	char 	*obuf = &putbuftmp[0];
	SIGNALHANDLER 	old_handler;
	ULONG	      	err;
	int 		old_alarm;

	if (pkt_type != DATA_PKTTYPE && pkt_type != ACK_PKTTYPE) {
		printf("*** netputpkt: ILLEGAL PACKET TYPE\n");
		return;
	}

	if (cnt > MAXPACKET) {
		printf("*** netputpkt: ILLEGAL PACKET SIZE\n");
		buf[MAXPACKET-1] =  NULL;
		printf("SIZE = %d, PKT = <<%s>>\n", cnt, buf);
		return;
	}

	/* restart here if timeout */
	if (pkt_type != ACK_PKTTYPE) {
		dprintf(("Putpkt (cpu %d): about to setjmp buf (0x%x)\n",
			cpuid(), &rexmit_buf[0]));
		dprintf(("Putpkt (cpu %d): accessing rexmitbuf\n", cpuid()));
		rexmit_buf[0] = 0x5; /* jfk */
		dprintf(("Putpkt (cpu %d): rexmitbuf accessed\n", cpuid()));

		/* we dont wait for ack to ack, hence not necessary */
		if (setjmp(rexmit_buf)) {
			bprintf(("(cpu %d), retransmitting packet len=%d buf=%s seq=0x%x type %s\r\n",
			    cpuid(), cnt, str_buf(buf, cnt), pkt_type == ACK_PKTTYPE ?
			    current_putseq: next_getseq,
			    pkt_type == ACK_PKTTYPE ? "ACK" : "DATA"));
			if (pkt_type == ACK_PKTTYPE) {
				printf("ACK packet not retransmitted\n");
				return;
			}
		}
	}

	dprintf(("Putpkt (cpu %d): jmp buf set\n", cpuid()));
	while (current_putseq != acked_putseq) {

		cc = 0; /* Index into buf */
		obuf[cc++] = SYN;
		dprintf(("Putpkt (cpu %d): Set SYN\n", cpuid()));


		type_len = pkt_type | ((cnt >> 6) & 0x1f) | 0x40;
		obuf[cc++] = type_len;			/* TYPE_LEN */

		dprintf(("Putpkt (cpu %d): Set typelen\n", cpuid()));

		obuf[cc++] = (cnt & 0x3f) | 0x40;	/* LEN1 */

		seq = (pkt_type == DATA_PKTTYPE)
		    ? current_putseq	/* sequence for data packets */
		    : next_getseq;	/* sequence for ack packets */
		obuf[cc++] = seq | 0x40;			/* SEQ */

		dprintf(("Putpkt (cpu %d): Set seq\n", cpuid()));

		cp = buf;
		for (i = cnt; i > 0; i--)			/* DATA */
			obuf[cc++] = *cp++;

		dprintf(("netputpkt: about to write (obuf = 0x%x, cc = %d\n", 
			obuf, cc));
		if ((err = Write(fd, obuf, cc, &wcc)) != ESUCCESS) {
			printf("Error in writing file (err = %d)\n", err);
			return;
		}

		dprintf(("netputpkt len=%d buf=%s seq=0x%x type %s\r\n",
		    cnt, str_buf(buf, cnt), seq,
		    pkt_type == ACK_PKTTYPE ? "ACK" : "DATA"));

		if (pkt_type == ACK_PKTTYPE)	/* don't send ACKs to ACKs */
			return;

		dprintf(("netputpkt still here!\n"));
		n_xmited++;

		old_handler = Signal (SIGALRM, rexmit_handler);
		old_alarm = alarm(REXMIT_TIME);

		ack_type = ACK_PKTTYPE;
		dprintf(("netputpkt calling netgetpkt!\n"));
		netgetpkt(fd, NULL, 0, &ack_type);

		Signal (SIGALRM, old_handler);
		alarm(old_alarm);
	}

	if (pkt_type == DATA_PKTTYPE) {
		current_putseq = (current_putseq + 1) & 0x3f;
		n_acked++;
	}
}

#endif /* NETDBX */
