/*
 * kl_get_ra() -- 
 *
 * Returns its own return address. Used in conjunction with
 * macros to determine where a call to a particular function
 * originated from.
 */
#ident  "$Header: "

#include <sys/regdef.h>
#include <sys/asm.h>

#ifdef ALLOC_DEBUG
LEAF(kl_get_ra)
    move    v0,ra
    j       ra
END(kl_get_ra)
#endif
