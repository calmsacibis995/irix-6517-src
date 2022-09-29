#ifndef __net_multi__
#define __net_multi__
#ifdef __cplusplus
extern "C" { 
#endif
/*
 * Generic filter for use by network interfaces whose devices cannot
 * perfectly filter multicast packets.  Such a network interface should
 * set the IFF_FILTMULTI flag in its ifnet structure, allocate a multicast
 * filter in its software control structure, and initialize it with:
 *
 *	 ok = mfnew(&softc->mf, count);
 *
 * Count is the initial hash table size; mfnew returns 0 if it cannot
 * allocate a hash table, 1 otherwise.
 *
 * The ifnet's ioctl function should interpret the SIOC{ADD,DEL}MULTI
 * commands by calling mfadd and mfdel to add and delete filters.
 * The interface receive interrupt handler should drop any multicast
 * packets for which mfmatch returns 0.
 *
 * $Revision: 1.16 $
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

/*
 * The multicast filter is implemented as a hash table which associates
 * multicast addresses with an opaque value.  The key is a union of the
 * beginning of an unspecified sockaddr with two long ints.
 */
union mkey {
	struct mkey_addr {
		u_short	mka_family;	/* address family, always AF_UNSPEC */
		u_char	mka_dhost[6];	/* destination multicast address */
	} mk_addr;
	struct mkey_hash {
		u_int	mkh_high;	/* for the hash function */
		u_int	mkh_low;
	} mk_hash;
};
#define	mk_family	mk_addr.mka_family
#define	mk_dhost	mk_addr.mka_dhost
#define	mk_high		mk_hash.mkh_high
#define	mk_low		mk_hash.mkh_low

/* coerce a sockaddr pointer into an mkey pointer */
#define	satomk(sa)	((union mkey *) (sa))

/*
 * Each table entry maps a key onto flags and a value.
 */
typedef	u_short	mval_t;

struct mfent {
	union mkey	mfe_key;	/* multicast address */
	short		mfe_flags;	/* entry flags, see below */
	mval_t		mfe_value;	/* address filter info */
	u_int		mfe_refcnt;	/* count of # times added */
};

#define	MFE_ACTIVE	0x1		/* if set, this entry is allocated */
#define	MFE_COLLISION	0x2		/* an add collided with this entry */

/*
 * The hash table structure.
 */
struct mfilter {
	u_short		mf_shift;	/* log2(hash table size) */
	u_short		mf_numfree;	/* number of free entries */
	struct mfent	*mf_base;	/* dynamically allocated hash table */
	struct mfent	*mf_limit;	/* end of hash table */
	struct mfmeters {
		u_long	mm_lookups;	/* hash table lookups */
		u_long	mm_probes;	/* linear probes in hash table */
		u_long	mm_hits;	/* lookups which found key */
		u_long	mm_misses;	/* keys not found */
		u_long	mm_adds;	/* entries added */
		u_long	mm_grows;	/* table expansions */
		u_long	mm_shrinks;	/* table contractions */
		u_long	mm_deletes;	/* entries deleted */
		u_long	mm_dprobes;	/* entries hashed while deleting */
		u_long	mm_dmoves;	/* entries moved while deleting */
	} mf_meters;
};

#ifdef _KERNEL
/*
 * Multicast filter operations.  Mfmatch and mfadd return 1 for success
 * and 0 for failure.  If mfmatch{cnt}(mf, key, &val) returns 1, the value
 * associated with key will be returned in val.  To find a value for an
 * Ethernet destination address, call mfethermatch(mf, host, &val).
 * Use mfhasvalue(mf, val) to find whether val is mapped by mf.
 *
 * The match, add, delete and hasvalue ops must be called at splimp.
 */
int	mfnew(struct mfilter *, int);
void	mfdestroy(struct mfilter *);
int	mfmatch(struct mfilter *, union mkey *, mval_t *);
int	mfmatchcnt(struct mfilter *, int, union mkey *, mval_t *);
int	mfethermatch(struct mfilter *, u_char *, mval_t *);
int	mfadd(struct mfilter *, union mkey *, mval_t);
void	mfdel(struct mfilter *, union mkey *);
int	mfhasvalue(struct mfilter *, mval_t);

#endif	/* _KERNEL */
#ifdef __cplusplus
}
#endif 
#endif	/* !__net_multi__ */
