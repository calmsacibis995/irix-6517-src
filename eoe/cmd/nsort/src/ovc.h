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
 * ovc.h	- headers for offset value code based sorting
 *
 *	This file is only for record/pointer/variable sort specific C files.
 *	Generally it should be used only in C files which are compiled into
 *	several difference object files, based on e.g., -DREC_OUT/PTR_OUT
 *
 *	$Ordinal-Id: ovc.h,v 1.14 1996/07/22 22:06:02 charles Exp $
 *	$Revision: 1.1 $
 */

#ifndef _OVC_H
#define _OVC_H

#ifndef _ORDINAL_H
#include "ordinal.h"
#endif

#if defined(REC_LINE) || defined(REC_SKIP) || defined(REC_OUT)
# define RECORD_SORT
#elif !defined(RECORD_SORT) && !defined(POINTER_SORT)
# define POINTER_SORT
#endif

#if defined(VARIABLE_SORT) || (!defined(RECORD_SORT) && !defined(POINTER_SORT))
#define POINTER_SORT
#endif

/*
** An offset value code has two parts:
**	first, a 16 bit offset
**	second, a 16 bit offset value which is derived from the two bytes
**		of the 'conditioned' key.  The conditioning permits comparing
**		the offset value as an unsigned integer, even though the
**		real type of the key may be signed, floating point, var char,
**
** The offset in offset value coding is always in the same units as the
** size of the value code.  Here, the offset is in units of 2 bytes,
** since the value part is two bytes. These sizes permit keys to be up to
** 128K - 2 bytes long.  We'll use only 64K keys, since records are only 64K.
**
** Multiple keys are handled as if they were concatenated together.
** Variable length fields are handled as if they were padded out to their
** maximum length with 'trailing ignore' bytes, such as blanks for character
** strings and nulls for binary strings.  This padding ensure that the
** ovc.offset for the key after a variable length key will be the same for
** all records of a sort:
**
**		var char 20	integer
**		abcdefghijkl	50
**		ab		4
**
**	The offset of the integer is 10 (20 bytes into the composite key)
** Once this padding is done, fields are put into the composite key directly
** adjacent. No alignment to the natural short, long, or double boundaries
** is done.
**	char 1, short, integer
**	ovc.value	ovc.eov
**	c0 s0 		s1 i0 i1 i2
** The final byte of the integer will be fetched from the keys when the
** recode function has found the char, short, and the first three bytes of
** the integer are equal to the other value. Then the first byte of ovc.value
** will be set to the last byte of the integer. If there are more key fields
** after the integer, then the first byte of the next field goes into the
** second byte of ovc.value; else the second byte will be zeroed.
**
*/

#define WIN	0
#define CURR	1


#if defined(RECORD_SORT)

typedef u1 ovcoff_t;
typedef u1 valuecode_t[3];
#define ITEM_SIZE(sort)	((sort)->item_size)
#define RECODE(s, a, new, ins) (s->recode)(s, a, (item_t *) &(new)->extra.data, (item_t *) &(ins)->extra.data)
# define OVCEOVSIZE      (sizeof(valuecode_t))		/* three bytes */
# define ASSIGN_OVC(which, offset, valuecode) \
	((ovc_t *) &args->win_ovc)[which] = BUILD_OVC(offset, valuecode)
# define ASSIGN_EOV(which, eov)		/* null, record sorts have no eov */

#define	ITEM_REC(item)	((byte *) &(item)->extra.data)
/* radix hash values (not -meth=prehash hash values) for a *record* sort
 * have 31 significant bits. 128 ovc offsets are reserved for hash values
 * 0xff is virgin, then 0xfe through 0x7f are hash values, then the first
 * real data ovc offset is 0x7e. The offsets of 1 and 2 are reserved for
 * delete dup and kept dup, so there are 125 real data offsets left - enough
 * to handle 375 byte wide records sorts, amply more than is sensible for a r.s.
 */
#define HASH_OVC_CONSUME	128
#define HASH_INITIAL_OFFSET	0	/* for use in BUILD_OVC() */

#define OVC_DEC_DIGITS	6	/* 1 digit per nibble of the ovc */
#else	/* now POINTER_SORT */

typedef u2 ovcoff_t;
typedef u2 valuecode_t;
#define ITEM_SIZE(sort)	((unsigned) sizeof(item_t))
# define RECODE(s, a, new, ins) (s->recode)/*recode_pointer*/(s, a, new, ins)
# define OVCEOVSIZE      (2 * sizeof(valuecode_t))
# define ASSIGN_OVC(which, off, vc) \
	((which) == WIN ? win_item : curr_item)->ovc = BUILD_OVC(off, vc)
