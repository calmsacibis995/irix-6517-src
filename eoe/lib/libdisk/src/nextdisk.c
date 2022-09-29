/**************************************************************************
 *									  *
 * 		 Copyright (C) 1988, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident	"libdisk/nextdisk.c $Revision: 1.3 $ of $Date: 1993/06/24 01:31:51 $"

/**	First version created by Dave Olson @ SGI, 10/88 **/

/* setnextdisk(), getnextdisk(), endnextdisk() */


#include <sys/types.h>
#include <sys/invent.h>
#include <diskinfo.h>
#include <invent.h>

static char *dev_dir;
unsigned defpart;

/* do setup to return disk info */
int
setnextdisk(char *devdir, unsigned pnum)
{
	if(dev_dir != (char *)0)
		endnextdisk();	/* set does an implied end */

	if(!devdir || setinvent() == -1)
		return -1;
	if(setdiskname(devdir)) {
		endinvent();
		return -1;
	}
	dev_dir = devdir;
	defpart = pnum;
	return 0;
}

void
endnextdisk(void)
{
	endinvent();
	enddiskname();
	dev_dir = (char *)0;
}


/* return name(s) of next disk */
struct disknames *
getnextdisk(void)
{
	inventory_t *pinv;
	struct disknames *dk;

	dk = 0;
	if(dev_dir != (char *)0)
		while(pinv = getinvent())
			if(dk = getdiskname(pinv, defpart))
				break;

	return dk;	/* null if nothing found */
}
