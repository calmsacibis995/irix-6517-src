/*
 * IP21prom/pod_cache.s
 *
 */

#include "ml.h"
#include <sys/regdef.h>
#include <asm.h>
#include <sys/sbd.h>
#include <sys/EVEREST/evdiag.h>
#include <sys/cpu.h>
#include "pod.h"
#include "pod_failure.h"
#include "prom_leds.h"
#include "ip21prom.h"
/*
 *	Module Name 		Description
 *      ---------------------------------------------------------
 *	dcache_addr		dcache address pattern test
 *	dcache_data		dcache data pattern test
 *	dcache_tag		dcache tag ram test
 *	flush_SAQueue
 *	get_dcacheline  	get dcache line size
 *	get_dcachesize  	get dcache size
 *	get_icacheline  	get icache line size
 *	get_icachesize  	get icache size
 *	pon_fix_dcache_parity   fix dcache parity
 *	pon_invalidate_IDcaches invalidate I&D cache
 *	pon_invalidate_dcache	invalidate dcache
 *  	gcache_invalidate	invalidate gcache
 *  	gcache_check	    	check tag and data rams of gcache
 *	pon_setup_stack_in_dcache set dcache to exclusive for stack
 *	pon_validate_scache_d	validate scache map to dcache
 *	read_scachesize  	get scache size
 */

#define K0_SAVE	    $f14
#define K1_SAVE	    $f15
#define	AT_SAVE	    $f16
#define	RA_SAVE	    $f17
#define	A0_SAVE	    $f18
#define	T3_SAVE	    $f19
#define	TA0_SAVE    $f20
#define	A1_SAVE	    $f21
#define	SP_SAVE	    $f22
#define	V0_SAVE	    $f23
#define	T1_SAVE	    $f24
#define	GP_SAVE	    $f25
#define	T9_SAVE	    $f26
#define	T8_SAVE	    $f27


#define BTAG_ST_WRITE     0x1f000 
#define BTAG_ST_READ      0x0 
#define PTAG_ST_WRITE     0xfc00000000  
#define PTAG_ST_READ      0xf000 
#define GCACHE_BYTES_TO_LINES_SHIFT 11
#define GCACHE_NUM_BYTES_IN_LINE 512

#define DUMP_SET_INFO(base, offset1, line_offset_reg) \
    dli t0, base; \
    li  t1, offset1; \
    daddu	t0, t0, t1; \
    daddu	t0, t0, line_offset_reg; \
    ld  t0, 0(t0); \
    DEBUG_REG(t0)

#define DUMP_LINE_INFO(base, line) \
    li  t2, line; \
    dsll	t2, 3; \
    DUMP_SET_INFO(base, 0, t2); \
    DUMP_SET_INFO(base, 0x10000, t2); \
    DUMP_SET_INFO(base, 0x20000, t2); \
    DUMP_SET_INFO(base, 0x30000, t2); 

#define SC_TAGRAM_ADDR_INIT_FOR_SET(reg1, reg2, reg3, reg4, set_reg, bit_reg, scr_reg) \
    dli	reg1, BB_BUSTAG_ADDR; \
    dli	reg2, BB_PTAG_E_ADDR; \
    dli	reg3, BB_BUSTAG_ST; \
    dli	reg4, BB_PTAG_E_ST; \
    li	scr_reg, 0x10000; \
    dmult   scr_reg, set_reg; \
    mflo    scr_reg; \
    daddu   reg1, scr_reg; \
    daddu   reg2, scr_reg; \
    daddu   reg3, scr_reg; \
    daddu   reg4, scr_reg; \
    dsll    scr_reg, bit_reg, 14; \
    or	    reg1, reg1, scr_reg; \
    or	    reg2, reg2, scr_reg; \
    or	    reg3, reg3, scr_reg; \
    or	    reg4, reg4, scr_reg

#define SC_TAGRAM_ADDR_NEXT_LINE(bustag_reg, proctag_reg, busstate_reg, procstate_reg, scr_reg) \
    	li  	scr_reg, 8; \
    	daddu	bustag_reg, bustag_reg, scr_reg; \
    	daddu	proctag_reg, proctag_reg, scr_reg; \
    	daddu	busstate_reg, busstate_reg, scr_reg; \
    	daddu	procstate_reg, procstate_reg, scr_reg

/* read_scachesize(), which this macro calls, uses t0, t1, v0, ra */
#if 0
#define SC_GET_NUM_LINES_IN_GCACHE(result_reg) \
	/* read Gcache size.  Default GCache size is 4MB */ ; \
    	/* 4M  gcache = 4 sets of 2048 (2K) lines of 512 bytes each */ ; \
    	/* 8M  gcache = 4 sets of 4096 (4k) lines of 512 bytes each */ ; \
    	/* 16M gcache = 4 sets of 8192 (8k) lines of 512 bytes each */ ; \
    	DMTC1(k0, K0_SAVE); \
    	DMTC1(k1, K1_SAVE); \
	jal	read_scachesize; \
	nop; \
    	DMFC1(k0, K0_SAVE); \
    	DMFC1(k1, K1_SAVE); \
	dsrl	result_reg, v0, GCACHE_BYTES_TO_LINES_SHIFT
#endif

#define SC_GET_NUM_LINES_IN_GCACHE(result_reg) \
    	li  result_reg, 2048

#define SC_TAG_BITS(result_reg, addr_reg, scr_reg) \
    	dli scr_reg, 0xfffff00000; \
    	and result_reg, addr_reg, scr_reg

#define SC_TAG_BITS_WITH_ADDR_BITS(result_reg, addr_reg, loc_reg, scr_reg) \
    	dli scr_reg, 0xfffff00000; \
    	and result_reg, addr_reg, scr_reg; \
    	li  scr_reg, 0xffffffffffcfffff; \
    	and result_reg, result_reg, scr_reg; \
    	li  scr_reg, 0xc000; \
    	and scr_reg, loc_reg, scr_reg; \
    	dsll	scr_reg, 6; \
    	or  result_reg, result_reg, scr_reg

#define SC_STATE_BITS(result_reg, addr_reg, scr_reg) \
    	dli scr_reg, 0xfff; \
    	and result_reg, addr_reg, scr_reg

#define GCACHE_CHECK_TAGS(tag_init, tag_inc, state_init, state_inc, num_sets, start_set, bits_15_14) \
    dli	a0, tag_init; \
    dli	a1, tag_inc; \
    dli	a2, state_init; \
    dli	a3, state_inc; \
    li	t2, num_sets; \
    li	t3, start_set; \
    li	v0, bits_15_14; \
    li	v1, 1; \
    jal	gcache_check_tags; \
    nop

#define GCACHE_SET_TAGS(tag_init, tag_inc, state_init, state_inc, num_sets, start_set, bits_15_14) \
    dli	a0, tag_init; \
    dli	a1, tag_inc; \
    dli	a2, state_init; \
    dli	a3, state_inc; \
    li	t2, num_sets; \
    li	t3, start_set; \
    li	v0, bits_15_14; \
    li	v1, 0; \
    jal	gcache_check_tags; \
    nop



/* ******************************************************************************
 *  gcache_check
 *
 *  This is the highest level routine for checking the gcache.  It checks both 
 *  the G$ tag rams and data rams.  It is called by both the master and slaves.
 * ******************************************************************************/

#if 0
LEAF (gcache_check)
    dli	t0, BB_BUSTAG_ADDR
    li	t1, 0x170000
    dadd    t0, t1
    DMTC1(t0, GP_SAVE)
    li	v0, 1
    j	gcache_print_tag_error_and_return
END(gcache_check)
#endif

