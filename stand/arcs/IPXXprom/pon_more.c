#ident	"IP12diags/pon/pon_more.c:  $Revision: 1.2 $"

#include "uif.h"
#include "saioctl.h"
#include "sys/z8530.h"
#include <libsc.h>
#include <pon.h>

int pon_simms(int);
int pon_fuses(void);

int
pon_morediags(void)
{
    char *ponmsg="Running power-on diagnostics...";
    int duart_argc = 2;
    static char *duart_argv[] = {"duart", "-i1"};
    char *cp;
    char diagmode = (cp = getenv("diagmode")) ? *cp : 0;
    unsigned failflags = 0;
    extern int *Reportlevel;
    int ontty;

    /* indicate that system is executing diags by turning on the led
     */
    ip12_set_cpuled (1);

    /* If console is set to the terminal, try to get the message printed
     * quickly since pon_graphics takes a long time.
     */
    cp = getenv("console");
    if (ontty = (cp && *cp == 'd'))
	    pongui_setup(ponmsg,0);

    *Reportlevel = NO_REPORT;
    failflags = pon_graphics();
    *Reportlevel = diagmode == 'v' ? VRB : NO_REPORT;

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

    failflags |= pon_fuses();

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
	    if (is_hp1())
		printf ("\n\tCheck or replace:  Graphics board\n");
	}
	if (failflags & FAIL_INT2) {
	    printf ("Interrupt controller diagnostic            *FAILED*\n");
	    if (is_hp1())
		printf ("\n\tCheck or replace:  CPU board\n");
	}
	if (failflags & FAIL_CACHES) {
	    printf ("Cache diagnostic                           *FAILED*\n");
	    if (is_hp1())
		printf ("\n\tCheck or replace:  CPU board\n");
	}
	if (failflags & FAIL_SCSICTRLR) {
	    printf ("SCSI controller diagnostic                 *FAILED*\n");
	    if (is_hp1())
		printf ("\n\tCheck or replace:  CPU board\n");
	}
	if (failflags & FAIL_SCSIDEV) {
	    printf ("SCSI device/cable diagnostic               *FAILED*\n");
	    if (is_hp1())
		printf ("\n\tCheck or replace:  Disk, Floppy, CDROM, or SCSI Cable\n");
	}
	if (failflags & FAIL_DUART) {
	    printf ("Keyboard/Mouse diagnostic                  *FAILED*\n");
	    if (is_hp1())
		printf ("\n\tCheck or replace:  CPU board\n");
	}
	if (failflags & FAIL_SCSIFUSE)
	    printf ("SCSI Power Fuse diagnostic                 *FAILED*\n");
	printf ("\n");
    }

    if (ontty)
    	failflags &= ~FAIL_NOGRAPHICS;

    /* set led on/off according to test results
     */
    if (failflags)
	ip12_set_cpuled (1);		/* color 1 = amber for HP1, ON for IP12 */
    else 
	if (is_hp1())
	    ip12_set_cpuled (2);	/* color 2 = green for HP1 */
	else
	    ip12_set_cpuled (0);	/* color 0 = OFF for IP12 */

    failflags &= ~FAIL_NOGRAPHICS;

    pongui_cleanup();

    return failflags;
}

/*
 * test the SCSI external power fuse
 *	CTSA = 1 if SCSI is OK
 */
int
pon_fuses(void)
{
    if (ip12_board_rev() < 1 || is_hp1())
	return 0;

    if (!(ip12_duart_rr0 (0) & RR0_CTS))
	return FAIL_SCSIFUSE;
    else
	return 0;
}

/*
 * examine the bad byte data saved by the memory configuration
 * and print an appropriate error message
 */
int
pon_simms(int testonly)
{
    return 0;
}
