/*
 * Copyright 1995 Silicon Graphics, Inc. 
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

#ident "$Revision: 1.1 $"

/*
 * This is a module for backward-compatibility of drivers.
 * Just take each packet off the old-style "ipintrq" and call
 * the new network_input mechanism.
 */

#include <sys/mbuf.h>
#include <net/netisr.h>
#include <net/if.h>

struct ifqueue ipintrq = {0, 0, 0, 250};

void schednetisr(int anisr)
{
	struct mbuf *m;

	while (1) {
		IF_DEQUEUE(&ipintrq, m);
		if (m == NULL) {
			return;
		}
		if (anisr == NETISR_IP) {
			network_input(m, AF_INET, 0);
			continue;
		}
		m_freem(m);
	}
}
