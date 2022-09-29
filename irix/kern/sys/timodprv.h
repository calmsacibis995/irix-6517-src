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
/* $Header: /proj/irix6.5.7m/isms/irix/kern/sys/RCS/timodprv.h,v 1.4 1994/11/01 23:12:04 sfc Exp $ */
#ifndef	_SYS_TIMODPRV_
#define	_SYS_TIMODPRV_	1

#ifdef __cplusplus
extern "C" {
#endif

struct tim_ptr {
        struct tim_tim *next;
        struct tim_tim *prev;
};

struct tim_tim {
	long 	 tim_flags;
	queue_t	*tim_rdq;
	mblk_t  *tim_iocsave;
	int	 tim_mymaxlen;
	int	 tim_mylen;
	caddr_t	 tim_myname;
	int	 tim_peermaxlen;
	int	 tim_peerlen;
	caddr_t	 tim_peername;
	mblk_t	*tim_consave;
	struct   tim_ptr inuse;
};

struct tim_tim *tim_tim;
int tim_cnt;

#define TIMOD_ID	3

#ifdef __cplusplus
}
#endif

#endif	/* _SYS_TIMODPRV_ */
