#ifndef _NETWORK_H_
#define _NETWORK_H_
/*
 * Copyright 1992 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Netlook Network Segments
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

#include "position.h"

class SegmentNode;

class Network				/* Network entity		*/
{
public:
	SegmentNode *segment;		/* Pointer to SegmentNode	*/
	char *display;			/* Displayed string		*/
	Position position;		/* Position in universe		*/
	float radius;			/* Radius of circle		*/
	unsigned short nnodes;		/* Number of nodes in network	*/
	unsigned short info;		/* Flags for the network	*/
	unsigned int connections;	/* Number of connections	*/

	Network(void);
	Network(SegmentNode *);
	~Network(void);

	void place(void);
	void grow(void);
	char *label(void);
	void render(unsigned int complex);
	void updateDisplayString(void);
	Node *checkPick(void);

protected:
	void		add(SegmentNode *);
};
#endif
