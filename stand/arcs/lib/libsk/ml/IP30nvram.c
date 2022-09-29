/* IP30 nvram code */

#ident "$Revision: 1.22 $"

#include <sys/types.h>
#include <sys/immu.h>
#include <sys/cpu.h>
#include <sys/ds1687clk.h>
#include <sys/RACER/IP30nvram.h>
#include <libsk.h>
#include <libsc.h>

#include <sys/RACER/sflash.h>

void ip30_init_cpu_disable(void);

static uchar_t	nvram_content[NVLEN_MAX];
static int	nvram_read;

/* nvchecksum calculates the checksum for the nvram */
static uchar_t
nvchecksum(void)
{
	/*
	 * seed the checksum so all-zeroes (all-ones) nvram doesn't have a zero
	 * (all-ones) checksum
	 */
	char		bitbucket[2];		/* extra byte for NULL */
	uchar_t		checksum = 0xa5;
	int		i;

	/* read the whole nvram into memory */
	if (!nvram_read)
		cpu_get_nvram_buf(NVOFF_CHECKSUM, NVLEN_CHECKSUM, bitbucket);

	/* NVOFF_REVISION must be the first checksummed offset, with 
	 * NVOFF_CHECKSUM before it.
	 */
	for (i = NVOFF_REVISION; i < NVLEN_MAX; i++) {
		checksum ^= nvram_content[i];

		/* rotate checksum */
		checksum = (checksum << 1) | (checksum >> 7);
	}

	return checksum;
}

/*
 * cpu_get_nvram_buf is the same as cpu_get_nvram_offset() except putting
 * the read string in the given buffer
 */
void
cpu_get_nvram_buf(int nv_off, int nv_len, char buf[])
{
	int		i;
	extern int	nic_get_eaddr(__int32_t *, __int32_t *, char *);
	int		nv_last = nv_off + nv_len;

	if (nv_off == NVOFF_ENET && nv_len == NVLEN_ENET) {
		(void)nic_get_eaddr(
			(__int32_t *)(IOC3_PCI_DEVIO_K1PTR + IOC3_MCR),
			(__int32_t *)(IOC3_PCI_DEVIO_K1PTR + IOC3_GPCR_S),
			buf);
		buf[6] = 0;
		return;
	}

	/* variable is a PDS variable.  Ignore it. */
	if (nv_off == NVOFF_PDSVAR) {
		buf[0] = 0;
		return;
	}

	/* save all nvram variables in memory for speed */
	if (!nvram_read) {
		ioc3reg_t	old;

		old = slow_ioc3_sio();

		/* access the first 2 sets of nvram variables through bank 0 */
		WR_DS1687_RTC(RTC_CTRL_A, RTC_OSCILLATOR_ON);
		for (i = NVRAM_SET_0_MIN; i <= NVRAM_SET_1_MAX; i++)
			nvram_content[i - NVRAM_REG_OFFSET] = RD_DS1687_RTC(i);

		/* must access the last set of nvram variables through bank 1 */
		WR_DS1687_RTC(RTC_CTRL_A,
			RTC_SELECT_BANK_1 | RTC_OSCILLATOR_ON);
		for (i = NVRAM_SET_2_MIN; i <= NVRAM_SET_2_MAX; i++) {
			WR_DS1687_RTC(RTC_X_RAM_ADDR, i & 0x7f);
			nvram_content[i - NVRAM_REG_OFFSET] =
				RD_DS1687_RTC(RTC_X_RAM_DATA);
		}

		restore_ioc3_sio(old);

		nvram_read = 1;
	}

	if (nv_last > NVLEN_MAX)
		nv_last = NVLEN_MAX;

	for (i = nv_off; i < nv_last; i++)
		*buf++ = nvram_content[i];
	*buf = 0;
}

/*
 * cpu_get_nvram reads a string with the given length from the nvram
 * starting at the specified byte offset
 */
char *
cpu_get_nvram_offset(int nv_off, int nv_len)
{
	/* add 1 for the NULL terminator */
	static char	buf[NVLEN_MAX + 1];

	cpu_get_nvram_buf(nv_off, nv_len, buf);

	return buf;
}

