#ident	"IP30diags/pon/pon_caches.c:  $Revision: 1.1 $"

#include "sys/types.h"
#include "sys/sbd.h"
#include "sys/cpu.h"
#include "libsk.h"
#include "uif.h"

/*
 * pon_caches: verify minimal functionality of the caches by walking
 *		ones and zeros across the entire width of the data/address
 *		lines to each cache ram
 *
 * checks for:
 *	stuck data lines
 *	shorted data lines
 *	stuck address lines
 *	shorted address lines
 *	basic spill/fill functionality
 *
 * Note: assume 8x64Kx18 SRAMs configuration
 *	 memory from 0x1000000 up to 0x2000000 are being used as scratch,
 *	 this implies >= 32MB main memory in minimal configuration
 *
 */

extern void cache_K0(void);
extern void uncache_K0(void);

extern int _dcache_size;
extern int _sidcache_size;
extern int master_cpuid;

typedef __uint64_t	cdata_t;

/* test over the secondary cache if it exists, otherwise the primary cache */
#define	CACHE_SIZE		(_sidcache_size ? _sidcache_size : _dcache_size)
#define	SCACHE_BUS_WIDTH	128	/* in bits */
#define	CACHE_WIDE		(SCACHE_BUS_WIDTH / (8 * sizeof(cdata_t)))
#define	FLUSH_ADDR_BASE		(K0_RAMBASE + 0x800000)
#define	SCRATCH_ADDR_BASE	(K0_RAMBASE + 0x1000000)
#define	MAX_SCACHE_SIZE		0x400000
#define	MEM_ARENA_ADDR		(SCRATCH_ADDR_BASE + _cpuid * MAX_SCACHE_SIZE)

#define	EXPECT(base, index, expr, tname, id)				\
{									\
	volatile cdata_t	*_addr = (base) + (index);		\
	cdata_t		_exp = (expr);					\
	cdata_t		_act = *_addr;					\
	if (_exp != _act) {						\
		report_cache_error(_addr, _exp, _act, tname, id);	\
		error++;						\
	}								\
}

static void
report_cache_error(volatile cdata_t *addr, cdata_t exp, cdata_t act,
	char *tname, int id)
{
	if (id == master_cpuid) {
		uncache_K0();
		msg_printf(ERR,
			"Address: %#16x, Expected: %#16x, Actual: %#16x (%s)\n",
			addr, exp, act, tname);
		cache_K0();
	}
}

int
pon_caches()
{
	int			_cpuid = cpuid();
	volatile cdata_t	*addr;
	cdata_t			data;
	int			error = 0;
	volatile cdata_t	*flush_addr0;
	volatile cdata_t	*flush_addr1;
	int			i;
	cdata_t			increment;
	int			index;
	__uint64_t		offset;

	if (_cpuid == master_cpuid)
		msg_printf (VRB, "Secondary and primary data caches test\n");
	cache_K0();

	addr = (volatile cdata_t *)MEM_ARENA_ADDR;
	flush_addr0 = (volatile cdata_t *)FLUSH_ADDR_BASE;
	flush_addr1 = (volatile cdata_t *)(FLUSH_ADDR_BASE + CACHE_SIZE / 2);

	/* cache ram data and spill/fill tests */
	for (index = 0; index < CACHE_WIDE; index++) {

		/* walking data ones tests */
		for (data = 0x1; data != 0x0; data <<= 1) {
			/* clear across the data lines */
			for (i = 0; i < CACHE_WIDE; i++)
	    			addr[i] = 0x0;

			/* write a single bit to "1" */
			addr[index] = data;

			/* force data to memory, cache is 2-way associative */
			*flush_addr0;
			*flush_addr1;

			for (i = 0; i < CACHE_WIDE; i++)
				if (i != index)		/* verify the data */
					EXPECT(addr, i, 0x0,
						"scache data walking 1s",
						_cpuid);
			EXPECT(addr, index, data, "scache data walking 1s",
				_cpuid);
		}

		/* walking data zeros tests */
		for (data = 0x1; data != 0x0; data <<= 1) {
			/* fill across the data lines */
			for (i = 0; i < CACHE_WIDE; i++)
				addr[i] = ~0x0;

			/* write a single bit to "0" */
			addr[index] = ~data;

			/* force data to memory, cache is 2-way associative */
			*flush_addr0;
			*flush_addr1;

			for (i = 0; i < CACHE_WIDE; i++)
				if (i != index)		/* verify the data */
					EXPECT(addr, i, ~0x0,
						"scache data walking 0s",
						_cpuid);
			EXPECT(addr, index, ~data, "scache data walking 0s",
				_cpuid);
		}
	}

	/* re-initialize part of the memory being used */
	for (i = 0; i < CACHE_WIDE; i++)
		addr[i] = 0x0;

	/* cache ram bank addressing tests */
	/*
	 * the values for data and increment are choosen such that data
	 * written to each x18 SRAM are different.  initial address starts
	 * at 0x10 since the lowest 4 bits are not used to address the SRAM
	 */
#define	INITIAL_DATA	0x0003000200010000
#define	INITIAL_ADDR	0x10
#define	ADDR_MSK	((CACHE_SIZE - 1) & ~0xf)

	increment = 0x0004000400040004;

	 /* walk a "1" across the address lines, writing distinct data */
	data = INITIAL_DATA;
	for (offset = INITIAL_ADDR; offset < CACHE_SIZE; offset <<= 1) {
		index = (int)(offset / sizeof(cdata_t));
		addr[index] = data;
		data += increment;
		addr[++index] = data;
		data += increment;
	}

	data = INITIAL_DATA;
	for (offset = INITIAL_ADDR; offset < CACHE_SIZE; offset <<= 1) {
		index = (int)(offset / sizeof(cdata_t));
		EXPECT(addr, index, data, "scache address walking 1s",
			_cpuid);
		data += increment;
		EXPECT(addr, ++index, data, "scache address walking 1s",
			_cpuid);
		data += increment;
	}

	/* re-initialize part of the memory being used */
	for (offset = INITIAL_ADDR; offset < CACHE_SIZE; offset <<= 1) {
		index = (int)(offset / sizeof(cdata_t));
		addr[index] = 0x0;
		addr[++index] = 0x0;
	}

	 /* walk a "0" across the address lines, writing distinct data */
	data = ~INITIAL_DATA;
	for (offset = INITIAL_ADDR; offset < CACHE_SIZE; offset <<= 1) {
		index = (int)((~offset & ADDR_MSK) / sizeof(cdata_t));
		addr[index] = data;
		data -= increment;
		addr[++index] = data;
		data -= increment;
	}

	data = ~INITIAL_DATA;
	for (offset = INITIAL_ADDR; offset < CACHE_SIZE; offset <<= 1) {
		index = (int)((~offset & ADDR_MSK) / sizeof(cdata_t));
		EXPECT(addr, index, data, "scache address walking 0s",
			_cpuid);
		data -= increment;
		EXPECT(addr, ++index, data, "scache address walking 0s",
			_cpuid);
		data -= increment;
	}

	/* re-initialize part of the memory being used */
	for (offset = INITIAL_ADDR; offset < CACHE_SIZE; offset <<= 1) {
		index = (int)((~offset & ADDR_MSK) / sizeof(cdata_t));
		addr[index] = 0x0;
		addr[++index] = 0x0;
	}

	flush_cache();

	uncache_K0();
	if (_cpuid == master_cpuid) {
		if (error)
			sum_error("Secondary and primary data caches");
		else
			okydoky();
	}

	return error;
}
