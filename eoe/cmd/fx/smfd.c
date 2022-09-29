#include "fx.h"

#if !defined(_STANDALONE) && defined(SMFD_NAME)	/* entire file */
/* this file is used only if building version that runs under unix, and
	SCSI is defined, and SMFD_NAME is defined in fx.h.  It contains
	routines to implement code specific to the Scientific Micro Systems
	SCSI floppy external drive.
	Written by Dave Olson, 6/88
*/

#include <sys/scsi.h>
#include <sys/buf.h>
#include <sys/iobuf.h>
#include <sys/elog.h>
#include <sys/dksc.h>
#include <sys/smfd.h>

/* have to match order of FD_FLOP_* */
static char *ftypes[SMFD_MAX_TYPES] = {"48", "96", "96hi", "",  "3.5",
	"3.5hi", "3.5.20m"};

/* given type ("partition" number), return string to create /dev name */
char *
smfd_partname(int p)
{
	if(p >= SMFD_MAX_TYPES)
		return NULL;
	return ftypes[p];
}

/* return minor # for floppy to match the given type */
getfloptype(char *argstr)
{
	char abuf[256];
	int i;
	static int ftype = FD_FLOP_35;	/* static so last choice can be
		preserved across .. at top level, same as drive #, etc. */

	for(;;argstr=NULL) { /* argstr only on first pass, in case invalid */
		if(argstr)
			strncpy(abuf, argstr,sizeof(abuf));
		else {
			printf("which floppy type (one of");
			for(i=0; i< SMFD_MAX_TYPES; i++)
			  if (*ftypes[i]){
			    if(i)
			      printf(i == (SMFD_MAX_TYPES-1) ? " or" : ",");
			    printf(" %s", ftypes[i]);
			    }
			printf(") [%s] ", ftypes[ftype]);
			ggets(abuf, sizeof(abuf));
			if(!*abuf)
				return ftype;
		}

		for(i=0; i< SMFD_MAX_TYPES; i++)
			if(strcmp(abuf, ftypes[i]) == 0)
				return ftype=i;
		(void)qcheck(abuf);	/* check for .. answer */
		if(argstr)
			printf("%s is not a valid floppy type\n", argstr);
	}
	/*NOTREACHED*/
}


/* format the floppy */
void
do_smfdformat(int noask)
{
	do_scsiformat(noask);
}


#endif	/* entire file */
