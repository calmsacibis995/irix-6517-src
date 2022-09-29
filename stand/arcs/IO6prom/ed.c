#ident	"IO6prom/ed.c : $Revision: 1.17 $"

#ifndef __ED_C__
#define __ED_C__ 	1

/*
 * File: ed.c
 *
 * enable and disable command handler.
 *
 */

#include <kllibc.h>
#include <libsc.h>
#include <libsk.h>
#include <libkl.h>

#include <sys/SN/SN0/ip27config.h> 	/* Definition of SN00 */
#include <sys/SN/SN0/ip27log.h>
#include <sys/SN/kldiag.h>

#include "io6prom_private.h"
#include "ed.h"

/*
 * These are a set of static globals, that are used
 * by both enable and disable commands and their
 * various sub routines.
 */

static char     	*cmdstr ;
static int      	cmd ;		/* Enable or Disable */
static char		*slotstr ;
static int 		compt_type ;
static int 		ed_debug ;
static unsigned int 	compt_mask ;
static int		mode ;

static int 		parse_e_d_args(int, char **) ;
static void 		get_compt_index(int, lboard_t *, char *) ;
static int 		write_edtab(lboard_t *, char *) ;

#define db_printf	if (ed_debug) printf

/*
 * enable_cmd()
 *      Enables a previously disabled board or component
 *      in a specified module and slot.
 */

int
enable_cmd(int argc, char **argv)
{
    /* Just return if the command completed successfully */

    cmd = ENABLE ;
    if (parse_e_d_args(argc, argv) == 0) {
        update_checksum();
        return 0;
    }

    /* If the command failed, dump out the syntax information */
    printf("Enable syntax: enable -m MODID -s SLOTID [-cpu a|b|ab]\n");
    printf("                                         [-mem [01234567]]\n") ;
    printf("                                         [-pci [01234567]]\n") ;
    printf("   The enable command allows one to re-enable a slot\n");
    printf("   or a component in a slot which has previously been disabled.\n");
    printf("   Re-enabling does not take effect immediately; the machine \n");
    printf("   must be rebooted.\n");

    return 0;
}

/*
 * disable_cmd()
 *	Syntax: disable [-R] [-y] [slot] [unit]
 *	Disables a board or unit in a specified slot.  The -R switch 
 *	causes the command to disable recursively; enable -R 1 will
 *	try to disable all the units of the board in slot 1.  The -y
 * 	says to answer all interactively queries with a yes.
 */

int
disable_cmd(int argc, char **argv)
{
    /* Just return if the command completed successfully */

    cmd = DISABLE ;
    if (parse_e_d_args(argc, argv) == 0) {
        update_checksum();
        return 0;
    }

    /* If the command failed, dump out the syntax information */
    printf("Disable syntax: disable -m MODID -s SLOTID [-cpu a|b|ab]\n");
    printf("                                           [-mem [01234567]]\n") ;
    printf("                                           [-pci [01234567]]\n") ;
    printf("   The disable command allows one to disable a slot\n");
    printf("   or a component in a slot. Disabling does not take\n");
    printf("   effect immediately; the machine must be rebooted.\n");
    printf("   A PCI component or IO slot can be disabled and the \n") ;
    printf("   kernel will ignore that component/slot.\n") ;

    return 0;
}

/* 
 * parse_compt
 *     parse the component portion of the command
 */

static int 
parse_compt(int i, int argc, char **argv)
{
    int j ;

    if ((i+1) == argc) {
	printf("%sable: Component ID needs to be specified.\n", cmdstr) ;
	return -1 ;
    }

    if (!strcasecmp(&argv[i][1], "cpu")) {
	if ((!SN00) && (*slotstr != 'n')) {
	    printf("-cpu option works for node boards only\n") ;
	    return -1 ;
        }
	compt_type = KLSTRUCT_CPU ;
	if (strchr(argv[i+1],'a') || strchr(argv[i+1],'A'))
	    compt_mask |= (1 << 0) ;
        if (strchr(argv[i+1],'b') || strchr(argv[i+1],'B'))
            compt_mask |= (1 << 1) ;
	if (!compt_mask) {
	    printf("%sable: Unknown CPU name %s\n", cmdstr, argv[i+1]) ;
	    return -1 ;
	}
    } else if (!strcasecmp(&argv[i][1], "pci")) {
	if ((!SN00) && (*slotstr != 'i')) {
	    printf("PCI option works for IO boards only\n") ;
	    return -1 ;
	}
	compt_type = KLSTRUCT_PCI ;

        j = 0 ;
        while (argv[i+1][j]) {
            if (!(isdigit(argv[i+1][j]))) {
		 printf("Error: PCI ID must be a number.\n");
                 return -1 ;
            }
	    compt_mask |= (1 << (argv[i+1][j])-'0') ;
	    j++ ;
	}
    } else if (!strcasecmp(&argv[i][1], "mem")) {
	if ((!SN00) && (*slotstr != 'n')) {
	    printf("-mem option works for node boards only\n") ;
	    return -1 ;
	}
	compt_type = KLSTRUCT_MEMBNK ;
        j = 0 ;
        while (argv[i+1][j]) {
            if (!(isdigit(argv[i+1][j]))) {
                printf("Error: Mem Bank ID must be a number.\n");
                return -1 ;
            }
            compt_mask |= (1 << (argv[i+1][j])-'0') ;
            j++ ;
         }
    } else {
	printf("%sable: Unknown option %s\n", cmdstr, argv[i]) ;
	return -1 ;
    }
    return COMPT_MODE ;
}

