/*
 * Copyright 1992 Silicon Graphics, Inc.  All rights reserved.
 *
 *	NIS support
 *
 *	$Revision: 1.6 $
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
#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <bstring.h>
}
#include "cf.h"
#include "node.h"
#include "define.h"

#define YPBUFSIZE	32

static struct ypbuf {
	struct in_addr addr;
	u_int flag;
} ypbuf[YPBUFSIZE];

static unsigned int ypbufp;

extern int _yp_disabled;

void
YPInit(void)
{
    char host[64];
    struct hostent *he;
    int i = 0;
    int disable = _yp_disabled;

    // Check in config file if NIS is on
    FILE *fp = (FILE *) fopen("/etc/config/yp", "r");
    if (fp == NULL)
	return;
    fscanf(fp,"%s", host); 
    fclose(fp);
    if (strcmp(host, "on") != 0)
	return;

    // Get NIS servers
    _yp_disabled = 0;
    fp = (FILE *) popen("/usr/bin/ypcat -k ypservers", "r");
    if (fp != NULL) {
	while ((i < YPBUFSIZE) && (fscanf(fp,"%s",host) != EOF)) {
	    if ((he = gethostbyname(host)) != NULL) {
		bcopy(he->h_addr,&(ypbuf[i].addr),he->h_length);
		ypbuf[i++].flag = YPSERVER;
	    }
	}
	pclose(fp);
    }
    ypbufp = i;

    // Get NIS master
    fp = (FILE *) popen("/usr/bin/ypwhich -m", "r");
    if (fp != NULL) {
	while (fscanf(fp, "%*s %s", host) != EOF) {
	    if ((he = gethostbyname(host)) == NULL)
		continue;

	    for (i = 0; i < ypbufp; i++) {
		if (ypbuf[i].addr.s_addr ==
			((struct in_addr *) he->h_addr)->s_addr) {
		    ypbuf[i].flag |= YPMASTER;
		    break;
		}
	    }
	}
	pclose(fp);
    }
    _yp_disabled = disable;
}

void
YPCheck(Node *node)
{
    if (!node->interface->ipaddr.isValid())
	return;

    unsigned long addr = node->interface->ipaddr.getAddr()->s_addr;
    for (unsigned int i = 0; i < ypbufp; i++) {
	if (addr == ypbuf[i].addr.s_addr) {
	    HostNode *h = node->interface->getHost();
	    node->info |= ypbuf[i].flag; 
	    if (ypbuf[i].flag & YPSERVER)
		h->setNISserver(1);
	    if (ypbuf[i].flag & YPMASTER)
		h->setNISmaster(1);
	    return;
	}
    }
}
