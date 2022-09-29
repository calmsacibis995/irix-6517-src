/*
 * A software multicast filter for network interfaces which control devices
 * lacking perfect filtering.
 *
 * $Revision: 1.18 $
 *
 * Copyright 1988, 1990, Silicon Graphics, Inc. 
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or 
 * duplicated in any form, in whole or in part, without the prior written 
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions 
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or 
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished - 
 * rights reserved under the Copyright Laws of the United States.
 */
#include <bstring.h>
#include "sys/debug.h"
#include "sys/types.h"
#include "sys/socket.h"
#include "sys/systm.h"
#include "netinet/if_ether.h"
#include "multi.h"

/*
 * Calculate the ceiling-log2 of an integer.
 */
#define	LOG2CEIL(n, c) { \
	unsigned int x = n; \
	c = (x & x-1) ? 1 : 0; \
	if (x & 0xffff0000) \
		c += 16, x &= 0xffff0000; \
	if (x & 0xff00ff00) \
		c += 8, x &= 0xff00ff00; \
	if (x & 0xf0f0f0f0) \
		c += 4, x &= 0xf0f0f0f0; \
	if (x & 0xcccccccc) \
		c += 2, x &= 0xcccccccc; \
	if (x & 0xaaaaaaaa) \
		c += 1; \
}

#define	MINSHIFT	4
#define	ALPHA(count)	((count) - (count) / 8)

/*
 * Initialize an mfilter to have room for at least count + 1 elements.
 * XXX should be void, is int because historically malloc could fail.
 */
int
mfnew(struct mfilter *mf, int count)
{
	int shift, size;
	caddr_t base;
	
	/*
	 * Reserve one empty slot for the hash probe sentinel.
	 */
	count++;
	LOG2CEIL(count, shift);
	if (shift < MINSHIFT)
		shift = MINSHIFT;
	count = 1 << shift;
	mf->mf_numfree = ALPHA(count);
	mf->mf_shift = shift;
	size = count * sizeof mf->mf_base[0];
	base = kern_malloc(size);
	bzero(base, size);
	mf->mf_base = (struct mfent *)base;
	mf->mf_limit = (struct mfent *)(base + size);
#ifdef OS_METER
	/* clear the meters */
	bzero(&mf->mf_meters, sizeof mf->mf_meters);
#endif
	return 1;
}

void
mfdestroy(struct mfilter *mf)
{
	kern_free(mf->mf_base);
}

/*
 * Multiplicative hash with linear probe.  Assumes ints are 32 bits.
 */
#define	PHI	2654435769	/* (golden mean) * (2 to the 32) */

#define	MFHASH(mf, mk) \
	(((PHI * ((mk)->mk_high ^ (mk)->mk_low)) & (0xffffffff)) \
			>> (32 - (mf)->mf_shift))

#define	KEYCMP(mk1, mk2) \
	((mk1)->mk_high != (mk2)->mk_high || (mk1)->mk_low != (mk2)->mk_low)

/*
 * Return a pointer to the entry matching mk if it exists, otherwise
 * to the empty entry in which mk should be installed.
 */
static struct mfent *
mflookup(struct mfilter *mf, union mkey *mk, int adding)
{
	struct mfent *mfe;

	METER(mf->mf_meters.mm_lookups++);
	mfe = &mf->mf_base[MFHASH(mf, mk)];

	ASSERT((__psunsigned_t)mfe < (__psunsigned_t)mf->mf_limit);
	ASSERT((__psunsigned_t)mfe >= (__psunsigned_t)mf->mf_base);

	while ((mfe->mfe_flags & MFE_ACTIVE) && KEYCMP(mk, &mfe->mfe_key)) {
		/*
		 * Because mf_numfree was initialized to 7/8 of the table
		 * size, we need not check for table fullness.  A probe is
		 * guaranteed to hit an empty (!MFE_ACTIVE) entry before
		 * revisiting the primary hash.
		 */
		METER(mf->mf_meters.mm_probes++);
		if (adding)
			mfe->mfe_flags |= MFE_COLLISION;
		if (mfe == mf->mf_base)
			mfe = mf->mf_limit;
		--mfe;
	}
	return mfe;
}

int
mfmatch(struct mfilter *mf, union mkey *mk, mval_t *mv)
{
	struct mfent *mfe;
	
	mfe = mflookup(mf, mk, 0);
	if (mfe->mfe_flags & MFE_ACTIVE) {
		METER(mf->mf_meters.mm_hits++);
		if (mv)
			*mv = mfe->mfe_value;
		return 1;
	}
	METER(mf->mf_meters.mm_misses++);
	return 0;
}

/*  
 * Reference-counted version of mfmatch --
 * if key not found	returns 0
 * if key found and
 *  refcnt>=0 (add):    returns 1
 *  refcnt<0 (delete):  returns 1 if key now has 0 references (ok to delete)
 * 			returns 0 if key has references (not ok to delete)
 */
