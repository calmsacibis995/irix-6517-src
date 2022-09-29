/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/*
 * HISTORY
 * $Log: direntry.h,v $
 * Revision 65.3  1998/01/16 20:18:50  lmc
 * Added include for idlbase.h.
 *
 * Revision 65.1  1997/10/20  19:20:11  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.101.1  1996/10/02  17:52:58  damon
 * 	Newest DFS from Transarc
 * 	[1996/10/01  18:41:21  damon]
 *
 * Revision 1.1.96.2  1994/06/09  14:12:05  annie
 * 	fixed copyright in src/file
 * 	[1994/06/09  13:25:35  annie]
 * 
 * Revision 1.1.96.1  1994/02/04  20:21:28  devsrc
 * 	Merged from 1.0.3a to 1.1
 * 	[1994/02/04  15:14:25  devsrc]
 * 
 * Revision 1.1.94.1  1993/12/07  17:27:13  jaffe
 * 	1.0.3a update from Transarc
 * 	[1993/12/03  15:41:12  jaffe]
 * 
 * Revision 1.1.5.3  1993/01/19  16:06:07  cjd
 * 	embedded copyright notice
 * 	[1993/01/19  14:16:10  cjd]
 * 
 * Revision 1.1.5.2  1993/01/13  18:09:11  shl
 * 	Transarc delta: cfe-ot6054-genl-and-i18n-spares 1.1
 * 	  Selected comments:
 * 	    Add spares to several RPC structures, with two goals:
 * 	    - general future usage
 * 	    - allow for at least some internationalization designs
 * 	    The only tricky part of this work is that the on-the-wire representation for
 * 	    directory contents is expanded.
 * 	[1993/01/12  21:22:15  shl]
 * 
 * Revision 1.1.3.3  1992/01/24  03:45:50  devsrc
 * 	Merging dfs6.3
 * 	[1992/01/24  00:14:04  devsrc]
 * 
 * Revision 1.1.3.2  1992/01/23  19:50:32  zeliff
 * 	Moving files onto the branch for dfs6.3 merge
 * 	[1992/01/23  18:34:55  zeliff]
 * 	Revision 1.1.1.3  1992/01/23  22:17:45  devsrc
 * 	Fixed logs
 * 
 * $EndLog$
*/
#ifndef	__DIRENTRYH__
#define	__DIRENTRYH__
#ifdef SGIMIPS
#include <dce/idlbase.h>
#endif

#if defined(DIRBLKSIZ) && ! defined (AFS_HPUX_ENV)
#define	DIR_BLKSIZE	DIRBLKSIZ
#else
#define	DIR_BLKSIZE	512
#endif

#ifdef	MAXNAMLEN
#define	DIR_MAXNAMLEN	MAXNAMLEN
#else
#define	DIR_MAXNAMLEN	255
#endif

/*
 * struct dir_minentry
 * struct dir_minxentry
 *
 * The dir_minentry structure is similar to the vendor-provided dirent or
 * direct structure, except that it doesn't have the entry's name.  This is
 * handy for use by the CM in implementing VOP_READDIR.
 *
 * The PX, in calling VOPX_READDIR, needs to get a format in which every
 * entry's offset is provided.  Some vendor-defined structures do not provide
 * this (e.g. OSF/1, HP/UX), and so we have had to define our own structure.
 * For this purpose we defined dir_minxentry.  This is identical to
 * dir_minentry but an extra offset field is added at the end.  This
 * particular similarity simplifies the programming of Episode's VOPX_READDIR.
 */

/*
 * Old (4.2 directory format) style entry (OSF/1)
 */
#ifdef AFS_OSF1_ENV
struct dir_minentry {
  unsigned long  inode;
  unsigned short recordlen;
  unsigned short namelen;
};
struct dir_minxentry {
  unsigned long  inode;
  unsigned short recordlen;
  unsigned short namelen;
  unsigned long  offset;
};
#define SIZEOF_DIR_MINENTRY		8
#define SIZEOF_DIR_MINXENTRY		12
#endif /* AFS_OSF1_ENV */

/*
 * New (SunOS 4.x directory format) style entry
 */
