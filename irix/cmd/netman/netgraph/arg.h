#ifndef __arg_h_
#define __arg_h_

/*
 * Copyright 1992 Silicon Graphics, Inc.  All rights reserved.
 *
 *	NetGraph
 *
 *	$Revision: 1.1 $
 *	$Date: 1992/09/08 00:33:44 $
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

#include "tuBase.h"

#include "constants.h"

class NetGraph;
class ParamControl;

class Arg  {
    friend class NetGraph;
    friend class ParamControl;
public:
    Arg(NetGraph* ng);

    void	initValues();

private:
    NetGraph	*netgraph;
    char	dataFile[FILENAMESIZE];
    char	*interface;
    tuBool	maxValues;
    tuBool	lockPercentages;
    tuBool	sameScale;
    int		defaultStyle;
    int		timeType;
    int		interval;
    int		avgPeriod;
    int		period;
    int		updateTime;
};

#endif /* __arg_h_ */
