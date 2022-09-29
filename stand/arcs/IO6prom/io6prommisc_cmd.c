#ident "IO6prom/io6prommisc_cmd.c : $Revision: 1.14 $"

/*
 * This file on SN0 contains the support for miscellaneous commands
 * like printint serial numbers, dump klcfg info, check system clock...
 */

#include <libsc.h>
#include <libsk.h>
#include <libkl.h>
#include <arcs/errno.h>
#include <rtc.h>
#include <sn0hinv.h>
#include <sys/SN/nvram.h>
#include <sys/SN/gda.h>

#include <sys/SN/SN0/ip27log.h>
#include <sys/SN/SN0/ip27config.h> 	/* definition of SN00 */
#include <ksys/elsc.h>
#include <klcfg_router.h>

#include "io6prom_private.h"

extern int 	prmod_snum(lboard_t *, void *, void *) ;
extern int	launch_set_module(lboard_t *ip27_ptr, moduleid_t module);

static int 	serial_is_settable = 0;

#ifdef INV_DEBUG
#define	IDPRT	printf
#else
#define	IDPRT	if (NULL) printf
#endif

/*
 * 2 routines to implement the move module command.
 */

static lboard_t *get_ip27(moduleid_t module)
{
        cnodeid_t       cnode;
        int             found_new_module = 0;
        gda_t           *gdap = (gda_t *) GDA_ADDR(get_nasid());
        lboard_t        *ip27_ptr;

        for (cnode = 0; cnode < MAX_COMPACT_NODES; cnode++) {
                nasid_t         nasid;

                nasid = gdap->g_nasidtable[cnode];

                if (nasid == INVALID_NASID)
                        continue;

                if (!(ip27_ptr = find_lboard((lboard_t *)
                        KL_CONFIG_INFO(nasid), KLTYPE_IP27))) {
                        IDPRT("WARNING: No IP27 on nasid %d\n", nasid);
                        continue;
                }

                if (ip27_ptr->brd_module == module) {
                        found_new_module = 1;
                        break;
                }
        }

        if (found_new_module)
                return ip27_ptr;
        else
                return NULL;
}

int mvmodule_cmd(int argc, char **argv)
{
        lboard_t        *ip27_ptr, *mod_brds[NODESLOTS_PER_MODULE];
        moduleid_t      old_module, new_module;
        char            mod_buf[8];
        int             i, this_module;

        if (argc != 3)
                return -1;

        old_module = atoi(argv[1]);
        new_module = atoi(argv[2]);

        if (ip27_ptr = get_ip27(new_module)) {
                char    buf[16];

                printf("Moduleid %d is already in use. Continue [n] ? ",
                        new_module);

                gets(buf);

                if ((buf[0] != 'y') && (buf[0] != 'Y'))
                        return 0;
        }

        if (!(ip27_ptr = get_ip27(old_module))) {
                printf("ERROR: Cannot find a IP27 board in module %d\n",
                        old_module);
                return 0;
        }

        if (module_brds(ip27_ptr->brd_nasid, mod_brds,
                        NODESLOTS_PER_MODULE) < 0) {
                IDPRT("WARNING: Cannot get all IP27 boards in module nasid "
                        "%d\n", ip27_ptr->brd_nasid);
                return 0;
        }

        sprintf(mod_buf, "%d", new_module);

	if (SN00) {
		for (i = 0; i < NODESLOTS_PER_MODULE; i++)
			if (mod_brds[i])
                                ip27log_setenv(mod_brds[i]->brd_nasid,
                                        IP27LOG_MODULE_KEY, mod_buf, 0);
		return 0;
	}

	for (i = 0; i < NODESLOTS_PER_MODULE; i++)
		if (mod_brds[i] && (mod_brds[i]->brd_nasid == get_nasid())) {
			if (elsc_module_set(get_elsc(), new_module) < 0)
				printf("ERROR: Setting moduleid at the MSC.\n");
			return 0;
		}
				

	for (i = 0; i < NODESLOTS_PER_MODULE; i++)
		if (mod_brds[i] && (launch_set_module(mod_brds[i], new_module) 
			>= 0))
			break;
        return 0;
}

