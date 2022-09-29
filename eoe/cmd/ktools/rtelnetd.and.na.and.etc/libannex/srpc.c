/*
 *****************************************************************************
 *
 *        Copyright 1989, Xylogics, Inc.  ALL RIGHTS RESERVED.
 *
 * ALL RIGHTS RESERVED. Licensed Material - Property of Xylogics, Inc.
 * This software is made available solely pursuant to the terms of a
 * software license agreement which governs its use.
 * Unauthorized duplication, distribution or sale is strictly prohibited.
 *
 * Module Description::
 *
 * 	Secure Remote Procedure Call (SRPC) library
 *
 * Original Author: Dave Harris		Created on: July 14, 1987
 *
 * Module Reviewers:
 *
 *	%$(reviewers)$%
 *
 * Revision Control Information:
 *
 * $Header: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/libannex/RCS/srpc.c,v 1.3 1996/10/04 12:09:23 cwilson Exp $
 *
 * This file created by RCS from:
 *
 * $Source: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/libannex/RCS/srpc.c,v $
 *
 * Revision History:
 *
 * $Log: srpc.c,v $
 * Revision 1.3  1996/10/04 12:09:23  cwilson
 * latest rev
 *
 * Revision 1.16  1993/12/30  13:14:21  carlson
 * SPR 2084 -- made compatible with 64 bit machines.
 *
 * Revision 1.15  1992/11/23  14:19:13  carlson
 * Annex code up to and including R7.0 can return a zero-length message
 * in response to a srpc call from na.  We have to accept these.
 *
 * Revision 1.14  92/11/13  14:37:30  carlson
 * SPR 1080 -- srpc_callresp now checks the sequence numbers on the
 * data received.
 * 
 * Revision 1.13  92/03/10  16:13:04  carlson
 * Added a few simple notes where the unfixable problems are.
 * 
 * Revision 1.12  91/08/01  18:06:44  emond
 * Made changes to force a signed char on certain machines which default
 * to unsighed chars, per Griff.
 * 
 * Revision 1.11  91/04/08  14:28:53  emond
 * ifndef'ed include of socket.h since we can optionally work with TLI instead.
 * 
 * Revision 1.10  89/10/16  17:29:38  loverso
 * Add workaround for broken Convergent compiler
 * 
 * Revision 1.9  89/04/13  16:19:53  loverso
 * further alignment fixes
 * 
 * Revision 1.8  89/04/05  12:40:24  loverso
 * Changed copyright notice
 * 
 * Revision 1.7  88/11/29  14:52:35  harris
 * Random_seed argument is treated as a long, so pass it as one.  Sun 4/Gould.
 * 
 * Revision 1.6  88/05/31  17:12:29  parker
 * fixes to get XENIX/SLIP to build again
 * 
 * Revision 1.5  88/05/25  09:15:58  parker
 * include file changes
 * 
 * Revision 1.4  88/05/19  18:01:19  harris
 * Changed encryption algortihm and SRPC protocol to be base on 16 byte
 * keys and translation tables generated from various keys.
 * 
 * Revision 1.3  88/05/10  17:21:23  harris
 * SYS_V uses rand() and srand() instead of random() and srandom().
 * 
 * Revision 1.2  88/05/05  16:52:45  harris
 * Enhanced encryption algorithm.  This allows wrong password detection.
 * 
 * Revision 1.1  88/05/04  17:39:55  harris
 * Initial revision
 * 
 * Revision 1.2  88/04/15  12:10:04  mattes
 * SL/IP integration
 * 
 * Revision 1.1  87/07/28  11:53:43  parker
 * merged in security
 * 
 *
 * This file is currently under revision by:
 *
 * $Locker:  $
 *
 *****************************************************************************
 */

#define RCSDATE $Date: 1996/10/04 12:09:23 $
#define RCSREV	$Revision: 1.3 $
#define RCSID   "$Header: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/libannex/RCS/srpc.c,v 1.3 1996/10/04 12:09:23 cwilson Exp $"
#ifndef lint
static char rcsid[] = RCSID;
#endif

