%{
#include <sys/types.h>

#define parse_YYMAXDEPTH	20

#include "exec.h"
#include "lex.h"
#include "cmd.h"
#include "pod.h"
#include "tlb.h"
#include "rtc.h"
#include "symbol.h"

#define LBYTE(caddr) \
	(char) ((*(int *) ((__psunsigned_t) (caddr) & ~3) << \
	  ((__psunsigned_t) (caddr) & 3) * 8) >> 24)

#define LBYTEU(caddr) \
	(u_char) ((*(uint *) ((__psunsigned_t) (caddr) & ~3) << \
	  ((__psunsigned_t) (caddr) & 3) * 8) >> 24)

extern hwreg_set_t	hwreg_hub;
extern hwreg_set_t	hwreg_c0;
extern hwreg_set_t	hwreg_router;
extern hwreg_set_t	hwreg_xbow;

#define SC(size, nargs)		((size) << 8 | (nargs))

void parse_push(parse_data_t *pd, __uint64_t value)
{
    if (pd->arg_ptr < PARSE_ARG_MAX)
	pd->arg_stack[pd->arg_ptr++] = value;
    else
	printf("Arg stack overflow\n");
}

__uint64_t parse_pop(parse_data_t *pd)
{
    i64		v;

    if (pd->arg_ptr == 0) {
	printf("Arg stack underflow\n");
	v = 0;
    } else
	v = pd->arg_stack[--pd->arg_ptr];

    return v;
}

void parse_push_string(parse_data_t *pd, char *s)
{
    int		i, len = strlen(s);
    __uint64_t	data;

    for (i = (len - 1) & ~7; i >= 0; i -= 8) {
	memcpy((char *) &data, s + i, 8);
	parse_push(pd, data);
    }

    parse_push(pd, len);
}

void parse_pop_string(parse_data_t *pd, char *buf)
{
    int		i, len = parse_pop(pd);
    __uint64_t	data;
    char       *s = buf;

    for (i = len; i > 0; i -= 8, s += 8) {
	data = parse_pop(pd);
	memcpy(s, (char *) &data, 8);
    }

    buf[len] = 0;
}
%}

%start command_line

/*
 * New command tokens must be added to
 *	1.  token list below
 *	2.  any_command rule below
 *	3.  array in cmd.c
 *	4.  implementation in exec.h and exec.c
 */

%token PX PD PO PB PR PF PA
%token LB LH LW LD LA
%token SB SH SW SD SDV SR SF
%token VR VW VX
%token REPEAT LOOP WHILE FOR IF DELAY SLEEP TIME ECHO
%token JUMP CALL RESET SOFTRESET NMI
%token HELP RESUME
%token INVAL FLUSH TLBC TLBR
%token DTAG ITAG STAG
%token DLINE ILINE SLINE
%token ADTAG AITAG ASTAG
%token ADLINE AILINE ASLINE SSCACHE SSTAG
%token GO HUBSDE RTRSDE CHKLINK BIST RBIST
%token DISABLE ENABLE TDISABLE
%token DIRINIT MEMINIT DIRTEST MEMTEST SANTEST
%token SLAVE
%token SEGS EXEC
%token WHY BTRACE RECONF CLEAR ERROR
%token NM
%token IOC3 JUNK ELSC
%token DISC PCFG NODE
%token CRB CRBX SC SCW SCR DIPS DBG PAS MODNIC MODULE PARTITION CPU DIS DMC
%token ROUTE FLASH FFLASH NIC RNIC CFG QUAL ECC IM
%token ERROR_DUMP EDUMP_BRI MAXERR RTAB RSTAT CREDITS VERSION DUMPSPOOL
%token MEMSET MEMCPY MEMCMP MEMSUM
%token SCANDIR DIRSTATE VERBOSE ALTREGS KDEBUG KERN_SYM RESET_DUMP
%token INITLOG INITALLLOGS CLEARLOG CLEARALLLOGS SETENV UNSETENV PRINTENV LOG SETPART
%token TESTXBOW TESTBRDG TESTCONF TESTPCI TESTSPIO TESTSDMA
%token FRU

/*
 * Special tokens
 */

%token OROR ANDAND LSHIFT RSHIFT INCR DECR
%token ISEQUAL NOTEQUAL LTEQUAL GTEQUAL FPPREF

%token MOD_HSPEC MOD_IO MOD_MSPEC MOD_UNCAC MOD_CAC
%token MOD_PHYS MOD_SIGNEXT MOD_NODE MOD_WIDGET MOD_BANK
%token MOD_DIRL MOD_DIRH MOD_PROT MOD_ECC

%token EOL BAD CONSTANT STRING GPR

/*
 * It would be nice if the union could be smaller because it is
 * currently responsible for much of the stack space usage.
 */

%union {
    __uint64_t		x;
    struct {
	hwreg_set_t    *regset;
	hwreg_t	       *hwreg;
    } v;
    char		s[LEX_STRING_MAX];
    struct {
	__uint64_t	reg;
	char		name[8];
    } gpr;
}

%left OROR
%left ANDAND
%left '|'
%left '^'
%left '&'
%left ISEQUAL NOTEQUAL
%left '<' '>' LTEQUAL GTEQUAL
%left LSHIFT RSHIFT
%left '+' '-'
%left '*' '/' '%'
%left '~' '!'

%%

command_line
	: command_seq command_end
	;

command_end
	: EOL			{ add(user_data, PARSE_OPER_END, 0); }
	;

command_seq
	: command
	| command ';' command_seq
	;

