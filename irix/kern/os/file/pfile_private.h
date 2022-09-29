/**************************************************************************
 *									  *
 *	 	Copyright (C) 1986-1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded	instructions, statements, and computer programs	 contain  *
 *  unpublished	 proprietary  information of Silicon Graphics, Inc., and  *
 *  are	protected by Federal copyright law.  They  may	not be disclosed  *
 *  to	third  parties	or copied or duplicated	in any form, in	whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ident	"$Id: pfile_private.h,v 1.1 1996/09/12 17:44:04 henseler Exp $"
#ifndef	_FILE_PFILE_PRIVATE_H_
#define	_FILE_PFILE_PRIVATE_H_

typedef struct pfile {
	off_t		pf_offset;
	mutex_t 	pf_offlock;
	bhv_desc_t	pf_bd;
} pfile_t;

#define BHV_TO_PFILE(bdp) \
	(ASSERT(BHV_OPS(bdp) == &pfile_ops), \
	(pfile_t *)(BHV_PDATA(bdp)))

#define VFILE_POSITION_PFILE	BHV_POSITION_BASE       /* chain bottom */

#endif /* _FILE_PFILE_PRIVATE_H_ */
