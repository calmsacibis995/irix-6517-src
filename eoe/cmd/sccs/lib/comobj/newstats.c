/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/* #ident	"@(#)sccs:lib/comobj/newstats.c	6.4" */
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/sccs/lib/comobj/RCS/newstats.c,v 1.5 1995/12/30 02:19:02 ack Exp $"
# include	"../../hdr/defines.h"

void
newstats(pkt,strp,ch)
register struct packet *pkt;
register char *strp;
register char *ch;
{
	char fivech[6];
	register char *r;
	int i;
	void	putline();

	r = fivech;
	for (i=0; i < 5; i++)
		*r++ = *ch;
	*r = '\0';
	sprintf(strp,"%c%c %s/%s/%s\n",CTLCHAR,STATS,fivech,fivech,fivech);
	putline(pkt,strp);
}
