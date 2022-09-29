/*
 * get_ra() -- Returns its own return address. Used in conjunction with
 *             macros to determine where a call to a particular function
 *             originated from.
 */

#ident  "$Revision: 1.1 $"

#include <sys/regdef.h>
#include <sys/asm.h>

#ifdef ALLOC_DEBUG
LEAF(get_ra)
    move    v0,ra
    j       ra
END(get_ra)
#endif