/*
 *	Include Files
 */
#include "../inc/config.h"

#include "port/port.h"
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/uio.h>
#ifndef TLI
#include <sys/socket.h>
#endif
#include <strings.h>

#include "srpc.h"
#include "../inc/courier/courier.h"

extern	int	debug;
extern	time_t	time();
extern	char	*malloc();

#ifdef NEED_SIGNED_CHARS
#define signchar(x) (x & 0x80?(0xffffff00 | x):x)
#else
#define signchar(x) (x)
#endif

#ifndef	SYS_V
extern	INT32	random();
#endif

UINT32 random_long();

/*
 *	Define encryption tables to be used so we don't need to allocate
 *	memory in cases where new encryption tables are required.
 */

KEYDATA		rtable;		/*  place to hold rcv side crypt tables  */
KEYDATA		xtable;		/*  place to hold xmt side crypt tables  */


srpc_create(Srpc, s, to, pid, rpnum, rpver, rproc, key)

SRPC			*Srpc;	/*  pointer to srpc_state structure  */
int			s;	/*  socket file descriptor  */
struct sockaddr_in	*to;	/*  destination information  */
UINT32			pid,	/*  supposedly unique pep id  */
			rpnum;	/*  remote program number  */
unsigned short		rpver,	/*  remote program version #  */
			rproc;	/*  remote procedure number  */
KEYDATA			*key;	/*  ptr to encryption info  */
{
	int		retcd;	/*  return from srpc_callresp  */
	int		size;	/*  size of returned message  */
	int		i;	/*  loop  */
	SRPC_OPEN	open;	/*  SRPC open message parameters  */
	SRPC_RETN	retn;	/*  SRPC open returned parameters  */

	/*  Use key tables passed by client  */

	Srpc->xmt_key = key;

	/*  Randomly select new key - place in open message  */

	if(key && key->password[0])
	{
	    random_seed(Srpc->xmt_key->password);
	    Srpc->rcv_key = make_table((char *)0, &rtable);
	    (void)bcopy(Srpc->rcv_key->password, open.open_key, KEYSZ);
	}
	else
	{
	    random_seed("Mr. Mxzyptlk");
	    Srpc->rcv_key = (KEYDATA *)0;
	    for(i = 0; i < KEYSZ; i++)
		open.open_key[i] = '\0';
	}

	if(debug)
	{
	    printf("Host sent call with new key.\n");
	    printkey(open.open_key);
	    printf("Sequence was %08lX",Srpc->rcv_seq);
	}

	/*  Randomly select a SRPC handle and sequence number  */

	Srpc->srpc_id = random_long();
	Srpc->xmt_seq = Srpc->rcv_seq = random_long();	/* same for now */

	if (debug)
	    printf(", sequence changed to %08lX.\n",Srpc->rcv_seq);

	/*  Now we make procedure call, encrypted with shared key  */

	retcd =
	srpc_callresp(Srpc, s, to, pid, rpnum, rpver, rproc,
		      (char *)&open, sizeof(SRPC_OPEN),
		      SRPC_DELAY, SRPC_TIMEOUT,
		      (char *)&retn, sizeof(SRPC_RETN), &size);

	if(retcd)
	    return(retcd);

	if(debug)
	{
	    printf("Host received answer with final key\n");
	    printkey(retn.retn_key);
	}

	/*
	 *  The response has been decrypted using the key selected
	 *  above.  But now, we switch to the final key, returned to
	 *  us by the server, in the open return message.  Or null.
	 */

	if(key && key->password[0])
	    Srpc->xmt_key = make_table(retn.retn_key, &xtable);
	else
	    Srpc->xmt_key = (KEYDATA *)0;

	Srpc->rcv_key = Srpc->xmt_key;		/* Got it? */

	return S_SUCCESS;
}


