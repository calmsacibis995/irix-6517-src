/*	@(#)tests.h	1.2 97/01/03 Connectathon Testsuite	*/
/*	1.4 Lachman ONC Test Suite source	*/

#define	TESTDIR	"/mnt/nfstestdir"
#define	DNAME	"dir."
#define	FNAME	"file."
#define	DCOUNT	10
#define	DDIRS	2
#define	DLEVS	5
#define	DFILS	5

#ifndef MAXPATHLEN
#define MAXPATHLEN	1024
#endif

extern int errno;

extern char *Myname;		/* name I was invoked with (for error msgs */

#ifdef STDARG
extern void error(char *, ...);
#endif