command
	:
	| '{' command_seq '}'
	| PX expr		{ add(user_data, exec_print, 16); }
	| PD expr		{ add(user_data, exec_print, 10); }
	| PO expr		{ add(user_data, exec_print, 8); }
	| PB expr		{ add(user_data, exec_print, 2); }
	| PR gpr		{ add(user_data, exec_printreg, ~0ULL); }
	| PR CONSTANT		{ add(user_data, exec_printreg, $2.x); }
	| PR string expr
		{
		    int			i;
		    hwreg_t	       *hwreg;

		    if ((i = cop0_lookup($2.s)) >= 0)
			add(user_data, exec_printcop0, 0x100 | i);
		    else if ((hwreg =
			      findreg(user_data, &hwreg_hub, $2.s, 1)) != 0)
			add(user_data, exec_phreg_value, (i64) hwreg);
		    else if ((hwreg =
			      findreg(user_data, &hwreg_xbow, $2.s, 1)) != 0)
			add(user_data, exec_pxreg_value, (i64) hwreg);
		    else if ((hwreg =
			      findreg(user_data, &hwreg_router, $2.s, 1)) != 0)
			add(user_data, exec_prreg_value, (i64) hwreg);
		    else {
		    unknown_regname:
			parse_yyerror("Unknown register name",
				      lex_data, user_data);
			YYABORT;
		    }
		}
	| PR string '~'
		{
		    hwreg_t	       *hwreg;

		    if ((hwreg =
			 findreg(user_data, &hwreg_hub, $2.s, 1)) != 0)
			add(user_data, exec_phreg_defl, (i64) hwreg);
		    else if ((hwreg =
			      findreg(user_data, &hwreg_xbow, $2.s, 1)) != 0)
			add(user_data, exec_pxreg_defl, (i64) hwreg);
		    else if ((hwreg =
			      findreg(user_data, &hwreg_router, $2.s, 1)) != 0)
			add(user_data, exec_prreg_defl, (i64) hwreg);
		    else
			goto unknown_regname;
		}
	| PR string
		{
		    int			i;
		    hwreg_t	       *hwreg;

		    if ((i = cop0_lookup($2.s)) >= 0)
			add(user_data, exec_printcop0, i);
		    else if ((hwreg =
			      findreg(user_data, &hwreg_hub, $2.s, 1)) != 0)
			add(user_data, exec_phreg_actual, (i64) hwreg);
		    else if ((hwreg =
			      findreg(user_data, &hwreg_xbow, $2.s, 1)) != 0)
			add(user_data, exec_pxreg_actual, (i64) hwreg);
		    else if ((hwreg = findreg(user_data,
					      &hwreg_router, $2.s, 1)) != 0) {
			parse_yyerror("Must specify register value to decode",
				      lex_data, user_data);
			YYABORT;
		    } else
			goto unknown_regname;
		}
	| PR MOD_NODE value string
		{
		    hwreg_t	       *hwreg;

		    if ((hwreg =
			 findreg(user_data, &hwreg_hub, $4.s, 1)) != 0)
			add(user_data, exec_phreg_remote, (i64) hwreg);
		    else if ((hwreg =
			      findreg(user_data, &hwreg_xbow, $4.s, 1)) != 0)
			add(user_data, exec_pxreg_remote, (i64) hwreg);
		    else
			goto unknown_regname;
		}
	| PR			{ add(user_data, exec_printregall, 0); }
	| PF value		{ add(user_data, exec_printcop1, 1); }
	| PF			{ add(user_data, exec_printcop1, 0); }
	| PA value value	{ add(user_data, exec_printaddr, 2); }
	| PA value		{ add(user_data, exec_printaddr, 1); }
	| LB value value	{ add(user_data, exec_load, SC(1, 2)); }
	| LB value		{ add(user_data, exec_load, SC(1, 1)); }
	| LH value value	{ add(user_data, exec_load, SC(2, 2)); }
	| LH value		{ add(user_data, exec_load, SC(2, 1)); }
	| LW value value	{ add(user_data, exec_load, SC(4, 2)); }
	| LW value		{ add(user_data, exec_load, SC(4, 1)); }
	| LD value value	{ add(user_data, exec_load, SC(8, 2)); }
	| LD value		{ add(user_data, exec_load, SC(8, 1)); }
	| LA value value	{ add(user_data, exec_load, SC(0, 2)); }
	| LA value		{ add(user_data, exec_load, SC(0, 1)); }
	| SB value value value	{ add(user_data, exec_store, SC(1, 3)); }
	| SB value value	{ add(user_data, exec_store, SC(1, 2)); }
	| SB value		{ add(user_data, exec_store, SC(1, 1)); }
	| SH value value value	{ add(user_data, exec_store, SC(2, 3)); }
	| SH value value	{ add(user_data, exec_store, SC(2, 2)); }
	| SH value		{ add(user_data, exec_store, SC(2, 1)); }
	| SW value value value	{ add(user_data, exec_store, SC(4, 3)); }
	| SW value value	{ add(user_data, exec_store, SC(4, 2)); }
	| SW value		{ add(user_data, exec_store, SC(4, 1)); }
	| SD value value value	{ add(user_data, exec_store, SC(8, 3)); }
	| SD value value	{ add(user_data, exec_store, SC(8, 2)); }
	| SD value		{ add(user_data, exec_store, SC(8, 1)); }
	| SDV value value	{ add(user_data, exec_sdv, 0); }
	| SR gpr expr		{ add(user_data, exec_setgpr, 0); }
	| SR string expr	{
				    int i;
				    if ((i = cop0_lookup($2.s)) < 0)
					goto unknown_regname;
				    add(user_data, exec_setcop0, i);
				}
	| SF value expr		{ add(user_data, exec_setcop1, ~0ULL); }
	| VR value vecadr	{ add(user_data, exec_vecr, 0); }
	| VW value vecadr expr	{ add(user_data, exec_vecw, 0); }
	| VX value vecadr expr	{ add(user_data, exec_vecx, 0); }
	| FPPREF CONSTANT '=' expr
				{ add(user_data, exec_setcop1, $2.x); }
	| gpr '=' expr		{ add(user_data, exec_setgpr, 0); }
	| string '=' expr	{
				    int i;
				    if ((i = cop0_lookup($1.s)) < 0)
					goto unknown_regname;
				    add(user_data, exec_setcop0, i);
				}
	| REPEAT expr		{
				    if ($2.x == 0) {
					parse_yyerror("Illegal repeat count",
						      lex_data, user_data);
					YYABORT;
				    }
				    add(user_data, PARSE_OPER_REPEAT, 0);
				}
				command
				{ add(user_data, PARSE_OPER_END, 0); }
	| LOOP
				{ add(user_data, PARSE_OPER_LOOP, 0); }
				command
				{ add(user_data, PARSE_OPER_END, 0); }
	| WHILE
				{ add(user_data, PARSE_OPER_WHILE, 0); }
				value
				{ add(user_data, PARSE_OPER_END, 0); }
				command
				{ add(user_data, PARSE_OPER_END, 0); }
	| FOR '(' command ';'
				{ add(user_data, PARSE_OPER_FOR, 0); }
				expr ';'
				{ add(user_data, PARSE_OPER_END, 0); }
				command
				{ add(user_data, PARSE_OPER_END, 0); }
				')' command
				{ add(user_data, PARSE_OPER_END, 0); }
	| IF
				{ add(user_data, PARSE_OPER_IF, 0); }
				value
				{ add(user_data, PARSE_OPER_END, 0); }
				command
				{ add(user_data, PARSE_OPER_END, 0); }
	| DELAY expr		{ add(user_data, exec_delay, 0); }
	| SLEEP expr		{ add(user_data, exec_sleep, 0); }
	| TIME
				{ add(user_data, PARSE_OPER_TIME, 0); }
				command
				{ add(user_data, PARSE_OPER_END, 0); }
	| ECHO push_string push_string push_string
				{ add(user_data, exec_echo, 3); }
	| ECHO push_string push_string
				{ add(user_data, exec_echo, 2); }
	| ECHO push_string	{ add(user_data, exec_echo, 1); }
	| JUMP value value value
				{ add(user_data, exec_jump, 3); }
	| JUMP value value	{ add(user_data, exec_jump, 2); }
	| JUMP value		{ add(user_data, exec_jump, 1); }
	| CALL value value value value value
				{ add(user_data, exec_call, 5); }
	| CALL value value value value
				{ add(user_data, exec_call, 4); }
	| CALL value value value
				{ add(user_data, exec_call, 3); }
	| CALL value value	{ add(user_data, exec_call, 2); }
	| CALL value		{ add(user_data, exec_call, 1); }
	| RESET			{ add(user_data, exec_reset, 0); }
	| SOFTRESET MOD_NODE expr
				{ add(user_data, exec_softreset, 0); }
	| NMI MOD_NODE value push_string
				{ add(user_data, exec_nmi, 2); }
	| NMI MOD_NODE value	{ add(user_data, exec_nmi, 1); }
	| HELP push_command	{ add(user_data, exec_help, 1); }
	| HELP			{ add(user_data, exec_help, 0); }
	| '?' push_command	{ add(user_data, exec_help, 1); }
	| '?'			{ add(user_data, exec_help, 0); }
	| RESUME		{ add(user_data, exec_resume, 0); }
	| INVAL push_string	{ add(user_data, exec_inval, 1); }
	| INVAL			{ add(user_data, exec_inval, 0); }
	| FLUSH			{ add(user_data, exec_flush, 0); }
	| TLBC expr		{ add(user_data, exec_tlbc, 1); }
	| TLBC			{ add(user_data, exec_tlbc, 0); }
	| TLBR expr		{ add(user_data, exec_tlbr, 1); }
	| TLBR			{ add(user_data, exec_tlbr, 0); }
	| DTAG expr		{ add(user_data, exec_dtag, 1); }
	| DTAG			{ add(user_data, exec_dtag, 0); }
	| ITAG expr		{ add(user_data, exec_itag, 1); }
	| ITAG			{ add(user_data, exec_itag, 0); }
	| STAG expr		{ add(user_data, exec_stag, 1); }
	| STAG			{ add(user_data, exec_stag, 0); }
	| DLINE expr		{ add(user_data, exec_dline, 0); }
	| ILINE expr		{ add(user_data, exec_iline, 0); }
	| SLINE expr		{ add(user_data, exec_sline, 0); }
	| ADTAG expr		{ add(user_data, exec_adtag, 0); }
	| AITAG expr		{ add(user_data, exec_aitag, 0); }
	| ASTAG expr		{ add(user_data, exec_astag, 0); }
	| ADLINE expr		{ add(user_data, exec_adline, 0); }
	| AILINE expr		{ add(user_data, exec_ailine, 0); }
	| ASLINE expr		{ add(user_data, exec_asline, 0); }
	| SSCACHE value value value
				{ add(user_data, exec_sscache, 0); }
	| SSTAG value value value value
				{ add(user_data, exec_sstag, 4); }
	| SSTAG value value value
				{ add(user_data, exec_sstag, 3); }
	| GO push_string	{ add(user_data, exec_go, 0); }
	| HUBSDE		{ add(user_data, exec_hubsde, 0); }
	| RTRSDE		{ add(user_data, exec_rtrsde, 0); }
	| CHKLINK		{ add(user_data, exec_chklink, 0); }
	| BIST push_string MOD_NODE value
				{ add(user_data, exec_bist, 0); }
	| BIST push_string	{ add(user_data, exec_bist, 1); }
	| RBIST push_string value
				{ add(user_data, exec_bist, 2); }
				/* 4=enable, 2=temp, 1=slice given */
	| ENABLE MOD_NODE value push_string
				{ add(user_data, exec_enable, 4 | 0 | 1); }
	| ENABLE MOD_NODE value
				{ add(user_data, exec_enable, 4 | 0 | 0); }
	| DISABLE MOD_NODE value push_string
				{ add(user_data, exec_enable, 0 | 0 | 1); }
	| DISABLE MOD_NODE value
				{ add(user_data, exec_enable, 0 | 0 | 0); }
	| TDISABLE MOD_NODE value push_string
				{ add(user_data, exec_enable, 0 | 2 | 1); }
	| TDISABLE MOD_NODE value
				{ add(user_data, exec_enable, 0 | 2 | 0); }
	| DIRINIT value value	{ add(user_data, exec_dirinit, 0); }
	| MEMINIT value value	{ add(user_data, exec_meminit, 0); }
	| DIRTEST value value	{ add(user_data, exec_dirtest, 0); }
	| MEMTEST value value	{ add(user_data, exec_memtest, 0); }
	| SANTEST value		{ add(user_data, exec_santest, 0); }
	| SLAVE			{ add(user_data, exec_slave, 0); }
	| SEGS value		{ add(user_data, exec_segs, 1); }
	| SEGS			{ add(user_data, exec_segs, 0); }
	| EXEC push_string value
				{ add(user_data, exec_exec, 2); }
	| EXEC push_string
				{ add(user_data, exec_exec, 1); }
	| EXEC
				{ add(user_data, exec_exec, 0); }
	| WHY			{ add(user_data, exec_why, 0); }
	| BTRACE value value	{ add(user_data, exec_btrace, 2); }
	| BTRACE		{ add(user_data, exec_btrace, 0); }
	| RECONF		{ add(user_data, exec_reconf, 0); }
	| CLEAR			{ add(user_data, exec_clear, 0); }
	| ERROR			{ add(user_data, exec_error, 0); }
	| IOC3			{ add(user_data, exec_ioc3, 0); }
	| JUNK			{ add(user_data, exec_junk, 0); }
	| ELSC			{ add(user_data, exec_elsc, 0); }
	| NM expr		{ add(user_data, exec_nm, 0); }
	| DISC			{ add(user_data, exec_disc, 0); }
	| PCFG MOD_NODE value push_string
				{ add(user_data, exec_pcfg, (0x2 | 0x1)); }
	| PCFG push_string	{ add(user_data, exec_pcfg, 0x2); }
	| PCFG MOD_NODE value	{ add(user_data, exec_pcfg, 0x1); }
	| PCFG			{ add(user_data, exec_pcfg, 0x0); }
	| NODE value value
				{ add(user_data, exec_node, 2); }
	| NODE expr		{ add(user_data, exec_node, 1); }
	| CRB MOD_NODE value	{ add(user_data, exec_crb, 1); }
	| CRB			{ add(user_data, exec_crb, 0); }
	| CRBX MOD_NODE value	{ add(user_data, exec_crbx, 1); }
	| CRBX			{ add(user_data, exec_crbx, 0); }
	| ROUTE value value	{ add(user_data, exec_route, 2); }
	| ROUTE			{ add(user_data, exec_route, 0); }
	| FLASH			{ add(user_data, parse_push, 0xbabababaUL); }
		value_list	{ add(user_data, exec_flash, 0); }
	| FFLASH 		{ add(user_data, parse_push, 0xbabababaUL); }
		value_list 	{ add(user_data, exec_flash, 1); }
	| NIC MOD_NODE expr	{ add(user_data, exec_nic, 1); }
	| NIC			{ add(user_data, exec_nic, 0); }
	| RNIC expr		{ add(user_data, exec_rnic, 1); }
	| RNIC			{ add(user_data, exec_rnic, 0); }
	| CFG MOD_NODE value	{ add(user_data, exec_cfg, 1); }
	| CFG			{ add(user_data, exec_cfg, 0); }
	| SC push_string	{ add(user_data, exec_sc, 1); }
	| SC			{ add(user_data, exec_sc, 0); }
	| SCW value value value	{ add(user_data, exec_scw, 3); }
	| SCW value value	{ add(user_data, exec_scw, 2); }
	| SCW value		{ add(user_data, exec_scw, 1); }
	| SCR value value	{ add(user_data, exec_scr, 2); }
	| SCR value		{ add(user_data, exec_scr, 1); }
	| DBG value value	{ add(user_data, exec_dbg, 2); }
	| DBG			{ add(user_data, exec_dbg, 0); }
	| PAS push_string
				{ add(user_data, exec_pas, 1); }
	| PAS			{ add(user_data, exec_pas, 0); }
	| MODNIC		{ add(user_data, exec_modnic, 0); }
	| MODULE		{ add(user_data, exec_module, 0); }
	| MODULE value		{ add(user_data, exec_module, 1); }
	| PARTITION		{ add(user_data, exec_partition, 0); }
	| PARTITION value	{ add(user_data, exec_partition, 1); }
	| DIPS			{ add(user_data, exec_dips, 0); }
	| CPU MOD_NODE value push_string
				{ add(user_data, exec_cpu, 2); }
	| CPU push_string	{ add(user_data, exec_cpu, 1); }
	| CPU			{ add(user_data, exec_cpu, 0); }
	| DIS value value	{ add(user_data, exec_dis, 2); }
	| DIS value		{ add(user_data, exec_dis, 1); }
	| DMC			{ add(user_data, exec_dmc, 0); }
	| DMC MOD_NODE value	{ add(user_data, exec_dmc, 1); }
	| QUAL push_string	{ add(user_data, exec_qual, 1); }
	| QUAL			{ add(user_data, exec_qual, 0); }
	| ECC value		{ add(user_data, exec_ecc, 1); }
	| ECC			{ add(user_data, exec_ecc, 0); }
	| IM value		{ add(user_data, exec_im, 1); }
	| IM			{ add(user_data, exec_im, 0); }
	| DUMPSPOOL MOD_NODE value push_string
				{ add(user_data, exec_dumpspool, 2); }
	| DUMPSPOOL		{ add(user_data, exec_dumpspool, 0); }
	| ERROR_DUMP		{ add(user_data, exec_error_dump, 0); }
	| EDUMP_BRI		{ add(user_data, exec_edump_bri, 0); }
	| EDUMP_BRI MOD_NODE value
				{ add(user_data, exec_edump_bri, 0x1); }
	| MAXERR expr		{ add(user_data, exec_maxerr, 1); }
	| RESET_DUMP		{ add(user_data, exec_reset_dump, 0); }
	| MAXERR		{ add(user_data, exec_maxerr, 0); }
	| RTAB expr		{ add(user_data, exec_rtab, 1); }
	| RTAB			{ add(user_data, exec_rtab, 0); }
	| RSTAT expr		{ add(user_data, exec_rstat, 0); }
	| CREDITS		{ add(user_data, exec_credits, 0); }
	| VERSION		{ add(user_data, exec_version, 0); }
	| MEMSET value value value
				{ add(user_data, exec_memop, 0); }
	| MEMCPY value value value
				{ add(user_data, exec_memop, 1); }
	| MEMCMP value value value
				{ add(user_data, exec_memop, 2); }
	| MEMSUM value value
				{ add(user_data, exec_memop, 3); }
	| SCANDIR value value
				{ add(user_data, exec_scandir, 2); }
	| SCANDIR value
				{ add(user_data, exec_scandir, 1); }
	| DIRSTATE value value value
				{ add(user_data, exec_dirstate, 3); }
	| DIRSTATE value value	{ add(user_data, exec_dirstate, 2); }
	| DIRSTATE value	{ add(user_data, exec_dirstate, 1); }
	| DIRSTATE		{ add(user_data, exec_dirstate, 0); }
	| VERBOSE value		{ add(user_data, exec_verbose, 1);  }
	| VERBOSE		{ add(user_data, exec_verbose, 0);  }
	| ALTREGS value		{ add(user_data, exec_altregs, 1);  }
	| ALTREGS		{ add(user_data, exec_altregs, 0);  }
	| KDEBUG value		{ add(user_data, exec_kdebug, 1);  }
	| KDEBUG		{ add(user_data, exec_kdebug, 0);  }
	| KERN_SYM		{ add(user_data, exec_kern_sym, 0); }
	| INITLOG MOD_NODE value
				{ add(user_data, exec_initlog, 4); }
	| INITLOG		{ add(user_data, exec_initlog, 0); }
	| INITALLLOGS		{ add(user_data, exec_initalllogs, 0); }
	| CLEARLOG MOD_NODE value
				{ add(user_data, exec_initlog, (4 | 1)); }
	| CLEARLOG		{ add(user_data, exec_initlog, 1); }
	| CLEARALLLOGS		{ add(user_data, exec_initalllogs, 1); }
				/* 4=node, 2=key, 1=string */
	| SETENV MOD_NODE value push_string push_string
				{ add(user_data, exec_setenv, 4 | 2 | 1); }
	| SETENV MOD_NODE value push_string
				{ add(user_data, exec_setenv, 4 | 2 | 0); }
	| SETENV push_string push_string
				{ add(user_data, exec_setenv, 0 | 2 | 1); }
	| SETENV push_string
				{ add(user_data, exec_setenv, 0 | 2 | 0); }
	| UNSETENV MOD_NODE value push_string
				{ add(user_data, exec_unsetenv, 4 | 2 | 0); }
	| UNSETENV push_string	{ add(user_data, exec_unsetenv, 0 | 2 | 0); }
	| PRINTENV MOD_NODE value push_string
				{ add(user_data, exec_printenv, 4 | 2 | 0); }
	| PRINTENV MOD_NODE value
				{ add(user_data, exec_printenv, 4 | 0 | 0); }
	| PRINTENV push_string	{ add(user_data, exec_printenv, 0 | 2 | 0); }
	| PRINTENV		{ add(user_data, exec_printenv, 0 | 0 | 0); }
				/* 8=reverse, 4=node, 2=skip, 1=count */
	| LOG MOD_NODE push_constant push_constant push_constant
				{ add(user_data, exec_log, 4 | 2 | 1); }
	| LOG MOD_NODE push_constant push_constant
				{ add(user_data, exec_log, 4 | 2 | 0); }
	| LOG MOD_NODE push_constant
				{ add(user_data, exec_log, 4 | 0 | 0); }
	| LOG push_constant push_constant
				{ add(user_data, exec_log, 0 | 2 | 1); }
	| LOG push_constant
				{ add(user_data, exec_log, 0 | 2 | 0); }
	| LOG
				{ add(user_data, exec_log, 0 | 0 | 0); }
	| FRU value
				{ add(user_data, exec_fru, 0); }
	| SETPART value		 { add(user_data, exec_setpart, 1); }
	| TESTXBOW
				{ add(user_data, exec_xbowtest, 0); }
	| TESTXBOW push_string
				{ add(user_data, exec_xbowtest, 1); }
	| TESTXBOW push_string push_string
				{ add(user_data, exec_xbowtest, 2); }
	| TESTSPIO
				{ add(user_data, exec_Diag, (SRL_PIO << 4) | 0); }
	| TESTSPIO push_string
				{ add(user_data, exec_Diag, (SRL_PIO << 4) | 1); }
	| TESTSPIO push_string push_string
				{ add(user_data, exec_Diag, (SRL_PIO << 4) | 2); }
	| TESTSPIO push_string push_string push_string
				{ add(user_data, exec_Diag, (SRL_PIO << 4) | 3); }
	| TESTSPIO push_string push_string push_string push_string
				{ add(user_data, exec_Diag, (SRL_PIO << 4) | 4); }
	| TESTSDMA
				{ add(user_data, exec_Diag, (SRL_DMA << 4) | 0); }
	| TESTSDMA push_string
				{ add(user_data, exec_Diag, (SRL_DMA << 4) | 1); }
	| TESTSDMA push_string push_string
				{ add(user_data, exec_Diag, (SRL_DMA << 4) | 2); }
	| TESTSDMA push_string push_string push_string
				{ add(user_data, exec_Diag, (SRL_DMA << 4) | 3); }
	| TESTSDMA push_string push_string push_string push_string
				{ add(user_data, exec_Diag, (SRL_DMA << 4) | 4); }
	| TESTBRDG
				{ add(user_data, exec_Diag, (DIAG_BRI << 4) | 0); }
	| TESTBRDG push_string
				{ add(user_data, exec_Diag, (DIAG_BRI << 4) | 1); }
	| TESTBRDG push_string push_string
				{ add(user_data, exec_Diag, (DIAG_BRI << 4) | 2); }
	| TESTBRDG push_string push_string push_string
				{ add(user_data, exec_Diag, (DIAG_BRI << 4) | 3); }
	| TESTCONF
				{ add(user_data, exec_Diag, (DIAG_IO6 << 4) | 0); }
	| TESTCONF push_string
				{ add(user_data, exec_Diag, (DIAG_IO6 << 4) | 1); }
	| TESTCONF push_string push_string
				{ add(user_data, exec_Diag, (DIAG_IO6 << 4) | 2); }
	| TESTCONF push_string push_string push_string
				{ add(user_data, exec_Diag, (DIAG_IO6 << 4) | 3); }
	| TESTPCI
				{ add(user_data, exec_Diag, (DIAG_PCI << 4) | 0); }
	| TESTPCI push_string
				{ add(user_data, exec_Diag, (DIAG_PCI << 4) | 1); }
	| TESTPCI push_string push_string
				{ add(user_data, exec_Diag, (DIAG_PCI << 4) | 2); }
	| TESTPCI push_string push_string push_string
				{ add(user_data, exec_Diag, (DIAG_PCI << 4) | 3); }
	| TESTPCI push_string push_string push_string push_string
				{ add(user_data, exec_Diag, (DIAG_PCI << 4) | 4); }
	;

