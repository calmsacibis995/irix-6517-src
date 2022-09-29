/*
 * Copyright 1995, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 * 
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 * 
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 * 
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 */

#ident "$Id"

#include "pmapi.h"
#include "impl.h"
#include "./pmnsutil.h"

static FILE	*outf;

/*
 * breadth-first traversal
 */
void
pmns_traverse(__pmnsNode *p, int depth, char *path, void(*func)(__pmnsNode *, int, char *))
{
    char	*newpath;
    __pmnsNode	*q;

    if (p != NULL) {
	/* breadth */
	for (q = p; q != NULL; q = q->next) 
	    (*func)(q, depth, path);
	if (depth > 0)
	    (*func)(NULL, -1, NULL);	/* end of level */
	/* descend */
	for (q = p; q != NULL; q = q->next) {
	    if (q->first != NULL) {
		newpath = (char *)malloc(strlen(path)+strlen(q->name)+2);
		if (depth == 0)
		    *newpath = '\0';
		else if (depth == 1)
		    strcpy(newpath, q->name);
		else {
		    strcpy(newpath, path);
		    strcat(newpath, ".");
		    strcat(newpath, q->name);
		}
		pmns_traverse(q->first, depth+1, newpath, func);
		free(newpath);
	    }
	}
    }
}

/*
 * generate an ASCII PMNS from the internal format produced by
 * pmLoadNameSpace and friends
 */
static void
output(__pmnsNode *p, int depth, char *path)
{
    static int lastdepth = -1;

    if (depth == 0) {
	fprintf(outf, "root {\n");
	lastdepth = 1;
	return;
    }
    else if (depth < 0) {
	if (lastdepth > 0 || (lastdepth == 1 && depth == -2))
	    fprintf(outf, "}\n");
	lastdepth = -1;
	return;
    }
    else if (depth != lastdepth)
	fprintf(outf, "\n%s {\n", path);
    lastdepth = depth;
    if (p->first != NULL)
	fprintf(outf, "\t%s\n", p->name);
    else {
	__pmID_int	*pmidp = (__pmID_int *)&p->pmid;
	fprintf(outf, "\t%s\t%d:%d:%d\n", p->name, pmidp->domain, pmidp->cluster, pmidp->item);
    }
}

void
pmns_output(__pmnsNode *root, FILE *f)
{
    outf = f;
    pmns_traverse(root, 0, "", output);
    output(NULL, -2, NULL);		/* special hack for null PMNS */
}
