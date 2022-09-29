#ident "$Header: "
/*
 * hubreg_cmd() -- Display the information about a hardware register
 * 		   Modelled after stand arcs/tools/hwreg/main.c
 */
/*
 * SN0 Address Analyzer
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <errno.h>
#include <klib/klib.h>
#include <klib/hwreg.h>
#include <klib/hwregcmd.h>
#include "icrash.h"

int hubreg_disabled = 0;

#define _HUBREG_USAGE \
             "[-bin|-oct|-dec|-hex] [-short] [-node n] register\n\
       hubreg [-b|-o|-d|-h] [-s] [-n n] register\n\
                          -> Display 'register' in different formats.\n\
       hubreg -l|-list    -> List all defined register names.\n\
       hubreg -h|-help    -> Display more expanded help info.\n\
       hubreg -listacc    -> Display info on access mode definitions."

       /*
	* This looks like an unneccessary duplication but the reason for
	* it is 'beauty'. The problem is that CMD_USAGE adds words
	* 'USAGE: hubreg' to the first line while CMD_HELP adds
	* 'COMMAND: hubreg' to the first line.
	* And since those two differ by two characters, using _HUBREG_USAGE
	* in CMD_HELP (which would be very logical and appropriate)
	* would result in misaligning of lines...
	* So I opted for duplicating the text but adding two blanks in
	* _HUBREG_HELP to make things look pretty when they are output
	* to the screen. Don't beat me for it...
	*/
#define _HUBREG_HELP \
               "[-bin|-oct|-dec|-hex] [-short] register\n\
         hubreg [-b|-o|-d|-x] [-s] [-n n] register\n\
                            -> Display 'register' in different formats.\n\
         hubreg -l|-list    -> List all defined register names.\n\
         hubreg -h|-help    -> Display more expanded help info.\n\
         hubreg -listacc    -> Display info on access mode definitions."

/*
 * has_hubregs() -- check if this coredump contains dump of hub registers
 */
int
has_hubregs()
{
    int			bwin, swin, node;
    i64                 addr;
    hwreg_set_t        *regset;
    hwreg_t            *r;
    i64                 value, na;
    k_ptr_t             hubreg_value = &value;

		/*
		 * pickup the first hub register (for convenience,
		 * in principle any register name would do), generate
		 * its address, don't forget remote bit because all
		 * hub regs addresses in the dump will be with the remote
		 * bit, add loop for all nodes from 0 to max adding
		 * the node number to the "virgin address" and checking
		 * if that address exists in the dump. 
		 * Return 1 (true) if it does for at least one address,
		 * return 0 if no such address is found for all nodes.
		 */

    strcpy(opt_addr, "PI_CPU_PROTECT");
    if ((r = hwreg_lookup_name(&hwreg_hub, opt_addr, 1)) != 0) {
		regset = &hwreg_hub;
    } else {
	return(-1);
    }

    addr = r->address;
    ADD_REGION(addr, IO_BASE);
    ADD_REMOTE_BIT(addr);
    ADD_BWIN(addr, 0);
    ADD_SWIN(addr, 1);
    na   = NODEOFF(addr);
    bwin = BWIN(addr);
    swin = SWIN(addr);
    if (na >= 256 * MB)
		na -= 256 * MB;
    if (bwin != 0 || swin != 1) 
		return(-1);
    for (node=0; node < K_NUMNODES; node++) {
	REM_NODE(addr);
	ADD_NODE(addr, node);
	if (!kl_get_block(addr, 8, hubreg_value, "hubreg")) {
		return(1);
	}
    }
    return(0);			/* no hub registers found */
}

/*
 * hubreg_usage() -- Print the usage string for the 'hubreg' command.
 */
void
hubreg_usage(command_t cmd)
{
	CMD_USAGE(cmd, _HUBREG_USAGE);
}

/*
 * hubreg_help() -- Print the help information for the 'hubreg' command.
 */
