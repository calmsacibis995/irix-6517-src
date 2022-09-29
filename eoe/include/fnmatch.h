#ifndef _FNMATCH_H
#define _FNMATCH_H
#ifdef __cplusplus
extern "C" {
#endif
#ident "$Revision: 1.2 $"
/*
*
* Copyright 1995, Silicon Graphics, Inc.
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

#define FNM_PATHNAME	0001
#define FNM_PERIOD	0002
#define FNM_NOESCAPE	0004

#define FNM_NOSYS	(-1)
#define FNM_NOMATCH	(-2)
#define FNM_BADPAT	(-3)

extern int	fnmatch(const char *, const char *, int);

#ifdef __cplusplus
}
#endif

#endif /*_FNMATCH_H*/
