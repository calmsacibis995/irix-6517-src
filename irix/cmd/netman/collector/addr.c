/*
 * Copyright 1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Address comparision
 *
 *	$Revision: 1.3 $
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

#include <string.h>
#include "account.h"

int
addrcmp(const struct addr *a1, const struct addr *a2)
{
	int rv;

	/* First try network addresses */
	rv = a1->a_naddr - a2->a_naddr;
	if (rv != 0)
		return rv;

	/* Now try Ethernet addresses */
	rv = memcmp(&a1->a_eaddr, &a2->a_eaddr, sizeof a1->a_eaddr);
	if (rv != 0)
		return rv;

	/* Try ports */
	return a1->a_port - a2->a_port;
}

int
keycmp(const struct collectkey *c1, const struct collectkey *c2)
{
	int rv;
	rv = addrcmp(&c1->ck_src, &c2->ck_src);
	if (rv != 0)
		return rv;

	return addrcmp(&c1->ck_dst, &c2->ck_dst);
}
