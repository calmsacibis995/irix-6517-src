/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1996, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ifndef __IO6PROM_PRIVATE__
#define __IO6PROM_PRIVATE__

#ident	"IO6prom/io6prom_private.h : $Revision: 1.11 $"

#if _LANGUAGE_C
extern void 	EnterInteractiveMode(void);
extern void 	sysctlr_message(char*);
extern int 	set_diagval_disable(int, int);
extern int 	_prom;
extern int 	verbose_faults;
extern int 	doass;
extern int 	stdio_init;
extern void 	hubuart_init(void);
extern void 	init_local_dir(void);
extern int 	rstat_cmd(int, char **, char **, struct cmd_table *);
extern void 	check_inventory(void);

#if defined(SABLE) && defined(FAKE_PROM)
extern void 	init_other_local_dir(void);
#endif
#endif /* _LANGUAGE_C */
#define	IO6PROM_SR	(NORMAL_SR | SR_BEV)
#endif /* __IO6PROM_PRIVATE__ */

