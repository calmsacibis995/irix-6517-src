#ident	"IP22diags/mem/walkingpat.c:  $Revision: 1.2 $"

/*
 * walkingpat.c - IDE walking disturb data path test
 *
 * The data bus is tested by doing a "walking 1" or "walking 0" test over
 * a given range of memory, loading and storing size word. 
 *
 * This is the walkingbit test with these enhancements:
 *	- read before write 
 *	- walk from high mem to low mem
 *	- user-defined data patterns or multiple patterns
*/

#include <fault.h>
#include "setjmp.h"
#include "mem.h"
#include <sys/cpu.h>
#include <sys/sbd.h>
#include <sys/types.h>
#include <sys/mc.h>

#ifdef  PiE_EMULATOR
#define BUSY_COUNT      0x80
#else
#define BUSY_COUNT      0x4000
#endif  /* PiE_EMULATOR */
#define TYPE u_int
#define ROW_SIZE 0x2000

bool_t
memwalkingpat(struct range *ra, int sense, enum runflag till,
	void (* errfun)(), u_int backgnd_pat )
{
	register int	busy_count = 0;
	int		pat, use_table;
	int		pat_count, patterns;
	jmp_buf		faultbuf;
	bool_t		passflag = TRUE;
#ifndef IP32
	register volatile u_int	*cpuctrl0 =				
		(u_int *)PHYS_TO_K1(CPUCTRL0);				
#endif /* !IP32 */
	register volatile TYPE	*ptr;					
	register volatile TYPE	*end;					
	register volatile TYPE	*rd_ptr;					
	register volatile TYPE	*addrhi;					
	register volatile TYPE	*addrlo;					
	register volatile TYPE	backgnd;
	register volatile int   count;

u_int tbl[36] = {
		0xaaaaaaaa, 0x55555555,	0xdbdbdbdb, 0xbdbdbdbd,
		0xdb6db6db, 0x92492492, 0x24924924, 0xfe7fe7fe,
		0xb0fb0fb0, 0xf80f80f8, 0x12012012, 0x03803803,
		0xe13e13e1, 0x7ef7ef7e, 0xfd7fd7fd, 0x7fb7fb7f,
		0xc3c3c3c3, 0x3c3c3c3c, 0xf0f0f0f0, 0x0f0f0f0f,
		0xd2d2d2d2, 0x2d2d2d2d, 0xb1b1b1b1, 0x1b1b1b1b,
		0x11111111, 0x22222222, 0x33333333, 0x44444444,
		0x0, 0x66666666, 0x77777777, 0x88888888,
		0x99999999, 0xffffffff, 0xbbbbbbbb, 0xdddddddd
		};

	msg_printf(VRB, "CPU local memory walking bit test with background write\n");
	if (!sense) msg_printf(VRB, "Usage: memtest2 16 [ # patterns (1-35) or 32 bit data pattern ] [addr range]\n");
		else 
		    msg_printf(VRB, "Usage: memtest2 17 [ # patterns (1-35) or 32 bit data pattern ] [addr range]\n");

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
	if ( backgnd_pat > 36 ) 
		{
	   	backgnd = backgnd_pat;
		pat_count = 1;
		use_table = 0;
		msg_printf(DBG,"Using a single user-defined pattern\n");
	} else {
		pat_count = 0xff & backgnd_pat;
		if (!backgnd_pat) pat_count = 4;
		use_table = 1;
		msg_printf(DBG,"Using pre-defined array of patterns\n");
	}	

patterns=0;

if (!sense) {

   while (patterns < pat_count)
	{
	   if (use_table) backgnd = tbl[patterns];
	   ptr=(u_int *)ra->ra_base;
	   count=ra->ra_count;
	   msg_printf(VRB,"Walking low to high with background 0x%x\n", backgnd);

    	   while (count--)  
		{ 
		*ptr = (u_int)backgnd; 
		ptr++;
		}
	   msg_printf(VRB,"Done with initial fill\n");

	   ptr=(u_int *)ra->ra_base;
	   count=ra->ra_count;

	    	while (--count >= 0) {
			register TYPE 	value = 0x1;

			/* read background pattern before */
			if (*ptr != (u_int)backgnd) { 
				passflag = FALSE;		
				if (errfun) 				
				(*errfun)((u_int)ptr, backgnd,	
					*ptr, sizeof(TYPE));	
				if (till == RUN_UNTIL_ERROR)		
					return passflag;		
				}						

				do {      
					*ptr = value;					
#ifndef IP32
					*cpuctrl0;					
#endif /* !IP32 */
					if (*ptr != value) {				
						passflag = FALSE;		
						if (errfun) 				
						(*errfun)((u_int)ptr, value,	
							*ptr, sizeof(TYPE));	
						if (till == RUN_UNTIL_ERROR)		
							return passflag;		
					        }						
						value <<= 1;				
					} while (value);					

			*ptr=backgnd;  /* write background pattern when done */
			ptr++;							
			if (!busy_count--) {
				busy_count = BUSY_COUNT;
				busy(1);
			        }
		} /* while count */ 

	/* decrement pointer to catch high to low interactions */
	/* memory should already have the background pattern in it */

		/* ptr was at the last address */ 
	   	ptr--;
	   	count=ra->ra_count;

		msg_printf(VRB,"Walking high to low\n");

	    	while (--count >= 0) 
			{
			register TYPE 	value = 0x1;

			if (*ptr != (u_int)backgnd) { 
				passflag = FALSE;		
				if (errfun) 				
				(*errfun)((u_int)ptr, backgnd,	
					*ptr, sizeof(TYPE));	
				if (till == RUN_UNTIL_ERROR)		
					return passflag;		
				}						

				do {     
					*ptr = value;					
#ifndef IP32
					*cpuctrl0;					
#endif /* !IP32 */
					if (*ptr != value) {				
						passflag = FALSE;		
						if (errfun) 				
						(*errfun)((u_int)ptr, value,	
							*ptr, sizeof(TYPE));	
						if (till == RUN_UNTIL_ERROR)		
							return passflag;		
					        }						
						value <<= 1;				
					} while (value);					
			ptr--;							
			if (!busy_count--) {
				busy_count = BUSY_COUNT;
				busy(1);
			        }
			} /* while count */
	patterns++;
	} /* for each pattern */

   } else {  /* if sense is TRUE, then do walking zeros */

   while (patterns < pat_count)
	{
	   if (use_table) backgnd = tbl[patterns];
	   ptr=(u_int *)ra->ra_base;
	   count=ra->ra_count;
	   msg_printf(VRB,"Walking low to high with background 0x%x\n", backgnd);

    	   while (count--)  
		{ 
		*ptr = (u_int)backgnd; 
		ptr++;
		}
	   msg_printf(VRB,"Done with initial fill\n");

	   ptr=(u_int *)ra->ra_base;
	   count=ra->ra_count;

	    	while (--count >= 0) {
			register TYPE 	 walking_1s = 0x1;
			register TYPE    walk_mask = 0xffffffff;

			/* read background pattern before */
			if (*ptr != (u_int)backgnd) { 
				passflag = FALSE;		
				if (errfun) 				
				(*errfun)((u_int)ptr, backgnd,	
					*ptr, sizeof(TYPE));	
				if (till == RUN_UNTIL_ERROR)		
					return passflag;		
				}						

				do {      
					register TYPE value = walking_1s ^ walk_mask;
					*ptr = value;					
#ifndef IP32
					*cpuctrl0;					
#endif /* !IP32 */
					if (*ptr != value) {				
						passflag = FALSE;		
						if (errfun) 				
						(*errfun)((u_int)ptr, value,	
							*ptr, sizeof(TYPE));	
						if (till == RUN_UNTIL_ERROR)		
							return passflag;		
					        }						
						walking_1s <<= 1;				
					} while (walking_1s);					

			*ptr=backgnd;  /* write background pattern when done */
			ptr++;							
			if (!busy_count--) {
				busy_count = BUSY_COUNT;
				busy(1);
			        }
		} /* while count */ 

	/* decrement pointer to catch high to low interactions */
	/* memory should already have the background pattern in it */

		/* ptr was at the last address */ 
	   	ptr--;
	   	count=ra->ra_count;

		msg_printf(VRB,"Walking high to low\n");

	    	while (--count >= 0) 
			{
			register TYPE 	 walking_1s = 0x1;
			register TYPE    walk_mask = 0xffffffff;

			if (*ptr != (u_int)backgnd) { 
				passflag = FALSE;		
				if (errfun) 				
				(*errfun)((u_int)ptr, backgnd,	
					*ptr, sizeof(TYPE));	
				if (till == RUN_UNTIL_ERROR)		
					return passflag;		
				}						

				do {     
					register TYPE  value = walking_1s ^ walk_mask;
					*ptr = value;					
#ifndef IP32
					*cpuctrl0;					
#endif /* !IP32 */
					if (*ptr != value) {				
						passflag = FALSE;		
						if (errfun) 				
						(*errfun)((u_int)ptr, value,	
							*ptr, sizeof(TYPE));	
						if (till == RUN_UNTIL_ERROR)		
							return passflag;		
					        }						
						walking_1s <<= 1;				
					} while (walking_1s);					
			ptr--;							
			if (!busy_count--) {
				busy_count = BUSY_COUNT;
				busy(1);
			        }
			} /* while count */
	patterns++;
	} /* for each pattern */

   }  /* if SENSE true */

busy(0);
nofault = 0;
return passflag;
}

