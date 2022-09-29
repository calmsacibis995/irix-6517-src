#ifndef REPORT_H
#define REPORT_H
/*
 * Copyright 1990 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Data Reporter include file
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

#include <stdio.h>
#include <rpc/types.h>
#include <rpc/xdr.h>
#include "collect.h"

struct reportvalue { 
	struct collectkey	rv_key;
	struct collectvalue	*rv_value;
}; 

int read_report(char *, unsigned long *, unsigned long *, time_t *, time_t *,
		unsigned int *, struct reportvalue ***, struct reportvalue ***);
int save_report(char *, unsigned long *, unsigned long *, time_t *, time_t *,
		unsigned int *, struct reportvalue ***);

bool_t xdrstdio_report(FILE *, enum xdr_op,
		       unsigned long *, unsigned long *,
		       time_t *, time_t *,
		       unsigned int *, struct reportvalue ***);

#endif /* !REPORT_H */
