/**************************************************************************
 *									  *
 * 		 Copyright (C) 1992-1995 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ifndef _RT_H_
#define _RT_H_


extern kthread_t	*rt_gq;
extern kthread_t	**bindings;
extern int		rt_dither;


void 		init_rt(void);
int 		rt_remove_q(kthread_t *);
void		reset_pri(kthread_t *);
void		rt_restrict(cpuid_t);
void		rt_unrestrict(cpuid_t);
void		rt_rebind(kthread_t *);

void            start_rt(kthread_t *, short);
void            end_rt(kthread_t *);
void		redo_rt(kthread_t *, short);

#ifdef MP
void		*rt_pin_thread(void);
void		rt_unpin_thread(void *);
kthread_t	*rt_thread_select(void);
cpuid_t		rt_add_gq(kthread_t *);
void		rt_setpri(short pri);
#else
#define		rt_pin_thread()		0
#define		rt_unpin_thread(x)
#define		rt_thread_select()	0
#endif

#define		RT_NO_DSPTCH	0x2000
#define		RT_NO_DSPTCH_NOT 0xdfff
#define		RT_Q		0x4000
#define		RT_CPU		0xbfff

#endif /* _RT_H_ */
