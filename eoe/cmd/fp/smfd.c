
/* this file is used only if building version that runs under unix, and
	SCSI is defined, and SMFD_NAME is defined in fx.h.  It contains
	routines to implement code specific to the Scientific Micro Systems
	SCSI floppy external drive.
	Written by Dave Olson, 6/88
*/

#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <sys/param.h>
#include <sys/smfd.h>


/* have to match order of FD_FLOP_* */
/*static char *ftypes[7] = {"48", "96", "96hi", "3.5.800k", "3.5",*/
static char *ftypes[SMFD_MAX_TYPES] = {"48", "96", "96hi", "3.5.800k", "3.5",
	"3.5hi", "3.5.20m"};

/* given type ("partition" number), return string to create /dev name */
char *
smfd_partname(int p)
{
	if(p >= SMFD_MAX_TYPES)
		return NULL;
	return ftypes[p];
}
