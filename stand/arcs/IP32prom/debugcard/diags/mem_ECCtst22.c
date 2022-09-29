/*
 * ===================================================
 * $Date: 1997/08/18 20:40:28 $
 * $Revision: 1.1 $
 * $Author: philw $
 * ===================================================
 *  test name:      mem_ECCtst22.c
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

.word 0xffffefbf, 0x00001040, 0x00000000, 0x000000f5
.word 0xffffefb7, 0x00001048, 0x00000000, 0x0000002e
.word 0xffffefaf, 0x00001050, 0x00000000, 0x0000004b
.word 0xffffefa7, 0x00001058, 0x00000000, 0x00000090
.word 0xffffef9f, 0x00001060, 0x00000000, 0x00000096
.word 0xffffef97, 0x00001068, 0x00000000, 0x0000004d
.word 0xffffef8f, 0x00001070, 0x00000000, 0x00000028
.word 0xffffef87, 0x00001078, 0x00000000, 0x000000f3

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

#define TESTADDR        0x1DFFFE0

#if 1
#define	CHECKBIT1   	0x27
#else
#define	CHECKBIT1   	0x6c
#endif

/*
 *	Special interrupt flags.
 */
static void         ecc_interrupt22 	(void)	    	     ;
extern int32_t*     global_interrupt    (void)     	     ;
extern void         unexpected_intr     (void)     	     ;
extern void         enable_interrupt    (void)     	     ;
extern void         advance_epc         (int32_t, uint32_t)  ;
extern void         advance_error_epc   (int32_t, uint32_t)  ;

