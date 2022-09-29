/*
 * Copyright 1996, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 *
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 *
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that (i) the above copyright notices and this
 * permission notice appear in all copies of the software and related
 * documentation, and (ii) the name of Silicon Graphics may not be
 * used in any advertising or publicity relating to the software
 * without the specific, prior written permission of Silicon Graphics.
 *
 * THE SOFTWARE IS PROVIDED "AS-IS" AND WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS, IMPLIED OR OTHERWISE, INCLUDING WITHOUT LIMITATION, ANY
 * WARRANTY OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.
 *
 * IN NO EVENT SHALL SILICON GRAPHICS BE LIABLE FOR ANY SPECIAL,
 * INCIDENTAL, INDIRECT OR CONSEQUENTIAL DAMAGES OF ANY KIND, OR ANY
 * DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
 * WHETHER OR NOT ADVISED OF THE POSSIBILITY OF DAMAGE, AND ON ANY
 * THEORY OF LIABILITY, ARISING OUT OF OR IN CONNECTION WITH THE USE
 * OR PERFORMANCE OF THIS SOFTWARE.
 *
 */


/*
 * stacktrace_s.s
 */

#include <sys/regdef.h>
#include <sys/asm.h>

/*
    void *_stacktrace_get_pc();
*/
LEAF(_stacktrace_get_pc)
    move	v0,ra	/* our ra is pc of the caller, I hope */
    j		ra
END(_stacktrace_get_pc)


/*
    void *_stacktrace_get_sp();
*/
LEAF(_stacktrace_get_sp)
    move	v0,sp	/* this is a leaf so sp doesn't change, I hope */
    j		ra
END(_stacktrace_get_sp)


/*
    void _stacktrace_get_regs(void *regs[32]);
*/
LEAF(_stacktrace_get_regs)
	sw	$31,	31*4(a0)  /* XXX not right, but this app doesn't care */
	sw	$30,	30*4(a0)
	sw	$29,	29*4(a0)
	sw	$28,	28*4(a0)
	sw	$27,	27*4(a0)
	sw	$26,	26*4(a0)
	sw	$25,	25*4(a0)
	sw	$24,	24*4(a0)
	sw	$23,	23*4(a0)
	sw	$22,	22*4(a0)
	sw	$21,	21*4(a0)
	sw	$20,	20*4(a0)
	sw	$19,	19*4(a0)
	sw	$18,	18*4(a0)
	sw	$17,	17*4(a0)
	sw	$16,	16*4(a0)
	sw	$15,	15*4(a0)
	sw	$14,	14*4(a0)
	sw	$13,	13*4(a0)
	sw	$12,	12*4(a0)
	sw	$11,	11*4(a0)
	sw	$10,	10*4(a0)
	sw	$9,	9*4(a0)
	sw	$8,	8*4(a0)
	sw	$7,	7*4(a0)
	sw	$6,	6*4(a0)
	sw	$5,	5*4(a0)
	sw	$4,	4*4(a0)
	sw	$3,	3*4(a0)
	sw	$2,	2*4(a0)
	/* sw	$1,	1*4(a0) */
	sw	$0,	0*4(a0)
	j	ra
END(_stacktrace_get_regs)