srpc_answer(Srpc, s, to, pid, key, message, length)

SRPC			*Srpc;		/* pointer to SRPC state structure */
int			s;		/* file descriptor for socket */
struct sockaddr_in	*to;		/* destination address */
UINT32			pid;		/* PID to return to remote caller */
KEYDATA			*key;		/* encryption key */
char			*message;	/* message containing open call */
int			length;		/* length of "message" */
{
	int 		ret;		/* return code from srpc_return */
	int		i;		/* loop */
	SRPC_OPEN	data;		/* decrypted open message args */
	SRPC_RETN	response;	/* response (return args) */

	/*  point to encryption tables, presumably setup by client  */

	Srpc->rcv_key = key;

	/*  reject any message too short to have a SRPC header  */

	if(length < sizeof(SHDR))
	{
		erpc_reject(s, (struct sockaddr_in *)(0),
			    pid, CMJ_INVARG, 0, 0);
		if(debug)
		  printf("Message rejected: SRPC OPEN is too short!\n");

		return S_FRAGMENT;
	}
	else
	{
		srpc_decode(Srpc, message, (char *)&data, sizeof(SRPC_OPEN));

	/*
	 *	For backwards compatibilty - if message is shorter than
	 *	the current SRPC_OPEN message definition, we probably
	 *	have an open from an Annex running release R3.0, with
	 *	shorter key - assume that the key is zero.
	 */

		if(length < sizeof(SRPC_OPEN))
		    for(i = 0; i < KEYSZ; i++)
			data.open_key[i] = '\0';

		Srpc->srpc_id = ntohl(data.open_hdr.srpc_id);
		Srpc->rcv_seq = ntohl(data.open_hdr.sequence);
		Srpc->xmt_seq = ~Srpc->rcv_seq;

	/*
	 *	Create new encryption tables base on key just received
	 *	and newly selected key at random.  Or disable encryption.
	 */
		if(key && key->password[0])
		{
			random_seed(Srpc->rcv_key->password);
			Srpc->xmt_key = make_table(data.open_key, &xtable);
			Srpc->rcv_key = make_table((char *)0, &rtable);
			(void)bcopy(Srpc->rcv_key->password,
				    response.retn_key, KEYSZ);
		}
		else
		{
			Srpc->rcv_key = (KEYDATA *)0;
			for(i = 0; i < KEYSZ; i++)
			    response.retn_key[i] = '\0';
		}

	/*
	 *	Return the answer containing final key, encrypted with
	 *	the key just received in the open message, if any
	 */

		ret = srpc_return(
			Srpc, s, to, pid,
			(char *)&response, sizeof(SRPC_RETN));

		if(ret)
			return ret;

/*
 * This SHOULD be done, but the srpc protocol doesn't match its own
 * specification.  Someday this should be rewritten.
 */
		/* Srpc->xmt_seq++; */

	/*
	 *	Now synchronize rcv and xmt side encryption
	 */

		Srpc->xmt_key = Srpc->rcv_key;
	}

	return S_SUCCESS;
}

srpc_decode(Srpc, message, arg, nbytes)

SRPC		*Srpc;		/*  pointer to srpc_state structure  */
char		*message,	/*  message to be interpreted        */
		*arg;		/*  decrypted message and srpc head  */
int		nbytes;		/*  number of bytes in message       */
{
	if (Srpc->rcv_key && Srpc->rcv_key->password[0]) {
	    if (debug)
		printf("srpc_decode:  deciphering.\n");
	    cipher(message, arg, nbytes, Srpc->rcv_key);
	} else {
	    if (debug)
		printf("srpc_decode:  copying.\n");
	    bcopy(message, arg, nbytes);
	}
}

srpc_encode(Srpc, message, arg, nbytes)

SRPC		*Srpc;		/*  pointer to srpc_state structure  */
char		*message,	/*  message to be interpreted        */
		*arg;		/*  decrypted message and srpc head  */

