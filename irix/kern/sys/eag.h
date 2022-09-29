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

#ifndef __SYS_EAG_H__
#define __SYS_EAG_H__
#ident	"$Revision: 1.20 $"

/*
 * Data types for Extended Attributes (plan G).
 */

struct eag_mount_s {
	char	*spec;
	char	*dir;
	int	flags;
	char	*fstype;
	char	*dataptr;
	int	datalen;
};

#if defined(_KERNEL) || defined(_STANDALONE) || defined(_KMEMUSER)

struct irix5_eag_mount_s {
	app32_ptr_t	spec;
	app32_ptr_t	dir;
	app32_int_t	flags;
	app32_ptr_t	fstype;
	app32_ptr_t	dataptr;
	app32_int_t	datalen;
};

#endif /* defined(_KERNEL) || defined(_STANDALONE) || defined(_KMEMUSER) */

#define	EAG_MAX_ATTR_NAME	80	/* Maximum size for an attribute name */

/* function prototypes */

#ifdef _KERNEL
union rval;
struct vfs;
extern int eag_mount( struct eag_mount_s *, union rval *, char *);
extern int eag_parseattr(char *, char *, void *);
extern int proc_attr_set(char *, char *, int);
extern int proc_attr_get(char *, char *);
#else	/* _KERNEL */
extern int sgi_eag_mount(char *, char *, int, char *, char *, int, char *);
extern int sgi_eag_getprocattr(const char *, void *);
extern int sgi_eag_setprocattr(const char *, const void *, int);
#endif /* _KERNEL */

#endif /* __SYS_EAG_H__ */