void
hubreg_help(command_t cmd)
{
    CMD_HELP(cmd, _HUBREG_HELP,
	    "Display contents of a hub register. The register name can "
	    "be given in upper case or in lower case or even as a full "
	    "address. If the address corresponds to a real hub register, "
	    "its name will be found and displayed. The register given by "
	    "its name (and not by its address), does not have to be its "
	    "full name, only the first so many letters making its name "
	    "unique are needed. If a unique match cannot be found, the "
	    "command will list all the register names matching the "
	    "characters given, and you can then pick up the correct one "
	    "from the list.");
    P0("    Flags:\n");
    P0("       -bin|-s          Print fields in binary instead of hex\n");
    P0("       -oct|-o          Print fields in octal instead of hex\n");
    P0("       -dec|-d          Print fields in decimal instead of hex\n");
    P0("       -hex|-x          Print fields in hex (default)\n");
    P0("       -short|-s        Print decoded fields in short form\n");
    P0("       -node n|-n n     Generate addresses for specified node 'n'\n");
    P0("       -list|-l         Lists all register names\n");
    P0("       -listacc         Display help info on access mode definitions\n");
    P0("       -help|-h         Display help info for this command\n");
    P0("    Examples:\n");
    P0("       1. hubreg NI_PORT_ERROR\n");
    P0("       2. hubreg ni_port_e\n");
    P0("       3. hubreg 0x9200000001200018\n");
    P0("       4. hubreg -o -s -n 3 md_memory_config\n");
    P0("       5. hubreg -bin -s -node 3 MD_MEMO\n");
    P0("       6. hubreg -list\n");
    if (hubreg_disabled > 0) {
	P0("\n    'hubreg' command is currently disabled because\n");
	switch (hubreg_disabled) {
	    case 1:
		P0("    it cannot be used on a running system.\n");
		break;
	    case 2:
		P0("    it is not available for this coredump v");
		P1("%d\n", K_DUMP_HDR->dmp_version);
		P0("    The coredump version must be >= 4.\n");
		break;
	    case 3:
		P0("    this coredump does not contain hub registers at all.\n");
		break;
	    default:
		P0("Internal error in 'icrash': routine icrash/cmds/cmd_hubreg.c\n");
		break;
	}
    }
}

int
hubreg_parse(command_t cmd)
{
    return (C_WRITE|C_TRUE|C_SELF);
}

