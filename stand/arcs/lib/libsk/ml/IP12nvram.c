/* IP20/22/26/IP28 nvram code
 * EEPROM specific code.
 *
 * Ideally the common routines that aren't different between this file and
 * dallas.c should be segrated into a 3rd file, but I didn't want to disturb
 * the other functionalities too much.
 */

#if IP20 || IP22 || IP26 || IP28			/* whole file */

#ident "$Revision: 1.22 $"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/sbd.h>
#include <sys/cpu.h>
#include <libsk.h>
#include <libsc.h>
#include <arcs/folder.h>
#include <arcs/errno.h>

struct string_list;

#if defined(IP22)
/* These define the EEPROM specific functions from their generic names,
 * to an eeprom specific name.  This allows co-existance with the dallas
 * routines for the same functions.
 */
#define cpu_get_nvram_offset	eeprom_cpu_get_nvram_offset
#define cpu_get_nvram_buf	eeprom_cpu_get_nvram_buf
#define cpu_set_nvram_offset	eeprom_cpu_set_nvram_offset
#define cpu_set_nvram		eeprom_cpu_set_nvram
#define cpu_is_nvvalid		eeprom_cpu_is_nvvalid
#define cpu_set_nvvalid		eeprom_cpu_set_nvvalid
#define cpu_nv_lock		eeprom_cpu_nv_lock
#define cpu_get_eaddr		eeprom_cpu_get_eaddr
#define cpu_nv_unlock		eeprom_cpu_nv_unlock
#define nvram_is_protected	eeprom_nvram_is_protected
#define cpu_get_nvram_persist	eeprom_cpu_get_nvram_persist
#define cpu_set_nvram_persist	eeprom_cpu_set_nvram_persist
#define cpu_nvram_init		eeprom_cpu_nvram_init
#endif	/* IP22 */

/* Exports */
#define	NVRAM_FUNC(type,name,args,pargs,cargs,return)		type eeprom_ ## name pargs;
#include <PROTOnvram.h>

/* Private */
static ushort	get_nvreg(int);
static int	set_nvreg(int, unsigned short);
static void	nvram_cs_on(void);
static void	nvram_cs_off(void);
static void	nvram_cmd(unsigned, unsigned);
static int	nvram_hold(void);

static unsigned short	nvram_content[NVLEN_MAX / 2];
static int 		nvram_read;

volatile unsigned char *cpu_auxctl = 
			    (unsigned char *) PHYS_TO_K1( CPU_AUX_CONTROL );

/*
 * nvchecksum -- calculate new checksum for non-volatile RAM
 */
char
nvchecksum(void)
{
	register int nv_reg;

	/*
	 * Seed the checksum so all-zeroes (all-ones) nvram doesn't have a zero
	 * (all-ones) checksum.
	 */
	register signed char checksum = 0xa5;

	/* save the whole nvram in memory for speed */
	if (!nvram_read) {
		int i;

		for (i = 0; i < NVLEN_MAX / 2; i++)
			nvram_content[i] = get_nvreg(i);

		nvram_read = 1;
	}

	for (nv_reg = 0; nv_reg < NVLEN_MAX / 2; nv_reg++) {
		if (nv_reg == (NVOFF_CHECKSUM / 2))
			if (NVOFF_CHECKSUM & 0x01)
				checksum ^= nvram_content[nv_reg] >> 8;
			else
				checksum ^= nvram_content[nv_reg] & 0xff;
		else
			checksum ^= (nvram_content[nv_reg] >> 8) ^
				(nvram_content[nv_reg] & 0xff);

		/* following is a tricky way to rotate */
		checksum = (checksum << 1) | (checksum < 0);
	}

	return (char)checksum;
}

/*
 * get_nvreg -- read a 16 bit register from non-volatile memory.  Bytes
 *  are stored in this string in big-endian order in each 16 bit word.
 */
