#ident	"IP12diags/hpc3/passwd_jumper.c:  $Revision: 1.6 $"

/*
 * passwd_jumper.c - Return 0 if passwd_jumper is installed, -1 otherwise
 */

#include "sys/types.h"
#include "libsk.h"
#include "uif.h"

/*
 * Command to check if passwd jumper is installed on the IP22/IP26/IP28
 * Returns 0 if installed, non-zero if it is not installed
 */

int
passwd_jumper(void)
{
    int val = -1;

    if (jumper_off()) {
	msg_printf( VRB, "base board password jumper is not installed\n");
    }
    else {
	msg_printf( VRB, "base board password jumper is installed\n");
	val = 0;
    }

    return val;

}
