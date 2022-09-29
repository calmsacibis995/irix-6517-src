/******************************************************************************
 *
 * Title:       ct_libmp.s
 * Author:      galles
 * Date:        3/26/96
 *
 ******************************************************************************
 *
 * Description:
 *	Handy MP routines.
 *
 * Compile options:
 *
 *
 * Special notes:
 *
 *
 *****************************************************************************/

#include <asm.h>
#include <regdef.h>
#include <sys/cpu.h>
#include <sys/sbd.h>
		
	.text
		
	.set noreorder

	/* calling convention: barrier(num_cpus, *bar_busy, *bar_cnt);
	 *
	 * a0 = num_cpus
	 * a1 = *bar_busy
	 * a2 = *bar_cnt
	 */

LEAF(barrier)

	/* check bar_busy - needed so we cannot start barrier
	 * before finishing previous barrier (on another proc)
	 * which could potentially result in deadlock.
	 *
	 */
100:
	lw	t1,0(a1)
	bnez	t1,100b
	nop

	/* increment barrier count */
100:
	ll	t1,0(a2)
	addi	t1,t1,1
	move	t2,t1
	sc	t1,0(a2)
#ifdef R10000	/* up till 2.6 at least have trouble with some sc sequences */
	nop;nop;nop;nop;nop;nop;nop;nop
	nop;nop;nop;nop;nop;nop;nop;nop
	nop;nop;nop;nop;nop;nop;nop;nop
	nop;nop;nop;nop;nop;nop;nop;nop
#endif
	beqz	t1,100b
	nop
	
	/* last CPU clear cnt, set bar_busy:	 */
	bne	t2,a0,100f
	nop
	/* clear bar_cnt */
	sw	zero,0(a2)
	/* set bar_busy */
	li	t3,1
	sw	t3,0(a1)
	
	/* wait here until all procs have performed barrier,
	 * which is signalled by bar_busy going to 1
	 */
100:
	lw	t2,0(a1)
	beqz	t2,100b
	nop

	/* increment barrier count */
100:
	ll	t1,0(a2)
	addi	t1,t1,1
	move	t2,t1
	sc	t1,0(a2)
#ifdef R10000	/* up till 2.6 at least have trouble with some sc sequences */
	nop;nop;nop;nop;nop;nop;nop;nop
	nop;nop;nop;nop;nop;nop;nop;nop
	nop;nop;nop;nop;nop;nop;nop;nop
	nop;nop;nop;nop;nop;nop;nop;nop
#endif
	beqz	t1,100b
	nop

	/* last proc resets bar_cnt, bar_busy */
	bne	a0, t2, 100f
	nop
	/* clear bar_cnt */	
	sw	zero,0(a2)
	/* clear bar_busy */
	sw	zero,0(a1)
100:
	j	ra
	nop
	END(barrier)


	/* Calling convention:	 store_sc(int * ptr, int data);
	 *
	 */
	
LEAF(store_sc)

100:	move	t1, a1
	ll	t2, 0(a0)
	sc	t1, 0(a0)
#ifdef R10000	/* up till 2.6 at least have trouble with some sc sequences */
	nop;nop;nop;nop;nop;nop;nop;nop
	nop;nop;nop;nop;nop;nop;nop;nop
	nop;nop;nop;nop;nop;nop;nop;nop
	nop;nop;nop;nop;nop;nop;nop;nop
#endif
        beqz    t1,100b
	nop
	j	ra
	nop
	END(store_sc)

LEAF(get_config)
	.set	noreorder
	mfc0	v0,C0_CONFIG
	j	ra
	nop	
	.set	reorder
	END(get_config)

