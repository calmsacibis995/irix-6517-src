#ident	"IP22diags/pon/pon_more.c:  $Revision: 1.49 $"

#include <sys/types.h>
#include <sys/sbd.h>
#include <sys/cpu.h>
#include <uif.h>
#include <pon.h>
#include <gfxgui.h>
#include "fforward/int2/int2.h"
#include "fforward/scsi/scsi.h"
#include <libsc.h>
#include "libsk.h"

void setstackseg(__psunsigned_t);
static int pon_simms(int);
void cache_K0(void);

int
pon_morediags(unsigned int failflags)
{
    static char *ponmsg="Running power-on diagnostics...";
    static char *cpubad="\n\tCheck or replace:  CPU base board\n";
    static char *scsict="SCSI controller %d diagnostic              *FAILED*\n";
    static char *scsidc="%sSCSI device/cable diagnostic      *FAILED*\n";
    char *cp;
    char diagmode = (cp = getenv("diagmode")) ? *cp : 0;
    int scsirc, ontty;
    extern int pon_caches();
    int (*fptr)(void);
    extern int decinsperloop, uc_decinsperloop;


    /* indicate that system is executing diags by turning on the amber
     */
    ip22_set_cpuled (2);

    /* If console is set to the terminal, try to get the message printed
     * quickly since pon_graphics takes a long time.
     */
    cp = getenv("console");
    if (ontty = (cp && *cp == 'd'))
	    pongui_setup(ponmsg,0);

    *Reportlevel = NO_REPORT;
    failflags = pon_graphics();

    /*  If graphics diagnostic fails, then give an immediate audio
     * indication of failure.
     */
    if (failflags & FAIL_BADGRAPHICS) {
	    play_hello_tune(2);
	    wait_hello_tune();
    }
    *Reportlevel = diagmode == 'v' ? VRB : NO_REPORT;

    if (!ontty)
	    pongui_setup(ponmsg,1);		/* prints power on message */

    if (pon_simms(1)) 
	failflags |= FAIL_MEMORY;

    if (int2_mask()) 
	failflags |= FAIL_INT2;

#ifdef IP24
    fptr = (int (*)(void))K0_TO_K1(pon_caches);
    if ((*fptr)())
	failflags |= FAIL_CACHES;
#endif

#if !defined(IP22_WD95) /* To be done... -Farid */
    if (scsirc = scsi()) {
	if (scsirc & ~0xff00)
	    failflags |= scsiunit_init(0) ? FAIL_SCSICTRLR : FAIL_SCSIDEV;
#ifndef  IP24
	if (scsirc &  0xff00)
	    failflags |= scsiunit_init(1) ? FAIL_SCSICTRLR : FAIL_SCSIDEV;
#endif
    }
#endif /* !IP22_WD95 */

#ifdef PCI2GIO
/* you are not expected to understand this. */
/* after the ide stuff we need to forget that we initialized the driver */
/* because the malloc pool is gone but the driver stucts are not */

ql_clear_structs();
#endif

    if (pon_pcctrl() == 0) {
	    if (pon_pckbd())
		    failflags |= FAIL_PCKBD;
	    if (pon_pcms())
		    failflags |= FAIL_PCMS;
    }
    else
	    failflags |= FAIL_PCCTRL;

#ifdef IP24_DONT_DO_ISDN
    if (pon_isdn())
	 failflags |= FAIL_ISDN;
#endif

    /* print messages corresponding to failed diagnostics
     */
    if (failflags) {
	printf ("\n");
	if (failflags & FAIL_MEMORY) {
	    printf ("Memory diagnostic                          *FAILED*\n\n");
	    pon_simms (0);
	}
	if (failflags & FAIL_BADGRAPHICS) {
	    printf ("Graphics diagnostic                        *FAILED*\n");
	    printf ("\n\tCheck or replace:  Graphics board\n");
	}
	if (failflags & FAIL_INT2) {
	    printf ("Interrupt controller diagnostic            *FAILED*\n");
	    printf (cpubad);
	}
	if (failflags & FAIL_CACHES) {
	    printf ("Cache diagnostic                           *FAILED*\n");
    	    printf("\n\tCheck or replace:  CPU module\n");
	}
	if (failflags & FAIL_SCSICTRLR) {
	    if (scsirc & ~0xff00) printf(scsict,0);
	    if (scsirc &  0xff00) printf(scsict,1);
	    printf (cpubad);
	}
	if (failflags & FAIL_SCSIDEV) {
	    if (scsirc & ~0xff00)
		printf(scsidc,is_fullhouse() ? "Internal " : "");
	    if (scsirc &  0xff00) printf(scsidc,"External ");
	    printf ("\n\tCheck or replace:  Disk, Floppy, CDROM, or SCSI Cable\n");
	}
	if (failflags & FAIL_PCCTRL) {
	    printf ("PC keyboard/mouse controller diagnostic    *FAILED*\n");
	    printf (cpubad);
	}
	if (failflags & FAIL_PCKBD) {
		printf("Keyboard diagnostic			*FAILED*\n");
		printf("\n\tCheck or replace: keyboard and cable\n");
	}
	if (failflags & FAIL_PCMS) {
		printf("Mouse diagnostic			*FAILED*\n");
		printf("\n\tCheck or replace: mouse and cable\n");
	}
#ifdef IP24
	if (failflags & FAIL_ISDN) {
	     printf("ISDN diagnostic				*FAILED*\n");
	}
#endif
	printf ("\n");
    }

    if (ontty)
    	failflags &= ~FAIL_NOGRAPHICS;

    /* set led color according to test results
     */
    if (failflags)
	ip22_set_cpuled(2);	/* amber (both machines) */
    else 
	ip22_set_cpuled(1);	/* green */

    failflags &= ~FAIL_NOGRAPHICS;

    pongui_cleanup();

#ifdef IP24
    if (!(failflags & FAIL_CACHES)) {
	cache_K0();
	IS_KSEG0(pon_caches) ? run_cached() : run_uncached();
	setstackseg(K0BASE);
	decinsperloop = 0;
	uc_decinsperloop = 0;
    }
#endif

    return failflags;
}

/*
 * examine the bad words data saved by the memory configuration
 * and print an appropriate error message
 */
#if IP24	/* PON's are specific to IPxx */
#define	_BADSIMM(bank,simm)	bank * 4 + simm
#else
#define	_BADSIMM(bank,simm)	(2 - bank) * 4 + (4 - simm)
#endif

static int
pon_simms(int testonly)
{
    int bank;
    unsigned bankerr;
    int errcount = 0;

    for (bank = 0; bank < 3; ++bank) {
	bankerr = (simmerrs << (bank * 4)) & BAD_ALLSIMMS;
	if (bankerr == BAD_ALLSIMMS || bankerr == 0)
	    continue;
	errcount++;
	if (testonly)		/* just test for failed simms */
	    continue;
	if (bankerr & BAD_SIMM3)
	    printf ("\tCheck or replace:  SIMM S%d\n", _BADSIMM(bank,3));
	if (bankerr & BAD_SIMM2)
	    printf ("\tCheck or replace:  SIMM S%d\n", _BADSIMM(bank,2));
	if (bankerr & BAD_SIMM1)
	    printf ("\tCheck or replace:  SIMM S%d\n", _BADSIMM(bank,1));
	if (bankerr & BAD_SIMM0)
	    printf ("\tCheck or replace:  SIMM S%d\n", _BADSIMM(bank,0));
    }
    return errcount;
}