/*
 * do_promlog
 *     For disable/enable of CPU and MEM do the
 *     IP27prom promlog stuff.
 *     Uses global variable 'cmd'.
 */

int
do_promlog(nasid_t nasid, char *key, char *str, unsigned char type)
{
    int rv = 0 ;
    char reason[64];

    db_printf("do_promlog: nasid %d, key %s, cmd %d, str %s\n",
	       nasid, key, cmd, str) ;

    sprintf(reason, "%sabled from BASEIO Command Monitor", (cmd == ENABLE ?
	"En" : "Dis"));

    if (!ed_debug) {
        if ((cmd == ENABLE) && 
	    ((type == KLSTRUCT_CPU) || 
	    ((type == KLSTRUCT_MEMBNK) && (!(*str))))) {
            if (!strcmp(key, DISABLE_MEM_MASK) || !strcmp(key, DISABLE_CPU_A) ||
		!strcmp(key, DISABLE_CPU_B))
                rv = ed_cpu_mem(nasid, key, NULL, reason, 0,
			(cmd == ENABLE ? 1 : 0));
            else
                rv = ip27log_unsetenv(nasid, key, 0) ;
	}
        else {
            if (!strcmp(key, DISABLE_MEM_MASK) || !strcmp(key, DISABLE_CPU_A) ||
		!strcmp(key, DISABLE_CPU_B))
		rv = ed_cpu_mem(nasid, key, str, reason, 0,
			(cmd == ENABLE ? 1 : 0));
            else
                rv = ip27log_setenv(nasid, key, str, IP27LOG_VERBOSE) ;
        }
     }

     return rv ;
}

static char md_size(short mbytes)
{
    switch(mbytes) {
        case 0:
            return MD_SIZE_EMPTY;
        case 8:
            return MD_SIZE_8MB;
        case 16:
            return MD_SIZE_16MB;
        case 32:
            return MD_SIZE_32MB;
        case 64:
            return MD_SIZE_64MB;
        case 128:
            return MD_SIZE_128MB;
        case 256:
            return MD_SIZE_256MB;
        case 512:
            return MD_SIZE_512MB;
        case 1024:
            return MD_SIZE_1GB;
	default:
            return MD_SIZE_EMPTY;
    }
}

void
disable_cpus(unsigned int compt_mask, lboard_t *rlb)
{
    char str[32], *key = 0 ;

    sprintf(str, "%d: CPU disabled from IO6 POD.", KLDIAG_CPU_DISABLED);

#if 0
        /* XXX Add a warning if this is the last CPU */
        /* This works only if we update klcfg in addn to promlog */
        if (kl_get_num_cpus() == 1) {
            printf("Warning: Cannot disable the last CPU in the system.\n") ;
            return 0 ;
        }
#endif
        if (compt_mask & 0x1) {
            key = DISABLE_CPU_A ;
            do_promlog(rlb->brd_nasid, key, str, KLSTRUCT_CPU) ;
        }
        if (compt_mask & 0x2) {
            key = DISABLE_CPU_B ;
            do_promlog(rlb->brd_nasid, key, str, KLSTRUCT_CPU) ;
        }
}

int
plog_swapbank(nasid_t n, int b)
{
    char 	*key = SWAP_BANK ;
    char  	str[MD_MEM_BANKS+1] ;
    int   	rv ;

    str[0] = (b ? ('0' + b) : 0) ;
    str[1] = 0 ;

    rv = do_promlog(n, key, str, KLSTRUCT_MEMBNK) ;

    return rv ;
}