gpr
	: '$' value
	| GPR			{ add(user_data, parse_push, $1.gpr.reg); }
	;

value
	: CONSTANT		{ add(user_data, parse_push, $1.x); }
	| gpr			{ add(user_data, exec_getgpr, 0); }
	| FPPREF value		{ add(user_data, exec_getcop1, 0); }
	| string
		{
		    int			indx;
		    syminfo_t		syminfo;

		    syminfo.sym_name = $1.s;

		    if (($$.v.hwreg =
			 findreg(user_data, &hwreg_hub, $1.s, 0)) != 0) {
			$$.v.regset = &hwreg_hub;
			add(user_data, parse_push,
			    $$.v.hwreg->address | 0x9200000001000000ULL);
		    } else if (($$.v.hwreg =
			 findreg(user_data, &hwreg_xbow, $1.s, 0)) != 0) {
			$$.v.regset = &hwreg_hub;
			add(user_data, parse_push,
			    $$.v.hwreg->address | 0x9200000000000000ULL);
		    } else if ((indx = cop0_lookup($1.s)) >= 0)
			add(user_data, exec_getcop0, indx);
		    else if ((indx = symbol_lookup_name(&syminfo)) >= 0)
			add(user_data, parse_push, (i64) syminfo.sym_addr);
		    else {
			parse_yyerror("Unknown register name or symbol",
				      lex_data, user_data);
			YYABORT;
		    }
		}
	| '(' expr ')'
	| MOD_HSPEC value	{ add(user_data, exec_modt, 0x90ULL); }
	| MOD_IO value		{ add(user_data, exec_modt, 0x92ULL); }
	| MOD_MSPEC value	{ add(user_data, exec_modt, 0x94ULL); }
	| MOD_UNCAC value	{ add(user_data, exec_modt, 0x96ULL); }
	| MOD_CAC value		{ add(user_data, exec_modt, 0xa8ULL); }
	| MOD_PHYS value	{ add(user_data, exec_modt, 0x00ULL); }
	| MOD_SIGNEXT value	{ add(user_data, exec_modt, ~0ULL); }
	| MOD_NODE value value	{ add(user_data, exec_modn, 0); }
	| MOD_WIDGET value value
				{ add(user_data, exec_modw, 0); }
	| MOD_BANK value value	{ add(user_data, exec_modb, 0); }
	| MOD_DIRL value	{ add(user_data, exec_modd, 0); }
	| MOD_DIRH value	{ add(user_data, exec_modd, 1); }
	| MOD_PROT value value	{ add(user_data, exec_modd, 2); }
	| MOD_ECC value		{ add(user_data, exec_modd, 3); }
	;

