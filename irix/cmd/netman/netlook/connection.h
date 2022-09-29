#ifndef _CONNECTION_H_
#define _CONNECTION_H_
/*
 * Copyright 1989-1992 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Netlook Connections
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

#include <time.h>

class Node;

class Endpoint				/* Endpoint of a connection	*/
{
public:
	Node *node;			/* Node that is the endpoint	*/
	unsigned int intensity;		/* Intensity of connection	*/
	unsigned int packets;		/* Number of packets transmitted*/
	unsigned int bytes;		/* Number of bytes transmitted	*/
	time_t timestamp;		/* Last transmission 		*/
};

class Connection
{
public:
	Connection(void);

	void calculate(void);
	void update(void);
	void render(void);

//private:
	Endpoint src;			/* Source of connection		*/
	Endpoint dst;			/* Destination of connection	*/
	unsigned int info;		/* Information bits		*/

};
#endif
