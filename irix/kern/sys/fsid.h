#ifndef __SYS_FSID_H__
#define __SYS_FSID_H__
/*
 * Filesystem type identifiers.
 *
 * Copyright 1992, Silicon Graphics, Inc.
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

#ident	"$Revision: 3.20 $"

/*
 * These file system names are used with the sysfs(2) call to
 * find the file system id to be used with mount(2), to mount a
 * particular type of file system.  Not all of the file systems
 * here can be directly mounted, some are used only internally.
 *
 * In the current implementation of user-mode file systems (dos,
 * iso9660, hfs) the sysfs(2) function does not recognize them.
 */

#define	FSID_EFS	"efs"		/* SGI Extent File System */
#define	FSID_NFS	"nfs"		/* Sun Network File System */
#define	FSID_NFS2	"nfs2"		/* Sun Network File System V2 */
#define FSID_NFS3	"nfs3"		/* Sun Network File System V3.0 */
#define	FSID_PROCFS	"proc"		/* process filesystem */
#define	FSID_DBG	FSID_PROCFS	/* old style debugging interface */
#define	FSID_SOCKET	"socket"	/* sockets (internal) */
#define	FSID_SPECFS	"specfs"	/* special devices (internal) */
#define FSID_FD		"fd"		/* /dev/fd file system */
#define FSID_NAMEFS	"namefs"	/* Named streams (fattach(3C)) */
#define FSID_FIFOFS	"fifofs"	/* SVR4 streams pipes (internal) */
#define FSID_DOS	"dos"		/* DOS FAT file system */
#define FSID_ISO9660	"iso9660"	/* CD-ROM */
#define FSID_CDFS	"cdfs"	/* CD-ROM (ABI requires this; it's just iso9660) */
#define FSID_MAC	"hfs"		/* Apple hierarchical file system */
#define FSID_AUTOFS	"autofs"	/* Sun automount file system */
#define FSID_CACHEFS	"cachefs"	/* Sun cache file system */
#define FSID_LOFS	"lofs"		/* Sun loopback file system */
#define	FSID_XFS	"xfs"		/* SGI xFS File System */
#define FSID_HWGFS	"hwgfs"		/* hardware graph /dev/hw */

#endif /* __SYS_FSID_H__ */
