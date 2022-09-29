#if IP30	/* whole file */
/*
 * Dallas ds1687 real time clock support
 *
 * $Revision: 1.18 $
 *
 */

#include <sys/types.h>
#include <sys/sbd.h>
#include <sys/cpu.h>
#include <sys/ds1687clk.h>
#include <arcs/types.h>
#include <arcs/time.h>
#include <libsc.h>
#include <libsk.h>
#include <sys/RACER/IP30nvram.h>

extern int month_days[12];		/* in libsc.a */

static uint_t	_clock_func(int, ushort_t *);
static void	reset_todc(void);

static void
ds1687_init(void)
{
	uchar_t	rtc_ctrl;
	ioc3reg_t old;

	old = slow_ioc3_sio();

	if (!RD_DS1687_RTC(RTC_CTRL_D))
		printf("\nReal time clock (ds1687) battery is exhausted.\n");

	/* Clear RTC_INHIBIT_UPDATE, other parts initialized */
	WR_DS1687_RTC(RTC_CTRL_B, RTC_BINARY_DATA_MODE | RTC_24HR_MODE);

	restore_ioc3_sio(old);
}

/*
 * cpu_get_tod -- get current time-of-day from clock chip, and return it in
 * ARCS TIMEINFO format.  the libsc routine get_tod will convert it to
 * seconds for those who need it in that form.
 */
void
cpu_get_tod(TIMEINFO *t)
{
	ushort_t	buf[7];
	static int	tries;

again:
	if (_clock_func(RTC_READ, buf) == RTC_INHIBIT_UPDATE ||
	    (buf[5] < 1970 || buf[4] > 12 || buf[3] > 31 ||
	     buf[2] > 23 || buf[1] > 59 || buf[0] > 59)) {
		if (tries++) {
			printf("Can't set tod clock.\n");
			return;
		}

		ds1687_init();
		reset_todc();
		printf("\nInitialized tod clock.\n");
		goto again;
	}

	/* set up ARCS time structure */
	t->Year = buf[5];
	t->Month = buf[4];
	t->Day = buf[3];
	t->Hour = buf[2];
	t->Minutes = buf[1];
	t->Seconds = buf[0];
	t->Milliseconds = 0;

	return;
}

/* reset_todc sets the real time clock to jan 1, 1970 */
static void
reset_todc(void)
{
	ushort_t buf[7];

	/* use Jan 1, 1970 as time reference */
	bzero(buf, sizeof(buf));
	buf[4] = buf[3] = 1;	/* month and day of the month, i.e. jan 1 */
	buf[5] = YRREF;		/* 1970 */
	buf[6] = 5;		/* thursday */
	_clock_func(RTC_WRITE, buf);
}

/* _clock_func reads/writes the real time clock */
static u_int
_clock_func(int func, ushort_t *ptr)
{
	ioc3reg_t old;

	old = slow_ioc3_sio();

	if (func == RTC_READ) {
		if (RD_DS1687_RTC(RTC_CTRL_B) & RTC_INHIBIT_UPDATE) {
			restore_ioc3_sio(old);
			return RTC_INHIBIT_UPDATE;
		}

		/* access all time registers through bank 1 */
		WR_DS1687_RTC(RTC_CTRL_A, RTC_SELECT_BANK_1|RTC_OSCILLATOR_ON);

		/*
		 * use RTC_INCR_IN_PROGRESS instead of RTC_UPDATING 
		 * so we don't need to wait as long in case the bit is set
		 */
		/* wait until the real time clock is done updating */
		while (RD_DS1687_RTC(RTC_X_CTRL_A) & RTC_INCR_IN_PROGRESS)
			;

		ptr[0] = RD_DS1687_RTC(RTC_SEC);
		ptr[1] = RD_DS1687_RTC(RTC_MIN);
		ptr[2] = RD_DS1687_RTC(RTC_HOUR);
		ptr[3] = RD_DS1687_RTC(RTC_DATE);
		ptr[4] = RD_DS1687_RTC(RTC_MONTH);
		/*
		 * do not combine the next 2 lines of code, otherwise, the
		 * compiler may choose to write to the address port twice,
		 * then read from the data port twice which, of course,
		 * will get the wrong result
		 */
		ptr[5] = RD_DS1687_RTC(RTC_YEAR);
		ptr[5] += RD_DS1687_RTC(RTC_CENTURY) * 100;
		ptr[6] = RD_DS1687_RTC(RTC_DAY);
	}
	else {
		WR_DS1687_RTC(RTC_CTRL_A,RTC_SELECT_BANK_1 | RTC_OSCILLATOR_ON);
		WR_DS1687_RTC(RTC_CTRL_B, RTC_INHIBIT_UPDATE |
			      RTC_BINARY_DATA_MODE | RTC_24HR_MODE);

		/* set the clock */
		WR_DS1687_RTC(RTC_SEC, *ptr++);
		WR_DS1687_RTC(RTC_MIN, *ptr++);
		WR_DS1687_RTC(RTC_HOUR, *ptr++);
		WR_DS1687_RTC(RTC_DATE, *ptr++);
		WR_DS1687_RTC(RTC_MONTH, *ptr++);
		WR_DS1687_RTC(RTC_YEAR, *ptr % 100);
		WR_DS1687_RTC(RTC_CENTURY, *ptr++ / 100);
		WR_DS1687_RTC(RTC_DAY, *ptr++);

		/* 24-hour mode, use binary date for input/output */
		WR_DS1687_RTC(RTC_CTRL_B, RTC_BINARY_DATA_MODE | RTC_24HR_MODE);
	}

	restore_ioc3_sio(old);

	return 0;
}

void (*fp_poweroff)(void);			/* for other devices */

void
cpu_soft_powerdown(void)
{
	void bev_poweroff(void);

	/* shut off the flat panel, if one attached */
	if (fp_poweroff)
		(*fp_poweroff)();

	bev_poweroff();		/* use common low-level code for switch */
	/* NOTREACHED -- power is now off */
}

/* Set rtc rd/wr pulse width ~135ns.  Should hold ip30clocklock when calling.
 */
ioc3reg_t
slow_ioc3_sio(void)
{
	volatile ioc3reg_t *ptr = (volatile ioc3reg_t *)
		(IOC3_PCI_DEVIO_K1PTR + IOC3_SIO_CR);
	ioc3reg_t old;

	old = *ptr;
	*ptr = (old & ~SIO_SR_CMD_PULSE) | (0x8 << SIO_CR_CMD_PULSE_SHIFT);

	return old;
}

/* Reset superIO/parallel rd/wr pulse width ~75ns */
void
restore_ioc3_sio(ioc3reg_t old)
{
	/* latch scratch register to redirect any power down noise */
	(void)RD_DS1687_RTC(NVRAM_REG_OFFSET);

	*(volatile ioc3reg_t *)(IOC3_PCI_DEVIO_K1PTR + IOC3_SIO_CR) = old;
}
#endif	/* IP30 */
