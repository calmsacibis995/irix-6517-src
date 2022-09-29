#ifndef __SYS_PROFIL_H__
#define __SYS_PROFIL_H__		/* Prevent nested includes */

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************
 *									  *
 * 		 Copyright (C) 1990-1993, Silicon Graphics, Inc.	  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident	"$Revision: 1.16 $"

#include <sys/types.h>		/* needed for __psunsigned_t */

#ifdef R10000
#include <sys/hwperftypes.h>
#endif

struct prof {
	void	*pr_base;	/* buffer base */
	unsigned pr_size;	/* buffer size */
	__psunsigned_t pr_off;	/* pc offset */
	unsigned pr_scale;	/* pc scaling */
};

#define PROF_USHORT	0x0001	/* treat pr_base as unsigned short array */
#define PROF_UINT	0x0002	/*		 as unsigned integer array */
#define PROF_FAST	0x0004	/* profile PROF_FAST_CNT times per clock tick */

#define PROF_FAST_CNT	10	/* number of fast prof ticks per clock tick */

/*
 * Maximum number of prof structures that can be passed as an argument to
 * the sprofil(2) system call.  This can be changed at sysgen time by
 * changing the parameter "nprofile" in the file /var/sysgen/mtune/kernel.
 * The kernel actually doesn't pay any attention to the constant below; it's
 * only present for backwards compatibility and will be removed at some point
 * in the future.
 */
#define PROFIL_MAX	100

#if defined(_KERNEL) || defined(_KMEMUSER)
extern int nprofile;

struct irix5_prof {
	app32_ptr_t	pr_base;	/* buffer base */
	unsigned	pr_size;	/* buffer size */
	unsigned	pr_off;		/* pc offset */
	unsigned	pr_scale;	/* pc scaling */
};

struct irix5_64_prof {
	app64_ptr_t	pr_base;	/* buffer base */
	unsigned	pr_size;	/* buffer size */
	app64_ptr_t	pr_off;		/* pc offset */
	unsigned	pr_scale;	/* pc scaling */
};

extern int sprofil(void *, int, void *, unsigned int, int);

#ifdef R10000
extern int evc_profil(hwperf_profevctrarg_t *, void *, int, unsigned int, int *);
#endif

#else /* !_KERNEL && !_KMEMUSER */

extern int profil(unsigned short *, unsigned int, unsigned int, unsigned int);
extern int sprofil(struct prof *, int, struct timeval *, unsigned int);
#endif /* _KERNEL && !_KMEMUSER */

#ifdef __cplusplus
}
#endif

#endif /* __SYS_PROFIL_H__ */
