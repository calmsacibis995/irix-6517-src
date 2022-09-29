/*      @(#)ypsym.h	1.2 88/06/18 4.0NFSSRC SMI   */

/*
 * This contains symbol and structure definitions for modules in the NIS
 * server.
 */

#include <dbm.h>			/* Pull this in first */
#define DATUM
#ifdef NULL
#undef NULL				/* Remove dbm.h's definition of NULL */
#endif

/*
 * For SGI, ypdbminit() is an outer layer on top of dbminit that translates
 * long map names to file names that at most 14 characters long (due to SVR0).
 */
extern int ypdbminit();

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <rpc/rpc.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <dirent.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <rpcsvc/yp_prot.h>
#include <rpcsvc/ypv1_prot.h>
#include <rpcsvc/ypclnt.h>

/* External refs to NIS server data structures */

extern char *progname;

/* External refs to NIS server-only functions */

extern bool ypcheck_map_existence();
extern bool ypset_current_map();
extern void ypclr_current_map();
extern void ypmkfilename();
extern int yplist_maps();

extern void logprintf(char *, ...);
extern int log_request;

#define	LOG_DISPATCH	0x0001
#define	LOG_INTERDOMAIN	0x0002
#define	LOG_QUERYCACHE	0x0004


extern void setForkCnt(int);

extern void yp_make_dbname(char *, char *, int);
