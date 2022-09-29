/*
 * ===================================================
 * $Date: 1997/08/18 20:40:20 $
 * $Revision: 1.1 $
 * $Author: philw $
 * ===================================================
 *  test name:      mem_ECCtst11.c
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
static void	ecc_interrupt12  (void)     ;
extern int32_t*	global_interrupt (void)     ;  
extern void	enable_interrupt (int)      ;
extern void	disable_interrupt(int,int)  ;

int
mem_ECCtst12 (void)
{
    volatile crime_reg  crimeptr ;
    volatile mem_reg    memptr   ;
    volatile uint8_t    bdata    ; 
    volatile uint16_t   hdata    ; 
    volatile uint32_t   wdata    ;
    int                 i, stat  ;
    volatile uint64_t   expected_data, ddata, ecc_replacement, tmp;
    volatile int32_t    *interrupt_flag ;


    /*
     * [00] Assign the base address for crime and memory controller, also
     *      goahead to register the interrupt handler routine.
     */
    * (interrupt_flag = (int32_t *) regist_interrupt (ecc_interrupt12))
       = 0xdeadbeef ;
    crimeptr = (crime_reg) CrimeBASE ; 
    memptr   = (mem_reg) MEMcntlBASE ;

    /*
     * [01] Make sure we have a clean interrupt state to start with.
     */
    crimeptr->intrpt_enable = (CRIMEerr_INT|MEMerr_INT) ;
    memptr->mem_error_status = 0x0 ;
    if ((tmp = crimeptr->intrpt_status) != NORM_INT_STAT) {
      printf("\n"
             "Error: Unexpected interrupt status in crime\n"
             "        Expected data = 0x%0lx\n"
             "        Observed data = 0x%0lx\n",
             0x0, tmp);
      return 0;
    }      

    /*
     * [02] enable ecc generating and disable ecc checking.
     *      initialize a line in memory.
     *      initialize two double word with bad ecc checkin bit (single bit)
     */
    memptr->ecc_repl = CHECKBIT1 ;  /* Setting up good check bits.      */
    memptr->status = ECCREPL ;	    /* and get ready to use replacement.*/
    BlockErr(TESTADDR,0x2,1) ;      /* write to memory (block write).   */
    memptr->status = ECCENABLE ;    /* Now enable ecc checking.         */
    enable_interrupt(0);            /* enable interrupt in status reg.  */
    stat = 0 ;                      /* Set the test stat to pass.       */

    /*
     *	[03] a doubleword read,
     */
    expected_data = ~TESTADDR ;
    expected_data = (expected_data << 32) | TESTADDR ;
    ddata = *(uint64_t *)((kseg1|TESTADDR)) ;   /* a doubleword read.   */
    for (i = 0x400 ; i > 0x0 ; i--);    /* Wait for a while.         */
    if (*interrupt_flag != (int32_t)0xcacaacac) {
                                    /* Does the second ecc error*/
      printf ("\n"
              "Error: [03] Unexpected interrupt flags\n"
              "        Expected data = 0x%0x\n"
              "        Observed data = 0x%0x\n",
              0xcacaacac, i) ;
        stat = -1 ;                 /* test failed.                     */
    }      
    if (ddata != expected_data) {   /* does the error get correct ??    */
      printf ("\n"
              "Error: [03] Unexpected doubleword read\n"
              "              Address = 0x%0x\n"
              "        Expected data = 0x%0lx\n"
              "        Observed data = 0x%0lx\n",
              (kseg1|TESTADDR), expected_data, ddata);
      stat = -1 ;                   /* test failed.                     */
    }
    if (stat == -1) return 0;       /* No point to continue if in error */
    *interrupt_flag = 0xdeadbeef ;  /* reset the interrupt flag for 	*/
                                    /* next test.                       */

    /*
     *	[04] a word read.
     */
    expected_data = (uint32_t)TESTADDR ; 
    wdata = *(uint32_t *)((kseg1|TESTADDR)+0x4) ;   /* a word read.     */
    for (i = 0x20 ; i > 0x0 ; i--);   /* let's wait for the interrupt.	*/
    if (*interrupt_flag != ((int32_t)0xcacaacac)) {
                                    /* here and another in trap handler */
      printf ("\n"
              "Error: [04] Expected interrupt does not happened\n");
      return 0;
    }        
    if (wdata != expected_data) {   /* Is data get correct ??           */
                                    /* go to error routine and stop.    */
      printf ("\n"
              "Error: [04] Unexpected word read\n"
              "              Address = 0x%0x\n"
              "        Expected data = 0x%0x\n"
              "        Observed data = 0x%0x\n",
              kseg1|(TESTADDR+0x4), expected_data, wdata);
      return 0;
    }    
    *interrupt_flag = 0xdeadbeef ;  /* reset the interrupt flag for 	*/

    /*
     *	[05] a half word read.
     */
    hdata = *(uint16_t *)((kseg1|TESTADDR)+0x6) ;   /* a word read.     */
    for (i = 0x20 ; i > 0x0 ; i--);   /* let's wait for the interrupt.	*/
    if (*interrupt_flag != ((int32_t)0xcacaacac)) {
                             	    /* here and another in trap handler	*/
      printf ("\n"
              "Error: [05] Expected interrupt does not happened\n");
      return 0;
    }      
    if ((uint32_t)hdata != (TESTADDR & 0xffff)) {   /* Is data get correct ??           */
      printf ("\n"
              "Error: [05] Unexpected halfword read\n"
              "              Address = 0x%0x\n"
              "        Expected data = 0x%0x\n"
              "        Observed data = 0x%0x\n",
              kseg1|(TESTADDR+0x6), (TESTADDR & 0xffff), hdata);
      return 0;
    }    
    *interrupt_flag = 0xdeadbeef ;  /* reset the interrupt flag for 	*/
	
    /*
     *	[06] a byte read.
     */
    bdata = *(uint8_t *)((kseg1|TESTADDR)+0x7) ;   /* a word read.     	*/
    for (i = 0x20 ; i > 0x0 ; i--);   /* let's wait for the interrupt.	*/
    if (*interrupt_flag != ((int32_t)0xcacaacac)) {
                             	    /* here and another in trap handler	*/
      printf ("\n"
              "Error: [06] Expected interrupt does not happened\n");
      return 0;
    }    
    if ((uint32_t)bdata != (TESTADDR & 0xff)) {
      printf ("\n"
              "Error: [06] Unexpected byte read\n"
              "              Address = 0x%0x\n"
              "        Expected data = 0x%0x\n"
              "        Observed data = 0x%0x\n",
              kseg1|(TESTADDR+0x7), (TESTADDR & 0xff), (uint8_t)bdata);
      return 0;
    }    

    /*
     * [07] enable ecc generating and disable ecc checking.
     *      initialize a line in memory.
     *      initialize two double word with bad ecc checkin bit (single bit)
     */
    memptr->ecc_repl = CHECKBIT1 ;  /* Setting up good check bits.      */
    memptr->status = ECCREPL ;	    /* and get ready to use replacement.*/
    BlockErr(TESTADDR,0x2,1) ;	    /* write to memory (block write). 	*/
    memptr->status = ECCENABLE ;    /* Now enable ecc checking.         */
    expected_data = ~TESTADDR ;     /* correct data after correction.   */
    
    /*
     *	[07] a byte write test.
     */
    *interrupt_flag = 0xbeefdead ;  /* reset the interrupt flag for 	*/
    *(uint8_t *)((kseg1|TESTADDR)+0x7) = 0xca ; /* a byte write	    	*/
    for (i = 0x10 ; i > 0x0 ; i--);   /* let's wait for the interrupt.	*/
    if (*interrupt_flag != ((int32_t)0xacaccaca)) {
                             	    /* here and another in trap handler	*/
      printf ("\n"
              "Error: [07] Expected interrupt does not happened\n");
      return 0;
    }    

    memptr->ecc_repl = CHECKBIT1 ;  /* Setting up good checkbits.       */
    memptr->status = ECCREPL ;	    /* and get ready to use replacement.*/
    BlockErr(TESTADDR,0x2,1) ;	    /* write to memory (block write). 	*/    
    memptr->status = ECCENABLE ;    /* Now enable ecc checking.         */
	
    /*
     *	[08] a halfword write test.
     */
    *interrupt_flag = 0xbeefdead ;  /* reset the interrupt flag for 	*/
    *(uint16_t *)((kseg1|TESTADDR)+0x6) = 0xacac; /* a byte write    	*/
    for (i = 0x10 ; i > 0x0 ; i--);   /* let's wait for the interrupt.	*/
    if (*interrupt_flag != ((int32_t)0xacaccaca)) {
                               	    /* here and another in trap handler	*/
     printf ("\n"
              "Error: [08] Expected interrupt does not happened\n");
      return 0;
    }        

    memptr->ecc_repl = CHECKBIT1 ;  /* Setting up good checkbits.       */
    memptr->status = ECCREPL ;	    /* and get ready to use replacement.*/
    BlockErr(TESTADDR,0x2,1) ;	    /* write to memory (block write). 	*/
    memptr->status = ECCENABLE ;    /* Now enable ecc checking.         */
	
    /*
     *	[09] a word write test.
     */
    *interrupt_flag = 0xbeefdead ;  /* reset the interrupt flag for 	*/
    *(uint32_t *)((kseg1|TESTADDR)+0x4) = 0x5a5aa5a5; /* a word write   */
    for (i = 0x10 ; i > 0x0 ; i--); /* let's wait for the interrupt.	*/
    if (*interrupt_flag != ((int32_t)0xacaccaca)) {
                            	    /* here and another in trap handler	*/
      printf ("\n"
              "Error: [09] Expected interrupt does not happened\n");
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
ecc_interrupt12 (void)
{
    volatile crime_reg  crimeptr 	;
    volatile mem_reg    memptr   	;
    volatile int32_t 	*interrupt_flag ;
    uint64_t            tmp2            ;
    uint32_t            tmp, addr       ; 
    int                 count	    	;


#if defined(Debug)
    printf ("Trap happened: cause register = 0x%0x", rd_cause());
#endif
    
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
      if (tmp != (CPUECC|SOFTERR|ECCRDERR)) {
	printf ("\n"
                "Error: [int01] Unexpected memory error status\n"
                "        Expected data = 0x%0x\n"
                "        Observed data = 0x%0x\n",
                (CPUECC|SOFTERR|ECCRDERR), tmp);
        break;
      }
      if (memptr->mem_error_addr != TESTADDR) {
	printf ("\n"
                "Error: [int02] Unexpected memory error address\n"
                "        Expected data = 0x%0lx\n"
                "        Observed data = 0x%0lx\n",
                TESTADDR, memptr->mem_error_addr);
        break;
      }        
      *interrupt_flag = 0xcacaacac ; 
      break ;
    case 0xbeefdead:                 /* the first read/modify/write ecc	*/
      if (tmp != (CPUECC|SOFTERR|ECCWRERR)) {
	printf ("\n"
                "Error: [int03] Unexpected read-modify-write ecc error.\n"
                "        Expected data = 0x%0lx\n"
                "        Observed data = 0x%0lx\n",
                (CPUECC|SOFTERR|ECCWRERR), tmp);
        break;
      }            
      if (memptr->mem_error_addr != TESTADDR) {
	printf ("\n"
                "Error: [int04] Unexpected memory error address\n"
                "        Expected data = 0x%0lx\n"
                "        Observed data = 0x%0lx\n",
                TESTADDR, memptr->mem_error_addr);
        break;
      }        
      *interrupt_flag = 0xacaccaca ; 
      break ;
    default:
      printf ("\n"
              "Error: [int05] Unexpected interrupt_flag\n"
              "        Observed data = 0x%0lx\n",
              *interrupt_flag);
      break;
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
 * $Log: mem_ECCtst12.c,v $
 * Revision 1.1  1997/08/18 20:40:20  philw
 * updated file from bonsai/patch2039 tree
 *
 * Revision 1.2  1996/10/31  21:50:26  kuang
 * Bring Bonsai IP32 debugcard up to the level of IP32 debugcard v2.4 on Pulpwood
 *
 * Revision 1.2  1996/10/04  20:32:44  kuang
 * Ported to run with crime ECC
 *
 * Revision 1.1  1996/04/04  23:49:55  kuang
 * Ported from petty_crime design verification test suite
 *
 * Revision 1.1  1995/07/07  01:06:58  kuang
 * Initial revision
 *
 * Revision 1.1  1995/07/06  22:40:33  kuang
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
