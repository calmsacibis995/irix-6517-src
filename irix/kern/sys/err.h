#ifndef __SYS_ERR_H__
#define __SYS_ERR_H__

/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*#ident	"@(#)kern-port:sys/err.h	10.1"*/

#ifdef __cplusplus
extern "C" {
#endif

#ident	"$Revision: 3.3 $"
/*
 * structure of the err buffer area
 */
#define	NESLOT	20
#define	E_LOG	01
#define	E_SLP	02

struct err {
	int		e_nslot;		/* number of errslots */
	int		e_flag;			/* state flags */
	struct errhdr	**e_org;		/* origin of buffer pool */
	struct errhdr	**e_nxt;		/* next slot to allocate */
	struct errslot {
		int	slot[8];
	} e_slot[NESLOT];			/* storage area */
	struct map	e_map[(NESLOT+3)/2];	/* free space in map */
	struct errhdr	*e_ptrs[NESLOT];	/* pointer to logged errors */
};

extern struct err err;

struct errhdr	*geteslot();
struct errhdr	*geterec();

#ifdef __cplusplus
}
#endif

#endif /* __SYS_ERR_H__ */
