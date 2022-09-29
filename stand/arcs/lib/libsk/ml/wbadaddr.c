/*
 * wbadaddr.c - write bad address checking
 */

#ident	"$Revision: 1.11 $"

#include <sys/types.h>
#include <sys/cpu.h>
#include <sys/param.h>
#include <sys/sbd.h>
#include <fault.h>
#include <setjmp.h>
#include <libsc.h>
#include <libsk.h>

/*
 * wbadaddr -- verify address is write accessible
 */
int
wbadaddr(volatile void *addr, int size)
{
	jmpbufptr oldnofault;
	jmp_buf ba_buf;
	long s;

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
		*(char *)addr = 0;
		break;

	case sizeof(short):
		*(short *)addr = 0;
		break;

	case sizeof(__uint32_t):
		*(__uint32_t *)addr = 0;
		break;

	case sizeof(__uint64_t):
		*(__uint64_t *)addr = 0;
		break;

	default:
		printf("wbadaddr: bad size");
	}
	wbflush();
	DELAY(10);
	_restore_exceptions();
	(void) spl(s);
	nofault = oldnofault;
	return(0);
}
