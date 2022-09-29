/*
 * ===================================================
 * $Date: 1997/08/18 20:40:13 $
 * $Revision: 1.1 $
 * $Author: philw $
 * ===================================================
 *  test name:      mem_ECCtst10.c
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
#define TESTADDR        0x1DFFFE0
#define DATA1		0xffffefff00001000
#define CHECKBITS1  	0x0000006c
#define DATA2		0xffffeff700001008
#define CHECKBITS2  	0x000000b7
#define DATA3		0xffffefef00001010
#define CHECKBITS3  	0x000000d2
#define DATA4		0xffffefe700001018
#define CHECKBITS4  	0x00000009
#define	CHECKBITS   	0x6cb7d209
#define	REPLACEMENT 	0x4cb3d209

/*
 *  For this test we should not expected any interrupt/trap to happened.
 */
int 
mem_ECCtst10 (void)
{
    volatile crime_reg  crimeptr ;
    volatile mem_reg    memptr   ;
    volatile uint64_t   wdata, expect_data ;
    uint8_t	*baddr   ; 
    uint16_t	*haddr   ; 
    int32_t	tmp ; 
    int		i ;

    /*
     * [00] Assign the base address for crime and memory controller.
     */
    crimeptr = (crime_reg) CrimeBASE ; 
    memptr   = (mem_reg) MEMcntlBASE ;

    /*
     * [01] Make sure we have a clean interrupt state to start with.
     */
    crimeptr->intrpt_enable = (CRIMEerr_INT|MEMerr_INT) ;
    memptr->mem_error_status = 0x0 ;
    if ((tmp = crimeptr->intrpt_status) != NORM_INT_STAT) {
      printf ("\n"
              "Error: [01] Unexpected interrupt status.\n"
              "        Expected data = 0x%0x\n"
              "        Observed data = 0x%0x\n",
              0x0, tmp) ;
      return 0;
    }

    /*
     * [02] disable ecc checking.
     *      initialize a line in memory.
     *      initialize two double word with bad ecc checkin bit (single bit)
     */
    memptr->ecc_repl = CHECKBITS ;  /* Set the ecc replacement register.*/
    memptr->status = ECCREPL     ;  /* and get ready to use replacement.*/
    BlockErr (TESTADDR, 0x9, 1)  ;  /* write to memory (block write). 	*/
    memptr->status = 0x0         ;  /* Now disable ecc checking.        */

    /*
     *	[03] a word read, nothing should happened - not interrupt/trap.
     */
    wdata = *(uint64_t *)((kseg1|TESTADDR)) ; /* Read the first word.	*/
    for (i = 10 ; i > 0x0 ; i--);    /* Wait for a while.               */
#if 0    
    expect_data = ((uint64_t)~TESTADDR) <<32 | (TESTADDR ^ 0x9);
#endif
    expect_data = ~((uint64_t)TESTADDR) ;
    expect_data = (expect_data << 32) ;
    expect_data |= (uint64_t)TESTADDR ;
    expect_data ^= (uint64_t)0x9 ;
    
    
    if (wdata != expect_data) {      /* is data get correct ??          */
                                     /* should not correct the data.    */
#if 0
    if (wdata !=
        (expect_data = ((uint64_t)~TESTADDR) <<32 | (TESTADDR ^ 0x9))) {
#endif
      printf ("\n"
              "Error: [03] Unexpected doubleword read.\n"
              "              Address = 0x%0x\n"
              "        Expected junk = 0x%0lx\n"
              "        Expected data = 0x%0lx\n"
              "        Observed data = 0x%0lx\n",
              (uint32_t*)(kseg1|TESTADDR), wdata, expect_data, wdata) ;
      return 0;
    }
        

    /*
     *	[04] a word write cause a read/modify/write sequence.
     */
    *(uint32_t *)((kseg1|TESTADDR)) = 0xdeadbeef ; /* read/modify/write	*/
    for (i = 0xff ; i > 0x0 ; i--);   /* let's wait for the interrupt.	*/
    tmp = *(int32_t *)((kseg1|TESTADDR)) ;
    if (tmp != ((int32_t)0xdeadbeef)) {
      printf ("\n"
              "Error: [04] Unexpected read/modify/write data.\n"
              "        Expected data = 0x%0x\n"
              "        Observed data = 0x%0x\n",
              0xdeadbeef, tmp);
      return 0;
    }     
    
    /*
     *	[05] We done.
     */
    return 1;
}

/*
 * =================================================
 *
 * $Log: mem_ECCtst10.c,v $
 * Revision 1.1  1997/08/18 20:40:13  philw
 * updated file from bonsai/patch2039 tree
 *
 * Revision 1.2  1996/10/31  21:50:21  kuang
 * Bring Bonsai IP32 debugcard up to the level of IP32 debugcard v2.4 on Pulpwood
 *
 * Revision 1.2  1996/10/04  20:32:38  kuang
 * Ported to run with crime ECC
 *
 * Revision 1.1  1996/04/04  23:49:53  kuang
 * Ported from petty_crime design verification test suite
 *
 * Revision 1.1  1995/06/30  16:36:24  kuang
 * Initial revision
 *
 * Revision 1.1  1995/06/30  16:32:35  kuang
 * Initial revision
 *
 * Revision 1.1  1995/06/29  01:19:40  kuang
 * Initial revision
 *
 * =================================================
 */

/* END OF FILE */
