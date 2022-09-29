#ident	"IP22diags/mem/private.c:  $Revision: 1.1 $"

/*  
 * private.c - functions private to IP22diags/mem used to test parity
 */

#include "private.h"
#include "sys/cpu.h"
#include "sys/sbd.h"

extern int _cpu_parerr_save;
extern int _cpu_paraddr_save;

unsigned char _parexpectsignature[] = {
#ifdef	_MIPSEL
	ERR_STAT_B0,		/* least significant byte in a double word */
	ERR_STAT_B1,
	ERR_STAT_B2,
	ERR_STAT_B3,
	ERR_STAT_B4,
	ERR_STAT_B5,
	ERR_STAT_B6,
	ERR_STAT_B7,		/* most significant byte in a double word */
#endif	/* _MIPSEL */
#ifdef	_MIPSEB
	ERR_STAT_B7,		/* least significant byte in a double word */
	ERR_STAT_B6,
	ERR_STAT_B5,
	ERR_STAT_B4,
	ERR_STAT_B3,
	ERR_STAT_B2,
	ERR_STAT_B1,
	ERR_STAT_B0,		/* most significant byte in a double word */
#endif	/* _MIPSEB */
};

/* 
 * _parexpectsig - returns parity error signature expected for an 
 *		access to testaddr with width size bytes
 */
unsigned char
_parexpectsig(void *testaddr, int size)
{
	unsigned int index = (unsigned int)testaddr & 0x7;

	switch (size) {
	case 1:
		return _parexpectsignature[index];
	case 2:
		return _parexpectsignature[index]
			| _parexpectsignature[index + 1];
	case 4:
		return _parexpectsignature[index]
			| _parexpectsignature[index + 1]
			| _parexpectsignature[index + 2]
			| _parexpectsignature[index + 3];
	}
}

bool_t
_parcheckresult(unsigned long gotval, unsigned long expval,
		unsigned char expsignat, unsigned long expaddr)
{
	bool_t passed = TRUE;
	unsigned long expected_cpu_err_stat =
		expsignat | CPU_ERR_STAT_PAR | CPU_ERR_STAT_RD;

	msg_printf(DBG, "testing address: 0x%08x\n", expaddr);

	if (gotval != expval) {
		msg_printf(ERR, "Bad data, Expected: 0x%08x, Actual: 0x%08x\n",
			expval, gotval);
		passed = FALSE;
	}

	if (_cpu_parerr_save != expected_cpu_err_stat) {
		msg_printf(ERR,
			"Bad Status, Expected: 0x%04x, Actual: 0x%04x\n",
			expected_cpu_err_stat, _cpu_parerr_save);
		passed = FALSE;
	}

	/*
	 * the lower 3 bits in the CPU parity address register are not
	 * reliable during a parity error
	 */
	if ((_cpu_paraddr_save ^ KDM_TO_PHYS(expaddr)) & ~0x7) { 
		msg_printf(ERR,
			"Bad Address, Expected: 0x%08x, Actual: 0x%08x\n",
			KDM_TO_PHYS(expaddr), _cpu_paraddr_save);
		passed = FALSE;
	}

	return passed;
}

/* Leave memory and fault-handling in a clean state. */
void
_parcleanup(void *testaddr)
{
	*(long *)((int)testaddr & ~3) = 0;
	parclrerr();
	pardisable();	/* disable parity checking due to a DMUX bug */
	nofault = 0;
}