int		nbytes;		/*  number of bytes in message       */
{
	if(Srpc->xmt_key && Srpc->xmt_key->password[0])
	  cipher(message, arg, nbytes, Srpc->xmt_key);
	else
	  bcopy(message, arg, nbytes);

	return;
}

srpc_cmp(Srpc, Shdr)			/*  validate SRPC header  */

SRPC	*Srpc;			/*  pointer to srpc_state structure  */
SHDR	*Shdr;			/*  pointer to recvd message header  */
{
	return (Srpc->srpc_id == ntohl(Shdr->srpc_id));
}

srpc_return(Srpc, s, to, pid, rdata, rlength)

SRPC			*Srpc;	/*  pointer to srpc_state structure  */
int			s;	/*  socket file descriptor  */
struct sockaddr_in 	*to;	/*  destination information  */
UINT32			pid;	/*  supposedly unique pep id  */
int			rlength;/*  length of return message  */
char			*rdata; /*  return message data area  */
{
	struct iovec	iov[2];
	int		iovlen = 1;	/*  erpc_return adds 1  */
	SHDR		*header;
	char		xdata[SRPC_MAX];

	header = (SHDR *)rdata;
	header->srpc_id = htonl(Srpc->srpc_id);
	header->sequence = htonl(Srpc->rcv_seq);

	srpc_encode(Srpc, rdata, xdata, rlength);

	iov[1].iov_base = xdata;
	iov[1].iov_len  = rlength;

	erpc_return(s, to, pid, iovlen, iov);

	return S_SUCCESS;
}


srpc_call(Srpc, s, to, pid, rpnum, rpver, rproc, cdata, clength)

SRPC			*Srpc;	/*  pointer to srpc_state structure  */
int			s,	/*  socket file descriptor  */
			clength;/*  length of call message  */
struct sockaddr_in	*to;	/*  destination information  */
UINT32			pid,	/*  supposedly unique pep id  */
			rpnum;	/*  remote program number  */
unsigned short		rpver,	/*  remote program version #  */
			rproc;	/*  remote procedure number  */
char			*cdata;	/*  call message data area  */
{
	struct iovec	iov[2];
	int		iovlen = 1;
	SHDR		*header;
	char		xdata[SRPC_MAX];

	header = (SHDR *)cdata;
	header->srpc_id = htonl(Srpc->srpc_id);
	header->sequence = htonl(Srpc->xmt_seq++);

	srpc_encode(Srpc, cdata, xdata, clength);

	iov[1].iov_base = xdata;
	iov[1].iov_len = clength;

	erpc_call(s, to, pid, rpnum, rpver, rproc, iovlen, iov); 

	return S_SUCCESS;
}


srpc_callresp(Srpc, s, to, pid, rpnum, rpver, rproc,
	      cdata, clength, delay, timeout,
	      rdata, rlength, status)

SRPC			*Srpc;	/*  pointer to srpc_state structure  */
int			s,	/*  socket file descriptor  */
			delay,	/*  expected turnaround time  */
			timeout,/*  maximum time for response  */
			clength,/*  length of call message  */
			rlength,/*  length of return message  */
			*status;/*  actual length, or abort/reject type  */
struct sockaddr_in	*to;	/*  destination information  */
UINT32			pid,	/*  supposedly unique pep id  */
			rpnum;	/*  remote program number  */
unsigned short		rpver,	/*  remote program version #  */
			rproc;	/*  remote procedure number  */
