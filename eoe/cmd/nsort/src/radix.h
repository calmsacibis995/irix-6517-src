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
 * radix.h	- definitions particular to radix sorting only
 *
 *	$Ordinal-Id: radix.h,v 1.2 1996/07/12 00:47:58 charles Exp $
 *	$Revision: 1.1 $
 */

#ifndef _RADIX_H_ 
#define _RADIX_H_ 

#define RADIXSHIFT	5
#define RADIXMASK	((1 << RADIXSHIFT) - 1)
void radix_count(sort_t		*sort,
		 bucket_t	*in_bucket,
		 unsigned	shift,
		 list_desc_t	*out_list,
		 struct merge_args	*args);
void radix_count_rs(sort_t	*sort,
		 bucket_t	*in_bucket,
		 unsigned	shift,
		 list_desc_t	*out_list,
		 struct merge_args	*args);
void radix_disperse(sort_t	*sort,
		    bucket_t	*in_bucket,
		    bucket_t	*out_buckets, 
		    unsigned	shift,
		    list_desc_t	*list);
void radix_disperse_rs(sort_t	*sort,
		    bucket_t	*in_bucket,
		    bucket_t	*out_buckets, 
		    unsigned	shift,
		    list_desc_t	*list);
typedef void (*radix_disperse_fp)(sort_t	*sort,
			     bucket_t	*in_bucket,
			     bucket_t	*out_buckets, 
			     unsigned	shift,
			     list_desc_t	*list);
typedef void (*radix_count_fp)(sort_t	*sort,
		 bucket_t	*in_bucket,
		 unsigned	shift,
		 list_desc_t	*out_list,
		 struct merge_args	*args);

#define REC_WORDS 3
#define LINE_RECS (sizeof(((rad_line_t *) 0)->rec) / (REC_WORDS * sizeof(unsigned)))
#define COPY_ITEM(dest, src)	((sort)->itemcopier)((item_t *) (dest), (item_t *) (src), item_size)

#define	RADIXBITS	32

typedef u4 radix_key_t;

#endif /* _RADIX_H_ */
