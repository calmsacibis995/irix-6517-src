/*
 * badaddr.c - read bad address checking
 */

#ident "$Revision: 1.30 $"

#include <sys/types.h>
#include <sys/cpu.h>
#include <sys/param.h>
#include <sys/sbd.h>
#include <fault.h>
#include <setjmp.h>
#include <libsc.h>
#include <libsk.h>

/*
 * badaddr_val -- verify address is read accessible, return value
 */
int
badaddr_val(volatile void *addr, int size, volatile void *value)
{
	jmpbufptr oldnofault;
	volatile long junk;
	jmp_buf ba_buf;
	long s;

	/*
	 * Save current status register and current setting of the
	 * exception vectors and replace them with prom exception
	 * vectors, so that calling badaddr() works even when called
	 * from the kernel.
	 */
#if MCCHIP
	*(volatile int *)PHYS_TO_K1(CPU_ERR_STAT) = 0;
#endif
	oldnofault = nofault;
	s = splerr();
	_save_exceptions();
	_hook_exceptions();

	if (setjmp(ba_buf)) {
		_restore_exceptions();
		(void) spl(s);
		nofault = oldnofault;
		return(1);
	}

	nofault = ba_buf;
	switch (size) {
	case sizeof(char):
		junk = *(volatile char *)addr;
		*(volatile char *)value = (char)junk;
		break;

	case sizeof(short):
		junk = *(volatile short *)addr;
		*(volatile short *)value = (short)junk;
		break;

	case sizeof(__int32_t):
		junk = *(volatile __int32_t *)addr;
		*(volatile __int32_t *)value = (__int32_t)junk;
		break;

	case sizeof(__int64_t):
		junk = *(volatile __int64_t *)addr;
		*(volatile __int64_t *)value = junk;
		break;

	default:
		printf("badaddr: bad size");
	}
	DELAY(10); /*
		    * Give interrupt some time to come in on machines that
		    * give bus errors asyncronously. (IP5 and IP12)
		    */
	_restore_exceptions();
	(void) spl(s);
	nofault = oldnofault;
	return(0);
}

/*
 * badaddr -- verify address is read accessible
 */
int
badaddr(volatile void *addr, int size)
{
#if _MIPS_SIM == _ABI64
	__int64_t junk;
#else
	__int32_t junk;
#endif

	return badaddr_val(addr, size, (volatile void *)&junk);
}
