#if !defined(DISKINVENT_H)
#define DISKINVENT_H

#if defined(__cplusplus)
extern "C" {
#endif

/* Copyright 1995, Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 */

/* diskinvent.h
 *
 * Generic thread safe disk inventory routines.
 */

#include <sgidefs.h>
#include <sys/types.h>
#include <invent.h>        /* SGI system inventory */
#include <sys/stat.h>
#include <sys/dvh.h>       /* disk volume header info */
#include <sys/sysmacros.h> /* major()/minor() */
#include <limits.h>        /* PATH_MAX */
#include <sys/conf.h>

#define DSK_PART_VH  8         /* volheader partition */
#define DSK_PART_VOL 10        /* volume partition */
#define DSK_DEVNAME_MAX MAXDEVNAME
#define DSK_MAX_INVENTRIES 10  /* max # of inventory entries per graph node */

typedef int (*dsk_cbfunc)(char *, struct stat64 *, inventory_t *, int, void *);

/* prototypes */

int dsk_walk_drives(
	int (*)(char *, struct stat64 *, inventory_t *, int, void *),
	void *);
int dsk_walk_partitions(
	int (*)(char *, struct stat64 *, inventory_t *, int, void *),
	void *);

/* prototypes from pathinfo.c */

char *path_to_alias(char *, char *, int *);
char *dev_to_alias(dev_t, char *, int *);
char *diskpath_to_alias(char *, char *, int *);
char *diskpath_to_alias2(char *, char *, int *, inventory_t *, int);
char *path_to_vhpath(char *, char *, int *);
char *path_to_volpath(char *, char *, int *);
char *dev_to_partpath(dev_t, int, int, char *, int *, struct stat64 *);
char *path_to_partpath(const char *, int, int, char *, int *, struct stat64 *);
char *path_to_rawpath(char *, char *, int *, struct stat64 *);
char *dev_to_rawpath(dev_t, char *, int *, struct stat64 *);
int  path_to_partnum(char *);
int  dev_to_partnum(dev_t);
int  path_to_ctlrnum(char *);
int  dev_to_ctlrnum(dev_t);

int  is_rootpart(char *);
__int64_t findsize(char *);
char *findrawpath(char *);
char *findblockpath(char *);

#if defined(__cplusplus)
}
#endif

#endif
