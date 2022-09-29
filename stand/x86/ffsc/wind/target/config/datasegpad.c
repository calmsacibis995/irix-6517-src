/* dataSegPad.c - padding for beginning of data segment */

/* Copyright 1984-1991 Wind River Systems, Inc. */
#include "copyright_wrs.h"

/*
modification history
--------------------
01c,19oct92,jcf  change to include when INCLUDE_MMU_FULL defined.
01b,28jul92,rdc  changed PAGE_SIZE to VM_PAGE_SIZE.
01a,21jul92,rdc  written.
*/

/*
DESCRIPTION

This module conditionally creates a data structure the size of one page;
it is explicility listed as the first module on the load line when VxWorks
is linked to insure that this data structure is the first item in the
data segment.  This mechanism is needed to insure that the data segment
does not overlap a page that is occupied by the text segment;  when text
segment protection is turned on, all pages that contain text are write 
protected.  This insures that the data segment does not lie in a page that
has been write protected.  If text segment protection has not been included,
this module compiles into a null object module.  In an embedded system, this
mechanism may not be needed if the loader explicitly places the data segment
in a section of memory seperate from the text segment.

*/

#include "vxworks.h"
#include "config.h"

#ifdef	INCLUDE_MMU_FULL		/* bootroms will not ref dataSegPad.o */

char dataSegPad [VM_PAGE_SIZE] = {1};

#endif /* INCLUDE_MMU_FULL */
