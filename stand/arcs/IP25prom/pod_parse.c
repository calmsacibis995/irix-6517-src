/*
 * File: 	pod_parse.c
 * Purpose:	Basic command parsing and command tables for pod.
 *
 * Copyright 1995, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 *
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 *
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 *
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 */

#include <sys/types.h>
#include <sys/sbd.h>
#include <sys/mips_addrspace.h>

#include "biendian.h"
#include "pod.h"
#include "prom_externs.h"
#include "ip25prom.h"
#include "pod_iadefs.h"
#include "pod_failure.h"
#include "niblet_debug.h"

int	load_io4prom(void);
int	dump_tag(int, __scunsigned_t);
int	pon_sload(int, struct flag_struct*);
int	pod_walk(__psunsigned_t, __psunsigned_t, int, int);
int	mem_test(__psunsigned_t, __psunsigned_t);
int	jump_addr(__psunsigned_t, uint, uint, struct flag_struct *);

__uint64_t _count(__uint64_t, int);

void	_register(int, int *, __scunsigned_t, struct reg_struct *);
void	decode_address(__scunsigned_t);
void	run_cached(void (*function)(int, int));
void	reset_system(void);
void	conf_register(int, uint, uint, __uint64_t, int);
void	dump_mc3(uint);
void	dump_io4(uint);
void	dump_evcfg(int *, int);
void	pod_bist(void);
void	podResume(void);
void	pod_reconf_mem(void);
void	tlb_dump(int *, int);
void	clear_ip25_state(void);
void	clear_mc3_state(void);
void	clear_io4_state(void);
void	zap_inventory(void);
void	dump_mpconf(uint);
void	show_page_size(void);
void	set_page_size(int);
int	testScacheFlipBits(__uint64_t, int);
void	margin(__uint64_t, int *, int *);


static void	parse_error(void);
static void	skipwhite(int **);
static void	copystr(int **, int **);
int 		lo_strcmp(int *, char *);

