/*
 * IDE universal status messages and notifier functions.
 */

#ident "$Revision: 1.13 $"

#include <saioctl.h>
#include <libsk.h>
#include <libsc.h>
#include <uif.h>

#if IP22
#define USAGE		"Usage: led n (0=off, 1=green, 2=amber, 3=red/amber)\n"
#define MAXVALUE	3
#endif
#if IP20 || IP26 || IP28
#define USAGE		"Usage: led n (0=off, 1=green, 2=amber)\n"
#define MAXVALUE	2
#endif

#if IP22 || IP26 || IP28
#define SETLED(X)	ip22_set_cpuled(X)
#endif
#if IP20
#define SETLED(X)	ip20_set_cpuled(X)
#endif

/*
 * Command to turn LED various colors (on/off on IP20).
 */
int
#if IP20
sickled(int argc,char *argv[])
#else
setled(int argc,char *argv[])
#endif
{
	extern int failflag;
	int value = -1;
	char *usage = USAGE;

	if ( argc != 2 ) {
		printf(usage);
		return (1);
	}

	atob(argv[1],&value);
	if (value < 0 || value > MAXVALUE) {
		printf(usage);
		return (1);
	}

#if IP22 || IP26 || IP28	/* these cpus require this side effect */
	if (value <= 1) failflag = 0;
#endif

	SETLED(value);

	return (0);
}

/*
 * Print out error summary of diagnostics test
 */
void
sum_error (char *test_name)
{
#ifdef PROM
	msg_printf (SUM, "\r\t\t\t\t\t\t\t\t- ERROR -\n");
#endif /* PROM */
	msg_printf (SUM, "\rHardware failure detected in %s test\n", test_name);
}   /* sum_error */

/*
 *  Print out commonly used successful completion message
 */
void
okydoky(void)
{
    msg_printf(VRB, "Test completed with no errors.\n");
}

void
gfx_exit(void)
{
    if (console_is_gfx())
    {
	/*
	 *  Now we need to reintialize the graphics console, but first
	 *  we need do some voodoo to convince the ioctl that graphics
	 *  really need initializing.
	 */
	ioctl(1, TIOCREOPEN, 0);
    }
    buffoff();
}


#ifndef PROM
/*
 *  emfail - "electronics module failure"
 *
 *  Report diagnostic-detected failure in terms of the appropriate field
 *  replaceable unit. Exit from IDE restoring the state of the console
 *  if necessary.
 */
int
emfail(int argc, char *argv[])
{
    extern int _scsi_diag_fail;
    int unit_num, scshift, type = 0;
    char *unit_name;
    char line[80];

    if (argc != 2)
    {
	msg_printf(SUM, "usage: emfail unit_number\n");
	return (1);
    }
    atob(argv[1], &unit_num);
    switch(unit_num)
    {
	case 1:				/* CPU */
	case 4:				/* tlb */
	case 5:				/* FPU */
	    type = 2;
	    break;
	case 2:
	    type = 1;
	    gfx_exit();
	    break;
	case 6:	/* SCSI */
	    if(_scsi_diag_fail == 0x10000) /*  No devices found when not diskless;
		could be a cabling, termination, or power problem... */
		unit_name = "termination or cable";
	    else { /* note that low bit won't ever be set, because
		* we use ID 0 for host adapter.  This might change someday. */
		line[0] = '\0';
		unit_name = line;
		if(_scsi_diag_fail & 0xff) {	/* controller 0 */
			strcat(line, "CTLR#0: device ");
			unit_num = _scsi_diag_fail ;
			for(scshift=0;unit_num; scshift++, unit_num >>= 1) {
				char devn[3];
				if(unit_num&1) {
					sprintf(devn, "%d ", scshift);
					strcat(line, devn);
				}
			}
		}
		if(_scsi_diag_fail & 0xff00) {	/* controller 1 */
			if(_scsi_diag_fail & 0xff)
				strcat(line, "and ");
			strcat(line, "CTLR#1: device ");
			unit_num = _scsi_diag_fail >> 8;
			for(scshift=0;unit_num; scshift++, unit_num >>= 1) {
				char devn[3];
				if(unit_num&1) {
					sprintf(devn, "%d ", scshift);
					strcat(line, devn);
				}
			}
		}
	    }
	    printf("ERROR:  Failure detected in SCSI (%s).\n",
		    unit_name);
	    return (1);
#if IP22 || IP26 || IP28
	case 14:			/* audio on daughter card */
	    type = 3;
	    break;
#else
	case 14:			/* audio on mother board */
#endif
	case 3:				/* int2 */
	case 7:				/* timer */
	case 8:				/* duart */
	case 9:				/* eeprom */
	case 10:			/* hpc */
	case 11:			/* parity */
	case 12:			/* secondary cache */
	case 13:			/* memory */
	case 15:			/* scsi ctrl */
	default:
	    break;
    }
    printf("ERROR:  Failure detected on the ");
    switch (type) {
    case 1:
	printf("graphics board.\n");
	break;
    case 2:
	printf("CPU module.\n");
	break;
#if IP22 || IP26 || IP28
    case 3:
	printf("Audio board.\n");
	break;
#endif
    default:
	printf("CPU base board.\n");
	break;
    }
    return (1);
}
#endif /* ! PROM */