value_list
	: value
	| value value_list
	;

vecadr
	: CONSTANT		{ add(user_data, parse_push, $1.x); }
	| string
		{
		    i64 vaddr;
		    if ((vaddr = vecadr(user_data, $1.s, 1)) == ~0ULL)
			YYABORT;
		    else
			add(user_data, parse_push, vaddr);
		}
	;

expr
	: value
	| '~' expr		{ add(user_data, exec_arith, $1.x | 0x10000); }
	| '*' expr %prec '~'	{ add(user_data, exec_arith, 'i'  | 0x10000); }
	| LD value		{ add(user_data, exec_arith, $1.x | 0x10000); }
	| LW value		{ add(user_data, exec_arith, $1.x | 0x10000); }
	| LH value		{ add(user_data, exec_arith, $1.x | 0x10000); }
	| LB value		{ add(user_data, exec_arith, $1.x | 0x10000); }
	| '!' expr %prec '~'	{ add(user_data, exec_arith, $1.x | 0x10000); }
	| '-' expr %prec '~'	{ add(user_data, exec_arith, 'n'  | 0x10000); }
	| '+' expr %prec '~'
	| expr '+' expr		{ add(user_data, exec_arith, $2.x); }
	| expr '-' expr		{ add(user_data, exec_arith, $2.x); }
	| expr '*' expr		{ add(user_data, exec_arith, $2.x); }
	| expr '/' expr		{ add(user_data, exec_arith, $2.x); }
	| expr '%' expr		{ add(user_data, exec_arith, $2.x); }
	| expr '|' expr		{ add(user_data, exec_arith, $2.x); }
	| expr '&' expr		{ add(user_data, exec_arith, $2.x); }
	| expr '^' expr		{ add(user_data, exec_arith, $2.x); }
	| expr LSHIFT expr	{ add(user_data, exec_arith, $2.x); }
	| expr RSHIFT expr	{ add(user_data, exec_arith, $2.x); }
	| expr ISEQUAL expr	{ add(user_data, exec_arith, $2.x); }
	| expr NOTEQUAL expr	{ add(user_data, exec_arith, $2.x); }
	| expr '<' expr		{ add(user_data, exec_arith, $2.x); }
	| expr '>' expr		{ add(user_data, exec_arith, $2.x); }
	| expr LTEQUAL expr	{ add(user_data, exec_arith, $2.x); }
	| expr GTEQUAL expr	{ add(user_data, exec_arith, $2.x); }
	| expr ANDAND expr	{ add(user_data, exec_arith, $2.x); }
	| expr OROR expr	{ add(user_data, exec_arith, $2.x); }
	;

