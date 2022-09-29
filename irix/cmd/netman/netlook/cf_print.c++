/*
 * Copyright 1991-1992 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Parser and Lexical Analyzer for Network Configuration Files
 *
 *	$Revision: 1.5 $
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

#include "cf.h"

extern "C" {
#include <netinet/in.h>
#include <stdio.h>
}


void
cf_print(FILE *fp, CfNode *root)
{
	if (root == 0)
		return;

	for (NetworkNode *n = (NetworkNode *) root->child;
					n != 0; n = (NetworkNode *) n->next) {
		fprintf(fp, "Network %s {\n", n->name == 0 ? "" : n->name);
		if (n->ipnum.isValid())
			fprintf(fp, "\tIPNet\t%s\n", n->ipnum.getString());
		if (n->ipmask.isValid())
			fprintf(fp, "\tIPMask\t%s\n", n->ipmask.getString());

		for (SegmentNode *s = (SegmentNode *) n->child;
					s != 0; s = (SegmentNode *) s->next) {
			fprintf(fp, "\tSegment %s {\n",
				    s->name == 0 ? "" : s->name);
			if (s->getType() != SEG_NULL)
				fprintf(fp, "\t\tType\t%s\n", s->printType());
			if (s->ipnum.isValid())
				fprintf(fp, "\t\tIPNet\t%s\n",
					    s->ipnum.getString());
			if (s->dnarea.isValid())
				fprintf(fp, "\t\tDNArea\t%s\n",
					    s->dnarea.getString());

			for (InterfaceNode *i = (InterfaceNode *) s->child;
				    i != 0; i = (InterfaceNode *) i->next) {
				HostNode *h = i->getHost();
				fprintf(fp, "\t\tNode %s {\n",
					    h->name == 0 ? "" : h->name);
				if (h->isNISserver())
					fputs("\t\t\tNISserver\n", fp);
				if (h->isNISmaster())
					fputs("\t\t\tNISmaster\n", fp);
				fprintf(fp, "\t\t\tInterface %s {\n",
					    i->name == 0 ? "" : i->name);
				if (i->physaddr.isValid())
					fprintf(fp, "\t\t\t\tPhysAddr\t%s\n",
						    i->physaddr.getString());
				if (i->ipaddr.isValid())
					fprintf(fp, "\t\t\t\tIPAddr\t\t%s\n",
						    i->ipaddr.getString());
				if (i->dnaddr.isValid())
					fprintf(fp, "\t\t\t\tDNAddr\t\t%s\n",
						    i->dnaddr.getString());
				fputs("\t\t\t}\n\t\t}\n", fp);
			}
			fputs("\t}\n", fp);
		}
		fputs("}\n", fp);
	}
}