const struct cmd_table {
	int token;
	int args;
	char *name;
	unsigned char *help;
} commands[] = {
{WRITE_BYTE,	2,	"wb",	"Write byte:       wb ADDRESS NUMBER"},
{WRITE_HALF,	2,	"wh",	"Write halfword:   wh ADDRESS NUMBER"},
{WRITE_WORD,	2,	"ww",	"Write word:       ww ADDRESS NUMBER"},
{WRITE_DOUBLE,	2,	"wd",	"Write double:     wd ADDRESS NUMBER"},
{DISP_BYTE,	1,	"db",	"Display byte:     db ADDRESS"},
{DISP_HALF,	1,	"dh",	"Display halfword: dh ADDRESS"},
{DISP_WORD,	1,	"dw",	"Display word:     dw ADDRESS"},
{DISP_DOUBLE,	1,	"dd",	"Display double:   dd ADDRESS"},
{UWRITE_BYTE,	2,	"uwb",	"Uncached Write byte: uwb ADDRESS NUMBER"},
{UWRITE_HALF,	2,	"uwh",	"Uncached Write halfword: uwh ADDRESS NUMBER"},
{UWRITE_WORD,	2,	"uww",	"Uncached Write word:     uww ADDRESS NUMBER"},
{UWRITE_DOUBLE,	2,	"uwd",	"Uncached Write double:   uwd ADDRESS NUMBER"},
{UDISP_BYTE,	1,	"udb",	"Uncached Display byte:     udb ADDRESS"},
{UDISP_HALF,	1,	"udh",	"Uncached Display halfword: udh ADDRESS"},
{UDISP_WORD,	1,	"udw",	"Uncached Display word:     udw ADDRESS"},
{UDISP_DOUBLE,	1,	"udd",	"Uncached Display double:   udd ADDRESS"},
{DISP_REG,	1,	"dr",	"Display register: dr <sr || sp || all>"},
{WRITE_REG,	2,	"wr",	"Write register:   wr < sr >"},
{SCOPE_LOOP,	1,	"sloop","'scope loop:      sloop (COMMAND)"},
{FINITE_LOOP,	2,	"loop",	"Finite loop:      loop TIMES (COMMAND)"},
{DISP_INFO,	0,	"info",	"Slot contents:    info"},
{TEST_MEM,	2,	"mem",	"Mem diagnostic:   mem LOADDR HIADDR"},
{JUMP_ADDR,	1,	"j",	"Jump to address:  j ADDRESS"},
{JUMP_ADDR1,	2,	"j1",	"Jump to address:  j1 ADDRESS PARAM1"},
{JUMP_ADDR2,	3,	"j2",	"Jump to address:  j2 ADDRESS PARAM1 PARAM2"},
{LEFT_PAREN,	0,	"(",		""},
{DISP_CONF,	2,	"dc",	"Disp. config reg: dc SLOT REGNUM"},
{WRITE_CONF,	3,	"wc",	"Write config reg: wc SLOT REGNUM VALUE"},
{DISP_MC3,	1,	"dmc",	"Disp mem bd regs: dmc SLOT"},
{DISP_IO4,	1,	"dio",	"Disp io brd regs: dio SLOT"},
{RESET_SYSTEM,	0,	"reset","Reset the system: reset"},
{DISP_HELP,	0,	"?",	""},
{DISP_HELP,	0,	"help",	""},
{POD_RESUME, 	0,	"resume",""},
{SEND_INT,	3,	"si",	"Send interrupt:   si SLOT CPU LEVEL"},
{FLUSHI,	0,	"flushi",	""},
{FLUSHT,	0,	"flusht",	"Clear TLB:        flusht"},
{TLB_DUMP,	1,	"td",		"Dump TLB:         td <index | all>"},
{CLEAR_STATE,	0,	"clear",	"Clear State:      clear"},
{DECODE_ADDR,	1,	"decode",	"Decode Address:   decode PHYSADDR"},
{WALK_MEM,	3,	"walk",		"Walk a bit:       walk <loaddr> <hiaddr> <cont on fails>"},
{DTAG_DUMP,	1,	"dtag",		"Dump dcache tag:  dtag line "},
{ITAG_DUMP,	1,	"itag",		"Dump icache tag:  dtag line "},
{STAG_DUMP,	1,	"stag",		"Dump scache tag:  stag line "},
{DTAG_FDUMP,	1,	"dline",	"Dump dcache line: dlineline "},
{ITAG_FDUMP,	1,	"iline",	"Dump icache line: iline line "},
{STAG_FDUMP,	1,	"sline",	"Dump scache line: sline line "},
{DTAG_ADUMP,	1,	"adtag",	"Dump dcache tag:  dtag line "},
{ITAG_ADUMP,	1,	"aitag",	"Dump icache tag:  dtag line "},
{STAG_ADUMP,	1,	"astag",	"Dump scache tag:  stag line "},
{DTAG_AFDUMP,	1,	"adline",	"Dump dcache line: dlineline "},
{ITAG_AFDUMP,	1,	"ailine",	"Dump icache line: iline line "},
{STAG_AFDUMP,	1,	"asline",	"Dump scache line: sline line "},
{ECC_SLINE,	1,	"esline",	"Write ECC error in scache line"},
{STAG_DUMP_ALL, 1,	"staga",	"Dump scache tags: staga state_mask"},
{DTAG_DUMP_ALL, 1,	"dtaga",	"Dump dcache tags: dtaga state_mask"},
{SCACHE_TEST,	1,	"ts",		"Test scache: ts Pattern"},
{SCACHE_TEST_ALL,1,	"tsa",		"Test all scache: tsa Pattern"},
{SLAVE_MODE,	0,	"slave",	"Goto slave mode:  slave"},
{MARGIN,	3,	"margin",	"Voltage margin: margin SLOT VOLT +/-/0"},
{SET_PGSZ,	1,	"setpg",	"Set Page Size:    setpg PAGESIZE"},
{SHOW_PGSZ,	0,	"showpg",	"Show Page Size:   showpg"},
{DOWNLOAD_IO4,  0,	"io", 		"Download IO PROM: io"},
{WHY,		0,	"why",		"Why are we here?: why"},
{RECONF_MEM,	0,	"reconf",	""},
{GOTO_MEM,	0,	"gm",		""},
{GOTO_CACHE,	0,	"gc",		""},
{DO_BIST,	0,	"bist",		""},
{DISABLE_UNIT,	2,	"disable",	"Disable unit:     disable SLOT UNIT"},
{ENABLE_UNIT,	2,	"enable",	"Enable unit:      enable SLOT UNIT"},
{FDISABLE_UNIT,	2,	"fdisable",	"Force disable:    fdisable SLOT UNIT"},
{FENABLE_UNIT,	2,	"fenable",	"Force enable:     fenable SLOT UNIT"},
{DISP_EVCONFIG,	1,	"devc",		"Display config:   devc SLOT | all"},
{POD_SELECT,	1,	"select",	""},
{ZAP_INVENTORY,	0,	"zap",		"Reinit inventory: zap"},
{DISP_MPCONF,   1,      "dmpc",         "Display MPCONF:   dmpc VPID"},
{SLICE,		2,	"slice",	"Display all IP25 registers for a given slice: slice SLOT SLICE"},
{SLOT,		1,	"slot",		"Display all IP25 registers for a given slot: slot SLOT"},
{ALL,		0,	"all",		"Display all IP25 registers in system: all"},
{WAIT, 		0,	"wait",		"Wait for diag launch: wait"},
{CT_TEST,	0,	"ct",		"Compare CC/T5 2ndary tags: ct"},
{CT_TEST_ALL,	0,	"cta",		"Compare all CC/T5 2ndary tags: cta"},
{CT_TEST_STRICT,0,	"cts",		"Compare CC/T5 2ndary tags - strict: cts"},
{CT_TEST_STRICT_ALL,0,	"ctaa",		"Compare all CC/T5 2ndary tags - strict:ctsa"},
{NULL,		0,	"",		""},
};