static ushort
get_nvreg(int nv_regnum)
{
	if (nvram_read)
		return nvram_content[nv_regnum];
	else {
		ushort ser_read;
		int i;
#ifdef CONSOLE_LED
		unsigned char health_led_off = *cpu_auxctl & CONSOLE_LED;
#endif

		*cpu_auxctl &= ~NVRAM_PRE;
		nvram_cs_on();			/* enable chip select */
		nvram_cmd(SER_READ, nv_regnum);

		/* clock the data out of serial mem */
		for (i = 0; i < 16; i++) {
			*cpu_auxctl &= ~SERCLK;
			*cpu_auxctl |= SERCLK;
			ser_read <<= 1;
			ser_read |= (*cpu_auxctl & SER_TO_CPU) ? 1 : 0;
		}
	
		nvram_cs_off();

#ifdef CONSOLE_LED
		*cpu_auxctl = (*cpu_auxctl & ~CONSOLE_LED) | health_led_off;
#endif
		return ser_read;
	}
}


/*
 * set_nvreg -- writes a 16 bit word into non-volatile memory.  Bytes
 *  are stored in this register in big-endian order in each 16 bit word.
 */
static int
set_nvreg(int nv_regnum, unsigned short val)
{
	int error;
	int i;
#ifdef CONSOLE_LED
	unsigned char health_led_off = *cpu_auxctl & CONSOLE_LED;
#endif
	unsigned short data = val;

	*cpu_auxctl &= ~NVRAM_PRE;
	nvram_cs_on();
	nvram_cmd(SER_WEN, 0);	
	nvram_cs_off();

	nvram_cs_on();
	nvram_cmd(SER_WRITE, nv_regnum);

	/*
	 * clock the data into serial mem 
	 */
	for (i = 0; i < 16; i++) {
		if (val & 0x8000)	/* pull the bit out of high order pos */
			*cpu_auxctl |= CPU_TO_SER;
		else
			*cpu_auxctl &= ~CPU_TO_SER;
		*cpu_auxctl &= ~SERCLK;
		*cpu_auxctl |= SERCLK;
		val <<= 1;
	}
	*cpu_auxctl &= ~CPU_TO_SER;	/* see data sheet timing diagram */
	
	nvram_cs_off();
	error = nvram_hold();

	nvram_cs_on();
	nvram_cmd(SER_WDS, 0);
	nvram_cs_off();

#ifdef CONSOLE_LED
	*cpu_auxctl = (*cpu_auxctl & ~CONSOLE_LED) | health_led_off;
#endif

	if (!error)
		nvram_content[nv_regnum] = data;

	return (error);
}

/*
 * enable the serial memory by setting the console chip select
 */
static void
nvram_cs_on(void)
{
	*cpu_auxctl &= NVRAM_PRE;
	*cpu_auxctl |= CONSOLE_CS;
	*cpu_auxctl |= SERCLK;
}

/*
 * turn off the chip select
 */
static void
nvram_cs_off(void)
{
	*cpu_auxctl &= ~SERCLK;
	*cpu_auxctl &= ~CONSOLE_CS;
	*cpu_auxctl |= SERCLK;
}

#define	BITS_IN_COMMAND	11

/*
 * clock in the nvram command and the register number.  For the
 * natl semi conducter nv ram chip the op code is 3 bits and
 * the address is 6/8 bits. 
 */
static void
nvram_cmd(unsigned int cmd, unsigned int reg)
{
	ushort ser_cmd;
	int i;

	ser_cmd = cmd | (reg << (16 - BITS_IN_COMMAND)); /* left justified */
	for (i = 0; i < BITS_IN_COMMAND; i++) {
		if (ser_cmd & 0x8000)	/* if high order bit set */
			*cpu_auxctl |= CPU_TO_SER;
		else
			*cpu_auxctl &= ~CPU_TO_SER;
		*cpu_auxctl &= ~SERCLK;
		*cpu_auxctl |= SERCLK;
		ser_cmd <<= 1;
	}
	*cpu_auxctl &= ~CPU_TO_SER;	/* see data sheet timing diagram */
}

