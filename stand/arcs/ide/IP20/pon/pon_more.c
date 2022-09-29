#ident	"IP20diags/pon/pon_more.c:  $Revision: 1.33 $"

#include <uif.h>
#include <libsc.h>
#include <sys/z8530.h>
#include <pon.h>
#include <gfxgui.h>

static int pon_simms(int);
void ip20_set_cpuled(int);
void play_hello_tune(int,int);
int int2_mask(void);
int pon_caches(void);
int scsi(void);
int scsiunit_init(int);
int duart_loopback(int, char **);


int
pon_morediags(void)
{
    char *ponmsg="Running power-on diagnostics...";
    char *cpubad="\n\tCheck or replace:  CPU base board\n";
    int duart_argc = 2;
    static char *duart_argv[] = {"duart", "-i1"};
    char *cp;
    char diagmode = (cp = getenv("diagmode")) ? *cp : 0;
    unsigned failflags = 0;
    extern int *Reportlevel;
    int ontty;

    /* indicate that system is executing diags by turning on the led
     */
    ip20_set_cpuled (1);

    /* If console is set to the terminal, try to get the message printed
     * quickly since pon_graphics takes a long time.
     */
    cp = getenv("console");
    if (ontty = (cp && *cp == 'd'))
	    pongui_setup(ponmsg,0);

    *Reportlevel = NO_REPORT;
    failflags = pon_graphics();
    *Reportlevel = (diagmode == 'M' || diagmode == 'v') ? VRB : NO_REPORT;

    /*  If graphics diagnostic fails, then give an immediate audio
     * indication of failure.
     */
    if (failflags & FAIL_BADGRAPHICS)
	    play_hello_tune(0,2);

    if (!ontty)
	    pongui_setup(ponmsg,1);		/* prints power on message */

    if (pon_simms(1)) 
	failflags |= FAIL_MEMORY;

    if (int2_mask()) 
	failflags |= FAIL_INT2;

    if (pon_caches()) 
	failflags |= FAIL_CACHES;

    if (scsi()) 
	if (scsiunit_init(0)) 
	    failflags |= (FAIL_SCSICTRLR);
	else
	    failflags |= (FAIL_SCSIDEV);

    if (duart_loopback(duart_argc, duart_argv)) 
	failflags |= FAIL_DUART;

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
	    printf ("\n\tCheck or replace:  CPU module\n");
	}
	if (failflags & FAIL_SCSICTRLR) {
	    printf ("SCSI controller diagnostic                 *FAILED*\n");
	    printf (cpubad);
	}
	if (failflags & FAIL_SCSIDEV) {
	    printf ("SCSI device/cable diagnostic               *FAILED*\n");
	    printf ("\n\tCheck or replace:  Disk, Floppy, CDROM, or SCSI Cable\n");
	}
	if (failflags & FAIL_DUART) {
	    printf ("Keyboard/Mouse diagnostic                  *FAILED*\n");
	    printf (cpubad);
	}
	printf ("\n");
    }

    if (ontty)
    	failflags &= ~FAIL_NOGRAPHICS;

    /* set led on/off according to test results
     */
    if (failflags)
	ip20_set_cpuled(1);	/* color 1 = amber */
    else 
	ip20_set_cpuled(2);	/* color 2 = green */

    failflags &= ~FAIL_NOGRAPHICS;

    pongui_cleanup();

    return failflags;
}

/*
 * examine the bad words data saved by the memory configuration
 * and print an appropriate error message
 * XXX	this routine needs to be modified after determining locations	XXX
 * XXX	of the memory SIMMs on the CPU board				XXX
 */
static int
pon_simms(int testonly)
{
    int bank;
    unsigned bankerr;
    int errcount = 0;

    /* report on all banks checked in lmem_conf.s */
    for (bank = 0; bank < 4; ++bank) {
	bankerr = (simmerrs << (bank * 4)) & BAD_ALLSIMMS;
	if (bankerr == BAD_ALLSIMMS || bankerr == 0)
	    continue;
	errcount++;
	if (testonly)		/* just test for failed simms */
	    continue;
	if (bankerr & BAD_SIMM3)
	    printf ("\tCheck or replace:  SIMM S4%c\n", 'C' - bank);
	if (bankerr & BAD_SIMM2)
	    printf ("\tCheck or replace:  SIMM S2%c\n", 'C' - bank);
	if (bankerr & BAD_SIMM1)
	    printf ("\tCheck or replace:  SIMM S3%c\n", 'C' - bank);
	if (bankerr & BAD_SIMM0)
	    printf ("\tCheck or replace:  SIMM S1%c\n", 'C' - bank);
    }
    return errcount;
}
