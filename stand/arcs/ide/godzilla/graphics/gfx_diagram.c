/*
 * gfx_diagram.c
 *
 * Copyright 1995, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 *
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 *
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 *
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 *
 * gfx_diagram.c - shows the diff gfx config possible
 *
 */
#ident "ide/godzilla/graphics/gfx_diagram.c:   $Revision: 1.1 $"

#include <sys/cpu.h>
#include <libsc.h>

/* args to print_impact_diagram */
#define GFX_DIAGRAM_GM10	10
#define GFX_DIAGRAM_GM20	20
#define GFX_DIAGRAM_NO_TMEZZ	0
#define GFX_DIAGRAM_TMEZZ	1

/* two adjacent DIMMs make up one memory bank. the memory hardware 	*/
/* is accessed in chunks of 128 bits (plus 8 ECC) and S1&S2 make up one */
/* chunk; S3&S4 make another, etc.					*/
/* when you install memory you should do it in pairs of S1&S2, 		*/
/* then S3&S4, etc.							*/

static char *solid_impact[] = {
        "     +---------------------------+    ",
        "     |                           |    ",
        "     |                           +--+ ",
        "     |                        xtalk | ",
        "     |            GM10        conn. | ",
        "     |                           +--+ ",
        "     |                           |    ",
        "     +---------------------------+    ",
        "     solid impact: GM10               ",
        0
};


static char *high_impact[] = {
        "     +---------------------------+    ",
        "     | +----------+              |    ",
        "     | |          |              +--+ ",
        "     | |          |           xtalk | ",
        "     | |  TMEZZ   |   GM10    conn. | ",
        "     | |          |              +--+ ",
        "     | +----------+              |    ",
        "     +---------------------------+    ",
        "     high impact: GM10 + TMEZZ        ",
        0
};

static char *super_solid_impact[] = {
        "     +---------------------------+    ",
        "     |                           |    ",
        "     |                           +--+ ",
        "     |                        power | ",
        "     |                        conn. | ",
        "     |                           +--+ ",
        "     |                           |    ",
        "     |            GM20           |    ",
        "     |                           +--+ ",
        "     |                        xtalk | ",
        "     |                        conn. | ",
        "     |                           +--+ ",
        "     |                           |    ",
        "     +---------------------------+    ",
        "     super solid impact: GM20         ",
        0
};

static char *maximum_impact[] = {
        "     +---------------------------+    ",
        "     | +----------+              |    ",
        "     | |          |              +--+ ",
        "     | |          |           power | ",
        "     | |  TMEZZ   |           conn. | ",
        "     | |          |              +--+ ",
        "     | +----------+              |    ",
        "     |                GM20       |    ",
        "     | +----------+              |    ",
        "     | |          |              +--+ ",
        "     | |          |           xtalk | ",
        "     | |  TMEZZ   |           conn. | ",
        "     | |          |              +--+ ",
        "     | +----------+              |    ",
        "     +---------------------------+    ",
        "     max impact: GM20 + 2 TMEZZ's     ",
        0
};

/* 
 * called from shell/field as help_impact
 */
void
print_impact_diagram (int argc, char *argv[])
{
	char 	**p;
	int	gamera, daughter, dont_print = 0;

	if (argc == 3) {
	    gamera = atoi(argv[1]);
	    daughter = atoi(argv[2]);

	    if ((gamera==GFX_DIAGRAM_GM10) && (daughter==GFX_DIAGRAM_NO_TMEZZ))
		p = solid_impact;
	    else if ((gamera==GFX_DIAGRAM_GM10) && (daughter==GFX_DIAGRAM_TMEZZ))
		p = high_impact;
	    else if ((gamera==GFX_DIAGRAM_GM20) && (daughter==GFX_DIAGRAM_NO_TMEZZ))
		p = super_solid_impact;
	    else if ((gamera==GFX_DIAGRAM_GM20) && (daughter==GFX_DIAGRAM_TMEZZ))
		p = maximum_impact;
	    else dont_print = 1;

	    if (dont_print == 0) {
		while(*p) printf("        %-40s\n", *p++);
	    }
	    else printf("usage: arg1 must be 20 or 10, arg2 must be 0 or 1\n");
	}
	else printf("usage: print_impact_diagram needs two args (10/20,0/1)\n");

}