/*
 * after write/erase commands, we must wait for the command to complete
 * write cycle time is 10 ms max (~5 ms nom); we timeout after ~20 ms
 *    NVDELAY_TIME * NVDELAY_LIMIT = 20 ms
 */
#define NVDELAY_TIME	100	/* 100 us delay times */
#define NVDELAY_LIMIT	200	/* 200 delay limit */

static int
nvram_hold(void)
{
	int error;
	int timeout = NVDELAY_LIMIT;

	nvram_cs_on();
	while (!(*cpu_auxctl & SER_TO_CPU) && timeout--)
		DELAY(NVDELAY_TIME);

	if (!(*cpu_auxctl & SER_TO_CPU))
		error = -1;
	else
		error = 0;
	nvram_cs_off();

	return (error);
}

/*
 * cpu_get_nvram -- read string from nvram at an offset
 */
char *
cpu_get_nvram_offset(int nv_off, int nv_len)
{
	static char buf[128];

	cpu_get_nvram_buf(nv_off, nv_len, buf);

	return buf;
}

/*
 * cpu_get_nvram_buf -- the same as cpu_get_nvram, but put it in a buffer
 *                      of our choice
 */
void
cpu_get_nvram_buf(int nv_off, int nv_len, char buf[])
{
	char *bufptr;
	int i;
	unsigned short nvreg;

	bufptr = buf;
	if (nv_off % 2 == 1) {
		nvreg = get_nvreg(nv_off / 2);
		*bufptr++ = nvreg & 0xff;
		nv_off++;
		nv_len--;
	}
	
	for (i = 0; i < nv_len / 2; i++) {
		nvreg = get_nvreg(nv_off / 2 + i);
		*bufptr++ = (char)(nvreg >> 8);
		*bufptr++ = (char)(nvreg & 0xff);
	}

	if (nv_len % 2 == 1) {
		nvreg = get_nvreg((nv_off + nv_len) / 2);
		*bufptr++ = nvreg >> 8;
	}

	*bufptr = 0;
}

/*
 * cpu_set_nvram_offset -- write a string to nvram at an offset
 */
int
cpu_set_nvram_offset(int nv_off, int nv_len, char *string)
{
	unsigned short curval;
	char checksum[2];
	int nv_off_save; 
	int i;

	nv_off_save = nv_off;

	if (nv_off % 2 == 1  &&  nv_len > 0) {
		curval  = get_nvreg(nv_off / 2);
		curval &= 0xff00;
		curval |= *string;
		if (set_nvreg(nv_off / 2, curval))
			return (-1);
		string++;
		nv_off++;
		nv_len--;
	}

	for (i = 0; i < nv_len / 2; i++) {
		curval = (unsigned short) *string++ << 8;
		curval |= *string;
		string++;
		if (set_nvreg(nv_off / 2 + i, curval))
			return (-1);
	}

	if (nv_len % 2 == 1) {
		curval = get_nvreg((nv_off + nv_len) / 2);
		curval &= 0x00ff;
		curval |= (unsigned short) *string << 8;
		if (set_nvreg((nv_off + nv_len) / 2, curval))
			return (-1);
	}

	if (nv_off_save != NVOFF_CHECKSUM) {
		checksum[0] = nvchecksum();
		checksum[1] = 0;
		return cpu_set_nvram_offset(NVOFF_CHECKSUM, NVLEN_CHECKSUM,
								checksum);
	}
	else
		return (0);
}

/*
 * cpu_set_nvram - set nvram string to value string
 *	XXX reset of nvram code needs to move out of prom area
 *	to someplace reasonable (here?)
 */
int
cpu_set_nvram(char *match, char *newstring)
{
    extern int _prom;

    if (_prom)
	return setenv_nvram (match, newstring);

    return 0;
}