/*
 * pod_parse is the commands driver for Power on menu.
 * Basic utilities for bring up, available functions are:
 *	- loprintf: can handle string, %s and %x.
 *	- pon_sload: down loading program from RS232 into cache/mem and run it.
 */

int find_token(int *cmd, int *cmd_token, int *num_args) {
	int i = 0;

	for (i = 0; (commands[i].token != NULL) &&
			(lo_strcmp(cmd, commands[i].name)); i++);
	if (*cmd == 0) {
		*cmd_token = NO_COMMAND;
		*num_args = 0;
	} else {
		*cmd_token = commands[i].token;
		*num_args = commands[i].args;
	}
	if (*cmd_token)
		return 1;
	else
		return 0;
}

static void skipwhite(int **src)
{
	while (**src == ' ' || **src == '\t')
		(*src)++;
}

static void parse_error(void)
{
	loprintf("*** POD syntax error.\n");
}

static void copystr(int **target, int **src)
{
	while (**src != ' ' && **src != '\t' && **src != 0 && **src != ';' 
								&& **src != ')')
		*((*target)++) = *((*src)++);
}


int get_arg(int *string, __scunsigned_t *value, int **bufp)
{
	int *ip;

	skipwhite(bufp);

	if (!(**bufp)) {
		loprintf("*** Insufficient arguments\n");;
		return 0;
	}
	/* get argv1 */
	ip = string;
	if (**bufp == '(') {
		*ip++ = **bufp;
		(*bufp)++;
		*ip = 0;
	} else {
		copystr(&ip, bufp);
		*ip = 0;
		*value = lo_atoh(string);
	}
	return 1;
}


int check_eol(int **bufp) {

	if (*bufp == (int *)NULL) {
		return 1;
	}

	if (**bufp == ')') {
		(*bufp)++;
		return 1;
	}

	if (!(**bufp)) {
		return 1;
	}

	if (**bufp == ';') {
		(*bufp)++;
	}

	return 0;
}


int *pod_parse(int *buf, struct reg_struct *gprs, int parse_level, int sloop,
				struct flag_struct *flags)
{
    int *bufp = buf;
    int cmd[32];
    int argv1[32];
    int argv2[32];
    int argv3[32];
    int *ip;
    __scunsigned_t arg1_val;
    __scunsigned_t arg2_val;
    __scunsigned_t arg3_val;
    int line_end;
    int cmd_token;
    int num_args;
    int i;
    __uint64_t addr;

    if (parse_level >= MAX_NEST) {
	loprintf("*** Too many levels of loops/parentheses.\n");
	return (int *)NULL;
    }

    line_end = 0;
    /* command format: cmd [argv1] [argv2] */
    /* get command */

