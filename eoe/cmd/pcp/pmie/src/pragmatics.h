/***********************************************************************
 * pragmatics.h - inference engine pragmatics analysis
 *
 * The analysis of how to organize the fetching of metrics (pragmatics)
 * and other parts of the inference engine that are particularly
 * sensitive to details of the performance metrics API are kept here.
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

/* $Id: pragmatics.h,v 1.1 1999/04/28 10:06:17 kenmcd Exp $ */

#ifndef PRAG_H
#define PRAG_H

#include "pmapi.h"

/* report where PCP data is coming from */
char *findsource(char *);

/* initialize performance metrics API */
void pmcsInit(void);

/* juggle contexts */
int newContext(char *);

/* initialize access to archive */
int initArchive(Archive *);

/* initialize timezone */
void zoneInit(void);

/* convert to canonical units */
pmUnits canon(pmUnits);

/* scale factor to canonical pmUnits */
double scale(pmUnits);

/* initialize Metric */
int initMetric(Metric *);

/* reinitialize Metric */
int reinitMetric(Metric *);

/* put initialiaed Metric onto fetch list */
void bundleMetric(Host *, Metric *);

/* reconnect attempt to host */
int reconnect(Host *);

/* pragmatics analysis */
void pragmatics(Symbol, RealTime);

/* execute fetches for given Task */
void taskFetch(Task *);

/* convert Expr value to pmValueSet value */
void fillVSet(Expr *, pmValueSet *);

/* send pmDescriptors for all expressions in given task */
void sendDescs(Task *);

/* put Metric onto wait list */
void waitMetric(Metric *);

/* remove Metric from wait list */
void unwaitMetric(Metric *);

/* check that pmUnits dimensions are equal */
#define dimeq(x, y)	(((x).dimSpace == (y).dimSpace) && \
			 ((x).dimTime == (y).dimTime) && \
			 ((x).dimCount == (y).dimCount))

/* check equality of two pmUnits */
#define unieq(x, y)	(((x).dimSpace == (y).dimSpace) && \
			 ((x).dimTime == (y).dimTime) && \
			 ((x).dimCount == (y).dimCount) && \
			 ((x).scaleSpace == (y).scaleSpace) && \
			 ((x).scaleTime == (y).scaleTime) && \
			 ((x).scaleCount == (y).scaleCount))

/* for initialization of pmUnits struct */
extern pmUnits	noUnits;
extern pmUnits	countUnits;

/* default context type */
extern int	dfltConn;

#endif /* PRAG_H */


