#ident "$Header: "
/*
 * dirmem_cmd() -- Display the information on directory memory that's
 *		   related to given address.
 *		   Modelled after stand/arcs/tools/hwreg/main.c
 */
/*
 *  * SN0 Address Analyzer
 *   */

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

int dirmem_disabled = -1;
int memory_premium  = -1;
extern dir_ent_flags;

#define _DIRMEM_USAGE \
             "[-bin|-oct|-dec|-hex] [-short] [-mem] [-prot] [-ecc] \\\n\
              [-region low[-hi]] addr\n\
       dirmem [-b|-o|-d|-h] [-s] [-m] [-p] [-e] [-r low[-hi]] addr\n\
                          -> Display, in different formats, the contents\n\
                             of the directory memory corresponding to\n\
                             memory address 'addr' .\n\
       dirmem -h|-help    -> Display more expanded help info."

       /*
        * This looks like an unneccessary duplication but the reason for
        * it is 'beauty'. The problem is that CMD_USAGE adds words
        * 'USAGE: dirmem' to the first line while CMD_HELP adds
        * 'COMMAND: dirmem' to the first line.
        * And since those two differ by two characters, using _HUBREG_USAGE
        * in CMD_HELP (which would be very logical and appropriate)
        * would result in misaligning of lines...
        * So I opted for duplicating the text but adding two blanks in
        * _HUBREG_HELP to make things look pretty when they are output
        * to the screen. Don't beat me for it...
        */
#define _DIRMEM_HELP \
               "[-bin|-oct|-dec|-hex] [-short] [-mem] [-prot] [-ecc] \\\n\
                [-region low[-hi]] addr\n\
         dirmem [-b|-o|-d|-h] [-s] [-m] [-p] [-e] [-r low[-hi]] addr\n\
                            -> Display, in different formats, the contents\n\
                               of the directory memory corresponding to\n\
                               memory address 'addr' .\n\
         dirmem -h|-help    -> Display more expanded help info."

/*
 * has_dirmem() -- check if this coredump contains dump of directory memory
 */
int
has_dirmem()
{
    i64       addr_lo, addr_hi;
    i64       value_lo, value_hi;
    k_ptr_t   dirmem_value_lo, dirmem_value_hi;
    struct    syment *sp;
    kaddr_t   start_addr;

    	/*
	 * To check if this dump contains directory memory at all
	 * the following "trick" is used:
	 * Since the directory memory exists for all memory locations,
	 * let's pick up one we are sure always exists and try to find
	 * the corresponding directory memory address for it.
	 * If it does not exist, we conclude that this dump does not
	 * contain directory memory at all.
	 * For this test we pickup kernel address "fork" because this one
	 * must always exist.
	 */

    dirmem_value_lo   = &value_lo;
    dirmem_value_hi   = &value_hi;

    strcpy(opt_addr, "fork");
    sp = kl_get_sym(opt_addr, B_TEMP);
    if (!KL_ERROR) {
        start_addr = sp->n_value;
        kl_free_sym(sp);
    } else {
	return(-1);		/* should never happen */
				/* the symbol "fork" must always exist */
    }

    addr_lo   = BDDIR_ENTRY_LO(start_addr);
    addr_hi   = BDDIR_ENTRY_HI(start_addr);
    
    if (kl_get_block(addr_lo,   8, dirmem_value_lo,   "dirmem"))
	return(0);			/* directory memory not found */
    if (kl_get_block(addr_hi,   8, dirmem_value_hi,   "dirmem"))
	return(0);			/* directory memory not found */

    					/* 0 = std dirmem	*/
    					/* 1 = premium dirmem	*/
    memory_premium = dir_ent_flags & DUMP_PREMIUM_DIR;
    return(1);				/* directory memory found */
}

/*
 * dirmem_usage() -- Print the usage string for the 'dirmem' command.
 */
void
dirmem_usage(command_t cmd)
{
            CMD_USAGE(cmd, _DIRMEM_USAGE);
}

/*
 * dirmem_help() -- Print the help information for the 'dirmem' command.
 */
