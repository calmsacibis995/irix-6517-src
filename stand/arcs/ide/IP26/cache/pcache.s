#include <sys/cpu.h>
#include <sys/sbd.h>
#include <sys/regdef.h>
#include <asm.h>
#include <sys/IP26.h>
#include <sys/IP22.h>


#define GCACHE_SIZE_ADDR        IP26_GCACHE_SIZE_ADDR


/*  End of stuff I added  */
/* get_dcachesize	*/   
#define MIN_CACH_POW2           12


#define Ra_save0        s0      /* level 0 ra save registers */
#define Ra_save1        s1      /* level 1 ra save registers */
#define Ra_save2        s2      /* level 2 ra save registers */

/*#define DC_INIT_ADDRESS 0xa8800000

#define PTAG_E_ADDR     TCC_ETAG_DA
#define PTAG_E_ST       TCC_ETAG_ST
#define STARTUP_CTRL    0x00
#define PTAG_ST_INIT    0x00
#define SET_ALLOW       GCACHE_SETALLOW

*/

#define SCACHE_TESTADDR	0xa800000008840000


/*  For dcache_tag */
/* #define DC_TAG_ADDRESS 0x900000001fcfc000 */
#define DC_TAG_ADDR_START 0x900000001cc00000
#define DC_TAG_ADDR_END   0x900000001cc3ffff
#define VALID_EX_BIT_MASK  0x780000000000800
#define MAX_TAG_PATTERN     4


/*  These are EV DIAG error codes for v0  */
#define DCACHE_TAG 11
#define SCACHE_TAG 12




/*--------------------------------------------------------------------------*/
/* Initialize DCache by invalidating it.  DCache is 16KB, with 32Byte line,
 * 28 bits Tag + 1 bit Exclusive + 8 bits Valid + 32 Bytes Data
 * Need two consecutive writes to half-line boundaries to clear Valid bits
 * for each line.
 */

LEAF(dcache_only_inval)
        .set    noreorder
        move    t3, ra
        li      v1, 16                  # size of the half of Dcache line
        jal     get_dcachesize
        nop

        # v1: half line size, v0: cache size to be initialized

        dli     t1,SCACHE_TESTADDR        # Starting address for initializing
                                        #       Dcache
        daddu   t2, t1, v0              # t2: loop terminator

        # for each cache line:
        #       mark it invalid
1:
        DMTC0(t1,C0_BADVADDR)
        DMTC0(zero,C0_DCACHE)           # clear all (4) Valid bits
        dctw                            # write to the Dcache Tag
        ssnop; ssnop; ssnop             # pipeline hazard prevention
        .set    reorder

        daddu   t1, v1                 # increment to next half-line
        bltu    t1, t2, 1b              # t2 is termination count
        j       t3
END(dcache_only_inval)


LEAF(dump_dcache_tag)
	.set	noreorder
	move	a4, ra
	li	a7, 16
	jal	get_dcachesize
	nop

/*	li	v0,256 
	nop
*/
	dli	a5, SCACHE_TESTADDR
	nop
	daddu	a6, a5, v0	#a6 is loop bound
	
1:
	DMTC0(t1,C0_BADVADDR)
	dctr
	ssnop;ssnop
	DMFC0(t0, C0_DCACHE)
	move	a0,t0
	jal	pon_puthex
	nop
	
	dla	a0,crlf
	nop

	jal	pon_puts
	nop


	daddu   a5, a7             # increment to next half-line
                                   # increment by 16

	bltu	a5,a6, 1b	
	nop

	move 	a0,a5 
	jal	pon_puthex
	nop

	dla	a0, dump_dcache_tag_end
	nop
	jal	pon_puts
	nop
	
	j	a4	
	nop

END(dump_dcache_tag)

	.data
dump_dcache_tag_end:
	.asciiz "\nDcache tag dump end \r\n"	
	.text
/*
 * return the size of the primary data cache
 */
LEAF(get_dcachesize)
        .set    noreorder
        DMFC0(t0,C0_CONFIG)
        .set reorder
        and     t0,CONFIG_DC
        srl     t0,CONFIG_DC_SHFT
        addi    t0,MIN_CACH_POW2
        li      v0,1
        sll     v0,t0
        j       ra
        END(get_dcachesize)

/*-----------------------------------------------------------------------------------*/ 


LEAF(dcache_tag)
        .set    noreorder
        move    Ra_save1, ra
        /*
        li      v1, 16               # size of the half of Dcache line
                                     # each cache tagram entry contains
                                     # 4 64 bit data
        jal     get_dcachesize    # 16k bytes
        nop
        */

        # for each cache line in cache tag ram fill up the tag
        # with various tag patterns and do cache tag write
        # do this for whole 16kB Dcache tag
        .set    noreorder
1:
        dli     t1, DC_TAG_ADDR_START
        dli     t2, DC_TAG_ADDR_END

2:
        DMTC0(t1,C0_BADVADDR)
        dli     s5, 0x7777777777777777
        dli     s4, 0xfffffff000
        and     s5, s4
        dli     s4, VALID_EX_BIT_MASK
        or      s5, s4
        DMTC0(s5, C0_DCACHE)
        dctw                       # Dcache Tag Write
        ssnop; ssnop; ssnop        # pipeline hazard prevention
        daddu   t1, 0x10        # increment to next half-line
                                   # increment by 16
        bltu    t1, t2, 2b         # t2 is termination count
        ssnop

        nop

        # for each cache line in cache tag ram do cache tag
        # read and compare the result, do this for whole
        # 16KB cache tag

        dli     t1,DC_TAG_ADDR_START  # Starting address DC tag ram
        dli     t2, DC_TAG_ADDR_END

3:
        DMTC0(t1,C0_BADVADDR)
        dctr                       # Dcache Tag Read
        ssnop;ssnop
        DMFC0(t3, C0_DCACHE)       # Read from Dcache register
        bne     t3, s5, 99f

        ssnop
        daddu   t1, 0x10        # increment to next half-line
                                   # increment by 16
        bltu    t1, t2, 3b         # t2 is termination count
        ssnop

        nop

        .set    reorder

        # done with no error
        dla     a0, dcache_tag_pass     # dcache tagram pass msg
        jal     pon_puts

        move    v0, zero                        # zero retn -> no error
        j       Ra_save1                        # Jump to saved ra

99:
        .set    noreorder
        dla     a0, dcache_tag_fail     # dcache tagram fail msg
        jal     pon_puts
        nop

        move    a0,t1
        jal     pon_puthex64              # output dcache tag entry
        nop

        dla     a0, crlf
        jal     pon_puts
        nop

        .set    reorder
        li      v0, DCACHE_TAG
        j       Ra_save1                        # Jump to saved ra
END(dcache_tag)

        .data
dcache_tag_pass:
        .asciiz "DCache tag test passed\r\n"
dcache_tag_fail:
        .asciiz "DCache tag test failed at entry "
tag_pattern:
        .word 0xaaaaaaaa, 0x55555555, 0, 0xffffffff
        .text