LEAF(gcache_check)
    	.set noreorder
    	DMTC1(ra, AT_SAVE)
    	li  	a0, 0
    	jal 	gcache_check_tag_addr
    	nop
    	li  	a0, 0
    	jal 	gcache_check_tag_data
    	nop
    	li  	a0, 0
    	jal 	gcache_check_data
    	nop

    	/*
    	 * Invalidate sets 0, 1, and 2, and then turn off FORCE SET 3 and turn
    	 * on bit 0 of the set allow register, putting us into a mode that is
    	 * "FORCE SET 0".  Call gcache_invalidate again in FORCE SET 0 mode so
    	 * that we invalidate the entries in set 3 that did not get invalidated
    	 * when we did the first series of invalidates because we were running
    	 * in set 3.  Both times when I call gcache_invalidate I call it twice
    	 * in a row.  The idea is that the first time we run it we are not running
    	 * fully in the gcache, so there are some entries that do not get invalidated
    	 * (because of our code section).  The second time we are running fully from
    	 * the gcache.  However, I think this is overkill and I don't believe
    	 * we really need the second one in each series.  The first time we 
    	 * invalidate, even if we leave some junk in set 3, that is ok because when
    	 * we go into FORCE SET 0, we will invalidate again and this time the junk
    	 * in set 3 will get cleaned up.  But I'm leaving the double calls just
    	 * in case I have missed something.
    	 */
        jal     gcache_invalidate
    	nop
        jal     gcache_invalidate
    	nop
    	dli 	t1, BB_STARTUP_CTRL
    	li  	a0, 1
    	sd  	a0, 0(t1)

        dli     t1, BB_SET_ALLOW
        li      a0, 0x1
        sd      a0, 0(t1)   # set allow set 0
        jal     gcache_invalidate
    	nop
        jal     gcache_invalidate
    	nop

    	GCACHE_SET_TAGS(0x41fc00000, 0x0, BTAG_ST_INIT, 0, 0x1, 0x0, 0x0)
    	GCACHE_SET_TAGS(0x418000000, 0x0, BTAG_ST_INIT, 0, 0x1, 0x1, 0x0)
    	GCACHE_SET_TAGS(0x418100000, 0x0, BTAG_ST_INIT, 0, 0x1, 0x2, 0x1)

    	li  	a0, 1
    	jal 	gcache_check_tag_addr
    	nop
    	li  	a0, 1
    	jal 	gcache_check_tag_data
    	nop
    	li  	a0, 1
    	jal 	gcache_check_data
    	nop
	jal	gcache_invalidate
	nop
	jal	gcache_invalidate
	nop

    	/* Put us back into FORCE SET 3, re-invalidate gcache */
    	dli 	t1, BB_STARTUP_CTRL
    	li  	a0, 3
    	sd  	a0, 0(t1)

        dli     t1, BB_SET_ALLOW
        li      a0, 0x8
        sd      a0, 0(t1)   # set allow set 0
        jal     gcache_invalidate
    	nop


    	DMFC1(ra, AT_SAVE)
        li      v0, 0
    	j   	ra
    	nop
END(gcache_check)



/* ******************************************************************************
 *  gcache_invalidate
 *
 *  This routine initializes the gcache tag and state rams, such that all
 *  entries are invalid.  The tags are set like this:
 *     set 0        set 1         set 2        set 3
 *     0x1cc        0x1dc         0x1ec        0x1fc
 *
 * Since the execution of the code and the writing of the tags will knock 
 * out of the gcache, there will be a couple of entries that are not intitialized
 * as shown above.  But this still achieves the desired effect of getting the
 * gcache into a known state.
 * ******************************************************************************/
LEAF(gcache_invalidate)
    	.set noreorder
    	DMTC1(ra, RA_SAVE)
    	GCACHE_SET_TAGS(0x802a000000, 0x0, BTAG_ST_INIT, 0x0, 0x1, 0x0, 0x0)
    	GCACHE_SET_TAGS(0x802b000000, 0x0, BTAG_ST_INIT, 0x0, 0x1, 0x1, 0x0)
    	GCACHE_SET_TAGS(0x802c000000, 0x0, BTAG_ST_INIT, 0x0, 0x1, 0x2, 0x0)
    	GCACHE_SET_TAGS(0x802d000000, 0x0, BTAG_ST_INIT, 0x0, 0x1, 0x3, 0x0)
    	DMFC1(ra, RA_SAVE)
    	j ra
    	nop
END(gcache_invalidate)



/* ******************************************************************************
 *  gcache_check_tag_addr
 *  Test the gcache tag addressing to make sure we can address each tag correctly
 *  from CC local register space.
 * ******************************************************************************/
LEAF(gcache_check_tag_addr)
    	.set noreorder
    	DMTC1(ra, RA_SAVE)
    	beq a0, r0, gcache_check_tag_addr_set_0_1_2
    	nop
gache_check_tag_addr_set_3:
    	GCACHE_CHECK_TAGS(0x0, 0xe000400000, 0x0, 0x1, 0x1, 0x3, 0x0)
    	beq 	v0, r0, gcache_check_tag_addr_pass_set_3
    	nop 
    	j   	gcache_check_tag_addr_fail_set_3
    	nop
    	

gcache_check_tag_addr_set_0_1_2:
    	GCACHE_CHECK_TAGS(0x0, 0xe000400000, 0x0, 0x1, 0x3, 0x0, 0x0)

    	beq 	v0, r0, gcache_check_tag_addr_pass_set_0_1_2
    	nop
    	j   	gcache_check_tag_addr_fail_set_0_1_2

gcache_check_tag_addr_fail_set_3:
    	ERROR_MSG("\r\nGcache TAG ADDR test for set 3 FAILED\r\n")
    	j   gcache_check_tag_addr_fail_info
    	nop
gcache_check_tag_addr_fail_set_0_1_2:
    	ERROR_MSG("\r\nGcache TAG ADDR test for set 0, 1, or 2 FAILED\r\n")
gcache_check_tag_addr_fail_info:
    	ERROR_MSG_FPREG("    line index:    ", T1_SAVE)
    	ERROR_MSG_FPREG("    set index:     ", T3_SAVE)
    	ERROR_MSG_FPREG("    address:       ", GP_SAVE)
    	ERROR_MSG_FPREG("    expected data: ", T9_SAVE)
    	ERROR_MSG_FPREG("    actual data:   ", T8_SAVE)
    	li  v0, EVDIAG_SCACHE_TAG
    	DMFC1(ra, RA_SAVE)
    	j   gcache_print_tag_error_and_return
    	nop

gcache_check_tag_addr_pass_set_3:
    	PASS_MSG("\Gcache TAG ADDR test for set 3 PASSED\r\n")
    	j   gcache_check_tag_addr_pass_return
    	nop
gcache_check_tag_addr_pass_set_0_1_2:
    	PASS_MSG("\Gcache TAG ADDR test for sets 0, 1, 2 PASSED\r\n")
gcache_check_tag_addr_pass_return:
	DMFC1(ra, RA_SAVE)
    	j ra
    	nop
END(gcache_check_tag_addr)


/* ******************************************************************************
 *  gcache_check_tag_data
 *  Test the gcache tag data rams to make sure that no bits are stuck at 0,
 *  stuck at 1, or stuck together.
 * ******************************************************************************/
LEAF(gcache_check_tag_data)
    	.set noreorder
	DMTC1(ra, RA_SAVE)
    	beq a0, r0, gcache_check_tag_data_set_0_1_2
    	nop
gcache_check_tag_data_set_3:
    	GCACHE_CHECK_TAGS(0xaaaaaaaaaaaaaaaa, 0x0, 0xaaaaaaaaaaaaaaaa, 0x0, 0x1, 0x3, 0x0)
    	bne 	v0, r0, gcache_check_tag_data_fail_set_3
    	nop
    	GCACHE_CHECK_TAGS(0x5555555555555555, 0x0, 0x5555555555555555, 0x0, 0x1, 0x3, 0x0)
    	beq 	v0, r0, gcache_check_tag_data_pass_set_3
    	nop
gcache_check_tag_data_set_0_1_2:
    	GCACHE_CHECK_TAGS(0xaaaaaaaaaaaaaaaa, 0x0, 0xaaaaaaaaaaaaaaaa, 0x0, 0x3, 0x0, 0x0)
    	bne 	v0, r0, gcache_check_tag_data_fail_set_0_1_2
    	nop
    	GCACHE_CHECK_TAGS(0x5555555555555555, 0x0, 0x5555555555555555, 0x0, 0x3, 0x0, 0x0)
    	beq 	v0, r0, gcache_check_tag_data_pass_set_0_1_2
    	nop
gcache_check_tag_data_fail_set_3:
    	ERROR_MSG("\r\nGcache TAG DATA test for set 3 FAILED\r\n")
gcache_check_tag_data_fail_set_0_1_2:
    	ERROR_MSG("\r\nGcache TAG DATA test for set 0, 1, or 2 FAILED\r\n")
gcache_check_tag_data_fail_info:
    	ERROR_MSG_FPREG("    line index:    ", T1_SAVE)
    	ERROR_MSG_FPREG("    set index:     ", T3_SAVE)
    	ERROR_MSG_FPREG("    address:       ", GP_SAVE)
    	ERROR_MSG_FPREG("    expected data: ", T9_SAVE)
    	ERROR_MSG_FPREG("    actual data:   ", T8_SAVE)
    	li  v0, EVDIAG_SCACHE_TAG
    	DMFC1(ra, RA_SAVE)
    	j   gcache_print_tag_error_and_return
    	nop

gcache_check_tag_data_pass_set_3:
    	PASS_MSG("\Gcache TAG DATA test for set 3 PASSED\r\n")
    	j   gcache_check_tag_data_pass_return
    	nop

gcache_check_tag_data_pass_set_0_1_2:
    	PASS_MSG("\Gcache TAG DATA test for sets 0, 1, 2 PASSED\r\n")
gcache_check_tag_data_pass_return:
	DMFC1(ra, RA_SAVE)
    	j ra
    	nop
END(gcache_check_tag_data)


/* ******************************************************************************
 *  gcache_check_data
 *  Test the gcache data rams to make sure that no bits are stuck at 0,
 *  stuck at 1, or stuck together.
 * ******************************************************************************/
