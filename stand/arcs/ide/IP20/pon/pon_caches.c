#ident	"IP20diags/pon/pon_caches.c:  $Revision: 1.13 $"

#include "sys/types.h"
#include "sys/cpu.h"
#include "sys/sbd.h"
#include <libsk.h>
#include "uif.h"

/*
 * WARNING: the PROM may not boot correctly after running this test if
 * 		the secondary cache is more than 2M since it overwrites PROM
 *		BSS/STACK locates between 0xa8740000 and 0xa8800000.
 *	    the dPROM may not boot correctly after running this test if
 *		the secondary cache is more than 1M since it overwrites dPROM
 *		TEXT locates above 0xa840000.
 */
#define	FIRST_ADDR	(K0_RAMBASE + 0x200000)
#define	SRAM_COUNT	8
#define	SRAM_SIZE	16

/* this test tries to verify the minimal functionality of the caches */
bool_t
pon_caches()
{
	extern u_int		_dcache_size;
	extern u_int		_sidcache_size;
	volatile u_short	*addr;
	u_int			cache_size;
	u_short			data;
	int			error = 0;
	int			i;
	int			j;

	msg_printf (VRB, "Secondary and primary caches address/data test\n");

	if (_sidcache_size)
		cache_size = _sidcache_size;
	else
		cache_size = _dcache_size;

	/*
	 * the secondary cache is organized as 8 64Kx16 static RAMs, we are
	 * going to walk a one across all 128 data lines.  we also want to
	 * check the interactions between the caches and memory
	 */
	for (i = 0; i < SRAM_COUNT * SRAM_SIZE; i++) {
		addr = (u_short *)FIRST_ADDR;
		for (j = 0; j < SRAM_COUNT; j++)
			addr[j] = 0x0;
		addr[i / SRAM_SIZE] = 0x1 << (i % SRAM_SIZE);

		/* force write back to memory */
		addr = (u_short *)(FIRST_ADDR + cache_size);
		for (j = 0; j < SRAM_COUNT; j++)
			addr[j] = 0xffff;
		addr[i / SRAM_SIZE] = ~(0x1 << (i % SRAM_SIZE));

		addr = (u_short *)FIRST_ADDR;
		for (j = 0; j < SRAM_COUNT; j++) {
			if (j == i / SRAM_SIZE) {
				if (addr[j] != 0x1 << (i % SRAM_SIZE)) {
					msg_printf(ERR,
						"Address: 0x%08x, Expected:0x%04x, Actual: 0x%04x\n", 
						(u_int)&addr[j], 0x1 << (i % SRAM_SIZE), addr[j]);
					error++;
				}
			} else {
				if (addr[j] != 0x0) {
					msg_printf(ERR,
						"Address: 0x%08x, Expected:0x0000, Actual: 0x%04x\n", 
						(u_int)&addr[j], addr[j]);
					error++;
				}
			}
		}

		/* force cache refill */
		addr = (u_short *)(FIRST_ADDR + cache_size);
		for (j = 0; j < SRAM_COUNT; j++) {
			if (j == i / SRAM_SIZE) {
				data = ~(0x1 << (i % SRAM_SIZE));
				if (addr[j] != data) {
					msg_printf(ERR,
						"Address: 0x%08x, Expected:0x%04x, Actual: 0x%04x\n", 
						(u_int)&addr[j], data, addr[j]);
					error++;
				}
			} else {
				if (addr[j] != 0xffff) {
					msg_printf(ERR,
						"Address: 0x%08x, Expected:0xffff, Actual: 0x%04x\n", 
						(u_int)&addr[j], addr[j]);
					error++;
				}
			}
		}
	}

	/*
	 * walk a one across the address lines
	 * since the lower 4 bits are not used to address the secondary cache
	 * RAM, we start from bit 4. 
	 */
	data = 0x0;
	for (i = 0x10; i < cache_size; i <<= 1) {
		addr = (u_short *)(FIRST_ADDR + i);
		for (j = 0; j < SRAM_COUNT; j++)
			addr[j] = data++;
	}

	for (i = 0x10; i < cache_size; i <<= 1) {
		addr = (u_short *)(FIRST_ADDR + i + cache_size);
		for (j = 0; j < SRAM_COUNT; j++)
			addr[j] = data++;
	}

	data = 0x0;
	for (i = 0x10; i < cache_size; i <<= 1) {
		addr = (u_short *)(FIRST_ADDR + i);
		for (j = 0; j < SRAM_COUNT; j++, data++) {
			if (addr[j] != data) {
				msg_printf(ERR,
					"Address: 0x%08x, Expected:0x%04x, Actual: 0x%04x\n", 
					(u_int)&addr[j], data, addr[j]);
				error++;
			}
		}
	}

	for (i = 0x10; i < cache_size; i <<= 1) {
		addr = (u_short *)(FIRST_ADDR + i + cache_size);
		for (j = 0; j < SRAM_COUNT; j++, data++) {
			if (addr[j] != data) {
				msg_printf(ERR,
					"Address: 0x%08x, Expected:0x%04x, Actual: 0x%04x\n", 
					(u_int)&addr[j], data, addr[j]);
				error++;
			}
		}
	}

	flush_cache();

	if (error)
		sum_error("Secondary/primary data caches address");
	else
		okydoky();

	return error;
}
