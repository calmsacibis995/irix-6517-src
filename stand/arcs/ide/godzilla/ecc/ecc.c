/*
 * ecc.c - IDE ECC checking mechanism test
 *
 * IP30 has a new ECC feature that allows to test the ECC mechanism
 * in the HEART chip.
 *
 * Copyright 1995, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 *
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 *
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 *
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 */
#ident "ide/godzilla/ecc/ecc.c: $Revision 1.1$"

/* done for IP30:
 *	- created ecc_testerrgen and ecc_errgen 
 *	- kept ecc_testsdlr from IP26
 * left to do: XXX
 * 	- write the XBOW to memory ECC test (?)
 * 	- write the ECC ram test ( part of mem test??)
 */

#include "sys/types.h"
#include "sys/cpu.h"
#include "sys/sbd.h"
#include "libsk.h"
#include "libsc.h"
#include "pon.h"		/* run_* protos */
#include "uif.h"
#include "d_ecc.h"
#include "sys/RACER/heart.h"
#include "d_godzilla.h"  
#include "d_frus.h"  
#include "d_prototypes.h"
#include "d_mem.h"


/*
 * Forward References
 */
static	bool_t _ecc_testsdlr(void);
static	bool_t _ecc_errgen(bool_t single_double);
static	bool_t _ecc_heartecc(bool_t single_double);
static	bool_t _ecc_testheartecc(void);
bool_t	ecc_test(int argc, char **argv);

/*
 * External Declarations
 */
extern	int 	ecc_sdlr(unsigned long *addr, long offset, long value);
extern	int	_ecc_testerrgen(void);



/* function name:       _ecc_testsdlr
 *      input:          none
 *      output:         error flag
 *                      1 = not OK, 0 = OK
 *      description:    tests the memory ECC with different data patterns
 *	remark:		originally in IP26
 */
static bool_t
_ecc_testsdlr(void)
{
	register	bool_t	ok = 1;
	__uint64_t	expect, addr, actual;
	__int64_t	value, op;
	__uint64_t	*addrptr = (__uint64_t *)PHYS_TO_K1(&addr);

	/* need to avoid R10k dirty writebacks, so make sure we run uncached */
	run_uncached();
	flush_cache();

	*addrptr= 0x1122334455667788L;
	value   = 0x8877665544332211L;
	op      = 0L;
	expect  = 0x8877665544332211L;
	if ((actual = ecc_sdlr(addrptr, op, value)) != expect) {
		msg_printf(DBG,
			"ecc sdr/sdl (op %ld): expect 0x%lx, actual 0x%lx\n",
			op, expect, actual);
		ok = 0;
	}
	*addrptr= 0x1122334455667788L;
	value   = 0x8877665544332211L;
	op      = 1L;
	expect  = 0x5588776655443322L;
	if ((actual = ecc_sdlr(addrptr, op, value)) != expect) {
		msg_printf(DBG,
			"ecc sdr/sdl (op %ld): expect 0x%lx, actual 0x%lx\n",
			op, expect, actual);
		ok = 0;
	}
	*addrptr= 0x1122334455667788L;
	value   = 0x8877665544332211L;
	op      = 4L;
	expect  = 0x5544332288776655L;
	if ((actual = ecc_sdlr(addrptr, op, value)) != expect) {
		msg_printf(DBG,
			"ecc sdr/sdl (op %ld): expect 0x%lx, actual 0x%lx\n",
			op, expect, actual);
		ok = 0;
	}
	*addrptr= 0x1122334455667788L;
	value   = 0x8877665544332211L;
	op      = 6L;
	expect  = 0x5544332211668877L;
	if ((actual = ecc_sdlr(addrptr, op, value)) != expect) {
		msg_printf(DBG,
			"ecc sdr/sdl (op %ld): expect 0x%lx, actual 0x%lx\n",
			op, expect, actual);
		ok = 0;
	}
	*addrptr= 0x1122334455667788L;
	value   = 0x8877665544332211L;
	op      = 7L;
	expect  = 0x5544332211667788L;
	if ((actual = ecc_sdlr(addrptr, op, value)) != expect) {
		msg_printf(DBG,
			"ecc sdr/sdl (op %ld): expect 0x%lx, actual 0x%lx\n",
			op, expect, actual);
		ok = 0;
	}

	run_cached();

	return ok ? 0 : 1;
}

/* From the heart spec:   (somewhat unclear)
 * ====================
 * single bit  error generation:
 * 	use Bit 19 in HEART MODE register: SB_ErrGen
 * Single bit error generation. Reset to 0. When this bit is set, the HEART
 * generates a single-bit ECC error on data written by CPU to memory in a 
 * DSPW Write Request. This can be used to write a single-bit ECC 
 * error diagnostic. The erred data is
 * first written to memory. Then the memory location is read. The error de
 * tection and/or correction logic in the HEART and in the R10k processor can
 * then be tested
 * Note that the error is not generated for Processor Block Write Requests.
 * 
 * double bit  error generation:
 * 	use Bit 19 in HEART MODE register: DB_ErrGen
 * Double bit error generation. Reset to 0. When this bit is set, the HEART
 * generates a double-bit ECC error on data that is written by CPU to 
 * memory in a DSPW Write Request. 
 * This can be used to write a double-bit ECC error diagnostic. The
 * erred data is first written to memory. Then the memory location is read.
 * The error detection logic in the HEART and in the R10k processor can then
 * be tested.
 * Note that the error is not generated for Processor Block Write Requests.
 */


/* IDE entry point for ECC diags.
 */
/*ARGSUSED*/
int
ecc_test(int argc, char **argv)
{
	heartreg_t tmp_hr_buf[2] = {HEART_MODE,D_EOLIST};
	heartreg_t tmp_hr_save[2];
	char *test_name = "ECC test";
	int errors = 0;

	printf("ECC Test\n");

	/* save all readable/writable H registers that are written to */
	_hr_save_regs(tmp_hr_buf, tmp_hr_save);

	/* test memory access from the CPU side */
	msg_printf(INFO,"\n%s: sdl/sdr test\n", test_name);
	errors += _ecc_testsdlr();		/* test SDL/SDR */
	msg_printf(DBG,"\n%s: sdl/sdr done (errors=%d)\n", test_name, errors);

	/* test the memory ECC feature via the HEART mode register */
	msg_printf(INFO,"\n%s: error generation\n", test_name);
	errors += _ecc_testerrgen();		
	msg_printf(DBG,"\n%s: testerrgen done (errors=%d)\n",test_name,errors);

	/* test memory access from the Xbow side (XXX not done) */

	/* test ECC ram XXX */

	msg_printf(DBG, "ECC test done\n");

_error:
	/* reset the heart/bridge in case of an error */
	if (_hb_reset(RES_HEART, RES_BRIDGE)) errors++;
	
	/* restore all readable/writable H registers that are written to */
	_hr_rest_regs(tmp_hr_buf, tmp_hr_save);

        /* top level function must report the pass/fail status */
#ifdef NOTNOW
        REPORT_PASS_OR_FAIL(errors, "ECC", D_FRU_MEM);
#endif
        REPORT_PASS_OR_FAIL( &CPU_Test_err[CPU_ERR_ECC_0000], errors );
}

