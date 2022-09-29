#ident	"$Id: cache_table.c,v 1.1 1994/07/21 01:24:15 davidl Exp $"
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



#include "pdiag.h"
#include "monitor.h"

/*
 * test table structure: (defined in monitor.h)
 *
 * struct test_tbl {
 *	char		*test_title;
 *	int		(*testptr)();
 *	unsigned	code_size;	** Used by cached execution	** 
 *	unsigned	icached;	** UNCACHED or CACHED 		**
 *      unsigned        bev_off;        ** set to use K0 vectors 	**
 *	unsigned	parm1;
 */

extern int	
	Dcache_aina(),
	Dcache_kh(),
	Dcache_march(),
	Dcache_mats(),
	Dtag_mats(),
	Dkseg01_func(),
	Dtag_parity(),
	Icache_mats(),
	Itag_mats(),
	Itag_parity();

struct test_tbl cache_testtable[] = {
        "Dtag Mats", Dtag_mats, 0, UNCACHED, 0, 0,
        "Dtag Parity test", Dtag_parity, 0, UNCACHED, 0, 0,
        "Dcache Aina", Dcache_aina, 0, UNCACHED, 0, 0,
        "Dcache KH", Dcache_kh, 0, UNCACHED, 0, 0,
        "Dcache March", Dcache_march, 0, UNCACHED, 0, 0,
        "Dcache Mats", Dcache_mats, 0, UNCACHED, 0, 0,
        "Dkseg01 func test", Dkseg01_func, 0, UNCACHED, 0, 0,
        "Itag Mats", Itag_mats, 0, UNCACHED, 0, 0,
        "Itag Parity test", Itag_parity, 0, UNCACHED, 0, 0,
        "Icache Mats", Icache_mats, 0, UNCACHED, 0, 0,
 	0, 0, 0, 0, 0, 0,
};

/* end */
