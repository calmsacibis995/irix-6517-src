#include <sys/types.h>

#include <hwreg.h>

#include "cmd.h"
#include "lex.h"
#include "libc.h"

/*
 * New tokens must be added to
 *	1.  parse.y token list
 *	2.  any_command rule below
 *	3.  cmd.c array below
 */

cmd_t cmds[] = {
 { 0,	       0,
   "Commands may be separated by semicolons, grouped with\n"
   "curly braces, and used in nested loop constructs.\n"		     },
 { 0,	       0,	       "Calculator"				     },
 { "px",       parse_PX,       "Print hex:           px EXPR"		     },
 { "pd",       parse_PD,       "Print decimal:       pd EXPR"		     },
 { "po",       parse_PO,       "Print octal:         po EXPR"		     },
 { "pb",       parse_PB,       "Print binary:        pb EXPR"		     },
 { "nm",       parse_NM,       "Look up PROM addr:   nm ADDR"		     },
 { 0,	       0,	       "Hardware Registers"			     },
 { "pr",       parse_PR,       "Print register(s):   pr [GPRNAME [VAL]"	     },
 { "pf",       parse_PF,       "Print fpreg(s):      pf [REGNO]"	     },
 { "sr",       parse_SR,       "Store register:      sr REG VAL"	     },
 { "sf",       parse_SF,       "Store fpreg:         sf REGNO VAL"	     },
 { 0,	       0,	       "Memory Access"				     },
 { "pa",       parse_PA,       "Print address:       pa ADDR [BITNO]"	     },
 { "lb",       parse_LB,       "Load byte:           lb ADDR [COUNT]"	     },
 { "lh",       parse_LH,       "Load half-word:      lh ADDR [COUNT]"	     },
 { "lw",       parse_LW,       "Load word:           lw ADDR [COUNT]"	     },
 { "ld",       parse_LD,       "Load double-word:    ld ADDR [COUNT]"	     },
 { "la",       parse_LA,       "Load ASCII:          la ADDR [COUNT]"	     },
 { "sb",       parse_SB,       "Store byte:          sb ADDR [VAL [COUNT]]"  },
 { "sh",       parse_SH,       "Store half-word:     sh ADDR [VAL [COUNT]]"  },
 { "sw",       parse_SW,       "Store word:          sw ADDR [VAL [COUNT]]"  },
 { "sd",       parse_SD,       "Store double-word:   sd ADDR [VAL [COUNT]]"  },
 { "sdv",      parse_SDV,      "Store and verify:    sdv ADDR VAL"	     },
 { 0,	       0,	       "Memory Operations"			     },
 { "memset",   parse_MEMSET,   "Fill mem w/ byte:    memset DST BYTE LEN"    },
 { "memcpy",   parse_MEMCPY,   "Copy memory bytes:   memcpy DST SRC LEN"     },
 { "memcmp",   parse_MEMCMP,   "Cmp memory bytes:    memcmp DST SRC LEN"     },
 { "memsum",   parse_MEMSUM,   "Add memory bytes:    memsum SRC LEN"	     },
 { 0,	       0,	       "Memory Testing"				     },
 { "santest",  parse_SANTEST,  "Mem. sanity test:    santest ADDR"	     },
 { "dirinit",  parse_DIRINIT,  "Dir/prot init:       dirinit START LEN"	     },
 { "meminit",  parse_MEMINIT,  "Memory clear:        meminit START LEN"	     },
 { "dirtest",  parse_DIRTEST,  "Dir. test/init:      dirtest START LEN"	     },
 { "memtest",  parse_MEMTEST,  "Memory test/init:    memtest START LEN"	     },
 { "clear",    parse_CLEAR,    "Clear errors:        clear"		     },
 { "error",    parse_ERROR,    "Display errors:      error"		     },
 { "qual",     parse_QUAL,     "Quality mode:        qual [1|0]"	     },
 { "ecc",      parse_ECC,      "ECC mode:            ecc [1|0]"		     },
 { "im",       parse_IM,       "Set R10k int mask:   im [BYTE]"		     },
 { "maxerr",   parse_MAXERR,   "Test error limit:    maxerr COUNT"	     },
 { "scandir",  parse_SCANDIR,  "Scan dir states:     scandir ADDR [LEN]"     },
 { "dirstate", parse_DIRSTATE, "Directory state:     dirstate [BASE "
						    "[LEN [STATE]]]"	     },
 { 0,	       0,	       "Network and Vectors"			     },
 { "vr",       parse_VR,       "Vector read:         vr VEC VADDR"	     },
 { "vw",       parse_VW,       "Vector write:        vw VEC VADDR VAL"	     },
 { "vx",       parse_VX,       "Vector exchange:     vx VEC VADDR VAL"	     },
 { "disc",     parse_DISC,     "Discover network:    disc"		     },
 { "pcfg",     parse_PCFG,     "Dump pcfg struct:    pcfg [n:NODE] [v]"	     },
 { "node",     parse_NODE,     "Get/set node ID:     node [[VEC] ID]"	     },
 { "route",    parse_ROUTE,    "Set up route:        route [VEC NODE]"	     },
 { "rnic",     parse_RNIC,     "Read router NIC:     rnic [VEC]"	     },
 { "cfg",      parse_CFG,      "Dump config info.:   cfg [n:NODE]"	     },
 { "rtab",     parse_RTAB,     "Dump route table:    rtab [VEC]"	     },
 { "rstat",    parse_RSTAT,    "Dmp/clr rtr stat:    rstat VEC"		     },
 { 0,	       0,	       "Control Structures"			     },
 { "reset",    parse_RESET,    "Reset the system:    reset"		     },
 { "softreset",parse_SOFTRESET,"Softreset a node:    softreset n:NODE"	     },
 { "call",     parse_CALL,     "Call subroutine:     call ADDR "
						    "[A0 [A1 [...]]]"	     },
 { "jump",     parse_JUMP,     "Inv. cache & jump:   jump ADDR [A0 [A1]]"    },
 { "resume",   parse_RESUME,   ".Restore & cont.:     resume"		     },
 { "slave",    parse_SLAVE,    "Goto slave mode:     slave"		     },
 { "repeat",   parse_REPEAT,   "Repeat count:        repeat COUNT CMD"	     },
 { "loop",     parse_LOOP,     "Repeat forever:      loop CMD"		     },
 { "while",    parse_WHILE,    "While loop:          while (EXPR) CMD"	     },
 { "for",      parse_FOR,      "For loop:            for (CMD;EXPR;CMD) CMD" },
 { "if",       parse_IF,       "If statement:        if (EXPR) CMD"	     },
 { "delay",    parse_DELAY,    "Delay:               delay MICROSEC"	     },
 { "sleep",    parse_SLEEP,    "Sleep:               sleep SEC"		     },
 { "time",     parse_TIME,     "Benchmark timing:    time CMD"		     },
 { "echo",     parse_ECHO,     "Echo string:         echo \"STRING\""	     },
 { 0,	       0,	       "Miscellaneous"				     },
 { "version",  parse_VERSION,  "Show PROM version:   version"		     },
 { "help",     parse_HELP,     "Display help:        help [CMDNAME]"	     },
 { "credits",  parse_CREDITS,  ".xtalk credits:      credits"		     },
 { "nic",      parse_NIC,      "Read hub NIC:        nic [n:NODE]"	     },
 { "flash",    parse_FLASH,    "Prgm remote PROM:    flash NODE [...]"     },
 { "fflash",   parse_FFLASH,   "Prgm remote PROM with values:    fflash NODE [...]"     },
 { 0,	       0,	       "TLB and Cache"				     },
 { "tlbc",     parse_TLBC,     "Clear TLB:           tlbc [INDEX]"	     },
 { "tlbr",     parse_TLBR,     "Read TLB:            tlbr [INDEX]"	     },
 { "inval",    parse_INVAL,    "Inv. cache(s):       inval [i][d][s]"	     },
 { "flush",    parse_FLUSH,    "Flush+inv caches:    flush"		     },
 { "dtag",     parse_DTAG,     "Dump dcache tag:     dtag line"		     },
 { "itag",     parse_ITAG,     "Dump icache tag:     dtag line"		     },
 { "stag",     parse_STAG,     "Dump scache tag:     stag line"		     },
 { "dline",    parse_DLINE,    "Dump dcache line:    dline line"	     },
 { "iline",    parse_ILINE,    "Dump icache line:    iline line"	     },
 { "sline",    parse_SLINE,    "Dump scache line:    sline line"	     },
 { "adtag",    parse_ADTAG,    "Dump dcache tag:     adtag line"	     },
 { "aitag",    parse_AITAG,    "Dump icache tag:     aitag line"	     },
 { "astag",    parse_ASTAG,    "Dump scache tag:     astag line"	     },
 { "adline",   parse_ADLINE,   "Dump dcache line:    adline line"	     },
 { "ailine",   parse_AILINE,   "Dump icache line:    ailine line"	     },
 { "asline",   parse_ASLINE,   "Dump scache line:    asline line"	     },
 { "sscache",  parse_SSCACHE,  "Store a scache dword:	sscache line taglo taghi"},
 { "sstag",    parse_SSTAG,    "Store a scache tag:	sstag line taglo taghi [way]"},
 { "go",       parse_GO,       "Set memory mode:     go dex|unc|cac"	     },
 { "hubsde",   parse_HUBSDE,   "Hub_send_data_err:   hubsde"		     },
 { "rtrsde",   parse_RTRSDE,   "Rtr_send_data_err:   rtrsde"		     },
 { "chklink",  parse_CHKLINK,  "Check local link:    chklink"		     },
 { "bist",     parse_BIST,     "Self-test hub:       bist le|ae|lr|ar "
						    "[n:NODE]"		     },
 { "rbist",    parse_RBIST,    "Self-test router:    rbist le|ae|lr|ar VEC"  },
 { "disable",  parse_DISABLE,  "Disable CPU/MEM:     disable n:NODE "
						    "[SLICE/BANKS]"	     },
 { "enable",   parse_ENABLE,   "Enable CPU/MEM:      enable n:NODE "
						    "[SLICE/BANKS]"	     },
 { "tdisable", parse_TDISABLE, "Temp. disable:       tdisable n:NODE [SLICE]"},
 { 0,	       0,	       "I/O PROM"				     },
 { "segs",     parse_SEGS,     "List segments:       segs [FLAG]"	     },
 { "exec",     parse_EXEC,     "Load/exec segment:   exec [SEGNAME [FLAG]]"  },
 { "reconf",   parse_RECONF,   "Reconfig. memory:    reconf"		     },
 { 0,	       0,	       "Console Selection"			     },
 { "ioc3",     parse_IOC3,     "Use IOC3 UART:       ioc3"		     },
 { "junk",     parse_JUNK,     "Use JunkBus UART:    junk"		     },
 { "elsc",     parse_ELSC,     "Use SysCtlr UART:    elsc"		     },
 { 0,	       0,	       "Error Registers"			     },
 { "crb",      parse_CRB,      "Dump II CRBs:        crb [n:NODE]"	     },
 { "crbx",     parse_CRBX,     "137-col wide crb:    crbx [n:NODE]"	     },
 { "dumpspool",parse_DUMPSPOOL,"Dump PI err spool:   dumpspool "
						    "[n:NODE SLICE]"	     },
 { "error_dump",
	       parse_ERROR_DUMP,
			       "Dump error info:     error_dump"	     },
 { "reset_dump",
	       parse_RESET_DUMP,
			       "Dump reset error:    reset_dump"	     },
 { "edump_bri",
	       parse_EDUMP_BRI,
			       "Dump bridge errs:    edump_bri [n:NODE]"     },
 { 0,	       0,	       "System Controller"			     },
 { "sc",       parse_SC,       "System ctlr cmd:     sc [\"STRING\"]"	     },
 { "scw",      parse_SCW,      "Wr sysctlr nvram:    scw ADDR [VAL [COUNT]]" },
 { "scr",      parse_SCR,      "Rd sysctlr nvram:    scr ADDR [COUNT]"	     },
 { "dips",     parse_DIPS,     "Rd sysctlr dbgsw:    dips"		     },
 { "dbg",      parse_DBG,      "Set/get debug sw:    dbg [VIRT_VAL PHYS_VAL]"},
 { "pas",      parse_PAS,      "Set/get password:    pas [\"PASW\"]"	     },
 { "module",   parse_MODULE,   "Set/get module #:    module [NUM]"	     },
 { "partition",parse_PARTITION,"Set/get partition #: partition [NUM]"	     },
 { "modnic",   parse_MODNIC,   "Get module NIC:      modnic"		     },
 { 0,	       0,	       "Debugging"				     },
 { "verbose",  parse_VERBOSE,  "Verbose mode:        verbose [1|0]"	     },
 { "altregs",  parse_ALTREGS,  "Use alt. regs:       altregs [NUM]"	     },
 { "kdebug",   parse_KDEBUG,   "kernel debugging:    kdebug [STACKADDR]"     },
 { "kern_sym", parse_KERN_SYM, "Use kernel symtab:   kern_sym"		     },
 { "nmi",      parse_NMI,      "Send NMI to node:    nmi n:NODE [a|b]"	     },
 { "why",      parse_WHY,      "Why are we here?:    why"		     },
 { "btrace",   parse_BTRACE,   "Stack backtrace:     btrace [epc sp]"	     },
 { "cpu",      parse_CPU,      "Switch to cpu:       cpu [[n:NODE] a|b]"     },
 { "dis",      parse_DIS,      "Disassemble:         dis ADDR [COUNT]"	     },
 { "dmc",      parse_DMC,      "Dump mem cfg:        dmc [n:NODE]"	     },
 { "fru",      parse_FRU,      "Run FRU analyzer:    fru [1(local) | "
						    "2(all node)]"	     },
 { 0,	       0,	       "Environment Variables and Error Log"	     },
 { "initlog",  parse_INITLOG,  "Init. PROM log:      initlog [n:NODE]"	     },
 { "clearlog", parse_CLEARLOG, "Clear PROM log:      clearlog [n:NODE]"	     },
 { "initalllogs",
	       parse_INITALLLOGS,
			       "Init. all PROM logs in system: initalllogs"  },
 { "clearalllogs",
	       parse_CLEARALLLOGS,
			       "Clear all PROM logs in system: clearalllogs"  },
 { "setenv",   parse_SETENV,   "Set variable:        setenv [n:NODE] "
						    "KEY [\"STRING\"]"	     },
 { "unsetenv", parse_UNSETENV, "Remove variable:     unsetenv [n:NODE] KEY"  },
 { "printenv", parse_PRINTENV, "Print variables:     printenv [n:NODE] [KEY]"},
 { "log",      parse_LOG,      "Tail log entries:    log [n:NODE] "
						    "[TAIL_CNT [HEAD_CNT]]"  },
 { "setpart",  parse_SETPART,  "Set partition:       setpart RTR_LIST"	     },
 { 0,	       0,	       "I/O Diagnostics"			     },
 { "dgxbow",   parse_TESTXBOW, "XBow Diagnostic:     dgxbow "
						    "[m<n|h|m>] "
						    "[n<NODE>]"		     },
 { "dgbrdg",   parse_TESTBRDG, "Bridge Diagnostic:   dgbrdg "
						    "[m<n|h|m>] "
						    "[n<NODE>] "
						    "[s<slot>] "	     },
 { "dgconf",   parse_TESTCONF, "IO6 Conf Spc Diag:   dgconf "
						    "[m<n|h|m>] "
						    "[n<NODE>] "
						    "[s<slot>]"		     },
 { "dgpci",    parse_TESTPCI,  "PCI Bus Diag.:       dgpci "
						    "[m<n|h|m>] "
						    "[n<NODE>] "
						    "[s<slot>] "
						    "[p<PCI#>]"		     },
 { "dgspio",   parse_TESTSPIO, "Serial PIO Diag:     dgspio "
						    "[m<n|h|m|x>] "
						    "[n<NODE>] "
						    "[s<slot>] "
						    "[p<PCI#>] "	     },
 { "dgsdma",   parse_TESTSDMA, "Serial DMA Diag:     dgsdma "
						    "[m<n|h|m|x>] "
						    "[n<NODE>] "
						    "[s<slot>] "
						    "[p<PCI#>] "	     },
 { 0,	       0,		0					     }
};

cmd_t *cmd_name2cmd(char *name)
{
    int		i;

    for (i = 0; cmds[i].help != 0; i++)
	if (cmds[i].name && strcmp(cmds[i].name, name) == 0)
	    return &cmds[i];

    return 0;
}

int cmd_name2token(char *name)
{
    cmd_t	*pc;

    if ((pc = cmd_name2cmd(name)) == 0)
	return 0;

    return pc->token;
}

char *cmd_name2help(char *name)
{
    cmd_t	*pc;

    if ((pc = cmd_name2cmd(name)) == 0)
	return 0;

    return pc->help;
}

cmd_t *cmd_token2cmd(int token)
{
    int		i;

    for (i = 0; cmds[i].help != 0; i++)
	if (cmds[i].name && cmds[i].token == token)
	    return &cmds[i];

    return 0;
}
