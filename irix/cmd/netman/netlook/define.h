#ifndef __DEFINE_H_
#define __DEFINE_H_
/*
 * Copyright 1992 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Netlook #define
 *
 *	$Revision: 1.2 $
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

#define PROGRAM "NetLook"
#define VERSION 1.1

#ifndef NULL
#define NULL		0
#endif

#define SCROLLSTEP	10.0	/* Step for 1 click of scroll bars	*/

#define NPROTOBUTTON	16

#define CONNTABLESIZE	73

#define X		0	/* Coordinates				*/
#define Y		1
#define Z		2

#define LOOKUP		1
#define INSERT		2

#define ALL		1
#define ACTIVE		2

#define ADD		1
#define IGNORE		2

#define PACKETS		1
#define BYTES		2

#define SOURCE		1
#define DEST		2
#define BOTH		3

#define ENDPOINT	1
#define HOP		2

#define SNOOPHOST	0x1	/* Node is the snooping host		*/
#define YPMASTER	0x2	/* Node is a NIS master			*/
#define YPSERVER	0x4	/* Node is a NIS server			*/
#define GATEWAY		0x8	/* Node is a gateway			*/
#define NODEPICKED	0x10	/* Node is picked			*/
#define NETWORKPICKED	0x20	/* Network is picked			*/
#define CONNPICKED	0x40	/* Connection is picked			*/
#define PICKED		0x70	/* Any pick				*/
#define HIDDEN		0x80	/* Object is hidden			*/
#define ADJUST		0x100	/* Object is being adjusted		*/

#define DMFULLNAME	1	/* Display as host name and domain	*/
#define DMNODENAME	2	/* Display as name			*/
#define DMINETADDR	3	/* Display as Internet address		*/
#define DMDECNETADDR	4	/* Display as DECnet address		*/
#define DMVENDOR	5	/* Display as vendor code		*/
#define DMPHYSADDR	6	/* Display as physical address		*/
#define DMMAX		DMPHYSADDR

#define LINESIZE	256	/* Size of text lines			*/
#define HOSTNAMESIZE	64	/* Size of host names			*/
#define FILENAMESIZE	256	/* Size of file names			*/
#define USERNAMESIZE	16	/* Size of user name			*/

#define SOLID		0
#define DASHED		1
#define DOTTED		2

#define ITOP(i)		((i) * 96.0)

class Node;

void	fontInit(void);
int	StringWidth(char *);
int	StringHeight(char *);
void	DrawStringCenterLeft(char *);
void	DrawStringCenterRight(char *);
void	DrawStringCenterCenter(char *);
void	DrawStringLowerLeft(char *);
void	DrawChar(unsigned char);

extern "C" {
int	gethostname(char *,int);
}
#endif
