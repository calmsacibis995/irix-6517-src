#ifndef __CKPT_SYS_H__
#define __CKPT_SYS_H__
/*
 * ckpt_sys.h
 *
 *	Checkpoint/restart header file.  Contains defs for use with
 *	checkpoint syssgi system call.
 *
 * Copyright 1995, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 *
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 *
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 *
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 */

#ident "$Revision: 1.20 $"


#if defined(_LANGUAGE_C) || defined(_LANGUAGE_C_PLUS_PLUS)

#define	CKPT_OPENDIR	0	/* open dir associated with fd	*/
#define	CKPT_EXECMAP	1	/* map in executible */
#define	CKPT_SETBRK	2	/* set brkbase and brksize */
#define	CKPT_PIDFORK	3	/* fork a process with given pid */
#define	CKPT_FPADDR	4	/* get address of file struct */
#define	CKPT_SVR3PIPE	5	/* create old svr3 pipe */
#define	CKPT_GETCPIDS	6	/* get pids of children */
#define	CKPT_UNMAPPRDA	7	/* remove prda */
#define	CKPT_GETSGPIDS	8	/* get share group pids */
#define	CKPT_FSETTIMES	9	/* set file times, using fd */
#define	CKPT_SETTIMES	10	/* set file times, using name */
#define	CKPT_SETEXEC	11	/* set exec vnode */
#define	CKPT_TESTSTUB	12	/* test if cpr module is linked in */
#define	CKPT_FDACCESS	14	/* fdaccess */
#define	CKPT_SOCKMATE	15	/* "id" of socketpair mate */
#define	CKPT_SETSUID	16	/* change suid of process */
#define	CKPT_PMOGETNEXT	17	/* get next pmo handle */
#define	CKPT_PM_CREATE	18	/* create a new policy module for proc */
#define	CKPT_MUNMAP_LOCAL 19	/* unmap *only* MAP_LOCAL mapping */

#ifdef CKPT_DEBUG
#define	CKPT_SLOWSYS	-1	/* slow syscall (debugging only) */
#define	CKPT_TARGET	-2	/* we are a restart target */
/*
 * flag defines
 */
#define	SLOWSYS_EINTR		0x1	/* return EINTR */
#define	SLOWSYS_SEMAWAIT	0x2	/* use semawait */
#endif


typedef struct ckpt_execargs {
	int fd;
	caddr_t vaddr;
	size_t len;
	size_t zfodlen;
	off_t off;
	int prot;
	int flags;
} ckpt_execargs_t;

#endif /* C || C++ */
#endif /* !__CKPT_SYS_H__ */

