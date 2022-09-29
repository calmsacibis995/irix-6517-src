#ident	"$Id: table_cmd.c,v 1.1 1994/07/21 00:19:03 davidl Exp $"
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
 * command table structure (defined in monitor h)
 *
 * typedef struct cmd_tbl {
 *	char    	*cmdstr;     	** command string **
 *	unsigned	cmdlen;         ** min  number of char's required **
 *	unsigned	(*cmdptr)();    ** command handler **
 *	unsigned	cmd_icached;    ** run icached: UNCACHED or CACHED ** 
 *	unsigned	parm1;          ** parameter 1 **
 * }CMD_TBL;
 *
 */

extern	print_menu(),
	cmd_em(),
	cmd_ll(),
	cmd_el(),
	cmd_fa(),
	cmd_la(),
	cmd_run(),
	cmd_quit(),
	cmd_exit(),
	cmd_help(),
	cmd_dummy(),
	cmd_c0(),
	cmd_c0sr(),
	cmd_c0pid(),
	cmd_cm(),
	cmd_pz(),
	cmd_cache(),
	cmd_SwapCache(),
	cmd_KH(),
	cmd_sw(),
	cmd_swloop(),
	cmd_lw(),
	cmd_lwloop(),
	cmd_rw(),
	cmd_fill(),
	cmd_dump(),
	cmd_jmp(),
	cmd_sload(),
	cmd_jal(),
	cmd_date(),
	cmd_memconfig(),
	cmd_aina(),
	cmd_config(),
	cmd_find(),
	cmd_init(),
	cmd_walk(),
	cmd_rtag(),
	cmd_wtag(),
	cmd_scache1(),
	cmd_scache2();
/*
 * Command table is divided into pages  Char '\n' is used
 * to indicate the start of a page 
 */

