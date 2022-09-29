#ident "$Revision: 1.4 $"

/*
 * Copyright 1992, Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 */

#include "libefs.h"
#include <sys/mkdev.h>

/* Translate the on-disk representation of dev (may be old or extended
 * style) into a dev_t. A new dev beyond the old range is indicated
 * by a -1 in the old dev field: this will cause older kernels to fail
 * to open the device.
 */

dev_t
efs_getdev(union di_addr *di)
{
	short olddev;
	major_t maj;

	if ((olddev = di->di_dev.odev) == -1)
		return (di->di_dev.ndev);
	else {
		maj = olddev >> ONBITSMINOR;
		return ((maj << NBITSMINOR) | (olddev & OMAXMIN));
	}
}

/* The converse: given an (extended) dev_t, put it into the on-disk
 * union. If its components are within the range understood by earlier
 * kernels, use the old format; if not, put -1 in the old field &
 * store it in the new one.
 */

void
efs_putdev(dev_t dev, union di_addr *di)
{
	major_t maj;
	minor_t min;

	if ( ((maj = (dev >> NBITSMINOR)) > OMAXMAJ) ||
	     ((min = (dev & MAXMIN)) > OMAXMIN)) {
		di->di_dev.odev = -1;
		di->di_dev.ndev = dev;
	}
	else
		di->di_dev.odev = (o_dev_t)((maj << ONBITSMINOR) | min);
}

