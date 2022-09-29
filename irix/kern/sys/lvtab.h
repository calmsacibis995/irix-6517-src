
/**************************************************************************
 *									  *
 * 		 Copyright (C) 1989, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ifndef __SYS_LVTAB_H__
#define __SYS_LVTAB_H__

#ifdef __cplusplus
extern "C" {
#endif

#ident "$Revision: 1.8 $"
#include <standards.h>
/* Structure for representing an /etc/lvtab entry. */
/* Author: Dave Higgen (daveh) @ SGI */

#if _SGIAPI
#define MAXVNAMELEN 	80
#define MAXVDEVNAMELEN 	5
#define MAXLVDEVS 	100
#define MAXLVGRAN 	1024
#define MAXLVKEYLEN	8

struct lvtabent	{

	char		*devname;	/* volume device name */
	char		*volname;	/* volume name (human-readable) */
	unsigned	stripe;		/* number of ways striped */
	unsigned	gran;		/* granularity of striping */
	unsigned	ndevs;		/* number of constituent devices.
					 * NOTE: this is basic count, NOT
					 * including any mirror devices*/
	int		mindex;		/* index in pathnames where mirror
					 * device pathnames start if present. */
	char		*pathnames[1];	/* pathnames of constituent devices */
	};


extern void freelvent(struct lvtabent *);
extern struct lvtabent *getlvent( FILE *);
#endif	/* _SGIAPI */

#ifdef __cplusplus
}
#endif

#endif  /* __SYS_LVTAB_H__ */