void
disable_memory(unsigned int compt_mask, lboard_t *rlb, int flag)
{
	char 		dis_mem_mask[32], 
			dis_mem_sz[32], 
			dis_mem_reason[32];
	unsigned int 	tmp_mask = 0 ;
        int  		next = 0, k, i ;
	char		str[32], *key = 0 ;

	/* Get the 3 plog vars that define state of memory banks */

	ip27log_getenv(rlb->brd_nasid, DISABLE_MEM_SIZE, dis_mem_sz, 
		DEFAULT_ZERO_STRING, 0);
	ip27log_getenv(rlb->brd_nasid, DISABLE_MEM_REASON, dis_mem_reason, 
		DEFAULT_ZERO_STRING, 0);
	ip27log_getenv(rlb->brd_nasid, DISABLE_MEM_MASK, dis_mem_mask, 
		NULL, 0) ;

	/* create a mask of already disabled banks */

	for (k = 0; k < strlen(dis_mem_mask); k++)
	    tmp_mask |= (1 << (dis_mem_mask[k] - '0')) ;

	if (cmd == ENABLE) {
	    for (k = 0; k < MD_MEM_BANKS; k++) {
		if ((k == 0) && (compt_mask & (1<<0))) {
		    plog_swapbank(rlb->brd_nasid, 0) ;
		}    
	        if (compt_mask & (1 << k)) {
	            dis_mem_sz[k] = '0';
                    dis_mem_reason[k] = '0';
                }
	    }
	    compt_mask = tmp_mask & (~compt_mask) ;
	}
	else if (cmd == DISABLE) {
	    klmembnk_t	*mem_ptr = (klmembnk_t *) find_first_component(rlb, 
			KLSTRUCT_MEMBNK);

	/*
         * Disable only if it not already disabled and (bank_size != 0)
         */

	    if (mem_ptr) {	/* found a mem bank on this brd */
		int msize = 0 ;

	        for (k=0; k < MD_MEM_BANKS; k++)
	            if ((compt_mask & (1 << k)) &&   /* this bank selected */
			(!(tmp_mask & (1 << k))) &&  /* not already disabled */
			((msize = md_size(mem_ptr->membnk_bnksz[k])) != 
					MD_SIZE_EMPTY)) { /* mem present*/
			/* 
			 * To disable bank0, bank1 should exist,
			 * not already disabled. And this is not
			 * a disable brd cmd(flag) 
			 */
			    if ((k==0) && (!flag)) {
				if (	(md_size(mem_ptr->membnk_bnksz[1])
							== MD_SIZE_EMPTY) ||
					(tmp_mask & (1<<1)) ||
					(compt_mask & (1<<1))) {
		        	    compt_mask &= ~(1<<k) ;
				    continue ;
				}
				plog_swapbank(rlb->brd_nasid, 2) ;
			    }
		            dis_mem_sz[k] = '0' + msize ;
			    dis_mem_reason[k] = '0' + MEM_DISABLE_USR;
			}
		    	else /* Mask this bank as an extra precaution */
			     /* Takes care of banks given on cmd line
				but do not physically exist */
		        	compt_mask &= ~(1<<k) ;
	    }

	    compt_mask |= tmp_mask;	/* Add to list of banks to disable */
	}

	/* Build the ascii mask from cmd line args + existing mask */

	for (i=0; i < MD_MEM_BANKS; i++)
	    if (compt_mask & (1<<i))
		str[next++] = i + '0' ;
	str[next] = 0 ;

	/* Put all the respective variables in promlog */

	key = DISABLE_MEM_MASK ;
	do_promlog(rlb->brd_nasid, key, str, KLSTRUCT_MEMBNK) ;
	key = DISABLE_MEM_SIZE ;
	do_promlog(rlb->brd_nasid, key, dis_mem_sz, 
		KLSTRUCT_MEMBNK) ;
	key = DISABLE_MEM_REASON ;
	do_promlog(rlb->brd_nasid, key, dis_mem_reason, 
		KLSTRUCT_MEMBNK) ;
}

