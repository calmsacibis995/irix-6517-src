/*
 ****************************************************************************
 *    Copyright © 1996 Ordinal Technology Corp.  All Rights Reserved.       *
 *                                                                          *
 *    DO NOT DISCLOSE THIS SOURCE CODE OUTSIDE OF SILICON GRAPHICS, INC.    *
 *  This source code contains unpublished proprietary information of        *
 *  Ordinal Technology Corp.  The copyright notice above is not evidence    *
 *  of any actual or intended publication of this information.              *
 *      This source code is licensed to Silicon Graphics, Inc. for          *
 *  its personal use in developing and maintaining products for the IRIX    *
 *  operating system.  This source code may not be disclosed to any third   *
 *  party outside of Silicon Graphics, Inc. without the prior written       *
 *  consent of Ordinal Technology Corp.                                     *
 ****************************************************************************
 *
 *
 *	$Ordinal-Id: hash.c,v 1.4 1996/10/02 23:53:52 charles Exp $
 */
#ident	"$Revision: 1.1 $"

#include "otcsort.h"

/*
 * compute_hash - compute a hash value based on the record's key fields
 *
 * Hash function semi-based on Knuth chapter 6.4
 * This code is duplicated in recode() and init_item()
 */
#if defined(HASHFAST)
unsigned calc_hash_fast(sort_t *sort, const byte *rec, unsigned rec_len)
#else
unsigned calc_hash(sort_t *sort, const byte *rec, unsigned rec_len)
#endif
{
    keydesc_t	*key = sort->keys;
    unsigned	knuth_golden = KNUTH_GOLDEN;
    unsigned	hash = 0;
    unsigned	key_len;
    int		i;
    const byte	*data;
#if defined(FASTHASH)
    const byte	*end;
    unsigned	word;
#endif

    do
    {
	/* The following section is taken from nrecode.c
	 */
	i = key->position;
#if defined(MOVABLE_KEYS)
	if (key->flags & FIELD_SHIFTY)	/* position of this key isn't constant*/
	{
	    if (item->extra.fpt->fpt[i] == FPT_UNSEEN)
		data = fill_fpt_entry(sort, item->extra.fpt, i);
	    else
		data = item->extra.fpt->record + item->id.fpt->fpt[i];
	}
	else if (sort->anyfpt)	/* no variable fields before this key */
	    data = (byte *) item->extra.fpt->record + i;
	else
#endif /* MOVABLE_KEYS */
	    data = rec + i;

	/* end of semi-duplicated section from recode.c */

	if (IsDelimType(key->type))
	    key_len = get_delim_length(data, key, rec_len - i);
	else
	    key_len = key->length;
#if defined(FASTHASH)
	/* This code is strangely ordered for better load-delay-slot filling
	 * It also may access up to three bytes of data beyond the end of
	 * the record (though it doesn't use them in the caculations)
	 * The nsort program uses this version.
	 */
	word = ulw(data);
	end = data + key_len;
	while ((data += sizeof(u4)) < end)
	{
	    hash ^= word * knuth_golden;
	    word = ulw(data);
	    hash = (hash << 1) | (hash >> 31);
	}
	/* Zero out the past past the end of the key field. If data == end then
	 * nothing is cleared, the  lowest byte if data == end + 1, etc.
	 * If the actual key length is zero, data == end + 4; skip the hash step
	 */
	if (data != end + sizeof(u4))
	{
	    word &= ~0 << ((data - end) << 3);
	    hash ^= word * knuth_golden;
	    hash = (hash << 1) | ((hash & 0x80000000) ? 1 : 0);
	}
#else	/* This safer version does not attempt unaligned reads of the data
	 * which might extend beyond the record's end and cross a page boundary.
	 * The chk program and the api version of nsort uses this
	 */
	for (i = key_len / (int) sizeof(u4); i != 0; i--, data += sizeof(u4))
	{
	    hash ^= ulw(data) * knuth_golden;
	    hash = (hash << 1) | ((hash & 0x80000000) ? 1 : 0);
	}
	if (i = (key_len % 4))
	{
	    switch (i)
	    {
	      case 1: i = data[0] << 24 ; break;
	      case 2: i = ulh(data) << 16; break;
	      /* the chk program may have an umapped segment immediately after
	       * the end of a record (unlike nsort), so don't use the simpler
	       * and faster ulw(data) &~0xff here, which could gen a sigsegv
	       * for prog. interface mention this: radix ptr sorts of
	       * trailing 3 byte reclens must have a valid fourth byte addr
	       */
	      case 3: i = (ulh(data) << 16) | (data[2] << 8); break;
	    }
	    hash ^= i * knuth_golden;
	    hash = (hash << 1) | ((hash & 0x80000000) ? 1 : 0);
	}
#endif

    } while ((++key)->type != typeEOF);

    return (hash);
}