    while (!line_end) {
	skipwhite(&bufp);
	ip = cmd;
	if (*bufp == '(') {
	    *ip++ = *bufp++;
	} else {
	    copystr(&ip, &bufp);
	}

	*ip = 0;
	if (!find_token(cmd, &cmd_token, &num_args)) {
	    loprintf("*** Invalid POD command: '%p'\n", cmd);
	    return (int *)NULL;
	}
	if (num_args > 0) {
	    if (!get_arg(argv1, &arg1_val, &bufp))
		return (int *)NULL;
	}

	if (num_args > 1) {
	    if (!get_arg(argv2, &arg2_val, &bufp))
		return (int *)NULL;
	}

	if (num_args > 2) {
	    if (!get_arg(argv3, &arg3_val, &bufp))
		return (int *)NULL;
	}
	line_end = check_eol(&bufp);

	if ((flags->selected != 0xff) &&
	    (flags->slice != flags->selected) &&
	    (cmd_token != POD_SELECT)) {
	    loprintf("*** Not selected\n");
	    return (int *)NULL;
	}

	do {
	    switch (cmd_token) {
	    case POD_SELECT:
		flags->selected = (char)(arg1_val & 0xff);
		loprintf("Selected slice %b\n",flags->selected);
		break;
	    case WALK_MEM:
		pod_walk(arg1_val, arg2_val, (int)arg3_val, 1);
		break;
	    case DO_BIST:
		if (flags->mem) {
		    loprintf(
			     "Your stack must be in cache to reconfigure memory.  Use the 'gc' command.\n");
		} else {
		    pod_bist();
		}
		break;
	    case RECONF_MEM:
		if (flags->mem) {
		    loprintf(
			     "Your stack must be in cache to reconfigure memory.  Use the 'gc' command.\n");
		} else {
		    pod_reconf_mem();
		}
		break;
	    case TLB_DUMP:
		tlb_dump(argv1, (int)arg1_val);
		break;
	    case DISP_EVCONFIG:
		dump_evcfg(argv1, (int)arg1_val);
		break;
	    case DISP_MPCONF:
		dump_mpconf((uint)arg1_val);
		break;

	    case DTAG_DUMP:		/* Dump data tags  */
		dumpPrimaryDataLine((int)arg1_val, 0, 0);
		dumpPrimaryDataLine((int)arg1_val, 1, 0);
		break;

	    case DTAG_FDUMP:		/* Dump data tags with data */
		dumpPrimaryDataLine((int)arg1_val, 0, 1);
		dumpPrimaryDataLine((int)arg1_val, 1, 1);
		break;

	    case ITAG_DUMP:		/* Dump intruction tags */
		dumpPrimaryInstructionLine((int)arg1_val, 0, 0);
		dumpPrimaryInstructionLine((int)arg1_val, 1, 0);
		break;

	    case ITAG_FDUMP:		/* Dump intruction tags + data */
		dumpPrimaryInstructionLine((int)arg1_val, 0, 1);
		dumpPrimaryInstructionLine((int)arg1_val, 1, 1);
		break;

	    case STAG_DUMP:
		dumpSecondaryLine((int)arg1_val, 0, 0);
		dumpSecondaryLine((int)arg1_val, 1, 0);
		break;

	    case STAG_FDUMP:
		dumpSecondaryLine((int)arg1_val, 0, 1);
		dumpSecondaryLine((int)arg1_val, 1, 1);
		break;
	    case DTAG_ADUMP:		/* Dump data tags  */
		dumpPrimaryDataLineAddr((int)arg1_val, 0, 0);
		dumpPrimaryDataLineAddr((int)arg1_val, 1, 0);
		break;

	    case DTAG_AFDUMP:		/* Dump data tags with data */
		dumpPrimaryDataLineAddr((int)arg1_val, 0, 1);
		dumpPrimaryDataLineAddr((int)arg1_val, 1, 1);
		break;

	    case ITAG_ADUMP:		/* Dump intruction tags */
		dumpPrimaryInstructionLineAddr((int)arg1_val, 0, 0);
		dumpPrimaryInstructionLineAddr((int)arg1_val, 1, 0);
		break;

	    case ITAG_AFDUMP:		/* Dump intruction tags + data */
		dumpPrimaryInstructionLineAddr((int)arg1_val, 0, 1);
		dumpPrimaryInstructionLineAddr((int)arg1_val, 1, 1);
		break;

	    case STAG_ADUMP:
		dumpSecondaryLineAddr((int)arg1_val, 0, 0);
		dumpSecondaryLineAddr((int)arg1_val, 1, 0);
		break;

	    case STAG_AFDUMP:
		dumpSecondaryLineAddr((int)arg1_val, 0, 1);
		dumpSecondaryLineAddr((int)arg1_val, 1, 1);
		break;

	    case STAG_DUMP_ALL:
		dumpSecondaryCache(1);
		break;

	    case DTAG_DUMP_ALL:
		dumpPrimaryCache(1);
		break;

	    case ECC_SLINE:
		setSecondaryECC((__uint64_t)arg1_val | K0BASE);
		break;

	    case SCACHE_TEST:
		testScacheFlipBits((__uint64_t)arg1_val, 1);
		break;

	    case SCACHE_TEST_ALL:
		testScacheFlipBits((__uint64_t)arg1_val, 0);
		break;

	    case DISP_MC3:
		dump_mc3((uint)arg1_val);
		break;
	    case DISP_IO4:
		dump_io4((uint)arg1_val);
		break;
	    case DISP_CONF:
		conf_register(READ, (uint)arg1_val, (uint)arg2_val, 0,
			      parse_level);
		break;
	    case WRITE_CONF:
		conf_register(WRITE, (uint)arg1_val, (uint)arg2_val,
			      arg3_val, parse_level);
		break;
	    case DISP_HELP:
		loprintf("\
All numerical inputs should be in hex with or without 0x preceding them.\n");
		loprintf("\
Commands may be separated by semicolons, and loops may be nested.\n");
		for (i = 0; (commands[i].token != NULL); i++)
		    if (get_char(commands[i].help))
			loprintf("     %s\n",
				 commands[i].help);
		break;
	    case UWRITE_BYTE:
		memory(WRITE, BYTE, K1BASE | arg1_val, arg2_val,
		       parse_level);
		break;
	    case UWRITE_HALF:
		memory(WRITE, HALF, K1BASE | arg1_val, arg2_val,
		       parse_level);
		break;
	    case UWRITE_WORD:
		memory(WRITE, WORD, K1BASE | arg1_val, arg2_val,
		       parse_level);
		break;
	    case UWRITE_DOUBLE:
		memory(WRITE, DOUBLE, K1BASE | arg1_val, arg2_val,
		       parse_level);
		break;
	    case UDISP_BYTE:
		memory(READ, BYTE, K1BASE | arg1_val, 0, parse_level);
		break;
	    case UDISP_HALF:
		memory(READ, HALF, K1BASE | arg1_val, 0, parse_level);
		break;
	    case UDISP_WORD:
		memory(READ, WORD, K1BASE | arg1_val, 0, parse_level);
		break;
	    case UDISP_DOUBLE:
		memory(READ, DOUBLE, K1BASE | arg1_val, 0, parse_level);
		break;
	    case WRITE_BYTE:
		memory(WRITE, BYTE, K0BASE | arg1_val, arg2_val,
		       parse_level);
		break;
	    case WRITE_HALF:
		memory(WRITE, HALF, K0BASE | arg1_val, arg2_val,
		       parse_level);
		break;
	    case WRITE_WORD:
		memory(WRITE, WORD, K0BASE | arg1_val, arg2_val,
		       parse_level);
		break;
	    case WRITE_DOUBLE:
		memory(WRITE, DOUBLE, K0BASE | arg1_val, arg2_val,
		       parse_level);
		break;
	    case DISP_BYTE:
		memory(READ, BYTE, K0BASE | arg1_val, 0, parse_level);
		break;
	    case DISP_HALF:
		memory(READ, HALF, K0BASE | arg1_val, 0, parse_level);
		break;
	    case DISP_WORD:
		memory(READ, WORD, K0BASE | arg1_val, 0, parse_level);
		break;
	    case DISP_DOUBLE:
		memory(READ, DOUBLE, K0BASE | arg1_val, 0, parse_level);
		break;
            case NIB_DISP_MASTER:
		for (i = 0; i < arg1_val; i++) {
		    addr = MASTER_DEBUG_ADDR + (i * 8);
		    loprintf("%x: %y \n", (__uint64_t *) addr, *(__uint64_t *)addr);
		}
		break;

            case NIB_DISP_SLAVE:
		for (i = 0; i < arg1_val; i++) {
		    addr = SLAVE_DEBUG_ADDR + (i * 8);
		    loprintf("%x: %y \n", addr, *(__uint64_t *)addr);
		}
		break;

	    case GOTO_MEM:
		run_cached(pod_loop);
		/* Never returns */
		break;
	    case GOTO_CACHE:
		podMode(EVDIAG_DEBUG,
			"Putting stack in dcache\r\n");
		/* Never returns */
		break;
	    case RESET_SYSTEM:
		reset_system();
		break;
	    case DISP_REG:
		_register(READ, argv1, 0, gprs);
		break;
	    case POD_RESUME:
		podResume();
		break;
	    case CLEAR_STATE:
		clearIP25State();
		loprintf("Cleared CPU error state.\n");
		clear_mc3_state();
		loprintf("Cleared memory error state.\n");
		clear_io4_state();
		loprintf("Cleared IO error state.\n");
		break;
	    case SEND_INT:
		send_int((int)arg1_val, (int)arg2_val, (int)arg3_val);
		break;
	    case WRITE_REG:
		_register(WRITE,argv1,arg2_val,gprs);
		break;
	    case WHY:
		flags->scroll_msg = 1;
		loprintf("Reason for entering POD mode: %s\n",
			 flags->diag_string);
		break;
	    case SCOPE_LOOP:
		if (*argv1 != '(') {
		    parse_error();	
		    return (int *)NULL;
		}	
		ip = pod_parse(bufp, gprs, parse_level+1, 1, 
			       flags);
		if (ip == (int *)NULL)
		    return ip;
		bufp = ip;
		line_end = check_eol(&bufp);
		/* Recheck the end of line because we made
		   recursive parser calls in the switch */
		break;
	    case ZAP_INVENTORY:
		zap_inventory();
		break;

	    case FINITE_LOOP:
		if (*argv2 != '(') {
		    parse_error();	
		    return (int *)NULL;
		}
		if (!arg1_val) {
		    loprintf("*** Loop count must be > 0.\n");
		    return (int *)NULL;
		}
				
		for (i = 0; i < arg1_val; i++) {
		    ip = pod_parse(bufp, gprs,
				   parse_level+1, 0, flags);
		    if (ip == (int *)NULL)
			return ip;
		}
		bufp = ip;
		line_end = check_eol(&bufp);
		/* Recheck the end of line because we made
		   recursive parser calls in the switch */
		break;
	    case LEFT_PAREN:
		bufp = pod_parse(bufp, gprs, parse_level+1, 0,
				 flags);
		line_end = check_eol(&bufp);
		/* Recheck the end of line because we made
		   recursive parser calls in the switch */
		break;
	    case DISP_INFO:
		info();
		break;
	    case DECODE_ADDR:
		decode_address(arg1_val);
		break;
	    case FLUSHT:
		flushTlb();
		break;
	    case FLUSHI:
		invalidateIDcache();
		break;
	    case TEST_MEM:
		mem_test(arg1_val, arg2_val);
		break;
	    case SLAVE_MODE:
		jump_addr((__psunsigned_t)prom_slave, 0, 0, flags);
		break;
	    case JUMP_ADDR:
		arg2_val = 0;
		break;
		/* Fall through */
	    case JUMP_ADDR1:
		arg3_val = 0;
		/* Fall through */
	    case JUMP_ADDR2:
		loprintf("Returned %x\n", 
			 jump_addr(arg1_val,
				   (uint)arg2_val, (uint)arg3_val, flags));
		break;

	    case DOWNLOAD_IO4:
		load_io4prom();
		break;
	    case DISABLE_UNIT:
		(void)set_unit_enable((uint)arg1_val, (uint)arg2_val, 0, 0);
		break;
	    case FDISABLE_UNIT:
		(void)set_unit_enable((uint)arg1_val, (uint)arg2_val, 0, 1);
		break;
	    case ENABLE_UNIT:
		(void)set_unit_enable((uint)arg1_val, (uint)arg2_val, 1, 0);
		break;
	    case FENABLE_UNIT:
		(void)set_unit_enable((uint)arg1_val, (uint)arg2_val, 1, 1);
		break;
	    case SLOT:
		dumpSlot((int)arg1_val);
		break;
	    case SLICE:
		dumpSlice((int)arg1_val, (int)arg2_val);
		break;
	    case ALL:
		dumpAll();
		break;
	    case WAIT:
		wait();
		break;
	    case CT_TEST:
		compareSecondaryTags(0, 0);
		break;
	    case CT_TEST_ALL:
		compareSecondaryTags(1, 0);
		break;
	    case CT_TEST_STRICT:
		compareSecondaryTags(0, 1);
		break;
	    case CT_TEST_STRICT_ALL:
		compareSecondaryTags(1, 1);
		break;
	    case MARGIN:
		margin(arg1_val, argv2, argv3);
		break;
	    case NO_COMMAND:
		break;
	    default:
		loprintf("*** Unimplemented POD command: '%p'\n", cmd);
		break;
	    } /* Switch */
	    if (sloop)
		if (pod_poll()) {
		    pod_getc();
		    sloop = 0;
		}
	} while (sloop);
    } /* While line end */
    return bufp;
}

