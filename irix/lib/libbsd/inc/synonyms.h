/*
 * libbsd/inc/synonyms.h
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

#ident "$Revision: 1.4 $"

#if defined(__STDC__)
#include "libc_synonyms.h"

/* functions */
#undef	dup2
#undef	getgroups
#undef	getpgrp
#undef	setgroups
#undef	setpgrp

#define dup2		_lbsd_dup2
#define getgroups	_lbsd_getgroups
#define getpgrp		_lbsd_getpgrp
#define setgroups	_lbsd_setgroups
#define setpgrp		_lbsd_setpgrp

#endif  /* __STDC__ */
