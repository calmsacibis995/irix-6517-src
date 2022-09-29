/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/*
 * HISTORY
 * $Log: astab.h,v $
 * Revision 65.3  1999/07/21 19:01:14  gwehrman
 * Defined SGIMIPS for this development header.  Fix for bug 657352.
 *
 * Revision 65.2  1997/12/16 17:05:50  lmc
 *  1.2.2 initial merge of file directory
 *
 * Revision 65.1  1997/10/20  19:20:32  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.720.1  1996/10/02  21:11:21  damon
 * 	Newest DFS from Transarc
 * 	[1996/10/01  18:51:26  damon]
 *
 * $EndLog$
 */
/* Copyright (C) 1996, 1990 Transarc Corporation -- All Rights Reserved */

/* $Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/xaggr/RCS/astab.h,v 65.3 1999/07/21 19:01:14 gwehrman Exp $ */

/*
 *		Aggregate table entry format
 */

#ifndef	_ASTABH_
#define	_ASTABH_
#ifndef SGIMIPS
#define SGIMIPS
#endif /* !SGIMIPS */

#define	ASTAB_INFIX	"/var/dfs/"
#define	ASTAB_SFX		"dfstab"

extern char *AstabFileName;

/* Sizes of strings */

#define	ASTABSIZE_SPEC	512
#define	ASTABSIZE_DIR	512
#define	ASTABSIZE_TYPE	64

/* Entry structure */

struct astab {
    char    as_spec[ASTABSIZE_SPEC];	/* block special device name */
    char    as_aggrName[ASTABSIZE_DIR];	/* Aggregate name */
    char    as_type[ASTABSIZE_TYPE];   	/* type of physical file system */
#ifdef SGIMIPS
    __uint32_t	as_aggrId;
#else
    u_long  as_aggrId;   		/* Aggregate Id */
#endif
};

/* File system extra data:  UFS version */

struct ufs_astab {
    afs_hyper_t uas_volId;		/* 1st & only volume */
    char uas_mountedon[ASTABSIZE_DIR];	/* name of mounted-on dir */
};

/* Values for as_type */

#define	ASTABTYPE_UFS	"ufs"
#define	ASTABTYPE_EPI	"lfs"
#define	ASTABTYPE_AIX3	"aix3"
#define	ASTABTYPE_VXFS	"vxfs"
#define	ASTABTYPE_DMEPI	"dmepi"

/*
 * Macros to make addition of new file system types in the future
 * easier.
 */
#define ASTABTYPE_SUPPORTS_EFS(atype) ((strncmp(atype, ASTABTYPE_EPI,  ASTABSIZE_TYPE) == 0) || \
				       (strncmp(atype, ASTABTYPE_VXFS, ASTABSIZE_TYPE) == 0))

#define ASTABTYPE_TO_AGTYPE(astype, agtype)  \
    if (!strncmp(astype, ASTABTYPE_UFS, ASTABSIZE_TYPE)) \
         agtype = AG_TYPE_UFS; \
    else if (!strncmp(astype, ASTABTYPE_EPI, ASTABSIZE_TYPE)) \
         agtype = AG_TYPE_EPI; \
    else if (!strncmp(astype, ASTABTYPE_VXFS, ASTABSIZE_TYPE)) \
         agtype = AG_TYPE_VXFS; \
    else if (!strncmp(astype, ASTABTYPE_DMEPI, ASTABSIZE_TYPE)) \
         agtype = AG_TYPE_DMEPI; \
    else \
         agtype = AG_TYPE_UNKNOWN

#endif	/* _ASTABH_ */
