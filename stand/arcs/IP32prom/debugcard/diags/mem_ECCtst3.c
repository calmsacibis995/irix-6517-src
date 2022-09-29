/*
 * ===================================================
 * $Date: 1997/08/18 20:40:30 $
 * $Revision: 1.1 $
 * $Author: philw $
 * ===================================================
 *  test name:      mem_ECCtst3.c
 *  descriptions:   multiple soft errors, for read, write, and
 *                  read-modify-write.

.word 0xffffefff, 0x00001000, 0x00000000, 0x0000006c
.word 0xffffeff7, 0x00001008, 0x00000000, 0x000000b7
.word 0xffffefef, 0x00001010, 0x00000000, 0x000000d2
.word 0xffffefe7, 0x00001018, 0x00000000, 0x00000009
.word 0xffffefdf, 0x00001020, 0x00000000, 0x0000000f
.word 0xffffefd7, 0x00001028, 0x00000000, 0x000000d4
.word 0xffffefcf, 0x00001030, 0x00000000, 0x000000b1
.word 0xffffefc7, 0x00001038, 0x00000000, 0x0000006a

Above data pattern was used in CRIME Simulation, following data pattern
are used for Moosehead Debugcard diagnostic -

.double 0xfe20001f01dfffe0, checkbits =  0x27
.double 0xfe20001701dfffe8, checkbits =  0xfc
.double 0xfe20000f01dffff0, checkbits =  0x99
.double 0xfe20000701dffff8, checkbits =  0x42
.double 0xfe1fffff01e00000, checkbits =  0x87
.double 0xfe1ffff701e00008, checkbits =  0x5c
.double 0xfe1fffef01e00010, checkbits =  0x39
.double 0xfe1fffe701e00018, checkbits =  0xe2
.double 0xfe1fffdf01e00020, checkbits =  0xe4
.double 0xfe1fffd701e00028, checkbits =  0x3f
.double 0xfe1fffcf01e00030, checkbits =  0x5a
.double 0xfe1fffc701e00038, checkbits =  0x81
.double 0xfe1fffbf01e00040, checkbits =  0x1e
.double 0xfe1fffb701e00048, checkbits =  0xc5
.double 0xfe1fffaf01e00050, checkbits =  0xa0
.double 0xfe1fffa701e00058, checkbits =  0x7b


 *
 */
 	
#include <sys/regdef.h>
#include <inttypes.h>
#include <crimereg.h>
#include <mooseaddr.h>
#include <cp0regdef.h>
#include <cacheops.h>


#define LINESIZE        0x20
#define TESTADDR        0x1DFFFE0
#if 0
#define	REPLACEMENT 	0x4c
#else
#define	REPLACEMENT 	0x67
#endif

/*
 *	Special interrupt flags.
 */
static void         ecc_interrupt3 (void)   ;
extern int32_t*     global_interrupt (void) ;  
extern void         unexpected_intr  (void) ;  
extern void         enable_interrupt (int)  ;  

