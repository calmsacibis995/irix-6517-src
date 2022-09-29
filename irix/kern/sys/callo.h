/**************************************************************************
 *									  *
 * 		 Copyright (C) 1989-1992 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

#ifndef _SYS_CALLO_H
#define	_SYS_CALLO_H
#ident	"$Revision: 3.24 $"

/*
 * The callout structure is for a routine arranging
 * to be called by the clock interrupt
 * (clock.c) with a specified argument,
 * in a specified amount of time.
 * Used, for example, to time tab delays on typewriters.
 */
union c_tid {
	toid_t c_eid;	/* entire identifier */
	struct {
#ifdef SN0XXL
		unsigned ci_cpuid : 9;	/* list entry is on -- max 512 cpus */
		unsigned ci_fast : 1;	/* on fast callout list? */
		unsigned ci_cid  : 22;	/* ID for removal */
#else
		unsigned ci_cpuid : 7;	/* list entry is on -- max 128 cpus */
		unsigned ci_fast : 1;	/* on fast callout list? */
		unsigned ci_cid  : 24;	/* ID for removal */
#endif
	} c_pid; 
}; 

/*
 * Note: keep c_next, c_time, and c_tid at the front of this structure.
 *   we frequently use those fields during timeout/timein/untimeout calls
 *   and this alignment will keep them in the same (32byte) cache line.
 */
struct callout {
    	__int64_t	c_time;		/* incremental time */
	struct	callout *c_next;	/* link to next entry */
	union c_tid c_tid;
	void	(*c_func)();		/* routine */
	void	*c_arg;			/* argument to routine */
	void	*c_arg1;		/* argument to routine */
	void 	*c_arg2;		/* argument to routine */
	void	*c_arg3;		/* argument to routine */
	int	c_pl;			/* see k_pri in kthread struct */
	cpuid_t	c_ownercpu;		/* which cpu owns structure when free */
	short	c_flags;		/* extra info about this callout */
};

/* c_tid access macros */
#define c_id 		c_tid.c_eid
#define c_cid 		c_tid.c_pid.ci_cid
#define c_cpuid		c_tid.c_pid.ci_cpuid
#define c_fast		c_tid.c_pid.ci_fast

/* flag bits/macros */
#define C_FLAG_ITHRD	0x01		/* ithread callout */
#define C_FLAG_ISTK	0x02		/* istack callout */

#define C_IS_ITHRD(x)	(x & C_FLAG_ITHRD)
#define C_IS_ISTK(x)	(x & C_FLAG_ISTK)

/*
 * We support 4 callout types: norm istk/ithrd and (on non-EVEREST
 * platforms) fast istk/ithrd.
 *
 * We'll keep C_NORM/C_FAST around for compatibility.
 */
#define C_NORM		0x00	/* normal (ithrd) callout list */
#define C_FAST		0x01	/* fast (ithrd) callout list */
#define C_CLOCK 	0x02	/* (ithrd) timeouts with absolute time */
#define C_NORM_ISTK	0x03	/* normal (istk) callout list */
#define C_FAST_ISTK	0x04	/* fast (istk) callout list */
#define C_CLOCK_ISTK	0x05	/* (istack) timeouts with absolute time */

extern void callout_free(struct callout *co);
extern int callout_get_pri(void);

#endif /* _SYS_CALLO_H */