int lo_strcmp(int *ip, char *cp)			/* biendian */
{
	FLIPINIT(dosw, cp);

	while (*ip == ' ' && *ip != 0)
		ip++;
	while ((EVCFLIP(dosw, cp) != 0) || (*ip != 0)) {
		if (*ip++ == EVCFLIP(dosw,cp++))
			continue;
		else
			return(-1);
	}
	return(0);
}


void lo_strcpy(char *dest, char *src)			/* biendian */
{
	FLIPINIT(dosw, src);

	while (EVCFLIP(dosw, src) != 0)
		*dest++ = EVCFLIP(dosw, src++);
}

__scunsigned_t lo_atoh(int *cp)
{
	register __scunsigned_t i;

	/* Ignore leading 0x or 0X */
	if (*cp == '0' && (*(cp+1) == 'x' || *(cp+1) == 'X'))
		cp += 2;

	for (i = 0 ; lo_ishex(*cp) ;)
		if (*cp <= '9')
			i = i*16 + *cp++ - '0';
		else {
			if (*cp >= 'a')
				i = i * 16 + *cp++ - 'a' + 10;
			else
				i = i * 16 + *cp++ - 'A' + 10;
		}
	return i;
}

uint lo_atoi(int *cp)
{
	register unsigned i;

	for (i = 0 ; isdigit(*cp) ;)
		i = i*10 + *cp++ - '0';
	return i;
}

