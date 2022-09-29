/*
 * xbow_html: generate HTML text based on xbow files
 *
 * constructs a Netscape-compatible "table" definition
 * based on data from xbow.h
 */

#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>

#include <sys/types.h>

struct macrofield_s {
    char                   *mname;
    __uint64_t              mvalue;
    char                   *fname;
    __uint64_t              foffset;
};

#define	MACROFIELD_LINE(m,f)		{ #m, m, #f, offsetof (xbow_t, f) },
#define	MACROFIELD_LINE_BITFIELD(m)	{ #m, XBOWCONST m, (char *)0, 0 },

#include "xbow.h"

int
main(int ac, char **av)
{
    int                     reg, nregs;

    nregs = sizeof xbow_macrofield / sizeof xbow_macrofield[0];

    printf("<HTML><HEAD><TITLE>Crossbow ASIC</TITLE></HEAD><BODY>\n");
    printf("<CENTER>\n");
    printf("<TABLE BORDER><CAPTION>Crossbow Memory Map</CAPTION>\n");
    printf("<TR><TH>field name<BR>MACRO</TH><TH>byte offset</TH><TH>Bitfield</TH></TR>\n");
    for (reg = 0; reg < nregs; reg++) {
	if (xbow_macrofield[reg].fname) {
	    printf("<TR><TD><STRONG>%s</STRONG><BR><KBD>%s</KBD></KBD></TD>",
		   xbow_macrofield[reg].fname,
		   xbow_macrofield[reg].mname);
	    if (xbow_macrofield[reg].mvalue != xbow_macrofield[reg].foffset) {
		printf("<TD><KBD><STRONG>0x%06llX<BR>0x%06llX</STRONG></KBD></TD></TR>",
		       xbow_macrofield[reg].mvalue,
		       xbow_macrofield[reg].foffset);
	    } else {
		printf("<TD><KBD>0x%06llX</KBD></TD></TR>",
		       xbow_macrofield[reg].foffset);
	    }
	} else {
	    printf("<TR><TD><KBD>%s</KBD></TD><TD></TD><TD><KBD>%016llX</KBD></TD></TR>\n",
		   xbow_macrofield[reg].mname,
		   xbow_macrofield[reg].mvalue);
	}
    }
    printf("</TABLE>\n");

    printf("</CENTER>\n");
    printf("</BODY></HTML>\n");
    return 0;
}
