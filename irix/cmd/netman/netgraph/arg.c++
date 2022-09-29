/*
 * Copyright 1989,1990,1991,1992 Silicon Graphics, Inc.  All rights reserved.
 *
 *	$Revision: 1.2 $
 *	$Date: 1992/09/12 01:18:38 $
 */

#include "string.h"

#include "arg.h"
#include "netGraph.h"


Arg::Arg(NetGraph* ng)
{
    netgraph = ng;
} 


// initialize arg to the values of the corresponding vars in netgraph
void
Arg::initValues()
{

    maxValues = netgraph->getMaxValues();
    lockPercentages = netgraph->getLockPercentages();
    sameScale = netgraph->getSameScale();
    timeType = netgraph->getTimeType();
    interval = netgraph->getInterval();
    avgPeriod = netgraph->getAvgPeriod();
    period = netgraph->getPeriod();
    avgPeriod = netgraph->getAvgPeriod();
    updateTime = netgraph->getUpdateTime();

/*****
    char* intfc = netgraph->getInterface();
    if (intfc)
	interface = strdup(intfc);
    else
	interface = strdup("");
******/
    interface = netgraph->getFullSnooperString();
    
} // Arg::initValues

