/*
 * raw_proto.c --
 *
 * Raw protocol family definitions.
 *
 * $Revision: 1.6 $
 *
 * Copyright 1988, 1990, Silicon Graphics, Inc. 
 * All Rights Reserved.
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

#include "sys/types.h"
#include "sys/socket.h"
#include "sys/protosw.h"
#include "sys/domain.h"
#include "raw.h"

extern	struct domain rawdomain;
void	raw_init(), raw_ctlinput();
int	raw_usrreq(), raw_output();

#define	RAWPROTOFLAGS	(PR_ATOMIC|PR_ADDR|PR_DESTADDROPT)

struct protosw rawprotosw[] = {
{ SOCK_RAW,	&rawdomain,	RAWPROTO_SNOOP,	RAWPROTOFLAGS,
  0,		raw_output,	raw_ctlinput,	0,
  raw_usrreq,
  raw_init,	0,		0,		0
},
{ SOCK_RAW,	&rawdomain,	RAWPROTO_DRAIN,	RAWPROTOFLAGS,
  0,		raw_output,	raw_ctlinput,	0,
  raw_usrreq,
  0,		0,		0,		0
}
};

struct domain rawdomain =
    { PF_RAW, "raw network", 0, 0, 0,
      rawprotosw, &rawprotosw[sizeof(rawprotosw)/sizeof(rawprotosw[0])] };