struct cmd_tbl cmd_table[] = {
"\n",									0, cmd_dummy, UNCACHED, 0,
"              << TEST CONTROL COMMANDS >>\n",				0, cmd_dummy, UNCACHED, 0,
"em [n]        Set error-mode: 0-exit, 1-continue, 2-loop",             2, cmd_em, UNCACHED, 0,
"el [n]        Set error-limit (0 - ignore error): 0-255",              2, cmd_el, UNCACHED, 0,
"ll [n]        Set loop-limit (0 - loop forever)",                      2, cmd_ll, UNCACHED, 0,
"fa <addr>     Set first_address for memory tests",                     2, cmd_fa, UNCACHED, 0,
"la <addr>     Set last_address for memory tests",                      2, cmd_la, UNCACHED, 0,
"run           Run all tests",                                          3, cmd_run, UNCACHED, 0,
"quit          Quit current test menu",                                 1, cmd_quit, UNCACHED, 0,
"exit          Exit diagnostics monitor",				4, cmd_exit, UNCACHED, 0,
"help [cmd]    Display help on a command or all commands",		1, cmd_help, UNCACHED, 0,
"?    [cmd]    Display help on a command or all commands",		1, cmd_help, UNCACHED, 0,
"menu          Show monitor menu",					1, print_menu, UNCACHED, 0,
"date [-s]     Display or set date in nvram/todc chip", 		4, cmd_date, UNCACHED, 0,
/*
"sload [-b]    Serial load s3rec data from port B (-b == Binary)", 	5, cmd_sload, UNCACHED, 0,
*/
"hwconfig      Show hardware configuration",				6, cmd_config,  UNCACHED, 0,
"c0            Show C0 registers", 		2, cmd_c0,  UNCACHED, 0,
"c0sr <n>      Set C0_STATUS register", 	4, cmd_c0sr, UNCACHED, 0,
"jal <addr>    Jump and Link (jal)",		3, cmd_jal, UNCACHED, 0,
"go <addr>     Jump to an address", 		2, cmd_jmp, UNCACHED, 0,
"\n",						0, cmd_dummy, UNCACHED, 0,
"              << DIAGMON COMMANDS >>\n",				0, cmd_dummy, UNCACHED, 0,
"init [-c|-m]        			Init caches or memory",		4, cmd_init, UNCACHED, 0,
"rtag [-d|i|s] <addr> [lines] [lpcnt] -q   Read primary/secondary cache tag", 4, cmd_rtag, UNCACHED, 0,
"wtag [-d|i|s] <addr> <data> [lines] [lpcnt]   Write data to primary/secondary cache tag", 4, cmd_wtag, UNCACHED, 0,
"r [-w|h|b] <addr> [lpcnt] [-q]    	Read  word, halfword or byte", 	1, cmd_lw, UNCACHED, 0,
"rl [-w|h|b] <addr>                	Read  word, halfword or byte scope loop", 2, cmd_lwloop, UNCACHED, 0,
"w [-w|h|b] <addr> <data> [lpcnt]  	Write word, halfword or byte",	1, cmd_sw, UNCACHED, 0,
"wl [-w|h|b] <addr> <data>         Write word, halfword or byte scope loop", 2, cmd_swloop, UNCACHED, 0,
"wr <addr> <data> [lpcnt] [-q]     Write then read word", 		2, cmd_rw, UNCACHED, 0,
"fill [-w|h|b] <addr> <data> [cnt]   Fill memory or cache locations", 4, cmd_fill, UNCACHED, 0,
"dump [-w|h|b] <addr> [cnt]        	Dump memory or cache locations", 4, cmd_dump, UNCACHED, 0,
"find <data> [<start> <end>]       	Find data in memory", 4, cmd_find, CACHED, 0,
"walk0 -[whb] <addr> [-m mask] [lpcnt]  Walk bit 0 pattern", 5, cmd_walk, UNCACHED, 1,
"walk1 -[whb] <addr> [-m mask] [lpcnt]  Walk bit 1 pattern", 5, cmd_walk, UNCACHED, 0,
"aina  <start> <end> [lpcnt]       	Addr-In-Addr Memory/Cache Test", 4, cmd_aina, UNCACHED, UNCACHED,
"ainac <start> <end> [lpcnt]       	AINA Memory/Cache Test (cache i-fetch)", 5, cmd_aina, CACHED, CACHED,
"kh  <start addr> <end> [lpcnt]    	KH Memory/Cache Test", 		2, cmd_KH, UNCACHED, UNCACHED,
"khc <start addr> <end> [lpcnt]    	KH Memory/Cache Test (cache i-fetch)", 3, cmd_KH, CACHED, CACHED,
"\n",						0, cmd_dummy, UNCACHED, 0,
"scache1        			Loops on a write to secondary cache", 7, cmd_scache1, UNCACHED, 0,
"scache2        			Loops on a write/read to secondary cache", 7, cmd_scache2,UNCACHED, 0, 
0, 0, 0, UNCACHED, 0,
};

/*
 * Commands for debugging the nvram in case nvram initialization failed.
 */

struct cmd_tbl nv_cmd_table[] = {
"\n",									0, cmd_dummy, UNCACHED, 0,
"              << NVRAM DEBUG COMMANDS >>\n",				0, cmd_dummy, UNCACHED, 0,
"r <addr> <lpcnt>		Read nvram byte", 	1, cmd_dummy, UNCACHED, 0,
"w <addr> <data> <lpcnt>   	Write nvram byte",	1, cmd_dummy, UNCACHED, 0,
"f <addr> <data> <bytecnt>	fill nvram bytes",	1, cmd_dummy, UNCACHED, 0,
"d <addr> <bytecnt>		dump nvram bytes",	1, cmd_dummy, UNCACHED, 0,
"a <faddr> <laddr>		Addr in Addr",		1, cmd_dummy, UNCACHED, 0,
"exit                     	exit nvram debug",	1, cmd_dummy, UNCACHED, 0,
0, 0, 0, UNCACHED, 0,
};

/* end */