int
hubreg_cmd(command_t cmd)
{

    i64			addr;
    hwreg_set_t	       *regset;
    hwreg_t	       *r;
    i64			value;
    k_ptr_t		hubreg_value;
    int			argc;
    char	       **argv;
    int			PRINTF(const char *, ...);

    argc = cmd.nargs;
    argv = cmd.args;

		/*
		 * set default values so that next command starts with
		 * the clean state.
		 * Idea: it might be useful to think that somebody would
		 * like to set his own defaults so he doesn't have to
		 * retype every the same parameters (e.g. the base, the
		 * short option, the node number. This needs input from
		 * people....
		 */
    hubreg_value   = &value;
    opt_node       = 0;
    opt_node_given = 0;
    opt_base       = 16;
    opt_nmode      = 0;
    opt_list       = 0;
    opt_short      = 0;
    winsize	   = 512 * MB;


    if (hubreg_disabled > 0) {
        P0("'hubreg' command is currently disabled (use 'help hubreg' for info).\n");
        return(1);
    }

    if (ACTIVE) {
	P0("!! 'hubreg' command is not available on running system !!\n");
        P0("'hubreg' command is now disabled (use 'help hubreg' for info).\n");
	hubreg_disabled = 1;
        return(1);
    }

    if (K_DUMP_HDR->dmp_version < 4) {
	P0("'hubreg' command is not available for coredump v.");
	P1("%d \n", K_DUMP_HDR->dmp_version);
        P0("         coredump version must be >= 4.\n");
        P0("'hubreg' command is now disabled (use 'help hubreg' for info).\n");
	hubreg_disabled = 2;
        return(1);
    }

    if (!has_hubregs()) {
	P0("This corefile does not contain hub registers\n");
        P0("'hubreg' command is now disabled (use 'help hubreg' for info).\n");
	hubreg_disabled = 3;
        return(1);
    }

    if (argc == 0) {
	hubreg_usage(cmd);
	return(0);
    }

    while (argc) {
	if      (strcmp(argv[0], "-bin")   == 0 ||
		 strcmp(argv[0], "-b")     == 0) 
	    opt_base = 2;
	else if (strcmp(argv[0], "-oct")   == 0 ||
		 strcmp(argv[0], "-o")     == 0)
	    opt_base = 8;
	else if (strcmp(argv[0], "-dec")   == 0 ||
		 strcmp(argv[0], "-d")     == 0)
	    opt_base = 10;
	else if (strcmp(argv[0], "-hex")   == 0 ||
		 strcmp(argv[0], "-x")     == 0)
	    opt_base = 16;
	else if (strcmp(argv[0], "-node")  == 0 ||
		 strcmp(argv[0], "-n")     == 0) {
	    argc--, argv++;
	    if (argc == 0)
		hubreg_usage(cmd);
	    opt_node = strtoul(argv[0], 0, 0);
	    if (opt_node >= K_NUMNODES) {
		P1("Node number %d exceeds total number of nodes"
		   " in this corefile.\n", opt_node);
		P1("This corefile contains %d nodes.\n", K_NUMNODES);
		return(1);		/* ignore the rest of the command */
	    }
	    opt_node_given = 1;
	} else if (strcmp(argv[0], "-list") == 0 ||
		   strcmp(argv[0], "-l")    == 0)
	    opt_list = 1;
	else if (strcmp(argv[0], "-short") == 0 ||
		 strcmp(argv[0], "-s")     == 0)
	    opt_short = 1;
	else if (strcmp(argv[0], "-listacc") == 0) {
	    listacc();
	    return(0);
	} else if (strcmp(argv[0], "-help") == 0 ||
		   strcmp(argv[0], "-h")    == 0) {
	    hubreg_help(cmd);
	    return(0);
	} else if (argv[0][0] == '-') {
	    P1("Unknown flag: %s\n", argv[0]);
	    hubreg_usage(cmd);
	    return(1);
	} else
	    break;

	argc--, argv++;
    }

    if (argc == 0) {
	if (opt_list) {
	    listreg_matches(&hwreg_hub, "");
	    return(0);
	}

	P0("Address required\n");
	return(1);
    }

    strcpy(opt_addr, argv[0]);
    argc--, argv++;

    /*
     * if node wasn't given in the command use node=0 as default
     */

    if (opt_node_given == 0) {
	opt_node_given = 1;
	opt_node = 0;
    }


    /*
     * Process address argument -- either a number or register name.
     */

    r = 0;

    if (isdigit(opt_addr[0])) {
	addr = strtoull(opt_addr, 0, 0);
	if (!IS_HUBSPACE(addr)) {
		P1("Address %s is not a hub register address\n", opt_addr);
		P0("A hub address must always be given complete 64bits\n");
		P0("such as 0x9200000001200018 (the node number may be left out).\n");
		return(1);
	}
    } else {
	if ((r = hwreg_lookup_name(&hwreg_hub, opt_addr, 1)) != 0) {
	    regset = &hwreg_hub;
	} else {
	    P1("Unknown or ambiguous register name: %s\n", opt_addr);
	    P0("Use -list to see all registers.  Possible matches:\n");

	    listreg_matches(&hwreg_hub, opt_addr);

	    return(1);
	}

	addr = r->address;
    }

    addr = HUBOFF(addr);
    ADD_REGION(addr, IO_BASE);
    if (opt_node_given)
        ADD_REMOTE_BIT(addr);
    ADD_BWIN(addr, 0);
    ADD_SWIN(addr, 1);

    if (opt_node_given)
	ADD_NODE(addr, opt_node);

    do_address(addr);

    if (r == 0) {
	regset = &hwreg_hub;
	r = hwreg_lookup_addr(regset, HUBOFF(addr));
    }

    if (r) {
        listreg(regset, r);
    } else {
        P0("Register matching address not found.\n");
        return(1);
    }

    if (argc > 0) {
	P0("Extra argument(s).\n");
	return(1);
    }

    if (kl_get_block(addr, 8, hubreg_value, "hubreg")) {
	P1("Address 0x%016llx was not found in the dump.\n", addr);
	return(-1);
    }

    if ( value == DUMP_HUB_INIT_PATTERN ) {
	P0("Value: not available\n");
    } else {
	P1("Value:  0x%016llx\n", value);
	P0("Decoded:\n");
	hwreg_decode(regset, r, opt_short, 0,
		 opt_base, value, PRINTF);
    }

    if (r->noteoff)
	P1("Note: %s\n", regset->strtab + r->noteoff);

    return(0);
}
