/* 
 * xbow.h will soon not provide the byte-offset macros
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
 * Second parameter is the name of the field within the xbow_t
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

#define	MACROFIELD_LINE(m,f)		{ #m, m, #f, offsetof (xbow_t, f)},
#define	MACROFIELD_LINE_BITFIELD(m)	/* ignored for checks */

#include "xbow.h"

main()
{
	int	i;
	int	e = 0;
	int	l = sizeof xbow_macrofield / sizeof xbow_macrofield[0];

	for (i=0; i < l; i++)
		if (xbow_macrofield[i].mvalue != xbow_macrofield[i].foffset) {
			printf("%30s = 0x%06X %30s @ 0x%06X ERROR\n",
			       xbow_macrofield[i].mname, 
			       xbow_macrofield[i].mvalue,
			       xbow_macrofield[i].fname, 
			       xbow_macrofield[i].foffset);
			e ++;
		}
	printf("xbow check done, %d errors\n", e);
	return (e != 0);
}
