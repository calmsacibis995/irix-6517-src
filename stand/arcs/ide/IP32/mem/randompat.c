#ident	"IP22diags/mem/randompat.c:  $Revision: 1.2 $"

/*
 * randompat.c - IDE random pattern data test 
 *
 * The data bus is tested by doing a "walking 1" or "walking 0" test over
 * a given range of memory, loading and storing size word. 
 *
 * Random pattern test with:
 *	- read before write 
 *	- user-defined data patterns or multiple patterns
 *
 * Checked and tested for IP32
 * contains IP22 specific code, see preproc. directives
 * 6/2/96
 *
*/

#include <fault.h>
#include "setjmp.h"
#include "mem.h"
#include <sys/cpu.h>
#include <sys/sbd.h>
#include <sys/types.h>

#ifdef  PiE_EMULATOR
#define BUSY_COUNT      0x80
#else
#define BUSY_COUNT      0x4000
#endif  /* PiE_EMULATOR */
#define TYPE u_int
#define ROW_SIZE 0x2000

bool_t
randompat(struct range *ra, int sense, enum runflag till,
	void (* errfun)(), u_int backgnd_pat )
{
	register int	busy_count = 0;
	int		use_table, nerrors;
	int		pat_count, patterns;
	jmp_buf		faultbuf;
	bool_t		passflag = TRUE;
#ifndef IP32
	register volatile u_int	*cpuctrl0 =				
		(u_int *)PHYS_TO_K1(CPUCTRL0);				
#endif /* !IP32 */
	register volatile u_int	*ptr;					
	register volatile u_int	*end;					
	register volatile u_int	*addrhi;					
	register volatile u_int	*addrlo;					
	register volatile u_int	backgnd;
	register volatile int   count;

	register u_int  j;
	register u_int  size;
	register u_int  seed;
	register u_int  old_j;
	register u_int  old_seed;
	register u_int  pat;
	register u_int	tmp;

/* 64 data seeds defined, but pointer for seed will go up to 120, into 'random' data */

int random[64] = { 0x0,
	0xaaaaaaaa, 0x55555555,	0xbdbdbdbd, 0xdbdbdbdb,
	0x12345678, 0x87654321, 0xeeeeeeee, 0xdb6db6db,
	0x75757575, 0x57575757, 0x33333333, 0x77777777,
	0x39393939, 0x45454545, 0xf0f0f0f0, 0xff00ff00,
	0x01010101, 0x02020202, 0x04040404, 0x08080808,
	0x10101010, 0x20202020, 0x40404040, 0x80808080,
	0xf1f1f1f1, 0xf2f2f2f2, 0xf4f4f4f4, 0xf8f8f8f8,
	0x1f1f1f1f, 0x2f2f2f2f, 0x4f4f4f4f, 0x8f8f8f8f,
	0xdb6db6db, 0x92492492, 0x24924924, 0xfe7fe7fe,
	0xb0fb0fb0, 0xf80f80f8, 0x12012012, 0x03803803,
	0xe13e13e1, 0x7ef7ef7e, 0xfd7fd7fd, 0x7fb7fb7f,
	0xc3c3c3c3, 0x3c3c3c3c, 0xf0f0f0f0, 0x0f0f0f0f,
	0xd2d2d2d2, 0x2d2d2d2d, 0xb1b1b1b1, 0x1b1b1b1b,
	0x11111111, 0x22222222, 0x33333333, 0x44444444,
	0x66666666, 0x77777777, 0x88888888, 0x99999999, 
	0xffffffff, 0xbbbbbbbb, 0xdddddddd
		};

	msg_printf(VRB, "CPU local memory random data test\n");
	msg_printf(VRB, "Usage: memtest2 18 [ # patterns (1-119) or seed pattern] [addr range]\n");

	if (setjmp(faultbuf)) {
		busy(0);
		show_fault();
		return FALSE;
	}
	nofault = faultbuf;

	addrlo = (u_int *)ra->ra_base;
	addrhi = addrlo + ra->ra_count; 

/* argv[2] can be either a data pattern or the # of patterns */ 

	/* get background pattern from the user prompt */	
	if ( backgnd_pat > 120 ) 
		{
		pat_count = 2;
		use_table = 0;
		msg_printf(DBG,"Using a single user-defined pattern\n");
	} else {
		pat_count = 0xff & backgnd_pat;
		if (!backgnd_pat) pat_count = 120;
		use_table = 1;
		msg_printf(DBG,"Using pre-defined array of patterns\n");
	}	

/* do the initial fill */

if (use_table)  
	pat = random[0];
   else
	pat = backgnd_pat;
	
ptr = addrlo;
end = addrhi-(2*ROW_SIZE);
count = 0;
seed = 0;

while ( ptr <= end )
	{
	  for (size = ROW_SIZE; size > 0; size--) {
		j = pat*(seed+count);
/*		printf("seed: %x, i=%x,   j=%x\n", seed, i, j); 
*/
		*ptr = j;
		ptr++;
		count++;
		}
	  for (size = ROW_SIZE; size > 0; size--) {
		j = pat*(seed+count);
		*ptr = ~j;
		ptr++;
		count++;
		}

	if (!busy_count--) {
		busy_count = (BUSY_COUNT/ROW_SIZE)*8;
		busy(1);
		}

	}

printf("done with initial write\n");

for (seed=1; seed < pat_count; seed++)
{
	count = 0;
	ptr = addrlo;
	end = addrhi-(2*ROW_SIZE);

	if (use_table) {
		pat = random[seed];
		old_j = random[seed-1];
		old_seed = seed - 1;
	    } else {
		pat = ~backgnd_pat;
		old_j = backgnd_pat;
		old_seed = 0; 
	}

	msg_printf(VRB, "seed %d = %x\n", seed, j);

    	while ( ptr < end ) 
	{

	  for (size = ROW_SIZE; size > 0; size--) {
		/* read before write */
		tmp = *ptr;
/* skip compare 
		j =  old_j*(old_seed + count);		
		if (j != tmp) {
			passflag = FALSE;
		   	printf("mem: read before write, addr %x exp %x got %x (curr %x)\n",
				(int) ptr, j, tmp, *ptr);
		  	nerrors++;
		  	}
*/
		j = pat*(seed + count);
		/* do multiple writes to disturb other cells */
		*ptr =  ~j;
		*ptr =  j;
		*ptr =  ~j;
		*ptr =  j;
		*ptr =  ~j;
		*ptr =  j;
		count++;
		ptr++;
	  	}

	  for (size = ROW_SIZE; size > 0; size--) {
		/* read before write */
		tmp = *ptr;
/* skip compare 
		j = old_j *(old_seed + count);		
		if (~j != tmp) {
			passflag = FALSE;
		   	printf("mem: reading inverted data, addr %x exp %x got %x (curr %x)\n",
				(int) ptr, ~j, tmp, *ptr);
		  	nerrors++;
		  	}
*/
		j = pat*(seed + count);

		/* do multiple writes to disturb other cells */
		*ptr =  ~j;
		*ptr =  j;
		*ptr =  ~j;
		*ptr =  j;
		*ptr =  ~j;   /* write inverted data */
		count++;
		ptr++;
		}

	if (!busy_count--) {
		busy_count = (BUSY_COUNT/ROW_SIZE)*8;
		busy(1);
		}

    	}   /* while ptr < end */

}  /* for each seed */

nofault = 0;

return passflag;

}