int lo_ishex(int c)
{
	if ((c >= '0' && c <= '9') ||
	    (c >= 'A' && c <= 'F') ||
	    (c >= 'a' && c <= 'f')) {
		return 1;
	}
	else
		return 0;
}

#define	NEWPOD 0
#if NEWPOD
/**************************************************************************
 *									  *
 * 		 Copyright (C) 1993 Silicon Graphics, Inc.	          *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#define	isspace(c) (((c) == ' ') || ((c) == '\t'))

#include "parse.h"

static	command_t	parse_commandTable[] = {
    {parseHelp,		"?",		"[command]"},
    {parseHelp,		"help",		"[command]"},
    {podJump,		"jump",		"[-s] <address> [#] [#]"},
    {podIcache,		"icache",	"[-c][-i][-t] <address or index>"},
    {podDcache,		"dcache",	"[-c][-i][-t] <address or index>"},
    {podScache,		"scache",	"[-c][-i][-t] <address or index>"},
    {regCmd, 		"reg",		"<name> [slot [slice]]"},
    {regDecodeCmd, 	"decode",	"<register> <value>"},
    {podReset,		"reset",	"--"},
    {NULL, 		NULL,		NULL}
};

int
parseStrCmp(char *s1, const char *s2)
{
    FLIPINIT(dosw2, s2);

    while ( *s1 && (*s1 == EVCFLIP(dosw2, s2))) {
	s1++;
	s2++;
    }
    return(*s1 != EVCFLIP(dosw2, s2));
}

int
parseToHex64(char *s, __uint64_t *v)
/*
 * Function: parseToHex64
 * Purpose: To parse a character string into a 64-bit hex number
 * Parameters:	s - pointer to string to parse
 *		v - pointer to location to store 64-bit value.
 * Returns:	0 - failed, !0 - OK
 */
{
    char	c;

    if ((s[0] == '0') && (s[0] && ((s[1] == 'x') || (s[1] == 'X')))) {
	s += 2;
    }
    
    *v = 0;

    while (c = *s++) {
	if ((c >= '0') && (c <= '9')) {
	    *v = (*v << 4) | (c - '0');
	} else if ((c >= 'a') && (c <= 'f')) {
	    *v = (*v << 4) | (c - 'a') + 10;
	} else if ((c >= 'A') && (c <= 'F')) {
	    *v = (*v << 4) | (c - 'A') + 10;
	} else {
	    return(0);
	}
    }
    return(1);
}

