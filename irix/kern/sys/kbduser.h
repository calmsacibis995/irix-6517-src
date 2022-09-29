/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef	_IO_KBD_KBDUSER_H	/* wrapper symbol for kernel use */
#define	_IO_KBD_KBDUSER_H	/* subject to change without notice */

/* #ident	"@(#)uts-3b2:io/kbd/kbduser.h	1.3"		*/

#ident "$Revision: 1.2 $"

/*	This file depends on "kbd.h" */

/*
 * one structure per active user per direction
 */
struct kbdusr {
	struct kbd_tab *u_current;/* current table */
	struct tablink *u_list;	/* list of attached tables */
	struct tablink *u_cp;	/* pointer to (top) tablink of current table */
	struct tablink *u_ocp;	/* for hot key mode 2, old current */
	struct queue *u_q;	/* mainly for timeout() */
	unsigned short u_state;	/* state bits */
	unsigned short u_time;	/* timer for this user */
	unsigned char u_str[KBDVL];	/* hot key verbose string header */
	unsigned char u_doing;	/* TRUE if "on" state (for up and/or down) */
	unsigned char u_hot;	/* hot key */
	unsigned char u_hkm;	/* hot key mode */
	unsigned char u_running;/* TRUE when srv proc running */
};

/*
 * one structure per open module.
 */
struct kbduu {
	struct kbdusr *iside;	/* input side structure */
	struct kbdusr *oside;	/* output side structure */
	struct kbd_tab *ltmp;	/* temp; for loading */
	struct kbd_tab *u_link;	/* user's private tables */
	unsigned char *lpos;	/* load position */
	int lsize;		/* remaining size to load */
	unsigned int lalsz;	/* size of alloc during load */
	int zuid;		/* effective uid of "push"-er */
	int zumem;		/* bytes of private space in use */
	unsigned char flag;	/* flag for loading */
	unsigned char u_use;	/* TRUE if in use */
};

/*
 * tablink structure is user entry point.  A user has a linked list of
 * these tables; one of them is current.
 */

struct tablink {
	struct kbd_tab *l_me;
	struct tablink *l_next;
	struct tablink *l_child;	/* for composites */
	unsigned char *l_ptr;	/* pointer for l_msg */
	time_t l_lbolt;		/* when it should time-out */
	toid_t l_tid;		/* timer id */
	unsigned short l_time;	/* timer value */
	unsigned short l_node;	/* current node (search) */
	unsigned short l_state;	/* state bits */
			/* partial match buffer, in case of failure */
	unsigned char l_msg[KBDIMAX];
};

#define l_alpid		l_child		/* reserved ALP id hook */

/*
 * Flags for de-referencing composite tables of "attached" and
 * "available" types.
 */

#define R_TC	1
#define R_AV	2

#endif	/* _IO_KBD_KBDUSER_H */
