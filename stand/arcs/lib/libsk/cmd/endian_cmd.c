#if R4000 && !EVEREST && !IP30 && !IP32
#ident "$Revision: 1.8 $"

#include <sys/cpu.h>
#include <sys/param.h>
#include <sys/sbd.h>
#include <sys/types.h>
#include <libsc.h>
#include <libsk.h>

#define	EEROM_REG_SIZE	sizeof(u_short)
#define	ENDIAN_BIG	0x0400
#define	ENDIAN_LITTLE	0x8000
#define	ENDIAN_MASK	0x8400
#define	RESET_SCK	*eerom &= ~EEROM_SCK
#define	SET_SCK		*eerom |= EEROM_SCK

static u_short	read_eerom(int);
static int	write_eerom(int, u_short);
static void	eerom_cs_on(void);
static void	eerom_cs_off(void);
static void	eerom_cmd(u_int, u_int);
static int	eerom_hold(void);

static volatile u_int *eerom = (u_int *)PHYS_TO_K1(EEROM);



/* allows user to switch the machine between little and big endians */
int
endian_cmd(int argc, char **argv)
{
	int error = 0;
	u_short new_endian;
	u_short reg0;

	if (argc == 2) {
		if (argv[1][0] != '-')
			error = 1;
		else {
			switch (argv[1][1]) {
			case 'b':
				new_endian = ENDIAN_BIG;
				break;

			case 'l':
				new_endian = ENDIAN_LITTLE;
				break;

			default:
				error = 1;
				break;
			}
		}
	} else
		error = 1;

	if (error) {
		printf("usage: %s -(b|l)\n", argv[0]);
		return 1;
	}

	reg0 = read_eerom(0);
	reg0 &= ~ENDIAN_MASK;
	reg0 |= new_endian;

	return write_eerom(0, reg0);
}



/* read a 16 bits register from the R4000 EEROM */
static u_short
read_eerom(int regnum)
{
	u_short data_read = 0;
	int i = EEROM_REG_SIZE;

	eerom_cs_on();
	eerom_cmd(SER_READ, regnum);

	/* clock the data out of the EEROM */
	while (i--) {
		RESET_SCK;
		SET_SCK;
		data_read <<= 1;
		data_read |= (*eerom & EEROM_SI) ? 1 : 0;
	}
	
	eerom_cs_off();

	return data_read;
}



/* write a 16 bits register in the R4000 EEROM */
static int
write_eerom(int regnum, u_short value)
{
	int error;
	int i = EEROM_REG_SIZE;

	eerom_cs_on();
	eerom_cmd(SER_WEN, 0);	
	eerom_cs_off();

	eerom_cs_on();
	eerom_cmd(SER_WRITE, regnum);

	/* clock the data into the EEROM */
	while (i--) {
		if (value & 0x8000)
			*eerom |= EEROM_SO;
		else
			*eerom &= ~EEROM_SO;
		RESET_SCK;
		SET_SCK;
		value <<= 1;
	}
	*eerom &= ~EEROM_SO;		/* see timing diagram, DI should */
					/* be low after writing */
	eerom_cs_off();
	error = eerom_hold();

	eerom_cs_on();
	eerom_cmd(SER_WDS, 0);
	eerom_cs_off();

	return error;
}



/* enable access to the EEROM by setting the chip select */
static void
eerom_cs_on(void)
{
	*eerom = 0x0;			/* see timing diagram, DI should */
					/* be low when asserting CS */
	RESET_SCK;
	*eerom |= EEROM_CS;
	SET_SCK;
}



/* disable access to the EEROM by clearing the chip select */
static void
eerom_cs_off(void)
{
	RESET_SCK;
	*eerom &= ~EEROM_CS;
	SET_SCK;
}



#define COMMAND_SIZE	11
/* clock in the EEROM command and the register number */
static void
eerom_cmd(u_int cmd, u_int regnum)
{
	int i = COMMAND_SIZE;

	cmd |= regnum << (16 - COMMAND_SIZE);
	while (i--) {
		if (cmd & 0x8000)
			*eerom |= EEROM_SO;
		else
			*eerom &= ~EEROM_SO;
		RESET_SCK;
		SET_SCK;
		cmd <<= 1;
	}
	*eerom &= ~EEROM_SO;		/* see timing diagram, DI should */
					/* be low after writing command */
}



/*
 * after the write command, we must wait for the command to complete.
 * write cycle time is 10 ms max (~5 ms nom); we timeout after ~20 ms
 *    NVDELAY_TIME * NVDELAY_LIMIT = 20 ms
 */
#define NVDELAY_TIME	100	/* 100 us delay times */
#define NVDELAY_LIMIT	200	/* 200 delay limit */

static int
eerom_hold(void)
{
	int error;
	int timeout = NVDELAY_LIMIT;

	eerom_cs_on();
	while (!(*eerom & EEROM_SI) && timeout--)
		DELAY(NVDELAY_TIME);

	if (!(*eerom & EEROM_SI))
		error = -1;
	else
		error = 0;
	eerom_cs_off();

	return error;
}
#else
extern int _prom;		/* silence compiler */
#endif	/* R4000 && !EVEREST && !IP30  && !IP32 */
