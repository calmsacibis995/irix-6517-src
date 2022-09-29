/***********************************************************************
 * fun.h - expression evaluator functions
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

/* $Id: fun.h,v 1.1 1999/04/28 10:06:17 kenmcd Exp $ */

#ifndef FUN_H
#define FUN_H

#include "dstruct.h"

#define ROTATE(x)  if ((x)->nsmpls > 1) rotate(x);
#define EVALARG(x) if ((x)->op < NOP) ((x)->eval)(x);

/* expression evaluator function prototypes */
void rule(Expr *);
void cndFetch_all(Expr *);
void cndFetch_n(Expr *);
void cndFetch_1(Expr *);
void cndDelay_n(Expr *);
void cndDelay_1(Expr *);
void cndRate_n(Expr *);
void cndRate_1(Expr *);
void cndSum_host(Expr *);
void cndSum_inst(Expr *);
void cndSum_time(Expr *);
void cndAvg_host(Expr *);
void cndAvg_inst(Expr *);
void cndAvg_time(Expr *);
void cndMax_host(Expr *);
void cndMax_inst(Expr *);
void cndMax_time(Expr *);
void cndMin_host(Expr *);
void cndMin_inst(Expr *);
void cndMin_time(Expr *);
void cndNeg_n(Expr *);
void cndNeg_1(Expr *);
void cndAdd_n_n(Expr *);
void cndAdd_1_n(Expr *);
void cndAdd_n_1(Expr *);
void cndAdd_1_1(Expr *);
void cndSub_n_n(Expr *);
void cndSub_1_n(Expr *);
void cndSub_n_1(Expr *);
void cndSub_1_1(Expr *);
void cndMul_n_n(Expr *);
void cndMul_1_n(Expr *);
void cndMul_n_1(Expr *);
void cndMul_1_1(Expr *);
void cndDiv_n_n(Expr *);
void cndDiv_1_n(Expr *);
void cndDiv_n_1(Expr *);
void cndDiv_1_1(Expr *);
void cndEq_n_n(Expr *);
void cndEq_1_n(Expr *);
void cndEq_n_1(Expr *);
void cndEq_1_1(Expr *);
void cndNeq_n_n(Expr *);
void cndNeq_1_n(Expr *);
void cndNeq_n_1(Expr *);
void cndNeq_1_1(Expr *);
void cndLt_n_n(Expr *);
void cndLt_1_n(Expr *);
void cndLt_n_1(Expr *);
void cndLt_1_1(Expr *);
void cndLte_n_n(Expr *);
void cndLte_1_n(Expr *);
void cndLte_n_1(Expr *);
void cndLte_1_1(Expr *);
void cndGt_n_n(Expr *);
void cndGt_1_n(Expr *);
void cndGt_n_1(Expr *);
void cndGt_1_1(Expr *);
void cndGte_n_n(Expr *);
void cndGte_1_n(Expr *);
void cndGte_n_1(Expr *);
void cndGte_1_1(Expr *);
void cndNot_n(Expr *);
void cndNot_1(Expr *);
void cndRise_n(Expr *);
void cndRise_1(Expr *);
void cndFall_n(Expr *);
void cndFall_1(Expr *);
void cndAnd_n_n(Expr *);
void cndAnd_1_n(Expr *);
void cndAnd_n_1(Expr *);
void cndAnd_1_1(Expr *);
void cndOr_n_n(Expr *);
void cndOr_1_n(Expr *);
void cndOr_n_1(Expr *);
void cndOr_1_1(Expr *);
void cndMatch_inst(Expr *);
void cndAll_host(Expr *);
void cndAll_inst(Expr *);
void cndAll_time(Expr *);
void cndSome_host(Expr *);
void cndSome_inst(Expr *);
void cndSome_time(Expr *);
void cndPcnt_host(Expr *);
void cndPcnt_inst(Expr *);
void cndPcnt_time(Expr *);
void cndCount_host(Expr *);
void cndCount_inst(Expr *);
void cndCount_time(Expr *);
void actAnd(Expr *);
void actOr(Expr *);
void actShell(Expr *);
void actAlarm(Expr *);
void actSyslog(Expr *);
void actPrint(Expr *);
void actArg(Expr *);
void actFake(Expr *);

#endif /* FUN_H */

