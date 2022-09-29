#ident	"IP30diags/pon/pon_scache1.c:  $Revision: 1.11 $"

#ifdef R10000

#include "sys/types.h"
#include "sys/sbd.h"
#include "sys/cpu.h"
#include "libsk.h"
#include "libsc.h"
#include "uif.h"
#include "IP30pon.h"

#define	INT_SHIFT	32	/* shift to get bits 63..32 of a double word */

/* since the MRU bit always read back as a set bit ... */
#define	_CTS_MASK	(CTS_MASK & ~CTS_MRU)
#define	_CTS_TAGHI_MASK	(CTS_TAGHI_MASK & ~(CTS_MRU >> INT_SHIFT))

#define	ECC_MASK	0x3ff	/* C0_ECC mask */

#define REPORT_CACHE_ERROR(A,B,C,D,E) \
	report_cache_error((volatile cdata_t *)(A),B,C,D,E)

/* check out the secondary cache using cacheops */
int
pon_scache(void)
{
	int			_cpuid = cpuid();
	__psunsigned_t		addr0;
	__psunsigned_t		addr1;
	__uint64_t		data_r;
	__uint64_t		data_w;
	__uint64_t		ecc_r;
	__uint64_t		ecc_w;
	int			error = 0;
	int			i;
	extern void		init_caches(void);
	__psunsigned_t		offset;
	static __uint64_t	patterns[] = {
					0xffffffff00000000, ~0xffffffff00000000,
					0xffff0000ffff0000, ~0xffff0000ffff0000,
					0xff00ff00ff00ff00, ~0xff00ff00ff00ff00,
					0xf0f0f0f0f0f0f0f0, ~0xf0f0f0f0f0f0f0f0,
					0xcccccccccccccccc, ~0xcccccccccccccccc,
					0xaaaaaaaaaaaaaaaa, ~0xaaaaaaaaaaaaaaaa,
				};
#define	PATTERN_SIZE		(sizeof(patterns) / sizeof(__uint64_t))
	int			sidcache_size = sCacheSize() / 2;
	__uint32_t		tag[2];

#ifdef VERBOSE
	pon_puts("Processor ");
	pon_putc(_cpuid + '0');
	pon_puts(" secondary cache internal data/tag test ");
	if (jumper_off())
		pon_puts("(extensive)");
	pon_puts("\r\n");
#endif	/* VERBOSE */

	/* tag sram address test using just the tag field */
#ifdef VERBOSE
	pon_puts("        SCACHE TAG SRAM address test\r\n");
#endif	/* VERBOSE */

	/* walk a one across the address line */
	for (offset = CACHE_SLINE_SIZE, data_w = 0x0;
	     offset < sidcache_size;
	     offset <<= 1, data_w += 0x1 << CTS_TAG_SHFT) {
		addr0 = K0_RAMBASE + offset;

		tag[0] = (__uint32_t)data_w;
		tag[1] = (__uint32_t)(data_w >> INT_SHIFT);
		_write_tag(CACH_SD, addr0, tag);

		tag[0] = (__uint32_t)(~data_w & CTS_TAG_MASK);
		tag[1] = (__uint32_t)((~data_w & CTS_TAG_MASK) >> INT_SHIFT);
		_write_tag(CACH_SD, addr0|0x1, tag);
	}

	for (offset = CACHE_SLINE_SIZE, data_w = 0x0;
	     offset < sidcache_size;
	     offset <<= 1, data_w += 0x1 << CTS_TAG_SHFT) {
		addr0 = K0_RAMBASE + offset;

		_read_tag(CACH_SD, addr0, tag);
		data_r = tag[0] | ((__uint64_t)tag[1] << INT_SHIFT);
		if (data_r != data_w) {
			error++;
			report_cache_tag_error((volatile cdata_t *)addr0,
				data_w, data_r,
				"SCACHE TAG SRAM address", _cpuid);
		}

		_read_tag(CACH_SD, addr0|0x1, tag);
		data_r = tag[0] | ((__uint64_t)tag[1] << INT_SHIFT);
		if (data_r != (~data_w & CTS_TAG_MASK)) {
			error++;
			report_cache_tag_error((volatile cdata_t *)(addr0|0x1),
				~data_w & CTS_TAG_MASK, data_r,
				"SCACHE TAG SRAM address", _cpuid);
		}
	}

	/* data sram address test */
#ifdef VERBOSE
	pon_puts("        SCACHE DATA SRAM address test\r\n");
#endif	/* VERBOSE */

	for (offset = 0x10, data_w = 0x0;
	     offset < sidcache_size;
	     offset <<= 1, data_w++) {
		addr0 = K0_RAMBASE + offset;

		tag[0] = (__uint32_t)data_w;
		tag[1] = (__uint32_t)(data_w >> INT_SHIFT);
		_write_cache_data(CACH_SD, addr0, tag, data_w & ECC_MASK);

		tag[0] = (__uint32_t)~data_w;
		tag[1] = (__uint32_t)(~data_w >> INT_SHIFT);
		_write_cache_data(CACH_SD, addr0|0x1, tag, ~data_w & ECC_MASK);
	}

	for (offset = 0x10, data_w = 0x0;
	     offset < sidcache_size;
	     offset <<= 1, data_w++) {
		addr0 = K0_RAMBASE + offset;

		ecc_r = _read_cache_data(CACH_SD, addr0, tag);
		data_r = tag[0] | ((__uint64_t)tag[1] << INT_SHIFT);
		if (data_r != data_w) {
			error++;
			REPORT_CACHE_ERROR(addr0, data_w, data_r,
				"SCACHE DATA SRAM address", _cpuid);
		}
		if (ecc_r != (data_w & ECC_MASK)) {
			error++;
			REPORT_CACHE_ERROR(addr0, data_w & ECC_MASK, ecc_r,
				"SCACHE DATA SRAM ECC address", _cpuid);
		}

		ecc_r = _read_cache_data(CACH_SD, addr0|0x1, tag);
		data_r = tag[0] | ((__uint64_t)tag[1] << INT_SHIFT);
		if (data_r != ~data_w) {
			error++;
			REPORT_CACHE_ERROR(addr0|0x1, ~data_w, data_r,
				"SCACHE DATA SRAM address", _cpuid);
		}
		if (ecc_r != (~data_w & ECC_MASK)) {
			error++;
			REPORT_CACHE_ERROR(addr0|0x1, ~data_w & ECC_MASK, ecc_r,
				"SCACHE DATA SRAM ECC address", _cpuid);
		}
	}

	/* tag sram data test */
#ifdef VERBOSE
	pon_puts("        SCACHE TAG SRAM data test\r\n");
#endif	/* VERBOSE */

	if (!jumper_off())
		sidcache_size = CACHE_SLINE_SIZE;
	for (addr0 = K0_RAMBASE, addr1 = K0_RAMBASE|0x1;
	     addr0 < K0_RAMBASE + sidcache_size;
	     addr0 += CACHE_SLINE_SIZE, addr1 += CACHE_SLINE_SIZE) {

		for (i = 0; i < PATTERN_SIZE; i++) {
			tag[0] = (__uint32_t)(patterns[i] & CTS_TAGLO_MASK);
			tag[1] = (__uint32_t)((patterns[i] >> INT_SHIFT) &
				_CTS_TAGHI_MASK);
			_write_tag(CACH_SD, addr0, tag);

			tag[0] = (__uint32_t)(~patterns[i] & CTS_TAGLO_MASK);
			tag[1] = (__uint32_t)((~patterns[i] >> INT_SHIFT) &
				_CTS_TAGHI_MASK);
			_write_tag(CACH_SD, addr1, tag);

			_read_tag(CACH_SD, addr0, tag);
			data_r = tag[0] | ((__uint64_t)tag[1] << INT_SHIFT);
			data_r &= ~CTS_MRU;
			if (data_r != (patterns[i] & _CTS_MASK)) {
				error++;
				report_cache_tag_error((
					volatile cdata_t *)addr0,
					patterns[i] & _CTS_MASK, data_r,
					"SCACHE TAG SRAM data", _cpuid);
			}

			_read_tag(CACH_SD, addr1, tag);
			data_r = tag[0] | ((__uint64_t)tag[1] << INT_SHIFT);
			data_r &= ~CTS_MRU;
			if (data_r != (~patterns[i] & _CTS_MASK)) {
				error++;
				report_cache_tag_error((
					volatile cdata_t *)addr1,
					~patterns[i] & _CTS_MASK, data_r,
					"SCACHE TAG SRAM data", _cpuid);
			}
		}
	}

	/* data sram data test */
#ifdef VERBOSE
	pon_puts("        SCACHE DATA SRAM data test\r\n");
#endif	/* VERBOSE */

	if (!jumper_off())
		sidcache_size = 0x10;
	for (addr0 = K0_RAMBASE, addr1 = K0_RAMBASE|0x1;
	     addr0 < K0_RAMBASE + sidcache_size;
	     addr0 += 0x8, addr1 += 0x8) {

next_double_word:
		for (i = 0; i < PATTERN_SIZE; i++) {
			tag[0] = (__uint32_t)patterns[i];
			tag[1] = (__uint32_t)(patterns[i] >> INT_SHIFT);
			_write_cache_data(CACH_SD, addr0, tag,
				patterns[i] & ECC_MASK);

			tag[0] = (__uint32_t)~patterns[i];
			tag[1] = (__uint32_t)(~patterns[i] >> INT_SHIFT);
			_write_cache_data(CACH_SD, addr1, tag,
				~patterns[i] & ECC_MASK);

			ecc_r = _read_cache_data(CACH_SD, addr0, tag);
			data_r = tag[0] | ((__uint64_t)tag[1] << INT_SHIFT);
			if (data_r != patterns[i]) {
				error++;
				REPORT_CACHE_ERROR(addr0, patterns[i], data_r,
					"SCACHE DATA SRAM data", _cpuid);
			}
			if (ecc_r != (patterns[i] & ECC_MASK)) {
				error++;
				REPORT_CACHE_ERROR(addr0,
					patterns[i] & ECC_MASK, ecc_r,
					"SCACHE DATA SRAM ECC data", _cpuid);
			}

			ecc_r = _read_cache_data(CACH_SD, addr1, tag);
			data_r = tag[0] | ((__uint64_t)tag[1] << INT_SHIFT);
			if (data_r != ~patterns[i]) {
				error++;
				REPORT_CACHE_ERROR(addr1, ~patterns[i], data_r,
					"SCACHE DATA SRAM data", _cpuid);
			}
			if (ecc_r != (~patterns[i] & ECC_MASK)) {
				error++;
				REPORT_CACHE_ERROR(addr1,
					~patterns[i] & ECC_MASK, ecc_r,
					"SCACHE DATA SRAM ECC data", _cpuid);
			}
		}

		/*
		 * since scache data cacheops access only half of a quadword
		 * at a time, go back and do the second half
		 */
		if ((addr0 & 0x8) == 0x0) {
			addr0 += 0x8;
			addr1 += 0x8;
			goto next_double_word;
		}
	}

	init_caches();

	return error;
}
#endif	/* R10000 */