LEAF(gcache_check_data)
    	.set noreorder
	DMTC1(ra, RA_SAVE)
    	DMTC1(a0, A0_SAVE)  /* be careful that gcache_check_data() doesn't use A0_SAVE! */
    	/* *****************************************************************************
    	 * Set up the gcache so that all lines in set 0 have a tag of 0xf0000, all lines
    	 * in set 1 have a tag of 0xf0004, all lines in set 2 have a tag of 0xf0008.
    	 * After calling GCACHE_SET_TAGS() to set up the tags and state, call 
    	 * gcache_set_vsyn to fix up the virtual synonm bits in the state.
    	 * *****************************************************************************/

    	beq a0, r0, gcache_check_data_set_0_1_2
    	nop
gcache_check_data_set_3:
    	GCACHE_SET_TAGS(0xf000c00000, 0x0, BTAG_ST_EXCL, 0x0, 0x1, 0x3, 0x0)
    	li  t2, 1   	    # do one set
    	li  t3, 3   	    # start with set 3
    	jal gcache_set_vsyn
    	nop
    	li  t2, 1   	    # do one set
    	li  t3, 3   	    # start with set 3
    	j   	gcache_check_data_common_code
    	nop


gcache_check_data_set_0_1_2:
    	GCACHE_SET_TAGS(0xf000000000, 0x0, BTAG_ST_EXCL, 0x0, 0x1, 0x0, 0x0)
    	GCACHE_SET_TAGS(0xf000400000, 0x0, BTAG_ST_EXCL, 0x0, 0x1, 0x1, 0x0)
    	GCACHE_SET_TAGS(0xf000800000, 0x0, BTAG_ST_EXCL, 0x0, 0x1, 0x2, 0x0)

    	li  	t2, 3	    # do 3 sets
	li	t3, 0	    # start with st 0
    	jal gcache_set_vsyn
    	nop
    	li  	t2, 3	    # do 3 sets
	li	t3, 0	    # start with st 0

gcache_check_data_common_code:

    	SC_GET_NUM_LINES_IN_GCACHE(t0)	# t0 = num of lines(2048)
    	li  	t8, GCACHE_NUM_BYTES_IN_LINE #(512)
    	dmult	t8, t0
    	mflo	t0  	    	 # t0 = number of bytes to cycle through
    	    	        	 # last one we want to store to

    	/* *****************************************************************************
    	 * This code contains a loop within a loop.  The outer loop stores a data value to
    	 * each double word ina single gcache set, starting at double word 0x0, going
    	 * up to address 2048 lines * 512 bytes/line = 0x100000.
    	 * The outer loop, the inner loop once for each of sets 0, 1, 2.
    	 *
    	 *       set0               set1                     set2
    	 *   data    addr    data        addr        data        addr   
    	 *   0x0  -> 0x0     0x400000 -> 0x400000    0x800000 -> 0x800000
    	 *   0x8  -> 0x8     0x400008 -> 0x400008    0x800008 -> 0x800008
    	 *   0x10 -> 0x10    0x400010 -> 0x400010    0x800010 -> 0x800010
    	 *       ...                 ...                   ...
    	 *******************************************************************************/
1:
    	dsll	t9, t3, 22  	    	# t9 = tag for this set
    	dli 	t8, 0xa80000f000000000
    	or  	t9, t9, t8
    	li  	a0, 0	    	    	
2:	/* store into set(s) */
    	or  	gp, t9, a0
    	sd  	gp, 0(gp)
    	daddiu	a0, 8 
	nop
   	blt 	a0, t0, 2b
        nop 
        daddiu  t3, 1
    	blt 	t3, t2, 1b  	    	# do outer loop for sets 0, 1, 2
        nop


    	/* *****************************************************************************
    	 * The code below contains a loop within a loop.  The inner loop runs through each
    	 * double word in a single set of the gcache, reading from it and comparing it
    	 * to the value that we stored in the above loop.
    	 * The outer loop executes the inner loop once for each set.
    	 *******************************************************************************/
	DMFC1(a0, A0_SAVE)
        beq a0, r0, gcache_read_data_set_0_1_2
        nop
	j	gcache_read_data_set_3
	nop
	
gcache_read_data_set_0_1_2:
        li      t2, 3       # do 3 sets
        li      t3, 0       # start with st 0
        j       gcache_read_data_common_code
        nop
gcache_read_data_set_3:
        li  t2, 1           # do one set
        li  t3, 3           # start with set 3

gcache_read_data_common_code:

4:
        dsll    t9, t3, 22              # t9 = tag for this set
        dli     t8, 0xa80000f000000000
        or      t9, t9, t8
        li      a0, 0

5:      
        or      gp, t9, a0
        daddiu  a0, 8
        ld      a1, 0(gp)
        bne     gp, a1, 99f
        nop
        blt     a0, t0, 5b
        nop

        daddiu  t3, 1
        blt     t3, t2, 4b              # do outer loop for sets 0, 1, 2
        nop

        DMFC1(a0, A0_SAVE)
    	beq a0, r0, gcache_check_data_passed_set_0_1_2
    	nop
gcache_check_data_passed_set_3:
    	PASS_MSG("\Gcache RAM DATA test for set 3 PASSED\r\n")
    	j   gcache_check_data_passed_common_code
    	nop
    	
gcache_check_data_passed_set_0_1_2:
    	PASS_MSG("\Gcache RAM DATA test for sets 0, 1, 2 PASSED\r\n")

gcache_check_data_passed_common_code:
        li      v0, 0
    	DMFC1(ra, RA_SAVE)
    	j   ra
    	nop

99:
	DMTC1(t3, T3_SAVE)
	DMTC1(gp, GP_SAVE)
	DMTC1(a1, A1_SAVE)
	DMFC1(a0, A0_SAVE)
    	beq a0, r0, gcache_check_data_failed_set_0_1_2
    	nop

gcache_check_data_failed_set_3:
    	ERROR_MSG("Gcache RAM DATA test for set 3 FAILED\r\n")
    	j   gcache_check_data_failed_common_code
    	nop
gcache_check_data_failed_set_0_1_2:
    	ERROR_MSG("Gcache RAM DATA test for set 0, 1, or 2 FAILED\r\n")

gcache_check_data_failed_common_code:
    	ERROR_MSG_FPREG("    set index:     ", T3_SAVE)
    	ERROR_MSG_FPREG("    address:       ", GP_SAVE)
    	ERROR_MSG_FPREG("    expected data: ", GP_SAVE)
    	ERROR_MSG_FPREG("    actual data:   ", A1_SAVE)
    	li  v0, EVDIAG_SCACHE_DATA
    	DMFC1(ra, RA_SAVE)
    	j   gcache_print_data_error_and_return
    	nop
END(gcache_check_data)



/* ******************************************************************************
 * gcache_check_tags
 * This routine writes the bus tags, proc tags, bus state, and proc state
 * tag rams and reads the values back to check that they are correct.  Currently
 * this routine does this for only sets 0, 1, and 2.  We are running in force
 * set 3 mode, so this means that any gcache misses we take will replace set 3,
 * and will not interfere in any way with our checking of sets 0, 1, and 2.
 *
 * This routine takes four arguments:
 *    a0: tag init value
 *        This value is used to calculate the value written to the bus/proc
 *        tags.  This value is written to the bus tag for line 0, then we add
 *        the tag_increment_value to it and write this new value to the proc tag for
 *        line 0.  Then we increment by the tag_increment_value again, and write the
 *        new value to the bus tag for line 1, etc.  When we write the tags, bits
 *        21:20 of the data written is actually taken from bits 15:14 of the tag
 *        address.  We write to the tag the value we computed into a0, but when we
 *        later check the tags we make the assumption that bits 21:20 were taken from
 *        15:14 of the tag address, so we are checking with a value that is slightly
 *        different than the value we wrote.  
 *    a1: tag increment value
 *    a2: state init value
 *        This value is used to calculate the value written to the bus/proc
 *        state.  This value is written to the bus state for line 0, then we add
 *        the state__increment_value to it and write this new value to the proc state for
 *        line 0.  Then we increment by the state_increment_value again, and write the
 *        new value to the bus state for line 1, etc.  Since only bits 11:0 of the
 *        bus state are bits we want to write with values, the increment value is added
 *        to a2 modulo 0xfff, so that only bits 11:0 change.  Bits 16:12 are always
 *        written with '1's because these bits are write enables for bits 11:0.
 *        In the case of the proc state, we have a similar situation except that the 
 *        data we write must be shifted left 22 bits.
 *    a3: state increment value
 *    t2: number of sets to initialize, starting at set specified by t3
 *    t3: first set to initialize
 *    v1: specifies whether we should only set the tags, or whether we should
 *        set them and then read them back to confirm they contain the right values.
 *        0 specifies set them, only.  1 specifies set them and then check the
 *        values. 
 * Note, all the of argument registers were chosen carefully to avoid conflicting
 * with registers t0, t1, v0, ra, which SC_GET_NUM_LINES_IN_GCACHE() changes!
 *
 * Register Conventions
 * a0  = argument - tag init value (data stored in bus/proc tags)
 * a1  = argument - tag increment value
 * a2  = argument - state init value (data stored in bus/proc state)
 * a3  = argument - state increment value
 * ta0 = bus tag address
 * ta1 = proc tag address
 * ta2 = bus state address
 * ta3 = proc state address 
 * t0  = num of lines in gcache
 * t1  = gcache line index
 * t2  = argument - num of sets to initialize
 * t3  = argument - first set to initialize
 * t8  = scratch.  Holds actual value in loop 2, and in error code
 * t9  = scratch.  Holds expected value in loop 2, and in error code
 * 
 * v1  = argument - 0 to set tags, 1 to set and then read back and check their values
 * gp  = scratch
 * ra  = return address 
 * Note: this routine saves ra in SP_SAVE so that the routines that call
 *       this routien can use SP_RA to save their ra.
 * ******************************************************************************/