void
dirmem_help(command_t cmd)
{
        CMD_HELP(cmd, _DIRMEM_HELP,
		"Display contents of the directory memory. The memory address "
		"'addr' can be given as a kernel symbolic address (example "
		"could be 'dirmem fork', or a hexadecimal address such "
		" as 'dirmem 0xa800000f47160000'. The leading '0x' can be "
		" omitted, the address will be understood to be hexadecimal.");
    P0("    Flags:\n");
    P0("       -bin  |-b        Print fields in binary instead of hex\n");
    P0("       -oct  |-o        Print fields in octal instead of hex\n");
    P0("       -dec  |-d        Print fields in decimal instead of hex\n");
    P0("       -hex  |-x        Print fields in hex (default)\n");
    P0("       -short|-s        Print decoded fields in short form\n");
    P0("       -mem  |-m        Print HI and LO parts of the directory format\n");
    P0("       -prot |-p        Print the directory page reference/protection\n");
    P0("       -region low-hi   or alternatively\n");
    P0("       -r low-hi\n");
    P0("                        Print directory page reference/protection for\n");
    P0("                              regions between 'low' and 'hi' included\n");
    P0("                              Default is 0-0\n");
    P0("                              'hi' maybe be left out if 'hi'='lo'\n");
    P0("                              Always low <= hi\n");
    P0("                              Limits: <low,hi>=<0,15> standard memory\n");
    P0("                                      <low,hi>=<0,63> premium memory\n");
    P0("       -ecc  |-e        Print the directory ECC bytes\n");
    P0("       -help |-h        Display help info for this command\n");
    P0("\n");
    P0("    If none of the [-m|-p|-e] is given, all three flags will be enabled.\n");
    P0("    Specify -p if all you want are the protection pages.\n");
    P0("    If -r is given, -p becomes enabled automatically.\n");
    P0("    The -ecc does not work yet. This dump contains no ECC information:\n");
    P0("    copying this information from a system to system dump has not been\n");
    P0("    implemented.\n");
    P0("\n");
    P0("    Examples:\n");
    P0("       1. dirmem fork\n");
    P0("       2. dirmem -o -m -r 0-3 0xa800000f47160000\n");
    P0("       3. dirmem -o -m -r 2 0xa800000f47160000\n");
    P0("       4. dirmem -p 0xa800000f47160000\n");
    P0("       5. dirmem -r 0-4 0xa800000f47160000\n");
    P0("       6. dirmem -help\n");
    if (dirmem_disabled > 0) {
        P0("\n    'dirmem' command is currently disabled because\n");
        switch (dirmem_disabled) {
            case 1:
                P0("    it cannot be used on a running system.\n");
                break;
            case 2:
                P0("    it is not available for this coredump v");
                P1("%d\n", K_DUMP_HDR->dmp_version);
                P0("    The coredump version must be >= 4.\n");
                break;
            case 3:
                P0("    this coredump does not contain directory memory at all.\n");
                break;
	    case 4:
		P0("    it cannot determine directory memory address for 'fork',\n");
		P0("    which is used as the internal consistency check.\n");
		break;
	    case 5:
		P0("    because it was incorrectly initialized, this may be\n");
		P0("    a problem with icrash itself or with this coredump.\n");
		break;
            default:
                P0("Internal error in 'icrash': routine icrash/cmds/cmd_dirmem.c\n");
                break;
        }
    }
}

int
dirmem_parse(command_t cmd)
{
    return (C_WRITE|C_TRUE|C_SELF);
}