push_string
	: string		{ add_push_string(user_data, $1.s); }
	| GPR			{ add_push_string(user_data, $1.gpr.name); }
	;

string
	: STRING '<' CONSTANT ':' CONSTANT '>'
		{ sprintf($$.s, "%s<%02d:%02d>", $1.s, $3.x, $5.x); }
	| STRING '<' CONSTANT '>'
		{ sprintf($$.s, "%s<%02d>", $1.s, $3.x); }
	| STRING		{ strcpy($$.s, $1.s); }
	;

push_command
	: any_command	{
			    add_push_string(user_data,
					    cmd_token2cmd($1.x)->name);
			}

push_constant
	: CONSTANT		{ add(user_data, parse_push, $1.x); }
	;

any_command
	: PX | PD | PO | PB | PR | PF | PA
	| LB | LH | LW | LD | LA
	| SB | SH | SW | SD | SDV | SR | SF
	| VR | VW | VX
	| REPEAT | LOOP | WHILE | FOR | IF | DELAY | SLEEP | TIME | ECHO
	| JUMP | CALL | RESET | SOFTRESET | NMI
	| HELP | RESUME
	| INVAL | FLUSH | TLBC | TLBR
	| DTAG | ITAG | STAG
	| DLINE | ILINE | SLINE
	| ADTAG | AITAG | ASTAG
	| ADLINE | AILINE | ASLINE
	| GO | HUBSDE | RTRSDE | CHKLINK | BIST | RBIST
	| DISABLE | ENABLE | TDISABLE
	| DIRINIT | MEMINIT | DIRTEST | MEMTEST | SANTEST
	| SLAVE
	| SEGS | EXEC
	| WHY | BTRACE | RECONF | CLEAR | ERROR
	| NM
	| IOC3 | JUNK | ELSC
	| DISC | PCFG | NODE | CRB | CRBX
	| SC | SCW | SCR | DIPS | DBG | PAS | MODNIC | MODULE | PARTITION
	| CPU | DIS | DMC
	| ROUTE | FLASH | FFLASH | NIC | RNIC | QUAL | ECC | IM
	| ERROR_DUMP | EDUMP_BRI | MAXERR | RTAB | RSTAT | CREDITS | VERSION
	| DUMPSPOOL | MEMSET | MEMCPY | MEMCMP | MEMSUM
	| SCANDIR | DIRSTATE | VERBOSE | ALTREGS | KDEBUG | KERN_SYM
	| RESET_DUMP
	| INITLOG | INITALLLOGS | SETENV | UNSETENV | PRINTENV | LOG
	| SETPART
	| TESTXBOW | TESTBRDG | TESTCONF | TESTPCI | TESTSPIO | TESTSDMA
	| FRU
	;

