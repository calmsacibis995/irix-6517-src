#ident	"IP30diags/pon/pon_scache.c:  $Revision: 1.9 $"

#include "sys/types.h"
#include "sys/sbd.h"
#include "sys/cpu.h"
#include "libsk.h"
#include "libsc.h"
#include "uif.h"

#define	INT_SHIFT	32	/* shift to get bits 63..32 of a double word */

/* since the MRU bit always read back as a set bit ... */
#define	_CTS_MASK	(CTS_MASK & ~CTS_MRU)
#define	_CTS_TAGHI_MASK	(CTS_TAGHI_MASK & ~(CTS_MRU >> INT_SHIFT))

#define	ECC_MASK	0x3ff	/* C0_ECC mask */

static void
report_cache_error(__psunsigned_t addr, __uint64_t exp, __uint64_t act,
		   char *tname)
{
	exp = ip28_ssram_swizzle(exp);
	act = ip28_ssram_swizzle(act);
	printf( "Secondary Cache Failure: Address:  %#016x %s\n"
		"\t\t\t Expected: %#016x SRAM %s\n"
		"\t\t\t Actual:   %#016x\n",
		addr, tname, exp, ip28_ssram((void *)addr,exp,act), act);
}
static void
report_cache_tag_error(__psunsigned_t addr, __uint64_t exp, __uint64_t act,
		   char *tname)
{
	printf( "Secondary Cache Failure: Address:  %#016x %s\n"
		"\t\t\t Expected: %#016x SRAM U1\n"
		"\t\t\t Actual:   %#016x\n",
		addr, tname, exp, act);
}

