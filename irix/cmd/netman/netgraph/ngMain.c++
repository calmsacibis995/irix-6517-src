/*
 * Copyright 1992 Silicon Graphics, Inc.  All rights reserved.
 *
 *	NetGraph Main
 *
 *	$Revision: 1.1 $
 *	$Date: 1992/09/08 00:34:15 $
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

#include <tuCallBack.h>
#include <tuScreen.h>
#include <tuTopLevel.h>
#include <tuWindow.h>
#include <tuVisual.h>
#include <tuXExec.h>

#include "netGraph.h"

NetGraph *netgraph;
tuXExec *exec;


void
main(int argc, char* argv[])
{

    // Create a netgraph
    netgraph = new NetGraph(argc, argv);

//    netgraph->run();

    // Execute
    exec = new tuXExec(netgraph->getDpy());
    netgraph->setExec(exec);
    
    exec->loop();

}




