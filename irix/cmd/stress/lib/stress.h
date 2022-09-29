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

#ident "$Revision: 1.3 $"
extern int exitonerror;
extern int errprintf(int, char *, ...);
extern char *errinit(char *);
extern void wrout(char *, ...);
extern char *Errcmd;

#define ERR_EXIT	0
#define ERR_ERRNO_EXIT	1
#define ERR_RET		2
#define ERR_ERRNO_RET	3

extern void parentexinit(int);
extern void slaveexinit(void);

#define EXONANY		0x1	/* on any member dying rather than just on
				 * parent dying
				 */
