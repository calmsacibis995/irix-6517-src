#include "sys/types.h"
#include "mem.h"

#define MARCHX_ERR	1
#define MARCHY_ERR	2
/* Described in van de Goor's book, "Testing Semiconductor Memories" and 
 * has the following flow:
 * 
 * (w0), u(r0,w1), d(r1,w0), (r0)
 *
 * Will detect address decoder faults, stuck-at-faults, transition faults,
 * coupling faults, and inversion coupling faults
 */

marchX(u_int first_loc, u_int last_loc)
{
	/*extern int TimerHandler();*/
	register unsigned int *lomem = (u_int *)first_loc;
	register unsigned int *himem = (u_int *)last_loc;
	register unsigned int mask = 0xffffffff;
	register unsigned int data, *ptr;
	register int fail = 0;
	uint bank, leaf, simm, slot;
	uint badbits = 0;
	unsigned long long badaddr;
 	uint *hiptr;
	register unsigned incc;


	incc = 4;
	/*InstallHandler( 0, TimerHandler);
	EnableTimer(4);
*/

	msg_printf(INFO, "\nMarchX Test from 0x%x to 0x%x\n",
		first_loc, last_loc);

	lomem = (u_int *)((u_int)lomem | 0xc);
	himem -= 0x100;
	msg_printf(INFO, "lomem %x, himem %x\n", lomem, himem);
 	/* from lomem to himem, write 0 */
	msg_printf(INFO, "Write 0's\n");
	for (ptr = lomem; ptr < himem; ptr+=incc) {
		*ptr = 0;
		/*wcheck_mem(ptr);*/
	}


	/* from lomem to himem read and verify 0's, then write 1's */
	msg_printf(INFO, "Verify 0's and then write 1's\n");
	for (ptr = lomem ; ptr < himem; ptr+=incc){
		if ((data = *ptr & mask) != 0) {
			fail += wfail_mem(ptr, 0, data, MARCHX_ERR);
		}
		*ptr = -1;
		/*wcheck_mem(ptr);*/
		/*hiptr = ptr;*/
	}
	hiptr = ptr - incc;

	/* from himem to lomem read and verify 1's, then write 0 */
	/* himem--; */
	msg_printf(INFO, "Verify 1's and then write 0\n");
	for (ptr = hiptr ; ptr >= lomem; ptr-=incc) {
		if ((data = *ptr & mask) != mask) {
		    fail += wfail_mem(ptr, mask, data, MARCHX_ERR);
		}
		*ptr = 0x0;
		/*wcheck_mem(ptr);*/
	}
		
	/* from lomem to himem, verify 0 */
	/* himem = memptr->himem; */
	msg_printf(INFO, "Verify 0's\n");
	for (ptr = lomem ; ptr < himem; ptr+=incc) {
		if ((data = *ptr & mask) != 0x0) {
		    fail += wfail_mem(ptr, 0, data, MARCHX_ERR);
		}
	}
/*
	printf("stop timer\n");
	StopTimer();
*/
	return (fail);
} /* marchX */


/* MarchY
 * Described in van de Goor's book, "Testing Semiconductor Memories" and 
 *
 * (w0), u(r0,w1,r1), d(r1,w0,r0), (r0)
 *
 * Will detect address decoder faults, stuck-at-faults, transition faults,
 * coupling faults, and linked transition faults
 */

marchY(u_int first_loc, u_int last_loc)
{
	register unsigned int *lomem = (u_int *)first_loc;
	register unsigned int *himem = (u_int *)last_loc;
	register unsigned int mask = 0xffffffff;
	register unsigned int data, *ptr;
	register int fail = 0;
	uint bank, leaf, simm, slot;
	uint badbits = 0;
	unsigned long long badaddr;
	uint *hiptr;
	register int incc;


	incc = 1;
	msg_printf(INFO,"\nMarchY test from 0x%x to 0x%x\n",
		first_loc, last_loc);

	/* from lomem to himem, write 0 */
	msg_printf(INFO,"Write 0's\n");
	for (ptr = lomem; ptr < himem; ptr+=incc) {
		*ptr = 0;
		/*wcheck_mem(ptr);*/
	}
	/* from lomem to himem read and verify 0's, write 1's, read 1's */
	msg_printf(INFO,"Verify 0's, write 1's, then verify 1's\n");
	for (ptr = lomem ; ptr < himem; ptr+=incc) {
		if ( (*ptr & mask) != 0) {
			data = *ptr & mask;
			fail += wfail_mem(ptr, 0, data, MARCHY_ERR);
		}
		*ptr = -1;
		/*wcheck_mem(ptr);*/
		if ((*ptr & mask) != mask) {
			data = *ptr & mask;
			wfail_mem(ptr, mask, data, MARCHY_ERR);
		}
		/*hiptr = ptr;*/
	}
	hiptr = ptr - incc;

	/* from himem to lomem read and verify 1's, write 0's, read 0's */
	/* himem--; */
	msg_printf(INFO,"Verify 1's and then write 0's\n");
	for (ptr = hiptr ; ptr >= lomem; ptr-=incc) {
		if (( *ptr & mask) != mask) {
			data = *ptr & mask;
			fail += wfail_mem(ptr, mask, data, MARCHY_ERR);
		}
		*ptr = 0;
		/*wcheck_mem(ptr);*/
		if ((*ptr & mask) != 0) {
			data = *ptr & mask;
			fail += wfail_mem(ptr, 0, data, MARCHY_ERR);
		}
	}
		
	/* himem = memptr->himem; */
	msg_printf(INFO,"Verify 0's\n");
	for (ptr = lomem ; ptr < himem; ptr+=incc) {
		if (( *ptr & mask) != 0) {
			data = *ptr & mask;
			fail += wfail_mem(ptr, 0, data, MARCHY_ERR);
		}
	}
	return (fail);
} /* marchY */
wfail_mem(u_int failaddr, u_int exp, u_int act)
{
	printf("Failing address %x\n", failaddr);
	printf("expected = %x, actual = %x, XOR = %x\n",
		exp, act, exp ^ act);
}