int 
mem_ECCtst3 (void)
{
    volatile crime_reg  crimeptr ;
    volatile mem_reg    memptr   ;
    volatile int32_t    *interrupt_flag;
    volatile uint64_t	ecc_replacement, tmp ;
    uint64_t            *daddr   ;
    uint8_t             *baddr   ; 
    uint16_t            *haddr   ; 
    uint32_t            wdata    ;
    int32_t             i ; 


    /*
     * [00] Assign the base address for crime and memory controller, also
     *      goahead to register the interrupt handler routine.
     */
    * (interrupt_flag = (int32_t *) regist_interrupt (ecc_interrupt3))
       = 0xdeadbeef ;
    crimeptr = (crime_reg) CrimeBASE ; 
    memptr   = (mem_reg) MEMcntlBASE ;

    /*
     * [01] Make sure we have a clean interrupt state to start with.
     */
    crimeptr->intrpt_enable = (CRIMEerr_INT|MEMerr_INT) ;
    memptr->mem_error_status = 0x0 ;    /* Clear interrupt status.      */

    /*
     * [02] enable ecc generating and disable ecc checking.
     *      initialize a line in memory.
     *      initialize two double word with bad ecc checkin bit (single bit)
     */
    memptr->ecc_repl = REPLACEMENT ;/* Set the ecc replacement register.*/
    memptr->status = ECCREPL ;	    /* and get ready to use replacement.*/
    BlockInit(TESTADDR) ; 	    /* write to memory (block write). 	*/
    memptr->status = ECCENABLE ;    /* Now enable ecc checking.         */
    enable_interrupt(0);

    /*
     *	[03] a word read, followed by another word read (in the trap handler
     *       routine).
     */
    wdata = *(uint32_t *)((kseg1|TESTADDR)) ; /* Read the first word.	*/
    for (i = 10 ; i > 0x0 ; i--);    /* Wait for a while.               */


    if ((tmp = *interrupt_flag) != 0xcacaacac) {
                                     /* Does the second ecc error       */
      printf ("\n"
              "Error: [03] Interrupt does not happen\n");
      return 0;
    }      
    *interrupt_flag = 0xbeefdead ;  /* reset the interrupt flag for 	*/
                                    /* read/modify/write test.          */

    /*
     *	[04] a word write cause a read/modify/write sequence.
     */
    *(uint32_t *)((kseg1|TESTADDR)) = ~wdata ;/*  Write the data back	*/
    for (i = 0xff ; i > 0x0 ; i--);   /* let's wait for the interrupt.	*/
    if ((tmp = *interrupt_flag) != ((int32_t)0xacaccaca)) {
                                     /* Does the second ecc error       */
      printf ("\n"
              "Error: [04] Interrupt does not happen\n");
      return 0;
    }      
	
    /*
     *	[05] We done.
     */
    return 1;
}


/*
 *  ECC interrupt handler routine.
 */