LEAF(gcache_check_tags)
        .set    noreorder

    	DMTC1(ra, SP_SAVE)
    	DMTC1(v0, V0_SAVE)
    	SC_GET_NUM_LINES_IN_GCACHE(t0)	# t0 = num of lines
    	DMFC1(v0, V0_SAVE)

/* *****************************************************************************
 * Loop 1 is an outer loop that contains loops 2 and 3.  We execute loop 1 once
 * for each of the three sets: 0, 1, 2.  Inside loop 1 we execute loop 2 for
 * each line in the gcache, and then we execute loop 3 for each line in the 
 * gcache.  Loop 2 writes values to the gcache tagram for the set we are 
 * working on and loop 3 reads back the tagram values and checks that they
 * are correct.
 *******************************************************************************/

1: 
    	# ta0,ta1,ta2,ta3 = BB_BUSTAG_ADDR,BB_PTAG_E_ADDR,BB_BUSTAG_ST,BB_PTAG_E_ST
    	SC_TAGRAM_ADDR_INIT_FOR_SET(ta0, ta1, ta2, ta3, t3, v0, gp)
	li	t1, 0	    	    	# t1 = line index
    	andi	a2, 0xfff   	    	# a2 = state init value, only 11:0 valid

/* *****************************************************************************
 * Loop 2 runs through all lines in the gcache, for a single set, setting the
 * bus tags, proc tags, bus state, proc state based on arguments a1, a1, a2, a3.
 *******************************************************************************/
2:
        sd      a0, (ta0)  	    	# write gcache bus tag 
    	daddu	a0, a1	    	    	# increment tag by 1

        sd      a0, (ta1)  	    	# write gcache proc tag
    	daddu	a0, a1	    	    	# increment tag by 1

    	li  	gp, 0x1f000 	    	# bus state 'state write enable'
        or  	t8, gp, a2
        sd  	t8, (ta2)    	    	# write gcache bus state
    	daddu	a2, a3	    	    	# increment state value by 1
    	andi	a2, 0xfff   	    	# unnecessary?

    	dsll	a2, 22	    	    
    	dli 	gp, 0x7c00000000    	# proc state 'state write enable'
        or  	t8, gp, a2
        sd  	t8, (ta3)     		# write gcache proc state
    	dsrl	a2, 22
    	daddu	a2, a3	    	    	# increment state value by 1
    	andi	a2, 0xfff   	    	# unnecessary?

    	SC_TAGRAM_ADDR_NEXT_LINE(ta0, ta1, ta2, ta3, gp)
        daddiu   t1, 1
        bltu    t1, t0, 2b
        nop
    	/* **** Loop 2 END ******************************************************/

    	beq 	v1, r0, 4f  	    	# if argument v1 is 0, don't do check
    	nop

/* *****************************************************************************
 * Loop 3 runs through all lines in the gcache, for a single set, and reads
 * the bus tags, proc tags, bus state, proc state.  For each datum read, it
 * checks that the value is equal to the data that we stored in Loop 1.
 *******************************************************************************/
    	SC_TAGRAM_ADDR_INIT_FOR_SET(ta0, ta1, ta2, ta3, t3, v0, gp)

        li      t1, 0	    	    	# t1 = line index
    	dmult	t0, a1
    	mflo	t8
    	dsll	t8, 1 
    	dsub 	a0, t8	    	    	# a0 = start data for bus/proc tags

    	dmult	t0, a3
    	mflo	t8
    	dsll	t8, 1
    	dsub	a2, t8	    	    	# a2 = start data for bus/proc state

3:    	/* **** Loop 3 LOOP *****************************************************/
    	ld  t8, 0(ta0)	    	    	# read bus tag
    	SC_TAG_BITS(t8, t8, gp)
    	SC_TAG_BITS_WITH_ADDR_BITS(t9, a0, ta0, gp)
        bne     t8, t9, 99f 	    	# branch on error
    	move	gp, ta0
    	daddu	a0, a1

        ld      t8, 0(ta1)  	    	# read even proc tag
    	SC_TAG_BITS(t8, t8, gp)
    	SC_TAG_BITS_WITH_ADDR_BITS(t9, a0, ta1, gp)
        bne     t8, t9, 99f 	    	# branch on error
    	move	gp, ta1

    	daddu	ta1, ta1, 0x80000    	# read odd proc tag
    	ld  	t8, 0(ta1)
    	SC_TAG_BITS(t8, t8, gp)
    	SC_TAG_BITS_WITH_ADDR_BITS(t9, a0, ta1, gp)
    	bne 	t8, t9, 99f 	    	# branch on error
    	move	gp, ta1
    	dsub	ta1, ta1, 0x80000
    	daddu	a0, a1

    	ld  t8, 0(ta2)
    	SC_STATE_BITS(t8, t8, gp)
    	SC_STATE_BITS(t9, a2, gp)
    	bne t8, t9, 99f
    	move	gp, ta2
    	daddu	a2, a3

    	ld  t8, 0(ta3)
    	SC_STATE_BITS(t8, t8, gp)
    	SC_STATE_BITS(t9, a2, gp)
    	bne t8, t9, 99f
    	move	gp, ta3

    	daddu	ta3, ta3, 0x80000    	# read odd proc tag state
    	ld  	t8, 0(ta3)
    	SC_STATE_BITS(t8, t8, gp)
    	SC_STATE_BITS(t9, a2, gp)
    	bne 	t8, t9, 99f 	    	# branch on error
    	move	gp, ta3
    	dsubu	ta3, ta3, 0x80000    	
    	daddu	a2, a3    	
   
    	SC_TAGRAM_ADDR_NEXT_LINE(ta0, ta1, ta2, ta3, gp)
	daddu 	t1, 1
	bltu	t1, t0, 3b 	    	# loop through each line in gcache
	nop
    	/* **** Loop 3 END ******************************************************/

4:
        daddiu  t3, 1
    	blt 	t3, t2, 1b  	    	# do outer loop for sets 0, 1, 2
        nop
    	/* **** Loop 1 END ******************************************************/

    	li  v0, 0
    	DMFC1(ra, SP_SAVE)
    	j   ra
    	nop

99:
	DMTC1(t1, T1_SAVE)
	DMTC1(t3, T3_SAVE)
	DMTC1(gp, GP_SAVE)
    	DMTC1(t9, T9_SAVE)
	DMTC1(t8, T8_SAVE)
    	li  v0, EVDIAG_SCACHE_TAG
    	DMFC1(ra, SP_SAVE)
    	j   ra
    	nop
END(gcache_check_tags)



/* *****************************************************************************
 * gcache_set_vsyn
 *
 * This routine is used by gcache_check_data to fix up the virtual synonym bits
 * of the G$ bus and processor state.  All it does is go through every tag in
 * the g$, read the state value and replace the virtual synonm bits of the
 * value read with new virtual synonym bits.  The new virtual synonm bits 
 * are chosen by using the address corresponding to the address of the
 * first double word of the line.  For example:
 *     line     address     vsyn
 *      0       0x0          0
 *      1       0x200        0
 *      2       0x400        0
 *      ...     ...         ...
 *      8       0x1000       1
 *      15      0x1e00       1
 *      ...     ...         ...
 *      16      0x2000       2
 *      ...     ...         ...
 *
 * This routine takes two arguments:
 *      t2: number of sets to initialize (does not change during execution)
 *      t3: first set index (gets changed during execution)
 *********************************************************************************/
LEAF(gcache_set_vsyn)
        .set    noreorder

    	DMTC1(ra, SP_SAVE)

    	SC_GET_NUM_LINES_IN_GCACHE(t0)	# t0 = num of lines

    	/* *****************************************************************************
    	 * Loop 1 is an outer loop that contains loop 2.  We execute loop 1 once
    	 * for each of the three sets: 0, 1, 2.  We execute loop 2 once for each line
    	 * in the gcache.  Loop 2 reads the bus tag state bits, OR's in the virtual
    	 * synonym, then writes them back.  Then it does the same for the proc tag state
    	 * bits.  The virtual syno
    	 *******************************************************************************/

    	li  	v0, 0x1f000 	    	# bus state 'state write enable'
    	dli 	v1, 0x7c00000000    	# proc state 'state write enable'