%%

void parse_yyerror(const char *s, void *lex_data, void *user_data)
{
    lex_data_t	       *ld	= lex_data;
    parse_data_t       *pd	= user_data;
    char	       *t;
    int			i;
    cmd_t	       *cmd;

    /*
     * Try to be helpful in the particular case where the entire
     * command line consists of a single valid token and a syntax
     * error resulted.  This must mean there was a missing argument.
     */

    if ((cmd = cmd_name2cmd(ld->buf)) != 0)
	printf(" Argument required for %s\n", cmd->help);
    else {
	for (i = 0; i < pd->prompt_col; i++)
	    putchar(' ');

	for (t = ld->buf; t < ld->last; t++)
	    putchar(' ');

	if (ld->word_too_long)
	    s = "String too long.";

	printf("^ %s\n", s);
    }
}

static void add(parse_data_t *pd, void (*func)(), i64 arg)
{
    if (pd->oper_count < PARSE_OPER_MAX) {
	parse_oper_t *op = &pd->oper[pd->oper_count++];

	op->func = func;
	op->arg = arg;
    }
}

static void add_push_string(parse_data_t *pd, char *s)
{
    int			i, len = strlen(s);

    for (i = (len - 1) & ~7; i >= 0; i -= 8) {
	__uint64_t	data;

	memcpy((char *) &data, s + i, 8);
	add(pd, parse_push, data);
    }

    add(pd, parse_push, (i64) len);
}

