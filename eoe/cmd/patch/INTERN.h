/* $Header: /proj/irix6.5.7m/isms/eoe/cmd/patch/RCS/INTERN.h,v 1.1 1996/01/17 02:43:33 cleo Exp $
 *
 * $Log: INTERN.h,v $
 * Revision 1.1  1996/01/17 02:43:33  cleo
 * initial checkin.
 *
 * Revision 1.3  1995/10/11  18:32:12  lguo
 * No Message Supplied
 *
 * Revision 2.0  86/09/17  15:35:58  lwall
 * Baseline for netwide release.
 * 
 */

#ifdef EXT
#undef EXT
#endif
#define EXT

#ifdef INIT
#undef INIT
#endif
#define INIT(x) = x

#define DOINIT