void
disable_node_brd(lboard_t *lb)
{
	unsigned int 	mask ;
	char		*str = STRING_ONE ;
	char		buf[128] ;

	printf("Disabling the last node board may prevent ") ;
	printf("system from booting.\n") ; 

	while (1) {
		printf("OK to disable? [y/n]: ") ;
		gets(buf) ;
		if ((*buf != 'y') && (*buf != 'n'))
			continue ;
		if (*buf != 'y') {
			printf("Not disabling node board.\n") ;
			return ;
		}
		else
			break ;
	}

	make_hwg_path(lb->brd_module, lb->brd_slot, buf) ;
	str = buf ;

	mask = 3 ; 		/* disable cpus A and B */
	disable_cpus(mask, lb) ;

	/* disable all memory banks, 
	 * flag = 1 means disable Bank 0 also.
	 */
	mask = 0xff ;
	disable_memory(mask, lb, 1) ; 

}

void
enable_node_brd(lboard_t *lb)
{
        unsigned int    mask ;
        char            str[10] ;

        mask = 3 ;              /* enable cpus A and B */
        disable_cpus(mask, lb) ;

        /* enable all memory banks,
         * flag = 1 means disable Bank 0 also.
         */
        mask = 0xff ;
        disable_memory(mask, lb, 1) ;

}

/*
 * get_my_node_slot_class
 *     visit_lboard callback.
 */

int
get_my_node_slot_class(lboard_t *lb, void *ap, void *rp)
{
    unchar      *nsclass = (unchar *)rp ;

    if (lb->brd_type == KLTYPE_IP27) {
      *nsclass = SLOTNUM_GETCLASS(lb->brd_slot) ;
      return 0 ;
    }

    return 1 ;
}

/*
 * parse_e_d_args
 *     main parse routine for enable/disable arguments
 *     Do sanity checks.
 *     Call the respective functions for CPU, MEM and IO.
 */