#include <arcs/time.h>

/*
 * This is useful to check the tod routine and clocks.
 * It is here for test only. serial is a convninent command
 * to put it in instead of putting it as another new command.
 * checkclk would have been good too.
 */

void
check_tod()
{
	TIMEINFO t ;
	extern void cpu_get_tod(TIMEINFO *) ;
	cpu_get_tod(&t) ;
	printf("The time now is ... \n") ;
	printf("cpu_get_tod: %d:%d:%d %d/%d/%d\n",
		t.Hour, t.Minutes, t.Seconds, t.Year, t.Day, t.Month) ;
}

/* routine to corrupt nvram so that we can test fixes. */

void
corrupt_nvram()
{
  	int i ;
	for(i=0;i<NVLEN_SWAP;i++)
		set_nvreg(NVOFF_SWAP+i, 0xff) ;
	update_checksum() ;

}

/*
 * serial_cmd()
 *      Allows the user to display the system serial number.
 */

int
serial_cmd(int argc, char **argv)
{
#if 0
	check_tod() ;
	corrupt_nvram() ;
#endif

    visit_lboard(prmod_snum, NULL, NULL) ;
    return 0;
}

/*
 * ERASE NVRAM Support
 */

#define NVERASE_BEGIN   NVOFF_REVISION
#define NVERASE_END     NVLEN_MAX

#define NVERASE_HELP    1
#define NVERASE_CHECK   2
#define NVERASE_WRITE   3

char *erasenvram_usage =
"Erases all the non-volatile memory in the system.\n"
"To check contents of the NVRAMs, add the following\n"
"Base addresses and offsets from 0xE to 0x7FF7 and \n"
"use 'g -b 0x<ADDR>' to look at the contents\n"
;
char *msg1 = "/hw/module/%d/slot/%s: " ;
char *msg2 = "Base 0x%x " ;
char *msg3 = "Offset 0x%x non-zero Value 0x%x";

#define PRINT_MSG1 printf(msg1, l->brd_module, sname) ;

int
do_nvram(__psunsigned_t nvb, int flag)
{
    int         i = 0 ;
    for (i = NVERASE_BEGIN; i < NVERASE_END; i++) {
        if (flag == NVERASE_WRITE)
            slv_set_nvreg(nvb, i, 0) ;
        else if (flag == NVERASE_CHECK) {
            if (slv_get_nvreg(nvb, i))
                return i ;
        }
    }
    return 0 ;
}

int
erase_nvram_lb(lboard_t *l, void *ap, void *rp)
{
    int                 i = 0 ;
    int                 flag = *(int *)ap ;
    __psunsigned_t      nvb ;
    char                sname[SLOTNUM_MAXLENGTH] ;
    char                nvbyte ;

    if (l->brd_type != KLTYPE_BASEIO)
        return 1 ;

    nvb = getnv_base_lb(l) ;
    get_slotname(l->brd_slot, sname) ;

    if (flag == NVERASE_HELP) {
        PRINT_MSG1
        printf(msg2, nvb) ;
        printf("\n") ;
        return 1 ;
    }

    if (flag == NVERASE_CHECK) {
        i = do_nvram(nvb, flag) ;
        if (i) {
            nvbyte = slv_get_nvreg(nvb, i) ;
            PRINT_MSG1
            printf(msg2, nvb) ;
            printf(msg3, i, nvbyte) ;
            printf("\n") ;
            return 1 ;
        }
        PRINT_MSG1
        printf("NVRAM is zeroed.") ;
        printf("\n") ;
        return 1 ;
    }

    if (flag == NVERASE_WRITE) {
        do_nvram(nvb, flag) ;
        PRINT_MSG1
        printf(" NVRAM Erased.") ;
        printf("\n") ;
        return 1 ;
    }

    return 1 ;
}

