/*
 * heart_html: generate HTML text based on heart files
 *
 * constructs a Netscape-compatible "table" definition
 * based on data from heart.h
 */

#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>

#include <sys/types.h>

struct macrofield_s {
	char	       *mname;
	__uint64_t	mvalue;
	char	       *fname;
	__uint64_t	foffset;
};

#define BRIDGE_BASE	0x1f000000
#define HEART_BASE	0x18000000
#define XBOW_BASE	0x10000000

#define	MACROFIELD_LINE(m,s,f)		{ #m, m, #f, offsetof (heart_##s, f)},
#define	MACROFIELD_LINE_BITFIELD(m)	{ #m, HEARTCONST m, (char *)0, 0 },

#include "heart.h"

int main(int ac, char **av)
{
	int		reg, nregs;

	nregs = sizeof heart_piu_macrofield / sizeof heart_piu_macrofield[0];

	printf("<HTML><HEAD><TITLE>HEART ASIC</TITLE></HEAD><BODY>\n");
	printf("<CENTER>\n");
	printf("<TABLE BORDER><CAPTION>heart PIU Memory Map</CAPTION>\n");
	printf("<TR><TH>field name<BR>MACRO</TH><TH>byte offset</TH><TH>Bitfield</TH></TR>\n");
	for (reg=0; reg < nregs; reg++) {
		if (heart_piu_macrofield[reg].fname) {
			printf("<TR><TD><STRONG>%s</STRONG><BR><KBD>%s</KBD></KBD></TD>",
			       heart_piu_macrofield[reg].fname,
			       heart_piu_macrofield[reg].mname);
			if (heart_piu_macrofield[reg].mvalue != heart_piu_macrofield[reg].foffset) {
				printf("<TD><KBD><BLINK>0x%06llX<BR>0x%06llX</BLINK></KBD></TD></TR>",
				       heart_piu_macrofield[reg].foffset,
				       heart_piu_macrofield[reg].mvalue);
			} else {
				printf("<TD><KBD>0x%06llX</KBD></TD></TR>",
				       heart_piu_macrofield[reg].foffset);
			}
		} else {
			printf("<TR><TD><KBD>%s</KBD></TD><TD></TD><TD><KBD>%016llX</KBD></TD></TR>\n",
			       heart_piu_macrofield[reg].mname,
			       heart_piu_macrofield[reg].mvalue);
		}
	}
	printf("</TABLE>\n");

	nregs = sizeof heart_iou_macrofield / sizeof heart_iou_macrofield[0];
	printf("<TABLE BORDER><CAPTION>heart IOU Memory Map</CAPTION>\n");
	printf("<TR><TH>field name<BR>MACRO</TH><TH>byte offset</TH><TH>Bitfield</TH></TR>\n");
	for (reg=0; reg < nregs; reg++) {
		if (heart_iou_macrofield[reg].fname) {
			printf("<TR><TD><STRONG>%s</STRONG><BR><KBD>%s</KBD></KBD></TD>",
			       heart_iou_macrofield[reg].fname,
			       heart_iou_macrofield[reg].mname);
			if (heart_iou_macrofield[reg].mvalue != heart_iou_macrofield[reg].foffset) {
				printf("<TD><KBD><STRONG>0x%06llX<BR>0x%06llX</STRONG></KBD></TD></TR>",
				       heart_iou_macrofield[reg].foffset,
				       heart_iou_macrofield[reg].mvalue);
			} else {
				printf("<TD><KBD>0x%06llX</KBD></TD></TR>",
				       heart_iou_macrofield[reg].foffset);
			}
		} else {
			printf("<TR><TD><KBD>%s</KBD></TD><TD></TD><TD><KBD>%016llX</KBD></TD></TR>\n",
			       heart_iou_macrofield[reg].mname,
			       heart_iou_macrofield[reg].mvalue);
		}
	}
	printf("</TABLE>\n");

	printf("</CENTER>\n");
	printf("</BODY></HTML>\n");
	return 0;
}
