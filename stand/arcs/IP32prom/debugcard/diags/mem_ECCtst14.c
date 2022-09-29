/*
 * ===================================================
 * $Date: 1997/08/18 20:40:24 $
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

.word 0xffffefbf, 0x00001040, 0x00000000, 0x000000f5
.word 0xffffefb7, 0x00001048, 0x00000000, 0x0000002e
.word 0xffffefaf, 0x00001050, 0x00000000, 0x0000004b
.word 0xffffefa7, 0x00001058, 0x00000000, 0x00000090
.word 0xffffef9f, 0x00001060, 0x00000000, 0x00000096
.word 0xffffef97, 0x00001068, 0x00000000, 0x0000004d
.word 0xffffef8f, 0x00001070, 0x00000000, 0x00000028
.word 0xffffef87, 0x00001078, 0x00000000, 0x000000f3

 *
 */
 	
#include <sys/regdef.h>
#include <inttypes.h>
#include <crimereg.h>
#include <mooseaddr.h>
#include <cp0regdef.h>
#include <cacheops.h>

#define TESTADDR        0x1DFFFE0
#define	CHECKBIT1   	0x6cb7d209
#define	CHECKBIT2   	0x0fd4b16a
#define	CHECKBIT3   	0xf52e4b90
#define	CHECKBIT4   	0x964d28f3
#define	REPLACEMENT 	0x4cb3d209

/*
 *	Special interrupt flags.
 */
static void	ecc_interrupt14     (void)              ;
extern int32_t*	global_interrupt    (void)              ;  
extern void	enable_interrupt    (int32_t)           ;
extern void	save_sp             (int32_t, int32_t)  ;
extern void	restore_sp          (int32_t, int32_t)  ;
extern void	advance_epc         (int32_t, int32_t)  ;
extern void	advance_error_epc   (int32_t, int32_t)  ;

