/*
 * Copyright 1988 Silicon Graphics, Inc.  All rights reserved.
 *
 * AMD7990 device driver utilities and generic operations.
 */
#ident "$Revision: 1.7 $"

#include "sys/types.h"
#include "sys/debug.h"
#include "sys/systm.h"
#include "sys/errno.h"
#include "net/multi.h"
#include "net/soioctl.h"
#include "bstring.h"
#include "lance.h"

static int      laf_hash(u_char *, int);

/*
 * Set up a 7990's initialization block given the state in its
 * etherif struct (Ethernet address, whether it is promiscuous),
 * and in its lanceif struct (its logical address filter), and
 * the receive and transmit descriptor addresses.
 */
void
lance_init(lif, flags, rdra, tdra, rlen, tlen)
	struct lanceif *lif;
	int flags;
	u_int rdra, tdra, rlen, tlen;
{
	volatile struct ib *ib;

	ib = lif->lif_ib;
	bzero((caddr_t) ib, sizeof *ib);
	if (flags & IFF_PROMISC)
		ib->ib_mode = IB_PROM;
	ib->ib_padr[0] = lif->lif_eif.eif_arpcom.ac_enaddr[1]; 
	ib->ib_padr[1] = lif->lif_eif.eif_arpcom.ac_enaddr[0];
	ib->ib_padr[2] = lif->lif_eif.eif_arpcom.ac_enaddr[3];
	ib->ib_padr[3] = lif->lif_eif.eif_arpcom.ac_enaddr[2];
	ib->ib_padr[4] = lif->lif_eif.eif_arpcom.ac_enaddr[5];
	ib->ib_padr[5] = lif->lif_eif.eif_arpcom.ac_enaddr[4];
	if (flags & IFF_ALLMULTI)
		LAF_SETALL(&ib->ib_laf);
	else 
		ib->ib_laf = lif->lif_laf;
	ib->ib_rlen = rlen;
	ib->ib_lrdra = LO_ADDR(rdra);
	ib->ib_hrdra = HI_ADDR(rdra);
	ib->ib_tlen = tlen;
	ib->ib_ltdra = LO_ADDR(tdra);
	ib->ib_htdra = HI_ADDR(tdra);
}

int
lance_ioctl(lif, cmd, data)
	struct lanceif *lif;
	int cmd;
	caddr_t data;
{
	struct etherif *eif;
	struct mfreq *mfr;
	union mkey *key;
	int error;

	eif = &lif->lif_eif;
	mfr = (struct mfreq *) data;
	key = mfr->mfr_key;
	error = 0;

	switch (cmd) {
	  case SIOCADDMULTI:
		mfr->mfr_value = laf_hash(key->mk_dhost, sizeof key->mk_dhost);
		if (LAF_TSTBIT(&lif->lif_laf, mfr->mfr_value)) {
			ASSERT(mfhasvalue(&eif->eif_mfilter, mfr->mfr_value));
			lif->lif_lafcoll++;
			break;
		}
		ASSERT(!mfhasvalue(&eif->eif_mfilter, mfr->mfr_value));
		LAF_SETBIT(&lif->lif_laf, mfr->mfr_value);
		if (!(eiftoifp(eif)->if_flags & IFF_ALLMULTI)) {
			error = eif_init(eif, eif->eif_arpcom.ac_if.if_flags);
			if (error)
				break;
		}
		if (lif->lif_nmulti == 0)
			eiftoifp(eif)->if_flags |= IFF_FILTMULTI;
		lif->lif_nmulti++;
		break;

	  case SIOCDELMULTI:
		if (mfr->mfr_value
		    != laf_hash(key->mk_dhost, sizeof key->mk_dhost)) {
			error = EINVAL;
			break;
		}
		if (mfhasvalue(&eif->eif_mfilter, mfr->mfr_value)) {
			/*
			 * Forget about this collision.
			 */
			--lif->lif_lafcoll;
			break;
		}
		/*
		 * If this multicast address is the last one to map the bit
		 * numbered by mfr->mfr_value in the filter, clear that bit
		 * and update the chip.
		 */
		LAF_CLRBIT(&lif->lif_laf, mfr->mfr_value);
		if (!(eiftoifp(eif)->if_flags & IFF_ALLMULTI)) {
			error = eif_init(eif, eif->eif_arpcom.ac_if.if_flags);
			if (error)
				break;
		}
		--lif->lif_nmulti;
		if (lif->lif_nmulti == 0)
			eiftoifp(eif)->if_flags &= ~IFF_FILTMULTI;
		break;

	default:
		error = EINVAL;
	}
	return error;
}

/*
 * Given a multicast ethernet address, this routine calculates the 
 * address's bit index in the logical address filter mask for the
 * AMD 7990 LANCE chip.
 *
 * Modified from CMC "User's Guide K1 Kernel Software For Ethernet
 * Node Processors", May 1986, Appendix D.
 */
#define CRC_MASK	0xEDB88320

static int
laf_hash(addr, len)
	u_char *addr;
	int len;
{
	u_int crc;
	u_char byte;
	int bits;

	for (crc = ~0; --len >= 0; addr++) {
		byte = *addr;
		for (bits = 8; --bits >= 0; ) {
			if ((byte ^ crc) & 1)
				crc = (crc >> 1) ^ CRC_MASK;
			else
				crc >>= 1;
			byte >>= 1;
		}
	}
	return (crc >> 26);
}