1: 
    	li  a0, 0
    	# ta0,ta1,ta2,ta3 = BB_BUSTAG_ADDR,BB_PTAG_E_ADDR,BB_BUSTAG_ST,BB_PTAG_E_ST
    	SC_TAGRAM_ADDR_INIT_FOR_SET(ta0, ta1, ta2, ta3, t3, a0, gp)
	li	t1, 0	    	    	# t1 = line index

2:    	/* **** Loop 1 LOOP *****************************************************/
    	dsll	t9, t1, 9
    	andi 	t9, t9, 0xf000
    	dsrl	t9, t9, 4    	    	# r9 contains virtual synonym in bits 11:8

    	ld  	t8, (ta2)
    	andi	t8, t8, 0xf0ff
    	or  	t8, t8, t9
    	or  	t8, t8, v0  	    	# Make bus state dirty & vsyn bits writable!
    	sd  	t8, (ta2)

    	ld  	t8, (ta3)
    	andi	t8, t8, 0xf0ff
    	or  	t8, t8, t9
    	dsll	t8, t8, 22
    	or  	t8, t8, v1  	    	# Make tag state dirty & vsyn bits writable!
    	sd  	t8, (ta3)
    	
    	SC_TAGRAM_ADDR_NEXT_LINE(ta0, ta1, ta2, ta3, gp)
        daddiu	t1, t1, 1
        bltu	t1, t0, 2b
        nop
    	/* **** Loop 1 END ******************************************************/

        daddiu  t3, 1
    	blt 	t3, t2, 1b  	    	# do outer loop for sets 0, 1, 2
        nop

    	/* **** Loop 0 END ******************************************************/
    	li  v0, 0
    	DMFC1(ra, SP_SAVE)
    	j   ra
    	nop
END(gcache_set_vsyn)

/* ******************************************************************************
 * gcache_print_tag_error_and_return
 *
 * This routine is called from the gcache checking routines after they have
 * printed out all of their detailed information about a gcache check that
 * failed. This routine prints out a more general message telling what type
 * of failure it was (bus tag, processor even tag, processor odd tag), what
 * chip, what cpu number, and what slot.  It assumes that at entry ra
 * contains the return address, v0 contains the return code, and GP_SAVE contais
 * the address of the failing location in the tagram.  Currently this routine
 * is called with a 'j', rather than a 'jalr'.  So, if x() calls y() and
 * y() calls gcache_print_tag_error_and_return(), when we return from
 * gcache_print_tag_error_and_retur() we return to routine x.
 ******************************************************************************/
LEAF(gcache_print_tag_error_and_return)
    	.set noreorder
    	DMTC1(ra, RA_SAVE)  	# save ra so we don't overwrite it during printing
    	DMTC1(v0, V0_SAVE)  	# save v0 so we don't overwrite it during printing
    	EV_GET_SPNUM(ta0, ta1)	# t0 = slot, t1 = proc number
    	DMFC1(gp, GP_SAVE)  	# gp contains the address that failed
    	DMTC1(ta0, TA0_SAVE)	# print macro takes FP reg as arg, so save slot in fpreg


    	li  a0, 0x00100000
    	and a4, a0, gp
    	bne a4, r0, print_proc
    	nop
print_bus:
    	bne ta1, r0, print_bus_cpu1
    	nop
print_bus_cpu0:
    	ERROR_MSG_FPREG("*** Bus tag failure on chip #N9K7, cpu 0, slot ", TA0_SAVE)
    	j   print_done
    	nop
    	
print_bus_cpu1:
    	ERROR_MSG_FPREG("*** Bus tag failure on chip #N9K7, cpu 1, slot ", TA0_SAVE)
    	j   print_done
    	nop

print_proc:
    	li  a0, 0x00080000
    	and a4, a0, gp
    	bne a4, r0, print_proc_odd
    	nop
print_proc_even:
    	bne ta1, r0, print_proc_even_cpu1
    	nop
print_proc_even_cpu0:
    	ERROR_MSG_FPREG("*** Processor even tag failure on chip #JOH7, cpu 0, slot ", TA0_SAVE)
    	j   print_done
    	nop
print_proc_even_cpu1:
    	ERROR_MSG_FPREG("*** Processor even tag failure on chip #JOH7, cpu 1, slot ", TA0_SAVE)
    	j   print_done
    	nop
print_proc_odd:
    	bne ta1, r0, print_proc_odd_cpu1
    	nop
print_proc_odd_cpu0:
    	ERROR_MSG_FPREG("*** Processor odd tag failure on chip #JOJ5, cpu 0, slot ", TA0_SAVE)
    	j   print_done
    	nop
print_proc_odd_cpu1:
    	ERROR_MSG_FPREG("*** Processor odd tag failure on chip #JOJ5, cpu 1, slot ", TA0_SAVE)
    	j   print_done
    	nop
print_done:
    	DMFC1(ra, RA_SAVE)
    	DMFC1(v0, V0_SAVE)
    	j   ra
    	nop
END(gcache_print_tag_error_and_return)






/* pon_validate_scache_d
 * Initialize GCache Tag and State RAMs where D-cache mapped to dirty 
 * exclusive.
 * GCached should be in a state like pon_initialize_scache.  i.e. everything
 * invalid and set 3 tags are those of BOOT PROM addresses.
 *
 * The registers being used and their usage are as follows:
 *
 * t0: loop count
 * t1: offset for each index
 * t2: base address of bus tag state
 * t3: base address of proc tag state 
 * ta0: bus tag state to write
 * ta1: proc tag state to write
 * ta2: bus/proc tag tag-addr to write
 * ta3: address of bus tag addr
 * v1: address of proc tag addr
 *
 * SAQs should not have any pending cached-write to G-cache when this 
 * routine is called.
 * (flush_SAQueue can be called prior to pon_initialize_scache to flush 
 * pending G-cache write's if any in SAQs.)
 */

#define	GTAG_TG_DC    (((POD_STACKADDR >> 20) & 0xfffff) << 20)
#define BTAG_ST_DC_EX (((0xf000 & POD_STACKADDR) >> 4) | 0x1f0aa)
#define PTAG_ST_DC_DE (((0xf000 & POD_STACKADDR) << 18) | 0xfc2a800000)
/* Shift right by 6 b/c we shift right by 9 to get the g$ index, then
   shift left by 3 to multiply the g$ index by 8 to get index into
   tag ram
*/
#define GTAG_DC_S_OFF (((POD_STACKADDR & 0xfc000) >> 6) | 0x30000)

/* this GTAG_DC_S_OFF is set3 with virtual synonym 1100 mmeory 
   mapped address, every 128 entries increment one v.s value, ie. 
   advance 0300 address, we use 16K page size, each scache tag has
   2048 entries, virtual M. value range from 0-15,  */



LEAF(pon_validate_scache_d)
	.set	noreorder
	dli	t0, CC_ADDR(0x1fc0f000)
	ld	t1, 0(t0)
	ld	t1, 0(t0)

	li	t0, 32
	li	t1, GTAG_DC_S_OFF	# Start index of G-cache tag to mapped D-cache

	dli	t2, BB_BUSTAG_ADDR	# bus tag address base
	dli	t3, BB_PTAG_E_ADDR	# proc tag address base
	dli	t8, BB_BUSTAG_ST	# bus tag state base
	dli	t9, BB_PTAG_E_ST	# proc tag state base
	dli	ta0, BTAG_ST_DC_EX	# bus tag state to map dcache exclusive
					# sectors state exclusive
	dli	ta1, PTAG_ST_DC_DE	# proc tag state to map dcache dirty exclusive'
	#dli	ta2, GTAG_TG_DC		# bus/proc tag tag-addr to map dcache
	dli	ta2, POD_STACKADDR 	# bus/proc tag tag-addr to map dcache

	dli	a0, POD_STACKADDR
	dli	a1, 0xfc2a800000
	dli	a2, 0x1f0aa
1:
	daddu	ta3, t2, t1	# address of bus tag addr
	daddu	v1, t3, t1	# address of proc tag addr
	sd	ta2, 0(ta3)	# write to bus tag addr
	sd	ta2, 0(v1)	# write to proc tag addr (even & odd)
	daddu	ta3, t8, t1	# address of bus tag state
	daddu	v1, t9, t1	# address of proc tag state
	sd	ta0, 0(ta3)	# write to bus tag state
	sd	ta1, 0(v1)	# write to proc tag state
	addi	t0, -1
	daddiu	a0, 0x200	# scache line size
	and	a3, a0, 0xf000	# recompute bus virtual synonym
	dsrl	a3, 4
	or	ta0, a3, a2
	and	a3, a0, 0xf000	# recompute processor virtual synonym
	dsll	a3, 18
	or	ta1, a3, a1
	daddu	ta2, 0x200 
	bgt	t0, zero, 1b	# Done with all 32 entries (mapped into 16KB)?
	addi	t1, t1, 8	# (BD) next index of G-cache tag to mapped D-cache
	j	ra
	nop
	.set	reorder