/*
 * cpu_set_nvram_offset writes the specified string with the given length to
 * the nvram starting at the specified byte offset
 */
int
cpu_set_nvram_offset(int nv_off, int nv_len, char *string)
{
	uchar_t		checksum;
	int		i;
	int		nv_last = nv_off + nv_len - 1;
	ioc3reg_t	old;

	if (nv_last >= NVLEN_MAX)		/* covers PDS case */
		return -1;

	nv_off += NVRAM_REG_OFFSET;
	nv_last += NVRAM_REG_OFFSET;

	old = slow_ioc3_sio();

	/* access the first 2 sets of nvram variables through bank 0 */
	if (nv_off <= NVRAM_SET_1_MAX)
		WR_DS1687_RTC(RTC_CTRL_A, RTC_OSCILLATOR_ON);
	while (nv_off <= nv_last && nv_off <= NVRAM_SET_1_MAX) {
		WR_DS1687_RTC(nv_off, *string);
		nvram_content[nv_off - NVRAM_REG_OFFSET] = *string;
		nv_off++;
		string++;
	}

	/* must access the last set of nvram variables through bank 1 */
	if (nv_off <= nv_last)
		WR_DS1687_RTC(RTC_CTRL_A,
			RTC_SELECT_BANK_1 | RTC_OSCILLATOR_ON);
	while (nv_off <= nv_last && nv_off <= NVRAM_SET_2_MAX) {
		WR_DS1687_RTC(RTC_X_RAM_ADDR, nv_off & 0x7f);
		WR_DS1687_RTC(RTC_X_RAM_DATA, *string);
		nvram_content[nv_off - NVRAM_REG_OFFSET] = *string;
		nv_off++;
		string++;
	}

	checksum = nvchecksum();
	WR_DS1687_RTC(NVOFF_CHECKSUM + NVRAM_REG_OFFSET, checksum);

	restore_ioc3_sio(old);

	return 0;
}

/*
 * cpu_set_nvram sets the nvram string identified by 'match' to the
 * string identified by 'newstring'
 */
int
cpu_set_nvram(char *match, char *newstring)
{
	/*  Most systems only let the prom do this.  It's only libsk programs,
	 * and symmon stubs everything, so this lets us set variables from
	 * ide, which we need for the mfg process.
	 */
	return setenv_nvram(match, newstring);
}

/* cpu_is_nvvalid returns 1 if the nvram is valid, 0 otherwise */
static int
rev_is_nvvalid(unsigned char revision)
{
	uchar_t checksum;
	char byte;

	checksum = *cpu_get_nvram_offset(NVOFF_CHECKSUM, NVLEN_CHECKSUM);

	if (checksum != nvchecksum()) {
		/* Check if only cpumask is incorrect -- may happen on a
		 * failed bootstrap -> NMI with diagmode hook.
		 */
		if (!(nvram_content[NVOFF_CPUDISABLE+1] & CPU_DISABLE_INVALID))
			return 0;
		nvram_content[NVOFF_CPUDISABLE+1] ^= CPU_DISABLE_INVALID;
		if (checksum != nvchecksum())
			return 0;

		printf("Clearing CPU disable mask with diagmode NMI hook.\n");
		cpu_set_nvram_offset(NVOFF_BOOTMASTER,NVLEN_BOOTMASTER,"");
		ip30_init_cpu_disable();	/* enable everything */
	}

	if ((uchar_t)*cpu_get_nvram_offset(NVOFF_REVISION, NVLEN_REVISION) !=
	    revision) {
		return 0;
	}

	return 1;
}

int 
cpu_is_nvvalid(void)
{
	return rev_is_nvvalid(NV_CURRENT_REV);
}

/*ARGSUSED*/
void
cpu_get_nvram_persist(int (*putstr)(char *,char *,struct string_list *),
                      struct string_list *env)
{
	flash_loadenv(putstr, env);
}

/*ARGSUSED*/
void
cpu_set_nvram_persist(char *a,char *b)
{
	(void)flash_setenv(a, b);
}

