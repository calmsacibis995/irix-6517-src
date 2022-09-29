/*
 * Copyright 1999, Silicon Graphics, Inc.
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

/* $Id: pmiestats.h,v 1.1 1999/04/28 10:06:17 kenmcd Exp $ */

#ifndef PMIESTATS_H
#define PMIESTATS_H

#include <sys/types.h>
#include <sys/param.h>

#define PMIE_DIR	"/var/tmp/pmie"

/* pmie performance instrumentation */
typedef struct {
    char		config[MAXPATHLEN+1];
    char		logfile[MAXPATHLEN+1];
    char		defaultfqdn[MAXHOSTNAMELEN+1];
    float		eval_expected;		/* pmcd.pmie.eval.expected */
    unsigned int	numrules;		/* pmcd.pmie.numrules      */
    unsigned int	actions;		/* pmcd.pmie.actions       */
    unsigned int	eval_true;		/* pmcd.pmie.eval.true     */
    unsigned int	eval_false;		/* pmcd.pmie.eval.false    */
    unsigned int	eval_unknown;		/* pmcd.pmie.eval.unknown  */
    unsigned int	eval_actual;		/* pmcd.pmie.eval.actual   */
    unsigned int	version;
} pmiestats_t;

#endif /* PMIESTATS_H */
