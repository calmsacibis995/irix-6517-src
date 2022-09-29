#ident	"$Revision: 1.9 $"

#include <sys/types.h>
#include <sys/sbd.h>
#include <gfxgui.h>
#include <pon.h>
#include <libsc.h>
#include <libsk.h>
#include <uif.h>

static int pon_simms(int);

/*  Local prototype of pon routines is not so good, but they all have
 * the same interface.
 */
int int2_mask(void);
int pon_iu(void);
int scsi(void);

/* Other local prototypes
 */
void setstackseg(__psunsigned_t);
void play_hello_tune(int);
void wait_hello_tune(void);

static char *ponmsg="Running power-on diagnostics...";
static char *cpubad="\n\tCheck or replace:  CPU base board\n";
static char *scsict="SCSI controller %d diagnostic              *FAILED*\n";
static char *scsidc="%sSCSI device/cable diagnostic      *FAILED*\n";

int
pon_morediags(void)
{
	extern int decinsperloop, uc_decinsperloop;
	int scsirc, ontty, diagmode;
	unsigned int failflags = 0;
	char *cp;

	diagmode = (cp = getenv("diagmode")) ? *cp : 0;

	/* indicate that system is executing diags by turning on the amber
	 */
	ip22_set_cpuled(2);

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

	/*  We should do this early, but waiting until last gives us a bit
	 * more time to warm-up, and above diags seem ok w/o it.  Used to
	 * be after SCSI, which gives us a lot of warm-up time with the
	 * spin-up, but with the reset hack I want to avoid spinning up
	 * the disks or clicking/whirring the tapes 2x.
	 */
	if (pon_iu())
		failflags |= FAIL_CPU;

#if !defined(IP22_WD95)
	if (scsirc = scsi()) {
		if (scsirc & ~0xff00)
			failflags |= scsiunit_init(0) ? FAIL_SCSICTRLR :
							FAIL_SCSIDEV;
		if (scsirc &  0xff00)
			failflags |= scsiunit_init(1) ? FAIL_SCSICTRLR :
							FAIL_SCSIDEV;
	}
#endif /* !IP22_WD95 */

	if (pon_pcctrl() == 0) {
		if (pon_pckbd())
			failflags |= FAIL_PCKBD;
		if (pon_pcms())
			failflags |= FAIL_PCMS;
	}
	else
		failflags |= FAIL_PCCTRL;

	/* print messages corresponding to failed diagnostics
	 */
	if (failflags) {
		printf("\n");
		if (failflags & FAIL_CPU) {
			printf( "CPU fast IU diagnostic                     "
				"*FAILED*\n\n"
				"\n\tCheck or replace:	CPU module\n");
		}
		if (failflags & FAIL_MEMORY) {
			printf( "Memory diagnostic                          "
				"*FAILED*\n\n");
			pon_simms (0);
		}
		if (failflags & FAIL_BADGRAPHICS) {
			printf( "Graphics diagnostic                        "
				"*FAILED*\n"
				"\n\tCheck or replace:  Graphics board\n");
}
		if (failflags & FAIL_INT2) {
			printf( "Interrupt controller diagnostic            "
				"*FAILED*\n%s",cpubad);
		}
		if (failflags & FAIL_SCSICTRLR) {
			if (scsirc & ~0xff00) printf(scsict,0);
			if (scsirc &  0xff00) printf(scsict,1);
			printf(cpubad);
		}
		if (failflags & FAIL_SCSIDEV) {
			if (scsirc & ~0xff00)
				printf(scsidc, "Internal ");
			if (scsirc &  0xff00) printf(scsidc,"External ");
			printf( "\n\tCheck or replace:  Disk, Floppy, CDROM, "
				"or SCSI Cable\n");
		}
		if (failflags & FAIL_PCCTRL) {
			printf( "PC keyboard/mouse controller diagnostic    "
				"*FAILED*\n%s",cpubad);
		}
		if (failflags & FAIL_PCKBD) {
			printf( "Keyboard diagnostic			    "
				"*FAILED*\n"
				"\n\tCheck or replace: keyboard and cable\n");
		}
		if (failflags & FAIL_PCMS) {
			printf( "Mouse diagnostic			    "
				"*FAILED*\n"
				"\n\tCheck or replace: mouse and cable\n");
		}
		printf("\n");
	}

	if (ontty)
		failflags &= ~FAIL_NOGRAPHICS;

	/* set led color according to test results
	 */
	if (failflags)
		ip22_set_cpuled(2);	/* amber */
	else 
		ip22_set_cpuled(1);	/* green */

	failflags &= ~FAIL_NOGRAPHICS;

	pongui_cleanup();

	return failflags;
}

/*
 * examine the bad words data saved by the memory configuration
 * and print an appropriate error message
 */
#define	_BADSIMM(bank,simm)	(2 - bank) * 4 + (4 - simm)

static int
pon_simms(int testonly)
{
	static char *msg = "\tCheck or replace:  SIMM S%d\n";
	unsigned bankerr;
	int errcount = 0;
	int bank;

	for (bank = 0; bank < 3; ++bank) {
		bankerr = (simmerrs << (bank * 4)) & BAD_ALLSIMMS;
		if (bankerr == BAD_ALLSIMMS || bankerr == 0)
			continue;
		errcount++;
		if (testonly)		/* just test for failed simms */
			continue;
		if (bankerr & BAD_SIMM3)
			printf(msg, _BADSIMM(bank,3));
		if (bankerr & BAD_SIMM2)
			printf(msg, _BADSIMM(bank,2));
		if (bankerr & BAD_SIMM1)
			printf(msg, _BADSIMM(bank,1));
		if (bankerr & BAD_SIMM0)
			printf(msg, _BADSIMM(bank,0));
	}

	return errcount;
}