int
erasenvram_cmd(int argc, char **argv)
{
    int         i = 0 ;
    char        ans ;
    char        buf[16] ;
    int         flag = 0 ;

    if (argv[1] && (!strcmp(argv[1], "-help"))) {
        printf("%s", erasenvram_usage) ;
        flag = NVERASE_HELP ;
    }

    if (argv[1] && (!strcmp(argv[1], "-check"))) {
        flag = NVERASE_CHECK ;
    }

    if (argc == 1) {
        printf("OK to erase all NVRAM (y/n) [n]:") ;
        gets(buf) ;
        if (buf[0] == '\0')
            ans = 'n' ;
        else
            ans = buf[0] ;

        if (ans != 'y') {
            printf("Not erasing any NVRAM\n") ;
            return 0 ;
        }

        flag = NVERASE_WRITE ;
    } else if (!flag) {
	printf("%s: Unknown options ignored, nvram not erased\n", argv[0]) ;
	return 0 ;
    }
    
    visit_lboard(erase_nvram_lb, (void *)&flag, NULL) ;
    return 0;
}

/*
 * End ERASE NVRAM support.
 */

/*
 * dumpklcfg_cmd
 * dumps klconfig info. Needed for internal debugging.
 */

int
dumpklcfg_cmd()
{
    cnodeid_t       cnode ;
    nasid_t         nasid ;
    lboard_t        *lbptr ;
    gda_t           *gdap;

    gdap = (gda_t *)GDA_ADDR(get_nasid());
    if (gdap->g_magic != GDA_MAGIC) {
        printf("dumpklcfg_cmd: Invalid GDA MAGIC\n") ;
        return 0 ;
    }

    for (cnode = 0; cnode < MAX_COMPACT_NODES; cnode ++) {
        nasid = gdap->g_nasidtable[cnode];

        if (nasid == INVALID_NASID)
            continue;

        printf("Nasid %d:", nasid) ;

        lbptr = (lboard_t *)KL_CONFIG_INFO(nasid) ;

        while (lbptr) {
            printf("(%s,%d,%d,%d,%d,%x", BOARD_NAME(lbptr->brd_type), 
			lbptr->brd_nasid, lbptr->brd_module, 
			lbptr->brd_partition, SLOTNUM_GETSLOT(lbptr->brd_slot),
			(lbptr->brd_nic+1)) ;
            if (lbptr->brd_flags & DUPLICATE_BOARD)
	        printf("-D") ;
    	    printf("), ") ;
            lbptr = KLCF_NEXT(lbptr);
        }
	printf("\n") ;
    }

	/* Useful to print router maps also */

	for (cnode = 0; cnode < MAX_COMPACT_NODES; cnode ++) {
		klrou_t *kr ;
		int i ;

        	nasid = gdap->g_nasidtable[cnode];
        	if (nasid == INVALID_NASID)
            		continue;
        	lbptr = (lboard_t *)KL_CONFIG_INFO(nasid) ;

        	while (lbptr) {
			
			lbptr = find_lboard_class(lbptr, KLCLASS_ROUTER) ;
			if(!lbptr)
				break;
			if (!KL_CONFIG_DUPLICATE_BOARD(lbptr)) {
				printf("%llx -> \n", lbptr->brd_nic) ;
				kr = (klrou_t *)find_first_component(lbptr,
					KLSTRUCT_ROU) ;
				ForEachRouterPort(i) {
					printf("[%d, %llx]; ",
						kr->rou_port[i].port_nasid,
						kr->rou_port[i].port_offset) ;
				}
				printf("\n") ;
			}
			lbptr = KLCF_NEXT(lbptr);
        	}
        	printf("\n") ;
    	}

    return 0 ;
}









