/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1993 Silicon Graphics, Inc.                *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/*
** Copyright (c) 1985, 1991
** by Xinet Inc.  All Rights Reserved.
** This program contains proprietary confidential information
** and trade secrets of Xinet Inc.
** Reverse engineering of object code is prohibited.  Use of
** copyright notice is precautionary and does not imply publication.
** @(#)afp.h    7.0     92/12/09        Xinet
*/


/*
 * This file is derived from the afp.h file authored by Xinet Inc.  This
 * file is a subset of the afp.h file and some type names have been changed.
 */

#ifndef __ANCILLARY_H__
#define __ANCILLARY_H__

#ident "$Revision: 1.2 $"

/*
 * Ancillary record definitions.
 */
typedef struct accrts{
  union{
    struct{
      u_char	aru_user;
      u_char	aru_world;
      u_char	aru_group;
      u_char	aru_owner;
    }aru_l;
    u_int	aru_long;
  } ar_u;
  u_char	ar_ancest;
  u_char	accpad2;
  u_char	accpad3;
  u_char	accpad4;
} accrts_t;

#define MAXCMTSIZ      199
#define CNNBUFLEN	32
#define SNBUFLEN	13
#define AI_VERSION    8139


/*
 * finder information stored with each file or dir
 */
typedef struct fInfo
{
	u_int		fi_type;	/* four chars (OSType) */
	u_int		fi_creator;	/* four chars (OSType) */
	short		fi_flags;
	short		fi_location[2];
	short		fi_fldr;	/* file's window */
	u_char		fi_dummy[16];
} fInfo_t;

/*
 * XiNET defined ancillary file record.
 */
typedef struct ai{
  accrts_t	ai_inhAR;	/* Access rights inhibit bits */
  fInfo_t	ai_finfo;	/* Finder info. */
  u_int	        ai_createDate;	/* File creation date */
  u_int		ai_backupDate;	/* File backup date */
  u_int		ai_version;	/*  */
  u_short	ai_attr;	/* Attributes */
  u_char	ai_comment[MAXCMTSIZ+1]; /* Comment [0]==length */
  char 		ai_lname[CNNBUFLEN]; /* Long name */
  char		ai_sname[SNBUFLEN]; /* Short name */
} ai_t;

#define AI_SIZE  300

/* attributes */

/*
 * File/Directory Attributes flags
 */
#define afpFDAInvisible		(1<<0)
#define afpFDAMultiUser		(1<<1)		/* file only */
#define afpFDAIsExpFolder	(1<<1)		/* dir only */
#define afpFDASystem		(1<<2)	
#define afpFDADAlreadyOpen	(1<<3)		/* file only */
#define afpFDAMounted	        (1<<3)		/* dir only */
#define afpFDARAlreadyOpen	(1<<4)		/* file only */
#define afpFDAInExpFolder	(1<<4)		/* dir only */
#define afpFDAReadOnly		(1<<5)		/* file only */
#define afpFDAWriteInhibit	(1<<5)		/* file only */
#define afpFDABackupNeeded	(1<<6)		
#define afpFDARenameInhibit	(1<<7)		
#define afpFDADeleteInhibit	(1<<8)		
#define afpFDAHasCustomIcon     (1<<9)          
#define afpFDACopyProtect	(1<<10)		/* file only */
#define afpFDASet		(1<<15)

/* fdFldr field masks */

#define fHasBeenInited (1<<8)
#define fHasBundle (1<<13)
#define fInvisible (1<<14)

/*
 * standard AFP access rights bits (non-Apple names)
 */
#define afpARSearch	0x01
#define afpARRead	0x02
#define afpARWrite	0x04
#define afpARBlankAccess 0x10 /* ar_user only */
#define afpAROwner	0x80	/* ar_user only */
/*
 * non-standard definitions to track "ancestor" permissions (ar_ancest only)
 */
#define afpARAncestS	0x01	/* S^A - search granted by all ancestors */
#define afpARAncestSW	0x02	/* W^A - search or write granted by all anc's */

#define afpAMRead	(1<<0)
#define afpAMWrite	(1<<1)
#define afpAMDenyRead	(1<<4)
#define afpAMDenyWrite	(1<<5)
#define afpOFRsrcFlag	(1<<7)

#endif
