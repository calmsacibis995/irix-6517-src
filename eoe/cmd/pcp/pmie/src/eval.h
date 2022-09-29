/***********************************************************************
 * eval.h
 ***********************************************************************
 *
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

/* $Id: eval.h,v 1.3 1999/08/16 23:13:49 kenmcd Exp $ */

#ifndef EVAL_H
#define EVAL_H

#include "dstruct.h"

/* fill in apprpriate evaluator function for given Expr */
void findEval(Expr *);

/* run evaluator until specified time reached */
void run(void);

/* invalidate one expression (and descendents) */
void clobber(Expr *);

/* invalidate all expressions being evaluated */
void invalidate(void);

/* report changes in pmcd connection state */
#define STATE_INIT	0
#define STATE_FAILINIT	1
#define STATE_RECONN	2
#define STATE_LOSTCONN	3
int host_state_changed(char *, int);

#endif /* EVAL_H */

