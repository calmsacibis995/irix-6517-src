#ident	"IP22diags/mem/parcheck.c:  $Revision: 1.1 $"

#include "private.h"
#include "sys/types.h"
#include "sys/cpu.h"
#include "sys/sbd.h"

/*
 * parcheck -- test of parity detection mechanism byte/halfword/word writes
 *
 * testaddr - address in UNCACHED space.  It must point to a memory
 *		location that is safe to destroy.
 *		The neighboring (buddy) longword must be accessible for reading.
 */
bool_t
parcheck(void *testaddr, unsigned int testval, int size)
{
	jmp_buf gotitbuf;
	unsigned char expsignat;
	unsigned int gotval;
	volatile bool_t expectpartrap = FALSE;
	volatile bool_t ok = TRUE;
	unsigned int oldSR = get_SR();

	expsignat = _parexpectsig(testaddr, size);

	parclrerr();
	if (*(volatile unsigned int *)PHYS_TO_K1(CPU_ERR_STAT)) {
		msg_printf(ERR, "Cannot clear CPU error status register: %x\n",
			*(volatile unsigned int *)PHYS_TO_K1(CPU_ERR_STAT));
		return FALSE;
	}

	if (!setjmp(gotitbuf)) {
		/* the code below is executed until a trap occurs */
		nofault = gotitbuf;

		_parwrite(testaddr, ~testval, size); 	/* force known value */
		parwbad(testaddr, testval, size);	/* force bad parity */
		set_SR(SR_BERRBIT | SR_IEC);	/* enable interrupt */
		parenable();			/* enable parity detection */

		/*
		 * read at address in other double word should not get parity
		 * trap
		 */
		_parread((int)testaddr ^ 0x8, size);

		/*
	 	 * read of ANY byte in the longword should get a parity trap.
	 	 * this trap will get caught by setjmp above.
	 	 */
		expectpartrap = TRUE;
		gotval = _parread(testaddr, size); /* trap should occur here */

		/* MUST NOT GET HERE */
		ok = FALSE;
		msg_printf(ERR,
			"Expected a parity trap but did not get one: 0x%04x\n",
			*(volatile unsigned int *)PHYS_TO_K1(CPU_ERR_STAT));
    	}
	else {
		/* the code below is executed when a trap has been detected */
		if (expectpartrap) {
			expectpartrap = FALSE;
			pardisable();
			gotval = _parread(testaddr, size);
			ok &= _parcheckresult(gotval, testval, expsignat,
				(unsigned long)testaddr);
		}
		else {
			msg_printf(ERR, "Unexpected trap in parity test\n");
			show_fault();
			ok = FALSE;
		}

		DoEret();
    	}

	_parcleanup(testaddr);
	set_SR(oldSR);
	if ( !ok ) {
		msg_printf(DBG,"parcheck FAILED( 0x%x,0x%x,0x%x\n",
				testaddr,testval,size);
	}
	return ok;
}


/*
 * pardisenw -- tests control bit for the parity mechanism in cpu register,
 *		using word-wise parity.
 */
bool_t
pardisenw(testaddr)
	unsigned long *testaddr;
{
	jmp_buf gotitbuf;
	volatile bool_t expectpartrap = FALSE;
	volatile bool_t ok = TRUE;
	unsigned int oldSR = get_SR();
	unsigned int cpu_err_stat;

	parclrerr();

	if (!setjmp(gotitbuf)) {
		nofault = gotitbuf;
		set_SR(SR_BERRBIT | SR_IEC);	/* enable interrupt */
		pardisable();		/* disable parity detection */
		parwbad(testaddr, 0, sizeof(int));	/* force bad parity */

		*(volatile unsigned long *)testaddr;	/* should not trap */

		/* nor should the parity error register have been set */
		cpu_err_stat = *(volatile unsigned int *)
			PHYS_TO_K1(CPU_ERR_STAT);
		if (cpu_err_stat & 0x1f00) {
			msg_printf(ERR,
				"Parity register records errors even with parity disabled: %x\n",
				cpu_err_stat);
			ok = FALSE;
		}

		parclrerr();		/* start with clean state */
		parenable();		/* enable parity detection */
		parwbad(testaddr, 0, sizeof(int));	/* force bad parity */

		expectpartrap = TRUE;
		*(volatile unsigned long *)testaddr;	/* trap should occur */

		/* MUST NOT GET HERE */
		msg_printf(ERR, "Expected a parity trap but did not get one\n");
		ok = FALSE;
	} else {
		/* this code executes if a trap has been detected */
		if (expectpartrap)
			expectpartrap = FALSE;
		else {
	    		msg_printf(ERR, "Unexpected trap in parity test\n");
			show_fault();
			ok = FALSE;
		}

		DoEret();
	}

	set_SR(oldSR);	
	_parcleanup(testaddr);
	return ok;
}