static hwreg_t *findreg(parse_data_t *pd, hwreg_set_t *regset,
			char *name, int list_if_ambiguous)
{
    char		caps[LEX_STRING_MAX];
    hwreg_t	       *hwreg;
    int			len	= strlen(name);
    int			i, found;
    int			lines	= 1;

    if ((hwreg = hwreg_lookup_name(regset, name, 1)) != 0) {
	pd->last_regset = regset;
	pd->last_hwreg	= hwreg;
	return hwreg;
    }

    if (! list_if_ambiguous)
	return 0;

    hwreg_upcase(caps, name);

    found = 0;

    for (i = 0; regset->regs[i].nameoff; i++)
	if (strncmp(caps,
		    regset->strtab + regset->regs[i].nameoff,
		    len) == 0) {
	    if (! more(&lines, 23))
		break;
	    if (! found) {
		printf("Ambiguous register name; choose one of:\n");
		found = 1;
	    }
	    printf("    %s\n", regset->strtab + regset->regs[i].nameoff);
	}

    return 0;
}

static i64 vecadr(parse_data_t *pd, char *name, int list_if_ambiguous)
{
    hwreg_set_t	       *regset;
    hwreg_t	       *hwreg;

    if (toupper(name[0]) == 'R' &&
	toupper(name[1]) == 'R' && name[2] == '_')
	regset = &hwreg_router;
    else if (toupper(name[0]) == 'N' &&
	     toupper(name[1]) == 'I' && name[2] == '_')
	regset = &hwreg_hub;
    else {
	printf("Illegal vector register: %s; "
	       "must be RR_xxx or NI_xxx.\n", name);
	return ~0ULL;
    }

    if ((hwreg = findreg(pd, regset, name, 1)) == 0) {
	printf("Unknown register name: %s\n", name);
	return ~0ULL;
    }

    return hwreg->address & 0x1fffff;
}