END(pon_validate_scache_d)

    
 /*
 * pon_fix_dcache_parity()
 * put dcache into useful state for power-on diagnostics or
 * for running pon command monitor.
 * Secondary (G-cache) tags where D-cache mapped are set to dirty exclusive
 * (D-cache may not be mapped to addresses congrurent to BOOT PROM.)
 *      all PD tags have correct parity.
 *      all PD lines marked exclusive.
 *      all PD data is initialized with correct parity.
 * Do so without causing bus cycles.
 * Does not use a stack. Cannot be called from C, since it does
 * not respect register conventions.
 * Note that this routine is level-1 in the calling chain.
 */
LEAF(pon_fix_dcache_parity)
    	.set noreorder
        move    Ra_save1,ra

        # mark each line exclusive using TFP store-tag
        jal     pon_setup_stack_in_dcache
    	nop

        # mark each line exclusive in G-cache
        jal     pon_validate_scache_d
    	nop

        # now, since all lines valid and exclusive (ensuring no memory
        # fetches as long as we hit both caches), initialize the data
        # in order calculate correct parity and set the WB bit for each
        # line.  First, get dcache size.
        # Because the data is bogus, set SR to ignore parity/ecc errors.

        jal     get_dcachesize
    	nop

        DMFC0(t3, C0_SR)
        dli     t1, POD_SR
        DMTC0(t1, C0_SR)

        # store to each line, t2 has loop termination
        dli     t1, POD_STACKADDR
        daddu   t2, t1, v0           # t2 is loop termination
                                     # (stack address + length)
1:
        sd      zero,0(t1)
        daddi   t1, 8                # increment to next doubleword
        bltu    t1, t2, 1b

        DMTC0(t3, C0_SR)
        j       Ra_save1             # Jump to saved return address
    	nop
        END(pon_fix_dcache_parity)

/* Two icaches line, each instruction takes 4 bytes, 16 instructions
   takes 64 bytes. Each icache line takes 32 bytes.
   Two icache lines operation add 0xffff to register,ie, signed
   extended become add -1 to the register, after 32 icache lines
   operation we add substract 256 from the register */

#define TWO_LINES_TCODE                         \
        addiu   ta2, s5, 0x0001;               \
        addiu   s5, ta2, 0x0002;               \
        addiu   ta2, s5, 0x0004;               \
        addiu   s5, ta2, 0x0008;               \
        addiu   ta2, s5, 0x0010;               \
        addiu   s5, ta2, 0x0020;               \
        addiu   ta2, s5, 0x0040;               \
        addiu   s5, ta2, 0x0080;               \
        addiu   ta2, s5, 0x0100;               \
        addiu   s5, ta2, 0x0200;               \
        addiu   ta2, s5, 0x0400;               \
        addiu   s5, ta2, 0x0800;               \
        addiu   ta2, s5, 0x1000;               \
        addiu   s5, ta2, 0x2000;               \
        addiu   ta2, s5, 0x4000;               \
        addiu   s5, ta2, 0x8000

/* Four icache lines per gcache line (128 bytes) */
#define FOUR_LINES_TCODE		\
	TWO_LINES_TCODE;		\
	TWO_LINES_TCODE

/* 32 icache lines per 1k */
#define THIRTYTWO_LINES_TCODE                   \
        FOUR_LINES_TCODE; FOUR_LINES_TCODE;     \
        FOUR_LINES_TCODE; FOUR_LINES_TCODE;     \
        FOUR_LINES_TCODE; FOUR_LINES_TCODE;     \
        FOUR_LINES_TCODE; FOUR_LINES_TCODE      \


#define ICACHE_LINE_CODE                                \
        addu    zero, zero, zero;       /* 0 */         \
        addu    zero, zero, zero;       /* 4 */         \
        addu    zero, zero, zero;       /* 8 */         \
        addu    zero, zero, zero;       /* 12 */        \
        addu    zero, zero, zero;       /* 16 */        \
        addu    zero, zero, zero;       /* 20 */        \
        addu    zero, zero, zero;       /* 24 */        \
        addu    zero, zero, zero        /* 28 */

/* Four icache lines per gcache line (128 bytes) */
#define FOUR_LINES_CODE		\
	ICACHE_LINE_CODE;	\
	ICACHE_LINE_CODE;	\
	ICACHE_LINE_CODE;	\
	ICACHE_LINE_CODE	\

/* 32 icache lines per 1k */
#define THIRTYTWO_LINES_CODE			\
	FOUR_LINES_CODE; FOUR_LINES_CODE;	\
	FOUR_LINES_CODE; FOUR_LINES_CODE;	\
	FOUR_LINES_CODE; FOUR_LINES_CODE;	\
	FOUR_LINES_CODE; FOUR_LINES_CODE	\


/* Initialize DCache by invalidating it.  DCache is 16KB, with 32Byte line, 
 * 28 bits Tag + 1 bit Exclusive + 8 bits Valid + 32 Bytes Data
 * Need two consecutive writes to half-line boundaries to clear Valid bits
 * for each line.
 */

LEAF(pon_invalidate_dcache)
	.set	noreorder
        move    t3, ra
        li      v1, 16			# size of the half of Dcache line
        jal     get_dcachesize
	nop

        # v1: half line size, v0: cache size to be initialized

        dli     t1,POD_STACKADDR	# Starting address for initializing
					# 	Dcache
        daddu   t2, t1, v0              # t2: loop terminator

        # for each cache line:
        # 	mark it invalid
1:
        DMTC0(t1,C0_BADVADDR)
	DMTC0(zero,C0_DCACHE)		# clear all (4) Valid bits
	dctw				# write to the Dcache Tag 
	ssnop; ssnop; ssnop		# pipeline hazard prevention
        .set    reorder

        daddu   t1, v1                 # increment to next half-line
        bltu    t1, t2, 1b		# t2 is termination count
	move	v0, zero
        j       t3
END(pon_invalidate_dcache)

LEAF(pon_invalidate_IDcaches)
	.set	noreorder
	.align 5			# first invalidate the icache

	THIRTYTWO_LINES_CODE; THIRTYTWO_LINES_CODE	/* 2k */
	THIRTYTWO_LINES_CODE; THIRTYTWO_LINES_CODE	/* 4k */
	THIRTYTWO_LINES_CODE; THIRTYTWO_LINES_CODE	/* 6k */
	THIRTYTWO_LINES_CODE; THIRTYTWO_LINES_CODE	/* 8k */
	THIRTYTWO_LINES_CODE; THIRTYTWO_LINES_CODE	/* 10k */
	THIRTYTWO_LINES_CODE; THIRTYTWO_LINES_CODE	/* 12k */
	THIRTYTWO_LINES_CODE; THIRTYTWO_LINES_CODE	/* 14k */
	THIRTYTWO_LINES_CODE; THIRTYTWO_LINES_CODE	/* 16k */

	j	pon_invalidate_dcache	# now go do the dcache
	nop
END(pon_invalidate_IDcaches)


/*
 * pon_test_Icaches
 * icache is 16KB, 512 cache lines, each line with 32 bytes.
 * icache test before Chandra mode.
 * use r10 (ta2) and r21 (s5), their binary representations (01010 and
 * 10101) are opposites.
 *
 */

LEAF(pon_test_Icaches)
        .set    noreorder
        move	t3, ra
	dli	ta2, 0
	dli	s5, 0

        .align 5                        # first invalidate the icache
        THIRTYTWO_LINES_TCODE; THIRTYTWO_LINES_TCODE      /* 2k */
        THIRTYTWO_LINES_TCODE; THIRTYTWO_LINES_TCODE      /* 4k */
        THIRTYTWO_LINES_TCODE; THIRTYTWO_LINES_TCODE      /* 6k */
        THIRTYTWO_LINES_TCODE; THIRTYTWO_LINES_TCODE      /* 8k */
        THIRTYTWO_LINES_TCODE; THIRTYTWO_LINES_TCODE      /* 10k */
        THIRTYTWO_LINES_TCODE; THIRTYTWO_LINES_TCODE      /* 12k */
        THIRTYTWO_LINES_TCODE; THIRTYTWO_LINES_TCODE      /* 14k */
        THIRTYTWO_LINES_TCODE; THIRTYTWO_LINES_TCODE      /* 16k */

        dli     ta2, 0xffffffffffffff00
        bne     s5, ta2, icache_fail
        nop
	j	t3
	nop

icache_fail:
        j       flash_cc_leds
        ori     a0, zero, FLED_ICACHE_FAIL
	j	t3
	nop
END(pon_test_Icaches)