/* check out the secondary cache using cacheops */
int
pon_scache(void)
{
	register __psunsigned_t	addr0;
	register __psunsigned_t	addr1;
	register __uint64_t	data_r;
	register __uint64_t	data_w;
	__uint32_t		ecc_r;
	__uint32_t		ecc_w;
	int			error = 0;
	int			i;
	__uint32_t		tag[2];
	__uint32_t		tag_mru[2];
	__uint32_t		tag_no_mru[2];
#ifdef VERBOSE
	extern __psint_t 	*Reportlevel;

	*Reportlevel = VRB;
#endif	/* VERBOSE */

#if 0	/* run before console/msg_printf ready -- will go to uart */
	msg_printf(VRB, "Secondary cache internal data/tag test\n");
#endif

	addr0 = K0_RAMBASE;
	addr1 = K0_RAMBASE | 0x1;		/* way 1 */

	/* walk 0/1's across the scache tag/way lines */
	for (data_w = 0x1;  data_w != 0x0; data_w <<= 1) {
		if (!(data_w & _CTS_MASK))	/* do read/write bit only */
			continue;

		tag[0] = (__uint32_t)(data_w & CTS_TAGLO_MASK);
		tag[1] = (__uint32_t)((data_w >> INT_SHIFT) & _CTS_TAGHI_MASK);
		_write_tag(CACH_SD, addr0, tag);

		tag[0] = (__uint32_t)(~data_w & CTS_TAGLO_MASK);
		tag[1] = (__uint32_t)((~data_w >> INT_SHIFT) & _CTS_TAGHI_MASK);
		_write_tag(CACH_SD, addr1, tag);

		_read_tag(CACH_SD, addr0, tag);
		data_r = tag[0] | ((__uint64_t)tag[1] << INT_SHIFT);
		data_r &= ~CTS_MRU;
		if (data_r != (data_w & _CTS_MASK)) {
			error++;
			report_cache_tag_error(addr0, data_w&_CTS_MASK, data_r,
				"TAG walking 1s");
		}

		_read_tag(CACH_SD, addr1, tag);
		data_r = tag[0] | ((__uint64_t)tag[1] << INT_SHIFT);
		data_r &= ~CTS_MRU;
		if (data_r != (~data_w & _CTS_MASK)) {
			error++;
			report_cache_tag_error(addr1, ~data_w&_CTS_MASK, data_r,
				"TAG walking 0s");
		}
	}

	/* see whether the MRU bit is actually shared between the 2 ways */
	tag_mru[0] = 0x0;
	tag_mru[1] = (__uint32_t)(CTS_MRU >> INT_SHIFT);
	tag_no_mru[0] = tag_no_mru[1] = 0x0;

	/* the 2nd call to _write_tag() is to clobber the signal line */
	_write_tag(CACH_SD, addr0, tag_mru);
	_write_tag(CACH_SD, addr0 + CACHE_SLINE_SIZE, tag_no_mru);
	_read_tag(CACH_SD, addr1, tag);
	if (!(((__uint64_t)tag[1] << INT_SHIFT) & CTS_MRU)){
		report_cache_error(addr0, 1, 0,
			"MRU bit set");
	}

	_write_tag(CACH_SD, addr0, tag_no_mru);
	_write_tag(CACH_SD, addr0 + CACHE_SLINE_SIZE, tag_mru);
	_read_tag(CACH_SD, addr1, tag);
	if (((__uint64_t)tag[1] << INT_SHIFT) & CTS_MRU){
		report_cache_error(addr0, 0, 1,
			"MRU bit clear");
	}

	_write_tag(CACH_SD, addr1, tag_mru);
	_write_tag(CACH_SD, addr1 + CACHE_SLINE_SIZE, tag_no_mru);
	_read_tag(CACH_SD, addr0, tag);
	if (!(((__uint64_t)tag[1] << INT_SHIFT) & CTS_MRU)){
		report_cache_error(addr1, 1, 0,
			"MRU bit set");
	}

	_write_tag(CACH_SD, addr1, tag_no_mru);
	_write_tag(CACH_SD, addr1 + CACHE_SLINE_SIZE, tag_mru);
	_read_tag(CACH_SD, addr0, tag);
	if (((__uint64_t)tag[1] << INT_SHIFT) & CTS_MRU){
		report_cache_error(addr1, 0, 1,
			"MRU bit clear");
	}

	/* re-initialize the tag to some valid values */
	tag[0] = tag[1] = 0x0;
	_write_tag(CACH_SD, addr0, tag);
	_write_tag(CACH_SD, addr1, tag);
	_write_tag(CACH_SD, addr0 + CACHE_SLINE_SIZE, tag);
	_write_tag(CACH_SD, addr1 + CACHE_SLINE_SIZE, tag);

	/* walk 0/1's across the upper 64 bits of the scache data/check lines */
	for (i = 0, data_w = 0x1;  data_w != 0x0; data_w <<= 1, i++) {
		ecc_w = 0x1 << (i % 10);

		tag[0] = (__uint32_t)data_w;
		tag[1] = (__uint32_t)(data_w >> INT_SHIFT);
		_write_cache_data(CACH_SD, addr0, tag, ecc_w);

		tag[0] = (__uint32_t)~data_w;
		tag[1] = (__uint32_t)(~data_w >> INT_SHIFT);
		_write_cache_data(CACH_SD, addr1, tag, ~ecc_w & ECC_MASK);

		ecc_r = _read_cache_data(CACH_SD, addr0, tag);
		data_r = tag[0] | ((__uint64_t)tag[1] << INT_SHIFT);
		if (data_r != data_w) {
			error++;
			report_cache_error(addr0, data_w, data_r,
				"DATA walking 1s");
		}
		if (ecc_r != ecc_w) {
			error++;
			report_cache_error(addr0, ecc_w, ecc_r,
				"ECC walking 1s");
		}

		ecc_r = _read_cache_data(CACH_SD, addr1, tag);
		data_r = tag[0] | ((__uint64_t)tag[1] << INT_SHIFT);
		if (data_r != ~data_w) {
			error++;
			report_cache_error(addr1, ~data_w, data_r,
				"DATA walking 0s");
		}
		if (ecc_r != (~ecc_w & ECC_MASK)) {
			error++;
			report_cache_error(addr1, ~ecc_w & ECC_MASK, ecc_r,
				"ECC walking 0s");
		}
	}

	/* walk 0/1's across the lower 64 bits of the scache data lines */
	addr0 += 0x8;
	addr1 += 0x8;
	for (data_w = 0x1;  data_w != 0x0; data_w <<= 1) {
		tag[0] = (__uint32_t)data_w;
		tag[1] = (__uint32_t)(data_w >> INT_SHIFT);
		_write_cache_data(CACH_SD, addr0, tag, 0x0);

		tag[0] = (__uint32_t)~data_w;
		tag[1] = (__uint32_t)(~data_w >> INT_SHIFT);
		_write_cache_data(CACH_SD, addr1, tag, 0x0);

		_read_cache_data(CACH_SD, addr0, tag);
		data_r = tag[0] | ((__uint64_t)tag[1] << INT_SHIFT);
		if (data_r != data_w) {
			error++;
			report_cache_error(addr0, data_w, data_r,
				"DATA walking 1s");
		}

		_read_cache_data(CACH_SD, addr1, tag);
		data_r = tag[0] | ((__uint64_t)tag[1] << INT_SHIFT);
		if (data_r != ~data_w) {
			error++;
			report_cache_error(addr1, ~data_w, data_r,
				"DATA walking 0s");
		}
	}

	/* re-initialize the data to some valid values */
	tag[0] = tag[1] = 0x0;
	_write_cache_data(CACH_SD, addr0, tag, 0x0);
	_write_cache_data(CACH_SD, addr1, tag, 0x0);

	return error;
}
