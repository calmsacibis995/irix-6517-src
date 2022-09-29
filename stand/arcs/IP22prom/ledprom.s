#ident	"IP22prom/ledprom.s:  $Revision: 1.7 $"

/* 
   Minimum neccessary required code is used to JUST FLASH
   LED.
*/
#include <asm.h>
#include <regdef.h>
#include <sys/hpc3.h>

#define WAITCOUNT	200000
#define PHYS_TO_K1(x)   ((x)|0xA0000000)        /* physical to kseg1 */

LEAF(start)
        .set    noreorder

         li     t0,PHYS_TO_K1(HPC3_PBUS_CFG_ADDR(6))
         li     t3,0xd46b
	 nop
         sw     t3,0(t0)

         li     t0,PHYS_TO_K1(HPC3_WRITE1); 	/* channel 6 ,offset 0x1c */
repeat:
         li     t3,0
	 nop
         sw     t3,0(t0)

         li     t2,WAITCOUNT    		# delay
2:       bnez   t2,2b
         subu   t2,1

         li     t3,LED_GREEN
	 nop
         sw     t3,0(t0)

         li     t2,WAITCOUNT    		# delay
3:       bnez   t2,3b
         subu   t2,1

         li     t3,LED_AMBER
	 nop
         sw     t3,0(t0)

         li     t2,WAITCOUNT    		# delay
4:       bnez   t2,4b
         subu   t2,1

         li     t3,LED_GREEN|LED_AMBER
	 nop
         sw     t3,0(t0)

         li     t2,WAITCOUNT    		# delay
5:       bnez   t2,5b
         subu   t2,1

         b	repeat
         nop

         j      ra
         nop

        .set    reorder
        END(start)
