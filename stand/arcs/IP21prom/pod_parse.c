/***********************************************************************\
*       File:           pod_parse.c                                     *
*                                                                       *
\***********************************************************************/

#include <sys/types.h>
#include <sys/sbd.h>
#include "biendian.h"
#include "pod.h"
#include "ip21prom.h"
#include "pod_iadefs.h"
#include "pod_failure.h"
#include "prom_externs.h"
#include "niblet_debug.h"

#ifndef NO_NIBLET
extern int do_niblet(unsigned int);
#endif

int	load_io4prom(void);
int	dump_tag(int, __scunsigned_t);
int	pon_sload(int, struct flag_struct*);
int	pod_walk(__psunsigned_t, __psunsigned_t, int, int);
int	mem_test(__psunsigned_t, __psunsigned_t);
int	gparity(__psunsigned_t, __psunsigned_t);
int	jump_addr(__psunsigned_t, uint, uint, struct flag_struct *);
void dump_all_stag(int);
void dump_all_dtag(int);
void gcache_invalidate(void);
void gcache_check_tag_addr(void);
void gcache_check_tag_data(void);
void gcache_check_data(void);
void gcache_check(void);
int _count(int, int);

void	_register(int, int *, __scunsigned_t, struct reg_struct *);
void	decode_address(__scunsigned_t);
void	run_cached(void (*function)(int, int));
void	reset_system(void);
void	conf_register(int, uint, uint, __uint64_t, int);
void	dump_mc3(uint);
void	dump_io4(uint);
void	dump_evcfg(int *, int);
void	pod_bist(void);
void	pod_resume(void);
void	pod_reconf_mem(void);
void	tlb_dump(int *, int);
void	clear_ip21_state(void);
void	clear_mc3_state(void);
void	clear_io4_state(void);
void	zap_inventory(void);
void	dump_mpconf(uint);
void	show_page_size(void);
void	set_page_size(int);


static void	parse_error(void);
static void	skipwhite(int **);
static void	copystr(int **, int **);

