#ident	"$Revision: 5.41 $"
/*LINTLIBRARY*/
/*PROTOLIB1*/

#include <assert.h>
#include <ctype.h>
#include <locale.h>
#include <setjmp.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <float.h>
#include <dirent.h>
#include <bstring.h>

#include <sys/types.h>
#include <fcntl.h>
#include <stropts.h>
#include <termio.h>
#include <ieeefp.h>
#include <sys/stat.h>
#include <ftw.h>
#include <search.h>
#include <grp.h>
#include <langinfo.h>
#include <memory.h>
#include <mon.h>
#include <nlist.h>
#include <pwd.h>
#include <wait.h>
#include <shadow.h>
#include <siginfo.h>
#include <signal.h>
#include <sys/utsname.h>
#include <ustat.h>
#include <ucontext.h>
#include <unistd.h>
#include <sys/mount.h>
#include <sys/lock.h>
#include <sys/poll.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/statfs.h>
#include <sys/fstyp.h>
#include <sys/times.h>
#include <sys/param.h>
#include <sys/mman.h>
#include <crypt.h>
#include <ndbm.h>
#include <invent.h>
#include <abi_mutex.h>
#include <ulocks.h>
#include <task.h>
#include <pfmt.h>
#include <utmpx.h>
#include <utmp.h>

/* 
** The below should eventually be in header files - when/if they 
** are, there is a possibility that lint will not build this
** library correctly - when this happens, remove the object
** from this file.
*/

/* END (3C) */
extern int end;
extern int etext;
extern int edata;

/* PERROR(3C) */
extern int errno;
extern char *sys_errlist[];
extern int sys_nerr;

long priocntl(idtype_t, id_t,int,...);
long priocntlset(procset_t *,int,...);
int grantpt(int);
int isastream(int);
void psignal(int, const char *);
char *ptsname(int);
int swapctl(int, void *);
int unlockpt(int);

/* sys/time.h: definition of structs timezone, timeval */
#include <sys/time.h>

/* sys/resource.h: definition of struct rlimit */
#include <sys/resource.h>

/* sys/uio.h: defintion ofstruct iovec */
#include <sys/uio.h>
