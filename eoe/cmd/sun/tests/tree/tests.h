/*	@(#)tests.h	1.3 90/01/03 NFS Rev 2 Testsuite	*/
/*      1.4 Lachman ONC Test Suite  source        */

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

/*
 * dir info format:
 * <dirinfo name> <level> <file name> <dir name> <fid> <did> <files> <dirs>
 */
#define DIRINFO_FMT		"%s %d %s %s %d %d %d %d\n"
#define DIRINFO_ITEMS	4
#define FILEREC_ITEMS	3
#define INITSTRING		"TESTFILE"
#define BUFLEN			1024
#define FILLSTR_LEN		8
#define FILL_LEN		8
#define SIZE_LEN		8
#define SPACES			3
#define FILE_FMT		"%s %8d %8s %s"

#define FILEMODE		0660
#define DIRMODE			0750

#define DIRINFO_FORM	"[0-9][0-9]*\\.DIRINFO"

/*
 * generate a randon number between min and max
 */
#define GEN_NUM(min,max) \
(((min) == (max)) ? (min) : (int)(drand48()*((double)((max)-(min)+1)))+(min))

/*
 * calculate file fill pattern length
 */
#define FILL_PAT_LEN(name) \
(strlen(name) + SIZE_LEN + FILLSTR_LEN + FILL_LEN + SPACES)

#define NONRAND_LEN(name)	(FILL_PAT_LEN(name) - FILL_LEN)

#define MAKE_FILE_NAME(name, lev, base, n) \
(void)sprintf(name, "%d.%s%d", lev, base, n)

extern int errno;

extern char *Progname;		/* name I was invoked with (for error msgs */
extern int Verbose;

extern double drand48();
extern void srand48();