static void
init_nv_counters(void)
{
	int val;
	char *cp = (char *)&val;

	/*
	 * initialize with values from flash headers
	 * if flash hdr is bad we will get 0
	 */
	val = flash_get_nflashed(SFLASH_RPROM_SEG);
	cpu_set_nvram_offset(NVOFF_NFLASHEDRP,NVLEN_NFLASHEDRP,cp);

	val = flash_get_nflashed(SFLASH_FPROM_SEG);
	cpu_set_nvram_offset(NVOFF_NFLASHEDFP,NVLEN_NFLASHEDFP,cp);

	val = flash_get_nflashed(SFLASH_PDS_SEG);
	cpu_set_nvram_offset(NVOFF_NFLASHEDPDS,NVLEN_NFLASHEDPDS,cp);

	val = 0;
	cpu_set_nvram_offset(NVOFF_UPTIME,NVLEN_UPTIME,cp);
}

/*
 * cpu_set_nvvalid validates the nvram by updating the revision and checksum
 * entries
 */
void
cpu_set_nvvalid(void (*delstr)(char *,struct string_list *),
	struct string_list *env)
{
	char		c;

	/* cpu_set_nvram_offset() will generate the correct checksum too */
	c = (uchar_t)NV_CURRENT_REV;
	cpu_set_nvram_offset(NVOFF_REVISION, NVLEN_REVISION, &c);
}

/* cpu_nv_lock locks the nvram if lock==1, unlocks otherwise */
void
cpu_nv_lock(int lock)
{
}

/* nvram_is_protected returns 1 if the nvram is protected, 0 otherwise */
int
nvram_is_protected(void)
{
	return 0;
}

void
ip30_init_cpu_disable(void)
{
	char buf[2];

	buf[0] = CPU_DISABLE_MAGIC >> 8;
	buf[1] = CPU_DISABLE_MAGIC & 0xff;
	cpu_set_nvram_offset(NVOFF_CPUDISABLE,NVLEN_CPUDISABLE,buf);
}

static int oldrev;

int
cpu_is_oldnvram(void)
{
	int rc = 0;

	oldrev = 0;

	if (rev_is_nvvalid(2)) {
		oldrev = 2;
		rc = 1;
	}
	else if (rev_is_nvvalid(3)) {
		oldrev = 3;
		rc = 1;
	}
	else if (rev_is_nvvalid(4)) {
		oldrev = 4;
		rc = 1;
	}
	else if (rev_is_nvvalid(5)) {
		oldrev = 5;
		rc = 1;
	}
	else if (rev_is_nvvalid(6)) {
		oldrev = 6;
		rc = 1;
	}

	/* always reset cpu mask when nvram checksum is wrong */ 
	ip30_init_cpu_disable();

	/* bad checksum and we dont recognize the version, init counters */ 
	if (rc == 0)
		init_nv_counters();

	return rc;
}

void
cpu_update_nvram(void)
{
	void initialize_envstrs(void);

	if (oldrev == 0) return;

	/* no compat hook for rev 0 -> rev 1 */

	if (oldrev <= 2) {
		cpu_set_nvram_offset(NVOFF_BOOTMASTER,NVLEN_BOOTMASTER,"\000");
	}

	if (oldrev <= 3) {
		cpu_set_nvram_offset(NVOFF_AUTOPOWER,NVLEN_AUTOPOWER,"y");
	}

	if (oldrev <= 4) {
		init_nv_counters();
	}

	if (oldrev <= 5) {
		cpu_set_nvram_offset(NVOFF_UNCACHEDPROM,NVLEN_UNCACHEDPROM,"");
	}

	if (oldrev <= 6) {
		unsigned short zero = 0;
		char *cp = (char *)&zero;

		cpu_set_nvram_offset(NVOFF_RPROMFLAGS,NVLEN_RPROMFLAGS,cp);
		/*
		 * boottune is not being added but most of systems have
		 * systems with garbage in them so take this chance to
		 * set initial values to it
		 */
		cpu_set_nvram_offset(NVOFF_BOOTTUNE,NVLEN_BOOTTUNE,"1");
	}

	oldrev = 0;

	initialize_envstrs();		/* normal env set-up */
}

void
cpu_resetenv(void)
{
	/* always clear processor disable mask for saftey */
	ip30_init_cpu_disable();

	/* XXX validate dallas flash/uptime variables (may be corrupt) */

	/* clear Flash PDS env vars */
	flash_resetenv();
}