char			*cdata,	/*  call message data area  */
			*rdata;	/*  return message data area  */
{
	struct iovec	iov[2], riov[3];

	int		erpc_retcd,	/* return from erpc_callresp */
			rcode,		/* return code reject? abort? */
			alength,	/* actual length of return data */
			skipsend;
	SHDR		*header;
	CMRETURN	cmr;
	char		xdata[SRPC_MAX],
			zdata[SRPC_MAX];

	header = (SHDR *)cdata;
	header->srpc_id = htonl(Srpc->srpc_id);
	header->sequence = htonl(Srpc->xmt_seq++);

	if(debug)
		dump("call", cdata, clength);

	srpc_encode(Srpc, cdata, xdata, clength);

	skipsend = 0;

try_again:
	iov[1].iov_base = xdata;
	iov[1].iov_len = clength;
	riov[1].iov_base = (char *)&cmr;
	riov[1].iov_len = sizeof(CMRETURN);
	riov[2].iov_base = zdata;
	riov[2].iov_len = rlength;

	erpc_retcd =
	erpc_callresp(s, to, pid, rpnum, rpver, rproc,
		      1, iov, delay, timeout, 2, riov,
		      skipsend);

	skipsend = 1;

	if(erpc_retcd == -1)
	{
		*status = 0;
		return S_TIMEDOUT;
	}
	alength = erpc_retcd;		/* get nbytes returned by erpc */
	alength -= sizeof(CMRETURN);	/* subtract courier header size */

	if(alength < 0)
	{
		*status = 0;
		return S_FRAGMENT;
	}

	/*  Note that status of -1 for reject or abort indicates fragment  */

	if(rcode = rejected(riov, alength))
	{
		*status = rcode;
		return S_REJECTED;
	}

	if(rcode = aborted(riov, alength))
	{
		*status = rcode;
		return S_ABORTED;
	}
	*status = alength;
	srpc_decode(Srpc, zdata, rdata, alength);

	if (debug)
		dump("return", rdata, alength);

/* void data returned -- assume everything's ok. */
	if (alength < sizeof(SHDR))
		return S_SUCCESS;

	header = (SHDR *)rdata;
	if (ntohl(header->srpc_id) != Srpc->srpc_id ||
	    ntohl(header->sequence) != Srpc->xmt_seq) {
		if (debug)
			printf("srpc_callresp:  got ID/seq %X/%X, expected %X/%X.\n",ntohl(header->srpc_id),ntohl(header->sequence),Srpc->srpc_id,Srpc->xmt_seq);
		goto try_again;
	}

	return S_SUCCESS;
}


/*
 *	The routines rejected() and aborted() make a few assumptions about
 *	courier.  A Courier return message header is equal to a Courier
 *	message header, and two bytes shorter than an abort or reject.  The
 *	two bytes are used to store a reject or abort detail code, net-wise.
 */

int
rejected(iov, length)		/*  see if msg is a valid reject  */

struct	iovec	iov[];		/*  iovec - pep hdr, courier hdr, detail  */
int		length;		/*  calculated length of detail segment  */
{
	COUR_MSG	*Courier;

	Courier = (COUR_MSG *)iov[1].iov_base;

	switch (ntohs(Courier->cm_type))
	{
	    case C_RETURN:
	    case C_ABORT:

		return 0;
		break;

	    case C_REJECT:

		if(length < sizeof (unsigned short))
		  return -1;
		else
		  return (int)ntohs(*(unsigned short *)iov[2].iov_base);
		break;
	}
	return -1;
}

int
aborted(iov, length)		/*  see if msg is a valid abort  */

struct	iovec	iov[];		/*  iovec - pep hdr, courier hdr, detail  */
int		length;		/*  calculated length of detail segment  */
{
	COUR_MSG	*Courier;

	Courier = (COUR_MSG *)iov[1].iov_base;

	switch (ntohs(Courier->cm_type))
	{
	    case C_RETURN:
	    case C_REJECT:

		return 0;
		break;

	    case C_ABORT:

		if(length < sizeof (unsigned short))
		  return -1;
		else
		  return (int)ntohs(*(unsigned short *)iov[2].iov_base);
		break;
	}
	return -1;
}

#ifdef	SYS_V
#define	srandom	srand
#endif

random_seed(seed)

