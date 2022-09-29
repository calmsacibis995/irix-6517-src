/* $Header: /proj/irix6.5.7m/isms/stand/arcs/tools/convert/RCS/s_rec.h,v 1.2 1994/11/09 22:03:52 sfc Exp $ */
/* $Log: s_rec.h,v $
 * Revision 1.2  1994/11/09 22:03:52  sfc
 * Merge sherwood/redwood up and including redwood version 1.2
 * > ----------------------------
 * > revision 1.2
 * > date: 93/07/15 19:48:57;  author: igehy;  state: ;  lines: +11 -9
 * > Made compile -fullwarn.
 * > ----------------------------
 *
 * Revision 1.2  1993/07/15  19:48:57  igehy
 * Made compile -fullwarn.
 *
 * Revision 1.1  1987/09/21  11:14:18  huy
 * Initial revision
 * */

/* Header file for Motorola S record format format output */

extern int	S3RecordInitialize(int);
extern int	S2RecordInitialize(int);
extern int	S1RecordInitialize(int);
extern int	SRecordConvert(char *, int);
extern int	SRecordWrite(void);
extern int	SRecordTerminate(void);
