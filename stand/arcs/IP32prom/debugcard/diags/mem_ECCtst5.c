/*
 * ===================================================
 * $Date: 1997/08/18 20:40:34 $
 * $Revision: 1.1 $
 * $Author: philw $
 * ===================================================
 *  test name:      mem_ECCtst4.c
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

0xfe20005f01dfffa0  ecc = 0xbe
0xfe20005701dfffa8  ecc = 0x65
0xfe20004f01dfffb0  ecc = 0x00
0xfe20004701dfffb8  ecc = 0xdb

0xfe20003f01dfffc0  ecc = 0x44
0xfe20003701dfffc8  ecc = 0x9f
0xfe20002f01dfffd0  ecc = 0xfa
0xfe20002701dfffd8  ecc = 0x21

0xfe20001f01dfffe0  ecc = 0x27
0xfe20001701dfffe8  ecc = 0xfc
0xfe20000f01dffff0  ecc = 0x99
0xfe20000701dffff8  ecc = 0x42

0xfe1fffff01e00000  ecc = 0x87
0xfe1ffff701e00008  ecc = 0x5c
0xfe1fffef01e00010  ecc = 0x39
0xfe1fffe701e00018  ecc = 0xe2

0xfe1fffdf01e00020  ecc = 0xe4
0xfe1fffd701e00028  ecc = 0x3f
0xfe1fffcf01e00030  ecc = 0x5a
0xfe1fffc701e00038  ecc = 0x81

 *
 */
 	
#include <sys/regdef.h>
#include <inttypes.h>
#include <crimereg.h>
#include <mooseaddr.h>
#include <cp0regdef.h>
#include <cacheops.h>

#define TESTADDR1       0x1DFFFA0
#define TESTADDR2       0x1DFFFE0
#if 0
#define	CHECKBIT1   	0x6c
#define CHECKBIT2    	0x0f
#else
static uint64_t CHECKBIT1   = 0xbe;
static uint64_t CHECKBIT2   = 0x27;
#endif

/*
 *	Special interrupt flags.
 */
static void	ecc_interrupt5      (void)             ;
extern int32_t* global_interrupt    (void)     	       ;
extern void     unexpected_intr     (void)     	       ;
extern void     enable_interrupt    (int)      	       ;
extern void	advance_epc         (int32_t, int32_t) ;
extern void	advance_error_epc   (int32_t, int32_t) ;

