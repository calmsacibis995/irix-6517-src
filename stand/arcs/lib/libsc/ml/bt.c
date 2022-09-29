#ident "$Revision: 1.3 $"

#include <libsc.h>

#define ADDIU_MASK	0x27bd0000
#define SWRASP_INSTR	0xafbf0000
#define MIPS_IMMASK	0x0000ffff
#define instck(instr,word)	(!((word&0xffff0000)^instr))

/* _btrace() does a backtrace "levels" level deep.  This routine
 * is called as btrace(levels), which is a assembler stub to get
 * the pc and sp from the calling location.
 *
 * Note that this is not symbolic at all.  It uses a brute force
 * heuristic of looking for the instructions that play with the
 * stack that are typically at the start of a function.
 *
 * The name of the function can be looked up with nm -x, and the
 * line that called the function can be found with dis.
 */

void
_btrace(int levels, unsigned long pc, unsigned long sp, int lev)
{
	unsigned long ra,call,*data,instr;
	int i;
	short spd;

	if (lev >= levels)		/* safety */
		return;

	printf("%2d: ",lev);

	data = (unsigned long *)pc;
	for (ra=spd=i=0; (i > -1024) && !(ra && spd) ; i--,data--) {
		if (instck(ADDIU_MASK,*data) && !spd) {
			spd = - (short) (*data&MIPS_IMMASK);
			if (spd < 0) spd = 0;	/* faked */
		}
		else if (instck(SWRASP_INSTR,*data) && !ra) {
			ra = (*data&MIPS_IMMASK);
		}
	}

	if (!spd || !ra) {
		printf("done\n");
		return;
	}

	call = 0;
	if (ra) {
		if (ra = *(unsigned long *)(sp+ra)) {
			instr = *(unsigned long *)(ra-8);

			/* if jal */
			if ((instr & 0xfc000000) == 0x0c000000)
				call = ((instr&0x03ffffff) << 2) |
					((ra-8) & 0xf0000000);
		}
	}

	if (call)
		printf("0x%8.8lx",call);
	else
		printf("0x????????");

	printf("(), pc=0x%8.8lx\n",pc);

	_btrace(levels,ra,sp+spd,lev+1);
}
