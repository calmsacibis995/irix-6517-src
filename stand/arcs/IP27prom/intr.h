/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1992-1996, Silicon Graphics, Inc.          *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/*
 * File: prom_error.h
 */

#ifndef	_INTR_H
#define	_INTR_H

extern void	intr_disable(void);
extern int	intr_process(void);
extern int	intr_default_hndlr(int);
extern int	intr_pi_err_hndlr(int);
extern int	intr_clear_md_error(int);
extern int	intr_ignore_action(int);
extern int	intr_fatal_action(int);

extern void	enable_cpu_intrs(void);
extern void	disable_cpu_intrs(void);

#define INTR_FATAL	2
#define INTR_IGNORE	1

#endif /* _INTR_H */
