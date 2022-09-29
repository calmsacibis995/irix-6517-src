/*
 * ===================================================
 * $Date: 1997/08/18 20:40:42 $
 * $Revision: 1.1 $
 * $Author: philw $
 * ===================================================
 *  test name:      mem_ECCtst9.c
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

 *
 */
 	
#include <sys/regdef.h>
#include <inttypes.h>
#include <crimereg.h>
#include <mooseaddr.h>
#include <cp0regdef.h>
#include <cacheops.h>

#define LINESIZE        0x20
#define	CHECKBITS   	0x6c

#if 0
#define TESTADDR        0x1DFFFE0
#else
static int32_t  TESTADDR = 0x01DFFFE0 ;
#endif

/*
 *  For this test we should not expected any interrupt/trap to happened.
 */
int 
mem_ECCtst9 (void)
{
    volatile crime_reg  crimeptr ;
    volatile mem_reg    memptr   ;
    volatile uint64_t	wdata, expect_data ;
    volatile int32_t	tmp ;
    int                 i ;

    /*
     * [00] Assign the base address for crime and memory controller.
     */
    crimeptr = (crime_reg) CrimeBASE ; 
    memptr   = (mem_reg) MEMcntlBASE ;

    /*
     * [01] Make sure we have a clean interrupt state to start with.
     */
    crimeptr->intrpt_enable = (CRIMEerr_INT|MEMerr_INT) ;
    memptr->mem_error_status = 0x0 ;    /* Clear interrupt status.      */    

    /*
     * [02] disable ecc checking.
     *      initialize a line in memory.
     *      initialize two double word with bad ecc checkin bit (single bit)
     */
    memptr->ecc_repl = CHECKBITS ;  /* Set the ecc replacement register.*/
    memptr->status = ECCREPL     ;  /* and get ready to use replacement.*/
    BlockErr (TESTADDR, 0x8, 1)  ;  /* write to memory (block write). 	*/
    memptr->status = 0x0         ;  /* Now disable ecc checking.        */

    /*
     *	[03] a doubleword read, nothing should happened - not interrupt/trap.
     */
    wdata = *(uint64_t *)((kseg1|TESTADDR)) ; /* Read the first word.	*/
    for (i = 0x1f ; i > 0x0 ; i--);  /* Wait for a while.               */
    
#if 0
    expect_data = ~TESTADDR ;
    expect_data = (expect_data << 32) | TESTADDR ;
    expect_data ^= 0x8 ;
#else
    expect_data = (uint64_t)(~TESTADDR) << 32 |
      (uint64_t)(TESTADDR ^ 0x8);
#endif    
    
    if (wdata != expect_data) {
      printf ("\n"
              "Error: [03] Unexpected read data\n"
              "              Address = 0x%0x\n"
              "        Expected data = 0x%0lx\n"
              "        Observed data = 0x%0lx\n",
              kseg1|TESTADDR, expect_data, wdata);
      return 0;
    }
    
    /*
     *	[04] a word write cause a read/modify/write sequence.
     */
    *(uint32_t *)((kseg1|TESTADDR)) = 0xdeadbeef ; /* read/modify/write	*/
    for (i = 0xff ; i > 0x0 ; i--);   /* let's wait for the interrupt.	*/
    
    /*
     *	[05] We done.
     */
    return 1;
}

/*
 * =================================================
 *
 * $Log: mem_ECCtst9.c,v $
 * Revision 1.1  1997/08/18 20:40:42  philw
 * updated file from bonsai/patch2039 tree
 *
 * Revision 1.3  1996/10/31  21:50:43  kuang
 * Bring Bonsai IP32 debugcard up to the level of IP32 debugcard v2.4 on Pulpwood
 *
 * Revision 1.3  1996/10/04  20:33:14  kuang
 * Ported to run with crime ECC
 *
 * Revision 1.2  1996/04/12  02:15:34  kuang
 * Now ECC tests run with R4600/DebugCard environment
 *
 * Revision 1.1  1996/04/04  23:50:08  kuang
 * Ported from petty_crime design verification test suite
 *
 * Revision 1.1  1995/06/30  16:36:34  kuang
 * Initial revision
 *
 * Revision 1.1  1995/06/30  16:32:32  kuang
 * Initial revision
 *
 * Revision 1.1  1995/06/29  01:19:40  kuang
 * Initial revision
 *
 * =================================================
 */

/* END OF FILE */
