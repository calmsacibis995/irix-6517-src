/**************************************************************************
 *                                                                        *
 *  Copyright (C) 1986-1996 Silicon Graphics, Inc.                        *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ident "$Revision: 1.82 $"
#ifndef __SN0_PRIVATE_H__
#define __SN0_PRIVATE_H__

/*
 * This file contains definitions that are private to the ml/SN0
 * directory.  They should not be used outside this directory.  We
 * reserve the right to change the implementation of functions and
 * variables here as well as any interfaces defined in this file.
 * Interfaces defined in sys/SN are considered by others to be
 * fair game.
 */

#include <sys/SN/agent.h>
#include <sys/SN/intr.h>
#include <sys/pda.h>
#include <sys/nodepda.h>
#include <sys/xtalk/xtalk.h>
#include <sys/xtalk/xtalk_private.h>

#include "../sn_private.h"

#if defined (HUB_II_IFDR_WAR)
extern int hub_dmatimeout_kick(cnodeid_t, int);
extern void hub_setup_kick(cnodeid_t);
extern void restore_bridges(cnodeid_t);
#endif

#if defined (HUB_II_IFDR_WAR)
extern void hub_init_ifdr_war(cnodeid_t);
extern int hub_ifdr_fix_value(cnodeid_t);
extern int hub_ifdr_check_value(cnodeid_t);
extern int hub_ifdr_war_freq;
#endif

#if	defined(HUB_ERR_STS_WAR)	
extern void hub_error_status_war(void);
#endif


/* huberror.c */
extern void hub_error_init(cnodeid_t);
extern void dump_error_spool(cpuid_t cpu, void (*pf)(char *, ...));
extern void hubni_error_handler(char *, int);
extern int check_ni_errors(void);

/* sn0drv.c */
extern int sn0drv_attach(dev_t vertex);

/* hubidbg.c */
extern void crbx(nasid_t nasid, void (*pf)(char *, ...));


/* bte.c */
void bte_lateinit(void);
void bte_wait_for_xfer_completion(void *);

/* SN0asm.s */
void bootstrap(void);

#endif /*__SN0_PRIVATE_H__ */