const struct cmd_table {
	int token;
	int args;
	char *name;
	unsigned char *help;
} commands[] = {
{WRITE_BYTE,	2,	"wb",		"Write byte:       wb ADDRESS NUMBER"},
{WRITE_HALF,	2,	"wh",		"Write halfword:   wh ADDRESS NUMBER"},
{WRITE_WORD,	2,	"ww",		"Write word:       ww ADDRESS NUMBER"},
{WRITE_DOUBLE,	2,	"wd",		"Write double:     wd ADDRESS NUMBER"},
{DISP_BYTE,	1,	"db",		"Display byte:     db ADDRESS"},
{DISP_HALF,	1,	"dh",		"Display halfword: dh ADDRESS"},
{DISP_WORD,	1,	"dw",		"Display word:     dw ADDRESS"},
{DISP_DOUBLE,	1,	"dd",		"Display double:   dd ADDRESS"},
{NIB_DISP_MASTER,	1,	"dm",		"Display niblet master debug buffer:   dm NUMBER"},
{NIB_DISP_SLAVE,	1,	"ds",		"Display niblet slave debug buffer:   ds NUMBER"},
/*
{SERIAL_LOAD,	0,	"sload",	"Serial download:  sload"},
{SERIAL_RUN,	0,	"srun",		"Download and run: srun"},
{LOAD_IOPROM,	1,	"pload",	"IO prom download: pload [baudrate]"},
*/
{DISP_REG,	1,	"dr",		"Display register: dr <sr || sp || all>"},
{WRITE_REG,	2,	"wr",		"Write register:   wr < sr >"},
{SCOPE_LOOP,	1,	"sloop",	"'scope loop:      sloop (COMMAND)"},
{FINITE_LOOP,	2,	"loop",		"Finite loop:      loop TIMES (COMMAND)"},
{DISP_INFO,	0,	"info",		"Slot contents:    info"},
{TEST_MEM,	2,	"mem",		"Mem diagnostic:   mem LOADDR HIADDR"},
{JUMP_ADDR,	1,	"j",		"Jump to address:  j ADDRESS"},
{JUMP_ADDR1,	2,	"j1",		"Jump to address:  j1 ADDRESS PARAM1"},
{JUMP_ADDR2,	3,	"j2",		"Jump to address:  j2 ADDRESS PARAM1 PARAM2"},
{LEFT_PAREN,	0,	"(",		""},
{DISP_CONF,	2,	"dc",		"Disp. config reg: dc SLOT REGNUM"},
{WRITE_CONF,	3,	"wc",		"Write config reg: wc SLOT REGNUM VALUE"},
{DISP_MC3,	1,	"dmc",		"Disp mem bd regs: dmc SLOT"},
{DISP_IO4,	1,	"dio",		"Disp io brd regs: dio SLOT"},
{RESET_SYSTEM,	0,	"reset",	"Reset the system: reset"},
{DISP_HELP,	0,	"?",		""},
{DISP_HELP,	0,	"help",		""},
{NIBLET,	1,	"niblet",	"Run Niblet:       niblet <0 - 9>"},
{POD_RESUME, 	0,	"resume",	""},
{SEND_INT,	3,	"si",		"Send interrupt:   si SLOT CPU LEVEL"},
{FLUSHI,	0,	"flushi",	""},
{FLUSHT,	0,	"flusht",	"Clear TLB:        flusht"},
{TLB_DUMP,	1,	"td",		"Dump TLB:         td <index | all>"},
{CLEAR_STATE,	0,	"clear",	"Clear State:      clear"},
{DECODE_ADDR,	1,	"decode",	"Decode Address:   decode PHYSADDR"},
{WALK_MEM,	3,	"walk",		"Walk a bit:       walk <loaddr> <hiaddr> <cont on fails>"},
{DTAG_DUMP,	1,	"dtag",		"Dump dcache tag:  dtag addr "},
{STAG_DUMP,	1,	"stag",		"Dump scache tag:  stag PHYSADDR "},
{STAG_DUMP_ALL, 1,	"staga",	"Dump scache tags: staga state_mask"},
{DTAG_DUMP_ALL, 1,	"dtaga",	"Dump dcache tags: dtaga state_mask"},
{SLAVE_MODE,	0,	"slave",	"Goto slave mode:  slave"},
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
{GPARITY_R,	2,	"gparity_r",	"G$ Parity Range:  gparity_r LOADDR HIADDR"},
{GPARITY,	0,	"gparity",	"G$ Parity:        gparity"},
#ifdef DEBUG
{GCACHE_INVALIDATE, 0, "gcache_invalidate", "invalidate gcache:gcache_invalidate"},
{GCACHE_CHECK_TAG_ADDR, 0, "gcache_check_tag_addr", "G$ tag addr test: gcache_check_tag_addr"},
{GCACHE_CHECK_TAG_DATA, 0, "gcache_check_tag_data", "G$ tag data test: gcache_check_tag_data"},
{GCACHE_CHECK_DATA, 0, "gcache_check_data",         "G$ data check:    gcache_check_data"},
{GCACHE_CHECK, 0, "gcache_check",                   "G$ test:          gcache_check"},
#endif
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
	int i,x, nib_num;
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
				flags->selected = arg1_val & 0xff;
				loprintf("Selected slice %b\n",flags->selected);
				break;
			/*
			case SERIAL_LOAD:
			case SERIAL_RUN:
				if (!flags->mem) {
					loprintf(
"Your stack must be in memory to download.  Use the 'gm' command.\n");
				} else {
					pon_sload(cmd_token, flags);
				}
				break;
			*/
			case WALK_MEM:
				pod_walk(arg1_val, arg2_val, arg3_val, 1);
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
				tlb_dump(argv1, arg1_val);
				break;
			case DISP_EVCONFIG:
				dump_evcfg(argv1, arg1_val);
				break;
                        case DISP_MPCONF:
                                dump_mpconf(arg1_val);
                                break;
			case DTAG_DUMP:
			case STAG_DUMP:
				dump_tag(cmd_token, arg1_val);
				break;

			case STAG_DUMP_ALL:
				dump_all_stag(arg1_val);
				break;

			case DTAG_DUMP_ALL:
				dump_all_dtag(arg1_val);
				break;

			case DISP_MC3:
				dump_mc3(arg1_val);
				break;
			case DISP_IO4:
				dump_io4(arg1_val);
				break;
			case DISP_CONF:
				conf_register(READ, arg1_val, arg2_val, 0,
							parse_level);
				break;
			case WRITE_CONF:
				conf_register(WRITE, arg1_val, arg2_val,
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
			case SET_PGSZ:
				set_page_size((int)arg1_val);
				break;
			case SHOW_PGSZ:
				show_page_size();
				break;
			case WRITE_BYTE:
				memory(WRITE, BYTE, arg1_val, arg2_val,
							parse_level);
				break;
			case WRITE_HALF:
				memory(WRITE, HALF, arg1_val, arg2_val,
							parse_level);
				break;
			case WRITE_WORD:
				memory(WRITE, WORD, arg1_val, arg2_val,
							parse_level);
				break;
			case WRITE_DOUBLE:
				memory(WRITE, DOUBLE, arg1_val, arg2_val,
							parse_level);
				break;
			case DISP_BYTE:
				memory(READ, BYTE, arg1_val, 0, parse_level);
				break;
			case DISP_HALF:
				memory(READ, HALF, arg1_val, 0, parse_level);
				break;
			case DISP_WORD:
				memory(READ, WORD, arg1_val, 0, parse_level);
				break;
			case DISP_DOUBLE:
				memory(READ, DOUBLE, arg1_val, 0, parse_level);
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

#ifndef NO_NIBLET
			case NIBLET:
				if (!flags->mem) {
					loprintf(
"Your stack must be in memory to run Niblet.  Use the 'gm' command.\n");
				} else {
					if (!lo_strcmp(argv1, "all")) {
						for (i = 0; i < 10; i++) {
							loprintf(" -- Test #%d --\n",
									i);
							flush_entire_tlb();
							x = do_niblet(i);
							if (x)
								break;
						}
						if (!x)
						    loprintf("All tests passed.\n");
						else
						    loprintf("*** Test %d FAILED\n",
								i);
					} else if (!lo_strcmp(argv1, "rand")) {
/*						loprintf("Randomly executings 50 niblet tests ...\n"); */
						loprintf("Sorry, not currently supported\n");
						break;
#if 0
						for (i = 0; i < 50; i++) {
							nib_num = _count(0, 0);
							nib_num = nib_num & 0x1f;  /* numbers from 0 to 31 */
							loprintf("Executing niblet %x\n", nib_num);
							flush_entire_tlb();
							x = do_niblet(nib_num);
							loprintf("niblet %x returned %x\n", nib_num, x);
							if (x) 
								break;
						}
						if (!x)
							loprintf("All tests passed.\n");
						else 
							loprintf("*** Test %d FAILED\n", nib_num);
#endif
					} else {
						flush_entire_tlb();
						do_niblet(arg1_val);
					}
				}
				break;
#endif /* ! NO_NIBLET */
			case GOTO_MEM:
				run_cached(pod_loop);
				/* Never returns */
				break;
			case GOTO_CACHE:
				pod_handler("Putting stack in dcache\r\n",
							EVDIAG_DEBUG);
				/* Never returns */
				break;
			case RESET_SYSTEM:
				reset_system();
				break;
			case DISP_REG:
				_register(READ, argv1, 0, gprs);
				break;
			case POD_RESUME:
				pod_resume();
				break;
			case CLEAR_STATE:
				clear_ip21_state();
				loprintf("Cleared CPU error state.\n");
				clear_mc3_state();
				loprintf("Cleared memory error state.\n");
				clear_io4_state();
				loprintf("Cleared IO error state.\n");
				break;
			case SEND_INT:
				send_int(arg1_val, arg2_val, arg3_val);
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
				flush_entire_tlb();
				break;
			case FLUSHI:
				call_asm(pon_invalidate_IDcaches, 0);
				break;
			case TEST_MEM:
				mem_test(arg1_val, arg2_val);
				break;
			case GPARITY_R:
				gparity(arg1_val, arg2_val);
				break;
			case GPARITY:
				gparity( 0x01000000, 0x01800000 );
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
						arg2_val, arg3_val, flags));
				break;

			case DOWNLOAD_IO4:
				load_io4prom();
				break;
			case DISABLE_UNIT:
				(void)set_unit_enable(arg1_val, arg2_val, 0, 0);
				break;
			case FDISABLE_UNIT:
				(void)set_unit_enable(arg1_val, arg2_val, 0, 1);
				break;
			case ENABLE_UNIT:
				(void)set_unit_enable(arg1_val, arg2_val, 1, 0);
				break;
			case FENABLE_UNIT:
				(void)set_unit_enable(arg1_val, arg2_val, 1, 1);
				break;
#ifdef DEBUG
			case GCACHE_INVALIDATE:
				loprintf("calling gcache_invalidate\n");
				gcache_invalidate();
				break;

			case GCACHE_CHECK_TAG_ADDR:
				loprintf("calling gcache_check_tag_addr\n");
				gcache_check_tag_addr();
				break;

			case GCACHE_CHECK_TAG_DATA:
				loprintf("calling gcache_check_tag_data\n");
				gcache_check_tag_data();
				break;

			case GCACHE_CHECK_DATA:
				loprintf("calling gcache_check_data\n");
				gcache_check_data();
				break;

			case GCACHE_CHECK:
				loprintf("calling gcache_check\n");
				gcache_check();
				break;
#endif
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

#ifdef NO_NIBLET
int do_niblet(int x)
{
        loprintf("Nilbet %d To be implemented\n",x);
}

#endif
