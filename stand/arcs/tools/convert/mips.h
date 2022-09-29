/* $Header: /proj/irix6.5.7m/isms/stand/arcs/tools/convert/RCS/mips.h,v 1.2 1994/11/09 22:03:38 sfc Exp $ */
/* $Log: mips.h,v $
 * Revision 1.2  1994/11/09 22:03:38  sfc
 * Merge sherwood/redwood up and including redwood version 1.2
 * > ----------------------------
 * > revision 1.2
 * > date: 93/07/15 19:48:32;  author: igehy;  state: ;  lines: +8 -5
 * > Made compile -fullwarn.
 * > ----------------------------
 *
 * Revision 1.2  1993/07/15  19:48:32  igehy
 * Made compile -fullwarn.
 *
 * Revision 1.1  1987/09/21  11:14:13  huy
 * Initial revision
 * */

/* Header file for Mips Object Files */

extern int	MipsInitialize(int);
extern int	MipsRead(char *, int, int);
extern int	MipsClose(int);
