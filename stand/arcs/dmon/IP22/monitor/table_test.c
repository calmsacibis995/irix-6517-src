#ident	"$Id: table_test.c,v 1.1 1994/07/21 00:19:27 davidl Exp $"
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


#include "sys/types.h"
#include "monitor.h"

/*
 * test table structure: (defined in monitor.h)
 *
 * struct test_tbl {
 *	char		*test_title;
 *	int		(*testptr)();
 *	unsigned	code_size;	** in bytes (required for K0, Ku) **
 *	unsigned	icached;	** run icached: UNCACHED or CACHED **
 *      unsigned        bev_off;        ** set to turn off BEV & use K0 vectors **
 *	unsigned	parm1;
 * };
 */

extern	cmd_settable();

extern	struct test_tbl cache_testtable[];
extern	struct test_tbl mem_testtable[];
/*extern	struct test_tbl io_testtable[];*/

/*
 * Top level test table (main test table)
 */
struct test_tbl test_table[] = {
        "CACHE Test", cmd_settable, 0, UNCACHED, 0, (unsigned)cache_testtable, 
        "MEM Test", cmd_settable, 0, UNCACHED, 0, (unsigned)mem_testtable, 
        /*"IO Test", cmd_settable, 0, UNCACHED, 0, (unsigned)io_testtable, */
	0, 0, 0, 0, 0, 0 
};

char maintest_menu[] = "Main Test Menu";

/* end */