int
parse_e_d_args(int argc, char **argv)
{
    int 	i, j ;
    int 	modid, slotid ;
    int		brd_class = KLCLASS_UNKNOWN ;
    lboard_t 	lb, *rlb = 0 ;
    char 	*key ;   	/* key string for promlog */
    char 	str[10] ;	/* key string value */
    char	state[MAX_COMPTS_PER_BRD] ;	/* enable or disable */

    /* Init globals */

    cmdstr = (cmd == ENABLE) ? "en" : "dis" ;
    modid  = slotid 	= -1 ;

    mode 	= UNKNOWN_MODE ;
    slotstr 	= 0 ;
    bzero(state, MAX_COMPTS_PER_BRD) ;

    compt_type 	= -1 ;
    compt_mask 	= 0 ;
    ed_debug 	= 0  ;

    /* Check arguments */

    if (argc < 2)                   /* too few */
        return -1 ;

    for (i = 1; i < argc; i++)
	if (!strcmp(argv[i], "-m")) {
	    if (++i == argc) {
		printf("Module id must be specified \n") ;
		return -1 ;
	    }

	    if (!(isdigit(argv[i][0]))) {
		printf("Module id must be a number\n");
		return -1 ;
	    }

	    modid = atoi(argv[i]) ;
	} else if (!strcmp(argv[i], "-s")) {
	    if (++i == argc) {
		printf("Slot id must be specified\n") ;
		return -1 ;
	    }

	    if (!strcmp(argv[i], "MotherBoard")) {
		slotid =  0 ;
	    } else {
		if (strncmp(argv[i], "io", 2) &&
		    (argv[i][0] != 'n')) {
		    printf("Slotname must begin with %s\n",
			   SN00 ? "'MotherBoard'" : "'io' or 'n'");
		    return -1 ;
		}

		if (argv[i][0] == 'i')
		    slotid = atoi(&argv[i][2]) ;
		else
		    slotid = atoi(&argv[i][1]) ;
	    }

	    slotstr = argv[i] ;
	} else if (!strcmp(argv[i], "-d"))
	    ed_debug = 1 ;
	else if (argv[i][0] != '-') {
	    printf("Unknown argument: %s\n", argv[i]);
	    return -1;
	} else if (!slotstr) {
	    printf("Slot id must be specified\n") ;
	    return -1 ;
	} else if ((mode = parse_compt(i, argc, argv)) == -1)
	    return -1 ;
	else
	    break;

    /* Sanity checks */
    if (modid == -1) {
	printf("Module id must be specified\n") ;
	return -1 ;
    }

    if (slotid == -1) {
	printf("Slot id must be specified.\n") ;
	return -1 ;
    }
  
    /* Get the mode of operation */
    if (mode==UNKNOWN_MODE) 	/* component mode not set */
        mode = SLOT_MODE ;

    /*
     * If 'MotherBoard' or nothing were specified,
     * this is the only way to know if we are dealing
     * with IO or NODE board part of IP29.
     */
    if (SN00) {
        if ((compt_type == KLSTRUCT_CPU) ||
            (compt_type == KLSTRUCT_MEMBNK))
                brd_class = KLCLASS_CPU ;
        else if (compt_type == KLSTRUCT_PCI)
                brd_class = KLCLASS_IO  ;
        else
		/* If nothing were specified we have to
		   disable the entire 'MotherBoard'.
		   Assume it is the CPU portion of the
		   Mother board.
		*/
                brd_class = KLCLASS_CPU ;
    } else {
        if (*slotstr == 'n')
                brd_class = KLCLASS_CPU ;
        else if (*slotstr == 'i')
                brd_class = KLCLASS_IO ;
        else
                brd_class = KLCLASS_UNKNOWN ;
    }

#if 0
    if ((mode == SLOT_MODE) && (brd_class == KLCLASS_CPU)) {
	printf("%sable: Cannot %sable CPU boards.\
 Try individual components\n", cmdstr, cmdstr) ;
	return -1 ;
    }
#endif

    /* Some other range checks */
    if ((brd_class == KLCLASS_IO) && (slotid >12)) {
	printf("%sable: Invalid slotid %d.\n", cmdstr, slotid) ;
	return -1 ;
    }

    /* The input is a set of banks of the form 01234... Check if
     * there are any illegal banks in them
     */
    if ((compt_type == KLSTRUCT_MEMBNK) && (MD_MEM_BANKS < 10)) {
        int	i;

        for (i = MD_MEM_BANKS; i < 10; i++)
            if (compt_mask & (1 << i)) {
                printf("ERROR: %d is not a valid MEMBANK.\n", i);
                printf("ERROR: Only MEMBANK(s) 0-%d are valid.\n", 
				MD_MEM_BANKS - 1);
                return 0;
            }           
    }

    db_printf("%sable: m = %d, s = %x, mask = %x\n", 
	       cmdstr, modid, slotid, compt_mask) ;

    /* fill a dummy lboard struct with the info needed and 
       call visit_lboard */

    {
        unchar  nsclass = 0 ;

        visit_lboard(get_my_node_slot_class, NULL, (void *)&nsclass) ;

        lb.brd_type   = brd_class | slotid ; /* need for SN00 */
        lb.brd_module = modid ;
        lb.brd_slot   = (brd_class == KLCLASS_IO) ? 
                      (SLOTNUM_XTALK_CLASS|slotid) :
                      (nsclass|slotid) ;
    }

    rlb = get_match_lb(&lb, 1) ;
    if (!rlb) {
	printf("%sable: Cannot find board in module %d slot %s.\n",
	        cmdstr, modid, slotstr) ;
	return -1 ;
    }

    /* Act on input data, go ahead and enable/disable components */
    /* input - modid, slotid, mode, compt_mask, compt_type, brd_class */

    if ((mode == SLOT_MODE) && (brd_class == KLCLASS_CPU)) {
	if (cmd == DISABLE)
	    disable_node_brd(rlb) ;
	else
	    enable_node_brd(rlb) ;
	return 0 ;
    }

    if (compt_type == KLSTRUCT_CPU) {
	disable_cpus(compt_mask, rlb) ;
	return 0 ;
    }

    /*
     * Read the variable(s) from the ip27log. Disable/enable add'l banks
     */
    if (compt_type == KLSTRUCT_MEMBNK) {
	disable_memory(compt_mask, rlb, 0) ;
	return 0 ;
    }

    if ((compt_type == KLSTRUCT_PCI) || (mode == SLOT_MODE)) {
	for (j=0; j<MAX_PCI_DEVS; j++) {
	    if (compt_mask & (1<<j))
	    	get_compt_index(j, rlb, state) ;
	}
        db_printf("write_edtab: mode = %d state = ", mode) ;
        for (i=0;i<8;i++)
	    db_printf("%x", state[i]) ;
	db_printf("\n") ;
	write_edtab(rlb, state) ;
    }

    return 0 ;
}

void
get_compt_index(int npci, lboard_t *lb, char *state)
{
    int 	i ;
    klinfo_t	*k ;

    for (i=0; i<lb->brd_numcompts; i++) {
	 k = (klinfo_t *)(NODE_OFFSET_TO_K1(
                                lb->brd_nasid,
                                lb->brd_compts[i])) ;
	if (k->struct_type == KLSTRUCT_BRI)
		    continue ;
	if (k->physid == npci)
	    state[i] = (cmd == DISABLE)?
			NVEDRSTATE_DISABLE:NVEDRSTATE_ENABLE ;
    }
}