int
parseToHex32(char *s, __uint32_t *v)
/*
 * Function: parseToHex32
 * Purpose: To convert ascii hex to 32-bit number
 * Parameters:	s - pointer to string
 *		v - pointer to 32-value on completion
 * Returns: 0 - failed, !0 OK
 */
{
    int	rv;
    __uint64_t	tv;
    
    rv = parseToHex64(s, &tv);
    *v = (__uint32_t)tv;
    return(rv);
}

int
parseToInt64(char *s, __uint64_t *v)
/*
 * Function: parseToInt64
 * Purpose: To parse an integer string and return its value
 * Parameters: 	s - pointer to string to parse
 *		v - pointer to 64-bit location result stored at.
 * Returns: 0 - failed, !0 - ok
 */
{
    char	c;

    *v = 0;

    if (s[0] == '0' && ((s[1] == 'x') || (s[1] == 'X'))) {
	return(parseToHex64(s, v));
    }

    while (c = *s++) {
	if ((c >= '0') && (c <= '9')) {
	    *v = *v * 10 + (c - '0');
	} else {
	    return(0);
	}
    }
    return(1);
}

#define	static

int
parseToInt32(char *s, __uint32_t *v)
/*
 * Function: parseToInt32
 * Purpose: Convert character hex input to 32-bit integer (assumes base-10)
 * Parameters: 	s - pointer to string to convert
 *		v - pointer to 32-bit integer stored on return.
 * Returns: 0 - failed, !0 - ok
 */
{
    __uint64_t	_v;
    int		rv;			/* return value */

    rv = parseToInt64(s, &_v);
    *v = (__uint32_t)_v;
    return(rv);
}

int
parseGetOpt(int argc, char **argv, int *optargc, char **optarg, char *options)
/*
 * Function: 	parseGetOpt
 * Purpose:    	Parse options like "getopt"
 * Parameters:	argc - # arguments
 *		argv - pointer to list of arguments
 *		optargc - pointer to current "getopt" argc value
 *		optarg - pointer to string pointer updated to point to 
 *		 	argument on return.
 *		opriont - pointer to string of options.
 * Returns: '?' if option not found, or character found. 0 --> indicates all
 *		done.
 */
{
    *optargc += 1;

    if (*optargc >= argc) {
	return(0);
    }

    if (argv[*optargc][0] != '-') {
	return(0);
    }

    if (argv[*optargc][1] == '-') {	/* -- case */
	*optargc += 1;
	return(0);
    }

    if (optarg) {
	*optarg = 0;
    }

    while (*options) {
	if (*options == argv[*optargc][1]) { /* is there an option? */
	    if (*(options + 1) == ':') { /* yes */
		if (argv[*optargc][2]) { /* right after it */
		    *optarg = &argv[*optargc][2];
		} else {		/* use next argument */
		    (*optargc)++;
		    if (*optargc > argc) {
			loprintf("%s: option requires a value: %s\n", 
				 argv[0], argv[(*optargc) - 1]);
			return('?');
		    }
		    *optarg = argv[*optargc];
		}
	    } else if (argv[*optargc][2]) { /* right after it */
		loprintf("%s: option does not require a value: %s\n", argv[0],
		       argv[*optargc]);
		return('?');
	    }
	    return(*options);
	}
	options += 1;
	if (*options == ':') {
	    options += 1;
	}
    }
    return('?');
}