#ifdef AFS_SUNOS5_ENV
struct dir_minentry {
  unsigned long inode;
  unsigned long offset;
  unsigned short recordlen;
};
#define dir_minxentry dir_minentry
#define SIZEOF_DIR_MINENTRY		10
#define SIZEOF_DIR_MINXENTRY		SIZEOF_DIR_MINENTRY
#endif /* AFS_SUNOS5_ENV */

#ifdef SGIMIPS
/*
 * XXX. This struct has different size for 32 & 64 bit values.
 * This is same  as dirent defined in sys/dir*.h. Why define again ????
 * Look at cm, px and xvnode code.
 * Also we assume that inode and offset returned by xfs can fit in 32 bits.
 * XFS uses upper 32 bits in offset for performance optimization info.
 */
struct dir_minentry {
  ino_t inode;
  off_t offset;
  unsigned short recordlen;
};

#define dir_minxentry dir_minentry

#define SIZEOF_DIR_MINENTRY	  \
		((unsigned long)(&(((struct dir_minentry *)0)->recordlen)) + sizeof(unsigned short))
#define SIZEOF_DIR_MINXENTRY	SIZEOF_DIR_MINENTRY
#endif /* SGIMIPS */

#ifdef AFS_AIX31_ENV
struct dir_minentry {
  unsigned long offset;
  unsigned long inode;
  unsigned short recordlen;
  unsigned short namelen;
};
#define dir_minxentry dir_minentry
#define SIZEOF_DIR_MINENTRY		12
#define SIZEOF_DIR_MINXENTRY		SIZEOF_DIR_MINENTRY
#endif /* AFS_AIX31_ENV */

#ifdef AFS_HPUX_ENV
struct dir_minentry {
  unsigned long inode;
  short recordlen;
  short namelen;
};
struct dir_minxentry {
  unsigned long inode;
  short recordlen;
  short namelen;
  unsigned long offset;
};
#define SIZEOF_DIR_MINENTRY		8
#define SIZEOF_DIR_MINXENTRY		12
#endif /* AFS_HPUX_ENV */

/*
 * Wire-format entry (includes i18n codeset tag)
 */
#ifndef SGIMIPS
struct dir_minwireentry {
  unsigned long offset;
  unsigned long inode;
  unsigned short recordlen;
  unsigned short namelen;
  unsigned long highOffset;
  unsigned long codesetTag;
};
#else
/*
 * This should be same in 32 bit and 64 bit modes.
 */
struct dir_minwireentry {
  idl_ulong_int  offset;
  idl_ulong_int  inode;
  idl_ushort_int  recordlen;
  idl_ushort_int  namelen;
  idl_ulong_int highOffset;
  idl_ulong_int codesetTag;
};
#endif /* !SGIMIPS */
#define SIZEOF_DIR_MINWIREENTRY		20

/*
 * Individual entry's fields
 */
#define	dir_offset	dir_minent.offset
#define	dir_inode      	dir_minent.inode
#define	dir_recordlen	dir_minent.recordlen
#define	dir_namelen	dir_minent.namelen
#define	dir_highOffset	dir_minent.highOffset
#define	dir_tag		dir_minent.codesetTag

/*
 * Macro to determine the size of an individual entry.  Takes pointer
 * to a *wire* entry, and computes the size of the corresponding structure
 * implied by the name (e.g. new dir entry for NDIRSIZE, etc.)
 */
#define NDIRSIZE(dp)  \
	(((SIZEOF_DIR_MINENTRY + ((dp)->namelen+1)) + 3) & ~3)

#define WDIRSIZE(dp)  \
	(((SIZEOF_DIR_MINWIREENTRY + ((dp)->namelen+1)) + 3) & ~3)

/* macro to give wire size of record, given pointer to local dir entry,
 * which may not have namelen field.
 */
#if defined(AFS_SUNOS5_ENV) || defined(AFS_IRIX_ENV)
    /* also for SGIMIPS .... */
#define WLDIRSIZE(dp) \
    (((SIZEOF_DIR_MINWIREENTRY + strlen(((char *)(dp))+SIZEOF_DIR_MINENTRY) + 1)\
      + 3) & ~3)
#else	/* not AFS_SUNOS5_ENV */
/* field names are the same, so do the easy thing */
#define WLDIRSIZE(dp)	WDIRSIZE(dp)
#endif /* not AFS_SUNOS5_ENV */

#endif	/* __DIRENTRYH__ */
