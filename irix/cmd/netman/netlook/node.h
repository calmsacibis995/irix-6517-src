#ifndef _NODE_H_
#define _NODE_H_
/*
 * Copyright 1992 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Netlook Nodes
 *
 *	$Revision: 1.1 $
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

#include <sys/types.h>
#include "position.h"

class Network;

class Node				/* Node entity			*/
{
public:
	InterfaceNode *interface;	/* Pointer to InterfaceNode	*/
	unsigned int info;		/* Flags for the node		*/
	char *display;			/* Displayed string		*/
	struct snoopstream *snoopstream;/* Stream for snooping on node	*/
	Node *snoop;			/* Next snooper			*/
	time_t timestamp;		/* Time of last activity	*/
	Position position;		/* Position in network		*/
	int connections;		/* Number of connections	*/
	short angle;			/* Angle in the network		*/
	short stringwidth;		/* Width of largest string	*/

	Node(void);
	Node(InterfaceNode *);
	~Node(void);

	char *label(unsigned int = 0);
	void newInfo(void);
	void render(unsigned int complex);
	void updateDisplayString(void);

	int getSnoopSocket(void);

	inline SegmentNode *getSegment(void);
	inline Network	*getNetwork(void);

protected:
	void add(InterfaceNode *);
};

// Inline functions
SegmentNode	*
Node::getSegment(void)
{
    return (SegmentNode *) interface->parent;
}

Network	*
Node::getNetwork(void)
{
    return (Network *) interface->parent->data;
}
#endif