/*
 * dcache_tag
 * basic dcache tag ram test
 */
/* #define DC_TAG_ADDRESS CC_ADDR(0x1fcfc000) */
#define DC_TAG_ADDR_START CC_ADDR(0x1cc00000)

#ifdef SABLE
#define DC_TAG_ADDR_END   CC_ADDR(0x1cc0ffff)
#else
#define DC_TAG_ADDR_END   CC_ADDR(0x1cc3ffff)
#endif

#define VALID_EX_BIT_MASK  0x780000000000800

LEAF(dcache_tag)
        .set    noreorder
        move    Ra_save1, ra
	/*
        li      v1, 16       	     # size of the half of Dcache line
				     # each cache tagram entry contains
				     # 4 64 bit data 
        jal     get_dcachesize	  # 16k bytes
        nop
	*/

        # for each cache line in cache tag ram fill up the tag
	# with various tag patterns and do cache tag write
	# do this for whole 16kB Dcache tag
        .set    noreorder
1:
        dli     t1, DC_TAG_ADDR_START  
        dli	t2, DC_TAG_ADDR_END

2:
        DMTC0(t1,C0_BADVADDR)
	dli 	s5, 0x7777777777777777
	dli	s4, 0xfffffff000
	and	s5, s4
	dli	s4, VALID_EX_BIT_MASK
	or      s5, s4
        DMTC0(s5, C0_DCACHE)
        dctw                       # Dcache Tag Write
        ssnop; ssnop; ssnop        # pipeline hazard prevention
        daddu   t1, 0x10	# increment to next half-line
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
	dctr			   # Dcache Tag Read
	ssnop;ssnop
	DMFC0(t3, C0_DCACHE)	   # Read from Dcache register
	bne     t3, s5, 99f

	ssnop
        daddu   t1, 0x10	# increment to next half-line
				   # increment by 16 
        bltu    t1, t2, 3b         # t2 is termination count
	ssnop

	nop

        .set    reorder

        # done with no error
	dla     a0, dcache_tag_pass     # dcache tagram pass msg
	jal     pod_puts

        move    v0, zero                        # zero retn -> no error
        j       Ra_save1                        # Jump to saved ra

99:
	.set	noreorder
	dla	a0, dcache_tag_fail	# dcache tagram fail msg
	jal 	pod_puts
	nop

	move 	a0,t1
	jal	pod_puthex   		# output dcache tag entry 
	nop

        dla     a0, crlf
        jal     pod_puts
	nop

        .set    reorder
        li  	v0, EVDIAG_DCACHE_TAG 
        j       Ra_save1                        # Jump to saved ra
END(dcache_tag)

	.data
dcache_tag_pass:
	.asciiz "\Dcache tag test pass \r\n"
dcache_tag_fail:
	.asciiz "\Dcache tag test failed at entry "
tag_pattern:
	.word 0xaaaaaaaa, 0x55555555, 0, 0xffffffff
	.text

/* Initialize Store Address Queue by issuing (SAQ_DEPTH) Even and
 * (SAQ_DEPTH) Odd uncached-writes to two even and odd aligned
 * local registers - 2 unused local CC register addresses used
 */
LEAF(flush_SAQueue)
        .set    noreorder
        dli     t0, SAQ_INIT_ADDRESS    # address to start (writing) at
        li      t1, SAQ_DEPTH           # number of entries in the queue
1:
        sd      zero, 0(t0)             # write (even) 8 bytes
        sd      zero, 8(t0)             # write (odd) 8 bytes
        addi    t1, -1
        bnez    t1, 1b
        nop
        j       ra
        nop
        .set    reorder
END(flush_SAQueue)


/*
 * Return the line size of the primary dcache
 */
LEAF(get_dcacheline)
        .set    noreorder
        DMFC0(v0, C0_CONFIG)
        .set reorder
        and     v0, CONFIG_DB
        beq     v0, zero, 1f
        li      v0, 32
        j       ra
1:
        li      v0, 16
        j       ra
        END(get_dcacheline)



/*
 * return the size of the primary data cache
 */
LEAF(get_dcachesize)
        .set    noreorder
        DMFC0(t0,C0_CONFIG)
        and     t0,CONFIG_DC
        srl     t0,CONFIG_DC_SHFT
        addi    t0,MIN_CACH_POW2
        li      v0,1
        sll     v0,t0
        j       ra
    	nop
        END(get_dcachesize)

/*
 * Return the line size of the primary icache
 */
LEAF(get_icacheline)
        .set    noreorder
        DMFC0(v0, C0_CONFIG)
        and     v0, CONFIG_IB
        beq     v0, zero, 1f
        li      v0, 16                  #this is reversed as Everest
        j       ra
    	nop
1:
        li      v0, 32
        j       ra
    	nop
        END(get_icacheline)

/*
 * return the size of the primary instruction cache
 */
LEAF(get_icachesize)
        .set    noreorder
        DMFC0(t0,C0_CONFIG)
        and     t0,CONFIG_IC
        srl     t0,CONFIG_IC_SHFT

	/* TFP encodes chip revision number in IC field for some revs of
	 * chip.
	 */
	DMFC0(t2,C0_PRID)
	andi	t2, t2, 0xff
	bne	t2, zero, 1f		# if revision non-zero, no encoding
	nop

	/* We've hit encoded case (PRID == 0)
	 */
	beq	t0, zero, 1f		# let encode of zero
	li	t0, 0x04		#     be a 32K icache
	li	t0, 0x03		# all others be 16K icache

1:      addi    t0,MIN_CACH_POW2-1
        li      v0,1
        sll     v0,t0
        j       ra
    	nop
        END(get_icachesize)


/*
 * check_scachesize
 * return the size of the secondary cache
 *
 * Assume the gcache tag ram size options are 1,2,4,8,& 16 MB.
 * By loading the following data in the order shown:
 *
 *       Write   Read    Read    Read    Read    Read
 * Addr:   Data:   16MB    8MB     4MB     2MB     1MB
 * index
 *    0       f     f        7       3       1       0
 *  512       e     e        6       2       0       0
 * 1024       d     d        5       1       1       0
 * 1536       c     c        4       0       0       0
 * 2048       b     b        3       3       1       0
 * 2560       a     a        2       2       0       0
 * 3072       9     9        1       1       1       0
 * 3584       8     8        0       0       0       0
 * 4096       7     7        7       3       1       0
 * 4608       6     6        6       2       0       0
 * 5120       5     5        5       1       1       0
 * 5632       4     4        4       0       0       0
 * 6144       3     3        3       3       1       0
 * 6656       2     2        2       2       0       0
 * 7168       1     1        1       1       1       0
 * 7680       0     0        0       0       0       0
 *
 * If the data is then read back (after all data has been written) it
 * should match one of the patterns in the five read columns.  The
 * column it matches gives the size of the cache.
 * we need to check the bus tag ram, odd tag ram and even tag ram
 * and make sure they are consistent.
 *
 * v0: return scache size
 */

LEAF(check_scachesize)
        .set    noreorder
	move 	s3, ra
#if SABLE 
	j	s3
	li 	v0, 4			# 4MB (BDSLOT)
#endif
	dli	t0, 0x9000000018080000  # uncacheable addr
	ld	t1, 0(t0)
	ld	t1, 0(t0)

/* Max GCache tag ram size is 16MB. */
	li	t0, 0x1fff		# 8192 entries,indeces loop terminator,max 16MB
	li	t1, 0			# t1 indicates which index
	li	s5, 512			# 512 entries for one value
	li	s6, 16			# 16 possible values
1:
	dli	v0, BB_BUSTAG_ADDR	# bus tag address base
	dli	v1, BB_PTAG_E_ADDR	# proc tag address base
	dli	t8, BB_BUSTAG_ST	# bus tag state base
	dli	t9, BB_PTAG_E_ST	# proc tag state base
	dli	a1, BTAG_ST_WRITE		# set dirty enable, virtual synonym
					# enable and state exclusive
	dli	a2, PTAG_ST_WRITE		# proc tag state init value

	dla	s1, tag_value
	lw	ta0, 0(s1)
	and	ta0, 0xfffff00000 # data to be written into set 0 of bus & proc 
				# tag addr. (i.e.  start from set 0)
