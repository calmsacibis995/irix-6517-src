/*
 * _range.h
 *
 *
 * Copyright 1991, Silicon Graphics, Inc.
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

#ident "$Revision: 1.3 $"

static int
valid_range(wchar_t c1, wchar_t c2)
{
	wchar_t mask;
	if(MB_CUR_MAX > 3 || eucw1 > 2)
		mask = EUCMASK;
	else
		mask = H_EUCMASK;
	return (c1 & mask) == (c2 & mask) && 
	(c1 > 0377 || !iscntrl(c1)) && 
	(c2 > 0377 || !iscntrl(c2));
}