void
ecc_interrupt3 (void)
{
    volatile crime_reg   crimeptr 	;
    volatile mem_reg     memptr   	;
    uint64_t    tmp2            ;
    uint32_t	tmp, addr       ; 
    int32_t 	*interrupt_flag ;
    int	    	count	    	;


    /*
     *	[00] First check the crime interrupt status register.
     */
    crimeptr = (crime_reg) CrimeBASE ; 
    memptr   = (mem_reg) MEMcntlBASE ;
    interrupt_flag = global_interrupt() ;

    if (crimeptr->intrpt_status != (MEMerr_INT|NORM_INT_STAT)) { /* Is there anything	*/
      printf ("\n"         	    /* else beside mem interrupt ??	*/
              "Error: [int00] Unexpected bits in status register.\n"
              "        Expected data = 0x%0lx\n"
              "        Observed data = 0x%0lx\n",
              MEMerr_INT, crimeptr->intrpt_status);
      unexpected_intr();
      return;
    }
    
    /*
     *	[01] Set interrupt flag accordingly.
     */
    tmp = memptr->mem_error_status ; 
    switch (*interrupt_flag) {
    case 0xdeadbeef:                /* the first read ecc error.        */
      if (tmp != (CPUECC|SOFTERR|ECCRDERR)) {
	printf ("\n"
                "Error: [int01] Unexpected memory error status\n"
                "        Expected data = 0x%0x\n"
                "        Observed data = 0x%0x\n",
                (CPUECC|SOFTERR|ECCRDERR), tmp);
        unexpected_intr();
        break;
      }          
      if (memptr->mem_error_addr != TESTADDR) {
 	printf ("\n"
                "Error: [int01] Unexpected memory error address\n"
                "        Expected data = 0x%0lx\n"
                "        Observed data = 0x%0lx\n",
                TESTADDR, memptr->mem_error_addr);
        unexpected_intr();
        break;
      }       
      tmp = *(uint32_t *)((kseg1|TESTADDR) + 4) ;
      for (count = 0xa ; count > 0 ; count--) ;
      if (memptr->mem_error_status != (CPUECC|SOFTERR|ECCRDERR)) {
	printf ("\n"
                "Error: [int012] Unexpected memory error status\n"
                "        Expected data = 0x%0lx\n"
                "        Observed data = 0x%0lx\n",
                (CPUECC|SOFTERR|ECCRDERR), tmp);
        unexpected_intr();
        break;
      }          
      if (memptr->mem_error_addr != TESTADDR) {
 	printf ("\n"
                "Error: [int012] Unexpected memory error address\n"
                "        Expected data = 0x%0lx\n"
                "        Observed data = 0x%0lx\n",
                TESTADDR, memptr->mem_error_addr);
        unexpected_intr();
        break;
      }       
      *interrupt_flag = 0xcacaacac ; 
      break ;
    case 0xbeefdead:                 /* the first read/modify/write ecc	*/
      if (tmp != (CPUECC|SOFTERR|ECCWRERR)) {
	printf ("\n"
                "Error: [int013] Unexpected read-modify-write ecc error.\n"
                "        Expected data = 0x%0x\n"
                "        Observed data = 0x%0x\n",
                (CPUECC|SOFTERR|ECCWRERR), tmp);
        unexpected_intr();
        break;
      }          
      if (memptr->mem_error_addr != TESTADDR) {
	printf ("\n"
                "Error: [int014] Unexpected memory error address\n"
                "        Expected data = 0x%0lx\n"
                "        Observed data = 0x%0lx\n",
                TESTADDR, memptr->mem_error_addr);
        unexpected_intr();
        break;
      }        
      tmp = *(uint32_t *)((kseg1|TESTADDR) + 8) ;
      for (count = 0xff ; count > 0 ; count--) ;
      if ((tmp = memptr->mem_error_status) != (CPUECC|SOFTERR|ECCWRERR)) {
	printf ("\n"
                "Error: [int015] Unexpected read-modify-write ecc error.\n"
                "        Expected data = 0x%0x\n"
                "        Observed data = 0x%0x\n",
                (CPUECC|SOFTERR|ECCWRERR), tmp);
        unexpected_intr();        
        break;
      }          
      if (memptr->mem_error_addr != TESTADDR) {
	printf ("\n"
                "Error: [int016] Unexpected memory error address\n"
                "        Expected data = 0x%0lx\n"
                "        Observed data = 0x%0lx\n",
                TESTADDR, memptr->mem_error_addr);
        unexpected_intr();        
        break;
      }        
      *interrupt_flag = 0xacaccaca ; 
      break ;
    default:                  
      printf ("\n"
              "Error: [int01] Unexpected interrupt_flag\n"
              "        Observed data = 0x%0lx\n",
              *interrupt_flag);
      unexpected_intr();      
      break ;
    }

    /*
     *  [02] Write to memory error status register to clear the ecc interrupt
     *       condition.
     */
    memptr->mem_error_status = 0x0 ;    /* Clear interrupt status.      */
    tmp2 = memptr->mem_error_status ;   /* Read it back to sync previous*/
                                        /* write, and we're done.       */

}


/*
 * =================================================
 *
 * $Log: mem_ECCtst3.c,v $
 * Revision 1.1  1997/08/18 20:40:30  philw
 * updated file from bonsai/patch2039 tree
 *
 * Revision 1.3  1996/10/31  21:50:34  kuang
 * Bring Bonsai IP32 debugcard up to the level of IP32 debugcard v2.4 on Pulpwood
 *
 * Revision 1.3  1996/10/04  20:32:56  kuang
 * Ported to run with crime ECC
 *
 * Revision 1.2  1996/04/12  02:15:27  kuang
 * Now ECC tests run with R4600/DebugCard environment
 *
 * Revision 1.1  1996/04/04  23:50:01  kuang
 * Ported from petty_crime design verification test suite
 *
 * Revision 1.3  1995/06/30  22:35:46  kuang
 * Removed the work around for rmw ecc error cpu_access bit problem
 *
 * Revision 1.2  1995/06/29  18:21:41  kuang
 * Added BUGfixed flag to get around the read-modify-write ecc error report probl
 * and made the 32 bit constant sign extended to 64 bit constant.
 *
 * Revision 1.2  1995/06/29  18:16:14  kuang
 * Added BUGfixed flag to get around the read-modify-write
 * ecc error report problem, made the 32 bit constant sign
 * extended to 64 bit constant.
 *
 * Revision 1.1  1995/06/29  01:19:40  kuang
 * Initial revision
 *
 * =================================================
 */

/* END OF FILE */