2:
	sll	t2, t1, 3	# shift index count into appropriate position
	or	t3, v0, t2	# address of bus tag addr.
	or	ta1, v1, t2	# address of proc tag addr.
	or	ta2, t8, t2	# address of bus tag state
	or	ta3, t9, t2	# address of proc tag state

        /* 4 write operations */
	sd	ta0, 0(t3)	# write into bus tag addr.
	sd	ta0, 0(ta1)	# write into proc tag addr.
	sd	a1, 0(ta2)	# write into bus tag state
	sd	a2, 0(ta3)	# write into proc tag state

	subu	s5, 1
	daddu 	t1, 1
	bnez    s5, 2b
	nop

	daddiu  s1, 4
	lw	ta0, 0(s1)
	and	ta0, 0xfffff00000 # data to be written into set 0 of bus & proc 
	li	s5, 512
	subu	s6, 1
	bnez    s6, 2b
	nop

        # now read back the data and compare the value for addr index 0
        # if read back 0xf, report 16mb
        # if read back 0x7, report 8mb
        # if read back 0x3, report 4mb
        # if read back 0x1, report 2mb
        # if read back 0x0, report 1mb

	dli     t0, BB_BUSTAG_ADDR
        ld      a0, 0(t0)               # read back from bus tag ram
	srl	a0, 20
        daddiu  a0, 1                   # gcache bus tag ram size

        dli     t0, BB_PTAG_E_ADDR
        ld      a1, 0(t0)               # read back from even tag ram
	srl	a1, 20
        daddiu  a1, 1                   # gcache even tag ram size

        # read back fom odd ram tag
        dli     t0, BB_PTAG_O_ADDR
        ld      a2, 0(t0)               # read back from odd tag ram
	srl	a2, 20
        daddiu  a2, 1                   # gcache odd tag ram size

        # compare 3 tag ram size, they must be the same, if not
        # report error
        bne     a0, a1, 99f
        nop
        bne     a0, a2, 99f
        nop
        bne     a1, a2, 99f
        nop

        # check scache tag size is 1,2,4,8,or 16 only
        dla     t0, scache_tag_size
        li      t2, 5
20:
        lw      t1, 0(t0)
        bne     a0, t1, 30f
        nop
        li      t2, 1
        move    v0, a0
        j       s3
        nop
30:
        daddiu  t0, 4
        subu    t2, 1
        bltz    t2, 98f
        nop
        bnez    t2, 20b
        nop
        j       s3
98:
	jal	flash_cc_leds
	li	a0, FLED_SCACHE_SIZE_WRONG		# BDSLOT
99:
	jal	flash_cc_leds
	li	a0, FLED_SCACHE_SIZE_INCONSISTENT	# BDSLOT
END(check_scachesize)

	.data
tag_value:
        .word   0xf00000,0xe00000,0xd00000,0xc00000,0xb00000,0xa00000,0x900000
	.word	0x800000,0x700000,0x600000,0x500000,0x400000, 0x300000,0x200000
	.word   0x100000,0
scache_tag_size:
        .word   1,2,4,8,16
	.text


/*
 * return the size of the secondary cache as stored in the gcache size
 * register
 */
LEAF(read_scachesize)
#ifdef SABLE
	li      v0, 0x10000
	j       ra
	nop
#endif
	.set	noreorder
	EV_GET_SPNUM(t0, t1)            # t0 gets slot
                                        # t1 gets proc. number
	EV_GET_PROCREG(t0, t1, EV_CACHE_SZ, t0)
	andi	t0, 0x1f
	li	t1, 16			# make sure it's in the range 1-16
	ble	t0, zero, 99f
	nop
	bgt	t0, t1, 99f
	nop
        sll     v0, t0, 20 		# 1 - 1MB, 2 - 2MB, 4 - 4MB etc
	b	1f
	nop
99:
	li	t0, 4			# default to 4 mb
        sll     v0, t0, 20 		# 1 - 1MB, 2 - 2MB, 4 - 4MB etc
1:
	j	ra
	nop
	.set	reorder
        END(read_scachesize)

/* pon_setup_stack_in_dcache: set all PD lines to exclusive
 * state, with addrs beginning at POD_STACKADDR and rising
 * contiguously for 'dcachesize'.  All further cached-write
 * within this range will now write-through without genernating
 * a D-cache miss op to G-pipe.  When G-pipe is properly
 * initialized with exclusive states for all lines, no memory
 * op will be generated by G-pipe.
 */
#define DCACHE_EXCLUSIVE 0x0780000000000800

LEAF(pon_setup_stack_in_dcache)
	.set	noreorder
        move    t3, ra
        li      v1, 16			# size of the half of Dcache line
        jal     get_dcachesize
	nop

        # v1: half line size, v0: cache size to be initialized

        dli     t1,POD_STACKADDR	# Starting address for initializing
					# 	Dcache
        daddu   t2, t1, v0              # t2: loop terminator
	dli	ta0,DCACHE_EXCLUSIVE	# ta0: Mask to set dcache line valid exclusive

        # for each cache line:
        # 	mark all word exclusive
1:
	or	ta1, t1, ta0		# Set all valid (4) and exclusive (1) bits
        DMTC0(t1,C0_BADVADDR)
	DMTC0(ta1,C0_DCACHE)		# Setup D-cache register
	dctw				# write to the Dcache Tag 
	ssnop; ssnop; ssnop		# pipeline hazard prevention
        .set    reorder

        daddu   t1, v1                 # increment to next half-line
        bltu    t1, t2, 1b		# t2 is termination count
        j       t3
END(pon_setup_stack_in_dcache)


/* ******************************************************************************
 * gcache_print_data_error_and_return
 *
 * This routine is called from the gcache_check_data routines after they have
 * printed out all of their detailed information about a gcache data ram 
 * check that failed. This routine prints out a specific SIMM location failure
 * High Odd, High Even, Low Odd or Low Even).  
 * When bit 3 of physical address set 0, it's even SIMM. When bit 3 of 
 * physical address set 1, it's odd SIMM. If the data low 32 bit doesn't
 * match it's LOW SIMM, if the data hight 32 bit doesn't match it's HIGH
 * SIMM. Following are the actual SIMM module location on the board.
 *
 *                              CPU0    CPU1
 *				------------
 * Odd Low SIMM location        N6H1    G6H1
 * Odd High SIMM Location       N6G7    G6G7
 * Even Low SIMM Location       N6F9    G6G9
 * Even High SIMM Location      N6G3    G6G3
 *
 ******************************************************************************/
LEAF(gcache_print_data_error_and_return)
    	.set noreorder
    	DMTC1(ra, RA_SAVE)  	# save ra so we don't overwrite it during printing
    	DMTC1(v0, V0_SAVE)  	# save v0 so we don't overwrite it during printing
    	EV_GET_SPNUM(ta0, ta1)	# ta0 = slot, ta1 = proc number
	
    	DMFC1(gp, GP_SAVE)  	# gp contains the address that failed
        DMFC1(s5, A1_SAVE)      # s5 contains the actual data read back

	li  a0, 8		# check bit 3 of physical address.
    	and s6, a0, gp
    	bne s6, r0, odd_simm   # bit 3 set to 1, it's in odd SIMM
    	nop
even_simm:
	and s6, gp, 0xffffffff  # check low 32 bit data match
        and t3, s5, 0xffffffff
        bne s6, t3, even_low_simm_fail
        nop

4:
        dli s6, 0xffffffff00000000
        and s6, gp, s6          # check high 32 bit data match
        and t3, s5, s6
        bne s6, t3, even_high_simm_fail
        nop
	j	print_done2
	
odd_simm: 
	and s6, gp, 0xffffffff  # check low 32 bit data match
	and t3, s5, 0xffffffff	
	bne s6, t3, odd_low_simm_fail
	nop

5:
	dli s6, 0xffffffff00000000
	and s6, gp, s6		# check high 32 bit data match
	and t3, s5, s6
	bne s6, t3, odd_high_simm_fail
	nop
	j	print_done2

odd_low_simm_fail:
	bne ta1, r0, odd_low_cpu1
	nop
odd_low_cpu0:
	ERROR_MSG("*** Odd Low SIMM failure on cpu0 location N6H1\r\n")
	b	5b
	nop
odd_low_cpu1:
	ERROR_MSG("*** Odd Low SIMM failure on cpu1 location G6H1\r\n")
	b	5b
	nop


odd_high_simm_fail:
	bne ta1, r0, odd_high_cpu1
	nop
odd_high_cpu0:
	ERROR_MSG("*** Odd High SIMM failure on cpu0 location N6G7\r\n")
	j	print_done2
odd_high_cpu1:
	ERROR_MSG("*** Odd High SIMM failure on cpu1 location G6G7\r\n")
	j	print_done2


even_low_simm_fail:
	bne ta1, r0, even_low_cpu1
	nop
even_low_cpu0:
	ERROR_MSG("*** Even Low SIMM failure on cpu0 location N6F9\r\n")
	b	4b
	nop
even_low_cpu1:
	ERROR_MSG("*** Even Low SIMM failure on cpu1 location G6G9\r\n")
	b	4b
	nop


even_high_simm_fail:
	bne ta1, r0, even_high_cpu1
	nop
even_high_cpu0:
	ERROR_MSG("*** Even High SIMM failure on cpu0 location N6G3\r\n" )
	j	print_done2
even_high_cpu1:
	ERROR_MSG("*** Even High SIMM failure on cpu1 location G6G3\r\n")
	j	print_done2

print_done2:
    	DMFC1(ra, RA_SAVE)
    	DMFC1(v0, V0_SAVE)
    	j   ra
    	nop
END(gcache_print_data_error_and_return)

