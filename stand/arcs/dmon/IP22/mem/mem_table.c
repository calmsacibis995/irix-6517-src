#ident	"$Id: mem_table.c,v 1.1 1994/07/20 23:50:39 davidl Exp $"
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
	mem_march(),
	mem_mats(),
	mem_kh(),
	mem_aina(),
	sim0(),
	sim1(),
	sim2(),
	sim3();

struct test_tbl mem_testtable[] = {
        "Aina test", mem_aina, 0, UNCACHED, 0, 0,
	"KH test", mem_kh, 0, UNCACHED, 0, 0,
        "March test", mem_march, 0, UNCACHED, 0, 0,
        "Mats test", mem_mats, 0, UNCACHED, 0, 0,
	"Sim0 Test", sim0, 0, UNCACHED, 0,0,
	"Sim1 Test", sim1, 0, UNCACHED, 0,0,
	"Sim2 Test", sim2, 0, UNCACHED, 0,0,
	"Sim3 Test", sim3, 0, UNCACHED, 0,0,
 	0, 0, 0, 0, 0, 0,
};

/* end */
