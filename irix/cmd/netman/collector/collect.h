#ifndef COLLECT_H
#define COLLECT_H
/*
 * Copyright 1990 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Data Collector include file
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

#include <protocols/ether.h>
#include <histogram.h>

struct entry;

#define COLLECT_MAGIC		0xAC011EC7
#define COLLECT_VERSION		1

struct addr {
	struct etheraddr	a_eaddr;
	unsigned short		a_port;
	unsigned long		a_naddr;
};

struct collectkey {
	struct addr		ck_src;
	struct addr		ck_dst;
};

struct collectvalue {
	struct collectvalue	*cv_next;
	union {
	    unsigned int	cvu_protoid;		
	    unsigned int	cvu_llctype;		
	} cv_u;
	struct counts		cv_count;
};

#define cv_protoid	cv_u.cvu_protoid
#define cv_llctype	cv_u.cvu_llctype

int save_entrylist(char *, unsigned long *, unsigned long *,
		   time_t *, time_t *, struct entry **, unsigned int *);
char *getpath(time_t *, time_t *);
char *getdirectory(time_t *, time_t *);
char *getfile(time_t *, time_t *);
int addrcmp(const struct addr *, const struct addr *);
int keycmp(const struct collectkey *, const struct collectkey *);

#endif /* !COLLECT_H */
