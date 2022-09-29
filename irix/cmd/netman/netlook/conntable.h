#ifndef _CONNTABLE_H_
#define _CONNTABLE_H_
/*
 * Copyright 1989-1992 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Netlook Connection Table
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

extern "C" {
#include <index.h>
}

class Node;
class ConnKey;
class Connection;

class ConnTable {
public:
	ConnTable(unsigned int count);
	~ConnTable(void);

	Connection	*lookup(Node *src, Node *dst);
	Connection	*insert(Node *src, Node *dst);
	Connection	*checkPick(void);

	void		render(void);
	void		rescale(void);
	void		clear(void);

protected:
	static unsigned int hash(ConnKey *, int);
	static int	cmp(ConnKey *, ConnKey *, int);

private:
	static struct indexops indexOps;
	Index		*in;
};
#endif
