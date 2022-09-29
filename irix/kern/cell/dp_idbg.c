/**************************************************************************
 *									  *
 * 		 Copyright (C) 1989-1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ident "$Id: dp_idbg.c,v 1.67 1997/08/27 21:38:36 sp Exp $"

#include <sgidefs.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/idbgentry.h>
#include <sys/map.h>
#include <sys/pda.h>
#include <sys/ksa.h>
#include <sys/runq.h>
#include <ksys/cell.h>
#include "tk_private.h"
#include <ksys/cell/relocation.h>
#include <ksys/vhost.h>
#include <ksys/vproc.h>
#include "os/proc/vproc_private.h"
#include <ksys/pid.h>
#include <ksys/childlist.h>
#include "os/proc/vpgrp_private.h"

/*
 * idbg modules for dp
 */
static void idbg_tktrace(__psint_t n, __psint_t a2, int argc, char **argv);
static void idbg_tktrace_cell(__psint_t);
static void idbg_tktrace_tag(__psint_t);
static void idbg_tktrace_id(__psint_t);
static void idbg_tkc_print(__psint_t);
static void idbg_tks_print(__psint_t);

#if DEBUG
extern void idbg_vproc_trace(__psint_t);
extern void idbg_mesgtrace(__psint_t n, __psint_t a2, int argc, char **argv);
extern void idbg_mesgmask(__psint_t n, __psint_t a2, int argc, char **argv);
extern void idbg_bag(__psint_t);
extern void idbg_mesgstats(__psint_t n, __psint_t a2, int argc, char **argv);
#endif
extern void idbg_mesgstats(__psint_t n, __psint_t a2, int argc, char **argv);
extern void idbg_mesglist(__psint_t n, __psint_t a2, int argc, char **argv);
#ifdef BLALOG
static void idbg_bla_print_log(__psint_t);
extern void bla_print_log(void);
#endif

int dp_idbg_init_done = 0;

void
dp_idbg_init(void)
{
	if (dp_idbg_init_done)
		return;

	dp_idbg_init_done = 1;

	idbg_addfunc("tktrace", idbg_tktrace);
	idbg_addfunc("tktrace_id", idbg_tktrace_id);
	idbg_addfunc("tktrace_cell", idbg_tktrace_cell);
	idbg_addfunc("tktrace_tag", idbg_tktrace_tag);
	idbg_addfunc("tkcprint", idbg_tkc_print);
	idbg_addfunc("tksprint", idbg_tks_print);
#if DEBUG
	idbg_addfunc("bag", idbg_bag);
	idbg_addfunc("mesgtrace", idbg_mesgtrace);
	idbg_addfunc("mesgmask", idbg_mesgmask);
#endif
	idbg_addfunc("mesgstats", idbg_mesgstats);
	idbg_addfunc("mesglist", idbg_mesglist);
#ifdef BLALOG
	idbg_addfunc("blalog", idbg_bla_print_log);
#endif
}

/*
 * LOCAL functions
 */
/* ARGSUSED */
static void
idbg_tktrace(__psint_t n, __psint_t a2, int argc, char **argv)
{
	int num = -1;
	int type = TK_LOG_ALL;
	char *pat = NULL;

	if (argc && *argv[0] == '?') {
		qprintf("Usage:tktrace n [type [pattern]]\n");
		qprintf("\tn - # of records to print\n");
		qprintf("\ttype - 0 - minimal; 0xf - lots\n");
		return;
	}

	switch (argc) {
	case 3:
		pat = argv[2];
	case 2:
		type = (int)atoi(argv[1]);
	case 1:
		num = (int)atoi(argv[0]);
	}
	tk_printlog(num, type, pat);
}

static void
idbg_tktrace_id(__psint_t n)
{
	char s[32];
	sprintf(s, "p&%d;", n);
	tk_printlog(-1, TK_LOG_ALL, s);
}

static void
idbg_tktrace_tag(__psint_t n)
{
	char s[32];
	sprintf(s, "t&%d;", n);
	tk_printlog(-1, TK_LOG_ALL, s);
}

static void
idbg_tktrace_cell(__psint_t n)
{
	char s[32];
	sprintf(s, "h&%d;", n);
	tk_printlog(-1, TK_LOG_ALL, s);
}

static void
idbg_tkc_print(__psint_t n)
{
	tkc_print((tkc_state_t *)n, NULL);
}

static void
idbg_tks_print(__psint_t n)
{
	tks_print((tks_state_t *)n, NULL);
}

#ifdef BLALOG
/* ARGSUSED */
static void
idbg_bla_print_log(__psint_t n)
{
	bla_print_log();
}
#endif /* BLALOG */