/* ---------------------------------------------------------------*/

/* 
 * ed - enable, disable table management routines
 * The nvram has 4K of space reserved for INVENTORY INFO. 
 * This space will be used to maintain a enable/disable
 * table for the system.
 * The edtab - ed table - is a sequence of records
 * indexed on a moduleid slotid key. Each record in 
 * this area represents a 'disabled' board. There can 
 * holes in this sequence when a previously disabled
 * board is enabled.
 * This scheme uses structures instead of individual
 * nvram offsets for each field. This makes programming
 * easier, but may result in loss of space due to struct
 * alignment and difficult to write individual bytes.
 */

void
predh(edtabhdr_t *edh)
{
    db_printf("magic = %x	valid = %x	cnt = %d	nic = %x\n",
            edh->magic, edh->valid, edh->cnt, edh->nic) ;
}

void
predr(edrec_t *edr, int i)
{
    int j ;

    db_printf(" %d -> 	%x 	%d 	%x	",
            i, edr->flag, edr->modid, edr->slotid) ; 
    for (j = 0; j < 8; j++) 
        db_printf("%x", edr->state[j]) ;
    db_printf("\n") ;
}

/*
 * update_edinfo
 *     update lboard with disable info
 *     if board is disabled do not check components.
 */

update_edinfo(lboard_t *l, void *ap, void *rp)
{
    edrec_t	*edr = (edrec_t	*)ap ;
    klinfo_t	*k ;
    int		i ;
    int	 	*rvp = (int *)rp ;

    if ((edr->modid == l->brd_module) &&
	(edr->slotid == l->brd_slot)) {

        /* Do not disable if it is the Global Master BaseIO */
	if ((l->brd_flags & GLOBAL_MASTER_IO6) && 
	   /* (edr->flag & NVEDREC_BRDDIS) && */ (cmd == DISABLE)) {
	    printf("Cannot disable the Master IO6.\n") ;
	    *rvp = 1 ;
	    return 0 ;
	}

	for (i=0;i<l->brd_numcompts;i++) {
	    k = KLCF_COMP(l, i) ;
	    if ((edr->flag & NVEDREC_BRDDIS) && (mode == SLOT_MODE)) {
		if (!ed_debug) 
		    if (cmd == DISABLE)
		        k->flags &= ~KLINFO_ENABLE ;
		    else if (cmd == ENABLE)
                        k->flags |= KLINFO_ENABLE ;

                edr->state[i] = 0 ;
		continue ;
	    }
	    if (edr->state[i] == NVEDRSTATE_DISABLE) { /*disabled */
		db_printf("disabling compt, m = %d, slot = %x id = %d\n",
			   l->brd_module, l->brd_slot, i) ;
		if (!ed_debug)
		    k->flags &= ~KLINFO_ENABLE ;
	    } else if (edr->state[i] == NVEDRSTATE_ENABLE) {
		db_printf("enabling compt, m = %d, slot = %x id = %d\n",
			   l->brd_module, l->brd_slot, i) ;
		if (!ed_debug)
		    k->flags |= KLINFO_ENABLE ;
		edr->state[i] = 0 ;
	    }
	}

        if (edr->flag & NVEDREC_BRDDIS) {
	  if (mode == SLOT_MODE) {
            if (cmd == DISABLE)
                if (!ed_debug)
                    l->brd_flags &= ~ENABLE_BOARD ;
            if (cmd == ENABLE) {
                if (!ed_debug)
                    l->brd_flags |= ENABLE_BOARD ;
                edr->flag &= ~NVEDREC_BRDDIS ;
                *rvp = -1 ;
            }
            db_printf("%sabling board, m = %d, slot = %x\n", cmdstr,
                       l->brd_module, l->brd_slot) ;
	  }
        }

	return 0 ;
    }
    return 1 ; /*continue to search */
}

/*
 * init_edh
 *     init ed header with defaults
 */

void
init_edh(edtabhdr_t *edh)
{
    int i ;

    /* clear the nvram inventory area */

    for (i=NVOFF_EDHDR;i<NVOFF_EDHDR+NVLEN_EDHDR;i++)
        set_nvreg(i,0) ;

    edh->magic = NVEDTAB_MAGIC ;
    edh->valid = NVEDTAB_VALID ;
    edh->cnt   = 0 ;
    edh->nic   = get_sys_nic() ;
    db_printf("get_sys_nic: sys nic %lx\n", edh->nic) ;
    cpu_set_nvram_offset(NVOFF_EDHDR, sizeof(edtabhdr_t), (char *)edh) ;
    update_checksum();

    if (ed_debug) {
        cpu_get_nvram_buf(NVOFF_EDHDR, sizeof(edtabhdr_t), (char *)edh) ;
        db_printf("new header written\n") ;
        predh(edh) ;
    }
}

