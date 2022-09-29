#ifndef __SYS_STDNAMES_H__
#define	__SYS_STDNAMES_H__

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
#ident	"$Revision: 1.5 $"

/*
** Typically a standard device name has the following format:
**
**	AAA#{d#{s# | vh | vol}:
**
**	where,
**		AAA		is a three character device name.
**		#		is a single digit, 0 - 9.
**
** This include file should be included by drivers that want to make
** use of the "standard" device name format.  The driver must
** #define STD_NAME_DEFINED as a three (3) character string representing
** the device, e.g., #define STD_NAME_DEFINED	"xyl"
*/		


/*
** The following represent:
**	maximum per format item size in a "standardized name"
**	total characters in a "standardized name"
**	Macro to create a buffer that will contain a "standardized name"
*/
#define	STDN_MAXITEMSZ		3
#define	STDN_MAXNAMESZ		((3 * STDN_MAXITEMSZ) + 4 + 1)
#define	STDNBUF()	char stdnamebuf[ STDN_MAXNAMESZ ]

/*
** This macros is used by device drivers to create a standardized name
** from the device name, controller, unit and filesystem.
** To use this, a driver needs to define STD_NAME_DEFINED as the
** correct device name string, and use the macro STDNBUF()
** to create buffer to store the "standardized name" in.
*/
#define	STDNAME(a,b,c)	stdname(STD_NAME_DEFINED, stdnamebuf, (a), (b), (c)) 

/*
** Defines for possible arguments to stdname
*/
#define	STDN_NULL_UNIT	-1	/* No unit		*/
#define	STDN_NULL_FS	-2	/* No fs		*/
#define	STDN_VH_FS	-1	/* Volume header fs	*/

#define	STDN_NULL_FMT	""	/* no format		*/
#define	STDN_CTLR_FMT	"%d"	/* controller format	*/
#define	STDN_UNIT_FMT	"d%d"	/* unit format		*/
#define	STDN_FS_FMT	"s%d"	/* partition format	*/
#define	STDN_VH_FMT	"vh"	/* volume header format	*/
#define	STDN_VOL_FMT	"vol"	/* entire disk format	*/
#define	STDN_END_FMT	":"	/* end of format 	*/

#ifdef _KERNEL
#define	PRDEV_MAXBUFSZ	(160+1)	/* 2, 80 character lines + NULL	*/

extern char	*stdname(char *, char *, int, int, int);
#endif

#endif /* __SYS_STDNAMES_H__ */