int 
mem_ECCtst14 (void)
{
    volatile crime_reg   crimeptr ;
    volatile mem_reg     memptr   ;
    uint8_t	*baddr   ; 
    uint16_t	*haddr   ; 
    uint32_t	wdata    ;
    uint64_t	*daddr, ecc_replacement, tmp ;
    int32_t 	*interrupt_flag, *interrupt_count, i;

    /*
     * [00] Assign the base address for crime and memory controller, also
     *      goahead to register the interrupt handler routine.
     */
    * (interrupt_flag = (int32_t *) regist_interrupt (ecc_interrupt14))
       = 0xdeadbeef ;
    * (interrupt_count = interrupt_flag + 0x2) = 0; /* init the intr count   */
    crimeptr = (crime_reg) CrimeBASE ; 
    memptr   = (mem_reg) MEMcntlBASE ;

    /*
     * [01] Make sure we have a clean interrupt state to start with.
     */
    crimeptr->intrpt_enable = (CRIMEerr_INT|MEMerr_INT) ;
    memptr->mem_error_status = 0x0 ;    /* Clear interrupt status.      */
    if ((tmp = crimeptr->intrpt_status) != NORM_INT_STAT) {
      printf ("\n"
              "Error: [01] Unexpected initial mem error status\n"
              "        Expected data = 0x%0lx\n"
              "        Observed data = 0x%0lx\n",
              0x0, tmp) ;
      return 0;
    }              

    /*
     * [02] enable ecc generating and disable ecc checking.
     *      initialize a line in memory.
     *      initialize two double word with bad ecc checkin bit (single bit)
     */
    memptr->ecc_repl = CHECKBIT1 ;  /* Set the ecc replacement register.*/
    memptr->status = ECCREPL     ;  /* and get ready to use replacement.*/
    BlockErr (TESTADDR, 0x7, 1)  ;  /* write to memory (block write). 	*/
    memptr->ecc_repl = CHECKBIT2 ;  /* checkbits for 2nd line.          */
    BlockErr (TESTADDR+0x20,0xa,1);/* write to memory (block write). 	*/
    memptr->ecc_repl = CHECKBIT3 ;  /* checkbits for 2nd line.          */
    BlockErr (TESTADDR+0x40,0x3,1);/* write to memory (block write). 	*/
    memptr->ecc_repl = CHECKBIT4 ;  /* checkbits for 2nd line.          */
    BlockErr (TESTADDR+0x60,0xc,1);/* write to memory (block write). 	*/
    memptr->status = ECCENABLE   ;  /* Now enable ecc checking.         */
    enable_interrupt(0);

    /*
     *	[03] a word read, followed by another word read (in the trap handler
     *       routine).
     */
    wdata = *(uint32_t *)((kseg1|TESTADDR)) ; /* Read the first word.	*/
    for (i = 10 ; i > 0x0 ; i--);    /* Wait for a while.               */
    if ((i = *interrupt_flag) != (int32_t)0xcacaacac) {
                                    /* Does interrupt returned          */
      printf ("\n"
              "Error: [03] Unexpected interrupt flags\n"
              "        Expected data = 0x%0x\n"
              "        Observed data = 0x%0x\n",
              0xcacaacac, i) ;
      return 0;
    }      
    *interrupt_count = (int32_t) -1 ; 
    *interrupt_flag = 0xbeefdead ;  /* reset the interrupt flag for 	*/
                                    /* read/modify/write test.          */

    /*
     *	[04] a word write cause a read/modify/write sequence.
     */
    *(uint32_t *)((kseg1|TESTADDR)) = ~wdata ;/*  Write the data back 	*/
    for (i = 0x00ff0 ; i > 0x0 ; i--); /* let's wait for the interrupt. */
    if ((i = *interrupt_flag) != ((int32_t)0xacaccaca)) {
      printf ("\n"
              "Error: [04] Unexpected interrupt flags\n"
              "        Expected data = 0x%0x\n"
              "        Observed data = 0x%0x\n",
              0xacaccaca, i) ;
      return 0;
    }
    if ((i = *interrupt_count) != ((int32_t)0xbeeffeeb)) {
      printf ("\n"
              "Error: [04] Unexpected interrupt count\n"
              "        Expected data = 0x%0x\n"
              "        Observed data = 0x%0x\n",
              0xbeeffeeb, i) ;
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
ecc_interrupt14 (void)
{
    crime_reg   crimeptr 	;
    mem_reg     memptr   	;
    uint64_t    tmp2            ;
    uint32_t	tmp, data       ; 
    int	    	count	    	;
    int32_t 	*interrupt_flag, *interrupt_count;

    /*
     *	[00] First check the crime interrupt status register.
     */
    crimeptr = (crime_reg) CrimeBASE ; 
    memptr   = (mem_reg) MEMcntlBASE ;
    interrupt_count = (interrupt_flag = (int32_t *) global_interrupt()) + 2 ;

    if ((tmp2 = crimeptr->intrpt_status) != (MEMerr_INT|NORM_INT_STAT))  {
      /*
       * We are not expected anything else beside mem interrupt.
       */
      printf ("\n"
              "Error: [int00] Unexpected interrupt status\n"
              "        Expected data = 0x%0x\n"
              "        Observed data = 0x%0x\n",
              MEMerr_INT, tmp2) ;
      return;
    }

    /*
     *	[01] Set interrupt flag accordingly.
     */
    tmp = memptr->mem_error_status ; 
    switch (*interrupt_flag) {
    case 0xdeadbeef:                /* the first read ecc error.        */
	switch (*interrupt_count) {
	case 0:	/* This is the first interrupt.	*/
          save_sp(0x0, 0x0) ; 	    	    /* save sp for later. */
          if (tmp != (CPUECC|HARDERR|ECCRDERR)) {
            printf ("\n"
                    "Error: [int010] Unexpected interrupt count\n"
                    "        Expected data = 0x%0x\n"
                    "        Observed data = 0x%0x\n",
                    (CPUECC|HARDERR|ECCRDERR), tmp);
            goto intr_exit;
          }
          if ((tmp = memptr->mem_error_addr) != TESTADDR) {
            printf ("\n"
                    "Error: [int011] Unexpected ERROR address.\n"
                    "        Expected data = 0x%0x\n"
                    "        Observed data = 0x%0x\n",
                    TESTADDR, tmp) ;
            goto intr_exit;
          }
	  *interrupt_count = (int32_t) 0x1 ; 
          data = *(uint32_t *)((TESTADDR|kseg1)+0x20) ;
          for (count = 0x7fffffff ; count > 0 ; count --);
          printf ("\n"
                  "Error: [int0111] No second interrupt.\n");
          goto intr_exit;
          
        case 1:
          if ((tmp = memptr->mem_error_status) !=
              (CPUECC|HARDERR|ECCRDERR|MHARDERR)) {
            printf ("\n"
                    "Error: [int012] Unexpected ERROR status.\n"
                    "        Expected data = 0x%0x\n"
                    "        Observed data = 0x%0x\n",
                    (CPUECC|HARDERR|ECCRDERR|MHARDERR), tmp2);
            goto intr_exit;
          }
          if ((tmp = memptr->mem_error_addr) != TESTADDR) {
            printf ("\n"
                    "Error: [int013] Unexpected ERROR address.\n"
                    "        Expected data = 0x%0x\n"
                    "        Observed data = 0x%0x\n",
                    TESTADDR, tmp) ;
            goto intr_exit;
          }
	  *interrupt_count = (int32_t) 0x2 ;             
          data = *(uint32_t *)((TESTADDR|kseg1)+0x40) ; /* second read    */
          for (count = 0x7fffffff ; count > 0 ; count --);
          printf ("\n"
                  "Error: [int0111] No third interrupt.\n");
          goto intr_exit;
          
        case 2:
          if ((tmp = memptr->mem_error_status) !=
              (CPUECC|HARDERR|ECCRDERR|MHARDERR)) {
            printf ("\n"
                    "Error: [int014] Unexpected ERROR status.\n"
                    "        Expected data = 0x%0x\n"
                    "        Observed data = 0x%0x\n",
                    (CPUECC|HARDERR|ECCRDERR|MHARDERR), tmp);
            goto intr_exit;
          }
          if ((tmp = memptr->mem_error_addr) != TESTADDR) {
            printf ("\n"
                    "Error: [int015] Unexpected ERROR address.\n"
                    "        Expected data = 0x%0x\n"
                    "        Observed data = 0x%0x\n",
                    TESTADDR, tmp) ;
            goto intr_exit;
          }
	  *interrupt_count = (int32_t) 0x3 ;         
          data = *(uint32_t *)((TESTADDR|kseg1)+0x60) ;
          for (count = 0x7fffffff ; count > 0 ; count --);
          printf ("\n"
                  "Error: [int0111] No fouth interrupt.\n");
          goto intr_exit;
          
        case 3:
          if ((tmp = memptr->mem_error_status) !=
              (CPUECC|HARDERR|ECCRDERR|MHARDERR)) {
            printf ("\n"
                    "Error: [int016] Unexpected ERROR status.\n"
                    "        Expected data = 0x%0x\n"
                    "        Observed data = 0x%0x\n",
                    (CPUECC|HARDERR|ECCRDERR|MHARDERR), tmp);
            goto intr_exit;
          }
          if ((tmp = memptr->mem_error_addr) != TESTADDR) {
            printf ("\n"
                    "Error: [int017] Unexpected ERROR address.\n"
                    "        Expected data = 0x%0x\n"
                    "        Observed data = 0x%0x\n",
                    TESTADDR, tmp) ;
            goto intr_exit;
          }
	  *interrupt_flag = 0xcacaacac ;
	  *interrupt_count = 0x5a5aa5a5 ;
intr_exit:
	  restore_sp(0x0, 0x0)  ;
	  advance_epc(0x0, 0x0) ;
	  advance_error_epc(0x0, 0x0) ;
          break ;
          
	default:
          printf ("\n"
                  "Error: [int018] Unexpected ERROR address.\n"
                  "        Expected data = 0x%0x\n"
                  "        Observed data = 0x%0x\n",
                  TESTADDR, tmp) ;
          return ;
        }           /* END OF *interrupt_count */
        break ;
        
    case 0xbeefdead:                 /* the first read/modify/write ecc	*/
      if (tmp != (CPUECC|HARDERR|ECCWRERR)) {
        printf ("\n"
                "Error: [int019] Unexpected ERROR status.\n"
                "        Expected data = 0x%0x\n"
                "        Observed data = 0x%0x\n",
                (CPUECC|HARDERR|ECCWRERR), tmp);
        break;
      }
      if ((tmp = memptr->mem_error_addr) != TESTADDR) {
        printf ("\n"
                "Error: [int020] Unexpected ERROR address.\n"
                "        Expected data = 0x%0x\n"
                "        Observed data = 0x%0x\n",
                TESTADDR, tmp) ;
        break;
      }
      *(uint32_t *)((kseg1|TESTADDR)+0x20) = 0xdeadbeef ;
      for (count = 15 ; count > 0 ; count --);/* wait for the change  */
      if ((tmp = memptr->mem_error_status) !=
          (CPUECC|HARDERR|ECCWRERR|MHARDERR)) {
        printf ("\n"
                "Error: [int021] Unexpected ERROR status.\n"
                "        Expected data = 0x%0x\n"
                "        Observed data = 0x%0x\n",
                (CPUECC|HARDERR|ECCWRERR|MHARDERR), tmp);
        break;
      }
      if ((tmp = memptr->mem_error_addr) != TESTADDR) {
        printf ("\n"
                "Error: [int022] Unexpected ERROR address.\n"
                "        Expected data = 0x%0x\n"
                "        Observed data = 0x%0x\n",
                TESTADDR, tmp) ;
        break;
      }
      *(uint32_t *)((kseg1|TESTADDR)+0x40) = 0xbeefdead ;
      for (count = 15 ; count > 0 ; count --);/* wait for the change   */
      if ((tmp = memptr->mem_error_status) !=
          (CPUECC|HARDERR|ECCWRERR|MHARDERR)) {
        printf ("\n"
                "Error: [int023] Unexpected ERROR status.\n"
                "        Expected data = 0x%0x\n"
                "        Observed data = 0x%0x\n",
                (CPUECC|HARDERR|ECCWRERR|MHARDERR), tmp);
        break;
      }
      if ((tmp = memptr->mem_error_addr) != TESTADDR) {
        printf ("\n"
                "Error: [int024] Unexpected ERROR address.\n"
                "        Expected data = 0x%0x\n"
                "        Observed data = 0x%0x\n",
                TESTADDR, tmp) ;
        break;
      }
      *(uint32_t *)((kseg1|TESTADDR)+0x60) = 0x5a5aa5a5 ;
      for (count = 15 ; count > 0 ; count --);/* wait for the change  */
      if ((tmp = memptr->mem_error_status) !=
          (CPUECC|HARDERR|ECCWRERR|MHARDERR)) {
        printf ("\n"
                "Error: [int025] Unexpected ERROR status.\n"
                "        Expected data = 0x%0x\n"
                "        Observed data = 0x%0x\n",
                (CPUECC|HARDERR|ECCWRERR|MHARDERR), tmp);
        break;
      }
      if ((tmp = memptr->mem_error_addr) != TESTADDR) {
        printf ("\n"
                "Error: [int026] Unexpected ERROR address.\n"
                "        Expected data = 0x%0x\n"
                "        Observed data = 0x%0x\n",
                TESTADDR, tmp) ;
        break;
      }
      *interrupt_count = (int32_t) 0xbeeffeeb ; 
      *interrupt_flag = 0xacaccaca ;
      break ;
	
    default:
      printf ("\n"
              "Error: [int027] Unexpected interrupt flag.\n");
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
 * $Log: mem_ECCtst14.c,v $
 * Revision 1.1  1997/08/18 20:40:24  philw
 * updated file from bonsai/patch2039 tree
 *
 * Revision 1.2  1996/10/31  21:50:29  kuang
 * Bring Bonsai IP32 debugcard up to the level of IP32 debugcard v2.4 on Pulpwood
 *
 * Revision 1.2  1996/10/04  20:32:48  kuang
 * Ported to run with crime ECC
 *
 * Revision 1.1  1996/04/04  23:49:58  kuang
 * Ported from petty_crime design verification test suite
 *
 * Revision 1.5  1995/12/07  07:08:13  pries
 * Fixed for new triton model with corrected error priorities.  Since Cache Error
 * masks additional errors, when we encounter a second error in the cache error
 * handler we should just read the memory status register rather than returning
 * to the user program or expecting a 2nd error to be posted to the processor.
 *
 * Revision 1.3  1995/07/11  18:23:06  kuang
 * Moved mem_blktst4 to R4000 VHDL only test list, Added cpu_Ptst.c
 *
 * Revision 1.3  1995/07/11  16:57:36  kuang
 * do not expect an interrupt during the second read-modify-write test in the
 * trap handler routine.
 *
 * Revision 1.2  1995/06/30  17:48:16  kuang
 * *** empty log message ***
 *
 * Revision 1.1  1995/06/30  16:32:41  kuang
 * Initial revision
 *
 * Revision 1.1  1995/06/29  01:19:40  kuang
 * Initial revision
 *
 * =================================================
 */

/* END OF FILE */
