#ident	"lib/libsk/cmd/off_cmd.c:  $Revision: 1.7 $"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/sbd.h>
#include <sys/cpu.h>
#include <arcs/types.h>
#include <arcs/time.h>

#if IP22 || IP26 || IP28
#include <sys/ds1286.h>
#endif

#include <libsc.h>
#include <libsk.h>

#if IP32
#include <sys/ds17287.h>
extern void cpu_powerdown(int);
#endif

/*
 * off - turn off power
 */
/*ARGSUSED*/
int
poweroff_cmd(int argc, char **argv, char **envp)
{
#if IP22 || IP26 || IP28 || IP32
  
#if defined(IP32)
    volatile ds17287_clk_t *clock;
#else
    volatile unsigned int *p;
    volatile ds1286_clk_t *clock;
    int command;
#endif
    int min, hr, day;

    DELAY(10000);
    if (argc == 1)
#if defined(IP32)
      cpu_powerdown(0);
#else
      cpu_soft_powerdown();
#endif  
    
    if (argc == 2) {
	TIMEINFO nt;

	/* format like IRIX: mmddhhmm[ccyy|yy][.ss]
	 */
	if (format_date (&nt, argv[1]) < 0)
		return -1;

	min = nt.Minutes;
	hr  = nt.Hour;
	day = nt.Day;
    }

    else if (argc == 4) {
	day = atoi(argv[1]);
	hr  = atoi(argv[2]);
	min = atoi(argv[3]);
    }

    else
	return -1;


    /* Set alarm to activate when minutes, hour and day match.
     */
    printf("Setting alarm to %d %d %d\n", day, hr, min);

    clock = RTDS_CLOCK_ADDR;
    clock->hour_alarm = (char) (((hr  / 10) << 4) | (hr  % 10));
    clock->min_alarm  = (char) (((min / 10) << 4) | (min % 10));
#if defined(IP32)
    clock->ram[DS_BANK1_DATE_ALARM] = (char) (((day / 10) << 4) | (day % 10));
    flushbus();
    cpu_powerdown(1);
#else
    clock->day_alarm  = (char) (((day / 10) << 4) | (day % 10));
    command = clock->command;
    command |= WTR_TIME_DAY_INTA;	/* Enable INTA */
    command &= ~WTR_DEAC_TDM;		/* Activate TDM */
    clock->command = command;

    set_SR (SR_PROMBASE);		/* no interrupts */

    p = (volatile uint *)PHYS_TO_K1(HPC3_PANEL);
    *p = ~POWER_SUP_INHIBIT;		/* turn off power */
    flushbus();
    DELAY(10000);

#endif
    return 0;
#else
    return -1;
#endif
}
