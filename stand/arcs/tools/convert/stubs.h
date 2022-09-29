/* $Header: /proj/irix6.5.7m/isms/stand/arcs/tools/convert/RCS/stubs.h,v 1.3 1996/08/13 18:37:44 poist Exp $ */
/* $Log: stubs.h,v $
 * Revision 1.3  1996/08/13 18:37:44  poist
 * new stub for setting a version number (version comes from command line)
 *
 * Revision 1.2  1994/11/09  22:04:06  sfc
 * Merge sherwood/redwood up and including redwood version 1.3
 * > ----------------------------
 * > revision 1.3
 * > date: 93/10/12 00:39:44;  author: jeffs;  state: ;  lines: +9 -6
 * > Fix stub types to match actual args.
 * > ----------------------------
 * > revision 1.2
 * > date: 93/07/15 19:49:28;  author: igehy;  state: ;  lines: +12 -10
 * > Made compile -fullwarn.
 * > ----------------------------
 *
 * Revision 1.3  1993/10/12  00:39:44  jeffs
 * Fix stub types to match actual args.
 *
 * Revision 1.2  1993/07/15  19:49:28  igehy
 * Made compile -fullwarn.
 *
 * Revision 1.1  1987/09/21  11:14:22  huy
 * Initial revision
 * */

extern int StubObjInitialize(int);
extern int StubObjRead(char *, int, int);
extern int StubObjClose(int);

extern int StubFmtInitialize(int);
extern int StubFmtConvert(char *, int);
extern int StubFmtWrite(void);
extern int StubFmtTerminate(void);
extern int StubFmtSetVersion(int);
