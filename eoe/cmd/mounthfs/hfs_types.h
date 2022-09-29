/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1993 Silicon Graphics, Inc.                *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ifndef __HFS_TYPES_H__
#define __HFS_TYPES_H__

#ident "$Revision: 1.2 $"

/*
 * Some common hfs types.
 * 
 * Independent of all other hfs types.
 */


/*
 * hnode types
 */
typedef enum{
  nt_data=0,			/* Data fork */
  nt_dir=0,			/* Directory */
  nt_resource=1,		/* Resource fork */
  nt_dotresource=2,		/* .HSResource directory */
  nt_dotancillary=3,		/* .HSAncillary file */
  nt_invalid=4} ntype_t;	/* Invalid type */


/*
 * hnode number
 */
typedef u_int hno_t;

#define HNO(fid,tag) ((fid<<2)|(tag&3))
#define HNO_NR(hno)  (hno & ~3)
#define TAG(hno) (hno&3)
#define FID(hno) (hno>>2)
#define ROOTHNO (2<<2)


/*
 * File ID  (HFS internal).
 */
typedef u_int fid_t;

/*
 * Internal key header
 */
typedef struct khdr{
  u_int	kh_node;		/* Number of leaf node containing this key */
    int kh_rec;			/* Number of record containing this key */
  u_int kh_gen;			/* Btree generation number of this key */
} khdr_t;

#define INVALIDATE_KHDR(k) k.kh_gen = 0xffffffff


/*
 * Record descriptor
 */

typedef struct rdesc{
  void   *data;
  u_int  count;
} rdesc_t;

#endif
