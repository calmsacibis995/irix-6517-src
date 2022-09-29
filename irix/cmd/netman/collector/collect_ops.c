/*
 * Copyright 1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Data Collector Index operations
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

#include <sys/time.h>
#include <bstring.h>
#include "index.h"
#include "collect.h"

static unsigned int collecthash(struct collectkey *, int);
static int collectcmp(struct collectkey *, struct collectkey *, int);
static int collectxdr(struct __xdr_s *,
				struct collectvalue **, struct collectkey *);

struct indexops collectops = { collecthash, collectcmp, collectxdr };

static unsigned int
collecthash(struct collectkey *ck, int size)
{
	return (ck->ck_src.a_eaddr.ea_vec[3] + ck->ck_src.a_eaddr.ea_vec[4] +
		ck->ck_src.a_eaddr.ea_vec[5] + ck->ck_src.a_port +
		ck->ck_src.a_naddr +
		ck->ck_dst.a_eaddr.ea_vec[3] + ck->ck_dst.a_eaddr.ea_vec[4] +
		ck->ck_dst.a_eaddr.ea_vec[5] + ck->ck_dst.a_port +
		ck->ck_dst.a_naddr);
}

static int
collectcmp(struct collectkey *ck1, struct collectkey *ck2, int size)
{
	return bcmp(ck1, ck2, size);
}

static int
collectxdr(struct __xdr_s *xdr, struct collectvalue **cp,
						struct collectkey *ck)
{
	return xdr_collectvaluechain(xdr, cp);
}
