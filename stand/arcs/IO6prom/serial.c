/***********************************************************************\
*	File:	serial.c						*
*									*
*	Contains basic support code for the system serial number.	*
*									*
\***********************************************************************/

/*
 * This file on SN0 contains the support for miscellaneous commands
 * like printint serial numbers, dump klcfg info, check system clock...
 */

/* #include <sys/types.h>
#include <sys/cpu.h>
#include <sys/mips_addrspace.h>
*/

#include <libsc.h>
#include <libsk.h>
#include <arcs/errno.h>
#include <rtc.h>
#include <sn0hinv.h>
#include <sys/SN/gda.h>

extern int 	prmod_snum(lboard_t *, void *, void *) ;
 
static int 	serial_is_settable = 0;

/*
 * magic_set()
 *	Returns true if the proper magic switches are set.
 */

static int
magic_set(void)
{
	return 0; 
}


/*
 * serial_cmd()
 *      Allows the user to display and alter the system serial
 *      number.  Eventually, this will only work if the serial
 *      number hasn't been written already.
 */

int
serial_cmd(int argc, char **argv)
{
    visit_lboard(prmod_snum, NULL, NULL) ;
    return 0;
}

int erase_nvram_cmd()
{
    /*
     * It is useful to have this command in SN0.
     * It can take a pathname as argument and erase
     * that nvram completely.
     */
    printf("fixme: erase_nvram_cmd"); /* XXX */
    return 0;
}

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
            printf("(%s,%d,%x", BOARD_NAME(lbptr->brd_type), 
			lbptr->brd_nasid, (lbptr->brd_nic+1)) ;
            if (lbptr->brd_flags & DUPLICATE_BOARD)
	        printf("-D") ;
    	    printf("), ") ;
            lbptr = KLCF_NEXT(lbptr);
        }
	printf("\n") ;
    }
    return 0 ;
}


/*
 * checkclk_cmd
 * Checks the system speed against a standard speed
 * like the tty baud rate generator clock.
 */

#define NUMCHAR_BASE    960
#define MICROSEC	1000000

int
checkclk(int nbuf, int base)
{
    char 		*buf ;
    int  		i, drift ;
    ULONG 		fd, nbytes, cnt ;
    long 		rc ;
    unsigned long   	tm0, tm1, time;
    unsigned long   	ustm0, ustm1, ustime, idl_ustime, a, b;

    if (nbuf == 0) {
        buf = (char *)calloc(10, base) ;
        nbytes = base*10 ;
    }
    else {
        buf = (char *)calloc(nbuf, base) ;
        nbytes = base*nbuf ;
    }

    if (!buf) {
        printf("Memory alloc error\n") ;
        return 1 ;
    }

    if ((rc = Open((CHAR *)cpu_get_serial(), OpenReadWrite, &fd)) != ESUCCESS) {
        perror(rc,"Open");
        return 1 ;
    }

    tm0         = get_tod();
    ustm0       = rtc_time() ;

    if ((Write(fd, buf, nbytes, &cnt) != ESUCCESS) &&
        (cnt != nbytes)) {
        perror(rc,"Write");
        return 1 ;
    }

    ustm1       = rtc_time() ;
    tm1         = get_tod();

    Close(fd) ;
    free(buf) ;

    ustime      = ustm1 - ustm0 ;
    time        = tm1-tm0 ;

    idl_ustime  = ((nbytes/base) * MICROSEC) ;
    if (ustime < idl_ustime) {
	a = idl_ustime ;
	b = ustime ;
    } else {
	a = ustime ;
	b = idl_ustime ;
    }

    drift = ((a - b) * 100)/a ;
    if (drift < 1)
        printf("RSLT: check_clock PASS. \n");
    else {
        printf("RSLT: check_clock FAIL. \n");
	printf("Clock drift is %d.\n"
               "Please Check midplane crystal jumper and flash prom frequency.\n", 
		drift) ;
    }

    if (nbuf)
    	printf("\
%d      %d              %ld             %d		%ld\n",
        nbytes, time, ustime, drift, (ustime%MICROSEC)/nbytes) ;

    return 0;
}

int
checkclk_cmd(int argc, char **argv)
{
    int i, n ;
    int base ;
    char *p  ;

    printf("Checking system clock, please wait for 10 secs ...\n") ;

    if (argv[1])
	n = atoi(argv[1]) ;
    else
   	n = 0 ;

    if ((p=getenv("dbaud")) && (p))
	base = atoi(p) / 10 ;
    else
	base = NUMCHAR_BASE ;

    if (n)
    	printf("\
Num	TOD-time	RT-time		%% drift	OH per chr\n") ;

    for (i=(n?1:0); i<=n; i++)
	checkclk(i, base) ;

    return 0 ;
}








