#include <sys/types.h>
#include <sys/sbd.h>
#include "biendian.h"
#include "pod.h"
#include "ip19prom.h"
#include "pod_failure.h"

extern void loprintf(char *, ...);
extern void loputs(void (*putc)(char), char *, int);
extern void loputchar(char);
#ifdef BRINGUP
extern sload(int, struct flag_struct);
extern int pon_sload(int, struct flag_struct*);
#else
extern sload(int, int *, struct flag_struct);
extern int pon_sload(int, int*, struct flag_struct*);
#endif
int *pod_parse(int *, struct reg_struct *, int, int, struct flag_struct *);
int lo_strcmp(int *, char *);
uint lo_atoh(int *);
int lo_ishex(int);

extern memory(int, int, uint, uint, uint, int);
extern _register(int, int *, int, struct reg_struct *);
extern info();
extern jump_addr(uint, uint, uint, struct flag_struct *);
extern prom_slave();
extern pon_flush_scache();
extern pon_flush_dcache();
extern pon_invalidate_icache();
extern void pod_loop(char *, int);
extern int set_unit_enable(uint, uint, uint, uint);

extern epcerrc();

const struct cmd_table {
	int token;
	int args;
	char *name;
	char *help;
} commands[] = {
{WRITE_BYTE,	2,	"wb",		"Write byte:       wb ADDRESS NUMBER"},
{WRITE_HALF,	2,	"wh",		"Write halfword:   wh ADDRESS NUMBER"},
{WRITE_WORD,	2,	"ww",		"Write word:       ww ADDRESS NUMBER"},
{WRITE_DOUBLE,	2,	"wd",		"Write double:     wd ADDRESS NUMBER"},
{DISP_BYTE,	1,	"db",		"Display byte:     db ADDRESS"},
{DISP_HALF,	1,	"dh",		"Display halfword: dh ADDRESS"},
{DISP_WORD,	1,	"dw",		"Display word:     dw ADDRESS"},
{DISP_DOUBLE,	1,	"dd",		"Display double:   dd ADDRESS"},
{SERIAL_LOAD,	0,	"sload",	"Serial download:  sload"},
{SERIAL_RUN,	0,	"srun",		"Download and run: srun"},
#if 0
{LOAD_IOPROM,	1,	"pload",	"IO prom download: pload [baudrate]"},
#endif
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
{TEST_SCACHE,	1,	"scache",	"Test 2ary cache:  scache COUNT"},
{DISP_MC3,	1,	"dmc",		"Disp mem bd regs: dmc SLOT"},
{DISP_IO4,	1,	"dio",		"Disp io brd regs: dio SLOT"},
{DECODE_ECC,	0,	"ecc",		"Display ecc info: ecc"},
{RESET_SYSTEM,	0,	"reset",	"Reset the system: reset"},
{PRM_CRD,	0,	"credits",	""},
{BIG_ENDIAN,	1,	"big",		""},
{DISP_HELP,	0,	"?",		""},
{DISP_HELP,	0,	"help",		""},
{NIBLET,	1,	"niblet",	"Run Niblet:       niblet <0 - 9>"},
{POD_RESUME, 	0,	"resume",	""},
{SEND_INT,	3,	"si",		"Send interrupt:   si SLOT CPU LEVEL"},
{FLUSHI,	0,	"flushi",	""},
{FLUSHD,	0,	"flushd",	""},
{FLUSHS,	0,	"flushs",	""},
{FLUSHT,	0,	"flusht",	"Clear TLB:        flusht"},
{TLB_DUMP,	1,	"td",		"Dump TLB:         td <index | l | h | all>"},
{CLEAR_STATE,	0,	"clear",	"Clear State:      clear"},
{DECODE_ADDR,	1,	"decode",	"Decode Address:   decode PHYSADDR"},
{SET_DE,	1,	"de",		"Set test DE bit:  de <0|1> (0 = cache errors enabled for tests)"},
{WALK_MEM,	3,	"walk",		"Walk a bit:       walk <loaddr> <hiaddr> <cont on fails>"},
{ITAG_DUMP,	1,	"itag",		"Dump icache tag:  itag <line_addr>"},
{DTAG_DUMP,	1,	"dtag",		"Dump dcache tag:  dtag <line_addr>"},
{STAG_DUMP,	1,	"stag",		"Dump scache tag:  stag <line_addr>"},
{SLAVE_MODE,	0,	"slave",	"Goto slave mode:  slave"},
{WRITE_EXTENDED,3,	"wx",		"Write Extended:   wx BLOC OFF VALUE"},
{DISP_EXTENDED, 2,  	"dx",		"Display Extended: dx BLOC OFF"},
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
{DISP_MPCONF,	1,	"dmpc",		"Display MPCONF:   dmpc VPID"},
{FIX_CHECKSUM,	0,	"fixcksum",	"Fix EAROM cksum:  fix"},
{DISP_CHECKSUM,	0,	"cksum",	"Show EAROM cksum: cksum"},
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

void skipwhite(src)
int **src;
{
	while (**src == ' ' || **src == '\t')
		(*src)++;
}

void parse_error() {
	loprintf("*** POD syntax error.\n");
}

void copystr(target, src)
int **target, **src;
{
	while (**src != ' ' && **src != '\t' && **src != 0 && **src != ';' 
								&& **src != ')')
		*((*target)++) = *((*src)++);
}


int get_arg(int *string, uint *value, int **bufp) {
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
#ifdef PDEBUG
loprintf("end of line: Right paren.\n");
#endif
		(*bufp)++;
		return 1;
	}

	if (!(**bufp)) {
#ifdef PDEBUG
loprintf("end of line: NULL\n");
#endif
		return 1;
	}

	if (**bufp == ';') {
#ifdef PDEBUG
loprintf("Semicolon.\n");
#endif
		(*bufp)++;
	}

#ifdef PDEBUG
loprintf("Interior of line\n");
#endif
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
	uint arg1_val;
	uint arg2_val;
	uint arg3_val;
	int line_end;
	int cmd_token;
        int num_args;
	int i;
	int x;

	extern prom_start();

#ifdef PDEBUG
	loprintf("Sloop = %d\n", sloop);
#endif /* PDEBUG */

	if (parse_level >= MAX_NEST) {
		loprintf("*** Too many levels of loops/parentheses.\n");
		return (int *)NULL;
	}

#ifdef PDEBUG
loprintf("Parser level %d\n", parse_level);
#endif

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
#ifdef PDEBUG
loprintf("Command == %p, token == %d, args = %d\n", cmd, cmd_token, num_args);
#endif

		if (num_args > 0) {
			if (!get_arg(argv1, &arg1_val, &bufp))
				return (int *)NULL;
#ifdef PDEBUG
loprintf("Argv1 = %p\n", argv1);
#endif
		}

		if (num_args > 1) {
			if (!get_arg(argv2, &arg2_val, &bufp))
				return (int *)NULL;
#ifdef PDEBUG
loprintf("Argv2 = %p\n", argv2);
#endif
		}

		if (num_args > 2) {
			if (!get_arg(argv3, &arg3_val, &bufp))
				return (int *)NULL;
#ifdef PDEBUG
loprintf("Argv2 = %p\n", argv3);
#endif
		}
		line_end = check_eol(&bufp);

		if ((flags->selected != 0xff) &&
					(flags->slice != flags->selected) &&
					(cmd_token != POD_SELECT)) {
			loprintf("*** Not selected\n");
			return; 
		}

		do {
		    switch (cmd_token) {
			case POD_SELECT:
				flags->selected = arg1_val;
				loprintf("Selected slice %b\n", arg1_val);
				break;
			case SERIAL_LOAD:
			case SERIAL_RUN:
				if (!flags->mem) {
					loprintf(
"Your stack must be in memory to download.  Use the 'gm' command.\n");
				} else {
					pon_sload(cmd_token, flags);
				}
				break;

			case WRITE_BYTE:
				memory(WRITE, BYTE, arg1_val, 0, arg2_val,
							parse_level);
				break;
			case WRITE_HALF:
				memory(WRITE, HALF, arg1_val, 0, arg2_val,
							parse_level);
				break;
			case WRITE_WORD:
				memory(WRITE, WORD, arg1_val, 0, arg2_val,
							parse_level);
				break;
			case WRITE_DOUBLE:
				memory(WRITE, DOUBLE, arg1_val, 0, arg2_val,
							parse_level);
				break;
			
			case WRITE_EXTENDED:
				u64sw(arg1_val, arg2_val, arg3_val);
				break;
			case DISP_BYTE:
				memory(READ, BYTE, arg1_val, 0, 0,
							parse_level);
				break;
			case DISP_HALF:
				memory(READ, HALF, arg1_val, 0, 0,
							parse_level);
				break;
			case DISP_WORD:
				memory(READ, WORD, arg1_val, 0, 0,
							parse_level);
				break;
			case DISP_DOUBLE:
				memory(READ, DOUBLE, arg1_val, 0, 0,
							parse_level);
				break;

			case DISP_EXTENDED:
				loprintf("Bloc %x Offset %x: %x\n",
					 arg1_val, arg2_val, 
					 u64lw(arg1_val, arg2_val));
				break;
			
			case TEST_SCACHE:
				pod_check_scache0(arg1_val, flags->de);
				break;
#ifndef NO_NIBLET
			case NIBLET:
				if (!flags->mem) {
					loprintf(
"Your stack must be in memory to run Niblet.  Use the 'gm' command.\n");
				} else {
					if (lo_strcmp(argv1, "all")) {
						do_niblet(arg1_val);
					} else {
						for (i = 0; i < 10; i++) {
							loprintf(" -- Test #%d --\n",
									i);
							x = do_niblet(i);
							if (x)
								break;
						}
						if (!x)
						    loprintf("All tests passed.\n");
						else
						    loprintf("*** Test %d FAILED\n",
								i);
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
			case SET_DE:
				if (arg1_val != 1 && arg1_val != 0) {
					loprintf("*** DE must be 0 or 1.");
					break;
				}
				flags->de = arg1_val;
				break;
			case BIG_ENDIAN:
				set_endian(arg1_val);
				break;
			case RESET_SYSTEM:
				reset_system();
				break;
			case DISP_REG:
				_register(READ, argv1, 0, gprs);
				break;
			case DISP_CONF:
				conf_register(READ, arg1_val, arg2_val, 0, 0,
							parse_level);
				break;
			case WRITE_CONF:
				conf_register(WRITE, arg1_val, arg2_val, 0,
							arg3_val, parse_level);
				break;
			case DISP_MC3:
				dump_mc3(arg1_val);
				break;
			case DISP_IO4:
				dump_io4(arg1_val);
				break;
			case DISP_EVCONFIG:
				dump_evcfg(argv1, arg1_val);
				break;
			case DISP_MPCONF:
				dump_mpconf(argv1, arg1_val);
				break;
			case POD_RESUME:
				pod_resume();
				break;
			case TLB_DUMP:
				tlb_dump(argv1, arg1_val);
				break;
			case CLEAR_STATE:
				clear_ip19_state();
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
			case WALK_MEM:
				pod_walk(arg1_val, arg2_val, arg3_val, 1);
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
#ifdef PDEBUG
loprintf("  In sloop: Parser level %d\n", parse_level);
#endif
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
			case FIX_CHECKSUM:
				fix_cksum();
				break;
			case DISP_CHECKSUM:
				disp_cksum();
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
#ifdef PDEBUG
loprintf("  In loop: Parser level %d\n", parse_level);
#endif
					if (ip == (int *)NULL)
						return ip;
				}
				bufp = ip;
				line_end = check_eol(&bufp);
				/* Recheck the end of line because we made
					recursive parser calls in the switch */
				break;
			case LEFT_PAREN:
#ifdef PDEBUG
loprintf("Left paren.\n");
#endif
				bufp = pod_parse(bufp, gprs, parse_level+1, 0,
						 flags);
#ifdef PDEBUG
loprintf("  Parser level %d\n", parse_level);
#endif

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
			case PRM_CRD:
				prm_crd(flags);
				break;
			case FLUSHT:
				flush_tlb();
				break;
			case FLUSHD:
				call_asm(pon_flush_dcache, 0);
				break;
			case FLUSHS:
				call_asm(pon_flush_scache, 0);
				break;
			case FLUSHI:
				call_asm(pon_invalidate_icache, 0);
				break;
			case DTAG_DUMP:
			case STAG_DUMP:
			case ITAG_DUMP:
				dump_tag(cmd_token, arg1_val);
				break;
			case DECODE_ECC:	
				pod_ecc();
				break;
			case TEST_MEM:
				mem_test(arg1_val, arg2_val, flags);
				break;
			case SLAVE_MODE:
				jump_addr((uint)prom_slave, 0, 0, flags);
				break;
			case JUMP_ADDR:
			  arg2_val = 0;
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
			case DISP_HELP:
				loprintf("\
All numerical inputs should be in hex with or without 0x preceding them.\n");
				loprintf("\
Commands may be separated by semicolons, and loops may be nested.\n");
				x = 2;
				for (i = 0; (commands[i].token != NULL); i++) {
					if (get_char(commands[i].help)) {
						loprintf("     %s\n",
							commands[i].help);
						x++;
					}
					if (!(x % 23) && !parse_level) {
						loprintf("--MORE--");
						pod_getc();
						loprintf("\r        \r");
					}
				}
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

uint lo_atoh(int *cp)
{
	register unsigned i;

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
