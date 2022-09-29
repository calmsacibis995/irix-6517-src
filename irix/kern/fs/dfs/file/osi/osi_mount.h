/*
 * HISTORY
 * $Log: osi_mount.h,v $
 * Revision 65.1  1997/10/20 19:17:44  jdoak
 * *** empty log message ***
 *
 * $EndLog$
 */
/* Copyright (C) 1996, 1993 Transarc Corporation - All rights reserved */

#ifndef TRANSARC_OSI_MOUNT_H
#define	TRANSARC_OSI_MOUNT_H

/* $Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/osi/RCS/osi_mount.h,v 65.1 1997/10/20 19:17:44 jdoak Exp $ */

#ifdef HAS_OSI_MOUNT_MACH_H
#include <dcedfs/osi_mount_mach.h>
#endif

/*
 * Define standard filesystem names.  These values may also be used by
 * operating systems that use strings to identify filesystem types.
 */
#define OSI_FSTYPE_DFS	"dfs"
#define OSI_FSTYPE_LFS	"lfs"
#define OSI_FSTYPE_AGFS	"agfs"
#define OSI_FSTYPE_DMEPI	"dmepi"

/*
 * The osi_mount_mach.h files define two additional sets of constants.  The
 * first is a set of type values, suitable for passing as the `type' parameter
 * to osi_mount.  They are named:
 * 	OSI_MOUNT_TYPE_DFS
 *	OSI_MOUNT_TYPE_AGFS
 *	OSI_MOUNT_TYPE_LFS
 *
 * The second is a set of flag values, suitable for passing as the `flags'
 * parameter to osi_mount.  They are named:
 *	OSI_MOUNT_FLAGS_READONLY
 *	OSI_MOUNT_FLAGS_NOSUID
 * This set of flags values is meant to be the most common subset of the mount 
 * flags that are supported by any particular OS.
 */

/*
 * Mount a filesystem, where `devName' is loosely defined as the device to 
 * mount and `mp' is the name of the mount point.  The other parameters are
 * used as follows:
 *	type		the type of the filesystem; one of the OSI_MOUNT_TYPE_*
 *			constants
 *	flags		mount-related flags values; any combination of the
 *			OSI_MOUNT_FLAGS_* constants
 *	options		a string of comma-separated options, most likely
 *			specified as options to the mount command.  The
 *			caller does not need to include common options in
 *			this string.  The common options are those that can
 *			be inferred from the `flags' parameter.  For
 *			example, if OSI_MOUNT_FLAGS_READONLY is set in
 *			`flags' it is not necessary to set options to "ro".
 *			This parameter can be null, if no uncommon options
 *			were specified.
 *	data		filesystem specific data; passed to the FS vfs mount
 *			subroutine
 *	dataSize	the size (in bytes) of the data pointed to by `data'
 */
extern int osi_mount(const char*	devName,
		     const char*	mp,
		     int		type,
		     int		flags,
		     const char*	options,
		     const void*	data,
		     unsigned		dataSize);

/*
 * The definition of osi_umount is the responsibility of the osi_mount_mach.h
 * file.  Some systems may require a full-blown function, while others may
 * be able to get by with a macro definition.
 */

#endif	/* TRANSARC_OSI_MOUNT_H */
