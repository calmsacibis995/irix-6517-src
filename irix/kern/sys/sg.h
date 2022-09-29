# ifndef __SYS_SG_H__
# define __SYS_SG_H__

/**************************************************************************
 *									  *
 * 		 Copyright (C) 1990, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ident "$Revision: 3.5 $"

/*
 * generic scatter / gather descriptor
 */
struct sg {
	unsigned long sg_ioaddr;
	unsigned long sg_bcount;
};

struct buf;
/* Prototype: */
extern int	sgset(struct buf *, struct sg *, int, unsigned *);

# endif  /* __SYS_SG_H__ */