int 
mem_ECCtst22 (void)
{
    volatile crime_reg  crimeptr ;
    volatile mem_reg    memptr   ;
    volatile uint64_t	ddata, ecc_replacement, tmp, expected_data ; 
    volatile int32_t    *interrupt_flag;
    uint8_t             bdata    ; 
    uint16_t            hdata    ; 
    uint32_t            wdata    ;
    int                 i        ;

    /*
     * [00] Assign the base address for crime and memory controller, also
     *      goahead to register the interrupt handler routine.
     */
    * (interrupt_flag = (int32_t *) regist_interrupt (ecc_interrupt22))
       = 0xdeadbeef ;
    crimeptr = (crime_reg) CrimeBASE ; 
    memptr   = (mem_reg) MEMcntlBASE ;

    /*
     * [01] Make sure we have a clean interrupt state to start with.
     */
    crimeptr->intrpt_enable = (CRIMEerr_INT|MEMerr_INT) ;
    crimeptr->cpu_error_enable |= CPUallIllegal ;
    memptr->mem_error_status = 0x0 ;    /* Clear interrupt status.      */
    
    /*
     * [02] enable ecc generating and disable ecc checking.
     *      initialize a line in memory.
     *      initialize two double word with bad ecc checkin bit (single bit)
     */
    memptr->ecc_repl = (uint64_t)((uint32_t)CHECKBIT1);
    memptr->status = ECCREPL ;	    /* and get ready to use replacement.*/
    BlockErr(TESTADDR,0x6,1);	    /* write to memory (block write). 	*/
    memptr->status = ECCENABLE ;    /* Now enable ecc checking.         */
    enable_interrupt();
    
    /*
     *	[03] a doubleword read,
     */
    ddata = *(uint64_t *)((kseg1|TESTADDR)) ;   /* a doubleword read.   */
    for (i = 10 ; i > 0x0 ; i--);    /* Wait for a while.               */
    if (*interrupt_flag != (int32_t)0xcacaacac) {
      printf ("\n"
              "Error: [03] Interrupt does not happen\n");
      return 0;
    }      
    *interrupt_flag = 0xdeadbeef ;  /* reset the interrupt flag for 	*/
                                    /* next test.                       */

    /*
     *	[04] a word read.
     */
    wdata = *(uint32_t *)((kseg1|TESTADDR)+0x4) ;   /* a word read.     */
    for (i = 0x10 ; i > 0x0 ; i--);   /* let's wait for the interrupt.	*/
    if (*interrupt_flag != ((int32_t)0xcacaacac)) {
      printf ("\n"
              "Error: [04] Interrupt does not happen\n");
      return 0;
    }      
    *interrupt_flag = 0xdeadbeef ;  /* reset the interrupt flag for 	*/
	
    /*
     *	[05] a half word read.
     */
    hdata = *(uint16_t *)((kseg1|TESTADDR)+0x6) ;   /* a word read.     */
    for (i = 0x10 ; i > 0x0 ; i--);   /* let's wait for the interrupt.	*/
    if (*interrupt_flag != ((int32_t)0xcacaacac)) {
      printf ("\n"
              "Error: [05] Interrupt does not happen\n");
      return 0;
    }      
    *interrupt_flag = 0xdeadbeef ;  /* reset the interrupt flag for 	*/
	
    /*
     *	[06] a byte read.
     */
    bdata = *(uint8_t *)((kseg1|TESTADDR)+0x7) ;   /* a word read.      */
    for (i = 0x10 ; i > 0x0 ; i--);   /* let's wait for the interrupt.	*/
    if (*interrupt_flag != ((int32_t)0xcacaacac)) {
      printf ("\n"
              "Error: [06] Interrupt does not happen\n");
      return 0;
    }      

    /*
     *	[07] a byte write test.
     */
    *interrupt_flag = 0xbeefdead ;  /* reset the interrupt flag for 	*/
    *(uint8_t *)((kseg1|TESTADDR)+0x7) = 0xca ; /* a byte write	    	*/
    for (i = 0x10 ; i > 0x0 ; i--);   /* let's wait for the interrupt.	*/
    if (*interrupt_flag != ((int32_t)0xacaccaca)) {
      printf ("\n"
              "Error: [07] Interrupt does not happen\n");
      return 0;
    }      

    memptr->ecc_repl = CHECKBIT1 ;  /* make it so in crime.             */
    memptr->status = ECCREPL ;	    /* and get ready to use replacement.*/
    BlockErr(TESTADDR,0x6,1);	    /* write to memory (block write). 	*/
    memptr->status = ECCENABLE ;    /* Now enable ecc checking.         */
	
    /*
     *	[08] a halfword write test.
     */
    *interrupt_flag = 0xbeefdead ;  /* reset the interrupt flag for 	*/
    *(uint16_t *)((kseg1|TESTADDR)+0x6) = 0xacac;/* a byte write    	*/
    for (i = 0x10 ; i > 0x0 ; i--);   /* let's wait for the interrupt.	*/
    if (*interrupt_flag != ((int32_t)0xacaccaca)) {
      printf ("\n"
              "Error: [081] Interrupt does not happen\n");
      return 0;
    }

    ecc_replacement = CHECKBIT1;             /* Setup single check bit  */
    memptr->ecc_repl = ecc_replacement ;    /* make it so in crime.     */
    memptr->status = ECCREPL ;	    /* and get ready to use replacement.*/
    BlockErr(TESTADDR,0x6,1);	    /* write to memory (block write). 	*/
    memptr->status = ECCENABLE ;    /* Now enable ecc checking.         */
	
    /*
     *	[09] a word write test.
     */
    *interrupt_flag = 0xbeefdead ;  /* reset the interrupt flag for 	*/
    *(uint32_t *)((kseg1|TESTADDR)+0x4) = 0x5a5aa5a5; /* a byte write   */
    for (i = 0x10 ; i > 0x0 ; i--); /* let's wait for the interrupt.	*/
    if (*interrupt_flag != ((int32_t)0xacaccaca)) {
       printf ("\n"
              "Error: [091] Interrupt does not happen\n");
      return 0;
    }     

    /*
     *	[10] We done.
     */
    
    return 1;
}


/*
 *  ECC interrupt handler routine.
 */