char	*seed;
{
	int	i;
	int	value = 0;

	/*
	 *  Hash 32-bits from a character string to seed srandom()
	 */

	while(*seed)
	{
		value *= 29;
		value += (*seed) % 31;
		*seed++;
	}

	srandom((int)(value ^ (int)time((time_t *)0)));

	return;
}

UINT32
random_long()
{
#ifdef	SYS_V

	return rand();
#else
	return (UINT32)random();
#endif
}

KEYDATA *
make_table(key, table)

char	*key;
KEYDATA	*table;
{
	if(!table)
	{
	    table = (KEYDATA *)malloc(sizeof(KEYDATA));

	    if(!table)
		goto nomem;
	}
	
	if(!key)
	    random_key(table->password);
	else
	    (void)bcopy(key, table->password, KEYSZ);

	generate_table(table);

nomem:
	return table;
}

random_key(password)

char	*password;
{
	int		i;
	UINT32		key;

	for(i = 0; i < KEYSZ; i += sizeof(UINT32))
	{
	    key = random_long();
	    (void)bcopy((char *)&key, &password[i], sizeof(UINT32));
	}

	for(i = 0; i < KEYSZ - 1; i++)
	    if(!password[i])
		password[i] = 0xff;

	password[KEYSZ - 1] = '\0';

	return;
}

generate_table(table)

KEYDATA	*table;
{
	int xc, i, k, temp;
	int slen;
	unsigned psuedo;
	INT32 seed, ptmp;
	char *buf = table->password;
	char *taba = table->table_a;
	char *tabb = table->table_b;
	char *tabc = table->table_c;

	slen = strlen(buf);
	if(!slen)
	  goto skipit;

	seed = 997;

	for (i = 0; i < slen; i++)
 		seed = seed * signchar(buf[i]) + i;

	for(i = 0; i < TABSZ; i++)
	{
		taba[i] = i;
		tabc[i] = '\0';
	}

	for(i = 0; i < TABSZ; i++)
	{
 		seed = 5 * seed + signchar(buf[i % slen]);  /*buf was signed */
		/*
		 * Use temp variable
		 * (workaround for compiler bug on Convergent)
		 */
		ptmp = seed % 65521;
		psuedo = ptmp;
		k = TABSZ - 1 - i;
		xc = (psuedo & MASK) % (k + 1);
		psuedo >>= 8;
		temp = taba[k];
		taba[k] = taba[xc];
		taba[xc] = temp;
		if(tabc[k] != 0) continue;
		xc = (psuedo & MASK) % k;
		while(tabc[xc] != 0) xc = (xc + 1) % k;
		tabc[k] = xc;
		tabc[xc] = k;
	}

	for(i = 0; i < TABSZ; i++)
		tabb[taba[i] & MASK] = i;
skipit:
	return;
}

cipher(src, dst, len, table)

char	*src;		/* pointer to message to be encrypted */
char	*dst;		/* pointer to place to put result */
int	len;		/* length of message */
KEYDATA	*table;		/* translation tables */
{
	int	i, xa, xb, y;
	char	*taba = table->table_a;
	char	*tabb = table->table_b;
	char	*tabc = table->table_c;	

	xa = xb = 0;

	for(i = 0; i < len; i++, src++, dst++)
	{
 		y = (signchar(taba[(signchar(*src)+xa)&MASK])+xb)&MASK;
 		*dst = signchar(tabb[(signchar(tabc[y])-xb)&MASK])-xa;
		xa++;
		xa &= MASK;
		if(!xa)
		{
			xb++;
			xb &= MASK;
		}
	}
}

dump(mesg, address, length)

char		*mesg;
char		*address;
int		length;
{
	unsigned char *data = (unsigned char *)address;
	int i;

	printf("%s: \n< ", mesg);

	for(i = 0; i < length; i++)
	    printf("%2.2x ", data[i]);

	printf(">\n");
}

printkey(string)
char	*string;
{
	dump("Key in hex",string,KEYSZ);
}
