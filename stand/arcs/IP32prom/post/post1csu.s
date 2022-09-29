/*
 *---------------------------------------------------------------
 * $Revision: 1.1 $
 * $Date: 1997/08/18 20:48:28 $
 *---------------------------------------------------------------
 */
 	
#include <sys/regdef.h>
#include <post1supt.h>

.extern     post1diags
.set        noreorder               /* the whole thing ought to be no   */
	
/*
 *  Function name:  main
 */
    .globl  main
    .ent    main
main:
    subu    sp, 18*8                /* reserved 18 double words.        */
    sd      AT, 0x00(sp)            /* save return address.             */
    sd      ra, 0x08(sp)            /* save return address.             */
    sd      s0, 0x10(sp)            /* t0 register.                     */
    sd      s1, 0x18(sp)            /* t1 register.                     */
    sd      s2, 0x20(sp)            /* t1 register.                     */
    sd      s3, 0x28(sp)            /* t1 register.                     */
    sd      s4, 0x30(sp)            /* t1 register.                     */
    sd      s5, 0x38(sp)            /* t1 register.                     */
    sd      s6, 0x40(sp)            /* t1 register.                     */
    sd      s7, 0x48(sp)            /* t1 register.                     */
    sd      s7, 0x50(sp)            /* t1 register.                     */
    
    la      s0, post1diags          /* loadup the diags main            */
    jal     s0                      /* branch to it.                    */
    nop                             /* delay slot.                      */

    ld      AT, 0x00(sp)            /* save return address.             */
    ld      ra, 0x08(sp)            /* save return address.             */
    ld      s0, 0x10(sp)            /* t0 register.                     */
    ld      s1, 0x18(sp)            /* t1 register.                     */
    ld      s2, 0x20(sp)            /* t1 register.                     */
    ld      s3, 0x28(sp)            /* t1 register.                     */
    ld      s4, 0x30(sp)            /* t1 register.                     */
    ld      s5, 0x38(sp)            /* t1 register.                     */
    ld      s6, 0x40(sp)            /* t1 register.                     */
    ld      s7, 0x48(sp)            /* t1 register.                     */
    ld      s7, 0x50(sp)            /* t1 register.                     */
    jr      ra                      /* return                           */
    addu    sp, 18*8                /* reserved 18 double words.        */
    
    .end    main
    
/*
 * ---------------------------------------------------
#
# $Log: post1csu.s,v $
# Revision 1.1  1997/08/18 20:48:28  philw
# updated file from bonsai/patch2039 tree
#
# Revision 1.3  1996/10/21  19:17:47  kuang
# Merged in changes from Pulpwood IP32prom to bring bonsai IP32prom to Pulpwood IP32 PROM v2.4
#
# Revision 1.2  1996/05/14  23:21:56  kuang
# Fixed the problem where multiple register get written to the same stack save location.
#
# Revision 1.1  1995/11/28  02:55:13  kuang
# original checkin
#
 * ---------------------------------------------------
 */

/* END OF FILE */
