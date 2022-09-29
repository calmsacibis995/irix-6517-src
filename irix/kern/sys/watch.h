/**************************************************************************
 *									  *
 * 		 Copyright (C) 1990-1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ifndef __SYS_WATCH_H__
#define __SYS_WATCH_H__
#ident "$Revision: 1.14 $"

/*
 * a watchpoint struct - per watchpoint
 */
typedef struct watch_s {
	caddr_t		w_vaddr;	/* start addr of watch point */
	ulong		w_length;	/* length in bytes */
	ulong		w_mode;		/* Read/Write/Execute */
	struct watch_s *w_next;		/* link */
} watch_t;
#define W_READ	0x1
#define W_WRITE 0x2
#define W_EXEC	0x4

/* struct per process that has watchpoints */
typedef struct pwatch_s {
	watch_t		*pw_list;
	uint		pw_flag;
	caddr_t		pw_skipaddr;
	caddr_t		pw_skipaddr2;
	caddr_t		pw_skippc;	/* pc at skipaddr */
	watch_t		pw_firstsys;	/* first watchpoint hit in system */
	ulong		pw_curmode;	/* info to /proc */
	caddr_t		pw_curaddr;	/* info to /proc */
	ulong		pw_cursize;	/* info to /proc */
} pwatch_t;

/* values for flag */
#define WSTEP	0x00000001	/* stepping over a watchpoint */
#define WINSYS	0x00000002	/* got watchpoint inside system space */
#define WSETSTEP 0x00000004	/* watchpoint code set the single step */
#define WINTSYS	0x00000008	/* interested system watchpoint */
#define WIGN	0x00000010	/* skip this one no matter what */


#ifdef _KERNEL

extern int	maxwatchpoints;	/* max watchpoints per process */

struct eframe_s;
struct uthread_s;

int getwatch(struct uthread_s *, int (*)(caddr_t, ulong, ulong, caddr_t *), caddr_t,
	     int *);
int deletewatch(struct uthread_s *, uvaddr_t);
int addwatch(struct uthread_s *, uvaddr_t, ulong, ulong);
void checkwatchstep(struct eframe_s *);
int stepoverwatch(void);
int clrsyswatch(void);
void cancelskipwatch(void);
int passwatch(struct uthread_s *, struct uthread_s *);
int intlb(struct uthread_s *, uvaddr_t, int);
#endif /* _KERNEL */

#endif /* __SYS_WATCH_H__ */
