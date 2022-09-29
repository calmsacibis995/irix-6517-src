
/*
 * @(#) $Id: rsvp_key.c,v 1.5 1998/11/25 08:43:14 eddiem Exp $
 */

/************************ rsvp_key.c  ********************************
 *                                                                   *
 *      Routines to compute and verify INTEGRITY objects	     *
 *                                                                   *
 *                                                                   *
 *********************************************************************/

/****************************************************************************

            RSVPD -- ReSerVation Protocol Daemon

                USC Information Sciences Institute
                Marina del Rey, California

	    Current Version:  Bob Braden, June 1996.

  Copyright (c) 1996 by the University of Southern California
  All rights reserved.

  Permission to use, copy, modify, and distribute this software and its
  documentation in source and binary forms for any purpose and without
  fee is hereby granted, provided that both the above copyright notice
  and this permission notice appear in all copies, and that any
  documentation, advertising materials, and other materials related to
  such distribution and use acknowledge that the software was developed
  in part by the University of Southern California, Information
  Sciences Institute.  The name of the University may not be used to
  endorse or promote products derived from this software without
  specific prior written permission.

  THE UNIVERSITY OF SOUTHERN CALIFORNIA makes no representations about
  the suitability of this software for any purpose.  THIS SOFTWARE IS
  PROVIDED "AS IS" AND WITHOUT ANY EXPRESS OR IMPLIED WARRANTIES,
  INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.

  Other copyrights might apply to parts of this software and are so
  noted when applicable.

********************************************************************/

/* This code adapted from an experimental implementation of
 * INTEGRITY by Prakash Jayaraman of USC.
 */

#include "rsvp_daemon.h"
#include "rsa_md5_global.h"
#include "rsa_md5c.h"

/* External declarations */
void		MD5Init(MD5_CTX *, UINT4 *);
void		MD5Update(MD5_CTX *, u_char *, u_int32_t);
void		MD5Final(u_char *, MD5_CTX *);
int		tolower(int);

/* Forward declarations
 */
void		incr_key_assoc_seqnos();
void		set_integrity(struct packet *, int, struct in_addr *);
void		fin_integrity(struct packet *);
KEY_ASSOC	*get_send_key(int);
KEY_ASSOC	*get_recv_key(INTEGRITY *);
KEY_ASSOC	*load_key(char *, char *);
int		hexcnv2(u_char *, char *);

/* Global variables
 */
INTEGRITY Auth_Obj;

#ifdef SECURITY


/*
 *	incr_key_assoc_seqnos(): Increment sequence numbers to send.
 */
void
incr_key_assoc_seqnos()
	{
	KEY_ASSOC	*kap;
	int		i;

	kap = &key_assoc_table[0];
	for (i= 0; i < Key_Assoc_Max; i++, kap++) {
		if (kap->kas_keylen && kap->kas_if >= 0)
			kap->kas_seqno++;
	}
}


/*
 *	set_integrity()
 *		Initialize INTEGRITY object, if one is required, for
 *		sending packet through specified vif.
 */
void
set_integrity(struct packet *pkt, int vif, struct in_addr  *srcp)
	{
	INTEGRITY	*intgp;
	KEY_ASSOC	*kap;

	if (!srcp)
		return;

	/*	Look up key for specified vif.  If none is found,
	 *	return without creating INTEGRITY object.
	 */
	kap = get_send_key(vif);
	if (!kap)
		return;

	/*	Create and initialize INTEGRITY object
	 */
	intgp = pkt->pkt_map->rsvp_integrity = &Auth_Obj;
	Init_Object(intgp, INTEGRITY, ctype_INTEGRITY_MD5_ipv4);
	intgp->intgr4_seqno = kap->kas_seqno;
	intgp->intgr4_keyid = kap->kas_keyid;
	intgp->intgr4_sender.s_addr = srcp->s_addr;
	memset(intgp->intgr4_digest, 0, MD5_LENG);
	memcpy(intgp->intgr4_digest, kap->kas_key, kap->kas_keylen);
}


/*
 *	fin_integrity()
 *		Compute crypto digest over given packet and complete
 *		the INTEGRITY object.
 */
void
fin_integrity(struct packet *pkt)
	{
	MD5_CTX		context;
	INTEGRITY	*intgp = pkt->pkt_map->rsvp_integrity;

	assert(intgp);

	/*	Initialize context block to crypto key, then zero
	 *	key field in packet digest to compute the rest.
	 *	Store final value back into digest field.
	 */
	MD5Init(&context, (u_int32_t *) intgp->intgr4_digest);
	memset(intgp->intgr4_digest, 0, MD5_LENG);
	MD5Update(&context, (u_char *) pkt->pkt_data, pkt->pkt_len);
	MD5Final(intgp->intgr4_digest, &context);
}
	
