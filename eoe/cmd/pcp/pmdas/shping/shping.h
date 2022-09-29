/*
 * Copyright 1995, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 * 
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 * 
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 * 
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 */

#ident "$Id: shping.h,v 1.2 1999/05/11 00:28:03 kenmcd Exp $"

#include "pmapi.h"
#include "impl.h"
#include "pmda.h"

typedef struct {
    char		*tag;
    char		*cmd;
    int			status;
    int			error;
    float		real;
    float		usr;
    float		sys;
} cmd_t;

#define STATUS_NOTYET	-1
#define STATUS_OK	0
#define STATUS_EXIT	1
#define STATUS_SIG	2
#define STATUS_TIMEOUT	3
#define STATUS_SYS	4

extern cmd_t		*cmdlist;

extern __uint32_t	cycletime;	/* seconds per command cycle */
extern __uint32_t	timeout;	/* response timeout in seconds */
extern pid_t		sprocpid;	/* for refresh() */
extern pmdaIndom	indomtab;	/* cmd tag indom */

extern int		pmDebug;
extern char		*pmProgname;

extern void		shping_init(pmdaInterface *);

extern void		logmessage(int, const char *, ...);