# define ASSIGN_EOV(which, eov) \
	((which) == WIN ? win_item : curr_item)->extra.var_extra.var_eov = (eov)

#define	ITEM_REC(item)	((item)->rec)
/* radix hash values (not -meth=prehash hash values) for a *pointer* sort
 * has 32 (31?) significant bits. two ovc offsets are reserved for hash values.
 * 0xffff is virgin, 0xfffe and 0xfffd indicate that the low two bytes of
 * the ovc and the first two bytes of the eov (i.e. var_extra.var_eov) contain
 * the hash value. the first real data ovc offset is 0xfffc.
 * Pointer sorts's initial hash ovc is for the highest offset, using the top 16
 * bits of the hash, the next one (HASH_INITIAL_OFFSET+1) has the low 16 bits.
 */
#define HASH_OVC_CONSUME	2
#define HASH_INITIAL_OFFSET	(-1)	/* for use in BUILD_OVC() */

#define OVC_DEC_DIGITS	8	/* 1 digit per nibble of ovc */
#endif /* of POINTER_SORT */

#define OFFSET_SHIFT	(sizeof(valuecode_t) << 3)	/* 16 or 24 */
#define VALUE_BITS	OFFSET_SHIFT
#define VALUE_MASK	((1 << OFFSET_SHIFT) - 1)	/* 0xFFFF or 0xFFFFFF */
#define OFFSET_MASK	(~VALUE_MASK)

#define VIRGIN_OFFSET	((1 << (sizeof(ovcoff_t) << 3)) - 1)	/* ff or ffff */
#define DEL_DUP_OVC_OFF  (VIRGIN_OFFSET - 1 - HASH_OVC_CONSUME)
#define KEPT_DUP_OVC_OFF (VIRGIN_OFFSET - 2 - HASH_OVC_CONSUME)
#define DEL_DUP_OVC	BUILD_OVC(DEL_DUP_OVC_OFF, 0)
#define KEPT_DUP_OVC	BUILD_OVC(KEPT_DUP_OVC_OFF, 0)
#define VIRGIN_OVC	(VIRGIN_OFFSET << OFFSET_SHIFT)

#define HASH_HIGHEST_OFFSET	(VIRGIN_OFFSET - 1)		/* fe or fffe */
#define HASH_LOWEST_OFFSET	(VIRGIN_OFFSET - HASH_OVC_CONSUME) /* 7f/fffd */
#define DATA_HIGHEST_OFFSET	(HASH_LOWEST_OFFSET - 1)	/* 7e or fffc */

/*
** For the generic recode function
*/
#define OVC_FETCHKEYS	0
#define OVC_EOV1	1
/* #define	OVC_EOV2 2	defunct: was used under THREE_PART_PTR_EOV */

#define BUILD_OVC(offset, value_code)	\
	(((HASH_LOWEST_OFFSET - (offset)) << OFFSET_SHIFT) + (value_code))

#define OFFSET_1	BUILD_OVC(1, 0)	/* 0x7e000000 or 0xFFFC0000 */
#define OFFSET_2	BUILD_OVC(2, 0)	/* 0x7d000000 or 0xFFFB0000 */
#define OFFSET_3	BUILD_OVC(3, 0)	/* 0x7c000000 or 0xFFFA0000 */
#define OFFSET_4	BUILD_OVC(4, 0)	/* 0x7b000000 or 0xFFF90000 */
#define OFFSET_5	BUILD_OVC(5, 0)	/* 0x7a000000 or 0xFFF80000 */

#define GET_OVC_OFFSET(ovc)	*((ovcoff_t *) &(ovc))

#if defined(CONST_EDITED_SIZE)

#define EDITED_SIZE(sort) CONST_EDITED_SIZE

# if defined(RECORD_SORT)
# undef  ITEM_SIZE
# define ITEM_SIZE(sort) ((int) ROUND_UP(CONST_EDITED_SIZE, 4) + 4)
# endif
#else

#define EDITED_SIZE(sort) ((sort)->edited_size)

#endif

#if defined(VARIABLE_SORT)
#define KEY_LENGTH(key, data, till_eorec)	\
	(IsDelimType((key)->type) ? get_delim_length((data), (key), (till_eorec)) \
				  : ((key)->length))
#else
#define KEY_LENGTH(key, data, till_eorec)	((key)->length)
#endif

#endif /* _OVC_H */
