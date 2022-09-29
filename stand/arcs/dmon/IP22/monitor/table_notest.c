#ident	"$Id: table_notest.c,v 1.1 1994/07/21 00:19:19 davidl Exp $"
/**************************************************************************
 *									  *
 * 		 Copyright (C) 1993, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/


/*
 * This is a dummy test table used for building a diag prom
 * without any test modules, i.e. build the montior only.
 */

#include "sys/types.h"
#include "mips/cpu.h"
#include "monitor.h"

/*
 * test table structure: (defined in monitor.h)
 *
 * struct test_tbl {
 *	char		*test_title;
 *	int		(*testptr)();
 *	unsigned	code_size;	** in bytes (required for K0, Ku) **
 *	unsigned	icached;	** run icached: UNCACHED or CACHED **
 *      unsigned        bev_off;        ** set to disable BEV for K0 vectors **
 *	unsigned	parm1;
 * };
 */

/*
 * Top level test table (main test table)
 */
struct test_tbl test_table[] = {
	0, 0, 0, 0, 0, 0 
};

char maintest_menu[] = "No Test Module";

/* end */