int
cpu_is_nvvalid(void)
{
    /* try twice */
    if (nvchecksum() != *cpu_get_nvram_offset(NVOFF_CHECKSUM,NVLEN_CHECKSUM) &&
	nvchecksum() != *cpu_get_nvram_offset(NVOFF_CHECKSUM,NVLEN_CHECKSUM) )
	return 0;
    

    if (NV_CURRENT_REV !=*cpu_get_nvram_offset(NVOFF_REVISION, NVLEN_REVISION))
	return 0;

    return 1;
}

/*ARGSUSED*/
void
cpu_set_nvvalid(void (*delstr)(char *,struct string_list *),
		struct string_list *env)
{
    char csum = NV_CURRENT_REV;
    cpu_set_nvram_offset(NVOFF_REVISION, NVLEN_REVISION, &csum);

    csum = nvchecksum();
    cpu_set_nvram_offset(NVOFF_CHECKSUM, NVLEN_CHECKSUM, &csum);
}

void
cpu_nv_lock(int lock)
{
#ifdef CONSOLE_LED
    unsigned char health_led_off = *cpu_auxctl & CONSOLE_LED;
#endif

    *cpu_auxctl &= ~NVRAM_PRE;
    nvram_cs_on();
    nvram_cmd(SER_WEN, 0);
    nvram_cs_off();

    *cpu_auxctl |= NVRAM_PRE;
    nvram_cs_on();
    nvram_cmd(SER_PREN, 0);
    nvram_cs_off();

    nvram_cs_on();
    nvram_cmd(SER_PRCLEAR, 0xff);
    nvram_cs_off();
    if (nvram_hold())
	printf("Error -- NVRAM did not complete PRCLEAR cycle\n");

    if (lock) {
	nvram_cs_on();
	nvram_cmd(SER_PREN, 0);
	nvram_cs_off();

	nvram_cs_on();
	nvram_cmd(SER_PRWRITE, NVFUSE_START / 2);
	nvram_cs_off();
	if (nvram_hold())
	    printf("Error -- NVRAM did not complete PRWRITE cycle\n");
    }

    *cpu_auxctl &= ~NVRAM_PRE;
    nvram_cs_on();
    nvram_cmd(SER_WDS, 0);
    nvram_cs_off();

#ifdef CONSOLE_LED
    *cpu_auxctl = (*cpu_auxctl & ~CONSOLE_LED) | health_led_off;
#endif
}



int
nvram_is_protected(void)
{
	ushort ser_read = 0;
	int i;
#ifdef CONSOLE_LED
	unsigned char health_led_off = *cpu_auxctl & CONSOLE_LED;
#endif

	*cpu_auxctl |= NVRAM_PRE;
	nvram_cs_on();			/* enable chip select */
	nvram_cmd(SER_PRREAD, 0);

	/* clock the data out of serial mem */
	for (i = 0; i < 8; i++) {
		*cpu_auxctl &= ~SERCLK;
		*cpu_auxctl |= SERCLK;
		ser_read <<= 1;
		ser_read |= (*cpu_auxctl & SER_TO_CPU) ? 1 : 0;
	}

	nvram_cs_off();
	*cpu_auxctl &= ~NVRAM_PRE;

#ifdef CONSOLE_LED
	*cpu_auxctl = (*cpu_auxctl & ~CONSOLE_LED) | health_led_off;
#endif

	return ser_read != 0xff;
}



/* NVRAM users */

/* the PROM uses this routine for the PROM_EADDR entry point */
void
cpu_get_eaddr(u_char eaddr[])
{
	cpu_get_nvram_buf(NVOFF_ENET, NVLEN_ENET, (char *)eaddr);
}

/* These are all stubs for EEPROM based systems */

/* unconditionally unlock dallas lock bit - nop in IP22 */
void
cpu_nv_unlock(void) { }

/* get and set persistent environment variables */
/*ARGSUSED*/
void
cpu_get_nvram_persist(int (*putstr)(char *,char *,struct string_list *),
		      struct string_list *env) { }

/*ARGSUSED*/
void
cpu_set_nvram_persist(char *a,char *b) { }

/* initialize factory fresh nvram parts */
void
cpu_nvram_init(void) { }
#endif	/* IPXX */