int
mfmatchcnt(struct mfilter *mf, int refcnt, union mkey *mk, mval_t *mv)
{
	struct mfent *mfe;
	
	mfe = mflookup(mf, mk, 0);
	if (mfe->mfe_flags & MFE_ACTIVE) {
		METER(mf->mf_meters.mm_hits++);
		if (mv)
			*mv = mfe->mfe_value;
		mfe->mfe_refcnt += refcnt;
		return ((refcnt < 0) ? (mfe->mfe_refcnt == 0) :	1);
	}
	METER(mf->mf_meters.mm_misses++);
	return 0;
}

int
mfethermatch(struct mfilter *mf, u_char *addr, mval_t *mv)
{
	union mkey mk;

	mk.mk_family = AF_UNSPEC;
	ether_copy(addr, mk.mk_dhost);
	return mfmatch(mf, &mk, mv);
}

/*
 * Must be called at splimp.
 */
int
mfadd(struct mfilter *mf, union mkey *mk, mval_t value)
{
	struct mfent *mfe;

	if (mf->mf_numfree == 0) {
		struct mfilter tmf;

		/*
		 * Hash table is full, so double its size and re-insert all
		 * active entries.
		 */
		METER(mf->mf_meters.mm_grows++);
		mfnew(&tmf, (1 << (mf->mf_shift+1)) - 1);
		for (mfe = mf->mf_base; mfe < mf->mf_limit; mfe++) {
			if (mfe->mfe_flags == 0)
				continue;
			(void) mfadd(&tmf, &mfe->mfe_key, mfe->mfe_value);
		}
#ifdef OS_METER
		tmf.mf_meters = mf->mf_meters;	/* copy the meters */
#endif
		mfdestroy(mf);
		*mf = tmf;
	}
	mfe = mflookup(mf, mk, 1);
	if (mfe->mfe_flags & MFE_ACTIVE)	/* mk is already in mf */
		return 1;
	METER(mf->mf_meters.mm_adds++);
	ASSERT(mf->mf_numfree > 0);
	--mf->mf_numfree;
	mfe->mfe_flags = MFE_ACTIVE;
	mfe->mfe_key = *mk;
	mfe->mfe_value = value;
	mfe->mfe_refcnt = 1;
	return 1;
}

/*
 * Perhaps shrink table when appropriate.
 */
void
mfdel(struct mfilter *mf, union mkey *mk)
{
	struct mfent *mfe;

	mfe = mflookup(mf, mk, 0);
	if (!(mfe->mfe_flags & MFE_ACTIVE))
		return;
	METER(mf->mf_meters.mm_deletes++);
	ASSERT(mf->mf_numfree < ALPHA(1<<mf->mf_shift));
	mf->mf_numfree++;

	if (mf->mf_shift > MINSHIFT
	    && mf->mf_numfree == (1<<mf->mf_shift) - (1<<(mf->mf_shift-2))) {
		struct mfilter tmf;

		/*
		 * If only 1/8 of the table is active, shrink it.
		 */
		METER(mf->mf_meters.mm_shrinks++);
		mfnew(&tmf, (1 << (mf->mf_shift-1)) - 1);
		mfe->mfe_flags = 0;
		for (mfe = mf->mf_base; mfe < mf->mf_limit; mfe++) {
			if (mfe->mfe_flags == 0)
				continue;
			(void) mfadd(&tmf, &mfe->mfe_key, mfe->mfe_value);
		}
#ifdef OS_METER
		tmf.mf_meters = mf->mf_meters;	/* copy the meters */
#endif
		mfdestroy(mf);
		*mf = tmf;
	} else if (mfe->mfe_flags & MFE_COLLISION) {
		struct mfent *mfe2;
		int slot, slot2, hash;

		/*
		 * Move colliding entries up into the newly freed slot.
		 */
		slot = slot2 = mfe - mf->mf_base;
		for (;;) {
			mfe->mfe_flags = 0;
			do {
				if (--slot2 < 0)
					slot2 += 1 << (mf)->mf_shift;
				mfe2 = &mf->mf_base[slot2];
				if (mfe2->mfe_flags == 0)
					return;
				METER(mf->mf_meters.mm_dprobes++);
				hash = MFHASH(mf, &mfe2->mfe_key);
			} while (slot2 <= hash && hash < slot
				 || slot < slot2 && (hash < slot
						     || slot2 <= hash));
			METER(mf->mf_meters.mm_dmoves++);
			*mfe = *mfe2;
			mfe = mfe2;
			slot = slot2;
		}
	} else {
		/*
		 * Simple deletion (no shrinking, no collisions).
		 */
		mfe->mfe_flags = 0;
	}
}

int
mfhasvalue(struct mfilter *mf, mval_t value)
{
	struct mfent *mfe;

	for (mfe = mf->mf_base; mfe < mf->mf_limit; mfe++) {
		if ((mfe->mfe_flags & MFE_ACTIVE) && mfe->mfe_value == value)
			return 1;
	}
	return 0;
}