/*
 * check_edtab
 *     Check all entries in the table and update the
 *     klconfig info, disabling components/boards.
 * Called:
 *     during io6prom, after gda is ready, before
 *     init_prom_soft, after nvram_base is initted.
 *     If the edtab is not initted, init it first.
 */

check_edtab()
{
    edtabhdr_t	*edh, edhs ;
    edrec_t	edrs, *edr ;
    int 	i, j, result ;

    db_printf("Magic offset : %x\n", NVOFF_EDHDR) ;

    edh = &edhs ;
    /* check header validity. If invalid, write a new header */
    cpu_get_nvram_buf(NVOFF_EDHDR, sizeof(edtabhdr_t), (char *)edh) ;

    predh(edh) ;

    if (edh->magic != NVEDTAB_MAGIC) {
        init_edh(edh) ;
	return 0 ;
    }

    if (!(edh->valid)) {
        db_printf("check_edtab: Header not valid \n") ;
        /* Might have trashed it while writing */
        init_edh(edh) ;
	return 0 ;
    }

    /* check if this edtab belongs to this system */
    /* XXX if ((edh->nic) && (!check_sys_nic(edh->nic))) { */
    if (!check_sys_nic(edh->nic)) {
	db_printf("This nvram did not previously belong to this system\n") ;
        init_edh(edh) ;
	return 0 ;
    }

    /* Scan all records and update klconfig */

    db_printf("check_edtab: num entries %d\n", edh->cnt) ;
    edr = &edrs ;
    for (i=0; i<edh->cnt; i++) {
	cpu_get_nvram_buf((NVOFF_EDREC + (i*sizeof(edrec_t))),
			  sizeof(edrec_t), (char *)edr) ;
        predr(edr, i) ;
	if (!(edr->flag & NVEDREC_VALID))
	    continue ;
	visit_lboard(update_edinfo, (void *)edr, &result) ;
    }
    return 1 ;
}

/*
 * write_edtab
 *     Write a ed record on user's request.
 */

