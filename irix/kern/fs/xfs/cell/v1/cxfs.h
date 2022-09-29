/**************************************************************************
 *									  *
 *		 Copyright (C) 1995-1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ifndef _CXFS_H_
#define _CXFS_H_
#ident	"$Revision: 1.3 $"

#include <sys/vnode.h>

#define CXFS_IFEXTENTS		0x01	/* Extents are present */
#define CXFS_MOREEXTENTS	0x02	/* More extents available */

#define CXFS_EXT_COUNT	512	/* Maximum extent count in one call */

extern	void	dsxvn_init(void);
extern	void	dcxvn_init(void);
extern	void	dcxvfs_init(void);

extern	int     cxfs_fetch_extents(bhv_desc_t *, char **, size_t *,
				   int *, off_t, size_t, int, void **);


#endif /* _CXFS_H_ */
