/* 
 * bridge.h will soon not provide the byte-offset macros
 * to C programs, except for *this* program.
 */
#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>

#include <sys/types.h>

int	errors = 0;

struct macrofield_s {
	char	       *mname;
	unsigned	mvalue;
	char	       *fname;
	unsigned	foffset;
};


/*
 * First parameter to MACROFIELD_LINE is the byte offset macro we want to
 * use for assembly code.
 *
 * Second parameter is the name of the field within the heart_t
 * stucture that we want C programs to use.
 *
 * Since we do not anticpate things changing, it is most likely that
 * any errors here are due to editing fumbles, and we do *not* want to
 * blindly update the macros from a fumbled C structure. Much better
 * to yell about the problem so the programmer can chase it down.
 *
 * We *might* be able to generate this table automagically from the
 * structure declaration, but this would only be worth the trouble if
 * we anticipate changes occurring, or we anticipate using this
 * facility in many places. Bear in mind that manual generation using
 * macros or global substitution in Your Favorite Editor is not
 * difficult at all, you just have to remember to go back and fix up
 * the arrays.
 */

#define	MACROFIELD_LINE(m,s,f)		{ #m, m, #f, offsetof (heart_##s, f)},
#define	MACROFIELD_LINE_BITFIELD(m)	/* ignored for checks */

#include "heart.h"

main()
{
	int	i;
	int	e = 0;
	int	l = sizeof heart_piu_macrofield / sizeof heart_piu_macrofield[0];

	for (i=0; i < l; i++)
		if (heart_piu_macrofield[i].mvalue != heart_piu_macrofield[i].foffset) {
			printf("%30s = 0x%06X %30s @ 0x%06X ERROR\n",
			       heart_piu_macrofield[i].mname, 
			       heart_piu_macrofield[i].mvalue,
			       heart_piu_macrofield[i].fname, 
			       heart_piu_macrofield[i].foffset);
			e ++;
		}

	l = sizeof heart_iou_macrofield / sizeof heart_iou_macrofield[0];
	for (i=0; i < l; i++)
		if (heart_iou_macrofield[i].mvalue != heart_iou_macrofield[i].foffset) {
			printf("%30s = 0x%06X %30s @ 0x%06X ERROR\n",
			       heart_iou_macrofield[i].mname, 
			       heart_iou_macrofield[i].mvalue,
			       heart_iou_macrofield[i].fname, 
			       heart_iou_macrofield[i].foffset);
			e ++;
		}
	printf("heart check done, %d errors\n", e);
	return (e != 0);
}