void
ecc_interrupt22 (void)
{
    volatile    crime_reg   crimeptr 	;
    volatile    mem_reg     memptr   	;
    volatile    int32_t     *interrupt_flag ;
    uint64_t    tmp2            ;
    uint32_t	tmp, addr       ; 
    int	    	count	    	;


    /*
     *	[00] First check the crime interrupt status register.
     */
    crimeptr = (crime_reg) CrimeBASE ; 
    memptr   = (mem_reg) MEMcntlBASE ;
    interrupt_flag = global_interrupt() ;

    if (crimeptr->intrpt_status != (MEMerr_INT|NORM_INT_STAT)) {/* Is there anything	*/
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
      if (tmp != (CPUECC|HARDERR|ECCRDERR)) {
	printf ("\n"
                "Error: [int01] Unexpected memory error status\n"
                "        Expected data = 0x%0x\n"
                "        Observed data = 0x%0x\n",
                (CPUECC|HARDERR|ECCRDERR), tmp);
        advance_epc (0x0, 0x0) ; 
        advance_error_epc (0x0, 0x0) ;
        break;
      }           
      if (memptr->mem_error_addr != TESTADDR) {
 	printf ("\n"
                "Error: [int01] Unexpected memory error address\n"
                "        Expected data = 0x%0lx\n"
                "        Observed data = 0x%0lx\n",
                TESTADDR, memptr->mem_error_addr);
        advance_epc (0x0, 0x0) ; 
        advance_error_epc (0x0, 0x0) ; 
        break;
      }        
      *interrupt_flag = 0xcacaacac ;
      advance_epc (0x0, 0x0) ;
      advance_error_epc (0x0, 0x0) ;
      break ;
    case 0xbeefdead:                 /* the first read/modify/write ecc	*/
      if (tmp != (CPUECC|HARDERR|ECCWRERR)) {
	printf ("\n"
                "Error: [int01] Unexpected read-modify-write ecc error.\n"
                "        Expected data = 0x%0x\n"
                "        Observed data = 0x%0x\n",
                (CPUECC|HARDERR|ECCWRERR), tmp);
        break;
      }        
      if (memptr->mem_error_addr != TESTADDR) {
	printf ("\n"
                "Error: [int01] Unexpected memory error address\n"
                "        Expected data = 0x%0lx\n"
                "        Observed data = 0x%0lx\n",
                TESTADDR, memptr->mem_error_addr);
        break;
      }        
      *interrupt_flag = 0xacaccaca ; 
      break ;
    default:
      printf ("\n"
              "Error: [int01] Unexpected interrupt_flag\n"
              "        Observed data = 0x%0x\n",
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
    if (tmp2 != 0x0) {
      printf ("\n"
              "Error: [int01] Can not clear memory error status\n"
              "        Observed data = 0x%0lx\n",
              tmp2);
      unexpected_intr();
    }
    return ;
}

/*
 * =================================================
 *
 * $Log: mem_ECCtst22.c,v $
 * Revision 1.1  1997/08/18 20:40:28  philw
 * updated file from bonsai/patch2039 tree
 *
 * Revision 1.2  1996/10/31  21:50:32  kuang
 * Bring Bonsai IP32 debugcard up to the level of IP32 debugcard v2.4 on Pulpwood
 *
 * Revision 1.2  1996/10/04  20:32:53  kuang
 * Ported to run with crime ECC
 *
 * Revision 1.1  1996/04/04  23:50:00  kuang
 * Ported from petty_crime design verification test suite
 *
 * Revision 1.3  1995/11/21  19:41:17  pries
 * Modified to support tests that take cache errors where they used to take
 * bus errors (due to correction of cpu model).
 *
 * Revision 1.2  1995/07/13  01:12:41  kuang
 * Avoid the incompleted read cause by a bus error exception.
 *
 * Revision 1.2  1995/07/12  23:39:01  kuang
 * Avoid the Triton C model incomplete read cause bus error problem.
 *
 * Revision 1.1  1995/07/06  22:39:44  kuang
 * Initial revision
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