write_edtab(lboard_t *l, char *state)
{
    edtabhdr_t	*edh, edhs ;
    edrec_t	*edr, edrs ;
    int 	i, j, new = 0, result = 0, end_of_rec = 0 ;
    int 	match = 0, match_found = 0 ;
    int		invalid = 0, invalid_found = 0 ;

    /* Get the header */
    cpu_get_nvram_buf(NVOFF_INVENTORY, sizeof(edtabhdr_t), (char *)&edhs) ;
    edh = &edhs ;

    predh(edh) ;

    /* check for validity */
    if ((edh->magic != NVEDTAB_MAGIC) ||
        (!(edh->valid))) {
	printf("Enable/Disable table not setup in NVRAM\n") ;
	return 0 ;
    }

    if (!check_sys_nic(edh->nic)) {
	printf("The NVRAM did not previously belong to this system.\n") ;
	return 0 ;
    }

    /* Basic check before proceeding */
    if ((cmd == ENABLE) && (!edh->cnt)) {
        printf("Nothing disabled.\n") ;
        return 0 ;
    }

    /* invalidate header before writing */
    edh->valid &= ~NVEDTAB_VALID ;
    cpu_set_nvram_offset(NVOFF_INVENTORY, sizeof(edtabhdr_t), (char *)edh) ;

    /* scan for the first free slot or go to the end */
    edr = &edrs ;
    bzero((char *)edr, sizeof(edrec_t)) ;
    for (i=0; i<edh->cnt; i++) {
	cpu_get_nvram_buf(NVOFF_EDREC + (i*sizeof(edrec_t)), 
			sizeof(edrec_t), (char *)&edrs) ;
        predr(edr, i) ;
	if (!(edr->flag & NVEDREC_VALID)) {
	    invalid = i ; 
	    invalid_found = 1 ;
	    continue ;
	}
	if ((l->brd_module == edr->modid) &&
	    (l->brd_slot   == edr->slotid)) {
		match_found = 1 ;
		match = i ;
		break ;
	 }
    }
    if ((edh->cnt == i) && (!invalid_found)) {
        end_of_rec = 1 ;
        new = i ;
    }

    if (cmd == DISABLE) {
	if (match_found)
	    new = match ;
	else if (invalid_found)
	    new = invalid ;
    } 

    if (cmd == ENABLE) 
	if (!match_found) {
            printf("enable: Given component/board not previously disabled\n") ;
            goto validate_hdr ;
        } else {
	    new = match ;
	}

    db_printf("write_edtab: found record %d\n", new) ;

    if (!match_found)
	bzero(edr->state, MAX_COMPTS_PER_BRD) ;

    /* fill up a edr record in memory */
    edr->modid 	= l->brd_module ;
    edr->slotid = l->brd_slot ;
    if (mode == SLOT_MODE)
	edr->flag |= NVEDREC_BRDDIS ;
    else
    for(i=0;i<l->brd_numcompts;i++)
	if (state[i]) {
            db_printf("%sabling compt - mod %d, slot %x, id %d\n", 
			cmdstr, l->brd_module, l->brd_slot, i) ;
	    edr->state[i] = state[i] ;
	}

    if (cmd == DISABLE)
        edr->flag |= NVEDREC_VALID ;
    else if ((cmd == ENABLE) && (!(edr->flag & NVEDREC_BRDDIS))) {
	for (j = 0; j < l->brd_numcompts; j++)
		if (edr->state[j] == NVEDRSTATE_DISABLE) break ;
	if (j == l->brd_numcompts) {
        	edr->flag &= ~NVEDREC_VALID ;
		if (new == (edh->cnt - 1)) /* last record */
		    result = -1 ;
	}
    }

    db_printf("writing klconfig ...\n") ;
    visit_lboard(update_edinfo, (void *)edr, &result) ;
    if (result == 1)
	goto validate_hdr ;

    if ((cmd == ENABLE) && (mode ==  SLOT_MODE))
	edr->flag &= ~NVEDREC_VALID ;

    if (end_of_rec)
	edh->cnt++ ;

    edh->cnt += result ;
    db_printf("writing record ...\n") ;
    predr(edr, new) ;

    cpu_set_nvram_offset(NVOFF_EDREC + (new*sizeof(edrec_t)),
				sizeof(edrec_t), (char *)&edrs) ;
validate_hdr:
    edh->valid |= NVEDTAB_VALID ;
    /* Before writing the header check all records once */
    for (i=0;i<edh->cnt;i++) {
        cpu_get_nvram_buf(NVOFF_EDREC + (i*sizeof(edrec_t)),
                        sizeof(edrec_t), (char *)&edrs) ;
        if (edr->flag & NVEDREC_VALID) break ;
    }
    if (i == edh->cnt) /* we came to the 'end' without finding a valid rec */
	edh->cnt = 0 ;

    predh(&edhs) ;
    cpu_set_nvram_offset(NVOFF_INVENTORY, sizeof(edtabhdr_t), (char *)&edhs) ;
    return 1 ;
}

/* 
 * Checks the disable io component table in the nvram
 * and sets up appropriate entries in the edtab
 */

void
nvram_disabled_io_info_update()
{
	unchar		index;
	lboard_t	board;
	dict_entry_t	dict;
	char		state[MAX_COMPTS_PER_BRD];

	/* If the table is empty we have nothing to do */
	if (nvram_dict_size() == 0) {
		return;
	}
	/* There is at least one valid disabled io component
	 * entry in the nvram table.
	 */
	index = 0;
	while (index < MAX_DISABLEDIO_COMPS) {
		/* Initialize state to all 0s */
		bzero(state,MAX_COMPTS_PER_BRD);

		/* Get the index next valid disabled io component entry */
		index = nvram_dict_next_index_get(index);
		if (index >= MAX_DISABLEDIO_COMPS)
			break;
		/* Read the entry */
		nvram_dict_index_get(&dict,index);
		/* fill in the appropriate info in the
		 * dummy board structure
		 */
		printf("Disabling dict [%d , %d , %d]\n",
		       dict.module,dict.slot,dict.comp);
		board.brd_module 	= dict.module;
		board.brd_slot 		= dict.slot;
		board.brd_numcompts	= MAX_COMPTS_PER_BRD;
		/* Set the state byte for the appropriate pci component */
		state[dict.comp] 	= NVEDRSTATE_DISABLE;
		/* Write an entry into the edtab */
		write_edtab(&board,state);
		/* Remove the entry from the disabled io component
		 * table
		 */
		nvram_dict_index_remove(index);
		index++;
	}

}
#endif /* __ED_C__ */





 

