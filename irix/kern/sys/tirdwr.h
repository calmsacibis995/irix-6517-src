/*
 * |-----------------------------------------------------------|
 * | Copyright (c) 1991, 1990 MIPS Computer Systems, Inc.      |
 * | All Rights Reserved                                       |
 * |-----------------------------------------------------------|
 * |          Restricted Rights Legend                         |
 * | Use, duplication, or disclosure by the Government is      |
 * | subject to restrictions as set forth in                   |
 * | subparagraph (c)(1)(ii) of the Rights in Technical        |
 * | Data and Computer Software Clause of DFARS 252.227-7013.  |
 * |         MIPS Computer Systems, Inc.                       |
 * |         950 DeGuigne Avenue                               |
 * |         Sunnyvale, California 94088-3650, USA             |
 * |-----------------------------------------------------------|
 */
/* $Header: /proj/irix6.5.7m/isms/irix/kern/sys/RCS/tirdwr.h,v 1.5 1994/11/01 23:12:06 sfc Exp $ */

#ifndef	_SYS_TIRDWR_
#define	_SYS_TIRDWR_	1

#ifdef __cplusplus
extern "C" {
#endif

struct trw_trw {
	long 	 trw_flags;
	queue_t	*trw_rdq;
	mblk_t  *trw_mp;
};

#ifndef _KERNEL_MASTER
#define USED 		001
#define ORDREL 		002
#define DISCON  	004
#define FATAL		010
#define WAITACK 	020

#define TIRDWRPRI 	PZERO+3
#endif /* _KERNEL_MASTER */

extern struct trw_trw trw_trw[];
extern int trw_cnt;

#define TIRDWR_ID	4

#ifdef __cplusplus
}
#endif

#endif	/* _SYS_TIRDWR_ */