static command_t *
parseLookup(char *s, command_t *ct)
/*
 * Function: parseLookup
 * Purpose: To look up a command in a command table
 * Parameters: 	s - poitner to string (ON STACK) to match.
 *		c - pointer to command table to look in.
 * Returns: Pointer to command structure OR NULL if not found.
 */
{
    register command_t	*c;

    for (c = ct; NULL != c->c_name; c++) {
	if (0 == parseStrCmp(s, c->c_name)) {
	    return(c);
	}
    }
    return(NULL);
}

static	int
parseBreakLine(char *s, int maxargc, int *argc, char *argv[])
/*
 * Function: parseBreakLine
 * Purpose:
 * Parameters:
 * Returns:
 */
{

    for (*argc = 0; (*s) && (*argc < maxargc); (*argc)++) {
	argv[*argc] = s;
	while (*s) {
	    if (isspace(*s)) {
		*s = '\0';
		s++;
		while (isspace(*s) && *s) { /* Skip ahead to next argument */
		    s++;
		}
		break;
	    }
	    s++;
	}
    }
    return(*s ? -1 : 0);
}

cmdRval_t
parseExecuteCommand(int argc, char *argv[], int level)
/*
 * Function: 	parseExecuteCommand
 * Purpose:	To actually execute a command line.
 * Parameters:	
 * Returns:
 */
{
    command_t	*c;
    cmdRval_t	crv;

    c = parseLookup(argv[0], parse_commandTable);

    if (NULL == c) {
	loprintf("%s: not found\n", argv[0]);
	return(pFailed);
    }

    crv = c->c_function(c, argc, argv, level);
    if (pHelp == crv) {
	/*
	 * If we want to print out help info, do it on the command that 
	 * failed - and don't propogate back up.
	 */
	loprintf("%s %s\n", c->c_name, c->c_help);
	if (0 != level) {
	    crv = pFailed;
	}
    }
    return(crv);
}    

cmdRval_t
parseExecuteLine(char *s, struct reg_struct *gprs, int level, int sloop)
/*
 * Function: parseExecuteLine
 * Purpose: To parse and execute an input line.
 * Parameters: s - input line (modified on return)
 * Returns: 0 - success, -1 failed.
 */
{
    char	*argv[COMMAND_ARGC];
    int		argc;

    if (0 > parseBreakLine(s, COMMAND_ARGC, &argc, argv)) {
	loprintf("Line too long or too many arguments\n");
	return(pFailed);
    } else if (argc == 0) {		/* empty line */
	return(pOK);
    }
    return(parseExecuteCommand(argc, argv, 0));
}

cmdRval_t
parseHelp(command_t *c, int argc, char **argv, int level)
/*
 * Function: 
 * Purpose:
 * Parameters:
 * Returns:
 */
{
    command_t	*ct;			/* temp command pointer */
    int		i;

    if (argc == 1) {			/* no command given */
	for (i = 0, ct = parse_commandTable; ct->c_name; ct++, i++) {
	    loprintf("%s\t%s\n", ct->c_name, ct->c_help);
	    if (i && ((i % 20) == 0)) {
		loprintf(" ... more");
		pod_getc();
	    }
	}
    } else {
	ct = parseLookup(argv[1], parse_commandTable);
	if (NULL == c) {
	    loprintf("%s: command not found: %s\n", c->c_name, argv[1]);
	    return(pFailed);
	} else {
	    loprintf("%s\t%s\n", ct->c_name, ct->c_help);
	}
    }
    return(pOK);
}


int
parseNew(int dex, int slice, int slot)
/*
 * Function: 
 * Purpose:
 * Parameters:
 * Returns:
 */
{
    char	b[256];
    extern	char * logetstring(char *, int);
    __uint64_t	ertoip;

    for (;;) {
	if (dex) {
	    loprintf("POD %b/%b> ", slot, slice);
	} else {
	    loprintf("Mem %b/%b> ", slot, slice);
	}
	logetstring(b, sizeof(b));
	parseExecuteLine(b, 0, 0, 0);
	if (ertoip = LD(EV_ERTOIP)) {
	    xlate_ertoip(ertoip);
	}
    }
}

#endif