int 
mem_ECCtst5 (void)
{
    volatile crime_reg  crimeptr ;
    volatile mem_reg    memptr   ;
    volatile uint32_t	wdata    ;
    volatile int32_t 	*interrupt_flag ;
    int32_t             i ;

    /*
     * [00] Assign the base address for crime and memory controller, also
     *      goahead to register the interrupt handler routine.
     */
    * (interrupt_flag = (int32_t *) regist_interrupt (ecc_interrupt5))
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
    memptr->status = ECCREPL     ;  /* and get ready to use replacement.*/
    memptr->ecc_repl = CHECKBIT1 ;  /* Set the ecc replacement register.*/
    BlockErr (TESTADDR1, 0x7, 1) ;  /* write to memory (block write). 	*/
    memptr->ecc_repl = CHECKBIT2 ;  /* checkbits for 2nd line.          */
    BlockErr (TESTADDR2,0xf,1)   ;  /* write to memory (block write). 	*/
    memptr->status = ECCENABLE   ;  /* Now enable ecc checking.         */
    enable_interrupt(0);

    /*
     *	[03] a word read, followed by another word read (in the trap handler
     *       routine).
     */
    wdata = *(uint32_t *)((kseg1|TESTADDR1)) ; /* Read the first word.	*/
    for (i = 0x17f ; i > 0x0 ; i--);    /* Wait for a while.            */
    if ((int32_t)0xcacaacac != *interrupt_flag) {
                                        /* Does the second ecc error    */
      printf ("\n"
              "Error: [03] Interrupt does not happen\n"
              "        Expected data = 0x%0x\n"
              "        Observed data = 0x%0x\n",
              0xcacaacac, *interrupt_flag);
      unexpected_intr();
      return 0;
    }      
    *interrupt_flag = 0xbeefdead ;  /* reset the interrupt flag for 	*/
                                    /* read/modify/write test.          */

    /*
     *	[04] a word write cause a read/modify/write sequence.
     */
    *(uint32_t *)((kseg1|TESTADDR1)) = ~wdata ;/*  Write the data back	*/
    for (i = 0x20 ; i > 0x0 ; i--);   /* let's wait for the interrupt.	*/
    if ((int32_t)0xacaccaca != *interrupt_flag) {
      printf ("\n"
              "Error: [04] Interrupt does not happen\n"
              "        Expected data = 0x%0x\n"
              "        Observed data = 0x%0x\n",
              0xacaccaca, *interrupt_flag);
      unexpected_intr();
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
ecc_interrupt5 (void)
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
      return;
    }

    /*
     *	[01] Set interrupt flag accordingly.
     */
    tmp = memptr->mem_error_status ; 
    switch (*interrupt_flag) {
    case 0xdeadbeef:                /* the first read ecc error.        */
      if (tmp != (CPUECC|HARDERR|ECCRDERR))
	printf ("\n"
                "Error: [int01] Unexpected memory error status\n"
                "        Expected data = 0x%0x\n"
                "        Observed data = 0x%0x\n",
                (CPUECC|HARDERR|ECCRDERR), tmp);
      if (memptr->mem_error_addr != TESTADDR1)
 	printf ("\n"
                "Error: [int01] Unexpected memory error address\n"
                "        Expected data = 0x%0lx\n"
                "        Observed data = 0x%0lx\n",
                TESTADDR1, memptr->mem_error_addr);
      *interrupt_flag = 0xcacaacac ;
      advance_epc (0x0, 0x0) ;
      advance_error_epc (0x0, 0x0) ;
      break ;
    case 0xbeefdead:                 /* the first read/modify/write ecc	*/
      if (tmp != (CPUECC|HARDERR|ECCWRERR)) {
	printf ("\n"
                "Error: [int012] Unexpected read-modify-write ecc error.\n"
                "        Expected data = 0x%0x\n"
                "        Observed data = 0x%0x\n",
                (CPUECC|HARDERR|ECCWRERR), tmp);
        break;
      }          
      if (memptr->mem_error_addr != TESTADDR1) {
	printf ("\n"
                "Error: [int012] Unexpected memory error address\n"
                "        Expected data = 0x%0lx\n"
                "        Observed data = 0x%0lx\n",
                TESTADDR1, memptr->mem_error_addr);
        break;
      }        
      *interrupt_flag = 0xacaccaca ; 
      break ;
    default:                  
      printf ("\n"
              "Error: [int01] Unexpected interrupt_flag\n"
              "        Observed data = 0x%0lx\n",
              *interrupt_flag);
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
 * $Log: mem_ECCtst5.c,v $
 * Revision 1.1  1997/08/18 20:40:34  philw
 * updated file from bonsai/patch2039 tree
 *
 * Revision 1.3  1996/10/31  21:50:37  kuang
 * Bring Bonsai IP32 debugcard up to the level of IP32 debugcard v2.4 on Pulpwood
 *
 * Revision 1.3  1996/10/04  20:33:02  kuang
 * Ported to run with crime ECC
 *
 * Revision 1.2  1996/04/12  02:15:30  kuang
 * Now ECC tests run with R4600/DebugCard environment
 *
 * Revision 1.1  1996/04/04  23:50:03  kuang
 * Ported from petty_crime design verification test suite
 *
 * Revision 1.4  1995/11/21  19:41:23  pries
 * Modified to support tests that take cache errors where they used to take
 * bus errors (due to correction of cpu model).
 *
 * Revision 1.3  1995/07/13  01:12:52  kuang
 * Avoid the incompleted read cause by a bus error exception.
 *
 * Revision 1.2  1995/07/13  01:10:14  kuang
 * *** empty log message ***
 *
 * Revision 1.1  1995/06/30  16:32:19  kuang
 * Initial revision
 *
 * Revision 1.1  1995/06/29  01:19:40  kuang
 * Initial revision
 *
 * =================================================
 */

/* END OF FILE */
