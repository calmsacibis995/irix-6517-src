/* $Header: /proj/irix6.5.7m/isms/eoe/cmd/patch/RCS/version.c,v 1.1 1996/01/17 02:45:14 cleo Exp $
 *
 * $Log: version.c,v $
 * Revision 1.1  1996/01/17 02:45:14  cleo
 * initial checkin.
 *
 * Revision 1.3  1995/10/11  18:36:32  lguo
 * No Message Supplied
 *
 * Revision 2.0  86/09/17  15:40:11  lwall
 * Baseline for netwide release.
 * 
 */

#include "EXTERN.h"
#include "common.h"
#include "util.h"
#include "INTERN.h"
#include "patchlevel.h"
#include "version.h"

void my_exit();

/* Print out the version number and die. */

void
version()
{
    fprintf(stderr, "Patch version %s\n", PATCH_VERSION);
    my_exit(0);
}
