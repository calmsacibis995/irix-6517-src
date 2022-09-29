/*
 * diagram.c
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
 * diagram.c - 
 * this program displays the memory organization, for IP30
 *
 * IP30 looks like:
 *       ------------------------------------
 *      |     -----------------------------  |
 *      |    |                             | |
 *      |    |          Processor          | |
 *      |    |          Module             | |
 * I/O  |    |                             | |
 *conns |     -----------------------------  |
 * on   |             --------    --------   |
 *this  |            | HEART  |  | Bridge |  |
 *edge  |            |        |  |        |  |
 *      |             --------    --------   |
 *      |                                    |
 *      |           ===================  S1  |
 *      |           ===================      |
 *      |           ===================      |
 *      |           ===================      |
 *      |           ===================      |
 *      |           ===================      |
 *      |           ===================      |
 *      |           ===================  S8  |
 *      |                                    |
 *       ------------------------------------
 */
#ident "ide/godzilla/mem/diagram.c:   $Revision: 1.5 $"

#include <sys/cpu.h>
#include <libsc.h>

/* two adjacent DIMMs make up one memory bank. the memory hardware 	*/
/* is accessed in chunks of 128 bits (plus 8 ECC) and S1&S2 make up one */
/* chunk; S3&S4 make another, etc.					*/
/* when you install memory you should do it in pairs of S1&S2, 		*/
/* then S3&S4, etc.							*/

static char *speedracer[] = {
        "     e|  o|    e|  o|    e|  o|    e|  o|",
        "     v|  d|    v|  d|    v|  d|    v|  d|",
        "     e|  d|    e|  d|    e|  d|    e|  d|",
        "     n|   |    n|   |    n|   |    n|   |",
        "      |   |     |   |     |   |     |   |",
        "     d|  d|    d|  d|    d|  d|    d|  d|",
        "     w|  w|    w|  w|    w|  w|    w|  w|",
        "     o|  o|    o|  o|    o|  o|    o|  o|",
        "     r|  r|    r|  r|    r|  r|    r|  r|",
        "     d|  d|    d|  d|    d|  d|    d|  d|",
        "      |   |     |   |     |   |     |   |",
        "      |   |     |   |     |   |     |   |",
        "      |   |     |   |     |   |     |   |",
        "     S1  S2    S3  S4    S5  S6    S7  S8",
        "     Bank 0    Bank 1    Bank 2    Bank 3",
        "                                         ",
        "-----------------------------------------",
        "          I/O connectors                 ",
        "                                         ",
	" NOTE: install memory in pairs of S1&S2, ",
	"       then S3&S4, etc.                  ",
        0
};

void
print_memory_diagram(void)
{
	char **p, *bd =
	   "+-------------------------------------------------+\n";

	p = speedracer;

	printf(bd);

	while(*p)
		printf("|    %-40s    |\n", *p++);

	printf(bd);
}