int dirmem_cmd(command_t cmd)
{

    int		    argc;
    char	  **argv;

    i64		    addr_lo, addr_hi, addr_prot, addr_ecc;
    hwreg_set_t    *regset;
    hwreg_t        *r;
    i64             value_lo, value_hi, value_prot, value_ecc;
    i64	            bitvec, mask;
    k_ptr_t         dirmem_value_lo, dirmem_value_hi;
    k_ptr_t         dirmem_value_prot, dirmem_value_ecc;
    int		    dir_fine, dir_state;
    char	    dir_hi[40], dir_lo[40], dir_prot[40], dir_ecc[10];
    char	    dir_fmt[30], dir_mem[10];
    char	    bit_vec[40];
    int		    mem, prot, ecc, err, i, j, k, nbits;
    int		    region, reg_low, reg_hi, reg_max;
    int		    OneCnt;
    int		    PRINTF(const char *, ...);
    char	  *tmpptr;

    int		    mode;
    k_uint_t        size;
    kaddr_t         start_addr;
    struct          syment *sp;

    argc = cmd.nargs;
    argv = cmd.args;

			/*
			 * set default values so that next command starts with
			 * the clean state.
			 */

    dirmem_value_lo   = &value_lo;
    dirmem_value_hi   = &value_hi;
    dirmem_value_prot = &value_prot;
    dirmem_value_ecc  = &value_ecc;

    opt_base  = 16;
    opt_short =  0;
    mem       =  0;
    prot      =  0;
    ecc       =  0;
    err       =  0;
    region    =  0;
    reg_low   =  0;
    reg_hi    =  0;

    			/* execute only once before the first 'dirmem' command 
			 */
    if (dirmem_disabled < 0) {

	i = has_dirmem();
	dirmem_disabled = 0;

        if (ACTIVE) {
            P0("!! 'dirmem' command is not available on running system !!\n");
            P0("'dirmem' command is now disabled (use 'help dirmem' for info).\n");
            dirmem_disabled = 1;
        }
        else if (K_DUMP_HDR->dmp_version < 4) {
            P0("'dirmem' command is not available for coredump v.");
            P1("%d \n", K_DUMP_HDR->dmp_version);
            P0("         coredump version must be >= 4.\n");
            P0("'dirmem' command is now disabled (use 'help dirmem' for info).\n");
            dirmem_disabled = 2;
        }
        else if (i == -1) {
	    P0("This corefile does not contain directory memory address for\n");
	    P0("the kernel address 'fork' (used as an internal consistency check.)\n");
	    P0("Something is probably very wrong with this corefile\n");
            P0("'dirmem' command is now disabled (use 'help dirmem' for info).\n");
	    dirmem_disabled = 4;
        }
        else if (!i) {
            P0("This corefile does not contain directory memory.\n");
            P0("'dirmem' command is now disabled (use 'help dirmem' for info).\n");
            dirmem_disabled = 3;
	} else if (memory_premium < 0) {
	    P0("Icrash: 'dirmem' command is incorrectly initialized\n");
	    P0("This is a serious error, either with icrash or with this coredump\n");
	    P0("'dirmem' command is now disabled\n");
	    dirmem_disabled = 5;
        }
	if (dirmem_disabled)
	    return(1);
    }

    if (dirmem_disabled) {
        P0("'dirmem' command is currently disabled (use 'help dirmem' for info).\n");
        return(1);
    }

    if (argc == 0) {
        dirmem_usage(cmd);
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
        else if (strcmp(argv[0], "-short") == 0 ||
                 strcmp(argv[0], "-s")     == 0)
            opt_short = 1;
	else if (strcmp(argv[0], "-mem"  ) == 0 ||
		 strcmp(argv[0], "-m")     == 0)
	    mem = 1;
	else if (strcmp(argv[0], "-prot" ) == 0 ||
		 strcmp(argv[0], "-p")     == 0)
	    prot = 1;
	else if (strcmp(argv[0], "-ecc"  ) == 0 ||
		 strcmp(argv[0], "-e")     == 0)
	    ecc = 1;
	else if (strcmp(argv[0], "-region")== 0 ||
		 strcmp(argv[0], "-r")     == 0) {
	    argc--, argv++;
	    if (argc == 0)
		dirmem_usage(cmd);
	    region   = 1;
	    strcpy(opt_addr, argv[0]);
	    tmpptr = opt_addr;
	    reg_low = strtoul(opt_addr, &tmpptr, 0);
	    if (tmpptr == opt_addr || (*tmpptr != NULL && *tmpptr != '-')) {
		P0("Incorrect format for -r parameter\n");
		dirmem_usage(cmd);
		return(1);
	    }
	    reg_hi = reg_low;
	    if (*tmpptr == '-')
		reg_hi = strtoul(++tmpptr, &tmpptr, 0);
	    if (*tmpptr != NULL) {
		P0("Incorrect format for -r parameter\n");
		dirmem_usage(cmd);
		return(1);
	    }
	    reg_max = memory_premium ? MAX_PREMIUM_REGIONS : MAX_NONPREMIUM_REGIONS;
	    if (reg_low <  0       || reg_hi <  0       ||
	        reg_low >= reg_max || reg_hi >= reg_max ||
		reg_low >  reg_hi ) {
		    P1("incorrect range for -r parameter 0< low|hi <%d\n", reg_max);
		    dirmem_usage(cmd);
		    return(1);
	    }
	} else if (strcmp(argv[0], "-help")  == 0 ||
                   strcmp(argv[0], "-h")     == 0) {
            dirmem_help(cmd);
            return(0);
        } else if (argv[0][0] == '-') {
            P1("Unknown flag: %s\n", argv[0]);
            dirmem_usage(cmd);
            return(1);
        } else
            break;

        argc--, argv++;
    }
    		/* The ECC bytes are not dumped yet so the -e option
		 * cannot be fully implemented at this time.
		 * If/when the dump is modified, take away this comment
		 * and delete the 'if' statement below.
		 */
    if (ecc) {
	P0("This dump contains no ECC information: copying this information\n");
	P0("from a system into the system dump has not been implemented\n");
	P0("so the '-e' option does not work.\n");
	return(1);
    }

    if (argc == 0) {
        P0("Address required\n");
	dirmem_usage(cmd);
	return(1);
    }

    strcpy(opt_addr, argv[0]);
    argc--, argv++;

    sp = kl_get_sym(opt_addr, B_TEMP);
    if (!KL_ERROR) {
            start_addr = sp->n_value;
        kl_free_sym(sp);
        if (DEBUG(DC_GLOBAL, 1)) {
            fprintf(cmd.ofp, "start_addr=0x%llx\n", start_addr);
        }
    }
    else {
        kl_get_value(opt_addr, &mode, 0, &start_addr);
        if (KL_ERROR) {
            kl_print_error();
            return(1);
        }
        /* If entered as a PFN, convert to physical address  
         */
        if (mode == 1) {
            start_addr = Ctob(start_addr);
        }
    }

    			/* if user specify any of these option
			 * set them as if all were specified
			 * this is a more friendly user default
			 */
    if (mem+prot+ecc == 0) {
	mem  = 1;
	prot = 1;
	ecc  = 0;	/* change to 1 when if/ecc dumping is implemented */
    }
     if (region == 1)
	 prot = 1;

    addr_lo   = BDDIR_ENTRY_LO(start_addr);
    addr_hi   = BDDIR_ENTRY_HI(start_addr);
    addr_prot = BDPRT_ENTRY(start_addr, 0);
    addr_ecc  = BDECC_ENTRY(start_addr);

    if (memory_premium) {
		        /* premium directory memory, system has > 16 nodes
		         */
	strcpy(dir_mem,  "PREMIUM");
        strcpy(dir_prot, "DIR_PROT_PREM");
    } else {
		        /* standard directory memory, system has <= 16 nodes
		         */
        strcpy(dir_mem,  "STANDARD");
        strcpy(dir_prot, "DIR_PROT_STD");
    }

    P0("\n");
    P1("Address: 0x%016llx\n", start_addr);
    P1("Nodes: %d\n", K_NUMNODES);
    P1("Directory memory: %s\n", dir_mem);

    			/* show
			 * HI and LO parts of Directory Memory Formats
			 */
    if (mem) {
    	if (kl_get_block(addr_lo, 8, dirmem_value_lo, "dirmem")) {
	    P1("Dir entry LO: addr 0x%016llx was not found in the dump.\n",
		    addr_lo);
	    err += 1;
        }

        if (kl_get_block(addr_hi, 8, dirmem_value_hi, "dirmem")) {
    	    P1("Dir entry HI: addr 0x%016llx was not found in the dump.\n",
		    addr_hi);
	    err += 1;
        }

	if (err) {
	    P0("Cannot show directory memory - address(es) not available.\n");
	} else {
    	    /* If this block is not a directory memory block, something is
             * very wrong, issue a message and return.
	     * Unfortunately we can know that only after having read the
	     * addr_hi and addr_lo, not before. The dir_ent_flags is set
	     * when the relevant memory block is read.
             */
	    if (dir_ent_flags & DUMP_DIRECTORY) {
		if (memory_premium) {
		    dir_state = PDIR_STATUS(value_lo);		/* status field */
		    dir_fine  = value_lo & MD_PDIR_FINE_MASK;	/* 1 = fine     */
		    						/* 0 = coarse   */

		    OneCnt    = P_ONECNT(value_lo);
		    				/* expect everything is a C-format
						 * except as modified by dir_fine
						 */
		    strcpy(dir_hi,   "DIR_PREM_C_HI");
		    strcpy(dir_lo,   "DIR_PREM_C_LO");
		    bitvec = P_BIT_VEC(value_hi, value_lo);
		    nbits  = 64;
		    mask   = 0x8000000000000000;

		    if (dir_fine) {
						/* A-format - SHARED-FINE */
			strcpy(dir_hi,  "DIR_PREM_A_HI");
			strcpy(dir_lo,  "DIR_PREM_A_LO");
			strcpy(dir_fmt, "A-format - SHARED-FINE");
			strcpy(bit_vec, "FINE_BIT_VEC");
			dir_state = 0;
		    } else if (dir_state == 0) {
						/* B-format - SHARED-COARSE */
			strcpy(dir_hi,  "DIR_PREM_B_HI");
			strcpy(dir_lo,  "DIR_PREM_B_LO");
			strcpy(dir_fmt, "B-format - SHARED-COARSE");
			strcpy(bit_vec, "COARSE_BIT_VEC");
		    }
	
		} else {
		    dir_state = SDIR_STATUS(value_lo);
		    strcpy(bit_vec, "FINE_BIT_VEC");
		    bitvec = S_BIT_VEC(value_hi, value_lo);
		    nbits  = 16;
		    mask   = 0x8000;

		    if (dir_state == 0) {
						/* A-format - SHARED */
			strcpy(dir_hi,  "DIR_STD_A_HI");
			strcpy(dir_lo,  "DIR_STD_A_LO");
			strcpy(dir_fmt, "A-format - SHARED");
		    } else {
						/* C-format */
			strcpy(dir_hi,  "DIR_STD_C_HI");
			strcpy(dir_lo,  "DIR_STD_C_LO");
		    }
		}
					/* dir_state: 0-7
    					 * case 0 is handled above
					 * 	(A or B format)
					 * case 6 does not exist
					 */
	        switch (dir_state) {
		    case 0:
			    break;
		    case 1: strcpy(dir_fmt, "C-format - POISONED");
		       	    break;
		    case 2: strcpy(dir_fmt, "C-format - EXCLUSIVE");
			    break;
		    case 3: strcpy(dir_fmt, "C-format - BUSY-SHARED");
			    break;
		    case 4: strcpy(dir_fmt, "C-format - BUSY-EXCLUSIVE");
			    break;
		    case 5: strcpy(dir_fmt, "C-format - WAIT");
			    break;
		    case 6: P0("icrash: error, dir_state=6 is undefined\n");
			    P0("This is a serious error, dump is incorrect\n");
			    return(1);
		    case 7: strcpy(dir_fmt, "C-format - UNOWNED");
			    break;
		}

		P1("Type: %s\n", dir_fmt);

		P0("\n");
        	P2("--- Dir entry HI at addr: 0x%016llx: 0x%016llx\n",
			addr_hi, value_hi);
        	if (!(r = hwreg_lookup_name(&hwreg_dir, dir_hi, 1)) == 0 ) {
        	    regset = &hwreg_dir;
        	    hwreg_decode(regset, r, opt_short, 0, opt_base, value_hi,
			    PRINTF);
		} else {
		    goto hwregerr;
		}

	        P0("\n");
        	P2("--- Dir entry LO at addr: 0x%016llx: 0x%016llx\n",
			addr_lo, value_lo);
        	if (!(r = hwreg_lookup_name(&hwreg_dir, dir_lo, 1)) == 0 ) {
        	    regset = &hwreg_dir;
        	    hwreg_decode(regset, r, opt_short, 0, opt_base, value_lo,
			    PRINTF);
		} else {
		    goto hwregerr;
		}

		/* When decoding the bit vector, remember the node numbering
		 * goes from the left (bit 63 is node 0, 62 node 1, etc...
		 * (for premium fine vect) and correspondingly for others
		 * The bit vector exists only for state=0.
		 */
		if (dir_state == 0) {
		    P0("\n");
		    P2("--- complete %s: 0x%016llx decoded:\n", bit_vec, bitvec);
		    for (j=0, i=0, k=0; i<nbits; bitvec <<= 1, ++i, ++j) {
			if (bitvec & mask) {
			    k +=1;
			    switch (j) {
				case 16: j = 0;
					 /* FALLTHROUGH */
				case  0: P0("\n ");
					 break;
				default:
					 break;
			    }
			    P1(" %3d", i);
			}
		    }
		    		/* data consistency check
				 * can be done only for premium memory and shared
				 * formats
				 */
		    if (nbits == 64 && k != OneCnt+1 ) {
			P0("\n");
			P0("--- CAREFUL: inconsistent data for the above bit vector\n");
			P2("    The count should be: %d but is: %d !!!!\n", OneCnt+1, k);
		    }

		    P0("\n");
		}

	    } else {
		goto theend;
	    }
    	}
    }

    			/* show
			 * Directory Memory Reference Count/Protection Format
			 */
    if (prot) {
	for (i=reg_low; i<=reg_hi; ++i) {
	    addr_prot = BDPRT_ENTRY(start_addr, i);
	    P0("\n");
	    P1("--- Region %d prot: ", i);

            if (kl_get_block(addr_prot, 8, dirmem_value_prot, "dirmem")) {
	    	P1("at addr 0x%016llx: was not found in the dump.\n", addr_prot);
            } else {
		if (value_prot == DUMP_HUB_INIT_PATTERN) {
		    P0(" not present in this coredump.\n");
		} else {
	    	    P2("at addr 0x%016llx: 0x%016llx\n", addr_prot, value_prot);
    	            if (!(r = hwreg_lookup_name(&hwreg_dir, dir_prot, 1)) == 0) {
    	                regset = &hwreg_dir;
    	                hwreg_decode(regset, r, opt_short, 0, opt_base, value_prot,
		                PRINTF);
	            } else {
	                goto hwregerr;
		    }
	        }
	    }
	}
    }

    			/* show
			 * ECC vector bytes
			 */
    if (ecc) {
        if (kl_get_block(addr_ecc, 8, dirmem_value_ecc, "dirmem")) {
	    P0("\n");
    	    P1("ECC: addrs 0x%016llx was not found in the dump.\n",
		    addr_ecc);
        } else {
	    P0("\n");
    	    P2("--- ECC at addr: 0x%016llx: 0x%016llx\n",
		    addr_ecc, value_ecc);
	    			/* add here the decoding when we have
				 * the 'ecc' block in the dump
				 */
	}
    }

    return(0);

    /* Something wrong with the connection to the stand/arcs/tools/hwreg
     * This is surely a serious error
     */
hwregerr:
    P0("Icrash: Register unknown, cannot determine bit fields\n");
    P0("Something very wrong with 'stand/arcs/tools/hwreg'\n");
    return(1);

	/* something desperately wrong, write a message and quit
	 */
theend:
    P0("Icrash: The block just read is not a directory memory block.\n");
    P0("Serious problem, relevant information:\n");
    P1("Dir entry HI  at addr: 0x%016llx\n", addr_hi);
    P1("Dir entry LO  at addr: 0x%016llx\n", addr_lo);
    P1("Region 0 prot at addr: 0x%016llx\n", addr_prot);
    P1("ECC           at addr: 0x%016llx\n", addr_ecc);
    return(1);
}
