/*  Routines to convert data from 32 bit underlying prom to 64 bit standalone
 * program usage.  Done at run-time to ensure common libsc code. 
 *
 *  These prom callbacks must be seperate so proms can build since they
 * contain back-end of these routines in IP*prom.
 */
#ident "$Revision: 1.3 $"

#if _K64PROM32

#define __USE_SPB32	1

#include <sys/types.h>
#include <sys/immu.h>
#include <arcs/spb.h>
#include <stringlist.h>
#include <libsc.h>

__uint32_t k64p32_callback_0(__uint32_t);

#define KDM32_TO_PHYS(addr)	((__psunsigned_t)(addr) & 0x1fffffff)
#define KDM32_TO_K1(addr)	PHYS_TO_K1(KDM32_TO_PHYS(addr))

static int isprom32_init, is_32;
int _prom;

/* _isK64PROM32 returns whether it's _K64PROM32 at runtime
 * Check to see if the first 2 ARCS prom vectors have the same MSByte
 * (i.e. are pointers into PROM), and also check that the 'next-to-MSByte'
 * is non-zero (64-bit pointers have zeroes in the
 * next-to-most-significant-byte.
 */

#define TVADDR ((char *)(((__psunsigned_t)SPB)+(0x800)))
int
_isK64PROM32(void)
{
	if(isprom32_init == 0) {
		isprom32_init = 1;
		is_32 = (TVADDR[0] == TVADDR[4] && TVADDR[1] != 0);
	}
	return is_32;
}

void
Halt(void)
{
	if (_isK64PROM32())
		EnterInteractiveMode(); /* good enough */
	__TV->Halt();
}

void
PowerDown(void)
{
	if (_isK64PROM32())
		EnterInteractiveMode(); /* good enough */
	__TV->PowerDown();
}

void
Restart(void)
{
	if (_isK64PROM32())
		EnterInteractiveMode(); /* good enough */
	__TV->Restart();
}

void
Reboot(void)
{
	if (_isK64PROM32())
		EnterInteractiveMode(); /* good enough */
	__TV->Reboot();
}

void
EnterInteractiveMode(void)
{
	if (_isK64PROM32())
	    k64p32_callback_0(((FirmwareVector32 *)__TV)->EnterInteractiveMode);
	    /* never returns */
	__TV->EnterInteractiveMode();
}
#endif