static int parse_execute(parse_data_t *pd, int skip)
{
    rtc_time_t		tm;
    i64			count;
    parse_oper_t       *op, *place, *place2;
    void	      (*new_func)();

    if (kbintr(&pd->next))
	return -1;

    while ((op = pd->oper_ptr++)->func != PARSE_OPER_END)
	switch ((i64) op->func) {
	case (i64) PARSE_OPER_WHILE:
	    place = pd->oper_ptr;
	    while (1) {
		if (parse_execute(pd, skip) < 0)
		    return -1;
		if (parse_pop(pd) == 0) {
		    if (parse_execute(pd, 1) < 0)
			return -1;
		    break;
		}
		if (parse_execute(pd, 0) < 0)
		    return -1;
		pd->oper_ptr = place;
	    }
	    break;
	case (i64) PARSE_OPER_FOR:
	    place = pd->oper_ptr;
	    while (1) {
		if (parse_execute(pd, skip) < 0)
		    return -1;
		if (parse_pop(pd) == 0) {
		    if (parse_execute(pd, 1) < 0)
			return -1;
		    if (parse_execute(pd, 1) < 0)
			return -1;
		    break;
		}
		place2 = pd->oper_ptr;
		if (parse_execute(pd, 1) < 0)
		    return -1;
		if (parse_execute(pd, 0) < 0)
		    return -1;
		pd->oper_ptr = place2;
		if (parse_execute(pd, 0) < 0)
		    return -1;
		pd->oper_ptr = place;
	    }
	    break;
	case (i64) PARSE_OPER_IF:
	    if (parse_execute(pd, 0) < 0)
		return -1;
	    if (parse_execute(pd, parse_pop(pd) == 0) < 0)
		return -1;
	    break;
	case (i64) PARSE_OPER_LOOP:
	    place = pd->oper_ptr;
	    while (! skip) {
		if (parse_execute(pd, skip) < 0)
		    return -1;
		pd->oper_ptr = place;
	    }
	    break;
	case (i64) PARSE_OPER_TIME:
	    if (skip) {
		if (parse_execute(pd, 1) < 0)
		    return -1;
	    } else {
		tm = rtc_time();
		if (parse_execute(pd, 0) < 0)
		    return -1;
		tm = rtc_time() - tm;
		if (tm < 1000)
		    printf("   TIME: %lld usec\n", tm);
		else if (tm < 1000000)
		    printf("   TIME: %lld.%03lld msec\n",
			   tm / 1000, tm % 1000);
		else
		    printf("   TIME: %lld.%06lld sec\n",
			   tm / 1000000, tm % 1000000);
	    }
	    break;
	case (i64) PARSE_OPER_REPEAT:
	    count = parse_pop(pd);
	    place = pd->oper_ptr;
	    while (! skip && count-- > 1) {
		if (parse_execute(pd, skip) < 0)
		    return -1;
		pd->oper_ptr = place;
	    }
	    if (parse_execute(pd, skip) < 0)
		return -1;
	    break;
	default:
	    if (! skip) {
		new_func = tlb_to_prom_addr(op->func);
		(*new_func)(pd, op->arg);
		break;
	    }
	}

    return 0;
}

void parse(parse_data_t *pd, char *s, int prompt_col)
{
    lex_data_t		ld;

    pd->oper_count  = 0;
    pd->last_regset = 0;
    pd->last_hwreg  = 0;

    parse_yylex_init(&ld, s);

    pd->arg_ptr	   = 0;
    pd->prompt_col = prompt_col;

    if (parse_yyparse(&ld, pd, 0) == 0) {
	if (pd->oper_count == PARSE_OPER_MAX)
	    parse_yyerror("Too many commands", &ld, pd);
	else {
	    pd->oper_ptr = &pd->oper[0];
	    parse_execute(pd, 0);
	}
    }
}