int
check_integrity(struct packet *pkt)
	{
	u_char		received_digest[MD5_LENG];
        MD5_CTX		context;
	KEY_ASSOC	*kap;
	INTEGRITY	*intp = pkt->pkt_map->rsvp_integrity;

	memcpy(received_digest, intp->intgr4_digest, MD5_LENG);
	kap = get_recv_key(intp);
	if (!kap)
		return PKT_ERR_NOINTASS;

	/*	Check sequence number.  A very small seq no is always
	 *	accepted, assuming other end restarted ( > 0 because
	 *	first few refreshes might have been list).
	 */
	if (intp->intgr4_seqno > MIN_INTGR_SEQ_NO &&
			LT(intp->intgr4_seqno, kap->kas_seqno))
		 return PKT_ERR_REPLAY;

	/*	Recompute digest and store into INTEGRITY object.
	 *	in 3 steps.
	 */
        MD5Init (&context, (u_int32_t *) kap->kas_key); /* Assuming length 16 */
	memset(intp->intgr4_digest, 0, MD5_LENG);
        MD5Update (&context, (u_char *) pkt->pkt_data, pkt->pkt_len); 
        MD5Final (intp->intgr4_digest, &context);

	/*
	 *	Check that recomputed digest matches what arrived.
	 */
        if (memcmp(received_digest, intp->intgr4_digest, MD5_LENG))
		return PKT_ERR_INTEGRITY;

	intp->intgr4_seqno = kap->kas_seqno;
	return(0);  /* OK */
}


/*	Return pointer to first entry of the Key Assocation table for
 *	given send interface, or NULL.
 */
KEY_ASSOC *
get_send_key(int vif)
	{
	KEY_ASSOC	*kap;
	int		i;

	kap = &key_assoc_table[0];
	for (i= 0; i < Key_Assoc_Max; i++, kap++) {
		if (kap->kas_keylen && kap->kas_if == vif)
			break;
	}
	if (i == Key_Assoc_Max)
		return(NULL);
	return(kap);
}


/*	Return pointer to receive entry of the Key Assocation table for
 *	given sender address and keyid, or NULL.
 */
KEY_ASSOC *
get_recv_key(INTEGRITY *intp)
	{
	KEY_ASSOC	*kap;
	int		i;

	kap = &key_assoc_table[0];
	for (i= 0; i < Key_Assoc_Max; i++, kap++) {
		if (kap->kas_keylen && kap->kas_if < 0 &&
		    kap->kas_keyid == intp->intgr4_keyid &&
		    kap->kas_sender.s_addr == intp->intgr4_sender.s_addr)
			break;
	}
	if (i == Key_Assoc_Max)
		return(NULL);
	return(kap);
}


KEY_ASSOC *
load_key(char *keyidstr, char *keystr)
	{
	KEY_ASSOC	*kap;
	u_char		ascikey[2*MD5_LENG], *cp;
	int 		i;

	kap = &key_assoc_table[0];
	for (i= 0; i < Key_Assoc_Max; i++, kap++)
		if (kap->kas_keylen == 0)
			break;

	if (i == Key_Assoc_Max) {
		if (i == KEY_TABLE_SIZE) {
			log(LOG_ERR, 0, "Too many key associations\n");
			return(NULL);
		}
		Key_Assoc_Max++;
	}
	kap->kas_keyid = atoi(keyidstr);
	if (strlen(keystr) > 2*MD5_LENG) {
		log(LOG_ERR, 0, "Send key too long\n");
		return(NULL);
	}
	/*
	 *	Convert hex key string
	 */
	memset(kap->kas_key, 0, MD5_LENG);
	memset(ascikey, '0', 2*MD5_LENG);
	/* keep compiler quiet - mwang */
	strcpy((char *) (cp = ascikey), keystr);

	for (i = 0; i < MD5_LENG; i++) {
		/* keep compiler quiet - mwang */
		if (!hexcnv2(&kap->kas_key[i], (char *)cp)) {
			log(LOG_ERR, 0, "Bad hex char: %c\n", *cp);
			exit(1);
		}
		cp += 2;
	}
	kap->kas_keylen = MD5_LENG;
		
	return(kap); /* OK */
}

char hextab[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
		 'a', 'b', 'c', 'd', 'e', 'f'};

int
hexcnv2(u_char *rp, char *cp)
	{
	char *xp = strchr(hextab, tolower(*cp++));
	char *yp = strchr(hextab, tolower(*cp));

	if (!xp || !yp)
		return 0;
	*rp = ((xp-hextab)<<4) + (yp-hextab);
	return 1;
}

#endif /* SECURITY */
